#include <base/color.h>
#include <base/str.h>

#include <engine/demo.h>
#include <engine/font_icons_phosphor.h>
#include <engine/friends.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/warning.h>

#include <game/client/components/menus.h>
#include <game/client/components/touch_controls.h>
#include <game/client/gameclient.h>
#include <game/client/xui/screens/settings.h>
#include <game/client/xui/screens/settings_internal.h>
#include <game/client/xui/xui.h>
#include <game/localization.h>
#include <game/voting.h>

#include <algorithm>
#include <array>
#include <deque>
#include <optional>
#include <string>
#include <vector>

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
	if(g_Config.m_ClTouchControls && GameClient()->m_TouchControls.IsEditingActive())
	{
		RunPage([&]() { RenderTouchControlsEditor(M); });
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
				static const int s_aPages[] = {CMenus::PAGE_PLAYERS, CMenus::PAGE_CALLVOTE, CMenus::PAGE_SERVER_INFO, PAGE_XX_ADMIN};
				const char *apTabs[] = {Localize("Players"), Localize("Vote"), Localize("Server info"), Localize("Admin")};
				constexpr int NumTabs = 4;
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
					if(!Admin)
					{
						if(Button(Localize("Open rcon console"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::TERMINAL_WINDOW, .m_W = Grow(), .m_pKey = "ig.rcon.open"}).m_Clicked)
							GameClient()->m_GameConsole.Toggle(CGameConsole::CONSOLETYPE_REMOTE);
					}
					else
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

				SBox ConsoleRow;
				ConsoleRow.m_W = Grow();
				ConsoleRow.m_Gap = 10.0f;
				Row(ConsoleRow, [&]() {
					if(Button(Localize("Console"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::TERMINAL_WINDOW, .m_Height = 48.0f, .m_FontSize = 15.0f, .m_W = Grow(), .m_pKey = "ig.console"}).m_Clicked)
						GameClient()->m_GameConsole.Toggle(CGameConsole::CONSOLETYPE_LOCAL);
					if(Button(Localize("Rcon"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::SHIELD_STAR, .m_Height = 48.0f, .m_FontSize = 15.0f, .m_W = Grow(), .m_pKey = "ig.rcon"}).m_Clicked)
						GameClient()->m_GameConsole.Toggle(CGameConsole::CONSOLETYPE_REMOTE);
				});
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

// ===== Touch controls editor =====
// Drives the CTouchControls editor core (the same engine as upstream DDNet):
// gestures are handled by CTouchControls::UpdateButtonsEditor while the menu is
// closed (drag = move, pinch = resize, long-press = select/deselect), this page
// provides the property panel, the button list and the file operations.

namespace
{
	constexpr int TOUCH_VIS_EXCLUDE = 0;
	constexpr int TOUCH_VIS_INCLUDE = 1;
	constexpr int TOUCH_VIS_IGNORE = 2;

	constexpr int TOUCH_POPUP_NONE = -1;
	constexpr int TOUCH_POPUP_BUTTON_CHANGED = 0;
	constexpr int TOUCH_POPUP_NO_SPACE = 1;
	constexpr int TOUCH_POPUP_BUTTON_INVISIBLE = 2;
	constexpr int TOUCH_POPUP_TURN_OFF = 3;
	constexpr int TOUCH_POPUP_DISCARD = 4;
	constexpr int TOUCH_POPUP_RESET = 5;
	constexpr int TOUCH_POPUP_IMPORT = 6;

	constexpr int TOUCH_PREDEFINED_EXTRA_MENU = 1;

	struct STouchCmdState
	{
		char m_aLabel[256] = "";
		char m_aCommand[1024] = "";
		int m_LabelType = 0; // CTouchControls::CButtonLabel::EType
	};

	struct STouchEditorState
	{
		int m_Tab = 0; // 0 = selected button, 1 = all buttons
		int m_X = 0;
		int m_Y = 0;
		int m_W = CTouchControls::BUTTON_SIZE_MINIMUM;
		int m_H = CTouchControls::BUTTON_SIZE_MINIMUM;
		int m_Shape = (int)CTouchControls::EButtonShape::RECT;
		int m_aVisibility[(int)CTouchControls::EButtonVisibility::NUM_VISIBILITIES] = {};
		int m_BehaviorType = 0; // 0 = bind, 1 = bind toggle, 2 = predefined
		int m_PredefinedType = 0;
		int m_ExtraMenu = 0;
		std::deque<STouchCmdState> m_Commands{2};
		size_t m_NumCommands = 2;
		unsigned m_HslInactive = 0;
		unsigned m_HslActive = 0;
		int m_PopupType = TOUCH_POPUP_NONE;
		bool m_PopupOpen = false;
		CTouchControls::CTouchButton *m_pPopupOld = nullptr;
		CTouchControls::CTouchButton *m_pPopupNew = nullptr;
		bool m_CloseAfterPopup = false;
	};

	STouchEditorState &TouchEd()
	{
		static STouchEditorState s_State;
		return s_State;
	}

	void TouchResizeCommands(STouchEditorState &Ed, size_t Count)
	{
		if(Count > Ed.m_Commands.size())
			Ed.m_Commands.resize(Count);
		if(Count > Ed.m_NumCommands)
			std::fill(Ed.m_Commands.begin() + Ed.m_NumCommands, Ed.m_Commands.begin() + Count, STouchCmdState{});
		Ed.m_NumCommands = Count;
	}

	void TouchResetCache()
	{
		STouchEditorState &Ed = TouchEd();
		Ed.m_X = 0;
		Ed.m_Y = 0;
		Ed.m_W = CTouchControls::BUTTON_SIZE_MINIMUM;
		Ed.m_H = CTouchControls::BUTTON_SIZE_MINIMUM;
		Ed.m_Shape = (int)CTouchControls::EButtonShape::RECT;
		std::fill(std::begin(Ed.m_aVisibility), std::end(Ed.m_aVisibility), TOUCH_VIS_IGNORE);
		Ed.m_aVisibility[(int)CTouchControls::EButtonVisibility::DEMO_PLAYER] = TOUCH_VIS_EXCLUDE;
		Ed.m_BehaviorType = 0;
		Ed.m_PredefinedType = 0;
		Ed.m_ExtraMenu = 0;
		TouchResizeCommands(Ed, 2);
		std::fill_n(Ed.m_Commands.begin(), Ed.m_NumCommands, STouchCmdState{});
	}

	std::unique_ptr<CTouchControls::CPredefinedTouchButtonBehavior> TouchMakePredefined(int Type)
	{
		using C = CTouchControls;
		switch(Type)
		{
		case 0: return std::make_unique<C::CIngameMenuTouchButtonBehavior>();
		case 1: return std::make_unique<C::CExtraMenuTouchButtonBehavior>(0);
		case 2: return std::make_unique<C::CEmoticonTouchButtonBehavior>();
		case 3: return std::make_unique<C::CSpectateTouchButtonBehavior>();
		case 4: return std::make_unique<C::CSwapActionTouchButtonBehavior>();
		case 5: return std::make_unique<C::CUseActionTouchButtonBehavior>();
		case 6: return std::make_unique<C::CJoystickActionTouchButtonBehavior>();
		case 7: return std::make_unique<C::CJoystickAimTouchButtonBehavior>();
		case 8: return std::make_unique<C::CJoystickAimRelativeTouchButtonBehavior>();
		case 9: return std::make_unique<C::CJoystickFireTouchButtonBehavior>();
		default: return std::make_unique<C::CJoystickHookTouchButtonBehavior>();
		}
	}

	int TouchPredefinedTypeById(const char *pId)
	{
		static const char *const s_apIds[] = {
			CTouchControls::CIngameMenuTouchButtonBehavior::BEHAVIOR_ID,
			CTouchControls::CExtraMenuTouchButtonBehavior::BEHAVIOR_ID,
			CTouchControls::CEmoticonTouchButtonBehavior::BEHAVIOR_ID,
			CTouchControls::CSpectateTouchButtonBehavior::BEHAVIOR_ID,
			CTouchControls::CSwapActionTouchButtonBehavior::BEHAVIOR_ID,
			CTouchControls::CUseActionTouchButtonBehavior::BEHAVIOR_ID,
			CTouchControls::CJoystickActionTouchButtonBehavior::BEHAVIOR_ID,
			CTouchControls::CJoystickAimTouchButtonBehavior::BEHAVIOR_ID,
			CTouchControls::CJoystickAimRelativeTouchButtonBehavior::BEHAVIOR_ID,
			CTouchControls::CJoystickFireTouchButtonBehavior::BEHAVIOR_ID,
			CTouchControls::CJoystickHookTouchButtonBehavior::BEHAVIOR_ID,
		};
		for(int i = 0; i < (int)(sizeof(s_apIds) / sizeof(s_apIds[0])); i++)
		{
			if(str_comp(pId, s_apIds[i]) == 0)
				return i;
		}
		return 0;
	}

	const char *TouchPredefinedName(int Type)
	{
		static const char *s_apNames[] = {
			Localize("Ingame menu", "Predefined touch button behavior"),
			Localize("Extra menu", "Predefined touch button behavior"),
			Localize("Emoticon", "Predefined touch button behavior"),
			Localize("Spectate", "Predefined touch button behavior"),
			Localize("Swap action", "Predefined touch button behavior"),
			Localize("Use action", "Predefined touch button behavior"),
			Localize("Joystick action", "Predefined touch button behavior"),
			Localize("Joystick aim", "Predefined touch button behavior"),
			Localize("Joystick relative aim", "Predefined touch button behavior"),
			Localize("Joystick fire", "Predefined touch button behavior"),
			Localize("Joystick hook", "Predefined touch button behavior"),
		};
		return s_apNames[std::clamp(Type, 0, (int)(sizeof(s_apNames) / sizeof(s_apNames[0])) - 1)];
	}

	const char *TouchPredefinedHelp(int Type)
	{
		switch(Type)
		{
		case 0: return Localize("Opens the ingame menu immediately when released.");
		case 1: return Localize("Toggles visibility of buttons with an extra menu visibility. Also opens the ingame menu on long press.");
		case 2: return Localize("Opens the emoticon selector (this does not work with binds).");
		case 3: return Localize("Opens the spectator menu (this does not work with binds).");
		case 4: return Localize("Swaps the active action (fire and hook) for direct touch input and virtual joysticks.");
		case 5: return Localize("Uses the active action with the current aiming position.");
		case 6: return Localize("Virtual joystick which uses the active action.");
		case 7: return Localize("Virtual joystick which only aims without using an action.");
		case 8: return Localize("Virtual joystick which only aims and moves the mouse pointer relatively.");
		case 9: return Localize("Virtual joystick which always uses fire.");
		default: return Localize("Virtual joystick which always uses hook.");
		}
	}

	void TouchVisibilityTitle(char *pBuf, int Size, int Type)
	{
		static const char *s_apNames[] = {
			Localize("Ingame", "Touch button visibility"),
			Localize("Zoom allowed", "Touch button visibility"),
			Localize("Vote active", "Touch button visibility"),
			Localize("Dummy allowed", "Touch button visibility"),
			Localize("Dummy connected", "Touch button visibility"),
			Localize("Rcon authed", "Touch button visibility"),
			Localize("Demo player", "Touch button visibility"),
			Localize("Extra menu", "Touch button visibility"),
		};
		if(Type < (int)CTouchControls::EButtonVisibility::EXTRA_MENU_1)
			str_copy(pBuf, s_apNames[Type], Size);
		else
			str_format(pBuf, Size, "%s %d", s_apNames[(int)CTouchControls::EButtonVisibility::EXTRA_MENU_1], Type - (int)CTouchControls::EButtonVisibility::EXTRA_MENU_1 + 1);
	}

	std::vector<std::string> TouchVisibilityOptions()
	{
		return {
			Localize("Default", "Touch button visibility"),
			Localize("Always show", "Touch button visibility"),
			Localize("Never show", "Touch button visibility"),
		};
	}

	std::vector<std::string> TouchBehaviorOptions()
	{
		return {
			Localize("Bind", "Touch button behavior"),
			Localize("Bind toggle", "Touch button behavior"),
			Localize("Predefined", "Touch button behavior"),
		};
	}

	std::vector<std::string> TouchLabelTypeOptions()
	{
		return {
			Localize("Plain", "Touch button label type"),
			Localize("Localized", "Touch button label type"),
			Localize("Icon", "Touch button label type"),
		};
	}

	std::vector<std::string> TouchDirectTouchIngameOptions()
	{
		return {
			Localize("Disabled", "Direct touch mode"),
			Localize("Action", "Direct touch mode"),
			Localize("Aim", "Direct touch mode"),
			Localize("Relative aim", "Direct touch mode"),
			Localize("Fire", "Direct touch mode"),
			Localize("Hook", "Direct touch mode"),
		};
	}

	std::vector<std::string> TouchDirectTouchSpectateOptions()
	{
		return {
			Localize("Disabled", "Direct touch mode"),
			Localize("Aim", "Direct touch mode"),
		};
	}

	// Mirror of upstream CMenusIngameTouchControls::CacheAllSettingsFromTarget.
	void TouchCacheFromTarget(CTouchControls::CTouchButton *pButton)
	{
		TouchResetCache();
		if(pButton == nullptr)
			return;
		STouchEditorState &Ed = TouchEd();
		Ed.m_X = pButton->m_UnitRect.m_X;
		Ed.m_Y = pButton->m_UnitRect.m_Y;
		Ed.m_W = pButton->m_UnitRect.m_W;
		Ed.m_H = pButton->m_UnitRect.m_H;
		Ed.m_Shape = (int)pButton->m_Shape;
		for(const auto &Visibility : pButton->m_vVisibilities)
			Ed.m_aVisibility[(int)Visibility.m_Type] = Visibility.m_Parity ? TOUCH_VIS_INCLUDE : TOUCH_VIS_EXCLUDE;
		const char *pType = pButton->m_pBehavior != nullptr ? pButton->m_pBehavior->GetBehaviorType() : "";
		if(str_comp(pType, CTouchControls::CBindTouchButtonBehavior::BEHAVIOR_TYPE) == 0)
		{
			Ed.m_BehaviorType = 0;
			auto *pBehavior = static_cast<CTouchControls::CBindTouchButtonBehavior *>(pButton->m_pBehavior.get());
			CTouchControls::CButtonLabel Label = pBehavior->GetLabel();
			str_copy(Ed.m_Commands[0].m_aLabel, Label.m_pLabel != nullptr ? Label.m_pLabel : "", sizeof(Ed.m_Commands[0].m_aLabel));
			Ed.m_Commands[0].m_LabelType = (int)Label.m_Type;
			str_copy(Ed.m_Commands[0].m_aCommand, pBehavior->GetCommand(), sizeof(Ed.m_Commands[0].m_aCommand));
		}
		else if(str_comp(pType, CTouchControls::CBindToggleTouchButtonBehavior::BEHAVIOR_TYPE) == 0)
		{
			Ed.m_BehaviorType = 1;
			auto *pBehavior = static_cast<CTouchControls::CBindToggleTouchButtonBehavior *>(pButton->m_pBehavior.get());
			std::vector<CTouchControls::CBindToggleTouchButtonBehavior::CCommand> vCommands = pBehavior->GetCommand();
			TouchResizeCommands(Ed, std::max<size_t>(vCommands.size(), 2));
			for(size_t i = 0; i < vCommands.size(); i++)
			{
				str_copy(Ed.m_Commands[i].m_aLabel, vCommands[i].m_Label.c_str(), sizeof(Ed.m_Commands[i].m_aLabel));
				Ed.m_Commands[i].m_LabelType = (int)vCommands[i].m_LabelType;
				str_copy(Ed.m_Commands[i].m_aCommand, vCommands[i].m_Command.c_str(), sizeof(Ed.m_Commands[i].m_aCommand));
			}
		}
		else if(str_comp(pType, CTouchControls::CPredefinedTouchButtonBehavior::BEHAVIOR_TYPE) == 0)
		{
			Ed.m_BehaviorType = 2;
			auto *pBehavior = static_cast<CTouchControls::CPredefinedTouchButtonBehavior *>(pButton->m_pBehavior.get());
			Ed.m_PredefinedType = TouchPredefinedTypeById(pBehavior->GetPredefinedType());
			if(Ed.m_PredefinedType == TOUCH_PREDEFINED_EXTRA_MENU)
			{
				auto *pExtraMenu = static_cast<CTouchControls::CExtraMenuTouchButtonBehavior *>(pButton->m_pBehavior.get());
				Ed.m_ExtraMenu = pExtraMenu->GetNumber();
			}
		}
	}

	// Mirror of upstream CMenusIngameTouchControls::SaveCachedSettingsToTarget.
	void TouchApplyToTarget(CTouchControls::CTouchButton *pButton)
	{
		STouchEditorState &Ed = TouchEd();
		pButton->m_UnitRect.m_W = std::clamp(Ed.m_W, (int)CTouchControls::BUTTON_SIZE_MINIMUM, (int)CTouchControls::BUTTON_SIZE_MAXIMUM);
		pButton->m_UnitRect.m_H = std::clamp(Ed.m_H, (int)CTouchControls::BUTTON_SIZE_MINIMUM, (int)CTouchControls::BUTTON_SIZE_MAXIMUM);
		pButton->m_UnitRect.m_X = std::clamp(Ed.m_X, 0, (int)CTouchControls::BUTTON_SIZE_SCALE - pButton->m_UnitRect.m_W);
		pButton->m_UnitRect.m_Y = std::clamp(Ed.m_Y, 0, (int)CTouchControls::BUTTON_SIZE_SCALE - pButton->m_UnitRect.m_H);
		pButton->m_vVisibilities.clear();
		for(int Visibility = 0; Visibility < (int)CTouchControls::EButtonVisibility::NUM_VISIBILITIES; Visibility++)
		{
			if(Ed.m_aVisibility[Visibility] != TOUCH_VIS_IGNORE)
				pButton->m_vVisibilities.emplace_back((CTouchControls::EButtonVisibility)Visibility, Ed.m_aVisibility[Visibility] == TOUCH_VIS_INCLUDE);
		}
		pButton->m_Shape = (CTouchControls::EButtonShape)Ed.m_Shape;
		pButton->UpdateScreenFromUnitRect();
		if(Ed.m_BehaviorType == 0)
		{
			pButton->m_pBehavior = std::make_unique<CTouchControls::CBindTouchButtonBehavior>(Ed.m_Commands[0].m_aLabel, (CTouchControls::CButtonLabel::EType)Ed.m_Commands[0].m_LabelType, Ed.m_Commands[0].m_aCommand);
		}
		else if(Ed.m_BehaviorType == 1)
		{
			std::vector<CTouchControls::CBindToggleTouchButtonBehavior::CCommand> vCommands;
			vCommands.reserve(Ed.m_NumCommands);
			for(size_t i = 0; i < Ed.m_NumCommands; i++)
			{
				const STouchCmdState &Command = Ed.m_Commands[i];
				vCommands.emplace_back(Command.m_aLabel, (CTouchControls::CButtonLabel::EType)Command.m_LabelType, Command.m_aCommand);
			}
			pButton->m_pBehavior = std::make_unique<CTouchControls::CBindToggleTouchButtonBehavior>(std::move(vCommands));
		}
		else
		{
			if(Ed.m_PredefinedType == TOUCH_PREDEFINED_EXTRA_MENU)
				pButton->m_pBehavior = std::make_unique<CTouchControls::CExtraMenuTouchButtonBehavior>(Ed.m_ExtraMenu);
			else
				pButton->m_pBehavior = TouchMakePredefined(Ed.m_PredefinedType);
		}
		pButton->UpdatePointers();
	}

	void TouchClampCache()
	{
		STouchEditorState &Ed = TouchEd();
		Ed.m_W = std::clamp(Ed.m_W, (int)CTouchControls::BUTTON_SIZE_MINIMUM, (int)CTouchControls::BUTTON_SIZE_MAXIMUM);
		Ed.m_H = std::clamp(Ed.m_H, (int)CTouchControls::BUTTON_SIZE_MINIMUM, (int)CTouchControls::BUTTON_SIZE_MAXIMUM);
		Ed.m_X = std::clamp(Ed.m_X, 0, (int)CTouchControls::BUTTON_SIZE_SCALE - Ed.m_W);
		Ed.m_Y = std::clamp(Ed.m_Y, 0, (int)CTouchControls::BUTTON_SIZE_SCALE - Ed.m_H);
	}

	void TouchUpdateSample(CTouchControls &TC)
	{
		TC.RemakeSampleButton();
		TouchApplyToTarget(TC.SampleButton());
		TC.SetShownRect(TC.SampleButton()->m_UnitRect);
	}

	void TouchApplyProperty(CTouchControls &TC)
	{
		TouchClampCache();
		if(TC.SelectedButton() != nullptr)
			TC.SetUnsavedChanges(true);
		if(TC.SampleButton() != nullptr)
			TouchUpdateSample(TC);
	}

	void TouchSaveChanges(CTouchControls &TC)
	{
		if(TC.SelectedButton() != nullptr)
		{
			TouchApplyToTarget(TC.SelectedButton());
			TC.SetEditingChanges(true);
		}
		TC.SetUnsavedChanges(false);
		if(TC.SampleButton() != nullptr)
			TouchUpdateSample(TC);
	}

	void TouchDiscardChanges(CTouchControls &TC)
	{
		TouchCacheFromTarget(TC.SelectedButton());
		TC.SetUnsavedChanges(false);
		if(TC.SampleButton() != nullptr)
			TouchUpdateSample(TC);
	}

	void TouchSelectButton(CTouchControls &TC, CTouchControls::CTouchButton *pButton, CMenus &M)
	{
		if(TC.HasUnsavedChanges())
		{
			STouchEditorState &Ed = TouchEd();
			Ed.m_PopupType = TOUCH_POPUP_BUTTON_CHANGED;
			Ed.m_PopupOpen = true;
			Ed.m_pPopupOld = TC.SelectedButton();
			Ed.m_pPopupNew = pButton;
			Ed.m_CloseAfterPopup = false;
			return;
		}
		TC.SetSelectedButton(pButton);
		TouchCacheFromTarget(pButton);
		TC.SetUnsavedChanges(false);
		TouchUpdateSample(TC);
		TouchEd().m_Tab = 0;
		(void)M;
	}

	void TouchButtonLabelOf(CTouchControls::CTouchButton *pButton, char *pBuf, int Size)
	{
		const CTouchControls::CButtonLabel Label = pButton->m_pBehavior != nullptr ? pButton->m_pBehavior->GetLabel() : CTouchControls::CButtonLabel{CTouchControls::CButtonLabel::EType::PLAIN, ""};
		str_copy(pBuf, Label.m_pLabel != nullptr && Label.m_pLabel[0] != '\0' ? Label.m_pLabel : Localize("No label"), Size);
	}

	void TouchButtonSummaryOf(CTouchControls::CTouchButton *pButton, char *pBuf, int Size)
	{
		const char *pType = pButton->m_pBehavior != nullptr ? pButton->m_pBehavior->GetBehaviorType() : "";
		if(str_comp(pType, CTouchControls::CBindTouchButtonBehavior::BEHAVIOR_TYPE) == 0)
			str_copy(pBuf, static_cast<CTouchControls::CBindTouchButtonBehavior *>(pButton->m_pBehavior.get())->GetCommand(), Size);
		else if(str_comp(pType, CTouchControls::CBindToggleTouchButtonBehavior::BEHAVIOR_TYPE) == 0)
		{
			auto *pBehavior = static_cast<CTouchControls::CBindToggleTouchButtonBehavior *>(pButton->m_pBehavior.get());
			str_copy(pBuf, pBehavior->GetCommand()[pBehavior->GetActiveCommandIndex()].m_Command.c_str(), Size);
		}
		else if(str_comp(pType, CTouchControls::CPredefinedTouchButtonBehavior::BEHAVIOR_TYPE) == 0)
		{
			auto *pBehavior = static_cast<CTouchControls::CPredefinedTouchButtonBehavior *>(pButton->m_pBehavior.get());
			const int Type = TouchPredefinedTypeById(pBehavior->GetPredefinedType());
			if(Type == TOUCH_PREDEFINED_EXTRA_MENU)
				str_format(pBuf, Size, "%s %d", TouchPredefinedName(Type), static_cast<CTouchControls::CExtraMenuTouchButtonBehavior *>(pButton->m_pBehavior.get())->GetNumber() + 1);
			else
				str_copy(pBuf, TouchPredefinedName(Type), Size);
		}
		else
			pBuf[0] = '\0';
	}

	void TouchAddWarning(IClient *pClient, const char *pTitle, const char *pMessage)
	{
		SWarning Warning(pTitle, pMessage);
		Warning.m_AutoHide = false;
		pClient->AddWarning(Warning);
	}

	void TouchRenderCmdRows(CXui &Ui, CTouchControls &TC, STouchCmdState &Command, const char *pPrefix, size_t Index, bool CanDelete)
	{
		char aKey[64];
		char aTitle[64];
		str_format(aKey, sizeof(aKey), "%s.cmd%u", pPrefix, (unsigned)Index);
		str_format(aTitle, sizeof(aTitle), "%s %u", Localize("Command"), (unsigned)(Index + 1));
		Ui.SettingsRow(aTitle, "", [&]() {
			SBox B;
			B.m_Gap = 8.0f;
			B.m_Cross = EAlign::CENTER;
			Ui.Row(B, [&]() {
				if(Ui.Field(aKey, Command.m_aCommand, sizeof(Command.m_aCommand), {.m_pIcon = Ph::TERMINAL, .m_W = Px(180.0f), .m_Right = true}).m_Changed)
					TouchApplyProperty(TC);
				char aDeleteKey[64];
				str_format(aDeleteKey, sizeof(aDeleteKey), "%s.del%u", pPrefix, (unsigned)Index);
				if(CanDelete && Ui.IconButton(Ph::TRASH, {.m_Variant = EVariant::PLAIN, .m_pKey = aDeleteKey}).m_Clicked)
				{
					STouchEditorState &Ed = TouchEd();
					if(Ed.m_NumCommands > 2)
					{
						const auto It = Ed.m_Commands.begin() + Index;
						std::move(It + 1, Ed.m_Commands.begin() + Ed.m_NumCommands, It);
						Ed.m_NumCommands--;
					}
					else
						str_copy(Command.m_aCommand, "", sizeof(Command.m_aCommand));
					TouchApplyProperty(TC);
				}
			});
		});
		str_format(aKey, sizeof(aKey), "%s.lbl%u", pPrefix, (unsigned)Index);
		str_format(aTitle, sizeof(aTitle), "%s %u", Localize("Label"), (unsigned)(Index + 1));
		Ui.SettingsRow(aTitle, "", [&]() {
			if(Ui.Field(aKey, Command.m_aLabel, sizeof(Command.m_aLabel), {.m_W = Px(180.0f), .m_Right = true}).m_Changed)
				TouchApplyProperty(TC);
		});
		str_format(aKey, sizeof(aKey), "%s.ltype%u", pPrefix, (unsigned)Index);
		str_format(aTitle, sizeof(aTitle), "%s %u", Localize("Label type"), (unsigned)(Index + 1));
		Ui.SettingsRow(aTitle, "", [&]() {
			std::vector<std::string> vOptions = TouchLabelTypeOptions();
			const int Before = Command.m_LabelType;
			SettingsUi::Dropdown(Ui, aKey, vOptions, &Command.m_LabelType, 170.0f);
			if(SettingsUi::DropdownOptions(Ui, aKey, vOptions, &Command.m_LabelType) && Command.m_LabelType != Before)
				TouchApplyProperty(TC);
		});
	}

	void TouchRenderButtonProps(CXui &Ui, CTouchControls &TC, CMenus &M)
	{
		STouchEditorState &Ed = TouchEd();
		Ui.SectionHeader(Localize("Layout"));
		Ui.Group([&]() {
			auto RectRow = [&](const char *pTitle, const char *pKey, int *pValue, float Min, float Max) {
				Ui.SettingsRow(pTitle, "", [&]() {
					const int Before = *pValue;
					Ui.Slider(pKey, pValue, {.m_Min = Min, .m_Max = Max, .m_Width = 220.0f});
					if(*pValue != Before)
						TouchApplyProperty(TC);
				});
			};
			RectRow("X", "tc.x", &Ed.m_X, 0.0f, (float)CTouchControls::BUTTON_SIZE_SCALE);
			RectRow("Y", "tc.y", &Ed.m_Y, 0.0f, (float)CTouchControls::BUTTON_SIZE_SCALE);
			RectRow(Localize("Width"), "tc.w", &Ed.m_W, (float)CTouchControls::BUTTON_SIZE_MINIMUM, (float)CTouchControls::BUTTON_SIZE_MAXIMUM);
			RectRow(Localize("Height"), "tc.h", &Ed.m_H, (float)CTouchControls::BUTTON_SIZE_MINIMUM, (float)CTouchControls::BUTTON_SIZE_MAXIMUM);
			Ui.SettingsRow(Localize("Shape"), "", [&]() {
				const char *apShapes[] = {Localize("Rectangle", "Touch button shape"), Localize("Circle", "Touch button shape")};
				const int Before = Ed.m_Shape;
				Ui.Segmented("tc.shape", apShapes, 2, &Ed.m_Shape);
				if(Ed.m_Shape != Before)
					TouchApplyProperty(TC);
			});
		});

		Ui.SectionHeader(Localize("Visibility"));
		Ui.Group([&]() {
			for(int Visibility = 0; Visibility < (int)CTouchControls::EButtonVisibility::NUM_VISIBILITIES; Visibility++)
			{
				char aKey[32];
				char aTitle[96];
				str_format(aKey, sizeof(aKey), "tc.vis%d", Visibility);
				TouchVisibilityTitle(aTitle, sizeof(aTitle), Visibility);
				Ui.SettingsRow(aTitle, "", [&]() {
					std::vector<std::string> vOptions = TouchVisibilityOptions();
					const int Before = Ed.m_aVisibility[Visibility];
					SettingsUi::Dropdown(Ui, aKey, vOptions, &Ed.m_aVisibility[Visibility], 170.0f);
					if(SettingsUi::DropdownOptions(Ui, aKey, vOptions, &Ed.m_aVisibility[Visibility]) && Ed.m_aVisibility[Visibility] != Before)
						TouchApplyProperty(TC);
				});
			}
		});

		Ui.SectionHeader(Localize("Behavior"));
		Ui.Group([&]() {
			Ui.SettingsRow(Localize("Type"), "", [&]() {
				std::vector<std::string> vOptions = TouchBehaviorOptions();
				const int Before = Ed.m_BehaviorType;
				SettingsUi::Dropdown(Ui, "tc.beh", vOptions, &Ed.m_BehaviorType, 190.0f);
				if(SettingsUi::DropdownOptions(Ui, "tc.beh", vOptions, &Ed.m_BehaviorType) && Ed.m_BehaviorType != Before)
				{
					if(Ed.m_BehaviorType == 0)
						TouchResizeCommands(Ed, 1);
					else if(Ed.m_BehaviorType == 1 && Ed.m_NumCommands < 2)
						TouchResizeCommands(Ed, 2);
					TouchApplyProperty(TC);
				}
			});
			if(Ed.m_BehaviorType == 0)
			{
				TouchRenderCmdRows(Ui, TC, Ed.m_Commands[0], "tc.b", 0, false);
			}
			else if(Ed.m_BehaviorType == 1)
			{
				for(size_t i = 0; i < Ed.m_NumCommands; i++)
				{
					char aPrefix[24];
					str_format(aPrefix, sizeof(aPrefix), "tc.t%u", (unsigned)i);
					TouchRenderCmdRows(Ui, TC, Ed.m_Commands[i], aPrefix, i, true);
				}
				Ui.SettingsRow(Localize("Commands"), "", [&]() {
					if(Ui.Button(Localize("Add command"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::PLUS, .m_pKey = "tc.addcmd"}).m_Clicked && Ed.m_NumCommands < 16)
					{
						TouchResizeCommands(Ed, Ed.m_NumCommands + 1);
						TouchApplyProperty(TC);
					}
				});
			}
			else
			{
				Ui.SettingsRow(Localize("Action"), "", [&]() {
					std::vector<std::string> vOptions;
					for(int i = 0; i < 11; i++)
						vOptions.emplace_back(TouchPredefinedName(i));
					const int Before = Ed.m_PredefinedType;
					SettingsUi::Dropdown(Ui, "tc.pred", vOptions, &Ed.m_PredefinedType, 230.0f);
					if(SettingsUi::DropdownOptions(Ui, "tc.pred", vOptions, &Ed.m_PredefinedType) && Ed.m_PredefinedType != Before)
						TouchApplyProperty(TC);
				});
				if(Ed.m_PredefinedType == TOUCH_PREDEFINED_EXTRA_MENU)
				{
					Ui.SettingsRow(Localize("Menu number"), "", [&]() {
						std::vector<std::string> vOptions;
						char aName[32];
						for(int i = 1; i <= CTouchControls::MAX_EXTRA_MENU_NUMBER; i++)
						{
							str_format(aName, sizeof(aName), "%d", i);
							vOptions.emplace_back(aName);
						}
						const int Before = Ed.m_ExtraMenu;
						SettingsUi::Dropdown(Ui, "tc.emenu", vOptions, &Ed.m_ExtraMenu, 160.0f);
						if(SettingsUi::DropdownOptions(Ui, "tc.emenu", vOptions, &Ed.m_ExtraMenu) && Ed.m_ExtraMenu != Before)
							TouchApplyProperty(TC);
					});
				}
				SBox HelpBox;
				HelpBox.m_W = Grow();
				HelpBox.m_Padding = SEdges(6.0f, 18.0f);
				STextStyle Help;
				Help.m_Size = 12.5f;
				Help.m_Color = XuiTheme::Color::TEXT2;
				Help.m_Wrap = true;
				Ui.Text(TouchPredefinedHelp(Ed.m_PredefinedType), Help, HelpBox);
			}
		});

		Ui.SectionHeader(Localize("Actions"));
		Ui.Group([&]() {
			Ui.SettingsRow(Localize("Apply changes"), Localize("Commit the position and settings to the selected button"), [&]() {
				if(Ui.Button(Localize("Apply"), {.m_Variant = EVariant::ACCENT, .m_pKey = "tc.apply"}).m_Clicked)
				{
					TouchSaveChanges(TC);
					Ui.ShowToast(Localize("Changes applied"), Ph::CHECK);
				}
			});
			Ui.SettingsRow(Localize("Discard changes"), "", [&]() {
				if(Ui.Button(Localize("Discard"), {.m_Variant = EVariant::GHOST, .m_pKey = "tc.revert"}).m_Clicked)
				{
					TouchDiscardChanges(TC);
					Ui.ShowToast(Localize("Changes discarded"), Ph::ARROW_COUNTER_CLOCKWISE);
				}
			});
			Ui.SettingsRow(Localize("Delete button"), "", [&]() {
				if(Ui.Button(Localize("Delete"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::TRASH, .m_pKey = "tc.del"}).m_Clicked)
				{
					TC.DeleteSelectedButton();
					TouchCacheFromTarget(nullptr);
					TC.SetEditingChanges(true);
					Ui.ShowToast(Localize("Button deleted"), Ph::TRASH);
				}
			});
			Ui.SettingsRow(Localize("Deselect"), "", [&]() {
				if(Ui.Button(Localize("Deselect"), {.m_Variant = EVariant::GHOST, .m_pKey = "tc.desel"}).m_Clicked)
				{
					if(TC.HasUnsavedChanges())
					{
						STouchEditorState &Ed = TouchEd();
						Ed.m_PopupType = TOUCH_POPUP_BUTTON_CHANGED;
						Ed.m_PopupOpen = true;
						Ed.m_pPopupOld = TC.SelectedButton();
						Ed.m_pPopupNew = nullptr;
						Ed.m_CloseAfterPopup = false;
					}
					else
					{
						TC.ResetButtonPointers();
						TouchCacheFromTarget(nullptr);
					}
				}
			});
		});
		(void)M;
	}

	void TouchRenderButtonList(CXui &Ui, CTouchControls &TC, CMenus &M)
	{
		Ui.SectionHeader(Localize("Buttons"));
		Ui.Group([&]() {
			Ui.SettingsRow(Localize("Add a new button"), Localize("Creates a custom command button at a free spot"), [&]() {
				if(Ui.Button(Localize("Add"), {.m_Variant = EVariant::ACCENT, .m_pIcon = Ph::PLUS, .m_pKey = "tc.new"}).m_Clicked)
				{
					const CTouchControls::CUnitRect Rect{0, 0, CTouchControls::BUTTON_SIZE_MINIMUM, CTouchControls::BUTTON_SIZE_MINIMUM};
					const std::optional<CTouchControls::CUnitRect> Free = TC.UpdatePosition(Rect, CTouchControls::EButtonShape::RECT, true);
					if(!Free.has_value())
						Ui.ShowToast(Localize("No space for button"), Ph::WARNING);
					else
					{
						CTouchControls::CTouchButton *pNewButton = TC.NewButton();
						TC.SetSelectedButton(pNewButton);
						TouchCacheFromTarget(nullptr);
						STouchEditorState &Ed = TouchEd();
						Ed.m_X = Free->m_X;
						Ed.m_Y = Free->m_Y;
						Ed.m_W = Free->m_W;
						Ed.m_H = Free->m_H;
						TouchApplyToTarget(TC.SelectedButton());
						TC.SetEditingChanges(true);
						TC.SetUnsavedChanges(false);
						TouchUpdateSample(TC);
						Ed.m_Tab = 0;
					}
				}
			});
			Ui.SettingsRow(Localize("Preview all buttons"), "", [&]() {
				int Preview = TC.PreviewAllButtons() ? 1 : 0;
				if(Ui.Switch("tc.preview", &Preview).m_Changed)
					TC.SetPreviewAllButtons(Preview != 0);
			});
		});

		Ui.SectionHeader(Localize("All buttons"));
		Ui.Group([&]() {
			const std::vector<CTouchControls::CTouchButton *> vpButtons = TC.GetButtonsEditor();
			int Index = 0;
			for(CTouchControls::CTouchButton *pButton : vpButtons)
			{
				char aKey[32];
				str_format(aKey, sizeof(aKey), "tc.b%d", Index);
				Index++;
				char aLabel[160];
				char aSummary[256];
				TouchButtonLabelOf(pButton, aLabel, sizeof(aLabel));
				TouchButtonSummaryOf(pButton, aSummary, sizeof(aSummary));
				Ui.SettingsRow(aLabel, aSummary, [&]() {
					if(Ui.Button(Localize("Select"), {.m_Variant = TC.SelectedButton() == pButton ? EVariant::ACCENT : EVariant::GHOST, .m_pKey = aKey}).m_Clicked)
						TouchSelectButton(TC, pButton, M);
				});
			}
		});
	}

	void TouchRenderFileMenu(CXui &Ui, CTouchControls &TC, CMenus &M)
	{
		CGameClient *pGame = Ui.Game();
		STouchEditorState &Ed = TouchEd();

		Ui.SectionHeader(Localize("Configuration"));
		Ui.Group([&]() {
			Ui.SettingsRow(Localize("Save changes"), Localize("Writes the current controls to touch_controls.json"), [&]() {
				if(Ui.Button(Localize("Save"), {.m_Variant = EVariant::ACCENT, .m_pIcon = Ph::CHECK, .m_pKey = "tc.save"}).m_Clicked)
				{
					if(TC.SaveConfigurationToFile())
					{
						TC.SetEditingChanges(false);
						Ui.ShowToast(Localize("Touch controls saved"), Ph::CHECK);
					}
					else
						TouchAddWarning(pGame->Client(), Localize("Error saving touch controls"), Localize("Could not save touch controls to file. See local console for details."));
				}
			});
			Ui.SettingsRow(Localize("Discard changes"), Localize("Reloads the last saved configuration"), [&]() {
				if(Ui.Button(Localize("Discard"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::ARROW_COUNTER_CLOCKWISE, .m_pKey = "tc.discard"}).m_Clicked)
				{
					Ed.m_PopupType = TOUCH_POPUP_DISCARD;
					Ed.m_PopupOpen = true;
				}
			});
			Ui.SettingsRow(Localize("Reset to defaults"), Localize("Restores the touch controls that ship with the game"), [&]() {
				if(Ui.Button(Localize("Reset"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::ARROW_CLOCKWISE, .m_pKey = "tc.reset"}).m_Clicked)
				{
					Ed.m_PopupType = TOUCH_POPUP_RESET;
					Ed.m_PopupOpen = true;
				}
			});
			Ui.SettingsRow(Localize("Import from clipboard"), Localize("Replaces the current controls with the clipboard content"), [&]() {
				if(Ui.Button(Localize("Import"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::CLIPBOARD, .m_pKey = "tc.import"}).m_Clicked)
				{
					Ed.m_PopupType = TOUCH_POPUP_IMPORT;
					Ed.m_PopupOpen = true;
				}
			});
			Ui.SettingsRow(Localize("Export to clipboard"), Localize("Copies the current controls to the clipboard to share them"), [&]() {
				if(Ui.Button(Localize("Export"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::EXPORT, .m_pKey = "tc.export"}).m_Clicked)
				{
					TC.SaveConfigurationToClipboard();
					Ui.ShowToast(Localize("Copied to clipboard"), Ph::CLIPBOARD);
				}
			});
		});

		Ui.SectionHeader(Localize("Direct touch"));
		Ui.Group([&]() {
			Ui.SettingsRow(Localize("Direct touch in game"), Localize("What a finger does on the game area while playing"), [&]() {
				std::vector<std::string> vOptions = TouchDirectTouchIngameOptions();
				int Value = (int)TC.DirectTouchIngame();
				SettingsUi::Dropdown(Ui, "tc.dtin", vOptions, &Value, 190.0f);
				if(SettingsUi::DropdownOptions(Ui, "tc.dtin", vOptions, &Value) && Value != (int)TC.DirectTouchIngame())
					TC.SetDirectTouchIngame((CTouchControls::EDirectTouchIngameMode)Value);
			});
			Ui.SettingsRow(Localize("Direct touch while spectating"), Localize("What a finger does on the game area while spectating"), [&]() {
				std::vector<std::string> vOptions = TouchDirectTouchSpectateOptions();
				int Value = (int)TC.DirectTouchSpectate();
				SettingsUi::Dropdown(Ui, "tc.dtspec", vOptions, &Value, 190.0f);
				if(SettingsUi::DropdownOptions(Ui, "tc.dtspec", vOptions, &Value) && Value != (int)TC.DirectTouchSpectate())
					TC.SetDirectTouchSpectate((CTouchControls::EDirectTouchSpectateMode)Value);
			});
		});

		Ui.SectionHeader(Localize("Background"));
		Ui.Group([&]() {
			Ui.SettingsRow(Localize("Inactive color"), Localize("Background of buttons while not pressed"), [&]() {
				if(Ui.ColorPickerButton("tc.cin", &Ed.m_HslInactive).m_Changed)
				{
					const ColorRGBA Picked = color_cast<ColorRGBA>(ColorHSLA(Ed.m_HslInactive));
					TC.SetBackgroundColorInactive(ColorRGBA(Picked.r, Picked.g, Picked.b, TC.BackgroundColorInactive().a));
					TC.SetEditingChanges(true);
				}
			});
			Ui.SettingsRow(Localize("Active color"), Localize("Background of buttons while pressed"), [&]() {
				if(Ui.ColorPickerButton("tc.cact", &Ed.m_HslActive).m_Changed)
				{
					const ColorRGBA Picked = color_cast<ColorRGBA>(ColorHSLA(Ed.m_HslActive));
					TC.SetBackgroundColorActive(ColorRGBA(Picked.r, Picked.g, Picked.b, TC.BackgroundColorActive().a));
					TC.SetEditingChanges(true);
				}
			});
		});

		Ui.SectionHeader(Localize("How to edit"));
		SBox HelpBox;
		HelpBox.m_W = Grow();
		HelpBox.m_Padding = SEdges(6.0f, 18.0f);
		STextStyle Help;
		Help.m_Size = 12.5f;
		Help.m_Color = XuiTheme::Color::TEXT2;
		Help.m_Wrap = true;
		Ui.Text(Localize("While the menu is closed: drag a button to move it, pinch with two fingers to resize it, long-press a button to select it and long-press an empty spot to deselect. Tap Done to leave the editor and remember to save your changes."), Help, HelpBox);
		(void)M;
	}
}

void CXui::RenderTouchControlsEditor(CMenus &M)
{
	CGameClient *pGame = GameClient();
	CTouchControls &TC = pGame->m_TouchControls;
	STouchEditorState &Ed = TouchEd();

	if(!TC.IsEditingActive())
	{
		Ed.m_PopupOpen = false;
		Ed.m_PopupType = TOUCH_POPUP_NONE;
		return;
	}

	// Resolve issues queued by the gesture layer (long-press select, drag release).
	const std::array<CTouchControls::CIssueParam, (unsigned)CTouchControls::EIssueType::NUM_ISSUES> aIssues = TC.Issues();
	for(unsigned Issue = 0; Issue < (unsigned)CTouchControls::EIssueType::NUM_ISSUES; Issue++)
	{
		if(aIssues[Issue].m_Resolved)
			continue;
		switch((CTouchControls::EIssueType)Issue)
		{
		case CTouchControls::EIssueType::CACHE_SETTINGS:
			TouchCacheFromTarget(aIssues[Issue].m_pTargetButton);
			break;
		case CTouchControls::EIssueType::CACHE_POSITION:
			if(aIssues[Issue].m_pTargetButton != nullptr)
			{
				Ed.m_X = aIssues[Issue].m_pTargetButton->m_UnitRect.m_X;
				Ed.m_Y = aIssues[Issue].m_pTargetButton->m_UnitRect.m_Y;
				Ed.m_W = aIssues[Issue].m_pTargetButton->m_UnitRect.m_W;
				Ed.m_H = aIssues[Issue].m_pTargetButton->m_UnitRect.m_H;
			}
			break;
		default:
			break;
		}
	}

	// Popups requested by the gesture layer while the menu was closed.
	const CTouchControls::CPopupParam Popup = TC.RequiredPopup();
	if(Popup.m_PopupType != CTouchControls::EPopupType::NUM_POPUPS)
	{
		Ed.m_PopupType = (int)Popup.m_PopupType;
		Ed.m_PopupOpen = true;
		Ed.m_pPopupOld = Popup.m_pOldSelectedButton;
		Ed.m_pPopupNew = Popup.m_pNewSelectedButton;
		Ed.m_CloseAfterPopup = Popup.m_PopupType == CTouchControls::EPopupType::BUTTON_INVISIBLE ? false : !Popup.m_KeepMenuOpen;
	}

	// Keep the color pickers in sync with the live configuration; the pickers
	// write through on every change, so this is lossless.
	Ed.m_HslInactive = color_cast<ColorHSLA>(TC.BackgroundColorInactive()).Pack(true);
	Ed.m_HslActive = color_cast<ColorHSLA>(TC.BackgroundColorActive()).Pack(true);

	auto Close = [&]() { M.SetActive(false); };
	auto PopupClose = [&]() {
		Ed.m_PopupOpen = false;
		Ed.m_PopupType = TOUCH_POPUP_NONE;
	};
	auto PopupSwitch = [&](bool Save) {
		if(Save && Ed.m_pPopupOld != nullptr)
		{
			TouchApplyToTarget(Ed.m_pPopupOld);
			TC.SetEditingChanges(true);
		}
		TC.SetSelectedButton(Ed.m_pPopupNew);
		TC.SetUnsavedChanges(false);
		if(Ed.m_pPopupNew != nullptr)
		{
			TouchCacheFromTarget(TC.SelectedButton());
			TouchUpdateSample(TC);
		}
		else
		{
			TouchCacheFromTarget(nullptr);
			TC.ResetButtonPointers();
		}
		PopupClose();
		if(Ed.m_CloseAfterPopup)
			Close();
		Ed.m_CloseAfterPopup = false;
	};
	auto TurnOffSave = [&]() {
		TouchSaveChanges(TC);
		TC.SetEditingChanges(true);
		if(TC.SaveConfigurationToFile())
			TC.SetEditingChanges(false);
		else
			TouchAddWarning(pGame->Client(), Localize("Error saving touch controls"), Localize("Could not save touch controls to file. See local console for details."));
		TC.SetEditingActive(false);
		TC.ResetButtonPointers();
		PopupClose();
		Close();
	};
	auto TurnOffExit = [&]() {
		TC.SetEditingActive(false);
		TC.ResetButtonPointers();
		PopupClose();
		Close();
	};
	auto FileDiscard = [&]() {
		if(TC.LoadConfigurationFromFile(IStorage::TYPE_ALL))
		{
			TC.SetEditingChanges(false);
			TC.ResetButtonPointers();
			TouchCacheFromTarget(nullptr);
		}
		else
			TouchAddWarning(pGame->Client(), Localize("Error loading touch controls"), Localize("Could not load touch controls from file. See local console for details."));
		PopupClose();
	};
	auto FileReset = [&]() {
		bool Success = false;
		for(int StorageType = IStorage::TYPE_SAVE + 1; StorageType < Storage()->NumPaths(); StorageType++)
		{
			if(TC.LoadConfigurationFromFile(StorageType))
			{
				Success = true;
				break;
			}
		}
		if(Success)
		{
			TC.SetEditingChanges(true);
			TC.ResetButtonPointers();
			TouchCacheFromTarget(nullptr);
		}
		else
			TouchAddWarning(pGame->Client(), Localize("Error loading touch controls"), Localize("Could not load default touch controls from file. See local console for details."));
		PopupClose();
	};
	auto FileImport = [&]() {
		if(TC.LoadConfigurationFromClipboard())
		{
			TC.SetEditingChanges(true);
			TC.ResetButtonPointers();
			TouchCacheFromTarget(nullptr);
		}
		else
			TouchAddWarning(pGame->Client(), Localize("Error loading touch controls"), Localize("Could not load touch controls from clipboard. See local console for details."));
		PopupClose();
	};

	SBox Scrim;
	Scrim.m_Position = EPos::ABSOLUTE;
	Scrim.m_Left = Scrim.m_Top = Scrim.m_Right = Scrim.m_Bottom = 0.0f;
	Scrim.m_Hit = EHit::BLOCK;
	const uint32_t ScrimId = MakeId("tc.scrim");
	const float Dim = Animate(ScrimId, 1.0f, XuiTheme::Motion::POP, 0, 0.0f);
	Paint(BoxId(ScrimId, EDir::ROW, Scrim), [Dim](CXui &U, const SRect &Rc) {
		U.DrawRect(Rc, SolidRect(ColorRGBA(0, 0, 0, 0.55f * Dim), 0.0f));
	});

	SBox Page;
	Page.m_Position = EPos::ABSOLUTE;
	Page.m_Left = 36.0f;
	Page.m_Right = 36.0f;
	Page.m_Top = 28.0f;
	Page.m_Bottom = 28.0f;
	Page.m_Gap = 12.0f;
	Page.m_Hit = EHit::BLOCK;
	Column(Page, [&]() {
		SBox Head;
		Head.m_W = Grow();
		Head.m_Gap = 12.0f;
		Head.m_Cross = EAlign::CENTER;
		Row(Head, [&]() {
			Icon(Ph::HAND_POINTING, 22.0f, Accent());
			STextStyle Title;
			Title.m_Size = 21.0f;
			Title.m_Font = EFont::EXTRABOLD;
			Text(Localize("Touch controls editor"), Title);
			Spacer();
			STextStyle Status;
			Status.m_Size = 12.5f;
			Status.m_Font = EFont::SEMIBOLD;
			Status.m_Color = TC.HasEditingChanges() ? XuiTheme::Color::RED_SOFT : XuiTheme::Color::GREEN;
			Text(TC.HasEditingChanges() ? Localize("Unsaved changes") : Localize("All changes saved"), Status);
			if(Button(Localize("Done"), {.m_Variant = EVariant::ACCENT, .m_pIcon = Ph::CHECK, .m_pKey = "tc.done"}).m_Clicked)
			{
				if(TC.HasEditingChanges())
				{
					Ed.m_PopupType = TOUCH_POPUP_TURN_OFF;
					Ed.m_PopupOpen = true;
				}
				else
				{
					TC.SetEditingActive(false);
					TC.ResetButtonPointers();
					Close();
				}
			}
		});

		SBox Body;
		Body.m_W = Grow();
		Body.m_H = Grow();
		if(TC.IsButtonEditing())
		{
			Column(Body, [&]() {
				const char *apTabs[] = {Localize("Button"), Localize("All buttons")};
				Tabs("tc.tab", apTabs, 2, &Ed.m_Tab);
				SBox Content;
				Content.m_W = Grow();
				Content.m_H = Grow();
				Scroll(Content, [&]() {
					if(Ed.m_Tab == 0)
						TouchRenderButtonProps(*this, TC, M);
					else
						TouchRenderButtonList(*this, TC, M);
				});
			});
		}
		else
		{
			Scroll(Body, [&]() {
				TouchRenderFileMenu(*this, TC, M);
			});
		}
	});

	Modal("tc.modal", &Ed.m_PopupOpen, [&]() {
		SBox Box;
		Box.m_W = Px(std::min(470.0f, ScreenSize().x - 60.0f));
		Box.m_Gap = 14.0f;
		Column(Box, [&]() {
			STextStyle Title;
			Title.m_Size = 17.0f;
			Title.m_Font = EFont::BOLD;
			STextStyle BodyText;
			BodyText.m_Size = 13.5f;
			BodyText.m_Color = XuiTheme::Color::TEXT2;
			BodyText.m_Wrap = true;
			SBox BodyBox;
			BodyBox.m_W = Grow();
			const char *pTitle = "";
			const char *pBody = "";
			switch(Ed.m_PopupType)
			{
			case TOUCH_POPUP_BUTTON_CHANGED:
				pTitle = Localize("Unsaved changes");
				pBody = Localize("Save all changes before switching selected button?");
				break;
			case TOUCH_POPUP_NO_SPACE:
				pTitle = Localize("No space for button");
				pBody = Localize("There is not enough space available for the button. Check its visibilities and size.");
				break;
			case TOUCH_POPUP_BUTTON_INVISIBLE:
				pTitle = Localize("Selected button not visible");
				pBody = Localize("The selected button is not visible. Do you want to deselect it or edit its visibility?");
				break;
			case TOUCH_POPUP_TURN_OFF:
				pTitle = Localize("Unsaved changes");
				pBody = Localize("Save the touch controls before turning off the editor?");
				break;
			case TOUCH_POPUP_DISCARD:
				pTitle = Localize("Discard changes");
				pBody = Localize("Are you sure that you want to discard the current changes to the touch controls?");
				break;
			case TOUCH_POPUP_RESET:
				pTitle = Localize("Reset to defaults");
				pBody = Localize("Are you sure that you want to reset the touch controls to default?");
				break;
			case TOUCH_POPUP_IMPORT:
				pTitle = Localize("Import from clipboard");
				pBody = Localize("Are you sure that you want to import the touch controls from the clipboard? This will overwrite your current touch controls.");
				break;
			default: break;
			}
			Text(pTitle, Title);
			Text(pBody, BodyText, BodyBox);
			SBox Btns;
			Btns.m_W = Grow();
			Btns.m_Gap = 10.0f;
			Row(Btns, [&]() {
				switch(Ed.m_PopupType)
				{
				case TOUCH_POPUP_BUTTON_CHANGED:
					if(Button(Localize("Save"), {.m_Variant = EVariant::ACCENT, .m_W = Grow(), .m_pKey = "tc.m.save"}).m_Clicked)
						PopupSwitch(true);
					if(Button(Localize("Discard"), {.m_Variant = EVariant::GHOST, .m_W = Grow(), .m_pKey = "tc.m.disc"}).m_Clicked)
						PopupSwitch(false);
					break;
				case TOUCH_POPUP_NO_SPACE:
					if(Button(Localize("Ok"), {.m_Variant = EVariant::ACCENT, .m_W = Grow(), .m_pKey = "tc.m.ok"}).m_Clicked)
						PopupClose();
					break;
				case TOUCH_POPUP_BUTTON_INVISIBLE:
					if(Button(Localize("Deselect"), {.m_Variant = EVariant::ACCENT, .m_W = Grow(), .m_pKey = "tc.m.desel"}).m_Clicked)
					{
						TC.ResetButtonPointers();
						TouchCacheFromTarget(nullptr);
						PopupClose();
					}
					if(Button(Localize("Edit"), {.m_Variant = EVariant::GHOST, .m_W = Grow(), .m_pKey = "tc.m.edit"}).m_Clicked)
						PopupClose();
					break;
				case TOUCH_POPUP_TURN_OFF:
					if(Button(Localize("Save and exit"), {.m_Variant = EVariant::ACCENT, .m_W = Grow(), .m_pKey = "tc.m.svex"}).m_Clicked)
						TurnOffSave();
					if(Button(Localize("Exit without saving"), {.m_Variant = EVariant::GHOST, .m_W = Grow(), .m_pKey = "tc.m.ex"}).m_Clicked)
						TurnOffExit();
					if(Button(Localize("Cancel"), {.m_Variant = EVariant::GHOST, .m_W = Grow(), .m_pKey = "tc.m.cancel"}).m_Clicked)
						PopupClose();
					break;
				case TOUCH_POPUP_DISCARD:
					if(Button(Localize("Yes"), {.m_Variant = EVariant::ACCENT, .m_W = Grow(), .m_pKey = "tc.m.yes"}).m_Clicked)
						FileDiscard();
					if(Button(Localize("No"), {.m_Variant = EVariant::GHOST, .m_W = Grow(), .m_pKey = "tc.m.no"}).m_Clicked)
						PopupClose();
					break;
				case TOUCH_POPUP_RESET:
					if(Button(Localize("Yes"), {.m_Variant = EVariant::ACCENT, .m_W = Grow(), .m_pKey = "tc.m.yes"}).m_Clicked)
						FileReset();
					if(Button(Localize("No"), {.m_Variant = EVariant::GHOST, .m_W = Grow(), .m_pKey = "tc.m.no"}).m_Clicked)
						PopupClose();
					break;
				case TOUCH_POPUP_IMPORT:
					if(Button(Localize("Yes"), {.m_Variant = EVariant::ACCENT, .m_W = Grow(), .m_pKey = "tc.m.yes"}).m_Clicked)
						FileImport();
					if(Button(Localize("No"), {.m_Variant = EVariant::GHOST, .m_W = Grow(), .m_pKey = "tc.m.no"}).m_Clicked)
						PopupClose();
					break;
				default:
					break;
				}
			});
		});
	});

	RenderShellPopup(M);
	RenderToasts();
	if(EscPressed() && M.m_Popup == CMenus::POPUP_NONE)
		Close();
}
