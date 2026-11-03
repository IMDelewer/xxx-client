#include <base/str.h>

#include <engine/favorites.h>
#include <engine/font_icons_phosphor.h>
#include <engine/friends.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/xui/xui.h>
#include <game/localization.h>

#include <string>
#include <vector>

using namespace Xui;

namespace
{
	void PaintThumb(CXui &Ui, int Node, const char *pMap, float Radius)
	{
		const CMapPreviews::SPreview Preview = Ui.Game()->m_MapPreviews.Artwork(pMap);
		Ui.Paint(Node, [Radius, Preview](CXui &U, const SRect &Rc) {
			if(Preview.Valid())
				U.DrawImage(Rc, Preview.m_Texture, vec2(Preview.m_Size.x, Preview.m_Size.y), Radius);
			else
				U.DrawRect(Rc, CXui::SolidRect(XuiTheme::Color::GROUP_FILL, Radius));
		});
	}

	ColorRGBA PingColor(int Ping)
	{
		if(Ping < 80)
			return XuiTheme::Color::GREEN;
		if(Ping < 150)
			return XuiTheme::Color::YELLOW;
		return XuiTheme::Color::RED_SOFT;
	}

	STextStyle Clip(float Size, EFont Font, ColorRGBA Color)
	{
		STextStyle S;
		S.m_Size = Size;
		S.m_Font = Font;
		S.m_Color = Color;
		S.m_Wrap = true;
		S.m_MaxLines = 1;
		return S;
	}

	SBox GrowBox()
	{
		SBox B;
		B.m_W = Grow();
		B.m_Shrink = true;
		return B;
	}

	SBox FixedBox(float W)
	{
		SBox B;
		B.m_W = Px(W);
		return B;
	}

	void CommunityIcon(CXui &Ui, CMenus &M, const char *pId, float W, float H)
	{
		const CCommunityIcon *pIcon = M.CommunityIcons().Find(pId);
		SBox B;
		B.m_W = Px(W);
		B.m_H = Px(H);
		const int Node = Ui.Row(B);
		if(pIcon == nullptr)
			return;
		Ui.Paint(Node, [&M, pIcon](CXui &U, const SRect &Rc) {
			U.DrawDirect(Rc, [&M, pIcon](const SRect &Px) {
				CUIRect R;
				R.x = Px.m_X;
				R.y = Px.m_Y;
				R.w = Px.m_W;
				R.h = Px.m_H;
				M.CommunityIcons().Render(pIcon, R, true);
			});
		});
	}
}

