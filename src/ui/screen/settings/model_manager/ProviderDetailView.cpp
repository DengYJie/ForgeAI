#include "ProviderDetailView.h"

#include <QHBoxLayout>
#include <QMap>
#include <QPainter>
#include <QSignalBlocker>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <functional>

#include <FluentQt/BasicInput.h>
#include <FluentQt/Collections.h>
#include <FluentQt/Layout.h>
#include <FluentQt/StatusInfo.h>
#include <FluentQt/TextFields.h>

#include "ModelActionsSplitButton.h"
#include "network/QtHttpClient.h"

namespace ui::screen::settings::model_manager {

    namespace {
        constexpr int ModelIdRole = Qt::UserRole + 1;

        QString protocolDisplayName(domain::model::ProviderType type) {
            using Type = domain::model::ProviderType;
            switch (type) {
            case Type::OpenAIChatCompletionsCompatible: return QStringLiteral("OpenAI 兼容协议 (/v1/chat/completions)");
            case Type::OpenAIResponses: return QStringLiteral("OpenAI 原生 Responses 协议");
            case Type::Anthropic: return QStringLiteral("Anthropic 原生 Messages 协议");
            case Type::GoogleGemini: return QStringLiteral("Google Gemini 原生协议");
            case Type::Ollama: return QStringLiteral("本地 Ollama 协议");
            case Type::AzureOpenAI: return QStringLiteral("Azure OpenAI 协议");
            case Type::AmazonBedrock: return QStringLiteral("Amazon Bedrock 协议");
            default: return QStringLiteral("自定义协议");
            }
        }

        QString groupName(const domain::model::Model &model) {
            if (model.isCustom) return QObject::tr("自定义模型");
            if (!model.group.trimmed().isEmpty()) return model.group;
            if (!model.family.trimmed().isEmpty()) return model.family;
            return QObject::tr("其他模型");
        }
    }

    ProviderDetailView::ProviderDetailView(QWidget *parent) : QWidget(parent) {
        setupUi();
    }

