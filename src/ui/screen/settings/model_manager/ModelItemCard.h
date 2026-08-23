#pragma once
#include <QWidget>
#include <FluentQt/FluentQt.h>
#include "domain/model/Model.h"

namespace fluent::basicinput {
    class ToggleSwitch;
    class Button;
}

namespace ui::screen::settings::model_manager {

    /**
     * @brief 模型项卡片控件
     * @details 展示模型的名称、ID、上下文限制、能力标签、计费信息以及启用开关
     */
    class ModelItemCard : public QWidget, public fluent::FluentElement {
        Q_OBJECT

    public:
        explicit ModelItemCard(const domain::model::Model &model, QWidget *parent = nullptr);
        ~ModelItemCard() override = default;

        const domain::model::Model &model() const { return m_model; }
        void setModel(const domain::model::Model &model);

    Q_SIGNALS:
        void modelToggled(const QString &modelId, bool enabled);
        void modelDeleted(const QString &modelId);

    protected:
        void paintEvent(QPaintEvent *event) override;
        void enterEvent(QEnterEvent *event) override;
        void leaveEvent(QEvent *event) override;
        void onThemeUpdated() override;

    private:
        void setupUi();
        void updateTagsLayout();

        domain::model::Model m_model;
        bool m_isHovered = false;

        fluent::basicinput::ToggleSwitch *m_toggleSwitch = nullptr;
        fluent::basicinput::Button *m_deleteBtn = nullptr;
        QWidget *m_tagsContainer = nullptr;
    };

} // namespace ui::screen::settings::model_manager
