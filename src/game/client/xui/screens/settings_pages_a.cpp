#include "settings_internal.h"

#include <base/str.h>

#include <engine/client.h>
#include <engine/font_icons_phosphor.h>
#include <engine/shared/config.h>
#include <engine/shared/localization.h>
#include <engine/storage.h>

#include <game/client/animstate.h>
#include <game/client/components/chat.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/menu_background.h>
#include <game/client/components/menus.h>
#include <game/client/components/skins.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <algorithm>
#include <cmath>

using namespace Xui;

namespace SettingsUi
{
	int &DummyIndex()
	{
		static int s_Dummy = 0;
		return s_Dummy;
	}

	static void SendInfo()
	{
		Game()->m_Menus.QueueSendInfo(DummyIndex() != 0);
	}

	static void ViewPath(const char *pName, bool CreateFolder = false)
	{
		char aPath[IO_MAX_PATH_LENGTH];
		if(CreateFolder)
			Game()->Storage()->CreateFolder(pName, IStorage::TYPE_SAVE);
		Game()->Storage()->GetCompletePath(IStorage::TYPE_SAVE, pName, aPath, sizeof(aPath));
		Game()->Client()->ViewFile(aPath);
	}

	static void AutoMax(SSection &S, const char *pLabel, int *pEnable, const char *pMaxLabel, int *pMax)
	{
		S.Toggle(pLabel, pEnable);
		S.Slider(pMaxLabel, pMax, 1, 1000).Zero("Unlimited").Show([pEnable]() { return *pEnable != 0; });
	}

	static std::vector<std::string> KeyPressModes()
	{
		return {"None", "Own", "Others", "All"};
	}

