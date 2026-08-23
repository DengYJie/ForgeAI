#pragma once
#include <FluentQt/DialogsFlyouts.h>
#include <FluentQt/BasicInput.h>
#include <FluentQt/TextFields.h>
#include "domain/model/ModelProvider.h"

namespace ui::screen::settings::model_manager {

    /**
     * @brief 添加自定义服务商对话框
     */
    class AddProviderDialog : public fluent::dialogs_flyouts::ContentDialog {
        Q_OBJECT

    public:
        explicit AddProviderDialog(QWidget *parent = nullptr);
        ~AddProviderDialog() override = default;

        domain::model::ModelProvider resultProvider() const;

    private:
        void setupContent();

        fluent::textfields::LineEdit *m_idEdit = nullptr;
        fluent::textfields::LineEdit *m_nameEdit = nullptr;
        fluent::basicinput::ComboBox *m_typeCombo = nullptr;
        fluent::textfields::LineEdit *m_urlEdit = nullptr;
        fluent::textfields::PasswordBox *m_keyEdit = nullptr;
    };

} // namespace ui::screen::settings::model_manager
