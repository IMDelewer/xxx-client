#include "xui.h"

#include <algorithm>
#include <cmath>

using namespace Xui;
using namespace XuiTheme;

CXui::SPrefSize CXui::Pref(int N)
{
	SXuiNode &Node = m_vNodes[N];
	if(Node.m_PrefW >= 0.0f)
		return {Node.m_PrefW, Node.m_PrefH};

	float W = 0.0f;
	float H = 0.0f;
	if(Node.m_IsText)
	{
		const STextStyle &Style = Node.m_TextStyle;
		SText *pText = GetText(Node.m_Text.c_str(), Style.m_Size * m_Scale, Style.m_Font, -1.0f, false, Style.m_Tabular, Style.m_MaxLines, Style.m_Tracking);
		W = pText->m_W / m_Scale;
		H = pText->m_Lines * Style.m_Size * Font::LINE_HEIGHT;
	}
	else
	{
		const bool Row = Node.m_Dir == EDir::ROW;
		int Count = 0;
		float Main = 0.0f;
		float Cross = 0.0f;
		for(int C = Node.m_First; C >= 0; C = m_vNodes[C].m_Next)
		{
			if(m_vNodes[C].m_Box.m_Position == EPos::ABSOLUTE)
				continue;
			const SPrefSize P = Pref(C);
			Main += Row ? P.m_W : P.m_H;
			Cross = std::max(Cross, Row ? P.m_H : P.m_W);
			Count++;
		}
		Main += Node.m_Box.m_Gap * std::max(0, Count - 1);
		W = Node.m_Box.m_Padding.X() + (Row ? Main : Cross);
		H = Node.m_Box.m_Padding.Y() + (Row ? Cross : Main);
	}
	const SBox &B = Node.m_Box;
	if(B.m_W.m_Kind == SSize::EKind::FIXED)
		W = B.m_W.m_Value;
	if(B.m_H.m_Kind == SSize::EKind::FIXED)
		H = B.m_H.m_Value;
	W = std::clamp(W, B.m_W.m_Min, B.m_W.m_Max);
	H = std::clamp(H, B.m_H.m_Min, B.m_H.m_Max);
	Node.m_PrefW = W;
	Node.m_PrefH = H;
	return {W, H};
}

void CXui::LayoutText(SXuiNode &Node, float W, float Fh, float MaxW)
{
	const STextStyle &Style = Node.m_TextStyle;
	const SPrefSize P = Pref((int)(&Node - m_vNodes.data()));
	int Lines = 1;
	if(Style.m_Wrap && W + 0.5f < P.m_W)
	{
		SText *pText = GetText(Node.m_Text.c_str(), Style.m_Size * m_Scale, Style.m_Font, W * m_Scale, true, Style.m_Tabular, Style.m_MaxLines, Style.m_Tracking);
		Lines = pText->m_Lines;
	}
	else
		Lines = std::max(1, (int)std::round(P.m_H / (Style.m_Size * Font::LINE_HEIGHT)));
	Node.m_W = W;
	Node.m_H = Fh >= 0.0f ? Fh : Lines * Style.m_Size * Font::LINE_HEIGHT;
}

