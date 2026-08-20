#pragma once

namespace ui::navigation {
    /**
 * @brief 导航焦点宿主接口
 *
 * 抽象统一的键盘焦点导航契约，使子部件能够向上委托方向键事件，实现跨层级与跨区域的焦点闭环。
 */
    class INavigationFocusHost {
    public:
        virtual ~INavigationFocusHost() = default;

        /**
     * @brief 沿可视导航项移动键盘焦点
     * @param delta 移动步长，-1 为向上/前一项，+1 为向下/后一项
     */
        virtual void moveFocusBy(int delta) = 0;
    };
} // namespace ui::navigation
