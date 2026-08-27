#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>

#include "domain/model/ResolvedModel.h"
#include "domain/llm/ChatRequest.h"
#include "domain/llm/ChatEvent.h"
#include "domain/llm/ResolvedChatOptions.h"
#include "llm/runtime/ChatRequestResolver.h"
#include "llm/protocol/anthropic/AnthropicProtocolAdapter.h"
#include "llm/protocol/anthropic/AnthropicStreamParser.h"
#include "llm/protocol/gemini/GeminiProtocolAdapter.h"
#include "llm/protocol/gemini/GeminiStreamParser.h"
#include "llm/protocol/openai/OpenAIChatCompletionsAdapter.h"
#include "llm/protocol/openai/OpenAIStreamParser.h"
#include "llm/protocol/openai_responses/OpenAIResponsesAdapter.h"
#include "llm/protocol/openai_responses/OpenAIResponsesStreamParser.h"
#include "llm/protocol/ollama/OllamaProtocolAdapter.h"
#include "data/importer/ModelsDevImporter.h"

#include "llm/mcp/McpClient.h"
#include "llm/mcp/IMcpTransport.h"
#include "llm/mcp/StdioMcpTransport.h"

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

static domain::model::ResolvedModel createTestModel(
    const QString &modelId = QStringLiteral("test-model"),
    domain::model::ProtocolType protocol = domain::model::ProtocolType::OpenAIChatCompletions,
    domain::model::ModelCapabilities caps = domain::model::ModelCapability::Chat | domain::model::ModelCapability::Streaming | domain::model::ModelCapability::ToolCalling | domain::model::ModelCapability::Thinking,
    int maxOutput = 8192
) {
    domain::model::ResolvedModel m;
    m.provider.id = QStringLiteral("test-provider");
    m.provider.protocol = protocol;
    m.binding.remoteModelId = modelId;
    domain::model::CanonicalModel c;
    c.id = modelId;
    c.capabilities = caps;
    c.limits.maxOutput = maxOutput;
    c.limits.context = 128000;
    c.defaultParams.temperature = 0.7;
    c.defaultParams.thinkingBudgetTokens = 4096;
    c.defaultParams.reasoningEffort = QStringLiteral("medium");
    m.canonical = c;
    return m;
}

class AgentProtocolTests final : public QObject {
    Q_OBJECT
private slots:
    // 既存协议适配与流解析测试
    void responsesUsesFunctionCallOutput();
    void ollamaKeepsMessageContent();
    void openAIChatCompletionsToolMappingAndStream();
    void anthropicToolMappingAndStream();
    void geminiToolMappingAndStream();
    void openAIResponsesToolStream();

    // ChatRequestResolver 语义解析测试
    void testResolverMaxTokensClamp();
    void testResolverThinkingAndBudget();
    void testResolverThinkingGatingWhenUnsupported();
    void testResolverToolCallingGating();
    void testResolverReasoningEffortPriority();

    // 协议适配器无硬编码与参数验证测试
    void testAnthropicNoHardcodedBudget();
    void testGeminiNoHardcodedBudget();
    void testOpenAIResponsesNoDefaultMedium();

    // ModelsDevImporter 元数据解析测试
    void testModelsDevCanonicalKeyMapping();
    void testModelsDevResolveProtocol();
    void testModelsDevReasoningSupport();

    // MCP 协议与传输测试
    void mcpClientInitializeAndHandshake();
    void mcpClientListTools();
    void mcpClientCallToolSuccessAndError();
    void mcpClientTimeoutAndDisconnected();
    void testStdioMcpTransportProcessLifecycleAndFraming();
    void testStdioMcpTransportInvalidExecutableError();
};

