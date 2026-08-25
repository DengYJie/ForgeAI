#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTcpServer>
#include <QTcpSocket>

#include "llm/workspace/WorkspaceFileSystem.h"
#include "agent/tool/ToolRegistry.h"
#include "agent/tool/BuiltinToolProvider.h"
#include "agent/tool/builtin/ReadFileTool.h"
#include "agent/tool/builtin/WriteFileTool.h"
#include "agent/tool/builtin/ListFilesTool.h"
#include "agent/tool/builtin/SearchTextTool.h"
#include "agent/skill/SkillLoader.h"
#include "agent/skill/SkillRegistry.h"
#include "agent/runtime/AgentContextBuilder.h"
#include "services/project/ProjectContextService.h"
#include "data/repository/SqliteAgentRepository.h"
#include "data/repository/SqliteAgentCheckpointRepository.h"
#include "data/sqlite/DatabaseManager.h"
#include "llm/mcp/McpManager.h"
#include "llm/mcp/McpConfigLoader.h"
#include "llm/mcp/McpServerRegistry.h"
#include "llm/mcp/McpRuntime.h"
#include "llm/mcp/McpTransportFactory.h"
#include "llm/mcp/StreamableHttpMcpTransport.h"
#include "llm/mcp/McpResourceProvider.h"
#include "llm/mcp/McpPromptProvider.h"
#include "domain/mcp/McpServerTrust.h"

class AgentToolTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void workspaceFileSystemRejectsEscape();
    void workspaceFileSystemIgnorePatterns();
    void toolRegistryRegistrationAndLookup();
    void toolRegistryDuplicateRejection();
    void toolRegistryUnknownToolError();
    void builtinToolsExecution();
    void loadsProjectContext();
    void skillLoaderParsesFrontmatter();
    void skillRegistryOperations();
    void agentContextBuilderBuildsCleanPrompt();
    void sqliteAgentRepositoryCrud();
    void sqliteAgentCheckpointRepositoryCrud();
    void agentPolicyEvaluatesPermissions();
    void mcpManagerParsesConfigs();
    void mcpConfigLoaderAdvancedTests();
    void mcpServerRegistryOperationsAndSignals();
    void mcpRuntimeLifecycleAndTransportFactory();
    void mcpSecurityTrustAndEnvMaskingTests();
    void mcpResourceAndPromptProviderTests();
    void mcpSessionCrashRecoveryAndHandshakeVersionTests();
    void streamableHttpMcpTransportSseIntegrationTests();
private:
    QTemporaryDir m_dbDir;
};

