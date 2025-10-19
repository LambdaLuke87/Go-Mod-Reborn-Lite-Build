/**********************************************************************
 *                 Go-Mod: Reborn Lite Build                          *
 *                  LambdaLuke87 - 2025-2026                          *
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

	m_pButtonLinkMenu = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("#Gomod_MenuTab_Main"), iXPos + XRES(16), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	m_pButtonLinkMenu->addActionSignal(new CMenuHandler_StringCommand("toggleLinkMenu"));
	m_pButtonLinkMenu->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	m_pButtonLinkMenu->setParent(this);

	m_pButtonSweepMenu = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("#Gomod_MenuTab_Weapons"), iXPos + XRES(82), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	m_pButtonSweepMenu->addActionSignal(new CMenuHandler_StringCommand("toggleWeaponsMenu"));
	m_pButtonSweepMenu->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	m_pButtonSweepMenu->setParent(this);

	m_pButtonItemsMenu = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("#Gomod_MenuTab_Items"), iXPos + XRES(148), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	m_pButtonItemsMenu->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	m_pButtonItemsMenu->addActionSignal(new CMenuHandler_StringCommand("toggleItemsMenu"));
	m_pButtonItemsMenu->setParent(this);

	m_pButtonNPCMenu = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("#Gomod_MenuTab_Monsters"), iXPos + XRES(214), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	m_pButtonNPCMenu->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	m_pButtonNPCMenu->addActionSignal(new CMenuHandler_StringCommand("toggleMonstersMenu"));
	m_pButtonNPCMenu->setParent(this);

	m_pButtonToolsMenu = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("#Gomod_MenuTab_Tools"), iXPos + XRES(280), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	m_pButtonToolsMenu->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	m_pButtonToolsMenu->addActionSignal(new CMenuHandler_StringCommand("toggleToolsMenu"));
	m_pButtonToolsMenu->setParent(this);

	m_pButtonAimMode = new CommandButton(CHudTextMessage::BufferedLocaliseTextString(""), iXPos + XRES(370), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(135), BUTTON_SIZE_Y - YRES(8));
	m_pButtonAimMode->m_bNoMarginSpace = true;
	m_pButtonAimMode->setText(gHUD.m_TextMessage.BufferedLocaliseTextString("#Gomod_MenuButton_FrontSpawn"));
	m_pButtonAimMode->addActionSignal(new CMenuHandler_StringCommand("button_front_spawn"));
	m_pButtonAimMode->setParent(this);
	m_pButtonAimMode->setVisible(false);

	CommandButton* pButtonExit = new CommandButton(CHudTextMessage::BufferedLocaliseTextString(""), iXPos + XRES(410), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(149), BUTTON_SIZE_Y - YRES(16));
	pButtonExit->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	pButtonExit->setParent(this);
}

void CGMMenuBase::ButtonHelper(const char* text, const char* command, int x, int y, int wide, int tall)
{
	CommandButton* EasyButton;
	EasyButton = new CommandButton("", x, y, wide, tall);
	EasyButton->m_bNoMarginSpace = true;
	EasyButton->setText(gHUD.m_TextMessage.BufferedLocaliseTextString(text));
	EasyButton->addActionSignal(new CMenuHandler_StringCommand(command));
	EasyButton->setParent(this);
}

void CGMMenuBase::ButtonHelperWithID(CommandButton*& newbutton, const char* text, const char* command, int x, int y, int wide, int tall)
{
	newbutton = new CommandButton("", x, y, wide, tall);
	newbutton->setVisible(false);
	newbutton->m_bNoMarginSpace = true;
	newbutton->setText(gHUD.m_TextMessage.BufferedLocaliseTextString(text));
	newbutton->addActionSignal(new CMenuHandler_StringCommand(command));
	newbutton->setParent(this);
}

void CGMMenuBase::ButtonOptionHelper(const char* text, int option, int x, int y, int wide, int tall)
{
	CommandButton* ButtonCategories;

	ButtonCategories = new CommandButton("", x, y, wide, tall);
	ButtonCategories->m_bNoMarginSpace = true;
	ButtonCategories->setText(gHUD.m_TextMessage.BufferedLocaliseTextString(text));
	ButtonCategories->addInputSignal(new CHandler_MenuButtonOver(this, option));
	ButtonCategories->setParent(this);
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

	ButtonHelper("#Gomod_Weapon_Crowbar", "button_weapon_crowbar", iXPos + XRES(16), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_Physgun", "button_weapon_physgun", iXPos + XRES(80), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_Toolbow", "button_weapon_toolbow", iXPos + XRES(144), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_Glock", "button_weapon_9mmhandgun", iXPos + XRES(208), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_Python", "button_weapon_357", iXPos + XRES(272), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_Mp5", "button_weapon_9mmAR", iXPos + XRES(336), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelper("#Gomod_Weapon_Shotgun", "button_weapon_shotgun", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_Crossbow", "button_weapon_crossbow", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_RPG", "button_weapon_rpg", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_Gauss", "button_weapon_gauss", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_Egon", "button_weapon_egon", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_HiveHand", "button_weapon_hornetgun", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelper("#Gomod_Weapon_Grenade", "button_weapon_handgrenade", iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_Satchel", "button_weapon_satchel", iXPos + XRES(80), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_Tripmine", "button_weapon_tripmine", iXPos + XRES(144), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_Snark", "button_weapon_snark", iXPos + XRES(208), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_Knife", "button_weapon_knife", iXPos + XRES(272), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_PipeWrench", "button_weapon_pipewrench", iXPos + XRES(336), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelper("#Gomod_Weapon_Barnacle", "button_weapon_grapple", iXPos + XRES(16), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_DesertEgle", "button_weapon_eagle", iXPos + XRES(80), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_M249", "button_weapon_m249", iXPos + XRES(144), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_SniperRifle", "button_weapon_sniperrifle", iXPos + XRES(208), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_Displacer", "button_weapon_displacer", iXPos + XRES(272), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_ShockRifle", "button_weapon_shockrifle", iXPos + XRES(336), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelper("#Gomod_Weapon_SporeLauncher", "button_weapon_sporelauncher", iXPos + XRES(16), iYPos + iYSize - YRES(64) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Weapon_Penguin", "button_weapon_penguin", iXPos + XRES(80), iYPos + iYSize - YRES(64) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelper("#Gomod_GiveItem_Mode", "button_self_pickup", iXPos + XRES(16), iYPos + iYSize - YRES(10) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
}

CItemsMenu::CItemsMenu(int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CGMMenuBase(iTrans, iRemoveMe, x, y, wide, tall)
{
	m_pButtonItemsMenu->setVisible(false);
	m_pButtonAimMode->setVisible(true);

	ButtonHelper("#Gomod_Item_HealthKit", "button_item_healthkit", iXPos + XRES(16), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Item_Battery", "button_item_battery", iXPos + XRES(80), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Item_LongJump", "button_item_longjump", iXPos + XRES(144), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Item_HEV", "button_item_suit", iXPos + XRES(208), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Item_GlockAmmo", "button_ammo_9mmclip", iXPos + XRES(272), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Item_Mp5Ammo", "button_ammo_9mmAR", iXPos + XRES(336), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelper("#Gomod_Item_ARGrenades", "button_ammo_ARgrenades", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Item_Arrows", "button_ammo_crossbow", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Item_Uranium", "button_ammo_gaussclip", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Item_Rockets", "button_ammo_rpgclip", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Item_Buckshot", "button_ammo_buckshot", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Item_357Ammo", "button_ammo_357", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelper("#Gomod_Item_556Ammo", "button_ammo_556", iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Item_Spores", "button_ammo_spore", iXPos + XRES(80), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Item_762Ammo", "button_ammo_762", iXPos + XRES(144), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelper("#Gomod_Prop_Xen_Hair", "button_xen_hair", iXPos + XRES(16), iYPos + iYSize - YRES(94) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_Prop_Xen_Tree", "button_xen_tree", iXPos + XRES(80), iYPos + iYSize - YRES(94) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_Prop_Xen_PlantLight", "button_xen_plantlight", iXPos + XRES(144), iYPos + iYSize - YRES(94) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_Prop_Xen_SporeXS", "button_xen_spore_small", iXPos + XRES(208), iYPos + iYSize - YRES(94) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_Prop_Xen_SporeL", "button_xen_spore_medium", iXPos + XRES(272), iYPos + iYSize - YRES(94) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_Prop_Xen_SporeXL", "button_xen_spore_large", iXPos + XRES(336), iYPos + iYSize - YRES(94) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));

	ButtonHelper("#Gomod_Prop_Chiken", "button_prop_chiken", iXPos + XRES(16), iYPos + iYSize - YRES(68) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_Prop_Chumtoads", "button_prop_chumtoad", iXPos + XRES(80), iYPos + iYSize - YRES(68) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_Prop_C4", "button_prop_c4", iXPos + XRES(144), iYPos + iYSize - YRES(68) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));

	ButtonHelper("#Gomod_GiveItem_Mode", "button_self_pickup", iXPos + XRES(16), iYPos + iYSize - YRES(10) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
}

CMonstersMenu::CMonstersMenu(int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CGMMenuBase(iTrans, iRemoveMe, x, y, wide, tall)
{
	m_pButtonNPCMenu->setVisible(false);
	m_pButtonAimMode->setVisible(true);

	ButtonOptionHelper("#Gomod_NPC_TAB1", 0, iXPos + XRES(16), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonOptionHelper("#Gomod_NPC_TAB2", 1, iXPos + XRES(80), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonOptionHelper("#Gomod_NPC_TAB3", 2, iXPos + XRES(144), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonOptionHelper("#Gomod_NPC_TAB4", 3, iXPos + XRES(208), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonOptionHelper("#Gomod_NPC_TAB5", 4, iXPos + XRES(272), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));

	// Military Aliens
	ButtonHelperWithID(ButtonAlienSlave, "#Gomod_NPC_AlienSlave", "button_monster_alien_slave", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonAlienGrunt, "#Gomod_NPC_AlienGrunt", "button_monster_alien_grunt", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonAlienController, "#Gomod_NPC_AlienController", "button_monster_alien_controller", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonGargantua, "#Gomod_NPC_Gargantua", "button_monster_gargantua", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonShockTrooper, "#Gomod_NPC_ShockTrooper", "button_monster_shocktrooper", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonVoltigore, "#Gomod_NPC_Voltigore", "button_monster_alien_voltigore", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	
	ButtonHelperWithID(ButtonNihilant, "#Gomod_NPC_Nihilanth", "button_monster_nihilanth", iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	
	// Wildlife Aliens and Zombies
	ButtonHelperWithID(ButtonHeadcrab, "#Gomod_NPC_Headcrab", "button_monster_headcrab", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonBabycrab, "#Gomod_NPC_BabyCrab", "button_monster_babycrab", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonShockRoach, "#Gomod_NPC_ShockRoach", "button_monster_shockroach", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonBabyVoltigore, "#Gomod_NPC_BabyVoltigore", "button_monster_alien_babyvoltigore", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonHoundeye, "#Gomod_NPC_Houndeye", "button_monster_houndeye", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonBullsquid, "#Gomod_NPC_Bullsquid", "button_monster_bullchicken", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	
	ButtonHelperWithID(ButtonPitdrone, "#Gomod_NPC_Pitdrone", "button_monster_pitdrone", iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonBigMomma, "#Gomod_NPC_BigMomma", "button_monster_bigmomma", iXPos + XRES(80), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonZombie, "#Gomod_NPC_Zombie", "button_monster_zombie", iXPos + XRES(144), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonZombieBarney, "#Gomod_NPC_ZombieBarney", "button_monster_zombie_barney", iXPos + XRES(208), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonZombieSoldier, "#Gomod_NPC_ZombieSoldier", "button_monster_zombie_soldier", iXPos + XRES(272), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonGonome, "#Gomod_NPC_Gonome", "button_monster_gonome", iXPos + XRES(336), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	
	ButtonHelperWithID(ButtonIchthyosaur, "#Gomod_NPC_Ichthyosaur", "button_monster_ichthyosaur", iXPos + XRES(16), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonBarnacle, "#Gomod_NPC_Barnacle", "button_monster_barnacle", iXPos + XRES(80), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	// Friendly Humans
	ButtonHelperWithID(ButtonScientist, "#Gomod_NPC_Scientist", "button_monster_scientist", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonClScientist, "#Gomod_NPC_ClScientist", "button_monster_cleansuit_scientist", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonBarney, "#Gomod_NPC_Barney", "button_monster_barney", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonOtis, "#Gomod_NPC_Otis", "button_monster_otis", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonAllyHgrunt, "#Gomod_NPC_AllyHgrunt", "button_monster_human_grunt_ally", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonMedicHgrunt, "#Gomod_NPC_MedicHgrunt", "button_monster_human_medic_ally", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	
	ButtonHelperWithID(ButtonTorchHgrunt, "#Gomod_NPC_TorchHgrunt", "button_monster_human_torch_ally", iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	
	// Enemy Humans and Machines
	ButtonHelperWithID(ButtonHGrunt, "#Gomod_NPC_HGrunt", "button_monster_human_grunt", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonMassassin, "#Gomod_NPC_MAssassin", "button_monster_male_assassin", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonFassassin, "#Gomod_NPC_FAssassin", "button_monster_human_assassin", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonApache, "#Gomod_NPC_Apache", "button_monster_apache", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonSentry, "#Gomod_NPC_Sentry", "button_monster_sentry", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	// Misc Monsters
	ButtonHelperWithID(ButtonBoid, "#Gomod_NPC_Boid", "button_monster_flyer", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonLeech, "#Gomod_NPC_Leech", "button_monster_leech", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonCockRoach, "#Gomod_NPC_CockRoach", "button_monster_cockroach", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonRat, "#Gomod_NPC_Rat", "button_monster_rat", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonGman, "#Gomod_NPC_Gman", "button_monster_gman", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonTentacle, "#Gomod_NPC_Tentacle", "button_monster_tentacle", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelper("#Gomod_MenuButton_NoAI", "button_ai_set", iXPos + XRES(144), iYPos + iYSize - YRES(30) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_MenuButton_Ally", "button_allied_set", iXPos + XRES(208), iYPos + iYSize - YRES(30) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_MenuButton_undo", "button_npc_undo", iXPos + XRES(144), iYPos + iYSize - YRES(4) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_MenuButton_remove", "button_npc_remove_all", iXPos + XRES(208), iYPos + iYSize - YRES(4) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
}

void CMonstersMenu::SetActiveInfo(int iShowText)
{
	// Hide All
	CommandButton* allButtons[] = {
		ButtonAlienSlave, ButtonBullsquid, ButtonHeadcrab, ButtonBabycrab, ButtonHoundeye,
		ButtonAlienGrunt, ButtonAlienController, ButtonZombie, ButtonZombieBarney, ButtonZombieSoldier,
		ButtonGargantua, ButtonBigMomma, ButtonIchthyosaur, ButtonScientist, ButtonClScientist,
		ButtonHGrunt, ButtonBarney, ButtonOtis, ButtonMassassin, ButtonFassassin,
		ButtonApache, ButtonSentry, ButtonRat, ButtonCockRoach, ButtonBarnacle,
		ButtonLeech, ButtonBoid, ButtonGman, ButtonGonome, ButtonShockTrooper,
		ButtonShockRoach, ButtonPitdrone, ButtonVoltigore, ButtonBabyVoltigore, ButtonAllyHgrunt,
		ButtonMedicHgrunt, ButtonTorchHgrunt, ButtonNihilant, ButtonTentacle};

	for (int i = 0; i < ARRAYSIZE(allButtons); i++)
	{
		if (allButtons[i])
			allButtons[i]->setVisible(false);
	}

	if (iShowText == 0) // Military Aliens
	{
		ButtonAlienSlave->setVisible(true);
		ButtonAlienGrunt->setVisible(true);
		ButtonAlienController->setVisible(true);
		ButtonGargantua->setVisible(true);
		ButtonShockTrooper->setVisible(true);
		ButtonVoltigore->setVisible(true);
		ButtonNihilant->setVisible(true);
	}
	else if (iShowText == 1) // Wildlife Aliens and Zombies
	{
		ButtonHeadcrab->setVisible(true);
		ButtonBabycrab->setVisible(true);
		ButtonShockRoach->setVisible(true);
		ButtonBabyVoltigore->setVisible(true);
		ButtonHoundeye->setVisible(true);
		ButtonBullsquid->setVisible(true);
		ButtonPitdrone->setVisible(true);
		ButtonBigMomma->setVisible(true);
		ButtonZombie->setVisible(true);
		ButtonZombieBarney->setVisible(true);
		ButtonZombieSoldier->setVisible(true);
		ButtonGonome->setVisible(true);
		ButtonIchthyosaur->setVisible(true);
		ButtonBarnacle->setVisible(true);
	}
	else if (iShowText == 2) // Friendly Humans
	{
		ButtonScientist->setVisible(true);
		ButtonClScientist->setVisible(true);
		ButtonBarney->setVisible(true);
		ButtonOtis->setVisible(true);
		ButtonAllyHgrunt->setVisible(true);
		ButtonMedicHgrunt->setVisible(true);
		ButtonTorchHgrunt->setVisible(true);
	}
	else if (iShowText == 3) // Enemy Humans and Machines
	{
		ButtonHGrunt->setVisible(true);
		ButtonMassassin->setVisible(true);
		ButtonFassassin->setVisible(true);
		ButtonApache->setVisible(true);
		ButtonSentry->setVisible(true);
	}
	else if (iShowText == 4) // Misc Monsters
	{
		ButtonBoid->setVisible(true);
		ButtonLeech->setVisible(true);
		ButtonCockRoach->setVisible(true);
		ButtonRat->setVisible(true);
		ButtonGman->setVisible(true);
		ButtonTentacle->setVisible(true);
	}
}

CToolsMenu::CToolsMenu(int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CGMMenuBase(iTrans, iRemoveMe, x, y, wide, tall)
{
	m_pButtonToolsMenu->setVisible(false);

	ButtonHelper("#Gomod_Tool_Duplicator", "button_tool_duplicator", iXPos + XRES(16), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_Remover", "button_tool_remover", iXPos + XRES(80), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_Gib", "button_tool_gib", iXPos + XRES(144), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_Poser", "button_tool_poser", iXPos + XRES(208), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_Camera", "button_tool_camera", iXPos + XRES(272), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_HPModify", "button_tool_health_set", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelper("#Gomod_Tool_NoCollide", "button_tool_no_collide", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_TakeDamage", "button_tool_take_damage", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_BloodColor", "button_tool_blood_color", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_FrameEditor", "button_tool_frame_set", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_Teleporter", "button_tool_teleporter", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelper("#Gomod_Tool_Glowsticks", "button_tool_glowsticks", iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_NoTools", "button_tool_none", iXPos + XRES(80), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	CommandButton* pButtonRender;
	pButtonRender = new CommandButton(CHudTextMessage::BufferedLocaliseTextString(""), iXPos + XRES(336), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonRender->m_bNoMarginSpace = true;
	pButtonRender->setText(gHUD.m_TextMessage.BufferedLocaliseTextString("#Gomod_Tool_Render"));
	pButtonRender->addActionSignal(new CMenuHandler_StringCommand("button_tool_render"));
	pButtonRender->addActionSignal(new CMenuHandler_StringCommand("toggleRenderMenu"));
	pButtonRender->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	pButtonRender->setParent(this);
}

CRenderToolMenu::CRenderToolMenu(int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CGMMenuBase(iTrans, iRemoveMe, x, y, wide, tall)
{
	ButtonHelper("#Gomod_RenderMode_Normal", "button_rendermode_normal", iXPos + XRES(16), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderMode_Color", "button_rendermode_color", iXPos + XRES(80), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderMode_Texture", "button_rendermode_texture", iXPos + XRES(144), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderMode_Glow", "button_rendermode_glow", iXPos + XRES(208), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderMode_Solid", "button_rendermode_solid", iXPos + XRES(272), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderMode_Additive", "button_rendermode_additive", iXPos + XRES(336), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));

	ButtonHelper("#Gomod_RenderFX_None", "button_renderfx_normal", iXPos + XRES(16), iYPos + iYSize - YRES(190) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_SPulse", "button_renderfx_slow_pulse", iXPos + XRES(80), iYPos + iYSize - YRES(190) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_FPulse", "button_renderfx_fast_pulse", iXPos + XRES(144), iYPos + iYSize - YRES(190) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_SWPulse", "button_renderfx_slow_wide_pulse", iXPos + XRES(208), iYPos + iYSize - YRES(190) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_FWPulse", "button_renderfx_fast_wide_pulse", iXPos + XRES(272), iYPos + iYSize - YRES(190) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_SFadeAway", "button_renderfx_slow_fade_away", iXPos + XRES(336), iYPos + iYSize - YRES(190) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));

	ButtonHelper("#Gomod_RenderFX_FFadeAway", "button_renderfx_fast_fade_away", iXPos + XRES(16), iYPos + iYSize - YRES(164) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_SSolid", "button_renderfx_slow_become_solid", iXPos + XRES(80), iYPos + iYSize - YRES(164) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_FSolid", "button_renderfx_fast_become_solid", iXPos + XRES(144), iYPos + iYSize - YRES(164) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_SStrobe", "button_renderfx_slow_strobe", iXPos + XRES(208), iYPos + iYSize - YRES(164) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_FStrobe", "button_renderfx_fast_strobe", iXPos + XRES(272), iYPos + iYSize - YRES(164) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_FRStrobe", "button_renderfx_faster_strobe", iXPos + XRES(336), iYPos + iYSize - YRES(164) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));

	ButtonHelper("#Gomod_RenderFX_SFlicker", "button_renderfx_slow_flicker", iXPos + XRES(16), iYPos + iYSize - YRES(138) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_FFlicker", "button_renderfx_fast_flicker", iXPos + XRES(80), iYPos + iYSize - YRES(138) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_ConstantGlow", "button_renderfx_constant_glow", iXPos + XRES(144), iYPos + iYSize - YRES(138) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_Distort", "button_renderfx_distort", iXPos + XRES(208), iYPos + iYSize - YRES(138) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_Hologram", "button_renderfx_hologram", iXPos + XRES(272), iYPos + iYSize - YRES(138) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_Explode", "button_renderfx_explode", iXPos + XRES(336), iYPos + iYSize - YRES(138) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));

	ButtonHelper("#Gomod_RenderFX_GlowShell", "button_renderfx_glow_shell", iXPos + XRES(16), iYPos + iYSize - YRES(112) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
}