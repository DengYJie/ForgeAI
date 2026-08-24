#pragma once

#include <QColor>
#include <QFont>
#include <QMarginsF>

namespace ui::markdown {

struct MarkdownTheme {
    QFont bodyFont;
    QFont codeFont;
    QColor background;
    QColor text;
    QColor secondaryText;
    QColor heading;
    QColor link;
    QColor linkHover;
    QColor selection;
    QColor inlineCodeBackground;
    QColor codeBackground;
    QColor codeBorder;
    QColor quoteBackground;
    QColor quoteBorder;
    QColor tableBorder;
    QColor tableHeader;
    QColor divider;
    QColor syntaxKeyword;
    QColor syntaxString;
    QColor syntaxComment;
    QColor syntaxNumber;
    QMarginsF contentMargins {12, 8, 12, 8};
    qreal blockGap = 12;
    qreal listIndent = 28;
    qreal codeRadius = 8;
    quint64 version = 1;

    QFont headingFont(int level) const;
    static MarkdownTheme light(const QFont& base = {});
    static MarkdownTheme dark(const QFont& base = {});
};

} // namespace ui::markdown
