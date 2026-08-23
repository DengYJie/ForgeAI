#pragma once
#include <FluentQt/DialogsFlyouts.h>
#include <FluentQt/BasicInput.h>
#include <FluentQt/TextFields.h>
#include "domain/model/Model.h"

namespace ui::screen::settings::model_manager {

    /**
     * @brief 添加自定义模型对话框
     */
    class AddModelDialog : public fluent::dialogs_flyouts::ContentDialog {
        Q_OBJECT

    public:
        explicit AddModelDialog(const QString &providerId, QWidget *parent = nullptr);
        ~AddModelDialog() override = default;

        domain::model::Model resultModel() const;

    private:
        void setupContent();

        QString m_providerId;
        fluent::textfields::LineEdit *m_idEdit = nullptr;
        fluent::textfields::LineEdit *m_nameEdit = nullptr;
        fluent::textfields::LineEdit *m_contextEdit = nullptr;
        fluent::basicinput::CheckBox *m_reasoningCheck = nullptr;
        fluent::basicinput::CheckBox *m_toolsCheck = nullptr;
        fluent::basicinput::CheckBox *m_visionCheck = nullptr;
    };

} // namespace ui::screen::settings::model_manager
