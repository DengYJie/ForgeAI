#include "StartTaskUseCase.h"
#include "domain/service/IAgentToolService.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUuid>

namespace application::usecase::work {
    StartTaskUseCase::StartTaskUseCase(domain::service::IAgentToolService* tools, QObject *parent)
        : QObject(parent), m_tools(tools) {
    }

    void StartTaskUseCase::execute(const QString &task) {
        if (task.trimmed().isEmpty()) return;
        emit taskStarted(task);
        if (!m_tools) { emit taskFailed(task, QStringLiteral("工具服务未就绪")); return; }
        const QStringList parts = task.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        const QString command = parts.value(0).toLower();
        domain::agent::ToolCall call;
        call.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        call.name = command == QStringLiteral("ls") ? QStringLiteral("list_files")
            : command == QStringLiteral("read") ? QStringLiteral("read_file") : command;
        QJsonObject arguments{{"path", parts.value(command == QStringLiteral("search") ? 2 : 1, QStringLiteral("."))}};
        if (command == QStringLiteral("search")) { call.name = QStringLiteral("search_text"); arguments.insert("query", parts.value(1)); }
        call.arguments = QString::fromUtf8(QJsonDocument(arguments).toJson(QJsonDocument::Compact));
        const auto result = m_tools->execute(call);
        emit toolFinished(call, result);
        if (result.isError) { emit taskFailed(task, result.content); return; }
        emit taskCompleted(task);
    }
} // namespace application::usecase::work
