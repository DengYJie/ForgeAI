#include "WorkspaceFileSystem.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace llm::workspace {

    WorkspaceFileSystem::WorkspaceFileSystem(QStringList ignorePatterns)
        : m_ignorePatterns(std::move(ignorePatterns)) {
        if (m_ignorePatterns.isEmpty()) {
            m_ignorePatterns = {
                QStringLiteral(".git"),
                QStringLiteral("node_modules"),
                QStringLiteral("build"),
                QStringLiteral(".vs"),
                QStringLiteral(".idea"),
                QStringLiteral("__pycache__")
            };
        }
    }

    QStringList WorkspaceFileSystem::ignorePatterns() const {
        return m_ignorePatterns;
    }

    bool WorkspaceFileSystem::isIgnored(const QString& relativePath) const {
        const QString cleaned = QDir::cleanPath(relativePath);
        const QStringList segments = cleaned.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        for (const auto& pattern : m_ignorePatterns) {
            for (const auto& segment : segments) {
                if (segment.compare(pattern, Qt::CaseInsensitive) == 0) {
                    return true;
                }
            }
        }
        return false;
    }

    QString WorkspaceFileSystem::resolveReadablePath(
        const QString& workspaceRoot,
        const QString& relativePath,
        QString* error
    ) const {
        const QString canonicalRoot = QDir(workspaceRoot).canonicalPath();
        if (canonicalRoot.isEmpty()) {
            core::logging::LoggingService::instance().warn(core::logging::Category::Workspace, QStringLiteral("工作区根目录不存在"), {
                {QStringLiteral("status"), QStringLiteral("InvalidRoot")}
            });
            if (error) *error = QStringLiteral("工作区根目录不存在");
            return {};
        }

        const QString cleanedRelative = QDir::cleanPath(relativePath);
        if (cleanedRelative.startsWith(QStringLiteral("../")) || cleanedRelative == QStringLiteral("..")) {
            core::logging::LoggingService::instance().warn(core::logging::Category::Workspace, QStringLiteral("读取路径越界访问被拒绝"), {
                {QStringLiteral("path"), cleanedRelative},
                {QStringLiteral("reason"), QStringLiteral("EscapeAttempt")}
            });
            if (error) *error = QStringLiteral("出于安全原因，无法访问项目外的路径。");
            return {};
        }

        const QString candidate = QDir(canonicalRoot).absoluteFilePath(cleanedRelative.isEmpty() ? QStringLiteral(".") : cleanedRelative);
        const QFileInfo candidateInfo(candidate);
        const QString resolved = candidateInfo.canonicalFilePath();

#ifdef Q_OS_WIN
        constexpr Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;
#else
        constexpr Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive;
#endif
        const QString prefix = QDir::cleanPath(canonicalRoot + QLatin1Char('/'));
        if (resolved.isEmpty() || (resolved.compare(canonicalRoot, caseSensitivity) != 0
            && !resolved.startsWith(prefix, caseSensitivity))) {
            core::logging::LoggingService::instance().warn(core::logging::Category::Workspace, QStringLiteral("文件不存在或位于工作区外部"), {
                {QStringLiteral("path"), cleanedRelative}
            });
            if (error) *error = QStringLiteral("出于安全原因，无法访问项目外的路径或文件不存在。");
            return {};
        }

        return resolved;
    }

    QString WorkspaceFileSystem::resolveWritablePath(
        const QString& workspaceRoot,
        const QString& relativePath,
        QString* error
    ) const {
        const QString canonicalRoot = QDir(workspaceRoot).canonicalPath();
        if (canonicalRoot.isEmpty()) {
            core::logging::LoggingService::instance().warn(core::logging::Category::Workspace, QStringLiteral("工作区根目录不存在"), {
                {QStringLiteral("status"), QStringLiteral("InvalidRoot")}
            });
            if (error) *error = QStringLiteral("工作区根目录不存在");
            return {};
        }

        const QString cleaned = QDir::cleanPath(relativePath);
        if (cleaned.isEmpty() || QDir::isAbsolutePath(cleaned) || cleaned == QStringLiteral("..") || cleaned.startsWith(QStringLiteral("../"))) {
            core::logging::LoggingService::instance().warn(core::logging::Category::Workspace, QStringLiteral("写入路径越界访问被拒绝"), {
                {QStringLiteral("path"), cleaned},
                {QStringLiteral("reason"), QStringLiteral("EscapeAttempt")}
            });
            if (error) *error = QStringLiteral("出于安全原因，无法访问项目外的路径。");
            return {};
        }

        const QString candidate = QDir(canonicalRoot).absoluteFilePath(cleaned);
        const QFileInfo candidateInfo(candidate);
        const QString parentDir = candidateInfo.absolutePath();
        const QString parent = QFileInfo(parentDir).canonicalFilePath();
        if (parent.isEmpty()) {
            core::logging::LoggingService::instance().warn(core::logging::Category::Workspace, QStringLiteral("写入目标父目录不存在"), {
                {QStringLiteral("path"), cleaned}
            });
            if (error) *error = QStringLiteral("目标目录不存在");
            return {};
        }

#ifdef Q_OS_WIN
        constexpr Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
        constexpr Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
        const QString prefix = QDir::cleanPath(canonicalRoot + QLatin1Char('/'));
        if (parent.compare(canonicalRoot, sensitivity) != 0 && !parent.startsWith(prefix, sensitivity)) {
            core::logging::LoggingService::instance().warn(core::logging::Category::Workspace, QStringLiteral("写入目标目录位于工作区外"), {
                {QStringLiteral("path"), cleaned}
            });
            if (error) *error = QStringLiteral("出于安全原因，无法访问项目外的路径。");
            return {};
        }

        if (candidateInfo.exists()) {
            const QString candidateCanonical = candidateInfo.canonicalFilePath();
            if (candidateCanonical.compare(canonicalRoot, sensitivity) != 0 && !candidateCanonical.startsWith(prefix, sensitivity)) {
                core::logging::LoggingService::instance().warn(core::logging::Category::Workspace, QStringLiteral("已存在的文件是符号链接且指向工作区外"), {
                    {QStringLiteral("path"), cleaned}
                });
                if (error) *error = QStringLiteral("出于安全原因，无法访问项目外的路径。");
                return {};
            }
        }

        return candidate;
    }

} // namespace llm::workspace
