#pragma once

#include <memory>
#include <optional>
#include <QObject>
#include <QTimer>
#include "application/ports/IChatModelGateway.h"
#include "network/IHttpClient.h"
#include "llm/protocol/IProtocolAdapter.h"
#include "domain/model/ResolvedModel.h"
#include "domain/llm/ChatRequest.h"
#include "domain/llm/ChatError.h"
#include "RequestState.h"
#include "TimeoutPolicy.h"
#include "RetryPolicy.h"
#include "RequestMetrics.h"

namespace llm::runtime {

    /**
     * @brief 全生命周期受控的 LLM 请求操作实现
     * @details 统筹状态流转、四级超时检测、指数退避重试决策、流式解析与资源统一释放
     */
    class ChatOperation : public application::ports::IChatOperation {
        Q_OBJECT

    public:
        ChatOperation(
            std::shared_ptr<network::IHttpClient> httpClient,
            std::shared_ptr<protocol::IProtocolAdapter> adapter,
            const domain::model::ResolvedModel &model,
            const domain::llm::ChatRequest &request,
            const TimeoutPolicy &timeoutPolicy = TimeoutPolicy{},
            const RetryPolicy &retryPolicy = RetryPolicy{},
            QObject *parent = nullptr
        );

        ~ChatOperation() override;

        /**
         * @brief 启动请求生命周期
         */
        void start();

        /**
         * @brief 客户端主动取消 (默认 UserCancelled)
         */
        void cancel() override;

        /**
         * @brief 带有详细取消原因的取消接口
         */
        void cancelWithReason(CancelReason reason);

        RequestState state() const { return m_state; }
        const RequestMetrics& metrics() const { return m_metrics; }

    private Q_SLOTS:
        void onDataReceived(const QByteArray &data);
        void onFinished();
        void onFailed(const QString &errorMessage, int httpStatusCode, const QByteArray &responseBody, int networkErrorCode);
        
        void onFirstTokenTimeout();
        void onIdleTimeout();
        void onOverallTimeout();
        void onRetryTimerFired();

    private:
        void performRequest();
        void finalizeRequest(RequestState finalState, const std::optional<domain::llm::ChatError> &error = std::nullopt);
        void stopAllTimers();
        void setState(RequestState newState);

        std::shared_ptr<network::IHttpClient> m_httpClient;
        std::shared_ptr<protocol::IProtocolAdapter> m_adapter;
        domain::model::ResolvedModel m_model;
        domain::llm::ChatRequest m_request;

        TimeoutPolicy m_timeoutPolicy;
        RetryPolicy m_retryPolicy;
        RequestMetrics m_metrics;

        RequestState m_state = RequestState::Created;
        int m_currentAttempt = 0;
        bool m_hasEmittedVisibleTokens = false;
        bool m_finishedEventEmitted = false;

        network::HttpOperation *m_currentHttpOp = nullptr;
        std::unique_ptr<protocol::IStreamParser> m_currentParser;

        QTimer *m_firstTokenTimer = nullptr;
        QTimer *m_idleTimer = nullptr;
        QTimer *m_overallTimer = nullptr;
        QTimer *m_retryTimer = nullptr;
    };

} // namespace llm::runtime
