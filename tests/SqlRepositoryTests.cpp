#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <QDateTime>
#include <QUuid>

#include "data/sqlite/SqlHelper.h"
#include "data/sqlite/DatabaseManager.h"
#include "data/repository/SqliteProjectRepository.h"
#include "data/repository/SqliteConversationRepository.h"
#include "data/repository/SqliteAgentRepository.h"
#include "data/repository/SqliteAgentCheckpointRepository.h"

class SqlRepositoryTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();

    // SqlHelper 执行与基础功能测试
    void testSqlHelperExecute_DdlAndDml_Succeeds();
    void testSqlHelperExecute_NamedParameters_BindsCorrectly();
    void testSqlHelperExecuteBatch_MultipleRows_InsertsAll();
    void testSqlHelperScalar_TypeConversions_ReturnsValues();
    void testSqlHelperQueryOne_FoundAndNotFound_ReturnsOptional();
    void testSqlHelperQueryAll_MultipleRows_ReturnsMappedList();

    // SqlHelper Schema 元数据自省测试
    void testSqlHelperSchema_TableAndColumnExists_DetectsAccurately();
    void testSqlHelperSchema_ColumnsListAndIndexExists_ReturnsMetadata();

    // SqlHelper 错误模型捕获测试
    void testSqlHelperErrorModel_SyntaxError_CapturesContext();
    void testSqlHelperErrorModel_ClosedDatabase_ReportsConnectionError();

    // 仓储层现代化 CRUD 测试
    void testSqliteProjectRepository_CrudAndLookup_Succeeds();
    void testSqliteConversationRepository_CrudAndOrdering_Succeeds();
    void testSqliteAgentRepository_CrudAndSerialization_Succeeds();
    void testSqliteAgentCheckpointRepository_CrudAndLatestQuery_Succeeds();

private:
    QTemporaryDir m_tempDir;
};

void SqlRepositoryTests::initTestCase() {
    QVERIFY(m_tempDir.isValid());
    const QString dbPath = m_tempDir.filePath(QStringLiteral("test_sql_repo.db"));
    QVERIFY(data::sqlite::DatabaseManager::instance().initialize(dbPath));
}

void SqlRepositoryTests::cleanupTestCase() {
    data::sqlite::DatabaseManager::instance().close();
}

void SqlRepositoryTests::testSqlHelperExecute_DdlAndDml_Succeeds() {
    auto db = data::sqlite::DatabaseManager::instance().database();
    QVERIFY(db.isOpen());

    const QString ddl = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS test_sql_ddl ("
        "  id TEXT PRIMARY KEY,"
        "  title TEXT NOT NULL,"
        "  val INTEGER DEFAULT 0"
        ");"
    );
    auto res = data::sqlite::SqlHelper::execute(ddl, db);
    QVERIFY(res.success);
    QVERIFY(static_cast<bool>(res));

    // 清空与插入
    data::sqlite::SqlHelper::execute(QStringLiteral("DELETE FROM test_sql_ddl;"), db);
    res = data::sqlite::SqlHelper::execute(
        QStringLiteral("INSERT INTO test_sql_ddl (id, title, val) VALUES (?, ?, ?);"),
        {QStringLiteral("row1"), QStringLiteral("Test Row"), 42},
        db
    );
    QVERIFY(res.success);
    QCOMPARE(res.affectedRows, 1);

    // 更新
    res = data::sqlite::SqlHelper::execute(
        QStringLiteral("UPDATE test_sql_ddl SET val = ? WHERE id = ?;"),
        {100, QStringLiteral("row1")},
        db
    );
    QVERIFY(res.success);
    QCOMPARE(res.affectedRows, 1);

    // 删除
    res = data::sqlite::SqlHelper::execute(
        QStringLiteral("DELETE FROM test_sql_ddl WHERE id = ?;"),
        {QStringLiteral("row1")},
        db
    );
    QVERIFY(res.success);
    QCOMPARE(res.affectedRows, 1);
}

