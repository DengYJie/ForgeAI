#include "ChatService.h"
#include <QTimer>

namespace services::chat {
    ChatService::ChatService(QObject *parent)
        : domain::service::IChatService(parent) {
    }

    void ChatService::sendMessage(const QString &sessionId, const QString &text) {
        const QString trimmed = text.trimmed();
        if (trimmed.isEmpty()) return;

        m_isGenerating = true;
        m_currentSessionId = sessionId;

        // 模拟响应（后续接入大模型提供商 API 流式管道）
        const QString reply = QStringLiteral("收到您的提问：“%1”。系统已完成处理并实时更新了侧边对话时间线。").arg(trimmed);

        // 异步派发回复完成信号
        QTimer::singleShot(50, this, [this, sessionId, reply]() {
            if (!m_isGenerating || m_currentSessionId != sessionId) return;
            m_isGenerating = false;
            emit messageGenerated(sessionId, reply);
            emit generationFinished(sessionId);
        });
    }

    void ChatService::stopGeneration() {
        if (!m_isGenerating) return;
        m_isGenerating = false;
        const QString sess = m_currentSessionId;
        m_currentSessionId.clear();
        emit generationFinished(sess);
    }

    bool ChatService::isGenerating() const {
        return m_isGenerating;
    }
} // namespace services::chat
