auto yuv422i_fragment_shader_source = R"glsl(
    #version 330 core
    in vec2               tex_coordinate;
    uniform sampler2DRect tex;
    out vec4              color;
    void main() {
        vec2 tex_size = textureSize(tex);
        vec2 coord = vec2(tex_coordinate.x * tex_size.x, tex_coordinate.y * tex_size.y);
        float y = texture(tex, coord).r;
        float u;
        float v;
        if(mod(floor(coord.x), 2) == 0) {
            vec2 next = vec2(coord.x + 1, coord.y);
            u = texture(tex, coord).g - 0.5;
            v = texture(tex, next).g - 0.5;
        } else {
            vec2 prev = vec2(coord.x - 1, coord.y);
            u = texture(tex, prev).g - 0.5;
            v = texture(tex, coord).g - 0.5;
        }
        float r = y + 1.40200 * v;
        float g = y - 0.34414 * u - 0.71414 * v;
        float b = y + 1.77200 * u;
        color = vec4(r, g, b, 1.0);
    }
)glsl";
