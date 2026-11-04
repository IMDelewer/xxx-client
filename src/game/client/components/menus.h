/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MENUS_H
#define GAME_CLIENT_COMPONENTS_MENUS_H

#include <base/bytes.h>
#include <base/str.h>
#include <base/types.h>
#include <base/vmath.h>

#include <engine/console.h>
#include <engine/demo.h>
#include <engine/friends.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/component.h>
#include <game/client/components/community_icons.h>
#include <game/client/components/mapimages.h>
#include <game/client/components/skins7.h>
#include <game/client/lineinput.h>
#include <game/client/ui.h>
#include <game/voting.h>

#include <chrono>
#include <optional>
#include <vector>

class CMenus : public CComponent
{
	friend class CXui;

public:
private:
	std::optional<std::chrono::nanoseconds> m_SkinList7LastRefreshTime;
	std::optional<std::chrono::nanoseconds> m_SkinPartsList7LastRefreshTime;

	// menu_services/assets.cpp
public:
	struct SCustomItem
	{
		IGraphics::CTextureHandle m_RenderTexture;

		char m_aName[50];

		bool operator<(const SCustomItem &Other) const { return str_comp(m_aName, Other.m_aName) < 0; }
	};

	struct SCustomEntities : public SCustomItem
	{
		struct SEntitiesImage
		{
			IGraphics::CTextureHandle m_Texture;
		};
		SEntitiesImage m_aImages[MAP_IMAGE_MOD_TYPE_COUNT];
	};

	struct SCustomGame : public SCustomItem
	{
	};

	struct SCustomEmoticon : public SCustomItem
	{
	};

	struct SCustomParticle : public SCustomItem
	{
	};

	struct SCustomHud : public SCustomItem
	{
	};

	struct SCustomExtras : public SCustomItem
	{
	};

protected:
	std::vector<SCustomEntities> m_vEntitiesList;
	std::vector<SCustomGame> m_vGameList;
	std::vector<SCustomEmoticon> m_vEmoticonList;
	std::vector<SCustomParticle> m_vParticlesList;
	std::vector<SCustomHud> m_vHudList;
	std::vector<SCustomExtras> m_vExtrasList;

	bool m_IsInit = false;

	static void LoadEntities(struct SCustomEntities *pEntitiesItem, void *pUser);
	static int EntitiesScan(const char *pName, int IsDir, int DirType, void *pUser);

	static int GameScan(const char *pName, int IsDir, int DirType, void *pUser);
	static int EmoticonsScan(const char *pName, int IsDir, int DirType, void *pUser);
	static int ParticlesScan(const char *pName, int IsDir, int DirType, void *pUser);
	static int HudScan(const char *pName, int IsDir, int DirType, void *pUser);
	static int ExtrasScan(const char *pName, int IsDir, int DirType, void *pUser);

