#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QTimer>
// Recompiled against updated McpClient.h and AgentRunState.h
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
#include "application/usecase/work/SwitchProjectUseCase.h"
#include "llm/mcp/McpProjectRuntimeCoordinator.h"
#include "llm/mcp/McpManager.h"

// 模拟 ChatOperation
class MockChatOperation final : public application::ports::IChatOperation {
    Q_OBJECT
public:
    explicit MockChatOperation(QList<domain::llm::ChatEvent> events, int delayMs = 10, QObject* parent = nullptr)
        : application::ports::IChatOperation(parent), m_events(std::move(events)) {
        if (delayMs >= 0) {
            QTimer::singleShot(delayMs, this, [this]() {
                if (m_cancelled) return;
                for (const auto& ev : m_events) {
                    if (m_cancelled) break;
                    emit eventReceived(ev);
                }
            });
        }
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
    void enqueueResponse(QList<domain::llm::ChatEvent> events, int delayMs = 10) {
        m_responseQueue.append({std::move(events), delayMs});
    }

    application::ports::IChatOperation* sendRequest(
        const domain::model::ModelProvider& provider,
        const domain::llm::ChatRequest& request
    ) override {
        Q_UNUSED(provider);
        Q_UNUSED(request);

        QList<domain::llm::ChatEvent> events;
        int delayMs = 10;
        if (!m_responseQueue.isEmpty()) {
            auto item = m_responseQueue.takeFirst();
            events = item.first;
            delayMs = item.second;
        } else {
            events.append(domain::llm::EventStarted{});
            events.append(domain::llm::EventTextDelta{QStringLiteral("默认模型回答")});
            events.append(domain::llm::EventFinished{});
        }

        return new MockChatOperation(std::move(events), delayMs);
    }

private:
    QList<QPair<QList<domain::llm::ChatEvent>, int>> m_responseQueue;
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
    void testAgentRuntimeEnforcesEnabledToolsFiltering();
    void testAgentRuntimeParallelToolExecution();
    void testAgentRuntimeTimeoutWatchdog();
    void testWorkViewModelMcpProjectSwitchUnload();
    void testAgentRuntimeConcurrentMcpToolCallsSafety();
    void testAgentRuntimeToolExecutionTimeoutProtection();
    void testAgentRuntimeSingleSlowToolTimeoutProtection();
    void testAgentRuntimeCancelRunPropagatesToTools();
    void testAgentRuntimeUncooperativeToolRuntimeDestructionSafety();
    void testAgentRuntimeImmediateToolCheckpointRecovery();
    void testAgentRuntimePermissionScopeRunApproval();
    void testAgentRuntimePermissionScopeProjectAndGlobalApproval();
    void testAgentRuntimeToolOutputTruncation();
    void testAgentRuntimeParallelResultOrderPreservation();

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

void WorkIntegrationTests::testAgentRuntimeEnforcesEnabledToolsFiltering() {
    MockChatGateway mockGateway;

    // 模型尝试调用 write_file
    QList<domain::llm::ChatEvent> r1;
    r1.append(domain::llm::EventStarted{});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_write"), QStringLiteral("write_file")});
    r1.append(domain::llm::EventToolCallDelta{QStringLiteral("call_write"), QStringLiteral(R"({"path":"blocked.txt","content":"should not exist"})")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_write")});
    r1.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r1);

    // 收到拒绝后模型直接输出文本
    QList<domain::llm::ChatEvent> r2;
    r2.append(domain::llm::EventStarted{});
    r2.append(domain::llm::EventTextDelta{QStringLiteral("工具被安全策略拦截。")});
    r2.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r2);

    auto workspaceFs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    auto builtinProvider = std::make_shared<agent::tool::BuiltinToolProvider>(workspaceFs);
    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(builtinProvider);

    data::repository::SqliteAgentCheckpointRepository cpRepo;
    agent::runtime::AgentRuntime runtime(&mockGateway, nullptr, &toolRegistry, &cpRepo);

    agent::runtime::AgentRunContext context;
    context.sessionId = QStringLiteral("sess_filter_tools");
    context.workspaceRoot = m_tempDir.path();
    // 仅允许 list_files，不启用 write_file
    context.enabledTools = {QStringLiteral("list_files")};

    bool toolResultError = false;
    QString toolResultContent;
    connect(&runtime, &agent::runtime::AgentRuntime::toolResultReady, [&](const QString&, const domain::agent::ToolResult& res) {
        if (res.toolCallId == QStringLiteral("call_write")) {
            toolResultError = res.isError;
            toolResultContent = res.content;
        }
    });

    bool completed = false;
    QEventLoop loop;
    connect(&runtime, &agent::runtime::AgentRuntime::runCompleted, [&]() {
        completed = true;
        loop.quit();
    });

    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    runtime.startRun(context, QStringLiteral("尝试调用未启用工具"));
    loop.exec();

    QVERIFY2(completed, "Enabled tools test timed out");
    QVERIFY(toolResultError);
    QVERIFY(toolResultContent.contains(QStringLiteral("未在当前智能体启用列表中")));

    // 校验被拦截的写操作未写入磁盘
    const QString blockedFile = QDir(m_tempDir.path()).filePath(QStringLiteral("blocked.txt"));
    QVERIFY(!QFile::exists(blockedFile));
}

void WorkIntegrationTests::testAgentRuntimeParallelToolExecution() {
    MockChatGateway mockGateway;

    // 创建两个测试文件
    const QString f1 = QDir(m_tempDir.path()).filePath(QStringLiteral("p1.txt"));
    const QString f2 = QDir(m_tempDir.path()).filePath(QStringLiteral("p2.txt"));
    {
        QFile file1(f1);
        QVERIFY(file1.open(QIODevice::WriteOnly | QIODevice::Text));
        file1.write("parallel 1");
    }
    {
        QFile file2(f2);
        QVERIFY(file2.open(QIODevice::WriteOnly | QIODevice::Text));
        file2.write("parallel 2");
    }

    // 第一轮：并发调用两个 read_file
    QList<domain::llm::ChatEvent> r1;
    r1.append(domain::llm::EventStarted{});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_p1"), QStringLiteral("read_file")});
    r1.append(domain::llm::EventToolCallDelta{QStringLiteral("call_p1"), QStringLiteral(R"({"path":"p1.txt"})")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_p1")});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_p2"), QStringLiteral("read_file")});
    r1.append(domain::llm::EventToolCallDelta{QStringLiteral("call_p2"), QStringLiteral(R"({"path":"p2.txt"})")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_p2")});
    r1.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r1);

    // 第二轮：汇总结果
    QList<domain::llm::ChatEvent> r2;
    r2.append(domain::llm::EventStarted{});
    r2.append(domain::llm::EventTextDelta{QStringLiteral("并发读取完成")});
    r2.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r2);

    auto workspaceFs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    auto builtinProvider = std::make_shared<agent::tool::BuiltinToolProvider>(workspaceFs);
    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(builtinProvider);

    data::repository::SqliteAgentCheckpointRepository cpRepo;
    agent::runtime::AgentRuntime runtime(&mockGateway, nullptr, &toolRegistry, &cpRepo);

    agent::runtime::AgentRunContext context;
    context.sessionId = QStringLiteral("sess_parallel");
    context.workspaceRoot = m_tempDir.path();
    context.policy.allowParallelToolExecution = true;

    QMap<QString, QString> resultsMap;
    connect(&runtime, &agent::runtime::AgentRuntime::toolResultReady, [&](const QString&, const domain::agent::ToolResult& res) {
        resultsMap.insert(res.toolCallId, res.content);
    });

    bool completed = false;
    QEventLoop loop;
    connect(&runtime, &agent::runtime::AgentRuntime::runCompleted, [&]() {
        completed = true;
        loop.quit();
    });

    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    runtime.startRun(context, QStringLiteral("并发读取文件"));
    loop.exec();

    QVERIFY2(completed, "Parallel execution test timed out");
    QCOMPARE(resultsMap.size(), 2);
    QVERIFY(resultsMap.value(QStringLiteral("call_p1")).contains(QStringLiteral("parallel 1")));
    QVERIFY(resultsMap.value(QStringLiteral("call_p2")).contains(QStringLiteral("parallel 2")));
}

void WorkIntegrationTests::testAgentRuntimeTimeoutWatchdog() {
    MockChatGateway mockGateway;

    // 模拟网关延迟 500ms，而策略超时设定为 100ms
    QList<domain::llm::ChatEvent> slowResponse;
    slowResponse.append(domain::llm::EventStarted{});
    slowResponse.append(domain::llm::EventTextDelta{QStringLiteral("迟到的回复")});
    slowResponse.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(slowResponse, 500);

    auto workspaceFs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    auto builtinProvider = std::make_shared<agent::tool::BuiltinToolProvider>(workspaceFs);
    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(builtinProvider);

    data::repository::SqliteAgentCheckpointRepository cpRepo;
    agent::runtime::AgentRuntime runtime(&mockGateway, nullptr, &toolRegistry, &cpRepo);

    agent::runtime::AgentRunContext context;
    context.sessionId = QStringLiteral("sess_timeout");
    context.workspaceRoot = m_tempDir.path();
    context.policy.timeoutMs = 100; // 超时看门狗 100ms

    bool failed = false;
    QString errorCode;
    QEventLoop loop;
    connect(&runtime, &agent::runtime::AgentRuntime::runFailed, [&](const QString&, const domain::llm::ChatError& err) {
        failed = true;
        errorCode = err.code;
        loop.quit();
    });

    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    runtime.startRun(context, QStringLiteral("超时任务"));
    loop.exec();

    QVERIFY2(failed, "Timeout watchdog should trigger runFailed");
    QCOMPARE(errorCode, QStringLiteral("RequestTimeout"));
    QCOMPARE(runtime.currentState().status, domain::agent::AgentRunStatus::Failed);
}

void WorkIntegrationTests::testWorkViewModelMcpProjectSwitchUnload() {
    QTemporaryDir proj1Dir;
    QTemporaryDir proj2Dir;
    QVERIFY(proj1Dir.isValid() && proj2Dir.isValid());

    // 在 Project 1 创建 .mcp.json
    QFile mcpFile1(QDir(proj1Dir.path()).filePath(QStringLiteral(".mcp.json")));
    QVERIFY(mcpFile1.open(QIODevice::WriteOnly | QIODevice::Text));
    mcpFile1.write(R"({
        "mcpServers": {
            "proj1_server": {
                "command": "cmd.exe",
                "args": ["/c", "echo {}"]
            }
        }
    })");
    mcpFile1.close();

