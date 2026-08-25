#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QEventLoop>

#include "ui/screen/work/WorkViewModel.h"
#include "agent/runtime/AgentRuntime.h"
#include "agent/runtime/AgentContextBuilder.h"
#include "agent/tool/ToolRegistry.h"
#include "agent/tool/BuiltinToolProvider.h"
#include "llm/workspace/WorkspaceFileSystem.h"
#include "llm/mcp/McpTool.h"
#include "llm/mcp/McpToolProvider.h"
#include "data/repository/SqliteModelRepository.h"
#include "data/repository/SqliteAgentCheckpointRepository.h"
#include "data/repository/SqliteConversationRepository.h"
#include "data/repository/SqliteProjectRepository.h"
#include "data/repository/JsonlMessageRepository.h"
#include "services/conversation/ConversationService.h"
#include "services/project/ProjectContextService.h"
#include "services/model/ModelService.h"
#include "core/model/ModelRegistry.h"
#include "data/sqlite/DatabaseManager.h"
#include "application/ports/IChatModelGateway.h"
#include "application/usecase/agent/RunAgentUseCase.h"

// 模拟 ChatOperation
class MockChatOperation final : public application::ports::IChatOperation {
    Q_OBJECT
public:
    explicit MockChatOperation(QList<domain::llm::ChatEvent> events, QObject* parent = nullptr)
        : application::ports::IChatOperation(parent), m_events(std::move(events)) {
        QTimer::singleShot(10, this, [this]() {
            if (m_cancelled) return;
            for (const auto& ev : m_events) {
                if (m_cancelled) break;
                emit eventReceived(ev);
            }
        });
    }

    void cancel() override {
        m_cancelled = true;
    }

private:
    QList<domain::llm::ChatEvent> m_events;
    bool m_cancelled = false;
};

// 模拟 ChatModelGateway
class MockChatGateway final : public application::ports::IChatModelGateway {
public:
    void enqueueResponse(QList<domain::llm::ChatEvent> events) {
        m_responseQueue.append(std::move(events));
    }

    application::ports::IChatOperation* sendRequest(
        const domain::model::ModelProvider& provider,
        const domain::llm::ChatRequest& request
    ) override {
        Q_UNUSED(provider);
        Q_UNUSED(request);

        QList<domain::llm::ChatEvent> events;
        if (!m_responseQueue.isEmpty()) {
            events = m_responseQueue.takeFirst();
        } else {
            events.append(domain::llm::EventStarted{});
            events.append(domain::llm::EventTextDelta{QStringLiteral("默认模型回答")});
            events.append(domain::llm::EventFinished{});
        }

        return new MockChatOperation(std::move(events));
    }

private:
    QList<QList<domain::llm::ChatEvent>> m_responseQueue;
};

class WorkIntegrationTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void testAgentRuntimeExecutesBuiltinTools();
    void testAgentRuntimePermissionAskUserAndGrant();
    void testAgentRuntimeSuspendAndResume();
    void testMcpToolProviderNamespacedExecution();
    void testWorkViewModelUdfProjection();
    void testAgentRuntimeStateRealtimeSynchronization();
    void testAgentRuntimeCheckpointRecoveryWithPendingToolsAndPermissions();
    void testAgentRuntimeMaxToolRoundsStrictLimit();

private:
    QTemporaryDir m_tempDir;
};

void WorkIntegrationTests::initTestCase() {
    QVERIFY(m_tempDir.isValid());
    const QString dbPath = m_tempDir.filePath(QStringLiteral("test_work.db"));
    QVERIFY(data::sqlite::DatabaseManager::instance().initialize(dbPath));
    data::repository::SqliteModelRepository modelRepo;
    QVERIFY(modelRepo.initializeDatabase({}, {}));
}

void WorkIntegrationTests::cleanupTestCase() {
    data::sqlite::DatabaseManager::instance().close();
}

