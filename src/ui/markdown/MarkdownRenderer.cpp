#include "MarkdownRenderer.h"

#include <QPainterPath>

namespace ui::markdown {

void MarkdownRenderer::paintInline(QPainter& painter, const InlineLayout& inlineLayout, const QPointF& origin,
                                   const MarkdownTheme& theme, int documentOffset, const TextSelection& selection) const
{
    QVector<QTextLayout::FormatRange> selections;
    if (selection.isValid()) {
        const int first = qMin(selection.anchor, selection.position);
        const int last = qMax(selection.anchor, selection.position);
        const int begin = qMax(0, first - documentOffset);
        const int end = qMin(inlineLayout.text.size(), last - documentOffset);
        if (end > begin) {
            QTextCharFormat format; format.setBackground(theme.selection); format.setForeground(theme.text);
            selections.push_back({begin, end - begin, format});
        }
    }
    inlineLayout.layout.draw(&painter, origin, selections);
}

int MarkdownRenderer::paint(QPainter& painter, const DocumentLayout& document, const MarkdownTheme& theme,
                            const QRectF& exposedDocumentRect, const TextSelection& selection, int hoveredBlock,
                            int hoveredCopyBlock, const QHash<int, BlockScrollOffset>& scrollOffsets,
                            const QHash<QString, QImage>& images, int copiedBlock) const
{
    const int first = document.firstVisibleBlock(exposedDocumentRect.top() - 80);
    const int last = document.lastVisibleBlock(exposedDocumentRect.bottom() + 80);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    int paintedBlocks = 0;
    for (int i = first; i <= last && i < document.blocks.size(); ++i) {
        const BlockLayout& block = document.blocks.at(i);
        if (!block.rect.intersects(exposedDocumentRect)) continue;
        ++paintedBlocks;
        if (block.quoteIndent > 0) {
            const QRectF quoteRect(block.rect.left() - block.quoteIndent + 2, block.rect.top() - 3,
                                   block.rect.width() + block.quoteIndent - 2, block.rect.height() + 6);
            painter.fillRect(quoteRect, theme.quoteBackground);
            painter.fillRect(QRectF(quoteRect.left(), quoteRect.top(), 3, quoteRect.height()), theme.quoteBorder);
        }
        switch (block.kind) {
        case BlockKind::Rule:
            painter.setPen(QPen(theme.divider, 1)); painter.drawLine(block.rect.left(), block.rect.center().y(), block.rect.right(), block.rect.center().y()); break;
        case BlockKind::CodeBlock: {
            painter.setPen(QPen(theme.codeBorder, 1)); painter.setBrush(theme.codeBackground); painter.drawRoundedRect(block.rect, theme.codeRadius, theme.codeRadius);
            painter.setPen(theme.secondaryText); painter.setFont(theme.codeFont); painter.drawText(QRectF(block.rect.left() + 12, block.rect.top(), 200, 30), Qt::AlignVCenter, block.language.isEmpty() ? QStringLiteral("text") : block.language);
            painter.setPen(QPen(theme.divider, 1));
            painter.drawLine(QPointF(block.rect.left() + 12, block.rect.top() + 30), QPointF(block.rect.right() - 12, block.rect.top() + 30));
            const bool blockHovered = (hoveredBlock == i);
            const bool copyHovered = (hoveredCopyBlock == i);
            const bool isCopied = (copiedBlock == i);
            if (blockHovered || isCopied) {
                if (copyHovered || isCopied) {
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(theme.inlineCodeBackground);
                    painter.drawRoundedRect(block.copyButtonRect, 4, 4);
                }
                if (isCopied) {
                    QPen checkPen(theme.link, 1.8);
                    checkPen.setCapStyle(Qt::RoundCap);
                    checkPen.setJoinStyle(Qt::RoundJoin);
                    painter.setPen(checkPen);
                    painter.setBrush(Qt::NoBrush);
                    const QPointF c = block.copyButtonRect.center();
                    QPainterPath checkPath;
                    checkPath.moveTo(c.x() - 4.5, c.y() + 0.5);
                    checkPath.lineTo(c.x() - 1.0, c.y() + 4.0);
                    checkPath.lineTo(c.x() + 5.0, c.y() - 3.5);
                    painter.drawPath(checkPath);
                } else {
                    const QPointF c = block.copyButtonRect.center();
                    QPen iconPen(copyHovered ? theme.text : theme.secondaryText, 1.2);
                    iconPen.setCapStyle(Qt::RoundCap);
                    iconPen.setJoinStyle(Qt::RoundJoin);
                    painter.setPen(iconPen);
                    painter.setBrush(Qt::NoBrush);

                    QPainterPath backPath;
                    backPath.moveTo(c.x() - 2.5, c.y() - 5.5);
                    backPath.lineTo(c.x() + 3.5, c.y() - 5.5);
                    backPath.arcTo(QRectF(c.x() + 2.5, c.y() - 5.5, 3.0, 3.0), 90, -90);
                    backPath.lineTo(c.x() + 5.5, c.y() + 1.5);
                    painter.drawPath(backPath);

                    const QRectF frontRect(c.x() - 5.5, c.y() - 2.5, 8.5, 9.5);
                    painter.setBrush(copyHovered ? theme.inlineCodeBackground : theme.codeBackground);
                    painter.drawRoundedRect(frontRect, 1.5, 1.5);
                }
            }
            const BlockScrollOffset scroll = scrollOffsets.value(i);
            qreal lineY = block.rect.top() + 42;
            painter.save(); painter.setClipRect(block.scrollInfo.viewportRect.isValid() ? block.scrollInfo.viewportRect : QRectF(block.rect.left() + 8, block.rect.top() + 30, block.rect.width() - 16, block.rect.height() - 34));
            for (int lineIndex = 0; lineIndex < block.codeLines.size(); ++lineIndex) {
                const auto& line = block.codeLines.at(lineIndex);
                paintInline(painter, *line, QPointF(block.contentX - scroll.x, lineY - scroll.y), theme, block.documentTextOffset + block.codeLineOffsets.value(lineIndex), selection);
                lineY += line->height;
            }
            painter.restore();
            if (blockHovered) {
                paintScrollBars(painter, block.scrollInfo, scroll, theme);
            }
            break;
        }
        case BlockKind::Table: {
            qreal y = block.rect.top();
            painter.save();
            QPainterPath clip;
            clip.addRoundedRect(block.rect, 5, 5);
            painter.setClipPath(clip);
            for (int row = 0; row < block.table->cells.size(); ++row) {
                const qreal h = block.table->rowHeights.at(row);
                if (block.table->headerRows.value(row)) painter.fillRect(QRectF(block.rect.left(), y, block.rect.width(), h), theme.tableHeader);
                qreal x = block.rect.left();
                for (int col = 0; col < block.table->columnWidths.size(); ++col) {
                    paintInline(painter, *block.table->cells[row][col], QPointF(x + 8, y + 8), theme, block.documentTextOffset, selection);
                    x += block.table->columnWidths[col];
                }
                if (row + 1 < block.table->cells.size()) {
                    painter.setPen(QPen(theme.tableBorder, 1));
                    painter.drawLine(QPointF(block.rect.left(), y + h), QPointF(block.rect.right(), y + h));
                }
                y += h;
            }
            qreal x = block.rect.left();
            painter.setPen(QPen(theme.tableBorder, 1));
            for (int col = 0; col + 1 < block.table->columnWidths.size(); ++col) {
                x += block.table->columnWidths[col];
                painter.drawLine(QPointF(x, block.rect.top()), QPointF(x, block.rect.bottom()));
            }
            painter.restore();
            painter.setPen(QPen(theme.tableBorder, 1)); painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(block.rect.adjusted(.5, .5, -.5, -.5), 5, 5);
            break;
        }
        case BlockKind::Image:
            painter.setPen(QPen(theme.tableBorder, 1)); painter.setBrush(theme.inlineCodeBackground); painter.drawRoundedRect(block.rect, 6, 6);
            if (const auto image = images.constFind(block.imageUrl); image != images.constEnd()) {
                const QSizeF size = image->size().scaled(block.rect.size().toSize(), Qt::KeepAspectRatio);
                const QRectF target(QPointF(block.rect.center().x() - size.width() / 2, block.rect.center().y() - size.height() / 2), size);
                painter.drawImage(target, *image);
            } else {
                painter.setPen(theme.secondaryText); painter.drawText(block.rect.adjusted(8, 8, -8, -8), Qt::AlignCenter | Qt::TextWordWrap,
                    block.imageAlt.isEmpty() ? QStringLiteral("Image\n%1").arg(block.imageUrl) : block.imageAlt);
            }
            break;
        case BlockKind::ListItem:
            painter.setPen(theme.text); painter.setFont(theme.bodyFont); painter.drawText(QRectF(block.rect.left(), block.rect.top(), block.contentX - block.rect.left() - 6, block.rect.height()), Qt::AlignTop | Qt::AlignRight, block.marker);
            if (block.taskItem) {
                painter.setPen(QPen(block.taskChecked ? theme.link : theme.tableBorder, 1));
                painter.setBrush(block.taskChecked ? theme.link : Qt::transparent);
                painter.drawRoundedRect(block.taskCheckRect, 3, 3);
                if (block.taskChecked) {
                    QPen checkPen(theme.codeBackground, 1.55);
                    checkPen.setCapStyle(Qt::RoundCap);
                    checkPen.setJoinStyle(Qt::RoundJoin);
                    painter.setPen(checkPen);
                    const QRectF glyph = block.taskCheckRect.adjusted(3.5, 3.5, -3.5, -3.5);
                    const QPointF start(glyph.left(), glyph.center().y());
                    const QPointF middle(glyph.left() + glyph.width() * .35, glyph.bottom());
                    const QPointF end(glyph.right(), glyph.top());
                    painter.drawLine(start, middle);
                    painter.drawLine(middle, end);
                }
            }
            [[fallthrough]];
        default:
            if (block.inlineLayout) paintInline(painter, *block.inlineLayout, QPointF(block.contentX, block.rect.top()), theme, block.documentTextOffset, selection);
            break;
        }
    }
    return paintedBlocks;
}

void MarkdownRenderer::paintScrollBars(QPainter& painter, const BlockScrollInfo& info, const BlockScrollOffset& offset,
                                       const MarkdownTheme& theme) const
{
    if (info.hasHorizontalScroll() && info.hScrollBarRect.isValid()) {
        painter.setPen(Qt::NoPen);
        QColor trackColor = theme.inlineCodeBackground;
        trackColor.setAlpha(90);
        painter.setBrush(trackColor);
        painter.drawRoundedRect(info.hScrollBarRect, info.hScrollBarRect.height() / 2, info.hScrollBarRect.height() / 2);

        const qreal viewW = info.viewportRect.width();
        const qreal contentW = info.contentSize.width();
        const qreal trackW = info.hScrollBarRect.width();
        const qreal thumbW = qBound<qreal>(24.0, trackW * (viewW / qMax<qreal>(1.0, contentW)), trackW * 0.8);
        const qreal maxScroll = info.maxScrollX();
        const qreal progress = (maxScroll > 0) ? qBound<qreal>(0, offset.x / maxScroll, 1.0) : 0;
        const qreal thumbX = info.hScrollBarRect.left() + (trackW - thumbW) * progress;
        const QRectF thumbRect(thumbX, info.hScrollBarRect.top(), thumbW, info.hScrollBarRect.height());

        QColor thumbColor = theme.secondaryText;
        thumbColor.setAlpha(160);
        painter.setBrush(thumbColor);
        painter.drawRoundedRect(thumbRect, thumbRect.height() / 2, thumbRect.height() / 2);
    }
}

HitTestResult MarkdownRenderer::hitTest(const DocumentLayout& document, const QPointF& position,
                                        const QHash<int, BlockScrollOffset>& scrollOffsets) const
{
    const int index = document.firstVisibleBlock(position.y());
    for (int i = index; i < document.blocks.size() && document.blocks[i].rect.top() <= position.y(); ++i) {
        const BlockLayout& block = document.blocks[i];
        if (!block.rect.contains(position)) continue;
        if (block.kind == BlockKind::CodeBlock && block.copyButtonRect.contains(position)) return {HitKind::CodeCopy, i, -1, block.code};
        if (block.taskItem && block.taskCheckRect.contains(position)) return {HitKind::TaskCheckbox, i, -1, block.taskChecked ? QStringLiteral("1") : QStringLiteral("0")};
        if (block.kind == BlockKind::Image) return {HitKind::Image, i, -1, block.imageUrl};
        if (block.inlineLayout) {
            const int cursor = block.inlineLayout->cursorAt(position.x() - block.contentX, position.y() - block.rect.top());
            if (cursor >= 0) {
                for (const LinkRange& link : block.inlineLayout->links) if (cursor >= link.start && cursor <= link.start + link.length) return {HitKind::Link, i, block.documentTextOffset + cursor, link.url};
                return {HitKind::Text, i, block.documentTextOffset + cursor, {}};
            }
        }
        if (block.kind == BlockKind::CodeBlock) {
            const BlockScrollOffset scroll = scrollOffsets.value(i);
            qreal lineY = block.rect.top() + 42;
            for (int lineIndex = 0; lineIndex < block.codeLines.size(); ++lineIndex) {
                const auto& line = block.codeLines.at(lineIndex);
                if (position.y() >= lineY && position.y() <= lineY + line->height) {
                    const int cursor = line->cursorAt(position.x() - (block.contentX - scroll.x), position.y() - lineY);
                    if (cursor >= 0) return {HitKind::Text, i, block.documentTextOffset + block.codeLineOffsets.value(lineIndex) + cursor, {}};
                }
                lineY += line->height;
            }
        }
        return {HitKind::None, i, -1, {}};
    }
    return {};
}

} // namespace ui::markdown
