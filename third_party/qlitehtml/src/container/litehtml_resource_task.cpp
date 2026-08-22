#include "litehtml_resource_task.h"
#include "litehtml_network_session.h"

#include <QFile>
#include <future>

namespace {

constexpr qint64 kMaxPayloadSizeBytes = 16 * 1024 * 1024;     // 16 MiB max payload

static QByteArray parseDataUrl(const QUrl &url)
{
    const QString urlStr = url.toString();
    const int commaIndex = urlStr.indexOf(',');
    if (commaIndex == -1)
        return {};
    
    const QString meta = urlStr.left(commaIndex);
    const QString dataStr = urlStr.mid(commaIndex + 1);

    // Rough check against oversized base64 strings
    if (dataStr.size() > kMaxPayloadSizeBytes * 4 / 3)
        return {};
    
    if (meta.contains(QLatin1String(";base64"), Qt::CaseInsensitive)) {
        return QByteArray::fromBase64(dataStr.toLatin1(), QByteArray::OmitTrailingEquals | QByteArray::AbortOnBase64DecodingErrors);
    } else {
        return QByteArray::fromPercentEncoding(dataStr.toLatin1());
    }
}

static QString normalizeQrcPath(const QUrl &url)
{
    if (url.scheme() == "qrc") {
        QString path = url.path();
        if (!path.startsWith('/'))
            path.prepend('/');
        return ":" + path;
    }
    const QString str = url.toString();
    if (str.startsWith("qrc:/")) {
        return ":" + str.mid(4);
    }
    if (str.startsWith("qrc:")) {
        return ":/" + str.mid(4);
    }
    if (str.startsWith(":/")) {
        return str;
    }
    return {};
}

static QString normalizeLocalPath(const QUrl &url)
{
    if (url.isLocalFile())
        return url.toLocalFile();
    if (url.scheme() == "file")
        return url.toLocalFile();
    return url.toString();
}

} // namespace

namespace qlitehtml::internal {

QByteArray GenericResourceFetcher::fetchRawData(
    const ResourceRequest &req,
    const ResourceHandler &handler,
    const std::shared_ptr<ResourceTaskContext> &ctx,
    const ChunkCallback &onChunk)
{
    if (!ctx || ctx->cancelToken->load())
        return {};

    const QUrl &url = req.url;

    // 1. Custom ResourceHandler override
    if (handler) {
        QByteArray data = handler(url, req.type);
        if (!data.isEmpty()) {
            if (onChunk)
                onChunk(data);
            return data;
        }
    }

    if (ctx->cancelToken->load())
        return {};

    // 2. data: URL scheme
    if (url.scheme() == "data" || url.toString().startsWith("data:")) {
        QByteArray data = parseDataUrl(url);
        if (!data.isEmpty() && onChunk)
            onChunk(data);
        return data;
    }

    // 3. qrc: / :/ embedded Qt resources
    if (url.scheme() == "qrc" || url.toString().startsWith("qrc:") || url.toString().startsWith(":/")) {
        const QString qrcPath = normalizeQrcPath(url);
        QFile file(qrcPath);
        if (file.size() <= kMaxPayloadSizeBytes && file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            if (onChunk)
                onChunk(data);
            return data;
        }
        return {};
    }

    // 4. file: local filesystem paths
    if (url.isLocalFile() || url.scheme() == "file") {
        const QString localPath = normalizeLocalPath(url);
        QFile file(localPath);
        if (file.size() <= kMaxPayloadSizeBytes && file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            if (onChunk)
                onChunk(data);
            return data;
        }
        return {};
    }

    // 5. http: / https: network requests (delegated to dedicated network thread)
    if (req.allowNetwork && (url.scheme() == "http" || url.scheme() == "https")) {
        auto prom = std::make_shared<std::promise<QByteArray>>();
        auto fut = prom->get_future();

        LiteHtmlNetworkSession::instance().fetch(
            req,
            ctx,
            onChunk,
            [prom](const QByteArray &data) {
                prom->set_value(data);
            });

        return fut.get();
    }

    return {};
}

} // namespace qlitehtml::internal