void WorkIntegrationTests::testAgentRuntimeExecutesBuiltinTools() {
    MockChatGateway mockGateway;

    // Round 1: 模型返回 write_file 工具调用
    QList<domain::llm::ChatEvent> r1;
    r1.append(domain::llm::EventStarted{});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_write_1"), QStringLiteral("write_file")});
    r1.append(domain::llm::EventToolCallDelta{QStringLiteral("call_write_1"), QStringLiteral(R"({"path":"agent_test.txt","content":"hello forgeai"})")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_write_1")});
    r1.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r1);

    // Round 2: 收到工具结果后，模型生成最终回答
    QList<domain::llm::ChatEvent> r2;
    r2.append(domain::llm::EventStarted{});
    r2.append(domain::llm::EventTextDelta{QStringLiteral("文件创建完成！")});
    r2.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r2);

    auto workspaceFs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    auto builtinProvider = std::make_shared<agent::tool::BuiltinToolProvider>(workspaceFs);
    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(builtinProvider);

    data::repository::SqliteAgentCheckpointRepository cpRepo;
    agent::runtime::AgentRuntime runtime(&mockGateway, nullptr, &toolRegistry, &cpRepo);

    agent::runtime::AgentRunContext context;
    context.sessionId = QStringLiteral("sess_test_exec");
    context.workspaceRoot = m_tempDir.path();
    context.policy.autoApproveWriteWorkspace = true;

    bool completed = false;
    QEventLoop loop;
    connect(&runtime, &agent::runtime::AgentRuntime::runCompleted, [&]() {
        completed = true;
        loop.quit();
    });
    connect(&runtime, &agent::runtime::AgentRuntime::runFailed, [&](const QString&, const domain::llm::ChatError& err) {
        qWarning() << "Run failed:" << err.userMessage;
        loop.quit();
    });

    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    runtime.startRun(context, QStringLiteral("请创建 agent_test.txt 文件"));
    loop.exec();

    QVERIFY2(completed, "testAgentRuntimeExecutesBuiltinTools timed out without runCompleted signal");
    QCOMPARE(runtime.currentState().status, domain::agent::AgentRunStatus::Completed);
    QCOMPARE(runtime.currentState().round, 2);

    // 验证文件是否已真实写入工作区磁盘
    const QString targetFile = QDir(m_tempDir.path()).filePath(QStringLiteral("agent_test.txt"));
    QVERIFY(QFile::exists(targetFile));

    QFile file(targetFile);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(file.readAll()), QStringLiteral("hello forgeai"));
    file.close();
}

void WorkIntegrationTests::testAgentRuntimePermissionAskUserAndGrant() {
    MockChatGateway mockGateway;

    // Round 1: 模型请求写文件
    QList<domain::llm::ChatEvent> r1;
    r1.append(domain::llm::EventStarted{});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_perm_1"), QStringLiteral("write_file")});
    r1.append(domain::llm::EventToolCallDelta{QStringLiteral("call_perm_1"), QStringLiteral(R"({"path":"perm_test.txt","content":"sensitive content"})")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_perm_1")});
    r1.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r1);

    // Round 2: 授权后模型返回完成
    QList<domain::llm::ChatEvent> r2;
    r2.append(domain::llm::EventStarted{});
    r2.append(domain::llm::EventTextDelta{QStringLiteral("已成功写入！")});
    r2.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r2);

    auto workspaceFs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    auto builtinProvider = std::make_shared<agent::tool::BuiltinToolProvider>(workspaceFs);
    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(builtinProvider);

    data::repository::SqliteAgentCheckpointRepository cpRepo;
    agent::runtime::AgentRuntime runtime(&mockGateway, nullptr, &toolRegistry, &cpRepo);

    agent::runtime::AgentRunContext context;
    context.sessionId = QStringLiteral("sess_test_perm");
    context.workspaceRoot = m_tempDir.path();
    // 禁止自动写工作区权限 -> 触发 AskUser
    context.policy.autoApproveWriteWorkspace = false;

    bool permissionRequestedReceived = false;
    QString pendingToolCallId;

    QEventLoop loop;
    connect(&runtime, &agent::runtime::AgentRuntime::permissionRequested, [&](const QString& sid, const domain::agent::ToolCall& call, const domain::agent::ToolPermission& perm) {
        Q_UNUSED(sid);
        Q_UNUSED(perm);
        permissionRequestedReceived = true;
        pendingToolCallId = call.id;
        loop.quit();
    });

    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    runtime.startRun(context, QStringLiteral("请写敏感文件"));
    loop.exec();

    // 验证此时暂停并处于 WaitingPermission 状态
    QVERIFY2(permissionRequestedReceived, "PermissionRequested was not received within timeout");
    QCOMPARE(runtime.currentState().status, domain::agent::AgentRunStatus::WaitingPermission);
    QCOMPARE(pendingToolCallId, QStringLiteral("call_perm_1"));

    // 验证在未授权前磁盘上文件尚未生成
    const QString targetFile = QDir(m_tempDir.path()).filePath(QStringLiteral("perm_test.txt"));
    QVERIFY(!QFile::exists(targetFile));

    // 用户在 UI 点击授权
    bool completeReceived = false;
    QEventLoop completeLoop;
    connect(&runtime, &agent::runtime::AgentRuntime::runCompleted, [&]() {
        completeReceived = true;
        completeLoop.quit();
    });

    QTimer::singleShot(3000, &completeLoop, &QEventLoop::quit);
    runtime.grantPermission(QStringLiteral("sess_test_perm"), pendingToolCallId, true);
    completeLoop.exec();

    // 验证授权后成功执行完成并写入磁盘
    QVERIFY2(completeReceived, "GrantPermission did not complete within timeout");
    QCOMPARE(runtime.currentState().status, domain::agent::AgentRunStatus::Completed);
    QVERIFY(QFile::exists(targetFile));
}

