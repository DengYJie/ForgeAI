#include "MarkdownLayout.h"

#include <QFontMetricsF>
#include <QTextCharFormat>
#include <algorithm>

namespace ui::markdown {
namespace {

struct InlineBuilder {
    QString text;
    QVector<QTextLayout::FormatRange> formats;
    QVector<LinkRange> links;
    const MarkdownTheme& theme;

    void add(const MarkdownNode& node, QTextCharFormat format = {}, QString activeLink = {}) {
        const int start = text.size();
        switch (node.type) {
        case MarkdownNodeType::Text: text += node.literal; break;
        case MarkdownNodeType::SoftBreak: text += u' '; break;
        case MarkdownNodeType::HardBreak: text += u'\n'; break;
        case MarkdownNodeType::InlineCode:
            format.setFont(theme.codeFont); format.setBackground(theme.inlineCodeBackground); text += node.literal; break;
        case MarkdownNodeType::Emphasis: format.setFontItalic(true); break;
        case MarkdownNodeType::Strong: format.setFontWeight(QFont::Bold); break;
        case MarkdownNodeType::Strikethrough: format.setFontStrikeOut(true); break;
        case MarkdownNodeType::Link:
            activeLink = node.attributes.url; format.setForeground(theme.link); format.setFontUnderline(true); break;
        case MarkdownNodeType::Image:
            text += node.literal.isEmpty() ? node.attributes.url : node.literal; break;
        default: break;
        }
        if (!node.children.empty()) {
            for (const auto& child : node.children) add(*child, format, activeLink);
        }
        const int length = text.size() - start;
        // Do not append an empty format range for plain text: in QTextLayout a
        // later empty range resets the inherited foreground to the platform
        // default (black), overriding the themed base format.
        if (length > 0 && (!format.isEmpty() || !activeLink.isEmpty())) {
            QTextLayout::FormatRange range{start, length, format};
            formats.push_back(range);
            if (!activeLink.isEmpty()) links.push_back({start, length, activeLink});
        }
    }
};

std::shared_ptr<InlineLayout> plainInline(const QString& text, const QFont& font,
                                          const MarkdownTheme& theme, qreal width, bool wrap = true)
{
    QTextCharFormat base; base.setForeground(theme.text);
    return std::make_shared<InlineLayout>(text, font, QVector<QTextLayout::FormatRange>{{0, static_cast<int>(text.size()), base}}, QVector<LinkRange>{}, width, wrap);
}

QString languageName(QString fence)
{
    return fence.section(u' ', 0, 0).trimmed().toLower();
}

} // namespace

InlineLayout::InlineLayout(QString value, const QFont& baseFont, const QVector<QTextLayout::FormatRange>& formats,
                           QVector<LinkRange> linkRanges, qreal availableWidth, bool wrap)
    : text(std::move(value)), layout(text, baseFont), links(std::move(linkRanges)), width(qMax<qreal>(1, availableWidth))
{
    QTextOption option;
    option.setWrapMode(wrap ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
    option.setUseDesignMetrics(true);
    layout.setTextOption(option);
    layout.setFormats(formats);
    layout.beginLayout();
    qreal y = 0;
    qreal maxLineWidth = 0;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) break;
        line.setLineWidth(width);
        line.setPosition(QPointF(0, y));
        y += line.height();
        maxLineWidth = qMax(maxLineWidth, line.naturalTextWidth());
    }
    layout.endLayout();
    height = y;
    width = maxLineWidth;
    if (height <= 0) height = QFontMetricsF(baseFont).height();
}

int InlineLayout::cursorAt(qreal x, qreal y) const
{
    for (int i = 0; i < layout.lineCount(); ++i) {
        const QTextLine line = layout.lineAt(i);
        if (y >= line.y() && y <= line.y() + line.height()) {
            if (x >= -4 && x <= line.naturalTextWidth() + 12) {
                return line.xToCursor(x);
            }
            return -1;
        }
    }
    return -1;
}

int DocumentLayout::firstVisibleBlock(qreal y) const
{
    auto it = std::lower_bound(blocks.begin(), blocks.end(), y, [](const BlockLayout& block, qreal value) {
        return block.rect.bottom() < value;
    });
    return static_cast<int>(std::distance(blocks.begin(), it));
}

