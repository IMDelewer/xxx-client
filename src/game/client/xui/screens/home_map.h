#ifndef GAME_CLIENT_XUI_SCREENS_HOME_MAP_H
#define GAME_CLIENT_XUI_SCREENS_HOME_MAP_H

#include <base/vmath.h>

#include <array>

namespace HomeMap
{
	constexpr float WIDTH = 1672.0f;
	constexpr float HEIGHT = 941.0f;
	constexpr float TEE_SIZE = 42.0f;
	struct SPlatform
	{
		float m_Left, m_Right, m_Surface, m_Spawn;
		vec2 m_Hook;
		float m_JumpHeight;
	};
	// Coordinates in menu_meadow.png. Walking routes stay inside their platform.
	inline const std::array<SPlatform, 8> PLATFORMS = {{
		{860, 937, 438, 900, vec2(958, 414), 22},
		{728, 830, 600, 785, vec2(900, 504), 28},
		{915, 1038, 704, 975, vec2(1024, 574), 24},
		{1149, 1227, 267, 1190, vec2(1238, 267), 22},
		{1185, 1271, 462, 1228, vec2(1208, 370), 24},
		{1402, 1466, 464, 1438, vec2(1440, 334), 22},
		{1055, 1125, 319, 1090, vec2(1140, 279), 22},
		{970, 1013, 402, 990, vec2(1040, 345), 20},
	}};
	struct SForbidden
	{
		float m_X, m_Y, m_W, m_H;
	};
	inline constexpr std::array<SForbidden, 7> FORBIDDEN = {{
		{550, 744, 158, 30},
		{1328, 644, 83, 31},
		{220, 780, 140, 82},
		{929, 849, 125, 77},
		{770, 779, 100, 112},
		{1106, 430, 66, 180},
		{1605, 365, 37, 190},
	}};
	inline bool Safe(vec2 Feet)
	{
		for(const auto &Area : FORBIDDEN)
			if(Feet.x >= Area.m_X && Feet.x <= Area.m_X + Area.m_W && Feet.y >= Area.m_Y && Feet.y <= Area.m_Y + Area.m_H)
				return false;
		return true;
	}
}

#endif
