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

signals:
    void documentRebuilt();
    void stableDocumentAppended();
    void tailDocumentChanged();
    void streamingChanged(bool streaming);
    void streamingFinished();
    void taskToggled(int sourceLine, bool checked);

private:
    qsizetype stableStreamingBoundary() const;

    QString m_markdown;
    QString m_streamTail;
    bool m_streaming = false;
    bool m_allowHtml = true;
    ui::markdown::MarkdownParser m_parser;
    ui::markdown::MarkdownDocument m_document;
    ui::markdown::MarkdownDocument m_activeTailDocument;
};

} // namespace ui::widget
