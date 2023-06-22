#include "args.hpp"
#include "assert.hpp"
#include "camera.hpp"
#include "file.hpp"
#include "v4l2.hpp"
#include "window.hpp"

auto main(const int argc, const char* const argv[]) -> int {
    const auto args = parse_args(argc, argv);

    const int fd = open(args.video_device, O_RDWR);
    DYN_ASSERT(fd != -1);
    DYN_ASSERT(v4l2::is_capture_device(fd), "not a capture device");

    if(args.list_formats) {
        v4l2::list_formats(fd);
        return 0;
    }

    if(args.savedir == nullptr) {
        print("no output directory specified");
        exit(1);
    }

    const auto pixfmt = pixel_format_to_v4l2(args.pixel_format);
    v4l2::set_format(fd, pixfmt, args.width, args.height);
    DYN_ASSERT(v4l2::ensure_format(fd, pixfmt, args.width, args.height));
    v4l2::set_interval(fd, 1, args.fps);

    const auto req     = v4l2::request_buffers(fd, 2);
    auto       buffers = v4l2::map_buffers(fd, req);
    v4l2::queue_buffer(fd, 0);
    v4l2::queue_buffer(fd, 1);
    v4l2::start_stream(fd);

    const auto file_manager = FileManager(args.savedir);
    auto       context      = Context{.pixel_format = args.pixel_format};
    auto       event_fifo  = args.event_fifo != nullptr ? RemoteServer(args.event_fifo) : RemoteServer();

    auto app = gawl::Application();
    app.open_window<Window>({}, context, gawl::Point{args.button_x, args.button_y}, args.movie);

    switch(args.pixel_format) {
    case PixelFormat::MPEG:
        init_yuyv_planar_graphic_globject();
        break;
    case PixelFormat::YUYV:
        init_yuyv_interleaved_graphic_globject();
        break;
    }

    auto camera = Camera(fd, buffers.data(), args.width, args.height, args.fps, file_manager, context, event_fifo);

    auto camera_worker = std::thread([&camera] { camera.run(); });
    app.run();

    camera.finish_thread();
    camera_worker.join();

    return 0;
}