int DocumentLayout::lastVisibleBlock(qreal y) const
{
    auto it = std::upper_bound(blocks.begin(), blocks.end(), y, [](qreal value, const BlockLayout& block) {
        return value < block.rect.top();
    });
    return qMax(0, static_cast<int>(std::distance(blocks.begin(), it)) - 1);
}

int DocumentLayout::textLength() const
{
    int result = 0;
    for (const BlockLayout& block : blocks) {
        int length = 0;
        if (block.inlineLayout) length = static_cast<int>(block.inlineLayout->text.size());
        else if (block.kind == BlockKind::CodeBlock) length = static_cast<int>(block.code.size());
        result = qMax(result, block.documentTextOffset + length + 1);
    }
    return result;
}

std::shared_ptr<InlineLayout> MarkdownLayoutEngine::makeInline(const MarkdownNode& node, const QFont& font,
                                                                 const MarkdownTheme& theme, qreal width) const
{
    InlineBuilder builder{{}, {}, {}, theme};
    if (node.type == MarkdownNodeType::Html && !node.literal.isEmpty())
        builder.text = node.literal;
    for (const auto& child : node.children) builder.add(*child);
    if (!builder.text.isEmpty()) {
        QTextCharFormat base;
        base.setForeground(theme.text);
        builder.formats.prepend({0, static_cast<int>(builder.text.size()), base});
    }
    return std::make_shared<InlineLayout>(builder.text, font, builder.formats, builder.links, width);
}

