// Internal helpers shared between the container implementation files
// (container_qpainter.cpp, container_selection.cpp, container_painting.cpp,
// container_serializer.cpp). Not part of the public API.

#pragma once

#include "container_qpainter_p.h"

#include <QCursor>
#include <QFont>
#include <QPainter>

namespace qlitehtml::internal {

using Font = QFont;
using Context = QPainter;

inline QFont toQFont(litehtml::uint_ptr hFont)
{
    return *reinterpret_cast<Font *>(hFont);
}

inline QPainter *toQPainter(litehtml::uint_ptr hdc)
{
    return reinterpret_cast<Context *>(hdc);
}

inline QRect toQRect(litehtml::position position)
{
    return {qRound(position.x),
            qRound(position.y),
            qRound(position.width),
            qRound(position.height)};
}

inline QColor toQColor(const litehtml::web_color &color)
{
    return {color.red, color.green, color.blue, color.alpha};
}

// CSS: 400 == normal, 700 == bold.
// Qt5: 50 == normal, 75 == bold
// Qt6: == CSS
inline QFont::Weight cssWeightToQtWeight(int cssWeight)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QFont::Weight(cssWeight);
#else
    if (cssWeight <= 400)
        return QFont::Weight(cssWeight * 50 / 400);
    if (cssWeight >= 700)
        return QFont::Weight(75 + (cssWeight - 700) * 25 / 300);
    return QFont::Weight(50 + (cssWeight - 400) * 25 / 300);
#endif
}

inline QFont::Style toQFontStyle(litehtml::font_style style)
{
    switch (style) {
    case litehtml::font_style_normal:
        return QFont::StyleNormal;
    case litehtml::font_style_italic:
        return QFont::StyleItalic;
    }
    // should not happen
    return QFont::StyleNormal;
}

inline Qt::PenStyle borderPenStyle(litehtml::border_style style)
{
    switch (style) {
    case litehtml::border_style_dotted:
        return Qt::DotLine;
    case litehtml::border_style_dashed:
        return Qt::DashLine;
    case litehtml::border_style_solid:
        return Qt::SolidLine;
    default:
        break;
    }
    return Qt::SolidLine;
}

inline QPen borderPen(const litehtml::border &border)
{
    return {toQColor(border.color), qreal(border.width), borderPenStyle(border.style)};
}

inline QCursor toQCursor(const QString &c)
{
    if (c == "all-scroll")
        return {Qt::SizeAllCursor};
    if (c == "auto")
        return {Qt::ArrowCursor};
    if (c == "context-menu")
        return {Qt::WhatsThisCursor}; // ??? or ArrowCursor
    if (c == "col-resize")
        return {Qt::SplitHCursor};
    if (c == "copy")
        return {Qt::DragCopyCursor};
    if (c == "crosshair")
        return {Qt::CrossCursor};
    if (c == "default")
        return {Qt::ArrowCursor};
    if (c == "e-resize")
        return {Qt::SizeHorCursor}; // ???
    if (c == "ew-resize")
        return {Qt::SizeHorCursor};
    if (c == "grab")
        return {Qt::OpenHandCursor};
    if (c == "grabbing")
        return {Qt::ClosedHandCursor};
    if (c == "help")
        return {Qt::WhatsThisCursor};
    if (c == "move")
        return {Qt::SizeAllCursor};
    if (c == "n-resize")
        return {Qt::SizeVerCursor}; // ???
    if (c == "ne-resize")
        return {Qt::SizeBDiagCursor}; // ???
    if (c == "nesw-resize")
        return {Qt::SizeBDiagCursor};
    if (c == "ns-resize")
        return {Qt::SizeVerCursor};
    if (c == "nw-resize")
        return {Qt::SizeFDiagCursor}; // ???
    if (c == "nwse-resize")
        return {Qt::SizeFDiagCursor};
    if (c == "no-drop")
        return {Qt::ForbiddenCursor};
    if (c == "none")
        return {Qt::BlankCursor};
    if (c == "not-allowed")
        return {Qt::ForbiddenCursor};
    if (c == "pointer")
        return {Qt::PointingHandCursor};
    if (c == "progress")
        return {Qt::BusyCursor}; // or WaitCursor
    if (c == "row-resize")
        return {Qt::SplitVCursor};
    if (c == "s-resize")
        return {Qt::SizeVerCursor};
    if (c == "se-resize")
        return {Qt::SizeFDiagCursor};
    if (c == "sw-resize")
        return {Qt::SizeBDiagCursor};
    if (c == "text")
        return {Qt::IBeamCursor};
    if (c == "vertical-text")
        return {Qt::IBeamCursor}; // Qt lacks vertical I-Beam, fallback to normal I-Beam
    if (c == "url")
        return {Qt::ArrowCursor};
    if (c == "w-resize")
        return {Qt::SizeHorCursor};
    if (c == "wait")
        return {Qt::WaitCursor};
    if (c == "zoom-in")
        return {Qt::CrossCursor}; // fallback for zoom
    if (c == "zoom-out")
        return {Qt::CrossCursor}; // fallback for zoom
    if (c == "alias")
        return {Qt::DragLinkCursor};
    if (c == "cell")
        return {Qt::CrossCursor}; // cell is usually a thick cross
    
    return {Qt::ArrowCursor};
}

// Document-tree traversal and hit-testing helpers, defined in
// litehtml_interactor.cpp.
std::vector<litehtml::element::ptr> path(const litehtml::element::ptr &element);
std::pair<litehtml::element::ptr, size_t> getCommonParent(const std::vector<litehtml::element::ptr> &a,
                                                          const std::vector<litehtml::element::ptr> &b);
std::pair<Selection::Element, Selection::Element> getStartAndEnd(const Selection::Element &a,
                                                                 const Selection::Element &b);
litehtml::element::ptr firstLeaf(const litehtml::element::ptr &element,
                                 const litehtml::element::ptr &stop);
litehtml::element::ptr nextLeaf(const litehtml::element::ptr &element,
                                const litehtml::element::ptr &stop);
Selection::Element selectionDetails(const litehtml::element::ptr &element,
                                    const QString &text,
                                    const QPoint &pos);
Selection::Element deepestElementAtPoint(const litehtml::document::ptr &document,
                                         const QPoint &pos,
                                         const QPoint &viewportPos,
                                         Selection::Mode mode);
litehtml::element::ptr elementAtPoint(const litehtml::document::ptr &document,
                                      const QPoint &documentPos,
                                      const QPoint &viewportPos);
litehtml::element::ptr firstMatchingAncestor(
    litehtml::element::ptr element, const std::function<bool(const litehtml::element::ptr &)> &match);

} // namespace qlitehtml::internal
