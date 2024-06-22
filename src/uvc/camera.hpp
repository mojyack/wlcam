#pragma once
#include "../context.hpp"
#include "../encoder/converter.hpp"
#include "../encoder/encoder.hpp"
#include "../file.hpp"
#include "../gawl/wayland/window.hpp"
#include "../pulse/pulse.hpp"
#include "../timer.hpp"
#include "../util/event.hpp"
#include "../v4l2.hpp"

constexpr auto num_buffers = 4;

struct CameraParams {
    int                  fd;
    uint32_t             width;
    uint32_t             height;
    uint32_t             fps;
    v4l2::Buffer*        buffers;
    gawl::WaylandWindow* window;
    FileManager*         file_manager;
    Context*             context;
    const char*          video_codec;
    const char*          audio_codec;
    const char*          video_filter;
    uint32_t             audio_sample_rate;
    bool                 ffmpeg_debug;
};

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
        auto init(std::string path, AVPixelFormat pix_fmt, const CameraParams& params) -> bool;

        ~RecordContext();
    };

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
