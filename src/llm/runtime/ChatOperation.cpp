#include "ChatOperation.h"
#include "RetryDecision.h"
#include <QVariant>
#include <QUuid>

namespace llm::runtime {

    ChatOperation::ChatOperation(
        std::shared_ptr<network::IHttpClient> httpClient,
        std::shared_ptr<protocol::IProtocolAdapter> adapter,
        const domain::model::ModelProvider &provider,
        const domain::llm::ChatRequest &request,
        const TimeoutPolicy &timeoutPolicy,
        const RetryPolicy &retryPolicy,
        QObject *parent)
        : application::ports::IChatOperation(parent)
        , m_httpClient(std::move(httpClient))
        , m_adapter(std::move(adapter))
        , m_provider(provider)
        , m_request(request)
        , m_timeoutPolicy(timeoutPolicy)
        , m_retryPolicy(retryPolicy) {
        
        m_metrics.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_metrics.providerId = provider.id;
        m_metrics.modelId = request.model;
        m_metrics.createdAt = QDateTime::currentMSecsSinceEpoch();

        m_firstTokenTimer = new QTimer(this);
        m_firstTokenTimer->setSingleShot(true);
        connect(m_firstTokenTimer, &QTimer::timeout, this, &ChatOperation::onFirstTokenTimeout);

        m_idleTimer = new QTimer(this);
        m_idleTimer->setSingleShot(true);
        connect(m_idleTimer, &QTimer::timeout, this, &ChatOperation::onIdleTimeout);

        m_overallTimer = new QTimer(this);
        m_overallTimer->setSingleShot(true);
        connect(m_overallTimer, &QTimer::timeout, this, &ChatOperation::onOverallTimeout);

        m_retryTimer = new QTimer(this);
        m_retryTimer->setSingleShot(true);
        connect(m_retryTimer, &QTimer::timeout, this, &ChatOperation::onRetryTimerFired);
    }

    ChatOperation::~ChatOperation() {
        stopAllTimers();
        if (m_currentHttpOp) {
            m_currentHttpOp->disconnect(this);
            m_currentHttpOp->cancel();
            m_currentHttpOp->deleteLater();
            m_currentHttpOp = nullptr;
        }
    }

    void ChatOperation::start() {
        if (m_state != RequestState::Created) {
            return;
        }

        if (m_timeoutPolicy.overallTimeoutMs > 0) {
            m_overallTimer->start(m_timeoutPolicy.overallTimeoutMs);
        }

        performRequest();
    }

    void ChatOperation::cancel() {
        cancelWithReason(CancelReason::UserCancelled);
    }

    void ChatOperation::cancelWithReason(CancelReason reason) {
        if (m_state == RequestState::Completed ||
            m_state == RequestState::Failed ||
            m_state == RequestState::Cancelled ||
            m_state == RequestState::Cancelling ||
            m_state == RequestState::TimedOut) {
            return;
        }

        setState(RequestState::Cancelling);

        domain::llm::ChatError err;
        err.category = domain::llm::ChatErrorCategory::Cancelled;
        err.code = cancelReasonToString(reason);
        err.message = QString("Request cancelled with reason: %1").arg(cancelReasonToString(reason));
        err.userMessage = QStringLiteral("已停止生成");
        err.providerId = m_provider.id;
        err.modelId = m_request.model;
        err.requestId = m_metrics.requestId;

        finalizeRequest(RequestState::Cancelled, err);
    }

    void ChatOperation::setState(RequestState newState) {
        m_state = newState;
    }

    void ChatOperation::stopAllTimers() {
        if (m_firstTokenTimer) m_firstTokenTimer->stop();
        if (m_idleTimer) m_idleTimer->stop();
        if (m_overallTimer) m_overallTimer->stop();
        if (m_retryTimer) m_retryTimer->stop();
    }

