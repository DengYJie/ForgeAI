#pragma once
#include <QString>
#include <QMap>
#include <QByteArray>

namespace network {

    /**
     * @brief HTTP 请求方法枚举
     */
    enum class HttpMethod {
        Get,
        Post,
        Delete,
        Put
    };

    /**
     * @brief 纯粹的 HTTP 请求结构体，不包含任何业务依赖
     */
    struct HttpRequest {
        QString url;
        HttpMethod method = HttpMethod::Post;
        QMap<QString, QString> headers;
        QByteArray body;
        int timeoutMs = 60000; ///< 默认 60 秒

        bool operator==(const HttpRequest &other) const = default;
    };

} // namespace network
