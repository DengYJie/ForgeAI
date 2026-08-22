// All painting callbacks of DocumentContainerPrivate: text, list markers,
// borders, background fills, images and gradients, plus the selection
// highlight overlay and the layer-clipping / tiling helpers.

#include "container_qpainter_p.h"
#include "container_internal.h"

#include <QDebug>
#include <QFontMetrics>
#include <QGradient>
#include <QLoggingCategory>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QRegion>

using namespace qlitehtml::internal;

namespace {
static Q_LOGGING_CATEGORY(log, "qlitehtml", QtCriticalMsg)
}

void DocumentContainerPrivate::draw_text(litehtml::uint_ptr hdc,
                                         const char *text,
                                         litehtml::uint_ptr hFont,
                                         litehtml::web_color color,
                                         const litehtml::position &pos)
{
    auto painter = toQPainter(hdc);
    const QFont font = toQFont(hFont);
    painter->setFont(font);
    const QColor normalColor = toQColor(color);

    // Look up whether this text element has a selection segment.
    // draw_text receives pos in viewport coordinates (document - scrollPosition);
    // segmentMap is keyed by the unadjusted document-coordinate placement rect.
    const QRect placementRect = toQRect(pos).translated(m_scrollPosition);
    const auto segIt = m_selection.segmentMap.constFind(placementRect);

    if (segIt == m_selection.segmentMap.constEnd() || !m_paletteCallback) {
        // No selection on this element — draw normally.
        painter->setPen(normalColor);
        painter->drawText(toQRect(pos), 0, QString::fromUtf8(text));
        return;
    }

    // This element has a selection. Split into up to three segments:
    //   [0, charStart)        — pre-selection  (normal color)
    //   [charStart, charEnd)  — selected        (highlighted color)
    //   [charEnd, end)        — post-selection  (normal color)
    const Selection::SegmentInfo &seg = segIt.value();
    const QString str = QString::fromUtf8(text);
    const QColor highlightColor = m_paletteCallback().color(QPalette::HighlightedText);
    const QRect drawRect = toQRect(pos);
    const QFontMetrics fm(font);

    // Helper: draw a substring starting at a given pixel x-offset within drawRect.
    const auto drawSegment = [&](const QString &sub, int xOffset, const QColor &col) {
        if (sub.isEmpty())
            return;
        QRect r = drawRect;
        r.setLeft(drawRect.left() + xOffset);
        painter->setPen(col);
        painter->drawText(r, 0, sub);
    };

    // Pre-selection segment
    if (seg.charStart > 0) {
        drawSegment(str.left(seg.charStart), 0, normalColor);
    }

    // Selected segment
    const QString selectedStr = (seg.charEnd < 0)
                                    ? str.mid(seg.charStart)
                                    : str.mid(seg.charStart, seg.charEnd - seg.charStart);
    drawSegment(selectedStr, seg.pixelStart, highlightColor);

    // Post-selection segment
    if (seg.charEnd >= 0 && seg.charEnd < str.size()) {
        drawSegment(str.mid(seg.charEnd), seg.pixelEnd, normalColor);
    }
}

