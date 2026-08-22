#include "container_qpainter.h"
#include "container_qpainter_p.h"
#include "container_internal.h"
#include "elements/button_element.h"
#include "elements/details_element.h"
#include "elements/form_control_element.h"
#include "elements/summary_element.h"

#include <QCursor>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QRegularExpression>
#include <QScreen>
#include <QThreadPool>
#include <QUrl>
#include <QWidget>

#include <algorithm>
#include <limits>
#include <set>

using namespace qlitehtml::internal;

namespace {
static Q_LOGGING_CATEGORY(log, "qlitehtml", QtCriticalMsg)
}

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
namespace Qt {
namespace {
auto constexpr SkipEmptyParts = QString::SkipEmptyParts;
}
} // namespace Qt
#endif

DocumentContainer::DocumentContainer()
    : d(std::make_shared<DocumentContainerPrivate>())
{
    d->m_owner = this;
    d->m_resourceManager = std::make_shared<LiteHtmlResourceManager>();

    const std::weak_ptr<DocumentContainerPrivate> weakPriv = d;
    d->m_interactor.setRelayoutCallback([weakPriv]() {
        if (const auto priv = weakPriv.lock()) {
            priv->rebuildRenderTree();
            priv->m_needRelayout = true;
            if (priv->m_owner)
                priv->m_owner->render(priv->m_clientRect.width(), priv->m_clientRect.height());
        }
    });
    d->m_resourceManager->setRelayoutCallback([weakPriv]() {
        if (const auto priv = weakPriv.lock()) {
            priv->m_needRelayout = true;
            if (priv->m_owner)
                priv->m_owner->render(priv->m_clientRect.width(), priv->m_clientRect.height());
        }
    });
    d->m_resourceManager->setRepaintCallback([weakPriv]() {
        if (const auto priv = weakPriv.lock()) {
            if (priv->m_repaintCallback)
                priv->m_repaintCallback();
        }
    });

    // Register custom elements for form controls
    registerElementFactory("input",
                           [](const char *tag_name,
                              const litehtml::string_map &attributes,
                              const std::shared_ptr<litehtml::document> &doc) {
                               const auto type = attributes.find("type");
                               if (type != attributes.end()) {
                                   if (type->second == "button" || type->second == "submit" || type->second == "reset") {
                                       auto btn = std::make_shared<button_element>(doc);
                                       btn->set_tagName(tag_name);
                                       return std::static_pointer_cast<litehtml::element>(btn);
                                   }
                               }
                               // Return nullptr for unsupported input types (text, etc.) to let litehtml handle them natively
                               return std::shared_ptr<litehtml::element>();
                           });

    registerElementFactory("button",
                           [](const char *tag_name,
                              const litehtml::string_map &,
                              const std::shared_ptr<litehtml::document> &doc) {
                               auto btn = std::make_shared<button_element>(doc);
                               btn->set_tagName(tag_name);
                               return std::static_pointer_cast<litehtml::element>(btn);
                           });

    registerElementFactory("details",
                           [](const char *tag_name,
                              const litehtml::string_map &,
                              const std::shared_ptr<litehtml::document> &doc) {
                               auto details = std::make_shared<details_element>(doc);
                               details->set_tagName(tag_name);
                               return std::static_pointer_cast<litehtml::element>(details);
                           });

    registerElementFactory("summary",
                           [](const char *tag_name,
                              const litehtml::string_map &,
                              const std::shared_ptr<litehtml::document> &doc) {
                               auto summary = std::make_shared<summary_element>(doc);
                               summary->set_tagName(tag_name);
                               return std::static_pointer_cast<litehtml::element>(summary);
                           });
}

DocumentContainer::~DocumentContainer()
{
    if (d) {
        d->m_owner = nullptr;
    }
}

