#include <base/detect.h>

#include <base/log.h>
#include <base/math.h>
#include <base/sphore.h>
#include <base/str.h>
#include <base/thread.h>

#include <engine/shared/config.h>
#include <engine/shared/localization.h>

#if defined(CONF_PLATFORM_IOS)
#include <ios/ios_main.h>
#endif

#include <SDL.h>
#include <SDL_messagebox.h>
#include <SDL_vulkan.h>

#if defined(CONF_VIDEORECORDER)
#include <engine/shared/video.h>
#endif

#include "backend_sdl.h"

#if defined(CONF_HEADLESS_CLIENT)
#include "backend/null/backend_null.h"
#endif

#include "backend/vulkan/backend_vulkan.h"

#include "graphics_threaded.h"

#include <engine/graphics.h>

#include <algorithm>

class IStorage;

// ------------ CCommandProcessor_SDL

void CCommandProcessor_SDL::HandleError()
{
	switch(m_Error.m_ErrorType)
	{
	case GFX_ERROR_TYPE_INIT:
		m_Error.m_vErrors.emplace_back(SGfxErrorContainer::SError{true, Localizable("Failed during initialization. Try to update your GPU drivers.", "Graphics error")});
		break;
	case GFX_ERROR_TYPE_OUT_OF_MEMORY_IMAGE:
		[[fallthrough]];
	case GFX_ERROR_TYPE_OUT_OF_MEMORY_BUFFER:
		[[fallthrough]];
	case GFX_ERROR_TYPE_OUT_OF_MEMORY_STAGING:
		m_Error.m_vErrors.emplace_back(SGfxErrorContainer::SError{true, Localizable("Out of VRAM. Try setting 'cl_skins_loaded_max' to a lower value or remove custom assets (skins, entities, etc.), especially those with high resolution.", "Graphics error")});
		break;
	case GFX_ERROR_TYPE_RENDER_RECORDING:
		m_Error.m_vErrors.emplace_back(SGfxErrorContainer::SError{true, Localizable("An error during command recording occurred. Try to update your GPU drivers.", "Graphics error")});
		break;
	case GFX_ERROR_TYPE_RENDER_CMD_FAILED:
		m_Error.m_vErrors.emplace_back(SGfxErrorContainer::SError{true, Localizable("A render command failed. Try to update your GPU drivers.", "Graphics error")});
		break;
	case GFX_ERROR_TYPE_RENDER_SUBMIT_FAILED:
		m_Error.m_vErrors.emplace_back(SGfxErrorContainer::SError{true, Localizable("Submitting the render commands failed. Try to update your GPU drivers.", "Graphics error")});
		break;
	case GFX_ERROR_TYPE_SWAP_FAILED:
		m_Error.m_vErrors.emplace_back(SGfxErrorContainer::SError{true, Localizable("Failed to swap framebuffers. Try to update your GPU drivers.", "Graphics error")});
		break;
	case GFX_ERROR_TYPE_UNKNOWN:
		[[fallthrough]];
	default:
		m_Error.m_vErrors.emplace_back(SGfxErrorContainer::SError{true, Localizable("Unknown error. Try to update your GPU drivers.", "Graphics error")});
		break;
	}
}

void CCommandProcessor_SDL::HandleWarning()
{
	switch(m_Warning.m_WarningType)
	{
	case GFX_WARNING_TYPE_INIT_FAILED:
		m_Warning.m_vWarnings.emplace_back(Localizable("Could not initialize the given graphics backend, reverting to the default backend now.", "Graphics error"));
		break;
	case GFX_WARNING_TYPE_INIT_FAILED_MISSING_INTEGRATED_GPU_DRIVER:
		m_Warning.m_vWarnings.emplace_back(Localizable("Could not initialize the given graphics backend, this is probably because you didn't install the driver of the integrated graphics card.", "Graphics error"));
		break;
	case GFX_WARNING_MISSING_EXTENSION:
		// ignore this warning for now
		return;
	case GFX_WARNING_LOW_ON_MEMORY:
		// ignore this warning for now
		return;
	case GFX_WARNING_TYPE_INIT_FAILED_NO_DEVICE_WITH_REQUIRED_VERSION:
	{
		// Ignore this warning for now completely.
		// A console message was already printed by the backend
		m_Warning.m_WarningType = GFX_WARNING_TYPE_NONE;
		m_Warning.m_vWarnings.clear();
		return;
	}
	default:
		dbg_assert_failed("Unhandled graphics warning type %d", (int)m_Warning.m_WarningType);
	}
}

