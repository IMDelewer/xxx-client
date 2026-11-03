#include <base/str.h>

#include <engine/demo.h>
#include <engine/font_icons_phosphor.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/xui/xui.h>
#include <game/localization.h>

#include <algorithm>

using namespace Xui;

void CXui::RenderDemoPlayback(CMenus &M)
{
	m_LightTheme = false;
	const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();
	static char s_aSliceName[128] = "cut";
	const int Total = std::max(1, pInfo->m_LastTick - pInfo->m_FirstTick);
	auto Pause = [&]() {
		if(pInfo->m_Paused)
			DemoPlayer()->Unpause();
		else
			DemoPlayer()->Pause();
	};
	auto Marker = [&](int Direction) {
		int Tick = Direction < 0 ? pInfo->m_FirstTick : pInfo->m_LastTick;
		for(int I = 0; I < pInfo->m_NumTimelineMarkers; ++I)
		{
			const int Candidate = pInfo->m_aTimelineMarkers[I];
			if(Direction < 0 && Candidate < pInfo->m_CurrentTick - 1)
				Tick = std::max(Tick, Candidate);
			if(Direction > 0 && Candidate > pInfo->m_CurrentTick + 1)
				Tick = std::min(Tick, Candidate);
		}
		M.HandleDemoSeeking((Tick - pInfo->m_FirstTick) / (float)Total, 0.0f);
	};
	const bool Keyboard = !GameClient()->m_GameConsole.IsActive() && M.m_Popup == CMenus::POPUP_NONE && M.m_DemoPlayerState == CMenus::DEMOPLAYER_NONE && g_Config.m_ClDemoKeyboardShortcuts;
	if(Keyboard)
	{
		if(Input()->KeyPress(KEY_SPACE) || Input()->KeyPress(KEY_RETURN) || Input()->KeyPress(KEY_K))
			Pause();
		if(Input()->KeyPress(KEY_UP))
			DemoPlayer()->AdjustSpeedIndex(1);
		if(Input()->KeyPress(KEY_DOWN))
			DemoPlayer()->AdjustSpeedIndex(-1);
		if(Input()->KeyPress(KEY_LEFT) || Input()->KeyPress(KEY_J))
		{
			if(Input()->ModifierIsPressed())
				Marker(-1);
			else
				M.HandleDemoSeeking(-1.0f, -5.0f);
		}
		if(Input()->KeyPress(KEY_RIGHT) || Input()->KeyPress(KEY_L))
		{
			if(Input()->ModifierIsPressed())
				Marker(1);
			else
				M.HandleDemoSeeking(-1.0f, 5.0f);
		}
		if(Input()->KeyPress(KEY_HOME))
			M.HandleDemoSeeking(0.0f, 0.0f);
		if(Input()->KeyPress(KEY_END))
			M.HandleDemoSeeking(1.0f, 0.0f);
		if(Input()->KeyPress(KEY_PERIOD))
			M.DemoSeekTick(IDemoPlayer::TICK_NEXT);
		if(Input()->KeyPress(KEY_COMMA))
			M.DemoSeekTick(IDemoPlayer::TICK_PREVIOUS);
		const int aKeys[] = {KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9};
		for(int I = 0; I < 10; ++I)
			if(Input()->KeyPress(aKeys[I]))
				M.HandleDemoSeeking(I / 10.0f, 0.0f);
	}
	if(!M.IsActive() && M.m_Popup == CMenus::POPUP_NONE)
		return;
	RunPage([&]() {
		const float Show = Animate(MakeId("sb.demo.presence"), GameClient()->m_Scoreboard.IsActive() ? 1.0f : 0.0f, XuiTheme::Motion::PAGE, 0, 0.0f);
		if(Show > 0.01f)
			RenderScoreboardPanel(Show);
		SBox Panel;
		Panel.m_Position = EPos::ABSOLUTE;
		Panel.m_Left = Panel.m_Right = 40.0f;
		Panel.m_Bottom = 30.0f;
		Panel.m_Padding = SEdges(20.0f);
		Panel.m_Gap = 12.0f;
		Panel.m_Hit = EHit::BLOCK;
		GlassPanel(Panel, [&]() {
			char aName[128];
			DemoPlayer()->GetDemoName(aName, sizeof(aName));
			Text(aName);
			int Position = std::clamp((int)((pInfo->m_CurrentTick - pInfo->m_FirstTick) * 10000.0 / Total), 0, 10000);
			if(Slider("demo.position", &Position, {.m_Min = 0, .m_Max = 10000, .m_Width = std::max(200.0f, m_ScreenUnits.x - 130.0f)}).m_Changed)
				M.HandleDemoSeeking(Position / 10000.0f, 0.0f);
			SBox Controls;
			Controls.m_W = Grow();
			Controls.m_Gap = 10.0f;
			Controls.m_Cross = EAlign::CENTER;
			Row(Controls, [&]() {
				if(Button("-5 s", {.m_pKey = "demo.back"}).m_Clicked)
					M.HandleDemoSeeking(-1.0f, -5.0f);
				if(Button("<", {.m_pKey = "demo.previous"}).m_Clicked)
					M.DemoSeekTick(IDemoPlayer::TICK_PREVIOUS);
				if(Button(Localize(pInfo->m_Paused ? "Play" : "Pause"), {.m_Variant = EVariant::ACCENT, .m_pKey = "demo.pause"}).m_Clicked)
					Pause();
				if(Button(">", {.m_pKey = "demo.next"}).m_Clicked)
					M.DemoSeekTick(IDemoPlayer::TICK_NEXT);
				if(Button("+5 s", {.m_pKey = "demo.forward"}).m_Clicked)
					M.HandleDemoSeeking(-1.0f, 5.0f);
				if(Button("-", {.m_pKey = "demo.slower"}).m_Clicked)
					DemoPlayer()->AdjustSpeedIndex(-1);
				char aSpeed[32];
				str_format(aSpeed, sizeof(aSpeed), "×%g", pInfo->m_Speed);
				Text(aSpeed);
				if(Button("+", {.m_pKey = "demo.faster"}).m_Clicked)
					DemoPlayer()->AdjustSpeedIndex(1);
				char aTime[96];
				const int Current = (pInfo->m_CurrentTick - pInfo->m_FirstTick) / SERVER_TICK_SPEED;
				const int Length = Total / SERVER_TICK_SPEED;
				str_format(aTime, sizeof(aTime), "%d:%02d / %d:%02d", Current / 60, Current % 60, Length / 60, Length % 60);
				Text(aTime, {}, {.m_W = Grow()});
				if(Button(Localize("Close"), {.m_pKey = "demo.close"}).m_Clicked)
					Client()->Disconnect();
			});
			Row(Controls, [&]() {
				if(Button(Localize("Previous marker"), {.m_pKey = "demo.marker.prev"}).m_Clicked)
					Marker(-1);
				if(Button(Localize("Next marker"), {.m_pKey = "demo.marker.next"}).m_Clicked)
					Marker(1);
				if(Button(Localize("Start"), {.m_pKey = "demo.cut.begin"}).m_Clicked)
					Client()->DemoSliceBegin();
				if(Button(Localize("End"), {.m_pKey = "demo.cut.end"}).m_Clicked)
					Client()->DemoSliceEnd();
				if(Button(Localize("Save cut"), {.m_pKey = "demo.cut.save"}).m_Clicked)
				{
					M.m_DemoPlayerState = CMenus::DEMOPLAYER_SLICE_SAVE;
					M.m_DemoSliceInput.Set("cut");
					str_copy(s_aSliceName, "cut");
				}
			});
		});
		if(M.m_DemoPlayerState == CMenus::DEMOPLAYER_SLICE_SAVE)
		{
			static bool s_Open = true;
			static bool s_Overwrite = false;
			static int s_RemoveChat = 0;
			static int s_Render = 0;
			Modal("demo.cut", &s_Open, [&]() {
				SectionHeader(Localize("Save cut"));
				if(Field("demo.cut.name", s_aSliceName, sizeof(s_aSliceName), {.m_W = Grow()}).m_Changed)
				{
					M.m_DemoSliceInput.Set(s_aSliceName);
					s_Overwrite = false;
				}
				SettingsRow(Localize("Remove chat"), "", [&]() { Switch("demo.cut.remove_chat", &s_RemoveChat); });
#if defined(CONF_VIDEORECORDER)
				SettingsRow(Localize("Render video"), "", [&]() { Switch("demo.cut.render", &s_Render); });
#endif
				if(s_Overwrite)
					Text(Localize("File already exists, do you want to overwrite it?"));
				Row({}, [&]() {
					if(Button(Localize("Cancel"), {.m_pKey = "demo.cut.cancel"}).m_Clicked)
						s_Open = false;
					if(Button(Localize("Save"), {.m_Variant = EVariant::ACCENT, .m_Disabled = !str_valid_filename(M.m_DemoSliceInput.GetString()), .m_pKey = "demo.cut.confirm"}).m_Clicked)
					{
						char aPath[IO_MAX_PATH_LENGTH];
						str_format(aPath, sizeof(aPath), "%s/%s%s", M.m_aCurrentDemoFolder, M.m_DemoSliceInput.GetString(), str_endswith(M.m_DemoSliceInput.GetString(), ".demo") ? "" : ".demo");
						if(!s_Overwrite && Storage()->FileExists(aPath, IStorage::TYPE_SAVE))
							s_Overwrite = true;
						else
						{
							Client()->DemoSlice(aPath, CMenus::DemoFilterChat, &s_RemoveChat);
							str_copy(M.m_aCurrentDemoSelectionName, M.m_DemoSliceInput.GetString());
							if(str_endswith(M.m_aCurrentDemoSelectionName, ".demo"))
								M.m_aCurrentDemoSelectionName[str_length(M.m_aCurrentDemoSelectionName) - 5] = '\0';
							if(M.m_DemolistStorageType != IStorage::TYPE_ALL && M.m_DemolistStorageType != IStorage::TYPE_SAVE)
								M.m_DemolistStorageType = IStorage::TYPE_ALL;
							M.DemolistPopulate();
							M.DemolistOnUpdate(false);
#if defined(CONF_VIDEORECORDER)
							if(s_Render)
							{
								M.m_Popup = CMenus::POPUP_RENDER_DEMO;
								M.m_DemoRenderInput.Set(M.m_aCurrentDemoSelectionName);
							}
#endif
							s_Open = false;
						}
					}
				});
			});
			if(!s_Open)
			{
				M.m_DemoPlayerState = CMenus::DEMOPLAYER_NONE;
				s_Open = true;
				s_Overwrite = false;
			}
		}
		else
			RenderShellPopup(M);
	});
}
