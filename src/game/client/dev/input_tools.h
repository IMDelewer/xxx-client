#ifndef GAME_CLIENT_COMPONENTS_XXX_DEV_INPUT_H
#define GAME_CLIENT_COMPONENTS_XXX_DEV_INPUT_H

#include <engine/console.h>

#include <game/client/component.h>

#include <string>
#include <vector>

class CDevInput : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnUpdate() override;
	void OnRender() override;

private:
	struct SDelayed
	{
		float m_Time;
		std::string m_Command;
	};
	std::vector<SDelayed> m_vDelayed;
	std::vector<double> m_vFrameTimes;
	std::string m_PerfLabel;
	int64_t m_PerfUntil = 0;
	int64_t m_PerfPrevious = 0;
	static void ConPerf(IConsole::IResult *pResult, void *pUserData);

	static void ConFriends(IConsole::IResult *pResult, void *pUserData);
	static void ConAfter(IConsole::IResult *pResult, void *pUserData);
	static void ConMouse(IConsole::IResult *pResult, void *pUserData);
	static void ConClick(IConsole::IResult *pResult, void *pUserData);
	static void ConPress(IConsole::IResult *pResult, void *pUserData);
	static void ConKey(IConsole::IResult *pResult, void *pUserData);
	static void ConText(IConsole::IResult *pResult, void *pUserData);
	static void ConStroke(IConsole::IResult *pResult, void *pUserData);
};

#endif
