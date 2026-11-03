#ifndef GAME_CLIENT_COMPONENTS_SCOREBOARD_H
#define GAME_CLIENT_COMPONENTS_SCOREBOARD_H

#include <engine/console.h>

#include <game/client/component.h>

#include <optional>

class CScoreboard : public CComponent
{
	static void ConKeyScoreboard(IConsole::IResult *pResult, void *pUserData);
	static void ConToggleScoreboardCursor(IConsole::IResult *pResult, void *pUserData);
	bool m_Active = false;
	std::optional<vec2> m_LastMousePos;
	bool m_MouseUnlocked = false;
	void SetUiMousePos(vec2 Pos);
	void LockMouse();

public:
	CScoreboard();
	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnReset() override;
	void OnRender() override;
	void OnRelease() override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	bool OnInput(const IInput::CEvent &Event) override;
	bool IsActive() const;
	bool CursorActive() const { return IsActive() && m_MouseUnlocked; }
};

#endif