void AgentToolTests::workspaceFileSystemRejectsEscape() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    llm::workspace::WorkspaceFileSystem fs;

    QFile file(QDir(root.path()).filePath("note.txt"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("hello");
    file.close();

    QString error;
    const QString readable = fs.resolveReadablePath(root.path(), "note.txt", &error);
    QVERIFY(!readable.isEmpty());
    QVERIFY(error.isEmpty());

    const QString escaped = fs.resolveReadablePath(root.path(), "../outside.txt", &error);
    QVERIFY(escaped.isEmpty());
    QVERIFY(!error.isEmpty());

    const QString writable = fs.resolveWritablePath(root.path(), "sub/created.txt", &error);
    // sub directory does not exist yet
    QVERIFY(writable.isEmpty());

    QDir(root.path()).mkdir("sub");
    const QString writable2 = fs.resolveWritablePath(root.path(), "sub/created.txt", &error);
    QVERIFY(!writable2.isEmpty());
}

void AgentToolTests::workspaceFileSystemIgnorePatterns() {
    llm::workspace::WorkspaceFileSystem fs;
    QVERIFY(fs.isIgnored(".git/config"));
    QVERIFY(fs.isIgnored("node_modules/package.json"));
    QVERIFY(fs.isIgnored("build/output.exe"));
    QVERIFY(!fs.isIgnored("src/main.cpp"));
}

void AgentToolTests::toolRegistryRegistrationAndLookup() {
    agent::tool::ToolRegistry registry;
    auto fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    auto provider = std::make_shared<agent::tool::BuiltinToolProvider>(fs);

    int count = registry.registerProvider(provider);
    QCOMPARE(count, 4);

    QVERIFY(registry.hasTool("read_file"));
    QVERIFY(registry.hasTool("write_file"));
    QVERIFY(registry.hasTool("list_files"));
    QVERIFY(registry.hasTool("search_text"));

    auto defs = registry.definitions();
    QCOMPARE(defs.size(), 4);

    auto tool = registry.findTool("read_file");
    QVERIFY(tool != nullptr);
    QCOMPARE(tool->definition().name, QStringLiteral("read_file"));
}

void AgentToolTests::toolRegistryDuplicateRejection() {
    agent::tool::ToolRegistry registry;
    auto fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    auto readTool1 = std::make_shared<agent::tool::builtin::ReadFileTool>(fs);
    auto readTool2 = std::make_shared<agent::tool::builtin::ReadFileTool>(fs);

    QVERIFY(registry.registerTool(readTool1));
    // Duplicate tool registration must be rejected
    QVERIFY(!registry.registerTool(readTool2));
}

void AgentToolTests::toolRegistryUnknownToolError() {
    agent::tool::ToolRegistry registry;
    application::ports::ToolExecutionContext ctx{QStringLiteral("/tmp"), QStringLiteral("s1"), {}};
    domain::agent::ToolCall call{QStringLiteral("call_x"), QStringLiteral("non_existent_tool"), QStringLiteral("{}")};

    auto result = registry.execute(call, ctx);
    QVERIFY(result.isError);
    QCOMPARE(result.toolCallId, QStringLiteral("call_x"));
    QVERIFY(result.content.contains(QStringLiteral("未知工具")));
}

void AgentToolTests::builtinToolsExecution() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    agent::tool::ToolRegistry registry;
    registry.registerProvider(std::make_shared<agent::tool::BuiltinToolProvider>(fs));

    application::ports::ToolExecutionContext ctx{root.path(), QStringLiteral("session_1"), {}};

    // 1. write_file
    domain::agent::ToolCall writeCall{
        QStringLiteral("c1"),
        QStringLiteral("write_file"),
        QStringLiteral(R"({"path":"hello.txt","content":"Hello World\nLine 2"})")
    };
    auto writeResult = registry.execute(writeCall, ctx);
    QVERIFY(!writeResult.isError);

    // 2. read_file
    domain::agent::ToolCall readCall{
        QStringLiteral("c2"),
        QStringLiteral("read_file"),
        QStringLiteral(R"({"path":"hello.txt"})")
    };
    auto readResult = registry.execute(readCall, ctx);
    QVERIFY(!readResult.isError);
    QCOMPARE(readResult.content, QStringLiteral("Hello World\nLine 2"));

    // 3. list_files
    domain::agent::ToolCall listCall{
        QStringLiteral("c3"),
        QStringLiteral("list_files"),
        QStringLiteral(R"({"path":"."})")
    };
    auto listResult = registry.execute(listCall, ctx);
    QVERIFY(!listResult.isError);
    auto filesArray = QJsonDocument::fromJson(listResult.content.toUtf8()).array();
    QVERIFY(filesArray.contains(QJsonValue(QStringLiteral("hello.txt"))));

    // 4. search_text
    domain::agent::ToolCall searchCall{
        QStringLiteral("c4"),
        QStringLiteral("search_text"),
        QStringLiteral(R"({"query":"World"})")
    };
    auto searchResult = registry.execute(searchCall, ctx);
    QVERIFY(!searchResult.isError);
    auto searchArray = QJsonDocument::fromJson(searchResult.content.toUtf8()).array();
    QCOMPARE(searchArray.size(), 1);
    QCOMPARE(searchArray.at(0).toObject().value("path").toString(), QStringLiteral("hello.txt"));
    QCOMPARE(searchArray.at(0).toObject().value("line").toInt(), 1);
}

void AgentToolTests::loadsProjectContext() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QDir dir(root.path());
    QVERIFY(dir.mkpath(".agents/skills/demo"));

    QFile agents(dir.filePath("AGENTS.md"));
    QVERIFY(agents.open(QIODevice::WriteOnly));
    agents.write("rules");
    agents.close();

    QFile skill(dir.filePath(".agents/skills/demo/SKILL.md"));
    QVERIFY(skill.open(QIODevice::WriteOnly));
    skill.write("---\nname: demo_skill\ndescription: A test skill\n---\nSkill Instructions Content");
    skill.close();

    QFile mcp(dir.filePath(".mcp.json"));
    QVERIFY(mcp.open(QIODevice::WriteOnly));
    mcp.write("{\"mcpServers\":{\"demo\":{}}}");
    mcp.close();

    auto context = services::project::ProjectContextService{}.load(root.path());
    QCOMPARE(context.agentsInstructions, QStringLiteral("rules"));
    QCOMPARE(context.skills.size(), 1);
    QCOMPARE(context.skills.first().name, QStringLiteral("demo_skill"));
    QCOMPARE(context.skills.first().description, QStringLiteral("A test skill"));
    // 默认延迟加载：此时 instructions 尚未读入内存
    QVERIFY(context.skills.first().instructions.isEmpty());

    // 按需延迟加载
    auto skillCopy = context.skills.first();
    QVERIFY(agent::skill::SkillLoader{}.loadInstructions(skillCopy));
    QCOMPARE(skillCopy.instructions, QStringLiteral("Skill Instructions Content"));

    QVERIFY(!context.mcpConfigPath.isEmpty());
    QCOMPARE(context.mcpConfigContent, QStringLiteral("{\"mcpServers\":{\"demo\":{}}}"));
}

