#include "ModelManagerWindow.h"

#include "AddModelDialog.h"
#include "AddProviderDialog.h"
#include "ProviderDetailView.h"
#include "ProviderNavigationPane.h"

#include <QApplication>
#include <QEvent>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <functional>
#include <utility>

#include <FluentQt/BasicInput.h>
#include <FluentQt/StatusInfo.h>
#include <FluentQt/TextFields.h>

namespace ui::screen::settings::model_manager {

    namespace {
        constexpr QSize kWorkspaceSize(928, 648);

        class SmokeOverlayWidget final : public QWidget, public fluent::FluentElement {
        public:
            SmokeOverlayWidget(QWidget *parent, std::function<void()> dismiss)
                : QWidget(parent), m_dismiss(std::move(dismiss)) {}

        protected:
            void mousePressEvent(QMouseEvent *event) override {
                if (m_dismiss) {
                    m_dismiss();
                }
                event->accept();
            }

            void paintEvent(QPaintEvent *) override {
                QPainter painter(this);
                const auto smoke = themeSmoke();
                QColor color = smoke.baseColor;
                color.setAlphaF(smoke.opacity);
                painter.fillRect(rect(), color);
            }

            void onThemeUpdated() override { update(); }

        private:
            std::function<void()> m_dismiss;
        };
    } // namespace

    ModelManagerWindow::ModelManagerWindow(
        ModelManagerViewModel *viewModel,
        QWidget *parent
    ) : QWidget(nullptr),
        m_viewModel(viewModel),
        m_owner(parent ? parent->window() : qApp->activeWindow()) {
        if (m_owner) {
            setParent(m_owner);
        }

        setAttribute(Qt::WA_TranslucentBackground);
        setWindowFlags(Qt::Widget | Qt::FramelessWindowHint);
        setFocusPolicy(Qt::StrongFocus);
        setFixedSize(kWorkspaceSize);

        auto *rootLayout = new QHBoxLayout(this);
        // 给抗锯齿的 overlay 圆角留出 surface 边缘；子控件不再覆盖角像素。
        rootLayout->setContentsMargins(8, 8, 8, 8);
        rootLayout->setSpacing(0);

        m_navPane = new ProviderNavigationPane(this);
        m_navPane->setFixedWidth(240);
        m_detailView = new ProviderDetailView(this);
        rootLayout->addWidget(m_navPane);
        rootLayout->addWidget(m_detailView, 1);

        connect(m_detailView, &ProviderDetailView::closeRequested, this, &QWidget::hide);

        if (m_viewModel) {
            m_viewModel->observe(this, &ModelManagerWindow::renderState);

            connect(m_viewModel, &ModelManagerViewModel::toastRequested, this, [this](const QString &msg, bool isErr) {
                fluent::status_info::Toast::showToast(
                    this, msg,
                    isErr ? fluent::status_info::Toast::Error : fluent::status_info::Toast::Success
                );
            });
        }

        // UI Intents -> ViewModel
        connect(m_navPane, &ProviderNavigationPane::providerSelected, this, [this](const QString &id) {
            if (m_viewModel) {
                m_viewModel->selectProvider(id);
            }
        });
        connect(m_navPane, &ProviderNavigationPane::addProviderRequested, this, &ModelManagerWindow::onAddProvider);
        connect(m_detailView, &ProviderDetailView::addModelRequested, this, &ModelManagerWindow::onAddModel);
        connect(m_detailView, &ProviderDetailView::providerDeleted, this, [this](const QString &id) {
            if (m_viewModel) {
                m_viewModel->deleteProvider(id);
            }
        });
        connect(m_detailView, &ProviderDetailView::providerChanged, this, [this](const domain::model::ModelProvider &provider) {
            if (m_viewModel) {
                m_viewModel->saveProvider(provider);
            }
        });
        connect(m_detailView, &ProviderDetailView::refreshModelsRequested, this, [this](const QString &providerId) {
            if (m_viewModel) {
                m_viewModel->refreshModels(providerId);
            }
        });
    }

    void ModelManagerWindow::renderState(const ModelManagerState &state) {
        if (m_navPane) {
            m_navPane->setProviders(state.providers);
            if (!state.selectedProviderId.isEmpty()) {
                m_navPane->selectProvider(state.selectedProviderId);
            }
        }
        if (m_detailView) {
            m_detailView->setProvider(state.selectedProvider);
            m_detailView->setRefreshing(state.isRefreshing);
        }
    }

