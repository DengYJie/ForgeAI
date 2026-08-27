#pragma once

#include "MarkdownDocument.h"
#include "MarkdownSnapshot.h"
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
    QVector<QVector<int>> cellDisplayTextOffsets;
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
    BlockId blockId = 0;
    ElementId elementId = 0;
    BlockKind kind = BlockKind::Paragraph;
    QRectF rect;
    qreal contentX = 0;
    qreal quoteIndent = 0;
    QString marker;
    bool taskItem = false;
    bool taskChecked = false;
    int taskSourceLine = 0;
    SourceRange taskMarkerRange;
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
    int displayTextOffset = 0;

    // Translate every geometry field when placing a cached block-local layout.
    void translate(qreal dx, qreal dy)
    {
        rect.translate(dx, dy);
        copyButtonRect.translate(dx, dy);
        if (taskItem && taskCheckRect.isValid())
            taskCheckRect.translate(dx, dy);
        scrollInfo.viewportRect.translate(dx, dy);
        scrollInfo.hScrollBarRect.translate(dx, dy);
        scrollInfo.vScrollBarRect.translate(dx, dy);
    }
};

struct DocumentLayout;
using DocumentLayoutPtr = std::shared_ptr<const DocumentLayout>;

struct DocumentLayout {
    struct SemanticPlacement {
        BlockId id = 0;
        QRectF rect;
        int firstFragment = 0;
        int fragmentCount = 0;
    };

    SourceRevision documentRevision = 0;
    QSizeF size;
    QVector<BlockLayout> blocks;
    QVector<SemanticPlacement> semanticBlocks;
    qreal width = 0;
    quint64 themeVersion = 0;
    // The y-cursor value at which appendNodes() finished writing blocks.
    // This is the authoritative "where the next block should start" cursor,
    // more reliable than inferring it from blocks.back().rect.bottom().
    qreal contentEndY = 0;

    int firstVisibleBlock(qreal y) const;
    int lastVisibleBlock(qreal y) const;
    int textLength() const;
    QString displayText() const;

    int blockCount() const { return blocks.size(); }
    const BlockLayout& blockAt(int index) const { return blocks.at(index); }
    const BlockLayout* begin() const { return blocks.begin(); }
    const BlockLayout* end() const { return blocks.end(); }
};

using PaintFragment = BlockLayout;
using LayoutSnapshot = DocumentLayout;

struct MarkdownLayoutMetrics {
    quint64 totalLayouts = 0;
    quint64 inlineLayoutCount = 0;
    quint64 intrinsicFastPathHits = 0;
    quint64 codeBlockCacheHits = 0;
    quint64 codeBlockCacheMisses = 0;
    quint64 tableCellFastPathHits = 0;
    quint64 tableCellWrappedCount = 0;
    quint64 blockCacheHits = 0;
    quint64 blockCacheMisses = 0;
    quint64 blockCacheEvictions = 0;
    qsizetype blockCacheEntries = 0;
    qsizetype blockCacheEstimatedBytes = 0;
    qsizetype blockCacheLimitBytes = 0;
    quint64 placementCount = 0;
    qint64 lastLayoutUs = 0;
};

class MarkdownLayoutEngine final {
public:
    DocumentLayout layout(const MarkdownDocument& document, qreal width, const MarkdownTheme& theme,
                          const QHash<QString, QImage>& images = {}) const;
    DocumentLayout layout(const DocumentSnapshot& document, qreal width, const MarkdownTheme& theme,
                          const QHash<QString, QImage>& images = {}) const;

    void clearCache();
    MarkdownLayoutMetrics metrics() const noexcept;
    void resetMetrics() noexcept;

    void removeBlocks(const QVector<BlockId>& ids);

private:
    void appendNodes(const std::vector<std::unique_ptr<MarkdownNode>>& nodes, DocumentLayout& result,
                     qreal& y, qreal width, qreal indent, int quoteDepth, int listDepth,
                     const MarkdownTheme& theme, int& textOffset,
                     const QHash<QString, QImage>& images, quint64 generation) const;

