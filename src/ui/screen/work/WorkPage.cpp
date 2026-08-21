#include "WorkPage.h"

#include <QVBoxLayout>
#include <FluentQt/TextFields.h>

#include "WorkViewModel.h"

namespace ui::screen::work {
    WorkPage::WorkPage(QWidget *parent)
        : QWidget(parent) {
        setupUi();
        setupViewModel();
    }

    void WorkPage::setupUi() {
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        setAutoFillBackground(false);

        m_rootLayout = new QVBoxLayout(this);
        m_rootLayout->setContentsMargins(36, 24, 36, 24);
        m_rootLayout->setSpacing(8);

        m_titleLabel = new fluent::textfields::Label(tr("工作"), this);
        m_titleLabel->setObjectName(QStringLiteral("workPageTitle"));
        m_titleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
        m_titleLabel->setFluentTypography(Typography::FontRole::Title);
        m_rootLayout->addWidget(m_titleLabel);

        m_subtitleLabel = new fluent::textfields::Label(tr("智能自动化工作流与任务协同执行中心"), this);
        m_subtitleLabel->setObjectName(QStringLiteral("workPageSubtitle"));
        m_subtitleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        m_subtitleLabel->setFluentTypography(Typography::FontRole::Body);
        m_rootLayout->addWidget(m_subtitleLabel);

        m_rootLayout->addStretch(1);
    }

    void WorkPage::setupViewModel() {
        m_viewModel = new WorkViewModel(this);
        m_viewModel->observe(this, &WorkPage::render);
    }

    void WorkPage::render(const WorkState &state) {
        Q_UNUSED(state);
    }

    void WorkPage::onThemeUpdated() {
        QWidget::update();
    }
} // namespace ui::screen::work
