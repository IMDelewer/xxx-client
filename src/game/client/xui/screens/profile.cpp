#include <base/str.h>
#include <base/time.h>

#include <engine/external/json-parser/json.h>
#include <engine/font_icons_phosphor.h>
#include <engine/friends.h>
#include <engine/http.h>
#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/xui/screens/settings.h>
#include <game/client/xui/xui.h>
#include <game/localization.h>

#include <algorithm>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

using namespace Xui;

namespace
{
	struct SFinish
	{
		std::string m_Map;
		std::string m_Type;
		double m_Time = 0.0;
		int64_t m_Timestamp = 0;
	};

	struct SPartner
	{
		std::string m_Name;
		int m_Finishes = 0;
	};

	struct SCategory
	{
		std::string m_Name;
		int m_Points = 0;
		int m_Total = 0;
		int m_Rank = 0;
	};

	struct SProfile
	{
		enum class EState
		{
			NONE,
			LOADING,
			READY,
			MISSING,
			ERROR,
		};
		EState m_State = EState::NONE;
		std::string m_Name;
		int m_Points = 0;
		int m_TotalPoints = 0;
		int m_Rank = 0;
		int m_TeamRank = 0;
		int m_SoloRank = 0;
		int m_Hours = 0;
		int m_TotalHours = -1;
		bool m_DdstatsDone = false;
		int m_DdstatsAttempts = 0;
		int64_t m_DdstatsRetryAt = 0;
		std::vector<std::pair<std::string, int>> m_vPlayedMaps;
		std::shared_ptr<IHttpRequest> m_pDdstats;
		int m_LastYear = 0;
		std::string m_Server;
		std::vector<SFinish> m_vFinishes;
		std::vector<SPartner> m_vPartners;
		std::vector<SCategory> m_vCategories;
		std::vector<std::pair<std::string, int>> m_vActivity;
		std::shared_ptr<IHttpRequest> m_pRequest;
	};

	SProfile gs_Profile;

	double Num(const json_value &Value)
	{
		if(Value.type == json_integer)
			return (double)Value.u.integer;
		if(Value.type == json_double)
			return Value.u.dbl;
		return 0.0;
	}

	const char *Str(const json_value &Value)
	{
		return Value.type == json_string ? (const char *)Value : "";
	}

	void Parse(const json_value &Root)
	{
		SProfile &P = gs_Profile;
		P.m_Points = (int)Num(Root["points"]["points"]);
		P.m_TotalPoints = (int)Num(Root["points"]["total"]);
		P.m_Rank = (int)Num(Root["points"]["rank"]);
		P.m_TeamRank = (int)Num(Root["team_rank"]["rank"]);
		P.m_SoloRank = (int)Num(Root["rank"]["rank"]);
		P.m_Hours = (int)Num(Root["hours_played_past_365_days"]);
		P.m_LastYear = (int)Num(Root["points_last_year"]["points"]);
		P.m_Server = Str(Root["favorite_server"]["server"]);
		P.m_vFinishes.clear();
		const json_value &Finishes = Root["last_finishes"];
		if(Finishes.type == json_array)
			for(unsigned i = 0; i < Finishes.u.array.length; i++)
			{
				const json_value &F = Finishes[i];
				P.m_vFinishes.push_back({Str(F["map"]), Str(F["type"]), Num(F["time"]), (int64_t)Num(F["timestamp"])});
			}
		P.m_vPartners.clear();
		const json_value &Partners = Root["favorite_partners"];
		if(Partners.type == json_array)
			for(unsigned i = 0; i < Partners.u.array.length && i < 5; i++)
				P.m_vPartners.push_back({Str(Partners[i]["name"]), (int)Num(Partners[i]["finishes"])});
		P.m_vCategories.clear();
		const json_value &Types = Root["types"];
		if(Types.type == json_object)
			for(unsigned i = 0; i < Types.u.object.length; i++)
			{
				const json_value &T = *Types.u.object.values[i].value;
				P.m_vCategories.push_back({Types.u.object.values[i].name, (int)Num(T["points"]["points"]), (int)Num(T["points"]["total"]), (int)Num(T["points"]["rank"])});
			}
		std::sort(P.m_vCategories.begin(), P.m_vCategories.end(), [](const SCategory &A, const SCategory &B) { return A.m_Points > B.m_Points; });
		P.m_vActivity.clear();
		const json_value &Activity = Root["activity"];
		if(Activity.type == json_array)
			for(unsigned i = 0; i < Activity.u.array.length; i++)
				P.m_vActivity.emplace_back(Str(Activity[i]["date"]), (int)Num(Activity[i]["hours_played"]));
	}

