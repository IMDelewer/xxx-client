#ifndef GAME_CLIENT_COMPONENTS_XXX_RAINBOW_H
#define GAME_CLIENT_COMPONENTS_XXX_RAINBOW_H

#include <base/color.h>

#include <game/client/component.h>

class CRainbow : public CComponent
{
public:
	enum
	{
		MODE_RAINBOW = 1,
		MODE_PULSE,
		MODE_DARKNESS,
		MODE_RANDOM,
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;

	ColorRGBA m_RainbowColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);

private:
	float m_Time = 0.0f;
	float m_LastSwap = -1.0f;
	ColorHSLA m_From = ColorHSLA(0.0f, 1.0f, 0.5f, 1.0f);
	ColorHSLA m_To = ColorHSLA(0.0f, 1.0f, 0.5f, 1.0f);
	bool m_HasTarget = false;
};

#endif
