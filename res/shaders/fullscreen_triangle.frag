#version 450

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 1) uniform sampler2D Sampler2D[];

void main()
{
    outFragColor = vec4(texture(Sampler2D[0], inUV).xyz, 1.0);
}
