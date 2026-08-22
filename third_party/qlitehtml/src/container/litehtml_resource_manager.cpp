#include "litehtml_resource_manager.h"
#include "container_internal.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFontDatabase>
#include <QImage>
#include <QImageReader>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRunnable>
#include <QThreadPool>
#include <QTimer>

namespace {
static Q_LOGGING_CATEGORY(log, "qlitehtml.resource", QtCriticalMsg)

constexpr int kNetworkTimeoutMs = 10000;                      // 10s timeout
constexpr qint64 kMaxImageSizeBytes = 16 * 1024 * 1024;       // 16 MiB max payload
constexpr int kMaxImageDimension = 16384;                     // 16384 px max width or height
constexpr qint64 kMaxImagePixelCount = 32 * 1024 * 1024;      // 32 megapixels max

static QByteArray parseDataUrl(const QUrl &url)
{
    const QString urlStr = url.toString();
    const int commaIndex = urlStr.indexOf(',');
    if (commaIndex == -1)
        return {};
    
    const QString meta = urlStr.left(commaIndex);
    const QString dataStr = urlStr.mid(commaIndex + 1);

    // Rough check against oversized base64 strings
    if (dataStr.size() > kMaxImageSizeBytes * 4 / 3)
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

static QString resolveFontFamily(const QStringList &candidates)
{
    static const QStringList systemFamilies = QFontDatabase().families();
    for (const QString &family : candidates) {
        for (const QString &sysFamily : systemFamilies) {
            if (sysFamily.compare(family, Qt::CaseInsensitive) == 0) {
                return sysFamily;
            }
        }
    }
    return candidates.isEmpty() ? QStringLiteral("sans-serif") : candidates.first();
}

} // namespace

namespace qlitehtml::internal {

LiteHtmlResourceManager::LiteHtmlResourceManager() = default;

void LiteHtmlResourceManager::setBaseUrl(const QString &baseUrl)
{
    m_baseUrl = baseUrl;
}

QString LiteHtmlResourceManager::baseUrl() const
{
    return m_baseUrl;
}

void LiteHtmlResourceManager::setResourceHandler(const DocumentContainer::ResourceHandler &handler)
{
    m_resourceHandler = handler;
}

DocumentContainer::ResourceHandler LiteHtmlResourceManager::resourceHandler() const
{
    return m_resourceHandler;
}

void LiteHtmlResourceManager::setRepaintCallback(const DocumentContainer::RepaintCallback &callback)
{
    m_repaintCallback = callback;
}

void LiteHtmlResourceManager::setRelayoutCallback(const std::function<void()> &callback)
{
    m_relayoutCallback = callback;
}

void LiteHtmlResourceManager::setAllowNetworkAccess(bool allow)
{
    m_allowNetworkAccess = allow;
}

bool LiteHtmlResourceManager::allowNetworkAccess() const
{
    return m_allowNetworkAccess;
}

QUrl LiteHtmlResourceManager::resolveUrl(const QString &url, const QString &baseUrl) const
{
    return qlitehtml::internal::resolveUrl(url, baseUrl.isEmpty() ? m_baseUrl : baseUrl);
}

void LiteHtmlResourceManager::load_image(const char *src,
                                         const char *baseurl,
                                         bool redraw_on_ready)
{
    const auto qtSrc = QString::fromUtf8(src);
    const auto qtBaseUrl = QString::fromUtf8(baseurl);
    const QUrl url = resolveUrl(qtSrc, qtBaseUrl);
    if (m_pixmaps.contains(url) || m_loadingImages.contains(url))
        return;

    // Fetch and decode on a worker thread.
    // The pixmap cache and layout are only touched on the main thread: convert
    // decoded QImage to QPixmap, re-layout and repaint there.
    // The weak reference guards against the manager/container being destroyed.
    m_loadingImages.insert(url);
    const uint64_t generation = m_cacheGeneration;
    const auto handler = m_resourceHandler; // snapshot handler on GUI thread to prevent cross-thread race condition
    const bool allowNetwork = m_allowNetworkAccess;
    const std::weak_ptr<LiteHtmlResourceManager> weak = shared_from_this();
    auto *task = QRunnable::create([weak, url, redraw_on_ready, generation, handler, allowNetwork] {
        const std::shared_ptr<LiteHtmlResourceManager> self = weak.lock();
        if (!self)
            return;
        
        QByteArray data;
        if (handler) {
            data = handler(url, ResourceType::Image);
        }
        if (data.isEmpty()) {
            if (url.scheme() == "data" || url.toString().startsWith("data:")) {
                data = parseDataUrl(url);
            } else if (url.scheme() == "qrc" || url.toString().startsWith("qrc:") || url.toString().startsWith(":/")) {
                const QString qrcPath = normalizeQrcPath(url);
                QFile file(qrcPath);
                if (file.size() <= kMaxImageSizeBytes && file.open(QIODevice::ReadOnly)) {
                    data = file.readAll();
                }
            } else if (url.isLocalFile() || url.scheme() == "file") {
                const QString localPath = normalizeLocalPath(url);
                QFile file(localPath);
                if (file.size() <= kMaxImageSizeBytes && file.open(QIODevice::ReadOnly)) {
                    data = file.readAll();
                }
            } else if (allowNetwork && (url.scheme() == "http" || url.scheme() == "https")) {
                QNetworkAccessManager netManager;
                QNetworkRequest request(url);
                request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
                request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"));
                
                QNetworkReply *reply = netManager.get(request);
                QEventLoop loop;
                QTimer timer;
                timer.setSingleShot(true);
                
                QObject::connect(&timer, &QTimer::timeout, &loop, [&reply, &loop] {
                    if (reply && reply->isRunning()) {
                        reply->abort();
                    }
                    loop.quit();
                });
                
                QObject::connect(reply, &QNetworkReply::finished, &loop, [&loop, &timer] {
                    timer.stop();
                    loop.quit();
                });

                QByteArray receivedData;
                QObject::connect(reply, &QNetworkReply::readyRead, [&reply, &receivedData] {
                    // Check Content-Length header early if available
                    const auto clHeader = reply->header(QNetworkRequest::ContentLengthHeader);
                    if (clHeader.isValid() && clHeader.toLongLong() > kMaxImageSizeBytes) {
                        reply->abort();
                        receivedData.clear();
                        return;
                    }
                    receivedData.append(reply->readAll());
                    if (receivedData.size() > kMaxImageSizeBytes) {
                        reply->abort();
                        receivedData.clear();
                    }
                });

                timer.start(kNetworkTimeoutMs);
                loop.exec();

                if (reply->error() == QNetworkReply::NoError && !receivedData.isEmpty()) {
                    data = std::move(receivedData);
                }
                reply->deleteLater();
            }
        }

        QImage image;
        if (!data.isEmpty() && data.size() <= kMaxImageSizeBytes) {
            QBuffer buffer(&data);
            buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer);
            reader.setAutoTransform(true);
            const QSize imgSize = reader.size();
            
            // Protection against decompression bombs
            if (imgSize.isValid()) {
                if (imgSize.width() <= kMaxImageDimension && 
                    imgSize.height() <= kMaxImageDimension &&
                    qint64(imgSize.width()) * imgSize.height() <= kMaxImagePixelCount) {
                    image = reader.read();
                }
            } else {
                image = reader.read();
                if (!image.isNull()) {
                    if (image.width() > kMaxImageDimension || 
                        image.height() > kMaxImageDimension ||
                        qint64(image.width()) * image.height() > kMaxImagePixelCount) {
                        image = QImage();
                    }
                }
            }
        }

        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [weak, url, image, redraw_on_ready, generation] {
                const std::shared_ptr<LiteHtmlResourceManager> self = weak.lock();
                if (!self)
                    return;
                
                // Discard stale completion if clearCache() was called during in-flight fetch
                if (self->m_cacheGeneration != generation)
                    return;

                self->m_loadingImages.remove(url);
                if (!image.isNull()) {
                    QPixmap pixmap = QPixmap::fromImage(image);
                    // Cost in KiB, approximated as 4 bytes per pixel.
                    const int cost = qMax(1, (pixmap.width() * pixmap.height() * 4) / 1024);
                    self->m_pixmaps.insert(url, new QPixmap(pixmap), cost);
                } else {
                    // Remember the failure so we do not refetch on every draw.
                    self->m_pixmaps.insert(url, new QPixmap());
                }
                if (redraw_on_ready) {
                    if (!image.isNull() && self->m_relayoutCallback) {
                        self->m_relayoutCallback();
                    }
                    if (self->m_repaintCallback)
                        self->m_repaintCallback();
                }
            },
            Qt::QueuedConnection);
    });
    QThreadPool::globalInstance()->start(task);
}

