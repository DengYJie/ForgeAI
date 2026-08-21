#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QString>
#include <FluentQt/Foundation.h>
#include <FluentQt/Design.h>

namespace fluent::textfields {
    class Label;
}

namespace fluent::basicinput {
    class Button;
    class ToggleButton;
}

namespace fluent::statusinfo {
    class InfoBadge;
}

namespace ui::animation {
    class AnimatedIcon;
    class AnimatedPanelLeftVisualSource;
}

namespace ui::screen::chat {
    /**
     * @brief 聊天面板顶部栏控件
     */
    class ChatHeader : public QWidget, public fluent::FluentElement {
        Q_OBJECT

    public:
        explicit ChatHeader(QWidget *parent = nullptr);

        ~ChatHeader() override = default;

        QHBoxLayout *leftLayout() const { return m_leftLayout; }
        QHBoxLayout *titleLayout() const { return m_titleLayout; }
        QHBoxLayout *centerLayout() const { return m_centerLayout; }
        QHBoxLayout *rightLayout() const { return m_rightLayout; }

        void setTitle(const QString &title) const;

        QString title() const;

        void setSidebarExpanded(bool expanded);

        bool isSidebarExpanded() const;

        void onThemeUpdated() override;

    Q_SIGNALS:
        void toggleSidebarRequested();

        void clearChatRequested();

        void toggleThirdPaneRequested(bool open);

    protected:
        void paintEvent(QPaintEvent *event) override;

    private:
        void setupUi();

        QHBoxLayout *m_mainLayout = nullptr;
        QHBoxLayout *m_leftLayout = nullptr;
        QHBoxLayout *m_titleLayout = nullptr;
        QHBoxLayout *m_centerLayout = nullptr;
        QHBoxLayout *m_rightLayout = nullptr;

        fluent::basicinput::Button *m_sidebarToggleButton = nullptr;
        ui::animation::AnimatedIcon *m_animatedSidebarIcon = nullptr;
        std::shared_ptr<ui::animation::AnimatedPanelLeftVisualSource> m_panelVisualSource;

        fluent::textfields::Label *m_titleLabel = nullptr;
        fluent::basicinput::Button *m_clearButton = nullptr;
        fluent::basicinput::Button *m_thirdPaneToggle = nullptr;
    };
} // namespace ui::screen::chat
