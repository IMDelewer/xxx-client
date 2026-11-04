#ifndef GAME_CLIENT_XUI_XUI_H
#define GAME_CLIENT_XUI_XUI_H

#include "screens/home_scene.h"
#include "spring.h"
#include "theme.h"

#include <base/color.h>
#include <base/vmath.h>

#include <engine/graphics.h>
#include <engine/textrender.h>

#include <game/client/component.h>
#include <game/client/lineinput.h>
#include <game/client/render.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Xui
{
	constexpr float INF = 1e30f;
	constexpr float UNSET = -1e30f;

	struct SSize
	{
		enum class EKind
		{
			FIT,
			FIXED,
			GROW,
			PERCENT,
		};
		EKind m_Kind = EKind::FIT;
		float m_Value = 0.0f;
		float m_Min = 0.0f;
		float m_Max = INF;

		constexpr SSize() = default;
		constexpr SSize(float Fixed) :
			m_Kind(EKind::FIXED), m_Value(Fixed) {}
		constexpr SSize(EKind Kind, float Value, float Min, float Max) :
			m_Kind(Kind), m_Value(Value), m_Min(Min), m_Max(Max) {}
	};
	constexpr SSize Px(float Value) { return SSize(Value); }
	constexpr SSize Fit(float Min = 0.0f, float Max = INF) { return SSize(SSize::EKind::FIT, 0.0f, Min, Max); }
	constexpr SSize Grow(float Factor = 1.0f, float Min = 0.0f, float Max = INF) { return SSize(SSize::EKind::GROW, Factor, Min, Max); }
	constexpr SSize Pct(float Percent, float Min = 0.0f, float Max = INF) { return SSize(SSize::EKind::PERCENT, Percent, Min, Max); }

	struct SEdges
	{
		float m_L = 0.0f, m_T = 0.0f, m_R = 0.0f, m_B = 0.0f;
		constexpr SEdges() = default;
		constexpr SEdges(float All) :
			m_L(All), m_T(All), m_R(All), m_B(All) {}
		constexpr SEdges(float Y, float X) :
			m_L(X), m_T(Y), m_R(X), m_B(Y) {}
		constexpr SEdges(float T, float R, float B, float L) :
			m_L(L), m_T(T), m_R(R), m_B(B) {}
		constexpr float X() const { return m_L + m_R; }
		constexpr float Y() const { return m_T + m_B; }
	};

	enum class EDir
	{
		ROW,
		COLUMN,
	};
	enum class EAlign
	{
		AUTO,
		START,
		CENTER,
		END,
		STRETCH,
		BETWEEN,
	};
	enum class EPos
	{
		FLOW,
		ABSOLUTE,
	};
	enum class EScroll
	{
		NONE,
		VERTICAL,
		HORIZONTAL,
	};
	enum class EHit
	{
		NONE,
		INTERACTIVE,
		BLOCK,
	};
	enum class ELayer
	{
		BASE,
		POPOVER,
		MODAL,
		TOAST,
		TOOLTIP,
		NUM,
	};

	struct SBox
	{
		float m_Gap = 0.0f;
		SEdges m_Padding;
		SSize m_W;
		SSize m_H;
		EAlign m_Main = EAlign::START;
		EAlign m_Cross = EAlign::START;
		EAlign m_Self = EAlign::AUTO;
		bool m_Wrap = false;
		bool m_Shrink = false;
		EPos m_Position = EPos::FLOW;
		float m_Left = UNSET;
		float m_Top = UNSET;
		float m_Right = UNSET;
		float m_Bottom = UNSET;
		EScroll m_Scroll = EScroll::NONE;
		bool m_Clip = false;
		bool m_Flip = false;
		EHit m_Hit = EHit::NONE;
		const char *m_pKey = nullptr;
	};

	inline vec4 Rad(float R) { return vec4(R, R, R, R); }

	struct SRect
	{
		float m_X = 0.0f, m_Y = 0.0f, m_W = 0.0f, m_H = 0.0f;
		float Right() const { return m_X + m_W; }
		float Bottom() const { return m_Y + m_H; }
		float CenterX() const { return m_X + m_W * 0.5f; }
		float CenterY() const { return m_Y + m_H * 0.5f; }
		SRect Inset(float Amount) const { return {m_X + Amount, m_Y + Amount, m_W - Amount * 2.0f, m_H - Amount * 2.0f}; }
		SRect Inset(float X, float Y) const { return {m_X + X, m_Y + Y, m_W - X * 2.0f, m_H - Y * 2.0f}; }
		bool Contains(vec2 P) const { return P.x >= m_X && P.x < m_X + m_W && P.y >= m_Y && P.y < m_Y + m_H; }
		bool Overlaps(const SRect &O) const { return m_X < O.m_X + O.m_W && O.m_X < m_X + m_W && m_Y < O.m_Y + O.m_H && O.m_Y < m_Y + m_H; }
	};

	enum class EFont
	{
		REGULAR,
		MEDIUM,
		SEMIBOLD,
		BOLD,
		EXTRABOLD,
		PHOSPHOR,
		PHOSPHOR_FILL,
		DEJAVU,
	};
	enum class ETextAlign
	{
		LEFT,
		CENTER,
		RIGHT,
	};

	struct STextStyle
	{
		float m_Size = XuiTheme::Font::BODY;
		EFont m_Font = EFont::REGULAR;
		ColorRGBA m_Color = XuiTheme::Color::TEXT;
		ETextAlign m_Align = ETextAlign::LEFT;
		bool m_Wrap = false;
		bool m_Tabular = false;
		bool m_PreserveColor = false;
		int m_MaxLines = 0;
		float m_Tracking = XuiTheme::Font::TRACKING;
	};

	enum class EVariant
	{
		GHOST,
		ACCENT,
		GLASS,
		PLAIN,
	};

	struct SButtonStyle
	{
		EVariant m_Variant = EVariant::GHOST;
		const char *m_pIcon = nullptr;
		bool m_Loading = false;
		bool m_Disabled = false;
		float m_Height = XuiTheme::Size::CONTROL_H;
		float m_PadX = XuiTheme::Size::CAP_PAD_X;
		int m_Force = 0; // 1 hover, 2 pressed, 3 focused: for showing a state statically
		float m_FontSize = XuiTheme::Font::CAP;
		EFont m_Font = EFont::SEMIBOLD;
		bool m_IconFill = false;
		SSize m_W;
		const char *m_pKey = nullptr;
	};

	struct SResult
	{
		uint32_t m_Id = 0;
		bool m_Clicked = false;
		bool m_RightClicked = false;
		bool m_Hovered = false;
		bool m_Pressed = false;
		bool m_Focused = false;
		bool m_Changed = false;
		bool m_Submitted = false;
	};

	struct SFieldStyle
	{
		const char *m_pPlaceholder = nullptr;
		const char *m_pIcon = nullptr;
		SSize m_W = Px(220.0f);
		bool m_Right = false;
		bool m_Disabled = false;
		bool m_Clearable = false;
		const char *m_pKey = nullptr;
	};

	struct SSwitchStyle
	{
		bool m_Disabled = false;
		bool m_UseGreen = false;
		const char *m_pKey = nullptr;
	};

	struct SSliderStyle
	{
		float m_Min = 0.0f;
		float m_Max = 100.0f;
		float m_Step = 1.0f;
		float m_Width = 180.0f;
		bool m_Disabled = false;
		const char *m_pFormat = "%d";
		const char *m_pKey = nullptr;
	};

	struct SMenuItem
	{
		const char *m_pLabel = nullptr;
		const char *m_pIcon = nullptr;
		bool m_Danger = false;
		bool m_Disabled = false;
		bool m_Separator = false;
	};

	struct SNavItem
	{
		const char *m_pLabel = nullptr;
		const char *m_pIcon = nullptr;
		const char *m_pCount = nullptr;
		bool m_Header = false;
		ColorRGBA m_Tile = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
	};

	struct SDockItem
	{
		const char *m_pLabel = nullptr;
		const char *m_pIcon = nullptr;
		bool m_Selectable = true;
	};

	struct SXf
	{
		float m_Sx = 1.0f, m_Sy = 1.0f, m_Tx = 0.0f, m_Ty = 0.0f;
		vec2 Apply(vec2 P) const { return vec2(P.x * m_Sx + m_Tx, P.y * m_Sy + m_Ty); }
		float Scale() const { return m_Sx < m_Sy ? m_Sx : m_Sy; }
		SXf Then(const SXf &Inner) const { return {m_Sx * Inner.m_Sx, m_Sy * Inner.m_Sy, m_Sx * Inner.m_Tx + m_Tx, m_Sy * Inner.m_Ty + m_Ty}; }
	};

	using FBuild = std::function<void()>;
	using FPage = std::function<void()>;
}

