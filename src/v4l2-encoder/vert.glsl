#version 130
in vec2  position;
in vec2  texcoord;
out vec2 tex_coordinate;
void main() {
    gl_Position    = vec4(position, 0.0, 1.0);
    tex_coordinate = texcoord;
}
