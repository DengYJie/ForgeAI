#pragma once
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QSet>
#include <QMap>

namespace core::logging {

    /**
     * @brief 敏感数据过滤与安全脱敏器
     */
    class SensitiveDataFilter {
    public:
        /**
         * @brief 对 URL 进行安全清洗，掩码 query 中的 key / token 等敏感参数
         */
        static QString sanitizeUrl(const QString &rawUrl) {
            QUrl url(rawUrl);
            if (!url.isValid() || !url.hasQuery()) {
                return rawUrl;
            }

            QUrlQuery query(url);
            QUrlQuery cleanQuery;
            const auto items = query.queryItems();
            for (const auto &item : items) {
                QString keyLower = item.first.toLower();
                if (keyLower == "key" || keyLower == "api_key" || keyLower == "apikey" ||
                    keyLower == "token" || keyLower == "access_token" || keyLower == "secret") {
                    cleanQuery.addQueryItem(item.first, QStringLiteral("****"));
                } else {
                    cleanQuery.addQueryItem(item.first, item.second);
                }
            }
            url.setQuery(cleanQuery);
            return url.toString();
        }

        /**
         * @brief 对文本字符串执行正则脱敏 (Bearer token, sk-*** 等)
         */
        static QString redactText(const QString &text) {
            if (text.isEmpty()) return text;

            QString result = text;
            
            static const QRegularExpression bearerRegex(QStringLiteral(R"((Bearer\s+)[A-Za-z0-9\-_]{8,})"), QRegularExpression::CaseInsensitiveOption);
            result.replace(bearerRegex, QStringLiteral(R"(\1****)"));

            static const QRegularExpression skRegex(QStringLiteral(R"(sk-[A-Za-z0-9\-_]{16,})"));
            result.replace(skRegex, QStringLiteral("sk-****"));

            static const QRegularExpression apiKeyRegex(QStringLiteral(R"(((?:x-)?api-key["'\s:]+)[A-Za-z0-9\-_]{8,})"), QRegularExpression::CaseInsensitiveOption);
            result.replace(apiKeyRegex, QStringLiteral(R"(\1****)"));

            return result;
        }

        /**
         * @brief Header 白名单过滤，仅保留安全的公开 Header，其它一律忽略或脱敏
         */
        static QMap<QString, QString> filterHeaders(const QMap<QString, QString> &headers) {
            static const QSet<QString> whitelist = {
                "content-type",
                "user-agent",
                "accept",
                "request-id",
                "x-request-id",
                "retry-after",
                "anthropic-version"
            };

            QMap<QString, QString> safeHeaders;
            for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
                QString keyLower = it.key().toLower();
                if (whitelist.contains(keyLower)) {
                    safeHeaders.insert(it.key(), it.value());
                } else if (keyLower == "authorization" || keyLower == "x-api-key" || keyLower == "api-key") {
                    safeHeaders.insert(it.key(), QStringLiteral("****"));
                }
            }
            return safeHeaders;
        }
    };

} // namespace core::logging
