#include <base/math.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/font_icons_phosphor.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include <generated/client_data.h>

#include <game/client/gameclient.h>
#include <game/client/xui/xui.h>
#include <game/localization.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

using namespace Xui;

namespace
{
	enum
	{
		MOD_STATE = 0,
		MOD_SCORE,
		MOD_CLOCK,
		MOD_FPS,
		MOD_PING,
		MOD_PRED,
		MOD_SPEED,
		MOD_POSITION,
		MOD_CHAT,
		MOD_ISLAND,
		MOD_SPLITS,
		MOD_VOTE,
		MOD_BROADCAST,
		MOD_ANGLE,
		NUM_MODULES,
	};

	struct SModuleDef
	{
		const char *m_pId;
		const char *m_pName;
		const char *m_pIcon;
		vec2 m_Default;
		int *m_pEnabled;
	};

	const std::array<SModuleDef, NUM_MODULES> &Modules()
	{
		static const std::array<SModuleDef, NUM_MODULES> s_aModules = {{
			{"state", "Weapons and jumps", Ph::SWORD, vec2(0.0f, 0.0f), &g_Config.m_ClShowhudDDRace},
			{"score", "Ranking", Ph::TROPHY, vec2(1.0f, 0.55f), &g_Config.m_ClShowhudScore},
			{"clock", "Clock", Ph::CLOCK, vec2(1.0f, 0.0f), &g_Config.m_ClShowLocalTimeAlways},
			{"fps", "FPS", Ph::GAUGE, vec2(0.86f, 0.0f), &g_Config.m_ClShowfps},
			{"ping", "Ping", Ph::WIFI_HIGH, vec2(0.78f, 0.0f), &g_Config.m_XxHudPing},
			{"pred", "Prediction", Ph::LIGHTNING, vec2(0.7f, 0.0f), &g_Config.m_ClShowpred},
			{"speed", "Speed", Ph::WIND, vec2(0.42f, 1.0f), &g_Config.m_ClShowhudPlayerSpeed},
			{"position", "Position", Ph::CROSSHAIR, vec2(0.58f, 1.0f), &g_Config.m_ClShowhudPlayerPosition},
			{"chat", "Chat", Ph::CHAT_CIRCLE, vec2(0.0f, 1.0f), &g_Config.m_ClShowChat},
			{"island", "Island and timer", Ph::TIMER, vec2(0.5f, 0.0f), &g_Config.m_XxIsland},
			{"splits", "Checkpoint splits", Ph::FLAG, vec2(0.5f, 0.12f), &g_Config.m_ClShowhudTimeCpDiff},
			{"vote", "Vote", Ph::CHECK_SQUARE, vec2(0.0f, 0.55f), &g_Config.m_XxHudVote},
			{"broadcast", "Broadcast", Ph::MEGAPHONE, vec2(0.5f, 0.74f), &g_Config.m_ClShowBroadcasts},
			{"angle", "Aim angle", Ph::COMPASS, vec2(0.66f, 1.0f), &g_Config.m_XxHudAngle},
		}};
		return s_aModules;
	}

	std::array<vec2, NUM_MODULES> s_aPos;

	float AimAngle(CGameClient *pGame)
	{
		if(pGame->Client()->State() == IClient::STATE_DEMOPLAYBACK || pGame->m_Snap.m_SpecInfo.m_Active)
		{
			const int Id = pGame->m_Snap.m_SpecInfo.m_Active ? pGame->m_Snap.m_SpecInfo.m_SpectatorId : pGame->m_Snap.m_LocalClientId;
			if(Id < 0 || Id >= MAX_CLIENTS || !pGame->m_Snap.m_aCharacters[Id].m_Active)
				return 0.0f;
			const auto &Character = pGame->m_Snap.m_aCharacters[Id];
			const float Radians = pGame->m_Players.GetPlayerTargetAngle(&Character.m_Prev, &Character.m_Cur, Id);
			return std::fmod(-Radians * 180.0f / pi + 720.0f, 360.0f);
		}
		const vec2 Mouse = pGame->m_Controls.m_aMousePos[g_Config.m_ClDummy];
		if(length(Mouse) < 0.001f)
			return 0.0f;
		const float Angle = std::atan2(-Mouse.y, Mouse.x) * 180.0f / pi;
		return Angle < 0.0f ? Angle + 360.0f : Angle;
	}
	char s_aLoadedLayout[sizeof(g_Config.m_XxHudLayout)] = "\x01";
	int s_Dragging = -1;
	vec2 s_DragOffset;
	std::vector<std::pair<bool, float>> s_vGuides;

	void LoadLayout()
	{
		if(str_comp(s_aLoadedLayout, g_Config.m_XxHudLayout) == 0)
			return;
		str_copy(s_aLoadedLayout, g_Config.m_XxHudLayout);
		for(int i = 0; i < NUM_MODULES; i++)
			s_aPos[i] = Modules()[i].m_Default;
		const char *p = g_Config.m_XxHudLayout;
		while(*p)
		{
			char aId[32];
			float X, Y;
			int Consumed = 0;
			if(sscanf(p, "%31[^:]:%f:%f%n", aId, &X, &Y, &Consumed) != 3 || Consumed <= 0)
				break;
			for(int i = 0; i < NUM_MODULES; i++)
				if(str_comp(aId, Modules()[i].m_pId) == 0)
					s_aPos[i] = vec2(std::clamp(X, 0.0f, 1.0f), std::clamp(Y, 0.0f, 1.0f));
			p += Consumed;
			if(*p == ';')
				p++;
		}
	}

	void SaveLayout()
	{
		char aBuf[sizeof(g_Config.m_XxHudLayout)] = "";
		for(int i = 0; i < NUM_MODULES; i++)
		{
			char aPart[64];
			str_format(aPart, sizeof(aPart), "%s%s:%.3f:%.3f", i ? ";" : "", Modules()[i].m_pId, s_aPos[i].x, s_aPos[i].y);
			str_append(aBuf, aPart);
		}
		str_copy(g_Config.m_XxHudLayout, aBuf);
		str_copy(s_aLoadedLayout, aBuf);
	}

	const ColorRGBA s_aMedalGold = XuiTheme::Rgb(0xFFD60A);

	ColorRGBA PingColor(int Ping)
	{
		if(Ping < 80)
			return XuiTheme::Color::GREEN;
		if(Ping < 150)
			return XuiTheme::Color::YELLOW;
		return XuiTheme::Color::RED_SOFT;
	}
}

void CXui::HudPill(const char *pIcon, const char *pText, ColorRGBA IconColor, float Width)
{
	SBox B;
	B.m_H = Px(30.0f);
	if(Width > 0.0f)
		B.m_W = Px(Width);
	B.m_Gap = 7.0f;
	B.m_Padding = SEdges(0.0f, 12.0f);
	B.m_Cross = EAlign::CENTER;
	const int Node = Row(B, [&]() {
		Icon(pIcon, 13.0f, IconColor);
		STextStyle S;
		S.m_Size = 12.5f;
		S.m_Font = EFont::SEMIBOLD;
		S.m_Tabular = true;
		Text(pText, S);
	});
	Paint(Node, [](CXui &U, const SRect &Rc) {
		IGraphics::SUiRect Bg = SolidRect(ColorRGBA(0.04f, 0.04f, 0.06f, 0.72f), Rc.m_H * 0.5f);
		Bg.m_ShadowBlur = 10.0f;
		Bg.m_Shadow = ColorRGBA(0, 0, 0, 0.25f);
		U.DrawRect(Rc, Bg);
		U.DrawRect(Rc, GlassHairline(Rc.m_H * 0.5f));
	});
}

