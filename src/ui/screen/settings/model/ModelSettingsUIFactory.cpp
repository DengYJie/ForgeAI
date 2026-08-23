#include "ModelSettingsUIFactory.h"
#include <FluentQt/Design.h>
#include <FluentQt/BasicInput.h>

namespace ui::screen::settings {
    ModelSettingsUIFactory::ModelSettingsUIFactory(ModelSettingsViewModel *viewModel)
        : m_viewModel(viewModel) {
    }

    QString ModelSettingsUIFactory::categoryDisplayName() const {
        return QObject::tr("模型与服务商");
    }

    QString ModelSettingsUIFactory::iconGlyph() const {
        return Typography::Icons::Cloud;
    }

    QString ModelSettingsUIFactory::title() const {
        return QObject::tr("模型与服务商");
    }

    QString ModelSettingsUIFactory::subtitle() const {
        return QObject::tr("配置大语言模型服务商 API 凭据、端点与已启用模型");
    }

    QWidget *ModelSettingsUIFactory::createControlWidget(QWidget *parent) {
        auto *btn = new fluent::basicinput::Button(parent);
        btn->setText(QObject::tr("管理模型与服务商"));
        btn->setMinimumWidth(140);

        auto *vm = m_viewModel;
        QObject::connect(btn, &fluent::basicinput::Button::clicked, [vm]() {
            if (vm) {
                vm->requestOpenModelManager();
            }
        });

        return btn;
    }
} // namespace ui::screen::settings
