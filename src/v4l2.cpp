#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "assert.hpp"
#include "v4l2.hpp"

namespace v4l2 {
template <class... Args>
auto xioctl(const int fd, const int request, Args&&... args) -> int {
    int r;
    do {
        r = ioctl(fd, request, std::forward<Args>(args)...);
    } while(r == -1 && errno == EINTR);
    return r;
}

auto is_capture_device(const int fd) -> bool {
    auto cap = v4l2_capability();
    DYN_ASSERT(xioctl(fd, VIDIOC_QUERYCAP, &cap) != -1);
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

auto list_formats(const int fd) -> void {
    auto i   = 0;
    auto fmt = v4l2_fmtdesc{};

    fmt.index = i;
    fmt.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;

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

auto ensure_format(const int fd, const uint32_t pixelformat, const uint32_t width, const uint32_t height) -> bool {
    auto fmt = v4l2_format{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    DYN_ASSERT(xioctl(fd, VIDIOC_G_FMT, &fmt) != -1);

    return fmt.fmt.pix.width == width && fmt.fmt.pix.height == height && fmt.fmt.pix.pixelformat == pixelformat;
}

auto get_current_format(const int fd, uint32_t& width, uint32_t& height) -> void {
    auto fmt = v4l2_format{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    DYN_ASSERT(xioctl(fd, VIDIOC_G_FMT, &fmt) != -1);
    width  = fmt.fmt.pix.width;
    height = fmt.fmt.pix.height;
}

auto set_format(const int fd, const uint32_t pixelformat, const uint32_t width, const uint32_t height) -> void {
    auto fmt                = v4l2_format{};
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    fmt.fmt.pix.pixelformat = pixelformat;
    fmt.fmt.pix.field       = V4L2_FIELD_ANY;

    DYN_ASSERT(xioctl(fd, VIDIOC_S_FMT, &fmt) != -1);

    if(!ensure_format(fd, pixelformat, width, height)) {
        warn("format no set properly");
    }
}

auto set_interval(const int fd, const uint32_t numerator, const uint32_t denominator) -> void {
    auto cprm         = v4l2_captureparm{};
    cprm.timeperframe = {.numerator = numerator, .denominator = denominator};

    auto prm         = v4l2_streamparm{};
    prm.type         = v4l2_buf_type::V4L2_BUF_TYPE_VIDEO_CAPTURE;
    prm.parm.capture = cprm;
    DYN_ASSERT(xioctl(fd, VIDIOC_S_PARM, &prm) != -1);
}

auto request_buffers(const int fd, const uint32_t count) -> v4l2_requestbuffers {
    auto req   = v4l2_requestbuffers{};
    req.count  = count;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    DYN_ASSERT(xioctl(fd, VIDIOC_REQBUFS, &req) != -1);
    DYN_ASSERT(req.count >= count);

    return req;
}

auto map_buffers(const int fd, const v4l2_requestbuffers& req) -> std::vector<Buffer> {
    auto buffers = std::vector<Buffer>(req.count);

    for(auto i = 0u; i < req.count; i += 1) {
        auto buf   = v4l2_buffer{};
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        DYN_ASSERT(xioctl(fd, VIDIOC_QUERYBUF, &buf) != -1);

        buffers[i].length = buf.length;
        buffers[i].start  = mmap(NULL, buf.length,
                                 PROT_READ | PROT_WRITE,
                                 MAP_SHARED, fd,
                                 buf.m.offset);
    }

    return buffers;
}

auto queue_buffer(const int fd, const uint32_t index) -> void {
    struct v4l2_buffer buf = {};
    buf.type               = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory             = V4L2_MEMORY_MMAP;
    buf.index              = index;

    DYN_ASSERT(xioctl(fd, VIDIOC_QBUF, &buf) != -1);
}

auto dequeue_buffer(const int fd) -> uint32_t {
    struct v4l2_buffer buf = {};
    buf.type               = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory             = V4L2_MEMORY_MMAP;

    DYN_ASSERT(xioctl(fd, VIDIOC_DQBUF, &buf) != -1);

    return buf.index;
}

auto start_stream(const int fd) -> void {
    auto type = v4l2_buf_type(V4L2_BUF_TYPE_VIDEO_CAPTURE);
    DYN_ASSERT(xioctl(fd, VIDIOC_STREAMON, &type) != -1);
}

auto stop_stream(const int fd) -> void {
    auto type = v4l2_buf_type(V4L2_BUF_TYPE_VIDEO_CAPTURE);
    DYN_ASSERT(xioctl(fd, VIDIOC_STREAMOFF, &type) != -1);
}
} // namespace v4l2
