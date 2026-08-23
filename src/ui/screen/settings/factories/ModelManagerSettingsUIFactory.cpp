#include "ModelManagerSettingsUIFactory.h"
#include "ui/screen/settings/SettingsUIRegistry.h"
#include "ui/screen/settings/SettingsPage.h"
#include "ui/screen/settings/SettingsViewModel.h"

#include <FluentQt/Design.h>
#include <FluentQt/BasicInput.h>

namespace ui::screen::settings {

    QString ModelManagerSettingsUIFactory::iconGlyph() const {
        return Typography::Icons::Cloud;
    }

    QString ModelManagerSettingsUIFactory::title() const {
        return QObject::tr("模型与服务商");
    }

    QString ModelManagerSettingsUIFactory::subtitle() const {
        return QObject::tr("配置大语言模型服务商 API 凭据、端点与已启用模型");
    }

    QWidget *ModelManagerSettingsUIFactory::createControlWidget(QWidget *parent) {
        auto *btn = new fluent::basicinput::Button(parent);
        btn->setText(QObject::tr("管理模型与服务商"));
        btn->setMinimumWidth(140);

        QObject::connect(btn, &fluent::basicinput::Button::clicked, btn, [btn]() {
            QWidget *w = btn;
            while (w) {
                if (auto *page = qobject_cast<SettingsPage *>(w)) {
                    if (page->viewModel()) {
                        page->viewModel()->openModelManager(page);
                        return;
                    }
                }
                w = w->parentWidget();
            }
        });

        return btn;
    }

} // namespace ui::screen::settings

REGISTER_SETTINGS_UI(ui::screen::settings::ModelManagerSettingsUIFactory)
