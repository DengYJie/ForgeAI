#include "CreateProjectDialog.h"

#include <QVBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <FluentQt/TextFields.h>
#include <FluentQt/BasicInput.h>

namespace ui::screen::work {

CreateProjectDialog::CreateProjectDialog(QWidget* parent) : ContentDialog(parent) {
    setTitle(tr("创建项目"));
    setPrimaryButtonText(tr("添加项目"));
    setCloseButtonText(tr("取消"));
    setDefaultButton(Primary);

    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 8, 0, 8);
    layout->setSpacing(12);

    auto* description = new fluent::textfields::Label(
        tr("添加一个本地文件夹作为项目工作区。项目 Agent 只能读取和修改该文件夹中的文件。"), content);
    description->setWordWrap(true);
    description->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
    layout->addWidget(description);

    auto addLabel = [content, layout](const QString& text) {
        auto* label = new fluent::textfields::Label(text, content);
        label->setFluentTypography(Typography::FontRole::Caption);
        label->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        layout->addWidget(label);
    };

    addLabel(tr("项目名称（可选）"));
    m_name = new fluent::textfields::LineEdit(content);
    m_name->setPlaceholderText(tr("默认使用文件夹名称"));
    layout->addWidget(m_name);

    addLabel(tr("工作区文件夹"));
    m_path = new fluent::textfields::LineEdit(content);
    m_path->setReadOnly(true);
    m_path->setPlaceholderText(tr("选择项目 Agent 可以读取和编辑的文件夹"));
    layout->addWidget(m_path);

    auto* choose = new fluent::basicinput::Button(tr("浏览…"), content);
    choose->setFluentStyle(fluent::basicinput::Button::Subtle);
    layout->addWidget(choose, 0, Qt::AlignLeft);

    setContent(content);

    connect(choose, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getExistingDirectory(this, tr("选择项目文件夹"));
        if (path.isEmpty()) return;
        m_path->setText(path);
        if (m_name->text().isEmpty()) {
            m_name->setText(QFileInfo(path).fileName());
        }
    });
}

QString CreateProjectDialog::name() const {
    return m_name->text();
}

QString CreateProjectDialog::path() const {
    return m_path->text();
}

void CreateProjectDialog::accept() {
    if (!m_path->text().trimmed().isEmpty()) {
        ContentDialog::accept();
    }
}

} // namespace ui::screen::work