    void ChatOperation::performRequest() {
        if (!m_httpClient || !m_adapter) {
            domain::llm::ChatError err;
            err.category = domain::llm::ChatErrorCategory::Configuration;
            err.code = QStringLiteral("MissingDependencies");
            err.message = QStringLiteral("HttpClient or ProtocolAdapter is not available");
            err.userMessage = QStringLiteral("客户端配置异常，缺少请求依赖。");
            finalizeRequest(RequestState::Failed, err);
            return;
        }

        m_currentAttempt++;
        setState(RequestState::Connecting);

        if (m_currentHttpOp) {
            m_currentHttpOp->disconnect(this);
            m_currentHttpOp->deleteLater();
            m_currentHttpOp = nullptr;
        }

        auto httpReq = m_adapter->buildChatRequest(m_provider, m_request);
        httpReq.timeoutMs = m_timeoutPolicy.connectTimeoutMs;

        m_currentParser = m_adapter->createStreamParser();
        m_currentHttpOp = m_httpClient->send(httpReq);

        if (m_currentHttpOp) {
            m_currentHttpOp->setParent(this);
            connect(m_currentHttpOp, &network::HttpOperation::dataReceived, this, &ChatOperation::onDataReceived);
            connect(m_currentHttpOp, &network::HttpOperation::finished, this, &ChatOperation::onFinished);
            connect(m_currentHttpOp, &network::HttpOperation::failed, this, &ChatOperation::onFailed);
        }

        m_metrics.connectedAt = QDateTime::currentMSecsSinceEpoch();
        setState(RequestState::WaitingFirstToken);

        if (m_timeoutPolicy.firstTokenTimeoutMs > 0) {
            m_firstTokenTimer->start(m_timeoutPolicy.firstTokenTimeoutMs);
        }
    }

