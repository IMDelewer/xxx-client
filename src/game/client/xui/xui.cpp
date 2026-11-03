#include "xui.h"

#include <base/log.h>

#include <engine/client.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/components/xxx/glass.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>

#include <algorithm>
#include <cmath>

using namespace Xui;
using namespace XuiTheme;

namespace
{
	uint32_t HashBytes(uint32_t Seed, const void *pData, size_t Size)
	{
		uint32_t Hash = Seed ? Seed : 2166136261u;
		const unsigned char *pBytes = (const unsigned char *)pData;
		for(size_t i = 0; i < Size; i++)
		{
			Hash ^= pBytes[i];
			Hash *= 16777619u;
		}
		return Hash ? Hash : 1u;
	}

	uint32_t HashString(uint32_t Seed, const char *pStr)
	{
		return HashBytes(Seed, pStr, strlen(pStr));
	}

	uint64_t HashText(const char *pText, int SizeQ, int Font, int WidthQ, int Flags, int TrackQ)
	{
		uint64_t Hash = 1469598103934665603ull;
		auto Mix = [&Hash](const void *pData, size_t Size) {
			const unsigned char *pBytes = (const unsigned char *)pData;
			for(size_t i = 0; i < Size; i++)
			{
				Hash ^= pBytes[i];
				Hash *= 1099511628211ull;
			}
		};
		Mix(pText, strlen(pText));
		Mix(&SizeQ, sizeof(SizeQ));
		Mix(&Font, sizeof(Font));
		Mix(&WidthQ, sizeof(WidthQ));
		Mix(&Flags, sizeof(Flags));
		Mix(&TrackQ, sizeof(TrackQ));
		return Hash;
	}

	EFontPreset PresetOf(EFont Font)
	{
		switch(Font)
		{
		case EFont::REGULAR: return EFontPreset::UI_REGULAR;
		case EFont::MEDIUM: return EFontPreset::UI_MEDIUM;
		case EFont::SEMIBOLD: return EFontPreset::UI_SEMIBOLD;
		case EFont::BOLD: return EFontPreset::UI_BOLD;
		case EFont::EXTRABOLD: return EFontPreset::UI_EXTRABOLD;
		case EFont::PHOSPHOR: return EFontPreset::PHOSPHOR;
		case EFont::PHOSPHOR_FILL: return EFontPreset::PHOSPHOR_FILL;
		case EFont::DEJAVU: break;
		}
		return EFontPreset::DEFAULT_FONT;
	}

	bool UsesTracking(EFont Font)
	{
		return Font != EFont::PHOSPHOR && Font != EFont::PHOSPHOR_FILL && Font != EFont::DEJAVU;
	}
}

void CXui::OnInit()
{
	m_ClientIcon = Graphics()->LoadTexture("xxx/icon.png", IStorage::TYPE_ALL);
	m_Initialized = true;
}

void CXui::OnReset()
{
	m_ProfileOpen = m_PlayerContextOpen = m_ChatContextOpen = false;
	m_ProfileTee.reset();
	m_ChatContextTee.reset();
	m_PlayerContextId = m_ChatContextClientId = -1;
}

void CXui::OnShutdown()
{
	m_HomeScene.Unload(Graphics());
	if(m_ClientIcon.IsValid())
		Graphics()->UnloadTexture(&m_ClientIcon);
	for(auto &Entry : m_TextCache)
		if(Entry.second.m_Index.Valid())
			TextRender()->DeleteTextContainer(Entry.second.m_Index);
	m_TextCache.clear();
}

void CXui::OnWindowResize()
{
	for(auto &Entry : m_TextCache)
		if(Entry.second.m_Index.Valid())
			TextRender()->DeleteTextContainer(Entry.second.m_Index);
	m_TextCache.clear();
}

void CXui::SetPage(FPage Page)
{
	m_Page = std::move(Page);
	m_Active = true;
}

void CXui::ClearPage()
{
	m_Page = nullptr;
	m_Active = false;
	m_ActiveId = 0;
	m_FocusId = 0;
	m_HoverId = 0;
	m_MouseDown = false;
	m_vEvents.clear();
	if(CLineInput::GetActiveInput() != nullptr && m_TextFocusId != 0)
		CLineInput::GetActiveInput()->Deactivate();
	m_TextFocusId = 0;
}

bool CXui::KeyPressed(int Key) const
{
	return std::find(m_vKeysThisFrame.begin(), m_vKeysThisFrame.end(), Key) != m_vKeysThisFrame.end();
}

bool CXui::Active() const
{
	const int State = Client()->State();
	return m_Active || State == IClient::STATE_OFFLINE || State == IClient::STATE_CONNECTING || State == IClient::STATE_LOADING || ((State == IClient::STATE_ONLINE || State == IClient::STATE_DEMOPLAYBACK) && (GameClient()->m_Menus.IsActive() || GameClient()->m_Scoreboard.CursorActive() || GameClient()->m_Spectator.IsActive() || GameClient()->m_Chat.IsActive() || m_ProfileOpen));
}