void SqlRepositoryTests::testSqlHelperExecute_NamedParameters_BindsCorrectly() {
    auto db = data::sqlite::DatabaseManager::instance().database();
    QVERIFY(db.isOpen());

    const QString ddl = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS test_named_bind ("
        "  id TEXT PRIMARY KEY,"
        "  username TEXT NOT NULL,"
        "  age INTEGER"
        ");"
    );
    data::sqlite::SqlHelper::execute(ddl, db);
    data::sqlite::SqlHelper::execute(QStringLiteral("DELETE FROM test_named_bind;"), db);

    const auto res = data::sqlite::SqlHelper::execute(
        QStringLiteral("INSERT INTO test_named_bind (id, username, age) VALUES (:id, :user, :age);"),
        {
            {QStringLiteral(":id"), QStringLiteral("u_named_1")},
            {QStringLiteral(":user"), QStringLiteral("Bob")},
            {QStringLiteral(":age"), 28}
        },
        db
    );
    QVERIFY(res.success);
    QCOMPARE(res.affectedRows, 1);

    const QString foundUser = data::sqlite::SqlHelper::scalarAs<QString>(
        QStringLiteral("SELECT username FROM test_named_bind WHERE id = ?;"),
        {QStringLiteral("u_named_1")},
        db
    );
    QCOMPARE(foundUser, QStringLiteral("Bob"));
}

void SqlRepositoryTests::testSqlHelperExecuteBatch_MultipleRows_InsertsAll() {
    auto db = data::sqlite::DatabaseManager::instance().database();
    QVERIFY(db.isOpen());

    const QString ddl = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS test_batch_table ("
        "  id TEXT PRIMARY KEY,"
        "  val REAL"
        ");"
    );
    data::sqlite::SqlHelper::execute(ddl, db);
    data::sqlite::SqlHelper::execute(QStringLiteral("DELETE FROM test_batch_table;"), db);

    QList<QVariantList> rows = {
        {QStringLiteral("b1"), 1.1},
        {QStringLiteral("b2"), 2.2},
        {QStringLiteral("b3"), 3.3}
    };

    const auto res = data::sqlite::SqlHelper::executeBatch(
        QStringLiteral("INSERT INTO test_batch_table (id, val) VALUES (?, ?);"),
        rows,
        db
    );
    QVERIFY(res.success);
    QCOMPARE(res.affectedRows, 3);

    const int total = data::sqlite::SqlHelper::scalarAs<int>(
        QStringLiteral("SELECT COUNT(*) FROM test_batch_table;"), db, 0
    );
    QCOMPARE(total, 3);
}

void SqlRepositoryTests::testSqlHelperScalar_TypeConversions_ReturnsValues() {
    auto db = data::sqlite::DatabaseManager::instance().database();
    QVERIFY(db.isOpen());

    const QString ddl = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS test_scalar ("
        "  id TEXT PRIMARY KEY,"
        "  int_val INTEGER,"
        "  big_val INTEGER,"
        "  str_val TEXT,"
        "  bool_val INTEGER"
        ");"
    );
    data::sqlite::SqlHelper::execute(ddl, db);
    data::sqlite::SqlHelper::execute(QStringLiteral("DELETE FROM test_scalar;"), db);

    data::sqlite::SqlHelper::execute(
        QStringLiteral("INSERT INTO test_scalar VALUES (?, ?, ?, ?, ?);"),
        {QStringLiteral("s1"), 42, static_cast<qint64>(9876543210123LL), QStringLiteral("hello scalar"), 1},
        db
    );

    QCOMPARE(data::sqlite::SqlHelper::scalarAs<int>(
        QStringLiteral("SELECT int_val FROM test_scalar WHERE id = ?;"), {QStringLiteral("s1")}, db), 42);
    QCOMPARE(data::sqlite::SqlHelper::scalarAs<qint64>(
        QStringLiteral("SELECT big_val FROM test_scalar WHERE id = ?;"), {QStringLiteral("s1")}, db), 9876543210123LL);
    QCOMPARE(data::sqlite::SqlHelper::scalarAs<QString>(
        QStringLiteral("SELECT str_val FROM test_scalar WHERE id = ?;"), {QStringLiteral("s1")}, db), QStringLiteral("hello scalar"));
    QCOMPARE(data::sqlite::SqlHelper::scalarAs<bool>(
        QStringLiteral("SELECT bool_val FROM test_scalar WHERE id = ?;"), {QStringLiteral("s1")}, db), true);

    // 默认值回退
    QCOMPARE(data::sqlite::SqlHelper::scalarAs<int>(
        QStringLiteral("SELECT int_val FROM test_scalar WHERE id = ?;"), {QStringLiteral("missing")}, db, 999), 999);
}