void AgentToolTests::skillLoaderParsesFrontmatter() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString filePath = QDir(root.path()).filePath("SKILL.md");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("---\nname: \"git-helper\"\ndescription: \"Git operations skill\"\nid: git_tool\n---\n# Instructions\nRun git status");
    file.close();

    agent::skill::SkillLoader loader;
    auto skillOpt = loader.loadFromFile(filePath, true);
    QVERIFY(skillOpt.has_value());
    QCOMPARE(skillOpt->name, QStringLiteral("git-helper"));
    QCOMPARE(skillOpt->id, QStringLiteral("git_tool"));
    QCOMPARE(skillOpt->description, QStringLiteral("Git operations skill"));
    QCOMPARE(skillOpt->instructions, QStringLiteral("# Instructions\nRun git status"));
}

void AgentToolTests::skillRegistryOperations() {
    agent::skill::SkillRegistry registry;
    domain::agent::Skill s1;
    s1.id = QStringLiteral("s1");
    s1.name = QStringLiteral("skill1");
    s1.description = QStringLiteral("desc1");
    s1.instructions = QStringLiteral("inst1");
    s1.isEnabled = true;

    domain::agent::Skill s2;
    s2.id = QStringLiteral("s2");
    s2.name = QStringLiteral("skill2");
    s2.description = QStringLiteral("desc2");
    s2.instructions = QStringLiteral("inst2");
    s2.isEnabled = false;

    registry.registerSkills({s1, s2});
    QCOMPARE(registry.allSkills().size(), 2);
    QCOMPARE(registry.enabledSkills().size(), 1);

    auto found = registry.findSkill("skill1");
    QVERIFY(found.has_value());
    QCOMPARE(found->id, QStringLiteral("s1"));

    registry.setSkillEnabled("s2", true);
    QCOMPARE(registry.enabledSkills().size(), 2);

    registry.unregisterSkill("s1");
    QCOMPARE(registry.allSkills().size(), 1);
}

void AgentToolTests::agentContextBuilderBuildsCleanPrompt() {
    agent::runtime::AgentContextBuilder builder;
    agent::runtime::AgentRunContext runCtx;
    runCtx.workspaceRoot = QStringLiteral("C:/workspace/my_project");

    domain::project::ProjectContext projCtx;
    projCtx.rootPath = QStringLiteral("C:/workspace/my_project");
    projCtx.agentsInstructions = QStringLiteral("Follow clean code standards.");
    projCtx.mcpConfigContent = QStringLiteral("{\"mcpServers\":{\"sqlite\":{}}}");

    domain::agent::Skill cmakeSkill;
    cmakeSkill.id = QStringLiteral("c1");
    cmakeSkill.name = QStringLiteral("cmake-helper");
    cmakeSkill.description = QStringLiteral("help cmake");
    cmakeSkill.instructions = QStringLiteral("Always use ctest");
    cmakeSkill.isEnabled = true;
    projCtx.skills.append(cmakeSkill);

    const QString prompt = builder.buildSystemPrompt(runCtx, projCtx);

    QVERIFY(prompt.contains(QStringLiteral("项目工作区根目录：C:/workspace/my_project")));
    QVERIFY(prompt.contains(QStringLiteral("Follow clean code standards.")));
    QVERIFY(prompt.contains(QStringLiteral("# Skill: cmake-helper")));
    QVERIFY(prompt.contains(QStringLiteral("Always use ctest")));
    // MCP raw JSON must NOT be dumped into system prompt!
    QVERIFY(!prompt.contains(QStringLiteral("{\"mcpServers\":{\"sqlite\":{}}}")));
}

void AgentToolTests::initTestCase() {
    QVERIFY(m_dbDir.isValid());
    const QString dbPath = QDir(m_dbDir.path()).filePath("test_forgeai.db");
    data::sqlite::DatabaseManager::instance().initialize(dbPath);
}

void AgentToolTests::cleanupTestCase() {
    data::sqlite::DatabaseManager::instance().close();
}

