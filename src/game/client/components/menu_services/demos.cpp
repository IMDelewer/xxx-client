/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/fs.h>
#include <base/hash.h>
#include <base/io.h>
#include <base/math.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/demo.h>
#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/localization.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <generated/client_data.h>

#include <game/client/components/console.h>
#include <game/client/components/maplayers.h>
#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/localization.h>

#include <chrono>

using namespace std::chrono_literals;

bool CMenus::DemoFilterChat(const void *pData, int Size, void *pUser)
{
	bool DoFilterChat = *(bool *)pUser;
	if(!DoFilterChat)
	{
		return false;
	}

	CUnpacker Unpacker;
	Unpacker.Reset(pData, Size);

	int Msg = Unpacker.GetInt();
	int Sys = Msg & 1;
	Msg >>= 1;

	return !Unpacker.Error() && !Sys && Msg == NETMSGTYPE_SV_CHAT;
}

void CMenus::HandleDemoSeeking(float PositionToSeek, float TimeToSeek)
{
	if((PositionToSeek >= 0.0f && PositionToSeek <= 1.0f) || TimeToSeek != 0.0f)
	{
		GameClient()->m_Chat.Reset();
		GameClient()->m_DamageInd.OnReset();
		GameClient()->m_InfoMessages.OnReset();
		GameClient()->m_Particles.OnReset();
		GameClient()->m_Sounds.OnReset();
		GameClient()->m_Scoreboard.OnReset();
		GameClient()->m_Statboard.OnReset();
		GameClient()->m_SuppressEvents = true;
		if(TimeToSeek != 0.0f)
			DemoPlayer()->SeekTime(TimeToSeek);
		else
			DemoPlayer()->SeekPercent(PositionToSeek);
		GameClient()->m_SuppressEvents = false;

		if(!DemoPlayer()->BaseInfo()->m_Paused &&
			!DemoPlayer()->BaseInfo()->m_LiveDemo &&
			PositionToSeek == 1.0f)
		{
			DemoPlayer()->Pause();
		}
	}
}

void CMenus::DemoSeekTick(IDemoPlayer::ETickOffset TickOffset)
{
	GameClient()->m_SuppressEvents = true;
	DemoPlayer()->SeekTick(TickOffset);
	GameClient()->m_SuppressEvents = false;
	DemoPlayer()->Pause();
}

int CMenus::DemolistFetchCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser)
{
	CMenus *pSelf = (CMenus *)pUser;
	if(str_comp(pInfo->m_pName, ".") == 0 ||
		(str_comp(pInfo->m_pName, "..") == 0 && (pSelf->m_aCurrentDemoFolder[0] == '\0' || (!pSelf->m_DemolistMultipleStorages && str_comp(pSelf->m_aCurrentDemoFolder, "demos") == 0))) ||
		(!IsDir && !str_endswith(pInfo->m_pName, ".demo")))
	{
		return 0;
	}

	CDemoItem Item;
	str_copy(Item.m_aFilename, pInfo->m_pName);
	if(IsDir)
	{
		str_format(Item.m_aName, sizeof(Item.m_aName), "%s/", pInfo->m_pName);
		Item.m_Date = 0;
	}
	else
	{
		str_truncate(Item.m_aName, sizeof(Item.m_aName), pInfo->m_pName, str_length(pInfo->m_pName) - str_length(".demo"));
		Item.m_Date = pInfo->m_TimeModified;
	}
	Item.m_InfosLoaded = false;
	Item.m_Valid = false;
	Item.m_IsDir = IsDir != 0;
	Item.m_IsLink = false;
	Item.m_StorageType = StorageType;
	pSelf->m_vDemos.push_back(Item);

	if(time_get_nanoseconds() - pSelf->m_DemoPopulateStartTime > 500ms)
	{
		pSelf->RenderLoading(Localize("Loading demo files"), "", 0);
	}

	return 0;
}

