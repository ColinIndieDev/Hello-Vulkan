#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

struct vertex {
    vec3 pos;
    vec4 color;
};

layout (buffer_reference, std430, scalar) readonly buffer vertex_buffer_ref {
    vertex vertices[];
};

layout (push_constant) uniform PushConstants {
    vertex_buffer_ref vertex_buffer;
    layout(row_major) mat4 projection;
} push;

layout(location = 0) out vec4 frag_color;

void main() {
    vertex v = push.vertex_buffer.vertices[gl_VertexIndex];

    vec4 position = vec4(v.pos, 1.0);
    gl_Position = push.projection * position;
    frag_color = v.color;
}
