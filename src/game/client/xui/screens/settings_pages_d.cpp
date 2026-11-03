#include "settings_internal.h"

#include <base/str.h>

#include <engine/font_icons_phosphor.h>
#include <engine/shared/config.h>

#include <game/client/components/chat.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <initializer_list>

using namespace Xui;

namespace SettingsUi
{
	namespace
	{
		struct SChip
		{
			const char *m_pLabel;
			unsigned *m_pColor;
		};

		void ColorChips(const char *pKey, std::initializer_list<SChip> Chips)
		{
			CXui &U = Ui();
			SBox B;
			B.m_Gap = 6.0f;
			B.m_Cross = EAlign::CENTER;
			U.Row(B, [&]() {
				int i = 0;
				for(const SChip &Chip : Chips)
				{
					SBox C;
					C.m_H = Px(34.0f);
					C.m_Gap = 8.0f;
					C.m_Padding = SEdges(0.0f, 12.0f, 0.0f, 4.0f);
					C.m_Cross = EAlign::CENTER;
					const int Node = U.Row(C, [&]() {
						char aKey[64];
						str_format(aKey, sizeof(aKey), "%s.%d", pKey, i);
						U.ColorPickerButton(aKey, Chip.m_pColor);
						STextStyle S;
						S.m_Size = 12.5f;
						S.m_Font = EFont::MEDIUM;
						S.m_Color = XuiTheme::Color::TEXT2;
						U.Text(Localize(Chip.m_pLabel), S);
					});
					U.Paint(Node, [](CXui &Ui, const SRect &Rc) { Ui.DrawRect(Rc, CXui::SolidRect(XuiTheme::Color::GHOST, Rc.m_H * 0.5f)); });
					i++;
				}
			});
		}

		void TriSegment(SSection &S, const char *pLabel, int *pValue, const char *pOff, const char *pOn, const char *pAlways)
		{
			S.Segment(pLabel, pValue, {pOff, pOn, pAlways});
		}

		ColorRGBA Col(unsigned Hsl, float Alpha = 1.0f)
		{
			return color_cast<ColorRGBA>(ColorHSLA(Hsl, true)).WithAlpha(Alpha);
		}

