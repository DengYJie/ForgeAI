#include "WorkViewModel.h"

#include "domain/service/IProjectContextService.h"
#include "domain/service/IConversationService.h"
#include "domain/service/IModelService.h"
#include "domain/repository/IConversationRepository.h"
#include "domain/repository/IProjectRepository.h"
#include "application/usecase/work/SwitchProjectUseCase.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QUuid>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <algorithm>

namespace ui::screen::work {
namespace {
QStringList canonicalReasoningEfforts(const QString& canonicalId) {
    static QHash<QString, QStringList> cache;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        QFile file(QStringLiteral(":/config/models.json"));
        if (file.open(QIODevice::ReadOnly)) {
            const QJsonObject models = QJsonDocument::fromJson(file.readAll()).object();
            for (auto it = models.begin(); it != models.end(); ++it) {
                QStringList values;
                for (const auto& option : it.value().toObject().value(QStringLiteral("reasoning_options")).toArray()) {
                    const QJsonObject object = option.toObject();
                    if (object.value(QStringLiteral("type")).toString() != QStringLiteral("effort")) continue;
                    for (const auto& value : object.value(QStringLiteral("values")).toArray()) values.append(value.toString());
                }
                cache.insert(it.key(), values);
            }
        }
    }
    return cache.value(canonicalId);
}


}

WorkViewModel::WorkViewModel(const application::usecase::work::WorkUseCases& useCases,
                             domain::service::IProjectContextService* projectContext, QObject* parent)
    : BaseViewModel<WorkViewModel, WorkState>(parent), m_useCases(useCases), m_projectContext(projectContext),
      m_conversationService(useCases.conversationService),
      m_conversationRepository(useCases.conversationRepository), m_projectRepository(useCases.projectRepository) {
    setupUseCaseConnections();
    if (m_projectRepository) {
        auto projects = m_projectRepository->getAllProjects();
        QList<ui::screen::chat::ChatSessionItemData> sessions;
        if (m_conversationRepository) for (const auto& conversation : m_conversationRepository->getAllConversations()) {
            if (!conversation.projectId.has_value()) continue;
            sessions.append({conversation.id.toString(), conversation.title, conversation.isPinned, conversation.isArchived,
                             conversation.updatedAt.toMSecsSinceEpoch(), conversation.projectId});
        }
        qInfo().noquote() << QStringLiteral("[WorkViewModel] Initialized with %1 project sessions across %2 projects")
            .arg(QString::number(sessions.size()), QString::number(projects.size()));
        updateState([this, projects, sessions](WorkState& state) { 
            state.projects = projects; 
            state.sessions = sessions; 
            for (const auto& p : projects) {
                if (p.isPinned) state.pinnedProjects.insert(p.id);
            }
            refreshAvailableModels(state);
        });
        if (!projects.isEmpty()) {
            selectProject(projects.first().id);
        } else {
            setProjectRoot(QDir::currentPath());
        }
    } else {
        setProjectRoot(QDir::currentPath());
        updateState([this](WorkState& state) {
            refreshAvailableModels(state);
        });
    }
}

WorkViewModel::~WorkViewModel() = default;

