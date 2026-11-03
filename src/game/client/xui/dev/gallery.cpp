#if defined(DDEV_XX)
#include <base/str.h>

#include <engine/font_icons_phosphor.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/xui/xui.h>

#include <cmath>

using namespace Xui;

namespace
{
struct SGallery
{
	int m_NavSel = 1;
	int m_Dock = 3;
	int m_LastConfig = -1;
	int m_aSwitch[4] = {1, 0, 1, 1};
	int m_Seg = 1;
	int m_Tabs = 0;
	int m_Region = 0;
	int m_aSlider[3] = {64, 25, 0};
	int m_KeyCode = 102;
	int m_Swatch = 0;
	unsigned m_Hsl = 0xB4FF80;
	bool m_aChip[4] = {true, false, false, true};
	char m_aText[64] = "xxx client";
	char m_aSearch[64] = "";
	char m_aFilter[64] = "";
	char m_aDisabled[64] = "Disabled field";
	char m_aName[64] = "nameless tee";
	bool m_CtxOpen = false;
	vec2 m_CtxAt = vec2(0, 0);
	int m_CtxChosen = -1;
	bool m_ModalOpen = false;
	bool m_PopOpen = false;
	int m_ListClicked = -1;
	float m_Progress = 0.0f;
};

const char *const gs_apSeg[] = {"Off", "Blur", "Glass"};
const char *const gs_apTabs[] = {"All", "Favorites", "Recent", "Friends"};
const char *const gs_apRegions[] = {"Any", "EU", "NA", "SA", "AS"};
const char *const gs_apPlayers[] = {"Kirito", "nameless tee", "Zerodin", "ChillerDragon", "Nami", "brainless tee", "Sora", "mela"};
}

