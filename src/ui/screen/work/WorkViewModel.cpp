#include "WorkViewModel.h"

#include "domain/service/IProjectContextService.h"
#include "domain/service/IConversationService.h"
#include "domain/repository/IConversationRepository.h"
#include "domain/repository/IProjectRepository.h"
#include "application/usecase/work/SwitchProjectUseCase.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QUuid>
#include <QTimer>
#include <algorithm>

namespace ui::screen::work {
namespace {
domain::conversation::Message& ensureStreamingMessage(WorkState& state) {
    if (!state.messages.isEmpty() && state.messages.last().role == domain::MessageRole::Assistant
        && state.messages.last().status == domain::MessageStatus::Sending) {
        return state.messages.last();
    }
    domain::conversation::Message message;
    message.id = QUuid::createUuid();
    message.role = domain::MessageRole::Assistant;
    message.status = domain::MessageStatus::Sending;
    message.createdAt = QDateTime::currentDateTime();
    state.messages.append(std::move(message));
    return state.messages.last();
}
}

WorkViewModel::WorkViewModel(const application::usecase::work::WorkUseCases& useCases,
                             domain::service::IProjectContextService* projectContext, QObject* parent)
    : BaseViewModel<WorkViewModel, WorkState>(parent), m_useCases(useCases), m_projectContext(projectContext),
      m_conversationService(useCases.conversationService),
      m_conversationRepository(useCases.conversationRepository), m_projectRepository(useCases.projectRepository) {
    setupUseCaseConnections();
    setProjectRoot(QDir::currentPath());
    if (m_projectRepository) {
        auto projects = m_projectRepository->getAllProjects();
        QList<ui::screen::chat::ChatSessionItemData> sessions;
        if (m_conversationRepository) for (const auto& conversation : m_conversationRepository->getAllConversations()) {
            if (!conversation.projectId.has_value()) continue;
            sessions.append({conversation.id.toString(), conversation.title, conversation.isPinned, conversation.isArchived,
                             conversation.updatedAt.toMSecsSinceEpoch(), conversation.projectId});
        }
        updateState([projects, sessions](WorkState& state) { 
            state.projects = projects; 
            state.sessions = sessions; 
            for (const auto& p : projects) {
                if (p.isPinned) state.pinnedProjects.insert(p.id);
            }
        });
    }
}

WorkViewModel::~WorkViewModel() = default;

void WorkViewModel::setupUseCaseConnections() {
    auto* agent = m_useCases.runAgent;
    if (agent) {
        connect(agent, &application::usecase::agent::RunAgentUseCase::userMessageCreated, this,
                [this](const QString& sessionId, const domain::conversation::Message& message) {
            if (sessionId != m_agentSessionId) return;
            updateState([this, message](WorkState& state) {
                state.messages.append(message);
                for (auto& item : state.sessions) {
                    if (item.id != m_agentSessionId || item.title != QStringLiteral("新对话")) continue;
                    const QString title = taskTitle(std::get<domain::conversation::TextBlock>(message.blocks.first().payload).text);
                    item.title = title;
                    if (m_conversationRepository) {
                        const auto conversation = m_conversationRepository->getConversation(QUuid(m_agentSessionId));
                        if (conversation) {
                            auto updated = *conversation;
                            updated.title = title;
                            updated.updatedAt = QDateTime::currentDateTime();
                            m_conversationRepository->saveConversation(updated);
                        }
                    }
                    break;
                }
                state.isProcessing = true;
                state.statusMessage = QStringLiteral("项目 Agent 正在处理…");
            });
        });
        connect(agent, &application::usecase::agent::RunAgentUseCase::tokenReceived, this,
                [this](const QString& sessionId, const QString& token) {
            if (sessionId != m_agentSessionId || token.isEmpty()) return;
            updateState([this, token](WorkState& state) {
                auto& message = ensureStreamingMessage(state);
                for (auto& block : message.blocks) {
                    if (block.isText()) { std::get<domain::conversation::TextBlock>(block.payload).text += token; return; }
                }
                message.blocks.append({domain::BlockType::Text, domain::conversation::TextBlock{token}});
            });
        });
        connect(agent, &application::usecase::agent::RunAgentUseCase::thoughtReceived, this,
                [this](const QString& sessionId, const QString& thought) {
            if (sessionId != m_agentSessionId || thought.isEmpty()) return;
            updateState([this, thought](WorkState& state) {
                auto& message = ensureStreamingMessage(state);
                for (auto& block : message.blocks) {
                    if (block.isThought()) { std::get<domain::conversation::ThoughtBlock>(block.payload).thought += thought; return; }
                }
                message.blocks.append({domain::BlockType::Thought, domain::conversation::ThoughtBlock{thought, 0}});
            });
        });
        connect(agent, &application::usecase::agent::RunAgentUseCase::stateChanged, this,
                [this](const domain::agent::AgentRunState& runState) {
            if (runState.conversationId != m_agentSessionId) return;
            updateState([runState](WorkState& state) {
                state.agentUiState.status = runState.status;
                state.agentUiState.currentRound = runState.round;
                state.agentUiState.statusMessage = runState.errorMessage;
                state.isProcessing = (runState.status == domain::agent::AgentRunStatus::Preparing
                    || runState.status == domain::agent::AgentRunStatus::CallingModel
                    || runState.status == domain::agent::AgentRunStatus::WaitingPermission
                    || runState.status == domain::agent::AgentRunStatus::WaitingTool
                    || runState.status == domain::agent::AgentRunStatus::ExecutingTool
                    || runState.status == domain::agent::AgentRunStatus::Continuing);
                state.agentUiState.isWaitingPermission = (runState.status == domain::agent::AgentRunStatus::WaitingPermission);
            });
        });
        connect(agent, &application::usecase::agent::RunAgentUseCase::permissionRequested, this,
                [this](const QString& sessionId, const domain::agent::ToolCall& call, const domain::agent::ToolPermission& permission) {
            if (sessionId != m_agentSessionId) return;
            updateState([call, permission](WorkState& state) {
                state.agentUiState.isWaitingPermission = true;
                state.agentUiState.permissionPendingCall = call;
                state.agentUiState.permissionRequired = permission;
                state.statusMessage = QStringLiteral("等待用户授权操作: %1").arg(permission.reason);
            });
        });
        connect(agent, &application::usecase::agent::RunAgentUseCase::toolCallFinished, this,
                [this](const QString& sessionId, const domain::agent::ToolCall& call) {
            if (sessionId != m_agentSessionId) return;
            updateState([this, call](WorkState& state) {
                auto& message = ensureStreamingMessage(state);
                domain::conversation::ToolCallBlock calls; calls.calls.append(call);
                message.blocks.append({domain::BlockType::ToolCall, calls});

                state.agentUiState.activeToolName = call.name;
                state.toolEvents.append(WorkState::ToolEvent{
                    call.name,
                    call.arguments,
                    {},
                    false
                });
            });
        });
        connect(agent, &application::usecase::agent::RunAgentUseCase::toolResultReady, this,
                [this](const QString& sessionId, const domain::agent::ToolResult& result) {
            if (sessionId != m_agentSessionId) return;
            updateState([this, result](WorkState& state) {
                auto& message = ensureStreamingMessage(state);
                domain::conversation::ToolResultBlock results; results.results.append(result);
                message.blocks.append({domain::BlockType::ToolResult, results});

                for (auto it = state.toolEvents.rbegin(); it != state.toolEvents.rend(); ++it) {
                    if (it->result.isEmpty()) {
                        it->result = result.content;
                        it->isError = result.isError;
                        break;
                    }
                }
            });
        });
        connect(agent, &application::usecase::agent::RunAgentUseCase::replyGenerated, this,
                [this](const QString& sessionId, const domain::conversation::Message& message) {
            if (sessionId != m_agentSessionId) return;
            updateState([this, message](WorkState& state) {
                if (!state.messages.isEmpty() && state.messages.last().role == domain::MessageRole::Assistant
                    && state.messages.last().status == domain::MessageStatus::Sending) state.messages.last() = message;
                else state.messages.append(message);
            });
        });
        connect(agent, &application::usecase::agent::RunAgentUseCase::runCompleted, this,
                [this](const QString& sessionId) {
            if (sessionId != m_agentSessionId) return;
            updateState([](WorkState& state) {
                state.isProcessing = false;
                state.statusMessage.clear();
                state.agentUiState.status = domain::agent::AgentRunStatus::Completed;
                state.agentUiState.isWaitingPermission = false;
            });
        });
        connect(agent, &application::usecase::agent::RunAgentUseCase::runFailed, this,
                [this](const QString& sessionId, const domain::llm::ChatError& error) {
            if (sessionId != m_agentSessionId) return;
            updateState([error](WorkState& state) {
                state.isProcessing = false;
                state.statusMessage = error.userMessage.isEmpty() ? error.message : error.userMessage;
                state.agentUiState.status = domain::agent::AgentRunStatus::Failed;
                state.agentUiState.isWaitingPermission = false;
            });
        });
    }
}

void WorkViewModel::grantPermission(const QString& toolCallId, bool granted) {
    if (m_agentSessionId.isEmpty() || !m_useCases.runAgent) return;
    updateState([](WorkState& state) {
        state.agentUiState.isWaitingPermission = false;
    });
    m_useCases.runAgent->grantPermission(m_agentSessionId, toolCallId, granted);
}

QString WorkViewModel::taskTitle(const QString& task) {
    const QString trimmed = task.trimmed();
    return trimmed.left(36) + (trimmed.size() > 36 ? QStringLiteral("…") : QString());
}

void WorkViewModel::startTask(const QString& task) {
    const QString trimmed = task.trimmed();
    if (trimmed.isEmpty() || m_currentProjectId.isNull() || m_agentSessionId.isEmpty()) return;
    updateState([trimmed](WorkState& state) { state.currentTask = taskTitle(trimmed); state.statusMessage.clear(); });
    if (m_useCases.runAgent) {
        // 调用 RunAgentUseCase 启动智能体编排任务
        m_useCases.runAgent->execute(
            m_agentSessionId,
            trimmed,
            m_currentProjectId,
            m_state.projectRoot
        );
    }
}

void WorkViewModel::cancelTask() {
    if (m_useCases.cancelAgentRun) {
        m_useCases.cancelAgentRun->execute();
    }
    updateState([](WorkState& state) { state.isProcessing = false; state.statusMessage.clear(); });
}

void WorkViewModel::setProjectRoot(const QString& rootPath) {
    domain::project::ProjectContext context;
    if (m_useCases.switchProject) {
        context = m_useCases.switchProject->execute(m_state.projectRoot, rootPath);
    } else if (m_projectContext) {
        context = m_projectContext->load(rootPath);
    } else {
        return;
    }

    updateState([context](WorkState& state) {
        state.projectRoot = context.rootPath;
        state.projectName = QFileInfo(context.rootPath).fileName();
        state.skillCount = context.skills.size();
        state.hasAgentsInstructions = !context.agentsInstructions.isEmpty();
        state.hasMcpConfig = !context.mcpConfigPath.isEmpty();
        state.mcpConfigContent = context.mcpConfigContent;
        QStringList instructions;
        if (!context.agentsInstructions.isEmpty()) instructions.append(context.agentsInstructions);
        for (const auto& skill : context.skills) instructions.append(QStringLiteral("# Skill: %1\n%2").arg(skill.name, skill.instructions));
        state.agentInstructions = instructions.join(QStringLiteral("\n\n"));
    });
}

void WorkViewModel::selectProject(const QUuid& projectId) {
    if (projectId.isNull()) return;
    const auto it = std::find_if(m_state.projects.cbegin(), m_state.projects.cend(), [projectId](const auto& project) { return project.id == projectId; });
    if (it == m_state.projects.cend()) return;
    m_currentProjectId = projectId;
    setProjectRoot(it->rootPath);
    updateState([projectId](WorkState& state) { state.currentProjectId = projectId; });
}

void WorkViewModel::addProject(const QString& rootPath, const QString& displayName) {
    if (!m_projectRepository) return;
    const QString canonical = QDir(rootPath).canonicalPath();
    if (canonical.isEmpty()) return;
    auto project = m_projectRepository->getProjectByPath(canonical);
    if (!project) {
        domain::project::Project created;
        created.id = QUuid::createUuid(); created.rootPath = canonical;
        created.name = displayName.trimmed().isEmpty() ? QFileInfo(canonical).fileName() : displayName.trimmed();
        created.createdAt = QDateTime::currentDateTime(); created.lastOpenedAt = created.createdAt;
        m_projectRepository->saveProject(created); project = created;
        updateState([created](WorkState& state) { state.projects.append(created); });
    }
    selectProject(project->id);
}

void WorkViewModel::removeProject(const QUuid& projectId) {
    QString targetRoot;
    for (const auto& p : m_state.projects) {
        if (p.id == projectId) {
            targetRoot = p.rootPath;
            break;
        }
    }
    if (m_useCases.switchProject && !targetRoot.isEmpty()) {
        m_useCases.switchProject->stopProjectRuntime(targetRoot);
    }

    if (m_projectRepository) {
        m_projectRepository->deleteProject(projectId);
    }
    
    updateState([this, projectId](WorkState& state) {
        // Cascade delete all conversations associated with this project
        QList<QString> sessionsToDelete;
        for (const auto& s : state.sessions) {
            if (s.projectId == projectId) {
                sessionsToDelete.append(s.id);
            }
        }
        for (const auto& sessionId : sessionsToDelete) {
            if (m_conversationService) {
                QString dummyCurrent;
                m_conversationService->deleteSession(state.sessions, sessionId, dummyCurrent);
            }
        }
        
        state.projects.removeIf([&](const auto& p) { return p.id == projectId; });
        state.sessions.removeIf([&](const auto& s) { return s.projectId == projectId; });
        state.pinnedProjects.remove(projectId);
        
        if (state.currentProjectId == projectId) {
            cancelTask();
            m_agentSessionId.clear();
            if (!state.projects.isEmpty()) {
                selectProject(state.projects.first().id);
            } else {
                state.currentProjectId = {};
                state.currentSessionId.clear();
                state.messages.clear();
            }
        }
    });
}

void WorkViewModel::renameProject(const QUuid& projectId, const QString& newName) {
    if (newName.trimmed().isEmpty()) return;
    if (m_projectRepository) {
        auto projectOpt = m_projectRepository->getProject(projectId);
        if (projectOpt) {
            auto proj = *projectOpt;
            proj.name = newName.trimmed();
            m_projectRepository->saveProject(proj);
        }
    }
    updateState([projectId, newName](WorkState& state) {
        for (auto& p : state.projects) {
            if (p.id == projectId) {
                p.name = newName.trimmed();
                break;
            }
        }
        if (state.currentProjectId == projectId) {
            state.projectName = newName.trimmed();
        }
    });
}

void WorkViewModel::toggleProjectPinned(const QUuid& projectId) {
    updateState([this, projectId](WorkState& state) {
        if (state.pinnedProjects.contains(projectId)) {
            state.pinnedProjects.remove(projectId);
        } else {
            state.pinnedProjects.insert(projectId);
        }
        
        // Save to Database
        if (m_projectRepository) {
            auto optProject = m_projectRepository->getProject(projectId);
            if (optProject.has_value()) {
                auto p = optProject.value();
                p.isPinned = state.pinnedProjects.contains(projectId);
                m_projectRepository->saveProject(p);
            }
        }
        
        for (auto& p : state.projects) {
            if (p.id == projectId) {
                p.isPinned = state.pinnedProjects.contains(projectId);
                break;
            }
        }
    });
}

void WorkViewModel::archiveProjectSessions(const QUuid& projectId) {
    updateState([this, projectId](WorkState& state) {
        QList<QString> sessionsToArchive;
        for (const auto& s : state.sessions) {
            if (s.projectId == projectId && !s.isArchived) {
                sessionsToArchive.append(s.id);
            }
        }
        for (const auto& id : sessionsToArchive) {
            if (m_conversationService) {
                m_conversationService->setSessionArchived(state.sessions, id, true);
            }
        }
        if (state.currentProjectId == projectId && !state.currentSessionId.isEmpty()) {
            cancelTask();
            m_agentSessionId.clear();
            state.currentSessionId.clear();
            state.messages.clear();
        }
    });
}

void WorkViewModel::newSession() {
    if (m_currentProjectId.isNull()) return;
    cancelTask();
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_agentSessionId = id;
    if (m_conversationRepository) {
        domain::conversation::Conversation conversation;
        conversation.id = QUuid(id); conversation.title = QStringLiteral("新对话");
        conversation.projectId = m_currentProjectId; conversation.createdAt = QDateTime::currentDateTime(); conversation.updatedAt = conversation.createdAt;
        m_conversationRepository->saveConversation(conversation);
    }
    updateState([this, id](WorkState& state) {
        state.currentSessionId = id;
        state.currentTask.clear();
        state.messages.clear();
        state.sessions.prepend({id, QStringLiteral("新对话"), false, false, QDateTime::currentMSecsSinceEpoch(), m_currentProjectId});
    });
}

void WorkViewModel::loadSession(const QString& sessionId) {
    if (sessionId == m_agentSessionId) return;
    cancelTask();
    if (m_conversationRepository && m_projectRepository) {
        const auto conversation = m_conversationRepository->getConversation(QUuid(sessionId));
        if (conversation && conversation->projectId.has_value()) {
            const auto project = m_projectRepository->getProject(*conversation->projectId);
            if (project) {
                m_currentProjectId = project->id;
                setProjectRoot(project->rootPath);
            }
        }
    }
    m_agentSessionId = sessionId;
    updateState([this, sessionId](WorkState& state) {
        state.currentProjectId = m_currentProjectId;
        state.currentSessionId = sessionId;
        state.currentTask = QStringLiteral("新对话");
        for (const auto& item : state.sessions) if (item.id == sessionId) { state.currentTask = item.title; break; }
        state.messages = m_conversationService ? m_conversationService->loadMessages(sessionId) : QList<domain::conversation::Message>{};
    });
}

void WorkViewModel::setSessionPinned(const QString& sessionId, bool pinned) {
    updateState([this, sessionId, pinned](WorkState& state) {
        if (m_conversationService) {
            m_conversationService->setSessionPinned(state.sessions, sessionId, pinned);
        }
    });
}

void WorkViewModel::setSessionArchived(const QString& sessionId, bool archived) {
    updateState([this, sessionId, archived](WorkState& state) {
        if (m_conversationService) {
            m_conversationService->setSessionArchived(state.sessions, sessionId, archived);
        }
        if (archived && state.currentSessionId == sessionId) {
            cancelTask();
            m_agentSessionId.clear();
            const auto projId = state.currentProjectId;
            const auto it = std::find_if(state.sessions.cbegin(), state.sessions.cend(), [projId](const auto& item) { 
                return !item.isArchived && item.projectId == projId; 
            });
            if (it != state.sessions.cend()) { 
                const QString next = it->id; 
                QTimer::singleShot(0, this, [this, next] { loadSession(next); }); 
            } else {
                QTimer::singleShot(0, this, &WorkViewModel::newSession);
            }
        }
    });
}

void WorkViewModel::emitStateChanged() { Q_EMIT stateChanged(m_state); }
} // namespace ui::screen::work
