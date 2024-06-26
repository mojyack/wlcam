#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "macros/unwrap.hpp"
#include "util/assert.hpp"
#include "v4l2.hpp"

// #include "ioctl-debug.hpp"

namespace {
template <class... Args>
auto xioctl(const int fd, const uint64_t request, Args&&... args) -> int {
    auto r = 0;
    do {
        // r = iocd::ioctl(stdout, fd, request, std::forward<Args>(args)...);
        r = ioctl(fd, request, std::forward<Args>(args)...);
    } while(r == -1 && errno == EINTR);
    return r;
}

auto export_dmabuf(const int fd, const uint32_t index, const uint32_t plane, const v4l2_buf_type type) -> std::optional<int> {
    auto expbuf  = v4l2_exportbuffer();
    expbuf.type  = type;
    expbuf.index = index;
    expbuf.plane = plane;
    expbuf.flags = O_CLOEXEC | O_RDWR;

    assert_b(xioctl(fd, VIDIOC_EXPBUF, &expbuf) == 0);

    return expbuf.fd;
}
} // namespace

namespace v4l2 {
auto is_capture_device(const int fd) -> std::optional<bool> {
    auto cap = v4l2_capability();
    assert_o(xioctl(fd, VIDIOC_QUERYCAP, &cap) != -1);
    return cap.capabilities & V4L2_CAP_VIDEO_CAPTURE && cap.capabilities & V4L2_CAP_STREAMING;
}

auto list_intervals(const int fd, const uint32_t pixelformat, const uint32_t width, const uint32_t height) -> void {
    auto i   = 0;
    auto fmt = v4l2_frmivalenum{};

    fmt.index        = i;
    fmt.pixel_format = pixelformat;
    fmt.width        = width;
    fmt.height       = height;

    while(xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &fmt) != -1) {
        if(fmt.type != V4L2_FRMIVAL_TYPE_DISCRETE) {
            return;
        }
        auto& disc = fmt.discrete;
        printf("    %u/%u\n", disc.numerator, disc.denominator);
        i += 1;
        fmt.index        = i;
        fmt.pixel_format = pixelformat;
        fmt.width        = width;
        fmt.height       = height;
    }
}

auto list_framesizes(const int fd, const uint32_t pixelformat) -> void {
    auto i   = 0;
    auto fmt = v4l2_frmsizeenum{};

    fmt.index        = i;
    fmt.pixel_format = pixelformat;

    while(xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fmt) != -1) {
        if(fmt.type != V4L2_FRMSIZE_TYPE_DISCRETE) {
            return;
        }
        auto& disc = fmt.discrete;
        printf("  %ux%u\n", disc.width, disc.height);
        list_intervals(fd, pixelformat, disc.width, disc.height);
        i += 1;
        fmt              = v4l2_frmsizeenum{};
        fmt.index        = i;
        fmt.pixel_format = pixelformat;
    }
}

auto list_formats(const int fd, const v4l2_buf_type buffer_type, const uint32_t mbus_code) -> void {
    auto i   = 0;
    auto fmt = v4l2_fmtdesc{};

    fmt.index     = i;
    fmt.type      = buffer_type;
    fmt.mbus_code = mbus_code;

    while(xioctl(fd, VIDIOC_ENUM_FMT, &fmt) != -1) {
        printf("%i: %c%c%c%c (%s)\n", fmt.index,
               fmt.pixelformat >> 0, fmt.pixelformat >> 8,
               fmt.pixelformat >> 16, fmt.pixelformat >> 24, fmt.description);
        list_framesizes(fd, fmt.pixelformat);
        i += 1;
        fmt       = v4l2_fmtdesc{};
        fmt.index = i;
        fmt.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }
}

