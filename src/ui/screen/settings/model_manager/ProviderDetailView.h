#pragma once

#include <QTimer>
#include <QWidget>
#include <QVBoxLayout>

#include <FluentQt/FluentQt.h>

#include "domain/model/ModelProvider.h"
#include "domain/model/ResolvedModel.h"

class QStandardItem;
class QStandardItemModel;
class QResizeEvent;

namespace fluent::basicinput { class ToggleSwitch; class Button; }
namespace fluent::scrolling { class ScrollView; }
namespace fluent::textfields { class Label; class LineEdit; class PasswordBox; }
namespace ui::widget::tree { class AutoHeightTreeView; }

namespace ui::screen::settings::model_manager {

    class ModelActionsSplitButton;

    class ProviderDetailView : public QWidget, public fluent::FluentElement {
        Q_OBJECT

    public:
        explicit ProviderDetailView(QWidget *parent = nullptr);
        void setProvider(const std::optional<domain::model::ModelProvider> &provider);
        void setProviderData(const std::optional<domain::model::ModelProvider> &provider, const QList<domain::model::ResolvedModel> &models = {});
        const domain::model::ModelProvider &provider() const { return m_provider; }
        void setRefreshing(bool refreshing);
        void setTestingConnection(bool testing);

    Q_SIGNALS:
        void baseUrlEditRequested(const QString &providerId, const QString &baseUrl);
        void apiKeyEditRequested(const QString &providerId, const QString &apiKey);
        void enabledChangeRequested(const QString &providerId, bool enabled);
        void providerDeleted(const QString &providerId);
        void refreshModelsRequested(const QString &providerId);
        void addModelRequested(const QString &providerId);
        void testConnectionRequested(const QString &providerId, const QString &baseUrl, const QString &apiKey);

    protected:
        void resizeEvent(QResizeEvent *event) override;
        void onThemeUpdated() override;

    private:
        void setupUi();
        void updateMargins();
        void rebuildModelTree();
        void testConnection();

        domain::model::ModelProvider m_provider;
        QList<domain::model::ResolvedModel> m_models;
        bool m_hasProvider = false;
        bool m_syncingTree = false;
        bool m_testingConnection = false;
        
        // View-local edit state for debouncing
        QString m_pendingBaseUrl;
        QString m_pendingApiKey;
        bool m_baseUrlDirty = false;
        bool m_apiKeyDirty = false;
        QTimer m_debounceTimer;

        QWidget *m_headerWidget = nullptr;
        QHBoxLayout *m_headerLayout = nullptr;
        fluent::scrolling::ScrollView *m_scrollView = nullptr;
        QWidget *m_scrollContent = nullptr;
        QVBoxLayout *m_mainLayout = nullptr;
        fluent::textfields::Label *m_nameLabel = nullptr;
        fluent::basicinput::ToggleSwitch *m_enableSwitch = nullptr;
        fluent::textfields::LineEdit *m_urlEdit = nullptr;
        fluent::textfields::PasswordBox *m_keyEdit = nullptr;
        fluent::basicinput::Button *m_testBtn = nullptr;
        fluent::textfields::Label *m_modelCountLabel = nullptr;
        ModelActionsSplitButton *m_actionButton = nullptr;
        ui::widget::tree::AutoHeightTreeView *m_modelTreeView = nullptr;
        QStandardItemModel *m_modelTreeModel = nullptr;
    };

} // namespace ui::screen::settings::model_manager