	static void ConchainAssetsEntities(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainAssetGame(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainAssetParticles(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainAssetEmoticons(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainAssetHud(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainAssetExtras(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	void ClearCustomItems(int CurTab);

	int m_MenuPage;
	int m_GamePage;
	int m_Popup;
	bool m_ShowStart;
	bool m_MenuActive;

	bool m_DummyNamePlatePreview = false;

	class CJoinTutorial
	{
	public:
		bool m_Queued = false;
		enum class EStatus
		{
			REFRESHING,
			SERVER_LIST_ERROR,
			NO_TUTORIAL_AVAILABLE,
		};
		EStatus m_Status = EStatus::REFRESHING;
		bool m_TryRefresh = false;
		bool m_TriedRefresh = false;
		enum class ELocalServerState
		{
			NOT_TRIED,
			TRY,
			WAITING_STOP,
			WAITING_START,
		};
		ELocalServerState m_LocalServerState = ELocalServerState::NOT_TRIED;
		std::chrono::nanoseconds m_StateChange = std::chrono::nanoseconds(0);
	};
	CJoinTutorial m_JoinTutorial;

	bool m_CreateDefaultFavoriteCommunities = false;
	bool m_ForceRefreshLanPage = false;

	char m_aNextServer[256];

	// loading
	class CLoadingState
	{
	public:
		std::chrono::nanoseconds m_LastRender{0};
		int m_Current;
		int m_Total;
	};
	CLoadingState m_LoadingState;

	//
	char m_aMessageTopic[512];
	char m_aMessageBody[512];
	char m_aMessageButton[512];

	CUIElement m_RefreshButton;
	CUIElement m_ConnectButton;

	// generic popups
	typedef void (CMenus::*FPopupButtonCallback)();
	void DefaultButtonCallback()
	{
		// do nothing
	}
	enum
	{
		BUTTON_CONFIRM = 0, // confirm / yes / close / ok
		BUTTON_CANCEL, // cancel / no
		NUM_BUTTONS
	};
	char m_aPopupTitle[128];
	char m_aPopupMessage[IO_MAX_PATH_LENGTH + 256];
	struct
	{
		char m_aLabel[64];
		int m_NextPopup;
		FPopupButtonCallback m_pfnCallback;
	} m_aPopupButtons[NUM_BUTTONS];

	void PopupMessage(const char *pTitle, const char *pMessage,
		const char *pButtonLabel, int NextPopup = POPUP_NONE, FPopupButtonCallback pfnButtonCallback = &CMenus::DefaultButtonCallback);
	void PopupConfirm(const char *pTitle, const char *pMessage,
		const char *pConfirmButtonLabel, const char *pCancelButtonLabel,
		FPopupButtonCallback pfnConfirmButtonCallback = &CMenus::DefaultButtonCallback, int ConfirmNextPopup = POPUP_NONE,
		FPopupButtonCallback pfnCancelButtonCallback = &CMenus::DefaultButtonCallback, int CancelNextPopup = POPUP_NONE);

	// some settings
	static float ms_ListitemAdditionalHeight;

	// for settings
	bool m_NeedRestartGraphics;
	bool m_NeedRestartSound;
	bool m_NeedRestartUpdate;
	bool m_NeedSendinfo;
	bool m_NeedSendDummyinfo;

	// for map download popup
	int64_t m_DownloadLastCheckTime;
	int m_DownloadLastCheckSize;
	float m_DownloadSpeed;

	// for password popup
	CLineInput m_PasswordInput;

	// for call vote
	int m_CallvoteSelectedOption;
	int m_CallvoteSelectedPlayer;
	CLineInputBuffered<VOTE_REASON_LENGTH> m_CallvoteReasonInput;
	CLineInputBuffered<64> m_FilterInput;
	bool m_ControlPageOpening;

	// demo
	enum
	{
		SORT_DEMONAME = 0,
		SORT_MARKERS,
		SORT_LENGTH,
		SORT_DATE,
	};

	class CDemoItem
	{
	public:
		char m_aFilename[IO_MAX_PATH_LENGTH];
		char m_aName[IO_MAX_PATH_LENGTH];
		bool m_IsDir;
		bool m_IsLink;
		int m_StorageType;
		time_t m_Date;
		int64_t m_Size;

		bool m_InfosLoaded;
		bool m_Valid;
		CDemoHeader m_Info;
		CTimelineMarkers m_TimelineMarkers;
		CMapInfo m_MapInfo;

		int NumMarkers() const
		{
			return std::clamp<int>(bytes_be_to_uint(m_TimelineMarkers.m_aNumTimelineMarkers), 0, MAX_TIMELINE_MARKERS);
		}

		int Length() const
		{
			return bytes_be_to_uint(m_Info.m_aLength);
		}

		unsigned MapSize() const
		{
			return bytes_be_to_uint(m_Info.m_aMapSize);
		}

		bool operator<(const CDemoItem &Other) const
		{
			if(!str_comp(Other.m_aFilename, ".."))
				return false;
			if(!str_comp(m_aFilename, ".."))
				return true;
			if(m_IsDir && !Other.m_IsDir)
				return true;
			if(!m_IsDir && Other.m_IsDir)
				return false;

			const CDemoItem &Left = g_Config.m_BrDemoSortOrder ? Other : *this;
			const CDemoItem &Right = g_Config.m_BrDemoSortOrder ? *this : Other;

			if(g_Config.m_BrDemoSort == SORT_DEMONAME)
				return str_comp_filenames(Left.m_aFilename, Right.m_aFilename) < 0;
			if(g_Config.m_BrDemoSort == SORT_DATE)
				return Left.m_Date < Right.m_Date;

			if(!Other.m_InfosLoaded)
				return m_InfosLoaded;
			if(!m_InfosLoaded)
				return !Other.m_InfosLoaded;

			if(g_Config.m_BrDemoSort == SORT_MARKERS)
				return Left.NumMarkers() < Right.NumMarkers();
			if(g_Config.m_BrDemoSort == SORT_LENGTH)
				return Left.Length() < Right.Length();

			// Unknown sort
			return true;
		}
	};

	char m_aCurrentDemoFolder[IO_MAX_PATH_LENGTH];
	char m_aCurrentDemoSelectionName[IO_MAX_PATH_LENGTH];
	CLineInputBuffered<IO_MAX_PATH_LENGTH> m_DemoRenameInput;
	CLineInputBuffered<IO_MAX_PATH_LENGTH> m_DemoSliceInput;
	CLineInputBuffered<IO_MAX_PATH_LENGTH> m_DemoSearchInput;
#if defined(CONF_VIDEORECORDER)
	CLineInputBuffered<IO_MAX_PATH_LENGTH> m_DemoRenderInput;
#endif
	int m_DemolistSelectedIndex;
	bool m_DemolistSelectedReveal = false;
	int m_DemolistStorageType;
	bool m_DemolistMultipleStorages = false;
	int m_Speed = 4;
	bool m_StartPaused = false;

	std::chrono::nanoseconds m_DemoPopulateStartTime{0};

	void DemolistOnUpdate(bool Reset);
	void DemolistSelectNeighbor();
	static int DemolistFetchCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser);

	// friends
	class CFriendItem
	{
		char m_aName[MAX_NAME_LENGTH];
		char m_aClan[MAX_CLAN_LENGTH];
		const CServerInfo *m_pServerInfo;
		int m_FriendState;
		bool m_IsPlayer;
		bool m_IsAfk;
		// skin info 0.6
		char m_aSkin[MAX_SKIN_LENGTH];
		bool m_CustomSkinColors;
		int m_CustomSkinColorBody;
		int m_CustomSkinColorFeet;
		// skin info 0.7
		char m_aaSkin7[protocol7::NUM_SKINPARTS][protocol7::MAX_SKIN_LENGTH];
		bool m_aUseCustomSkinColor7[protocol7::NUM_SKINPARTS];
		int m_aCustomSkinColor7[protocol7::NUM_SKINPARTS];

	public:
		CFriendItem(const CFriendInfo *pFriendInfo) :
			m_pServerInfo(nullptr),
			m_IsPlayer(false),
			m_IsAfk(false),
			m_CustomSkinColors(false),
			m_CustomSkinColorBody(0),
			m_CustomSkinColorFeet(0)
		{
			str_copy(m_aName, pFriendInfo->m_aName);
			str_copy(m_aClan, pFriendInfo->m_aClan);
			m_FriendState = m_aName[0] == '\0' ? IFriends::FRIEND_CLAN : IFriends::FRIEND_PLAYER;
			m_aSkin[0] = '\0';
			for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
			{
				m_aaSkin7[Part][0] = '\0';
				m_aUseCustomSkinColor7[Part] = false;
				m_aCustomSkinColor7[Part] = 0;
			}
		}
		CFriendItem(const CServerInfo::CClient &CurrentClient, const CServerInfo *pServerInfo) :
			m_pServerInfo(pServerInfo),
			m_FriendState(CurrentClient.m_FriendState),
			m_IsPlayer(CurrentClient.m_Player),
			m_IsAfk(CurrentClient.m_Afk),
			m_CustomSkinColors(CurrentClient.m_CustomSkinColors),
			m_CustomSkinColorBody(CurrentClient.m_CustomSkinColorBody),
			m_CustomSkinColorFeet(CurrentClient.m_CustomSkinColorFeet)
		{
			str_copy(m_aName, CurrentClient.m_aName);
			str_copy(m_aClan, CurrentClient.m_aClan);
			str_copy(m_aSkin, CurrentClient.m_aSkin);
			for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
			{
				str_copy(m_aaSkin7[Part], CurrentClient.m_aaSkin7[Part]);
				m_aUseCustomSkinColor7[Part] = CurrentClient.m_aUseCustomSkinColor7[Part];
				m_aCustomSkinColor7[Part] = CurrentClient.m_aCustomSkinColor7[Part];
			}
		}

		const char *Name() const { return m_aName; }
		const char *Clan() const { return m_aClan; }
		const CServerInfo *ServerInfo() const { return m_pServerInfo; }
		int FriendState() const { return m_FriendState; }
		bool IsPlayer() const { return m_IsPlayer; }
		bool IsAfk() const { return m_IsAfk; }
		// 0.6 skin
		const char *Skin() const { return m_aSkin; }
		bool CustomSkinColors() const { return m_CustomSkinColors; }
		int CustomSkinColorBody() const { return m_CustomSkinColorBody; }
		int CustomSkinColorFeet() const { return m_CustomSkinColorFeet; }
		// 0.7 skin
		const char *Skin7(int Part) const { return m_aaSkin7[Part]; }
		bool UseCustomSkinColor7(int Part) const { return m_aUseCustomSkinColor7[Part]; }
		int CustomSkinColor7(int Part) const { return m_aCustomSkinColor7[Part]; }

		const void *ListItemId() const { return &m_aName; }
		const void *RemoveButtonId() const { return &m_FriendState; }
		const void *CommunityTooltipId() const { return &m_IsPlayer; }
		const void *SkinTooltipId() const { return &m_aSkin; }

		bool operator<(const CFriendItem &Other) const
		{
			const int Result = str_comp_nocase(m_aName, Other.m_aName);
			return Result < 0 || (Result == 0 && str_comp_nocase(m_aClan, Other.m_aClan) < 0);
		}
	};

	enum
	{
		FRIEND_PLAYER_ON = 0,
		FRIEND_CLAN_ON,
		FRIEND_OFF,
		NUM_FRIEND_TYPES
	};
	std::vector<CFriendItem> m_avFriends[NUM_FRIEND_TYPES];
	const CFriendItem *m_pRemoveFriend = nullptr;

	// found in menus.cpp
	void Render();

#if defined(CONF_VIDEORECORDER)
	void PopupConfirmDemoReplaceVideo();
#endif

	static void ConchainBackgroundEntities(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainUpdateMusicState(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	// found in menu_services/demos.cpp
	vec2 m_DemoControlsPositionOffset = vec2(0.0f, 0.0f);
	bool m_PausedBeforeSeeking;
	float m_PrevSeekAmount;
	float m_LastPauseChange = -1.0f;
	float m_LastSpeedChange = -1.0f;
	static constexpr int DEFAULT_SKIP_DURATION_INDEX = 3;
	int m_SkipDurationIndex = DEFAULT_SKIP_DURATION_INDEX;
	static bool DemoFilterChat(const void *pData, int Size, void *pUser);
	bool FetchHeader(CDemoItem &Item);
	void FetchAllHeaders();
	void RenameDemo();
#if defined(CONF_VIDEORECORDER)
	void RequestDemoRender();
#endif
	void HandleDemoSeeking(float PositionToSeek, float TimeToSeek);

	bool m_DemoBrowserListInitialized = false;

	void PopupConfirmPlayDemo();
	void PopupConfirmDeleteDemo();
	void PopupConfirmDeleteFolder();
	static void ConchainDemoPlay(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainDemoSpeed(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	// found in menu_services/ghosts.cpp
	STextContainerIndex m_MotdTextContainerIndex;

	void PopupConfirmDisconnect();
	void PopupConfirmDisconnectDummy();

	// found in menu_services/browser.cpp
	int m_SelectedIndex;
	bool m_ServerBrowserShouldRevealSelection;
	std::vector<CUIElement *> m_avpServerBrowserUiElements[IServerBrowser::NUM_TYPES];
	std::vector<char> m_vServerbrowserElementIds[IServerBrowser::NUM_TYPES];

	void PopupConfirmSwitchServer();

	void ResetServerbrowserFilters();
	void UpdateLastServer();

	struct SPopupCountrySelectionContext
	{
		CMenus *m_pMenus;
		int m_Selection;
		bool m_New;
	};
	static CUi::EPopupMenuFunctionResult PopupCountrySelection(void *pContext, CUIRect View, bool Active);

	void FriendlistOnUpdate();
	void PopupConfirmRemoveFriend();

	static void ConchainFriendlistUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainFavoritesUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainCommunitiesUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainUiPageUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	void UpdateCommunityCache(bool Force);

	// found in menu_services/settings.cpp

	// found in menu_services/assets.cpp

	// found in menus_settings_appearance.cpp

	// found in menus_settings_controls.cpp
	// TODO: Change PopupConfirm to avoid using a function pointer to a CMenus
	//       member function, to move this function to CMenusSettingsControls

	// found in menus_settings_credits.cpp

	// found in menus_settings_ddnet.cpp

	// found in xxx/menus_xxx.cpp

	// found in menus_settings_general.cpp

	// found in menus_settings_graphics.cpp

	// found in menus_settings_language.cpp

	// found in menus_settings_player.cpp

	// found in menus_settings_sound.cpp

	// found in menus_settings_tee.cpp

	bool m_SkinListScrollToSelected = false;

	// found in menus_settings_tee7.cpp

	bool m_CustomSkinMenu = false;
	int m_TeePartSelected = protocol7::SKINPART_BODY;
	const CSkins7::CSkin *m_pSelectedSkin = nullptr;
	int m_SelectedSkinIndex7 = -1;
	int m_DeletedSkinIndex7 = -1;
	CLineInputBuffered<protocol7::MAX_SKIN_ARRAY_SIZE, protocol7::MAX_SKIN_LENGTH> m_SkinNameInput;
	bool m_SkinPartListNeedsUpdate = false;

	std::vector<CButtonContainer> m_vButtonContainersNamePlateShow = {{}, {}, {}, {}};
	std::vector<CButtonContainer> m_vButtonContainersNamePlateKeyPresses = {{}, {}, {}, {}};
	std::vector<CButtonContainer> m_vButtonContainersAntiPingPlayers = {{}, {}, {}};

public:
	void EnsureAssetList(int Tab, const std::function<void()> &LoadedFunc);
	int NumAssets(int Tab) const;
	const SCustomItem *AssetAt(int Tab, int Index) const;
	const char *SelectedAsset(int Tab) const;
	void SelectAsset(int Tab, const char *pName);
	static const char *AssetFolder(int Tab);
	void ReloadAssets(int Tab) { ClearCustomItems(Tab); }
	void UpdateMusicState();
	CCommunityIcons &CommunityIcons() { return m_CommunityIcons; }
	void QueueSendInfo(bool Dummy)
	{
		if(Dummy)
			m_NeedSendDummyinfo = true;
		else
			m_NeedSendinfo = true;
	}
	bool NeedsRestart() const { return m_NeedRestartGraphics || m_NeedRestartSound || m_NeedRestartUpdate; }
	void SetNeedRestartGraphics(bool Need) { m_NeedRestartGraphics = Need; }
	void SetNeedRestartSound(bool Need) { m_NeedRestartSound = Need; }

	CMenus();
	int Sizeof() const override { return sizeof(*this); }

	void RenderLoadingDirect(const char *pCaption, const char *pContent, std::optional<float> Progress);
	void RenderLoading(const char *pCaption, const char *pContent, int IncreaseCounter);
	void FinishLoading();

	bool IsInit() const { return m_IsInit; }

	bool IsActive() const { return m_MenuActive; }
	void SetActive(bool Active);

	void OnInterfacesInit(CGameClient *pClient) override;
	void OnInit() override;

	void OnStateChange(int NewState, int OldState) override;
	void OnWindowResize() override;
	void OnRender() override;
	bool OnInput(const IInput::CEvent &Event) override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	void OnShutdown() override;

	enum
	{
		PAGE_NEWS = 1,
		PAGE_GAME,
		PAGE_PLAYERS,
		PAGE_SERVER_INFO,
		PAGE_CALLVOTE,
		PAGE_INTERNET,
		PAGE_LAN,
		PAGE_FAVORITES,
		PAGE_FAVORITE_COMMUNITY_1,
		PAGE_FAVORITE_COMMUNITY_2,
		PAGE_FAVORITE_COMMUNITY_3,
		PAGE_FAVORITE_COMMUNITY_4,
		PAGE_FAVORITE_COMMUNITY_5,
		PAGE_DEMOS,
		PAGE_SETTINGS,
		PAGE_NETWORK,
		PAGE_GHOST,

		PAGE_LENGTH,
	};

	enum
	{
		SETTINGS_LANGUAGE = 0,
		SETTINGS_GENERAL,
		SETTINGS_PLAYER,
		SETTINGS_TEE,
		SETTINGS_APPEARANCE,
		SETTINGS_CONTROLS,
		SETTINGS_GRAPHICS,
		SETTINGS_SOUND,
		SETTINGS_DDNET,
		SETTINGS_XXX,
		SETTINGS_ASSETS,
		SETTINGS_CREDITS,

		SETTINGS_LENGTH,
	};

	enum
	{
		BIG_TAB_NEWS = 0,
		BIG_TAB_INTERNET,
		BIG_TAB_LAN,
		BIG_TAB_FAVORITES,
		BIT_TAB_FAVORITE_COMMUNITY_1,
		BIT_TAB_FAVORITE_COMMUNITY_2,
		BIT_TAB_FAVORITE_COMMUNITY_3,
		BIT_TAB_FAVORITE_COMMUNITY_4,
		BIT_TAB_FAVORITE_COMMUNITY_5,
		BIG_TAB_DEMOS,

		BIG_TAB_LENGTH,
	};

	enum
	{
		SMALL_TAB_HOME = 0,
		SMALL_TAB_QUIT,
		SMALL_TAB_SETTINGS,
		SMALL_TAB_EDITOR,
		SMALL_TAB_DEMOBUTTON,
		SMALL_TAB_SERVER,
		SMALL_TAB_BROWSER_FILTER,
		SMALL_TAB_BROWSER_INFO,
		SMALL_TAB_BROWSER_FRIENDS,

		SMALL_TAB_LENGTH,
	};

	// DDRace

	std::vector<CDemoItem> m_vDemos;
	std::vector<CDemoItem *> m_vpFilteredDemos;
	void DemolistPopulate();
	void RefreshFilteredDemos();
	void DemoSeekTick(IDemoPlayer::ETickOffset TickOffset);
	bool m_Dummy;

	const char *GetCurrentDemoFolder() const { return m_aCurrentDemoFolder; }

	// Ghost
	struct CGhostItem
	{
		char m_aFilename[IO_MAX_PATH_LENGTH];
		char m_aPlayer[MAX_NAME_LENGTH];

		bool m_Failed;
		int m_Time;
		int m_Slot;
		bool m_Own;
		time_t m_Date;

		CGhostItem() :
			m_Slot(-1), m_Own(false) { m_aFilename[0] = 0; }

		bool Active() const { return m_Slot != -1; }
		bool HasFile() const { return m_aFilename[0]; }
	};

	enum
	{
		GHOST_SORT_NONE = -1,
		GHOST_SORT_NAME,
		GHOST_SORT_TIME,
		GHOST_SORT_DATE,
	};

	std::vector<CGhostItem> m_vGhosts;

	std::chrono::nanoseconds m_GhostPopulateStartTime{0};

	void GhostlistPopulate();
	CGhostItem *GetOwnGhost();
	void UpdateOwnGhost(CGhostItem Item);
	void DeleteGhostItem(int Index);
	void SortGhostlist();

	bool CanDisplayWarning() const;

	void PopupWarning(const char *pTopic, const char *pBody, const char *pButton, std::chrono::nanoseconds Duration);

	std::chrono::nanoseconds m_PopupWarningLastTime;
	std::chrono::nanoseconds m_PopupWarningDuration;

	int m_DemoPlayerState;

	enum
	{
		POPUP_NONE = 0,
		POPUP_MESSAGE, // generic message popup (one button)
		POPUP_CONFIRM, // generic confirmation popup (two buttons)
		POPUP_FIRST_LAUNCH,
		POPUP_JOIN_TUTORIAL,
		POPUP_POINTS,
		POPUP_DISCONNECTED,
		POPUP_LANGUAGE,
		POPUP_OPTIONAL_FONTS,
		POPUP_RENAME_DEMO,
		POPUP_RENDER_DEMO,
		POPUP_RENDER_DONE,
		POPUP_PASSWORD,
		POPUP_QUIT,
		POPUP_RESTART,
		POPUP_WARNING,
		POPUP_SAVE_SKIN,
	};

	enum
	{
		// demo player states
		DEMOPLAYER_NONE = 0,
		DEMOPLAYER_SLICE_SAVE,
	};

	int MenuPage() const { return m_MenuPage; }
	void RenderShell();
	void SetMenuPage(int NewPage);
	void RefreshBrowserTab(bool Force);
	void ForceRefreshLanPage();
	void SetShowStart(bool ShowStart);
	void ShowQuitPopup();
	void JoinTutorial();
	void Connect(const char *pAddress);

private:
	CCommunityIcons m_CommunityIcons;

	static int GhostlistFetchCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser);

	// found in menu_services/ghosts.cpp
};
#endif
