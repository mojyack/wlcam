#pragma once
#include <gawl/pixelbuffer.hpp>

namespace yuv {
auto split_yuv422_interleaved_planar(const std::byte* ptr, uint32_t width, uint32_t height) -> std::array<gawl::PixelBuffer, 3>;
auto yuv420sp_uvsp_to_uvp(const std::byte* uv, std::byte* u, std::byte* v, uint32_t width, uint32_t height, uint32_t stride) -> void;
} // namespace yuv