class CXui;
using FXuiPaint = std::function<void(CXui &, const Xui::SRect &)>;

struct SXuiNode
{
	uint32_t m_Id = 0;
	int m_Parent = -1;
	int m_First = -1;
	int m_Last = -1;
	int m_Next = -1;
	Xui::SBox m_Box;
	Xui::EDir m_Dir = Xui::EDir::ROW;
	bool m_IsText = false;
	std::string m_Text;
	Xui::STextStyle m_TextStyle;
	float m_X = 0.0f, m_Y = 0.0f, m_W = 0.0f, m_H = 0.0f;
	float m_PrefW = -1.0f, m_PrefH = -1.0f;
	float m_ContentW = 0.0f, m_ContentH = 0.0f;
	float m_ScrollX = 0.0f, m_ScrollY = 0.0f;
	float m_Scale = 1.0f, m_Alpha = 1.0f, m_OffX = 0.0f, m_OffY = 0.0f;
	FXuiPaint m_Paint;
	FXuiPaint m_PaintOver;
	Xui::SRect m_Abs;
	Xui::SRect m_Screen;
	Xui::SRect m_ClipPx;
	Xui::SXf m_Xf;
	float m_AlphaAcc = 1.0f;
	bool m_Clipped = false;
	int m_Layer = 0;
};

