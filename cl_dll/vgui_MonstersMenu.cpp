/**********************************************************************
 *                 Go-Mod: Reborn Buld Lite                           *
 *                   LambdaLuke87 - 2024                              *
 ********************************************************************/


#include "hud.h"
#include "cl_util.h"
#include "vgui_TeamFortressViewport.h"

CMonstersMenu::CMonstersMenu(int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CMenuPanel(iTrans, iRemoveMe, x, y, wide, tall)
{

	CSchemeManager* pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle("Title Font");

    m_pPanel = new CTransparentPanel(200, GMENU_WINDOW_X, GMENU_WINDOW_Y, GMENU_WINDOW_SIZE_X, GMENU_WINDOW_SIZE_Y);
	m_pPanel->setParent(this);
	m_pPanel->setBorder(new LineBorder(Color(0, 112, 0, 0)));
	m_pPanel->setVisible(true);

	int iXSize, iYSize, iXPos, iYPos;
	m_pPanel->getPos(iXPos, iYPos);
	m_pPanel->getSize(iXSize, iYSize);

	CommandButton* pButtonLinkMenu = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Main"), iXPos + XRES(16), iYPos + iYSize - YRES(270) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	pButtonLinkMenu->addActionSignal(new CMenuHandler_StringCommand("toggleLinkMenu"));
	pButtonLinkMenu->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	pButtonLinkMenu->setParent(this);

	CommandButton* pButtonWeapons = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Sweeps"), iXPos + XRES(82), iYPos + iYSize - YRES(270) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	pButtonWeapons->addActionSignal(new CMenuHandler_StringCommand("toggleHLWeaponsMenu"));
	pButtonWeapons->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	pButtonWeapons->setParent(this);

	CommandButton* pButtonItems = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Items"), iXPos + XRES(148), iYPos + iYSize - YRES(270) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	pButtonItems->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	pButtonItems->addActionSignal(new CMenuHandler_StringCommand("toggleGMpropsMenu"));
	pButtonItems->setParent(this);

	/*CommandButton* pButtonMonsters = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("NPCS"), iXPos + XRES(214), iYPos + iYSize - YRES(270) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	pButtonMonsters->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	pButtonMonsters->addActionSignal(new CMenuHandler_StringCommand("toggleMonstersMenu"));
	pButtonMonsters->setParent(this);*/

	CommandButton* pButtonTools = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Tools"), iXPos + XRES(280), iYPos + iYSize - YRES(270) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	pButtonTools->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	pButtonTools->addActionSignal(new CMenuHandler_StringCommand("toggleGMToolsMenu"));
	pButtonTools->setParent(this);

	//TODO
}