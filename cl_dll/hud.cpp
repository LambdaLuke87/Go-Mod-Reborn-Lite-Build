/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// hud.cpp
//
// implementation of CHud class
//

#include "hud.h"
#include "cl_util.h"
#include <string.h>
#include <stdio.h>
#include "parsemsg.h"
#include "gomod_viewport.h"

#include "demo.h"
#include "demo_api.h"

#include "environment.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/mmui.h"

#include "gm_audio.h"
#include "physgun_beam.h"

hud_player_info_t g_PlayerInfoList[MAX_PLAYERS_HUD + 1];	// player info from the engine
extra_player_info_t g_PlayerExtraInfo[MAX_PLAYERS_HUD + 1]; // additional player info sent directly to the client dll

int giR, giG, giB;
int giRCritical, giGCritical, giBCritical;
int giNVR, giNVG, giNVB;
cvar_t* m_pCvarHudRed;
cvar_t* m_pCvarHudGreen;
cvar_t* m_pCvarHudBlue;
cvar_t* m_pCvarHudRed_Critical;
cvar_t* m_pCvarHudGreen_Critical;
cvar_t* m_pCvarHudBlue_Critical;
cvar_t* m_pCvarNightVisionR;
cvar_t* m_pCvarNightVisionG;
cvar_t* m_pCvarNightVisionB;
cvar_t* m_pCvarPhysgunR;
cvar_t* m_pCvarPhysgunG;
cvar_t* m_pCvarPhysgunB;

// Go-Mod crosshair (ammo.cpp DrawLineCrosshair)
cvar_t* m_pCvarLegacyCrosshair;
cvar_t* m_pCvarCrosshairSize;
cvar_t* m_pCvarCrosshairTranslucent;
cvar_t* m_pCvarCrosshairR;
cvar_t* m_pCvarCrosshairG;
cvar_t* m_pCvarCrosshairB;
#define RGB_CROSSHAIR 0x32FA32 // 50 250 50

// default color of the physgun beam
#define RGB_PHYSGUN 0x33CCFF

extern int giOldWeapons;

extern void ImGuiMenu_SetOpen(bool open);
extern void GoModMenu_Reload();

extern bool g_bShowMenu;
extern bool UTIL_IsSandbox();

// snarkfrenzy_hud.cpp
void SnarkFrenzyHUD_Init();
void SnarkFrenzyHUD_Reset();

extern void ImGuiCursor_Update(); // hud_imgui.cpp

class CHLVoiceStatusHelper : public IVoiceStatusHelper
{
public:
	void GetPlayerTextColor(int entindex, int color[3]) override
	{
		color[0] = color[1] = color[2] = 255;

		if (entindex >= 0 && entindex < sizeof(g_PlayerExtraInfo) / sizeof(g_PlayerExtraInfo[0]))
		{
			int iTeam = g_PlayerExtraInfo[entindex].teamnumber;

			if (iTeam < 0)
			{
				iTeam = 0;
			}

			iTeam = iTeam % iNumberOfTeamColors;

			color[0] = iTeamColors[iTeam][0];
			color[1] = iTeamColors[iTeam][1];
			color[2] = iTeamColors[iTeam][2];
		}
	}

	void UpdateCursorState() override
	{
		// the scoreboard (ImGui) takes / gives back the mouse to mute players
		ImGuiCursor_Update();
		gViewPort->UpdateCursorState();
	}

	int GetAckIconHeight() override
	{
		return ScreenHeight - gHUD.m_iFontHeight * 3 - 6;
	}

	bool CanShowSpeakerLabels() override
	{
		return !gHUD.m_Scoreboard.IsVisible();
	}
};
static CHLVoiceStatusHelper g_VoiceStatusHelper;


extern client_sprite_t* GetSpriteList(client_sprite_t* pList, const char* psz, int iRes, int iCount);

extern float IN_GetMouseSensitivity();
cvar_t* cl_lw = NULL;
cvar_t* cl_rollangle = nullptr;
cvar_t* cl_rollspeed = nullptr;
cvar_t* cl_bobtilt = nullptr;
cvar_t* r_decals = nullptr;
cvar_t* cl_sourcewpnlag = nullptr;

cvar_t* cl_weather = NULL;

void ShutdownInput();

static cvar_t* CVAR_CREATE_INTVALUE(const char* name, int value, int flags)
{
	char valueStr[12];
	sprintf(valueStr, "%d", value);
	return CVAR_CREATE(name, valueStr, flags);
}

