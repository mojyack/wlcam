#pragma once
#include "../context.hpp"
#include "../file.hpp"
#include "../gawl/wayland/window.hpp"
#include "../remote-server.hpp"
#include "../util/event.hpp"
#include "../v4l2.hpp"

constexpr auto num_buffers = 4;

class Camera {
  private:
    struct Loader {
        Event       event;
        std::thread thread;
    };

    v4l2::Buffer*                   buffers;
    gawl::WaylandWindow*            window;
    const FileManager*              file_manager;
    Context*                        context;
    RemoteServer*                   event_fifo;
    std::thread                     worker;
    std::array<Loader, num_buffers> loaders;
    std::atomic_size_t              current_frame_count = 0;
    std::atomic_size_t              front_frame_count   = 0;
    int                             fd;
    uint32_t                        width;
    uint32_t                        height;
    uint32_t                        fps;
    bool                            running = false;

    auto loader_main(size_t index) -> bool;
    auto worker_main() -> bool;

  public:
    auto run() -> void;
    auto shutdown() -> void;

    Camera(int fd, v4l2::Buffer* buffers, uint32_t width, uint32_t height, uint32_t fps, gawl::WaylandWindow& window, const FileManager& file_manager, Context& context, RemoteServer* event_fifo);
    ~Camera();
};