    void ModelManagerWindow::open(QWidget *parent) {
        if (parent) {
            QWidget *newOwner = parent->window();
            if (newOwner != m_owner) {
                if (m_owner && m_smokeOverlay) {
                    m_owner->removeEventFilter(this);
                    m_smokeOverlay->deleteLater();
                    m_smokeOverlay = nullptr;
                }
                m_owner = newOwner;
                if (m_owner) {
                    setParent(m_owner);
                }
            }
        }
        if (!m_owner && qApp) {
            m_owner = qApp->activeWindow();
            if (m_owner) {
                setParent(m_owner);
            }
        }
        if (!m_owner) {
            return;
        }

        if (!m_smokeOverlay) {
            m_smokeOverlay = new SmokeOverlayWidget(m_owner, [this]() { hide(); });
            m_owner->installEventFilter(this);
        }
        updateSmokeGeometry();
        const QPoint targetPosition = centeredPosition();
        m_smokeOverlay->show();
        m_smokeOverlay->raise();
        move(targetPosition + QPoint(0, 12));
        show();
        raise();
        setFocus(Qt::ActiveWindowFocusReason);

        if (!m_entranceAnimation) {
            m_entranceAnimation = new QPropertyAnimation(this, "pos", this);
            m_entranceAnimation->setEasingCurve(themeAnimation().entrance);
        }
        m_entranceAnimation->stop();
        m_entranceAnimation->setDuration(themeAnimation().fast);
        m_entranceAnimation->setStartValue(pos());
        m_entranceAnimation->setEndValue(targetPosition);
        m_entranceAnimation->start();

        if (m_viewModel) {
            m_viewModel->loadProviders();
        }
    }

    bool ModelManagerWindow::eventFilter(QObject *watched, QEvent *event) {
        if (watched == m_owner && event->type() == QEvent::Resize) {
            updateSmokeGeometry();
            centerInOwner();
        }
        return QWidget::eventFilter(watched, event);
    }

    void ModelManagerWindow::hideEvent(QHideEvent *event) {
        if (m_entranceAnimation) {
            m_entranceAnimation->stop();
        }
        if (m_smokeOverlay) {
            m_smokeOverlay->hide();
        }
        QWidget::hideEvent(event);
    }

    void ModelManagerWindow::keyPressEvent(QKeyEvent *event) {
        if (event->key() == Qt::Key_Escape) {
            hide();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void ModelManagerWindow::paintEvent(QPaintEvent *) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF surface = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath path;
        path.addRoundedRect(surface, themeRadius().overlay, themeRadius().overlay);
        painter.fillPath(path, themeColorsRef().bgLayer);
        painter.setPen(themeColorsRef().strokeCard);
        painter.drawPath(path);
    }

    void ModelManagerWindow::resizeEvent(QResizeEvent *event) {
        QWidget::resizeEvent(event);
        // QRegion mask 只能做像素级裁剪，会让高 DPI 的圆角锯齿；改由 surface
        // 留白保留抗锯齿角像素。
        clearMask();
        update();
    }

    void ModelManagerWindow::onThemeUpdated() {
        update();
    }

    void ModelManagerWindow::centerInOwner() {
        if (m_entranceAnimation) {
            m_entranceAnimation->stop();
        }
        move(centeredPosition());
    }

    QPoint ModelManagerWindow::centeredPosition() const {
        if (!m_owner) {
            return pos();
        }
        const QRect bounds = m_owner->rect();
        return {bounds.left() + (bounds.width() - width()) / 2,
                bounds.top() + (bounds.height() - height()) / 2};
    }

    void ModelManagerWindow::updateSmokeGeometry() {
        if (m_smokeOverlay && m_owner) {
            m_smokeOverlay->setGeometry(m_owner->rect());
        }
    }

    void ModelManagerWindow::onAddProvider() {
        AddProviderDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            const auto provider = dialog.resultProvider();
            if (!provider.id.isEmpty() && m_viewModel) {
                m_viewModel->addProvider(provider);
            }
        }
    }

    void ModelManagerWindow::onAddModel(const QString &providerId) {
        AddModelDialog dialog(providerId, this);
        if (dialog.exec() == QDialog::Accepted) {
            const auto model = dialog.resultModel();
            if (!model.id.isEmpty() && m_viewModel) {
                m_viewModel->addModel(providerId, model);
            }
        }
    }

} // namespace ui::screen::settings::model_manager
