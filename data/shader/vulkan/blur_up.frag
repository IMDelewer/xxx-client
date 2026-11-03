#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler2D gSource;

layout(push_constant) uniform SBlurBO {
	layout(offset = 0) vec2 gHalfPixel; // half a texel of the source
	layout(offset = 8) float gOffset;
	layout(offset = 12) float gSaturation;
} gBlurBO;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 FragClr;

void main()
{
	vec2 h = gBlurBO.gHalfPixel * gBlurBO.gOffset;
	vec4 sum = texture(gSource, inUv + vec2(-2.0 * h.x, 0.0));
	sum += texture(gSource, inUv + vec2(-h.x, h.y)) * 2.0;
	sum += texture(gSource, inUv + vec2(0.0, 2.0 * h.y));
	sum += texture(gSource, inUv + vec2(h.x, h.y)) * 2.0;
	sum += texture(gSource, inUv + vec2(2.0 * h.x, 0.0));
	sum += texture(gSource, inUv + vec2(h.x, -h.y)) * 2.0;
	sum += texture(gSource, inUv + vec2(0.0, -2.0 * h.y));
	sum += texture(gSource, inUv + vec2(-h.x, -h.y)) * 2.0;
	vec3 color = (sum / 12.0).rgb;
	float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
	FragClr = vec4(max(mix(vec3(luma), color, gBlurBO.gSaturation), vec3(0.0)), 1.0);
}
