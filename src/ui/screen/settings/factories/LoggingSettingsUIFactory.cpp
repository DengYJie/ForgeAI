#include "LoggingSettingsUIFactory.h"
#include "core/settings/SettingsRegistry.h"
#include "core/settings/providers/LoggingSettingsProvider.h"
#include "core/logging/LoggingSettingsService.h"
#include "ui/screen/settings/SettingsUIRegistry.h"

#include <QHBoxLayout>
#include <QFileDialog>
#include <QDate>
#include <FluentQt/Design.h>
#include <FluentQt/BasicInput.h>
#include <FluentQt/TextFields.h>
#include <FluentQt/StatusInfo.h>

namespace ui::screen::settings {

    QString LoggingLevelSettingsUIFactory::iconGlyph() const {
        return Typography::Icons::Document;
    }

    QString LoggingLevelSettingsUIFactory::title() const {
        return QObject::tr("日志级别");
    }

    QString LoggingLevelSettingsUIFactory::subtitle() const {
        return QObject::tr("控制输出到日志文件的详细程度");
    }

    QWidget *LoggingLevelSettingsUIFactory::createControlWidget(QWidget *parent) {
        auto providerBase = core::settings::SettingsRegistry::instance().getProvider("logging");
        auto provider = std::dynamic_pointer_cast<core::settings::LoggingSettingsProvider>(providerBase);

        auto *combo = new fluent::basicinput::ComboBox(parent);
        combo->addItems({QObject::tr("普通"), QObject::tr("详细"), QObject::tr("调试")});
        combo->setMinimumWidth(130);

        if (provider) {
            int currentLevel = provider->get(core::settings::LoggingSettingsProvider::LogLevelKey);
            combo->setCurrentIndex(currentLevel);

            QObject::connect(combo, qOverload<int>(&fluent::basicinput::ComboBox::currentIndexChanged),
                             [provider](int idx) {
                                 provider->set(core::settings::LoggingSettingsProvider::LogLevelKey, idx);
                             });

            QObject::connect(provider.get(), &core::settings::ISettingsProvider::dataChanged, combo,
                             [combo, provider]() {
                                 int index = provider->get(core::settings::LoggingSettingsProvider::LogLevelKey);
                                 if (combo->currentIndex() != index) {
                                     combo->setCurrentIndex(index);
                                 }
                             });
        }

        return combo;
    }

    QString LoggingStorageSettingsUIFactory::iconGlyph() const {
        return Typography::Icons::Delete;
    }

    QString LoggingStorageSettingsUIFactory::title() const {
        return QObject::tr("日志占用");
    }

    QString LoggingStorageSettingsUIFactory::subtitle() const {
        return QObject::tr("查看本地历史日志占用并在需要时清理");
    }

    QWidget *LoggingStorageSettingsUIFactory::createControlWidget(QWidget *parent) {
        auto *container = new QWidget(parent);
        auto *layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(16);

        auto *sizeLabel = new fluent::textfields::Label(
            core::logging::LoggingSettingsService::instance().getFormattedLogSize(),
            container
        );
        sizeLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        sizeLabel->setFluentTypography(Typography::FontRole::Body);

        auto *clearBtn = new fluent::basicinput::Button(container);
        clearBtn->setText(QObject::tr("清除日志"));
        clearBtn->setMinimumWidth(100);

        layout->addWidget(sizeLabel, 0, Qt::AlignVCenter);
        layout->addWidget(clearBtn, 0, Qt::AlignVCenter);

        QObject::connect(clearBtn, &fluent::basicinput::Button::clicked, parent, [parent, sizeLabel]() {
            bool ok = core::logging::LoggingSettingsService::instance().clearLogs();
            if (ok) {
                sizeLabel->setText(core::logging::LoggingSettingsService::instance().getFormattedLogSize());
                fluent::status_info::Toast::showToast(
                    parent,
                    QObject::tr("日志已清除"),
                    fluent::status_info::Toast::Success
                );
            }
        });

        QObject::connect(&core::logging::LoggingSettingsService::instance(),
                         &core::logging::LoggingSettingsService::logSizeChanged,
                         sizeLabel,
                         [sizeLabel]() {
                             sizeLabel->setText(core::logging::LoggingSettingsService::instance().getFormattedLogSize());
                         });

        return container;
    }

    QString LoggingOpenDirSettingsUIFactory::iconGlyph() const {
        return Typography::Icons::Folder;
    }

    QString LoggingOpenDirSettingsUIFactory::title() const {
        return QObject::tr("日志目录");
    }

    QString LoggingOpenDirSettingsUIFactory::subtitle() const {
        return QObject::tr("在系统文件管理器中打开日志数据文件夹");
    }

    QWidget *LoggingOpenDirSettingsUIFactory::createControlWidget(QWidget *parent) {
        auto *btn = new fluent::basicinput::Button(parent);
        btn->setText(QObject::tr("打开日志目录"));
        btn->setMinimumWidth(130);

        QObject::connect(btn, &fluent::basicinput::Button::clicked, []() {
            core::logging::LoggingSettingsService::instance().openLogDirectory();
        });

        return btn;
    }

    QString LoggingExportSettingsUIFactory::iconGlyph() const {
        return Typography::Icons::Download;
    }

    QString LoggingExportSettingsUIFactory::title() const {
        return QObject::tr("导出诊断日志");
    }

    QString LoggingExportSettingsUIFactory::subtitle() const {
        return QObject::tr("自动收集系统运行环境与脱敏日志，用于故障排查与技术支持");
    }

    QWidget *LoggingExportSettingsUIFactory::createControlWidget(QWidget *parent) {
        auto *btn = new fluent::basicinput::Button(parent);
        btn->setText(QObject::tr("导出诊断日志"));
        btn->setMinimumWidth(130);

        QObject::connect(btn, &fluent::basicinput::Button::clicked, parent, [parent]() {
            QString defaultName = QStringLiteral("forgeai-diagnostics-") +
                                  QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")) +
                                  QStringLiteral(".zip");

            QString savePath = QFileDialog::getSaveFileName(
                parent,
                QObject::tr("导出诊断日志压缩包"),
                defaultName,
                QStringLiteral("ZIP 压缩包 (*.zip)")
            );

            if (savePath.isEmpty()) {
                return;
            }

            bool success = core::logging::LoggingSettingsService::instance().exportDiagnostics(savePath);
            if (success) {
                fluent::status_info::Toast::showToast(
                    parent,
                    QObject::tr("诊断日志已成功导出"),
                    fluent::status_info::Toast::Success
                );
            } else {
                fluent::status_info::Toast::showToast(
                    parent,
                    QObject::tr("导出诊断日志失败"),
                    fluent::status_info::Toast::Error
                );
            }
        });

        return btn;
    }

} // namespace ui::screen::settings

REGISTER_SETTINGS_UI(ui::screen::settings::LoggingLevelSettingsUIFactory)
REGISTER_SETTINGS_UI(ui::screen::settings::LoggingStorageSettingsUIFactory)
REGISTER_SETTINGS_UI(ui::screen::settings::LoggingOpenDirSettingsUIFactory)
REGISTER_SETTINGS_UI(ui::screen::settings::LoggingExportSettingsUIFactory)
