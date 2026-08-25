#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

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

QTEST_GUILESS_MAIN(AgentToolTests)
#include "AgentToolTests.moc"