void SqlRepositoryTests::testSqlHelperQueryOne_FoundAndNotFound_ReturnsOptional() {
    auto db = data::sqlite::DatabaseManager::instance().database();
    QVERIFY(db.isOpen());

    const QString ddl = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS test_query_mapping ("
        "  id TEXT PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  order_num INTEGER"
        ");"
    );
    data::sqlite::SqlHelper::execute(ddl, db);
    data::sqlite::SqlHelper::execute(QStringLiteral("DELETE FROM test_query_mapping;"), db);

    data::sqlite::SqlHelper::executeBatch(
        QStringLiteral("INSERT INTO test_query_mapping VALUES (?, ?, ?);"),
        {
            {QStringLiteral("m1"), QStringLiteral("Alpha"), 10},
            {QStringLiteral("m2"), QStringLiteral("Beta"), 20},
            {QStringLiteral("m3"), QStringLiteral("Gamma"), 30}
        },
        db
    );

    struct Item { QString id; QString name; int order; };
    auto mapper = [](const QSqlQuery& q) {
        return Item{ q.value(0).toString(), q.value(1).toString(), q.value(2).toInt() };
    };

    // 查存在
    auto oneFound = data::sqlite::SqlHelper::queryOne<Item>(
        QStringLiteral("SELECT id, name, order_num FROM test_query_mapping WHERE id = ?;"),
        {QStringLiteral("m2")}, db, mapper
    );
    QVERIFY(oneFound.has_value());
    QCOMPARE(oneFound->name, QStringLiteral("Beta"));
    QCOMPARE(oneFound->order, 20);

    // 查不存在
    auto oneMissing = data::sqlite::SqlHelper::queryOne<Item>(
        QStringLiteral("SELECT id, name, order_num FROM test_query_mapping WHERE id = ?;"),
        {QStringLiteral("missing")}, db, mapper
    );
    QVERIFY(!oneMissing.has_value());
}

void SqlRepositoryTests::testSqlHelperQueryAll_MultipleRows_ReturnsMappedList() {
    auto db = data::sqlite::DatabaseManager::instance().database();
    QVERIFY(db.isOpen());

    const QString ddl = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS test_query_all_table ("
        "  id TEXT PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  order_num INTEGER"
        ");"
    );
    data::sqlite::SqlHelper::execute(ddl, db);
    data::sqlite::SqlHelper::execute(QStringLiteral("DELETE FROM test_query_all_table;"), db);

    data::sqlite::SqlHelper::executeBatch(
        QStringLiteral("INSERT INTO test_query_all_table VALUES (?, ?, ?);"),
        {
            {QStringLiteral("m1"), QStringLiteral("Alpha"), 10},
            {QStringLiteral("m2"), QStringLiteral("Beta"), 20},
            {QStringLiteral("m3"), QStringLiteral("Gamma"), 30}
        },
        db
    );

    struct Item { QString id; QString name; int order; };
    auto mapper = [](const QSqlQuery& q) {
        return Item{ q.value(0).toString(), q.value(1).toString(), q.value(2).toInt() };
    };

    auto allItems = data::sqlite::SqlHelper::queryAll<Item>(
        QStringLiteral("SELECT id, name, order_num FROM test_query_all_table ORDER BY order_num ASC;"),
        db, mapper
    );
    QCOMPARE(allItems.size(), 3);
    QCOMPARE(allItems[0].name, QStringLiteral("Alpha"));
    QCOMPARE(allItems[1].name, QStringLiteral("Beta"));
    QCOMPARE(allItems[2].name, QStringLiteral("Gamma"));
}

void SqlRepositoryTests::testSqlHelperSchema_TableAndColumnExists_DetectsAccurately() {
    auto db = data::sqlite::DatabaseManager::instance().database();
    QVERIFY(db.isOpen());

    const QString ddl = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS test_schema_exist ("
        "  col_alpha TEXT PRIMARY KEY,"
        "  col_beta INTEGER"
        ");"
    );
    data::sqlite::SqlHelper::execute(ddl, db);

    // tableExists
    QVERIFY(data::sqlite::SqlHelper::tableExists(QStringLiteral("test_schema_exist"), db));
    QVERIFY(!data::sqlite::SqlHelper::tableExists(QStringLiteral("no_such_table"), db));

    // columnExists (不区分大小写)
    QVERIFY(data::sqlite::SqlHelper::columnExists(QStringLiteral("test_schema_exist"), QStringLiteral("col_alpha"), db));
    QVERIFY(data::sqlite::SqlHelper::columnExists(QStringLiteral("test_schema_exist"), QStringLiteral("COL_BETA"), db));
    QVERIFY(!data::sqlite::SqlHelper::columnExists(QStringLiteral("test_schema_exist"), QStringLiteral("missing_col"), db));
}

