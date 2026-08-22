#pragma once

#include <QString>

namespace core::markdown {

class MarkdownRenderer
{
public:
    MarkdownRenderer() = default;

    void setAllowHtml(bool allow) { m_allowHtml = allow; }
    bool allowHtml() const { return m_allowHtml; }

    QString renderFragment(const QString &markdown) const;
    QString wrapDocument(const QString &fragment, const QString &css) const;

private:
    bool m_allowHtml = true;
};

} // namespace core::markdown
