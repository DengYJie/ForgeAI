#include "MarkdownRenderer.h"

#include <QRegularExpression>
#include <md4c-html.h>

namespace core::markdown {

namespace {

void appendHtml(const MD_CHAR *text, MD_SIZE size, void *userdata)
{
    auto *output = static_cast<QByteArray *>(userdata);
    output->append(text, static_cast<qsizetype>(size));
}

} // namespace

QString MarkdownRenderer::renderFragment(const QString &markdown) const
{
    const QByteArray input = markdown.toUtf8();
    QByteArray output;

    unsigned parserFlags = MD_DIALECT_GITHUB | MD_FLAG_HARD_SOFT_BREAKS;
    if (!m_allowHtml) {
        parserFlags |= MD_FLAG_NOHTML;
    }

    const int result = md_html(input.constData(),
                               static_cast<MD_SIZE>(input.size()),
                               appendHtml,
                               &output,
                               parserFlags,
                               0);
    if (result != 0) {
        return QStringLiteral("<pre class=\"markdown-error\">")
               + markdown.toHtmlEscaped()
               + QStringLiteral("</pre>");
    }

    QString html = QString::fromUtf8(output);

    // md4c represents GFM task items as a normal list marker plus a disabled
    // input. Replace that input with a stylable, non-interactive marker so the
    // view can suppress the redundant bullet and use its own color scheme.
    static const QRegularExpression checkedLiRe(QStringLiteral(
        R"(<li>\s*<input\s+type="checkbox"\s+class="task-list-item-checkbox"\s+disabled\s+checked\s*>)"));
    static const QRegularExpression uncheckedLiRe(
        QStringLiteral(R"(<li>\s*<input\s+type="checkbox"\s+class="task-list-item-checkbox"\s+disabled\s*>)"));
    static const QRegularExpression checkedRe(
        QStringLiteral(R"(<input\s+type="checkbox"\s+class="task-list-item-checkbox"\s+disabled\s+checked\s*>)"));
    static const QRegularExpression uncheckedRe(
        QStringLiteral(R"(<input\s+type="checkbox"\s+class="task-list-item-checkbox"\s+disabled\s*>)"));

    html.replace(checkedLiRe,
                 QStringLiteral(
                     "<li class=\"task-list-item\"><span class=\"task-list-checkbox is-checked\">✓</span>"));
    html.replace(uncheckedLiRe,
                 QStringLiteral("<li class=\"task-list-item\"><span class=\"task-list-checkbox\"></span>"));
    html.replace(checkedRe, QStringLiteral("<span class=\"task-list-checkbox is-checked\">✓</span>"));
    html.replace(uncheckedRe, QStringLiteral("<span class=\"task-list-checkbox\"></span>"));

    return html;
}

QString MarkdownRenderer::wrapDocument(const QString &fragment, const QString &css) const
{
    return QStringLiteral("<!doctype html><html><head><meta charset=\"utf-8\"><style>%1</style></head>"
                          "<body><article class=\"markdown-body\">%2</article></body></html>")
        .arg(css, fragment);
}

} // namespace core::markdown
