#pragma once

#include "container_qpainter.h"
#include "elements/details_element.h"
#include "litehtml_renderer.h"

#include <litehtml.h>

#include <QCache>
#include <QHash>
#include <QPaintDevice>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>

#include <unordered_map>

#include "litehtml_interactor.h"
#include "litehtml_resource_manager.h"

class DocumentContainerPrivate final
    : public litehtml::document_container
    , public std::enable_shared_from_this<DocumentContainerPrivate>
{
public: // document_container API
    litehtml::uint_ptr create_font(const litehtml::font_description &descr,
                                   const litehtml::document *doc,
                                   litehtml::font_metrics *fm) override;
    void delete_font(litehtml::uint_ptr hFont) override;
    litehtml::pixel_t text_width(const char *text, litehtml::uint_ptr hFont) override;
    void draw_text(litehtml::uint_ptr hdc,
                   const char *text,
                   litehtml::uint_ptr hFont,
                   litehtml::web_color color,
                   const litehtml::position &pos) override;
    litehtml::pixel_t pt_to_px(float pt) const override;
    litehtml::pixel_t get_default_font_size() const override;
    const char *get_default_font_name() const override;
    void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker &marker) override;
    void load_image(const char *src,
                    const char *baseurl,
                    bool redraw_on_ready) override;
    void get_image_size(const char *src,
                        const char *baseurl,
                        litehtml::size &sz) override;
    void draw_image(litehtml::uint_ptr hdc,
                    const litehtml::background_layer &layer,
                    const std::string &url,
                    const std::string &base_url) override;
    void draw_solid_fill(litehtml::uint_ptr hdc,
                         const litehtml::background_layer &layer,
                         const litehtml::web_color &color) override;
    void draw_linear_gradient(litehtml::uint_ptr hdc,
                              const litehtml::background_layer &layer,
                              const litehtml::background_layer::linear_gradient &gradient) override;
    void draw_radial_gradient(litehtml::uint_ptr hdc,
                              const litehtml::background_layer &layer,
                              const litehtml::background_layer::radial_gradient &gradient) override;
    void draw_conic_gradient(litehtml::uint_ptr hdc,
                             const litehtml::background_layer &layer,
                             const litehtml::background_layer::conic_gradient &gradient) override;
    void draw_borders(litehtml::uint_ptr hdc,
                      const litehtml::borders &borders,
                      const litehtml::position &draw_pos,
                      bool root) override;
    void set_caption(const char *caption) override;
    void set_base_url(const char *base_url) override;
    void link(const std::shared_ptr<litehtml::document> &doc,
              const litehtml::element::ptr &el) override;
    void on_anchor_click(const char *url, const litehtml::element::ptr &el) override;
    bool on_element_click(const litehtml::element::ptr &el) override;
    void on_mouse_event(const litehtml::element::ptr &el, litehtml::mouse_event event) override;
    void set_cursor(const char *cursor) override;
    void transform_text(litehtml::string &text, litehtml::text_transform tt) override;
    void import_css(litehtml::string &text,
                    const litehtml::string &url,
                    litehtml::string &baseurl) override;
    void set_clip(const litehtml::position &pos,
                  const litehtml::border_radiuses &bdr_radius) override;
    void del_clip() override;
    void get_viewport(litehtml::position &viewport) const override;
    std::shared_ptr<litehtml::element> create_element(
        const char *tag_name,
        const litehtml::string_map &attributes,
        const std::shared_ptr<litehtml::document> &doc) override;
    void get_media_features(litehtml::media_features &media) const override;
    void get_language(litehtml::string &language, litehtml::string &culture) const override;

    QPixmap getPixmap(const QString &imageUrl, const QString &baseUrl);
    QString serifFont() const;
    QString sansSerifFont() const;
    QString monospaceFont() const;
    QUrl resolveUrl(const QString &url, const QString &baseUrl) const;
    void drawSelection(QPainter *painter, const QRect &clip) const;

    QPaintDevice *m_paintDevice = nullptr;
    QPainter *m_painter = nullptr;
    // The owning public container; used by async image completion to trigger
    // a re-layout through DocumentContainer::render().
    DocumentContainer *m_owner = nullptr;
    litehtml::document::ptr m_document;
    QHash<QByteArray, DocumentContainer::ElementFactory> m_elementFactories;
    // Set when the document or its fonts changed and render() must re-layout,
    // even if the viewport width did not change.
    bool m_needRelayout = true;
    QString m_baseUrl;
    QRect m_clientRect;
    QPoint m_scrollPosition;
    QString m_caption;
    QFont m_defaultFont = QFont(sansSerifFont(), 16);
    mutable qlitehtml::internal::LiteHtmlRenderer m_renderer;
    qlitehtml::internal::LiteHtmlInteractor m_interactor;
    std::shared_ptr<qlitehtml::internal::LiteHtmlResourceManager> m_resourceManager;
    QByteArray m_defaultFontFamilyName = m_defaultFont.family().toUtf8();
    // Invoked on the main thread after an async image finished loading, so
    // the widget can repaint the (re-laid-out) viewport.
    DocumentContainer::RepaintCallback m_repaintCallback;
    DocumentContainer::PaletteCallback m_paletteCallback;

    void rebuildRenderTree();
};

class DocumentContainerContextPrivate
{
public:
    QString masterStyleSheet;
};
