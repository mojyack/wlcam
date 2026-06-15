#include <thread>

#include <linux/v4l2-subdev.h>

#include "../file.hpp"
#include "../gawl/wayland/application.hpp"
#include "../macros/unwrap.hpp"
#include "../media-device.hpp"
#include "../record-context.hpp"
#include "../timer.hpp"
#include "../udev.hpp"
#include "../util/event.hpp"
#include "../v4l2.hpp"
#include "../window.hpp"
#include "algorithm.hpp"
#include "args.hpp"
#include "cio2.hpp"
#include "imgu.hpp"
#include "intel-ipu3.h"
#include "params.hpp"
#include "uapi.hpp"

auto running       = false;
auto camera_thread = std::thread();

class IPU3WindowCallbacks : public WindowCallbacks {
  public:
    Event window_ready;

    auto get_window() -> gawl::WaylandWindow* {
        return std::bit_cast<gawl::WaylandWindow*>(window);
    }

    auto on_created(gawl::Window* window) -> coop::Async<bool> override {
        window_ready.notify();
        co_return co_await WindowCallbacks::on_created(window);
    }

    auto close() -> void override {
        running = false;
        camera_thread.join();
        application->quit();
    }
};

auto main(const int argc, const char* const argv[]) -> int {
    unwrap(args, ipu3::Args::parse(argc, argv));

    // find devices
    unwrap(node_map, dev::enumerate());
    unwrap_mut(cio2_device, parse_device(args.cio2_devnode, node_map));
    ensure(cio2_device.disable_all_links());

    unwrap_mut(imgu_device, parse_device(args.imgu_devnode, node_map));
    ensure(imgu_device.disable_all_links());

    auto imgu_0 = ipu3::ImgUDevice();
    ensure(imgu_0.init(imgu_device, args.imgu_entity));

    auto cio2_0 = ipu3::cio2::CIO2Device(&cio2_device);
    ensure(cio2_0.init(args.cio2_entity));

    // configure pipeline
    constexpr auto imgu_input_format = v4l2_fourcc('i', 'p', '3', 'b');
    constexpr auto num_buffers       = 3;
    constexpr auto outbuf_mp         = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    constexpr auto capbuf_mp         = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    constexpr auto outbuf_meta       = V4L2_BUF_TYPE_META_OUTPUT;
    constexpr auto capbuf_meta       = V4L2_BUF_TYPE_META_CAPTURE;

    const auto cio2_fd        = cio2_0.cio2.as_handle();
    const auto cio2_sensor_fd = cio2_0.sensor.fd.as_handle();
    const auto cio2_output_fd = cio2_0.output.as_handle();
    ensure(v4l2::set_format_subdev(cio2_sensor_fd, cio2_0.sensor.pad_index, args.sensor_mbus_code, args.sensor_width, args.sensor_height));
    ensure(v4l2::set_format_subdev(cio2_fd, 0, args.sensor_mbus_code, args.sensor_width, args.sensor_height));
    unwrap(cio_output_fmt, v4l2::set_format_mp(cio2_output_fd, capbuf_mp, imgu_input_format, 1, args.sensor_width, args.sensor_height, nullptr));

    const auto output     = algo::align_size({int(args.width), int(args.height)});
    const auto viewfinder = algo::calculate_best_viewfinder(output, {1920, 1280}); // TODO: reflect window size
    unwrap(pipeline_config, algo::calculate_pipeline_config({args.sensor_width, args.sensor_height}, output, viewfinder));
    const auto bds_grid = uapi::create_bds_grid(pipeline_config.bds);

    const auto imgu_fd        = imgu_0.imgu.as_handle();
    const auto imgu_input_fd  = imgu_0.input.as_handle();
    const auto imgu_output_fd = imgu_0.output.as_handle();
    const auto imgu_vf_fd     = imgu_0.viewfinder.as_handle();
    const auto imgu_param_fd  = imgu_0.parameters.as_handle();
    const auto imgu_stat_fd   = imgu_0.stat.as_handle();
    const auto plane_fmts     = cio_output_fmt.fmt.pix_mp.plane_fmt;
    ensure(v4l2::set_format_mp(imgu_input_fd, outbuf_mp, imgu_input_format, 1, args.sensor_width, args.sensor_height, plane_fmts));
    ensure(v4l2::set_format_subdev(imgu_fd, imgu_0.imgu_input_pad_index, MEDIA_BUS_FMT_FIXED, pipeline_config.gdc.width, pipeline_config.gdc.height));
    ensure(v4l2::set_format_subdev(imgu_fd, imgu_0.imgu_parameters_pad_index, MEDIA_BUS_FMT_FIXED, output.width, output.height));
    ensure(v4l2::set_format_subdev(imgu_fd, imgu_0.imgu_stat_pad_index, MEDIA_BUS_FMT_FIXED, output.width, output.height));
    ensure(v4l2::set_format_subdev(imgu_fd, imgu_0.imgu_output_pad_index, MEDIA_BUS_FMT_FIXED, output.width, output.height));
    unwrap(imgu_output_fmt, v4l2::set_format_mp(imgu_output_fd, capbuf_mp, V4L2_PIX_FMT_NV12, 2, output.width, output.height, nullptr));
    ensure(v4l2::set_format_subdev(imgu_fd, imgu_0.imgu_viewfinder_pad_index, MEDIA_BUS_FMT_FIXED, output.width, output.height));
    unwrap(imgu_vf_fmt, v4l2::set_format_mp(imgu_vf_fd, capbuf_mp, V4L2_PIX_FMT_NV12, 2, output.width, output.height, nullptr));

    const auto& iif = pipeline_config.iif;
    const auto& bds = pipeline_config.bds;
    ensure(v4l2::set_selection_subdev(imgu_fd, imgu_0.imgu_input_pad_index, V4L2_SEL_TGT_CROP, 0, 0, iif.width, iif.height));
    ensure(v4l2::set_selection_subdev(imgu_fd, imgu_0.imgu_input_pad_index, V4L2_SEL_TGT_COMPOSE, 0, 0, bds.width, bds.height));

    ensure(v4l2::request_buffers(imgu_input_fd, outbuf_mp, V4L2_MEMORY_DMABUF, num_buffers));
    unwrap(imgu_parameters_req, v4l2::request_buffers(imgu_param_fd, outbuf_meta, V4L2_MEMORY_MMAP, num_buffers));
    unwrap(imgu_parameter_buffers, v4l2::query_and_export_buffers(imgu_param_fd, outbuf_meta, imgu_parameters_req));
    unwrap(imgu_stat_req, v4l2::request_buffers(imgu_stat_fd, capbuf_meta, V4L2_MEMORY_MMAP, num_buffers));
    unwrap(imgu_stat_buffers, v4l2::query_and_export_buffers(imgu_stat_fd, capbuf_meta, imgu_stat_req));
    (void)imgu_stat_buffers; // status feedback is not implemented

    unwrap(imgu_output_req, v4l2::request_buffers(imgu_output_fd, capbuf_mp, V4L2_MEMORY_MMAP, num_buffers));
    unwrap(imgu_output_buffers, v4l2::query_and_export_buffers_mp(imgu_output_fd, capbuf_mp, imgu_output_req));
    ensure(v4l2::request_buffers(imgu_output_fd, capbuf_mp, V4L2_MEMORY_MMAP, 0));
    ensure(v4l2::request_buffers(imgu_output_fd, capbuf_mp, V4L2_MEMORY_DMABUF, num_buffers));

    unwrap(imgu_vf_req, v4l2::request_buffers(imgu_vf_fd, capbuf_mp, V4L2_MEMORY_MMAP, num_buffers));
    unwrap(imgu_vf_buffers, v4l2::query_and_export_buffers_mp(imgu_vf_fd, capbuf_mp, imgu_vf_req));
    ensure(v4l2::request_buffers(imgu_vf_fd, capbuf_mp, V4L2_MEMORY_MMAP, 0));
    ensure(v4l2::request_buffers(imgu_vf_fd, capbuf_mp, V4L2_MEMORY_DMABUF, num_buffers));

    unwrap(cio2_output_req, v4l2::request_buffers(cio2_output_fd, capbuf_mp, V4L2_MEMORY_MMAP, num_buffers));
    unwrap(cio2_output_buffers, v4l2::query_and_export_buffers_mp(cio2_output_fd, capbuf_mp, cio2_output_req));
    ensure(v4l2::request_buffers(cio2_output_fd, capbuf_mp, V4L2_MEMORY_MMAP, 0));
    ensure(v4l2::request_buffers(cio2_output_fd, capbuf_mp, V4L2_MEMORY_DMABUF, num_buffers));

    // start stream
    ensure(v4l2::start_stream(cio2_output_fd, capbuf_mp));
    ensure(v4l2::start_stream(imgu_output_fd, capbuf_mp));
    ensure(v4l2::start_stream(imgu_vf_fd, capbuf_mp));
    ensure(v4l2::start_stream(imgu_param_fd, outbuf_meta));
    ensure(v4l2::start_stream(imgu_stat_fd, capbuf_meta));
    ensure(v4l2::start_stream(imgu_input_fd, outbuf_mp));

    // get buffers
    for(auto i = 0; i < num_buffers; i += 1) {
        ensure(v4l2::queue_buffer_mp(cio2_output_fd, capbuf_mp, V4L2_MEMORY_DMABUF, i, &cio2_output_buffers[i], 1));
    }

    auto output_mmap_ptrs = std::array<void*, num_buffers>();
    for(auto i = 0u; i < num_buffers; i += 1) {
        output_mmap_ptrs[i] = mmap(NULL, imgu_output_buffers[i].length,
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED, imgu_output_buffers[i].fd.as_handle(), 0);
        ensure(output_mmap_ptrs[i] != MAP_FAILED, "errno={}", errno);
    }

    auto vf_mmap_ptrs = std::array<void*, num_buffers>();
    for(auto i = 0u; i < num_buffers; i += 1) {
        vf_mmap_ptrs[i] = mmap(NULL, imgu_vf_buffers[i].length,
                               PROT_READ | PROT_WRITE,
                               MAP_SHARED, imgu_vf_buffers[i].fd.as_handle(), 0);
        ensure(vf_mmap_ptrs[i] != MAP_FAILED, "errno={}", errno);
    }

    auto params_mmap_ptrs = std::array<ipu3_uapi_params*, num_buffers>();
    for(auto i = 0u; i < num_buffers; i += 1) {
        params_mmap_ptrs[i] = (ipu3_uapi_params*)mmap(NULL, imgu_parameter_buffers[i].length,
                                                      PROT_READ | PROT_WRITE,
                                                      MAP_SHARED, imgu_parameter_buffers[i].fd.as_handle(), 0);
        ensure(params_mmap_ptrs[i] != MAP_FAILED, "errno={}", errno);
        memset(params_mmap_ptrs[i], 0, imgu_parameter_buffers[i].length);
        init_params_buffer(*params_mmap_ptrs[i], pipeline_config, bds_grid);
    }
    params_buffers = params_mmap_ptrs; // for params.cpp

    // prepare gui
    auto app = gawl::WaylandApplication();
    ensure(init_yuv420sp_shader());
    auto viewfinder_cbs = std::shared_ptr<IPU3WindowCallbacks>(new IPU3WindowCallbacks());
    create_buttons(viewfinder_cbs->buttons, args.ipu3_params);

    running       = true;
    camera_thread = std::thread([&]() -> bool {
        constexpr auto error_value = false;

        viewfinder_cbs->window_ready.wait();
        auto&      context        = viewfinder_cbs->get_context();
        auto       window_context = viewfinder_cbs->get_window()->fork_context();
        auto       file_manager   = FileManager(args.savedir);
        auto       record_context = std::unique_ptr<RecordContext>();
        const auto output_width   = imgu_output_fmt.fmt.pix_mp.width;
        const auto output_height  = imgu_output_fmt.fmt.pix_mp.height;
        const auto output_stride  = imgu_output_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
        const auto vf_width       = imgu_vf_fmt.fmt.pix_mp.width;
        const auto vf_height      = imgu_vf_fmt.fmt.pix_mp.height;
        const auto vf_stride      = imgu_vf_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
        auto       ubuf           = std::vector<std::byte>(output_height / 4 * output_width);
        auto       vbuf           = std::vector<std::byte>(output_height / 4 * output_width);

        std::println("ipu3 sensor {}", cio2_0.sensor.dev_node);
        if(cio2_0.sensor.lens) {
            std::println("ipu3 lens {}", cio2_0.sensor.lens->dev_node);
        }
        std::println("ready");

        auto counter = FPSCounter();

    loop:
        if(!running) {
            return true;
        }
        // get raw image
        unwrap_v(i, v4l2::dequeue_buffer_mp(cio2_output_fd, capbuf_mp, V4L2_MEMORY_DMABUF));

        // start processing
        ensure_v(v4l2::queue_buffer_mp(imgu_output_fd, capbuf_mp, V4L2_MEMORY_DMABUF, i, &imgu_output_buffers[i], 1));
        ensure_v(v4l2::queue_buffer_mp(imgu_vf_fd, capbuf_mp, V4L2_MEMORY_DMABUF, i, &imgu_vf_buffers[i], 1));
        ensure_v(v4l2::queue_buffer(imgu_param_fd, outbuf_meta, i));
        ensure_v(v4l2::queue_buffer(imgu_stat_fd, capbuf_meta, i));
        ensure_v(v4l2::queue_buffer_mp(imgu_input_fd, outbuf_mp, V4L2_MEMORY_DMABUF, i, &cio2_output_buffers[i], 1));

        // get processed image
        ensure_v(v4l2::dequeue_buffer_mp(imgu_output_fd, capbuf_mp, V4L2_MEMORY_DMABUF));
        ensure_v(v4l2::dequeue_buffer_mp(imgu_vf_fd, capbuf_mp, V4L2_MEMORY_DMABUF));
        ensure_v(v4l2::dequeue_buffer(imgu_param_fd, outbuf_meta));
        ensure_v(v4l2::dequeue_buffer(imgu_stat_fd, capbuf_meta));
        ensure_v(v4l2::dequeue_buffer_mp(imgu_input_fd, outbuf_mp, V4L2_MEMORY_DMABUF));

        // load texutre
        auto       frame      = std::shared_ptr<Frame>(new YUV420SPFrame(vf_width, vf_height, vf_stride));
        const auto byte_array = Frame::ByteArray{static_cast<std::byte*>(vf_mmap_ptrs[i]), imgu_vf_buffers[i].length};
        ensure_v(frame->load_texture(byte_array));

        // process commands while flushing texture
        switch(std::exchange(context.camera_command, Command::None)) {
        case Command::TakePhoto: {
            const auto path = file_manager.get_next_path().string() + ".jpg";

            const auto byte_array = Frame::ByteArray{static_cast<std::byte*>(output_mmap_ptrs[i]), imgu_output_buffers[i].length};
            auto       frame      = YUV420SPFrame(output_width, output_height, output_stride);
            ensure_v(frame.save_to_jpeg(byte_array, path.data()));

            context.ui_command = Command::TakePhotoDone;
        } break;
        case Command::StartRecording: {
            const auto path = file_manager.get_next_path().string() + ".mkv";

            unwrap_v(pix_fmt, frame->get_pixel_format());
            auto rc = std::unique_ptr<RecordContext>(new RecordContext());
            ensure_v(rc->init(path, pix_fmt, args));
            record_context.reset(rc.release());

            context.ui_command = Command::StartRecordingDone;
        } break;
        case Command::StopRecording: {
            record_context.reset();

            context.ui_command = Command::StopRecordingDone;
        } break;
        default:
            break;
        }

        if(record_context) {
            unwrap_v(planes, frame->get_planes(byte_array));
            record_context->encoder.add_frame(planes, record_context->timer.elapsed<std::chrono::microseconds>());
        }

        // update displayed image
        window_context.flush();
        context.frame = frame;

        // release buffer to system
        ensure_v(v4l2::queue_buffer_mp(cio2_output_fd, capbuf_mp, V4L2_MEMORY_DMABUF, i, &cio2_output_buffers[i], 1));

        if(const auto c = counter.tick(); c >= 0) {
            context.capture_rate = c;
        }

        goto loop;
    });

    auto runner = coop::Runner();
    runner.push_task(app.open_window({.title = "wlcam"}, std::move(viewfinder_cbs)));
    runner.push_task(app.run());
    runner.run();

    return 0;
}
