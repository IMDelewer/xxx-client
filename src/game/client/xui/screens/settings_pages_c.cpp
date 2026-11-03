#include "settings_internal.h"

#include <base/str.h>

#include <engine/client.h>
#include <engine/console.h>
#include <engine/font_icons_phosphor.h>
#include <engine/friends.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/components/menu_services/file_dialog.h>
#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <algorithm>
#include <cmath>

using namespace Xui;

void SettingsShowCatalog();

namespace SettingsUi
{
	namespace
	{
		int gs_AssetsMode = 0;
		const char *const gs_apAssetTabs[] = {"Entities", "Game", "Emoticons", "Particles", "HUD", "Extras"};

		void AssetsInstalled()
		{
			CXui &U = Ui();
			static int s_Tab = 0;
			static char s_aFilter[6][64] = {};
			CMenus &Menus = Game()->m_Menus;

			SBox Bar;
			Bar.m_Gap = 10.0f;
			Bar.m_W = Grow();
			Bar.m_Cross = EAlign::CENTER;
			Bar.m_Wrap = true;
			U.Row(Bar, [&]() {
				U.Segmented("assets.tabs", gs_apAssetTabs, 6, &s_Tab);
				U.SearchField("assets.filter", s_aFilter[s_Tab], sizeof(s_aFilter[s_Tab]), Localize("Search"), Px(200.0f));
				if(U.IconButton(Ph::ARROW_CLOCKWISE, {.m_pKey = "assets.reload"}).m_Clicked)
					U.AfterFrame([&Menus]() { Menus.ReloadAssets(s_Tab); });
				if(U.Button(Localize("Assets directory"), {.m_pIcon = Ph::PACKAGE, .m_pKey = "assets.dir"}).m_Clicked)
				{
					char aPath[IO_MAX_PATH_LENGTH];
					Game()->Storage()->CreateFolder("assets", IStorage::TYPE_SAVE);
					Game()->Storage()->CreateFolder(CMenus::AssetFolder(s_Tab), IStorage::TYPE_SAVE);
					Game()->Storage()->GetCompletePath(IStorage::TYPE_SAVE, CMenus::AssetFolder(s_Tab), aPath, sizeof(aPath));
					Game()->Client()->ViewFile(aPath);
				}
			});

			Menus.EnsureAssetList(s_Tab, []() {});
			std::vector<const CMenus::SCustomItem *> vItems;
			for(int i = 0; i < Menus.NumAssets(s_Tab); i++)
			{
				const CMenus::SCustomItem *pItem = Menus.AssetAt(s_Tab, i);
				if(s_aFilter[s_Tab][0] == '\0' || str_utf8_find_nocase(pItem->m_aName, s_aFilter[s_Tab]) != nullptr)
					vItems.push_back(pItem);
			}

			const int PerRow = 3;
			SBox Grid;
			Grid.m_W = Grow();
			Grid.m_Gap = 10.0f;
			U.Column(Grid, [&]() {
				for(size_t Start = 0; Start < vItems.size(); Start += PerRow)
				{
					SBox RowBox;
					RowBox.m_Gap = 10.0f;
					RowBox.m_W = Grow();
					U.Row(RowBox, [&]() {
						for(int c = 0; c < PerRow; c++)
						{
							SBox Cell;
							Cell.m_W = Grow(1.0f, 0.0f, 1000.0f);
							if(Start + c >= vItems.size())
							{
								U.Row(Cell);
								continue;
							}
							const CMenus::SCustomItem *pItem = vItems[Start + c];
							const bool Selected = str_comp(pItem->m_aName, Menus.SelectedAsset(s_Tab)) == 0;
							char aKey[96];
							str_format(aKey, sizeof(aKey), "asset.%d.%s", s_Tab, pItem->m_aName);
							Cell.m_Gap = 6.0f;
							Cell.m_Padding = SEdges(6.0f);
							Cell.m_Hit = EHit::INTERACTIVE;
							const uint32_t Id = U.MakeId(aKey);
							const SResult R = U.Interact(Id);
							const bool Hover = R.m_Hovered;
							const int Node = U.BoxId(Id, EDir::COLUMN, Cell, [&]() {
								SBox Preview;
								Preview.m_W = Grow();
								Preview.m_H = Px(104.0f);
								const int PNode = U.Row(Preview);
								const IGraphics::CTextureHandle Tex = pItem->m_RenderTexture;
								U.Paint(PNode, [Tex](CXui &Ui, const SRect &Rc) {
									Ui.DrawRect(Rc, CXui::SolidRect(XuiTheme::Color::GHOST, 11.0f));
									if(!Tex.IsValid())
										return;
									Ui.DrawDirect(Rc.Inset(6.0f), [Tex](const SRect &P) {
										IGraphics *pGraphics = Game()->Graphics();
										const float Size = std::min(P.m_W, P.m_H);
										pGraphics->WrapClamp();
										pGraphics->TextureSet(Tex);
										pGraphics->QuadsBegin();
										pGraphics->SetColor(1, 1, 1, 1);
										const IGraphics::CQuadItem Quad(P.m_X + (P.m_W - Size) / 2.0f, P.m_Y + (P.m_H - Size) / 2.0f, Size, Size);
										pGraphics->QuadsDrawTL(&Quad, 1);
										pGraphics->QuadsEnd();
										pGraphics->WrapNormal();
									});
								});
								STextStyle Name;
								Name.m_Size = 13.5f;
								Name.m_Font = EFont::BOLD;
								Name.m_MaxLines = 1;
								SBox NB;
								NB.m_Padding = SEdges(0.0f, 6.0f);
								NB.m_W = Grow();
								U.Text(pItem->m_aName, Name, NB);
							});
							U.Paint(Node, [Selected, Hover](CXui &Ui, const SRect &Rc) {
								Ui.DrawRect(Rc, CXui::SolidRect(Selected ? XuiTheme::Color::SELECTED : (Hover ? XuiTheme::Color::ROW_HOVER : XuiTheme::Color::GROUP_FILL), 16.0f));
							});
							if(R.m_Clicked && pItem->m_aName[0] != '\0')
								Menus.SelectAsset(s_Tab, pItem->m_aName);
						}
					});
				}
			});
		}

