#include "planar.hpp"

namespace {
auto planar_fragment_shader_source = R"glsl(
    #version 330 core
    in vec2           tex_coordinate;
    uniform sampler2D tex_0;
    uniform sampler2D tex_1;
    uniform sampler2D tex_2;
    out vec4          color;
    void main() {
        float y = texture(tex_0, tex_coordinate).r;
        float u = texture(tex_1, tex_coordinate).r - 0.5;
        float v = texture(tex_2, tex_coordinate).r - 0.5;
        float r = y + 1.40200 * v;
        float g = y - 0.34414 * u - 0.71414 * v;
        float b = y + 1.77200 * u;
        color = vec4(r, g, b, 1.0);
    }
)glsl";

auto shader = (gawl::impl::GraphicShader*)(nullptr);
} // namespace

auto init_planar_shader() -> void {
    shader = new gawl::impl::GraphicShader(gawl::impl::graphic_vertex_shader_source, planar_fragment_shader_source);
}

auto PlanarGraphic::update_texture(const int width, const int height, const int stride, const int ppc_x, const int ppc_y, const std::byte* const y, const std::byte* const u, const std::byte* const v) -> void {
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

PlanarGraphic::PlanarGraphic()
    : MultiTex(*::shader, 3) {
}
