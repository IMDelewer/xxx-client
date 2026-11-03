#include "settings_internal.h"

#include <base/str.h>

#include <engine/client.h>
#include <engine/font_icons_phosphor.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/sound.h>

#include <game/client/components/background.h>
#include <game/client/components/binds.h>
#include <game/client/components/camera.h>
#include <game/client/components/mapimages.h>
#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <algorithm>
#include <cmath>
#include <numeric>

using namespace Xui;

namespace SettingsUi
{
	namespace
	{
		struct SBindDef
		{
			const char *m_pHeader;
			const char *m_pLabel;
			const char *m_pCommand;
		};

		const SBindDef gs_aBinds[] = {
			{"Movement", "Move left", "+left"},
			{nullptr, "Move right", "+right"},
			{nullptr, "Jump", "+jump"},
			{nullptr, "Fire", "+fire"},
			{nullptr, "Hook", "+hook"},
			{nullptr, "Pause", "say /pause"},
			{nullptr, "Kill", "kill"},
			{nullptr, "Zoom in", "zoom+"},
			{nullptr, "Zoom out", "zoom-"},
			{nullptr, "Default zoom", "zoom"},
			{nullptr, "Show others", "say /showothers"},
			{nullptr, "Show all", "say /showall"},
			{nullptr, "Toggle dyncam", "toggle cl_dyncam 0 1"},
			{nullptr, "Toggle ghost", "toggle cl_race_show_ghost 0 1"},
			{"Weapon", "Hammer", "+weapon1"},
			{nullptr, "Pistol", "+weapon2"},
			{nullptr, "Shotgun", "+weapon3"},
			{nullptr, "Grenade", "+weapon4"},
			{nullptr, "Laser", "+weapon5"},
			{nullptr, "Next weapon", "+nextweapon"},
			{nullptr, "Prev. weapon", "+prevweapon"},
			{"Voting", "Vote yes", "vote yes"},
			{nullptr, "Vote no", "vote no"},
			{"Chat", "Chat", "+show_chat; chat all"},
			{nullptr, "Team chat", "+show_chat; chat team"},
			{nullptr, "Converse", "+show_chat; chat all /c "},
			{nullptr, "Chat command", "+show_chat; chat all /"},
			{nullptr, "Show chat", "+show_chat"},
			{"Dummy", "Toggle dummy", "toggle cl_dummy 0 1"},
			{nullptr, "Dummy copy", "toggle cl_dummy_copy_moves 0 1"},
			{nullptr, "Hammerfly dummy", "toggle cl_dummy_hammer 0 1"},
			{"Practice", "Teleport to cursor", "tp_to_cursor"},
			{nullptr, "Teleport dummy to cursor", "tp_dummy_to_cursor"},
			{"Miscellaneous", "Emoticon", "+emote"},
			{nullptr, "Spectator mode", "+spectate"},
			{nullptr, "Spectate next", "spectate_next"},
			{nullptr, "Spectate previous", "spectate_previous"},
			{nullptr, "Console", "toggle_local_console"},
			{nullptr, "Remote console", "toggle_remote_console"},
			{nullptr, "Screenshot", "screenshot"},
			{nullptr, "Scoreboard", "+scoreboard"},
			{nullptr, "Scoreboard cursor", "toggle_scoreboard_cursor"},
			{nullptr, "Statboard", "+statboard"},
			{nullptr, "Lock team", "say /lock"},
			{nullptr, "Show entities", "toggle cl_overlay_entities 0 100"},
			{nullptr, "Show HUD", "toggle cl_showhud 0 1"},
		};

		bool IsPredefinedCommand(const char *pCommand)
		{
			return std::any_of(std::begin(gs_aBinds), std::end(gs_aBinds), [pCommand](const SBindDef &Def) { return str_comp(Def.m_pCommand, pCommand) == 0; });
		}

		std::vector<std::string> JoystickNames()
		{
			std::vector<std::string> vNames;
			for(size_t i = 0; i < Game()->Input()->NumJoysticks(); i++)
			{
				char aName[256];
				str_format(aName, sizeof(aName), "%s %d: %s", Localize("Controller"), (int)i, Game()->Input()->GetJoystick(i)->GetName());
				vNames.push_back(aName);
			}
			return vNames;
		}

