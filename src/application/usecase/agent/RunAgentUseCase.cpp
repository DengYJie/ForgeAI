#include "RunAgentUseCase.h"

#include <algorithm>

namespace application::usecase::agent {

    RunAgentUseCase::RunAgentUseCase(
        ports::IAgentRuntime* runtime,
        domain::service::IModelService* modelService,
        domain::service::IProjectContextService* projectContextService,
        QObject* parent
    ) : QObject(parent),
        m_runtime(runtime),
        m_modelService(modelService),
        m_projectContextService(projectContextService) {

        if (m_runtime) {
            connect(m_runtime, &ports::IAgentRuntime::userMessageCreated, this, &RunAgentUseCase::userMessageCreated);
            connect(m_runtime, &ports::IAgentRuntime::stateChanged, this, &RunAgentUseCase::stateChanged);
            connect(m_runtime, &ports::IAgentRuntime::tokenReceived, this, &RunAgentUseCase::tokenReceived);
            connect(m_runtime, &ports::IAgentRuntime::thoughtReceived, this, &RunAgentUseCase::thoughtReceived);
            connect(m_runtime, &ports::IAgentRuntime::toolCallStarted, this, &RunAgentUseCase::toolCallStarted);
            connect(m_runtime, &ports::IAgentRuntime::toolCallFinished, this, &RunAgentUseCase::toolCallFinished);
            connect(m_runtime, &ports::IAgentRuntime::toolResultReady, this, &RunAgentUseCase::toolResultReady);
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
        const QString& customSystemPrompt
    ) {
        if (!m_runtime) {
            domain::llm::ChatError err;
            err.category = domain::llm::ChatErrorCategory::Configuration;
            err.code = QStringLiteral("MissingRuntime");
            err.userMessage = QStringLiteral("Agent 运行时未就绪。");
            emit runFailed(sessionId, err);
            return;
        }

        if (!m_modelService) {
            domain::llm::ChatError err;
            err.category = domain::llm::ChatErrorCategory::Configuration;
            err.code = QStringLiteral("MissingModelService");
            err.userMessage = QStringLiteral("模型服务未就绪。");
            emit runFailed(sessionId, err);
            return;
        }

        const auto models = m_modelService->getEnabledResolvedModels();
        if (models.isEmpty()) {
            domain::llm::ChatError err;
            err.category = domain::llm::ChatErrorCategory::Configuration;
            err.code = QStringLiteral("NoActiveProvider");
            err.userMessage = QStringLiteral("未配置或启用任何模型提供商，请在设置中添加。");
            err.suggestedAction = QStringLiteral("OpenSettings");
            emit runFailed(sessionId, err);
            return;
        }

        const auto selected = std::find_if(models.cbegin(), models.cend(), [&](const domain::model::ResolvedModel& model) {
            return (providerId.isEmpty() || model.provider.id == providerId)
                && (modelId.isEmpty() || model.requestModelId() == modelId);
        });

        if (selected == models.cend()) {
            domain::llm::ChatError err;
            err.category = domain::llm::ChatErrorCategory::Configuration;
            err.code = QStringLiteral("SelectedModelUnavailable");
            err.userMessage = QStringLiteral("所选模型不可用，请重新选择。");
            emit runFailed(sessionId, err);
            return;
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

        if (!customSystemPrompt.trimmed().isEmpty()) {
            context.systemPrompt = customSystemPrompt;
        } else if (m_projectContextService && !workspaceRoot.isEmpty()) {
            const auto projCtx = m_projectContextService->load(workspaceRoot);
            context.systemPrompt = m_contextBuilder.buildSystemPrompt(context, projCtx);
        } else {
            domain::project::ProjectContext emptyProjCtx;
            emptyProjCtx.rootPath = workspaceRoot;
            context.systemPrompt = m_contextBuilder.buildSystemPrompt(context, emptyProjCtx);
        }

        m_runtime->startRun(context, prompt);
    }

} // namespace application::usecase::agent
