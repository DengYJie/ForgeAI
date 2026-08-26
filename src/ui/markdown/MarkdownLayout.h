#pragma once

#include "MarkdownDocument.h"
#include "MarkdownTheme.h"
#include "ui/markdown/syntax/MarkdownSyntaxHighlighter.h"

#include <QHash>
#include <QImage>
#include <QRectF>
#include <QTextLayout>
#include <memory>

namespace ui::markdown {

struct LinkRange { int start = 0; int length = 0; QString url; };
struct CodeSpanRange { int start = 0; int length = 0; };

struct InlineLayout {
    QString text;
    QTextLayout layout;
    QVector<LinkRange> links;
    QVector<CodeSpanRange> codeSpans;
    qreal availableWidth = 0;
    qreal usedWidth = 0;
    qreal height = 0;

    InlineLayout(QString value, const QFont& baseFont, const QVector<QTextLayout::FormatRange>& formats,
                 QVector<LinkRange> linkRanges, QVector<CodeSpanRange> codeRanges, qreal availableWidth, bool wrap = true);
    InlineLayout(QString value, const QFont& baseFont, const QVector<QTextLayout::FormatRange>& formats,
                 QVector<LinkRange> linkRanges, qreal availableWidth, bool wrap = true);
    int cursorAt(qreal x, qreal y) const;
};

struct PreparedInline {
    QString text;
    QVector<QTextLayout::FormatRange> formats;
    QVector<LinkRange> links;
    QVector<CodeSpanRange> codeSpans;
    QFont font;
    bool hasExplicitLineBreaks = false;
    std::shared_ptr<InlineLayout> intrinsicLayout;
    qreal intrinsicWidth = 0;
    qreal intrinsicHeight = 0;
};

struct CodeBlockContentLayout {
    QVector<std::shared_ptr<InlineLayout>> lines;
    QVector<int> lineOffsets;
    qreal contentWidth = 0;
    qreal contentHeight = 0;
};

struct TableIntrinsicData {
    int columns = 0;
    QVector<qreal> preferredWidths;
    QVector<QVector<std::shared_ptr<InlineLayout>>> intrinsicCells;
    QVector<bool> headerRows;
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
    QSizeF imageIntrinsicSize;
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

struct MarkdownLayoutMetrics {
    quint64 totalLayouts = 0;
    quint64 inlineLayoutCount = 0;
    quint64 intrinsicFastPathHits = 0;
    quint64 codeBlockCacheHits = 0;
    quint64 codeBlockCacheMisses = 0;
    quint64 tableCellFastPathHits = 0;
    quint64 tableCellWrappedCount = 0;
    qint64 lastLayoutUs = 0;
};

class MarkdownLayoutEngine final {
public:
    DocumentLayout layout(const MarkdownDocument& document, qreal width, const MarkdownTheme& theme,
                          const QHash<QString, QImage>& images = {}) const;
    DocumentLayout layout(const MarkdownDocument& stableDocument, const MarkdownDocument& activeTail,
                          qreal width, const MarkdownTheme& theme,
                          const QHash<QString, QImage>& images = {}) const;

    void clearCache();
    MarkdownLayoutMetrics metrics() const noexcept;
    void resetMetrics() noexcept;

private:
    void appendNodes(const std::vector<std::unique_ptr<MarkdownNode>>& nodes, DocumentLayout& result,
                     qreal& y, qreal width, qreal indent, int quoteDepth, int listDepth,
                     const MarkdownTheme& theme, int& textOffset,
                     const QHash<QString, QImage>& images) const;

    PreparedInline prepareInline(const MarkdownNode& node, const QFont& font,
                                const MarkdownTheme& theme) const;
    const PreparedInline& prepareCachedInline(const MarkdownNode& node, const QFont& font,
                                              const MarkdownTheme& theme) const;
    std::shared_ptr<InlineLayout> layoutInline(const PreparedInline& prepared,
                                               qreal availableWidth, bool wrap = true) const;
    std::shared_ptr<InlineLayout> makeIntrinsicInline(const MarkdownNode& node, const QFont& font,
                                                      const MarkdownTheme& theme) const;
    std::shared_ptr<InlineLayout> makeInline(const MarkdownNode& node, const QFont& font,
                                             const MarkdownTheme& theme, qreal width) const;

    MarkdownSyntaxHighlighter m_syntaxHighlighter;

    struct CodeCacheKey {
        quint64 themeVersion = 0;
        QString language;
        size_t codeHash = 0;
        bool operator==(const CodeCacheKey& o) const {
            return themeVersion == o.themeVersion && language == o.language && codeHash == o.codeHash;
        }
    };
    friend inline size_t qHash(const CodeCacheKey& k, size_t seed = 0) noexcept {
        return qHashMulti(seed, k.themeVersion, k.language, k.codeHash);
    }

    struct TableCacheKey {
        const MarkdownNode* node = nullptr;
        quint64 themeVersion = 0;
        bool operator==(const TableCacheKey& o) const {
            return node == o.node && themeVersion == o.themeVersion;
        }
    };
    friend inline size_t qHash(const TableCacheKey& k, size_t seed = 0) noexcept {
        return qHashMulti(seed, k.node, k.themeVersion);
    }

    struct InlineCacheKey {
        const MarkdownNode* node = nullptr;
        quint64 themeVersion = 0;
        int fontPixelSize = 0;
        int fontWeight = 0;
        bool operator==(const InlineCacheKey& o) const {
            return node == o.node && themeVersion == o.themeVersion &&
                   fontPixelSize == o.fontPixelSize && fontWeight == o.fontWeight;
        }
    };
    friend inline size_t qHash(const InlineCacheKey& k, size_t seed = 0) noexcept {
        return qHashMulti(seed, k.node, k.themeVersion, k.fontPixelSize, k.fontWeight);
    }

    mutable QHash<CodeCacheKey, CodeBlockContentLayout> m_codeBlockCache;
    mutable QHash<TableCacheKey, std::shared_ptr<TableIntrinsicData>> m_tableCache;
    mutable QHash<InlineCacheKey, PreparedInline> m_inlineCache;
    mutable MarkdownLayoutMetrics m_metrics;
};

} // namespace ui::markdown
