#include <base/str.h>

#include <engine/client/keyboard.h>
#include <engine/font_icons_phosphor.h>
#include <engine/keys.h>

#include <game/client/xui/xui.h>
#include <game/localization.h>

#include <algorithm>
#include <cmath>

using namespace Xui;

namespace
{
	void FocusRing(CXui &Ui, const SRect &R, float Radius, float Amount, ColorRGBA Accent)
	{
		if(Amount < 0.01f)
			return;
		IGraphics::SUiRect Ring{};
		Ring.m_Radii = vec4(Radius + XuiTheme::Size::FOCUS_RING, Radius + XuiTheme::Size::FOCUS_RING, Radius + XuiTheme::Size::FOCUS_RING, Radius + XuiTheme::Size::FOCUS_RING);
		Ring.m_BorderWidth = XuiTheme::Size::FOCUS_RING;
		Ring.m_Border = CXui::WithAlpha(Accent, 0.6f * Amount);
		Ui.DrawRect(R.Inset(-XuiTheme::Size::FOCUS_RING), Ring);
	}

	ColorRGBA HsvToRgb(float H, float S, float V)
	{
		H = std::fmod(H, 1.0f) * 6.0f;
		const int I = (int)H;
		const float F = H - I;
		const float P = V * (1.0f - S);
		const float Q = V * (1.0f - S * F);
		const float T = V * (1.0f - S * (1.0f - F));
		switch(I % 6)
		{
		case 0: return ColorRGBA(V, T, P, 1);
		case 1: return ColorRGBA(Q, V, P, 1);
		case 2: return ColorRGBA(P, V, T, 1);
		case 3: return ColorRGBA(P, Q, V, 1);
		case 4: return ColorRGBA(T, P, V, 1);
		}
		return ColorRGBA(V, P, Q, 1);
	}

	void RgbToHsv(ColorRGBA C, float &H, float &S, float &V)
	{
		const float Max = std::max({C.r, C.g, C.b});
		const float Min = std::min({C.r, C.g, C.b});
		const float D = Max - Min;
		V = Max;
		S = Max <= 0.0f ? 0.0f : D / Max;
		if(D <= 0.0f)
		{
			H = 0.0f;
			return;
		}
		if(Max == C.r)
			H = std::fmod((C.g - C.b) / D + 6.0f, 6.0f);
		else if(Max == C.g)
			H = (C.b - C.r) / D + 2.0f;
		else
			H = (C.r - C.g) / D + 4.0f;
		H /= 6.0f;
	}
}

