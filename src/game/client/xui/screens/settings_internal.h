#ifndef GAME_CLIENT_XUI_SCREENS_SETTINGS_INTERNAL_H
#define GAME_CLIENT_XUI_SCREENS_SETTINGS_INTERNAL_H

#include <game/client/xui/xui.h>

#include <functional>
#include <string>
#include <vector>

class CGameClient;

namespace SettingsUi
{
	bool ConfigInt(const void *pVar, long long &Default, long long &Min, long long &Max);
	inline void ConfigRange(const void *pVar, int &Min, int &Max)
	{
		long long Def, Lo, Hi;
		if(ConfigInt(pVar, Def, Lo, Hi))
		{
			Min = (int)Lo;
			Max = (int)Hi;
		}
	}

	struct SRow
	{
		enum class EType
		{
			HEADER,
			TOGGLE,
			SLIDER,
			SEGMENT,
			DROPDOWN,
			TEXT,
			COLOR,
			KEY,
			ACTION,
			CUSTOM,
			BLOCK,
			EXPAND,
		};

		EType m_Type = EType::TOGGLE;
		const char *m_pLabel = "";
		const char *m_pDesc = "";
		int *m_pInt = nullptr;
		unsigned *m_pColor = nullptr;
		char *m_pText = nullptr;
		int m_TextSize = 0;
		int m_Min = 0;
		int m_Max = 1;
		const char *m_pUnit = "";
		int m_ExpandParent = -1;
		bool m_Deferred = false;
		const char *m_pZero = nullptr;
		std::vector<std::string> m_vOptions;
		int m_OptionBase = 0;
		std::string m_Command;
		const char *m_pButton = "";
		const char *m_pIcon = nullptr;
		std::function<void()> m_Fn;
		std::function<bool()> m_Show;
		std::function<int()> m_Get;
		std::function<void(int)> m_Set;
		std::function<std::vector<std::string>()> m_Options;

		SRow &Deferred()
		{
			m_Deferred = true;
			return *this;
		}
		SRow &Desc(const char *pDesc)
		{
			m_pDesc = pDesc;
			return *this;
		}
		SRow &Show(std::function<bool()> Show)
		{
			m_Show = std::move(Show);
			return *this;
		}
		SRow &Zero(const char *pZero)
		{
			m_pZero = pZero;
			return *this;
		}
		SRow &Icon(const char *pIcon)
		{
			m_pIcon = pIcon;
			return *this;
		}
		SRow &Unit(const char *pUnit)
		{
			m_pUnit = pUnit;
			return *this;
		}
		SRow &OnChange(std::function<void()> Fn)
		{
			m_Fn = std::move(Fn);
			return *this;
		}
	};

	struct SSection
	{
		const char *m_pId = "";
		const char *m_pGroup = nullptr;
		bool m_Ours = false;
		const char *m_pTitle = "";
		const char *m_pDesc = "";
		const char *m_pIcon = "";
		ColorRGBA m_Color = ColorRGBA(0.0f, 0.0f, 0.0f, -1.0f);
		std::vector<SRow> m_vRows;
		std::function<void()> m_Preview;

