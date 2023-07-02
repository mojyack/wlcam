#include "../assert.hpp"
#include "../file.hpp"
#include "../v4l2.hpp"
#include "../window.hpp"
#include "args.hpp"
#include "camera.hpp"

auto main(const int argc, const char* const argv[]) -> int {
    const auto args = parse_args(argc, argv);

    const int fd = open(args.video_device, O_RDWR);
    DYN_ASSERT(fd != -1);
    DYN_ASSERT(v4l2::is_capture_device(fd), "not a capture device");

    if(args.list_formats) {
        v4l2::list_formats(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0);
        return 0;
    }

    if(args.savedir == nullptr) {
        print("no output directory specified");
        exit(1);
    }

    v4l2::set_format(fd, args.pixel_format, args.width, args.height);
    v4l2::set_interval(fd, 1, args.fps);

    const auto req     = v4l2::request_buffers(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_MEMORY_MMAP, num_buffers);
    auto       buffers = v4l2::map_buffers(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, req);
    for(auto i = 0; i < num_buffers; i += 1) {
        v4l2::queue_buffer(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, i);
    }
    v4l2::start_stream(fd);

    const auto file_manager = FileManager(args.savedir);
    auto       context      = Context();
    auto       event_fifo   = args.event_fifo != nullptr ? RemoteServer(args.event_fifo) : RemoteServer();

    auto  app    = gawl::Application();
    auto& window = app.open_window<Window>({}, context, args.movie).get_window();

    switch(args.pixel_format) {
    case v4l2::fourcc("MJPG"):
        init_planar_graphic_globject();
        break;
        V4L2_PIX_FMT_YUV420;
    case v4l2::fourcc("YUYV"):
        init_yuv422i_graphic_globject();
        break;
    case v4l2::fourcc("NV12"):
        init_yuv420sp_graphic_globject();
        break;
    default:
        PANIC("unsupported pixel format");
    }

    const auto fmt    = v4l2::get_current_format(fd);
    auto       camera = Camera(fd, buffers.data(), fmt.width, fmt.height, args.fps, window, file_manager, context, event_fifo);

    auto camera_worker = std::thread([&camera] { camera.run(); });
    app.run();

    camera.finish_thread();
    camera_worker.join();

    return 0;
}
