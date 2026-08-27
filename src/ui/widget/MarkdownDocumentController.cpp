#include "MarkdownDocumentController.h"

#include <QElapsedTimer>

namespace ui::widget {

MarkdownDocumentController::MarkdownDocumentController(QObject* parent)
    : QObject(parent)
{
    m_updateTimer.setSingleShot(true);
    m_updateTimer.setInterval(33);
    connect(&m_updateTimer, &QTimer::timeout, this, [this] { rebuild(false); });
}

void MarkdownDocumentController::setMarkdown(const QString& markdown)
{
    m_updateTimer.stop();
    m_pendingChunks = 0;
    if (!m_source.setText(markdown)) return;
    ++m_renderEpoch;
    rebuild(true);
}

QString MarkdownDocumentController::markdown() const
{
    return m_source.text();
}

void MarkdownDocumentController::beginStream()
{
    m_updateTimer.stop();
    m_pendingChunks = 0;
    m_streaming = true;
    m_source.clear();
    ++m_renderEpoch;
    m_metrics = {};
    m_metrics.renderEpoch = m_renderEpoch;
    emit streamingChanged(true);
    rebuild(false);
}

void MarkdownDocumentController::appendMarkdown(const QString& chunk)
{
    if (!m_source.append(chunk)) return;
    ++m_pendingChunks;
    if (m_streaming) {
        scheduleUpdate();
    } else {
        ++m_renderEpoch;
        rebuild(true);
    }
}

void MarkdownDocumentController::finishStream()
{
    if (!m_streaming) return;
    m_updateTimer.stop();
    m_streaming = false;
    ++m_renderEpoch;
    m_metrics.renderEpoch = m_renderEpoch;
    emit streamingChanged(false);
    rebuild(true);
}

void MarkdownDocumentController::setAllowHtml(bool allow)
{
    if (m_allowHtml == allow) return;
    m_allowHtml = allow;
    m_updateTimer.stop();
    ++m_renderEpoch;
    rebuild(!m_streaming);
}

void MarkdownDocumentController::toggleTask(ui::markdown::SourceRange markerRange, bool currentlyChecked)
{
    const ui::markdown::SourceOffsetRange range{markerRange.begin, markerRange.end};
    if (!range.isValid() || range.length() != 1) return;
    const int sourceLine = m_source.text().left(range.begin).count(u'\n') + 1;
    const bool checked = !currentlyChecked;
    if (!m_source.replaceRange(range, checked ? QStringView{u"x"} : QStringView{u" "})) return;
    ++m_renderEpoch;
    rebuild(true);
    emit taskToggled(sourceLine, checked);
}

void MarkdownDocumentController::scheduleUpdate()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start();
        ++m_metrics.scheduledUpdateCount;
    }
}

void MarkdownDocumentController::rebuild(bool canonical)
{
    m_updateTimer.stop();
    m_metrics.coalescedChunkCount += qMax(0, m_pendingChunks - 1);
    m_pendingChunks = 0;
    const auto source = m_source.snapshot();
    auto projection = ui::markdown::ParseProjection::identity(source, m_renderEpoch);
    projection.canonical = canonical;
    QElapsedTimer timer;
    timer.start();
    const auto parsed = m_parser.parse(projection, parseOptions());
    ui::markdown::BlockChangeSet changes;
    auto next = m_reconciler.reconcile(parsed, m_document.document ? &m_document : nullptr, &changes);
    m_document = std::move(next);
    m_metrics.lastParseUs = timer.nsecsElapsed() / 1000;
    ++m_metrics.parseCount;
    m_metrics.renderEpoch = m_renderEpoch;
    m_metrics.lastChanges = changes;
    emit documentChanged();
}

ui::markdown::MarkdownParseOptions MarkdownDocumentController::parseOptions() const
{
    return {m_allowHtml};
}

} // namespace ui::widget
