/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "spectator.h"

#include "camera.h"

#include <base/dbg.h>
#include <base/log.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <limits>

bool CSpectator::CanChangeSpectatorId()
{
	// don't change SpectatorId when not spectating
	if(!GameClient()->m_Snap.m_SpecInfo.m_Active)
		return false;

	// stop follow mode from changing SpectatorId
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK && GameClient()->m_DemoSpecId == SPEC_FOLLOW)
		return false;

	return true;
}

void CSpectator::SpectateNext(bool Reverse)
{
	int CurIndex = -1;
	const CNetObj_PlayerInfo **paPlayerInfos = GameClient()->m_Snap.m_apInfoByDDTeamName;

	// m_SpectatorId may be uninitialized if m_Active is false
	if(GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(paPlayerInfos[i] && paPlayerInfos[i]->m_ClientId == GameClient()->m_Snap.m_SpecInfo.m_SpectatorId)
			{
				CurIndex = i;
				break;
			}
		}
	}

	int Start;
	if(CurIndex != -1)
	{
		if(Reverse)
			Start = CurIndex - 1;
		else
			Start = CurIndex + 1;
	}
	else
	{
		if(Reverse)
			Start = -1;
		else
			Start = 0;
	}

	int Increment = Reverse ? -1 : 1;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		int PlayerIndex = (Start + i * Increment) % MAX_CLIENTS;
		// % in C++ takes the sign of the dividend, not divisor
		if(PlayerIndex < 0)
			PlayerIndex += MAX_CLIENTS;

		const CNetObj_PlayerInfo *pPlayerInfo = paPlayerInfos[PlayerIndex];
		if(pPlayerInfo && pPlayerInfo->m_Team != TEAM_SPECTATORS)
		{
			Spectate(pPlayerInfo->m_ClientId);
			break;
		}
	}
}

void CSpectator::ConKeySpectator(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;

	if(pSelf->GameClient()->m_Scoreboard.IsActive())
		return;

	if(pSelf->GameClient()->m_Snap.m_SpecInfo.m_Active || pSelf->Client()->State() == IClient::STATE_DEMOPLAYBACK)
		pSelf->m_Active = pResult->GetInteger(0) != 0;
	else
		pSelf->m_Active = false;
}

void CSpectator::ConSpectate(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	if(!pSelf->CanChangeSpectatorId())
		return;

	const int SpectatorId = pResult->GetInteger(0);
	if(SpectatorId != SPEC_FREEVIEW &&
		SpectatorId != SPEC_FOLLOW &&
		!in_range(SpectatorId, 0, (int)MAX_CLIENTS - 1))
	{
		log_error("spectator", "Invalid spectator ID: %d", SpectatorId);
		return;
	}
	pSelf->Spectate(SpectatorId);
}

void CSpectator::ConSpectateNext(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	if(!pSelf->CanChangeSpectatorId())
		return;

	pSelf->SpectateNext(false);
}

void CSpectator::ConSpectatePrevious(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	if(!pSelf->CanChangeSpectatorId())
		return;

	pSelf->SpectateNext(true);
}

void CSpectator::ConSpectateClosest(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	pSelf->SpectateClosest();
}

void CSpectator::ConMultiView(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	int Input = pResult->GetInteger(0);

	if(Input == -1)
		std::fill(std::begin(pSelf->GameClient()->m_aMultiViewId), std::end(pSelf->GameClient()->m_aMultiViewId), false); // remove everyone from multiview
	else if(Input < MAX_CLIENTS && Input >= 0)
		pSelf->GameClient()->m_aMultiViewId[Input] = !pSelf->GameClient()->m_aMultiViewId[Input]; // activate or deactivate one player from multiview
}

CSpectator::CSpectator()
{
	m_SelectorMouse = vec2(0.0f, 0.0f);
	OnReset();
}

