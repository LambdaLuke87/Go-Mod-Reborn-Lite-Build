/**********************************************************************
 *                 Go-Mod: Reborn Buld Lite                           *
 *                   LambdaLuke87 - 2024                              *
 ********************************************************************/


#include "hud.h"
#include "cl_util.h"
#include "vgui_TeamFortressViewport.h"

CWeaponsMenu ::CWeaponsMenu(int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CMenuPanel(iTrans, iRemoveMe, x, y, wide, tall)
{
	CSchemeManager* pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle("Title Font");

	int lado;
	int alto;
	int x_lado;

	lado = 10;
	alto = 70;

	x_lado = 155;

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

	/*CommandButton* pButtonWeapons = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Sweeps"), iXPos + XRES(82), iYPos + iYSize - YRES(270) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	pButtonWeapons->addActionSignal(new CMenuHandler_StringCommand("toggleHLWeaponsMenu"));
	pButtonWeapons->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	pButtonWeapons->setParent(this);*/

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

	// AimSpawn Start
	m_pSpeak = new CommandButton("", 659, 10, 50, 30);
	m_pSpeak->setContentAlignment(vgui::Label::a_center);
	m_pSpeak->setParent(m_pPanel);
	m_pSpeak->addActionSignal(new CMenuHandler_StringCommand("button_aim_spawn"));

	CommandButton* pButtonWeapon;
	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Crowbar"), iXPos + XRES(16), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_crowbar"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Physgun"), iXPos + XRES(80), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_physgun"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Toolgun"), iXPos + XRES(144), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_removetool"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Glock"), iXPos + XRES(208), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_9mmhandgun"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Python"), iXPos + XRES(272), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_357"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Mp5"), iXPos + XRES(336), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_9mmAR"));
	pButtonWeapon->setParent(this);



	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Shotgun"), iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_shotgun"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Crossbow"), iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_crossbow"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("RPG"), iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_rpg"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Gauss"), iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_gauss"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Egon"), iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_egon"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("HiveHand"), iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_hornetgun"));
	pButtonWeapon->setParent(this);



	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Grenade"), iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_handgrenade"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Satchel"), iXPos + XRES(80), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_satchel"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Tripmine"), iXPos + XRES(144), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_tripmine"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Snark"), iXPos + XRES(208), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_snark"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("BGraple"), iXPos + XRES(272), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_grapple"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Displacer"), iXPos + XRES(336), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_displacer"));
	pButtonWeapon->setParent(this);



	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Knife"), iXPos + XRES(16), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_knife"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("SniperRifle"), iXPos + XRES(80), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_sniperrifle"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Penguin"), iXPos + XRES(144), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_penguin"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("PipeWrench"), iXPos + XRES(208), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_pipewrench"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("M249"), iXPos + XRES(272), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_m249"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("ShockRifle"), iXPos + XRES(336), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_shockrifle"));
	pButtonWeapon->setParent(this);



	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Spore Launcher"), iXPos + XRES(16), iYPos + iYSize - YRES(64) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_sporelauncher"));
	pButtonWeapon->setParent(this);

	pButtonWeapon = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Desert Egle"), iXPos + XRES(80), iYPos + iYSize - YRES(64) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonWeapon->addActionSignal(new CMenuHandler_StringCommand("button_weapon_eagle"));
	pButtonWeapon->setParent(this);
}