litehtml::uint_ptr DocumentContainerPrivate::create_font(const litehtml::font_description &descr,
                                                         const litehtml::document *doc,
                                                         litehtml::font_metrics *fm)
{
    Q_UNUSED(doc)
    const QStringList splitNames
        = QString::fromUtf8(descr.family.data(), int(descr.family.size())).split(',', Qt::SkipEmptyParts);
    QStringList familyNames;
    std::transform(splitNames.cbegin(),
                   splitNames.cend(),
                   std::back_inserter(familyNames),
                   [this](const QString &s) {
                       // clean whitespace and quotes
                       QString name = s.trimmed();
                       if (name.startsWith('\"'))
                           name = name.mid(1);
                       if (name.endsWith('\"'))
                           name.chop(1);
                       const QString lowerName = name.toLower();
                       if (lowerName == "serif")
                           return serifFont();
                       if (lowerName == "sans-serif")
                           return sansSerifFont();
                       if (lowerName == "monospace")
                           return monospaceFont();
                       return name;
                   });
    auto font = new QFont();
#if (QT_VERSION >= QT_VERSION_CHECK(5, 13, 0))
    font->setFamilies(familyNames);
#else
    struct CompareCaseinsensitive
    {
        bool operator()(const QString &a, const QString &b) const
        {
            return a.compare(b, Qt::CaseInsensitive) < 0;
        }
    };
    static const QStringList known = QFontDatabase().families();
    static const std::set<QString, CompareCaseinsensitive> knownFamilies(known.cbegin(),
                                                                         known.cend());
    font->setFamily(familyNames.last());
    for (const QString &name : qAsConst(familyNames)) {
        const auto found = knownFamilies.find(name);
        if (found != knownFamilies.end()) {
            font->setFamily(*found);
            break;
        }
    }
#endif
    font->setPixelSize(qRound(descr.size));
    font->setWeight(cssWeightToQtWeight(descr.weight));
    font->setStyle(toQFontStyle(descr.style));
    if (descr.decoration_line & litehtml::text_decoration_line_underline)
        font->setUnderline(true);
    if (descr.decoration_line & litehtml::text_decoration_line_overline)
        font->setOverline(true);
    if (descr.decoration_line & litehtml::text_decoration_line_line_through)
        font->setStrikeOut(true);
    if (fm) {
        const QFontMetrics metrics(*font);
        fm->height = metrics.height();
        fm->ascent = metrics.ascent();
        fm->descent = metrics.descent();
        fm->x_height = metrics.xHeight();
        fm->draw_spaces = true;
    }
    return reinterpret_cast<litehtml::uint_ptr>(font);
}

void DocumentContainerPrivate::delete_font(litehtml::uint_ptr hFont)
{
    auto font = reinterpret_cast<Font *>(hFont);
    delete font;
}

litehtml::pixel_t DocumentContainerPrivate::text_width(const char *text, litehtml::uint_ptr hFont)
{
    const QFontMetrics fm(toQFont(hFont));
    return fm.horizontalAdvance(QString::fromUtf8(text));
}

litehtml::pixel_t DocumentContainerPrivate::pt_to_px(float pt) const
{
    const qreal dpi = m_paintDevice ? m_paintDevice->logicalDpiY() : 96.0;
    return qreal(pt) * dpi / 72.0;
}

litehtml::pixel_t DocumentContainerPrivate::get_default_font_size() const
{
    return m_resourceManager ? m_resourceManager->defaultFontSize(m_defaultFont, m_paintDevice) : 16;
}

const char *DocumentContainerPrivate::get_default_font_name() const
{
    return m_defaultFontFamilyName.constData();
}

void DocumentContainerPrivate::load_image(const char *src,
                                          const char *baseurl,
                                          bool redraw_on_ready)
{
    if (m_resourceManager)
        m_resourceManager->load_image(src, baseurl, redraw_on_ready);
}

void DocumentContainerPrivate::get_image_size(const char *src,
                                              const char *baseurl,
                                              litehtml::size &sz)
{
    if (m_resourceManager)
        m_resourceManager->get_image_size(src, baseurl, sz);
}

void DocumentContainerPrivate::set_caption(const char *caption)
{
    m_caption = QString::fromUtf8(caption);
}

void DocumentContainerPrivate::set_base_url(const char *base_url)
{
    m_baseUrl = QString::fromUtf8(base_url);
    m_interactor.setBaseUrl(m_baseUrl);
    if (m_resourceManager)
        m_resourceManager->setBaseUrl(m_baseUrl);
}

void DocumentContainerPrivate::link(const std::shared_ptr<litehtml::document> &doc,
                                    const litehtml::element::ptr &el)
{
    // TODO
    qDebug(log) << "link";
    Q_UNUSED(doc)
    Q_UNUSED(el)
}

void DocumentContainerPrivate::on_anchor_click(const char *url,
                                               const litehtml::element::ptr &el)
{
    m_interactor.on_anchor_click(url, el);
}

