#pragma once

#include <litehtml.h>
#include <QPainter>
#include <QFont>
#include <QColor>
#include <QRect>
#include <QPixmap>
#include <QPalette>
#include <QString>

namespace qlitehtml {
namespace internal {

// Selection segment info for a single text node
struct SelectionSegmentInfo {
    int charStart = 0;
    int charEnd = -1;
    int pixelStart = 0;
    int pixelEnd = -1;
};

class LiteHtmlRenderer {
public:
    LiteHtmlRenderer() = default;
    ~LiteHtmlRenderer() = default;

    void draw_text(QPainter* painter,
                   const QString& text,
                   const QFont& font,
                   const QColor& color,
                   const QRect& placementRect,
                   const SelectionSegmentInfo* selectionSeg,
                   const QPalette& palette);

    void draw_list_marker(QPainter* painter,
                          const litehtml::list_marker& marker,
                          const QPixmap& imagePixmap);

    void draw_solid_fill(QPainter* painter,
                         const litehtml::background_layer& layer,
                         const QColor& color);

    void draw_linear_gradient(QPainter* painter,
                              const litehtml::background_layer& layer,
                              const litehtml::background_layer::linear_gradient& gradient);

    void draw_radial_gradient(QPainter* painter,
                              const litehtml::background_layer& layer,
                              const litehtml::background_layer::radial_gradient& gradient);

    void draw_conic_gradient(QPainter* painter,
                             const litehtml::background_layer& layer,
                             const litehtml::background_layer::conic_gradient& gradient);

    void draw_borders(QPainter* painter,
                      const litehtml::borders& borders,
                      const litehtml::position& draw_pos,
                      bool root);

    void draw_image(QPainter* painter,
                    const litehtml::background_layer& layer,
                    const QPixmap& pixmap);

    void draw_selection(QPainter* painter,
                        const QVector<QRect>& selectionRects,
                        const QPoint& scrollPosition,
                        const QRect& clip,
                        const QPalette& palette);
};

} // namespace internal
} // namespace qlitehtml