int __MsgFunc_HudColor(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	giR = READ_BYTE();
	giG = READ_BYTE();
	giB = READ_BYTE();
	return 1;
}

int __MsgFunc_HudColor_Critical(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	giRCritical = READ_BYTE();
	giGCritical = READ_BYTE();
	giBCritical = READ_BYTE();
	return 1;
}

int __MsgFunc_OldWeapon(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	giOldWeapons = READ_BYTE();
	return 1;
}

//DECLARE_MESSAGE(m_Logo, Logo)
int __MsgFunc_Logo(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_Logo(pszName, iSize, pbuf));
}

// LRC
int __MsgFunc_SetFog(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_SetFog(pszName, iSize, pbuf));
}

int __MsgFunc_Rain(const char* pszName, int iSize, void* pbuf)
{
	return g_Environment.MsgFunc_Rain(pszName, iSize, pbuf);
}

int __MsgFunc_Snow(const char* pszName, int iSize, void* pbuf)
{
	return g_Environment.MsgFunc_Snow(pszName, iSize, pbuf);
}

//DECLARE_MESSAGE(m_Logo, Logo)
int __MsgFunc_ResetHUD(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_ResetHUD(pszName, iSize, pbuf));
}

int __MsgFunc_InitHUD(const char* pszName, int iSize, void* pbuf)
{
	gHUD.MsgFunc_InitHUD(pszName, iSize, pbuf);
	return 1;
}

int __MsgFunc_ViewMode(const char* pszName, int iSize, void* pbuf)
{
	gHUD.MsgFunc_ViewMode(pszName, iSize, pbuf);
	return 1;
}

int __MsgFunc_SetFOV(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_SetFOV(pszName, iSize, pbuf));
}

int __MsgFunc_Concuss(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_Concuss(pszName, iSize, pbuf));
}

int __MsgFunc_Weapons(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_Weapons(pszName, iSize, pbuf));
}

int __MsgFunc_GameMode(const char* pszName, int iSize, void* pbuf)
{
	return static_cast<int>(gHUD.MsgFunc_GameMode(pszName, iSize, pbuf));
}

// Command menu (commandmenu.txt): ImGui (imgui_hudpanels.cpp)
extern void GoModCmdMenu_Show();
extern void GoModCmdMenu_Release();
extern void GoModCmdMenu_Hide();
extern bool GoModCmdMenu_IsOpen();

// CTF end of game stats: ImGui (imgui_hudpanels.cpp)
extern int GoModStats_MsgStatsInfo(const char* pszName, int iSize, void* pbuf);
extern int GoModStats_MsgStatsPlayer(const char* pszName, int iSize, void* pbuf);

void __CmdFunc_OpenCommandMenu()
{
	ImGuiMenu_SetOpen(false);
	GoModCmdMenu_Show();
}

// TFC "special" command: just send it down to the server
void __CmdFunc_InputPlayerSpecial()
{
	EngineClientCmd("_special");
}

void __CmdFunc_CloseCommandMenu()
{
	GoModCmdMenu_Release();
}

void __CmdFunc_ForceCloseCommandMenu()
{
	GoModCmdMenu_Hide();
}

// MOTD / Map Info: ImGui (imgui_gomod_menu.cpp). In the sandbox they are in the Go-Mod menu (Information)
extern void GoModInfo_Toggle(int page);
extern int GoModInfo_MsgMOTD(const char* pszName, int iSize, void* pbuf);

void __CmdFunc_OpenMOTDMenu()
{
	GoModInfo_Toggle(0);
}

void __CmdFunc_OpenMapInfoMenu()
{
	GoModInfo_Toggle(1);
}

// TFFree Command Menu Message Handlers
int __MsgFunc_ValClass(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_ValClass(pszName, iSize, pbuf));
	return 0;
}

int __MsgFunc_TeamNames(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_TeamNames(pszName, iSize, pbuf));
	return 0;
}

int __MsgFunc_Feign(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_Feign(pszName, iSize, pbuf));
	return 0;
}

int __MsgFunc_Detpack(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_Detpack(pszName, iSize, pbuf));
	return 0;
}

int __MsgFunc_VGUIMenu(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_VGUIMenu(pszName, iSize, pbuf));
	return 0;
}

int __MsgFunc_MOTD(const char* pszName, int iSize, void* pbuf)
{
	return GoModInfo_MsgMOTD(pszName, iSize, pbuf);
}

int __MsgFunc_BuildSt(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_BuildSt(pszName, iSize, pbuf));
	return 0;
}

int __MsgFunc_RandomPC(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_RandomPC(pszName, iSize, pbuf));
	return 0;
}

