#include "button_element.h"
#include <QPainter>
#include <QFontMetrics>
#include <QColor>

void button_element::parse_attributes()
{
    form_control_element::parse_attributes();

    const char *val = get_attr("value");
    if (val) {
        m_value = QString::fromUtf8(val);
    }
}

void button_element::get_content_size(litehtml::size& sz, litehtml::pixel_t max_width)
{
    form_control_element::get_content_size(sz, max_width);
    if (!m_value.isEmpty()) {
        auto font = reinterpret_cast<QFont *>(css().get_font());
        if (font) {
            QFontMetrics fm(*font);
            sz.width = fm.horizontalAdvance(m_value) + 16;
            sz.height = fm.height() + 8;
        }
    }
}

void button_element::draw_control(QPainter *painter, const QRectF &rect, const litehtml::css_properties &css, const std::shared_ptr<litehtml::render_item> &ri)
{
    // If it's an <input type="button">, it won't have children to render the text natively.
    // We must manually draw the m_value using the computed font and color.
    if (!m_value.isEmpty() && m_children.empty()) {
        auto font = reinterpret_cast<QFont *>(css.get_font());
        if (font) {
            painter->setFont(*font);
        }
        litehtml::web_color c = css.get_color();
        painter->setPen(QColor(c.red, c.green, c.blue, c.alpha));
        painter->drawText(rect, Qt::AlignCenter, m_value);
    }
}
