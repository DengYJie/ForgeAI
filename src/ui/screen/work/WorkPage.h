#pragma once

#include "ui/base/BasePage.h"

class QVBoxLayout;

namespace ui::widget {
    class CollapsibleSplitView;
}
namespace ui::widget::chat { class ChatInputBox; class ChatHeader; }
namespace fluent::textfields { class Label; }
namespace ui::widget::message { class MessageListView; }

namespace ui::screen::work {
    class WorkViewModel;
    struct WorkState;

    /**
     * @brief 工作流主界面 (纯 View)，接收注入的 WorkViewModel
     */
    class WorkPage : public ui::base::BasePage {
        Q_OBJECT

    public:
        explicit WorkPage(
            WorkViewModel *viewModel = nullptr,
            QWidget *parent = nullptr
        );

        ~WorkPage() override = default;

    private:
        void setupUi();

        void render(const WorkState &state);

        WorkViewModel *m_viewModel = nullptr;
        QVBoxLayout *m_rootLayout = nullptr;

        ui::widget::CollapsibleSplitView *m_splitView = nullptr;
        QWidget *m_sidebarWidget = nullptr;
        QWidget *m_workAreaWidget = nullptr;

        ui::widget::chat::ChatHeader *m_header = nullptr;
        fluent::textfields::Label *m_projectPathLabel = nullptr;
        ui::widget::chat::ChatInputBox *m_agentInput = nullptr;
        ui::widget::message::MessageListView *m_messageList = nullptr;
    };
} // namespace ui::screen::work
