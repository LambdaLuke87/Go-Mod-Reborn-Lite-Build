//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// Go-Mod: Reborn - game data of the old client viewport (HL SDK TeamFortressViewport),
// without VGUI. See gomod_viewport.h.
//
//=============================================================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "keydefs.h"
#include "demo.h"
#include "demo_api.h"
#define TF_DEFS_ONLY
#include "tf_defs.h"
#include "voice_status.h"

#include "gomod_viewport.h"

extern bool g_iVisibleMouse;
extern bool g_bShowMenu; // hud_imgui.cpp

int g_iPlayerClass;
int g_iTeamNumber;
int g_iUser1 = 0;
int g_iUser2 = 0;
int g_iUser3 = 0;

void IN_ResetMouse();
void IN_ResetRelativeMouseState();

extern void ImGuiMenu_SetOpen(bool open);
extern bool GoModInfo_QueueGameMenu(int iMenu); // imgui_gomod_menu.cpp

// imgui_hudpanels.cpp: CTF team / character menus, spectator panel, command menu and CTF stats (ImGui)
extern void GoModCTF_Open(int menu);
extern void GoModCTF_Reset();
extern void GoModCTF_TeamFull(bool full);
extern bool GoModCTF_IsOpen();
extern bool GoModSpec_MenuOpen();
extern bool GoModCmdMenu_IsOpen();
extern bool GoModCmdMenu_WantsCursor();
extern void GoModStats_Open();
extern void GoModStats_Close();
extern void GoModStats_Reset();
extern bool GoModStats_IsOpen();

static CGoModViewport g_GoModViewport;
CGoModViewport* gViewPort = &g_GoModViewport;

// Team Colors
int iNumberOfTeamColors = 5;
int iTeamColors[5][3] =
	{
		{255, 170, 0},	 // HL orange (default)
		{125, 165, 210}, // Blue
		{200, 90, 70},	 // Red
		{225, 205, 45},	 // Yellow
		{145, 215, 140}, // Green
};

char* sLocalisedClasses[][7] =
	{
		{"#Barney",
			"#Cleansuit",
			"#Gina",
			"#Gordon",
			"#Otis",
			"#Scientist",
			"#Civ_Random"},
		{"#SquadLeader",
			"#DrillSgt",
			"#Grunt",
			"#Recruit",
			"#Adrian",
			"#Tower",
			"#Op4_Random"}};

char* sCTFClassSelection[][7] =
	{
		{"Barney",
			"Cleansuit",
			"Gina",
			"Gordon",
			"Otis",
			"Scientist",
			"Civ_Random"},
		{"SquadLeader",
			"DrillSgt",
			"Grunt",
			"Recruit",
			"Adrian",
			"Tower",
			"Op4_Random"}};

const char* sLocalisedStatsTeams[StatsTeamsCount] =
	{
		"#CTFTitleStats_Both",
		"#CTFTitleStats_BlackMesa",
		"#CTFTitleStats_OpposingForce",
		"#CTFTitleStats_Individual",
};

const char* sCTFStatsSelection[StatsTeamsCount] =
	{
		"Both",
		"BlackMesa",
		"OpposingForce",
		"Individual"};

void GoModViewport_VidInit()
{
	gViewPort->Initialize();
}

//-----------------------------------------------------------------------------
// Purpose: Called everytime a new level is started. Clears out the data.
//-----------------------------------------------------------------------------
void CGoModViewport::Initialize()
{
	GoModCTF_Reset();
	gHUD.m_Scoreboard.SetForced(false);
	GoModStats_Reset();
	GoModCmdMenu_Hide();

	m_iRandomPC = false;

	// reset player info
	g_iPlayerClass = 0;
	g_iTeamNumber = 0;

	m_szServerName[0] = '\0';
	for (int i = 0; i < 5; i++)
	{
		m_iValidClasses[i] = 0;
		m_sTeamNames[i][0] = '\0';
	}
}

void CGoModViewport::SwitchToScoreBoard()
{
	GoModStats_Close();

	// ImGui scoreboard (scoreboard.cpp), the mouse goes back to the game
	gHUD.m_Scoreboard.SetForced(true);

	IN_ResetMouse();
	g_iVisibleMouse = false;
}

//================================================================
// server message "VGUIMenu": the menus are ImGui
void CGoModViewport::ShowGameMenu(int iMenu)
{
	// the MOTD / Map Info (ImGui) is open: this menu is shown when it is closed
	if (GoModInfo_QueueGameMenu(iMenu))
		return;

	// Don't open menus in demo playback
	if (0 != gEngfuncs.pDemoAPI->IsPlayingback())
		return;

	switch (iMenu)
	{
	// CTF team / character menus (imgui_hudpanels.cpp)
	case MENU_TEAM:
	case MENU_CLASS:
		ImGuiMenu_SetOpen(false);
		GoModCmdMenu_Hide();
		GoModCTF_Open(iMenu);
		break;

	// CTF end of game stats (imgui_hudpanels.cpp)
	case MENU_STATSMENU:
		ImGuiMenu_SetOpen(false);
		GoModCmdMenu_Hide();
		GoModStats_Open();
		break;

	case MENU_SCOREBOARD:
		ImGuiMenu_SetOpen(false);
		SwitchToScoreBoard();
		break;

	default:
		break;
	}
}

// Return true if the HUD's allowed to print text messages
bool CGoModViewport::AllowedToPrintText()
{
	// Prevent text messages when the team / character menus are up
	if (GoModCTF_IsOpen() && g_iPlayerClass == 0)
		return false;

	return true;
}