void CXui::RenderPlayerStateModule(bool Preview)
{
	CGameClient *pGame = GameClient();
	const int ClientId = pGame->m_Snap.m_SpecInfo.m_Active && pGame->m_Snap.m_SpecInfo.m_SpectatorId >= 0 ? pGame->m_Snap.m_SpecInfo.m_SpectatorId : pGame->m_Snap.m_LocalClientId;
	const bool Live = !Preview && ClientId >= 0 && pGame->m_Snap.m_aCharacters[ClientId].m_Active;
	bool aGot[NUM_WEAPONS] = {true, true, false, false, false, false};
	int Active = WEAPON_GUN;
	int Jumps = 2, JumpsLeft = 1;
	float Ninja = 0.0f;
	int Health = 10, Armor = 6, Ammo = 7;
	const bool ShowHealth = Preview || pGame->m_GameInfo.m_HudHealthArmor;
	const bool ShowAmmo = Preview || pGame->m_GameInfo.m_HudAmmo;
	std::vector<IGraphics::CTextureHandle> vStatus;
	if(Live)
	{
		const CCharacterCore *pCore = &pGame->m_aClients[ClientId].m_Predicted;
		const CNetObj_Character *pCur = &pGame->m_aClients[ClientId].m_RenderCur;
		for(int w = 0; w < NUM_WEAPONS; w++)
			aGot[w] = pCore->m_aWeapons[w].m_Got;
		Active = pCur->m_Weapon;
		const CNetObj_Character &Snap = pGame->m_Snap.m_aCharacters[ClientId].m_Cur;
		Health = Snap.m_Health;
		Armor = Snap.m_Armor;
		Ammo = Snap.m_AmmoCount;
		if(pGame->m_Snap.m_aCharacters[ClientId].m_HasExtendedDisplayInfo)
		{
			const bool Grounded = Collision()->IsOnGround(vec2(pCur->m_X, pCur->m_Y), CCharacterCore::PhysicalSize());
			int Used = pCore->m_JumpedTotal;
			if(pCore->m_Jumps > 1)
				Used += !Grounded;
			else if(pCore->m_Jumps == 1)
				Used = pCur->m_Jumped & 2;
			else if(pCore->m_Jumps == -1)
				Used = !Grounded;
			if(pCore->m_EndlessJump && Used >= absolute(pCore->m_Jumps))
				Used = absolute(pCore->m_Jumps) - 1;
			int Unused = absolute(pCore->m_Jumps) - Used;
			if(!(pCur->m_Jumped & 2) && Unused <= 0)
				Unused = 1;
			Jumps = std::clamp(absolute(pCore->m_Jumps), 0, 10);
			JumpsLeft = std::clamp(Unused, 0, Jumps);
		}
		else
			Jumps = JumpsLeft = std::clamp(absolute(pGame->m_Snap.m_aCharacters[ClientId].m_ExtendedData.m_Jumps), 0, 10);
		if(aGot[WEAPON_NINJA])
		{
			const int Max = g_pData->m_Weapons.m_Ninja.m_Duration * Client()->GameTickSpeed() / 1000;
			Ninja = std::clamp(pCore->m_Ninja.m_ActivationTick + Max - Client()->GameTick(g_Config.m_ClDummy), 0, Max) / (float)std::max(1, Max);
		}
		const auto &Hud = pGame->m_HudSkin;
		if(pCore->m_EndlessJump)
			vStatus.push_back(Hud.m_SpriteHudEndlessJump);
		if(pCore->m_EndlessHook)
			vStatus.push_back(Hud.m_SpriteHudEndlessHook);
		if(pCore->m_Jetpack)
			vStatus.push_back(Hud.m_SpriteHudJetpack);
		if(pCore->m_Solo)
			vStatus.push_back(Hud.m_SpriteHudSolo);
		if(pCore->m_CollisionDisabled)
			vStatus.push_back(Hud.m_SpriteHudCollisionDisabled);
		if(pCore->m_HookHitDisabled)
			vStatus.push_back(Hud.m_SpriteHudHookHitDisabled);
		if(pCore->m_HammerHitDisabled)
			vStatus.push_back(Hud.m_SpriteHudHammerHitDisabled);
	}

	auto Sprite = [this](IGraphics::CTextureHandle Texture, float W, float H, float Alpha) {
		SBox B;
		B.m_W = Px(W);
		B.m_H = Px(H);
		const int Node = Row(B);
		Paint(Node, [this, Texture, Alpha](CXui &U, const SRect &Rc) {
			const float A = Alpha * U.CurrentAlpha();
			U.DrawDirect(Rc, [this, Texture, A](const SRect &Px) {
				Graphics()->TextureSet(Texture);
				Graphics()->QuadsBegin();
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, A);
				Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
				const IGraphics::CQuadItem Quad(Px.m_X, Px.m_Y, Px.m_W, Px.m_H);
				Graphics()->QuadsDrawTL(&Quad, 1);
				Graphics()->QuadsEnd();
			});
		});
		return Node;
	};

	SBox Card;
	Card.m_Gap = 6.0f;
	Card.m_Padding = SEdges(8.0f, 12.0f);
	const int CardNode = Column(Card, [&]() {
		auto Segments = [&](int Value, ColorRGBA Color) {
			SBox Bar;
			Bar.m_Gap = 2.0f;
			Bar.m_Cross = EAlign::CENTER;
			Row(Bar, [&]() {
				for(int s = 0; s < 10; s++)
				{
					SBox Seg;
					Seg.m_W = Px(10.0f);
					Seg.m_H = Px(6.0f);
					const bool Full = s < Value;
					Paint(Row(Seg), [Full, Color](CXui &U, const SRect &Rc) { U.DrawRect(Rc, SolidRect(Full ? Color : ColorRGBA(1, 1, 1, 0.1f), 2.0f)); });
				}
			});
		};
		if(ShowHealth)
		{
			SBox Bars;
			Bars.m_Gap = 4.0f;
			Bars.m_Padding = SEdges(2.0f, 4.0f, 2.0f, 4.0f);
			Column(Bars, [&]() {
				Segments(std::clamp(Health, 0, 10), XuiTheme::Rgb(0xFF453A));
				Segments(std::clamp(Armor, 0, 10), XuiTheme::Rgb(0x0A84FF));
			});
		}
		SBox Weapons;
		Weapons.m_Gap = 4.0f;
		Weapons.m_Cross = EAlign::CENTER;
		Row(Weapons, [&]() {
			for(int w = 0; w < NUM_WEAPONS; w++)
			{
				if(!aGot[w])
					continue;
				const CDataSprite *pSprite = g_pData->m_Weapons.m_aId[w].m_pSpriteBody;
				const float Aspect = pSprite && pSprite->m_H > 0 ? (float)pSprite->m_W / pSprite->m_H : 2.0f;
				const float H = 22.0f;
				SBox Slot;
				Slot.m_Padding = SEdges(4.0f, 5.0f);
				Slot.m_Gap = 4.0f;
				Slot.m_Main = EAlign::CENTER;
				Slot.m_Cross = EAlign::CENTER;
				const bool Selected = Active == w;
				const int SlotNode = Row(Slot, [&]() {
					const float SpriteW = std::min(H * Aspect, 46.0f);
					Sprite(pGame->m_GameSkin.m_aSpritePickupWeapons[w], SpriteW, SpriteW / Aspect, Selected ? 1.0f : 0.55f);
					if(Selected && ShowAmmo && w != WEAPON_HAMMER && w != WEAPON_NINJA && Ammo >= 0)
					{
						char aAmmo[16];
						str_format(aAmmo, sizeof(aAmmo), "%d", Ammo);
						STextStyle A;
						A.m_Size = 11.5f;
						A.m_Font = EFont::BOLD;
						A.m_Tabular = true;
						Text(aAmmo, A);
					}
				});
				if(Selected)
				{
					const ColorRGBA Acc = m_Accent;
					Paint(SlotNode, [Acc](CXui &U, const SRect &Rc) {
						U.DrawRect(Rc, SolidRect(ColorRGBA(1, 1, 1, 0.1f), 9.0f));
						U.DrawRect({Rc.m_X + Rc.m_W * 0.3f, Rc.Bottom() - 2.0f, Rc.m_W * 0.4f, 2.0f}, SolidRect(Acc, 1.0f));
					});
				}
			}
		});
		if(Jumps > 0 || Ninja > 0.0f || !vStatus.empty())
		{
			SBox Info;
			Info.m_Gap = 8.0f;
			Info.m_Cross = EAlign::CENTER;
			Info.m_Padding = SEdges(0.0f, 4.0f);
			Row(Info, [&]() {
				if(Jumps > 0)
				{
					SBox Dots;
					Dots.m_Gap = 4.0f;
					Dots.m_Cross = EAlign::CENTER;
					Row(Dots, [&]() {
						for(int j = 0; j < Jumps; j++)
						{
							SBox Dot;
							Dot.m_W = Px(7.0f);
							Dot.m_H = Px(7.0f);
							const bool Full = j < JumpsLeft;
							Paint(Row(Dot), [Full](CXui &U, const SRect &Rc) {
								if(Full)
									U.DrawRect(Rc, SolidRect(ColorRGBA(1, 1, 1, 0.95f), Rc.m_H * 0.5f));
								else
								{
									IGraphics::SUiRect R = SolidRect(ColorRGBA(1, 1, 1, 0.0f), Rc.m_H * 0.5f);
									R.m_BorderWidth = 1.2f;
									R.m_Border = ColorRGBA(1, 1, 1, 0.4f);
									U.DrawRect(Rc, R);
								}
							});
						}
					});
				}
				for(IGraphics::CTextureHandle Texture : vStatus)
					Sprite(Texture, 14.0f, 14.0f, 0.9f);
				if(Ninja > 0.0f)
				{
					SBox Bar;
					Bar.m_W = Px(46.0f);
					Bar.m_H = Px(4.0f);
					Paint(Row(Bar), [Ninja](CXui &U, const SRect &Rc) {
						U.DrawRect(Rc, SolidRect(ColorRGBA(1, 1, 1, 0.12f), 2.0f));
						U.DrawRect({Rc.m_X, Rc.m_Y, Rc.m_W * Ninja, Rc.m_H}, SolidRect(XuiTheme::Rgb(0xBF5AF2), 2.0f));
					});
				}
			});
		}
	});
	Paint(CardNode, [](CXui &U, const SRect &Rc) {
		IGraphics::SUiRect Bg = SolidRect(ColorRGBA(0.04f, 0.04f, 0.06f, 0.6f), 16.0f);
		Bg.m_ShadowBlur = 12.0f;
		Bg.m_Shadow = ColorRGBA(0, 0, 0, 0.22f);
		U.DrawRect(Rc, Bg);
		U.DrawRect(Rc, GlassHairline(16.0f));
	});
}

