#pragma once

#include "form_control_element.h"

class button_element : public form_control_element
{
public:
    using form_control_element::form_control_element;

    void parse_attributes() override;
    
protected:
    void get_content_size(litehtml::size& sz, litehtml::pixel_t max_width) override;
    void draw_control(QPainter *painter, const QRectF &rect, const litehtml::css_properties &css, const std::shared_ptr<litehtml::render_item> &ri) override;

private:
    QString m_value;
};
