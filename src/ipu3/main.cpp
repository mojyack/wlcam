#include <thread>

#include <linux/v4l2-subdev.h>

#include "../assert.hpp"
#include "../file.hpp"
#include "../jpeg.hpp"
#include "../media-device.hpp"
#include "../udev.hpp"
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

auto main(const int argc, const char* const argv[]) -> int {
    const auto args = ipu3::parse_args(argc, argv);

    const auto node_map    = dev::enumerate();
    auto       cio2_device = parse_device(args.cio2_devnode, node_map);
    cio2_device.disable_all_links();

    auto imgu_device = parse_device(args.imgu_devnode, node_map);
    imgu_device.disable_all_links();

    auto imgu_0 = ipu3::ImgUDevice();
    imgu_0.init(imgu_device, args.imgu_entity);

    auto cio2_0 = ipu3::cio2::CIO2Device(&cio2_device);
    cio2_0.init(args.cio2_entity);

    constexpr auto imgu_input_format = v4l2_fourcc('i', 'p', '3', 'b');
    constexpr auto num_buffers       = 3;

    v4l2::set_format_subdev(cio2_0.sensor.fd, cio2_0.sensor.pad_index, args.sensor_mbus_code, args.sensor_width, args.sensor_height);
    v4l2::set_format_subdev(cio2_0.cio2, 0, args.sensor_mbus_code, args.sensor_width, args.sensor_height);
    const auto cio_output_fmt = v4l2::set_format_mp(cio2_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, imgu_input_format, 1, args.sensor_width, args.sensor_height, nullptr);

    const auto output          = algo::align_size({args.width, args.height});
    const auto viewfinder      = algo::align_size({1920, 1280}); // TODO
    const auto pipeline_config = algo::calculate_pipeline_config({args.sensor_width, args.sensor_height}, output, viewfinder);
    const auto bds_grid        = uapi::create_bds_grid(pipeline_config.bds);

    v4l2::set_format_mp(imgu_0.input, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, imgu_input_format, 1, args.sensor_width, args.sensor_height, cio_output_fmt.fmt.pix_mp.plane_fmt);
    v4l2::set_format_subdev(imgu_0.imgu, imgu_0.imgu_input_pad_index, MEDIA_BUS_FMT_FIXED, pipeline_config.gdc.width, pipeline_config.gdc.height); // GDC
    v4l2::set_format_subdev(imgu_0.imgu, imgu_0.imgu_parameters_pad_index, MEDIA_BUS_FMT_FIXED, output.width, output.height);
    v4l2::set_format_subdev(imgu_0.imgu, imgu_0.imgu_stat_pad_index, MEDIA_BUS_FMT_FIXED, output.width, output.height);
    v4l2::set_format_subdev(imgu_0.imgu, imgu_0.imgu_output_pad_index, MEDIA_BUS_FMT_FIXED, output.width, output.height);
    const auto imgu_output_fmt = v4l2::set_format_mp(imgu_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_PIX_FMT_NV12, 2, output.width, output.height, nullptr);
    v4l2::set_format_subdev(imgu_0.imgu, imgu_0.imgu_viewfinder_pad_index, MEDIA_BUS_FMT_FIXED, output.width, output.height);
    v4l2::set_format_mp(imgu_0.viewfinder, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_PIX_FMT_NV12, 2, output.width, output.height, nullptr);

    v4l2::set_selection_subdev(imgu_0.imgu, imgu_0.imgu_input_pad_index, V4L2_SEL_TGT_CROP, 0, 0, pipeline_config.iif.width, pipeline_config.iif.height);    // IF
    v4l2::set_selection_subdev(imgu_0.imgu, imgu_0.imgu_input_pad_index, V4L2_SEL_TGT_COMPOSE, 0, 0, pipeline_config.bds.width, pipeline_config.bds.height); // BDS

    v4l2::request_buffers(imgu_0.input, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_DMABUF, num_buffers);
    const auto imgu_parameters_req    = v4l2::request_buffers(imgu_0.parameters, V4L2_BUF_TYPE_META_OUTPUT, V4L2_MEMORY_MMAP, num_buffers);
    const auto imgu_parameter_buffers = v4l2::query_and_export_buffers(imgu_0.parameters, V4L2_BUF_TYPE_META_OUTPUT, imgu_parameters_req);
    const auto imgu_stat_req          = v4l2::request_buffers(imgu_0.stat, V4L2_BUF_TYPE_META_CAPTURE, V4L2_MEMORY_MMAP, num_buffers);
    const auto imgu_stat_buffers      = v4l2::query_and_export_buffers(imgu_0.stat, V4L2_BUF_TYPE_META_CAPTURE, imgu_stat_req);

    const auto imgu_output_req     = v4l2::request_buffers(imgu_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP, num_buffers);
    const auto imgu_output_buffers = v4l2::query_and_export_buffers_mp(imgu_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, imgu_output_req);
    v4l2::request_buffers(imgu_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP, 0);
    v4l2::request_buffers(imgu_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_DMABUF, num_buffers);

    v4l2::request_buffers(imgu_0.viewfinder, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_DMABUF, num_buffers);

    const auto cio2_output_req     = v4l2::request_buffers(cio2_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP, num_buffers);
    const auto cio2_output_buffers = v4l2::query_and_export_buffers_mp(cio2_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, cio2_output_req);
    v4l2::request_buffers(cio2_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP, 0);
    v4l2::request_buffers(cio2_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_DMABUF, num_buffers);

    v4l2::start_stream(cio2_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
    v4l2::start_stream(imgu_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
    v4l2::start_stream(imgu_0.viewfinder, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
    v4l2::start_stream(imgu_0.parameters, V4L2_BUF_TYPE_META_OUTPUT);
    v4l2::start_stream(imgu_0.stat, V4L2_BUF_TYPE_META_CAPTURE);
    v4l2::start_stream(imgu_0.input, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

    for(auto i = 0; i < num_buffers; i += 1) {
        v4l2::queue_buffer_mp(cio2_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_DMABUF, i, {&cio2_output_buffers[i]});
    }

    auto output_mmap_ptrs = std::array<void*, num_buffers>();
    for(auto i = 0u; i < num_buffers; i += 1) {
        output_mmap_ptrs[i] = mmap(NULL, imgu_output_buffers[i].length,
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED, imgu_output_buffers[i].fd, 0);
        DYN_ASSERT(output_mmap_ptrs[i] != MAP_FAILED, errno);
    }

    auto params_mmap_ptrs = std::array<ipu3_uapi_params*, num_buffers>();
    for(auto i = 0u; i < num_buffers; i += 1) {
        params_mmap_ptrs[i] = (ipu3_uapi_params*)mmap(NULL, imgu_parameter_buffers[i].length,
                                                      PROT_READ | PROT_WRITE,
                                                      MAP_SHARED, imgu_parameter_buffers[i].fd, 0);
        DYN_ASSERT(params_mmap_ptrs[i] != MAP_FAILED, errno);
        memset(params_mmap_ptrs[i], 0, imgu_parameter_buffers[i].length);
        init_params_buffer(*params_mmap_ptrs[i], pipeline_config, bds_grid);
    }

    auto control_rows  = create_control_rows();
    auto apply_control = [&](Control& control, const int value) -> bool {
        apply_controls(params_mmap_ptrs.data(), params_mmap_ptrs.size(), control, value);
        return false;
    };

    auto context = Context();
    auto app     = gawl::Application();
    auto window  = &app.open_window<Window>({}, context, false).get_window();
    app.open_window<VCWindow>({.manual_refresh = true}, control_rows, apply_control);

    init_yuv420sp_graphic_globject();

    auto finish_camera_thread = false;
    auto camera_thread        = std::thread([&]() {
        auto       window_context = window->fork_context();
        auto       file_manager   = FileManager(args.savedir);
        const auto width          = imgu_output_fmt.fmt.pix_mp.width;
        const auto height         = imgu_output_fmt.fmt.pix_mp.height;
        const auto stride         = imgu_output_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
        auto       ubuf           = std::vector<std::byte>(height / 4 * width);
        auto       vbuf           = std::vector<std::byte>(height / 4 * width);

        while(!finish_camera_thread) {
            const auto i = v4l2::dequeue_buffer_mp(cio2_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_DMABUF);

            v4l2::queue_buffer_mp(imgu_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_DMABUF, i, {&imgu_output_buffers[i]});
            v4l2::queue_buffer(imgu_0.parameters, V4L2_BUF_TYPE_META_OUTPUT, i);
            v4l2::queue_buffer(imgu_0.stat, V4L2_BUF_TYPE_META_CAPTURE, i);
            v4l2::queue_buffer_mp(imgu_0.input, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_DMABUF, i, {&cio2_output_buffers[i]});

            v4l2::dequeue_buffer_mp(imgu_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_DMABUF);
            v4l2::dequeue_buffer(imgu_0.parameters, V4L2_BUF_TYPE_META_OUTPUT);
            v4l2::dequeue_buffer(imgu_0.stat, V4L2_BUF_TYPE_META_CAPTURE);
            v4l2::dequeue_buffer_mp(imgu_0.input, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_DMABUF);

            const auto buf = std::bit_cast<std::byte*>(output_mmap_ptrs[i]);
            auto       img = std::shared_ptr<GraphicLike>(new GraphicLike(Tag<YUV420spGraphic>(), width, height, stride, buf, buf + stride * height));

            // proc commands while flushing texture
            switch(std::exchange(context.camera_command, Command::None)) {
            case Command::TakePhoto: {
                const auto path  = file_manager.get_next_path().string() + ".jpg";
                const auto uvbuf = buf + stride * height;
                yuv::yuv420sp_uvsp_to_uvp(uvbuf, ubuf.data(), vbuf.data(), width, height, stride);

                const auto [jpegbuf, jpegsize] = jpg::encode_yuvp_to_jpeg(width, height, stride, 2, 2, buf, ubuf.data(), vbuf.data());
                const auto fd                  = open(path.c_str(), O_RDWR | O_CREAT, 0644);
                DYN_ASSERT(write(fd, jpegbuf.get(), jpegsize) == ssize_t(jpegsize));
                close(fd);

                context.ui_command = Command::TakePhotoDone;
            } break;
            default:
                break;
            }

            window_context.flush();
            std::swap(context.critical_graphic, img);

            v4l2::queue_buffer_mp(cio2_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_DMABUF, i, {&cio2_output_buffers[i]});
        }
    });

    app.run();

    finish_camera_thread = true;
    camera_thread.join();

    return 0;
}