bool CXui::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!Active())
		return false;
	Ui()->ConvertMouseMove(&x, &y, CursorType);
	Ui()->OnCursorMove(x, y);
	return true;
}

bool CXui::OnInput(const IInput::CEvent &Event)
{
	if(!Active())
		return false;

	if((Event.m_Flags & IInput::FLAG_RELEASE) && Event.m_Key == m_KeyCaptureRelease)
	{
		m_KeyCaptureRelease = KEY_UNKNOWN;
		return true;
	}
	if(m_KeyCaptureId != 0)
	{
		if((Event.m_Flags & IInput::FLAG_PRESS) && Event.m_Key > KEY_UNKNOWN)
		{
			m_vEvents.push_back({SInputEvent::EType::BIND_KEY, Event.m_Key});
			m_KeyCaptureRelease = Event.m_Key;
		}
		return true;
	}
	CLineInput *pActiveInput = CLineInput::GetActiveInput();
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK && !pActiveInput && Event.m_Key > KEY_UNKNOWN && Event.m_Key < KEY_LAST && str_comp(GameClient()->m_Binds.Get(Event.m_Key, CBinds::GetModifierMask(Input())), "+scoreboard") == 0)
	{
		Console()->ExecuteLineStroked((Event.m_Flags & IInput::FLAG_PRESS) != 0, "+scoreboard", IConsole::CLIENT_ID_UNSPECIFIED);
		return true;
	}
	if(pActiveInput && m_TextFocusId != 0 && pActiveInput->ProcessInput(Event))
		return true;
	if(!m_Active && !(Event.m_Key == KEY_ESCAPE && (m_ProfileOpen || ContextOpen() || GameClient()->m_Spectator.IsActive())))
		Ui()->OnInput(Event);

	if(!(Event.m_Flags & (IInput::FLAG_PRESS | IInput::FLAG_RELEASE)))
		return true;

	const bool Press = (Event.m_Flags & IInput::FLAG_PRESS) != 0;
	if(Event.m_Key >= KEY_MOUSE_1 && Event.m_Key <= KEY_MOUSE_3)
		m_vEvents.push_back({Press ? SInputEvent::EType::MOUSE_DOWN : SInputEvent::EType::MOUSE_UP, Event.m_Key});
	else if(Event.m_Key == KEY_MOUSE_WHEEL_UP || Event.m_Key == KEY_MOUSE_WHEEL_DOWN)
	{
		if(Press)
			m_vEvents.push_back({SInputEvent::EType::WHEEL, Event.m_Key == KEY_MOUSE_WHEEL_UP ? 1 : -1});
	}
	else if(Press)
		m_vEvents.push_back({SInputEvent::EType::KEY, Event.m_Key});
	return true;
}

void CXui::ProcessEvents()
{
	m_ClickedId = 0;
	m_RightClickedId = 0;
	m_EscPressed = false;
	m_Wheel = 0.0f;
	m_vKeysThisFrame.clear();

	for(const SInputEvent &Event : m_vEvents)
	{
		switch(Event.m_Type)
		{
		case SInputEvent::EType::MOUSE_DOWN:
			if(Event.m_Key == KEY_MOUSE_1)
			{
				m_MouseDown = true;
				m_ActiveId = m_HoverId;
				m_FocusVisible = false;
				if(m_HoverId != 0)
				{
					bool Focusable = false;
					for(const auto &vFocus : m_avFocusPrev)
						Focusable = Focusable || std::find(vFocus.begin(), vFocus.end(), m_HoverId) != vFocus.end();
					m_FocusId = Focusable ? m_HoverId : 0;
				}
				else
					m_FocusId = 0;
			}
			break;
		case SInputEvent::EType::MOUSE_UP:
			if(Event.m_Key == KEY_MOUSE_1)
			{
				if(m_ActiveId != 0 && m_ActiveId == m_HoverId)
					m_ClickedId = m_ActiveId;
				m_ActiveId = 0;
				m_MouseDown = false;
			}
			else if(Event.m_Key == KEY_MOUSE_2)
				m_RightClickedId = m_HoverId;
			break;
		case SInputEvent::EType::WHEEL:
			m_Wheel += Event.m_Key;
			break;
		case SInputEvent::EType::BIND_KEY:
			m_vKeysThisFrame.push_back(Event.m_Key);
			break;
		case SInputEvent::EType::KEY:
		{
			m_vKeysThisFrame.push_back(Event.m_Key);
			if(Event.m_Key == KEY_ESCAPE)
			{
				bool Closed = false;
				for(int i = (int)m_vOverlaysPrev.size() - 1; i >= 0 && !Closed; i--)
				{
					if(m_vOverlaysPrev[i].m_pOpen != nullptr && *m_vOverlaysPrev[i].m_pOpen)
					{
						*m_vOverlaysPrev[i].m_pOpen = false;
						Closed = true;
					}
				}
				if(!Closed && m_TextFocusId != 0 && m_FocusId == m_TextFocusId)
				{
					m_FocusId = 0;
					Closed = true;
				}
				if(!Closed)
					m_EscPressed = true;
			}
			else if(Event.m_Key == KEY_TAB)
			{
				int Trap = (int)ELayer::BASE;
				for(int L = (int)ELayer::POPOVER; L <= (int)ELayer::MODAL; L++)
					if(!m_avFocusPrev[L].empty())
						Trap = L;
				const std::vector<uint32_t> &vList = m_avFocusPrev[Trap];
				if(!vList.empty())
				{
					const bool Back = Input()->ShiftIsPressed();
					auto It = std::find(vList.begin(), vList.end(), m_FocusId);
					int Index = It == vList.end() ? (Back ? 0 : -1) : (int)(It - vList.begin());
					Index = (Index + (Back ? -1 : 1) + (int)vList.size()) % (int)vList.size();
					m_FocusId = vList[Index];
					m_FocusVisible = true;
				}
			}
			else if((Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER || Event.m_Key == KEY_SPACE) && m_FocusId != 0 && m_FocusId != m_TextFocusId)
			{
				m_ClickedId = m_FocusId;
				m_FocusVisible = true;
			}
			break;
		}
		}
	}
	m_vEvents.clear();
}

