#pragma once

#include "ui/base/BasePage.h"

class QVBoxLayout;

namespace fluent::textfields {
    class Label;
}

namespace ui::screen::knowledge {
    class KnowledgeViewModel;
    struct KnowledgeState;

    class KnowledgePage : public ui::base::BasePage {
        Q_OBJECT

    public:
        explicit KnowledgePage(QWidget *parent = nullptr);

        ~KnowledgePage() override = default;

    private:
        void setupUi();
        void setupViewModel();

        void render(const KnowledgeState &state);

        KnowledgeViewModel *m_viewModel = nullptr;
        QVBoxLayout *m_rootLayout = nullptr;
        fluent::textfields::Label *m_titleLabel = nullptr;
        fluent::textfields::Label *m_subtitleLabel = nullptr;
    };
} // namespace ui::screen::knowledge
