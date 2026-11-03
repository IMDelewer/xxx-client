/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/dbg.h>
#include <base/log.h>
#include <base/time.h>

#include <engine/engine.h>
#include <engine/favorites.h>
#include <engine/font_icons.h>
#include <engine/friends.h>
#include <engine/gfx/image_manipulation.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/shared/localization.h>
#include <engine/textrender.h>

#include <game/client/animstate.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/localization.h>

void CMenus::Connect(const char *pAddress)
{
	if(Client()->State() == IClient::STATE_ONLINE && GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmDisconnectTime && g_Config.m_ClConfirmDisconnectTime >= 0)
	{
		// The popup is only rendered while the menus are active
		SetActive(true);
		str_copy(m_aNextServer, pAddress);
		PopupConfirm(Localize("Disconnect"), Localize("Are you sure that you want to disconnect and switch to a different server?"), Localize("Yes"), Localize("No"), &CMenus::PopupConfirmSwitchServer);
	}
	else
	{
		Client()->Connect(pAddress);
	}
}

void CMenus::PopupConfirmSwitchServer()
{
	Client()->Connect(m_aNextServer);
}

void CMenus::UpdateLastServer()
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	const NETADDR &Server = Client()->ServerAddress();
	char aIp[NETADDR_MAXSTRSIZE];
	net_addr_str(&Server, aIp, sizeof(aIp), true);
	char aAddress[sizeof(g_Config.m_XxLastServerAddress)];
	str_format(aAddress, sizeof(aAddress), "%s%s", (Server.type & NETTYPE_TW7) ? "tw-0.7+udp://" : "", aIp);
	if(str_comp(aAddress, g_Config.m_XxLastServerAddress) != 0)
	{
		str_copy(g_Config.m_XxLastServerAddress, aAddress);
		g_Config.m_XxLastServerName[0] = '\0';
		g_Config.m_XxLastServerMap[0] = '\0';
	}
	const CServerInfo &Info = Client()->ServerInfo();
	if(Info.m_aName[0])
		str_copy(g_Config.m_XxLastServerName, Info.m_aName);
	if(Info.m_aMap[0])
		str_copy(g_Config.m_XxLastServerMap, Info.m_aMap);
}

void CMenus::ResetServerbrowserFilters()
{
	g_Config.m_BrFilterString[0] = '\0';
	g_Config.m_BrExcludeString[0] = '\0';
	g_Config.m_BrFilterFull = 0;
	g_Config.m_BrFilterEmpty = 0;
	g_Config.m_BrFilterSpectators = 0;
	g_Config.m_BrFilterFriends = 0;
	g_Config.m_BrFilterCountry = 0;
	g_Config.m_BrFilterCountryIndex = DefaultConfig::BrFilterCountryIndex;
	g_Config.m_BrFilterPw = 0;
	g_Config.m_BrFilterGametype[0] = '\0';
	g_Config.m_BrFilterGametypeStrict = 0;
	g_Config.m_BrFilterConnectingPlayers = 1;
	g_Config.m_BrFilterServerAddress[0] = '\0';
	g_Config.m_BrFilterLogin = true;

	if(g_Config.m_UiPage != PAGE_LAN)
	{
		if(ServerBrowser()->CommunityCache().AnyRanksAvailable())
		{
			g_Config.m_BrFilterUnfinishedMap = 0;
		}
		if(g_Config.m_UiPage == PAGE_INTERNET || g_Config.m_UiPage == PAGE_FAVORITES)
		{
			ServerBrowser()->CommunitiesFilter().Clear();
		}
		ServerBrowser()->CountriesFilter().Clear();
		ServerBrowser()->TypesFilter().Clear();
		UpdateCommunityCache(true);
	}

	Client()->ServerBrowserUpdate();
}

