#pragma once

#include <QPointer>
#include <QWidget>

#include <FluentQt/Foundation.h>

#include "ModelManagerViewModel.h"

class QPropertyAnimation;

namespace fluent::basicinput {
    class Button;
}

namespace ui::screen::settings::model_manager {

    class ProviderNavigationPane;
    class ProviderDetailView;

    /**
     * @brief 轻量的 Fluent 风格模态工作台。
     *
     * 采用 UDF 架构，仅通过 ModelManagerViewModel 进行状态单向驱动，
     * 同窗口常驻缓存复用，关闭时仅 hide() 遮罩与自身。
     */
    class ModelManagerWindow : public QWidget, public fluent::FluentElement {
        Q_OBJECT

    public:
        explicit ModelManagerWindow(
            ModelManagerViewModel *viewModel,
            QWidget *parent = nullptr
        );
        ~ModelManagerWindow() override = default;

        /**
         * @brief 打开/激活工作台（支持动态切换宿主父窗口）
         */
        void open(QWidget *parent = nullptr);

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;
        void hideEvent(QHideEvent *event) override;
        void keyPressEvent(QKeyEvent *event) override;
        void paintEvent(QPaintEvent *event) override;
        void resizeEvent(QResizeEvent *event) override;
        void onThemeUpdated() override;

    private:
        void renderState(const ModelManagerState &state);
        void onAddProvider();
        void onAddModel(const QString &providerId);
        void centerInOwner();
        QPoint centeredPosition() const;
        void updateSmokeGeometry();

        ModelManagerViewModel *m_viewModel = nullptr;

        QPointer<QWidget> m_owner;
        QWidget *m_smokeOverlay = nullptr;
        QPropertyAnimation *m_entranceAnimation = nullptr;
        ProviderNavigationPane *m_navPane = nullptr;
        ProviderDetailView *m_detailView = nullptr;
    };

} // namespace ui::screen::settings::model_manager

