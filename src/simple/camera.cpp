#include <chrono>
#include <filesystem>
#include <fstream>

#include <sys/poll.h>
#include <unistd.h>

#include "../assert.hpp"
#include "../remote-server.hpp"
#include "camera.hpp"

// jpeg.cpp
auto calc_jpeg_size(const std::byte* const ptr) -> size_t;
auto decode_jpeg_yuv422_planar(const std::byte* ptr, size_t len) -> std::unique_ptr<Image>;

// yuv.cpp
// auto split_yuv422_interleaved_planar(const std::byte* const ptr, const uint32_t width, const uint32_t height) -> std::array<gawl::PixelBuffer, 3>;

namespace {
auto save_jpeg_frame(const char* const path, const std::byte* const ptr) -> void {
    const auto fd   = open(path, O_RDWR | O_CREAT, 0644);
    const auto size = calc_jpeg_size(ptr);
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

class YUYVImage : public Image {
  private:
    std::byte* data;
    size_t     buffer_index;
    int        width;
    int        height;
    int        dev_fd;

  public:
    auto get_plane(const int num) -> PixelBuffer override {
        DYN_ASSERT(num == 0, "no such plane");
        return {width, height, data};
    }

    YUYVImage(const int width, const int height, std::byte* const data, const int dev_fd, const size_t buffer_index)
        : data(data),
          buffer_index(buffer_index),
          width(width),
          height(height),
          dev_fd(dev_fd) {}

    ~YUYVImage() {
        v4l2::queue_buffer(dev_fd, buffer_index);
    }
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

    while(!finish) {
        DYN_ASSERT(poll(fds.data(), 1, 0) != -1);
        const auto index = v4l2::dequeue_buffer(fd);

        // if recording, save elapsed time
        if(record_context) {
            record_context->save_duration();
        }

        // proc command
        switch(std::exchange(context.camera_command, Command::None)) {
        case Command::TakePhoto: {
            switch(context.pixel_format) {
            case PixelFormat::MPEG: {
                const auto path = file_manager.get_next_path().string() + ".jpg";
                save_jpeg_frame(path.data(), static_cast<const std::byte*>(buffers[index].start));
                context.ui_command = Command::TakePhotoDone;
            } break;
            case PixelFormat::YUYV:
                // TODO implement yuyv save
                break;
            }
        } break;
        case Command::StartRecording: {
            auto path = file_manager.get_next_path().string();
            std::filesystem::create_directories(path);
            if(event_fifo) {
                event_fifo.send_event(RemoteEvents::RecordStart{path});
            }
            record_context.emplace(std::move(path));
            context.ui_command = Command::StartRecordingDone;
        } break;
        case Command::StopRecording: {
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
        auto new_image = std::unique_ptr<Image>();
        switch(context.pixel_format) {
        case PixelFormat::MPEG:
            new_image = decode_jpeg_yuv422_planar(static_cast<std::byte*>(buffers[index].start), buffers[index].length);
            v4l2::queue_buffer(fd, index);
            break;
        case PixelFormat::YUYV: {
            new_image = std::make_unique<YUYVImage>(width, height, (std::byte*)buffers[index].start, fd, index);
        } break;
        }

        auto [lock, image] = context.critical_image.access();
        image              = std::move(new_image);
    }
    if(event_fifo) {
        event_fifo.send_event(RemoteEvents::Bye{});
    }
}

auto Camera::finish_thread() -> void {
    finish = true;
}

Camera::Camera(const int fd, v4l2::Buffer* const buffers, const uint32_t width, const uint32_t height, const uint32_t fps, const FileManager& file_manager, Context& context, RemoteServer& event_fifo)
    : buffers(buffers),
      file_manager(file_manager),
      context(context),
      event_fifo(event_fifo),
      fd(fd),
      width(width),
      height(height),
      fps(fps),
      finish(false) {}