	STextStyle Style(float Size, EFont Font, ColorRGBA Color)
	{
		STextStyle S;
		S.m_Size = Size;
		S.m_Font = Font;
		S.m_Color = Color;
		return S;
	}

}

void CXui::RenderProfile(CMenus &M)
{
	SProfile &P = gs_Profile;
	const char *pName = m_ProfileOpen ? m_ProfileName.c_str() : Client()->PlayerName();
	const bool Own = str_comp(pName, Client()->PlayerName()) == 0;
	if(P.m_Name != pName)
	{
		if(P.m_pRequest)
			P.m_pRequest->Abort();
		if(P.m_pDdstats)
			P.m_pDdstats->Abort();
		P = SProfile();
		P.m_Name = pName;
	}
	if(P.m_State == SProfile::EState::NONE)
	{
		char aEscaped[256];
		EscapeUrl(aEscaped, pName);
		char aUrl[512];
		str_format(aUrl, sizeof(aUrl), "https://ddnet.org/players/?json2=%s", aEscaped);
		P.m_pRequest = HttpGet(aUrl);
		P.m_pRequest->Timeout(CTimeout{10000, 0, 500, 10});
		P.m_pRequest->LogProgress(HTTPLOG::FAILURE);
		Http()->Run(P.m_pRequest);
		P.m_State = SProfile::EState::LOADING;
	}
	if(P.m_State == SProfile::EState::LOADING && P.m_pRequest && P.m_pRequest->State() != EHttpState::QUEUED && P.m_pRequest->State() != EHttpState::RUNNING)
	{
		if(P.m_pRequest->State() == EHttpState::DONE)
		{
			json_value *pJson = P.m_pRequest->ResultJson();
			if(pJson != nullptr && pJson->type == json_object && (*pJson)["player"].type == json_string)
			{
				Parse(*pJson);
				P.m_State = SProfile::EState::READY;
			}
			else
				P.m_State = SProfile::EState::MISSING;
			if(pJson != nullptr)
				json_value_free(pJson);
		}
		else
			P.m_State = SProfile::EState::ERROR;
		P.m_pRequest = nullptr;
	}

	if(!P.m_DdstatsDone && !P.m_pDdstats && time_get() >= P.m_DdstatsRetryAt)
	{
		char aEscaped[256], aUrl[512];
		EscapeUrl(aEscaped, pName);
		str_format(aUrl, sizeof(aUrl), "https://ddstats.tw/player/json?player=%s", aEscaped);
		++P.m_DdstatsAttempts;
		P.m_pDdstats = HttpGet(aUrl);
		P.m_pDdstats->FailOnErrorStatus(false);
		P.m_pDdstats->Timeout(CTimeout{10000, 0, 500, 10});
		P.m_pDdstats->MaxResponseSize(8 * 1024 * 1024);
		P.m_pDdstats->LogProgress(HTTPLOG::FAILURE);
		Http()->Run(P.m_pDdstats);
	}
	if(P.m_pDdstats && P.m_pDdstats->State() != EHttpState::QUEUED && P.m_pDdstats->State() != EHttpState::RUNNING)
	{
		const bool Completed = P.m_pDdstats->State() == EHttpState::DONE;
		const int Status = Completed ? P.m_pDdstats->StatusCode() : 0;
		if(Completed && Status == 200)
		{
			json_value *pJson = P.m_pDdstats->ResultJson();
			if(pJson && pJson->type == json_object)
			{
				const json_value &Seconds = (*pJson)["general_activity"]["total_seconds_played"];
				if((Seconds.type == json_integer || Seconds.type == json_double) && Num(Seconds) >= 0)
					P.m_TotalHours = (int)std::min(Num(Seconds) / 3600.0, 100000000.0);
				P.m_vPlayedMaps.clear();
				const json_value &Maps = (*pJson)["most_played_maps"];
				if(Maps.type == json_array)
					for(unsigned i = 0; i < std::min(Maps.u.array.length, 5u); ++i)
						P.m_vPlayedMaps.emplace_back(Str(Maps[i]["map_name"]), (int)(Num(Maps[i]["seconds_played"]) / 3600.0));
			}
			if(pJson)
				json_value_free(pJson);
		}
		P.m_DdstatsDone = P.m_TotalHours >= 0 || P.m_DdstatsAttempts >= 2 || Status == 404;
		P.m_DdstatsRetryAt = time_get() + time_freq() * 2;
		P.m_pDdstats = nullptr;
	}

	auto Card = [&](const SBox &Box, const std::function<void()> &Content, float Radius = 18.0f) {
		const int Node = Column(Box, Content);
		Paint(Node, [Radius](CXui &U, const SRect &Rc) { U.DrawRect(Rc, HomePanel(Radius)); });
	};
	PushId(MakeId(pName));
	const bool Ready = P.m_State == SProfile::EState::READY;
	SBox Page;
	Page.m_W = Grow();
	Page.m_H = Grow();
	Page.m_Gap = 20.0f;
	Row(Page, [&]() {
		SBox Left;
		Left.m_W = Grow();
		Left.m_Shrink = true;
		Left.m_Gap = 16.0f;
		Left.m_H = Grow();
		Left.m_Cross = EAlign::STRETCH;
		Left.m_Scroll = EScroll::VERTICAL;
		Left.m_Hit = EHit::BLOCK;
		Left.m_pKey = "profile.details";
		Left.m_Padding.m_R = XuiTheme::Size::SCROLLBAR_W + 8.0f;
		Column(Left, [&]() {
			SBox Head;
			Head.m_Gap = 18.0f;
			Head.m_W = Grow();
			Head.m_Padding = SEdges(20.0f);
			Head.m_Wrap = true;
			Head.m_Cross = EAlign::CENTER;
			const int HeadNode = Row(Head, [&]() {
				SBox TeeBox;
				TeeBox.m_W = Px(104.0f);
				TeeBox.m_H = Px(104.0f);
				CTeeRenderInfo Info;
				const CSkin *pSkin = GameClient()->m_Skins.Find(g_Config.m_ClPlayerSkin);
				if(pSkin != nullptr)
					Info.Apply(pSkin);
				Info.ApplyColors(g_Config.m_ClPlayerUseCustomColor, g_Config.m_ClPlayerColorBody, g_Config.m_ClPlayerColorFeet);
				if(m_ProfileOpen && m_ProfileTee)
					Info = m_ProfileTee->TeeRenderInfo();
				else if(!Own)
					for(const auto &Data : GameClient()->m_aClients)
						if(Data.m_Active && str_comp(Data.m_aName, pName) == 0)
						{
							Info = Data.m_RenderInfo;
							break;
						}
				const int Emote = Own ? g_Config.m_ClPlayerDefaultEyes : EMOTE_NORMAL;
				Paint(Row(TeeBox), [Info, Emote](CXui &U, const SRect &Rc) {
					const vec2 Delta = U.Mouse() - vec2(Rc.CenterX(), Rc.CenterY());
					U.DrawTee(Info, Emote, length(Delta) < 1.0f ? vec2(1.0f, 0.0f) : normalize(Delta), Rc);
				});
				SBox Names;
				Names.m_Gap = 4.0f;
				Names.m_W = Grow();
				Names.m_Shrink = true;
				Column(Names, [&]() {
					STextStyle N = Style(38.0f, EFont::EXTRABOLD, XuiTheme::Color::TEXT);
					N.m_Tracking = -0.04f;
					N.m_Wrap = true;
					N.m_MaxLines = 2;
					Text(pName, N, {.m_W = Grow()});
					char aLine[256];
					if(Ready)
						str_format(aLine, sizeof(aLine), Localize("Rank #%d · %d points · plays on %s"), P.m_Rank, P.m_Points, P.m_Server.empty() ? "—" : P.m_Server.c_str());
					else if(P.m_State == SProfile::EState::LOADING)
						str_copy(aLine, Localize("Loading stats from ddnet.org…"));
					else if(P.m_State == SProfile::EState::MISSING)
						str_copy(aLine, Localize("No finishes on ddnet.org yet"));
					else
						str_copy(aLine, Localize("Could not reach ddnet.org"));
					STextStyle L = Style(15.0f, EFont::REGULAR, XuiTheme::Color::TEXT2);
					L.m_Tabular = true;
					L.m_Wrap = true;
					Text(aLine, L, {.m_W = Grow()});
				});
				Spacer();
				if(Own && Button(Localize("Edit profile"), {.m_Variant = EVariant::GLASS, .m_pIcon = Ph::PENCIL_SIMPLE, .m_pKey = "pr.edit"}).m_Clicked)
				{
					SettingsSelectNav(2);
					m_ShellProfile = false;
					m_ShellHome = false;
					M.SetMenuPage(CMenus::PAGE_SETTINGS);
					if(m_ProfileOpen)
					{
						m_ProfileOpen = false;
						M.m_GamePage = CMenus::PAGE_SETTINGS;
						M.SetActive(true);
					}
				}
			});
			Paint(HeadNode, [](CXui &U, const SRect &Rc) { U.DrawRect(Rc, HomePanel(24.0f)); });

			if(P.m_TotalHours < 0)
			{
				Text(Localize(P.m_DdstatsDone ? "DDStats unavailable" : "Loading DDStats…"), Style(13.0f, EFont::REGULAR, XuiTheme::Color::TEXT2));
				if(P.m_DdstatsDone && Button(Localize("Retry DDStats"), {.m_Variant = EVariant::GHOST, .m_pKey = "profile.retry.ddstats"}).m_Clicked)
				{
					P.m_DdstatsAttempts = 0;
					P.m_DdstatsDone = false;
					P.m_DdstatsRetryAt = 0;
				}
			}

			if(!Ready && P.m_TotalHours < 0)
			{
				if(P.m_State == SProfile::EState::LOADING)
					Spinner(28.0f);
				return;
			}

			SBox Stats;
			Stats.m_Gap = 10.0f;
			Stats.m_W = Grow();
			Stats.m_Wrap = true;
			Row(Stats, [&]() {
				struct SStat
				{
					const char *m_pLabel;
					int m_Value;
					bool m_Rank;
				};
				const SStat aStats[] = {
					{"Points", P.m_Points, false},
					{"Team rank", P.m_TeamRank, true},
					{"Solo rank", P.m_SoloRank, true},
					{"Hours · DDStats", P.m_TotalHours, false},
					{"Hours this year · DDNet", P.m_Hours, false},
					{"Points this year", P.m_LastYear, false},
				};
				for(const SStat &Stat : aStats)
				{
					SBox C;
					C.m_Gap = 4.0f;
					C.m_W = Fit(130.0f);
					C.m_Padding = SEdges(16.0f);
					Card(C, [&]() {
						Text(Localize(Stat.m_pLabel), Style(12.0f, EFont::SEMIBOLD, XuiTheme::Color::TEXT3));
						char aValue[32];
						if(Stat.m_Value < 0 || (Stat.m_Rank && Stat.m_Value == 0))
							str_copy(aValue, "—");
						else
							str_format(aValue, sizeof(aValue), Stat.m_Rank ? "#%d" : "%d", Stat.m_Value);
						STextStyle V = Style(24.0f, EFont::BOLD, XuiTheme::Color::TEXT);
						V.m_Tabular = true;
						Text(aValue, V);
					});
				}
			});

			if(!P.m_vPlayedMaps.empty())
			{
				const int PlayedNode = Group([&]() {
					GroupRow([&]() { Text(Localize("Most played maps · DDStats"), Style(13.0f, EFont::BOLD, XuiTheme::Color::TEXT2)); }, true);
					for(const auto &[Map, Hours] : P.m_vPlayedMaps)
						GroupRow([&]() {
							Text(Map.c_str(), Style(13.0f, EFont::REGULAR, XuiTheme::Color::TEXT), {.m_W = Grow()});
							char aHours[32];
							str_format(aHours, sizeof(aHours), "%d h", Hours);
							Text(aHours, Style(13.0f, EFont::MEDIUM, XuiTheme::Color::TEXT2));
						},
							false);
				});

				Paint(PlayedNode, [](CXui &U, const SRect &Rc) { U.DrawRect(Rc, HomePanel(22.0f)); });
			}

			SBox ActivityBox;
			ActivityBox.m_Gap = 10.0f;
			ActivityBox.m_W = Grow();
			ActivityBox.m_Padding = SEdges(20.0f);
			Card(ActivityBox, [&]() {
				Text(Localize("Activity · last 26 weeks · from ddnet.org"), Style(12.0f, EFont::SEMIBOLD, XuiTheme::Color::TEXT3));
				constexpr int Weeks = 26;
				int aHours[Weeks * 7] = {};
				const time_t Now = ::time(nullptr);
				char aWeekday[2] = {};
				str_timestamp_ex(Now, aWeekday, sizeof(aWeekday), "%w");
				const int TodayIndex = Weeks * 7 - 7 + str_toint(aWeekday);
				for(const auto &[Date, Hours] : P.m_vActivity)
				{
					tm Day = {};
					if(sscanf(Date.c_str(), "%d-%d-%d", &Day.tm_year, &Day.tm_mon, &Day.tm_mday) != 3)
						continue;
					Day.tm_year -= 1900;
					Day.tm_mon -= 1;
					Day.tm_hour = 12;
					const time_t T = mktime(&Day);
					const int DaysAgo = (int)((Now - T) / 86400);
					const int Index = TodayIndex - DaysAgo;
					if(Index >= 0 && Index < Weeks * 7)
						aHours[Index] = Hours;
				}
				SBox Grid;
				Grid.m_W = Px(Weeks * 16.0f);
				Grid.m_H = Px(7 * 16.0f);
				const ColorRGBA Acc = m_Accent;
				std::vector<int> vHours(aHours, aHours + Weeks * 7);
				Paint(Row(Grid), [vHours, Acc, TodayIndex](CXui &U, const SRect &Rc) {
					for(int i = 0; i < (int)vHours.size(); i++)
					{
						const int Week = i / 7;
						const int Day = i % 7;
						const float A = vHours[i] <= 0 ? 0.0f : std::min(1.0f, 0.25f + vHours[i] / 8.0f);
						const ColorRGBA C = i > TodayIndex ? ColorRGBA(0, 0, 0, 0) : (A <= 0.0f ? ColorRGBA(1, 1, 1, 0.07f) : Acc.WithAlpha(A));
						U.DrawRect({Rc.m_X + Week * 16.0f, Rc.m_Y + Day * 16.0f, 13.0f, 13.0f}, SolidRect(C, 3.0f));
					}
				});
			});

			SBox Categories;
			Categories.m_Gap = 10.0f;
			Categories.m_W = Grow();
			Categories.m_Padding = SEdges(20.0f);
			Card(Categories, [&]() {
				Text(Localize("Points by category"), Style(12.0f, EFont::SEMIBOLD, XuiTheme::Color::TEXT3));
				int Shown = 0;
				for(const SCategory &Cat : P.m_vCategories)
				{
					if(Cat.m_Points <= 0 || Shown++ >= 7)
						continue;
					SBox R;
					R.m_W = Grow();
					R.m_Gap = 14.0f;
					R.m_Cross = EAlign::CENTER;
					Row(R, [&]() {
						SBox NB;
						NB.m_W = Px(110.0f);
						Text(Cat.m_Name.c_str(), Style(13.5f, EFont::SEMIBOLD, XuiTheme::Color::TEXT), NB);
						SBox Bar;
						Bar.m_W = Grow();
						Bar.m_H = Px(6.0f);
						const float Pct = Cat.m_Total > 0 ? std::clamp((float)Cat.m_Points / Cat.m_Total, 0.02f, 1.0f) : 0.0f;
						const ColorRGBA Acc = m_Accent;
						Paint(Row(Bar), [Pct, Acc](CXui &U, const SRect &Rc) {
							U.DrawRect(Rc, SolidRect(ColorRGBA(1, 1, 1, 0.08f), 3.0f));
							U.DrawRect({Rc.m_X, Rc.m_Y, Rc.m_W * Pct, Rc.m_H}, SolidRect(Acc, 3.0f));
						});
						char aMeta[64];
						str_format(aMeta, sizeof(aMeta), "%d / %d · #%d", Cat.m_Points, Cat.m_Total, Cat.m_Rank);
						STextStyle MS = Style(12.5f, EFont::REGULAR, XuiTheme::Color::TEXT3);
						MS.m_Tabular = true;
						MS.m_Align = ETextAlign::RIGHT;
						SBox MB;
						MB.m_W = Px(150.0f);
						Text(aMeta, MS, MB);
					});
				}
			});
		});

		if(!Ready)
			return;
		SBox Side;
		Side.m_W = Px(380.0f);
		Side.m_Padding = SEdges(16.0f, 10.0f, 10.0f, 10.0f);
		Side.m_Gap = 2.0f;
		Side.m_Self = EAlign::START;
		Side.m_H = Grow();
		Side.m_Scroll = EScroll::VERTICAL;
		Side.m_Hit = EHit::BLOCK;
		Side.m_pKey = "profile.finishes";
		Card(Side, [&]() {
			SBox H;
			H.m_Padding = SEdges(0.0f, 8.0f, 6.0f, 8.0f);
			Row(H, [&]() { Text(Localize("Last finishes"), Style(12.0f, EFont::SEMIBOLD, XuiTheme::Color::TEXT3)); });
			for(size_t i = 0; i < P.m_vFinishes.size() && i < 8; i++)
			{
				const SFinish &F = P.m_vFinishes[i];
				SBox R;
				R.m_W = Grow();
				R.m_Gap = 12.0f;
				R.m_Cross = EAlign::CENTER;
				R.m_Padding = SEdges(7.0f, 8.0f);
				Row(R, [&]() {
					SBox Thumb;
					Thumb.m_W = Px(52.0f);
					Thumb.m_H = Px(32.0f);
					const auto Preview = GameClient()->m_MapPreviews.Artwork(F.m_Map.c_str());
					Paint(Row(Thumb), [Preview](CXui &U, const SRect &Rc) {
						if(Preview.Valid())
							U.DrawImage(Rc, Preview.m_Texture, vec2(Preview.m_Size.x, Preview.m_Size.y), 8.0f);
						else
							U.DrawRect(Rc, SolidRect(XuiTheme::Color::GROUP_FILL, 8.0f));
					});
					SBox Col;
					Col.m_W = Grow();
					Col.m_Shrink = true;
					Column(Col, [&]() {
						STextStyle MapStyle = Style(14.0f, EFont::BOLD, XuiTheme::Color::TEXT);
						MapStyle.m_Wrap = true;
						MapStyle.m_MaxLines = 1;
						SBox GB;
						GB.m_W = Grow();
						Text(F.m_Map.c_str(), MapStyle, GB);
						char aDate[32];
						str_timestamp_ex((time_t)F.m_Timestamp, aDate, sizeof(aDate), "%Y-%m-%d");
						char aMeta[96];
						str_format(aMeta, sizeof(aMeta), "%s · %s", F.m_Type.c_str(), aDate);
						Text(aMeta, Style(12.0f, EFont::REGULAR, XuiTheme::Color::TEXT3));
					});
					char aTime[32];
					str_time((int64_t)(F.m_Time * 100.0), F.m_Time >= 3600.0 ? ETimeFormat::HOURS_CENTISECS : ETimeFormat::MINS_CENTISECS, aTime, sizeof(aTime));
					STextStyle TS = Style(13.0f, EFont::SEMIBOLD, XuiTheme::Color::TEXT);
					TS.m_Tabular = true;
					Text(aTime, TS);
				});
			}
			if(!P.m_vPartners.empty())
			{
				SBox H2;
				H2.m_Padding = SEdges(14.0f, 8.0f, 6.0f, 8.0f);
				Row(H2, [&]() { Text(Localize("Plays most with"), Style(12.0f, EFont::SEMIBOLD, XuiTheme::Color::TEXT3)); });
				for(const SPartner &Partner : P.m_vPartners)
				{
					SBox R;
					R.m_W = Grow();
					R.m_Padding = SEdges(6.0f, 8.0f);
					Row(R, [&]() {
						SBox NB;
						NB.m_W = Grow();
						Text(Partner.m_Name.c_str(), Style(14.0f, EFont::SEMIBOLD, XuiTheme::Color::TEXT), NB);
						char aN[16];
						str_format(aN, sizeof(aN), "%d", Partner.m_Finishes);
						STextStyle NS = Style(12.5f, EFont::REGULAR, XuiTheme::Color::TEXT3);
						NS.m_Tabular = true;
						Text(aN, NS);
					});
				}
			} }, 22.0f);
	});
	PopId();
}

