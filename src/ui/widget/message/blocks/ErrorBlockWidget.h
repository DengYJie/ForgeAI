#pragma once

#include <QWidget>
#include <FluentQt/Foundation.h>
#include <FluentQt/Layout.h>

class QTextBrowser;

namespace ui::widget::message::blocks {

class FlatExpander;

/**
 * @brief 异常与错误诊断展示卡片
 */
class ErrorBlockWidget : public QWidget, public fluent::FluentElement {
    Q_OBJECT
public:
    explicit ErrorBlockWidget(QWidget *parent = nullptr);
    explicit ErrorBlockWidget(const QString &summary, const QString &details, QWidget *parent = nullptr);
    ~ErrorBlockWidget() override;

    void setError(const QString &summary, const QString &details);
    void setExpanded(bool expanded);
    bool isExpanded() const;

    void onThemeUpdated() override;

signals:
    void contentHeightChanged();

private:
    void setupUi();
    void updateVisuals();

    QString m_summary;
    QString m_details;

    FlatExpander *m_expander = nullptr;
    QWidget *m_container = nullptr;
    QTextBrowser *m_detailsBrowser = nullptr;
};

} // namespace ui::widget::message::blocks
