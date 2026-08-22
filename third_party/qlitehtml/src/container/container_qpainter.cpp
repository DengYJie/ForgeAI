#include "container_qpainter.h"
#include "container_qpainter_p.h"
#include "container_internal.h"
#include "elements/element_checkbox.h"

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

const int kDragDistance = 5;

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

    // Built-in custom element: <input type="checkbox">.
    registerElementFactory("input",
                           [](const char *,
                              const litehtml::string_map &attributes,
                              const std::shared_ptr<litehtml::document> &doc) {
                               const auto type = attributes.find("type");
                               if (type != attributes.end() && type->second == "checkbox") {
                                   auto checkBox = std::make_shared<checkbox>(doc);
                                   checkBox->set_checked(attributes.find("checked")
                                                         != attributes.end());
                                   return std::static_pointer_cast<litehtml::element>(checkBox);
                               }
                               return std::shared_ptr<litehtml::element>{};
                           });
}

DocumentContainer::~DocumentContainer() = default;

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
    // Use Qt's logical widget DPI consistently. On Windows GetDeviceCaps(LOGPIXELSY)
    // includes the desktop scale factor and made the preview too large on HiDPI screens.
    const qreal dpi = m_paintDevice->logicalDpiY();
    return qreal(pt) * dpi / 72.0;

#if 0
// magic factor of 11/12 to account for differences to webengine/webkit
// return m_paintDevice->physicalDpiY() * pt * 11 / m_paintDevice->logicalDpiY() / 12;
#endif
}

litehtml::pixel_t DocumentContainerPrivate::get_default_font_size() const
{
    int pointSize = m_defaultFont.pointSize();
    if (pointSize <= 0) {
        int pixelSize = m_defaultFont.pixelSize();
        if (pixelSize > 0 && m_paintDevice) {
            // Convert pixel size back to point size: pt = px * 72 / DPI
            // (for [#3539](https://github.com/pbek/QOwnNotes/issues/3539))
            pointSize = qRound(pixelSize * 72.0 / m_paintDevice->logicalDpiY());
        }
    }
    if (pointSize <= 0) {
        pointSize = 16;
    }
    return pointSize;
}

const char *DocumentContainerPrivate::get_default_font_name() const
{
    return m_defaultFontFamilyName.constData();
}

void DocumentContainerPrivate::load_image(const char *src,
                                          const char *baseurl,
                                          bool redraw_on_ready)
{
    Q_UNUSED(redraw_on_ready)
    const auto qtSrc = QString::fromUtf8(src);
    const auto qtBaseUrl = QString::fromUtf8(baseurl);
    const QUrl url = resolveUrl(qtSrc, qtBaseUrl);
    if (m_pixmaps.contains(url) || m_loadingImages.contains(url))
        return;

    // Without a resource handler there is nothing to fetch; remember the
    // miss so get_image_size()/draw_image() do not retry.
    if (!m_resourceHandler) {
        m_pixmaps.insert(url, new QPixmap());
        return;
    }

    // Fetch on a worker thread; the data callback must be thread-safe. The
    // pixmap cache and layout are only touched on the main thread: decode,
    // re-layout (image size may change the flow) and repaint there.
    // The weak reference guards against the container being destroyed while
    // the fetch or the queued completion is still pending.
    m_loadingImages.insert(url);
    const std::weak_ptr<DocumentContainerPrivate> weak = shared_from_this();
    auto *task = QRunnable::create([weak, url] {
        const std::shared_ptr<DocumentContainerPrivate> self = weak.lock();
        if (!self)
            return;
        const QByteArray data = self->m_resourceHandler(url, DocumentContainer::ResourceType::Image);
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [weak, url, data] {
                const std::shared_ptr<DocumentContainerPrivate> self = weak.lock();
                if (!self)
                    return;
                self->m_loadingImages.remove(url);
                QPixmap pixmap;
                pixmap.loadFromData(data);
                if (!pixmap.isNull()) {
                    // Cost in KiB, approximated as 4 bytes per pixel.
                    const int cost = qMax(1, (pixmap.width() * pixmap.height() * 4) / 1024);
                    self->m_pixmaps.insert(url, new QPixmap(pixmap), cost);
                } else {
                    // Remember the failure so we do not refetch on every draw.
                    self->m_pixmaps.insert(url, new QPixmap());
                }
                if (!pixmap.isNull() && self->m_owner) {
                    self->m_needRelayout = true;
                    self->m_owner->render(self->m_clientRect.width(), self->m_clientRect.height());
                }
                if (self->m_repaintCallback)
                    self->m_repaintCallback();
            },
            Qt::QueuedConnection);
    });
    QThreadPool::globalInstance()->start(task);
}

