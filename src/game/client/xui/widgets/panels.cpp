#include <engine/client.h>
#include <engine/shared/config.h>

#include <game/client/xui/xui.h>

#include <algorithm>
#include <cmath>

using namespace Xui;

ColorRGBA CXui::Mix(ColorRGBA A, ColorRGBA B, float T)
{
	return ColorRGBA(A.r + (B.r - A.r) * T, A.g + (B.g - A.g) * T, A.b + (B.b - A.b) * T, A.a + (B.a - A.a) * T);
}

ColorRGBA CXui::WithAlpha(ColorRGBA C, float Alpha)
{
	C.a = Alpha;
	return C;
}

IGraphics::SUiRect CXui::HomePanel(float Radius)
{
	IGraphics::SUiRect R = GlassRect(Radius, 1.0f - g_Config.m_XxUiTransparency / 100.0f);
	R.m_BorderWidth = 1.0f;
	R.m_Border = XuiTheme::Home::BORDER;
	return R;
}

IGraphics::SUiRect CXui::GlassRect(float Radius, float PanelAlpha)
{
	IGraphics::SUiRect R{};
	R.m_Radii = vec4(Radius, Radius, Radius, Radius);
	if(!g_Config.m_XxUiGlass)
		PanelAlpha = 1.0f;
	if(PanelAlpha < 0.0f)
	{
		R.m_Fill = XuiTheme::Glass::FILL_TOP;
		R.m_Fill2 = XuiTheme::Glass::FILL_BOTTOM;
	}
	else
	{
		R.m_Fill = XuiTheme::Home::PANEL.WithAlpha(PanelAlpha);
		R.m_Fill2 = XuiTheme::Home::PANEL.WithAlpha(std::min(1.0f, PanelAlpha + 0.06f));
	}
	R.m_GradientAngle = 90.0f;
	R.m_BorderWidth = XuiTheme::Glass::RIM_WIDTH;
	R.m_Border = ColorRGBA(1, 1, 1, 0.16f);
	R.m_Border2 = ColorRGBA(1, 1, 1, 0.08f);
	R.m_BorderAngle = XuiTheme::Glass::RIM_ANGLE;
	R.m_Shadow = XuiTheme::Glass::SHADOW;
	R.m_ShadowOffset = vec2(0.0f, XuiTheme::Glass::SHADOW_Y);
	R.m_ShadowBlur = XuiTheme::Glass::SHADOW_BLUR;
	R.m_Glass = g_Config.m_XxUiGlass ? 1.0f : 0.0f;
	R.m_Saturation = XuiTheme::Glass::SATURATION;
	R.m_Refraction = 0.35f;
	return R;
}

IGraphics::SUiRect CXui::GlassHairline(float Radius)
{
	IGraphics::SUiRect R{};
	R.m_Radii = vec4(Radius, Radius, Radius, Radius);
	R.m_BorderWidth = 0.5f;
	R.m_Border = XuiTheme::Glass::INNER_STROKE;
	return R;
}

IGraphics::SUiRect CXui::SolidRect(ColorRGBA Color, float Radius)
{
	IGraphics::SUiRect R{};
	R.m_Radii = vec4(Radius, Radius, Radius, Radius);
	R.m_Fill = Color;
	return R;
}

IGraphics::SUiRect CXui::AccentRect(ColorRGBA Accent, float Radius)
{
	IGraphics::SUiRect R{};
	R.m_Radii = vec4(Radius, Radius, Radius, Radius);
	R.m_Fill = Mix(Accent, ColorRGBA(1, 1, 1, 1), 0.18f);
	R.m_Fill2 = Accent;
	R.m_GradientAngle = 90.0f;
	R.m_BorderWidth = 1.0f;
	R.m_Border = ColorRGBA(1, 1, 1, 0.35f);
	R.m_Border2 = ColorRGBA(1, 1, 1, 0.0f);
	R.m_BorderAngle = 90.0f;
	return R;
}

IGraphics::SUiRect CXui::GroupRect(float Radius)
{
	IGraphics::SUiRect R{};
	R.m_Radii = vec4(Radius, Radius, Radius, Radius);
	R.m_Fill = XuiTheme::Color::GROUP_FILL;
	R.m_BorderWidth = 1.0f;
	R.m_Border = XuiTheme::Color::GROUP_STROKE;
	return R;
}

int CXui::GlassPanel(const SBox &B, const FBuild &Children, float Radius, bool Dark)
{
	SBox Box = B;
	if(Box.m_Hit == EHit::NONE)
		Box.m_Hit = EHit::BLOCK;
	const int Node = Column(Box, Children);
	const bool Menu = Client()->State() == IClient::STATE_OFFLINE || Client()->State() == IClient::STATE_CONNECTING || Client()->State() == IClient::STATE_LOADING;
	Paint(Node, [Radius, Menu, Dark = Dark || Menu](CXui &Ui, const SRect &R) {
		IGraphics::SUiRect Glass = Menu || Ui.LightTheme() ? HomePanel(Radius) : GlassRect(Radius, Dark ? XuiTheme::Glass::PANEL_ALPHA : -1.0f);
		Ui.DrawRect(R, Glass);
	});
	return Node;
}

