#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "llm/protocol/anthropic/AnthropicProtocolAdapter.h"
#include "llm/protocol/anthropic/AnthropicStreamParser.h"
#include "llm/protocol/gemini/GeminiProtocolAdapter.h"
#include "llm/protocol/gemini/GeminiStreamParser.h"
#include "llm/protocol/openai/OpenAIChatCompletionsAdapter.h"
#include "llm/protocol/openai/OpenAIStreamParser.h"
#include "llm/protocol/openai_responses/OpenAIResponsesAdapter.h"
#include "llm/protocol/openai_responses/OpenAIResponsesStreamParser.h"
#include "llm/protocol/ollama/OllamaProtocolAdapter.h"
#include "domain/llm/ChatRequest.h"
#include "domain/llm/ChatEvent.h"

#include "llm/mcp/McpClient.h"
#include "llm/mcp/IMcpTransport.h"

class MockMcpTransport final : public llm::mcp::IMcpTransport {
    Q_OBJECT
public:
    bool start() override { m_connected = true; return true; }
    void close() override { m_connected = false; emit closed(); }
    bool isConnected() const override { return m_connected; }
    void setConnected(bool c) { m_connected = c; }

    bool sendJson(const QJsonObject& json) override {
        if (!m_connected) return false;
        m_sentMessages.append(json);

        if (m_autoResponder) {
            auto resp = m_autoResponder(json);
            if (!resp.isEmpty()) {
                QTimer::singleShot(0, this, [this, resp]() {
                    emit messageReceived(resp);
                });
            }
        }
        return true;
    }

    void setAutoResponder(std::function<QJsonObject(const QJsonObject&)> responder) {
        m_autoResponder = std::move(responder);
    }

    QList<QJsonObject> sentMessages() const { return m_sentMessages; }

private:
    bool m_connected = true;
    QList<QJsonObject> m_sentMessages;
    std::function<QJsonObject(const QJsonObject&)> m_autoResponder;
};