void WorkIntegrationTests::testAgentRuntimeSuspendAndResume() {
    MockChatGateway mockGateway;

    QList<domain::llm::ChatEvent> r1;
    r1.append(domain::llm::EventStarted{});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_sus_1"), QStringLiteral("read_file")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_sus_1")});
    r1.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r1);

    auto workspaceFs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    auto builtinProvider = std::make_shared<agent::tool::BuiltinToolProvider>(workspaceFs);
    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(builtinProvider);

    data::repository::SqliteAgentCheckpointRepository cpRepo;
    agent::runtime::AgentRuntime runtime(&mockGateway, nullptr, &toolRegistry, &cpRepo);

    agent::runtime::AgentRunContext context;
    context.sessionId = QStringLiteral("sess_test_suspend");
    context.workspaceRoot = m_tempDir.path();

    runtime.startRun(context, QStringLiteral("开始任务"));
    runtime.suspendRun();

    QCOMPARE(runtime.currentState().status, domain::agent::AgentRunStatus::Suspended);

    // 验证快照成功保存到 SQLite
    auto cpOpt = cpRepo.getLatestCheckpoint(QStringLiteral("sess_test_suspend"));
    QVERIFY(cpOpt.has_value());
    QCOMPARE(cpOpt->sessionId, QStringLiteral("sess_test_suspend"));

    // 恢复运行
    QList<domain::llm::ChatEvent> rResume;
    rResume.append(domain::llm::EventStarted{});
    rResume.append(domain::llm::EventTextDelta{QStringLiteral("恢复后的回复")});
    rResume.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(rResume);

    bool resumeCompleted = false;
    QEventLoop resumeLoop;
    connect(&runtime, &agent::runtime::AgentRuntime::runCompleted, [&]() {
        resumeCompleted = true;
        resumeLoop.quit();
    });

    QTimer::singleShot(3000, &resumeLoop, &QEventLoop::quit);
    runtime.resumeRun(context);
    resumeLoop.exec();

    QVERIFY2(resumeCompleted, "Resume run timed out without completion");
    QCOMPARE(runtime.currentState().status, domain::agent::AgentRunStatus::Completed);
}

void WorkIntegrationTests::testMcpToolProviderNamespacedExecution() {
    domain::agent::ToolDefinition toolDef;
    toolDef.name = QStringLiteral("scan_dir");
    toolDef.description = QStringLiteral("扫描目录");

    // 包装为带命名空间的 McpTool
    auto mcpTool = std::make_shared<llm::mcp::McpTool>(nullptr, QStringLiteral("fs_server"), toolDef);
    QCOMPARE(mcpTool->definition().name, QStringLiteral("mcp::fs_server::scan_dir"));

    agent::tool::ToolRegistry registry;
    QVERIFY(registry.registerTool(mcpTool));

    // 验证 ToolRegistry 能够通过命名空间寻找到该工具
    const auto defs = registry.definitions();
    QCOMPARE(defs.size(), 1);
    QCOMPARE(defs.first().name, QStringLiteral("mcp::fs_server::scan_dir"));

    auto found = registry.findTool(QStringLiteral("mcp::fs_server::scan_dir"));
    QVERIFY(found != nullptr);
    QCOMPARE(found->definition().name, QStringLiteral("mcp::fs_server::scan_dir"));
}

