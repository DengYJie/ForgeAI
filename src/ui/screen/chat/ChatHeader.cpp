#include "ChatHeader.h"

#include <QPainter>
#include <FluentQt/TextFields.h>
#include <FluentQt/BasicInput.h>
#include <FluentQt/StatusInfo.h>
#include "ui/animation/AnimatedIcon.h"
#include "ui/animation/AnimatedPanelLeftVisualSource.h"

namespace ui::screen::chat {
    ChatHeader::ChatHeader(QWidget *parent)
        : QWidget(parent) {
        setupUi();
    }

    void ChatHeader::setupUi() {
        setFixedHeight(42);

        m_mainLayout = new QHBoxLayout(this);
        m_mainLayout->setContentsMargins(4, 0, 16, 0);
        m_mainLayout->setSpacing(8);

        m_leftLayout = new QHBoxLayout();
        m_leftLayout->setContentsMargins(0, 0, 0, 0);
        m_leftLayout->setSpacing(4);

        m_sidebarToggleButton = new fluent::basicinput::Button(this);
        m_sidebarToggleButton->setFluentStyle(fluent::basicinput::Button::Subtle);
        m_sidebarToggleButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
        m_sidebarToggleButton->setFixedSize(30, 30);
        m_sidebarToggleButton->setToolTip(tr("折叠侧边栏"));
        m_sidebarToggleButton->setCursor(Qt::PointingHandCursor);

        m_panelVisualSource = std::make_shared<ui::animation::AnimatedPanelLeftVisualSource>(this);
        m_animatedSidebarIcon = new ui::animation::AnimatedIcon(m_sidebarToggleButton);
        m_panelVisualSource->setRepaintCallback([this]() {
            if (m_animatedSidebarIcon) {
                m_animatedSidebarIcon->update();
            }
        });
        m_animatedSidebarIcon->setSource(m_panelVisualSource);
        m_animatedSidebarIcon->setAttribute(Qt::WA_TransparentForMouseEvents);

        connect(m_sidebarToggleButton, &QPushButton::clicked, this, &ChatHeader::toggleSidebarRequested);
        m_leftLayout->addWidget(m_sidebarToggleButton);

        m_mainLayout->addLayout(m_leftLayout);

        m_titleLayout = new QHBoxLayout();
        m_titleLayout->setContentsMargins(0, 0, 0, 0);
        m_titleLayout->setSpacing(4);

        m_titleLabel = new fluent::textfields::Label(tr("新对话"), this);
        m_titleLabel->setFluentTypography(Typography::FontRole::Body);
        m_titleLayout->addWidget(m_titleLabel);
        m_mainLayout->addLayout(m_titleLayout);

        m_centerLayout = new QHBoxLayout();
        m_centerLayout->setContentsMargins(0, 0, 0, 0);
        m_centerLayout->setSpacing(4);
        m_mainLayout->addLayout(m_centerLayout, 1);

        m_rightLayout = new QHBoxLayout();
        m_rightLayout->setContentsMargins(0, 0, 0, 0);
        m_rightLayout->setSpacing(4);

        m_clearButton = new fluent::basicinput::Button(this);
        m_clearButton->setFluentStyle(fluent::basicinput::Button::Subtle);
        m_clearButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
        m_clearButton->setIconGlyph(Typography::Icons::Clear, 13);
        m_clearButton->setFixedSize(30, 30);
        m_clearButton->setToolTip(tr("清空对话上下文"));
        m_clearButton->setCursor(Qt::PointingHandCursor);
        connect(m_clearButton, &QPushButton::clicked, this, &ChatHeader::clearChatRequested);
        m_rightLayout->addWidget(m_clearButton);

        m_thirdPaneToggle = new fluent::basicinput::Button(this);
        m_thirdPaneToggle->setCheckable(true);
        m_thirdPaneToggle->setFluentStyle(fluent::basicinput::Button::Subtle);
        m_thirdPaneToggle->setFluentLayout(fluent::basicinput::Button::IconOnly);
        m_thirdPaneToggle->setIconGlyph(
            Typography::Icons::glyph(QStringLiteral("ic_fluent_dock_panel_right_20_regular")), 13);
        m_thirdPaneToggle->setFixedSize(30, 30);
        m_thirdPaneToggle->setToolTip(tr("切换辅助工作区"));
        m_thirdPaneToggle->setCursor(Qt::PointingHandCursor);
        connect(m_thirdPaneToggle, &QPushButton::toggled, this, &ChatHeader::toggleThirdPaneRequested);
        m_rightLayout->addWidget(m_thirdPaneToggle);

        m_mainLayout->addLayout(m_rightLayout);
    }

    void ChatHeader::setTitle(const QString &title) const {
        if (m_titleLabel) {
            m_titleLabel->setText(title);
        }
    }

    QString ChatHeader::title() const {
        return m_titleLabel ? m_titleLabel->text() : QString();
    }

    void ChatHeader::setSidebarExpanded(bool expanded) {
        if (m_panelVisualSource) {
            m_panelVisualSource->setExpanded(expanded);
        }
        if (m_animatedSidebarIcon) {
            m_animatedSidebarIcon->update();
        }
        if (m_sidebarToggleButton) {
            m_sidebarToggleButton->setToolTip(expanded ? tr("折叠侧边栏") : tr("展开侧边栏"));
        }
    }

    bool ChatHeader::isSidebarExpanded() const {
        return m_panelVisualSource ? m_panelVisualSource->isExpanded() : true;
    }

    void ChatHeader::onThemeUpdated() {
        update();
    }

    void ChatHeader::paintEvent(QPaintEvent *event) {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setPen(themeColorsRef().strokeDivider);
        painter.drawLine(0, height() - 1, width(), height() - 1);
    }
} // namespace ui::screen::chat
