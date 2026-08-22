#pragma once

#include "qlitehtml_types.h"
#include "litehtml_resource_task.h"
#include <litehtml.h>

#include <QByteArray>
#include <QCache>
#include <QFont>
#include <QHash>
#include <QPaintDevice>
#include <QPixmap>
#include <QSet>
#include <QSize>
#include <QString>
#include <QUrl>

#include <cstdint>
#include <functional>
#include <memory>

namespace qlitehtml::internal {

/**
 * @brief Manages resources (images, stylesheets, fonts) for LiteHTML documents.
 * 
 * Implements a modern multi-tier caching architecture (L1 Decoded Pixmaps, L2 RAM Raw Bytes,
 * L3 HTTP Disk Cache), header sniffing for zero-CLS layout placeholders, cancellation tokens,
 * and thread-safe asynchronous loading.
 */
class LiteHtmlResourceManager : public std::enable_shared_from_this<LiteHtmlResourceManager>
{
public:
    using ResourceHandler = qlitehtml::ResourceHandler;
    using RepaintCallback = qlitehtml::RepaintCallback;

    LiteHtmlResourceManager();
    ~LiteHtmlResourceManager();

    void setBaseUrl(const QString &baseUrl);
    QString baseUrl() const;

    /**
     * @brief Sets the custom resource handler for intercepting requests.
     * @note The handler must be thread-safe as it is called from worker threads.
     */
    void setResourceHandler(const ResourceHandler &handler);
    ResourceHandler resourceHandler() const;

    void setRepaintCallback(const RepaintCallback &callback);
    void setRelayoutCallback(const std::function<void()> &callback);

    /**
     * @brief Resolves a relative or absolute URL against the document base URL.
     */
    QUrl resolveUrl(const QString &url, const QString &baseUrl = QString()) const;

    /**
     * @brief Asynchronously loads an image, performing header sniffing and 3-tier caching.
     */
    void load_image(const char *src, const char *baseurl, bool redraw_on_ready = true);

    /**
     * @brief Queries the image dimensions (returns intrinsic dimensions from header sniff if in-flight).
     */
    void get_image_size(const char *src, const char *baseurl, litehtml::size &sz);

    /**
     * @brief Retrieves a decoded QPixmap from L1/L2 cache (fast synchronous hit).
     */
    QPixmap getPixmap(const QString &imageUrl, const QString &baseUrl = QString());

    /**
     * @brief Imports external CSS stylesheets (synchronous local hit, asynchronous remote prefetch).
     */
    void import_css(litehtml::string &text,
                    const litehtml::string &url,
                    litehtml::string &baseurl);

    /**
     * @brief Clears all caches, aborts all in-flight requests, and advances the cache generation token.
     */
    void clearCache();

    /**
     * @brief Cancels an in-flight resource task for a specific URL.
     */
    void cancelUrl(const QUrl &url);

    /**
     * @brief Aborts all active in-flight resource tasks immediately.
     */
    void cancelAll();

    /**
     * @brief Configures permission for making external HTTP/HTTPS network requests.
     */
    void setAllowNetworkAccess(bool allow);
    bool allowNetworkAccess() const;

    /**
     * @brief Cross-platform font fallback resolvers backed by QFontDatabase.
     */
    QString serifFont() const;
    QString sansSerifFont() const;
    QString monospaceFont() const;

    int defaultFontSize(const QFont &defaultFont, QPaintDevice *paintDevice) const;

private:
    QString m_baseUrl;
    QCache<QUrl, QPixmap> m_pixmaps{64 * 1024};         ///< L1: 64 MiB Decoded QPixmap LRU Cache (GUI thread).
    QCache<QUrl, QByteArray> m_rawBytesCache{64 * 1024}; ///< L2: 64 MiB Raw Binary RAM Cache (GUI thread).
    QHash<QUrl, QSize> m_intrinsicSizes;                 ///< Intrinsic size table populated via Header Sniffing.
    QHash<QUrl, std::shared_ptr<ResourceTaskContext>> m_activeTasks; ///< Active in-flight tasks for aborting.
    QSet<QUrl> m_loadingImages;
    uint64_t m_cacheGeneration = 0;
    bool m_allowNetworkAccess = false;
    ResourceHandler m_resourceHandler;
    RepaintCallback m_repaintCallback;
    std::function<void()> m_relayoutCallback;

    mutable QString m_cachedSerifFont;
    mutable QString m_cachedSansSerifFont;
    mutable QString m_cachedMonospaceFont;
};

} // namespace qlitehtml::internal