void WorkIntegrationTests::testWorkViewModelUdfProjection() {
    MockChatGateway mockGateway;

    QList<domain::llm::ChatEvent> r1;
    r1.append(domain::llm::EventStarted{});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_udf_1"), QStringLiteral("list_files")});
    r1.append(domain::llm::EventToolCallDelta{QStringLiteral("call_udf_1"), QStringLiteral(R"({"path":"."})")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_udf_1")});
    r1.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r1);

    QList<domain::llm::ChatEvent> r2;
    r2.append(domain::llm::EventStarted{});
    r2.append(domain::llm::EventTextDelta{QStringLiteral("所有文件列表已找到")});
    r2.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r2);

    auto workspaceFs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    auto builtinProvider = std::make_shared<agent::tool::BuiltinToolProvider>(workspaceFs);
    auto toolRegistry = std::make_unique<agent::tool::ToolRegistry>();
    toolRegistry->registerProvider(builtinProvider);

    auto runtime = std::make_unique<agent::runtime::AgentRuntime>(&mockGateway, nullptr, toolRegistry.get());

    // 构造模拟 ModelService
    auto modelRepo = std::make_shared<data::repository::SqliteModelRepository>();
    auto modelRegistry = std::make_shared<core::model::ModelRegistry>(modelRepo);
    auto modelService = std::make_unique<services::model::ModelService>(modelRegistry);

    // 注入一个可用的 ResolvedModel
    domain::model::ModelProvider provider;
    provider.id = QStringLiteral("mock_prov");
    provider.name = QStringLiteral("Mock Provider");
    provider.baseUrl = QStringLiteral("https://api.mock.com");
    provider.apiKey = QStringLiteral("mock-key");
    provider.isEnabled = true;
    provider.isCustom = true;
    provider.origin = domain::model::DataOrigin::User;
    modelService->saveProvider(provider);

    domain::model::ProviderModel binding;
    binding.providerId = QStringLiteral("mock_prov");
    binding.remoteModelId = QStringLiteral("mock-model");
    binding.isEnabled = true;
    binding.isCustom = true;
    binding.origin = domain::model::DataOrigin::User;
    modelService->saveProviderModel(binding);

    auto runAgentUseCase = std::make_unique<application::usecase::agent::RunAgentUseCase>(
        runtime.get(),
        modelService.get()
    );

    auto projRepo = std::make_unique<data::repository::SqliteProjectRepository>();
    auto convRepo = std::make_unique<data::repository::SqliteConversationRepository>();
    services::project::ProjectContextService projContextService;

    application::usecase::work::WorkUseCases useCases;
    useCases.runAgent = runAgentUseCase.get();
    useCases.projectRepository = projRepo.get();
    useCases.conversationRepository = convRepo.get();

    ui::screen::work::WorkViewModel vm(useCases, &projContextService);
    vm.addProject(m_tempDir.path(), QStringLiteral("TestProject"));
    vm.newSession();

    bool udfCompleted = false;
    QEventLoop loop;
    connect(runAgentUseCase.get(), &application::usecase::agent::RunAgentUseCase::runCompleted, [&]() {
        udfCompleted = true;
        loop.quit();
    });
    connect(runAgentUseCase.get(), &application::usecase::agent::RunAgentUseCase::runFailed, [&](const QString& sid, const domain::llm::ChatError& err) {
        qWarning() << "[TEST FAIL REASON]" << sid << err.code << err.userMessage << err.message;
        loop.quit();
    });

    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    vm.startTask(QStringLiteral("列出文件"));
    loop.exec();

    QVERIFY2(udfCompleted, "WorkViewModel UDF projection timed out without runCompleted signal");

    // 验证 UI 状态单向投影
    const auto state = vm.state();
    QCOMPARE(state.agentUiState.status, domain::agent::AgentRunStatus::Completed);
    QVERIFY(!state.messages.isEmpty());
    QVERIFY(!state.toolEvents.isEmpty());
    QCOMPARE(state.toolEvents.first().name, QStringLiteral("list_files"));
}

