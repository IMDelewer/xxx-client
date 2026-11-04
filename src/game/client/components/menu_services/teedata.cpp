#include "teedata.h"

#include <base/io.h>
#include <base/str.h>

#include <engine/engine.h>
#include <engine/external/json-parser/json.h>
#include <engine/gfx/image_loader.h>
#include <engine/gfx/image_manipulation.h>
#include <engine/shared/config.h>
#include <engine/shared/jobs.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/localization.h>

#include <algorithm>

namespace
{
	const char *const gs_apEndpoints[] = {"entity", "gameskin", "emoticon", "particle", "mapres", "skin"};
	const char *const gs_apFolders[] = {"assets/entities", "assets/game", "assets/emoticons", "assets/particles", "mapres", "skins"};
	std::string String(const json_value &Value) { return Value.type == json_string ? Value.u.string.ptr : ""; }
	std::string AssetUrl(const std::string &Path)
	{
		if(Path.empty() || Path[0] != '/' || Path.find("..") != std::string::npos || Path.find_first_of("?#\\\r\n") != std::string::npos)
			return "";
		std::string Url = "https://teedata.net/databasev2";
		for(unsigned char Ch : Path)
		{
			if(Ch == '/' || (Ch >= 'a' && Ch <= 'z') || (Ch >= 'A' && Ch <= 'Z') || (Ch >= '0' && Ch <= '9') || Ch == '-' || Ch == '_' || Ch == '.')
				Url += Ch;
			else
			{
				char aEncoded[4];
				str_format(aEncoded, sizeof(aEncoded), "%%%02X", Ch);
				Url += aEncoded;
			}
		}
		return Url.size() < 256 ? Url : "";
	}
}

class CTeedataCatalog::CPreviewJob : public IJob
{
	std::shared_ptr<IHttpRequest> m_pRequest;

public:
	CImageInfo m_Image;
	vec2 m_Size = vec2(1, 1);
	bool m_Ok = false;

	explicit CPreviewJob(std::shared_ptr<IHttpRequest> pRequest) :
		m_pRequest(std::move(pRequest))
	{
		Abortable(false);
	}

protected:
	void Run() override
	{
		unsigned char *pData;
		size_t Size;
		m_pRequest->Result(&pData, &Size);
		CByteBufferReader Reader(pData, Size);
		int Incompatible;
		if(!CImageLoader::LoadPng(Reader, "teedata preview", m_Image, Incompatible) || !ConvertToRgba(m_Image))
			return;
		m_Size = vec2(m_Image.m_Width, m_Image.m_Height);
		const size_t Largest = std::max(m_Image.m_Width, m_Image.m_Height);
		if(Largest > 512)
			ResizeImage(m_Image, std::max<size_t>(1, m_Image.m_Width * 512 / Largest), std::max<size_t>(1, m_Image.m_Height * 512 / Largest));
		m_Ok = true;
		m_pRequest.reset();
	}
};

void CTeedataCatalog::ClearItems()
{
	for(SItem &Item : m_vItems)
	{
		if(Item.m_pPreview)
			Item.m_pPreview->Abort();
		if(Item.m_Texture.IsValid())
			Graphics()->UnloadTexture(&Item.m_Texture);
	}
	m_vItems.clear();
}

void CTeedataCatalog::OnShutdown()
{
	if(m_pList)
		m_pList->Abort();
	if(m_pInstall)
		m_pInstall->Abort();
	ClearItems();
}

void CTeedataCatalog::Request(int Category, const char *pSearch, int Page)
{
	if(Category < 0 || Category >= (int)std::size(gs_apEndpoints))
		return;
	if(m_pList)
		m_pList->Abort();
	ClearItems();
	m_Category = Category;
	m_Page = std::max(1, Page);
	m_Total = 0;
	m_Error.clear();
	char aSearch[128], aUrl[256];
	EscapeUrl(aSearch, pSearch);
	str_format(aUrl, sizeof(aUrl), "https://teedata.net/api/%s/read?status=accepted&sort=download&limit=%d&page=%d&name=%s", gs_apEndpoints[Category], PAGE_SIZE, m_Page, aSearch);
	m_pList = HttpGet(aUrl);
	m_pList->Timeout({10000, 30000, 100, 10});
	m_pList->MaxResponseSize(2 * 1024 * 1024);
	Http()->Run(m_pList);
}

void CTeedataCatalog::ReadList()
{
	json_value *pRoot = m_pList->State() == EHttpState::DONE ? m_pList->ResultJson() : nullptr;
	m_pList.reset();
	if(!pRoot)
	{
		m_Error = "Could not load the catalog. Try again.";
		return;
	}
	const json_value &Data = (*pRoot)["result"].type == json_object ? (*pRoot)["result"] : *pRoot;
	const json_value &Items = Data["items"];
	if(Items.type != json_array || Data["totalCount"].type != json_integer)
		m_Error = "The catalog returned an unsupported response";
	else
	{
		m_Total = std::max(0, (int)Data["totalCount"].u.integer);
		for(unsigned i = 0; i < Items.u.array.length && i < PAGE_SIZE; i++)
		{
			const json_value &Value = Items[i];
			if(Value["id"].type != json_integer || String(Value["extension"]) != ".png")
				continue;
			SItem Item;
			Item.m_Id = (int)Value["id"].u.integer;
			Item.m_Name = String(Value["name"]);
			Item.m_Author = String(Value["author"]["name"]);
			Item.m_File = AssetUrl(String(Value["file_path"]));
			Item.m_Thumbnail = AssetUrl(String(Value["thumbnail_path"]));
			if(Item.m_Id <= 0 || Item.m_Name.empty() || Item.m_File.empty())
				continue;
			char aLocal[50];
			str_format(aLocal, sizeof(aLocal), "teedata_%d", Item.m_Id);
			Item.m_LocalName = aLocal;
			m_vItems.push_back(std::move(Item));
		}
	}
	json_value_free(pRoot);
}

