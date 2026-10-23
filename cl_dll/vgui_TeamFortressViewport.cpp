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
#include "vgui_StatsMenuPanel.h"

#include "shake.h"
#include "screenfade.h"

extern bool g_iVisibleMouse;
extern bool g_bShowMenu; // hud_imgui.cpp
extern bool UTIL_IsMultiplayer();
extern bool UTIL_IsSandbox();
class CCommandMenu;
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

//================================================================
// COMMAND MENU
//================================================================
void CCommandMenu::AddButton(CommandButton* pButton)
{
	if (m_iButtons >= MAX_BUTTONS)
		return;

	m_aButtons[m_iButtons] = pButton;
	m_iButtons++;
	pButton->setParent(this);
	pButton->setFont(Scheme::sf_primary3);

	// give the button a default key binding
	if (m_iButtons < 10)
	{
		pButton->setBoundKey(m_iButtons + '0');
	}
	else if (m_iButtons == 10)
	{
		pButton->setBoundKey('0');
	}
}

void CCommandMenu::RemoveAllButtons()
{
	/*
	for(int i=0;i<m_iButtons;i++)
	{
		CommandButton *pTemp = m_aButtons[i]; 
		m_aButtons[i] = NULL;
		
		pTemp
		if(pTemp)
		{
			delete(pTemp);
		}
		
	}
	*/
	removeAllChildren();
	m_iButtons = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Tries to find a button that has a key bound to the input, and
//			presses the button if found
// Input  : keyNum - the character number of the input key
// Output : Returns true if the command menu should close, false otherwise
//-----------------------------------------------------------------------------
bool CCommandMenu::KeyInput(int keyNum)
{
	// loop through all our buttons looking for one bound to keyNum
	for (int i = 0; i < m_iButtons; i++)
	{
		if (!m_aButtons[i]->IsNotValid())
		{
			if (m_aButtons[i]->getBoundKey() == keyNum)
			{
				// hit the button
				if (m_aButtons[i]->GetSubMenu())
				{
					// open the sub menu
					gViewPort->SetCurrentCommandMenu(m_aButtons[i]->GetSubMenu());
					return false;
				}
				else
				{
					// run the bound command
					m_aButtons[i]->fireActionSignal();
					return true;
				}
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: clears the current menus buttons of any armed (highlighted)
//			state, and all their sub buttons
//-----------------------------------------------------------------------------
void CCommandMenu::ClearButtonsOfArmedState()
{
	for (int i = 0; i < GetNumButtons(); i++)
	{
		m_aButtons[i]->setArmed(false);

		if (m_aButtons[i]->GetSubMenu())
		{
			m_aButtons[i]->GetSubMenu()->ClearButtonsOfArmedState();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pSubMenu -
// Output : CommandButton
//-----------------------------------------------------------------------------
CommandButton* CCommandMenu::FindButtonWithSubmenu(CCommandMenu* pSubMenu)
{
	for (int i = 0; i < GetNumButtons(); i++)
	{
		if (m_aButtons[i]->GetSubMenu() == pSubMenu)
			return m_aButtons[i];
	}

	return NULL;
}

// Recalculate the visible buttons
bool CCommandMenu::RecalculateVisibles(int iYOffset, bool bHideAll)
{
	int i, iCurrentY = 0;
	int iVisibleButtons = 0;

	// Cycle through all the buttons in this menu, and see which will be visible
	for (i = 0; i < m_iButtons; i++)
	{
		int iClass = m_aButtons[i]->GetPlayerClass();

		if ((0 != iClass && iClass != g_iPlayerClass) || (m_aButtons[i]->IsNotValid()) || bHideAll)
		{
			m_aButtons[i]->setVisible(false);
			if (m_aButtons[i]->GetSubMenu() != NULL)
			{
				(m_aButtons[i]->GetSubMenu())->RecalculateVisibles(0, true);
			}
		}
		else
		{
			// If it's got a submenu, force it to check visibilities
			if (m_aButtons[i]->GetSubMenu() != NULL)
			{
				if (!(m_aButtons[i]->GetSubMenu())->RecalculateVisibles(0, false))
				{
					// The submenu had no visible buttons, so don't display this button
					m_aButtons[i]->setVisible(false);
					continue;
				}
			}

			m_aButtons[i]->setVisible(true);
			iVisibleButtons++;
		}
	}

	// Set Size
	setSize(_size[0], (iVisibleButtons * (m_flButtonSizeY - 1)) + 1);

	if (0 != iYOffset)
	{
		m_iYOffset = iYOffset;
	}

	for (i = 0; i < m_iButtons; i++)
	{
		if (m_aButtons[i]->isVisible())
		{
			if (m_aButtons[i]->GetSubMenu() != NULL)
				(m_aButtons[i]->GetSubMenu())->RecalculateVisibles(iCurrentY + m_iYOffset, false);


			// Make sure it's at the right Y position
			// m_aButtons[i]->getPos( iXPos, iYPos );

			if (m_iDirection)
			{
				m_aButtons[i]->setPos(0, (iVisibleButtons - 1) * (m_flButtonSizeY - 1) - iCurrentY);
			}
			else
			{
				m_aButtons[i]->setPos(0, iCurrentY);
			}

			iCurrentY += (m_flButtonSizeY - 1);
		}
	}

	return 0 != iVisibleButtons;
}

// Make sure all submenus can fit on the screen
void CCommandMenu::RecalculatePositions(int iYOffset)
{
	int iTop;
	int iAdjust = 0;

	m_iYOffset += iYOffset;

	if (m_iDirection)
		iTop = ScreenHeight - (m_iYOffset + _size[1]);
	else
		iTop = m_iYOffset;

	if (iTop < 0)
		iTop = 0;

	// Calculate if this is going to fit onscreen, and shuffle it up if it won't
	int iBottom = iTop + _size[1];

	if (iBottom > ScreenHeight)
	{
		// Move in increments of button sizes
		while (iAdjust < (iBottom - ScreenHeight))
		{
			iAdjust += m_flButtonSizeY - 1;
		}

		iTop -= iAdjust;

		// Make sure it doesn't move off the top of the screen (the menu's too big to fit it all)
		if (iTop < 0)
		{
			iAdjust -= (0 - iTop);
			iTop = 0;
		}
	}

	setPos(_pos[0], iTop);

	// We need to force all menus below this one to update their positions now, because they
	// might have submenus riding off buttons in this menu that have just shifted.
	for (int i = 0; i < m_iButtons; i++)
		m_aButtons[i]->UpdateSubMenus(iAdjust);
}


// Make this menu and all menus above it in the chain visible
void CCommandMenu::MakeVisible(CCommandMenu* pChildMenu)
{
	/*
	// Push down the button leading to the child menu
	for (int i = 0; i < m_iButtons; i++)
	{
		if ( (pChildMenu != NULL) && (m_aButtons[i]->GetSubMenu() == pChildMenu) )
		{
			m_aButtons[i]->setArmed( true );
		}
		else
		{
			m_aButtons[i]->setArmed( false );
		}
	}
*/

	setVisible(true);
	if (m_pParentMenu)
		m_pParentMenu->MakeVisible(this);
}

//================================================================
// CreateSubMenu
CCommandMenu* TeamFortressViewport::CreateSubMenu(CommandButton* pButton, CCommandMenu* pParentMenu, int iYOffset, int iXOffset)
{
	int iXPos = 0;
	int iYPos = 0;
	int iWide = CMENU_SIZE_X;
	int iTall = 0;
	bool iDirection = false;

	if (pParentMenu)
	{
		iXPos = m_pCurrentCommandMenu->GetXOffset() + (CMENU_SIZE_X - 1) + iXOffset;
		iYPos = m_pCurrentCommandMenu->GetYOffset() + iYOffset;
		iDirection = pParentMenu->GetDirection();
	}

	CCommandMenu* pMenu = new CCommandMenu(pParentMenu, iDirection, iXPos, iYPos, iWide, iTall);
	pMenu->setParent(this);
	pButton->AddSubMenu(pMenu);
	pButton->setFont(Scheme::sf_primary3);
	pMenu->m_flButtonSizeY = m_pCurrentCommandMenu->m_flButtonSizeY;

	// Create the Submenu-open signal
	InputSignal* pISignal = new CMenuHandler_PopupSubMenuInput(pButton, pMenu);
	pButton->addInputSignal(pISignal);

	// Put a > to show it's a submenu
	CImageLabel* pLabel = new CImageLabel("arrowright", XRES(CMENU_SIZE_X - SUBMENU_SIZE_X), YRES(SUBMENU_SIZE_Y));
	pLabel->setParent(pButton);
	pLabel->addInputSignal(pISignal);

	// Reposition
	pLabel->getPos(iXPos, iYPos);
	pLabel->setPos(pButton->getWide() - pLabel->getImageWide() - 4, -4);

	// Create the mouse off signal for the Label too
	if (!pButton->m_bNoHighlight)
		pLabel->addInputSignal(new CHandler_CommandButtonHighlight(pButton));

	return pMenu;
}

//-----------------------------------------------------------------------------
// Purpose: Makes sure the memory allocated for TeamFortressViewport is nulled out
// Input  : stAllocateBlock -
// Output : void *
//-----------------------------------------------------------------------------
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
			// send a message to close the command menu
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
	m_pStatsMenu = NULL;
	m_pCurrentMenu = NULL;
	m_pCurrentCommandMenu = NULL;

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

	// VGUI MENUS (team / character menus and spectator panel: ImGui, imgui_hudpanels.cpp)
	CreateStatsMenu();

	// Init command menus
	m_iNumMenus = 0;
	m_iCurrentTeamNumber = m_iUser1 = m_iUser2 = m_iUser3 = 0;

	m_StandardMenu = CreateCommandMenu("commandmenu.txt", false, CMENU_TOP, false, CMENU_SIZE_X, BUTTON_SIZE_Y, 0);
}

//-----------------------------------------------------------------------------
// Purpose: Called everytime a new level is started. Viewport clears out it's data.
//-----------------------------------------------------------------------------
void TeamFortressViewport::Initialize()
{
	// Force each menu to Initialize
	GoModCTF_Reset();
	gHUD.m_Scoreboard.SetForced(false);

	if (m_pStatsMenu)
	{
		m_pStatsMenu->Initialize();
	}

	// Make sure all menus are hidden
	HideVGUIMenu();
	HideCommandMenu();

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

//-----------------------------------------------------------------------------
// Purpose: Read the Command Menu structure from the txt file and create the menu.
//			Returns Index of menu in m_pCommandMenus
//-----------------------------------------------------------------------------
int TeamFortressViewport::CreateCommandMenu(const char* menuFile, bool direction, int yOffset, bool flatDesign, float flButtonSizeX, float flButtonSizeY, int xOffset)
{
	// COMMAND MENU
	// Create the root of this new Command Menu

	int newIndex = m_iNumMenus;

	m_pCommandMenus[newIndex] = new CCommandMenu(NULL, direction, xOffset, yOffset, flButtonSizeX, 300); // This will be resized once we know how many items are in it
	m_pCommandMenus[newIndex]->setParent(this);
	m_pCommandMenus[newIndex]->setVisible(false);
	m_pCommandMenus[newIndex]->m_flButtonSizeY = flButtonSizeY;
	m_pCommandMenus[newIndex]->m_iSpectCmdMenu = direction;

	m_iNumMenus++;

	// Read Command Menu from the txt file
	char token[1024];
	char* pfile = (char*)gEngfuncs.COM_LoadFile(menuFile, 5, NULL);
	if (!pfile)
	{
		gEngfuncs.Con_DPrintf("Unable to open %s\n", menuFile);
		SetCurrentCommandMenu(NULL);
		return newIndex;
	}

	// First, read in the localisation strings

	// Detpack strings
	gHUD.m_TextMessage.LocaliseTextString("#DetpackSet_For5Seconds", m_sDetpackStrings[0], MAX_BUTTON_SIZE);
	gHUD.m_TextMessage.LocaliseTextString("#DetpackSet_For20Seconds", m_sDetpackStrings[1], MAX_BUTTON_SIZE);
	gHUD.m_TextMessage.LocaliseTextString("#DetpackSet_For50Seconds", m_sDetpackStrings[2], MAX_BUTTON_SIZE);

	// Now start parsing the menu structure
	m_pCurrentCommandMenu = m_pCommandMenus[newIndex];
	char szLastButtonText[32] = "file start";
	pfile = gEngfuncs.COM_ParseFile(pfile, token);
	while ((strlen(token) > 0) && (m_iNumMenus < MAX_MENUS))
	{
		// Keep looping until we hit the end of this menu
		while (token[0] != '}' && (strlen(token) > 0))
		{
			char cText[32] = "";
			char cBoundKey[32] = "";
			char cCustom[32] = "";
			static const int cCommandLength = 128;
			char cCommand[cCommandLength] = "";
			char szMap[MAX_MAPNAME] = "";
			int iPlayerClass = 0;
			bool iCustom = false;
			int iTeamOnly = -1;
			bool iToggle = false;
			int iButtonY;
			bool bGetExtraToken = true;
			CommandButton* pButton = NULL;

			// We should never be here without a Command Menu
			if (!m_pCurrentCommandMenu)
			{
				gEngfuncs.Con_Printf("Error in %s file after '%s'.\n", menuFile, szLastButtonText);
				m_iInitialized = false;
				return newIndex;
			}

			// token should already be the bound key, or the custom name
			strncpy(cCustom, token, 32);
			cCustom[31] = '\0';

			// See if it's a custom button
			if (0 == strcmp(cCustom, "CUSTOM"))
			{
				iCustom = true;

				// Get the next token
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
			}
			// See if it's a map
			else if (0 == strcmp(cCustom, "MAP"))
			{
				// Get the mapname
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
				strncpy(szMap, token, MAX_MAPNAME);
				szMap[MAX_MAPNAME - 1] = '\0';

				// Get the next token
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
			}
			else if (0 == strncmp(cCustom, "TEAM", 4)) // TEAM1, TEAM2, TEAM3, TEAM4
			{
				// make it a team only button
				iTeamOnly = atoi(cCustom + 4);

				// Get the next token
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
			}
			else if (0 == strncmp(cCustom, "TOGGLE", 6))
			{
				iToggle = true;
				// Get the next token
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
			}
			else
			{
				// See if it's a Class
			}

			// Get the button bound key
			strncpy(cBoundKey, token, 32);
			cText[31] = '\0';

			// Get the button text
			pfile = gEngfuncs.COM_ParseFile(pfile, token);

			CHudTextMessage::LocaliseTextString(token, cText, sizeof(cText));

			// save off the last button text we've come across (for error reporting)
			strcpy(szLastButtonText, cText);

			// Get the button command
			pfile = gEngfuncs.COM_ParseFile(pfile, token);
			strncpy(cCommand, token, cCommandLength);
			cCommand[cCommandLength - 1] = '\0';

			iButtonY = (BUTTON_SIZE_Y - 1) * m_pCurrentCommandMenu->GetNumButtons();

			// Custom button handling
			if (iCustom)
			{
				pButton = CreateCustomButton(cText, cCommand, iButtonY);

				// Get the next token to see if we're a menu
				pfile = gEngfuncs.COM_ParseFile(pfile, token);

				if (token[0] == '{')
				{
					strcpy(cCommand, token);
				}
				else
				{
					bGetExtraToken = false;
				}
			}
			else if (szMap[0] != '\0')
			{
				// create a map button
				pButton = new MapButton(szMap, cText, xOffset, iButtonY, flButtonSizeX, flButtonSizeY);
			}
			else if (iTeamOnly != -1)
			{
				// button that only shows up if the player is on team iTeamOnly
				pButton = new TeamOnlyCommandButton(iTeamOnly, cText, xOffset, iButtonY, flButtonSizeX, flButtonSizeY, flatDesign);
			}
			else if (iToggle && !direction)
			{
				pButton = new ToggleCommandButton(cCommand, cText, xOffset, iButtonY, flButtonSizeX, flButtonSizeY, flatDesign);
			}
			else if (direction)
			{
				if (iToggle)
					pButton = new SpectToggleButton(cCommand, cText, xOffset, iButtonY, flButtonSizeX, flButtonSizeY, flatDesign);
				else
					pButton = new SpectButton(iPlayerClass, cText, xOffset, iButtonY, flButtonSizeX, flButtonSizeY);
			}
			else
			{
				// normal button
				pButton = new CommandButton(iPlayerClass, cText, xOffset, iButtonY, flButtonSizeX, flButtonSizeY, flatDesign);
			}

			// add the button into the command menu
			if (pButton)
			{
				m_pCurrentCommandMenu->AddButton(pButton);
				pButton->setBoundKey(cBoundKey[0]);
				pButton->setParentMenu(m_pCurrentCommandMenu);

				// Override font in CommandMenu
				pButton->setFont(Scheme::sf_primary3);
			}

			// Find out if it's a submenu or a button we're dealing with
			if (cCommand[0] == '{')
			{
				if (m_iNumMenus >= MAX_MENUS)
				{
					gEngfuncs.Con_Printf("Too many menus in %s past '%s'\n", menuFile, szLastButtonText);
				}
				else if (pButton)
				{
					// Create the menu
					m_pCommandMenus[m_iNumMenus] = CreateSubMenu(pButton, m_pCurrentCommandMenu, iButtonY);
					m_pCurrentCommandMenu = m_pCommandMenus[m_iNumMenus];
					m_iNumMenus++;
				}
			}
			else if (!iCustom)
			{
				// Create the button and attach it to the current menu
				if (iToggle)
					pButton->addActionSignal(new CMenuHandler_ToggleCvar(cCommand));
				else
					pButton->addActionSignal(new CMenuHandler_StringCommand(cCommand));
				// Create an input signal that'll popup the current menu
				pButton->addInputSignal(new CMenuHandler_PopupSubMenuInput(pButton, m_pCurrentCommandMenu));
			}

			// Get the next token
			if (bGetExtraToken)
			{
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
			}
		}

		// Move back up a menu
		m_pCurrentCommandMenu = m_pCurrentCommandMenu->GetParentMenu();

		pfile = gEngfuncs.COM_ParseFile(pfile, token);
	}

	SetCurrentMenu(NULL);
	SetCurrentCommandMenu(NULL);
	gEngfuncs.COM_FreeFile(pfile);

	m_iInitialized = true;
	return newIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Creates all the class choices under a spy's disguise menus, and
//			maps a command to them
// Output : CCommandMenu
//-----------------------------------------------------------------------------
CCommandMenu* TeamFortressViewport::CreateDisguiseSubmenu(CommandButton* pButton, CCommandMenu* pParentMenu, const char* commandText, int iYOffset, int iXOffset)
{
	// create the submenu, under which the class choices will be listed
	CCommandMenu* pMenu = CreateSubMenu(pButton, pParentMenu, iYOffset, iXOffset);
	m_pCommandMenus[m_iNumMenus] = pMenu;
	m_iNumMenus++;

	// create the class choice buttons
	//#ifdef _TFC
	for (int i = PC_FIRSTCLASS; i <= PC_LASTCLASS; i++)
	{
		CommandButton* pDisguiseButton = new CommandButton(
			CHudTextMessage::BufferedLocaliseTextString(sLocalisedClasses[m_iCTFTeamNumber - 1][i]),
			0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y);

		char sz[256];
		sprintf(sz, "%s %d", commandText, i);
		pDisguiseButton->addActionSignal(new CMenuHandler_StringCommand(sz));

		pMenu->AddButton(pDisguiseButton);
	}
	//#endif

	return pMenu;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pButtonText -
//			*pButtonName -
// Output : CommandButton
//-----------------------------------------------------------------------------
CommandButton* TeamFortressViewport::CreateCustomButton(char* pButtonText, char* pButtonName, int iYOffset)
{
	CommandButton* pButton = NULL;
	CCommandMenu* pMenu = NULL;

	// ChangeTeam
	if (0 == strcmp(pButtonName, "!CHANGETEAM"))
	{
		// ChangeTeam Submenu
		pButton = new CommandButton(pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y);

		// Create the submenu
		pMenu = CreateSubMenu(pButton, m_pCurrentCommandMenu, iYOffset);
		m_pCommandMenus[m_iNumMenus] = pMenu;
		m_iNumMenus++;

		// ChangeTeam buttons
		for (int i = 0; i < 4; i++)
		{
			char sz[256];
			sprintf(sz, "jointeam %d", i + 1);
			m_pTeamButtons[i] = new TeamButton(i + 1, "teamname", 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y);
			m_pTeamButtons[i]->addActionSignal(new CMenuHandler_StringCommandWatch(sz));
			pMenu->AddButton(m_pTeamButtons[i]);
		}

		// Auto Assign button
		m_pTeamButtons[4] = new TeamButton(5, gHUD.m_TextMessage.BufferedLocaliseTextString("#Team_AutoAssign"), 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y);
		m_pTeamButtons[4]->addActionSignal(new CMenuHandler_StringCommand("jointeam 5"));
		pMenu->AddButton(m_pTeamButtons[4]);

		// Spectate button
		m_pTeamButtons[5] = new SpectateButton(CHudTextMessage::BufferedLocaliseTextString("#Menu_Spectate"), 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y, false);
		m_pTeamButtons[5]->addActionSignal(new CMenuHandler_StringCommand("spectate"));
		pMenu->AddButton(m_pTeamButtons[5]);
	}
	// ChangeClass
	else if (0 == strcmp(pButtonName, "!CHANGECLASS"))
	{
		// Create the Change class menu
		pButton = new ClassButton(-1, pButtonText, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y, false);

		// ChangeClass Submenu
		pMenu = CreateSubMenu(pButton, m_pCurrentCommandMenu, iYOffset);
		m_pCommandMenus[m_iNumMenus] = pMenu;
		m_iNumMenus++;

		//#ifdef _TFC
		for (int i = PC_FIRSTCLASS; i <= PC_LASTCLASS; i++)
		{
			char sz[256];

			// ChangeClass buttons
			CHudTextMessage::LocaliseTextString(sLocalisedClasses[m_iCTFTeamNumber - 1][i], sz, 256);
			ClassButton* pClassButton = new ClassButton(i, sz, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y, false);

			sprintf(sz, "%s", sCTFClassSelection[m_iCTFTeamNumber - 1][i]);
			pClassButton->addActionSignal(new CMenuHandler_StringCommandClassSelect(sz));
			pMenu->AddButton(pClassButton);
		}
		//#endif
	}

	return pButton;
}

void TeamFortressViewport::SaveStatsMenu()
{
	m_pStatsMenu->SaveStats();
}

CMenuPanel* TeamFortressViewport::ShowStatsMenu()
{
	if (0 != gEngfuncs.pDemoAPI->IsPlayingback())
	{
		return nullptr;
	}

	m_pStatsMenu->Reset();
	m_pStatsMenu->m_pSaveButton->setVisible(true);

	return m_pStatsMenu;
}

void TeamFortressViewport::SwitchToStatsMenu()
{
	gHUD.m_Scoreboard.SetForced(false); // Go-Mod: ImGui scoreboard (scoreboard.cpp)
	m_pStatsMenu->setVisible(true);

	g_iVisibleMouse = true;
	App::getInstance()->setCursorOveride(App::getInstance()->getScheme()->getCursor(Scheme::scu_arrow));
}

void TeamFortressViewport::SwitchToScoreBoard()
{
	m_pStatsMenu->setVisible(false);

	// Go-Mod: ImGui scoreboard (scoreboard.cpp), the mouse goes back to the game
	gHUD.m_Scoreboard.SetForced(true);

	IN_ResetMouse();
	g_iVisibleMouse = false;
	App::getInstance()->setCursorOveride(App::getInstance()->getScheme()->getCursor(Scheme::scu_none));
}

void TeamFortressViewport::HideScoreStatsWindow()
{
	gHUD.m_Scoreboard.SetForced(false);

	if (m_pStatsMenu)
	{
		if (m_pStatsMenu->isVisible())
		{
			m_pStatsMenu->setVisible(false);
		}
	}

	IN_ResetMouse();

	g_iVisibleMouse = false;
	App::getInstance()->setCursorOveride(App::getInstance()->getScheme()->getCursor(Scheme::scu_none));
}

//=======================================================================
void TeamFortressViewport::ShowCommandMenu(int menuIndex)
{
	if (!m_iInitialized)
		return;

	//Already have a menu open.
	if (m_pCurrentMenu)
		return;

	// is the command menu open?
	if (m_pCurrentCommandMenu == m_pCommandMenus[menuIndex])
	{
		HideCommandMenu();
		return;
	}

	// Not visible while in intermission
	if (gHUD.m_iIntermission)
		return;

	// Recalculate visible menus
	UpdateCommandMenu(menuIndex);
	HideVGUIMenu();

	SetCurrentCommandMenu(m_pCommandMenus[menuIndex]);
	m_flMenuOpenTime = gHUD.m_flTime;
	UpdateCursorState();

	// get command menu parameters
	for (int i = 2; i < gEngfuncs.Cmd_Argc(); i++)
	{
		const char* param = gEngfuncs.Cmd_Argv(i - 1);
		if (param)
		{
			if (m_pCurrentCommandMenu->KeyInput(param[0]))
			{
				// kill the menu open time, since the key input is final
				HideCommandMenu();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles the key input of "-commandmenu"
// Input  :
//-----------------------------------------------------------------------------
void TeamFortressViewport::InputSignalHideCommandMenu()
{
	if (!m_iInitialized)
		return;

	// if they've just tapped the command menu key, leave it open
	if ((m_flMenuOpenTime + 0.3) > gHUD.m_flTime)
		return;

	HideCommandMenu();
}

//-----------------------------------------------------------------------------
// Purpose: Hides the command menu
//-----------------------------------------------------------------------------
void TeamFortressViewport::HideCommandMenu()
{
	if (!m_iInitialized)
		return;

	if (m_pCommandMenus[m_StandardMenu])
	{
		m_pCommandMenus[m_StandardMenu]->ClearButtonsOfArmedState();
	}


	m_flMenuOpenTime = 0.0f;
	SetCurrentCommandMenu(NULL);
	UpdateCursorState();
}

//-----------------------------------------------------------------------------
// Purpose: Activate's the player special ability
//			called when the player hits their "special" key
//-----------------------------------------------------------------------------
void TeamFortressViewport::InputPlayerSpecial()
{
	if (!m_iInitialized)
		return;

	{
		// if it's any other class, just send the command down to the server
		EngineClientCmd("_special");
	}
}

// Set the submenu of the Command Menu
void TeamFortressViewport::SetCurrentCommandMenu(CCommandMenu* pNewMenu)
{
	for (int i = 0; i < m_iNumMenus; i++)
		m_pCommandMenus[i]->setVisible(false);

	m_pCurrentCommandMenu = pNewMenu;

	if (m_pCurrentCommandMenu)
		m_pCurrentCommandMenu->MakeVisible(NULL);
}

void TeamFortressViewport::UpdateCommandMenu(int menuIndex)
{
	m_pCommandMenus[menuIndex]->RecalculateVisibles(0, false);
	m_pCommandMenus[menuIndex]->RecalculatePositions(0);
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
		HideCommandMenu();
		GoModCTF_Open(iMenu);
		return;

	case MENU_STATSMENU:
		SwitchToStatsMenu();
		m_pStatsMenu->m_pSaveButton->setVisible(true);
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
	HideCommandMenu();

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

void TeamFortressViewport::CreateStatsMenu()
{
	// Create the panel
	m_pStatsMenu = new CStatsMenuPanel(100, false, 0, 0, ScreenWidth, ScreenHeight);
	m_pStatsMenu->setParent(this);
	m_pStatsMenu->setVisible(false);
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
	// the ImGui menus (spectator, team / character) own the cursor too
	if (GoModSpec_MenuOpen() || GoModCTF_IsOpen())
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
	else if (m_pCurrentCommandMenu)
	{
		// commandmenu doesn't have cursor if hud_capturemouse is turned off
		if (gHUD.m_pCvarStealMouse->value != 0.0f)
		{
			g_iVisibleMouse = true;
			App::getInstance()->setCursorOveride(App::getInstance()->getScheme()->getCursor(Scheme::scu_arrow));
			return;
		}
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

void TeamFortressViewport::UpdateHighlights()
{
	if (m_pCurrentCommandMenu)
		m_pCurrentCommandMenu->MakeVisible(NULL);
}

void TeamFortressViewport::paintBackground()
{
	int wide, tall;
	getParent()->getSize(wide, tall);
	setSize(wide, tall);

	// See if the command menu is visible and needs recalculating due to some external change
	if (g_iTeamNumber != m_iCurrentTeamNumber)
	{
		UpdateCommandMenu(m_StandardMenu);

		m_iCurrentTeamNumber = g_iTeamNumber;
	}

	if (g_iPlayerClass != m_iCurrentPlayerClass)
	{
		UpdateCommandMenu(m_StandardMenu);

		m_iCurrentPlayerClass = g_iPlayerClass;
	}

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

	// if we're in a command menu, try hit one of it's buttons
	if (down && m_pCurrentCommandMenu)
	{
		// Escape hides the command menu
		if (keynum == K_ESCAPE)
		{
			HideCommandMenu();
			return false;
		}

		// only trap the number keys
		if (keynum >= '0' && keynum <= '9')
		{
			if (m_pCurrentCommandMenu->KeyInput(keynum))
			{
				// a final command has been issued, so close the command menu
				HideCommandMenu();
			}

			return false;
		}
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

	// Force the menu to update
	UpdateCommandMenu(m_StandardMenu);

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

		// Set the team name buttons
		if (m_pTeamButtons[i])
			m_pTeamButtons[i]->setText(m_sTeamNames[teamNum]);

		// range check this value...m_pDisguiseButtons[5];
		if (teamNum < 5)
		{
			// Set the disguise buttons
			if (m_pDisguiseButtons[teamNum])
				m_pDisguiseButtons[teamNum]->setText(m_sTeamNames[teamNum]);
		}
	}

	return true;
}

bool TeamFortressViewport::MsgFunc_Feign(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_iIsFeigning = READ_BYTE() != 0;

	// Force the menu to update
	UpdateCommandMenu(m_StandardMenu);

	return true;
}

bool TeamFortressViewport::MsgFunc_Detpack(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_iIsSettingDetpack = READ_BYTE();

	// Force the menu to update
	UpdateCommandMenu(m_StandardMenu);

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

	// Force the menu to update
	UpdateCommandMenu(m_StandardMenu);

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

	// Force the menu to update
	UpdateCommandMenu(m_StandardMenu);

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

bool TeamFortressViewport::MsgFunc_StatsInfo(const char* pszName, int iSize, void* pbuf)
{
	return m_pStatsMenu->MsgFunc_StatsInfo(pszName, iSize, pbuf);
}

bool TeamFortressViewport::MsgFunc_StatsPlayer(const char* pszName, int iSize, void* pbuf)
{
	return m_pStatsMenu->MsgFunc_StatsPlayer(pszName, iSize, pbuf);
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
	return gViewPort && (gViewPort->m_pCurrentMenu || gViewPort->m_pCurrentCommandMenu);
}

// Go-Mod: CTF team / character menus (ImGui, imgui_hudpanels.cpp)
int GoModCTF_MenuTeamNumber()
{
	return gViewPort ? gViewPort->m_iCTFTeamNumber : 0;
}

void GoModCTF_SetStatsPlayerImage(const char* image)
{
	if (gViewPort && gViewPort->m_pStatsMenu)
		gViewPort->m_pStatsMenu->SetPlayerImage(image);
}