bool DocumentContainerPrivate::on_element_click(const litehtml::element::ptr &el)
{
    return m_interactor.on_element_click(el);
}

void DocumentContainerPrivate::on_mouse_event(const litehtml::element::ptr &el,
                                              litehtml::mouse_event event)
{
    m_interactor.on_mouse_event(el, event);
}

void DocumentContainerPrivate::set_cursor(const char *cursor)
{
    m_interactor.set_cursor(cursor);
}

void DocumentContainerPrivate::transform_text(litehtml::string &text, litehtml::text_transform tt)
{
    if (text.empty())
        return;
    QString str = QString::fromUtf8(text.data(), int(text.length()));
    switch (tt) {
    case litehtml::text_transform_uppercase:
        str = str.toUpper();
        break;
    case litehtml::text_transform_lowercase:
        str = str.toLower();
        break;
    case litehtml::text_transform_capitalize: {
        bool capitalizeNext = true;
        for (int i = 0; i < str.length(); ++i) {
            if (str.at(i).isSpace()) {
                capitalizeNext = true;
            } else if (capitalizeNext) {
                str[i] = str[i].toUpper();
                capitalizeNext = false;
            }
        }
        break;
    }
    default:
        return;
    }
    const QByteArray utf8 = str.toUtf8();
    text.assign(utf8.constData(), size_t(utf8.size()));
}

void DocumentContainerPrivate::import_css(litehtml::string &text,
                                          const litehtml::string &url,
                                          litehtml::string &baseurl)
{
    if (m_resourceManager)
        m_resourceManager->import_css(text, url, baseurl);
    else
        text.clear();
}

void DocumentContainerPrivate::set_clip(const litehtml::position &pos,
                                        const litehtml::border_radiuses &bdr_radius)
{
    if (m_painter) {
        m_painter->save();
        m_painter->setClipRect(toQRect(pos), Qt::IntersectClip);
        // Note: We could also apply bdr_radius via QPainterPath here if needed
        // for rounded corner clipping of child elements.
        if (bdr_radius.top_left_x > 0 || bdr_radius.top_right_x > 0 ||
            bdr_radius.bottom_left_x > 0 || bdr_radius.bottom_right_x > 0) {
            QPainterPath path;
            const QRectF borderBox = toQRect(pos);
            const auto &r = bdr_radius;
            
            if (r.top_left_x == r.top_right_x && r.top_left_x == r.bottom_left_x && r.top_left_x == r.bottom_right_x &&
                r.top_left_y == r.top_right_y && r.top_left_y == r.bottom_left_y && r.top_left_y == r.bottom_right_y) {
                path.addRoundedRect(borderBox, r.top_left_x, r.top_left_y);
            } else {
                path.setFillRule(Qt::WindingFill);
                qreal tlx = r.top_left_x, tly = r.top_left_y;
                qreal trx = r.top_right_x, try_ = r.top_right_y;
                qreal blx = r.bottom_left_x, bly = r.bottom_left_y;
                qreal brx = r.bottom_right_x, bry = r.bottom_right_y;
                
                path.moveTo(borderBox.left() + tlx, borderBox.top());
                path.lineTo(borderBox.right() - trx, borderBox.top());
                if (trx > 0 && try_ > 0)
                    path.arcTo(borderBox.right() - 2*trx, borderBox.top(), 2*trx, 2*try_, 90, -90);
                
                path.lineTo(borderBox.right(), borderBox.bottom() - bry);
                if (brx > 0 && bry > 0)
                    path.arcTo(borderBox.right() - 2*brx, borderBox.bottom() - 2*bry, 2*brx, 2*bry, 0, -90);
                
                path.lineTo(borderBox.left() + blx, borderBox.bottom());
                if (blx > 0 && bly > 0)
                    path.arcTo(borderBox.left(), borderBox.bottom() - 2*bly, 2*blx, 2*bly, 270, -90);
                
                path.lineTo(borderBox.left(), borderBox.top() + tly);
                if (tlx > 0 && tly > 0)
                    path.arcTo(borderBox.left(), borderBox.top(), 2*tlx, 2*tly, 180, -90);
                    
                path.closeSubpath();
            }
            m_painter->setClipPath(path, Qt::IntersectClip);
        }
    }
}

