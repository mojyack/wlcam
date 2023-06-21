#pragma once
#include <gawl/graphic-globject.hpp>
#include <gawl/pixelbuffer.hpp>
#include <gawl/screen.hpp>

template <size_t ntex, GLuint texformat = GL_TEXTURE_2D>
class MultiTexGraphicBase {
  private:
    using GL        = gawl::internal::GraphicGLObject;
    using Point     = gawl::Point;
    using Rectangle = gawl::Rectangle;

    GL*                      gl;
    std::array<GLuint, ntex> textures = {0};

    auto do_draw(gawl::concepts::Screen auto& screen) const -> void {
        const auto vabinder = gl->bind_vao();
        const auto ebbinder = gl->bind_ebo();
        const auto shbinder = gl->use_shader();
        const auto fbbinder = screen.prepare();

        glActiveTexture(GL_TEXTURE0);
        auto txbinder_y = bind_texture(0);
        glActiveTexture(GL_TEXTURE1);
        auto txbinder_u = bind_texture(1);
        glActiveTexture(GL_TEXTURE2);
        auto txbinder_v = bind_texture(2);

        const auto prog = gl->get_shader();
        for(auto i = size_t(0); i < ntex; i += 1) {
            auto name = std::array{'t', 'e', 'x', '_', char('0' + i), '\0'};
            glUniform1i(glGetUniformLocation(prog, name.data()), i);
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glActiveTexture(GL_TEXTURE2);
        txbinder_v.unbind();
        glActiveTexture(GL_TEXTURE1);
        txbinder_u.unbind();
        glActiveTexture(GL_TEXTURE0);
        txbinder_y.unbind();
    }

  protected:
    int width;
    int height;

    auto bind_texture(const uint32_t number) const -> gawl::internal::TextureBinder {
        return textures[number];
    }

    auto release_texture() -> void {
        if(textures[0] != 0) {
            glDeleteTextures(ntex, textures.data());
        }
    }

  public:
    auto get_width(const gawl::concepts::MetaScreen auto& screen) const -> int {
        return width / screen.get_scale();
    }

    auto get_height(const gawl::concepts::MetaScreen auto& screen) const -> int {
        return height / screen.get_scale();
    }

    // activate texture unit and bind texture before call this
    auto update_texture(const int width, const int height, const std::byte* const data, const GLuint pixformat = GL_RED) -> void {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, width);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        this->width  = width;
        this->height = height;

        glTexImage2D(texformat, 0, pixformat, this->width, this->height, 0, pixformat, GL_UNSIGNED_BYTE, data);
    }

    auto draw(gawl::concepts::Screen auto& screen, const Point& point) -> void {
        draw_rect(screen, {{point.x, point.y}, {point.x + width, point.y + height}});
    }

    auto draw_rect(gawl::concepts::Screen auto& screen, const Rectangle& rect) -> void {
        gl->move_vertices(screen, Rectangle(rect).magnify(screen.get_scale()), false);
        do_draw(screen);
    }

    auto draw_fit_rect(gawl::concepts::Screen auto& screen, const Rectangle& rect) -> void {
        draw_rect(screen, calc_fit_rect(rect, width, height));
    }

    auto draw_transformed(gawl::concepts::Screen auto& screen, const std::array<Point, 4>& vertices) -> void {
        auto       v = vertices;
        const auto s = screen.get_scale();
        for(auto& p : v) {
            p.magnify(s);
        }
        gl->move_vertices(screen, v, false);
        do_draw(screen);
    }

    auto operator=(MultiTexGraphicBase&& o) -> MultiTexGraphicBase& {
        release_texture();
        gl       = o.gl;
        textures = std::exchange(o.textures, {});
        width    = o.width;
        height   = o.height;
        return *this;
    }

    MultiTexGraphicBase(GL& gl) : gl(&gl) {
        glGenTextures(ntex, textures.data());
        for(auto i = 0; i < 3; i += 1) {
            glActiveTexture(GL_TEXTURE0 + i);
            const auto txbinder = bind_texture(i);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        }
    }

    MultiTexGraphicBase(MultiTexGraphicBase&& o) {
        *this = std::move(o);
    }

    ~MultiTexGraphicBase() {
        release_texture();
    }
};

