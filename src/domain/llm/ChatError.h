#pragma once
#include <QString>

namespace domain::llm {

    /**
     * @brief LLM 协议层统一错误类型
     */
    enum class ChatErrorType {
        NetworkError,          ///< 网络连接、DNS、TLS 错误等
        Timeout,               ///< 请求超时
        Unauthorized,          ///< 401/403 认证失败
        RateLimited,           ///< 429 请求频率超限
        InvalidRequest,        ///< 400 格式错误
        ModelNotFound,         ///< 404 模型不存在
        ContextLengthExceeded, ///< 上下文超长
        ServerError,           ///< 500/502/503/504 服务器错误
        ProtocolError,         ///< 返回了非预期的数据格式或无法解析的 JSON
        Cancelled,             ///< 客户端主动取消
        Unknown                ///< 其他未归类错误
    };

    /**
     * @brief 包含详细信息的错误实体
     */
    struct ChatError {
        ChatErrorType type = ChatErrorType::Unknown;
        QString message;      ///< UI 展示的友好的错误提示
        QString originalText; ///< 服务商返回的原始 JSON 或出错堆栈（仅供日志排查）

        bool operator==(const ChatError &other) const = default;
    };

} // namespace domain::llm