void CXui::RenderHudModules(float Alpha, bool Edit)
{
	LoadLayout();
	CGameClient *pGame = GameClient();
	const bool Online = Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK;
	const bool Preview = Edit && !Online;
	const int LocalId = pGame->m_Snap.m_LocalClientId;
	const bool HasChar = Online && LocalId >= 0 && pGame->m_Snap.m_aCharacters[LocalId].m_Active;
	const bool DDRace = Online ? pGame->m_GameInfo.m_HudDDRace : true;
	if(!Edit && Alpha < 0.01f)
		return;

	struct SPlaced
	{
		int m_Index;
		SRect m_Rect;
	};
	std::vector<SPlaced> vPlaced;
	STextStyle AngleText;
	AngleText.m_Size = 12.5f;
	AngleText.m_Font = EFont::SEMIBOLD;
	AngleText.m_Tabular = true;
	const float AngleWidth = 44.0f + TextWidth("360.0°", AngleText);
	for(int i = 0; i < NUM_MODULES; i++)
	{
		const SModuleDef &Def = Modules()[i];
		if(!*Def.m_pEnabled)
			continue;
		if(Edit && g_Config.m_XxIsland && (g_Config.m_XxIslandDock & (1 << i)) && s_Dragging != i)
			continue;
		if(!Edit)
		{
			if(i == MOD_STATE && (!HasChar || !(DDRace || pGame->m_GameInfo.m_HudHealthArmor || pGame->m_GameInfo.m_HudAmmo) || !g_Config.m_ClShowhud))
				continue;
			if((i == MOD_SPEED || i == MOD_POSITION) && !HasChar)
				continue;
			if(i == MOD_ISLAND)
				continue;
			if(g_Config.m_XxIsland && (g_Config.m_XxIslandDock & (1 << i)))
				continue;
			if(g_Config.m_XxFocus && i != MOD_STATE && i != MOD_SPLITS)
				continue;
			if(i == MOD_VOTE && (!pGame->m_Voting.IsVoting() || pGame->m_Voting.SecondsLeft() < 0 || g_Config.m_XxIsland))
				continue;
			if(i == MOD_BROADCAST && ((pGame->m_Broadcast.m_BroadcastTick - Client()->GameTick(g_Config.m_ClDummy)) <= 0 || LeftBroadcast()))
				continue;
		}
		char aKey[32];
		str_format(aKey, sizeof(aKey), "hud.%s", Def.m_pId);
		const uint32_t Id = MakeId(aKey);
		SState &St = State(Id);
		const vec2 Size(i == MOD_ANGLE ? AngleWidth : St.m_Rect.m_W, i == MOD_ANGLE ? 30.0f : St.m_Rect.m_H);
		const float Margin = 12.0f;
		const vec2 Room(std::max(1.0f, m_ScreenUnits.x - 2.0f * Margin - Size.x), std::max(1.0f, m_ScreenUnits.y - 2.0f * Margin - Size.y));
		vec2 Pos = vec2(Margin, Margin) + s_aPos[i] * Room;

		if(Edit)
		{
			const SResult R = Interact(Id);
			if(R.m_Pressed && s_Dragging < 0)
			{
				s_Dragging = i;
				s_DragOffset = Mouse() - Pos;
			}
			if(s_Dragging == i)
			{
				if(!R.m_Pressed && !Input()->KeyIsPressed(KEY_MOUSE_1))
				{
					s_Dragging = -1;
					const SRect Island = State(MakeId("hud.island", false)).m_Rect;
					const vec2 Mid = vec2(St.m_Rect.m_X + St.m_Rect.m_W * 0.5f, St.m_Rect.m_Y + St.m_Rect.m_H * 0.5f);
					const bool Dockable = i == MOD_CLOCK || i == MOD_FPS || i == MOD_PING || i == MOD_PRED || i == MOD_SPEED || i == MOD_POSITION;
					if(Dockable && g_Config.m_XxIsland && Island.m_W > 0.0f && Mid.x > Island.m_X - 20.0f && Mid.x < Island.Right() + 20.0f && Mid.y > Island.m_Y - 20.0f && Mid.y < Island.Bottom() + 20.0f)
					{
						g_Config.m_XxIslandDock |= 1 << i;
						s_aPos[i] = Modules()[i].m_Default;
					}
					SaveLayout();
				}
				else
				{
					vec2 Want = Mouse() - s_DragOffset;
					s_vGuides.clear();
					const float Snap = 10.0f;
					float BestX = Snap, BestY = Snap;
					vec2 Snapped = Want;
					auto TryX = [&](float Candidate, float Guide) {
						if(absolute(Want.x - Candidate) < BestX)
						{
							BestX = absolute(Want.x - Candidate);
							Snapped.x = Candidate;
							s_vGuides.erase(std::remove_if(s_vGuides.begin(), s_vGuides.end(), [](const auto &G) { return G.first; }), s_vGuides.end());
							s_vGuides.emplace_back(true, Guide);
						}
					};
					auto TryY = [&](float Candidate, float Guide) {
						if(absolute(Want.y - Candidate) < BestY)
						{
							BestY = absolute(Want.y - Candidate);
							Snapped.y = Candidate;
							s_vGuides.erase(std::remove_if(s_vGuides.begin(), s_vGuides.end(), [](const auto &G) { return !G.first; }), s_vGuides.end());
							s_vGuides.emplace_back(false, Guide);
						}
					};
					TryX(Margin, Margin);
					TryX(m_ScreenUnits.x - Margin - Size.x, m_ScreenUnits.x - Margin);
					TryX((m_ScreenUnits.x - Size.x) * 0.5f, m_ScreenUnits.x * 0.5f);
					TryY(Margin, Margin);
					TryY(m_ScreenUnits.y - Margin - Size.y, m_ScreenUnits.y - Margin);
					TryY((m_ScreenUnits.y - Size.y) * 0.5f, m_ScreenUnits.y * 0.5f);
					for(int o = 0; o < NUM_MODULES; o++)
					{
						if(o == i || !*Modules()[o].m_pEnabled)
							continue;
						char aOther[32];
						str_format(aOther, sizeof(aOther), "hud.%s", Modules()[o].m_pId);
						const SRect Other = State(MakeId(aOther, false)).m_Rect;
						if(Other.m_W <= 0.0f)
							continue;
						TryX(Other.m_X, Other.m_X);
						TryX(Other.Right() - Size.x, Other.Right());
						TryX(Other.Right() + 8.0f, Other.Right() + 4.0f);
						TryX(Other.m_X - 8.0f - Size.x, Other.m_X - 4.0f);
						TryY(Other.m_Y, Other.m_Y);
						TryY(Other.Bottom() - Size.y, Other.Bottom());
						TryY(Other.Bottom() + 8.0f, Other.Bottom() + 4.0f);
						TryY(Other.m_Y - 8.0f - Size.y, Other.m_Y - 4.0f);
					}
					Pos = vec2(std::clamp(Snapped.x, Margin, Margin + Room.x), std::clamp(Snapped.y, Margin, Margin + Room.y));
					s_aPos[i] = vec2(std::clamp((Pos.x - Margin) / Room.x, 0.0f, 1.0f), std::clamp((Pos.y - Margin) / Room.y, 0.0f, 1.0f));
				}
			}
		}

		if(!Edit && Size.x > 0.0f)
		{
			const bool Up = s_aPos[i].y > 0.5f;
			for(int Pass = 0; Pass < 4; Pass++)
			{
				bool Moved = false;
				for(const SPlaced &Other : vPlaced)
				{
					const SRect &R = Other.m_Rect;
					if(Pos.x < R.Right() && Pos.x + Size.x > R.m_X && Pos.y < R.Bottom() && Pos.y + Size.y > R.m_Y)
					{
						Pos.y = Up ? R.m_Y - Size.y - 6.0f : R.Bottom() + 6.0f;
						Moved = true;
					}
				}
				if(!Moved)
					break;
			}
			Pos.y = std::clamp(Pos.y, Margin, std::max(Margin, m_ScreenUnits.y - Margin - Size.y));
		}

		SBox B;
		B.m_Position = EPos::ABSOLUTE;
		B.m_Left = Pos.x;
		B.m_Top = Pos.y;
		B.m_Hit = Edit ? EHit::INTERACTIVE : EHit::NONE;
		B.m_Cross = s_aPos[i].x > 0.66f ? EAlign::END : EAlign::START;
		B.m_Gap = 6.0f;
		const int Node = BoxId(Id, EDir::COLUMN, B, [&]() {
			char aBuf[64];
			switch(i)
			{
			case MOD_STATE:
				RenderPlayerStateModule(Preview || !HasChar);
				break;

			case MOD_SCORE:
				if(Preview)
				{
					SBox Col;
					Col.m_Gap = 6.0f;
					Col.m_Cross = EAlign::END;
					Column(Col, [&]() {
						HudPill(Ph::TROPHY, "1   president   37.44", s_aMedalGold);
						HudPill(Ph::USER, "8   you   47.42", XuiTheme::Color::TEXT2);
					});
				}
				else
					RenderScoreHud(1.0f);
				break;
			case MOD_CLOCK:
				str_timestamp_format(aBuf, sizeof(aBuf), "%H:%M");
				HudPill(Ph::CLOCK, aBuf, XuiTheme::Color::TEXT2);
				break;
			case MOD_FPS:
				str_format(aBuf, sizeof(aBuf), "%d", round_to_int(1.0f / std::max(0.0001f, Client()->FrameTimeAverage())));
				HudPill(Ph::GAUGE, aBuf, XuiTheme::Color::TEXT2);
				break;
			case MOD_PING:
			{
				const int Ping = Online && pGame->m_Snap.m_pLocalInfo ? pGame->m_Snap.m_pLocalInfo->m_Latency : 24;
				str_format(aBuf, sizeof(aBuf), "%d ms", Ping);
				HudPill(Ph::WIFI_HIGH, aBuf, PingColor(Ping));
				break;
			}
			case MOD_PRED:
				str_format(aBuf, sizeof(aBuf), "%d ms", Online ? Client()->GetPredictionTime() : 40);
				HudPill(Ph::LIGHTNING, aBuf, XuiTheme::Color::TEXT2);
				break;
			case MOD_SPEED:
			{
				const float Speed = HasChar ? length(pGame->m_aClients[LocalId].m_Predicted.m_Vel) * 50.0f / 32.0f : 12.4f;
				str_format(aBuf, sizeof(aBuf), "%.1f", Speed);
				HudPill(Ph::WIND, aBuf, XuiTheme::Color::TEXT2);
				break;
			}
			case MOD_ANGLE:
				str_format(aBuf, sizeof(aBuf), "%.1f°", Preview ? 45.0f : AimAngle(pGame));
				HudPill(Ph::COMPASS, aBuf, XuiTheme::Color::TEXT2, AngleWidth);
				break;
			case MOD_POSITION:
			{
				const vec2 P = HasChar ? pGame->m_aClients[LocalId].m_RenderPos / 32.0f : vec2(120.5f, 42.0f);
				str_format(aBuf, sizeof(aBuf), "%.1f  %.1f", P.x, P.y);
				HudPill(Ph::CROSSHAIR, aBuf, XuiTheme::Color::TEXT2);
				break;
			}
			case MOD_CHAT:
				RenderChatModule(Preview, Edit);
				break;
			case MOD_SPLITS:
				RenderSplitsModule(Preview || (Edit && !Online));
				break;
			case MOD_VOTE:
				RenderVoteModule(Preview || (Edit && !pGame->m_Voting.IsVoting()));
				break;
			case MOD_BROADCAST:
				RenderBroadcastModule(Preview || (Edit && (pGame->m_Broadcast.m_BroadcastTick - Client()->GameTick(g_Config.m_ClDummy)) <= 0));
				break;

			case MOD_ISLAND:
			{
				SBox Pill;
				Pill.m_H = Px(38.0f);
				Pill.m_Gap = 8.0f;
				Pill.m_Padding = SEdges(0.0f, 18.0f);
				Pill.m_Cross = EAlign::CENTER;
				const int PillNode = Row(Pill, [&]() {
					Icon(Ph::FLAG_CHECKERED, 14.0f, ColorRGBA(1, 1, 1, 1), true);
					STextStyle S;
					S.m_Size = 13.5f;
					S.m_Font = EFont::BOLD;
					S.m_Tabular = true;
					S.m_Color = ColorRGBA(1, 1, 1, 1);
					S.m_PreserveColor = true;
					char aIsland[128] = "01:23.45";
					char aExtra[96];
					if(IslandExtras(aExtra, sizeof(aExtra)))
					{
						str_append(aIsland, "  ·  ");
						str_append(aIsland, aExtra);
					}
					Text(aIsland, S);
				});
				Paint(PillNode, [](CXui &U, const SRect &Rc) {
					IGraphics::SUiRect Glass = GlassRect(Rc.m_H * 0.5f, 0.74f);
					Glass.m_Fill = ColorRGBA(0.05f, 0.05f, 0.07f, 0.78f);
					Glass.m_Fill2 = ColorRGBA(0.03f, 0.03f, 0.05f, 0.72f);
					U.DrawRect(Rc, Glass);
				});
				break;
			}
			}
		});
		SetTransform(Node, 1.0f, Edit || i == MOD_CHAT ? 1.0f : Alpha);
		if(Edit)
		{
			const bool Drag = s_Dragging == i;
			const ColorRGBA Acc = m_Accent;
			const float Hover = St.m_Hover.Update(Drag || (Mouse().x >= St.m_Rect.m_X && Mouse().x <= St.m_Rect.Right() && Mouse().y >= St.m_Rect.m_Y && Mouse().y <= St.m_Rect.Bottom()) ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
			Paint(Node, [Acc, Hover](CXui &U, const SRect &Rc) {
				IGraphics::SUiRect R{};
				R.m_Radii = Rad(14.0f);
				R.m_Fill = WithAlpha(Acc, 0.06f + 0.1f * Hover);
				R.m_BorderWidth = 1.5f;
				R.m_Border = WithAlpha(Acc, 0.45f + 0.45f * Hover);
				U.DrawRect(Rc.Inset(-5.0f), R);
			});
		}
		vPlaced.push_back({i, SRect{Pos.x, Pos.y, Size.x, Size.y}});
	}
}

void CXui::RenderSplitsModule(bool Preview)
{
	CGameClient *pGame = GameClient();
	CHud &Hud = pGame->m_Hud;
	const int Tick = Client()->GameTick(g_Config.m_ClDummy);
	const int Speed = Client()->GameTickSpeed();
	auto FormatDiff = [](float Diff, char *pBuf, int Size) {
		const float Abs = absolute(Diff);
		const char Sign = Diff < 0.0f ? '-' : '+';
		if(Abs < 60.0f)
			str_format(pBuf, Size, "%c%.2f", Sign, Abs);
		else
			str_format(pBuf, Size, "%c%d:%05.2f", Sign, (int)Abs / 60, std::fmod(Abs, 60.0f));
	};
	char aMain[64] = "";
	char aDiff[32] = "";
	float Diff = 0.0f;
	float Age = 0.0f;
	bool Finish = false;
	if(Preview)
	{
		Diff = -0.42f;
		FormatDiff(Diff, aDiff, sizeof(aDiff));
		str_copy(aMain, "CP");
	}
	else if(Hud.m_ShowFinishTime && Hud.m_DDRaceTime && Hud.m_FinishTimeLastReceivedTick + Speed * 6 > Tick)
	{
		Finish = true;
		Age = (Tick - Hud.m_FinishTimeLastReceivedTick) / (float)Speed;
		str_time(Hud.m_DDRaceTime, ETimeFormat::HOURS_CENTISECS, aMain, sizeof(aMain));
		Diff = Hud.m_FinishTimeDiff;
		if(Diff != 0.0f)
			FormatDiff(Diff, aDiff, sizeof(aDiff));
	}
	else if(g_Config.m_ClShowhudTimeCpDiff && Hud.m_TimeCpLastReceivedTick + Speed * 6 > Tick && Hud.m_TimeCpDiff != 0.0f)
	{
		Age = (Tick - Hud.m_TimeCpLastReceivedTick) / (float)Speed;
		Diff = Hud.m_TimeCpDiff;
		FormatDiff(Diff, aDiff, sizeof(aDiff));
		str_copy(aMain, "CP");
	}
	else
		return;
	const float Alpha = std::clamp(6.0f - Age, 0.0f, 1.0f);
	const float Pop = 1.0f + 0.12f * std::clamp(1.0f - Age * 5.0f, 0.0f, 1.0f);
	const ColorRGBA DiffColor = Diff < 0.0f ? XuiTheme::Color::GREEN : XuiTheme::Color::RED_SOFT;
	SBox B;
	B.m_H = Px(36.0f);
	B.m_Gap = 9.0f;
	B.m_Padding = SEdges(0.0f, 16.0f);
	B.m_Cross = EAlign::CENTER;
	const int Node = Row(B, [&]() {
		Icon(Finish ? Ph::FLAG_CHECKERED : Ph::FLAG, 15.0f, Finish ? s_aMedalGold : XuiTheme::Color::TEXT2, true);
		STextStyle S;
		S.m_Size = 14.0f;
		S.m_Font = EFont::BOLD;
		S.m_Tabular = true;
		S.m_Color = Finish ? XuiTheme::Color::TEXT : XuiTheme::Color::TEXT3;
		Text(aMain, S);
		if(aDiff[0] != '\0')
		{
			STextStyle D = S;
			D.m_Size = 16.0f;
			D.m_Font = EFont::EXTRABOLD;
			D.m_Color = DiffColor;
			Text(aDiff, D);
		}
	});
	SetTransform(Node, Pop, Alpha);
	Paint(Node, [DiffColor](CXui &U, const SRect &Rc) {
		IGraphics::SUiRect Bg = SolidRect(ColorRGBA(0.04f, 0.04f, 0.06f, 0.78f), Rc.m_H * 0.5f);
		Bg.m_BorderWidth = 1.2f;
		Bg.m_Border = WithAlpha(DiffColor, 0.45f);
		Bg.m_ShadowBlur = 14.0f;
		Bg.m_Shadow = WithAlpha(DiffColor, 0.18f);
		U.DrawRect(Rc, Bg);
	});
}

void CXui::RenderVoteModule(bool Preview)
{
	CVoting &Voting = GameClient()->m_Voting;
	const char *pDesc = Preview ? "Change map: Kobra 4" : Voting.VoteDescription();
	const char *pReason = Preview ? "" : Voting.VoteReason();
	const int Seconds = Preview ? 24 : std::max(0, Voting.SecondsLeft());
	const float Left = Preview ? 0.8f : 1.0f - Voting.ElapsedFraction();
	const int Yes = Preview ? 5 : Voting.Yes();
	const int No = Preview ? 2 : Voting.No();
	const int Total = std::max(Preview ? 9 : Voting.Total(), Yes + No);
	const int Taken = Preview ? 0 : Voting.TakenChoice();
	if(str_find_nocase(pReason, "f3") || str_find_nocase(pReason, "f4"))
		pReason = "";
	SBox Card;
	Card.m_W = Px(290.0f);
	Card.m_Gap = 7.0f;
	Card.m_Padding = SEdges(10.0f, 12.0f);
	const int CardNode = Column(Card, [&]() {
		SBox Head;
		Head.m_W = Grow();
		Head.m_Gap = 10.0f;
		Head.m_Cross = EAlign::CENTER;
		Row(Head, [&]() {
			char aSec[8];
			str_format(aSec, sizeof(aSec), "%d", Seconds);
			RingProgress(Left, 28.0f, aSec);
			SBox Names;
			Names.m_W = Grow();
			Names.m_Shrink = true;
			Names.m_Gap = 2.0f;
			Column(Names, [&]() {
				STextStyle T;
				T.m_Size = 13.5f;
				T.m_Font = EFont::BOLD;
				T.m_Wrap = true;
				T.m_MaxLines = 2;
				SBox GB;
				GB.m_W = Grow();
				Text(pDesc, T, GB);
				if(pReason[0] != '\0')
				{
					STextStyle R;
					R.m_Size = 11.5f;
					R.m_Color = XuiTheme::Color::TEXT2;
					R.m_Wrap = true;
					R.m_MaxLines = 1;
					Text(pReason, R, GB);
				}
			});
		});
		const float YesF = Total > 0 ? (float)Yes / Total : 0.0f;
		const float NoF = Total > 0 ? (float)No / Total : 0.0f;
		SBox Bar;
		Bar.m_W = Grow();
		Bar.m_H = Px(4.0f);
		Paint(Row(Bar), [YesF, NoF](CXui &U, const SRect &Rc) {
			U.DrawRect(Rc, SolidRect(ColorRGBA(1, 1, 1, 0.08f), Rc.m_H * 0.5f));
			if(YesF > 0.0f)
				U.DrawRect({Rc.m_X, Rc.m_Y, std::max(Rc.m_H, Rc.m_W * YesF), Rc.m_H}, SolidRect(XuiTheme::Color::GREEN, Rc.m_H * 0.5f));
			if(NoF > 0.0f)
			{
				const float W = std::max(Rc.m_H, Rc.m_W * NoF);
				U.DrawRect({Rc.Right() - W, Rc.m_Y, W, Rc.m_H}, SolidRect(XuiTheme::Color::RED_SOFT, Rc.m_H * 0.5f));
			}
		});
		SBox Keys;
		Keys.m_W = Grow();
		Keys.m_Gap = 6.0f;
		Row(Keys, [&]() {
			auto Key = [&](const char *pKey, const char *pLabel, int Count, bool Chosen, ColorRGBA Color) {
				SBox K;
				K.m_W = Grow();
				K.m_H = Px(24.0f);
				K.m_Gap = 6.0f;
				K.m_Main = EAlign::CENTER;
				K.m_Cross = EAlign::CENTER;
				const int Node = Row(K, [&]() {
					STextStyle KS;
					KS.m_Size = 10.5f;
					KS.m_Font = EFont::BOLD;
					KS.m_Color = XuiTheme::Color::TEXT3;
					Text(pKey, KS);
					STextStyle L;
					L.m_Size = 12.0f;
					L.m_Font = EFont::BOLD;
					L.m_Color = Chosen ? Color : XuiTheme::Color::TEXT;
					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "%s  %d", pLabel, Count);
					L.m_Tabular = true;
					Text(aBuf, L);
				});
				Paint(Node, [Chosen, Color](CXui &U, const SRect &Rc) {
					IGraphics::SUiRect R = SolidRect(Chosen ? WithAlpha(Color, 0.2f) : XuiTheme::Color::GHOST, Rc.m_H * 0.5f);
					if(Chosen)
					{
						R.m_BorderWidth = 1.0f;
						R.m_Border = WithAlpha(Color, 0.6f);
					}
					U.DrawRect(Rc, R);
				});
			};
			Key("F3", Localize("Yes"), Yes, Taken == 1, XuiTheme::Color::GREEN);
			Key("F4", Localize("No"), No, Taken == -1, XuiTheme::Color::RED_SOFT);
		});
	});
	Paint(CardNode, [](CXui &U, const SRect &Rc) {
		IGraphics::SUiRect Bg = SolidRect(ColorRGBA(0.04f, 0.04f, 0.06f, 0.78f), 18.0f);
		Bg.m_ShadowBlur = 14.0f;
		Bg.m_Shadow = ColorRGBA(0, 0, 0, 0.25f);
		U.DrawRect(Rc, Bg);
		U.DrawRect(Rc, GlassHairline(18.0f));
	});
}