void CSpectator::OnConsoleInit()
{
	Console()->Register("+spectate", "", CFGFLAG_CLIENT, ConKeySpectator, this, "Open spectator mode selector");
	Console()->Register("spectate", "i[spectator-id]", CFGFLAG_CLIENT, ConSpectate, this, "Switch spectator mode");
	Console()->Register("spectate_next", "", CFGFLAG_CLIENT, ConSpectateNext, this, "Spectate the next player");
	Console()->Register("spectate_previous", "", CFGFLAG_CLIENT, ConSpectatePrevious, this, "Spectate the previous player");
	Console()->Register("spectate_closest", "", CFGFLAG_CLIENT, ConSpectateClosest, this, "Spectate the closest player");
	Console()->Register("spectate_multiview", "i[id]", CFGFLAG_CLIENT, ConMultiView, this, "Add/remove Client-IDs to spectate them exclusively (-1 to reset)");
}

bool CSpectator::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_Active)
		return false;

	Ui()->ConvertMouseMove(&x, &y, CursorType);
	Ui()->OnCursorMove(x, y);
	return true;
}

bool CSpectator::OnInput(const IInput::CEvent &Event)
{
	if(IsActive() && (GameClient()->m_Xui.ProfileOpen() || (GameClient()->m_Xui.ContextOpen() && Event.m_Key == KEY_ESCAPE)))
		return GameClient()->m_Xui.OnInput(Event);
	if(IsActive() && Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		OnRelease();
		return true;
	}

	if(IsActive())
	{
		if(Event.m_Key > KEY_UNKNOWN && Event.m_Key < KEY_LAST && str_comp(GameClient()->m_Binds.Get(Event.m_Key, CBinds::GetModifierMask(Input())), "+spectate") == 0)
		{
			Console()->ExecuteLineStroked((Event.m_Flags & IInput::FLAG_PRESS) != 0, "+spectate", IConsole::CLIENT_ID_UNSPECIFIED);
			return true;
		}
		return GameClient()->m_Xui.OnInput(Event);
	}

	if(g_Config.m_ClSpectatorMouseclicks)
	{
		if(GameClient()->m_Snap.m_SpecInfo.m_Active && !IsActive() && !GameClient()->m_MultiViewActivated &&
			!Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive() && !GameClient()->m_Menus.IsActive())
		{
			if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_MOUSE_1)
			{
				if(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW)
					Spectate(SPEC_FREEVIEW);
				else
					SpectateClosest();
				return true;
			}
		}
	}

	if(GameClient()->m_Camera.SpectatingPlayer() && GameClient()->m_Camera.CanUseAutoSpecCamera())
	{
		if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_MOUSE_2)
		{
			GameClient()->m_Camera.ResetAutoSpecCamera();
			return true;
		}
	}

	return false;
}

void CSpectator::OnRelease()
{
	OnReset();
}

void CSpectator::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(!GameClient()->m_MultiViewActivated && m_MultiViewActivateDelay != 0.0f)
	{
		if(m_MultiViewActivateDelay <= Client()->LocalTime())
		{
			m_MultiViewActivateDelay = 0.0f;
			GameClient()->m_MultiViewActivated = true;
		}
	}

	if(!m_Active)
	{
		// closing the spectator menu
		if(m_WasActive)
		{
			if(m_SelectedSpectatorId != NO_SELECTION)
			{
				if(m_SelectedSpectatorId == MULTI_VIEW)
					GameClient()->m_MultiViewActivated = true;
				else if(m_SelectedSpectatorId == SPEC_FREEVIEW || m_SelectedSpectatorId == SPEC_FOLLOW)
					GameClient()->m_MultiViewActivated = false;

				if(!GameClient()->m_MultiViewActivated)
					Spectate(m_SelectedSpectatorId);

				if(GameClient()->m_MultiViewActivated && m_SelectedSpectatorId != MULTI_VIEW && GameClient()->m_Teams.Team(m_SelectedSpectatorId) != GameClient()->m_MultiViewTeam)
				{
					GameClient()->ResetMultiView();
					Spectate(m_SelectedSpectatorId);
					m_MultiViewActivateDelay = Client()->LocalTime() + 0.3f;
				}
			}
			m_WasActive = false;
		}
		return;
	}

	if((!GameClient()->m_Snap.m_SpecInfo.m_Active && Client()->State() != IClient::STATE_DEMOPLAYBACK) ||
		GameClient()->m_Scoreboard.IsActive())
	{
		OnRelease();
		return;
	}

	m_WasActive = true;
	m_SelectedSpectatorId = NO_SELECTION;

	m_SelectedSpectatorId = GameClient()->m_Xui.RenderSpectator();
}

