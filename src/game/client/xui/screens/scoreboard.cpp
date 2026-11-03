#include <base/math.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/font_icons_phosphor.h>
#include <engine/friends.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/shared/protocol7.h>

#include <generated/protocol.h>

#include <game/client/gameclient.h>
#include <game/client/xui/xui.h>
#include <game/localization.h>

#include <vector>

using namespace Xui;

namespace
{
	struct SEntry
	{
		const CNetObj_PlayerInfo *m_pInfo;
		int m_DDTeam;
		bool m_Dead;
		int m_Rank = 0;
	};

	const ColorRGBA s_aMedals[3] = {XuiTheme::Rgb(0xFFD60A), XuiTheme::Rgb(0xD1D1D6), XuiTheme::Rgb(0xD08A4C)};

	void FormatScore(CGameClient *pGame, const CNetObj_PlayerInfo *pInfo, char *pBuf, int Size)
	{
		const CNetObj_GameInfo *pGameInfo = pGame->m_Snap.m_pGameInfoObj;
		const bool Race7 = pGame->Client()->IsSixup() && pGameInfo && (pGameInfo->m_GameFlags & protocol7::GAMEFLAG_RACE);
		const CGameClient::CClientData &Data = pGame->m_aClients[pInfo->m_ClientId];
		pBuf[0] = '\0';
		if(Race7)
		{
			if(pInfo->m_Score != protocol7::FinishTime::NOT_FINISHED)
				str_time(absolute(static_cast<int64_t>(pInfo->m_Score)) / 10, ETimeFormat::MINS_CENTISECS, pBuf, Size);
		}
		else if(pGame->m_ReceivedDDNetPlayerFinishTimes)
		{
			if(Data.m_FinishTimeSeconds != FinishTime::NOT_FINISHED_MILLIS)
			{
				const int64_t Centis = (static_cast<int64_t>(Data.m_FinishTimeSeconds) * 1000 + Data.m_FinishTimeMillis % 1000) / 10;
				str_time(Centis, Data.m_FinishTimeSeconds >= 3600 ? ETimeFormat::HOURS_CENTISECS : ETimeFormat::MINS_CENTISECS, pBuf, Size);
			}
		}
		else if(pGame->m_GameInfo.m_TimeScore)
		{
			if(pInfo->m_Score != FinishTime::NOT_FINISHED_TIMESCORE)
				str_time(absolute(static_cast<int64_t>(pInfo->m_Score)) * 100, ETimeFormat::HOURS, pBuf, Size);
		}
		else
			str_format(pBuf, Size, "%d", std::clamp(pInfo->m_Score, -999, 99999));
	}

	bool UsesTime(CGameClient *pGame)
	{
		const CNetObj_GameInfo *pGameInfo = pGame->m_Snap.m_pGameInfoObj;
		return (pGame->Client()->IsSixup() && pGameInfo && (pGameInfo->m_GameFlags & protocol7::GAMEFLAG_RACE)) || pGame->m_GameInfo.m_TimeScore || pGame->m_ReceivedDDNetPlayerFinishTimes;
	}

	ColorRGBA PingColor(int Ping)
	{
		if(Ping < 80)
			return XuiTheme::Color::GREEN;
		if(Ping < 150)
			return XuiTheme::Color::YELLOW;
		return XuiTheme::Color::RED_SOFT;
	}

	STextStyle Style(float Size, EFont Font, ColorRGBA Color, bool Clip = false)
	{
		STextStyle S;
		S.m_Size = Size;
		S.m_Font = Font;
		S.m_Color = Color;
		if(Clip)
		{
			S.m_Wrap = true;
			S.m_MaxLines = 1;
		}
		return S;
	}

	SBox Width(float W)
	{
		SBox B;
		B.m_W = Px(W);
		return B;
	}

	SBox GrowWidth()
	{
		SBox B;
		B.m_W = Grow();
		B.m_Shrink = true;
		return B;
	}
}

void CXui::RenderGameOverlay(bool Scoreboard)
{
	m_LightTheme = false;
	CGameClient *pGame = GameClient();
	if(pGame->m_Snap.m_pGameInfoObj == nullptr)
		return;
	const bool GameOver = pGame->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER;
	const bool Problems = Client()->State() != IClient::STATE_DEMOPLAYBACK && Client()->ConnectionProblems();
	const bool Paused = (pGame->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED) && !GameOver;

	RunPage([&]() {
		const float Show = Animate(MakeId("sb.presence"), Scoreboard ? 1.0f : 0.0f, XuiTheme::Motion::PAGE, 0, 0.0f);
		m_ScoreboardShow = Show;
		if(Show < 0.99f && g_Config.m_ClShowhud)
			RenderHudModules(1.0f - Show, false);
		if(Show < 0.99f && !pGame->m_Scoreboard.IsActive())
			RenderLeftBroadcast();
		if(pGame->m_Motd.IsActive() && !Scoreboard)
			RenderMotdCard();
		if(Show > 0.01f)
			RenderScoreboardPanel(Show);
		RenderHudInfo(Scoreboard, Problems, Paused);
		RenderPlayerContext();
		RenderViewedProfile();
	});
	if(pGame->m_Scoreboard.CursorActive() || pGame->m_Chat.IsActive() || m_ProfileOpen)
	{
		RenderTools()->RenderCursor(m_Mouse, XuiTheme::Size::MENU_CURSOR * m_Scale);
		Ui()->MapScreen();
	}
}

