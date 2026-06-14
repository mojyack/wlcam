// render RGBA into NV12
#version 130
in vec2           tex_coordinate;
uniform sampler2D src;
uniform ivec2     src_size; // RGBA image size (W, H)
uniform ivec2     dst_size; // this plane's pixel size
uniform int       plane;    // 0 = Y, 1 = UV
out vec4          color;

vec3 fetch(int x, int y) {
    x = clamp(x, 0, src_size.x - 1);
    y = clamp(y, 0, src_size.y - 1);
    return texelFetch(src, ivec2(x, src_size.y - 1 - y), 0).rgb; // flip vertical
}

float to_y(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114)) * 0.858824 + 0.062745;
}

float to_u(vec3 c) {
    return dot(c, vec3(-0.168736, -0.331264, 0.5)) * 0.878431 + 0.501961;
}

float to_v(vec3 c) {
    return dot(c, vec3(0.5, -0.418688, -0.081312)) * 0.878431 + 0.501961;
}

void main() {
    int dx = int(tex_coordinate.x * float(dst_size.x));
    int dy = int(tex_coordinate.y * float(dst_size.y));
    if(plane == 0) {
        color = vec4(to_y(fetch(dx, dy)), 0.0, 0.0, 1.0);
    } else {
        int sx = dx * 2;
        int sy = dy * 2;
        vec3 a = 0.25 * (fetch(sx, sy) + fetch(sx + 1, sy) + fetch(sx, sy + 1) + fetch(sx + 1, sy + 1));
        color = vec4(to_u(a), to_v(a), 0.0, 1.0);
    }
}
