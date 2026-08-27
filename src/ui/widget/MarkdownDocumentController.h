#pragma once

#include "ui/markdown/MarkdownSnapshot.h"

#include <QObject>
#include <QTimer>

namespace ui::widget {

struct MarkdownDocumentControllerMetrics {
    quint64 parseCount = 0;
    quint64 scheduledUpdateCount = 0;
    quint64 coalescedChunkCount = 0;
    quint64 renderEpoch = 0;
    qint64 lastParseUs = 0;
    ui::markdown::BlockChangeSet lastChanges;
};

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
    bool isStreaming() const noexcept { return m_streaming; }

    void setAllowHtml(bool allow);
    bool allowHtml() const noexcept { return m_allowHtml; }

    void toggleTask(ui::markdown::SourceRange markerRange, bool currentlyChecked);

    const ui::markdown::DocumentSnapshot& document() const noexcept { return m_document; }
    MarkdownDocumentControllerMetrics metrics() const { return m_metrics; }

signals:
    void documentChanged();
    void streamingChanged(bool streaming);
    void taskToggled(int sourceLine, bool checked);

private:
    void scheduleUpdate();
    void rebuild(bool canonical);
    ui::markdown::MarkdownParseOptions parseOptions() const;

    ui::markdown::MarkdownSourceBuffer m_source;
    ui::markdown::MarkdownSnapshotParser m_parser;
    ui::markdown::BlockReconciler m_reconciler;
    ui::markdown::DocumentSnapshot m_document;
    QTimer m_updateTimer;
    bool m_streaming = false;
    bool m_allowHtml = true;
    quint64 m_renderEpoch = 0;
    int m_pendingChunks = 0;
    MarkdownDocumentControllerMetrics m_metrics;
};

} // namespace ui::widget