void DocumentContainerPrivate::get_image_size(const char *src,
                                              const char *baseurl,
                                              litehtml::size &sz)
{
    const auto qtSrc = QString::fromUtf8(src);
    const auto qtBaseUrl = QString::fromUtf8(baseurl);
    if (qtSrc.isEmpty()) // for some reason that happens
        return;
    qDebug(log) << "get_image_size:"
                << QStringLiteral("src = \"%1\";").arg(qtSrc).toUtf8().constData()
                << QStringLiteral("base = \"%1\"").arg(qtBaseUrl).toUtf8().constData();
    const QPixmap pm = getPixmap(qtSrc, qtBaseUrl);
    sz.width = pm.width();
    sz.height = pm.height();
}

void DocumentContainerPrivate::buildIndex()
{
    m_index.elementToIndex.clear();
    m_index.indexToElement.clear();
    m_index.text.clear();

    // The leaf traversal below never visits the <body> element itself (it is
    // not a leaf), so membership is checked via ancestry instead. Note that
    // litehtml's is_ancestor() checks whether its argument is an ancestor of
    // *this* element.
    const litehtml::element::ptr body = m_document->root()->select_one("body");
    int index = 0;
    litehtml::element::ptr current = firstLeaf(m_document->root(), nullptr);
    while (current != m_document->root()) {
        m_index.elementToIndex.insert({current, index});
        if ((!body || current->is_ancestor(body))
            && current->css().get_display() != litehtml::display_none
            && current->css().get_visibility() != litehtml::visibility_hidden) {
            litehtml::string text;
            current->get_text(text);
            if (!text.empty()) {
                m_index.indexToElement.push_back({index, current});
                const QString str = QString::fromUtf8(text.data(), int(text.size()));
                m_index.text += str;
                index += str.size();
            }
        }
        m_lastIndexedElement = current;
        current = nextLeaf(current, m_document->root());
    }
}

// Incremental continuation of buildIndex(): re-indexes only the leaf
// elements added after m_lastIndexedElement (e.g. via appendHtml).
void DocumentContainerPrivate::updateIndex()
{
    if (!m_document || !m_document->root())
        return;

    litehtml::element::ptr body = m_document->root()->select_one("body");
    litehtml::element::ptr current = m_lastIndexedElement
                                          ? nextLeaf(m_lastIndexedElement, m_document->root())
                                          : firstLeaf(m_document->root(), nullptr);
    while (current != m_document->root()) {
        const int offset = int(m_index.text.size());
        m_index.elementToIndex.insert({current, offset});
        if ((!body || current->is_ancestor(body))
            && current->css().get_display() != litehtml::display_none
            && current->css().get_visibility() != litehtml::visibility_hidden) {
            litehtml::string text;
            current->get_text(text);
            if (!text.empty()) {
                m_index.indexToElement.push_back({offset, current});
                m_index.text += QString::fromUtf8(text.data(), int(text.size()));
            }
        }
        m_lastIndexedElement = current;
        current = nextLeaf(current, m_document->root());
    }
}

void DocumentContainerPrivate::updateSelection()
{
    const QString oldText = m_selection.text;
    m_selection.update();
    if (!m_clipboardCallback)
        return;

    const QString newText = m_selection.text;
    if (oldText.isEmpty() && !newText.isEmpty())
        m_clipboardCallback(true);
    else if (!oldText.isEmpty() && newText.isEmpty())
        m_clipboardCallback(false);
}

