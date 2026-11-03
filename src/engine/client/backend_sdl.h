#ifndef ENGINE_CLIENT_BACKEND_SDL_H
#define ENGINE_CLIENT_BACKEND_SDL_H

#include <base/detect.h>

#include <engine/client/backend/backend_base.h>
#include <engine/client/graphics_threaded.h>
#include <engine/graphics.h>

#include <SDL_video.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#if defined(CONF_PLATFORM_MACOS) || defined(CONF_PLATFORM_IOS)
#include <objc/message.h>
#include <objc/runtime.h>

class CAutoreleasePool
{
private:
	id m_Pool;

public:
	CAutoreleasePool()
	{
		Class NSAutoreleasePoolClass = (Class)objc_getClass("NSAutoreleasePool");
		m_Pool = class_createInstance(NSAutoreleasePoolClass, 0);
		SEL selector = sel_registerName("init");
		((id (*)(id, SEL))objc_msgSend)(m_Pool, selector);
	}

	~CAutoreleasePool()
	{
		SEL selector = sel_registerName("drain");
		((id (*)(id, SEL))objc_msgSend)(m_Pool, selector);
	}
};
#endif

// basic threaded backend, abstract, missing init and shutdown functions
class CGraphicsBackend_Threaded : public IGraphicsBackend
{
private:
	TTranslateFunc m_TranslateFunc;
	std::string m_FatalError;
	SGfxWarningContainer m_Warning;

public:
	// constructed on the main thread, the rest of the functions is run on the render thread
	class ICommandProcessor
	{
	public:
		virtual ~ICommandProcessor() = default;
		virtual void RunBuffer(CCommandBuffer *pBuffer) = 0;

		virtual const SGfxErrorContainer &GetError() const = 0;
		virtual void ErroneousCleanup() = 0;

		virtual const SGfxWarningContainer &GetWarning() const = 0;
	};

	CGraphicsBackend_Threaded(TTranslateFunc &&TranslateFunc);

	void RunBuffer(CCommandBuffer *pBuffer) override;
	void RunBufferSingleThreadedUnsafe(CCommandBuffer *pBuffer) override;
	bool IsIdle() const override;
	void WaitForIdle() override;

	void ProcessError(const SGfxErrorContainer &Error);

protected:
	void StartProcessor(ICommandProcessor *pProcessor);
	void StopProcessor();

	bool HasWarning() const
	{
		return m_Warning.m_WarningType != GFX_WARNING_TYPE_NONE;
	}

private:
	ICommandProcessor *m_pProcessor;
	std::atomic_bool m_Shutdown;
#if !defined(CONF_PLATFORM_EMSCRIPTEN)
	std::mutex m_BufferSwapMutex;
	std::condition_variable m_BufferSwapCond;
	CCommandBuffer *m_pBuffer;
	bool m_Started = false;
	std::atomic_bool m_BufferInProcess;
	void *m_pThread;
	static void ThreadFunc(void *pUser);
#endif

public:
	const char *GetFatalError() const override;
	bool GetWarning(std::vector<std::string> &WarningStrings) override;
};

// takes care of implementation independent operations
class CCommandProcessorFragment_General
{
	void Cmd_Signal(const CCommandBuffer::SCommand_Signal *pCommand);

public:
	bool RunCommand(const CCommandBuffer::SCommand *pBaseCommand);
};

struct SBackendCapabilities
{
	bool m_TileBuffering;
	bool m_QuadBuffering;
	bool m_TextBuffering;
	bool m_QuadContainerBuffering;

	bool m_MipMapping;
	bool m_NPOTTextures;
	bool m_3DTextures;
	bool m_2DArrayTextures;
	bool m_ShaderSupport;

	// use quads as much as possible, even if the user config says otherwise
	bool m_TrianglesAsQuads;
};

// command processor implementation, uses the fragments to combine into one processor
class CCommandProcessor_SDL : public CGraphicsBackend_Threaded::ICommandProcessor
{
	CCommandProcessorFragment_GLBase *m_pGLBackend;
	CCommandProcessorFragment_General m_General;

	SGfxErrorContainer m_Error;
	SGfxWarningContainer m_Warning;

public:
	CCommandProcessor_SDL();
	~CCommandProcessor_SDL() override;
	void RunBuffer(CCommandBuffer *pBuffer) override;

	const SGfxErrorContainer &GetError() const override;
	void ErroneousCleanup() override;

	const SGfxWarningContainer &GetWarning() const override;

	void HandleError();
	void HandleWarning();
};

static constexpr size_t GPU_INFO_STRING_SIZE = 256;

