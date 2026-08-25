#pragma once

#include <QString>
#include <QStringList>
#include <QMap>

namespace domain::mcp {

    /**
     * @brief MCP 服务信任级别
     */
    enum class McpTrustLevel {
        Untrusted,      ///< 未受信（需要用户确认首次运行）
        ApprovedOnce,   ///< 单次批准运行
        AlwaysAllow,    ///< 总是允许自动运行
        Denied          ///< 显式禁止运行
    };

    /**
     * @brief MCP 安全信任策略与敏感数据脱敏工具
     */
    class McpServerTrustPolicy {
    public:
        McpServerTrustPolicy() = default;

        /**
         * @brief 判断指定服务是否被信任允许直接执行
         */
        bool isServerTrusted(const QString& serverId, bool autoApproveFlag = false) const {
            if (m_trustOverrides.contains(serverId)) {
                auto level = m_trustOverrides.value(serverId);
                return level == McpTrustLevel::AlwaysAllow || level == McpTrustLevel::ApprovedOnce;
            }
            if (autoApproveFlag) {
                return true;
            }
            return m_defaultTrustLevel == McpTrustLevel::AlwaysAllow;
        }

        /**
         * @brief 设置服务信任级别
         */
        void setServerTrust(const QString& serverId, McpTrustLevel level) {
            m_trustOverrides.insert(serverId, level);
        }

        /**
         * @brief 清除特定服务的信任设定
         */
        void clearServerTrust(const QString& serverId) {
            m_trustOverrides.remove(serverId);
        }

        /**
         * @brief 检查环境变量键名是否属于敏感安全字段（如 Key, Token, Password 等）
         */
        static bool isSensitiveEnvKey(const QString& key) {
            const QString upperKey = key.toUpper();
            static const QStringList sensitiveKeywords = {
                QStringLiteral("KEY"),
                QStringLiteral("TOKEN"),
                QStringLiteral("SECRET"),
                QStringLiteral("PASSWORD"),
                QStringLiteral("PASS"),
                QStringLiteral("AUTH"),
                QStringLiteral("CREDENTIAL"),
                QStringLiteral("PRIVATE"),
                QStringLiteral("APIKEY")
            };

            for (const auto& kw : sensitiveKeywords) {
                if (upperKey.contains(kw)) {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief 对环境变量 Map 进行脱敏处理（用于日志记录与 UI 展示）
         */
        static QMap<QString, QString> maskSensitiveEnv(const QMap<QString, QString>& rawEnv) {
            QMap<QString, QString> masked;
            for (auto it = rawEnv.cbegin(); it != rawEnv.cend(); ++it) {
                if (isSensitiveEnvKey(it.key())) {
                    const QString val = it.value();
                    if (val.length() <= 8) {
                        masked.insert(it.key(), QStringLiteral("******"));
                    } else {
                        masked.insert(it.key(), val.left(3) + QStringLiteral("******") + val.right(3));
                    }
                } else {
                    masked.insert(it.key(), it.value());
                }
            }
            return masked;
        }

    private:
        McpTrustLevel m_defaultTrustLevel = McpTrustLevel::Untrusted;
        QMap<QString, McpTrustLevel> m_trustOverrides;
    };

} // namespace domain::mcp
