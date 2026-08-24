#include "LoggingSettingsUIFactory.h"

#include <QHBoxLayout>
#include <QFileDialog>
#include <QDate>
#include <FluentQt/Design.h>
#include <FluentQt/BasicInput.h>
#include <FluentQt/TextFields.h>
#include <FluentQt/StatusInfo.h>

namespace ui::screen::settings {

    LoggingLevelSettingsUIFactory::LoggingLevelSettingsUIFactory(LoggingSettingsViewModel *viewModel)
        : m_viewModel(viewModel) {
    }

    QString LoggingLevelSettingsUIFactory::categoryDisplayName() const {
        return QObject::tr("日志与诊断");
    }

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
        if (!m_viewModel) return new QWidget(parent);

        auto *combo = new fluent::basicinput::ComboBox(parent);
        combo->addItems({QObject::tr("普通"), QObject::tr("详细"), QObject::tr("调试")});
        combo->setMinimumWidth(130);

        int currentLevel = m_viewModel->logLevel();
        combo->setCurrentIndex(currentLevel);

        auto *vm = m_viewModel;
        QObject::connect(combo, qOverload<int>(&fluent::basicinput::ComboBox::currentIndexChanged),
                         [vm](int idx) {
                             if (vm) {
                                 vm->setLogLevel(idx);
                             }
                         });

        QObject::connect(m_viewModel, &LoggingSettingsViewModel::logLevelChanged, combo,
                         [combo](int level) {
                             if (combo->currentIndex() != level) {
                                 combo->setCurrentIndex(level);
                             }
                         });

        return combo;
    }

    LoggingStorageSettingsUIFactory::LoggingStorageSettingsUIFactory(LoggingSettingsViewModel *viewModel)
        : m_viewModel(viewModel) {
    }

    QString LoggingStorageSettingsUIFactory::categoryDisplayName() const {
        return QObject::tr("日志与诊断");
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
        if (!m_viewModel) return new QWidget(parent);

        auto *container = new QWidget(parent);
        auto *layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(12);

        auto *sizeLabel = new fluent::textfields::Label(
            m_viewModel->formattedLogSize(),
            container
        );
        sizeLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        sizeLabel->setFluentTypography(Typography::FontRole::Body);

        auto *clearBtn = new fluent::basicinput::Button(container);
        clearBtn->setText(QObject::tr("清除日志"));
        clearBtn->setMinimumWidth(100);
        clearBtn->setCriticalOnHover(true);
        clearBtn->setIconGlyph(Typography::Icons::Delete);
        clearBtn->setFluentLayout(fluent::basicinput::Button::IconBefore);

        layout->addWidget(sizeLabel, 0, Qt::AlignVCenter);
        layout->addWidget(clearBtn, 0, Qt::AlignVCenter);

        auto *vm = m_viewModel;
        QObject::connect(clearBtn, &fluent::basicinput::Button::clicked, parent, [parent, vm]() {
            if (!vm) return;
            bool ok = vm->clearLogs();
            if (ok) {
                fluent::status_info::Toast::showToast(
                    parent,
                    QObject::tr("日志已清除"),
                    fluent::status_info::Toast::Success
                );
            }
        });

        QObject::connect(m_viewModel, &LoggingSettingsViewModel::logSizeChanged, sizeLabel,
                         [sizeLabel](const QString &formattedSize) {
                             if (sizeLabel) {
                                 sizeLabel->setText(formattedSize);
                             }
                         });

        return container;
    }

    LoggingOpenDirSettingsUIFactory::LoggingOpenDirSettingsUIFactory(LoggingSettingsViewModel *viewModel)
        : m_viewModel(viewModel) {
    }

    QString LoggingOpenDirSettingsUIFactory::categoryDisplayName() const {
        return QObject::tr("日志与诊断");
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
        btn->setIconGlyph(Typography::Icons::Folder);
        btn->setFluentLayout(fluent::basicinput::Button::IconBefore);

        auto *vm = m_viewModel;
        QObject::connect(btn, &fluent::basicinput::Button::clicked, [vm]() {
            if (vm) {
                vm->openLogDirectory();
            }
        });

        return btn;
    }

    LoggingExportSettingsUIFactory::LoggingExportSettingsUIFactory(LoggingSettingsViewModel *viewModel)
        : m_viewModel(viewModel) {
    }

    QString LoggingExportSettingsUIFactory::categoryDisplayName() const {
        return QObject::tr("日志与诊断");
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
        btn->setFluentStyle(fluent::basicinput::Button::Accent);
        btn->setIconGlyph(Typography::Icons::Download);
        btn->setFluentLayout(fluent::basicinput::Button::IconBefore);

        auto *vm = m_viewModel;
        QObject::connect(btn, &fluent::basicinput::Button::clicked, parent, [parent, vm]() {
            if (!vm) return;
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

            bool success = vm->exportDiagnostics(savePath);
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
