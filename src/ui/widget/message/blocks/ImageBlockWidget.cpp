#include "ImageBlockWidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QFileInfo>
#include <FluentQt/Design.h>

namespace ui::widget::message::blocks {

ImageBlockWidget::ImageBlockWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

ImageBlockWidget::ImageBlockWidget(const domain::conversation::ImageBlock &block, QWidget *parent)
    : QWidget(parent)
    , m_block(block)
{
    setupUi();
    setImage(block);
}

ImageBlockWidget::~ImageBlockWidget() = default;

void ImageBlockWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(0);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setScaledContents(false);
    m_imageLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(m_imageLabel);

    updateVisuals();
}

void ImageBlockWidget::setImage(const domain::conversation::ImageBlock &block)
{
    m_block = block;
    
    if (QFileInfo::exists(block.urlOrLocalPath)) {
        QPixmap pixmap(block.urlOrLocalPath);
        if (!pixmap.isNull()) {
            if (pixmap.width() > 400 || pixmap.height() > 300) {
                pixmap = pixmap.scaled(400, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            m_imageLabel->setPixmap(pixmap);
        } else {
            m_imageLabel->setText(QStringLiteral("[无法加载图片: %1]").arg(block.urlOrLocalPath));
        }
    } else {
        m_imageLabel->setText(QStringLiteral("[图片: %1]").arg(block.urlOrLocalPath));
    }
    
    emit contentHeightChanged();
}

void ImageBlockWidget::onThemeUpdated()
{
    updateVisuals();
}

void ImageBlockWidget::updateVisuals()
{
    const auto &colors = themeColorsRef();
    m_imageLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  border: 1px solid %1;"
        "  border-radius: 8px;"
        "  background: %2;"
        "  padding: 4px;"
        "}"
    ).arg(colors.strokeDefault.name(QColor::HexRgb), colors.bgLayerAlt.name(QColor::HexRgb)));
}

} // namespace ui::widget::message::blocks
