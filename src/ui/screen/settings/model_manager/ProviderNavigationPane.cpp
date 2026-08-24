#include "ProviderNavigationPane.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QDebug>
#include <FluentQt/TextFields.h>
#include <FluentQt/BasicInput.h>
#include <FluentQt/Scrolling.h>

namespace ui::screen::settings::model_manager {

    ProviderNavigationPane::ProviderNavigationPane(QWidget *parent)
        : QWidget(parent) {
        setFixedWidth(240);
        setupUi();
    }

    void ProviderNavigationPane::setupUi() {
        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(12, 16, 12, 16);
        rootLayout->setSpacing(10);

        // 1. 顶部搜索框
        m_searchBox = new fluent::textfields::LineEdit(this);
        m_searchBox->setPlaceholderText(tr("搜索服务商..."));
        m_searchBox->setFixedHeight(34);
        connect(m_searchBox, &fluent::textfields::LineEdit::textChanged, this, &ProviderNavigationPane::filterProviders);
        rootLayout->addWidget(m_searchBox);

        // 2. 中间滚动列表
        m_scrollView = new fluent::scrolling::ScrollView(this);
        m_scrollView->setWidgetResizable(true);
        m_scrollView->setFrameShape(QFrame::NoFrame);
        m_scrollView->setHorizontalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);
        if (m_scrollView->viewport()) {
            QPalette pal = m_scrollView->viewport()->palette();
            pal.setColor(QPalette::Window, themeColorsRef().bgLayer);
            pal.setColor(QPalette::Base, themeColorsRef().bgLayer);
            m_scrollView->viewport()->setPalette(pal);
            m_scrollView->viewport()->setAutoFillBackground(true);
        }

        m_listContainer = new QWidget(m_scrollView);
        m_listContainer->setAutoFillBackground(false);
        auto *listLayout = new QVBoxLayout(m_listContainer);
        listLayout->setContentsMargins(0, 4, 0, 4);
        listLayout->setSpacing(2);
        listLayout->addStretch(1);

        m_scrollView->setWidget(m_listContainer);
        rootLayout->addWidget(m_scrollView, 1);

        // 3. 底部添加服务商按钮
        m_addBtn = new fluent::basicinput::Button(this);
        m_addBtn->setText(tr("+ 添加服务商"));
        m_addBtn->setFixedHeight(36);
        connect(m_addBtn, &fluent::basicinput::Button::clicked, this, &ProviderNavigationPane::addProviderRequested);
        rootLayout->addWidget(m_addBtn);
    }

    void ProviderNavigationPane::setProviders(const QList<domain::model::ModelProvider> &providers) {
        if (!shouldRebuildProviderItems(providers)) {
            qDebug().noquote() << "[ProviderNavigationPane] reuse provider items count=" << providers.size();
            m_providers = providers;
            for (const auto &provider : m_providers) {
                if (auto *item = m_itemMap.value(provider.id, nullptr)) {
                    item->setProvider(provider);
                }
            }
            if (!m_currentSelectedId.isEmpty()) {
                selectProvider(m_currentSelectedId);
            }
            filterProviders(m_searchBox ? m_searchBox->text() : QString());
            return;
        }

        qDebug().noquote() << "[ProviderNavigationPane] rebuild provider items count=" << providers.size();
        m_providers = providers;
        m_itemMap.clear();

        auto *listLayout = qobject_cast<QVBoxLayout *>(m_listContainer->layout());
        if (!listLayout) return;

        // 清除旧 item
        while (QLayoutItem *item = listLayout->takeAt(0)) {
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            delete item;
        }

        for (const auto &provider : m_providers) {
            auto *navItem = new ProviderNavigationItem(provider, m_listContainer);
            m_itemMap.insert(provider.id, navItem);
            listLayout->addWidget(navItem);

            connect(navItem, &ProviderNavigationItem::clicked, this, [this](const QString &id) {
                selectProvider(id);
                Q_EMIT providerSelected(id);
            });
        }

        listLayout->addStretch(1);

        if (!m_providers.isEmpty()) {
            if (m_currentSelectedId.isEmpty() || !m_itemMap.contains(m_currentSelectedId)) {
                selectProvider(m_providers.first().id);
            } else {
                selectProvider(m_currentSelectedId);
            }
        }
    }

    bool ProviderNavigationPane::shouldRebuildProviderItems(const QList<domain::model::ModelProvider> &providers) const {
        if (providers.size() != m_providers.size()) return true;
        for (int i = 0; i < providers.size(); ++i) {
            if (providers.at(i).id != m_providers.at(i).id) return true;
        }
        return false;
    }

    void ProviderNavigationPane::selectProvider(const QString &providerId) {
        m_currentSelectedId = providerId;
        for (auto it = m_itemMap.begin(); it != m_itemMap.end(); ++it) {
            it.value()->setSelected(it.key() == providerId);
        }
    }

    void ProviderNavigationPane::filterProviders(const QString &keyword) {
        QString lowerKw = keyword.trimmed().toLower();
        for (auto it = m_itemMap.begin(); it != m_itemMap.end(); ++it) {
            bool matches = lowerKw.isEmpty() ||
                           it.value()->provider().name.toLower().contains(lowerKw) ||
                           it.value()->provider().id.toLower().contains(lowerKw);
            it.value()->setVisible(matches);
        }
    }

    void ProviderNavigationPane::paintEvent(QPaintEvent *) {
        QPainter painter(this);
        const auto &colors = themeColorsRef();

        // 与工作台 surface 共用 bgLayer；导航只额外绘制轻量分隔线。
        painter.setPen(colors.strokeDivider);
        painter.drawLine(rect().topRight(), rect().bottomRight());
    }

    void ProviderNavigationPane::onThemeUpdated() {
        if (m_scrollView && m_scrollView->viewport()) {
            QPalette pal = m_scrollView->viewport()->palette();
            pal.setColor(QPalette::Window, themeColorsRef().bgLayer);
            pal.setColor(QPalette::Base, themeColorsRef().bgLayer);
            m_scrollView->viewport()->setPalette(pal);
            m_scrollView->viewport()->setAutoFillBackground(true);
        }
        update();
    }

} // namespace ui::screen::settings::model_manager