void WorkViewModel::setupUseCaseConnections() {
    if (m_useCases.getModels) {
        connect(m_useCases.getModels, &application::usecase::settings::GetModelsUseCase::modelsChanged, this, [this] {
            updateState([this](WorkState& state) { refreshAvailableModels(state); });
        });
    }

    auto* agent = m_useCases.runAgent;
    if (agent) {
        connect(agent, &application::usecase::agent::RunAgentUseCase::userMessageCreated, this,
                [this](const QString& sessionId, const domain::conversation::Message& message) {
            if (sessionId != m_agentSessionId) return;
            qInfo().noquote() << QStringLiteral("[WorkViewModel] userMessageCreated -> sessionId: %1, msgId: %2")
                .arg(sessionId, message.id.toString(QUuid::WithoutBraces));
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
        connect(agent, &application::usecase::agent::RunAgentUseCase::assistantMessageStarted, this,
                [this](const QString& sessionId, const domain::conversation::Message& message) {
            if (sessionId != m_agentSessionId) return;
            qInfo().noquote() << QStringLiteral("[WorkViewModel] assistantMessageStarted -> sessionId: %1, msgId: %2")
                .arg(sessionId, message.id.toString(QUuid::WithoutBraces));
            updateState([message](WorkState& state) {
                auto it = std::find_if(state.messages.begin(), state.messages.end(), [&](const auto& msg) {
                    return msg.id == message.id;
                });
                if (it == state.messages.end()) {
                    state.messages.append(message);
                }
            });
        });
        connect(agent, &application::usecase::agent::RunAgentUseCase::tokenReceived, this,
                [this](const QString& sessionId, const QUuid& messageId, const QString& token) {
            if (sessionId != m_agentSessionId || token.isEmpty()) return;
            updateState([messageId, token](WorkState& state) {
                auto it = std::find_if(state.messages.begin(), state.messages.end(), [&](const auto& msg) {
                    return msg.id == messageId;
                });
                if (it == state.messages.end()) return;
                for (auto& block : it->blocks) {
                    if (block.isText()) { std::get<domain::conversation::TextBlock>(block.payload).text += token; return; }
                }
                it->blocks.append({domain::BlockType::Text, domain::conversation::TextBlock{token}});
            });
        });
        connect(agent, &application::usecase::agent::RunAgentUseCase::thoughtReceived, this,
                [this](const QString& sessionId, const QUuid& messageId, const QString& thought) {
            if (sessionId != m_agentSessionId || thought.isEmpty()) return;
            updateState([messageId, thought](WorkState& state) {
                auto it = std::find_if(state.messages.begin(), state.messages.end(), [&](const auto& msg) {
                    return msg.id == messageId;
                });
                if (it == state.messages.end()) return;
                for (auto& block : it->blocks) {
                    if (block.isThought()) { std::get<domain::conversation::ThoughtBlock>(block.payload).thought += thought; return; }
                }
                it->blocks.append({domain::BlockType::Thought, domain::conversation::ThoughtBlock{thought, 0}});
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
            qInfo().noquote() << QStringLiteral("[WorkViewModel] permissionRequested -> tool: %1, id: %2, reason: %3")
                .arg(call.name, call.id, permission.reason);
            updateState([call, permission](WorkState& state) {
                state.agentUiState.isWaitingPermission = true;
                bool exists = false;
                for (const auto& item : state.agentUiState.pendingPermissions) {
                    if (item.call.id == call.id) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    state.agentUiState.pendingPermissions.append({call, permission});
                }
                state.statusMessage = QStringLiteral("等待用户授权操作: %1").arg(permission.reason);
            });
        });
        connect(agent, &application::usecase::agent::RunAgentUseCase::toolCallFinished, this,
                [this](const QString& sessionId, const QUuid& messageId, const domain::agent::ToolCall& call) {
            if (sessionId != m_agentSessionId) return;
            qInfo().noquote() << QStringLiteral("[WorkViewModel] toolCallFinished -> tool: %1, id: %2, args: %3")
                .arg(call.name, call.id, call.arguments);
            updateState([messageId, call](WorkState& state) {
                auto it = std::find_if(state.messages.begin(), state.messages.end(), [&](const auto& msg) {
                    return msg.id == messageId;
                });
                if (it != state.messages.end()) {
                    domain::conversation::ToolCallBlock calls; calls.calls.append(call);
                    it->blocks.append({domain::BlockType::ToolCall, calls});
                }

                state.agentUiState.activeToolName = call.name;
                state.toolEvents.append(WorkState::ToolEvent{
                    call.id,
                    call.name,
                    call.arguments,
                    {},
                    false
                });
            });
        });
        connect(agent, &application::usecase::agent::RunAgentUseCase::toolResultReady, this,
                [this](const QString& sessionId, const QUuid& messageId, const domain::agent::ToolResult& result) {
            if (sessionId != m_agentSessionId) return;
            qInfo().noquote() << QStringLiteral("[WorkViewModel] toolResultReady -> id: %1, isError: %2, content: %3")
                .arg(result.toolCallId).arg(result.isError).arg(result.content.left(100));
            updateState([messageId, result](WorkState& state) {
                auto it = std::find_if(state.messages.begin(), state.messages.end(), [&](const auto& msg) {
                    return msg.id == messageId;
                });
                if (it == state.messages.end() && !state.messages.isEmpty() && state.messages.last().role == domain::MessageRole::Assistant) {
                    it = std::prev(state.messages.end());
                }
                if (it != state.messages.end()) {
                    domain::conversation::ToolResultBlock results;
                    results.results.append(result);
                    it->blocks.append({domain::BlockType::ToolResult, results});
                }

                auto eventIt = std::find_if(state.toolEvents.begin(), state.toolEvents.end(), [&](const auto& ev) {
                    return ev.toolCallId == result.toolCallId;
                });
                if (eventIt != state.toolEvents.end()) {
                    eventIt->result = result.content;
                    eventIt->isError = result.isError;
                }
            });
        });
        connect(agent, &application::usecase::agent::RunAgentUseCase::replyGenerated, this,
                [this](const QString& sessionId, const domain::conversation::Message& message) {
            if (sessionId != m_agentSessionId) return;
            qInfo().noquote() << QStringLiteral("[WorkViewModel] replyGenerated -> msgId: %1, blocks: %2")
                .arg(message.id.toString(QUuid::WithoutBraces)).arg(message.blocks.size());
            updateState([message](WorkState& state) {
                auto it = std::find_if(state.messages.begin(), state.messages.end(), [&](const auto& msg) {
                    return msg.id == message.id;
                });
                if (it != state.messages.end()) {
                    *it = message;
                } else {
                    state.messages.append(message);
                }
            });
        });
        connect(agent, &application::usecase::agent::RunAgentUseCase::runCompleted, this,
                [this](const QString& sessionId) {
            if (sessionId != m_agentSessionId) return;
            qInfo().noquote() << QStringLiteral("[WorkViewModel] runCompleted for session: %1").arg(sessionId);
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
            qWarning().noquote() << QStringLiteral("[WorkViewModel] runFailed -> code: %1, msg: %2, userMsg: %3")
                .arg(error.code, error.message, error.userMessage);
            const QString displayErr = error.userMessage.isEmpty() ? error.message : error.userMessage;
            updateState([error, displayErr](WorkState& state) {
                state.isProcessing = false;
                state.statusMessage = displayErr;
                state.agentUiState.status = domain::agent::AgentRunStatus::Failed;
                state.agentUiState.isWaitingPermission = false;

                // 如果存在未完成的 Assistant 消息占位，将其标记为失败并在末尾追加错误信息
                if (!state.messages.isEmpty() && state.messages.last().role == domain::MessageRole::Assistant) {
                    auto& lastMsg = state.messages.last();
                    lastMsg.status = domain::MessageStatus::Failed;
                    lastMsg.blocks.append({domain::BlockType::Text, domain::conversation::TextBlock{
                        QStringLiteral("**任务执行失败**：%1").arg(displayErr)
                    }});
                }
            });
        });
    }
}

void WorkViewModel::grantPermission(const QString& toolCallId, bool granted, domain::agent::PermissionScope scope, const QString& customInput) {
    if (m_agentSessionId.isEmpty() || !m_useCases.runAgent) return;
    qInfo().noquote() << QStringLiteral("[WorkViewModel] grantPermission -> toolCallId: %1, granted: %2, scope: %3, customInput: %4")
        .arg(toolCallId).arg(granted).arg(static_cast<int>(scope)).arg(customInput);
    updateState([toolCallId](WorkState& state) {
        state.agentUiState.pendingPermissions.removeIf([&](const auto& p) {
            return p.call.id == toolCallId;
        });
        state.agentUiState.isWaitingPermission = !state.agentUiState.pendingPermissions.isEmpty();
        if (!state.agentUiState.isWaitingPermission) {
            state.statusMessage.clear();
        }
    });
    m_useCases.runAgent->grantPermission(m_agentSessionId, toolCallId, granted, scope, customInput);
}

QString WorkViewModel::taskTitle(const QString& task) {
    const QString trimmed = task.trimmed();
    return trimmed.left(36) + (trimmed.size() > 36 ? QStringLiteral("…") : QString());
}

void WorkViewModel::startTask(const QString& task) {
    const QString trimmed = task.trimmed();
    if (trimmed.isEmpty()) {
        qWarning().noquote() << QStringLiteral("[WorkViewModel] startTask ignored: prompt is empty");
        return;
    }
    if (m_currentProjectId.isNull()) {
        qWarning().noquote() << QStringLiteral("[WorkViewModel] startTask ignored: m_currentProjectId is null");
        return;
    }
    if (m_agentSessionId.isEmpty()) {
        qInfo().noquote() << QStringLiteral("[WorkViewModel] startTask: m_agentSessionId is empty, creating new session...");
        newSession();
    }
    qInfo().noquote() << QStringLiteral("[WorkViewModel] startTask -> session: %1, project: %2, provider: %3, model: %4, prompt: %5")
        .arg(m_agentSessionId, m_currentProjectId.toString(QUuid::WithoutBraces), m_state.currentModelProviderId, m_state.currentModelId, trimmed);
    updateState([trimmed](WorkState& state) { state.currentTask = taskTitle(trimmed); state.statusMessage.clear(); });
    if (m_useCases.runAgent) {
        // 调用 RunAgentUseCase 启动智能体编排任务
        m_useCases.runAgent->execute(
            m_agentSessionId,
            trimmed,
            m_currentProjectId,
            m_state.projectRoot,
            m_state.currentModelProviderId,
            m_state.currentModelId,
            m_state.useWebSearch,
            m_state.useDeepThinking,
            m_state.reasoningEffort
        );
    } else {
        qWarning().noquote() << QStringLiteral("[WorkViewModel] startTask failed: m_useCases.runAgent is null!");
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
    if (m_currentProjectId == projectId && m_state.currentProjectId == projectId) {
        // 当前项目已处于选中状态，点击根节点（折叠/展开）不重复切换或清空当前会话
        return;
    }
    const auto it = std::find_if(m_state.projects.cbegin(), m_state.projects.cend(), [projectId](const auto& project) { return project.id == projectId; });
    if (it == m_state.projects.cend()) return;

    cancelTask();
    m_currentProjectId = projectId;
    setProjectRoot(it->rootPath);
    updateState([projectId](WorkState& state) {
        state.currentProjectId = projectId;
    });

    // 切换项目时：优先加载该项目最近的已有会话；若项目尚无历史会话，才创建新对话
    QString latestSessionId;
    for (const auto& s : m_state.sessions) {
        if (s.projectId.has_value() && s.projectId.value() == projectId && !s.isArchived) {
            latestSessionId = s.id;
            break;
        }
    }
    if (!latestSessionId.isEmpty()) {
        loadSession(latestSessionId);
    } else {
        newSession();
    }
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
    if (projectId.isNull()) return;

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

    if (m_conversationRepository) {
        for (const auto& s : m_state.sessions) {
            if (s.projectId == projectId) {
                m_conversationRepository->deleteConversation(QUuid(s.id));
            }
        }
    }

    const bool isCurrent = (m_currentProjectId == projectId);
    if (isCurrent) {
        cancelTask();
        m_agentSessionId.clear();
    }

    QUuid nextProjectId;
    updateState([projectId, isCurrent, &nextProjectId](WorkState& state) {
        state.projects.removeIf([&](const auto& p) { return p.id == projectId; });
        state.sessions.removeIf([&](const auto& s) { return s.projectId == projectId; });
        state.pinnedProjects.remove(projectId);

        if (isCurrent) {
            if (!state.projects.isEmpty()) {
                nextProjectId = state.projects.first().id;
            } else {
                state.currentProjectId = {};
                state.currentSessionId.clear();
                state.currentTask.clear();
                state.messages.clear();
                state.toolEvents.clear();
                state.agentUiState = {};
            }
        }
    });

    if (isCurrent && !nextProjectId.isNull()) {
        selectProject(nextProjectId);
    }
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
        qInfo().noquote() << QStringLiteral("[WorkViewModel] newSession created -> sessionId: %1, projectId: %2")
            .arg(id, m_currentProjectId.toString(QUuid::WithoutBraces));
    }
    updateState([this, id](WorkState& state) {
        state.currentProjectId = m_currentProjectId;
        state.currentSessionId = id;
        state.currentTask.clear();
        state.messages.clear();
        state.toolEvents.clear();
        state.agentUiState = {};
        state.sessions.prepend({id, QStringLiteral("新对话"), false, false, QDateTime::currentMSecsSinceEpoch(), m_currentProjectId});
    });
}

void WorkViewModel::loadSession(const QString& sessionId) {
    if (sessionId == m_agentSessionId) return;
    cancelTask();
    QUuid projectId = m_currentProjectId;
    QString rootPath;
    if (m_conversationRepository && m_projectRepository) {
        const auto conversation = m_conversationRepository->getConversation(QUuid(sessionId));
        if (conversation && conversation->projectId.has_value()) {
            const auto project = m_projectRepository->getProject(*conversation->projectId);
            if (project) {
                projectId = project->id;
                rootPath = project->rootPath;
            }
        }
    }
    m_currentProjectId = projectId;
    if (!rootPath.isEmpty()) {
        setProjectRoot(rootPath);
    }
    m_agentSessionId = sessionId;
    const auto loadedMessages = m_conversationService
        ? m_conversationService->loadMessages(sessionId)
        : QList<domain::conversation::Message>{};

    qInfo().noquote() << QStringLiteral("[WorkViewModel] loadSession -> sessionId: %1, projectId: %2, loaded %3 messages")
        .arg(sessionId, projectId.toString(QUuid::WithoutBraces), QString::number(loadedMessages.size()));

    updateState([this, sessionId, projectId, loadedMessages](WorkState& state) {
        state.currentProjectId = projectId;
        state.currentSessionId = sessionId;
        state.currentTask = QStringLiteral("新对话");
        for (const auto& item : state.sessions) {
            if (item.id == sessionId) {
                state.currentTask = item.title;
                break;
            }
        }
        state.messages = loadedMessages;
        state.toolEvents.clear();
        state.agentUiState = {};
    });
}

void WorkViewModel::setSessionPinned(const QString& sessionId, bool pinned) {
    updateState([this, sessionId, pinned](WorkState& state) {
        if (m_conversationService) {
            m_conversationService->setSessionPinned(state.sessions, sessionId, pinned);
        } else {
            for (auto& s : state.sessions) {
                if (s.id == sessionId) {
                    s.isPinned = pinned;
                    break;
                }
            }
        }
    });
}

void WorkViewModel::setSessionArchived(const QString& sessionId, bool archived) {
    bool shouldSwitch = false;
    QString nextSessionId;
    const bool isCurrent = (m_agentSessionId == sessionId);

    updateState([this, sessionId, archived, isCurrent, &shouldSwitch, &nextSessionId](WorkState& state) {
        if (m_conversationService) {
            m_conversationService->setSessionArchived(state.sessions, sessionId, archived);
        } else {
            for (auto& s : state.sessions) {
                if (s.id == sessionId) {
                    s.isArchived = archived;
                    break;
                }
            }
        }
        if (archived && isCurrent) {
            shouldSwitch = true;
            const auto projId = state.currentProjectId;
            const auto it = std::find_if(state.sessions.cbegin(), state.sessions.cend(), [projId](const auto& item) { 
                return !item.isArchived && item.projectId == projId; 
            });
            if (it != state.sessions.cend()) { 
                nextSessionId = it->id; 
            }
        }
    });

    if (shouldSwitch) {
        cancelTask();
        m_agentSessionId.clear();
        if (!nextSessionId.isEmpty()) {
            loadSession(nextSessionId);
        } else {
            newSession();
        }
    }
}

void WorkViewModel::setWebSearchEnabled(bool enabled) {
    updateState([enabled](WorkState& state) { state.useWebSearch = enabled; });
}

void WorkViewModel::setDeepThinkingEnabled(bool enabled) {
    updateState([enabled](WorkState& state) { state.useDeepThinking = enabled; });
}

void WorkViewModel::setReasoningEffort(const QString& effort) {
    updateState([effort](WorkState& state) {
        state.reasoningEffort = effort;
        state.useDeepThinking = !effort.isEmpty() && effort != QStringLiteral("none");
    });
}

void WorkViewModel::setModel(const QString &providerId, const QString &modelId) {
    updateState([providerId, modelId](WorkState &s) {
        const auto it = std::find_if(s.availableModels.cbegin(), s.availableModels.cend(), [&](const ui::screen::chat::ChatModelOption& option) {
            return option.providerId == providerId && option.modelId == modelId;
        });
        if (it == s.availableModels.cend()) return;
        s.currentModelProviderId = it->providerId;
        s.currentModelId = it->modelId;
        s.currentModelName = it->displayName;
        s.useDeepThinking = s.useDeepThinking && it->supportsDeepThinking;
        s.useWebSearch = s.useWebSearch && it->supportsWebSearch;
        s.reasoningEffort = it->reasoningEfforts.contains(QStringLiteral("medium"))
            ? QStringLiteral("medium") : (it->reasoningEfforts.isEmpty() ? QString() : it->reasoningEfforts.first());
    });
}

void WorkViewModel::refreshAvailableModels() {
    updateState([this](WorkState& state) {
        refreshAvailableModels(state);
    });
}

void WorkViewModel::refreshAvailableModels(WorkState &s) {
    s.availableModels.clear();
    if (!m_useCases.getModels) return;
    const auto models = m_useCases.getModels->getEnabledResolvedModels();
    for (const auto& model : models) {
        const auto capabilities = model.effectiveCapabilities();
        QStringList efforts;
        if (model.binding.reasoningSupport.has_value()) {
            efforts = model.binding.reasoningSupport->effortLevels;
        }
        if (efforts.isEmpty() && model.binding.canonicalModelId.has_value()) {
            efforts = canonicalReasoningEfforts(*model.binding.canonicalModelId);
        }

        const bool supportsAttachments = capabilities.testFlag(domain::model::ModelCapability::Vision)
            || capabilities.testFlag(domain::model::ModelCapability::Pdf)
            || capabilities.testFlag(domain::model::ModelCapability::Audio)
            || capabilities.testFlag(domain::model::ModelCapability::Video);
        const bool supportsWeb = (model.provider.protocol == domain::model::ProtocolType::OpenAIResponses);
        const bool supportsThinking = capabilities.testFlag(domain::model::ModelCapability::Thinking)
            || (model.binding.reasoningSupport.has_value() && model.binding.reasoningSupport->supported);

        s.availableModels.append({
            model.provider.id,
            model.requestModelId(),
            model.displayName(),
            model.provider.name,
            supportsAttachments,
            supportsWeb,
            supportsThinking,
            efforts
        });
    }
    if (s.availableModels.isEmpty()) {
        s.currentModelProviderId.clear();
        s.currentModelId.clear();
        s.currentModelName = QStringLiteral("选择模型");
        return;
    }
    const auto current = std::find_if(s.availableModels.cbegin(), s.availableModels.cend(), [&](const ui::screen::chat::ChatModelOption& option) {
        return option.providerId == s.currentModelProviderId && option.modelId == s.currentModelId;
    });
    if (current == s.availableModels.cend()) {
        const auto& fallback = s.availableModels.first();
        s.currentModelProviderId = fallback.providerId;
        s.currentModelId = fallback.modelId;
        s.currentModelName = fallback.displayName;
        s.useDeepThinking = s.useDeepThinking && fallback.supportsDeepThinking;
        s.useWebSearch = s.useWebSearch && fallback.supportsWebSearch;
        s.reasoningEffort = fallback.reasoningEfforts.contains(QStringLiteral("medium"))
            ? QStringLiteral("medium") : (fallback.reasoningEfforts.isEmpty() ? QString() : fallback.reasoningEfforts.first());
    }
}

void WorkViewModel::emitStateChanged() { Q_EMIT stateChanged(m_state); }
} // namespace ui::screen::work
