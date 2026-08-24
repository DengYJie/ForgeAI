#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QList>

#include "domain/model/ModelCapabilities.h"
#include "ModelCapabilityBadge.h"

namespace ui::widget::badge {

    /**
     * @brief 模型能力徽标集合容器
     * @details 根据给定的 ModelCapabilities 标志位集合，自动动态排列展示对应的能力胶囊徽标，支持全局切换形态
     */
    class ModelCapabilityBadgeGroup : public QWidget {
        Q_OBJECT
        Q_PROPERTY(domain::model::ModelCapabilities capabilities READ capabilities WRITE setCapabilities NOTIFY capabilitiesChanged)
        Q_PROPERTY(BadgeDisplayMode displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)

    public:
        explicit ModelCapabilityBadgeGroup(domain::model::ModelCapabilities caps = domain::model::ModelCapability::None,
                                           BadgeDisplayMode mode = BadgeDisplayMode::IconOnly,
                                           QWidget *parent = nullptr);
        ~ModelCapabilityBadgeGroup() override = default;

        domain::model::ModelCapabilities capabilities() const { return m_capabilities; }
        void setCapabilities(domain::model::ModelCapabilities caps);

        BadgeDisplayMode displayMode() const { return m_displayMode; }
        void setDisplayMode(BadgeDisplayMode mode);

    Q_SIGNALS:
        void capabilitiesChanged(domain::model::ModelCapabilities caps);
        void displayModeChanged(BadgeDisplayMode mode);

    private:
        void rebuildBadges();

        domain::model::ModelCapabilities m_capabilities;
        BadgeDisplayMode m_displayMode = BadgeDisplayMode::IconOnly;
        QHBoxLayout *m_layout = nullptr;
    };

} // namespace ui::widget::badge