int __MsgFunc_ServerName(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_ServerName(pszName, iSize, pbuf));
	return 0;
}

/*
int __MsgFunc_ScoreInfo(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_ScoreInfo( pszName, iSize, pbuf ));
	return 0;
}

int __MsgFunc_TeamScore(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_TeamScore( pszName, iSize, pbuf ));
	return 0;
}

int __MsgFunc_TeamInfo(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_TeamInfo( pszName, iSize, pbuf ));
	return 0;
}
*/

int __MsgFunc_ToolBowSkin(const char* pszName, int iSize, void* pbuf)
{
	gHUD.MsgFunc_ToolBowSkin(pszName, iSize, pbuf);
	return 1;
}

int __MsgFunc_NightVision(const char* pszName, int iSize, void* pbuf)
{
	gHUD.MsgFunc_NightVision(pszName, iSize, pbuf);
	return 1;
}

int __MsgFunc_Spectator(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_Spectator(pszName, iSize, pbuf));
	return 0;
}

int __MsgFunc_SpecFade(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_SpecFade(pszName, iSize, pbuf));
	return 0;
}

int __MsgFunc_ResetFade(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_ResetFade(pszName, iSize, pbuf));
	return 0;
}

int __MsgFunc_AllowSpec(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_AllowSpec(pszName, iSize, pbuf));
	return 0;
}

int __MsgFunc_TeamFull(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_TeamFull(pszName, iSize, pbuf));
	return 0;
}

int __MsgFunc_SetMenuTeam(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return static_cast<int>(gViewPort->MsgFunc_SetMenuTeam(pszName, iSize, pbuf));
	return 0;
}

int __MsgFunc_StatsInfo(const char* pszName, int iSize, void* pbuf)
{
	return GoModStats_MsgStatsInfo(pszName, iSize, pbuf);
}

int __MsgFunc_StatsPlayer(const char* pszName, int iSize, void* pbuf)
{
	return GoModStats_MsgStatsPlayer(pszName, iSize, pbuf);
}

void __CmdFunc_HUDColor()
{
	gHUD.HUDColorCmd(0);
}

void __CmdFunc_HUDColor_Critical()
{
	gHUD.HUDColorCmd(1);
}

void __CmdFunc_Custom_NightVision_Color()
{
	gHUD.HUDColorCmd(2);
}

void __CmdFunc_Physgun_Color()
{
	gHUD.HUDColorCmd(3);
}

void __CmdFunc_Crosshair_Color()
{
	gHUD.HUDColorCmd(4);
}

void __CmdFunc_ShowMenu()
{
	if (!UTIL_IsSandbox())
	{
		gEngfuncs.Con_Printf("this gamemode is not Sandbox\n");
		return;
	}

	// avoid open in the CTF team / character menu or in the CTF stats
	extern bool GoModCTF_IsOpen();
	extern bool GoModStats_IsOpen();
	if (GoModCTF_IsOpen() || GoModStats_IsOpen())
		return;

	// avoid open in CommandMenu
	if (GoModCmdMenu_IsOpen())
		return;

	ImGuiMenu_SetOpen(!g_bShowMenu);
}

// Defined in cl_lrc_fx.cpp
void HookLrcEffectMessages();

