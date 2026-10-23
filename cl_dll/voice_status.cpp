//========= Copyright (c) 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: client voice manager. Go-Mod: Reborn - the speaker labels and the
//          "you are talking" icon are drawn with Dear ImGui (no VGUI).
//
// $NoKeywords: $
//=============================================================================

#include "hud.h"

#include "cl_util.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "parsemsg.h"
#include "demo.h"
#include "demo_api.h"
#include "voice_status.h"
#include "r_efx.h"
#include "entity_types.h"
#include "imgui/imgui.h"

#include <algorithm>
#include <math.h>

// imgui_gomod_menu.cpp
extern unsigned int GoModUI_LoadImage(const char* path, int* w, int* h);
extern void GoModUI_DeleteTexture(unsigned int& tex);


extern bool cam_thirdperson;




// ---------------------------------------------------------------------- //
// The voice manager for the client.
// ---------------------------------------------------------------------- //
CVoiceStatus g_VoiceStatus;

CVoiceStatus* GetClientVoiceMgr()
{
	return &g_VoiceStatus;
}



// ---------------------------------------------------------------------- //
// CVoiceStatus.
// ---------------------------------------------------------------------- //

static CVoiceStatus* g_pInternalVoiceStatus = NULL;

int __MsgFunc_VoiceMask(const char* pszName, int iSize, void* pbuf)
{
	if (g_pInternalVoiceStatus)
		g_pInternalVoiceStatus->HandleVoiceMaskMsg(iSize, pbuf);

	return 1;
}

int __MsgFunc_ReqState(const char* pszName, int iSize, void* pbuf)
{
	if (g_pInternalVoiceStatus)
		g_pInternalVoiceStatus->HandleReqStateMsg(iSize, pbuf);

	return 1;
}


int g_BannedPlayerPrintCount;
void ForEachBannedPlayer(char id[16])
{
	char str[256];
	sprintf(str, "Ban %d: %2X%2X%2X%2X%2X%2X%2X%2X%2X%2X%2X%2X%2X%2X%2X%2X\n",
		g_BannedPlayerPrintCount++,
		id[0], id[1], id[2], id[3],
		id[4], id[5], id[6], id[7],
		id[8], id[9], id[10], id[11],
		id[12], id[13], id[14], id[15]);

	gEngfuncs.pfnConsolePrint(str);
}


void ShowBannedCallback()
{
	if (g_pInternalVoiceStatus)
	{
		g_BannedPlayerPrintCount = 0;
		gEngfuncs.pfnConsolePrint("------- BANNED PLAYERS -------\n");
		g_pInternalVoiceStatus->m_BanMgr.ForEachBannedPlayer(ForEachBannedPlayer);
		gEngfuncs.pfnConsolePrint("------------------------------\n");
	}
}


// ---------------------------------------------------------------------- //
// CVoiceStatus.
// ---------------------------------------------------------------------- //

CVoiceStatus::CVoiceStatus()
{
	m_bBanMgrInitialized = false;
	m_LastUpdateServerState = 0;

	m_LocalTexture = m_AckTexture = 0;
	m_LocalTexSize[0] = m_LocalTexSize[1] = 0;
	m_AckTexSize[0] = m_AckTexSize[1] = 0;
	m_bTexturesLoaded = false;

	m_bTalking = m_bServerAcked = false;
	m_bInSquelchMode = false;
	m_pHelper = NULL;
	m_VoiceHeadModel = 0;
	m_VoiceHeadModelHeight = 45;

	for (int i = 0; i < MAX_VOICE_SPEAKERS; i++)
		m_Labels[i].m_clientindex = -1;

	m_bServerModEnable = -1;

	m_pchGameDir = NULL;
}


CVoiceStatus::~CVoiceStatus()
{
	g_pInternalVoiceStatus = NULL;

	// the textures are not deleted here: the GL context is already gone when the dll is unloaded

	if (m_pchGameDir)
	{
		if (m_bBanMgrInitialized)
		{
			m_BanMgr.SaveState(m_pchGameDir);
		}

		free(m_pchGameDir);
	}
}