void CXui::OnRender()
{
#if defined(DDEV_XX)
	const bool Wanted = g_Config.m_XxUiGallery != 0;
	if(Wanted && !m_Page)
		SetPage([this]() { RenderGallery(); });
	else if(!Wanted && m_Page && m_GalleryPage)
		ClearPage();
	m_GalleryPage = Wanted;
#endif
	if(!m_Page)
		return;

	RunPage(m_Page);
	RenderTools()->RenderCursor(m_Mouse, XuiTheme::Size::GALLERY_CURSOR * m_Scale);
	Ui()->MapScreen();
}

void CXui::RunPage(const FPage &Page)
{
	BeginFrame();
	Page();
	EndFrame();
}

void CXui::BeginFrame()
{
	m_Frame++;
	m_Dt = std::clamp(Client()->RenderFrameTime(), 0.0001f, 0.1f);
	m_Time += m_Dt;

	m_ScreenPx = vec2(Graphics()->WindowWidth(), Graphics()->WindowHeight());
	m_Scale = std::max(0.2f, m_ScreenPx.y / 900.0f * g_Config.m_XxUiScale / 100.0f);
	m_ScreenUnits = m_ScreenPx / m_Scale;
	Graphics()->MapScreenToSize(m_ScreenPx.x, m_ScreenPx.y);
	Graphics()->ClipDisable();
	m_ClipOn = false;
	m_ClipWantOn = false;
	m_LightTheme = g_Config.m_XxUiTheme != 0;
	const float ThemeTarget = m_LightTheme ? 1.0f : 0.0f;
	if(m_ThemeLightness < 0.0f)
		m_ThemeLightness = ThemeTarget;
	m_ThemeLightness += (ThemeTarget - m_ThemeLightness) * (1.0f - std::exp(-12.0f * m_Dt));
	XuiTheme::UpdateTheme(m_ThemeLightness);
	m_Accent = ::Glass::Accent();
	m_OnAccent = 0.2126f * m_Accent.r + 0.7152f * m_Accent.g + 0.0722f * m_Accent.b > 0.6f ? ColorRGBA(0.07f, 0.07f, 0.09f, 1.0f) : ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);

	m_Mouse = Ui()->UpdatedMousePos();

	ProcessEvents();

	m_vNodes.clear();
	m_vStack.clear();
	m_vIdStack.clear();
	m_UsedIds.clear();
	for(auto &vRoots : m_avLayerRoots)
		vRoots.clear();
	for(int L = 0; L < (int)ELayer::NUM; L++)
	{
		m_avFocusPrev[L].swap(m_avFocusNow[L]);
		m_avFocusNow[L].clear();
	}
	m_vOverlaysPrev.swap(m_vOverlaysNow);
	m_vOverlaysNow.clear();

	m_CurLayer = (int)ELayer::BASE;
	SBox Root;
	Root.m_W = Px(m_ScreenUnits.x);
	Root.m_H = Px(m_ScreenUnits.y);
	const uint32_t RootId = HashString(0, "base");
	const int Index = AddNode(RootId, EDir::COLUMN, Root);
	SetTransform(Index, m_MasterScale, m_MasterAlpha);
	m_avLayerRoots[(int)ELayer::BASE].push_back(Index);
	m_vStack.push_back(Index);
	m_vIdStack.push_back(RootId);
}