SResult CXui::Switch(const char *pLabel, int *pValue, const SSwitchStyle &Style)
{
	const uint32_t Id = MakeId(Style.m_pKey ? Style.m_pKey : pLabel);
	SResult R = Interact(Id, !Style.m_Disabled);
	SState &St = State(Id);
	bool On = *pValue != 0;

	bool &Dragging = St.m_Flag;
	const float PressX = St.m_aScratch[0];
	if(R.m_Pressed)
	{
		if(St.m_Int == 0)
		{
			St.m_Int = 1;
			St.m_aScratch[0] = m_Mouse.x;
			Dragging = false;
		}
		else if(!Dragging && std::abs(m_Mouse.x - PressX) > 4.0f * m_Scale)
			Dragging = true;
	}
	float DragFrac = On ? 1.0f : 0.0f;
	if(Dragging && St.m_ScreenRect.m_W > 0.0f)
	{
		const float KnobLeft = (m_Mouse.x - St.m_ScreenRect.m_X) / m_Scale - XuiTheme::Size::KNOB * 0.5f;
		DragFrac = std::clamp((KnobLeft - 2.0f) / (XuiTheme::Size::SWITCH_W - 4.0f - XuiTheme::Size::KNOB), 0.0f, 1.0f);
	}
	bool Clicked = R.m_Clicked;
	if(!R.m_Pressed && St.m_Int == 1)
	{
		St.m_Int = 0;
		if(Dragging)
		{
			const bool Want = DragFrac > 0.5f;
			if(Want != On)
			{
				*pValue = Want ? 1 : 0;
				R.m_Changed = true;
				On = Want;
			}
			Clicked = false;
		}
		Dragging = false;
	}
	if(Clicked)
	{
		On = !On;
		*pValue = On ? 1 : 0;
		R.m_Changed = true;
	}

	if(!St.m_Init)
	{
		St.m_Init = true;
		St.m_Hover.Snap(On ? 1.0f : 0.0f);
		St.m_Press.Snap(On ? XuiTheme::Size::SWITCH_W - 2.0f - XuiTheme::Size::KNOB : 2.0f);
	}
	const bool Focused = R.m_Focused && m_FocusVisible;
	const float OnAmount = St.m_Hover.Update(On ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
	const float Left = St.m_Press.Update(Dragging ? 2.0f + DragFrac * (XuiTheme::Size::SWITCH_W - 4.0f - XuiTheme::Size::KNOB) : (On ? XuiTheme::Size::SWITCH_W - 2.0f - XuiTheme::Size::KNOB : 2.0f), m_Dt, XuiTheme::Motion::KNOB);
	const float Width = Animate(Id, R.m_Pressed ? XuiTheme::Size::KNOB_PRESSED_W : XuiTheme::Size::KNOB, XuiTheme::Motion::HOVER, 0, XuiTheme::Size::KNOB);
	const float Focus = St.m_Focus.Update(Focused ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
	const ColorRGBA OnColor = Style.m_UseGreen ? XuiTheme::Color::GREEN : m_Accent;
	const ColorRGBA Accent = m_Accent;
	const ColorRGBA KnobColor = Style.m_UseGreen ? XuiTheme::Color::KNOB : Mix(XuiTheme::Color::KNOB, Mix(m_OnAccent, XuiTheme::Color::KNOB, 0.25f), OnAmount);

	SBox B;
	B.m_W = Px(XuiTheme::Size::SWITCH_W);
	B.m_H = Px(XuiTheme::Size::SWITCH_H);
	B.m_Hit = Style.m_Disabled ? EHit::BLOCK : EHit::INTERACTIVE;
	const int Node = BoxId(Id, EDir::ROW, B);
	SetTransform(Node, 1.0f, Style.m_Disabled ? 0.4f : 1.0f);
	Paint(Node, [=](CXui &Ui, const SRect &Rc) {
		FocusRing(Ui, Rc, Rc.m_H * 0.5f, Focus, Accent);
		Ui.DrawRect(Rc, SolidRect(Mix(XuiTheme::Color::TRACK_OFF, OnColor, OnAmount), Rc.m_H * 0.5f));
		const float KnobLeft = std::min(Left, XuiTheme::Size::SWITCH_W - 2.0f - Width);
		IGraphics::SUiRect Knob = SolidRect(KnobColor, XuiTheme::Size::KNOB * 0.5f + 0.5f);
		Knob.m_Shadow = ColorRGBA(0, 0, 0, 0.25f);
		Knob.m_ShadowOffset = vec2(0.0f, 3.0f);
		Knob.m_ShadowBlur = 8.0f;
		Ui.DrawRect({Rc.m_X + KnobLeft, Rc.m_Y + 2.0f, Width, XuiTheme::Size::KNOB}, Knob);
	});
	return R;
}

SResult CXui::Field(const char *pKey, char *pBuffer, int BufferSize, const SFieldStyle &Style)
{
	const uint32_t Id = MakeId(pKey);
	SResult R = Interact(Id, !Style.m_Disabled);
	SState &St = State(Id);
	if(!St.m_pInput)
		St.m_pInput = std::make_unique<CLineInput>();
	CLineInput *pInput = St.m_pInput.get();
	if(St.m_pBound != pBuffer)
	{
		pInput->SetBuffer(pBuffer, BufferSize);
		St.m_pBound = pBuffer;
	}
	const bool Focused = m_FocusId == Id && !Style.m_Disabled;
	if(Focused && !pInput->IsActive())
	{
		pInput->Activate(EInputPriority::UI);
		m_TextFocusId = Id;
	}
	else if(!Focused && pInput->IsActive())
	{
		pInput->Deactivate();
		if(m_TextFocusId == Id)
			m_TextFocusId = 0;
	}
	CLineInput::SMouseSelection *pSel = pInput->GetMouseSelection();
	if(Focused && R.m_Pressed)
	{
		if(!pSel->m_Selecting)
		{
			pSel->m_Selecting = true;
			pSel->m_PressMouse = m_Mouse;
		}
		pSel->m_ReleaseMouse = m_Mouse;
	}
	else
		pSel->m_Selecting = false;
	const bool Changed = pInput->WasChanged();
	R.m_Changed = Changed;
	R.m_Submitted = Focused && (KeyPressed(KEY_RETURN) || KeyPressed(KEY_KP_ENTER));

	const float FocusAmount = St.m_Focus.Update(Focused ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
	const ColorRGBA Accent = m_Accent;
	const bool Active = pInput->IsActive();
	const bool Right = Style.m_Right;
	const bool HasText = pBuffer[0] != '\0';
	const char *pPlaceholder = Style.m_pPlaceholder;

	SBox B;
	B.m_Gap = 8.0f;
	B.m_Padding = SEdges(0.0f, XuiTheme::Size::FIELD_PAD_X);
	B.m_W = Style.m_W;
	B.m_H = Px(XuiTheme::Size::CONTROL_H);
	B.m_Cross = EAlign::CENTER;
	B.m_Hit = Style.m_Disabled ? EHit::BLOCK : EHit::INTERACTIVE;
	bool Clear = false;
	const int Node = BoxId(Id, EDir::ROW, B, [&]() {
		if(Style.m_pIcon != nullptr)
			Icon(Style.m_pIcon, 16.0f, XuiTheme::Color::TEXT3);
		SBox TextBox;
		TextBox.m_W = Grow();
		TextBox.m_H = Pct(100.0f);
		TextBox.m_Clip = true;
		const int TextNode = Box(EDir::ROW, TextBox);
		SState *pSt = &St;
		Paint(TextNode, [=, this](CXui &Ui, const SRect &Rc) {
			STextStyle TextStyle;
			TextStyle.m_Size = XuiTheme::Font::FIELD;
			TextStyle.m_Align = Right ? ETextAlign::RIGHT : ETextAlign::LEFT;
			if(HasText && !Active)
				Ui.DrawText(pBuffer, TextStyle, Rc);
			else if(!HasText && pPlaceholder != nullptr)
			{
				TextStyle.m_Color = XuiTheme::Color::TEXT3;
				Ui.DrawText(pPlaceholder, TextStyle, Rc);
			}
			if(Active)
			{
				const float Scale = Ui.CurrentXf().Scale();
				Ui.DrawDirect(Rc, [=, this](const SRect &Px) {
					TextRender()->SetFontPreset(EFontPreset::UI_REGULAR);
					TextRender()->TextColor(XuiTheme::Color::TEXT);
					CUIRect Rect;
					Rect.x = Px.m_X - pSt->m_aScratch[1];
					Rect.y = Px.m_Y;
					Rect.w = Px.m_W + pSt->m_aScratch[1];
					Rect.h = Px.m_H;
					pInput->Render(&Rect, XuiTheme::Font::FIELD * Scale, TEXTALIGN_ML, Changed, -1.0f, 0.0f);
					pSt->m_aScratch[1] = std::max(0.0f, pInput->GetCaretPosition().x - Rect.x - (Px.m_W - 6.0f * Scale));
					TextRender()->TextColor(TextRender()->DefaultTextColor());
					TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
				});
			}
		});
		if(Style.m_Clearable && HasText)
		{
			SButtonStyle Small;
			Small.m_Height = 20.0f;
			Small.m_Variant = EVariant::PLAIN;
			Small.m_pKey = "clear";
			if(IconButton(Ph::X_CIRCLE, Small).m_Clicked)
				Clear = true;
		}
	});
	Paint(Node, [=](CXui &Ui, const SRect &Rc) {
		const float Radius = Rc.m_H * 0.5f;
		FocusRing(Ui, Rc, Radius, FocusAmount, Accent);
		Ui.DrawRect(Rc, SolidRect(XuiTheme::Color::GHOST, Radius));
	});
	SetTransform(Node, 1.0f, Style.m_Disabled ? 0.4f : 1.0f);
	if(Clear)
	{
		pInput->Clear();
		R.m_Changed = true;
	}
	return R;
}

SResult CXui::SearchField(const char *pKey, char *pBuffer, int BufferSize, const char *pPlaceholder, SSize W)
{
	SFieldStyle Style;
	Style.m_pPlaceholder = pPlaceholder;
	Style.m_pIcon = Ph::MAGNIFYING_GLASS;
	Style.m_W = W;
	Style.m_Clearable = true;
	return Field(pKey, pBuffer, BufferSize, Style);
}

SResult CXui::Segmented(const char *pKey, const char *const *ppLabels, int Count, int *pValue, bool Disabled)
{
	const uint32_t Id = MakeId(pKey);
	SResult R;
	R.m_Id = Id;
	std::vector<uint32_t> vItemIds;
	const ColorRGBA Accent = m_Accent;
	SBox B;
	B.m_Padding = SEdges(XuiTheme::Size::SEGMENT_PAD);
	B.m_Cross = EAlign::CENTER;
	B.m_Hit = EHit::BLOCK;
	const int Node = BoxId(Id, EDir::ROW, B, [&]() {
		for(int i = 0; i < Count; i++)
		{
			const uint32_t ItemId = MakeId(ppLabels[i]);
			vItemIds.push_back(ItemId);
			SResult Item = Interact(ItemId, !Disabled);
			SState &St = State(ItemId);
			const bool Selected = *pValue == i;
			if(!St.m_Init)
			{
				St.m_Init = true;
				St.m_Hover.Snap(Selected ? 1.0f : 0.0f);
			}
			const float Sel = St.m_Hover.Update(Selected ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
			const float Focus = St.m_Focus.Update(Item.m_Focused && m_FocusVisible ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
			if(Item.m_Clicked && !Selected)
			{
				*pValue = i;
				R.m_Changed = true;
			}
			SBox ItemBox;
			ItemBox.m_H = Px(XuiTheme::Size::SEGMENT_H);
			ItemBox.m_Padding = SEdges(0.0f, XuiTheme::Size::SEGMENT_ITEM_PAD_X);
			ItemBox.m_Main = EAlign::CENTER;
			ItemBox.m_Cross = EAlign::CENTER;
			ItemBox.m_Hit = Disabled ? EHit::BLOCK : EHit::INTERACTIVE;
			const int ItemNode = BoxId(ItemId, EDir::ROW, ItemBox, [&]() {
				STextStyle Label;
				Label.m_Size = XuiTheme::Font::SEGMENT;
				Label.m_Font = EFont::SEMIBOLD;
				Label.m_Color = Mix(XuiTheme::Color::TEXT_IDLE, XuiTheme::Color::TEXT, Sel);
				Text(ppLabels[i], Label);
			});
			Paint(ItemNode, [Focus, Accent](CXui &Ui, const SRect &Rc) {
				FocusRing(Ui, Rc, Rc.m_H * 0.5f, Focus, Accent);
			});
		}
	});
	SetTransform(Node, 1.0f, Disabled ? 0.4f : 1.0f);
	const int Selected = *pValue;
	Paint(Node, [this, Id, vItemIds, Selected](CXui &Ui, const SRect &Rc) {
		Ui.DrawRect(Rc, SolidRect(XuiTheme::Color::GHOST, Rc.m_H * 0.5f));
		if(Selected < 0 || Selected >= (int)vItemIds.size())
			return;
		auto It = m_States.find(vItemIds[Selected]);
		if(It == m_States.end() || It->second.m_Rect.m_W <= 0.0f)
			return;
		const SRect Item = It->second.m_Rect;
		const float X = Ui.Animate(Id, Item.m_X, XuiTheme::Motion::INDICATOR, 1);
		const float W = Ui.Animate(Id, Item.m_W, XuiTheme::Motion::INDICATOR, 2);
		const float Vel = m_States[Id].m_aAnim[1].m_Spring.m_Velocity;
		const float Stretch = std::clamp(std::abs(Vel) / 650.0f, 0.0f, 1.0f) * XuiTheme::Size::SEGMENT_STRETCH * W;
		IGraphics::SUiRect Pill = SolidRect(XuiTheme::Color::SEGMENT_PILL, XuiTheme::Size::SEGMENT_H * 0.5f);
		Pill.m_Shadow = ColorRGBA(0, 0, 0, 0.25f);
		Pill.m_ShadowOffset = vec2(0.0f, 2.0f);
		Pill.m_ShadowBlur = 8.0f;
		Pill.m_BorderWidth = 1.0f;
		Pill.m_Border = ColorRGBA(1, 1, 1, 0.3f);
		Pill.m_Border2 = ColorRGBA(1, 1, 1, 0.0f);
		Pill.m_BorderAngle = 135.0f;
		Ui.DrawRect({X - (Vel < 0.0f ? Stretch : 0.0f), Item.m_Y, W + Stretch, Item.m_H}, Pill);
	});
	return R;
}

SResult CXui::Tabs(const char *pKey, const char *const *ppLabels, int Count, int *pValue)
{
	return Segmented(pKey, ppLabels, Count, pValue);
}

SResult CXui::Slider(const char *pKey, int *pValue, const SSliderStyle &Style)
{
	const uint32_t Id = MakeId(pKey);
	SResult R = Interact(Id, !Style.m_Disabled);
	SState &St = State(Id);
	const float Range = Style.m_Max - Style.m_Min;
	if(R.m_Pressed && St.m_ScreenRect.m_W > 0.0f)
	{
		const float Half = XuiTheme::Size::SLIDER_THUMB * 0.5f * m_Scale;
		const float Frac = std::clamp((m_Mouse.x - St.m_ScreenRect.m_X - Half) / (St.m_ScreenRect.m_W - Half * 2.0f), 0.0f, 1.0f);
		float Value = Style.m_Min + Frac * Range;
		if(Style.m_Step > 0.0f)
			Value = Style.m_Min + std::round((Value - Style.m_Min) / Style.m_Step) * Style.m_Step;
		if((int)Value != *pValue)
		{
			*pValue = (int)Value;
			R.m_Changed = true;
		}
	}
	if(R.m_Focused && m_FocusVisible)
	{
		int Delta = 0;
		if(KeyPressed(KEY_RIGHT) || KeyPressed(KEY_UP))
			Delta = 1;
		else if(KeyPressed(KEY_LEFT) || KeyPressed(KEY_DOWN))
			Delta = -1;
		if(Delta != 0)
		{
			*pValue = std::clamp(*pValue + Delta * (int)std::max(1.0f, Style.m_Step), (int)Style.m_Min, (int)Style.m_Max);
			R.m_Changed = true;
		}
	}
	const float Frac = Range > 0.0f ? std::clamp((*pValue - Style.m_Min) / Range, 0.0f, 1.0f) : 0.0f;
	const float Bubble = Animate(Id, R.m_Pressed ? 1.0f : 0.0f, XuiTheme::Motion::POP, 0, 0.0f);
	const float Focus = St.m_Focus.Update(R.m_Focused && m_FocusVisible ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
	const ColorRGBA Accent = m_Accent;
	char aValue[32];
	str_format(aValue, sizeof(aValue), Style.m_pFormat, *pValue);
	std::string Value = aValue;
	const float Width = Style.m_Width;

	SBox B;
	B.m_W = Px(Width);
	B.m_H = Px(XuiTheme::Size::SLIDER_THUMB);
	B.m_Hit = Style.m_Disabled ? EHit::BLOCK : EHit::INTERACTIVE;
	const int Node = BoxId(Id, EDir::ROW, B);
	SetTransform(Node, 1.0f, Style.m_Disabled ? 0.4f : 1.0f);
	Paint(Node, [=](CXui &Ui, const SRect &Rc) {
		const float Half = XuiTheme::Size::SLIDER_THUMB * 0.5f;
		const float Cx = Rc.m_X + Half + Frac * (Rc.m_W - XuiTheme::Size::SLIDER_THUMB);
		const float TrackY = Rc.CenterY() - XuiTheme::Size::SLIDER_TRACK * 0.5f;
		Ui.DrawRect({Rc.m_X, TrackY, Rc.m_W, XuiTheme::Size::SLIDER_TRACK}, SolidRect(XuiTheme::Color::TRACK_OFF, XuiTheme::Size::SLIDER_TRACK * 0.5f));
		Ui.DrawRect({Rc.m_X, TrackY, Cx - Rc.m_X, XuiTheme::Size::SLIDER_TRACK}, SolidRect(Accent, XuiTheme::Size::SLIDER_TRACK * 0.5f));
		FocusRing(Ui, {Cx - Half, Rc.m_Y, XuiTheme::Size::SLIDER_THUMB, XuiTheme::Size::SLIDER_THUMB}, Half, Focus, Accent);
		IGraphics::SUiRect Thumb = SolidRect(XuiTheme::Color::KNOB, Half);
		Thumb.m_Shadow = ColorRGBA(0, 0, 0, 0.3f);
		Thumb.m_ShadowOffset = vec2(0.0f, 3.0f);
		Thumb.m_ShadowBlur = 8.0f;
		Ui.DrawRect({Cx - Half, Rc.m_Y, XuiTheme::Size::SLIDER_THUMB, XuiTheme::Size::SLIDER_THUMB}, Thumb);
	});
	PaintOver(Node, [=](CXui &Ui, const SRect &Rc) {
		if(Bubble < 0.01f)
			return;
		STextStyle Label;
		Label.m_Size = XuiTheme::Font::CAP;
		Label.m_Font = EFont::SEMIBOLD;
		Label.m_Align = ETextAlign::CENTER;
		Label.m_Tabular = true;
		const float TextW = Ui.TextWidth(Value.c_str(), Label);
		const float W = std::max(40.0f, TextW + 24.0f);
		const float Cx = Rc.m_X + XuiTheme::Size::SLIDER_THUMB * 0.5f + Frac * (Rc.m_W - XuiTheme::Size::SLIDER_THUMB);
		const float BubbleX = std::clamp(Cx - W * 0.5f, Rc.m_X, std::max(Rc.m_X, Rc.m_X + Rc.m_W - W));
		const SRect Bubble_ = {BubbleX, Rc.m_Y - XuiTheme::Size::SLIDER_BUBBLE_H - 10.0f - (1.0f - Bubble) * -6.0f, W, XuiTheme::Size::SLIDER_BUBBLE_H};
		Ui.DrawRect(Bubble_, GlassRect(XuiTheme::Size::SLIDER_BUBBLE_H * 0.5f, XuiTheme::Glass::PANEL_ALPHA));
		Ui.DrawText(Value.c_str(), Label, Bubble_);
	});
	return R;
}

SResult CXui::Kbd(const char *pKey, int *pKeyCode, bool Disabled)
{
	const uint32_t Id = MakeId(pKey);
	SResult R = Interact(Id, !Disabled);
	SState &St = State(Id);
	if(R.m_Clicked)
		m_KeyCaptureId = Id;
	bool Capturing = m_KeyCaptureId == Id;
	if(Capturing && Disabled)
		Capturing = false;
	if(Capturing && !R.m_Clicked)
	{
		for(int Key : m_vKeysThisFrame)
		{
			*pKeyCode = Key == KEY_ESCAPE ? KEY_UNKNOWN : Key;
			R.m_Changed = true;
			Capturing = false;
			break;
		}
	}
	if(!Capturing && m_KeyCaptureId == Id)
		m_KeyCaptureId = 0;
	Tooltip(Id, Localize("Press Escape to clear this bind"));
	const float Hover = St.m_Hover.Update(R.m_Hovered || Capturing ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
	const float Scale = St.m_Scale.Update(R.m_Pressed ? XuiTheme::Motion::PRESS_SCALE : 1.0f, m_Dt, XuiTheme::Motion::PRESS);
	const float Focus = St.m_Focus.Update((Capturing || (R.m_Focused && m_FocusVisible)) ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
	const ColorRGBA Accent = m_Accent;
	const char *pName = Capturing ? Localize("Press a key") : (*pKeyCode > 0 ? KeyName(*pKeyCode) : Localize("None"));

	SBox B;
	B.m_H = Px(XuiTheme::Size::KBD_H);
	B.m_W = Fit(XuiTheme::Size::KBD_MIN_W);
	B.m_Padding = SEdges(0.0f, XuiTheme::Size::KBD_PAD_X);
	B.m_Main = EAlign::CENTER;
	B.m_Cross = EAlign::CENTER;
	B.m_Hit = Disabled ? EHit::BLOCK : EHit::INTERACTIVE;
	const int Node = BoxId(Id, EDir::ROW, B, [&]() {
		STextStyle Label;
		Label.m_Size = XuiTheme::Font::KBD;
		Label.m_Font = EFont::SEMIBOLD;
		Text(pName, Label);
	});
	SetTransform(Node, Scale, Disabled ? 0.4f : 1.0f);
	Paint(Node, [=](CXui &Ui, const SRect &Rc) {
		FocusRing(Ui, Rc, Rc.m_H * 0.5f, Focus, Accent);
		Ui.DrawRect(Rc, SolidRect(Mix(XuiTheme::Color::GHOST, XuiTheme::Color::GHOST_HOVER, Hover), Rc.m_H * 0.5f));
	});
	return R;
}

SResult CXui::KbdLabel(const char *pText, float Height)
{
	const uint32_t Id = MakeId(pText);
	SBox B;
	B.m_H = Px(Height);
	B.m_W = Fit(Height < XuiTheme::Size::KBD_H ? 26.0f : XuiTheme::Size::KBD_MIN_W);
	B.m_Padding = SEdges(0.0f, Height < XuiTheme::Size::KBD_H ? 6.0f : XuiTheme::Size::KBD_PAD_X);
	B.m_Main = EAlign::CENTER;
	B.m_Cross = EAlign::CENTER;
	const bool Small = Height < XuiTheme::Size::KBD_H;
	const int Node = BoxId(Id, EDir::ROW, B, [&]() {
		STextStyle Label;
		Label.m_Size = Small ? 10.5f : XuiTheme::Font::KBD;
		Label.m_Font = EFont::SEMIBOLD;
		Text(pText, Label);
	});
	Paint(Node, [Small](CXui &Ui, const SRect &Rc) {
		Ui.DrawRect(Rc, SolidRect(Small ? ColorRGBA(0, 0, 0, 0.25f) : XuiTheme::Color::GHOST, Rc.m_H * 0.5f));
	});
	SResult R;
	R.m_Id = Id;
	return R;
}

SResult CXui::Swatch(const char *pKey, ColorRGBA Color, bool Selected)
{
	const uint32_t Id = MakeId(pKey);
	SResult R = Interact(Id);
	SState &St = State(Id);
	const float Scale = St.m_Scale.Update(R.m_Pressed ? XuiTheme::Motion::PRESS_SCALE : (R.m_Hovered ? XuiTheme::Motion::SWATCH_HOVER_SCALE : 1.0f), m_Dt, XuiTheme::Motion::PRESS);
	if(!St.m_Init)
	{
		St.m_Init = true;
		St.m_Hover.Snap(Selected ? 1.0f : 0.0f);
	}
	const float Ring = St.m_Hover.Update(Selected ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::POP);
	const float Focus = St.m_Focus.Update(R.m_Focused && m_FocusVisible ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
	const ColorRGBA Accent = m_Accent;
	SBox B;
	B.m_W = Px(XuiTheme::Size::SWATCH);
	B.m_H = Px(XuiTheme::Size::SWATCH);
	B.m_Hit = EHit::INTERACTIVE;
	const int Node = BoxId(Id, EDir::ROW, B);
	SetTransform(Node, Scale, 1.0f);
	Paint(Node, [=](CXui &Ui, const SRect &Rc) {
		FocusRing(Ui, Rc.Inset(-4.0f * Ring), Rc.m_H * 0.5f, Focus, Accent);
		if(Ring > 0.01f)
		{
			Ui.DrawRect(Rc.Inset(-4.0f * Ring), SolidRect(WithAlpha(Color, 1.0f), Rc.m_H * 0.5f + 4.0f));
			Ui.DrawRect(Rc.Inset(-2.0f * Ring), SolidRect(XuiTheme::Rgb(0x1C1C1E), Rc.m_H * 0.5f + 2.0f));
		}
		Ui.DrawRect(Rc, SolidRect(Color, Rc.m_H * 0.5f));
	});
	return R;
}

SResult CXui::SwatchRow(const char *pKey, const ColorRGBA *pColors, int Count, int *pSelected)
{
	SResult Result;
	SBox B;
	B.m_Gap = 8.0f;
	B.m_Cross = EAlign::CENTER;
	B.m_pKey = pKey;
	Row(B, [&]() {
		for(int i = 0; i < Count; i++)
		{
			char aKey[16];
			str_format(aKey, sizeof(aKey), "sw%d", i);
			SResult R = Swatch(aKey, pColors[i], *pSelected == i);
			if(R.m_Clicked)
			{
				*pSelected = i;
				Result.m_Changed = true;
				Result.m_Clicked = true;
			}
		}
	});
	return Result;
}

SResult CXui::ColorPickerButton(const char *pKey, unsigned *pHsl)
{
	const uint32_t Id = MakeId(pKey);
	SState &St = State(Id);
	SResult R = Interact(Id);
	bool *pOpen = &St.m_Flag;
	if(R.m_Clicked)
	{
		*pOpen = !*pOpen;
		RgbToHsv(color_cast<ColorRGBA>(ColorHSLA(*pHsl)), St.m_aScratch[0], St.m_aScratch[1], St.m_aScratch[2]);
	}
	const ColorRGBA Current = color_cast<ColorRGBA>(ColorHSLA(*pHsl));
	const float Scale = St.m_Scale.Update(R.m_Pressed ? XuiTheme::Motion::PRESS_SCALE : (R.m_Hovered ? XuiTheme::Motion::SWATCH_HOVER_SCALE : 1.0f), m_Dt, XuiTheme::Motion::PRESS);
	SBox B;
	B.m_W = Px(XuiTheme::Size::SWATCH);
	B.m_H = Px(XuiTheme::Size::SWATCH);
	B.m_Hit = EHit::INTERACTIVE;
	const int Node = BoxId(Id, EDir::ROW, B);
	SetTransform(Node, Scale, 1.0f);
	Paint(Node, [Current](CXui &Ui, const SRect &Rc) {
		Ui.DrawRect(Rc.Inset(-2.0f), SolidRect(XuiTheme::Rgb(0x1C1C1E), Rc.m_H * 0.5f + 2.0f));
		Ui.DrawRect(Rc, SolidRect(Current, Rc.m_H * 0.5f));
	});

	char aPopKey[48];
	str_format(aPopKey, sizeof(aPopKey), "%s.picker", pKey);
	Popover(aPopKey, pOpen, Id, [&]() {
		static const ColorRGBA s_aPresets[] = {XuiTheme::Rgb(0xFF3B30), XuiTheme::Rgb(0xFF9F0A), XuiTheme::Rgb(0xFFD60A), XuiTheme::Rgb(0x30D158), XuiTheme::Rgb(0x0A84FF), XuiTheme::Rgb(0xFF375F), XuiTheme::Rgb(0xF2F2F7)};
		SBox Col;
		Col.m_Gap = 12.0f;
		Col.m_Padding = SEdges(6.0f);
		Column(Col, [&]() {
			float &H = St.m_aScratch[0];
			float &S = St.m_aScratch[1];
			float &V = St.m_aScratch[2];
			const uint32_t SvId = MakeId("sv");
			SResult Sv = Interact(SvId, true, false);
			SState &SvSt = State(SvId);
			bool Edited = false;
			if(Sv.m_Pressed && SvSt.m_ScreenRect.m_W > 0.0f)
			{
				S = std::clamp((m_Mouse.x - SvSt.m_ScreenRect.m_X) / SvSt.m_ScreenRect.m_W, 0.0f, 1.0f);
				V = 1.0f - std::clamp((m_Mouse.y - SvSt.m_ScreenRect.m_Y) / SvSt.m_ScreenRect.m_H, 0.0f, 1.0f);
				Edited = true;
			}
			SBox SvBox;
			SvBox.m_W = Px(204.0f);
			SvBox.m_H = Px(140.0f);
			SvBox.m_Hit = EHit::INTERACTIVE;
			const float Hue = H;
			const float Sat = S;
			const float Val = V;
			const int SvNode = BoxId(SvId, EDir::ROW, SvBox);
			Paint(SvNode, [=](CXui &Ui, const SRect &Rc) {
				IGraphics::SUiRect Base = SolidRect(HsvToRgb(Hue, 1.0f, 1.0f), 12.0f);
				Base.m_Fill = ColorRGBA(1, 1, 1, 1);
				Base.m_Fill2 = HsvToRgb(Hue, 1.0f, 1.0f);
				Base.m_GradientAngle = 0.0f;
				Ui.DrawRect(Rc, Base);
				IGraphics::SUiRect Shade = SolidRect(ColorRGBA(0, 0, 0, 0), 12.0f);
				Shade.m_Fill2 = ColorRGBA(0, 0, 0, 1);
				Shade.m_GradientAngle = 90.0f;
				Ui.DrawRect(Rc, Shade);
				const float Tx = Rc.m_X + Sat * Rc.m_W;
				const float Ty = Rc.m_Y + (1.0f - Val) * Rc.m_H;
				Ui.DrawRect({Tx - 9.0f, Ty - 9.0f, 18.0f, 18.0f}, SolidRect(ColorRGBA(1, 1, 1, 1), 9.0f));
				Ui.DrawRect({Tx - 7.0f, Ty - 7.0f, 14.0f, 14.0f}, SolidRect(HsvToRgb(Hue, Sat, Val), 7.0f));
			});

			const uint32_t HueId = MakeId("hue");
			SResult HueR = Interact(HueId, true, false);
			SState &HueSt = State(HueId);
			if(HueR.m_Pressed && HueSt.m_ScreenRect.m_W > 0.0f)
			{
				H = std::clamp((m_Mouse.x - HueSt.m_ScreenRect.m_X) / HueSt.m_ScreenRect.m_W, 0.0f, 0.999f);
				Edited = true;
			}
			SBox HueBox;
			HueBox.m_W = Px(204.0f);
			HueBox.m_H = Px(14.0f);
			HueBox.m_Hit = EHit::INTERACTIVE;
			const int HueNode = BoxId(HueId, EDir::ROW, HueBox);
			Paint(HueNode, [Hue](CXui &Ui, const SRect &Rc) {
				const float SegW = Rc.m_W / 6.0f;
				for(int i = 0; i < 6; i++)
				{
					IGraphics::SUiRect Seg = SolidRect(HsvToRgb(i / 6.0f, 1.0f, 1.0f), 0.0f);
					Seg.m_Fill2 = HsvToRgb((i + 1) / 6.0f, 1.0f, 1.0f);
					if(i == 5)
						Seg.m_Fill2 = HsvToRgb(0.0f, 1.0f, 1.0f);
					Seg.m_GradientAngle = 0.0f;
					const float Cap = Rc.m_H * 0.5f;
					Seg.m_Radii = vec4(i == 0 ? Cap : 0.0f, i == 5 ? Cap : 0.0f, i == 5 ? Cap : 0.0f, i == 0 ? Cap : 0.0f);
					Ui.DrawRect({Rc.m_X + SegW * i - (i > 0 ? 0.5f : 0.0f), Rc.m_Y, SegW + (i > 0 ? 0.5f : 0.0f), Rc.m_H}, Seg);
				}
				const float Tx = Rc.m_X + Hue * Rc.m_W;
				Ui.DrawRect({Tx - 9.0f, Rc.CenterY() - 9.0f, 18.0f, 18.0f}, SolidRect(ColorRGBA(1, 1, 1, 1), 9.0f));
				Ui.DrawRect({Tx - 7.0f, Rc.CenterY() - 7.0f, 14.0f, 14.0f}, SolidRect(HsvToRgb(Hue, 1.0f, 1.0f), 7.0f));
			});

			int Selected = -1;
			SwatchRow("presets", s_aPresets, 7, &Selected);
			if(Selected >= 0)
			{
				RgbToHsv(s_aPresets[Selected], H, S, V);
				Edited = true;
			}
			if(Edited)
				*pHsl = color_cast<ColorHSLA>(HsvToRgb(H, S, V)).Pack(false);
		});
	});
	R.m_Changed = false;
	return R;
}
