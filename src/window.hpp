#pragma once
#include "gawl/textrender.hpp"
#include "gawl/window-no-touch-callbacks.hpp"

#include "context.hpp"

class WindowCallbacks : public gawl::WindowNoTouchCallbacks {
  private:
    Context*         context;
    gawl::TextRender font;
    int              shutter_anim = 0;
    bool             movie        = false;
    bool             recording    = false;

  public:
    auto refresh() -> void override;
    auto on_click(uint32_t button, gawl::ButtonState state) -> void override;

    auto init() -> bool;

    WindowCallbacks(Context& context);
    ~WindowCallbacks();
};