int CXui::AddNode(uint32_t Id, EDir Dir, const SBox &B)
{
	const int Index = (int)m_vNodes.size();
	m_vNodes.emplace_back();
	SXuiNode &Node = m_vNodes.back();
	Node.m_Id = Id;
	Node.m_Box = B;
	Node.m_Dir = Dir;
	Node.m_Layer = m_CurLayer;
	if(!m_vStack.empty())
	{
		const int Parent = m_vStack.back();
		Node.m_Parent = Parent;
		SXuiNode &ParentNode = m_vNodes[Parent];
		if(ParentNode.m_Last >= 0)
			m_vNodes[ParentNode.m_Last].m_Next = Index;
		else
			ParentNode.m_First = Index;
		ParentNode.m_Last = Index;
	}
	return Index;
}

uint32_t CXui::CurrentId() const
{
	return m_vIdStack.empty() ? 0 : m_vIdStack.back();
}

uint32_t CXui::IdOf(const char *pKey) const
{
	return HashString(CurrentId(), pKey);
}

uint32_t CXui::MakeId(const char *pKey, bool Unique)
{
	uint32_t Id;
	if(pKey != nullptr)
		Id = HashString(CurrentId(), pKey);
	else
	{
		int Ordinal = 0;
		if(!m_vStack.empty())
		{
			for(int C = m_vNodes[m_vStack.back()].m_First; C >= 0; C = m_vNodes[C].m_Next)
				Ordinal++;
		}
		Id = HashBytes(CurrentId(), &Ordinal, sizeof(Ordinal));
		Id = HashString(Id, "#");
	}
	if(Unique)
	{
		while(!m_UsedIds.insert(Id).second)
			Id = HashString(Id, "+");
	}
	return Id;
}

void CXui::PushId(uint32_t Id)
{
	m_vIdStack.push_back(Id);
}

void CXui::PopId()
{
	m_vIdStack.pop_back();
}

int CXui::Box(EDir Dir, const SBox &B, const FBuild &Children)
{
	return BoxId(MakeId(B.m_pKey), Dir, B, Children);
}

int CXui::BoxId(uint32_t Id, EDir Dir, const SBox &B, const FBuild &Children)
{
	const int Index = AddNode(Id, Dir, B);
	if(Children)
	{
		m_vStack.push_back(Index);
		m_vIdStack.push_back(Id);
		Children();
		m_vIdStack.pop_back();
		m_vStack.pop_back();
	}
	return Index;
}

int CXui::Spacer(float Factor)
{
	SBox B;
	B.m_W = Grow(Factor);
	B.m_H = Grow(Factor);
	return Box(EDir::ROW, B);
}

int CXui::Gap(float Size)
{
	SBox B;
	B.m_W = Px(Size);
	B.m_H = Px(Size);
	return Box(EDir::ROW, B);
}

int CXui::Text(const char *pText, const STextStyle &Style, const SBox &B)
{
	const uint32_t Id = MakeId(B.m_pKey, false);
	const int Index = AddNode(Id, EDir::ROW, B);
	SXuiNode &Node = m_vNodes[Index];
	Node.m_IsText = true;
	Node.m_Text = pText ? pText : "";
	Node.m_TextStyle = Style;

	return Index;
}

int CXui::Icon(const char *pIcon, float Size, ColorRGBA Color, bool Fill)
{
	STextStyle Style;
	Style.m_Size = Size;
	Style.m_Font = Fill ? EFont::PHOSPHOR_FILL : EFont::PHOSPHOR;
	Style.m_Color = Color;
	Style.m_Align = ETextAlign::CENTER;
	SBox B;
	B.m_W = Px(Size);
	B.m_H = Px(Size);
	return Text(pIcon, Style, B);
}

void CXui::Layer(ELayer Layer, const FBuild &Children)
{
	std::vector<int> SavedStack;
	std::vector<uint32_t> SavedIds;
	SavedStack.swap(m_vStack);
	SavedIds.swap(m_vIdStack);
	const int SavedLayer = m_CurLayer;
	m_CurLayer = (int)Layer;

	SBox Root;
	Root.m_W = Px(m_ScreenUnits.x);
	Root.m_H = Px(m_ScreenUnits.y);
	const uint32_t RootId = HashBytes(HashString(0, "layer"), &Layer, sizeof(Layer));
	const int Index = AddNode(RootId, EDir::COLUMN, Root);
	m_avLayerRoots[(int)Layer].push_back(Index);
	m_vStack.push_back(Index);
	m_vIdStack.push_back(RootId);
	if(Children)
		Children();
	m_vStack.swap(SavedStack);
	m_vIdStack.swap(SavedIds);
	m_CurLayer = SavedLayer;
}

void CXui::Paint(int Node, FXuiPaint Paint)
{
	m_vNodes[Node].m_Paint = std::move(Paint);
}

void CXui::PaintOver(int Node, FXuiPaint Paint)
{
	m_vNodes[Node].m_PaintOver = std::move(Paint);
}

