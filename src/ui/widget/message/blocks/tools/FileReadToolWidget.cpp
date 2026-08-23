#include "FileReadToolWidget.h"
#include "../FlatExpander.h"

#include <QAbstractTextDocumentLayout>
#include <QScrollBar>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <FluentQt/Design.h>

namespace ui::widget::message::blocks::tools {

FileReadToolWidget::FileReadToolWidget(QWidget* parent)
    : AbstractToolBlockWidget(parent)
{
    initBaseUi();
}

FileReadToolWidget::FileReadToolWidget(const domain::agent::ToolCall& call, QWidget* parent)
    : AbstractToolBlockWidget(call, parent)
{
    initBaseUi();
    onCallUpdated(call);
}

FileReadToolWidget::~FileReadToolWidget() = default;

QString FileReadToolWidget::customToolIcon(const QString& /*toolName*/) const
{
    return Typography::Icons::glyph(QStringLiteral("ic_fluent_document_20_regular"));
}

QString FileReadToolWidget::customToolTitle(const QString& /*name*/, const QString& arguments) const
{
    QString path = arguments;
    path.remove('\n').remove('\"').remove('{').remove('}').remove(QStringLiteral("file_path:"));
    path = path.trimmed();
    if (path.isEmpty()) {
        return QStringLiteral("读取文件");
    }
    return QStringLiteral("读取文件: %1").arg(path);
}

QWidget* FileReadToolWidget::createContentWidget(QWidget* parent)
{
    return createStandardContentWidget(parent, QStringLiteral("文件"), QStringLiteral("内容"));
}

void FileReadToolWidget::onCallUpdated(const domain::agent::ToolCall& /*call*/)
{
    updateVisualContent();
}

void FileReadToolWidget::onResultUpdated(const domain::agent::ToolResult& /*result*/)
{
    updateVisualContent();
}

void FileReadToolWidget::updateVisualContent()
{
    const bool isDark = (effectiveTheme() == fluent::FluentElement::Dark);

    // Simple formatting for file path
    QString pathHtml;
    if (!m_call.arguments.isEmpty()) {
        const QString pathColor = isDark ? QStringLiteral("#e6edf3") : QStringLiteral("#24292e");
        pathHtml = QStringLiteral("<span style=\"color:%1;\">%2</span>")
                       .arg(pathColor, m_call.arguments.toHtmlEscaped());
    }
    updateStandardSection1(pathHtml);

    // Simple formatting for content
    QString contentHtml;
    if (!m_result.content.isEmpty()) {
        const QString textColor = isDark ? QStringLiteral("#c9d1d9") : QStringLiteral("#24292e");
        QString escaped = m_result.content.toHtmlEscaped();
        escaped.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
        contentHtml = QStringLiteral("<div style=\"color:%1; font-family:'Menlo','Monaco','Consolas',monospace; font-size:12px; line-height:1.45; white-space:pre;\">%2</div>")
            .arg(textColor, escaped);
    }
    updateStandardSection2(contentHtml);

    if (m_expander && m_expander->isExpanded()) {
        m_expander->forceUpdateContentHeight();
    }
    updateGeometry();
    emit contentHeightChanged();
}

void FileReadToolWidget::onThemeUpdated()
{
    AbstractToolBlockWidget::onThemeUpdated();
    updateVisualContent();
}

} // namespace ui::widget::message::blocks::tools
