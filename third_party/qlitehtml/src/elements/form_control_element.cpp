#include "form_control_element.h"

void form_control_element::addProperty(const char *name, const char *defaultValue)
{
    const char *attr_value = get_attr(name);
    if (attr_value) {
        m_style.add_property(litehtml::_id(name), attr_value, "", true);
    } else if (defaultValue) {
        m_style.add_property(litehtml::_id(name), defaultValue, "", true);
    }
}

void form_control_element::parse_attributes()
{
    const char *disabled_attr = get_attr("disabled");
    m_disabled = (disabled_attr != nullptr);

    const char *checked_attr = get_attr("checked");
    m_checked = (checked_attr != nullptr);

    m_style.add("display: inline-block; box-sizing: border-box;", "", nullptr);
    
    if (m_disabled) {
        m_style.add("cursor: not-allowed; opacity: 0.5;", "", nullptr);
    } else {
        m_style.add("cursor: pointer;", "", nullptr);
    }
}

bool form_control_element::is_replaced() const
{
    return tag() == litehtml::_id("input");
}

void form_control_element::get_content_size(litehtml::size& sz, litehtml::pixel_t max_width)
{
    Q_UNUSED(max_width)
    sz.width = 80;
    sz.height = 24;
}

bool form_control_element::on_mouse_over()
{
    if (m_disabled) return false;
    m_hovered = true;
    return true; 
}

bool form_control_element::on_mouse_leave()
{
    if (m_disabled) return false;
    m_hovered = false;
    m_pressed = false;
    return true;
}

bool form_control_element::on_lbutton_down()
{
    if (m_disabled) return false;
    m_pressed = true;
    return true;
}

bool form_control_element::on_lbutton_up(bool is_click)
{
    if (m_disabled) return false;
    m_pressed = false;
    if (is_click) {
        on_click();
    }
    return true;
}

void form_control_element::draw(litehtml::uint_ptr hdc,
                                litehtml::pixel_t x,
                                litehtml::pixel_t y,
                                const litehtml::position *clip,
                                const std::shared_ptr<litehtml::render_item> &ri)
{
    litehtml::html_tag::draw(hdc, x, y, clip, ri);

    const litehtml::position pos = ri ? ri->calc_placement(x, y) : get_placement();
    auto *painter = reinterpret_cast<QPainter *>(hdc);
    const QRectF controlRect(pos.x, pos.y, pos.width, pos.height);

    painter->save();
    if (clip) {
        painter->setClipRect(QRectF(clip->x, clip->y, clip->width, clip->height), Qt::IntersectClip);
    }
    draw_control(painter, controlRect, css(), ri);
    painter->restore();
}

