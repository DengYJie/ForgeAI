#pragma once

#include <QByteArray>
#include <QHash>
#include <QPointer>
#include <QVariantAnimation>
#include <FluentQt/Collections.h>

namespace ui::widget {

class SlideViewportContainer;
class LightDismissOverlay;

/**
 * @brief 分栏面板显示模式
 */
enum class SplitPaneDisplayMode {
    Inline,         ///< 折叠至 0px，展开时挤压主内容
    CompactInline,  ///< 折叠至紧凑宽度 (48px)，展开时挤压主内容
    Overlay,        ///< 折叠至 0px，展开时悬浮覆盖主内容（不挤压），支持遮罩点击收起
    CompactOverlay  ///< 折叠至紧凑宽度 (48px)，展开时悬浮延伸覆盖主内容（不挤压），支持遮罩点击收起
};

/**
 * @brief 支持平滑开合动效、紧凑模式及悬浮遮罩的分栏视图
 */
class CollapsibleSplitView : public fluent::collections::SplitView {
    Q_OBJECT

public:
    explicit CollapsibleSplitView(QWidget *parent = nullptr);
    ~CollapsibleSplitView() override;

    /**
     * @brief 添加一个具备平滑折叠动画与视口裁剪能力的分栏面板
     * @param pane 业务内容部件
     * @param mode 面板显示模式
     * @param compactLength 折叠后的紧凑宽度（像素）
     * @param startExpanded 初始是否处于展开状态
     * @param initialOpenLength 初始展开目标宽度（像素）
     * @param options 底层 SplitView 面板配置项（支持 options.minimumSize 和 options.maximumSize）
     * @return 添加后的面板索引
     */
    int addCollapsiblePane(
        QWidget *pane,
        SplitPaneDisplayMode mode = SplitPaneDisplayMode::Inline,
        int compactLength = 48,
        bool startExpanded = true,
        int initialOpenLength = 320,
        const fluent::collections::SplitViewPaneOptions &options = {});

    /**
     * @brief 移除指定索引的可折叠面板并释放相关资源
     * @param index 面板索引
     */
    void removeCollapsiblePane(int index);

    /**
     * @brief 查询指定面板当前是否处于展开状态
     * @param index 面板索引
     */
    bool isPaneExpanded(int index) const;

    /**
     * @brief 查询指定面板当前是否正在执行开合动画
     * @param index 面板索引
     */
    bool isPaneAnimating(int index) const;

    /**
     * @brief 获取指定面板的显示模式
     * @param index 面板索引
     */
    SplitPaneDisplayMode paneDisplayMode(int index) const;

    /**
     * @brief 获取指定面板当前的展开目标宽度
     * @param index 面板索引
     */
    int paneOpenLength(int index) const;

    /**
     * @brief 获取指定面板的紧凑模式宽度
     * @param index 面板索引
     */
    int paneCompactLength(int index) const;

    /**
     * @brief 获取指定面板展开态下的最小宽度约束
     * @param index 面板索引
     */
    int paneMinOpenLength(int index) const;

    /**
     * @brief 获取指定面板展开态下的最大宽度约束
     * @param index 面板索引
     */
    int paneMaxOpenLength(int index) const;

    /**
     * @brief 切换指定面板的展开/折叠状态
     * @param index 面板索引
     * @param animated 是否启用过渡动画
     */
    void togglePane(int index, bool animated = true);

    /**
     * @brief 设置指定面板的展开/折叠状态
     * @param index 面板索引
     * @param expanded 目标展开状态
     * @param animated 是否启用过渡动画
     */
    void setPaneExpanded(int index, bool expanded, bool animated = true);

    /**
     * @brief 动态修改指定面板的显示模式
     * @param index 面板索引
     * @param mode 新的显示模式
     */
    void setPaneDisplayMode(int index, SplitPaneDisplayMode mode);

    /**
     * @brief 设置指定面板的紧凑宽度
     * @param index 面板索引
     * @param length 紧凑宽度（像素）
     */
    void setPaneCompactLength(int index, int length);

