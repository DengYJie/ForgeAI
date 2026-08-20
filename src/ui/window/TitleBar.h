#pragma once

#include <FluentQt/Windowing.h>
#include <FluentQt/BasicInput.h>
#include <QIcon>
#include <QString>
#include <QWidget>

namespace ui::animation {
    class AnimatedIcon;
}

namespace ui::window {
    class ElidedLabel;

    /**
 * @brief WinUI 3 标准窗口标题栏
 *
 * 遵循 Windows 11 Fluent 标题栏设计规范，提供 32px 标准高度与 48px 扩展高度（容纳搜索框/头像时），
 * 支持 16px 规范安全边距、满出血背板标题按钮、文字溢出截断以及系统窗口事件的原生无缝集成。
 */
    class TitleBar : public fluent::windowing::TitleBar {
        Q_OBJECT
        Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)

        Q_PROPERTY(QString subtitle READ subtitle WRITE setSubtitle NOTIFY titleChanged)

        Q_PROPERTY(QIcon icon READ icon WRITE setIcon NOTIFY iconChanged)

        Q_PROPERTY(
            bool backButtonVisible READ isBackButtonVisible WRITE setBackButtonVisible NOTIFY backButtonVisibleChanged)

        Q_PROPERTY(
            bool backButtonEnabled READ isBackButtonEnabled WRITE setBackButtonEnabled NOTIFY backButtonEnabledChanged)

        Q_PROPERTY(
            bool
 paneToggleButtonVisible READ isPaneToggleButtonVisible WRITE setPaneToggleButtonVisible NOTIFY paneToggleButtonVisibleChanged)



        Q_PROPERTY(HeightOption heightOption READ heightOption WRITE setHeightOption NOTIFY heightOptionChanged)

        Q_PROPERTY(
            int minDragRegionWidth READ minDragRegionWidth WRITE setMinDragRegionWidth NOTIFY minDragRegionWidthChanged)

    public:
        /**
     * @brief 标题栏高度选项
     */
        enum class HeightOption {
            // 标准高度: WinUI 3 默认 32px
            Standard,
            // 扩展高度: 容纳搜索框或用户卡片时 48px
            Tall
        };

        Q_ENUM(HeightOption)

        /**
     * @brief 构造函数
     * @param parent 父容器控件
     */
        explicit TitleBar(QWidget *parent = nullptr);

        /**
     * @brief 析构函数
     */
        ~TitleBar() override;

        /**
     * @brief 设置应用窗口主标题
     * @param title 主标题文本
     */
        void setTitle(const QString &title);

        /**
     * @brief 获取应用窗口主标题
     * @return 当前主标题文本
     */
        QString title() const;

        /**
     * @brief 设置应用窗口副标题
     * @param subtitle 副标题文本
     */
        void setSubtitle(const QString &subtitle);

        /**
     * @brief 获取应用窗口副标题
     * @return 当前副标题文本
     */
        QString subtitle() const;

        /**
     * @brief 设置应用窗口图标
     * @param icon 图标对象
     */
        void setIcon(const QIcon &icon);

        /**
     * @brief 获取应用窗口图标
     * @return 当前图标对象
     */
        QIcon icon() const;

        /**
     * @brief 设置后退导航按钮的可见性
     * @param visible 是否可见
     */
        void setBackButtonVisible(bool visible);

        /**
     * @brief 查询后退导航按钮的可见性
     * @return 是否可见
     */
        bool isBackButtonVisible() const;

        /**
     * @brief 设置后退导航按钮的交互可用性
     * @param enabled 是否可用
     */
        void setBackButtonEnabled(bool enabled);

        /**
     * @brief 查询后退导航按钮的交互可用性
     * @return 是否可用
     */
        bool isBackButtonEnabled() const;

        /**
     * @brief 设置侧边栏展开折叠按钮的可见性
     * @param visible 是否可见
     */
        void setPaneToggleButtonVisible(bool visible);

