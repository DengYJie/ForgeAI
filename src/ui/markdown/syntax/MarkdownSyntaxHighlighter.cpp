#include "MarkdownSyntaxHighlighter.h"

#include <QRegularExpression>

namespace ui::markdown {
namespace {

void appendMatches(QVector<QTextLayout::FormatRange>& formats, const QRegularExpression& expression,
                   const QString& source, const QColor& color, QFont::Weight weight = QFont::Normal)
{
    QTextCharFormat format;
    format.setForeground(color);
    format.setFontWeight(weight);
    auto match = expression.globalMatch(source);
    while (match.hasNext()) {
        const QRegularExpressionMatch value = match.next();
        formats.push_back({static_cast<int>(value.capturedStart()), static_cast<int>(value.capturedLength()), format});
    }
}

} // namespace

QVector<QTextLayout::FormatRange> MarkdownSyntaxHighlighter::highlightLine(const QString& source, const QString& language,
                                                                             const MarkdownTheme& theme) const
{
    QVector<QTextLayout::FormatRange> formats;
    const QString normalized = language.trimmed().toLower();
    if (normalized.isEmpty() || normalized == u"text" || normalized == u"plain" || normalized == u"plaintext") return formats;

    if (normalized == u"cpp" || normalized == u"c++" || normalized == u"c" || normalized == u"h" || normalized == u"hpp"
        || normalized == u"js" || normalized == u"javascript" || normalized == u"ts" || normalized == u"typescript") {
        appendMatches(formats, QRegularExpression(QStringLiteral("//.*$|/\\*.*\\*/")), source, theme.syntaxComment);
        appendMatches(formats, QRegularExpression(QStringLiteral("\"(?:\\\\.|[^\"])*\"|'(?:\\\\.|[^'])*'")), source, theme.syntaxString);
        appendMatches(formats, QRegularExpression(QStringLiteral("\\b(?:class|struct|namespace|using|auto|const|static|void|int|double|float|bool|return|if|else|for|while|switch|case|break|continue|new|delete|public|private|protected|async|await|function|let|var|import|export)\\b")), source, theme.syntaxKeyword, QFont::DemiBold);
    } else if (normalized == u"py" || normalized == u"python" || normalized == u"bash" || normalized == u"sh" || normalized == u"shell") {
        appendMatches(formats, QRegularExpression(QStringLiteral("#.*$")), source, theme.syntaxComment);
        appendMatches(formats, QRegularExpression(QStringLiteral("\"(?:\\\\.|[^\"])*\"|'(?:\\\\.|[^'])*'")), source, theme.syntaxString);
        appendMatches(formats, QRegularExpression(QStringLiteral("\\b(?:def|class|return|if|elif|else|for|while|in|import|from|as|try|except|with|lambda|True|False|None|function|then|fi|do|done)\\b")), source, theme.syntaxKeyword, QFont::DemiBold);
    } else if (normalized == u"json") {
        appendMatches(formats, QRegularExpression(QStringLiteral("\"(?:\\\\.|[^\"])*\"(?=\\s*:)")), source, theme.syntaxKeyword);
        appendMatches(formats, QRegularExpression(QStringLiteral("\"(?:\\\\.|[^\"])*\"")), source, theme.syntaxString);
        appendMatches(formats, QRegularExpression(QStringLiteral("\\b(?:true|false|null)\\b")), source, theme.syntaxKeyword, QFont::DemiBold);
    }
    appendMatches(formats, QRegularExpression(QStringLiteral("(?<![A-Za-z_])(?:0x[0-9A-Fa-f]+|\\d+(?:\\.\\d+)?)(?![A-Za-z_])")), source, theme.syntaxNumber);
    return formats;
}

} // namespace ui::markdown
