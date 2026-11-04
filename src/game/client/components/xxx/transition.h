#ifndef GAME_CLIENT_COMPONENTS_XXX_TRANSITION_H
#define GAME_CLIENT_COMPONENTS_XXX_TRANSITION_H

#include <base/vmath.h>

#include <game/client/component.h>

class CTransition : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnStateChange(int NewState, int OldState) override;
	void OnRender() override;

	void OnMapLoaded();

	bool WorldBehindMenu() const;
	bool Revealing() const { return m_Phase == EPhase::REVEAL; }
	vec2 MapCenter() const { return m_MapCenter; }
	float MapZoom() const;

private:
	enum class EPhase
	{
		MENU,
		LOADING,
		REVEAL,
		GAME,
	};

	EPhase m_Phase = EPhase::MENU;
	float m_PhaseStart = 0.0f;
	float m_MapReadyTime = 0.0f;
	float m_ReturnStart = -100.0f;
	bool m_MapReady = false;
	bool m_ZoomEase = false;
	vec2 m_MapCenter = vec2(0.0f, 0.0f);

	float Now() const;
	float Elapsed() const { return Now() - m_PhaseStart; }
	void UpdateActivity();
	void RenderBlur(float Radius, float Dim);
};

#endif
