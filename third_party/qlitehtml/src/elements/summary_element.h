#pragma once

#include <litehtml.h>

class summary_element : public litehtml::html_tag
{
public:
    using litehtml::html_tag::html_tag;

    void draw(litehtml::uint_ptr hdc,
              litehtml::pixel_t x,
              litehtml::pixel_t y,
              const litehtml::position *clip,
              const std::shared_ptr<litehtml::render_item> &ri) override;
};
