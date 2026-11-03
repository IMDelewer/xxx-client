#include <game/client/xui/xui.h>

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

	IGraphics::SUiRect ActivePill(float Radius)
	{
		IGraphics::SUiRect Pill = CXui::SolidRect(XuiTheme::Color::NAV_ACTIVE, Radius);
		Pill.m_BorderWidth = 1.0f;
		Pill.m_Border = XuiTheme::Color::NAV_ACTIVE_HIGHLIGHT;
		Pill.m_Border2 = ColorRGBA(1, 1, 1, 0.0f);
		Pill.m_BorderAngle = 135.0f;
		return Pill;
	}
}

SResult CXui::NavItemImpl(const SNavItem &Item, bool Active, bool OwnBackground)
{
	const uint32_t Id = MakeId(Item.m_pLabel);
	SResult R = Interact(Id);
	SState &St = State(Id);
	const float Hover = St.m_Hover.Update(R.m_Hovered ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
	if(!St.m_Init)
	{
		St.m_Init = true;
		St.m_Press.Snap(Active ? 1.0f : 0.0f);
	}
	const float Act = St.m_Press.Update(Active ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
	const float Focus = St.m_Focus.Update(R.m_Focused && m_FocusVisible ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
	const ColorRGBA Accent = m_Accent;

	SBox B;
	B.m_Gap = XuiTheme::Size::NAV_GAP;
	B.m_Padding = SEdges(0.0f, XuiTheme::Size::NAV_PAD_X);
	B.m_H = Px(XuiTheme::Size::NAV_H);
	B.m_Cross = EAlign::CENTER;
	B.m_Hit = EHit::INTERACTIVE;
	const int Node = BoxId(Id, EDir::ROW, B, [&]() {
		SBox IconBox;
		IconBox.m_W = Px(XuiTheme::Size::NAV_ICON_BOX);
		IconBox.m_H = Px(XuiTheme::Size::NAV_ICON_BOX);
		IconBox.m_Main = EAlign::CENTER;
		IconBox.m_Cross = EAlign::CENTER;
		const bool Tile = Item.m_Tile.a > 0.0f;
		const int IconNode = Row(IconBox, [&]() {
			if(Item.m_pIcon != nullptr)
			{
				if(Tile)
					Icon(Item.m_pIcon, XuiTheme::Size::NAV_ICON_SIZE - 2.0f, ColorRGBA(1, 1, 1, 1), false);
				else
					Icon(Item.m_pIcon, XuiTheme::Size::NAV_ICON_SIZE, Mix(XuiTheme::Color::NAV_ICON_IDLE, Accent, Act), false);
			}
		});
		if(Tile)
		{
			const ColorRGBA TileColor = Item.m_Tile;
			Paint(IconNode, [TileColor](CXui &Ui, const SRect &Rc) {
				IGraphics::SUiRect R = SolidRect(Mix(TileColor, ColorRGBA(1, 1, 1, 1), 0.12f), 9.0f);
				R.m_Fill2 = TileColor;
				R.m_GradientAngle = 90.0f;
				Ui.DrawRect(Rc, R);
			});
		}
		STextStyle Label;
		Label.m_Size = XuiTheme::Font::NAV;
		Label.m_Font = EFont::MEDIUM;
		Label.m_Wrap = true;
		Label.m_MaxLines = 1;
		SBox LabelBox;
		LabelBox.m_W = Grow();
		LabelBox.m_Shrink = true;
		Text(Item.m_pLabel, Label, LabelBox);
		if(Item.m_pCount != nullptr)
		{
			STextStyle Count;
			Count.m_Size = XuiTheme::Font::CAPTION;
			Count.m_Color = XuiTheme::Color::TEXT3;
			Count.m_Tabular = true;
			Text(Item.m_pCount, Count);
		}
	});
	Paint(Node, [=](CXui &Ui, const SRect &Rc) {
		const float Radius = Rc.m_H * 0.5f;
		FocusRing(Ui, Rc, Radius, Focus, Accent);
		if(Hover > 0.01f)
			Ui.DrawRect(Rc, SolidRect(WithAlpha(XuiTheme::Color::NAV_HOVER, XuiTheme::Color::NAV_HOVER.a * Hover * (OwnBackground ? 1.0f - Act : 1.0f)), Radius));
		if(OwnBackground && Act > 0.01f)
		{
			IGraphics::SUiRect Pill = ActivePill(Radius);
			Pill.m_Fill.a *= Act;
			Pill.m_Border.a *= Act;
			Ui.DrawRect(Rc, Pill);
		}
	});
	return R;
}

SResult CXui::NavItem(const SNavItem &Item, bool Active)
{
	return NavItemImpl(Item, Active, true);
}

void CXui::Sidebar(const char *pKey, const SNavItem *pItems, int Count, int *pSelected, const char *pHeader)
{
	const uint32_t Id = MakeId(pKey);
	std::vector<uint32_t> vIds(Count, 0);
	SBox B;
	B.m_Gap = 2.0f;
	B.m_W = Grow();
	B.m_Cross = EAlign::STRETCH;
	const int Node = BoxId(Id, EDir::COLUMN, B, [&]() {
		if(pHeader != nullptr)
			SectionHeader(pHeader);
		bool First = pHeader == nullptr;
		for(int i = 0; i < Count; i++)
		{
			if(pItems[i].m_Header)
			{
				STextStyle Style;
				Style.m_Size = XuiTheme::Font::SECTION_HEADER;
				Style.m_Font = EFont::SEMIBOLD;
				Style.m_Color = XuiTheme::Color::TEXT3;
				SBox HB;
				HB.m_Padding = SEdges(First ? 6.0f : 14.0f, 10.0f, 6.0f, 10.0f);
				Row(HB, [&]() { Text(pItems[i].m_pLabel, Style); });
				First = false;
				continue;
			}
			First = false;
			const SResult R = NavItemImpl(pItems[i], *pSelected == i, false);
			vIds[i] = R.m_Id;
			if(R.m_Clicked)
				*pSelected = i;
		}
	});
	const int Selected = *pSelected;
	Paint(Node, [this, Id, vIds, Selected](CXui &Ui, const SRect &) {
		if(Selected < 0 || Selected >= (int)vIds.size() || vIds[Selected] == 0)
			return;
		auto It = m_States.find(vIds[Selected]);
		if(It == m_States.end() || It->second.m_Rect.m_W <= 0.0f)
			return;
		const SRect Item = It->second.m_Rect;
		const float Y = Ui.Animate(Id, Item.m_Y, XuiTheme::Motion::INDICATOR, 1);
		const float H = Ui.Animate(Id, Item.m_H, XuiTheme::Motion::INDICATOR, 2);
		const float X = Ui.Animate(Id, Item.m_X, XuiTheme::Motion::INDICATOR, 3);
		const float W = Ui.Animate(Id, Item.m_W, XuiTheme::Motion::INDICATOR, 4);
		const float Vel = m_States[Id].m_aAnim[1].m_Spring.m_Velocity;
		const float Stretch = std::clamp(std::abs(Vel) / 500.0f, 0.0f, 1.0f) * XuiTheme::Size::SEGMENT_STRETCH * H;
		Ui.DrawRect({X, Y - (Vel < 0.0f ? Stretch : 0.0f), W, H + Stretch}, ActivePill(XuiTheme::Size::NAV_H * 0.5f));
	});
}

int CXui::Dock(const char *pKey, const SDockItem *pItems, int Count, int *pSelected)
{
	int Clicked = -1;
	const uint32_t Id = MakeId(pKey);
	std::vector<uint32_t> vIds(Count, 0);
	const ColorRGBA Accent = m_Accent;

	SBox Wrap;
	Wrap.m_Position = EPos::ABSOLUTE;
	Wrap.m_Left = 0.0f;
	Wrap.m_Right = 0.0f;
	Wrap.m_Bottom = XuiTheme::Size::DOCK_BOTTOM;
	Wrap.m_Main = EAlign::CENTER;
	BoxId(Id, EDir::ROW, Wrap, [&]() {
		SBox B;
		B.m_Gap = XuiTheme::Size::DOCK_GAP;
		B.m_Padding = SEdges(XuiTheme::Size::DOCK_PAD);
		B.m_H = Px(XuiTheme::Size::DOCK_H);
		B.m_Cross = EAlign::CENTER;
		B.m_Hit = EHit::BLOCK;
		B.m_pKey = "bar";
		const uint32_t BarId = MakeId("bar");
		const int Bar = BoxId(BarId, EDir::ROW, B, [&]() {
			for(int i = 0; i < Count; i++)
			{
				const uint32_t ItemId = MakeId(pItems[i].m_pLabel);
				vIds[i] = ItemId;
				SResult R = Interact(ItemId);
				SState &St = State(ItemId);
				const bool Selected = *pSelected == i;
				if(!St.m_Init)
				{
					St.m_Init = true;
					St.m_Hover.Snap(Selected ? 1.0f : 0.0f);
				}
				const float Sel = St.m_Hover.Update(Selected ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
				const float IconScale = St.m_Scale.Update(R.m_Pressed ? XuiTheme::Motion::PRESS_SCALE : (R.m_Hovered ? XuiTheme::Motion::ICON_HOVER_SCALE : 1.0f), m_Dt, XuiTheme::Motion::PRESS);
				const float Focus = St.m_Focus.Update(R.m_Focused && m_FocusVisible ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
				if(R.m_Clicked)
				{
					Clicked = i;
					if(pItems[i].m_Selectable)
						*pSelected = i;
				}
				SBox ItemBox;
				ItemBox.m_W = Px(XuiTheme::Size::DOCK_ITEM_W);
				ItemBox.m_H = Px(XuiTheme::Size::DOCK_ITEM_H);
				ItemBox.m_Gap = 3.0f;
				ItemBox.m_Main = EAlign::CENTER;
				ItemBox.m_Cross = EAlign::CENTER;
				ItemBox.m_Hit = EHit::INTERACTIVE;
				const ColorRGBA TextColor = Mix(m_LightTheme ? XuiTheme::Home::TEXT.WithAlpha(0.72f) : XuiTheme::Color::DOCK_TEXT_IDLE, m_OnAccent, Sel);
				const int ItemNode = BoxId(ItemId, EDir::COLUMN, ItemBox, [&]() {
					const int IconNode = Icon(pItems[i].m_pIcon, 22.0f, TextColor, Selected);
					SetTransform(IconNode, IconScale, 1.0f);
					STextStyle Label;
					Label.m_Size = XuiTheme::Font::DOCK;
					Label.m_Font = EFont::SEMIBOLD;
					Label.m_Color = TextColor;
					Label.m_PreserveColor = true;
					Text(pItems[i].m_pLabel, Label);
				});
				Paint(ItemNode, [Focus, Accent](CXui &Ui, const SRect &Rc) {
					FocusRing(Ui, Rc, XuiTheme::Size::DOCK_ITEM_H * 0.5f, Focus, Accent);
				});
			}
		});
		const int Selected = *pSelected;
		Paint(Bar, [this, Id, vIds, Selected, Accent](CXui &Ui, const SRect &Rc) {
			Ui.DrawRect(Rc, HomePanel(XuiTheme::Size::DOCK_H * 0.5f));
			Ui.DrawRect(Rc, GlassHairline(XuiTheme::Size::DOCK_H * 0.5f));
			if(Selected < 0 || Selected >= (int)vIds.size())
				return;
			auto It = m_States.find(vIds[Selected]);
			if(It == m_States.end() || It->second.m_Rect.m_W <= 0.0f)
				return;
			const SRect Item = It->second.m_Rect;
			const float X = Ui.Animate(Id, Item.m_X, XuiTheme::Motion::INDICATOR, 1);
			const float Vel = m_States[Id].m_aAnim[1].m_Spring.m_Velocity;
			const float Stretch = std::clamp(std::abs(Vel) / 900.0f, 0.0f, 1.0f) * XuiTheme::Size::SEGMENT_STRETCH * Item.m_W;
			IGraphics::SUiRect Pill = AccentRect(Accent, XuiTheme::Size::DOCK_ITEM_H * 0.5f);
			Pill.m_ShadowBlur = 18.0f;
			Ui.DrawRect({X - (Vel < 0.0f ? Stretch : 0.0f), Item.m_Y, Item.m_W + Stretch, Item.m_H}, Pill);
		});
	});
	return Clicked;
}
