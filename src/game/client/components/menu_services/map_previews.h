#ifndef GAME_CLIENT_COMPONENTS_MENU_SERVICES_MAP_PREVIEWS_H
#define GAME_CLIENT_COMPONENTS_MENU_SERVICES_MAP_PREVIEWS_H

#include <base/vmath.h>

#include <engine/graphics.h>
#include <engine/image.h>

#include <game/client/component.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

class CLayers;
class CTile;
class CTeleTile;

class CMapPreviews : public CComponent
{
public:
	struct SPreview
	{
		IGraphics::CTextureHandle m_Texture;
		ivec2 m_Size = ivec2(0, 0);
		vec2 m_Focus = vec2(-1.0f, -1.0f);
		bool Valid() const { return m_Texture.IsValid(); }
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnMapLoad() override;
	void OnUpdate() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnShutdown() override;

	static bool BuildImage(const CTile *pGame, const CTile *pFront, const CTeleTile *pTele, int Width, int Height, int Scale, CImageInfo &Image);
	static bool BuildImage(CLayers &Layers, int Scale, CImageInfo &Image);

	const SPreview &Current() const { return m_Current; }
	SPreview Preview(const char *pMap, int Crc);
	SPreview Artwork(const char *pMap);

	void DrawPreview(const SPreview &Map, float X, float Y, float W, float H, float Alpha, vec2 Focus = vec2(-1.0f, -1.0f), bool Overview = false);

private:
	class CPreviewJob;
	struct SEntry
	{
		std::shared_ptr<CPreviewJob> m_pJob;
		std::shared_ptr<class IHttpRequest> m_pHttp;
		SPreview m_Preview;
		int64_t m_LastUse = 0;
		size_t m_TextureBytes = 0;
		bool m_Failed = false;
		bool m_Thumb = false;
	};

	void UpdateThumb(const char *pMap, SEntry &Entry);
	void LoadCurrent();
	void UnloadCurrent();
	void FlushDeferred();
	std::vector<IGraphics::CTextureHandle> m_vDeferredUnload;
	void UnloadPreviews();
	void DrawTexture(const SPreview &Map, float X, float Y, float W, float H, vec2 WorldMin, vec2 WorldMax, float Alpha);

	SPreview m_Current;
	std::map<std::string, SEntry> m_Previews;
	int m_RunningJobs = 0;
};

#endif
