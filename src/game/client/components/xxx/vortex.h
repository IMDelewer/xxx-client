#ifndef GAME_CLIENT_COMPONENTS_XXX_VORTEX_H
#define GAME_CLIENT_COMPONENTS_XXX_VORTEX_H

#include <game/client/component.h>

#include <vector>

class CVortex : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }

	void Render(float Alpha, float SpeedBoost);

private:
	struct SParticle
	{
		float m_Radius;
		float m_Angle;
		float m_Speed;
		float m_Width;
	};

	std::vector<SParticle> m_vParticles;
	uint32_t m_Seed = 0x9e3779b9u;
	float m_Boost = 1.0f;

	float Random();
	SParticle Spawn(float Radius);
	void RenderStars(float Width, float Height, float Alpha);
};

#endif