		void ChatPreview()
		{
			CXui &U = Ui();
			struct SLine
			{
				const char *m_pName;
				const char *m_pText;
				unsigned m_Color;
				bool m_Heart;
				bool m_System;
			};
			const SLine aLines[] = {
				{nullptr, "'Kirito' entered and joined the game", g_Config.m_ClMessageSystemColor, false, true},
				{"Kirito", "gl hf everyone", g_Config.m_ClMessageColor, false, false},
				{"Asuna", "team, wait for me at the freeze", g_Config.m_ClMessageTeamColor, false, false},
				{"Sinon", "nameless tee, nice save!", g_Config.m_ClMessageHighlightColor, false, false},
				{"Klein", "added you as a friend", g_Config.m_ClMessageFriendColor, g_Config.m_ClMessageFriend != 0, false},
				{nullptr, "— local echo", g_Config.m_ClMessageClientColor, false, true},
			};
			const float Size = std::clamp(g_Config.m_ClChatFontSize / 6.0f, 9.0f, 17.0f);
			SBox Box;
			Box.m_W = Grow();
			Box.m_Padding = SEdges(12.0f);
			Box.m_Gap = 5.0f;
			const ColorRGBA Bg = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClChatBackgroundColor, true));
			const int Node = U.Column(Box, [&]() {
				for(const SLine &Line : aLines)
				{
					if(Line.m_System && !g_Config.m_ClShowChatSystem && Line.m_pName == nullptr && Line.m_Color == g_Config.m_ClMessageSystemColor)
						continue;
					SBox R;
					R.m_W = Grow();
					R.m_Gap = 5.0f;
					R.m_Cross = EAlign::CENTER;
					U.Row(R, [&]() {
						STextStyle S;
						S.m_Size = Size;
						S.m_Font = EFont::DEJAVU;
						S.m_Wrap = true;
						S.m_MaxLines = 2;
						if(Line.m_Heart)
							U.Icon(Ph::HEART, Size, Col(g_Config.m_ClMessageFriendColor), true);
						if(Line.m_pName)
						{
							STextStyle N = S;
							N.m_Color = g_Config.m_ClChatTeamColors ? XuiTheme::Rgb(0xFF9F0A) : ColorRGBA(0.8f, 0.8f, 0.8f, 1.0f);
							N.m_Wrap = false;
							char aName[64];
							str_format(aName, sizeof(aName), "%s:", Line.m_pName);
							U.Text(aName, N);
						}
						S.m_Color = Col(Line.m_Color);
						SBox TB;
						TB.m_W = Grow();
						TB.m_Shrink = true;
						U.Text(Line.m_pText, S, TB);
					});
				}
			});
			U.Paint(Node, [Bg](CXui &Ui, const SRect &Rc) {
				IGraphics::SUiRect Back = CXui::SolidRect(ColorRGBA(0.1f, 0.12f, 0.2f, 1.0f), 16.0f);
				Back.m_Fill2 = ColorRGBA(0.2f, 0.16f, 0.24f, 1.0f);
				Back.m_GradientAngle = 90.0f;
				Ui.DrawRect(Rc, Back);
				Ui.DrawRect(Rc.Inset(6.0f), CXui::SolidRect(Bg, 10.0f));
			});
		}

		void LaserTable()
		{
			CXui &U = Ui();
			struct SLaser
			{
				const char *m_pName;
				unsigned *m_pOutline;
				unsigned *m_pInner;
			};
			const SLaser aLasers[] = {
				{"Rifle", &g_Config.m_ClLaserRifleOutlineColor, &g_Config.m_ClLaserRifleInnerColor},
				{"Shotgun", &g_Config.m_ClLaserShotgunOutlineColor, &g_Config.m_ClLaserShotgunInnerColor},
				{"Door", &g_Config.m_ClLaserDoorOutlineColor, &g_Config.m_ClLaserDoorInnerColor},
				{"Freeze", &g_Config.m_ClLaserFreezeOutlineColor, &g_Config.m_ClLaserFreezeInnerColor},
				{"Dragger", &g_Config.m_ClLaserDraggerOutlineColor, &g_Config.m_ClLaserDraggerInnerColor},
			};
			U.Group([&]() {
				bool First = true;
				uint32_t Index = 0;
				for(const SLaser &Laser : aLasers)
				{
					U.PushId(++Index);
					U.GroupRow([&]() {
						STextStyle L;
						L.m_Size = 14.5f;
						L.m_Font = EFont::MEDIUM;
						SBox LB;
						LB.m_W = Px(90.0f);
						U.Text(Localize(Laser.m_pName), L, LB);
						SBox Beam;
						Beam.m_W = Grow();
						Beam.m_H = Px(20.0f);
						const ColorRGBA Outline = Col(*Laser.m_pOutline);
						const ColorRGBA Inner = Col(*Laser.m_pInner);
						U.Paint(U.Row(Beam), [Outline, Inner](CXui &Ui, const SRect &Rc) {
							const float H = Rc.m_H;
							Ui.DrawRect({Rc.m_X, Rc.m_Y + H * 0.2f, Rc.m_W, H * 0.6f}, CXui::SolidRect(Outline, H * 0.3f));
							Ui.DrawRect({Rc.m_X + 3.0f, Rc.m_Y + H * 0.35f, Rc.m_W - 6.0f, H * 0.3f}, CXui::SolidRect(Inner, H * 0.15f));
							IGraphics::SUiRect Glow = CXui::SolidRect(ColorRGBA(0, 0, 0, 0), H * 0.3f);
							Glow.m_ShadowBlur = 10.0f;
							Glow.m_Shadow = Outline.WithAlpha(0.5f);
							Ui.DrawRect({Rc.m_X, Rc.m_Y + H * 0.2f, Rc.m_W, H * 0.6f}, Glow);
						});
						U.ColorPickerButton("outline", Laser.m_pOutline);
						U.ColorPickerButton("inner", Laser.m_pInner);
					},
						First, 50.0f);
					U.PopId();
					First = false;
				}
			});
			SBox Bar;
			Bar.m_Padding = SEdges(6.0f, 4.0f, 0.0f, 4.0f);
			Bar.m_Gap = 8.0f;
			Bar.m_Cross = EAlign::CENTER;
			U.Row(Bar, [&]() {
				STextStyle H;
				H.m_Size = 12.0f;
				H.m_Color = XuiTheme::Color::TEXT3;
				U.Text(Localize("Left swatch is the outline, right one the core"), H);
				U.Spacer();
				if(U.Button(Localize("Use rifle colours for all"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::COPY, .m_Height = 32.0f, .m_FontSize = 12.5f, .m_pKey = "laser.copy"}).m_Clicked)
				{
					for(const SLaser &Laser : aLasers)
					{
						*Laser.m_pOutline = g_Config.m_ClLaserRifleOutlineColor;
						*Laser.m_pInner = g_Config.m_ClLaserRifleInnerColor;
					}
				}
			});
		}
	}

	void BuildHud(SSection &S)
	{
		S.Header("Layout");
		S.Action("HUD editor", "Edit", []() { Ui().m_HudEditor = true; }).Desc("Drag modules anywhere, they snap to edges and to each other");

		S.Header("In game");
		S.Toggle("Show HUD", &g_Config.m_ClShowhud).Desc("Master switch for everything below");
		S.Toggle("Votes window after voting", &g_Config.m_ClShowVotesAfterVoting);

		S.Header("DDRace");
		S.Toggle("Jumps indicator", &g_Config.m_ClShowhudJumpsIndicator).Show([]() { return g_Config.m_ClShowhudDDRace != 0; });
		S.Toggle("Dummy actions", &g_Config.m_ClShowhudDummyActions);
		S.Slider("Freeze bars inside freeze", &g_Config.m_ClFreezeBarsAlphaInsideFreeze, 0, 100, "%").Show([]() { return g_Config.m_ClShowFreezeBars != 0; });

		S.Header("Scoreboard");
		S.Toggle("Client IDs", &g_Config.m_ClShowIds).Desc("In the scoreboard, chat and spectator menu");
		S.Custom("Name colours", []() { ColorChips("sb.colors", {{"Authed", &g_Config.m_ClAuthedPlayerColor}, {"Same clan", &g_Config.m_ClSameClanColor}}); });

		S.Header("Kill feed");
		S.Toggle("Kill messages", &g_Config.m_ClShowKillMessages);
		S.Toggle("Finish messages", &g_Config.m_ClShowFinishMessages);
		S.Custom("Colours", []() { ColorChips("kill.colors", {{"Normal", &g_Config.m_ClKillMessageNormalColor}, {"Highlight", &g_Config.m_ClKillMessageHighlightColor}}); });
	}

	void BuildChat(SSection &S)
	{
		S.m_Preview = []() {
			STextStyle H;
			H.m_Size = 12.0f;
			H.m_Font = EFont::SEMIBOLD;
			H.m_Color = XuiTheme::Color::TEXT3;
			Ui().Text(Localize("Live preview"), H);
			ChatPreview();
		};
		S.Header("Visibility");
		S.Segment("Show chat", &g_Config.m_ClShowChat, {"Off", "On", "Always"}).Desc("Always keeps the large chat area open");
		S.Toggle("System messages", &g_Config.m_ClShowChatSystem);
		S.Toggle("Only friends", &g_Config.m_ClShowChatFriends);
		S.Toggle("Only team members", &g_Config.m_ClShowChatTeamMembersOnly);

		S.Header("Style");
		S.Toggle("Names in team colours", &g_Config.m_ClChatTeamColors);
		S.Toggle("Friend heart", &g_Config.m_ClMessageFriend);
		S.Toggle("Classic look", &g_Config.m_ClChatOld).Desc("No tees, no background").OnChange([]() { Game()->m_Chat.RebuildChat(); });
		S.Slider("Font size", &g_Config.m_ClChatFontSize, 10, 100).OnChange([]() {
			Game()->m_Chat.EnsureCoherentWidth();
			Game()->m_Chat.RebuildChat();
		});
		S.Slider("Width", &g_Config.m_ClChatWidth, 120, 400).OnChange([]() {
			Game()->m_Chat.EnsureCoherentFontSize();
			Game()->m_Chat.RebuildChat();
		});
		S.Color("Background", &g_Config.m_ClChatBackgroundColor);

		S.Header("Message colours");
		S.Block("Message colours", []() {
			CXui &U = Ui();
			U.Group([&]() {
				struct SEntry
				{
					const char *m_pLabel;
					unsigned *m_pColor;
				};
				const SEntry aEntries[] = {
					{"Normal", &g_Config.m_ClMessageColor},
					{"Team", &g_Config.m_ClMessageTeamColor},
					{"Highlighted", &g_Config.m_ClMessageHighlightColor},
					{"Friends", &g_Config.m_ClMessageFriendColor},
					{"System", &g_Config.m_ClMessageSystemColor},
					{"Client echo", &g_Config.m_ClMessageClientColor},
				};
				for(int Start = 0; Start < 6; Start += 3)
				{
					U.GroupRow([&]() {
						for(int i = Start; i < Start + 3; i++)
						{
							SBox C;
							C.m_W = Grow();
							C.m_Gap = 10.0f;
							C.m_Cross = EAlign::CENTER;
							U.PushId((uint32_t)i + 1);
							U.Row(C, [&]() {
								U.ColorPickerButton("c", aEntries[i].m_pColor);
								STextStyle L;
								L.m_Size = 14.0f;
								L.m_Font = EFont::MEDIUM;
								L.m_Color = Col(*aEntries[i].m_pColor);
								U.Text(Localize(aEntries[i].m_pLabel), L);
							});
							U.PopId();
						}
					},
						Start == 0, 52.0f);
				}
			});
		});
	}

	void BuildNamePlates(SSection &S)
	{
		S.Header("Name plates");
		S.VirtualSegment(
			"Show", []() { return (g_Config.m_ClNamePlates ? 2 : 0) + (g_Config.m_ClNamePlatesOwn ? 1 : 0); }, [](int V) {
			g_Config.m_ClNamePlates = V & 2 ? 1 : 0;
			g_Config.m_ClNamePlatesOwn = V & 1 ? 1 : 0; },
			{"None", "Own", "Others", "All"});
		S.Slider("Size", &g_Config.m_ClNamePlatesSize, -50, 100);
		S.Slider("Offset", &g_Config.m_ClNamePlatesOffset, 10, 50);
		S.Toggle("Team colours", &g_Config.m_ClNamePlatesTeamcolors);
		S.Toggle("Friend heart", &g_Config.m_ClNamePlatesFriendMark);
		S.Toggle("Clan above the name", &g_Config.m_ClNamePlatesClan);
		S.Slider("Clan size", &g_Config.m_ClNamePlatesClanSize, -50, 100).Show([]() { return g_Config.m_ClNamePlatesClan != 0; });

		S.Header("Extras");
		S.VirtualSegment(
			"Client IDs", []() { return g_Config.m_ClNamePlatesIds ? (g_Config.m_ClNamePlatesIdsSeparateLine ? 2 : 1) : 0; }, [](int V) {
			g_Config.m_ClNamePlatesIds = V > 0;
			g_Config.m_ClNamePlatesIdsSeparateLine = V == 2; },
			{"Off", "Inline", "Own line"});
		S.Slider("ID size", &g_Config.m_ClNamePlatesIdsSize, -50, 100).Show([]() { return g_Config.m_ClNamePlatesIds > 0 && g_Config.m_ClNamePlatesIdsSeparateLine > 0; });
		S.Segment("Hook strength", &g_Config.m_ClNamePlatesStrong, {"Off", "Icon", "Number"});
		S.Slider("Hook strength size", &g_Config.m_ClNamePlatesStrongSize, -50, 100).Show([]() { return g_Config.m_ClNamePlatesStrong > 0; });
		S.Segment("Key presses", &g_Config.m_ClShowDirection, {"Off", "Others", "Everyone", "Own"});
		S.Slider("Key press size", &g_Config.m_ClDirectionSize, -50, 100).Show([]() { return g_Config.m_ClShowDirection > 0; });

		S.Header("Lasers");
		S.Block("Lasers", []() { LaserTable(); });
	}
}