void DocumentContainerPrivate::draw_list_marker(litehtml::uint_ptr hdc,
                                                const litehtml::list_marker &marker)
{
    auto painter = toQPainter(hdc);
    if (marker.image.empty()) {
        if (marker.marker_type == litehtml::list_style_type_square) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(toQColor(marker.color));
            painter->drawRect(toQRect(marker.pos));
        } else if (marker.marker_type == litehtml::list_style_type_disc) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(toQColor(marker.color));
            painter->drawEllipse(toQRect(marker.pos));
        } else if (marker.marker_type == litehtml::list_style_type_circle) {
            painter->setPen(toQColor(marker.color));
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(toQRect(marker.pos));
        } else if (marker.marker_type == litehtml::list_style_type_decimal ||
                   marker.marker_type == litehtml::list_style_type_lower_alpha ||
                   marker.marker_type == litehtml::list_style_type_upper_alpha) {
            painter->setPen(toQColor(marker.color));
            if (marker.font)
                painter->setFont(toQFont(marker.font));
                
            QString text;
            if (marker.marker_type == litehtml::list_style_type_decimal) {
                text = QString::number(marker.index) + QStringLiteral(".");
            } else {
                int idx = marker.index > 0 ? marker.index : 1;
                bool upper = (marker.marker_type == litehtml::list_style_type_upper_alpha);
                while (idx > 0) {
                    int rem = (idx - 1) % 26;
                    text.prepend(QChar((upper ? 'A' : 'a') + rem));
                    idx = (idx - 1) / 26;
                }
                text += QStringLiteral(".");
            }
            
            // litehtml's marker.pos width is usually small, so we align right
            painter->drawText(toQRect(marker.pos), Qt::AlignRight | Qt::AlignTop, text);
        } else {
            // TODO: Implement other list types (roman, alpha, etc.)
            // For now, fallback to bullet
            painter->setPen(Qt::NoPen);
            painter->setBrush(toQColor(marker.color));
            painter->drawEllipse(toQRect(marker.pos));
            qWarning(log) << "list marker of type" << marker.marker_type << "not fully supported, falling back to bullet";
        }
    } else {
        const QPixmap pixmap = getPixmap(
            QString::fromUtf8(marker.image.data(), int(marker.image.size())),
            QString::fromUtf8(marker.baseurl));
        painter->drawPixmap(toQRect(marker.pos), pixmap);
    }
}

void DocumentContainerPrivate::drawSelection(QPainter *painter, const QRect &clip) const
{
    painter->save();
    painter->setClipRect(clip, Qt::IntersectClip);
    for (const QRect &r : m_selection.selection) {
        const QRect clientRect = r.translated(-m_scrollPosition);
        const QPalette palette = m_paletteCallback();
        painter->fillRect(clientRect, palette.brush(QPalette::Highlight));
    }
    painter->restore();
}

// Clips the painter to the layer's border box with its border radius, so
// background fills/images/gradients stay inside the rounded element.
static void clipBackgroundLayer(QPainter *painter, const litehtml::background_layer &layer)
{
    if (layer.is_root)
        return;
        
    painter->setClipRect(toQRect(layer.clip_box), Qt::IntersectClip);
    
    const QRectF borderBox(layer.border_box.x, layer.border_box.y, layer.border_box.width, layer.border_box.height);
    const litehtml::border_radiuses &r = layer.border_radius;
    
    QPainterPath path;
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
    
    painter->setClipPath(path, Qt::IntersectClip);
}

// Tiles the given fill callback across the layer's clip_box, one origin_box-sized
// cell at a time, honoring layer.repeat. Mirrors litehtml's cairo container.
static void drawPattern(
    QPainter *painter,
    const litehtml::background_layer &layer,
    const std::function<void(QPainter *, int, int, int, int)> &fill)
{
    const QRect origin = toQRect(layer.origin_box);
    const QRect clip = toQRect(layer.clip_box);
    int startX = origin.x();
    int numX = 1;
    int startY = origin.y();
    int numY = 1;
    if (layer.repeat == litehtml::background_repeat_repeat_x
        || layer.repeat == litehtml::background_repeat_repeat) {
        if (origin.left() > clip.left()) {
            int numLeft = (origin.left() - clip.left()) / std::max(1, origin.width());
            if (origin.left() - numLeft * origin.width() > clip.left())
                ++numLeft;
            startX = origin.left() - numLeft * origin.width();
            numX += numLeft;
        }
        if (origin.right() < clip.right()) {
            int numRight = (clip.right() - origin.right()) / std::max(1, origin.width());
            if (origin.left() + numRight * origin.width() < clip.right())
                ++numRight;
            numX += numRight;
        }
    }
    if (layer.repeat == litehtml::background_repeat_repeat_y
        || layer.repeat == litehtml::background_repeat_repeat) {
        if (origin.top() > clip.top()) {
            int numTop = (origin.top() - clip.top()) / std::max(1, origin.height());
            if (origin.top() - numTop * origin.height() > clip.top())
                ++numTop;
            startY = origin.top() - numTop * origin.height();
            numY += numTop;
        }
        if (origin.bottom() < clip.bottom()) {
            int numBottom = (clip.bottom() - origin.bottom()) / std::max(1, origin.height());
            if (origin.bottom() + numBottom * origin.height() < clip.bottom())
                ++numBottom;
            numY += numBottom;
        }
    }
    for (int ix = 0; ix < numX; ++ix) {
        for (int iy = 0; iy < numY; ++iy)
            fill(painter,
                 startX + ix * origin.width(),
                 startY + iy * origin.height(),
                 origin.width(),
                 origin.height());
    }
}

