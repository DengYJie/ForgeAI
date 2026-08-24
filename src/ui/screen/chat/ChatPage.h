#pragma once

#include "ui/base/BasePage.h"
#include "domain/conversation/Message.h"
#include <QVector>

class QVBoxLayout;

namespace fluent::textfields {
    class Label;
}

namespace ui::widget {
    class CollapsibleSplitView;
}

namespace ui::widget::chat {
    class ChatHeader;
    class ChatAnchorBar;
    class ChatInputBox;
}

namespace ui::widget::message {
    class MessageListView;
}

namespace ui::screen::chat {
    class ChatSidebar;
    class ChatViewModel;
    struct ChatState;

    /**
     * @brief 对话主界面 (纯 View)，仅依赖注入的 ChatViewModel 进行单向数据流渲染
     */
    class ChatPage : public ui::base::BasePage {
        Q_OBJECT

    public:
        explicit ChatPage(
            ChatViewModel *viewModel = nullptr,
            QWidget *parent = nullptr
        );

        ~ChatPage() override = default;

    private:
        void setupUi();
        void updateModelChoices(const ChatState& state);
        void showModelPicker();
        void showFullModelPicker(const QPoint& globalOrigin = {});
        void rebuildModelPicker(const QString& query);
        struct ModelChoice {
            QString providerId;
            QString modelId;
            QString displayName;
            QString providerName;
        };

        /**
         * @brief 响应单向状态流更新，对界面全部组件执行声明式渲染
         * @param state 权威不可变状态快照
         */
        void render(const ChatState &state);

        ChatViewModel *m_viewModel = nullptr;
        QVBoxLayout *m_rootLayout = nullptr;

        ui::widget::CollapsibleSplitView *m_splitView = nullptr;
        ChatSidebar *m_sidebar = nullptr;
        QWidget *m_chatAreaWidget = nullptr;

        ui::widget::chat::ChatHeader *m_header = nullptr;
        ui::widget::chat::ChatAnchorBar *m_anchorBar = nullptr;
        ui::widget::message::MessageListView *m_messageListView = nullptr;
        fluent::textfields::Label* m_emptyStateLabel = nullptr;
        QWidget *m_mainRight = nullptr;
        ui::widget::chat::ChatInputBox *m_inputBox = nullptr;
        fluent::textfields::Label* m_statusLabel = nullptr;
        QVector<ModelChoice> m_modelChoices;
        QString m_currentModelProviderId;
        QString m_currentModelId;
        QWidget* m_modelPickerPopup = nullptr;
        QWidget* m_modelPickerRows = nullptr;
        QPoint m_modelPickerOrigin;
    };
} // namespace ui::screen::chat
