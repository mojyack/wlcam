#pragma once
#include "gawl/textrender.hpp"
#include "gawl/window-no-touch-callbacks.hpp"
#include "graphics-wrapper.hpp"

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
};

class WindowCallbacks : public gawl::WindowNoTouchCallbacks {
  private:
    WindowContext    context;
    gawl::TextRender font;
    gawl::Point      cursor;
    int              shutter_anim = 0;
    bool             movie        = false;
    bool             recording    = false;

  public:
    auto refresh() -> void override;
    auto on_pointer(const gawl::Point& pos) -> void override;
    auto on_click(uint32_t button, gawl::ButtonState state) -> void override;

    auto init() -> bool;
    auto get_context() -> WindowContext&;

    ~WindowCallbacks();
};
