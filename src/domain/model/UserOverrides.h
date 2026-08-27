#pragma once
#include <QString>
#include <QMap>
#include <optional>
#include "ModelProvider.h"

namespace domain::model {

    /**
     * @brief 用户对官方 Provider 的局部覆盖配置
     * @details 只存储用户有意修改的字段，未修改的字段为 std::nullopt。
     *          Read-time Merge 时以官方预置基线为缺省值。
     */
    struct UserProviderOverride {
        QString providerId;                          ///< 对应的官方 Provider ID (PK)
        std::optional<bool>    isEnabled;            ///< 是否全局启用（覆盖官方默认）
        std::optional<QString> baseUrl;              ///< 自定义 Base URL
        std::optional<QString> apiKey;               ///< API 密钥
        QMap<QString, QString> customHeaders;        ///< 自定义 HTTP Header

        bool operator==(const UserProviderOverride &other) const = default;
    };

    /**
     * @brief 用户对官方 ProviderModel 的局部覆盖配置
     */
    struct UserModelOverride {
        QString providerId;                          ///< 归属服务商 ID (PK part)
        QString remoteModelId;                       ///< 模型 ID (PK part)
        std::optional<bool>    isEnabled;            ///< 是否启用
        std::optional<QString> customAlias;          ///< 自定义显示名称

        bool operator==(const UserModelOverride &other) const = default;
    };

    /**
     * @brief 用户完全手动添加的自定义服务商（无官方基线）
     */
    struct UserCustomProvider {
        QString id;
        QString name;
        QString icon;
        ProtocolType protocol = ProtocolType::OpenAIChatCompletions;
        QString baseUrl;
        QString apiKey;
        QMap<QString, QString> customHeaders;
        int timeoutMs = 60000;
        bool isEnabled = false;

        bool operator==(const UserCustomProvider &other) const = default;
    };

    /**
     * @brief 用户手动添加的自定义模型（脱离 models.dev 的纯自定义）
     */
    struct UserCustomModel {
        QString providerId;
        QString remoteModelId;
        QString displayName;
        bool isEnabled = true;

        bool operator==(const UserCustomModel &other) const = default;
    };

} // namespace domain::model
