#include "ui-v4l2.hpp"
#include "macros/unwrap.hpp"

namespace {
auto refresh_ctrls(V4L2ControlBundle& bundle) -> bool {
    for(auto& ctrl : bundle.ctrls) {
        unwrap_mut(c, v4l2::query_control(bundle.fd, ctrl.id));
        ctrl = std::move(c);
    }
    if(bundle.fixup) {
        bundle.fixup();
    }
    return true;
}
} // namespace

auto V4L2Menu::get_count() const -> size_t {
    return ctrl->menus.size();
}

auto V4L2Menu::get_value() const -> size_t {
    for(auto i = 0uz; i < ctrl->menus.size(); i += 1) {
        if(ctrl->menus[i].index == uint32_t(ctrl->current)) {
            return i;
        }
    }
    PANIC("menu bug current={}", ctrl->current);
}

auto V4L2Menu::get_label(size_t value) const -> std::string_view {
    return ctrl->menus[value].name;
}

auto V4L2Menu::set_value(size_t value) -> void {
    ensure(v4l2::set_control(bundle->fd, ctrl->id, ctrl->menus[value].index));
    ensure(refresh_ctrls(*bundle));
}

auto V4L2MenuButton::get_label() -> std::string_view {
    return menu.ctrl->name;
}

auto V4L2MenuButton::on_pressed() -> void {
    if(expand.get_index() == Expand::index_of<Menu*>) {
        expand.reset();
        pressed = false;
    } else {
        expand.emplace<Menu*>(&menu);
    }
}

auto V4L2MenuButton::is_active() -> bool {
    return !menu.ctrl->inactive && !menu.ctrl->ro;
}

auto V4L2Slider::get_range() const -> std::array<int, 2> {
    return {int(ctrl->min / ctrl->step), int(ctrl->max / ctrl->step)};
}

auto V4L2Slider::get_value() const -> int {
    return ctrl->current / ctrl->step;
}

auto V4L2Slider::set_value(int value) -> void {
    value *= ctrl->step;
    ensure(v4l2::set_control(bundle->fd, ctrl->id, value));
    // no need to refresh, slider should not cause control (de)activation
    ctrl->current = value;
}

auto V4L2SliderButton::get_label() -> std::string_view {
    return slider.ctrl->name;
}

auto V4L2SliderButton::on_pressed() -> void {
    if(expand.get_index() == Expand::index_of<Slider*>) {
        expand.reset();
        pressed = false;
    } else {
        expand.emplace<Slider*>(&slider);
    }
}

auto V4L2SliderButton::is_active() -> bool {
    return !slider.ctrl->inactive && !slider.ctrl->ro;
}

auto V4L2Button::get_label() -> std::string_view {
    return ctrl->name;
}

auto V4L2Button::on_pressed() -> void {
    if(ctrl->current) {
        pressed = false;
    }
    ensure(v4l2::set_control(bundle->fd, ctrl->id, !ctrl->current));
    ensure(refresh_ctrls(*bundle));
}

auto V4L2Button::is_active() -> bool {
    return !ctrl->inactive && !ctrl->ro;
}

auto build_buttons_from_controls(V4L2ControlBundle& bundle, std::vector<std::unique_ptr<Button>>& buttons) -> void {
    for(auto& ctrl : bundle.ctrls) {
        auto element = (V4L2Element*)(nullptr);
        auto button  = (Button*)(nullptr);
        switch(ctrl.type) {
        case v4l2::ControlType::Int: {
            const auto btn = new V4L2SliderButton();
            element        = &btn->slider;
            button         = btn;
        } break;
        case v4l2::ControlType::Bool: {
            const auto btn = new V4L2Button();
            element        = btn;
            button         = btn;
            btn->pressed   = ctrl.current;
        } break;
        case v4l2::ControlType::Menu: {
            const auto btn = new V4L2MenuButton();
            element        = &btn->menu;
            button         = btn;
        } break;
        }
        element->bundle = &bundle;
        element->ctrl   = &ctrl;
        buttons.emplace_back(button);
    }
}
