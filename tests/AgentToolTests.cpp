#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "services/agent/AgentToolService.h"
#include "services/project/ProjectContextService.h"

class AgentToolTests final : public QObject {
    Q_OBJECT
private slots:
    void rejectsPathsOutsideWorkspace();
    void loadsProjectContext();
};
void AgentToolTests::rejectsPathsOutsideWorkspace() {
    QTemporaryDir root; QVERIFY(root.isValid());
    QFile file(QDir(root.path()).filePath("note.txt")); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("ok"); file.close();
    services::agent::AgentToolService tools(root.path());
    QCOMPARE(tools.execute({"read", "read_file", R"({"path":"note.txt"})"}).content, QStringLiteral("ok"));
    QVERIFY(tools.execute({"bad", "read_file", R"({"path":"../bad.txt"})"}).isError);
    const auto written = tools.execute({"write", "write_file", R"({"path":"created.txt","content":"new"})"});
    QVERIFY(!written.isError);
    QCOMPARE(tools.execute({"read-created", "read_file", R"({"path":"created.txt"})"}).content, QStringLiteral("new"));
}
void AgentToolTests::loadsProjectContext() {
    QTemporaryDir root; QVERIFY(root.isValid()); QDir dir(root.path()); QVERIFY(dir.mkpath(".agents/skills/demo"));
    QFile agents(dir.filePath("AGENTS.md")); QVERIFY(agents.open(QIODevice::WriteOnly)); agents.write("rules"); agents.close();
    QFile skill(dir.filePath(".agents/skills/demo/SKILL.md")); QVERIFY(skill.open(QIODevice::WriteOnly)); skill.write("skill"); skill.close();
    QFile mcp(dir.filePath(".mcp.json")); QVERIFY(mcp.open(QIODevice::WriteOnly)); mcp.write("{\"mcpServers\":{\"demo\":{}}}"); mcp.close();
    auto context = services::project::ProjectContextService{}.load(root.path());
    QCOMPARE(context.agentsInstructions, QStringLiteral("rules")); QCOMPARE(context.skills.size(), 1); QVERIFY(!context.mcpConfigPath.isEmpty());
    QCOMPARE(context.mcpConfigContent, QStringLiteral("{\"mcpServers\":{\"demo\":{}}}"));
}
QTEST_APPLESS_MAIN(AgentToolTests)
#include "AgentToolTests.moc"
