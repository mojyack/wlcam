// YUV422 Interleaved aka YUY2
// YUYV YUYV....

#pragma once
#include "../gawl/graphic-base.hpp"

auto init_yuv422i_shader() -> void;

class YUV422iGraphic : public gawl::impl::GraphicBase {
  public:
    auto update_texture(int width, int height, int stride, const std::byte* yuv) -> void;

    YUV422iGraphic();
};
