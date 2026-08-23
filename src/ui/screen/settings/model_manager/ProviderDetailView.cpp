#include "ProviderDetailView.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QDesktopServices>
#include <QUrl>
#include <FluentQt/Layout.h>
#include <FluentQt/TextFields.h>
#include <FluentQt/BasicInput.h>
#include <FluentQt/Scrolling.h>
#include <FluentQt/StatusInfo.h>
#include <FluentQt/Design.h>
#include "network/QtHttpClient.h"

namespace ui::screen::settings::model_manager {

    namespace {
        QString protocolDisplayName(domain::model::ProviderType type) {
            switch (type) {
                case domain::model::ProviderType::OpenAIChatCompletionsCompatible:
                    return QStringLiteral("OpenAI 兼容协议 (/v1/chat/completions)");
                case domain::model::ProviderType::OpenAIResponses:
                    return QStringLiteral("OpenAI 原生 Responses 协议");
                case domain::model::ProviderType::Anthropic:
                    return QStringLiteral("Anthropic 原生 Messages 协议");
                case domain::model::ProviderType::GoogleGemini:
                    return QStringLiteral("Google Gemini 原生协议");
                case domain::model::ProviderType::Ollama:
                    return QStringLiteral("本地 Ollama 协议");
                case domain::model::ProviderType::AzureOpenAI:
                    return QStringLiteral("Azure OpenAI 协议");
                case domain::model::ProviderType::AmazonBedrock:
                    return QStringLiteral("Amazon Bedrock 协议");
                default:
                    return QStringLiteral("自定义协议");
            }
        }
    } // namespace

    ProviderDetailView::ProviderDetailView(QWidget *parent)
        : QWidget(parent) {
        setupUi();
    }

    void ProviderDetailView::setupUi() {
        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);