    PreparedInline prepareInline(const MarkdownNode& node, const QFont& font,
                                const MarkdownTheme& theme) const;
    const PreparedInline& prepareCachedInline(const MarkdownNode& node, const QFont& font,
                                              const MarkdownTheme& theme, quint64 generation) const;
    std::shared_ptr<InlineLayout> layoutInline(const PreparedInline& prepared,
                                               qreal availableWidth, bool wrap = true) const;
    std::shared_ptr<InlineLayout> makeIntrinsicInline(const MarkdownNode& node, const QFont& font,
                                                      const MarkdownTheme& theme, quint64 generation) const;
    std::shared_ptr<InlineLayout> makeInline(const MarkdownNode& node, const QFont& font,
                                             const MarkdownTheme& theme, qreal width, quint64 generation) const;

    MarkdownSyntaxHighlighter m_syntaxHighlighter;
    struct CodeCacheKey {
        quint64 themeVersion = 0;
        QString language;
        QString source;
        bool operator==(const CodeCacheKey& o) const {
            return themeVersion == o.themeVersion && language == o.language && source == o.source;
        }
    };
    friend inline size_t qHash(const CodeCacheKey& k, size_t seed = 0) noexcept {
        return qHashMulti(seed, k.themeVersion, k.language, k.source);
    }

    struct TableCacheKey {
        quint64 nodeHash = 0;
        quint64 themeVersion = 0;
        int rowCount = 0;
        int totalCols = 0;
        bool operator==(const TableCacheKey& o) const {
            return nodeHash == o.nodeHash && themeVersion == o.themeVersion &&
                   rowCount == o.rowCount && totalCols == o.totalCols;
        }
    };
    friend inline size_t qHash(const TableCacheKey& k, size_t seed = 0) noexcept {
        return qHashMulti(seed, k.nodeHash, k.themeVersion, k.rowCount, k.totalCols);
    }

    struct InlineCacheKey {
        quint64 nodeHash = 0;
        quint64 themeVersion = 0;
        int fontPixelSize = 0;
        int fontWeight = 0;
        int childCount = 0;
        int literalLength = 0;
        bool operator==(const InlineCacheKey& o) const {
            return nodeHash == o.nodeHash && themeVersion == o.themeVersion &&
                   fontPixelSize == o.fontPixelSize && fontWeight == o.fontWeight &&
                   childCount == o.childCount && literalLength == o.literalLength;
        }
    };
    friend inline size_t qHash(const InlineCacheKey& k, size_t seed = 0) noexcept {
        return qHashMulti(seed, k.nodeHash, k.themeVersion, k.fontPixelSize, k.fontWeight, k.childCount, k.literalLength);
    }

    struct BlockCacheKey {
        BlockId blockId = 0;
        quint64 semanticHash = 0;
        quint64 dependencyRevision = 0;
        qint64 width64 = 0;
        quint64 themeVersion = 0;
        quint64 resourceFingerprint = 0;
        bool operator==(const BlockCacheKey&) const = default;
    };
    friend inline size_t qHash(const BlockCacheKey& k, size_t seed = 0) noexcept {
        return qHashMulti(seed, k.blockId, k.semanticHash, k.dependencyRevision,
                          k.width64, k.themeVersion, k.resourceFingerprint);
    }
    struct BlockCacheEntry {
        DocumentLayout local;
        qsizetype estimatedCost = 0;
        quint64 accessTick = 0;
    };

    mutable QHash<CodeCacheKey, CodeBlockContentLayout> m_codeBlockCache;
    mutable QHash<TableCacheKey, std::shared_ptr<TableIntrinsicData>> m_tableCache;
    mutable QHash<InlineCacheKey, PreparedInline> m_inlineCache;
    mutable QHash<BlockCacheKey, BlockCacheEntry> m_blockCache;
    mutable qsizetype m_blockCacheCost = 0;
    mutable quint64 m_accessTick = 0;
    static constexpr qsizetype MaxBlockCacheCost = 64 * 1024 * 1024;
    mutable MarkdownLayoutMetrics m_metrics;
};

} // namespace ui::markdown
