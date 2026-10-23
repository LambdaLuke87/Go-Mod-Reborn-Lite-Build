#pragma once

// Go-Mod: Reborn - the game data that the old VGUI viewport (TeamFortressViewport) kept,
// without VGUI: team names, server name, valid classes, spectators, the CTF team of the
// character menu and the server message "VGUIMenu" (it opens the ImGui menus).
// All the panels are Dear ImGui now (scoreboard.cpp, imgui/imgui_hudpanels.cpp, imgui/imgui_gomod_menu.cpp).

#include "global_consts.h"

const int StatsTeamsCount = 4;

extern char* sCTFClassSelection[][7];
extern char* sLocalisedClasses[][7];
extern const char* sLocalisedStatsTeams[StatsTeamsCount];
extern const char* sCTFStatsSelection[StatsTeamsCount];
extern int iTeamColors[5][3];
extern int iNumberOfTeamColors;

extern void GoModSpec_Update();	 // imgui_hudpanels.cpp
extern void GoModCmdMenu_Hide(); // imgui_hudpanels.cpp

class CGoModViewport
{
public:
	void Initialize(); // every new level

	void UpdateCursorState();
	bool KeyInput(bool down, int keynum, const char* pszCurrentBinding);
	bool AllowedToPrintText();

	void ShowGameMenu(int iMenu); // MENU_TEAM, MENU_CLASS, MENU_STATSMENU, MENU_SCOREBOARD
	void SwitchToScoreBoard();

	int GetValidClasses(int iTeam) { return m_iValidClasses[iTeam]; }
	int GetNumberOfTeams() { return m_iNumberOfTeams; }
	bool GetIsFeigning() { return m_iIsFeigning; }
	int GetIsSettingDetpack() { return m_iIsSettingDetpack; }
	int GetBuildState() { return m_iBuildState; }
	bool IsRandomPC() { return m_iRandomPC; }
	const char* GetTeamName(int iTeam) { return m_sTeamNames[iTeam]; }
	bool GetAllowSpectators() { return m_iAllowSpectators; }

	// Message Handlers
	bool MsgFunc_ValClass(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_TeamNames(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_Feign(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_Detpack(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_VGUIMenu(const char* pszName, int iSize, void* pbuf); // server message name
	bool MsgFunc_BuildSt(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_RandomPC(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_ServerName(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_Spectator(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_AllowSpec(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_SpecFade(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_ResetFade(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_TeamFull(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_SetMenuTeam(const char* pszName, int iSize, void* pbuf);

public:
	int m_iCTFTeamNumber = 0;
	char m_szServerName[MAX_SERVERNAME_LENGTH] = {};

private:
	bool m_iAllowSpectators = false;

	int m_iValidClasses[5] = {};
	bool m_iIsFeigning = false;
	int m_iIsSettingDetpack = 0;
	int m_iNumberOfTeams = 0;
	int m_iBuildState = 0;
	bool m_iRandomPC = false;
	char m_sTeamNames[5][MAX_TEAMNAME_SIZE] = {};
};

extern CGoModViewport* gViewPort; // never NULL

void GoModViewport_VidInit(); // HUD_VidInit
