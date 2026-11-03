#include "map_previews.h"

#include <base/io.h>
#include <base/mem.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/engine.h>
#include <engine/gfx/image_loader.h>
#include <engine/gfx/image_manipulation.h>
#include <engine/http.h>
#include <engine/map.h>
#include <engine/shared/config.h>
#include <engine/shared/jobs.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/layers.h>
#include <game/mapitems.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
	enum
	{
		CLASS_EMPTY = 0,
		CLASS_UNFREEZE,
		CLASS_TELE,
		CLASS_RACE,
		CLASS_FREEZE,
		CLASS_DEEP,
		CLASS_DEATH,
		CLASS_NOHOOK,
		CLASS_SOLID,
		CLASS_NOHOOK_EDGE,
		CLASS_SOLID_EDGE,
		NUM_CLASSES,
	};

	const uint8_t s_aaPalette[NUM_CLASSES][4] = {
		{14, 16, 24, 150},
		{90, 220, 140, 175},
		{196, 128, 255, 215},
		{255, 214, 10, 240},
		{92, 150, 255, 180},
		{52, 92, 232, 225},
		{255, 82, 72, 235},
		{54, 58, 74, 215},
		{80, 86, 106, 205},
		{150, 155, 178, 255},
		{236, 239, 248, 255},
	};

	int ClassOf(int Index)
	{
		switch(Index)
		{
		case TILE_SOLID: return CLASS_SOLID;
		case TILE_NOHOOK: return CLASS_NOHOOK;
		case TILE_DEATH: return CLASS_DEATH;
		case TILE_FREEZE:
		case TILE_LFREEZE: return CLASS_FREEZE;
		case TILE_DFREEZE: return CLASS_DEEP;
		case TILE_UNFREEZE:
		case TILE_DUNFREEZE:
		case TILE_LUNFREEZE: return CLASS_UNFREEZE;
		case TILE_START:
		case TILE_FINISH: return CLASS_RACE;
		default: return CLASS_EMPTY;
		}
	}

	bool IsTeleIn(int Type)
	{
		return Type == TILE_TELEIN || Type == TILE_TELEINEVIL || Type == TILE_TELECHECKIN || Type == TILE_TELECHECKINEVIL;
	}

	template<typename T>
	const T *LayerData(IMap *pMap, int Data, int Width, int Height)
	{
		if(Data < 0 || pMap->GetDataSize(Data) < (int)(sizeof(T) * Width * Height))
			return nullptr;
		return static_cast<const T *>(pMap->GetData(Data));
	}

	struct SFindMap
	{
		char m_aPrefix[IO_MAX_PATH_LENGTH];
		char m_aFound[IO_MAX_PATH_LENGTH] = "";
	};

	vec2 PreviewFocus(CLayers &Layers)
	{
		const auto *pLayer = Layers.GameLayer();
		if(!pLayer)
			return vec2(-1.0f, -1.0f);
		const auto *pTiles = LayerData<CTile>(Layers.Map(), pLayer->m_Data, pLayer->m_Width, pLayer->m_Height);
		if(!pTiles)
			return vec2(-1.0f, -1.0f);
		vec2 Start(-1.0f, -1.0f);
		for(int i = 0; i < pLayer->m_Width * pLayer->m_Height; ++i)
		{
			const vec2 Position((i % pLayer->m_Width) * 32.0f + 16.0f, (i / pLayer->m_Width) * 32.0f + 16.0f);
			if(pTiles[i].m_Index >= ENTITY_OFFSET + ENTITY_SPAWN && pTiles[i].m_Index <= ENTITY_OFFSET + ENTITY_SPAWN_BLUE)
				return Position;
			if(Start.x < 0.0f && pTiles[i].m_Index == TILE_START)
				Start = Position;
		}
		return Start;
	}

	int FindDownloaded(const char *pName, int IsDir, int DirType, void *pUser)
	{
		SFindMap *pFind = static_cast<SFindMap *>(pUser);
		if(IsDir || pFind->m_aFound[0] != '\0' || !str_startswith(pName, pFind->m_aPrefix) || !str_endswith(pName, ".map"))
			return 0;
		str_format(pFind->m_aFound, sizeof(pFind->m_aFound), "downloadedmaps/%s", pName);
		return 1;
	}
}

