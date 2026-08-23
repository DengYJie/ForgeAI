#pragma once
#include <QWidget>
#include <FluentQt/FluentQt.h>
#include "domain/model/ModelProvider.h"

namespace ui::screen::settings::model_manager {

    /**
     * @brief 左侧服务商导航项控件
     * @details 展示服务商名称、模型数徽标、启停状态指示与 Fluent 选中指示条
     */
    class ProviderNavigationItem : public QWidget, public fluent::FluentElement {
        Q_OBJECT

    public:
        explicit ProviderNavigationItem(const domain::model::ModelProvider &provider, QWidget *parent = nullptr);
        ~ProviderNavigationItem() override = default;

        const domain::model::ModelProvider &provider() const { return m_provider; }
        void setProvider(const domain::model::ModelProvider &provider);

        bool isSelected() const { return m_isSelected; }
        void setSelected(bool selected);

    Q_SIGNALS:
        void clicked(const QString &providerId);

    protected:
        void paintEvent(QPaintEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void enterEvent(QEnterEvent *event) override;
        void leaveEvent(QEvent *event) override;
        void onThemeUpdated() override;

    private:
        domain::model::ModelProvider m_provider;
        bool m_isSelected = false;
        bool m_isHovered = false;
        bool m_isPressed = false;
    };

} // namespace ui::screen::settings::model_manager
