#include <algorithm>
#include <array>
#include <format>
#include <print>
#include <span>
#include <string_view>

#include <linux/media-bus-format.h>
#include <linux/media.h>
#include <linux/v4l2-controls.h>
#include <poll.h>
#include <sys/ioctl.h>

#include "../gawl/wayland/application.hpp"
#include "../graphics/bayer.hpp"
#include "../macros/unwrap.hpp"
#include "../media-device.hpp"
#include "../udev.hpp"
#include "../ui-v4l2.hpp"
#include "../v4l2.hpp"
#include "../window.hpp"
#include "args.hpp"
#include "camera.hpp"

namespace {
struct BayerFormat {
    uint32_t             fourcc;
    std::array<float, 2> first_red; // position of the first red sample
};

auto bayer_format_for(const uint32_t mbus) -> std::optional<BayerFormat> {
    switch(mbus) {
    case MEDIA_BUS_FMT_SRGGB10_1X10:
        return BayerFormat{V4L2_PIX_FMT_SRGGB10P, {0.0f, 0.0f}};
    case MEDIA_BUS_FMT_SGRBG10_1X10:
        return BayerFormat{V4L2_PIX_FMT_SGRBG10P, {1.0f, 0.0f}};
    case MEDIA_BUS_FMT_SGBRG10_1X10:
        return BayerFormat{V4L2_PIX_FMT_SGBRG10P, {0.0f, 1.0f}};
    case MEDIA_BUS_FMT_SBGGR10_1X10:
        return BayerFormat{V4L2_PIX_FMT_SBGGR10P, {1.0f, 1.0f}};
    default:
        return std::nullopt;
    }
}

auto find_first_mbus_code(const int fd, const uint32_t pad) -> std::optional<uint32_t> {
    auto e  = v4l2_subdev_mbus_code_enum();
    e.pad   = pad;
    e.index = 0;
    e.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    ensure(ioctl(fd, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &e) == 0, "failed to enumerate sensor mbus code");
    return e.code;
}

struct Pipeline {
    FileDescriptor     sensor_fd;
    FileDescriptor     capture_fd;
    FileDescriptor     vcm_fd;
    v4l2::SubdevFormat format;
};

auto setup_pipeline(MediaDevice& media, const camss::Args& args) -> std::optional<Pipeline> {
    // find entities
    unwrap(csiphy, media.find_entity_by_name(std::format("msm_csiphy{}", args.csiphy)));
    unwrap(csid, media.find_entity_by_name(std::format("msm_csid{}", args.csid)));
    unwrap(rdi, media.find_entity_by_name(std::format("msm_vfe{}_rdi0", args.vfe))); // hardcode rdi0

    // find sensor entity, rdi capture entity, and links
    unwrap(sensor_csiphy_link, media.find_link(nullptr, &csiphy));
    unwrap(csiphy_csid_link, media.find_link(&csiphy, &csid));
    unwrap(csid_rdi_link, media.find_link(&csid, &rdi));
    unwrap(rdi_capture_link, media.find_link(&rdi, nullptr));
    const auto& sensor  = *sensor_csiphy_link.src_entity;
    const auto& capture = *rdi_capture_link.sink_entity;

    // open subdevs
    auto sensor_fd = FileDescriptor(open(sensor.dev_node.data(), O_RDWR));
    ensure(sensor_fd.as_handle() >= 0);
    auto csiphy_fd = FileDescriptor(open(csiphy.dev_node.data(), O_RDWR));
    ensure(csiphy_fd.as_handle() >= 0);
    auto csid_fd = FileDescriptor(open(csid.dev_node.data(), O_RDWR));
    ensure(csid_fd.as_handle() >= 0);
    auto vfe_fd = FileDescriptor(open(rdi.dev_node.data(), O_RDWR));
    ensure(vfe_fd.as_handle() >= 0);
    auto capture_fd = FileDescriptor(open(capture.dev_node.data(), O_RDWR));
    ensure(capture_fd.as_handle() >= 0);

    // enable links
    ensure(media.enable_exclusive_link(sensor_csiphy_link));
    ensure(media.enable_exclusive_link(csiphy_csid_link));
    ensure(media.enable_exclusive_link(csid_rdi_link));
    ensure(media.enable_exclusive_link(rdi_capture_link));

    // set and retrive effective format
    unwrap(mbus, find_first_mbus_code(sensor_fd.as_handle(), sensor_csiphy_link.src_pad_num));
    const auto format = v4l2::SubdevFormat{mbus, uint32_t(args.width), uint32_t(args.height)};
    unwrap(actual, v4l2::set_format_subdev(sensor_fd.as_handle(), sensor_csiphy_link.src_pad_num, format));

    // sync format
    ensure(v4l2::set_format_subdev(csiphy_fd.as_handle(), sensor_csiphy_link.sink_pad_num, actual));
    ensure(v4l2::set_format_subdev(csiphy_fd.as_handle(), csiphy_csid_link.src_pad_num, actual));
    ensure(v4l2::set_format_subdev(csid_fd.as_handle(), csiphy_csid_link.sink_pad_num, actual));
    ensure(v4l2::set_format_subdev(csid_fd.as_handle(), csid_rdi_link.src_pad_num, actual));
    ensure(v4l2::set_format_subdev(vfe_fd.as_handle(), csid_rdi_link.sink_pad_num, actual));

    // find vcm
    auto vcm_fd = FileDescriptor();
    if(sensor.ancillary_entities.size() == 1) {
        unwrap(vcm, media.find_entity_by_id(sensor.ancillary_entities[0]));
        vcm_fd = FileDescriptor(open(vcm.dev_node.data(), O_RDWR));
    }

    return Pipeline{
        .sensor_fd  = std::move(sensor_fd),
        .capture_fd = std::move(capture_fd),
        .vcm_fd     = std::move(vcm_fd),
        .format     = actual,
    };
}

auto filter_controls_by_whitelist(std::vector<v4l2::Control>& ctrls, const std::span<const std::string_view> allowed_names) -> void {
    std::erase_if(ctrls, [allowed_names](const v4l2::Control& ctrl) {
        const auto allowed = std::ranges::find(allowed_names, std::string_view(ctrl.name)) != allowed_names.end();
        std::println("{} {}", allowed ? '*' : '-', ctrl.name);
        return !allowed;
    });
}

class CamssWindowCallbacks : public WindowCallbacks {
  public:
    camss::Camera cam;