void DocumentContainerPrivate::del_clip()
{
    if (m_painter) {
        m_painter->restore();
    }
}

void DocumentContainerPrivate::get_viewport(litehtml::position &viewport) const
{
    viewport.x = m_clientRect.x();
    viewport.y = m_clientRect.y();
    viewport.width = m_clientRect.width();
    viewport.height = m_clientRect.height();
}

std::shared_ptr<litehtml::element> DocumentContainerPrivate::create_element(
    const char *tag_name,
    const litehtml::string_map &attributes,
    const std::shared_ptr<litehtml::document> &doc)
{
    const auto it = m_elementFactories.constFind(
        QByteArray::fromRawData(tag_name, int(std::strlen(tag_name))));
    if (it != m_elementFactories.constEnd()) {
        if (const auto element = it.value()(tag_name, attributes, doc))
            return element;
    }
    return {};
}

void DocumentContainerPrivate::get_media_features(litehtml::media_features &media) const
{
    media.type = litehtml::media_type_screen;
    // Width/height are the viewport in CSS pixels (virtual coordinates, as
    // passed to render()). Screen size feeds device queries.
    media.width = m_clientRect.width();
    media.height = m_clientRect.height();
    const QScreen *screen = nullptr;
    if (auto *widget = dynamic_cast<QWidget *>(m_paintDevice))
        screen = widget->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (screen) {
        media.device_width = screen->geometry().width();
        media.device_height = screen->geometry().height();
        media.resolution = screen->logicalDotsPerInch();
    }
    media.color = 24;
    media.color_index = 0;
    media.monochrome = 0;
}

void DocumentContainerPrivate::rebuildRenderTree()
{
    if (!m_document)
        return;

    m_document->rebuild_render_tree();
    m_needRelayout = true;
}

void DocumentContainerPrivate::get_language(litehtml::string &language,
                                            litehtml::string &culture) const
{
    const QLocale locale = QLocale::system();
    const QString name = locale.name(); // e.g. "zh_CN"
    const QStringList parts = name.split(QLatin1Char('_'));
    if (!parts.isEmpty()) {
        const QByteArray lang = parts.at(0).toUtf8();
        language.assign(lang.constData(), size_t(lang.size()));
        if (parts.size() > 1) {
            const QByteArray cult = parts.at(1).toUtf8();
            culture.assign(cult.constData(), size_t(cult.size()));
        }
    }
}

void DocumentContainer::setPaintDevice(QPaintDevice *paintDevice)
{
    d->m_paintDevice = paintDevice;
}

void DocumentContainer::setDocument(const QByteArray &data, DocumentContainerContext *context)
{
    d->m_interactor.clear();
    // Cache the root elements (they do not change often, and selecting them each render is costly)
    d->m_document = litehtml::document::createFromString(
        data.constData(),
        d.get(),
        context && !context->d->masterStyleSheet.isEmpty()
            ? context->d->masterStyleSheet.toUtf8().constData()
            : nullptr);
    d->m_interactor.setDocument(d->m_document);
    d->m_needRelayout = true;
    d->m_interactor.buildIndex();
}

bool DocumentContainer::hasDocument() const
{
    return d->m_document.get();
}

void DocumentContainer::setBaseUrl(const QString &url)
{
    d->set_base_url(url.toUtf8().constData());
}

void DocumentContainer::clearResourceCache()
{
    if (d->m_resourceManager)
        d->m_resourceManager->clearCache();
}

void DocumentContainer::registerElementFactory(const QByteArray &tagName,
                                               const ElementFactory &factory)
{
    d->m_elementFactories.insert(tagName, factory);
}

void DocumentContainer::appendHtml(const QByteArray &html)
{
    if (html.isEmpty())
        return;
    if (!d->m_document) {
        setDocument(html, nullptr);
        return;
    }

    litehtml::element::ptr body = d->m_document->root()->select_one("body");
    const litehtml::element::ptr parent = body ? body : d->m_document->root();
    d->m_document->append_children_from_string(*parent, html.constData(), false);
    d->m_needRelayout = true;
    render(d->m_clientRect.width(), d->m_clientRect.height());
    d->m_interactor.updateIndex();
}

QVector<QRect> DocumentContainer::fixedBoxes() const
{
    if (!d->m_document)
        return {};

    litehtml::position::vector boxes;
    d->m_document->get_fixed_boxes(boxes);

    QVector<QRect> result;
    result.reserve(int(boxes.size()));
    for (const litehtml::position &box : boxes)
        result.append(toQRect(box));
    return result;
}

