#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler2D gBackdrop;

layout(push_constant) uniform SPushBO {
	layout(offset = 32) vec4 gView; // pixels per unit x, y, framebuffer size
} gPushBO;

layout(location = 0) flat in vec4 vRect;
layout(location = 1) flat in vec4 vRadii;
layout(location = 2) flat in vec4 vFill;
layout(location = 3) flat in vec4 vFill2;
layout(location = 4) flat in vec4 vBorder;
layout(location = 5) flat in vec4 vBorder2;
layout(location = 6) flat in vec4 vShadow;
layout(location = 7) flat in vec4 vShadowParams;
layout(location = 8) flat in vec4 vParams0;
layout(location = 9) flat in vec4 vParams1;
layout(location = 10) flat in vec4 vClip;
layout(location = 11) flat in vec4 vFill3;
layout(location = 12) flat in vec4 vFill4;
layout(location = 13) noperspective in vec4 vPos;

layout(location = 0) out vec4 FragClr;

// stops of the glass rim: position, alpha (css 140deg gradient of .glass::before)
const vec2 RIM[6] = vec2[](
	vec2(0.0, 0.42),
	vec2(0.22, 0.10),
	vec2(0.45, 0.0),
	vec2(0.60, 0.0),
	vec2(0.82, 0.12),
	vec2(1.0, 0.28));

const float RIM_ANGLE = 140.0;
const float REFRACTION_BAND = 10.0;
const float REFRACTION_MAX = 0.015;
const float SPECULAR_ALPHA = 0.10;
const float SPECULAR_HOVER_ALPHA = 0.16;

float RimAlpha(float t)
{
	t = clamp(t, 0.0, 1.0);
	for(int i = 1; i < 6; ++i)
	{
		if(t <= RIM[i].x)
			return mix(RIM[i - 1].y, RIM[i].y, (t - RIM[i - 1].x) / (RIM[i].x - RIM[i - 1].x));
	}
	return RIM[5].y;
}

float Erf(float x)
{
	float s = sign(x);
	x = abs(x);
	float x2 = x * x;
	float denom = 1.0 + 0.278393 * x + 0.230389 * x2 + 0.000972 * x2 * x + 0.078108 * x2 * x2;
	float d2 = denom * denom;
	return s * (1.0 - 1.0 / (d2 * d2));
}

vec2 Erf(vec2 x)
{
	return vec2(Erf(x.x), Erf(x.y));
}

float CornerRadius(vec2 p, vec4 radii)
{
	if(p.x < 0.0)
		return p.y < 0.0 ? radii.x : radii.w;
	return p.y < 0.0 ? radii.y : radii.z;
}

