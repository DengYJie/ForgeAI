#include "OpenAIResponsesStreamParser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace llm::protocol::openai_responses {

    OpenAIResponsesStreamParser::OpenAIResponsesStreamParser() = default;
    OpenAIResponsesStreamParser::~OpenAIResponsesStreamParser() = default;

    void OpenAIResponsesStreamParser::reset() {
        m_buffer.clear();
        m_currentEventType.clear();
        m_indexToTool.clear();
        m_itemIdToCallId.clear();
        m_currentCallId.clear();
        m_hasFinished = false;
        m_isFinished = false;
    }

    QList<domain::llm::ChatEvent> OpenAIResponsesStreamParser::feed(const QByteArray &chunk) {
        QList<domain::llm::ChatEvent> events;
        if (m_isFinished || chunk.isEmpty()) return events;

        m_buffer.append(chunk);

        while (true) {
            int newlineIndex = m_buffer.indexOf('\n');
            if (newlineIndex == -1) {
                break;
            }

            QByteArray line = m_buffer.left(newlineIndex).trimmed();
            m_buffer.remove(0, newlineIndex + 1);

            if (!line.isEmpty()) {
                events.append(parseLine(line));
            }
        }

        return events;
    }

    QList<domain::llm::ChatEvent> OpenAIResponsesStreamParser::finish() {
        QList<domain::llm::ChatEvent> events;
        if (m_isFinished) return events;

        m_isFinished = true;
        QByteArray remaining = m_buffer.trimmed();
        m_buffer.clear();

        if (!remaining.isEmpty()) {
            events.append(parseLine(remaining));
        }

        return events;
    }

    QList<domain::llm::ChatEvent> OpenAIResponsesStreamParser::parseLine(const QByteArray &line) {
        QList<domain::llm::ChatEvent> events;

        if (line.startsWith("event: ")) {
            m_currentEventType = QString::fromUtf8(line.mid(7).trimmed());
            return events;
        }

        if (!line.startsWith("data: ")) {
            return events;
        }

        QByteArray data = line.mid(6).trimmed();
        if (data == "[DONE]") {
            if (!m_hasFinished) {
                m_hasFinished = true;
                events.append(domain::llm::EventFinished{"stop"});
            }
            return events;
        }

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            return events;
        }

        QJsonObject obj = doc.object();
        QString type = obj.value("type").toString();
        if (type.isEmpty()) {
            type = m_currentEventType;
        }

        if (type == "response.created") {
            events.append(domain::llm::EventStarted{});
        } else if (type == "response.output_text.delta" || type == "response.text.delta") {
            if (obj.contains("delta") && !obj.value("delta").isNull()) {
                events.append(domain::llm::EventTextDelta{obj.value("delta").toString()});
            }
        } else if (type == "response.reasoning_text.delta" || type == "response.thought.delta"
                       || type == "response.reasoning_summary_text.delta") {
            if (obj.contains("delta") && !obj.value("delta").isNull()) {
                events.append(domain::llm::EventThinkingDelta{obj.value("delta").toString()});
            }
        } else if (type == "response.output_item.added") {
            int outputIndex = obj.value("output_index").toInt(-1);
            if (obj.contains("item") && obj.value("item").isObject()) {
                QJsonObject itemObj = obj.value("item").toObject();
                if (itemObj.value("type").toString() == "function_call") {
                    QString itemId = itemObj.value("id").toString();
                    QString callId = itemObj.value("call_id").toString();
                    if (callId.isEmpty()) callId = itemId;
                    if (!itemId.isEmpty()) m_itemIdToCallId[itemId] = callId;
                    m_currentCallId = callId;
                    QString name = itemObj.value("name").toString();
                    if (outputIndex >= 0) {
                        m_indexToTool[outputIndex] = {itemId, callId, name, outputIndex};
                    }
                    events.append(domain::llm::EventToolCallStarted{callId, name});
                }
            }
        } else if (type == "response.function_call_arguments.delta") {
            // 使用 item_id 或 output_index 关联，不依赖 delta 中的 call_id
            QString itemId = obj.value("item_id").toString();
            int outputIndex = obj.value("output_index").toInt(-1);
            QString callId;
            if (m_itemIdToCallId.contains(itemId)) {
                callId = m_itemIdToCallId.value(itemId);
            } else if (outputIndex >= 0 && m_indexToTool.contains(outputIndex)) {
                callId = m_indexToTool.value(outputIndex).callId;
            } else {
                callId = m_currentCallId;
            }
            QString delta = obj.value("delta").toString();
            if (!delta.isEmpty() && !callId.isEmpty()) {
                events.append(domain::llm::EventToolCallDelta{callId, delta});
            }
        } else if (type == "response.function_call_arguments.done") {
            QString itemId = obj.value("item_id").toString();
            int outputIndex = obj.value("output_index").toInt(-1);
            QString callId;
            if (m_itemIdToCallId.contains(itemId)) {
                callId = m_itemIdToCallId.value(itemId);
            } else if (outputIndex >= 0 && m_indexToTool.contains(outputIndex)) {
                callId = m_indexToTool.value(outputIndex).callId;
            } else {
                callId = m_currentCallId;
            }
            if (!callId.isEmpty()) {
                events.append(domain::llm::EventToolCallFinished{callId});
            }
        } else if (type == "response.output_item.done") {
            if (obj.contains("item") && obj.value("item").isObject()) {
                QJsonObject itemObj = obj.value("item").toObject();
                if (itemObj.value("type").toString() == "function_call") {
                    QString itemId = itemObj.value("id").toString();
                    QString callId = itemObj.value("call_id").toString();
                    if (callId.isEmpty() && m_itemIdToCallId.contains(itemId)) {
                        callId = m_itemIdToCallId.value(itemId);
                    }
                    if (callId.isEmpty()) callId = m_currentCallId;
                    if (!callId.isEmpty()) {
                        events.append(domain::llm::EventToolCallFinished{callId});
                    }
                }
            }
        } else if (type == "response.completed") {
            if (obj.contains("response") && obj.value("response").isObject()) {
                QJsonObject respObj = obj.value("response").toObject();
                if (respObj.contains("usage") && respObj.value("usage").isObject()) {
                    QJsonObject usageObj = respObj.value("usage").toObject();
                    domain::llm::TokenUsage usage;
                    usage.inputTokens = usageObj.value("input_tokens").toInt();
                    usage.outputTokens = usageObj.value("output_tokens").toInt();
                    usage.totalTokens = usageObj.value("total_tokens").toInt();
                    events.append(domain::llm::EventUsageUpdated{usage});
                }
            }
            if (!m_hasFinished) {
                m_hasFinished = true;
                events.append(domain::llm::EventFinished{"stop"});
            }
        } else if (type == "error") {
            domain::llm::ChatError err;
            err.category = domain::llm::ChatErrorCategory::Provider;
            err.code = QStringLiteral("StreamError");
            if (obj.contains("error") && obj.value("error").isObject()) {
                QJsonObject errObj = obj.value("error").toObject();
                err.message = errObj.value("message").toString();
                err.userMessage = err.message;
            } else {
                err.message = "OpenAI Responses API Error";
                err.userMessage = QStringLiteral("OpenAI Responses 流式响应中返回错误。");
            }
            events.append(domain::llm::EventError{err});
        }

        return events;
    }

} // namespace llm::protocol::openai_responses