	void BuildGeneral(SSection &S)
	{
		S.Header("Client");
		S.VirtualDropdown(
			 "Language", []() {
			const auto &vLanguages = g_Localization.Languages();
			for(int i = 0; i < (int)vLanguages.size(); i++)
				if(str_comp(vLanguages[i].m_Filename.c_str(), g_Config.m_ClLanguagefile) == 0)
					return i;
			return 0; }, [](int V) {
			const auto &vLanguages = g_Localization.Languages();
			if(V < 0 || V >= (int)vLanguages.size())
				return;
			str_copy(g_Config.m_ClLanguagefile, vLanguages[V].m_Filename.c_str());
			Game()->OnLanguageChange(); }, []() {
			std::vector<std::string> vNames;
			for(const auto &Language : g_Localization.Languages())
				vNames.push_back(Language.m_Name);
			return vNames; })
			.Icon(Ph::TRANSLATE);

		S.Toggle("Skip the main menu", &g_Config.m_ClSkipStartMenu);
		S.Slider("Update rate", &g_Config.m_ClRefreshRate, 0, 1000, " Hz").Zero("Unlimited");
		S.VirtualToggle(
			 "Lower update rate to save power", []() { return g_Config.m_ClRefreshRate <= 480 && g_Config.m_ClRefreshRate != 0 ? 1 : 0; }, [](int V) { g_Config.m_ClRefreshRate = V ? 480 : 0; })
			.Desc("Higher input latency");
		S.Custom("Files", []() {
			 CXui &U = Ui();
			 SBox B;
			 B.m_Gap = 6.0f;
			 U.Row(B, [&]() {
				 const SButtonStyle Style = {.m_Variant = EVariant::GHOST, .m_Height = 32.0f, .m_PadX = 12.0f, .m_FontSize = 12.5f};
				 SButtonStyle St = Style;
				 St.m_pIcon = Ph::FILE_TEXT;
				 St.m_pKey = "files.cfg";
				 if(U.Button(Localize("Settings"), St).m_Clicked)
					 ViewPath(CONFIG_FILE);
				 St.m_pIcon = Ph::FLOPPY_DISK;
				 St.m_pKey = "files.saves";
				 if(U.Button(Localize("Saves"), St).m_Clicked)
					 ViewPath(SAVES_FILE);
				 St.m_pIcon = Ph::FOLDER;
				 St.m_pKey = "files.dir";
				 if(U.Button(Localize("Config folder"), St).m_Clicked)
					 ViewPath("");
				 St.m_pIcon = Ph::IMAGE;
				 St.m_pKey = "files.themes";
				 if(U.Button(Localize("Themes"), St).m_Clicked)
					 ViewPath("themes", true);
			 });
		 }).Desc("Open in your file manager");
		S.VirtualDropdown(
			 "Menu theme", []() {
			const std::vector<CTheme> &vThemes = Game()->m_MenuBackground.GetThemes();
			for(int i = 0; i < (int)vThemes.size(); i++)
				if(str_comp(vThemes[i].m_Name.c_str(), g_Config.m_ClMenuMap) == 0)
					return i;
			return 0; }, [](int V) {
			const std::vector<CTheme> &vThemes = Game()->m_MenuBackground.GetThemes();
			if(V < 0 || V >= (int)vThemes.size())
				return;
			str_copy(g_Config.m_ClMenuMap, vThemes[V].m_Name.c_str());
			Game()->m_MenuBackground.LoadMenuBackground(vThemes[V].m_HasDay, vThemes[V].m_HasNight); }, []() {
			std::vector<std::string> vNames;
			for(const CTheme &Theme : Game()->m_MenuBackground.GetThemes())
			{
				if(Theme.m_Name.empty())
					vNames.push_back("(none)");
				else if(Theme.m_Name == "auto")
					vNames.push_back("(seasons)");
				else if(Theme.m_Name == "rand")
					vNames.push_back("(random)");
				else if(Theme.m_HasDay && !Theme.m_HasNight)
					vNames.push_back(Theme.m_Name + " (day)");
				else if(!Theme.m_HasDay && Theme.m_HasNight)
					vNames.push_back(Theme.m_Name + " (night)");
				else
					vNames.push_back(Theme.m_Name);
			}
			return vNames; })
			.Show([]() { return g_Config.m_XxMenuBackground == 0; });

		S.Header("Camera and weapons");
		S.VirtualToggle(
			 "Dynamic camera", []() { return g_Config.m_ClDyncam || g_Config.m_ClMouseFollowfactor > 0 ? 1 : 0; }, [](int V) {
			 if(V)
				 g_Config.m_ClDyncam = 1;
			 else
			 {
				 g_Config.m_ClDyncam = 0;
				 g_Config.m_ClMouseFollowfactor = 0;
			 } })
			.Desc("Camera follows the cursor");
		S.VirtualToggle(
			 "Smooth dynamic camera", []() { return g_Config.m_ClDyncamSmoothness != 0 ? 1 : 0; }, [](int V) {
			 if(V)
			 {
				 g_Config.m_ClDyncamSmoothness = 50;
				 g_Config.m_ClDyncamStabilizing = 50;
			 }
			 else
				 g_Config.m_ClDyncamSmoothness = 0; })
			.Show([]() { return g_Config.m_ClDyncam != 0; });
		S.Toggle("Switch weapon on pickup", &g_Config.m_ClAutoswitchWeapons);
		S.Toggle("Switch weapon when out of ammo", &g_Config.m_ClAutoswitchWeaponsOutOfAmmo);

		S.Header("Demos and screenshots");
		AutoMax(S, "Automatically record demos", &g_Config.m_ClAutoDemoRecord, "Max demos", &g_Config.m_ClAutoDemoMax);
		AutoMax(S, "Automatically take game over screenshot", &g_Config.m_ClAutoScreenshot, "Max screenshots", &g_Config.m_ClAutoScreenshotMax);
		AutoMax(S, "Automatically take statboard screenshot", &g_Config.m_ClAutoStatboardScreenshot, "Max statboard screenshots", &g_Config.m_ClAutoStatboardScreenshotMax);
		AutoMax(S, "Automatically create statboard csv", &g_Config.m_ClAutoCSV, "Max CSVs", &g_Config.m_ClAutoCSVMax);
	}