void CXui::RenderScoreHud(float Alpha)
{
	CGameClient *pGame = GameClient();
	struct SPill
	{
		int m_Rank;
		const CNetObj_PlayerInfo *m_pInfo;
		int m_Team;
	};
	std::vector<SPill> vPills;
	const bool Teams = pGame->IsTeamPlay() && pGame->m_Snap.m_pGameDataObj;
	if(!Teams)
	{
		int Rank = 0;
		int LocalRank = -1;
		const CNetObj_PlayerInfo *pLocal = nullptr;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			const CNetObj_PlayerInfo *pInfo = pGame->m_Snap.m_apInfoByScore[i];
			if(pInfo == nullptr)
				break;
			if(pInfo->m_Team == TEAM_SPECTATORS)
				continue;
			Rank++;
			if(Rank <= 2)
				vPills.push_back({Rank, pInfo, -1});
			if(pInfo->m_ClientId == pGame->m_Snap.m_LocalClientId)
			{
				LocalRank = Rank;
				pLocal = pInfo;
			}
		}
		if(pLocal != nullptr && LocalRank > 2)
			vPills.back() = {LocalRank, pLocal, -1};
	}
	else
	{
		vPills.push_back({pGame->m_Snap.m_pGameDataObj->m_TeamscoreRed, nullptr, TEAM_RED});
		vPills.push_back({pGame->m_Snap.m_pGameDataObj->m_TeamscoreBlue, nullptr, TEAM_BLUE});
	}
	if(vPills.empty())
		return;

	SBox Wrap;
	Wrap.m_Gap = 6.0f;
	Wrap.m_Cross = EAlign::END;
	const int WrapNode = Column(Wrap, [&]() {
		for(size_t p = 0; p < vPills.size(); p++)
		{
			const SPill &Pill = vPills[p];
			const bool Local = Pill.m_pInfo != nullptr && Pill.m_pInfo->m_ClientId == pGame->m_Snap.m_LocalClientId;
			SBox B;
			B.m_H = Px(34.0f);
			B.m_Gap = 8.0f;
			B.m_Cross = EAlign::CENTER;
			B.m_Padding = SEdges(0.0f, 12.0f, 0.0f, 6.0f);
			const int Node = Row(B, [&]() {
				if(Pill.m_Team >= 0)
				{
					SBox Dot;
					Dot.m_W = Px(22.0f);
					Dot.m_H = Px(22.0f);
					const ColorRGBA C = Pill.m_Team == TEAM_RED ? XuiTheme::Rgb(0xFF453A) : XuiTheme::Rgb(0x0A84FF);
					Paint(Row(Dot), [C](CXui &U, const SRect &Rc) { U.DrawRect(Rc, SolidRect(C, Rc.m_H * 0.5f)); });
					char aScore[16];
					str_format(aScore, sizeof(aScore), "%d", Pill.m_Rank);
					STextStyle S = Style(15.0f, EFont::BOLD, XuiTheme::Color::TEXT);
					S.m_Tabular = true;
					Text(aScore, S);
					return;
				}
				char aRank[8];
				str_format(aRank, sizeof(aRank), "%d", Pill.m_Rank);
				SBox Badge;
				Badge.m_W = Px(22.0f);
				Badge.m_H = Px(22.0f);
				Badge.m_Main = EAlign::CENTER;
				Badge.m_Cross = EAlign::CENTER;
				Badge.m_Padding = SEdges(2.0f, 3.0f, 0.0f, 0.0f);
				const bool Medal = Pill.m_Rank <= 3;
				const ColorRGBA BadgeColor = Medal ? s_aMedals[Pill.m_Rank - 1] : CXui::WithAlpha(XuiTheme::Color::TEXT, 0.18f);
				const int BadgeNode = Row(Badge, [&]() {
					STextStyle R = Style(11.5f, EFont::BOLD, Medal ? ColorRGBA(0, 0, 0, 0.85f) : XuiTheme::Color::TEXT);
					R.m_Tabular = true;
					Text(aRank, R);
				});
				Paint(BadgeNode, [BadgeColor](CXui &U, const SRect &Rc) { U.DrawRect(Rc, SolidRect(BadgeColor, Rc.m_H * 0.5f)); });
				SBox TeeBox;
				TeeBox.m_W = Px(24.0f);
				TeeBox.m_H = Px(24.0f);
				const CTeeRenderInfo TeeInfo = pGame->m_aClients[Pill.m_pInfo->m_ClientId].m_RenderInfo;
				Paint(Row(TeeBox), [TeeInfo](CXui &U, const SRect &Rc) { U.DrawTee(TeeInfo, 0, vec2(1.0f, 0.0f), Rc); });
				SBox NameBox;
				NameBox.m_W = Fit(0.0f, 130.0f);
				Text(pGame->m_aClients[Pill.m_pInfo->m_ClientId].m_aName, Style(13.0f, Local ? EFont::BOLD : EFont::SEMIBOLD, XuiTheme::Color::TEXT, true), NameBox);
				char aScore[32];
				FormatScore(pGame, Pill.m_pInfo, aScore, sizeof(aScore));
				if(aScore[0] != '\0')
				{
					STextStyle S = Style(13.0f, EFont::SEMIBOLD, XuiTheme::Color::TEXT2);
					S.m_Tabular = true;
					Text(aScore, S);
				}
			});
			const ColorRGBA Accent = m_Accent;
			Paint(Node, [Local, Accent](CXui &U, const SRect &Rc) {
				IGraphics::SUiRect Bg = SolidRect(ColorRGBA(0.04f, 0.04f, 0.06f, 0.72f), Rc.m_H * 0.5f);
				Bg.m_ShadowBlur = 12.0f;
				Bg.m_Shadow = ColorRGBA(0, 0, 0, 0.3f);
				U.DrawRect(Rc, Bg);
				U.DrawRect(Rc, GlassHairline(Rc.m_H * 0.5f));
				if(Local)
				{
					IGraphics::SUiRect Ring{};
					Ring.m_Radii = Rad(Rc.m_H * 0.5f);
					Ring.m_BorderWidth = 1.5f;
					Ring.m_Border = WithAlpha(Accent, 0.7f);
					U.DrawRect(Rc, Ring);
				}
			});
		}
	});
	SetTransform(WrapNode, 1.0f, Alpha);
}



