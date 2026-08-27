#include "PermissionFloatingCard.h"

#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <FluentQt/BasicInput.h>
#include <FluentQt/Design.h>
#include <FluentQt/TextFields.h>

namespace ui::screen::work {

/**
 * @brief Fluent Token 驱动的工具标识胶囊
 */
class ToolPillBadge final : public QWidget, public fluent::FluentElement {
public:
    explicit ToolPillBadge(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedHeight(22);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    void setText(const QString& text) {
        if (m_text != text) {
            m_text = text;
            updateGeometry();
            update();
        }
    }

    QSize sizeHint() const override {
        QFont font = Typography::fontStyle(Typography::FontRole::Caption).toQFont();
        font.setFamily(QStringLiteral("Consolas"));
        const QFontMetrics fm(font);
        const int textWidth = fm.horizontalAdvance(m_text);
        return QSize(textWidth + 16, 22);
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const bool isDark = (effectiveTheme() == fluent::FluentElement::Dark);
        const auto& colors = themeColorsRef();

        const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
        const QColor bg = isDark ? QColor(0, 120, 212, 45) : QColor(0, 120, 212, 22);

        painter.setBrush(bg);
        painter.setPen(QPen(colors.strokeCard, 1));
        painter.drawRoundedRect(r, 4.0, 4.0);

        QFont font = Typography::fontStyle(Typography::FontRole::Caption).toQFont();
        font.setFamily(QStringLiteral("Consolas"));
        painter.setFont(font);
        painter.setPen(colors.textAccentPrimary);
        painter.drawText(r, Qt::AlignCenter, m_text);
    }

    void onThemeUpdated() override {
        update();
    }

private:
    QString m_text;
};

/**
 * @brief Fluent Token 驱动的代码/参数表面容器
 */
class ArgumentsCodeSurface final : public QWidget, public fluent::FluentElement {
public:
    explicit ArgumentsCodeSurface(QWidget* parent = nullptr) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(10, 8, 10, 8);
        layout->setSpacing(0);

        m_contentLabel = new fluent::textfields::Label(this);
        m_contentLabel->setFluentTypography(Typography::FontRole::Caption);
        m_contentLabel->setWordWrap(true);
        m_contentLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        QFont codeFont = Typography::fontStyle(Typography::FontRole::Caption).toQFont();
        codeFont.setFamily(QStringLiteral("Consolas"));
        m_contentLabel->setFont(codeFont);

        layout->addWidget(m_contentLabel);
    }

    void setCodeText(const QString& text) {
        m_contentLabel->setText(text);
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const auto& colors = themeColorsRef();
        const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);

        painter.setBrush(colors.bgSolid);
        painter.setPen(QPen(colors.strokeDivider, 1));
        painter.drawRoundedRect(r, 4.0, 4.0);
    }

    void onThemeUpdated() override {
        update();
    }

private:
    fluent::textfields::Label* m_contentLabel = nullptr;
};

/**
 * @brief Fluent 盾牌图标小组件
 */
class ShieldIconWidget final : public QWidget, public fluent::FluentElement {
public:
    explicit ShieldIconWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(18, 18);
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(themeColorsRef().textAccentPrimary);
        Typography::Icons::paintGlyph(painter, rect(), Typography::Icons::Shield, 16, Qt::AlignCenter);
    }

    void onThemeUpdated() override {
        update();
    }
};

/**
 * @brief 选项行单选指示器组件
 */
class ScopeRadioRow final : public QWidget, public fluent::FluentElement {
public:
    explicit ScopeRadioRow(const QString& title, std::function<void()> onClicked = nullptr, QWidget* parent = nullptr)
        : QWidget(parent), m_title(title), m_onClicked(std::move(onClicked)) {
        setCursor(Qt::PointingHandCursor);
        setFixedHeight(28);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(4, 2, 4, 2);
        layout->setSpacing(8);

        m_label = new fluent::textfields::Label(title, this);
        m_label->setFluentTypography(Typography::FontRole::Body);
        layout->addSpacing(22); // 为左侧 Radio 圆圈留白
        layout->addWidget(m_label);
        layout->addStretch(1);
    }

    void setOnClicked(std::function<void()> onClicked) {
        m_onClicked = std::move(onClicked);
    }

    void setSelected(bool selected) {
        if (m_selected != selected) {
            m_selected = selected;
            update();
        }
    }