// This is called every time the DLL is loaded
void CHud::Init()
{
	HookLrcEffectMessages();

	HOOK_MESSAGE(Logo);
	HOOK_MESSAGE(ResetHUD);
	HOOK_MESSAGE(GameMode);
	SnarkFrenzyHUD_Init(); // "SnarkFrenzy" message (snarkfrenzy_hud.cpp)
	HOOK_MESSAGE(InitHUD);
	HOOK_MESSAGE(ViewMode);
	HOOK_MESSAGE(SetFOV);
	HOOK_MESSAGE(Concuss);
	HOOK_MESSAGE(HudColor);
	HOOK_MESSAGE(OldWeapon);
	HOOK_MESSAGE(Weapons);
	HOOK_MESSAGE(ToolBowSkin);
	HOOK_MESSAGE(NightVision);
	HOOK_MESSAGE(SetFog);
	HOOK_MESSAGE(Rain);
	HOOK_MESSAGE(Snow);

	// TFFree CommandMenu
	HOOK_COMMAND("+commandmenu", OpenCommandMenu);
	HOOK_COMMAND("-commandmenu", CloseCommandMenu);
	HOOK_COMMAND("ForceCloseCommandMenu", ForceCloseCommandMenu);
	HOOK_COMMAND("special", InputPlayerSpecial);

	HOOK_COMMAND("toggleMOTDMenu", OpenMOTDMenu);
	HOOK_COMMAND("toggleMapInfoMenu", OpenMapInfoMenu);
	HOOK_COMMAND("hud_color", HUDColor);
	HOOK_COMMAND("hud_color_critical", HUDColor_Critical);
	HOOK_COMMAND("nightvision_color", Custom_NightVision_Color);
	HOOK_COMMAND("physgun_color", Physgun_Color);
	HOOK_COMMAND("crosshair_color", Crosshair_Color);

	HOOK_MESSAGE(ValClass);
	HOOK_MESSAGE(TeamNames);
	HOOK_MESSAGE(Feign);
	HOOK_MESSAGE(Detpack);
	HOOK_MESSAGE(MOTD);
	HOOK_MESSAGE(BuildSt);
	HOOK_MESSAGE(RandomPC);
	HOOK_MESSAGE(ServerName);
	/*
	HOOK_MESSAGE( ScoreInfo );
	HOOK_MESSAGE( TeamScore );
	HOOK_MESSAGE( TeamInfo );
	*/

	HOOK_MESSAGE(Spectator);
	HOOK_MESSAGE(AllowSpec);
	HOOK_MESSAGE(SetMenuTeam);
	HOOK_MESSAGE(StatsInfo);
	HOOK_MESSAGE(StatsPlayer);

	HOOK_MESSAGE(SpecFade);
	HOOK_MESSAGE(ResetFade);
	HOOK_MESSAGE(TeamFull);

	// the server opens the menus with "VGUIMenu" (gomod_viewport.cpp)
	HOOK_MESSAGE(VGUIMenu);

	gEngfuncs.pfnAddCommand("gomod_menu_reload", GoModMenu_Reload);
	gEngfuncs.pfnAddCommand("toggleSandboxMenu", __CmdFunc_ShowMenu);
	gEngfuncs.pfnRegisterVariable("cl_gomod_menu_theme", "0", FCVAR_ARCHIVE);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui_ImplOpenGL3_Init("#version 120");

	// Main menu / pause menu ImGui (Create Server, ...)
	MMUI_Init();

	CVAR_CREATE("hud_classautokill", "1", FCVAR_ARCHIVE | FCVAR_USERINFO); // controls whether or not to suicide immediately on TF class switch
	CVAR_CREATE("hud_takesshots", "0", FCVAR_ARCHIVE);					   // controls whether or not to automatically take screenshots at the end of a round

	int hudR, hudG, hudB;
	int hudR_Critical, hudG_Critical, hudB_Critical;
	int CustomNVR, CustomNVG, CustomNVB;
	int physR, physG, physB;
	UnpackRGB(physR, physG, physB, RGB_PHYSGUN);
	UnpackRGB(hudR, hudG, hudB, RGB_YELLOWISH);
	UnpackRGB(hudR_Critical, hudG_Critical, hudB_Critical, RGB_REDISH);
	UnpackRGB(CustomNVR, CustomNVG, CustomNVB, RGB_REDISH);
	m_pCvarHudRed = CVAR_CREATE_INTVALUE("hud_color_r", hudR, FCVAR_ARCHIVE);
	m_pCvarHudGreen = CVAR_CREATE_INTVALUE("hud_color_g", hudG, FCVAR_ARCHIVE);
	m_pCvarHudBlue = CVAR_CREATE_INTVALUE("hud_color_b", hudB, FCVAR_ARCHIVE);
	m_pCvarHudRed_Critical = CVAR_CREATE_INTVALUE("hud_color_critical_r", hudR_Critical, FCVAR_ARCHIVE);
	m_pCvarHudGreen_Critical = CVAR_CREATE_INTVALUE("hud_color_critical_g", hudG_Critical, FCVAR_ARCHIVE);
	m_pCvarHudBlue_Critical = CVAR_CREATE_INTVALUE("hud_color_critical_b", hudB_Critical, FCVAR_ARCHIVE);
	m_pCvarNightVisionR = CVAR_CREATE_INTVALUE("nightvision_color_r", CustomNVR, FCVAR_ARCHIVE);
	m_pCvarNightVisionG = CVAR_CREATE_INTVALUE("nightvision_color_g", CustomNVG, FCVAR_ARCHIVE);
	m_pCvarNightVisionB = CVAR_CREATE_INTVALUE("nightvision_color_b", CustomNVB, FCVAR_ARCHIVE);
	m_pCvarPhysgunR = CVAR_CREATE_INTVALUE("physgun_color_r", physR, FCVAR_ARCHIVE);
	m_pCvarPhysgunG = CVAR_CREATE_INTVALUE("physgun_color_g", physG, FCVAR_ARCHIVE);
	m_pCvarPhysgunB = CVAR_CREATE_INTVALUE("physgun_color_b", physB, FCVAR_ARCHIVE);

	int crossR, crossG, crossB;
	UnpackRGB(crossR, crossG, crossB, RGB_CROSSHAIR);
	m_pCvarCrosshairR = CVAR_CREATE_INTVALUE("crosshair_color_r", crossR, FCVAR_ARCHIVE);
	m_pCvarCrosshairG = CVAR_CREATE_INTVALUE("crosshair_color_g", crossG, FCVAR_ARCHIVE);
	m_pCvarCrosshairB = CVAR_CREATE_INTVALUE("crosshair_color_b", crossB, FCVAR_ARCHIVE);
	m_pCvarCrosshairSize = CVAR_CREATE("cl_crosshair_size", "auto", FCVAR_ARCHIVE);
	m_pCvarCrosshairTranslucent = CVAR_CREATE("cl_crosshair_translucent", "1", FCVAR_ARCHIVE);
	m_pCvarLegacyCrosshair = CVAR_CREATE("cl_legacy_crosshair", "0", FCVAR_ARCHIVE);
	PhysGun_Init(); // userinfo key with the color, so the other players see it

	m_iLogo = 0;
	m_iFOV = 0;
	setNightVisionState(false);

	CVAR_CREATE("zoom_sensitivity_ratio", "1.2", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_autowepswitch", "1", FCVAR_ARCHIVE | FCVAR_USERINFO);
	default_fov = CVAR_CREATE("default_fov", "90", FCVAR_ARCHIVE);
	m_pCvarStealMouse = CVAR_CREATE("hud_capturemouse", "1", FCVAR_ARCHIVE);
	m_pCvarDraw = CVAR_CREATE("hud_draw", "1", FCVAR_ARCHIVE);
	cl_lw = gEngfuncs.pfnGetCvarPointer("cl_lw");
	cl_rollangle = CVAR_CREATE("cl_rollangle", "2.0", FCVAR_ARCHIVE);
	cl_rollspeed = CVAR_CREATE("cl_rollspeed", "200", FCVAR_ARCHIVE);
	cl_bobtilt = CVAR_CREATE("cl_bobtilt", "1", FCVAR_ARCHIVE);
	r_decals = gEngfuncs.pfnGetCvarPointer("r_decals");
	cl_sourcewpnlag = CVAR_CREATE("cl_sourcewpnlag", "0", FCVAR_ARCHIVE);

	cl_weather = CVAR_CREATE("cl_weather", "1", FCVAR_ARCHIVE);

	// Player Sounds
	CVAR_CREATE("cl_player_sfx_type", "hevsuit", FCVAR_ARCHIVE | FCVAR_USERINFO); // Custom Player sounds

	m_pSpriteList = NULL;

	// Clear any old HUD list
	if (m_pHudList)
	{
		HUDLIST* pList;
		while (m_pHudList)
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			free(pList);
		}
		m_pHudList = NULL;
	}

	GMAudio_Init();

	// In case we get messages before the first update -- time will be valid
	m_flTime = 1.0;

	m_Ammo.Init();
	m_Health.Init();
	m_SayText.Init();
	m_Spectator.Init();
	m_Geiger.Init();
	m_Train.Init();
	m_Battery.Init();
	m_Flash.Init();
	m_Message.Init();
	m_Scoreboard.Init();
	m_StatusBar.Init();
	m_DeathNotice.Init();
	m_AmmoSecondary.Init();
	m_TextMessage.Init();
	m_StatusIcons.Init();
	m_FlagIcons.Init();
	m_PlayerBrowse.Init();
	GetClientVoiceMgr()->Init(&g_VoiceStatusHelper);

	m_Menu.Init();

	MsgFunc_ResetHUD(0, 0, NULL);

#ifdef STEAM_RICH_PRESENCE
	gEngfuncs.pfnClientCmd("richpresence_gamemode\n"); // reset
	gEngfuncs.pfnClientCmd("richpresence_update\n");
#endif
}