		void AssetsCatalog()
		{
			CXui &U = Ui();
			CTeedataCatalog &Catalog = Game()->m_Teedata;
			static int s_Category = 0;
			static char s_aSearch[40] = "";
			auto RequestPage = [&U, &Catalog](int Page) {
				const int Category = s_Category;
				const std::string Search = s_aSearch;
				U.AfterFrame([pCatalog = &Catalog, Category, Page, Search]() { pCatalog->Request(Category, Search.c_str(), Page); });
			};
			const char *apCategories[] = {Localize("Entities"), Localize("Weapons"), Localize("Emoticons"), Localize("Particles"), Localize("Map tiles"), Localize("Skins")};
			if(U.Tabs("catalog.categories", apCategories, std::size(apCategories), &s_Category).m_Changed || Catalog.Category() < 0)
				RequestPage(1);
			SBox Bar;
			Bar.m_W = Grow();
			Bar.m_Gap = 8.0f;
			Bar.m_Cross = EAlign::CENTER;
			U.Row(Bar, [&]() {
				const SResult Search = U.SearchField("catalog.search", s_aSearch, sizeof(s_aSearch), Localize("Search assets"), Grow());
				if(U.Button(Localize("Search"), {.m_Variant = EVariant::GHOST, .m_pKey = "catalog.fetch"}).m_Clicked || Search.m_Submitted)
					RequestPage(1);
				if(U.IconButton(Ph::ARROW_CLOCKWISE, {.m_pKey = "catalog.refresh"}).m_Clicked)
					RequestPage(Catalog.Page());
				if(U.Button("teedata.net", {.m_Variant = EVariant::GHOST, .m_pKey = "catalog.website"}).m_Clicked)
					Game()->Client()->ViewLink("https://teedata.net");
			});
			STextStyle Detail;
			Detail.m_Size = 12.0f;
			Detail.m_Color = XuiTheme::Color::TEXT2;
			Detail.m_Wrap = true;
			U.Text(Localize("Community assets from teedata.net. Original artwork belongs to its authors."), Detail, {.m_W = Grow()});
			if(Catalog.Error()[0])
			{
				U.Text(Localize(Catalog.Error()), Detail, {.m_W = Grow()});
				return;
			}
			if(Catalog.Loading())
			{
				U.Spinner(24.0f, U.Accent());
				return;
			}
			const auto &Items = Catalog.Items();
			if(Items.empty())
				U.Text(Localize("No assets found"), Detail);
			for(size_t Start = 0; Start < Items.size(); Start += 3)
			{
				SBox Row;
				Row.m_W = Grow();
				Row.m_Gap = 10.0f;
				U.Row(Row, [&]() {
					for(size_t i = Start; i < std::min(Start + 3, Items.size()); i++)
					{
						const auto &Item = Items[i];
						U.PushId(Item.m_Id);
						SBox Cell;
						Cell.m_W = Grow();
						Cell.m_Shrink = true;
						Cell.m_Padding = SEdges(10.0f);
						Cell.m_Gap = 8.0f;
						const int Node = U.Column(Cell, [&]() {
							SBox Preview;
							Preview.m_W = Grow();
							Preview.m_H = Px(100.0f);
							Preview.m_Main = Preview.m_Cross = EAlign::CENTER;
							const int Image = U.Row(Preview, [&]() {
								if(!Item.m_Texture.IsValid())
								{
									if(Item.m_pPreview)
										U.Spinner();
									else
										U.Icon(Ph::IMAGE, 26.0f, XuiTheme::Color::TEXT3);
								}
							});
							const auto Texture = Item.m_Texture;
							const vec2 Size = Item.m_Size;
							U.Paint(Image, [Texture, Size](CXui &Ui, const SRect &Rc) {
								Ui.DrawRect(Rc, CXui::SolidRect(XuiTheme::Color::GHOST, 12.0f));
								if(Texture.IsValid())
									Ui.DrawImage(Rc.Inset(4.0f), Texture, Size, 8.0f);
							});
							STextStyle Name;
							Name.m_Size = 13.5f;
							Name.m_Font = EFont::SEMIBOLD;
							Name.m_Wrap = true;
							Name.m_MaxLines = 2;
							U.Text(Item.m_Name.c_str(), Name, {.m_W = Grow()});
							U.Text(Item.m_Author.c_str(), Detail, {.m_W = Grow()});
							const bool Installed = Catalog.Installed(i);
							const bool Busy = Catalog.InstallingId() >= 0;
							const char *pLabel = Installed ? Localize(s_Category == 4 ? "Installed" : "Apply") : Localize(Catalog.InstallingId() == Item.m_Id ? "Downloading…" : "Install");
							if(U.Button(pLabel, {.m_Variant = Installed ? EVariant::GHOST : EVariant::ACCENT, .m_Disabled = (Installed && s_Category == 4) || (!Installed && Busy), .m_W = Grow(), .m_pKey = "install"}).m_Clicked)
							{
								if(Installed)
									Catalog.Apply(i);
								else
									Catalog.Install(i);
							}
						});
						U.Paint(Node, [](CXui &Ui, const SRect &Rc) { Ui.DrawRect(Rc, CXui::GroupRect(16.0f)); });
						U.PopId();
					}
				});
			}
			U.Row(Bar, [&]() {
				if(U.Button(Localize("Previous"), {.m_Variant = EVariant::GHOST, .m_Disabled = Catalog.Page() <= 1, .m_pKey = "catalog.prev"}).m_Clicked)
					RequestPage(Catalog.Page() - 1);
				char aPage[96];
				str_format(aPage, sizeof(aPage), Localize("Page %d · %d assets"), Catalog.Page(), Catalog.Total());
				U.Text(aPage, Detail);
				if(U.Button(Localize("Next"), {.m_Variant = EVariant::GHOST, .m_Disabled = Catalog.Page() * CTeedataCatalog::PAGE_SIZE >= Catalog.Total(), .m_pKey = "catalog.next"}).m_Clicked)
					RequestPage(Catalog.Page() + 1);
			});
		}

