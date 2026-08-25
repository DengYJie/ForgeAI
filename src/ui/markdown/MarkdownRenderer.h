#pragma once

#include "MarkdownLayout.h"

#include <QPainter>
#include <QHash>
#include <QImage>

namespace ui::markdown {

enum class HitKind { None, Text, Link, CodeCopy, TaskCheckbox, Image };
struct HitTestResult { HitKind kind = HitKind::None; int blockIndex = -1; int textOffset = -1; QString value; };
struct TextSelection { int anchor = -1; int position = -1; bool isValid() const { return anchor >= 0 && position >= 0 && anchor != position; } };

class MarkdownRenderer final {
public:
    int paint(QPainter& painter, const DocumentLayout& document, const MarkdownTheme& theme,
              const QRectF& exposedDocumentRect, const TextSelection& selection, int hoveredBlock = -1,
              int hoveredCopyBlock = -1, const QHash<int, BlockScrollOffset>& scrollOffsets = {},
              const QHash<QString, QImage>& images = {}, int copiedBlock = -1) const;
    HitTestResult hitTest(const DocumentLayout& document, const QPointF& documentPosition,
                          const QHash<int, BlockScrollOffset>& scrollOffsets = {}) const;
private:
    void paintInline(QPainter& painter, const InlineLayout& layout, const QPointF& origin,
                     const MarkdownTheme& theme, int documentOffset, const TextSelection& selection) const;
    void paintScrollBars(QPainter& painter, const BlockScrollInfo& info, const BlockScrollOffset& offset,
                         const MarkdownTheme& theme) const;
};

} // namespace ui::markdown