void DocumentContainer::setScrollPosition(const QPoint &pos)
{
    d->m_scrollPosition = pos;
}

void DocumentContainer::render(int width, int height)
{
    // litehtml lays out by width; a height-only change (scrollbar range update)
    // must not re-layout the whole document.
    const bool layoutChanged = d->m_needRelayout || width != d->m_clientRect.width();
    d->m_clientRect = {0, 0, width, height};
    d->m_interactor.setClientRect(d->m_clientRect);
    if (!d->m_document)
        return;
    if (layoutChanged) {
        d->m_needRelayout = false;
        // Removed re-rendering at bestWidth because it incorrectly collapses
        // block-level elements (like blockquotes and backgrounds) to their intrinsic content width.
        d->m_document->render(width);
    }
    d->m_interactor.updateSelection();
}

void DocumentContainer::draw(QPainter *painter, const QRect &clip)
{
    // Keep the configured paint device available between paint events.  Input
    // handlers can rebuild the litehtml render tree outside draw(), where it
    // is still needed to convert the widget's pixel-sized default font into
    // CSS points.  Clearing it here made that conversion fall back to 16pt.
    QPaintDevice *const configuredPaintDevice = d->m_paintDevice;
    d->m_paintDevice = painter->device();
    d->m_painter = painter;
    d->drawSelection(painter, clip);
    const QPoint pos = -d->m_scrollPosition;
    const litehtml::position clipRect(clip.x(), clip.y(), clip.width(), clip.height());
    d->m_document->draw(reinterpret_cast<litehtml::uint_ptr>(painter), pos.x(), pos.y(), &clipRect);
    d->m_painter = nullptr;
    d->m_paintDevice = configuredPaintDevice;
}

int DocumentContainer::documentWidth() const
{
    if (!d->m_document)
        return 0;
    return d->m_document->width();
}

int DocumentContainer::documentHeight() const
{
    if (!d->m_document)
        return 0;
    return d->m_document->height();
}

int DocumentContainer::anchorY(const QString &anchorName) const
{
    if (!d->m_document)
        return -1;
    litehtml::element::ptr element = d->m_document->root()->select_one(
        QStringLiteral("a[name=\"%1\"]").arg(anchorName).toUtf8().constData());
    if (!element)
        element = d->m_document->root()->select_one(
            QStringLiteral("#%1").arg(anchorName).toUtf8().constData());
    if (element)
        return element->get_placement().y;
    return -1;
}

QVector<QRect> DocumentContainer::selectionRects() const
{
    return d->m_interactor.selectionRects();
}

QVector<QRect> DocumentContainer::mousePressEvent(const QPoint &documentPos,
                                                  const QPoint &viewportPos,
                                                  Qt::MouseButton button,
                                                  Qt::KeyboardModifiers modifiers)
{
    return d->m_interactor.mousePressEvent(documentPos, viewportPos, button, modifiers);
}

QVector<QRect> DocumentContainer::mouseMoveEvent(const QPoint &documentPos,
                                                 const QPoint &viewportPos)
{
    return d->m_interactor.mouseMoveEvent(documentPos, viewportPos);
}

QVector<QRect> DocumentContainer::mouseReleaseEvent(const QPoint &documentPos,
                                                   const QPoint &viewportPos,
                                                   Qt::MouseButton button)
{
    return d->m_interactor.mouseReleaseEvent(documentPos, viewportPos, button);
}

QVector<QRect> DocumentContainer::mouseDoubleClickEvent(const QPoint &documentPos,
                                                       const QPoint &viewportPos,
                                                       Qt::MouseButton button)
{
    return d->m_interactor.mouseDoubleClickEvent(documentPos, viewportPos, button);
}

QVector<QRect> DocumentContainer::leaveEvent()
{
    return d->m_interactor.leaveEvent();
}

QVector<QRect> DocumentContainer::scrollAt(const QPoint &documentPos, const QPoint &viewportPos, const QPoint &delta)
{
    return d->m_interactor.scrollAt(documentPos, viewportPos, delta);
}

QUrl DocumentContainer::linkAt(const QPoint &documentPos, const QPoint &viewportPos)
{
    return d->m_interactor.linkAt(documentPos, viewportPos);
}

