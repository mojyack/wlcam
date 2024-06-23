#pragma once
#include "../file.hpp"
#include "../gawl/wayland/window.hpp"
#include "../record-context.hpp"
#include "../timer.hpp"
#include "../util/event.hpp"
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
    FileManager*         file_manager;
    const CommonArgs*    args;
};

class Camera {
  private:
    struct Loader {
        Event       event;
        std::thread thread;
    };

    CameraParams                    params;
    std::shared_ptr<RecordContext>  record_context;
    std::thread                     worker;
    std::array<Loader, num_buffers> loaders;
    std::atomic_size_t              current_frame_count = 0;
    std::atomic_size_t              front_frame_count   = 0;
    bool                            running             = false;

    auto loader_main(size_t index) -> bool;
    auto worker_main() -> bool;

  public:
    auto run() -> void;
    auto shutdown() -> void;

    Camera(CameraParams params);
    ~Camera();
};
