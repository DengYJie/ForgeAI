#pragma once

#include "domain/service/IChatService.h"
#include <QTimer>

namespace services::chat {
    /**
     * @brief 对话生成服务实现
     */
    class ChatService : public domain::service::IChatService {
        Q_OBJECT

    public:
        explicit ChatService(QObject *parent = nullptr);

        ~ChatService() override = default;

        void sendMessage(const QString &sessionId, const QString &text) override;

        void stopGeneration() override;

        bool isGenerating() const override;

    private:
        bool m_isGenerating = false;
        QString m_currentSessionId;
    };
} // namespace services::chat