float RoundBoxSdf(vec2 p, vec2 halfSize, float radius)
{
	vec2 q = abs(p) - halfSize + radius;
	return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

float ShadowSlice(float x, float y, float sigma, float radius, vec2 halfSize)
{
	float delta = min(halfSize.y - radius - abs(y), 0.0);
	float curved = halfSize.x - radius + sqrt(max(0.0, radius * radius - delta * delta));
	vec2 integral = 0.5 + 0.5 * Erf((x + vec2(-curved, curved)) * (0.70710678 / sigma));
	return integral.y - integral.x;
}

float RoundBoxShadow(vec2 p, vec2 halfSize, float radius, float sigma)
{
	float low = p.y - halfSize.y;
	float high = p.y + halfSize.y;
	float start = clamp(-3.0 * sigma, low, high);
	float end = clamp(3.0 * sigma, low, high);
	float step = (end - start) / 4.0;
	float y = start + step * 0.5;
	float value = 0.0;
	for(int i = 0; i < 4; ++i)
	{
		float gauss = exp(-(y * y) / (2.0 * sigma * sigma)) / (2.50662827 * sigma);
		value += ShadowSlice(p.x, p.y - y, sigma, radius, halfSize) * gauss * step;
		y += step;
	}
	return value;
}

float GradientT(vec2 p, vec2 size, vec2 dir)
{
	float len = abs(size.x * dir.x) + abs(size.y * dir.y);
	return clamp(dot(p, dir) / max(len, 0.0001) + 0.5, 0.0, 1.0);
}

vec4 Premultiply(vec4 c)
{
	return vec4(c.rgb * c.a, c.a);
}

vec4 Over(vec4 top, vec4 bottom)
{
	return top + bottom * (1.0 - top.a);
}

void main()
{
	if(vClip.z >= 0.0 && (vPos.z < vClip.x || vPos.w < vClip.y || vPos.z > vClip.x + vClip.z || vPos.w > vClip.y + vClip.w))
		discard;

	float pxPerUnit = gPushBO.gView.x;
	vec2 p = vPos.xy;
	vec2 size = vRect.zw * pxPerUnit;
	vec2 halfSize = size * 0.5;
	vec4 radii = min(vRadii * pxPerUnit, vec4(min(halfSize.x, halfSize.y)));

	float dist = RoundBoxSdf(p, halfSize, CornerRadius(p, radii));
	float coverage = clamp(0.5 - dist, 0.0, 1.0);

	float glass = vParams0.w;
	vec4 body = vec4(0.0);
	if(glass > 0.0)
	{
		float inside = max(-dist, 0.0);
		float edge = 1.0 - smoothstep(0.0, REFRACTION_BAND * pxPerUnit, inside);
		vec2 shift = -(p / halfSize) * (REFRACTION_MAX * min(size.x, size.y) * vParams1.y * edge * edge);
		vec2 uv = (gl_FragCoord.xy + shift) / gPushBO.gView.zw;
		body = vec4(texture(gBackdrop, clamp(uv, vec2(0.0), vec2(1.0))).rgb, 1.0) * min(glass, 1.0);
	}

	vec4 fill = Premultiply(vFill);
	if(vFill3.a >= 0.0)
	{
		vec2 uv = clamp(p / size + 0.5, 0.0, 1.0);
		fill = mix(mix(fill, Premultiply(vFill2), uv.x), mix(Premultiply(vFill3), Premultiply(vFill4), uv.x), uv.y);
	}
	else if(vFill2.a >= 0.0)
	{
		float angle = radians(vParams0.x);
		float t = GradientT(p, size, vec2(cos(angle), sin(angle)));
		fill = mix(fill, Premultiply(vFill2), t);
	}
	body = Over(fill, body);

	if(glass > 0.0)
	{
		vec2 q = p / halfSize;
		vec2 diagonal = vec2(0.70710678);
		float band = -0.55;
		float alpha = SPECULAR_ALPHA;
		if(vParams1.z >= 0.0 && vParams1.w >= 0.0)
		{
			vec2 toSpecular = (vParams1.zw - (vRect.xy + vRect.zw * 0.5)) * pxPerUnit / halfSize;
			band = clamp(dot(toSpecular, diagonal), -1.0, 1.0) * 0.6;
			alpha = SPECULAR_HOVER_ALPHA;
		}
		float x = (dot(q, diagonal) - band) / 0.38;
		float highlight = alpha * exp(-x * x) * (1.0 - 0.35 * length(q));
		body = Over(vec4(highlight), body);
	}

	body *= coverage;

	if(vParams0.y > 0.0 && (vBorder.a > 0.0 || vBorder2.a > 0.0))
	{
		float width = vParams0.y * pxPerUnit;
		float ring = coverage - clamp(0.5 - (dist + width), 0.0, 1.0);
		float angle = radians(vParams0.z);
		float t = GradientT(p, size, vec2(sin(angle), -cos(angle)));
		vec4 border = vBorder;
		if(vBorder2.a >= 0.0)
		{
			if(abs(vParams0.z - RIM_ANGLE) < 0.5)
				border = vec4(mix(vBorder.rgb, vBorder2.rgb, t), RimAlpha(t) * vBorder.a);
			else
				border = mix(vBorder, vBorder2, t);
		}
		body = Over(Premultiply(border) * ring, body);
	}

	vec4 result = body;
	if(vShadow.a > 0.0)
	{
		vec2 offset = vShadowParams.xy * pxPerUnit;
		float spread = vShadowParams.w * pxPerUnit;
		float blur = vShadowParams.z * pxPerUnit;
		vec2 sp = p - offset;
		vec2 shadowHalf = halfSize + spread;
		float radius = clamp(CornerRadius(sp, radii) + spread, 0.0, min(shadowHalf.x, shadowHalf.y));
		float amount;
		if(blur < 0.5)
			amount = clamp(0.5 - RoundBoxSdf(sp, shadowHalf, radius), 0.0, 1.0);
		else
			amount = RoundBoxShadow(sp, shadowHalf, radius, blur * 0.5);
		amount *= 1.0 - coverage;
		result = Over(result, Premultiply(vShadow) * amount);
	}

	FragClr = result;
}