void CXui::RenderBrowser(CMenus &M)
{
	IServerBrowser *pBrowser = ServerBrowser();
	const int Page = g_Config.m_UiPage;
	static bool s_FiltersOpen = false;
	static int s_ContextIndex = -1;
	static bool s_ContextOpen = false;
	static vec2 s_ContextAt;
	static int s_CommunityClick = -1;
	static bool s_CommunityMulti = false;
	static int s_PageClick = -1;
	static bool s_RefreshClick = false;
	static char s_aCommunityCtx[CServerInfo::MAX_COMMUNITY_ID_LENGTH] = "";
	static bool s_CommunityCtxOpen = false;
	static vec2 s_CommunityCtxAt;
	static NETADDR s_aFavAddr[MAX_SERVER_ADDRESSES];
	static int s_FavNum = 0;
	static bool s_FavRemove = false;
	auto QueueFavorite = [](const CServerInfo *pInfo, bool Remove) {
		s_FavNum = pInfo->m_NumAddresses;
		for(int i = 0; i < s_FavNum; i++)
			s_aFavAddr[i] = pInfo->m_aAddresses[i];
		s_FavRemove = Remove;
	};

	auto Update = [&]() { Client()->ServerBrowserUpdate(); };

	const int NumServers = pBrowser->NumSortedServers();
	const CServerInfo *pSelected = nullptr;
	for(int i = 0; i < NumServers; i++)
	{
		const CServerInfo *pInfo = pBrowser->SortedGet(i);
		if(str_comp(pInfo->m_aAddress, g_Config.m_UiServerAddress) == 0)
		{
			pSelected = pInfo;
			break;
		}
	}

	SBox Wrap;
	Wrap.m_Gap = 16.0f;
	Wrap.m_W = Grow();
	Wrap.m_H = Grow();
	Row(Wrap, [&]() {
		SBox Side;
		Side.m_W = Px(280.0f);
		Side.m_H = Grow();
		Side.m_Padding = SEdges(14.0f, 12.0f);
		Side.m_Clip = true;
		GlassPanel(Side, [&]() {
			SBox S;
			S.m_W = Grow();
			S.m_H = Grow();
			S.m_Gap = 3.0f;
			S.m_Cross = EAlign::STRETCH;
			S.m_Padding.m_R = XuiTheme::Size::SCROLLBAR_W + 10.0f;
			auto Section = [&](const char *pLabel) {
				STextStyle Style;
				Style.m_Size = XuiTheme::Font::SECTION_HEADER;
				Style.m_Font = EFont::SEMIBOLD;
				Style.m_Color = XuiTheme::Color::TEXT3;
				SBox Gap;
				Gap.m_H = Px(12.0f);
				Row(Gap);
				SBox B;
				B.m_Padding = SEdges(0.0f, 12.0f, 4.0f, 12.0f);
				Text(pLabel, Style, B);
			};
			Scroll(S, [&]() {
				char aCount[16];
				str_format(aCount, sizeof(aCount), "%d", NumServers);
				struct STab
				{
					const char *m_pLabel;
					const char *m_pIcon;
					int m_Page;
				};
				const STab aTabs[] = {
					{Localize("Internet"), Ph::GLOBE_HEMISPHERE_WEST, CMenus::PAGE_INTERNET},
					{Localize("LAN"), Ph::WIFI_HIGH, CMenus::PAGE_LAN},
					{Localize("Favorites"), Ph::STAR, CMenus::PAGE_FAVORITES},
				};
				for(const STab &Tab : aTabs)
				{
					PushId((uint32_t)Tab.m_Page + 100);
					if(NavItem({Tab.m_pLabel, Tab.m_pIcon, Page == Tab.m_Page ? aCount : nullptr}, Page == Tab.m_Page).m_Clicked)
						s_PageClick = Tab.m_Page;
					PopId();
				}

				auto CommunityRow = [&](const CCommunity &Community, bool Selected, bool Unfiltered, int Key, int TargetPage) {
						char aKey[16];
						str_format(aKey, sizeof(aKey), "cm%d", Key);
						const uint32_t Id = MakeId(aKey);
						const SResult R = Interact(Id);
						SState &St = State(Id);
						const float Hover = St.m_Hover.Update(R.m_Hovered ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
						if(R.m_Clicked)
						{
							if(TargetPage >= 0)
								s_PageClick = TargetPage;
							else
							{
								s_CommunityClick = Key;
								s_CommunityMulti = Input()->ModifierIsPressed() || Input()->ShiftIsPressed();
							}
						}
						if(R.m_RightClicked)
						{
							str_copy(s_aCommunityCtx, Community.Id());
							s_CommunityCtxOpen = true;
							s_CommunityCtxAt = Mouse();
						}
						SBox Item;
						Item.m_W = Grow();
						Item.m_H = Px(40.0f);
						Item.m_Gap = 10.0f;
						Item.m_Padding = SEdges(0.0f, 10.0f);
						Item.m_Cross = EAlign::CENTER;
						Item.m_Hit = EHit::INTERACTIVE;
						const int Node = BoxId(Id, EDir::ROW, Item, [&]() {
							if(M.CommunityIcons().Find(Community.Id()) != nullptr)
								CommunityIcon(*this, M, Community.Id(), 32.0f, 16.0f);
							const bool None = str_comp(Community.Id(), IServerBrowser::COMMUNITY_NONE) == 0;
							Text(None ? Localize("Other servers") : Community.Name(), Clip(14.0f, EFont::MEDIUM, Selected || Unfiltered ? XuiTheme::Color::TEXT : XuiTheme::Color::TEXT2), GrowBox());
							char aPlayers[16];
							str_format(aPlayers, sizeof(aPlayers), "%d", Community.NumPlayers());
							STextStyle Num;
							Num.m_Size = 12.0f;
							Num.m_Color = XuiTheme::Color::TEXT3;
							Num.m_Tabular = true;
							Num.m_Align = ETextAlign::RIGHT;
							SBox CountBox;
							CountBox.m_W = Px(42.0f);
							Text(aPlayers, Num, CountBox);
						});
						const ColorRGBA Acc = m_Accent;
						Paint(Node, [Hover, Selected, Acc](CXui &U, const SRect &Rc) {
							if(Selected)
							{
								U.DrawRect(Rc, SolidRect(ColorRGBA(1, 1, 1, 0.14f + 0.04f * Hover), Rc.m_H * 0.5f));
								U.DrawRect({Rc.m_X + 4.0f, Rc.m_Y + 10.0f, 3.0f, Rc.m_H - 20.0f}, SolidRect(Acc, 1.5f));
							}
							else if(Hover > 0.01f)
								U.DrawRect(Rc, SolidRect(ColorRGBA(1, 1, 1, 0.06f * Hover), Rc.m_H * 0.5f));
						});
				};

				const std::vector<const CCommunity *> vFavorites = pBrowser->FavoriteCommunities();
				if(!vFavorites.empty())
				{
					Section(Localize("Pinned"));
					for(size_t i = 0; i < vFavorites.size() && i < 5; i++)
					{
						const int TabPage = CMenus::PAGE_FAVORITE_COMMUNITY_1 + (int)i;
						PushId((uint32_t)TabPage + 200);
						CommunityRow(*vFavorites[i], Page == TabPage, true, (int)i, TabPage);
						PopId();
					}
				}

				if(Page == CMenus::PAGE_INTERNET || Page == CMenus::PAGE_FAVORITES)
				{
					Section(Localize("Communities"));
					const IFilterList &Filter = pBrowser->CommunitiesFilter();
					const std::vector<CCommunity> &vCommunities = pBrowser->Communities();
					const bool All = Filter.Empty();
					PushId(300);
					if(NavItem({Localize("All communities"), Ph::LIST, nullptr}, All).m_Clicked && !All)
						s_CommunityClick = -2;
					PopId();
					for(size_t i = 0; i < vCommunities.size(); i++)
					{
						CommunityRow(vCommunities[i], !All && !Filter.Filtered(vCommunities[i].Id()), All, (int)i, -1);
					}
				}
			}); }, 20.0f);

		SBox Mid;
		Mid.m_W = Grow();
		Mid.m_H = Grow();
		Mid.m_Shrink = true;
		Mid.m_Clip = true;
		GlassPanel(Mid, [&]() {
			SBox Bar;
			Bar.m_W = Grow();
			Bar.m_Gap = 10.0f;
			Bar.m_Cross = EAlign::CENTER;
			Bar.m_Padding = SEdges(14.0f, 16.0f);
			Row(Bar, [&]() {
				if(SearchField("br.search", g_Config.m_BrFilterString, sizeof(g_Config.m_BrFilterString), Localize("Search map, server or address"), Px(300.0f)).m_Changed)
					Update();
				int ActiveFilters = g_Config.m_BrFilterEmpty + g_Config.m_BrFilterFull + g_Config.m_BrFilterPw + g_Config.m_BrFilterFriends + g_Config.m_BrFilterSpectators + g_Config.m_BrFilterUnfinishedMap + (g_Config.m_BrExcludeString[0] ? 1 : 0);
				char aFilters[32];
				if(ActiveFilters > 0)
					str_format(aFilters, sizeof(aFilters), "%s (%d)", Localize("Filters"), ActiveFilters);
				else
					str_copy(aFilters, Localize("Filters"));
				const SResult FilterBtn = Button(aFilters, {.m_Variant = s_FiltersOpen ? EVariant::GLASS : EVariant::GHOST, .m_pIcon = Ph::FUNNEL, .m_pKey = "br.filters"});
				if(FilterBtn.m_Clicked)
					s_FiltersOpen = !s_FiltersOpen;
				Popover("br.filterpop", &s_FiltersOpen, FilterBtn.m_Id, [&]() {
					SBox Col;
					Col.m_W = Px(340.0f);
					Col.m_Gap = 8.0f;
					Column(Col, [&]() {
						struct SToggle
						{
							const char *m_pLabel;
							int *m_pValue;
							bool m_Invert;
						};
						const SToggle aToggles[] = {
							{Localize("Has people playing"), &g_Config.m_BrFilterEmpty, false},
							{Localize("Server not full"), &g_Config.m_BrFilterFull, false},
							{Localize("No password"), &g_Config.m_BrFilterPw, false},
							{Localize("No login required"), &g_Config.m_BrFilterLogin, false},
							{Localize("Show friends only"), &g_Config.m_BrFilterFriends, false},
							{Localize("Count players only"), &g_Config.m_BrFilterSpectators, false},
							{Localize("Unfinished map"), &g_Config.m_BrFilterUnfinishedMap, false},
							{Localize("Filter connecting players"), &g_Config.m_BrFilterConnectingPlayers, false},
						};
						Group([&]() {
							for(size_t i = 0; i < std::size(aToggles); i++)
							{
								GroupRow([&]() {
									STextStyle L;
									L.m_Size = 14.0f;
									L.m_Font = EFont::MEDIUM;
									Text(aToggles[i].m_pLabel, L, GrowBox());
									char aKey[16];
									str_format(aKey, sizeof(aKey), "bf%d", (int)i);
									if(Switch(aToggles[i].m_pLabel, aToggles[i].m_pValue, {.m_pKey = aKey}).m_Changed)
										Update();
								},
									i == 0, 46.0f);
							}
						});
						STextStyle H;
						H.m_Size = 12.0f;
						H.m_Font = EFont::SEMIBOLD;
						H.m_Color = XuiTheme::Color::TEXT3;
						Text(Localize("Exclude"), H);
						if(Field("br.exclude", g_Config.m_BrExcludeString, sizeof(g_Config.m_BrExcludeString), {.m_pPlaceholder = Localize("Words to hide"), .m_pIcon = Ph::MINUS, .m_W = Grow(), .m_Clearable = true}).m_Changed)
							Update();
						Text(Localize("Game types"), H);
						if(Field("br.gametype", g_Config.m_BrFilterGametype, sizeof(g_Config.m_BrFilterGametype), {.m_pPlaceholder = "DDraceNetwork, Gores", .m_W = Grow(), .m_Clearable = true}).m_Changed)
							Update();
						if(Button(Localize("Reset filter"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::ARROW_COUNTER_CLOCKWISE, .m_pKey = "br.reset"}).m_Clicked)
							s_CommunityClick = -3;
					});
				});

				if(IconButton(Ph::ARROW_CLOCKWISE, {.m_Variant = EVariant::GHOST, .m_Loading = pBrowser->IsRefreshing(), .m_pKey = "br.refresh"}).m_Clicked)
					s_RefreshClick = true;

				Spacer();
				char aStatus[64];
				if(pBrowser->IsGettingServerlist())
					str_copy(aStatus, Localize("Getting server list from master server"));
				else
					str_format(aStatus, sizeof(aStatus), Localize("%d of %d servers"), NumServers, pBrowser->NumServers());
				STextStyle Count;
				Count.m_Size = 12.5f;
				Count.m_Font = EFont::SEMIBOLD;
				Count.m_Color = XuiTheme::Color::TEXT3;
				Count.m_Tabular = true;
				Text(aStatus, Count);
			});

			auto Header = [&](const char *pLabel, int Sort, float W, bool Right) {
				const bool Active = g_Config.m_BrSort == Sort;
				char aKey[16];
				str_format(aKey, sizeof(aKey), "hd%d", Sort);
				const uint32_t Id = MakeId(aKey);
				const SResult R = Interact(Id);
				if(R.m_Clicked)
				{
					if(Active)
						g_Config.m_BrSortOrder = g_Config.m_BrSortOrder ? 0 : 1;
					else
					{
						g_Config.m_BrSort = Sort;
						g_Config.m_BrSortOrder = Sort == IServerBrowser::SORT_NUMPLAYERS ? 1 : 0;
					}
					Update();
				}
				SBox B;
				B.m_W = W > 0.0f ? Px(W) : Grow();
				B.m_Shrink = W <= 0.0f;
				B.m_Gap = 4.0f;
				B.m_Cross = EAlign::CENTER;
				B.m_Main = Right ? EAlign::END : EAlign::START;
				B.m_Hit = EHit::INTERACTIVE;
				BoxId(Id, EDir::ROW, B, [&]() {
					STextStyle S;
					S.m_Size = 12.0f;
					S.m_Font = EFont::SEMIBOLD;
					S.m_Color = Active || R.m_Hovered ? XuiTheme::Color::TEXT : XuiTheme::Color::TEXT3;
					Text(pLabel, S);
					if(Active)
						Icon(g_Config.m_BrSortOrder ? Ph::CARET_DOWN : Ph::CARET_UP, 11.0f, XuiTheme::Color::TEXT2);
				});
			};

			SBox Head;
			Head.m_W = Grow();
			Head.m_Gap = 12.0f;
			Head.m_Cross = EAlign::CENTER;
			Head.m_Padding = SEdges(4.0f, 24.0f, 10.0f, 24.0f);
			const int HeadNode = Row(Head, [&]() {
				Row(FixedBox(48.0f));
				Header(Localize("Map · server"), IServerBrowser::SORT_MAP, 0.0f, false);
				Header(Localize("Type"), IServerBrowser::SORT_GAMETYPE, 110.0f, false);
				Header(Localize("Ping"), IServerBrowser::SORT_PING, 56.0f, true);
				Header(Localize("Players"), IServerBrowser::SORT_NUMPLAYERS, 76.0f, true);
			});
			Paint(HeadNode, [](CXui &U, const SRect &Rc) {
				U.DrawRect({Rc.m_X + 16.0f, Rc.m_Y + Rc.m_H - 1.0f, Rc.m_W - 32.0f, 1.0f}, SolidRect(ColorRGBA(1, 1, 1, 0.07f), 0.0f));
			});

			SBox ListBox;
			ListBox.m_W = Grow();
			ListBox.m_H = Grow();
			ListBox.m_Padding = SEdges(0.0f, 8.0f, 8.0f, 8.0f);
			Column(ListBox, [&]() {
				if(NumServers == 0)
				{
					SBox Empty;
					Empty.m_W = Grow();
					Empty.m_H = Grow();
					Empty.m_Main = EAlign::CENTER;
					Empty.m_Cross = EAlign::CENTER;
					Empty.m_Gap = 12.0f;
					Column(Empty, [&]() {
						if(pBrowser->IsGettingServerlist() || pBrowser->IsRefreshing())
							Spinner(28.0f);
						STextStyle S;
						S.m_Size = 14.0f;
						S.m_Color = XuiTheme::Color::TEXT3;
						if(pBrowser->IsServerlistError())
							Text(Localize("Could not get server list from master server"), S);
						else if(pBrowser->IsGettingServerlist())
							Text(Localize("Getting server list from master server"), S);
						else if(pBrowser->NumServers() > 0)
							Text(Localize("No servers match your filter criteria"), S);
						else
							Text(Localize("No servers found"), S);
					});
					return;
				}
				int Clicked = -1, Activated = -1, RightClicked = -1;
				SBox VB;
				VB.m_W = Grow();
				VB.m_H = Grow();
				VirtualList("br.list", NumServers, 54.0f, VB, [&](int i) {
					const CServerInfo *pInfo = pBrowser->SortedGet(i);
					SBox Thumb;
					Thumb.m_W = Px(48.0f);
					Thumb.m_H = Px(36.0f);
					PaintThumb(*this, Row(Thumb), pInfo->m_aMap, 9.0f);
					SBox Names = GrowBox();
					Names.m_Gap = 2.0f;
					Column(Names, [&]() {
						SBox Line = GrowBox();
						Line.m_Gap = 6.0f;
						Line.m_Cross = EAlign::CENTER;
						Row(Line, [&]() {
							Text(pInfo->m_aMap, Clip(14.5f, EFont::BOLD, XuiTheme::Color::TEXT));
							if(pInfo->m_Favorite != TRISTATE::NONE)
								Icon(Ph::STAR, 11.0f, XuiTheme::Color::YELLOW, true);
							if(pInfo->m_Flags & SERVER_FLAG_PASSWORD)
								Icon(Ph::LOCK_SIMPLE, 11.0f, XuiTheme::Color::TEXT3, true);
						});
						Text(pInfo->m_aName, Clip(12.0f, EFont::REGULAR, XuiTheme::Color::TEXT3), GrowBox());
					});
					Text(pInfo->m_aGameType, Clip(12.5f, EFont::MEDIUM, XuiTheme::Color::TEXT2), FixedBox(110.0f));
					{
						SBox PB = FixedBox(56.0f);
						PB.m_Gap = 6.0f;
						PB.m_Cross = EAlign::CENTER;
						PB.m_Main = EAlign::END;
						const ColorRGBA PC = PingColor(pInfo->m_Latency);
						Row(PB, [&]() {
							SBox Dot;
							Dot.m_W = Px(6.0f);
							Dot.m_H = Px(6.0f);
							Paint(Row(Dot), [PC](CXui &U, const SRect &Rc) { U.DrawRect(Rc, SolidRect(PC, 3.0f)); });
							char aPing[16];
							str_format(aPing, sizeof(aPing), "%d", pInfo->m_Latency);
							STextStyle PingStyle = Clip(12.5f, EFont::SEMIBOLD, XuiTheme::Color::TEXT2);
							PingStyle.m_Tabular = true;
							Text(aPing, PingStyle);
						});
					}
					{
						const int Players = pBrowser->Players(*pInfo);
						const int Max = pBrowser->Max(*pInfo);
						const float Fill = Max > 0 ? std::clamp((float)Players / Max, 0.0f, 1.0f) : 0.0f;
						SBox PB = FixedBox(76.0f);
						PB.m_Gap = 5.0f;
						PB.m_Cross = EAlign::END;
						Column(PB, [&]() {
							char aPlayers[16];
							str_format(aPlayers, sizeof(aPlayers), "%d/%d", Players, Max);
							STextStyle PlayersStyle = Clip(13.0f, EFont::BOLD, Fill >= 1.0f ? XuiTheme::Color::RED_SOFT : (Players > 0 ? XuiTheme::Color::TEXT : XuiTheme::Color::TEXT3));
							PlayersStyle.m_Align = ETextAlign::RIGHT;
							PlayersStyle.m_Tabular = true;
							Text(aPlayers, PlayersStyle);
							SBox Bar;
							Bar.m_W = Px(56.0f);
							Bar.m_H = Px(3.0f);
							const ColorRGBA BarColor = Fill >= 1.0f ? XuiTheme::Color::RED_SOFT : ColorRGBA(1, 1, 1, 0.45f);
							Paint(Row(Bar), [Fill, BarColor](CXui &U, const SRect &Rc) {
								U.DrawRect(Rc, SolidRect(ColorRGBA(1, 1, 1, 0.08f), 1.5f));
								if(Fill > 0.0f)
									U.DrawRect({Rc.m_X + Rc.m_W * (1.0f - Fill), Rc.m_Y, Rc.m_W * Fill, Rc.m_H}, SolidRect(BarColor, 1.5f));
							});
						});
					}
				},
					&Clicked, [&](int i) { return pBrowser->SortedGet(i) == pSelected; }, &Activated, &RightClicked, 14.0f);
				if(Clicked >= 0)
					str_copy(g_Config.m_UiServerAddress, pBrowser->SortedGet(Clicked)->m_aAddress);
				if(Activated >= 0)
					M.Connect(pBrowser->SortedGet(Activated)->m_aAddress);
				if(RightClicked >= 0)
				{
					s_ContextIndex = RightClicked;
					s_ContextOpen = true;
					s_ContextAt = Mouse();
					str_copy(g_Config.m_UiServerAddress, pBrowser->SortedGet(RightClicked)->m_aAddress);
				}
			}); }, 20.0f);

		SBox Detail;
		Detail.m_W = Px(340.0f);
		Detail.m_H = Grow();
		Detail.m_Padding = SEdges(20.0f);
		Detail.m_Gap = 14.0f;
		Detail.m_Clip = true;
		GlassPanel(Detail, [&]() {
			if(pSelected == nullptr)
			{
				SBox Empty;
				Empty.m_W = Grow();
				Empty.m_H = Grow();
				Empty.m_Main = EAlign::CENTER;
				Empty.m_Cross = EAlign::CENTER;
				Empty.m_Gap = 10.0f;
				Column(Empty, [&]() {
					Icon(Ph::HARD_DRIVES, 34.0f, XuiTheme::Color::TEXT3);
					STextStyle S;
					S.m_Size = 13.5f;
					S.m_Color = XuiTheme::Color::TEXT3;
					Text(Localize("Select a server"), S);
				});
				return;
			}
			SBox Thumb;
			Thumb.m_W = Grow();
			Thumb.m_H = Px(176.0f);
			Thumb.m_Padding = SEdges(10.0f);
			Thumb.m_Cross = EAlign::END;
			const int ThumbNode = Row(Thumb, [&]() {
				SBox Pill;
				Pill.m_H = Px(28.0f);
				Pill.m_Padding = SEdges(0.0f, 12.0f);
				Pill.m_Cross = EAlign::CENTER;
				Pill.m_Gap = 6.0f;
				const int PillNode = Row(Pill, [&]() {
					const CCommunity *pCommunity = pBrowser->Community(pSelected->m_aCommunityId);
					if(pCommunity != nullptr && M.CommunityIcons().Find(pCommunity->Id()) != nullptr)
						CommunityIcon(*this, M, pCommunity->Id(), 36.0f, 18.0f);
					STextStyle S;
					S.m_Size = 12.0f;
					S.m_Font = EFont::BOLD;
					Text(pSelected->m_aGameType, S);
				});
				Paint(PillNode, [](CXui &U, const SRect &Rc) {
					U.DrawRect(Rc, GlassRect(Rc.m_H * 0.5f));
					U.DrawRect(Rc, GlassHairline(Rc.m_H * 0.5f));
				});
				Spacer();
			});
			PaintThumb(*this, ThumbNode, pSelected->m_aMap, 24.0f);

			SBox Names;
			Names.m_W = Grow();
			Names.m_Gap = 4.0f;
			Names.m_Padding = SEdges(0.0f, 10.0f);
			Column(Names, [&]() {
				STextStyle MapStyle = Clip(26.0f, EFont::EXTRABOLD, XuiTheme::Color::TEXT);
				MapStyle.m_Tracking = -0.03f;
				Text(pSelected->m_aMap, MapStyle, GrowBox());
				STextStyle NameStyle;
				NameStyle.m_Size = 13.0f;
				NameStyle.m_Color = XuiTheme::Color::TEXT2;
				NameStyle.m_Wrap = true;
				NameStyle.m_MaxLines = 2;
				Text(pSelected->m_aName, NameStyle, GrowBox());
			});

			SBox Addr;
			Addr.m_W = Grow();
			Addr.m_H = Px(40.0f);
			Addr.m_Gap = 8.0f;
			Addr.m_Cross = EAlign::CENTER;
			Addr.m_Padding = SEdges(0.0f, 6.0f, 0.0f, 14.0f);
			const int AddrNode = Row(Addr, [&]() {
				STextStyle A = Clip(13.0f, EFont::SEMIBOLD, XuiTheme::Color::TEXT);
				A.m_Tabular = true;
				Text(pSelected->m_aAddress, A, GrowBox());
				char aPing[16];
				str_format(aPing, sizeof(aPing), "%d ms", pSelected->m_Latency);
				STextStyle P;
				P.m_Size = 12.0f;
				P.m_Font = EFont::SEMIBOLD;
				P.m_Color = PingColor(pSelected->m_Latency);
				Text(aPing, P);
				if(Button(Localize("Copy"), {.m_Variant = EVariant::GLASS, .m_Height = 30.0f, .m_PadX = 12.0f, .m_FontSize = 12.0f, .m_pKey = "br.copy"}).m_Clicked)
				{
					Input()->SetClipboardText(pSelected->m_aAddress);
					ShowToast(Localize("Address copied"), Ph::COPY);
				}
			});
			Paint(AddrNode, [](CXui &U, const SRect &Rc) { U.DrawRect(Rc, SolidRect(XuiTheme::Color::GHOST, Rc.m_H * 0.5f)); });

			char aPlayersHead[64];
			str_format(aPlayersHead, sizeof(aPlayersHead), Localize("%d players"), pSelected->m_NumClients);
			SBox PlayersBox;
			PlayersBox.m_W = Grow();
			PlayersBox.m_H = Grow();
			PlayersBox.m_Gap = 2.0f;
			PlayersBox.m_Padding = SEdges(0.0f, 6.0f);
			Scroll(PlayersBox, [&]() {
				STextStyle H;
				H.m_Size = 12.0f;
				H.m_Font = EFont::SEMIBOLD;
				H.m_Color = XuiTheme::Color::TEXT3;
				SBox HB;
				HB.m_Padding = SEdges(4.0f, 8.0f, 6.0f, 8.0f);
				Row(HB, [&]() { Text(aPlayersHead, H); });
				for(size_t i = 0; i < pSelected->m_vClients.size(); i++)
				{
					const CServerInfo::CClient &C = pSelected->m_vClients[i];
					SBox R;
					R.m_W = Grow();
					R.m_Gap = 10.0f;
					R.m_Cross = EAlign::CENTER;
					R.m_Padding = SEdges(4.0f, 8.0f);
					Row(R, [&]() {
						TeeAvatar(C.m_aSkin, 28.0f);
						const bool Friend = C.m_FriendState == IFriends::FRIEND_PLAYER;
						Text(C.m_aName, Clip(13.5f, EFont::SEMIBOLD, Friend ? XuiTheme::Color::GREEN : (C.m_Player ? XuiTheme::Color::TEXT : XuiTheme::Color::TEXT3)), GrowBox());
						if(C.m_aClan[0] != '\0')
						{
							SBox CB;
							CB.m_W = Fit(0.0f, 110.0f);
							Text(C.m_aClan, Clip(12.0f, EFont::REGULAR, XuiTheme::Color::TEXT3), CB);
						}
					});
				}
			});

			SBox Actions;
			Actions.m_W = Grow();
			Actions.m_Gap = 10.0f;
			Actions.m_Cross = EAlign::CENTER;
			Row(Actions, [&]() {
				const bool Fav = pSelected->m_Favorite != TRISTATE::NONE;
				const SResult Favorite = IconButton(Ph::STAR, {.m_Variant = EVariant::GLASS, .m_Height = 48.0f, .m_IconFill = Fav, .m_pKey = "br.selfav"});
				Tooltip(Favorite.m_Id, Localize(Fav ? "Remove from favorites" : "Add to favorites"));
				if(Favorite.m_Clicked)
				{
					QueueFavorite(pSelected, Fav);
				}
				if(Button(Localize("Join"), {.m_Variant = EVariant::ACCENT, .m_pIcon = Ph::PLAY, .m_Height = 48.0f, .m_FontSize = 16.0f, .m_IconFill = true, .m_W = Grow(), .m_pKey = "br.join"}).m_Clicked)
					M.Connect(pSelected->m_aAddress);
			}); }, 22.0f);
	});

	if(s_aCommunityCtx[0] != '\0')
	{
		IFilterList &FavFilter = pBrowser->FavoriteCommunitiesFilter();
		const bool Pinned = FavFilter.Filtered(s_aCommunityCtx);
		const bool Full = pBrowser->FavoriteCommunities().size() >= 5;
		const SMenuItem aItems[] = {
			{Pinned ? Localize("Unpin tab") : Localize("Pin as tab"), Ph::PUSH_PIN, false, !Pinned && Full},
		};
		int Chosen = -1;
		const CCommunity *pCommunity = pBrowser->Community(s_aCommunityCtx);
		ContextMenu("br.cctx", pCommunity ? pCommunity->Name() : s_aCommunityCtx, aItems, 1, &s_CommunityCtxOpen, s_CommunityCtxAt, &Chosen);
		if(Chosen == 0)
		{
			char aId[CServerInfo::MAX_COMMUNITY_ID_LENGTH];
			str_copy(aId, s_aCommunityCtx);
			s_aCommunityCtx[0] = '\0';
			if(Pinned)
			{
				FavFilter.Remove(aId);
				if(Page >= CMenus::PAGE_FAVORITE_COMMUNITY_1 && Page <= CMenus::PAGE_FAVORITE_COMMUNITY_5)
					M.SetMenuPage(CMenus::PAGE_INTERNET);
			}
			else
				FavFilter.Add(aId);
			M.UpdateCommunityCache(true);
			return;
		}
		if(!s_CommunityCtxOpen)
			s_aCommunityCtx[0] = '\0';
	}

	if(s_FavNum > 0)
	{
		if(s_FavRemove)
			Favorites()->Remove(s_aFavAddr, s_FavNum);
		else
			Favorites()->Add(s_aFavAddr, s_FavNum);
		s_FavNum = 0;
		Update();
		return;
	}

	if(s_PageClick != -1)
	{
		const int NewPage = s_PageClick;
		s_PageClick = -1;
		if(NewPage != Page)
		{
			s_ContextIndex = -1;
			s_ContextOpen = false;
			M.SetMenuPage(NewPage);
			return;
		}
	}

	if(s_CommunityClick != -1)
	{
		const int Click = s_CommunityClick;
		s_CommunityClick = -1;
		IFilterList &Filter = pBrowser->CommunitiesFilter();
		std::vector<std::string> vIds;
		for(const CCommunity &Community : pBrowser->Communities())
			vIds.emplace_back(Community.Id());
		if(Click == -3)
		{
			M.ResetServerbrowserFilters();
			return;
		}
		if(Click == -2)
			Filter.Clear();
		else if(Click >= 0 && Click < (int)vIds.size() && !s_CommunityMulti)
		{
			const char *pId = vIds[Click].c_str();
			int NumSelected = 0;
			for(const std::string &Id : vIds)
				NumSelected += Filter.Empty() || !Filter.Filtered(Id.c_str());
			const bool OnlyThis = !Filter.Empty() && !Filter.Filtered(pId) && NumSelected == 1;
			Filter.Clear();
			if(!OnlyThis)
				for(const std::string &Id : vIds)
					if(Id != pId)
						Filter.Add(Id.c_str());
		}
		else if(Click >= 0 && Click < (int)vIds.size())
		{
			const char *pId = vIds[Click].c_str();
			if(Filter.Empty())
			{
				for(const std::string &Id : vIds)
					if(Id != pId)
						Filter.Add(Id.c_str());
			}
			else if(Filter.Filtered(pId))
				Filter.Remove(pId);
			else
			{
				Filter.Add(pId);
				bool AnyLeft = false;
				for(const std::string &Id : vIds)
					AnyLeft = AnyLeft || !Filter.Filtered(Id.c_str());
				if(!AnyLeft)
					Filter.Clear();
			}
		}
		M.UpdateCommunityCache(true);
		Update();
	}

	if(s_ContextIndex >= 0 && s_ContextIndex < NumServers)
	{
		const CServerInfo *pInfo = pBrowser->SortedGet(s_ContextIndex);
		const bool Fav = pInfo->m_Favorite != TRISTATE::NONE;
		const SMenuItem aItems[] = {
			{Localize("Join"), Ph::PLAY},
			{Localize("Copy address"), Ph::COPY},
			{Fav ? Localize("Remove from favorites") : Localize("Add to favorites"), Ph::STAR},
		};
		int Chosen = -1;
		ContextMenu("br.ctx", pInfo->m_aMap, aItems, (int)std::size(aItems), &s_ContextOpen, s_ContextAt, &Chosen);
		if(Chosen == 0)
			M.Connect(pInfo->m_aAddress);
		else if(Chosen == 1)
		{
			Input()->SetClipboardText(pInfo->m_aAddress);
			ShowToast(Localize("Address copied"), Ph::COPY);
		}
		else if(Chosen == 2)
		{
			QueueFavorite(pInfo, Fav);
		}
		if(!s_ContextOpen)
			s_ContextIndex = -1;
	}

	if(KeyPressed(KEY_F5) || s_RefreshClick)
	{
		s_RefreshClick = false;
		s_ContextIndex = -1;
		M.RefreshBrowserTab(true);
	}
}
