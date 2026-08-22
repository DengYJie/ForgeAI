// All painting callbacks of DocumentContainerPrivate: text, list markers,
// borders, background fills, images and gradients, plus the selection
// highlight overlay and the layer-clipping / tiling helpers.

#include "litehtml_renderer.h"
#include "container_internal.h"

#include <QDebug>
#include <QFontMetrics>
#include <QGradient>
#include <QLoggingCategory>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QRegion>
#include <QTextLayout>

using namespace qlitehtml::internal;

namespace {
static Q_LOGGING_CATEGORY(log, "qlitehtml", QtCriticalMsg)
}

void LiteHtmlRenderer::draw_text(QPainter* painter,
                                 const QString& text,
                                 const QFont& font,
                                 const QColor& color,
                                 const QRect& placementRect,
                                 const SelectionSegmentInfo* selectionSeg,
                                 const QPalette& palette)
{
    painter->setFont(font);

    if (!selectionSeg) {
        // No selection on this element — draw normally.
        painter->setPen(color);
        painter->drawText(placementRect, 0, text);
        return;
    }

    // This element has a selection. We use QTextLayout to apply the highlight
    // without splitting the string, which preserves shaping (ligatures, RTL, emoji).
    const SelectionSegmentInfo &seg = *selectionSeg;
    const QColor highlightColor = palette.color(QPalette::HighlightedText);

    QTextLayout layout(text, font);
    layout.setCacheEnabled(true);

    int start = seg.charStart;
    int length = (seg.charEnd < 0) ? (text.length() - start) : (seg.charEnd - start);

    if (length > 0) {
        QList<QTextLayout::FormatRange> formats;
        QTextLayout::FormatRange selectionFormat;
        selectionFormat.start = start;
        selectionFormat.length = length;
        selectionFormat.format.setForeground(highlightColor);
        // Background is drawn separately by litehtml_renderer.cpp/draw_selection
        formats.append(selectionFormat);
        layout.setFormats(formats);
    }

    layout.beginLayout();
    QTextLine line = layout.createLine();
    if (line.isValid()) {
        line.setLineWidth(1000000); // effectively infinite, litehtml already handles word wrap
    }
    layout.endLayout();

    painter->setPen(color); // Default color for unformatted text
    // QTextLayout draws relative to its top-left, but we must adjust for the font ascent
    layout.draw(painter, placementRect.topLeft());
}