class CXui : public CComponent
{
public:
	struct SText
	{
		STextContainerIndex m_Index;
		float m_W = 0.0f;
		float m_H = 0.0f;
		int m_Lines = 1;
		uint64_t m_Used = 0;
	};

	struct SAnim
	{
		CSpring m_Spring;
		bool m_Init = false;
	};

	struct SState
	{
		float m_LastSeen = 0.0f;
		Xui::SRect m_Rect;
		Xui::SRect m_ScreenRect;
		CSpring m_Hover;
		CSpring m_Press;
		CSpring m_Focus;
		CSpring m_Scale;
		bool m_Init = false;
		SState() { m_Scale.Snap(1.0f); }
		SAnim m_aAnim[8];
		CSpring m_FlipX, m_FlipY, m_FlipW, m_FlipH;
		bool m_FlipInit = false;
		float m_ScrollX = 0.0f, m_ScrollY = 0.0f;
		float m_VelX = 0.0f, m_VelY = 0.0f;
		float m_aScratch[4] = {0, 0, 0, 0};
		int m_LastClickRow = -1;
		float m_LastClickTime = -1.0f;
		int m_Int = 0;
		bool m_Flag = false;
		std::unique_ptr<CLineInput> m_pInput;
		char *m_pBound = nullptr;
	};

	int Sizeof() const override { return sizeof(*this); }
	class CGameClient *Game() const { return GameClient(); }
	void OnInit() override;
	void OnReset() override;
	void OnShutdown() override;
	void OnRender() override;
	void OnWindowResize() override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	bool OnInput(const IInput::CEvent &Event) override;

	bool Active() const;
	bool GalleryOpen() const { return m_Active; }
	void AfterFrame(std::function<void()> &&Fn) { m_vAfterFrame.push_back(std::move(Fn)); }
	bool m_HudEditor = false;
	void RenderHudModules(float Alpha, bool Edit);
	void RenderHudEditor();
#if defined(DDEV_XX)
	void DebugScreen(const char *pName);
#endif
	bool HudModulePos(const char *pId, vec2 *pOut);
	bool IslandExtras(char *pBuf, int Size);
	void RenderShell(class CMenus &Menus);
	void RenderBackground(vec2 Size);
	void RenderGrade();
	void SetMasterTransform(float Scale, float Alpha)
	{
		m_MasterScale = Scale;
		m_MasterAlpha = Alpha;
	}
	static bool HandlesPopup(int Popup);
	void RenderShellPopup(class CMenus &Menus);
	void RenderBrowser(class CMenus &Menus);
	void RenderDemos(class CMenus &Menus);
	void RenderDemoPlayback(class CMenus &Menus);
	void RenderIngame(class CMenus &Menus);
	void RenderProfile(class CMenus &Menus);
	void OpenPlayerProfile(const char *pName);
	bool ProfileOpen() const { return m_ProfileOpen; }
	bool ContextOpen() const { return m_PlayerContextOpen || m_ChatContextOpen; }
	void OpenPlayerContext(int Id);
	void RenderPlayerContext();
	void RenderViewedProfile();
	int RenderSpectator();
	void RenderVoteTab(class CMenus &Menus);
	void RenderGameOverlay(bool Scoreboard);
	void SetPage(Xui::FPage Page);
	void ClearPage();
	bool HasPage() const { return (bool)m_Page; }

