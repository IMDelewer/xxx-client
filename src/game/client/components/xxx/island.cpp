#include "island.h"

#include <base/math.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/components/console.h>
#include <game/client/components/menus.h>
#include <game/client/components/scoreboard.h>
#include <game/client/components/sounds.h>
#include <game/client/components/statboard.h>
#include <game/client/components/voting.h>
#include <game/client/gameclient.h>
#include <game/client/xui/theme.h>

#include <algorithm>
#include <cmath>

static const ColorRGBA IslandInk(1.0f, 1.0f, 1.0f, 1.0f);
static const ColorRGBA IslandInk2(1.0f, 1.0f, 1.0f, 0.72f);
static const ColorRGBA IslandInk3(1.0f, 1.0f, 1.0f, 0.5f);

static float IslandFit(float Width, float Target)
{
	if(Target <= 1.0f)
		return 1.0f;
	return std::clamp((Width / Target - 0.8f) / 0.2f, 0.0f, 1.0f);
}

namespace
{
	constexpr int WEIGHT_REGULAR = 400;
	constexpr int WEIGHT_SEMIBOLD = 600;
	constexpr int WEIGHT_BOLD = 700;

	constexpr float TEST_VOTE_SECONDS = 12.0f;
	constexpr float TEST_DOWNLOAD_SECONDS = 5.0f;
	constexpr float PI = 3.14159265f;

	float Approach(float Current, float Target, float Speed, float Delta)
	{
		return Current + (Target - Current) * (1.0f - std::exp(-Speed * Delta));
	}

	ColorRGBA Mix(ColorRGBA A, ColorRGBA B, float t)
	{
		return ColorRGBA(A.r + (B.r - A.r) * t, A.g + (B.g - A.g) * t, A.b + (B.b - A.b) * t, A.a + (B.a - A.a) * t);
	}

	ColorRGBA AccentColor()
	{
		return color_cast<ColorRGBA>(ColorHSLA(g_Config.m_XxUiAccent));
	}

	void FormatClock(char *pBuf, int BufSize, int Seconds)
	{
		str_format(pBuf, BufSize, "%d:%02d", Seconds / 60, Seconds % 60);
	}
}

bool CIsland::SourceEnabled(int Source)
{
	switch(Source)
	{
	case SRC_MUSIC: return g_Config.m_XxIslandSrcMusic;
	case SRC_SPLITS: return g_Config.m_XxIslandSrcSplits;
	case SRC_CLIPS: return g_Config.m_XxIslandSrcClips;
	case SRC_ACHIEVEMENTS: return g_Config.m_XxIslandSrcAchievements;
	case SRC_BLOCK: return g_Config.m_XxIslandSrcBlock;
	case SRC_FRIENDS: return g_Config.m_XxIslandSrcFriends;
	case SRC_CHAT: return g_Config.m_XxIslandSrcChat;
	case SRC_VOTES: return g_Config.m_XxIslandSrcVotes;
	case SRC_DOWNLOAD: return g_Config.m_XxIslandSrcDownload;
	default: return true;
	}
}

void CIsland::OnConsoleInit()
{
#if defined(DDEV_XX)
	Console()->Register("xx_island_test", "?s[template] ?s[title] ?s[subtitle]", CFGFLAG_CLIENT, ConIslandTest, this, "Test a dynamic island template: notify, vote, download, queue, music, split, achievement, block, clear");
#endif
}

#if defined(DDEV_XX)
void CIsland::ConIslandTest(IConsole::IResult *pResult, void *pUserData)
{
	CIsland *pSelf = static_cast<CIsland *>(pUserData);
	const char *pName = pResult->NumArguments() > 0 ? pResult->GetString(0) : "notify";
	const ColorRGBA Accent = AccentColor();

	SNotification N;
	N.m_Color = Accent;
	if(str_comp(pName, "notify") == 0)
	{
		N.m_pIcon = FontIcon::STAR;
		N.m_Title = pResult->NumArguments() > 1 ? pResult->GetString(1) : "xxx client";
		N.m_Subtitle = pResult->NumArguments() > 2 ? pResult->GetString(2) : "Dynamic island works";
		pSelf->Push(N);
	}
	else if(str_comp(pName, "vote") == 0)
	{
		pSelf->m_TestVote = STestVote();
		pSelf->m_TestVote.m_Active = true;
		pSelf->m_VoteSeq++;
	}
	else if(str_comp(pName, "download") == 0)
	{
		pSelf->m_TestDownload = STestDownload();
		pSelf->m_TestDownload.m_Active = true;
	}
	else if(str_comp(pName, "queue") == 0)
	{
		pSelf->SetQueue(2, "Kobra 4 · 62/64", [pSelf]() { pSelf->ClearQueue(); });
	}
	else if(str_comp(pName, "music") == 0)
	{
		SActivity Activity;
		Activity.m_Source = SRC_NONE;
		Activity.m_pIcon = FontIcon::MUSIC;
		Activity.m_Text = "Midnight City";
		Activity.m_Artist = "M83";
		Activity.m_Color = Accent;
		Activity.m_Position = 74;
		Activity.m_Length = 243;
		pSelf->SetActivity("test_music", Activity);
		pSelf->m_MusicOpen = true;
		pSelf->m_MusicOpenLimit = 10.0f;
		pSelf->m_MusicOpenElapsed = 0.0f;
	}
	else if(str_comp(pName, "split") == 0)
	{
		static const float s_aDeltas[] = {-0.88f, 0.15f, -1.24f};
		const int Index = pSelf->m_TestSplitCounter++ % 3;
		N.m_Template = TEMPLATE_SPLIT;
		N.m_pIcon = FontIcon::FLAG_CHECKERED;
		N.m_Title = Index == 2 ? "Finish · PB" : "CP 3";
		N.m_Subtitle = Index == 2 ? "12:48.32" : "4:58.40";
		N.m_HasDelta = true;
		N.m_Delta = s_aDeltas[Index];
		N.m_Gold = Index == 2;
		pSelf->Push(N);
	}
	else if(str_comp(pName, "achievement") == 0)
	{
		N.m_Template = TEMPLATE_ACHIEVEMENT;
		N.m_pIcon = FontIcon::STAR;
		N.m_Title = "Night Owl";
		N.m_Subtitle = "Achievement · 5 sessions after 2 am";
		N.m_Color = XuiTheme::Color::YELLOW;
		pSelf->Push(N);
	}
	else if(str_comp(pName, "block") == 0)
	{
		N.m_Template = TEMPLATE_BLOCK;
		N.m_pIcon = FontIcon::ARROW_ROTATE_LEFT;
		N.m_Title = "REVENGE";
		N.m_Subtitle = "You blocked xX_Blocker_Xx back";
		N.m_Color = XuiTheme::Color::RED_SOFT;
		pSelf->Push(N);
	}
	else if(str_comp(pName, "clear") == 0)
	{
		pSelf->OnReset();
		pSelf->ClearActivity("test_music");
		pSelf->m_TestVote = STestVote();
		pSelf->m_TestDownload = STestDownload();
	}
	else
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "xx_island_test", "Unknown template. Use notify, vote, download, queue, music, split, achievement, block or clear");
	}
}
#endif

