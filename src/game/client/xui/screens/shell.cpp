#include <base/net.h>
#include <base/str.h>

#include <engine/client/keyboard.h>
#include <engine/font_icons_phosphor.h>
#include <engine/friends.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/xui/screens/settings.h>
#include <game/client/xui/xui.h>
#include <game/localization.h>
#include <game/version.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace Xui;

#if defined(DDEV_XX)
void CXui::DebugScreen(const char *pName)
{
	CMenus &Menus = GameClient()->m_Menus;
	m_HudEditor = str_comp(pName, "hud") == 0;
	m_ShellProfile = str_comp(pName, "profile") == 0;
	m_ShellHome = str_comp(pName, "home") == 0;
	if(str_comp(pName, "browser") == 0)
		Menus.SetMenuPage(CMenus::PAGE_INTERNET);
	else
	{
		Menus.SetMenuPage(CMenus::PAGE_SETTINGS);
		SettingsSelectSection(str_comp(pName, "catalog") == 0 ? "assets" : pName);
		if(str_comp(pName, "catalog") == 0)
			SettingsShowCatalog();
	}
}
#endif

namespace
{
	struct SPopular
	{
		std::string m_Map;
		std::string m_Name;
		std::string m_Type;
		std::string m_Address;
		int m_Crc = 0;
		int m_Players = 0;
		int m_Max = 0;
	};

	struct SFriendEntry
	{
		std::string m_Name;
		std::string m_Clan;
		CServerInfo::CClient m_Client = {};
		std::string m_Where;
		std::string m_Address;
	};

	struct SLast
	{
		std::string m_Address;
		bool m_Found = false;
		std::string m_Map;
		std::string m_Name;
		std::string m_Info;
	};

	struct SShellData
	{
		std::vector<SPopular> m_vPopular;
		std::vector<SFriendEntry> m_vFriends;
		int m_FriendsOnline = 0;
		SLast m_Last;
	};

	void Gather(IServerBrowser *pBrowser, IFriends *pFriends, SShellData &Data)
	{
		Data.m_vPopular.clear();
		Data.m_vFriends.clear();
		Data.m_FriendsOnline = 0;
		Data.m_Last = SLast();
		Data.m_Last.m_Address = g_Config.m_XxLastServerAddress[0] ? g_Config.m_XxLastServerAddress : g_Config.m_UiServerAddress;
		if(g_Config.m_XxLastServerAddress[0])
		{
			Data.m_Last.m_Map = g_Config.m_XxLastServerMap;
			Data.m_Last.m_Name = g_Config.m_XxLastServerName;
			Data.m_Last.m_Info = Data.m_Last.m_Address;
			Data.m_Last.m_Found = !Data.m_Last.m_Map.empty() || !Data.m_Last.m_Name.empty();
		}
		const char *pLast = Data.m_Last.m_Address.c_str();
		if(const char *pNumeric = str_startswith(pLast, "tw-0.7+udp://"))
			pLast = pNumeric;
		NETADDR LastAddress = {};
		const bool NumericLast = net_addr_from_str(&LastAddress, pLast) == 0;

		std::vector<const CServerInfo *> vServers;
		const int Num = pBrowser->NumInternetServers();
		vServers.reserve(Num);
		for(int i = 0; i < Num; i++)
			vServers.push_back(pBrowser->InternetGet(i));

		for(const CServerInfo *pInfo : vServers)
		{
			bool Last = !Data.m_Last.m_Address.empty() && str_comp(pInfo->m_aAddress, Data.m_Last.m_Address.c_str()) == 0;
			for(int i = 0; NumericLast && !Last && i < pInfo->m_NumAddresses; ++i)
			{
				NETADDR Address = pInfo->m_aAddresses[i];
				Address.type &= ~NETTYPE_TW7;
				Last = net_addr_comp(&Address, &LastAddress) == 0;
			}
			if(Last)
			{
				Data.m_Last.m_Found = true;
				Data.m_Last.m_Map = pInfo->m_aMap;
				Data.m_Last.m_Name = pInfo->m_aName;
				char aBuf[160];
				str_format(aBuf, sizeof(aBuf), Localize("%d/%d players"), pInfo->m_NumPlayers, pInfo->m_MaxPlayers);
				if(pInfo->m_Latency >= 0)
				{
					char aPing[32];
					str_format(aPing, sizeof(aPing), " · %d ms", pInfo->m_Latency);
					str_append(aBuf, aPing);
				}
				Data.m_Last.m_Info = aBuf;
			}
			for(const CServerInfo::CClient &Client : pInfo->m_vClients)
			{
				if(!pFriends->IsFriend(Client.m_aName, Client.m_aClan, true))
					continue;
				Data.m_FriendsOnline++;
				if(std::none_of(Data.m_vFriends.begin(), Data.m_vFriends.end(), [&](const SFriendEntry &F) { return F.m_Name == Client.m_aName && F.m_Clan == Client.m_aClan; }))
				{
					SFriendEntry Entry;
					Entry.m_Name = Client.m_aName;
					Entry.m_Clan = Client.m_aClan;
					Entry.m_Client = Client;
					Entry.m_Where = std::string(pInfo->m_aMap) + " \xC2\xB7 " + pInfo->m_aName;
					Entry.m_Address = pInfo->m_aAddress;
					Data.m_vFriends.push_back(std::move(Entry));
				}
			}
		}

		Data.m_FriendsOnline = (int)Data.m_vFriends.size();
		const size_t Take = std::min<size_t>(5, vServers.size());
		std::partial_sort(vServers.begin(), vServers.begin() + Take, vServers.end(), [](const CServerInfo *pA, const CServerInfo *pB) {
			return pA->m_NumPlayers > pB->m_NumPlayers;
		});
		for(size_t i = 0; i < Take; i++)
		{
			const CServerInfo *pInfo = vServers[i];
			if(pInfo->m_NumPlayers <= 0)
				break;
			SPopular Entry;
			Entry.m_Map = pInfo->m_aMap;
			Entry.m_Name = pInfo->m_aName;
			Entry.m_Type = pInfo->m_aGameType;
			Entry.m_Address = pInfo->m_aAddress;
			Entry.m_Crc = pInfo->m_MapCrc;
			Entry.m_Players = pInfo->m_NumPlayers;
			Entry.m_Max = pInfo->m_MaxPlayers;
			Data.m_vPopular.push_back(std::move(Entry));
		}
	}
}

