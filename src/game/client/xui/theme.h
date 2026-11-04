#ifndef GAME_CLIENT_XUI_THEME_H
#define GAME_CLIENT_XUI_THEME_H

#include <base/color.h>

// Every xxx UI value lives here, in mockup px at 1440x900. Screens never hard-code colours or sizes.
namespace XuiTheme
{
	inline ColorRGBA Rgb(unsigned Hex, float a = 1.0f) { return ColorRGBA(((Hex >> 16) & 0xff) / 255.0f, ((Hex >> 8) & 0xff) / 255.0f, (Hex & 0xff) / 255.0f, a); }

	namespace Color
	{
		inline const ColorRGBA ACCENT_DEFAULT = Rgb(0xAC6CFF);
		inline const ColorRGBA GREEN = Rgb(0x30D158);
		inline const ColorRGBA YELLOW = Rgb(0xFFD60A);
		inline const ColorRGBA ORANGE = Rgb(0xFF9F0A);
		inline const ColorRGBA CYAN = Rgb(0x64D2FF);
		inline const ColorRGBA PINK = Rgb(0xFF375F);
		inline const ColorRGBA RED_SOFT = Rgb(0xFF6961);
		inline const ColorRGBA GREEN_SOFT = Rgb(0x8FF0A4);
		inline ColorRGBA BACKGROUND = Rgb(0x05060A);

		inline ColorRGBA TEXT = ColorRGBA(1, 1, 1, 1);
		inline ColorRGBA TEXT2 = ColorRGBA(1, 1, 1, 0.66f);
		inline ColorRGBA TEXT3 = ColorRGBA(1, 1, 1, 0.5f);
		inline ColorRGBA SEPARATOR = ColorRGBA(1, 1, 1, 0.08f);

		inline const ColorRGBA GHOST = Rgb(0x767680, 0.16f);
		inline const ColorRGBA GHOST_HOVER = Rgb(0x767680, 0.26f);
		inline ColorRGBA SELECTED = ColorRGBA(1, 1, 1, 0.14f);
		inline ColorRGBA ROW_HOVER = ColorRGBA(1, 1, 1, 0.04f);
		inline ColorRGBA NAV_HOVER = ColorRGBA(1, 1, 1, 0.07f);
		inline ColorRGBA GROUP_FILL = ColorRGBA(1, 1, 1, 0.035f);
		inline ColorRGBA GROUP_STROKE = ColorRGBA(1, 1, 1, 0.06f);
		inline ColorRGBA MENU_FILL = Rgb(0x18181C, 0.86f);
		inline ColorRGBA MENU_ITEM_HOVER = ColorRGBA(1, 1, 1, 0.1f);
		inline const ColorRGBA KNOB = ColorRGBA(1, 1, 1, 1);
		inline const ColorRGBA ISLAND = ColorRGBA(0, 0, 0, 1);
		inline const ColorRGBA SCRIM = ColorRGBA(0, 0, 0, 0.42f);
		inline ColorRGBA SCROLLBAR = ColorRGBA(1, 1, 1, 0.2f);
		inline ColorRGBA SEGMENT_PILL = ColorRGBA(1, 1, 1, 0.2f);
		inline ColorRGBA SEGMENT_PILL_HIGHLIGHT = ColorRGBA(1, 1, 1, 0.18f);
		inline ColorRGBA DOCK_TEXT_IDLE = ColorRGBA(1, 1, 1, 0.72f);
		inline ColorRGBA ICON_BUTTON_HOVER = ColorRGBA(1, 1, 1, 0.1f);
		inline ColorRGBA SKELETON = ColorRGBA(1, 1, 1, 0.06f);
		inline ColorRGBA SKELETON_SHINE = ColorRGBA(1, 1, 1, 0.12f);
		inline ColorRGBA TEXT_IDLE = ColorRGBA(1, 1, 1, 0.65f);
		inline ColorRGBA NAV_ICON_IDLE = ColorRGBA(1, 1, 1, 0.55f);
		inline ColorRGBA NAV_ACTIVE = ColorRGBA(1, 1, 1, 0.14f);
		inline ColorRGBA NAV_ACTIVE_HIGHLIGHT = ColorRGBA(1, 1, 1, 0.22f);
		inline const ColorRGBA TRACK_OFF = Rgb(0x787880, 0.32f);
		inline ColorRGBA TRACK = ColorRGBA(1, 1, 1, 0.14f);
		inline ColorRGBA TOOLTIP_FILL = Rgb(0x18181C, 0.92f);
	}

