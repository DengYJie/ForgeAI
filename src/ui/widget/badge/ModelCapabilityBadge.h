#pragma once

#include <QWidget>
#include <FluentQt/FluentQt.h>

#include "domain/model/ModelCapabilities.h"
#include "ModelCapabilityStyle.h"

namespace ui::widget::badge {

    /**
     * @brief 单个大模型能力徽标胶囊控件
     * @details 支持紧凑图标型（IconOnly）、图文结合型（IconAndText）与纯文字型（TextOnly）形态切换
     */
    class ModelCapabilityBadge : public QWidget, public fluent::FluentElement {
        Q_OBJECT
        Q_PROPERTY(domain::model::ModelCapability capability READ capability WRITE setCapability NOTIFY capabilityChanged)
        Q_PROPERTY(BadgeDisplayMode displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)

    public:
        explicit ModelCapabilityBadge(domain::model::ModelCapability cap = domain::model::ModelCapability::None,
                                      BadgeDisplayMode mode = BadgeDisplayMode::IconOnly,
                                      QWidget *parent = nullptr);
        ~ModelCapabilityBadge() override = default;

        domain::model::ModelCapability capability() const { return m_capability; }
        void setCapability(domain::model::ModelCapability cap);

        BadgeDisplayMode displayMode() const { return m_displayMode; }
        void setDisplayMode(BadgeDisplayMode mode);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    Q_SIGNALS:
        void capabilityChanged(domain::model::ModelCapability cap);
        void displayModeChanged(BadgeDisplayMode mode);

    protected:
        void paintEvent(QPaintEvent *event) override;
        void onThemeUpdated() override;

    private:
        void updateVisualState();

        domain::model::ModelCapability m_capability = domain::model::ModelCapability::None;
        BadgeDisplayMode m_displayMode = BadgeDisplayMode::IconOnly;
    };

} // namespace ui::widget::badge