void CCommandProcessor_SDL::RunBuffer(CCommandBuffer *pBuffer)
{
	m_pGLBackend->StartCommands(pBuffer->m_CommandCount, pBuffer->m_RenderCallCount);

	for(const CCommandBuffer::SCommand *pCommand = pBuffer->Head(); pCommand; pCommand = pCommand->m_pNext)
	{
		auto Res = m_pGLBackend->RunCommand(pCommand);
		if(Res == ERunCommandReturnTypes::RUN_COMMAND_COMMAND_HANDLED)
		{
			continue;
		}
		else if(Res == ERunCommandReturnTypes::RUN_COMMAND_COMMAND_ERROR)
		{
			m_Error = m_pGLBackend->GetError();
			HandleError();
			return;
		}
		else if(Res == ERunCommandReturnTypes::RUN_COMMAND_COMMAND_WARNING)
		{
			m_Warning = m_pGLBackend->GetWarning();
			HandleWarning();
			return;
		}

		if(m_General.RunCommand(pCommand))
			continue;

		dbg_assert_failed("Unknown graphics command %d", pCommand->m_Cmd);
	}

	m_pGLBackend->EndCommands();
}

CCommandProcessor_SDL::CCommandProcessor_SDL()
{
#if defined(CONF_HEADLESS_CLIENT)
	m_pGLBackend = new CCommandProcessorFragment_Null();
#else
	m_pGLBackend = CreateVulkanCommandProcessorFragment();
#endif
}

CCommandProcessor_SDL::~CCommandProcessor_SDL()
{
	delete m_pGLBackend;
}

const SGfxErrorContainer &CCommandProcessor_SDL::GetError() const
{
	return m_Error;
}

void CCommandProcessor_SDL::ErroneousCleanup()
{
	m_pGLBackend->ErroneousCleanup();
}

const SGfxWarningContainer &CCommandProcessor_SDL::GetWarning() const
{
	return m_Warning;
}

// ------------ CGraphicsBackend_SDL

static Uint32 MessageBoxTypeToSdlFlags(IGraphics::EMessageBoxType Type)
{
	switch(Type)
	{
	case IGraphics::EMessageBoxType::ERROR:
		return SDL_MESSAGEBOX_ERROR;
	case IGraphics::EMessageBoxType::WARNING:
		return SDL_MESSAGEBOX_WARNING;
	case IGraphics::EMessageBoxType::INFO:
		return SDL_MESSAGEBOX_INFORMATION;
	default:
		dbg_assert_failed("Type invalid");
	}
}

static std::optional<int> ShowMessageBoxImpl(const IGraphics::CMessageBox &MessageBox, SDL_Window *pWindow)
{
	dbg_assert(!MessageBox.m_vButtons.empty(), "At least one button is required");

	std::vector<SDL_MessageBoxButtonData> vButtonData;
	vButtonData.reserve(MessageBox.m_vButtons.size());
	for(const auto &Button : MessageBox.m_vButtons)
	{
		SDL_MessageBoxButtonData ButtonData{};
		ButtonData.buttonid = vButtonData.size();
		ButtonData.flags = (Button.m_Confirm ? SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT : 0) | (Button.m_Cancel ? SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT : 0);
		ButtonData.text = Button.m_pLabel;
		vButtonData.emplace_back(ButtonData);
	}
#if defined(CONF_FAMILY_WINDOWS)
	// TODO SDL3: The order of buttons is not defined by default, but the flags returned by MessageBoxTypeToSdlFlags do not work together
	//            with SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT with SDL2 on various platforms. Windows appears to be the only platform that
	//            lays out buttons from right to left by default, so we reverse the order manually.
	std::reverse(vButtonData.begin(), vButtonData.end());
#endif
	SDL_MessageBoxData MessageBoxData{};
	MessageBoxData.title = MessageBox.m_pTitle;
	MessageBoxData.message = MessageBox.m_pMessage;
	MessageBoxData.flags = MessageBoxTypeToSdlFlags(MessageBox.m_Type);
	MessageBoxData.numbuttons = vButtonData.size();
	MessageBoxData.buttons = vButtonData.data();
	MessageBoxData.window = pWindow;
	int ButtonId = -1;
	if(SDL_ShowMessageBox(&MessageBoxData, &ButtonId) != 0)
	{
		return std::nullopt;
	}
	return ButtonId;
}

std::optional<int> ShowMessageBoxWithoutGraphics(const IGraphics::CMessageBox &MessageBox)
{
	return ShowMessageBoxImpl(MessageBox, nullptr);
}