    llm::mcp::McpManager mcpManager;
    llm::mcp::McpProjectRuntimeCoordinator coordinator(&mcpManager);

    // 验证通过 coordinator.loadProject 动态挂载并解析 .mcp.json
    coordinator.loadProject(proj1Dir.path());
    QVERIFY(mcpManager.getSession(QStringLiteral("proj1_server")) != nullptr);

    // 通过 McpProjectRuntimeCoordinator + SwitchProjectUseCase + WorkViewModel 触发项目切换
    services::project::ProjectContextService projectCtxService;
    application::usecase::work::SwitchProjectUseCase switchUseCase(&coordinator, &projectCtxService);

    application::usecase::work::WorkUseCases useCases;
    useCases.switchProject = &switchUseCase;

    ui::screen::work::WorkViewModel vm(useCases);
    vm.setProjectRoot(proj1Dir.path());
    QCOMPARE(vm.state().projectRoot, QDir(proj1Dir.path()).canonicalPath());
    QVERIFY(mcpManager.getSession(QStringLiteral("proj1_server")) != nullptr);

    // 切换到 Project 2，验证 Project 1 的 MCP 服务被 SwitchProjectUseCase 自动卸载
    vm.setProjectRoot(proj2Dir.path());
    QCOMPARE(vm.state().projectRoot, QDir(proj2Dir.path()).canonicalPath());
    QVERIFY(mcpManager.getSession(QStringLiteral("proj1_server")) == nullptr);
}