static bool ParseBroadcast(const char *pRaw, char *pOut, int Size)
{
	int n = 0;
	int Trailing = 0;
	int MaxTrailing = 0;
	for(const char *p = pRaw; *p && n < Size - 1; p++)
	{
		if(p[0] == '^' && p[1] >= '0' && p[1] <= '9' && p[2] >= '0' && p[2] <= '9' && p[3] >= '0' && p[3] <= '9')
		{
			p += 3;
			continue;
		}
		if(*p == '\n')
		{
			MaxTrailing = std::max(MaxTrailing, Trailing);
			Trailing = 0;
			while(n > 0 && pOut[n - 1] == ' ')
				n--;
		}
		else if(*p == ' ')
			Trailing++;
		else
			Trailing = 0;
		pOut[n++] = *p;
	}
	MaxTrailing = std::max(MaxTrailing, Trailing);
	while(n > 0 && (pOut[n - 1] == ' ' || pOut[n - 1] == '\n'))
		n--;
	pOut[n] = '\0';
	return MaxTrailing >= 12;
}

bool CXui::LeftBroadcast()
{
	CBroadcast &Broadcast = GameClient()->m_Broadcast;
	if((Broadcast.m_BroadcastTick - Client()->GameTick(g_Config.m_ClDummy)) <= 0)
		return false;
	char aText[1024];
	return ParseBroadcast(Broadcast.m_aBroadcastText, aText, sizeof(aText));
}

