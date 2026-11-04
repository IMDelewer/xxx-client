#include <base/color.h>
#include <base/math.h>

#include <engine/font_icons.h>
#include <engine/font_icons_phosphor.h>
#include <engine/shared/config.h>

#include <game/client/ui.h>
#include <game/client/xui/theme.h>
#include <game/editor/editor.h>
#include <game/editor/editor_ui.h>

void CEditor::UpdateTooltip(const void *pId, const CUIRect *pRect, const char *pToolTip)
{
	if(Ui()->MouseInside(pRect) && !pToolTip)
		str_copy(m_aTooltip, "");
	else if(Ui()->HotItem() == pId && pToolTip)
		str_copy(m_aTooltip, pToolTip);
}

ColorRGBA CEditor::GetButtonColor(const void *pId, int Checked)
{
	const bool Hovered = Ui()->HotItem() == pId;
	if(Checked < 0)
		return XuiTheme::Color::GROUP_FILL;
	const ColorRGBA Accent = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_XxUiAccent));
	switch(Checked)
	{
	case EditorButtonChecked::DANGEROUS_ACTION:
		return XuiTheme::Color::RED_SOFT.WithAlpha(Hovered ? 0.2f : 0.12f);
	case EditorButtonChecked::POSITIVE_ACTION:
		return XuiTheme::Color::GREEN.WithAlpha(Hovered ? 0.2f : 0.12f);
	case 8:
		return ColorRGBA(0, 0, 0, 0);
	case 4:
	case 5:
		return XuiTheme::Color::ORANGE.WithAlpha(Checked == 5 ? 0.24f : Hovered ? 0.16f :
											  0.08f);
	case 2:
	case 3:
		return XuiTheme::Color::CYAN.WithAlpha(Checked == 3 ? 0.24f : Hovered ? 0.16f :
											0.08f);
	case 1:
	case 7:
		return Accent.WithAlpha(Hovered ? 0.26f : 0.18f);
	default:
		return Hovered ? XuiTheme::Color::ROW_HOVER : XuiTheme::Color::GROUP_FILL;
	}
}

int CEditor::DoButtonLogic(const void *pId, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(Ui()->MouseInside(pRect))
	{
		if(Flags & BUTTONFLAG_RIGHT)
			m_pUiGotContext = pId;
	}

	UpdateTooltip(pId, pRect, pToolTip);
	return Ui()->DoButtonLogic(pId, Checked, pRect, Flags);
}