std::optional<int> CGraphicsBackend_SDL::ShowMessageBox(const IGraphics::CMessageBox &MessageBox)
{
	if(m_pProcessor != nullptr)
	{
		m_pProcessor->ErroneousCleanup();
	}
	// TODO: Remove this workaround when https://github.com/libsdl-org/SDL/issues/3750 is
	// fixed and pass the window to SDL_ShowSimpleMessageBox to make the popup modal instead
	// of destroying the window before opening the popup.
	if(m_pWindow != nullptr)
	{
		SDL_DestroyWindow(m_pWindow);
		m_pWindow = nullptr;
	}
	return ShowMessageBoxImpl(MessageBox, m_pWindow);
}

const char *CGraphicsBackend_SDL::GetScreenName(int Screen) const
{
	const char *pName = SDL_GetDisplayName(Screen);
	return pName == nullptr ? "unknown/error" : pName;
}

static void DisplayToVideoMode(CVideoMode *pVMode, SDL_DisplayMode *pMode, float HiDPIScale, int RefreshRate)
{
	pVMode->m_CanvasWidth = pMode->w * HiDPIScale;
	pVMode->m_CanvasHeight = pMode->h * HiDPIScale;
	pVMode->m_WindowWidth = pMode->w;
	pVMode->m_WindowHeight = pMode->h;
	pVMode->m_RefreshRate = RefreshRate;
}