        /**
     * @brief 查询侧边栏展开折叠按钮的可见性
     * @return 是否可见
     */
        bool isPaneToggleButtonVisible() const;

        /**
     * @brief 设置标题栏高度模式
     * @param option 目标高度模式
     */
        void setHeightOption(HeightOption option);

        /**
     * @brief 获取当前标题栏高度模式
     * @return 当前高度模式
     */
        HeightOption heightOption() const;

        /**
     * @brief 内容区对齐方式
     */
        enum class ContentAlignment {
            // 绝对居中对齐
            Center,
            // 自适应拉伸对齐
            Stretch
        };

        Q_ENUM(ContentAlignment)

        /**
     * @brief 注入中间主内容区域
     * @param contentWidget 自定义内容控件指针
     * @param alignment 对齐方式
     */
        void setContentWidget(QWidget *contentWidget, ContentAlignment alignment = ContentAlignment::Stretch);

        /**
     * @brief 获取中间主内容区域控件
     * @return 内容控件指针
     */
        QWidget *contentWidget() const;

        /**
     * @brief 获取所有需要响应鼠标点击事件的内部控件列表。
     * 供 WindowBase 获取并向 QWindowKit 注册点击穿透，以避免这些控件被当作无边框拖拽区。
     */
        QList<QWidget *> hitTestVisibleWidgets() const;

        /**
     * @brief 获取当前中间内容区域的对齐方式
     * @return 对齐方式枚举
     */
        ContentAlignment contentAlignment() const;

        /**
     * @brief 注入自定义右侧控制头内容
     * @param rightHeaderWidget 右侧自定义控件指针 (例如用户头像区)
     */
        void setRightHeaderWidget(QWidget *rightHeaderWidget);

        /**
     * @brief 获取自定义右侧控制头内容控件
     * @return 右侧自定义控件指针
     */
        QWidget *rightHeaderWidget() const;

        /**
     * @brief 注入自定义左侧控制头内容
     * @param leftHeaderWidget 左侧自定义控件指针
     */
        void setLeftHeaderWidget(QWidget *leftHeaderWidget);

        /**
     * @brief 获取自定义左侧控制头内容控件
     * @return 左侧自定义控件指针
     */
        QWidget *leftHeaderWidget() const;

        /**
     * @brief 设置右侧头部与系统三键之间的最小拖拽安全区宽度
     * @param width 宽度像素值，默认 48px（WinUI 3 标准），设为 0 即紧贴三键
     */
        void setMinDragRegionWidth(int width);

        /**
     * @brief 获取右侧最小拖拽安全区宽度
     * @return 宽度像素值
     */
        int minDragRegionWidth() const;

        /**
     * @brief 系统控制按钮类型
     */
        enum class SystemButtonType {
            Minimize,
            Maximize,
            Close
        };

        Q_ENUM(SystemButtonType)

        /**
     * @brief 获取对应的系统控制按钮
     * 用于 QWindowKit 注册系统最小化/最大化/关闭动作
     * @param type 按钮类型
     * @return 按钮控件指针
     */
        QWidget *systemButton(SystemButtonType type) const;

        /**
     * @brief 设置系统控制按钮的可见性并刷新布局
     * @param type 按钮类型
     * @param visible 是否可见
     */
        void setSystemButtonVisible(SystemButtonType type, bool visible);

        /**
     * @brief 查询系统控制按钮的可见性
     * @param type 按钮类型
     * @return 是否可见
     */
        bool isSystemButtonVisible(SystemButtonType type) const;

        /**
     * @brief 主题刷新回调：同步前景色、圆角及非活动态透明度
     */
        void onThemeUpdated() override;

        signals:
        /**
     * @brief 后退按钮点击信号
     */




        void backButtonClicked();

        /**
     * @brief 侧边栏切换按钮点击信号
     */
        void paneToggleButtonClicked();

        /**
     * @brief 主标题变更信号
     * @param title 新标题内容
     */
        void titleChanged(const QString &title);

