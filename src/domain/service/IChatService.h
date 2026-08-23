#pragma once

#include <QObject>
#include <QString>

namespace domain::service {
    /**
     * @brief 对话生成与大模型交互服务接口
     */
    class IChatService : public QObject {
        Q_OBJECT

    public:
        using QObject::QObject;
        ~IChatService() override = default;

        /**
         * @brief 向指定会话发送消息并触发生成
         * @param sessionId 目标会话 ID
         * @param text 用户提问文本
         */
        virtual void sendMessage(const QString &sessionId, const QString &text) = 0;

        /**
         * @brief 中止正在进行的生成任务
         */
        virtual void stopGeneration() = 0;

        /**
         * @brief 当前是否处于生成状态
         */
        virtual bool isGenerating() const = 0;

    Q_SIGNALS:
        /**
         * @brief 流式增量 Token 收到通知
         */
        void tokenReceived(const QString &sessionId, const QString &token);

        /**
         * @brief 回复生成完成通知
         */
        void messageGenerated(const QString &sessionId, const QString &fullReply);

        /**
         * @brief 本轮生成完全结束
         */
        void generationFinished(const QString &sessionId);

        /**
         * @brief 生成过程异常报错通知
         */
        void generationFailed(const QString &sessionId, const QString &errorMessage);
    };
} // namespace domain::service
