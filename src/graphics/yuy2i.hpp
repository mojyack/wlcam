#pragma once
#include <gawl/graphic-base.hpp>
#include <gawl/pixelbuffer.hpp>

extern const char*                      yuy2i_fragment_shader_source;
inline gawl::internal::GraphicGLObject* yuy2i_globject;

inline auto init_yuyv_interleaved_graphic_globject() -> void {
    yuy2i_globject = new gawl::internal::GraphicGLObject(gawl::internal::graphic_vertex_shader_source, yuy2i_fragment_shader_source);
}

class YUYVInterleavedGraphic : public gawl::internal::GraphicBase<gawl::internal::GraphicGLObject> {
  public:
    auto update_texture(const int width, const int height, const std::byte* const yuv) -> void {
        const auto txbinder = this->bind_texture();
        this->width         = width;
        this->height        = height;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, this->width);
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RG, this->width, this->height, 0, GL_RG, GL_UNSIGNED_BYTE, yuv);
    }

    YUYVInterleavedGraphic(const int width, const int height, const std::byte* const yuv)
        : GraphicBase<gawl::internal::GraphicGLObject>(*yuy2i_globject) {
        update_texture(width, height, yuv);
    }
};
