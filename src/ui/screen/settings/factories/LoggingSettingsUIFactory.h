#pragma once
#include "ui/screen/settings/ISettingsUIFactory.h"

namespace ui::screen::settings {

    /**
     * @brief 日志级别设置项工厂
     */
    class LoggingLevelSettingsUIFactory : public ISettingsUIFactory {
    public:
        QString targetProviderId() const override { return QStringLiteral("logging"); }
        QString iconGlyph() const override;
        QString title() const override;
        QString subtitle() const override;
        QWidget *createControlWidget(QWidget *parent) override;
    };

    /**
     * @brief 日志占用与清理设置项工厂
     */
    class LoggingStorageSettingsUIFactory : public ISettingsUIFactory {
    public:
        QString targetProviderId() const override { return QStringLiteral("logging"); }
        QString iconGlyph() const override;
        QString title() const override;
        QString subtitle() const override;
        QWidget *createControlWidget(QWidget *parent) override;
    };

    /**
     * @brief 打开日志目录设置项工厂
     */
    class LoggingOpenDirSettingsUIFactory : public ISettingsUIFactory {
    public:
        QString targetProviderId() const override { return QStringLiteral("logging"); }
        QString iconGlyph() const override;
        QString title() const override;
        QString subtitle() const override;
        QWidget *createControlWidget(QWidget *parent) override;
    };

    /**
     * @brief 导出诊断日志设置项工厂
     */
    class LoggingExportSettingsUIFactory : public ISettingsUIFactory {
    public:
        QString targetProviderId() const override { return QStringLiteral("logging"); }
        QString iconGlyph() const override;
        QString title() const override;
        QString subtitle() const override;
        QWidget *createControlWidget(QWidget *parent) override;
    };

} // namespace ui::screen::settings
