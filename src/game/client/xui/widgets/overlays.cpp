#include <base/str.h>

#include <engine/font_icons_phosphor.h>

#include <game/client/xui/xui.h>

#include <algorithm>
#include <cmath>

using namespace Xui;

void CXui::OverlayPanel(const char *pKey, bool *pOpen, const SRect &Anchor, float Gap, SSize W, float Pad, const FBuild &Content)
{
	const uint32_t Id = MakeId(pKey);
	const float P = Presence(Id, *pOpen, XuiTheme::Motion::POP);
	if(P < 0.003f && !*pOpen)
		return;
	RegisterOverlay(Id, pOpen);
	const SRect Prev = State(Id).m_Rect;
	const vec2 Screen = m_ScreenUnits;
	Layer(ELayer::POPOVER, [&]() {
		const uint32_t ScrimId = MakeId("scrim");
		const SResult Scrim = Interact(ScrimId, *pOpen, false);
		if(Scrim.m_Clicked || Scrim.m_RightClicked)
			*pOpen = false;
		SBox ScrimBox;
		ScrimBox.m_Position = EPos::ABSOLUTE;
		ScrimBox.m_Left = ScrimBox.m_Top = ScrimBox.m_Right = ScrimBox.m_Bottom = 0.0f;
		ScrimBox.m_Hit = *pOpen ? EHit::INTERACTIVE : EHit::NONE;
		BoxId(ScrimId, EDir::ROW, ScrimBox);

		const float Margin = 12.0f;
		float X = Anchor.m_X;
		float Y = Anchor.Bottom() + Gap;
		if(Prev.m_W > 0.0f)
		{
			if(X + Prev.m_W > Screen.x - Margin)
				X = std::max(Margin, Screen.x - Margin - Prev.m_W);
			if(Y + Prev.m_H > Screen.y - Margin)
				Y = std::max(Margin, Anchor.m_Y - Gap - Prev.m_H);
		}
		SBox Panel;
		Panel.m_Position = EPos::ABSOLUTE;
		Panel.m_Left = X;
		Panel.m_Top = Y;
		Panel.m_W = W;
		Panel.m_Padding = SEdges(Pad);
		Panel.m_Hit = EHit::BLOCK;
		const int Node = BoxId(Id, EDir::COLUMN, Panel, Content);
		SetTransform(Node, XuiTheme::Motion::POP_FROM + (1.0f - XuiTheme::Motion::POP_FROM) * P, std::clamp(P * 1.5f, 0.0f, 1.0f));
		Paint(Node, [](CXui &Ui, const SRect &Rc) {
			IGraphics::SUiRect Glass = GlassRect(XuiTheme::Radius::LG);
			Glass.m_Fill = XuiTheme::Color::MENU_FILL;
			Glass.m_Fill2 = XuiTheme::Color::MENU_FILL;
			if(Ui.LightTheme())
				Glass = HomePanel(XuiTheme::Radius::LG);
			Ui.DrawRect(Rc, Glass);
			Ui.DrawRect(Rc, GlassHairline(XuiTheme::Radius::LG));
		});
	});
}

void CXui::Popover(const char *pKey, bool *pOpen, uint32_t AnchorId, const FBuild &Content)
{
	OverlayPanel(pKey, pOpen, AnchorRect(AnchorId), XuiTheme::Size::POPOVER_GAP, Fit(), XuiTheme::Size::POPOVER_PAD, Content);
}

