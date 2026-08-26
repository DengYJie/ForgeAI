#pragma once

#include "ui/markdown/MarkdownDocument.h"

#include <QObject>
#include <QString>

namespace ui::widget {

class MarkdownDocumentController : public QObject
{
    Q_OBJECT

public:
    explicit MarkdownDocumentController(QObject* parent = nullptr);

    void setMarkdown(const QString& markdown);
    QString markdown() const;

    void beginStream();
    void appendMarkdown(const QString& chunk);
    void finishStream();
    bool isStreaming() const;

    void setAllowHtml(bool allow);
    bool allowHtml() const;

    void toggleTaskAtLine(int sourceLine, bool currentlyChecked);

    const ui::markdown::MarkdownDocument& stableDocument() const;
    const ui::markdown::MarkdownDocument& tailDocument() const;

    quint64 fullParseCount() const noexcept { return m_fullParseCount; }
    quint64 stableParseCount() const noexcept { return m_stableParseCount; }
    quint64 tailParseCount() const noexcept { return m_tailParseCount; }
    quint64 tailGeneration() const noexcept { return m_tailGeneration; }

signals:
    void documentRebuilt();
    void stableDocumentAppended();
    void tailDocumentChanged();
    void streamingChanged(bool streaming);
    // streamingFinished is intentionally NOT emitted here.
    // MarkdownView emits streamingFinished() after it receives the final
    // layoutReady() triggered by documentRebuilt(), ensuring consumers
    // always see the completed layout when the signal fires.
    void tailGenerationChanged(quint64 generation);
    void taskToggled(int sourceLine, bool checked);

private:
    qsizetype stableStreamingBoundary() const;
    ui::markdown::MarkdownParseOptions parseOptions() const;

    QString m_markdown;
    QString m_streamTail;
    bool m_streaming = false;
    bool m_allowHtml = true;
    ui::markdown::MarkdownParser m_parser;
    ui::markdown::MarkdownDocument m_document;
    ui::markdown::MarkdownDocument m_activeTailDocument;
    quint64 m_fullParseCount = 0;
    quint64 m_stableParseCount = 0;
    quint64 m_tailParseCount = 0;
    quint64 m_tailGeneration = 0;
};

} // namespace ui::widget