void CIsland::OnReset()
{
	m_Queue.clear();
	m_HasCurrent = false;
	m_MusicOpen = false;
	m_WasVoting = false;
	ClearQueue();
}

void CIsland::Push(const SNotification &Notification)
{
	if(!g_Config.m_XxIsland || !SourceEnabled(Notification.m_Source))
		return;

	auto It = std::find_if(m_Queue.begin(), m_Queue.end(), [&](const SNotification &Other) {
		return Other.m_Priority < Notification.m_Priority;
	});
	m_Queue.insert(It, Notification);

	if(m_HasCurrent && Notification.m_Priority > m_Current.m_Notification.m_Priority)
		m_Current.m_Duration = std::min(m_Current.m_Duration, m_Current.m_Elapsed + 0.15f);
}

void CIsland::SetActivity(const char *pId, const SActivity &Activity)
{
	for(auto &Entry : m_vActivities)
	{
		if(Entry.m_Id == pId)
		{
			Entry.m_Activity = Activity;
			return;
		}
	}
	m_vActivities.push_back({pId, Activity});
}

void CIsland::ClearActivity(const char *pId)
{
	m_vActivities.erase(std::remove_if(m_vActivities.begin(), m_vActivities.end(), [&](const SActivityEntry &Entry) {
		return Entry.m_Id == pId;
	}),
		m_vActivities.end());
}

void CIsland::SetQueue(int FreeSlots, const char *pServer, std::function<void()> Leave)
{
	m_QueueActive = true;
	m_QueueSlots = FreeSlots;
	m_QueueServer = pServer;
	m_QueueLeave = std::move(Leave);
}

void CIsland::ClearQueue()
{
	m_QueueActive = false;
	m_QueueLeave = nullptr;
}

const CIsland::SActivity *CIsland::TopActivity() const
{
	const SActivity *pTop = nullptr;
	for(const auto &Entry : m_vActivities)
	{
		if(!SourceEnabled(Entry.m_Activity.m_Source))
			continue;
		if(!pTop || Entry.m_Activity.m_Priority > pTop->m_Priority)
			pTop = &Entry.m_Activity;
	}
	return pTop;
}

void CIsland::NextNotification()
{
	m_HasCurrent = false;
	if(m_Queue.empty())
		return;

	m_Current.m_Notification = m_Queue.front();
	m_Queue.pop_front();
	m_Current.m_Elapsed = 0.0f;
	m_Current.m_Started = time_get();
	m_Current.m_Duration = m_Current.m_Notification.m_Duration * g_Config.m_XxIslandDuration / 100.0f;
	m_HasCurrent = true;
	m_NotificationSeq++;

	if(g_Config.m_XxIslandSounds && m_Current.m_Notification.m_Sound)
		GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_HIGHLIGHT, 0.5f);
}

bool CIsland::BuildDownload(SContent &Out, float Delta)
{
	if(m_TestDownload.m_Active)
	{
		m_TestDownload.m_Elapsed += Delta;
		if(m_TestDownload.m_Elapsed >= TEST_DOWNLOAD_SECONDS)
		{
			m_TestDownload.m_Active = false;
			SNotification N;
			N.m_pIcon = FontIcon::STAR;
			N.m_Title = "Connected";
			N.m_Subtitle = "Kobra 4";
			N.m_Color = AccentColor();
			Push(N);
			return false;
		}
		const float t = m_TestDownload.m_Elapsed / TEST_DOWNLOAD_SECONDS;
		const float Eased = t * t * (3.0f - 2.0f * t);
		Out.m_Shape = SHAPE_DOWNLOAD;
		Out.m_Key = 2000;
		Out.m_Real = false;
		Out.m_Fraction = Eased;
		Out.m_Title = "Downloading map";
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Kobra 4 · %.1f of 3.4 MiB", 3.4f * Eased);
		Out.m_Subtitle = aBuf;
		Out.m_Color = AccentColor();
		return true;
	}

	if(Client()->State() != IClient::STATE_LOADING || !g_Config.m_XxIslandSrcDownload)
		return false;
	const int Total = Client()->MapDownloadTotalsize();
	if(Total <= 0)
		return false;
	const int Amount = Client()->MapDownloadAmount();
	Out.m_Shape = SHAPE_DOWNLOAD;
	Out.m_Key = 2000;
	Out.m_Real = true;
	Out.m_Fraction = std::clamp(Amount / (float)Total, 0.0f, 1.0f);
	Out.m_Title = "Downloading map";
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s · %d of %d KiB", Client()->MapDownloadName(), Amount / 1024, Total / 1024);
	Out.m_Subtitle = aBuf;
	Out.m_Color = AccentColor();
	return true;
}

bool CIsland::BuildVote(SContent &Out, float Delta)
{
	if(m_TestVote.m_Active)
	{
		m_TestVote.m_Elapsed += Delta;
		const float Elapsed = m_TestVote.m_Elapsed;
		const int Yes = 3 + (int)(Elapsed * 0.9f) + (m_TestVote.m_Voted == 1);
		const int No = 1 + (int)(Elapsed * 0.4f) + (m_TestVote.m_Voted == -1);
		if(Elapsed >= TEST_VOTE_SECONDS)
		{
			m_TestVote.m_Active = false;
			SNotification N;
			N.m_pIcon = FontIcon::STAR;
			N.m_Title = Yes > No ? "Vote passed" : "Vote failed";
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Change map to Kobra 4 · %d : %d", Yes, No);
			N.m_Subtitle = aBuf;
			N.m_Color = AccentColor();
			Push(N);
			return false;
		}
		Out.m_Shape = SHAPE_VOTE;
		Out.m_Key = 1000 + m_VoteSeq;
		Out.m_Real = false;
		Out.m_Caption = "Vote · called by Kirito";
		Out.m_Title = "Change map to Kobra 4";
		Out.m_Yes = Yes;
		Out.m_No = No;
		Out.m_Voted = m_TestVote.m_Voted;
		Out.m_Seconds = (int)std::ceil(TEST_VOTE_SECONDS - Elapsed);
		Out.m_Fraction = Elapsed / TEST_VOTE_SECONDS;
		Out.m_Color = AccentColor();
		return true;
	}

	CVoting &Voting = GameClient()->m_Voting;
	const bool IsVoting = Voting.IsVoting() && Voting.SecondsLeft() >= 0;
	if(IsVoting && !m_WasVoting)
		m_VoteSeq++;
	m_WasVoting = IsVoting;
	if(!IsVoting)
		return false;

	Out.m_Shape = SHAPE_VOTE;
	Out.m_Key = 1000 + m_VoteSeq;
	Out.m_Real = true;
	Out.m_Caption = Voting.VoteReason()[0] != '\0' ? std::string("Vote · ") + Voting.VoteReason() : "Vote";
	Out.m_Title = Voting.VoteDescription();
	Out.m_Yes = Voting.Yes();
	Out.m_No = Voting.No();
	Out.m_Voted = Voting.TakenChoice();
	Out.m_Seconds = Voting.SecondsLeft() + 1;
	Out.m_Fraction = Voting.ElapsedFraction();
	Out.m_Color = AccentColor();
	return true;
}

