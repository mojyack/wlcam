#pragma once
#include "gawl/textrender.hpp"
#include "gawl/window-no-touch-callbacks.hpp"
#include "graphics-wrapper.hpp"
#include "timer.hpp"

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
    std::string            error_message;
    Command                ui_command;     // camera -> ui
    Command                camera_command; // ui -> camera
};

class WindowCallbacks : public gawl::WindowNoTouchCallbacks {
  private:
    static constexpr auto error_value = false;

    WindowContext    context;
    gawl::TextRender font;
    gawl::Point      cursor;
    Timer            record_timer;
    int              shutter_anim = 0;
    bool             movie        = false;
    bool             recording    = false;

  public:
    auto refresh() -> void override;
    auto on_created(gawl::Window* window) -> coop::Async<bool> override;
    auto on_pointer(gawl::Point pos) -> coop::Async<bool> override;
    auto on_click(uint32_t button, gawl::ButtonState state) -> coop::Async<bool> override;

    auto get_window() const -> gawl::Window*;
    auto get_context() -> WindowContext&;

    ~WindowCallbacks();
};
