#ifndef GAME_CLIENT_COMPONENTS_XXX_GLASS_TEST_H
#define GAME_CLIENT_COMPONENTS_XXX_GLASS_TEST_H

#include <game/client/component.h>

class CGlassTest : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;

private:
	void RenderBackdrop(float Width, float Time);
	void RenderRects(float Width, float Time);
	void RenderDebug();

	float m_BlurMs = 0.0f;
	float m_RectsMs = 0.0f;
};

#endif
