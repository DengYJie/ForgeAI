#pragma once
#include <QString>
#include "domain/repository/IMessageTranscriptRepository.h"

namespace data::repository {
    /**
     * @brief 基于 JSONL 单行文本流的消息与轨迹仓储实现（纯追加 Append-only 写入）
     */
    class JsonlMessageRepository : public domain::repository::IMessageTranscriptRepository {
    public:
        /**
         * @param storageDir 存放 .jsonl 文件的物理目录 (如 ~/.forgeai/sessions/)
         */
        explicit JsonlMessageRepository(const QString &storageDir);

        ~JsonlMessageRepository() override = default;

        void appendMessage(const QUuid &conversationId, const domain::conversation::Message &message) override;

        QList<domain::conversation::Message> getMessagesByConversationId(const QUuid &conversationId) override;

        QList<domain::conversation::Message> getMessagesByTurnId(const QUuid &turnId) override;

        void deleteTranscript(const QUuid &conversationId) override;

    private:
        QString m_storageDir;

        // 根据 conversationId 计算对应的文件绝对路径
        QString getFilePath(const QUuid &conversationId) const;
    };
} // namespace data::repository