// 模拟测试用的慢速工具（支持合作式感知 CancellationToken）
class SlowTestTool final : public application::ports::ITool {
public:
    domain::agent::ToolDefinition definition() const override {
        domain::agent::ToolDefinition def;
        def.name = QStringLiteral("slow_tool");
        return def;
    }
    application::ports::ToolExecutionTraits traits() const override {
        return {true, true, false, QString()};
    }
    std::unique_ptr<application::ports::IToolOperation> execute(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) override {
        return std::make_unique<application::ports::ThreadedToolOperation>(
            call.id,
            [this, call, context]() {
                for (int i = 0; i < 40; ++i) {
                    if (context.cancellationToken.isCanceled()) {
                        wasCanceled = true;
                        return domain::agent::ToolResult{call.id, QStringLiteral("操作已合作式取消"), true};
                    }
                    QThread::msleep(10);
                }
                return domain::agent::ToolResult{call.id, QStringLiteral("slow finish"), false};
            },
            context.timeoutMs > 0 ? context.timeoutMs : 10000
        );
    }

    std::atomic<bool> wasCanceled{false};
};

// 模拟非线程安全 MCP 工具
class NonThreadSafeMockTool final : public application::ports::ITool {
public:
    explicit NonThreadSafeMockTool(const QString& name) : m_name(name) {}
    domain::agent::ToolDefinition definition() const override {
        domain::agent::ToolDefinition def;
        def.name = m_name;
        return def;
    }
    application::ports::ToolExecutionTraits traits() const override {
        return {false, true, false, QStringLiteral("mcp-session:%1").arg(m_name)};
    }
    std::unique_ptr<application::ports::IToolOperation> execute(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext& context
    ) override {
        Q_UNUSED(context);
        return std::make_unique<application::ports::ImmediateToolOperation>(
            call.id,
            [this, call]() {
                return domain::agent::ToolResult{call.id, QStringLiteral("mcp res: %1").arg(m_name), false};
            }
        );
    }
private:
    QString m_name;
};

class SimpleMockProvider final : public application::ports::IToolProvider {
public:
    explicit SimpleMockProvider(QList<std::shared_ptr<application::ports::ITool>> tools) : m_tools(std::move(tools)) {}
    QList<std::shared_ptr<application::ports::ITool>> tools() const override { return m_tools; }
private:
    QList<std::shared_ptr<application::ports::ITool>> m_tools;
};

void WorkIntegrationTests::testAgentRuntimeConcurrentMcpToolCallsSafety() {
    MockChatGateway mockGateway;

    // 模型下发两个并发 MCP 工具调用
    QList<domain::llm::ChatEvent> r1;
    r1.append(domain::llm::EventStarted{});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_mcp1"), QStringLiteral("mcp::server::tool1")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_mcp1")});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_mcp2"), QStringLiteral("mcp::server::tool2")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_mcp2")});
    r1.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r1);

    QList<domain::llm::ChatEvent> r2;
    r2.append(domain::llm::EventStarted{});
    r2.append(domain::llm::EventTextDelta{QStringLiteral("MCP 工具调用完成")});
    r2.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r2);

    auto mcp1 = std::make_shared<NonThreadSafeMockTool>(QStringLiteral("mcp::server::tool1"));
    auto mcp2 = std::make_shared<NonThreadSafeMockTool>(QStringLiteral("mcp::server::tool2"));
    auto provider = std::make_shared<SimpleMockProvider>(QList<std::shared_ptr<application::ports::ITool>>{mcp1, mcp2});

    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(provider);

    data::repository::SqliteAgentCheckpointRepository cpRepo;
    agent::runtime::AgentRuntime runtime(&mockGateway, nullptr, &toolRegistry, &cpRepo);

    agent::runtime::AgentRunContext context;
    context.sessionId = QStringLiteral("sess_mcp_concurrency");
    context.workspaceRoot = m_tempDir.path();
    context.policy.allowParallelToolExecution = true;

    QMap<QString, QString> resultsMap;
    connect(&runtime, &agent::runtime::AgentRuntime::toolResultReady, [&](const QString&, const domain::agent::ToolResult& res) {
        resultsMap.insert(res.toolCallId, res.content);
    });

    bool completed = false;
    QEventLoop loop;
    connect(&runtime, &agent::runtime::AgentRuntime::runCompleted, [&]() {
        completed = true;
        loop.quit();
    });

    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    runtime.startRun(context, QStringLiteral("调用多个 MCP 工具"));
    loop.exec();

    QVERIFY2(completed, "Concurrent MCP tool calls test timed out");
    QCOMPARE(resultsMap.size(), 2);
    QCOMPARE(resultsMap.value(QStringLiteral("call_mcp1")), QStringLiteral("mcp res: mcp::server::tool1"));
    QCOMPARE(resultsMap.value(QStringLiteral("call_mcp2")), QStringLiteral("mcp res: mcp::server::tool2"));
}

void WorkIntegrationTests::testAgentRuntimeToolExecutionTimeoutProtection() {
    MockChatGateway mockGateway;

    // 模型下发两个并发工具调用（包含耗时 400ms 的慢速工具）
    QList<domain::llm::ChatEvent> r1;
    r1.append(domain::llm::EventStarted{});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_slow1"), QStringLiteral("slow_tool")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_slow1")});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_slow2"), QStringLiteral("slow_tool")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_slow2")});
    r1.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r1);

    QList<domain::llm::ChatEvent> r2;
    r2.append(domain::llm::EventStarted{});
    r2.append(domain::llm::EventTextDelta{QStringLiteral("处理完毕")});
    r2.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r2);

    auto slowTool = std::make_shared<SlowTestTool>();
    auto provider = std::make_shared<SimpleMockProvider>(QList<std::shared_ptr<application::ports::ITool>>{slowTool});

    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(provider);

    data::repository::SqliteAgentCheckpointRepository cpRepo;
    agent::runtime::AgentRuntime runtime(&mockGateway, nullptr, &toolRegistry, &cpRepo);

    agent::runtime::AgentRunContext context;
    context.sessionId = QStringLiteral("sess_tool_timeout");
    context.workspaceRoot = m_tempDir.path();
    context.policy.timeoutMs = 100; // 设定超时时间为 100ms
    context.policy.allowParallelToolExecution = true;

    QMap<QString, domain::agent::ToolResult> resultsMap;
    connect(&runtime, &agent::runtime::AgentRuntime::toolResultReady, [&](const QString&, const domain::agent::ToolResult& res) {
        resultsMap.insert(res.toolCallId, res);
    });

    bool completed = false;
    QEventLoop loop;
    connect(&runtime, &agent::runtime::AgentRuntime::runCompleted, [&]() {
        completed = true;
        loop.quit();
    });

    QElapsedTimer elapsed;
    elapsed.start();

    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    runtime.startRun(context, QStringLiteral("超时工具调用"));
    loop.exec();

    const qint64 totalElapsedMs = elapsed.elapsed();

    QVERIFY2(completed, "Tool execution timeout test timed out");
    QCOMPARE(resultsMap.size(), 2);
    QVERIFY(resultsMap.value(QStringLiteral("call_slow1")).isError);
    QVERIFY(resultsMap.value(QStringLiteral("call_slow1")).content.contains(QStringLiteral("超时")));

    // 关键断言：总耗时紧贴 100ms 超时限制，证明未被 400ms 后台阻塞
    QVERIFY2(totalElapsedMs < 300, QStringLiteral("工具超时应立即返回，实际耗时 %1 ms").arg(totalElapsedMs).toUtf8().constData());
}

