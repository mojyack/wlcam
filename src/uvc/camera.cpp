#include <poll.h>
#include <unistd.h>

#include "../macros/unwrap.hpp"
#include "../remote-server.hpp"
#include "../util/assert.hpp"
#include "camera.hpp"

Camera::RecordContext::RecordContext(std::string path, const AVPixelFormat pix_fmt, const int width, const int height)
    : encoder(ff::EncoderParams{
          .output = std::move(path),
          .video  = ff::VideoParams{
               .codec = {
                   .name = "h264_vaapi",
                  // .name = "libx264",
              },
               .pix_fmt = pix_fmt,
               .width   = width,
               .height  = height,
              // .threads = 1,
          },
          .ffmpeg_debug = true,
      }) {}

auto Camera::loader_main(const size_t index) -> bool {
    unwrap_ob(fmt, v4l2::get_current_format(fd));
    auto  window_context = window->fork_context();
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
        frame.reset(new YUV422IFrame(width, height, fmt.bytesperline));
        break;
    case v4l2::fourcc("NV12"):
        frame.reset(new YUV420SPFrame(width, height, fmt.bytesperline));
        break;
    default:
        WARN("pixelformat bug");
        return false;
    }
    const auto byte_array = Frame::ByteArray{static_cast<std::byte*>(buffers[index].start), buffers[index].length};
    frame->load_texture(byte_array);
    assert_b(v4l2::queue_buffer(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, index));
    window_context.flush();

    if(front_frame_count.exchange(frame_count) > frame_count) {
        // other loader already decoded newer frame
        goto loop;
    }
    context->frame = frame;

    // proc command
    switch(std::exchange(context->camera_command, Command::None)) {
    case Command::TakePhoto: {
        const auto path = file_manager->get_next_path().string() + ".jpg";
        assert_b(frame->save_to_jpeg(byte_array, path));
        context->ui_command = Command::TakePhotoDone;
    } break;
    case Command::StartRecording: {
        auto path = file_manager->get_next_path().string() + ".mkv";
        unwrap_ob(pix_fmt, frame->get_pixel_format());
        auto rc = std::unique_ptr<RecordContext>(new RecordContext(path, pix_fmt, width, height));
        assert_b(rc->encoder.init());
        record_context.reset(rc.release());
        context->ui_command = Command::StartRecordingDone;
    } break;
    case Command::StopRecording: {
        record_context.reset();
        context->ui_command = Command::StopRecordingDone;
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
    fds[0].fd     = fd;
    fds[0].events = POLLIN;

    if(event_fifo) {
        event_fifo->send_event(RemoteEvents::Hello{});
        event_fifo->send_event(RemoteEvents::Configure{.fps = int(fps)});
    }

    auto timer = Timer();
    auto loops = 0;

loop:
    if(!running) {
        if(event_fifo) {
            event_fifo->send_event(RemoteEvents::Bye{});
        }
        return true;
    }
    assert_b(poll(fds.data(), 1, 0) != -1);
    unwrap_ob(index, v4l2::dequeue_buffer(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE));

    loaders[index].event.wakeup();

    // show fps
    loops += 1;
    if(timer.elapsed<std::chrono::milliseconds>() >= 1000) {
        print(loops, " fps");
        timer.reset();
        loops = 0;
    }

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

Camera::Camera(const int fd, v4l2::Buffer* const buffers, const uint32_t width, const uint32_t height, const uint32_t fps, gawl::WaylandWindow& window, const FileManager& file_manager, Context& context, RemoteServer* event_fifo)
    : buffers(buffers),
      window(&window),
      file_manager(&file_manager),
      context(&context),
      event_fifo(event_fifo),
      fd(fd),
      width(width),
      height(height),
      fps(fps) {}

Camera::~Camera() {
    shutdown();
}