// CHud destructor
// cleans up memory allocated for m_rg* arrays
CHud::~CHud()
{
	delete[] m_rghSprites;
	delete[] m_rgrcRects;
	delete[] m_rgszSpriteNames;

	if (m_pHudList)
	{
		HUDLIST* pList;
		while (m_pHudList)
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			free(pList);
		}
		m_pHudList = NULL;
	}
}

// GetSpriteIndex()
// searches through the sprite list loaded from hud.txt for a name matching SpriteName
// returns an index into the gHUD.m_rghSprites[] array
// returns 0 if sprite not found
int CHud::GetSpriteIndex(const char* SpriteName)
{
	// look through the loaded sprite name list for SpriteName
	for (int i = 0; i < m_iSpriteCount; i++)
	{
		if (strncmp(SpriteName, m_rgszSpriteNames + (i * MAX_SPRITE_NAME_LENGTH), MAX_SPRITE_NAME_LENGTH) == 0)
			return i;
	}

	return -1; // invalid sprite
}

void CHud::VidInit()
{
	PhysGun_VidInit();
	SnarkFrenzyHUD_Reset();

	m_scrinfo.iSize = sizeof(m_scrinfo);
	GetScreenInfo(&m_scrinfo);

	// ----------
	// Load Sprites
	// ---------
	//	m_hsprFont = LoadSprite("sprites/%d_font.spr");

	m_hsprLogo = 0;
	m_hsprCursor = 0;

	if (ScreenWidth > 2560 && ScreenHeight > 1600)
		m_iRes = 2560;
	else if (ScreenWidth >= 1280 && ScreenHeight > 720)
		m_iRes = 1280;
	else if (ScreenWidth >= 640)
		m_iRes = 640;
	else
		m_iRes = 320;

	// Only load this once
	if (!m_pSpriteList)
	{
		// we need to load the hud.txt, and all sprites within
		m_pSpriteList = SPR_GetList("sprites/hud.txt", &m_iSpriteCountAllRes);

		if (m_pSpriteList)
		{
			// count the number of sprites of the appropriate res
			m_iSpriteCount = 0;
			client_sprite_t* p = m_pSpriteList;
			int j;
			for (j = 0; j < m_iSpriteCountAllRes; j++)
			{
				if (p->iRes == m_iRes)
					m_iSpriteCount++;
				p++;
			}

			// allocated memory for sprite handle arrays
			m_rghSprites = new HSPRITE[m_iSpriteCount];
			m_rgrcRects = new Rect[m_iSpriteCount];
			m_rgszSpriteNames = new char[m_iSpriteCount * MAX_SPRITE_NAME_LENGTH];

			p = m_pSpriteList;
			int index = 0;
			for (j = 0; j < m_iSpriteCountAllRes; j++)
			{
				if (p->iRes == m_iRes)
				{
					char sz[256];
					sprintf(sz, "sprites/%s.spr", p->szSprite);
					m_rghSprites[index] = SPR_Load(sz);
					m_rgrcRects[index] = p->rc;
					strncpy(&m_rgszSpriteNames[index * MAX_SPRITE_NAME_LENGTH], p->szName, MAX_SPRITE_NAME_LENGTH);

					index++;
				}

				p++;
			}
		}
	}
	else
	{
		// we have already have loaded the sprite reference from hud.txt, but
		// we need to make sure all the sprites have been loaded (we've gone through a transition, or loaded a save game)
		client_sprite_t* p = m_pSpriteList;
		int index = 0;
		for (int j = 0; j < m_iSpriteCountAllRes; j++)
		{
			if (p->iRes == m_iRes)
			{
				char sz[256];
				sprintf(sz, "sprites/%s.spr", p->szSprite);
				m_rghSprites[index] = SPR_Load(sz);
				index++;
			}

			p++;
		}
	}

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex("number_0");

	m_iFontHeight = m_rgrcRects[m_HUD_number_0].bottom - m_rgrcRects[m_HUD_number_0].top;

	GMAudio_StopAll();

	m_Ammo.VidInit();
	m_Health.VidInit();
	m_Spectator.VidInit();
	m_Geiger.VidInit();
	m_Train.VidInit();
	m_Battery.VidInit();
	m_Flash.VidInit();
	m_Message.VidInit();
	m_Scoreboard.VidInit();
	m_StatusBar.VidInit();
	m_DeathNotice.VidInit();
	m_SayText.VidInit();
	m_Menu.VidInit();
	m_AmmoSecondary.VidInit();
	m_TextMessage.VidInit();
	m_StatusIcons.VidInit();
	m_FlagIcons.VidInit();
	m_PlayerBrowse.VidInit();
	GetClientVoiceMgr()->VidInit();

	memset(&fog, 0, sizeof(fog));
}