void WorkIntegrationTests::testAgentRuntimeSingleSlowToolTimeoutProtection() {
    MockChatGateway mockGateway;

    // 模型下发单个慢速工具调用
    QList<domain::llm::ChatEvent> r1;
    r1.append(domain::llm::EventStarted{});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_slow_single"), QStringLiteral("slow_tool")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_slow_single")});
    r1.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r1);

    auto slowTool = std::make_shared<SlowTestTool>();
    auto provider = std::make_shared<SimpleMockProvider>(QList<std::shared_ptr<application::ports::ITool>>{slowTool});

    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(provider);

    data::repository::SqliteAgentCheckpointRepository cpRepo;
    agent::runtime::AgentRuntime runtime(&mockGateway, nullptr, &toolRegistry, &cpRepo);

    agent::runtime::AgentRunContext context;
    context.sessionId = QStringLiteral("sess_single_slow_tool");
    context.workspaceRoot = m_tempDir.path();
    context.policy.timeoutMs = 100; // 设定超时 100ms
    context.policy.allowParallelToolExecution = true; // 这是默认开启的

    QMap<QString, domain::agent::ToolResult> resultsMap;
    connect(&runtime, &agent::runtime::AgentRuntime::toolResultReady, [&](const QString&, const domain::agent::ToolResult& res) {
        resultsMap.insert(res.toolCallId, res);
    });

    QEventLoop loop;
    connect(&runtime, &agent::runtime::AgentRuntime::runCompleted, [&loop]() {
        loop.quit();
    });

    QElapsedTimer elapsed;
    elapsed.start();

    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    runtime.startRun(context, QStringLiteral("单次超时工具"));
    loop.exec();

    const qint64 totalElapsedMs = elapsed.elapsed();

    QCOMPARE(resultsMap.size(), 1);
    QVERIFY(resultsMap.value(QStringLiteral("call_slow_single")).isError);
    QVERIFY(resultsMap.value(QStringLiteral("call_slow_single")).content.contains(QStringLiteral("超时")));

    // 关键断言：即使只有 1 个工具，也应该受到超时与异步控制的保护
    QVERIFY2(totalElapsedMs < 300, QStringLiteral("单工具超时也应立即返回，实际耗时 %1 ms").arg(totalElapsedMs).toUtf8().constData());
}

class EventLoopMockMcpTool final : public application::ports::ITool {
public:
    domain::agent::ToolDefinition definition() const override {
        return {QStringLiteral("mcp_loop_tool"), {}};
    }
    application::ports::ToolExecutionTraits traits() const override {
        return {false, true, false, QStringLiteral("mcp-session:mock_loop")};
    }
    std::unique_ptr<application::ports::IToolOperation> execute(const domain::agent::ToolCall& call, const application::ports::ToolExecutionContext& context) override {
        return std::make_unique<application::ports::ThreadedToolOperation>(
            call.id,
            [this, call, context]() {
                for (int i = 0; i < 80; ++i) {
                    if (context.cancellationToken.isCanceled()) {
                        wasCanceled = true;
                        return domain::agent::ToolResult{call.id, QStringLiteral("MCP_Cancelled_Via_Token"), true};
                    }
                    QThread::msleep(10);
                }
                return domain::agent::ToolResult{call.id, QStringLiteral("MCP_Finished_Naturally"), false};
            },
            context.timeoutMs > 0 ? context.timeoutMs : 10000
        );
    }
    std::atomic<bool> wasCanceled{false};
};