void CXui::Layout(int N, float Fw, float Fh, float MaxW, float MaxH)
{
	SXuiNode &Node = m_vNodes[N];
	const SBox &B = Node.m_Box;
	const SPrefSize P = Pref(N);

	float W;
	if(Fw >= 0.0f)
		W = Fw;
	else if(B.m_W.m_Kind == SSize::EKind::FIXED)
		W = B.m_W.m_Value;
	else if(B.m_W.m_Kind == SSize::EKind::PERCENT && MaxW < INF * 0.5f)
		W = B.m_W.m_Value / 100.0f * MaxW;
	else
		W = std::min(P.m_W, MaxW);
	W = std::clamp(W, B.m_W.m_Min, B.m_W.m_Max);

	float H = 0.0f;
	bool HKnown = true;
	if(Fh >= 0.0f)
		H = Fh;
	else if(B.m_H.m_Kind == SSize::EKind::FIXED)
		H = B.m_H.m_Value;
	else if(B.m_H.m_Kind == SSize::EKind::PERCENT && MaxH < INF * 0.5f)
		H = B.m_H.m_Value / 100.0f * MaxH;
	else
		HKnown = false;
	if(HKnown)
		H = std::clamp(H, B.m_H.m_Min, B.m_H.m_Max);

	if(Node.m_IsText)
	{
		LayoutText(Node, W, HKnown ? H : -1.0f, MaxW);
		if(!HKnown)
			Node.m_H = std::clamp(Node.m_H, B.m_H.m_Min, B.m_H.m_Max);
		return;
	}

	const bool Row = Node.m_Dir == EDir::ROW;
	const float PadX = B.m_Padding.X();
	const float PadY = B.m_Padding.Y();
	const float InnerW = std::max(0.0f, W - PadX);
	const float InnerH = HKnown ? std::max(0.0f, H - PadY) : -1.0f;
	const bool ScrollMain = (B.m_Scroll == EScroll::VERTICAL && !Row) || (B.m_Scroll == EScroll::HORIZONTAL && Row);
	const bool ScrollCross = (B.m_Scroll == EScroll::VERTICAL && Row) || (B.m_Scroll == EScroll::HORIZONTAL && !Row);
	const bool MainKnown = !ScrollMain && (Row ? true : HKnown);
	const bool CrossKnown = !ScrollCross && (Row ? HKnown : true);
	const float InnerMain = Row ? InnerW : InnerH;
	const float InnerCross = Row ? InnerH : InnerW;
	const float MaxCrossForChild = CrossKnown ? InnerCross : INF;

	std::vector<int> vFlow;
	std::vector<int> vAbs;
	for(int C = Node.m_First; C >= 0; C = m_vNodes[C].m_Next)
		(m_vNodes[C].m_Box.m_Position == EPos::ABSOLUTE ? vAbs : vFlow).push_back(C);

	struct SItem
	{
		int m_Node;
		float m_Basis;
		float m_Final;
		float m_CrossForce;
		float m_CrossW;
		bool m_Laid;
		float m_Grow;
		bool m_Shrinkable;
		EAlign m_Align;
	};
	std::vector<SItem> vItems;
	vItems.reserve(vFlow.size());

	for(int C : vFlow)
	{
		const SXuiNode &Child = m_vNodes[C];
		const SSize &MS = Row ? Child.m_Box.m_W : Child.m_Box.m_H;
		const SSize &CS = Row ? Child.m_Box.m_H : Child.m_Box.m_W;
		SItem Item{};
		Item.m_Node = C;
		Item.m_Align = Child.m_Box.m_Self == EAlign::AUTO ? B.m_Cross : Child.m_Box.m_Self;
		Item.m_CrossForce = -1.0f;
		if(CS.m_Kind == SSize::EKind::FIXED)
			Item.m_CrossForce = std::clamp(CS.m_Value, CS.m_Min, CS.m_Max);
		else if(CS.m_Kind == SSize::EKind::PERCENT && CrossKnown)
			Item.m_CrossForce = std::clamp(CS.m_Value / 100.0f * InnerCross, CS.m_Min, CS.m_Max);
		else if(CrossKnown && (Item.m_Align == EAlign::STRETCH || CS.m_Kind == SSize::EKind::GROW))
			Item.m_CrossForce = std::clamp(InnerCross, CS.m_Min, CS.m_Max);
		Item.m_Grow = MS.m_Kind == SSize::EKind::GROW ? MS.m_Value : 0.0f;
		Item.m_Shrinkable = Child.m_IsText || Child.m_Box.m_Shrink || MS.m_Kind == SSize::EKind::GROW;

		if(MS.m_Kind == SSize::EKind::FIXED)
			Item.m_Basis = std::clamp(MS.m_Value, MS.m_Min, MS.m_Max);
		else if(MS.m_Kind == SSize::EKind::PERCENT && MainKnown)
			Item.m_Basis = std::clamp(MS.m_Value / 100.0f * InnerMain, MS.m_Min, MS.m_Max);
		else if(MS.m_Kind == SSize::EKind::GROW && MainKnown)
			Item.m_Basis = MS.m_Min;
		else if(Row)
			Item.m_Basis = std::clamp(Pref(C).m_W, MS.m_Min, MS.m_Max);
		else
		{
			const float Cw = Item.m_CrossForce >= 0.0f ? Item.m_CrossForce : std::min(Pref(C).m_W, InnerW);
			Item.m_CrossW = Cw;
			Layout(C, Cw, -1.0f, InnerW, INF);
			Item.m_Basis = m_vNodes[C].m_H;
			Item.m_Laid = true;
		}
		Item.m_Final = Item.m_Basis;
		if(!Row && !Item.m_Laid)
			Item.m_CrossW = Item.m_CrossForce;
		vItems.push_back(Item);
	}

	struct SLine
	{
		int m_Begin, m_End;
		float m_Cross = 0.0f;
		float m_Main = 0.0f;
	};
	std::vector<SLine> vLines;
	if(Row && B.m_Wrap && MainKnown)
	{
		int Begin = 0;
		float Sum = 0.0f;
		for(int i = 0; i < (int)vItems.size(); i++)
		{
			const float Need = vItems[i].m_Basis + (i > Begin ? B.m_Gap : 0.0f);
			if(i > Begin && Sum + Need > InnerMain + 0.01f)
			{
				vLines.push_back({Begin, i});
				Begin = i;
				Sum = vItems[i].m_Basis;
			}
			else
				Sum += Need;
		}
		vLines.push_back({Begin, (int)vItems.size()});
	}
	else
		vLines.push_back({0, (int)vItems.size()});

	const bool DoGrow = MainKnown;
	for(SLine &Line : vLines)
	{
		const int Count = Line.m_End - Line.m_Begin;
		if(Count <= 0)
			continue;
		float Total = B.m_Gap * (Count - 1);
		float GrowSum = 0.0f;
		for(int i = Line.m_Begin; i < Line.m_End; i++)
		{
			Total += vItems[i].m_Basis;
			GrowSum += vItems[i].m_Grow;
		}
		if(DoGrow)
		{
			const float Free = InnerMain - Total;
			if(Free > 0.01f && GrowSum > 0.0f)
			{
				for(int i = Line.m_Begin; i < Line.m_End; i++)
				{
					if(vItems[i].m_Grow <= 0.0f)
						continue;
					const SSize &MS = Row ? m_vNodes[vItems[i].m_Node].m_Box.m_W : m_vNodes[vItems[i].m_Node].m_Box.m_H;
					vItems[i].m_Final = std::clamp(vItems[i].m_Basis + Free * vItems[i].m_Grow / GrowSum, MS.m_Min, MS.m_Max);
				}
			}
			else if(Free < -0.01f)
			{
				float Deficit = -Free;
				for(int Pass = 0; Pass < 3 && Deficit > 0.01f; Pass++)
				{
					float Weight = 0.0f;
					for(int i = Line.m_Begin; i < Line.m_End; i++)
					{
						const SSize &MS = Row ? m_vNodes[vItems[i].m_Node].m_Box.m_W : m_vNodes[vItems[i].m_Node].m_Box.m_H;
						if(vItems[i].m_Shrinkable && vItems[i].m_Final > MS.m_Min + 0.01f)
							Weight += vItems[i].m_Final;
					}
					if(Weight <= 0.0f)
						break;
					float Taken = 0.0f;
					for(int i = Line.m_Begin; i < Line.m_End; i++)
					{
						const SSize &MS = Row ? m_vNodes[vItems[i].m_Node].m_Box.m_W : m_vNodes[vItems[i].m_Node].m_Box.m_H;
						if(!vItems[i].m_Shrinkable || vItems[i].m_Final <= MS.m_Min + 0.01f)
							continue;
						const float Want = Deficit * vItems[i].m_Final / Weight;
						const float NewFinal = std::max(MS.m_Min, vItems[i].m_Final - Want);
						Taken += vItems[i].m_Final - NewFinal;
						vItems[i].m_Final = NewFinal;
					}
					Deficit -= Taken;
				}
			}
		}

		for(int i = Line.m_Begin; i < Line.m_End; i++)
		{
			SItem &Item = vItems[i];
			SXuiNode &Child = m_vNodes[Item.m_Node];
			if(Row)
				Layout(Item.m_Node, Item.m_Final, Item.m_CrossForce, Item.m_Final, MaxCrossForChild);
			else if(!Item.m_Laid || std::abs(Item.m_Final - Item.m_Basis) > 0.01f || Child.m_Box.m_H.m_Kind == SSize::EKind::GROW)
				Layout(Item.m_Node, Item.m_CrossW, Item.m_Final, InnerW, Item.m_Final);
			Line.m_Main += Item.m_Final;
			Line.m_Cross = std::max(Line.m_Cross, Row ? Child.m_H : Child.m_W);
		}
		Line.m_Main += B.m_Gap * (Count - 1);
	}

	float ContentMain = 0.0f;
	float ContentCross = 0.0f;
	for(size_t l = 0; l < vLines.size(); l++)
	{
		ContentMain = std::max(ContentMain, vLines[l].m_Main);
		ContentCross += vLines[l].m_Cross + (l > 0 ? B.m_Gap : 0.0f);
	}

	if(!HKnown)
	{
		H = PadY + (Row ? ContentCross : ContentMain);
		H = std::clamp(H, B.m_H.m_Min, B.m_H.m_Max);
	}
	const float FinalInnerMain = Row ? InnerW : std::max(0.0f, H - PadY);
	const float FinalInnerCross = Row ? std::max(0.0f, H - PadY) : InnerW;

	Node.m_W = W;
	Node.m_H = H;
	Node.m_ContentW = PadX + (Row ? ContentMain : ContentCross);
	Node.m_ContentH = PadY + (Row ? ContentCross : ContentMain);

	float CrossCursor = 0.0f;
	for(size_t l = 0; l < vLines.size(); l++)
	{
		const SLine &Line = vLines[l];
		const int Count = Line.m_End - Line.m_Begin;
		if(Count <= 0)
			continue;
		const float Left = ScrollMain ? 0.0f : std::max(0.0f, FinalInnerMain - Line.m_Main);
		float Start = 0.0f;
		float Spacing = B.m_Gap;
		switch(B.m_Main)
		{
		case EAlign::CENTER: Start = Left * 0.5f; break;
		case EAlign::END: Start = Left; break;
		case EAlign::BETWEEN:
			if(Count > 1)
				Spacing += Left / (Count - 1);
			break;
		default: break;
		}
		const float Space = (vLines.size() == 1 && !ScrollCross) ? FinalInnerCross : Line.m_Cross;
		float Cursor = Start;
		for(int i = Line.m_Begin; i < Line.m_End; i++)
		{
			SItem &Item = vItems[i];
			SXuiNode &Child = m_vNodes[Item.m_Node];
			const float CrossSize = Row ? Child.m_H : Child.m_W;
			float CrossOffset = 0.0f;
			if(Item.m_Align == EAlign::CENTER)
				CrossOffset = (Space - CrossSize) * 0.5f;
			else if(Item.m_Align == EAlign::END)
				CrossOffset = Space - CrossSize;
			const float MainPos = Cursor;
			const float CrossPos = CrossCursor + CrossOffset;
			if(Row)
			{
				Child.m_X = B.m_Padding.m_L + MainPos;
				Child.m_Y = B.m_Padding.m_T + CrossPos;
			}
			else
			{
				Child.m_X = B.m_Padding.m_L + CrossPos;
				Child.m_Y = B.m_Padding.m_T + MainPos;
			}
			Cursor += Item.m_Final + Spacing;
		}
		CrossCursor += Line.m_Cross + B.m_Gap;
	}

	for(int C : vAbs)
	{
		SXuiNode &Child = m_vNodes[C];
		const SBox &CB = Child.m_Box;
		float Cw = -1.0f;
		float Ch = -1.0f;
		if(CB.m_Left != UNSET && CB.m_Right != UNSET)
			Cw = std::max(0.0f, W - CB.m_Left - CB.m_Right);
		if(CB.m_Top != UNSET && CB.m_Bottom != UNSET)
			Ch = std::max(0.0f, H - CB.m_Top - CB.m_Bottom);
		Layout(C, Cw, Ch, W, H);
		Child.m_X = CB.m_Left != UNSET ? CB.m_Left : (CB.m_Right != UNSET ? W - CB.m_Right - Child.m_W : 0.0f);
		Child.m_Y = CB.m_Top != UNSET ? CB.m_Top : (CB.m_Bottom != UNSET ? H - CB.m_Bottom - Child.m_H : 0.0f);
	}
}