void CXui::RenderLeftBroadcast()
{
	CBroadcast &Broadcast = GameClient()->m_Broadcast;
	if(!g_Config.m_ClShowBroadcasts || !LeftBroadcast())
		return;
	char aText[1024];
	ParseBroadcast(Broadcast.m_aBroadcastText, aText, sizeof(aText));
	const char *pStart = str_utf8_skip_whitespaces(aText);
	if(pStart[0] == '\0')
		return;
	const float Left = (Broadcast.m_BroadcastTick - Client()->GameTick(g_Config.m_ClDummy)) / (float)Client()->GameTickSpeed();
	SBox Wrap;
	Wrap.m_Position = EPos::ABSOLUTE;
	Wrap.m_Left = 14.0f;
	Wrap.m_Top = m_ScreenUnits.y * 0.36f;
	const int Node = Column(Wrap, [&]() {
		SBox B;
		B.m_Padding = SEdges(8.0f, 14.0f);
		B.m_W = Fit(0.0f, 520.0f);
		const int Inner = Column(B, [&]() {
			STextStyle S;
			S.m_Size = 16.0f;
			S.m_Font = EFont::BOLD;
			S.m_Wrap = true;
			Text(pStart, S);
		});
		Paint(Inner, [](CXui &U, const SRect &Rc) {
			U.DrawRect(Rc, SolidRect(ColorRGBA(0.04f, 0.04f, 0.06f, 0.62f), 14.0f));
			U.DrawRect(Rc, GlassHairline(14.0f));
		});
	});
	SetTransform(Node, 1.0f, std::clamp(Left, 0.0f, 1.0f));
}

