#include "MarkdownDocument.h"

#include <cmark-gfm.h>
#include <cmark-gfm-core-extensions.h>
#include <cmark-gfm-extension_api.h>

namespace ui::markdown {
namespace {

MarkdownNodeType nodeType(cmark_node* node)
{
    const auto type = cmark_node_get_type(node);
    if (type == CMARK_NODE_DOCUMENT) return MarkdownNodeType::Document;
    if (type == CMARK_NODE_PARAGRAPH) return MarkdownNodeType::Paragraph;
    if (type == CMARK_NODE_HEADING) return MarkdownNodeType::Heading;
    if (type == CMARK_NODE_TEXT) return MarkdownNodeType::Text;
    if (type == CMARK_NODE_SOFTBREAK) return MarkdownNodeType::SoftBreak;
    if (type == CMARK_NODE_LINEBREAK) return MarkdownNodeType::HardBreak;
    if (type == CMARK_NODE_EMPH) return MarkdownNodeType::Emphasis;
    if (type == CMARK_NODE_STRONG) return MarkdownNodeType::Strong;
    if (type == CMARK_NODE_CODE) return MarkdownNodeType::InlineCode;
    if (type == CMARK_NODE_LINK) return MarkdownNodeType::Link;
    if (type == CMARK_NODE_IMAGE) return MarkdownNodeType::Image;
    if (type == CMARK_NODE_CODE_BLOCK) return MarkdownNodeType::CodeBlock;
    if (type == CMARK_NODE_BLOCK_QUOTE) return MarkdownNodeType::BlockQuote;
    if (type == CMARK_NODE_LIST) return MarkdownNodeType::List;
    if (type == CMARK_NODE_ITEM) return MarkdownNodeType::ListItem;
    if (type == CMARK_NODE_THEMATIC_BREAK) return MarkdownNodeType::ThematicBreak;
    if (type == CMARK_NODE_HTML_INLINE || type == CMARK_NODE_HTML_BLOCK) return MarkdownNodeType::Html;

    const QString typeName = QString::fromLatin1(cmark_node_get_type_string(node));
    if (typeName == u"strikethrough") return MarkdownNodeType::Strikethrough;
    if (typeName == u"table") return MarkdownNodeType::Table;
    if (typeName == u"table_row") return MarkdownNodeType::TableRow;
    if (typeName == u"table_cell") return MarkdownNodeType::TableCell;
    return MarkdownNodeType::Unknown;
}

std::unique_ptr<MarkdownNode> adaptNode(cmark_node* source, bool allowHtml)
{
    auto result = std::make_unique<MarkdownNode>(nodeType(source));
    if (!allowHtml && result->type == MarkdownNodeType::Html)
        result->type = MarkdownNodeType::Text;
    if (const char* literal = cmark_node_get_literal(source))
        result->literal = QString::fromUtf8(literal);
    if (const char* url = cmark_node_get_url(source))
        result->attributes.url = QString::fromUtf8(url);
    if (const char* title = cmark_node_get_title(source))
        result->attributes.title = QString::fromUtf8(title);
    if (const char* info = cmark_node_get_fence_info(source))
        result->attributes.fenceInfo = QString::fromUtf8(info).trimmed();

    result->attributes.headingLevel = cmark_node_get_heading_level(source);
    result->attributes.listStart = cmark_node_get_list_start(source);
    result->attributes.orderedList = cmark_node_get_list_type(source) == CMARK_ORDERED_LIST;
    result->sourceRange = {cmark_node_get_start_line(source), cmark_node_get_start_column(source),
                           cmark_node_get_end_line(source), cmark_node_get_end_column(source)};
    if (result->type == MarkdownNodeType::ListItem) {
        result->attributes.taskListItem = QString::fromLatin1(cmark_node_get_type_string(source)) == u"tasklist";
        result->attributes.taskChecked = cmark_gfm_extensions_get_tasklist_item_checked(source);
    }
    if (result->type == MarkdownNodeType::TableRow)
        result->attributes.tableHeader = cmark_gfm_extensions_get_table_row_is_header(source);

    for (cmark_node* child = cmark_node_first_child(source); child; child = cmark_node_next(child))
        result->children.push_back(adaptNode(child, allowHtml));
    return result;
}

} // namespace

MarkdownDocument::MarkdownDocument() : m_root(std::make_unique<MarkdownNode>(MarkdownNodeType::Document)) {}
MarkdownDocument::~MarkdownDocument() = default;
MarkdownDocument::MarkdownDocument(MarkdownDocument&&) noexcept = default;
MarkdownDocument& MarkdownDocument::operator=(MarkdownDocument&&) noexcept = default;
const MarkdownNode& MarkdownDocument::root() const noexcept { return *m_root; }
const QString& MarkdownDocument::source() const noexcept { return m_source; }
bool MarkdownDocument::isEmpty() const noexcept { return m_root->children.empty(); }
void MarkdownDocument::append(MarkdownDocument&& fragment)
{
    m_source += fragment.m_source;
    for (auto& child : fragment.m_root->children) m_root->children.push_back(std::move(child));
    fragment.m_root->children.clear();
}

MarkdownDocument MarkdownParser::parse(const QString& markdown, const MarkdownParseOptions& options) const
{
    MarkdownDocument result;
    result.m_source = markdown;
    cmark_gfm_core_extensions_ensure_registered();
    cmark_parser* parser = cmark_parser_new(CMARK_OPT_DEFAULT | CMARK_OPT_VALIDATE_UTF8 | CMARK_OPT_SMART);
    for (const char* extension : {"table", "strikethrough", "autolink", "tasklist"})
        cmark_parser_attach_syntax_extension(parser, cmark_find_syntax_extension(extension));
    const QByteArray utf8 = markdown.toUtf8();
    cmark_parser_feed(parser, utf8.constData(), static_cast<size_t>(utf8.size()));
    cmark_node* root = cmark_parser_finish(parser);
    cmark_parser_free(parser);
    if (root) {
        result.m_root = adaptNode(root, options.allowHtml);
        cmark_node_free(root);
    }
    return result;
}

} // namespace ui::markdown
