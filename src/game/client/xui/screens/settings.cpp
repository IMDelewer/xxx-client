#include "settings.h"

#include "settings_internal.h"

#include <base/str.h>

#include <engine/font_icons_phosphor.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/xui/xui.h>
#include <game/localization.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>

using namespace Xui;

namespace SettingsUi
{
	static CGameClient *gs_pGame = nullptr;

	static CXui *gs_pUi = nullptr;

	CGameClient *Game() { return gs_pGame; }
	CXui &Ui() { return *gs_pUi; }
	void SetContext(CGameClient *pGame, CXui *pUi)
	{
		gs_pGame = pGame;
		gs_pUi = pUi;
	}

	static bool IsModified(const SRow &R)
	{
		long long Default;
		switch(R.m_Type)
		{
		case SRow::EType::TOGGLE:
		case SRow::EType::SLIDER:
		case SRow::EType::SEGMENT:
		case SRow::EType::DROPDOWN:
			return R.m_pInt != nullptr && DefaultInt(R.m_pInt, Default) && *R.m_pInt != Default;
		case SRow::EType::COLOR:
			return R.m_pColor != nullptr && DefaultInt(R.m_pColor, Default) && (long long)*R.m_pColor != Default;
		case SRow::EType::TEXT:
		{
			std::string Str;
			return DefaultString(R.m_pText, Str) && Str != R.m_pText;
		}
		default:
			return false;
		}
	}

	static void ResetRow(SRow &R)
	{
		long long Default;
		std::string Str;
		if(R.m_pInt != nullptr && DefaultInt(R.m_pInt, Default))
			*R.m_pInt = (int)Default;
		else if(R.m_pColor != nullptr && DefaultInt(R.m_pColor, Default))
			*R.m_pColor = (unsigned)Default;
		else if(R.m_pText != nullptr && DefaultString(R.m_pText, Str))
			str_copy(R.m_pText, Str.c_str(), R.m_TextSize);
		if(R.m_Fn && R.m_Type != SRow::EType::ACTION && R.m_Type != SRow::EType::CUSTOM)
			R.m_Fn();
	}

	void SliderControl(CXui &Ui, const char *pKey, int *pValue, int Min, int Max, const char *pUnit, const char *pZero)
	{
		SBox B;
		B.m_Gap = 14.0f;
		B.m_Cross = EAlign::CENTER;
		Ui.Row(B, [&]() {
			Ui.Slider(pKey, pValue, {.m_Min = (float)Min, .m_Max = (float)Max, .m_Width = 180.0f});
			char aValue[32];
			if(pZero != nullptr && *pValue == 0)
				str_copy(aValue, Localize(pZero));
			else
				str_format(aValue, sizeof(aValue), "%d%s", *pValue, pUnit);
			STextStyle Value;
			Value.m_Size = 13.0f;
			Value.m_Color = XuiTheme::Color::TEXT2;
			Value.m_Align = ETextAlign::RIGHT;
			Value.m_Tabular = true;
			SBox VB;
			VB.m_W = Px(pZero != nullptr && *pValue == 0 ? 110.0f : 64.0f);
			Ui.Text(aValue, Value, VB);
		});
	}

	static std::map<std::string, bool> &DropdownOpen()
	{
		static std::map<std::string, bool> s_Open;
		return s_Open;
	}

	static std::map<std::string, uint32_t> &DropdownAnchor()
	{
		static std::map<std::string, uint32_t> s_Anchor;
		return s_Anchor;
	}

	void Dropdown(CXui &Ui, const char *pKey, const std::vector<std::string> &vOptions, int *pValue, float Width, const char *pIcon)
	{
		const int Count = (int)vOptions.size();
		const int Current = std::clamp(*pValue, 0, std::max(0, Count - 1));
		bool &Open = DropdownOpen()[pKey];
		const SResult R = Ui.Button(Count > 0 ? vOptions[Current].c_str() : "", {.m_Variant = Open ? EVariant::GLASS : EVariant::GHOST, .m_pIcon = pIcon != nullptr ? pIcon : Ph::CARET_UP_DOWN, .m_W = Fit(0.0f, Width), .m_pKey = pKey});
		DropdownAnchor()[pKey] = R.m_Id;
		if(R.m_Clicked)
			Open = !Open;
	}