    void ChatOperation::onDataReceived(const QByteArray &data) {
        if (m_state == RequestState::Cancelling || m_state == RequestState::Cancelled) {
            return;
        }

        if (!m_currentParser) return;

        auto events = m_currentParser->feed(data);
        for (const auto &evt : events) {
            std::visit([this](const auto &arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, domain::llm::EventTextDelta> ||
                              std::is_same_v<T, domain::llm::EventThinkingDelta> ||
                              std::is_same_v<T, domain::llm::EventToolCallStarted> ||
                              std::is_same_v<T, domain::llm::EventToolCallDelta>) {
                    if (!m_hasEmittedVisibleTokens) {
                        m_hasEmittedVisibleTokens = true;
                        m_firstTokenTimer->stop();
                        m_metrics.firstTokenAt = QDateTime::currentMSecsSinceEpoch();
                        setState(RequestState::Streaming);
                    }

                    if (m_state == RequestState::Streaming && m_timeoutPolicy.idleTimeoutMs > 0) {
                        m_idleTimer->start(m_timeoutPolicy.idleTimeoutMs);
                    }
                }
            }, evt);

            emit eventReceived(evt);
        }
    }

    void ChatOperation::onFinished() {
        if (m_state == RequestState::Cancelling || m_state == RequestState::Cancelled) {
            return;
        }

        if (m_currentParser) {
            auto events = m_currentParser->finish();
            for (const auto &evt : events) {
                emit eventReceived(evt);
            }
        }

        finalizeRequest(RequestState::Completed);
    }

    void ChatOperation::onFailed(
        const QString &errorMessage,
        int httpStatusCode,
        const QByteArray &responseBody,
        int networkErrorCode) {
        
        if (m_state == RequestState::Cancelling || m_state == RequestState::Cancelled) {
            return;
        }

        stopAllTimers();

        domain::llm::ChatError err;
        if (httpStatusCode > 0 && m_adapter) {
            err = m_adapter->parseError(httpStatusCode, responseBody);
        } else if (networkErrorCode == 4 || errorMessage.contains("Timeout", Qt::CaseInsensitive)) {
            err.category = domain::llm::ChatErrorCategory::Timeout;
            err.code = QStringLiteral("ConnectTimeout");
            err.message = errorMessage;
            err.userMessage = QStringLiteral("网络连接超时，请检查网络或代理配置。");
            err.retryable = true;
            err.suggestedAction = QStringLiteral("Retry");
        } else if (networkErrorCode == 5 || errorMessage == QStringLiteral("Cancelled")) {
            err.category = domain::llm::ChatErrorCategory::Cancelled;
            err.code = QStringLiteral("UserCancelled");
            err.message = errorMessage;
            err.userMessage = QStringLiteral("已停止生成");
        } else {
            err.category = domain::llm::ChatErrorCategory::Network;
            err.code = QStringLiteral("NetworkError");
            err.message = errorMessage;
            err.userMessage = QStringLiteral("网络连接失败，请检查网络连接。");
            err.retryable = true;
            err.suggestedAction = QStringLiteral("Retry");
        }

        err.httpStatus = httpStatusCode;
        err.providerId = m_provider.id;
        err.modelId = m_request.model;
        err.requestId = m_metrics.requestId;

        auto decision = RetryDecision::evaluate(err, m_retryPolicy, m_currentAttempt, m_hasEmittedVisibleTokens);
        if (decision.action == RetryAction::RetryAfter) {
            setState(RequestState::WaitingRetry);
            m_metrics.retryCount++;

            if (m_currentHttpOp) {
                m_currentHttpOp->disconnect(this);
                m_currentHttpOp->deleteLater();
                m_currentHttpOp = nullptr;
            }
            m_currentParser.reset();

            m_retryTimer->start(decision.delayMs);
            return;
        }

        finalizeRequest(RequestState::Failed, err);
    }

    void ChatOperation::onFirstTokenTimeout() {
        if (m_state != RequestState::WaitingFirstToken) return;

        domain::llm::ChatError err;
        err.category = domain::llm::ChatErrorCategory::Timeout;
        err.code = QStringLiteral("FirstTokenTimeout");
        err.message = QStringLiteral("First token timeout after %1 ms").arg(m_timeoutPolicy.firstTokenTimeoutMs);
        err.userMessage = QStringLiteral("等待大模型响应首字超时，请稍后重试。");
        err.retryable = true;
        err.suggestedAction = QStringLiteral("Retry");
        err.providerId = m_provider.id;
        err.modelId = m_request.model;
        err.requestId = m_metrics.requestId;

        auto decision = RetryDecision::evaluate(err, m_retryPolicy, m_currentAttempt, m_hasEmittedVisibleTokens);
        if (decision.action == RetryAction::RetryAfter) {
            setState(RequestState::WaitingRetry);
            m_metrics.retryCount++;

            if (m_currentHttpOp) {
                m_currentHttpOp->disconnect(this);
                m_currentHttpOp->cancel();
                m_currentHttpOp->deleteLater();
                m_currentHttpOp = nullptr;
            }
            m_currentParser.reset();

            m_retryTimer->start(decision.delayMs);
            return;
        }

        finalizeRequest(RequestState::TimedOut, err);
    }

    void ChatOperation::onIdleTimeout() {
        if (m_state != RequestState::Streaming) return;

        domain::llm::ChatError err;
        err.category = domain::llm::ChatErrorCategory::Timeout;
        err.code = QStringLiteral("IdleTimeout");
        err.message = QStringLiteral("Stream idle timeout after %1 ms").arg(m_timeoutPolicy.idleTimeoutMs);
        err.userMessage = QStringLiteral("流式输出长时间无数据中断，请重试。");
        err.suggestedAction = QStringLiteral("Retry");
        err.providerId = m_provider.id;
        err.modelId = m_request.model;
        err.requestId = m_metrics.requestId;

        finalizeRequest(RequestState::TimedOut, err);
    }

    void ChatOperation::onOverallTimeout() {
        domain::llm::ChatError err;
        err.category = domain::llm::ChatErrorCategory::Timeout;
        err.code = QStringLiteral("OverallTimeout");
        err.message = QStringLiteral("Overall duration limit reached (%1 ms)").arg(m_timeoutPolicy.overallTimeoutMs);
        err.userMessage = QStringLiteral("会话生成耗时过长已超时。");
        err.providerId = m_provider.id;
        err.modelId = m_request.model;
        err.requestId = m_metrics.requestId;

        finalizeRequest(RequestState::TimedOut, err);
    }

    void ChatOperation::onRetryTimerFired() {
        if (m_state == RequestState::WaitingRetry) {
            performRequest();
        }
    }

    void ChatOperation::finalizeRequest(RequestState finalState, const std::optional<domain::llm::ChatError> &error) {
        stopAllTimers();

        if (m_currentHttpOp) {
            m_currentHttpOp->disconnect(this);
            if (m_state == RequestState::Cancelling || finalState == RequestState::Cancelled || finalState == RequestState::TimedOut) {
                m_currentHttpOp->cancel();
            }
            m_currentHttpOp->deleteLater();
            m_currentHttpOp = nullptr;
        }

        m_currentParser.reset();
        m_metrics.finishedAt = QDateTime::currentMSecsSinceEpoch();
        setState(finalState);

        if (error.has_value()) {
            emit eventReceived(domain::llm::EventError{*error});
        } else if (finalState == RequestState::Completed) {
            emit eventReceived(domain::llm::EventFinished{"stop"});
        }
    }

} // namespace llm::runtime