namespace
{
	void DrawGrade(CXui &Ui, const SRect &Rc)
	{
		Ui.DrawRect(Rc, CXui::SolidRect(XuiTheme::Rgb(0x05060A, 0.55f), 0.0f));
		const float Fade = Rc.m_H * 0.34f;
		IGraphics::SUiRect Edge = CXui::SolidRect(ColorRGBA(0, 0, 0, 0.62f), 0.0f);
		Edge.m_Fill2 = ColorRGBA(0, 0, 0, 0.0f);
		Edge.m_GradientAngle = 90.0f;
		Ui.DrawRect({Rc.m_X, Rc.m_Y, Rc.m_W, Fade}, Edge);
		Edge.m_Fill = ColorRGBA(0, 0, 0, 0.0f);
		Edge.m_Fill2 = ColorRGBA(0, 0, 0, 0.62f);
		Ui.DrawRect({Rc.m_X, Rc.Bottom() - Fade, Rc.m_W, Fade}, Edge);
		const float Side = Rc.m_W * 0.18f;
		Edge.m_Fill = ColorRGBA(0, 0, 0, 0.5f);
		Edge.m_Fill2 = ColorRGBA(0, 0, 0, 0.0f);
		Edge.m_GradientAngle = 0.0f;
		Ui.DrawRect({Rc.m_X, Rc.m_Y, Side, Rc.m_H}, Edge);
		Edge.m_Fill = ColorRGBA(0, 0, 0, 0.0f);
		Edge.m_Fill2 = ColorRGBA(0, 0, 0, 0.5f);
		Ui.DrawRect({Rc.Right() - Side, Rc.m_Y, Side, Rc.m_H}, Edge);
	}
}

void CXui::RenderGrade()
{
	RunPage([this]() {
		SBox B;
		B.m_Position = EPos::ABSOLUTE;
		B.m_Left = B.m_Top = B.m_Right = B.m_Bottom = 0.0f;
		Paint(Row(B), DrawGrade);
	});
}

