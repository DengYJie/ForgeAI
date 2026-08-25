#include "AgentToolService.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDirIterator>

namespace services::agent {
AgentToolService::AgentToolService(QString workspaceRoot, QObject* parent)
    : IAgentToolService(parent) { setWorkspaceRoot(workspaceRoot); }

void AgentToolService::setWorkspaceRoot(const QString& workspaceRoot) {
    m_workspaceRoot = QDir(workspaceRoot).canonicalPath();
}

QList<domain::agent::ToolDefinition> AgentToolService::definitions() const {
    return {
        {QStringLiteral("list_files"), QStringLiteral("列出工作区内指定目录的直接内容。路径相对于项目根目录。"),
         QJsonObject{{"type", "object"}, {"properties", QJsonObject{{"path", QJsonObject{{"type", "string"}, {"description", "相对项目根目录的目录，默认为 ."}}}}}}},
        {QStringLiteral("read_file"), QStringLiteral("读取工作区内的 UTF-8 文本文件。路径必须相对于项目根目录。"),
         QJsonObject{{"type", "object"}, {"properties", QJsonObject{{"path", QJsonObject{{"type", "string"}, {"description", "相对项目根目录的文件路径"}}}}}, {"required", QJsonArray{QStringLiteral("path")}}}},
        {QStringLiteral("search_text"), QStringLiteral("在工作区文件中按不区分大小写方式搜索文本，最多返回 100 个命中。"),
         QJsonObject{{"type", "object"}, {"properties", QJsonObject{{"query", QJsonObject{{"type", "string"}, {"description", "要搜索的文本"}}}, {"path", QJsonObject{{"type", "string"}, {"description", "相对项目根目录的起始目录，默认为 ."}}}}}, {"required", QJsonArray{QStringLiteral("query")}}}}
        ,{QStringLiteral("write_file"), QStringLiteral("创建或覆盖工作区内的 UTF-8 文本文件。仅在用户明确要求修改项目时使用。"),
         QJsonObject{{"type", "object"}, {"properties", QJsonObject{{"path", QJsonObject{{"type", "string"}}}, {"content", QJsonObject{{"type", "string"}}}}}, {"required", QJsonArray{QStringLiteral("path"), QStringLiteral("content")}}}}
    };
}

QString AgentToolService::resolveWritableWorkspacePath(const QString& relativePath, QString* error) const {
    const QString cleaned = QDir::cleanPath(relativePath);
    if (cleaned.isEmpty() || QDir::isAbsolutePath(cleaned) || cleaned == QStringLiteral("..") || cleaned.startsWith(QStringLiteral("../"))) {
        if (error) *error = QStringLiteral("路径必须位于工作区内"); return {};
    }
    const QString candidate = QDir(m_workspaceRoot).absoluteFilePath(cleaned);
    const QFileInfo parentInfo(QFileInfo(candidate).dir().absolutePath());
    const QString parent = parentInfo.canonicalFilePath();
    if (parent.isEmpty()) { if (error) *error = QStringLiteral("目标目录不存在"); return {}; }
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
    const QString prefix = QDir::cleanPath(m_workspaceRoot + QLatin1Char('/'));
    if (parent.compare(m_workspaceRoot, sensitivity) != 0 && !parent.startsWith(prefix, sensitivity)) {
        if (error) *error = QStringLiteral("路径必须位于工作区内"); return {};
    }
    QFileInfo candidateInfo(candidate);
    if (candidateInfo.exists()) {
        const QString candidateCanonical = candidateInfo.canonicalFilePath();
        if (candidateCanonical.compare(m_workspaceRoot, sensitivity) != 0 && !candidateCanonical.startsWith(prefix, sensitivity)) {
            if (error) *error = QStringLiteral("已存在的文件是符号链接且指向工作区外"); return {};
        }
    }
    return candidate;
}

QString AgentToolService::resolveWorkspacePath(const QString& relativePath, QString* error) const {
    const QString candidate = QDir(m_workspaceRoot).absoluteFilePath(relativePath);
    const QString resolved = QFileInfo(candidate).canonicalFilePath();
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive;
#endif
    // canonicalFilePath() always normalizes separators to '/', while
    // QDir::separator() is '\\' on Windows.  Keep both operands in the
    // canonical form, otherwise a valid child path is rejected as external.
    const QString prefix = QDir::cleanPath(m_workspaceRoot + QLatin1Char('/'));
    if (resolved.isEmpty() || (resolved.compare(m_workspaceRoot, caseSensitivity) != 0
        && !resolved.startsWith(prefix, caseSensitivity))) {
        if (error) *error = QStringLiteral("路径必须位于工作区内");
        return {};
    }
    return resolved;
}

domain::agent::ToolResult AgentToolService::execute(const domain::agent::ToolCall& call) {
    domain::agent::ToolResult result{call.id, {}, true};
    const QJsonObject args = QJsonDocument::fromJson(call.arguments.toUtf8()).object();
    QString error;
    const bool isWrite = call.name == QStringLiteral("write_file");
    const QString path = isWrite ? resolveWritableWorkspacePath(args.value(QStringLiteral("path")).toString(), &error)
                                 : resolveWorkspacePath(args.value(QStringLiteral("path")).toString(QStringLiteral(".")), &error);
    if (path.isEmpty()) { result.content = error; return result; }
    if (call.name == QStringLiteral("list_files")) {
        QJsonArray files;
        const QDir dir(path);
        for (const QFileInfo& entry : dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries, QDir::DirsFirst | QDir::Name))
            files.append(entry.fileName() + (entry.isDir() ? QStringLiteral("/") : QString()));
        result.content = QString::fromUtf8(QJsonDocument(files).toJson(QJsonDocument::Compact)); result.isError = false;
    } else if (call.name == QStringLiteral("read_file")) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { result.content = QStringLiteral("无法读取文件"); return result; }
        result.content = QString::fromUtf8(file.read(64 * 1024)); result.isError = false;
    } else if (call.name == QStringLiteral("search_text")) {
        const QString query = args.value(QStringLiteral("query")).toString();
        if (query.isEmpty()) { result.content = QStringLiteral("缺少 query 参数"); return result; }
        QJsonArray hits;
        QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext() && hits.size() < 100) {
            const QString filePath = it.next();
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text) || file.size() > 1024 * 1024) continue;
            const QString content = QString::fromUtf8(file.readAll());
            const int offset = content.indexOf(query, 0, Qt::CaseInsensitive);
            if (offset >= 0) hits.append(QJsonObject{{"path", QDir(m_workspaceRoot).relativeFilePath(filePath)}, {"line", content.left(offset).count(QLatin1Char('\n')) + 1}});
        }
        result.content = QString::fromUtf8(QJsonDocument(hits).toJson(QJsonDocument::Compact)); result.isError = false;
    } else if (call.name == QStringLiteral("write_file")) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) { result.content = QStringLiteral("无法写入文件"); return result; }
        file.write(args.value(QStringLiteral("content")).toString().toUtf8());
        result.content = QStringLiteral("已写入 ") + QDir(m_workspaceRoot).relativeFilePath(path);
        result.isError = false;
    } else result.content = QStringLiteral("未知工具: ") + call.name;
    return result;
}
} // namespace services::agent
