#pragma once
#include <QWidget>
#include <FluentQt/FluentQt.h>
#include "domain/model/ModelProvider.h"
#include "ModelItemCard.h"

namespace fluent::basicinput {
    class ToggleSwitch;
    class Button;
}
namespace fluent::textfields {
    class LineEdit;
    class PasswordBox;
    class Label;
}
namespace fluent::scrolling {
    class ScrollView;
}

namespace ui::screen::settings::model_manager {

    /**
     * @brief 服务商详情与模型管理视图
     * @details 展示当前选中服务商的连接配置、API 凭证与已注册模型列表，支持启停与动态刷新
     */
    class ProviderDetailView : public QWidget, public fluent::FluentElement {
        Q_OBJECT

    public:
        explicit ProviderDetailView(QWidget *parent = nullptr);
        ~ProviderDetailView() override = default;

        void setProvider(const domain::model::ModelProvider &provider);
        const domain::model::ModelProvider &provider() const { return m_provider; }

        void setRefreshing(bool refreshing);

    Q_SIGNALS:
        void providerChanged(const domain::model::ModelProvider &provider);
        void providerDeleted(const QString &providerId);
        void refreshModelsRequested(const QString &providerId);
        void addModelRequested(const QString &providerId);

    protected:
        void onThemeUpdated() override;

    private:
        void setupUi();
        void updateModelListUi();
        void testConnection();

        domain::model::ModelProvider m_provider;

        fluent::textfields::Label *m_nameLabel = nullptr;
        fluent::textfields::Label *m_protocolLabel = nullptr;
        fluent::basicinput::ToggleSwitch *m_enableSwitch = nullptr;
        fluent::basicinput::Button *m_deleteProviderBtn = nullptr;

        fluent::textfields::LineEdit *m_urlEdit = nullptr;
        fluent::textfields::PasswordBox *m_keyEdit = nullptr;
        fluent::basicinput::Button *m_testBtn = nullptr;

        fluent::textfields::Label *m_modelCountLabel = nullptr;
        fluent::basicinput::Button *m_refreshBtn = nullptr;
        fluent::basicinput::Button *m_addModelBtn = nullptr;
        QWidget *m_modelListContainer = nullptr;
    };

} // namespace ui::screen::settings::model_manager
