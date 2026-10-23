//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Client DLL VGUI Viewport
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
#include <VGUI_Cursor.h>
#include <VGUI_Frame.h>
#include <VGUI_Label.h>
#include <VGUI_Surface.h>
#include <VGUI_BorderLayout.h>
#include <VGUI_Panel.h>
#include <VGUI_ImagePanel.h>
#include <VGUI_Button.h>
#include <VGUI_ActionSignal.h>
#include <VGUI_InputSignal.h>
#include <VGUI_MenuSeparator.h>
#include <VGUI_TextPanel.h>
#include <VGUI_LoweredBorder.h>
#include <VGUI_LineBorder.h>
#include <VGUI_Scheme.h>
#include <VGUI_Font.h>
#include <VGUI_App.h>
#include <VGUI_BuildGroup.h>

#include "hud.h"
#include "cl_util.h"
#include "camera.h"
#include "kbutton.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "camera.h"
#include "in_defs.h"
#include "parsemsg.h"
#include "pm_shared.h"
#include "keydefs.h"
#include "demo.h"
#include "demo_api.h"

#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"

#include "shake.h"
#include "screenfade.h"

extern bool g_iVisibleMouse;
extern bool g_bShowMenu; // hud_imgui.cpp
extern bool UTIL_IsMultiplayer();
extern bool UTIL_IsSandbox();
int g_iPlayerClass;
int g_iTeamNumber;
int g_iUser1 = 0;
int g_iUser2 = 0;
int g_iUser3 = 0;

// Scoreboard positions
#define SBOARD_INDENT_X XRES(104)
#define SBOARD_INDENT_Y YRES(40)

// low-res scoreboard indents
#define SBOARD_INDENT_X_512 30
#define SBOARD_INDENT_Y_512 30

#define SBOARD_INDENT_X_400 0
#define SBOARD_INDENT_Y_400 20

void IN_ResetMouse();
void IN_ResetRelativeMouseState();
extern float* GetClientColor(int clientIndex);

extern void ImGuiMenu_SetOpen(bool open);
extern bool GoModInfo_QueueVGUIMenu(int iMenu);

// imgui_hudpanels.cpp: CTF team / character menus and spectator panel (ImGui)
extern void GoModCTF_Open(int menu);
extern void GoModCTF_Reset();
extern void GoModCTF_TeamFull(bool full);
extern bool GoModCTF_IsOpen();
extern bool GoModSpec_MenuOpen();

// imgui_hudpanels.cpp: command menu (commandmenu.txt) and CTF stats (ImGui)
extern bool GoModCmdMenu_IsOpen();
extern bool GoModCmdMenu_WantsCursor();
extern void GoModCmdMenu_Hide();
extern void GoModStats_Open();
extern void GoModStats_Close();
extern void GoModStats_Reset();
extern bool GoModStats_IsOpen();

using namespace vgui;

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