void CXui::RenderShell(CMenus &Menus)
{
	if(m_HudEditor)
	{
		RunPage([&]() { RenderHudEditor(); });
		return;
	}
	static SShellData s_Data;
	if(m_Frame - (uint64_t)std::max(0, m_PopRefresh) > 45 || m_PopRefresh < 0)
	{
		Gather(GameClient()->ServerBrowser(), GameClient()->Friends(), s_Data);
		m_PopRefresh = (int)m_Frame;
	}

	const int MenuPage = Menus.MenuPage();
	int DockIndex = 0;
	if(!m_ShellHome)
	{
		if(MenuPage == CMenus::PAGE_DEMOS)
			DockIndex = 2;
		else if(MenuPage == CMenus::PAGE_SETTINGS)
			DockIndex = 3;
		else
			DockIndex = 1;
	}
	if(m_ShellProfile)
		DockIndex = -1;
	m_LegacyNode = -1;

	if(Client()->State() == IClient::STATE_CONNECTING || Client()->State() == IClient::STATE_LOADING)
	{
		if(Input()->KeyPress(KEY_ESCAPE))
			Client()->Disconnect();
	}

	RunPage([&]() {
		SBox Grade;
		Grade.m_Position = EPos::ABSOLUTE;
		Grade.m_Left = Grade.m_Top = Grade.m_Right = Grade.m_Bottom = 0.0f;
		m_HomeScene.Render(*this, Menus, m_ScreenUnits);
		if(!m_LightTheme)
			Paint(Row(Grade), [](CXui &U, const SRect &Rc) { U.DrawRect(Rc, SolidRect(XuiTheme::Home::SCRIM, 0)); });

		STextStyle Wordmark;
		Wordmark.m_Size = 26.0f;
		Wordmark.m_Font = EFont::EXTRABOLD;
		Wordmark.m_Tracking = -0.04f;
		SBox Brand;
		Brand.m_Gap = 10.0f;
		Brand.m_Cross = EAlign::CENTER;
		Brand.m_Position = EPos::ABSOLUTE;
		Brand.m_Left = 36.0f;
		Brand.m_Top = 24.0f;
		Brand.m_pKey = "brand";
		Row(Brand, [&]() {
			Text("xxx", Wordmark);
			STextStyle Version;
			Version.m_Size = 12.0f;
			Version.m_Font = EFont::SEMIBOLD;
			Version.m_Color = XuiTheme::Color::TEXT3;
			Text(XXX_VERSION, Version);
		});

		const uint32_t ProfileId = MakeId("profile");
		const SResult ProfileR = Interact(ProfileId);
		SState &ProfileSt = State(ProfileId);
		const float ProfileScale = ProfileSt.m_Scale.Update(ProfileR.m_Pressed ? XuiTheme::Motion::PRESS_SCALE : (ProfileR.m_Hovered ? XuiTheme::Motion::HOVER_SCALE : 1.0f), m_Dt, XuiTheme::Motion::PRESS);
		if(ProfileR.m_Clicked)
		{
			m_ProfileName.clear();
			m_ShellProfile = true;
			m_ShellHome = false;
		}
		SBox Profile;
		Profile.m_Gap = 10.0f;
		Profile.m_Padding = SEdges(0.0f, 16.0f, 0.0f, 6.0f);
		Profile.m_H = Px(48.0f);
		Profile.m_Cross = EAlign::CENTER;
		Profile.m_Position = EPos::ABSOLUTE;
		Profile.m_Right = 32.0f;
		Profile.m_Top = 18.0f;
		Profile.m_Hit = EHit::INTERACTIVE;
		const int ProfileNode = BoxId(ProfileId, EDir::ROW, Profile, [&]() {
			SBox Av;
			Av.m_W = Px(38.0f);
			Av.m_H = Px(38.0f);
			Av.m_Main = EAlign::CENTER;
			Av.m_Cross = EAlign::CENTER;
			Av.m_Clip = true;
			const int AvNode = Row(Av, [&]() {
				TeeAvatar(g_Config.m_ClPlayerSkin, 34.0f);
			});
			Paint(AvNode, [](CXui &Ui, const SRect &Rc) {
				Ui.DrawRect(Rc, SolidRect(ColorRGBA(0, 0, 0, 0.35f), Rc.m_H * 0.5f));
			});
			SBox Col;
			Col.m_Gap = 1.0f;
			Column(Col, [&]() {
				STextStyle Name;
				Name.m_Size = 14.0f;
				Name.m_Font = EFont::BOLD;
				Text(Client()->PlayerName(), Name);
				STextStyle Sub;
				Sub.m_Size = 11.5f;
				Sub.m_Color = XuiTheme::Color::TEXT3;
				Sub.m_Tabular = true;
				Text(g_Config.m_PlayerClan[0] != '\0' ? g_Config.m_PlayerClan : "Tee", Sub);
			});
		});
		SetTransform(ProfileNode, ProfileScale, 1.0f);
		Paint(ProfileNode, [](CXui &Ui, const SRect &Rc) {
			Ui.DrawRect(Rc, HomePanel(Rc.m_H * 0.5f));
			Ui.DrawRect(Rc, GlassHairline(Rc.m_H * 0.5f));
		});

		SBox Page;
		Page.m_Position = EPos::ABSOLUTE;
		if(m_ShellHome || m_ShellProfile)
		{
			Page.m_Left = 60.0f;
			Page.m_Right = 36.0f;
			Page.m_Top = 112.0f;
			Page.m_Bottom = XuiTheme::Size::DOCK_BOTTOM + XuiTheme::Size::DOCK_H + 16.0f;
		}
		else
		{
			Page.m_Left = 36.0f;
			Page.m_Right = 36.0f;
			Page.m_Top = 96.0f;
			Page.m_Bottom = XuiTheme::Size::DOCK_BOTTOM + XuiTheme::Size::DOCK_H + 16.0f;
		}
		char aPageKey[16];
		str_format(aPageKey, sizeof(aPageKey), "page%d", DockIndex);
		PageTransition(aPageKey, Page, [&]() {
			if(m_ShellProfile)
			{
				SBox Fill;
				Fill.m_W = Grow();
				Fill.m_H = Grow();
				Row(Fill, [&]() { RenderProfile(Menus); });
				return;
			}
			if(!m_ShellHome && MenuPage >= CMenus::PAGE_INTERNET && MenuPage <= CMenus::PAGE_FAVORITE_COMMUNITY_5)
			{
				SBox Fill;
				Fill.m_W = Grow();
				Fill.m_H = Grow();
				Row(Fill, [&]() { RenderBrowser(Menus); });
				return;
			}
			if(!m_ShellHome && MenuPage == CMenus::PAGE_DEMOS)
			{
				SBox Fill;
				Fill.m_W = Grow();
				Fill.m_H = Grow();
				Row(Fill, [&]() { RenderDemos(Menus); });
				return;
			}
			if(!m_ShellHome && MenuPage == CMenus::PAGE_SETTINGS)
			{
				SBox Fill;
				Fill.m_W = Grow();
				Fill.m_H = Grow();
				Row(Fill, [&]() { RenderSettings(*this); });
				return;
			}
			if(!m_ShellHome)
			{
				SBox Fill;
				Fill.m_W = Grow();
				Fill.m_H = Grow();
				Fill.m_Padding = SEdges(12.0f);
				Fill.m_pKey = "legacy";
				m_LegacyNode = GlassPanel(Fill, nullptr, XuiTheme::Radius::XL, true);
				return;
			}
			auto HomeCard = [&](const SBox &Box, const FBuild &Content, float Radius = 28.0f, bool = false) {
				SBox Panel = Box;
				Panel.m_Hit = EHit::BLOCK;
				const int Node = Column(Panel, Content);
				Paint(Node, [Radius](CXui &U, const SRect &Rc) { U.DrawRect(Rc, HomePanel(Radius)); });
				return Node;
			};
			SBox Shell;
			Shell.m_Gap = 24.0f;
			Shell.m_W = Grow();
			Shell.m_H = Grow();
			Shell.m_Cross = EAlign::STRETCH;
			Shell.m_Main = EAlign::BETWEEN;
			Row(Shell, [&]() {
				SBox Left;
				Left.m_W = Px(std::min(710.0f, m_ScreenUnits.x - 440.0f));
				Left.m_H = Grow();
				Left.m_Shrink = true;
				Left.m_Scroll = m_ScreenUnits.y < 700.0f ? EScroll::VERTICAL : EScroll::NONE;
				Column(Left, [&]() {
					SBox LastCard;
					LastCard.m_W = Grow();
					LastCard.m_Padding = SEdges(22.0f);
					LastCard.m_Clip = true;
					HomeCard(LastCard, [&]() {
						SBox Summary;
						Summary.m_W = Grow();
						Summary.m_Gap = 20.0f;
						Summary.m_Cross = EAlign::CENTER;
						Row(Summary, [&]() {
							SBox Description;
							Description.m_W = Grow();
							Column(Description, [&]() {
								STextStyle Small;
								Small.m_Size = 13.0f;
								Small.m_Font = EFont::SEMIBOLD;
								Small.m_Color = XuiTheme::Color::TEXT3;
								Text(Localize("Last server"), Small);

								const uint32_t TitleId = MakeId("title");
								const SResult TitleR = Interact(TitleId);
								SBox TitleBox;
								TitleBox.m_Padding = SEdges(10.0f, 0.0f, 8.0f, 0.0f);
								TitleBox.m_H = Fit();
								TitleBox.m_W = Grow();
								TitleBox.m_Cross = EAlign::CENTER;
								TitleBox.m_Hit = EHit::INTERACTIVE;
								BoxId(TitleId, EDir::ROW, TitleBox, [&]() {
									STextStyle Hero;
									Hero.m_Size = 42.0f;
									Hero.m_Wrap = true;
									Hero.m_MaxLines = 3;
									Hero.m_Font = EFont::EXTRABOLD;
									Hero.m_Tracking = -0.045f;
									const char *pHero = s_Data.m_Last.m_Found ? (s_Data.m_Last.m_Map.empty() ? s_Data.m_Last.m_Name.c_str() : s_Data.m_Last.m_Map.c_str()) : (s_Data.m_Last.m_Address.empty() ? Localize("Welcome") : s_Data.m_Last.m_Address.c_str());
									Text(pHero, Hero, {.m_W = Grow()});
								});
								if(TitleR.m_Clicked)
									Menus.Connect(s_Data.m_Last.m_Address.c_str());

								STextStyle Sub;
								Sub.m_Size = 14.0f;
								Sub.m_Wrap = true;
								Sub.m_MaxLines = 3;
								Sub.m_Color = XuiTheme::Color::TEXT2;
								Text(s_Data.m_Last.m_Found ? s_Data.m_Last.m_Name.c_str() : Localize("Join your last server or pick one below"), Sub, {.m_W = Grow()});
								STextStyle Meta;
								Meta.m_Size = 13.0f;
								Meta.m_Color = XuiTheme::Color::TEXT3;
								Meta.m_Tabular = true;
								Meta.m_Wrap = true;
								SBox MetaBox;
								MetaBox.m_W = Grow();
								MetaBox.m_Padding = SEdges(4.0f, 0.0f, 0.0f, 0.0f);
								Text(s_Data.m_Last.m_Found ? s_Data.m_Last.m_Info.c_str() : s_Data.m_Last.m_Address.c_str(), Meta, MetaBox);
							});
							if(!s_Data.m_Last.m_Map.empty())
							{
								SBox Image;
								Image.m_W = Px(200.0f);
								Image.m_H = Px(144.0f);
								Image.m_Main = EAlign::CENTER;
								Image.m_Cross = EAlign::CENTER;
								Image.m_Gap = 10.0f;
								Image.m_Padding = SEdges(16.0f);
								const auto Preview = GameClient()->m_MapPreviews.Artwork(s_Data.m_Last.m_Map.c_str());
								const int ImageNode = Column(Image, [&]() {
									if(!Preview.Valid())
									{
										Icon(Ph::MAP_TRIFOLD, 32.0f, XuiTheme::Color::TEXT3);
										STextStyle Name;
										Name.m_Size = 14.0f;
										Name.m_Color = XuiTheme::Color::TEXT2;
										Name.m_Wrap = true;
										Name.m_MaxLines = 2;
										Name.m_Align = ETextAlign::CENTER;
										Text(s_Data.m_Last.m_Map.c_str(), Name, {.m_W = Grow()});
									}
								});
								Paint(ImageNode, [Preview](CXui &U, const SRect &Rc) {
									if(Preview.Valid())
										U.DrawImage(Rc, Preview.m_Texture, vec2(Preview.m_Size.x, Preview.m_Size.y), 20.0f);
									else
										U.DrawRect(Rc, SolidRect(XuiTheme::Color::GROUP_FILL, 20.0f));
								});
							}
						});

						SBox Actions;
						Actions.m_Gap = 8.0f;
						Actions.m_W = Grow();
						Actions.m_Wrap = true;
						Actions.m_Padding = SEdges(26.0f, 0.0f, 0.0f, 0.0f);
						Row(Actions, [&]() {
							if(Button(Localize("Connect"), {.m_Variant = EVariant::ACCENT, .m_pIcon = Ph::PLAY, .m_Height = 50.0f, .m_PadX = 30.0f, .m_FontSize = 16.0f, .m_Font = EFont::BOLD, .m_IconFill = true}).m_Clicked)
								Menus.Connect(s_Data.m_Last.m_Address.c_str());
							if(Button(Localize("All servers"), {.m_Variant = EVariant::GLASS, .m_pIcon = Ph::HARD_DRIVES, .m_Height = 50.0f, .m_PadX = 24.0f, .m_FontSize = 16.0f}).m_Clicked)
							{
								m_ShellHome = false;
								Menus.SetMenuPage(g_Config.m_UiPage >= CMenus::PAGE_INTERNET && g_Config.m_UiPage <= CMenus::PAGE_FAVORITE_COMMUNITY_5 ? g_Config.m_UiPage : CMenus::PAGE_INTERNET);
							}
							CLocalServer &Local = GameClient()->m_LocalServer;
							const bool Running = Local.IsServerRunning();
							static bool s_LocalOpen = false;
							const SResult LocalBtn = Button(Running ? Localize("Local server · running") : Localize("Local server"), {.m_Variant = s_LocalOpen ? EVariant::GLASS : EVariant::GHOST, .m_pIcon = Running ? Ph::BROADCAST : Ph::HARD_DRIVE, .m_Loading = Local.IsStarting(), .m_Height = 50.0f, .m_PadX = 22.0f, .m_FontSize = 14.0f, .m_pKey = "local"});
							if(LocalBtn.m_Clicked)
								s_LocalOpen = !s_LocalOpen;
							Popover("local.pop", &s_LocalOpen, LocalBtn.m_Id, [&]() {
								SBox Col;
								Col.m_W = Px(300.0f);
								Col.m_Gap = 6.0f;
								Column(Col, [&]() {
									if(Running)
									{
										char aLan[64];
										if(Local.LanAddress(aLan, sizeof(aLan)))
										{
											char aLabel[96];
											str_format(aLabel, sizeof(aLabel), "%s  %s", Localize("Address for your network:"), aLan);
											if(Button(aLabel, {.m_Variant = EVariant::GLASS, .m_pIcon = Ph::COPY, .m_W = Grow(), .m_pKey = "local.addr"}).m_Clicked)
											{
												Input()->SetClipboardText(aLan);
												ShowToast(Localize("Address copied"), Ph::COPY);
											}
										}
										if(Button(Localize("Join"), {.m_Variant = EVariant::ACCENT, .m_pIcon = Ph::SIGN_IN, .m_W = Grow(), .m_pKey = "local.join"}).m_Clicked)
										{
											Local.Connect();
											s_LocalOpen = false;
										}
										if(Button(Localize("Stop server"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::STOP, .m_W = Grow(), .m_pKey = "local.stop"}).m_Clicked)
										{
											Local.KillServer();
											s_LocalOpen = false;
										}
										return;
									}

									struct SMode
									{
										const char *m_pLabel;
										const char *m_pIcon;
										std::vector<const char *> m_vArgs;
									};
									const SMode aModes[] = {
										{Localize("Only me"), Ph::LOCK_SIMPLE, {"bindaddr 127.0.0.1", "sv_register 0"}},
										{Localize("Local network"), Ph::WIFI_HIGH, {"sv_register 0"}},
										{Localize("Internet"), Ph::GLOBE_HEMISPHERE_WEST, {"sv_register 1"}},
									};
									for(int i = 0; i < 3; i++)
									{
										char aKey[24];
										str_format(aKey, sizeof(aKey), "local.mode%d", i);
										const uint32_t Id = MakeId(aKey);
										const SResult R = Interact(Id);
										const float Hover = State(Id).m_Hover.Update(R.m_Hovered ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
										SBox Item;
										Item.m_W = Grow();
										Item.m_Gap = 12.0f;
										Item.m_Padding = SEdges(9.0f, 12.0f);
										Item.m_Cross = EAlign::CENTER;
										Item.m_Hit = EHit::INTERACTIVE;
										const int Node = BoxId(Id, EDir::ROW, Item, [&]() {
											Icon(aModes[i].m_pIcon, 18.0f, XuiTheme::Color::TEXT);
											SBox Names;
											Names.m_W = Grow();
											Names.m_Shrink = true;
											Names.m_Gap = 2.0f;
											Column(Names, [&]() {
												STextStyle L;
												L.m_Size = 14.0f;
												L.m_Font = EFont::SEMIBOLD;
												Text(aModes[i].m_pLabel, L);
											});
										});
										Paint(Node, [Hover](CXui &U, const SRect &Rc) { U.DrawRect(Rc, SolidRect(ColorRGBA(1, 1, 1, 0.04f + 0.06f * Hover), 12.0f)); });
										if(R.m_Clicked)
										{
											if(Local.RunServer(aModes[i].m_vArgs))
												Local.Connect();
											s_LocalOpen = false;
										}
									}
								});
							});
						});
					});
					Spacer();

					SBox List;
					List.m_W = Px(std::min(500.0f, m_ScreenUnits.x - 440.0f));
					List.m_Padding = SEdges(8.0f, 14.0f);
					List.m_Cross = EAlign::STRETCH;
					HomeCard(List, [&]() {
						SBox Head;
						Head.m_W = Grow();
						Head.m_Padding = SEdges(0.0f, 4.0f, 8.0f, 4.0f);
						Head.m_Main = EAlign::BETWEEN;
						Head.m_Cross = EAlign::CENTER;
						Row(Head, [&]() {
							STextStyle PopTitle;
							PopTitle.m_Size = 17.0f;
							PopTitle.m_Font = EFont::SEMIBOLD;
							PopTitle.m_Color = XuiTheme::Color::TEXT;
							Text(Localize("Popular now"), PopTitle);
							const uint32_t AllId = MakeId("all");
							const SResult AllR = Interact(AllId);
							const float AllHover = State(AllId).m_Hover.Update(AllR.m_Hovered ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
							SBox AllBox;
							AllBox.m_Hit = EHit::INTERACTIVE;
							BoxId(AllId, EDir::ROW, AllBox, [&]() {
								STextStyle All;
								All.m_Size = 12.0f;
								All.m_Font = EFont::SEMIBOLD;
								All.m_Color = Mix(XuiTheme::Color::TEXT3, XuiTheme::Color::TEXT, AllHover);
								Text(Localize("All servers"), All);
							});
							if(AllR.m_Clicked)
							{
								m_ShellHome = false;
								Menus.SetMenuPage(g_Config.m_UiPage >= CMenus::PAGE_INTERNET && g_Config.m_UiPage <= CMenus::PAGE_FAVORITE_COMMUNITY_5 ? g_Config.m_UiPage : CMenus::PAGE_INTERNET);
							}
						});

						if(s_Data.m_vPopular.empty())
						{
							STextStyle Empty;
							Empty.m_Size = 13.0f;
							Empty.m_Color = XuiTheme::Color::TEXT3;
							SBox EmptyBox;
							EmptyBox.m_Padding = SEdges(14.0f, 6.0f);
							Text(Client()->State() == IClient::STATE_OFFLINE ? Localize("Loading the server list...") : "", Empty, EmptyBox);
						}
						for(size_t i = 0; i < std::min<size_t>(3, s_Data.m_vPopular.size()); i++)
						{
							const SPopular &Pop = s_Data.m_vPopular[i];
							char aKey[16];
							str_format(aKey, sizeof(aKey), "pop%d", (int)i);
							const uint32_t RowId = MakeId(aKey);
							const SResult RowR = Interact(RowId, true, true);
							const float Hover = State(RowId).m_Hover.Update(RowR.m_Hovered ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
							if(RowR.m_Clicked)
								Menus.Connect(Pop.m_Address.c_str());
							SBox RowBox;
							RowBox.m_Gap = 14.0f;
							RowBox.m_Padding = SEdges(8.0f, 6.0f);
							RowBox.m_Cross = EAlign::CENTER;
							RowBox.m_Hit = EHit::INTERACTIVE;
							const int RowNode = BoxId(RowId, EDir::ROW, RowBox, [&]() {
								SBox Thumb;
								Thumb.m_W = Px(68.0f);
								Thumb.m_H = Px(46.0f);
								const int ThumbNode = Row(Thumb);
								CMapPreviews &Minimap = GameClient()->m_MapPreviews;
								const CMapPreviews::SPreview Preview = Minimap.Artwork(Pop.m_Map.c_str());
								Paint(ThumbNode, [Preview](CXui &U, const SRect &Rc) {
									if(Preview.Valid())
										U.DrawImage(Rc, Preview.m_Texture, vec2(Preview.m_Size.x, Preview.m_Size.y), 10.0f);
									else
										U.DrawRect(Rc, SolidRect(XuiTheme::Color::GROUP_FILL, 10.0f));
								});
								SBox Names;
								Names.m_W = Grow();
								Names.m_Shrink = true;
								Column(Names, [&]() {
									STextStyle MapStyle;
									MapStyle.m_Size = 15.0f;
									MapStyle.m_Font = EFont::BOLD;
									MapStyle.m_Wrap = true;
									MapStyle.m_MaxLines = 2;
									Text(Pop.m_Map.c_str(), MapStyle, {.m_W = Grow()});
									STextStyle NameStyle;
									NameStyle.m_Size = 12.0f;
									NameStyle.m_Color = XuiTheme::Color::TEXT3;
									NameStyle.m_Wrap = true;
									NameStyle.m_MaxLines = 2;
									Text(Pop.m_Name.c_str(), NameStyle, {.m_W = Grow()});
								});
								Tooltip(RowId, (Pop.m_Name + "\n" + Pop.m_Map + "\n" + Pop.m_Type).c_str());
								char aPlayers[32];
								str_format(aPlayers, sizeof(aPlayers), "%d/%d", Pop.m_Players, Pop.m_Max);
								STextStyle Players;
								Players.m_Size = 13.0f;
								Players.m_Font = EFont::SEMIBOLD;
								Players.m_Align = ETextAlign::RIGHT;
								Players.m_Tabular = true;
								SBox PlayersBox;
								PlayersBox.m_W = Px(70.0f);
								Text(aPlayers, Players, PlayersBox);
							});
							Paint(RowNode, [Hover](CXui &Ui, const SRect &Rc) {
								if(Hover > 0.01f)
									Ui.DrawRect(Rc, SolidRect(WithAlpha(XuiTheme::Color::ROW_HOVER, XuiTheme::Color::ROW_HOVER.a * Hover), 0.0f));
								Ui.DrawRect({Rc.m_X, Rc.m_Y, Rc.m_W, 1.0f}, SolidRect(ColorRGBA(1, 1, 1, 0.07f), 0.0f));
							});
						}
					});
				});

				SBox Friends;
				Friends.m_Gap = 2.0f;
				Friends.m_Padding = SEdges(16.0f, 10.0f, 10.0f, 10.0f);
				Friends.m_W = Px(320.0f);
				Friends.m_Cross = EAlign::STRETCH;
				Friends.m_Self = EAlign::START;
				Friends.m_H = Fit(0.0f, std::max(160.0f, m_ScreenUnits.y - 260.0f));
				Friends.m_Scroll = EScroll::VERTICAL;
				HomeCard(Friends, [&]() {
					SBox Head;
					Head.m_Padding = SEdges(0.0f, 8.0f, 8.0f, 8.0f);
					Head.m_Main = EAlign::BETWEEN;
					Head.m_Cross = EAlign::END;
					Row(Head, [&]() {
						STextStyle Title;
						Title.m_Size = 17.0f;
						Title.m_Font = EFont::BOLD;
						Text(Localize("Friends"), Title);
						char aOnline[32];
						str_format(aOnline, sizeof(aOnline), Localize("%d online"), s_Data.m_FriendsOnline);
						STextStyle Online;
						Online.m_Size = 12.5f;
						Online.m_Font = EFont::SEMIBOLD;
						Online.m_Color = XuiTheme::Color::TEXT3;
						Text(aOnline, Online);
					});
					if(s_Data.m_vFriends.empty())
					{
						STextStyle Empty;
						Empty.m_Size = 13.0f;
						Empty.m_Color = XuiTheme::Color::TEXT3;
						SBox EmptyBox;
						EmptyBox.m_Padding = SEdges(10.0f, 8.0f);
						Text(Localize(GameClient()->ServerBrowser()->IsGettingServerlist() ? "Updating friend status..." : "Nobody you know is online right now"), Empty, EmptyBox);
					}
					for(size_t i = 0; i < s_Data.m_vFriends.size(); i++)
					{
						const SFriendEntry &Friend = s_Data.m_vFriends[i];
						char aKey[16];
						str_format(aKey, sizeof(aKey), "fr%d", (int)i);
						const uint32_t RowId = MakeId(aKey);
						const SResult RowR = Interact(RowId, true, false);
						const float Hover = State(RowId).m_Hover.Update(RowR.m_Hovered ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
						SBox RowBox;
						RowBox.m_Gap = 12.0f;
						RowBox.m_Padding = SEdges(7.0f, 8.0f);
						RowBox.m_Cross = EAlign::CENTER;
						RowBox.m_Hit = EHit::INTERACTIVE;
						const int RowNode = BoxId(RowId, EDir::ROW, RowBox, [&]() {
							SBox Av;
							Av.m_W = Px(40.0f);
							Av.m_H = Px(40.0f);
							Av.m_Main = EAlign::CENTER;
							Av.m_Cross = EAlign::CENTER;
							const int AvNode = Row(Av, [&]() {
								if(Friend.m_Client.m_aSkin[0] || Friend.m_Client.m_aaSkin7[protocol7::SKINPART_BODY][0])
								{
									const CTeeRenderInfo Info = HomeFriendRenderInfo(*GameClient(), Friend.m_Client);
									SBox Tee;
									Tee.m_W = Tee.m_H = Px(34);
									Paint(Row(Tee), [Info](CXui &U, const SRect &Rc) { U.DrawTee(Info, 0, vec2(1, 0), Rc); });
								}
								else
									Icon(Ph::USER, 22, XuiTheme::Color::TEXT);
							});
							Paint(AvNode, [](CXui &Ui, const SRect &Rc) {
								Ui.DrawRect(Rc, SolidRect(ColorRGBA(0, 0, 0, 0.3f), Rc.m_H * 0.5f));
							});
							SBox Names;
							Names.m_W = Grow();
							Names.m_Shrink = true;
							Column(Names, [&]() {
								STextStyle Name;
								Name.m_Size = 14.0f;
								Name.m_Font = EFont::SEMIBOLD;
								Name.m_Wrap = true;
								Name.m_MaxLines = 2;
								Text(Friend.m_Name.c_str(), Name, {.m_W = Grow()});
								STextStyle Where;
								Where.m_Size = 12.0f;
								Where.m_Color = XuiTheme::Color::TEXT3;
								Where.m_Wrap = true;
								Where.m_MaxLines = 3;
								Text(Friend.m_Where.c_str(), Where, {.m_W = Grow()});
							});
							if(Button(Localize("Join"), {.m_Disabled = Friend.m_Address.empty(), .m_Height = 30.0f, .m_PadX = 12.0f, .m_FontSize = 12.5f, .m_pKey = "join"}).m_Clicked)
								Menus.Connect(Friend.m_Address.c_str());
						});
						Paint(RowNode, [Hover](CXui &Ui, const SRect &Rc) {
							if(Hover > 0.01f)
								Ui.DrawRect(Rc, SolidRect(WithAlpha(XuiTheme::Color::ROW_HOVER, XuiTheme::Color::ROW_HOVER.a * Hover), XuiTheme::Radius::SM));
						});
					} }, XuiTheme::Radius::XL, true);
			});
		});

		const SDockItem aDock[] = {
			{Localize("Home"), Ph::HOUSE, true},
			{Localize("Servers"), Ph::HARD_DRIVES, true},
			{Localize("Clips"), Ph::FILM_SLATE, true},
			{Localize("Settings"), Ph::SLIDERS_HORIZONTAL, true},
			{Localize("Editor"), Ph::PENCIL_SIMPLE, false},
			{Localize("Quit"), Ph::POWER, false},
		};
		int Selected = DockIndex;
		const int Clicked = Dock("dock", aDock, std::size(aDock), &Selected);
		if(Clicked >= 0)
			m_ShellProfile = false;
		if(Clicked == 0)
			m_ShellHome = true;
		else if(Clicked >= 1 && Clicked <= 3)
		{
			m_ShellHome = false;
			Menus.SetMenuPage(Clicked == 1 ? CMenus::PAGE_INTERNET : Clicked == 2 ? CMenus::PAGE_DEMOS :
												CMenus::PAGE_SETTINGS);
		}
		else if(Clicked == 4)
		{
			g_Config.m_ClEditor = 1;
			Input()->MouseModeRelative();
		}
		else if(Clicked == 5)
		{
			if(GameClient()->Editor()->HasUnsavedData() || (GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmQuitTime && g_Config.m_ClConfirmQuitTime >= 0))
				Menus.ShowQuitPopup();
			else
				Client()->Quit();
		}

		if(Client()->State() == IClient::STATE_CONNECTING || Client()->State() == IClient::STATE_LOADING)
			Layer(ELayer::MODAL, [&]() {
				SBox Wrap;
				Wrap.m_Position = EPos::ABSOLUTE;
				Wrap.m_Left = Wrap.m_Right = 24.0f;
				Wrap.m_Top = 90.0f;
				Wrap.m_Main = EAlign::CENTER;
				Row(Wrap, [&]() {
					SBox Card;
					Card.m_W = Px(std::min(440.0f, m_ScreenUnits.x - 48.0f));
					Card.m_Padding = SEdges(18.0f);
					Card.m_Gap = 10.0f;
					GlassPanel(Card, [&]() {
						const int Total = Client()->MapDownloadTotalsize();
						const int Amount = Client()->MapDownloadAmount();
						const bool Downloading = Total > 0 && Amount < Total;
						STextStyle Title;
						Title.m_Font = EFont::SEMIBOLD;
						Text(Localize(Downloading ? "Downloading map" : Client()->State() == IClient::STATE_LOADING ? "Loading map" : "Connecting"), Title);
						STextStyle Detail;
						Detail.m_Color = XuiTheme::Color::TEXT2;
						Detail.m_Wrap = true;
						Text(Client()->ConnectAddressString(), Detail, {.m_W = Grow()});
						if(Downloading)
						{
							char aProgress[256];
							str_format(aProgress, sizeof(aProgress), "%s · %d%% · %.1f / %.1f MiB", Client()->MapDownloadName(), (int)(100.0 * Amount / Total), Amount / 1048576.0, Total / 1048576.0);
							Text(aProgress, Detail, {.m_W = Grow()});
							ProgressBar("connect.progress", std::clamp((float)Amount / Total, 0.0f, 1.0f), std::min(404.0f, m_ScreenUnits.x - 84.0f));
						}
						else
							Spinner(20.0f, Accent());
						if(Button(Localize("Cancel"), {.m_Variant = EVariant::GHOST, .m_pKey = "connect.cancel"}).m_Clicked)
							Client()->Disconnect(); }, XuiTheme::Radius::XL);
				});
			});
		RenderShellPopup(Menus);
	});
}
