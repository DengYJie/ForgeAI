#include "ChatPage.h"

#include <QVBoxLayout>
#include <FluentQt/TextFields.h>

#include "ChatViewModel.h"

namespace ui::screen::chat {
    ChatPage::ChatPage(QWidget *parent)
        : QWidget(parent) {
        setupUi();
        setupViewModel();
    }

    void ChatPage::setupUi() {
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        setAutoFillBackground(false);

        m_rootLayout = new QVBoxLayout(this);
        m_rootLayout->setContentsMargins(36, 24, 36, 24);
        m_rootLayout->setSpacing(8);

        m_titleLabel = new fluent::textfields::Label(tr("对话"), this);
        m_titleLabel->setObjectName(QStringLiteral("chatPageTitle"));
        m_titleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
        m_titleLabel->setFluentTypography(Typography::FontRole::Title);
        m_rootLayout->addWidget(m_titleLabel);

        m_subtitleLabel = new fluent::textfields::Label(tr("与 AI 助手开展多轮智能对话与内容生成"), this);
        m_subtitleLabel->setObjectName(QStringLiteral("chatPageSubtitle"));
        m_subtitleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        m_subtitleLabel->setFluentTypography(Typography::FontRole::Body);
        m_rootLayout->addWidget(m_subtitleLabel);

        m_rootLayout->addStretch(1);
    }

    void ChatPage::setupViewModel() {
        m_viewModel = new ChatViewModel(this);
        m_viewModel->observe(this, &ChatPage::render);
    }

    void ChatPage::render(const ChatState &state) {
        Q_UNUSED(state);
    }

    void ChatPage::onThemeUpdated() {
        QWidget::update();
    }
} // namespace ui::screen::chat
