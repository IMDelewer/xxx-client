#include "glass.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/xui/theme.h>

#include <algorithm>

ColorRGBA Glass::Accent()
{
	return color_cast<ColorRGBA>(ColorHSLA(g_Config.m_XxUiAccent));
}

static ColorRGBA Mix(ColorRGBA A, ColorRGBA B, float t)
{
	return ColorRGBA(A.r + (B.r - A.r) * t, A.g + (B.g - A.g) * t, A.b + (B.b - A.b) * t, A.a + (B.a - A.a) * t);
}

void Glass::Draw(IGraphics *pGraphics, float x, float y, float w, float h, ColorRGBA Tint, float Hot, int Corners, float Rounding)
{
	if(w <= 0.0f || h <= 0.0f)
		return;
	Hot = std::clamp(Hot, 0.0f, 1.0f);
	const float r = std::min(Rounding, std::min(w, h) / 2.0f);

	IGraphics::SUiRect Rect{x, y, w, h};
	Rect.m_Radii = vec4((Corners & IGraphics::CORNER_TL) ? r : 0.0f, (Corners & IGraphics::CORNER_TR) ? r : 0.0f, (Corners & IGraphics::CORNER_BR) ? r : 0.0f, (Corners & IGraphics::CORNER_BL) ? r : 0.0f);

	const bool Accented = std::max({Tint.r, Tint.g, Tint.b}) - std::min({Tint.r, Tint.g, Tint.b}) > 0.2f;
	if(Accented)
	{
		const ColorRGBA White(1.0f, 1.0f, 1.0f, 1.0f);
		Rect.m_Fill = Mix(Mix(Tint, White, 0.18f), White, Hot * 0.12f);
		Rect.m_Fill2 = Mix(Tint, White, Hot * 0.12f);
		Rect.m_BorderWidth = 1.0f;
		Rect.m_Border = ColorRGBA(1.0f, 1.0f, 1.0f, 0.35f);
		Rect.m_Border2 = ColorRGBA(1.0f, 1.0f, 1.0f, 0.0f);
		Rect.m_BorderAngle = 90.0f;
	}
	else
	{
		const float Boost = 1.0f + Hot * 0.8f;
		Rect.m_Fill = XuiTheme::Glass::FILL_TOP.WithMultipliedAlpha(Boost);
		Rect.m_Fill2 = XuiTheme::Glass::FILL_BOTTOM.WithMultipliedAlpha(Boost);
		Rect.m_BorderWidth = XuiTheme::Glass::RIM_WIDTH;
		Rect.m_Border = ColorRGBA(1.0f, 1.0f, 1.0f, 0.16f);
		Rect.m_Border2 = ColorRGBA(1.0f, 1.0f, 1.0f, 0.08f);
		Rect.m_BorderAngle = XuiTheme::Glass::RIM_ANGLE;
		Rect.m_ShadowOffset = vec2(0.0f, XuiTheme::Glass::SHADOW_Y);
		Rect.m_ShadowBlur = XuiTheme::Glass::SHADOW_BLUR;
		Rect.m_Shadow = XuiTheme::Glass::SHADOW;
		Rect.m_Glass = 1.0f;
		Rect.m_Saturation = XuiTheme::Glass::SATURATION;
		Rect.m_Refraction = 0.35f;
	}
	pGraphics->DrawUiRects(&Rect, 1);
}