void CIsland::BuildContent(SContent &Out, float Delta)
{
	if(m_HasCurrent)
		m_Current.m_Elapsed = (time_get() - m_Current.m_Started) / (float)time_freq();
	if(m_HasCurrent && m_Current.m_Elapsed >= m_Current.m_Duration)
		m_HasCurrent = false;
	if(!m_HasCurrent && !m_Queue.empty())
		NextNotification();

	if(BuildDownload(Out, Delta) || BuildVote(Out, Delta))
		return;

	if(m_QueueActive)
	{
		Out.m_Shape = SHAPE_QUEUE;
		Out.m_Key = 3000;
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), m_QueueSlots == 1 ? "Waiting for %d free slot" : "Waiting for %d free slots", m_QueueSlots);
		Out.m_Title = aBuf;
		Out.m_Subtitle = m_QueueServer;
		Out.m_Color = AccentColor();
		return;
	}

	if(m_HasCurrent)
	{
		const SNotification &N = m_Current.m_Notification;
		switch(N.m_Template)
		{
		case TEMPLATE_SPLIT: Out.m_Shape = SHAPE_SPLIT; break;
		case TEMPLATE_ACHIEVEMENT: Out.m_Shape = SHAPE_ACHIEVEMENT; break;
		case TEMPLATE_BLOCK: Out.m_Shape = SHAPE_BLOCK; break;
		default: Out.m_Shape = SHAPE_NOTIFY; break;
		}
		Out.m_Key = 100 + m_NotificationSeq;
		Out.m_pIcon = N.m_pIcon;
		Out.m_Title = N.m_Title;
		Out.m_Subtitle = N.m_Subtitle;
		Out.m_Color = N.m_Color;
		Out.m_HasDelta = N.m_HasDelta;
		Out.m_Delta = N.m_Delta;
		Out.m_Gold = N.m_Gold;
		Out.m_Fraction = std::clamp(1.0f - m_Current.m_Elapsed / std::max(m_Current.m_Duration, 0.01f), 0.0f, 1.0f);
		return;
	}

	const SActivity *pActivity = TopActivity();
	if(!pActivity)
		m_MusicOpen = false;
	if(m_MusicOpen && m_MusicOpenLimit > 0.0f)
	{
		m_MusicOpenElapsed += Delta;
		if(m_MusicOpenElapsed > m_MusicOpenLimit)
			m_MusicOpen = false;
	}
	if(pActivity)
	{
		Out.m_Shape = m_MusicOpen ? SHAPE_MUSIC : SHAPE_IDLE;
		Out.m_Key = m_MusicOpen ? 4000 : 5000;
		Out.m_Activity = *pActivity;
		Out.m_Title = pActivity->m_Text;
		Out.m_Color = pActivity->m_Color;
	}
}

bool CIsland::TimerText(char *pBuf, int BufSize, ColorRGBA &Color) const
{
	const CNetObj_GameInfo *pInfo = GameClient()->m_Snap.m_pGameInfoObj;
	if(!pInfo)
		return false;
	Color = IslandInk;
	if(pInfo->m_GameStateFlags & GAMESTATEFLAG_SUDDENDEATH)
	{
		str_copy(pBuf, "Sudden Death", BufSize);
		Color = ColorRGBA(1.0f, 0.25f, 0.25f, 1.0f);
		return true;
	}

	int Time;
	if(pInfo->m_TimeLimit && pInfo->m_WarmupTimer <= 0)
	{
		Time = pInfo->m_TimeLimit * 60 - ((Client()->GameTick(g_Config.m_ClDummy) - pInfo->m_RoundStartTick) / Client()->GameTickSpeed());
		if(pInfo->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER)
			Time = 0;
	}
	else if(pInfo->m_GameStateFlags & GAMESTATEFLAG_RACETIME)
	{
		Time = (Client()->GameTick(g_Config.m_ClDummy) + pInfo->m_WarmupTimer) / Client()->GameTickSpeed();
	}
	else
	{
		Time = (Client()->GameTick(g_Config.m_ClDummy) - pInfo->m_RoundStartTick) / Client()->GameTickSpeed();
	}

	str_time((int64_t)Time * 100, ETimeFormat::DAYS, pBuf, BufSize);
	if(pInfo->m_TimeLimit && Time <= 60 && pInfo->m_WarmupTimer <= 0)
	{
		const float Alpha = Time <= 10 && (2 * time() / time_freq()) % 2 ? 0.5f : 1.0f;
		Color = ColorRGBA(1.0f, 0.25f, 0.25f, Alpha);
	}
	return true;
}

void CIsland::ShapeSize(const SContent &Content, float &Width, float &Height, float &Radius) const
{
	using namespace XuiTheme::Island;
	switch(Content.m_Shape)
	{
	case SHAPE_IDLE:
		Width = std::clamp(16.0f + 18.0f + 10.0f + TextWidthOf(13.0f, WEIGHT_SEMIBOLD, Content.m_Title.c_str()) + 16.0f, 120.0f, 280.0f);
		Height = PILL_H;
		Radius = PILL_H / 2.0f;
		break;
	case SHAPE_NOTIFY:
	case SHAPE_BLOCK:
		Width = NOTIFY_W;
		Height = NOTIFY_H;
		Radius = NOTIFY_H / 2.0f;
		break;
	case SHAPE_ACHIEVEMENT:
		Width = NOTIFY_W - 40.0f;
		Height = NOTIFY_H;
		Radius = NOTIFY_H / 2.0f;
		break;
	case SHAPE_SPLIT:
		Width = SPLIT_W;
		Height = SPLIT_H;
		Radius = SPLIT_H / 2.0f;
		break;
	case SHAPE_VOTE:
		Width = VOTE_W;
		Height = VOTE_H;
		Radius = 40.0f;
		break;
	case SHAPE_DOWNLOAD:
	case SHAPE_QUEUE:
		Width = PROGRESS_W;
		Height = PROGRESS_H;
		Radius = 40.0f;
		break;
	case SHAPE_MUSIC:
		Width = CARD_W;
		Height = CARD_H;
		Radius = 44.0f;
		break;
	default:
		Width = TIMER_W * 0.8f;
		Height = PILL_H;
		Radius = PILL_H / 2.0f;
		break;
	}
}