void WorkIntegrationTests::testAgentRuntimeStateRealtimeSynchronization() {
    MockChatGateway mockGateway;

    QList<domain::llm::ChatEvent> r1;
    r1.append(domain::llm::EventStarted{});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_sync_1"), QStringLiteral("list_files")});
    r1.append(domain::llm::EventToolCallDelta{QStringLiteral("call_sync_1"), QStringLiteral(R"({"path":"."})")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_sync_1")});
    r1.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r1);

    QList<domain::llm::ChatEvent> r2;
    r2.append(domain::llm::EventStarted{});
    r2.append(domain::llm::EventTextDelta{QStringLiteral("完成")});
    r2.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r2);

    auto workspaceFs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    auto builtinProvider = std::make_shared<agent::tool::BuiltinToolProvider>(workspaceFs);
    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(builtinProvider);

    data::repository::SqliteAgentCheckpointRepository cpRepo;
    agent::runtime::AgentRuntime runtime(&mockGateway, nullptr, &toolRegistry, &cpRepo);

    agent::runtime::AgentRunContext context;
    context.sessionId = QStringLiteral("sess_state_sync");
    context.workspaceRoot = m_tempDir.path();

    bool sawPendingCallsInState = false;
    bool sawResultsInState = false;

    connect(&runtime, &agent::runtime::AgentRuntime::stateChanged, [&](const domain::agent::AgentRunState& s) {
        if (!s.pendingCalls.isEmpty()) {
            sawPendingCallsInState = true;
        }
        if (!s.results.isEmpty()) {
            sawResultsInState = true;
        }
    });

    bool completed = false;
    QEventLoop loop;
    connect(&runtime, &agent::runtime::AgentRuntime::runCompleted, [&]() {
        completed = true;
        loop.quit();
    });

    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    runtime.startRun(context, QStringLiteral("测试状态同步"));
    loop.exec();

    QVERIFY2(completed, "State sync test timed out");
    QVERIFY2(sawPendingCallsInState, "AgentRunState should accurately contain pendingCalls during tool lifecycle");
    QVERIFY2(sawResultsInState, "AgentRunState should accurately contain results during tool lifecycle");
}

