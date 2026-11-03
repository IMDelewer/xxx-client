#include <base/str.h>

#include <engine/font_icons_phosphor.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/xui/xui.h>
#include <game/localization.h>
#include <game/voting.h>

#include <string>
#include <vector>

using namespace Xui;

namespace
{
enum
{
	VOTE_SERVER = 0,
	VOTE_KICK,
	VOTE_SPEC,
};

int CheckboxPrefix(const char *pText)
{
	const unsigned char *p = (const unsigned char *)pText;
	if(p[0] == 0xE2 && p[1] == 0x98 && p[2] == 0x90)
		return 1;
	if(p[0] == 0xE2 && p[1] == 0x98 && (p[2] == 0x91 || p[2] == 0x92))
		return 2;
	if(p[0] == '[' && (p[1] == ' ' || p[1] == 'x' || p[1] == 'X') && p[2] == ']')
		return p[1] == ' ' ? 1 : 2;
	return 0;
}

bool IsSeparator(const char *pDescription)
{
	const char *p = pDescription;
	while(*p == ' ')
		p++;
	if(*p == '\0')
		return true;
	int Marks = 0;
	for(const char *q = p; *q; q++)
		if(*q == '-' || *q == '=' || *q == '_' || *q == '*' || *q == '#' || *q == '~' || (unsigned char)*q == 0xE2)
			Marks++;
	return Marks >= 3 && (p[0] == '-' || p[0] == '=' || p[0] == '_' || p[0] == '*' || p[0] == '#' || p[0] == '~' || (unsigned char)p[0] == 0xE2);
}

bool StripTree(const char *pText, std::string &Out)
{
	const unsigned char *p = (const unsigned char *)pText;
	bool Tree = false;
	while(true)
	{
		if(p[0] == 0xE2 && (p[1] == 0x94 || p[1] == 0x95) && p[2])
		{
			p += 3;
			Tree = true;
		}
		else if(*p == ' ' || *p == '-' || *p == '|')
			p++;
		else
			break;
	}
	if(!Tree)
		return false;
	Out = (const char *)p;
	return true;
}

void CleanSeparator(const char *pDescription, char *pOut, int Size)
{
	int n = 0;
	for(const char *p = pDescription; *p && n < Size - 1; p++)
	{
		const unsigned char c = (unsigned char)*p;
		if(c == '-' || c == '=' || c == '_' || c == '*' || c == '#' || c == '~' || c == '|')
			continue;
		if(c == 0xE2 && p[1] && p[2])
		{
			p += 2;
			continue;
		}
		pOut[n++] = *p;
	}
	pOut[n] = '\0';
	for(const char *pEmpty : {"[]", "()", "[ ]"})
	{
		char *pFound;
		while((pFound = (char *)str_find(pOut, pEmpty)) != nullptr)
			memmove(pFound, pFound + str_length(pEmpty), str_length(pFound + str_length(pEmpty)) + 1);
	}
	str_utf8_trim_right(pOut);
	const char *pStart = str_utf8_skip_whitespaces(pOut);
	if(pStart != pOut)
		memmove(pOut, pStart, str_length(pStart) + 1);
}
}