    bool isSelected() const { return m_selected; }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            if (m_onClicked) {
                m_onClicked();
            }
        }
        QWidget::mousePressEvent(event);
    }

    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const auto& colors = themeColorsRef();

        // 绘制 Radio Button 圆圈 (x: 6, y: centerY - 7, d: 14)
        const int centerY = height() / 2;
        const QRectF outerRect(6, centerY - 7, 14, 14);

        if (m_selected) {
            painter.setPen(QPen(colors.textAccentPrimary, 1.5));
            painter.setBrush(colors.bgSolid);
            painter.drawEllipse(outerRect);

            // 选中中心内圆点
            painter.setPen(Qt::NoPen);
            painter.setBrush(colors.textAccentPrimary);
            painter.drawEllipse(QRectF(9, centerY - 4, 8, 8));
        } else {
            painter.setPen(QPen(colors.strokeDefault, 1.2));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(outerRect);
        }
    }

    void onThemeUpdated() override {
        update();
    }

private:
    QString m_title;
    bool m_selected = false;
    std::function<void()> m_onClicked;
    fluent::textfields::Label* m_label = nullptr;
};

PermissionFloatingCard::PermissionFloatingCard(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void PermissionFloatingCard::setupUi() {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 14, 16, 14);
    mainLayout->setSpacing(10);

    // 1. Top Header Row: Shield Icon + Header Title + Tool Badge
    auto* topRow = new QWidget(this);
    auto* topLayout = new QHBoxLayout(topRow);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(8);

    auto* shieldIcon = new ShieldIconWidget(topRow);
    topLayout->addWidget(shieldIcon);

    m_headerTitleLabel = new fluent::textfields::Label(topRow);
    m_headerTitleLabel->setFluentTypography(Typography::FontRole::BodyStrong);
    m_headerTitleLabel->setText(tr("权限确认"));
    topLayout->addWidget(m_headerTitleLabel);

    m_toolBadge = new ToolPillBadge(topRow);
    topLayout->addWidget(m_toolBadge);

    topLayout->addStretch(1);
    mainLayout->addWidget(topRow);

    // 2. Reason Label (Secondary Text Role)
    m_reasonLabel = new fluent::textfields::Label(this);
    m_reasonLabel->setFluentTypography(Typography::FontRole::Caption);
    m_reasonLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
    m_reasonLabel->setWordWrap(true);
    mainLayout->addWidget(m_reasonLabel);

    // 3. Arguments Monospace Code Surface
    m_argsSurface = new ArgumentsCodeSurface(this);
    mainLayout->addWidget(m_argsSurface);

    // 4. 垂直 5 项单选选项列表
    auto* optionsContainer = new QWidget(this);
    auto* optionsLayout = new QVBoxLayout(optionsContainer);
    optionsLayout->setContentsMargins(0, 4, 0, 4);
    optionsLayout->setSpacing(4);

    const QStringList optionTitles = {
        tr("仅一次 (仅允许当前单次调用)"),
        tr("本对话 (在当前会话中记住此授权)"),
        tr("本项目 (在当前项目中记住此授权)"),
        tr("全局 (全局记住此授权)")
    };

    for (int i = 0; i < optionTitles.size(); ++i) {
        auto* row = new ScopeRadioRow(optionTitles[i], [this, i]() {
            selectOption(i);
        }, optionsContainer);
        m_radioRows.append(row);
        optionsLayout->addWidget(row);
    }

    // 5. 第 5 项：输入框选项
    auto* customInputRow = new QWidget(optionsContainer);
    auto* customInputLayout = new QHBoxLayout(customInputRow);
    customInputLayout->setContentsMargins(4, 2, 4, 2);
    customInputLayout->setSpacing(8);

    auto* customRadio = new ScopeRadioRow(QString(), [this]() {
        selectOption(4);
    }, customInputRow);
    customRadio->setFixedWidth(24);
    m_radioRows.append(customRadio);
    customInputLayout->addWidget(customRadio);

    m_customInputEdit = new fluent::textfields::LineEdit(customInputRow);
    m_customInputEdit->setPlaceholderText(tr("输入不同意的原因或替代建议（按 Enter 确认并提交）..."));
    customInputLayout->addWidget(m_customInputEdit);

    optionsLayout->addWidget(customInputRow);
    mainLayout->addWidget(optionsContainer);

    // 输入框回车触发 Approve
    connect(m_customInputEdit, &QLineEdit::returnPressed, this, [this]() {
        selectOption(4);
        triggerApprove();
    });

    // 6. 底部操作栏：右侧放置 [Skip] 与 [Approve]
    auto* bottomRow = new QWidget(this);
    auto* bottomLayout = new QHBoxLayout(bottomRow);
    bottomLayout->setContentsMargins(0, 4, 0, 0);
    bottomLayout->setSpacing(8);

    bottomLayout->addStretch(1);

    m_skipBtn = new fluent::basicinput::Button(bottomRow);
    m_skipBtn->setText(tr("Skip (跳过)"));
    m_skipBtn->setFluentSize(fluent::basicinput::Button::ButtonSize::Small);
    m_skipBtn->setCriticalOnHover(true);
    bottomLayout->addWidget(m_skipBtn);

    m_approveBtn = new fluent::basicinput::Button(bottomRow);
    m_approveBtn->setText(tr("允许 (Approve)"));
    m_approveBtn->setFluentStyle(fluent::basicinput::Button::ButtonStyle::Accent);
    m_approveBtn->setFluentSize(fluent::basicinput::Button::ButtonSize::Small);
    bottomLayout->addWidget(m_approveBtn);

    mainLayout->addWidget(bottomRow);

    // 默认选中第 0 项（仅一次）
    selectOption(0);

    // 按钮信号连接
    connect(m_skipBtn, &QPushButton::clicked, this, &PermissionFloatingCard::triggerSkip);
    connect(m_approveBtn, &QPushButton::clicked, this, &PermissionFloatingCard::triggerApprove);
}