        auto *scrollArea = new fluent::scrolling::ScrollView(this);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setHorizontalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);

        auto *contentWidget = new QWidget(scrollArea);
        auto *contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(24, 20, 24, 24);
        contentLayout->setSpacing(16);

        // ================= 1. 服务商头部卡片 =================
        auto *headerCard = new fluent::layout::Card(contentWidget);
        auto *headerLayout = new QHBoxLayout(headerCard);
        headerLayout->setContentsMargins(16, 14, 16, 14);
        headerLayout->setSpacing(12);

        auto *titleCol = new QVBoxLayout();
        titleCol->setContentsMargins(0, 0, 0, 0);
        titleCol->setSpacing(2);

        m_nameLabel = new fluent::textfields::Label(contentWidget);
        m_nameLabel->setFluentTypography(Typography::FontRole::Title);
        m_nameLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);

        m_protocolLabel = new fluent::textfields::Label(contentWidget);
        m_protocolLabel->setFluentTypography(Typography::FontRole::Caption);
        m_protocolLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);

        titleCol->addWidget(m_nameLabel);
        titleCol->addWidget(m_protocolLabel);
        headerLayout->addLayout(titleCol, 1);

        m_deleteProviderBtn = new fluent::basicinput::Button(contentWidget);
        m_deleteProviderBtn->setText(tr("删除服务商"));
        connect(m_deleteProviderBtn, &fluent::basicinput::Button::clicked, this, [this]() {
            Q_EMIT providerDeleted(m_provider.id);
        });
        headerLayout->addWidget(m_deleteProviderBtn);

        m_enableSwitch = new fluent::basicinput::ToggleSwitch(contentWidget);
        connect(m_enableSwitch, &fluent::basicinput::ToggleSwitch::toggled, this, [this](bool checked) {
            m_provider.isEnabled = checked;
            Q_EMIT providerChanged(m_provider);
        });
        headerLayout->addWidget(m_enableSwitch);

        contentLayout->addWidget(headerCard);

        // ================= 2. 连接端点与凭证卡片 =================
        auto *connCard = new fluent::layout::Card(contentWidget);
        auto *connLayout = new QVBoxLayout(connCard);
        connLayout->setContentsMargins(16, 16, 16, 16);
        connLayout->setSpacing(12);

        auto *connTitle = new fluent::textfields::Label(tr("服务商连接配置"), connCard);
        connTitle->setFluentTypography(Typography::FontRole::BodyStrong);
        connTitle->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
        connLayout->addWidget(connTitle);

        // 2.1 Base URL
        auto *urlLabel = new fluent::textfields::Label(tr("API 基础地址 (Base URL)"), connCard);
        urlLabel->setFluentTypography(Typography::FontRole::Caption);
        urlLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        m_urlEdit = new fluent::textfields::LineEdit(connCard);
        connect(m_urlEdit, &fluent::textfields::LineEdit::textChanged, this, [this](const QString &text) {
            m_provider.baseUrl = text.trimmed();
            Q_EMIT providerChanged(m_provider);
        });
        connLayout->addWidget(urlLabel);
        connLayout->addWidget(m_urlEdit);

        // 2.2 API Key
        auto *keyLabel = new fluent::textfields::Label(tr("API 密钥 (API Key)"), connCard);
        keyLabel->setFluentTypography(Typography::FontRole::Caption);
        keyLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);

        auto *keyRow = new QHBoxLayout();
        keyRow->setContentsMargins(0, 0, 0, 0);
        keyRow->setSpacing(10);

        m_keyEdit = new fluent::textfields::PasswordBox(connCard);
        m_keyEdit->setPlaceholderText(QStringLiteral("sk-..."));
        connect(m_keyEdit, &fluent::textfields::PasswordBox::textChanged, this, [this](const QString &text) {
            m_provider.apiKey = text.trimmed();
            Q_EMIT providerChanged(m_provider);
        });

        m_testBtn = new fluent::basicinput::Button(connCard);
        m_testBtn->setText(tr("测试连接"));
        m_testBtn->setMinimumWidth(90);
        connect(m_testBtn, &fluent::basicinput::Button::clicked, this, &ProviderDetailView::testConnection);

        keyRow->addWidget(m_keyEdit, 1);
        keyRow->addWidget(m_testBtn);

        connLayout->addWidget(keyLabel);
        connLayout->addLayout(keyRow);

        contentLayout->addWidget(connCard);

        // ================= 3. 模型列表卡片 =================
        auto *modelCard = new fluent::layout::Card(contentWidget);
        auto *modelCardLayout = new QVBoxLayout(modelCard);
        modelCardLayout->setContentsMargins(16, 16, 16, 16);
        modelCardLayout->setSpacing(12);

        auto *modelHeaderRow = new QHBoxLayout();
        modelHeaderRow->setContentsMargins(0, 0, 0, 0);
        modelHeaderRow->setSpacing(10);

        m_modelCountLabel = new fluent::textfields::Label(tr("可用模型"), modelCard);
        m_modelCountLabel->setFluentTypography(Typography::FontRole::BodyStrong);
        m_modelCountLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);

        m_refreshBtn = new fluent::basicinput::Button(modelCard);
        m_refreshBtn->setText(tr("探测刷新 ↻"));

        m_addModelBtn = new fluent::basicinput::Button(modelCard);
        m_addModelBtn->setText(tr("添加模型 +"));

        connect(m_refreshBtn, &fluent::basicinput::Button::clicked, this, [this]() {
            Q_EMIT refreshModelsRequested(m_provider.id);
        });
        connect(m_addModelBtn, &fluent::basicinput::Button::clicked, this, [this]() {
            Q_EMIT addModelRequested(m_provider.id);
        });

        modelHeaderRow->addWidget(m_modelCountLabel);
        modelHeaderRow->addStretch(1);
        modelHeaderRow->addWidget(m_refreshBtn);
        modelHeaderRow->addWidget(m_addModelBtn);
        modelCardLayout->addLayout(modelHeaderRow);

        m_modelListContainer = new QWidget(modelCard);
        auto *listLayout = new QVBoxLayout(m_modelListContainer);
        listLayout->setContentsMargins(0, 4, 0, 4);
        listLayout->setSpacing(8);
        modelCardLayout->addWidget(m_modelListContainer);

        contentLayout->addWidget(modelCard);
        contentLayout->addStretch(1);

        scrollArea->setWidget(contentWidget);
        rootLayout->addWidget(scrollArea);
    }

    void ProviderDetailView::setProvider(const domain::model::ModelProvider &provider) {
        m_provider = provider;

        if (m_nameLabel) m_nameLabel->setText(provider.name);
        if (m_protocolLabel) m_protocolLabel->setText(protocolDisplayName(provider.type));
        if (m_enableSwitch) m_enableSwitch->setIsOn(provider.isEnabled);
        if (m_urlEdit) m_urlEdit->setText(provider.baseUrl);
        if (m_keyEdit) m_keyEdit->setText(provider.apiKey);

        updateModelListUi();
    }

    void ProviderDetailView::updateModelListUi() {
        if (!m_modelListContainer) return;
        auto *listLayout = qobject_cast<QVBoxLayout *>(m_modelListContainer->layout());
        if (!listLayout) return;

        if (m_modelCountLabel) {
            m_modelCountLabel->setText(tr("可用模型 (%1)").arg(m_provider.models.size()));
        }

        while (QLayoutItem *item = listLayout->takeAt(0)) {
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            delete item;
        }

        for (const auto &model : m_provider.models) {
            auto *card = new ModelItemCard(model, m_modelListContainer);
            listLayout->addWidget(card);

            connect(card, &ModelItemCard::modelToggled, this, [this](const QString &modelId, bool enabled) {
                for (auto &m : m_provider.models) {
                    if (m.id == modelId) {
                        m.isEnabled = enabled;
                        break;
                    }
                }
                Q_EMIT providerChanged(m_provider);
            });

            connect(card, &ModelItemCard::modelDeleted, this, [this](const QString &modelId) {
                m_provider.models.removeIf([&modelId](const domain::model::Model &m) {
                    return m.id == modelId;
                });
                updateModelListUi();
                Q_EMIT providerChanged(m_provider);
            });
        }
    }

    void ProviderDetailView::setRefreshing(bool refreshing) {
        if (m_refreshBtn) {
            m_refreshBtn->setEnabled(!refreshing);
            m_refreshBtn->setText(refreshing ? tr("正在探测...") : tr("探测刷新 ↻"));
        }
    }

    void ProviderDetailView::testConnection() {
        if (m_provider.baseUrl.isEmpty()) {
            fluent::status_info::Toast::showToast(
                this,
                tr("API 基础地址不能为空"),
                fluent::status_info::Toast::Warning
            );
            return;
        }

        m_testBtn->setEnabled(false);
        m_testBtn->setText(tr("正在测试..."));

        auto *client = new network::QtHttpClient(this);
        network::HttpRequest req;
        req.url = m_provider.baseUrl;
        req.method = network::HttpMethod::Get;
        req.timeoutMs = 8000;
        if (!m_provider.apiKey.isEmpty()) {
            req.headers.insert(QStringLiteral("Authorization"), QStringLiteral("Bearer ") + m_provider.apiKey);
        }

        auto *op = client->send(req);
        connect(op, &network::HttpOperation::finished, this, [this, client, op]() {
            m_testBtn->setEnabled(true);
            m_testBtn->setText(tr("测试连接"));
            fluent::status_info::Toast::showToast(
                this,
                tr("连接成功"),
                fluent::status_info::Toast::Success
            );
            op->deleteLater();
            client->deleteLater();
        });

        connect(op, &network::HttpOperation::failed, this, [this, client, op](const QString &errMsg, int code) {
            m_testBtn->setEnabled(true);
            m_testBtn->setText(tr("测试连接"));
            fluent::status_info::Toast::showToast(
                this,
                tr("连接失败: %1 (代码 %2)").arg(errMsg).arg(code),
                fluent::status_info::Toast::Error
            );
            op->deleteLater();
            client->deleteLater();
        });
    }

    void ProviderDetailView::onThemeUpdated() {
        update();
    }

} // namespace ui::screen::settings::model_manager
