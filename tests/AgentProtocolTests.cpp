#include <QtTest>
#include <QJsonDocument>

#include "llm/protocol/openai_responses/OpenAIResponsesAdapter.h"
#include "llm/protocol/ollama/OllamaProtocolAdapter.h"

class AgentProtocolTests final : public QObject {
    Q_OBJECT
private slots:
    void responsesUsesFunctionCallOutput();
    void ollamaKeepsMessageContent();
};

void AgentProtocolTests::responsesUsesFunctionCallOutput() {
    domain::llm::ChatRequest request;
    request.model = QStringLiteral("test-model");
    domain::llm::ChatMessage assistant;
    assistant.role = domain::MessageRole::Assistant;
    assistant.toolCalls = QList<domain::agent::ToolCall>{{QStringLiteral("call-1"), QStringLiteral("read_file"), QStringLiteral(R"({"path":"a.txt"})")}};
    request.messages.append(assistant);
    request.messages.append({domain::MessageRole::Tool, QStringLiteral("file body"), QStringLiteral("read_file"), QStringLiteral("call-1")});

    const auto http = llm::protocol::openai_responses::OpenAIResponsesAdapter{}.buildChatRequest({}, request);
    const auto input = QJsonDocument::fromJson(http.body).object().value(QStringLiteral("input")).toArray();
    QCOMPARE(input.size(), 2);
    QCOMPARE(input.at(0).toObject().value(QStringLiteral("type")).toString(), QStringLiteral("function_call"));
    QCOMPARE(input.at(1).toObject().value(QStringLiteral("type")).toString(), QStringLiteral("function_call_output"));
    QCOMPARE(input.at(1).toObject().value(QStringLiteral("call_id")).toString(), QStringLiteral("call-1"));
}

void AgentProtocolTests::ollamaKeepsMessageContent() {
    domain::llm::ChatRequest request;
    request.model = QStringLiteral("test-model");
    request.messages.append({domain::MessageRole::User, QStringLiteral("hello project")});
    const auto http = llm::protocol::ollama::OllamaProtocolAdapter{}.buildChatRequest({}, request);
    const auto messages = QJsonDocument::fromJson(http.body).object().value(QStringLiteral("messages")).toArray();
    QCOMPARE(messages.at(0).toObject().value(QStringLiteral("content")).toString(), QStringLiteral("hello project"));
}

QTEST_APPLESS_MAIN(AgentProtocolTests)
#include "AgentProtocolTests.moc"
