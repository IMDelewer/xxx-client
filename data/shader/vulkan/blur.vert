#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec2 outUv;

void main()
{
	vec2 corner = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
	outUv = corner;
	gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