void WorkIntegrationTests::testAgentRuntimeCancelRunPropagatesToTools() {
    MockChatGateway mockGateway;

    // 模型下发两个工具：一个慢速并发工具（isThreadSafe=true），一个 MCP 模拟工具（isThreadSafe=false）
    QList<domain::llm::ChatEvent> r1;
    r1.append(domain::llm::EventStarted{});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_slow"), QStringLiteral("slow_tool")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_slow")});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_mcp"), QStringLiteral("mcp_loop_tool")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_mcp")});
    r1.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r1);

    auto slowTool = std::make_shared<SlowTestTool>();
    auto mcpTool = std::make_shared<EventLoopMockMcpTool>();
    auto provider = std::make_shared<SimpleMockProvider>(QList<std::shared_ptr<application::ports::ITool>>{slowTool, mcpTool});

    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(provider);

    agent::runtime::AgentRuntime runtime(&mockGateway, nullptr, &toolRegistry, nullptr);
    agent::runtime::AgentRunContext context;
    context.sessionId = QStringLiteral("sess_cancel_test");
    context.workspaceRoot = m_tempDir.path();
    context.policy.timeoutMs = 5000; // 足够长，保证不会自然超时
    context.policy.allowParallelToolExecution = true;

    QMap<QString, domain::agent::ToolResult> resultsMap;
    connect(&runtime, &agent::runtime::AgentRuntime::toolResultReady, [&](const QString&, const domain::agent::ToolResult& res) {
        resultsMap.insert(res.toolCallId, res);
    });

    QEventLoop loop;
    connect(&runtime, &agent::runtime::AgentRuntime::runCompleted, [&]() {
        loop.quit();
    });
    connect(&runtime, &agent::runtime::AgentRuntime::runFailed, [&]() {
        loop.quit();
    });

    // 启动非阻塞取消定时器
    QTimer::singleShot(150, [&runtime]() {
        runtime.cancelRun();
    });

    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    runtime.startRun(context, QStringLiteral("触发取消"));
    loop.exec();

    // cancelRun() destroys ops immediately (after emitting toolResultReady) and then
    // emits runCompleted synchronously. The background threads may still be sleeping
    // at that point. Give them a moment to detect the cancellation token.
    QTest::qWait(400);

    // 验证：
    // 1. 并发工具捕获到了取消信号（通过 CancellationToken 合作式传播）
    QVERIFY2(slowTool->wasCanceled, "Slow built-in tool was not cooperatively cancelled");
    QCOMPARE(resultsMap.size(), 2);
    // cancelRun() emits a generic "操作已取消" result for rapid cleanup; the thread's
    // cooperative result arrives later (discarded via QPointer) — so isError is the right check.
    QVERIFY(resultsMap.value(QStringLiteral("call_slow")).isError);

    // 2. MCP 模拟工具也捕获到了取消信号
    QVERIFY2(mcpTool->wasCanceled, "MCP tool was not cooperatively cancelled");
    QVERIFY(resultsMap.value(QStringLiteral("call_mcp")).isError);
}

class UncooperativeSlowTool final : public application::ports::ITool {
public:
    domain::agent::ToolDefinition definition() const override {
        domain::agent::ToolDefinition def;
        def.name = QStringLiteral("uncooperative_tool");
        return def;
    }
    application::ports::ToolExecutionTraits traits() const override {
        return {true, true, false, QString()};
    }
    std::unique_ptr<application::ports::IToolOperation> execute(const domain::agent::ToolCall& call, const application::ports::ToolExecutionContext& context) override {
        return std::make_unique<application::ports::ThreadedToolOperation>(
            call.id,
            [call]() {
                QThread::msleep(800);
                return domain::agent::ToolResult{call.id, QStringLiteral("不合作执行完毕"), false};
            },
            context.timeoutMs > 0 ? context.timeoutMs : 30000
        );
    }
};

void WorkIntegrationTests::testAgentRuntimeUncooperativeToolRuntimeDestructionSafety() {
    MockChatGateway mockGateway;

    // 下发不合作工具
    QList<domain::llm::ChatEvent> r1;
    r1.append(domain::llm::EventStarted{});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_uncoop"), QStringLiteral("uncooperative_tool")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_uncoop")});
    r1.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r1);

    auto uncoopTool = std::make_shared<UncooperativeSlowTool>();
    auto provider = std::make_shared<SimpleMockProvider>(QList<std::shared_ptr<application::ports::ITool>>{uncoopTool});

    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(provider);

    agent::runtime::AgentRunContext context;
    context.sessionId = QStringLiteral("sess_uncoop_test");
    context.workspaceRoot = m_tempDir.path();
    context.policy.timeoutMs = 100;
    context.policy.allowParallelToolExecution = true;

    // 使用堆分配，以便于随时 delete
    auto* runtime = new agent::runtime::AgentRuntime(&mockGateway, nullptr, &toolRegistry, nullptr);

    QEventLoop loop;
    connect(runtime, &agent::runtime::AgentRuntime::toolResultReady, [&](const QString&, const domain::agent::ToolResult&) {
        // 一旦超时结果就绪，立刻销毁 runtime！
        runtime->deleteLater();
        loop.quit();
    });

    runtime->startRun(context, QStringLiteral("执行不合作工具"));
    
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec();

    // 此时 runtime 已被销毁，主线程继续，但后台那个不合作线程仍在 msleep(800)
    // 我们等它结束，验证是否发生 UAF 崩溃
    QTest::qWait(1000); // 强行让主线程在事件循环里等 1 秒，如果后台悬挂 this->m_toolRegistry，此时就会 SegmentFault

    // 如果运行到这里没有崩溃，说明后台线程捕获的是 std::shared_ptr<ITool>，与 Runtime 解耦了，安全过关
    QVERIFY(true);
}