		std::vector<std::string> AxisNames()
		{
			std::vector<std::string> vNames;
			IInput::IJoystick *pJoystick = Game()->Input()->GetActiveJoystick();
			const int Num = pJoystick != nullptr ? pJoystick->GetNumAxes() : 0;
			for(int i = 0; i < Num; i++)
			{
				char aName[32];
				str_format(aName, sizeof(aName), "%s %d", Localize("Axis"), i + 1);
				vNames.push_back(aName);
			}
			return vNames;
		}

		bool HasJoystick()
		{
			return g_Config.m_InpControllerEnable && Game()->Input()->NumJoysticks() > 0;
		}

		void CustomBinds()
		{
			CXui &U = Ui();
			std::vector<std::pair<int, std::string>> vCustom;
			for(int Key = KEY_A; Key < KEY_LAST; Key++)
			{
				const char *pBind = Game()->m_Binds.Get(CBindSlot(Key, KeyModifier::NONE));
				if(pBind != nullptr && pBind[0] != '\0' && !IsPredefinedCommand(pBind))
					vCustom.emplace_back(Key, pBind);
			}
			if(vCustom.empty())
				return;
			U.SectionHeader(Localize("Custom binds"));
			U.Group([&]() {
				for(auto &[Key, Command] : vCustom)
				{
					char aKey[48];
					str_format(aKey, sizeof(aKey), "custom.%d", Key);
					U.SettingsRow(Command.c_str(), "", [&]() {
						SBox B;
						B.m_Gap = 8.0f;
						B.m_Cross = EAlign::CENTER;
						U.Row(B, [&]() {
							int Current = Key;
							if(U.Kbd(aKey, &Current).m_Changed)
							{
								Game()->m_Binds.Bind(Key, "");
								if(Current != KEY_UNKNOWN)
									Game()->m_Binds.Bind(Current, Command.c_str());
							}
							char aDel[48];
							str_format(aDel, sizeof(aDel), "%s.del", aKey);
							if(U.IconButton(Ph::TRASH, {.m_Variant = EVariant::PLAIN, .m_pKey = aDel}).m_Clicked)
								Game()->m_Binds.Bind(Key, "");
						});
					});
				}
			});
		}
	}



	void BuildControls(SSection &S)
	{
		S.Header("Mouse");
		S.Slider("Ingame mouse sens.", &g_Config.m_InpMousesens, 1, 500);
		S.Slider("UI mouse sens.", &g_Config.m_UiMousesens, 1, 500);

		S.Header("Controller");
		S.Toggle("Enable controller", &g_Config.m_InpControllerEnable);
		S.Custom("Controller", []() {
			 STextStyle Style;
			 Style.m_Color = XuiTheme::Color::TEXT3;
			 Ui().Text(Localize("No controller found. Plug in a controller."), Style);
		 }).Show([]() { return g_Config.m_InpControllerEnable != 0 && Game()->Input()->NumJoysticks() == 0; });
		S.VirtualDropdown(
			 "Controller device", []() { return (int)Game()->Input()->GetActiveJoystick()->GetIndex(); }, [](int V) { Game()->Input()->SetActiveJoystick(V); }, []() { return JoystickNames(); })
			.Show([]() { return HasJoystick(); });
		S.Segment("Ingame controller mode", &g_Config.m_InpControllerAbsolute, {"Relative", "Absolute"}).Show([]() { return HasJoystick(); });
		S.Slider("Ingame controller sens.", &g_Config.m_InpControllerSens, 1, 500).Show([]() { return HasJoystick() && !g_Config.m_InpControllerAbsolute; });
		S.Slider("UI controller sens.", &g_Config.m_UiControllerSens, 1, 500).Show([]() { return HasJoystick(); });
		S.Slider("Controller jitter tolerance", &g_Config.m_InpControllerTolerance, 0, 50).Show([]() { return HasJoystick(); });
		S.VirtualDropdown(
			 "Aim X axis", []() { return g_Config.m_InpControllerX; }, [](int V) {
			 if(g_Config.m_InpControllerY == V)
				 g_Config.m_InpControllerY = g_Config.m_InpControllerX;
			 g_Config.m_InpControllerX = V; },
			 []() { return AxisNames(); })
			.Show([]() { return HasJoystick(); });
		S.VirtualDropdown(
			 "Aim Y axis", []() { return g_Config.m_InpControllerY; }, [](int V) {
			 if(g_Config.m_InpControllerX == V)
				 g_Config.m_InpControllerX = g_Config.m_InpControllerY;
			 g_Config.m_InpControllerY = V; },
			 []() { return AxisNames(); })
			.Show([]() { return HasJoystick(); });

		const char *pHeader = nullptr;
		for(const SBindDef &Def : gs_aBinds)
		{
			if(Def.m_pHeader != nullptr)
			{
				pHeader = Def.m_pHeader;
				S.Header(pHeader);
			}
			S.Key(Def.m_pLabel, Def.m_pCommand);
		}
		S.Block("Custom binds", []() { CustomBinds(); });

		S.Header("Reset");
		S.Action("Reset controls", "Reset to defaults", []() {
			 Game()->m_Binds.SetDefaults();
			 g_Config.m_InpMousesens = 200;
			 g_Config.m_UiMousesens = 200;
			 g_Config.m_InpControllerEnable = 0;
			 g_Config.m_InpControllerGUID[0] = '\0';
			 g_Config.m_InpControllerAbsolute = 0;
			 g_Config.m_InpControllerSens = 100;
			 g_Config.m_InpControllerX = 0;
			 g_Config.m_InpControllerY = 1;
			 g_Config.m_InpControllerTolerance = 5;
			 g_Config.m_UiControllerSens = 100;
		 }).Desc("Restore every key bind, mouse and controller setting");
	}

