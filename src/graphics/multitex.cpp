#include "multitex.hpp"
#include "../gawl/misc.hpp"

auto MultiTex::do_draw(gawl::Screen& screen) const -> void {
    const auto vabinder = shader->bind_vao();
    const auto ebbinder = shader->bind_ebo();
    const auto shbinder = shader->use_shader();
    const auto fbbinder = screen.prepare();

    for(auto i = 0u; i < ntex; i += 1) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
    }

    const auto prog = shader->get_shader();
    for(auto i = 0u; i < ntex; i += 1) {
        auto name = std::array{'t', 'e', 'x', '_', char('0' + i), '\0'};
        glUniform1i(glGetUniformLocation(prog, name.data()), i);
    }

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    for(auto i = ntex; i > 0; i -= 1) {
        glActiveTexture(GL_TEXTURE0 + i - 1);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

auto MultiTex::bind_texture(const uint32_t number) const -> gawl::impl::TextureBinder {
    return textures[number];
}

auto MultiTex::release_texture() -> void {
    if(textures[0] != 0) {
        glDeleteTextures(ntex, textures.data());
    }
}

auto MultiTex::get_width(const gawl::MetaScreen& screen) const -> int {
    return width / screen.get_scale();
}

auto MultiTex::get_height(const gawl::MetaScreen& screen) const -> int {
    return height / screen.get_scale();
}

auto MultiTex::update_texture(const int width, const int height, const int stride, const std::byte* const data, const GLuint pixformat) -> void {
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, stride == 0 ? width : stride);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    this->width  = width;
    this->height = height;
    glTexImage2D(GL_TEXTURE_2D, 0, pixformat, width, height, 0, pixformat, GL_UNSIGNED_BYTE, data);
}

auto MultiTex::draw(gawl::Screen& screen, const gawl::Point& point) -> void {
    draw_rect(screen, {{point.x, point.y}, {point.x + width, point.y + height}});
}

auto MultiTex::draw_rect(gawl::Screen& screen, const gawl::Rectangle& rect) -> void {
    shader->move_vertices(screen, gawl::Rectangle(rect).magnify(screen.get_scale()), false);
    do_draw(screen);
}

auto MultiTex::draw_fit_rect(gawl::Screen& screen, const gawl::Rectangle& rect) -> void {
    draw_rect(screen, gawl::calc_fit_rect(rect, width, height));
}

auto MultiTex::draw_transformed(gawl::Screen& screen, const std::array<gawl::Point, 4>& vertices) -> void {
    auto       v = vertices;
    const auto s = screen.get_scale();
    for(auto& p : v) {
        p.magnify(s);
    }
    shader->move_vertices(screen, v, false);
    do_draw(screen);
}

auto MultiTex::operator=(MultiTex&& o) -> MultiTex& {
    release_texture();
    shader   = o.shader;
    textures = std::exchange(o.textures, {});
    width    = o.width;
    height   = o.height;
    return *this;
}

MultiTex::MultiTex(MultiTex&& o) noexcept {
    *this = std::move(o);
}

MultiTex::MultiTex(gawl::impl::GraphicShader& shader, const uint32_t ntex)
    : shader(&shader),
      ntex(ntex) {
    glGenTextures(ntex, textures.data());
    for(auto i = 0u; i < ntex; i += 1) {
        glActiveTexture(GL_TEXTURE0 + i);
        const auto txbinder = bind_texture(i);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    }
}

MultiTex::~MultiTex() {
    release_texture();
}