void AgentProtocolTests::responsesUsesFunctionCallOutput() {
    auto model = createTestModel(QStringLiteral("test-model"), domain::model::ProtocolType::OpenAIResponses);
    domain::llm::ChatRequest request;
    request.model = QStringLiteral("test-model");
    domain::llm::ChatMessage assistant;
    assistant.role = domain::MessageRole::Assistant;
    assistant.toolCalls = QList<domain::agent::ToolCall>{{QStringLiteral("call-1"), QStringLiteral("read_file"), QStringLiteral(R"({"path":"a.txt"})")}};
    request.messages.append(assistant);
    request.messages.append({domain::MessageRole::Tool, QStringLiteral("file body"), QStringLiteral("read_file"), QStringLiteral("call-1")});

    const auto options = llm::runtime::ChatRequestResolver::resolve(model, request);
    const auto http = llm::protocol::openai_responses::OpenAIResponsesAdapter{}.buildChatRequest(model, request, options);
    const auto input = QJsonDocument::fromJson(http.body).object().value(QStringLiteral("input")).toArray();
    QCOMPARE(input.size(), 2);
    QCOMPARE(input.at(0).toObject().value(QStringLiteral("type")).toString(), QStringLiteral("function_call"));
    QCOMPARE(input.at(1).toObject().value(QStringLiteral("type")).toString(), QStringLiteral("function_call_output"));
    QCOMPARE(input.at(1).toObject().value(QStringLiteral("call_id")).toString(), QStringLiteral("call-1"));
}

void AgentProtocolTests::ollamaKeepsMessageContent() {
    auto model = createTestModel(QStringLiteral("test-model"), domain::model::ProtocolType::OllamaChat);
    domain::llm::ChatRequest request;
    request.model = QStringLiteral("test-model");
    request.messages.append({domain::MessageRole::User, QStringLiteral("hello project")});

    const auto options = llm::runtime::ChatRequestResolver::resolve(model, request);
    const auto http = llm::protocol::ollama::OllamaProtocolAdapter{}.buildChatRequest(model, request, options);
    const auto messages = QJsonDocument::fromJson(http.body).object().value(QStringLiteral("messages")).toArray();
    QCOMPARE(messages.at(0).toObject().value(QStringLiteral("content")).toString(), QStringLiteral("hello project"));
}

void AgentProtocolTests::openAIChatCompletionsToolMappingAndStream() {
    auto model = createTestModel(QStringLiteral("gpt-4o"), domain::model::ProtocolType::OpenAIChatCompletions);
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

    const auto options = llm::runtime::ChatRequestResolver::resolve(model, request);
    const auto http = llm::protocol::openai::OpenAIChatCompletionsAdapter{}.buildChatRequest(model, request, options);
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
    auto model = createTestModel(QStringLiteral("claude-3-5-sonnet-20241022"), domain::model::ProtocolType::AnthropicMessages);
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

    const auto options = llm::runtime::ChatRequestResolver::resolve(model, request);
    const auto http = llm::protocol::anthropic::AnthropicProtocolAdapter{}.buildChatRequest(model, request, options);
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
    auto model = createTestModel(QStringLiteral("gemini-1.5-pro"), domain::model::ProtocolType::GeminiGenerateContent);
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

    const auto options = llm::runtime::ChatRequestResolver::resolve(model, request);
    const auto http = llm::protocol::gemini::GeminiProtocolAdapter{}.buildChatRequest(model, request, options);
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

    QCOMPARE(contents.at(1).toObject().value(QStringLiteral("role")).toString(), QStringLiteral("function"));
    const auto userParts = contents.at(1).toObject().value(QStringLiteral("parts")).toArray();
    QCOMPARE(userParts.at(0).toObject().value(QStringLiteral("functionResponse")).toObject().value(QStringLiteral("name")).toString(), QStringLiteral("search_text"));

    // Stream parser verification
    llm::protocol::gemini::GeminiStreamParser parser;
    QByteArray streamChunk = "data: {\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"search_text\",\"args\":{\"query\":\"main\"}}}]}}]}\n\n";
    auto events = parser.feed(streamChunk);
    QCOMPARE(events.size(), 3);
    QVERIFY(std::holds_alternative<domain::llm::EventToolCallStarted>(events[0]));
    QCOMPARE(std::get<domain::llm::EventToolCallStarted>(events[0]).functionName, QStringLiteral("search_text"));
    QVERIFY(std::holds_alternative<domain::llm::EventToolCallDelta>(events[1]));
    QVERIFY(std::holds_alternative<domain::llm::EventToolCallFinished>(events[2]));
}

