// HTML serialization of the current text selection (DocumentContainer::
// selectedHtml): rebuilds the DOM path of each selected leaf element and
// inlines the computed styles so the copied fragment renders standalone.

#include "container_qpainter_p.h"
#include "container_internal.h"

#include <algorithm>
#include <string>

using namespace qlitehtml::internal;

// Helper function to get computed style properties from a litehtml element
static QString getElementStyles(const litehtml::element::ptr &element)
{
    if (!element)
        return QString();

    QStringList styles;

    // Font family/weight/style come from the resolved QFont the container
    // created for this element's computed style.
    const QFont font = toQFont(element->css().get_font());
    if (!font.family().isEmpty()) {
        styles << QStringLiteral("font-family: %1").arg(font.family());
    }

    // Get font size
    const int fontSize = qRound(element->css().get_font_size());
    if (fontSize > 0) {
        styles << QStringLiteral("font-size: %1px").arg(fontSize);
    }

    // Get font weight (Qt6 QFont::Weight matches the CSS 1-1000 scale)
    const int fontWeight = int(font.weight());
    if (fontWeight != 400) {
        styles << QStringLiteral("font-weight: %1").arg(fontWeight);
    }

    // Get font style
    if (font.italic()) {
        styles << QStringLiteral("font-style: italic");
    }

    // Get text decoration
    const int decorationLine = element->css().get_text_decoration_line();
    if (decorationLine != litehtml::text_decoration_line_none) {
        QStringList decorations;
        if (decorationLine & litehtml::text_decoration_line_underline)
            decorations << QStringLiteral("underline");
        if (decorationLine & litehtml::text_decoration_line_overline)
            decorations << QStringLiteral("overline");
        if (decorationLine & litehtml::text_decoration_line_line_through)
            decorations << QStringLiteral("line-through");
        if (!decorations.isEmpty())
            styles << QStringLiteral("text-decoration: %1").arg(decorations.join(QLatin1Char(' ')));
    }

    // Get color
    const litehtml::web_color color = element->css().get_color();
    if (color.alpha > 0) {
        styles << QStringLiteral("color: rgb(%1, %2, %3)")
                      .arg(color.red)
                      .arg(color.green)
                      .arg(color.blue);
    }

    // Get background color
    if (const litehtml::background *bg = element->get_background(false)) {
        const litehtml::web_color bgColor = bg->m_color;
        if (bgColor.alpha > 0) {
            styles << QStringLiteral("background-color: rgb(%1, %2, %3)")
                          .arg(bgColor.red)
                          .arg(bgColor.green)
                          .arg(bgColor.blue);
        }
    }

    // Get text alignment
    switch (element->css().get_text_align()) {
    case litehtml::text_align_right:
        styles << QStringLiteral("text-align: right");
        break;
    case litehtml::text_align_center:
        styles << QStringLiteral("text-align: center");
        break;
    case litehtml::text_align_justify:
        styles << QStringLiteral("text-align: justify");
        break;
    default:
        break;
    }

    return styles.join(QStringLiteral("; "));
}

// Helper structure to track parent elements during traversal
struct ElementContext
{
    litehtml::element::ptr element;
    QString tagName;
    QString styles;
    bool opened = false;
};

