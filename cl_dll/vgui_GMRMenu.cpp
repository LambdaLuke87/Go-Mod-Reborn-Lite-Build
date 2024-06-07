/**********************************************************************
 *                 Go-Mod: Reborn Lite Build                          *
 *                   LambdaLuke87 - 2024                              *
 ********************************************************************/


#include "hud.h"
#include "cl_util.h"
#include "vgui_TeamFortressViewport.h"
#include "VGUI_TextImage.h"

#define LINKMENU_TITLE_X XRES(16)
#define LINKMENU_TITLE_Y YRES(16)

CGMMenuBase::CGMMenuBase(int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CMenuPanel(iTrans, iRemoveMe, x, y, wide, tall)
{
	m_pPanel = new CTransparentPanel(200, GMENU_WINDOW_X, GMENU_WINDOW_Y, GMENU_WINDOW_SIZE_X, GMENU_WINDOW_SIZE_Y);
	m_pPanel->setParent(this);
	m_pPanel->setBorder(new LineBorder(Color(255, 255, 255, 0)));
	m_pPanel->setVisible(true);

	m_pPanel->getPos(iXPos, iYPos);
	m_pPanel->getSize(iXSize, iYSize);

	m_pButtonLinkMenu = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Main"), iXPos + XRES(16), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	m_pButtonLinkMenu->addActionSignal(new CMenuHandler_StringCommand("toggleLinkMenu"));
	m_pButtonLinkMenu->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	m_pButtonLinkMenu->setParent(this);

	m_pButtonSweepMenu = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Sweeps"), iXPos + XRES(82), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	m_pButtonSweepMenu->addActionSignal(new CMenuHandler_StringCommand("toggleWeaponsMenu"));
	m_pButtonSweepMenu->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	m_pButtonSweepMenu->setParent(this);

	m_pButtonItemsMenu = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Items"), iXPos + XRES(148), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	m_pButtonItemsMenu->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	m_pButtonItemsMenu->addActionSignal(new CMenuHandler_StringCommand("toggleItemsMenu"));
	m_pButtonItemsMenu->setParent(this);

	m_pButtonNPCMenu = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("NPCS"), iXPos + XRES(214), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	m_pButtonNPCMenu->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	m_pButtonNPCMenu->addActionSignal(new CMenuHandler_StringCommand("toggleMonstersMenu"));
	m_pButtonNPCMenu->setParent(this);

	m_pButtonToolsMenu = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Tools"), iXPos + XRES(280), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	m_pButtonToolsMenu->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	m_pButtonToolsMenu->addActionSignal(new CMenuHandler_StringCommand("toggleToolsMenu"));
	m_pButtonToolsMenu->setParent(this);

	m_pButtonAimMode = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Aim"), iXPos + XRES(370), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(135), BUTTON_SIZE_Y - YRES(8));
	m_pButtonAimMode->addActionSignal(new CMenuHandler_StringCommand("button_aim_spawn"));
	m_pButtonAimMode->setParent(this);
	m_pButtonAimMode->setVisible(false);

	CommandButton* pButtonExit = new CommandButton(CHudTextMessage::BufferedLocaliseTextString(""), iXPos + XRES(410), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(149), BUTTON_SIZE_Y - YRES(16));
	pButtonExit->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	pButtonExit->setParent(this);
}

CMenuPanel* CLinkMenu_Create(const char* szMOTD, int iTrans, bool iRemoveMe, int x, int y, int wide, int tall)
{
	return new CLinkMenu(szMOTD, iTrans, iRemoveMe, x, y, wide, tall);
}

CLinkMenu::CLinkMenu(const char* szMOTD, int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CGMMenuBase(iTrans, iRemoveMe, x, y, wide, tall)
{
	m_pButtonLinkMenu->setVisible(false);

	CSchemeManager* pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle("Title Font");

	// color schemes
	int r, g, b, a;

	// Create the Scroll panel
	ScrollPanel* pScrollPanel = new CTFScrollPanel(iXPos + XRES(16), iYPos + LINKMENU_TITLE_Y * 2 + YRES(16), iXSize - XRES(32), iYSize - (YRES(48) + BUTTON_SIZE_Y * 2));
	pScrollPanel->setParent(this);

	// force the scrollbars on so clientClip will take them in account after the validate
	pScrollPanel->setScrollBarAutoVisible(false, false);
	pScrollPanel->setScrollBarVisible(true, true);
	pScrollPanel->validate();

	// Create the text panel
	TextPanel* pText = new TextPanel("", 0, 0, 64, 64);
	pText->setParent(pScrollPanel->getClient());

	// get the font and colors from the scheme
	pText->setFont(pSchemes->getFont(hTitleScheme));
	pSchemes->getFgColor(hTitleScheme, r, g, b, a);
	pText->setFgColor(r, g, b, a);
	pSchemes->getBgColor(hTitleScheme, r, g, b, a);
	pText->setBgColor(r, g, b, a);
	pText->setText(szMOTD);

	// Get the total size of the MOTD text and resize the text panel
	int iScrollSizeX, iScrollSizeY;

	// First, set the size so that the client's wdith is correct at least because the
	//  width is critical for getting the "wrapped" size right.
	// You'll see a horizontal scroll bar if there is a single word that won't wrap in the
	//  specified width.
	pText->getTextImage()->setSize(pScrollPanel->getClientClip()->getWide(), pScrollPanel->getClientClip()->getTall());
	pText->getTextImage()->getTextSizeWrapped(iScrollSizeX, iScrollSizeY);

	// Now resize the textpanel to fit the scrolled size
	pText->setSize(iScrollSizeX, iScrollSizeY);

	// turn the scrollbars back into automode
	pScrollPanel->setScrollBarAutoVisible(true, true);
	pScrollPanel->setScrollBarVisible(false, false);

	pScrollPanel->validate();
}

CWeaponsMenu ::CWeaponsMenu(int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CGMMenuBase(iTrans, iRemoveMe, x, y, wide, tall)
{
	m_pButtonSweepMenu->setVisible(false);
	m_pButtonAimMode->setVisible(true);

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

CItemsMenu::CItemsMenu(int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CGMMenuBase(iTrans, iRemoveMe, x, y, wide, tall)
{
	m_pButtonItemsMenu->setVisible(false);
	m_pButtonAimMode->setVisible(true);

	CommandButton* pButtonItem;
	pButtonItem = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("HealthKit"), iXPos + XRES(16), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonItem->addActionSignal(new CMenuHandler_StringCommand("button_item_healthkit"));
	pButtonItem->setParent(this);

	pButtonItem = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Battery"), iXPos + XRES(80), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonItem->addActionSignal(new CMenuHandler_StringCommand("button_item_battery"));
	pButtonItem->setParent(this);

	pButtonItem = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("LonJump"), iXPos + XRES(144), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonItem->addActionSignal(new CMenuHandler_StringCommand("button_item_longjump"));
	pButtonItem->setParent(this);

	pButtonItem = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("HEV Suit"), iXPos + XRES(208), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonItem->addActionSignal(new CMenuHandler_StringCommand("button_item_suit"));
	pButtonItem->setParent(this);

	pButtonItem = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Glock Ammo"), iXPos + XRES(272), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonItem->addActionSignal(new CMenuHandler_StringCommand("button_ammo_9mmclip"));
	pButtonItem->setParent(this);

	pButtonItem = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Mp5 Ammo"), iXPos + XRES(336), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonItem->addActionSignal(new CMenuHandler_StringCommand("button_ammo_9mmAR"));
	pButtonItem->setParent(this);



	pButtonItem = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("AR Grenades"), iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonItem->addActionSignal(new CMenuHandler_StringCommand("button_ammo_ARgrenades"));
	pButtonItem->setParent(this);

	pButtonItem = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Arrows"), iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonItem->addActionSignal(new CMenuHandler_StringCommand("button_ammo_crossbow"));
	pButtonItem->setParent(this);

	pButtonItem = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Uranium"), iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonItem->addActionSignal(new CMenuHandler_StringCommand("button_ammo_gaussclip"));
	pButtonItem->setParent(this);

	pButtonItem = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Rockets"), iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonItem->addActionSignal(new CMenuHandler_StringCommand("button_ammo_rpgclip"));
	pButtonItem->setParent(this);

	pButtonItem = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Buckshot"), iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonItem->addActionSignal(new CMenuHandler_StringCommand("button_ammo_buckshot"));
	pButtonItem->setParent(this);

	pButtonItem = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("357 clip"), iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonItem->addActionSignal(new CMenuHandler_StringCommand("button_ammo_357"));
	pButtonItem->setParent(this);



	pButtonItem = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("556 Ammo"), iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonItem->addActionSignal(new CMenuHandler_StringCommand("button_ammo_556"));
	pButtonItem->setParent(this);

	pButtonItem = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Spores"), iXPos + XRES(80), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonItem->addActionSignal(new CMenuHandler_StringCommand("button_ammo_spore"));
	pButtonItem->setParent(this);

	pButtonItem = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("762 Ammo"), iXPos + XRES(144), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonItem->addActionSignal(new CMenuHandler_StringCommand("button_ammo_762"));
	pButtonItem->setParent(this);
}

CMonstersMenu::CMonstersMenu(int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CGMMenuBase(iTrans, iRemoveMe, x, y, wide, tall)
{
	m_pButtonNPCMenu->setVisible(false);
	m_pButtonAimMode->setVisible(true);

	CommandButton* pButtonNPC;
	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Alien Controller"), iXPos + XRES(16), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_alien_controller"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Alien Grunt"), iXPos + XRES(80), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_alien_grunt"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Alien Slave"), iXPos + XRES(144), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_alien_slave"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Gargantua"), iXPos + XRES(208), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_gargantua"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Shock Trooper"), iXPos + XRES(272), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_shocktrooper"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Shock Roach"), iXPos + XRES(336), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + -YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_shockroach"));
	pButtonNPC->setParent(this);



	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Voltigore"), iXPos + XRES(16), iYPos + iYSize - YRES(198) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_alien_voltigore"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Baby Voltigore"), iXPos + XRES(80), iYPos + iYSize - YRES(198) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_alien_babyvoltigore"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Pítdrone"), iXPos + XRES(144), iYPos + iYSize - YRES(198) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_pitdrone"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Houndeye"), iXPos + XRES(208), iYPos + iYSize - YRES(198) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_houndeye"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Bullsquid"), iXPos + XRES(272), iYPos + iYSize - YRES(198) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_bullchicken"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Headcrab"), iXPos + XRES(336), iYPos + iYSize - YRES(198) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + -YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_headcrab"));
	pButtonNPC->setParent(this);



	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("BabyCrab"), iXPos + XRES(16), iYPos + iYSize - YRES(172) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_babycrab"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Big Momma"), iXPos + XRES(80), iYPos + iYSize - YRES(172) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_bigmomma"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Tentacle"), iXPos + XRES(144), iYPos + iYSize - YRES(172) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_tentacle"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Leech"), iXPos + XRES(208), iYPos + iYSize - YRES(172) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_leech"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Boid"), iXPos + XRES(272), iYPos + iYSize - YRES(172) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_flyer"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Ichthyosaur"), iXPos + XRES(336), iYPos + iYSize - YRES(172) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + -YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_ichthyosaur"));
	pButtonNPC->setParent(this);



	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Gonome"), iXPos + XRES(16), iYPos + iYSize - YRES(146) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_gonome"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Zombie"), iXPos + XRES(80), iYPos + iYSize - YRES(146) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_zombie"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Zombie Barney"), iXPos + XRES(144), iYPos + iYSize - YRES(146) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_zombie_barney"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Zombie Soldier"), iXPos + XRES(208), iYPos + iYSize - YRES(146) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_zombie_soldier"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Gman"), iXPos + XRES(272), iYPos + iYSize - YRES(146) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_gman"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Rat"), iXPos + XRES(336), iYPos + iYSize - YRES(146) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + -YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_rat"));
	pButtonNPC->setParent(this);



	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("CockRoach"), iXPos + XRES(16), iYPos + iYSize - YRES(120) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_cockroach"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Scientist"), iXPos + XRES(80), iYPos + iYSize - YRES(120) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_scientist"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Cleansuit Scientist"), iXPos + XRES(144), iYPos + iYSize - YRES(120) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_cleansuit_scientist"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Barney"), iXPos + XRES(208), iYPos + iYSize - YRES(120) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_barney"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Otis"), iXPos + XRES(272), iYPos + iYSize - YRES(120) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_otis"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Ally Hgrunt"), iXPos + XRES(336), iYPos + iYSize - YRES(120) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + -YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_human_grunt_ally"));
	pButtonNPC->setParent(this);



	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Medic Hgrunt"), iXPos + XRES(16), iYPos + iYSize - YRES(94) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_human_medic_ally"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Torch Hgrunt"), iXPos + XRES(80), iYPos + iYSize - YRES(94) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_human_torch_ally"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Human Grunt"), iXPos + XRES(144), iYPos + iYSize - YRES(94) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_human_grunt"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Male Assassin"), iXPos + XRES(208), iYPos + iYSize - YRES(94) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_male_assassin"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Female Assass"), iXPos + XRES(272), iYPos + iYSize - YRES(94) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_human_assassin"));
	pButtonNPC->setParent(this);

	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Apache"), iXPos + XRES(336), iYPos + iYSize - YRES(94) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + -YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_apache"));
	pButtonNPC->setParent(this);



	pButtonNPC = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Sentry"), iXPos + XRES(16), iYPos + iYSize - YRES(68) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	pButtonNPC->addActionSignal(new CMenuHandler_StringCommand("button_monster_sentry"));
	pButtonNPC->setParent(this);
}

CToolsMenu::CToolsMenu(int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CGMMenuBase(iTrans, iRemoveMe, x, y, wide, tall)
{
	m_pButtonToolsMenu->setVisible(false);
}