#include "PermissionFloatingCard.h"

#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
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

PermissionFloatingCard::PermissionFloatingCard(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void PermissionFloatingCard::setupUi() {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 12, 14, 12);
    mainLayout->setSpacing(8);

    // 1. Top Header Row: Fluent Shield Icon + Header Title + Tool Pill Badge
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

    // 4. Action Buttons Row (Fluent Button Standards)
    auto* btnRow = new QWidget(this);
    auto* btnLayout = new QHBoxLayout(btnRow);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(8);

    m_denyBtn = new fluent::basicinput::Button(btnRow);
    m_denyBtn->setText(tr("拒绝"));
    m_denyBtn->setCriticalOnHover(true);
    m_denyBtn->setFluentSize(fluent::basicinput::Button::ButtonSize::Small);
    btnLayout->addWidget(m_denyBtn);

    btnLayout->addStretch(1);

    m_allowOnceBtn = new fluent::basicinput::Button(btnRow);
    m_allowOnceBtn->setText(tr("允许本次"));
    m_allowOnceBtn->setFluentStyle(fluent::basicinput::Button::ButtonStyle::Accent);
    m_allowOnceBtn->setFluentSize(fluent::basicinput::Button::ButtonSize::Small);
    btnLayout->addWidget(m_allowOnceBtn);

    m_allowRunBtn = new fluent::basicinput::Button(btnRow);
    m_allowRunBtn->setText(tr("本次任务允许"));
    m_allowRunBtn->setFluentSize(fluent::basicinput::Button::ButtonSize::Small);
    btnLayout->addWidget(m_allowRunBtn);

    m_allowProjectBtn = new fluent::basicinput::Button(btnRow);
    m_allowProjectBtn->setText(tr("项目永久允许"));
    m_allowProjectBtn->setFluentSize(fluent::basicinput::Button::ButtonSize::Small);
    btnLayout->addWidget(m_allowProjectBtn);

    mainLayout->addWidget(btnRow);

    // Signals connection
    connect(m_denyBtn, &QPushButton::clicked, this, [this] {
        if (!m_currentCall.id.isEmpty()) {
            emit permissionDecided(m_currentCall.id, false, domain::agent::PermissionScope::Once);
        }
    });
    connect(m_allowOnceBtn, &QPushButton::clicked, this, [this] {
        if (!m_currentCall.id.isEmpty()) {
            emit permissionDecided(m_currentCall.id, true, domain::agent::PermissionScope::Once);
        }
    });
    connect(m_allowRunBtn, &QPushButton::clicked, this, [this] {
        if (!m_currentCall.id.isEmpty()) {
            emit permissionDecided(m_currentCall.id, true, domain::agent::PermissionScope::Run);
        }
    });
    connect(m_allowProjectBtn, &QPushButton::clicked, this, [this] {
        if (!m_currentCall.id.isEmpty()) {
            emit permissionDecided(m_currentCall.id, true, domain::agent::PermissionScope::Project);
        }
    });
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

    // Format arguments nicely if JSON
    QString formattedArgs = call.arguments;
    const auto doc = QJsonDocument::fromJson(call.arguments.toUtf8());
    if (!doc.isNull()) {
        formattedArgs = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
    }
    m_argsSurface->setCodeText(formattedArgs);
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
