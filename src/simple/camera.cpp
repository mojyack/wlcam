#include <chrono>
#include <filesystem>
#include <fstream>

#include <sys/poll.h>
#include <unistd.h>

#include "../assert.hpp"
#include "../jpeg.hpp"
#include "../remote-server.hpp"
#include "../yuv.hpp"
#include "camera.hpp"

namespace {
auto save_jpeg_frame(const char* const path, const std::byte* const ptr) -> void {
    const auto fd   = open(path, O_RDWR | O_CREAT, 0644);
    const auto size = jpg::calc_jpeg_size(ptr);
    DYN_ASSERT(write(fd, ptr, size) == ssize_t(size));
    close(fd);
}

class Timer {
  private:
    std::chrono::steady_clock::time_point time;

  public:
    auto reset() -> void {
        time = std::chrono::high_resolution_clock::now();
    }

    template <class T>
    auto elapsed() -> auto{
        const auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<T>(now - time).count();
    }

    Timer() {
        reset();
    }
};

struct RecordContext {
    size_t        frame = 0;
    std::string   tempdir;
    std::ofstream catfile;
    Timer         timer;

    auto save_filename() -> void {
        catfile << "file " << frame << ".jpg" << std::endl;
    }

    auto save_duration() -> void {
        catfile << "duration " << timer.elapsed<std::chrono::microseconds>() << "us" << std::endl;
        timer.reset();
    }

    RecordContext(std::string tempdir_)
        : tempdir(std::move(tempdir_)),
          catfile(tempdir + "/concat.txt") {}
};
} // namespace

