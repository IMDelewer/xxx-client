#ifndef GAME_CLIENT_XUI_SCREENS_HOME_SCENE_H
#define GAME_CLIENT_XUI_SCREENS_HOME_SCENE_H

#include <engine/graphics.h>
#include <engine/serverbrowser.h>

#include <game/client/render.h>

#include <string>

class CXui;
class CMenus;
class CGameClient;

CTeeRenderInfo HomeFriendRenderInfo(CGameClient &Game, const CServerInfo::CClient &Client);

class CHomeScene
{
	IGraphics::CTextureHandle m_Background;
	bool m_Loaded = false;
	std::string m_LoadedPath;
	vec2 m_ImageSize = vec2(1672, 941);

public:
	void Unload(IGraphics *pGraphics);
	void Render(CXui &Ui, CMenus &Menus, vec2 Size);
	void Draw(CGameClient &Game, vec2 Position, vec2 Size);
};

#endif