	// frame
	float Scale() const { return m_Scale; }
	bool HomeVisible() const { return m_ShellHome && !m_ShellProfile; }
	bool LightTheme() const { return m_LightTheme; }
	IGraphics::CTextureHandle ClientIcon() const { return m_ClientIcon; }
	static IGraphics::SUiRect HomePanel(float Radius);
	float Dt() const { return m_Dt; }
	bool MouseDown() const { return m_MouseDown; }
	float Time() const { return m_Time; }
	vec2 ScreenSize() const { return m_ScreenUnits; }
	vec2 Mouse() const { return m_Mouse / m_Scale; }
	vec2 MousePx() const { return m_Mouse; }
	ColorRGBA Accent() const { return m_Accent; }
	ColorRGBA OnAccent() const { return m_OnAccent; }
	bool EscPressed() const { return m_EscPressed; }
	bool KeyPressed(int Key) const;

	// builder
	int Box(Xui::EDir Dir, const Xui::SBox &B, const Xui::FBuild &Children = {});
	int BoxId(uint32_t Id, Xui::EDir Dir, const Xui::SBox &B, const Xui::FBuild &Children = {});
	int Row(const Xui::SBox &B, const Xui::FBuild &Children = {}) { return Box(Xui::EDir::ROW, B, Children); }
	int Column(const Xui::SBox &B, const Xui::FBuild &Children = {}) { return Box(Xui::EDir::COLUMN, B, Children); }
	int Spacer(float Factor = 1.0f);
	int Gap(float Size);
	int Text(const char *pText, const Xui::STextStyle &Style = {}, const Xui::SBox &B = {});
	int Icon(const char *pIcon, float Size = XuiTheme::Size::ICON, ColorRGBA Color = XuiTheme::Color::TEXT, bool Fill = false);
	void Layer(Xui::ELayer Layer, const Xui::FBuild &Children);
	void Paint(int Node, FXuiPaint Paint);
	void PaintOver(int Node, FXuiPaint Paint);
	void SetTransform(int Node, float Scale, float Alpha, float OffX = 0.0f, float OffY = 0.0f);
	SXuiNode &Node(int Index) { return m_vNodes[Index]; }
	uint32_t CurrentId() const;
	uint32_t MakeId(const char *pKey, bool Unique = true);
	uint32_t IdOf(const char *pKey) const;
	void PushId(uint32_t Id);
	void PopId();

	// state
	SState &State(uint32_t Id);
	float Animate(uint32_t Id, float Target, XuiTheme::Motion::SSpring Spec, int Slot = 0, float Init = -1e30f);
	Xui::SRect AnchorRect(uint32_t Id);
	Xui::SResult Interact(uint32_t Id, bool Enabled = true, bool Focusable = true);
	void Focus(uint32_t Id);
	uint32_t FocusId() const { return m_FocusId; }
	bool FocusVisible() const { return m_FocusVisible; }
	uint32_t HoverId() const { return m_HoverId; }
	uint32_t ActiveId() const { return m_ActiveId; }
	void RegisterOverlay(uint32_t Id, bool *pOpen);
	float Presence(uint32_t Id, bool Want, XuiTheme::Motion::SSpring Spec = XuiTheme::Motion::POP);

	// drawing, valid inside paint callbacks
	void DrawRect(const Xui::SRect &R, IGraphics::SUiRect Style);
	void DrawText(const char *pText, const Xui::STextStyle &Style, const Xui::SRect &R);
	void DrawImage(const Xui::SRect &R, IGraphics::CTextureHandle Texture, vec2 ImageSize, float Radius);
	void DrawTee(const CTeeRenderInfo &Info, int Emote, vec2 Dir, const Xui::SRect &R);
	void DrawDirect(const Xui::SRect &Bounds, std::function<void(const Xui::SRect &)> Draw);
	float TextWidth(const char *pText, const Xui::STextStyle &Style);
	float LineHeight(const Xui::STextStyle &Style) const { return Style.m_Size * XuiTheme::Font::LINE_HEIGHT; }
	const Xui::SXf &CurrentXf() const { return m_CurXf; }
	float CurrentAlpha() const { return m_CurAlpha; }
	vec2 ToScreen(float X, float Y) const { return m_CurXf.Apply(vec2(X, Y)); }

