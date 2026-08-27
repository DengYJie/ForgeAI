#pragma once

#include <QObject>
#include <QList>
#include "ui/screen/chat/ChatSessionListModel.h"

namespace domain::service {
    class IConversationService;
}

namespace application::usecase::conversation {
    /**
     * @brief 加载全量会话元数据列表用例
     * @details 负责从持久化仓储/服务中拉取会话元数据（ID、标题、置顶状态、时间戳），供侧边栏展示。
     */
    class LoadSessionsUseCase : public QObject {
        Q_OBJECT

    public:
        explicit LoadSessionsUseCase(
            domain::service::IConversationService *conversationService,
            QObject *parent = nullptr
        );

        ~LoadSessionsUseCase() override = default;

        /**
         * @brief 执行拉取会话列表（可按项目 ID 隔离，默认仅加载全局普通对话）
         * @return 会话元数据列表
         */
        QList<ui::screen::chat::ChatSessionItemData> execute(const std::optional<QUuid>& projectId = std::nullopt);

    private:
        domain::service::IConversationService *m_conversationService = nullptr;
    };
} // namespace application::usecase::conversation
