#include "litehtml_resource_manager.h"
#include "container_internal.h"
#include "litehtml_resource_task.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QImage>
#include <QImageReader>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QNetworkReply>
#include <QThreadPool>

namespace {
static Q_LOGGING_CATEGORY(log, "qlitehtml.resource", QtCriticalMsg)

constexpr qint64 kMaxImageSizeBytes = 16 * 1024 * 1024;       // 16 MiB max payload
constexpr int kMaxImageDimension = 16384;                     // 16384 px max width or height
constexpr qint64 kMaxImagePixelCount = 32 * 1024 * 1024;      // 32 megapixels max

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

LiteHtmlResourceManager::~LiteHtmlResourceManager()
{
    cancelAll();
}

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

void LiteHtmlResourceManager::cancelUrl(const QUrl &url)
{
    const auto it = m_activeTasks.find(url);
    if (it != m_activeTasks.end()) {
        const auto &task = it.value();
        if (task) {
            task->cancelToken->store(true);
            if (auto *reply = task->reply.data()) {
                QMetaObject::invokeMethod(reply, &QNetworkReply::abort, Qt::QueuedConnection);
            }
        }
        m_activeTasks.erase(it);
    }
    m_loadingImages.remove(url);
}

void LiteHtmlResourceManager::cancelAll()
{
    for (const auto &task : m_activeTasks) {
        if (task) {
            task->cancelToken->store(true);
            if (auto *reply = task->reply.data()) {
                QMetaObject::invokeMethod(reply, &QNetworkReply::abort, Qt::QueuedConnection);
            }
        }
    }
    m_activeTasks.clear();
    m_loadingImages.clear();
}

void LiteHtmlResourceManager::clearCache()
{
    cancelAll();
    m_pixmaps.clear();
    m_rawBytesCache.clear();
    m_intrinsicSizes.clear();
    ++m_cacheGeneration;
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

    // Fast-path: check L2 raw bytes in RAM
    if (const QByteArray *cachedBytes = m_rawBytesCache.object(url)) {
        QImage img;
        img.loadFromData(*cachedBytes);
        if (!img.isNull()) {
            QPixmap pm = QPixmap::fromImage(img);
            const int cost = qMax(1, (pm.width() * pm.height() * 4) / 1024);
            m_pixmaps.insert(url, new QPixmap(pm), cost);
            if (redraw_on_ready) {
                if (m_relayoutCallback)
                    m_relayoutCallback();
                if (m_repaintCallback)
                    m_repaintCallback();
            }
            return;
        }
    }

    // Register generic active task context with cancel token
    auto taskCtx = std::make_shared<ResourceTaskContext>();
    m_activeTasks.insert(url, taskCtx);
    m_loadingImages.insert(url);

    const uint64_t generation = m_cacheGeneration;
    const auto handler = m_resourceHandler; // snapshot handler on GUI thread
    const bool allowNetwork = m_allowNetworkAccess;
    const std::weak_ptr<LiteHtmlResourceManager> weak = shared_from_this();

    ResourceRequest req{url, ResourceType::Image, 0, allowNetwork};

    // Phase 1: Header Sniffing for Intrinsic Sizing (Anti-CLS)
    auto onChunk = [weak, url](const QByteArray &cumulativeData) {
        if (cumulativeData.size() >= 512) {
            QBuffer buf(const_cast<QByteArray *>(&cumulativeData));
            buf.open(QIODevice::ReadOnly);
            QImageReader reader(&buf);
            const QSize s = reader.size();
            if (s.isValid() && s.width() <= kMaxImageDimension && s.height() <= kMaxImageDimension) {
                QMetaObject::invokeMethod(
                    QCoreApplication::instance(),
                    [weak, url, s] {
                        if (const auto self = weak.lock()) {
                            if (!self->m_intrinsicSizes.contains(url)) {
                                self->m_intrinsicSizes.insert(url, s);
                                if (self->m_relayoutCallback)
                                    self->m_relayoutCallback();
                            }
                        }
                    },
                    Qt::QueuedConnection);
            }
        }
    };

    // Phase 2: Image Decoding Post-Processor (Worker thread)
    auto processor = [](const QByteArray &data, const std::shared_ptr<ResourceTaskContext> &ctx) -> QImage {
        if (data.isEmpty() || data.size() > kMaxImageSizeBytes || ctx->cancelToken->load())
            return {};

        QBuffer buffer(const_cast<QByteArray *>(&data));
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        reader.setAutoTransform(true);
        const QSize imgSize = reader.size();

        // Protection against decompression bombs
        if (imgSize.isValid()) {
            if (imgSize.width() <= kMaxImageDimension &&
                imgSize.height() <= kMaxImageDimension &&
                qint64(imgSize.width()) * imgSize.height() <= kMaxImagePixelCount) {
                return reader.read();
            }
        } else {
            QImage img = reader.read();
            if (!img.isNull()) {
                if (img.width() <= kMaxImageDimension &&
                    img.height() <= kMaxImageDimension &&
                    qint64(img.width()) * img.height() <= kMaxImagePixelCount) {
                    return img;
                }
            }
        }
        return {};
    };

    // Phase 3: Completion Callback (GUI thread)
    auto onComplete = [weak, url, redraw_on_ready, generation](const QByteArray &rawData, QImage &&image) {
        const std::shared_ptr<LiteHtmlResourceManager> self = weak.lock();
        if (!self || self->m_cacheGeneration != generation)
            return;

        self->m_activeTasks.remove(url);
        self->m_loadingImages.remove(url);

        if (!image.isNull()) {
            // L2 Cache: save raw compressed bytes
            if (!rawData.isEmpty()) {
                const int rawCost = qMax(1, int(rawData.size() / 1024));
                self->m_rawBytesCache.insert(url, new QByteArray(rawData), rawCost);
            }
            // L1 Cache: save decoded QPixmap
            QPixmap pixmap = QPixmap::fromImage(image);
            const int cost = qMax(1, (pixmap.width() * pixmap.height() * 4) / 1024);
            self->m_pixmaps.insert(url, new QPixmap(pixmap), cost);
        } else {
            // Remember failure so we do not refetch on every draw
            self->m_pixmaps.insert(url, new QPixmap());
        }

        if (redraw_on_ready) {
            if (!image.isNull() && self->m_relayoutCallback) {
                self->m_relayoutCallback();
            }
            if (self->m_repaintCallback)
                self->m_repaintCallback();
        }
    };

    auto *task = new GenericResourceTask<QImage>(
        std::move(req),
        handler,
        taskCtx,
        std::move(onChunk),
        std::move(processor),
        std::move(onComplete));

    QThreadPool::globalInstance()->start(task, req.priority);
}

void LiteHtmlResourceManager::get_image_size(const char *src,
                                             const char *baseurl,
                                             litehtml::size &sz)
{
    const auto qtSrc = QString::fromUtf8(src);
    const auto qtBaseUrl = QString::fromUtf8(baseurl);
    if (qtSrc.isEmpty())
        return;
    const QUrl url = resolveUrl(qtSrc, qtBaseUrl);

    // 1. Intrinsic size from Header Sniffing (immediate layout without waiting for full download)
    if (m_intrinsicSizes.contains(url)) {
        const QSize s = m_intrinsicSizes.value(url);
        sz.width = s.width();
        sz.height = s.height();
        return;
    }

    // 2. Pixmap from L1/L2 cache
    const QPixmap pm = getPixmap(qtSrc, qtBaseUrl);
    sz.width = pm.width();
    sz.height = pm.height();
}

QPixmap LiteHtmlResourceManager::getPixmap(const QString &imageUrl, const QString &baseUrl)
{
    const QUrl url = resolveUrl(imageUrl, baseUrl);
    // L1: Decoded QPixmap Cache
    if (const QPixmap *pixmap = m_pixmaps.object(url))
        return *pixmap;
    // L2: Raw bytes RAM Cache -> Fast in-memory decode
    if (const QByteArray *rawBytes = m_rawBytesCache.object(url)) {
        QImage img;
        img.loadFromData(*rawBytes);
        if (!img.isNull()) {
            QPixmap pm = QPixmap::fromImage(img);
            const int cost = qMax(1, (pm.width() * pm.height() * 4) / 1024);
            m_pixmaps.insert(url, new QPixmap(pm), cost);
            return pm;
        }
    }
    qWarning(log) << "draw_background: pixmap not loaded for" << url;
    return {};
}

void LiteHtmlResourceManager::import_css(litehtml::string &text,
                                         const litehtml::string &url,
                                         litehtml::string &baseurl)
{
    const QUrl actualUrl = resolveUrl(QString::fromUtf8(url.data(), int(url.size())),
                                      QString::fromUtf8(baseurl.data(), int(baseurl.size())));
    const QString urlString = actualUrl.toString(QUrl::None);
    const int lastSlash = urlString.lastIndexOf('/');
    baseurl = urlString.left(lastSlash).toUtf8().constData();

    // 1. Custom handler override
    if (m_resourceHandler) {
        const QByteArray data = m_resourceHandler(actualUrl, ResourceType::StyleSheet);
        if (!data.isEmpty()) {
            text = data.constData();
            return;
        }
    }

    // 2. Check L2 raw bytes in-memory cache
    if (const QByteArray *cached = m_rawBytesCache.object(actualUrl)) {
        text = cached->constData();
        return;
    }

    // 3. Synchronous local protocols (data:, qrc:, file:)
    if (actualUrl.scheme() == "data" || actualUrl.scheme() == "qrc" || actualUrl.isLocalFile() || actualUrl.scheme() == "file") {
        ResourceRequest req{actualUrl, ResourceType::StyleSheet, 0, m_allowNetworkAccess};
        auto ctx = std::make_shared<ResourceTaskContext>();
        QByteArray data = GenericResourceFetcher::fetchRawData(req, nullptr, ctx);
        if (!data.isEmpty()) {
            m_rawBytesCache.insert(actualUrl, new QByteArray(data), qMax(1, int(data.size() / 1024)));
            text = data.constData();
            return;
        }
    }

    // 4. HTTP/HTTPS asynchronous prefetch (never block the UI thread!)
    if (m_allowNetworkAccess && (actualUrl.scheme() == "http" || actualUrl.scheme() == "https")) {
        if (!m_activeTasks.contains(actualUrl)) {
            ResourceRequest req{actualUrl, ResourceType::StyleSheet, 0, m_allowNetworkAccess};
            auto ctx = std::make_shared<ResourceTaskContext>();
            m_activeTasks.insert(actualUrl, ctx);

            const std::weak_ptr<LiteHtmlResourceManager> weak = shared_from_this();
            const uint64_t generation = m_cacheGeneration;

            auto processor = [](const QByteArray &data, const std::shared_ptr<ResourceTaskContext> &) -> QString {
                return QString::fromUtf8(data);
            };

            auto onComplete = [weak, actualUrl, generation](const QByteArray &rawData, QString &&) {
                const std::shared_ptr<LiteHtmlResourceManager> self = weak.lock();
                if (!self || self->m_cacheGeneration != generation)
                    return;

                self->m_activeTasks.remove(actualUrl);
                if (!rawData.isEmpty()) {
                    self->m_rawBytesCache.insert(actualUrl, new QByteArray(rawData), qMax(1, int(rawData.size() / 1024)));
                    // Trigger relayout so litehtml re-evaluates document styles with the newly loaded CSS
                    if (self->m_relayoutCallback)
                        self->m_relayoutCallback();
                }
            };

            auto *task = new GenericResourceTask<QString>(
                std::move(req),
                m_resourceHandler,
                ctx,
                nullptr,
                std::move(processor),
                std::move(onComplete));

            QThreadPool::globalInstance()->start(task, req.priority);
        }
    }

    text.clear();
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