bool CHud::MsgFunc_Logo(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	// update Train data
	m_iLogo = READ_BYTE();

	return true;
}

float g_lastFOV = 0.0;

/*
============
COM_FileBase
============
*/
// Extracts the base name of a file (no path, no extension, assumes '/' as path separator)
void COM_FileBase(const char* in, char* out)
{
	int len, start, end;

	len = strlen(in);

	// scan backward for '.'
	end = len - 1;
	while (0 != end && in[end] != '.' && in[end] != '/' && in[end] != '\\')
		end--;

	if (in[end] != '.') // no '.', copy to end
		end = len - 1;
	else
		end--; // Found ',', copy to left of '.'


	// Scan backward for '/'
	start = len - 1;
	while (start >= 0 && in[start] != '/' && in[start] != '\\')
		start--;

	if (in[start] != '/' && in[start] != '\\')
		start = 0;
	else
		start++;

	// Length of new sting
	len = end - start + 1;

	// Copy partial string
	strncpy(out, &in[start], len);
	// Terminate it
	out[len] = 0;
}

/*
=================
HUD_IsGame

=================
*/
bool HUD_IsGame(const char* game)
{
	const char* gamedir;
	char gd[1024];

	gamedir = gEngfuncs.pfnGetGameDirectory();
	if (gamedir && '\0' != gamedir[0])
	{
		COM_FileBase(gamedir, gd);
		if (!stricmp(gd, game))
			return true;
	}
	return false;
}