void WorkIntegrationTests::testAgentRuntimeCheckpointRecoveryWithPendingToolsAndPermissions() {
    MockChatGateway mockGateway;

    // Round 1: 模型返回写文件工具请求
    QList<domain::llm::ChatEvent> r1;
    r1.append(domain::llm::EventStarted{});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_rec_1"), QStringLiteral("write_file")});
    r1.append(domain::llm::EventToolCallDelta{QStringLiteral("call_rec_1"), QStringLiteral(R"({"path":"recovered_file.txt","content":"checkpoint data"})")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_rec_1")});
    r1.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r1);

    auto workspaceFs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    auto builtinProvider = std::make_shared<agent::tool::BuiltinToolProvider>(workspaceFs);
    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(builtinProvider);

    data::repository::SqliteAgentCheckpointRepository cpRepo;

    agent::runtime::AgentRunContext context;
    context.sessionId = QStringLiteral("sess_test_deep_recovery");
    context.workspaceRoot = m_tempDir.path();
    context.policy.autoApproveWriteWorkspace = false; // 触发 AskUser

    // 实例 1 启动运行直至暂停并写入快照
    {
        agent::runtime::AgentRuntime runtime1(&mockGateway, nullptr, &toolRegistry, &cpRepo);
        bool paused = false;
        QEventLoop loop1;
        connect(&runtime1, &agent::runtime::AgentRuntime::permissionRequested, [&]() {
            paused = true;
            loop1.quit();
        });

        QTimer::singleShot(3000, &loop1, &QEventLoop::quit);
        runtime1.startRun(context, QStringLiteral("请写恢复文件"));
        loop1.exec();

        QVERIFY2(paused, "Runtime1 should pause in WaitingPermission");
        QCOMPARE(runtime1.currentState().status, domain::agent::AgentRunStatus::WaitingPermission);
    }

    // 校验 SQLite 快照中准确保存了现场状态与 pendingToolCalls
    auto cp = cpRepo.getLatestCheckpoint(QStringLiteral("sess_test_deep_recovery"));
    QVERIFY(cp.has_value());
    QCOMPARE(cp->status, domain::agent::AgentRunStatus::WaitingPermission);
    QCOMPARE(cp->pendingToolCalls.size(), 1);
    QCOMPARE(cp->pendingToolCalls.first().id, QStringLiteral("call_rec_1"));

    // 实例 2（模拟重启）恢复运行
    QList<domain::llm::ChatEvent> r2;
    r2.append(domain::llm::EventStarted{});
    r2.append(domain::llm::EventTextDelta{QStringLiteral("恢复后完成写入")});
    r2.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r2);

    agent::runtime::AgentRuntime runtime2(&mockGateway, nullptr, &toolRegistry, &cpRepo);

    bool permRequested2 = false;
    QString pendingCallId2;
    QEventLoop loop2;
    connect(&runtime2, &agent::runtime::AgentRuntime::permissionRequested, [&](const QString& sid, const domain::agent::ToolCall& call, const domain::agent::ToolPermission& perm) {
        Q_UNUSED(sid);
        Q_UNUSED(perm);
        permRequested2 = true;
        pendingCallId2 = call.id;
        loop2.quit();
    });

    QTimer::singleShot(3000, &loop2, &QEventLoop::quit);
    runtime2.resumeRun(context);
    loop2.exec();

    QVERIFY2(permRequested2, "Runtime2 should restore and request permission for pending tool call");
    QCOMPARE(pendingCallId2, QStringLiteral("call_rec_1"));

    // 授权并等待完成
    bool completed2 = false;
    QEventLoop loopFinish;
    connect(&runtime2, &agent::runtime::AgentRuntime::runCompleted, [&]() {
        completed2 = true;
        loopFinish.quit();
    });

    QTimer::singleShot(3000, &loopFinish, &QEventLoop::quit);
    runtime2.grantPermission(QStringLiteral("sess_test_deep_recovery"), pendingCallId2, true);
    loopFinish.exec();

    QVERIFY2(completed2, "Runtime2 should complete after granting permission");
    QCOMPARE(runtime2.currentState().status, domain::agent::AgentRunStatus::Completed);

    // 验证文件在恢复授权后写入成功
    const QString targetFile = QDir(m_tempDir.path()).filePath(QStringLiteral("recovered_file.txt"));
    QVERIFY(QFile::exists(targetFile));
}

void WorkIntegrationTests::testAgentRuntimeMaxToolRoundsStrictLimit() {
    MockChatGateway mockGateway;

    // 为多轮持续返回 tool_call
    for (int i = 0; i < 5; ++i) {
        QList<domain::llm::ChatEvent> r;
        r.append(domain::llm::EventStarted{});
        r.append(domain::llm::EventToolCallStarted{QStringLiteral("call_%1").arg(i), QStringLiteral("list_files")});
        r.append(domain::llm::EventToolCallDelta{QStringLiteral("call_%1").arg(i), QStringLiteral(R"({"path":"."})")});
        r.append(domain::llm::EventToolCallFinished{QStringLiteral("call_%1").arg(i)});
        r.append(domain::llm::EventFinished{});
        mockGateway.enqueueResponse(r);
    }

    auto workspaceFs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    auto builtinProvider = std::make_shared<agent::tool::BuiltinToolProvider>(workspaceFs);
    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(builtinProvider);

    data::repository::SqliteAgentCheckpointRepository cpRepo;
    agent::runtime::AgentRuntime runtime(&mockGateway, nullptr, &toolRegistry, &cpRepo);

    agent::runtime::AgentRunContext context;
    context.sessionId = QStringLiteral("sess_max_rounds");
    context.workspaceRoot = m_tempDir.path();
    context.policy.maxToolRounds = 2; // 严格限定最多 2 轮

    bool completed = false;
    QEventLoop loop;
    connect(&runtime, &agent::runtime::AgentRuntime::runCompleted, [&]() {
        completed = true;
        loop.quit();
    });

    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    runtime.startRun(context, QStringLiteral("循环工具调用任务"));
    loop.exec();

    QVERIFY2(completed, "Max tool rounds test timed out");
    QCOMPARE(runtime.currentState().status, domain::agent::AgentRunStatus::Completed);
    QCOMPARE(runtime.currentState().round, 2);
}

QTEST_GUILESS_MAIN(WorkIntegrationTests)
#include "WorkIntegrationTests.moc"