	bool DropdownOptions(CXui &Ui, const char *pKey, const std::vector<std::string> &vOptions, int *pValue)
	{
		bool &Open = DropdownOpen()[pKey];
		if(!Open)
			return false;
		bool Changed = false;
		char aPop[112];
		str_format(aPop, sizeof(aPop), "%s.pop", pKey);
		Ui.Popover(aPop, &Open, DropdownAnchor()[pKey], [&]() {
			SBox List;
			List.m_W = Px(260.0f);
			List.m_H = Fit(0.0f, 340.0f);
			List.m_Gap = 2.0f;
			Ui.Scroll(List, [&]() {
				for(int i = 0; i < (int)vOptions.size(); i++)
				{
					char aItem[112];
					str_format(aItem, sizeof(aItem), "%s.opt%d", pKey, i);
					const uint32_t Id = Ui.MakeId(aItem);
					const SResult R = Ui.Interact(Id);
					const bool Selected = i == *pValue;
					const float Hover = Ui.State(Id).m_Hover.Update(R.m_Hovered ? 1.0f : 0.0f, Ui.Dt(), XuiTheme::Motion::HOVER);
					if(R.m_Clicked)
					{
						*pValue = i;
						Open = false;
						Changed = true;
					}
					SBox Item;
					Item.m_W = Grow();
					Item.m_H = Px(36.0f);
					Item.m_Gap = 10.0f;
					Item.m_Cross = EAlign::CENTER;
					Item.m_Padding = SEdges(0.0f, 14.0f);
					Item.m_Hit = EHit::INTERACTIVE;
					const ColorRGBA Acc = Ui.Accent();
					const int Node = Ui.BoxId(Id, EDir::ROW, Item, [&]() {
						STextStyle S;
						S.m_Size = 14.0f;
						S.m_Font = Selected ? EFont::SEMIBOLD : EFont::REGULAR;
						S.m_Color = Selected ? XuiTheme::Color::TEXT : XuiTheme::Color::TEXT2;
						SBox TB;
						TB.m_W = Grow();
						Ui.Text(vOptions[i].c_str(), S, TB);
						if(Selected)
							Ui.Icon(Ph::CHECK, 15.0f, Acc);
					});
					Ui.Paint(Node, [Selected, Hover, Acc](CXui &U, const SRect &Rc) {
						U.DrawRect(Rc, CXui::SolidRect(Selected ? CXui::WithAlpha(Acc, 0.16f) : ColorRGBA(1, 1, 1, 0.06f * Hover), 10.0f));
					});
				}
			});
		});
		return Changed;
	}

	static void ResetDot(CXui &Ui, const char *pKey, SRow &R)
	{
		char aKey[96];
		str_format(aKey, sizeof(aKey), "%s.reset", pKey);
		SBox B;
		B.m_W = Px(18.0f);
		B.m_H = Px(18.0f);
		B.m_Main = EAlign::CENTER;
		B.m_Cross = EAlign::CENTER;
		B.m_Hit = EHit::INTERACTIVE;
		const uint32_t Id = Ui.MakeId(aKey);
		const SResult Res = Ui.Interact(Id);
		const int Node = Ui.BoxId(Id, EDir::ROW, B);
		const bool Hover = Res.m_Hovered;
		const ColorRGBA Accent = Ui.Accent();
		Ui.Paint(Node, [Accent, Hover](CXui &U, const SRect &Rc) {
			const float D = Hover ? 10.0f : 8.0f;
			U.DrawRect({Rc.CenterX() - D * 0.5f, Rc.CenterY() - D * 0.5f, D, D}, CXui::SolidRect(Accent, D * 0.5f));
		});
		Ui.Tooltip(Id, Localize("Reset to default"));
		if(Res.m_Clicked)
			ResetRow(R);
	}

	static int FindBoundKey(const char *pCommand)
	{
		CGameClient *pGame = Game();
		for(int Key = KEY_A; Key < KEY_LAST; Key++)
		{
			const char *pBind = pGame->m_Binds.Get(CBindSlot(Key, KeyModifier::NONE));
			if(pBind != nullptr && pBind[0] != '\0' && str_comp(pBind, pCommand) == 0)
				return Key;
		}
		return 0;
	}