void DocumentContainerPrivate::clearSelection()
{
    const QString oldText = m_selection.text;
    m_selection = {};
    if (!m_clipboardCallback)
        return;

    if (!oldText.isEmpty())
        m_clipboardCallback(false);
}

void DocumentContainerPrivate::set_caption(const char *caption)
{
    m_caption = QString::fromUtf8(caption);
}

void DocumentContainerPrivate::set_base_url(const char *base_url)
{
    m_baseUrl = QString::fromUtf8(base_url);
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
    Q_UNUSED(el)
    if (!m_blockLinks)
        m_linkCallback(resolveUrl(QString::fromUtf8(url), m_baseUrl));
}

void DocumentContainerPrivate::on_mouse_event(const litehtml::element::ptr &el,
                                              litehtml::mouse_event event)
{
    Q_UNUSED(el)
    Q_UNUSED(event)
    // Mouse enter/leave styling is driven by QLiteHtmlWidget's own mouse
    // tracking (document::on_mouse_over/on_mouse_leave), nothing to do here.
}

void DocumentContainerPrivate::set_cursor(const char *cursor)
{
    m_cursorCallback(toQCursor(QString::fromUtf8(cursor)));
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
    // Without a resource handler we cannot fetch the stylesheet; leave text
    // empty so litehtml skips the import instead of crashing on an empty
    // std::function call.
    if (!m_resourceHandler) {
        text.clear();
        return;
    }
    const QUrl actualUrl = resolveUrl(QString::fromUtf8(url.data(), int(url.size())),
                                      QString::fromUtf8(baseurl.data(), int(baseurl.size())));
    const QString urlString = actualUrl.toString(QUrl::None);
    const int lastSlash = urlString.lastIndexOf('/');
    baseurl = urlString.left(lastSlash).toUtf8().constData();
    text = m_resourceHandler(actualUrl, DocumentContainer::ResourceType::StyleSheet).constData();
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

void DocumentContainer::setScrollPosition(const QPoint &pos)
{
    d->m_scrollPosition = pos;
}

void DocumentContainer::setDocument(const QByteArray &data, DocumentContainerContext *context)
{
    // The image cache is keyed by resolved QUrl and intentionally preserved
    // across setDocument() calls: streaming renderers (e.g. chat markdown at
    // 30 Hz) would otherwise re-decode the same images on every refresh. Call
    // clearResourceCache() when a hard resource reload is required.
    d->clearSelection();
    const std::string masterCss
        = context && !context->d->masterStyleSheet.isEmpty()
              ? context->d->masterStyleSheet.toUtf8().constData()
              : litehtml::master_css;
    d->m_document = litehtml::document::createFromString(litehtml::estring(data.constData()),
                                                         d.get(),
                                                         masterCss,
                                                         std::string());
    d->m_needRelayout = true;
    d->buildIndex();
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
    d->m_pixmaps.clear();
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
    d->updateIndex();
}

QVector<QRect> DocumentContainer::fixedBoxes() const
{
    QVector<QRect> result;
    if (!d->m_document)
        return result;
    litehtml::position::vector boxes;
    d->m_document->get_fixed_boxes(boxes);
    result.reserve(int(boxes.size()));
    for (const litehtml::position &box : boxes)
        result.append(toQRect(box));
    return result;
}

QVector<QRect> DocumentContainer::selectionRects() const
{
    return d->m_selection.selection;
}

void DocumentContainer::render(int width, int height)
{
    // litehtml lays out by width; a height-only change (scrollbar range update)
    // must not re-layout the whole document.
    const bool layoutChanged = d->m_needRelayout || width != d->m_clientRect.width();
    d->m_clientRect = {0, 0, width, height};
    if (!d->m_document)
        return;
    if (layoutChanged) {
        d->m_needRelayout = false;
        // Removed re-rendering at bestWidth because it incorrectly collapses
        // block-level elements (like blockquotes and backgrounds) to their intrinsic content width.
        d->m_document->render(width);
    }
    d->updateSelection();
}

void DocumentContainer::draw(QPainter *painter, const QRect &clip)
{
    d->m_paintDevice = painter->device();
    d->m_painter = painter;
    d->drawSelection(painter, clip);
    const QPoint pos = -d->m_scrollPosition;
    const litehtml::position clipRect(clip.x(), clip.y(), clip.width(), clip.height());
    d->m_document->draw(reinterpret_cast<litehtml::uint_ptr>(painter), pos.x(), pos.y(), &clipRect);
    d->m_painter = nullptr;
    d->m_paintDevice = nullptr;
}

int DocumentContainer::documentWidth() const
{
    return d->m_document->width();
}

int DocumentContainer::documentHeight() const
{
    return d->m_document->height();
}

int DocumentContainer::anchorY(const QString &anchorName) const
{
    litehtml::element::ptr element = d->m_document->root()->select_one(
        QStringLiteral("#%1").arg(anchorName).toUtf8().constData());
    if (!element) {
        element = d->m_document->root()->select_one(
            QStringLiteral("[name=%1]").arg(anchorName).toUtf8().constData());
    }
    if (element)
        return element->get_placement().y;
    return -1;
}

QVector<QRect> DocumentContainer::mousePressEvent(const QPoint &documentPos,
                                                  const QPoint &viewportPos,
                                                  Qt::MouseButton button,
                                                  Qt::KeyboardModifiers modifiers)
{
    if (!d->m_document || button != Qt::LeftButton)
        return {};
    QVector<QRect> redrawRects;
    // selection
    if (modifiers.testFlag(Qt::ShiftModifier) && d->m_selection.isValid()) {
        d->m_selection.selectionStartDocumentPos = documentPos;
        d->m_selection.endElem = deepestElementAtPoint(d->m_document,
                                                        documentPos,
                                                        viewportPos,
                                                        d->m_selection.mode);
        d->updateSelection();
        if (d->m_selection.isValid())
            redrawRects.append(d->m_selection.boundingRect());
    } else {
        if (d->m_selection.isValid())
            redrawRects.append(d->m_selection.boundingRect());
        d->clearSelection();
        d->m_selection.selectionStartDocumentPos = documentPos;
        d->m_selection.startElem = deepestElementAtPoint(d->m_document,
                                                          documentPos,
                                                          viewportPos,
                                                          d->m_selection.mode);
    }
    // post to litehtml
    litehtml::position::vector redrawBoxes;
    if (d->m_document->on_lbutton_down(documentPos.x(),
                                       documentPos.y(),
                                       viewportPos.x(),
                                       viewportPos.y(),
                                       redrawBoxes)) {
        for (const litehtml::position &box : redrawBoxes)
            redrawRects.append(toQRect(box));
        // Custom elements (e.g. the checkbox) may flip internal state without
        // a CSS style change, so litehtml reports no redraw box for them.
        // Repaint the clicked element's box as well.
        const litehtml::element::ptr clicked = elementAtPoint(d->m_document, documentPos, viewportPos);
        if (clicked)
            redrawRects.append(toQRect(clicked->get_placement()));
    }
    return redrawRects;
}

QVector<QRect> DocumentContainer::mouseMoveEvent(const QPoint &documentPos,
                                                 const QPoint &viewportPos)
{
    if (!d->m_document)
        return {};
    QVector<QRect> redrawRects;
    // selection
    if (d->m_selection.isSelecting
        || (!d->m_selection.selectionStartDocumentPos.isNull()
            && (d->m_selection.selectionStartDocumentPos - documentPos).manhattanLength()
                   >= kDragDistance
            && d->m_selection.startElem.element)) {
        const Selection::Element element = deepestElementAtPoint(d->m_document,
                                                                  documentPos,
                                                                  viewportPos,
                                                                  d->m_selection.mode);
        if (element.element
            && (element.element != d->m_selection.endElem.element
                || element.index != d->m_selection.endElem.index)) {
            redrawRects.append(
                d->m_selection
                    .boundingRect() /*.adjusted(-1, -1, +1, +1)*/); // redraw old selection area
            d->m_selection.endElem = element;
            d->updateSelection();
            redrawRects.append(d->m_selection.boundingRect());
        }
        d->m_selection.isSelecting = true;
    }
    litehtml::position::vector redrawBoxes;
    if (d->m_document->on_mouse_over(documentPos.x(),
                                     documentPos.y(),
                                     viewportPos.x(),
                                     viewportPos.y(),
                                     redrawBoxes)) {
        for (const litehtml::position &box : redrawBoxes)
            redrawRects.append(toQRect(box));
    }
    return redrawRects;
}

QVector<QRect> DocumentContainer::mouseReleaseEvent(const QPoint &documentPos,
                                                    const QPoint &viewportPos,
                                                    Qt::MouseButton button)
{
    if (!d->m_document || button != Qt::LeftButton)
        return {};
    QVector<QRect> redrawRects;
    // selection
    d->m_selection.isSelecting = false;
    d->m_selection.selectionStartDocumentPos = {};
    if (d->m_selection.isValid())
        d->m_blockLinks = true;
    else
        d->clearSelection();
    litehtml::position::vector redrawBoxes;
    if (d->m_document->on_lbutton_up(documentPos.x(),
                                     documentPos.y(),
                                     viewportPos.x(),
                                     viewportPos.y(),
                                     redrawBoxes)) {
        for (const litehtml::position &box : redrawBoxes)
            redrawRects.append(toQRect(box));
    }
    d->m_blockLinks = false;
    return redrawRects;
}

QVector<QRect> DocumentContainer::mouseDoubleClickEvent(const QPoint &documentPos,
                                                        const QPoint &viewportPos,
                                                        Qt::MouseButton button)
{
    if (!d->m_document || button != Qt::LeftButton)
        return {};
    QVector<QRect> redrawRects;
    d->clearSelection();
    d->m_selection.mode = Selection::Mode::Word;
    const Selection::Element element = deepestElementAtPoint(d->m_document,
                                                              documentPos,
                                                              viewportPos,
                                                              d->m_selection.mode);
    if (element.element) {
        d->m_selection.startElem = element;
        d->m_selection.endElem = d->m_selection.startElem;
        d->m_selection.isSelecting = true;
        d->updateSelection();
        if (d->m_selection.isValid())
            redrawRects.append(d->m_selection.boundingRect());
    } else {
        if (d->m_selection.isValid())
            redrawRects.append(d->m_selection.boundingRect());
        d->clearSelection();
    }
    return redrawRects;
}

QVector<QRect> DocumentContainer::leaveEvent()
{
    if (!d->m_document)
        return {};
    litehtml::position::vector redrawBoxes;
    if (d->m_document->on_mouse_leave(redrawBoxes)) {
        QVector<QRect> redrawRects;
        for (const litehtml::position &box : redrawBoxes)
            redrawRects.append(toQRect(box));
        return redrawRects;
    }
    return {};
}

QVector<QRect> DocumentContainer::scrollAt(const QPoint &documentPos, const QPoint &viewportPos, const QPoint &delta)
{
    if (!d->m_document)
        return {};
        
    std::vector<litehtml::scroll_values> scroll_values = 
        d->m_document->on_scroll(delta.x(), delta.y(), documentPos.x(), documentPos.y(), viewportPos.x(), viewportPos.y());
        
    QVector<QRect> redrawRects;
    for (const auto &val : scroll_values) {
        if (val.dx != 0 || val.dy != 0) {
            // Document coordinate scroll box mapped to a slightly expanded rect to prevent edge artifacts
            QRect rect = toQRect(val.scroll_box).adjusted(-1, -1, 1, 1);
            // Include fixed positioning and selection intersections if they exist
            // (on_scroll simply returns the scroll_box of the element that handled it)
            redrawRects.append(rect);
        }
    }
    return redrawRects;
}

QUrl DocumentContainer::linkAt(const QPoint &documentPos, const QPoint &viewportPos)
{
    const litehtml::element::ptr element
        = firstMatchingAncestor(elementAtPoint(d->m_document, documentPos, viewportPos),
                                [](const litehtml::element::ptr &candidate) {
                                    const char *href = candidate->get_attr("href");
                                    return href && href[0] != '\0';
                                });
    if (!element)
        return {};
    const char *href = element->get_attr("href");
    if (href)
        return d->resolveUrl(QString::fromUtf8(href), d->m_baseUrl);
    return {};
}

QUrl DocumentContainer::imageAt(const QPoint &documentPos, const QPoint &viewportPos)
{
    const litehtml::element::ptr element
        = firstMatchingAncestor(elementAtPoint(d->m_document, documentPos, viewportPos),
                                [](const litehtml::element::ptr &candidate) {
                                    const char *tagName = candidate->get_tagName();
                                    if (!tagName || strcmp(tagName, "img") != 0) {
                                        return false;
                                    }

                                    const char *src = candidate->get_attr("src");
                                    return src && src[0] != '\0';
                                });
    if (!element)
        return {};

    const char *src = element->get_attr("src");
    if (src) {
        return d->resolveUrl(QString::fromUtf8(src), d->m_baseUrl);
    }

    return {};
}

QString DocumentContainer::caption() const
{
    return d->m_caption;
}

QString DocumentContainer::selectedText() const
{
    return d->m_selection.text;
}

void DocumentContainer::findText(const QString &text,
                                 QTextDocument::FindFlags flags,
                                 bool incremental,
                                 bool *wrapped,
                                 bool *success,
                                 QVector<QRect> *oldSelection,
                                 QVector<QRect> *newSelection)
{
    if (success)
        *success = false;
    if (oldSelection)
        oldSelection->clear();
    if (newSelection)
        newSelection->clear();
    if (!d->m_document)
        return;
    const bool backward = flags & QTextDocument::FindBackward;
    int startIndex = backward ? -1 : 0;
    if (d->m_selection.startElem.element && d->m_selection.endElem.element) { // selection
        // poor-man's incremental search starts at beginning of selection,
        // non-incremental at end (forward search) or beginning (backward search)
        Selection::Element start;
        Selection::Element end;
        std::tie(start, end) = getStartAndEnd(d->m_selection.startElem, d->m_selection.endElem);
        Selection::Element searchStart;
        if (incremental || backward) {
            if (start.index < 0) // fully selected
                searchStart = {firstLeaf(start.element, nullptr), 0, -1};
            else
                searchStart = start;
        } else {
            if (end.index < 0) // fully selected
                searchStart = {nextLeaf(end.element, nullptr), 0, -1};
            else
                searchStart = end;
        }
        const auto findInIndex = d->m_index.elementToIndex.find(searchStart.element);
        if (findInIndex == std::end(d->m_index.elementToIndex)) {
            qWarning() << "internal error: cannot find litehmtl element in index";
            return;
        }
        startIndex = findInIndex->second + searchStart.index;
        if (backward)
            --startIndex;
    }

    const auto fillXPos = [](const Selection::Element &e) {
        litehtml::string ttext;
        e.element->get_text(ttext);
        const QString text = QString::fromUtf8(ttext.data(), int(ttext.size()));
        const QFont &font = toQFont(e.element->css().get_font());
        const QFontMetrics fm(font);
        return Selection::Element{e.element, e.index, fm.size(0, text.left(e.index)).width()};
    };

    QString term = QRegularExpression::escape(text);
    if (flags & QTextDocument::FindWholeWords)
        term = QStringLiteral("\\b%1\\b").arg(term);
    const QRegularExpression::PatternOptions patternOptions
        = (flags & QTextDocument::FindCaseSensitively) ? QRegularExpression::NoPatternOption
                                                       : QRegularExpression::CaseInsensitiveOption;
    const QRegularExpression expression(term, patternOptions);

    int foundIndex = backward ? d->m_index.text.lastIndexOf(expression, startIndex)
                              : d->m_index.text.indexOf(expression, startIndex);
    if (foundIndex < 0) { // wrap
        foundIndex = backward ? d->m_index.text.lastIndexOf(expression)
                              : d->m_index.text.indexOf(expression);
        if (wrapped && foundIndex >= 0)
            *wrapped = true;
    }
    if (foundIndex >= 0) {
        const Index::Entry startEntry = d->m_index.findElement(foundIndex);
        const Index::Entry endEntry = d->m_index.findElement(foundIndex + text.size());
        if (!startEntry.second || !endEntry.second) {
            qWarning() << "internal error: search ended up with nullptr elements";
            return;
        }
        if (oldSelection)
            *oldSelection = d->m_selection.selection;
        d->clearSelection();
        d->m_selection.startElem = fillXPos({startEntry.second, foundIndex - startEntry.first, -1});
        d->m_selection.endElem = fillXPos(
            {endEntry.second, int(foundIndex + text.size() - endEntry.first), -1});
        d->updateSelection();
        if (newSelection)
            *newSelection = d->m_selection.selection;
        if (success)
            *success = true;
        return;
    }
    return;
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
    d->m_resourceHandler = handler;
}

void DocumentContainer::setCursorCallback(const DocumentContainer::CursorCallback &callback)
{
    d->m_cursorCallback = callback;
}

void DocumentContainer::setLinkCallback(const DocumentContainer::LinkCallback &callback)
{
    d->m_linkCallback = callback;
}

void DocumentContainer::setPaletteCallback(const DocumentContainer::PaletteCallback &callback)
{
    d->m_paletteCallback = callback;
}

void DocumentContainer::setClipboardCallback(const DocumentContainer::ClipboardCallback &callback)
{
    d->m_clipboardCallback = callback;
}

void DocumentContainer::setRepaintCallback(const DocumentContainer::RepaintCallback &callback)
{
    d->m_repaintCallback = callback;
}

static litehtml::element::ptr elementForY(int y, const litehtml::document::ptr &document)
{
    if (!document)
        return {};

    const std::function<litehtml::element::ptr(int, const litehtml::element::ptr &)> recursion =
        [&recursion](int y, const litehtml::element::ptr &element) {
            const int subY = y - qRound(element->get_placement().y);
            if (subY <= 0)
                return element;
            for (const litehtml::element::ptr &child : element->children()) {
                const litehtml::element::ptr result = recursion(subY, child);
                if (result)
                    return result;
            }
            return litehtml::element::ptr{};
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
    const QUrl url = resolveUrl(imageUrl, baseUrl);
    // object() refreshes the LRU position on access.
    if (const QPixmap *pixmap = m_pixmaps.object(url))
        return *pixmap;
    qWarning(log) << "draw_background: pixmap not loaded for" << url;
    return {};
}

QString DocumentContainerPrivate::serifFont() const
{
    // TODO make configurable
    return {"Times New Roman"};
}

QString DocumentContainerPrivate::sansSerifFont() const
{
    // TODO make configurable
    return {"Arial"};
}

QString DocumentContainerPrivate::monospaceFont() const
{
    // TODO make configurable
    return {"Courier"};
}

QUrl DocumentContainerPrivate::resolveUrl(const QString &url, const QString &baseUrl) const
{
    // several cases:
    // full url: "https://foo.bar/blah.css"
    // relative path: "foo/bar.css"
    // server relative path: "/foo/bar.css"
    // net path: "//foo.bar/blah.css"
    // fragment only: "#foo-fragment"
    const QUrl qurl(url);
    if (qurl.scheme().isEmpty()) {
        if (url.startsWith('#')) // leave alone if just a fragment
            return qurl;
        const QUrl pageBaseUrl = QUrl(baseUrl.isEmpty() ? m_baseUrl : baseUrl);
        if (url.startsWith("//")) // net path
            return QUrl(pageBaseUrl.scheme() + ":" + url);
        QUrl serverUrl = QUrl(pageBaseUrl);
        serverUrl.setPath("");
        const QString actualBaseUrl = url.startsWith('/')
                                          ? serverUrl.toString(QUrl::FullyEncoded)
                                          : pageBaseUrl.toString(QUrl::FullyEncoded);
        QUrl resolvedUrl(actualBaseUrl + '/' + url);
        resolvedUrl.setPath(QDir::cleanPath(resolvedUrl.path(QUrl::FullyEncoded)));
        return resolvedUrl;
    }
    return qurl;
}

DocumentContainerContext::DocumentContainerContext()
    : d(new DocumentContainerContextPrivate)
{}

DocumentContainerContext::~DocumentContainerContext() = default;

void DocumentContainerContext::setMasterStyleSheet(const QString &css)
{
    d->masterStyleSheet = css;
}
