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
    m_document = m_parser.parse(m_markdown);
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
        m_document.append(m_parser.parse(m_streamTail.left(boundary)));
        m_streamTail.remove(0, boundary);
        emit stableDocumentAppended();
    }
    m_activeTailDocument = m_parser.parse(m_streamTail);
    emit tailDocumentChanged();
}

void MarkdownDocumentController::finishStream()
{
    m_streaming = false;
    m_streamTail.clear();
    m_activeTailDocument = ui::markdown::MarkdownDocument{};
    m_document = m_parser.parse(m_markdown);
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
    m_document = m_parser.parse(m_markdown);
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
    bool inFence = false;
    qsizetype lastBoundary = 0;
    qsizetype start = 0;
    while (start <= m_streamTail.size()) {
        const qsizetype end = m_streamTail.indexOf(u'\n', start);
        const qsizetype lineEnd = end < 0 ? m_streamTail.size() : end;
        const QStringView line{m_streamTail.constData() + start, lineEnd - start};
        const QStringView trimmed = line.trimmed();
        if (trimmed.startsWith(u"```") || trimmed.startsWith(u"~~~")) inFence = !inFence;
        if (!inFence && trimmed.isEmpty()) lastBoundary = end < 0 ? lineEnd : end + 1;
        if (end < 0) break;
        start = end + 1;
    }
    return lastBoundary;
}

} // namespace ui::widget
