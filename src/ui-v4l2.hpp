#pragma once
#include <memory>

#include "ui.hpp"
#include "v4l2.hpp"

struct V4L2ControlBundle {
    int                        fd;
    std::vector<v4l2::Control> ctrls;
    std::function<void()>      fixup;
};

struct V4L2Element {
    V4L2ControlBundle* bundle;
    v4l2::Control*     ctrl;
};

struct V4L2Menu : V4L2Element, Menu {
    auto get_count() const -> size_t override;
    auto get_value() const -> size_t override;
    auto get_label(size_t value) const -> std::string_view override;
    auto set_value(size_t value) -> void override;
};

struct V4L2MenuButton : Button {
    V4L2Menu menu;

    auto get_label() -> std::string_view override;
    auto on_pressed() -> void override;
    auto is_active() -> bool override;
};

struct V4L2Slider : V4L2Element, Slider {
    auto get_range() const -> std::array<int, 2> override;
    auto get_value() const -> int override;
    auto set_value(int value) -> void override;
};

struct V4L2SliderButton : Button {
    V4L2Slider slider;

    auto get_label() -> std::string_view override;
    auto on_pressed() -> void override;
    auto is_active() -> bool override;
};

struct V4L2Button : V4L2Element, Button {
    auto get_label() -> std::string_view override;
    auto on_pressed() -> void override;
    auto is_active() -> bool override;
};

auto build_buttons_from_controls(V4L2ControlBundle& bundle, std::vector<std::unique_ptr<Button>>& buttons) -> void;
