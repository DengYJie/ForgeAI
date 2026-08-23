#include "WorkPage.h"

#include <QVBoxLayout>
#include <FluentQt/TextFields.h>

#include "ui/widget/CollapsibleSplitView.h"
#include "WorkViewModel.h"

namespace ui::screen::work {
    WorkPage::WorkPage(
        WorkViewModel *viewModel,
        QWidget *parent
    ) : BasePage(parent),
        m_viewModel(viewModel) {
        setupUi();
        if (m_viewModel) {
            m_viewModel->observe(this, &WorkPage::render);
        }
    }

    void WorkPage::setupUi() {
        m_rootLayout = new QVBoxLayout(this);
        m_rootLayout->setContentsMargins(0, 0, 0, 0);
        m_rootLayout->setSpacing(0);

        m_splitView = new ui::widget::CollapsibleSplitView(this);
        m_rootLayout->addWidget(m_splitView);

        // 1. 左侧工作流管理侧边栏 (可折叠面板)
        m_sidebarWidget = new QWidget(this);
        auto *sidebarLayout = new QVBoxLayout(m_sidebarWidget);
        sidebarLayout->setContentsMargins(16, 24, 16, 24);
        sidebarLayout->setSpacing(12);

        auto *sidebarTitle = new fluent::textfields::Label(tr("工作流管理"), m_sidebarWidget);
        sidebarTitle->setFluentTypography(Typography::FontRole::Subtitle);
        sidebarTitle->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
        sidebarLayout->addWidget(sidebarTitle);

        sidebarLayout->addStretch(1);

        m_splitView->addCollapsiblePane(
            m_sidebarWidget,
            ui::widget::SplitPaneDisplayMode::CompactInline,
            48,
            true,
            260);

        // 2. 右侧主工作区 (自适应填充)
        m_workAreaWidget = new QWidget(this);
        auto *workAreaLayout = new QVBoxLayout(m_workAreaWidget);
        workAreaLayout->setContentsMargins(36, 24, 36, 24);
        workAreaLayout->setSpacing(8);

        m_titleLabel = new fluent::textfields::Label(tr("工作"), m_workAreaWidget);
        m_titleLabel->setObjectName(QStringLiteral("workPageTitle"));
        m_titleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
        m_titleLabel->setFluentTypography(Typography::FontRole::Title);
        workAreaLayout->addWidget(m_titleLabel);

        m_subtitleLabel = new fluent::textfields::Label(tr("智能自动化工作流与任务协同执行中心"), m_workAreaWidget);
        m_subtitleLabel->setObjectName(QStringLiteral("workPageSubtitle"));
        m_subtitleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        m_subtitleLabel->setFluentTypography(Typography::FontRole::Body);
        workAreaLayout->addWidget(m_subtitleLabel);

        workAreaLayout->addStretch(1);

        fluent::collections::SplitViewPaneOptions workPaneOptions;
        workPaneOptions.fill = true;
        workPaneOptions.minimumSize = 300;
        m_splitView->addPane(m_workAreaWidget, workPaneOptions);
    }

    void WorkPage::render(const WorkState &state) {
        Q_UNUSED(state);
    }
} // namespace ui::screen::work
