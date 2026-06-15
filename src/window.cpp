#include <linux/input.h>

#include "gawl/fc.hpp"
#include "gawl/misc.hpp"
#include "gawl/window.hpp"
#include "macros/coop-unwrap.hpp"
#include "ui.hpp"
#include "util/variant.hpp"
#include "window.hpp"

namespace colors {
constexpr auto palette_1 = gawl::parse_color_code(0x5F7470FF);
constexpr auto palette_2 = gawl::parse_color_code(0x889696FF);
constexpr auto palette_3 = gawl::parse_color_code(0xB8BDB5FF);
constexpr auto palette_4 = gawl::parse_color_code(0xD2D4C8FF);
constexpr auto palette_5 = gawl::parse_color_code(0xE0E2DBFF);

constexpr auto palette_white = gawl::parse_color_code(0xFFFFFFFF);
constexpr auto palette_black = gawl::parse_color_code(0x000000FF);

constexpr auto bg = palette_2;

constexpr auto button_bg_active   = palette_1;
constexpr auto button_fg_active   = palette_5;
constexpr auto button_bg_inactive = palette_3;
constexpr auto button_fg_inactive = palette_1;

constexpr auto slider_fg = palette_black;
constexpr auto slider_bg = palette_3;
} // namespace colors

constexpr auto button_size          = gawl::Point{48, 48};
constexpr auto button_shadow_offset = gawl::Point{4, 4};
constexpr auto button_label_size    = 8;

// layout

struct LayoutRule {
    gawl::Point button_step;
    gawl::Point menu_step;

    virtual auto preview_rect(std::array<int, 2> window_size) const -> gawl::Rectangle                                     = 0;
    virtual auto buttons_origin(std::array<int, 2> window_size) const -> gawl::Point                                       = 0;
    virtual auto slider_range(gawl::Point button_base, std::array<int, 2> window_size) const -> std::array<gawl::Point, 2> = 0;

    virtual ~LayoutRule() {
    }
};

/* horizontal, left to right */
struct LayoutRuleBottom : LayoutRule {
    auto preview_rect(std::array<int, 2> window_size) const -> gawl::Rectangle override {
        return {{0, 0}, {double(window_size[0]), double(window_size[1] - button_size.y)}};
    }

    auto buttons_origin(const std::array<int, 2> window_size) const -> gawl::Point override {
        return {0, double(window_size[1] - button_size.y)};
    }

    auto slider_range(const gawl::Point button_base, const std::array<int, 2> /*window_size*/) const -> std::array<gawl::Point, 2> override {
        const auto min = button_base + menu_step;
        const auto max = gawl::Point{min.x, std::max(min.y - button_size.y * 10, 0.0)};
        return {min, max};
    }

    LayoutRuleBottom() {
        button_step = {button_size.x, 0};
        menu_step   = {0, -button_size.y};
    }
};

/* vertical, bottom to top */
struct LayoutRuleLeft : LayoutRule {
    auto preview_rect(std::array<int, 2> window_size) const -> gawl::Rectangle override {
        return {{button_size.x, 0}, {double(window_size[0]), double(window_size[1])}};
    }

    auto buttons_origin(const std::array<int, 2> window_size) const -> gawl::Point override {
        return {0, double(window_size[1] - button_size.y)};
    }

    auto slider_range(gawl::Point button_base, std::array<int, 2> window_size) const -> std::array<gawl::Point, 2> override {
        const auto min = button_base + menu_step;
        const auto max = gawl::Point{std::min(min.x + button_size.x * 10, window_size[0] - button_size.x), min.y};
        return {min, max};
    }

    LayoutRuleLeft() {
        button_step = {0, -button_size.y};
        menu_step   = {button_size.x, 0};
    }
};

const auto rule_bottom = LayoutRuleBottom();
const auto rule_left   = LayoutRuleLeft();
const auto rules       = std::array{
    (LayoutRule*)&rule_bottom,
    (LayoutRule*)&rule_left,
};

auto button_layout = 0;

struct TakeButton : Button {
    WindowCallbacks* window;
    bool             last_movie = false;

