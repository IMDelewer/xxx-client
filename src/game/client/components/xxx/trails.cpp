#include "trails.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/render.h>

#include <algorithm>
#include <cmath>

void CTrails::ClearHistory(int ClientId)
{
	for(SHistory &Entry : m_aaHistory[ClientId])
		Entry = SHistory();
	m_aHistoryValid[ClientId] = false;
}

void CTrails::OnReset()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
		ClearHistory(i);
}

bool CTrails::PredictsPlayer(int ClientId) const
{
	if(!GameClient()->Predict() || !GameClient()->m_PredictedWorld.GetCharacterById(ClientId))
		return false;
	return ClientId == GameClient()->m_Snap.m_LocalClientId || (GameClient()->AntiPingPlayers() && !GameClient()->IsOtherTeam(ClientId));
}

void CTrails::OnRender()
{
	if(!g_Config.m_XxTeeTrail)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(!GameClient()->m_Snap.m_pGameInfoObj)
		return;

	Graphics()->TextureClear();
	const bool ZoomAllowed = GameClient()->m_Camera.ZoomAllowed();

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		const bool Local = GameClient()->m_Snap.m_LocalClientId == ClientId;
		if(!Local && (!g_Config.m_XxTeeTrailOthers || !ZoomAllowed))
			continue;

		const CGameClient::CSnapState::CCharacterInfo &Character = GameClient()->m_Snap.m_aCharacters[ClientId];
		if(!Character.m_Active)
		{
			if(m_aHistoryValid[ClientId])
				ClearHistory(ClientId);
			continue;
		}
		m_aHistoryValid[ClientId] = true;

		const CTeeRenderInfo Info = GameClient()->m_aClients[ClientId].m_RenderInfo;
		const bool Predicted = PredictsPlayer(ClientId);
		const int GameTick = Client()->GameTick(g_Config.m_ClDummy);
		const int StartTick = Predicted ? Client()->PredGameTick(g_Config.m_ClDummy) : GameTick;
		const float IntraTick = Predicted ? Client()->PredIntraGameTick(g_Config.m_ClDummy) : Client()->IntraGameTick(g_Config.m_ClDummy);

		const vec2 CurPos = vec2(Character.m_Cur.m_X, Character.m_Cur.m_Y);
		const vec2 PrevPos = vec2(Character.m_Prev.m_X, Character.m_Prev.m_Y);
		m_aaHistory[ClientId][GameTick % HISTORY] = {mix(PrevPos, CurPos, IntraTick), GameTick};

		float Alpha = g_Config.m_XxTeeTrailAlpha / 100.0f;
		if(GameClient()->IsOtherTeam(ClientId))
			Alpha *= g_Config.m_ClShowOthersAlpha / 100.0f;

		const int Length = g_Config.m_XxTeeTrailLength + (Predicted ? 0 : 2);
		const float BaseWidth = g_Config.m_XxTeeTrailWidth;
		bool Full = false;

		m_vTrail.clear();
		for(int i = 0; i < Length; i++)
		{
			SPart Part;
			const int Tick = StartTick - i;
			if(Predicted)
			{
				if(GameClient()->m_aClients[ClientId].m_aPredTick[Tick % HISTORY] != Tick)
					continue;
				Part.m_Pos = GameClient()->m_aClients[ClientId].m_aPredPos[Tick % HISTORY];
				Full = Full || i == Length - 1;
			}
			else
			{
				if(m_aaHistory[ClientId][Tick % HISTORY].m_Tick != Tick)
					continue;
				Part.m_Pos = m_aaHistory[ClientId][Tick % HISTORY].m_Pos;
				Full = Full || i == Length - 2 || i == Length - 3;
			}
			Part.m_RawPos = Part.m_Pos;
			Part.m_Tick = Tick;
			m_vTrail.push_back(Part);
		}

		const int Trim = (int)IntraTick;
		for(int i = 0; i < Trim && !m_vTrail.empty(); i++)
			m_vTrail.pop_back();
		if(m_vTrail.size() < 3)
			continue;

		m_vTrail.front().m_Pos = Predicted ? GameClient()->m_aClients[ClientId].m_RenderPos : mix(PrevPos, CurPos, IntraTick);
		if(Full)
			m_vTrail.back().m_Pos = mix(m_vTrail.back().m_Pos, m_vTrail[m_vTrail.size() - 2].m_Pos, std::fmod(IntraTick, 1.0f));

		const int Count = (int)m_vTrail.size();
		for(int i = 0; i < Count; i++)
		{
			SPart &Part = m_vTrail[i];
			const float Size = float(Count - 1 + Trim);
			if(i == 0)
				Part.m_Progress = 0.0f;
			else if(i == Count - 1)
				Part.m_Progress = 1.0f;
			else
				Part.m_Progress = ((float)i + IntraTick - 1.0f) / (Size - 1.0f);

			switch(g_Config.m_XxTeeTrailColorMode)
			{
			case COLORMODE_TEE:
				Part.m_Color = Info.m_CustomColoredSkin ? Info.m_ColorBody : Info.m_BloodColor;
				break;
			case COLORMODE_RAINBOW:
			{
				const float Cycle = (1.0f / Length) * 0.5f;
				Part.m_Color = color_cast<ColorRGBA>(ColorHSLA(std::fmod(((Part.m_Tick + 6361 * ClientId) % 1000000) * Cycle, 1.0f), 1.0f, 0.5f));
				break;
			}
			case COLORMODE_SPEED:
			{
				float Speed = 0.0f;
				if(Count > 3)
				{
					const SPart &Other = i < 2 ? m_vTrail[i + 2] : m_vTrail[i - 2];
					Speed = distance(Other.m_RawPos, Part.m_RawPos) / std::max(1, std::abs(Other.m_Tick - Part.m_Tick));
				}
				Part.m_Color = color_cast<ColorRGBA>(ColorHSLA(65280 * ((int)(Speed * Speed / 12.5f) + 1)).UnclampLighting(ColorHSLA::DARKEST_LGT));
				break;
			}
			case COLORMODE_RANDOM:
			{
				const unsigned Hash = (unsigned)(Part.m_Tick / 4 + 7919 * ClientId) * 2654435761u;
				Part.m_Color = color_cast<ColorRGBA>(ColorHSLA(((Hash >> 8) & 0xffff) / 65535.0f, 1.0f, 0.5f));
				break;
			}
			default:
				Part.m_Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_XxTeeTrailColor));
				break;
			}
			Part.m_Color.a = Alpha;
			if(g_Config.m_XxTeeTrailFade)
				Part.m_Color.a *= 1.0f - Part.m_Progress;
			Part.m_Width = g_Config.m_XxTeeTrailTaper ? BaseWidth * (1.0f - Part.m_Progress) : BaseWidth;
		}

		m_vTrail.erase(std::unique(m_vTrail.begin(), m_vTrail.end(), [](const SPart &A, const SPart &B) { return A.m_Pos == B.m_Pos; }), m_vTrail.end());
		if(m_vTrail.size() < 3)
			continue;

		const int N = (int)m_vTrail.size();
		for(int i = 0; i < N; i++)
		{
			SPart &Part = m_vTrail[i];
			const vec2 Pos = Part.m_Pos;
			const vec2 PrevP = i == 0 ? Pos - normalize(m_vTrail[1].m_Pos - Pos) : m_vTrail[i - 1].m_Pos;
			const vec2 NextP = i == N - 1 ? Pos + normalize(Pos - m_vTrail[i - 1].m_Pos) : m_vTrail[i + 1].m_Pos;
			const vec2 NextDir = normalize(NextP - Pos);
			const vec2 PrevDir = normalize(Pos - PrevP);
			const vec2 Normal = vec2(-PrevDir.y, PrevDir.x);
			vec2 Tangent = normalize(NextDir + PrevDir);
			if(Tangent == vec2(0.0f, 0.0f))
				Tangent = Normal;
			const vec2 Perp = vec2(-Tangent.y, Tangent.x);
			const float Width = Part.m_Width;
			const float Scaled = Width / dot(Normal, Perp);
			float TopScaled = Scaled;
			float BottomScaled = Scaled;
			if(dot(PrevDir, Tangent) > 0.0f)
				TopScaled = std::min(Width * 3.0f, TopScaled);
			else
				BottomScaled = std::min(Width * 3.0f, BottomScaled);
			Part.m_Top = Pos + Perp * TopScaled;
			Part.m_Bottom = Pos - Perp * BottomScaled;

			if(dot(PrevDir, NextDir) < -0.25f)
			{
				const vec2 A = Pos + Tangent * Width;
				const vec2 B = Pos - Tangent * Width;
				const float Det = PrevDir.x * NextDir.y - PrevDir.y * NextDir.x;
				Part.m_Top = Det >= 0.0f ? A : B;
				Part.m_Bottom = Det >= 0.0f ? B : A;
				Part.m_Flip = i > 0;
			}
		}

		const bool LineMode = g_Config.m_XxTeeTrailWidth == 0;
		if(LineMode)
			Graphics()->LinesBegin();
		else
			Graphics()->QuadsBegin();
		for(int i = 0; i < N - 1; i++)
		{
			const SPart &Part = m_vTrail[i];
			const SPart &Next = m_vTrail[i + 1];
			const float Dist = distance(Part.m_RawPos, Next.m_RawPos);
			const float MaxDiff = 120.0f;
			if(i > 0 && std::abs(Dist - distance(m_vTrail[i - 1].m_RawPos, Part.m_RawPos)) > MaxDiff)
				continue;
			if(i < N - 2 && std::abs(Dist - distance(Next.m_RawPos, m_vTrail[i + 2].m_RawPos)) > MaxDiff)
				continue;

			if(LineMode)
			{
				Graphics()->SetColor(Part.m_Color);
				const IGraphics::CLineItem Line(Part.m_Pos.x, Part.m_Pos.y, Next.m_Pos.x, Next.m_Pos.y);
				Graphics()->LinesDraw(&Line, 1);
			}
			else
			{
				const vec2 Top = Part.m_Flip ? Part.m_Bottom : Part.m_Top;
				const vec2 Bottom = Part.m_Flip ? Part.m_Top : Part.m_Bottom;
				Graphics()->SetColor4(Next.m_Color, Next.m_Color, Part.m_Color, Part.m_Color);
				const IGraphics::CFreeformItem Quad(Next.m_Top, Next.m_Bottom, Top, Bottom);
				Graphics()->QuadsDrawFreeform(&Quad, 1);
			}
		}
		if(LineMode)
			Graphics()->LinesEnd();
		else
			Graphics()->QuadsEnd();
	}
}