void CXui::RenderGallery()
{
	static SGallery s;
	if(s.m_LastConfig != g_Config.m_XxUiGallery)
	{
		const int V = g_Config.m_XxUiGallery;
		s.m_LastConfig = V;
		if(V >= 1 && V <= 5)
			s.m_NavSel = V < 5 ? V : 5;
		else if(V >= 6)
			s.m_NavSel = 3;
		s.m_CtxOpen = V == 6;
		s.m_CtxAt = vec2(560.0f, 330.0f);
		s.m_ModalOpen = V == 7;
		s.m_PopOpen = V == 9;
		if(V == 8)
			ShowToast("Settings saved", Ph::CHECK_CIRCLE, XuiTheme::Color::GREEN);
	}
	s.m_Progress = std::fmod(s.m_Progress + m_Dt * 0.12f, 1.0f);

	static const SNavItem s_aNav[] = {
		{"Gallery", nullptr, nullptr, true},
		{"Buttons", Ph::SLIDERS_HORIZONTAL, nullptr, false},
		{"Inputs", Ph::PENCIL_SIMPLE, nullptr, false},
		{"Feedback", Ph::BELL, nullptr, false},
		{"Settings", Ph::GEAR_SIX, nullptr, false},
		{"Lists", Ph::STACK, "10k", false},
		{"xxx", nullptr, nullptr, true},
		{"Island", Ph::LIGHTNING, nullptr, false},
		{"Sound", Ph::MUSIC_NOTES, nullptr, false},
	};
	static const SDockItem s_aDock[] = {
		{"Home", Ph::HOUSE, true},
		{"Servers", Ph::HARD_DRIVES, true},
		{"Clips", Ph::FILM_SLATE, true},
		{"Settings", Ph::SLIDERS_HORIZONTAL, true},
		{"Island", Ph::BELL, false},
		{"Quit", Ph::POWER, false},
	};
	static const char *const s_apTitles[] = {"Buttons", "Inputs", "Feedback", "Settings", "Lists", "Island", "Sound"};
	static const char *const s_apDescs[] = {
		"Cap buttons in every variant and state",
		"Fields, switches, segmented controls, sliders and key capture",
		"Progress, loading, avatars and overlays",
		"A group of rows, the way every settings page is made",
		"Ten thousand rows, rendered only where they are visible",
		"Pill behaviour and sizes",
		"Volume and sound packs"};
	const int Page = s.m_NavSel < 6 ? s.m_NavSel - 1 : s.m_NavSel - 2;

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
	Row(Brand, [&]() {
		Text("xxx", Wordmark);
		STextStyle Version;
		Version.m_Size = 12.0f;
		Version.m_Font = EFont::SEMIBOLD;
		Version.m_Color = XuiTheme::Color::TEXT3;
		Text("0.1", Version);
	});

	auto Caption = [&](const char *pText) {
		STextStyle Style;
		Style.m_Size = 11.5f;
		Style.m_Color = XuiTheme::Color::TEXT3;
		Text(pText, Style);
	};
	auto Cell = [&](const char *pCaption, const FBuild &Content) {
		SBox B;
		B.m_Gap = 8.0f;
		B.m_Cross = EAlign::CENTER;
		Column(B, [&]() {
			Content();
			Caption(pCaption);
		});
	};
	auto Flow = [&](const FBuild &Content) {
		SBox B;
		B.m_Gap = 18.0f;
		B.m_W = Grow();
		B.m_Wrap = true;
		B.m_Cross = EAlign::END;
		Row(B, Content);
	};
	auto Label = [&](const char *pText) {
		STextStyle Style;
		Style.m_Size = XuiTheme::Font::SECTION_HEADER;
		Style.m_Font = EFont::SEMIBOLD;
		Style.m_Color = XuiTheme::Color::TEXT3;
		SBox B;
		B.m_Padding = SEdges(8.0f, 0.0f, 0.0f, 4.0f);
		Text(pText, Style, B);
	};

	auto ButtonsPage = [&]() {
		Label("Variants");
		Flow([&]() {
			Cell("Accent", [&]() { Button("Connect", {.m_Variant = EVariant::ACCENT, .m_pIcon = Ph::PLAY}); });
			Cell("Ghost", [&]() { Button("Map info"); });
			Cell("Glass", [&]() { Button("Glass", {.m_Variant = EVariant::GLASS}); });
			Cell("With icon", [&]() { Button("Settings", {.m_pIcon = Ph::GEAR_SIX}); });
			Cell("Loading", [&]() { Button("Connecting", {.m_Variant = EVariant::ACCENT, .m_Loading = true}); });
			Cell("Large", [&]() { Button("Connect", {.m_Variant = EVariant::ACCENT, .m_pIcon = Ph::PLAY, .m_Height = 50.0f, .m_PadX = 30.0f, .m_FontSize = 16.0f}); });
		});
		Label("States");
		Flow([&]() {
			Cell("Rest", [&]() { Button("Button", {.m_pKey = "s0"}); });
			Cell("Hover", [&]() { Button("Button", {.m_Force = 1, .m_pKey = "s1"}); });
			Cell("Pressed", [&]() { Button("Button", {.m_Force = 2, .m_pKey = "s2"}); });
			Cell("Focus", [&]() { Button("Button", {.m_Force = 3, .m_pKey = "s3"}); });
			Cell("Disabled", [&]() { Button("Button", {.m_Disabled = true, .m_pKey = "s4"}); });
			Cell("Accent rest", [&]() { Button("Button", {.m_Variant = EVariant::ACCENT, .m_pKey = "a0"}); });
			Cell("Accent hover", [&]() { Button("Button", {.m_Variant = EVariant::ACCENT, .m_Force = 1, .m_pKey = "a1"}); });
			Cell("Accent pressed", [&]() { Button("Button", {.m_Variant = EVariant::ACCENT, .m_Force = 2, .m_pKey = "a2"}); });
			Cell("Accent focus", [&]() { Button("Button", {.m_Variant = EVariant::ACCENT, .m_Force = 3, .m_pKey = "a3"}); });
			Cell("Accent disabled", [&]() { Button("Button", {.m_Variant = EVariant::ACCENT, .m_Disabled = true, .m_pKey = "a4"}); });
		});
		Label("Icon buttons");
		Flow([&]() {
			Cell("Ghost", [&]() { IconButton(Ph::ARROW_CLOCKWISE, {.m_pKey = "i0"}); });
			Cell("Accent", [&]() { IconButton(Ph::PLUS, {.m_Variant = EVariant::ACCENT, .m_pKey = "i1"}); });
			Cell("Glass", [&]() { IconButton(Ph::COPY, {.m_Variant = EVariant::GLASS, .m_pKey = "i2"}); });
			Cell("Plain", [&]() { IconButton(Ph::TRASH, {.m_Variant = EVariant::PLAIN, .m_pKey = "i3"}); });
			Cell("Disabled", [&]() { IconButton(Ph::SHARE_NETWORK, {.m_Disabled = true, .m_pKey = "i4"}); });
			static bool s_Star = false;
			Cell("Star", [&]() {
				if(StarButton(Ph::STAR, s_Star, "star").m_Clicked)
					s_Star = !s_Star;
			});
		});
		Label("Badges, chips and keys");
		Flow([&]() {
			Cell("Badge", [&]() { Badge("12"); });
			Cell("Green", [&]() { Badge("Online", XuiTheme::Color::GREEN); });
			Cell("Yellow", [&]() { Badge("Beta", XuiTheme::Color::YELLOW); });
			Cell("Cyan", [&]() { Badge("EU", XuiTheme::Color::CYAN); });
			Cell("Chip", [&]() { Chip("Hide full", &s.m_aChip[0]); });
			Cell("Chip off", [&]() { Chip("Favorites", &s.m_aChip[1], Ph::STAR); });
			Cell("Key", [&]() { KbdLabel("F3"); });
			Cell("Small key", [&]() { KbdLabel("F4", 20.0f); });
		});
	};

	auto InputsPage = [&]() {
		Label("Fields");
		Flow([&]() {
			Cell("Field", [&]() { Field("name", s.m_aName, sizeof(s.m_aName), {.m_pPlaceholder = "Player name"}); });
			Cell("Search", [&]() { SearchField("search", s.m_aFilter, sizeof(s.m_aFilter), "Search settings"); });
			Cell("Empty", [&]() { Field("empty", s.m_aText + 63, 1, {.m_pPlaceholder = "Placeholder"}); });
			Cell("Disabled", [&]() { Field("dis", s.m_aDisabled, sizeof(s.m_aDisabled), {.m_Disabled = true}); });
		});
		Label("Switches");
		Flow([&]() {
			Cell("On", [&]() { Switch("sw0", &s.m_aSwitch[0]); });
			Cell("Off", [&]() { Switch("sw1", &s.m_aSwitch[1]); });
			Cell("Green", [&]() { Switch("sw2", &s.m_aSwitch[2], {.m_UseGreen = true}); });
			Cell("Disabled", [&]() { Switch("sw3", &s.m_aSwitch[3], {.m_Disabled = true}); });
		});
		Label("Segmented and tabs");
		Flow([&]() {
			Cell("Segmented", [&]() { Segmented("seg", gs_apSeg, 3, &s.m_Seg); });
			Cell("Tabs", [&]() { Tabs("tabs", gs_apTabs, 4, &s.m_Tabs); });
			Cell("Regions", [&]() { Segmented("reg", gs_apRegions, 5, &s.m_Region); });
		});
		Label("Sliders");
		Flow([&]() {
			Cell("Slider", [&]() { Slider("sl0", &s.m_aSlider[0], {.m_Min = 0, .m_Max = 100}); });
			Cell("Percent", [&]() { Slider("sl1", &s.m_aSlider[1], {.m_Min = 0, .m_Max = 100, .m_pFormat = "%d%%"}); });
			Cell("Disabled", [&]() { Slider("sl2", &s.m_aSlider[2], {.m_Min = 0, .m_Max = 100, .m_Disabled = true}); });
		});
		Label("Keys and colours");
		Flow([&]() {
			Cell("Key capture", [&]() { Kbd("kbd", &s.m_KeyCode); });
			static const ColorRGBA s_aColors[] = {XuiTheme::Color::ACCENT_DEFAULT, XuiTheme::Color::ORANGE, XuiTheme::Color::YELLOW, XuiTheme::Color::GREEN, XuiTheme::Rgb(0x0A84FF), XuiTheme::Color::PINK, XuiTheme::Rgb(0xF2F2F7)};
			Cell("Swatches", [&]() { SwatchRow("swatches", s_aColors, 7, &s.m_Swatch); });
			Cell("Picker", [&]() { ColorPickerButton("picker", &s.m_Hsl); });
		});
	};

	auto FeedbackPage = [&]() {
		Label("Progress");
		Flow([&]() {
			Cell("Bar", [&]() { ProgressBar("pb0", s.m_Progress, 200.0f); });
			Cell("Fixed", [&]() { ProgressBar("pb1", 0.35f, 200.0f); });
			Cell("Spinner", [&]() { Spinner(24.0f); });
			Cell("Accent ring", [&]() { RingProgress(s.m_Progress, 44.0f, "12"); });
			Cell("Ring 70%", [&]() { RingProgress(0.7f, 44.0f, "7"); });
		});
		Label("Skeleton and tee");
		Flow([&]() {
			Cell("Skeleton", [&]() {
				SBox B;
				B.m_Gap = 8.0f;
				Column(B, [&]() {
					Skeleton(220.0f, 14.0f);
					Skeleton(160.0f, 12.0f);
				});
			});
			for(int Size : {32, 48, 64})
			{
				char aCaption[16];
				str_format(aCaption, sizeof(aCaption), "Tee %d", Size);
				Cell(aCaption, [&]() { TeeAvatar("default", (float)Size); });
			}
		});
		Label("Overlays");
		Flow([&]() {
			SResult Ctx;
			Cell("Right-click the button", [&]() {
				Ctx = Button("Context menu", {.m_pIcon = Ph::SHARE_NETWORK});
				if(Ctx.m_Clicked || Ctx.m_RightClicked)
				{
					s.m_CtxOpen = true;
					s.m_CtxAt = Mouse();
				}
			});
			Cell("Toast", [&]() {
				if(Button("Show toast", {.m_pIcon = Ph::BELL}).m_Clicked)
					ShowToast("Copied to clipboard");
			});
			Cell("Modal", [&]() {
				if(Button("Open modal", {.m_Variant = EVariant::GLASS}).m_Clicked)
					s.m_ModalOpen = true;
			});
			Cell("Popover", [&]() {
				const SResult Pop = Button("Popover", {.m_pKey = "popbtn"});
				if(Pop.m_Clicked)
					s.m_PopOpen = true;
				Popover("pop", &s.m_PopOpen, Pop.m_Id, [&]() {
					SBox B;
					B.m_Gap = 4.0f;
					B.m_Padding = SEdges(6.0f, 8.0f);
					Column(B, [&]() {
						STextStyle Title;
						Title.m_Size = 13.5f;
						Title.m_Font = EFont::SEMIBOLD;
						Text("Popover", Title);
						Caption("Anchored to its button and dismissed outside");
					});
				});
			});
			Cell("Tooltip", [&]() {
				const SResult Tip = Button("Hover me", {.m_pKey = "tipbtn"});
				Tooltip(Tip.m_Id, "A tooltip appears after a moment");
			});
		});
	};

	auto SettingsGroup = [&](int Variant) {
		Group([&]() {
			if(Variant == 0)
			{
				SettingsRow("Dynamic island", "Show a pill with notifications at the top of the screen", [&]() { Switch("island", &g_Config.m_XxIsland); });
				SettingsRow("Collapsed island", "Keep the small pill visible when there is nothing to report", [&]() { Switch("idle", &g_Config.m_XxIslandIdle); });
				SettingsRow("Island size", "", [&]() {
					SBox B;
					B.m_Gap = 14.0f;
					B.m_Cross = EAlign::CENTER;
					Row(B, [&]() {
						Slider("size", &g_Config.m_XxIslandScale, {.m_Min = 50, .m_Max = 200, .m_Width = 180.0f});
						char aValue[16];
						str_format(aValue, sizeof(aValue), "%d%%", g_Config.m_XxIslandScale);
						STextStyle Value;
						Value.m_Size = 13.0f;
						Value.m_Color = XuiTheme::Color::TEXT2;
						Value.m_Align = ETextAlign::RIGHT;
						Value.m_Tabular = true;
						SBox VB;
						VB.m_W = Px(56.0f);
						Text(aValue, Value, VB);
					});
				});
				SettingsRow("Glass style", "How panels look", [&]() { Segmented("style", gs_apSeg, 3, &s.m_Seg); });
				SettingsRow("Player name", "", [&]() { Field("pname", s.m_aName, sizeof(s.m_aName), {.m_W = Px(200.0f), .m_Right = true}); });
				SettingsRow("Zoom in", "Hold to zoom the camera", [&]() { Kbd("zoom", &s.m_KeyCode); });
				SettingsRow("Accent", "", [&]() {
					static const ColorRGBA s_aColors[] = {XuiTheme::Color::ACCENT_DEFAULT, XuiTheme::Color::ORANGE, XuiTheme::Color::YELLOW, XuiTheme::Color::GREEN, XuiTheme::Rgb(0x0A84FF), XuiTheme::Color::PINK, XuiTheme::Rgb(0xF2F2F7)};
					SwatchRow("accent", s_aColors, 7, &s.m_Swatch);
				});
				SettingsRow("Reset layout", "Restore the default interface positions", [&]() {
					if(Button("Reset", {.m_Variant = EVariant::ACCENT, .m_Height = 34.0f, .m_PadX = 16.0f}).m_Clicked)
						ShowToast("Layout reset", Ph::ARROW_CLOCKWISE, XuiTheme::Color::CYAN);
				});
			}
			else if(Variant == 1)
			{
				SettingsRow("Show island on game start", "", [&]() { Switch("a", &s.m_aSwitch[0]); });
				SettingsRow("Notification sound", "Play a soft tick when a notification appears", [&]() { Switch("b", &g_Config.m_XxIslandSounds); });
				SettingsRow("Notification duration", "", [&]() { Slider("dur", &g_Config.m_XxIslandDuration, {.m_Min = 25, .m_Max = 400, .m_pFormat = "%d%%"}); });
			}
			else
			{
				SettingsRow("Master volume", "", [&]() { Slider("vol", &s.m_aSlider[0], {.m_Min = 0, .m_Max = 100, .m_pFormat = "%d%%"}); });
				SettingsRow("Game sounds", "", [&]() { Switch("snd", &s.m_aSwitch[2]); });
				SettingsRow("Music", "", [&]() { Switch("mus", &s.m_aSwitch[1]); });
			}
		});
	};

	auto ListPage = [&]() {
		SBox B;
		B.m_W = Grow();
		B.m_H = Grow();
		B.m_Clip = true;
		B.m_Cross = EAlign::STRETCH;
		VirtualList("list", 10000, 56.0f, B, [&](int Index) {
			const int Row_ = Index;
			TeeAvatar("default", 34.0f);
			SBox Texts;
			Texts.m_Gap = 1.0f;
			Texts.m_W = Grow();
			Column(Texts, [&]() {
				char aName[64];
				str_format(aName, sizeof(aName), "%s %d", gs_apPlayers[Row_ % 8], Row_);
				STextStyle Name;
				Name.m_Size = 14.0f;
				Name.m_Font = EFont::SEMIBOLD;
				Text(aName, Name);
				char aSub[64];
				str_format(aSub, sizeof(aSub), "Kobra %d  -  %d points", 1 + Row_ % 4, Row_ * 37 % 9000);
				STextStyle Sub;
				Sub.m_Size = 12.0f;
				Sub.m_Color = XuiTheme::Color::TEXT3;
				Sub.m_Tabular = true;
				Text(aSub, Sub);
			});
			char aPing[16];
			str_format(aPing, sizeof(aPing), "%d ms", 12 + Row_ * 7 % 80);
			Badge(aPing, Row_ % 5 == 0 ? XuiTheme::Color::YELLOW : XuiTheme::Color::GREEN);
		},
			&s.m_ListClicked);
	};

	SBox Shell;
	Shell.m_Position = EPos::ABSOLUTE;
	Shell.m_Left = 36.0f;
	Shell.m_Right = 36.0f;
	Shell.m_Top = 96.0f;
	Shell.m_Bottom = 120.0f;
	if(s.m_Dock != 3)
	{
		const char *apNames[] = {"Home", "Servers", "Clips"};
		Shell.m_Left = 60.0f;
		Shell.m_Top = 112.0f;
		PageTransition(apNames[s.m_Dock], Shell, [&]() {
			STextStyle Hero;
			Hero.m_Size = 76.0f;
			Hero.m_Font = EFont::EXTRABOLD;
			Hero.m_Tracking = -0.045f;
			Text(apNames[s.m_Dock], Hero);
			STextStyle Sub;
			Sub.m_Size = 16.0f;
			Sub.m_Color = XuiTheme::Color::TEXT2;
			Text("Pick Settings in the dock to see the component pages", Sub);
		});
	}
	else
	{
		char aKey[16];
		str_format(aKey, sizeof(aKey), "page%d", s.m_NavSel);
		PageTransition(aKey, Shell, [&]() {
			SBox RowBox;
			RowBox.m_Gap = 16.0f;
			RowBox.m_W = Grow();
			RowBox.m_H = Grow();
			RowBox.m_Cross = EAlign::STRETCH;
			Row(RowBox, [&]() {
				GlassPanel({.m_Gap = 2.0f, .m_Padding = SEdges(12.0f, 10.0f), .m_W = Px(250.0f), .m_Cross = EAlign::STRETCH, .m_Scroll = EScroll::VERTICAL}, [&]() {
					SearchField("nav-search", s.m_aSearch, sizeof(s.m_aSearch), "Search", Grow());
					Gap(8.0f);
					Sidebar("nav", s_aNav, (int)std::size(s_aNav), &s.m_NavSel);
				},
					XuiTheme::Radius::LG, true);
				GlassPanel({.m_Gap = 16.0f, .m_Padding = SEdges(30.0f, 34.0f), .m_W = Grow(), .m_Cross = EAlign::STRETCH, .m_Scroll = EScroll::VERTICAL}, [&]() {
					PageHeader(s_apTitles[Page], s_apDescs[Page]);
					switch(Page)
					{
					case 0: ButtonsPage(); break;
					case 1: InputsPage(); break;
					case 2: FeedbackPage(); break;
					case 3: SettingsGroup(0); break;
					case 4: ListPage(); break;
					case 5: SettingsGroup(1); break;
					default: SettingsGroup(2); break;
					}
				},
					XuiTheme::Radius::XL, true);
			});
		});
	}

	const int DockClicked = Dock("dock", s_aDock, (int)std::size(s_aDock), &s.m_Dock);
	if(DockClicked == 4)
		ShowToast("Island test notification", Ph::BELL, XuiTheme::Color::YELLOW);
	else if(DockClicked == 5)
		g_Config.m_XxUiGallery = 0;

	static const SMenuItem s_aCtx[] = {
		{"Join server", Ph::PLAY},
		{"Add to favorites", Ph::STAR},
		{"Copy address", Ph::COPY},
		{nullptr, nullptr, false, false, true},
		{"Report", Ph::WARNING_CIRCLE},
		{"Remove", Ph::TRASH, true},
	};
	ContextMenu("ctx", "Kobra 4 - DDNet EU", s_aCtx, (int)std::size(s_aCtx), &s.m_CtxOpen, s.m_CtxAt, &s.m_CtxChosen);
	if(s.m_CtxChosen >= 0)
	{
		ShowToast(s_aCtx[s.m_CtxChosen].m_pLabel);
		s.m_CtxChosen = -1;
	}

	Modal("modal", &s.m_ModalOpen, [&]() {
		STextStyle Title;
		Title.m_Size = 20.0f;
		Title.m_Font = EFont::BOLD;
		Text("Leave the server?", Title);
		STextStyle Body;
		Body.m_Size = 14.0f;
		Body.m_Color = XuiTheme::Color::TEXT2;
		Body.m_Wrap = true;
		SBox BodyBox;
		BodyBox.m_W = Grow();
		Text("You will return to the main menu. Your position on the map is not saved.", Body, BodyBox);
		SBox Buttons;
		Buttons.m_Gap = 10.0f;
		Buttons.m_Main = EAlign::END;
		Buttons.m_W = Grow();
		Row(Buttons, [&]() {
			if(Button("Cancel", {.m_pKey = "cancel"}).m_Clicked)
				s.m_ModalOpen = false;
			if(Button("Leave", {.m_Variant = EVariant::ACCENT, .m_pKey = "leave"}).m_Clicked)
			{
				s.m_ModalOpen = false;
				ShowToast("Left the server", Ph::INFO, XuiTheme::Color::CYAN);
			}
		});
	});
}

#endif