    auto get_label() -> std::string_view override {
        if(last_movie != window->movie) {
            wrapped_text.reset();
            last_movie = window->movie;
        }
        return window->movie ? "Record" : "Take";
    }

    auto on_pressed() -> void override {
        if(window->movie) {
            if(window->recording) {
                window->context.camera_command = Command::StopRecording;
                pressed                        = false;
            } else {
                window->context.camera_command = Command::StartRecording;
            }
        } else {
            window->context.camera_command = Command::TakePhoto;
            pressed                        = false;
        }
    }
};

struct ModeButton : Button {
    WindowCallbacks* window;

    auto get_label() -> std::string_view override {
        return "Mode";
    }

    auto on_pressed() -> void override {
        if(window->movie && window->recording) {
            window->context.camera_command = Command::StopRecording;
        }
        window->movie = !window->movie;
        pressed       = false;
    }
};

struct LayoutButton : Button {
    auto get_label() -> std::string_view override {
        return "Layout";
    }

    auto on_pressed() -> void override {
        button_layout = (button_layout + 1) % rules.size();
        pressed       = false;
    }
};

struct ExitButton : Button {
    WindowCallbacks* window;

    auto get_label() -> std::string_view override {
        return "Exit";
    }

    auto on_pressed() -> void override {
        window->close();
        pressed = false;
    }
};

auto in_rect(const gawl::Rectangle& rect, const gawl::Point& point) -> bool {
    return rect.a.x <= point.x && rect.b.x > point.x && rect.a.y <= point.y && rect.b.y > point.y;
}

struct PressedButton {
    Button* button;
};

struct PressedMenu {
    Button* button;
    Menu*   menu;
    size_t  index;
};

struct PressedSlider {
    Button* button;
    Slider* slider;
};

using Pressed = Variant<PressedButton, PressedMenu, PressedSlider>;

auto pressed = Pressed();

constexpr auto bottom_bar_height = 32;

auto WindowCallbacks::draw_button(const gawl::Point& base, const std::string_view label, const bool pressed, const bool active, gawl::WrappedText& wrapped_text) -> void {
    const auto rect_size   = button_size - button_shadow_offset;
    const auto button_rect = gawl::Rectangle{base, base + rect_size};

    const auto& bg = active ? colors::button_bg_active : colors::button_bg_inactive;
    const auto& fg = active ? colors::button_fg_active : colors::button_fg_inactive;
    if(pressed) {
        gawl::draw_rect(*window, button_rect + button_shadow_offset, bg);
        font.draw_wrapped(*window, button_rect + button_shadow_offset, button_label_size, fg, label, wrapped_text, {.size = button_label_size});
    } else {
        gawl::draw_rect(*window, button_rect + button_shadow_offset, {0, 0, 0, 0.5});
        gawl::draw_rect(*window, button_rect, bg);
        font.draw_wrapped(*window, button_rect, button_label_size, fg, label, wrapped_text, {.size = button_label_size});
    }
}

