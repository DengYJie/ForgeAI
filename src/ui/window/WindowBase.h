#pragma once

#include <QWidget>

#include <FluentQt/Design.h>
#include <FluentQt/Foundation.h>
#include <FluentQt/Windowing.h>

namespace ui {
    namespace window {
        class TitleBar;
    }
}

class WindowBasePrivate;

/**
 * @brief 自定义窗口基类，基于 QWidget + QWindowKit 实现现代无边框和原生系统特效。
 * @note 采用 Pimpl 模式封装，隐藏底层 QWindowKit 实现，确保 ABI 稳定。
 */
class WindowBase : public QWidget, public fluent::FluentElement, public fluent::QMLPlus {
    Q_OBJECT
    Q_PROPERTY(QWidget * contentWidget READ contentWidget WRITE setContentWidget NOTIFY contentWidgetChanged)

    Q_PROPERTY(
        fluent::windowing::BackdropEffect
 backdropEffect READ backdropEffect WRITE setBackdropEffect NOTIFY backdropEffectChanged)

    Q_PROPERTY(
        bool
 customWindowChromeEnabled READ customWindowChromeEnabled WRITE setCustomWindowChromeEnabled NOTIFY customWindowChromeEnabledChanged)


public:
    /**
     * @brief 构造函数。初始化基础布局。
     */
    explicit WindowBase(QWidget *parent = nullptr);

    ~WindowBase() override;

    /**
     * @brief 获取当前托管的主内容控件。
     */
    QWidget *contentWidget() const;

    /**
     * @brief 设置主内容控件。通常在构造窗口 UI 时调用。
     * @param widget 要嵌入的主界面控件，生命周期归 WindowBase 管理。
     */
    void setContentWidget(QWidget *widget);

    /**
     * @brief 获取标题栏控件。用于自定义添加额外标题栏动作或组件。
     */
    ui::window::TitleBar *titleBar() const;

    /**
     * @brief 获取内容区容器控件。
     */
    QWidget *contentHost() const;

    /**
     * @brief 设置窗口背景效果（Mica / Acrylic / Solid）。
     * @note 建议在窗口初始化或主题切换时调用。
     * @param effect 目标背景效果类型。
     */
    void setBackdropEffect(fluent::windowing::BackdropEffect effect);

    /**
     * @brief 获取当前生效的背景效果。
     */
    fluent::windowing::BackdropEffect backdropEffect() const;

    /**
     * @brief 启用或禁用 Fluent 自绘标题栏。
     * @note 请在窗口调用 show() 之前设置。若设置为 false，则窗口保留原生系统边框。
     * @param enabled 是否开启自绘沉浸式标题栏。
     */
    void setCustomWindowChromeEnabled(bool enabled);

    /**
     * @brief 获取是否启用了自绘标题栏。
     */
    bool customWindowChromeEnabled() const;

    /**
     * @brief 设置指定子控件在自绘标题栏区域内的点击穿透性 (Hit-Test)。
     * @param widget 目标控件指针
     * @param visible 是否允许响应鼠标交互（若为 false 则该区域被作为标题栏拖拽区）
     */
    void setHitTestVisible(QWidget *widget, bool visible = true);

    /**
     * @brief 主题更新回调。在系统或应用切换深浅色模式时自动触发。
     */
    void onThemeUpdated() override;

    Q_SIGNALS:




    void contentWidgetChanged();

    void backdropEffectChanged();

    void customWindowChromeEnabledChanged();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Q_DECLARE_PRIVATE(WindowBase)
    const std

    ::unique_ptr<WindowBasePrivate> d_ptr;
};