void CXui::ContextMenu(const char *pKey, const char *pTitle, const SMenuItem *pItems, int Count, bool *pOpen, vec2 At, int *pChosen)
{
	STextStyle Measure;
	Measure.m_Size = XuiTheme::Font::CTX_ITEM;
	Measure.m_Font = EFont::MEDIUM;
	float Width = XuiTheme::Size::MENU_W;
	for(int i = 0; i < Count; ++i)
	{
		if(pItems[i].m_Separator)
			continue;
		const float IconWidth = pItems[i].m_pIcon ? 17.0f + XuiTheme::Size::CTX_ITEM_GAP : 0.0f;
		Width = std::max(Width, TextWidth(pItems[i].m_pLabel, Measure) + IconWidth + 2.0f * (XuiTheme::Size::CTX_ITEM_PAD_X + XuiTheme::Size::CTX_PAD));
	}
	Width = std::min(Width, m_ScreenUnits.x - 24.0f);
	OverlayPanel(pKey, pOpen, {At.x, At.y, 0.0f, 0.0f}, 0.0f, Px(Width), XuiTheme::Size::CTX_PAD, [&]() {
		if(pTitle != nullptr)
		{
			STextStyle Title;
			Title.m_Size = 11.5f;
			Title.m_Font = EFont::SEMIBOLD;
			Title.m_Color = XuiTheme::Color::TEXT3;
			Title.m_Wrap = true;
			Title.m_MaxLines = 2;
			SBox TitleBox;
			TitleBox.m_Padding = SEdges(6.0f, 12.0f, 4.0f, 12.0f);
			TitleBox.m_W = Grow();
			Row(TitleBox, [&]() { Text(pTitle, Title, {.m_W = Grow()}); });
		}
		for(int i = 0; i < Count; i++)
		{
			const SMenuItem &Item = pItems[i];
			if(Item.m_Separator)
			{
				SBox Sep;
				Sep.m_H = Px(9.0f);
				Sep.m_W = Grow();
				const int SepNode = Row(Sep);
				Paint(SepNode, [](CXui &Ui, const SRect &Rc) {
					Ui.DrawRect({Rc.m_X + 8.0f, Rc.CenterY(), Rc.m_W - 16.0f, 1.0f}, SolidRect(XuiTheme::Color::SEPARATOR, 0.0f));
				});
				continue;
			}
			const uint32_t ItemId = MakeId(Item.m_pLabel);
			SResult R = Interact(ItemId, !Item.m_Disabled);
			SState &St = State(ItemId);
			const float Hover = St.m_Hover.Update(R.m_Hovered ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
			const float Focus = St.m_Focus.Update(R.m_Focused && m_FocusVisible ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
			if(R.m_Clicked)
			{
				*pChosen = i;
				*pOpen = false;
			}
			SBox ItemBox;
			ItemBox.m_W = Grow();
			ItemBox.m_H = Fit(XuiTheme::Size::MENU_ITEM_H);
			ItemBox.m_Padding = SEdges(6.0f, XuiTheme::Size::CTX_ITEM_PAD_X);
			ItemBox.m_Gap = XuiTheme::Size::CTX_ITEM_GAP;
			ItemBox.m_Cross = EAlign::CENTER;
			ItemBox.m_Hit = Item.m_Disabled ? EHit::BLOCK : EHit::INTERACTIVE;
			const ColorRGBA TextColor = Item.m_Danger ? XuiTheme::Color::RED_SOFT : XuiTheme::Color::TEXT;
			const int ItemNode = BoxId(ItemId, EDir::ROW, ItemBox, [&]() {
				if(Item.m_pIcon != nullptr)
					Icon(Item.m_pIcon, 17.0f, TextColor);
				STextStyle Label;
				Label.m_Size = XuiTheme::Font::CTX_ITEM;
				Label.m_Font = EFont::MEDIUM;
				Label.m_Color = TextColor;
				Label.m_Wrap = true;
				SBox LabelBox;
				LabelBox.m_W = Grow();
				LabelBox.m_Shrink = true;
				Text(Item.m_pLabel, Label, LabelBox);
			});
			SetTransform(ItemNode, 1.0f, Item.m_Disabled ? 0.4f : 1.0f);
			const ColorRGBA Accent = m_Accent;
			Paint(ItemNode, [Hover, Focus, Accent](CXui &Ui, const SRect &Rc) {
				if(Focus > 0.01f)
				{
					IGraphics::SUiRect Ring{};
					Ring.m_Radii = vec4(XuiTheme::Radius::SM, XuiTheme::Radius::SM, XuiTheme::Radius::SM, XuiTheme::Radius::SM);
					Ring.m_BorderWidth = XuiTheme::Size::FOCUS_RING;
					Ring.m_Border = WithAlpha(Accent, 0.6f * Focus);
					Ui.DrawRect(Rc, Ring);
				}
				if(Hover > 0.01f)
					Ui.DrawRect(Rc, SolidRect(WithAlpha(XuiTheme::Color::MENU_ITEM_HOVER, XuiTheme::Color::MENU_ITEM_HOVER.a * Hover), XuiTheme::Radius::SM));
			});
		}
	});
}

void CXui::Modal(const char *pKey, bool *pOpen, const FBuild &Content)
{
	const uint32_t Id = MakeId(pKey);
	const float P = Presence(Id, *pOpen, XuiTheme::Motion::POP);
	if(P < 0.003f && !*pOpen)
		return;
	RegisterOverlay(Id, pOpen);
	Layer(ELayer::MODAL, [&]() {
		const uint32_t ScrimId = MakeId("scrim");
		const SResult Scrim = Interact(ScrimId, *pOpen, false);
		if(Scrim.m_Clicked)
			*pOpen = false;
		SBox ScrimBox;
		ScrimBox.m_Position = EPos::ABSOLUTE;
		ScrimBox.m_Left = ScrimBox.m_Top = ScrimBox.m_Right = ScrimBox.m_Bottom = 0.0f;
		ScrimBox.m_Hit = *pOpen ? EHit::INTERACTIVE : EHit::BLOCK;
		const int ScrimNode = BoxId(ScrimId, EDir::ROW, ScrimBox);
		Paint(ScrimNode, [P](CXui &Ui, const SRect &Rc) {
			Ui.DrawRect(Rc, SolidRect(WithAlpha(XuiTheme::Color::SCRIM, XuiTheme::Color::SCRIM.a * std::clamp(P, 0.0f, 1.0f)), 0.0f));
		});

		SBox Wrap;
		Wrap.m_Position = EPos::ABSOLUTE;
		Wrap.m_Left = Wrap.m_Top = Wrap.m_Right = Wrap.m_Bottom = 0.0f;
		Wrap.m_Main = EAlign::CENTER;
		Wrap.m_Cross = EAlign::CENTER;
		Row(Wrap, [&]() {
			SBox Panel;
			Panel.m_W = Px(XuiTheme::Size::MODAL_W);
			Panel.m_Padding = SEdges(24.0f);
			Panel.m_Gap = 16.0f;
			Panel.m_Hit = EHit::BLOCK;
			const int Node = BoxId(Id, EDir::COLUMN, Panel, Content);
			SetTransform(Node, XuiTheme::Motion::POP_FROM + (1.0f - XuiTheme::Motion::POP_FROM) * P, std::clamp(P * 1.5f, 0.0f, 1.0f));
			Paint(Node, [](CXui &Ui, const SRect &Rc) {
				Ui.DrawRect(Rc, GlassRect(XuiTheme::Radius::XXL, XuiTheme::Glass::PANEL_ALPHA));
				Ui.DrawRect(Rc, GlassHairline(XuiTheme::Radius::XXL));
			});
		});
	});
}

void CXui::ShowToast(const char *pTitle, const char *pIcon, ColorRGBA IconColor, float Seconds)
{
	static uint32_t s_Counter = 0;
	SToast Toast;
	Toast.m_Title = pTitle;
	Toast.m_Icon = pIcon ? pIcon : Ph::CHECK_CIRCLE;
	Toast.m_IconColor = IconColor;
	Toast.m_Life = Seconds;
	Toast.m_Id = ++s_Counter;
	m_vToasts.push_back(std::move(Toast));
}

void CXui::RenderToasts()
{
	if(m_vToasts.empty())
		return;
	for(SToast &Toast : m_vToasts)
		Toast.m_Age += m_Dt;
	m_vToasts.erase(std::remove_if(m_vToasts.begin(), m_vToasts.end(), [](const SToast &T) { return T.m_Age > T.m_Life + 0.6f; }), m_vToasts.end());
	Layer(ELayer::TOAST, [&]() {
		SBox Stack;
		Stack.m_Position = EPos::ABSOLUTE;
		Stack.m_Left = 0.0f;
		Stack.m_Right = 0.0f;
		Stack.m_Bottom = XuiTheme::Size::TOAST_BOTTOM;
		Stack.m_Gap = 8.0f;
		Stack.m_Cross = EAlign::CENTER;
		Column(Stack, [&]() {
			for(const SToast &Toast : m_vToasts)
			{
				char aKey[24];
				str_format(aKey, sizeof(aKey), "toast%u", Toast.m_Id);
				const uint32_t Id = MakeId(aKey);
				const float P = Presence(Id, Toast.m_Age < Toast.m_Life, XuiTheme::Motion::POP);
				SBox B;
				B.m_H = Px(XuiTheme::Size::TOAST_H);
				B.m_Padding = SEdges(0.0f, 22.0f, 0.0f, 20.0f);
				B.m_Gap = 10.0f;
				B.m_Cross = EAlign::CENTER;
				B.m_Flip = true;
				const int Node = BoxId(Id, EDir::ROW, B, [&]() {
					Icon(Toast.m_Icon.c_str(), 20.0f, Toast.m_IconColor, true);
					STextStyle Title;
					Title.m_Size = XuiTheme::Font::NAV;
					Title.m_Font = EFont::SEMIBOLD;
					Text(Toast.m_Title.c_str(), Title);
				});
				SetTransform(Node, 0.92f + 0.08f * P, std::clamp(P * 1.5f, 0.0f, 1.0f), 0.0f, (1.0f - P) * 18.0f);
				Paint(Node, [](CXui &Ui, const SRect &Rc) {
					Ui.DrawRect(Rc, GlassRect(Rc.m_H * 0.5f, XuiTheme::Glass::PANEL_ALPHA));
					Ui.DrawRect(Rc, GlassHairline(Rc.m_H * 0.5f));
				});
			}
		});
	});
}

void CXui::Tooltip(uint32_t AnchorId, const char *pText)
{
	SState &St = State(AnchorId);
	if(m_HoverId == AnchorId && !m_MouseDown)
		St.m_aScratch[3] += m_Dt;
	else
		St.m_aScratch[3] = 0.0f;
	const uint32_t Id = AnchorId * 31u + 7u;
	const float P = Animate(Id, St.m_aScratch[3] > 0.45f ? 1.0f : 0.0f, XuiTheme::Motion::POP, 0, 0.0f);
	if(P < 0.01f)
		return;
	const SRect Anchor = AnchorRect(AnchorId);
	STextStyle Label;
	Label.m_Size = 12.5f;
	Label.m_Font = EFont::MEDIUM;
	Label.m_Wrap = true;
	const float TextW = TextWidth(pText, Label);
	const float W = std::min(TextW + 20.0f, std::min(XuiTheme::Size::TOOLTIP_MAX_W, m_ScreenUnits.x - 16.0f));
	const float H = Label.m_Size * XuiTheme::Font::LINE_HEIGHT * std::ceil(TextW / std::max(1.0f, W - 20.0f)) + 12.0f;
	float X = std::clamp(Anchor.CenterX() - W * 0.5f, 8.0f, m_ScreenUnits.x - W - 8.0f);
	float Y = Anchor.m_Y - H - 8.0f;
	if(Y < 8.0f)
		Y = Anchor.Bottom() + 8.0f;
	Layer(ELayer::TOOLTIP, [&]() {
		SBox B;
		B.m_Position = EPos::ABSOLUTE;
		B.m_Left = X;
		B.m_Top = Y;
		B.m_Padding = SEdges(6.0f, 10.0f);
		B.m_W = Px(W);
		const int Node = BoxId(MakeId("tip"), EDir::ROW, B, [&]() {
			SBox TextBox;
			TextBox.m_W = Grow();
			Text(pText, Label, TextBox);
		});
		SetTransform(Node, 0.94f + 0.06f * P, std::clamp(P * 1.5f, 0.0f, 1.0f), 0.0f, (1.0f - P) * 4.0f);
		Paint(Node, [](CXui &Ui, const SRect &Rc) {
			IGraphics::SUiRect Fill = SolidRect(XuiTheme::Color::TOOLTIP_FILL, 10.0f);
			Fill.m_Shadow = ColorRGBA(0, 0, 0, 0.35f);
			Fill.m_ShadowOffset = vec2(0.0f, 6.0f);
			Fill.m_ShadowBlur = 20.0f;
			Ui.DrawRect(Rc, Fill);
			Ui.DrawRect(Rc, GlassHairline(10.0f));
		});
	});
}
