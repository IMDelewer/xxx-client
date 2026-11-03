#include <base/str.h>

#include <engine/demo.h>
#include <engine/font_icons_phosphor.h>
#include <engine/friends.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/xui/screens/settings.h>
#include <game/client/xui/xui.h>
#include <game/localization.h>
#include <game/voting.h>

using namespace Xui;

namespace
{
	struct SAction
	{
		const char *m_pLabel;
		const char *m_pIcon;
		bool m_On;
		bool m_Enabled;
	};

	bool ActionTile(CXui &Ui, const SAction &A, const char *pKey)
	{
		const uint32_t Id = Ui.MakeId(pKey);
		const SResult R = Ui.Interact(Id, A.m_Enabled);
		CXui::SState &St = Ui.State(Id);
		const float Hover = St.m_Hover.Update(R.m_Hovered ? 1.0f : 0.0f, Ui.Dt(), XuiTheme::Motion::HOVER);
		const float Scale = St.m_Scale.Update(R.m_Pressed ? XuiTheme::Motion::PRESS_SCALE : (R.m_Hovered ? XuiTheme::Motion::HOVER_SCALE : 1.0f), Ui.Dt(), XuiTheme::Motion::PRESS);
		SBox B;
		B.m_W = Grow();
		B.m_H = Px(66.0f);
		B.m_Gap = 6.0f;
		B.m_Main = EAlign::CENTER;
		B.m_Cross = EAlign::CENTER;
		B.m_Hit = A.m_Enabled ? EHit::INTERACTIVE : EHit::BLOCK;
		const ColorRGBA Accent = Ui.Accent();
		const ColorRGBA OnAccent = Ui.OnAccent();
		const bool On = A.m_On;
		const int Node = Ui.BoxId(Id, EDir::COLUMN, B, [&]() {
			Ui.Icon(A.m_pIcon, 20.0f, On ? OnAccent : XuiTheme::Color::TEXT, On);
			STextStyle L;
			L.m_Size = 11.5f;
			L.m_Font = EFont::SEMIBOLD;
			L.m_Color = On ? OnAccent : XuiTheme::Color::TEXT2;
			Ui.Text(A.m_pLabel, L);
		});
		Ui.SetTransform(Node, Scale, A.m_Enabled ? 1.0f : 0.4f);
		Ui.Paint(Node, [Hover, On, Accent](CXui &U, const SRect &Rc) {
			const ColorRGBA Fill = On ? Accent : CXui::Mix(ColorRGBA(1, 1, 1, 0.06f), ColorRGBA(1, 1, 1, 0.12f), Hover);
			U.DrawRect(Rc, CXui::SolidRect(Fill, 16.0f));
		});
		return R.m_Clicked;
	}

	bool SmallToggle(CXui &Ui, const char *pIcon, bool On, ColorRGBA OnColor, const char *pKey)
	{
		const uint32_t Id = Ui.MakeId(pKey);
		const SResult R = Ui.Interact(Id);
		CXui::SState &St = Ui.State(Id);
		const float Scale = St.m_Scale.Update(R.m_Pressed ? XuiTheme::Motion::PRESS_SCALE : (R.m_Hovered ? 1.15f : 1.0f), Ui.Dt(), XuiTheme::Motion::PRESS);
		SBox B;
		B.m_W = Px(30.0f);
		B.m_H = Px(30.0f);
		B.m_Main = EAlign::CENTER;
		B.m_Cross = EAlign::CENTER;
		B.m_Hit = EHit::INTERACTIVE;
		const int Node = Ui.BoxId(Id, EDir::ROW, B, [&]() { Ui.Icon(pIcon, 15.0f, On ? OnColor : XuiTheme::Color::TEXT3, On); });
		Ui.SetTransform(Node, Scale, 1.0f);
		Ui.Paint(Node, [](CXui &U, const SRect &Rc) { U.DrawRect(Rc, CXui::SolidRect(XuiTheme::Color::GHOST, Rc.m_H * 0.5f)); });
		return R.m_Clicked;
	}