    /**
     * @brief 设置指定面板的展开目标宽度
     * @param index 面板索引
     * @param length 展开宽度（像素）
     */
    void setPaneOpenLength(int index, int length);

    /**
     * @brief 设置指定面板在展开态下的最小宽度约束
     * @param index 面板索引
     * @param minLength 最小宽度（像素）
     */
    void setPaneMinOpenLength(int index, int minLength);

    /**
     * @brief 设置指定面板在展开态下的最大宽度约束
     * @param index 面板索引
     * @param maxLength 最大宽度（像素）
     */
    void setPaneMaxOpenLength(int index, int maxLength);

    /**
     * @brief 设置指定面板的自动折叠断点宽度（0 表示禁用，例如 768）
     * @param index 面板索引
     * @param breakpointWidth 触发自动折叠的控件最小总宽度
     */
    void setAutoCollapseBreakpoint(int index, int breakpointWidth);

    /**
     * @brief 获取指定面板的自动折叠断点宽度
     */
    int autoCollapseBreakpoint(int index) const;

    /**
     * @brief 显式设置用户对指定面板的开合偏好（覆盖断点自动行为）
     */
    void setUserExplicitExpansion(int index, bool expanded);

    /**
     * @brief 启用或禁用悬浮模式下的点击遮罩自动收起行为
     * @param enabled 是否启用
     */
    void setLightDismissEnabled(bool enabled);

    /**
     * @brief 获取当前是否启用了点击遮罩自动收起
     */
    bool isLightDismissEnabled() const { return m_lightDismissEnabled; }

    /**
     * @brief 序列化当前所有面板的开合状态与配置
     * @return 包含布局与可折叠扩展状态的二进制数据
     */
    QByteArray saveCollapsibleState() const;

    /**
     * @brief 从二进制数据中反序列化并恢复面板状态
     * @param state 序列化数据
     * @return 是否成功恢复
     */
    bool restoreCollapsibleState(const QByteArray &state);

Q_SIGNALS:
    /**
     * @brief 面板开始展开时触发
     */
    void paneOpening(int index);

    /**
     * @brief 面板完全展开后触发
     */
    void paneOpened(int index);

    /**
     * @brief 面板开始折叠时触发
     */
    void paneClosing(int index);

    /**
     * @brief 面板完全折叠后触发
     */
    void paneClosed(int index);

    /**
     * @brief 面板显示模式变更时触发
     */
    void paneDisplayModeChanged(int index, SplitPaneDisplayMode mode);

    /**
     * @brief 面板紧凑宽度变更时触发
     */
    void paneCompactLengthChanged(int index, int length);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct PaneState {
        QPointer<QWidget> userWidget;
        QPointer<SlideViewportContainer> viewportWrapper;
        QPointer<QVariantAnimation> animation;

        SplitPaneDisplayMode mode = SplitPaneDisplayMode::Inline;
        bool isExpanded = true;
        bool animationOpening = false;

        int compactLength = 48;
        int openLength = 320;
        int minOpenLength = 120;
        int maxOpenLength = 16777215;
        int currentAnimatedLength = 320;

        int autoCollapseBreakpoint = 0;
        bool userExplicitClosed = false;
        bool autoCollapsedByBreakpoint = false;
    };

    PaneState *stateForIndex(int index);
    const PaneState *stateForIndex(int index) const;
    int indexForPane(QWidget *wrapper) const;

    bool isOverlayMode(SplitPaneDisplayMode mode) const;
    int collapsedLength(const PaneState &state) const;
    bool shouldAnimate() const;

    void updatePaneConstraints(int index);
    void updateOverlayLayout();
    void animateTo(int index, int targetLength, bool opening);
    void finishPaneAnimation(int index);

    void onPaneSizeChanged(int index, int size);
    void onPaneDestroyed(QObject *obj);
    void handleLightDismissClicked();

    QHash<QWidget *, PaneState> m_paneStates;
    QPointer<LightDismissOverlay> m_lightDismissOverlay;
    int m_suppressSizeMemory = 0;
    bool m_lightDismissEnabled = true;
};

} // namespace ui::widget