/*
=====================
HUD_GetFOV

Returns last FOV
=====================
*/
float HUD_GetFOV()
{
	if (0 != gEngfuncs.pDemoAPI->IsRecording())
	{
		// Write it
		int i = 0;
		unsigned char buf[100];

		// Active
		*(float*)&buf[i] = g_lastFOV;
		i += sizeof(float);

		Demo_WriteBuffer(TYPE_ZOOM, i, buf);
	}

	if (0 != gEngfuncs.pDemoAPI->IsPlayingback())
	{
		g_lastFOV = g_demozoom;
	}
	return g_lastFOV;
}

bool CHud::MsgFunc_SetFOV(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int newfov = READ_BYTE();
	int def_fov = CVAR_GET_FLOAT("default_fov");

	//Weapon prediction already takes care of changing the fog. ( g_lastFOV ).
	//But it doesn't restore correctly so this still needs to be used
	/*
	if ( cl_lw && cl_lw->value )
		return 1;
		*/

	g_lastFOV = newfov;

	if (newfov == 0)
	{
		m_iFOV = def_fov;
	}
	else
	{
		m_iFOV = newfov;
	}

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	if (m_iFOV == def_fov)
	{
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = IN_GetMouseSensitivity() * ((float)newfov / (float)def_fov) * CVAR_GET_FLOAT("zoom_sensitivity_ratio");
	}

	return true;
}


void CHud::AddHudElem(CHudBase* phudelem)
{
	HUDLIST *pdl, *ptemp;

	//phudelem->Think();

	if (!phudelem)
		return;

	pdl = (HUDLIST*)malloc(sizeof(HUDLIST));
	if (!pdl)
		return;

	memset(pdl, 0, sizeof(HUDLIST));
	pdl->p = phudelem;

	if (!m_pHudList)
	{
		m_pHudList = pdl;
		return;
	}

	ptemp = m_pHudList;

	while (ptemp->pNext)
		ptemp = ptemp->pNext;

	ptemp->pNext = pdl;
}

float CHud::GetSensitivity()
{
	return m_flMouseSensitivity;
}

void CHud::setNightVisionState(bool state)
{
	mNightVisionState = state;
}