int CVoiceStatus::Init(IVoiceStatusHelper* pHelper)
{
	// Setup the voice_modenable cvar.
	gEngfuncs.pfnRegisterVariable("voice_modenable", "1", FCVAR_ARCHIVE);

	gEngfuncs.pfnRegisterVariable("voice_clientdebug", "0", 0);

	gEngfuncs.pfnAddCommand("voice_showbanned", ShowBannedCallback);

	if (gEngfuncs.pfnGetGameDirectory())
	{
		m_BanMgr.Init(gEngfuncs.pfnGetGameDirectory());
		m_bBanMgrInitialized = true;
	}

	assert(!g_pInternalVoiceStatus);
	g_pInternalVoiceStatus = this;

	m_VoiceHeadModel = 0;
	memset(m_Labels, 0, sizeof(m_Labels));

	for (int i = 0; i < MAX_VOICE_SPEAKERS; i++)
		m_Labels[i].m_clientindex = -1;

	m_bInSquelchMode = false;

	m_pHelper = pHelper;
	gHUD.AddHudElem(this);
	m_iFlags = HUD_ACTIVE;
	HOOK_MESSAGE(VoiceMask);
	HOOK_MESSAGE(ReqState);

	// Cache the game directory for use when we shut down
	const char* pchGameDirT = gEngfuncs.pfnGetGameDirectory();
	m_pchGameDir = (char*)malloc(strlen(pchGameDirT) + 1);
	strcpy(m_pchGameDir, pchGameDirT);

	return 1;
}


bool CVoiceStatus::VidInit()
{
	// the icons are loaded again the first time they are drawn
	FreeTextures();

	// new level: nobody is talking
	for (int i = 0; i < MAX_VOICE_SPEAKERS; i++)
		m_Labels[i].m_clientindex = -1;

	// Figure out the voice head model height.
	m_VoiceHeadModelHeight = 45;
	char* pFile = (char*)gEngfuncs.COM_LoadFile("scripts/voicemodel.txt", 5, NULL);
	if (pFile)
	{
		char token[4096];
		gEngfuncs.COM_ParseFile(pFile, token);
		if (token[0] >= '0' && token[0] <= '9')
		{
			m_VoiceHeadModelHeight = (float)atof(token);
		}

		gEngfuncs.COM_FreeFile(pFile);
	}

	m_VoiceHeadModel = gEngfuncs.pfnSPR_Load("sprites/voiceicon.spr");
	return true;
}


void CVoiceStatus::Frame(double frametime)
{
	// check server banned players once per second
	if (gEngfuncs.GetClientTime() - m_LastUpdateServerState > 1)
	{
		UpdateServerState(false);
	}

}


void CVoiceStatus::CreateEntities()
{
	if (0 == m_VoiceHeadModel)
		return;

	cl_entity_t* localPlayer = gEngfuncs.GetLocalPlayer();

	int iOutModel = 0;
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (!m_VoicePlayers[i])
			continue;

		cl_entity_s* pClient = gEngfuncs.GetEntityByIndex(i + 1);

		// Don't show an icon if the player is not in our PVS.
		if (!pClient || pClient->curstate.messagenum < localPlayer->curstate.messagenum)
			continue;

		// Don't show an icon for dead or spectating players (ie: invisible entities).
		if ((pClient->curstate.effects & EF_NODRAW) != 0)
			continue;

		// Don't show an icon for the local player unless we're in thirdperson mode.
		if (pClient == localPlayer && !cam_thirdperson)
			continue;

		cl_entity_s* pEnt = &m_VoiceHeadModels[iOutModel];
		++iOutModel;

		memset(pEnt, 0, sizeof(*pEnt));

		pEnt->curstate.rendermode = kRenderTransAdd;
		pEnt->curstate.renderamt = 255;
		pEnt->baseline.renderamt = 255;
		pEnt->curstate.renderfx = kRenderFxNoDissipation;
		pEnt->curstate.framerate = 1;
		pEnt->curstate.frame = 0;
		pEnt->model = (struct model_s*)gEngfuncs.GetSpritePointer(m_VoiceHeadModel);
		pEnt->angles[0] = pEnt->angles[1] = pEnt->angles[2] = 0;
		pEnt->curstate.scale = 0.5f;

		pEnt->origin[0] = pEnt->origin[1] = 0;
		pEnt->origin[2] = 45;

		VectorAdd(pEnt->origin, pClient->origin, pEnt->origin);

		// Tell the engine.
		gEngfuncs.CL_CreateVisibleEntity(ET_NORMAL, pEnt);
	}
}


