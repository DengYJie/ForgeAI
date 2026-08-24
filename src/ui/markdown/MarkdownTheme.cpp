#include "MarkdownTheme.h"

namespace ui::markdown {

QFont MarkdownTheme::headingFont(int level) const
{
    QFont result = bodyFont;
    const qreal scale[] = {1.80, 1.52, 1.28, 1.12, 1.0, 0.92};
    result.setPointSizeF(bodyFont.pointSizeF() * scale[qBound(1, level, 6) - 1]);
    result.setWeight(QFont::DemiBold);
    return result;
}

MarkdownTheme MarkdownTheme::light(const QFont& base)
{
    MarkdownTheme theme;
    theme.bodyFont = base.family().isEmpty() ? QFont(QStringLiteral("Segoe UI")) : base;
    theme.bodyFont.setStyleStrategy(QFont::PreferAntialias);
    if (theme.bodyFont.pointSizeF() <= 0) theme.bodyFont.setPointSizeF(10.5);
    theme.codeFont = QFont(QStringLiteral("Cascadia Mono"));
    theme.codeFont.setStyleStrategy(QFont::PreferAntialias);
    theme.codeFont.setPointSizeF(theme.bodyFont.pointSizeF() * .93);
    theme.background = Qt::transparent;
    theme.text = QColor("#1b1b1f"); theme.secondaryText = QColor("#5f5e67");
    theme.heading = theme.text; theme.link = QColor("#0f6cbd"); theme.linkHover = QColor("#004e8c");
    theme.selection = QColor("#b4d6fa"); theme.inlineCodeBackground = QColor("#f1f3f5");
    theme.codeBackground = QColor("#f6f8fa"); theme.codeBorder = QColor("#d8dee4");
    theme.quoteBackground = QColor("#f5f7fa"); theme.quoteBorder = QColor("#0f6cbd");
    theme.tableBorder = QColor("#d1d9e0"); theme.tableHeader = QColor("#f6f8fa"); theme.divider = QColor("#d1d9e0");
    theme.syntaxKeyword = QColor("#7f3fbf"); theme.syntaxString = QColor("#a31515"); theme.syntaxComment = QColor("#5e6a73"); theme.syntaxNumber = QColor("#0c7a43");
    return theme;
}

MarkdownTheme MarkdownTheme::dark(const QFont& base)
{
    MarkdownTheme theme = light(base);
    theme.version = 2;
    theme.text = QColor("#f4f4f5"); theme.secondaryText = QColor("#c7c7ca"); theme.heading = theme.text;
    theme.link = QColor("#7cc4ff"); theme.linkHover = QColor("#b4dcff"); theme.selection = QColor("#264f78");
    theme.inlineCodeBackground = QColor("#2b2b2f"); theme.codeBackground = QColor("#202024");
    theme.codeBorder = QColor("#3d3d42"); theme.quoteBackground = QColor("#29292d");
    theme.quoteBorder = QColor("#7cc4ff"); theme.tableBorder = QColor("#414146"); theme.tableHeader = QColor("#29292d");
    theme.divider = QColor("#414146");
    theme.syntaxKeyword = QColor("#d7a9ff"); theme.syntaxString = QColor("#a7e3a1"); theme.syntaxComment = QColor("#8d9aa5"); theme.syntaxNumber = QColor("#7dd3fc");
    return theme;
}

} // namespace ui::markdown