	// recipes (widgets/panels.cpp)
	static IGraphics::SUiRect GlassRect(float Radius, float PanelAlpha = -1.0f);
	static IGraphics::SUiRect GlassHairline(float Radius);
	static IGraphics::SUiRect SolidRect(ColorRGBA Color, float Radius);
	static IGraphics::SUiRect AccentRect(ColorRGBA Accent, float Radius);
	static IGraphics::SUiRect GroupRect(float Radius);
	static ColorRGBA Mix(ColorRGBA A, ColorRGBA B, float T);
	static ColorRGBA WithAlpha(ColorRGBA C, float Alpha);

	// containers
	int GlassPanel(const Xui::SBox &B, const Xui::FBuild &Children, float Radius = XuiTheme::Radius::XL, bool Dark = false);
	int Panel(const Xui::SBox &B, const Xui::FBuild &Children, float Radius = XuiTheme::Radius::XL);
	int Group(const Xui::FBuild &Rows);
	int GroupRow(const Xui::FBuild &Content, bool First, float MinHeight = XuiTheme::Size::ROW_MIN_H);
	void SettingsRow(const char *pTitle, const char *pSubtitle, const Xui::FBuild &Control, const char *pIcon = nullptr);
	int PageTransition(const char *pKey, const Xui::SBox &B, const Xui::FBuild &Children);
	int Scroll(const Xui::SBox &B, const Xui::FBuild &Children);
	void SectionHeader(const char *pText);
	void PageHeader(const char *pTitle, const char *pSubtitle);

	// controls
	Xui::SResult Button(const char *pLabel, const Xui::SButtonStyle &Style = {});
	Xui::SResult IconButton(const char *pIcon, const Xui::SButtonStyle &Style = {});
	Xui::SResult StarButton(const char *pIcon, bool Filled = false, const char *pKey = nullptr);
	Xui::SResult Switch(const char *pLabel, int *pValue, const Xui::SSwitchStyle &Style = {});
	Xui::SResult Field(const char *pKey, char *pBuffer, int BufferSize, const Xui::SFieldStyle &Style = {});
	Xui::SResult SearchField(const char *pKey, char *pBuffer, int BufferSize, const char *pPlaceholder = "Search", Xui::SSize W = Xui::Px(220.0f));
	Xui::SResult Segmented(const char *pKey, const char *const *ppLabels, int Count, int *pValue, bool Disabled = false);
	Xui::SResult Tabs(const char *pKey, const char *const *ppLabels, int Count, int *pValue);
	Xui::SResult Slider(const char *pKey, int *pValue, const Xui::SSliderStyle &Style = {});
	Xui::SResult Kbd(const char *pKey, int *pKey1, bool Disabled = false);
	Xui::SResult KbdLabel(const char *pText, float Height = XuiTheme::Size::KBD_H);
	Xui::SResult Swatch(const char *pKey, ColorRGBA Color, bool Selected);
	Xui::SResult SwatchRow(const char *pKey, const ColorRGBA *pColors, int Count, int *pSelected);
	Xui::SResult ColorPickerButton(const char *pKey, unsigned *pHsl);
	Xui::SResult NavItem(const Xui::SNavItem &Item, bool Active);
	void Sidebar(const char *pKey, const Xui::SNavItem *pItems, int Count, int *pSelected, const char *pHeader = nullptr);
	int Dock(const char *pKey, const Xui::SDockItem *pItems, int Count, int *pSelected);

	// overlays
	void ContextMenu(const char *pKey, const char *pTitle, const Xui::SMenuItem *pItems, int Count, bool *pOpen, vec2 At, int *pChosen);
	void Popover(const char *pKey, bool *pOpen, uint32_t AnchorId, const Xui::FBuild &Content);
	void Modal(const char *pKey, bool *pOpen, const Xui::FBuild &Content);
	void ShowToast(const char *pTitle, const char *pIcon = nullptr, ColorRGBA IconColor = XuiTheme::Color::GREEN, float Seconds = 2.6f);
	void Tooltip(uint32_t AnchorId, const char *pText);
	void RenderToasts();