QUrl DocumentContainer::imageAt(const QPoint &documentPos, const QPoint &viewportPos)
{
    return d->m_interactor.imageAt(documentPos, viewportPos);
}

QString DocumentContainer::caption() const
{
    return d->m_caption;
}

QString DocumentContainer::selectedText() const
{
    return d->m_interactor.selectedText();
}

void DocumentContainer::findText(const QString &text,
                                 QTextDocument::FindFlags flags,
                                 bool incremental,
                                 bool *wrapped,
                                 bool *success,
                                 QVector<QRect> *oldSelection,
                                 QVector<QRect> *newSelection)
{
    d->m_interactor.findText(text, flags, incremental, wrapped, success, oldSelection, newSelection);
}

void DocumentContainer::setDefaultFont(const QFont &font)
{
    d->m_defaultFont = font;
    d->m_defaultFontFamilyName = d->m_defaultFont.family().toUtf8();
    // Since font family name and size are read only once, when parsing html,
    // we need to trigger the reparse of this info.
    if (d->m_document && d->m_document->root()) {
        d->m_document->root()->refresh_styles();
        d->m_document->root()->compute_styles(true);
        d->m_needRelayout = true;
    }
}

QFont DocumentContainer::defaultFont() const
{
    return d->m_defaultFont;
}

void DocumentContainer::setResourceHandler(const DocumentContainer::ResourceHandler &handler)
{
    if (d->m_resourceManager)
        d->m_resourceManager->setResourceHandler(handler);
}

void DocumentContainer::setAllowNetworkAccess(bool allow)
{
    if (d->m_resourceManager)
        d->m_resourceManager->setAllowNetworkAccess(allow);
}

bool DocumentContainer::allowNetworkAccess() const
{
    return d->m_resourceManager ? d->m_resourceManager->allowNetworkAccess() : false;
}

void DocumentContainer::setCursorCallback(const DocumentContainer::CursorCallback &callback)
{
    d->m_interactor.setCursorCallback(callback);
}

void DocumentContainer::setLinkCallback(const DocumentContainer::LinkCallback &callback)
{
    d->m_interactor.setLinkCallback(callback);
}

void DocumentContainer::setPaletteCallback(const PaletteCallback &callback)
{
    d->m_paletteCallback = callback;
}

void DocumentContainer::setFormControlCallback(const FormControlCallback &callback)
{
    d->m_interactor.setFormControlCallback(callback);
}

void DocumentContainer::setDetailsCallback(const DetailsCallback &callback)
{
    d->m_interactor.setDetailsCallback(callback);
}

void DocumentContainer::setClipboardCallback(const ClipboardCallback &callback)
{
    d->m_interactor.setClipboardCallback(callback);
}

void DocumentContainer::setRepaintCallback(const DocumentContainer::RepaintCallback &callback)
{
    d->m_repaintCallback = callback;
    if (d->m_resourceManager)
        d->m_resourceManager->setRepaintCallback(callback);
}

static litehtml::element::ptr elementForY(int y, const litehtml::document::ptr &document)
{
    if (!document)
        return {};

    const std::function<litehtml::element::ptr(int, const litehtml::element::ptr &)> recursion =
        [&recursion](int y, const litehtml::element::ptr &element) {
            for (const auto &child : element->children()) {
                if (child->get_placement().y > y)
                    return child;
                const litehtml::element::ptr found = recursion(y, child);
                if (found)
                    return found;
            }
            return litehtml::element::ptr();
        };

    return recursion(y, document->root());
}

int DocumentContainer::withFixedElementPosition(int y, const std::function<void()> &action)
{
    const litehtml::element::ptr element = elementForY(y, d->m_document);
    action();
    if (element)
        return element->get_placement().y;
    return -1;
}

QPixmap DocumentContainerPrivate::getPixmap(const QString &imageUrl, const QString &baseUrl)
{
    return m_resourceManager ? m_resourceManager->getPixmap(imageUrl, baseUrl) : QPixmap();
}

QString DocumentContainerPrivate::serifFont() const
{
    return m_resourceManager ? m_resourceManager->serifFont() : QStringLiteral("Times New Roman");
}

QString DocumentContainerPrivate::sansSerifFont() const
{
    return m_resourceManager ? m_resourceManager->sansSerifFont() : QStringLiteral("Microsoft YaHei");
}