void CXui::OpenPlayerContext(int Id)
{
	if(Id < 0 || Id >= MAX_CLIENTS || !GameClient()->m_aClients[Id].m_Active)
		return;
	m_PlayerContextId = Id;
	m_PlayerContextName = GameClient()->m_aClients[Id].m_aName;
	m_PlayerContextClan = GameClient()->m_aClients[Id].m_aClan;
	m_PlayerContextAt = Mouse();
	m_PlayerContextOpen = true;
}

void CXui::RenderPlayerContext()
{
	if(m_PlayerContextId < 0 || m_PlayerContextId >= MAX_CLIENTS)
		return;
	auto &Data = GameClient()->m_aClients[m_PlayerContextId];
	if(!Data.m_Active || m_PlayerContextName != Data.m_aName || m_PlayerContextClan != Data.m_aClan || (!GameClient()->m_Scoreboard.CursorActive() && !GameClient()->m_Spectator.IsActive() && !GameClient()->m_Chat.IsActive()))
	{
		m_PlayerContextOpen = false;
		return;
	}
	const bool Friend = GameClient()->Friends()->IsFriend(Data.m_aName, Data.m_aClan, true);
	const SMenuItem aItems[] = {
		{Localize("KyW · Player profile"), Ph::USER},
		{Friend ? Localize("Remove friend") : Localize("Add friend"), Ph::HEART},
		{Data.m_ChatIgnore ? Localize("Unmute chat") : Localize("Mute chat"), Ph::CHAT_CIRCLE},
		{Data.m_EmoticonIgnore ? Localize("Show emoticons") : Localize("Hide emoticons"), Ph::SMILEY},
		{Localize("Copy name"), Ph::COPY},
	};
	int Chosen = -1;
	const bool Own = m_PlayerContextId == GameClient()->m_aLocalIds[0] || m_PlayerContextId == GameClient()->m_aLocalIds[1];
	const SMenuItem aSelfItems[] = {aItems[0], aItems[4]};
	ContextMenu("player.context", Data.m_aName, Own ? aSelfItems : aItems, Own ? std::size(aSelfItems) : std::size(aItems), &m_PlayerContextOpen, m_PlayerContextAt, &Chosen);
	if(Own && Chosen == 1)
		Chosen = 4;
	if(Chosen == 0)
		OpenPlayerProfile(Data.m_aName);
	else if(Chosen == 1)
	{
		if(Friend)
			GameClient()->Friends()->RemoveFriend(Data.m_aName, Data.m_aClan);
		else
			GameClient()->Friends()->AddFriend(Data.m_aName, Data.m_aClan);
		Data.m_Friend = !Friend;
		GameClient()->m_Menus.FriendlistOnUpdate();
	}
	else if(Chosen == 2)
		Data.m_ChatIgnore = !Data.m_ChatIgnore;
	else if(Chosen == 3)
		Data.m_EmoticonIgnore = !Data.m_EmoticonIgnore;
	else if(Chosen == 4)
		Input()->SetClipboardText(Data.m_aName);
}