void WorkIntegrationTests::testAgentRuntimeImmediateToolCheckpointRecovery() {
    MockChatGateway mockGateway;

    // 模型返回 2 个工具调用
    QList<domain::llm::ChatEvent> r1;
    r1.append(domain::llm::EventStarted{});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_a"), QStringLiteral("write_file")});
    r1.append(domain::llm::EventToolCallDelta{QStringLiteral("call_a"), QStringLiteral(R"({"path":"file_a.txt","content":"data A"})")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_a")});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_b"), QStringLiteral("write_file")});
    r1.append(domain::llm::EventToolCallDelta{QStringLiteral("call_b"), QStringLiteral(R"({"path":"file_b.txt","content":"data B"})")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_b")});
    r1.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r1);

    // 恢复后的第 2 轮回答
    QList<domain::llm::ChatEvent> r2;
    r2.append(domain::llm::EventStarted{});
    r2.append(domain::llm::EventTextDelta{QStringLiteral("全部写入完成")});
    r2.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r2);

    auto workspaceFs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    auto builtinProvider = std::make_shared<agent::tool::BuiltinToolProvider>(workspaceFs);
    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(builtinProvider);

    data::repository::SqliteAgentCheckpointRepository cpRepo;

    agent::runtime::AgentRunContext context;
    context.sessionId = QStringLiteral("sess_crash_rec_test");
    context.workspaceRoot = m_tempDir.path();
    context.policy.autoApproveWriteWorkspace = true;
    context.policy.allowParallelToolExecution = false; // 串行执行便于拦截中途状态

    // 实例 1：执行第一个工具后立即模拟崩溃销毁
    {
        auto* runtime1 = new agent::runtime::AgentRuntime(&mockGateway, nullptr, &toolRegistry, &cpRepo);
        QEventLoop loop1;
        int toolResultsReceived = 0;
        connect(runtime1, &agent::runtime::AgentRuntime::toolResultReady, [&](const QString&, const domain::agent::ToolResult& res) {
            ++toolResultsReceived;
            if (res.toolCallId == QStringLiteral("call_a")) {
                // 第一个工具一执行完，立刻模拟崩溃销毁 runtime1
                runtime1->deleteLater();
                loop1.quit();
            }
        });

        runtime1->startRun(context, QStringLiteral("写两个文件"));
        QTimer::singleShot(2000, &loop1, &QEventLoop::quit);
        loop1.exec();

        QCOMPARE(toolResultsReceived, 1);
    }

    // 校验：此时 Checkpoint 已经即刻持久化了 call_a 的执行结果！
    auto cpOpt = cpRepo.getLatestCheckpoint(QStringLiteral("sess_crash_rec_test"));
    QVERIFY(cpOpt.has_value());
    QCOMPARE(cpOpt->pendingToolCalls.size(), 2);
    QCOMPARE(cpOpt->pendingToolResults.size(), 1);
    QCOMPARE(cpOpt->pendingToolResults.first().toolCallId, QStringLiteral("call_a"));

    // 实例 2：模拟应用重启恢复
    {
        agent::runtime::AgentRuntime runtime2(&mockGateway, nullptr, &toolRegistry, &cpRepo);
        bool completed = false;
        QList<QString> executedToolIds;
        QEventLoop loop2;

        connect(&runtime2, &agent::runtime::AgentRuntime::toolResultReady, [&](const QString&, const domain::agent::ToolResult& res) {
            executedToolIds.append(res.toolCallId);
        });
        connect(&runtime2, &agent::runtime::AgentRuntime::runCompleted, [&]() {
            completed = true;
            loop2.quit();
        });

        runtime2.resumeRun(context);
        QTimer::singleShot(3000, &loop2, &QEventLoop::quit);
        loop2.exec();

        QVERIFY2(completed, "Runtime2 should complete execution after resuming");
        // 恢复后只重新执行未完成的 call_b，已经完成并持久化的 call_a 绝对不重复执行！
        QCOMPARE(executedToolIds.size(), 1);
        QCOMPARE(executedToolIds.first(), QStringLiteral("call_b"));
    }

    // 最终两个文件都成功落盘
    QVERIFY(QFile::exists(QDir(m_tempDir.path()).filePath(QStringLiteral("file_a.txt"))));
    QVERIFY(QFile::exists(QDir(m_tempDir.path()).filePath(QStringLiteral("file_b.txt"))));
}

void WorkIntegrationTests::testAgentRuntimePermissionScopeRunApproval() {
    MockChatGateway mockGateway;

    // Round 1: 模型返回第 1 次 write_file
    QList<domain::llm::ChatEvent> r1;
    r1.append(domain::llm::EventStarted{});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_w1"), QStringLiteral("write_file")});
    r1.append(domain::llm::EventToolCallDelta{QStringLiteral("call_w1"), QStringLiteral(R"({"path":"run_mem_1.txt","content":"data 1"})")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_w1")});
    r1.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r1);

    // Round 2: 模型返回第 2 次 write_file
    QList<domain::llm::ChatEvent> r2;
    r2.append(domain::llm::EventStarted{});
    r2.append(domain::llm::EventToolCallStarted{QStringLiteral("call_w2"), QStringLiteral("write_file")});
    r2.append(domain::llm::EventToolCallDelta{QStringLiteral("call_w2"), QStringLiteral(R"({"path":"run_mem_2.txt","content":"data 2"})")});
    r2.append(domain::llm::EventToolCallFinished{QStringLiteral("call_w2")});
    r2.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r2);

    // Round 3: 最终回复
    QList<domain::llm::ChatEvent> r3;
    r3.append(domain::llm::EventStarted{});
    r3.append(domain::llm::EventTextDelta{QStringLiteral("两份文件全部写入完成")});
    r3.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r3);

    auto workspaceFs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    auto builtinProvider = std::make_shared<agent::tool::BuiltinToolProvider>(workspaceFs);
    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(builtinProvider);

    agent::runtime::AgentRuntime runtime(&mockGateway, nullptr, &toolRegistry, nullptr);

    agent::runtime::AgentRunContext context;
    context.sessionId = QStringLiteral("sess_perm_scope_test");
    context.workspaceRoot = m_tempDir.path();
    context.policy.autoApproveWriteWorkspace = false; // 默认要求询问权限

    int permissionRequests = 0;
    QString firstCallId;
    connect(&runtime, &agent::runtime::AgentRuntime::permissionRequested, [&](const QString&, const domain::agent::ToolCall& call, const domain::agent::ToolPermission&) {
        ++permissionRequests;
        if (firstCallId.isEmpty()) {
            firstCallId = call.id;
            // 第一次被询问时，选择以 PermissionScope::Run 记忆授权！
            QTimer::singleShot(10, [&runtime, call]() {
                runtime.grantPermission(QStringLiteral("sess_perm_scope_test"), call.id, true, domain::agent::PermissionScope::Run);
            });
        }
    });

    bool completed = false;
    QEventLoop loop;
    connect(&runtime, &agent::runtime::AgentRuntime::runCompleted, [&]() {
        completed = true;
        loop.quit();
    });

    runtime.startRun(context, QStringLiteral("写两个文件"));
    QTimer::singleShot(4000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY2(completed, "Runtime should complete execution");
    // 关键验证：由于第 1 次授权选择了 PermissionScope::Run，第 2 次相同工具调用被自动放行，权限请求总次数恰好为 1！
    QCOMPARE(permissionRequests, 1);
    QVERIFY(QFile::exists(QDir(m_tempDir.path()).filePath(QStringLiteral("run_mem_1.txt"))));
    QVERIFY(QFile::exists(QDir(m_tempDir.path()).filePath(QStringLiteral("run_mem_2.txt"))));
}

