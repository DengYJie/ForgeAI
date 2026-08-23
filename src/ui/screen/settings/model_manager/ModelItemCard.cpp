#include "ModelItemCard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <FluentQt/BasicInput.h>
#include <FluentQt/TextFields.h>
#include <FluentQt/Design.h>

namespace ui::screen::settings::model_manager {

    namespace {
        QString formatContextLimit(int limit) {
            if (limit >= 1000000) {
                double val = limit / 1000000.0;
                return (val == static_cast<int>(val)) ? QStringLiteral("%1M").arg(static_cast<int>(val))
                                                      : QStringLiteral("%1M").arg(val, 0, 'f', 1);
            }
            if (limit >= 1000) {
                return QStringLiteral("%1K").arg(limit / 1000);
            }
            return QString::number(limit);
        }

        class CapabilityTag : public QWidget, public fluent::FluentElement {
        public:
            CapabilityTag(const QString &text, QWidget *parent = nullptr)
                : QWidget(parent), m_text(text) {
                setFixedHeight(22);
                setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            }

        protected:
            void paintEvent(QPaintEvent *) override {
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);

                const bool isDark = (effectiveTheme() == fluent::FluentElement::Dark);
                const auto &colors = themeColorsRef();

                QRectF rect = this->rect();
                rect.adjust(0.5, 0.5, -0.5, -0.5);

                QColor bg = isDark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 10);
                QPainterPath path;
                path.addRoundedRect(rect, 4, 4);
                painter.fillPath(path, bg);

                painter.setFont(Typography::fontStyle(Typography::FontRole::Caption).toQFont());
                painter.setPen(colors.textSecondary);
                painter.drawText(rect, Qt::AlignCenter, m_text);
            }

            QSize sizeHint() const override {
                QFont font = Typography::fontStyle(Typography::FontRole::Caption).toQFont();
                QFontMetrics fm(font);
                int textWidth = fm.horizontalAdvance(m_text);
                return QSize(textWidth + 14, 22);
            }

