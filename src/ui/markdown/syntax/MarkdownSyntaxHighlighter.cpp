#include "MarkdownSyntaxHighlighter.h"

#include <QRegularExpression>

namespace ui::markdown {
namespace {

struct RegexCache {
    QRegularExpression cppComment{QStringLiteral("//.*$|/\\*.*\\*/")};
    QRegularExpression cppString{QStringLiteral("\"(?:\\\\.|[^\"])*\"|'(?:\\\\.|[^'])*'")};
    QRegularExpression cppKeyword{QStringLiteral("\\b(?:class|struct|namespace|using|auto|const|static|void|int|double|float|bool|return|if|else|for|while|switch|case|break|continue|new|delete|public|private|protected|async|await|function|let|var|import|export)\\b")};
    
    QRegularExpression pyComment{QStringLiteral("#.*$")};
    QRegularExpression pyString{QStringLiteral("\"(?:\\\\.|[^\"])*\"|'(?:\\\\.|[^'])*'")};
    QRegularExpression pyKeyword{QStringLiteral("\\b(?:def|class|return|if|elif|else|for|while|in|import|from|as|try|except|with|lambda|True|False|None|function|then|fi|do|done)\\b")};

    QRegularExpression jsonKey{QStringLiteral("\"(?:\\\\.|[^\"])*\"(?=\\s*:)")};
    QRegularExpression jsonString{QStringLiteral("\"(?:\\\\.|[^\"])*\"")};
    QRegularExpression jsonKeyword{QStringLiteral("\\b(?:true|false|null)\\b")};

    QRegularExpression number{QStringLiteral("(?<![A-Za-z_])(?:0x[0-9A-Fa-f]+|\\d+(?:\\.\\d+)?)(?![A-Za-z_])")};
};

const RegexCache& getRegexCache() {
    static const RegexCache cache;
    return cache;
}

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

    const auto& rx = getRegexCache();

    if (normalized == u"cpp" || normalized == u"c++" || normalized == u"c" || normalized == u"h" || normalized == u"hpp"
        || normalized == u"js" || normalized == u"javascript" || normalized == u"ts" || normalized == u"typescript") {
        appendMatches(formats, rx.cppComment, source, theme.syntaxComment);
        appendMatches(formats, rx.cppString, source, theme.syntaxString);
        appendMatches(formats, rx.cppKeyword, source, theme.syntaxKeyword, QFont::DemiBold);
    } else if (normalized == u"py" || normalized == u"python" || normalized == u"bash" || normalized == u"sh" || normalized == u"shell") {
        appendMatches(formats, rx.pyComment, source, theme.syntaxComment);
        appendMatches(formats, rx.pyString, source, theme.syntaxString);
        appendMatches(formats, rx.pyKeyword, source, theme.syntaxKeyword, QFont::DemiBold);
    } else if (normalized == u"json") {
        appendMatches(formats, rx.jsonKey, source, theme.syntaxKeyword);
        appendMatches(formats, rx.jsonString, source, theme.syntaxString);
        appendMatches(formats, rx.jsonKeyword, source, theme.syntaxKeyword, QFont::DemiBold);
    }
    appendMatches(formats, rx.number, source, theme.syntaxNumber);
    return formats;
}

} // namespace ui::markdown
