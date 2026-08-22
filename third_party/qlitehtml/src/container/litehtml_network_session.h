#pragma once

#include "qlitehtml_global.h"
#include "qlitehtml_types.h"

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QUrl>

#include <functional>
#include <memory>

class QNetworkAccessManager;
class QNetworkDiskCache;
class QNetworkReply;

namespace qlitehtml::internal {

struct ResourceTaskContext;
struct ResourceRequest;

/**
 * @brief Dedicated network worker running on its own dedicated QThread.
 * 
 * Owns QNetworkAccessManager and QNetworkDiskCache to guarantee all QObject operations,
 * socket notifications, and timeouts execute within a single thread's event loop.
 */
class NetworkWorker : public QObject
{
    Q_OBJECT
public:
    explicit NetworkWorker(const QString &cacheDir, qint64 maxCacheSize, QObject *parent = nullptr);
    ~NetworkWorker() override;

public:
    void fetch(const QUrl &url,
               const std::shared_ptr<ResourceTaskContext> &ctx,
               const std::function<void(const QByteArray &)> &onChunk,
               const std::function<void(const QByteArray &)> &onComplete);

    void setCacheDirectory(const QString &path);
    void setMaxCacheSize(qint64 bytes);

private:
    QNetworkAccessManager *m_nam = nullptr;
    QNetworkDiskCache *m_diskCache = nullptr;
};

/**
 * @brief Singleton session manager for global HTTP/2 connection reuse and disk caching.
 * 
 * Routes all HTTP/HTTPS network requests onto a dedicated background network thread,
 * completely isolating network I/O from UI and QThreadPool worker threads.
 */
class LiteHtmlNetworkSession : public QObject
{
    Q_OBJECT
public:
    static LiteHtmlNetworkSession &instance();

    /**
     * @brief Asynchronously dispatches a network request onto the dedicated network thread.
     * @param req The request parameters.
     * @param ctx Task cancellation context and reply tracking.
     * @param onChunk Streaming chunk callback (called from network thread).
     * @param onComplete Completion callback with full payload (called from network thread).
     */
    void fetch(const ResourceRequest &req,
               const std::shared_ptr<ResourceTaskContext> &ctx,
               const std::function<void(const QByteArray &)> &onChunk,
               const std::function<void(const QByteArray &)> &onComplete);

    void setCacheDirectory(const QString &path);
    void setMaxCacheSize(qint64 bytes);

private:
    LiteHtmlNetworkSession();
    ~LiteHtmlNetworkSession() override;

    QThread m_networkThread;
    NetworkWorker *m_worker = nullptr;
};

} // namespace qlitehtml::internal
