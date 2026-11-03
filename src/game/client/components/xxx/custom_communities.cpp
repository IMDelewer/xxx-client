#include "custom_communities.h"

#include <base/log.h>

#include <engine/client/serverbrowser.h>
#include <engine/external/json-parser/json.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

static constexpr const char *CUSTOM_COMMUNITIES_FILE = "custom-communities-ddnet-info.json";

void CCustomCommunities::Download()
{
	if(m_pTask)
	{
		m_pTask->Abort();
		m_pTask = nullptr;
	}
	if(g_Config.m_XxCustomCommunitiesUrl[0] == '\0' || str_comp_num(g_Config.m_XxCustomCommunitiesUrl, "https://", 8) != 0)
	{
		Load();
		return;
	}
	m_pTask = HttpGetFile(g_Config.m_XxCustomCommunitiesUrl, Storage(), CUSTOM_COMMUNITIES_FILE, IStorage::TYPE_SAVE);
	m_pTask->Timeout(CTimeout{10000, 0, 500, 10});
	m_pTask->SkipByFileTime(false);
	m_pTask->IpResolve(IPRESOLVE::V4);
	Http()->Run(m_pTask);
}

void CCustomCommunities::Load()
{
	CServerBrowser *pBrowser = dynamic_cast<CServerBrowser *>(ServerBrowser());
	if(pBrowser == nullptr)
		return;

	if(g_Config.m_XxCustomCommunitiesUrl[0] == '\0')
	{
		pBrowser->m_CustomCommunitiesFunction = nullptr;
		pBrowser->LoadDDNetInfo();
		return;
	}

	void *pBuf;
	unsigned Length;
	if(!Storage()->ReadFile(CUSTOM_COMMUNITIES_FILE, IStorage::TYPE_SAVE, &pBuf, &Length))
		return;
	json_settings Settings{};
	char aError[256];
	json_value *pInfo = json_parse_ex(&Settings, static_cast<json_char *>(pBuf), Length, aError);
	free(pBuf);
	if(pInfo == nullptr || pInfo->type != json_object)
	{
		log_error("customcommunities", "invalid json: '%s'", pInfo == nullptr ? aError : "root is not an object");
		if(pInfo != nullptr)
			json_value_free(pInfo);
		return;
	}

	pBrowser->m_CustomCommunitiesFunction = nullptr;
	if(m_pInfo)
		json_value_free(m_pInfo);
	m_pInfo = pInfo;
	pBrowser->m_CustomCommunitiesFunction = [this](std::vector<json_value *> &vCommunities) {
		if(m_pInfo == nullptr)
			return;
		const json_value &Communities = (*m_pInfo)["communities"];
		if(Communities.type == json_array)
			vCommunities.insert(vCommunities.end(), Communities.u.array.values, Communities.u.array.values + Communities.u.array.length);
	};
	pBrowser->LoadDDNetInfo();
}

void CCustomCommunities::OnConsoleInit()
{
	Console()->Chain(
		"xx_custom_communities_url", [](IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData) {
			pfnCallback(pResult, pCallbackUserData);
			if(pResult->NumArguments() > 0)
				static_cast<CCustomCommunities *>(pUserData)->Download();
		},
		this);
}

void CCustomCommunities::OnInit()
{
	Load();
	Download();
}

void CCustomCommunities::OnRender()
{
	if(!m_pTask)
		return;
	const EHttpState State = m_pTask->State();
	if(State == EHttpState::RUNNING || State == EHttpState::QUEUED)
		return;
	m_pTask = nullptr;
	Load();
}

void CCustomCommunities::OnShutdown()
{
	if(m_pTask)
	{
		m_pTask->Abort();
		m_pTask = nullptr;
	}
	CServerBrowser *pBrowser = dynamic_cast<CServerBrowser *>(ServerBrowser());
	if(pBrowser != nullptr)
		pBrowser->m_CustomCommunitiesFunction = nullptr;
	if(m_pInfo)
	{
		json_value_free(m_pInfo);
		m_pInfo = nullptr;
	}
}