void CXui::SetTransform(int Node, float Scale, float Alpha, float OffX, float OffY)
{
	SXuiNode &N = m_vNodes[Node];
	N.m_Scale = Scale;
	N.m_Alpha = Alpha;
	N.m_OffX = OffX;
	N.m_OffY = OffY;
}

CXui::SState &CXui::State(uint32_t Id)
{
	SState &State = m_States[Id];
	State.m_LastSeen = m_Time;
	return State;
}

float CXui::Animate(uint32_t Id, float Target, Motion::SSpring Spec, int Slot, float Init)
{
	SAnim &Anim = State(Id).m_aAnim[Slot];
	if(!Anim.m_Init)
	{
		Anim.m_Spring.Snap(Init > -1e29f ? Init : Target);
		Anim.m_Init = true;
	}
	return Anim.m_Spring.Update(Target, m_Dt, Spec);
}

SRect CXui::AnchorRect(uint32_t Id)
{
	auto It = m_States.find(Id);
	if(It == m_States.end())
		return SRect();
	const SRect &S = It->second.m_ScreenRect;
	return {S.m_X / m_Scale, S.m_Y / m_Scale, S.m_W / m_Scale, S.m_H / m_Scale};
}

SResult CXui::Interact(uint32_t Id, bool Enabled, bool Focusable)
{
	SResult R;
	R.m_Id = Id;
	if(!Enabled)
		return R;
	if(Focusable)
		m_avFocusNow[m_CurLayer].push_back(Id);
	R.m_Hovered = m_HoverId == Id && (m_ActiveId == 0 || m_ActiveId == Id);
	R.m_Pressed = m_ActiveId == Id && m_MouseDown;
	R.m_Clicked = m_ClickedId == Id;
	R.m_RightClicked = m_RightClickedId == Id;
	R.m_Focused = m_FocusId == Id;
	return R;
}

void CXui::Focus(uint32_t Id)
{
	m_FocusId = Id;
}

void CXui::RegisterOverlay(uint32_t Id, bool *pOpen)
{
	m_vOverlaysNow.push_back({Id, pOpen});
}

float CXui::Presence(uint32_t Id, bool Want, Motion::SSpring Spec)
{
	return Animate(Id, Want ? 1.0f : 0.0f, Spec, 7, 0.0f);
}

void CXui::EndFrame()
{
	if(m_KeyCaptureId != 0 && m_UsedIds.count(m_KeyCaptureId) == 0)
		m_KeyCaptureId = 0;
	// the base layer root and any layers were created during the build
	m_vStack.clear();
	m_vIdStack.clear();

	RenderToasts();

	LayoutAll();
	m_PrevMouseDown = m_MouseDown;
	HitTest();
	PaintAll();
	Flush();
	if(m_ClipOn)
	{
		Graphics()->ClipDisable();
		m_ClipOn = false;
	}

	if(m_Frame % 30 == 0)
		GarbageCollect();
	TrimTextCache();

	std::vector<std::function<void()>> vAfterFrame;
	vAfterFrame.swap(m_vAfterFrame);
	for(const auto &Fn : vAfterFrame)
		Fn();
}

void CXui::GarbageCollect()
{
	for(auto It = m_States.begin(); It != m_States.end();)
	{
		if(m_Time - It->second.m_LastSeen > Motion::GC_SECONDS)
		{
			if(It->first == m_TextFocusId)
				m_TextFocusId = 0;
			It = m_States.erase(It);
		}
		else
			++It;
	}
}

CXui::SText *CXui::GetText(const char *pText, float SizePx, EFont Font, float WidthPx, bool Wrap, bool Tabular, int MaxLines, float Tracking)
{
	const int SizeQ = (int)(SizePx * 10.0f + 0.5f);
	const int WidthQ = WidthPx > 0.0f ? (int)(WidthPx + 0.5f) : -1;
	const int Flags = (Wrap ? 1 : 0) | (Tabular ? 2 : 0) | (MaxLines << 4);
	const uint64_t Key = HashText(pText, SizeQ, (int)Font, WidthQ, Flags, (int)std::lround(Tracking * 10000.0f));
	auto It = m_TextCache.find(Key);
	if(It != m_TextCache.end())
	{
		It->second.m_Used = m_Frame;
		return &It->second;
	}

	SText &Entry = m_TextCache[Key];
	Entry.m_Used = m_Frame;

	CTextCursor Cursor;
	Cursor.m_FontSize = (float)SizeQ / 10.0f;
	Cursor.m_LineWidth = WidthQ > 0 ? (float)WidthQ : -1.0f;
	Cursor.m_LineSpacing = Cursor.m_FontSize * (Font::LINE_HEIGHT - 1.0f);
	Cursor.m_MaxLines = MaxLines;
	if(WidthQ > 0 && !Wrap)
		Cursor.m_Flags |= TEXTFLAG_ELLIPSIS_AT_END;
	if(Tabular)
		Cursor.m_Flags |= TEXTFLAG_TABULAR_NUMS;
	if(UsesTracking(Font))
		Cursor.m_LetterSpacing = Tracking;

	if(pText[0] == '\0')
	{
		Entry.m_H = Cursor.m_FontSize;
		return &Entry;
	}
	const unsigned OldFlags = TextRender()->GetRenderFlags();
	TextRender()->SetRenderFlags(TEXT_RENDER_FLAG_FULL_ADVANCE);
	TextRender()->SetFontPreset(PresetOf(Font));
	TextRender()->CreateTextContainer(Entry.m_Index, &Cursor, pText);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	TextRender()->SetRenderFlags(OldFlags);

	if(!Entry.m_Index.Valid())
		return &Entry;
	const STextBoundingBox Box = TextRender()->GetBoundingBoxTextContainer(Entry.m_Index);
	Entry.m_W = Box.m_W;
	Entry.m_H = Box.m_H;
	Entry.m_Lines = std::max(1, Cursor.m_LineCount);
	return &Entry;
}

