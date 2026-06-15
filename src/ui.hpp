#pragma once
#include <string_view>

#include "gawl/point.hpp"
#include "gawl/textrender.hpp"
#include "util/variant.hpp"

struct Menu {
    std::vector<gawl::WrappedText> wrapped_texts;

    virtual auto get_count() const -> size_t                       = 0;
    virtual auto get_value() const -> size_t                       = 0;
    virtual auto get_label(size_t value) const -> std::string_view = 0;
    virtual auto set_value(size_t value) -> void                   = 0;

    virtual ~Menu() {
    }
};

struct Slider {
    // runtime states
    gawl::Point min_pos       = {0, 0};
    gawl::Point max_pos       = {0, 0};
    gawl::Point displayed_pos = {0, 0};

    virtual auto get_range() const -> std::array<int, 2> = 0;
    virtual auto get_value() const -> int                = 0;
    virtual auto set_value(int value) -> void            = 0;

    virtual ~Slider() {
    }
};

using Expand = Variant<Menu*, Slider*>;

struct Button {
    virtual auto get_label() -> std::string_view = 0;
    virtual auto on_pressed() -> void            = 0;

    virtual auto is_active() -> bool {
        return true;
    }

    // runtime states
    gawl::Point       displayed_pos = {0, 0};
    gawl::WrappedText wrapped_text;
    bool              pressed = false;
    Expand            expand;

    virtual ~Button() {
    }
};
