#pragma once
#include <array>
#include <vector>

namespace yuv {
auto yuv422i_to_yuv422p(const std::byte* const yuv, const uint32_t width, const uint32_t height, const uint32_t stride) -> std::array<std::vector<std::byte>, 3>;
auto yuv420sp_uvsp_to_uvp(const std::byte* uv, std::byte* u, std::byte* v, uint32_t width, uint32_t height, uint32_t stride) -> void;
} // namespace yuv