void CXui::RenderBroadcastModule(bool Preview)
{
	CBroadcast &Broadcast = GameClient()->m_Broadcast;
	char aText[1024];
	float Alpha = 1.0f;
	if(Preview)
		str_copy(aText, "Welcome to Kobra 4\nGood luck!");
	else
	{
		const float Left = (Broadcast.m_BroadcastTick - Client()->GameTick(g_Config.m_ClDummy)) / (float)Client()->GameTickSpeed();
		Alpha = std::clamp(Left, 0.0f, 1.0f);
		ParseBroadcast(Broadcast.m_aBroadcastText, aText, sizeof(aText));
	}
	const char *pStart = str_utf8_skip_whitespaces(aText);
	if(pStart[0] == '\0')
		return;
	SBox B;
	B.m_Padding = SEdges(10.0f, 18.0f);
	B.m_Cross = EAlign::CENTER;
	B.m_W = Fit(0.0f, 620.0f);
	const int Node = Column(B, [&]() {
		STextStyle S;
		S.m_Size = 15.0f;
		S.m_Font = EFont::SEMIBOLD;
		S.m_Wrap = true;
		S.m_Align = ETextAlign::CENTER;
		Text(pStart, S);
	});
	SetTransform(Node, 1.0f, Alpha);
	Paint(Node, [](CXui &U, const SRect &Rc) {
		U.DrawRect(Rc, SolidRect(ColorRGBA(0.04f, 0.04f, 0.06f, 0.7f), std::min(22.0f, Rc.m_H * 0.5f)));
		U.DrawRect(Rc, GlassHairline(std::min(22.0f, Rc.m_H * 0.5f)));
	});
}

bool CXui::IslandExtras(char *pBuf, int Size)
{
	pBuf[0] = '\0';
	if(!g_Config.m_XxIsland)
		return false;
	CGameClient *pGame = GameClient();
	const int LocalId = pGame->m_Snap.m_LocalClientId;
	const bool HasChar = LocalId >= 0 && pGame->m_Snap.m_aCharacters[LocalId].m_Active;
	for(int i = 0; i < NUM_MODULES; i++)
	{
		if(!(g_Config.m_XxIslandDock & (1 << i)) || !*Modules()[i].m_pEnabled)
			continue;
		char aPart[48] = "";
		switch(i)
		{
		case MOD_CLOCK: str_timestamp_format(aPart, sizeof(aPart), "%H:%M"); break;
		case MOD_FPS: str_format(aPart, sizeof(aPart), "%d fps", round_to_int(1.0f / std::max(0.0001f, Client()->FrameTimeAverage()))); break;
		case MOD_PING: str_format(aPart, sizeof(aPart), "%d ms", pGame->m_Snap.m_pLocalInfo ? pGame->m_Snap.m_pLocalInfo->m_Latency : 0); break;
		case MOD_PRED: str_format(aPart, sizeof(aPart), "%d ms", Client()->GetPredictionTime()); break;
		case MOD_SPEED:
			if(HasChar)
				str_format(aPart, sizeof(aPart), "%.1f", length(pGame->m_aClients[LocalId].m_Predicted.m_Vel) * 50.0f / 32.0f);
			break;
		case MOD_ANGLE:
			if(HasChar)
				str_format(aPart, sizeof(aPart), "%.1f°", AimAngle(pGame));
			break;
		case MOD_POSITION:
			if(HasChar)
				str_format(aPart, sizeof(aPart), "%.0f %.0f", pGame->m_aClients[LocalId].m_RenderPos.x / 32.0f, pGame->m_aClients[LocalId].m_RenderPos.y / 32.0f);
			break;
		}
		if(aPart[0] == '\0')
			continue;
		if(pBuf[0] != '\0')
			str_append(pBuf, "  ·  ", Size);
		str_append(pBuf, aPart, Size);
	}
	return pBuf[0] != '\0';
}

bool CXui::HudModulePos(const char *pId, vec2 *pOut)
{
	LoadLayout();
	for(int i = 0; i < NUM_MODULES; i++)
		if(str_comp(Modules()[i].m_pId, pId) == 0)
		{
			*pOut = s_aPos[i];
			return true;
		}
	return false;
}

