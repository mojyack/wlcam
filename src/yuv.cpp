#include "yuv.hpp"
#include "assert.hpp"

namespace yuv {
auto yuv422i_to_yuv422p(const std::byte* const yuv, const uint32_t width, const uint32_t height, const uint32_t stride) -> std::array<std::vector<std::byte>, 3> {
    auto buf_y = std::vector<std::byte>(width * height);
    auto buf_u = std::vector<std::byte>(width * height / 2);
    auto buf_v = std::vector<std::byte>(width * height / 2);

    for(auto r = 0u; r < height; r += 1) {
        for(auto c = 0u; c < width / 2; c += 1) {
            buf_y[r * width + c * 2 + 0] = yuv[r * stride + c * 4 + 0];
            buf_u[r * width / 2 + c]     = yuv[r * stride + c * 4 + 1];
            buf_y[r * width + c * 2 + 1] = yuv[r * stride + c * 4 + 2];
            buf_v[r * width / 2 + c]     = yuv[r * stride + c * 4 + 3];
        }
    }

    return {std::move(buf_y), std::move(buf_u), std::move(buf_v)};
}

auto yuv420sp_uvsp_to_uvp(const std::byte* const uv, std::byte* const u, std::byte* const v, const uint32_t width, const uint32_t height, const uint32_t stride) -> void {
    for(auto r = 0u; r < height / 2; r += 1) {
        for(auto c = 0u; c < width / 2; c += 1) {
            u[r * width / 2 + c] = uv[r * stride + c * 2 + 0];
            v[r * width / 2 + c] = uv[r * stride + c * 2 + 1];
        }
    }
}
} // namespace yuv