void LiteHtmlResourceManager::get_image_size(const char *src,
                                             const char *baseurl,
                                             litehtml::size &sz)
{
    const auto qtSrc = QString::fromUtf8(src);
    const auto qtBaseUrl = QString::fromUtf8(baseurl);
    if (qtSrc.isEmpty())
        return;
    qDebug(log) << "get_image_size:"
                << QStringLiteral("src = \"%1\";").arg(qtSrc).toUtf8().constData()
                << QStringLiteral("base = \"%1\"").arg(qtBaseUrl).toUtf8().constData();
    const QPixmap pm = getPixmap(qtSrc, qtBaseUrl);
    sz.width = pm.width();
    sz.height = pm.height();
}

QPixmap LiteHtmlResourceManager::getPixmap(const QString &imageUrl, const QString &baseUrl)
{
    const QUrl url = resolveUrl(imageUrl, baseUrl);
    // object() refreshes the LRU position on access.
    if (const QPixmap *pixmap = m_pixmaps.object(url))
        return *pixmap;
    qWarning(log) << "draw_background: pixmap not loaded for" << url;
    return {};
}

void LiteHtmlResourceManager::import_css(litehtml::string &text,
                                         const litehtml::string &url,
                                         litehtml::string &baseurl)
{
    if (!m_resourceHandler) {
        text.clear();
        return;
    }
    const QUrl actualUrl = resolveUrl(QString::fromUtf8(url.data(), int(url.size())),
                                      QString::fromUtf8(baseurl.data(), int(baseurl.size())));
    const QString urlString = actualUrl.toString(QUrl::None);
    const int lastSlash = urlString.lastIndexOf('/');
    baseurl = urlString.left(lastSlash).toUtf8().constData();
    text = m_resourceHandler(actualUrl, ResourceType::StyleSheet).constData();
}