    auto on_created(gawl::Window* window) -> coop::Async<bool> override {
        co_await cam.start();
        co_return co_await WindowCallbacks::on_created(window);
    }

    auto close() -> void override {
        cam.shutdown();
        application->quit();
    }
};
} // namespace

// ui.cpp
auto add_debayer_param_buttons(std::vector<std::unique_ptr<Button>>& buttons) -> void;

auto main(const int argc, const char* const argv[]) -> int {
    unwrap(args, camss::Args::parse(argc, argv));

    // configure the camss media pipeline (sensor -> csiphy -> csid -> vfe rdi)
    unwrap(node_map, dev::enumerate());
    unwrap_mut(media, parse_device(args.media_device, node_map));
    unwrap(pipeline, setup_pipeline(media, args));

    // setup vfe output
    const auto fd = pipeline.capture_fd.as_handle();
    unwrap(bayer_fmt, bayer_format_for(pipeline.format.code));
    constexpr auto cap_mp = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    unwrap(fmt, v4l2::set_format_mp(fd, cap_mp, bayer_fmt.fourcc, 1, pipeline.format.width, pipeline.format.height, nullptr));

    const auto width  = fmt.fmt.pix_mp.width;
    const auto height = fmt.fmt.pix_mp.height;
    const auto stride = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;

    // allocate MMAP buffers, export them as dmabufs, then re-import as DMABUF
    unwrap(req, v4l2::request_buffers(fd, cap_mp, V4L2_MEMORY_MMAP, camss::num_buffers));
    unwrap_mut(dmabufs, v4l2::query_and_export_buffers_mp(fd, cap_mp, req));
    ensure(v4l2::request_buffers(fd, cap_mp, V4L2_MEMORY_MMAP, 0));
    ensure(v4l2::request_buffers(fd, cap_mp, V4L2_MEMORY_DMABUF, camss::num_buffers));

    // map the dmabufs so the loader can upload them to the GPU
    auto mmap_ptrs = std::array<void*, camss::num_buffers>();
    for(auto i = 0u; i < camss::num_buffers; i += 1) {
        mmap_ptrs[i] = mmap(NULL, dmabufs[i].length, PROT_READ, MAP_SHARED, dmabufs[i].fd.as_handle(), 0);
        ensure(mmap_ptrs[i] != MAP_FAILED, "mmap failed: {}", errno);
    }

    for(auto i = 0u; i < camss::num_buffers; i += 1) {
        ensure(v4l2::queue_buffer_mp(fd, cap_mp, V4L2_MEMORY_DMABUF, i, &dmabufs[i], 1));
    }

    ensure(v4l2::start_stream(fd, cap_mp));

    std::println("camss: {}x{} stride={}", width, height, stride);

    auto app = gawl::WaylandApplication();

    bayer_params.first_red   = bayer_fmt.first_red;
    bayer_params.cell        = args.cell;
    bayer_params.black_level = args.black_level / 255.f;
    bayer_params.gamma       = args.gamma / 100.f;
    bayer_params.wb_gain     = {args.wb_r / 100.f, args.wb_g / 100.f, args.wb_b / 100.f};
    bayer_params.lsc         = args.lsc / 100.f;
    bayer_params.rotate      = args.rotate / 90;
    ensure(init_bayer_shader());

    auto cbs = std::shared_ptr<CamssWindowCallbacks>(new CamssWindowCallbacks());
    ensure(cbs->cam.init({
        .fd             = fd,
        .width          = width,
        .height         = height,
        .stride         = stride,
        .dmabufs        = dmabufs.data(),
        .mmap_ptrs      = mmap_ptrs.data(),
        .window_context = &cbs->get_context(),
        .args           = &args,
    }));

    auto bundle_sensor = V4L2ControlBundle{
        .fd    = pipeline.sensor_fd.as_handle(),
        .ctrls = v4l2::query_controls(pipeline.sensor_fd.as_handle()),
    };
    bundle_sensor.fixup = [&bundle = bundle_sensor, sensor_height = pipeline.format.height] {
        for(auto& ctrl : bundle.ctrls) {
            if(ctrl.id == V4L2_CID_VBLANK) { // override vblank max to usable range (max fps ~ max/8 fps)
                ctrl.max = std::min<uint32_t>(ctrl.max, 8 * (sensor_height + ctrl.min) - sensor_height);
            }
        }
    };
    bundle_sensor.fixup();
    // TODO: make configurable
    static const auto allowed_controls = std::vector<std::string_view>{
        // sensor
        "Analogue Gain",
        "Digital Gain",
        "Exposure",
        "Vertical Blanking",
        // vcm
        "Focus, Absolute",
    };
    filter_controls_by_whitelist(bundle_sensor.ctrls, allowed_controls);
    build_buttons_from_controls(bundle_sensor, cbs->buttons);
    auto bundle_vcm = V4L2ControlBundle();
    if(pipeline.vcm_fd.as_handle() >= 0) {
        bundle_vcm.fd    = pipeline.vcm_fd.as_handle();
        bundle_vcm.ctrls = v4l2::query_controls(pipeline.vcm_fd.as_handle());
        filter_controls_by_whitelist(bundle_vcm.ctrls, allowed_controls);
        build_buttons_from_controls(bundle_vcm, cbs->buttons);
    }
    add_debayer_param_buttons(cbs->buttons);

    auto runner = coop::Runner();
    runner.push_task(app.run());
    runner.push_task(app.open_window({.title = "wlcam"}, std::move(cbs)));
    runner.run();

    return 0;
}
