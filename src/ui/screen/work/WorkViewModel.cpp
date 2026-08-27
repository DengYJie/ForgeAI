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
                state.messages.append(message);
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
                state.agentUiState.permissionPendingCall = call;
                state.agentUiState.permissionRequired = permission;
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
                if (it != state.messages.end()) {
                    domain::conversation::ToolResultBlock results; results.results.append(result);
                    it->blocks.append({domain::BlockType::ToolResult, results});
                }

                for (auto rit = state.toolEvents.rbegin(); rit != state.toolEvents.rend(); ++rit) {
                    if (rit->result.isEmpty()) {
                        rit->result = result.content;
                        rit->isError = result.isError;
                        break;
                    }
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
    qInfo().noquote() << QStringLiteral("[WorkViewModel] grantPermission -> toolCallId: %1, granted: %2").arg(toolCallId).arg(granted);
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
        s.reasoningEffort = it->reasoningEfforts.contains(QStringLiteral("medium"))
            ? QStringLiteral("medium") : (it->reasoningEfforts.isEmpty() ? QString() : it->reasoningEfforts.first());
    });
}

void WorkViewModel::refreshAvailableModels(WorkState &s) {
    s.availableModels.clear();
    if (!m_useCases.getModels) return;
    const auto models = m_useCases.getModels->getEnabledResolvedModels();
    for (const auto& model : models) {
        const auto capabilities = model.effectiveCapabilities();
        QStringList efforts;
        const QJsonArray options = QJsonDocument::fromJson(model.binding.reasoningOptionsJson.toUtf8()).array();
        for (const auto& option : options) {
            const QJsonObject object = option.toObject();
            if (object.value(QStringLiteral("type")).toString() != QStringLiteral("effort")) continue;
            for (const auto& value : object.value(QStringLiteral("values")).toArray()) efforts.append(value.toString());
        }
        if (efforts.isEmpty() && model.binding.canonicalModelId.has_value())
            efforts = canonicalReasoningEfforts(*model.binding.canonicalModelId);
        s.availableModels.append({
            model.provider.id, model.requestModelId(), model.displayName(), model.provider.name,
            capabilities.testFlag(domain::model::ModelCapability::Vision)
                || capabilities.testFlag(domain::model::ModelCapability::Pdf)
                || capabilities.testFlag(domain::model::ModelCapability::Audio)
                || capabilities.testFlag(domain::model::ModelCapability::Video),
            model.provider.type == domain::model::ProviderType::OpenAIResponses,
            capabilities.testFlag(domain::model::ModelCapability::Thinking), efforts
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
        s.reasoningEffort = fallback.reasoningEfforts.contains(QStringLiteral("medium"))
            ? QStringLiteral("medium") : (fallback.reasoningEfforts.isEmpty() ? QString() : fallback.reasoningEfforts.first());
    }
}

void WorkViewModel::emitStateChanged() { Q_EMIT stateChanged(m_state); }
} // namespace ui::screen::work