void CXui::LayoutAll()
{
	for(int L = 0; L < (int)ELayer::NUM; L++)
	{
		for(int Root : m_avLayerRoots[L])
			Layout(Root, m_ScreenUnits.x, m_ScreenUnits.y, m_ScreenUnits.x, m_ScreenUnits.y);
	}
	m_vOrder.clear();
	for(int L = 0; L < (int)ELayer::NUM; L++)
	{
		for(int Root : m_avLayerRoots[L])
		{
			SXf Base;
			Base.m_Sx = Base.m_Sy = m_Scale;
			Place(Root, Base, 0.0f, 0.0f, 1.0f, false, SRect());
		}
	}
}

void CXui::Place(int N, const SXf &Parent, float Px, float Py, float PAlpha, bool PClipped, const SRect &PClip)
{
	SXuiNode &Node = m_vNodes[N];
	m_vOrder.push_back(N);
	Node.m_Abs = {Px + Node.m_X, Py + Node.m_Y, Node.m_W, Node.m_H};
	Node.m_AlphaAcc = PAlpha * Node.m_Alpha;

	SXf Xf = Parent;
	if(Node.m_Box.m_Flip)
	{
		SState &St = State(Node.m_Id);
		if(!St.m_FlipInit)
		{
			St.m_FlipX.Snap(Node.m_X);
			St.m_FlipY.Snap(Node.m_Y);
			St.m_FlipW.Snap(Node.m_W);
			St.m_FlipH.Snap(Node.m_H);
			St.m_FlipInit = true;
		}
		const float Fx = St.m_FlipX.Update(Node.m_X, m_Dt, Motion::LAYOUT);
		const float Fy = St.m_FlipY.Update(Node.m_Y, m_Dt, Motion::LAYOUT);
		const float Fw = St.m_FlipW.Update(Node.m_W, m_Dt, Motion::LAYOUT);
		const float Fh = St.m_FlipH.Update(Node.m_H, m_Dt, Motion::LAYOUT);
		if(!(St.m_FlipX.Resting() && St.m_FlipY.Resting() && St.m_FlipW.Resting() && St.m_FlipH.Resting()))
		{
			const float Sx = Node.m_W > 0.5f ? Fw / Node.m_W : 1.0f;
			const float Sy = Node.m_H > 0.5f ? Fh / Node.m_H : 1.0f;
			SXf Flip;
			Flip.m_Sx = Sx;
			Flip.m_Sy = Sy;
			Flip.m_Tx = Node.m_Abs.m_X * (1.0f - Sx) + (Fx - Node.m_X);
			Flip.m_Ty = Node.m_Abs.m_Y * (1.0f - Sy) + (Fy - Node.m_Y);
			Xf = Xf.Then(Flip);
		}
	}
	if(Node.m_Scale != 1.0f || Node.m_OffX != 0.0f || Node.m_OffY != 0.0f)
	{
		SXf Local;
		Local.m_Sx = Local.m_Sy = Node.m_Scale;
		Local.m_Tx = Node.m_Abs.CenterX() * (1.0f - Node.m_Scale) + Node.m_OffX;
		Local.m_Ty = Node.m_Abs.CenterY() * (1.0f - Node.m_Scale) + Node.m_OffY;
		Xf = Xf.Then(Local);
	}
	Node.m_Xf = Xf;
	const vec2 P0 = Xf.Apply(vec2(Node.m_Abs.m_X, Node.m_Abs.m_Y));
	Node.m_Screen = {P0.x, P0.y, Node.m_Abs.m_W * Xf.m_Sx, Node.m_Abs.m_H * Xf.m_Sy};

	const bool Clips = Node.m_Box.m_Clip || Node.m_Box.m_Scroll != EScroll::NONE;
	Node.m_Clipped = Clips || PClipped;
	Node.m_ClipPx = PClip;
	if(Clips)
	{
		if(PClipped)
		{
			const float X0 = std::max(PClip.m_X, Node.m_Screen.m_X);
			const float Y0 = std::max(PClip.m_Y, Node.m_Screen.m_Y);
			const float X1 = std::min(PClip.Right(), Node.m_Screen.Right());
			const float Y1 = std::min(PClip.Bottom(), Node.m_Screen.Bottom());
			Node.m_ClipPx = {X0, Y0, std::max(0.0f, X1 - X0), std::max(0.0f, Y1 - Y0)};
		}
		else
			Node.m_ClipPx = Node.m_Screen;
	}
	else if(!PClipped)
		Node.m_ClipPx = SRect();

	auto It = m_States.find(Node.m_Id);
	if(It != m_States.end())
	{
		It->second.m_Rect = Node.m_Abs;
		It->second.m_ScreenRect = Node.m_Screen;
	}

	float ScrollX = 0.0f;
	float ScrollY = 0.0f;
	if(Node.m_Box.m_Scroll != EScroll::NONE)
	{
		SState &St = State(Node.m_Id);
		const bool Vertical = Node.m_Box.m_Scroll == EScroll::VERTICAL;
		float &Pos = Vertical ? St.m_ScrollY : St.m_ScrollX;
		float &Vel = Vertical ? St.m_VelY : St.m_VelX;
		const float Max = std::max(0.0f, Vertical ? Node.m_ContentH - Node.m_H : Node.m_ContentW - Node.m_W);
		if(Node.m_Id == m_ScrollTarget && m_Wheel != 0.0f)
			Vel -= m_Wheel * Motion::WHEEL_IMPULSE;
		bool Dragging = false;
		if(Vertical && Max > 0.0f && Node.m_Screen.m_H > 0.0f)
		{
			const float Track = Node.m_H - 8.0f;
			const float ThumbH = std::max(28.0f, Track * Node.m_H / Node.m_ContentH);
			const float ThumbTop = 4.0f + (Track - ThumbH) * std::clamp(Pos / Max, 0.0f, 1.0f);
			const float LocalY = (m_Mouse.y - Node.m_Screen.m_Y) * Node.m_H / Node.m_Screen.m_H;
			if(m_MouseDown && !m_PrevMouseDown && m_ScrollbarHover == Node.m_Id)
			{
				m_ScrollDrag = Node.m_Id;
				m_ScrollDragOffset = LocalY >= ThumbTop && LocalY <= ThumbTop + ThumbH ? LocalY - ThumbTop : ThumbH * 0.5f;
			}
			if(m_ScrollDrag == Node.m_Id)
			{
				if(!m_MouseDown)
					m_ScrollDrag = 0;
				else if(Track - ThumbH > 0.0f)
				{
					Pos = std::clamp((LocalY - m_ScrollDragOffset - 4.0f) / (Track - ThumbH), 0.0f, 1.0f) * Max;
					Vel = 0.0f;
					Dragging = true;
				}
			}
		}
		Pos += Vel * m_Dt;
		Vel *= std::exp(-Motion::SCROLL_FRICTION * m_Dt);
		if(std::abs(Vel) < 2.0f)
			Vel = 0.0f;
		if(Pos < 0.0f || Pos > Max)
		{
			Pos = std::clamp(Pos, 0.0f, Max);
			Vel = 0.0f;
		}
		const float Active = (std::abs(Vel) > 4.0f || Node.m_Id == m_ScrollTarget || Dragging) ? 1.0f : 0.0f;
		St.m_aScratch[0] += (Active - St.m_aScratch[0]) * std::min(1.0f, m_Dt * (Active > 0.0f ? 14.0f : 3.0f));
		Node.m_ScrollX = St.m_ScrollX;
		Node.m_ScrollY = St.m_ScrollY;
		ScrollX = Node.m_ScrollX;
		ScrollY = Node.m_ScrollY;
	}

	for(int C = Node.m_First; C >= 0; C = m_vNodes[C].m_Next)
		Place(C, Node.m_Xf, Node.m_Abs.m_X - ScrollX, Node.m_Abs.m_Y - ScrollY, Node.m_AlphaAcc, Node.m_Clipped, Node.m_ClipPx);
}

