#include "AddModelDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <FluentQt/Design.h>

namespace ui::screen::settings::model_manager {

    AddModelDialog::AddModelDialog(const QString &providerId, QWidget *parent)
        : fluent::dialogs_flyouts::ContentDialog(parent), m_providerId(providerId) {
        setTitle(tr("添加模型"));
        setPrimaryButtonText(tr("添加"));
        setCloseButtonText(tr("取消"));
        setDefaultButton(Primary);
        setupContent();
    }

    void AddModelDialog::setupContent() {
        auto *contentWidget = new QWidget(this);
        auto *layout = new QVBoxLayout(contentWidget);
        layout->setContentsMargins(0, 8, 0, 8);
        layout->setSpacing(12);

        // 1. 模型 ID
        auto *idLabel = new fluent::textfields::Label(tr("模型 API 标识符 (ID)"), contentWidget);
        idLabel->setFluentTypography(Typography::FontRole::Caption);
        idLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        m_idEdit = new fluent::textfields::LineEdit(contentWidget);
        m_idEdit->setPlaceholderText(QStringLiteral("例如: qwen2.5-72b-instruct"));
        layout->addWidget(idLabel);
        layout->addWidget(m_idEdit);

        // 2. 显示名称
        auto *nameLabel = new fluent::textfields::Label(tr("显示名称"), contentWidget);
        nameLabel->setFluentTypography(Typography::FontRole::Caption);
        nameLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        m_nameEdit = new fluent::textfields::LineEdit(contentWidget);
        m_nameEdit->setPlaceholderText(QStringLiteral("例如: Qwen 2.5 72B (自建)"));
        layout->addWidget(nameLabel);
        layout->addWidget(m_nameEdit);

        // 3. 上下文大小
        auto *ctxLabel = new fluent::textfields::Label(tr("上下文上限 (Tokens)"), contentWidget);
        ctxLabel->setFluentTypography(Typography::FontRole::Caption);
        ctxLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        m_contextEdit = new fluent::textfields::LineEdit(contentWidget);
        m_contextEdit->setText(QStringLiteral("128000"));
        layout->addWidget(ctxLabel);
        layout->addWidget(m_contextEdit);

        // 4. 特性勾选
        auto *capsLabel = new fluent::textfields::Label(tr("模型能力"), contentWidget);
        capsLabel->setFluentTypography(Typography::FontRole::Caption);
        capsLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        layout->addWidget(capsLabel);

        auto *capsRow = new QHBoxLayout();
        capsRow->setContentsMargins(0, 0, 0, 0);
        capsRow->setSpacing(12);

        m_reasoningCheck = new fluent::basicinput::CheckBox(contentWidget);
        m_reasoningCheck->setText(tr("Thinking 思考流"));
        m_toolsCheck = new fluent::basicinput::CheckBox(contentWidget);
        m_toolsCheck->setText(tr("Tools 函数调用"));
        m_visionCheck = new fluent::basicinput::CheckBox(contentWidget);
        m_visionCheck->setText(tr("Vision 图像输入"));

        capsRow->addWidget(m_reasoningCheck);
        capsRow->addWidget(m_toolsCheck);
        capsRow->addWidget(m_visionCheck);
        capsRow->addStretch(1);
        layout->addLayout(capsRow);

        setContent(contentWidget);
    }

    domain::model::ProviderModel AddModelDialog::resultModel() const {
        domain::model::ProviderModel binding;
        binding.providerId = m_providerId;
        binding.remoteModelId = m_idEdit ? m_idEdit->text().trimmed() : QString();
        binding.isEnabled = true;
        binding.isCustom = true;
        binding.origin = domain::model::DataOrigin::User;

        int ctx = m_contextEdit ? m_contextEdit->text().toInt() : 128000;
        if (ctx <= 0) ctx = 128000;
        domain::model::ModelLimit limit;
        limit.context = ctx;
        limit.maxInput = ctx;
        limit.maxOutput = 8192;
        binding.limitsOverride = limit;

        domain::model::ModelCapabilities caps = domain::model::ModelCapability::Chat | domain::model::ModelCapability::Streaming;
        if (m_reasoningCheck && m_reasoningCheck->isChecked()) {
            caps |= domain::model::ModelCapability::Thinking;
        }
        if (m_toolsCheck && m_toolsCheck->isChecked()) {
            caps |= domain::model::ModelCapability::ToolCalling;
        }
        if (m_visionCheck && m_visionCheck->isChecked()) {
            caps |= domain::model::ModelCapability::Vision;
        }
        binding.capabilitiesOverride = caps;

        return binding;
    }

} // namespace ui::screen::settings::model_manager
