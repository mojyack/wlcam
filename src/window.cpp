#include <linux/input.h>

#include "gawl/fc.hpp"
#include "gawl/misc.hpp"
#include "gawl/window.hpp"
#include "macros/unwrap.hpp"
#include "window.hpp"

constexpr auto bottom_bar_height = 32;

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
    gawl::clear_screen({0, 0, 0, 0});
    const auto [width, height] = window->window_size;
    const auto screen_rect     = gawl::Rectangle{{0, 0}, {1. * width, 1. * height}};
    const auto frame           = context.frame;
    if(frame) {
        frame->draw_fit_rect(*window, screen_rect);
    }

    gawl::mask_alpha();

    // shot animation
    if(shutter_anim > 0) {
        gawl::draw_rect(*window, screen_rect, {0.8, 0.8, 0.8, 1.0 * shutter_anim / shutter_anim_duration});
        shutter_anim -= 1;
    }

    // status bar
    const auto bar_rect = gawl::Rectangle{{0, 1. * height - bottom_bar_height}, screen_rect.b};
    gawl::draw_rect(*window, bar_rect, {0, 0, 0, 0.7});
    font.draw_fit_rect(*window, bar_rect, {1, 1, 1, 1}, movie ? "video" : "photo", {.align_x = gawl::Align::Left});
    if(!context.error_message.empty()) {
        font.draw_fit_rect(*window, bar_rect, {1, 0, 0, 1}, context.error_message.data(), {.align_x = gawl::Align::Right});
    } else {
        if(recording) {
            const auto ms  = record_timer.elapsed<std::chrono::milliseconds>();
            const auto sec = ms / 1000;
            const auto min = sec / 60;
            auto       buf = std::array<char, 10>();
            snprintf(buf.data(), buf.size(), "%02d:%02d.%03d", std::min(int(min), 99), int(sec % 60), int(ms % 1000));
            font.draw_fit_rect(*window, bar_rect, {1, 1, 1, 1}, buf.data(), {.align_x = gawl::Align::Right});
        }
    }

    gawl::unmask_alpha();
}

auto WindowCallbacks::on_created(gawl::Window* /*window*/) -> coop::Async<bool> {
    co_unwrap_v_mut(font_path, gawl::find_fontpath_from_name("Noto Sans CJK JP"));
    font.init({std::move(font_path)}, bottom_bar_height * 0.8);
    co_return true;
}

auto WindowCallbacks::on_pointer(const gawl::Point pos) -> coop::Async<bool> {
    cursor = pos;
    co_return true;
}

auto WindowCallbacks::on_click(const uint32_t button, const gawl::ButtonState state) -> coop::Async<bool> {
    if(button != BTN_LEFT) {
        co_return true;
    }

    if(state != gawl::ButtonState::Press) {
        co_return true;
    }

    const auto [width, height] = window->window_size;
    if(cursor.y >= height - bottom_bar_height) {
        movie = !movie;
        co_return true;
    }

    if(movie) {
        context.camera_command = recording ? Command::StopRecording : Command::StartRecording;
    } else {
        context.camera_command = Command::TakePhoto;
    }
    co_return true;
}

auto WindowCallbacks::get_window() const -> gawl::Window* {
    return window;
}

auto WindowCallbacks::get_context() -> WindowContext& {
    return context;
}

WindowCallbacks::~WindowCallbacks() {
    if(recording) {
        context.camera_command = Command::StopRecording;
    }
}
