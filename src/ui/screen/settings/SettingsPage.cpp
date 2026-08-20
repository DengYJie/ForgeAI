#include "SettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QResizeEvent>
#include <QPainter>
#include <QPalette>

#include <FluentQt/Design.h>
#include <FluentQt/Foundation.h>
#include <FluentQt/Scrolling.h>
#include <FluentQt/Layout.h>
#include <FluentQt/TextFields.h>
#include <FluentQt/BasicInput.h>

#include "core/settings/SettingsRegistry.h"
#include "ui/screen/settings/SettingsUIRegistry.h"

using namespace fluent;

namespace ui::screen::settings {
    namespace {
        constexpr int kStackedCardWidthThreshold = 500;
        constexpr int kNarrowWidthThreshold = 640;

        class SettingsScrollView : public fluent::scrolling::ScrollView {
        public:
            explicit SettingsScrollView(QWidget *parent = nullptr)
                : fluent::scrolling::ScrollView(parent) {
                setAttribute(Qt::WA_OpaquePaintEvent, false);
                QPalette pal = palette();
                pal.setColor(QPalette::Window, Qt::transparent);
                setPalette(pal);
            }

        protected:
            void paintEvent(QPaintEvent *) override {
                // Keep transparent to show mica background
            }
        };

        class SecondaryLabel : public fluent::textfields::Label {
        public:
            SecondaryLabel(const QString &text, QWidget *parent = nullptr)
                : Label(text, parent) {
                setTextColorRole(TextColorRole::Secondary);
            }
        };
    } // namespace

    class SettingsCardItem : public fluent::layout::Card {
    public:
        SettingsCardItem(const QString &iconGlyph,
                         const QString &title,
                         const QString &subtitle,
                         QWidget *trailingWidget,
                         QWidget *parent = nullptr)
            : fluent::layout::Card(parent)
              , m_trailing(trailingWidget) {
            setObjectName(QStringLiteral("settingsCardItem"));
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            setMinimumHeight(68);

            m_layout = new QGridLayout(this);
            m_layout->setContentsMargins(16, 12, 16, 12);
            m_layout->setHorizontalSpacing(16);
            m_layout->setVerticalSpacing(8);

            if (!iconGlyph.isEmpty()) {
                auto *iconView = new fluent::FontIcon(iconGlyph, this);
                iconView->setObjectName(QStringLiteral("settingsCardIcon"));
                iconView->setIconSize(Typography::IconSize::Standard);
                iconView->setFixedSize(28, 28);
                m_layout->addWidget(iconView, 0, 0, Qt::AlignVCenter);
            }

            auto *textColumn = new QWidget(this);
            auto *textLayout = new QVBoxLayout(textColumn);
            textLayout->setContentsMargins(0, 0, 0, 0);
            textLayout->setSpacing(2);

            auto *titleLabel = new fluent::textfields::Label(title, textColumn);
            titleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
            titleLabel->setFluentTypography(Typography::FontRole::BodyStrong);
            textLayout->addWidget(titleLabel);

            if (!subtitle.isEmpty()) {
                auto *subtitleLabel = new SecondaryLabel(subtitle, textColumn);
                subtitleLabel->setFluentTypography(Typography::FontRole::Caption);
                subtitleLabel->setWordWrap(true);
                textLayout->addWidget(subtitleLabel);
            }

            m_layout->addWidget(textColumn, 0, 1, Qt::AlignVCenter);

            if (m_trailing) {
                m_trailing->setParent(this);
                m_layout->addWidget(m_trailing, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
            }

            updateLayoutMode();
        }

        void setStacked(bool stacked) {
            if (m_stacked == stacked || !m_trailing) return;
            m_stacked = stacked;

            m_layout->removeWidget(m_trailing);
            if (m_stacked) {
                m_layout->addWidget(m_trailing, 1, 1, Qt::AlignLeft | Qt::AlignVCenter);
                setMinimumHeight(100);
            } else {
                m_layout->addWidget(m_trailing, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
                setMinimumHeight(68);
            }
            m_trailing->show();
            updateGeometry();
        }

    protected:
        void resizeEvent(QResizeEvent *event) override {
            Card::resizeEvent(event);
            updateLayoutMode();
        }

    private:
        void updateLayoutMode() {
            setStacked(width() > 0 && width() < kStackedCardWidthThreshold);
        }

        QGridLayout *m_layout = nullptr;
        QWidget *m_trailing = nullptr;
        bool m_stacked = false;
    };

    SettingsPage::SettingsPage(QWidget *parent)
        : QWidget(parent) {
        setObjectName(QStringLiteral("settingsPage"));
        setAutoFillBackground(false);

        setupUi();
        setupConnections();
    }

    void SettingsPage::setupUi() {
        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);

        auto *scrollArea = new SettingsScrollView(this);
        scrollArea->setObjectName(QStringLiteral("settingsScrollArea"));
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setHorizontalScrollMode(fluent::scrolling::ScrollView::ScrollMode::Disabled);
        scrollArea->setHorizontalScrollBarVisibility(fluent::scrolling::ScrollView::ScrollBarVisibility::Hidden);

        m_viewport = new QWidget(scrollArea);
        m_viewport->setObjectName(QStringLiteral("settingsViewport"));
        m_viewport->setAutoFillBackground(false);

        m_contentLayout = new QVBoxLayout(m_viewport);
        m_contentLayout->setContentsMargins(36, 28, 36, 36);
        m_contentLayout->setSpacing(6);

        // 1. 页面主标题
        m_titleLabel = new fluent::textfields::Label(tr("Settings"), m_viewport);
        m_titleLabel->setObjectName(QStringLiteral("settingsPageTitle"));
        m_titleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
        m_titleLabel->setFluentTypography(Typography::FontRole::Title);
        m_contentLayout->addWidget(m_titleLabel);
        m_contentLayout->addSpacing(12);

        // 2. 分组：外观与行为 (Appearance & behavior)
        m_contentLayout->addWidget(createSectionHeader(tr("Appearance & behavior")));

        // 动态加载所有注册在该分组下的 UI 组件
        auto factories = SettingsUIRegistry::instance().allFactories();
        for (auto factory: factories) {
            auto provider = core::settings::SettingsRegistry::instance().getProvider(factory->targetProviderId());
            if (provider && provider->category() == "Appearance & behavior") {
                QWidget *control = factory->createControlWidget(m_viewport);
                m_contentLayout->addWidget(createSettingsCard(
                    factory->iconGlyph(),
                    factory->title(),
                    factory->subtitle(),
                    control
                ));
            }
        }

        m_contentLayout->addStretch(1);

        scrollArea->setWidget(m_viewport);
        rootLayout->addWidget(scrollArea);

        updateResponsiveLayout();
    }

