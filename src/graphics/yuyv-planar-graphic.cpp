const char* yuyv_planar_graphic_fragment_shader_source = R"glsl(
    #version 330 core
    in vec2           tex_coordinate;
    uniform sampler2D tex_y;
    uniform sampler2D tex_u;
    uniform sampler2D tex_v;
    out vec4          color;
    void main() {
        float y = texture(tex_y, tex_coordinate).r;
        float u = texture(tex_u, tex_coordinate).r - 0.5;
        float v = texture(tex_v, tex_coordinate).r - 0.5;
        float r = y + 1.40200 * v;
        float g = y - 0.34414 * u - 0.71414 * v;
        float b = y + 1.77200 * u;
        color = vec4(r, g, b, 1.0);
    }
)glsl";
