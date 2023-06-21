#pragma once
#include "multitex.hpp"

extern const char*                      yuy2p_fragment_shader_source;
inline gawl::internal::GraphicGLObject* yuy2p_globject;

inline auto init_yuyv_planar_graphic_globject() -> void {
    yuy2p_globject = new gawl::internal::GraphicGLObject(gawl::internal::graphic_vertex_shader_source, yuy2p_fragment_shader_source);
}

class YUYVPlanarGraphic : public MultiTexGraphicBase<3> {
  public:
    auto update_texture(const int width, const int height, const std::byte* const y, const std::byte* const u, const std::byte* const v) -> void {
        {
            glActiveTexture(GL_TEXTURE2);
            const auto txbinder_v = this->bind_texture(2);
            MultiTexGraphicBase::update_texture(width / 2, plane.height, plane.data);
        }
        {
            const auto plane = image.get_plane(1);
            glActiveTexture(GL_TEXTURE1);
            const auto txbinder_u = this->bind_texture(1);
            MultiTexGraphicBase::update_texture(plane.width, plane.height, plane.data);
        }
        {
            const auto plane = image.get_plane(0);
            glActiveTexture(GL_TEXTURE0);
            const auto txbinder_y = this->bind_texture(0);
            MultiTexGraphicBase::update_texture(plane.width, plane.height, plane.data);
        }
    }

    auto operator=(YUYVPlanarGraphic&& o) -> YUYVPlanarGraphic& {
        MultiTexGraphicBase::operator=(std::move(o));
        return *this;
    }

    YUYVPlanarGraphic(Image& image)
        : MultiTexGraphicBase(*yuyv_planar_graphic_globject) {
        update_texture(image);
    }

    YUYVPlanarGraphic(YUYVPlanarGraphic&& o)
        : MultiTexGraphicBase(std::move(o)) {
    }
};