        private:
            QString m_text;
        };
    } // namespace

    ModelItemCard::ModelItemCard(const domain::model::Model &model, QWidget *parent)
        : QWidget(parent), m_model(model) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        setupUi();
    }

    void ModelItemCard::setupUi() {
        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(14, 10, 14, 10);
        rootLayout->setSpacing(6);

        // 顶部行：名称、ID 与右侧开关
        auto *topRow = new QHBoxLayout();
        topRow->setContentsMargins(0, 0, 0, 0);
        topRow->setSpacing(8);

        auto *nameLabel = new fluent::textfields::Label(m_model.displayName, this);
        nameLabel->setFluentTypography(Typography::FontRole::BodyStrong);
        nameLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);

        auto *idLabel = new fluent::textfields::Label(QStringLiteral("(%1)").arg(m_model.id), this);
        idLabel->setFluentTypography(Typography::FontRole::Caption);
        idLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);

        topRow->addWidget(nameLabel);
        topRow->addWidget(idLabel);
        topRow->addStretch(1);

        if (m_model.isCustom) {
            m_deleteBtn = new fluent::basicinput::Button(this);
            m_deleteBtn->setText(tr("删除"));
            m_deleteBtn->setFixedHeight(26);
            connect(m_deleteBtn, &fluent::basicinput::Button::clicked, this, [this]() {
                Q_EMIT modelDeleted(m_model.id);
            });
            topRow->addWidget(m_deleteBtn);
        }

        m_toggleSwitch = new fluent::basicinput::ToggleSwitch(this);
        m_toggleSwitch->setIsOn(m_model.isEnabled);
        connect(m_toggleSwitch, &fluent::basicinput::ToggleSwitch::toggled, this, [this](bool checked) {
            m_model.isEnabled = checked;
            Q_EMIT modelToggled(m_model.id, checked);
        });
        topRow->addWidget(m_toggleSwitch);

        rootLayout->addLayout(topRow);

        // 底部行：能力 Tags
        m_tagsContainer = new QWidget(this);
        auto *tagsLayout = new QHBoxLayout(m_tagsContainer);
        tagsLayout->setContentsMargins(0, 0, 0, 0);
        tagsLayout->setSpacing(6);

        updateTagsLayout();

        rootLayout->addWidget(m_tagsContainer);
    }

    void ModelItemCard::updateTagsLayout() {
        auto *tagsLayout = qobject_cast<QHBoxLayout *>(m_tagsContainer->layout());
        if (!tagsLayout) return;

        // 清空现有 tag
        while (QLayoutItem *item = tagsLayout->takeAt(0)) {
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            delete item;
        }

        // 1. 上下文
        if (m_model.limits.context > 0) {
            tagsLayout->addWidget(new CapabilityTag(formatContextLimit(m_model.limits.context), m_tagsContainer));
        }

        // 2. 能力标志
        if (m_model.capabilities.testFlag(domain::model::ModelCapability::Thinking)) {
            tagsLayout->addWidget(new CapabilityTag(QStringLiteral("Thinking"), m_tagsContainer));
        }
        if (m_model.capabilities.testFlag(domain::model::ModelCapability::ToolCalling)) {
            tagsLayout->addWidget(new CapabilityTag(QStringLiteral("Tools"), m_tagsContainer));
        }
        if (m_model.capabilities.testFlag(domain::model::ModelCapability::Vision)) {
            tagsLayout->addWidget(new CapabilityTag(QStringLiteral("Vision"), m_tagsContainer));
        }
        if (m_model.capabilities.testFlag(domain::model::ModelCapability::Audio)) {
            tagsLayout->addWidget(new CapabilityTag(QStringLiteral("Audio"), m_tagsContainer));
        }
        if (m_model.capabilities.testFlag(domain::model::ModelCapability::Video)) {
            tagsLayout->addWidget(new CapabilityTag(QStringLiteral("Video"), m_tagsContainer));
        }
        if (m_model.capabilities.testFlag(domain::model::ModelCapability::Pdf)) {
            tagsLayout->addWidget(new CapabilityTag(QStringLiteral("PDF"), m_tagsContainer));
        }
        if (m_model.capabilities.testFlag(domain::model::ModelCapability::StructuredOutputs)) {
            tagsLayout->addWidget(new CapabilityTag(QStringLiteral("Structured"), m_tagsContainer));
        }

        // 3. 计费
        if (m_model.pricing.inputPrice > 0.0 || m_model.pricing.outputPrice > 0.0) {
            QString priceStr = QStringLiteral("$%1 / $%2").arg(m_model.pricing.inputPrice).arg(m_model.pricing.outputPrice);
            tagsLayout->addWidget(new CapabilityTag(priceStr, m_tagsContainer));
        }

        tagsLayout->addStretch(1);
    }

    void ModelItemCard::setModel(const domain::model::Model &model) {
        m_model = model;
        if (m_toggleSwitch) {
            m_toggleSwitch->setIsOn(m_model.isEnabled);
        }
        updateTagsLayout();
        update();
    }

    void ModelItemCard::paintEvent(QPaintEvent *) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const bool isDark = (effectiveTheme() == fluent::FluentElement::Dark);
        QRectF rect = this->rect();
        rect.adjust(0.5, 0.5, -0.5, -0.5);

        QColor bgColor;
        if (m_isHovered) {
            bgColor = isDark ? QColor(255, 255, 255, 14) : QColor(0, 0, 0, 8);
        } else {
            bgColor = isDark ? QColor(255, 255, 255, 7) : QColor(0, 0, 0, 4);
        }

        QPainterPath path;
        path.addRoundedRect(rect, 8, 8);
        painter.fillPath(path, bgColor);
    }

    void ModelItemCard::enterEvent(QEnterEvent *) {
        m_isHovered = true;
        update();
    }

    void ModelItemCard::leaveEvent(QEvent *) {
        m_isHovered = false;
        update();
    }

    void ModelItemCard::onThemeUpdated() {
        update();
    }

} // namespace ui::screen::settings::model_manager
