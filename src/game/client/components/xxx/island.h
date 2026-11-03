#ifndef GAME_CLIENT_COMPONENTS_XXX_ISLAND_H
#define GAME_CLIENT_COMPONENTS_XXX_ISLAND_H

#include <base/color.h>

#include <engine/console.h>
#include <engine/graphics.h>

#include <game/client/component.h>
#include <game/client/xui/spring.h>

#include <deque>
#include <functional>
#include <string>
#include <vector>

class CIsland : public CComponent
{
public:
	enum
	{
		PRIORITY_LOW = 0,
		PRIORITY_NORMAL = 1,
		PRIORITY_HIGH = 2,
	};

	enum ESource
	{
		SRC_NONE = -1,
		SRC_MUSIC,
		SRC_SPLITS,
		SRC_CLIPS,
		SRC_ACHIEVEMENTS,
		SRC_BLOCK,
		SRC_FRIENDS,
		SRC_CHAT,
		SRC_VOTES,
		SRC_DOWNLOAD,
	};

	enum ETemplate
	{
		TEMPLATE_NOTIFY,
		TEMPLATE_SPLIT,
		TEMPLATE_ACHIEVEMENT,
		TEMPLATE_BLOCK,
	};

	enum EMusicControl
	{
		MUSIC_PREVIOUS,
		MUSIC_TOGGLE,
		MUSIC_NEXT,
	};

	struct SNotification
	{
		int m_Template = TEMPLATE_NOTIFY;
		int m_Source = SRC_NONE;
		const char *m_pIcon = nullptr;
		std::string m_Title;
		std::string m_Subtitle;
		ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		float m_Duration = 4.2f;
		int m_Priority = PRIORITY_NORMAL;
		bool m_Sound = true;
		bool m_HasDelta = false;
		float m_Delta = 0.0f;
		bool m_Gold = false;
	};

	struct SActivity
	{
		int m_Source = SRC_MUSIC;
		const char *m_pIcon = nullptr;
		std::string m_Text;
		std::string m_Artist;
		ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		float m_Progress = -1.0f;
		int m_Position = 0;
		int m_Length = 0;
		bool m_Playing = true;
		int m_Priority = PRIORITY_NORMAL;
		IGraphics::CTextureHandle m_Cover;
		std::function<void(int)> m_Control;
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnReset() override;
	void OnRender() override;

	void Push(const SNotification &Notification);
	void SetActivity(const char *pId, const SActivity &Activity);
	void ClearActivity(const char *pId);
	void SetQueue(int FreeSlots, const char *pServer, std::function<void()> Leave);
	void ClearQueue();

private:
	enum EShape
	{
		SHAPE_NONE,
		SHAPE_IDLE,
		SHAPE_NOTIFY,
		SHAPE_SPLIT,
		SHAPE_ACHIEVEMENT,
		SHAPE_BLOCK,
		SHAPE_VOTE,
		SHAPE_DOWNLOAD,
		SHAPE_QUEUE,
		SHAPE_MUSIC,
	};

	struct SActivityEntry
	{
		std::string m_Id;
		SActivity m_Activity;
	};

	struct SContent
	{
		int m_Shape = SHAPE_NONE;
		int m_Key = 0;
		const char *m_pIcon = nullptr;
		std::string m_Title;
		std::string m_Subtitle;
		std::string m_Caption;
		ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		float m_Fraction = 0.0f;
		float m_Delta = 0.0f;
		bool m_HasDelta = false;
		bool m_Gold = false;
		int m_Yes = 0;
		int m_No = 0;
		int m_Voted = 0;
		int m_Seconds = 0;
		bool m_Real = false;
		SActivity m_Activity;
	};

	struct SPill
	{
		CSpring m_Width;
		CSpring m_Height;
		CSpring m_Radius;
		float m_Alpha = 0.0f;
		bool m_Init = false;
	};