	static void CountryGrid(int *pCountry, bool *pClose = nullptr)
	{
		CXui &U = Ui();
		static char s_aFilter[25] = "";
		SBox Bar;
		Bar.m_W = Grow();
		Bar.m_Padding = SEdges(4.0f, 0.0f);
		U.Row(Bar, [&]() { U.SearchField("country.filter", s_aFilter, sizeof(s_aFilter), Localize("Search country"), Px(260.0f)); });

		std::vector<const CCountryFlags::CCountryFlag *> vFlags;
		for(size_t i = 0; i < Game()->m_CountryFlags.Num(); ++i)
		{
			const CCountryFlags::CCountryFlag &Flag = Game()->m_CountryFlags.GetByIndex(i);
			if(s_aFilter[0] == '\0' || str_utf8_find_nocase(Flag.m_aCountryCodeString, s_aFilter) != nullptr)
				vFlags.push_back(&Flag);
		}
		const int PerRow = 7;
		const int Rows = ((int)vFlags.size() + PerRow - 1) / PerRow;
		SBox List;
		List.m_W = Grow();
		List.m_H = Px(320.0f);
		List.m_Padding = SEdges(4.0f);
		U.VirtualList("country.list", Rows, 76.0f, List, [&](int Row) {
			SBox RowBox;
			RowBox.m_Gap = 6.0f;
			RowBox.m_W = Grow();
			RowBox.m_H = Px(76.0f);
			RowBox.m_Cross = EAlign::CENTER;
			U.Row(RowBox, [&]() {
				for(int c = 0; c < PerRow; c++)
				{
					const int Index = Row * PerRow + c;
					if(Index >= (int)vFlags.size())
					{
						SBox Pad;
						Pad.m_W = Grow();
						U.Row(Pad);
						continue;
					}
					const CCountryFlags::CCountryFlag *pFlag = vFlags[Index];
					char aKey[32];
					str_format(aKey, sizeof(aKey), "flag%d", pFlag->m_CountryCode);
					const bool Selected = pFlag->m_CountryCode == *pCountry;
					SBox Cell;
					Cell.m_W = Grow();
					Cell.m_Shrink = true;
					Cell.m_H = Px(68.0f);
					Cell.m_Hit = EHit::INTERACTIVE;
					Cell.m_Main = EAlign::CENTER;
					Cell.m_Cross = EAlign::CENTER;
					Cell.m_Gap = 6.0f;
					const uint32_t Id = U.MakeId(aKey);
					const SResult R = U.Interact(Id);
					const int Code = pFlag->m_CountryCode;
					const int Node = U.BoxId(Id, EDir::COLUMN, Cell, [&]() {
						SBox FlagBox;
						FlagBox.m_W = Px(44.0f);
						FlagBox.m_H = Px(22.0f);
						U.Paint(U.Row(FlagBox), [Code](CXui &Ui, const SRect &Rc) {
							Ui.DrawDirect(Rc, [Code](const SRect &P) { Game()->m_CountryFlags.Render(Code, ColorRGBA(1, 1, 1, 1), P.m_X, P.m_Y, P.m_W, P.m_H); });
						});
						STextStyle L;
						L.m_Size = 11.0f;
						L.m_Font = EFont::SEMIBOLD;
						L.m_Color = Selected ? XuiTheme::Color::TEXT : XuiTheme::Color::TEXT3;
						U.Text(Code == -1 ? Localize("None") : pFlag->m_aCountryCodeString, L);
					});
					const bool Hover = R.m_Hovered;
					const ColorRGBA Acc = U.Accent();
					U.Paint(Node, [Selected, Hover, Acc](CXui &Ui, const SRect &Rc) {
						Ui.DrawRect(Rc, CXui::SolidRect(Selected ? CXui::WithAlpha(Acc, 0.22f) : (Hover ? XuiTheme::Color::GHOST_HOVER : XuiTheme::Color::GHOST), 14.0f));
					});
					if(R.m_Clicked)
					{
						*pCountry = pFlag->m_CountryCode;
						SendInfo();
						if(pClose != nullptr)
							*pClose = false;
					}
				}
			});
		});
	}