    void ProviderDetailView::setupUi() {
        m_debounceTimer.setSingleShot(true);
        connect(&m_debounceTimer, &QTimer::timeout, this, [this] {
            if (m_hasProvider) Q_EMIT providerChanged(m_provider);
        });

        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(24, 16, 24, 20);
        mainLayout->setSpacing(12);

        auto *header = new QWidget(this);
        auto *headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(8);
        auto *titleColumn = new QVBoxLayout();
        titleColumn->setContentsMargins(0, 0, 0, 0);
        titleColumn->setSpacing(2);
        m_nameLabel = new fluent::textfields::Label(header);
        m_nameLabel->setFluentTypography(Typography::FontRole::Title);
        m_protocolLabel = new fluent::textfields::Label(header);
        m_protocolLabel->setFluentTypography(Typography::FontRole::Caption);
        m_protocolLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        titleColumn->addWidget(m_nameLabel);
        titleColumn->addWidget(m_protocolLabel);
        headerLayout->addLayout(titleColumn, 1);
        m_enableSwitch = new fluent::basicinput::ToggleSwitch(header);
        connect(m_enableSwitch, &fluent::basicinput::ToggleSwitch::toggled, this, [this](bool checked) {
            if (!m_hasProvider) return;
            m_provider.isEnabled = checked;
            Q_EMIT providerChanged(m_provider);
        });
        headerLayout->addWidget(m_enableSwitch, 0, Qt::AlignVCenter);
        m_closeBtn = new fluent::basicinput::Button(header);
        m_closeBtn->setFluentStyle(fluent::basicinput::Button::Subtle);
        m_closeBtn->setFluentLayout(fluent::basicinput::Button::IconOnly);
        m_closeBtn->setIconGlyph(Typography::Icons::ChromeClose);
        m_closeBtn->setFluentSize(fluent::basicinput::Button::Small);
        m_closeBtn->setToolTip(tr("关闭"));
        connect(m_closeBtn, &fluent::basicinput::Button::clicked, this, &ProviderDetailView::closeRequested);
        headerLayout->addWidget(m_closeBtn, 0, Qt::AlignVCenter);
        mainLayout->addWidget(header);

        auto *connectionSection = new QWidget(this);
        auto *connectionLayout = new QVBoxLayout(connectionSection);
        connectionLayout->setContentsMargins(0, 0, 0, 0);
        connectionLayout->setSpacing(8);
        auto *connectionTitle = new fluent::textfields::Label(tr("服务商连接配置"), connectionSection);
        connectionTitle->setFluentTypography(Typography::FontRole::BodyStrong);
        connectionLayout->addWidget(connectionTitle);
        auto *urlLabel = new fluent::textfields::Label(tr("API 基础地址 (Base URL)"), connectionSection);
        urlLabel->setFluentTypography(Typography::FontRole::Caption);
        urlLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
        m_urlEdit = new fluent::textfields::LineEdit(connectionSection);
        connect(m_urlEdit, &fluent::textfields::LineEdit::textChanged, this, [this](const QString &text) {
            if (!m_hasProvider) return;
            m_provider.baseUrl = text.trimmed();
            m_debounceTimer.start(350);
        });
        connectionLayout->addWidget(urlLabel);
        connectionLayout->addWidget(m_urlEdit);
        auto *keyRow = new QHBoxLayout();
        keyRow->setContentsMargins(0, 0, 0, 0);
        keyRow->setSpacing(8);
        m_keyEdit = new fluent::textfields::PasswordBox(connectionSection);
        m_keyEdit->setPlaceholderText(QStringLiteral("sk-..."));
        connect(m_keyEdit, &fluent::textfields::PasswordBox::textChanged, this, [this](const QString &text) {
            if (!m_hasProvider) return;
            m_provider.apiKey = text.trimmed();
            m_debounceTimer.start(350);
        });
        m_testBtn = new fluent::basicinput::Button(connectionSection);
        m_testBtn->setText(tr("检测"));
        connect(m_testBtn, &fluent::basicinput::Button::clicked, this, &ProviderDetailView::testConnection);
        keyRow->addWidget(m_keyEdit, 1);
        keyRow->addWidget(m_testBtn);
        connectionLayout->addWidget(new fluent::textfields::Label(tr("API 密钥 (API Key)"), connectionSection));
        connectionLayout->addLayout(keyRow);
        mainLayout->addWidget(connectionSection);

        auto *modelHeader = new QHBoxLayout();
        modelHeader->setContentsMargins(0, 0, 0, 0);
        m_modelCountLabel = new fluent::textfields::Label(this);
        m_modelCountLabel->setFluentTypography(Typography::FontRole::BodyStrong);
        m_actionButton = new ModelActionsSplitButton(this);
        connect(m_actionButton, &ModelActionsSplitButton::refreshRequested, this, [this] {
            if (m_hasProvider) Q_EMIT refreshModelsRequested(m_provider.id);
        });
        connect(m_actionButton, &ModelActionsSplitButton::addRequested, this, [this] {
            if (m_hasProvider) Q_EMIT addModelRequested(m_provider.id);
        });
        modelHeader->addWidget(m_modelCountLabel);
        modelHeader->addStretch(1);
        modelHeader->addWidget(m_actionButton);
        mainLayout->addLayout(modelHeader);

        m_modelTreeView = new fluent::collections::TreeView(this);
        m_modelTreeView->setBackgroundVisible(true);
        m_modelTreeView->setBorderVisible(false);
        m_modelTreeView->setHeaderHidden(true);
        m_modelTreeView->setUniformRowHeights(true);
        m_modelTreeView->setIndentation(16);
        m_modelTreeView->setSelectionIndicatorVisible(true);
        m_modelTreeView->setIndicatorMotionAnimationEnabled(true);
        m_modelTreeView->setScrollChainingEnabled(false);
        m_modelTreeView->setOverscrollEnabled(false);
        m_modelTreeView->setPlaceholderText(tr("没有可用模型"));
        m_modelTreeView->viewport()->setAutoFillBackground(false);
        m_modelTreeModel = new QStandardItemModel(m_modelTreeView);
        m_modelTreeView->setModel(m_modelTreeModel);
        connect(m_modelTreeModel, &QStandardItemModel::itemChanged, this, [this](QStandardItem *item) {
            if (m_syncingTree || !item) return;
            m_syncingTree = true;
            if (item->hasChildren()) setSubtreeCheckState(item, item->checkState());
            syncModelStatesFromTree();
            m_syncingTree = false;
            Q_EMIT providerChanged(m_provider);
        });
        mainLayout->addWidget(m_modelTreeView, 1);
    }

    void ProviderDetailView::setProvider(const std::optional<domain::model::ModelProvider> &provider) {
        m_debounceTimer.stop();
        m_hasProvider = provider.has_value();
        m_provider = provider.value_or(domain::model::ModelProvider{});
        const QSignalBlocker switchBlocker(m_enableSwitch);
        const QSignalBlocker urlBlocker(m_urlEdit);
        const QSignalBlocker keyBlocker(m_keyEdit);
        m_nameLabel->setText(m_hasProvider ? m_provider.name : QString());
        m_protocolLabel->setText(m_hasProvider ? protocolDisplayName(m_provider.type) : QString());
        m_enableSwitch->setIsOn(m_hasProvider && m_provider.isEnabled);
        m_urlEdit->setText(m_provider.baseUrl);
        m_keyEdit->setText(m_provider.apiKey);
        rebuildModelTree();
    }