void DocumentContainerPrivate::draw_solid_fill(litehtml::uint_ptr hdc,
                                               const litehtml::background_layer &layer,
                                               const litehtml::web_color &color)
{
    if (color == litehtml::web_color::transparent)
        return;
    auto painter = toQPainter(hdc);
    if (layer.is_root) {
        // TODO ?
        return;
    }
    painter->save();
    clipBackgroundLayer(painter, layer);
    const QRect borderBox = toQRect(layer.border_box);
    painter->setPen(Qt::NoPen);
    painter->setBrush(toQColor(color));
    painter->drawRect(borderBox);
    drawSelection(painter, borderBox);
    painter->restore();
}

void DocumentContainerPrivate::draw_image(litehtml::uint_ptr hdc,
                                          const litehtml::background_layer &layer,
                                          const std::string &url,
                                          const std::string &base_url)
{
    if (url.empty() || (layer.clip_box.width == 0 && layer.clip_box.height == 0))
        return;
    auto painter = toQPainter(hdc);
    painter->save();
    clipBackgroundLayer(painter, layer);
    const QPixmap pixmap = getPixmap(QString::fromUtf8(url.data(), int(url.size())),
                                     QString::fromUtf8(base_url.data(), int(base_url.size())));
    if (pixmap.isNull())
        qWarning(log) << "draw_image: pixmap not loaded for" << QString::fromUtf8(url.data(), int(url.size()));
    // Scale at draw time (the painter has SmoothPixmapTransform enabled) so
    // repaints do not allocate a temporary scaled pixmap on every frame.
    painter->setPen(Qt::NoPen);
    drawPattern(painter, layer, [&pixmap](QPainter *p, int x, int y, int w, int h) {
        p->drawPixmap(QRect(x, y, w, h), pixmap, pixmap.rect());
    });
    painter->restore();
}

void DocumentContainerPrivate::draw_linear_gradient(litehtml::uint_ptr hdc,
                                                    const litehtml::background_layer &layer,
                                                    const litehtml::background_layer::linear_gradient &gradient)
{
    auto painter = toQPainter(hdc);
    painter->save();
    clipBackgroundLayer(painter, layer);
    // Gradient coordinates are relative to the origin box.
    QLinearGradient g(gradient.start.x - layer.origin_box.x,
                      gradient.start.y - layer.origin_box.y,
                      gradient.end.x - layer.origin_box.x,
                      gradient.end.y - layer.origin_box.y);
    for (const auto &stop : gradient.color_points)
        g.setColorAt(stop.offset, toQColor(stop.color));
    painter->setPen(Qt::NoPen);
    painter->setBrush(g);
    drawPattern(painter, layer, [](QPainter *p, int x, int y, int w, int h) {
        p->fillRect(x, y, w, h, p->brush());
    });
    painter->restore();
}

void DocumentContainerPrivate::draw_radial_gradient(litehtml::uint_ptr hdc,
                                                    const litehtml::background_layer &layer,
                                                    const litehtml::background_layer::radial_gradient &gradient)
{
    auto painter = toQPainter(hdc);
    painter->save();
    clipBackgroundLayer(painter, layer);
    const QPointF center(gradient.position.x - layer.origin_box.x,
                         gradient.position.y - layer.origin_box.y);
    QRadialGradient g(center, std::max(1.0f, gradient.radius.x));
    for (const auto &stop : gradient.color_points)
        g.setColorAt(stop.offset, toQColor(stop.color));
    painter->setPen(Qt::NoPen);
    painter->setBrush(g);
    drawPattern(painter, layer, [](QPainter *p, int x, int y, int w, int h) {
        p->fillRect(x, y, w, h, p->brush());
    });
    painter->restore();
}