void CXui::HitTest()
{
	m_HoverId = 0;
	m_HitNode = -1;
	m_ScrollTarget = 0;
	const vec2 P = m_Mouse;
	for(int i = (int)m_vOrder.size() - 1; i >= 0; i--)
	{
		const SXuiNode &Node = m_vNodes[m_vOrder[i]];
		if((Node.m_Box.m_Hit == EHit::NONE && Node.m_Box.m_Scroll == EScroll::NONE) || Node.m_AlphaAcc < 0.05f)
			continue;
		if(!Node.m_Screen.Contains(P))
			continue;
		if(Node.m_Clipped && !Node.m_ClipPx.Contains(P) && !(Node.m_Box.m_Clip || Node.m_Box.m_Scroll != EScroll::NONE))
			continue;
		if(Node.m_Clipped && Node.m_ClipPx.m_W >= 0.0f && !Node.m_ClipPx.Contains(P))
			continue;
		m_HitNode = m_vOrder[i];
		if(Node.m_Box.m_Hit == EHit::INTERACTIVE)
			m_HoverId = Node.m_Id;
		break;
	}
	m_ScrollbarHover = 0;
	if(m_HitNode >= 0)
	{
		for(int N = m_HitNode; N >= 0; N = m_vNodes[N].m_Parent)
		{
			if(m_vNodes[N].m_Box.m_Scroll != EScroll::NONE)
			{
				m_ScrollTarget = m_vNodes[N].m_Id;
				break;
			}
		}
		for(int N = m_HitNode; N >= 0; N = m_vNodes[N].m_Parent)
		{
			const SXuiNode &S = m_vNodes[N];
			if(S.m_Box.m_Scroll != EScroll::VERTICAL || S.m_ContentH <= S.m_H + 0.5f || S.m_Screen.m_W <= 0.0f)
				continue;
			const float Zone = (Size::SCROLLBAR_W + 8.0f) * S.m_Screen.m_W / std::max(S.m_W, 1.0f);
			if(P.x >= S.m_Screen.m_X + S.m_Screen.m_W - Zone)
			{
				m_ScrollbarHover = S.m_Id;
				m_HoverId = 0;
			}
			break;
		}
	}
	if(m_ScrollDrag != 0)
		m_HoverId = 0;
	if(!m_MouseDown)
		m_ActiveId = 0;
	if(m_ActiveId != 0 && !m_MouseDown)
		m_ActiveId = 0;
}