void CXui::RenderScoreboardPanel(float Presence)
{
	CGameClient *pGame = GameClient();
	const bool Teams = pGame->IsTeamPlay();
	const bool TimeColumn = UsesTime(pGame);
	const int LocalId = pGame->m_Snap.m_LocalClientId;
	const int SpecId = pGame->m_Snap.m_SpecInfo.m_Active ? pGame->m_Snap.m_SpecInfo.m_SpectatorId : -1;
	const int FocusId = SpecId >= 0 ? SpecId : LocalId;
	const int MaxTeamSize = pGame->MaxTeamSize();

	std::vector<SEntry> avPlayers[2];
	std::vector<const CNetObj_PlayerInfo *> vSpectators;
	bool HasClan = false;
	bool HasFlags = false;
	for(int Dead = 0; Dead < 2; Dead++)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			const CNetObj_PlayerInfo *pInfo = pGame->m_Snap.m_apInfoByDDTeamScore[i];
			if(pInfo == nullptr)
				continue;
			if(pInfo->m_Team == TEAM_SPECTATORS)
			{
				if(Dead == 0)
					vSpectators.push_back(pInfo);
				continue;
			}
			const bool IsDead = Client()->m_TranslationContext.m_aClients[pInfo->m_ClientId].m_PlayerFlags7 & protocol7::PLAYERFLAG_DEAD;
			if(IsDead != (Dead == 1))
				continue;
			const CGameClient::CClientData &Data = pGame->m_aClients[pInfo->m_ClientId];
			HasClan |= Data.m_aClan[0] != '\0';
			HasFlags |= Data.m_Country != -1;
			const int Side = Teams && pInfo->m_Team == TEAM_BLUE ? 1 : 0;
			avPlayers[Side].push_back({pInfo, pGame->m_Teams.Team(pInfo->m_ClientId), IsDead});
		}
	}

#if defined(DDEV_XX)
	if(g_Config.m_XxDevFakePlayers > 0 && !avPlayers[0].empty())
	{
		const std::vector<SEntry> vReal = avPlayers[0];
		for(int i = 0; i < g_Config.m_XxDevFakePlayers; i++)
		{
			SEntry E = vReal[i % vReal.size()];
			E.m_DDTeam = i < 12 ? 1 + i / 3 : TEAM_FLOCK;
			avPlayers[0].push_back(E);
		}
		std::stable_sort(avPlayers[0].begin(), avPlayers[0].end(), [](const SEntry &A, const SEntry &B) { return (A.m_DDTeam == TEAM_FLOCK ? 999 : A.m_DDTeam) < (B.m_DDTeam == TEAM_FLOCK ? 999 : B.m_DDTeam); });
	}
