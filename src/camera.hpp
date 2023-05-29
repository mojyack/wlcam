#pragma once
#include "context.hpp"
#include "file.hpp"
#include "remote-server.hpp"
#include "v4l2.hpp"

class Camera {
  private:
    v4l2::Buffer*      buffers;
    const FileManager& file_manager;
    Context&           context;
    RemoteServer&      event_fifo;
    int                fd;
    uint32_t           width;
    uint32_t           height;
    uint32_t           fps;
    bool               finish;

  public:
    auto run() -> void;

    auto finish_thread() -> void;

    Camera(int fd, v4l2::Buffer* buffers, uint32_t width, uint32_t height, uint32_t fps, const FileManager& file_manager, Context& context, RemoteServer& event_fifo);
};
