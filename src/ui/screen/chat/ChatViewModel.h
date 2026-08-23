#pragma once

#include "ui/base/BaseViewModel.h"
#include "domain/conversation/Message.h"
#include "ui/widget/chat/ChatAnchorBar.h"
#include "ChatSessionListModel.h"
#include "application/usecase/chat/ChatUseCases.h"
#include <QString>
#include <QList>
#include <QUuid>

namespace ui::screen::chat {
    /**
     * @brief 对话页面的权威单一不可变状态快照
     */
    struct ChatState {
        // 会话列表
        QList<ChatSessionItemData> sessions;                ///< 侧边栏展示的全量会话元数据列表
        QString currentSessionId;                           ///< 当前选中的会话 ID
        QString sessionTitle = QStringLiteral("新对话");     ///< 当前会话的标题
        bool sessionTitleManuallyEdited = false;            ///< 用户是否手动改过标题（若是则首条提问不自动覆盖）

        // 模型与生成控制
        QString currentModelName = QStringLiteral("DeepSeek-R1"); ///< 当前激活的模型名称
        bool isGenerating = false;                          ///< 当前是否处于大模型回复生成中
        QString statusMessage;                              ///< 底部状态栏展示的状态/错误消息

        // 核心领域实体列表（单向权威数据源）
        QList<domain::conversation::Message> messages;      ///< 当前会话的完整消息流列表

        // 派生的时间线锚点列表与当前激活索引
        QList<ui::widget::chat::ChatAnchorItem> anchors;    ///< 从消息流计算出的对话锚点时间线
        int activeAnchorIndex = -1;                         ///< 当前激活/视口顶部对齐的锚点索引

        bool operator==(const ChatState &other) const = default;
    };

    /**
     * @brief 对话界面的 ViewModel (UDF 架构核心)，协调 UseCases 与 UI State
     */
    class ChatViewModel : public BaseViewModel<ChatViewModel, ChatState> {
        Q_OBJECT

    public:
        explicit ChatViewModel(
            const application::usecase::chat::ChatUseCases &useCases,
            QObject *parent = nullptr
        );

        ~ChatViewModel() override;

        /**
         * @brief 切换并加载指定会话
         * @param sessionId 目标会话 ID
         */
        void loadSession(const QString &sessionId);

        /**
         * @brief 发起“新建会话”（若存在空白未命名会话则自动复用）
         */
        void newSession();

        /**
         * @brief 删除指定会话（若删除当前会话则自动回退至相邻会话）
         * @param sessionId 待删除的会话 ID
         */
        void deleteSession(const QString &sessionId);

        /**
         * @brief 发送用户提问并启动流式生成
         * @param text 用户输入的文本内容
         */
        void sendMessage(const QString &text);

        /**
         * @brief 中止当前正在进行的大模型生成任务
         */
        void stopGeneration();

        /**
         * @brief 切换当前所选大模型
         * @param modelName 模型名称
         */
        void setModelName(const QString &modelName);

        /**
         * @brief 根据消息 ID 定位并高亮对应时间线锚点
         * @param messageId 目标消息的唯一标识
         */
        void setActiveAnchorByMessageId(const QUuid &messageId);

        /**
         * @brief 设置激活的时间线锚点索引
         * @param index 锚点索引
         */
        void setActiveAnchorIndex(int index);

    Q_SIGNALS:
        /**
         * @brief 状态变更分发信号
         */
        void stateChanged(const ui::screen::chat::ChatState &state);

    protected:
        void emitStateChanged() override;

    private:
        void setupUseCaseConnections();

        static void recalculateAnchors(ChatState &s);

        static void syncSessionTitle(ChatState &s, const QString &sessionId, const QString &title);

        application::usecase::chat::ChatUseCases m_useCases;
    };
} // namespace ui::screen::chat
