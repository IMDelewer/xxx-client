#include <base/str.h>

#include <engine/keys.h>

#include <game/client/gameclient.h>
#include <game/client/xui/xui.h>

#include <algorithm>
#include <cmath>

using namespace Xui;

namespace
{
	void DrawArc(CXui &Ui, const SRect &R, float StartDeg, float SweepDeg, float Stroke, ColorRGBA Color)
	{
		const float Radius = std::min(R.m_W, R.m_H) * 0.5f - Stroke * 0.5f;
		if(Radius <= 0.0f || SweepDeg <= 0.0f)
			return;
		const float Length = Radius * SweepDeg * pi / 180.0f;
		const int Dots = std::clamp((int)std::ceil(Length / (Stroke * 0.35f)), 2, 220);
		for(int i = 0; i <= Dots; i++)
		{
			const float Angle = (StartDeg + SweepDeg * i / Dots) * pi / 180.0f;
			const float X = R.CenterX() + std::cos(Angle) * Radius;
			const float Y = R.CenterY() + std::sin(Angle) * Radius;
			Ui.DrawRect({X - Stroke * 0.5f, Y - Stroke * 0.5f, Stroke, Stroke}, CXui::SolidRect(Color, Stroke * 0.5f));
		}
	}

	void DrawRingTrack(CXui &Ui, const SRect &R, float Stroke, ColorRGBA Color)
	{
		IGraphics::SUiRect Ring{};
		const float Radius = std::min(R.m_W, R.m_H) * 0.5f;
		Ring.m_Radii = vec4(Radius, Radius, Radius, Radius);
		Ring.m_BorderWidth = Stroke;
		Ring.m_Border = Color;
		Ui.DrawRect(R, Ring);
	}
}

void CXui::Badge(const char *pText, ColorRGBA Color)
{
	SBox B;
	B.m_H = Px(XuiTheme::Size::BADGE_H);
	B.m_Padding = SEdges(0.0f, 8.0f);
	B.m_Main = EAlign::CENTER;
	B.m_Cross = EAlign::CENTER;
	const int Node = Row(B, [&]() {
		STextStyle Label;
		Label.m_Size = 11.0f;
		Label.m_Font = EFont::BOLD;
		Label.m_Color = Color;
		Label.m_Tabular = true;
		Text(pText, Label);
	});
	Paint(Node, [Color](CXui &Ui, const SRect &Rc) {
		Ui.DrawRect(Rc, SolidRect(WithAlpha(Color, 0.18f), Rc.m_H * 0.5f));
	});
}

