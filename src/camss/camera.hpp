#pragma once
#include <coop/generator.hpp>
#include <coop/promise.hpp>
#include <coop/single-event.hpp>

#include "../args.hpp"
#include "../record-context.hpp"
#include "../v4l2-encoder/encoder.hpp"
#include "../v4l2.hpp"
#include "../window.hpp"

namespace camss {
constexpr auto num_buffers = 4;

struct CameraParams {
    int                    fd;
    uint32_t               width;
    uint32_t               height;
    uint32_t               stride;
    const v4l2::DMABuffer* dmabufs;   // num_buffers entries, used to requeue
    void* const*           mmap_ptrs; // CPU-readable mapping of each dmabuf
    WindowContext*         window_context;
    const CommonArgs*      args;
};

class Camera {
  private:
    struct Loader {
        coop::SingleEvent event;
        coop::TaskHandle  task;
    };

    CameraParams                    params;
    std::array<Loader, num_buffers> loaders;
    coop::TaskHandle                dispatcher;
    size_t                          current_frame_count = 0;
    size_t                          front_frame_count   = 0;
    std::string                     venus_node;

    std::unique_ptr<ff::V4L2H264Encoder> enc;
    std::unique_ptr<RecordContext>       rec;

    auto loader_main(size_t index) -> coop::Async<void>;
    auto dispatcher_main() -> coop::Async<bool>;

  public:
    auto init(CameraParams params) -> bool;
    auto start() -> coop::Async<void>;
    auto shutdown() -> void;

    ~Camera();
};
} // namespace camss