void CXui::RenderVoteTab(CMenus &M)
{
	CGameClient *pGame = GameClient();
	CVoting &Voting = pGame->m_Voting;
	static int s_Mode = VOTE_SERVER;
	static int s_Selected = -1;
	static char s_aFilter[64] = "";
	static char s_aReason[64] = "";
	static int s_LastMode = -1;
	if(s_LastMode != s_Mode)
	{
		s_Selected = -1;
		s_LastMode = s_Mode;
	}

	if(Voting.IsVoting() && Voting.SecondsLeft() >= 0)
	{
		SBox Card;
		Card.m_W = Grow();
		Card.m_Padding = SEdges(12.0f, 14.0f);
		Card.m_Gap = 10.0f;
		const int CardNode = Column(Card, [&]() {
			SBox Head;
			Head.m_W = Grow();
			Head.m_Gap = 12.0f;
			Head.m_Cross = EAlign::CENTER;
			Row(Head, [&]() {
				char aSeconds[16];
				str_format(aSeconds, sizeof(aSeconds), "%d", Voting.SecondsLeft() + 1);
				RingProgress(1.0f - Voting.ElapsedFraction(), 40.0f, aSeconds);
				SBox Names;
				Names.m_W = Grow();
				Names.m_Shrink = true;
				Names.m_Gap = 2.0f;
				Column(Names, [&]() {
					STextStyle Cap;
					Cap.m_Size = 12.0f;
					Cap.m_Font = EFont::SEMIBOLD;
					Cap.m_Color = XuiTheme::Color::TEXT3;
					Cap.m_Wrap = true;
					Cap.m_MaxLines = 1;
					SBox GB;
					GB.m_W = Grow();
					char aCaption[160];
					if(Voting.VoteReason()[0] != '\0')
						str_format(aCaption, sizeof(aCaption), "%s · %s", Localize("Current vote"), Voting.VoteReason());
					else
						str_copy(aCaption, Localize("Current vote"));
					Text(aCaption, Cap, GB);
					STextStyle T;
					T.m_Size = 16.0f;
					T.m_Font = EFont::BOLD;
					T.m_Wrap = true;
					T.m_MaxLines = 2;
					Text(Voting.VoteDescription(), T, GB);
				});
			});

			const int Yes = Voting.Yes();
			const int No = Voting.No();
			const int Total = std::max(Voting.Total(), Yes + No);
			const float YesF = Total > 0 ? (float)Yes / Total : 0.0f;
			const float NoF = Total > 0 ? (float)No / Total : 0.0f;
			SBox Bar;
			Bar.m_W = Grow();
			Bar.m_H = Px(8.0f);
			const ColorRGBA Green = XuiTheme::Color::GREEN;
			const ColorRGBA Red = XuiTheme::Color::RED_SOFT;
			Paint(Row(Bar), [YesF, NoF, Green, Red](CXui &U, const SRect &Rc) {
				U.DrawRect(Rc, SolidRect(ColorRGBA(1, 1, 1, 0.08f), Rc.m_H * 0.5f));
				if(YesF > 0.0f)
					U.DrawRect({Rc.m_X, Rc.m_Y, std::max(Rc.m_H, Rc.m_W * YesF), Rc.m_H}, SolidRect(Green, Rc.m_H * 0.5f));
				if(NoF > 0.0f)
				{
					const float W = std::max(Rc.m_H, Rc.m_W * NoF);
					U.DrawRect({Rc.Right() - W, Rc.m_Y, W, Rc.m_H}, SolidRect(Red, Rc.m_H * 0.5f));
				}
			});
			SBox Counts;
			Counts.m_W = Grow();
			Counts.m_Cross = EAlign::CENTER;
			Row(Counts, [&]() {
				STextStyle S;
				S.m_Size = 12.0f;
				S.m_Font = EFont::SEMIBOLD;
				S.m_Tabular = true;
				char aBuf[64];
				S.m_Color = Green;
				str_format(aBuf, sizeof(aBuf), Localize("%d yes"), Yes);
				Text(aBuf, S);
				Spacer();
				S.m_Color = XuiTheme::Color::TEXT3;
				str_format(aBuf, sizeof(aBuf), Localize("%d not voted"), std::max(0, Total - Yes - No));
				Text(aBuf, S);
				Spacer();
				S.m_Color = Red;
				str_format(aBuf, sizeof(aBuf), Localize("%d no"), No);
				Text(aBuf, S);
			});

			SBox Buttons;
			Buttons.m_W = Grow();
			Buttons.m_Gap = 8.0f;
			Row(Buttons, [&]() {
				const int Taken = Voting.TakenChoice();
				if(Button(Localize("Yes"), {.m_Variant = Taken == 1 ? EVariant::ACCENT : EVariant::GHOST, .m_pIcon = Taken == 1 ? Ph::CHECK : Ph::THUMBS_UP, .m_Height = 40.0f, .m_FontSize = 14.0f, .m_W = Grow(), .m_pKey = "vt.yes"}).m_Clicked)
					Voting.Vote(1);
				if(Button(Localize("No"), {.m_Variant = Taken == -1 ? EVariant::GLASS : EVariant::GHOST, .m_pIcon = Taken == -1 ? Ph::CHECK : Ph::THUMBS_DOWN, .m_Height = 40.0f, .m_FontSize = 14.0f, .m_W = Grow(), .m_pKey = "vt.no"}).m_Clicked)
					Voting.Vote(-1);
			});
		});
		Paint(CardNode, [](CXui &U, const SRect &Rc) { U.DrawRect(Rc, GroupRect(20.0f)); });
	}

	const char *apModes[] = {Localize("Server"), Localize("Kick"), Localize("Spectate")};
	SBox Top;
	Top.m_W = Grow();
	Top.m_Gap = 10.0f;
	Top.m_Cross = EAlign::CENTER;
	Row(Top, [&]() {
		Segmented("vt.mode", apModes, 3, &s_Mode);
		SearchField("vt.filter", s_aFilter, sizeof(s_aFilter), Localize("Search"), Grow());
	});

	enum class EKind
	{
		OPTION,
		HEADER,
		SPACER,
	};
	struct SItem
	{
		int m_Id;
		EKind m_Kind;
		std::string m_Label;
		const CGameClient::CClientData *m_pClient;
		int m_Checkbox;
		bool m_Info = false;
	};
	std::vector<SItem> vItems;
	std::vector<SItem> vTabs;
	if(s_Mode == VOTE_SERVER)
	{
		int Index = 0;
		bool Leading = true;
		for(const CVoteOptionClient *pOption = Voting.FirstOption(); pOption; pOption = pOption->m_pNext, Index++)
		{
			const int Checkbox = CheckboxPrefix(pOption->m_aDescription);
			if(Leading && Checkbox != 0 && !IsSeparator(pOption->m_aDescription))
			{
				vTabs.push_back({Index, EKind::OPTION, pOption->m_aDescription, nullptr, Checkbox});
				continue;
			}
			if(Leading)
			{
				Leading = false;
				if(vTabs.size() < 2)
				{
					for(const SItem &Tab : vTabs)
						vItems.push_back(Tab);
					vTabs.clear();
				}
			}
			if(IsSeparator(pOption->m_aDescription))
			{
				char aClean[VOTE_DESC_LENGTH];
				CleanSeparator(pOption->m_aDescription, aClean, sizeof(aClean));
				if(s_aFilter[0] == '\0')
					vItems.push_back({-1, aClean[0] != '\0' ? EKind::HEADER : EKind::SPACER, aClean, nullptr, 0});
				continue;
			}
			if(s_aFilter[0] != '\0' && !str_utf8_find_nocase(pOption->m_aDescription, s_aFilter))
				continue;
			std::string Stripped;
			if(StripTree(pOption->m_aDescription, Stripped))
			{
				if(str_utf8_skip_whitespaces(Stripped.c_str())[0] == '\0')
				{
					if(s_aFilter[0] == '\0')
						vItems.push_back({-1, EKind::SPACER, "", nullptr, 0});
					continue;
				}
				SItem Item{Index, EKind::OPTION, Stripped, nullptr, CheckboxPrefix(Stripped.c_str())};
				Item.m_Info = true;
				vItems.push_back(Item);
				continue;
			}
			vItems.push_back({Index, EKind::OPTION, pOption->m_aDescription, nullptr, Checkbox});
		}
		if(Leading && vTabs.size() < 2)
		{
			for(const SItem &Tab : vTabs)
				vItems.push_back(Tab);
			vTabs.clear();
		}
		while(!vItems.empty() && vItems.front().m_Kind == EKind::SPACER)
			vItems.erase(vItems.begin());
		for(size_t i = 0; i < vItems.size(); i++)
		{
			const bool NextIsHeader = i + 1 >= vItems.size() || vItems[i + 1].m_Kind == EKind::HEADER;
			if(vItems[i].m_Kind == EKind::HEADER && NextIsHeader)
				vItems.erase(vItems.begin() + i--);
			else if(vItems[i].m_Kind == EKind::SPACER && i > 0 && vItems[i - 1].m_Kind == EKind::SPACER)
				vItems.erase(vItems.begin() + i--);
		}
	}
	else
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			const CNetObj_PlayerInfo *pInfo = pGame->m_Snap.m_apInfoByName[i];
			if(pInfo == nullptr || pInfo->m_ClientId == pGame->m_Snap.m_LocalClientId)
				continue;
			const CGameClient::CClientData &C = pGame->m_aClients[pInfo->m_ClientId];
			if(s_aFilter[0] != '\0' && !str_utf8_find_nocase(C.m_aName, s_aFilter) && !str_utf8_find_nocase(C.m_aClan, s_aFilter))
				continue;
			vItems.push_back({pInfo->m_ClientId, EKind::OPTION, C.m_aName, &C, 0});
		}
	}

	bool ActivateMenu = false;
	auto CallSelected = [&]() {
		if(s_Selected < 0)
			return;
		if(s_Mode == VOTE_SERVER)
		{
			Voting.CallvoteOption(s_Selected, s_aReason);
			s_aReason[0] = '\0';
			if(g_Config.m_UiCloseWindowAfterChangingSetting && !ActivateMenu)
				M.SetActive(false);
			return;
		}
		else if(s_Mode == VOTE_KICK)
			Voting.CallvoteKick(s_Selected, s_aReason);
		else
			Voting.CallvoteSpectate(s_Selected, s_aReason);
		s_aReason[0] = '\0';
		s_Selected = -1;
		M.SetActive(false);
	};

	bool Activate = false;
	const ColorRGBA Acc = m_Accent;
	const ColorRGBA OnAcc = m_OnAccent;
	auto LabelOf = [](const SItem &Item) {
		const char *pLabel = Item.m_Label.c_str();
		if(Item.m_Checkbox != 0)
			pLabel = str_utf8_skip_whitespaces(pLabel + 3);
		return pLabel;
	};

	if(!vTabs.empty())
	{
		SBox TabRow;
		TabRow.m_W = Grow();
		TabRow.m_Gap = 6.0f;
		TabRow.m_Wrap = true;
		Row(TabRow, [&]() {
			for(const SItem &Tab : vTabs)
			{
				char aKey[24];
				str_format(aKey, sizeof(aKey), "vtab%d", Tab.m_Id);
				const uint32_t Id = MakeId(aKey);
				const SResult R = Interact(Id);
				const float Hover = State(Id).m_Hover.Update(R.m_Hovered ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
				const bool On = Tab.m_Checkbox == 2;
				if(R.m_Clicked && !On)
				{
					s_Selected = Tab.m_Id;
					ActivateMenu = true;
					Activate = true;
				}
				SBox B;
				B.m_H = Px(34.0f);
				B.m_Padding = SEdges(0.0f, 16.0f);
				B.m_Cross = EAlign::CENTER;
				B.m_Hit = EHit::INTERACTIVE;
				const int Node = BoxId(Id, EDir::ROW, B, [&]() {
					STextStyle S;
					S.m_Size = 13.5f;
					S.m_Font = On ? EFont::BOLD : EFont::SEMIBOLD;
					S.m_Color = On ? OnAcc : Mix(XuiTheme::Color::TEXT2, XuiTheme::Color::TEXT, Hover);
					Text(LabelOf(Tab), S);
				});
				Paint(Node, [On, Hover, Acc](CXui &U, const SRect &Rc) {
					U.DrawRect(Rc, On ? AccentRect(Acc, Rc.m_H * 0.5f) : SolidRect(Mix(XuiTheme::Color::GHOST, XuiTheme::Color::GHOST_HOVER, Hover), Rc.m_H * 0.5f));
				});
			}
		});
	}

	auto OptionCell = [&](const SItem &Item, SSize Width) {
		char aKey[24];
		str_format(aKey, sizeof(aKey), "vt%d.%d", s_Mode, Item.m_Id);
		const uint32_t Id = MakeId(aKey);
		const SResult R = Interact(Id);
		SState &St = State(Id);
		const float Hover = St.m_Hover.Update(R.m_Hovered ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
		const bool Selected = s_Selected == Item.m_Id;
		const int Checkbox = Item.m_Checkbox;
		if(R.m_Clicked)
		{
			if((Selected && m_Time - St.m_aScratch[0] < 0.4f) || Checkbox != 0)
				Activate = true;
			ActivateMenu = Checkbox != 0;
			s_Selected = Item.m_Id;
			St.m_aScratch[0] = m_Time;
		}
		SBox Cell;
		Cell.m_W = Width;
		Cell.m_Shrink = true;
		Cell.m_H = Px(Item.m_pClient ? 46.0f : (Item.m_Info ? 32.0f : 42.0f));
		Cell.m_Gap = 10.0f;
		Cell.m_Cross = EAlign::CENTER;
		Cell.m_Padding = Item.m_Info ? SEdges(0.0f, 14.0f, 0.0f, 30.0f) : SEdges(0.0f, 14.0f);
		Cell.m_Hit = EHit::INTERACTIVE;
		const int Node = BoxId(Id, EDir::ROW, Cell, [&]() {
			if(Item.m_pClient)
			{
				SBox TeeBox;
				TeeBox.m_W = Px(32.0f);
				TeeBox.m_H = Px(32.0f);
				const CTeeRenderInfo TeeInfo = Item.m_pClient->m_RenderInfo;
				Paint(Row(TeeBox), [TeeInfo](CXui &U, const SRect &Rc) { U.DrawTee(TeeInfo, 0, vec2(1.0f, 0.0f), Rc); });
			}
			STextStyle L;
			L.m_Size = Item.m_Info ? 13.0f : 14.0f;
			L.m_Font = Selected || Checkbox == 2 ? EFont::SEMIBOLD : EFont::MEDIUM;
			L.m_Color = Checkbox == 1 || Item.m_Info ? XuiTheme::Color::TEXT2 : XuiTheme::Color::TEXT;
			L.m_Wrap = true;
			L.m_MaxLines = 1;
			SBox LB;
			LB.m_W = Grow();
			LB.m_Shrink = true;
			Text(LabelOf(Item), L, LB);
			if(Item.m_pClient && Item.m_pClient->m_aClan[0] != '\0')
			{
				STextStyle C;
				C.m_Size = 12.5f;
				C.m_Color = XuiTheme::Color::TEXT3;
				Text(Item.m_pClient->m_aClan, C);
			}
			if(Checkbox != 0)
			{
				SBox Track;
				Track.m_W = Px(34.0f);
				Track.m_H = Px(20.0f);
				const bool On = Checkbox == 2;
				Paint(Row(Track), [On, Acc, OnAcc](CXui &U, const SRect &Rc) {
					U.DrawRect(Rc, SolidRect(On ? Acc : XuiTheme::Color::TRACK_OFF, Rc.m_H * 0.5f));
					const float K = Rc.m_H - 4.0f;
					U.DrawRect({On ? Rc.Right() - K - 2.0f : Rc.m_X + 2.0f, Rc.m_Y + 2.0f, K, K}, SolidRect(On ? Mix(OnAcc, XuiTheme::Color::KNOB, 0.25f) : XuiTheme::Color::KNOB, K * 0.5f));
				});
			}
			else if(Selected)
				Icon(Ph::CHECK_CIRCLE, 18.0f, Acc, true);
			else if(!Item.m_Info)
				Icon(Ph::CARET_RIGHT, 14.0f, WithAlpha(XuiTheme::Color::TEXT3, 0.4f + 0.6f * Hover));
		});
		const bool Highlight = Selected && Checkbox == 0;
		const bool Info = Item.m_Info;
		Paint(Node, [Hover, Highlight, Acc, Info](CXui &U, const SRect &Rc) {
			if(Info)
			{
				U.DrawRect({Rc.m_X + 14.0f, Rc.m_Y + 4.0f, 2.0f, Rc.m_H - 8.0f}, SolidRect(ColorRGBA(1, 1, 1, 0.14f), 1.0f));
				if(Hover > 0.01f || Highlight)
					U.DrawRect(Rc, SolidRect(Highlight ? WithAlpha(Acc, 0.14f) : ColorRGBA(1, 1, 1, 0.04f * Hover), 10.0f));
				return;
			}
			if(Highlight)
				U.DrawRect(Rc, SolidRect(WithAlpha(Acc, 0.2f), 14.0f));
			else
				U.DrawRect(Rc, SolidRect(ColorRGBA(1, 1, 1, 0.035f + 0.05f * Hover), 14.0f));
		});
	};

	SBox ListBox;
	ListBox.m_W = Grow();
	ListBox.m_H = Px(std::clamp(m_ScreenUnits.y - (Voting.IsVoting() ? 740.0f : 580.0f) - (vTabs.empty() ? 0.0f : 44.0f), 140.0f, 640.0f));
	ListBox.m_Gap = 4.0f;
	ListBox.m_Cross = EAlign::STRETCH;
	Scroll(ListBox, [&]() {
		if(vItems.empty())
		{
			SBox E;
			E.m_Padding = SEdges(24.0f, 12.0f);
			E.m_Main = EAlign::CENTER;
			Row(E, [&]() {
				STextStyle S;
				S.m_Size = 14.0f;
				S.m_Color = XuiTheme::Color::TEXT3;
				if(s_aFilter[0] != '\0')
					Text(Localize("Nothing found"), S);
				else if(s_Mode == VOTE_SERVER)
					Text(Localize("This server has no vote options"), S);
				else
					Text(Localize("Nobody else is here"), S);
			});
			return;
		}
		auto Short = [&](size_t i) { return i < vItems.size() && vItems[i].m_Kind == EKind::OPTION && !vItems[i].m_Info && str_length(LabelOf(vItems[i])) <= 40; };
		for(size_t i = 0; i < vItems.size(); i++)
		{
			const SItem &Item = vItems[i];
			if(Item.m_Kind == EKind::SPACER)
			{
				SBox Sp;
				Sp.m_H = Px(8.0f);
				Row(Sp);
				continue;
			}
			if(Item.m_Kind == EKind::HEADER)
			{
				SBox H;
				H.m_Padding = SEdges(i == 0 ? 2.0f : 14.0f, 8.0f, 4.0f, 8.0f);
				H.m_Gap = 10.0f;
				H.m_Cross = EAlign::CENTER;
				H.m_W = Grow();
				Row(H, [&]() {
					STextStyle S;
					S.m_Size = 12.0f;
					S.m_Font = EFont::BOLD;
					S.m_Color = XuiTheme::Color::TEXT2;
					Text(Item.m_Label.c_str(), S);
					SBox Line;
					Line.m_W = Grow();
					Line.m_H = Px(1.0f);
					Paint(Row(Line), [](CXui &U, const SRect &Rc) { U.DrawRect(Rc, SolidRect(ColorRGBA(1, 1, 1, 0.08f), 0.0f)); });
				});
				continue;
			}
			if(Item.m_pClient == nullptr && Short(i) && Short(i + 1))
			{
				SBox Pair;
				Pair.m_W = Grow();
				Pair.m_Gap = 4.0f;
				Row(Pair, [&]() {
					OptionCell(vItems[i], Grow());
					OptionCell(vItems[i + 1], Grow());
				});
				i++;
				continue;
			}
			OptionCell(Item, Grow());
		}
	});

	SBox Bottom;
	Bottom.m_W = Grow();
	Bottom.m_Gap = 10.0f;
	Bottom.m_Cross = EAlign::CENTER;
	Row(Bottom, [&]() {
		const SResult Reason = Field("vt.reason", s_aReason, sizeof(s_aReason), {.m_pPlaceholder = Localize("Reason (optional)"), .m_pIcon = Ph::CHAT_CIRCLE, .m_W = Grow(), .m_Clearable = true});
		if(Reason.m_Submitted)
			Activate = true;
		const char *pLabel = s_Mode == VOTE_KICK ? Localize("Vote kick") : (s_Mode == VOTE_SPEC ? Localize("Vote spectate") : Localize("Call vote"));
		if(Button(pLabel, {.m_Variant = EVariant::ACCENT, .m_pIcon = Ph::CHECK_SQUARE, .m_Disabled = s_Selected < 0 || Voting.IsVoting(), .m_pKey = "vt.call"}).m_Clicked)
			Activate = true;
	});

	if(Activate && (!Voting.IsVoting() || ActivateMenu))
		CallSelected();
}