// graphics backend implemented with SDL and the Vulkan
class CGraphicsBackend_SDL : public CGraphicsBackend_Threaded
{
	SDL_Window *m_pWindow = nullptr;
	ICommandProcessor *m_pProcessor = nullptr;
	std::atomic<uint64_t> m_TextureMemoryUsage{0};
	std::atomic<uint64_t> m_BufferMemoryUsage{0};
	std::atomic<uint64_t> m_StreamMemoryUsage{0};
	std::atomic<uint64_t> m_StagingMemoryUsage{0};
	std::atomic<uint64_t> m_UiBlurGpuNs{0};
	std::atomic<uint64_t> m_UiRectsGpuNs{0};

	TTwGraphicsGpuList m_GpuList;

	TGLBackendReadPresentedImageData m_ReadPresentedImageDataFunc;

	int m_NumScreens;

	SBackendCapabilities m_Capabilities;

	char m_aVendorString[GPU_INFO_STRING_SIZE] = {};
	char m_aVersionString[GPU_INFO_STRING_SIZE] = {};
	char m_aRendererString[GPU_INFO_STRING_SIZE] = {};

	char m_aErrorString[256];

public:
	CGraphicsBackend_SDL(TTranslateFunc &&TranslateFunc);
	int Init(const char *pName, int *pScreen, int *pWidth, int *pHeight, int *pRefreshRate, int *pFsaaSamples, int Flags, int *pDesktopWidth, int *pDesktopHeight, int *pCurrentWidth, int *pCurrentHeight, class IStorage *pStorage) override;
	int Shutdown() override;

	uint64_t TextureMemoryUsage() const override;
	uint64_t BufferMemoryUsage() const override;
	uint64_t StreamedMemoryUsage() const override;
	uint64_t StagingMemoryUsage() const override;

	const TTwGraphicsGpuList &GetGpus() const override;

	int GetNumScreens() const override { return m_NumScreens; }
	const char *GetScreenName(int Screen) const override;

	void GetVideoModes(CVideoMode *pModes, int MaxModes, int *pNumModes, float HiDPIScale, int MaxWindowWidth, int MaxWindowHeight, int ScreenId) override;
	void GetCurrentVideoMode(CVideoMode &CurMode, float HiDPIScale, int MaxWindowWidth, int MaxWindowHeight, int ScreenId) override;

	void Minimize() override;
	void SetWindowParams(int FullscreenMode, bool IsBorderless) override;
	bool SetWindowScreen(int Index, bool MoveToCenter, ivec2 *pDesktopSize) override;
	bool UpdateDisplayMode(int Index, ivec2 *pDesktopSize) override;
	int GetWindowScreen() override;
	int WindowActive() override;
	int WindowOpen() override;
	void SetWindowGrab(bool Grab) override;
	bool ResizeWindow(int w, int h, int RefreshRate) override;
	void GetViewportSize(int &w, int &h) override;
	void GetDisplayCutoutInsets(int &Left, int &Right) override;
	void NotifyWindow() override;
	bool IsScreenKeyboardShown() override;

	void WindowDestroyNtf(uint32_t WindowId) override;
	void WindowCreateNtf(uint32_t WindowId) override;

	bool UseTrianglesAsQuad() override { return m_Capabilities.m_TrianglesAsQuads; }
	bool HasTileBuffering() override { return m_Capabilities.m_TileBuffering; }
	bool HasQuadBuffering() override { return m_Capabilities.m_QuadBuffering; }
	bool HasTextBuffering() override { return m_Capabilities.m_TextBuffering; }
	bool HasQuadContainerBuffering() override { return m_Capabilities.m_QuadContainerBuffering; }
	bool Uses2DTextureArrays() override { return m_Capabilities.m_2DArrayTextures; }
	bool HasTextureArraysSupport() override { return m_Capabilities.m_2DArrayTextures || m_Capabilities.m_3DTextures; }

	const char *GetErrorString() override
	{
		if(m_aErrorString[0] != '\0')
			return m_aErrorString;

		return nullptr;
	}

	const char *GetVendorString() override
	{
		return m_aVendorString;
	}

	const char *GetVersionString() override
	{
		return m_aVersionString;
	}

	const char *GetRendererString() override
	{
		return m_aRendererString;
	}

	void GetUiGpuTimes(float &BlurMs, float &RectsMs) const override
	{
		BlurMs = m_UiBlurGpuNs / 1000000.0f;
		RectsMs = m_UiRectsGpuNs / 1000000.0f;
	}

	TGLBackendReadPresentedImageData &GetReadPresentedImageDataFuncUnsafe() override;

	std::optional<int> ShowMessageBox(const IGraphics::CMessageBox &MessageBox) override;
};

#endif // ENGINE_CLIENT_BACKEND_SDL_H
