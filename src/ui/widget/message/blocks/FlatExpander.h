#pragma once

#include <FluentQt/Foundation.h>
#include <FluentQt/Design.h>
#include <QPointer>
#include <QWidget>
#include <QString>

class QVBoxLayout;
class QVariantAnimation;

namespace ui::widget::message::blocks {

class FlatExpanderHeader;

/**
 * @brief 专为 AI 消息块定制的高性能自绘 Fluent 折叠容器
 *
 * 遵循 Fluent Design 2 规范，采用纯粹自绘与直接事件捕获：
 * - 头部自绘：半透明 Subtle 悬浮动效 + Fluent 矢量图标 + 标题/次要说明 + 旋转 Chevron
 * - 零多余 QWidget 嵌套与布局竞争，物理上杜绝重叠 bug，大幅提升长列表性能
 * - 平滑 Spring/Easing 动画折叠与动态流式自动高度撑开
 */
class FlatExpander : public QWidget, public fluent::FluentElement {
    Q_OBJECT
    Q_PROPERTY(bool expanded READ isExpanded WRITE setExpanded NOTIFY expandedChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle)
public:
    enum class ChevronPosition {
        Left,        ///< 箭头在最左侧 (如: ∨ 已处理)
        InlineRight, ///< 箭头紧随标题后 (如: 已处理 · 20 秒 ∨)
        FarRight     ///< 箭头推到卡片最右侧
    };

    explicit FlatExpander(const QString &title = {}, int headerHeight = 26, QWidget *parent = nullptr);
    ~FlatExpander() override;

    // 标题与次要说明
    QString title() const { return m_title; }
    void setTitle(const QString &title);
    QString subtitle() const { return m_subtitle; }
    void setSubtitle(const QString &subtitle);
    void setHeaderText(const QString &text) { setTitle(text); }

    // 头部 Fluent 原生 FontIcon 图标
    QString leadingIcon() const { return m_leadingGlyph; }
    void setLeadingIcon(const QString &fluentGlyph, int iconSize = Typography::IconSize::Standard);

    // 布局与箭头风格
    void setChevronPosition(ChevronPosition pos);
    ChevronPosition chevronPosition() const { return m_chevronPos; }
    void setHeaderCompact(bool compact);
    bool isHeaderCompact() const { return m_headerCompact; }
    void setHeaderHeight(int h);
    int headerHeight() const { return m_headerHeight; }

    // 内容区
    void setContentWidget(QWidget *widget);
    QWidget *contentWidget() const { return m_contentWidget.data(); }

    // 展开与折叠控制
    bool isExpanded() const { return m_expanded; }
    void setExpanded(bool expanded, bool animated = true);
    void toggleExpanded();

    // 动态强制刷新内容高度 (流式打字期间调用)
    void forceUpdateContentHeight();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override;

protected:
    void resizeEvent(QResizeEvent *event) override;

signals:
    void expandedChanged(bool expanded);
    void expansionFinished(bool expanded);
    void contentHeightChanged();

private:
    friend class FlatExpanderHeader;

    void setupUi();
    int naturalContentHeight() const;
    void applyFraction(qreal fraction);
    void updateLayout();

    int m_headerHeight = 26;
    bool m_expanded = false;
    bool m_headerCompact = true;
    ChevronPosition m_chevronPos = ChevronPosition::InlineRight;
    qreal m_fraction = 0.0;
    int m_contentTargetHeight = 0;

    QString m_title;
    QString m_subtitle;
    QString m_leadingGlyph;
    int m_leadingIconSize = Typography::IconSize::Standard;

    FlatExpanderHeader *m_headerBar = nullptr;
    QWidget *m_clip = nullptr;
    QVBoxLayout *m_clipLayout = nullptr;
    QPointer<QWidget> m_contentWidget;
    QVariantAnimation *m_animation = nullptr;
};

} // namespace ui::widget::message::blocks