void CXui::PaintAll()
{
	for(int L = 0; L < (int)ELayer::NUM; L++)
	{
		for(int Root : m_avLayerRoots[L])
			PaintNode(Root);
	}
}

void CXui::PaintNode(int N)
{
	SXuiNode &Node = m_vNodes[N];
	if(Node.m_AlphaAcc <= 0.003f)
		return;
	m_CurXf = Node.m_Xf;
	m_CurAlpha = Node.m_AlphaAcc;
	if(Node.m_IsText)
		DrawText(Node.m_Text.c_str(), Node.m_TextStyle, Node.m_Abs);
	if(Node.m_Paint)
		Node.m_Paint(*this, Node.m_Abs);

	const bool Clips = Node.m_Box.m_Clip || Node.m_Box.m_Scroll != EScroll::NONE;
	const bool PrevOn = m_ClipWantOn;
	const SRect PrevClip = m_ClipWant;
	if(Clips)
		SetClip(true, Node.m_ClipPx);
	for(int C = Node.m_First; C >= 0; C = m_vNodes[C].m_Next)
		PaintNode(C);
	m_CurXf = Node.m_Xf;
	m_CurAlpha = Node.m_AlphaAcc;
	if(Node.m_Box.m_Scroll == EScroll::VERTICAL)
		DrawScrollbar(Node);
	if(Node.m_PaintOver)
		Node.m_PaintOver(*this, Node.m_Abs);
	if(Clips)
		SetClip(PrevOn, PrevClip);
}

void CXui::DrawScrollbar(const SXuiNode &Node)
{
	if(Node.m_ContentH <= Node.m_H + 0.5f)
		return;
	auto It = m_States.find(Node.m_Id);
	if(It == m_States.end())
		return;
	const float Fade = std::clamp(It->second.m_aScratch[0], 0.0f, 1.0f);
	if(Fade < 0.02f)
		return;
	const float Max = Node.m_ContentH - Node.m_H;
	const float Track = Node.m_H - 8.0f;
	const float ThumbH = std::max(28.0f, Track * Node.m_H / Node.m_ContentH);
	const float ThumbY = Node.m_Abs.m_Y + 4.0f + (Track - ThumbH) * std::clamp(Node.m_ScrollY / Max, 0.0f, 1.0f);
	IGraphics::SUiRect Thumb = SolidRect(WithAlpha(Color::SCROLLBAR, Color::SCROLLBAR.a * Fade), Size::SCROLLBAR_W * 0.5f);
	DrawRect({Node.m_Abs.Right() - Size::SCROLLBAR_W - 3.0f, ThumbY, Size::SCROLLBAR_W, ThumbH}, Thumb);
}
