#pragma once

#include <QColor>
#include <QString>

namespace ui::widget {

struct MarkdownColorScheme {
    QColor background;
    QColor text;
    QColor secondaryText;
    QColor border;
    QColor link;
    QColor quoteBackground;
    QColor codeBackground;
    QColor tableStripe;
};

struct MarkdownStyle {
    QString bodyFontFamily;
    QString monospaceFontFamily = QStringLiteral("Consolas, 'Cascadia Mono', monospace");
    int fontSize = 14;
    qreal lineHeight = 1.58;
    int verticalPadding = 20;
    int horizontalPadding = 24;
    int blockSpacing = 14;
    int largeBlockSpacing = 16;
    int headingTopSpacing = 24;
    int headingBottomSpacing = 12;
    QString additionalCss;
};

class MarkdownStyleSheet
{
public:
    MarkdownColorScheme colors;
    MarkdownStyle style;

    QString build() const;
};

} // namespace ui::widget
