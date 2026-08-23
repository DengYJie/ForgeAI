#include "ErrorBlockWidget.h"
#include "FlatExpander.h"

#include <QAbstractTextDocumentLayout>
#include <QScrollBar>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <FluentQt/Design.h>

namespace ui::widget::message::blocks {

ErrorBlockWidget::ErrorBlockWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

ErrorBlockWidget::ErrorBlockWidget(const QString &summary, const QString &details, QWidget *parent)
    : QWidget(parent)
    , m_summary(summary)
    , m_details(details)
{
    setupUi();
    setError(summary, details);
}

ErrorBlockWidget::~ErrorBlockWidget() = default;

void ErrorBlockWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_expander = new FlatExpander(QStringLiteral("异常与错误诊断"), 26, this);
    m_expander->setLeadingIcon(Typography::Icons::glyph(QStringLiteral("ic_fluent_error_circle_20_regular")));
    m_expander->setChevronPosition(FlatExpander::ChevronPosition::InlineRight);
    m_expander->setHeaderCompact(true);
    m_expander->setExpanded(false, false);

    m_container = new QWidget(m_expander);
    auto *containerLayout = new QVBoxLayout(m_container);
    containerLayout->setContentsMargins(14, 2, 14, 14);
    containerLayout->setSpacing(0);

    m_detailsBrowser = new QTextBrowser(m_container);
    m_detailsBrowser->setReadOnly(true);
    m_detailsBrowser->setFont(QFont(QStringLiteral("Consolas"), 9));
    m_detailsBrowser->setFrameShape(QFrame::NoFrame);
    m_detailsBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_detailsBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_detailsBrowser->document()->setDocumentMargin(0);

    QPalette pal = m_detailsBrowser->palette();
    pal.setColor(QPalette::Base, Qt::transparent);
    m_detailsBrowser->setPalette(pal);
    m_detailsBrowser->viewport()->setPalette(pal);
    m_detailsBrowser->setAttribute(Qt::WA_TranslucentBackground);
    m_detailsBrowser->viewport()->setAttribute(Qt::WA_TranslucentBackground);

    m_detailsBrowser->setMinimumHeight(30);
    m_detailsBrowser->setMaximumHeight(260);

    connect(m_detailsBrowser->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this, [this](const QSizeF& size) {
        if (!m_detailsBrowser->isVisible()) return;
        const int targetH = qBound(30, qRound(size.height()) + 8, 260);
        if (m_detailsBrowser->height() != targetH) {
            m_detailsBrowser->setFixedHeight(targetH);
            if (m_expander && m_expander->isExpanded()) {
                m_expander->forceUpdateContentHeight();
            }
            updateGeometry();
            emit contentHeightChanged();
        }
    });

    containerLayout->addWidget(m_detailsBrowser);
    m_expander->setContentWidget(m_container);

    connect(m_expander, &FlatExpander::contentHeightChanged, this, [this]() {
        updateGeometry();
        emit contentHeightChanged();
    });

    connect(m_expander, &FlatExpander::expandedChanged, this, [this](bool) {
        updateGeometry();
        emit contentHeightChanged();
    });

    layout->addWidget(m_expander);
    setError(QStringLiteral("出现异常错误"), QString{});
}

void ErrorBlockWidget::setError(const QString &summary, const QString &details)
{
    m_summary = summary;
    m_details = details;

    if (m_expander) {
        m_expander->setTitle(QStringLiteral("错误诊断: %1").arg(summary));
    }
    if (m_detailsBrowser) {
        const bool isDark = (effectiveTheme() == fluent::FluentElement::Dark);
        const QString errColor = isDark ? QStringLiteral("#f85149") : QStringLiteral("#cf222e");
        QString escaped = details.toHtmlEscaped();
        escaped.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
        m_detailsBrowser->setHtml(QStringLiteral("<div style=\"color:%1; font-family:'Menlo','Monaco','Consolas',monospace; font-size:12px; line-height:1.45; white-space:pre;\">%2</div>")
            .arg(errColor, escaped));

        const int docH = qRound(m_detailsBrowser->document()->documentLayout()->documentSize().height());
        const int targetH = qBound(30, docH + 8, 260);
        m_detailsBrowser->setFixedHeight(targetH);
    }

    if (m_expander && m_expander->isExpanded()) {
        m_expander->forceUpdateContentHeight();
    }
    updateGeometry();
    emit contentHeightChanged();
}

void ErrorBlockWidget::setExpanded(bool expanded)
{
    if (m_expander) {
        m_expander->setExpanded(expanded);
    }
}

bool ErrorBlockWidget::isExpanded() const
{
    return m_expander ? m_expander->isExpanded() : false;
}

void ErrorBlockWidget::onThemeUpdated()
{
    if (m_expander) {
        m_expander->onThemeUpdated();
    }
    if (!m_details.isEmpty()) {
        setError(m_summary, m_details);
    }
}

void ErrorBlockWidget::updateVisuals()
{
    onThemeUpdated();
}

} // namespace ui::widget::message::blocks
