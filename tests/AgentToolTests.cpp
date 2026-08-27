#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
// Recompiled against updated McpClient.h and AgentRunState.h
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
#include "llm/mcp/McpTool.h"
#include "agent/runtime/ToolExecutionScheduler.h"
#include "domain/mcp/McpServerTrust.h"
#include "core/logging/SensitiveDataFilter.h"
#include "core/logging/LogCategory.h"
#include "agent/task/ProcessOutputBuffer.h"
#include "agent/task/ProcessOutputDecoder.h"
#include "agent/task/ProcessTaskRuntime.h"
#include "agent/tool/builtin/CheckTaskTool.h"
#include "agent/tool/builtin/RunCommandTool.h"
#include "services/process/ShellService.h"
#include "services/process/ProcessLaunchResolver.h"
#include "services/process/ShellCommandRiskAnalyzer.h"

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
    void testSensitiveDataFilterAndArgKeys();
    void testToolExecutionErrorSanitization();
    void testToolExecutionSchedulerBatches();
    void testAsyncToolOperationLifecycle();
    void testFineGrainedPermissionEvaluation();
    void testAgentPolicyWildcardOverrides();
    void testListFilesTool();
    void testReadFileToolWithLineRangeAndBinaryRejection();
    void testSearchTextToolAdvanced();
    void testWriteFileToolOverwriteProtectionAndAtomicCommit();
    void testApplyPatchToolExactMatchingAndAtomicRollback();
    void testShellServiceAndLaunchResolver();
    void testShellCommandRiskAnalyzer();
    void testRunCommandToolExecutionAndSandboxing();
    void testRunCommandCompoundCommandsAndPipes();
    void testProcessOutputBufferCursorTracking();
    void testRunCommandBackgroundModeAndCheckTask();
    void testCheckTaskIncrementalCursorStreaming();
    void testCheckTaskWaitMsLongPolling();
    void testProcessTaskCancellationAndOwnership();
    void testProcessOutputDecoderUtf8MultiByteBoundary();
    void testProcessOutputDecoderGbkAndShiftJis();
    void testCheckTaskCancelDoesNotUAF();
    void testForegroundRunCancelDoesNotUAF();
    void testRunCommandFailedToStartReturnsFailed();
    void testProcessTaskRuntimeCleanupAndTTL();
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
    QCOMPARE(count, 7);

    QVERIFY(registry.hasTool("read_file"));
    QVERIFY(registry.hasTool("write_file"));
    QVERIFY(registry.hasTool("list_files"));
    QVERIFY(registry.hasTool("search_text"));
    QVERIFY(registry.hasTool("apply_patch"));
    QVERIFY(registry.hasTool("run_command"));
    QVERIFY(registry.hasTool("check_task"));

    auto defs = registry.definitions();
    QCOMPARE(defs.size(), 7);

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

static domain::agent::ToolResult runOpSync(std::unique_ptr<application::ports::IToolOperation> op) {
    if (!op) return {};
    domain::agent::ToolResult res;
    bool done = false;
    QEventLoop loop;
    QObject::connect(op.get(), &application::ports::IToolOperation::finished, [&](const domain::agent::ToolResult& r) {
        res = r;
        done = true;
        loop.quit();
    });
    op->start();
    if (!done) {
        loop.exec();
    }
    return res;
}

