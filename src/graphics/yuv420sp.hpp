// YUV420 Semi-planar aka NV12
// YYYY.... UV....

#pragma once
#include "multitex.hpp"

extern const char*                      yuv420sp_fragment_shader_source;
inline gawl::internal::GraphicGLObject* yuv420sp_globject;

inline auto init_yuv420sp_graphic_globject() -> void {
    yuv420sp_globject = new gawl::internal::GraphicGLObject(gawl::internal::graphic_vertex_shader_source, yuv420sp_fragment_shader_source);
}

class YUV420spGraphic : public MultiTex<2, GL_TEXTURE_2D> {
  public:
    auto update_texture(const int width, const int height, const int stride, const std::byte* const y, const std::byte* const uv) -> void {
        {
            glActiveTexture(GL_TEXTURE1);
            const auto txbinder_uv = this->bind_texture(1);
            MultiTex::update_texture(width / 2, height / 2, stride / 2, uv, GL_RG);
        }
        {
            glActiveTexture(GL_TEXTURE0);
            const auto txbinder_y = this->bind_texture(0);
            MultiTex::update_texture(width, height, stride, y);
        }
    }

    auto operator=(YUV420spGraphic&& o) -> YUV420spGraphic& {
        MultiTex::operator=(std::move(o));
        return *this;
    }

    YUV420spGraphic(const int width, const int height, const int stride, const std::byte* const y, const std::byte* const uv)
        : MultiTex(*yuv420sp_globject) {
        update_texture(width, height, stride, y, uv);
    }

    YUV420spGraphic(YUV420spGraphic&& o)
        : MultiTex(std::move(o)) {
    }
};
