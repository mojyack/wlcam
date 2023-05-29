#include <gawl/polygon.hpp>

#include "window.hpp"

namespace {
auto draw_circle_line(auto& window, const gawl::Point pos, const double r, const double line, const gawl::Color color) {
    const auto vertices_ = gawl::trianglate_circle(pos, r);
    const auto vertices  = std::span(vertices_.begin() + 1, vertices_.end());
    gawl::draw_outlines(window, vertices, color, line);
}
} // namespace

auto Window::calc_button_pos() -> gawl::Point {
    const auto [screen_width, screen_height] = window.get_window_size();
    return gawl::Point{
        (screen_width - (button_r + button_line_width) * 2) * button_pos.x + button_r,
        (screen_height - (button_r + button_line_width) * 2) * button_pos.y + button_r,
    };
}

auto Window::refresh_callback() -> void {
    // proc command
    constexpr auto shutter_anim_duration = 10;
    switch(std::exchange(context.ui_command, Command::None)) {
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
    auto new_pixel_buffers = PixelBuffers();
    {
        auto [lock, pixel_buffers] = context.critical_pixel_buffers.access();
        if(!pixel_buffers.y.empty()) {
            new_pixel_buffers = std::move(pixel_buffers);
        }
    }
    if(!new_pixel_buffers.y.empty()) {
        switch(context.pixel_format) {
        case PixelFormat::MPEG:
            if(auto ptr = graphic.get<YUYVPlanarGraphic>(); ptr != nullptr) {
                ptr->update_texture(new_pixel_buffers.y, new_pixel_buffers.u, new_pixel_buffers.v);
            } else {
                graphic.emplace<YUYVPlanarGraphic>(new_pixel_buffers.y, new_pixel_buffers.u, new_pixel_buffers.v);
            }
            break;
        case PixelFormat::YUYV:
            if(auto ptr = graphic.get<YUYVInterleavedGraphic>(); ptr != nullptr) {
                ptr->update_texture(new_pixel_buffers.y);
            } else {
                graphic.emplace<YUYVInterleavedGraphic>(new_pixel_buffers.y);
            }
            break;
        }
    }

    gawl::clear_screen({0, 0, 0, 0});
    const auto [screen_width, screen_height] = window.get_window_size();
    const auto screen_rect                   = gawl::Rectangle{{0, 0}, {1. * screen_width, 1. * screen_height}};
    if(graphic.is_valid()) {
        graphic.apply([this, screen_rect](auto& g) { g.draw_fit_rect(window, screen_rect); });
    }

    gawl::mask_alpha();

    // shot animation
    if(shutter_anim > 0) {
        gawl::draw_rect(window, screen_rect, {0.8, 0.8, 0.8, 1.0 * shutter_anim / shutter_anim_duration});
        shutter_anim -= 1;
    }

    // draw button
    {
        const auto button_pos = calc_button_pos();
        draw_circle_line(window,
                         button_pos,
                         button_r,
                         button_line_width,
                         {1, 1, 1, 0.5});
        if(movie) {
            if(recording) {
                const auto rect_a = gawl::Rectangle{
                    {button_pos.x - button_r * 0.6, button_pos.y - button_r * 0.6},
                    {button_pos.x - button_r * 0.2, button_pos.y + button_r * 0.6},
                };
                const auto rect_b = gawl::Rectangle{
                    {button_pos.x + button_r * 0.2, button_pos.y - button_r * 0.6},
                    {button_pos.x + button_r * 0.6, button_pos.y + button_r * 0.6},
                };
                gawl::draw_rect(window, rect_a, {1, 0, 0, 0.5});
                gawl::draw_rect(window, rect_b, {1, 0, 0, 0.5});

            } else {
                const auto rect = gawl::Rectangle{
                    {button_pos.x - button_r * 0.5, button_pos.y - button_r * 0.5},
                    {button_pos.x + button_r * 0.5, button_pos.y + button_r * 0.5},
                };
                gawl::draw_rect(window, rect, {1, 0, 0, 0.5});
            }
        }
    }

    gawl::unmask_alpha();
}

auto Window::pointer_move_callback(const gawl::Point& point) -> void {
    cursor_pos = point;

    if(cursor_click_pos.x >= 0) {
        auto move = button_moved;
        if(!move) {
            constexpr auto move_threshold   = 20;
            const auto [button_x, button_y] = calc_button_pos();
            const auto dx                   = button_x - cursor_pos.x;
            const auto dy                   = button_y - cursor_pos.y;
            move                            = std::abs(dx) >= move_threshold || std::abs(dy) >= move_threshold;
        }

        if(move) {
            const auto [screen_width, screen_height] = window.get_window_size();

            const auto x = std::clamp((cursor_pos.x - button_r) / (screen_width - button_r * 2), 0.0, 1.0);
            const auto y = std::clamp((cursor_pos.y - button_r) / (screen_height - button_r * 2), 0.0, 1.0);
            button_pos   = {x, y};
            button_moved = true;
        }
    }
}

auto Window::click_callback(const uint32_t button, const gawl::ButtonState state) -> void {
    if(button != BTN_LEFT) {
        return;
    }

    if(state == gawl::ButtonState::Press) {
        cursor_click_pos = cursor_pos;
        button_moved     = false;
    } else if(state == gawl::ButtonState::Release) {
        if(!button_moved) {
            const auto [button_x, button_y] = calc_button_pos();
            const auto dx                   = button_x - cursor_pos.x;
            const auto dy                   = button_y - cursor_pos.y;
            if(dx * dx + dy * dy <= button_r * button_r) {
                if(movie) {
                    context.camera_command = recording ? Command::StopRecording : Command::StartRecording;
                } else {
                    context.camera_command = Command::TakePhoto;
                }
            }
        }
        cursor_click_pos.x = -1;
    }
}

Window::~Window() {
    if(recording) {
        context.camera_command = Command::StopRecording;
    }
}
