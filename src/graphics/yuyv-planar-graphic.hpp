#pragma once
#include "yuv-planar-graphic-base.hpp"

using PixelBuffer = gawl::PixelBuffer;

extern const char*      yuyv_planar_graphic_fragment_shader_source;
inline GraphicGLObject* yuyv_planar_graphic_globject;

inline auto init_yuyv_planar_graphic_globject() -> void {
    yuyv_planar_graphic_globject = new GraphicGLObject(gawl::internal::graphic_vertex_shader_source, yuyv_planar_graphic_fragment_shader_source);
}

class YUYVPlanarGraphic : public YUVPlanarGraphicBase {
  public:
    // activate texture unit and bind texture before call this
    auto update_texture(const PixelBuffer& buffer) -> void {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, buffer.get_width());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        this->width  = buffer.get_width();
        this->height = buffer.get_height();

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, this->width, this->height, 0, GL_RED, GL_UNSIGNED_BYTE, buffer.get_buffer());
    }

    auto update_texture(const PixelBuffer& buffer_y, const PixelBuffer& buffer_u, const PixelBuffer& buffer_v) -> void {
        {
            glActiveTexture(GL_TEXTURE2);
            const auto txbinder_v = this->bind_texture(2);
            update_texture(buffer_v);
        }
        {
            glActiveTexture(GL_TEXTURE1);
            const auto txbinder_u = this->bind_texture(1);
            update_texture(buffer_u);
        }
        {
            glActiveTexture(GL_TEXTURE0);
            const auto txbinder_y = this->bind_texture(0);
            update_texture(buffer_y);
        }
    }

    auto operator=(YUYVPlanarGraphic&& o) -> YUYVPlanarGraphic& {
        YUVPlanarGraphicBase::operator=(std::move(o));
        return *this;
    }

    YUYVPlanarGraphic(const PixelBuffer& buffer_y, const PixelBuffer& buffer_u, const PixelBuffer& buffer_v)
        : YUVPlanarGraphicBase(*yuyv_planar_graphic_globject) {
        update_texture(buffer_y, buffer_u, buffer_v);
    }

    YUYVPlanarGraphic(YUYVPlanarGraphic&& o)
        : YUVPlanarGraphicBase(std::move(o)) {
    }
};
