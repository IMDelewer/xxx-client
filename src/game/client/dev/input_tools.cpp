#if defined(DDEV_XX)
#include "input_tools.h"

#include "frame_metrics.h"

#include <base/io.h>
#include <base/log.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/friends.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>

#include <algorithm>

void CDevInput::OnConsoleInit()
{
	Console()->Register("xx_dev_screen", "s[screen]", CFGFLAG_CLIENT, [](IConsole::IResult *pResult, void *pUserData) { static_cast<CDevInput *>(pUserData)->GameClient()->m_Xui.DebugScreen(pResult->GetString(0)); }, this, "Open an XUI screen for visual testing");
	Console()->Register("xx_dev_wheel", "i[steps]", CFGFLAG_CLIENT, [](IConsole::IResult *pResult, void *pUserData) {
		CDevInput *pSelf = static_cast<CDevInput *>(pUserData);
		const int Steps = std::clamp(pResult->GetInteger(0), -20, 20);
		const int Key = Steps > 0 ? KEY_MOUSE_WHEEL_DOWN : KEY_MOUSE_WHEEL_UP;
		for(int i = 0; i < std::abs(Steps); i++)
		{
			pSelf->Input()->DevPushKey(Key, true);
			pSelf->Input()->DevPushKey(Key, false);
		}
	}, this, "Scroll the XUI region under the cursor for testing");

	Console()->Register("xx_dev_perf", "s[label] f[seconds]", CFGFLAG_CLIENT, ConPerf, this, "Capture frame times to xxx_perf_LABEL.json and CSV");
	Console()->Register("xx_dev_friends", "", CFGFLAG_CLIENT, ConFriends, this, "Inspect friend presence in the full server list");
	Console()->Register("xx_after", "f[seconds] r[command]", CFGFLAG_CLIENT, ConAfter, this, "Run a console command after a delay");
	Console()->Register("xx_screenshot", "?s[name]", CFGFLAG_CLIENT, [](IConsole::IResult *pResult, void *pUserData) { static_cast<CDevInput *>(pUserData)->Graphics()->TakeScreenshot(pResult->NumArguments() ? pResult->GetString(0) : nullptr); }, this, "Save a screenshot of the rendered frame");
	Console()->Register("xx_dev_mouse", "f[x] f[y]", CFGFLAG_CLIENT, ConMouse, this, "Move the ui cursor to window pixel coordinates");
	Console()->Register("xx_dev_click", "?s[left|right]", CFGFLAG_CLIENT, ConClick, this, "Click with the ui cursor");
	Console()->Register("xx_dev_press", "i[down] ?s[left|right]", CFGFLAG_CLIENT, ConPress, this, "Press or release a mouse button");
	Console()->Register("xx_dev_key", "s[key]", CFGFLAG_CLIENT, ConKey, this, "Press and release a key by name");
	Console()->Register("xx_dev_text", "r[text]", CFGFLAG_CLIENT, ConText, this, "Type text");
	Console()->Register("xx_dev_stroke", "i[stroke] r[command]", CFGFLAG_CLIENT, ConStroke, this, "Run a +command with the given stroke (1 press, 0 release)");
}

void CDevInput::OnUpdate()
{
	const float Now = Client()->LocalTime();
	std::vector<SDelayed> vDue;
	m_vDelayed.erase(std::remove_if(m_vDelayed.begin(), m_vDelayed.end(), [&](const SDelayed &Delayed) {
		if(Delayed.m_Time > Now)
			return false;
		vDue.push_back(Delayed);
		return true;
	}),
		m_vDelayed.end());
	for(const SDelayed &Delayed : vDue)
		Console()->ExecuteLine(Delayed.m_Command.c_str(), IConsole::CLIENT_ID_UNSPECIFIED);
}

void CDevInput::ConAfter(IConsole::IResult *pResult, void *pUserData)
{
	CDevInput *pSelf = static_cast<CDevInput *>(pUserData);
	pSelf->m_vDelayed.push_back({pSelf->Client()->LocalTime() + pResult->GetFloat(0), pResult->GetString(1)});
}

void CDevInput::ConMouse(IConsole::IResult *pResult, void *pUserData)
{
	CDevInput *pSelf = static_cast<CDevInput *>(pUserData);
	const vec2 Target = vec2(pResult->GetFloat(0), pResult->GetFloat(1));
	const vec2 Current = pSelf->Ui()->UpdatedMousePos();
	pSelf->Ui()->OnCursorMove(Target.x - Current.x, Target.y - Current.y);
}

void CDevInput::ConClick(IConsole::IResult *pResult, void *pUserData)
{
	CDevInput *pSelf = static_cast<CDevInput *>(pUserData);
	const int Key = pResult->NumArguments() > 0 && str_comp_nocase(pResult->GetString(0), "right") == 0 ? KEY_MOUSE_2 : KEY_MOUSE_1;
	pSelf->Input()->DevPushKey(Key, true);
	pSelf->Input()->DevPushKey(KEY_UNKNOWN, false);
	pSelf->Input()->DevPushKey(KEY_UNKNOWN, false);
	pSelf->Input()->DevPushKey(Key, false);
}

void CDevInput::ConPress(IConsole::IResult *pResult, void *pUserData)
{
	CDevInput *pSelf = static_cast<CDevInput *>(pUserData);
	const int Key = pResult->NumArguments() > 1 && str_comp_nocase(pResult->GetString(1), "right") == 0 ? KEY_MOUSE_2 : KEY_MOUSE_1;
	pSelf->Input()->DevPushKey(Key, pResult->GetInteger(0) != 0);
}

