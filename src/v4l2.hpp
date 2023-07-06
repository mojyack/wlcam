#pragma once
#include <vector>

#include <fcntl.h>
#include <linux/v4l2-subdev.h>
#include <linux/videodev2.h>
#include <sys/mman.h>

#include "fd.hpp"

namespace v4l2 {
constexpr auto fourcc(const char c[5]) -> uint32_t {
    return v4l2_fourcc(c[0], c[1], c[2], c[3]);
}

struct Buffer {
    void*  start = nullptr;
    size_t length;

    Buffer() = default;

    Buffer(Buffer&& other) {
        std::swap(start, other.start);
    }

    ~Buffer() {
        if(start != nullptr) {
            munmap(start, length);
        }
    }
};

struct DMABuffer {
    FileDescriptor fd;
    size_t         length;
};

auto is_capture_device(int fd) -> bool;

auto list_intervals(int fd, uint32_t pixelformat, uint32_t width, uint32_t height) -> void;

auto list_framesizes(int fd, uint32_t pixelformat) -> void;

auto list_formats(int fd, uint32_t buffer_type, uint32_t mbus_code) -> void;

auto list_formats(const int fd, const v4l2_buf_type buffer_type, const uint32_t mbus_code) -> void;

auto get_current_format(int fd) -> v4l2_pix_format;

auto set_format(int fd, uint32_t pixelformat, uint32_t width, uint32_t height) -> v4l2_pix_format;

auto set_format_mp(int fd, v4l2_buf_type buffer_type, uint32_t pixelformat, uint32_t planes_count, uint32_t width, uint32_t height, const v4l2_plane_pix_format* plane_fmts) -> v4l2_format;

auto set_format_subdev(int fd, uint32_t pad_index, uint32_t mbus_code, uint32_t width, uint32_t height) -> void;

auto set_interval(int fd, uint32_t numerator, uint32_t denominator) -> void;

auto request_buffers(int fd, v4l2_buf_type buffer_type, v4l2_memory memory_type, uint32_t count) -> v4l2_requestbuffers;

auto map_buffers(int fd, v4l2_buf_type type, const v4l2_requestbuffers& req) -> std::vector<Buffer>;

auto query_and_export_buffers(int fd, v4l2_buf_type type, const v4l2_requestbuffers& req) -> std::vector<DMABuffer>;

auto query_and_export_buffers_mp(int fd, v4l2_buf_type type, const v4l2_requestbuffers& req) -> std::vector<DMABuffer>;

auto queue_buffer(int fd, v4l2_buf_type buffer_type, uint32_t index) -> void;

auto queue_buffer_mp(int fd, v4l2_buf_type buffer_type, v4l2_memory memory_type, uint32_t index, const DMABuffer* dma_buffers, size_t dma_buffers_size) -> void;

auto dequeue_buffer(int fd, v4l2_buf_type buffer_type) -> uint32_t;

auto dequeue_buffer_mp(int fd, v4l2_buf_type buffer_type, v4l2_memory memory_type) -> uint32_t;

auto start_stream(int fd, v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE) -> void;

auto stop_stream(int fd) -> void;

auto query_controls(const int fd) -> std::vector<v4l2_queryctrl>;

auto set_control(const int fd, const uint32_t cid, const int32_t value) -> void;

auto set_selection_subdev(int fd, uint32_t pad_index, uint32_t target, int32_t x, int32_t y, uint32_t w, uint32_t h) -> void;
} // namespace v4l2
