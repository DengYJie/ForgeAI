#pragma once
#include <QString>
#include <QDateTime>

namespace domain::llm {

    /**
     * @brief LLM 错误大类分层
     */
    enum class ChatErrorCategory {
        Network,          ///< 网络连接、DNS、TLS、连接重置等底层传输错误
        Authentication,   ///< API Key 缺失、无效或已过期 (401)
        Authorization,    ///< 权限不足、无权访问指定资源 (403)
        RateLimit,        ///< 请求频率超限 (429)
        Quota,            ///< 账户配额不足、账单欠费 (429/402/403)
        Request,          ///< 客户端请求格式不合规、参数错误 (400/422)
        Model,            ///< 模型不存在、未部署或已被下线 (404/503)
        Context,          ///< 上下文超长、输入 Tokens 超过模型限制 (400)
        Provider,         ///< 服务商服务端内部故障或网关错误 (500/502/503/504)
        Protocol,         ///< SSE 流格式损坏、未预期的 JSON 或响应格式
        Timeout,          ///< 连接超时、首字超时或流空闲超时
        Cancelled,        ///< 客户端主动取消或请求被覆盖
        Configuration,    ///< 本地 Provider 或 Model 配置错误
        Internal,         ///< 客户端内部逻辑异常
        Unknown           ///< 未归类异常
    };

    /**
     * @brief 包含详细分类、排查与治理信息的错误实体
     */
    struct ChatError {
        ChatErrorCategory category = ChatErrorCategory::Unknown;
        
        QString code;                 ///< 标准化细分错误码 (如 "TooManyRequests", "ContextLengthExceeded")
        QString message;              ///< 开发者排查日志及系统错误描述
        QString userMessage;          ///< UI 用户友好的可读提示文本

        // 来源与追踪上下文
        QString providerId;
        QString modelId;
        QString requestId;
        qint64 timestamp = 0;

        // HTTP 与服务商原生信息
        int httpStatus = 0;
        QString providerErrorCode;    ///< 服务商返回的原生错误字段 (如 "insufficient_quota")
        QString providerMessage;      ///< 服务商原始 message 字段
        QString originalText;         ///< 原始错误响应 Payload (用于日志审计)

        // 请求治理与决策
        bool retryable = false;       ///< 是否属于安全可自动重试的错误
        int retryAfterSeconds = 0;    ///< 服务商建议的重试等待时间 (秒)
        QString suggestedAction;      ///< 建议的 UI 恢复动作 (如 "Retry", "CompressContext", "OpenSettings", "ChangeModel")

        ChatError() : timestamp(QDateTime::currentMSecsSinceEpoch()) {}

        bool operator==(const ChatError &other) const = default;
    };

} // namespace domain::llm
