#pragma once
#include <gawl/graphic-globject.hpp>
#include <gawl/pixelbuffer.hpp>
#include <gawl/screen.hpp>

using GraphicGLObject = gawl::internal::GraphicGLObject;
using TextureBinder   = gawl::internal::TextureBinder;

class YUVPlanarGraphicBase {
  private:
    using GL        = gawl::internal::GraphicGLObject;
    using Point     = gawl::Point;
    using Rectangle = gawl::Rectangle;

    GL*                   gl;
    std::array<GLuint, 3> textures = {0}; // Y, U, V

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
        glUniform1i(glGetUniformLocation(prog, "tex_y"), 0);
        glUniform1i(glGetUniformLocation(prog, "tex_u"), 1);
        glUniform1i(glGetUniformLocation(prog, "tex_v"), 2);

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

    auto bind_texture(const uint32_t number) const -> TextureBinder {
        return textures[number];
    }

    auto release_texture() -> void {
        if(textures[0] != 0) {
            glDeleteTextures(3, textures.data());
        }
    }

  public:
    auto get_width(const gawl::concepts::MetaScreen auto& screen) const -> int {
        return width / screen.get_scale();
    }

    auto get_height(const gawl::concepts::MetaScreen auto& screen) const -> int {
        return height / screen.get_scale();
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

    auto operator=(YUVPlanarGraphicBase&& o) -> YUVPlanarGraphicBase& {
        release_texture();
        gl       = o.gl;
        textures = std::exchange(o.textures, {0, 0, 0});
        width    = o.width;
        height   = o.height;
        return *this;
    }

    YUVPlanarGraphicBase(GL& gl) : gl(&gl) {
        glGenTextures(3, textures.data());
        for(auto i = 0; i < 3; i += 1) {
            glActiveTexture(GL_TEXTURE0 + i);
            const auto txbinder = bind_texture(i);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        }
    }

    YUVPlanarGraphicBase(YUVPlanarGraphicBase&& o) {
        *this = std::move(o);
    }

    ~YUVPlanarGraphicBase() {
        release_texture();
    }
};

