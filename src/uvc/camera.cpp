#include <coop/io.hpp>
#include <coop/parallel.hpp>
#include <coop/thread.hpp>
#include <poll.h>
#include <unistd.h>

#include "../macros/coop-unwrap.hpp"
#include "camera.hpp"

auto Camera::loader_main(const size_t index) -> coop::Async<void> {
    coop_unwrap(fmt, v4l2::get_current_format(params.fd));
    auto& event = loaders[index].event;
loop:
    co_await event;

    const auto frame_count = (current_frame_count += 1);

    // unpack image
    auto frame = std::shared_ptr<Frame>();
    switch(fmt.pixelformat) {
    case v4l2::fourcc("MJPG"):
        frame.reset(new JpegFrame());
        break;
    case v4l2::fourcc("YUYV"):
        frame.reset(new YUV422IFrame(params.width, params.height, fmt.bytesperline));
        break;
    case v4l2::fourcc("NV12"):
        frame.reset(new YUV420SPFrame(params.width, params.height, fmt.bytesperline));
        break;
    default:
        coop_bail("pixelformat bug");
    }
    const auto byte_array = Frame::ByteArray{static_cast<std::byte*>(params.buffers[index].start), params.buffers[index].length};
    coop_ensure(co_await coop::run_blocking([&, window = params.window]() {
        auto       window_context = window->fork_context();
        const auto ret            = frame->load_texture(byte_array);
        window_context.flush();
        return ret;
    }));
    coop_ensure(v4l2::queue_buffer(params.fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, index));

    if(front_frame_count < frame_count) {
        front_frame_count = frame_count;
    } else {
        // other loader already decoded newer frame
        goto loop;
    }
    params.window_context->frame = frame;

    // proc command
    switch(std::exchange(params.window_context->camera_command, Command::None)) {
    case Command::TakePhoto: {
        const auto path = params.file_manager->get_next_path().string() + ".jpg";

        coop_ensure(co_await coop::run_blocking([&]() { return frame->save_to_jpeg(byte_array, path.data()); }));

        params.window_context->ui_command = Command::TakePhotoDone;
    } break;
    case Command::StartRecording: {
        const auto path = params.file_manager->get_next_path().string() + ".mkv";

        co_unwrap_v(pix_fmt, frame->get_pixel_format());
        record_context.reset(new RecordContext());
        co_ensure_v(record_context->init(path, pix_fmt, *params.args));

        params.window_context->ui_command = Command::StartRecordingDone;
    } break;
    case Command::StopRecording: {
        record_context.reset();

        params.window_context->ui_command = Command::StopRecordingDone;
    } break;
    default:
        break;
    }

    if(const auto rc = record_context) {
        co_unwrap_v(planes, frame->get_planes(byte_array));
        co_await coop::run_blocking([&]() { rc->encoder.add_frame(planes, rc->timer.elapsed<std::chrono::microseconds>()); });
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

    // loop
    auto timer = FPSTimer();
loop:
    const auto res = co_await coop::wait_for_file(params.fd, true, false);
    co_ensure_v(res.read && !res.error);
    co_unwrap_v(index, v4l2::dequeue_buffer(params.fd, V4L2_BUF_TYPE_VIDEO_CAPTURE));
    loaders[index].event.notify();

    timer.tick();

    goto loop;
}

auto Camera::run(CameraParams params) -> coop::Async<void> {
    this->params = std::move(params);
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