void WorkIntegrationTests::testAgentRuntimePermissionScopeProjectAndGlobalApproval() {
    MockChatGateway mockGateway;

    // Run 1 in Project A
    QList<domain::llm::ChatEvent> r1;
    r1.append(domain::llm::EventStarted{});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_p1"), QStringLiteral("write_file")});
    r1.append(domain::llm::EventToolCallDelta{QStringLiteral("call_p1"), QStringLiteral(R"({"path":"proj_a.txt","content":"proj A content"})")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_p1")});
    r1.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r1);

    QList<domain::llm::ChatEvent> r2;
    r2.append(domain::llm::EventStarted{});
    r2.append(domain::llm::EventTextDelta{QStringLiteral("完成写入 A")});
    r2.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r2);

    // Run 2 in Project A (different session, same project)
    QList<domain::llm::ChatEvent> r3;
    r3.append(domain::llm::EventStarted{});
    r3.append(domain::llm::EventToolCallStarted{QStringLiteral("call_p2"), QStringLiteral("write_file")});
    r3.append(domain::llm::EventToolCallDelta{QStringLiteral("call_p2"), QStringLiteral(R"({"path":"proj_a2.txt","content":"proj A2 content"})")});
    r3.append(domain::llm::EventToolCallFinished{QStringLiteral("call_p2")});
    r3.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r3);

    QList<domain::llm::ChatEvent> r4;
    r4.append(domain::llm::EventStarted{});
    r4.append(domain::llm::EventTextDelta{QStringLiteral("完成写入 A2")});
    r4.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r4);

    auto workspaceFs = std::make_shared<llm::workspace::WorkspaceFileSystem>();
    auto builtinProvider = std::make_shared<agent::tool::BuiltinToolProvider>(workspaceFs);
    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(builtinProvider);

    agent::runtime::AgentRuntime runtime(&mockGateway, nullptr, &toolRegistry, nullptr);

    const QUuid projectAId = QUuid::createUuid();

    agent::runtime::AgentRunContext ctx1;
    ctx1.sessionId = QStringLiteral("sess_proj_1");
    ctx1.projectId = projectAId;
    ctx1.workspaceRoot = m_tempDir.path();
    ctx1.policy.autoApproveWriteWorkspace = false; // 要求授权

    int permissionRequests = 0;
    connect(&runtime, &agent::runtime::AgentRuntime::permissionRequested, [&](const QString&, const domain::agent::ToolCall& call, const domain::agent::ToolPermission&) {
        ++permissionRequests;
        // 以 PermissionScope::Project 授权
        QTimer::singleShot(10, [&runtime, call]() {
            runtime.grantPermission(QStringLiteral("sess_proj_1"), call.id, true, domain::agent::PermissionScope::Project);
        });
    });

    bool completed1 = false;
    QEventLoop loop1;
    connect(&runtime, &agent::runtime::AgentRuntime::runCompleted, [&]() {
        completed1 = true;
        loop1.quit();
    });

    runtime.startRun(ctx1, QStringLiteral("项目 A 第 1 次运行"));
    QTimer::singleShot(3000, &loop1, &QEventLoop::quit);
    loop1.exec();

    QVERIFY(completed1);
    QCOMPARE(permissionRequests, 1);

    // 在同一个项目 projectAId 发起新的 Session 运行
    agent::runtime::AgentRunContext ctx2;
    ctx2.sessionId = QStringLiteral("sess_proj_2");
    ctx2.projectId = projectAId;
    ctx2.workspaceRoot = m_tempDir.path();
    ctx2.policy.autoApproveWriteWorkspace = false;

    bool completed2 = false;
    QEventLoop loop2;
    connect(&runtime, &agent::runtime::AgentRuntime::runCompleted, [&]() {
        completed2 = true;
        loop2.quit();
    });

    runtime.startRun(ctx2, QStringLiteral("项目 A 第 2 次运行（新会话）"));
    QTimer::singleShot(3000, &loop2, &QEventLoop::quit);
    loop2.exec();

    QVERIFY(completed2);
    // 关键验证：由于以 PermissionScope::Project 授权，同一个项目的新会话无需再次弹窗，请求总次数仍为 1！
    QCOMPARE(permissionRequests, 1);
}

// 模拟返回超大文本的 Tool
class HugeOutputTool final : public application::ports::ITool {
public:
    domain::agent::ToolDefinition definition() const override {
        domain::agent::ToolDefinition def;
        def.name = QStringLiteral("get_huge_log");
        def.description = QStringLiteral("返回超大文本日志");
        return def;
    }
    application::ports::ToolExecutionTraits traits() const override {
        return {true, true, true, QString()};
    }
    QList<domain::agent::ToolPermission> permissions() const override {
        return {};
    }
    std::unique_ptr<application::ports::IToolOperation> execute(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext&
    ) override {
        return std::make_unique<application::ports::ImmediateToolOperation>(
            call.id,
            [call]() -> domain::agent::ToolResult {
                return {call.id, QString(50000, QLatin1Char('A')), false};
            }
        );
    }
};

void WorkIntegrationTests::testAgentRuntimeToolOutputTruncation() {
    MockChatGateway mockGateway;

    QList<domain::llm::ChatEvent> r1;
    r1.append(domain::llm::EventStarted{});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_huge"), QStringLiteral("get_huge_log")});
    r1.append(domain::llm::EventToolCallDelta{QStringLiteral("call_huge"), QStringLiteral("{}")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_huge")});
    r1.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r1);

    QList<domain::llm::ChatEvent> r2;
    r2.append(domain::llm::EventStarted{});
    r2.append(domain::llm::EventTextDelta{QStringLiteral("完成")});
    r2.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r2);

    auto hugeTool = std::make_shared<HugeOutputTool>();
    auto provider = std::make_shared<SimpleMockProvider>(QList<std::shared_ptr<application::ports::ITool>>{hugeTool});
    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(provider);

    agent::runtime::AgentRuntime runtime(&mockGateway, nullptr, &toolRegistry, nullptr);

    agent::runtime::AgentRunContext context;
    context.sessionId = QStringLiteral("sess_truncation_test");
    context.workspaceRoot = m_tempDir.path();
    context.policy.maxToolOutputChars = 500; // 限制为 500 字符

    domain::agent::ToolResult receivedResult;
    connect(&runtime, &agent::runtime::AgentRuntime::toolResultReady, [&](const QString&, const domain::agent::ToolResult& res) {
        receivedResult = res;
    });

    bool completed = false;
    QEventLoop loop;
    connect(&runtime, &agent::runtime::AgentRuntime::runCompleted, [&]() {
        completed = true;
        loop.quit();
    });

    runtime.startRun(context, QStringLiteral("获取大日志"));
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY2(completed, "Runtime should complete");
    // 验证截断生效：输出长度约为 500 + 截断通知文本长度，远小于 50000
    QVERIFY(receivedResult.content.length() < 1000);
    QVERIFY(receivedResult.content.contains(QStringLiteral("工具输出已截断")));
    QVERIFY(receivedResult.content.contains(QStringLiteral("50000")));
}

