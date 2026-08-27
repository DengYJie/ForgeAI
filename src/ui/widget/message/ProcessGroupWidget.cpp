#include "blocks/FlatExpander.h"
#include "ProcessGroupWidget.h"
#include <QVBoxLayout>

namespace ui::widget::message {

    ProcessGroupWidget::ProcessGroupWidget(QWidget* parent)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        m_expander = new blocks::FlatExpander(QStringLiteral("已处理"), 28, this);
        m_expander->setChevronPosition(blocks::FlatExpander::ChevronPosition::InlineRight);
        m_expander->setHeaderCompact(true);
        m_expander->setExpanded(false, false);

        m_contentContainer = new QWidget(m_expander);
        m_contentLayout = new QVBoxLayout(m_contentContainer);
        m_contentLayout->setContentsMargins(4, 2, 4, 4);
        m_contentLayout->setSpacing(4);

        m_expander->setContentWidget(m_contentContainer);

        connect(m_expander, &blocks::FlatExpander::contentHeightChanged, this, [this]() {
            updateGeometry();
            emit contentHeightChanged();
            });

        connect(m_expander, &blocks::FlatExpander::expandedChanged, this, [this](bool) {
            updateGeometry();
            emit contentHeightChanged();
            });

        layout->addWidget(m_expander);
    }

    ProcessGroupWidget::~ProcessGroupWidget() = default;

    void ProcessGroupWidget::setTitle(const QString& title)
    {
        m_baseTitle = title;
        if (m_expander) {
            m_expander->setHeaderText(m_baseTitle);
        }
    }

    void ProcessGroupWidget::setDurationMs(qint64 ms)
    {
        double secs = ms / 1000.0;
        if (secs < 1.0) {
            setTitle(QStringLiteral("已处理 · %1 毫秒").arg(ms));
        }
        else {
            setTitle(QStringLiteral("已处理 · %1 秒").arg(QString::number(secs, 'f', 1)));
        }
    }

    void ProcessGroupWidget::setExpanded(bool expanded, bool animated)
    {
        if (m_expander) {
            m_expander->setExpanded(expanded, animated);
        }
    }

    bool ProcessGroupWidget::isExpanded() const
    {
        return m_expander ? m_expander->isExpanded() : false;
    }

    void ProcessGroupWidget::addProcessWidget(QWidget* widget)
    {
        m_contentLayout->addWidget(widget);

        // Connect child widget's height change signal to recalculate outer expander's size
        connect(widget, SIGNAL(contentHeightChanged()), this, SLOT(onChildContentHeightChanged()));

        if (isExpanded()) {
            onChildContentHeightChanged();
        }
    }

    void ProcessGroupWidget::onChildContentHeightChanged()
    {
        if (m_expander && m_expander->isExpanded()) {
            m_expander->forceUpdateContentHeight();
        }
        else {
            updateGeometry();
            emit contentHeightChanged();
        }
    }

    void ProcessGroupWidget::onThemeUpdated()
    {
        if (m_expander) {
            m_expander->onThemeUpdated();
        }
    }

    QSize ProcessGroupWidget::sizeHint() const
    {
        return m_expander ? m_expander->sizeHint() : QSize(200, 28);
    }

    QSize ProcessGroupWidget::minimumSizeHint() const
    {
        return m_expander ? m_expander->minimumSizeHint() : QSize(0, 28);
    }

} // namespace ui::widget::message