    void SettingsPage::setupConnections() {
    }

    QWidget *SettingsPage::createSectionHeader(const QString &title) {
        auto *label = new fluent::textfields::Label(title, this);
        label->setObjectName(QStringLiteral("settingsSectionHeader"));
        label->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
        label->setFluentTypography(Typography::FontRole::BodyStrong);
        label->setContentsMargins(2, 12, 0, 4);
        return label;
    }

    QWidget *SettingsPage::createSettingsCard(const QString &iconGlyph,
                                              const QString &title,
                                              const QString &subtitle,
                                              QWidget *trailingWidget) {
        auto *card = new SettingsCardItem(iconGlyph, title, subtitle, trailingWidget, m_viewport);
        m_cards.append(card);
        return card;
    }

    void SettingsPage::onThemeUpdated() {
        if (m_titleLabel) m_titleLabel->onThemeUpdated();
        update();
    }

    void SettingsPage::resizeEvent(QResizeEvent *event) {
        QWidget::resizeEvent(event);
        updateResponsiveLayout();
    }

    void SettingsPage::updateResponsiveLayout() {
        if (!m_contentLayout) return;
        const bool narrow = width() > 0 && width() < kNarrowWidthThreshold;
        const int marginH = narrow ? 20 : 36;
        m_contentLayout->setContentsMargins(marginH, 24, marginH, 36);

        for (auto *card: m_cards) {
            if (card) {
                card->setStacked(narrow || (card->width() > 0 && card->width() < kStackedCardWidthThreshold));
            }
        }
    }
} // namespace ui::screen::settings
