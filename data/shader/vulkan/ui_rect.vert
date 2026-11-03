#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 inRect;
layout(location = 1) in vec4 inRadii;
layout(location = 2) in vec4 inFill;
layout(location = 3) in vec4 inFill2;
layout(location = 4) in vec4 inBorder;
layout(location = 5) in vec4 inBorder2;
layout(location = 6) in vec4 inShadow;
layout(location = 7) in vec4 inShadowParams;
layout(location = 8) in vec4 inParams0;
layout(location = 9) in vec4 inParams1;
layout(location = 10) in vec4 inClip;
layout(location = 11) in vec4 inFill3;
layout(location = 12) in vec4 inFill4;

layout(push_constant) uniform SPushBO {
	layout(offset = 0) mat4x2 gPos;
	layout(offset = 32) vec4 gView; // pixels per unit x, y, framebuffer size
} gPushBO;

layout(location = 0) flat out vec4 vRect;
layout(location = 1) flat out vec4 vRadii;
layout(location = 2) flat out vec4 vFill;
layout(location = 3) flat out vec4 vFill2;
layout(location = 4) flat out vec4 vBorder;
layout(location = 5) flat out vec4 vBorder2;
layout(location = 6) flat out vec4 vShadow;
layout(location = 7) flat out vec4 vShadowParams;
layout(location = 8) flat out vec4 vParams0;
layout(location = 9) flat out vec4 vParams1;
layout(location = 10) flat out vec4 vClip;
layout(location = 11) flat out vec4 vFill3;
layout(location = 12) flat out vec4 vFill4;
layout(location = 13) noperspective out vec4 vPos; // xy: pixels from the rect centre, zw: position in units

void main()
{
	vec2 corner = vec2(float(gl_VertexIndex & 1), float(gl_VertexIndex >> 1));
	float pxPerUnit = gPushBO.gView.x;

	float reach = abs(inShadowParams.x) + abs(inShadowParams.y) + max(inShadowParams.w, 0.0) + inShadowParams.z * 1.5 + 2.0 / pxPerUnit;
	vec2 lower = inRect.xy - reach;
	vec2 upper = inRect.xy + inRect.zw + reach;
	vec2 pos = mix(lower, upper, corner);

	gl_Position = vec4(gPushBO.gPos * vec4(pos, 0.0, 1.0), 0.0, 1.0);
	vPos = vec4((pos - (inRect.xy + inRect.zw * 0.5)) * pxPerUnit, pos);

	vRect = inRect;
	vRadii = inRadii;
	vFill = inFill;
	vFill2 = inFill2;
	vBorder = inBorder;
	vBorder2 = inBorder2;
	vShadow = inShadow;
	vShadowParams = inShadowParams;
	vParams0 = inParams0;
	vParams1 = inParams1;
	vClip = inClip;
	vFill3 = inFill3;
	vFill4 = inFill4;
}
