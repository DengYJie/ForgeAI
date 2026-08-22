#pragma once

#include "qlitehtml_types.h"
#include <litehtml.h>

#include <QCache>
#include <QFont>
#include <QPaintDevice>
#include <QPixmap>
#include <QSet>
#include <QString>
#include <QUrl>

#include <cstdint>
#include <functional>
#include <memory>

namespace qlitehtml::internal {

class LiteHtmlResourceManager : public std::enable_shared_from_this<LiteHtmlResourceManager>
{
public:
    using ResourceHandler = qlitehtml::ResourceHandler;
    using RepaintCallback = qlitehtml::RepaintCallback;

    LiteHtmlResourceManager();
    ~LiteHtmlResourceManager() = default;

    void setBaseUrl(const QString &baseUrl);
    QString baseUrl() const;

    // Note: ResourceHandler must be thread-safe as it will be invoked from worker threads.
    void setResourceHandler(const ResourceHandler &handler);
    ResourceHandler resourceHandler() const;

    void setRepaintCallback(const RepaintCallback &callback);
    void setRelayoutCallback(const std::function<void()> &callback);

    QUrl resolveUrl(const QString &url, const QString &baseUrl = QString()) const;

    void load_image(const char *src, const char *baseurl, bool redraw_on_ready = true);
    void get_image_size(const char *src, const char *baseurl, litehtml::size &sz);
    QPixmap getPixmap(const QString &imageUrl, const QString &baseUrl = QString());

    void import_css(litehtml::string &text,
                    const litehtml::string &url,
                    litehtml::string &baseurl);

    void clearCache();

    void setAllowNetworkAccess(bool allow);
    bool allowNetworkAccess() const;

    QString serifFont() const;
    QString sansSerifFont() const;
    QString monospaceFont() const;

    int defaultFontSize(const QFont &defaultFont, QPaintDevice *paintDevice) const;

private:
    QString m_baseUrl;
    QCache<QUrl, QPixmap> m_pixmaps{64 * 1024}; // 64 MiB limit
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