	void InfoRow(CXui &Ui, const char *pKey, const char *pValue, bool First)
	{
		Ui.GroupRow([&]() {
			STextStyle K;
			K.m_Size = 14.0f;
			K.m_Color = XuiTheme::Color::TEXT2;
			SBox KB;
			KB.m_W = Grow();
			Ui.Text(pKey, K, KB);
			STextStyle V;
			V.m_Size = 14.0f;
			V.m_Font = EFont::SEMIBOLD;
			V.m_Tabular = true;
			V.m_Wrap = true;
			V.m_MaxLines = 1;
			SBox VB;
			VB.m_W = Fit(0.0f, 380.0f);
			Ui.Text(pValue, V, VB);
		},
			First, 46.0f);
	}
}

void CXui::RenderIngame(CMenus &M)
{
	if(m_HudEditor)
	{
		RunPage([&]() { RenderHudEditor(); });
		return;
	}
	CGameClient *pGame = GameClient();
	const int Local = pGame->m_Snap.m_LocalClientId;
	const CNetObj_PlayerInfo *pLocalInfo = pGame->m_Snap.m_pLocalInfo;
	const bool Paused = Local >= 0 && pGame->m_aClients[Local].m_Paused;
	const bool Spec = Local >= 0 && pGame->m_aClients[Local].m_Spec;
	const bool Spectator = pLocalInfo != nullptr && pLocalInfo->m_Team == TEAM_SPECTATORS;
	const CServerInfo &Info = Client()->ServerInfo();

	auto Close = [&]() { M.SetActive(false); };
	auto Disconnect = [&]() {
		if(pGame->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmDisconnectTime && g_Config.m_ClConfirmDisconnectTime >= 0)
			M.PopupConfirm(Localize("Disconnect"), Localize("Are you sure that you want to disconnect?"), Localize("Yes"), Localize("No"), &CMenus::PopupConfirmDisconnect);
		else
		{
			Client()->Disconnect();
			M.RefreshBrowserTab(true);
		}
	};

	const bool Wide = M.m_GamePage == CMenus::PAGE_NETWORK || M.m_GamePage == CMenus::PAGE_SETTINGS || M.m_GamePage == CMenus::PAGE_DEMOS;

	RunPage([&]() {
		SBox Scrim;
		Scrim.m_Position = EPos::ABSOLUTE;
		Scrim.m_Left = Scrim.m_Top = Scrim.m_Right = Scrim.m_Bottom = 0.0f;
		Scrim.m_Hit = EHit::INTERACTIVE;
		const uint32_t ScrimId = MakeId("ig.scrim");
		const SResult ScrimR = Interact(ScrimId, true, false);
		if(ScrimR.m_Clicked && M.m_Popup == CMenus::POPUP_NONE && !Wide)
			Close();
		const float Dim = Animate(ScrimId, 1.0f, XuiTheme::Motion::POP, 0, 0.0f);
		Paint(BoxId(ScrimId, EDir::ROW, Scrim), [Dim, Wide](CXui &U, const SRect &Rc) {
			U.DrawRect(Rc, SolidRect(ColorRGBA(0, 0, 0, (Wide ? 0.55f : 0.35f) * Dim), 0.0f));
		});

		if(Wide)
		{
			SBox Page;
			Page.m_Position = EPos::ABSOLUTE;
			Page.m_Left = 36.0f;
			Page.m_Right = 36.0f;
			Page.m_Top = 28.0f;
			Page.m_Bottom = 28.0f;
			Page.m_Gap = 14.0f;
			Page.m_Hit = EHit::BLOCK;
			Column(Page, [&]() {
				SBox Bar;
				Bar.m_W = Grow();
				Bar.m_Gap = 12.0f;
				Bar.m_Cross = EAlign::CENTER;
				Row(Bar, [&]() {
					if(Button(Localize("Back"), {.m_Variant = EVariant::GLASS, .m_pIcon = Ph::ARROW_LEFT, .m_pKey = "ig.back"}).m_Clicked || (EscPressed() && M.m_Popup == CMenus::POPUP_NONE))
						M.m_GamePage = CMenus::PAGE_GAME;
					STextStyle T;
					T.m_Size = 22.0f;
					T.m_Font = EFont::EXTRABOLD;
					Text(M.m_GamePage == CMenus::PAGE_SETTINGS ? Localize("Settings") : Localize("Server browser"), T);
				});
				SBox Body;
				Body.m_W = Grow();
				Body.m_H = Grow();
				Row(Body, [&]() {
					if(M.m_GamePage == CMenus::PAGE_SETTINGS)
						RenderSettings(*this);
					else if(M.m_GamePage == CMenus::PAGE_DEMOS)
						RenderDemos(M);
					else
						RenderBrowser(M);
				});
			});
			RenderShellPopup(M);
			return;
		}

		SBox Wrap;
		Wrap.m_Position = EPos::ABSOLUTE;
		Wrap.m_Left = Wrap.m_Right = 0.0f;
		Wrap.m_Top = 110.0f;
		Wrap.m_Main = EAlign::CENTER;
		Row(Wrap, [&]() {
			SBox Panel;
			Panel.m_W = Px(std::min(760.0f, m_ScreenUnits.x - 48.0f));
			Panel.m_Padding = SEdges(14.0f);
			Panel.m_Gap = 14.0f;
			Panel.m_Hit = EHit::BLOCK;
			const uint32_t PanelId = MakeId("ig.panel");
			const float P = Presence(PanelId, true);
			const int PanelNode = BoxId(PanelId, EDir::COLUMN, Panel, [&]() {
				SBox Head;
				Head.m_W = Grow();
				Head.m_Gap = 14.0f;
				Head.m_Cross = EAlign::CENTER;
				Row(Head, [&]() {
					SBox Thumb;
					Thumb.m_W = Px(96.0f);
					Thumb.m_H = Px(60.0f);
					Thumb.m_Main = EAlign::CENTER;
					Thumb.m_Cross = EAlign::CENTER;
					CMapPreviews &Minimap = pGame->m_MapPreviews;
					const CMapPreviews::SPreview Preview = Minimap.Preview(pGame->Map()->BaseName(), 0);
					const int ThumbNode = Row(Thumb, [&]() {
						if(!Preview.Valid())
							Icon(Ph::MAP_TRIFOLD, 24.0f, ColorRGBA(1, 1, 1, 0.7f));
					});
					const ColorRGBA Accent = m_Accent;
					Paint(ThumbNode, [Accent, Preview, &Minimap](CXui &U, const SRect &Rc) {
						IGraphics::SUiRect R = SolidRect(Preview.Valid() ? ColorRGBA(0.06f, 0.06f, 0.08f, 1.0f) : Mix(Accent, ColorRGBA(0, 0, 0, 1), 0.35f), 14.0f);
						if(!Preview.Valid())
						{
							R.m_Fill2 = Mix(Accent, ColorRGBA(0, 0, 0, 1), 0.75f);
							R.m_GradientAngle = 135.0f;
						}
						U.DrawRect(Rc, R);
						if(Preview.Valid())
						{
							const float A = U.CurrentAlpha();
							U.DrawDirect({Rc.m_X + 4.0f, Rc.m_Y + 4.0f, Rc.m_W - 8.0f, Rc.m_H - 8.0f}, [&Minimap, Preview, A](const SRect &Px) {
								Minimap.DrawPreview(Preview, Px.m_X, Px.m_Y, Px.m_W, Px.m_H, A);
							});
						}
					});
					SBox Names;
					Names.m_W = Grow();
					Names.m_Shrink = true;
					Names.m_Gap = 2.0f;
					Column(Names, [&]() {
						STextStyle MapStyle;
						MapStyle.m_Size = 20.0f;
						MapStyle.m_Font = EFont::EXTRABOLD;
						MapStyle.m_Tracking = -0.02f;
						MapStyle.m_Wrap = true;
						MapStyle.m_MaxLines = 1;
						SBox GB;
						GB.m_W = Grow();
						Text(Info.m_aMap[0] != '\0' ? Info.m_aMap : pGame->Map()->BaseName(), MapStyle, GB);
						char aSub[256];
						str_format(aSub, sizeof(aSub), "%s · %s", Info.m_aName, Client()->ConnectAddressString());
						STextStyle Sub;
						Sub.m_Size = 12.5f;
						Sub.m_Color = XuiTheme::Color::TEXT3;
						Sub.m_Wrap = true;
						Sub.m_MaxLines = 1;
						Sub.m_Tabular = true;
						Text(aSub, Sub, GB);
					});
					if(Button(Localize("Resume"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::PLAY, .m_IconFill = true, .m_pKey = "ig.resume"}).m_Clicked)
						Close();
				});

				{
					const bool Recording = Client()->DemoRecorder(RECORDER_MANUAL)->IsRecording();
					const bool DummyBusy = Client()->DummyConnecting() || Client()->DummyConnectingDelayed();
					const SAction aActions[] = {
						{Localize("Kill"), Ph::SKULL, false, pLocalInfo != nullptr && !Spectator},
						{Spectator || Spec ? Localize("Join") : Localize("Spectate"), Spectator || Spec ? Ph::SIGN_IN : Ph::EYE, Spectator || Spec, pLocalInfo != nullptr && !Client()->DummyConnecting()},
						{Paused ? Localize("Unpause") : Localize("Pause"), Paused ? Ph::PLAY : Ph::PAUSE, Paused, pGame->m_ReceivedDDNetPlayer && pLocalInfo != nullptr},
						{Localize("Dummy"), Ph::USERS_THREE, Client()->DummyConnected(), !DummyBusy && Client()->DummyAllowed()},
						{Recording ? Localize("Stop") : Localize("Record"), Recording ? Ph::STOP : Ph::RECORD, Recording, true},
						{Localize("Settings"), Ph::GEAR_SIX, false, true},
					};
					{
						SBox Grid;
						Grid.m_W = Grow();
						Grid.m_Gap = 8.0f;
						Row(Grid, [&]() {
							for(int i = 0; i < 6; i++)
							{
								if((i == 2 && !pGame->m_ReceivedDDNetPlayer) || (i == 3 && !Client()->DummyAllowed()))
									continue;
								char aKey[16];
								str_format(aKey, sizeof(aKey), "ig.act%d", i);
								if(!ActionTile(*this, aActions[i], aKey))
									continue;
								switch(i)
								{
								case 0:
									pGame->SendKill();
									Close();
									break;
								case 1:
									if(Spec || Paused)
										Console()->ExecuteLine("say /pause", IConsole::CLIENT_ID_UNSPECIFIED);
									else if(Spectator)
										pGame->SendSwitchTeam(TEAM_GAME);
									else
										pGame->SendSwitchTeam(TEAM_SPECTATORS);
									Close();
									break;
								case 2:
									Console()->ExecuteLine("say /pause", IConsole::CLIENT_ID_UNSPECIFIED);
									Close();
									break;
								case 3:
									if(Client()->DummyConnected())
										Client()->DummyDisconnect(nullptr);
									else
										Client()->DummyConnect();
									Close();
									break;
								case 4:
									if(Recording)
										Client()->DemoRecorder(RECORDER_MANUAL)->Stop(IDemoRecorder::EStopMode::KEEP_FILE);
									else
										Client()->DemoRecorder_Start(pGame->Map()->BaseName(), true, RECORDER_MANUAL);
									break;
								case 5:
									M.m_GamePage = CMenus::PAGE_SETTINGS;
									break;
								}
							}
						});
					}
				}
				if(M.m_GamePage == CMenus::PAGE_GAME)
					M.m_GamePage = CMenus::PAGE_PLAYERS;
				constexpr int PAGE_XX_ADMIN = 1000;
				const bool Admin = Client()->RconAuthed();
				if(!Admin && M.m_GamePage == PAGE_XX_ADMIN)
					M.m_GamePage = CMenus::PAGE_PLAYERS;
				static const int s_aPages[] = {CMenus::PAGE_PLAYERS, CMenus::PAGE_CALLVOTE, CMenus::PAGE_SERVER_INFO, PAGE_XX_ADMIN};
				const char *apTabs[] = {Localize("Players"), Localize("Vote"), Localize("Server info"), Localize("Admin")};
				const int NumTabs = Admin ? 4 : 3;
				int Tab = 0;
				for(int i = 0; i < NumTabs; i++)
					if(M.m_GamePage == s_aPages[i])
						Tab = i;
				if(Segmented("ig.tabs", apTabs, NumTabs, &Tab).m_Changed)
					M.m_GamePage = s_aPages[Tab];

				if(M.m_GamePage == CMenus::PAGE_PLAYERS)
				{
					SBox ListBox;
					ListBox.m_W = Grow();
					ListBox.m_H = Fit(0.0f, 330.0f);
					Scroll(ListBox, [&]() {
						Group([&]() {
							bool First = true;
							int Count = 0;
							for(int i = 0; i < MAX_CLIENTS; i++)
							{
								const CNetObj_PlayerInfo *pInfo = pGame->m_Snap.m_apInfoByName[i];
								if(pInfo == nullptr)
									continue;
								const int Id = pInfo->m_ClientId;
								const bool Self = Id == Local || (Client()->DummyConnected() && Id == pGame->m_aLocalIds[!g_Config.m_ClDummy]);
								CGameClient::CClientData &C = pGame->m_aClients[Id];
								Count++;
								PushId((uint32_t)Id + 1);
								GroupRow([&]() {
									SBox TeeBox;
									TeeBox.m_W = Px(30.0f);
									TeeBox.m_H = Px(30.0f);
									const int TeeNode = Row(TeeBox);
									CTeeRenderInfo TeeInfo = C.m_RenderInfo;
									Paint(TeeNode, [TeeInfo](CXui &U, const SRect &Rc) { U.DrawTee(TeeInfo, 0, vec2(1.0f, 0.0f), Rc); });
									SBox NameBox;
									NameBox.m_W = Grow();
									NameBox.m_Shrink = true;
									NameBox.m_Gap = 1.0f;
									Column(NameBox, [&]() {
										STextStyle N;
										N.m_Size = 14.5f;
										N.m_Font = EFont::SEMIBOLD;
										N.m_Color = C.m_Friend ? XuiTheme::Color::GREEN : XuiTheme::Color::TEXT;
										N.m_Wrap = true;
										N.m_MaxLines = 1;
										SBox GB;
										GB.m_W = Grow();
										Text(C.m_aName, N, GB);
										if(C.m_aClan[0] != '\0')
										{
											STextStyle Cl;
											Cl.m_Size = 12.0f;
											Cl.m_Color = XuiTheme::Color::TEXT3;
											Text(C.m_aClan, Cl, GB);
										}
									});
									if(Self)
										Badge(Id == Local ? Localize("You") : Localize("Dummy"), m_Accent);
									if(pInfo->m_Team == TEAM_SPECTATORS || C.m_Spec || C.m_Paused)
										Badge(Localize("Spec"), XuiTheme::Color::TEXT3);
									{
										char aPing[16];
										str_format(aPing, sizeof(aPing), "%d ms", pInfo->m_Latency);
										STextStyle P;
										P.m_Size = 12.0f;
										P.m_Font = EFont::SEMIBOLD;
										P.m_Tabular = true;
										P.m_Color = pInfo->m_Latency < 80 ? XuiTheme::Color::GREEN : pInfo->m_Latency < 150 ? XuiTheme::Color::YELLOW :
																				      XuiTheme::Color::RED_SOFT;
										SBox PB;
										PB.m_W = Px(52.0f);
										Text(aPing, P, PB);
									}
									if(Self)
										return;
									if(SmallToggle(*this, C.m_ChatIgnore ? Ph::SPEAKER_SLASH : Ph::SPEAKER_HIGH, C.m_ChatIgnore, XuiTheme::Color::RED_SOFT, "mute"))
										C.m_ChatIgnore ^= 1;
									if(SmallToggle(*this, Ph::HEART, C.m_Friend, XuiTheme::Color::PINK, "friend"))
									{
										if(C.m_Friend)
											pGame->Friends()->RemoveFriend(C.m_aName, C.m_aClan);
										else
											pGame->Friends()->AddFriend(C.m_aName, C.m_aClan);
									}
								},
									First, 52.0f);
								PopId();
								First = false;
							}
							if(Count == 0)
								GroupRow([&]() {
									STextStyle S;
									S.m_Size = 14.0f;
									S.m_Color = XuiTheme::Color::TEXT3;
									Text(Localize("Nobody else is here"), S);
								},
									true, 52.0f);
						});
					});
				}
				else if(M.m_GamePage == CMenus::PAGE_CALLVOTE)
					RenderVoteTab(M);
				else if(M.m_GamePage == PAGE_XX_ADMIN)
				{
					const SAction aAdmin[] = {
						{Localize("Super"), Ph::LIGHTNING, false, true},
						{Localize("Normal"), Ph::USER, false, true},
						{Localize("Restart"), Ph::ARROW_CLOCKWISE, false, true},
						{Localize("Reload map"), Ph::MAP_TRIFOLD, false, true},
						{Localize("Test cmds"), Ph::FLASK, false, true},
						{Localize("Clients info"), Ph::FINGERPRINT, false, true},
					};
					static const char *const s_apAdminCmds[] = {"super", "unsuper", "restart", "reload", "toggle sv_test_cmds 0 1", "xx_debug_clients"};
					SBox Grid;
					Grid.m_W = Grow();
					Grid.m_Gap = 8.0f;
					Row(Grid, [&]() {
						for(int i = 0; i < 6; i++)
						{
							char aKey[16];
							str_format(aKey, sizeof(aKey), "ig.adm%d", i);
							if(Console()->GetCommandInfo(s_apAdminCmds[i], CFGFLAG_SERVER, true) == nullptr && i != 4)
								continue;
							if(i == 4 && Console()->GetCommandInfo("sv_test_cmds", CFGFLAG_SERVER, true) == nullptr)
								continue;
							if(ActionTile(*this, aAdmin[i], aKey))
							{
								Client()->Rcon(s_apAdminCmds[i]);
								ShowToast(aAdmin[i].m_pLabel, aAdmin[i].m_pIcon);
							}
						}
					});
					static char s_aMap[128] = "";
					static char s_aName[64] = "";
					static char s_aPassword[64] = "";
					static char s_aCommand[256] = "";
					auto FieldRow = [&](const char *pKey, char *pBuf, int Size, const char *pPlaceholder, const char *pIcon, const char *pButton, const char *pFormat) {
						SBox R;
						R.m_W = Grow();
						R.m_Gap = 8.0f;
						R.m_Cross = EAlign::CENTER;
						Row(R, [&]() {
							const SResult F = Field(pKey, pBuf, Size, {.m_pPlaceholder = pPlaceholder, .m_pIcon = pIcon, .m_W = Grow()});
							char aBtnKey[32];
							str_format(aBtnKey, sizeof(aBtnKey), "%s.go", pKey);
							if((Button(pButton, {.m_Variant = EVariant::GLASS, .m_pKey = aBtnKey}).m_Clicked || F.m_Submitted) && (pBuf[0] != '\0' || str_find(pFormat, "password")))
							{
								char aCmd[512];
								str_format(aCmd, sizeof(aCmd), pFormat, pBuf);
								Client()->Rcon(aCmd);
								ShowToast(aCmd, Ph::TERMINAL);
								if(str_find(pFormat, "%s\"") == nullptr)
									pBuf[0] = '\0';
							}
						});
					};
					Group([&]() {
						if(Console()->GetCommandInfo("change_map", CFGFLAG_SERVER, true))
							GroupRow([&]() { FieldRow("ig.adm.map", s_aMap, sizeof(s_aMap), Localize("Map name"), Ph::MAP_TRIFOLD, Localize("Change map"), "change_map \"%s\""); }, true);
						if(Console()->GetCommandInfo("sv_name", CFGFLAG_SERVER, true))
							GroupRow([&]() { FieldRow("ig.adm.name", s_aName, sizeof(s_aName), Localize("Server name"), Ph::TAG, Localize("Rename"), "sv_name \"%s\""); }, false);
						if(Console()->GetCommandInfo("password", CFGFLAG_SERVER, true))
							GroupRow([&]() { FieldRow("ig.adm.pw", s_aPassword, sizeof(s_aPassword), Localize("Password (empty = none)"), Ph::LOCK_SIMPLE, Localize("Set"), "password \"%s\""); }, false);
						GroupRow([&]() { FieldRow("ig.adm.cmd", s_aCommand, sizeof(s_aCommand), Localize("Any rcon command"), Ph::TERMINAL, Localize("Run"), "%s"); }, false);
					});
				}
				else
				{
					char aPlayers[32], aPing[32];
					str_format(aPlayers, sizeof(aPlayers), "%d / %d", Info.m_NumClients, Info.m_MaxClients);
					str_format(aPing, sizeof(aPing), "%d ms", pGame->m_Snap.m_pLocalInfo ? pGame->m_Snap.m_apPlayerInfos[Local] ? pGame->m_Snap.m_apPlayerInfos[Local]->m_Latency : 0 : 0);
					Group([&]() {
						InfoRow(*this, Localize("Map"), Info.m_aMap[0] != '\0' ? Info.m_aMap : pGame->Map()->BaseName(), true);
						InfoRow(*this, Localize("Server"), Info.m_aName, false);
						InfoRow(*this, Localize("Address"), Client()->ConnectAddressString(), false);
						InfoRow(*this, Localize("Game type"), Info.m_aGameType, false);
						InfoRow(*this, Localize("Players"), aPlayers, false);
						InfoRow(*this, Localize("Ping"), aPing, false);
						InfoRow(*this, Localize("Version"), Info.m_aVersion, false);
					});
					SBox Copy;
					Copy.m_W = Grow();
					Row(Copy, [&]() {
						if(Button(Localize("Copy address"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::COPY, .m_pKey = "ig.copy"}).m_Clicked)
						{
							Input()->SetClipboardText(Client()->ConnectAddressString());
							ShowToast(Localize("Address copied"), Ph::COPY);
						}
					});
				}

				SBox Bottom;
				Bottom.m_W = Grow();
				Bottom.m_Gap = 10.0f;
				Row(Bottom, [&]() {
					if(Button(Localize("Server browser"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::HARD_DRIVES, .m_Height = 48.0f, .m_FontSize = 15.0f, .m_W = Grow(), .m_pKey = "ig.browser"}).m_Clicked)
					{
						M.m_GamePage = CMenus::PAGE_NETWORK;
						M.RefreshBrowserTab(false);
					}
					if(Button(Localize("Disconnect"), {.m_Variant = EVariant::ACCENT, .m_pIcon = Ph::SIGN_OUT, .m_Height = 48.0f, .m_FontSize = 15.0f, .m_W = Grow(), .m_pKey = "ig.disconnect"}).m_Clicked)
						Disconnect();
				});
			});
			SetTransform(PanelNode, XuiTheme::Motion::POP_FROM + (1.0f - XuiTheme::Motion::POP_FROM) * P, std::clamp(P * 1.4f, 0.0f, 1.0f));
			Paint(PanelNode, [](CXui &U, const SRect &Rc) {
				U.DrawRect(Rc, GlassRect(36.0f, 0.8f));
				U.DrawRect(Rc, GlassHairline(36.0f));
			});
		});

		RenderShellPopup(M);
		RenderToasts();
		if(EscPressed() && M.m_Popup == CMenus::POPUP_NONE)
			Close();
	});
}