void CXui::RenderChatModule(bool Preview, bool Edit)
{
	CChat &Chat = GameClient()->m_Chat;
	const bool Typing = !Preview && Chat.m_Mode != CChat::MODE_NONE;
	const int64_t Now = time();
	const ColorRGBA Acc = m_Accent;
	struct SMsg
	{
		int m_ClientId;
		std::string m_Name;
		std::string m_Text;
		ColorRGBA m_NameColor;
		ColorRGBA m_TextColor;
		bool m_Highlight;
		float m_Alpha;
		int m_Repeat;
		const char *m_pTag;
		std::shared_ptr<CManagedTeeRenderInfo> m_pTee;
		int m_Line = -1;
	};
	std::vector<SMsg> vMsgs;
	if(Preview)
	{
		vMsgs.push_back({-1, "", Localize("Welcome to the server!"), XuiTheme::Color::TEXT2, ColorRGBA(0.95f, 0.88f, 0.55f, 1.0f), false, 1.0f, 0, nullptr});
		vMsgs.push_back({0, "president", "gg, who wants to do Kobra next?", XuiTheme::Color::TEXT, XuiTheme::Color::TEXT, false, 1.0f, 0, nullptr});
		vMsgs.push_back({1, "kilolik69", "me, give me a sec", XuiTheme::Color::GREEN, XuiTheme::Color::TEXT, false, 1.0f, 0, Localize("Team")});
	}
	else
	{
		const int MaxLines = Typing ? 14 : 8;
		for(int i = 0; i < CChat::MAX_LINES && (int)vMsgs.size() < MaxLines; i++)
		{
			const CChat::CLine &Line = Chat.m_aLines[((Chat.m_CurrentLine - i) + CChat::MAX_LINES) % CChat::MAX_LINES];
			if(!Line.m_Initialized)
				break;
			const float Age = (Now - Line.m_Time) / (float)time_freq();
			if(Age > 16.0f && !Typing && !Edit)
				break;
			SMsg Msg;
			Msg.m_pTee = Line.m_pManagedTeeRenderInfo;
			Msg.m_Line = ((Chat.m_CurrentLine - i) + CChat::MAX_LINES) % CChat::MAX_LINES;
			Msg.m_ClientId = Line.m_ClientId;
			Msg.m_Name = Line.m_aName;
			Msg.m_Text = Line.m_aText;
			Msg.m_Alpha = Typing || Edit ? 1.0f : std::clamp(16.0f - Age, 0.0f, 1.0f);
			Msg.m_Repeat = Line.m_TimesRepeated;
			Msg.m_Highlight = Line.m_Highlighted;
			Msg.m_pTag = Line.m_Whisper ? Localize("Whisper") : (Line.m_Team ? Localize("Team chat") : nullptr);
			Msg.m_NameColor = Line.m_Friend ? XuiTheme::Color::GREEN : (Line.m_NameColor == TEAM_RED ? XuiTheme::Rgb(0xFF6961) : (Line.m_NameColor == TEAM_BLUE ? XuiTheme::Rgb(0x6CB4FF) : XuiTheme::Color::TEXT));
			if(Line.m_ClientId >= 0 && Line.m_ClientId < MAX_CLIENTS && GameClient()->m_aClients[Line.m_ClientId].m_AuthLevel)
				Msg.m_NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClAuthedPlayerColor));
			Msg.m_TextColor = Line.m_CustomColor.value_or(Line.m_ClientId == CChat::SERVER_MSG ? ColorRGBA(0.95f, 0.88f, 0.55f, 1.0f) : (Line.m_ClientId == CChat::CLIENT_MSG ? XuiTheme::Rgb(0x8FD0FF) : XuiTheme::Color::TEXT));
			vMsgs.push_back(std::move(Msg));
		}
		std::reverse(vMsgs.begin(), vMsgs.end());
	}
	if(vMsgs.empty() && !Typing && !Edit)
		return;

	float MaxAlpha = Typing || Edit ? 1.0f : 0.0f;
	for(const SMsg &Msg : vMsgs)
		MaxAlpha = std::max(MaxAlpha, Msg.m_Alpha);

	SBox Panel;
	Panel.m_W = Px((float)g_Config.m_XxChatWidth);
	Panel.m_Gap = 2.0f;
	Panel.m_Padding = SEdges(8.0f);
	const int PanelNode = Column(Panel, [&]() {
		for(size_t m = 0; m < vMsgs.size(); m++)
		{
			const SMsg &Msg = vMsgs[m];
			const bool Player = Msg.m_ClientId >= 0;
			SBox R;
			R.m_W = Grow();
			R.m_Gap = 9.0f;
			R.m_Padding = Player ? SEdges(5.0f, 8.0f) : SEdges(2.0f, 8.0f);
			R.m_Cross = Player ? EAlign::START : EAlign::CENTER;
			char aMessageKey[32];
			str_format(aMessageKey, sizeof(aMessageKey), "chat.line.%d", Msg.m_Line);
			const uint32_t MessageId = MakeId(aMessageKey);
			const SResult MessageResult = Interact(MessageId, Typing, false);
			if(MessageResult.m_RightClicked)
			{
				m_ChatContextOpen = true;
				m_ChatContextAt = Mouse();
				m_ChatContextText = Msg.m_Text;
				m_ChatContextTee = Msg.m_pTee;
				m_ChatContextName = Msg.m_Name;
				m_ChatContextClientId = Msg.m_ClientId;
			}
			R.m_Hit = Typing ? EHit::INTERACTIVE : EHit::NONE;
			const int RowNode = BoxId(MessageId, EDir::ROW, R, [&]() {
				SBox Head;
				Head.m_W = Px(24.0f);
				Head.m_H = Px(Player ? 24.0f : 18.0f);
				Head.m_Main = EAlign::CENTER;
				Head.m_Cross = EAlign::CENTER;
				if(Player && !Preview && Msg.m_pTee)
				{
					const CTeeRenderInfo TeeInfo = Msg.m_pTee->TeeRenderInfo();
					Paint(Row(Head), [TeeInfo](CXui &U, const SRect &Rc) { U.DrawTee(TeeInfo, 0, vec2(1.0f, 0.0f), Rc); });
				}
				else
					Row(Head, [&]() { Icon(Player ? Ph::USER_CIRCLE : Ph::INFO, Player ? 16.0f : 13.0f, Player ? XuiTheme::Color::TEXT2 : Msg.m_TextColor, !Player); });
				SBox Body;
				Body.m_W = Grow();
				Body.m_Shrink = true;
				Body.m_Gap = 1.0f;
				Column(Body, [&]() {
					if(Player)
					{
						SBox NameRow;
						NameRow.m_Gap = 6.0f;
						NameRow.m_Cross = EAlign::CENTER;
						Row(NameRow, [&]() {
							STextStyle N;
							N.m_Size = 12.5f;
							N.m_Font = EFont::BOLD;
							N.m_Color = Msg.m_NameColor;
							Text(Msg.m_Name.c_str(), N);
							if(Msg.m_pTag)
							{
								STextStyle T;
								T.m_Size = 10.0f;
								T.m_Font = EFont::BOLD;
								T.m_Color = XuiTheme::Color::TEXT3;
								Text(Msg.m_pTag, T);
							}
							if(Msg.m_Repeat > 0)
							{
								char aRep[16];
								str_format(aRep, sizeof(aRep), "×%d", Msg.m_Repeat + 1);
								STextStyle T;
								T.m_Size = 10.5f;
								T.m_Font = EFont::SEMIBOLD;
								T.m_Color = XuiTheme::Color::TEXT3;
								Text(aRep, T);
							}
						});
					}
					STextStyle BodyStyle;
					BodyStyle.m_Size = Player ? 14.0f : 13.0f;
					BodyStyle.m_Wrap = true;
					BodyStyle.m_Color = Msg.m_TextColor;
					SBox TB;
					TB.m_W = Grow();
					Text(Msg.m_Text.c_str(), BodyStyle, TB);
				});
			});
			SetTransform(RowNode, 1.0f, Msg.m_Alpha);
			if(Msg.m_Highlight)
				Paint(RowNode, [Acc](CXui &U, const SRect &Rc) { U.DrawRect(Rc, SolidRect(WithAlpha(Acc, 0.22f), 12.0f)); });
		}
		if(Typing || (Edit && Preview))
		{
			SBox Input;
			Input.m_W = Grow();
			Input.m_H = Px(38.0f);
			Input.m_Gap = 8.0f;
			Input.m_Cross = EAlign::CENTER;
			Input.m_Padding = SEdges(4.0f, 0.0f, 0.0f, 0.0f);
			Row(Input, [&]() {
				const bool TeamMode = !Preview && Chat.m_Mode == CChat::MODE_TEAM;
				SBox Cap;
				Cap.m_H = Px(30.0f);
				Cap.m_Padding = SEdges(0.0f, 12.0f);
				Cap.m_Cross = EAlign::CENTER;
				const int CapNode = Row(Cap, [&]() {
					STextStyle S;
					S.m_Size = 12.0f;
					S.m_Font = EFont::BOLD;
					S.m_Color = TeamMode ? XuiTheme::Color::GREEN : XuiTheme::Color::TEXT;
					Text(TeamMode ? Localize("Team") : Localize("All"), S);
				});
				Paint(CapNode, [](CXui &U, const SRect &Rc) { U.DrawRect(Rc, SolidRect(XuiTheme::Color::GHOST, Rc.m_H * 0.5f)); });
				SBox Field;
				Field.m_W = Grow();
				Field.m_H = Px(30.0f);
				Field.m_Clip = true;
				const int FieldNode = Row(Field);
				const bool Empty = Preview || Chat.m_Input.GetString()[0] == '\0';
				Paint(FieldNode, [this, &Chat, Preview, Empty](CXui &U, const SRect &Rc) {
					U.DrawRect(Rc, SolidRect(ColorRGBA(1, 1, 1, 0.07f), Rc.m_H * 0.5f));
					const SRect Inner = Rc.Inset(12.0f, 0.0f);
					if(Empty)
					{
						STextStyle P;
						P.m_Size = 13.5f;
						P.m_Color = XuiTheme::Color::TEXT3;
						U.DrawText(Localize("Message, Tab to complete names"), P, Inner);
					}
					if(Preview)
						return;
					const float Scale = U.CurrentXf().Scale();
					U.DrawDirect(Inner, [this, &Chat, Scale](const SRect &Px) {
						static float s_Scroll = 0.0f;
						const bool Changed = Chat.m_Input.WasChanged() || Chat.m_Input.WasCursorChanged();
						TextRender()->SetFontPreset(EFontPreset::UI_REGULAR);
						TextRender()->TextColor(XuiTheme::Color::TEXT);
						CUIRect Rect;
						Rect.x = Px.m_X - s_Scroll;
						Rect.y = Px.m_Y;
						Rect.w = Px.m_W + s_Scroll;
						Rect.h = Px.m_H;
						Chat.m_Input.Render(&Rect, 13.5f * Scale, TEXTALIGN_ML, Changed, -1.0f, 0.0f);
						s_Scroll = std::max(0.0f, Chat.m_Input.GetCaretPosition().x - Rect.x - (Px.m_W - 6.0f * Scale));
						TextRender()->TextColor(TextRender()->DefaultTextColor());
						TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
					});
				});
			});
		}
	});
	if(!Typing)
		m_ChatContextOpen = false;
	const SMenuItem aMessageItems[] = {
		{Localize("Copy message"), Ph::COPY},
		{Localize("Copy name"), Ph::USER},
		{Localize("KyW · Player profile"), Ph::USER_CIRCLE},
		{Localize("Mute chat"), Ph::CHAT_CIRCLE},
	};
	int Chosen = -1;
	ContextMenu("chat.context", m_ChatContextName.c_str(), aMessageItems, m_ChatContextClientId >= 0 ? 4 : 1, &m_ChatContextOpen, m_ChatContextAt, &Chosen);
	if(Chosen == 0)
		Input()->SetClipboardText(m_ChatContextText.c_str());
	else if(Chosen == 1)
		Input()->SetClipboardText(m_ChatContextName.c_str());
	else if(Chosen == 2)
	{
		OpenPlayerProfile(m_ChatContextName.c_str());
		m_ProfileTee = m_ChatContextTee;
	}
	else if(Chosen == 3 && m_ChatContextClientId >= 0 && m_ChatContextClientId < MAX_CLIENTS)
	{
		auto &Data = GameClient()->m_aClients[m_ChatContextClientId];
		if(Data.m_Active && m_ChatContextName == Data.m_aName)
			Data.m_ChatIgnore = true;
	}

	const float BgAlpha = Typing ? 0.82f : 0.5f * MaxAlpha;
	Paint(PanelNode, [BgAlpha](CXui &U, const SRect &Rc) {
		if(BgAlpha < 0.01f)
			return;
		IGraphics::SUiRect Bg = SolidRect(ColorRGBA(0.04f, 0.04f, 0.06f, BgAlpha), 22.0f);
		U.DrawRect(Rc, Bg);
		if(BgAlpha > 0.3f)
			U.DrawRect(Rc, GlassHairline(22.0f));
	});
}