void MarkdownLayoutEngine::appendNodes(const std::vector<std::unique_ptr<MarkdownNode>>& nodes, DocumentLayout& result,
                                       qreal& y, qreal width, qreal indent, int quoteDepth, int listDepth,
                                       const MarkdownTheme& theme, int& textOffset,
                                       const QHash<QString, QImage>& images) const
{
    const qreal right = width - theme.contentMargins.right();
    for (const auto& nodePtr : nodes) {
        const MarkdownNode& node = *nodePtr;
        if (node.type == MarkdownNodeType::BlockQuote) {
            appendNodes(node.children, result, y, width, indent + 18, quoteDepth + 1, listDepth, theme, textOffset, images);
            continue;
        }
        // ListItem can arrive here from a nested list. Its parent marker policy is
        // already consumed, so use a stable bullet and keep continuation text aligned.
        if (node.type == MarkdownNodeType::ListItem) {
            bool first = true;
            for (const auto& child : node.children) {
                if (child->type == MarkdownNodeType::Paragraph) {
                    BlockLayout block; block.kind = BlockKind::ListItem;
                    block.marker = first && !node.attributes.taskListItem ? QStringLiteral("◦") : QString();
                    block.taskItem = first && node.attributes.taskListItem;
                    block.taskChecked = node.attributes.taskChecked;
                    block.taskSourceLine = node.sourceRange.startLine;
                    block.quoteIndent = quoteDepth * 18; block.contentX = indent + theme.listIndent;
                    block.inlineLayout = makeInline(*child, theme.bodyFont, theme, right - block.contentX);
                    block.documentTextOffset = textOffset; textOffset += static_cast<int>(block.inlineLayout->text.size()) + 1;
                    block.rect = QRectF(indent, y, right - indent, block.inlineLayout->height + 4);
                    if (block.taskItem) block.taskCheckRect = QRectF(indent, qRound(y + (block.rect.height() - 16) / 2), 16, 16);
                    result.blocks.push_back(std::move(block)); y += result.blocks.back().rect.height() + 3; first = false;
                } else if (child->type == MarkdownNodeType::List) {
                    appendNodes(child->children, result, y, width, indent + theme.listIndent, quoteDepth, listDepth + 1, theme, textOffset, images);
                }
            }
            continue;
        }
        if (node.type == MarkdownNodeType::List) {
            int number = node.attributes.listStart;
            for (const auto& itemPtr : node.children) {
                const MarkdownNode& item = *itemPtr;
                const qreal markerIndent = indent + listDepth * theme.listIndent;
                const QString marker = node.attributes.orderedList ? QString::number(number++) + u'.' : QStringLiteral("•");
                bool first = true;
                for (const auto& child : item.children) {
                    if (child->type == MarkdownNodeType::Paragraph) {
                        BlockLayout block;
                        block.kind = BlockKind::ListItem; block.marker = first && !item.attributes.taskListItem ? marker : QString();
                        block.taskItem = first && item.attributes.taskListItem;
                        block.taskChecked = item.attributes.taskChecked;
                        block.taskSourceLine = item.sourceRange.startLine;
                        block.quoteIndent = quoteDepth * 18; block.contentX = markerIndent + theme.listIndent;
                        block.inlineLayout = makeInline(*child, theme.bodyFont, theme, right - block.contentX);
                        block.documentTextOffset = textOffset; textOffset += static_cast<int>(block.inlineLayout->text.size()) + 1;
                        block.rect = QRectF(markerIndent, y, right - markerIndent, block.inlineLayout->height + 4);
                        if (block.taskItem) block.taskCheckRect = QRectF(markerIndent, qRound(y + (block.rect.height() - 16) / 2), 16, 16);
                        result.blocks.push_back(std::move(block)); y += result.blocks.back().rect.height() + 3; first = false;
                    } else {
                        appendNodes(child->children, result, y, width, markerIndent + theme.listIndent, quoteDepth, listDepth + 1, theme, textOffset, images);
                    }
                }
            }
            y += theme.blockGap * .5;
            continue;
        }
        if (node.type == MarkdownNodeType::ThematicBreak) {
            BlockLayout block; block.kind = BlockKind::Rule; block.quoteIndent = quoteDepth * 18;
            block.documentTextOffset = textOffset++;
            block.rect = QRectF(indent, y + theme.blockGap * .5, right - indent, 1); result.blocks.push_back(block);
            y += theme.blockGap + 1; continue;
        }
        if (node.type == MarkdownNodeType::CodeBlock) {
            QString codeText = node.literal;
            if (codeText.endsWith(u'\n')) codeText.chop(1);
            BlockLayout block; block.kind = BlockKind::CodeBlock; block.code = codeText; block.language = languageName(node.attributes.fenceInfo);
            block.quoteIndent = quoteDepth * 18; block.contentX = indent + 12;
            const QStringList lines = codeText.split(u'\n'); qreal codeHeight = 0;
            const qreal available = qMax<qreal>(1, right - block.contentX - 12);
            int codeOffset = 0;
            qreal maxLineWidth = 0;
            for (const QString& line : lines) {
                QTextCharFormat base; base.setForeground(theme.text);
                QVector<QTextLayout::FormatRange> formats{{0, static_cast<int>(line.size()), base}};
                formats += m_syntaxHighlighter.highlightLine(line, block.language, theme);
                auto l = std::make_shared<InlineLayout>(line, theme.codeFont, formats, QVector<LinkRange>{}, 4096, false);
                maxLineWidth = qMax(maxLineWidth, l->width);
                codeHeight += l->height; block.codeLines.push_back(l); block.codeLineOffsets.push_back(codeOffset);
                codeOffset += static_cast<int>(line.size()) + 1;
            }
            block.rect = QRectF(indent, y, right - indent, 30 + 12 + codeHeight + 12);
            block.copyButtonRect = QRectF(right - 34, y + 4, 24, 22);
            block.scrollInfo.viewportRect = QRectF(block.rect.left() + 8, block.rect.top() + 30, block.rect.width() - 16, block.rect.height() - 34);
            block.scrollInfo.contentSize = QSizeF(maxLineWidth, codeHeight);
            if (block.scrollInfo.hasHorizontalScroll()) {
                block.scrollInfo.hScrollBarRect = QRectF(block.scrollInfo.viewportRect.left() + 4, block.rect.bottom() - 7, block.scrollInfo.viewportRect.width() - 8, 4);
            }
            block.documentTextOffset = textOffset; textOffset += static_cast<int>(codeText.size()) + 1; result.blocks.push_back(std::move(block)); y += result.blocks.back().rect.height() + theme.blockGap; continue;
        }
        if (node.type == MarkdownNodeType::Table) {
            auto data = std::make_shared<TableLayoutData>(); int columns = 0;
            for (const auto& row : node.children) columns = qMax(columns, static_cast<int>(row->children.size()));
            if (columns == 0) continue;
            const qreal availableWidth = right - indent;
            QVector<qreal> preferred(columns, 64);
            for (const auto& row : node.children) {
                for (int column = 0; column < row->children.size(); ++column) {
                    const auto intrinsic = makeInline(*row->children.at(column), theme.bodyFont, theme, 4096);
                    preferred[column] = qMax(preferred[column], intrinsic->layout.maximumWidth() + 16);
                }
            }
            qreal preferredTotal = 0;
            for (const qreal value : preferred) preferredTotal += value;
            data->columnWidths.resize(columns);
            if (preferredTotal <= availableWidth) {
                const qreal extra = (availableWidth - preferredTotal) / columns;
                for (int column = 0; column < columns; ++column) data->columnWidths[column] = preferred[column] + extra;
            } else {
                const qreal scale = availableWidth / preferredTotal;
                for (int column = 0; column < columns; ++column) data->columnWidths[column] = qMax<qreal>(24, preferred[column] * scale);
            }
            qreal totalHeight = 0;
            for (const auto& row : node.children) {
                QVector<std::shared_ptr<InlineLayout>> cells; qreal rowHeight = 0;
                const QFont cellFont = [&] { QFont font = theme.bodyFont; if (row->attributes.tableHeader) font.setWeight(QFont::DemiBold); return font; }();
                for (int column = 0; column < row->children.size(); ++column) {
                    auto value = makeInline(*row->children.at(column), cellFont, theme, data->columnWidths[column] - 16);
                    rowHeight = qMax(rowHeight, value->height); cells.push_back(value);
                }
                while (cells.size() < columns) cells.push_back(plainInline({}, cellFont, theme, data->columnWidths[cells.size()] - 16));
                data->cells.push_back(cells); data->rowHeights.push_back(rowHeight + 16); data->headerRows.push_back(row->attributes.tableHeader); totalHeight += rowHeight + 16;
            }
            BlockLayout block; block.kind = BlockKind::Table; block.table = data; block.quoteIndent = quoteDepth * 18; block.documentTextOffset = textOffset++;
            block.rect = QRectF(indent, y, right - indent, totalHeight); result.blocks.push_back(std::move(block)); y += totalHeight + theme.blockGap; continue;
        }
        if (node.type == MarkdownNodeType::Paragraph && node.children.size() == 1 && node.children.front()->type == MarkdownNodeType::Image) {
            const MarkdownNode& imageNode = *node.children.front();
            BlockLayout block; block.kind = BlockKind::Image; block.imageUrl = imageNode.attributes.url;
            block.imageAlt = makeInline(imageNode, theme.bodyFont, theme, right - indent)->text;
            block.documentTextOffset = textOffset++;
            QSizeF intrinsic(360, 180);
            if (const auto imgIt = images.constFind(block.imageUrl); imgIt != images.constEnd() && !imgIt->isNull()) {
                intrinsic = imgIt->size();
            }
            const qreal maxW = right - indent;
            const qreal maxH = 600;
            qreal w = qMin<qreal>(intrinsic.width(), maxW);
            qreal h = intrinsic.height() * (w / qMax<qreal>(1.0, intrinsic.width()));
            if (h > maxH) {
                h = maxH;
                w = intrinsic.width() * (h / qMax<qreal>(1.0, intrinsic.height()));
            }
            block.rect = QRectF(indent, y, w, h);
            block.imageIntrinsicSize = intrinsic;
            result.blocks.push_back(std::move(block)); y += h + theme.blockGap; continue;
        }
        if (node.type == MarkdownNodeType::Image) {
            BlockLayout block; block.kind = BlockKind::Image; block.imageUrl = node.attributes.url; block.imageAlt = node.literal;
            block.documentTextOffset = textOffset++;
            QSizeF intrinsic(360, 180);
            if (const auto imgIt = images.constFind(block.imageUrl); imgIt != images.constEnd() && !imgIt->isNull()) {
                intrinsic = imgIt->size();
            }
            const qreal maxW = right - indent;
            const qreal maxH = 600;
            qreal w = qMin<qreal>(intrinsic.width(), maxW);
            qreal h = intrinsic.height() * (w / qMax<qreal>(1.0, intrinsic.width()));
            if (h > maxH) {
                h = maxH;
                w = intrinsic.width() * (h / qMax<qreal>(1.0, intrinsic.height()));
            }
            block.rect = QRectF(indent, y, w, h);
            block.imageIntrinsicSize = intrinsic;
            result.blocks.push_back(std::move(block)); y += h + theme.blockGap; continue;
        }
        if (node.type != MarkdownNodeType::Paragraph && node.type != MarkdownNodeType::Heading && node.type != MarkdownNodeType::Html) continue;
        BlockLayout block; block.kind = node.type == MarkdownNodeType::Heading ? BlockKind::Heading : (node.type == MarkdownNodeType::Html ? BlockKind::Html : (quoteDepth ? BlockKind::QuoteContent : BlockKind::Paragraph));
        block.quoteIndent = quoteDepth * 18; block.contentX = indent;
        const QFont font = node.type == MarkdownNodeType::Heading ? theme.headingFont(node.attributes.headingLevel) : theme.bodyFont;
        block.inlineLayout = makeInline(node, font, theme, right - indent); block.documentTextOffset = textOffset; textOffset += static_cast<int>(block.inlineLayout->text.size()) + 1;
        const qreal top = node.type == MarkdownNodeType::Heading ? theme.blockGap * .65 : 0;
        block.rect = QRectF(indent, y + top, right - indent, block.inlineLayout->height); result.blocks.push_back(std::move(block)); y += top + result.blocks.back().rect.height() + theme.blockGap;
    }
}

