#include "transition.h"

#include <base/time.h>

#include <engine/client.h>
#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/xui/theme.h>
#include <game/layers.h>
#include <game/localization.h>
#include <game/mapitems.h>

#include <algorithm>
#include <chrono>
#include <cmath>

static constexpr float REFERENCE_HEIGHT = 900.0f;
static constexpr float BLUR_RADIUS = 28.0f;
static constexpr float BRIGHTNESS = 0.55f;
static constexpr float SCENE_SCALE = 1.3f;
static constexpr float FADE_IN_TIME = 0.7f;
static constexpr float REVEAL_BLUR_TIME = 0.45f;
static constexpr float REVEAL_ZOOM_TIME = 0.55f;
static constexpr float REVEAL_UI_TIME = 0.3f;
static constexpr float RETURN_TIME = 0.9f;
static const char *const ACTIVITY_ID = "connect";

static float EaseOut(float T)
{
	T = std::clamp(T, 0.0f, 1.0f);
	return 1.0f - std::pow(1.0f - T, 3.0f);
}

float CTransition::Now() const
{
	return std::chrono::duration<float>(time_get_nanoseconds()).count();
}

void CTransition::OnStateChange(int NewState, int OldState)
{
	const bool Connecting = NewState == IClient::STATE_CONNECTING || NewState == IClient::STATE_LOADING;
	if(Connecting && m_Phase != EPhase::LOADING)
	{
		m_Phase = EPhase::LOADING;
		m_PhaseStart = Now();
		m_MapReady = false;
	}
	else if(NewState == IClient::STATE_DEMOPLAYBACK)
	{
		GameClient()->m_Island.ClearActivity(ACTIVITY_ID);
		m_Phase = EPhase::GAME;
		m_MapReady = false;
		GameClient()->m_Xui.SetMasterTransform(1.0f, 1.0f);
	}
	else if(NewState == IClient::STATE_ONLINE && OldState != IClient::STATE_ONLINE)
	{
		GameClient()->m_Island.ClearActivity(ACTIVITY_ID);
		if(m_Phase == EPhase::LOADING)
		{
			m_Phase = EPhase::REVEAL;
			m_PhaseStart = Now();
			CIsland::SNotification Notification;
			Notification.m_pIcon = FontIcon::NETWORK_WIRED;
			Notification.m_Title = Localize("Connected");
			Notification.m_Subtitle = Client()->ConnectAddressString();
			Notification.m_Color = XuiTheme::Color::GREEN;
			GameClient()->m_Island.Push(Notification);

			m_ZoomEase = m_MapReady && GameClient()->m_Camera.ZoomAllowed();
			if(m_ZoomEase)
			{
				const float Target = GameClient()->m_Camera.m_Zoom;
				GameClient()->m_Camera.m_Zoom = Target / SCENE_SCALE;
				GameClient()->m_Camera.SetZoom(Target, (int)(REVEAL_ZOOM_TIME * 1000.0f), false);
			}
		}
		else
		{
			m_Phase = EPhase::GAME;
		}
	}
	else if(NewState == IClient::STATE_OFFLINE)
	{
		GameClient()->m_Island.ClearActivity(ACTIVITY_ID);
		if(OldState == IClient::STATE_ONLINE)
			m_ReturnStart = Now();
		m_Phase = EPhase::MENU;
		m_MapReady = false;
	}
}

void CTransition::OnMapLoaded()
{
	if(m_Phase != EPhase::LOADING)
		return;

	m_MapReady = true;
	m_MapReadyTime = Now();

	m_MapCenter = vec2(0.0f, 0.0f);
	CLayers *pLayers = GameClient()->Layers();
	CMapItemLayerTilemap *pGameLayer = pLayers->GameLayer();
	if(pGameLayer == nullptr)
		return;
	const CTile *pTiles = static_cast<const CTile *>(pLayers->Map()->GetData(pGameLayer->m_Data));
	if(pTiles == nullptr)
		return;
	const int Width = pGameLayer->m_Width;
	const int Height = pGameLayer->m_Height;
	for(int i = 0; i < Width * Height; i++)
	{
		const int Index = pTiles[i].m_Index;
		if(Index >= ENTITY_OFFSET + ENTITY_SPAWN && Index <= ENTITY_OFFSET + ENTITY_SPAWN_BLUE)
		{
			m_MapCenter = vec2((i % Width) * 32.0f + 16.0f, (i / Width) * 32.0f + 16.0f);
			return;
		}
	}
	m_MapCenter = vec2(Width * 16.0f, Height * 16.0f);
}

