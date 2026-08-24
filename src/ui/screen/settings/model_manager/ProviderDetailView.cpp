#include "ProviderDetailView.h"

#include <QFile>
#include <QIcon>
#include <FluentQt/BasicInput.h>
#include <FluentQt/Collections.h>
#include <FluentQt/Design.h>
#include <FluentQt/Layout.h>
#include <FluentQt/Scrolling.h>
#include <FluentQt/StatusInfo.h>
#include <FluentQt/TextFields.h>

#include "ModelActionsSplitButton.h"
#include "ModelTreeItemDelegate.h"
#include "domain/model/ModelCapabilities.h"
#include "network/QtHttpClient.h"

namespace ui::screen::settings::model_manager {

    namespace {
        constexpr int kMaxContentWidth = 1064;

        class TransparentScrollView : public fluent::scrolling::ScrollView {
        public:
            explicit TransparentScrollView(QWidget *parent = nullptr)
                : fluent::scrolling::ScrollView(parent) {
                setWidgetResizable(true);
                setFrameShape(QFrame::NoFrame);
                setHorizontalScrollMode(fluent::scrolling::ScrollView::ScrollMode::Disabled);
                setHorizontalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);
                applyTransparentViewport();
            }

            void onThemeUpdated() override {
                fluent::scrolling::ScrollView::onThemeUpdated();
                applyTransparentViewport();
            }

        private:
            void applyTransparentViewport() {
                setAutoFillBackground(false);
                setAttribute(Qt::WA_NoSystemBackground, true);
                setAttribute(Qt::WA_TranslucentBackground, true);

                if (auto *vp = viewport()) {
                    vp->setAutoFillBackground(false);
                    vp->setAttribute(Qt::WA_NoSystemBackground, true);
                    vp->setAttribute(Qt::WA_TranslucentBackground, true);
                    QPalette pal = vp->palette();
                    pal.setColor(QPalette::Window, Qt::transparent);
                    pal.setColor(QPalette::Base, Qt::transparent);
                    vp->setPalette(pal);
                }
            }
        };

