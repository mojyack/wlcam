#include <linux/input.h>

#include "gawl/fc.hpp"
#include "gawl/misc.hpp"
#include "gawl/window.hpp"
#include "macros/unwrap.hpp"
#include "util/assert.hpp"
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
    const auto [width, height] = window->get_window_size();
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
    font.draw_fit_rect(*window, bar_rect, {1, 1, 1, 1}, movie ? "video" : "photo", 0, gawl::Align::Left, gawl::Align::Center);
    if(recording) {
        const auto ms  = record_timer.elapsed<std::chrono::milliseconds>();
        const auto sec = ms / 1000;
        const auto min = sec / 60;
        auto       buf = std::array<char, 10>();
        snprintf(buf.data(), buf.size(), "%02d:%02d.%03d", std::min(int(min), 99), int(sec % 60), int(ms % 1000));
        font.draw_fit_rect(*window, bar_rect, {1, 1, 1, 1}, buf.data(), 0, gawl::Align::Right, gawl::Align::Center);
    }

    gawl::unmask_alpha();
}

auto WindowCallbacks::on_pointer(const gawl::Point& pos) -> void {
    cursor = pos;
}

auto WindowCallbacks::on_click(const uint32_t button, const gawl::ButtonState state) -> void {
    if(button != BTN_LEFT) {
        return;
    }

    if(state != gawl::ButtonState::Press) {
        return;
    }

    const auto [width, height] = window->get_window_size();
    if(cursor.y >= height - bottom_bar_height) {
        movie = !movie;
        return;
    }

    if(movie) {
        context.camera_command = recording ? Command::StopRecording : Command::StartRecording;
    } else {
        context.camera_command = Command::TakePhoto;
    }
}

auto WindowCallbacks::init() -> bool {
    unwrap_ob_mut(font_path, gawl::find_fontpath_from_name("Noto Sans CJK JP"));
    font.init({std::move(font_path)}, bottom_bar_height * 0.8);
    return true;
}

auto WindowCallbacks::get_context() -> WindowContext& {
    return context;
}

WindowCallbacks::~WindowCallbacks() {
    if(recording) {
        context.camera_command = Command::StopRecording;
    }
}