class AgentProtocolTests final : public QObject {
    Q_OBJECT
private slots:
    void responsesUsesFunctionCallOutput();
    void ollamaKeepsMessageContent();
    void openAIChatCompletionsToolMappingAndStream();
    void anthropicToolMappingAndStream();
    void geminiToolMappingAndStream();
    void openAIResponsesToolStream();
    void mcpClientInitializeAndHandshake();
    void mcpClientListTools();
    void mcpClientCallToolSuccessAndError();
    void mcpClientTimeoutAndDisconnected();
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

void AgentProtocolTests::openAIChatCompletionsToolMappingAndStream() {
    domain::llm::ChatRequest request;
    request.model = QStringLiteral("gpt-4o");
    domain::agent::ToolDefinition toolDef{
        QStringLiteral("read_file"),
        QStringLiteral("Read file content"),
        QJsonObject{{"type", "object"}, {"properties", QJsonObject{{"path", QJsonObject{{"type", "string"}}}}}}
    };
    request.tools = QList<domain::agent::ToolDefinition>{toolDef};

    domain::llm::ChatMessage assistant;
    assistant.role = domain::MessageRole::Assistant;
    assistant.toolCalls = QList<domain::agent::ToolCall>{{QStringLiteral("call_1"), QStringLiteral("read_file"), QStringLiteral(R"({"path":"test.txt"})")}};
    request.messages.append(assistant);
    request.messages.append({domain::MessageRole::Tool, QStringLiteral("file content"), QStringLiteral("read_file"), QStringLiteral("call_1")});

    const auto http = llm::protocol::openai::OpenAIChatCompletionsAdapter{}.buildChatRequest({}, request);
    const auto doc = QJsonDocument::fromJson(http.body).object();
    const auto tools = doc.value(QStringLiteral("tools")).toArray();
    QCOMPARE(tools.size(), 1);
    QCOMPARE(tools.at(0).toObject().value(QStringLiteral("type")).toString(), QStringLiteral("function"));
    QCOMPARE(tools.at(0).toObject().value(QStringLiteral("function")).toObject().value(QStringLiteral("name")).toString(), QStringLiteral("read_file"));

    const auto msgs = doc.value(QStringLiteral("messages")).toArray();
    QCOMPARE(msgs.size(), 2);
    QCOMPARE(msgs.at(0).toObject().value(QStringLiteral("role")).toString(), QStringLiteral("assistant"));
    QCOMPARE(msgs.at(1).toObject().value(QStringLiteral("role")).toString(), QStringLiteral("tool"));
    QCOMPARE(msgs.at(1).toObject().value(QStringLiteral("tool_call_id")).toString(), QStringLiteral("call_1"));

    // Stream parser verification
    llm::protocol::openai::OpenAIStreamParser parser;
    QByteArray chunk = "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"id\":\"call_99\",\"function\":{\"name\":\"read_file\",\"arguments\":\"{\\\"path\\\":\"}}]}}]}\n\n";
    auto events = parser.feed(chunk);
    QCOMPARE(events.size(), 2);
    QVERIFY(std::holds_alternative<domain::llm::EventToolCallStarted>(events[0]));
    QCOMPARE(std::get<domain::llm::EventToolCallStarted>(events[0]).id, QStringLiteral("call_99"));
    QCOMPARE(std::get<domain::llm::EventToolCallStarted>(events[0]).functionName, QStringLiteral("read_file"));
    QVERIFY(std::holds_alternative<domain::llm::EventToolCallDelta>(events[1]));
    QCOMPARE(std::get<domain::llm::EventToolCallDelta>(events[1]).argumentsDelta, QStringLiteral("{\"path\":"));
}

void AgentProtocolTests::anthropicToolMappingAndStream() {
    domain::llm::ChatRequest request;
    request.model = QStringLiteral("claude-3-5-sonnet-20241022");
    domain::agent::ToolDefinition toolDef{
        QStringLiteral("list_files"),
        QStringLiteral("List directory files"),
        QJsonObject{{"type", "object"}}
    };
    request.tools = QList<domain::agent::ToolDefinition>{toolDef};

    domain::llm::ChatMessage assistant;
    assistant.role = domain::MessageRole::Assistant;
    assistant.toolCalls = QList<domain::agent::ToolCall>{{QStringLiteral("toolu_1"), QStringLiteral("list_files"), QStringLiteral(R"({"path":"."})")}};
    request.messages.append(assistant);
    request.messages.append({domain::MessageRole::Tool, QStringLiteral("[\"a.txt\"]"), QStringLiteral("list_files"), QStringLiteral("toolu_1")});

    const auto http = llm::protocol::anthropic::AnthropicProtocolAdapter{}.buildChatRequest({}, request);
    const auto doc = QJsonDocument::fromJson(http.body).object();
    const auto tools = doc.value(QStringLiteral("tools")).toArray();
    QCOMPARE(tools.size(), 1);
    QCOMPARE(tools.at(0).toObject().value(QStringLiteral("name")).toString(), QStringLiteral("list_files"));
    QCOMPARE(tools.at(0).toObject().value(QStringLiteral("input_schema")).toObject().value(QStringLiteral("type")).toString(), QStringLiteral("object"));

    const auto msgs = doc.value(QStringLiteral("messages")).toArray();
    QCOMPARE(msgs.size(), 2);
    QCOMPARE(msgs.at(0).toObject().value(QStringLiteral("role")).toString(), QStringLiteral("assistant"));
    const auto assistantContent = msgs.at(0).toObject().value(QStringLiteral("content")).toArray();
    QCOMPARE(assistantContent.at(0).toObject().value(QStringLiteral("type")).toString(), QStringLiteral("tool_use"));
    QCOMPARE(assistantContent.at(0).toObject().value(QStringLiteral("id")).toString(), QStringLiteral("toolu_1"));

    QCOMPARE(msgs.at(1).toObject().value(QStringLiteral("role")).toString(), QStringLiteral("user"));
    const auto userContent = msgs.at(1).toObject().value(QStringLiteral("content")).toArray();
    QCOMPARE(userContent.at(0).toObject().value(QStringLiteral("type")).toString(), QStringLiteral("tool_result"));
    QCOMPARE(userContent.at(0).toObject().value(QStringLiteral("tool_use_id")).toString(), QStringLiteral("toolu_1"));

    // Stream parser verification
    llm::protocol::anthropic::AnthropicStreamParser parser;
    QByteArray startChunk = "event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":1,\"content_block\":{\"type\":\"tool_use\",\"id\":\"toolu_99\",\"name\":\"list_files\"}}\n\n";
    auto events = parser.feed(startChunk);
    QCOMPARE(events.size(), 1);
    QVERIFY(std::holds_alternative<domain::llm::EventToolCallStarted>(events[0]));
    QCOMPARE(std::get<domain::llm::EventToolCallStarted>(events[0]).id, QStringLiteral("toolu_99"));
    QCOMPARE(std::get<domain::llm::EventToolCallStarted>(events[0]).functionName, QStringLiteral("list_files"));

    QByteArray deltaChunk = "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":1,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"{\\\"path\\\": \\\".\\\"}\"}}\n\n";
    events = parser.feed(deltaChunk);
    QCOMPARE(events.size(), 1);
    QVERIFY(std::holds_alternative<domain::llm::EventToolCallDelta>(events[0]));
    QCOMPARE(std::get<domain::llm::EventToolCallDelta>(events[0]).argumentsDelta, QStringLiteral("{\"path\": \".\"}"));

    QByteArray stopChunk = "event: content_block_stop\ndata: {\"type\":\"content_block_stop\",\"index\":1}\n\n";
    events = parser.feed(stopChunk);
    QCOMPARE(events.size(), 1);
    QVERIFY(std::holds_alternative<domain::llm::EventToolCallFinished>(events[0]));
    QCOMPARE(std::get<domain::llm::EventToolCallFinished>(events[0]).id, QStringLiteral("toolu_99"));
}

void AgentProtocolTests::geminiToolMappingAndStream() {
    domain::llm::ChatRequest request;
    request.model = QStringLiteral("gemini-1.5-pro");
    domain::agent::ToolDefinition toolDef{
        QStringLiteral("search_text"),
        QStringLiteral("Search workspace files"),
        QJsonObject{{"type", "object"}}
    };
    request.tools = QList<domain::agent::ToolDefinition>{toolDef};

    domain::llm::ChatMessage assistant;
    assistant.role = domain::MessageRole::Assistant;
    assistant.toolCalls = QList<domain::agent::ToolCall>{{QStringLiteral("call_g1"), QStringLiteral("search_text"), QStringLiteral(R"({"query":"main"})")}};
    request.messages.append(assistant);
    request.messages.append({domain::MessageRole::Tool, QStringLiteral("hit in main.cpp"), QStringLiteral("search_text"), QStringLiteral("call_g1")});

    const auto http = llm::protocol::gemini::GeminiProtocolAdapter{}.buildChatRequest({}, request);
    const auto doc = QJsonDocument::fromJson(http.body).object();
    const auto tools = doc.value(QStringLiteral("tools")).toArray();
    QCOMPARE(tools.size(), 1);
    const auto fnDecls = tools.at(0).toObject().value(QStringLiteral("functionDeclarations")).toArray();
    QCOMPARE(fnDecls.size(), 1);
    QCOMPARE(fnDecls.at(0).toObject().value(QStringLiteral("name")).toString(), QStringLiteral("search_text"));

    const auto contents = doc.value(QStringLiteral("contents")).toArray();
    QCOMPARE(contents.size(), 2);
    QCOMPARE(contents.at(0).toObject().value(QStringLiteral("role")).toString(), QStringLiteral("model"));
    const auto modelParts = contents.at(0).toObject().value(QStringLiteral("parts")).toArray();
    QCOMPARE(modelParts.at(0).toObject().value(QStringLiteral("functionCall")).toObject().value(QStringLiteral("name")).toString(), QStringLiteral("search_text"));

    QCOMPARE(contents.at(1).toObject().value(QStringLiteral("role")).toString(), QStringLiteral("user"));
    const auto userParts = contents.at(1).toObject().value(QStringLiteral("parts")).toArray();
    QCOMPARE(userParts.at(0).toObject().value(QStringLiteral("functionResponse")).toObject().value(QStringLiteral("name")).toString(), QStringLiteral("search_text"));

    // Stream parser verification
    llm::protocol::gemini::GeminiStreamParser parser;
    QByteArray chunk = "data: {\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"search_text\",\"args\":{\"query\":\"abc\"}}}]}}]}\n\n";
    auto events = parser.feed(chunk);
    QCOMPARE(events.size(), 3);
    QVERIFY(std::holds_alternative<domain::llm::EventToolCallStarted>(events[0]));
    QCOMPARE(std::get<domain::llm::EventToolCallStarted>(events[0]).functionName, QStringLiteral("search_text"));
    QVERIFY(std::holds_alternative<domain::llm::EventToolCallDelta>(events[1]));
    QVERIFY(std::holds_alternative<domain::llm::EventToolCallFinished>(events[2]));
}

void AgentProtocolTests::openAIResponsesToolStream() {
    llm::protocol::openai_responses::OpenAIResponsesStreamParser parser;
    QByteArray itemAdded = "event: response.output_item.added\ndata: {\"type\":\"response.output_item.added\",\"item\":{\"type\":\"function_call\",\"call_id\":\"call_resp_1\",\"name\":\"read_file\"}}\n\n";
    auto events = parser.feed(itemAdded);
    QCOMPARE(events.size(), 1);
    QVERIFY(std::holds_alternative<domain::llm::EventToolCallStarted>(events[0]));
    QCOMPARE(std::get<domain::llm::EventToolCallStarted>(events[0]).id, QStringLiteral("call_resp_1"));
    QCOMPARE(std::get<domain::llm::EventToolCallStarted>(events[0]).functionName, QStringLiteral("read_file"));

    QByteArray delta = "event: response.function_call_arguments.delta\ndata: {\"type\":\"response.function_call_arguments.delta\",\"call_id\":\"call_resp_1\",\"delta\":\"{\\\"path\\\":\\\"main.cpp\\\"}\"}\n\n";
    events = parser.feed(delta);
    QCOMPARE(events.size(), 1);
    QVERIFY(std::holds_alternative<domain::llm::EventToolCallDelta>(events[0]));
    QCOMPARE(std::get<domain::llm::EventToolCallDelta>(events[0]).argumentsDelta, QStringLiteral("{\"path\":\"main.cpp\"}"));

    QByteArray done = "event: response.function_call_arguments.done\ndata: {\"type\":\"response.function_call_arguments.done\",\"call_id\":\"call_resp_1\"}\n\n";
    events = parser.feed(done);
    QCOMPARE(events.size(), 1);
    QVERIFY(std::holds_alternative<domain::llm::EventToolCallFinished>(events[0]));
    QCOMPARE(std::get<domain::llm::EventToolCallFinished>(events[0]).id, QStringLiteral("call_resp_1"));
}

void AgentProtocolTests::mcpClientInitializeAndHandshake() {
    MockMcpTransport transport;
    transport.setAutoResponder([](const QJsonObject& req) -> QJsonObject {
        const QString method = req.value(QStringLiteral("method")).toString();
        const int id = req.value(QStringLiteral("id")).toInt();
        if (method == QStringLiteral("initialize")) {
            return QJsonObject{
                {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                {QStringLiteral("id"), id},
                {QStringLiteral("result"), QJsonObject{
                    {QStringLiteral("protocolVersion"), QStringLiteral("2024-11-05")},
                    {QStringLiteral("capabilities"), QJsonObject{}},
                    {QStringLiteral("serverInfo"), QJsonObject{{QStringLiteral("name"), QStringLiteral("mock-mcp-server")}}}
                }}
            };
        }
        return {};
    });

    llm::mcp::McpClient client(&transport);
    QVERIFY(client.initialize());

    // 验证握手后自动发出了 initialized 通知
    const auto sent = transport.sentMessages();
    QVERIFY(sent.size() >= 2);
    QCOMPARE(sent.at(0).value(QStringLiteral("method")).toString(), QStringLiteral("initialize"));
    QCOMPARE(sent.at(1).value(QStringLiteral("method")).toString(), QStringLiteral("notifications/initialized"));
}

void AgentProtocolTests::mcpClientListTools() {
    MockMcpTransport transport;
    transport.setAutoResponder([](const QJsonObject& req) -> QJsonObject {
        const QString method = req.value(QStringLiteral("method")).toString();
        const int id = req.value(QStringLiteral("id")).toInt();
        if (method == QStringLiteral("tools/list")) {
            QJsonArray tools;
            tools.append(QJsonObject{
                {QStringLiteral("name"), QStringLiteral("exec_sql")},
                {QStringLiteral("description"), QStringLiteral("Execute SQL Query")},
                {QStringLiteral("inputSchema"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("object")},
                    {QStringLiteral("properties"), QJsonObject{{QStringLiteral("sql"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}}}
                }}
            });
            return QJsonObject{
                {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                {QStringLiteral("id"), id},
                {QStringLiteral("result"), QJsonObject{{QStringLiteral("tools"), tools}}}
            };
        }
        return {};
    });

    llm::mcp::McpClient client(&transport);
    const auto tools = client.listTools();
    QCOMPARE(tools.size(), 1);
    QCOMPARE(tools.first().name, QStringLiteral("exec_sql"));
    QCOMPARE(tools.first().description, QStringLiteral("Execute SQL Query"));
    QVERIFY(tools.first().parameters.contains(QStringLiteral("properties")));
}

void AgentProtocolTests::mcpClientCallToolSuccessAndError() {
    MockMcpTransport transport;
    transport.setAutoResponder([](const QJsonObject& req) -> QJsonObject {
        const QString method = req.value(QStringLiteral("method")).toString();
        const int id = req.value(QStringLiteral("id")).toInt();
        if (method == QStringLiteral("tools/call")) {
            const auto params = req.value(QStringLiteral("params")).toObject();
            const QString name = params.value(QStringLiteral("name")).toString();
            if (name == QStringLiteral("fail_tool")) {
                return QJsonObject{
                    {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("id"), id},
                    {QStringLiteral("error"), QJsonObject{
                        {QStringLiteral("code"), -32602},
                        {QStringLiteral("message"), QStringLiteral("工具执行内部异常")}
                    }}
                };
            }
            QJsonArray contents;
            contents.append(QJsonObject{
                {QStringLiteral("type"), QStringLiteral("text")},
                {QStringLiteral("text"), QStringLiteral("MCP 执行成功输出")}
            });
            return QJsonObject{
                {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                {QStringLiteral("id"), id},
                {QStringLiteral("result"), QJsonObject{
                    {QStringLiteral("isError"), false},
                    {QStringLiteral("content"), contents}
                }}
            };
        }
        return {};
    });

    llm::mcp::McpClient client(&transport);

    // 成功调用
    const auto successRes = client.callTool(QStringLiteral("call_1"), QStringLiteral("ok_tool"), QStringLiteral("{}"));
    QCOMPARE(successRes.toolCallId, QStringLiteral("call_1"));
    QVERIFY(!successRes.isError);
    QCOMPARE(successRes.content, QStringLiteral("MCP 执行成功输出"));

    // 失败调用
    const auto failRes = client.callTool(QStringLiteral("call_2"), QStringLiteral("fail_tool"), QStringLiteral("{}"));
    QCOMPARE(failRes.toolCallId, QStringLiteral("call_2"));
    QVERIFY(failRes.isError);
    QCOMPARE(failRes.content, QStringLiteral("工具执行内部异常"));
}

void AgentProtocolTests::mcpClientTimeoutAndDisconnected() {
    MockMcpTransport transport;
    transport.setConnected(false);

    llm::mcp::McpClient client(&transport);
    const auto disconnRes = client.callTool(QStringLiteral("call_dis"), QStringLiteral("any_tool"), QStringLiteral("{}"));
    QVERIFY(disconnRes.isError);
    QVERIFY(disconnRes.content.contains(QStringLiteral("断开连接")));

    // 超时测试
    transport.setConnected(true);
    transport.setAutoResponder(nullptr); // 不产生任何回复
    const auto timeoutRes = client.callTool(QStringLiteral("call_to"), QStringLiteral("slow_tool"), QStringLiteral("{}"), 50);
    QVERIFY(timeoutRes.isError);
    QVERIFY(timeoutRes.content.contains(QStringLiteral("超时")));
}

QTEST_GUILESS_MAIN(AgentProtocolTests)
#include "AgentProtocolTests.moc"