void CMenus::DemolistPopulate()
{
	m_vDemos.clear();

	int NumStoragesWithDemos = 0;
	for(int StorageType = IStorage::TYPE_SAVE; StorageType < Storage()->NumPaths(); ++StorageType)
	{
		if(Storage()->FolderExists("demos", StorageType))
		{
			NumStoragesWithDemos++;
		}
	}
	m_DemolistMultipleStorages = NumStoragesWithDemos > 1;

	if(m_aCurrentDemoFolder[0] == '\0')
	{
		{
			CDemoItem Item;
			str_copy(Item.m_aFilename, "demos");
			str_copy(Item.m_aName, Localize("All combined"));
			Item.m_InfosLoaded = false;
			Item.m_Valid = false;
			Item.m_Date = 0;
			Item.m_IsDir = true;
			Item.m_IsLink = true;
			Item.m_StorageType = IStorage::TYPE_ALL;
			m_vDemos.push_back(Item);
		}

		for(int StorageType = IStorage::TYPE_SAVE; StorageType < Storage()->NumPaths(); ++StorageType)
		{
			if(Storage()->FolderExists("demos", StorageType))
			{
				CDemoItem Item;
				str_copy(Item.m_aFilename, "demos");
				Storage()->GetCompletePath(StorageType, "demos", Item.m_aName, sizeof(Item.m_aName));
				str_append(Item.m_aName, "/");
				Item.m_InfosLoaded = false;
				Item.m_Valid = false;
				Item.m_Date = 0;
				Item.m_IsDir = true;
				Item.m_IsLink = true;
				Item.m_StorageType = StorageType;
				m_vDemos.push_back(Item);
			}
		}
	}
	else
	{
		m_DemoPopulateStartTime = time_get_nanoseconds();
		Storage()->ListDirectoryInfo(m_DemolistStorageType, m_aCurrentDemoFolder, DemolistFetchCallback, this);

		// Make sure there is a demo item to navigate back to the parent folder, if the folder contents could not be enumerated.
		if(m_vDemos.empty())
		{
			CDemoItem Item;
			str_copy(Item.m_aFilename, "..");
			str_copy(Item.m_aName, "../");
			Item.m_Date = 0;
			Item.m_InfosLoaded = false;
			Item.m_Valid = false;
			Item.m_IsDir = true;
			Item.m_IsLink = false;
			Item.m_StorageType = m_DemolistStorageType;
			m_vDemos.push_back(Item);
		}

		if(g_Config.m_BrDemoFetchInfo)
			FetchAllHeaders();

		std::stable_sort(m_vDemos.begin(), m_vDemos.end());
	}
	RefreshFilteredDemos();
}

void CMenus::RefreshFilteredDemos()
{
	m_vpFilteredDemos.clear();
	for(auto &Demo : m_vDemos)
	{
		if(str_find_nocase(Demo.m_aFilename, m_DemoSearchInput.GetString()))
		{
			m_vpFilteredDemos.push_back(&Demo);
		}
	}
}

void CMenus::DemolistOnUpdate(bool Reset)
{
	if(Reset)
	{
		if(m_vpFilteredDemos.empty())
		{
			m_DemolistSelectedIndex = -1;
			m_aCurrentDemoSelectionName[0] = '\0';
		}
		else
		{
			m_DemolistSelectedIndex = 0;
			str_copy(m_aCurrentDemoSelectionName, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aName);
		}
	}
	else
	{
		RefreshFilteredDemos();

		// search for selected index
		m_DemolistSelectedIndex = -1;
		int SelectedIndex = -1;
		for(const auto &pItem : m_vpFilteredDemos)
		{
			SelectedIndex++;
			if(str_comp(m_aCurrentDemoSelectionName, pItem->m_aName) == 0)
			{
				m_DemolistSelectedIndex = SelectedIndex;
				break;
			}
		}
	}

	if(m_DemolistSelectedIndex >= 0)
		m_DemolistSelectedReveal = true;
}

bool CMenus::FetchHeader(CDemoItem &Item)
{
	if(!Item.m_InfosLoaded)
	{
		char aBuffer[IO_MAX_PATH_LENGTH];
		str_format(aBuffer, sizeof(aBuffer), "%s/%s", m_aCurrentDemoFolder, Item.m_aFilename);
		IOHANDLE File;
		Item.m_Valid = DemoPlayer()->GetDemoInfo(Storage(), aBuffer, Item.m_StorageType, &Item.m_Info, &Item.m_TimelineMarkers, &Item.m_MapInfo, &File);
		Item.m_InfosLoaded = true;

		if(Item.m_Valid && File)
		{
			Item.m_Size = io_length(File);
			io_close(File);
		}
	}
	return Item.m_Valid;
}

void CMenus::FetchAllHeaders()
{
	for(auto &Item : m_vDemos)
	{
		FetchHeader(Item);
	}
	std::stable_sort(m_vDemos.begin(), m_vDemos.end());
}

void CMenus::PopupConfirmPlayDemo()
{
	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename);
	const char *pError = Client()->DemoPlayer_Play(aBuf, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_StorageType);
	m_LastPauseChange = -1.0f;
	m_LastSpeedChange = -1.0f;
	if(pError)
	{
		PopupMessage(Localize("Error loading demo"), pError, Localize("Ok"));
	}
	else
	{
		Ui()->SetActiveItem(nullptr);
		return;
	}
}

void CMenus::DemolistSelectNeighbor()
{
	const int NeighborIndex = m_DemolistSelectedIndex + 1 < (int)m_vpFilteredDemos.size() ? m_DemolistSelectedIndex + 1 : m_DemolistSelectedIndex - 1;
	str_copy(m_aCurrentDemoSelectionName, NeighborIndex >= 0 ? m_vpFilteredDemos[NeighborIndex]->m_aName : "");
}

void CMenus::PopupConfirmDeleteDemo()
{
	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename);
	if(Storage()->RemoveFile(aBuf, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_StorageType))
	{
		DemolistSelectNeighbor();
		DemolistPopulate();
		DemolistOnUpdate(false);
	}
	else
	{
		char aError[128 + IO_MAX_PATH_LENGTH];
		str_format(aError, sizeof(aError), Localize("Unable to delete the demo '%s'"), m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename);
		PopupMessage(Localize("Error"), aError, Localize("Ok"));
	}
}