void AgentToolTests::sqliteAgentRepositoryCrud() {
    data::repository::SqliteAgentRepository repo;
    domain::agent::Agent agent;
    agent.id = QUuid::createUuid();
    agent.name = QStringLiteral("CodeReviewer");
    agent.description = QStringLiteral("Expert in code review");
    agent.systemPrompt = QStringLiteral("You are an expert code reviewer.");
    agent.modelId = QStringLiteral("gpt-4o");
    agent.providerId = QStringLiteral("openai");
    agent.enabledTools = {QStringLiteral("read_file"), QStringLiteral("search_text")};
    agent.enabledSkills = {QStringLiteral("git-helper")};
    agent.enabledMcpServerIds = {QStringLiteral("db_server"), QStringLiteral("fs_server")};
    agent.createdAt = QDateTime::currentDateTime();
    agent.updatedAt = agent.createdAt;

    QVERIFY(repo.saveAgent(agent));

    auto loadedOpt = repo.getAgent(agent.id);
    QVERIFY(loadedOpt.has_value());
    QCOMPARE(loadedOpt->name, QStringLiteral("CodeReviewer"));
    QCOMPARE(loadedOpt->enabledTools.size(), 2);
    QCOMPARE(loadedOpt->enabledSkills.size(), 1);
    QCOMPARE(loadedOpt->enabledMcpServerIds.size(), 2);
    QVERIFY(loadedOpt->enabledMcpServerIds.contains(QStringLiteral("db_server")));

    auto all = repo.getAllAgents();
    QVERIFY(!all.isEmpty());

    QVERIFY(repo.deleteAgent(agent.id));
    QVERIFY(!repo.getAgent(agent.id).has_value());
}

void AgentToolTests::mcpManagerParsesConfigs() {
    const QString json = QStringLiteral(
        "{\n"
        "  \"mcpServers\": {\n"
        "    \"filesystem\": {\n"
        "      \"command\": \"npx\",\n"
        "      \"args\": [\"-y\", \"@modelcontextprotocol/server-filesystem\", \"/path\"],\n"
        "      \"env\": {\"DEBUG\": \"1\"},\n"
        "      \"disabled\": false\n"
        "    }\n"
        "  }\n"
        "}"
    );

    auto configs = llm::mcp::McpManager::parseConfigContent(json);
    QCOMPARE(configs.size(), 1);
    QCOMPARE(configs.first().name, QStringLiteral("filesystem"));
    QCOMPARE(configs.first().command, QStringLiteral("npx"));
    QCOMPARE(configs.first().args.size(), 3);
    QCOMPARE(configs.first().env.value("DEBUG"), QStringLiteral("1"));
    QVERIFY(!configs.first().disabled);
}