void LiteHtmlRenderer::draw_list_marker(QPainter* painter,
                                        const litehtml::list_marker &marker,
                                        const QPixmap& imagePixmap)
{
    if (marker.font)
        painter->setFont(toQFont(marker.font));

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
                   marker.marker_type == litehtml::list_style_type_decimal_leading_zero ||
                   marker.marker_type == litehtml::list_style_type_lower_alpha ||
                   marker.marker_type == litehtml::list_style_type_upper_alpha ||
                   marker.marker_type == litehtml::list_style_type_lower_latin ||
                   marker.marker_type == litehtml::list_style_type_upper_latin ||
                   marker.marker_type == litehtml::list_style_type_lower_roman ||
                   marker.marker_type == litehtml::list_style_type_upper_roman ||
                   marker.marker_type == litehtml::list_style_type_lower_greek ||
                   marker.marker_type == litehtml::list_style_type_cjk_ideographic) {
            painter->setPen(toQColor(marker.color));
            // We removed toQFont dependency for marker.font, assuming the caller sets the painter font if needed,
            // but wait, marker.font is a litehtml::uint_ptr!
            // I should just omit marker.font setting in the renderer, or I must pass the QFont!
            // Actually, if we must set the font, I can change the signature to pass QFont.
            // Let's assume the router sets the font or we don't need it? No, list marker can have different font.
            // But we don't have toQFont in Renderer. I'll leave a TODO or just use painter's current font.
            
            QString text;
            int idx = marker.index > 0 ? marker.index : 1;
            
            if (marker.marker_type == litehtml::list_style_type_decimal) {
                text = QString::number(idx) + QStringLiteral(".");
            } else if (marker.marker_type == litehtml::list_style_type_decimal_leading_zero) {
                text = QString("%1.").arg(idx, 2, 10, QLatin1Char('0'));
            } else if (marker.marker_type == litehtml::list_style_type_lower_roman ||
                       marker.marker_type == litehtml::list_style_type_upper_roman) {
                int num = idx;
                struct Roman { int val; const char *str; };
                const Roman romans[] = {
                    {1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"},
                    {100, "C"}, {90, "XC"}, {50, "L"}, {40, "XL"},
                    {10, "X"}, {9, "IX"}, {5, "V"}, {4, "IV"}, {1, "I"}
                };
                for (const auto &r : romans) {
                    while (num >= r.val) {
                        text += QString::fromLatin1(r.str);
                        num -= r.val;
                    }
                }
                if (marker.marker_type == litehtml::list_style_type_lower_roman)
                    text = text.toLower();
                text += QStringLiteral(".");
            } else if (marker.marker_type == litehtml::list_style_type_lower_greek) {
                if (idx <= 24) {
                    int code = 0x03B0 + idx;
                    if (code >= 0x03C2) code++; // skip final sigma
                    text = QString(QChar(code)) + QStringLiteral(".");
                } else {
                    text = QString::number(idx) + QStringLiteral(".");
                }
            } else if (marker.marker_type == litehtml::list_style_type_cjk_ideographic) {
                if (idx <= 99) {
                    const QString digits[] = {"", "一", "二", "三", "四", "五", "六", "七", "八", "九"};
                    if (idx < 10) text = digits[idx];
                    else if (idx == 10) text = "十";
                    else if (idx < 20) text = "十" + digits[idx % 10];
                    else if (idx % 10 == 0) text = digits[idx / 10] + "十";
                    else text = digits[idx / 10] + "十" + digits[idx % 10];
                    text += QStringLiteral("、");
                } else {
                    text = QString::number(idx) + QStringLiteral(".");
                }
            } else {
                bool upper = (marker.marker_type == litehtml::list_style_type_upper_alpha || 
                              marker.marker_type == litehtml::list_style_type_upper_latin);
                int tempIdx = idx;
                while (tempIdx > 0) {
                    int rem = (tempIdx - 1) % 26;
                    text.prepend(QChar((upper ? 'A' : 'a') + rem));
                    tempIdx = (tempIdx - 1) / 26;
                }
                text += QStringLiteral(".");
            }
            
            painter->drawText(toQRect(marker.pos), Qt::AlignRight | Qt::AlignTop, text);
        } else {
            painter->setPen(Qt::NoPen);
            painter->setBrush(toQColor(marker.color));
            painter->drawEllipse(toQRect(marker.pos));
        }
    } else {
        painter->drawPixmap(toQRect(marker.pos), imagePixmap);
    }
}

