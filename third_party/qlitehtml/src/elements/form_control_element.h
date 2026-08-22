#pragma once

#include <litehtml.h>
#include <litehtml/render_item.h>
#include <QPainter>
#include <memory>

class form_control_element : public litehtml::html_tag
{
public:
    using litehtml::html_tag::html_tag;

    void parse_attributes() override;
    
    void draw(litehtml::uint_ptr hdc,
              litehtml::pixel_t x,
              litehtml::pixel_t y,
              const litehtml::position *clip,
              const std::shared_ptr<litehtml::render_item> &ri) override;

    bool is_replaced() const override;
    void get_content_size(litehtml::size& sz, litehtml::pixel_t max_width) override;

    bool on_mouse_over() override;
    bool on_mouse_leave() override;
    bool on_lbutton_down() override;
    bool on_lbutton_up(bool is_click = true) override;

    bool is_checked() const { return m_checked; }

protected:
    void addProperty(const char *name, const char *defaultValue);
    
    virtual void draw_control(QPainter *painter, const QRectF &rect, const litehtml::css_properties &css, const std::shared_ptr<litehtml::render_item> &ri) {}

    bool m_disabled = false;
    bool m_checked = false;
    bool m_hovered = false;
    bool m_pressed = false;
};