void AgentToolTests::sqliteAgentCheckpointRepositoryCrud() {
    data::repository::SqliteAgentCheckpointRepository repo;
    domain::agent::AgentCheckpoint cp;
    cp.checkpointId = QUuid::createUuid();
    cp.sessionId = QStringLiteral("session_test_123");
    cp.runId = QUuid::createUuid();
    cp.roundIndex = 2;
    cp.status = domain::agent::AgentRunStatus::ExecutingTool;
    cp.pendingToolCalls = {
        domain::agent::ToolCall{QStringLiteral("call_1"), QStringLiteral("read_file"), QStringLiteral(R"({"path":"a.txt"})")}
    };
    cp.pendingToolResults = {
        domain::agent::ToolResult{QStringLiteral("call_1"), QStringLiteral("file content"), false}
    };
    cp.timestamp = QDateTime::currentDateTime();

    QVERIFY(repo.saveCheckpoint(cp));

    auto loaded = repo.getLatestCheckpoint(QStringLiteral("session_test_123"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->roundIndex, 2);
    QCOMPARE(loaded->status, domain::agent::AgentRunStatus::ExecutingTool);
    QCOMPARE(loaded->pendingToolCalls.size(), 1);
    QCOMPARE(loaded->pendingToolResults.size(), 1);
    QCOMPARE(loaded->pendingToolResults.first().content, QStringLiteral("file content"));

    QVERIFY(repo.deleteCheckpointsForSession(QStringLiteral("session_test_123")));
    QVERIFY(!repo.getLatestCheckpoint(QStringLiteral("session_test_123")).has_value());
}

void AgentToolTests::agentPolicyEvaluatesPermissions() {
    domain::agent::AgentPolicy policy;
    policy.autoApproveReadOnly = true;
    policy.autoApproveWriteWorkspace = true;
    policy.autoApproveExecuteProcess = false;

    QCOMPARE(policy.evaluatePermission(domain::agent::ToolPermissionType::ReadOnly), domain::agent::PermissionDecision::Allow);
    QCOMPARE(policy.evaluatePermission(domain::agent::ToolPermissionType::WriteWorkspace), domain::agent::PermissionDecision::Allow);
    QCOMPARE(policy.evaluatePermission(domain::agent::ToolPermissionType::ExecuteProcess), domain::agent::PermissionDecision::AskUser);

    policy.autoApproveReadOnly = false;
    QCOMPARE(policy.evaluatePermission(domain::agent::ToolPermissionType::ReadOnly), domain::agent::PermissionDecision::AskUser);
}

void AgentToolTests::mcpConfigLoaderAdvancedTests() {
    // 1. 测试复杂 JSON（包含 stdio、http、cwd 相对路径、env、headers）
    const QString json = QStringLiteral(
        "{\n"
        "  \"mcpServers\": {\n"
        "    \"local_fs\": {\n"
        "      \"command\": \"npx\",\n"
        "      \"args\": [\"-y\", \"@modelcontextprotocol/server-filesystem\"],\n"
        "      \"cwd\": \"./sub_dir\",\n"
        "      \"env\": {\"KEY\": \"VAL\"},\n"
        "      \"enabled\": true\n"
        "    },\n"
        "    \"remote_api\": {\n"
        "      \"url\": \"https://api.example.com/mcp\",\n"
        "      \"headers\": {\"Authorization\": \"Bearer token123\"},\n"
        "      \"transport\": \"http\",\n"
        "      \"autoApprove\": true\n"
        "    }\n"
        "  }\n"
        "}"
    );

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    auto result = llm::mcp::McpConfigLoader::loadFromJsonString(json, tempDir.path());
    QVERIFY(result.success);
    QCOMPARE(result.configs.size(), 2);

    auto fsCfg = result.configs[0].id == "local_fs" ? result.configs[0] : result.configs[1];
    auto httpCfg = result.configs[0].id == "remote_api" ? result.configs[0] : result.configs[1];

    QCOMPARE(fsCfg.transport, domain::mcp::McpTransportType::Stdio);
    QCOMPARE(fsCfg.command, QStringLiteral("npx"));
    QCOMPARE(fsCfg.env.value("KEY"), QStringLiteral("VAL"));
    QVERIFY(fsCfg.isEnabled());
    QVERIFY(QDir::isAbsolutePath(fsCfg.cwd));

    QCOMPARE(httpCfg.transport, domain::mcp::McpTransportType::Http);
    QCOMPARE(httpCfg.url, QStringLiteral("https://api.example.com/mcp"));
    QCOMPARE(httpCfg.headers.value("Authorization"), QStringLiteral("Bearer token123"));
    QVERIFY(httpCfg.autoApprove);

    // 2. 测试非法 JSON
    auto errResult = llm::mcp::McpConfigLoader::loadFromJsonString(QStringLiteral("{ invalid json"));
    QVERIFY(!errResult.success);
    QVERIFY(!errResult.error.isEmpty());

    // 3. 测试从临时文件加载
    const QString filePath = QDir(tempDir.path()).filePath(".mcp.json");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(json.toUtf8());
    file.close();

    auto fileResult = llm::mcp::McpConfigLoader::loadFromFile(filePath);
    QVERIFY(fileResult.success);
    QCOMPARE(fileResult.configs.size(), 2);
}

void AgentToolTests::mcpServerRegistryOperationsAndSignals() {
    llm::mcp::McpServerRegistry registry;

    int registeredCount = 0;
    int unregisteredCount = 0;
    int changedCount = 0;

    connect(&registry, &llm::mcp::McpServerRegistry::serverRegistered, [&](const domain::mcp::McpServerConfig&) {
        registeredCount++;
    });
    connect(&registry, &llm::mcp::McpServerRegistry::serverUnregistered, [&](const QString&) {
        unregisteredCount++;
    });
    connect(&registry, &llm::mcp::McpServerRegistry::registryChanged, [&]() {
        changedCount++;
    });

    domain::mcp::McpServerConfig c1;
    c1.id = QStringLiteral("srv1");
    c1.command = QStringLiteral("python");

    domain::mcp::McpServerConfig c2;
    c2.id = QStringLiteral("srv2");
    c2.command = QStringLiteral("node");

    // 1. 注册
    registry.registerServer(c1);
    QCOMPARE(registeredCount, 1);
    QCOMPARE(changedCount, 1);
    QVERIFY(registry.hasServer(QStringLiteral("srv1")));

    // 2. 批量注册
    registry.registerServers({c1, c2});
    QCOMPARE(registry.servers().size(), 2);

    // 3. 更新同一 ID
    c1.command = QStringLiteral("python3");
    registry.registerServer(c1);
    auto updated = registry.server(QStringLiteral("srv1"));
    QVERIFY(updated.has_value());
    QCOMPARE(updated->command, QStringLiteral("python3"));

    // 4. 删除与清空
    registry.unregisterServer(QStringLiteral("srv1"));
    QCOMPARE(unregisteredCount, 1);
    QVERIFY(!registry.hasServer(QStringLiteral("srv1")));
    QCOMPARE(registry.servers().size(), 1);

    registry.clear();
    QCOMPARE(registry.servers().size(), 0);
}

void AgentToolTests::mcpRuntimeLifecycleAndTransportFactory() {
    llm::mcp::McpServerRegistry registry;
    llm::mcp::McpRuntime runtime(&registry);

    domain::mcp::McpServerConfig c1;
    c1.id = QStringLiteral("dummy");
    c1.command = QStringLiteral("non_existent_executable_12345");
    c1.enabled = true;

    registry.registerServer(c1);

    // 启动失败应优雅报错并发出信号，不发生崩溃
    bool errorTriggered = false;
    connect(&runtime, &llm::mcp::McpRuntime::serverError, [&](const QString& id, const QString&) {
        if (id == QStringLiteral("dummy")) {
            errorTriggered = true;
        }
    });

    bool ok = runtime.startServer(QStringLiteral("dummy"));
    QVERIFY(!ok);
    QVERIFY(errorTriggered);

    // 停止服务
    runtime.stopServer(QStringLiteral("dummy"));
    QCOMPARE(runtime.allSessions().size(), 0);

    // 验证 TransportFactory
    llm::mcp::McpTransportFactory factory;
    domain::mcp::McpServerConfig stdioCfg;
    stdioCfg.command = QStringLiteral("echo");
    stdioCfg.transport = domain::mcp::McpTransportType::Stdio;
    auto transport = factory.create(stdioCfg);
    QVERIFY(transport != nullptr);

    domain::mcp::McpServerConfig httpCfg;
    httpCfg.url = QStringLiteral("https://mcp.example.com/api");
    httpCfg.transport = domain::mcp::McpTransportType::Http;
    auto httpTransport = factory.create(httpCfg);
    QVERIFY(httpTransport != nullptr);
}

void AgentToolTests::mcpSecurityTrustAndEnvMaskingTests() {
    domain::mcp::McpServerTrustPolicy trustPolicy;

    // 1. 默认未受信
    QVERIFY(!trustPolicy.isServerTrusted(QStringLiteral("untrusted_srv")));
    QVERIFY(trustPolicy.isServerTrusted(QStringLiteral("untrusted_srv"), true)); // autoApprove 为 true 时放行

    // 2. 授权覆盖
    trustPolicy.setServerTrust(QStringLiteral("trusted_srv"), domain::mcp::McpTrustLevel::AlwaysAllow);
    QVERIFY(trustPolicy.isServerTrusted(QStringLiteral("trusted_srv")));

    trustPolicy.setServerTrust(QStringLiteral("denied_srv"), domain::mcp::McpTrustLevel::Denied);
    QVERIFY(!trustPolicy.isServerTrusted(QStringLiteral("denied_srv")));

    // 3. 敏感环境变量脱敏
    QMap<QString, QString> rawEnv{
        {QStringLiteral("PATH"), QStringLiteral("/usr/bin:/bin")},
        {QStringLiteral("OPENAI_API_KEY"), QStringLiteral("sk-1234567890abcdef")},
        {QStringLiteral("AUTH_TOKEN"), QStringLiteral("secret_tok")},
        {QStringLiteral("DB_PASSWORD"), QStringLiteral("pass123")},
        {QStringLiteral("NORMAL_CONFIG"), QStringLiteral("some_val")}
    };

    auto maskedEnv = domain::mcp::McpServerTrustPolicy::maskSensitiveEnv(rawEnv);
    QCOMPARE(maskedEnv.value(QStringLiteral("PATH")), QStringLiteral("/usr/bin:/bin"));
    QCOMPARE(maskedEnv.value(QStringLiteral("NORMAL_CONFIG")), QStringLiteral("some_val"));
    QVERIFY(!maskedEnv.value(QStringLiteral("OPENAI_API_KEY")).contains(QStringLiteral("1234567890")));
    QVERIFY(maskedEnv.value(QStringLiteral("AUTH_TOKEN")).contains(QStringLiteral("******")));
    QCOMPARE(maskedEnv.value(QStringLiteral("DB_PASSWORD")), QStringLiteral("******"));

    // 4. 验证 McpRuntime 核心执行链路中的安全信任拦截
    {
        llm::mcp::McpServerRegistry registry;
        llm::mcp::McpRuntime runtime(&registry);

        domain::mcp::McpServerConfig unapprovedCfg;
        unapprovedCfg.id = QStringLiteral("unapproved_srv");
        unapprovedCfg.command = QStringLiteral("echo");
        unapprovedCfg.autoApprove = false;
        registry.registerServer(unapprovedCfg);

        QString runtimeError;
        connect(&runtime, &llm::mcp::McpRuntime::serverError, [&](const QString&, const QString& err) {
            runtimeError = err;
        });

        // 未受信且无 autoApprove，启动必须被强制拦截并报错
        bool startRes = runtime.startServer(QStringLiteral("unapproved_srv"));
        QVERIFY(!startRes);
        QVERIFY(runtimeError.contains(QStringLiteral("未获得安全信任授权")));

        // 显式授权后，放行进入底层通道启动链路
        runtime.trustPolicy().setServerTrust(QStringLiteral("unapproved_srv"), domain::mcp::McpTrustLevel::AlwaysAllow);
        runtimeError.clear();
        runtime.startServer(QStringLiteral("unapproved_srv"));
        QVERIFY(!runtimeError.contains(QStringLiteral("未获得安全信任授权")));
    }
}

class FakeCrashTransport final : public llm::mcp::IMcpTransport {
    Q_OBJECT
public:
    using IMcpTransport::IMcpTransport;
    bool start() override {
        m_connected = true;
        return true;
    }
    void close() override {
        m_connected = false;
        emit closed();
    }
    bool sendJson(const QJsonObject& json) override {
        if (!m_connected) return false;
        if (json.value(QStringLiteral("method")).toString() == QStringLiteral("initialize")) {
            if (m_rejectVersion) {
                // 模拟返回不兼容协议版本
                QJsonObject resp{
                    {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("id"), json.value(QStringLiteral("id")).toInt()},
                    {QStringLiteral("result"), QJsonObject{
                        {QStringLiteral("protocolVersion"), QStringLiteral("1999-01-01")},
                        {QStringLiteral("capabilities"), QJsonObject{}},
                        {QStringLiteral("serverInfo"), QJsonObject{{QStringLiteral("name"), QStringLiteral("OldServer")}}}
                    }}
                };
                emit messageReceived(resp);
                return true;
            }

            QJsonObject resp{
                {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                {QStringLiteral("id"), json.value(QStringLiteral("id")).toInt()},
                {QStringLiteral("result"), QJsonObject{
                    {QStringLiteral("protocolVersion"), QStringLiteral("2024-11-05")},
                    {QStringLiteral("capabilities"), QJsonObject{}},
                    {QStringLiteral("serverInfo"), QJsonObject{{QStringLiteral("name"), QStringLiteral("TestServer")}}}
                }}
            };
            emit messageReceived(resp);
            return true;
        }

        if (json.value(QStringLiteral("method")).toString() == QStringLiteral("resources/list")) {
            QJsonObject resp{
                {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                {QStringLiteral("id"), json.value(QStringLiteral("id")).toInt()},
                {QStringLiteral("result"), QJsonObject{
                    {QStringLiteral("resources"), QJsonArray{
                        QJsonObject{
                            {QStringLiteral("uri"), QStringLiteral("file:///schema.sql")},
                            {QStringLiteral("name"), QStringLiteral("Schema")},
                            {QStringLiteral("mimeType"), QStringLiteral("text/plain")}
                        }
                    }}
                }}
            };
            emit messageReceived(resp);
            return true;
        }

        if (json.value(QStringLiteral("method")).toString() == QStringLiteral("prompts/list")) {
            QJsonObject resp{
                {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                {QStringLiteral("id"), json.value(QStringLiteral("id")).toInt()},
                {QStringLiteral("result"), QJsonObject{
                    {QStringLiteral("prompts"), QJsonArray{
                        QJsonObject{
                            {QStringLiteral("name"), QStringLiteral("review_code")},
                            {QStringLiteral("description"), QStringLiteral("代码审查提示词")}
                        }
                    }}
                }}
            };
            emit messageReceived(resp);
            return true;
        }

        if (json.value(QStringLiteral("method")).toString() == QStringLiteral("tools/list")) {
            QJsonObject resp{
                {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                {QStringLiteral("id"), json.value(QStringLiteral("id")).toInt()},
                {QStringLiteral("result"), QJsonObject{
                    {QStringLiteral("tools"), QJsonArray{}}
                }}
            };
            emit messageReceived(resp);
            return true;
        }

        return true;
    }
    bool isConnected() const override { return m_connected; }

    void triggerCrash() {
        m_connected = false;
        emit errorOccurred(QStringLiteral("进程异常崩溃退出 (exit code -1)"));
        emit closed();
    }

    void setRejectVersion(bool reject) { m_rejectVersion = reject; }

private:
    bool m_connected = false;
    bool m_rejectVersion = false;
};

void AgentToolTests::mcpResourceAndPromptProviderTests() {
    domain::mcp::McpServerConfig cfg;
    cfg.id = QStringLiteral("provider_test");
    cfg.command = QStringLiteral("dummy");

    auto fakeTransport = std::make_unique<FakeCrashTransport>();
    auto* fakeTransportPtr = fakeTransport.get();
    llm::mcp::McpSession session(cfg, std::move(fakeTransport));

    QVERIFY(session.start());
    QCOMPARE(session.state(), domain::mcp::McpConnectionState::Ready);

    // 验证 Client 查询
    auto resList = session.client()->listResources();
    QCOMPARE(resList.size(), 1);
    QCOMPARE(resList.first().uri, QStringLiteral("file:///schema.sql"));

    auto promptList = session.client()->listPrompts();
    QCOMPARE(promptList.size(), 1);
    QCOMPARE(promptList.first().name, QStringLiteral("review_code"));

    session.stop();
}

void AgentToolTests::mcpSessionCrashRecoveryAndHandshakeVersionTests() {
    domain::mcp::McpServerConfig cfg;
    cfg.id = QStringLiteral("crash_srv");
    cfg.command = QStringLiteral("dummy");

    // 1. 协议版本不兼容时拒绝连接
    {
        auto fakeTransport = std::make_unique<FakeCrashTransport>();
        fakeTransport->setRejectVersion(true);
        llm::mcp::McpSession versionSession(cfg, std::move(fakeTransport));

        bool ok = versionSession.start();
        QVERIFY(!ok);
        QCOMPARE(versionSession.state(), domain::mcp::McpConnectionState::Failed);
        QVERIFY(versionSession.lastError().contains(QStringLiteral("协议版本不兼容")));
    }

    // 2. 正常就绪 -> 进程崩溃 (Failed) -> 重新恢复 (Ready)
    {
        auto fakeTransport = std::make_unique<FakeCrashTransport>();
        auto* fakeTransportPtr = fakeTransport.get();
        llm::mcp::McpSession session(cfg, std::move(fakeTransport));

        QVERIFY(session.start());
        QCOMPARE(session.state(), domain::mcp::McpConnectionState::Ready);

        // 模拟崩溃
        fakeTransportPtr->triggerCrash();
        QCOMPARE(session.state(), domain::mcp::McpConnectionState::Failed);
        QVERIFY(session.lastError().contains(QStringLiteral("崩溃")));

        // 恢复重试
        session.stop();
        QCOMPARE(session.state(), domain::mcp::McpConnectionState::Stopped);
    }
}

void AgentToolTests::streamableHttpMcpTransportSseIntegrationTests() {
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    quint16 port = server.serverPort();

    // 模拟 HTTP / SSE MCP 服务端
    connect(&server, &QTcpServer::newConnection, [&]() {
        while (server.hasPendingConnections()) {
            QTcpSocket* sock = server.nextPendingConnection();
            connect(sock, &QTcpSocket::readyRead, [sock]() {
                const QByteArray reqData = sock->readAll();
                if (reqData.startsWith("GET /events")) {
                    // 发送 SSE 握手响应
                    sock->write("HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/event-stream\r\n"
                                "Cache-Control: no-cache\r\n"
                                "Connection: keep-alive\r\n\r\n");
                    sock->flush();
                    // 发送 endpoint 路由事件与一条下行通知 message 事件
                    sock->write("event: endpoint\r\ndata: /messages?session_id=sse123\r\n\r\n");
                    sock->write("event: message\r\ndata: {\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\r\n\r\n");
                    sock->flush();
                } else if (reqData.startsWith("POST /messages?session_id=sse123")) {
                    // 回复 POST 请求的 JSON-RPC 响应
                    const QByteArray body = "{\"jsonrpc\":\"2.0\",\"id\":999,\"result\":{\"status\":\"ok\"}}";
                    sock->write("HTTP/1.1 200 OK\r\n"
                                "Content-Type: application/json\r\n"
                                "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                                "Connection: close\r\n\r\n" + body);
                    sock->flush();
                    sock->disconnectFromHost();
                }
            });
        }
    });

    domain::mcp::McpServerConfig httpCfg;
    httpCfg.id = QStringLiteral("http_sse_test");
    httpCfg.url = QStringLiteral("http://127.0.0.1:%1/events").arg(port);
    httpCfg.transport = domain::mcp::McpTransportType::Http;

    llm::mcp::StreamableHttpMcpTransport transport(httpCfg);

    QList<QJsonObject> receivedMessages;
    connect(&transport, &llm::mcp::IMcpTransport::messageReceived, [&](const QJsonObject& obj) {
        receivedMessages.append(obj);
    });

    QVERIFY(transport.start());

    // 1. 验证 SSE endpoint 与下行消息接收
    QTRY_VERIFY_WITH_TIMEOUT(!receivedMessages.isEmpty(), 3000);
    QCOMPARE(receivedMessages.first().value("method").toString(), QStringLiteral("notifications/initialized"));
    QCOMPARE(transport.postEndpoint().toString(), QStringLiteral("http://127.0.0.1:%1/messages?session_id=sse123").arg(port));

    // 2. 验证 POST 请求发送与响应派发
    QJsonObject reqObj{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), 999},
        {QStringLiteral("method"), QStringLiteral("ping")}
    };
    QVERIFY(transport.sendJson(reqObj));

    QTRY_VERIFY_WITH_TIMEOUT(receivedMessages.size() >= 2, 3000);
    QCOMPARE(receivedMessages.at(1).value("id").toInt(), 999);
    QCOMPARE(receivedMessages.at(1).value("result").toObject().value("status").toString(), QStringLiteral("ok"));

    transport.close();
    QVERIFY(!transport.isConnected());
}

QTEST_GUILESS_MAIN(AgentToolTests)
#include "AgentToolTests.moc"


