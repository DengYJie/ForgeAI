#pragma once
#include "ui/screen/settings/ISettingsUIFactory.h"
#include "LoggingSettingsViewModel.h"

namespace ui::screen::settings {

    /**
     * @brief 日志级别设置项工厂
     */
    class LoggingLevelSettingsUIFactory : public ISettingsUIFactory {
    public:
        /**
         * @param viewModel 日志设置局部 ViewModel 指针
         */
        explicit LoggingLevelSettingsUIFactory(LoggingSettingsViewModel *viewModel);

        QString id() const override { return QStringLiteral("logging.level"); }
        QString providerId() const override { return QStringLiteral("logging"); }
        QString categoryId() const override { return QStringLiteral("diagnostics"); }
        QString categoryDisplayName() const override;
        int categoryOrder() const override { return 20; }
        int itemOrder() const override { return 0; }

        QString iconGlyph() const override;
        QString title() const override;
        QString subtitle() const override;
        QWidget *createControlWidget(QWidget *parent) override;

    private:
        LoggingSettingsViewModel *m_viewModel = nullptr;
    };

    /**
     * @brief 日志占用与清理设置项工厂
     */
    class LoggingStorageSettingsUIFactory : public ISettingsUIFactory {
    public:
        /**
         * @param viewModel 日志设置局部 ViewModel 指针
         */
        explicit LoggingStorageSettingsUIFactory(LoggingSettingsViewModel *viewModel);

        QString id() const override { return QStringLiteral("logging.storage"); }
        QString providerId() const override { return QStringLiteral("logging"); }
        QString categoryId() const override { return QStringLiteral("diagnostics"); }
        QString categoryDisplayName() const override;
        int categoryOrder() const override { return 20; }
        int itemOrder() const override { return 1; }

        QString iconGlyph() const override;
        QString title() const override;
        QString subtitle() const override;
        QWidget *createControlWidget(QWidget *parent) override;

    private:
        LoggingSettingsViewModel *m_viewModel = nullptr;
    };

    /**
     * @brief 打开日志目录设置项工厂
     */
    class LoggingOpenDirSettingsUIFactory : public ISettingsUIFactory {
    public:
        /**
         * @param viewModel 日志设置局部 ViewModel 指针
         */
        explicit LoggingOpenDirSettingsUIFactory(LoggingSettingsViewModel *viewModel);

        QString id() const override { return QStringLiteral("logging.open_dir"); }
        QString providerId() const override { return QStringLiteral("logging"); }
        QString categoryId() const override { return QStringLiteral("diagnostics"); }
        QString categoryDisplayName() const override;
        int categoryOrder() const override { return 20; }
        int itemOrder() const override { return 2; }

        QString iconGlyph() const override;
        QString title() const override;
        QString subtitle() const override;
        QWidget *createControlWidget(QWidget *parent) override;

    private:
        LoggingSettingsViewModel *m_viewModel = nullptr;
    };

    /**
     * @brief 导出诊断日志设置项工厂
     */
    class LoggingExportSettingsUIFactory : public ISettingsUIFactory {
    public:
        /**
         * @param viewModel 日志设置局部 ViewModel 指针
         */
        explicit LoggingExportSettingsUIFactory(LoggingSettingsViewModel *viewModel);

        QString id() const override { return QStringLiteral("logging.export"); }
        QString providerId() const override { return QStringLiteral("logging"); }
        QString categoryId() const override { return QStringLiteral("diagnostics"); }
        QString categoryDisplayName() const override;
        int categoryOrder() const override { return 20; }
        int itemOrder() const override { return 3; }

        QString iconGlyph() const override;
        QString title() const override;
        QString subtitle() const override;
        QWidget *createControlWidget(QWidget *parent) override;

    private:
        LoggingSettingsViewModel *m_viewModel = nullptr;
    };

} // namespace ui::screen::settings
