#include "MarkdownSource.h"

namespace ui::markdown {

ParseProjection ParseProjection::identity(const SourceSnapshot& source, ProjectionRevision revision)
{
    ParseProjection result;
    result.text = source.text;
    result.sourceRevision = source.revision;
    result.projectionRevision = revision;
    result.canonical = true;
    result.projectionToSource.resize(source.text.size() + 1);
    for (qsizetype i = 0; i <= source.text.size(); ++i)
        result.projectionToSource[i] = i;
    return result;
}

bool MarkdownSourceBuffer::setText(QString text)
{
    if (m_text == text) return false;
    m_text = std::move(text);
    ++m_revision;
    return true;
}

bool MarkdownSourceBuffer::clear()
{
    return setText({});
}

bool MarkdownSourceBuffer::append(QStringView chunk)
{
    if (chunk.isEmpty()) return false;
    m_text += chunk;
    ++m_revision;
    return true;
}

bool MarkdownSourceBuffer::replaceRange(SourceOffsetRange range, QStringView replacement)
{
    if (!range.isValid() || range.end > m_text.size()) return false;
    if (QStringView{m_text}.mid(range.begin, range.length()) == replacement) return false;
    m_text.replace(range.begin, range.length(), replacement.toString());
    ++m_revision;
    return true;
}

} // namespace ui::markdown
