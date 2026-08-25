#pragma once

#include <FluentQt/DialogsFlyouts.h>

namespace fluent::textfields { class LineEdit; }

namespace ui::screen::work {

class CreateProjectDialog final : public ::fluent::dialogs_flyouts::ContentDialog {
    Q_OBJECT
public:
    explicit CreateProjectDialog(QWidget* parent = nullptr);

    QString name() const;
    QString path() const;

protected:
    void accept() override;

private:
    ::fluent::textfields::LineEdit* m_name = nullptr;
    ::fluent::textfields::LineEdit* m_path = nullptr;
};

} // namespace ui::screen::work