	static CTeeRenderInfo OwnTeeInfo(bool Dummy)
	{
		const char *pSkin = Dummy ? g_Config.m_ClDummySkin : g_Config.m_ClPlayerSkin;
		CTeeRenderInfo Info;
		const CSkins::CSkinContainer *pContainer = Game()->m_Skins.FindContainerOrNullptr(pSkin[0] == '\0' ? "default" : pSkin);
		const CSkin *pUse = pContainer == nullptr || pContainer->Skin() == nullptr ? Game()->m_Skins.Find("default") : pContainer->Skin().get();
		Info.Apply(pUse);
		Info.ApplyColors(Dummy ? g_Config.m_ClDummyUseCustomColor : g_Config.m_ClPlayerUseCustomColor, Dummy ? g_Config.m_ClDummyColorBody : g_Config.m_ClPlayerColorBody, Dummy ? g_Config.m_ClDummyColorFeet : g_Config.m_ClPlayerColorFeet);
		return Info;
	}

	static void TeeStage(float W, float H, float TeeSize)
	{
		CXui &U = Ui();
		const bool D = DummyIndex() != 0;
		const CTeeRenderInfo Info = OwnTeeInfo(D);
		const int Emote = D ? g_Config.m_ClDummyDefaultEyes : g_Config.m_ClPlayerDefaultEyes;
		SBox B;
		B.m_W = Px(W);
		B.m_H = Px(H);
		const int Node = U.Row(B);
		const ColorRGBA Accent = U.Accent();
		U.Paint(Node, [Info, Emote, TeeSize, Accent](CXui &Ui, const SRect &Rc) {
			IGraphics::SUiRect Bg = CXui::SolidRect(CXui::Mix(XuiTheme::Rgb(0x10142A), Accent, 0.10f), 22.0f);
			Bg.m_Fill2 = XuiTheme::Rgb(0x07080F);
			Bg.m_GradientAngle = 90.0f;
			Ui.DrawRect(Rc, Bg);
			Ui.DrawRect({Rc.CenterX() - TeeSize * 0.42f, Rc.CenterY() + TeeSize * 0.36f, TeeSize * 0.84f, TeeSize * 0.12f}, CXui::SolidRect(ColorRGBA(0, 0, 0, 0.35f), TeeSize * 0.06f));
			const SRect Tee = {Rc.CenterX() - TeeSize * 0.5f, Rc.CenterY() - TeeSize * 0.5f, TeeSize, TeeSize};
			const vec2 Delta = Ui.Mouse() - vec2(Tee.CenterX(), Tee.CenterY());
			const vec2 Dir = length(Delta) < 1.0f ? vec2(1.0f, 0.0f) : normalize(Delta);
			Ui.DrawTee(Info, Emote, Dir, Tee);
		});
	}

	static void FieldLabel(const char *pText)
	{
		STextStyle S;
		S.m_Size = 12.0f;
		S.m_Font = EFont::SEMIBOLD;
		S.m_Color = XuiTheme::Color::TEXT3;
		Ui().Text(Localize(pText), S);
	}

	static void ProfileSwitch()
	{
		static const char *const s_apProfiles[] = {"Player", "Dummy"};
		const char *apLabels[] = {Localize(s_apProfiles[0]), Localize(s_apProfiles[1])};
		Ui().Segmented("profile.which", apLabels, 2, &DummyIndex());
	}

	static void HeroCard(const std::function<void()> &Left, const std::function<void()> &Right)
	{
		CXui &U = Ui();
		SBox Card;
		Card.m_W = Grow();
		Card.m_Gap = 24.0f;
		Card.m_Padding = SEdges(16.0f);
		Card.m_Cross = EAlign::STRETCH;
		const int Node = U.Row(Card, [&]() {
			Left();
			SBox Col;
			Col.m_W = Grow();
			Col.m_Shrink = true;
			Col.m_Gap = 10.0f;
			Col.m_Padding = SEdges(4.0f, 0.0f);
			U.Column(Col, Right);
		});
		U.Paint(Node, [](CXui &Ui, const SRect &Rc) { Ui.DrawRect(Rc, CXui::GroupRect(24.0f)); });
	}