void AgentToolTests::toolRegistryUnknownToolError() {
    agent::tool::ToolRegistry registry;
    application::ports::ToolExecutionContext ctx{QUuid::createUuid(), QStringLiteral("s1"), QUuid::createUuid(), QStringLiteral("/tmp"), 30000, {}};
    domain::agent::ToolCall call{QStringLiteral("call_x"), QStringLiteral("non_existent_tool"), QStringLiteral("{}")};

    auto result = runOpSync(registry.execute(call, ctx));
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

    application::ports::ToolExecutionContext ctx{QUuid::createUuid(), QStringLiteral("session_1"), QUuid::createUuid(), root.path(), 30000, {}};

    // 1. write_file
    domain::agent::ToolCall writeCall{
        QStringLiteral("c1"),
        QStringLiteral("write_file"),
        QString::fromUtf8(R"({"path":"hello.txt","content":"Hello World\nLine 2"})")
    };
    auto writeResult = runOpSync(registry.execute(writeCall, ctx));
    QVERIFY(!writeResult.isError);

    // 2. read_file
    domain::agent::ToolCall readCall{
        QStringLiteral("c2"),
        QStringLiteral("read_file"),
        QStringLiteral(R"({"path":"hello.txt"})")
    };
    auto readResult = runOpSync(registry.execute(readCall, ctx));
    QVERIFY(!readResult.isError);
    auto readObj = QJsonDocument::fromJson(readResult.content.toUtf8()).object();
    QVERIFY(readObj.value("content").toString().contains(QStringLiteral("Hello World")));

    // 3. list_files
    domain::agent::ToolCall listCall{
        QStringLiteral("c3"),
        QStringLiteral("list_files"),
        QStringLiteral(R"({"path":"."})")
    };
    auto listResult = runOpSync(registry.execute(listCall, ctx));
    QVERIFY(!listResult.isError);
    auto listObj = QJsonDocument::fromJson(listResult.content.toUtf8()).object();
    auto entries = listObj.value("entries").toArray();
    bool foundHello = false;
    for (const auto& e : entries) {
        if (e.toObject().value("name").toString() == QStringLiteral("hello.txt")) {
            foundHello = true;
            break;
        }
    }
    QVERIFY(foundHello);

    // 4. search_text
    domain::agent::ToolCall searchCall{
        QStringLiteral("c4"),
        QStringLiteral("search_text"),
        QStringLiteral(R"({"query":"World"})")
    };
    auto searchResult = runOpSync(registry.execute(searchCall, ctx));
    QVERIFY(!searchResult.isError);
    auto searchObj = QJsonDocument::fromJson(searchResult.content.toUtf8()).object();
    auto searchMatches = searchObj.value("matches").toArray();
    QCOMPARE(searchMatches.size(), 1);
    QCOMPARE(searchMatches.at(0).toObject().value("path").toString(), QStringLiteral("hello.txt"));
    QCOMPARE(searchMatches.at(0).toObject().value("line").toInt(), 1);
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

    QObject::connect(&registry, &llm::mcp::McpServerRegistry::serverRegistered, [&](const domain::mcp::McpServerConfig&) {
        registeredCount++;
    });
    QObject::connect(&registry, &llm::mcp::McpServerRegistry::serverUnregistered, [&](const QString&) {
        unregisteredCount++;
    });
    QObject::connect(&registry, &llm::mcp::McpServerRegistry::registryChanged, [&]() {
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
    QObject::connect(&runtime, &llm::mcp::McpRuntime::serverError, [&](const QString& id, const QString&) {
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
        QObject::connect(&runtime, &llm::mcp::McpRuntime::serverError, [&](const QString&, const QString& err) {
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
    QObject::connect(&server, &QTcpServer::newConnection, [&]() {
        while (server.hasPendingConnections()) {
            QTcpSocket* sock = server.nextPendingConnection();
            QObject::connect(sock, &QTcpSocket::readyRead, [sock]() {
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
    QObject::connect(&transport, &llm::mcp::IMcpTransport::messageReceived, [&](const QJsonObject& obj) {
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

void AgentToolTests::testSensitiveDataFilterAndArgKeys() {
    // 1. 验证从参数 JSON 字符串中仅提取键名列表
    const QString argsJson = QStringLiteral(R"({"path": "src/main.cpp", "content": "secret password 123", "dryRun": true})");
    const QString extractedKeys = core::logging::SensitiveDataFilter::extractArgKeys(argsJson);
    QVERIFY(extractedKeys.contains(QStringLiteral("path")));
    QVERIFY(extractedKeys.contains(QStringLiteral("content")));
    QVERIFY(extractedKeys.contains(QStringLiteral("dryRun")));
    QVERIFY(!extractedKeys.contains(QStringLiteral("secret password 123")));
    QVERIFY(!extractedKeys.contains(QStringLiteral("src/main.cpp")));

    // 2. 验证 URL 脱敏
    const QString rawUrl = QStringLiteral("https://api.example.com/mcp?apiKey=sk-1234567890abcdef&token=my_secret_token&user=test");
    const QString cleanUrl = core::logging::SensitiveDataFilter::sanitizeUrl(rawUrl);
    QVERIFY(!cleanUrl.contains(QStringLiteral("sk-1234567890abcdef")));
    QVERIFY(!cleanUrl.contains(QStringLiteral("my_secret_token")));
    QVERIFY(cleanUrl.contains(QStringLiteral("user=test")));

    // 3. 验证 Header 过滤
    QMap<QString, QString> headers{
        {QStringLiteral("Authorization"), QStringLiteral("Bearer secret-jwt-token")},
        {QStringLiteral("Content-Type"), QStringLiteral("application/json")},
        {QStringLiteral("X-Custom-Secret"), QStringLiteral("confidential-data")}
    };
    const auto safeHeaders = core::logging::SensitiveDataFilter::filterHeaders(headers);
    QCOMPARE(safeHeaders.value(QStringLiteral("Content-Type")), QStringLiteral("application/json"));
    QCOMPARE(safeHeaders.value(QStringLiteral("Authorization")), QStringLiteral("****"));
    QVERIFY(!safeHeaders.contains(QStringLiteral("X-Custom-Secret")));
}

void AgentToolTests::testToolExecutionErrorSanitization() {
    QTemporaryDir root;
    QVERIFY(root.isValid());

    auto fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    agent::tool::builtin::ReadFileTool readTool(fs);
    agent::tool::builtin::WriteFileTool writeTool(fs);
    agent::tool::builtin::SearchTextTool searchTool(fs);

    application::ports::ToolExecutionContext ctx{QUuid::createUuid(), QStringLiteral("session-1"), QUuid::createUuid(), root.path(), 30000, {}};

    // 1. 缺少参数：提供模型可识别的明确错误
    domain::agent::ToolCall callEmptyRead{QStringLiteral("c1"), QStringLiteral("read_file"), QStringLiteral("{}")};
    auto resEmpty = runOpSync(readTool.execute(callEmptyRead, ctx));
    QVERIFY(resEmpty.isError);
    QCOMPARE(resEmpty.content, QStringLiteral("缺少 path 参数"));

    // 2. 越界路径：返回友好的脱敏错误提示
    domain::agent::ToolCall callEscapeRead{QStringLiteral("c2"), QStringLiteral("read_file"), QString::fromUtf8(R"({"path":"../../etc/passwd"})")};
    auto resEscape = runOpSync(readTool.execute(callEscapeRead, ctx));
    QVERIFY(resEscape.isError);
    QVERIFY(resEscape.content.contains(QStringLiteral("出于安全原因，无法访问项目外的路径")));

    // 3. 不存在的文件读写
    domain::agent::ToolCall callNotFoundRead{QStringLiteral("c3"), QStringLiteral("read_file"), QStringLiteral(R"({"path":"not_exist.txt"})")};
    auto resNotFound = runOpSync(readTool.execute(callNotFoundRead, ctx));
    QVERIFY(resNotFound.isError);
    QVERIFY(resNotFound.content.contains(QStringLiteral("出于安全原因，无法访问项目外的路径或文件不存在。")));
}

void AgentToolTests::testToolExecutionSchedulerBatches() {
    auto fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    agent::tool::ToolRegistry registry;
    registry.registerProvider(std::make_shared<agent::tool::BuiltinToolProvider>(fs));

    llm::mcp::McpServerConfig srv1Cfg{QStringLiteral("github")};
    llm::mcp::McpSession srv1Session(srv1Cfg);
    domain::agent::ToolDefinition def1;
    def1.name = QStringLiteral("get_issue");
    auto mcpTool1 = std::make_shared<llm::mcp::McpTool>(&srv1Session, QStringLiteral("github"), def1);
    registry.registerTool(mcpTool1);

    domain::agent::ToolDefinition def2;
    def2.name = QStringLiteral("create_issue");
    auto mcpTool2 = std::make_shared<llm::mcp::McpTool>(&srv1Session, QStringLiteral("github"), def2);
    registry.registerTool(mcpTool2);

    llm::mcp::McpServerConfig srv2Cfg{QStringLiteral("weather")};
    llm::mcp::McpSession srv2Session(srv2Cfg);
    domain::agent::ToolDefinition def3;
    def3.name = QStringLiteral("query_weather");
    auto mcpTool3 = std::make_shared<llm::mcp::McpTool>(&srv2Session, QStringLiteral("weather"), def3);
    registry.registerTool(mcpTool3);

    QList<domain::agent::ToolCall> calls = {
        {QStringLiteral("c1"), QStringLiteral("read_file"), QStringLiteral("{}")},
        {QStringLiteral("c2"), QStringLiteral("mcp::github::get_issue"), QStringLiteral("{}")},
        {QStringLiteral("c3"), QStringLiteral("mcp::weather::query_weather"), QStringLiteral("{}")},
        {QStringLiteral("c4"), QStringLiteral("mcp::github::create_issue"), QStringLiteral("{}")},
        {QStringLiteral("c5"), QStringLiteral("write_file"), QStringLiteral("{}")}
    };

    auto batches = agent::runtime::ToolExecutionScheduler::scheduleBatches(calls, &registry, true);
    QCOMPARE(batches.size(), 3);

    // Batch 0: read_file, github::get_issue, weather::query_weather
    QCOMPARE(batches[0].size(), 3);
    QCOMPARE(batches[0][0].name, QStringLiteral("read_file"));
    QCOMPARE(batches[0][1].name, QStringLiteral("mcp::github::get_issue"));
    QCOMPARE(batches[0][2].name, QStringLiteral("mcp::weather::query_weather"));

    // Batch 1: github::create_issue
    QCOMPARE(batches[1].size(), 1);
    QCOMPARE(batches[1][0].name, QStringLiteral("mcp::github::create_issue"));

    // Batch 2: write_file
    QCOMPARE(batches[2].size(), 1);
    QCOMPARE(batches[2][0].name, QStringLiteral("write_file"));
}

void AgentToolTests::testAsyncToolOperationLifecycle() {
    auto op = std::make_unique<application::ports::ImmediateToolOperation>(
        QStringLiteral("test_op"),
        []() -> domain::agent::ToolResult {
            return {QStringLiteral("test_op"), QStringLiteral("done"), false};
        }
    );
    QCOMPARE(op->state(), application::ports::ToolOperationState::Created);

    op->cancel();
    QCOMPARE(op->state(), application::ports::ToolOperationState::Cancelled);
}

void AgentToolTests::testFineGrainedPermissionEvaluation() {
    domain::agent::AgentPolicy policy;
    policy.autoApproveReadOnly = true;
    policy.autoApproveWriteWorkspace = true;
    policy.autoApproveExecuteProcess = false;
    policy.autoApproveDestructive = false;

    // 细粒度只读/读网络
    QCOMPARE(policy.evaluatePermission(domain::agent::ToolPermissionType::FileSystemRead), domain::agent::PermissionDecision::Allow);
    QCOMPARE(policy.evaluatePermission(domain::agent::ToolPermissionType::NetworkRead), domain::agent::PermissionDecision::Allow);
    QCOMPARE(policy.evaluatePermission(domain::agent::ToolPermissionType::ExternalServiceRead), domain::agent::PermissionDecision::Allow);

    // 细粒度写入/命令执行
    QCOMPARE(policy.evaluatePermission(domain::agent::ToolPermissionType::FileSystemWrite), domain::agent::PermissionDecision::Allow);
    QCOMPARE(policy.evaluatePermission(domain::agent::ToolPermissionType::ProcessExecute), domain::agent::PermissionDecision::AskUser);
    QCOMPARE(policy.evaluatePermission(domain::agent::ToolPermissionType::DestructiveOperation), domain::agent::PermissionDecision::AskUser);
}

void AgentToolTests::testAgentPolicyWildcardOverrides() {
    domain::agent::AgentPolicy policy;
    policy.autoApproveWriteWorkspace = false; // 默认写操作要询问

    domain::agent::ToolPermission writePerm{domain::agent::ToolPermissionType::FileSystemWrite, QStringLiteral("写入")};

    // 1. 无规则时回退到 evaluatePermission (AskUser)
    QCOMPARE(policy.evaluateTool(QStringLiteral("write_file"), writePerm), domain::agent::PermissionDecision::AskUser);

    // 2. 精确工具名覆盖 -> Allow
    policy.toolRules.insert(QStringLiteral("write_file"), domain::agent::PermissionDecision::Allow);
    QCOMPARE(policy.evaluateTool(QStringLiteral("write_file"), writePerm), domain::agent::PermissionDecision::Allow);

    // 3. 服务级通配符 (如 "db_server.*" 以及 "mcp::filesystem::*")
    domain::agent::ToolPermission mcpPerm{domain::agent::ToolPermissionType::ExternalServiceWrite, QStringLiteral("MCP写")};
    policy.toolRules.insert(QStringLiteral("db_server.*"), domain::agent::PermissionDecision::Deny);
    QCOMPARE(policy.evaluateTool(QStringLiteral("db_server.insert_row"), mcpPerm), domain::agent::PermissionDecision::Deny);
    QCOMPARE(policy.evaluateTool(QStringLiteral("db_server.delete_table"), mcpPerm), domain::agent::PermissionDecision::Deny);

    // 测试 mcp::<server>::<tool> 深度分层通配符
    policy.toolRules.insert(QStringLiteral("mcp::filesystem::*"), domain::agent::PermissionDecision::Allow);
    policy.toolRules.insert(QStringLiteral("mcp::database::*"), domain::agent::PermissionDecision::Deny);
    QCOMPARE(policy.evaluateTool(QStringLiteral("mcp::filesystem::read_file"), mcpPerm), domain::agent::PermissionDecision::Allow);
    QCOMPARE(policy.evaluateTool(QStringLiteral("mcp::filesystem::write_file"), mcpPerm), domain::agent::PermissionDecision::Allow);
    QCOMPARE(policy.evaluateTool(QStringLiteral("mcp::database::query_sql"), mcpPerm), domain::agent::PermissionDecision::Deny);

    // 4. 精确优先于通配符: 单独放行 "db_server.insert_row"
    policy.toolRules.insert(QStringLiteral("db_server.insert_row"), domain::agent::PermissionDecision::Allow);
    QCOMPARE(policy.evaluateTool(QStringLiteral("db_server.insert_row"), mcpPerm), domain::agent::PermissionDecision::Allow);
    QCOMPARE(policy.evaluateTool(QStringLiteral("db_server.delete_table"), mcpPerm), domain::agent::PermissionDecision::Deny);

    // 5. 全局通配符 "*"
    domain::agent::ToolPermission anyPerm{domain::agent::ToolPermissionType::ReadOnly, QStringLiteral("只读")};
    policy.toolRules.clear();
    policy.toolRules.insert(QStringLiteral("*"), domain::agent::PermissionDecision::Deny);
    QCOMPARE(policy.evaluateTool(QStringLiteral("read_file"), anyPerm), domain::agent::PermissionDecision::Deny);
    QCOMPARE(policy.evaluateTool(QStringLiteral("mcp::filesystem::read_file"), anyPerm), domain::agent::PermissionDecision::Deny);
}

void AgentToolTests::testListFilesTool() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QDir dir(temp.path());
    QVERIFY(dir.mkdir(QStringLiteral("sub_dir")));
    QFile f1(dir.filePath(QStringLiteral("visible.txt")));
    QVERIFY(f1.open(QIODevice::WriteOnly));
    f1.write("visible");
    f1.close();

    QFile f2(dir.filePath(QStringLiteral(".hidden_file")));
    QVERIFY(f2.open(QIODevice::WriteOnly));
    f2.write("hidden");
    f2.close();

    agent::tool::ToolRegistry registry;
    registry.registerProvider(std::make_shared<agent::tool::BuiltinToolProvider>());
    application::ports::ToolExecutionContext ctx{QUuid::createUuid(), QStringLiteral("s1"), QUuid::createUuid(), temp.path(), 30000, {}};

    // 1. 默认不包含隐藏文件
    domain::agent::ToolCall call1{
        QStringLiteral("l1"),
        QStringLiteral("list_files"),
        QStringLiteral("{\"path\":\".\"}")
    };
    auto res1 = runOpSync(registry.execute(call1, ctx));
    QVERIFY(!res1.isError);
    auto obj1 = QJsonDocument::fromJson(res1.content.toUtf8()).object();
    auto entries1 = obj1.value(QStringLiteral("entries")).toArray();
    QCOMPARE(entries1.size(), 2); // sub_dir, visible.txt

    // 2. include_hidden = true
    domain::agent::ToolCall call2{
        QStringLiteral("l2"),
        QStringLiteral("list_files"),
        QStringLiteral("{\"path\":\".\",\"include_hidden\":true}")
    };
    auto res2 = runOpSync(registry.execute(call2, ctx));
    QVERIFY(!res2.isError);
    auto obj2 = QJsonDocument::fromJson(res2.content.toUtf8()).object();
    auto entries2 = obj2.value(QStringLiteral("entries")).toArray();
    QCOMPARE(entries2.size(), 3); // .hidden_file, sub_dir, visible.txt

    // 3. 越界路径拒绝
    domain::agent::ToolCall call3{
        QStringLiteral("l3"),
        QStringLiteral("list_files"),
        QStringLiteral("{\"path\":\"../escape\"}")
    };
    auto res3 = runOpSync(registry.execute(call3, ctx));
    QVERIFY(res3.isError);
    QCOMPARE(res3.errorCode, QStringLiteral("PathValidationFailed"));
}

void AgentToolTests::testReadFileToolWithLineRangeAndBinaryRejection() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QDir dir(temp.path());

    // 文本文件 10 行
    QFile textFile(dir.filePath(QStringLiteral("sample.txt")));
    QVERIFY(textFile.open(QIODevice::WriteOnly));
    for (int i = 1; i <= 10; ++i) {
        textFile.write(QStringLiteral("Line %1 content\n").arg(i).toUtf8());
    }
    textFile.close();

    // 二进制文件 (含 \0)
    QFile binFile(dir.filePath(QStringLiteral("data.bin")));
    QVERIFY(binFile.open(QIODevice::WriteOnly));
    const char binData[] = {'E', 'L', 'F', '\0', 0x01, 0x02};
    binFile.write(binData, sizeof(binData));
    binFile.close();

    agent::tool::ToolRegistry registry;
    registry.registerProvider(std::make_shared<agent::tool::BuiltinToolProvider>());
    application::ports::ToolExecutionContext ctx{QUuid::createUuid(), QStringLiteral("s1"), QUuid::createUuid(), temp.path(), 30000, {}};

    // 1. 指定行号范围读取 (3 ~ 6)
    domain::agent::ToolCall callRange{
        QStringLiteral("r1"),
        QStringLiteral("read_file"),
        QStringLiteral("{\"path\":\"sample.txt\",\"start_line\":3,\"end_line\":6}")
    };
    auto resRange = runOpSync(registry.execute(callRange, ctx));
    QVERIFY(!resRange.isError);
    auto objRange = QJsonDocument::fromJson(resRange.content.toUtf8()).object();
    QCOMPARE(objRange.value(QStringLiteral("start_line")).toInt(), 3);
    QCOMPARE(objRange.value(QStringLiteral("end_line")).toInt(), 6);
    QCOMPARE(objRange.value(QStringLiteral("total_lines")).toInt(), 10);
    const QString content = objRange.value(QStringLiteral("content")).toString();
    QVERIFY(content.contains(QStringLiteral("Line 3 content")));
    QVERIFY(content.contains(QStringLiteral("Line 6 content")));
    QVERIFY(!content.contains(QStringLiteral("Line 2 content")));
    QVERIFY(!content.contains(QStringLiteral("Line 7 content")));

    // 2. 二进制文件直接拒绝
    domain::agent::ToolCall callBin{
        QStringLiteral("r2"),
        QStringLiteral("read_file"),
        QStringLiteral("{\"path\":\"data.bin\"}")
    };
    auto resBin = runOpSync(registry.execute(callBin, ctx));
    QVERIFY(resBin.isError);
    QCOMPARE(resBin.errorCode, QStringLiteral("UnsupportedBinaryFile"));

    // 3. 越界逃逸防护
    domain::agent::ToolCall callEscape{
        QStringLiteral("r3"),
        QStringLiteral("read_file"),
        QStringLiteral("{\"path\":\"../../outside.txt\"}")
    };
    auto resEscape = runOpSync(registry.execute(callEscape, ctx));
    QVERIFY(resEscape.isError);
    QCOMPARE(resEscape.errorCode, QStringLiteral("PathValidationFailed"));
}

void AgentToolTests::testSearchTextToolAdvanced() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QDir dir(temp.path());

    QFile cppFile(dir.filePath(QStringLiteral("main.cpp")));
    QVERIFY(cppFile.open(QIODevice::WriteOnly));
    cppFile.write("int main() {\n    int count = 42;\n    return count;\n}");
    cppFile.close();

    QFile hFile(dir.filePath(QStringLiteral("header.h")));
    QVERIFY(hFile.open(QIODevice::WriteOnly));
    hFile.write("class Header {\n    int count = 10;\n};");
    hFile.close();

    agent::tool::ToolRegistry registry;
    registry.registerProvider(std::make_shared<agent::tool::BuiltinToolProvider>());
    application::ports::ToolExecutionContext ctx{QUuid::createUuid(), QStringLiteral("s1"), QUuid::createUuid(), temp.path(), 30000, {}};

    // 1. 正则搜索
    domain::agent::ToolCall callRegex{
        QStringLiteral("s1"),
        QStringLiteral("search_text"),
        QStringLiteral("{\"query\":\"int\\\\s+count\\\\s*=\\\\s*\\\\d+\",\"regex\":true}")
    };
    auto resRegex = runOpSync(registry.execute(callRegex, ctx));
    QVERIFY(!resRegex.isError);
    auto objRegex = QJsonDocument::fromJson(resRegex.content.toUtf8()).object();
    auto matchesRegex = objRegex.value(QStringLiteral("matches")).toArray();
    QCOMPARE(matchesRegex.size(), 2);

    // 2. 文件通配过滤 (*.h)
    domain::agent::ToolCall callPattern{
        QStringLiteral("s2"),
        QStringLiteral("search_text"),
        QStringLiteral("{\"query\":\"count\",\"file_pattern\":\"*.h\"}")
    };
    auto resPattern = runOpSync(registry.execute(callPattern, ctx));
    QVERIFY(!resPattern.isError);
    auto objPattern = QJsonDocument::fromJson(resPattern.content.toUtf8()).object();
    auto matchesPattern = objPattern.value(QStringLiteral("matches")).toArray();
    QCOMPARE(matchesPattern.size(), 1);
    QCOMPARE(matchesPattern.at(0).toObject().value(QStringLiteral("path")).toString(), QStringLiteral("header.h"));
    QCOMPARE(matchesPattern.at(0).toObject().value(QStringLiteral("line")).toInt(), 2);
}

void AgentToolTests::testWriteFileToolOverwriteProtectionAndAtomicCommit() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    agent::tool::ToolRegistry registry;
    registry.registerProvider(std::make_shared<agent::tool::BuiltinToolProvider>());
    application::ports::ToolExecutionContext ctx{QUuid::createUuid(), QStringLiteral("s1"), QUuid::createUuid(), temp.path(), 30000, {}};

    // 1. 创建新文件
    domain::agent::ToolCall callCreate{
        QStringLiteral("w1"),
        QStringLiteral("write_file"),
        QStringLiteral("{\"path\":\"created.txt\",\"content\":\"Initial Content\"}")
    };
    auto resCreate = runOpSync(registry.execute(callCreate, ctx));
    QVERIFY(!resCreate.isError);
    auto objCreate = QJsonDocument::fromJson(resCreate.content.toUtf8()).object();
    QCOMPARE(objCreate.value(QStringLiteral("created")).toBool(), true);

    // 2. 尝试无 overwrite 覆盖 -> 拒绝
    domain::agent::ToolCall callNoOverwrite{
        QStringLiteral("w2"),
        QStringLiteral("write_file"),
        QStringLiteral("{\"path\":\"created.txt\",\"content\":\"New Overwrite Content\"}")
    };
    auto resNoOverwrite = runOpSync(registry.execute(callNoOverwrite, ctx));
    QVERIFY(resNoOverwrite.isError);
    QCOMPARE(resNoOverwrite.errorCode, QStringLiteral("FileAlreadyExists"));

    // 3. 显式 overwrite = true 覆盖 -> 成功
    domain::agent::ToolCall callOverwrite{
        QStringLiteral("w3"),
        QStringLiteral("write_file"),
        QStringLiteral("{\"path\":\"created.txt\",\"content\":\"New Overwrite Content\",\"overwrite\":true}")
    };
    auto resOverwrite = runOpSync(registry.execute(callOverwrite, ctx));
    QVERIFY(!resOverwrite.isError);
    auto objOverwrite = QJsonDocument::fromJson(resOverwrite.content.toUtf8()).object();
    QCOMPARE(objOverwrite.value(QStringLiteral("created")).toBool(), false);

    // 验证文件真实内容
    QFile f(QDir(temp.path()).filePath(QStringLiteral("created.txt")));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), QByteArray("New Overwrite Content"));
}

void AgentToolTests::testApplyPatchToolExactMatchingAndAtomicRollback() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QDir dir(temp.path());

    QFile file(dir.filePath(QStringLiteral("code.cpp")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("void foo() {\n    int a = 1;\n    int b = 2;\n    int duplicate = 99;\n    int duplicate = 99;\n}\n");
    file.close();

    agent::tool::ToolRegistry registry;
    registry.registerProvider(std::make_shared<agent::tool::BuiltinToolProvider>());
    application::ports::ToolExecutionContext ctx{QUuid::createUuid(), QStringLiteral("s1"), QUuid::createUuid(), temp.path(), 30000, {}};

    // 1. 找不到 old_text -> PatchContextNotFound
    domain::agent::ToolCall callNotFound{
        QStringLiteral("p1"),
        QStringLiteral("apply_patch"),
        QStringLiteral("{\"path\":\"code.cpp\",\"patches\":[{\"old_text\":\"void not_exist()\",\"new_text\":\"void exist()\"}]}")
    };
    auto resNotFound = runOpSync(registry.execute(callNotFound, ctx));
    QVERIFY(resNotFound.isError);
    QCOMPARE(resNotFound.errorCode, QStringLiteral("PatchContextNotFound"));

    // 2. 出现多次 -> PatchContextAmbiguous
    domain::agent::ToolCall callAmbiguous{
        QStringLiteral("p2"),
        QStringLiteral("apply_patch"),
        QStringLiteral("{\"path\":\"code.cpp\",\"patches\":[{\"old_text\":\"    int duplicate = 99;\",\"new_text\":\"    int duplicate = 100;\"}]}")
    };
    auto resAmbiguous = runOpSync(registry.execute(callAmbiguous, ctx));
    QVERIFY(resAmbiguous.isError);
    QCOMPARE(resAmbiguous.errorCode, QStringLiteral("PatchContextAmbiguous"));

    // 3. 多 Patch 事务性回滚：Patch 1 有效，但 Patch 2 找不到 -> 文件不应被任何一个 Patch 修改
    domain::agent::ToolCall callRollback{
        QStringLiteral("p3"),
        QStringLiteral("apply_patch"),
        QStringLiteral("{\"path\":\"code.cpp\",\"patches\":[{\"old_text\":\"    int a = 1;\",\"new_text\":\"    int a = 100;\"},{\"old_text\":\"non_existent_code_line\",\"new_text\":\"something\"}]}")
    };
    auto resRollback = runOpSync(registry.execute(callRollback, ctx));
    QVERIFY(resRollback.isError);
    QCOMPARE(resRollback.errorCode, QStringLiteral("PatchContextNotFound"));

    // 验证文件原内容未受任何影响
    QFile f1(dir.filePath(QStringLiteral("code.cpp")));
    QVERIFY(f1.open(QIODevice::ReadOnly));
    QVERIFY(f1.readAll().contains("int a = 1;"));
    f1.close();

    // 4. 两个 Patch 均唯一匹配 -> 原子应用成功
    domain::agent::ToolCall callSuccess{
        QStringLiteral("p4"),
        QStringLiteral("apply_patch"),
        QStringLiteral("{\"path\":\"code.cpp\",\"patches\":[{\"old_text\":\"    int a = 1;\",\"new_text\":\"    int a = 10;\"},{\"old_text\":\"    int b = 2;\",\"new_text\":\"    int b = 20;\"}]}")
    };
    auto resSuccess = runOpSync(registry.execute(callSuccess, ctx));
    QVERIFY(!resSuccess.isError);
    auto objSuccess = QJsonDocument::fromJson(resSuccess.content.toUtf8()).object();
    QCOMPARE(objSuccess.value(QStringLiteral("patch_count")).toInt(), 2);
    QCOMPARE(objSuccess.value(QStringLiteral("changed")).toBool(), true);

    QFile f2(dir.filePath(QStringLiteral("code.cpp")));
    QVERIFY(f2.open(QIODevice::ReadOnly));
    const auto finalContent = f2.readAll();
    QVERIFY(finalContent.contains("int a = 10;"));
    QVERIFY(finalContent.contains("int b = 20;"));
}

void AgentToolTests::testShellServiceAndLaunchResolver() {
    services::process::ShellService shellService;
    const auto shells = shellService.availableShells();
    QVERIFY(!shells.isEmpty());

    const auto defShell = shellService.defaultShell();
    QVERIFY(defShell.has_value());
    QVERIFY(!defShell->executable.isEmpty());
    QVERIFY(!defShell->id.isEmpty());

    // 测试 ProcessLaunchResolver
    services::process::ProcessLaunchResolver resolver;
    domain::agent::task::ProcessTaskSpec spec;
    spec.launchMode = domain::agent::task::ProcessLaunchMode::ShellCommand;
    spec.command = QStringLiteral("cmake --build build && ctest");

    auto launch = resolver.resolveShellCommand(spec, defShell.value());
    QCOMPARE(launch.executable, defShell->executable);
    QVERIFY(launch.arguments.contains(QStringLiteral("cmake --build build && ctest")));
}

void AgentToolTests::testShellCommandRiskAnalyzer() {
    services::process::ShellCommandRiskAnalyzer analyzer;

    // 安全命令
    auto safe1 = analyzer.analyze(QStringLiteral("echo hello"));
    QVERIFY(!safe1.destructive);

    auto safe2 = analyzer.analyze(QStringLiteral("git status && git log -n 5"));
    QVERIFY(!safe2.destructive);

    // 危险命令
    auto riskRm = analyzer.analyze(QStringLiteral("rm -rf /tmp/test"));
    QVERIFY(riskRm.destructive);

    auto riskGit = analyzer.analyze(QStringLiteral("git reset --hard HEAD~1"));
    QVERIFY(riskGit.destructive);

    auto riskPs = analyzer.analyze(QStringLiteral("Remove-Item -Recurse -Force ./build"));
    QVERIFY(riskPs.destructive);

    auto riskReboot = analyzer.analyze(QStringLiteral("shutdown /r /t 0"));
    QVERIFY(riskReboot.destructive);
}

void AgentToolTests::testRunCommandToolExecutionAndSandboxing() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    agent::tool::ToolRegistry registry;
    registry.registerProvider(std::make_shared<agent::tool::BuiltinToolProvider>());
    application::ports::ToolExecutionContext ctx{QUuid::createUuid(), QStringLiteral("s1"), QUuid::createUuid(), temp.path(), 30000, {}};

    auto runTool = registry.findTool(QStringLiteral("run_command"));
    QVERIFY(runTool != nullptr);

    // 1. 权限动态识别：高危命令组合返回 ProcessExecute + DestructiveOperation
    domain::agent::ToolCall callRm{
        QStringLiteral("rc1"),
        QStringLiteral("run_command"),
        QStringLiteral("{\"command\":\"rm -rf .\"}")
    };
    const auto permsRm = runTool->permissions(callRm);
    QCOMPARE(permsRm.size(), 2);
    QCOMPARE(permsRm.at(0).type, domain::agent::ToolPermissionType::ProcessExecute);
    QCOMPARE(permsRm.at(1).type, domain::agent::ToolPermissionType::DestructiveOperation);

    domain::agent::ToolCall callEcho{
        QStringLiteral("rc2"),
        QStringLiteral("run_command"),
        QStringLiteral("{\"command\":\"echo \\\"ForgeAI Shell Ready\\\"\"}")
    };
    const auto permsEcho = runTool->permissions(callEcho);
    QCOMPARE(permsEcho.size(), 1);
    QCOMPARE(permsEcho.first().type, domain::agent::ToolPermissionType::ProcessExecute);

    // 2. 工作目录越界逃逸防护
    domain::agent::ToolCall callEscape{
        QStringLiteral("rc3"),
        QStringLiteral("run_command"),
        QStringLiteral("{\"command\":\"echo test\",\"working_directory\":\"../../escape_dir\"}")
    };
    auto resEscape = runOpSync(registry.execute(callEscape, ctx));
    QVERIFY(resEscape.isError);
    QCOMPARE(resEscape.errorCode, QStringLiteral("WorkingDirectoryEscape"));

    // 3. 执行真实命令 (echo "ForgeAI Shell Ready")
    auto resEcho = runOpSync(registry.execute(callEcho, ctx));
    QVERIFY(!resEcho.isError);
    auto objEcho = QJsonDocument::fromJson(resEcho.content.toUtf8()).object();
    QCOMPARE(objEcho.value(QStringLiteral("exit_code")).toInt(), 0);
    QVERIFY(objEcho.value(QStringLiteral("stdout")).toString().contains(QStringLiteral("ForgeAI Shell Ready")));
}

void AgentToolTests::testRunCommandCompoundCommandsAndPipes() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    agent::tool::ToolRegistry registry;
    registry.registerProvider(std::make_shared<agent::tool::BuiltinToolProvider>());
    application::ports::ToolExecutionContext ctx{QUuid::createUuid(), QStringLiteral("s1"), QUuid::createUuid(), temp.path(), 30000, {}};

    // 复合命令：echo step1 && echo step2
    domain::agent::ToolCall callCompound{
        QStringLiteral("rc_compound"),
        QStringLiteral("run_command"),
        QStringLiteral("{\"command\":\"echo step1 && echo step2\"}")
    };
    auto resCompound = runOpSync(registry.execute(callCompound, ctx));
    QVERIFY(!resCompound.isError);
    auto objCompound = QJsonDocument::fromJson(resCompound.content.toUtf8()).object();
    QCOMPARE(objCompound.value(QStringLiteral("exit_code")).toInt(), 0);
    const QString out = objCompound.value(QStringLiteral("stdout")).toString();
    QVERIFY(out.contains(QStringLiteral("step1")));
    QVERIFY(out.contains(QStringLiteral("step2")));
}

void AgentToolTests::testProcessOutputBufferCursorTracking() {
    agent::task::ProcessOutputBuffer buffer(100); // 100 bytes capacity

    buffer.append("Hello ");
    QCOMPARE(buffer.totalProducedBytes(), 6ULL);
    QCOMPARE(buffer.availableHeadOffset(), 0ULL);
    QVERIFY(!buffer.hasTruncated());

    quint64 nextCursor = 0;
    bool lost = false;
    quint64 avail = 0;
    QByteArray chunk1 = buffer.readBytesFrom(0, 10, &lost, &avail, &nextCursor);
    QCOMPARE(chunk1, QByteArray("Hello "));
    QCOMPARE(nextCursor, 6ULL);
    QVERIFY(!lost);

    buffer.append("World!");
    QByteArray chunk2 = buffer.readBytesFrom(nextCursor, 10, &lost, &avail, &nextCursor);
    QCOMPARE(chunk2, QByteArray("World!"));
    QCOMPARE(nextCursor, 12ULL);

    // 测试缓冲区溢出与游标过期
    QByteArray bigData(150, 'A');
    buffer.append(bigData);
    QCOMPARE(buffer.totalProducedBytes(), 162ULL);
    QVERIFY(buffer.hasTruncated());
    QVERIFY(buffer.availableHeadOffset() > 0);

    // 读取过期的旧游标 0 应标记 lost
    QByteArray chunk3 = buffer.readBytesFrom(0, 50, &lost, &avail, &nextCursor);
    QVERIFY(lost);
    QCOMPARE(avail, buffer.availableHeadOffset());
}

void AgentToolTests::testRunCommandBackgroundModeAndCheckTask() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    auto taskRuntime = std::make_shared<agent::task::ProcessTaskRuntime>();
    auto fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    agent::tool::ToolRegistry registry;
    registry.registerProvider(std::make_shared<agent::tool::BuiltinToolProvider>(taskRuntime, nullptr, fs));

    application::ports::ToolExecutionContext ctx{QUuid::createUuid(), QStringLiteral("s1"), QUuid::createUuid(), temp.path(), 30000, {}};

    // 1. 启动后台任务
    domain::agent::ToolCall bgCall{
        QStringLiteral("bg1"),
        QStringLiteral("run_command"),
        QStringLiteral("{\"command\":\"echo \\\"bg_test_message_123\\\"\",\"background\":true}")
    };
    auto bgRes = runOpSync(registry.execute(bgCall, ctx));
    QVERIFY(!bgRes.isError);

    auto bgObj = QJsonDocument::fromJson(bgRes.content.toUtf8()).object();
    const QString taskId = bgObj.value(QStringLiteral("task_id")).toString();
    QVERIFY(!taskId.isEmpty());
    QCOMPARE(bgObj.value(QStringLiteral("status")).toString(), QStringLiteral("running"));

    // 2. 调用 check_task 轮询查询
    domain::agent::ToolCall checkCall{
        QStringLiteral("chk1"),
        QStringLiteral("check_task"),
        QStringLiteral("{\"task_id\":\"%1\",\"wait_ms\":2000}").arg(taskId)
    };
    auto checkRes = runOpSync(registry.execute(checkCall, ctx));
    QVERIFY(!checkRes.isError);

    auto checkObj = QJsonDocument::fromJson(checkRes.content.toUtf8()).object();
    QCOMPARE(checkObj.value(QStringLiteral("task_id")).toString(), taskId);
    QVERIFY(checkObj.value(QStringLiteral("stdout")).toString().contains(QStringLiteral("bg_test_message_123")));
}

void AgentToolTests::testCheckTaskIncrementalCursorStreaming() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    auto taskRuntime = std::make_shared<agent::task::ProcessTaskRuntime>();
    auto fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    agent::tool::ToolRegistry registry;
    registry.registerProvider(std::make_shared<agent::tool::BuiltinToolProvider>(taskRuntime, nullptr, fs));

    application::ports::ToolExecutionContext ctx{QUuid::createUuid(), QStringLiteral("s1"), QUuid::createUuid(), temp.path(), 30000, {}};

    domain::agent::ToolCall bgCall{
        QStringLiteral("bg2"),
        QStringLiteral("run_command"),
        QStringLiteral("{\"command\":\"echo \\\"1234567890abcdefghij\\\"\",\"background\":true}")
    };
    auto bgRes = runOpSync(registry.execute(bgCall, ctx));
    const QString taskId = QJsonDocument::fromJson(bgRes.content.toUtf8()).object().value(QStringLiteral("task_id")).toString();

    // 第一次读取前 10 个字节
    domain::agent::ToolCall check1{
        QStringLiteral("chk_inc1"),
        QStringLiteral("check_task"),
        QStringLiteral("{\"task_id\":\"%1\",\"stdout_cursor\":0,\"max_output_bytes\":10,\"wait_ms\":2000}").arg(taskId)
    };
    auto res1 = runOpSync(registry.execute(check1, ctx));
    QVERIFY(!res1.isError);
    auto obj1 = QJsonDocument::fromJson(res1.content.toUtf8()).object();
    const quint64 cursor1 = obj1.value(QStringLiteral("stdout_cursor")).toInteger();
    const QString text1 = obj1.value(QStringLiteral("stdout")).toString();
    QCOMPARE(text1.length(), 10);

    // 第二次基于 cursor1 读取后续增量
    domain::agent::ToolCall check2{
        QStringLiteral("chk_inc2"),
        QStringLiteral("check_task"),
        QStringLiteral("{\"task_id\":\"%1\",\"stdout_cursor\":%2,\"max_output_bytes\":5000}").arg(taskId).arg(cursor1)
    };
    auto res2 = runOpSync(registry.execute(check2, ctx));
    QVERIFY(!res2.isError);
    auto obj2 = QJsonDocument::fromJson(res2.content.toUtf8()).object();
    const QString text2 = obj2.value(QStringLiteral("stdout")).toString();

    // 增量文本不包含第一次读过的 text1 前缀
    QVERIFY(!text2.startsWith(text1));
}

void AgentToolTests::testCheckTaskWaitMsLongPolling() {
    auto taskRuntime = std::make_shared<agent::task::ProcessTaskRuntime>();
    agent::tool::builtin::CheckTaskTool checkTool(taskRuntime);

    application::ports::ToolExecutionContext ctx{QUuid::createUuid(), QStringLiteral("s1"), QUuid::createUuid(), QStringLiteral("."), 30000, {}};

    domain::agent::ToolCall callNonExistent{
        QStringLiteral("chk_wait1"),
        QStringLiteral("check_task"),
        QStringLiteral("{\"task_id\":\"task_non_existent\",\"wait_ms\":100}")
    };
    auto res = runOpSync(checkTool.execute(callNonExistent, ctx));
    QVERIFY(res.isError);
    QCOMPARE(res.errorCode, QStringLiteral("TaskNotFound"));
}

void AgentToolTests::testProcessTaskCancellationAndOwnership() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    auto taskRuntime = std::make_shared<agent::task::ProcessTaskRuntime>();
    auto fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    agent::tool::ToolRegistry registry;
    registry.registerProvider(std::make_shared<agent::tool::BuiltinToolProvider>(taskRuntime, nullptr, fs));

    const QUuid runA = QUuid::createUuid();
    const QUuid runB = QUuid::createUuid();

    application::ports::ToolExecutionContext ctxA{runA, QStringLiteral("s1"), QUuid::createUuid(), temp.path(), 30000, {}};
    application::ports::ToolExecutionContext ctxB{runB, QStringLiteral("s2"), QUuid::createUuid(), temp.path(), 30000, {}};

    domain::agent::ToolCall bgCall{
        QStringLiteral("bg_own"),
        QStringLiteral("run_command"),
        QStringLiteral("{\"command\":\"ping 127.0.0.1 -n 5 > nul || sleep 5\",\"background\":true}")
    };
    auto bgRes = runOpSync(registry.execute(bgCall, ctxA));
    const QString taskId = QJsonDocument::fromJson(bgRes.content.toUtf8()).object().value(QStringLiteral("task_id")).toString();

    // Run B 尝试越权读取 Run A 的任务日志 -> 返回 TaskNotOwnedByRun
    domain::agent::ToolCall checkB{
        QStringLiteral("chk_unauth"),
        QStringLiteral("check_task"),
        QStringLiteral("{\"task_id\":\"%1\"}").arg(taskId)
    };
    auto resB = runOpSync(registry.execute(checkB, ctxB));
    QVERIFY(resB.isError);
    QCOMPARE(resB.errorCode, QStringLiteral("TaskNotOwnedByRun"));

    // Run A 取消任务
    QVERIFY(taskRuntime->cancel(taskId, runA));
    auto snap = taskRuntime->snapshot(taskId);
    QVERIFY(snap.has_value());
    QCOMPARE(snap->state, domain::agent::task::ProcessTaskState::Cancelled);
}

void AgentToolTests::testProcessOutputDecoderUtf8MultiByteBoundary() {
    // 1. 测试 UTF-8 多字节中文 ("你好世界" 每个字 3 字节)
    const QString fullText = QStringLiteral("你好世界🌟🚀");
    const QByteArray utf8Bytes = fullText.toUtf8();

    // 假设在第 4 字节处切断（"你" 3字节，"好" 第1字节）
    const QByteArray chunk1 = utf8Bytes.left(4);
    auto res1 = agent::task::ProcessOutputDecoder::decodeChunk(chunk1, QStringLiteral("utf-8"), false);
    QCOMPARE(res1.bytesConsumed, 3); // 自动回退未完成的第4字节
    QCOMPARE(res1.text, QStringLiteral("你"));
    QVERIFY(!res1.hasError);

    // 随后从已消费的游标继续解码剩余字节
    const QByteArray chunk2 = utf8Bytes.mid(res1.bytesConsumed);
    auto res2 = agent::task::ProcessOutputDecoder::decodeChunk(chunk2, QStringLiteral("utf-8"), true);
    QCOMPARE(res2.text, QStringLiteral("好世界🌟🚀"));
    QVERIFY(!res2.hasError);
}

void AgentToolTests::testProcessOutputDecoderGbkAndShiftJis() {
    // 1. 规范化编码名称测试
    QCOMPARE(agent::task::ProcessOutputDecoder::normalizeEncoding(QStringLiteral("GBK")), QStringLiteral("gb18030"));
    QCOMPARE(agent::task::ProcessOutputDecoder::normalizeEncoding(QStringLiteral("sjis")), QStringLiteral("shift-jis"));
    QCOMPARE(agent::task::ProcessOutputDecoder::normalizeEncoding(QStringLiteral("system")), QStringLiteral("system"));

    // 2. ASCII 兼容测试
    const QByteArray ascii = "Hello, world!";
    auto res = agent::task::ProcessOutputDecoder::decodeChunk(ascii, QStringLiteral("gb18030"), true);
    QCOMPARE(res.text, QStringLiteral("Hello, world!"));
    QVERIFY(!res.hasError);
}

void AgentToolTests::testCheckTaskCancelDoesNotUAF() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    auto taskRuntime = std::make_shared<agent::task::ProcessTaskRuntime>();
    auto fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    agent::tool::builtin::CheckTaskTool checkTool(taskRuntime);

    application::ports::ToolExecutionContext ctx{QUuid::createUuid(), QStringLiteral("s1"), QUuid::createUuid(), temp.path(), 30000, {}};

    domain::agent::task::ProcessTaskSpec spec;
    spec.launchMode = domain::agent::task::ProcessLaunchMode::ShellCommand;
    spec.command = QStringLiteral("ping 127.0.0.1 -n 5 > nul || sleep 5");
    spec.workingDirectory = temp.path();
    spec.background = true;
    spec.runId = ctx.runId;

    const QString taskId = taskRuntime->start(spec, ctx);

    domain::agent::ToolCall checkCall{
        QStringLiteral("chk_uaf"),
        QStringLiteral("check_task"),
        QStringLiteral("{\"task_id\":\"%1\",\"wait_ms\":3000}").arg(taskId)
    };

    // 启动异步等待 Operation，随后立即取消并销毁 Operation 实例
    auto op = checkTool.execute(checkCall, ctx);
    QVERIFY(op != nullptr);
    op->start();
    op->cancel();
    op.reset(); // 彻底析构 Operation 对象

    // 模拟等待事件循环推进，确保定时器或异步回调触发时不会发生 UAF 崩溃
    QTest::qWait(100);
    taskRuntime->cancel(taskId, ctx.runId);
}

void AgentToolTests::testForegroundRunCancelDoesNotUAF() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    auto taskRuntime = std::make_shared<agent::task::ProcessTaskRuntime>();
    auto fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    agent::tool::builtin::RunCommandTool runTool(taskRuntime, nullptr, fs);

    application::ports::ToolExecutionContext ctx{QUuid::createUuid(), QStringLiteral("s1"), QUuid::createUuid(), temp.path(), 30000, {}};

    domain::agent::ToolCall runCall{
        QStringLiteral("run_uaf"),
        QStringLiteral("run_command"),
        QStringLiteral("{\"command\":\"ping 127.0.0.1 -n 5 > nul || sleep 5\",\"background\":false}")
    };

    auto op = runTool.execute(runCall, ctx);
    QVERIFY(op != nullptr);
    op->start();
    op->cancel();
    op.reset(); // 彻底析构

    QTest::qWait(100);
}

void AgentToolTests::testRunCommandFailedToStartReturnsFailed() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    auto taskRuntime = std::make_shared<agent::task::ProcessTaskRuntime>();
    auto fs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    agent::tool::builtin::RunCommandTool runTool(taskRuntime, nullptr, fs);

    application::ports::ToolExecutionContext ctx{QUuid::createUuid(), QStringLiteral("s1"), QUuid::createUuid(), temp.path(), 30000, {}};

    domain::agent::task::ProcessTaskSpec spec;
    spec.launchMode = domain::agent::task::ProcessLaunchMode::DirectProcess;
    spec.program = QStringLiteral("/__non_existent_forge_ai_binary__");
    spec.workingDirectory = temp.path();
    spec.background = true;
    spec.runId = ctx.runId;

    const QString taskId = taskRuntime->start(spec, ctx);
    QVERIFY(!taskId.isEmpty());

    // 等待子进程启动失败信号派发
    QTest::qWait(100);

    auto snap = taskRuntime->snapshot(taskId);
    QVERIFY(snap.has_value());
    QCOMPARE(snap->state, domain::agent::task::ProcessTaskState::Failed);
}

void AgentToolTests::testProcessTaskRuntimeCleanupAndTTL() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    auto taskRuntime = std::make_shared<agent::task::ProcessTaskRuntime>();
    application::ports::ToolExecutionContext ctx{QUuid::createUuid(), QStringLiteral("s1"), QUuid::createUuid(), temp.path(), 30000, {}};

    domain::agent::task::ProcessTaskSpec spec;
    spec.launchMode = domain::agent::task::ProcessLaunchMode::ShellCommand;
    spec.command = QStringLiteral("echo \"done_cleanup_test\"");
    spec.workingDirectory = temp.path();
    spec.background = true;
    spec.runId = ctx.runId;

    const QString taskId = taskRuntime->start(spec, ctx);

    // 等待执行完成
    for (int i = 0; i < 40; ++i) {
        auto s = taskRuntime->snapshot(taskId);
        if (s.has_value() && (s->state == domain::agent::task::ProcessTaskState::Completed || s->state == domain::agent::task::ProcessTaskState::Failed)) {
            break;
        }
        QTest::qWait(50);
    }

    auto snap = taskRuntime->snapshot(taskId);
    QVERIFY(snap.has_value());
    QCOMPARE(snap->state, domain::agent::task::ProcessTaskState::Completed);

    // 清理已结束任务 (TTL = 0ms)
    int cleaned = taskRuntime->cleanupFinishedTasks(0);
    QCOMPARE(cleaned, 1);

    auto snapAfter = taskRuntime->snapshot(taskId);
    QVERIFY(!snapAfter.has_value());
}

QTEST_GUILESS_MAIN(AgentToolTests)
#include "AgentToolTests.moc"