int CXui::Panel(const SBox &B, const FBuild &Children, float Radius)
{
	return GlassPanel(B, Children, Radius, true);
}

int CXui::Group(const FBuild &Rows)
{
	SBox B;
	B.m_Cross = EAlign::STRETCH;
	B.m_Clip = true;
	B.m_W = Grow();
	const int Node = Column(B, [this, &Rows]() {
		m_RowCounter = 0;
		Rows();
	});
	Paint(Node, [](CXui &Ui, const SRect &R) {
		Ui.DrawRect(R, GroupRect(XuiTheme::Radius::XL));
	});
	return Node;
}

int CXui::GroupRow(const FBuild &Content, bool First, float MinHeight)
{
	SBox B;
	B.m_Gap = XuiTheme::Size::ROW_GAP;
	B.m_Padding = SEdges(XuiTheme::Size::ROW_PAD_Y, XuiTheme::Size::ROW_PAD_X);
	B.m_H = Fit(MinHeight);
	B.m_Cross = EAlign::CENTER;
	const int Node = Row(B, Content);
	if(!First)
	{
		Paint(Node, [](CXui &Ui, const SRect &R) {
			Ui.DrawRect({R.m_X + XuiTheme::Size::SEPARATOR_INSET, R.m_Y, R.m_W - XuiTheme::Size::SEPARATOR_INSET, 1.0f}, SolidRect(XuiTheme::Color::SEPARATOR, 0.0f));
		});
	}
	return Node;
}

void CXui::SettingsRow(const char *pTitle, const char *pSubtitle, const FBuild &Control, const char *pIcon)
{
	const bool First = m_RowCounter++ == 0;
	GroupRow([&]() {
		if(pIcon != nullptr)
			Icon(pIcon, XuiTheme::Size::ICON, XuiTheme::Color::TEXT2);
		SBox Texts;
		Texts.m_Gap = 2.0f;
		Texts.m_W = Grow();
		Texts.m_Shrink = true;
		Column(Texts, [&]() {
			STextStyle Title;
			Title.m_Size = XuiTheme::Font::ROW_TITLE;
			Title.m_Font = EFont::MEDIUM;
			Text(pTitle, Title);
			if(pSubtitle != nullptr && pSubtitle[0] != '\0')
			{
				STextStyle Sub;
				Sub.m_Size = XuiTheme::Font::ROW_SUBTITLE;
				Sub.m_Color = XuiTheme::Color::TEXT3;
				Text(pSubtitle, Sub);
			}
		});
		if(Control)
			Control();
	},
		First);
}

int CXui::PageTransition(const char *pKey, const SBox &B, const FBuild &Children)
{
	const uint32_t Id = MakeId(pKey);
	const float T = Animate(Id, 1.0f, XuiTheme::Motion::PAGE, 0, 0.0f);
	const int Node = BoxId(Id, EDir::COLUMN, B, Children);
	const float Scale = XuiTheme::Motion::PAGE_FROM_SCALE + (1.0f - XuiTheme::Motion::PAGE_FROM_SCALE) * T;
	SetTransform(Node, Scale, std::clamp(T * 1.4f, 0.0f, 1.0f), 0.0f, (1.0f - T) * XuiTheme::Motion::PAGE_FROM_Y);
	return Node;
}

int CXui::Scroll(const SBox &B, const FBuild &Children)
{
	SBox Box = B;
	Box.m_Scroll = EScroll::VERTICAL;
	Box.m_Hit = EHit::BLOCK;
	return Column(Box, Children);
}

void CXui::SectionHeader(const char *pText)
{
	STextStyle Style;
	Style.m_Size = XuiTheme::Font::SECTION_HEADER;
	Style.m_Font = EFont::SEMIBOLD;
	Style.m_Color = XuiTheme::Color::TEXT3;
	SBox B;
	B.m_Padding = SEdges(6.0f, 10.0f);
	Text(pText, Style, B);
}

void CXui::PageHeader(const char *pTitle, const char *pSubtitle)
{
	SBox B;
	B.m_Gap = 4.0f;
	B.m_Padding = SEdges(0.0f, 4.0f);
	Column(B, [&]() {
		STextStyle Title;
		Title.m_Size = XuiTheme::Font::PAGE_TITLE;
		Title.m_Font = EFont::EXTRABOLD;
		Title.m_Tracking = -0.035f;
		Text(pTitle, Title);
		STextStyle Sub;
		Sub.m_Size = XuiTheme::Font::PAGE_SUBTITLE;
		Sub.m_Color = XuiTheme::Color::TEXT3;
		Text(pSubtitle, Sub);
	});
}
