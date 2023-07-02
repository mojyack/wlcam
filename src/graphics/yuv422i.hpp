// YUV422 Interleaved aka YUY2
// YUYV YUYV....

#pragma once
#include <gawl/graphic-base.hpp>

extern const char*                      yuv422i_fragment_shader_source;
inline gawl::internal::GraphicGLObject* yuv422i_globject;

inline auto init_yuv422i_graphic_globject() -> void {
    yuv422i_globject = new gawl::internal::GraphicGLObject(gawl::internal::graphic_vertex_shader_source, yuv422i_fragment_shader_source);
}

class YUV422iGraphic : public gawl::internal::GraphicBase<gawl::internal::GraphicGLObject> {
  public:
    auto update_texture(const int width, const int height, const int stride, const std::byte* const yuv) -> void {
        const auto txbinder = this->bind_texture();
        this->width         = width;
        this->height        = height;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, stride == 0 ? width : stride / 2);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, this->width, this->height, 0, GL_RG, GL_UNSIGNED_BYTE, yuv);
    }

    YUV422iGraphic(const int width, const int height, const int stride, const std::byte* const yuv)
        : GraphicBase<gawl::internal::GraphicGLObject>(*yuv422i_globject) {
        update_texture(width, height, stride, yuv);
    }
};
