#pragma once
#include "../context.hpp"
#include "../file.hpp"
#include "../gawl/wayland/window.hpp"
#include "../remote-server.hpp"
#include "../v4l2.hpp"

class Camera {
  private:
    v4l2::Buffer*        buffers;
    gawl::WaylandWindow* window;
    const FileManager*   file_manager;
    Context*             context;
    RemoteServer*        event_fifo;
    std::thread          worker;
    int                  fd;
    uint32_t             width;
    uint32_t             height;
    uint32_t             fps;
    bool                 running = false;

    auto worker_main() -> bool;

  public:
    auto run() -> void;
    auto shutdown() -> void;

    Camera(int fd, v4l2::Buffer* buffers, uint32_t width, uint32_t height, uint32_t fps, gawl::WaylandWindow& window, const FileManager& file_manager, Context& context, RemoteServer* event_fifo);
    ~Camera();
};

constexpr auto num_buffers = 4;