	static void RenderControl(CXui &Ui, SRow &R, const char *pKey)
	{
		switch(R.m_Type)
		{
		case SRow::EType::TOGGLE:
		{
			int Value = R.m_pInt != nullptr ? *R.m_pInt : R.m_Get();
			if(Ui.Switch(pKey, &Value).m_Changed)
			{
				if(R.m_pInt != nullptr)
					*R.m_pInt = Value;
				else
					R.m_Set(Value);
				if(R.m_Fn)
					R.m_Fn();
			}
			break;
		}
		case SRow::EType::SLIDER:
		{
			static std::map<std::string, int> s_Pending;
			int Value = R.m_pInt != nullptr ? *R.m_pInt : R.m_Get();
			auto Pending = s_Pending.find(pKey);
			if(R.m_Deferred && Pending != s_Pending.end())
				Value = Pending->second;
			int Before = Value;
			SliderControl(Ui, pKey, &Value, R.m_Min, R.m_Max, R.m_pUnit, R.m_pZero);
			if(R.m_Deferred)
			{
				if(Value != Before)
					s_Pending[pKey] = Value;
				Pending = s_Pending.find(pKey);
				if(Pending == s_Pending.end() || Ui.MouseDown())
					break;
				Value = Pending->second;
				s_Pending.erase(Pending);
				Before = R.m_pInt != nullptr ? *R.m_pInt : R.m_Get();
			}
			if(Value != Before)
			{
				if(R.m_pInt != nullptr)
					*R.m_pInt = Value;
				else
					R.m_Set(Value);
				if(R.m_Fn)
					R.m_Fn();
			}
			break;
		}
		case SRow::EType::SEGMENT:
		{
			std::vector<const char *> vpLabels;
			for(const std::string &Option : R.m_vOptions)
				vpLabels.push_back(Localize(Option.c_str()));
			int Value = (R.m_pInt != nullptr ? *R.m_pInt : R.m_Get()) - R.m_OptionBase;
			if(Ui.Segmented(pKey, vpLabels.data(), (int)vpLabels.size(), &Value).m_Changed)
			{
				if(R.m_pInt != nullptr)
					*R.m_pInt = Value + R.m_OptionBase;
				else
					R.m_Set(Value + R.m_OptionBase);
				if(R.m_Fn)
					R.m_Fn();
			}
			break;
		}
		case SRow::EType::DROPDOWN:
		{
			std::vector<std::string> vOptions = R.m_Options ? R.m_Options() : R.m_vOptions;
			if(!R.m_Options)
				for(std::string &Option : vOptions)
					Option = Localize(Option.c_str());
			int Value = (R.m_pInt != nullptr ? *R.m_pInt : R.m_Get()) - R.m_OptionBase;
			const int Before = Value;
			Dropdown(Ui, pKey, vOptions, &Value, 200.0f, R.m_pIcon);
			if(Value != Before)
			{
				if(R.m_pInt != nullptr)
					*R.m_pInt = Value + R.m_OptionBase;
				else
					R.m_Set(Value + R.m_OptionBase);
				if(R.m_Fn)
					R.m_Fn();
			}
			break;
		}
		case SRow::EType::TEXT:
			if(Ui.Field(pKey, R.m_pText, R.m_TextSize, {.m_W = Px(200.0f), .m_Right = true}).m_Changed && R.m_Fn)
				R.m_Fn();
			break;
		case SRow::EType::COLOR:
			if(Ui.ColorPickerButton(pKey, R.m_pColor).m_Changed && R.m_Fn)
				R.m_Fn();
			break;
		case SRow::EType::KEY:
		{
			const int Old = FindBoundKey(R.m_Command.c_str());
			int Key = Old;
			if(Ui.Kbd(pKey, &Key).m_Changed)
			{
				if(Old != 0)
					Game()->m_Binds.Bind(Old, "");
				if(Key != KEY_UNKNOWN)
					Game()->m_Binds.Bind(Key, R.m_Command.c_str());
			}
			break;
		}
		case SRow::EType::ACTION:
			if(Ui.Button(Localize(R.m_pButton), {.m_Variant = EVariant::ACCENT, .m_Height = 34.0f, .m_PadX = 16.0f, .m_pKey = pKey}).m_Clicked && R.m_Fn)
				R.m_Fn();
			break;
		case SRow::EType::CUSTOM:
			if(R.m_Fn)
				R.m_Fn();
			break;
		default:
			break;
		}
	}

	static void RenderRow(CXui &Ui, SRow &R, const char *pKey, const char *pDescOverride)
	{
		const char *pDesc = pDescOverride != nullptr ? pDescOverride : (R.m_pDesc[0] != '\0' ? Localize(R.m_pDesc) : "");
		Ui.SettingsRow(Localize(R.m_pLabel), pDesc, [&]() {
			SBox B;
			B.m_Gap = 12.0f;
			B.m_Cross = EAlign::CENTER;
			Ui.Row(B, [&]() {
				RenderControl(Ui, R, pKey);
				SBox Gap;
				Gap.m_W = Px(18.0f);
				Gap.m_Main = EAlign::CENTER;
				Gap.m_Cross = EAlign::CENTER;
				Ui.Row(Gap, [&]() {
					if(IsModified(R))
						ResetDot(Ui, pKey, R);
				});
			});
		});
	}

