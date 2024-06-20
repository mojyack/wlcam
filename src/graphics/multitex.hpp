#pragma once
#include "../gawl/graphic-shader.hpp"
#include "../gawl/screen.hpp"

class MultiTex {
  private:
    gawl::impl::GraphicShader* shader;
    uint32_t                   ntex;
    std::array<GLuint, 3>      textures;

    auto do_draw(gawl::Screen& screen) const -> void;

  protected:
    int width;
    int height;

    auto bind_texture(uint32_t number) const -> gawl::impl::TextureBinder;
    auto release_texture() -> void;

  public:
    auto get_width(const gawl::MetaScreen& screen) const -> int;
    auto get_height(const gawl::MetaScreen& screen) const -> int;

    // activate texture unit and bind texture before call this
    auto update_texture(int width, int height, int stride, const std::byte* data, GLuint pixformat = GL_RED) -> void;
    auto draw(gawl::Screen& screen, const gawl::Point& point) -> void;
    auto draw_rect(gawl::Screen& screen, const gawl::Rectangle& rect) -> void;
    auto draw_fit_rect(gawl::Screen& screen, const gawl::Rectangle& rect) -> void;
    auto draw_transformed(gawl::Screen& screen, const std::array<gawl::Point, 4>& vertices) -> void;

    auto operator=(MultiTex&& o) -> MultiTex&;

    MultiTex(MultiTex&& o) noexcept;
    // must be ntex <= 3
    MultiTex(gawl::impl::GraphicShader& shader, uint32_t ntex);
    ~MultiTex();
};
