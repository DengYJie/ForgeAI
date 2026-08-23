#include "GenericToolWidget.h"
#include "../FlatExpander.h"

#include <QAbstractTextDocumentLayout>
#include <QScrollBar>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <FluentQt/Design.h>

namespace ui::widget::message::blocks::tools {

GenericToolWidget::GenericToolWidget(QWidget* parent)
    : AbstractToolBlockWidget(parent)
{
    initBaseUi();
}

GenericToolWidget::GenericToolWidget(const domain::agent::ToolCall& call, QWidget* parent)
    : AbstractToolBlockWidget(call, parent)
{
    initBaseUi();
    onCallUpdated(call);
}

GenericToolWidget::~GenericToolWidget() = default;

QWidget* GenericToolWidget::createContentWidget(QWidget* parent)
{
    return createStandardContentWidget(parent, QStringLiteral("参数"), QStringLiteral("输出"));
}

void GenericToolWidget::onCallUpdated(const domain::agent::ToolCall& /*call*/)
{
    updateVisualContent();
}

void GenericToolWidget::onResultUpdated(const domain::agent::ToolResult& /*result*/)
{
    updateVisualContent();
}

void GenericToolWidget::updateVisualContent()
{
    const bool isDark = (effectiveTheme() == fluent::FluentElement::Dark);

    QString argsHtml;
    if (!m_call.arguments.isEmpty()) {
        const QString argsColor = isDark ? QStringLiteral("#e6edf3") : QStringLiteral("#24292e");
        argsHtml = QStringLiteral("<span style=\"color:%1;\">%2</span>")
                       .arg(argsColor, m_call.arguments.toHtmlEscaped());
    }
    updateStandardSection1(argsHtml);

    QString resultHtml;
    if (!m_result.content.isEmpty()) {
        const QString textColor = isDark ? QStringLiteral("#c9d1d9") : QStringLiteral("#24292e");
        QString escaped = m_result.content.toHtmlEscaped();
        escaped.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
        resultHtml = QStringLiteral("<div style=\"color:%1; font-family:'Menlo','Monaco','Consolas',monospace; font-size:12px; line-height:1.45; white-space:pre;\">%2</div>")
            .arg(textColor, escaped);
    }
    updateStandardSection2(resultHtml);

    if (m_expander && m_expander->isExpanded()) {
        m_expander->forceUpdateContentHeight();
    }
    updateGeometry();
    emit contentHeightChanged();
}

void GenericToolWidget::onThemeUpdated()
{
    AbstractToolBlockWidget::onThemeUpdated();
    updateVisualContent();
}

} // namespace ui::widget::message::blocks::tools