void CGoModViewport::UpdateCursorState()
{
	// the Go-Mod ImGui menu is open: it owns the cursor (hud_imgui.cpp)
	if (g_bShowMenu)
	{
		g_iVisibleMouse = true;
		return;
	}

	// the scoreboard has the mouse to mute players
	if (GetClientVoiceMgr()->IsInSquelchMode())
	{
		g_iVisibleMouse = true;
		return;
	}

	// the ImGui menus (spectator, team / character, command menu, stats) own the cursor too
	if (GoModSpec_MenuOpen() || GoModCTF_IsOpen() || GoModCmdMenu_WantsCursor() || GoModStats_IsOpen())
	{
		g_iVisibleMouse = true;
		return;
	}

	// Don't reset mouse in demo playback
	if (0 == gEngfuncs.pDemoAPI->IsPlayingback())
	{
		IN_ResetMouse();
	}

	if (g_iVisibleMouse)
	{
		// Clear any residual input so our camera doesn't jerk when dismissing the UI
		IN_ResetRelativeMouseState();
	}

	g_iVisibleMouse = false;
}

// Direct Key Input
bool CGoModViewport::KeyInput(bool down, int keynum, const char* pszCurrentBinding)
{
	// Enter gets out of Spectator Mode by bringing up the Team Menu (CTF)
	if (0 != g_iUser1 && 0 == gEngfuncs.Con_IsVisible() && gHUD.m_Teamplay == 2 && !GoModCTF_IsOpen())
	{
		if (down && (keynum == K_ENTER || keynum == K_KP_ENTER))
			ShowGameMenu(MENU_TEAM);
	}

	return true;
}

//================================================================
// Message Handlers
bool CGoModViewport::MsgFunc_ValClass(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	for (int i = 0; i < 5; i++)
		m_iValidClasses[i] = READ_SHORT();

	return true;
}

bool CGoModViewport::MsgFunc_TeamNames(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_iNumberOfTeams = READ_BYTE();
	if (m_iNumberOfTeams > 4)
		m_iNumberOfTeams = 4;

	for (int i = 0; i < m_iNumberOfTeams; i++)
	{
		const int teamNum = i + 1;
		gHUD.m_TextMessage.LocaliseTextString(READ_STRING(), m_sTeamNames[teamNum], MAX_TEAMNAME_SIZE);
	}

	return true;
}

bool CGoModViewport::MsgFunc_Feign(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_iIsFeigning = READ_BYTE() != 0;
	return true;
}

bool CGoModViewport::MsgFunc_Detpack(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_iIsSettingDetpack = READ_BYTE();
	return true;
}

bool CGoModViewport::MsgFunc_VGUIMenu(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	ShowGameMenu(READ_BYTE());
	return true;
}

bool CGoModViewport::MsgFunc_BuildSt(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_iBuildState = READ_SHORT();
	return true;
}

bool CGoModViewport::MsgFunc_RandomPC(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_iRandomPC = READ_BYTE() != 0;
	return true;
}

bool CGoModViewport::MsgFunc_ServerName(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	strncpy(m_szServerName, READ_STRING(), sizeof(m_szServerName));
	m_szServerName[sizeof(m_szServerName) - 1] = 0;

	return true;
}

bool CGoModViewport::MsgFunc_Spectator(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	short cl = READ_BYTE();
	if (cl > 0 && cl <= MAX_PLAYERS_HUD)
	{
		g_IsSpectator[cl] = READ_BYTE();
	}

	return true;
}

bool CGoModViewport::MsgFunc_AllowSpec(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_iAllowSpectators = READ_BYTE() != 0;
	return true;
}

// used to reset the player's screen immediately (nothing to do, the message must be hooked)
bool CGoModViewport::MsgFunc_ResetFade(const char* pszName, int iSize, void* pbuf)
{
	return true;
}

// used to fade a player's screen out/in when they're spectating someone who is teleported
bool CGoModViewport::MsgFunc_SpecFade(const char* pszName, int iSize, void* pbuf)
{
	return true;
}

bool CGoModViewport::MsgFunc_TeamFull(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	GoModCTF_TeamFull(READ_BYTE() != 0);
	return true;
}

bool CGoModViewport::MsgFunc_SetMenuTeam(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_iCTFTeamNumber = READ_BYTE();
	return true;
}

//-----------------------------------------------------------------------------
// Accessors for the ImGui files (they don't include this header)
//-----------------------------------------------------------------------------

// Admin Settings / command menu: the team buttons of "CUSTOM !CHANGETEAM"
int GoModAdmin_NumberOfTeams()
{
	return gViewPort->GetNumberOfTeams();
}

const char* GoModAdmin_TeamName(int team)
{
	if (team < 1 || team > 4)
		return "";
	return gViewPort->GetTeamName(team);
}

bool GoModAdmin_AllowSpectators()
{
	return gViewPort->GetAllowSpectators();
}

// MOTD / Map Info (imgui_gomod_menu.cpp)
const char* GoModInfo_ServerName()
{
	return gViewPort->m_szServerName;
}

// a game menu that waited for the MOTD / Map Info to be closed
void GoModInfo_ShowGameMenu(int iMenu)
{
	gViewPort->ShowGameMenu(iMenu);
}

// a game menu (team, character, command menu, stats) is open
bool GoModInfo_GameMenuOpen()
{
	return GoModCTF_IsOpen() || GoModCmdMenu_IsOpen() || GoModStats_IsOpen();
}

// CTF team / character menus (imgui_hudpanels.cpp)
int GoModCTF_MenuTeamNumber()
{
	return gViewPort->m_iCTFTeamNumber;
}
