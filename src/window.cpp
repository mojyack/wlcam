#include <linux/input.h>

#include "gawl/fc.hpp"
#include "gawl/misc.hpp"
#include "gawl/window.hpp"
#include "macros/unwrap.hpp"
#include "util/assert.hpp"
#include "window.hpp"

auto WindowCallbacks::refresh() -> void {
    // proc command
    constexpr auto shutter_anim_duration = 10;
    switch(std::exchange(context->ui_command, Command::None)) {
    case Command::TakePhotoDone:
        shutter_anim = shutter_anim_duration;
        break;
    case Command::StartRecordingDone:
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
    const auto [screen_width, screen_height] = window->get_window_size();
    const auto screen_rect                   = gawl::Rectangle{{0, 0}, {1. * screen_width, 1. * screen_height}};
    const auto frame                         = context->frame;
    if(frame) {
        frame->draw_fit_rect(*window, screen_rect);
    }

    gawl::mask_alpha();

    // shot animation
    if(shutter_anim > 0) {
        gawl::draw_rect(*window, screen_rect, {0.8, 0.8, 0.8, 1.0 * shutter_anim / shutter_anim_duration});
        shutter_anim -= 1;
    }

    gawl::unmask_alpha();
}

auto WindowCallbacks::on_click(const uint32_t button, const gawl::ButtonState state) -> void {
    if(button == BTN_RIGHT && state == gawl::ButtonState::Press) {
        movie = !movie;
        printf("movie: %d\n", movie);
    }

    if(button != BTN_LEFT) {
        return;
    }

    if(state == gawl::ButtonState::Press) {
        if(movie) {
            context->camera_command = recording ? Command::StopRecording : Command::StartRecording;
        } else {
            context->camera_command = Command::TakePhoto;
        }
    }
}

auto WindowCallbacks::init() -> bool {
    unwrap_ob_mut(font_path, gawl::find_fontpath_from_name("Noto Sans CJK JP"));
    font.init({std::move(font_path)}, 32);
    return true;
}

WindowCallbacks::WindowCallbacks(Context& context)
    : context(&context) {
}

WindowCallbacks::~WindowCallbacks() {
    if(recording) {
        context->camera_command = Command::StopRecording;
    }
}