CUi::EPopupMenuFunctionResult CMenus::PopupCountrySelection(void *pContext, CUIRect View, bool Active)
{
	SPopupCountrySelectionContext *pPopupContext = static_cast<SPopupCountrySelectionContext *>(pContext);
	CMenus *pMenus = pPopupContext->m_pMenus;

	static CListBox s_ListBox;
	s_ListBox.SetActive(Active);
	s_ListBox.DoStart(50.0f, pMenus->GameClient()->m_CountryFlags.Num(), 8, 1, -1, &View, false);

	if(pPopupContext->m_New)
	{
		pPopupContext->m_New = false;
		s_ListBox.ScrollToSelected();
	}

	for(size_t i = 0; i < pMenus->GameClient()->m_CountryFlags.Num(); ++i)
	{
		const CCountryFlags::CCountryFlag &Entry = pMenus->GameClient()->m_CountryFlags.GetByIndex(i);

		const CListboxItem Item = s_ListBox.DoNextItem(&Entry, Entry.m_CountryCode == pPopupContext->m_Selection);
		if(!Item.m_Visible)
			continue;

		CUIRect FlagRect, Label;
		Item.m_Rect.Margin(5.0f, &FlagRect);
		FlagRect.HSplitBottom(12.0f, &FlagRect, &Label);
		Label.HSplitTop(2.0f, nullptr, &Label);
		const float OldWidth = FlagRect.w;
		FlagRect.w = FlagRect.h * 2.0f;
		FlagRect.x += (OldWidth - FlagRect.w) / 2.0f;
		pMenus->GameClient()->m_CountryFlags.Render(Entry.m_CountryCode, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), FlagRect.x, FlagRect.y, FlagRect.w, FlagRect.h);

		pMenus->Ui()->DoLabel(&Label, Entry.m_aCountryCodeString, 10.0f, TEXTALIGN_MC);
	}

	const int NewSelected = s_ListBox.DoEnd();
	pPopupContext->m_Selection = NewSelected >= 0 ? pMenus->GameClient()->m_CountryFlags.GetByIndex(NewSelected).m_CountryCode : CountryCode::DEFAULT;
	if(s_ListBox.WasItemSelected() || s_ListBox.WasItemActivated())
	{
		g_Config.m_BrFilterCountry = 1;
		g_Config.m_BrFilterCountryIndex = pPopupContext->m_Selection;
		pMenus->Client()->ServerBrowserUpdate();
		return CUi::POPUP_CLOSE_CURRENT;
	}

	return CUi::POPUP_KEEP_OPEN;
}

void CMenus::FriendlistOnUpdate()
{
	// TODO: friends are currently updated every frame; optimize and only update friends when necessary
}

void CMenus::PopupConfirmRemoveFriend()
{
	GameClient()->Friends()->RemoveFriend(m_pRemoveFriend->FriendState() == IFriends::FRIEND_PLAYER ? m_pRemoveFriend->Name() : "", m_pRemoveFriend->Clan());
	FriendlistOnUpdate();
	Client()->ServerBrowserUpdate();
	m_pRemoveFriend = nullptr;
}

void CMenus::ConchainFriendlistUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CMenus *pThis = ((CMenus *)pUserData);
	if(pResult->NumArguments() >= 1 && (pThis->Client()->State() == IClient::STATE_OFFLINE || pThis->Client()->State() == IClient::STATE_ONLINE))
	{
		pThis->FriendlistOnUpdate();
		pThis->Client()->ServerBrowserUpdate();
	}
}

void CMenus::ConchainFavoritesUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() >= 1 && g_Config.m_UiPage == PAGE_FAVORITES)
		((CMenus *)pUserData)->ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
}

void CMenus::ConchainCommunitiesUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CMenus *pThis = static_cast<CMenus *>(pUserData);
	if(pResult->NumArguments() >= 1 && (g_Config.m_UiPage == PAGE_INTERNET || g_Config.m_UiPage == PAGE_FAVORITES || (g_Config.m_UiPage >= PAGE_FAVORITE_COMMUNITY_1 && g_Config.m_UiPage <= PAGE_FAVORITE_COMMUNITY_5)))
	{
		pThis->UpdateCommunityCache(true);
		pThis->Client()->ServerBrowserUpdate();
	}
}

void CMenus::ConchainUiPageUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CMenus *pThis = static_cast<CMenus *>(pUserData);
	if(pResult->NumArguments() >= 1)
	{
		if(g_Config.m_UiPage >= PAGE_FAVORITE_COMMUNITY_1 && g_Config.m_UiPage <= PAGE_FAVORITE_COMMUNITY_5 &&
			(size_t)(g_Config.m_UiPage - PAGE_FAVORITE_COMMUNITY_1) >= pThis->ServerBrowser()->FavoriteCommunities().size())
		{
			// Reset page to internet when there is no favorite community for this page.
			g_Config.m_UiPage = PAGE_INTERNET;
		}

		pThis->SetMenuPage(g_Config.m_UiPage);
	}
}

void CMenus::UpdateCommunityCache(bool Force)
{
	if(g_Config.m_UiPage >= PAGE_FAVORITE_COMMUNITY_1 && g_Config.m_UiPage <= PAGE_FAVORITE_COMMUNITY_5 &&
		(size_t)(g_Config.m_UiPage - PAGE_FAVORITE_COMMUNITY_1) >= ServerBrowser()->FavoriteCommunities().size())
	{
		// Reset page to internet when there is no favorite community for this page,
		// i.e. when favorite community is removed via console while the page is open.
		// This also updates the community cache because the page is changed.
		SetMenuPage(PAGE_INTERNET);
	}
	else
	{
		ServerBrowser()->CommunityCache().Update(Force);
	}
}