void CIsland::OnRender()
{
	if(!g_Config.m_XxIsland || GameClient()->m_Xui.m_HudEditor)
		return;
	const int State = Client()->State();
	if(State != IClient::STATE_ONLINE && State != IClient::STATE_DEMOPLAYBACK && State != IClient::STATE_LOADING)
		return;

	const float Delta = std::clamp(Client()->RenderFrameTime(), 0.0f, 0.1f);
	const float Scale = g_Config.m_XxIslandScale / 100.0f;
	const CScreenRect Saved = Graphics()->GetScreen();
	m_MapHeight = 900.0f / Scale;
	m_MapWidth = 900.0f * Graphics()->ScreenAspect() / Scale;
	Graphics()->MapScreenToSize(m_MapWidth, m_MapHeight);
	TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.0f);

	RenderFrame(Delta);

	TextRender()->TextColor(TextRender()->DefaultTextColor());
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
	Graphics()->MapScreen(Saved);
}

void CIsland::RenderFrame(float Delta)
{
	using namespace XuiTheme;
	const int State = Client()->State();

	m_Scale = 1.0f;
	m_ContentAlpha = 1.0f;
	m_Interactive = false;
	m_CursorVisible = GameClient()->m_Menus.IsActive() || GameClient()->m_GameConsole.IsActive() || State == IClient::STATE_LOADING;
	m_Clicked = Input()->KeyPress(KEY_MOUSE_1);
	const vec2 Native = Input()->NativeMousePos();
	m_Mouse = vec2(Native.x / Graphics()->WindowWidth() * m_MapWidth, Native.y / Graphics()->WindowHeight() * m_MapHeight);

	const bool Blocked = GameClient()->m_Menus.IsActive() || GameClient()->m_Scoreboard.IsActive() || GameClient()->m_Statboard.IsActive();
	SContent Content;
	BuildContent(Content, Delta);
	const bool Hidden = (Blocked || State == IClient::STATE_LOADING) && Content.m_Shape != SHAPE_DOWNLOAD;

	if(Content.m_Key != m_Shown.m_Key || Content.m_Shape != m_Shown.m_Shape)
	{
		if(m_Shown.m_Shape != SHAPE_NONE)
		{
			const float t = std::clamp((m_ShownAge - Island::CONTENT_DELAY) / Island::CONTENT_FADE, 0.0f, 1.0f);
			float Radius;
			ShapeSize(m_Shown, m_PreviousWidth, m_PreviousHeight, Radius);
			m_Previous = m_Shown;
			m_PreviousAlpha = t * t * (3.0f - 2.0f * t);
		}
		m_ShownAge = 0.0f;
	}
	else
	{
		m_ShownAge += Delta;
	}
	m_Shown = Content;
	m_PreviousAlpha = std::max(0.0f, m_PreviousAlpha - Delta / 0.12f);

	float TargetW, TargetH, TargetR;
	ShapeSize(Content, TargetW, TargetH, TargetR);

	char aTimer[192];
	ColorRGBA TimerColor = IslandInk;
	const bool Online = State == IClient::STATE_ONLINE || State == IClient::STATE_DEMOPLAYBACK;
	bool TimerAvailable = g_Config.m_XxIslandIdle && g_Config.m_ClShowhud && g_Config.m_ClShowhudTimer && Online && TimerText(aTimer, sizeof(aTimer), TimerColor);
	char aExtras[128];
	if(Online && g_Config.m_ClShowhud && GameClient()->m_Xui.IslandExtras(aExtras, sizeof(aExtras)))
	{
		if(!TimerAvailable)
		{
			aTimer[0] = '\0';
			TimerColor = IslandInk;
		}
		else
			str_append(aTimer, "  ·  ");
		str_append(aTimer, aExtras);
		TimerAvailable = true;
	}
	const bool TimerShown = TimerAvailable && !Hidden && (Content.m_Shape == SHAPE_NONE || Content.m_Shape == SHAPE_IDLE);
	float TimerTextW = 0.0f;
	float TimerTargetW = 0.0f;
	if(TimerAvailable)
	{
		TimerTextW = TextWidthOf(13.0f, WEIGHT_BOLD, aTimer);
		TimerTargetW = std::max(Island::TIMER_W, 14.0f + 6.0f + TimerTextW + 28.0f);
	}

	for(SPill *pPill : {&m_Main, &m_Timer})
	{
		if(pPill->m_Init)
			continue;
		pPill->m_Init = true;
		pPill->m_Width.Snap(0.0f);
		pPill->m_Height.Snap(Island::PILL_H);
		pPill->m_Radius.Snap(Island::PILL_H / 2.0f);
	}

	m_Main.m_Width.Update(TargetW, Delta, Motion::ISLAND);
	m_Main.m_Height.Update(TargetH, Delta, Motion::ISLAND);
	m_Main.m_Radius.Update(TargetR, Delta, Motion::ISLAND);
	m_Main.m_Alpha = Approach(m_Main.m_Alpha, Content.m_Shape != SHAPE_NONE && !Hidden ? 1.0f : 0.0f, 16.0f, Delta);
	m_Timer.m_Width.Update(TimerShown ? TimerTargetW : 0.0f, Delta, Motion::ISLAND);
	m_Timer.m_Height.Update(Island::PILL_H, Delta, Motion::ISLAND);
	m_Timer.m_Radius.Update(Island::PILL_H / 2.0f, Delta, Motion::ISLAND);
	m_Timer.m_Alpha = Approach(m_Timer.m_Alpha, TimerShown ? 1.0f : 0.0f, 16.0f, Delta);

	const float WidthA = m_Main.m_Alpha > 0.01f ? std::max(0.0f, m_Main.m_Width.m_Value) : 0.0f;
	const float WidthB = m_Timer.m_Alpha > 0.01f ? std::max(0.0f, m_Timer.m_Width.m_Value) : 0.0f;
	if(WidthA <= 1.0f && WidthB <= 1.0f)
		return;
	const float Gap = Island::GAP * std::clamp(std::min(WidthA, WidthB) / 20.0f, 0.0f, 1.0f);
	const float TotalW = WidthA + Gap + WidthB;
	vec2 Anchor(0.5f, 0.0f);
	GameClient()->m_Xui.HudModulePos("island", &Anchor);
	const float Margin = (float)g_Config.m_XxIslandOffset;
	const float x0 = mix(Margin, m_MapWidth - Margin - TotalW, Anchor.x);
	const float y = mix(Margin, m_MapHeight - Margin - Island::PILL_H, Anchor.y);

	if(WidthA > 1.0f)
	{
		const float h = std::max(0.0f, m_Main.m_Height.m_Value);
		if(m_CursorVisible && m_Clicked && Content.m_Shape == SHAPE_IDLE && Hovered(x0, y, WidthA, h))
			m_MusicOpen = true;

		SPill Pill = m_Main;
		Pill.m_Width.m_Value = WidthA;
		RenderPill(Pill, x0, y);

		const float cx = x0 + WidthA / 2.0f;
		const float cy = y + h / 2.0f;
		if(m_PreviousAlpha > 0.01f)
			RenderContent(m_Previous, cx - m_PreviousWidth / 2.0f, cy - m_PreviousHeight / 2.0f, m_PreviousWidth, m_PreviousHeight, 10.0f, m_PreviousAlpha * m_Main.m_Alpha * IslandFit(WidthA, m_PreviousWidth), false);
		if(Content.m_Shape != SHAPE_NONE)
		{
			const float t = std::clamp((m_ShownAge - Island::CONTENT_DELAY) / Island::CONTENT_FADE, 0.0f, 1.0f);
			const float Alpha = t * t * (3.0f - 2.0f * t);
			RenderContent(Content, cx - TargetW / 2.0f, cy - TargetH / 2.0f, TargetW, TargetH, m_ShownAge, Alpha * m_Main.m_Alpha * IslandFit(WidthA, TargetW), Alpha > 0.6f);
		}
	}

	if(WidthB > 1.0f)
	{
		SPill Pill = m_Timer;
		Pill.m_Width.m_Value = WidthB;
		const float x = x0 + WidthA + Gap;
		RenderPill(Pill, x, y);
		if(TimerAvailable)
		{
			m_Cx = x + WidthB / 2.0f;
			m_Cy = y + Island::PILL_H / 2.0f;
			m_Scale = 1.0f;
			m_ContentAlpha = m_Timer.m_Alpha * IslandFit(WidthB, TimerTargetW);
			const float Total = 14.0f + 6.0f + TimerTextW;
			const float Left = m_Cx - Total / 2.0f;
			Icon(Left + 7.0f, m_Cy, 13.0f, TimerColor, FontIcon::FLAG_CHECKERED);
			Label(Left + 20.0f, m_Cy, 13.0f, WEIGHT_BOLD, TimerColor, aTimer);
			m_ContentAlpha = 1.0f;
		}
	}
}