        /**
     * @brief 副标题变更信号
     * @param subtitle 新副标题内容
     */
        void subtitleChanged(const QString &subtitle);

        /**
     * @brief 图标变更信号
     * @param icon 新图标
     */
        void iconChanged(const QIcon &icon);

        /**
     * @brief 后退按钮可见性变更信号
     * @param visible 新可见性状态
     */
        void backButtonVisibleChanged(bool visible);

        void backButtonEnabledChanged(bool enabled);

        /**
     * @brief 侧边栏切换按钮可见性变更信号
     * @param visible 新可见性状态
     */
        void paneToggleButtonVisibleChanged(bool visible);

        /**
     * @brief 标题栏高度模式变更信号
     * @param option 新高度模式
     */
        void heightOptionChanged(HeightOption option);

        /**
     * @brief 最小拖拽安全区宽度变更信号
     * @param width 新宽度像素值
     */
        void minDragRegionWidthChanged(int width);

        /**
     * @brief 可点击交互控件变更信号（供 WindowBase / QWindowKit 动态同步穿透区域）
     * @param widget 发生变化的控件指针
     * @param visible 是否需要接收鼠标交互（true 为交互控件，false 为窗口拖拽区）
     */
        void hitTestWidgetChanged(QWidget *widget, bool visible);

    protected:
        // 重写 showEvent 以确保在已加入窗口树时正确安装事件过滤器
        void showEvent(QShowEvent *event) override;

        // 监听尺寸变化以动态计算绝对居中搜索框及溢出截断区域
        void resizeEvent(QResizeEvent *event) override;

        // 监听宿主窗口状态以同步最大化还原图标与右上角圆角裁切
        bool eventFilter(QObject *watched, QEvent *event) override;

    private:
        // 同步当前窗口的最大化/还原状态到 UI
        void syncWindowState();

    private:
        // 根据子控件存在情况与高度模式计算并应用物理高度
        void updateHeight();

        // 计算各分区绝对像素坐标并更新拖拽安全区
        void updateLayout();

        // 保证背板填满标题栏高度
        void updateCaptionButtonSizes();

        // 窗口非活动时降低交互元素不透明度
        void syncActivationOpacity();

        // 统一构造符合 WinUI 3 交互态与提示气泡的系统控制按钮
        fluent::basicinput::Button *createCaptionButton(const QString &objectName, const QString &glyph,
                                                        const QString &tooltip, bool isCloseBtn = false);

        QString m_title;
        QString m_subtitle;
        QIcon m_icon;

        ui::animation::AnimatedIcon *m_animatedBackIcon = nullptr;
        ui::animation::AnimatedIcon *m_animatedPaneToggleIcon = nullptr;

        bool m_backButtonVisible = false;
        bool m_backButtonEnabled = true;
        bool m_paneToggleButtonVisible = false;
        HeightOption m_heightOption = HeightOption::Standard;
        int m_minDragRegionWidth = 48;

        // 内部状态跟踪
        bool m_isCompact = false;
        int m_compactModeThresholdWidth = 0;

        // 自定义注入控件
        QWidget *m_leftHeaderWidget = nullptr;
        QWidget *m_contentWidget = nullptr;
        ContentAlignment m_contentAlignment = ContentAlignment::Stretch;
        QWidget *m_rightHeaderWidget = nullptr;

        // 标题与导航控件
        fluent::basicinput::Button *m_backButton = nullptr;
        fluent::basicinput::Button *m_paneToggleButton = nullptr;
        fluent::basicinput::Button *m_iconButton = nullptr;
        ElidedLabel *m_titleLabel = nullptr;
        ElidedLabel *m_subtitleLabel = nullptr;

        // 窗口系统控制按钮
        fluent::basicinput::Button *m_minimizeButton = nullptr;
        fluent::basicinput::Button *m_maximizeButton = nullptr;
        fluent::basicinput::Button *m_closeButton = nullptr;
    };
} // namespace ui::window
