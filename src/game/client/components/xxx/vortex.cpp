#include "vortex.h"

#include <base/math.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/xui/theme.h>

#include <algorithm>
#include <cmath>

static constexpr float REFERENCE_HEIGHT = 900.0f;
static constexpr float CENTER_X = 760.0f / 1440.0f;
static constexpr float CENTER_Y = 420.0f / 900.0f;
static constexpr float MAX_RADIUS = 760.0f;
static constexpr float MIN_RADIUS = 26.0f;
static constexpr float STRETCH_X = 1.25f;
static constexpr float STRETCH_Y = 0.62f;
static constexpr int PARTICLES_PER_PERCENT = 11;
static constexpr int TRAIL_SEGMENTS = 10;
static constexpr float TRAIL_STEP = 1.1f;
static constexpr int STAR_COUNT = 240;

float CVortex::Random()
{
	m_Seed = m_Seed * 1664525u + 1013904223u;
	return (m_Seed >> 8) / (float)(1 << 24);
}

CVortex::SParticle CVortex::Spawn(float Radius)
{
	SParticle Particle;
	Particle.m_Radius = Radius;
	Particle.m_Angle = Random() * 2.0f * pi;
	Particle.m_Speed = 0.6f + Random() * 0.8f;
	Particle.m_Width = Random() < 0.08f ? 2.0f : 1.0f;
	return Particle;
}

void CVortex::RenderStars(float Width, float Height, float Alpha)
{
	const float Time = Client()->GlobalTime();
	uint32_t Seed = 12345u;
	auto Next = [&Seed]() {
		Seed = Seed * 1664525u + 1013904223u;
		return (Seed >> 8) / (float)(1 << 24);
	};
	const ColorRGBA Accent = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_XxUiAccent));
	std::vector<IGraphics::SUiRect> vStars;
	vStars.reserve(STAR_COUNT);
	for(int i = 0; i < STAR_COUNT; i++)
	{
		const float X = Next() * Width;
		const float Y = Next() * Height;
		const float Size = 0.5f + Next() * Next() * 1.4f;
		const float Base = 0.25f + Next() * 0.6f;
		const float Phase = Next() * 2.0f * pi;
		const float Rate = 0.3f + Next() * 0.9f;
		const bool Tinted = Next() < 0.12f;
		const float Twinkle = 0.65f + 0.35f * std::sin(Time * Rate + Phase);
		IGraphics::SUiRect Star;
		Star.m_X = X - Size;
		Star.m_Y = Y - Size;
		Star.m_W = Size * 2.0f;
		Star.m_H = Size * 2.0f;
		Star.m_Radii = vec4(Size, Size, Size, Size);
		const ColorRGBA Color = Tinted ? Accent : ColorRGBA(0.85f, 0.9f, 1.0f, 1.0f);
		Star.m_Fill = Color.WithAlpha(Base * Twinkle * Alpha);
		vStars.push_back(Star);
	}
	Graphics()->DrawUiRects(vStars.data(), vStars.size());
}