std::string CTeedataCatalog::LocalPath(int Category, const SItem &Item)
{
	return std::string(gs_apFolders[Category]) + "/" + Item.m_LocalName + ".png";
}

bool CTeedataCatalog::Installed(int Index) const
{
	return m_Category >= 0 && Index >= 0 && Index < (int)m_vItems.size() && Storage()->FileExists(LocalPath(m_Category, m_vItems[Index]).c_str(), IStorage::TYPE_SAVE);
}

void CTeedataCatalog::Install(int Index)
{
	if(m_pInstall || Index < 0 || Index >= (int)m_vItems.size())
		return;
	m_Error.clear();
	m_Installing = m_vItems[Index];
	m_InstallCategory = m_Category;
	m_pInstall = HttpGet(m_Installing.m_File.c_str());
	m_pInstall->HeaderString("Referer", "https://teedata.net/");
	m_pInstall->HeaderString("User-Agent", "Mozilla/5.0");
	m_pInstall->HeaderString("Accept-Encoding", "identity");
	m_pInstall->HeaderString("Connection", "close");
	m_pInstall->Timeout({10000, 60000, 100, 15});
	m_pInstall->MaxResponseSize(16 * 1024 * 1024);
	Http()->Run(m_pInstall);
}

void CTeedataCatalog::Apply(int Index)
{
	if(!Installed(Index))
		return;
	if(m_Category < 4)
		GameClient()->m_Menus.SelectAsset(m_Category, m_vItems[Index].m_LocalName.c_str());
	else if(m_Category == 5)
	{
		str_copy(g_Config.m_ClPlayerSkin, m_vItems[Index].m_LocalName.c_str());
		GameClient()->m_Menus.QueueSendInfo(false);
	}
}

void CTeedataCatalog::FinishInstall()
{
	unsigned char *pData = nullptr;
	size_t Size = 0;
	CImageInfo Image;
	if(m_pInstall->State() == EHttpState::DONE)
		m_pInstall->Result(&pData, &Size);
	if(!pData || !Graphics()->LoadPng(Image, pData, Size, "teedata"))
		m_Error = "Could not download a valid PNG. Try again.";
	else
	{
		Image.Free();
		Storage()->CreateFolder("assets", IStorage::TYPE_SAVE);
		Storage()->CreateFolder(gs_apFolders[m_InstallCategory], IStorage::TYPE_SAVE);
		const std::string Path = LocalPath(m_InstallCategory, m_Installing);
		IOHANDLE File = Storage()->OpenFile(Path.c_str(), IOFLAG_WRITE, IStorage::TYPE_SAVE);
		if(!File)
			m_Error = "Could not save the downloaded asset";
		else
		{
			const bool Written = io_write(File, pData, Size) == Size;
			const int Closed = io_close(File);
			if(!Written || Closed != 0)
			{
				Storage()->RemoveFile(Path.c_str(), IStorage::TYPE_SAVE);
				m_Error = "Could not save the downloaded asset";
			}
			else
			{
				if(m_InstallCategory < 4)
					GameClient()->m_Menus.ReloadAssets(m_InstallCategory);
				else if(m_InstallCategory == 5)
					GameClient()->m_Skins.Refresh([]() {});
				CIsland::SNotification N;
				N.m_Title = Localize("Asset installed");
				N.m_Subtitle = m_Installing.m_Name + " · " + m_Installing.m_Author;
				N.m_Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_XxUiAccent));
				GameClient()->m_Island.Push(N);
			}
		}
	}
	m_pInstall.reset();
}

void CTeedataCatalog::OnRender()
{
	if(m_pList && m_pList->Done())
		ReadList();
	if(m_pInstall && m_pInstall->Done())
		FinishInstall();
	int Active = 0;
	bool Uploaded = false;
	for(SItem &Item : m_vItems)
	{
		if(Item.m_pPreview && Item.m_pPreview->Done())
		{
			if(Item.m_pPreview->State() == EHttpState::DONE)
			{
				Item.m_pDecode = std::make_shared<CPreviewJob>(Item.m_pPreview);
				Engine()->AddJob(Item.m_pDecode);
			}
			Item.m_pPreview.reset();
		}
		if(Item.m_pDecode && Item.m_pDecode->State() == IJob::STATE_DONE && !Uploaded)
		{
			if(Item.m_pDecode->m_Ok)
			{
				Item.m_Size = Item.m_pDecode->m_Size;
				Item.m_Texture = Graphics()->LoadTextureRawMove(Item.m_pDecode->m_Image, 0, "teedata preview");
				Uploaded = true;
			}
			Item.m_pDecode.reset();
		}
		Active += Item.m_pPreview != nullptr || Item.m_pDecode != nullptr;
	}
	for(SItem &Item : m_vItems)
	{
		if(Active >= 3)
			break;
		if(Item.m_PreviewRequested || Item.m_Thumbnail.empty())
			continue;
		Item.m_PreviewRequested = true;
		Item.m_pPreview = HttpGet(Item.m_Thumbnail.c_str());
		Item.m_pPreview->HeaderString("Referer", "https://teedata.net/");
		Item.m_pPreview->HeaderString("User-Agent", "Mozilla/5.0");
		Item.m_pPreview->HeaderString("Accept-Encoding", "identity");
		Item.m_pPreview->HeaderString("Connection", "close");
		Item.m_pPreview->Timeout({10000, 60000, 10, 30});
		Item.m_pPreview->MaxResponseSize(2 * 1024 * 1024);
		Http()->Run(Item.m_pPreview);
		Active++;
	}
}