auto get_current_format(const int fd) -> std::optional<v4l2_pix_format> {
    auto fmt = v4l2_format{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    assert_o(xioctl(fd, VIDIOC_G_FMT, &fmt) != -1, "get_format failed: ", errno);
    return fmt.fmt.pix;
}

auto set_format(const int fd, const uint32_t pixelformat, const uint32_t width, const uint32_t height) -> std::optional<v4l2_pix_format> {
    auto fmt                = v4l2_format{};
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    fmt.fmt.pix.pixelformat = pixelformat;
    fmt.fmt.pix.field       = V4L2_FIELD_ANY;

    assert_o(xioctl(fd, VIDIOC_S_FMT, &fmt) != -1, "set_format failed: ", errno);
    return fmt.fmt.pix;
}

auto set_format_mp(const int fd, const v4l2_buf_type buffer_type, const uint32_t pixelformat, const uint32_t planes_count, const uint32_t width, const uint32_t height, const v4l2_plane_pix_format* const plane_fmts) -> std::optional<v4l2_format> {
    auto  fmt = v4l2_format();
    auto& pix = fmt.fmt.pix_mp;

    fmt.type        = buffer_type;
    pix.width       = width;
    pix.height      = height;
    pix.pixelformat = pixelformat;
    pix.num_planes  = planes_count;
    pix.field       = V4L2_FIELD_NONE;
    if(plane_fmts != nullptr) {
        for(auto i = 0u; i < planes_count; i += 1) {
            pix.plane_fmt[i] = plane_fmts[i];
        }
    }

    assert_o(xioctl(fd, VIDIOC_S_FMT, &fmt) != -1, "set_format failed: ", errno);
    return fmt;
}

auto set_format_subdev(const int fd, const uint32_t pad_index, const uint32_t mbus_code, const uint32_t width, const uint32_t height) -> bool {
    auto fmt = v4l2_subdev_format();

    fmt.which         = V4L2_SUBDEV_FORMAT_ACTIVE;
    fmt.pad           = pad_index;
    fmt.format.width  = width;
    fmt.format.height = height;
    fmt.format.code   = mbus_code;
    fmt.format.field  = V4L2_FIELD_NONE;

    return xioctl(fd, VIDIOC_SUBDEV_S_FMT, &fmt) == 0;
}

auto set_interval(const int fd, const uint32_t numerator, const uint32_t denominator) -> bool {
    auto cprm         = v4l2_captureparm{};
    cprm.timeperframe = {.numerator = numerator, .denominator = denominator};

    auto prm         = v4l2_streamparm{};
    prm.type         = v4l2_buf_type::V4L2_BUF_TYPE_VIDEO_CAPTURE;
    prm.parm.capture = cprm;

    return xioctl(fd, VIDIOC_S_PARM, &prm) != -1;
}

auto request_buffers(const int fd, const v4l2_buf_type buffer_type, const v4l2_memory memory_type, const uint32_t count) -> std::optional<v4l2_requestbuffers> {
    auto req   = v4l2_requestbuffers();
    req.count  = count;
    req.type   = buffer_type;
    req.memory = memory_type;

    assert_o(xioctl(fd, VIDIOC_REQBUFS, &req) != -1);
    assert_o(req.count >= count);

    return req;
}

auto map_buffers(const int fd, const v4l2_buf_type buffer_type, const v4l2_requestbuffers& req) -> std::optional<std::vector<Buffer>> {
    auto buffers = std::vector<Buffer>(req.count);

    for(auto i = 0u; i < req.count; i += 1) {
        auto buf   = v4l2_buffer{};
        buf.type   = buffer_type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        assert_o(xioctl(fd, VIDIOC_QUERYBUF, &buf) != -1);

        buffers[i].length = buf.length;
        buffers[i].start  = mmap(NULL, buf.length,
                                 PROT_READ | PROT_WRITE,
                                 MAP_SHARED, fd,
                                 buf.m.offset);
    }

    return buffers;
}

auto query_and_export_buffers(const int fd, const v4l2_buf_type buffer_type, const v4l2_requestbuffers& req) -> std::optional<std::vector<DMABuffer>> {
    auto buffers = std::vector<DMABuffer>();

    for(auto i = 0u; i < req.count; i += 1) {
        auto buf = v4l2_buffer();

        buf.type  = buffer_type;
        buf.index = i;
        assert_o(xioctl(fd, VIDIOC_QUERYBUF, &buf) == 0, errno);

        unwrap_oo(dmabuffd, export_dmabuf(fd, i, 0, buffer_type));
        buffers.emplace_back(DMABuffer{FileDescriptor(dmabuffd), buf.length});
    }

    return buffers;
}

auto query_and_export_buffers_mp(const int fd, const v4l2_buf_type buffer_type, const v4l2_requestbuffers& req) -> std::optional<std::vector<DMABuffer>> {
    auto buffers = std::vector<DMABuffer>();

    for(auto i = 0u; i < req.count; i += 1) {
        auto buf    = v4l2_buffer();
        auto planes = std::array<v4l2_plane, VIDEO_MAX_PLANES>();

        buf.type     = buffer_type;
        buf.index    = i;
        buf.length   = planes.size();
        buf.m.planes = planes.data();
        assert_o(xioctl(fd, VIDIOC_QUERYBUF, &buf) == 0, errno);

        for(auto j = 0u; j < buf.length; j += 1) {
            unwrap_oo(dmabuffd, export_dmabuf(fd, i, j, buffer_type));
            buffers.emplace_back(DMABuffer{FileDescriptor(dmabuffd), planes[j].length});
        }
    }

    return buffers;
}

auto queue_buffer(const int fd, const v4l2_buf_type buffer_type, const uint32_t index) -> bool {
    auto buf   = v4l2_buffer();
    buf.type   = buffer_type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index  = index;

    return xioctl(fd, VIDIOC_QBUF, &buf) != -1;
}

auto queue_buffer_mp(const int fd, const v4l2_buf_type buffer_type, const v4l2_memory memory_type, const uint32_t index, const DMABuffer* const dma_buffers, const size_t dma_buffers_size) -> bool {
    auto buf    = v4l2_buffer();
    auto planes = std::array<v4l2_plane, VIDEO_MAX_PLANES>();

    buf.type     = buffer_type;
    buf.memory   = memory_type;
    buf.index    = index;
    buf.length   = dma_buffers_size;
    buf.field    = V4L2_FIELD_NONE;
    buf.m.planes = planes.data();
    for(auto i = 0u; i < dma_buffers_size; i += 1) {
        planes[i].m.fd   = dma_buffers[i].fd.as_handle();
        planes[i].length = dma_buffers[i].length;
    }

    return xioctl(fd, VIDIOC_QBUF, &buf) != -1;
}

auto dequeue_buffer_mp(const int fd, const v4l2_buf_type buffer_type, const v4l2_memory memory_type) -> std::optional<uint32_t> {
    auto buf    = v4l2_buffer();
    auto planes = std::array<v4l2_plane, VIDEO_MAX_PLANES>();

    buf.type     = buffer_type;
    buf.memory   = memory_type;
    buf.length   = planes.size();
    buf.m.planes = planes.data();
    assert_o(xioctl(fd, VIDIOC_DQBUF, &buf) != -1);
    return buf.index;
}

auto dequeue_buffer(const int fd, const v4l2_buf_type buffer_type) -> std::optional<uint32_t> {
    auto buf = v4l2_buffer();

    buf.type   = buffer_type;
    buf.memory = V4L2_MEMORY_MMAP;
    assert_o(xioctl(fd, VIDIOC_DQBUF, &buf) != -1);
    return buf.index;
}

auto start_stream(const int fd, v4l2_buf_type type) -> bool {
    return xioctl(fd, VIDIOC_STREAMON, &type) != -1;
}

auto stop_stream(const int fd) -> bool {
    auto type = v4l2_buf_type(V4L2_BUF_TYPE_VIDEO_CAPTURE);
    return xioctl(fd, VIDIOC_STREAMOFF, &type) != -1;
}

auto query_controls(const int fd) -> std::vector<v4l2_queryctrl> {
    auto query = v4l2_queryctrl();
    query.id   = V4L2_CTRL_CLASS_USER | V4L2_CTRL_FLAG_NEXT_CTRL;
    while(0 == ioctl(fd, VIDIOC_QUERYCTRL, &query)) {
        if(V4L2_CTRL_ID2CLASS(query.id) != V4L2_CTRL_CLASS_USER)
            break;
        if(query.flags & V4L2_CTRL_FLAG_DISABLED)
            continue;

        query.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    auto ret = std::vector<v4l2_queryctrl>();
    for(auto id = V4L2_CID_BASE + 1; id < V4L2_CID_LASTP1; id += 1) {
        query.id = id;
        if(xioctl(fd, VIDIOC_QUERYCTRL, &query) == 0) {
            ret.emplace_back(query);
        }
    }
    return ret;
}

auto set_control(const int fd, const uint32_t cid, const int32_t value) -> bool {
    auto ctrl = v4l2_control{cid, value};
    return xioctl(fd, VIDIOC_S_CTRL, &ctrl) != -1;
}

auto set_selection_subdev(const int fd, const uint32_t pad_index, const uint32_t target, const int32_t x, const int32_t y, const uint32_t w, const uint32_t h) -> bool {
    auto sel = v4l2_subdev_selection();

    sel.which  = V4L2_SUBDEV_FORMAT_ACTIVE;
    sel.pad    = pad_index;
    sel.target = target;
    sel.flags  = 0;
    sel.r      = v4l2_rect{x, y, w, h};

    return xioctl(fd, VIDIOC_SUBDEV_S_SELECTION, &sel) == 0;
}
} // namespace v4l2
