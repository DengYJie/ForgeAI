#pragma once

#include <litehtml.h>

class details_element : public litehtml::html_tag
{
public:
    using litehtml::html_tag::html_tag;

    void parse_attributes() override;

    bool is_open() const { return m_open; }
    void toggle();

private:
    bool m_open = false;
};