class CMapPreviews::CPreviewJob : public IJob
{
public:
	CPreviewJob(IStorage *pStorage, const char *pMap, int Crc) :
		m_pStorage(pStorage), m_Crc(Crc)
	{
		str_copy(m_aMap, pMap);
		Abortable(true);
	}

	CImageInfo m_Image;
	ivec2 m_Size = ivec2(0, 0);
	vec2 m_Focus = vec2(-1.0f, -1.0f);
	bool m_Ok = false;

protected:
	void Run() override
	{
		char aPath[IO_MAX_PATH_LENGTH];
		str_format(aPath, sizeof(aPath), "downloadedmaps/%s_%08x.map", m_aMap, m_Crc);
		if(!m_pStorage->FileExists(aPath, IStorage::TYPE_ALL))
		{
			str_format(aPath, sizeof(aPath), "maps/%s.map", m_aMap);
			if(!m_pStorage->FileExists(aPath, IStorage::TYPE_ALL))
			{
				SFindMap Find;
				str_format(Find.m_aPrefix, sizeof(Find.m_aPrefix), "%s_", m_aMap);
				m_pStorage->ListDirectory(IStorage::TYPE_ALL, "downloadedmaps", FindDownloaded, &Find);
				if(Find.m_aFound[0] == '\0')
					return;
				str_copy(aPath, Find.m_aFound);
			}
		}
		if(State() == IJob::STATE_ABORTED)
			return;
		std::unique_ptr<IMap> pMap = CreateMap();
		if(!pMap->Load(m_pStorage, aPath, IStorage::TYPE_ALL))
			return;
		CLayers Layers;
		Layers.Init(pMap.get(), true, false);
		if(Layers.GameLayer() == nullptr)
			return;
		m_Size = ivec2(Layers.GameLayer()->m_Width, Layers.GameLayer()->m_Height);
		m_Focus = PreviewFocus(Layers);
		m_Ok = BuildImage(Layers, 1, m_Image);
	}

private:
	IStorage *m_pStorage;
	char m_aMap[MAX_MAP_LENGTH];
	int m_Crc;
};

bool CMapPreviews::BuildImage(const CTile *pGame, const CTile *pFront, const CTeleTile *pTele, int Width, int Height, int Scale, CImageInfo &Image)
{
	if(pGame == nullptr || Width <= 0 || Height <= 0)
		return false;
	std::vector<uint8_t> vClasses((size_t)Width * Height);
	for(int i = 0; i < Width * Height; i++)
	{
		int Class = ClassOf(pGame[i].m_Index);
		if(pFront != nullptr)
			Class = std::max(Class, ClassOf(pFront[i].m_Index));
		if(pTele != nullptr && IsTeleIn(pTele[i].m_Type))
			Class = std::max(Class, (int)CLASS_TELE);
		vClasses[i] = Class;
	}
	for(int y = 0; y < Height; y++)
	{
		for(int x = 0; x < Width; x++)
		{
			uint8_t &Class = vClasses[(size_t)y * Width + x];
			if(Class != CLASS_SOLID && Class != CLASS_NOHOOK)
				continue;
			bool Edge = false;
			const int aNeighbors[4][2] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
			for(const auto &N : aNeighbors)
			{
				if(N[0] < 0 || N[1] < 0 || N[0] >= Width || N[1] >= Height)
					continue;
				const int Other = vClasses[(size_t)N[1] * Width + N[0]];
				if(Other != CLASS_SOLID && Other != CLASS_NOHOOK && Other != CLASS_SOLID_EDGE && Other != CLASS_NOHOOK_EDGE)
					Edge = true;
			}
			if(Edge)
				Class = Class == CLASS_SOLID ? CLASS_SOLID_EDGE : CLASS_NOHOOK_EDGE;
		}
	}

	const int Step = std::max(1, (std::max(Width, Height) + 4095) / 4096);
	Scale = Step > 1 ? 1 : std::max(1, Scale);
	Image.Free();
	Image.m_Width = (size_t)(Width + Step - 1) / Step * Scale;
	Image.m_Height = (size_t)(Height + Step - 1) / Step * Scale;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.AllocateFillZero();
	for(size_t y = 0; y < Image.m_Height; y++)
	{
		for(size_t x = 0; x < Image.m_Width; x++)
		{
			int Class = CLASS_EMPTY;
			const int TileX = (int)(x / Scale) * Step;
			const int TileY = (int)(y / Scale) * Step;
			for(int dy = 0; dy < Step && TileY + dy < Height; dy++)
				for(int dx = 0; dx < Step && TileX + dx < Width; dx++)
					Class = std::max(Class, (int)vClasses[(size_t)(TileY + dy) * Width + TileX + dx]);
			mem_copy(&Image.m_pData[(y * Image.m_Width + x) * 4], s_aaPalette[Class], 4);
		}
	}
	return true;
}

