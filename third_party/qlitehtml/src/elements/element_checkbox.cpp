#include "element_checkbox.h"

#include <QApplication>
#include <QCheckBox>
#include <QDebug>
#include <QLoggingCategory>
#include <QPainter>
#include <QStyleOptionButton>
#include <QStylePainter>

static Q_LOGGING_CATEGORY(log, "qlitehtml_checkbox", QtWarningMsg)

    void checkbox::addProperty(const char *name, const char *defaultValue)
{
    const char *attr_value = get_attr(name);
    if (attr_value) {
        qDebug(log) << name << attr_value;
        m_style.add_property(litehtml::_id(name), attr_value, "", true);
    } else {
        qDebug(log) << "using default " << name << 10;
        m_style.add_property(litehtml::_id(name), defaultValue, "", true);
    }
}

bool checkbox::on_lbutton_down()
{
    m_checked = !m_checked;
    // Return true so litehtml reports the click; the container repaints the
    // clicked element's box (custom state changes are not style changes).
    return true;
}

void checkbox::parse_attributes()
{
    addProperty("width", "12");
    addProperty("height", "12");
    addProperty("margin-right", "5");
}

void checkbox::draw(litehtml::uint_ptr hdc,
                    litehtml::pixel_t x,
                    litehtml::pixel_t y,
                    const litehtml::position *clip,
                    const std::shared_ptr<litehtml::render_item> &ri)
{
    Q_UNUSED(clip)
    Q_UNUSED(ri)
    Q_UNUSED(x)
    Q_UNUSED(y)
    // v0.10 already passes the render item's absolute position as (x, y);
    // get_placement() carries the rendered box size.
    const litehtml::position pos = get_placement();

    //     qDebug(log) << "draw checkbox " << QRect(pos.x, pos.y, pos.width, pos.height);

    auto palette = qApp->palette();

    auto *paint = reinterpret_cast<QPainter *>(hdc);
    const QRectF checkboxRect(pos.x, pos.y, pos.width, pos.height);

    auto savedBrush = paint->brush();
    auto savedPen = paint->pen();

    paint->setPen(palette.windowText().color());
    paint->setBrush(palette.base());

    paint->drawRoundedRect(checkboxRect, 1., 1.);

    if (m_checked) {
        QRect check = checkboxRect.toRect().adjusted(2, 2, -2, -2);
        paint->setBrush(palette.windowText().color());
        paint->drawRect(check);
    }

    // restore
    paint->setPen(savedPen);
    paint->setBrush(savedBrush);
}