void CXui::TrimTextCache()
{
	if(m_TextCache.size() < 2000)
		return;
	for(auto It = m_TextCache.begin(); It != m_TextCache.end();)
	{
		if(m_Frame - It->second.m_Used > 120)
		{
			if(It->second.m_Index.Valid())
				TextRender()->DeleteTextContainer(It->second.m_Index);
			It = m_TextCache.erase(It);
		}
		else
			++It;
	}
}

float CXui::TextWidth(const char *pText, const STextStyle &Style)
{
	SText *pEntry = GetText(pText, Style.m_Size * m_Scale, Style.m_Font, -1.0f, false, Style.m_Tabular, Style.m_MaxLines, Style.m_Tracking);
	return pEntry->m_W / m_Scale;
}

void CXui::DrawText(const char *pText, const STextStyle &Style, const SRect &R)
{
	if(pText == nullptr || pText[0] == '\0')
		return;
	const float Scale = m_CurXf.Scale();
	const float SizePx = Style.m_Size * Scale;
	if(SizePx < 1.0f)
		return;
	const float WidthPx = R.m_W * m_CurXf.m_Sx;
	if(WidthPx < 0.5f || R.m_H <= 0.0f)
		return;
	SText *pFree = GetText(pText, SizePx, Style.m_Font, -1.0f, false, Style.m_Tabular, Style.m_MaxLines, Style.m_Tracking);
	SText *pEntry = pFree;
	const float Slack = std::abs(Scale - m_Scale) > 0.001f ? 0.5f + SizePx * 0.25f : 0.5f;
	if(pFree->m_W > WidthPx + Slack)
		pEntry = GetText(pText, SizePx, Style.m_Font, WidthPx, Style.m_Wrap, Style.m_Tabular, Style.m_MaxLines, Style.m_Tracking);

	if(!pEntry->m_Index.Valid())
		return;

	const vec2 Origin = m_CurXf.Apply(vec2(R.m_X, R.m_Y));
	const float HeightPx = R.m_H * m_CurXf.m_Sy;
	float X = Origin.x;
	if(Style.m_Align == ETextAlign::CENTER)
		X += (WidthPx - pEntry->m_W) * 0.5f;
	else if(Style.m_Align == ETextAlign::RIGHT)
		X += WidthPx - pEntry->m_W;
	float Y;
	if(pEntry->m_Lines > 1)
		Y = Origin.y + (Font::LINE_HEIGHT - 1.0f) * SizePx * 0.5f;
	else
		Y = Origin.y + (HeightPx - pEntry->m_H) * 0.5f;
	Y += Font::BASELINE_SHIFT * SizePx;
	if(Style.m_Font == EFont::PHOSPHOR || Style.m_Font == EFont::PHOSPHOR_FILL)
	{
		X += Font::ICON_SHIFT_X * SizePx;
		Y += Font::ICON_SHIFT_Y * SizePx;
	}
	X = std::round(X);
	Y = std::round(Y);

	ColorRGBA Color = Style.m_Color;
	Color.a *= m_CurAlpha;
	if(Color.a <= 0.003f)
		return;

	SRect Bounds = {X, Y, pEntry->m_W, pEntry->m_H};
	SPendingDraw Draw;
	Draw.m_Kind = SPendingDraw::EKind::TEXT;
	Draw.m_Bounds = Bounds;
	Draw.m_Index = pEntry->m_Index;
	Draw.m_Color = Color;
	Draw.m_Pos = vec2(X, Y);
	SyncClip();
	m_vPendDraws.push_back(std::move(Draw));
}