bool CMapPreviews::BuildImage(CLayers &Layers, int Scale, CImageInfo &Image)
{
	const CMapItemLayerTilemap *pGameLayer = Layers.GameLayer();
	if(pGameLayer == nullptr)
		return false;
	const int Width = pGameLayer->m_Width;
	const int Height = pGameLayer->m_Height;
	IMap *pMap = Layers.Map();
	const CTile *pFront = Layers.FrontLayer() ? LayerData<CTile>(pMap, Layers.FrontLayer()->m_Front, Width, Height) : nullptr;
	const CTeleTile *pTele = Layers.TeleLayer() ? LayerData<CTeleTile>(pMap, Layers.TeleLayer()->m_Tele, Width, Height) : nullptr;
	return BuildImage(LayerData<CTile>(pMap, pGameLayer->m_Data, Width, Height), pFront, pTele, Width, Height, Scale, Image);
}

void CMapPreviews::OnMapLoad()
{
	UnloadCurrent();
	const CMapItemLayerTilemap *pGameLayer = Layers()->GameLayer();
	if(pGameLayer == nullptr)
		return;
	const int Scale = std::clamp(2048 / std::max(1, std::max(pGameLayer->m_Width, pGameLayer->m_Height)), 1, 4);
	CImageInfo Image;
	if(!BuildImage(*Layers(), Scale, Image))
		return;
	m_Current.m_Size = ivec2(pGameLayer->m_Width, pGameLayer->m_Height);
	m_Current.m_Focus = PreviewFocus(*Layers());
	m_Current.m_Texture = Graphics()->LoadTextureRawMove(Image, 0, "minimap");
}





void CMapPreviews::OnUpdate()
{
	FlushDeferred();
	if(m_Previews.size() <= 48)
		return;
	const int64_t Now = time_get();
	for(auto It = m_Previews.begin(); It != m_Previews.end();)
	{
		SEntry &Entry = It->second;
		if(Entry.m_pJob == nullptr && Now - Entry.m_LastUse > time_freq() * 30)
		{
			Graphics()->UnloadTexture(&Entry.m_Preview.m_Texture);
			It = m_Previews.erase(It);
		}
		else
			++It;
	}
}

void CMapPreviews::OnStateChange(int NewState, int OldState)
{
	if(NewState == IClient::STATE_OFFLINE)
		UnloadCurrent();
}

void CMapPreviews::FlushDeferred()
{
	for(IGraphics::CTextureHandle &Texture : m_vDeferredUnload)
		Graphics()->UnloadTexture(&Texture);
	m_vDeferredUnload.clear();
}

void CMapPreviews::OnShutdown()
{
	UnloadCurrent();
	FlushDeferred();
	UnloadPreviews();
}

void CMapPreviews::UnloadCurrent()
{
	if(m_Current.m_Texture.IsValid())
		m_vDeferredUnload.push_back(m_Current.m_Texture);
	m_Current = SPreview();
}

void CMapPreviews::UnloadPreviews()
{
	for(auto &[_, Entry] : m_Previews)
	{
		if(Entry.m_pJob)
			Entry.m_pJob->Abort();
		Graphics()->UnloadTexture(&Entry.m_Preview.m_Texture);
	}
	m_Previews.clear();
}