	namespace Glass
	{
		constexpr float BLUR = 22.0f;
		constexpr float SATURATION = 1.15f;
		inline const ColorRGBA FILL_TOP = ColorRGBA(1, 1, 1, 0.06f);
		inline const ColorRGBA FILL_BOTTOM = ColorRGBA(1, 1, 1, 0.025f);
		// rim: 1px stroke, linear gradient at 140deg with stops .42@0 .10@22% 0@45% 0@60% .12@82% .28@100%
		constexpr float RIM_ANGLE = 140.0f;
		constexpr float RIM_WIDTH = 1.0f;
		inline const ColorRGBA SHADOW = ColorRGBA(0, 0, 0, 0.18f);
		constexpr float SHADOW_Y = 4.0f;
		constexpr float SHADOW_BLUR = 16.0f;
		inline const ColorRGBA INNER_STROKE = ColorRGBA(1, 1, 1, 0.06f);
		constexpr float PANEL_ALPHA = 0.74f; // dark tint panel: rgba(14,14,18,a) -> rgba(8,8,10,a+.06)
		inline const ColorRGBA PANEL_TOP = Rgb(0x0E0E12, PANEL_ALPHA);
		inline const ColorRGBA PANEL_BOTTOM = Rgb(0x08080A, PANEL_ALPHA + 0.06f);
	}

	namespace Home
	{
		inline const ColorRGBA SCRIM = Rgb(0x0B1420, 0.10f);
		inline const ColorRGBA TEE_LABEL = Rgb(0x0B1420, 0.55f);
		inline const ColorRGBA TEXT = Rgb(0x182B43);
		inline const ColorRGBA BLUE = Rgb(0x2776D9);
		inline ColorRGBA PANEL = Rgb(0x191B20, 0.96f);
		inline const ColorRGBA BORDER = Rgb(0xFFFFFF, 0.09f);
		inline const ColorRGBA SHADOW = Rgb(0x000000, 0.22f);
	}

	inline void UpdateTheme(float Lightness)
	{
		auto Blend = [Lightness](ColorRGBA Dark, ColorRGBA Light) {
			return ColorRGBA(Dark.r + (Light.r - Dark.r) * Lightness, Dark.g + (Light.g - Dark.g) * Lightness, Dark.b + (Light.b - Dark.b) * Lightness, Dark.a + (Light.a - Dark.a) * Lightness);
		};
		const ColorRGBA Ink = Blend(Rgb(0xFFFFFF), Rgb(0x18202D));
		Color::TEXT = Ink.WithAlpha(1);
		Color::TEXT2 = Ink.WithAlpha(0.66f);
		Color::TEXT3 = Ink.WithAlpha(0.5f);
		Color::SEPARATOR = Ink.WithAlpha(0.08f);
		Color::SELECTED = Ink.WithAlpha(0.14f);
		Color::ROW_HOVER = Ink.WithAlpha(0.04f);
		Color::NAV_HOVER = Ink.WithAlpha(0.07f);
		Color::GROUP_FILL = Ink.WithAlpha(0.035f);
		Color::GROUP_STROKE = Ink.WithAlpha(0.06f);
		Color::MENU_ITEM_HOVER = Ink.WithAlpha(0.1f);
		Color::SCROLLBAR = Ink.WithAlpha(0.2f);
		Color::SEGMENT_PILL = Ink.WithAlpha(0.2f);
		Color::SEGMENT_PILL_HIGHLIGHT = Ink.WithAlpha(0.18f);
		Color::DOCK_TEXT_IDLE = Ink.WithAlpha(0.72f);
		Color::ICON_BUTTON_HOVER = Ink.WithAlpha(0.1f);
		Color::SKELETON = Ink.WithAlpha(0.06f);
		Color::SKELETON_SHINE = Ink.WithAlpha(0.12f);
		Color::TEXT_IDLE = Ink.WithAlpha(0.65f);
		Color::NAV_ICON_IDLE = Ink.WithAlpha(0.55f);
		Color::NAV_ACTIVE = Ink.WithAlpha(0.14f);
		Color::NAV_ACTIVE_HIGHLIGHT = Ink.WithAlpha(0.22f);
		Color::TRACK = Ink.WithAlpha(0.14f);
		Color::MENU_FILL = Blend(Rgb(0x18181C, 0.94f), Rgb(0xF6F7FB, 0.94f));
		Color::TOOLTIP_FILL = Blend(Rgb(0x18181C, 0.96f), Rgb(0xF6F7FB, 0.96f));
		Color::BACKGROUND = Blend(Rgb(0x05060A), Rgb(0xE8EDF4));
		Home::PANEL = Blend(Rgb(0x191B20), Rgb(0xF6F7FB));
	}

