#pragma once
#define GAWL_MOUSE
// #define GAWL_KEYCODE
#include <gawl/graphic.hpp>
#include <gawl/wayland/gawl.hpp>

#include "context.hpp"

class Window {
  private:
    gawl::Window<Window>& window;
    Context&              context;
    int                   shutter_anim     = 0;
    bool                  movie            = false;
    bool                  recording        = false;

  public:
    auto get_window() -> gawl::Window<Window>& {
        return window;
    }

    auto refresh_callback() -> void;

    auto click_callback(const uint32_t button, const gawl::ButtonState state) -> void;

    /*
    auto keycode_callback(const uint32_t keycode, const gawl::ButtonState state) -> void {
    }
    */

    Window(gawl::Window<Window>& window, Context& context, const bool movie)
        : window(window),
          context(context),
          movie(movie) {}

    ~Window();
};
