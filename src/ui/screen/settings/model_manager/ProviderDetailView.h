#pragma once

#include <QTimer>
#include <QWidget>
#include <QVBoxLayout>

#include <FluentQt/FluentQt.h>

#include "domain/model/ModelProvider.h"

class QStandardItem;
class QStandardItemModel;
class QResizeEvent;

namespace fluent::basicinput { class ToggleSwitch; class Button; }
namespace fluent::collections { class TreeView; }
namespace fluent::scrolling { class ScrollView; }
namespace fluent::textfields { class Label; class LineEdit; class PasswordBox; }

namespace ui::screen::settings::model_manager {

    class ModelActionsSplitButton;

    class ProviderDetailView : public QWidget, public fluent::FluentElement {
        Q_OBJECT

    public:
        explicit ProviderDetailView(QWidget *parent = nullptr);
        void setProvider(const std::optional<domain::model::ModelProvider> &provider);
        const domain::model::ModelProvider &provider() const { return m_provider; }
        void setRefreshing(bool refreshing);

    Q_SIGNALS:
        void providerChanged(const domain::model::ModelProvider &provider);
        void providerDeleted(const QString &providerId);
        void refreshModelsRequested(const QString &providerId);
        void addModelRequested(const QString &providerId);

    protected:
        void resizeEvent(QResizeEvent *event) override;
        void onThemeUpdated() override;

    private:
        void setupUi();
        void updateMargins();
        void updateTreeHeight();
        void rebuildModelTree();
        void testConnection();

        domain::model::ModelProvider m_provider;
        bool m_hasProvider = false;
        bool m_syncingTree = false;
        QTimer m_debounceTimer;

        QWidget *m_headerWidget = nullptr;
        QHBoxLayout *m_headerLayout = nullptr;
        fluent::scrolling::ScrollView *m_scrollView = nullptr;
        QWidget *m_scrollContent = nullptr;
        QVBoxLayout *m_mainLayout = nullptr;
        fluent::textfields::Label *m_nameLabel = nullptr;
        fluent::textfields::Label *m_protocolLabel = nullptr;
        fluent::basicinput::ToggleSwitch *m_enableSwitch = nullptr;
        fluent::textfields::LineEdit *m_urlEdit = nullptr;
        fluent::textfields::PasswordBox *m_keyEdit = nullptr;
        fluent::basicinput::Button *m_testBtn = nullptr;
        fluent::textfields::Label *m_modelCountLabel = nullptr;
        ModelActionsSplitButton *m_actionButton = nullptr;
        fluent::collections::TreeView *m_modelTreeView = nullptr;
        QStandardItemModel *m_modelTreeModel = nullptr;
    };

} // namespace ui::screen::settings::model_manager