	namespace
	{
		CVideoMode gs_aModes[256];
		int gs_NumModes = -1;
		bool gs_ModesReload = false;
		int gs_InitFsaa = -1;
		bool gs_GpuChanged = false;
		int gs_InitGpu = -2;

		void EnsureModes()
		{
			static bool s_Listener = false;
			if(!s_Listener)
			{
				s_Listener = true;
				Game()->Graphics()->AddWindowPropChangeListener([]() { gs_ModesReload = true; });
				gs_InitFsaa = g_Config.m_GfxFsaaSamples;
			}
			static int s_InitAll = g_Config.m_GfxDisplayAllVideoModes;
			if(gs_NumModes < 0 || gs_ModesReload || s_InitAll != g_Config.m_GfxDisplayAllVideoModes)
			{
				gs_NumModes = Game()->Graphics()->GetVideoModes(gs_aModes, 256, g_Config.m_GfxScreen);
				gs_ModesReload = false;
				s_InitAll = g_Config.m_GfxDisplayAllVideoModes;
			}
		}

		int WindowMode()
		{
			return g_Config.m_GfxFullscreen ? (g_Config.m_GfxFullscreen == 1 ? 4 : (g_Config.m_GfxFullscreen == 2 ? 3 : 2)) : (g_Config.m_GfxBorderless ? 1 : 0);
		}

		void CheckRestart()
		{
			Game()->m_Menus.SetNeedRestartGraphics(!(gs_InitFsaa == g_Config.m_GfxFsaaSamples && !gs_GpuChanged));
		}
	}