void CHud::HUDColorCmd(int colorType)
{
	const char* cvarR;
	const char* cvarG;
	const char* cvarB;
	const char* cmdName;
	long defaultColor;

	cvar_t* pCvarRed;
	cvar_t* pCvarGreen;
	cvar_t* pCvarBlue;

	switch (colorType)
	{
	case 0: // Normal HUD
		cvarR = "hud_color_r";
		cvarG = "hud_color_g";
		cvarB = "hud_color_b";
		cmdName = "hud_color";
		defaultColor = RGB_YELLOWISH;

		pCvarRed = m_pCvarHudRed;
		pCvarGreen = m_pCvarHudGreen;
		pCvarBlue = m_pCvarHudBlue;
		break;

	case 1: // Critical HUD
		cvarR = "hud_color_critical_r";
		cvarG = "hud_color_critical_g";
		cvarB = "hud_color_critical_b";
		cmdName = "hud_color_critical";
		defaultColor = RGB_REDISH;

		pCvarRed = m_pCvarHudRed_Critical;
		pCvarGreen = m_pCvarHudGreen_Critical;
		pCvarBlue = m_pCvarHudBlue_Critical;
		break;

	case 2: // Nightvision HUD
		cvarR = "nightvision_color_r";
		cvarG = "nightvision_color_g";
		cvarB = "nightvision_color_b";
		cmdName = "nightvision_color";
		defaultColor = RGB_REDISH;

		pCvarRed = m_pCvarNightVisionR;
		pCvarGreen = m_pCvarNightVisionG;
		pCvarBlue = m_pCvarNightVisionB;
		break;

	case 3: // Physgun beam
		cvarR = "physgun_color_r";
		cvarG = "physgun_color_g";
		cvarB = "physgun_color_b";
		cmdName = "physgun_color";
		defaultColor = RGB_PHYSGUN;

		pCvarRed = m_pCvarPhysgunR;
		pCvarGreen = m_pCvarPhysgunG;
		pCvarBlue = m_pCvarPhysgunB;
		break;

	case 4: // Go-Mod crosshair
		cvarR = "crosshair_color_r";
		cvarG = "crosshair_color_g";
		cvarB = "crosshair_color_b";
		cmdName = "crosshair_color";
		defaultColor = RGB_CROSSHAIR;

		pCvarRed = m_pCvarCrosshairR;
		pCvarGreen = m_pCvarCrosshairG;
		pCvarBlue = m_pCvarCrosshairB;
		break;

	default:
		gEngfuncs.Con_Printf("Invalid HUD color type: %d\n", colorType);
		return;
	}

	int r, g, b;
	bool shouldPrintHelp = false;

	if (gEngfuncs.Cmd_Argc() == 4)
	{
		r = atoi(gEngfuncs.Cmd_Argv(1));
		g = atoi(gEngfuncs.Cmd_Argv(2));
		b = atoi(gEngfuncs.Cmd_Argv(3));
	}
	else if (gEngfuncs.Cmd_Argc() == 2)
	{
		const char* param = gEngfuncs.Cmd_Argv(1);

		if (param && *param)
		{
			if (strcmp(param, "default") == 0)
			{
				UnpackRGB(r, g, b, defaultColor);
			}
			else if (strncmp(param, "0x", 2) == 0 || *param == '#')
			{
				const bool sharp = *param == '#';
				const int expectedLength = sharp ? 7 : 8;
				const int shift = sharp ? 1 : 2;

				if (strlen(param) != expectedLength)
				{
					gEngfuncs.Con_Printf("6 hex digits expected after 0x or #\n");
					shouldPrintHelp = true;
				}
				else
				{
					char* endPtr;
					long color = strtol(param + shift, &endPtr, 16);

					if (*endPtr != '\0')
					{
						gEngfuncs.Con_Printf("Error parsing hex value\n");
						shouldPrintHelp = true;
					}
					else
					{
						UnpackRGB(r, g, b, color);
					}
				}
			}
			else
			{
				if (sscanf(param, "%d %d %d", &r, &g, &b) != 3)
				{
					gEngfuncs.Con_Printf("Expected three integer values\n");
					shouldPrintHelp = true;
				}
			}
		}
		else
		{
			shouldPrintHelp = true;
		}
	}
	else
	{
		shouldPrintHelp = true;
	}

	if (shouldPrintHelp)
	{
		const int hudR = pCvarRed->value;
		const int hudG = pCvarGreen->value;
		const int hudB = pCvarBlue->value;
		const int currentHudColor = ((hudR & 0xFF) << 16) | ((hudG & 0xFF) << 8) | (hudB & 0xFF);

		gEngfuncs.Con_Printf(
			"Current HUD color: %d %d %d (%06X)\n"
			"usage:\n"
			"%s RRR GGG BBB\n"
			"%s \"RRR GGG BBB\"\n"
			"%s 0xRRGGBB\n"
			"%s #RRGGBB\n"
			"%s default\n",
			hudR, hudG, hudB, currentHudColor,
			cmdName, cmdName, cmdName, cmdName, cmdName);
	}
	else
	{
		r = V_max(0, V_min(r, 255));
		g = V_max(0, V_min(g, 255));
		b = V_max(0, V_min(b, 255));

		gEngfuncs.Cvar_SetValue(cvarR, r);
		gEngfuncs.Cvar_SetValue(cvarG, g);
		gEngfuncs.Cvar_SetValue(cvarB, b);

		gEngfuncs.Con_Printf(
			"Set %s color to (%d, %d, %d)\n",
			cmdName, r, g, b);
	}
}