	static bool Visible(const SRow &R)
	{
		return !R.m_Show || R.m_Show();
	}

	static const char *GroupTitle(const SSection &S)
	{
		if(S.m_pGroup == nullptr)
			return S.m_pTitle;
		if(str_comp(S.m_pGroup, "look") == 0)
			return "Appearance";
		if(str_comp(S.m_pGroup, "controls") == 0)
			return "Controls";
		return S.m_pTitle;
	}

	static const char *GroupIcon(const SSection &S)
	{
		if(S.m_pGroup != nullptr && str_comp(S.m_pGroup, "look") == 0)
			return Ph::PAINT_BRUSH_BROAD;
		return S.m_pIcon;
	}

	static std::vector<SSection> &Sections()
	{
		static std::vector<SSection> s_vSections;
		if(!s_vSections.empty())
			return s_vSections;
		struct SDef
		{
			const char *m_pId;
			const char *m_pGroup;
			const char *m_pTitle;
			const char *m_pDesc;
			const char *m_pIcon;
			void (*m_pBuild)(SSection &);
			unsigned m_Color;
		};
		static const SDef s_aDefs[] = {
			{"general", nullptr, "General", "Camera, demos and client behaviour", Ph::GEAR_SIX, BuildGeneral, 0x8E8E93},
			{"player", nullptr, "Player", "How others see you", Ph::USER_CIRCLE, BuildPlayer, 0x0A84FF},
			{"tee", nullptr, "Tee", "Skin and colours", Ph::PALETTE, BuildTee, 0xFF9F0A},
			{"chat", nullptr, "Chat", "Messages, filters and colours", Ph::CHAT_CIRCLE, BuildChat, 0x34C759},
			{"nameplates", nullptr, "Name plates", "Names, hook lines and lasers", Ph::TAG, BuildNamePlates, 0xAC8E68},
			{"controls", "controls", "Controls", "Keys and mouse", Ph::MOUSE_SIMPLE, BuildControls, 0x30D158},
			{"graphics", nullptr, "Graphics", "Display and renderer", Ph::MONITOR, BuildGraphics, 0x64D2FF},
			{"sound", nullptr, "Sound", "Volume and sources", Ph::SPEAKER_HIGH, BuildSound, 0xFF375F},
			{"ddnet", "ddnet", "DDNet", "Race, ghost and replays", Ph::FLAG, BuildDdnet, 0xFF453A},
			{"race", "ddnet", "Race", "Timer, records and ghost", Ph::TIMER, BuildRace, 0xFF453A},
			{"assets", nullptr, "Assets", "Game textures", Ph::PACKAGE, BuildAssets, 0xBF5AF2},
			{"social", nullptr, "Friends", "Friends list", Ph::USERS, BuildSocial, 0xFF2D55},
			{"appearance", "look", "Interface", "", Ph::PAINT_BRUSH_BROAD, BuildAppearance, 0x5E5CE6},
			{"hud", "look", "HUD", "What is shown in game", Ph::EYE, BuildHud, 0x5E5CE6},
			{"island", "look", "Island", "Notifications, votes and now playing", Ph::BELL, BuildIsland, 0x5E5CE6},
			{"visual", nullptr, "World", "World and tees", Ph::STACK, BuildVisual, 0x32ADE6},
			{"gameplay", nullptr, "Gameplay", "Game feel, camera and prediction", Ph::LIGHTNING, BuildGameplay, 0xFF9500},
			{"about", nullptr, "About xxx", "Authors, technology and licenses", Ph::INFO, BuildAbout, 0x8E8E93},
		};
		for(const SDef &Def : s_aDefs)
		{
			s_vSections.emplace_back();
			SSection &S = s_vSections.back();
			S.m_pId = Def.m_pId;
			S.m_pGroup = Def.m_pGroup;
			S.m_Ours = str_comp(Def.m_pId, "appearance") == 0 || str_comp(Def.m_pId, "hud") == 0 || str_comp(Def.m_pId, "island") == 0 || str_comp(Def.m_pId, "visual") == 0 || str_comp(Def.m_pId, "gameplay") == 0;
			S.m_pTitle = Def.m_pTitle;
			S.m_pDesc = Def.m_pDesc;
			S.m_pIcon = Def.m_pIcon;
			S.m_Color = Def.m_Color != 0 ? XuiTheme::Rgb(Def.m_Color) : ColorRGBA(0.0f, 0.0f, 0.0f, -1.0f);
			Def.m_pBuild(S);
		}
		return s_vSections;
	}