// Helper function to serialize a single leaf element with its parent hierarchy
static void serializeLeafWithParents(const litehtml::element::ptr &leafElement,
                                     QVector<ElementContext> &parentStack,
                                     QString &html)
{
    if (!leafElement)
        return;

    // Build the path from root to this leaf
    const std::vector<litehtml::element::ptr> leafPath = path(leafElement);

    // Find common ancestor with current stack
    int commonDepth = 0;
    const int minSize = std::min(static_cast<int>(parentStack.size()),
                                 static_cast<int>(leafPath.size()));
    for (int i = 0; i < minSize; ++i) {
        if (i >= parentStack.size() || parentStack[i].element != leafPath[i]) {
            break;
        }
        commonDepth = i + 1;
    }

    // Close tags that are no longer in the path
    for (int i = parentStack.size() - 1; i >= commonDepth; --i) {
        if (parentStack[i].opened && !parentStack[i].tagName.isEmpty()) {
            html += QStringLiteral("</") + parentStack[i].tagName + QStringLiteral(">");
        }
    }
    parentStack.resize(commonDepth);

    // Open new parent tags
    for (size_t i = commonDepth; i < leafPath.size(); ++i) {
        const litehtml::element::ptr &elem = leafPath[i];
        const char *tagName = elem->get_tagName();

        ElementContext ctx;
        ctx.element = elem;

        if (tagName && strlen(tagName) > 0) {
            QString tag = QString::fromUtf8(tagName);
            ctx.tagName = tag;

            // Skip html, head, body wrappers
            if (tag == QLatin1String("html") || tag == QLatin1String("head")
                || tag == QLatin1String("body")) {
                ctx.opened = false;
                parentStack.append(ctx);
                continue;
            }

            // Open tag
            html += QStringLiteral("<") + tag;

            // Get and add inline styles
            QString inlineStyles = getElementStyles(elem);
            if (!inlineStyles.isEmpty()) {
                html += QStringLiteral(" style=\"%1\"").arg(inlineStyles);
            }

            // Add certain important attributes
            if (tag == QLatin1String("a")) {
                const char *href = elem->get_attr("href");
                if (href && strlen(href) > 0) {
                    html += QStringLiteral(" href=\"%1\"").arg(QString::fromUtf8(href));
                }
            } else if (tag == QLatin1String("img")) {
                const char *src = elem->get_attr("src");
                if (src && strlen(src) > 0) {
                    html += QStringLiteral(" src=\"%1\"").arg(QString::fromUtf8(src));
                }
                const char *alt = elem->get_attr("alt");
                if (alt && strlen(alt) > 0) {
                    html += QStringLiteral(" alt=\"%1\"").arg(QString::fromUtf8(alt));
                }
            }

            html += QStringLiteral(">");
            ctx.opened = true;
        }

        parentStack.append(ctx);
    }

    // Add the text content of the leaf
    litehtml::string elemText;
    leafElement->get_text(elemText);
    if (!elemText.empty()) {
        QString text = QString::fromUtf8(elemText.data(), int(elemText.size()));
        // HTML escape the text
        text.replace(QLatin1Char('&'), QLatin1String("&amp;"));
        text.replace(QLatin1Char('<'), QLatin1String("&lt;"));
        text.replace(QLatin1Char('>'), QLatin1String("&gt;"));
        html += text;
    }
}

QString DocumentContainer::selectedHtml() const
{
    const Selection &sel = d->m_interactor.selection();
    if (!sel.startElem.element || !sel.endElem.element) {
        return QString();
    }

    // Get ordered start and end elements (same logic as Selection::update)
    Selection::Element start;
    Selection::Element end;
    std::tie(start, end) = getStartAndEnd(sel.startElem, sel.endElem);

    QString bodyHtml;
    QVector<ElementContext> parentStack;

    // Process start element
    serializeLeafWithParents(start.element, parentStack, bodyHtml);

    // If start and end are different, traverse all elements between them
    if (start.element != end.element) {
        litehtml::element::ptr current = start.element;
        do {
            current = nextLeaf(current, end.element);
            if (current && current != end.element) {
                serializeLeafWithParents(current, parentStack, bodyHtml);
            }
        } while (current && current != end.element);

        // Process end element
        if (end.element) {
            serializeLeafWithParents(end.element, parentStack, bodyHtml);
        }
    }

    // Close all remaining open tags
    for (int i = parentStack.size() - 1; i >= 0; --i) {
        if (parentStack[i].opened && !parentStack[i].tagName.isEmpty()) {
            bodyHtml += QStringLiteral("</") + parentStack[i].tagName + QStringLiteral(">");
        }
    }

    // Wrap in proper HTML document structure
    QString html;
    html += QStringLiteral("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"></head><body>");
    html += bodyHtml;
    html += QStringLiteral("</body></html>");

    return html;
}
