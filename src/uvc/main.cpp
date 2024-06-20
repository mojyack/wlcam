#include "../file.hpp"
#include "../gawl/wayland/application.hpp"
#include "../macros/unwrap.hpp"
#include "../util/assert.hpp"
#include "../v4l2.hpp"
#include "../window.hpp"
#include "args.hpp"
#include "camera.hpp"

class UVCWindowCallbacks : public WindowCallbacks {
  public:
    Camera* cam;

    auto close() -> void override {
        cam->shutdown();
        application->quit();
    }

    UVCWindowCallbacks(Context& context)
        : WindowCallbacks(context) {}
};

auto run(const int argc, const char* const argv[]) -> bool {
    const auto args = parse_args(argc, argv);

    const int fd = open(args.video_device, O_RDWR);
    DYN_ASSERT(fd != -1);
    unwrap_ob(is_capture_device, v4l2::is_capture_device(fd));
    assert_b(is_capture_device, "not a capture device");

    if(args.list_formats) {
        v4l2::list_formats(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0);
        return 0;
    }

    assert_b(v4l2::set_format(fd, args.pixel_format, args.width, args.height));
    assert_b(v4l2::set_interval(fd, 1, args.fps));

    unwrap_ob(req, v4l2::request_buffers(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_MEMORY_MMAP, num_buffers));
    unwrap_ob_mut(buffers, v4l2::map_buffers(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, req));
    for(auto i = 0; i < num_buffers; i += 1) {
        assert_b(v4l2::queue_buffer(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, i));
    }
    assert_b(v4l2::start_stream(fd));

    const auto file_manager = FileManager(args.savedir);
    auto       context      = Context();
    auto       event_fifo   = args.event_fifo != nullptr ? RemoteServer(args.event_fifo) : RemoteServer();

    auto       callbacks = std::shared_ptr<UVCWindowCallbacks>(new UVCWindowCallbacks(context));
    auto       app       = gawl::WaylandApplication();
    const auto window    = app.open_window({.title = "wlcam"}, callbacks);
    const auto wlwindow  = std::bit_cast<gawl::WaylandWindow*>(window);

    switch(args.pixel_format) {
    case v4l2::fourcc("MJPG"):
        init_planar_shader();
        break;
        V4L2_PIX_FMT_YUV420;
    case v4l2::fourcc("YUYV"):
        init_yuv422i_shader();
        break;
    case v4l2::fourcc("NV12"):
        init_yuv420sp_shader();
        break;
    default:
        PANIC("unsupported pixel format");
    }

    unwrap_ob(fmt, v4l2::get_current_format(fd));
    auto camera    = Camera(fd, buffers.data(), fmt.width, fmt.height, args.fps, *wlwindow, file_manager, context, args.event_fifo ? &event_fifo : nullptr);
    callbacks->cam = &camera;

    camera.run();
    app.run();
    camera.shutdown();

    return 0;
}

auto main(const int argc, const char* const argv[]) -> int {
    return run(argc, argv) ? 0 : 1;
}