void AgentProtocolTests::openAIResponsesToolStream() {
    llm::protocol::openai_responses::OpenAIResponsesStreamParser parser;
    QByteArray startChunk = "event: response.output_item.added\ndata: {\"item\":{\"id\":\"item_1\",\"type\":\"function_call\",\"call_id\":\"call_resp_1\",\"name\":\"edit_file\"}}\n\n";
    auto events1 = parser.feed(startChunk);
    QCOMPARE(events1.size(), 1);
    QVERIFY(std::holds_alternative<domain::llm::EventToolCallStarted>(events1[0]));
    QCOMPARE(std::get<domain::llm::EventToolCallStarted>(events1[0]).id, QStringLiteral("call_resp_1"));
    QCOMPARE(std::get<domain::llm::EventToolCallStarted>(events1[0]).functionName, QStringLiteral("edit_file"));

    QByteArray deltaChunk = "event: response.function_call_arguments.delta\ndata: {\"item_id\":\"item_1\",\"delta\":\"{\\\"content\\\": \\\"abc\\\"}\"}\n\n";
    auto events2 = parser.feed(deltaChunk);
    QCOMPARE(events2.size(), 1);
    QVERIFY(std::holds_alternative<domain::llm::EventToolCallDelta>(events2[0]));
    QCOMPARE(std::get<domain::llm::EventToolCallDelta>(events2[0]).argumentsDelta, QStringLiteral("{\"content\": \"abc\"}"));

    QByteArray doneChunk = "event: response.function_call_arguments.done\ndata: {\"item_id\":\"item_1\"}\n\n";
    auto events3 = parser.feed(doneChunk);
    QCOMPARE(events3.size(), 1);
    QVERIFY(std::holds_alternative<domain::llm::EventToolCallFinished>(events3[0]));
    QCOMPARE(std::get<domain::llm::EventToolCallFinished>(events3[0]).id, QStringLiteral("call_resp_1"));
}

void AgentProtocolTests::testResolverMaxTokensClamp() {
    auto model = createTestModel(QStringLiteral("test-model"), domain::model::ProtocolType::OpenAIChatCompletions, domain::model::ModelCapability::Chat, 2048);
    domain::llm::ChatRequest req;
    req.maxTokens = 4096; // 超过 limits.maxOutput (2048)

    const auto options = llm::runtime::ChatRequestResolver::resolve(model, req);
    QVERIFY(options.maxOutputTokens.has_value());
    QCOMPARE(options.maxOutputTokens.value(), 2048); // 必须被夹紧至 2048
}

void AgentProtocolTests::testResolverThinkingAndBudget() {
    auto model = createTestModel(QStringLiteral("test-model"), domain::model::ProtocolType::OpenAIChatCompletions,
                                domain::model::ModelCapability::Chat | domain::model::ModelCapability::Thinking);
    model.canonical->defaultParams.thinkingBudgetTokens = 8192;
    model.canonical->defaultParams.reasoningEffort = QStringLiteral("high");

    domain::llm::ChatRequest req;
    req.useDeepThinking = true;

    const auto options = llm::runtime::ChatRequestResolver::resolve(model, req);
    QVERIFY(options.thinkingEnabled);
    QCOMPARE(options.reasoningEffort, QStringLiteral("high"));
    QVERIFY(options.thinkingBudgetTokens.has_value());
    QCOMPARE(options.thinkingBudgetTokens.value(), 8192);
}

void AgentProtocolTests::testResolverThinkingGatingWhenUnsupported() {
    // 模型不支持 Thinking 能力
    auto model = createTestModel(QStringLiteral("test-model"), domain::model::ProtocolType::OpenAIChatCompletions,
                                domain::model::ModelCapability::Chat);
    domain::llm::ChatRequest req;
    req.useDeepThinking = true; // 用户请求开启

    const auto options = llm::runtime::ChatRequestResolver::resolve(model, req);
    QVERIFY(!options.thinkingEnabled); // 必须被门控关闭
}

void AgentProtocolTests::testResolverToolCallingGating() {
    // 1. 模型不支持 ToolCalling
    auto noToolModel = createTestModel(QStringLiteral("test-model"), domain::model::ProtocolType::OpenAIChatCompletions,
                                      domain::model::ModelCapability::Chat);
    domain::llm::ChatRequest req;
    req.tools = QList<domain::agent::ToolDefinition>{
        domain::agent::ToolDefinition{QStringLiteral("t1"), QStringLiteral("desc"), QJsonObject{}}
    };

    auto options1 = llm::runtime::ChatRequestResolver::resolve(noToolModel, req);
    QVERIFY(!options1.toolsEnabled);

    // 2. 模型支持 ToolCalling
    auto toolModel = createTestModel(QStringLiteral("test-model"), domain::model::ProtocolType::OpenAIChatCompletions,
                                    domain::model::ModelCapability::Chat | domain::model::ModelCapability::ToolCalling);
    auto options2 = llm::runtime::ChatRequestResolver::resolve(toolModel, req);
    QVERIFY(options2.toolsEnabled);
}

