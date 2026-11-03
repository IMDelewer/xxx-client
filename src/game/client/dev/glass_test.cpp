#if defined(DDEV_XX)
#include "glass_test.h"

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/xui/theme.h>

#include <cmath>
#include <vector>

using SRect = IGraphics::SUiRect;

static constexpr float REFERENCE_HEIGHT = 900.0f;

static ColorRGBA Mix(ColorRGBA A, ColorRGBA B, float T)
{
	return ColorRGBA(A.r + (B.r - A.r) * T, A.g + (B.g - A.g) * T, A.b + (B.b - A.b) * T, A.a + (B.a - A.a) * T);
}

static SRect GlassRect(float x, float y, float w, float h, float Radius)
{
	SRect Rect;
	Rect.m_X = x;
	Rect.m_Y = y;
	Rect.m_W = w;
	Rect.m_H = h;
	Rect.m_Radii = vec4(Radius, Radius, Radius, Radius);
	Rect.m_Fill = XuiTheme::Glass::FILL_TOP;
	Rect.m_Fill2 = XuiTheme::Glass::FILL_BOTTOM;
	Rect.m_BorderWidth = XuiTheme::Glass::RIM_WIDTH;
	Rect.m_Border = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	Rect.m_Border2 = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	Rect.m_BorderAngle = XuiTheme::Glass::RIM_ANGLE;
	Rect.m_ShadowOffset = vec2(0.0f, XuiTheme::Glass::SHADOW_Y);
	Rect.m_ShadowBlur = XuiTheme::Glass::SHADOW_BLUR;
	Rect.m_Shadow = XuiTheme::Glass::SHADOW;
	Rect.m_Glass = 1.0f;
	Rect.m_Saturation = XuiTheme::Glass::SATURATION;
	Rect.m_Refraction = 1.0f;
	return Rect;
}

static SRect HairlineRect(const SRect &Parent)
{
	SRect Rect;
	Rect.m_X = Parent.m_X;
	Rect.m_Y = Parent.m_Y;
	Rect.m_W = Parent.m_W;
	Rect.m_H = Parent.m_H;
	Rect.m_Radii = Parent.m_Radii;
	Rect.m_BorderWidth = 0.5f;
	Rect.m_Border = XuiTheme::Glass::INNER_STROKE;
	return Rect;
}

static SRect PanelRect(float x, float y, float w, float h, float Radius)
{
	SRect Rect;
	Rect.m_X = x;
	Rect.m_Y = y;
	Rect.m_W = w;
	Rect.m_H = h;
	Rect.m_Radii = vec4(Radius, Radius, Radius, Radius);
	Rect.m_Fill = XuiTheme::Glass::PANEL_TOP;
	Rect.m_Fill2 = XuiTheme::Glass::PANEL_BOTTOM;
	Rect.m_ShadowOffset = vec2(0.0f, XuiTheme::Glass::SHADOW_Y);
	Rect.m_ShadowBlur = XuiTheme::Glass::SHADOW_BLUR;
	Rect.m_Shadow = XuiTheme::Glass::SHADOW;
	Rect.m_Glass = 1.0f;
	Rect.m_Saturation = XuiTheme::Glass::SATURATION;
	Rect.m_Refraction = 1.0f;
	return Rect;
}