    void ProviderDetailView::rebuildModelTree() {
        m_syncingTree = true;
        m_modelTreeModel->clear();
        QMap<QString, QStandardItem *> groups;
        for (const auto &model : m_provider.models) {
            const QString group = groupName(model);
            auto *groupItem = groups.value(group, nullptr);
            if (!groupItem) {
                groupItem = new QStandardItem(group);
                groupItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
                groups.insert(group, groupItem);
                m_modelTreeModel->appendRow(groupItem);
            }
            auto *modelItem = new QStandardItem(model.displayName.isEmpty() ? model.id : model.displayName);
            modelItem->setData(model.id, ModelIdRole);
            modelItem->setToolTip(model.id);
            modelItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            modelItem->setCheckState(model.isEnabled ? Qt::Checked : Qt::Unchecked);
            groupItem->appendRow(modelItem);
        }
        for (auto *group : groups) {
            int checked = 0;
            for (int row = 0; row < group->rowCount(); ++row)
                checked += group->child(row)->checkState() == Qt::Checked;
            group->setCheckState(checked == 0 ? Qt::Unchecked : checked == group->rowCount() ? Qt::Checked : Qt::PartiallyChecked);
        }
        m_modelCountLabel->setText(tr("可用模型 (%1)").arg(m_provider.models.size()));
        m_modelTreeView->expandAll();
        m_syncingTree = false;
    }

    void ProviderDetailView::setSubtreeCheckState(QStandardItem *item, Qt::CheckState state) {
        for (int row = 0; row < item->rowCount(); ++row) {
            auto *child = item->child(row);
            child->setCheckState(state);
            setSubtreeCheckState(child, state);
        }
    }

    void ProviderDetailView::syncModelStatesFromTree() {
        std::function<void(QStandardItem *)> visit = [this, &visit](QStandardItem *item) {
            for (int row = 0; row < item->rowCount(); ++row) {
                auto *child = item->child(row);
                const QString id = child->data(ModelIdRole).toString();
                if (!id.isEmpty()) {
                    for (auto &model : m_provider.models)
                        if (model.id == id) model.isEnabled = child->checkState() == Qt::Checked;
                }
                visit(child);
            }
        };
        visit(m_modelTreeModel->invisibleRootItem());
    }

    void ProviderDetailView::setRefreshing(bool refreshing) {
        if (m_actionButton) {
            m_actionButton->setRefreshing(refreshing);
        }
    }

    void ProviderDetailView::testConnection() {
        if (m_provider.baseUrl.isEmpty()) {
            fluent::status_info::Toast::showToast(this, tr("API 基础地址不能为空"), fluent::status_info::Toast::Warning);
            return;
        }
        m_testBtn->setEnabled(false);
        m_testBtn->setText(tr("正在检测..."));
        auto *client = new network::QtHttpClient(this);
        network::HttpRequest request;
        request.url = m_provider.baseUrl;
        request.method = network::HttpMethod::Get;
        request.timeoutMs = 8000;
        if (!m_provider.apiKey.isEmpty()) request.headers.insert(QStringLiteral("Authorization"), QStringLiteral("Bearer ") + m_provider.apiKey);
        auto *operation = client->send(request);
        connect(operation, &network::HttpOperation::finished, this, [this, client, operation] {
            m_testBtn->setEnabled(true); m_testBtn->setText(tr("检测"));
            fluent::status_info::Toast::showToast(this, tr("检测成功"), fluent::status_info::Toast::Success);
            operation->deleteLater(); client->deleteLater();
        });
        connect(operation, &network::HttpOperation::failed, this, [this, client, operation](const QString &error, int code) {
            m_testBtn->setEnabled(true); m_testBtn->setText(tr("检测"));
            fluent::status_info::Toast::showToast(this, tr("检测失败: %1 (代码 %2)").arg(error).arg(code), fluent::status_info::Toast::Error);
            operation->deleteLater(); client->deleteLater();
        });
    }

    void ProviderDetailView::paintEvent(QPaintEvent *) {
        QPainter painter(this);
        painter.fillRect(rect(), themeColorsRef().bgLayer);
    }

    void ProviderDetailView::onThemeUpdated() { update(); }

} // namespace ui::screen::settings::model_manager