void PermissionFloatingCard::selectOption(int index) {
    m_selectedOptionIndex = index;
    for (int i = 0; i < m_radioRows.size(); ++i) {
        m_radioRows[i]->setSelected(i == index);
    }
}

void PermissionFloatingCard::triggerApprove() {
    if (m_currentCall.id.isEmpty()) return;

    domain::agent::PermissionScope scope = domain::agent::PermissionScope::Once;
    if (m_selectedOptionIndex == 1) {
        scope = domain::agent::PermissionScope::Run;
    } else if (m_selectedOptionIndex == 2) {
        scope = domain::agent::PermissionScope::Project;
    } else if (m_selectedOptionIndex == 3) {
        scope = domain::agent::PermissionScope::Global;
    } else {
        scope = domain::agent::PermissionScope::Once;
    }

    const QString customText = m_customInputEdit ? m_customInputEdit->text().trimmed() : QString();
    emit permissionDecided(m_currentCall.id, true, scope, customText);
}

void PermissionFloatingCard::triggerSkip() {
    if (m_currentCall.id.isEmpty()) return;

    const QString customText = m_customInputEdit ? m_customInputEdit->text().trimmed() : QString();
    emit permissionDecided(m_currentCall.id, false, domain::agent::PermissionScope::Once, customText);
}

void PermissionFloatingCard::setPermission(
    const domain::agent::ToolCall& call,
    const domain::agent::ToolPermission& permission,
    int currentIndex,
    int totalCount
) {
    m_currentCall = call;
    m_currentPermission = permission;

    if (totalCount > 1) {
        m_headerTitleLabel->setText(tr("权限确认 (%1/%2)").arg(currentIndex).arg(totalCount));
    } else {
        m_headerTitleLabel->setText(tr("权限确认"));
    }

    m_toolBadge->setText(call.name);

    const QString reason = permission.reason.trimmed().isEmpty()
        ? tr("智能体请求执行此操作，请核对参数后确认是否允许。")
        : permission.reason;
    m_reasonLabel->setText(reason);

    // 格式化参数
    QString formattedArgs = call.arguments;
    const auto doc = QJsonDocument::fromJson(call.arguments.toUtf8());
    if (!doc.isNull()) {
        formattedArgs = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
    }
    m_argsSurface->setCodeText(formattedArgs);

    if (m_customInputEdit) {
        m_customInputEdit->clear();
    }
    selectOption(0);
}

void PermissionFloatingCard::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto& colors = themeColorsRef();
    const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);

    painter.setBrush(colors.bgLayer);
    painter.setPen(QPen(colors.strokeCard, 1));
    painter.drawRoundedRect(r, 8.0, 8.0);
}

void PermissionFloatingCard::onThemeUpdated() {
    update();
}

} // namespace ui::screen::work
