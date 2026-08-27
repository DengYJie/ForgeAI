#pragma once

#include <QString>
#include <QVector>

namespace ui::markdown {

using SourceRevision = quint64;
using ProjectionRevision = quint64;

struct SourceOffsetRange {
    qsizetype begin = 0;
    qsizetype end = 0;

    bool isValid() const noexcept { return begin >= 0 && end >= begin; }
    qsizetype length() const noexcept { return end - begin; }
};

struct SourceSnapshot {
    QString text;
    SourceRevision revision = 0;
};

struct ParseProjection {
    QString text;
    SourceRevision sourceRevision = 0;
    ProjectionRevision projectionRevision = 0;
    QVector<qsizetype> projectionToSource;
    bool canonical = true;

    static ParseProjection identity(const SourceSnapshot& source, ProjectionRevision revision = 0);
};

class MarkdownSourceBuffer final
{
public:
    bool setText(QString text);
    bool clear();
    bool append(QStringView chunk);
    bool replaceRange(SourceOffsetRange range, QStringView replacement);

    const QString& text() const noexcept { return m_text; }
    SourceRevision revision() const noexcept { return m_revision; }
    SourceSnapshot snapshot() const { return {m_text, m_revision}; }

private:
    QString m_text;
    SourceRevision m_revision = 0;
};

} // namespace ui::markdown