DocumentLayout MarkdownLayoutEngine::layout(const MarkdownDocument& document, qreal width, const MarkdownTheme& theme,
                                            const QHash<QString, QImage>& images) const
{
    DocumentLayout result; result.width = qMax<qreal>(1, width); result.themeVersion = theme.version; int offset = 0;
    qreal y = theme.contentMargins.top();
    appendNodes(document.root().children, result, y, result.width, theme.contentMargins.left(), 0, 0, theme, offset, images);
    qreal maxContentWidth = 0;
    for (const auto& b : result.blocks) {
        if (b.inlineLayout) {
            maxContentWidth = qMax(maxContentWidth, b.contentX + b.inlineLayout->width + theme.contentMargins.right());
        } else {
            maxContentWidth = qMax(maxContentWidth, b.rect.right() + theme.contentMargins.right());
        }
    }
    const qreal totalHeight = result.blocks.isEmpty()
        ? (theme.contentMargins.top() + theme.contentMargins.bottom())
        : (result.blocks.back().rect.bottom() + theme.contentMargins.bottom());
    result.size = QSizeF(maxContentWidth > 0 ? qMin(result.width, maxContentWidth) : result.width, totalHeight);
    return result;
}

DocumentLayout MarkdownLayoutEngine::layout(const MarkdownDocument& stableDocument, const MarkdownDocument& activeTail,
                                            qreal width, const MarkdownTheme& theme,
                                            const QHash<QString, QImage>& images) const
{
    DocumentLayout result; result.width = qMax<qreal>(1, width); result.themeVersion = theme.version; int offset = 0;
    qreal y = theme.contentMargins.top();
    appendNodes(stableDocument.root().children, result, y, result.width, theme.contentMargins.left(), 0, 0, theme, offset, images);
    appendNodes(activeTail.root().children, result, y, result.width, theme.contentMargins.left(), 0, 0, theme, offset, images);
    qreal maxContentWidth = 0;
    for (const auto& b : result.blocks) {
        if (b.inlineLayout) {
            maxContentWidth = qMax(maxContentWidth, b.contentX + b.inlineLayout->width + theme.contentMargins.right());
        } else {
            maxContentWidth = qMax(maxContentWidth, b.rect.right() + theme.contentMargins.right());
        }
    }
    const qreal totalHeight = result.blocks.isEmpty()
        ? (theme.contentMargins.top() + theme.contentMargins.bottom())
        : (result.blocks.back().rect.bottom() + theme.contentMargins.bottom());
    result.size = QSizeF(maxContentWidth > 0 ? qMin(result.width, maxContentWidth) : result.width, totalHeight);
    return result;
}

} // namespace ui::markdown
