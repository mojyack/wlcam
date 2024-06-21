// YUV420 Semi-planar aka NV12
// YYYY.... UV....

#pragma once
#include "multitex.hpp"

auto init_yuv420sp_shader() -> void;

class YUV420spGraphic : public MultiTex {
  public:
    auto update_texture(int width, int height, int stride, const std::byte* y, const std::byte* uv) -> void;

    YUV420spGraphic();
};