	namespace Size
	{
		constexpr float PAGE_MARGIN = 36.0f;
		constexpr float PAGE_TOP = 104.0f;
		constexpr float HEADER_TOP = 20.0f;
		constexpr float HEADER_H = 44.0f;
		constexpr float MENU_CURSOR = 14.0f;
		constexpr float GALLERY_CURSOR = 20.0f;
		constexpr float HOME_GAP = 24.0f;
		constexpr float HOME_PREVIEW_W = 320.0f;
		constexpr float HOME_PREVIEW_H = 224.0f;
		constexpr float HOME_FRIENDS_W = 280.0f;
		constexpr float HOME_TITLE = 42.0f;
		constexpr float BROWSER_SIDEBAR_W = 196.0f;
		constexpr float BROWSER_DETAIL_W = 296.0f;
		constexpr float SETTINGS_ROW_H = 48.0f;
		constexpr float HELP_ICON_H = 24.0f;
		constexpr float TOOLTIP_MAX_W = 360.0f;
		constexpr float SPACE[] = {4, 8, 12, 16, 24, 32, 48};
		constexpr float CONTROL_H = 36.0f; // cap, field
		constexpr float NAV_H = 40.0f;
		constexpr float ROW_MIN_H = 56.0f;
		constexpr float ROW_PAD_X = 18.0f;
		constexpr float ROW_PAD_Y = 10.0f;
		constexpr float ROW_GAP = 16.0f;
		constexpr float SWITCH_W = 51.0f;
		constexpr float SWITCH_H = 31.0f;
		constexpr float KNOB = 27.0f;
		constexpr float KNOB_PRESSED_W = 33.0f;
		constexpr float SEGMENT_H = 30.0f;
		constexpr float SEGMENT_PAD = 3.0f;
		constexpr float SLIDER_TRACK = 4.0f;
		constexpr float SLIDER_THUMB = 26.0f;
		constexpr float KBD_H = 28.0f;
		constexpr float KBD_MIN_W = 44.0f;
		constexpr float SWATCH = 28.0f;
		constexpr float DOCK_H = 68.0f;
		constexpr float DOCK_PAD = 6.0f;
		constexpr float DOCK_ITEM_W = 84.0f;
		constexpr float DOCK_ITEM_H = 56.0f;
		constexpr float DOCK_BOTTOM = 22.0f;
		constexpr float MENU_W = 230.0f;
		constexpr float MENU_ITEM_H = 36.0f;
		constexpr float ISLAND_TOP = 14.0f;
		constexpr float ICON = 20.0f;
		constexpr float NAV_ICON_BOX = 28.0f;
		constexpr float CAP_PAD_X = 14.0f;
		constexpr float CAP_GAP = 8.0f;
		constexpr float FIELD_PAD_X = 16.0f;
		constexpr float NAV_PAD_X = 10.0f;
		constexpr float NAV_GAP = 12.0f;
		constexpr float NAV_ICON_SIZE = 18.0f;
		constexpr float SEGMENT_ITEM_PAD_X = 14.0f;
		constexpr float SEGMENT_STRETCH = 0.12f; // width gain at peak velocity
		constexpr float KBD_PAD_X = 10.0f;
		constexpr float DOCK_GAP = 2.0f;
		constexpr float DOCK_ICON = 24.0f;
		constexpr float CTX_PAD = 6.0f;
		constexpr float CTX_ITEM_PAD_X = 12.0f;
		constexpr float CTX_ITEM_GAP = 10.0f;
		constexpr float ICON_BUTTON = 28.0f;
		constexpr float SCROLLBAR_W = 6.0f;
		constexpr float FOCUS_RING = 2.0f;
		constexpr float ROW_ICON_BOX = 28.0f;
		constexpr float SEPARATOR_INSET = 18.0f;
		constexpr float POPOVER_PAD = 8.0f;
		constexpr float POPOVER_GAP = 8.0f;
		constexpr float MODAL_W = 420.0f;
		constexpr float TOAST_H = 48.0f;
		constexpr float TOAST_BOTTOM = 106.0f;
		constexpr float BADGE_H = 20.0f;
		constexpr float CHIP_H = 28.0f;
		constexpr float PROGRESS_H = 6.0f;
		constexpr float RING_STROKE = 4.0f;
		constexpr float SLIDER_BUBBLE_H = 28.0f;
		constexpr float SPINNER = 20.0f;
	}

	namespace Scene
	{
		constexpr float HEIGHT = 900.0f;
		inline const ColorRGBA SKY_TOP = Rgb(0x0B111B);
		inline const ColorRGBA SKY_BOTTOM = Rgb(0x06090F);
		inline const ColorRGBA LIGHT_BLUE = Rgb(0x456C9C);
		inline const ColorRGBA LIGHT_TEAL = Rgb(0x477A79);
	}

