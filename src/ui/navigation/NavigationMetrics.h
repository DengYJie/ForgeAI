#pragma once

#include <QVector>

#include <FluentQt/Design.h>

namespace ui::navigation {
    class NavigationTreeWidget;
    class NavigationSectionHeader;

    /**
 * @brief 溢出条目（按布局顺序），供 panel 渲染 overflow flyout。
 *
 * node 与 header 二选一非空：node 为顶级节点，header 为分区标题。
 */
    struct NavigationOverflowEntry {
        NavigationTreeWidget *node = nullptr;
        NavigationSectionHeader *header = nullptr;
    };

    /**
 * @brief 导航条目归属区域 (MenuItems vs FooterMenuItems)
 */
    enum class NavigationItemPosition {
        Top, // 顶部主列表区 (MenuItems)
        Bottom // 底部页脚区 (FooterMenuItems)
    };

    /**
 * @name 导航布局度量标准
 * @{
 */
    // 对齐 NavigationView 导航项标准行高（ControlHeight::Large）
    constexpr int kItemHeight = 40;
    constexpr int kTextLeftOffset = 40;
    constexpr int kIconSlotWidth = Typography::IconSize::Standard;
    // 为行间留出呼吸间距
    constexpr int kItemBgPaddingV = 2;
    // 分区标题高：与导航项行高一致
    constexpr int kSectionHeight = Spacing::ControlHeight::Large;
    constexpr int kCompactPaneWidth = Breakpoints::NavigationPaneCompactWidth;
    // 为指示条与外边框预留间隙
    constexpr int kRowLeftInset = Spacing::XSmall;
    // 保证图标与指示条的视觉呼吸感
    constexpr int kContentStart = Spacing::Medium;
    // 确保图标几何居中
    constexpr int kIconAreaWidth = Typography::IconSize::Standard;
    // 对齐 WinUI3 ExpandCollapseChevron 点击区域 40px
    constexpr int kChevronAreaWidth = 40;
    constexpr int kChevronRightInset = 6;
    // Top 模式分类箭头所需占据的额外布局宽度
    constexpr int kTopChevronAddedWidth = 30;
    // chevron 紧凑指示符使用 12px 原生字形
    constexpr int kChevronIconPixelSize = Typography::IconSize::Compact;
    constexpr int kSelectionIndicatorWidth = 3;
    // 视觉高度对齐
    constexpr int kSelectionIndicatorHeight = 16;
    constexpr int kSelectionIndicatorTextGap = Spacing::Small;
    // 与 MenuFlyout 行宽对齐
    constexpr int kCompactFlyoutRowWidth = 200;
    constexpr int kCompactFlyoutAnchorOffsetX = Spacing::XSmall;
    constexpr int kCompactFlyoutSlideOffset = 40;
    // paintLayeredShadow 最外层扩 9px+offsetY 2px，margin≥12 才不裁阴影
    constexpr int kShadowMargin = 12;
    // Top 模式水平内边距
    constexpr int kTopBarItemHorizontalPadding = 12;
    // Top 模式项高度：对齐 NavigationView 顶部栏 48px 规范
    constexpr int kTopBarItemHeight = 48;
    // Top 模式水平指示条高度
    constexpr int kTopSelectionIndicatorHeight = 3;
    // Top 模式指示条默认/最小宽度
    constexpr int kTopSelectionIndicatorWidth = 16;
    // Top 模式按钮间距
    constexpr int kTopBarButtonSpacing = 4;
    // 分隔线线体厚度
    constexpr int kSeparatorLineThickness = 1;
    // 分隔线前侧（Left=上 / Top=左）留白：WinUI3 NavigationViewItemSeparatorMargin
    constexpr int kSeparatorLeadingMargin = 3;
    // 分隔线后侧（Left=下 / Top=右）留白：WinUI3 NavigationViewItemSeparatorMargin
    constexpr int kSeparatorTrailingMargin = 4;
    /** @} */
} // namespace ui::navigation
