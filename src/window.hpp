#pragma once
#include "gawl/textrender.hpp"
#include "gawl/window-no-touch-callbacks.hpp"
#include "graphics-wrapper.hpp"
#include "timer.hpp"
#include "ui.hpp"

enum class Command {
    None,
    TakePhoto,
    TakePhotoDone,
    StartRecording,
    StartRecordingDone,
    StopRecording,
    StopRecordingDone,
};

struct WindowContext {
    std::shared_ptr<Frame> frame;
    Command                ui_command;     // camera -> ui
    Command                camera_command; // ui -> camera
    int                    capture_rate = 0;
};

class WindowCallbacks : public gawl::WindowNoTouchCallbacks {
  public:
    WindowContext                        context;
    gawl::TextRender                     font;
    gawl::Point                          cursor;
    std::vector<std::unique_ptr<Button>> buttons;
    Timer                                record_timer;
    FPSCounter                           render_counter;
    int                                  render_rate  = 0;
    int                                  shutter_anim = 0;
    bool                                 movie        = false;
    bool                                 recording    = false;

    auto draw_button(const gawl::Point& base, std::string_view label, bool pressed, bool active, gawl::WrappedText& wrapped_text) -> void;

    auto refresh() -> void override;
    auto on_created(gawl::Window* window) -> coop::Async<bool> override;
    auto on_pointer(gawl::Point pos) -> coop::Async<bool> override;
    auto on_click(uint32_t button, gawl::ButtonState state) -> coop::Async<bool> override;

    auto get_window() const -> gawl::Window*;
    auto get_context() -> WindowContext&;

    WindowCallbacks();
    ~WindowCallbacks();
};