int CEditor::DoButton_Editor(const void *pId, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	pRect->Draw(GetButtonColor(pId, Checked), IGraphics::CORNER_ALL, 4.0f);
	CUIRect NewRect = *pRect;
	SLabelProperties Props;
	Props.m_MaxWidth = NewRect.w;
	Props.m_EllipsisAtEnd = true;
	if(Checked < 0)
		Props.SetColor(XuiTheme::Color::TEXT3);
	Ui()->DoLabel(&NewRect, pText, EditorFontSizes::MENU, TEXTALIGN_MC, Props);
	Checked %= 2;
	return DoButtonLogic(pId, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_Env(const void *pId, const char *pText, int Checked, const CUIRect *pRect, const char *pToolTip, ColorRGBA BaseColor, int Corners)
{
	float Bright = Checked ? 1.0f : 0.5f;
	float Alpha = Ui()->HotItem() == pId ? 1.0f : 0.75f;
	ColorRGBA Color = ColorRGBA(BaseColor.r * Bright, BaseColor.g * Bright, BaseColor.b * Bright, Alpha);

	pRect->Draw(Color, Corners, 3.0f);
	Ui()->DoLabel(pRect, pText, 10.0f, TEXTALIGN_MC);
	Checked %= 2;
	return DoButtonLogic(pId, Checked, pRect, BUTTONFLAG_LEFT, pToolTip);
}

int CEditor::DoButton_Ex(const void *pId, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int Corners, float FontSize, int Align)
{
	pRect->Draw(GetButtonColor(pId, Checked), Corners, 3.0f);

	CUIRect Rect;
	pRect->VMargin(((Align & TEXTALIGN_MASK_HORIZONTAL) == TEXTALIGN_CENTER) ? 1.0f : 5.0f, &Rect);

	SLabelProperties Props;
	Props.m_MaxWidth = Rect.w;
	Props.m_EllipsisAtEnd = true;
	Ui()->DoLabel(&Rect, pText, FontSize, Align, Props);

	return DoButtonLogic(pId, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_FontIcon(const void *pId, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip, int Corners, float FontSize)
{
	pRect->Draw(GetButtonColor(pId, Checked), Corners, 3.0f);

	static const struct
	{
		const char *m_pOriginal;
		const char *m_pIcon;
	} s_aIcons[] = {
		{FontIcon::BACKWARD_STEP, Ph::SKIP_BACK},
		{FontIcon::PLAY, Ph::PLAY},
		{FontIcon::PAUSE, Ph::PAUSE},
		{FontIcon::CIRCLE_CHEVRON_DOWN, Ph::CARET_DOWN},
		{FontIcon::BORDER_ALL, Ph::GRID_FOUR},
		{FontIcon::MINUS, Ph::MINUS},
		{FontIcon::MAGNIFYING_GLASS, Ph::MAGNIFYING_GLASS},
		{FontIcon::PLUS, Ph::PLUS},
		{FontIcon::UNDO, Ph::ARROW_COUNTER_CLOCKWISE},
		{FontIcon::REDO, Ph::ARROW_CLOCKWISE},
		{FontIcon::ARROWS_LEFT_RIGHT, Ph::ARROWS_HORIZONTAL},
		{FontIcon::ARROWS_UP_DOWN, Ph::ARROWS_VERTICAL},
		{FontIcon::ARROW_ROTATE_LEFT, Ph::ARROW_ARC_LEFT},
		{FontIcon::ARROW_ROTATE_RIGHT, Ph::ARROW_ARC_RIGHT},
		{FontIcon::EYE_DROPPER, Ph::EYEDROPPER},
		{FontIcon::BRUSH, Ph::PAINT_BRUSH},
		{FontIcon::LAYER_GROUP, Ph::STACK},
		{FontIcon::IMAGE, Ph::IMAGE},
		{FontIcon::MUSIC, Ph::MUSIC_NOTES},
		{FontIcon::EYE, Ph::EYE},
		{FontIcon::EYE_SLASH, Ph::EYE_SLASH},
		{FontIcon::CIRCLE, Ph::CIRCLE},
	};
	const unsigned PreviousFlags = TextRender()->GetRenderFlags();
	const char *pIcon = nullptr;
	for(const auto &Icon : s_aIcons)
		if(str_comp(pText, Icon.m_pOriginal) == 0)
		{
			pIcon = Icon.m_pIcon;
			break;
		}
	const ColorRGBA PreviousColor = TextRender()->GetTextColor();
	TextRender()->TextColor(Checked < 0 ? XuiTheme::Color::TEXT3 : PreviousColor == TextRender()->DefaultTextColor() ? XuiTheme::Color::TEXT :
															   PreviousColor);
	if(pIcon)
	{
		const float Size = FontSize + 2.0f;
		TextRender()->SetFontPreset(EFontPreset::PHOSPHOR);
		TextRender()->SetRenderFlags(TEXT_RENDER_FLAG_FULL_ADVANCE | TEXT_RENDER_FLAG_CENTERED_GLYPH);
		CTextCursor Cursor;
		Cursor.SetPosition(pRect->Center() - vec2(Size * 0.5f, Size * 0.5f));
		Cursor.m_FontSize = Size;
		TextRender()->TextEx(&Cursor, pIcon);
	}
	else
	{
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | TEXT_RENDER_FLAG_NO_X_BEARING | TEXT_RENDER_FLAG_NO_Y_BEARING);
		Ui()->DoLabel(pRect, pText, FontSize, TEXTALIGN_MC);
	}
	TextRender()->TextColor(PreviousColor);
	TextRender()->SetRenderFlags(PreviousFlags);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

	return DoButtonLogic(pId, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_MenuItem(const void *pId, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if((Ui()->HotItem() == pId && Checked == 0) || Checked > 0)
		pRect->Draw(GetButtonColor(pId, Checked), IGraphics::CORNER_ALL, 3.0f);

	CUIRect Rect;
	pRect->VMargin(5.0f, &Rect);

	SLabelProperties Props;
	Props.m_MaxWidth = Rect.w;
	Props.m_EllipsisAtEnd = true;
	if(Checked < 0)
	{
		Props.SetColor(ColorRGBA(0.6f, 0.6f, 0.6f, 1.0f));
	}
	Ui()->DoLabel(&Rect, pText, 10.0f, TEXTALIGN_ML, Props);

	return DoButtonLogic(pId, Checked, pRect, Flags, pToolTip);
}

int CEditor::DoButton_DraggableEx(const void *pId, const char *pText, int Checked, const CUIRect *pRect, bool *pClicked, bool *pAbrupted, int Flags, const char *pToolTip, int Corners, float FontSize)
{
	pRect->Draw(GetButtonColor(pId, Checked), Corners, 3.0f);

	CUIRect Rect;
	pRect->VMargin(pRect->w > 20.0f ? 5.0f : 0.0f, &Rect);

	SLabelProperties Props;
	Props.m_MaxWidth = Rect.w;
	Props.m_EllipsisAtEnd = true;
	if(Checked == 1 || Checked == 3 || Checked == 5 || Checked == 7)
	{
		CUIRect Marker = *pRect;
		Marker.w = 2.0f;
		Marker.HMargin(3.0f, &Marker);
		Marker.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_XxUiAccent)), IGraphics::CORNER_ALL, 1.0f);
	}
	Ui()->DoLabel(&Rect, pText, FontSize, TEXTALIGN_ML, Props);

	if(Ui()->MouseInside(pRect))
	{
		if(Flags & BUTTONFLAG_RIGHT)
			m_pUiGotContext = pId;
	}

	UpdateTooltip(pId, pRect, pToolTip);
	return Ui()->DoDraggableButtonLogic(pId, Checked, pRect, pClicked, pAbrupted);
}

bool CEditor::DoEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners, const char *pToolTip, const std::vector<STextColorSplit> &vColorSplits)
{
	UpdateTooltip(pLineInput, pRect, pToolTip);
	return Ui()->DoEditBox(pLineInput, pRect, FontSize, Corners, vColorSplits);
}

bool CEditor::DoClearableEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners, const char *pToolTip, const std::vector<STextColorSplit> &vColorSplits)
{
	UpdateTooltip(pLineInput, pRect, pToolTip);
	return Ui()->DoClearableEditBox(pLineInput, pRect, FontSize, Corners, vColorSplits);
}

