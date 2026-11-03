#include "rainbow.h"

#include <engine/shared/config.h>

#include <game/client/gameclient.h>

#include <cmath>
#include <cstdlib>

static float RandomUnit()
{
	return (float)rand() / (float)RAND_MAX;
}

void CRainbow::OnRender()
{
	if(!g_Config.m_XxRainbowTees && !g_Config.m_XxRainbowWeapon && !g_Config.m_XxRainbowHook)
		return;

	m_Time += Client()->RenderFrameTime() * ((float)g_Config.m_XxRainbowSpeed / 100.0f);
	const float Phase = std::fmod(m_Time, 1.0f);

	ColorRGBA Color;
	switch(g_Config.m_XxRainbowMode)
	{
	case MODE_PULSE:
		Color = color_cast<ColorRGBA>(ColorHSLA(std::fmod(std::floor(m_Time) * 0.1f, 1.0f), 1.0f, 0.5f + std::fabs(Phase - 0.5f)));
		break;
	case MODE_DARKNESS:
		Color = ColorRGBA(0.0f, 0.0f, 0.0f);
		break;
	case MODE_RANDOM:
	{
		if(!m_HasTarget)
		{
			m_HasTarget = true;
			m_To = ColorHSLA(RandomUnit(), 1.0f, RandomUnit(), 1.0f);
		}
		if(m_Time - m_LastSwap > 1.0f)
		{
			m_LastSwap = m_Time;
			m_From = m_To;
			m_To = ColorHSLA(RandomUnit(), 1.0f, RandomUnit(), 1.0f);
		}
		const ColorHSLA Mixed(mix(m_From.h, m_To.h, Phase), mix(m_From.s, m_To.s, Phase), mix(m_From.l, m_To.l, Phase), 1.0f);
		Color = color_cast<ColorRGBA>(Mixed);
		break;
	}
	default:
		Color = color_cast<ColorRGBA>(ColorHSLA(Phase, 1.0f, 0.5f));
		break;
	}
	m_RainbowColor = Color;

	if(!g_Config.m_XxRainbowTees)
		return;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!GameClient()->m_Snap.m_aCharacters[i].m_Active)
			continue;
		if(GameClient()->m_Snap.m_LocalClientId != i && !g_Config.m_XxRainbowOthers)
			continue;
		CTeeRenderInfo &Info = GameClient()->m_aClients[i].m_RenderInfo;
		Info.m_BloodColor = Color;
		Info.m_ColorBody = Color;
		Info.m_ColorFeet = Color;
		Info.m_CustomColoredSkin = true;
	}
}
