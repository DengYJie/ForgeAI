#include "ModelCapabilityBadgeGroup.h"
#include "ModelCapabilityStyle.h"

namespace ui::widget::badge {

    ModelCapabilityBadgeGroup::ModelCapabilityBadgeGroup(domain::model::ModelCapabilities caps,
                                                         BadgeDisplayMode mode,
                                                         QWidget *parent)
        : QWidget(parent), m_capabilities(caps), m_displayMode(mode) {
        m_layout = new QHBoxLayout(this);
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(6);
        setAttribute(Qt::WA_TranslucentBackground, true);
        rebuildBadges();
    }

    void ModelCapabilityBadgeGroup::setCapabilities(domain::model::ModelCapabilities caps) {
        if (m_capabilities == caps) return;
        m_capabilities = caps;
        rebuildBadges();
        Q_EMIT capabilitiesChanged(m_capabilities);
    }

    void ModelCapabilityBadgeGroup::setDisplayMode(BadgeDisplayMode mode) {
        if (m_displayMode == mode) return;
        m_displayMode = mode;
        rebuildBadges();
        Q_EMIT displayModeChanged(m_displayMode);
    }

    void ModelCapabilityBadgeGroup::rebuildBadges() {
        QLayoutItem *child = nullptr;
        while ((child = m_layout->takeAt(0)) != nullptr) {
            delete child->widget();
            delete child;
        }

        const auto &featuredCaps = ModelCapabilityStyle::featuredBadgeCapabilities();

        for (const auto cap : featuredCaps) {
            if (m_capabilities.testFlag(cap)) {
                auto *badge = new ModelCapabilityBadge(cap, m_displayMode, this);
                m_layout->addWidget(badge);
            }
        }
        m_layout->addStretch(1);
    }

} // namespace ui::widget::badge