	static size_t RenderExpand(CXui &Ui, SSection &S, size_t Index, int SectionIndex)
	{
		SRow &E = S.m_vRows[Index];
		std::vector<size_t> vChildren;
		for(size_t j = Index + 1; j < S.m_vRows.size() && S.m_vRows[j].m_ExpandParent == (int)Index; j++)
			vChildren.push_back(j);
		const size_t Next = vChildren.empty() ? Index + 1 : vChildren.back() + 1;
		int Total = 0, On = 0;
		for(size_t j : vChildren)
		{
			const SRow &C = S.m_vRows[j];
			if(C.m_Type == SRow::EType::TOGGLE && C.m_pInt != nullptr)
			{
				Total++;
				On += *C.m_pInt != 0;
			}
		}
		char aKey[96];
		str_format(aKey, sizeof(aKey), "s%d.x%d", SectionIndex, (int)Index);
		static std::map<std::string, bool> s_Open;
		bool &Open = s_Open[aKey];
		const uint32_t Id = Ui.MakeId(aKey);
		const SResult R = Ui.Interact(Id);
		if(R.m_Clicked)
			Open = !Open;
		const float Hover = Ui.State(Id).m_Hover.Update(R.m_Hovered ? 1.0f : 0.0f, Ui.Dt(), XuiTheme::Motion::HOVER);
		SBox Row;
		Row.m_W = Grow();
		Row.m_H = Px(E.m_pDesc[0] != '\0' ? 62.0f : 52.0f);
		Row.m_Gap = 10.0f;
		Row.m_Cross = EAlign::CENTER;
		Row.m_Padding = SEdges(0.0f, 18.0f);
		Row.m_Hit = EHit::INTERACTIVE;
		const int Node = Ui.BoxId(Id, EDir::ROW, Row, [&]() {
			SBox Names;
			Names.m_W = Grow();
			Names.m_Shrink = true;
			Names.m_Gap = 2.0f;
			Ui.Column(Names, [&]() {
				SBox Line;
				Line.m_Gap = 8.0f;
				Line.m_Cross = EAlign::CENTER;
				Ui.Row(Line, [&]() {
					STextStyle L;
					L.m_Size = 15.0f;
					L.m_Font = EFont::MEDIUM;
					Ui.Text(Localize(E.m_pLabel), L);
					if(Total > 0)
					{
						char aCount[16];
						str_format(aCount, sizeof(aCount), "%d/%d", On, Total);
						STextStyle C;
						C.m_Size = 13.0f;
						C.m_Font = EFont::SEMIBOLD;
						C.m_Color = XuiTheme::Color::TEXT3;
						C.m_Tabular = true;
						Ui.Text(aCount, C);
					}
					Ui.Icon(Open ? Ph::CARET_UP : Ph::CARET_DOWN, 13.0f, XuiTheme::Color::TEXT3);
				});
				if(E.m_pDesc[0] != '\0')
				{
					STextStyle D;
					D.m_Size = 12.0f;
					D.m_Color = XuiTheme::Color::TEXT3;
					D.m_Wrap = true;
					D.m_MaxLines = 1;
					SBox DB;
					DB.m_W = Grow();
					Ui.Text(Localize(E.m_pDesc), D, DB);
				}
			});
			if(Total > 0)
			{
				int Master = On > 0;
				char aSwitch[112];
				str_format(aSwitch, sizeof(aSwitch), "%s.all", aKey);
				Ui.PushId(Ui.MakeId(aSwitch));
				if(Ui.Switch("", &Master).m_Changed)
					for(size_t j : vChildren)
						if(S.m_vRows[j].m_Type == SRow::EType::TOGGLE && S.m_vRows[j].m_pInt != nullptr)
							*S.m_vRows[j].m_pInt = Master;
				Ui.PopId();
			}
		});
		Ui.Paint(Node, [Hover](CXui &U, const SRect &Rc) {
			if(Hover > 0.01f)
				U.DrawRect(Rc, CXui::SolidRect(ColorRGBA(1, 1, 1, 0.035f * Hover), 0.0f));
		});
		if(!Open)
			return Next;
		SBox Children;
		Children.m_W = Grow();
		Children.m_Padding = SEdges(0.0f, 0.0f, 6.0f, 22.0f);
		Children.m_Cross = EAlign::STRETCH;
		Ui.Column(Children, [&]() {
			for(size_t j : vChildren)
			{
				SRow &C = S.m_vRows[j];
				if(!Visible(C))
					continue;
				char aChild[96];
				str_format(aChild, sizeof(aChild), "s%d.r%d", SectionIndex, (int)j);
				RenderRow(Ui, C, aChild, nullptr);
			}
		});
		return Next;
	}