void CXui::DrawTee(const CTeeRenderInfo &Info, int Emote, vec2 Dir, const SRect &R)
{
	const float Scale = m_CurXf.Scale();
	const vec2 Origin = m_CurXf.Apply(vec2(R.m_X, R.m_Y));
	const float W = R.m_W * m_CurXf.m_Sx;
	const float H = R.m_H * m_CurXf.m_Sy;
	SPendingDraw Draw;
	Draw.m_Kind = SPendingDraw::EKind::TEE;
	Draw.m_Bounds = {Origin.x, Origin.y, W, H};
	Draw.m_Tee = Info;
	Draw.m_Tee.m_Size = std::min(R.m_W, R.m_H) * Scale;
	Draw.m_Color = ColorRGBA(1, 1, 1, m_CurAlpha);
	Draw.m_Pos = vec2(Origin.x + W * 0.5f, Origin.y + H * 0.5f);
	Draw.m_Emote = Emote;
	Draw.m_Dir = Dir;
	SyncClip();
	m_vPendDraws.push_back(std::move(Draw));
}

void CXui::DrawImage(const SRect &R, IGraphics::CTextureHandle Texture, vec2 ImageSize, float Radius)
{
	if(!Texture.IsValid() || R.m_W <= 0 || R.m_H <= 0 || ImageSize.x <= 0 || ImageSize.y <= 0)
		return;
	const float Alpha = CurrentAlpha();
	DrawDirect(R, [this, Texture, ImageSize, Radius, Alpha, R](const SRect &Px) {
		IGraphics *pGraphics = Graphics();
		const float Aspect = ImageSize.x / ImageSize.y;
		const float BoxAspect = Px.m_W / Px.m_H;
		const vec2 Span(Aspect > BoxAspect ? BoxAspect / Aspect : 1.0f, Aspect < BoxAspect ? Aspect / BoxAspect : 1.0f);
		const vec2 Origin = (vec2(1, 1) - Span) * 0.5f;
		const float Round = std::clamp(Radius * Px.m_W / R.m_W, 0.0f, std::min(Px.m_W, Px.m_H) * 0.5f);
		const vec2 Center(Px.CenterX(), Px.CenterY());
		std::array<vec2, 36> aEdge;
		for(int Corner = 0; Corner < 4; ++Corner)
		{
			const vec2 C(Px.m_X + ((Corner == 1 || Corner == 2) ? Px.m_W - Round : Round), Px.m_Y + (Corner >= 2 ? Px.m_H - Round : Round));
			for(int Segment = 0; Segment <= 8; ++Segment)
				aEdge[Corner * 9 + Segment] = C + direction(pi + Corner * pi / 2.0f + Segment * pi / 16.0f) * Round;
		}
		auto Uv = [&](vec2 P) { return Origin + vec2((P.x - Px.m_X) / Px.m_W, (P.y - Px.m_Y) / Px.m_H) * Span; };
		pGraphics->WrapClamp();
		pGraphics->TextureSet(Texture);
		pGraphics->QuadsBegin();
		pGraphics->SetColor(1, 1, 1, Alpha);
		for(size_t i = 0; i < aEdge.size(); ++i)
		{
			const vec2 A = aEdge[i], B = aEdge[(i + 1) % aEdge.size()];
			const vec2 U = Uv(Center), V = Uv(A), W = Uv(B);
			pGraphics->QuadsSetSubsetFree(U.x, U.y, V.x, V.y, U.x, U.y, W.x, W.y);
			const IGraphics::CFreeformItem Quad(Center.x, Center.y, A.x, A.y, Center.x, Center.y, B.x, B.y);
			pGraphics->QuadsDrawFreeform(&Quad, 1);
		}
		pGraphics->QuadsEnd();
		pGraphics->WrapNormal();
	});
}

void CXui::DrawDirect(const SRect &Bounds, std::function<void(const SRect &)> Draw)
{
	const vec2 Origin = m_CurXf.Apply(vec2(Bounds.m_X, Bounds.m_Y));
	SPendingDraw Pending;
	Pending.m_Kind = SPendingDraw::EKind::DIRECT;
	Pending.m_Bounds = {Origin.x, Origin.y, Bounds.m_W * m_CurXf.m_Sx, Bounds.m_H * m_CurXf.m_Sy};
	Pending.m_Direct = std::move(Draw);
	SyncClip();
	m_vPendDraws.push_back(std::move(Pending));
}

