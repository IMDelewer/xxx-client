#include "home_scene.h"

#include <engine/gfx/image_manipulation.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/client/xui/xui.h>

#include <algorithm>

using namespace Xui;

CTeeRenderInfo HomeFriendRenderInfo(CGameClient &Game, const CServerInfo::CClient &Client)
{
	CTeeRenderInfo Info;
	if(Client.m_aSkin[0])
	{
		if(const CSkin *pSkin = Game.m_Skins.Find(Client.m_aSkin))
			Info.Apply(pSkin);
		Info.ApplyColors(Client.m_CustomSkinColors, Client.m_CustomSkinColorBody, Client.m_CustomSkinColorFeet);
	}
	else if(Client.m_aaSkin7[protocol7::SKINPART_BODY][0])
	{
		for(auto &Sixup : Info.m_aSixup)
			for(int Part = 0; Part < protocol7::NUM_SKINPARTS; ++Part)
			{
				Game.m_Skins7.FindSkinPart(Part, Client.m_aaSkin7[Part], false)->ApplyTo(Sixup);
				Game.m_Skins7.ApplyColorTo(Sixup, Client.m_aUseCustomSkinColor7[Part], Client.m_aCustomSkinColor7[Part], Part);
			}
	}
	return Info;
}

void CHomeScene::Unload(IGraphics *pGraphics)
{
	if(m_Background.IsValid())
		pGraphics->UnloadTexture(&m_Background);
	m_Loaded = false;
}

void CHomeScene::Render(CXui &Ui, CMenus &, vec2 Size)
{
	CGameClient *pGame = Ui.Game();
	const std::string Path = g_Config.m_XxUiBackground;
	if(m_Loaded && m_LoadedPath != Path)
		Unload(pGame->Graphics());
	if(!m_Loaded)
	{
		CImageInfo Image;
		const bool Custom = !Path.empty() && pGame->Graphics()->LoadPng(Image, Path.c_str(), IStorage::TYPE_ALL_OR_ABSOLUTE);
		if(Custom || pGame->Graphics()->LoadPng(Image, "xxx/menu_world.png", IStorage::TYPE_ALL))
		{
			m_ImageSize = vec2(Image.m_Width, Image.m_Height);
			ConvertToRgba(Image);
			m_Background = pGame->Graphics()->LoadTextureRawMove(Image, 0, "xxx/menu_world.png");
		}
		m_Loaded = true;
		m_LoadedPath = Path;
	}
	const vec2 ImageSize = m_ImageSize;
	const float Scale = std::max(Size.x / ImageSize.x, Size.y / ImageSize.y);
	const vec2 Offset = (Size - ImageSize * Scale) * 0.5f;
	SBox Background;
	Background.m_Position = EPos::ABSOLUTE;
	Background.m_Left = Offset.x;
	Background.m_Top = Offset.y;
	Background.m_W = Px(ImageSize.x * Scale);
	Background.m_H = Px(ImageSize.y * Scale);
	const auto Texture = m_Background;
	Ui.Paint(Ui.Row(Background), [Texture, pGame](CXui &U, const SRect &Rc) {
		U.DrawDirect(Rc, [Texture, pGame](const SRect &Px) {
			IGraphics *pGraphics = pGame->Graphics();
			pGraphics->TextureSet(Texture);
			pGraphics->QuadsBegin();
			pGraphics->SetColor(1, 1, 1, 1);
			const IGraphics::CQuadItem Quad(Px.m_X, Px.m_Y, Px.m_W, Px.m_H);
			pGraphics->QuadsDrawTL(&Quad, 1);
			pGraphics->QuadsEnd();
		});
	});
}
