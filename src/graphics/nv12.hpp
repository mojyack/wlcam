#pragma once
#include <gawl/graphic-base.hpp>
#include <gawl/pixelbuffer.hpp>

#include "multitex.hpp"

extern const char*                      nv12_fragment_shader_source;
inline gawl::internal::GraphicGLObject* nv12_globject;

inline auto init_nv12_graphic_globject() -> void {
    nv12_globject = new gawl::internal::GraphicGLObject(gawl::internal::graphic_vertex_shader_source, nv12_fragment_shader_source);
}

class NV12Graphic : public MultiTexGraphicBase<2, GL_TEXTURE_2D> {
  public:
    auto update_texture(const int width, const int height, const std::byte* const y, const std::byte* const uv) -> void {
        {
            glActiveTexture(GL_TEXTURE1);
            const auto txbinder_uv = this->bind_texture(1);
            MultiTexGraphicBase::update_texture(width / 2, height / 2, uv, GL_RG);
        }
        {
            glActiveTexture(GL_TEXTURE0);
            const auto txbinder_y = this->bind_texture(0);
            MultiTexGraphicBase::update_texture(width, height, y);
        }
    }

    auto operator=(NV12Graphic&& o) -> NV12Graphic& {
        MultiTexGraphicBase::operator=(std::move(o));
        return *this;
    }

    NV12Graphic(const int width, const int height, const std::byte* const y, const std::byte* const uv)
        : MultiTexGraphicBase(*nv12_globject) {
        update_texture(width, height, y, uv);
    }

    NV12Graphic(NV12Graphic&& o)
        : MultiTexGraphicBase(std::move(o)) {
    }
};