void CXui::DrawRect(const SRect &R, IGraphics::SUiRect Style)
{
	const float Scale = m_CurXf.Scale();
	const vec2 Origin = m_CurXf.Apply(vec2(R.m_X, R.m_Y));
	Style.m_X = Origin.x;
	Style.m_Y = Origin.y;
	Style.m_W = R.m_W * m_CurXf.m_Sx;
	Style.m_H = R.m_H * m_CurXf.m_Sy;
	if(Style.m_W <= 0.0f || Style.m_H <= 0.0f)
		return;

	const float Half = std::min(Style.m_W, Style.m_H) * 0.5f;
	auto FixRadius = [&](float Radius) { return Radius < 0.0f ? Half : std::min(Radius * Scale, Half); };
	Style.m_Radii = vec4(FixRadius(Style.m_Radii.x), FixRadius(Style.m_Radii.y), FixRadius(Style.m_Radii.z), FixRadius(Style.m_Radii.w));
	Style.m_BorderWidth *= Scale;
	Style.m_ShadowOffset *= Scale;
	Style.m_ShadowBlur *= Scale;
	Style.m_ShadowSpread *= Scale;
	Style.m_Fill.a *= m_CurAlpha;
	if(Style.m_Fill2.a >= 0.0f)
		Style.m_Fill2.a *= m_CurAlpha;
	Style.m_Border.a *= m_CurAlpha;
	if(Style.m_Border2.a >= 0.0f)
		Style.m_Border2.a *= m_CurAlpha;
	Style.m_Shadow.a *= m_CurAlpha;
	Style.m_Glass *= m_CurAlpha;

	SRect Bounds = {Style.m_X, Style.m_Y, Style.m_W, Style.m_H};
	if(Style.m_Shadow.a > 0.0f)
	{
		const float Reach = Style.m_ShadowBlur * 0.5f + Style.m_ShadowSpread + std::max(std::abs(Style.m_ShadowOffset.x), std::abs(Style.m_ShadowOffset.y));
		Bounds = {Bounds.m_X - Reach, Bounds.m_Y - Reach, Bounds.m_W + Reach * 2.0f, Bounds.m_H + Reach * 2.0f};
	}
	EmitRect(Style, Bounds);
}

void CXui::EmitRect(const IGraphics::SUiRect &R, const SRect &Bounds)
{
	SyncClip();
	for(const SPendingDraw &Draw : m_vPendDraws)
	{
		if(Draw.m_Bounds.Overlaps(Bounds))
		{
			Flush();
			break;
		}
	}
	if(R.m_Glass > 0.0f)
	{
		for(const SPendingRect &Rect : m_vPendRects)
		{
			if(Rect.m_Bounds.Overlaps(Bounds))
			{
				Flush();
				break;
			}
		}
	}
	m_vPendRects.push_back({R, Bounds});
}

void CXui::SetClip(bool On, const SRect &Clip)
{
	m_ClipWantOn = On;
	m_ClipWant = Clip;
}

void CXui::SyncClip()
{
	const bool Same = m_ClipWantOn == m_ClipOn && (!m_ClipOn || (m_ClipWant.m_X == m_ClipCur.m_X && m_ClipWant.m_Y == m_ClipCur.m_Y && m_ClipWant.m_W == m_ClipCur.m_W && m_ClipWant.m_H == m_ClipCur.m_H));
	if(Same)
		return;
	Flush();
	m_ClipOn = m_ClipWantOn;
	m_ClipCur = m_ClipWant;
	if(m_ClipOn)
	{
		const float Dpi = Graphics()->ScreenHiDPIScale();
		const int X = (int)std::floor(m_ClipCur.m_X * Dpi);
		const int Y = (int)std::floor(m_ClipCur.m_Y * Dpi);
		const int X2 = (int)std::ceil((m_ClipCur.m_X + m_ClipCur.m_W) * Dpi);
		const int Y2 = (int)std::ceil((m_ClipCur.m_Y + m_ClipCur.m_H) * Dpi);
		Graphics()->ClipEnable(X, Y, std::max(0, X2 - X), std::max(0, Y2 - Y));
	}
	else
		Graphics()->ClipDisable();
}

void CXui::Flush()
{
	if(!m_vPendRects.empty())
	{
		std::vector<IGraphics::SUiRect> vRects;
		vRects.reserve(m_vPendRects.size());
		for(const SPendingRect &Rect : m_vPendRects)
			vRects.push_back(Rect.m_Rect);
		Graphics()->DrawUiRects(vRects.data(), (int)vRects.size());
		m_vPendRects.clear();
	}
	if(m_vPendDraws.empty())
		return;
	std::vector<SPendingDraw> vDraws;
	vDraws.swap(m_vPendDraws);
	for(SPendingDraw &Draw : vDraws)
	{
		switch(Draw.m_Kind)
		{
		case SPendingDraw::EKind::TEXT:
			TextRender()->RenderTextContainer(Draw.m_Index, Draw.m_Color, ColorRGBA(0, 0, 0, 0), Draw.m_Pos.x, Draw.m_Pos.y);
			break;
		case SPendingDraw::EKind::TEE:
			Graphics()->TextureClear();
			RenderTools()->RenderIdleTeeAtCenter(&Draw.m_Tee, Draw.m_Pos, Draw.m_Emote, Draw.m_Dir, Draw.m_Color.a);
			break;
		case SPendingDraw::EKind::DIRECT:
			Draw.m_Direct(Draw.m_Bounds);
			break;
		}
	}
}