void AgentProtocolTests::testResolverReasoningEffortPriority() {
    auto model = createTestModel(QStringLiteral("test-model"), domain::model::ProtocolType::OpenAIChatCompletions,
                                domain::model::ModelCapability::Chat | domain::model::ModelCapability::Thinking);
    model.canonical->defaultParams.reasoningEffort = QStringLiteral("low");

    domain::llm::ChatRequest req;
    req.useDeepThinking = true;
    req.reasoningEffort = QStringLiteral("high"); // 请求级覆盖

    const auto options = llm::runtime::ChatRequestResolver::resolve(model, req);
    QCOMPARE(options.reasoningEffort, QStringLiteral("high")); // 必须优先采用请求级
}

void AgentProtocolTests::testAnthropicNoHardcodedBudget() {
    auto model = createTestModel(QStringLiteral("claude-3-7-sonnet"), domain::model::ProtocolType::AnthropicMessages);
    domain::llm::ChatRequest req;
    req.model = QStringLiteral("claude-3-7-sonnet");
    req.useDeepThinking = true;

    domain::llm::ResolvedChatOptions options;
    options.thinkingEnabled = true;
    options.thinkingBudgetTokens = 12345; // 自定义预算，非原硬编码 1024/8192/16384

    const auto http = llm::protocol::anthropic::AnthropicProtocolAdapter{}.buildChatRequest(model, req, options);
    const auto doc = QJsonDocument::fromJson(http.body).object();
    const auto thinking = doc.value(QStringLiteral("thinking")).toObject();
    QCOMPARE(thinking.value(QStringLiteral("type")).toString(), QStringLiteral("enabled"));
    QCOMPARE(thinking.value(QStringLiteral("budget_tokens")).toInt(), 12345);
}

void AgentProtocolTests::testGeminiNoHardcodedBudget() {
    auto model = createTestModel(QStringLiteral("gemini-2.0-flash-thinking-exp"), domain::model::ProtocolType::GeminiGenerateContent);
    domain::llm::ChatRequest req;
    req.model = QStringLiteral("gemini-2.0-flash-thinking-exp");
    req.useDeepThinking = true;

    domain::llm::ResolvedChatOptions options;
    options.thinkingEnabled = true;
    options.thinkingBudgetTokens = 9999; // 非硬编码

    const auto http = llm::protocol::gemini::GeminiProtocolAdapter{}.buildChatRequest(model, req, options);
    const auto doc = QJsonDocument::fromJson(http.body).object();
    const auto genConfig = doc.value(QStringLiteral("generationConfig")).toObject();
    const auto thinkingConfig = genConfig.value(QStringLiteral("thinkingConfig")).toObject();
    QCOMPARE(thinkingConfig.value(QStringLiteral("thinkingBudget")).toInt(), 9999);
}

void AgentProtocolTests::testOpenAIResponsesNoDefaultMedium() {
    auto model = createTestModel(QStringLiteral("gpt-4o"), domain::model::ProtocolType::OpenAIResponses);
    domain::llm::ChatRequest req;
    req.model = QStringLiteral("gpt-4o");

    domain::llm::ResolvedChatOptions options;
    options.thinkingEnabled = false; // 关闭思考

    const auto http = llm::protocol::openai_responses::OpenAIResponsesAdapter{}.buildChatRequest(model, req, options);
    const auto doc = QJsonDocument::fromJson(http.body).object();
    QVERIFY(!doc.contains(QStringLiteral("reasoning"))); // 不应有默认 medium reasoning 注入
}