void CXui::RenderHudEditor()
{
	LoadLayout();
	static vec2 s_InspectorAnchor(1.0f, 0.0f);
	static vec2 s_InspectorGrab;
	static bool s_InspectorMoving = false;
	static bool s_InspectorHidden = false;
	static uint64_t s_LastEditorFrame = 0;
	if(s_LastEditorFrame + 1 != m_Frame)
	{
		s_InspectorMoving = false;
		s_InspectorHidden = false;
	}
	s_LastEditorFrame = m_Frame;
	if(KeyPressed(KEY_TAB))
	{
		s_InspectorHidden = !s_InspectorHidden;
		s_InspectorMoving = false;
	}
	if(KeyPressed(KEY_ESCAPE))
	{
		SaveLayout();
		m_HudEditor = false;
		return;
	}
	const float InspectorWidth = std::min(300.0f, m_ScreenUnits.x * 0.35f);
	const float InspectorHeight = std::min(720.0f, m_ScreenUnits.y - 48.0f);
	const vec2 InspectorRoom(std::max(0.0f, m_ScreenUnits.x - InspectorWidth - 48.0f), std::max(0.0f, m_ScreenUnits.y - InspectorHeight - 48.0f));
	vec2 InspectorPos = vec2(24, 24) + s_InspectorAnchor * InspectorRoom;
	const uint32_t DragId = MakeId("hud.inspector.drag");
	if(!s_InspectorHidden)
	{
		const SResult Drag = Interact(DragId);
		if(Drag.m_Pressed && !s_InspectorMoving && s_Dragging < 0)
		{
			s_InspectorMoving = true;
			s_InspectorGrab = Mouse() - InspectorPos;
		}
		if(s_InspectorMoving)
		{
			if(!Drag.m_Pressed && !Input()->KeyIsPressed(KEY_MOUSE_1))
				s_InspectorMoving = false;
			else
			{
				InspectorPos = Mouse() - s_InspectorGrab;
				InspectorPos.x = std::clamp(InspectorPos.x, 24.0f, 24.0f + InspectorRoom.x);
				InspectorPos.y = std::clamp(InspectorPos.y, 24.0f, 24.0f + InspectorRoom.y);
				s_InspectorAnchor = vec2(InspectorRoom.x > 0 ? (InspectorPos.x - 24.0f) / InspectorRoom.x : 0.0f, InspectorRoom.y > 0 ? (InspectorPos.y - 24.0f) / InspectorRoom.y : 0.0f);
			}
		}
	}
	const bool Online = Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK;
	if(!Online)
	{
		SBox Back;
		Back.m_Position = EPos::ABSOLUTE;
		Back.m_Left = Back.m_Top = Back.m_Right = Back.m_Bottom = 0.0f;
		Paint(Row(Back), [](CXui &U, const SRect &Rc) {
			IGraphics::SUiRect R = SolidRect(ColorRGBA(0.22f, 0.28f, 0.42f, 0.35f), 0.0f);
			R.m_Fill2 = ColorRGBA(0.1f, 0.08f, 0.12f, 0.45f);
			U.DrawRect(Rc, R);
		});
	}
	XuiTheme::UpdateTheme(0.0f);
	RenderHudModules(1.0f, true);
	XuiTheme::UpdateTheme(m_ThemeLightness);

	if(s_Dragging >= 0 && !s_vGuides.empty())
	{
		const ColorRGBA Acc = m_Accent;
		const std::vector<std::pair<bool, float>> vGuides = s_vGuides;
		SBox Layer;
		Layer.m_Position = EPos::ABSOLUTE;
		Layer.m_Left = Layer.m_Top = Layer.m_Right = Layer.m_Bottom = 0.0f;
		Paint(Row(Layer), [Acc, vGuides](CXui &U, const SRect &Rc) {
			for(const auto &[Vertical, At] : vGuides)
			{
				if(Vertical)
					U.DrawRect({At - 0.5f, Rc.m_Y, 1.0f, Rc.m_H}, SolidRect(WithAlpha(Acc, 0.8f), 0.0f));
				else
					U.DrawRect({Rc.m_X, At - 0.5f, Rc.m_W, 1.0f}, SolidRect(WithAlpha(Acc, 0.8f), 0.0f));
			}
		});
	}
	else if(s_Dragging < 0)
		s_vGuides.clear();

	if(s_InspectorHidden)
		return;
	Layer(ELayer::POPOVER, [&]() {
		SBox Inspector;
		Inspector.m_Position = EPos::ABSOLUTE;
		Inspector.m_Left = InspectorPos.x;
		Inspector.m_Top = InspectorPos.y;
		Inspector.m_W = Px(InspectorWidth);
		Inspector.m_H = Px(InspectorHeight);
		Inspector.m_Padding = SEdges(16.0f);
		Inspector.m_Gap = 14.0f;
		Inspector.m_pKey = "hud.inspector";
		GlassPanel(Inspector, [&]() {
		SBox Header;
		Header.m_W = Grow();
		Header.m_Gap = 10.0f;
		Header.m_Cross = EAlign::CENTER;
		Row(Header, [&]() {
			SBox Grip;
			Grip.m_W = Grow();
			Grip.m_H = Px(32.0f);
			Grip.m_Gap = 8.0f;
			Grip.m_Cross = EAlign::CENTER;
			Grip.m_Hit = EHit::INTERACTIVE;
			BoxId(DragId, EDir::ROW, Grip, [&]() {
				Icon(Ph::DOTS_SIX_VERTICAL, 18.0f, XuiTheme::Color::TEXT2);
				STextStyle Title;
				Title.m_Size = 18.0f;
				Title.m_Font = EFont::BOLD;
				Text(Localize("HUD editor"), Title);
			});
			const SResult Hide = IconButton(Ph::EYE_SLASH, {.m_Variant = EVariant::PLAIN, .m_Height = 32.0f, .m_pKey = "hud.inspector.hide"});
			Tooltip(Hide.m_Id, Localize("Hide panel · Tab"));
			if(Hide.m_Clicked)
				s_InspectorHidden = true;
		});
		STextStyle Hint;
		Hint.m_Size = 12.5f;
		Hint.m_Color = XuiTheme::Color::TEXT2;
		Hint.m_Wrap = true;
		Text(Localize("Drag modules to position them. Guides help align them."), Hint, {.m_W = Grow()});
		Text(Localize("Move this panel by its title. Tab hides or shows it."), Hint, {.m_W = Grow()});
		if(!Online)
			Text(Localize("Join a server to edit it over the real game"), Hint, {.m_W = Grow()});
		SBox List;
		List.m_W = List.m_H = Grow();
		List.m_Padding.m_R = 10.0f;
		List.m_pKey = "hud.modules";
		Scroll(List, [&]() {
			Group([&]() {
				for(int i = 0; i < NUM_MODULES; i++)
				{
					const SModuleDef &Def = Modules()[i];
					PushId((uint32_t)i + 1);
					GroupRow([&]() {
						Icon(Def.m_pIcon, 16.0f, XuiTheme::Color::TEXT2);
						STextStyle Label;
						Label.m_Size = 12.5f;
						Label.m_Wrap = true;
						Text(Localize(Def.m_pName), Label, {.m_W = Grow()});
						if(Switch("enabled", Def.m_pEnabled).m_Changed)
							g_Config.m_XxIslandDock &= ~(1 << i);
					}, i == 0, 48.0f);
					PopId();
				}
			});
			SectionHeader(Localize("Other elements"));
			Group([&]() {
				SettingsRow(Localize("Race timer"), nullptr, [&]() { Switch("hud.timer", &g_Config.m_ClShowhudTimer); });
				SettingsRow(Localize("Health and ammo"), nullptr, [&]() { Switch("hud.health", &g_Config.m_ClShowhudHealthAmmo); });
				SettingsRow(Localize("Freeze bars"), nullptr, [&]() { Switch("hud.freeze", &g_Config.m_ClShowFreezeBars); });
				SettingsRow(Localize("Spectator count"), nullptr, [&]() { Switch("hud.specs", &g_Config.m_ClShowhudSpectatorCount); });
				SettingsRow(Localize("Kill messages"), nullptr, [&]() { Switch("hud.kills", &g_Config.m_ClShowKillMessages); });
			});
		});
		if(Button(Localize("Reset layout"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::ARROW_COUNTER_CLOCKWISE, .m_W = Grow(), .m_pKey = "hud.reset"}).m_Clicked)
		{
			g_Config.m_XxHudLayout[0] = '\0';
			s_aLoadedLayout[0] = '\x01';
		}
		if(Button(Localize("Done"), {.m_Variant = EVariant::ACCENT, .m_pIcon = Ph::CHECK, .m_W = Grow(), .m_pKey = "hud.done"}).m_Clicked)
		{
			SaveLayout();
			m_HudEditor = false;
		} }, XuiTheme::Radius::XL);
	});
}