bool CTransition::WorldBehindMenu() const
{
	return m_Phase == EPhase::LOADING && m_MapReady;
}

float CTransition::MapZoom() const
{
	if(!GameClient()->m_Camera.ZoomAllowed())
		return 1.0f;
	return 1.0f / SCENE_SCALE;
}

void CTransition::UpdateActivity()
{
	if(m_Phase != EPhase::LOADING)
		return;

	CIsland::SActivity Activity;
	Activity.m_pIcon = FontIcon::NETWORK_WIRED;
	Activity.m_Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_XxUiAccent));
	Activity.m_Priority = CIsland::PRIORITY_HIGH;
	const int Total = Client()->MapDownloadTotalsize();
	if(Total > 0 && Client()->MapDownloadAmount() < Total)
	{
		Activity.m_Text = Localize("Downloading map");
		Activity.m_Progress = (float)Client()->MapDownloadAmount() / (float)Total;
	}
	else if(m_MapReady)
		Activity.m_Text = Localize("Entering map");
	else if(Client()->State() == IClient::STATE_LOADING)
		Activity.m_Text = Localize("Loading map");
	else
		Activity.m_Text = Localize("Connecting");
	GameClient()->m_Island.SetActivity(ACTIVITY_ID, Activity);
}

void CTransition::RenderBlur(float Radius, float Dim)
{
	const float Height = REFERENCE_HEIGHT;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreenToSize(Width, Height);
	Graphics()->BlurBackdrop(std::max(Radius, 6.0f), std::min(1.0f, Radius / 6.0f));
	if(Dim > 0.0f)
	{
		IGraphics::SUiRect Rect;
		Rect.m_X = -2.0f;
		Rect.m_Y = -2.0f;
		Rect.m_W = Width + 4.0f;
		Rect.m_H = Height + 4.0f;
		Rect.m_Fill = ColorRGBA(0.0f, 0.0f, 0.0f, Dim);
		Graphics()->DrawUiRects(&Rect, 1);
	}
}

void CTransition::OnRender()
{
	float MasterAlpha = 1.0f;
	float MasterScale = 1.0f;

	if(m_Phase == EPhase::LOADING)
	{
		UpdateActivity();
		if(m_MapReady)
		{
			const float T = EaseOut((Now() - m_MapReadyTime) / FADE_IN_TIME);
			RenderBlur(BLUR_RADIUS, (1.0f - BRIGHTNESS) * T);
		}
	}
	else if(m_Phase == EPhase::REVEAL)
	{
		const float Blur = 1.0f - EaseOut(Elapsed() / REVEAL_BLUR_TIME);
		const float Ui = EaseOut(Elapsed() / REVEAL_UI_TIME);
		RenderBlur(BLUR_RADIUS * Blur, (1.0f - BRIGHTNESS) * Blur);
		MasterAlpha = 1.0f - Ui;
		MasterScale = 1.0f + 0.05f * Ui;
		if(MasterAlpha > 0.01f)
		{
			GameClient()->m_Xui.SetMasterTransform(MasterScale, MasterAlpha);
			GameClient()->m_Xui.RenderShell(GameClient()->m_Menus);
		}
		else if(Elapsed() > REVEAL_BLUR_TIME)
			m_Phase = EPhase::GAME;
	}
	else if(m_Phase == EPhase::MENU)
	{
		const float T = EaseOut((Now() - m_ReturnStart) / RETURN_TIME);
		MasterAlpha = T;
		MasterScale = 1.05f - 0.05f * T;
	}

	if(m_Phase != EPhase::REVEAL)
		GameClient()->m_Xui.SetMasterTransform(MasterScale, MasterAlpha);
}