void CIsland::RenderPill(const SPill &Pill, float x, float y)
{
	using namespace XuiTheme;
	const float w = Pill.m_Width.m_Value;
	const float h = std::max(0.0f, Pill.m_Height.m_Value);
	const float r = std::clamp(Pill.m_Radius.m_Value, 0.0f, std::min(w, h) / 2.0f);
	IGraphics::SUiRect Rect{x, y, w, h};
	Rect.m_Radii = vec4(r, r, r, r);
	Rect.m_Fill = ColorRGBA(0.05f, 0.05f, 0.07f, 0.78f * Pill.m_Alpha);
	Rect.m_Fill2 = ColorRGBA(0.03f, 0.03f, 0.05f, 0.72f * Pill.m_Alpha);
	Rect.m_GradientAngle = 90.0f;
	Rect.m_Glass = Pill.m_Alpha;
	Rect.m_Refraction = 1.0f;
	Rect.m_BorderWidth = XuiTheme::Glass::RIM_WIDTH;
	Rect.m_Border = ColorRGBA(1.0f, 1.0f, 1.0f, 0.16f * Pill.m_Alpha);
	Rect.m_Border2 = ColorRGBA(1.0f, 1.0f, 1.0f, 0.04f * Pill.m_Alpha);
	Rect.m_BorderAngle = XuiTheme::Glass::RIM_ANGLE;
	Rect.m_ShadowOffset = vec2(0.0f, Island::SHADOW_Y);
	Rect.m_ShadowBlur = Island::SHADOW_BLUR;
	Rect.m_Shadow = Island::SHADOW.WithMultipliedAlpha(Pill.m_Alpha);
	Graphics()->DrawUiRects(&Rect, 1);
}

void CIsland::RenderContent(const SContent &Content, float x, float y, float w, float h, float Age, float Alpha, bool Interactive)
{
	using namespace XuiTheme;
	const float t = std::clamp((Age - Island::CONTENT_DELAY) / Island::CONTENT_FADE, 0.0f, 1.0f);
	const float Ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
	m_Cx = x + w / 2.0f;
	m_Cy = y + h / 2.0f;
	m_Scale = Age >= 10.0f ? 1.0f : Island::CONTENT_SCALE_FROM + (1.0f - Island::CONTENT_SCALE_FROM) * Ease;
	m_ContentAlpha = Alpha;
	m_Interactive = Interactive;
	if(Alpha < 0.005f)
		return;

	switch(Content.m_Shape)
	{
	case SHAPE_IDLE: RenderIdle(Content, x, y, w, h); break;
	case SHAPE_NOTIFY:
	case SHAPE_ACHIEVEMENT:
	case SHAPE_BLOCK: RenderNotify(Content, x, y, w, h, Age); break;
	case SHAPE_SPLIT: RenderSplit(Content, x, y, w, h); break;
	case SHAPE_VOTE: RenderVote(Content, x, y, w, h); break;
	case SHAPE_DOWNLOAD:
	case SHAPE_QUEUE: RenderProgress(Content, x, y, w, h); break;
	case SHAPE_MUSIC: RenderMusic(Content, x, y, w, h); break;
	default: break;
	}
	m_Interactive = false;
	m_Scale = 1.0f;
	m_ContentAlpha = 1.0f;
}

void CIsland::RenderIdle(const SContent &Content, float x, float y, float w, float h)
{
	using namespace XuiTheme;
	const float cy = y + h / 2.0f;
	Equaliser(x + 16.0f, cy, Content.m_Color, Content.m_Activity.m_Playing);
	Label(x + 16.0f + 18.0f + 10.0f, cy, 13.0f, WEIGHT_SEMIBOLD, IslandInk, Content.m_Title.c_str(), w - 16.0f - 18.0f - 10.0f - 16.0f);
}