SEditResult<int> CEditor::UiDoValueSelector(const void *pId, CUIRect *pRect, const char *pLabel, int Current, int Min, int Max, int Step, float Scale, const char *pToolTip, bool IsDegree, bool IsHex, int Corners, const ColorRGBA *pColor, bool ShowValue)
{
	// logic
	static bool s_DidScroll = false;
	static float s_ScrollValue = 0.0f;
	static CLineInputNumber s_NumberInput;
	static int s_ButtonUsed = -1;
	static const void *s_pLastTextId = nullptr;

	const bool Inside = Ui()->MouseInside(pRect);
	const int Base = IsHex ? 16 : 10;

	if(Ui()->HotItem() == pId && s_ButtonUsed >= 0 && !Ui()->MouseButton(s_ButtonUsed))
	{
		Ui()->DisableMouseLock();
		if(Ui()->CheckActiveItem(pId))
		{
			Ui()->SetActiveItem(nullptr);
		}
		if(Inside && ((s_ButtonUsed == 0 && !s_DidScroll && Ui()->DoDoubleClickLogic(pId)) || s_ButtonUsed == 1))
		{
			s_pLastTextId = pId;
			s_NumberInput.SetInteger(Current, Base);
			s_NumberInput.SelectAll();
		}
		s_ButtonUsed = -1;
	}

	if(s_pLastTextId == pId)
	{
		str_copy(m_aTooltip, "Type your number. Press enter to confirm.");
		Ui()->SetActiveItem(&s_NumberInput);
		DoEditBox(&s_NumberInput, pRect, 10.0f, Corners);

		if(Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER) || ((Ui()->MouseButtonClicked(1) || Ui()->MouseButtonClicked(0)) && !Inside))
		{
			Current = std::clamp(s_NumberInput.GetInteger(Base), Min, Max);
			Ui()->DisableMouseLock();
			Ui()->SetActiveItem(nullptr);
			s_pLastTextId = nullptr;
		}

		if(Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))
		{
			Ui()->DisableMouseLock();
			Ui()->SetActiveItem(nullptr);
			s_pLastTextId = nullptr;
		}
	}
	else
	{
		if(Ui()->CheckActiveItem(pId))
		{
			if(s_ButtonUsed == 0 && Ui()->MouseButton(0))
			{
				s_ScrollValue += Ui()->MouseDeltaX() * (Input()->ShiftIsPressed() ? 0.05f : 1.0f);

				if(absolute(s_ScrollValue) >= Scale)
				{
					int Count = (int)(s_ScrollValue / Scale);
					s_ScrollValue = std::fmod(s_ScrollValue, Scale);
					Current += Step * Count;
					Current = std::clamp(Current, Min, Max);
					s_DidScroll = true;

					// Constrain to discrete steps
					if(Count > 0)
						Current = Current / Step * Step;
					else
						Current = std::ceil(Current / (float)Step) * Step;
				}
			}

			if(pToolTip && s_pLastTextId != pId)
				str_copy(m_aTooltip, pToolTip);
		}
		else if(Ui()->HotItem() == pId)
		{
			if(Ui()->MouseButton(0))
			{
				s_ButtonUsed = 0;
				s_DidScroll = false;
				s_ScrollValue = 0.0f;
				Ui()->SetActiveItem(pId);
				Ui()->EnableMouseLock(pId);
			}
			else if(Ui()->MouseButton(1))
			{
				s_ButtonUsed = 1;
				Ui()->SetActiveItem(pId);
			}

			if(pToolTip && s_pLastTextId != pId)
				str_copy(m_aTooltip, pToolTip);
		}

		// render
		char aBuf[128];
		if(pLabel[0] != '\0')
		{
			if(ShowValue)
				str_format(aBuf, sizeof(aBuf), "%s %d", pLabel, Current);
			else
				str_copy(aBuf, pLabel);
		}
		else if(IsDegree)
			str_format(aBuf, sizeof(aBuf), "%d°", Current);
		else if(IsHex)
			str_format(aBuf, sizeof(aBuf), "#%06X", Current);
		else
			str_format(aBuf, sizeof(aBuf), "%d", Current);
		pRect->Draw(pColor ? *pColor : GetButtonColor(pId, 0), Corners, 3.0f);
		Ui()->DoLabel(pRect, aBuf, 10, TEXTALIGN_MC);
	}

	if(Inside && !Ui()->MouseButton(0) && !Ui()->MouseButton(1))
		Ui()->SetHotItem(pId);

	static const void *s_pEditing = nullptr;
	EEditState State = EEditState::NONE;
	if(s_pEditing == pId)
	{
		State = EEditState::EDITING;
	}
	if(((Ui()->CheckActiveItem(pId) && Ui()->CheckMouseLock() && s_DidScroll) || s_pLastTextId == pId) && s_pEditing != pId)
	{
		State = EEditState::START;
		s_pEditing = pId;
	}
	if(!Ui()->CheckMouseLock() && s_pLastTextId != pId && s_pEditing == pId)
	{
		State = EEditState::END;
		s_pEditing = nullptr;
	}

	return SEditResult<int>{State, Current};
}

void CEditor::RenderBackground(CUIRect View, IGraphics::CTextureHandle Texture, float Size, float Brightness) const
{
	Graphics()->TextureSet(Texture);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Brightness, Brightness, Brightness, 1.0f);
	Graphics()->QuadsSetSubset(0, 0, View.w / Size, View.h / Size);
	IGraphics::CQuadItem QuadItem(View.x, View.y, View.w, View.h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}
