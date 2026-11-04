#include "fonts.h"

#include <base/str.h>

#include <engine/external/json-parser/json.h>
#include <engine/http.h>
#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/localization.h>

#include <algorithm>
#include <cstdlib>

bool COptionalFonts::Verify(const SFont &Font) const
{
	const std::string Path = "fonts/" + Font.m_File;
	void *pData;
	unsigned Size;
	if(!Storage()->ReadFile(Path.c_str(), IStorage::TYPE_ALL, &pData, &Size))
		return false;
	const bool Valid = Size == (unsigned)Font.m_Size && sha256(pData, Size) == Font.m_Sha256;
	free(pData);
	return Valid;
}

void COptionalFonts::OnInit()
{
	void *pData;
	unsigned Size;
	if(!Storage()->ReadFile("fonts/index.json", IStorage::TYPE_ALL, &pData, &Size))
		return;
	json_value *pRoot = JsonParse(static_cast<const char *>(pData), Size);
	free(pData);
	if(pRoot)
	{
		const json_value &Fonts = (*pRoot)["optional font files"];
		if(Fonts.type == json_array)
			for(unsigned i = 0; i < Fonts.u.array.length; ++i)
			{
				const json_value &F = Fonts[i];
				SFont Font{};
				if(F["file"].type != json_string || F["family"].type != json_string || F["sha256"].type != json_string || F["size"].type != json_integer)
					continue;
				Font.m_File = F["file"].u.string.ptr;
				Font.m_Family = F["family"].u.string.ptr;
				if(Font.m_File.empty() || Font.m_File.find_first_of("/\\:") != std::string::npos || Font.m_File.find("..") != std::string::npos || F["size"].u.integer <= 0 || F["size"].u.integer > 32 * 1024 * 1024 || sha256_from_str(&Font.m_Sha256, F["sha256"].u.string.ptr) != 0)
					continue;
				Font.m_Size = (int)F["size"].u.integer;
				Font.m_Installed = Verify(Font) && TextRender()->LoadFontFile(("fonts/" + Font.m_File).c_str(), Font.m_Family.c_str());
				m_vFonts.push_back(std::move(Font));
			}
		json_value_free(pRoot);
	}
	if(m_vFonts.empty())
		m_Error = "Could not load the font package information";
	if(Installed())
		GameClient()->OnLanguageChange();
	m_Offer = g_Config.m_XxFontPrompt != 0 && !Installed();
	if(NeedsFonts(g_Config.m_ClLanguagefile) && !Installed())
	{
		str_copy(m_aPendingLanguage, g_Config.m_ClLanguagefile);
		g_Config.m_ClLanguagefile[0] = '\0';
		GameClient()->OnLanguageChange();
		m_Offer = true;
	}
}

bool COptionalFonts::Installed() const
{
	return !m_vFonts.empty() && std::all_of(m_vFonts.begin(), m_vFonts.end(), [](const SFont &Font) { return Font.m_Installed; });
}

bool COptionalFonts::NeedsFonts(const char *pFilename)
{
	return str_comp(pFilename, "languages/japanese.txt") == 0 || str_comp(pFilename, "languages/korean.txt") == 0 || str_comp(pFilename, "languages/simplified_chinese.txt") == 0 || str_comp(pFilename, "languages/traditional_chinese.txt") == 0;
}

const char *COptionalFonts::LanguageName(const char *pFilename, const char *pName)
{
	if(str_comp(pFilename, "languages/japanese.txt") == 0)
		return "Japanese";
	if(str_comp(pFilename, "languages/korean.txt") == 0)
		return "Korean";
	if(str_comp(pFilename, "languages/simplified_chinese.txt") == 0)
		return "Chinese (Simplified)";
	if(str_comp(pFilename, "languages/traditional_chinese.txt") == 0)
		return "Chinese (Traditional)";
	return pName;
}