	void BuildGraphics(SSection &S)
	{
		S.Header("Display");
		S.VirtualDropdown(
			"Window mode", []() { return WindowMode(); }, [](int V) {
			static const int s_aFullscreen[] = {0, 0, 3, 2, 1};
			Game()->Graphics()->SetWindowParams(s_aFullscreen[V], V == 1); },
			[]() { return std::vector<std::string>{Localize("Windowed"), Localize("Windowed borderless"), Localize("Windowed fullscreen"), Localize("Desktop fullscreen"), Localize("Fullscreen")}; });
		S.VirtualDropdown(
			 "Screen", []() { return g_Config.m_GfxScreen; }, [](int V) { Game()->Graphics()->SwitchWindowScreen(V, true); }, []() {
			std::vector<std::string> vNames;
			for(int i = 0; i < Game()->Graphics()->GetNumScreens(); i++)
			{
				char aName[128];
				str_format(aName, sizeof(aName), "%s %d: %s", Localize("Screen"), i, Game()->Graphics()->GetScreenName(i));
				vNames.push_back(aName);
			}
			return vNames; })
			.Show([]() { return Game()->Graphics()->GetNumScreens() > 1; });
		S.VirtualDropdown(
			"Resolution", []() {
			EnsureModes();
			for(int i = 0; i < gs_NumModes; i++)
				if(g_Config.m_GfxScreenWidth == gs_aModes[i].m_WindowWidth && g_Config.m_GfxScreenHeight == gs_aModes[i].m_WindowHeight && g_Config.m_GfxScreenRefreshRate == gs_aModes[i].m_RefreshRate)
					return i;
			return 0; }, [](int V) {
			EnsureModes();
			if(V < 0 || V >= gs_NumModes)
				return;
			g_Config.m_GfxScreenWidth = gs_aModes[V].m_WindowWidth;
			g_Config.m_GfxScreenHeight = gs_aModes[V].m_WindowHeight;
			g_Config.m_GfxScreenRefreshRate = gs_aModes[V].m_RefreshRate;
			Game()->Graphics()->ResizeToScreen(); }, []() {
			EnsureModes();
			std::vector<std::string> vNames;
			for(int i = 0; i < gs_NumModes; i++)
			{
				const int G = std::gcd(gs_aModes[i].m_WindowWidth, gs_aModes[i].m_WindowHeight);
				char aMode[96];
				str_format(aMode, sizeof(aMode), "%dx%d @%dhz (%d:%d)", gs_aModes[i].m_CanvasWidth, gs_aModes[i].m_CanvasHeight, gs_aModes[i].m_RefreshRate, gs_aModes[i].m_WindowWidth / G, gs_aModes[i].m_WindowHeight / G);
				vNames.push_back(aMode);
			}
			return vNames; });
		S.Toggle("V-Sync", &g_Config.m_GfxVsync).Desc("May cause delay").OnChange([]() { Game()->Graphics()->SetVSync(g_Config.m_GfxVsync != 0); });
		S.Slider("FPS limit", &g_Config.m_GfxRefreshRate, 0, 1000, " Hz").Zero("Unlimited");
		S.Toggle("Show FPS", &g_Config.m_ClShowfps).Desc("Small counter in the top right");

		S.Header("Quality");
		S.VirtualSegment(
			 "Anti-aliasing", []() {
			const int V = g_Config.m_GfxFsaaSamples;
			return V >= 16 ? 4 : V >= 8 ? 3 : V >= 4 ? 2 : V >= 2 ? 1 : 0; }, [](int V) {
			static const int s_aSamples[] = {0, 2, 4, 8, 16};
			g_Config.m_GfxFsaaSamples = s_aSamples[std::clamp(V, 0, 4)];
			uint32_t Backend = 0;
			if(Game()->Graphics()->SetMultiSampling(g_Config.m_GfxFsaaSamples, Backend))
				g_Config.m_GfxFsaaSamples = (int)Backend;
			CheckRestart(); },
			 {"Off", "2x", "4x", "8x", "16x"})
			.Desc("May add input delay");
		S.Toggle("High detail", &g_Config.m_GfxHighDetail).Desc("Extra map layers marked as detail");

		S.Header("Advanced");
		S.Toggle("List every video mode", &g_Config.m_GfxDisplayAllVideoModes);

		S.VirtualDropdown(
			 "Graphics card", []() {
			const auto &GpuList = Game()->Graphics()->GetGpus();
			if(str_comp("auto", g_Config.m_GfxGpuName) == 0)
				return 0;
			for(size_t i = 0; i < GpuList.m_vGpus.size(); i++)
				if(str_comp(GpuList.m_vGpus[i].m_aName, g_Config.m_GfxGpuName) == 0)
					return (int)i + 1;
			return 0; }, [](int V) {
			const auto &GpuList = Game()->Graphics()->GetGpus();
			if(gs_InitGpu == -2)
				gs_InitGpu = 0;
			if(V == 0)
				str_copy(g_Config.m_GfxGpuName, "auto");
			else if(V - 1 < (int)GpuList.m_vGpus.size())
				str_copy(g_Config.m_GfxGpuName, GpuList.m_vGpus[V - 1].m_aName);
			gs_GpuChanged = V != gs_InitGpu;
			CheckRestart(); },
			 []() {
			const auto &GpuList = Game()->Graphics()->GetGpus();
			std::vector<std::string> vNames;
			char aAuto[320];
			str_format(aAuto, sizeof(aAuto), "%s (%s)", Localize("auto"), GpuList.m_AutoGpu.m_aName);
			vNames.push_back(aAuto);
			for(const auto &Gpu : GpuList.m_vGpus)
				vNames.push_back(Gpu.m_aName);
			return vNames; })
			.Show([]() { return Game()->Graphics()->GetGpus().m_vGpus.size() > 1; });
	}