auto WindowCallbacks::refresh() -> void {
    // proc command
    constexpr auto shutter_anim_duration = 10;
    switch(std::exchange(context.ui_command, Command::None)) {
    case Command::TakePhotoDone:
        shutter_anim = shutter_anim_duration;
        break;
    case Command::StartRecordingDone:
        record_timer.reset();
        recording = true;
        break;
    case Command::StopRecordingDone:
        recording = false;
        break;
    default:
        break;
    }

    // render
    gawl::clear_screen(colors::bg);
    gawl::mask_alpha();

    const auto& rule = *rules[button_layout];

    const auto preview_rect = rule.preview_rect(window->window_size);
    const auto frame        = context.frame;
    if(frame) {
        frame->draw_fit_rect(*window, preview_rect);
    }

    // shot animation
    if(shutter_anim > 0) {
        gawl::draw_rect(*window, preview_rect, {0.8, 0.8, 0.8, 1.0 * shutter_anim / shutter_anim_duration});
        shutter_anim -= 1;
    }

    // record time
    if(recording) {
        const auto ms  = record_timer.elapsed<std::chrono::milliseconds>();
        const auto sec = ms / 1000;
        const auto min = sec / 60;
        auto       str = std::format("{:02d}:{:02d}.{:03d}", min, sec % 60, ms % 1000);
        font.draw_fit_rect(*window, preview_rect, colors::palette_white, str, {.align_x = gawl::Align::Right, .align_y = gawl::Align::Right});
    }

    // ui elements
    auto base = rule.buttons_origin(window->window_size);
    for(const auto& button : buttons) {
        draw_button(base, button->get_label(), button->pressed, button->is_active(), button->wrapped_text);
        button->displayed_pos = base;
        // menu
        switch(button->expand.get_index()) {
        case Expand::index_of<Menu*>: {
            auto& menu  = *button->expand.as<Menu*>();
            auto  mbase = base + rule.menu_step;

            const auto press = pressed.get<PressedMenu>();
            const auto count = menu.get_count();
            if(menu.wrapped_texts.size() != menu.get_count()) {
                menu.wrapped_texts.clear();
                menu.wrapped_texts.resize(count);
            }
            for(auto i = 0uz; i < count; i += 1) {
                const auto active = i == menu.get_value() || (press != nullptr && press->menu == &menu && press->index == i);
                draw_button(mbase, menu.get_label(i), active, true, menu.wrapped_texts[i]);
                mbase += rule.menu_step;
            }
        } break;
        case Expand::index_of<Slider*>: {
            const auto [pmin, pmax] = rule.slider_range(base, window->window_size);
            { // background
                const auto left   = gawl::Point{std::min(pmin.x, pmax.x), std::min(pmin.y, pmax.y)};
                const auto right  = gawl::Point{std::max(pmin.x, pmax.x), std::max(pmin.y, pmax.y)};
                auto       ground = gawl::Rectangle{left + button_size * 0.5, right + button_size * 0.5}.expand(4, 4);
                gawl::draw_rect(*window, ground, colors::slider_bg);
                gawl::draw_rect(*window, {left, left + button_size}, colors::slider_bg);
                gawl::draw_rect(*window, {right, right + button_size}, colors::slider_bg);
            }

            // min/max labels
            auto& slider            = *button->expand.as<Slider*>();
            const auto [vmin, vmax] = slider.get_range();
            font.draw_fit_rect(*window, gawl::Rectangle{pmin, pmin + button_size}, colors::slider_fg, std::to_string(vmin), {.size = button_label_size});
            font.draw_fit_rect(*window, gawl::Rectangle{pmax, pmax + button_size}, colors::slider_fg, std::to_string(vmax), {.size = button_label_size});

            // knob
            const auto vcur  = slider.get_value();
            const auto pdist = pmax - pmin;
            const auto pknob = pmin + pdist * (1. * (vcur - vmin) / (vmax - vmin));

            const auto press  = pressed.get<PressedSlider>();
            const auto active = press != nullptr && press->slider == &slider;

            auto wrapped_text = gawl::WrappedText();
            draw_button(pknob, std::to_string(vcur), active, true, wrapped_text);

            slider.min_pos       = pmin;
            slider.max_pos       = pmax;
            slider.displayed_pos = pknob;
        } break;
        }
        base += rule.button_step;
    }
    gawl::unmask_alpha();
}

auto WindowCallbacks::on_created(gawl::Window* /*window*/) -> coop::Async<bool> {
    coop_unwrap_mut(font_path, gawl::find_fontpath_from_name("Noto Sans CJK JP"));
    font.init({std::move(font_path)}, bottom_bar_height * 0.8);
    co_return true;
}