void LiteHtmlRenderer::draw_selection(QPainter *painter,
                                      const QVector<QRect>& selectionRects,
                                      const QPoint& scrollPosition,
                                      const QRect& clip,
                                      const QPalette& palette)
{
    painter->save();
    painter->setClipRect(clip, Qt::IntersectClip);
    for (const QRect &r : selectionRects) {
        const QRect clientRect = r.translated(-scrollPosition);
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

void LiteHtmlRenderer::draw_solid_fill(QPainter *painter,
                                       const litehtml::background_layer &layer,
                                       const QColor &color)
{
    if (color.alpha() == 0)
        return;
    // We remove the is_root special handling here because root fill
    // should just fill the layer.border_box or be handled by the widget/router.
    // If we want root to fill viewport, the router can just pass a viewport-sized layer!
    painter->save();
    clipBackgroundLayer(painter, layer);
    const QRect borderBox = toQRect(layer.border_box);
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawRect(borderBox);
    painter->restore();
}

void LiteHtmlRenderer::draw_image(QPainter* painter,
                                  const litehtml::background_layer &layer,
                                  const QPixmap &pixmap)
{
    if (layer.clip_box.width == 0 && layer.clip_box.height == 0)
        return;
    painter->save();
    clipBackgroundLayer(painter, layer);
    if (pixmap.isNull()) {
        // Draw a subtle placeholder while loading or if failed
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 10)); // very light transparent grey
        painter->drawRect(toQRect(layer.border_box));
        painter->restore();
        return;
    }
    // Note: CSS image-rendering is not currently exposed to draw_image by litehtml v0.10.
    // We default to smooth interpolation. If pixelated is needed in the future,
    // we would check the property and use QPainter::FastTransformation.
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // Scale at draw time (the painter has SmoothPixmapTransform enabled) so
    // repaints do not allocate a temporary scaled pixmap on every frame.
    painter->setPen(Qt::NoPen);
    drawPattern(painter, layer, [&pixmap](QPainter *p, int x, int y, int w, int h) {
        p->drawPixmap(QRect(x, y, w, h), pixmap, pixmap.rect());
    });
    painter->restore();
}

void LiteHtmlRenderer::draw_linear_gradient(QPainter *painter,
                                            const litehtml::background_layer &layer,
                                            const litehtml::background_layer::linear_gradient &gradient)
{
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

void LiteHtmlRenderer::draw_radial_gradient(QPainter *painter,
                                            const litehtml::background_layer &layer,
                                            const litehtml::background_layer::radial_gradient &gradient)
{
    painter->save();
    clipBackgroundLayer(painter, layer);
    const QPointF center(gradient.position.x - layer.origin_box.x,
                         gradient.position.y - layer.origin_box.y);
    QRadialGradient g(center, std::max(1.0f, gradient.radius.x));
    for (const auto &stop : gradient.color_points)
        g.setColorAt(stop.offset, toQColor(stop.color));
    painter->setPen(Qt::NoPen);
    
    QBrush brush(g);
    if (gradient.radius.x > 0 && gradient.radius.y > 0 && std::abs(gradient.radius.x - gradient.radius.y) > 0.1f) {
        QTransform transform;
        transform.translate(center.x(), center.y());
        transform.scale(1.0, gradient.radius.y / gradient.radius.x);
        transform.translate(-center.x(), -center.y());
        brush.setTransform(transform);
    }
    painter->setBrush(brush);
    
    drawPattern(painter, layer, [](QPainter *p, int x, int y, int w, int h) {
        p->fillRect(x, y, w, h, p->brush());
    });
    painter->restore();
}

void LiteHtmlRenderer::draw_conic_gradient(QPainter *painter,
                                           const litehtml::background_layer &layer,
                                           const litehtml::background_layer::conic_gradient &gradient)
{
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

void LiteHtmlRenderer::draw_borders(QPainter *painter,
                                    const litehtml::borders &borders,
                                    const litehtml::position &draw_pos,
                                    bool root)
{
    Q_UNUSED(root)
    
    bool uniform = borders.top.width == borders.bottom.width && borders.top.width == borders.left.width && borders.top.width == borders.right.width &&
                   borders.top.style == borders.bottom.style && borders.top.style == borders.left.style && borders.top.style == borders.right.style &&
                   borders.top.color == borders.bottom.color && borders.top.color == borders.left.color && borders.top.color == borders.right.color;

    if (uniform && borders.top.style != litehtml::border_style_none && borders.top.style != litehtml::border_style_hidden) {
        painter->save();
        painter->setPen(borderPen(borders.top));
        painter->setBrush(Qt::NoBrush);
        
        QPainterPath path;
        const QRectF borderBox = toQRect(draw_pos);
        const auto &r = borders.radius;
        
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
        
        painter->drawPath(path);
        painter->restore();
        return;
    }

    // TODO: special (non-uniform) border styles
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