#endif

	for(auto &vSide : avPlayers)
	{
		int Rank = 0;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			const CNetObj_PlayerInfo *pInfo = pGame->m_Snap.m_apInfoByScore[i];
			if(pInfo == nullptr)
				break;
			for(SEntry &E : vSide)
			{
				if(E.m_pInfo != pInfo)
					continue;
				E.m_Rank = ++Rank;
			}
		}
	}
	const int Total = (int)(avPlayers[0].size() + avPlayers[1].size());
	std::vector<std::vector<SEntry>> vColumns;
	if(Teams)
	{
		vColumns.push_back(avPlayers[0]);
		vColumns.push_back(avPlayers[1]);
	}
	else
	{
		const std::vector<SEntry> &vAll = avPlayers[0];
		const int NumCols = Total > 40 && m_ScreenUnits.x >= 1300.0f ? 3 : (Total > 16 ? 2 : 1);
		const size_t Per = (vAll.size() + NumCols - 1) / NumCols;
		size_t Begin = 0;
		for(int c = 0; c < NumCols && Begin < vAll.size(); c++)
		{
			size_t End = c == NumCols - 1 ? vAll.size() : std::min(vAll.size(), Begin + Per);
			while(End > Begin && End < vAll.size() && vAll[End].m_DDTeam != TEAM_FLOCK && vAll[End].m_DDTeam == vAll[End - 1].m_DDTeam && End - Begin < Per + 4)
				End++;
			vColumns.emplace_back(vAll.begin() + Begin, vAll.begin() + End);
			Begin = End;
		}
		if(vColumns.empty())
			vColumns.emplace_back();
	}

	auto BandEnd = [](const std::vector<SEntry> &vRows, size_t i) {
		size_t End = i + 1;
		if(vRows[i].m_DDTeam != TEAM_FLOCK)
			while(End < vRows.size() && vRows[End].m_DDTeam == vRows[i].m_DDTeam)
				End++;
		return End;
	};
	float Units = 1.0f;
	for(const auto &vColumn : vColumns)
	{
		float ColUnits = 0.0f;
		for(size_t i = 0; i < vColumn.size(); i = BandEnd(vColumn, i))
			ColUnits += vColumn[i].m_DDTeam != TEAM_FLOCK ? 0.9f : 0.0f;
		Units = std::max(Units, ColUnits + vColumn.size());
	}
	const float PlaybackSpace = Client()->State() == IClient::STATE_DEMOPLAYBACK && pGame->m_Menus.IsActive() ? 180.0f : 0.0f;
	const float Avail = m_ScreenUnits.y - PlaybackSpace - 56.0f - 196.0f - (Teams ? 44.0f : 0.0f) - (vSpectators.empty() ? 0.0f : 40.0f);
	const float RowH = std::clamp(std::floor(Avail / Units) - 3.0f, 16.0f, 38.0f);
	const bool Compact = RowH < 26.0f;
	const float FontSize = RowH >= 32.0f ? 14.0f : (RowH >= 26.0f ? 13.0f : (RowH >= 21.0f ? 12.0f : 11.0f));
	const float TeeSize = std::min(RowH + 4.0f, 38.0f);
	const float BandHeadH = std::max(15.0f, std::round(RowH * 0.62f));
	const int NumCols = (int)vColumns.size();
	const float ColW = NumCols == 1 ? 700.0f : (NumCols == 2 ? 540.0f : 420.0f);
	const float PanelW = std::min(NumCols * ColW + (NumCols - 1) * 18.0f + 36.0f, m_ScreenUnits.x - 48.0f);
	if(NumCols > 1)
	{
		HasClan = false;
		HasFlags = false;
	}
	const float ClanW = NumCols > 1 ? 104.0f : 130.0f;
	const float ScoreW = Compact ? 70.0f : 84.0f;
	const float RowGap = Compact ? 1.0f : 3.0f;
	const ColorRGBA Accent = m_Accent;

	auto RankBadge = [&](int Rank, float Size, float Font) {
		char aRank[8];
		str_format(aRank, sizeof(aRank), "%d", Rank);
		SBox Badge;
		Badge.m_W = Px(Size);
		Badge.m_H = Px(Size);
		Badge.m_Main = EAlign::CENTER;
		Badge.m_Cross = EAlign::CENTER;
		Badge.m_Padding = SEdges(Font * 0.16f, Font * 0.26f, 0.0f, 0.0f);
		const bool Medal = false;
		const int Node = Row(Badge, [&]() {
			STextStyle R = Style(Font, EFont::BOLD, Medal ? ColorRGBA(0, 0, 0, 0.82f) : XuiTheme::Color::TEXT3);
			R.m_Tabular = true;
			Text(Rank > 0 ? aRank : "·", R);
		});
		if(Medal)
		{
			const ColorRGBA C = s_aMedals[Rank - 1];
			Paint(Node, [C](CXui &U, const SRect &Rc) {
				IGraphics::SUiRect R = SolidRect(Mix(C, ColorRGBA(1, 1, 1, 1), 0.25f), Rc.m_H * 0.5f);
				R.m_Fill2 = Mix(C, ColorRGBA(0, 0, 0, 1), 0.12f);
				U.DrawRect(Rc, R);
			});
		}
	};

	SBox Wrap;
	Wrap.m_Position = EPos::ABSOLUTE;
	Wrap.m_Left = Wrap.m_Right = 0.0f;
	Wrap.m_Top = 56.0f;
	Wrap.m_Main = EAlign::CENTER;
	Row(Wrap, [&]() {
		SBox Panel;
		Panel.m_W = Px(PanelW);
		Panel.m_Padding = SEdges(18.0f);
		Panel.m_Gap = 14.0f;
		const int PanelNode = Column(Panel, [&]() {
			const CServerInfo &Info = Client()->ServerInfo();
			SBox Head;
			Head.m_W = Grow();
			Head.m_Gap = 10.0f;
			Head.m_Cross = EAlign::CENTER;
			Head.m_Padding = SEdges(0.0f, 6.0f, 6.0f, 6.0f);
			Row(Head, [&]() {
				STextStyle MapStyle = Style(17.0f, EFont::BOLD, XuiTheme::Color::TEXT);
				Text(Info.m_aMap[0] != '\0' ? Info.m_aMap : pGame->Map()->BaseName(), MapStyle);
				SBox NameBox = GrowWidth();
				Text(Info.m_aName, Style(12.5f, EFont::REGULAR, XuiTheme::Color::TEXT3, true), NameBox);
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "%d / %d", Total + (int)vSpectators.size(), Info.m_MaxClients);
				STextStyle Count = Style(12.5f, EFont::SEMIBOLD, XuiTheme::Color::TEXT2);
				Count.m_Tabular = true;
				Text(aBuf, Count);
			});
			SBox Rule;
			Rule.m_W = Grow();
			Rule.m_H = Px(1.0f);
			Paint(Row(Rule), [](CXui &U, const SRect &Rc) { U.DrawRect(Rc, SolidRect(ColorRGBA(1, 1, 1, 0.07f), 0.0f)); });

			SBox Columns;
			Columns.m_W = Grow();
			Columns.m_Gap = 18.0f;
			Columns.m_Cross = EAlign::START;
			int Appear = 0;
			Row(Columns, [&]() {
				for(size_t c = 0; c < vColumns.size(); c++)
				{
					const std::vector<SEntry> &vRows = vColumns[c];
					SBox Col = GrowWidth();
					Col.m_Gap = RowGap;
					Col.m_Cross = EAlign::STRETCH;
					float ContentH = 20.0f + (Teams ? 40.0f : 0.0f) + vRows.size() * (RowH + RowGap);
					for(size_t i = 0; i < vRows.size(); i = BandEnd(vRows, i))
						if(vRows[i].m_DDTeam != TEAM_FLOCK)
							ContentH += BandHeadH + 4.0f + RowGap;
					Col.m_H = Px(std::min(ContentH, std::max(120.0f, Avail + 76.0f)));

					PushId((uint32_t)c + 1);
					Scroll(Col, [&]() {
						if(Teams)
						{
							const bool Red = c == 0;
							const CNetObj_GameData *pData = pGame->m_Snap.m_pGameDataObj;
							SBox TB;
							TB.m_H = Px(40.0f);
							TB.m_Gap = 10.0f;
							TB.m_Cross = EAlign::CENTER;
							TB.m_Padding = SEdges(0.0f, 14.0f);
							const ColorRGBA TeamColor = Red ? XuiTheme::Rgb(0xFF453A) : XuiTheme::Rgb(0x0A84FF);
							const int TBNode = Row(TB, [&]() {
								Text(Red ? Localize("Red team") : Localize("Blue team"), Style(15.0f, EFont::BOLD, XuiTheme::Color::TEXT), GrowWidth());
								if(pData)
								{
									char aScore[16];
									str_format(aScore, sizeof(aScore), "%d", Red ? pData->m_TeamscoreRed : pData->m_TeamscoreBlue);
									STextStyle S = Style(20.0f, EFont::EXTRABOLD, XuiTheme::Color::TEXT);
									S.m_Tabular = true;
									Text(aScore, S);
								}
							});
							Paint(TBNode, [TeamColor](CXui &U, const SRect &Rc) {
								IGraphics::SUiRect R = SolidRect(WithAlpha(TeamColor, 0.34f), 14.0f);
								R.m_Fill2 = WithAlpha(TeamColor, 0.1f);
								R.m_GradientAngle = 0.0f;
								U.DrawRect(Rc, R);
							});
						}

						SBox HeadRow;
						HeadRow.m_H = Px(20.0f);
						HeadRow.m_Gap = 10.0f;
						HeadRow.m_Cross = EAlign::CENTER;
						HeadRow.m_Padding = SEdges(0.0f, 14.0f, 0.0f, 10.0f);
						Row(HeadRow, [&]() {
							STextStyle H = Style(10.5f, EFont::SEMIBOLD, XuiTheme::Color::TEXT3);
							H.m_Align = ETextAlign::CENTER;
							Text("#", H, Width(24.0f));
							Row(Width(TeeSize));
							H.m_Align = ETextAlign::LEFT;
							Text(Localize("Player"), H, GrowWidth());
							if(HasClan)
								Text(Localize("Clan"), H, Width(ClanW));
							if(HasFlags)
								Row(Width(24.0f));
							H.m_Align = ETextAlign::RIGHT;
							Text(TimeColumn ? Localize("Time") : Localize("Score"), H, Width(ScoreW));
							Text(Localize("Ping"), H, Width(52.0f));
						});

						for(size_t i = 0; i < vRows.size();)
						{
							const size_t End = BandEnd(vRows, i);
							const int DDTeam = vRows[i].m_DDTeam;
							const bool Band = DDTeam != TEAM_FLOCK;
							const ColorRGBA TeamColor = Band ? pGame->GetDDTeamColor(DDTeam) : ColorRGBA();
							SBox BandBox;
							BandBox.m_W = Grow();
							BandBox.m_Cross = EAlign::STRETCH;
							BandBox.m_Gap = RowGap;
							BandBox.m_Padding = Band ? SEdges(0.0f, 0.0f, 4.0f, 0.0f) : SEdges();
							const size_t Start = i;
							const int BandNode = Column(BandBox, [&]() {
								if(Band)
								{
									SBox BH;
									BH.m_H = Px(BandHeadH);
									BH.m_Gap = 7.0f;
									BH.m_Cross = EAlign::CENTER;
									BH.m_Padding = SEdges(0.0f, 14.0f);
									Row(BH, [&]() {
										SBox Dot;
										Dot.m_W = Px(7.0f);
										Dot.m_H = Px(7.0f);
										Paint(Row(Dot), [TeamColor](CXui &U, const SRect &Rc) { U.DrawRect(Rc, SolidRect(TeamColor, Rc.m_H * 0.5f)); });
										char aTeam[64];
										if(DDTeam == pGame->m_Teams.TeamSuper())
											str_copy(aTeam, Localize("Super"));
										else
											str_format(aTeam, sizeof(aTeam), Localize("Team %d"), DDTeam);
										Text(aTeam, Style(std::min(11.5f, FontSize - 1.0f), EFont::BOLD, Mix(TeamColor, ColorRGBA(1, 1, 1, 1), 0.55f)));
										if(MaxTeamSize > 0 && DDTeam != pGame->m_Teams.TeamSuper())
										{
											char aCount[16];
											str_format(aCount, sizeof(aCount), "%d/%d", (int)(End - Start), MaxTeamSize);
											STextStyle CS = Style(std::min(11.0f, FontSize - 1.5f), EFont::MEDIUM, XuiTheme::Color::TEXT3);
											CS.m_Tabular = true;
											Text(aCount, CS);
										}
									});
								}
								for(size_t r = Start; r < End; r++)
								{
									const SEntry &E = vRows[r];
									const CNetObj_PlayerInfo *pInfo = E.m_pInfo;
									const CGameClient::CClientData &Data = pGame->m_aClients[pInfo->m_ClientId];
									const bool Highlight = pInfo->m_ClientId == FocusId;
									const float RowAlpha = E.m_Dead ? 0.5f : 1.0f;
									char aKey[16];
									str_format(aKey, sizeof(aKey), "p%d", pInfo->m_ClientId);
									SBox R;
									R.m_H = Px(RowH);
									R.m_W = Grow();
									R.m_Gap = 10.0f;
									R.m_Cross = EAlign::CENTER;
									R.m_Padding = SEdges(0.0f, 14.0f, 0.0f, 10.0f);
									R.m_Flip = true;
									R.m_Hit = pGame->m_Scoreboard.CursorActive() ? EHit::INTERACTIVE : EHit::NONE;
									const uint32_t PlayerRowId = MakeId(aKey);
									if(Interact(PlayerRowId, pGame->m_Scoreboard.CursorActive()).m_RightClicked)
										OpenPlayerContext(pInfo->m_ClientId);
									const int RowNode = BoxId(PlayerRowId, EDir::ROW, R, [&]() {
										SBox RankBox = Width(24.0f);
										RankBox.m_Main = EAlign::CENTER;
										RankBox.m_Cross = EAlign::CENTER;
										Row(RankBox, [&]() { RankBadge(E.m_Rank, std::min(RowH - 6.0f, 22.0f), FontSize - 2.5f); });
										SBox TeeBox;
										TeeBox.m_W = Px(TeeSize);
										TeeBox.m_H = Px(TeeSize);
										const CTeeRenderInfo TeeInfo = Data.m_RenderInfo;
										Paint(Row(TeeBox), [TeeInfo](CXui &U, const SRect &Rc) { U.DrawTee(TeeInfo, 0, vec2(1.0f, 0.0f), Rc); });
										char aName[64];
										if(g_Config.m_ClShowIds)
										{
											char aId[16];
											pGame->FormatClientId(pInfo->m_ClientId, aId, EClientIdFormat::INDENT_AUTO);
											str_format(aName, sizeof(aName), "%s%s", aId, Data.m_aName);
										}
										else
											str_copy(aName, Data.m_aName);
										ColorRGBA NameColor = XuiTheme::Color::TEXT;
										if(Data.m_AuthLevel)
											NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClAuthedPlayerColor));
										SBox NB = GrowWidth();
										NB.m_Shrink = true;
										Column(NB, [&]() {
											Row({.m_Gap = 6.0f, .m_W = Grow(), .m_Cross = EAlign::CENTER}, [&]() {
												SBox NameText;
												NameText.m_W = Grow();
												NameText.m_Shrink = true;
												Text(aName, Style(FontSize, Highlight ? EFont::BOLD : EFont::SEMIBOLD, WithAlpha(NameColor, RowAlpha), true), NameText);
												if(Data.m_Friend)
													Icon(Ph::HEART, FontSize - 2.0f, XuiTheme::Color::PINK, true);
												if(Data.m_Afk)
													Icon(Ph::MOON, FontSize - 2.0f, XuiTheme::Color::TEXT3, true);
											});
											if(NumCols > 1 && RowH >= 32.0f && Data.m_aClan[0])
												Text(Data.m_aClan, Style(10.0f, EFont::REGULAR, WithAlpha(XuiTheme::Color::TEXT3, RowAlpha), true), {.m_W = Grow(), .m_Shrink = true});
										});
										if(HasClan)
										{
											ColorRGBA ClanColor = XuiTheme::Color::TEXT3;
											const int Own = pGame->m_aLocalIds[g_Config.m_ClDummy];
											if(Own >= 0 && Data.m_aClan[0] != '\0' && str_comp(Data.m_aClan, pGame->m_aClients[Own].m_aClan) == 0)
												ClanColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClSameClanColor));
											Text(Data.m_aClan, Style(FontSize - 1.0f, EFont::MEDIUM, WithAlpha(ClanColor, RowAlpha), true), Width(ClanW));
										}
										if(HasFlags)
										{
											SBox FlagBox;
											FlagBox.m_W = Px(24.0f);
											FlagBox.m_H = Px(12.0f);
											const int Country = Data.m_Country;
											const int FlagNode = Row(FlagBox);
											if(Country != -1)
												Paint(FlagNode, [pGame, Country, RowAlpha](CXui &U, const SRect &Rc) {
													const float A = 0.9f * RowAlpha * U.CurrentAlpha();
													U.DrawDirect(Rc, [pGame, Country, A](const SRect &Px) {
														pGame->m_CountryFlags.Render(Country, ColorRGBA(1.0f, 1.0f, 1.0f, A), Px.m_X, Px.m_Y, Px.m_W, Px.m_H);
													});
												});
										}
										char aScore[32];
										FormatScore(pGame, pInfo, aScore, sizeof(aScore));
										STextStyle S = Style(FontSize, EFont::SEMIBOLD, WithAlpha(aScore[0] != '\0' ? XuiTheme::Color::TEXT : XuiTheme::Color::TEXT3, RowAlpha));
										S.m_Tabular = true;
										S.m_Align = ETextAlign::RIGHT;
										Text(aScore[0] != '\0' ? aScore : "—", S, Width(ScoreW));
										SBox PingBox = Width(52.0f);
										PingBox.m_Gap = 6.0f;
										PingBox.m_Main = EAlign::END;
										PingBox.m_Cross = EAlign::CENTER;
										Row(PingBox, [&]() {
											const int Ping = std::clamp(pInfo->m_Latency, 0, 999);
											const ColorRGBA PC = g_Config.m_ClEnablePingColor ? PingColor(Ping) : XuiTheme::Color::TEXT3;
											SBox Dot;
											Dot.m_W = Px(6.0f);
											Dot.m_H = Px(6.0f);
											Paint(Row(Dot), [PC](CXui &U, const SRect &Rc) { U.DrawRect(Rc, SolidRect(PC, Rc.m_H * 0.5f)); });
											char aPing[16];
											str_format(aPing, sizeof(aPing), "%d", Ping);
											STextStyle P = Style(FontSize - 1.5f, EFont::MEDIUM, XuiTheme::Color::TEXT2);
											P.m_Tabular = true;
											Text(aPing, P);
										});
									});
									const float Delay = std::min(Appear * 0.018f, 0.35f);
									const float Show = std::clamp((Presence - Delay) / (1.0f - Delay), 0.0f, 1.0f);
									Appear++;
									SetTransform(RowNode, 1.0f, Show, 0.0f, (1.0f - Show) * 10.0f);
									const bool Zebra = r % 2 == 1;
									Paint(RowNode, [Highlight, Zebra, Accent](CXui &U, const SRect &Rc) {
										if(Highlight)
										{
											U.DrawRect(Rc, SolidRect(ColorRGBA(1, 1, 1, 0.07f), 6.0f));
											U.DrawRect({Rc.m_X, Rc.m_Y + 6.0f, 2.0f, Rc.m_H - 12.0f}, SolidRect(Accent, 1.0f));
										}
										else if(Zebra)
											U.DrawRect(Rc, SolidRect(ColorRGBA(1, 1, 1, 0.02f), 6.0f));
									});
								}
							});
							if(Band)
								Paint(BandNode, [TeamColor](CXui &U, const SRect &Rc) {
									U.DrawRect({Rc.m_X, Rc.m_Y + 4.0f, 3.0f, Rc.m_H - 8.0f}, SolidRect(WithAlpha(TeamColor, 0.85f), 1.5f));
								});
							i = End;
						}
						if(vRows.empty())
						{
							SBox E;
							E.m_Padding = SEdges(10.0f, 14.0f);
							Row(E, [&]() { Text(Localize("No players"), Style(13.0f, EFont::REGULAR, XuiTheme::Color::TEXT3)); });
						}
					});
					PopId();
				}
			});

			if(!vSpectators.empty())
			{
				SBox SpecRow;
				SpecRow.m_W = Grow();
				SpecRow.m_Gap = 6.0f;
				SpecRow.m_Wrap = true;
				SpecRow.m_H = Fit(0.0f, 64.0f);
				SpecRow.m_Scroll = EScroll::VERTICAL;
				SpecRow.m_Clip = true;
				SpecRow.m_Cross = EAlign::CENTER;
				SpecRow.m_Padding = SEdges(2.0f, 6.0f, 0.0f, 6.0f);
				Row(SpecRow, [&]() {
					Icon(Ph::EYE, 15.0f, XuiTheme::Color::TEXT3);
					char aSpec[32];
					str_format(aSpec, sizeof(aSpec), Localize("%d spectators"), (int)vSpectators.size());
					Text(aSpec, Style(12.0f, EFont::SEMIBOLD, XuiTheme::Color::TEXT3));
					for(const CNetObj_PlayerInfo *pInfo : vSpectators)
					{
						const CGameClient::CClientData &Data = pGame->m_aClients[pInfo->m_ClientId];
						SBox C;
						C.m_H = Px(26.0f);
						C.m_Gap = 6.0f;
						C.m_Padding = SEdges(0.0f, 10.0f, 0.0f, 4.0f);
						C.m_Cross = EAlign::CENTER;
						char aSpecKey[32];
						str_format(aSpecKey, sizeof(aSpecKey), "spec%d", pInfo->m_ClientId);
						const uint32_t SpecId = MakeId(aSpecKey);
						C.m_Hit = pGame->m_Scoreboard.CursorActive() ? EHit::INTERACTIVE : EHit::NONE;
						if(Interact(SpecId, pGame->m_Scoreboard.CursorActive()).m_RightClicked)
							OpenPlayerContext(pInfo->m_ClientId);
						const int Node = BoxId(SpecId, EDir::ROW, C, [&]() {
							SBox TeeBox;
							TeeBox.m_W = Px(20.0f);
							TeeBox.m_H = Px(20.0f);
							const CTeeRenderInfo TeeInfo = Data.m_RenderInfo;
							Paint(Row(TeeBox), [TeeInfo](CXui &U, const SRect &Rc) { U.DrawTee(TeeInfo, 0, vec2(1.0f, 0.0f), Rc); });
							Text(Data.m_aName, Style(12.0f, EFont::MEDIUM, XuiTheme::Color::TEXT));
						});
						Paint(Node, [](CXui &U, const SRect &Rc) { U.DrawRect(Rc, SolidRect(XuiTheme::Color::GHOST, Rc.m_H * 0.5f)); });
					}
				});
			}
		});
		SetTransform(PanelNode, 0.965f + 0.035f * Presence, std::min(1.0f, Presence * 1.4f), 0.0f, (1.0f - Presence) * -14.0f);
		Paint(PanelNode, [](CXui &U, const SRect &Rc) {
			U.DrawRect(Rc, SolidRect(ColorRGBA(0.035f, 0.035f, 0.045f, 0.9f), 16.0f));
			U.DrawRect(Rc, GlassHairline(16.0f));
		});
	});
}

