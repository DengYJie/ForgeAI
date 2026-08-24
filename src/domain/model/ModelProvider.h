#pragma once
#include <QString>
#include <QMap>
#include <optional>
#include "ProviderModel.h"

namespace domain::model {

    /**
     * @brief 服务商协议驱动类型
     */
    enum class ProviderType {
        OpenAIChatCompletionsCompatible, ///< OpenAI 兼容协议 (基于 /v1/chat/completions)
        OpenAIResponses,                 ///< OpenAI 原生 Responses API
        Anthropic,                       ///< Anthropic 原生 Messages 协议
        Ollama,                          ///< 本地 Ollama 协议
        GoogleGemini,                    ///< Google Gemini 原生协议
        AzureOpenAI,                     ///< Azure OpenAI 专用协议
        AmazonBedrock                    ///< Amazon Bedrock 原生协议
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
        ProviderType type = ProviderType::OpenAIChatCompletionsCompatible; ///< 协议驱动类型

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
