#include <poll.h>
#include <unistd.h>

#include "../macros/unwrap.hpp"
#include "../util/assert.hpp"
#include "camera.hpp"

auto Camera::loader_main(const size_t index) -> bool {
    unwrap_ob(fmt, v4l2::get_current_format(params.fd));
    auto  window_context = params.window->fork_context();
    auto& event          = loaders[index].event;
loop:
    if(!running) {
        return true;
    }
    event.wait();
    event.clear();
    if(!running) {
        return true;
    }

    const auto frame_count = current_frame_count.fetch_add(1);

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
        WARN("pixelformat bug");
        return false;
    }
    const auto byte_array = Frame::ByteArray{static_cast<std::byte*>(params.buffers[index].start), params.buffers[index].length};
    assert_b(frame->load_texture(byte_array));
    assert_b(v4l2::queue_buffer(params.fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, index));
    window_context.flush();

    if(front_frame_count.exchange(frame_count) > frame_count) {
        // other loader already decoded newer frame
        goto loop;
    }
    params.window_context->frame = frame;

    // proc command
    switch(std::exchange(params.window_context->camera_command, Command::None)) {
    case Command::TakePhoto: {
        const auto path = params.file_manager->get_next_path().string() + ".jpg";

        assert_b(frame->save_to_jpeg(byte_array, path));

        params.window_context->ui_command = Command::TakePhotoDone;
    } break;
    case Command::StartRecording: {
        const auto path = params.file_manager->get_next_path().string() + ".mkv";

        unwrap_ob(pix_fmt, frame->get_pixel_format());
        auto rc = std::unique_ptr<RecordContext>(new RecordContext());
        assert_b(rc->init(path, pix_fmt, *params.args));
        record_context.reset(rc.release());

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
        unwrap_ob(planes, frame->get_planes(byte_array));
        rc->encoder.add_frame(planes, rc->timer.elapsed<std::chrono::microseconds>());
    }

    goto loop;
}

auto Camera::worker_main() -> bool {
    auto fds      = std::array<pollfd, 1>();
    fds[0].fd     = params.fd;
    fds[0].events = POLLIN;

    auto timer = FPSTimer();

loop:
    if(!running) {
        return true;
    }

    assert_b(poll(fds.data(), 1, 0) != -1);
    unwrap_ob(index, v4l2::dequeue_buffer(params.fd, V4L2_BUF_TYPE_VIDEO_CAPTURE));
    loaders[index].event.wakeup();

    timer.tick();

    goto loop;
}

auto Camera::run() -> void {
    running = true;
    for(auto i = 0u; i < loaders.size(); i += 1) {
        loaders[i].thread = std::thread([this, i]() {
            if(!Camera::loader_main(i)) {
                // TODO: find better error handling
                std::exit(1);
            }
        });
    }
    worker = std::thread(&Camera::worker_main, this);
}

auto Camera::shutdown() -> void {
    if(!running) {
        return;
    }
    running = false;
    for(auto& loader : loaders) {
        loader.event.wakeup();
        loader.thread.join();
    }
    worker.join();
}

Camera::Camera(CameraParams params)
    : params(std::move(params)) {
}

Camera::~Camera() {
    shutdown();
}
