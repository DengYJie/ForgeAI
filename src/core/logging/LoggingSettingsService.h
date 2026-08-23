#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include "domain/model/ModelProvider.h"
#include "LogLevel.h"

namespace core::logging {

    /**
     * @brief 日志管理与诊断服务
     */
    class LoggingSettingsService : public QObject {
        Q_OBJECT

    public:
        static LoggingSettingsService &instance();

        explicit LoggingSettingsService(QObject *parent = nullptr);
        ~LoggingSettingsService() override = default;

        /**
         * @brief 获取本地日志目录路径
         */
        QString getLogDirectory() const;

        /**
         * @brief 计算当前日志目录占用的总字节数
         */
        qint64 getLogDirectorySizeBytes() const;

        /**
         * @brief 获取格式化后的日志占用大小文本 (如 "42.7 MB", "1.2 MB", "0.0 KB")
         */
        QString getFormattedLogSize() const;

        /**
         * @brief 清除历史日志文件并安全清空当前日志
         * @return 是否成功
         */
        bool clearLogs();

        /**
         * @brief 在操作系统资源管理器中打开日志文件夹
         */
        void openLogDirectory() const;

        /**
         * @brief 导出诊断日志压缩包
         */
        bool exportDiagnostics(
            const QString &destinationZipPath,
            const QList<domain::model::ModelProvider> &providers = {}
        ) const;

    Q_SIGNALS:
        void logSizeChanged(qint64 newSizeBytes);
    };

} // namespace core::logging
