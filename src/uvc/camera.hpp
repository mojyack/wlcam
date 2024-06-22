#pragma once
#include "../context.hpp"
#include "../encoder/converter.hpp"
#include "../encoder/encoder.hpp"
#include "../file.hpp"
#include "../gawl/wayland/window.hpp"
#include "../pulse/pulse.hpp"
#include "../remote-server.hpp"
#include "../timer.hpp"
#include "../util/event.hpp"
#include "../v4l2.hpp"

constexpr auto num_buffers = 4;

class Camera {
  private:
    struct RecordContext {
        ff::Encoder        encoder;
        ff::AudioConverter converter;
        pa::Recorder       recorder;
        Timer              timer;
        std::thread        recorder_thread;
        bool               running;

        auto recorder_main() -> bool;
        auto init(std::string path, AVPixelFormat pix_fmt, int width, int height, int sample_rate) -> bool;

        ~RecordContext();
    };

    struct Loader {
        Event       event;
        std::thread thread;
    };

    v4l2::Buffer*                   buffers;
    gawl::WaylandWindow*            window;
    const FileManager*              file_manager;
    Context*                        context;
    std::shared_ptr<RecordContext>  record_context;
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