void SqlRepositoryTests::testSqlHelperSchema_ColumnsListAndIndexExists_ReturnsMetadata() {
    auto db = data::sqlite::DatabaseManager::instance().database();
    QVERIFY(db.isOpen());

    const QString ddl = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS test_schema_meta ("
        "  c1 TEXT PRIMARY KEY,"
        "  c2 INTEGER,"
        "  c3 REAL"
        ");"
    );
    data::sqlite::SqlHelper::execute(ddl, db);
    data::sqlite::SqlHelper::execute(
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_test_c2 ON test_schema_meta(c2);"), db
    );

    // columns list
    const auto cols = data::sqlite::SqlHelper::columns(QStringLiteral("test_schema_meta"), db);
    QCOMPARE(cols.size(), 3);
    QVERIFY(cols.contains(QStringLiteral("c1")));
    QVERIFY(cols.contains(QStringLiteral("c2")));
    QVERIFY(cols.contains(QStringLiteral("c3")));

    // indexExists
    QVERIFY(data::sqlite::SqlHelper::indexExists(QStringLiteral("idx_test_c2"), db));
    QVERIFY(!data::sqlite::SqlHelper::indexExists(QStringLiteral("idx_missing"), db));
}

void SqlRepositoryTests::testSqlHelperErrorModel_SyntaxError_CapturesContext() {
    auto db = data::sqlite::DatabaseManager::instance().database();
    QVERIFY(db.isOpen());

    auto errRes = data::sqlite::SqlHelper::execute(QStringLiteral("INSERT INTO non_existent_table SYNTAX ERROR"), db);
    QVERIFY(!errRes.success);
    QVERIFY(!static_cast<bool>(errRes));
    QVERIFY(errRes.error.isValid());
    QVERIFY(!errRes.error.message.isEmpty());
}

void SqlRepositoryTests::testSqlHelperErrorModel_ClosedDatabase_ReportsConnectionError() {
    QSqlDatabase closedDb;
    auto closedRes = data::sqlite::SqlHelper::execute(QStringLiteral("SELECT 1;"), closedDb);
    QVERIFY(!closedRes.success);
    QCOMPARE(closedRes.error.type, QSqlError::ConnectionError);
}

void SqlRepositoryTests::testSqliteProjectRepository_CrudAndLookup_Succeeds() {
    data::repository::SqliteProjectRepository projectRepo;
    QVERIFY(projectRepo.initializeDatabase());

    domain::project::Project p;
    p.id = QUuid::createUuid();
    p.name = QStringLiteral("ForgeAI Project");
    p.rootPath = m_tempDir.filePath(QStringLiteral("forgeai_root"));
    p.createdAt = QDateTime::currentDateTime();
    p.lastOpenedAt = QDateTime::currentDateTime();
    p.isPinned = true;

    projectRepo.saveProject(p);

    auto retrieved = projectRepo.getProject(p.id);
    QVERIFY(retrieved.has_value());
    QCOMPARE(retrieved->name, p.name);
    QCOMPARE(retrieved->rootPath, p.rootPath);
    QCOMPARE(retrieved->isPinned, true);

    auto retrievedByPath = projectRepo.getProjectByPath(p.rootPath);
    QVERIFY(retrievedByPath.has_value());
    QCOMPARE(retrievedByPath->id, p.id);

    auto allProjects = projectRepo.getAllProjects();
    QVERIFY(!allProjects.isEmpty());

    projectRepo.deleteProject(p.id);
    QVERIFY(!projectRepo.getProject(p.id).has_value());
}