void CVortex::Render(float Alpha, float SpeedBoost)
{
	if(Alpha <= 0.0f)
		return;

	const float Height = REFERENCE_HEIGHT;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreenToSize(Width, Height);

	const float Dt = std::clamp(Client()->RenderFrameTime(), 0.0001f, 0.05f);
	m_Boost += (SpeedBoost - m_Boost) * std::min(1.0f, Dt * 2.5f);

	const int Target = std::max(0, PARTICLES_PER_PERCENT * g_Config.m_XxMenuVortexDensity);
	while((int)m_vParticles.size() < Target)
		m_vParticles.push_back(Spawn(40.0f + Random() * (MAX_RADIUS - 40.0f)));
	if((int)m_vParticles.size() > Target)
		m_vParticles.resize(Target);

	const float Speed = g_Config.m_XxMenuVortexSpeed / 100.0f * m_Boost;
	const float Frames = Dt * 60.0f * Speed;

	IGraphics::SUiRect Base;
	Base.m_X = -2.0f;
	Base.m_Y = -2.0f;
	Base.m_W = Width + 4.0f;
	Base.m_H = Height + 4.0f;
	Base.m_Fill = XuiTheme::Color::BACKGROUND.WithAlpha(Alpha);
	Graphics()->DrawUiRects(&Base, 1);

	RenderStars(Width, Height, Alpha);

	const float CenterX = Width * CENTER_X;
	const float CenterY = Height * CENTER_Y;
	const ColorRGBA Accent = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_XxUiAccent));

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	for(SParticle &Particle : m_vParticles)
	{
		Particle.m_Angle += (2.4f / std::pow(Particle.m_Radius, 0.72f)) * Particle.m_Speed * 0.3f * Frames;
		Particle.m_Radius -= (0.2f + 15.0f / Particle.m_Radius) * Particle.m_Speed * Frames;
		if(Particle.m_Radius < MIN_RADIUS)
		{
			Particle = Spawn(700.0f + Random() * 160.0f);
			continue;
		}

		const float K = std::clamp(1.0f - Particle.m_Radius / MAX_RADIUS, 0.0f, 1.0f);
		const float Hot = std::pow(K, 3.2f);
		const ColorRGBA Color(
			Accent.r + (1.0f - Accent.r) * Hot,
			Accent.g * (1.0f - K) * 0.6f + 0.88f * Hot,
			Accent.b * (1.0f - K) * 0.6f + 0.82f * Hot,
			1.0f);
		const float HeadAlpha = (0.08f + 0.55f * K * K) * Alpha;
		const float HalfWidth = Particle.m_Width * (0.6f + K * 1.4f) * 0.5f;

		float Radius = Particle.m_Radius;
		float Angle = Particle.m_Angle;
		vec2 Previous = vec2(CenterX + std::cos(Angle) * Radius * STRETCH_X, CenterY + std::sin(Angle) * Radius * STRETCH_Y);
		for(int i = 0; i < TRAIL_SEGMENTS; i++)
		{
			Angle -= (2.4f / std::pow(Radius, 0.72f)) * Particle.m_Speed * 0.3f * TRAIL_STEP;
			Radius += (0.2f + 15.0f / Radius) * Particle.m_Speed * TRAIL_STEP;
			const vec2 Next = vec2(CenterX + std::cos(Angle) * Radius * STRETCH_X, CenterY + std::sin(Angle) * Radius * STRETCH_Y);
			const vec2 Direction = Next - Previous;
			const float Length = length(Direction);
			if(Length > 0.0001f)
			{
				const vec2 Normal = vec2(-Direction.y, Direction.x) / Length * HalfWidth;
				const float A0 = HeadAlpha * std::pow(1.0f - (float)i / TRAIL_SEGMENTS, 1.6f);
				const float A1 = HeadAlpha * std::pow(1.0f - (float)(i + 1) / TRAIL_SEGMENTS, 1.6f);
				Graphics()->SetColor4(Color.WithAlpha(A0), Color.WithAlpha(A1), Color.WithAlpha(A0), Color.WithAlpha(A1));
				const IGraphics::CFreeformItem Item(Previous.x + Normal.x, Previous.y + Normal.y, Next.x + Normal.x, Next.y + Normal.y, Previous.x - Normal.x, Previous.y - Normal.y, Next.x - Normal.x, Next.y - Normal.y);
				Graphics()->QuadsDrawFreeform(&Item, 1);
			}
			Previous = Next;
		}
	}
	Graphics()->QuadsEnd();

	IGraphics::SUiRect Hole;
	Hole.m_X = CenterX - 80.0f;
	Hole.m_Y = CenterY - 52.0f;
	Hole.m_W = 160.0f;
	Hole.m_H = 104.0f;
	Hole.m_Radii = vec4(52.0f, 52.0f, 52.0f, 52.0f);
	Hole.m_Fill = ColorRGBA(0.0f, 0.0f, 0.0f, Alpha);
	Hole.m_ShadowBlur = 90.0f;
	Hole.m_ShadowSpread = 10.0f;
	Hole.m_Shadow = ColorRGBA(0.0f, 0.0f, 0.0f, 0.9f * Alpha);
	Graphics()->DrawUiRects(&Hole, 1);
}
