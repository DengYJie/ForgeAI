#pragma once
#include <QString>

namespace llm::runtime {

    /**
     * @brief LLM 请求生命周期内部状态
     */
    enum class RequestState {
        Created,            ///< 请求已构建初始化
        Connecting,         ///< 正在建立 HTTP/TLS 连接
        WaitingFirstToken,  ///< 连接建立成功，正在等待模型响应首个 Token/块
        Streaming,          ///< 已接收到 Token/块，正在流式持续接收中
        WaitingRetry,       ///< 遭遇瞬态错误，正在等待退避重试
        Cancelling,         ///< 正在执行优雅取消与释放流程
        Cancelled,          ///< 请求已取消
        Completed,          ///< 请求已圆满成功结束
        Failed,             ///< 发生不可恢复错误或超出最大重试次数
        TimedOut            ///< 连接、首字或流空闲超时
    };

    /**
     * @brief 取消原因细分
     */
    enum class CancelReason {
        UserCancelled,          ///< 用户主动点击停止
        ProviderChanged,        ///< 用户切换了服务商
        ModelChanged,           ///< 用户切换了模型
        ConversationClosed,     ///< 会话被关闭或切换
        ApplicationShutdown,    ///< 应用程序退出
        Superseded,             ///< 被同一会话的新提问覆盖
        Timeout                 ///< 内部超时触发取消
    };

    inline QString requestStateToString(RequestState state) {
        switch (state) {
            case RequestState::Created: return QStringLiteral("Created");
            case RequestState::Connecting: return QStringLiteral("Connecting");
            case RequestState::WaitingFirstToken: return QStringLiteral("WaitingFirstToken");
            case RequestState::Streaming: return QStringLiteral("Streaming");
            case RequestState::WaitingRetry: return QStringLiteral("WaitingRetry");
            case RequestState::Cancelling: return QStringLiteral("Cancelling");
            case RequestState::Cancelled: return QStringLiteral("Cancelled");
            case RequestState::Completed: return QStringLiteral("Completed");
            case RequestState::Failed: return QStringLiteral("Failed");
            case RequestState::TimedOut: return QStringLiteral("TimedOut");
        }
        return QStringLiteral("Unknown");
    }

    inline QString cancelReasonToString(CancelReason reason) {
        switch (reason) {
            case CancelReason::UserCancelled: return QStringLiteral("UserCancelled");
            case CancelReason::ProviderChanged: return QStringLiteral("ProviderChanged");
            case CancelReason::ModelChanged: return QStringLiteral("ModelChanged");
            case CancelReason::ConversationClosed: return QStringLiteral("ConversationClosed");
            case CancelReason::ApplicationShutdown: return QStringLiteral("ApplicationShutdown");
            case CancelReason::Superseded: return QStringLiteral("Superseded");
            case CancelReason::Timeout: return QStringLiteral("Timeout");
        }
        return QStringLiteral("Unknown");
    }

} // namespace llm::runtime
