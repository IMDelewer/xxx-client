#ifndef GAME_CLIENT_COMPONENTS_MENU_SERVICES_FONTS_H
#define GAME_CLIENT_COMPONENTS_MENU_SERVICES_FONTS_H

#include <base/hash.h>
#include <base/io.h>

#include <game/client/component.h>

#include <memory>
#include <string>
#include <vector>

class IHttpRequest;

class COptionalFonts : public CComponent
{
	struct SFont
	{
		std::string m_File;
		std::string m_Family;
		SHA256_DIGEST m_Sha256;
		int m_Size;
		bool m_Installed;
	};
	std::vector<SFont> m_vFonts;
	std::shared_ptr<IHttpRequest> m_pRequest;
	std::string m_Error;
	char m_aPendingLanguage[IO_MAX_PATH_LENGTH] = "";
	bool m_Offer = false;
	bool m_Downloading = false;
	size_t m_Next = 0;

	bool Verify(const SFont &Font) const;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnUpdate() override;
	void OnShutdown() override;
	bool Installed() const;
	bool Downloading() const { return m_Downloading; }
	bool Offer() const { return m_Offer && !m_vFonts.empty() && !Installed() && !Downloading(); }
	int Progress() const;
	const char *Error() const { return m_Error.c_str(); }
	void Start();
	void Dismiss();
	void Cancel();
	bool SelectLanguage(const char *pFilename);
	static bool NeedsFonts(const char *pFilename);
	static const char *LanguageName(const char *pFilename, const char *pName);
};

#endif