void AgentProtocolTests::testModelsDevCanonicalKeyMapping() {
    QJsonObject modelsRoot{
        {QStringLiteral("openai/gpt-4o"), QJsonObject{
            {QStringLiteral("id"), QStringLiteral("gpt-4o-canonical")},
            {QStringLiteral("name"), QStringLiteral("GPT-4o")},
            {QStringLiteral("limit"), QJsonObject{{QStringLiteral("context"), 128000}, {QStringLiteral("output"), 4096}}}
        }}
    };

    QJsonObject apiRoot{
        {QStringLiteral("openai"), QJsonObject{
            {QStringLiteral("name"), QStringLiteral("OpenAI")},
            {QStringLiteral("npm"), QStringLiteral("@ai-sdk/openai")},
            {QStringLiteral("models"), QJsonObject{
                {QStringLiteral("openai/gpt-4o"), QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("gpt-4o-2024-08-06")} // remoteId 与 canonicalId 不同
                }}
            }}
        }}
    };

    auto result = data::importer::ModelsDevImporter::parseAll(apiRoot, modelsRoot);
    QCOMPARE(result.canonicalModels.size(), 1);
    QCOMPARE(result.providerModels.size(), 1);

    const auto &binding = result.providerModels.first();
    QCOMPARE(binding.remoteModelId, QStringLiteral("gpt-4o-2024-08-06"));
    QVERIFY(binding.canonicalModelId.has_value());
    QCOMPARE(*binding.canonicalModelId, QStringLiteral("gpt-4o-canonical")); // 正确关联至 Canonical 模型
    QCOMPARE(result.unresolvedBindingsCount, 0);
}

void AgentProtocolTests::testModelsDevResolveProtocol() {
    // 1. ForgeAI Provider Override 覆盖表
    QJsonObject openAiProviderObj{{QStringLiteral("npm"), QStringLiteral("@ai-sdk/openai")}};
    QCOMPARE(data::importer::ModelsDevImporter::resolveProtocol(QStringLiteral("openai"), openAiProviderObj),
             domain::model::ProtocolType::OpenAIResponses);

    QJsonObject deepseekProviderObj{{QStringLiteral("npm"), QStringLiteral("@ai-sdk/openai-compatible")}};
    QCOMPARE(data::importer::ModelsDevImporter::resolveProtocol(QStringLiteral("deepseek"), deepseekProviderObj),
             domain::model::ProtocolType::OpenAIResponses);

    QJsonObject ollamaNoNpmObj{};
    QCOMPARE(data::importer::ModelsDevImporter::resolveProtocol(QStringLiteral("ollama"), ollamaNoNpmObj),
             domain::model::ProtocolType::OllamaChat);

    // 2. models.dev native SDK 家族映射
    QJsonObject anthropicObj{{QStringLiteral("npm"), QStringLiteral("@ai-sdk/anthropic")}};
    QCOMPARE(data::importer::ModelsDevImporter::resolveProtocol(QStringLiteral("anthropic"), anthropicObj),
             domain::model::ProtocolType::AnthropicMessages);

    QJsonObject googleObj{{QStringLiteral("npm"), QStringLiteral("@ai-sdk/google")}};
    QCOMPARE(data::importer::ModelsDevImporter::resolveProtocol(QStringLiteral("google"), googleObj),
             domain::model::ProtocolType::GeminiGenerateContent);

    // 3. @ai-sdk/openai-compatible 以及社区 OpenAI 兼容衍生包映射到 OpenAIChatCompletions
    QJsonObject compatibleObj{{QStringLiteral("npm"), QStringLiteral("@ai-sdk/openai-compatible")}};
    QCOMPARE(data::importer::ModelsDevImporter::resolveProtocol(QStringLiteral("hpc-ai"), compatibleObj),
             domain::model::ProtocolType::OpenAIChatCompletions);

    QJsonObject openrouterObj{{QStringLiteral("npm"), QStringLiteral("@openrouter/ai-sdk-provider")}};
    QCOMPARE(data::importer::ModelsDevImporter::resolveProtocol(QStringLiteral("openrouter"), openrouterObj),
             domain::model::ProtocolType::OpenAIChatCompletions);

    QJsonObject groqObj{{QStringLiteral("npm"), QStringLiteral("@ai-sdk/groq")}};
    QCOMPARE(data::importer::ModelsDevImporter::resolveProtocol(QStringLiteral("groq"), groqObj),
             domain::model::ProtocolType::OpenAIChatCompletions);

    QJsonObject metaObj{{QStringLiteral("npm"), QStringLiteral("@ai-sdk/openai")}};
    QCOMPARE(data::importer::ModelsDevImporter::resolveProtocol(QStringLiteral("meta"), metaObj),
             domain::model::ProtocolType::OpenAIChatCompletions);

    // 4. 未知/不支持的 npm package 坚决返回 Unknown（不盲目默认 OpenAI）
    QJsonObject unknownNpmObj{{QStringLiteral("npm"), QStringLiteral("@ai-sdk/cohere-native")}};
    QCOMPARE(data::importer::ModelsDevImporter::resolveProtocol(QStringLiteral("cohere"), unknownNpmObj),
             domain::model::ProtocolType::Unknown);
}

