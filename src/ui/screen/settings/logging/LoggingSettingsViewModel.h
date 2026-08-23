#pragma once
#include <QObject>
#include <QString>

namespace core::settings {
    class LoggingSettingsProvider;
}
namespace core::logging {
    class LoggingSettingsService;
}

namespace ui::screen::settings {
    /**
     * @brief 日志与诊断设置局部 ViewModel
     * @details 集中处理日志等级切换、大小统计、清理、目录打开及诊断日志导出
     */
    class LoggingSettingsViewModel : public QObject {
        Q_OBJECT

    public:
        /**
         * @param provider 日志设置持久化提供者指针
         * @param service 日志管理服务指针
         * @param parent 父 QObject
         */
        explicit LoggingSettingsViewModel(
            core::settings::LoggingSettingsProvider *provider,
            core::logging::LoggingSettingsService *service = nullptr,
            QObject *parent = nullptr
        );
        ~LoggingSettingsViewModel() override = default;

        /**
         * @brief 获取当前日志级别索引 (0: 普通, 1: 详细, 2: 调试)
         */
        int logLevel() const;

        /**
         * @brief 设置日志级别索引
         * @param level 日志级别索引
         */
        void setLogLevel(int level);

        /**
         * @brief 获取格式化后的日志目录占用大小文本 (如 "1.2 MB")
         */
        QString formattedLogSize() const;

        /**
         * @brief 清理历史日志文件
         * @return 是否清理成功
         */
        bool clearLogs();

        /**
         * @brief 在操作系统文件管理器中打开日志目录
         */
        void openLogDirectory();

        /**
         * @brief 导出诊断日志压缩包至指定路径
         * @param destinationZipPath 保存目标 zip 文件路径
         * @return 是否导出成功
         */
        bool exportDiagnostics(const QString &destinationZipPath);

    Q_SIGNALS:
        /**
         * @brief 日志级别变更信号
         * @param level 新的日志级别索引
         */
        void logLevelChanged(int level);

        /**
         * @brief 日志占用大小变更信号
         * @param formattedSize 格式化后的大小文本
         */
        void logSizeChanged(const QString &formattedSize);

    private:
        core::settings::LoggingSettingsProvider *m_provider = nullptr;
        core::logging::LoggingSettingsService *m_service = nullptr;
    };
} // namespace ui::screen::settings