bool COptionalFonts::SelectLanguage(const char *pFilename)
{
	if(NeedsFonts(pFilename) && !Installed())
	{
		str_copy(m_aPendingLanguage, pFilename);
		m_Offer = true;
		return false;
	}
	m_aPendingLanguage[0] = '\0';
	str_copy(g_Config.m_ClLanguagefile, pFilename);
	GameClient()->OnLanguageChange();
	return true;
}

void COptionalFonts::Dismiss()
{
	m_Offer = false;
	m_aPendingLanguage[0] = '\0';
	g_Config.m_XxFontPrompt = 0;
}

void COptionalFonts::Start()
{
	if(Downloading() || Installed() || m_vFonts.empty())
		return;
	m_Error.clear();
	m_Downloading = true;
	m_Offer = false;
	g_Config.m_XxFontPrompt = 0;
	m_Next = 0;
}

void COptionalFonts::Cancel()
{
	if(m_pRequest)
		m_pRequest->Abort();
	m_pRequest.reset();
	m_Downloading = false;
}

void COptionalFonts::OnShutdown()
{
	Cancel();
}

int COptionalFonts::Progress() const
{
	int64_t Total = 0;
	int64_t Loaded = 0;
	for(size_t i = 0; i < m_vFonts.size(); ++i)
	{
		Total += m_vFonts[i].m_Size;
		if(m_vFonts[i].m_Installed)
			Loaded += m_vFonts[i].m_Size;
		else if(i == m_Next && m_pRequest)
			Loaded += std::min<int64_t>(m_vFonts[i].m_Size, (int64_t)m_pRequest->Current());
	}
	return Total ? (int)(Loaded * 100 / Total) : 0;
}

void COptionalFonts::OnUpdate()
{
	if(!Downloading())
		return;
	if(m_pRequest)
	{
		if(!m_pRequest->Done())
			return;
		const bool Ok = m_pRequest->State() == EHttpState::DONE;
		m_pRequest.reset();
		if(!Ok)
		{
			m_Downloading = false;
			m_Error = "Could not download the fonts. Try again.";
			return;
		}
		m_vFonts[m_Next].m_Installed = true;
		++m_Next;
	}
	while(m_Next < m_vFonts.size() && m_vFonts[m_Next].m_Installed)
		++m_Next;
	if(m_Next == m_vFonts.size())
	{
		m_Downloading = false;
		for(SFont &Font : m_vFonts)
		{
			if(!TextRender()->LoadFontFile(("fonts/" + Font.m_File).c_str(), Font.m_Family.c_str()))
			{
				Font.m_Installed = false;
				m_Error = "Could not load the downloaded fonts";
				return;
			}
		}
		if(m_aPendingLanguage[0])
			str_copy(g_Config.m_ClLanguagefile, m_aPendingLanguage);
		m_aPendingLanguage[0] = '\0';
		GameClient()->OnLanguageChange();
		CIsland::SNotification Notice;
		Notice.m_Title = Localize("Additional fonts installed");
		GameClient()->m_Island.Push(Notice);
		return;
	}
	const SFont &Font = m_vFonts[m_Next];
	Storage()->CreateFolder("fonts", IStorage::TYPE_SAVE);
	const std::string Url = "https://raw.githubusercontent.com/ddnet/ddnet/176fc2f9ab85ace703c6ec10da0249b01995ba94/data/fonts/" + Font.m_File;
	m_pRequest = HttpGetFile(Url.c_str(), Storage(), ("fonts/" + Font.m_File).c_str(), IStorage::TYPE_SAVE);
	m_pRequest->Timeout({10000, 300000, 500, 30});
	m_pRequest->MaxResponseSize(Font.m_Size);
	m_pRequest->ExpectSha256(Font.m_Sha256);
	m_pRequest->FailOnErrorStatus(true);
	m_pRequest->LogProgress(HTTPLOG::FAILURE);
	Http()->Run(m_pRequest);
}