	static void RenderRows(CXui &Ui, SSection &S, int SectionIndex)
	{
		size_t i = 0;
		size_t N = S.m_vRows.size();
		std::vector<size_t> vHeaders;
		for(size_t r = 0; r < N; r++)
			if(S.m_vRows[r].m_Type == SRow::EType::HEADER && Visible(S.m_vRows[r]))
				vHeaders.push_back(r);
		const bool Tabs = vHeaders.size() >= 3 && str_comp(S.m_pId, "about") != 0;
		if(Tabs)
		{
			static std::map<int, int> s_Tabs;
			int &Tab = s_Tabs[SectionIndex];
			Tab = std::clamp(Tab, 0, (int)vHeaders.size() - 1);
			std::vector<const char *> vLabels;
			for(size_t h : vHeaders)
				vLabels.push_back(Localize(S.m_vRows[h].m_pLabel));
			char aKey[32];
			str_format(aKey, sizeof(aKey), "settings.sub%d", SectionIndex);
			Ui.Tabs(aKey, vLabels.data(), (int)vLabels.size(), &Tab);
			i = Tab == 0 ? 0 : vHeaders[Tab] + 1;
			if(Tab + 1 < (int)vHeaders.size())
				N = vHeaders[Tab + 1];
		}
		while(i < N)
		{
			SRow &R = S.m_vRows[i];
			if(!Visible(R))
			{
				i++;
				continue;
			}
			if(R.m_Type == SRow::EType::HEADER)
			{
				if(!Tabs)
					Ui.SectionHeader(Localize(R.m_pLabel));
				i++;
				continue;
			}
			if(R.m_Type == SRow::EType::BLOCK)
			{
				if(R.m_Fn)
					R.m_Fn();
				i++;
				continue;
			}
			Ui.Group([&]() {
				while(i < N && S.m_vRows[i].m_Type != SRow::EType::HEADER && S.m_vRows[i].m_Type != SRow::EType::BLOCK)
				{
					SRow &Row = S.m_vRows[i];
					if(Row.m_Type == SRow::EType::EXPAND)
					{
						if(Visible(Row))
							i = RenderExpand(Ui, S, i, SectionIndex);
						else
						{
							const int Parent = (int)i;
							i++;
							while(i < N && S.m_vRows[i].m_ExpandParent == Parent)
								i++;
						}
						continue;
					}
					if(Row.m_ExpandParent >= 0)
					{
						i++;
						continue;
					}
					if(Visible(Row))
					{
						char aKey[64];
						str_format(aKey, sizeof(aKey), "s%d.r%d", SectionIndex, (int)i);
						RenderRow(Ui, Row, aKey, nullptr);
						if(Row.m_Type == SRow::EType::DROPDOWN)
						{
							std::vector<std::string> vOptions = Row.m_Options ? Row.m_Options() : Row.m_vOptions;
							if(!Row.m_Options)
								for(std::string &Option : vOptions)
									Option = Localize(Option.c_str());
							int Value = (Row.m_pInt != nullptr ? *Row.m_pInt : Row.m_Get()) - Row.m_OptionBase;
							if(DropdownOptions(Ui, aKey, vOptions, &Value))
							{
								if(Row.m_pInt != nullptr)
									*Row.m_pInt = Value + Row.m_OptionBase;
								else
									Row.m_Set(Value + Row.m_OptionBase);
								if(Row.m_Fn)
									Row.m_Fn();
							}
						}
					}
					i++;
				}
			});
		}
	}

}

using namespace SettingsUi;

static int s_Selected = 1;

void SettingsSelectNav(int Index)
{
	s_Selected = Index;
}

