#include "bayer.hpp"
#include "../macros/assert.hpp"

namespace {
auto bayer_fragment_shader_source = R"glsl(
    #version 330 core

    in vec2           tex_coordinate;
    uniform sampler2D tex_0;

    uniform vec2  img_size;
    uniform vec2  bayer_first_red;
    uniform float black_level;
    uniform vec3  wb_gain;
    uniform float gamma;
    uniform int   cell;
    uniform int   rotate;
    uniform float lsc;

    out vec4 color;

    // value of a single sensor pixel from the packed RAW10 texture
    float fetch(int px, int py) {
        px = clamp(px, 0, int(img_size.x) - 1);
        py = clamp(py, 0, int(img_size.y) - 1);
        int bx = (px >> 2) * 5 + (px & 3); // MS byte of this pixel
        return texelFetch(tex_0, ivec2(bx, py), 0).r;
    }

    // value of a logical Bayer cell (averaged over its cell x cell block)
    float cell_val(int cx, int cy) {
        int ox = cx * cell;
        int oy = cy * cell;
        if(cell == 1) {
            return fetch(ox, oy);
        }
        return 0.25 * (fetch(ox, oy) + fetch(ox + 1, oy) + fetch(ox, oy + 1) + fetch(ox + 1, oy + 1));
    }

    void main(void) {
        vec2 uv;
        switch(rotate) {
            case 0:
                uv = tex_coordinate; // 0
                break;
            case 1:
                uv = vec2(tex_coordinate.y, 1.0 - tex_coordinate.x); // 90 CW
                break;
            case 2:
                uv = vec2(1.0 - tex_coordinate.x, 1.0 - tex_coordinate.y); // 180
                break;
            case 3:
                uv = vec2(1.0 - tex_coordinate.y, tex_coordinate.x); // 270 CW
                break;
        }

        vec2 grid = img_size / float(cell);
        int  cx   = int(uv.x * grid.x);
        int  cy   = int(uv.y * grid.y);

        float C     = cell_val(cx, cy);
        float vert  = 0.5 * (cell_val(cx, cy - 1) + cell_val(cx, cy + 1));
        float horiz = 0.5 * (cell_val(cx - 1, cy) + cell_val(cx + 1, cy));
        float green = 0.5 * (vert + horiz);
        float diag  = 0.25 * (cell_val(cx - 1, cy - 1) + cell_val(cx + 1, cy - 1) + cell_val(cx - 1, cy + 1) + cell_val(cx + 1, cy + 1));

        vec2 alt      = mod(vec2(cx, cy) + bayer_first_red, 2.0);
        bool even_col = alt.x < 1.0;
        bool even_row = alt.y < 1.0;
        vec3 rgb = even_col ?
            (even_row ? vec3(C, green, diag) : vec3(vert, C, horiz)) :
            (even_row ? vec3(horiz, C, vert) : vec3(diag, green, C));

        rgb = rgb - vec3(black_level);

        vec2  d   = (uv - 0.5) * img_size;
        float r2  = dot(d, d) / dot(0.5 * img_size, 0.5 * img_size);
        rgb *= 1.0 + lsc * r2;

        rgb = rgb * wb_gain;
        rgb = clamp(rgb, 0.0, 1.0);
        rgb = pow(rgb, vec3(gamma));
        color = vec4(rgb, 1.0);
    }
)glsl";

class BayerShader : public gawl::impl::GraphicShader {
  public:
    std::array<float, 2> img_size = {0, 0};

    GLint loc_img_size  = -1;
    GLint loc_first_red = -1;
    GLint loc_black     = -1;
    GLint loc_wb        = -1;
    GLint loc_gamma     = -1;
    GLint loc_cell      = -1;
    GLint loc_rotate    = -1;
    GLint loc_lsc       = -1;

    auto cache_locations() -> void {
        const auto p  = get_shader();
        loc_img_size  = glGetUniformLocation(p, "img_size");
        loc_first_red = glGetUniformLocation(p, "bayer_first_red");
        loc_black     = glGetUniformLocation(p, "black_level");
        loc_wb        = glGetUniformLocation(p, "wb_gain");
        loc_gamma     = glGetUniformLocation(p, "gamma");
        loc_cell      = glGetUniformLocation(p, "cell");
        loc_rotate    = glGetUniformLocation(p, "rotate");
        loc_lsc       = glGetUniformLocation(p, "lsc_strength");
    }

    auto set_parameters(GLuint /*program*/) -> void override {
        glUniform2fv(loc_img_size, 1, img_size.data());
        glUniform2fv(loc_first_red, 1, bayer_params.first_red.data());
        glUniform1f(loc_black, bayer_params.black_level);
        glUniform3fv(loc_wb, 1, bayer_params.wb_gain.data());
        glUniform1f(loc_gamma, bayer_params.gamma);
        glUniform1i(loc_cell, bayer_params.cell);
        glUniform1i(loc_rotate, bayer_params.rotate);
        glUniform1f(loc_lsc, bayer_params.lsc);
    }
};

auto shader = BayerShader();
} // namespace

auto init_bayer_shader() -> bool {
    ensure(shader.init(gawl::impl::graphic_vertex_shader_source, bayer_fragment_shader_source));
    shader.cache_locations();
    return true;
}

auto BayerGraphic::update_texture(const int width, const int height, const int stride, const std::byte* const data) -> void {
    const auto txbinder = bind_texture();
    this->width         = width;
    this->height        = height;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); // texture width already equals stride
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, stride, height, 0, GL_RED, GL_UNSIGNED_BYTE, data);
    ::shader.img_size = {GLfloat(width), GLfloat(height)};
}

BayerGraphic::BayerGraphic()
    : GraphicBase(::shader) {
}
