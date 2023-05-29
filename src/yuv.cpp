#include <gawl/pixelbuffer.hpp>

#include "assert.hpp"

auto split_yuv422_interleaved_planar(const std::byte* const ptr, const uint32_t width, const uint32_t height) -> std::array<gawl::PixelBuffer, 3> {
    auto buf_y = std::vector<std::byte>(width * height);
    auto buf_u = std::vector<std::byte>(width * height / 2);
    auto buf_v = std::vector<std::byte>(width * height / 2);
    for(auto i = size_t(0); i < width * height * 2; i += 4) {
        buf_y[i / 2]     = ptr[i];
        buf_u[i / 4]     = ptr[i + 1];
        buf_y[i / 2 + 1] = ptr[i + 2];
        buf_v[i / 4]     = ptr[i + 3];
    }

    return {
        gawl::PixelBuffer::from_raw(width, height, std::move(buf_y)),
        gawl::PixelBuffer::from_raw(width / 2, height, std::move(buf_u)),
        gawl::PixelBuffer::from_raw(width / 2, height, std::move(buf_v)),
    };
}