	// display
	void Badge(const char *pText, ColorRGBA Color = XuiTheme::Color::ACCENT_DEFAULT);
	Xui::SResult Chip(const char *pLabel, bool *pSelected, const char *pIcon = nullptr);
	void ProgressBar(const char *pKey, float Value, float Width = 200.0f);
	void Spinner(float Size = XuiTheme::Size::SPINNER, ColorRGBA Color = XuiTheme::Color::TEXT);
	void RingProgress(float Value, float Size = 44.0f, const char *pLabel = nullptr);
	void Skeleton(float W, float H, float Radius = XuiTheme::Radius::XS);
	void TeeAvatar(const char *pSkin, float Size, int Emote = 0);
	void VirtualList(const char *pKey, int Count, float RowHeight, const Xui::SBox &B, const std::function<void(int)> &Row, int *pClicked = nullptr, const std::function<bool(int)> &Selected = {}, int *pActivated = nullptr, int *pRightClicked = nullptr, float RowRadius = 0.0f);

	// gallery
	void RenderGallery();

private:
	struct SPendingRect
	{
		IGraphics::SUiRect m_Rect;
		Xui::SRect m_Bounds;
	};
	struct SPendingDraw
	{
		enum class EKind
		{
			TEXT,
			TEE,
			DIRECT,
		};
		EKind m_Kind;
		Xui::SRect m_Bounds;
		STextContainerIndex m_Index;
		ColorRGBA m_Color;
		vec2 m_Pos;
		std::function<void(const Xui::SRect &)> m_Direct;
		CTeeRenderInfo m_Tee;
		int m_Emote = 0;
		vec2 m_Dir;
	};
	struct SOverlay
	{
		uint32_t m_Id;
		bool *m_pOpen;
	};
	struct SToast
	{
		std::string m_Title;
		std::string m_Icon;
		ColorRGBA m_IconColor;
		float m_Age = 0.0f;
		float m_Life = 2.6f;
		uint32_t m_Id = 0;
	};

