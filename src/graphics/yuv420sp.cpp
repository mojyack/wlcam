#include "yuv420sp.hpp"

namespace {
auto yuv420sp_fragment_shader_source = R"glsl(
    #version 330 core
    in vec2           tex_coordinate;
    uniform sampler2D tex_0; // y
    uniform sampler2D tex_1; // uv
    out vec4          color;
    void main() {
        float y = texture(tex_0, tex_coordinate).r;
        float u = texture(tex_1, tex_coordinate).r - 0.5;
        float v = texture(tex_1, tex_coordinate).g - 0.5;

        // if(tex_coordinate.y < 0.5) {
        //     float e;
        //     if(tex_coordinate.x < 1.0 / 3) {
        //         e = u;
        //     } else if(tex_coordinate.x < 2.0 / 3) {
        //         e = y;
        //     } else {
        //         e = v;
        //     }
        //     color = vec4(e, e, e, 1);
        //     return;
        // }

        float r = y + 1.40200 * v;
        float g = y - 0.34414 * u - 0.71414 * v;
        float b = y + 1.77200 * u;
        color = vec4(r, g, b, 1.0);
    }
)glsl";

auto shader = (gawl::impl::GraphicShader*)(nullptr);
} // namespace

auto init_yuv420sp_shader() -> void {
    shader = new gawl::impl::GraphicShader(gawl::impl::graphic_vertex_shader_source, yuv420sp_fragment_shader_source);
}

auto YUV420spGraphic::update_texture(const int width, const int height, const int stride, const std::byte* const y, const std::byte* const uv) -> void {
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

YUV420spGraphic::YUV420spGraphic(const int width, const int height, const int stride, const std::byte* const y, const std::byte* const uv)
    : MultiTex(*::shader, 2) {
    update_texture(width, height, stride, y, uv);
}
