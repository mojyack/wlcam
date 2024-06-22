#pragma once
#include "gawl/window-no-touch-callbacks.hpp"

#include "context.hpp"

class WindowCallbacks : public gawl::WindowNoTouchCallbacks {
  private:
    Context* context;
    int      shutter_anim = 0;
    bool     movie        = false;
    bool     recording    = false;

  public:
    auto refresh() -> void override;
    auto on_click(uint32_t button, gawl::ButtonState state) -> void override;

    WindowCallbacks(Context& context);
    ~WindowCallbacks();
};