	namespace Island
	{
		constexpr float GAP = 8.0f;
		constexpr float PILL_H = 38.0f;
		constexpr float TIMER_W = 90.0f;
		constexpr float IDLE_W = 200.0f;
		constexpr float NOTIFY_W = 500.0f;
		constexpr float NOTIFY_H = 84.0f;
		constexpr float VOTE_W = 460.0f;
		constexpr float VOTE_H = 120.0f;
		constexpr float PROGRESS_W = 460.0f;
		constexpr float PROGRESS_H = 80.0f;
		constexpr float CARD_W = 540.0f;
		constexpr float CARD_H = 280.0f;
		constexpr float SPLIT_W = 400.0f;
		constexpr float SPLIT_H = 72.0f;
		constexpr float CONTENT_DELAY = 0.16f;
		constexpr float CONTENT_FADE = 0.3f;
		constexpr float CONTENT_SCALE_FROM = 0.94f;
		constexpr float SHADOW_Y = 10.0f;
		constexpr float SHADOW_BLUR = 36.0f;
		inline const ColorRGBA SHADOW = ColorRGBA(0, 0, 0, 0.55f);
		inline const ColorRGBA INNER_STROKE = ColorRGBA(1, 1, 1, 0.07f);
		inline const ColorRGBA RING_TRACK = ColorRGBA(1, 1, 1, 0.12f);
		inline const ColorRGBA PROGRESS_TRACK = ColorRGBA(1, 1, 1, 0.1f);
	}

	namespace Radius
	{
		constexpr float XS = 6.0f;
		constexpr float SM = 8.0f;
		constexpr float MD = 10.0f;
		constexpr float LG = 12.0f;
		constexpr float XL = 14.0f;
		constexpr float XXL = 16.0f;
		constexpr float PILL = -1.0f; // h / 2
	}

	namespace Font
	{
		constexpr float NAV = 14.0f;
		constexpr float CAP = 13.0f;
		constexpr float SEGMENT = 13.0f;
		constexpr float KBD = 12.0f;
		constexpr float FIELD = 14.0f;
		constexpr float ROW_TITLE = 15.0f;
		constexpr float ROW_SUBTITLE = 12.5f;
		constexpr float CTX_ITEM = 13.5f;
		constexpr float SECTION_HEADER = 12.0f;
		constexpr float PAGE_TITLE = 34.0f;
		constexpr float PAGE_SUBTITLE = 13.5f;
		constexpr float LINE_HEIGHT = 1.21f; // Inter normal line height
		constexpr float BASELINE_SHIFT = 0.0f; // em, vertical optical correction of text inside its line box
		constexpr float ICON_SHIFT_X = -0.125f; // em, Phosphor glyphs land right of their advance box
		constexpr float ICON_SHIFT_Y = 0.19f; // em, and above the line box centre
		constexpr float BODY = 14.0f;
		constexpr float SMALL = 13.0f;
		constexpr float CAPTION = 12.0f;
		constexpr float DOCK = 10.5f;
		constexpr float TITLE = 20.0f;
		constexpr float LARGE_TITLE = 34.0f;
		constexpr float TRACKING = -0.011f; // em
	}

	// Springs: stiffness/damping tuned to match the mockup's cubic-bezier overshoots.
	namespace Motion
	{
		struct SSpring
		{
			float m_Stiffness;
			float m_Damping;
		};
		constexpr SSpring PRESS = {520.0f, 26.0f};
		constexpr SSpring KNOB = {420.0f, 24.0f}; // .4s cubic-bezier(.2,1.5,.4,1)
		constexpr SSpring INDICATOR = {300.0f, 24.0f}; // .55s cubic-bezier(.25,1.35,.4,1), dock and segmented indicator
		constexpr SSpring ISLAND = {260.0f, 20.0f}; // .62s cubic-bezier(.2,1.25,.35,1)
		constexpr SSpring LAYOUT = {260.0f, 26.0f}; // FLIP layout transitions
		constexpr SSpring HOVER = {420.0f, 34.0f}; // colour and opacity fades, no overshoot
		constexpr SSpring POP = {480.0f, 28.0f};
		constexpr SSpring PAGE = {300.0f, 30.0f};
		constexpr float WHEEL_IMPULSE = 620.0f; // px/s per wheel notch, at 900 px window height
		constexpr float SCROLL_FRICTION = 7.0f;
		constexpr float GC_SECONDS = 2.0f;
		constexpr float POP_FROM = 1.0f;
		constexpr float PAGE_FROM_SCALE = 1.0f;
		constexpr float PAGE_FROM_Y = 0.0f;
		constexpr float SWATCH_HOVER_SCALE = 1.0f;
		constexpr float STAR_HOVER_SCALE = 1.0f;
		constexpr float HOVER_SCALE = 1.0f;
		constexpr float PRESS_SCALE = 1.0f;
		constexpr float ICON_HOVER_SCALE = 1.0f;
		constexpr float FADE = 0.16f;
		constexpr float STAGGER = 0.02f;
	}
}

#endif
