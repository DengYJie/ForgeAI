#include "ProviderNavigationPane.h"

#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <QDebug>

#include <FluentQt/BasicInput.h>
#include <FluentQt/Collections.h>
#include <FluentQt/MenusToolbars.h>
#include <FluentQt/TextFields.h>

#include "ProviderFilterProxyModel.h"
#include "ProviderItemDelegate.h"
#include "ProviderListModel.h"

namespace ui::screen::settings::model_manager {

    ProviderNavigationPane::ProviderNavigationPane(QWidget* parent)
        : QWidget(parent) {
        setFixedWidth(240);
        setupUi();
    }

    void ProviderNavigationPane::setupUi() {
        auto* rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);

        // 1. ListView 容器
        m_listView = new fluent::collections::ListView(this);
        m_listView->setBackgroundVisible(false);
        m_listView->setBorderVisible(false);
        m_listView->setSelectionMode(fluent::collections::SelectionMode::Single);
        m_listView->setSelectedIndicatorAnimationEnabled(true);
        m_listView->setSelectionIndicatorVisible(true);
        m_listView->setPlaceholderText(tr("未找到匹配的服务商"));
        m_listView->viewport()->setAutoFillBackground(false);
        m_listView->setProperty("fluentPreserveParentSurface", true);
        m_listView->viewport()->installEventFilter(this);

        // 2. 顶部搜索框 (作为 ListView Header)
        auto* headerWidget = new QWidget(m_listView);
        auto* headerLayout = new QVBoxLayout(headerWidget);
        headerLayout->setContentsMargins(12, 12, 12, 8);
        headerLayout->setSpacing(0);

        m_searchBox = new fluent::textfields::LineEdit(headerWidget);
        m_searchBox->setPlaceholderText(tr("搜索服务商..."));
        m_searchBox->setFixedHeight(34);
        connect(m_searchBox, &fluent::textfields::LineEdit::textChanged, this, [this](const QString& text) {
            if (m_proxyModel) {
                m_proxyModel->setSearchKeyword(text);
                if (!m_currentSelectedId.isEmpty()) {
                    selectProvider(m_currentSelectedId);
                }
            }
            });
        headerLayout->addWidget(m_searchBox);
        m_listView->setHeader(headerWidget);

        // 3. 底部添加服务商按钮
        auto* footerWidget = new QWidget(m_listView);
        auto* footerLayout = new QVBoxLayout(footerWidget);
        footerLayout->setContentsMargins(12, 8, 12, 12);
        footerLayout->setSpacing(0);

        m_addBtn = new fluent::basicinput::Button(footerWidget);
        m_addBtn->setText(tr("+ 添加服务商"));
        m_addBtn->setFixedHeight(36);
        connect(m_addBtn, &fluent::basicinput::Button::clicked, this, &ProviderNavigationPane::addProviderRequested);
        footerLayout->addWidget(m_addBtn);
        m_listView->setFooter(footerWidget);

        // 4. Model / Proxy / Delegate 装配
        m_listModel = new ProviderListModel(this);
        m_proxyModel = new ProviderFilterProxyModel(this);
        m_proxyModel->setSourceModel(m_listModel);

        m_itemDelegate = new ProviderItemDelegate(this);

        m_listView->setModel(m_proxyModel);
        m_listView->setItemDelegate(m_itemDelegate);

        connect(m_listView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &ProviderNavigationPane::onSelectionChanged);

        rootLayout->addWidget(m_listView);
    }

    bool ProviderNavigationPane::eventFilter(QObject* watched, QEvent* event) {
        if (m_listView && watched == m_listView->viewport()) {
            if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
                auto* mouseEvent = static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() == Qt::LeftButton) {
                    const QPoint pos = mouseEvent->pos();
                    const QModelIndex idx = m_listView->indexAt(pos);
                    if (idx.isValid()) {
                        const QRect itemRect = static_cast<const QAbstractItemView *>(m_listView)->visualRect(idx);
                        const QRect btnRect(itemRect.right() - 28, itemRect.top() + (itemRect.height() - 24) / 2, 24, 24);
                        if (btnRect.contains(pos)) {
                            if (event->type() == QEvent::MouseButtonRelease) {
                                const QString providerId = idx.data(ProviderIdRole).toString();
                                const QPoint btnBottomRight = m_listView->viewport()->mapToGlobal(btnRect.bottomRight());
                                showProviderContextMenu(providerId, btnBottomRight);
                            }
                            return true;
                        }
                    }
                }
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void ProviderNavigationPane::setProviders(const QList<domain::model::ModelProvider>& providers) {
        if (!m_listModel) return;

        qInfo().noquote() << QStringLiteral("[ProviderNavigationPane] setProviders: 传入 %1 个服务商").arg(providers.size());
        m_listModel->setProviders(providers);

        if (!m_currentSelectedId.isEmpty()) {
            selectProvider(m_currentSelectedId);
        }
        else if (!providers.isEmpty()) {
            selectProvider(providers.first().id);
        }
    }

    void ProviderNavigationPane::selectProvider(const QString& providerId) {
        if (!m_listModel || !m_proxyModel || !m_listView) return;

        const int srcRow = m_listModel->rowForProvider(providerId);
        if (srcRow < 0) return;

        const QModelIndex srcIdx = m_listModel->index(srcRow, 0);
        const QModelIndex proxyIdx = m_proxyModel->mapFromSource(srcIdx);

        if (proxyIdx.isValid()) {
            m_currentSelectedId = providerId;
            m_listView->setCurrentIndex(proxyIdx);
            m_listView->scrollTo(proxyIdx);
        }
    }

    void ProviderNavigationPane::onSelectionChanged() {
        if (!m_listView || !m_proxyModel) return;

        const QModelIndex current = m_listView->currentIndex();
        if (!current.isValid()) return;

        const QString providerId = current.data(ProviderIdRole).toString();
        if (!providerId.isEmpty() && providerId != m_currentSelectedId) {
            m_currentSelectedId = providerId;
            Q_EMIT providerSelected(providerId);
        }
    }

    void ProviderNavigationPane::showProviderContextMenu(const QString& providerId, const QPoint& globalPos) {
        Q_EMIT providerMenuRequested(providerId, globalPos);

        if (!m_listModel) return;
        const auto providerOpt = m_listModel->findProvider(providerId);
        if (!providerOpt.has_value()) return;

        auto* menu = new fluent::menus_toolbars::FluentMenu(QString(), this);
        const auto& provider = providerOpt.value();

        auto* toggleAction = menu->addAction(provider.isEnabled ? tr("停用服务商") : tr("启用服务商"));
        connect(toggleAction, &QAction::triggered, this, [this, providerId] {
            selectProvider(providerId);
        });

        const int menuWidth = menu->sizeHint().width();
        const QPoint rightAlignedPos(globalPos.x() - menuWidth, globalPos.y() + 4);

        menu->exec(rightAlignedPos);
        menu->deleteLater();
    }

    void ProviderNavigationPane::paintEvent(QPaintEvent*) {
        QPainter painter(this);
        const auto& colors = themeColorsRef();

        // 绘制右侧轻量分隔线
        painter.setPen(colors.strokeDivider);
        painter.drawLine(rect().topRight(), rect().bottomRight());
    }

    void ProviderNavigationPane::onThemeUpdated() {
        update();
    }

} // namespace ui::screen::settings::model_manager