	void BuildPlayer(SSection &S)
	{
		S.Block("Profile", []() {
			HeroCard([]() { TeeStage(220.0f, 250.0f, 150.0f); }, []() {
			CXui &U = Ui();
			const bool D = DummyIndex() != 0;
			ProfileSwitch();
			FieldLabel("Name");
			char *pName = D ? g_Config.m_ClDummyName : g_Config.m_PlayerName;
			if(U.Field("player.name", pName, D ? sizeof(g_Config.m_ClDummyName) : sizeof(g_Config.m_PlayerName), {.m_pPlaceholder = Game()->Client()->PlayerName(), .m_pIcon = Ph::USER, .m_W = Grow()}).m_Changed)
				SendInfo();
			FieldLabel("Clan");
			char *pClan = D ? g_Config.m_ClDummyClan : g_Config.m_PlayerClan;
			if(U.Field("player.clan", pClan, D ? sizeof(g_Config.m_ClDummyClan) : sizeof(g_Config.m_PlayerClan), {.m_pPlaceholder = Localize("No clan"), .m_pIcon = Ph::USERS_THREE, .m_W = Grow()}).m_Changed)
				SendInfo();
			FieldLabel("Country");
			int *pCountry = D ? &g_Config.m_ClDummyCountry : &g_Config.m_PlayerCountry;
			const CCountryFlags::CCountryFlag &Flag = Game()->m_CountryFlags.GetByCountryCode(*pCountry);
			static bool s_Open = false;
			const uint32_t Id = U.MakeId("player.country");
			const SResult R = U.Interact(Id);
			CXui::SState &St = U.State(Id);
			const float Hover = St.m_Hover.Update(R.m_Hovered ? 1.0f : 0.0f, U.Dt(), XuiTheme::Motion::HOVER);
			if(R.m_Clicked)
				s_Open = !s_Open;
			SBox Btn;
			Btn.m_W = Grow();
			Btn.m_H = Px(40.0f);
			Btn.m_Gap = 10.0f;
			Btn.m_Padding = SEdges(0.0f, 14.0f, 0.0f, 8.0f);
			Btn.m_Cross = EAlign::CENTER;
			Btn.m_Hit = EHit::INTERACTIVE;
			const int Node = U.BoxId(Id, EDir::ROW, Btn, [&]() {
				SBox FlagBox;
				FlagBox.m_W = Px(40.0f);
				FlagBox.m_H = Px(20.0f);
				const int Code = *pCountry;
				CGameClient *pGame = Game();
				U.Paint(U.Row(FlagBox), [Code, pGame](CXui &Ui, const SRect &Rc) {
					Ui.DrawDirect(Rc, [Code, pGame](const SRect &Px) { pGame->m_CountryFlags.Render(Code, ColorRGBA(1, 1, 1, 1), Px.m_X, Px.m_Y, Px.m_W, Px.m_H); });
				});
				STextStyle T;
				T.m_Size = 14.0f;
				T.m_Font = EFont::MEDIUM;
				SBox TB;
				TB.m_W = Grow();
				U.Text(Code == -1 ? Localize("Not set") : Flag.m_aCountryCodeString, T, TB);
				U.Icon(Ph::CARET_DOWN, 14.0f, XuiTheme::Color::TEXT2);
			});
			U.Paint(Node, [Hover](CXui &Ui, const SRect &Rc) {
				Ui.DrawRect(Rc, CXui::SolidRect(CXui::Mix(XuiTheme::Color::GHOST, XuiTheme::Rgb(0x767680, 0.28f), Hover), Rc.m_H * 0.5f));
			});
			U.Popover("player.countrypop", &s_Open, Id, [pCountry]() {
				SBox Wrap;
				Wrap.m_W = Px(600.0f);
				Wrap.m_Gap = 6.0f;
				Ui().Column(Wrap, [pCountry]() { CountryGrid(pCountry, &s_Open); });
			}); });
		});
	}