	Xui::FPage m_Page;
	bool m_Active = false;
	bool m_Initialized = false;
	bool m_LightTheme = false;
	float m_ThemeLightness = -1.0f;
	CHomeScene m_HomeScene;
	IGraphics::CTextureHandle m_ClientIcon;
	uint64_t m_Frame = 0;
	float m_Dt = 0.016f;
	float m_Time = 0.0f;
	float m_Scale = 1.0f;
	vec2 m_ScreenPx = vec2(1440, 900);
	vec2 m_ScreenUnits = vec2(1440, 900);
	ColorRGBA m_Accent = XuiTheme::Color::ACCENT_DEFAULT;
	ColorRGBA m_OnAccent = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);

	std::vector<SXuiNode> m_vNodes;
	std::vector<int> m_vStack;
	std::vector<uint32_t> m_vIdStack;
	std::vector<int> m_avLayerRoots[(int)Xui::ELayer::NUM];
	int m_CurLayer = 0;
	std::unordered_set<uint32_t> m_UsedIds;
	std::unordered_map<uint32_t, SState> m_States;
	std::unordered_map<uint64_t, SText> m_TextCache;

	// input
	struct SInputEvent
	{
		enum class EType
		{
			MOUSE_DOWN,
			MOUSE_UP,
			WHEEL,
			KEY,
			BIND_KEY,
		};
		EType m_Type;
		int m_Key;
	};
	std::vector<SInputEvent> m_vEvents;
	std::vector<int> m_vKeysThisFrame;
	vec2 m_Mouse = vec2(0, 0);
	bool m_MouseDown = false;
	bool m_PrevMouseDown = false;
	uint32_t m_ScrollbarHover = 0;
	uint32_t m_ScrollDrag = 0;
	float m_ScrollDragOffset = 0.0f;
	uint32_t m_HoverId = 0;
	uint32_t m_ActiveId = 0;
	uint32_t m_FocusId = 0;
	uint32_t m_KeyCaptureId = 0;
	int m_KeyCaptureRelease = 0;
	uint32_t m_ClickedId = 0;
	uint32_t m_RightClickedId = 0;
	bool m_FocusVisible = false;
	bool m_EscPressed = false;
	float m_Wheel = 0.0f;
	std::vector<uint32_t> m_avFocusNow[(int)Xui::ELayer::NUM];
	std::vector<uint32_t> m_avFocusPrev[(int)Xui::ELayer::NUM];
	std::vector<SOverlay> m_vOverlaysNow;
	std::vector<SOverlay> m_vOverlaysPrev;
	std::vector<SToast> m_vToasts;
	uint32_t m_TextFocusId = 0;
	uint32_t m_ScrollTarget = 0;
	int m_RowCounter = 0;
	int m_LegacyNode = -1;
	bool m_ShellHome = true;
	bool m_ShellProfile = false;
	bool m_ProfileOpen = false;
	bool m_ProfileFromScoreboard = false;
	bool m_ChatContextOpen = false;
	vec2 m_ChatContextAt = vec2(0, 0);
	std::string m_ChatContextText;
	std::shared_ptr<CManagedTeeRenderInfo> m_ChatContextTee;
	std::string m_ChatContextName;
	int m_ChatContextClientId = -1;
	std::string m_ProfileName;
	std::shared_ptr<CManagedTeeRenderInfo> m_ProfileTee;
	bool m_PlayerContextOpen = false;
	int m_PlayerContextId = -1;
	std::string m_PlayerContextName;
	std::string m_PlayerContextClan;
	vec2 m_PlayerContextAt = vec2(0, 0);
	int m_PopRefresh = -1000;
	int m_HitNode = -1;
	float m_ScoreboardShow = 0.0f;
	float m_MasterScale = 1.0f;
	float m_MasterAlpha = 1.0f;
	std::vector<int> m_vOrder;
	bool m_GalleryPage = false;

	// render
	Xui::SXf m_CurXf;
	float m_CurAlpha = 1.0f;
	std::vector<SPendingRect> m_vPendRects;
	std::vector<IGraphics::SUiRect> m_vRectBatch;
	std::vector<SPendingDraw> m_vPendDraws;
	std::vector<std::function<void()>> m_vAfterFrame;
	bool m_ClipOn = false;
	Xui::SRect m_ClipCur;
	Xui::SRect m_ClipWant;
	bool m_ClipWantOn = false;

	void RunPage(const Xui::FPage &Page);
	void RenderConnection(class CMenus &Menus);
	void RenderScoreHud(float Alpha);
	void HudPill(const char *pIcon, const char *pText, ColorRGBA IconColor, float Width = 0.0f);
	void RenderPlayerStateModule(bool Preview);
	void RenderChatModule(bool Preview, bool Edit);
	void RenderSplitsModule(bool Preview);
	void RenderVoteModule(bool Preview);
	bool LeftBroadcast();
	void RenderLeftBroadcast();
	void RenderMotdCard();
	void RenderBroadcastModule(bool Preview);
	void RenderScoreboardPanel(float Presence);
	void RenderHudInfo(bool Scoreboard, bool Problems, bool Paused);
	void BeginFrame();
	void EndFrame();
	void ProcessEvents();
	void ResetFrameState();
	int AddNode(uint32_t Id, Xui::EDir Dir, const Xui::SBox &B);
	void GarbageCollect();
	void TrimTextCache();

	// layout.cpp
	struct SPrefSize
	{
		float m_W, m_H;
	};
	SPrefSize Pref(int N);
	void Layout(int N, float Fw, float Fh, float MaxW, float MaxH);
	void LayoutText(SXuiNode &Node, float Fw, float Fh, float MaxW);
	void LayoutAll();
	void Place(int N, const Xui::SXf &Parent, float ParentX, float ParentY, float ParentAlpha, bool ParentClipped, const Xui::SRect &ParentClip);
	void HitTest();
	void PaintAll();
	void PaintNode(int N);

	// render helpers
	SText *GetText(const char *pText, float SizePx, Xui::EFont Font, float WidthPx, bool Wrap, bool Tabular, int MaxLines, float Tracking);
	void EmitRect(const IGraphics::SUiRect &R, const Xui::SRect &Bounds);
	void SetClip(bool On, const Xui::SRect &Clip);
	void Flush();
	void SyncClip();
	void OverlayPanel(const char *pKey, bool *pOpen, const Xui::SRect &Anchor, float Gap, Xui::SSize W, float Pad, const Xui::FBuild &Content);
	Xui::SResult NavItemImpl(const Xui::SNavItem &Item, bool Active, bool OwnBackground);
	void DrawScrollbar(const SXuiNode &Node);
};

#endif