        QString groupName(const domain::model::Model &model) {
            if (model.isCustom) return QObject::tr("自定义模型");
            if (!model.group.trimmed().isEmpty()) return model.group;
            if (!model.family.trimmed().isEmpty()) return model.family;
            return QObject::tr("其他模型");
        }
    } // namespace

    ProviderDetailView::ProviderDetailView(QWidget *parent) : QWidget(parent) {
        setupUi();
    }

    void ProviderDetailView::setupUi() {
        m_debounceTimer.setSingleShot(true);
        connect(&m_debounceTimer, &QTimer::timeout, this, [this] {
            if (m_hasProvider) Q_EMIT providerChanged(m_provider);
        });

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);

        // 1. 固定顶部标题区
        m_headerWidget = new QWidget(this);
        m_headerLayout = new QHBoxLayout(m_headerWidget);
        m_headerLayout->setContentsMargins(32, 24, 32, 8);
        m_headerLayout->setSpacing(12);

        m_nameLabel = new fluent::textfields::Label(m_headerWidget);
        m_nameLabel->setFluentTypography(Typography::FontRole::Title);
        m_nameLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);

        m_headerLayout->addWidget(m_nameLabel, 1, Qt::AlignVCenter);

        m_enableSwitch = new fluent::basicinput::ToggleSwitch(m_headerWidget);
        connect(m_enableSwitch, &fluent::basicinput::ToggleSwitch::toggled, this, [this](bool checked) {
            if (!m_hasProvider) return;
            m_provider.isEnabled = checked;
            Q_EMIT providerChanged(m_provider);
        });
        m_headerLayout->addWidget(m_enableSwitch, 0, Qt::AlignVCenter);

        rootLayout->addWidget(m_headerWidget);

        // 2. 下方透明可滚动内容区
        m_scrollView = new TransparentScrollView(this);

        m_scrollContent = new QWidget(m_scrollView);
        m_scrollContent->setAutoFillBackground(false);
        m_scrollContent->setAttribute(Qt::WA_TranslucentBackground, true);

        m_mainLayout = new QVBoxLayout(m_scrollContent);
        m_mainLayout->setContentsMargins(32, 8, 32, 24);
        m_mainLayout->setSpacing(24);

        m_scrollView->setWidget(m_scrollContent);
        rootLayout->addWidget(m_scrollView, 1);

        // 3. 连接配置卡片
        auto *connectionCard = new fluent::layout::Card(m_scrollContent);
        connectionCard->setObjectName(QStringLiteral("providerConnectionCard"));
        connectionCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto *connectionLayout = new QVBoxLayout(connectionCard);
        connectionLayout->setContentsMargins(20, 16, 20, 16);
        connectionLayout->setSpacing(12);

        const auto spacing = themeSpacing();
        const int controlHeight = spacing.controlHeight.standard;

        auto *urlLabel = new fluent::textfields::Label(tr("API 基础地址 (Base URL)"), connectionCard);
        urlLabel->setFluentTypography(Typography::FontRole::Caption);
        urlLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);

        m_urlEdit = new fluent::textfields::LineEdit(connectionCard);
        m_urlEdit->setPlaceholderText(QStringLiteral("https://api.openai.com/v1"));
        m_urlEdit->setFixedHeight(controlHeight);
        connect(m_urlEdit, &fluent::textfields::LineEdit::textChanged, this, [this](const QString &text) {
            if (!m_hasProvider) return;
            m_provider.baseUrl = text.trimmed();
            m_debounceTimer.start(350);
        });

        auto *keyLabel = new fluent::textfields::Label(tr("API 密钥 (API Key)"), connectionCard);
        keyLabel->setFluentTypography(Typography::FontRole::Caption);
        keyLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);

        auto *keyRow = new QHBoxLayout();
        keyRow->setContentsMargins(0, 0, 0, 0);
        keyRow->setSpacing(8);

        m_keyEdit = new fluent::textfields::PasswordBox(connectionCard);
        m_keyEdit->setPlaceholderText(QStringLiteral("sk-..."));
        m_keyEdit->setFixedHeight(controlHeight);
        connect(m_keyEdit, &fluent::textfields::PasswordBox::textChanged, this, [this](const QString &text) {
            if (!m_hasProvider) return;
            m_provider.apiKey = text.trimmed();
            m_debounceTimer.start(350);
        });

        m_testBtn = new fluent::basicinput::Button(connectionCard);
        m_testBtn->setText(tr("检测"));
        m_testBtn->setFixedHeight(controlHeight);
        m_testBtn->setFixedWidth(80);
        m_testBtn->setFluentStyle(fluent::basicinput::Button::Standard);
        connect(m_testBtn, &fluent::basicinput::Button::clicked, this, &ProviderDetailView::testConnection);

        keyRow->addWidget(m_keyEdit, 1);
        keyRow->addWidget(m_testBtn, 0);

        connectionLayout->addWidget(urlLabel);
        connectionLayout->addWidget(m_urlEdit);
        connectionLayout->addSpacing(4);
        connectionLayout->addWidget(keyLabel);
        connectionLayout->addLayout(keyRow);

        m_mainLayout->addWidget(connectionCard);

        // 4. 模型列表区
        auto *modelHeader = new QHBoxLayout();
        modelHeader->setContentsMargins(0, 0, 0, 0);
        modelHeader->setSpacing(8);

        m_modelCountLabel = new fluent::textfields::Label(m_scrollContent);
        m_modelCountLabel->setFluentTypography(Typography::FontRole::BodyStrong);
        m_modelCountLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);

        m_actionButton = new ModelActionsSplitButton(m_scrollContent);
        connect(m_actionButton, &ModelActionsSplitButton::refreshRequested, this, [this] {
            if (m_hasProvider) Q_EMIT refreshModelsRequested(m_provider.id);
        });
        connect(m_actionButton, &ModelActionsSplitButton::addRequested, this, [this] {
            if (m_hasProvider) Q_EMIT addModelRequested(m_provider.id);
        });

        modelHeader->addWidget(m_modelCountLabel);
        modelHeader->addStretch(1);
        modelHeader->addWidget(m_actionButton);
        m_mainLayout->addLayout(modelHeader);

        m_modelTreeView = new fluent::collections::TreeView(m_scrollContent);
        m_modelTreeView->setBackgroundVisible(false);
        m_modelTreeView->setBorderVisible(false);
        m_modelTreeView->setHeaderHidden(true);
        m_modelTreeView->setRootIsDecorated(false);
        m_modelTreeView->setUniformRowHeights(false);
        m_modelTreeView->setIndentation(0);
        m_modelTreeView->setSelectionMode(fluent::collections::SelectionMode::None);
        m_modelTreeView->setFocusPolicy(Qt::NoFocus);
        m_modelTreeView->setSelectionIndicatorVisible(false);
        m_modelTreeView->setIndicatorMotionAnimationEnabled(false);
        m_modelTreeView->setScrollChainingEnabled(true);
        m_modelTreeView->setOverscrollEnabled(false);
        m_modelTreeView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_modelTreeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_modelTreeView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_modelTreeView->setPlaceholderText(tr("没有可用模型"));
        m_modelTreeView->viewport()->setAutoFillBackground(false);
        m_modelTreeView->setProperty("fluentPreserveParentSurface", true);

        auto *treeDelegate = new ModelTreeItemDelegate(m_modelTreeView, this);
        m_modelTreeView->setItemDelegate(treeDelegate);

        connect(m_modelTreeView, &QTreeView::clicked, this, [this](const QModelIndex &index) {
            if (!index.parent().isValid()) {
                m_modelTreeView->toggleExpanded(index);
            }
        });
        connect(m_modelTreeView, &fluent::collections::TreeView::expanded, this, &ProviderDetailView::updateTreeHeight);
        connect(m_modelTreeView, &fluent::collections::TreeView::collapsed, this, &ProviderDetailView::updateTreeHeight);

        m_modelTreeModel = new QStandardItemModel(m_modelTreeView);
        m_modelTreeView->setModel(m_modelTreeModel);

        m_mainLayout->addWidget(m_modelTreeView);
        m_mainLayout->addStretch(1);
        updateMargins();
    }

    void ProviderDetailView::updateTreeHeight() {
        if (!m_modelTreeView || !m_modelTreeModel) return;
        const int rootCount = m_modelTreeModel->rowCount();
        if (rootCount == 0) {
            m_modelTreeView->setFixedHeight(100);
            return;
        }
        const auto spacing = themeSpacing();
        int totalHeight = 0;
        for (int i = 0; i < rootCount; ++i) {
            const QModelIndex groupIdx = m_modelTreeModel->index(i, 0);
            totalHeight += spacing.controlHeight.large + spacing.xSmall;
            if (m_modelTreeView->isExpanded(groupIdx)) {
                const int childCount = m_modelTreeModel->rowCount(groupIdx);
                for (int c = 0; c < childCount; ++c) {
                    const bool isLast = (c == childCount - 1);
                    totalHeight += isLast ? (spacing.controlHeight.large + 2) : spacing.controlHeight.large;
                }
            }
            totalHeight += spacing.gap.tight;
        }
        m_modelTreeView->setFixedHeight(totalHeight + spacing.medium);
    }

    void ProviderDetailView::updateMargins() {
        const int w = width();
        const int marginH = (w > kMaxContentWidth + 64)
                                ? (w - kMaxContentWidth) / 2
                                : (w < 600 ? 16 : 32);
        if (m_headerLayout) {
            m_headerLayout->setContentsMargins(marginH, 24, marginH, 8);
        }
        if (m_mainLayout) {
            m_mainLayout->setContentsMargins(marginH, 8, marginH, 24);
        }
    }

    void ProviderDetailView::resizeEvent(QResizeEvent *event) {
        QWidget::resizeEvent(event);
        updateMargins();
    }

    void ProviderDetailView::setProvider(const std::optional<domain::model::ModelProvider> &provider) {
        m_debounceTimer.stop();
        m_hasProvider = provider.has_value();
        m_provider = provider.value_or(domain::model::ModelProvider{});
        const QSignalBlocker switchBlocker(m_enableSwitch);
        const QSignalBlocker urlBlocker(m_urlEdit);
        const QSignalBlocker keyBlocker(m_keyEdit);
        m_nameLabel->setText(m_hasProvider ? m_provider.name : QString());
        m_enableSwitch->setIsOn(m_hasProvider && m_provider.isEnabled);
        m_urlEdit->setText(m_provider.baseUrl);
        m_keyEdit->setText(m_provider.apiKey);
        rebuildModelTree();
    }

    void ProviderDetailView::rebuildModelTree() {
        m_syncingTree = true;
        m_modelTreeModel->clear();
        QMap<QString, QStandardItem *> groups;

        QIcon brandIcon;
        if (!m_provider.icon.isEmpty() && QFile::exists(m_provider.icon)) {
            brandIcon = QIcon(m_provider.icon);
        }

        for (const auto &model : m_provider.models) {
            const QString group = groupName(model);
            auto *groupItem = groups.value(group, nullptr);
            if (!groupItem) {
                groupItem = new QStandardItem(group);
                groupItem->setData(true, IsGroupRole);
                groupItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                groups.insert(group, groupItem);
                m_modelTreeModel->appendRow(groupItem);
            }
            auto *modelItem = new QStandardItem(model.displayName.isEmpty() ? model.id : model.displayName);
            modelItem->setData(false, IsGroupRole);
            modelItem->setData(model.id, ModelIdRole);
            modelItem->setData(static_cast<int>(model.capabilities), ModelCapabilitiesRole);
            if (!brandIcon.isNull()) {
                modelItem->setData(brandIcon, BrandIconRole);
            }
            modelItem->setToolTip(model.id);
            modelItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            groupItem->appendRow(modelItem);
        }

        m_modelCountLabel->setText(tr("可用模型 (%1)").arg(m_provider.models.size()));
        m_modelTreeView->expandAll();
        updateTreeHeight();
        m_syncingTree = false;
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

    void ProviderDetailView::onThemeUpdated() { update(); }

} // namespace ui::screen::settings::model_manager
