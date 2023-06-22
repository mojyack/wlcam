// generic planar
// YYYY.... U(U...).... V(V...)....

#pragma once
#include "multitex.hpp"

extern const char*                      planar_fragment_shader_source;
inline gawl::internal::GraphicGLObject* planar_globject;

inline auto init_planar_graphic_globject() -> void {
    planar_globject = new gawl::internal::GraphicGLObject(gawl::internal::graphic_vertex_shader_source, planar_fragment_shader_source);
}

enum class PlanarType {
    P444,
    P422,
    P420,
};

class PlanarGraphic : public MultiTex<3> {
  public:
    auto update_texture(const int width, const int height, const int stride, const PlanarType planar_type, const std::byte* const y, const std::byte* const u, const std::byte* const v) -> void {
        auto ppc_x = int(); // pixel per chrominance
        auto ppc_y = int();
        switch(planar_type) {
        case PlanarType::P444:
            ppc_x = 1;
            ppc_y = 1;
            break;
        case PlanarType::P422:
            ppc_x = 2;
            ppc_y = 1;
            break;
        case PlanarType::P420:
            ppc_x = 2;
            ppc_y = 2;
            break;
        }
        {
            glActiveTexture(GL_TEXTURE2);
            const auto txbinder_v = this->bind_texture(2);
            MultiTex::update_texture(width / ppc_x, height / ppc_y, stride / ppc_x, v);
        }
        {
            glActiveTexture(GL_TEXTURE1);
            const auto txbinder_u = this->bind_texture(1);
            MultiTex::update_texture(width / ppc_x, height / ppc_y, stride / ppc_x, u);
        }
        {
            glActiveTexture(GL_TEXTURE0);
            const auto txbinder_y = this->bind_texture(0);
            MultiTex::update_texture(width, height, stride, y);
        }
    }

    auto operator=(PlanarGraphic&& o) -> PlanarGraphic& {
        MultiTex::operator=(std::move(o));
        return *this;
    }

    PlanarGraphic(const int width, const int height, const int stride, const PlanarType planar_type, const std::byte* const y, const std::byte* const u, const std::byte* const v)
        : MultiTex(*planar_globject) {
        update_texture(width, height, stride, planar_type, y, u, v);
    }

    PlanarGraphic(PlanarGraphic&& o)
        : MultiTex(std::move(o)) {
    }
};