	void BuildSound(SSection &S)
	{
		S.Toggle("Use sounds", &g_Config.m_SndEnable).OnChange([]() {
			Game()->m_Menus.UpdateMusicState();
			Game()->m_Menus.SetNeedRestartSound(g_Config.m_SndEnable && !Game()->Sound()->IsSoundEnabled());
		});
		auto On = []() { return g_Config.m_SndEnable != 0; };
		S.Toggle("Mute in background", &g_Config.m_SndNonactiveMute).Desc("When the window is not focused").Show(On);
		S.Header("Volume");
		S.Slider("Master", &g_Config.m_SndVolume, 0, 100, "%").Show(On);
		S.Slider("Game", &g_Config.m_SndGameVolume, 0, 100, "%").Show(On);
		S.Slider("Chat", &g_Config.m_SndChatVolume, 0, 100, "%").Show(On);
		S.Slider("Map", &g_Config.m_SndMapVolume, 0, 100, "%").Show(On);
		S.Slider("Menu music", &g_Config.m_SndBackgroundMusicVolume, 0, 100, "%").Show(On);
		S.Header("Game sounds");
		S.Toggle("Game sounds", &g_Config.m_SndGame).Show(On);
		S.Toggle("Gun", &g_Config.m_SndGun).Show(On);
		S.Toggle("Long pain", &g_Config.m_SndLongPain).Desc("When shooting in freeze").Show(On);
		S.Toggle("Menu music", &g_Config.m_SndMusic).OnChange([]() { Game()->m_Menus.UpdateMusicState(); }).Show(On);
		S.Header("Chat sounds");
		S.Toggle("Server messages", &g_Config.m_SndServerMessage).Show(On);
		S.Toggle("Chat", &g_Config.m_SndChat).Show(On);
		S.Toggle("Team chat", &g_Config.m_SndTeamChat).Show(On);
		S.Toggle("Mentions", &g_Config.m_SndHighlight).Show(On);
	}