auto WindowCallbacks::on_pointer(const gawl::Point pos) -> coop::Async<bool> {
    cursor = pos;
    switch(pressed.get_index()) {
    case Pressed::index_of<PressedButton>: {
        auto&      button = *pressed.as<PressedButton>().button;
        const auto rect   = gawl::Rectangle{button.displayed_pos, button.displayed_pos + button_size};
        if(!in_rect(rect, cursor)) {
            button.pressed = false;
            pressed.reset();
        }
    } break;
    case Pressed::index_of<PressedMenu>: {
        const auto& press = pressed.as<PressedMenu>();
        const auto& rule  = *rules[button_layout];
        const auto& base  = press.button->displayed_pos;
        const auto  mbase = base + rule.menu_step * press.index;
        const auto  rect  = gawl::Rectangle{mbase, mbase + button_size};
        if(!in_rect(rect, cursor)) {
            pressed.reset();
        }
    } break;
    case Pressed::index_of<PressedSlider>: {
        auto& slider = *pressed.as<PressedSlider>().slider;

        const auto a     = slider.max_pos - slider.min_pos;
        const auto b     = (cursor - button_size * 0.5) - slider.min_pos;
        const auto adota = a.x * a.x + a.y * a.y;
        const auto adotb = a.x * b.x + a.y * b.y;
        const auto rate  = std::clamp(adotb / adota, 0.0, 1.0);

        const auto [vmin, vmax] = slider.get_range();
        const auto vnew         = vmin + (vmax - vmin) * rate;
        slider.set_value(vnew);
    } break;
    }
    co_return true;
}

auto WindowCallbacks::on_click(const uint32_t button, const gawl::ButtonState state) -> coop::Async<bool> {
    if(button != BTN_LEFT) {
        co_return true;
    }

    if(state != gawl::ButtonState::Press) {
        switch(pressed.get_index()) {
        case Pressed::index_of<PressedButton>: {
            auto& button = *pressed.as<PressedButton>().button;
            if(state == gawl::ButtonState::Release) {
                button.on_pressed();
            }
            pressed.reset();
        } break;
        case Pressed::index_of<PressedMenu>: {
            auto& press = pressed.as<PressedMenu>();
            if(state == gawl::ButtonState::Release) {
                press.menu->set_value(press.index);
            }
            pressed.reset();
        } break;
        case Pressed::index_of<PressedSlider>: {
            pressed.reset();
        } break;
        }
        co_return true;
    }

    for(const auto& button : buttons) {
        const auto rect = gawl::Rectangle{button->displayed_pos, button->displayed_pos + button_size};
        if(in_rect(rect, cursor) && (button->is_active() || button->pressed)) {
            pressed.emplace<PressedButton>(button.get());
            button->pressed = true;
            co_return true;
        }
        switch(button->expand.get_index()) {
        case Expand::index_of<Menu*>: {
            auto&       menu  = *button->expand.as<Menu*>();
            const auto& rule  = *rules[button_layout];
            auto        mbase = button->displayed_pos + rule.menu_step;
            for(auto i = 0uz; i < menu.get_count(); i += 1) {
                const auto rect = gawl::Rectangle{mbase, mbase + button_size};
                if(in_rect(rect, cursor)) {
                    pressed.emplace<PressedMenu>(button.get(), &menu, i);
                    co_return true;
                }
                mbase += rule.menu_step;
            }
        } break;
        case Expand::index_of<Slider*>: {
            auto&      slider = *button->expand.as<Slider*>();
            const auto rect   = gawl::Rectangle{slider.displayed_pos, slider.displayed_pos + button_size};
            if(in_rect(rect, cursor)) {
                pressed.emplace<PressedSlider>(button.get(), &slider);
                co_return true;
            }
        } break;
        }
    }
    co_return true;
}

auto WindowCallbacks::get_window() const -> gawl::Window* {
    return window;
}

auto WindowCallbacks::get_context() -> WindowContext& {
    return context;
}

WindowCallbacks::WindowCallbacks() {
    auto take   = new TakeButton();
    auto mode   = new ModeButton();
    auto layout = new LayoutButton();
    auto exit   = new ExitButton();

    take->window = this;
    mode->window = this;
    exit->window = this;

    buttons.emplace_back(take);
    buttons.emplace_back(mode);
    buttons.emplace_back(layout);
    buttons.emplace_back(exit);
}

WindowCallbacks::~WindowCallbacks() {
    if(recording) {
        context.camera_command = Command::StopRecording;
    }
}
