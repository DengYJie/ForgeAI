#pragma once
#include <QString>
#include <QList>
#include <QMap>
#include <optional>
#include "Model.h"

namespace domain::model {

    /**
     * @brief 服务商协议驱动类型
     */
    enum class ProviderType {
        OpenAICompatible,   ///< OpenAI 兼容协议
        Anthropic,          ///< Anthropic 原生 Messages 协议
        Ollama,             ///< 本地 Ollama 协议 (支持模型检测与拉取)
        GoogleGemini        ///< Google Gemini 原生协议
    };

    /**
     * @brief 模型服务商实体
     */
    struct ModelProvider {
        QString id;                           ///< 服务商唯一标识
        QString name;                         ///< 显示名称
        QString icon;                         ///< 服务商图标路径或内置图标标识
        QString docUrl;                       ///< 官方文档或 API 说明地址
        QString envVarName;                   ///< 对应的环境变量名
        ProviderType type = ProviderType::OpenAICompatible; ///< 协议驱动类型

        QString baseUrl;                      ///< API 基础请求地址
        QString apiKey;                       ///< API 密钥

        QMap<QString, QString> customHeaders; ///< 自定义 HTTP Header (如自定义鉴权或网关头)
        std::optional<QString> proxyUrl;      ///< 独立网络代理配置 (留空使用系统代理)
        int timeoutMs = 60000;                ///< 请求超时时间（毫秒，默认 60s）

        QList<Model> models;                  ///< 该服务商下注册的所有模型列表
        bool isEnabled = true;                ///< 是否全局启用该服务商

        bool operator==(const ModelProvider &other) const = default;
    };

} // namespace domain::model
