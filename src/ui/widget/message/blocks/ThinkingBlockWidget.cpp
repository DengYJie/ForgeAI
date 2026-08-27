#include "ThinkingBlockWidget.h"
#include "FlatExpander.h"
#include "ui/widget/MarkdownView.h"

#include <QVBoxLayout>

namespace ui::widget::message::blocks {

ThinkingBlockWidget::ThinkingBlockWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

ThinkingBlockWidget::~ThinkingBlockWidget() = default;

void ThinkingBlockWidget::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_expander = new FlatExpander(QStringLiteral("思考过程"), 26, this);
    m_expander->setLeadingIcon(Typography::Icons::glyph(QStringLiteral("ic_fluent_brain_circuit_20_regular")));
    m_expander->setChevronPosition(FlatExpander::ChevronPosition::InlineRight);
    m_expander->setHeaderCompact(true);
    m_expander->setExpanded(false, false);

    auto* container = new QWidget(m_expander);
    auto* containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(8, 2, 4, 4);
    containerLayout->setSpacing(0);

    m_markdownView = new ui::widget::MarkdownView(container);
    m_markdownView->setTransparentBackground(true);
    m_markdownView->setAutoFitHeight(true);
    m_markdownView->setMaximumHeight(m_maxHeight);
    m_markdownView->setAllowNetworkAccess(true);

    connect(m_markdownView, &ui::widget::MarkdownView::autoFitHeightChanged, this, [this](int) {
        if (m_expander) {
            m_expander->forceUpdateContentHeight();
        }
        updateGeometry();
        emit contentHeightChanged();
    });

    containerLayout->addWidget(m_markdownView);
    m_expander->setContentWidget(container);

    connect(m_expander, &FlatExpander::contentHeightChanged, this, [this]() {
        updateGeometry();
        emit contentHeightChanged();
    });

    connect(m_expander, &FlatExpander::expandedChanged, this, [this](bool) {
        updateGeometry();
        emit contentHeightChanged();
    });

    layout->addWidget(m_expander);
    updateTitle();
}

    void ThinkingBlockWidget::updateTitle()
    {
        if (m_durationMs > 0) {
            double secs = m_durationMs / 1000.0;
            m_expander->setHeaderText(QStringLiteral("思考过程 (%1s)").arg(QString::number(secs, 'f', 1)));
        }
        else {
            m_expander->setHeaderText(QStringLiteral("思考过程"));
        }
    }

    void ThinkingBlockWidget::setThought(const QString& thought)
    {
        if (m_thoughtText == thought) return;

        if (m_markdownView && thought.startsWith(m_thoughtText)) {
            if (!m_markdownView->isStreaming()) {
                m_markdownView->beginStream();
            }
            QString chunk = thought.mid(m_thoughtText.length());
            m_thoughtText = thought;
            m_markdownView->appendMarkdown(chunk);
            return;
        }

        m_thoughtText = thought;
        m_markdownView->setMarkdown(thought);
    }

    void ThinkingBlockWidget::appendThought(const QString& chunk)
    {
        m_thoughtText += chunk;
        m_markdownView->appendMarkdown(chunk);
    }

    void ThinkingBlockWidget::setDurationMs(qint64 ms)
    {
        m_durationMs = ms;
        updateTitle();
    }

    void ThinkingBlockWidget::setMaxHeight(int maxH)
    {
        m_maxHeight = maxH;
        if (m_markdownView) {
            m_markdownView->setMaximumHeight(maxH);
        }
        if (m_expander) {
            m_expander->forceUpdateContentHeight();
        }
        updateGeometry();
        emit contentHeightChanged();
    }

    void ThinkingBlockWidget::setExpanded(bool expanded, bool animated)
    {
        if (m_expander) {
            m_expander->setExpanded(expanded, animated);
        }
    }

    bool ThinkingBlockWidget::isExpanded() const
    {
        return m_expander ? m_expander->isExpanded() : false;
    }

    void ThinkingBlockWidget::beginStream()
    {
        m_thoughtText.clear();
        m_markdownView->beginStream();
    }

    void ThinkingBlockWidget::finishStream()
    {
        m_markdownView->finishStream();
    }

    bool ThinkingBlockWidget::isStreaming() const
    {
        return m_markdownView ? m_markdownView->isStreaming() : false;
    }

    void ThinkingBlockWidget::onThemeUpdated()
    {
        if (m_markdownView) {
            m_markdownView->onThemeUpdated();
        }
        if (m_expander) {
            m_expander->onThemeUpdated();
        }
    }

} // namespace ui::widget::message::blocks