// Used for Class specific buttons
const char* sTFClasses[] =
	{
		"",
		"SCOUT",
		"SNIPER",
		"SOLDIER",
		"DEMOMAN",
		"MEDIC",
		"HWGUY",
		"PYRO",
		"SPY",
		"ENGINEER",
		"CIVILIAN",
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


// Get the name of TGA file, based on GameDir
char* GetVGUITGAName(const char* pszName)
{
	int i;
	char sz[256];
	static char gd[256];
	const char* gamedir;

	if (ScreenWidth < 640)
		i = 320;
	else
		i = 640;
	sprintf(sz, pszName, i);

	gamedir = gEngfuncs.pfnGetGameDirectory();
	sprintf(gd, "%s/gfx/vgui/%s.tga", gamedir, sz);

	return gd;
}

void* TeamFortressViewport::operator new(size_t stAllocateBlock)
{
	//	void *mem = Panel::operator new( stAllocateBlock );
	void* mem = ::operator new(stAllocateBlock);
	memset(mem, 0, stAllocateBlock);
	return mem;
}

//-----------------------------------------------------------------------------
// Purpose: InputSignal handler for the main viewport
//-----------------------------------------------------------------------------
class CViewPortInputHandler : public InputSignal
{
public:
	bool bPressed;

	CViewPortInputHandler()
	{
	}

	void cursorMoved(int x, int y, Panel* panel) override {}
	void cursorEntered(Panel* panel) override {}
	void cursorExited(Panel* panel) override {}
	void mousePressed(MouseCode code, Panel* panel) override
	{
		if (code != MOUSE_LEFT)
		{
			// send a message to close the command menu (ImGui)
			// this needs to be a message, since a direct call screws the timing
			gEngfuncs.pfnClientCmd("ForceCloseCommandMenu\n");
		}
	}
	void mouseReleased(MouseCode code, Panel* panel) override
	{
	}

	void mouseDoublePressed(MouseCode code, Panel* panel) override {}
	void mouseWheeled(int delta, Panel* panel) override {}
	void keyPressed(KeyCode code, Panel* panel) override {}
	void keyTyped(KeyCode code, Panel* panel) override {}
	void keyReleased(KeyCode code, Panel* panel) override {}
	void keyFocusTicked(Panel* panel) override {}
};


//================================================================
TeamFortressViewport::TeamFortressViewport(int x, int y, int wide, int tall) : Panel(x, y, wide, tall), m_SchemeManager(wide, tall)
{
	gViewPort = this;
	m_iInitialized = false;
	m_pCurrentMenu = NULL;

	Initialize();
	addInputSignal(new CViewPortInputHandler);

	int r, g, b, a;

	Scheme* pScheme = App::getInstance()->getScheme();

	// primary text color
	// Get the colors
	//!! two different types of scheme here, need to integrate
	SchemeHandle_t hPrimaryScheme = m_SchemeManager.getSchemeHandle("Primary Button Text");
	{
		// font
		pScheme->setFont(Scheme::sf_primary1, m_SchemeManager.getFont(hPrimaryScheme));

		// text color
		m_SchemeManager.getFgColor(hPrimaryScheme, r, g, b, a);
		pScheme->setColor(Scheme::sc_primary1, r, g, b, a); // sc_primary1 is non-transparent orange

		// background color (transparent black)
		m_SchemeManager.getBgColor(hPrimaryScheme, r, g, b, a);
		pScheme->setColor(Scheme::sc_primary3, r, g, b, a);

		// armed foreground color
		m_SchemeManager.getFgArmedColor(hPrimaryScheme, r, g, b, a);
		pScheme->setColor(Scheme::sc_secondary2, r, g, b, a);

		// armed background color
		m_SchemeManager.getBgArmedColor(hPrimaryScheme, r, g, b, a);
		pScheme->setColor(Scheme::sc_primary2, r, g, b, a);

		//!! need to get this color from scheme file
		// used for orange borders around buttons
		m_SchemeManager.getBorderColor(hPrimaryScheme, r, g, b, a);
		// pScheme->setColor(Scheme::sc_secondary1, r, g, b, a );
		pScheme->setColor(Scheme::sc_secondary1, 255 * 0.7, 170 * 0.7, 0, 0);
	}

	// Change the second primary font (used in the scoreboard)
	SchemeHandle_t hScoreboardScheme = m_SchemeManager.getSchemeHandle("Scoreboard Text");
	{
		pScheme->setFont(Scheme::sf_primary2, m_SchemeManager.getFont(hScoreboardScheme));
	}

	// Change the third primary font (used in command menu)
	SchemeHandle_t hCommandMenuScheme = m_SchemeManager.getSchemeHandle("CommandMenu Text");
	{
		pScheme->setFont(Scheme::sf_primary3, m_SchemeManager.getFont(hCommandMenuScheme));
	}

	App::getInstance()->setScheme(pScheme);

	// team / character menus, spectator panel, command menu and stats: ImGui (imgui_hudpanels.cpp)
}

//-----------------------------------------------------------------------------
// Purpose: Called everytime a new level is started. Viewport clears out it's data.
//-----------------------------------------------------------------------------
void TeamFortressViewport::Initialize()
{
	// Force each menu to Initialize
	GoModCTF_Reset();
	gHUD.m_Scoreboard.SetForced(false);

	GoModStats_Reset();

	// Make sure all menus are hidden
	HideVGUIMenu();
	GoModCmdMenu_Hide();

	// Clear out some data
	m_iRandomPC = false;

	// reset player info
	g_iPlayerClass = 0;
	g_iTeamNumber = 0;

	strcpy(m_szServerName, "");
	for (int i = 0; i < 5; i++)
	{
		m_iValidClasses[i] = 0;
		strcpy(m_sTeamNames[i], "");
	}

	App::getInstance()->setCursorOveride(App::getInstance()->getScheme()->getCursor(Scheme::scu_none));
}




void TeamFortressViewport::SwitchToScoreBoard()
{
	GoModStats_Close();

	// Go-Mod: ImGui scoreboard (scoreboard.cpp), the mouse goes back to the game
	gHUD.m_Scoreboard.SetForced(true);

	IN_ResetMouse();
	g_iVisibleMouse = false;
	App::getInstance()->setCursorOveride(App::getInstance()->getScheme()->getCursor(Scheme::scu_none));
}



void COM_FileBase(const char* in, char* out);

//======================================================================
// Set the VGUI Menu
void TeamFortressViewport::SetCurrentMenu(CMenuPanel* pMenu)
{
	m_pCurrentMenu = pMenu;
	if (m_pCurrentMenu)
	{
		// Don't open menus in demo playback
		if (0 != gEngfuncs.pDemoAPI->IsPlayingback())
			return;

		m_pCurrentMenu->Open();
	}
	else
	{
		gEngfuncs.pfnClientCmd("closemenus;");
	}
}

//================================================================
// VGUI Menus
void TeamFortressViewport::ShowVGUIMenu(int iMenu)
{
	// Go-Mod: the MOTD / Map Info (ImGui) is open: this menu is shown when it is closed
	if (GoModInfo_QueueVGUIMenu(iMenu))
		return;

	ImGuiMenu_SetOpen(false); // close ImGui

	CMenuPanel* pNewMenu = NULL;

	// Don't open menus in demo playback
	if (0 != gEngfuncs.pDemoAPI->IsPlayingback())
		return;

	/*
	// Don't open any menus except the stats menu during intermission
	// Stats menu is displayed during intermission.
	if (gHUD.m_iIntermission && iMenu != MENU_STATSMENU)
		return;
	*/

	// Don't create one if it's already in the list
	if (m_pCurrentMenu)
	{
		CMenuPanel* pMenu = m_pCurrentMenu;
		while (pMenu != NULL)
		{
			if (pMenu->GetMenuID() == iMenu)
				return;
			pMenu = pMenu->GetNextMenu();
		}
	}

	switch (iMenu)
	{
	// Go-Mod: CTF team / character menus are ImGui (imgui_hudpanels.cpp)
	case MENU_TEAM:
	case MENU_CLASS:
		GoModCmdMenu_Hide();
		GoModCTF_Open(iMenu);
		return;

	// Go-Mod: CTF end of game stats are ImGui (imgui_hudpanels.cpp)
	case MENU_STATSMENU:
		GoModCmdMenu_Hide();
		GoModStats_Open();
		return;

	case MENU_SCOREBOARD:
		SwitchToScoreBoard();
		return;

	default:
		break;
	}

	if (!pNewMenu)
		return;

	// Close the Command Menu if it's open
	GoModCmdMenu_Hide();

	pNewMenu->SetMenuID(iMenu);
	pNewMenu->SetActive(true);
	pNewMenu->setParent(this);

	// See if another menu is visible, and if so, cache this one for display once the other one's finished
	if (m_pCurrentMenu)
	{
		m_pCurrentMenu->SetNextMenu(pNewMenu);
	}
	else
	{
		m_pCurrentMenu = pNewMenu;
		m_pCurrentMenu->Open();
		UpdateCursorState();
	}
}

// Removes all VGUI Menu's onscreen
void TeamFortressViewport::HideVGUIMenu()
{
	while (m_pCurrentMenu)
	{
		HideTopMenu();
	}
}

// Remove the top VGUI menu, and bring up the next one
void TeamFortressViewport::HideTopMenu()
{
	if (m_pCurrentMenu)
	{
		// Close the top one
		m_pCurrentMenu->Close();

		// Bring up the next one
		gViewPort->SetCurrentMenu(m_pCurrentMenu->GetNextMenu());
	}

	UpdateCursorState();
}

// Return true if the HUD's allowed to print text messages
bool TeamFortressViewport::AllowedToPrintText()
{
	// Prevent text messages when the team / character menus are up (ImGui)
	if (GoModCTF_IsOpen() && g_iPlayerClass == 0)
		return false;

	return true;
}


//======================================================================================
// UPDATE HUD SECTIONS
//======================================================================================
// We've got an update on player info
// Recalculate any menus that use it.
void TeamFortressViewport::UpdateOnPlayerInfo()
{
}

void TeamFortressViewport::UpdateCursorState()
{
	// the Go-Mod ImGui menu is open: it owns the cursor (hud_imgui.cpp)
	if (g_bShowMenu)
	{
		g_iVisibleMouse = true;
		return;
	}

	// the scoreboard (ImGui) has the mouse to mute players
	if (GetClientVoiceMgr()->IsInSquelchMode())
	{
		g_iVisibleMouse = true;
		return;
	}

	// Need cursor if any VGUI window is up
	// the ImGui menus (spectator, team / character, command menu, stats) own the cursor too
	if (GoModSpec_MenuOpen() || GoModCTF_IsOpen() || GoModCmdMenu_WantsCursor() || GoModStats_IsOpen())
	{
		g_iVisibleMouse = true;
		return;
	}

	if (m_pCurrentMenu)
	{
		g_iVisibleMouse = true;
		App::getInstance()->setCursorOveride(App::getInstance()->getScheme()->getCursor(Scheme::scu_arrow));
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
	App::getInstance()->setCursorOveride(App::getInstance()->getScheme()->getCursor(Scheme::scu_none));
}


void TeamFortressViewport::paintBackground()
{
	int wide, tall;
	getParent()->getSize(wide, tall);
	setSize(wide, tall);

	int extents[4];
	getAbsExtents(extents[0], extents[1], extents[2], extents[3]);
	VGui_ViewportPaintBackground(extents);
}

//================================================================
// Input Handler for Drag N Drop panels
void CDragNDropHandler::cursorMoved(int x, int y, Panel* panel)
{
	if (m_bDragging)
	{
		App::getInstance()->getCursorPos(x, y);
		m_pPanel->setPos(m_iaDragOrgPos[0] + (x - m_iaDragStart[0]), m_iaDragOrgPos[1] + (y - m_iaDragStart[1]));

		if (m_pPanel->getParent() != null)
		{
			m_pPanel->getParent()->repaint();
		}
	}
}

void CDragNDropHandler::mousePressed(MouseCode code, Panel* panel)
{
	int x, y;
	App::getInstance()->getCursorPos(x, y);
	m_bDragging = true;
	m_iaDragStart[0] = x;
	m_iaDragStart[1] = y;
	m_pPanel->getPos(m_iaDragOrgPos[0], m_iaDragOrgPos[1]);
	App::getInstance()->setMouseCapture(panel);

	m_pPanel->setDragged(m_bDragging);
	m_pPanel->requestFocus();
}

void CDragNDropHandler::mouseReleased(MouseCode code, Panel* panel)
{
	m_bDragging = false;
	m_pPanel->setDragged(m_bDragging);
	App::getInstance()->setMouseCapture(null);
}

//================================================================
// Number Key Input
bool TeamFortressViewport::SlotInput(int iSlot)
{
	// If there's a menu up, give it the input
	if (m_pCurrentMenu)
		return m_pCurrentMenu->SlotInput(iSlot);

	return false;
}

// Direct Key Input
bool TeamFortressViewport::KeyInput(bool down, int keynum, const char* pszCurrentBinding)
{
	// Enter gets out of Spectator Mode by bringing up the Team Menu (CTF, ImGui)
	if (0 != g_iUser1 && 0 == gEngfuncs.Con_IsVisible() && gHUD.m_Teamplay == 2 && !GoModCTF_IsOpen())
	{
		if (down && (keynum == K_ENTER || keynum == K_KP_ENTER))
			ShowVGUIMenu(MENU_TEAM);
	}

	return true;
}

//================================================================
// Message Handlers
bool TeamFortressViewport::MsgFunc_ValClass(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	for (int i = 0; i < 5; i++)
		m_iValidClasses[i] = READ_SHORT();

	return true;
}

bool TeamFortressViewport::MsgFunc_TeamNames(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_iNumberOfTeams = READ_BYTE();

	for (int i = 0; i < m_iNumberOfTeams; i++)
	{
		int teamNum = i + 1;

		gHUD.m_TextMessage.LocaliseTextString(READ_STRING(), m_sTeamNames[teamNum], MAX_TEAMNAME_SIZE);

	}

	return true;
}

bool TeamFortressViewport::MsgFunc_Feign(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_iIsFeigning = READ_BYTE() != 0;

	return true;
}

bool TeamFortressViewport::MsgFunc_Detpack(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_iIsSettingDetpack = READ_BYTE();

	return true;
}

bool TeamFortressViewport::MsgFunc_VGUIMenu(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int iMenu = READ_BYTE();

	// Bring up the menu6
	ShowVGUIMenu(iMenu);

	return true;
}

bool TeamFortressViewport::MsgFunc_BuildSt(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_iBuildState = READ_SHORT();

	return true;
}

bool TeamFortressViewport::MsgFunc_RandomPC(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_iRandomPC = READ_BYTE() != 0;

	return true;
}

bool TeamFortressViewport::MsgFunc_ServerName(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	strncpy(m_szServerName, READ_STRING(), sizeof(m_szServerName));
	m_szServerName[sizeof(m_szServerName) - 1] = 0;

	return true;
}

bool TeamFortressViewport::MsgFunc_Spectator(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	short cl = READ_BYTE();
	if (cl > 0 && cl <= MAX_PLAYERS_HUD)
	{
		g_IsSpectator[cl] = READ_BYTE();
	}

	return true;
}

bool TeamFortressViewport::MsgFunc_AllowSpec(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_iAllowSpectators = READ_BYTE() != 0;

	return true;
}


// used to reset the player's screen immediately
bool TeamFortressViewport::MsgFunc_ResetFade(const char* pszName, int iSize, void* pbuf)
{

	return true;
}

// used to fade a player's screen out/in when they're spectating someone who is teleported
bool TeamFortressViewport::MsgFunc_SpecFade(const char* pszName, int iSize, void* pbuf)
{

	return true;
}

bool TeamFortressViewport::MsgFunc_TeamFull(const char* pszName, int iSize, void* pbuf)
{
	// Go-Mod: the team menu is ImGui (imgui_hudpanels.cpp)
	BEGIN_READ(pbuf, iSize);
	GoModCTF_TeamFull(READ_BYTE() != 0);
	return true;
}

bool TeamFortressViewport::MsgFunc_SetMenuTeam(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_iCTFTeamNumber = READ_BYTE();
	return true;
}



//-----------------------------------------------------------------------------
// Go-Mod: Admin Settings (ImGui copy of the command menu, imgui_gomod_menu.cpp)
// the team buttons of "CUSTOM !CHANGETEAM" need this, without including vgui there
//-----------------------------------------------------------------------------
int GoModAdmin_NumberOfTeams()
{
	return gViewPort ? gViewPort->GetNumberOfTeams() : 0;
}

const char* GoModAdmin_TeamName(int team)
{
	if (!gViewPort || team < 1 || team > 4)
		return "";
	return gViewPort->GetTeamName(team);
}

bool GoModAdmin_AllowSpectators()
{
	return gViewPort && gViewPort->GetAllowSpectators();
}

//-----------------------------------------------------------------------------
// Go-Mod: MOTD / Map Info (ImGui, imgui_gomod_menu.cpp)
//-----------------------------------------------------------------------------
const char* GoModInfo_ServerName()
{
	return gViewPort ? gViewPort->m_szServerName : "";
}

// a VGUI menu that waited for the MOTD / Map Info to be closed
void GoModInfo_ShowVGUIMenu(int iMenu)
{
	if (gViewPort)
		gViewPort->ShowVGUIMenu(iMenu);
}

// a VGUI menu (team, class...) is open
bool GoModInfo_VGUIMenuOpen()
{
	return (gViewPort && gViewPort->m_pCurrentMenu) || GoModCmdMenu_IsOpen();
}

// Go-Mod: CTF team / character menus (ImGui, imgui_hudpanels.cpp)
int GoModCTF_MenuTeamNumber()
{
	return gViewPort ? gViewPort->m_iCTFTeamNumber : 0;
}