void CXui::OpenPlayerProfile(const char *pName)
{
	m_ProfileFromScoreboard = GameClient()->m_Scoreboard.IsActive();
	m_ProfileName = pName;
	m_ProfileTee.reset();
	for(const auto &Data : GameClient()->m_aClients)
		if(Data.m_Active && str_comp(Data.m_aName, pName) == 0)
		{
			m_ProfileTee = GameClient()->CreateManagedTeeRenderInfo(Data);
			break;
		}
	m_ProfileOpen = true;
}

void CXui::RenderViewedProfile()
{
	if(m_ProfileFromScoreboard && !GameClient()->m_Scoreboard.IsActive())
		m_ProfileOpen = false;
	if(!m_ProfileOpen)
		return;
	RegisterOverlay(MakeId("kyw.overlay"), &m_ProfileOpen);
	Layer(ELayer::MODAL, [&]() {
		SBox Card;
		Card.m_Position = EPos::ABSOLUTE;
		Card.m_Left = Card.m_Right = 28.0f;
		Card.m_Top = Card.m_Bottom = 28.0f;
		Card.m_Gap = 16.0f;
		Card.m_Padding = SEdges(20.0f);
		Card.m_Hit = EHit::BLOCK;
		const int Node = Column(Card, [&]() {
			if(Button(Localize("Back"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::ARROW_LEFT, .m_pKey = "kyw.back"}).m_Clicked)
				m_ProfileOpen = false;
			RenderProfile(GameClient()->m_Menus);
		});
		Paint(Node, [](CXui &U, const SRect &Rc) { U.DrawRect(Rc, HomePanel(24.0f)); });
	});
}