void CGraphicsBackend_SDL::GetVideoModes(CVideoMode *pModes, int MaxModes, int *pNumModes, float HiDPIScale, int MaxWindowWidth, int MaxWindowHeight, int ScreenId)
{
	SDL_DisplayMode DesktopMode;
	int MaxModesAvailable = SDL_GetNumDisplayModes(ScreenId);

	// Only collect fullscreen modes when requested, that makes sure in windowed mode no refresh rates are shown that aren't supported without
	// fullscreen anyway(except fullscreen desktop)
	bool IsFullscreenDesktop = m_pWindow != nullptr && (((SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP) || g_Config.m_GfxFullscreen == 3);
	bool CollectFullscreenModes = m_pWindow == nullptr || ((SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_FULLSCREEN) != 0 && !IsFullscreenDesktop);

	if(SDL_GetDesktopDisplayMode(ScreenId, &DesktopMode) < 0)
	{
		log_error("gfx", "Unable to get desktop display mode of screen %d: %s", ScreenId, SDL_GetError());
	}

	constexpr int ModeCount = 256;
	SDL_DisplayMode aModes[ModeCount];
	int NumModes = 0;
	for(int i = 0; i < MaxModesAvailable && NumModes < ModeCount; i++)
	{
		SDL_DisplayMode Mode;
		if(SDL_GetDisplayMode(ScreenId, i, &Mode) < 0)
		{
			log_error("gfx", "Unable to get display mode %d of screen %d: %s", i, ScreenId, SDL_GetError());
			continue;
		}

		aModes[NumModes] = Mode;
		++NumModes;
	}

	int NumModesInserted = 0;
	auto &&ModeInsert = [&](SDL_DisplayMode &Mode) {
		if(NumModesInserted < MaxModes)
		{
			// if last mode was equal, ignore this one --- in fullscreen this can really only happen if the screen
			// supports different color modes
			// in non fullscreen these are the modes that show different refresh rate, but are basically the same
			if(NumModesInserted > 0 && pModes[NumModesInserted - 1].m_WindowWidth == Mode.w && pModes[NumModesInserted - 1].m_WindowHeight == Mode.h && (pModes[NumModesInserted - 1].m_RefreshRate == Mode.refresh_rate || (Mode.refresh_rate != DesktopMode.refresh_rate && !CollectFullscreenModes)))
				return;

			DisplayToVideoMode(&pModes[NumModesInserted], &Mode, HiDPIScale, !CollectFullscreenModes ? DesktopMode.refresh_rate : Mode.refresh_rate);
			NumModesInserted++;
		}
	};

	for(int i = 0; i < NumModes; i++)
	{
		SDL_DisplayMode &Mode = aModes[i];

		if(Mode.w > MaxWindowWidth || Mode.h > MaxWindowHeight)
			continue;

		ModeInsert(Mode);

		if(IsFullscreenDesktop)
			break;

		if(NumModesInserted >= MaxModes)
			break;
	}
	*pNumModes = NumModesInserted;
}

void CGraphicsBackend_SDL::GetCurrentVideoMode(CVideoMode &CurMode, float HiDPIScale, int MaxWindowWidth, int MaxWindowHeight, int ScreenId)
{
	SDL_DisplayMode DpMode;
	// if "real" fullscreen, obtain the video mode for that
	if((SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN)
	{
		if(SDL_GetCurrentDisplayMode(ScreenId, &DpMode))
		{
			log_error("gfx", "Unable to get current display mode of screen %d: %s", ScreenId, SDL_GetError());
		}
	}
	else
	{
		if(SDL_GetDesktopDisplayMode(ScreenId, &DpMode) < 0)
		{
			log_error("gfx", "Unable to get desktop display mode of screen %d: %s", ScreenId, SDL_GetError());
		}
		else
		{
			int Width = 0;
			int Height = 0;
			SDL_Vulkan_GetDrawableSize(m_pWindow, &Width, &Height);
			// SDL video modes are in screen space which are logical pixels
			DpMode.w = Width / HiDPIScale;
			DpMode.h = Height / HiDPIScale;
		}
	}
	DisplayToVideoMode(&CurMode, &DpMode, HiDPIScale, DpMode.refresh_rate);
}

CGraphicsBackend_SDL::CGraphicsBackend_SDL(TTranslateFunc &&TranslateFunc) :
	CGraphicsBackend_Threaded(std::move(TranslateFunc))
{
	m_aErrorString[0] = '\0';
}

int CGraphicsBackend_SDL::Init(const char *pName, int *pScreen, int *pWidth, int *pHeight, int *pRefreshRate, int *pFsaaSamples, int Flags, int *pDesktopWidth, int *pDesktopHeight, int *pCurrentWidth, int *pCurrentHeight, IStorage *pStorage)
{
#if defined(CONF_HEADLESS_CLIENT)
	int InitError = 0;
	*pScreen = 0;
	*pWidth = *pDesktopWidth = *pCurrentWidth = 800;
	*pHeight = *pDesktopHeight = *pCurrentHeight = 600;
	*pRefreshRate = 60;
	*pFsaaSamples = 0;
	log_info("gfx", "Created headless context");
#else
	// print sdl version
	{
		SDL_version Compiled;
		SDL_version Linked;

		SDL_VERSION(&Compiled);
		SDL_GetVersion(&Linked);
		log_info("sdl", "SDL version %d.%d.%d (compiled = %d.%d.%d)",
			Linked.major, Linked.minor, Linked.patch,
			Compiled.major, Compiled.minor, Compiled.patch);
	}

	if(!SDL_WasInit(SDL_INIT_VIDEO))
	{
		if(SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
		{
			log_error("gfx", "Unable to initialize SDL video: %s", SDL_GetError());
			return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_SDL_INIT_FAILED;
		}
	}

	log_info("gfx", "Created Vulkan context");

	// set screen
	m_NumScreens = SDL_GetNumVideoDisplays();
	if(m_NumScreens > 0)
	{
		SDL_Rect ScreenPos;
		*pScreen = std::clamp(*pScreen, 0, m_NumScreens - 1);
		if(SDL_GetDisplayBounds(*pScreen, &ScreenPos) != 0)
		{
			log_error("gfx", "Unable to get display bounds of screen %d: %s", *pScreen, SDL_GetError());
			return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_SDL_SCREEN_INFO_REQUEST_FAILED;
		}
	}
	else
	{
		log_error("gfx", "Unable to get number of screens: %s", SDL_GetError());
		return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_SDL_SCREEN_REQUEST_FAILED;
	}

	// store desktop resolution for settings reset button
	SDL_DisplayMode DisplayMode;
	if(SDL_GetDesktopDisplayMode(*pScreen, &DisplayMode))
	{
		log_error("gfx", "Unable to get desktop display mode of screen %d: %s", *pScreen, SDL_GetError());
		return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_SDL_SCREEN_RESOLUTION_REQUEST_FAILED;
	}

	bool IsDesktopChanged = *pDesktopWidth == 0 || *pDesktopHeight == 0 || *pDesktopWidth != DisplayMode.w || *pDesktopHeight != DisplayMode.h;

	*pDesktopWidth = DisplayMode.w;
	*pDesktopHeight = DisplayMode.h;

	// fetch supported video modes
	bool SupportedResolution = false;

	CVideoMode aModes[256];
	int ModesCount = 0;
	int IndexOfResolution = -1;
	GetVideoModes(aModes, std::size(aModes), &ModesCount, 1, *pDesktopWidth, *pDesktopHeight, *pScreen);

	for(int i = 0; i < ModesCount; i++)
	{
		if(*pWidth == aModes[i].m_WindowWidth && *pHeight == aModes[i].m_WindowHeight && (*pRefreshRate == aModes[i].m_RefreshRate || *pRefreshRate == 0))
		{
			SupportedResolution = true;
			IndexOfResolution = i;
			break;
		}
	}

	// set flags
	int SdlFlags = SDL_WINDOW_INPUT_GRABBED | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_ALLOW_HIGHDPI;
	SdlFlags |= SDL_WINDOW_VULKAN;
	if(Flags & IGraphicsBackend::INITFLAG_RESIZABLE)
		SdlFlags |= SDL_WINDOW_RESIZABLE;
	if(Flags & IGraphicsBackend::INITFLAG_BORDERLESS)
		SdlFlags |= SDL_WINDOW_BORDERLESS;
	if(Flags & IGraphicsBackend::INITFLAG_FULLSCREEN)
		SdlFlags |= SDL_WINDOW_FULLSCREEN;
	else if(Flags & (IGraphicsBackend::INITFLAG_DESKTOP_FULLSCREEN))
		SdlFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

	bool IsFullscreen = (SdlFlags & SDL_WINDOW_FULLSCREEN) != 0 || g_Config.m_GfxFullscreen == 3;
	// use desktop resolution as default resolution, clamp resolution if users's display is smaller than we remembered
	// if the user starts in fullscreen, and the resolution was not found use the desktop one
	if((IsFullscreen && !SupportedResolution) || *pWidth == 0 || *pHeight == 0 || (IsDesktopChanged && (!SupportedResolution || !IsFullscreen) && (*pWidth > *pDesktopWidth || *pHeight > *pDesktopHeight)))
	{
		*pWidth = *pDesktopWidth;
		*pHeight = *pDesktopHeight;
		*pRefreshRate = DisplayMode.refresh_rate;
	}

	// if in fullscreen and refresh rate wasn't set yet, just use the one from the found list
	if(*pRefreshRate == 0 && SupportedResolution)
	{
		*pRefreshRate = aModes[IndexOfResolution].m_RefreshRate;
	}
	else if(*pRefreshRate == 0)
	{
		*pRefreshRate = DisplayMode.refresh_rate;
	}

	m_pWindow = SDL_CreateWindow(
		pName,
		SDL_WINDOWPOS_CENTERED_DISPLAY(*pScreen),
		SDL_WINDOWPOS_CENTERED_DISPLAY(*pScreen),
		*pWidth,
		*pHeight,
		SdlFlags);

	// set caption
	if(m_pWindow == nullptr)
	{
		log_error("gfx", "Unable to create window: %s", SDL_GetError());
		return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_SDL_WINDOW_CREATE_FAILED;
	}

	int InitError = 0;

	// SDL_Vulkan_GetDrawableSize reports HiDPI resolution even with SDL_WINDOW_ALLOW_HIGHDPI not set, which is wrong
	if(SdlFlags & SDL_WINDOW_ALLOW_HIGHDPI)
	{
		SDL_Vulkan_GetDrawableSize(m_pWindow, pCurrentWidth, pCurrentHeight);
	}
	else
	{
		SDL_GetWindowSize(m_pWindow, pCurrentWidth, pCurrentHeight);
	}
	SDL_GetWindowSize(m_pWindow, pWidth, pHeight);

#endif // !CONF_HEADLESS_CLIENT

	// start the command processor
	dbg_assert(m_pProcessor == nullptr, "Processor was not cleaned up properly.");
	m_pProcessor = new CCommandProcessor_SDL();
	StartProcessor(m_pProcessor);

	// issue init commands
	CCommandBuffer CmdBuffer(1024, 512);
	CCommandProcessorFragment_GLBase::SCommand_PreInit CmdPre;
	CmdPre.m_pWindow = m_pWindow;
	CmdPre.m_Width = *pCurrentWidth;
	CmdPre.m_Height = *pCurrentHeight;
	CmdPre.m_pVendorString = m_aVendorString;
	CmdPre.m_pVersionString = m_aVersionString;
	CmdPre.m_pRendererString = m_aRendererString;
	CmdPre.m_pGpuList = &m_GpuList;
	CmdBuffer.AddCommandUnsafe(CmdPre);
	RunBufferSingleThreadedUnsafe(&CmdBuffer);
	CmdBuffer.Reset();

	const char *pErrorStr = nullptr;
	if(InitError == 0)
	{
		CCommandProcessorFragment_GLBase::SCommand_Init CmdGL;
		CmdGL.m_pWindow = m_pWindow;
		CmdGL.m_Width = *pCurrentWidth;
		CmdGL.m_Height = *pCurrentHeight;
		CmdGL.m_pTextureMemoryUsage = &m_TextureMemoryUsage;
		CmdGL.m_pBufferMemoryUsage = &m_BufferMemoryUsage;
		CmdGL.m_pStreamMemoryUsage = &m_StreamMemoryUsage;
		CmdGL.m_pStagingMemoryUsage = &m_StagingMemoryUsage;
		CmdGL.m_pUiBlurGpuNs = &m_UiBlurGpuNs;
		CmdGL.m_pUiRectsGpuNs = &m_UiRectsGpuNs;
		CmdGL.m_pGpuList = &m_GpuList;
		CmdGL.m_pReadPresentedImageDataFunc = &m_ReadPresentedImageDataFunc;
		CmdGL.m_pStorage = pStorage;
		CmdGL.m_pCapabilities = &m_Capabilities;
		CmdGL.m_pInitError = &InitError;
		CmdGL.m_pErrStringPtr = &pErrorStr;
		CmdGL.m_pVendorString = m_aVendorString;
		CmdGL.m_pVersionString = m_aVersionString;
		CmdGL.m_pRendererString = m_aRendererString;
		CmdBuffer.AddCommandUnsafe(CmdGL);

		RunBuffer(&CmdBuffer);
		WaitForIdle();
		CmdBuffer.Reset();
	}

	if(InitError != 0)
	{
		if(InitError != -2)
		{
			// shutdown the backend, as it might have been initialized
			CCommandProcessorFragment_GLBase::SCommand_Shutdown CmdGL;
			CmdBuffer.AddCommandUnsafe(CmdGL);
			RunBuffer(&CmdBuffer);
			WaitForIdle();
			CmdBuffer.Reset();
		}

		CCommandProcessorFragment_GLBase::SCommand_PostShutdown CmdPost;
		CmdBuffer.AddCommandUnsafe(CmdPost);
		RunBufferSingleThreadedUnsafe(&CmdBuffer);
		CmdBuffer.Reset();

		// stop and delete the processor
		StopProcessor();
		delete m_pProcessor;
		m_pProcessor = nullptr;

		SDL_DestroyWindow(m_pWindow);
		m_pWindow = nullptr;

		if(pErrorStr != nullptr)
		{
			str_copy(m_aErrorString, pErrorStr);
		}

		return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_BACKEND_INIT_FAILED;
	}

	{
		CCommandBuffer::SCommand_Update_Viewport CmdSDL2;
		CmdSDL2.m_X = 0;
		CmdSDL2.m_Y = 0;
		CmdSDL2.m_Width = *pCurrentWidth;
		CmdSDL2.m_Height = *pCurrentHeight;
		CmdSDL2.m_DrawableWidth = *pCurrentWidth;
		CmdSDL2.m_DrawableHeight = *pCurrentHeight;
		CmdSDL2.m_ByResize = true;
		CmdBuffer.AddCommandUnsafe(CmdSDL2);
		RunBuffer(&CmdBuffer);
		WaitForIdle();
		CmdBuffer.Reset();
	}

	return EGraphicsBackendErrorCodes::GRAPHICS_BACKEND_ERROR_CODE_NONE;
}

int CGraphicsBackend_SDL::Shutdown()
{
	if(m_pProcessor != nullptr)
	{
		// issue a shutdown command
		CCommandBuffer CmdBuffer(1024, 512);
		CCommandProcessorFragment_GLBase::SCommand_Shutdown CmdGL;
		CmdBuffer.AddCommandUnsafe(CmdGL);
		RunBuffer(&CmdBuffer);
		WaitForIdle();
		CmdBuffer.Reset();

		CCommandProcessorFragment_GLBase::SCommand_PostShutdown CmdPost;
		CmdBuffer.AddCommandUnsafe(CmdPost);
		RunBufferSingleThreadedUnsafe(&CmdBuffer);
		CmdBuffer.Reset();

		// stop and delete the processor
		StopProcessor();
		delete m_pProcessor;
		m_pProcessor = nullptr;
	}

	SDL_DestroyWindow(m_pWindow);
	m_pWindow = nullptr;

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	return 0;
}

uint64_t CGraphicsBackend_SDL::TextureMemoryUsage() const
{
	return m_TextureMemoryUsage;
}

uint64_t CGraphicsBackend_SDL::BufferMemoryUsage() const
{
	return m_BufferMemoryUsage;
}

uint64_t CGraphicsBackend_SDL::StreamedMemoryUsage() const
{
	return m_StreamMemoryUsage;
}

uint64_t CGraphicsBackend_SDL::StagingMemoryUsage() const
{
	return m_StagingMemoryUsage;
}

const TTwGraphicsGpuList &CGraphicsBackend_SDL::GetGpus() const
{
	return m_GpuList;
}

void CGraphicsBackend_SDL::Minimize()
{
	SDL_MinimizeWindow(m_pWindow);
}

void CGraphicsBackend_SDL::SetWindowParams(int FullscreenMode, bool IsBorderless)
{
	// The flags have to be kept consistent with flags set in the CGraphics_Threaded::IssueInit function!

	if(FullscreenMode > 0)
	{
		bool IsDesktopFullscreen = FullscreenMode == 2;
#ifndef CONF_FAMILY_WINDOWS
		//  Windowed fullscreen is only available on Windows, use desktop fullscreen on other platforms
		IsDesktopFullscreen |= FullscreenMode == 3;
#endif
		if(FullscreenMode == 1)
		{
#if defined(CONF_PLATFORM_MACOS) || defined(CONF_PLATFORM_HAIKU)
			// Todo SDL: remove this when fixed (game freezes when losing focus in fullscreen)
			SDL_SetWindowFullscreen(m_pWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
#else
			SDL_SetWindowFullscreen(m_pWindow, SDL_WINDOW_FULLSCREEN);
#endif
			SDL_SetWindowResizable(m_pWindow, SDL_FALSE);
		}
		else if(IsDesktopFullscreen)
		{
			SDL_SetWindowFullscreen(m_pWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
			SDL_SetWindowResizable(m_pWindow, SDL_FALSE);
		}
		else // Windowed fullscreen
		{
			SDL_SetWindowFullscreen(m_pWindow, 0);
			SDL_SetWindowBordered(m_pWindow, SDL_TRUE);
			SDL_SetWindowResizable(m_pWindow, SDL_FALSE);
			SDL_DisplayMode DpMode;
			if(SDL_GetDesktopDisplayMode(g_Config.m_GfxScreen, &DpMode) < 0)
			{
				log_error("gfx", "Unable to get desktop display mode of screen %d: %s", g_Config.m_GfxScreen, SDL_GetError());
			}
			else
			{
				ResizeWindow(DpMode.w, DpMode.h, DpMode.refresh_rate);
				SDL_SetWindowPosition(m_pWindow, SDL_WINDOWPOS_CENTERED_DISPLAY(g_Config.m_GfxScreen), SDL_WINDOWPOS_CENTERED_DISPLAY(g_Config.m_GfxScreen));
			}
		}
	}
	else // Windowed
	{
		SDL_SetWindowFullscreen(m_pWindow, 0);
		SDL_SetWindowBordered(m_pWindow, SDL_bool(!IsBorderless));
		SDL_SetWindowResizable(m_pWindow, SDL_TRUE);
	}
}

bool CGraphicsBackend_SDL::SetWindowScreen(int Index, bool MoveToCenter, ivec2 *pDesktopSize)
{
	if(Index < 0 || Index >= m_NumScreens)
	{
		log_error("gfx", "Invalid screen number: %d (min: 0, max: %d)", Index, m_NumScreens);
		return false;
	}

	SDL_Rect ScreenPos;
	if(SDL_GetDisplayBounds(Index, &ScreenPos) != 0)
	{
		log_error("gfx", "Unable to get bounds of screen %d: %s", Index, SDL_GetError());
		return false;
	}

	if(MoveToCenter)
	{
		SDL_SetWindowPosition(m_pWindow,
			SDL_WINDOWPOS_CENTERED_DISPLAY(Index),
			SDL_WINDOWPOS_CENTERED_DISPLAY(Index));
	}
	else
	{
		SDL_SetWindowPosition(m_pWindow,
			SDL_WINDOWPOS_UNDEFINED_DISPLAY(Index),
			SDL_WINDOWPOS_UNDEFINED_DISPLAY(Index));
	}

	return UpdateDisplayMode(Index, pDesktopSize);
}

bool CGraphicsBackend_SDL::UpdateDisplayMode(int Index, ivec2 *pDesktopSize)
{
	SDL_DisplayMode DisplayMode;
	if(SDL_GetDesktopDisplayMode(Index, &DisplayMode) < 0)
	{
		log_error("gfx", "Unable to get desktop display mode of screen %d: %s", Index, SDL_GetError());
		return false;
	}

	g_Config.m_GfxScreen = Index;
	pDesktopSize->x = DisplayMode.w;
	pDesktopSize->y = DisplayMode.h;
	return true;
}

int CGraphicsBackend_SDL::GetWindowScreen()
{
	return SDL_GetWindowDisplayIndex(m_pWindow);
}

int CGraphicsBackend_SDL::WindowActive()
{
	return m_pWindow && SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_INPUT_FOCUS;
}

int CGraphicsBackend_SDL::WindowOpen()
{
	return m_pWindow && SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_SHOWN;
}

void CGraphicsBackend_SDL::SetWindowGrab(bool Grab)
{
	// Works around https://github.com/libsdl-org/sdl2-compat/issues/578.
	if(!m_pWindow)
		return;

	SDL_SetWindowGrab(m_pWindow, Grab ? SDL_TRUE : SDL_FALSE);
}

bool CGraphicsBackend_SDL::ResizeWindow(int w, int h, int RefreshRate)
{
	// don't call resize events when the window is at fullscreen desktop
	if(!m_pWindow || (SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP)
		return false;

	// if the window is at fullscreen use SDL_SetWindowDisplayMode instead, suggested by SDL
	if(SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_FULLSCREEN)
	{
#ifdef CONF_FAMILY_WINDOWS
		// in windows make the window windowed mode first, this prevents strange window glitches (other games probably do something similar)
		SetWindowParams(0, true);
#endif
		SDL_DisplayMode SetMode = {};
		SDL_DisplayMode ClosestMode = {};
		SetMode.format = 0;
		SetMode.w = w;
		SetMode.h = h;
		SetMode.refresh_rate = RefreshRate;
		SDL_SetWindowDisplayMode(m_pWindow, SDL_GetClosestDisplayMode(g_Config.m_GfxScreen, &SetMode, &ClosestMode));
#ifdef CONF_FAMILY_WINDOWS
		// now change it back to fullscreen, this will restore the above set state, bcs SDL saves fullscreen modes apart from other video modes (as of SDL 2.0.16)
		// see implementation of SDL_SetWindowDisplayMode
		SetWindowParams(1, false);
#endif
		return true;
	}
	else
	{
		SDL_SetWindowSize(m_pWindow, w, h);
		if(SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_MAXIMIZED)
			// remove maximize flag
			SDL_RestoreWindow(m_pWindow);
	}

	return false;
}

void CGraphicsBackend_SDL::GetViewportSize(int &w, int &h)
{
	SDL_Vulkan_GetDrawableSize(m_pWindow, &w, &h);
}

void CGraphicsBackend_SDL::GetDisplayCutoutInsets(int &Left, int &Right)
{
#if defined(CONF_PLATFORM_IOS)
	IosDisplayCutoutInsets(m_pWindow, &Left, &Right);
#else
	Left = 0;
	Right = 0;
#endif
}

void CGraphicsBackend_SDL::NotifyWindow()
{
	// Minimum version 2.0.16, after version 2.0.22 the naming is changed to 2.24.0 etc.
#if SDL_MAJOR_VERSION > 2 || (SDL_MAJOR_VERSION == 2 && SDL_MINOR_VERSION == 0 && SDL_PATCHLEVEL >= 16) || (SDL_MAJOR_VERSION == 2 && SDL_MINOR_VERSION > 0)
	if(SDL_FlashWindow(m_pWindow, SDL_FlashOperation::SDL_FLASH_UNTIL_FOCUSED) != 0)
	{
		// fails if SDL hasn't implemented it
		return;
	}
#endif
}

bool CGraphicsBackend_SDL::IsScreenKeyboardShown()
{
	return SDL_IsScreenKeyboardShown(m_pWindow);
}

void CGraphicsBackend_SDL::WindowDestroyNtf(uint32_t WindowId)
{
}

void CGraphicsBackend_SDL::WindowCreateNtf(uint32_t WindowId)
{
	m_pWindow = SDL_GetWindowFromID(WindowId);
}

TGLBackendReadPresentedImageData &CGraphicsBackend_SDL::GetReadPresentedImageDataFuncUnsafe()
{
	return m_ReadPresentedImageDataFunc;
}

IGraphicsBackend *CreateGraphicsBackend(TTranslateFunc &&TranslateFunc) { return new CGraphicsBackend_SDL(std::move(TranslateFunc)); }
