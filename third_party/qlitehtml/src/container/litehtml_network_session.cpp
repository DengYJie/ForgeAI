#include "litehtml_network_session.h"
#include "litehtml_resource_task.h"

#include <QDir>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTimer>

namespace qlitehtml::internal {

NetworkWorker::NetworkWorker(const QString &cacheDir, qint64 maxCacheSize, QObject *parent)
    : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
    m_diskCache = new QNetworkDiskCache(this);
    m_diskCache->setCacheDirectory(cacheDir);
    m_diskCache->setMaximumCacheSize(maxCacheSize);
    m_nam->setCache(m_diskCache);
}

NetworkWorker::~NetworkWorker() = default;

void NetworkWorker::fetch(const QUrl &url,
                          const std::shared_ptr<ResourceTaskContext> &ctx,
                          const std::function<void(const QByteArray &)> &onChunk,
                          const std::function<void(const QByteArray &)> &onComplete)
{
    if (!ctx || ctx->cancelToken->load()) {
        if (onComplete)
            onComplete({});
        return;
    }

    QNetworkRequest netReq(url);
    netReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    netReq.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    netReq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"));

    QNetworkReply *reply = m_nam->get(netReq);
    ctx->reply = reply;

    constexpr int kNetworkTimeoutMs = 10000;
    constexpr qint64 kMaxPayloadSizeBytes = 16 * 1024 * 1024;

    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);

    auto *receivedData = new QByteArray();

    QObject::connect(timer, &QTimer::timeout, reply, [reply]() {
        if (reply && reply->isRunning()) {
            reply->abort();
        }
    });

    QObject::connect(reply, &QNetworkReply::readyRead, reply, [reply, receivedData, onChunk, ctx]() {
        if (!ctx || ctx->cancelToken->load()) {
            reply->abort();
            receivedData->clear();
            return;
        }
        const auto clHeader = reply->header(QNetworkRequest::ContentLengthHeader);
        if (clHeader.isValid() && clHeader.toLongLong() > kMaxPayloadSizeBytes) {
            reply->abort();
            receivedData->clear();
            return;
        }
        receivedData->append(reply->readAll());
        if (receivedData->size() > kMaxPayloadSizeBytes) {
            reply->abort();
            receivedData->clear();
            return;
        }
        if (onChunk) {
            onChunk(*receivedData);
        }
    });

    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, timer, receivedData, ctx, onComplete]() {
        timer->stop();
        QByteArray result;
        if (ctx && !ctx->cancelToken->load() && reply->error() == QNetworkReply::NoError) {
            result = std::move(*receivedData);
        }
        delete receivedData;
        reply->deleteLater();
        if (ctx) {
            ctx->reply = nullptr;
        }
        if (onComplete) {
            onComplete(result);
        }
    });

    timer->start(kNetworkTimeoutMs);
}

void NetworkWorker::setCacheDirectory(const QString &path)
{
    if (m_diskCache) {
        m_diskCache->setCacheDirectory(path);
    }
}

void NetworkWorker::setMaxCacheSize(qint64 bytes)
{
    if (m_diskCache) {
        m_diskCache->setMaximumCacheSize(bytes);
    }
}

LiteHtmlNetworkSession &LiteHtmlNetworkSession::instance()
{
    static LiteHtmlNetworkSession session;
    return session;
}

LiteHtmlNetworkSession::LiteHtmlNetworkSession()
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheDir.isEmpty()) {
        cacheDir = QDir::tempPath() + QStringLiteral("/qlitehtml_cache");
    } else {
        cacheDir += QStringLiteral("/qlitehtml_cache");
    }

    m_worker = new NetworkWorker(cacheDir, 256 * 1024 * 1024);
    m_worker->moveToThread(&m_networkThread);
    connect(&m_networkThread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_networkThread.start();
}

LiteHtmlNetworkSession::~LiteHtmlNetworkSession()
{
    m_networkThread.quit();
    m_networkThread.wait();
    m_worker = nullptr;
}

void LiteHtmlNetworkSession::fetch(const ResourceRequest &req,
                                   const std::shared_ptr<ResourceTaskContext> &ctx,
                                   const std::function<void(const QByteArray &)> &onChunk,
                                   const std::function<void(const QByteArray &)> &onComplete)
{
    const QUrl url = req.url;
    QMetaObject::invokeMethod(
        m_worker,
        [this, url, ctx, onChunk, onComplete]() {
            m_worker->fetch(url, ctx, onChunk, onComplete);
        },
        Qt::QueuedConnection);
}

void LiteHtmlNetworkSession::setCacheDirectory(const QString &path)
{
    QMetaObject::invokeMethod(
        m_worker,
        [this, path]() {
            m_worker->setCacheDirectory(path);
        },
        Qt::QueuedConnection);
}

void LiteHtmlNetworkSession::setMaxCacheSize(qint64 bytes)
{
    QMetaObject::invokeMethod(
        m_worker,
        [this, bytes]() {
            m_worker->setMaxCacheSize(bytes);
        },
        Qt::QueuedConnection);
}

} // namespace qlitehtml::internal
