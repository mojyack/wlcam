#pragma once
#include <vector>

#include <fcntl.h>
#include <linux/v4l2-subdev.h>
#include <linux/videodev2.h>
#include <sys/mman.h>

#include "util/fd.hpp"

namespace v4l2 {
constexpr auto fourcc(const char c[5]) -> uint32_t {
    return v4l2_fourcc(c[0], c[1], c[2], c[3]);
}

struct Buffer {
    void*  start = nullptr;
    size_t length;

    Buffer(){};

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

auto is_capture_device(int fd) -> std::optional<bool>;
auto list_intervals(int fd, uint32_t pixelformat, uint32_t width, uint32_t height) -> void;
auto list_framesizes(int fd, uint32_t pixelformat) -> void;
auto list_formats(int fd, uint32_t buffer_type, uint32_t mbus_code) -> void;
auto list_formats(int fd, v4l2_buf_type buffer_type, uint32_t mbus_code) -> void;
auto get_current_format(int fd) -> std::optional<v4l2_pix_format>;
auto set_format(int fd, uint32_t pixelformat, uint32_t width, uint32_t height) -> std::optional<v4l2_pix_format>;
auto set_format_mp(int fd, v4l2_buf_type buffer_type, uint32_t pixelformat, uint32_t planes_count, uint32_t width, uint32_t height, const v4l2_plane_pix_format* plane_fmts) -> std::optional<v4l2_format>;
auto set_format_subdev(int fd, uint32_t pad_index, uint32_t mbus_code, uint32_t width, uint32_t height) -> bool;
auto set_interval(int fd, uint32_t numerator, uint32_t denominator) -> bool;
auto request_buffers(int fd, v4l2_buf_type buffer_type, v4l2_memory memory_type, uint32_t count) -> std::optional<v4l2_requestbuffers>;
auto map_buffers(int fd, v4l2_buf_type type, const v4l2_requestbuffers& req) -> std::optional<std::vector<Buffer>>;
auto query_and_export_buffers(int fd, v4l2_buf_type type, const v4l2_requestbuffers& req) -> std::optional<std::vector<DMABuffer>>;
auto query_and_export_buffers_mp(int fd, v4l2_buf_type type, const v4l2_requestbuffers& req) -> std::optional<std::vector<DMABuffer>>;
auto queue_buffer(int fd, v4l2_buf_type buffer_type, uint32_t index) -> bool;
auto queue_buffer_mp(int fd, v4l2_buf_type buffer_type, v4l2_memory memory_type, uint32_t index, const DMABuffer* dma_buffers, size_t dma_buffers_size) -> bool;
auto dequeue_buffer(int fd, v4l2_buf_type buffer_type) -> std::optional<uint32_t>;
auto dequeue_buffer_mp(int fd, v4l2_buf_type buffer_type, v4l2_memory memory_type) -> std::optional<uint32_t>;
auto start_stream(int fd, v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE) -> bool;
auto stop_stream(int fd) -> bool;
auto query_controls(int fd) -> std::vector<v4l2_queryctrl>;
auto set_control(int fd, uint32_t cid, int32_t value) -> bool;
auto set_selection_subdev(int fd, uint32_t pad_index, uint32_t target, int32_t x, int32_t y, uint32_t w, uint32_t h) -> bool;
} // namespace v4l2