void DocumentContainerPrivate::draw_conic_gradient(litehtml::uint_ptr hdc,
                                                   const litehtml::background_layer &layer,
                                                   const litehtml::background_layer::conic_gradient &gradient)
{
    auto painter = toQPainter(hdc);
    painter->save();
    clipBackgroundLayer(painter, layer);
    const QPointF center(gradient.position.x - layer.origin_box.x,
                         gradient.position.y - layer.origin_box.y);
    // CSS conic gradient: 0deg points up, angles run clockwise. Qt measures
    // counter-clockwise from 3 o'clock, so negate and rotate by 90 degrees.
    QConicalGradient g(center, 90.0 - gradient.angle);
    for (const auto &stop : gradient.color_points)
        g.setColorAt(stop.offset, toQColor(stop.color));
    painter->setPen(Qt::NoPen);
    painter->setBrush(g);
    drawPattern(painter, layer, [](QPainter *p, int x, int y, int w, int h) {
        p->fillRect(x, y, w, h, p->brush());
    });
    painter->restore();
}

void DocumentContainerPrivate::draw_borders(litehtml::uint_ptr hdc,
                                            const litehtml::borders &borders,
                                            const litehtml::position &draw_pos,
                                            bool root)
{
    Q_UNUSED(root)
    // TODO: special border styles
    auto painter = toQPainter(hdc);
    if (borders.top.style != litehtml::border_style_none
        && borders.top.style != litehtml::border_style_hidden) {
        painter->setPen(borderPen(borders.top));
        painter->drawLine(draw_pos.left() + borders.radius.top_left_x,
                          draw_pos.top(),
                          draw_pos.right() - borders.radius.top_right_x,
                          draw_pos.top());
        painter->drawArc(draw_pos.left(),
                         draw_pos.top(),
                         2 * borders.radius.top_left_x,
                         2 * borders.radius.top_left_y,
                         90 * 16,
                         90 * 16);
        painter->drawArc(draw_pos.right() - 2 * borders.radius.top_right_x,
                         draw_pos.top(),
                         2 * borders.radius.top_right_x,
                         2 * borders.radius.top_right_y,
                         0,
                         90 * 16);
    }
    if (borders.bottom.style != litehtml::border_style_none
        && borders.bottom.style != litehtml::border_style_hidden) {
        painter->setPen(borderPen(borders.bottom));
        painter->drawLine(draw_pos.left() + borders.radius.bottom_left_x,
                          draw_pos.bottom(),
                          draw_pos.right() - borders.radius.bottom_right_x,
                          draw_pos.bottom());
        painter->drawArc(draw_pos.left(),
                         draw_pos.bottom() - 2 * borders.radius.bottom_left_y,
                         2 * borders.radius.bottom_left_x,
                         2 * borders.radius.bottom_left_y,
                         180 * 16,
                         90 * 16);
        painter->drawArc(draw_pos.right() - 2 * borders.radius.bottom_right_x,
                         draw_pos.bottom() - 2 * borders.radius.bottom_right_y,
                         2 * borders.radius.bottom_right_x,
                         2 * borders.radius.bottom_right_y,
                         270 * 16,
                         90 * 16);
    }
    if (borders.left.style != litehtml::border_style_none
        && borders.left.style != litehtml::border_style_hidden) {
        painter->setPen(borderPen(borders.left));
        painter->drawLine(draw_pos.left(),
                          draw_pos.top() + borders.radius.top_left_y,
                          draw_pos.left(),
                          draw_pos.bottom() - borders.radius.bottom_left_y);
    }
    if (borders.right.style != litehtml::border_style_none
        && borders.right.style != litehtml::border_style_hidden) {
        painter->setPen(borderPen(borders.right));
        painter->drawLine(draw_pos.right(),
                          draw_pos.top() + borders.radius.top_right_y,
                          draw_pos.right(),
                          draw_pos.bottom() - borders.radius.bottom_right_y);
    }
}