void CIsland::RenderNotify(const SContent &Content, float x, float y, float w, float h, float Age)
{
	using namespace XuiTheme;
	const float cy = y + h / 2.0f;
	const bool Block = Content.m_Shape == SHAPE_BLOCK;
	const float BubbleCx = x + 16.0f + 27.0f;

	if(Content.m_Shape == SHAPE_ACHIEVEMENT)
	{
		const float t = std::clamp((Age - Island::CONTENT_DELAY) / 0.9f, 0.0f, 1.0f);
		const float Out = 1.0f - (1.0f - t) * (1.0f - t);
		for(int i = 0; i < 12; i++)
		{
			const float Angle = i / 12.0f * 2.0f * PI;
			const float Dist = 29.0f + 8.0f * Out;
			const float Size = 4.5f * (1.0f - t);
			if(Size <= 0.1f)
				continue;
			const ColorRGBA Spark = Mix(Color::YELLOW, Color::ORANGE, (i % 3) / 2.0f);
			Box(BubbleCx + std::cos(Angle) * Dist - Size / 2.0f, cy + std::sin(Angle) * Dist - Size / 2.0f, Size, Size, Size / 2.0f, Spark.WithMultipliedAlpha(1.0f - t * 0.6f));
		}
	}

	Box(x + 16.0f, cy - 27.0f, 54.0f, 54.0f, 27.0f, Content.m_Color.WithAlpha(Block ? 0.26f : 0.18f));
	if(Content.m_pIcon)
		Icon(BubbleCx, cy, 24.0f, Content.m_Color, Content.m_pIcon);

	float Right = x + w - 16.0f;
	if(m_CursorVisible && m_Interactive)
	{
		if(Button(Right - 28.0f, cy - 14.0f, 28.0f, 28.0f, 14.0f, Color::GHOST, Color::GHOST_HOVER) && m_HasCurrent)
			m_Current.m_Elapsed = m_Current.m_Duration;
		Icon(Right - 14.0f, cy, 12.0f, IslandInk, FontIcon::XMARK);
		Right -= 28.0f + 12.0f;
	}
	else
	{
		Right -= 8.0f;
	}

	const float TextX = x + 84.0f;
	const float MaxW = Right - TextX;
	const bool HasSub = !Content.m_Subtitle.empty();
	Label(TextX, HasSub ? cy - 8.0f : cy, 16.0f, WEIGHT_BOLD, Block ? Content.m_Color : IslandInk, Content.m_Title.c_str(), MaxW);
	if(HasSub)
		Label(TextX, cy + 10.0f, 13.0f, WEIGHT_REGULAR, IslandInk2, Content.m_Subtitle.c_str(), MaxW);

	const float BarW = w - 84.0f - 24.0f;
	Box(TextX, y + h - 12.0f, BarW, 3.0f, 1.5f, Island::PROGRESS_TRACK);
	if(Content.m_Fraction > 0.0f)
		Box(TextX, y + h - 12.0f, std::max(3.0f, BarW * Content.m_Fraction), 3.0f, 1.5f, Content.m_Color);
}

void CIsland::RenderSplit(const SContent &Content, float x, float y, float w, float h)
{
	using namespace XuiTheme;
	const float cy = y + h / 2.0f;
	ColorRGBA Delta = Content.m_Delta <= 0.0f ? Color::GREEN : Color::RED_SOFT;
	if(Content.m_Gold)
		Delta = Color::YELLOW;

	Box(x + 14.0f, cy - 22.0f, 44.0f, 44.0f, 22.0f, Delta.WithAlpha(0.18f));
	Icon(x + 36.0f, cy, 20.0f, Delta, Content.m_pIcon ? Content.m_pIcon : FontIcon::FLAG_CHECKERED);

	float Right = x + w - 24.0f;
	if(Content.m_HasDelta)
	{
		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "%s%.2f", Content.m_Delta <= 0.0f ? "\xE2\x88\x92" : "+", std::fabs(Content.m_Delta));
		Label(Right, cy, 26.0f, WEIGHT_BOLD, Delta, aBuf, -1.0f, 1);
		Right -= TextWidthOf(26.0f, WEIGHT_BOLD, aBuf) + 14.0f;
	}
	const float TextX = x + 72.0f;
	Label(TextX, cy - 8.0f, 15.0f, WEIGHT_BOLD, IslandInk, Content.m_Title.c_str(), Right - TextX);
	Label(TextX, cy + 10.0f, 13.0f, WEIGHT_REGULAR, IslandInk2, Content.m_Subtitle.c_str(), Right - TextX);

	const float BarW = w - 72.0f - 24.0f;
	Box(TextX, y + h - 9.0f, BarW, 3.0f, 1.5f, Island::PROGRESS_TRACK);
	if(Content.m_Fraction > 0.0f)
		Box(TextX, y + h - 9.0f, std::max(3.0f, BarW * Content.m_Fraction), 3.0f, 1.5f, Delta);
}

void CIsland::RenderVote(const SContent &Content, float x, float y, float w, float h)
{
	using namespace XuiTheme;
	const float RingCy = y + 13.0f + 22.0f;
	const float RingCx = x + 20.0f + 22.0f;
	Ring(RingCx, RingCy, 18.0f, 4.0f, 0.0f, 360.0f, Island::RING_TRACK);
	Ring(RingCx, RingCy, 18.0f, 4.0f, 360.0f * std::clamp(Content.m_Fraction, 0.0f, 0.995f), 360.0f, Content.m_Color);
	char aBuf[16];
	str_format(aBuf, sizeof(aBuf), "%d", Content.m_Seconds);
	Label(RingCx, RingCy, 13.0f, WEIGHT_BOLD, IslandInk, aBuf, -1.0f, 0);

	const float TextX = x + 20.0f + 44.0f + 14.0f;
	Label(TextX, RingCy - 11.0f, 12.0f, WEIGHT_SEMIBOLD, IslandInk3, Content.m_Caption.c_str(), x + w - 20.0f - TextX);
	Label(TextX, RingCy + 11.0f, 17.0f, WEIGHT_BOLD, IslandInk, Content.m_Title.c_str(), x + w - 20.0f - TextX);

	const float ButtonW = (w - 40.0f - 8.0f) / 2.0f;
	const float ButtonY = y + 13.0f + 44.0f + 12.0f;
	for(int i = 0; i < 2; i++)
	{
		const bool Yes = i == 0;
		const int Choice = Yes ? 1 : -1;
		const bool Chosen = Content.m_Voted == Choice;
		const ColorRGBA ChosenFill = Yes ? Color::GREEN : ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		const float bx = x + 20.0f + i * (ButtonW + 8.0f);
		const ColorRGBA Fill = Chosen ? ChosenFill : ColorRGBA(0.463f, 0.463f, 0.502f, 0.24f);
		const ColorRGBA FillHover = Chosen ? ChosenFill : ColorRGBA(0.463f, 0.463f, 0.502f, 0.34f);
		if(Button(bx, ButtonY, ButtonW, 38.0f, 19.0f, Fill, FillHover))
		{
			if(Content.m_Real)
				GameClient()->m_Voting.Vote(Choice);
			else
				m_TestVote.m_Voted = Choice;
		}
		const ColorRGBA TextColor = Chosen ? ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f) : IslandInk;
		char aCount[16];
		str_format(aCount, sizeof(aCount), "%d", Yes ? Content.m_Yes : Content.m_No);
		const char *pKey = Yes ? "F3" : "F4";
		const char *pLabel = Yes ? "Yes" : "No";
		const float LabelW = TextWidthOf(14.0f, WEIGHT_SEMIBOLD, pLabel);
		const float CountW = TextWidthOf(13.0f, WEIGHT_BOLD, aCount);
		const float ChipW = std::max(26.0f, TextWidthOf(10.5f, WEIGHT_SEMIBOLD, pKey) + 12.0f);
		const float Total = LabelW + 6.0f + CountW + 8.0f + ChipW;
		float cx = bx + (ButtonW - Total) / 2.0f;
		const float cy = ButtonY + 19.0f;
		Label(cx, cy, 14.0f, WEIGHT_SEMIBOLD, TextColor, pLabel);
		cx += LabelW + 6.0f;
		Label(cx, cy, 13.0f, WEIGHT_BOLD, TextColor.WithMultipliedAlpha(0.7f), aCount);
		cx += CountW + 8.0f;
		Box(cx, cy - 10.0f, ChipW, 20.0f, 10.0f, Chosen ? ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.12f));
		Label(cx + ChipW / 2.0f, cy, 10.5f, WEIGHT_SEMIBOLD, TextColor, pKey, -1.0f, 0);
	}
}

