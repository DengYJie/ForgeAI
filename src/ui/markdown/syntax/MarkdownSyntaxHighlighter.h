#pragma once

#include "ui/markdown/MarkdownTheme.h"

#include <QTextLayout>

namespace ui::markdown {

class MarkdownSyntaxHighlighter final {
public:
    QVector<QTextLayout::FormatRange> highlightLine(const QString& source, const QString& language,
                                                     const MarkdownTheme& theme) const;
};

} // namespace ui::markdown