void SqlRepositoryTests::testSqliteConversationRepository_CrudAndOrdering_Succeeds() {
    data::repository::SqliteConversationRepository convRepo;
    QVERIFY(convRepo.initializeDatabase());

    domain::conversation::Conversation conv;
    conv.id = QUuid::createUuid();
    conv.title = QStringLiteral("Architectural Discussion");
    conv.isPinned = true;
    conv.isArchived = false;
    conv.createdAt = QDateTime::currentDateTime();
    conv.updatedAt = QDateTime::currentDateTime();

    convRepo.saveConversation(conv);

    auto retrieved = convRepo.getConversation(conv.id);
    QVERIFY(retrieved.has_value());
    QCOMPARE(retrieved->title, conv.title);
    QCOMPARE(retrieved->isPinned, true);

    auto allConvs = convRepo.getAllConversations();
    QVERIFY(!allConvs.isEmpty());

    convRepo.deleteConversation(conv.id);
    QVERIFY(!convRepo.getConversation(conv.id).has_value());
}

void SqlRepositoryTests::testSqliteAgentRepository_CrudAndSerialization_Succeeds() {
    data::repository::SqliteAgentRepository agentRepo;
    QVERIFY(agentRepo.initializeDatabase());

    domain::agent::Agent agent;
    agent.id = QUuid::createUuid();
    agent.name = QStringLiteral("Code Assistant");
    agent.description = QStringLiteral("Specialized in C++ Qt");
    agent.avatar = QStringLiteral("avatar.png");
    agent.systemPrompt = QStringLiteral("You are an expert.");
    agent.modelId = QStringLiteral("gpt-4o");
    agent.providerId = QStringLiteral("openai");
    agent.enabledTools = { QStringLiteral("read_file"), QStringLiteral("write_file") };
    agent.enabledSkills = { QStringLiteral("git") };
    agent.enabledMcpServerIds = { QStringLiteral("mcp-1") };
    agent.createdAt = QDateTime::currentDateTime();
    agent.updatedAt = QDateTime::currentDateTime();

    QVERIFY(agentRepo.saveAgent(agent));

    auto retrieved = agentRepo.getAgent(agent.id);
    QVERIFY(retrieved.has_value());
    QCOMPARE(retrieved->name, agent.name);
    QCOMPARE(retrieved->enabledTools.size(), 2);
    QCOMPARE(retrieved->enabledTools[0], QStringLiteral("read_file"));
    QCOMPARE(retrieved->enabledSkills.size(), 1);
    QCOMPARE(retrieved->enabledMcpServerIds.size(), 1);

    auto allAgents = agentRepo.getAllAgents();
    QVERIFY(!allAgents.isEmpty());

    QVERIFY(agentRepo.deleteAgent(agent.id));
    QVERIFY(!agentRepo.getAgent(agent.id).has_value());
}

void SqlRepositoryTests::testSqliteAgentCheckpointRepository_CrudAndLatestQuery_Succeeds() {
    data::repository::SqliteAgentCheckpointRepository cpRepo;
    QVERIFY(cpRepo.initializeDatabase());

    domain::agent::AgentCheckpoint cp;
    cp.checkpointId = QUuid::createUuid();
    cp.sessionId = QStringLiteral("sess_test_cp");
    cp.runId = QUuid::createUuid();
    cp.roundIndex = 1;
    cp.status = domain::agent::AgentRunStatus::ExecutingTool;
    cp.pendingToolCalls = { {QStringLiteral("c1"), QStringLiteral("write_file"), QStringLiteral("{\"path\":\"test.txt\"}"), {}} };
    cp.pendingToolResults = { {QStringLiteral("c1"), QStringLiteral("saved"), false} };
    cp.timestamp = QDateTime::currentDateTime();

    QVERIFY(cpRepo.saveCheckpoint(cp));

    auto retrieved = cpRepo.getLatestCheckpoint(QStringLiteral("sess_test_cp"));
    QVERIFY(retrieved.has_value());
    QCOMPARE(retrieved->sessionId, QStringLiteral("sess_test_cp"));
    QCOMPARE(retrieved->roundIndex, 1);
    QCOMPARE(retrieved->pendingToolCalls.size(), 1);
    QCOMPARE(retrieved->pendingToolCalls[0].name, QStringLiteral("write_file"));
    QCOMPARE(retrieved->pendingToolResults.size(), 1);

    QVERIFY(cpRepo.deleteCheckpointsForSession(QStringLiteral("sess_test_cp")));
    QVERIFY(!cpRepo.getLatestCheckpoint(QStringLiteral("sess_test_cp")).has_value());
}

QTEST_GUILESS_MAIN(SqlRepositoryTests)
#include "SqlRepositoryTests.moc"