void CVoiceStatus::UpdateSpeakerStatus(int entindex, bool bTalking)
{
	cvar_t* pVoiceLoopback = NULL;

	if (!m_pHelper)
		return;

	if (0 != gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
	{
		char msg[256];
		snprintf(msg, sizeof(msg), "CVoiceStatus::UpdateSpeakerStatus: ent %d talking = %d\n", entindex, static_cast<int>(bTalking));
		gEngfuncs.pfnConsolePrint(msg);
	}

	int iLocalPlayerIndex = gEngfuncs.GetLocalPlayer()->index;

	// Is it the local player talking?
	if (entindex == -1)
	{
		m_bTalking = bTalking;
		if (bTalking)
		{
			// Enable voice for them automatically if they try to talk.
			gEngfuncs.pfnClientCmd("voice_modenable 1");
		}

		// now set the player index to the correct index for the local player
		// this will allow us to have the local player's icon flash in the scoreboard
		entindex = iLocalPlayerIndex;

		pVoiceLoopback = gEngfuncs.pfnGetCvarPointer("voice_loopback");
	}
	else if (entindex == -2)
	{
		m_bServerAcked = bTalking;
	}

	if (entindex >= 0 && entindex <= MAX_PLAYERS)
	{
		int iClient = entindex - 1;
		if (iClient < 0)
		{
			return;
		}

		CVoiceLabel* pLabel = FindVoiceLabel(iClient);
		if (bTalking)
		{
			m_VoicePlayers[iClient] = true;
			m_VoiceEnabledPlayers[iClient] = true;

			// If we don't have a label for this guy yet, then create one.
			if (!pLabel)
			{
				// if this isn't the local player (unless they have voice_loopback on)
				if ((entindex != iLocalPlayerIndex) || (pVoiceLoopback && 0 != pVoiceLoopback->value))
				{
					if (pLabel = GetFreeVoiceLabel())
					{
						// Get the name from the engine.
						hud_player_info_t info;
						memset(&info, 0, sizeof(info));
						gEngfuncs.pfnGetPlayerInfo(entindex, &info);

						strncpy(pLabel->m_szName, info.name ? info.name : "", sizeof(pLabel->m_szName) - 1);
						pLabel->m_szName[sizeof(pLabel->m_szName) - 1] = '\0';

						m_pHelper->GetPlayerTextColor(entindex, pLabel->m_Color);

						pLabel->m_clientindex = iClient;
					}
				}
			}
		}
		else
		{
			m_VoicePlayers[iClient] = false;

			// If we have a label for this guy, kill it.
			if (pLabel)
				pLabel->m_clientindex = -1;
		}
	}
}


void CVoiceStatus::UpdateServerState(bool bForce)
{
	// Can't do anything when we're not in a level.
	char const* pLevelName = gEngfuncs.pfnGetLevelName();
	if (pLevelName[0] == 0)
	{
		if (0 != gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
		{
			gEngfuncs.pfnConsolePrint("CVoiceStatus::UpdateServerState: pLevelName[0]==0\n");
		}

		return;
	}

	int bCVarModEnable = static_cast<int>(0 != gEngfuncs.pfnGetCvarFloat("voice_modenable"));
	if (bForce || m_bServerModEnable != bCVarModEnable)
	{
		m_bServerModEnable = bCVarModEnable;

		char str[256];
		snprintf(str, sizeof(str), "VModEnable %d", m_bServerModEnable);
		ServerCmd(str);

		if (0 != gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
		{
			char msg[256];
			sprintf(msg, "CVoiceStatus::UpdateServerState: Sending '%s'\n", str);
			gEngfuncs.pfnConsolePrint(msg);
		}
	}

	char str[2048];
	sprintf(str, "vban");
	bool bChange = false;

	for (unsigned long dw = 0; dw < VOICE_MAX_PLAYERS_DW; dw++)
	{
		//The ban mask is a 32 bit int, so make sure this doesn't silently break.
		//Note that the server will also need updating.
		static_assert(MAX_PLAYERS <= 32, "The voice ban bit vector only supports up to 32 players");

		unsigned long serverBanMask = 0;
		unsigned long banMask = 0;
		for (unsigned long i = 0; i < MAX_PLAYERS; i++)
		{
			char playerID[16];
			if (0 == gEngfuncs.GetPlayerUniqueID(i + 1, playerID))
				continue;

			if (m_BanMgr.GetPlayerBan(playerID))
				banMask |= 1 << i;

			if (m_ServerBannedPlayers[dw * MAX_PLAYERS + i])
				serverBanMask |= 1 << i;
		}

		if (serverBanMask != banMask)
			bChange = true;

		// Ok, the server needs to be updated.
		char numStr[512];
		sprintf(numStr, " %lx", banMask);
		strcat(str, numStr);
	}

	if (bChange || bForce)
	{
		if (0 != gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
		{
			char msg[256];
			sprintf(msg, "CVoiceStatus::UpdateServerState: Sending '%s'\n", str);
			gEngfuncs.pfnConsolePrint(msg);
		}

		gEngfuncs.pfnServerCmdUnreliable(str); // Tell the server..
	}
	else
	{
		if (0 != gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
		{
			gEngfuncs.pfnConsolePrint("CVoiceStatus::UpdateServerState: no change\n");
		}
	}

	m_LastUpdateServerState = gEngfuncs.GetClientTime();
}

void CVoiceStatus::HandleVoiceMaskMsg(int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	unsigned long dw;
	for (dw = 0; dw < VOICE_MAX_PLAYERS_DW; dw++)
	{
		m_AudiblePlayers.SetDWord(dw, (unsigned long)READ_LONG());
		m_ServerBannedPlayers.SetDWord(dw, (unsigned long)READ_LONG());

		if (0 != gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
		{
			char str[256];
			gEngfuncs.pfnConsolePrint("CVoiceStatus::HandleVoiceMaskMsg\n");

			sprintf(str, "    - m_AudiblePlayers[%lu] = %u\n", dw, m_AudiblePlayers.GetDWord(dw));
			gEngfuncs.pfnConsolePrint(str);

			sprintf(str, "    - m_ServerBannedPlayers[%lu] = %u\n", dw, m_ServerBannedPlayers.GetDWord(dw));
			gEngfuncs.pfnConsolePrint(str);
		}
	}

	m_bServerModEnable = READ_BYTE();
}

void CVoiceStatus::HandleReqStateMsg(int iSize, void* pbuf)
{
	if (0 != gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
	{
		gEngfuncs.pfnConsolePrint("CVoiceStatus::HandleReqStateMsg\n");
	}

	UpdateServerState(true);
}

void CVoiceStatus::StartSquelchMode()
{
	if (m_bInSquelchMode)
		return;

	m_bInSquelchMode = true;
	m_pHelper->UpdateCursorState();
}

void CVoiceStatus::StopSquelchMode()
{
	m_bInSquelchMode = false;
	m_pHelper->UpdateCursorState();
}

bool CVoiceStatus::IsInSquelchMode()
{
	return m_bInSquelchMode;
}

CVoiceLabel* CVoiceStatus::FindVoiceLabel(int clientindex)
{
	for (int i = 0; i < MAX_VOICE_SPEAKERS; i++)
	{
		if (m_Labels[i].m_clientindex == clientindex)
			return &m_Labels[i];
	}

	return NULL;
}


CVoiceLabel* CVoiceStatus::GetFreeVoiceLabel()
{
	return FindVoiceLabel(-1);
}


// ---------------------------------------------------------------------- //
// ImGui: speaker labels (right side of the screen) and the local icon
// (lower right, when you are talking). Drawn inside the ImGui frame (hud_imgui.cpp).
// ---------------------------------------------------------------------- //

// speaker drawn by hand (the label icon, and the local icon if gfx/vgui/icntlk_pl.tga is missing)
static void DrawSpeakerIcon(ImDrawList* dl, const ImVec2& p, float s, ImU32 col)
{
	const float cy = p.y + s * 0.5f;

	// box and cone
	dl->AddRectFilled(ImVec2(p.x + s * 0.06f, cy - s * 0.15f), ImVec2(p.x + s * 0.28f, cy + s * 0.15f), col);
	const ImVec2 cone[4] = {
		ImVec2(p.x + s * 0.28f, cy - s * 0.15f),
		ImVec2(p.x + s * 0.52f, cy - s * 0.36f),
		ImVec2(p.x + s * 0.52f, cy + s * 0.36f),
		ImVec2(p.x + s * 0.28f, cy + s * 0.15f)};
	dl->AddConvexPolyFilled(cone, 4, col);

	// sound waves
	const float th = (std::max)(1.0f, s * 0.08f);
	dl->PathArcTo(ImVec2(p.x + s * 0.54f, cy), s * 0.18f, -0.85f, 0.85f);
	dl->PathStroke(col, 0, th);
	dl->PathArcTo(ImVec2(p.x + s * 0.54f, cy), s * 0.34f, -0.85f, 0.85f);
	dl->PathStroke(col, 0, th);
}

void CVoiceStatus::FreeTextures()
{
	if (m_LocalTexture)
		GoModUI_DeleteTexture(m_LocalTexture);
	if (m_AckTexture)
		GoModUI_DeleteTexture(m_AckTexture);

	m_LocalTexture = m_AckTexture = 0;
	m_LocalTexSize[0] = m_LocalTexSize[1] = 0;
	m_AckTexSize[0] = m_AckTexSize[1] = 0;
	m_bTexturesLoaded = false;
}

bool CVoiceStatus::HasImGui()
{
	if (!m_pHelper)
		return false;

	if (m_bTalking || m_bServerAcked)
		return true;

	if (!m_pHelper->CanShowSpeakerLabels())
		return false;

	for (int i = 0; i < MAX_VOICE_SPEAKERS; i++)
	{
		if (m_Labels[i].m_clientindex != -1)
			return true;
	}

	return false;
}

void CVoiceStatus::DrawImGui()
{
	if (!m_pHelper)
		return;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();

	// ---- who is talking: a list at the right side, from the middle of the screen ----
	if (m_pHelper->CanShowSpeakerLabels())
	{
		const float fontSize = (std::max)(12.0f, ScreenHeight * 0.022f);
		ImGui::PushFont(nullptr, fontSize);

		const float border = (std::max)(2.0f, fontSize * 0.2f);
		const float iconSize = fontSize;
		const float maxText = (float)(ScreenWidth * 2) / 3;
		float y = (float)(ScreenHeight / 2);

		for (int i = 0; i < MAX_VOICE_SPEAKERS; i++)
		{
			const CVoiceLabel& label = m_Labels[i];
			if (label.m_clientindex == -1)
				continue;

			ImVec2 text = ImGui::CalcTextSize(label.m_szName);
			if (text.x > maxText)
				text.x = maxText;

			const float bgWide = text.x + fontSize * 0.6f + iconSize + border * 3;
			const float bgTall = (std::max)(text.y, iconSize) + border * 2;
			const float x = (float)ScreenWidth - bgWide - 8.0f;

			// team color, a bit transparent
			dl->AddRectFilled(ImVec2(x, y), ImVec2(x + bgWide, y + bgTall),
				IM_COL32(label.m_Color[0], label.m_Color[1], label.m_Color[2], 120), fontSize * 0.15f);

			const ImVec2 textPos(x + border, y + (bgTall - text.y) * 0.5f);
			dl->PushClipRect(ImVec2(x, y), ImVec2(x + border + text.x, y + bgTall), true);
			dl->AddText(textPos, IM_COL32(255, 255, 255, 255), label.m_szName);
			dl->PopClipRect();

			DrawSpeakerIcon(dl, ImVec2(x + bgWide - border - iconSize, y + (bgTall - iconSize) * 0.5f), iconSize, IM_COL32(255, 255, 255, 230));

			y += bgTall + 2.0f;
		}

		ImGui::PopFont();
	}

	// ---- lower right: you are talking (blue icon = the server got it, with voice_clientdebug) ----
	if (m_bTalking || m_bServerAcked)
	{
		if (!m_bTexturesLoaded)
		{
			m_bTexturesLoaded = true;
			m_LocalTexture = GoModUI_LoadImage("gfx/vgui/icntlk_pl.tga", &m_LocalTexSize[0], &m_LocalTexSize[1]);
			m_AckTexture = GoModUI_LoadImage("gfx/vgui/icntlk_sv.tga", &m_AckTexSize[0], &m_AckTexSize[1]);
		}

		unsigned int tex = m_LocalTexture;
		int w = m_LocalTexSize[0], h = m_LocalTexSize[1];

		if (m_bServerAcked && m_AckTexture && 0 != gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
		{
			tex = m_AckTexture;
			w = m_AckTexSize[0];
			h = m_AckTexSize[1];
		}

		if (!tex)
			w = h = (int)(std::max)(24.0f, ScreenHeight * 0.05f);

		const float x = (float)(ScreenWidth - w - 10);
		const float y = (float)(m_pHelper->GetAckIconHeight() - h);
		const ImU32 tint = IM_COL32(255, 255, 255, 120);

		if (tex)
			dl->AddImage((ImTextureID)(intptr_t)tex, ImVec2(x, y), ImVec2(x + w, y + h), ImVec2(0, 0), ImVec2(1, 1), tint);
		else
			DrawSpeakerIcon(dl, ImVec2(x, y), (float)w, tint);
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the target client has been banned
// Input  : playerID -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CVoiceStatus::IsPlayerBlocked(int iPlayer)
{
	char playerID[16];
	if (0 == gEngfuncs.GetPlayerUniqueID(iPlayer, playerID))
		return false;

	return m_BanMgr.GetPlayerBan(playerID);
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the player can't hear the other client due to game rules (eg. the other team)
// Input  : playerID -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CVoiceStatus::IsPlayerAudible(int iPlayer)
{
	return !!m_AudiblePlayers[iPlayer - 1];
}

//-----------------------------------------------------------------------------
// Purpose: blocks/unblocks the target client from being heard
// Input  : playerID -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CVoiceStatus::SetPlayerBlockedState(int iPlayer, bool blocked)
{
	if (0 != gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
	{
		gEngfuncs.pfnConsolePrint("CVoiceStatus::SetPlayerBlockedState part 1\n");
	}

	char playerID[16];
	if (0 == gEngfuncs.GetPlayerUniqueID(iPlayer, playerID))
		return;

	if (0 != gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
	{
		gEngfuncs.pfnConsolePrint("CVoiceStatus::SetPlayerBlockedState part 2\n");
	}

	// Squelch or (try to) unsquelch this player.
	if (0 != gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
	{
		char str[256];
		sprintf(str, "CVoiceStatus::SetPlayerBlockedState: setting player %d ban to %d\n", iPlayer, static_cast<int>(!m_BanMgr.GetPlayerBan(playerID)));
		gEngfuncs.pfnConsolePrint(str);
	}

	m_BanMgr.SetPlayerBan(playerID, blocked);
	UpdateServerState(false);
}
