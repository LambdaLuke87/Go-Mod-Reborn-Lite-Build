/**********************************************************************
 *                 Go-Mod: Reborn Buld Lite                           *
 *                   LambdaLuke87 - 2024                              *
 ********************************************************************/


#include "hud.h"
#include "cl_util.h"
#include "vgui_TeamFortressViewport.h"

CLinkMenu::CLinkMenu(int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CMenuPanel(iTrans, iRemoveMe, x,y,wide,tall)
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

	CommandButton* pButtonWeapons = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Sweeps"), iXPos + XRES(82), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	pButtonWeapons->addActionSignal(new CMenuHandler_StringCommand("toggleHLWeaponsMenu"));
	pButtonWeapons->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	pButtonWeapons->setParent(this);

	CommandButton* pButtonItems = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Items"), iXPos + XRES(148), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	pButtonItems->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	pButtonItems->addActionSignal(new CMenuHandler_StringCommand("toggleGMpropsMenu"));
	pButtonItems->setParent(this);

	CommandButton* pButtonMonsters = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("NPCS"), iXPos + XRES(214), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	pButtonMonsters->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	pButtonMonsters->addActionSignal(new CMenuHandler_StringCommand("toggleMonstersMenu"));
	pButtonMonsters->setParent(this);

	CommandButton* pButtonTools = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Tools"), iXPos + XRES(280), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	pButtonTools->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	pButtonTools->addActionSignal(new CMenuHandler_StringCommand("toggleGMToolsMenu"));
	pButtonTools->setParent(this);

	CommandButton* pButtonExit = new CommandButton(CHudTextMessage::BufferedLocaliseTextString(""), iXPos + XRES(410), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(149), BUTTON_SIZE_Y - YRES(16));
	pButtonExit->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	pButtonExit->setParent(this);

	// RemoveMode
	m_pMyPicture = new CImageLabel("toolgun_remove_select", 622, 75);
	m_pMyPicture->setVisible(true);
	m_pMyPicture->setParent(m_pPanel);

	m_pSpeak = new CommandButton("", 625, 77, 123, 49);
	m_pSpeak->setContentAlignment(vgui::Label::a_center);
	m_pSpeak->setParent(m_pPanel);
	m_pSpeak->addActionSignal(new CMenuHandler_StringCommand("button_tool_removemode"));
	m_pSpeak->addActionSignal(new CMenuHandler_StringCommand("weapon_removetool"));

	// DuplicateMode
	m_pMyPicture = new CImageLabel("toolgun_duplicate_select", 622, 127);
	m_pMyPicture->setVisible(true);
	m_pMyPicture->setParent(m_pPanel);

	m_pSpeak = new CommandButton("", 625, 129, 123, 49);
	m_pSpeak->setContentAlignment(vgui::Label::a_center);
	m_pSpeak->setParent(m_pPanel);
	m_pSpeak->addActionSignal(new CMenuHandler_StringCommand("button_tool_duplicatemode"));
	m_pSpeak->addActionSignal(new CMenuHandler_StringCommand("weapon_removetool"));

	// CameraMode
	m_pMyPicture = new CImageLabel("toolgun_camera_select", 622, 180);
	m_pMyPicture->setVisible(true);
	m_pMyPicture->setParent(m_pPanel);

	m_pSpeak = new CommandButton("", 625, 182, 123, 49);
	m_pSpeak->setContentAlignment(vgui::Label::a_center);
	m_pSpeak->setParent(m_pPanel);
	m_pSpeak->addActionSignal(new CMenuHandler_StringCommand("button_tool_cameramode"));
	m_pSpeak->addActionSignal(new CMenuHandler_StringCommand("weapon_removetool"));

	// GibMode
	m_pMyPicture = new CImageLabel("toolgun_gib_select", 622, 232);
	m_pMyPicture->setVisible(true);
	m_pMyPicture->setParent(m_pPanel);

	m_pSpeak = new CommandButton("", 625, 234, 123, 49);
	m_pSpeak->setContentAlignment(vgui::Label::a_center);
	m_pSpeak->setParent(m_pPanel);
	m_pSpeak->addActionSignal(new CMenuHandler_StringCommand("button_tool_gibmode"));
	m_pSpeak->addActionSignal(new CMenuHandler_StringCommand("weapon_removetool"));

	// PoserMode
	m_pMyPicture = new CImageLabel("toolgun_poser_select", 622, 284);
	m_pMyPicture->setVisible(true);
	m_pMyPicture->setParent(m_pPanel);

	m_pSpeak = new CommandButton("", 625, 286, 123, 49);
	m_pSpeak->setContentAlignment(vgui::Label::a_center);
	m_pSpeak->setParent(m_pPanel);
	m_pSpeak->addActionSignal(new CMenuHandler_StringCommand("button_tool_posermode"));
	m_pSpeak->addActionSignal(new CMenuHandler_StringCommand("weapon_removetool"));

	// Climbermode
	m_pMyPicture = new CImageLabel("physgun_cannon_select", 622, 354);
	m_pMyPicture->setVisible(true);
	m_pMyPicture->setParent(m_pPanel);

	m_pSpeak = new CommandButton("", 625, 356, 123, 49);
	m_pSpeak->setContentAlignment(vgui::Label::a_center);
	m_pSpeak->setParent(m_pPanel);
	m_pSpeak->addActionSignal(new CMenuHandler_StringCommand("button_pg_cannonmode"));
	m_pSpeak->addActionSignal(new CMenuHandler_StringCommand("weapon_physgun"));

	// PhysMode
	m_pMyPicture = new CImageLabel("physgun_climber_select", 622, 406);
	m_pMyPicture->setVisible(true);
	m_pMyPicture->setParent(m_pPanel);

	m_pSpeak = new CommandButton("", 625, 408, 123, 49);
	m_pSpeak->setContentAlignment(vgui::Label::a_center);
	m_pSpeak->setParent(m_pPanel);
	m_pSpeak->addActionSignal(new CMenuHandler_StringCommand("button_pg_climbmode"));
	m_pSpeak->addActionSignal(new CMenuHandler_StringCommand("weapon_physgun"));
}