		void AccentRow()
		{
			CXui &U = Ui();
			static const ColorRGBA s_aColors[] = {XuiTheme::Rgb(0xFF3B30), XuiTheme::Rgb(0xFF9F0A), XuiTheme::Rgb(0xFFD60A), XuiTheme::Rgb(0x30D158), XuiTheme::Rgb(0x0A84FF), XuiTheme::Rgb(0xFF375F), XuiTheme::Rgb(0xF2F2F7)};
			int Selected = -1;
			const ColorRGBA Current = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_XxUiAccent));
			for(int i = 0; i < 7; i++)
			{
				if(std::fabs(s_aColors[i].r - Current.r) < 0.02f && std::fabs(s_aColors[i].g - Current.g) < 0.02f && std::fabs(s_aColors[i].b - Current.b) < 0.02f)
					Selected = i;
			}
			SBox B;
			B.m_Gap = 12.0f;
			B.m_Cross = EAlign::CENTER;
			U.Row(B, [&]() {
				if(U.SwatchRow("accent", s_aColors, 7, &Selected).m_Changed)
					g_Config.m_XxUiAccent = color_cast<ColorHSLA>(s_aColors[Selected]).Pack(false);
				U.ColorPickerButton("accent.custom", &g_Config.m_XxUiAccent);
			});
		}

		void AppearancePreview()
		{
			CXui &U = Ui();
			STextStyle Label;
			Label.m_Size = 12.0f;
			Label.m_Font = EFont::SEMIBOLD;
			Label.m_Color = XuiTheme::Color::TEXT3;
			U.Text(Localize("Live preview"), Label);
			SBox Preview;
			Preview.m_W = Grow();
			Preview.m_Gap = 16.0f;
			Preview.m_Padding = SEdges(24.0f);
			Preview.m_Cross = EAlign::CENTER;
			U.GlassPanel(Preview, [&]() {
				U.Icon(Ph::PALETTE, 40.0f, U.Accent(), true);
				STextStyle Title;
				Title.m_Size = 24.0f;
				Title.m_Font = EFont::BOLD;
				U.Text("XUI", Title);
				U.Text("xxx client", Label);
				U.Switch(Localize("Liquid glass"), &g_Config.m_XxUiGlass);
				if(U.Button(Localize("HUD editor"), {.m_Variant = EVariant::ACCENT, .m_pIcon = Ph::LAYOUT, .m_pKey = "preview.hud"}).m_Clicked)
					U.m_HudEditor = true; }, XuiTheme::Radius::XL);
		}

	}

	void BuildAssets(SSection &S)
	{
		S.Segment("Source", &gs_AssetsMode, {"Installed", "Catalog"}).Desc("Assets on this computer or the community catalog");
		S.Block("Assets", []() {
			if(gs_AssetsMode == 0)
				AssetsInstalled();
			else
				AssetsCatalog();
		});
	}

	void BuildAppearance(SSection &S)
	{
		S.m_Preview = []() { AppearancePreview(); };
		S.Custom("Accent", []() { AccentRow(); });
		S.Segment("Theme", &g_Config.m_XxUiTheme, {"Dark", "Light"});
		S.Toggle("Liquid glass", &g_Config.m_XxUiGlass);
		S.Slider("Panel transparency", &g_Config.m_XxUiTransparency, 0, 90, "%").Show([]() { return g_Config.m_XxUiGlass != 0; });
		S.Slider("Backdrop blur", &g_Config.m_XxUiBlur, 0, 200, "%").Show([]() { return g_Config.m_XxUiGlass != 0; });
		S.Action("Menu background", "Choose PNG…", []() {
			const SImageFileSelection Selection = SelectBackgroundImage();
			if(!Selection.m_Error.empty())
			{
				Ui().ShowToast(Localize(Selection.m_Error.c_str()), Ph::WARNING_CIRCLE, XuiTheme::Color::YELLOW);
				return;
			}
			if(Selection.m_Path.empty())
				return;
			CImageInfo Image;
			if(Selection.m_Path.size() >= sizeof(g_Config.m_XxUiBackground) || !Game()->Graphics()->LoadPng(Image, Selection.m_Path.c_str(), IStorage::TYPE_ABSOLUTE))
			{
				Ui().ShowToast(Localize("Could not load the background image"), Ph::WARNING_CIRCLE, XuiTheme::Color::YELLOW);
				return;
			}
			Image.Free();
			str_copy(g_Config.m_XxUiBackground, Selection.m_Path.c_str());
		});
		S.Action("Default background", "Restore", []() { g_Config.m_XxUiBackground[0] = '\0'; }).Show([]() { return g_Config.m_XxUiBackground[0] != '\0'; });
		S.Slider("UI scale", &g_Config.m_XxUiScale, 50, 200, "%").Deferred();
		S.Segment("Name plates", &g_Config.m_ClNamePlates, {"Off", "On"});
		S.Slider("Name plates size", &g_Config.m_ClNamePlatesSize, -50, 100).Show([]() { return g_Config.m_ClNamePlates != 0; });
		S.Toggle("Tee trail", &g_Config.m_XxTeeTrail);
		S.Slider("Island size", &g_Config.m_XxIslandScale, 50, 200, "%");
	}

	void BuildVisual(SSection &S)
	{
		S.Header("Tee trail");
		S.Toggle("Draw tee trails", &g_Config.m_XxTeeTrail);
		S.Toggle("Trails for other tees", &g_Config.m_XxTeeTrailOthers).Show([]() { return g_Config.m_XxTeeTrail != 0; });
		S.Slider("Trail width", &g_Config.m_XxTeeTrailWidth, 0, 20).Show([]() { return g_Config.m_XxTeeTrail != 0; });
		S.Slider("Trail length", &g_Config.m_XxTeeTrailLength, 5, 200).Show([]() { return g_Config.m_XxTeeTrail != 0; });
		S.Slider("Trail alpha", &g_Config.m_XxTeeTrailAlpha, 1, 100, "%").Show([]() { return g_Config.m_XxTeeTrail != 0; });
		S.Toggle("Taper over length", &g_Config.m_XxTeeTrailTaper).Show([]() { return g_Config.m_XxTeeTrail != 0; });
		S.Toggle("Fade over length", &g_Config.m_XxTeeTrailFade).Show([]() { return g_Config.m_XxTeeTrail != 0; });
		S.Dropdown("Trail color mode", &g_Config.m_XxTeeTrailColorMode, {"Solid", "Tee", "Rainbow", "Speed", "Random"}, 1).Show([]() { return g_Config.m_XxTeeTrail != 0; });
		S.Color("Trail solid color", &g_Config.m_XxTeeTrailColor).Show([]() { return g_Config.m_XxTeeTrail != 0; });
		S.Header("Rainbow");
		S.Toggle("Rainbow tee", &g_Config.m_XxRainbowTees);
		S.Toggle("Rainbow hook", &g_Config.m_XxRainbowHook);
		S.Toggle("Rainbow weapon", &g_Config.m_XxRainbowWeapon);
		S.Toggle("Rainbow for other tees", &g_Config.m_XxRainbowOthers).Show([]() { return g_Config.m_XxRainbowTees || g_Config.m_XxRainbowHook || g_Config.m_XxRainbowWeapon; });
		S.Segment("Rainbow mode", &g_Config.m_XxRainbowMode, {"Rainbow", "Pulse", "Darkness", "Random"}, 1).Show([]() { return g_Config.m_XxRainbowTees || g_Config.m_XxRainbowHook || g_Config.m_XxRainbowWeapon; });
		S.Slider("Rainbow speed", &g_Config.m_XxRainbowSpeed, 0, 1000, "%").Show([]() { return g_Config.m_XxRainbowTees || g_Config.m_XxRainbowHook || g_Config.m_XxRainbowWeapon; });
		S.Header("Freeze");
		S.Toggle("Use skin colors for frozen tees", &g_Config.m_XxColorFreeze);
		S.Slider("Darken frozen tees", &g_Config.m_XxColorFreezeDarken, 0, 100, "%").Show([]() { return g_Config.m_XxColorFreeze != 0; });
		S.Toggle("Also color frozen feet", &g_Config.m_XxColorFreezeFeet).Show([]() { return g_Config.m_XxColorFreeze != 0; });
		S.Header("Name plates");
		S.Toggle("Ping circle", &g_Config.m_XxNameplatePingCircle);
		S.Toggle("Country flag", &g_Config.m_XxNameplateCountry);
		S.Toggle("Skin name", &g_Config.m_XxNameplateSkins);

		S.Header("Tees and weapons");
		S.Toggle("White feet", &g_Config.m_XxWhiteFeet);
		S.Text("White feet base skin", g_Config.m_XxWhiteFeetSkin, sizeof(g_Config.m_XxWhiteFeetSkin));
		S.Toggle("Tiny tees", &g_Config.m_XxTinyTees);
		S.Toggle("Tiny tees for other tees", &g_Config.m_XxTinyTeesOthers);
		S.Slider("Tiny tee size", &g_Config.m_XxTinyTeeSize, 85, 115, "%");
		S.Segment("Render weapons as gun", &g_Config.m_XxRenderWeaponsAsGun, {"Off", "With color", "Plain"});
		S.Segment("Hammer rotates with cursor", &g_Config.m_XxHammerRotatesWithCursor, {"Off", "Rotate", "Like gun"});

		S.Header("Crosshair");
		S.Segment("Cursor style", &g_Config.m_XxCursorStyle, {"Game skin", "Crosshair"});
		S.Slider("Cursor scale", &g_Config.m_XxCursorScale, 0, 500, "%");
		S.Toggle("Show cursor in free spectate", &g_Config.m_XxRenderCursorSpec);
		S.Slider("Free spectate cursor alpha", &g_Config.m_XxRenderCursorSpecAlpha, 0, 100, "%");
	}

	void BuildGameplay(SSection &S)
	{
		S.Header("Camera");
		S.Slider("Smooth camera", &g_Config.m_XxCameraSmooth, 0, 100, "%");
		S.Slider("Look ahead", &g_Config.m_XxCameraLead, 0, 100, "%");

		S.Header("Game feel");
		S.Toggle("Focus mode", &g_Config.m_XxFocus).Desc("Hides chat, pills and other players' names. Bind a key: toggle xx_focus 0 1");

		S.Header("Prediction");
		S.Toggle("Predict movement", &g_Config.m_ClPredict);
		S.Toggle("Predict dummy", &g_Config.m_ClPredictDummy);
		S.Segment("Predict freeze", &g_Config.m_ClPredictFreeze, {"Off", "On", "Partial"});
		S.Slider("Antiping delay", &g_Config.m_ClAntiPingLimit, 0, 500, " ms").Desc("0 disables the extra delay");
		S.Segment("Unpredicted shadow", &g_Config.m_ClUnpredictedShadow, {"Off", "Self", "Others", "All"});
		S.Header("Dummy");
		S.Segment("Stop keys on switch", &g_Config.m_ClDummyResetOnSwitch, {"Off", "Dummy", "Player"});
		S.Toggle("Dummy hammers", &g_Config.m_ClDummyHammer).Desc("Hammerfly helper, resets on restart");
		S.Toggle("Dummy copies moves", &g_Config.m_ClDummyCopyMoves).Desc("Resets on restart");
		S.Header("Tees");
		S.Toggle("Show emotes", &g_Config.m_ClShowEmotes);
		S.Toggle("Show ninja skin", &g_Config.m_ClShowNinja);
	}

	void BuildRace(SSection &S)
	{
		S.Header("HUD");
		S.Toggle("Show race timer", &g_Config.m_ClShowhudTimer);
		S.Toggle("Old style records", &g_Config.m_ClShowRecord).Desc("Show the classic DDRace server and personal best lines");
	}

	void BuildSocial(SSection &S)
	{
		S.Header("Chat");
		S.Toggle("Reset chat input on escape", &g_Config.m_ClChatReset);
		S.Header("Friends");
		S.Block("Friends", []() {
			CXui &U = Ui();
			IFriends *pFriends = Game()->Friends();
			static char s_aName[MAX_NAME_LENGTH] = "";
			static char s_aClan[MAX_CLAN_LENGTH] = "";
			int Remove = -1;
			U.Group([&]() {
				U.GroupRow([&]() {
					U.Field("friend.name", s_aName, sizeof(s_aName), {.m_pPlaceholder = Localize("Name"), .m_pIcon = Ph::USER, .m_W = Grow()});
					U.Field("friend.clan", s_aClan, sizeof(s_aClan), {.m_pPlaceholder = Localize("Clan"), .m_W = Px(160.0f)});
					if(U.Button(Localize("Add friend"), {.m_Variant = EVariant::ACCENT, .m_pIcon = Ph::USER_PLUS, .m_Disabled = s_aName[0] == '\0' && s_aClan[0] == '\0', .m_pKey = "friend.add"}).m_Clicked)
					{
						pFriends->AddFriend(s_aName, s_aClan);
						s_aName[0] = '\0';
						s_aClan[0] = '\0';
					}
				},
					true);
				for(int i = 0; i < pFriends->NumFriends(); i++)
				{
					const CFriendInfo *pInfo = pFriends->GetFriend(i);
					U.PushId((uint32_t)i + 1);
					U.GroupRow([&]() {
						U.Icon(Ph::HEART, 16.0f, XuiTheme::Color::PINK, true);
						SBox NB;
						NB.m_W = Grow();
						NB.m_Gap = 1.0f;
						U.Column(NB, [&]() {
							STextStyle N;
							N.m_Size = 14.5f;
							N.m_Font = EFont::SEMIBOLD;
							U.Text(pInfo->m_aName[0] != '\0' ? pInfo->m_aName : Localize("Whole clan"), N);
							if(pInfo->m_aClan[0] != '\0')
							{
								STextStyle C;
								C.m_Size = 12.0f;
								C.m_Color = XuiTheme::Color::TEXT3;
								U.Text(pInfo->m_aClan, C);
							}
						});
						if(U.IconButton(Ph::TRASH, {.m_Variant = EVariant::GHOST, .m_pKey = "rm"}).m_Clicked)
							Remove = i;
					},
						false);
					U.PopId();
				}
			});
			if(Remove >= 0)
			{
				const CFriendInfo *pInfo = pFriends->GetFriend(Remove);
				char aName[MAX_NAME_LENGTH], aClan[MAX_CLAN_LENGTH];
				str_copy(aName, pInfo->m_aName);
				str_copy(aClan, pInfo->m_aClan);
				pFriends->RemoveFriend(aName, aClan);
			}
		});
		S.Header("Wars and team");
	}

	void BuildIsland(SSection &S)
	{
		S.Toggle("Dynamic island", &g_Config.m_XxIsland).Desc("Show a pill with notifications at the top of the screen");
		S.Toggle("Timer pill", &g_Config.m_XxIslandIdle).Desc("Keep the race timer pill visible when there is nothing to report");
		S.Slider("Size", &g_Config.m_XxIslandScale, 50, 200, "%");
		S.Slider("Top offset", &g_Config.m_XxIslandOffset, 0, 100);
		S.Slider("Notification duration", &g_Config.m_XxIslandDuration, 25, 400, "%");
		S.Toggle("Notification sound", &g_Config.m_XxIslandSounds);
		S.Header("Sources");
		S.Toggle("Now playing", &g_Config.m_XxIslandSrcMusic).Desc("Spotify or any MPRIS player");
		S.Toggle("Split deltas", &g_Config.m_XxIslandSrcSplits);
		S.Toggle("Clips", &g_Config.m_XxIslandSrcClips);
		S.Toggle("Achievements", &g_Config.m_XxIslandSrcAchievements);
		S.Toggle("Block events", &g_Config.m_XxIslandSrcBlock);
		S.Toggle("Friends", &g_Config.m_XxIslandSrcFriends);
		S.Toggle("Chat highlights", &g_Config.m_XxIslandSrcChat);
		S.Toggle("Votes", &g_Config.m_XxIslandSrcVotes);
		S.Toggle("Map download", &g_Config.m_XxIslandSrcDownload);
#if defined(DDEV_XX)
		S.Header("Test");
		static const char *const s_apTests[] = {"notify", "vote", "download", "queue", "music", "split", "achievement", "block"};
		for(const char *pTest : s_apTests)
		{
			S.Action(pTest, "Run test", [pTest]() {
				 char aCommand[64];
				 str_format(aCommand, sizeof(aCommand), "xx_island_test %s", pTest);
				 Game()->Console()->ExecuteLine(aCommand, -1);
			 }).Desc("Show this island template");
		}
#endif
	}
}

void SettingsShowCatalog()
{
	SettingsUi::gs_AssetsMode = 1;
}