void AgentProtocolTests::testModelsDevReasoningSupport() {
    QJsonObject modelObj{
        {QStringLiteral("id"), QStringLiteral("deepseek-reasoner")},
        {QStringLiteral("reasoning"), true},
        {QStringLiteral("reasoning_options"), QJsonArray{
            QJsonObject{{QStringLiteral("type"), QStringLiteral("effort")}, {QStringLiteral("effort"), QJsonArray{QStringLiteral("low"), QStringLiteral("high")}}},
            QJsonObject{{QStringLiteral("type"), QStringLiteral("budget")}, {QStringLiteral("min"), 1024}, {QStringLiteral("max"), 65536}, {QStringLiteral("default"), 4096}}
        }}
    };

    auto binding = data::importer::ModelsDevImporter::parseProviderModel(
        QStringLiteral("deepseek"), QStringLiteral("DeepSeek"), QStringLiteral("deepseek-reasoner"), modelObj
    );

    QVERIFY(binding.reasoningSupport.has_value());
    const auto &supp = *binding.reasoningSupport;
    QVERIFY(supp.supported);
    QCOMPARE(supp.effortLevels, QStringList({QStringLiteral("low"), QStringLiteral("high")}));
    QCOMPARE(supp.minBudgetTokens.value_or(0), 1024);
    QCOMPARE(supp.maxBudgetTokens.value_or(0), 65536);
    QCOMPARE(supp.defaultBudgetTokens.value_or(0), 4096);
}

void AgentProtocolTests::mcpClientInitializeAndHandshake() {
    MockMcpTransport transport;
    transport.setAutoResponder([](const QJsonObject& req) -> QJsonObject {
        QString method = req.value("method").toString();
        int id = req.value("id").toInt();
        if (method == "initialize") {
            return QJsonObject{
                {"jsonrpc", "2.0"},
                {"id", id},
                {"result", QJsonObject{
                    {"protocolVersion", "2024-11-05"},
                    {"serverInfo", QJsonObject{{"name", "mock-server"}, {"version", "1.0.0"}}},
                    {"capabilities", QJsonObject{{"tools", QJsonObject{}}}}
                }}
            };
        }
        return {};
    });

    llm::mcp::McpClient client(&transport);
    bool initOk = client.initialize(1000);
    QVERIFY(initOk);
}

void AgentProtocolTests::mcpClientListTools() {
    MockMcpTransport transport;
    transport.setAutoResponder([](const QJsonObject& req) -> QJsonObject {
        QString method = req.value("method").toString();
        int id = req.value("id").toInt();
        if (method == "initialize") {
            return QJsonObject{
                {"jsonrpc", "2.0"},
                {"id", id},
                {"result", QJsonObject{
                    {"protocolVersion", "2024-11-05"},
                    {"capabilities", QJsonObject{{"tools", QJsonObject{}}}}
                }}
            };
        }
        if (method == "tools/list") {
            QJsonArray tools;
            tools.append(QJsonObject{
                {"name", "query_sql"},
                {"description", "Execute SQL query"},
                {"inputSchema", QJsonObject{{"type", "object"}}}
            });
            return QJsonObject{
                {"jsonrpc", "2.0"},
                {"id", id},
                {"result", QJsonObject{{"tools", tools}}}
            };
        }
        return {};
    });

    llm::mcp::McpClient client(&transport);
    bool initOk = client.initialize(1000);
    QVERIFY(initOk);

    auto receivedTools = client.listTools(1000);
    QCOMPARE(receivedTools.size(), 1);
    QCOMPARE(receivedTools[0].name, QStringLiteral("query_sql"));
}