void CXui::RenderMotdCard()
{
	const char *pMotd = GameClient()->m_Motd.ServerMotd();
	if(pMotd[0] == '\0')
		return;
	SBox Wrap;
	Wrap.m_Position = EPos::ABSOLUTE;
	Wrap.m_Left = Wrap.m_Right = Wrap.m_Top = Wrap.m_Bottom = 0.0f;
	Wrap.m_Main = EAlign::CENTER;
	Wrap.m_Cross = EAlign::CENTER;
	Column(Wrap, [&]() {
		SBox Card;
		Card.m_W = Fit(0.0f, std::min(720.0f, m_ScreenUnits.x - 48.0f));
		Card.m_H = Fit(0.0f, m_ScreenUnits.y * 0.7f);
		Card.m_Padding = SEdges(18.0f, 22.0f);
		const int Node = Scroll(Card, [&]() {
			STextStyle B;
			B.m_Size = 14.5f;
			B.m_Wrap = true;
			Text(pMotd, B);
		});
		Paint(Node, [](CXui &U, const SRect &Rc) {
			U.DrawRect(Rc, GlassRect(22.0f, 0.82f));
			U.DrawRect(Rc, GlassHairline(22.0f));
		});
	});
}

void CXui::RenderHudInfo(bool Scoreboard, bool Problems, bool Paused)
{
	auto Pill = [this](const FBuild &Content, ColorRGBA Tint) {
		SBox B;
		B.m_H = Px(30.0f);
		B.m_Gap = 8.0f;
		B.m_Padding = SEdges(0.0f, 12.0f);
		B.m_Cross = EAlign::CENTER;
		const int Node = Row(B, Content);
		Paint(Node, [Tint](CXui &U, const SRect &Rc) {
			U.DrawRect(Rc, SolidRect(Tint, Rc.m_H * 0.5f));
			U.DrawRect(Rc, GlassHairline(Rc.m_H * 0.5f));
		});
	};
	const ColorRGBA Dark(0.04f, 0.04f, 0.06f, 0.7f);

	if(Problems || Paused)
	{
		SBox Center;
		Center.m_Position = EPos::ABSOLUTE;
		Center.m_Left = Center.m_Right = 0.0f;
		Center.m_Top = 92.0f;
		Center.m_Main = EAlign::CENTER;
		Center.m_Gap = 8.0f;
		Row(Center, [&]() {
			STextStyle S;
			S.m_Size = 15.0f;
			S.m_Font = EFont::BOLD;
			if(Problems)
			{
				const float Blink = 0.75f + 0.25f * std::sin(Time() * 6.0f);
				Pill([&]() {
					Icon(Ph::WIFI_SLASH, 17.0f, WithAlpha(XuiTheme::Color::ORANGE, Blink), true);
					Text(Localize("Connection Problems…"), S);
				},
					ColorRGBA(0.12f, 0.06f, 0.0f, 0.82f));
			}
			if(Paused)
			{
				Pill([&]() {
					Icon(Ph::PAUSE, 17.0f, XuiTheme::Color::TEXT, true);
					Text(Localize("Game paused"), S);
				},
					Dark);
			}
		});
	}
}

