#include "MarkdownDocumentController.h"

#include <QRegularExpression>
#include <QStringView>

namespace ui::widget {

MarkdownDocumentController::MarkdownDocumentController(QObject* parent)
    : QObject(parent)
{}

void MarkdownDocumentController::setMarkdown(const QString& markdown)
{
    if (m_markdown == markdown) return;
    m_markdown = markdown;
    m_activeTailDocument = ui::markdown::MarkdownDocument{};
    ++m_fullParseCount;
    m_document = m_parser.parse(m_markdown, parseOptions());
    emit documentRebuilt();
}

QString MarkdownDocumentController::markdown() const
{
    return m_markdown;
}

void MarkdownDocumentController::beginStream()
{
    m_streaming = true;
    m_markdown.clear();
    m_streamTail.clear();
    m_document = ui::markdown::MarkdownDocument{};
    m_activeTailDocument = ui::markdown::MarkdownDocument{};
    m_fullParseCount = 0;
    m_stableParseCount = 0;
    m_tailParseCount = 0;
    emit streamingChanged(true);
    emit documentRebuilt();
}

void MarkdownDocumentController::appendMarkdown(const QString& chunk)
{
    if (chunk.isEmpty()) return;
    m_markdown += chunk;
    m_streamTail += chunk;
    const qsizetype boundary = stableStreamingBoundary();
    if (boundary > 0) {
        ++m_stableParseCount;
        m_document.append(m_parser.parse(m_streamTail.left(boundary), parseOptions()));
        m_streamTail.remove(0, boundary);
        emit stableDocumentAppended();
    }
    ++m_tailParseCount;
    m_activeTailDocument = m_parser.parse(m_streamTail, parseOptions());
    emit tailDocumentChanged();
}

void MarkdownDocumentController::finishStream()
{
    m_streaming = false;
    m_streamTail.clear();
    m_activeTailDocument = ui::markdown::MarkdownDocument{};
    ++m_fullParseCount;
    m_document = m_parser.parse(m_markdown, parseOptions());
    emit streamingChanged(false);
    emit streamingFinished();
    emit documentRebuilt();
}

bool MarkdownDocumentController::isStreaming() const
{
    return m_streaming;
}

void MarkdownDocumentController::setAllowHtml(bool allow)
{
    m_allowHtml = allow;
}

bool MarkdownDocumentController::allowHtml() const
{
    return m_allowHtml;
}

void MarkdownDocumentController::toggleTaskAtLine(int sourceLine, bool currentlyChecked)
{
    if (sourceLine <= 0) return;
    int start = 0;
    for (int line = 1; line < sourceLine; ++line) {
        start = m_markdown.indexOf(u'\n', start);
        if (start < 0) return;
        ++start;
    }
    const int end = m_markdown.indexOf(u'\n', start);
    const int length = (end < 0 ? m_markdown.size() : end) - start;
    const QString lineText = m_markdown.mid(start, length);
    const QRegularExpression checkbox(QStringLiteral("\\[([ xX])\\]"));
    const QRegularExpressionMatch match = checkbox.match(lineText);
    if (!match.hasMatch()) return;
    const bool checked = !currentlyChecked;
    m_markdown.replace(start + match.capturedStart(1), 1, checked ? QStringLiteral("x") : QStringLiteral(" "));
    emit taskToggled(sourceLine, checked);
    m_activeTailDocument = ui::markdown::MarkdownDocument{};
    m_document = m_parser.parse(m_markdown, parseOptions());
    emit documentRebuilt();
}

const ui::markdown::MarkdownDocument& MarkdownDocumentController::stableDocument() const
{
    return m_document;
}

const ui::markdown::MarkdownDocument& MarkdownDocumentController::tailDocument() const
{
    return m_activeTailDocument;
}

qsizetype MarkdownDocumentController::stableStreamingBoundary() const
{
    if (m_streamTail.isEmpty()) return 0;

    bool inFence = false;
    bool inList = false;
    int consecutiveEmptyLines = 0;
    qsizetype lastBoundary = 0;
    qsizetype start = 0;

    auto isListMarker = [](const QStringView& line) -> bool {
        const QStringView t = line.trimmed();
        if (t.startsWith(u"- ") || t.startsWith(u"* ") || t.startsWith(u"+ ")) return true;
        int i = 0;
        while (i < t.size() && t[i].isDigit()) ++i;
        if (i > 0 && i < t.size() - 1 && (t[i] == u'.' || t[i] == u')') && t[i + 1].isSpace()) return true;
        return false;
    };

    auto isTopLevelBlockStart = [&isListMarker](const QStringView& line) -> bool {
        if (line.isEmpty()) return false;
        if (line.startsWith(u' ') || line.startsWith(u'\t')) return false;
        const QStringView t = line.trimmed();
        if (t.startsWith(u'#') || t.startsWith(u"---") || t.startsWith(u"***") || t.startsWith(u"___")
            || t.startsWith(u"```") || t.startsWith(u"~~~") || t.startsWith(u">")) {
            return true;
        }
        if (!isListMarker(line)) {
            return true;
        }
        return false;
    };

    while (start <= m_streamTail.size()) {
        const qsizetype end = m_streamTail.indexOf(u'\n', start);
        const qsizetype lineEnd = end < 0 ? m_streamTail.size() : end;
        const QStringView line{m_streamTail.constData() + start, lineEnd - start};
        const QStringView trimmed = line.trimmed();

        if (trimmed.startsWith(u"```") || trimmed.startsWith(u"~~~")) {
            inFence = !inFence;
            inList = false;
            consecutiveEmptyLines = 0;
        } else if (!inFence) {
            if (trimmed.isEmpty()) {
                ++consecutiveEmptyLines;
                const qsizetype boundaryCandidate = end < 0 ? lineEnd : end + 1;
                if (consecutiveEmptyLines >= 2) {
                    inList = false;
                    lastBoundary = boundaryCandidate;
                } else if (!inList) {
                    if (end >= 0 && end + 1 < m_streamTail.size()) {
                        const qsizetype nextEnd = m_streamTail.indexOf(u'\n', end + 1);
                        const qsizetype nextLineEnd = nextEnd < 0 ? m_streamTail.size() : nextEnd;
                        const QStringView nextLine{m_streamTail.constData() + end + 1, nextLineEnd - (end + 1)};
                        if (isTopLevelBlockStart(nextLine)) {
                            lastBoundary = boundaryCandidate;
                        }
                    }
                }
            } else {
                consecutiveEmptyLines = 0;
                if (isListMarker(line)) {
                    inList = true;
                } else if (!line.startsWith(u' ') && !line.startsWith(u'\t')) {
                    if (isTopLevelBlockStart(line)) {
                        inList = false;
                    }
                }
            }
        }

        if (end < 0) break;
        start = end + 1;
    }
    return lastBoundary;
}

ui::markdown::MarkdownParseOptions MarkdownDocumentController::parseOptions() const
{
    return {m_allowHtml};
}

} // namespace ui::widget
