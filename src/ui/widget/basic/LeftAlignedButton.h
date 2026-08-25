#pragma once

#include <FluentQt/BasicInput.h>

namespace ui::widget::basic {

class LeftAlignedButton : public ::fluent::basicinput::Button {
    Q_OBJECT
public:
    using Button::Button;

protected:
    QRectF contentPaintRect(const QRectF& surfaceRect) const override;
};

} // namespace ui::widget::basic