CMapPreviews::SPreview CMapPreviews::Preview(const char *pMap, int Crc)
{
	if(pMap == nullptr || pMap[0] == '\0')
		return SPreview();
	if(m_Current.Valid() && Client()->State() == IClient::STATE_ONLINE && GameClient()->Map()->IsLoaded() && str_comp(pMap, GameClient()->Map()->BaseName()) == 0)
		return m_Current;
	char aKey[MAX_MAP_LENGTH + 16];
	str_format(aKey, sizeof(aKey), "%s_%08x", pMap, Crc);
	auto It = m_Previews.find(aKey);
	if(It == m_Previews.end())
	{
		int Running = 0;
		for(const auto &[_, Entry] : m_Previews)
			Running += Entry.m_pJob != nullptr;
		if(Running >= 2)
			return SPreview();
		It = m_Previews.emplace(aKey, SEntry()).first;
		It->second.m_pJob = std::make_shared<CPreviewJob>(Storage(), pMap, Crc);
		Engine()->AddJob(It->second.m_pJob);
	}
	SEntry &Entry = It->second;
	Entry.m_LastUse = time_get();
	if(Entry.m_pJob && Entry.m_pJob->Done())
	{
		if(Entry.m_pJob->m_Ok)
		{
			Entry.m_Preview.m_Size = Entry.m_pJob->m_Size;
			Entry.m_Preview.m_Focus = Entry.m_pJob->m_Focus;
			Entry.m_Preview.m_Texture = Graphics()->LoadTextureRawMove(Entry.m_pJob->m_Image, 0, aKey);
		}
		else
			Entry.m_Failed = true;
		Entry.m_pJob = nullptr;
	}
	if(Entry.m_Failed && !Entry.m_Preview.Valid())
		UpdateThumb(pMap, Entry);
	return Entry.m_Preview;
}

CMapPreviews::SPreview CMapPreviews::Artwork(const char *pMap)
{
	if(!pMap || !pMap[0])
		return {};
	SEntry &Entry = m_Previews[std::string("art:") + pMap];
	Entry.m_LastUse = time_get();
	if(!Entry.m_Preview.Valid())
		UpdateThumb(pMap, Entry);
	if(!Entry.m_Preview.Valid() && Entry.m_Thumb && !Entry.m_pHttp)
		return Preview(pMap, 0);
	return Entry.m_Preview;
}

void CMapPreviews::UpdateThumb(const char *pMap, SEntry &Entry)
{
	char aName[MAX_MAP_LENGTH];
	str_copy(aName, pMap);
	for(char *p = aName; *p; p++)
		if(*p == ' ' || *p == '/' || *p == '\\')
			*p = '_';
	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "mapthumbs/%s.png", aName);
	auto LoadFile = [&]() {
		CImageInfo Image;
		IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
		if(!File)
			return false;
		unsigned char aSignature[8];
		const bool Png = io_read(File, aSignature, sizeof(aSignature)) == sizeof(aSignature) && mem_comp(aSignature, "\x89PNG\r\n\x1a\n", sizeof(aSignature)) == 0;
		io_close(File);
		if(!Png)
			return false;
		File = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
		int PngliteIncompatible;
		if(CImageLoader::LoadPng(File, aPath, Image, PngliteIncompatible) && Image.m_Width > 0 && Image.m_Height > 0)
		{
			Entry.m_Preview.m_Size = ivec2(Image.m_Width, Image.m_Height);
			ConvertToRgba(Image);
			Entry.m_Preview.m_Texture = Graphics()->LoadTextureRawMove(Image, 0, aPath);
			return true;
		}
		Image.Free();
		return false;
	};
	if(Entry.m_pHttp)
	{
		const EHttpState State = Entry.m_pHttp->State();
		if(State == EHttpState::QUEUED || State == EHttpState::RUNNING)
			return;
		const bool Ok = State == EHttpState::DONE && Entry.m_pHttp->StatusCode() == 200;
		Entry.m_pHttp = nullptr;
		if(!Ok || !LoadFile())
			Storage()->RemoveFile(aPath, IStorage::TYPE_SAVE);
		return;
	}
	if(Entry.m_Thumb)
		return;
	if(Storage()->FileExists(aPath, IStorage::TYPE_SAVE))
	{
		if(LoadFile())
		{
			Entry.m_Thumb = true;
			return;
		}
		Storage()->RemoveFile(aPath, IStorage::TYPE_SAVE);
	}
	int Running = 0;
	for(const auto &[_, Other] : m_Previews)
		Running += Other.m_pHttp != nullptr;
	if(Running >= 3)
		return;
	Entry.m_Thumb = true;
	Storage()->CreateFolder("mapthumbs", IStorage::TYPE_SAVE);
	char aEscaped[MAX_MAP_LENGTH * 3];
	EscapeUrl(aEscaped, aName);
	char aUrl[512];
	str_format(aUrl, sizeof(aUrl), "https://ddnet.org/ranks/maps/%s.png", aEscaped);
	std::shared_ptr<IHttpRequest> pRequest = HttpGetFile(aUrl, Storage(), aPath, IStorage::TYPE_SAVE);
	pRequest->Timeout(CTimeout{8000, 0, 500, 10});
	pRequest->LogProgress(HTTPLOG::NONE);
	pRequest->FailOnErrorStatus(true);
	Entry.m_pHttp = pRequest;
	Http()->Run(pRequest);
}

