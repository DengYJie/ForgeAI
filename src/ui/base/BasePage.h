#pragma once

#include <QWidget>
#include <QPointer>
#include <FluentQt/Design.h>
#include <FluentQt/Foundation.h>

class QResizeEvent;
class QShowEvent;

namespace ui::base {
    class BasePage : public QWidget, public fluent::FluentElement, public fluent::QMLPlus {
        Q_OBJECT

    public:
        explicit BasePage(QWidget *parent = nullptr);

        ~BasePage() override = default;

        void onThemeUpdated() override;

        /**
         * @brief 设置当前页面首选默认焦点控件（页面显示时自动 setFocus）
         */
        void setDefaultFocusWidget(QWidget *widget);

        QWidget *defaultFocusWidget() const { return m_defaultFocusWidget.data(); }

    protected:
        /**
         * @brief 响应式布局更新钩子，子类可在此根据页面可用宽度调整局部排版
         * @param availableWidth 当前页面的宽度（像素）
         */
        virtual void updateResponsiveLayout(int availableWidth);

        void resizeEvent(QResizeEvent *event) override;

        void showEvent(QShowEvent *event) override;

    private:
        QPointer<QWidget> m_defaultFocusWidget;
    };
} // namespace ui::base
