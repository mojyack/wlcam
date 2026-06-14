#include <coop/io.hpp>
#include <coop/parallel.hpp>
#include <coop/task-handle.hpp>
#include <coop/thread.hpp>
#include <sys/ioctl.h>

#include "../file.hpp"
#include "../graphics/bayer.hpp"
#include "../macros/coop-assert.hpp"
#include "../macros/unwrap.hpp"
#include "camera.hpp"

namespace {
auto find_venus_encoder_node() -> std::optional<std::string> {
    for(auto i = 0; i < 32; i += 1) {
        auto       path = std::format("/dev/video{}", i);
        const auto fd   = open(path.data(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if(fd < 0) {
            continue;
        }
        auto cap = v4l2_capability();
        auto ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
        close(fd);
        if(ret != 0) {
            continue;
        }
        if(std::string_view((const char*)cap.driver) != "qcom-venus") {
            continue;
        }
        if(!std::string_view((const char*)cap.card).contains("encoder")) {
            continue;
        }
        return path;
    }
    return std::nullopt;
}

} // namespace

namespace camss {
auto Camera::loader_main(const size_t index) -> coop::Async<void> {
    auto& event = loaders[index].event;
loop:
    co_await event;

    const auto frame_count = (current_frame_count += 1);

    auto       bayer_frame = new BayerFrame(params.width, params.height, params.stride);
    auto       frame       = std::shared_ptr<Frame>(bayer_frame);
    const auto byte_array  = Frame::ByteArray{static_cast<const std::byte*>(params.mmap_ptrs[index]), params.dmabufs[index].length};
    coop_ensure(frame->load_texture(byte_array));
    coop_ensure(v4l2::queue_buffer_mp(params.fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_DMABUF, index, &params.dmabufs[index], 1));

    if(front_frame_count < frame_count) {
        front_frame_count = frame_count;
    } else {
        // a later frame was already published
        goto loop;
    }
    params.window_context->frame = frame;

    // proc command
    switch(std::exchange(params.window_context->camera_command, Command::None)) {
    case Command::TakePhoto: {
        const auto path = std::format("{}/{}.jpg", params.args->savedir, get_save_filename());
        coop_ensure(frame->save_to_jpeg(byte_array, path.data()));
        params.window_context->ui_command = Command::TakePhotoDone;
    } break;
    case Command::StartRecording: {
        const auto path = std::format("{}/{}.mkv", params.args->savedir, get_save_filename());

        auto rec = std::array{params.width, params.height};
        if(bayer_params.rotate % 2 != 0) {
            std::swap(rec[0], rec[1]);
        }

        const auto fps     = params.window_context->capture_rate > 0 ? params.window_context->capture_rate : 30;
        const auto bitrate = 200000 * fps;

        auto enc = std::make_unique<ff::V4L2H264Encoder>();
        coop_ensure(enc->init(venus_node.data(), rec[0], rec[1], bitrate, fps));
        auto ctx = std::make_unique<RecordContext>();
        coop_ensure(ctx->init(path, rec[0], rec[1], enc->coded_width(), enc->coded_height(), *params.args));

        this->enc = std::move(enc);
        this->rec = std::move(ctx);

        params.window_context->ui_command = Command::StartRecordingDone;
    } break;
    case Command::StopRecording: {
        if(rec) {
            enc->drain([&](const ff::V4L2H264Encoder::Packet& p) {
                rec->encoder.add_video_packet(p.data, p.size, p.pts_us, p.keyframe);
            });
            enc.reset();
            rec.reset();
        }

        params.window_context->ui_command = Command::StopRecordingDone;
    } break;
    default:
        break;
    }

    if(rec) {
        const auto ts = rec->timer.elapsed<std::chrono::microseconds>();
        if(const auto tex = bayer_frame->get_rgba_texture()) {
            coop_ensure(enc->encode(*tex, ts, [&](const ff::V4L2H264Encoder::Packet& p) {
                rec->encoder.add_video_packet(p.data, p.size, p.pts_us, p.keyframe);
            }));
        }
    }

    goto loop;
}

auto Camera::dispatcher_main() -> coop::Async<bool> {
    constexpr static auto error_value = false;

    // start loaders
    auto& runner = *co_await coop::reveal_runner();
    for(auto i = 0u; i < loaders.size(); i += 1) {
        runner.push_task(loader_main(i), &loaders[i].task);
    }

    auto counter = FPSCounter();
loop:
    const auto res = co_await coop::wait_for_file(params.fd, true, false);
    co_ensure_v(res.read && !res.error);
    co_unwrap_v(index, v4l2::dequeue_buffer_mp(params.fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_DMABUF));
    loaders[index].event.notify();

    if(const auto c = counter.tick(); c >= 0) {
        params.window_context->capture_rate = c;
    }

    goto loop;
}

auto Camera::init(CameraParams params) -> bool {
    this->params = std::move(params);

    unwrap_mut(node, find_venus_encoder_node());
    this->venus_node = std::move(node);

    return true;
}

auto Camera::start() -> coop::Async<void> {
    auto& runner = *co_await coop::reveal_runner();
    runner.push_task(dispatcher_main(), &dispatcher);
}

auto Camera::shutdown() -> void {
    for(auto& loader : loaders) {
        loader.task.cancel();
    }
    dispatcher.cancel();
}

Camera::~Camera() {
    shutdown();
}
} // namespace camss
