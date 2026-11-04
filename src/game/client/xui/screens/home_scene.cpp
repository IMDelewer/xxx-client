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
	SBox Background;
	Background.m_Position = EPos::ABSOLUTE;
	Background.m_Left = Background.m_Top = 0.0f;
	Background.m_W = Px(Size.x);
	Background.m_H = Px(Size.y);
	Ui.Paint(Ui.Row(Background), [this, pGame](CXui &U, const SRect &Rc) {
		U.DrawDirect(Rc, [this, pGame](const SRect &Px) {
			Draw(*pGame, vec2(Px.m_X, Px.m_Y), vec2(Px.m_W, Px.m_H));
		});
	});
}

void CXui::RenderBackground(vec2 Size)
{
	m_HomeScene.Draw(*GameClient(), vec2(0, 0), Size);
}

void CHomeScene::Draw(CGameClient &Game, vec2 Position, vec2 Size)
{
	IGraphics *pGraphics = Game.Graphics();
	const std::string Path = g_Config.m_XxUiBackground;
	if(m_Loaded && m_LoadedPath != Path)
		Unload(pGraphics);
	if(!m_Loaded)
	{
		CImageInfo Image;
		const bool Custom = !Path.empty() && pGraphics->LoadPng(Image, Path.c_str(), IStorage::TYPE_ALL_OR_ABSOLUTE);
		if(Custom || pGraphics->LoadPng(Image, "xxx/menu_world.png", IStorage::TYPE_ALL))
		{
			m_ImageSize = vec2(Image.m_Width, Image.m_Height);
			ConvertToRgba(Image);
			m_Background = pGraphics->LoadTextureRawMove(Image, 0, "menu background");
		}
		m_Loaded = true;
		m_LoadedPath = Path;
	}
	if(!m_Background.IsValid())
	{
		IGraphics::SUiRect Background = CXui::SolidRect(XuiTheme::Color::BACKGROUND, 0);
		Background.m_X = Position.x;
		Background.m_Y = Position.y;
		Background.m_W = Size.x;
		Background.m_H = Size.y;
		pGraphics->DrawUiRects(&Background, 1);
		return;
	}
	const vec2 ImageSize = m_ImageSize;
	const float Scale = std::max(Size.x / ImageSize.x, Size.y / ImageSize.y);
	const vec2 Offset = Position + (Size - ImageSize * Scale) * 0.5f;
	pGraphics->TextureSet(m_Background);
	pGraphics->QuadsBegin();
	pGraphics->SetColor(1, 1, 1, 1);
	const IGraphics::CQuadItem Quad(Offset.x, Offset.y, ImageSize.x * Scale, ImageSize.y * Scale);
	pGraphics->QuadsDrawTL(&Quad, 1);
	pGraphics->QuadsEnd();
}
