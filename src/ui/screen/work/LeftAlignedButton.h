#pragma once

#include <FluentQt/BasicInput.h>

namespace ui::screen::work {

class LeftAlignedButton final : public ::fluent::basicinput::Button {
    Q_OBJECT
public:
    using Button::Button;

protected:
    QRectF contentPaintRect(const QRectF& surfaceRect) const override;
};

} // namespace ui::screen::work