auto Camera::run() -> void {
    auto fds      = std::array<pollfd, 1>();
    fds[0].fd     = fd;
    fds[0].events = POLLIN;

    auto record_context = std::optional<RecordContext>();

    if(event_fifo) {
        event_fifo.send_event(RemoteEvents::Hello{});
        event_fifo.send_event(RemoteEvents::Configure{.fps = int(fps)});
    }

    const auto fmt            = v4l2::get_current_format(fd);
    auto       window_context = window.fork_context();
    auto       ubuf           = (std::byte*)(nullptr);
    auto       vbuf           = (std::byte*)(nullptr);
    if(fmt.pixelformat == v4l2::fourcc("NV12")) {
        ubuf = new std::byte[height / 4 * width];
        vbuf = new std::byte[height / 4 * width];
    }

    while(!finish) {
        DYN_ASSERT(poll(fds.data(), 1, 0) != -1);
        const auto index = v4l2::dequeue_buffer(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE);

        // if recording, save elapsed time
        if(record_context) {
            record_context->save_duration();
        }

        // proc command
        switch(std::exchange(context.camera_command, Command::None)) {
        case Command::TakePhoto: {
            const auto path = file_manager.get_next_path().string() + ".jpg";
            const auto buf  = std::bit_cast<const std::byte*>(buffers[index].start);
            switch(fmt.pixelformat) {
            case v4l2::fourcc("MJPG"): {
                save_jpeg_frame(path.data(), buf);
                context.ui_command = Command::TakePhotoDone;
            } break;
            case v4l2::fourcc("YUYV"): {
                const auto [ybuf, ubuf, vbuf] = yuv::yuv422i_to_yuv422p(buf, width, height, fmt.bytesperline);

                const auto [jpegbuf, jpegsize] = jpg::encode_yuvp_to_jpeg(width, height, fmt.bytesperline / 2, 2, 1, ybuf.data(), ubuf.data(), vbuf.data());
                const auto fd                  = open(path.c_str(), O_RDWR | O_CREAT, 0644);
                DYN_ASSERT(write(fd, jpegbuf.get(), jpegsize) == ssize_t(jpegsize));
                close(fd);

                context.ui_command = Command::TakePhotoDone;
            } break;
            case v4l2::fourcc("NV12"): {
                const auto uvbuf = buf + fmt.bytesperline * height;
                yuv::yuv420sp_uvsp_to_uvp(uvbuf, ubuf, vbuf, width, height, fmt.bytesperline);

                const auto [jpegbuf, jpegsize] = jpg::encode_yuvp_to_jpeg(width, height, fmt.bytesperline, 2, 2, buf, ubuf, vbuf);
                const auto fd                  = open(path.c_str(), O_RDWR | O_CREAT, 0644);
                DYN_ASSERT(write(fd, jpegbuf.get(), jpegsize) == ssize_t(jpegsize));
                close(fd);

                context.ui_command = Command::TakePhotoDone;
            } break;
            }
        } break;
        case Command::StartRecording: {
            if(fmt.pixelformat != v4l2::fourcc("MJPG")) {
                print("currently movie is only supported in mpeg format");
                break;
            }
            auto path = file_manager.get_next_path().string();
            std::filesystem::create_directories(path);
            if(event_fifo) {
                event_fifo.send_event(RemoteEvents::RecordStart{path});
            }
            record_context.emplace(std::move(path));
            context.ui_command = Command::StartRecordingDone;
        } break;
        case Command::StopRecording: {
            if(fmt.pixelformat != v4l2::fourcc("MJPG")) {
                break;
            }
            if(event_fifo) {
                const auto path = std::move(record_context->tempdir);
                record_context.reset();
                event_fifo.send_event(RemoteEvents::RecordStop{path});
            } else {
                record_context.reset();
            }
            context.ui_command = Command::StopRecordingDone;
        } break;
        default:
            break;
        }

        // record frame
        if(record_context) {
            const auto frame = record_context->frame += 1;
            const auto path  = build_string(record_context->tempdir, "/", frame, ".jpg");
            save_jpeg_frame(path.data(), static_cast<const std::byte*>(buffers[index].start));
            record_context->save_filename();
        }

        // unpack image
        auto img = std::shared_ptr<GraphicLike>();
        switch(fmt.pixelformat) {
        case v4l2::fourcc("MJPG"): {
            constexpr auto downscale_factor = 4;
            const auto     bufs             = jpg::decode_jpeg_to_yuvp(static_cast<std::byte*>(buffers[index].start), buffers[index].length, downscale_factor);
            img.reset(new GraphicLike(Tag<PlanarGraphic>(), width / downscale_factor, height / downscale_factor, fmt.bytesperline, bufs.ppc_x, bufs.ppc_y, bufs.y.data(), bufs.u.data(), bufs.v.data()));
        } break;
        case v4l2::fourcc("YUYV"): {
            img.reset(new GraphicLike(Tag<YUV422iGraphic>(), width, height, fmt.bytesperline, static_cast<std::byte*>(buffers[index].start)));
        } break;
        case v4l2::fourcc("NV12"): {
            const auto buf    = std::bit_cast<std::byte*>(buffers[index].start);
            const auto stride = fmt.bytesperline;
            auto       img    = std::shared_ptr<GraphicLike>(new GraphicLike(Tag<YUV420spGraphic>(), width, height, stride, buf, buf + stride * height));
        } break;
        }

        v4l2::queue_buffer(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, index);

        window_context.flush();
        std::swap(context.critical_graphic, img);
    }
    if(event_fifo) {
        event_fifo.send_event(RemoteEvents::Bye{});
    }
}

auto Camera::finish_thread() -> void {
    finish = true;
}

Camera::Camera(const int fd, v4l2::Buffer* const buffers, const uint32_t width, const uint32_t height, const uint32_t fps, gawl::Window<Window>& window, const FileManager& file_manager, Context& context, RemoteServer& event_fifo)
    : buffers(buffers),
      file_manager(file_manager),
      window(window),
      context(context),
      event_fifo(event_fifo),
      fd(fd),
      width(width),
      height(height),
      fps(fps),
      finish(false) {}