	static void SkinGrid()
	{
		CXui &U = Ui();
		SBox Bar;
		Bar.m_Gap = 10.0f;
		Bar.m_Padding = SEdges(4.0f, 0.0f);
		Bar.m_Cross = EAlign::CENTER;
		Bar.m_W = Grow();
		Bar.m_Wrap = true;
		U.Row(Bar, [&]() {
			if(U.SearchField("skin.filter", g_Config.m_ClSkinFilterString, sizeof(g_Config.m_ClSkinFilterString), Localize("Search skins"), Px(260.0f)).m_Changed)
				Game()->m_Skins.SkinList().ForceRefresh();
			U.Spacer();
			struct SOpt
			{
				const char *m_pLabel;
				const char *m_pIcon;
				int *m_pValue;
			};
			const SOpt aOpts[] = {
				{"Download skins", Ph::DOWNLOAD_SIMPLE, &g_Config.m_ClDownloadSkins},
				{"Community skins", Ph::USERS_THREE, &g_Config.m_ClDownloadCommunitySkins},
				{"Vanilla only", Ph::STAR, &g_Config.m_ClVanillaSkinsOnly},
				{"Fat skins", Ph::CIRCLE, &g_Config.m_ClFatSkins},
			};
			for(const SOpt &Opt : aOpts)
			{
				bool On = *Opt.m_pValue != 0;
				if(U.Chip(Localize(Opt.m_pLabel), &On, Opt.m_pIcon).m_Clicked)
				{
					*Opt.m_pValue = On ? 1 : 0;
					if(Opt.m_pValue != &g_Config.m_ClFatSkins)
						U.AfterFrame([]() { Game()->RefreshSkins(CSkinDescriptor::FLAG_SIX); });
					Game()->m_Skins.SkinList().ForceRefresh();
				}
			}
		});

		Game()->m_Skins.RequestCatalog();
		std::vector<CSkins::CSkinListEntry> &vSkins = Game()->m_Skins.SkinList().Skins();
		static float s_ListW = 720.0f;
		const int PerRow = std::max(3, (int)((s_ListW - 8.0f) / 112.0f));
		const int Rows = ((int)vSkins.size() + PerRow - 1) / PerRow;
		SBox Measure;
		Measure.m_W = Grow();
		U.Paint(U.Row(Measure), [](CXui &, const SRect &Rc) { s_ListW = Rc.m_W; });
		SBox List;
		List.m_W = Grow();
		List.m_H = Px(420.0f);
		List.m_Padding = SEdges(4.0f);
		const bool Dummy = DummyIndex() != 0;
		char *pSkinName = Dummy ? g_Config.m_ClDummySkin : g_Config.m_ClPlayerSkin;
		const int SkinSize = Dummy ? sizeof(g_Config.m_ClDummySkin) : sizeof(g_Config.m_ClPlayerSkin);
		U.VirtualList("skin.list", Rows, 92.0f, List, [&](int Row) {
			SBox RowBox;
			RowBox.m_Gap = 8.0f;
			RowBox.m_W = Grow();
			RowBox.m_H = Px(92.0f);
			RowBox.m_Cross = EAlign::CENTER;
			U.Row(RowBox, [&]() {
				for(int c = 0; c < PerRow; c++)
				{
					const int Index = Row * PerRow + c;
					if(Index >= (int)vSkins.size())
					{
						SBox Pad;
						Pad.m_W = Grow();
						U.Row(Pad);
						continue;
					}
					CSkins::CSkinListEntry &Entry = vSkins[Index];
					Entry.RequestLoad();
					const char *pName = Entry.SkinContainer()->Name();
					const bool Selected = Dummy ? Entry.IsSelectedDummy() : Entry.IsSelectedMain();
					char aKey[96];
					str_format(aKey, sizeof(aKey), "skin.%s", pName);
					SBox Cell;
					Cell.m_W = Grow();
					Cell.m_Shrink = true;
					Cell.m_H = Px(84.0f);
					Cell.m_Hit = EHit::INTERACTIVE;
					Cell.m_Main = EAlign::CENTER;
					Cell.m_Cross = EAlign::CENTER;
					Cell.m_Gap = 4.0f;
					const uint32_t Id = U.MakeId(aKey);
					const SResult R = U.Interact(Id);
					const bool Hover = R.m_Hovered;
					const int Node = U.BoxId(Id, EDir::COLUMN, Cell, [&]() {
						U.TeeAvatar(pName, 48.0f);
						STextStyle Name;
						Name.m_Size = 11.5f;
						Name.m_Color = XuiTheme::Color::TEXT2;
						Name.m_Align = ETextAlign::CENTER;
						Name.m_MaxLines = 1;
						Name.m_Wrap = true;
						SBox NB;
						NB.m_W = Grow();
						NB.m_Padding = SEdges(0.0f, 6.0f);
						U.Text(pName, Name, NB);
					});
					U.Paint(Node, [Selected, Hover](CXui &Ui, const SRect &Rc) {
						Ui.DrawRect(Rc, CXui::SolidRect(Selected ? XuiTheme::Color::SELECTED : (Hover ? XuiTheme::Color::GHOST_HOVER : XuiTheme::Color::GHOST), 14.0f));
					});
					if(R.m_Clicked)
					{
						str_copy(pSkinName, pName, SkinSize);
						SendInfo();
						Game()->m_Skins.SkinList().ForceRefresh();
					}
				}
			});
		});
	}

