#include "ModelCapabilityBadge.h"

#include <QPainter>

namespace ui::widget::badge {

    ModelCapabilityBadge::ModelCapabilityBadge(domain::model::ModelCapability cap,
                                               BadgeDisplayMode mode,
                                               QWidget *parent)
        : QWidget(parent), m_capability(cap), m_displayMode(mode) {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        updateVisualState();
    }

    void ModelCapabilityBadge::setCapability(domain::model::ModelCapability cap) {
        if (m_capability == cap) return;
        m_capability = cap;
        updateVisualState();
        updateGeometry();
        update();
        Q_EMIT capabilityChanged(m_capability);
    }

    void ModelCapabilityBadge::setDisplayMode(BadgeDisplayMode mode) {
        if (m_displayMode == mode) return;
        m_displayMode = mode;
        updateGeometry();
        update();
        Q_EMIT displayModeChanged(m_displayMode);
    }

    QSize ModelCapabilityBadge::sizeHint() const {
        return ModelCapabilityStyle::sizeHintFor(m_capability, m_displayMode).toSize();
    }

    QSize ModelCapabilityBadge::minimumSizeHint() const {
        return sizeHint();
    }

    void ModelCapabilityBadge::paintEvent(QPaintEvent *) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        ModelCapabilityStyle::paintBadge(&painter, rect(), m_capability, this, m_displayMode);
    }

    void ModelCapabilityBadge::onThemeUpdated() {
        updateVisualState();
        update();
    }

    void ModelCapabilityBadge::updateVisualState() {
        const auto &visual = ModelCapabilityStyle::visualRef(m_capability, this);
        setToolTip(visual.tooltip);
    }

} // namespace ui::widget::badge
