#include "MarkdownStyle.h"

namespace ui::widget {
namespace {

QString cssColor(const QColor &color)
{
    if (!color.isValid() || color == Qt::transparent || color.alpha() == 0)
        return QStringLiteral("transparent");

    if (color.alpha() == 255)
        return color.name(QColor::HexRgb);

    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(QString::number(color.alphaF(), 'f', 3));
}

} // namespace

QString MarkdownStyleSheet::build() const
{
    QString css = QStringLiteral(R"(
        html, body { width: 100%%; margin: 0; padding: 0; background: %1; color: %2; }
        .markdown-body { box-sizing: border-box; display: block; width: 100%%; max-width: none; padding: %3px %4px; font-family: '%5'; font-size: %6px; line-height: %7; }
        .markdown-body > * { margin-top: 0; margin-bottom: %8px; }
        .markdown-body > :last-child { margin-bottom: 0; }
        .markdown-body > .markdown-stream-content, .markdown-body > .markdown-stream-pending { margin: 0; padding: 0; }
        .markdown-stream-content > *, .markdown-stream-pending > * { margin-top: 0; margin-bottom: %8px; }
        .markdown-stream-sentinel { display: block; width: 0; height: 0; min-height: 0; margin: 0; padding: 0; overflow: hidden; font-size: 0; line-height: 0; }
        .markdown-body h1, .markdown-body h2, .markdown-body h3, .markdown-body h4 { color: %2; font-weight: 600; line-height: 1.25; margin-top: %9px; margin-bottom: %10px; }
        .markdown-body h1 { font-size: 1.75em; border-bottom: none; padding-bottom: 0; }
        .markdown-body h2 { font-size: 1.45em; border-bottom: none; padding-bottom: 0; }
        .markdown-body h3 { font-size: 1.2em; }
        .markdown-body h1:first-child, .markdown-body h2:first-child, .markdown-body h3:first-child, .markdown-body h4:first-child { margin-top: 0; }
        .markdown-body p { margin-top: 0; margin-bottom: %8px; }
        .markdown-body a { color: %12; text-decoration: none; font-weight: 500; }
        .markdown-body a:hover { text-decoration: underline; }
        .markdown-body blockquote { margin-top: 0; margin-bottom: %13px; margin-left: 0; margin-right: 0; padding: 10px 16px; color: %14; border-left: 4px solid %12; background: %15; border-radius: 4px; }
        .markdown-body blockquote p { margin: 0; }
        .markdown-body code { font-family: %16; color: %2; background: %17; padding: 3px 6px; border-radius: 4px; font-size: 13px; }
        .markdown-body pre { overflow: auto; margin-top: 0; margin-bottom: %13px; padding: 14px 16px; color: %2; background: %17; border: 1px solid %11; border-radius: 8px; font-size: 13px; line-height: 1.5; }
        .markdown-body pre code { padding: 0; background: transparent; font-size: 13px; border-radius: 0; }
        .markdown-body ul, .markdown-body ol { margin-top: 0; margin-bottom: %13px; padding-left: 28px; }
        .markdown-body li { margin: 4px 0; }
        .markdown-body li > p { margin: 0; }
        .markdown-body li.task-list-item { list-style-type: none; margin-left: -22px; }
        .markdown-body .task-list-checkbox { display: inline-block; width: 16px; height: 16px; margin-right: 8px; vertical-align: -3px; border: 1px solid %11; border-radius: 2px; background: %1; }
        .markdown-body .task-list-checkbox.is-checked { color: %1; border-color: %12; background: %12; font-size: 13px; font-weight: bold; line-height: 16px; text-align: center; }
        .markdown-body table { border-collapse: collapse; display: block; overflow: auto; max-width: 100%%; margin-top: 0; margin-bottom: %13px; }
        .markdown-body th, .markdown-body td { border: none; border-bottom: 1px solid %11; padding: 10px 16px; text-align: left; }
        .markdown-body th { font-weight: 600; color: %14; border-bottom: 2px solid %11; }
        .markdown-body tr:last-child td { border-bottom: none; }
        .markdown-body tr:nth-child(even) { background: %18; }
        .markdown-body hr { border: 0; border-top: 1px solid %11; margin-top: 24px; margin-bottom: 24px; }
        .markdown-body img { max-width: 100%%; height: auto; border-radius: 6px; }
        .markdown-error { white-space: pre-wrap; color: %2; }
        .markdown-body details.thought-chain { margin-top: 0; margin-bottom: 12px; padding: 8px 12px; border: 1px solid %11; border-radius: 6px; background: %15; font-size: 13px; color: %14; }
        .markdown-body details.thought-chain summary { cursor: pointer; font-weight: 500; color: %14; user-select: none; }
        .markdown-body details.thought-chain summary:hover { color: %2; }
        .markdown-body details.thought-chain .thought-body { margin-top: 8px; padding-top: 8px; border-top: 1px dashed %11; color: %14; }
        .markdown-body details.tool-card { margin-top: 0; margin-bottom: 12px; padding: 8px 12px; border: 1px solid %11; border-radius: 6px; background: %17; font-size: 13px; }
        .markdown-body details.tool-card summary { cursor: pointer; font-weight: 500; color: %2; user-select: none; }
        .markdown-body details.tool-card .tool-badge { display: inline-block; padding: 2px 6px; border-radius: 4px; background: %15; font-size: 11px; margin-right: 6px; }
        .markdown-body details.tool-card .tool-status { float: right; font-size: 12px; }
        .markdown-body details.tool-card .tool-status.success { color: #107c41; }
        .markdown-body details.tool-card .tool-status.error { color: #d83b01; }
        .markdown-body details.tool-card .tool-content { margin-top: 8px; }
        .markdown-body details.tool-card pre { margin-top: 4px; margin-bottom: 4px; }
        .markdown-body .image-gallery { display: flex; flex-wrap: wrap; gap: 8px; margin-bottom: 12px; }
        .markdown-body .image-gallery img { max-height: 240px; border-radius: 6px; object-fit: cover; }
    )")
        .arg(cssColor(colors.background),
             cssColor(colors.text),
             QString::number(style.verticalPadding),
             QString::number(style.horizontalPadding),
             style.bodyFontFamily,
             QString::number(style.fontSize),
             QString::number(style.lineHeight, 'f', 2),
             QString::number(style.blockSpacing),
             QString::number(style.headingTopSpacing),
             QString::number(style.headingBottomSpacing),
             cssColor(colors.border),
             cssColor(colors.link),
             QString::number(style.largeBlockSpacing),
             cssColor(colors.secondaryText),
             cssColor(colors.quoteBackground),
             style.monospaceFontFamily,
             cssColor(colors.codeBackground),
             cssColor(colors.tableStripe));

    return css + QLatin1Char('\n') + style.additionalCss;
}

} // namespace ui::widget
