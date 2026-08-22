#include "details_element.h"

void details_element::parse_attributes()
{
    html_tag::parse_attributes();

    m_open = (get_attr("open") != nullptr);
}

void details_element::toggle()
{
    m_open = !m_open;
    if (m_open) {
        set_attr("open", "");
    } else {
        m_attrs.erase("open");
    }
}
