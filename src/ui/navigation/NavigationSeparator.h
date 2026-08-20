#pragma once

#include "ui/navigation/NavigationWidget.h"

namespace ui::navigation {
    /**
 * @brief 导航区域分割线
 *
 * 在 Main 与 Footer 等不同语义功能区之间提供符合 Fluent 规范的 1px 视效分割。
 */
    class NavigationSeparator : public NavigationWidget {
        Q_OBJECT

    public:
        explicit NavigationSeparator(QWidget *parent = nullptr);

        ~NavigationSeparator() override = default;

        void setOrientation(Qt::Orientation orientation) override;

        QSize sizeHint() const override;

    protected:
        void paintEvent(QPaintEvent *event) override;
    };
} // namespace ui::navigation
