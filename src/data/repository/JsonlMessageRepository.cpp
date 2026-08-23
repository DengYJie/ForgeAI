#include "JsonlMessageRepository.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

namespace data::repository {
    JsonlMessageRepository::JsonlMessageRepository(const QString &storageDir)
        : m_storageDir(storageDir) {
        QDir().mkpath(m_storageDir);
    }

    QString JsonlMessageRepository::getFilePath(const QUuid &conversationId) const {
        return QDir(m_storageDir).filePath(conversationId.toString(QUuid::WithoutBraces) + ".jsonl");
    }

    void JsonlMessageRepository::appendMessage(const QUuid &conversationId,
                                               const domain::conversation::Message &message) {
        QString path = getFilePath(conversationId);
        QFile file(path);

        // Append 模式打开，极其高效，不会覆盖之前的数据
        if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qWarning() << "Failed to open jsonl file for append:" << path;
            return;
        }

        QJsonObject msgObj;
        msgObj["id"] = message.id.toString(QUuid::WithoutBraces);
        if (!message.parentId.isNull()) {
            msgObj["parentId"] = message.parentId.toString(QUuid::WithoutBraces);
        }
        msgObj["role"] = static_cast<int>(message.role);
        msgObj["status"] = static_cast<int>(message.status);
        if (!message.errorMessage.isEmpty()) {
            msgObj["errorMessage"] = message.errorMessage;
        }
        if (!message.turnOptions.isEmpty()) {
            msgObj["turnOptions"] = message.turnOptions;
        }
        msgObj["createdAt"] = message.createdAt.toMSecsSinceEpoch();

        QJsonArray blocksArray;
        for (const auto &block: message.blocks) {
            QJsonObject blockObj;
            blockObj["type"] = static_cast<int>(block.type);

            std::visit([&blockObj](auto &&payload) {
                using T = std::decay_t<decltype(payload)>;
                if constexpr (std::is_same_v<T, domain::conversation::TextBlock>) {
                    blockObj["payload"] = QJsonObject{{"text", payload.text}};
                } else if constexpr (std::is_same_v<T, domain::conversation::ThoughtBlock>) {
                    blockObj["payload"] = QJsonObject{
                        {"thought", payload.thought},
                        {"durationMs", payload.durationMs}
                    };
                } else if constexpr (std::is_same_v<T, domain::conversation::ImageBlock>) {
                    blockObj["payload"] = QJsonObject{
                        {"url", payload.urlOrLocalPath},
                        {"mimeType", payload.mimeType}
                    };
                } else if constexpr (std::is_same_v<T, domain::conversation::ToolCallBlock>) {
                    QJsonArray callsArr;
                    for (const auto &call: payload.calls) {
                        callsArr.append(QJsonObject{
                            {"id", call.id},
                            {"name", call.name},
                            {"arguments", call.arguments}
                        });
                    }
                    blockObj["payload"] = QJsonObject{{"calls", callsArr}};
                } else if constexpr (std::is_same_v<T, domain::conversation::ToolResultBlock>) {
                    QJsonArray resultsArr;
                    for (const auto &res: payload.results) {
                        resultsArr.append(QJsonObject{
                            {"toolCallId", res.toolCallId},
                            {"content", res.content},
                            {"isError", res.isError}
                        });
                    }
                    blockObj["payload"] = QJsonObject{{"results", resultsArr}};
                }
            }, block.payload);

            blocksArray.append(blockObj);
        }
        msgObj["blocks"] = blocksArray;

        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        // 关键：使用 Compact 格式，保证一整条 Message 只占文本文件的一行
        out << QJsonDocument(msgObj).toJson(QJsonDocument::Compact) << "\n";
    }

    QList<domain::conversation::Message> JsonlMessageRepository::getMessagesByConversationId(
        const QUuid &conversationId) {
        QList<domain::conversation::Message> messages;
        QString path = getFilePath(conversationId);
        QFile file(path);

        // ReadOnly 模式读取
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return messages; // 文件不存在说明还没产生聊天
        }

        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);

        // 逐行读取，这就是 JSONL 的精髓
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty()) continue;

            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &error);
            if (error.error != QJsonParseError::NoError || !doc.isObject()) {
                qWarning() << "Failed to parse JSONL line:" << error.errorString();
                continue;
            }

            QJsonObject msgObj = doc.object();
            domain::conversation::Message msg;
            msg.id = QUuid(msgObj["id"].toString());
            if (msgObj.contains("parentId")) {
                msg.parentId = QUuid(msgObj["parentId"].toString());
            }
            msg.role = static_cast<domain::MessageRole>(msgObj["role"].toInt());
            msg.status = static_cast<domain::MessageStatus>(msgObj["status"].toInt());
            if (msgObj.contains("errorMessage")) {
                msg.errorMessage = msgObj["errorMessage"].toString();
            }
            if (msgObj.contains("turnOptions")) {
                msg.turnOptions = msgObj["turnOptions"].toObject();
            }
            msg.createdAt = QDateTime::fromMSecsSinceEpoch(msgObj["createdAt"].toVariant().toLongLong());

            QJsonArray blocksArray = msgObj["blocks"].toArray();
            for (const QJsonValue &blockVal: blocksArray) {
                QJsonObject blockObj = blockVal.toObject();
                domain::conversation::MessageBlock block;
                block.type = static_cast<domain::BlockType>(blockObj["type"].toInt());
                QJsonObject payloadObj = blockObj["payload"].toObject();

                switch (block.type) {
                    case domain::BlockType::Text: {
                        block.payload = domain::conversation::TextBlock{payloadObj["text"].toString()};
                        break;
                    }
                    case domain::BlockType::Thought: {
                        block.payload = domain::conversation::ThoughtBlock{
                            payloadObj["thought"].toString(),
                            payloadObj["durationMs"].toVariant().toLongLong()
                        };
                        break;
                    }
                    case domain::BlockType::Image: {
                        block.payload = domain::conversation::ImageBlock{
                            payloadObj["url"].toString(),
                            payloadObj["mimeType"].toString()
                        };
                        break;
                    }
                    case domain::BlockType::ToolCall: {
                        domain::conversation::ToolCallBlock callBlock;
                        for (const QJsonValue &v: payloadObj["calls"].toArray()) {
                            QJsonObject callObj = v.toObject();
                            callBlock.calls.append({
                                callObj["id"].toString(),
                                callObj["name"].toString(),
                                callObj["arguments"].toString()
                            });
                        }
                        block.payload = callBlock;
                        break;
                    }
                    case domain::BlockType::ToolResult: {
                        domain::conversation::ToolResultBlock resBlock;
                        for (const QJsonValue &v: payloadObj["results"].toArray()) {
                            QJsonObject resObj = v.toObject();
                            resBlock.results.append({
                                resObj["toolCallId"].toString(),
                                resObj["content"].toString(),
                                resObj["isError"].toBool()
                            });
                        }
                        block.payload = resBlock;
                        break;
                    }
                    default:
                        break;
                }
                msg.blocks.append(block);
            }
            messages.append(msg);
        }

        return messages;
    }



    void JsonlMessageRepository::deleteTranscript(const QUuid &conversationId) {
        QFile::remove(getFilePath(conversationId));
    }
} // namespace data::repository
