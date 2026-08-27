#pragma once
#include <QString>
#include <QMap>
#include <optional>
#include "ProviderModel.h"

namespace domain::model {

    /**
     * @brief 协议驱动类型
     */
    enum class ProtocolType {
        Unknown = 0,                 ///< 未知/不支持的协议类型
        OpenAIChatCompletions = 1,   ///< OpenAI 兼容 Chat Completions 协议 (/v1/chat/completions)
        OpenAIResponses = 2,         ///< OpenAI 原生 Responses 协议 (/v1/responses)
        AnthropicMessages = 3,       ///< Anthropic 原生 Messages 协议 (/v1/messages)
        OllamaChat = 4,              ///< 本地 Ollama 协议 (/api/chat)
        GeminiGenerateContent = 5,   ///< Google Gemini 原生协议 (:generateContent / :streamGenerateContent)
        AzureOpenAI = 6,             ///< Azure OpenAI 专用协议
        AmazonBedrock = 7            ///< Amazon Bedrock 原生协议
    };

    /**
     * @brief 模型服务商实体
     */
    struct ModelProvider {
        QString id;                           ///< 服务商唯一标识
        QString name;                         ///< 显示名称
        QString icon;                         ///< 服务商图标路径或内置图标标识
        QString docUrl;                       ///< 官方文档或 API 说明地址
        QString envVarName;                   ///< 环境变量名
        QString sdkPackage;                   ///< models.dev npm package 原值 (adapter-family hint)
        ProtocolType protocol = ProtocolType::Unknown; ///< 协议驱动类型

        QString baseUrl;                      ///< API 基础请求地址
        QString apiKey;                       ///< API 密钥

        QMap<QString, QString> customHeaders; ///< 自定义 HTTP Header
        std::optional<QString> proxyUrl;      ///< 代理配置
        int timeoutMs = 60000;                ///< 超时时间（毫秒）

        bool isEnabled = false;               ///< 是否全局启用
        bool isCustom = false;                ///< 是否为用户自定义添加
        DataOrigin origin = DataOrigin::BuiltIn; ///< 数据来源

        bool operator==(const ModelProvider &other) const = default;
    };

} // namespace domain::model
