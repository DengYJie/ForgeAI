#include "BashToolWidget.h"
#include "../FlatExpander.h"

#include <QAbstractTextDocumentLayout>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <FluentQt/Design.h>

namespace ui::widget::message::blocks::tools {

BashToolWidget::BashToolWidget(QWidget* parent)
    : AbstractToolBlockWidget(parent)
{
    initBaseUi();
}

BashToolWidget::BashToolWidget(const domain::agent::ToolCall& call, QWidget* parent)
    : AbstractToolBlockWidget(call, parent)
{
    initBaseUi();
    onCallUpdated(call);
}

BashToolWidget::~BashToolWidget() = default;

QString BashToolWidget::customToolIcon(const QString& /*toolName*/) const
{
    return Typography::Icons::glyph(QStringLiteral("ic_fluent_window_console_20_regular"));
}

QString BashToolWidget::colorizeCommandHtml(const QString& cmd, bool isDark) const
{
    if (cmd.trimmed().isEmpty()) return {};
    const QString cmdColor = isDark ? QStringLiteral("#3fb950") : QStringLiteral("#1a7f37");
    const QString flagColor = isDark ? QStringLiteral("#bc8cff") : QStringLiteral("#8250df");
    const QString textColor = isDark ? QStringLiteral("#e6edf3") : QStringLiteral("#24292e");

    QStringList parts = cmd.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return {};

    QString html;
    for (int i = 0; i < parts.size(); ++i) {
        QString token = parts[i].toHtmlEscaped();
        if (i == 0) {
            html += QStringLiteral("<span style=\"color:%1; font-weight:bold;\">%2</span>").arg(cmdColor, token);
        } else if (token.startsWith(QLatin1Char('-'))) {
            html += QStringLiteral("<span style=\"color:%1;\">%2</span>").arg(flagColor, token);
        } else {
            html += QStringLiteral("<span style=\"color:%1;\">%2</span>").arg(textColor, token);
        }
        if (i < parts.size() - 1) html += QLatin1Char(' ');
    }
    return html;
}

QString BashToolWidget::colorizeOutputHtml(const QString& output, bool isDark) const
{
    if (output.trimmed().isEmpty()) return {};
    const QString numColor = isDark ? QStringLiteral("#58a6ff") : QStringLiteral("#0969da");
    const QString textColor = isDark ? QStringLiteral("#c9d1d9") : QStringLiteral("#24292e");

    QString escaped = output.toHtmlEscaped();
    static const QRegularExpression numRe(QStringLiteral("(\\b\\d+\\b|\\b\\d{2}:\\d{2}\\b)"));
    escaped.replace(numRe, QStringLiteral("<span style=\"color:%1;\">\\1</span>").arg(numColor));
    escaped.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));

    return QStringLiteral("<div style=\"color:%1; font-family:'Menlo','Monaco','Consolas',monospace; font-size:12px; line-height:1.45; white-space:pre;\">%2</div>")
        .arg(textColor, escaped);
}

QWidget* BashToolWidget::createContentWidget(QWidget* parent)
{
    return createStandardContentWidget(parent, QStringLiteral("命令"), QStringLiteral("输出"));
}

void BashToolWidget::onCallUpdated(const domain::agent::ToolCall& /*call*/)
{
    updateVisualContent();
}

void BashToolWidget::onResultUpdated(const domain::agent::ToolResult& /*result*/)
{
    updateVisualContent();
}

void BashToolWidget::updateVisualContent()
{
    const bool isDark = (effectiveTheme() == fluent::FluentElement::Dark);

    updateStandardSection1(colorizeCommandHtml(m_call.arguments, isDark));
    updateStandardSection2(colorizeOutputHtml(m_result.content, isDark));

    if (m_expander && m_expander->isExpanded()) {
        m_expander->forceUpdateContentHeight();
    }
    updateGeometry();
    emit contentHeightChanged();
}

void BashToolWidget::onThemeUpdated()
{
    AbstractToolBlockWidget::onThemeUpdated();
    updateVisualContent();
}

} // namespace ui::widget::message::blocks::tools
