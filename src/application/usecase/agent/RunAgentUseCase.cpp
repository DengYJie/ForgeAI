#include "RunAgentUseCase.h"

#include <algorithm>
#include <QDir>
#include <QFile>
#include <QDebug>

namespace application::usecase::agent {

    RunAgentUseCase::RunAgentUseCase(
        ports::IAgentRuntime* runtime,
        domain::service::IModelService* modelService,
        domain::service::IProjectContextService* projectContextService,
        ports::IProjectRuntimeCoordinator* runtimeCoordinator,
        domain::repository::IAgentRepository* agentRepository,
        ::agent::skill::SkillRegistry* skillRegistry,
        QObject* parent
    ) : QObject(parent),
        m_runtime(runtime),
        m_modelService(modelService),
        m_projectContextService(projectContextService),
        m_runtimeCoordinator(runtimeCoordinator),
        m_agentRepository(agentRepository),
        m_skillRegistry(skillRegistry) {

        if (m_runtime) {
            connect(m_runtime, &ports::IAgentRuntime::userMessageCreated, this, &RunAgentUseCase::userMessageCreated);
            connect(m_runtime, &ports::IAgentRuntime::assistantMessageStarted, this, &RunAgentUseCase::assistantMessageStarted);
            connect(m_runtime, &ports::IAgentRuntime::stateChanged, this, &RunAgentUseCase::stateChanged);
            connect(m_runtime, &ports::IAgentRuntime::tokenReceived, this, &RunAgentUseCase::tokenReceived);
            connect(m_runtime, &ports::IAgentRuntime::thoughtReceived, this, &RunAgentUseCase::thoughtReceived);
            connect(m_runtime, &ports::IAgentRuntime::toolCallFinished, this, &RunAgentUseCase::toolCallFinished);
            connect(m_runtime, &ports::IAgentRuntime::toolResultReady, this, &RunAgentUseCase::toolResultReady);
            connect(m_runtime, &ports::IAgentRuntime::permissionRequested, this, &RunAgentUseCase::permissionRequested);
            connect(m_runtime, &ports::IAgentRuntime::replyGenerated, this, &RunAgentUseCase::replyGenerated);
            connect(m_runtime, &ports::IAgentRuntime::runCompleted, this, &RunAgentUseCase::runCompleted);
            connect(m_runtime, &ports::IAgentRuntime::runFailed, this, &RunAgentUseCase::runFailed);
        }
    }

    ports::IAgentRuntime* RunAgentUseCase::runtime() const {
        return m_runtime;
    }

    bool RunAgentUseCase::isRunning() const {
        return m_runtime ? m_runtime->isRunning() : false;
    }

    void RunAgentUseCase::cancelCurrent() {
        if (m_runtime) {
            m_runtime->cancelRun();
        }
    }

    void RunAgentUseCase::grantPermission(const QString& sessionId, const QString& toolCallId, bool granted) {
        if (m_runtime) {
            m_runtime->grantPermission(sessionId, toolCallId, granted);
        }
    }