void SettingsSelectSection(const char *pId)
{
	const auto &vSections = SettingsUi::Sections();
	int Nav = -1;
	for(size_t i = 0; i < vSections.size(); i++)
	{
		if(i == 0 || vSections[i].m_Ours != vSections[i - 1].m_Ours)
			Nav++;
		const char *pGroup = vSections[i].m_pGroup;
		if(pGroup == nullptr || i == 0 || vSections[i - 1].m_pGroup == nullptr || str_comp(vSections[i - 1].m_pGroup, pGroup) != 0)
			Nav++;
		if(str_comp(vSections[i].m_pId, pId) == 0)
		{
			s_Selected = Nav;
			return;
		}
	}
}

void RenderSettings(CXui &Ui)
{
	SetContext(Ui.Game(), &Ui);
	std::vector<SSection> &vSections = Sections();

	static char s_aSearch[64] = "";

	static std::vector<SNavItem> s_vNav;
	static std::vector<int> s_vNavToSection;
	if(s_vNav.empty())
	{
		for(int i = 0; i < (int)vSections.size(); i++)
		{
			const char *pGroup = vSections[i].m_pGroup;
			if(pGroup != nullptr && i > 0 && vSections[i - 1].m_pGroup != nullptr && str_comp(vSections[i - 1].m_pGroup, pGroup) == 0)
				continue;
			if(i == 0 || vSections[i].m_Ours != vSections[i - 1].m_Ours)
			{
				s_vNav.push_back({vSections[i].m_Ours ? "xxx" : "DDNet", nullptr, nullptr, true});
				s_vNavToSection.push_back(-1);
			}
			s_vNav.push_back({vSections[i].m_pTitle, vSections[i].m_pIcon, nullptr, false});
			s_vNavToSection.push_back(i);
		}
	}

	for(size_t i = 0; i < s_vNav.size(); i++)
	{
		if(s_vNavToSection[i] < 0)
			continue;
		const SSection &S = vSections[s_vNavToSection[i]];
		s_vNav[i].m_Tile = S.m_Color.a < 0.0f ? Ui.Accent() : S.m_Color;
		s_vNav[i].m_pLabel = Localize(GroupTitle(S));
		s_vNav[i].m_pIcon = GroupIcon(S);
	}

	const bool Searching = s_aSearch[0] != '\0';
	int Selected = Searching ? -1 : s_Selected;

	SBox Wrap;
	Wrap.m_Gap = 16.0f;
	Wrap.m_W = Grow();
	Wrap.m_H = Grow();
	Ui.Row(Wrap, [&]() {
		SBox SideBox;
		SideBox.m_W = Px(250.0f);
		SideBox.m_H = Grow();
		SideBox.m_Padding = SEdges(12.0f, 10.0f);
		SideBox.m_Gap = 10.0f;
		SideBox.m_Clip = true;
		Ui.GlassPanel(SideBox, [&]() {
			Ui.SearchField("settings.search", s_aSearch, sizeof(s_aSearch), Localize("Search"), Grow());
			SBox Scroll;
			Scroll.m_W = Grow();
			Scroll.m_H = Grow();
			Scroll.m_Padding.m_R = XuiTheme::Size::SCROLLBAR_W + 10.0f;
			Ui.Scroll(Scroll, [&]() {
				Ui.Sidebar("settings.nav", s_vNav.data(), (int)s_vNav.size(), &Selected, nullptr);
			}); }, XuiTheme::Radius::LG);
		if(Selected >= 0 && (Searching || Selected != s_Selected))
		{
			s_Selected = Selected;
			s_aSearch[0] = '\0';
		}

		SBox ContentBox;
		ContentBox.m_W = Grow();
		ContentBox.m_H = Grow();
		ContentBox.m_Padding = SEdges(30.0f, 34.0f);
		ContentBox.m_Clip = true;
		Ui.GlassPanel(ContentBox, [&]() {
			SBox Scroll;
			Scroll.m_W = Grow();
			Scroll.m_H = Grow();
			Scroll.m_Gap = 16.0f;
			Ui.Scroll(Scroll, [&]() {
				int NavIndex = std::clamp(s_Selected, 0, (int)s_vNavToSection.size() - 1);
				while(NavIndex < (int)s_vNavToSection.size() - 1 && s_vNavToSection[NavIndex] < 0)
					NavIndex++;
				const int SectionIndex = s_vNavToSection[NavIndex];
				if(Searching)
				{
					struct SMatch
					{
						int m_Section;
						int m_Row;
					};
					std::vector<SMatch> vMatches;
					for(int s = 0; s < (int)vSections.size(); s++)
					{
						for(int r = 0; r < (int)vSections[s].m_vRows.size(); r++)
						{
							const SRow &Row = vSections[s].m_vRows[r];
							if(Row.m_Type == SRow::EType::HEADER || Row.m_Type == SRow::EType::BLOCK || !Visible(Row))
								continue;
							if(str_utf8_find_nocase(Localize(Row.m_pLabel), s_aSearch) != nullptr || (Row.m_pDesc[0] != '\0' && str_utf8_find_nocase(Localize(Row.m_pDesc), s_aSearch) != nullptr))
								vMatches.push_back({s, r});
						}
					}
					char aCount[48];
					str_format(aCount, sizeof(aCount), Localize("%d results"), (int)vMatches.size());
					Ui.PageHeader(Localize("Search"), aCount);
					if(vMatches.empty())
					{
						Ui.Group([&]() { Ui.SettingsRow(Localize("Nothing found"), "", []() {}); });
					}
					else
					{
						Ui.Group([&]() {
							for(const SMatch &M : vMatches)
							{
								SSection &S = vSections[M.m_Section];
								SRow &Row = S.m_vRows[M.m_Row];
								char aKey[64];
								str_format(aKey, sizeof(aKey), "q%d.r%d", M.m_Section, M.m_Row);
								char aDesc[256];
								str_format(aDesc, sizeof(aDesc), "%s%s%s", Localize(S.m_pTitle), Row.m_pDesc[0] != '\0' ? " · " : "", Row.m_pDesc[0] != '\0' ? Localize(Row.m_pDesc) : "");
								RenderRow(Ui, Row, aKey, aDesc);
							}
						});
					}
					return;
				}
				int PageIndex = SectionIndex;
				std::vector<int> vGroup;
				static int *s_pGroupTab = nullptr;
				static std::vector<const char *> s_vGroupLabels;
				s_pGroupTab = nullptr;
				if(vSections[SectionIndex].m_pGroup != nullptr)
				{
					for(int i = 0; i < (int)vSections.size(); i++)
						if(vSections[i].m_pGroup != nullptr && str_comp(vSections[i].m_pGroup, vSections[SectionIndex].m_pGroup) == 0)
							vGroup.push_back(i);
					static std::map<std::string, int> s_GroupTab;
					int &Tab = s_GroupTab[vSections[SectionIndex].m_pGroup];
					Tab = std::clamp(Tab, 0, (int)vGroup.size() - 1);
					std::vector<const char *> vLabels;
					for(int i : vGroup)
						vLabels.push_back(Localize(vSections[i].m_pTitle));
					PageIndex = vGroup[Tab];
					s_pGroupTab = &Tab;
					s_vGroupLabels = vLabels;
				}
				SSection &S = vSections[PageIndex];
				SBox Transition;
				Transition.m_W = Grow();
				Ui.PageTransition(S.m_pGroup != nullptr ? S.m_pGroup : S.m_pId, Transition, [&]() {
					SBox Page;
					Page.m_W = Grow(1.0f, 0.0f, S.m_Preview ? 1400.0f : 1080.0f);
					Page.m_Gap = 16.0f;
					Ui.Column(Page, [&]() {
						if(str_comp(S.m_pId, "about") != 0)
							Ui.PageHeader(Localize(GroupTitle(S)), Localize(S.m_pDesc));
						if(s_pGroupTab != nullptr)
						{
							char aKey[64];
							str_format(aKey, sizeof(aKey), "settings.tabs.%s", S.m_pGroup);
							Ui.Segmented(aKey, s_vGroupLabels.data(), (int)s_vGroupLabels.size(), s_pGroupTab);
						}
						if(!S.m_Preview)
						{
							RenderRows(Ui, S, PageIndex);
							return;
						}
						SBox Split;
						Split.m_Gap = 20.0f;
						Split.m_W = Grow();
						Split.m_Cross = EAlign::START;
						Ui.Row(Split, [&]() {
							SBox Left;
							Left.m_W = Grow();
							Left.m_Gap = 16.0f;
							Left.m_Shrink = true;
							Ui.Column(Left, [&]() { RenderRows(Ui, S, PageIndex); });
							SBox Right;
							Right.m_W = Px(330.0f);
							Right.m_Gap = 12.0f;
							Ui.Column(Right, [&]() { S.m_Preview(); });
						});
					});
				});
			}); }, XuiTheme::Radius::XL);
	});
}
