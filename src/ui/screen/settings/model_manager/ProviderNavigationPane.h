#pragma once

#include <QWidget>
#include <QList>
#include <FluentQt/FluentQt.h>
#include "domain/model/ModelProvider.h"

namespace fluent::textfields {
    class LineEdit;
}
namespace fluent::basicinput {
    class Button;
}
namespace fluent::collections {
    class ListView;
}

namespace ui::screen::settings::model_manager {

    class ProviderListModel;
    class ProviderFilterProxyModel;
    class ProviderItemDelegate;

    /**
     * @brief 服务商左侧导航面板 (基于 FluentQt ListView 体系)
     * @details 提供服务商列表实时搜索过滤、选中切换、平滑指示器动效以及添加服务商触发
     */
    class ProviderNavigationPane : public QWidget, public fluent::FluentElement {
        Q_OBJECT

    public:
        explicit ProviderNavigationPane(QWidget *parent = nullptr);
        ~ProviderNavigationPane() override = default;

        void setProviders(const QList<domain::model::ModelProvider> &providers);
        void selectProvider(const QString &providerId);
        QString currentSelectedId() const { return m_currentSelectedId; }

    Q_SIGNALS:
        void providerSelected(const QString &providerId);
        void addProviderRequested();
        void providerMenuRequested(const QString &providerId, const QPoint &globalPos);

    protected:
        void paintEvent(QPaintEvent *event) override;
        bool eventFilter(QObject *watched, QEvent *event) override;
        void onThemeUpdated() override;

    private:
        void setupUi();
        void onSelectionChanged();
        void showProviderContextMenu(const QString &providerId, const QPoint &globalPos);

        fluent::textfields::LineEdit *m_searchBox = nullptr;
        fluent::collections::ListView *m_listView = nullptr;
        fluent::basicinput::Button *m_addBtn = nullptr;

        ProviderListModel *m_listModel = nullptr;
        ProviderFilterProxyModel *m_proxyModel = nullptr;
        ProviderItemDelegate *m_itemDelegate = nullptr;

        QString m_currentSelectedId;
    };

} // namespace ui::screen::settings::model_manager