SResult CXui::Chip(const char *pLabel, bool *pSelected, const char *pIcon)
{
	const uint32_t Id = MakeId(pLabel);
	SResult R = Interact(Id);
	SState &St = State(Id);
	if(R.m_Clicked)
	{
		*pSelected = !*pSelected;
		R.m_Changed = true;
	}
	if(!St.m_Init)
	{
		St.m_Init = true;
		St.m_Focus.Snap(*pSelected ? 1.0f : 0.0f);
	}
	const float Hover = St.m_Hover.Update(R.m_Hovered ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
	const float Sel = St.m_Focus.Update(*pSelected ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
	const float Scale = St.m_Scale.Update(R.m_Pressed ? XuiTheme::Motion::PRESS_SCALE : (R.m_Hovered ? XuiTheme::Motion::HOVER_SCALE : 1.0f), m_Dt, XuiTheme::Motion::PRESS);
	const ColorRGBA Accent = m_Accent;
	SBox B;
	B.m_H = Px(XuiTheme::Size::CHIP_H);
	B.m_Padding = SEdges(0.0f, 12.0f);
	B.m_Gap = 6.0f;
	B.m_Main = EAlign::CENTER;
	B.m_Cross = EAlign::CENTER;
	B.m_Hit = EHit::INTERACTIVE;
	const int Node = BoxId(Id, EDir::ROW, B, [&]() {
		if(pIcon != nullptr)
			Icon(pIcon, 14.0f, Mix(XuiTheme::Color::TEXT_IDLE, XuiTheme::Color::TEXT, Sel), *pSelected);
		STextStyle Label;
		Label.m_Size = 12.5f;
		Label.m_Font = EFont::SEMIBOLD;
		Label.m_Color = Mix(XuiTheme::Color::TEXT_IDLE, XuiTheme::Color::TEXT, Sel);
		Text(pLabel, Label);
	});
	SetTransform(Node, Scale, 1.0f);
	Paint(Node, [=](CXui &Ui, const SRect &Rc) {
		const float Radius = Rc.m_H * 0.5f;
		Ui.DrawRect(Rc, SolidRect(Mix(XuiTheme::Color::GHOST, XuiTheme::Color::GHOST_HOVER, Hover), Radius));
		if(Sel > 0.01f)
		{
			IGraphics::SUiRect Tint = SolidRect(WithAlpha(Accent, 0.28f * Sel), Radius);
			Tint.m_BorderWidth = 1.0f;
			Tint.m_Border = WithAlpha(Accent, 0.55f * Sel);
			Ui.DrawRect(Rc, Tint);
		}
	});
	return R;
}

void CXui::ProgressBar(const char *pKey, float Value, float Width)
{
	const uint32_t Id = MakeId(pKey);
	const float Shown = Animate(Id, std::clamp(Value, 0.0f, 1.0f), XuiTheme::Motion::HOVER, 0, 0.0f);
	const ColorRGBA Accent = m_Accent;
	SBox B;
	B.m_W = Px(Width);
	B.m_H = Px(XuiTheme::Size::PROGRESS_H);
	const int Node = BoxId(Id, EDir::ROW, B);
	Paint(Node, [Shown, Accent](CXui &Ui, const SRect &Rc) {
		Ui.DrawRect(Rc, SolidRect(XuiTheme::Color::TRACK, Rc.m_H * 0.5f));
		if(Shown > 0.001f)
			Ui.DrawRect({Rc.m_X, Rc.m_Y, std::max(Rc.m_H, Rc.m_W * Shown), Rc.m_H}, SolidRect(Accent, Rc.m_H * 0.5f));
	});
}

void CXui::Spinner(float Size, ColorRGBA Color)
{
	SBox B;
	B.m_W = Px(Size);
	B.m_H = Px(Size);
	const ColorRGBA Accent = m_Accent;
	const int Node = Row(B);
	const float Time = m_Time;
	Paint(Node, [Time, Color, Accent](CXui &Ui, const SRect &Rc) {
		const float Stroke = std::max(2.0f, Rc.m_W * 0.1f);
		DrawRingTrack(Ui, Rc, Stroke, WithAlpha(Color, 0.14f));
		DrawArc(Ui, Rc, std::fmod(Time, 1.0f) * 360.0f, 95.0f, Stroke, WithAlpha(Color, 1.0f));
	});
}

void CXui::RingProgress(float Value, float Size, const char *pLabel)
{
	SBox B;
	B.m_W = Px(Size);
	B.m_H = Px(Size);
	B.m_Main = EAlign::CENTER;
	B.m_Cross = EAlign::CENTER;
	const ColorRGBA Accent = m_Accent;
	const uint32_t Id = MakeId(pLabel ? pLabel : "ring");
	const float Shown = Animate(Id, std::clamp(Value, 0.0f, 1.0f), XuiTheme::Motion::HOVER, 0, 0.0f);
	const int Node = BoxId(Id, EDir::ROW, B, [&]() {
		if(pLabel != nullptr)
		{
			STextStyle Label;
			Label.m_Size = 13.0f;
			Label.m_Font = EFont::BOLD;
			Label.m_Tabular = true;
			Text(pLabel, Label);
		}
	});
	Paint(Node, [Shown, Accent](CXui &Ui, const SRect &Rc) {
		const float Stroke = XuiTheme::Size::RING_STROKE * Rc.m_W / 44.0f;
		DrawRingTrack(Ui, Rc, Stroke, ColorRGBA(1, 1, 1, 0.12f));
		DrawArc(Ui, Rc, -90.0f, 360.0f * Shown, Stroke, Accent);
	});
}

void CXui::Skeleton(float W, float H, float Radius)
{
	SBox B;
	B.m_W = Px(W);
	B.m_H = Px(H);
	B.m_Clip = true;
	const int Node = Row(B);
	const float Time = m_Time;
	Paint(Node, [Radius](CXui &Ui, const SRect &Rc) {
		Ui.DrawRect(Rc, SolidRect(XuiTheme::Color::SKELETON, Radius));
	});
	PaintOver(Node, [Time](CXui &Ui, const SRect &Rc) {
		const float Phase = std::fmod(Time * 0.7f, 1.6f) - 0.3f;
		const float BandW = Rc.m_W * 0.5f;
		const float X = Rc.m_X + Phase * Rc.m_W - BandW * 0.5f;
		IGraphics::SUiRect Left = CXui::SolidRect(ColorRGBA(1, 1, 1, 0), 0.0f);
		Left.m_Fill2 = XuiTheme::Color::SKELETON_SHINE;
		Left.m_GradientAngle = 0.0f;
		Ui.DrawRect({X, Rc.m_Y, BandW * 0.5f, Rc.m_H}, Left);
		IGraphics::SUiRect Right = CXui::SolidRect(XuiTheme::Color::SKELETON_SHINE, 0.0f);
		Right.m_Fill2 = ColorRGBA(1, 1, 1, 0);
		Right.m_GradientAngle = 0.0f;
		Ui.DrawRect({X + BandW * 0.5f, Rc.m_Y, BandW * 0.5f, Rc.m_H}, Right);
	});
}

void CXui::TeeAvatar(const char *pSkin, float Size, int Emote)
{
	CTeeRenderInfo Info;
	const CSkin *pSkinData = GameClient()->m_Skins.Find(pSkin);
	if(pSkinData != nullptr)
		Info.Apply(pSkinData);
	SBox B;
	B.m_W = Px(Size);
	B.m_H = Px(Size);
	const int Node = Row(B);
	Paint(Node, [Info, Emote](CXui &Ui, const SRect &Rc) {
		Ui.DrawTee(Info, Emote, vec2(1.0f, 0.0f), Rc);
	});
}

void CXui::VirtualList(const char *pKey, int Count, float RowHeight, const SBox &B, const std::function<void(int)> &RowFn, int *pClicked, const std::function<bool(int)> &Selected, int *pActivated, int *pRightClicked, float RowRadius)
{
	const uint32_t Id = MakeId(pKey);
	SState &St = State(Id);
	const float ViewH = St.m_Rect.m_H > 0.0f ? St.m_Rect.m_H : m_ScreenUnits.y;
	const int First = std::clamp((int)std::floor(St.m_ScrollY / RowHeight) - 2, 0, std::max(0, Count - 1));
	const int Last = std::min(Count, (int)std::ceil((St.m_ScrollY + ViewH) / RowHeight) + 2);
	SBox Box = B;
	Box.m_Scroll = EScroll::VERTICAL;
	Box.m_Hit = EHit::BLOCK;
	BoxId(Id, EDir::COLUMN, Box, [&]() {
		SBox Content;
		Content.m_H = Px(Count * RowHeight);
		Content.m_W = Grow();
		Column(Content, [&]() {
			for(int i = First; i < Last; i++)
			{
				char aKey[16];
				str_format(aKey, sizeof(aKey), "r%d", i);
				const uint32_t RowId = MakeId(aKey);
				const SResult R = Interact(RowId, true, false);
				SState &RowState = State(RowId);
				const float Hover = RowState.m_Hover.Update(R.m_Hovered ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
				if(R.m_Clicked)
				{
					if(pClicked != nullptr)
						*pClicked = i;
					if(pActivated != nullptr && St.m_LastClickRow == i && m_Time - St.m_LastClickTime < 0.4f)
						*pActivated = i;
					St.m_LastClickRow = i;
					St.m_LastClickTime = m_Time;
				}
				if(R.m_RightClicked && pRightClicked != nullptr)
					*pRightClicked = i;
				const bool IsSelected = Selected && Selected(i);
				if(pActivated && m_FocusId == RowId && (KeyPressed(KEY_RETURN) || KeyPressed(KEY_KP_ENTER)))
					*pActivated = i;
				SBox RowBox;
				RowBox.m_Position = EPos::ABSOLUTE;
				RowBox.m_Top = i * RowHeight;
				RowBox.m_Left = 0.0f;
				RowBox.m_Right = 0.0f;
				RowBox.m_H = Px(RowHeight);
				RowBox.m_Padding = SEdges(0.0f, 16.0f);
				RowBox.m_Gap = 12.0f;
				RowBox.m_Cross = EAlign::CENTER;
				RowBox.m_Hit = EHit::INTERACTIVE;
				const int RowNode = BoxId(RowId, EDir::ROW, RowBox, [&]() { RowFn(i); });
				Paint(RowNode, [Hover, IsSelected, RowRadius](CXui &Ui, const SRect &Rc) {
					if(IsSelected)
						Ui.DrawRect(Rc, SolidRect(ColorRGBA(1, 1, 1, 0.1f), RowRadius));
					else if(Hover > 0.01f)
						Ui.DrawRect(Rc, SolidRect(WithAlpha(XuiTheme::Color::ROW_HOVER, XuiTheme::Color::ROW_HOVER.a * Hover), RowRadius));
				});
			}
		});
	});
}