QString DocumentContainerPrivate::monospaceFont() const
{
    return m_resourceManager ? m_resourceManager->monospaceFont() : QStringLiteral("Courier");
}

QUrl DocumentContainerPrivate::resolveUrl(const QString &url, const QString &baseUrl) const
{
    return m_resourceManager ? m_resourceManager->resolveUrl(url, baseUrl) : QUrl(url);
}

DocumentContainerContext::DocumentContainerContext()
    : d(new DocumentContainerContextPrivate)
{}

DocumentContainerContext::~DocumentContainerContext() = default;

void DocumentContainerContext::setMasterStyleSheet(const QString &css)
{
    d->masterStyleSheet = css;
}

void DocumentContainerPrivate::draw_text(litehtml::uint_ptr hdc,
                                         const char *text,
                                         litehtml::uint_ptr hFont,
                                         litehtml::web_color color,
                                         const litehtml::position &pos)
{
    const QRect placementRect = toQRect(pos);
    SelectionSegmentInfo localSeg;
    const SelectionSegmentInfo *pSeg = m_interactor.getSelectionSegmentInfo(placementRect, localSeg);

    m_renderer.draw_text(toQPainter(hdc),
                         QString::fromUtf8(text),
                         toQFont(hFont),
                         toQColor(color),
                         placementRect,
                         pSeg,
                         m_paletteCallback ? m_paletteCallback() : QPalette());
}

void DocumentContainerPrivate::draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker &marker)
{
    QPixmap pixmap;
    if (!marker.image.empty()) {
        pixmap = getPixmap(QString::fromUtf8(marker.image.data(), int(marker.image.size())),
                           QString::fromUtf8(marker.baseurl));
    }
    m_renderer.draw_list_marker(toQPainter(hdc), marker, pixmap);
}

void DocumentContainerPrivate::draw_solid_fill(litehtml::uint_ptr hdc,
                                               const litehtml::background_layer &layer,
                                               const litehtml::web_color &color)
{
    if (layer.is_root) {
        // We still need root fill in the router because it uses m_clientRect
        auto painter = toQPainter(hdc);
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(toQColor(color));
        painter->drawRect(m_clientRect);
        painter->restore();
        return;
    }
    m_renderer.draw_solid_fill(toQPainter(hdc), layer, toQColor(color));
}

void DocumentContainerPrivate::draw_linear_gradient(litehtml::uint_ptr hdc,
                                                    const litehtml::background_layer &layer,
                                                    const litehtml::background_layer::linear_gradient &gradient)
{
    m_renderer.draw_linear_gradient(toQPainter(hdc), layer, gradient);
}

void DocumentContainerPrivate::draw_radial_gradient(litehtml::uint_ptr hdc,
                                                    const litehtml::background_layer &layer,
                                                    const litehtml::background_layer::radial_gradient &gradient)
{
    m_renderer.draw_radial_gradient(toQPainter(hdc), layer, gradient);
}

void DocumentContainerPrivate::draw_conic_gradient(litehtml::uint_ptr hdc,
                                                   const litehtml::background_layer &layer,
                                                   const litehtml::background_layer::conic_gradient &gradient)
{
    m_renderer.draw_conic_gradient(toQPainter(hdc), layer, gradient);
}

void DocumentContainerPrivate::draw_borders(litehtml::uint_ptr hdc,
                                            const litehtml::borders &borders,
                                            const litehtml::position &draw_pos,
                                            bool root)
{
    m_renderer.draw_borders(toQPainter(hdc), borders, draw_pos, root);
}

void DocumentContainerPrivate::draw_image(litehtml::uint_ptr hdc,
                                          const litehtml::background_layer &layer,
                                          const std::string &url,
                                          const std::string &base_url)
{
    if (url.empty()) return;
    QPixmap pixmap = getPixmap(QString::fromUtf8(url.data(), int(url.size())),
                               QString::fromUtf8(base_url.data(), int(base_url.size())));
    m_renderer.draw_image(toQPainter(hdc), layer, pixmap);
}

void DocumentContainerPrivate::drawSelection(QPainter *painter, const QRect &clip) const
{
    m_renderer.draw_selection(painter, m_interactor.selectionRects(), m_scrollPosition, clip, m_paletteCallback ? m_paletteCallback() : QPalette());
}