void CSpectator::OnReset()
{
	m_WasActive = false;
	m_Active = false;
	m_SelectedSpectatorId = NO_SELECTION;
	m_TouchState = CUi::CTouchState();
	m_MultiViewActivateDelay = 0.0f;
}

void CSpectator::Spectate(int SpectatorId)
{
	dbg_assert(SpectatorId == SPEC_FREEVIEW || SpectatorId == SPEC_FOLLOW || in_range(SpectatorId, 0, (int)MAX_CLIENTS - 1), "Invalid SpectatorId: %d", SpectatorId);

	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		GameClient()->m_DemoSpecId = SpectatorId;
		// The tick must be rendered for the spectator mode to be updated, so we do it manually when demo playback is paused
		// TODO: https://github.com/ddnet/ddnet/issues/11681
		if(DemoPlayer()->BaseInfo()->m_Paused)
			GameClient()->m_Menus.DemoSeekTick(IDemoPlayer::TICK_CURRENT);
		return;
	}

	if(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == SpectatorId)
		return;

	if(Client()->IsSixup())
	{
		protocol7::CNetMsg_Cl_SetSpectatorMode Msg;
		if(SpectatorId == SPEC_FREEVIEW)
		{
			Msg.m_SpecMode = protocol7::SPEC_FREEVIEW;
			Msg.m_SpectatorId = -1;
		}
		else
		{
			Msg.m_SpecMode = protocol7::SPEC_PLAYER;
			Msg.m_SpectatorId = SpectatorId;
		}
		Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL, true);
		return;
	}
	CNetMsg_Cl_SetSpectatorMode Msg;
	Msg.m_SpectatorId = SpectatorId;
	Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);
}

void CSpectator::SpectateClosest()
{
	if(!CanChangeSpectatorId())
		return;

	const CGameClient::CSnapState &Snap = GameClient()->m_Snap;
	int SpectatorId = Snap.m_SpecInfo.m_SpectatorId;

	int NewSpectatorId = -1;

	vec2 CurPosition = GameClient()->m_Camera.m_Center;
	if(SpectatorId != SPEC_FREEVIEW && Snap.m_aCharacters[SpectatorId].m_Active)
	{
		const CNetObj_Character &CurCharacter = Snap.m_aCharacters[SpectatorId].m_Cur;
		CurPosition = vec2(CurCharacter.m_X, CurCharacter.m_Y);
	}

	int ClosestDistance = std::numeric_limits<int>::max();
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(ClientId == SpectatorId || !Snap.m_aCharacters[ClientId].m_Active || !Snap.m_apPlayerInfos[ClientId] || Snap.m_apPlayerInfos[ClientId]->m_Team == TEAM_SPECTATORS)
			continue;

		if(Client()->State() != IClient::STATE_DEMOPLAYBACK && ClientId == Snap.m_LocalClientId)
			continue;

		const CNetObj_Character &MaybeClosestCharacter = Snap.m_aCharacters[ClientId].m_Cur;
		int Distance = distance(CurPosition, vec2(MaybeClosestCharacter.m_X, MaybeClosestCharacter.m_Y));
		if(NewSpectatorId == -1 || Distance < ClosestDistance)
		{
			NewSpectatorId = ClientId;
			ClosestDistance = Distance;
		}
	}
	if(NewSpectatorId > -1)
		Spectate(NewSpectatorId);
}
