#include "AbstractToolBlockWidget.h"
#include "FlatExpander.h"

#include <QVBoxLayout>
#include <QTextBrowser>
#include <QAbstractTextDocumentLayout>
#include <FluentQt/Design.h>
#include <FluentQt/Layout.h>
#include <FluentQt/TextFields.h>

namespace ui::widget::message::blocks {

AbstractToolBlockWidget::AbstractToolBlockWidget(QWidget* parent)
    : QWidget(parent)
{
}

AbstractToolBlockWidget::AbstractToolBlockWidget(const domain::agent::ToolCall& call, QWidget* parent)
    : QWidget(parent)
    , m_call(call)
{
}

AbstractToolBlockWidget::~AbstractToolBlockWidget() = default;

void AbstractToolBlockWidget::initBaseUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_expander = new FlatExpander(QStringLiteral("工具调用"), 26, this);
    m_expander->setExpanded(false, false);

    m_contentWidget = createContentWidget(m_expander);
    if (m_contentWidget) {
        m_expander->setContentWidget(m_contentWidget);
    }

    connect(m_expander, &FlatExpander::contentHeightChanged, this, [this]() {
        updateGeometry();
        emit contentHeightChanged();
    });

    connect(m_expander, &FlatExpander::expandedChanged, this, [this](bool) {
        updateGeometry();
        emit contentHeightChanged();
    });

    layout->addWidget(m_expander);
    updateHeader();
    onThemeUpdated();
}

QString AbstractToolBlockWidget::customToolIcon(const QString& toolName) const
{
    const QString lower = toolName.toLower();
    if (lower.contains("bash") || lower.contains("cmd") || lower.contains("terminal") || lower.contains("exec") || lower.contains("command")) {
        return Typography::Icons::glyph(QStringLiteral("ic_fluent_window_console_20_regular"));
    }
    if (lower.contains("file") || lower.contains("read") || lower.contains("edit") || lower.contains("write")) {
        return Typography::Icons::glyph(QStringLiteral("ic_fluent_document_20_regular"));
    }
    if (lower.contains("search") || lower.contains("grep") || lower.contains("find") || lower.contains("glob")) {
        return Typography::Icons::glyph(QStringLiteral("ic_fluent_search_20_regular"));
    }
    if (lower.contains("web") || lower.contains("browse") || lower.contains("url") || lower.contains("http")) {
        return Typography::Icons::glyph(QStringLiteral("ic_fluent_globe_20_regular"));
    }
    return Typography::Icons::glyph(QStringLiteral("ic_fluent_wrench_20_regular"));
}

QString AbstractToolBlockWidget::customToolTitle(const QString& name, const QString& arguments) const
{
    const QString lower = name.toLower();
    if (lower.contains("bash") || lower.contains("cmd") || lower.contains("run_command")) {
        if (arguments.contains("ls") || arguments.contains("dir")) {
            return QStringLiteral("查看文件列表");
        }
        if (arguments.contains("cat") || arguments.contains("view_file") || arguments.contains("Get-Content")) {
            return QStringLiteral("查看文件内容");
        }
        if (arguments.contains("git")) {
            return QStringLiteral("执行 Git 操作");
        }
        if (arguments.contains("test") || arguments.contains("ctest") || arguments.contains("ninja") || arguments.contains("cmake")) {
            return QStringLiteral("执行构建与测试");
        }
        QString clean = arguments;
        clean.remove('\n').remove('\"');
        if (clean.length() > 30) clean = clean.left(27) + QStringLiteral("...");
        return QStringLiteral("执行终端命令: %1").arg(clean);
    }
    if (lower.contains("view_file") || lower.contains("read")) {
        return QStringLiteral("读取文件");
    }
    if (lower.contains("edit") || lower.contains("replace") || lower.contains("write")) {
        return QStringLiteral("修改文件");
    }
    if (lower.contains("search") || lower.contains("grep")) {
        return QStringLiteral("代码/文件检索");
    }
    if (lower.contains("web") || lower.contains("browse")) {
        return QStringLiteral("网络检索");
    }
    return name;
}

void AbstractToolBlockWidget::updateHeader()
{
    if (!m_expander) return;

    QString icon = customToolIcon(m_call.name);
    QString title = customToolTitle(m_call.name, m_call.arguments);

    m_expander->setLeadingIcon(icon);
    m_expander->setTitle(title);
    m_expander->setSubtitle(m_status == Status::Running ? QStringLiteral("· 运行中") : (m_status == Status::Error ? QStringLiteral("· 失败") : QString{}));
    m_expander->setChevronPosition(FlatExpander::ChevronPosition::InlineRight);
    m_expander->setHeaderCompact(true);
}

void AbstractToolBlockWidget::setToolCall(const domain::agent::ToolCall& call)
{
    if (m_call.id == call.id && m_call.name == call.name && m_call.arguments == call.arguments) {
        return;
    }
    m_call = call;
    onCallUpdated(call);
    setStatus(Status::Running);
}