void CIsland::RenderProgress(const SContent &Content, float x, float y, float w, float h)
{
	using namespace XuiTheme;
	const bool Download = Content.m_Shape == SHAPE_DOWNLOAD;
	const float cy = y + h / 2.0f;
	const float RingCx = x + 16.0f + 24.0f;
	Ring(RingCx, cy, 19.5f, 4.0f, 0.0f, 360.0f, Island::RING_TRACK);
	if(Download)
	{
		Ring(RingCx, cy, 19.5f, 4.0f, 0.0f, 360.0f * std::clamp(Content.m_Fraction, 0.003f, 1.0f), Content.m_Color);
		char aBuf[16];
		str_format(aBuf, sizeof(aBuf), "%d%%", (int)(Content.m_Fraction * 100.0f));
		Label(RingCx, cy, 12.0f, WEIGHT_BOLD, IslandInk, aBuf, -1.0f, 0);
	}
	else
	{
		const float Start = std::fmod(Client()->GlobalTime(), 1.0f) * 360.0f;
		Ring(RingCx, cy, 19.5f, 4.0f, Start, Start + 97.0f, Content.m_Color);
	}

	const char *pButton = Download ? "Cancel" : "Leave queue";
	const float ButtonW = TextWidthOf(13.0f, WEIGHT_SEMIBOLD, pButton) + 28.0f;
	const float ButtonX = x + w - 16.0f - ButtonW;
	if(Button(ButtonX, cy - 18.0f, ButtonW, 36.0f, 18.0f, Color::GHOST, Color::GHOST_HOVER))
	{
		if(Download)
		{
			if(Content.m_Real)
				Client()->Disconnect();
			else
				m_TestDownload.m_Active = false;
		}
		else
		{
			std::function<void()> Leave = m_QueueLeave;
			ClearQueue();
			if(Leave)
				Leave();
		}
	}
	Label(ButtonX + ButtonW / 2.0f, cy, 13.0f, WEIGHT_SEMIBOLD, IslandInk, pButton, -1.0f, 0);

	const float TextX = x + 16.0f + 48.0f + 14.0f;
	Label(TextX, cy - 8.0f, 15.0f, WEIGHT_BOLD, IslandInk, Content.m_Title.c_str(), ButtonX - 14.0f - TextX);
	Label(TextX, cy + 10.0f, 13.0f, WEIGHT_REGULAR, IslandInk2, Content.m_Subtitle.c_str(), ButtonX - 14.0f - TextX);
}

void CIsland::RenderMusic(const SContent &Content, float x, float y, float w, float h)
{
	using namespace XuiTheme;
	const SActivity &A = Content.m_Activity;
	const ColorRGBA Accent = Content.m_Color;

	if(Button(x + w - 18.0f - 28.0f, y + 16.0f, 28.0f, 28.0f, 14.0f, Color::GHOST, Color::GHOST_HOVER))
	{
		m_MusicOpen = false;
		m_MusicOpenLimit = -1.0f;
	}
	Icon(x + w - 18.0f - 14.0f, y + 30.0f, 12.0f, IslandInk, FontIcon::CHEVRON_UP);

	const float CoverY = y + (h - 132.0f) / 2.0f + 12.0f;
	const float CoverX = x + 18.0f;
	if(A.m_Cover.IsValid())
	{
		const float S = m_Scale;
		Graphics()->TextureSet(A.m_Cover);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, m_ContentAlpha);
		const IGraphics::CQuadItem Quad(m_Cx + (CoverX - m_Cx) * S, m_Cy + (CoverY - m_Cy) * S, 132.0f * S, 132.0f * S);
		Graphics()->QuadsDrawTL(&Quad, 1);
		Graphics()->QuadsEnd();
		Graphics()->TextureClear();
	}
	else
	{
		Box(CoverX, CoverY, 132.0f, 132.0f, 26.0f, Mix(Accent, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), 0.45f), ColorRGBA(Accent.r * 0.12f, Accent.g * 0.12f, Accent.b * 0.12f, 1.0f));
		Icon(CoverX + 66.0f, CoverY + 66.0f, 56.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.9f), FontIcon::MUSIC);
	}

	const float ColX = CoverX + 132.0f + 18.0f;
	const float ColW = x + w - 18.0f - ColX;
	Label(ColX, CoverY + 12.0f, 20.0f, WEIGHT_BOLD, IslandInk, A.m_Text.c_str(), ColW);
	Label(ColX, CoverY + 34.0f, 14.0f, WEIGHT_REGULAR, IslandInk2, A.m_Artist.c_str(), ColW);

	const float Fraction = A.m_Length > 0 ? std::clamp(A.m_Position / (float)A.m_Length, 0.0f, 1.0f) : std::max(0.0f, A.m_Progress);
	Box(ColX, CoverY + 48.0f, ColW, 5.0f, 2.5f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.14f));
	if(Fraction > 0.0f)
		Box(ColX, CoverY + 48.0f, std::max(5.0f, ColW * Fraction), 5.0f, 2.5f, IslandInk);
	char aPos[16], aLen[16];
	FormatClock(aPos, sizeof(aPos), A.m_Position);
	FormatClock(aLen, sizeof(aLen), A.m_Length);
	Label(ColX, CoverY + 66.0f, 11.5f, WEIGHT_REGULAR, IslandInk3, aPos);
	Label(ColX + ColW, CoverY + 66.0f, 11.5f, WEIGHT_REGULAR, IslandInk3, aLen, -1.0f, 1);

	const float RowY = CoverY + 108.0f;
	float bx = ColX + (ColW - 176.0f) / 2.0f;
	const struct
	{
		float m_Size;
		int m_Control;
		const char *m_pIcon;
		float m_IconSize;
		float m_FillAlpha;
	} aButtons[] = {
		{40.0f, MUSIC_PREVIOUS, FontIcon::BACKWARD_STEP, 18.0f, 0.0f},
		{52.0f, MUSIC_TOGGLE, A.m_Playing ? FontIcon::PAUSE : FontIcon::PLAY, 22.0f, 0.12f},
		{40.0f, MUSIC_NEXT, FontIcon::FORWARD_STEP, 18.0f, 0.0f},
	};
	for(const auto &Entry : aButtons)
	{
		if(Button(bx, RowY - Entry.m_Size / 2.0f, Entry.m_Size, Entry.m_Size, Entry.m_Size / 2.0f, ColorRGBA(1.0f, 1.0f, 1.0f, Entry.m_FillAlpha), ColorRGBA(1.0f, 1.0f, 1.0f, Entry.m_FillAlpha + 0.1f)) && A.m_Control)
			A.m_Control(Entry.m_Control);
		Icon(bx + Entry.m_Size / 2.0f, RowY, Entry.m_IconSize, IslandInk, Entry.m_pIcon);
		bx += Entry.m_Size + 22.0f;
	}
}

