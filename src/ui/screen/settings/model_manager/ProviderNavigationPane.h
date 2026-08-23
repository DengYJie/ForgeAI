#pragma once
#include <QWidget>
#include <QList>
#include <QMap>
#include <FluentQt/FluentQt.h>
#include "domain/model/ModelProvider.h"
#include "ProviderNavigationItem.h"

namespace fluent::textfields {
    class LineEdit;
}
namespace fluent::basicinput {
    class Button;
}
namespace fluent::scrolling {
    class ScrollView;
}

namespace ui::screen::settings::model_manager {

    /**
     * @brief 服务商左侧导航面板
     * @details 提供服务商列表实时搜索过滤、选中切换以及添加服务商触发
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

    protected:
        void paintEvent(QPaintEvent *event) override;
        void onThemeUpdated() override;

    private:
        void setupUi();
        void filterProviders(const QString &keyword);

        fluent::textfields::LineEdit *m_searchBox = nullptr;
        fluent::scrolling::ScrollView *m_scrollView = nullptr;
        QWidget *m_listContainer = nullptr;
        fluent::basicinput::Button *m_addBtn = nullptr;

        QList<domain::model::ModelProvider> m_providers;
        QMap<QString, ProviderNavigationItem *> m_itemMap;
        QString m_currentSelectedId;
    };

} // namespace ui::screen::settings::model_manager
