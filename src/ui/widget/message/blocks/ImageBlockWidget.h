#pragma once

#include <QWidget>
#include <FluentQt/Foundation.h>
#include "domain/conversation/MessageBlock.h"

class QLabel;

namespace ui::widget::message::blocks {

/**
 * @brief 图片展示块（仿 Cherry Studio ImageBlock）
 *
 * 支持本地图片缓存加载、远程预览与圆角裁剪。
 */
class ImageBlockWidget : public QWidget, public fluent::FluentElement {
    Q_OBJECT
public:
    explicit ImageBlockWidget(QWidget *parent = nullptr);
    explicit ImageBlockWidget(const domain::conversation::ImageBlock &block, QWidget *parent = nullptr);
    ~ImageBlockWidget() override;

    void setImage(const domain::conversation::ImageBlock &block);
    void onThemeUpdated() override;

signals:
    void contentHeightChanged();

private:
    void setupUi();
    void updateVisuals();

    domain::conversation::ImageBlock m_block;
    QLabel *m_imageLabel = nullptr;
};

} // namespace ui::widget::message::blocks