int CXui::RenderSpectator()
{
	int Selected = -3;
	CGameClient *pGame = GameClient();
	std::vector<int> vPlayers;
	for(const auto *pInfo : pGame->m_Snap.m_apInfoByDDTeamName)
		if(pInfo && pInfo->m_Team != TEAM_SPECTATORS)
			vPlayers.push_back(pInfo->m_ClientId);
#if defined(DDEV_XX)
	if(g_Config.m_XxDevFakePlayers && !vPlayers.empty())
		while((int)vPlayers.size() < g_Config.m_XxDevFakePlayers)
			vPlayers.push_back(vPlayers.front());
#endif
	const int Count = (int)vPlayers.size();
	const int Columns = Count <= 16 ? 1 : Count <= 32 ? 2 :
				      Count <= 64         ? 3 :
							    4;
	const int Rows = std::max(1, (Count + Columns - 1) / Columns);
	const float RowHeight = std::clamp((m_ScreenUnits.y - 240.0f) / Rows, 10.0f, 42.0f);

	RunPage([&]() {
		SBox Wrap;
		Wrap.m_Position = EPos::ABSOLUTE;
		Wrap.m_Left = Wrap.m_Right = 0.0f;
		Wrap.m_Top = 80.0f;
		Wrap.m_Main = EAlign::CENTER;
		Row(Wrap, [&]() {
			SBox Panel;
			Panel.m_W = Px(std::min(Columns == 1 ? 680.0f : Columns * 320.0f + 40.0f, m_ScreenUnits.x - 48.0f));
			Panel.m_H = Px(std::min(m_ScreenUnits.y - 120.0f, Rows * RowHeight + 150.0f));
			Panel.m_Padding = SEdges(20.0f);
			Panel.m_Gap = 12.0f;
			Panel.m_Hit = EHit::BLOCK;
			const int Node = Column(Panel, [&]() {
				Text(Localize("Spectate"), Style(22.0f, EFont::BOLD, XuiTheme::Color::TEXT));
				SBox Modes;
				Modes.m_Gap = 8.0f;
				Modes.m_W = Grow();
				Row(Modes, [&]() {
					const char *apModes[] = {Localize("Free-View"), Localize("Multi-View"), Localize("Follow")};
					const int aIds[] = {SPEC_FREEVIEW, -4, SPEC_FOLLOW};
					for(int i = 0; i < (Client()->State() == IClient::STATE_DEMOPLAYBACK ? 3 : 2); ++i)
					{
						PushId(i + 1);
						const SResult Result = Button(apModes[i], {.m_Variant = EVariant::GLASS, .m_pKey = "spec.mode"});
						if(Result.m_Hovered)
							Selected = aIds[i];
						if(Result.m_Clicked)
						{
							pGame->m_MultiViewActivated = aIds[i] == -4;
							if(aIds[i] != -4)
								pGame->m_Spectator.Spectate(aIds[i]);
							pGame->m_Spectator.OnRelease();
						}
						PopId();
					}
				});
				SBox List;
				List.m_W = Grow();
				List.m_H = Grow();
				List.m_Gap = 10.0f;
				Row(List, [&]() {
					for(int Col = 0; Col < Columns; ++Col)
						Column({.m_W = Grow()}, [&]() {
							for(int Index = Col * Rows; Index < std::min(Count, (Col + 1) * Rows); ++Index)
							{
								const int Id = vPlayers[Index];
								const auto &Data = pGame->m_aClients[Id];
								PushId(Index + 10);
								const uint32_t RowId = MakeId("spec.player");
								const SResult Result = Interact(RowId);
								if(Result.m_Hovered)
									Selected = Id;
								if(Result.m_RightClicked)
									OpenPlayerContext(Id);
								if(Result.m_Clicked)
								{
									if(pGame->m_MultiViewActivated)
										pGame->m_aMultiViewId[Id] = !pGame->m_aMultiViewId[Id];
									else
									{
										pGame->m_Spectator.Spectate(Id);
										pGame->m_Spectator.OnRelease();
									}
								}
								SBox RowBox;
								RowBox.m_W = Grow();
								RowBox.m_H = Px(RowHeight);
								RowBox.m_Gap = 6.0f;
								RowBox.m_Cross = EAlign::CENTER;
								RowBox.m_Padding = SEdges(0.0f, 6.0f);
								RowBox.m_Hit = EHit::INTERACTIVE;
								const int RowNode = BoxId(RowId, EDir::ROW, RowBox, [&]() {
									const CTeeRenderInfo TeeInfo = Data.m_RenderInfo;
									Paint(Row({.m_W = Px(std::min(32.0f, RowHeight)), .m_H = Px(std::min(32.0f, RowHeight))}), [TeeInfo](CXui &U, const SRect &Rc) { U.DrawTee(TeeInfo, EMOTE_NORMAL, vec2(1, 0), Rc); });
									Text(Data.m_aName, Style(std::min(14.0f, RowHeight - 4.0f), EFont::SEMIBOLD, XuiTheme::Color::TEXT), {.m_W = Grow()});

									if(pGame->m_MultiViewActivated && pGame->m_aMultiViewId[Id])
										Icon(Ph::CHECK, 16.0f, m_Accent);
								});
								Paint(RowNode, [Hover = Result.m_Hovered](CXui &U, const SRect &Rc) { U.DrawRect(Rc, SolidRect(ColorRGBA(1, 1, 1, Hover ? 0.1f : 0.03f), 8.0f)); });
								PopId();
							}
						});
				});
			});
			Paint(Node, [](CXui &U, const SRect &Rc) { U.DrawRect(Rc, HomePanel(20.0f)); });
		});
		RenderPlayerContext();
		RenderViewedProfile();
	});
	RenderTools()->RenderCursor(m_Mouse, XuiTheme::Size::MENU_CURSOR * m_Scale);
	Ui()->MapScreen();
	return Selected;
}
