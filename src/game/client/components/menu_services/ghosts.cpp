/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/color.h>
#include <base/dbg.h>
#include <base/math.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/console.h>
#include <engine/demo.h>
#include <engine/favorites.h>
#include <engine/font_icons.h>
#include <engine/friends.h>
#include <engine/ghost.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/shared/localization.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <generated/client_data.h>
#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/ghost.h>
#include <game/client/components/menus.h>
#include <game/client/components/motd.h>
#include <game/client/components/touch_controls.h>
#include <game/client/components/voting.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>
#include <chrono>

using namespace std::chrono_literals;

void CMenus::PopupConfirmDisconnect()
{
	Client()->Disconnect();
}

void CMenus::PopupConfirmDisconnectDummy()
{
	Client()->DummyDisconnect(nullptr);
	SetActive(false);
}

// ghost stuff
int CMenus::GhostlistFetchCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser)
{
	CMenus *pSelf = (CMenus *)pUser;
	const char *pMap = pSelf->GameClient()->Map()->BaseName();
	if(IsDir || !str_endswith(pInfo->m_pName, ".gho") || !str_startswith(pInfo->m_pName, pMap))
		return 0;

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/%s", pSelf->GameClient()->m_Ghost.GetGhostDir(), pInfo->m_pName);

	CGhostInfo Info;
	if(!pSelf->GameClient()->m_Ghost.GhostLoader()->GetGhostInfo(aFilename, &Info, pMap, pSelf->GameClient()->Map()->Sha256(), pSelf->GameClient()->Map()->Crc()))
		return 0;

	CGhostItem Item;
	str_copy(Item.m_aFilename, aFilename);
	str_copy(Item.m_aPlayer, Info.m_aOwner);
	Item.m_Date = pInfo->m_TimeModified;
	Item.m_Time = Info.m_Time;
	if(Item.m_Time > 0)
		pSelf->m_vGhosts.push_back(Item);

	if(time_get_nanoseconds() - pSelf->m_GhostPopulateStartTime > 500ms)
	{
		pSelf->RenderLoading(Localize("Loading ghost files"), "", 0);
	}

	return 0;
}

void CMenus::GhostlistPopulate()
{
	m_vGhosts.clear();
	m_GhostPopulateStartTime = time_get_nanoseconds();
	Storage()->ListDirectoryInfo(IStorage::TYPE_ALL, GameClient()->m_Ghost.GetGhostDir(), GhostlistFetchCallback, this);
	SortGhostlist();

	CGhostItem *pOwnGhost = nullptr;
	for(auto &Ghost : m_vGhosts)
	{
		Ghost.m_Failed = false;
		if(str_comp(Ghost.m_aPlayer, Client()->PlayerName()) == 0 && (!pOwnGhost || Ghost.m_Time < pOwnGhost->m_Time))
			pOwnGhost = &Ghost;
	}

	if(pOwnGhost)
	{
		pOwnGhost->m_Own = true;
		pOwnGhost->m_Slot = GameClient()->m_Ghost.Load(pOwnGhost->m_aFilename);
	}
}

CMenus::CGhostItem *CMenus::GetOwnGhost()
{
	for(auto &Ghost : m_vGhosts)
		if(Ghost.m_Own)
			return &Ghost;
	return nullptr;
}

void CMenus::UpdateOwnGhost(CGhostItem Item)
{
	int Own = -1;
	for(size_t i = 0; i < m_vGhosts.size(); i++)
		if(m_vGhosts[i].m_Own)
			Own = i;

	if(Own == -1)
	{
		Item.m_Own = true;
	}
	else if(g_Config.m_ClRaceGhostSaveBest && (Item.HasFile() || !m_vGhosts[Own].HasFile()))
	{
		Item.m_Own = true;
		DeleteGhostItem(Own);
	}
	else if(m_vGhosts[Own].m_Time > Item.m_Time)
	{
		Item.m_Own = true;
		m_vGhosts[Own].m_Own = false;
		m_vGhosts[Own].m_Slot = -1;
	}
	else
	{
		Item.m_Own = false;
		Item.m_Slot = -1;
	}

	Item.m_Date = std::time(nullptr);
	Item.m_Failed = false;
	m_vGhosts.push_back(Item);
	SortGhostlist();
}

void CMenus::DeleteGhostItem(int Index)
{
	if(m_vGhosts[Index].HasFile())
		Storage()->RemoveFile(m_vGhosts[Index].m_aFilename, IStorage::TYPE_SAVE);
	m_vGhosts.erase(m_vGhosts.begin() + Index);
}

void CMenus::SortGhostlist()
{
	if(g_Config.m_GhSort == GHOST_SORT_NAME)
		std::stable_sort(m_vGhosts.begin(), m_vGhosts.end(), [](const CGhostItem &Left, const CGhostItem &Right) {
			return g_Config.m_GhSortOrder ? (str_comp(Left.m_aPlayer, Right.m_aPlayer) > 0) : (str_comp(Left.m_aPlayer, Right.m_aPlayer) < 0);
		});
	else if(g_Config.m_GhSort == GHOST_SORT_TIME)
		std::stable_sort(m_vGhosts.begin(), m_vGhosts.end(), [](const CGhostItem &Left, const CGhostItem &Right) {
			return g_Config.m_GhSortOrder ? (Left.m_Time > Right.m_Time) : (Left.m_Time < Right.m_Time);
		});
	else if(g_Config.m_GhSort == GHOST_SORT_DATE)
		std::stable_sort(m_vGhosts.begin(), m_vGhosts.end(), [](const CGhostItem &Left, const CGhostItem &Right) {
			return g_Config.m_GhSortOrder ? (Left.m_Date > Right.m_Date) : (Left.m_Date < Right.m_Date);
		});
}
