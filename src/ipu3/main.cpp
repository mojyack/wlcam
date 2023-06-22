#include <thread>

#include <linux/v4l2-subdev.h>

#include "../assert.hpp"
#include "../media-device.hpp"
#include "../udev.hpp"
#include "../v4l2.hpp"
#include "../window.hpp"
#include "args.hpp"
#include "cio2.hpp"
#include "imgu.hpp"

auto main(const int argc, const char* const argv[]) -> int {
    const auto args = ipu3::parse_args(argc, argv);

    const auto node_map    = dev::enumerate();
    auto       cio2_device = parse_device(args.cio2_devnode.c_str(), node_map);
    cio2_device.disable_all_links();

    auto imgu_device = parse_device(args.imgu_devnode.c_str(), node_map);
    imgu_device.disable_all_links();

    auto imgu_0 = ipu3::ImgUDevice();
    imgu_0.init(imgu_device, args.imgu_entity);

    auto cio2_0 = ipu3::cio2::CIO2Device(&cio2_device);
    cio2_0.init(args.cio2_entity);

    constexpr auto imgu_input_format = v4l2_fourcc('i', 'p', '3', 'b');
    constexpr auto num_buffers       = 4;

    v4l2::set_format_subdev(cio2_0.sensor.fd, cio2_0.sensor.pad_index, args.sensor_mbus_code, args.sensor_width, args.sensor_height);
    v4l2::set_format_subdev(cio2_0.cio2, 0, args.sensor_mbus_code, args.sensor_width, args.sensor_height);
    const auto cio_output_fmt = v4l2::set_format_mp(cio2_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, imgu_input_format, 1, args.sensor_width, args.sensor_height, nullptr);

    v4l2::set_format_mp(imgu_0.input, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, imgu_input_format, 1, args.sensor_width, args.sensor_height, cio_output_fmt.fmt.pix_mp.plane_fmt);
    v4l2::set_format_subdev(imgu_0.imgu, imgu_0.imgu_input_pad_index, MEDIA_BUS_FMT_FIXED, args.width, args.height);
    v4l2::set_format_subdev(imgu_0.imgu, imgu_0.imgu_parameters_pad_index, MEDIA_BUS_FMT_FIXED, args.width, args.height);
    v4l2::set_format_subdev(imgu_0.imgu, imgu_0.imgu_stat_pad_index, MEDIA_BUS_FMT_FIXED, args.width, args.height);
    v4l2::set_format_subdev(imgu_0.imgu, imgu_0.imgu_output_pad_index, MEDIA_BUS_FMT_FIXED, args.width, args.height);
    const auto imgu_output_fmt = v4l2::set_format_mp(imgu_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_PIX_FMT_NV12, 2, args.width, args.height, nullptr);
    v4l2::set_format_subdev(imgu_0.imgu, imgu_0.imgu_viewfinder_pad_index, MEDIA_BUS_FMT_FIXED, args.width, args.height);
    v4l2::set_format_mp(imgu_0.viewfinder, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_PIX_FMT_NV12, 2, args.width, args.height, nullptr);

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

    // clear stat buffer in order to unset ipu3_uapi_params.use
    for(auto i = 0u; i < num_buffers; i += 1) {
        const auto addr = mmap(NULL, imgu_stat_buffers[i].length,
                               PROT_READ | PROT_WRITE,
                               MAP_SHARED, imgu_stat_buffers[i].fd, 0);
        DYN_ASSERT(addr != MAP_FAILED, errno);
        memset(addr, 0, imgu_stat_buffers[i].length);
        munmap(addr, imgu_stat_buffers[i].length);
    }

    auto context = Context();
    auto app     = gawl::Application();
    auto window  = &app.open_window<Window>({}, context, gawl::Point{0, 0}, false).get_window();

    init_yuv420sp_graphic_globject();

    auto camera_thread = std::thread([&]() {
        auto       window_context = window->fork_context();
        const auto stride         = imgu_output_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
        while(1) {
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
            auto       img = std::shared_ptr<GraphicLike>(new GraphicLike(Tag<YUV420spGraphic>(), args.width, args.height, stride, buf, buf + stride * args.height));
            window_context.flush();
            std::swap(context.critical_graphic, img);

            v4l2::queue_buffer_mp(cio2_0.output, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_DMABUF, i, {&cio2_output_buffers[i]});
        }
    });

    app.run();

    return 0;
}
