#pragma once
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>

#include <vector>

namespace v4l2 {
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

auto is_capture_device(int fd) -> bool;

auto list_intervals(int fd, uint32_t pixelformat, uint32_t width, uint32_t height) -> void;

auto list_framesizes(int fd, uint32_t pixelformat) -> void;

auto list_formats(int fd) -> void;

auto ensure_format(int fd, uint32_t pixelformat, uint32_t width, uint32_t height) -> bool;

auto get_current_format(int fd, uint32_t& width, uint32_t& height) -> void;

auto set_format(int fd, uint32_t pixelformat, uint32_t width, uint32_t height) -> void;

auto set_interval(int fd, uint32_t numerator, uint32_t denominator) -> void;

auto request_buffers(int fd, uint32_t count) -> v4l2_requestbuffers;

auto map_buffers(int fd, const v4l2_requestbuffers& req) -> std::vector<Buffer>;

auto queue_buffer(int fd, uint32_t index) -> void;

auto dequeue_buffer(int fd) -> uint32_t;

auto start_stream(int fd) -> void;

auto stop_stream(int fd) -> void;

} // namespace v4l2