void CMenus::PopupConfirmDeleteFolder()
{
	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename);
	if(Storage()->RemoveFolder(aBuf, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_StorageType))
	{
		DemolistSelectNeighbor();
		DemolistPopulate();
		DemolistOnUpdate(false);
	}
	else
	{
		char aError[128 + IO_MAX_PATH_LENGTH];
		str_format(aError, sizeof(aError), Localize("Unable to delete the folder '%s'. Make sure it's empty first."), m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename);
		PopupMessage(Localize("Error"), aError, Localize("Ok"));
	}
}

void CMenus::ConchainDemoPlay(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = static_cast<CMenus *>(pUserData);
	pThis->m_LastPauseChange = pThis->Client()->GlobalTime();
	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::ConchainDemoSpeed(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = static_cast<CMenus *>(pUserData);
	if(pResult->NumArguments() == 1)
	{
		pThis->m_LastSpeedChange = pThis->Client()->GlobalTime();
	}
	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::RenameDemo()
{
	m_Popup = POPUP_NONE;
	char aBufOld[IO_MAX_PATH_LENGTH];
	str_format(aBufOld, sizeof(aBufOld), "%s/%s", m_aCurrentDemoFolder, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename);
	char aBufNew[IO_MAX_PATH_LENGTH];
	str_format(aBufNew, sizeof(aBufNew), "%s/%s", m_aCurrentDemoFolder, m_DemoRenameInput.GetString());
	if(!m_vpFilteredDemos[m_DemolistSelectedIndex]->m_IsDir && !str_endswith(aBufNew, ".demo"))
		str_append(aBufNew, ".demo");

	if(str_comp(aBufOld, aBufNew) == 0)
	{
		// Nothing to rename, also same capitalization
	}
	else if(!str_valid_filename(m_DemoRenameInput.GetString()))
	{
		PopupMessage(Localize("Error"), Localize("This name cannot be used for files and folders"), Localize("Ok"), POPUP_RENAME_DEMO);
	}
	else if(str_utf8_comp_nocase(aBufOld, aBufNew) != 0 && // Allow renaming if it only changes capitalization to support case-insensitive filesystems
		Storage()->FileExists(aBufNew, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_StorageType))
	{
		PopupMessage(Localize("Error"), Localize("A demo with this name already exists"), Localize("Ok"), POPUP_RENAME_DEMO);
	}
	else if(Storage()->FolderExists(aBufNew, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_StorageType))
	{
		PopupMessage(Localize("Error"), Localize("A folder with this name already exists"), Localize("Ok"), POPUP_RENAME_DEMO);
	}
	else if(Storage()->RenameFile(aBufOld, aBufNew, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_StorageType))
	{
		str_copy(m_aCurrentDemoSelectionName, m_DemoRenameInput.GetString());
		if(!m_vpFilteredDemos[m_DemolistSelectedIndex]->m_IsDir)
			fs_split_file_extension(m_DemoRenameInput.GetString(), m_aCurrentDemoSelectionName, sizeof(m_aCurrentDemoSelectionName));
		DemolistPopulate();
		DemolistOnUpdate(false);
	}
	else
	{
		PopupMessage(Localize("Error"), m_vpFilteredDemos[m_DemolistSelectedIndex]->m_IsDir ? Localize("Unable to rename the folder") : Localize("Unable to rename the demo"), Localize("Ok"), POPUP_RENAME_DEMO);
	}
}

#if defined(CONF_VIDEORECORDER)
void CMenus::RequestDemoRender()
{
	m_Popup = POPUP_NONE;
	char aVideoPath[IO_MAX_PATH_LENGTH];
	str_format(aVideoPath, sizeof(aVideoPath), "videos/%s", m_DemoRenderInput.GetString());
	if(!str_endswith(aVideoPath, ".mp4"))
		str_append(aVideoPath, ".mp4");

	if(!str_valid_filename(m_DemoRenderInput.GetString()))
	{
		PopupMessage(Localize("Error"), Localize("This name cannot be used for files and folders"), Localize("Ok"), POPUP_RENDER_DEMO);
	}
	else if(Storage()->FolderExists(aVideoPath, IStorage::TYPE_SAVE))
	{
		PopupMessage(Localize("Error"), Localize("A folder with this name already exists"), Localize("Ok"), POPUP_RENDER_DEMO);
	}
	else if(Storage()->FileExists(aVideoPath, IStorage::TYPE_SAVE))
	{
		char aMessage[128 + IO_MAX_PATH_LENGTH];
		str_format(aMessage, sizeof(aMessage), Localize("File '%s' already exists, do you want to overwrite it?"), m_DemoRenderInput.GetString());
		PopupConfirm(Localize("Replace video"), aMessage, Localize("Yes"), Localize("No"), &CMenus::PopupConfirmDemoReplaceVideo, POPUP_NONE, &CMenus::DefaultButtonCallback, POPUP_RENDER_DEMO);
	}
	else
	{
		PopupConfirmDemoReplaceVideo();
	}
}
#endif
