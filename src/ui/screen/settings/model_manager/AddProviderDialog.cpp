#include "AddProviderDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <FluentQt/Design.h>

namespace ui::screen::settings::model_manager {

    AddProviderDialog::AddProviderDialog(QWidget *parent)
        : fluent::dialogs_flyouts::ContentDialog(parent) {
        setTitle(tr("添加服务商"));
        setPrimaryButtonText(tr("添加"));
        setCloseButtonText(tr("取消"));
        setDefaultButton(Primary);
        setupContent();
    }

    void AddProviderDialog::setupContent() {
        auto *contentWidget = new QWidget(this);
        auto *layout = new QVBoxLayout(contentWidget);
        layout->setContentsMargins(0, 8, 0, 8);
        layout->setSpacing(12);

        // 1. 服务商 ID
        auto *idLabel = new fluent::textfields::Label(tr("服务商唯一标识 (ID)"), contentWidget);
        idLabel->setFluentTypography(Typography::FontRole::Caption);
        idLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        m_idEdit = new fluent::textfields::LineEdit(contentWidget);
        m_idEdit->setPlaceholderText(QStringLiteral("例如: my-openai-gateway"));
        layout->addWidget(idLabel);
        layout->addWidget(m_idEdit);

        // 2. 显示名称
        auto *nameLabel = new fluent::textfields::Label(tr("显示名称"), contentWidget);
        nameLabel->setFluentTypography(Typography::FontRole::Caption);
        nameLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        m_nameEdit = new fluent::textfields::LineEdit(contentWidget);
        m_nameEdit->setPlaceholderText(QStringLiteral("例如: 自建中转网关"));
        layout->addWidget(nameLabel);
        layout->addWidget(m_nameEdit);

        // 3. 协议类型
        auto *typeLabel = new fluent::textfields::Label(tr("协议驱动类型"), contentWidget);
        typeLabel->setFluentTypography(Typography::FontRole::Caption);
        typeLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        m_typeCombo = new fluent::basicinput::ComboBox(contentWidget);
        m_typeCombo->addItems({
            tr("OpenAI 兼容 (/v1/chat/completions)"),
            tr("OpenAI 原生 Responses"),
            tr("Anthropic 原生 Messages"),
            tr("Google Gemini"),
            tr("本地 Ollama"),
            tr("Azure OpenAI"),
            tr("Amazon Bedrock")
        });
        layout->addWidget(typeLabel);
        layout->addWidget(m_typeCombo);

        // 4. API 基础地址
        auto *urlLabel = new fluent::textfields::Label(tr("API 基础地址 (Base URL)"), contentWidget);
        urlLabel->setFluentTypography(Typography::FontRole::Caption);
        urlLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        m_urlEdit = new fluent::textfields::LineEdit(contentWidget);
        m_urlEdit->setPlaceholderText(QStringLiteral("https://api.example.com/v1"));
        layout->addWidget(urlLabel);
        layout->addWidget(m_urlEdit);

        // 5. API Key
        auto *keyLabel = new fluent::textfields::Label(tr("API 密钥 (API Key)"), contentWidget);
        keyLabel->setFluentTypography(Typography::FontRole::Caption);
        keyLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        m_keyEdit = new fluent::textfields::PasswordBox(contentWidget);
        m_keyEdit->setPlaceholderText(QStringLiteral("sk-..."));
        layout->addWidget(keyLabel);
        layout->addWidget(m_keyEdit);

        setContent(contentWidget);
    }

    domain::model::ModelProvider AddProviderDialog::resultProvider() const {
        domain::model::ModelProvider provider;
        provider.id = m_idEdit ? m_idEdit->text().trimmed() : QString();
        provider.name = m_nameEdit ? m_nameEdit->text().trimmed() : QString();
        if (provider.name.isEmpty()) {
            provider.name = provider.id;
        }

        int typeIdx = m_typeCombo ? m_typeCombo->currentIndex() : 0;
        switch (typeIdx) {
            case 0: provider.protocol = domain::model::ProtocolType::OpenAIChatCompletions; break;
            case 1: provider.protocol = domain::model::ProtocolType::OpenAIResponses; break;
            case 2: provider.protocol = domain::model::ProtocolType::AnthropicMessages; break;
            case 3: provider.protocol = domain::model::ProtocolType::GeminiGenerateContent; break;
            case 4: provider.protocol = domain::model::ProtocolType::OllamaChat; break;
            case 5: provider.protocol = domain::model::ProtocolType::AzureOpenAI; break;
            case 6: provider.protocol = domain::model::ProtocolType::AmazonBedrock; break;
            default: provider.protocol = domain::model::ProtocolType::OpenAIChatCompletions; break;
        }

        provider.baseUrl = m_urlEdit ? m_urlEdit->text().trimmed() : QString();
        provider.apiKey = m_keyEdit ? m_keyEdit->text().trimmed() : QString();
        provider.isEnabled = true;

        return provider;
    }

} // namespace ui::screen::settings::model_manager
