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
