#include "AppearanceSettingsUIFactory.h"
#include <FluentQt/Design.h>
#include <FluentQt/BasicInput.h>

namespace ui::screen::settings {
    AppearanceSettingsUIFactory::AppearanceSettingsUIFactory(AppearanceSettingsViewModel *viewModel)
        : m_viewModel(viewModel) {
    }

    QString AppearanceSettingsUIFactory::categoryDisplayName() const {
        return QObject::tr("外观与行为");
    }

    QString AppearanceSettingsUIFactory::iconGlyph() const {
        return Typography::Icons::Brightness;
    }

    QString AppearanceSettingsUIFactory::title() const {
        return QObject::tr("应用主题");
    }

    QString AppearanceSettingsUIFactory::subtitle() const {
        return QObject::tr("选择要显示的应用主题");
    }

    QWidget *AppearanceSettingsUIFactory::createControlWidget(QWidget *parent) {
        if (!m_viewModel) return new QWidget(parent);

        auto *themeCombo = new fluent::basicinput::ComboBox(parent);
        themeCombo->addItems({QObject::tr("跟随系统"), QObject::tr("浅色"), QObject::tr("深色")});
        themeCombo->setMinimumWidth(130);

        // 初始化当前选中项
        int currentMode = static_cast<int>(m_viewModel->themeMode());
        themeCombo->setCurrentIndex(currentMode);

        // UI 变更 -> ViewModel
        auto *vm = m_viewModel;
        QObject::connect(themeCombo, qOverload<int>(&fluent::basicinput::ComboBox::currentIndexChanged),
                         [vm](int idx) {
                             if (vm) {
                                 vm->setThemeMode(static_cast<core::settings::ThemeMode>(idx));
                             }
                         });

        // ViewModel 变更 -> UI 同步
        QObject::connect(m_viewModel, &AppearanceSettingsViewModel::themeModeChanged, themeCombo,
                         [themeCombo](core::settings::ThemeMode mode) {
                             int index = static_cast<int>(mode);
                             if (themeCombo->currentIndex() != index) {
                                 themeCombo->setCurrentIndex(index);
                             }
                         });

        return themeCombo;
    }
} // namespace ui::screen::settings