void CDevInput::ConKey(IConsole::IResult *pResult, void *pUserData)
{
	CDevInput *pSelf = static_cast<CDevInput *>(pUserData);
	const int Key = pSelf->Input()->FindKeyByName(pResult->GetString(0));
	if(Key == KEY_UNKNOWN)
	{
		log_error("xxx", "Unknown key '%s'", pResult->GetString(0));
		return;
	}
	pSelf->Input()->DevPushKey(Key, true);
	pSelf->Input()->DevPushKey(Key, false);
}

void CDevInput::ConText(IConsole::IResult *pResult, void *pUserData)
{
	CDevInput *pSelf = static_cast<CDevInput *>(pUserData);
	pSelf->Input()->DevPushText(pResult->GetString(0));
}

void CDevInput::ConStroke(IConsole::IResult *pResult, void *pUserData)
{
	CDevInput *pSelf = static_cast<CDevInput *>(pUserData);
	pSelf->Console()->ExecuteLineStroked(pResult->GetInteger(0), pResult->GetString(1), IConsole::CLIENT_ID_UNSPECIFIED);
}

void CDevInput::ConFriends(IConsole::IResult *pResult, void *pUserData)
{
	CGameClient *pGame = static_cast<CDevInput *>(pUserData)->GameClient();
	IFriends *pFriends = pGame->Friends();
	IServerBrowser *pBrowser = pGame->ServerBrowser();
	log_info("xxx", "friends %d, internet servers %d", pFriends->NumFriends(), pBrowser->NumInternetServers());
	for(int i = 0; i < pBrowser->NumInternetServers(); ++i)
	{
		const CServerInfo *pServer = pBrowser->InternetGet(i);
		for(const auto &Client : pServer->m_vClients)
			for(int j = 0; j < pFriends->NumFriends(); ++j)
				if(str_comp(Client.m_aName, pFriends->GetFriend(j)->m_aName) == 0)
					log_info("xxx", "friend %s clan '%s', match %d, skin '%s', server '%s'", Client.m_aName, Client.m_aClan, pFriends->IsFriend(Client.m_aName, Client.m_aClan, true), Client.m_aSkin, pServer->m_aName);
	}
}

void CDevInput::ConPerf(IConsole::IResult *pResult, void *pUserData)
{
	CDevInput *pSelf = static_cast<CDevInput *>(pUserData);
	pSelf->m_PerfLabel.clear();
	for(const char *p = pResult->GetString(0); *p && pSelf->m_PerfLabel.size() < 48; ++p)
		pSelf->m_PerfLabel += (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' ? *p : '_';
	pSelf->m_vFrameTimes.clear();
	pSelf->m_vFrameTimes.reserve(200000);
	pSelf->m_PerfPrevious = 0;
	pSelf->m_PerfUntil = time_get() + (int64_t)(time_freq() * std::clamp(pResult->GetFloat(1), 0.5f, 120.0f));
}

void CDevInput::OnRender()
{
	if(!m_PerfUntil)
		return;
	const int64_t Now = time_get();
	if(m_PerfPrevious && m_vFrameTimes.size() < 1000000)
		m_vFrameTimes.push_back((Now - m_PerfPrevious) * 1000.0 / time_freq());
	m_PerfPrevious = Now;
	if(Now < m_PerfUntil)
		return;
	m_PerfUntil = 0;
	const SFrameMetrics Stats = SFrameMetrics::Compute(m_vFrameTimes);
	char aSummary[768];
	str_format(aSummary, sizeof(aSummary), "{\"label\":\"%s\",\"frames\":%d,\"fps\":%.2f,\"mean_ms\":%.4f,\"p50_ms\":%.4f,\"p95_ms\":%.4f,\"p99_ms\":%.4f,\"max_ms\":%.4f,\"over_8ms\":%d,\"over_16ms\":%d}", m_PerfLabel.c_str(), Stats.m_Frames, Stats.m_Fps, Stats.m_Mean, Stats.m_P50, Stats.m_P95, Stats.m_P99, Stats.m_Max, Stats.m_Over8, Stats.m_Over16);
	log_info("xxx-perf", "%s", aSummary);
	char aPath[128];
	str_format(aPath, sizeof(aPath), "xxx_perf_%s.json", m_PerfLabel.c_str());
	IOHANDLE Json = Storage()->OpenFile(aPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(Json)
	{
		io_write(Json, aSummary, str_length(aSummary));
		io_write_newline(Json);
		io_close(Json);
	}
	str_format(aPath, sizeof(aPath), "xxx_perf_%s.csv", m_PerfLabel.c_str());
	IOHANDLE Csv = Storage()->OpenFile(aPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(Csv)
	{
		const char *pHeader = "frame,ms\n";
		io_write(Csv, pHeader, str_length(pHeader));
		for(size_t i = 0; i < m_vFrameTimes.size(); ++i)
		{
			char aLine[64];
			str_format(aLine, sizeof(aLine), "%zu,%.6f\n", i, m_vFrameTimes[i]);
			io_write(Csv, aLine, str_length(aLine));
		}
		io_close(Csv);
	}
	m_vFrameTimes.clear();
}

#endif
