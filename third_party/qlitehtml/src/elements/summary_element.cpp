#include "summary_element.h"
#include "details_element.h"

#include <litehtml/render_item.h>

#include <QPainter>
#include <QPainterPath>

void summary_element::draw(litehtml::uint_ptr hdc,
                           litehtml::pixel_t x,
                           litehtml::pixel_t y,
                           const litehtml::position *clip,
                           const std::shared_ptr<litehtml::render_item> &ri)
{
    // Draw content and children natively
    html_tag::draw(hdc, x, y, clip, ri);

    // Only draw disclosure triangle if parent is details_element and we are the first summary child
    auto parent_el = parent();
    if (!parent_el || parent_el->tag() != litehtml::_id("details")) {
        return;
    }

    auto details = std::dynamic_pointer_cast<details_element>(parent_el);
    if (!details) {
        return;
    }

    for (const auto &child : details->children()) {
        if (child->tag() == litehtml::_id("summary")) {
            if (child.get() != this) {
                return; // Not the first summary child
            }
            break;
        }
    }

    litehtml::position pos = ri ? ri->calc_placement(x, y) : get_placement();

    float fontSize = (float)css().get_font_size();
    if (fontSize <= 0) fontSize = 16.0f;
    float triSize = std::max(6.0f, fontSize * 0.45f);

    float lineHeight = (float)css().line_height().computed_value;
    if (lineHeight <= 0) lineHeight = fontSize * 1.2f;

    float topOffset = ri ? (float)ri->content_offset_top() : 0.0f;
    float leftPadding = (float)css().get_padding().left.val();
    if (leftPadding <= 0) leftPadding = 18.0f;
    float triX = (float)pos.x - leftPadding + (leftPadding - triSize) / 2.0f;
    float triY = (float)pos.y + topOffset + (lineHeight - triSize) / 2.0f;

    litehtml::web_color c = css().get_color();
    QColor markerColor(c.red, c.green, c.blue, c.alpha);

    // Respect standard Web CSS: list-style: none suppresses the disclosure marker
    if (css().get_list_style_type() == litehtml::list_style_type_none) {
        return;
    }

    auto *painter = reinterpret_cast<QPainter *>(hdc);

    QPainterPath path;
    if (details->is_open()) {
        // Downward pointing triangle (disclosure-open)
        path.moveTo(triX, triY + 1.0f);
        path.lineTo(triX + triSize, triY + 1.0f);
        path.lineTo(triX + triSize * 0.5f, triY + 1.0f + triSize * 0.866f);
        path.closeSubpath();
    } else {
        // Rightward pointing triangle (disclosure-closed)
        path.moveTo(triX, triY);
        path.lineTo(triX + triSize * 0.866f, triY + triSize * 0.5f);
        path.lineTo(triX, triY + triSize);
        path.closeSubpath();
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(markerColor);
    painter->drawPath(path);
    painter->restore();
}
