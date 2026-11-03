#include "file_dialog.h"

#include <base/detect.h>

#if defined(CONF_FAMILY_WINDOWS)
#include <base/windows.h>

#include <shobjidl.h>
#include <windows.h>
#elif !defined(CONF_PLATFORM_MACOS)
#include <sys/wait.h>

#include <cstdio>
#endif

#if !defined(CONF_PLATFORM_MACOS)
SImageFileSelection SelectBackgroundImage()
{
#if defined(CONF_FAMILY_WINDOWS)
	const HRESULT Initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	IFileOpenDialog *pDialog = nullptr;
	HRESULT Result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDialog));
	SImageFileSelection Selection;
	if(SUCCEEDED(Result))
	{
		const COMDLG_FILTERSPEC Filter = {L"PNG image", L"*.png"};
		pDialog->SetFileTypes(1, &Filter);
		pDialog->SetOptions(FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
		Result = pDialog->Show(nullptr);
		if(SUCCEEDED(Result))
		{
			IShellItem *pItem = nullptr;
			Result = pDialog->GetResult(&pItem);
			if(SUCCEEDED(Result))
			{
				PWSTR pPath = nullptr;
				Result = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pPath);
				if(SUCCEEDED(Result))
				{
					const auto Path = windows_wide_to_utf8(pPath);
					if(Path)
						Selection.m_Path = *Path;
					else
						Selection.m_Error = "Could not read the selected path";
					CoTaskMemFree(pPath);
				}
				pItem->Release();
			}
		}
		pDialog->Release();
	}
	if(FAILED(Result) && Result != HRESULT_FROM_WIN32(ERROR_CANCELLED))
		Selection.m_Error = "Could not open the file chooser";
	if(SUCCEEDED(Initialized))
		CoUninitialize();
	return Selection;
#else
	const char *apCommands[] = {
		"zenity --file-selection --title='XUI background' --file-filter='PNG images | *.png *.PNG' 2>/dev/null",
		"kdialog --getopenfilename . '*.png *.PNG|PNG images' --title 'XUI background' 2>/dev/null",
	};
	for(const char *pCommand : apCommands)
	{
		FILE *pPipe = popen(pCommand, "r");
		if(pPipe == nullptr)
			continue;
		std::string Path;
		char aBuffer[1024];
		while(fgets(aBuffer, sizeof(aBuffer), pPipe))
			Path += aBuffer;
		const int Status = pclose(pPipe);
		if(Status == 0)
		{
			if(!Path.empty() && Path.back() == '\n')
				Path.pop_back();
			return {Path, ""};
		}
		if(Status >= 0 && WIFEXITED(Status) && WEXITSTATUS(Status) == 1)
			return {};
	}
	return {"", "Install zenity or kdialog to use the file chooser"};
#endif
}
#endif