		int m_CurrentExpand = -1;
		SRow &Add(SRow::EType Type, const char *pLabel)
		{
			m_vRows.emplace_back();
			m_vRows.back().m_Type = Type;
			m_vRows.back().m_pLabel = pLabel;
			if(Type != SRow::EType::HEADER && Type != SRow::EType::EXPAND && Type != SRow::EType::BLOCK)
				m_vRows.back().m_ExpandParent = m_CurrentExpand;
			return m_vRows.back();
		}
		void Header(const char *pLabel)
		{
			m_CurrentExpand = -1;
			Add(SRow::EType::HEADER, pLabel);
		}
		SRow &Expand(const char *pLabel)
		{
			m_CurrentExpand = -1;
			Add(SRow::EType::EXPAND, pLabel);
			m_CurrentExpand = (int)m_vRows.size() - 1;
			return m_vRows.back();
		}
		void EndExpand() { m_CurrentExpand = -1; }
		SRow &Toggle(const char *pLabel, int *pValue)
		{
			SRow &R = Add(SRow::EType::TOGGLE, pLabel);
			R.m_pInt = pValue;
			return R;
		}
		SRow &Slider(const char *pLabel, int *pValue, int Min, int Max, const char *pUnit = "")
		{
			if(Min == Max)
				ConfigRange(pValue, Min, Max);
			SRow &R = Add(SRow::EType::SLIDER, pLabel);
			R.m_pInt = pValue;
			R.m_Min = Min;
			R.m_Max = Max;
			R.m_pUnit = pUnit;
			return R;
		}
		SRow &Segment(const char *pLabel, int *pValue, std::vector<std::string> vOptions, int Base = 0)
		{
			SRow &R = Add(SRow::EType::SEGMENT, pLabel);
			R.m_pInt = pValue;
			R.m_vOptions = std::move(vOptions);
			R.m_OptionBase = Base;
			return R;
		}
		SRow &Dropdown(const char *pLabel, int *pValue, std::vector<std::string> vOptions, int Base = 0)
		{
			SRow &R = Segment(pLabel, pValue, std::move(vOptions), Base);
			R.m_Type = SRow::EType::DROPDOWN;
			return R;
		}
		SRow &Text(const char *pLabel, char *pBuffer, int Size)
		{
			SRow &R = Add(SRow::EType::TEXT, pLabel);
			R.m_pText = pBuffer;
			R.m_TextSize = Size;
			return R;
		}
		SRow &Color(const char *pLabel, unsigned *pValue)
		{
			SRow &R = Add(SRow::EType::COLOR, pLabel);
			R.m_pColor = pValue;
			return R;
		}
		SRow &Key(const char *pLabel, const char *pCommand)
		{
			SRow &R = Add(SRow::EType::KEY, pLabel);
			R.m_Command = pCommand;
			return R;
		}
		SRow &Action(const char *pLabel, const char *pButton, std::function<void()> Fn)
		{
			SRow &R = Add(SRow::EType::ACTION, pLabel);
			R.m_pButton = pButton;
			R.m_Fn = std::move(Fn);
			return R;
		}
		SRow &Custom(const char *pLabel, std::function<void()> Control)
		{
			SRow &R = Add(SRow::EType::CUSTOM, pLabel);
			R.m_Fn = std::move(Control);
			return R;
		}
		SRow &Block(const char *pLabel, std::function<void()> Content)
		{
			SRow &R = Add(SRow::EType::BLOCK, pLabel);
			R.m_Fn = std::move(Content);
			return R;
		}
		SRow &Virtual(const char *pLabel, std::function<int()> Get, std::function<void(int)> Set, int Min, int Max)
		{
			SRow &R = Add(SRow::EType::SLIDER, pLabel);
			R.m_Get = std::move(Get);
			R.m_Set = std::move(Set);
			R.m_Min = Min;
			R.m_Max = Max;
			return R;
		}
		SRow &VirtualToggle(const char *pLabel, std::function<int()> Get, std::function<void(int)> Set)
		{
			SRow &R = Add(SRow::EType::TOGGLE, pLabel);
			R.m_Get = std::move(Get);
			R.m_Set = std::move(Set);
			return R;
		}
		SRow &VirtualSegment(const char *pLabel, std::function<int()> Get, std::function<void(int)> Set, std::vector<std::string> vOptions)
		{
			SRow &R = Add(SRow::EType::SEGMENT, pLabel);
			R.m_Get = std::move(Get);
			R.m_Set = std::move(Set);
			R.m_vOptions = std::move(vOptions);
			return R;
		}
		SRow &VirtualDropdown(const char *pLabel, std::function<int()> Get, std::function<void(int)> Set, std::function<std::vector<std::string>()> Options)
		{
			SRow &R = Add(SRow::EType::DROPDOWN, pLabel);
			R.m_Get = std::move(Get);
			R.m_Set = std::move(Set);
			R.m_Options = std::move(Options);
			return R;
		}
	};

	CGameClient *Game();
	CXui &Ui();
	void SetContext(CGameClient *pGame, CXui *pUi);
	int &DummyIndex();

	inline bool DefaultInt(const void *pVar, long long &Default)
	{
		long long Min, Max;
		return ConfigInt(pVar, Default, Min, Max);
	}
	bool DefaultString(const char *pVar, std::string &Default);

	void Dropdown(CXui &Ui, const char *pKey, const std::vector<std::string> &vOptions, int *pValue, float Width = 200.0f, const char *pIcon = nullptr);
	bool DropdownOptions(CXui &Ui, const char *pKey, const std::vector<std::string> &vOptions, int *pValue);
	void SliderControl(CXui &Ui, const char *pKey, int *pValue, int Min, int Max, const char *pUnit, const char *pZero);

	void BuildGeneral(SSection &S);
	void BuildAbout(SSection &S);
	void BuildPlayer(SSection &S);
	void BuildTee(SSection &S);
	void BuildHud(SSection &S);
	void BuildChat(SSection &S);
	void BuildNamePlates(SSection &S);
	void BuildControls(SSection &S);
	void BuildGraphics(SSection &S);
	void BuildSound(SSection &S);
	void BuildDdnet(SSection &S);
	void BuildAssets(SSection &S);
	void BuildAppearance(SSection &S);
	void BuildVisual(SSection &S);
	void BuildGameplay(SSection &S);
	void BuildRace(SSection &S);
	void BuildSocial(SSection &S);
	void BuildIsland(SSection &S);
}

const char *SettingsCreditsText();

#endif