void CGlassTest::RenderBackdrop(float Width, float Time)
{
	Graphics()->TextureClear();
	Graphics()->DrawRect(0.0f, 0.0f, Width, REFERENCE_HEIGHT, ColorRGBA(0.06f, 0.07f, 0.14f, 0.45f), IGraphics::CORNER_NONE, 0.0f);

	const ColorRGBA aBlobs[] = {ColorRGBA(1.0f, 0.23f, 0.19f, 1.0f), ColorRGBA(0.19f, 0.82f, 0.35f, 1.0f), ColorRGBA(0.39f, 0.82f, 1.0f, 1.0f), ColorRGBA(1.0f, 0.84f, 0.04f, 1.0f), ColorRGBA(1.0f, 0.22f, 0.37f, 1.0f)};
	for(int i = 0; i < 5; i++)
	{
		const float x = 40.0f + i * 150.0f + std::sin(Time * 0.3f + i) * 30.0f;
		Graphics()->DrawRect(x, 90.0f + (i % 2) * 40.0f, 170.0f, 420.0f - (i % 3) * 60.0f, aBlobs[i].WithAlpha(0.9f), IGraphics::CORNER_ALL, 60.0f);
	}

	Graphics()->DrawRect(Width * 0.5f, 60.0f, Width * 0.5f - 40.0f, 330.0f, ColorRGBA(0.85f, 0.86f, 0.9f, 1.0f), IGraphics::CORNER_ALL, 24.0f);
	Graphics()->DrawRect(Width * 0.5f + 30.0f, 430.0f, Width * 0.5f - 100.0f, 360.0f, ColorRGBA(0.12f, 0.13f, 0.2f, 1.0f), IGraphics::CORNER_ALL, 24.0f);

	const float StripeX = std::fmod(Time * 90.0f, 1000.0f) - 120.0f;
	Graphics()->DrawRect(StripeX, 0.0f, 46.0f, REFERENCE_HEIGHT, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), IGraphics::CORNER_NONE, 0.0f);
	Graphics()->DrawRect(StripeX + 70.0f, 0.0f, 10.0f, REFERENCE_HEIGHT, ColorRGBA(1.0f, 0.9f, 0.2f, 1.0f), IGraphics::CORNER_NONE, 0.0f);
	const float StripeY = 120.0f + std::fmod(Time * 40.0f, 640.0f);
	Graphics()->DrawRect(0.0f, StripeY, Width, 6.0f, ColorRGBA(0.2f, 1.0f, 0.9f, 1.0f), IGraphics::CORNER_NONE, 0.0f);

	const char *pText = "The quick brown fox jumps over the lazy dog 0123456789";
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	for(int i = 0; i < 6; i++)
		TextRender()->Text(60.0f, 110.0f + i * 22.0f, 18.0f, pText);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CGlassTest::RenderRects(float Width, float Time)
{
	std::vector<SRect> vRects;
	const ColorRGBA Accent = XuiTheme::Color::ACCENT_DEFAULT;

	// big glass panel, exact .glass recipe
	SRect Glass = GlassRect(70.0f, 130.0f, 560.0f, 330.0f, 28.0f);
	const vec2 Orbit(Glass.m_X + Glass.m_W * 0.5f + std::cos(Time * 0.7f) * 240.0f, Glass.m_Y + Glass.m_H * 0.5f + std::sin(Time * 0.7f) * 140.0f);
	Glass.m_Specular = Orbit;
	vRects.push_back(Glass);
	vRects.push_back(HairlineRect(Glass));

	// dark panel
	SRect Panel = PanelRect(Width * 0.5f + 40.0f, 80.0f, 360.0f, 220.0f, 22.0f);
	vRects.push_back(Panel);
	vRects.push_back(HairlineRect(Panel));

	// context menu
	SRect Ctx = GlassRect(Width * 0.5f + 430.0f, 80.0f, 230.0f, 220.0f, 20.0f);
	Ctx.m_Fill = XuiTheme::Color::MENU_FILL;
	Ctx.m_Fill2 = ColorRGBA(0, 0, 0, -1);
	Ctx.m_Saturation = 1.0f;
	vRects.push_back(Ctx);

	// accent pill button
	SRect Red;
	Red.m_X = Width * 0.5f + 60.0f;
	Red.m_Y = 330.0f;
	Red.m_W = 180.0f;
	Red.m_H = XuiTheme::Size::CONTROL_H;
	Red.m_Radii = vec4(18, 18, 18, 18);
	Red.m_Fill = Mix(Accent, ColorRGBA(1, 1, 1, 1), 0.18f);
	Red.m_Fill2 = Accent;
	Red.m_BorderWidth = 1.0f;
	Red.m_Border = ColorRGBA(1.0f, 1.0f, 1.0f, 0.35f);
	Red.m_Border2 = ColorRGBA(1.0f, 1.0f, 1.0f, 0.0f);
	Red.m_BorderAngle = 180.0f;
	Red.m_ShadowOffset = vec2(0.0f, 6.0f);
	Red.m_ShadowBlur = 20.0f;
	Red.m_Shadow = Accent.WithAlpha(0.3f);
	vRects.push_back(Red);

	// island
	SRect Island;
	Island.m_X = Width * 0.5f - 120.0f;
	Island.m_Y = 14.0f;
	Island.m_W = 240.0f;
	Island.m_H = 38.0f;
	Island.m_Radii = vec4(19, 19, 19, 19);
	Island.m_Fill = XuiTheme::Color::ISLAND;
	Island.m_BorderWidth = 1.0f;
	Island.m_Border = ColorRGBA(1.0f, 1.0f, 1.0f, 0.07f);
	Island.m_ShadowOffset = vec2(0.0f, 10.0f);
	Island.m_ShadowBlur = 36.0f;
	Island.m_Shadow = ColorRGBA(0.0f, 0.0f, 0.0f, 0.55f);
	vRects.push_back(Island);

	// shadows of several sizes on the light card
	const float aBlur[] = {6.0f, 16.0f, 30.0f, 48.0f};
	for(int i = 0; i < 4; i++)
	{
		SRect Box;
		Box.m_X = Width * 0.5f + 40.0f + i * 120.0f;
		Box.m_Y = 110.0f;
		Box.m_W = 90.0f;
		Box.m_H = 90.0f;
		Box.m_Radii = vec4(14, 14, 14, 14);
		Box.m_Fill = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		Box.m_ShadowOffset = vec2(0.0f, 8.0f);
		Box.m_ShadowBlur = aBlur[i];
		Box.m_ShadowSpread = i == 3 ? -8.0f : 0.0f;
		Box.m_Shadow = ColorRGBA(0.0f, 0.0f, 0.0f, 0.5f);
		vRects.push_back(Box);
	}

	// gradients at several angles
	const float aAngles[] = {0.0f, 45.0f, 90.0f, 135.0f};
	for(int i = 0; i < 4; i++)
	{
		SRect Box;
		Box.m_X = Width * 0.5f + 60.0f + i * 120.0f;
		Box.m_Y = 460.0f;
		Box.m_W = 100.0f;
		Box.m_H = 60.0f;
		Box.m_Radii = vec4(12, 12, 12, 12);
		Box.m_Fill = XuiTheme::Color::CYAN;
		Box.m_Fill2 = XuiTheme::Color::PINK;
		Box.m_GradientAngle = aAngles[i];
		vRects.push_back(Box);
	}

	// borders
	const float aBorders[] = {1.0f, 2.0f, 4.0f};
	for(int i = 0; i < 3; i++)
	{
		SRect Box;
		Box.m_X = Width * 0.5f + 60.0f + i * 150.0f;
		Box.m_Y = 550.0f;
		Box.m_W = 120.0f;
		Box.m_H = 70.0f;
		Box.m_Radii = vec4(20, 20, 20, 20);
		Box.m_Fill = ColorRGBA(1, 1, 1, 0.08f);
		Box.m_BorderWidth = aBorders[i];
		Box.m_Border = XuiTheme::Color::YELLOW;
		Box.m_Border2 = XuiTheme::Color::GREEN;
		Box.m_BorderAngle = 60.0f;
		vRects.push_back(Box);
	}

	// one pixel hairlines and radii from tiny to huge
	for(int i = 0; i < 6; i++)
	{
		SRect Line;
		Line.m_X = Width * 0.5f + 60.0f;
		Line.m_Y = 650.0f + i * 6.0f;
		Line.m_W = 300.0f;
		Line.m_H = 1.0f;
		Line.m_Fill = ColorRGBA(1.0f, 1.0f, 1.0f, 0.8f);
		vRects.push_back(Line);
	}
	const float aRadii[] = {0.0f, 2.0f, 8.0f, 24.0f, 1000.0f};
	for(int i = 0; i < 5; i++)
	{
		SRect Box;
		Box.m_X = Width * 0.5f + 60.0f + i * 84.0f;
		Box.m_Y = 700.0f;
		Box.m_W = 70.0f;
		Box.m_H = 70.0f;
		Box.m_Radii = vec4(aRadii[i], aRadii[i], aRadii[i], aRadii[i]);
		Box.m_Fill = Accent;
		vRects.push_back(Box);
	}
	SRect Mixed;
	Mixed.m_X = 90.0f;
	Mixed.m_Y = 520.0f;
	Mixed.m_W = 200.0f;
	Mixed.m_H = 80.0f;
	Mixed.m_Radii = vec4(36, 4, 36, 4);
	Mixed.m_Fill = XuiTheme::Color::GREEN.WithAlpha(0.8f);
	Mixed.m_Fill2 = XuiTheme::Color::CYAN.WithAlpha(0.8f);
	Mixed.m_ClipX = 90.0f;
	Mixed.m_ClipY = 520.0f;
	Mixed.m_ClipW = 150.0f + std::sin(Time) * 50.0f;
	Mixed.m_ClipH = 80.0f;
	vRects.push_back(Mixed);

	// small glass pills over the moving stripes
	for(int i = 0; i < 3; i++)
	{
		SRect Pill = GlassRect(330.0f + i * 100.0f, 520.0f, 90.0f, 50.0f, 25.0f);
		Pill.m_ShadowBlur = 20.0f;
		Pill.m_ShadowOffset = vec2(0.0f, 6.0f);
		vRects.push_back(Pill);
	}

	Graphics()->DrawUiRects(vRects.data(), vRects.size());

	const float PixelScale = Graphics()->ScreenHeight() / REFERENCE_HEIGHT;
	Graphics()->ClipEnable((int)(330.0f * PixelScale), (int)(620.0f * PixelScale), (int)(150.0f * PixelScale), (int)(60.0f * PixelScale));
	SRect Clipped = GlassRect(300.0f, 610.0f, 260.0f, 80.0f, 40.0f);
	Clipped.m_Fill = Accent.WithAlpha(0.35f);
	Clipped.m_Fill2 = ColorRGBA(0, 0, 0, -1);
	Graphics()->DrawUiRects(&Clipped, 1);
	Graphics()->ClipDisable();
}

void CGlassTest::RenderDebug()
{
	const IGraphics::SUiStats Stats = Graphics()->GetUiStats();
	m_BlurMs += (Stats.m_BlurMs - m_BlurMs) * 0.05f;
	m_RectsMs += (Stats.m_RectsMs - m_RectsMs) * 0.05f;

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreenToSize(Width, Height);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "ui rects: %d, draw calls: %d, blur: %.2f ms, rects: %.2f ms", Stats.m_Rects, Stats.m_DrawCalls, m_BlurMs, m_RectsMs);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	TextRender()->Text(4.0f, Height - 12.0f, 6.0f, aBuf);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CGlassTest::OnRender()
{
	if(g_Config.m_XxGlassTest)
	{
		const float Width = REFERENCE_HEIGHT * Graphics()->ScreenAspect();
		Graphics()->MapScreenToSize(Width, REFERENCE_HEIGHT);
		const float Time = Client()->LocalTime();
		RenderBackdrop(Width, Time);
		RenderRects(Width, Time);
	}

	if(g_Config.m_XxUiDebug)
		RenderDebug();
}

#endif
