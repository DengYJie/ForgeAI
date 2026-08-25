#pragma once

#include "MarkdownDocument.h"
#include "MarkdownTheme.h"
#include "ui/markdown/syntax/MarkdownSyntaxHighlighter.h"

#include <QRectF>
#include <QTextLayout>
#include <memory>

namespace ui::markdown {

struct LinkRange { int start = 0; int length = 0; QString url; };

struct InlineLayout {
    QString text;
    QTextLayout layout;
    QVector<LinkRange> links;
    qreal height = 0;
    qreal width = 0;

    InlineLayout(QString value, const QFont& baseFont, const QVector<QTextLayout::FormatRange>& formats,
                 QVector<LinkRange> linkRanges, qreal availableWidth, bool wrap = true);
    int cursorAt(qreal x, qreal y) const;
};

enum class BlockKind { Paragraph, Heading, Rule, QuoteContent, ListItem, CodeBlock, Table, Image, Html };

struct TableLayoutData {
    QVector<qreal> columnWidths;
    QVector<qreal> rowHeights;
    QVector<QVector<std::shared_ptr<InlineLayout>>> cells;
    QVector<bool> headerRows;
};

struct BlockScrollOffset {
    qreal x = 0;
    qreal y = 0;
};

struct BlockScrollInfo {
    QSizeF contentSize;
    QRectF viewportRect;
    QRectF hScrollBarRect;
    QRectF vScrollBarRect;
    bool hasHorizontalScroll() const { return contentSize.width() > viewportRect.width() + 1.0; }
    bool hasVerticalScroll() const { return contentSize.height() > viewportRect.height() + 1.0; }
    qreal maxScrollX() const { return qMax<qreal>(0, contentSize.width() - viewportRect.width()); }
    qreal maxScrollY() const { return qMax<qreal>(0, contentSize.height() - viewportRect.height()); }
};

struct BlockLayout {
    BlockKind kind = BlockKind::Paragraph;
    QRectF rect;
    qreal contentX = 0;
    qreal quoteIndent = 0;
    QString marker;
    bool taskItem = false;
    bool taskChecked = false;
    int taskSourceLine = 0;
    QRectF taskCheckRect;
    QString code;
    QString language;
    QRectF copyButtonRect;
    QString imageUrl;
    QString imageAlt;
    std::shared_ptr<InlineLayout> inlineLayout;
    QVector<std::shared_ptr<InlineLayout>> codeLines;
    QVector<int> codeLineOffsets;
    std::shared_ptr<TableLayoutData> table;
    BlockScrollInfo scrollInfo;
    int documentTextOffset = 0;
};

struct DocumentLayout {
    QSizeF size;
    QVector<BlockLayout> blocks;
    qreal width = 0;
    quint64 themeVersion = 0;
    int firstVisibleBlock(qreal y) const;
    int lastVisibleBlock(qreal y) const;
    int textLength() const;
};

class MarkdownLayoutEngine final {
public:
    DocumentLayout layout(const MarkdownDocument& document, qreal width, const MarkdownTheme& theme) const;
    DocumentLayout layout(const MarkdownDocument& stableDocument, const MarkdownDocument& activeTail,
                          qreal width, const MarkdownTheme& theme) const;

private:
    void appendNodes(const std::vector<std::unique_ptr<MarkdownNode>>& nodes, DocumentLayout& result,
                     qreal& y, qreal width, qreal indent, int quoteDepth, int listDepth,
                     const MarkdownTheme& theme, int& textOffset) const;
    std::shared_ptr<InlineLayout> makeInline(const MarkdownNode& node, const QFont& font,
                                              const MarkdownTheme& theme, qreal width) const;
    MarkdownSyntaxHighlighter m_syntaxHighlighter;
};

} // namespace ui::markdown
