#pragma once

#include "ui/base/BasePage.h"
#include <QSet>
#include <QUuid>

class QVBoxLayout;

namespace ui::widget {
    class CollapsibleSplitView;
}
namespace ui::widget::chat { class ConversationPane; }
namespace ui::screen::chat { class ChatSidebar; }
namespace fluent::collections { class TreeView; }
namespace fluent::basicinput { class Button; }
class QStyledItemDelegate;
class QStandardItemModel;
namespace fluent::textfields { class Label; }
namespace ui::widget::message { class MessageListView; }

namespace ui::screen::work {
    class WorkViewModel;
    struct WorkState;
    class ProjectHeader;

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
        void showProjectContextMenu(const QUuid &projectId, const QPoint &globalPos);

        WorkViewModel *m_viewModel = nullptr;
        QVBoxLayout *m_rootLayout = nullptr;

        ui::widget::CollapsibleSplitView *m_splitView = nullptr;
        fluent::collections::TreeView *m_sessionTree = nullptr;
        QStandardItemModel *m_sessionTreeModel = nullptr;
        QSet<QUuid> m_expandedProjects;
        QSet<QUuid> m_collapsedProjects;
        fluent::basicinput::Button *m_newConversationButton = nullptr;
        ProjectHeader *m_treeHeader = nullptr;
        QStyledItemDelegate *m_sessionTreeDelegate = nullptr;
        QWidget *m_workAreaWidget = nullptr;
        ui::widget::chat::ConversationPane *m_pane = nullptr;
    };
} // namespace ui::screen::work
