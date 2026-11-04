#ifndef GAME_CLIENT_COMPONENTS_MENU_SERVICES_TEEDATA_H
#define GAME_CLIENT_COMPONENTS_MENU_SERVICES_TEEDATA_H

#include <engine/graphics.h>
#include <engine/http.h>

#include <game/client/component.h>

#include <memory>
#include <string>
#include <vector>

class CTeedataCatalog : public CComponent
{
	class CPreviewJob;

public:
	struct SItem
	{
		int m_Id = 0;
		std::string m_Name, m_Author, m_File, m_Thumbnail, m_LocalName;
		IGraphics::CTextureHandle m_Texture;
		vec2 m_Size = vec2(1, 1);
		std::shared_ptr<IHttpRequest> m_pPreview;
		std::shared_ptr<CPreviewJob> m_pDecode;
		bool m_PreviewRequested = false;
	};
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnShutdown() override;
	void Request(int Category, const char *pSearch, int Page);
	void Install(int Index);
	void Apply(int Index);
	bool Installed(int Index) const;
	bool Loading() const { return m_pList != nullptr; }
	int InstallingId() const { return m_pInstall ? m_Installing.m_Id : -1; }
	int Category() const { return m_Category; }
	int Page() const { return m_Page; }
	int Total() const { return m_Total; }
	const char *Error() const { return m_Error.c_str(); }
	const std::vector<SItem> &Items() const { return m_vItems; }
	static constexpr int PAGE_SIZE = 12;

private:
	std::vector<SItem> m_vItems;
	std::shared_ptr<IHttpRequest> m_pList, m_pInstall;
	SItem m_Installing;
	int m_Category = -1, m_InstallCategory = -1, m_Page = 1, m_Total = 0;
	std::string m_Error;
	void ClearItems();
	void ReadList();
	void FinishInstall();
	static std::string LocalPath(int Category, const SItem &Item);
};
#endif
