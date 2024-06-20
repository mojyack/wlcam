#include "yuv422i.hpp"

namespace {
auto yuv422i_fragment_shader_source = R"glsl(
    #version 330 core
    in vec2           tex_coordinate;
    uniform sampler2D tex_0;
    out vec4          color;
    void main() {
        vec2 tex_size = textureSize(tex_0, 0);
        float y = texture(tex_0, tex_coordinate).r;
        float u;
        float v;
        float tex_x = tex_coordinate.x * tex_size.x;
        if(int(tex_x) % 2 == 0) {
            vec2 next = vec2((tex_x + 1) / tex_size.x, tex_coordinate.y);
            u = texture(tex_0, tex_coordinate).g - 0.5;
            v = texture(tex_0, next).g - 0.5;
        } else {
            vec2 prev = vec2((tex_x - 1) / tex_size.x, tex_coordinate.y);
            u = texture(tex_0, prev).g - 0.5;
            v = texture(tex_0, tex_coordinate).g - 0.5;
        }
        float r = y + 1.40200 * v;
        float g = y - 0.34414 * u - 0.71414 * v;
        float b = y + 1.77200 * u;
        color = vec4(r, g, b, 1.0);
    }
)glsl";

auto shader = (gawl::impl::GraphicShader*)(nullptr);
} // namespace

auto init_yuv422i_shader() -> void {
    shader = new gawl::impl::GraphicShader(gawl::impl::graphic_vertex_shader_source, yuv422i_fragment_shader_source);
}

auto YUV422iGraphic::update_texture(const int width, const int height, const int stride, const std::byte* const yuv) -> void {
    const auto txbinder = this->bind_texture();
    this->width         = width;
    this->height        = height;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, stride == 0 ? width : stride / 2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, this->width, this->height, 0, GL_RG, GL_UNSIGNED_BYTE, yuv);
}

YUV422iGraphic::YUV422iGraphic(const int width, const int height, const int stride, const std::byte* const yuv)
    : GraphicBase(*::shader) {
    update_texture(width, height, stride, yuv);
}
