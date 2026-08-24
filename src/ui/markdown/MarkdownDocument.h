#pragma once

#include <QString>
#include <QVector>
#include <memory>
#include <vector>

namespace ui::markdown {

enum class MarkdownNodeType {
    Document, Paragraph, Heading, Text, SoftBreak, HardBreak, Emphasis, Strong,
    Strikethrough, InlineCode, Link, Image, CodeBlock, BlockQuote, List,
    ListItem, ThematicBreak, Table, TableRow, TableCell, Html, Unknown
};

struct SourceRange {
    int startLine = 0;
    int startColumn = 0;
    int endLine = 0;
    int endColumn = 0;
};

struct MarkdownAttributes {
    int headingLevel = 0;
    int listStart = 1;
    bool orderedList = false;
    bool taskListItem = false;
    bool taskChecked = false;
    bool tableHeader = false;
    QString url;
    QString title;
    QString fenceInfo;
};

class MarkdownNode final {
public:
    MarkdownNodeType type = MarkdownNodeType::Unknown;
    QString literal;
    MarkdownAttributes attributes;
    SourceRange sourceRange;
    std::vector<std::unique_ptr<MarkdownNode>> children;

    MarkdownNode() = default;
    explicit MarkdownNode(MarkdownNodeType nodeType) : type(nodeType) {}
    MarkdownNode(const MarkdownNode&) = delete;
    MarkdownNode& operator=(const MarkdownNode&) = delete;
};

class MarkdownDocument final {
public:
    MarkdownDocument();
    ~MarkdownDocument();
    MarkdownDocument(MarkdownDocument&&) noexcept;
    MarkdownDocument& operator=(MarkdownDocument&&) noexcept;
    MarkdownDocument(const MarkdownDocument&) = delete;
    MarkdownDocument& operator=(const MarkdownDocument&) = delete;

    const MarkdownNode& root() const noexcept;
    const QString& source() const noexcept;
    bool isEmpty() const noexcept;
    void append(MarkdownDocument&& fragment);

private:
    friend class MarkdownParser;
    QString m_source;
    std::unique_ptr<MarkdownNode> m_root;
};

class MarkdownParser final {
public:
    MarkdownDocument parse(const QString& markdown) const;
};

} // namespace ui::markdown