float CIsland::TextWidthOf(float Size, int Weight, const char *pText) const
{
	(void)Weight;
	return TextRender()->TextWidth(Size, pText, -1, -1.0f);
}

void CIsland::Label(float x, float cy, float Size, int Weight, ColorRGBA Color, const char *pText, float MaxWidth, int Align)
{
	if(!pText || pText[0] == '\0')
		return;
	float Width = TextWidthOf(Size, Weight, pText);
	if(MaxWidth > 0.0f)
		Width = std::min(Width, MaxWidth);
	if(Align == 0)
		x -= Width / 2.0f;
	else if(Align > 0)
		x -= Width;

	const float S = m_Scale;
	const float X = m_Cx + (x - m_Cx) * S;
	const float Y = m_Cy + (cy - m_Cy) * S - Size * S * 0.5f;
	TextRender()->TextColor(Color.WithMultipliedAlpha(m_ContentAlpha));
	TextRender()->TextOutlineColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f));
	const int Passes = Weight >= WEIGHT_SEMIBOLD ? 2 : 1;
	for(int Pass = 0; Pass < Passes; Pass++)
	{
		CTextCursor Cursor;
		Cursor.m_Flags = TEXTFLAG_RENDER | TEXTFLAG_ELLIPSIS_AT_END;
		Cursor.m_FontSize = Size * S;
		Cursor.m_LineWidth = MaxWidth > 0.0f ? MaxWidth * S : -1.0f;
		Cursor.m_MaxLines = 1;
		Cursor.SetPosition(vec2(X + Pass * Size * S * 0.03f, Y));
		TextRender()->TextEx(&Cursor, pText);
	}
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
}

void CIsland::Icon(float cx, float cy, float Size, ColorRGBA Color, const char *pIcon)
{
	const float S = m_Scale;
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	const float Width = TextRender()->TextWidth(Size * S, pIcon, -1, -1.0f);
	TextRender()->TextColor(Color.WithMultipliedAlpha(m_ContentAlpha));
	TextRender()->TextOutlineColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f));
	TextRender()->Text(m_Cx + (cx - m_Cx) * S - Width / 2.0f, m_Cy + (cy - m_Cy) * S - Size * S * 0.5f, Size * S, pIcon, -1.0f);
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
}

void CIsland::Box(float x, float y, float w, float h, float Radius, ColorRGBA Fill, ColorRGBA Fill2)
{
	const float S = m_Scale;
	IGraphics::SUiRect Rect{m_Cx + (x - m_Cx) * S, m_Cy + (y - m_Cy) * S, w * S, h * S};
	const float r = std::min(Radius, std::min(w, h) / 2.0f) * S;
	Rect.m_Radii = vec4(r, r, r, r);
	Rect.m_Fill = Fill.WithMultipliedAlpha(m_ContentAlpha);
	if(Fill2.a >= 0.0f)
		Rect.m_Fill2 = Fill2.WithMultipliedAlpha(m_ContentAlpha);
	Graphics()->DrawUiRects(&Rect, 1);
}

void CIsland::Ring(float cx, float cy, float Radius, float Stroke, float From, float To, ColorRGBA Color)
{
	const float S = m_Scale;
	const float Length = (To - From) / 360.0f * 2.0f * PI * Radius;
	const int Steps = std::max(1, (int)std::ceil(std::fabs(Length) / (Stroke * 0.3f)));
	const float Dot = Stroke * S;
	const float Fade = Color.a * m_ContentAlpha;
	const ColorRGBA Solid(Color.r * Fade, Color.g * Fade, Color.b * Fade, 1.0f);
	std::vector<IGraphics::SUiRect> vDots;
	vDots.reserve(Steps + 1);
	for(int i = 0; i <= Steps; i++)
	{
		const float Angle = (From + (To - From) * i / Steps) * PI / 180.0f - PI / 2.0f;
		const float px = cx + std::cos(Angle) * Radius;
		const float py = cy + std::sin(Angle) * Radius;
		IGraphics::SUiRect Rect{m_Cx + (px - m_Cx) * S - Dot / 2.0f, m_Cy + (py - m_Cy) * S - Dot / 2.0f, Dot, Dot};
		Rect.m_Radii = vec4(Dot / 2.0f, Dot / 2.0f, Dot / 2.0f, Dot / 2.0f);
		Rect.m_Fill = Solid;
		vDots.push_back(Rect);
	}
	Graphics()->DrawUiRects(vDots.data(), (int)vDots.size());
}

void CIsland::Equaliser(float x, float cy, ColorRGBA Color, bool Playing)
{
	static const float s_aDelays[] = {0.0f, 0.25f, 0.5f, 0.15f};
	const float Now = Client()->GlobalTime();
	for(int i = 0; i < 4; i++)
	{
		float Height = 4.0f;
		if(Playing)
			Height = 4.0f + 10.0f * (0.5f - 0.5f * std::cos((Now - s_aDelays[i]) / 1.1f * 2.0f * PI));
		Box(x + i * 5.0f, cy - Height / 2.0f, 3.0f, Height, 1.5f, Color);
	}
}

bool CIsland::Hovered(float x, float y, float w, float h) const
{
	const float S = m_Scale;
	const float X = m_Cx + (x - m_Cx) * S;
	const float Y = m_Cy + (y - m_Cy) * S;
	return m_Mouse.x >= X && m_Mouse.x <= X + w * S && m_Mouse.y >= Y && m_Mouse.y <= Y + h * S;
}

bool CIsland::Button(float x, float y, float w, float h, float Radius, ColorRGBA Fill, ColorRGBA FillHover)
{
	const bool Hover = m_Interactive && m_CursorVisible && Hovered(x, y, w, h);
	Box(x, y, w, h, Radius, Hover ? FillHover : Fill);
	return Hover && m_Clicked;
}
