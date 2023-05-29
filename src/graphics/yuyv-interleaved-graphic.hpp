#pragma once
#include <gawl/graphic-base.hpp>
#include <gawl/pixelbuffer.hpp>

using PixelBuffer     = gawl::PixelBuffer;
using GraphicGLObject = gawl::internal::GraphicGLObject;

extern const char*      yuyv_interleaved_graphic_fragment_shader_source;
inline GraphicGLObject* yuyv_interleaved_graphic_globject;

inline auto init_yuyv_interleaved_graphic_globject() -> void {
    yuyv_interleaved_graphic_globject = new GraphicGLObject(gawl::internal::graphic_vertex_shader_source, yuyv_interleaved_graphic_fragment_shader_source);
}

class YUYVInterleavedGraphic : public gawl::internal::GraphicBase<GraphicGLObject> {
  public:
    auto update_texture(const PixelBuffer& buffer) -> void {
        const auto txbinder = this->bind_texture();
        this->width         = buffer.get_width();
        this->height        = buffer.get_height();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, this->width);
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RG, this->width, this->height, 0, GL_RG, GL_UNSIGNED_BYTE, buffer.get_buffer());
    }

    YUYVInterleavedGraphic(const PixelBuffer& buffer)
        : GraphicBase<GraphicGLObject>(*yuyv_interleaved_graphic_globject) {
        update_texture(buffer);
    }
};
