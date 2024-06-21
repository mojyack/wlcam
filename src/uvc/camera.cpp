#include <chrono>
#include <filesystem>
#include <fstream>

#include <poll.h>
#include <unistd.h>

#include "../encoder/encoder.hpp"
#include "../jpeg.hpp"
#include "../macros/unwrap.hpp"
#include "../remote-server.hpp"
#include "../util/assert.hpp"
#include "../yuv.hpp"
#include "camera.hpp"

namespace {
class Timer {
  private:
    std::chrono::steady_clock::time_point time;

  public:
    auto reset() -> void {
        time = std::chrono::high_resolution_clock::now();
    }

    template <class T>
    auto elapsed() -> auto {
        const auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<T>(now - time).count();
    }

    Timer() {
        reset();
    }
};

struct RecordContext {
    ff::Encoder encoder;
    Timer       timer;

    RecordContext(std::string path, const AVPixelFormat pix_fmt, const int width, const int height)
        : encoder(ff::EncoderParams{
              .output = std::move(path),
              .video  = ff::VideoParams{
                   .codec = {
                       .name = "libx264",
                  },
                   .pix_fmt = pix_fmt,
                   .width   = width,
                   .height  = height,
              },
          }) {}
};
} // namespace

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

    if(front_frame_count.exchange(frame_count) <= frame_count) {
        context->frame = frame;
    }

    // proc command
    switch(std::exchange(context->camera_command, Command::None)) {
    case Command::TakePhoto: {
        const auto path = file_manager->get_next_path().string() + ".jpg";
        assert_b(frame->save_to_jpeg(byte_array, path));
        context->ui_command = Command::TakePhotoDone;
    } break;
    case Command::StartRecording: {
        /*
if(fmt.pixelformat != v4l2::fourcc("MJPG")) {
print("currently movie is only supported in mpeg format");
break;
}
auto path = file_manager->get_next_path().string();
std::filesystem::create_directories(path);
if(event_fifo) {
event_fifo->send_event(RemoteEvents::RecordStart{path});
}
record_context.emplace(std::move(path), AV_PIX_FMT_YUV422P, width, height);
*/
        context->ui_command = Command::StartRecordingDone;
    } break;
    case Command::StopRecording: {
        /*
if(fmt.pixelformat != v4l2::fourcc("MJPG")) {
break;
}
if(event_fifo) {
const auto path = std::move(record_context->tempdir);
record_context.reset();
event_fifo->send_event(RemoteEvents::RecordStop{path});
} else {
record_context.reset();
}
*/
        context->ui_command = Command::StopRecordingDone;
    } break;
    default:
        break;
    }

    goto loop;
}

auto Camera::worker_main() -> bool {
    auto fds      = std::array<pollfd, 1>();
    fds[0].fd     = fd;
    fds[0].events = POLLIN;

    auto record_context = std::optional<RecordContext>();

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

    /*
    // if recording, save elapsed time
    if(record_context) {
        record_context->save_duration();
    }

    // record frame
    if(record_context) {
        const auto frame = record_context->frame += 1;
        const auto path  = build_string(record_context->tempdir, "/", frame, ".jpg");
        save_jpeg_frame(path.data(), static_cast<const std::byte*>(buffers[index].start));
        record_context->save_filename();
    }
    */

    loaders[index].event.wakeup();

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
        loaders[i].thread = std::thread(&Camera::loader_main, this, i);
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
