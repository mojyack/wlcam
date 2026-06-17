#include "../gawl/wayland/application.hpp"
#include "../macros/unwrap.hpp"
#include "../ui-v4l2.hpp"
#include "../v4l2.hpp"
#include "../window.hpp"
#include "args.hpp"
#include "camera.hpp"

class UVCWindowCallbacks : public WindowCallbacks {
  public:
    CameraParams params;
    Camera       cam;

    auto on_created(gawl::Window* window) -> coop::Async<bool> override {
        params.window = std::bit_cast<gawl::WaylandWindow*>(window);
        co_await cam.run(params);
        co_return co_await WindowCallbacks::on_created(window);
    }

    auto close() -> void override {
        cam.shutdown();
        application->quit();
    }
};

auto main(const int argc, const char* const* const argv) -> int {
    unwrap(args, Args::parse(argc, argv));

    const auto fdh = FileDescriptor(open(args.video_device, O_RDWR));
    const auto fd  = fdh.as_handle();
    ensure(fd >= 0);
    unwrap(is_capture_device, v4l2::is_capture_device(fd));
    ensure(is_capture_device, "not a capture device");

    if(args.list_formats) {
        v4l2::list_formats(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0);
        return 0;
    }

    ensure(v4l2::set_format(fd, args.pixel_format.data, args.width, args.height));
    ensure(v4l2::set_interval(fd, 1, args.fps));

    unwrap(req, v4l2::request_buffers(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_MEMORY_MMAP, num_buffers));
    unwrap_mut(buffers, v4l2::map_buffers(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, req));
    for(auto i = 0; i < num_buffers; i += 1) {
        ensure(v4l2::queue_buffer(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, i));
    }
    ensure(v4l2::start_stream(fd));

    auto app = gawl::WaylandApplication();

    switch(args.pixel_format.data) {
    case v4l2::fourcc("MJPG"):
        ensure(init_planar_shader());
        break;
    case v4l2::fourcc("YUYV"):
        ensure(init_yuv422i_shader());
        break;
    case v4l2::fourcc("NV12"):
        ensure(init_yuv420sp_shader());
        break;
    default:
        bail("unsupported pixel format");
    }

    unwrap(fmt, v4l2::get_current_format(fd));
    auto cbs    = std::shared_ptr<UVCWindowCallbacks>(new UVCWindowCallbacks());
    cbs->params = CameraParams{
        .fd             = fd,
        .width          = fmt.width,
        .height         = fmt.height,
        .fps            = uint32_t(args.fps),
        .buffers        = buffers.data(),
        .window         = nullptr, // set later
        .window_context = &cbs->get_context(),
        .args           = &args,
    };

    auto bundle = V4L2ControlBundle{
        .fd    = fd,
        .ctrls = v4l2::query_controls(fd),
    };
    for(auto& ctrl : bundle.ctrls) {
        auto element = (V4L2Element*)(nullptr);
        auto button  = (Button*)(nullptr);
        switch(ctrl.type) {
        case v4l2::ControlType::Int: {
            const auto btn = new V4L2SliderButton();
            element        = &btn->slider;
            button         = btn;
        } break;
        case v4l2::ControlType::Bool: {
            const auto btn = new V4L2Button();
            element        = btn;
            button         = btn;
            btn->pressed   = ctrl.current;
        } break;
        case v4l2::ControlType::Menu: {
            const auto btn = new V4L2MenuButton();
            element        = &btn->menu;
            button         = btn;
        } break;
        }
        element->bundle = &bundle;
        element->ctrl   = &ctrl;
        cbs->buttons.emplace_back(button);
    }

    auto runner = coop::Runner();
    runner.push_task(app.run());
    runner.push_task(app.open_window({.title = "wlcam"}, std::move(cbs)));
    runner.run();

    return 0;
}