// 模拟不同延迟的并发 Tool
class VariableDelayTool final : public application::ports::ITool {
public:
    explicit VariableDelayTool(const QString& name, int delayMs)
        : m_name(name), m_delayMs(delayMs) {}

    domain::agent::ToolDefinition definition() const override {
        domain::agent::ToolDefinition def;
        def.name = m_name;
        return def;
    }
    application::ports::ToolExecutionTraits traits() const override {
        return {true, true, true, QString()};
    }
    QList<domain::agent::ToolPermission> permissions() const override {
        return {};
    }
    std::unique_ptr<application::ports::IToolOperation> execute(
        const domain::agent::ToolCall& call,
        const application::ports::ToolExecutionContext&
    ) override {
        const QString name = m_name;
        const int delay = m_delayMs;
        return std::make_unique<application::ports::ThreadedToolOperation>(
            call.id,
            [call, name, delay]() -> domain::agent::ToolResult {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                return {call.id, QStringLiteral("result_from_%1").arg(name), false};
            }
        );
    }

private:
    QString m_name;
    int m_delayMs = 0;
};

void WorkIntegrationTests::testAgentRuntimeParallelResultOrderPreservation() {
    MockChatGateway mockGateway;

    // 模型下发顺序：call_1 (慢 80ms), call_2 (快 10ms), call_3 (中 40ms)
    QList<domain::llm::ChatEvent> r1;
    r1.append(domain::llm::EventStarted{});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_1"), QStringLiteral("tool_slow")});
    r1.append(domain::llm::EventToolCallDelta{QStringLiteral("call_1"), QStringLiteral("{}")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_1")});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_2"), QStringLiteral("tool_fast")});
    r1.append(domain::llm::EventToolCallDelta{QStringLiteral("call_2"), QStringLiteral("{}")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_2")});
    r1.append(domain::llm::EventToolCallStarted{QStringLiteral("call_3"), QStringLiteral("tool_mid")});
    r1.append(domain::llm::EventToolCallDelta{QStringLiteral("call_3"), QStringLiteral("{}")});
    r1.append(domain::llm::EventToolCallFinished{QStringLiteral("call_3")});
    r1.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r1);

    QList<domain::llm::ChatEvent> r2;
    r2.append(domain::llm::EventStarted{});
    r2.append(domain::llm::EventTextDelta{QStringLiteral("全部完成")});
    r2.append(domain::llm::EventFinished{});
    mockGateway.enqueueResponse(r2);

    auto slowTool = std::make_shared<VariableDelayTool>(QStringLiteral("tool_slow"), 80);
    auto fastTool = std::make_shared<VariableDelayTool>(QStringLiteral("tool_fast"), 10);
    auto midTool = std::make_shared<VariableDelayTool>(QStringLiteral("tool_mid"), 40);

    auto provider = std::make_shared<SimpleMockProvider>(QList<std::shared_ptr<application::ports::ITool>>{slowTool, fastTool, midTool});
    agent::tool::ToolRegistry toolRegistry;
    toolRegistry.registerProvider(provider);

    auto convRepo = std::make_unique<data::repository::SqliteConversationRepository>();
    auto transcriptRepo = std::make_unique<data::repository::JsonlMessageRepository>(m_tempDir.path());
    services::conversation::ConversationService convService(convRepo.get(), transcriptRepo.get());

    agent::runtime::AgentRuntime runtime(&mockGateway, &convService, &toolRegistry, nullptr);

    agent::runtime::AgentRunContext context;
    context.sessionId = QStringLiteral("sess_order_test");
    context.workspaceRoot = m_tempDir.path();
    context.policy.allowParallelToolExecution = true;

    QList<QString> arrivalOrder;
    connect(&runtime, &agent::runtime::AgentRuntime::toolResultReady, [&](const QString&, const domain::agent::ToolResult& res) {
        arrivalOrder.append(res.toolCallId);
    });

    bool completed = false;
    QEventLoop loop;
    connect(&runtime, &agent::runtime::AgentRuntime::runCompleted, [&]() {
        completed = true;
        loop.quit();
    });

    runtime.startRun(context, QStringLiteral("乱序并发测试"));
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY2(completed, "Runtime should complete");

    // 实际到达时间是乱序的 (fast -> mid -> slow)
    QCOMPARE(arrivalOrder.size(), 3);
    QCOMPARE(arrivalOrder[0], QStringLiteral("call_2"));
    QCOMPARE(arrivalOrder[1], QStringLiteral("call_3"));
    QCOMPARE(arrivalOrder[2], QStringLiteral("call_1"));

    // 但是最终入库的 ToolResultBlock 必须严格按照模型下发顺序保序为 [call_1, call_2, call_3]！
    const auto history = convService.loadMessages(QStringLiteral("sess_order_test"));
    domain::conversation::ToolResultBlock toolBlock;
    for (const auto& msg : history) {
        if (msg.role == domain::MessageRole::Tool) {
            for (const auto& blk : msg.blocks) {
                if (blk.isToolResult()) {
                    toolBlock = std::get<domain::conversation::ToolResultBlock>(blk.payload);
                }
            }
        }
    }
    QCOMPARE(toolBlock.results.size(), 3);
    QCOMPARE(toolBlock.results[0].toolCallId, QStringLiteral("call_1"));
    QCOMPARE(toolBlock.results[1].toolCallId, QStringLiteral("call_2"));
    QCOMPARE(toolBlock.results[2].toolCallId, QStringLiteral("call_3"));
}

QTEST_GUILESS_MAIN(WorkIntegrationTests)
#include "WorkIntegrationTests.moc"