void CMapPreviews::DrawTexture(const SPreview &Map, float X, float Y, float W, float H, vec2 WorldMin, vec2 WorldMax, float Alpha)
{
	const vec2 MapSize = vec2(Map.m_Size.x, Map.m_Size.y) * 32.0f;
	const float X0 = std::max(WorldMin.x, 0.0f);
	const float Y0 = std::max(WorldMin.y, 0.0f);
	const float X1 = std::min(WorldMax.x, MapSize.x);
	const float Y1 = std::min(WorldMax.y, MapSize.y);
	if(X1 <= X0 || Y1 <= Y0)
		return;
	const vec2 Span = WorldMax - WorldMin;
	Graphics()->WrapClamp();
	Graphics()->TextureSet(Map.m_Texture);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
	Graphics()->QuadsSetSubset(X0 / MapSize.x, Y0 / MapSize.y, X1 / MapSize.x, Y1 / MapSize.y);
	const IGraphics::CQuadItem Quad(X + (X0 - WorldMin.x) / Span.x * W, Y + (Y0 - WorldMin.y) / Span.y * H, (X1 - X0) / Span.x * W, (Y1 - Y0) / Span.y * H);
	Graphics()->QuadsDrawTL(&Quad, 1);
	Graphics()->QuadsEnd();
	Graphics()->WrapNormal();
}

void CMapPreviews::DrawPreview(const SPreview &Map, float X, float Y, float W, float H, float Alpha, vec2 Focus, bool Overview)
{
	if(!Map.Valid() || Map.m_Size.x <= 0 || Map.m_Size.y <= 0 || W <= 0.0f || H <= 0.0f)
		return;
	const vec2 MapSize = vec2(Map.m_Size.x, Map.m_Size.y) * 32.0f;
	if(Focus.x < 0.0f && !Overview)
		Focus = Map.m_Focus;
	const float MapAspect = MapSize.x / MapSize.y;
	const float RectAspect = W / H;
	const float Ratio = std::max(MapAspect / RectAspect, RectAspect / MapAspect);
	if(Overview || (Focus.x < 0.0f && Ratio < 2.2f))
	{
		const float Zoom = std::min(W / Map.m_Size.x, H / Map.m_Size.y);
		const vec2 Size = vec2(Map.m_Size.x, Map.m_Size.y) * Zoom;
		const vec2 Pos = vec2(X + (W - Size.x) * 0.5f, Y + (H - Size.y) * 0.5f);
		DrawTexture(Map, Pos.x, Pos.y, Size.x, Size.y, vec2(0.0f, 0.0f), MapSize, Alpha);
		return;
	}
	vec2 Span = MapAspect > RectAspect ? vec2(MapSize.y * RectAspect, MapSize.y) : vec2(MapSize.x, MapSize.x / RectAspect);
	if(Focus.x >= 0.0f)
	{
		Span.x = std::min(MapSize.x, 64.0f * 32.0f);
		Span.y = Span.x / RectAspect;
		if(Span.y > MapSize.y)
		{
			Span.y = MapSize.y;
			Span.x = Span.y * RectAspect;
		}
	}
	if(Focus.x < 0.0f)
		Focus = MapSize * 0.5f;
	Focus.x = Span.x < MapSize.x ? std::clamp(Focus.x, Span.x * 0.5f, MapSize.x - Span.x * 0.5f) : MapSize.x * 0.5f;
	Focus.y = Span.y < MapSize.y ? std::clamp(Focus.y, Span.y * 0.5f, MapSize.y - Span.y * 0.5f) : MapSize.y * 0.5f;
	DrawTexture(Map, X, Y, W, H, Focus - Span * 0.5f, Focus + Span * 0.5f, Alpha);
}