void AbstractToolBlockWidget::setToolResult(const domain::agent::ToolResult& result)
{
    if (m_result.toolCallId == result.toolCallId && m_result.content == result.content && m_result.isError == result.isError) {
        return;
    }
    m_result = result;
    onResultUpdated(result);
    setStatus(result.isError ? Status::Error : Status::Success);
}

void AbstractToolBlockWidget::setStatus(Status status)
{
    m_status = status;
    updateHeader();
    onThemeUpdated();
}

void AbstractToolBlockWidget::setExpanded(bool expanded, bool animated)
{
    if (m_expander) {
        m_expander->setExpanded(expanded, animated);
    }
}

bool AbstractToolBlockWidget::isExpanded() const
{
    return m_expander ? m_expander->isExpanded() : false;
}

void AbstractToolBlockWidget::onThemeUpdated()
{
    if (m_expander) {
        m_expander->onThemeUpdated();
    }
}

QWidget* AbstractToolBlockWidget::createStandardContentWidget(QWidget* parent, const QString& section1Title, const QString& section2Title)
{
    m_cardSurface = new fluent::layout::Card(parent);
    m_cardSurface->setAppearance(fluent::layout::Card::LayerAlt);
    auto* contentLayout = new QVBoxLayout(m_cardSurface);
    contentLayout->setContentsMargins(14, 12, 14, 14);
    contentLayout->setSpacing(6);

    // Section 1
    m_section1Label = new fluent::textfields::Label(section1Title, m_cardSurface);
    m_section1Label->setFluentTypography(Typography::FontRole::Caption);
    m_section1Label->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
    contentLayout->addWidget(m_section1Label);

    m_section1Card = new fluent::layout::Card(m_cardSurface);
    m_section1Card->setAppearance(fluent::layout::Card::Layer);
    auto* pillLayout = new QVBoxLayout(m_section1Card);
    pillLayout->setContentsMargins(10, 6, 10, 6);
    pillLayout->setSpacing(0);

    m_section1Text = new fluent::textfields::Label(m_section1Card);
    m_section1Text->setFont(QFont(QStringLiteral("Consolas"), 9));
    m_section1Text->setWordWrap(true);
    m_section1Text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pillLayout->addWidget(m_section1Text);
    contentLayout->addWidget(m_section1Card);

    // Section 2
    m_section2Label = new fluent::textfields::Label(section2Title, m_cardSurface);
    m_section2Label->setFluentTypography(Typography::FontRole::Caption);
    m_section2Label->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
    contentLayout->addWidget(m_section2Label);

    m_section2Browser = new QTextBrowser(m_cardSurface);
    m_section2Browser->setReadOnly(true);
    m_section2Browser->setFont(QFont(QStringLiteral("Consolas"), 9));
    m_section2Browser->setFrameShape(QFrame::NoFrame);
    m_section2Browser->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_section2Browser->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_section2Browser->document()->setDocumentMargin(0);

    QPalette pal = m_section2Browser->palette();
    pal.setColor(QPalette::Base, Qt::transparent);
    m_section2Browser->setPalette(pal);
    m_section2Browser->viewport()->setPalette(pal);
    m_section2Browser->setAttribute(Qt::WA_TranslucentBackground);
    m_section2Browser->viewport()->setAttribute(Qt::WA_TranslucentBackground);

    m_section2Browser->setMinimumHeight(30);
    m_section2Browser->setMaximumHeight(260);

    connect(m_section2Browser->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this, [this](const QSizeF& size) {
        if (!m_section2Browser->isVisible()) return;
        const int targetH = qBound(30, qRound(size.height()) + 8, 260);
        if (m_section2Browser->height() != targetH) {
            m_section2Browser->setFixedHeight(targetH);
            if (m_expander && m_expander->isExpanded()) {
                m_expander->forceUpdateContentHeight();
            }
            updateGeometry();
            emit contentHeightChanged();
        }
    });

    contentLayout->addWidget(m_section2Browser);

    return m_cardSurface;
}

void AbstractToolBlockWidget::updateStandardSection1(const QString& html)
{
    if (!html.isEmpty()) {
        m_section1Label->setVisible(true);
        m_section1Text->setText(html);
        m_section1Card->setVisible(true);
    } else {
        m_section1Label->setVisible(false);
        m_section1Card->setVisible(false);
    }
}

void AbstractToolBlockWidget::updateStandardSection2(const QString& html)
{
    if (!html.isEmpty()) {
        m_section2Label->setVisible(true);
        m_section2Browser->setHtml(html);
        m_section2Browser->setVisible(true);

        const int docH = qRound(m_section2Browser->document()->documentLayout()->documentSize().height());
        const int targetH = qBound(30, docH + 8, 260);
        m_section2Browser->setFixedHeight(targetH);
    } else {
        m_section2Label->setVisible(false);
        m_section2Browser->setVisible(false);
    }
}

} // namespace ui::widget::message::blocks
