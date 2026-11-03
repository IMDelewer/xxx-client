#include <game/client/xui/xui.h>

#include <algorithm>
#include <cmath>

using namespace Xui;

SResult CXui::Button(const char *pLabel, const SButtonStyle &Style)
{
	const uint32_t Id = MakeId(Style.m_pKey ? Style.m_pKey : pLabel);
	SResult R = Interact(Id, !Style.m_Disabled);
	SState &St = State(Id);
	const bool Hovered = R.m_Hovered || Style.m_Force == 1 || Style.m_Force == 2;
	const bool Pressed = R.m_Pressed || Style.m_Force == 2;
	const bool Focused = (R.m_Focused && m_FocusVisible) || Style.m_Force == 3;
	const float Hover = St.m_Hover.Update(Hovered ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
	const float Scale = St.m_Scale.Update(Pressed ? XuiTheme::Motion::PRESS_SCALE : (Hovered ? XuiTheme::Motion::HOVER_SCALE : 1.0f), m_Dt, XuiTheme::Motion::PRESS);
	const float Focus = St.m_Focus.Update(Focused ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);

	SBox B;
	B.m_Gap = XuiTheme::Size::CAP_GAP;
	B.m_Padding = SEdges(0.0f, Style.m_PadX);
	B.m_W = Style.m_W;
	B.m_H = Px(Style.m_Height);
	B.m_Main = EAlign::CENTER;
	B.m_Cross = EAlign::CENTER;
	B.m_Hit = Style.m_Disabled ? EHit::BLOCK : EHit::INTERACTIVE;
	const EVariant Variant = Style.m_Variant;
	const ColorRGBA Accent = m_Accent;
	const ColorRGBA Ink = Variant == EVariant::ACCENT ? m_OnAccent : (m_LightTheme ? XuiTheme::Home::TEXT : XuiTheme::Color::TEXT);
	const int Node = BoxId(Id, EDir::ROW, B, [&]() {
		if(Style.m_Loading)
			Spinner(Style.m_FontSize + 3.0f, Ink);
		else if(Style.m_pIcon != nullptr)
			Icon(Style.m_pIcon, Style.m_FontSize + 3.0f, Ink, Style.m_IconFill);
		if(pLabel != nullptr && pLabel[0] != '\0')
		{
			STextStyle Label;
			Label.m_Size = Style.m_FontSize;
			Label.m_Font = Style.m_Font;
			Label.m_Color = Ink;
			Label.m_PreserveColor = Variant == EVariant::ACCENT;
			Text(pLabel, Label);
		}
	});
	SetTransform(Node, Scale, Style.m_Disabled ? 0.4f : 1.0f);
	Paint(Node, [Variant, Hover, Focus, Accent](CXui &Ui, const SRect &Rc) {
		const float Radius = Rc.m_H * 0.5f;
		IGraphics::SUiRect Fill;
		switch(Variant)
		{
		case EVariant::ACCENT:
			Fill = Ui.LightTheme() ? SolidRect(Accent, Radius) : AccentRect(Accent, Radius);
			break;
		case EVariant::GLASS:
			Fill = HomePanel(Radius);
			break;
		case EVariant::PLAIN:
			Fill = SolidRect(WithAlpha(ColorRGBA(1, 1, 1, 1), 0.1f * Hover), Radius);
			break;
		default:
			Fill = SolidRect(Mix(XuiTheme::Color::GHOST, XuiTheme::Color::GHOST_HOVER, Hover), Radius);
			break;
		}
		if(Focus > 0.01f)
		{
			IGraphics::SUiRect Ring{};
			Ring.m_Radii = Rad(Radius + XuiTheme::Size::FOCUS_RING);
			Ring.m_BorderWidth = XuiTheme::Size::FOCUS_RING;
			Ring.m_Border = WithAlpha(Accent, 0.6f * Focus);
			Ui.DrawRect(Rc.Inset(-XuiTheme::Size::FOCUS_RING), Ring);
		}
		Ui.DrawRect(Rc, Fill);
		if(Variant == EVariant::GLASS)
			Ui.DrawRect(Rc, GlassHairline(Radius));
	});
	R.m_Hovered = Hovered;
	R.m_Pressed = Pressed;
	return R;
}

SResult CXui::IconButton(const char *pIcon, const SButtonStyle &Style)
{
	const uint32_t Id = MakeId(Style.m_pKey ? Style.m_pKey : pIcon);
	SResult R = Interact(Id, !Style.m_Disabled);
	SState &St = State(Id);
	const bool Focused = R.m_Focused && m_FocusVisible;
	const float Hover = St.m_Hover.Update(R.m_Hovered ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
	const float Scale = St.m_Scale.Update(R.m_Pressed ? XuiTheme::Motion::PRESS_SCALE : (R.m_Hovered ? XuiTheme::Motion::HOVER_SCALE : 1.0f), m_Dt, XuiTheme::Motion::PRESS);
	const float Focus = St.m_Focus.Update(Focused ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
	const float Size = Style.m_Height == XuiTheme::Size::CONTROL_H ? XuiTheme::Size::ICON_BUTTON : Style.m_Height;

	SBox B;
	B.m_W = Px(Size);
	B.m_H = Px(Size);
	B.m_Main = EAlign::CENTER;
	B.m_Cross = EAlign::CENTER;
	B.m_Hit = Style.m_Disabled ? EHit::BLOCK : EHit::INTERACTIVE;
	const EVariant Variant = Style.m_Variant;
	const ColorRGBA Accent = m_Accent;
	const int Node = BoxId(Id, EDir::ROW, B, [&]() {
		Icon(pIcon, Size * 0.5f + 2.0f, Variant == EVariant::ACCENT ? m_OnAccent : XuiTheme::Color::TEXT, Style.m_IconFill);
	});
	SetTransform(Node, Scale, Style.m_Disabled ? 0.4f : 1.0f);
	Paint(Node, [Variant, Hover, Focus, Accent](CXui &Ui, const SRect &Rc) {
		const float Radius = Rc.m_H * 0.5f;
		if(Focus > 0.01f)
		{
			IGraphics::SUiRect Ring{};
			Ring.m_Radii = Rad(Radius + XuiTheme::Size::FOCUS_RING);
			Ring.m_BorderWidth = XuiTheme::Size::FOCUS_RING;
			Ring.m_Border = WithAlpha(Accent, 0.6f * Focus);
			Ui.DrawRect(Rc.Inset(-XuiTheme::Size::FOCUS_RING), Ring);
		}
		if(Variant == EVariant::GHOST)
			Ui.DrawRect(Rc, SolidRect(Mix(XuiTheme::Color::GHOST, XuiTheme::Color::GHOST_HOVER, Hover), Radius));
		else if(Variant == EVariant::ACCENT)
			Ui.DrawRect(Rc, AccentRect(Accent, Radius));
		else if(Variant == EVariant::GLASS)
		{
			Ui.DrawRect(Rc, HomePanel(Radius));
			Ui.DrawRect(Rc, GlassHairline(Radius));
		}
		else if(Hover > 0.01f)
			Ui.DrawRect(Rc, SolidRect(WithAlpha(XuiTheme::Color::ICON_BUTTON_HOVER, XuiTheme::Color::ICON_BUTTON_HOVER.a * Hover), Radius));
	});
	return R;
}

SResult CXui::StarButton(const char *pIcon, bool Filled, const char *pKey)
{
	const uint32_t Id = MakeId(pKey ? pKey : pIcon);
	SResult R = Interact(Id);
	SState &St = State(Id);
	const float Scale = St.m_Scale.Update(R.m_Pressed ? XuiTheme::Motion::PRESS_SCALE : (R.m_Hovered ? XuiTheme::Motion::STAR_HOVER_SCALE : 1.0f), m_Dt, XuiTheme::Motion::PRESS);
	SBox B;
	B.m_W = Px(XuiTheme::Size::ICON_BUTTON);
	B.m_H = Px(XuiTheme::Size::ICON_BUTTON);
	B.m_Main = EAlign::CENTER;
	B.m_Cross = EAlign::CENTER;
	B.m_Hit = EHit::INTERACTIVE;
	const int Node = BoxId(Id, EDir::ROW, B, [&]() {
		Icon(pIcon, 16.0f, Filled ? XuiTheme::Color::YELLOW : XuiTheme::Color::TEXT, Filled);
	});
	SetTransform(Node, Scale, 1.0f);
	return R;
}