void AgentProtocolTests::mcpClientCallToolSuccessAndError() {
    MockMcpTransport transport;
    transport.setAutoResponder([](const QJsonObject& req) -> QJsonObject {
        QString method = req.value("method").toString();
        int id = req.value("id").toInt();
        if (method == "initialize") {
            return QJsonObject{
                {"jsonrpc", "2.0"},
                {"id", id},
                {"result", QJsonObject{
                    {"protocolVersion", "2024-11-05"},
                    {"capabilities", QJsonObject{{"tools", QJsonObject{}}}}
                }}
            };
        }
        if (method == "tools/call") {
            QJsonObject params = req.value("params").toObject();
            QString name = params.value("name").toString();
            if (name == "success_tool") {
                QJsonArray content;
                content.append(QJsonObject{{"type", "text"}, {"text", "result_ok"}});
                return QJsonObject{
                    {"jsonrpc", "2.0"},
                    {"id", id},
                    {"result", QJsonObject{{"content", content}}}
                };
            }
            if (name == "error_tool") {
                return QJsonObject{
                    {"jsonrpc", "2.0"},
                    {"id", id},
                    {"error", QJsonObject{{"code", -32603}, {"message", "Tool execution failed"}}}
                };
            }
        }
        return {};
    });

    llm::mcp::McpClient client(&transport);
    bool initOk = client.initialize(1000);
    QVERIFY(initOk);

    // Call success
    auto successResult = client.callTool(QStringLiteral("call-1"), QStringLiteral("success_tool"), QStringLiteral("{}"), 1000);
    QVERIFY(!successResult.isError);
    QCOMPARE(successResult.content, QStringLiteral("result_ok"));

    // Call error
    auto errorResult = client.callTool(QStringLiteral("call-2"), QStringLiteral("error_tool"), QStringLiteral("{}"), 1000);
    QVERIFY(errorResult.isError);
    QCOMPARE(errorResult.content, QStringLiteral("Tool execution failed"));
}

void AgentProtocolTests::mcpClientTimeoutAndDisconnected() {
    MockMcpTransport transport;
    // 不设 autoResponder，使请求必定超时
    transport.setAutoResponder([](const QJsonObject&) -> QJsonObject {
        return {};
    });

    llm::mcp::McpClient client(&transport);
    bool initOk = client.initialize(50); // 50ms 超时
    QVERIFY(!initOk);
    QVERIFY(!client.lastError().isEmpty());
}

void AgentProtocolTests::testStdioMcpTransportProcessLifecycleAndFraming() {
    llm::mcp::McpServerConfig config;
#if defined(Q_OS_WIN)
    config.command = QStringLiteral("powershell.exe");
    config.args = {QStringLiteral("-NoProfile"), QStringLiteral("-Command"), QStringLiteral("Write-Output '{\"jsonrpc\":\"2.0\",\"id\":100,\"result\":{\"status\":\"alive\"}}'")};
#else
    config.command = QStringLiteral("sh");
    config.args = {QStringLiteral("-c"), QStringLiteral("echo '{\"jsonrpc\":\"2.0\",\"id\":100,\"result\":{\"status\":\"alive\"}}'")};
#endif

    llm::mcp::StdioMcpTransport transport(config);

    bool messageReceived = false;
    QJsonObject receivedObj;
    QEventLoop loop;

    QObject::connect(&transport, &llm::mcp::IMcpTransport::messageReceived, [&](const QJsonObject& obj) {
        messageReceived = true;
        receivedObj = obj;
        loop.quit();
    });

    QTimer::singleShot(4000, &loop, &QEventLoop::quit);

    QVERIFY(transport.start());
    loop.exec();

    QVERIFY2(messageReceived, "StdioMcpTransport should parse newline-delimited JSON output from subprocess");
    QCOMPARE(receivedObj.value(QStringLiteral("id")).toInt(), 100);
    QCOMPARE(receivedObj.value(QStringLiteral("result")).toObject().value(QStringLiteral("status")).toString(), QStringLiteral("alive"));

    transport.close();
    QVERIFY(!transport.isConnected());
}

void AgentProtocolTests::testStdioMcpTransportInvalidExecutableError() {
    llm::mcp::McpServerConfig config;
    config.command = QStringLiteral("non_existent_executable_12345678");

    llm::mcp::StdioMcpTransport transport(config);

    bool errorOccurred = false;
    QString errorMsg;
    QObject::connect(&transport, &llm::mcp::IMcpTransport::errorOccurred, [&](const QString& err) {
        errorOccurred = true;
        errorMsg = err;
    });

    QVERIFY(!transport.start());
    QVERIFY(errorOccurred);
    QVERIFY(errorMsg.contains(QStringLiteral("non_existent_executable_12345678")) || errorMsg.contains(QStringLiteral("启动失败")));
}

QTEST_GUILESS_MAIN(AgentProtocolTests)
#include "AgentProtocolTests.moc"