void LiteHtmlResourceManager::clearCache()
{
    m_pixmaps.clear();
    m_loadingImages.clear();
    ++m_cacheGeneration;
}

QString LiteHtmlResourceManager::serifFont() const
{
    if (!m_cachedSerifFont.isEmpty())
        return m_cachedSerifFont;

#if defined(Q_OS_WIN)
    static const QStringList candidates = {
        QStringLiteral("Times New Roman"),
        QStringLiteral("Georgia"),
        QStringLiteral("SimSun"),
        QStringLiteral("Songti SC"),
        QStringLiteral("Cambria")
    };
#elif defined(Q_OS_MACOS)
    static const QStringList candidates = {
        QStringLiteral("Times New Roman"),
        QStringLiteral("Times"),
        QStringLiteral("Songti SC"),
        QStringLiteral("Georgia")
    };
#else
    static const QStringList candidates = {
        QStringLiteral("Noto Serif CJK SC"),
        QStringLiteral("Noto Serif"),
        QStringLiteral("DejaVu Serif"),
        QStringLiteral("Liberation Serif"),
        QStringLiteral("Times New Roman")
    };
#endif
    m_cachedSerifFont = resolveFontFamily(candidates);
    return m_cachedSerifFont;
}

QString LiteHtmlResourceManager::sansSerifFont() const
{
    if (!m_cachedSansSerifFont.isEmpty())
        return m_cachedSansSerifFont;

#if defined(Q_OS_WIN)
    static const QStringList candidates = {
        QStringLiteral("Microsoft YaHei"),
        QStringLiteral("Segoe UI"),
        QStringLiteral("SimHei"),
        QStringLiteral("Arial"),
        QStringLiteral("Tahoma")
    };
#elif defined(Q_OS_MACOS)
    static const QStringList candidates = {
        QStringLiteral("PingFang SC"),
        QStringLiteral("Helvetica Neue"),
        QStringLiteral("Arial"),
        QStringLiteral("Heiti SC")
    };
#else
    static const QStringList candidates = {
        QStringLiteral("Noto Sans CJK SC"),
        QStringLiteral("DejaVu Sans"),
        QStringLiteral("Liberation Sans"),
        QStringLiteral("WenQuanYi Micro Hei"),
        QStringLiteral("Arial")
    };
#endif
    m_cachedSansSerifFont = resolveFontFamily(candidates);
    return m_cachedSansSerifFont;
}

QString LiteHtmlResourceManager::monospaceFont() const
{
    if (!m_cachedMonospaceFont.isEmpty())
        return m_cachedMonospaceFont;

#if defined(Q_OS_WIN)
    static const QStringList candidates = {
        QStringLiteral("Consolas"),
        QStringLiteral("Cascadia Code"),
        QStringLiteral("Courier New"),
        QStringLiteral("Microsoft YaHei UI")
    };
#elif defined(Q_OS_MACOS)
    static const QStringList candidates = {
        QStringLiteral("Menlo"),
        QStringLiteral("Monaco"),
        QStringLiteral("Courier New"),
        QStringLiteral("Courier")
    };
#else
    static const QStringList candidates = {
        QStringLiteral("Noto Sans Mono CJK SC"),
        QStringLiteral("DejaVu Sans Mono"),
        QStringLiteral("Liberation Mono"),
        QStringLiteral("Courier New")
    };
#endif
    m_cachedMonospaceFont = resolveFontFamily(candidates);
    return m_cachedMonospaceFont;
}

int LiteHtmlResourceManager::defaultFontSize(const QFont &defaultFont, QPaintDevice *paintDevice) const
{
    int pointSize = defaultFont.pointSize();
    if (pointSize <= 0) {
        int pixelSize = defaultFont.pixelSize();
        if (pixelSize > 0 && paintDevice) {
            pointSize = qRound(pixelSize * 72.0 / paintDevice->logicalDpiY());
        }
    }
    if (pointSize <= 0) {
        pointSize = 16;
    }
    return pointSize;
}

} // namespace qlitehtml::internal
