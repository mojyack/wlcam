#pragma once
#include <coop/generator.hpp>
#include <coop/promise.hpp>
#include <coop/single-event.hpp>
#include <coop/thread.hpp>

#include "../file.hpp"
#include "../gawl/wayland/eglobject.hpp"
#include "../gawl/wayland/window.hpp"
#include "../record-context.hpp"
#include "../v4l2.hpp"
#include "../window.hpp"

constexpr auto num_buffers = 4;

struct CameraParams {
    int                  fd;
    uint32_t             width;
    uint32_t             height;
    uint32_t             fps;
    v4l2::Buffer*        buffers;
    gawl::WaylandWindow* window;
    WindowContext*       window_context;
    const CommonArgs*    args;
};

class Camera {
  private:
    struct Loader {
        gawl::EGLSubObject context;
        coop::Thread       thread;
        coop::SingleEvent  event;
        coop::TaskHandle   task;
    };

    CameraParams                    params;
    std::shared_ptr<RecordContext>  record_context;
    std::array<Loader, num_buffers> loaders;
    coop::TaskHandle                dispatcher;
    size_t                          current_frame_count = 0;
    size_t                          front_frame_count   = 0;

    auto loader_main(size_t index) -> coop::Async<void>;
    auto dispatcher_main() -> coop::Async<bool>;

  public:
    auto run(CameraParams params) -> coop::Async<void>;
    auto shutdown() -> void;

    ~Camera();
};