    void RunAgentUseCase::execute(
        const QString& sessionId,
        const QString& prompt,
        const QUuid& projectId,
        const QString& workspaceRoot,
        const QString& providerId,
        const QString& modelId,
        bool useWebSearch,
        bool useDeepThinking,
        const QString& reasoningEffort,
        const QString& customSystemPrompt,
        const QUuid& agentId
    ) {
        if (!m_runtime) {
            domain::llm::ChatError err;
            err.category = domain::llm::ChatErrorCategory::Configuration;
            err.code = QStringLiteral("MissingRuntime");
            err.userMessage = QStringLiteral("Agent 运行时未就绪。");
            qWarning().noquote() << QStringLiteral("[RunAgentUseCase] execute failed: MissingRuntime");
            emit runFailed(sessionId, err);
            return;
        }

        if (!m_modelService) {
            domain::llm::ChatError err;
            err.category = domain::llm::ChatErrorCategory::Configuration;
            err.code = QStringLiteral("MissingModelService");
            err.userMessage = QStringLiteral("模型服务未就绪。");
            qWarning().noquote() << QStringLiteral("[RunAgentUseCase] execute failed: MissingModelService");
            emit runFailed(sessionId, err);
            return;
        }

        // 查找关联的 Agent 配置（如指定 agentId 或项目默认配置）
        std::optional<domain::agent::Agent> agentConfig;
        if (m_agentRepository) {
            if (!agentId.isNull()) {
                agentConfig = m_agentRepository->getAgent(agentId);
            } else if (!projectId.isNull()) {
                const auto agents = m_agentRepository->getAllAgents();
                for (const auto& a : agents) {
                    if (a.projectId.has_value() && a.projectId.value() == projectId) {
                        agentConfig = a;
                        break;
                    }
                }
            }
        }

        QString effProviderId = providerId;
        QString effModelId = modelId;
        QString effSystemPrompt = customSystemPrompt;
        QStringList enabledMcpServers;
        QStringList enabledSkills;

        if (agentConfig.has_value()) {
            if (effProviderId.isEmpty() && !agentConfig->providerId.isEmpty()) {
                effProviderId = agentConfig->providerId;
            }
            if (effModelId.isEmpty() && !agentConfig->modelId.isEmpty()) {
                effModelId = agentConfig->modelId;
            }
            if (effSystemPrompt.isEmpty() && !agentConfig->systemPrompt.isEmpty()) {
                effSystemPrompt = agentConfig->systemPrompt;
            }
            enabledMcpServers = agentConfig->enabledMcpServerIds;
            enabledSkills = agentConfig->enabledSkills;
        }

        const auto models = m_modelService->getEnabledResolvedModels();
        if (models.isEmpty()) {
            domain::llm::ChatError err;
            err.category = domain::llm::ChatErrorCategory::Configuration;
            err.code = QStringLiteral("NoActiveProvider");
            err.userMessage = QStringLiteral("未配置或启用任何模型提供商，请在设置中添加。");
            err.suggestedAction = QStringLiteral("OpenSettings");
            qWarning().noquote() << QStringLiteral("[RunAgentUseCase] execute failed: NoActiveProvider");
            emit runFailed(sessionId, err);
            return;
        }

        const auto selected = std::find_if(models.cbegin(), models.cend(), [&](const domain::model::ResolvedModel& model) {
            return (effProviderId.isEmpty() || model.provider.id == effProviderId)
                && (effModelId.isEmpty() || model.requestModelId() == effModelId);
        });

        if (selected == models.cend()) {
            domain::llm::ChatError err;
            err.category = domain::llm::ChatErrorCategory::Configuration;
            err.code = QStringLiteral("SelectedModelUnavailable");
            err.userMessage = QStringLiteral("所选模型不可用，请重新选择。");
            qWarning().noquote() << QStringLiteral("[RunAgentUseCase] execute failed: SelectedModelUnavailable (provider=%1, model=%2)")
                .arg(effProviderId, effModelId);
            emit runFailed(sessionId, err);
            return;
        }

        qInfo().noquote() << QStringLiteral("[RunAgentUseCase] execute -> resolved model: %1 (%2)")
            .arg(selected->displayName(), selected->requestModelId());

        // 通过项目运行时协调器挂载项目关联资源（如 MCP 外部扩展）
        if (m_runtimeCoordinator && !workspaceRoot.isEmpty()) {
            m_runtimeCoordinator->loadProject(workspaceRoot, enabledMcpServers);
        }

        ::agent::runtime::AgentRunContext context;
        context.runId = QUuid::createUuid();
        context.sessionId = sessionId;
        context.projectId = projectId;
        context.workspaceRoot = workspaceRoot;
        context.provider = selected->provider;
        context.modelId = selected->requestModelId();
        context.useWebSearch = useWebSearch;
        context.useDeepThinking = useDeepThinking;
        context.reasoningEffort = reasoningEffort;
        if (agentConfig.has_value()) {
            context.enabledTools = agentConfig->enabledTools;
        }

        if (!effSystemPrompt.trimmed().isEmpty()) {
            context.systemPrompt = effSystemPrompt;
        } else {
            domain::project::ProjectContext projCtx;
            if (m_projectContextService && !workspaceRoot.isEmpty()) {
                projCtx = m_projectContextService->load(workspaceRoot);
            } else {
                projCtx.rootPath = workspaceRoot;
            }

            if (m_skillRegistry) {
                m_skillRegistry->registerSkills(projCtx.skills);
            }

            QList<domain::agent::Skill> filteredSkills;
            for (const auto& sk : projCtx.skills) {
                const QString skId = sk.id.isEmpty() ? sk.name : sk.id;
                if (!enabledSkills.isEmpty() && !enabledSkills.contains(sk.id) && !enabledSkills.contains(sk.name)) {
                    continue;
                }
                if (m_skillRegistry) {
                    auto regSkill = m_skillRegistry->findSkill(skId);
                    if (!regSkill.has_value() || !regSkill->isEnabled) {
                        continue;
                    }
                } else if (!sk.isEnabled) {
                    continue;
                }
                filteredSkills.append(sk);
            }

            context.systemPrompt = m_contextBuilder.buildSystemPrompt(context, projCtx, filteredSkills);
        }

        m_runtime->startRun(context, prompt);
    }

} // namespace application::usecase::agent
