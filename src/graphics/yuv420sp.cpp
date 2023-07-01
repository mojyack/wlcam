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
