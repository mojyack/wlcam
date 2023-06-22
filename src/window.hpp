#define GAWL_MOUSE
// #define GAWL_KEYCODE
#include <gawl/graphic.hpp>
#include <gawl/wayland/gawl.hpp>

#include "context.hpp"

class Window {
  private:
    gawl::Window<Window>& window;
    Context&              context;
    gawl::Point           button_pos;
    gawl::Point           cursor_pos;
    gawl::Point           cursor_click_pos = {-1, -1};
    int                   shutter_anim     = 0;
    bool                  movie            = false;
    bool                  recording        = false;
    bool                  button_moved     = false;

    static constexpr auto button_r          = 40.0;
    static constexpr auto button_line_width = 3.0;

    auto calc_button_pos() -> gawl::Point;

  public:
    auto get_window() -> gawl::Window<Window>& {
        return window;
    }

    auto refresh_callback() -> void;

    auto pointer_move_callback(const gawl::Point& point) -> void;

    auto click_callback(const uint32_t button, const gawl::ButtonState state) -> void;

    /*
    auto keycode_callback(const uint32_t keycode, const gawl::ButtonState state) -> void {
    }
    */

    Window(gawl::Window<Window>& window, Context& context, const gawl::Point button_pos, const bool movie)
        : window(window),
          context(context),
          button_pos(button_pos),
          movie(movie) {}

    ~Window();
};
