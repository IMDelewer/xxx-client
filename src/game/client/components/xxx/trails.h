#ifndef GAME_CLIENT_COMPONENTS_XXX_TRAILS_H
#define GAME_CLIENT_COMPONENTS_XXX_TRAILS_H

#include <base/color.h>

#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include <vector>

class CTrails : public CComponent
{
public:
	enum
	{
		COLORMODE_SOLID = 1,
		COLORMODE_TEE,
		COLORMODE_RAINBOW,
		COLORMODE_SPEED,
		COLORMODE_RANDOM,
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnReset() override;

private:
	static constexpr int HISTORY = 200;

	struct SHistory
	{
		vec2 m_Pos = vec2(0.0f, 0.0f);
		int m_Tick = -1;
	};

	struct SPart
	{
		vec2 m_Pos = vec2(0.0f, 0.0f);
		vec2 m_RawPos = vec2(0.0f, 0.0f);
		ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		float m_Width = 0.0f;
		float m_Progress = 1.0f;
		vec2 m_Top = vec2(0.0f, 0.0f);
		vec2 m_Bottom = vec2(0.0f, 0.0f);
		bool m_Flip = false;
		int m_Tick = -1;
	};

	SHistory m_aaHistory[MAX_CLIENTS][HISTORY];
	bool m_aHistoryValid[MAX_CLIENTS] = {};
	std::vector<SPart> m_vTrail;

	void ClearHistory(int ClientId);
	bool PredictsPlayer(int ClientId) const;
};

#endif
