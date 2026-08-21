#include "KnowledgePage.h"

#include <QVBoxLayout>
#include <FluentQt/TextFields.h>

#include "KnowledgeViewModel.h"

namespace ui::screen::knowledge {
    KnowledgePage::KnowledgePage(QWidget *parent)
        : QWidget(parent) {
        setupUi();
        setupViewModel();
    }

    void KnowledgePage::setupUi() {
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        setAutoFillBackground(false);

        m_rootLayout = new QVBoxLayout(this);
        m_rootLayout->setContentsMargins(36, 24, 36, 24);
        m_rootLayout->setSpacing(8);

        m_titleLabel = new fluent::textfields::Label(tr("知识库"), this);
        m_titleLabel->setObjectName(QStringLiteral("knowledgePageTitle"));
        m_titleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
        m_titleLabel->setFluentTypography(Typography::FontRole::Title);
        m_rootLayout->addWidget(m_titleLabel);

        m_subtitleLabel = new fluent::textfields::Label(tr("结构化本地与云端知识库与向量检索管理"), this);
        m_subtitleLabel->setObjectName(QStringLiteral("knowledgePageSubtitle"));
        m_subtitleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        m_subtitleLabel->setFluentTypography(Typography::FontRole::Body);
        m_rootLayout->addWidget(m_subtitleLabel);

        m_rootLayout->addStretch(1);
    }

    void KnowledgePage::setupViewModel() {
        m_viewModel = new KnowledgeViewModel(this);
        m_viewModel->observe(this, &KnowledgePage::render);
    }

    void KnowledgePage::render(const KnowledgeState &state) {
        Q_UNUSED(state);
    }

    void KnowledgePage::onThemeUpdated() {
        QWidget::update();
    }
} // namespace ui::screen::knowledge
