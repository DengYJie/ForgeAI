#include "FileDiffToolWidget.h"
#include "../FlatExpander.h"

#include <QAbstractTextDocumentLayout>
#include <QScrollBar>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <FluentQt/Design.h>

namespace ui::widget::message::blocks::tools {

FileDiffToolWidget::FileDiffToolWidget(QWidget* parent)
    : AbstractToolBlockWidget(parent)
{
    initBaseUi();
}

FileDiffToolWidget::FileDiffToolWidget(const domain::agent::ToolCall& call, QWidget* parent)
    : AbstractToolBlockWidget(call, parent)
{
    initBaseUi();
    onCallUpdated(call);
}

FileDiffToolWidget::~FileDiffToolWidget() = default;

QString FileDiffToolWidget::customToolIcon(const QString& /*toolName*/) const
{
    return Typography::Icons::glyph(QStringLiteral("ic_fluent_document_edit_20_regular"));
}

QString FileDiffToolWidget::customToolTitle(const QString& /*name*/, const QString& arguments) const
{
    QString path = arguments;
    path.remove('\n').remove('\"').remove('{').remove('}').remove(QStringLiteral("file_path:"));
    path = path.trimmed();
    if (path.isEmpty()) {
        return QStringLiteral("修改文件");
    }
    return QStringLiteral("修改文件: %1").arg(path);
}

QString FileDiffToolWidget::colorizeDiffHtml(const QString& diff, bool isDark) const
{
    if (diff.trimmed().isEmpty()) return {};

    const QString addColor = isDark ? QStringLiteral("#3fb950") : QStringLiteral("#1a7f37");
    const QString delColor = isDark ? QStringLiteral("#f85149") : QStringLiteral("#cf222e");
    const QString hunkColor = isDark ? QStringLiteral("#58a6ff") : QStringLiteral("#0969da");
    const QString normColor = isDark ? QStringLiteral("#c9d1d9") : QStringLiteral("#24292e");

    QStringList lines = diff.split(QLatin1Char('\n'));
    QString html;
    for (const QString& line : lines) {
        QString escaped = line.toHtmlEscaped();
        if (line.startsWith(QLatin1Char('+')) && !line.startsWith(QStringLiteral("+++"))) {
            html += QStringLiteral("<div style=\"color:%1; background:rgba(46,160,67,0.15);\">%2</div>").arg(addColor, escaped);
        } else if (line.startsWith(QLatin1Char('-')) && !line.startsWith(QStringLiteral("---"))) {
            html += QStringLiteral("<div style=\"color:%1; background:rgba(248,81,73,0.15);\">%2</div>").arg(delColor, escaped);
        } else if (line.startsWith(QStringLiteral("@@"))) {
            html += QStringLiteral("<div style=\"color:%1;\">%2</div>").arg(hunkColor, escaped);
        } else {
            html += QStringLiteral("<div style=\"color:%1;\">%2</div>").arg(normColor, escaped);
        }
    }

    return QStringLiteral("<div style=\"font-family:'Menlo','Monaco','Consolas',monospace; font-size:12px; line-height:1.45; white-space:pre;\">%1</div>").arg(html);
}

QWidget* FileDiffToolWidget::createContentWidget(QWidget* parent)
{
    return createStandardContentWidget(parent, QStringLiteral("文件"), QStringLiteral("变更"));
}

void FileDiffToolWidget::onCallUpdated(const domain::agent::ToolCall& /*call*/)
{
    updateVisualContent();
}

void FileDiffToolWidget::onResultUpdated(const domain::agent::ToolResult& /*result*/)
{
    updateVisualContent();
}

void FileDiffToolWidget::updateVisualContent()
{
    const bool isDark = (effectiveTheme() == fluent::FluentElement::Dark);

    QString pathHtml;
    if (!m_call.arguments.isEmpty()) {
        const QString pathColor = isDark ? QStringLiteral("#e6edf3") : QStringLiteral("#24292e");
        pathHtml = QStringLiteral("<span style=\"color:%1;\">%2</span>")
                       .arg(pathColor, m_call.arguments.toHtmlEscaped());
    }
    updateStandardSection1(pathHtml);

    QString diffHtml;
    if (!m_result.content.isEmpty()) {
        diffHtml = colorizeDiffHtml(m_result.content, isDark);
    }
    updateStandardSection2(diffHtml);

    if (m_expander && m_expander->isExpanded()) {
        m_expander->forceUpdateContentHeight();
    }
    updateGeometry();
    emit contentHeightChanged();
}

void FileDiffToolWidget::onThemeUpdated()
{
    AbstractToolBlockWidget::onThemeUpdated();
    updateVisualContent();
}

} // namespace ui::widget::message::blocks::tools
