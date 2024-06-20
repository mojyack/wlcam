#include <thread>

#include <linux/v4l2-subdev.h>

#include "../file.hpp"
#include "../gawl/wayland/application.hpp"
#include "../jpeg.hpp"
#include "../macros/unwrap.hpp"
#include "../media-device.hpp"
#include "../remote-server.hpp"
#include "../udev.hpp"
#include "../util/assert.hpp"
#include "../v4l2.hpp"
#include "../window.hpp"
#include "../yuv.hpp"
#include "algorithm.hpp"
#include "args.hpp"
#include "cio2.hpp"
#include "imgu.hpp"
#include "intel-ipu3.h"
#include "params.hpp"
#include "uapi.hpp"

class IPU3WindowCallbacks : public WindowCallbacks {
  public:
    auto close() -> void override {
        application->quit();
    }

    IPU3WindowCallbacks(Context& context)
        : WindowCallbacks(context) {}
};

auto run(const int argc, const char* const argv[]) -> bool {
    const auto args = ipu3::parse_args(argc, argv);

    unwrap_ob(node_map, dev::enumerate());
    unwrap_ob_mut(cio2_device, parse_device(args.cio2_devnode, node_map));
    assert_b(cio2_device.disable_all_links());

    unwrap_ob_mut(imgu_device, parse_device(args.imgu_devnode, node_map));
    assert_b(imgu_device.disable_all_links());

    auto imgu_0 = ipu3::ImgUDevice();
    assert_b(imgu_0.init(imgu_device, args.imgu_entity));

    auto cio2_0 = ipu3::cio2::CIO2Device(&cio2_device);
    assert_b(cio2_0.init(args.cio2_entity));

    constexpr auto imgu_input_format = v4l2_fourcc('i', 'p', '3', 'b');
    constexpr auto num_buffers       = 3;
    constexpr auto outbuf_mp         = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    constexpr auto capbuf_mp         = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    constexpr auto outbuf_meta       = V4L2_BUF_TYPE_META_OUTPUT;
    constexpr auto capbuf_meta       = V4L2_BUF_TYPE_META_CAPTURE;

    const auto cio2_fd        = cio2_0.cio2.as_handle();
    const auto cio2_sensor_fd = cio2_0.sensor.fd.as_handle();
    const auto cio2_output_fd = cio2_0.output.as_handle();
    assert_b(v4l2::set_format_subdev(cio2_sensor_fd, cio2_0.sensor.pad_index, args.sensor_mbus_code, args.sensor_width, args.sensor_height));
    assert_b(v4l2::set_format_subdev(cio2_fd, 0, args.sensor_mbus_code, args.sensor_width, args.sensor_height));
    unwrap_ob(cio_output_fmt, v4l2::set_format_mp(cio2_output_fd, outbuf_mp, imgu_input_format, 1, args.sensor_width, args.sensor_height, nullptr));

    const auto output          = algo::align_size({args.width, args.height});
    const auto viewfinder      = algo::calculate_best_viewfinder(output, {1920, 1280}); // TODO: reflect window size
    const auto pipeline_config = algo::calculate_pipeline_config({args.sensor_width, args.sensor_height}, output, viewfinder);
    const auto bds_grid        = uapi::create_bds_grid(pipeline_config.bds);

    const auto imgu_fd        = imgu_0.imgu.as_handle();
    const auto imgu_input_fd  = imgu_0.input.as_handle();
    const auto imgu_output_fd = imgu_0.output.as_handle();
    const auto imgu_vf_fd     = imgu_0.viewfinder.as_handle();
    const auto imgu_param_fd  = imgu_0.parameters.as_handle();
    const auto imgu_stat_fd   = imgu_0.stat.as_handle();
    const auto plane_fmts     = cio_output_fmt.fmt.pix_mp.plane_fmt;
    assert_b(v4l2::set_format_mp(imgu_input_fd, outbuf_mp, imgu_input_format, 1, args.sensor_width, args.sensor_height, plane_fmts));
    assert_b(v4l2::set_format_subdev(imgu_fd, imgu_0.imgu_input_pad_index, MEDIA_BUS_FMT_FIXED, pipeline_config.gdc.width, pipeline_config.gdc.height));
    assert_b(v4l2::set_format_subdev(imgu_fd, imgu_0.imgu_parameters_pad_index, MEDIA_BUS_FMT_FIXED, output.width, output.height));
    assert_b(v4l2::set_format_subdev(imgu_fd, imgu_0.imgu_stat_pad_index, MEDIA_BUS_FMT_FIXED, output.width, output.height));
    assert_b(v4l2::set_format_subdev(imgu_fd, imgu_0.imgu_output_pad_index, MEDIA_BUS_FMT_FIXED, output.width, output.height));
    unwrap_ob(imgu_output_fmt, v4l2::set_format_mp(imgu_output_fd, capbuf_mp, V4L2_PIX_FMT_NV12, 2, output.width, output.height, nullptr));
    assert_b(v4l2::set_format_subdev(imgu_fd, imgu_0.imgu_viewfinder_pad_index, MEDIA_BUS_FMT_FIXED, output.width, output.height));
    unwrap_ob(imgu_vf_fmt, v4l2::set_format_mp(imgu_vf_fd, capbuf_mp, V4L2_PIX_FMT_NV12, 2, output.width, output.height, nullptr));

    const auto& iif = pipeline_config.iif;
    const auto& bds = pipeline_config.bds;
    assert_b(v4l2::set_selection_subdev(imgu_fd, imgu_0.imgu_input_pad_index, V4L2_SEL_TGT_CROP, 0, 0, iif.width, iif.height));
    assert_b(v4l2::set_selection_subdev(imgu_fd, imgu_0.imgu_input_pad_index, V4L2_SEL_TGT_COMPOSE, 0, 0, bds.width, bds.height));

    assert_b(v4l2::request_buffers(imgu_input_fd, outbuf_mp, V4L2_MEMORY_DMABUF, num_buffers));
    unwrap_ob(imgu_parameters_req, v4l2::request_buffers(imgu_param_fd, outbuf_meta, V4L2_MEMORY_MMAP, num_buffers));
    unwrap_ob(imgu_parameter_buffers, v4l2::query_and_export_buffers(imgu_param_fd, outbuf_meta, imgu_parameters_req));
    unwrap_ob(imgu_stat_req, v4l2::request_buffers(imgu_stat_fd, capbuf_meta, V4L2_MEMORY_MMAP, num_buffers));
    unwrap_ob(imgu_stat_buffers, v4l2::query_and_export_buffers(imgu_stat_fd, capbuf_meta, imgu_stat_req));
    (void)imgu_stat_buffers; // status feedback is not implemented

    unwrap_ob(imgu_output_req, v4l2::request_buffers(imgu_output_fd, capbuf_mp, V4L2_MEMORY_MMAP, num_buffers));
    unwrap_ob(imgu_output_buffers, v4l2::query_and_export_buffers_mp(imgu_output_fd, capbuf_mp, imgu_output_req));
    assert_b(v4l2::request_buffers(imgu_output_fd, capbuf_mp, V4L2_MEMORY_MMAP, 0));
    assert_b(v4l2::request_buffers(imgu_output_fd, capbuf_mp, V4L2_MEMORY_DMABUF, num_buffers));

    unwrap_ob(imgu_vf_req, v4l2::request_buffers(imgu_vf_fd, capbuf_mp, V4L2_MEMORY_MMAP, num_buffers));
    unwrap_ob(imgu_vf_buffers, v4l2::query_and_export_buffers_mp(imgu_vf_fd, capbuf_mp, imgu_vf_req));
    assert_b(v4l2::request_buffers(imgu_vf_fd, capbuf_mp, V4L2_MEMORY_MMAP, 0));
    assert_b(v4l2::request_buffers(imgu_vf_fd, capbuf_mp, V4L2_MEMORY_DMABUF, num_buffers));

    unwrap_ob(cio2_output_req, v4l2::request_buffers(cio2_output_fd, capbuf_mp, V4L2_MEMORY_MMAP, num_buffers));
    unwrap_ob(cio2_output_buffers, v4l2::query_and_export_buffers_mp(cio2_output_fd, capbuf_mp, cio2_output_req));
    assert_b(v4l2::request_buffers(cio2_output_fd, capbuf_mp, V4L2_MEMORY_MMAP, 0));
    assert_b(v4l2::request_buffers(cio2_output_fd, capbuf_mp, V4L2_MEMORY_DMABUF, num_buffers));

    assert_b(v4l2::start_stream(cio2_output_fd, capbuf_mp));
    assert_b(v4l2::start_stream(imgu_output_fd, capbuf_mp));
    assert_b(v4l2::start_stream(imgu_vf_fd, capbuf_mp));
    assert_b(v4l2::start_stream(imgu_param_fd, outbuf_meta));
    assert_b(v4l2::start_stream(imgu_stat_fd, capbuf_meta));
    assert_b(v4l2::start_stream(imgu_input_fd, outbuf_mp));

    for(auto i = 0; i < num_buffers; i += 1) {
        assert_b(v4l2::queue_buffer_mp(cio2_output_fd, capbuf_mp, V4L2_MEMORY_DMABUF, i, &cio2_output_buffers[i], 1));
    }

    auto output_mmap_ptrs = std::array<void*, num_buffers>();
    for(auto i = 0u; i < num_buffers; i += 1) {
        output_mmap_ptrs[i] = mmap(NULL, imgu_output_buffers[i].length,
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED, imgu_output_buffers[i].fd.as_handle(), 0);
        DYN_ASSERT(output_mmap_ptrs[i] != MAP_FAILED, errno);
    }

    auto vf_mmap_ptrs = std::array<void*, num_buffers>();
    for(auto i = 0u; i < num_buffers; i += 1) {
        vf_mmap_ptrs[i] = mmap(NULL, imgu_vf_buffers[i].length,
                               PROT_READ | PROT_WRITE,
                               MAP_SHARED, imgu_vf_buffers[i].fd.as_handle(), 0);
        DYN_ASSERT(vf_mmap_ptrs[i] != MAP_FAILED, errno);
    }

    auto params_mmap_ptrs = std::array<ipu3_uapi_params*, num_buffers>();
    for(auto i = 0u; i < num_buffers; i += 1) {
        params_mmap_ptrs[i] = (ipu3_uapi_params*)mmap(NULL, imgu_parameter_buffers[i].length,
                                                      PROT_READ | PROT_WRITE,
                                                      MAP_SHARED, imgu_parameter_buffers[i].fd.as_handle(), 0);
        DYN_ASSERT(params_mmap_ptrs[i] != MAP_FAILED, errno);
        memset(params_mmap_ptrs[i], 0, imgu_parameter_buffers[i].length);
        init_params_buffer(*params_mmap_ptrs[i], pipeline_config, bds_grid);
    }

    auto context = Context();

    auto       window_callbacks = std::shared_ptr<IPU3WindowCallbacks>(new IPU3WindowCallbacks(context));
    auto       app              = gawl::WaylandApplication();
    const auto window           = app.open_window({.title = "wlcam"}, window_callbacks);
    const auto wlwindow         = std::bit_cast<gawl::WaylandWindow*>(window);

    auto control_rows                   = create_control_rows();
    auto params_callbacks               = std::shared_ptr<ParamsCallbacks>(new ParamsCallbacks());
    params_callbacks->params_array      = params_mmap_ptrs.data();
    params_callbacks->params_array_size = params_mmap_ptrs.size();
    auto vcw_callbacks                  = std::shared_ptr<vcw::Callbacks>(new vcw::Callbacks(control_rows, params_callbacks));
    app.open_window({.title = "ipu3 parameters", .manual_refresh = true}, vcw_callbacks);

    init_yuv420sp_shader();

    auto finish_camera_thread = false;
    auto camera_thread        = std::thread([&]() -> bool {
        auto       window_context = wlwindow->fork_context();
        auto       file_manager   = FileManager(args.savedir);
        auto       event_fifo     = args.event_fifo != nullptr ? RemoteServer(args.event_fifo) : RemoteServer();
        const auto output_width   = imgu_output_fmt.fmt.pix_mp.width;
        const auto output_height  = imgu_output_fmt.fmt.pix_mp.height;
        const auto output_stride  = imgu_output_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
        const auto vf_width       = imgu_vf_fmt.fmt.pix_mp.width;
        const auto vf_height      = imgu_vf_fmt.fmt.pix_mp.height;
        const auto vf_stride      = imgu_vf_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
        auto       ubuf           = std::vector<std::byte>(output_height / 4 * output_width);
        auto       vbuf           = std::vector<std::byte>(output_height / 4 * output_width);

        if(event_fifo) {
            event_fifo.send_event(RemoteEvents::Hello{});
            event_fifo.send_event(RemoteEvents::Raw{build_string("ipu3 sensor ", cio2_0.sensor.dev_node)});
            if(cio2_0.sensor.lens) {
                event_fifo.send_event(RemoteEvents::Raw{build_string("ipu3 lens ", cio2_0.sensor.lens->dev_node)});
            }
        }

        while(!finish_camera_thread) {
            unwrap_ob(i, v4l2::dequeue_buffer_mp(cio2_output_fd, capbuf_mp, V4L2_MEMORY_DMABUF));

            assert_b(v4l2::queue_buffer_mp(imgu_output_fd, capbuf_mp, V4L2_MEMORY_DMABUF, i, &imgu_output_buffers[i], 1));
            assert_b(v4l2::queue_buffer_mp(imgu_vf_fd, capbuf_mp, V4L2_MEMORY_DMABUF, i, &imgu_vf_buffers[i], 1));
            assert_b(v4l2::queue_buffer(imgu_param_fd, outbuf_meta, i));
            assert_b(v4l2::queue_buffer(imgu_stat_fd, capbuf_meta, i));
            assert_b(v4l2::queue_buffer_mp(imgu_input_fd, outbuf_mp, V4L2_MEMORY_DMABUF, i, &cio2_output_buffers[i], 1));

            assert_b(v4l2::dequeue_buffer_mp(imgu_output_fd, capbuf_mp, V4L2_MEMORY_DMABUF));
            assert_b(v4l2::dequeue_buffer_mp(imgu_vf_fd, capbuf_mp, V4L2_MEMORY_DMABUF));
            assert_b(v4l2::dequeue_buffer(imgu_param_fd, outbuf_meta));
            assert_b(v4l2::dequeue_buffer(imgu_stat_fd, capbuf_meta));
            assert_b(v4l2::dequeue_buffer_mp(imgu_input_fd, outbuf_mp, V4L2_MEMORY_DMABUF));

            const auto vf_buf = std::bit_cast<std::byte*>(vf_mmap_ptrs[i]);
            auto       img    = std::shared_ptr<GraphicLike>(new GraphicLike(Tag<YUV420spGraphic>(), vf_width, vf_height, vf_stride, vf_buf, vf_buf + vf_stride * vf_height));

            // proc commands while flushing texture
            switch(std::exchange(context.camera_command, Command::None)) {
            case Command::TakePhoto: {
                const auto buf   = std::bit_cast<std::byte*>(vf_mmap_ptrs[i]);
                const auto path  = file_manager.get_next_path().string() + ".jpg";
                const auto uvbuf = buf + output_stride * output_height;
                yuv::yuv420sp_uvsp_to_uvp(uvbuf, ubuf.data(), vbuf.data(), output_width, output_height, output_stride);

                unwrap_ob(jpeg, jpg::encode_yuvp_to_jpeg(output_width, output_height, output_stride, 2, 2, buf, ubuf.data(), vbuf.data()));
                const auto fd = FileDescriptor(open(path.c_str(), O_RDWR | O_CREAT, 0644));
                assert_b(fd.as_handle() >= 0);
                assert_b(write(fd.as_handle(), jpeg.buffer.get(), jpeg.size) == ssize_t(jpeg.size));

                context.ui_command = Command::TakePhotoDone;
            } break;
            default:
                break;
            }

            window_context.flush();
            std::swap(context.critical_graphic, img);

            assert_b(v4l2::queue_buffer_mp(cio2_output_fd, capbuf_mp, V4L2_MEMORY_DMABUF, i, &cio2_output_buffers[i], 1));
        }

        event_fifo.send_event(RemoteEvents::Bye{});
        return true;
    });

    app.run();

    finish_camera_thread = true;
    camera_thread.join();
    std::quick_exit(0);

    return 0;
}

auto main(const int argc, const char* const argv[]) -> int {
    return run(argc, argv) ? 0 : 1;
}