	void BuildDdnet(SSection &S)
	{
		S.Header("Demos and replays");
		S.Toggle("Keep your best race as a demo", &g_Config.m_ClAutoRaceRecord);
		S.Toggle("Replays", &g_Config.m_ClReplays);
		S.Slider("Replay length", &g_Config.m_ClReplayLength, 10, 600, " s").Show([]() { return g_Config.m_ClReplays != 0; });

		S.Header("Ghost");
		S.Toggle("Race ghost", &g_Config.m_ClRaceGhost).Desc("Races your best time next to you");
		S.Toggle("Show ghost", &g_Config.m_ClRaceShowGhost).Show([]() { return g_Config.m_ClRaceGhost != 0; });
		S.Slider("Ghost opacity", &g_Config.m_ClRaceGhostAlpha, 0, 100, "%").Show([]() { return g_Config.m_ClRaceGhost != 0; });
		S.Toggle("Save ghost", &g_Config.m_ClRaceSaveGhost).Show([]() { return g_Config.m_ClRaceGhost != 0; });
		S.Toggle("Only save new records", &g_Config.m_ClRaceGhostSaveBest).Show([]() { return g_Config.m_ClRaceGhost != 0 && g_Config.m_ClRaceSaveGhost != 0; });

		S.Header("World");
		S.Slider("Overlay entities", &g_Config.m_ClOverlayEntities, 0, 100, "%");
		S.Toggle("Text entities", &g_Config.m_ClTextEntities);
		S.Slider("Text entities size", &g_Config.m_ClTextEntitiesSize, 20, 100).Show([]() { return g_Config.m_ClTextEntities != 0; }).OnChange([]() { Game()->m_MapImages.SetTextureScale(g_Config.m_ClTextEntitiesSize); });
		S.VirtualToggle("Show others", []() { return g_Config.m_ClShowOthers == SHOW_OTHERS_ON ? 1 : 0; }, [](int V) { g_Config.m_ClShowOthers = V ? SHOW_OTHERS_ON : SHOW_OTHERS_OFF; });
		S.Slider("Other teams opacity", &g_Config.m_ClShowOthersAlpha, 0, 100, "%").Desc("Adjust the opacity of entities belonging to other teams, such as tees and name plates");
		S.VirtualToggle("Show others (own team only)", []() { return g_Config.m_ClShowOthers == SHOW_OTHERS_ONLY_TEAM ? 1 : 0; }, [](int V) { g_Config.m_ClShowOthers = V ? SHOW_OTHERS_ONLY_TEAM : SHOW_OTHERS_OFF; });
		S.Toggle("Background quads", &g_Config.m_ClShowQuads).Desc("Quads are used for background decoration");
		S.Slider("Default zoom", &g_Config.m_ClDefaultZoom, 0, 20).OnChange([]() { Game()->m_Camera.SetZoom(CCamera::ZoomStepsToValue(g_Config.m_ClDefaultZoom - 10), g_Config.m_ClSmoothZoomTime, true); });
		S.Header("Network");
		S.Slider("Prediction margin", &g_Config.m_ClPredictionMargin, 1, 300, " ms");
		S.Toggle("Predict events", &g_Config.m_ClPredictEvents);
		S.Toggle("AntiPing", &g_Config.m_ClAntiPing).Desc("Predicts other players so the game feels lag free");
		S.VirtualSegment(
			 "Predict players", []() { return std::min(g_Config.m_ClAntiPingPlayers, 2); }, [](int V) { g_Config.m_ClAntiPingPlayers = V; }, {"Off", "All", "Interact"})
			.Show([]() { return g_Config.m_ClAntiPing != 0; });
		S.VirtualToggle("Always predict frozen tees", []() { return g_Config.m_ClAntiPingPlayers == 3 ? 1 : 0; }, [](int V) { g_Config.m_ClAntiPingPlayers = V ? 3 : 2; }).Show([]() { return g_Config.m_ClAntiPing != 0 && g_Config.m_ClAntiPingPlayers >= 2; });
		S.Toggle("Predict weapons", &g_Config.m_ClAntiPingWeapons).Show([]() { return g_Config.m_ClAntiPing != 0; });
		S.Toggle("Predict grenades", &g_Config.m_ClAntiPingGrenade).Show([]() { return g_Config.m_ClAntiPing != 0; });

		S.Header("Background colours");
		S.Color("Normal view", &g_Config.m_ClBackgroundColor);
		S.Color("Entities view", &g_Config.m_ClBackgroundEntitiesColor);
		S.Text("Background map", g_Config.m_ClBackgroundEntities, sizeof(g_Config.m_ClBackgroundEntities));
		S.Action("Reload background map", "Reload", []() { Game()->m_Background.LoadBackground(); });
		S.VirtualToggle(
			"Use current map as background", []() { return str_comp(g_Config.m_ClBackgroundEntities, CURRENT_MAP) == 0 ? 1 : 0; }, [](int V) {
			if(V)
				str_copy(g_Config.m_ClBackgroundEntities, CURRENT_MAP);
			else
				g_Config.m_ClBackgroundEntities[0] = '\0';
			Game()->m_Background.LoadBackground(); });
		S.Toggle("Show tiles layers from BG map", &g_Config.m_ClBackgroundShowTilesLayers);

		S.Header("Other");
		S.Action("New random timeout code", "Generate", []() { Game()->Client()->GenerateTimeoutSeed(); });
		S.Text("Run on join", g_Config.m_ClRunOnJoin, sizeof(g_Config.m_ClRunOnJoin)).Desc("Chat command (e.g. showall 1)");
	}
}