	struct SCurrent
	{
		int64_t m_Started = 0;
		SNotification m_Notification;
		float m_Elapsed = 0.0f;
		float m_Duration = 0.0f;
	};

	struct STestVote
	{
		bool m_Active = false;
		float m_Elapsed = 0.0f;
		int m_Voted = 0;
	};

	struct STestDownload
	{
		bool m_Active = false;
		float m_Elapsed = 0.0f;
	};

	std::deque<SNotification> m_Queue;
	std::vector<SActivityEntry> m_vActivities;
	SCurrent m_Current;
	bool m_HasCurrent = false;
	int m_NotificationSeq = 0;
	int m_VoteSeq = 0;
	bool m_WasVoting = false;

	bool m_QueueActive = false;
	int m_QueueSlots = 0;
	std::string m_QueueServer;
	std::function<void()> m_QueueLeave;

	bool m_MusicOpen = false;
	float m_MusicOpenLimit = -1.0f;
	float m_MusicOpenElapsed = 0.0f;
	STestVote m_TestVote;
	STestDownload m_TestDownload;
	int m_TestSplitCounter = 0;

	SPill m_Main;
	SPill m_Timer;
	SContent m_Shown;
	SContent m_Previous;
	float m_ShownAge = 0.0f;
	float m_PreviousAlpha = 0.0f;
	float m_PreviousWidth = 0.0f;
	float m_PreviousHeight = 0.0f;
	float m_MapWidth = 0.0f;
	float m_MapHeight = 0.0f;

	float m_Cx = 0.0f;
	float m_Cy = 0.0f;
	float m_Scale = 1.0f;
	float m_ContentAlpha = 1.0f;
	bool m_Interactive = false;
	bool m_CursorVisible = false;
	bool m_Clicked = false;
	vec2 m_Mouse = vec2(-1.0f, -1.0f);

	static bool SourceEnabled(int Source);
	const SActivity *TopActivity() const;
	void NextNotification();
	void BuildContent(SContent &Out, float Delta);
	bool BuildDownload(SContent &Out, float Delta);
	bool BuildVote(SContent &Out, float Delta);
	bool TimerText(char *pBuf, int BufSize, ColorRGBA &Color) const;
	void ShapeSize(const SContent &Content, float &Width, float &Height, float &Radius) const;

	void RenderFrame(float Delta);
	void RenderPill(const SPill &Pill, float x, float y);
	void RenderContent(const SContent &Content, float x, float y, float w, float h, float Age, float Alpha, bool Interactive);
	void RenderTimer(float x, float y, float w, float h, float Alpha);
	void RenderIdle(const SContent &Content, float x, float y, float w, float h);
	void RenderNotify(const SContent &Content, float x, float y, float w, float h, float Age);
	void RenderSplit(const SContent &Content, float x, float y, float w, float h);
	void RenderVote(const SContent &Content, float x, float y, float w, float h);
	void RenderProgress(const SContent &Content, float x, float y, float w, float h);
	void RenderMusic(const SContent &Content, float x, float y, float w, float h);

	float TextWidthOf(float Size, int Weight, const char *pText) const;
	void Label(float x, float cy, float Size, int Weight, ColorRGBA Color, const char *pText, float MaxWidth = -1.0f, int Align = -1);
	void Icon(float cx, float cy, float Size, ColorRGBA Color, const char *pIcon);

	void Box(float x, float y, float w, float h, float Radius, ColorRGBA Fill, ColorRGBA Fill2 = ColorRGBA(0.0f, 0.0f, 0.0f, -1.0f));
	void Ring(float cx, float cy, float Radius, float Stroke, float From, float To, ColorRGBA Color);
	void Equaliser(float x, float cy, ColorRGBA Color, bool Playing);
	bool Button(float x, float y, float w, float h, float Radius, ColorRGBA Fill, ColorRGBA FillHover);
	bool Hovered(float x, float y, float w, float h) const;

	static void ConIslandTest(IConsole::IResult *pResult, void *pUserData);
};

#endif