	void BuildTee(SSection &S)
	{
		S.Block("Tee", []() {
			HeroCard([]() { TeeStage(240.0f, 290.0f, 170.0f); }, []() {
			CXui &U = Ui();
			const bool D = DummyIndex() != 0;
			ProfileSwitch();
			FieldLabel("Skin");
			SBox SkinRow;
			SkinRow.m_W = Grow();
			SkinRow.m_Gap = 8.0f;
			SkinRow.m_Cross = EAlign::CENTER;
			U.Row(SkinRow, [&]() {
				char *pSkin = D ? g_Config.m_ClDummySkin : g_Config.m_ClPlayerSkin;
				if(U.Field("tee.skin", pSkin, D ? sizeof(g_Config.m_ClDummySkin) : sizeof(g_Config.m_ClPlayerSkin), {.m_pPlaceholder = "default", .m_pIcon = Ph::T_SHIRT, .m_W = Grow()}).m_Changed)
				{
					SendInfo();
					Game()->m_Skins.SkinList().ForceRefresh();
				}
				if(U.IconButton(Ph::DICE_FIVE, {.m_Variant = EVariant::GHOST, .m_pKey = "tee.random"}).m_Clicked)
				{
					Game()->m_Skins.RandomizeSkin(D ? 1 : 0);
					SendInfo();
				}
			});
			FieldLabel("Eyes");
			int Eyes = D ? g_Config.m_ClDummyDefaultEyes : g_Config.m_ClPlayerDefaultEyes;
			const char *apEyes[] = {Localize("Normal"), Localize("Angry"), Localize("Pain"), Localize("Happy"), Localize("Surprise"), Localize("Blink")};
			if(U.Segmented("tee.eyes", apEyes, 6, &Eyes).m_Changed)
			{
				(D ? g_Config.m_ClDummyDefaultEyes : g_Config.m_ClPlayerDefaultEyes) = Eyes;
				SendInfo();
			}
			SBox ColorRow;
			ColorRow.m_W = Grow();
			ColorRow.m_Gap = 12.0f;
			ColorRow.m_Cross = EAlign::CENTER;
			ColorRow.m_Padding = SEdges(6.0f, 0.0f, 0.0f, 0.0f);
			U.Row(ColorRow, [&]() {
				int Custom = D ? g_Config.m_ClDummyUseCustomColor : g_Config.m_ClPlayerUseCustomColor;
				if(U.Switch("tee.custom", &Custom).m_Changed)
				{
					(D ? g_Config.m_ClDummyUseCustomColor : g_Config.m_ClPlayerUseCustomColor) = Custom;
					SendInfo();
				}
				STextStyle L;
				L.m_Size = 14.0f;
				L.m_Font = EFont::MEDIUM;
				U.Text(Localize("Custom colours"), L);
				U.Spacer();
				if(Custom)
				{
					unsigned *pBody = D ? &g_Config.m_ClDummyColorBody : &g_Config.m_ClPlayerColorBody;
					unsigned *pFeet = D ? &g_Config.m_ClDummyColorFeet : &g_Config.m_ClPlayerColorFeet;
					FieldLabel("Body");
					if(U.ColorPickerButton("tee.body", pBody).m_Changed)
						SendInfo();
					FieldLabel("Feet");
					if(U.ColorPickerButton("tee.feet", pFeet).m_Changed)
						SendInfo();
				}
			}); });
		});
		S.Header("Skins");
		S.Block("Skin list", []() { SkinGrid(); });
	}

}
