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
	//m_pButtonLinkMenu->setVisible(false);

	CSchemeManager* pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle("Title Font");

	m_pButtonLinkMenu = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("#Gomod_MenuTab_Main"), iXPos + XRES(16), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	m_pButtonLinkMenu->addInputSignal(new CHandler_MenuButtonOver(this, 0));
	m_pButtonLinkMenu->setParent(this);
	m_pButtonLinkMenu->setVisible(false);

	m_pButtonSweepMenu = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("#Gomod_MenuTab_Weapons"), iXPos + XRES(82), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	m_pButtonSweepMenu->addInputSignal(new CHandler_MenuButtonOver(this, 1));
	m_pButtonSweepMenu->setParent(this);
	m_pButtonSweepMenu->setVisible(true);

	m_pButtonItemsMenu = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("#Gomod_MenuTab_Items"), iXPos + XRES(148), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	m_pButtonItemsMenu->addInputSignal(new CHandler_MenuButtonOver(this, 2));
	m_pButtonItemsMenu->setParent(this);
	m_pButtonItemsMenu->setVisible(true);

	m_pButtonNPCMenu = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("#Gomod_MenuTab_Monsters"), iXPos + XRES(214), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	m_pButtonNPCMenu->addInputSignal(new CHandler_MenuButtonOver(this, 3));
	m_pButtonNPCMenu->setParent(this);
	m_pButtonNPCMenu->setVisible(true);

	m_pButtonToolsMenu = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("#Gomod_MenuTab_Tools"), iXPos + XRES(280), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	m_pButtonToolsMenu->addInputSignal(new CHandler_MenuButtonOver(this, 4));
	m_pButtonToolsMenu->setParent(this);
	m_pButtonToolsMenu->setVisible(true);

	// Aim Button
	m_pButtonAimMode = new CommandButton(CHudTextMessage::BufferedLocaliseTextString(""), iXPos + XRES(370), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(135), BUTTON_SIZE_Y - YRES(8));
	m_pButtonAimMode->m_bNoMarginSpace = true;
	m_pButtonAimMode->setText(gHUD.m_TextMessage.BufferedLocaliseTextString("#Gomod_MenuButton_FrontSpawn"));
	m_pButtonAimMode->addActionSignal(new CMenuHandler_StringCommand("button_front_spawn"));
	m_pButtonAimMode->setParent(this);
	m_pButtonAimMode->setVisible(false);

	// SubHeaders Weapons
	m_pButtonHLSweeps = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("#Gomod_SWEEP_TAB1"), iXPos + XRES(16), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	m_pButtonHLSweeps->addInputSignal(new CHandler_MenuButtonOver(this, 5));
	m_pButtonHLSweeps->setParent(this);
	m_pButtonHLSweeps->setVisible(false);

	m_pButtonOp4Sweeps = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("#Gomod_SWEEP_TAB2"), iXPos + XRES(80), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	m_pButtonOp4Sweeps->addInputSignal(new CHandler_MenuButtonOver(this, 6));
	m_pButtonOp4Sweeps->setParent(this);
	m_pButtonOp4Sweeps->setVisible(false);

	// SubHeaders NPCS
	m_pButtonMilAliens = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("#Gomod_NPC_TAB1"), iXPos + XRES(16), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	m_pButtonMilAliens->addInputSignal(new CHandler_MenuButtonOver(this, 7));
	m_pButtonMilAliens->setParent(this);
	m_pButtonMilAliens->setVisible(false);

	m_pButtonWildAliens = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("#Gomod_NPC_TAB2"), iXPos + XRES(80), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	m_pButtonWildAliens->addInputSignal(new CHandler_MenuButtonOver(this, 8));
	m_pButtonWildAliens->setParent(this);
	m_pButtonWildAliens->setVisible(false);

	m_pButtonFriendHumans = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("#Gomod_NPC_TAB3"), iXPos + XRES(144), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	m_pButtonFriendHumans->addInputSignal(new CHandler_MenuButtonOver(this, 9));
	m_pButtonFriendHumans->setParent(this);
	m_pButtonFriendHumans->setVisible(false);

	m_pButtonMilUnits = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("#Gomod_NPC_TAB4"), iXPos + XRES(208), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	m_pButtonMilUnits->addInputSignal(new CHandler_MenuButtonOver(this, 10));
	m_pButtonMilUnits->setParent(this);
	m_pButtonMilUnits->setVisible(false);

	m_pButtonMiscNPCS = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("#Gomod_NPC_TA5"), iXPos + XRES(272), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	m_pButtonMiscNPCS->addInputSignal(new CHandler_MenuButtonOver(this, 11));
	m_pButtonMiscNPCS->setParent(this);
	m_pButtonMiscNPCS->setVisible(false);

	ButtonHelperWithID(m_pButtonCrowbar, "#Gomod_Weapon_Crowbar", "summon weapon_crowbar", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonPhysgun, "#Gomod_Weapon_Physgun", "summon weapon_physgun", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonToolbow, "#Gomod_Weapon_Toolbow", "summon weapon_toolbow", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonGlock, "#Gomod_Weapon_Glock", "summon weapon_9mmhandgun", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonPython, "#Gomod_Weapon_Python", "summon weapon_357", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonMP5, "#Gomod_Weapon_Mp5", "summon weapon_9mmAR", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelperWithID(m_pButtonShotgun, "#Gomod_Weapon_Shotgun", "summon weapon_shotgun", iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonCrossbow, "#Gomod_Weapon_Crossbow", "summon weapon_crossbow", iXPos + XRES(80), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonRPG, "#Gomod_Weapon_RPG", "summon weapon_rpg", iXPos + XRES(144), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonGauss, "#Gomod_Weapon_Gauss", "summon weapon_gauss", iXPos + XRES(208), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonEgon, "#Gomod_Weapon_Egon", "summon weapon_egon", iXPos + XRES(272), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonHiveHand, "#Gomod_Weapon_HiveHand", "summon weapon_hornetgun", iXPos + XRES(336), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelperWithID(m_pButtonGrenade, "#Gomod_Weapon_Grenade", "summon weapon_handgrenade", iXPos + XRES(16), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonSatchel, "#Gomod_Weapon_Satchel", "summon weapon_satchel", iXPos + XRES(80), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonTripmine, "#Gomod_Weapon_Tripmine", "summon weapon_tripmine", iXPos + XRES(144), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonSnark, "#Gomod_Weapon_Snark", "summon weapon_snark", iXPos + XRES(208), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	
	ButtonHelperWithID(m_pButtonKnife, "#Gomod_Weapon_Knife", "summon weapon_knife", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonPipeWrench, "#Gomod_Weapon_PipeWrench", "summon weapon_pipewrench", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonGrapple, "#Gomod_Weapon_Barnacle", "summon weapon_grapple", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonDEagle, "#Gomod_Weapon_DesertEgle", "summon weapon_eagle", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonM249, "#Gomod_Weapon_M249", "summon weapon_m249", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonSniperRifle, "#Gomod_Weapon_SniperRifle", "summon weapon_sniperrifle", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	
	ButtonHelperWithID(m_pButtonDisplacer, "#Gomod_Weapon_Displacer", "summon weapon_displacer", iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonShockRifle, "#Gomod_Weapon_ShockRifle", "summon weapon_shockrifle", iXPos + XRES(80), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonSporeLauncher, "#Gomod_Weapon_SporeLauncher", "summon weapon_sporelauncher", iXPos + XRES(144), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonPenguin, "#Gomod_Weapon_Penguin", "summon weapon_penguin", iXPos + XRES(208), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	// AMMO AND ITEMS
	ButtonHelperWithID(m_pButtonHealthKit, "#Gomod_Item_HealthKit", "summon item_healthkit", iXPos + XRES(16), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonBattery, "#Gomod_Item_Battery", "summon item_battery", iXPos + XRES(80), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonLongJump, "#Gomod_Item_LongJump", "summon item_longjump", iXPos + XRES(144), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonHEV, "#Gomod_Item_HEV", "summon item_suit", iXPos + XRES(208), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonGlockAmmo, "#Gomod_Item_GlockAmmo", "summon ammo_9mmclip", iXPos + XRES(272), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonMP5Ammo, "#Gomod_Item_Mp5Ammo", "summon ammo_9mmAR", iXPos + XRES(336), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelperWithID(m_pButtonARGrenades, "#Gomod_Item_ARGrenades", "summon ammo_ARgrenades", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonArrows, "#Gomod_Item_Arrows", "summon ammo_crossbow", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonUranium, "#Gomod_Item_Uranium", "summon ammo_gaussclip", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonRockets, "#Gomod_Item_Rockets", "summon ammo_rpgclip", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonBuckshot, "#Gomod_Item_Buckshot", "summon ammo_buckshot", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButton357Ammo, "#Gomod_Item_357Ammo", "summon ammo_357", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelperWithID(m_pButton556Ammo, "#Gomod_Item_556Ammo", "summon ammo_556", iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButtonSpores, "#Gomod_Item_Spores", "summon ammo_spore", iXPos + XRES(80), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(m_pButton762Ammo, "#Gomod_Item_762Ammo", "summon ammo_762", iXPos + XRES(144), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	// Military Aliens
	ButtonHelperWithID(ButtonAlienSlave, "#Gomod_NPC_AlienSlave", "summon monster_alien_slave", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonAlienGrunt, "#Gomod_NPC_AlienGrunt", "summon monster_alien_grunt", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonAlienController, "#Gomod_NPC_AlienController", "summon monster_alien_controller", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonGargantua, "#Gomod_NPC_Gargantua", "summon monster_gargantua", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonShockTrooper, "#Gomod_NPC_ShockTrooper", "summon monster_shocktrooper", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonVoltigore, "#Gomod_NPC_Voltigore", "summon monster_alien_voltigore", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelperWithID(ButtonNihilant, "#Gomod_NPC_Nihilanth", "summon monster_nihilanth", iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	// Wildlife Aliens and Zombies
	ButtonHelperWithID(ButtonHeadcrab, "#Gomod_NPC_Headcrab", "summon monster_headcrab", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonBabycrab, "#Gomod_NPC_BabyCrab", "summon monster_babycrab", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonShockRoach, "#Gomod_NPC_ShockRoach", "summon monster_shockroach", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonBabyVoltigore, "#Gomod_NPC_BabyVoltigore", "summon monster_alien_babyvoltigore", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonHoundeye, "#Gomod_NPC_Houndeye", "summon monster_houndeye", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonBullsquid, "#Gomod_NPC_Bullsquid", "summon monster_bullchicken", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelperWithID(ButtonPitdrone, "#Gomod_NPC_Pitdrone", "summon monster_pitdrone", iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonBigMomma, "#Gomod_NPC_BigMomma", "summon monster_bigmomma", iXPos + XRES(80), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonZombie, "#Gomod_NPC_Zombie", "summon monster_zombie", iXPos + XRES(144), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonZombieBarney, "#Gomod_NPC_ZombieBarney", "summon monster_zombie_barney", iXPos + XRES(208), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonZombieSoldier, "#Gomod_NPC_ZombieSoldier", "summon monster_zombie_soldier", iXPos + XRES(272), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonGonome, "#Gomod_NPC_Gonome", "summon monster_gonome", iXPos + XRES(336), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelperWithID(ButtonIchthyosaur, "#Gomod_NPC_Ichthyosaur", "summon monster_ichthyosaur", iXPos + XRES(16), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonBarnacle, "#Gomod_NPC_Barnacle", "summon monster_barnacle", iXPos + XRES(80), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	// Friendly Humans
	ButtonHelperWithID(ButtonScientist, "#Gomod_NPC_Scientist", "summon monster_scientist", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonClScientist, "#Gomod_NPC_ClScientist", "summon monster_cleansuit_scientist", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonBarney, "#Gomod_NPC_Barney", "summon monster_barney", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonOtis, "#Gomod_NPC_Otis", "summon monster_otis", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonAllyHgrunt, "#Gomod_NPC_AllyHgrunt", "summon monster_human_grunt_ally", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonMedicHgrunt, "#Gomod_NPC_MedicHgrunt", "summon monster_human_medic_ally", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelperWithID(ButtonTorchHgrunt, "#Gomod_NPC_TorchHgrunt", "summon monster_human_torch_ally", iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	// Enemy Humans and Machines
	ButtonHelperWithID(ButtonHGrunt, "#Gomod_NPC_HGrunt", "summon monster_human_grunt", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonMassassin, "#Gomod_NPC_MAssassin", "summon monster_male_assassin", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonFassassin, "#Gomod_NPC_FAssassin", "summon monster_human_assassin", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonApache, "#Gomod_NPC_Apache", "summon monster_apache", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonSentry, "#Gomod_NPC_Sentry", "summon monster_sentry", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	// Misc Monsters
	ButtonHelperWithID(ButtonBoid, "#Gomod_NPC_Boid", "summon monster_flyer", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonLeech, "#Gomod_NPC_Leech", "summon monster_leech", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonCockRoach, "#Gomod_NPC_CockRoach", "summon monster_cockroach", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonRat, "#Gomod_NPC_Rat", "summon monster_rat", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonGman, "#Gomod_NPC_Gman", "summon monster_gman", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonTentacle, "#Gomod_NPC_Tentacle", "summon monster_tentacle", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	//ButtonHelperWithID("#Gomod_Prop_Xen_Hair", "summon xen_hair", iXPos + XRES(16), iYPos + iYSize - YRES(94) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	//ButtonHelperWithID("#Gomod_Prop_Xen_Tree", "summon xen_tree", iXPos + XRES(80), iYPos + iYSize - YRES(94) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	//ButtonHelperWithID("#Gomod_Prop_Xen_PlantLight", "summon xen_plantlight", iXPos + XRES(144), iYPos + iYSize - YRES(94) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	//ButtonHelperWithID("#Gomod_Prop_Xen_SporeXS", "summon xen_spore_small", iXPos + XRES(208), iYPos + iYSize - YRES(94) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	//ButtonHelperWithID("#Gomod_Prop_Xen_SporeL", "summon xen_spore_medium", iXPos + XRES(272), iYPos + iYSize - YRES(94) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	//ButtonHelperWithID("#Gomod_Prop_Xen_SporeXL", "summon xen_spore_large", iXPos + XRES(336), iYPos + iYSize - YRES(94) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));

	//ButtonHelperWithID("#Gomod_Prop_Chiken", "summon prop_chiken", iXPos + XRES(16), iYPos + iYSize - YRES(68) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	//ButtonHelperWithID("#Gomod_Prop_Chumtoads", "summon prop_chumtoad", iXPos + XRES(80), iYPos + iYSize - YRES(68) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	//ButtonHelperWithID("#Gomod_Prop_C4", "summon prop_c4", iXPos + XRES(144), iYPos + iYSize - YRES(68) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));

	// color schemes
	int r, g, b, a;

	// Create the Scroll panel
	pScrollPanel = new CTFScrollPanel(iXPos + XRES(16), iYPos + LINKMENU_TITLE_Y * 2 + YRES(16), iXSize - XRES(32), iYSize - (YRES(48) + BUTTON_SIZE_Y * 2));
	pScrollPanel->setParent(this);

	// force the scrollbars on so clientClip will take them in account after the validate
	pScrollPanel->setScrollBarAutoVisible(false, false);
	pScrollPanel->setScrollBarVisible(true, true);
	pScrollPanel->validate();

	// Create the text panel
	pText = new TextPanel("", 0, 0, 64, 64);
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

void CLinkMenu::SetActiveInfo(int iShowText)
{
	CommandButton* allButtons[] = {
		// SUBHEADERS WEAPONS
		m_pButtonHLSweeps, m_pButtonOp4Sweeps,
		// SUBHEADERS NPCS
		m_pButtonMilAliens, m_pButtonWildAliens, m_pButtonFriendHumans, m_pButtonMilUnits,
		m_pButtonMiscNPCS,
		// WEAPONS
		m_pButtonCrowbar, m_pButtonPhysgun, m_pButtonToolbow, m_pButtonGlock,
		m_pButtonPython, m_pButtonMP5, m_pButtonShotgun, m_pButtonCrossbow, m_pButtonRPG,
		m_pButtonGauss, m_pButtonEgon, m_pButtonHiveHand, m_pButtonGrenade, m_pButtonSatchel,
		m_pButtonTripmine, m_pButtonSnark, m_pButtonKnife, m_pButtonPipeWrench, m_pButtonGrapple,
		m_pButtonDEagle, m_pButtonM249, m_pButtonSniperRifle, m_pButtonDisplacer, m_pButtonShockRifle,
		m_pButtonSporeLauncher, m_pButtonPenguin,
	    // ITEMS AND AMMO
		m_pButtonHealthKit, m_pButtonBattery, m_pButtonLongJump, m_pButtonHEV,
		m_pButtonGlockAmmo, m_pButtonMP5Ammo, m_pButtonARGrenades, m_pButtonArrows, m_pButtonUranium,
		m_pButtonRockets, m_pButtonBuckshot, m_pButton357Ammo, m_pButton556Ammo, m_pButtonSpores,
		m_pButton762Ammo,
	    // NPCS
		ButtonAlienSlave, ButtonBullsquid, ButtonHeadcrab, ButtonBabycrab, ButtonHoundeye,
		ButtonAlienGrunt, ButtonAlienController, ButtonZombie, ButtonZombieBarney, ButtonZombieSoldier,
		ButtonGargantua, ButtonBigMomma, ButtonIchthyosaur, ButtonScientist, ButtonClScientist,
		ButtonHGrunt, ButtonBarney, ButtonOtis, ButtonMassassin, ButtonFassassin,
		ButtonApache, ButtonSentry, ButtonRat, ButtonCockRoach, ButtonBarnacle,
		ButtonLeech, ButtonBoid, ButtonGman, ButtonGonome, ButtonShockTrooper,
		ButtonShockRoach, ButtonPitdrone, ButtonVoltigore, ButtonBabyVoltigore, ButtonAllyHgrunt,
		ButtonMedicHgrunt, ButtonTorchHgrunt, ButtonNihilant, ButtonTentacle};

	CommandButton* headerbuttons[] = {
		m_pButtonLinkMenu, m_pButtonSweepMenu, m_pButtonItemsMenu, m_pButtonNPCMenu, m_pButtonToolsMenu,
		m_pButtonAimMode};

	// Hide All
	for (int i = 0; i < ARRAYSIZE(allButtons); i++)
	{
		if (allButtons[i])
			allButtons[i]->setVisible(false);
	}

	// Show Headers
	for (int i = 0; i < ARRAYSIZE(headerbuttons); i++)
	{
		if (headerbuttons[i])
			headerbuttons[i]->setVisible(true);
	}

	pScrollPanel->setVisible(false);
	pText->setVisible(false);

	if (iShowText == 0) // Link Menu
	{
		pScrollPanel->setVisible(true);
		pText->setVisible(true);
		m_pButtonLinkMenu->setVisible(false);
		//m_pButtonAimMode->setVisible(false);
	}
	else if (iShowText == 1) // Weapons Menu
	{
		m_pButtonSweepMenu->setVisible(false);
		m_pButtonHLSweeps->setVisible(true);
		m_pButtonOp4Sweeps->setVisible(true);
	}
	else if (iShowText == 2) // Items and Ammo Menu
	{
		m_pButtonItemsMenu->setVisible(false);
		m_pButtonHealthKit->setVisible(true);
		m_pButtonBattery->setVisible(true);
		m_pButtonLongJump->setVisible(true);
		m_pButtonHEV->setVisible(true);
		m_pButtonGlockAmmo->setVisible(true);
		m_pButtonMP5Ammo->setVisible(true);
		m_pButtonARGrenades->setVisible(true);
		m_pButtonArrows->setVisible(true);
		m_pButtonUranium->setVisible(true);
		m_pButtonRockets->setVisible(true);
		m_pButtonBuckshot->setVisible(true);
		m_pButton357Ammo->setVisible(true);
		m_pButton556Ammo->setVisible(true);
		m_pButtonSpores->setVisible(true);
		m_pButton762Ammo->setVisible(true);
	}
	else if (iShowText == 3) // NPC Menu
	{
		m_pButtonNPCMenu->setVisible(false);
		m_pButtonMilAliens->setVisible(true);
		m_pButtonWildAliens->setVisible(true);
		m_pButtonFriendHumans->setVisible(true);
		m_pButtonMilUnits->setVisible(true);
		m_pButtonMiscNPCS->setVisible(true);
	}
	else if (iShowText == 4) // Weapons Menu
	{
		m_pButtonToolsMenu->setVisible(false);
	}

	if (iShowText == 5)
	{
		m_pButtonSweepMenu->setVisible(false);
		m_pButtonOp4Sweeps->setVisible(true);
		m_pButtonCrowbar->setVisible(true);
		m_pButtonPhysgun->setVisible(true);
		m_pButtonToolbow->setVisible(true);
		m_pButtonGlock->setVisible(true);
		m_pButtonPython->setVisible(true);
		m_pButtonMP5->setVisible(true);
		m_pButtonShotgun->setVisible(true);
		m_pButtonCrossbow->setVisible(true);
		m_pButtonRPG->setVisible(true);
		m_pButtonGauss->setVisible(true);
		m_pButtonEgon->setVisible(true);
		m_pButtonHiveHand->setVisible(true);
		m_pButtonGrenade->setVisible(true);
		m_pButtonSatchel->setVisible(true);
		m_pButtonTripmine->setVisible(true);
		m_pButtonSnark->setVisible(true);
	}
	else if (iShowText == 6)
	{
		m_pButtonSweepMenu->setVisible(false);
		m_pButtonHLSweeps->setVisible(true);
		m_pButtonKnife->setVisible(true);
		m_pButtonPipeWrench->setVisible(true);
		m_pButtonGrapple->setVisible(true);
		m_pButtonDEagle->setVisible(true);
		m_pButtonM249->setVisible(true);
		m_pButtonSniperRifle->setVisible(true);
		m_pButtonDisplacer->setVisible(true);
		m_pButtonShockRifle->setVisible(true);
		m_pButtonSporeLauncher->setVisible(true);
		m_pButtonPenguin->setVisible(true);
	}
	else if (iShowText == 7) // Military Aliens
	{
		m_pButtonNPCMenu->setVisible(false);
		m_pButtonWildAliens->setVisible(true);
		m_pButtonFriendHumans->setVisible(true);
		m_pButtonMilUnits->setVisible(true);
		m_pButtonMiscNPCS->setVisible(true);

		ButtonAlienSlave->setVisible(true);
		ButtonAlienGrunt->setVisible(true);
		ButtonAlienController->setVisible(true);
		ButtonGargantua->setVisible(true);
		ButtonShockTrooper->setVisible(true);
		ButtonVoltigore->setVisible(true);
		ButtonNihilant->setVisible(true);
	}
	else if (iShowText == 8) // Wildlife Aliens and Zombies
	{
		m_pButtonNPCMenu->setVisible(false);
		m_pButtonMilAliens->setVisible(true);
		m_pButtonFriendHumans->setVisible(true);
		m_pButtonMilUnits->setVisible(true);
		m_pButtonMiscNPCS->setVisible(true);

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
	else if (iShowText == 9) // Friendly Humans
	{
		m_pButtonNPCMenu->setVisible(false);
		m_pButtonMilAliens->setVisible(true);
		m_pButtonWildAliens->setVisible(true);
		m_pButtonMilUnits->setVisible(true);
		m_pButtonMiscNPCS->setVisible(true);

		ButtonScientist->setVisible(true);
		ButtonClScientist->setVisible(true);
		ButtonBarney->setVisible(true);
		ButtonOtis->setVisible(true);
		ButtonAllyHgrunt->setVisible(true);
		ButtonMedicHgrunt->setVisible(true);
		ButtonTorchHgrunt->setVisible(true);
	}
	else if (iShowText == 10) // Enemy Humans and Machines
	{
		m_pButtonNPCMenu->setVisible(false);
		m_pButtonMilAliens->setVisible(true);
		m_pButtonWildAliens->setVisible(true);
		m_pButtonFriendHumans->setVisible(true);
		m_pButtonMiscNPCS->setVisible(true);

		ButtonHGrunt->setVisible(true);
		ButtonMassassin->setVisible(true);
		ButtonFassassin->setVisible(true);
		ButtonApache->setVisible(true);
		ButtonSentry->setVisible(true);
	}
	else if (iShowText == 11) // Misc Monsters
	{
		m_pButtonNPCMenu->setVisible(false);
		m_pButtonMilAliens->setVisible(true);
		m_pButtonWildAliens->setVisible(true);
		m_pButtonFriendHumans->setVisible(true);
		m_pButtonMilUnits->setVisible(true);

		ButtonBoid->setVisible(true);
		ButtonLeech->setVisible(true);
		ButtonCockRoach->setVisible(true);
		ButtonRat->setVisible(true);
		ButtonGman->setVisible(true);
		ButtonTentacle->setVisible(true);
	}
}

CWeaponsMenu ::CWeaponsMenu(int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CGMMenuBase(iTrans, iRemoveMe, x, y, wide, tall)
{
	//m_pButtonSweepMenu->setVisible(false);
	//m_pButtonAimMode->setVisible(true);

	ButtonHelper("#Gomod_GiveItem_Mode", "button_self_pickup", iXPos + XRES(16), iYPos + iYSize - YRES(10) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
}

CItemsMenu::CItemsMenu(int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CGMMenuBase(iTrans, iRemoveMe, x, y, wide, tall)
{
	//m_pButtonItemsMenu->setVisible(false);
	//m_pButtonAimMode->setVisible(true);

	ButtonHelper("#Gomod_GiveItem_Mode", "button_self_pickup", iXPos + XRES(16), iYPos + iYSize - YRES(10) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
}

CMonstersMenu::CMonstersMenu(int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CGMMenuBase(iTrans, iRemoveMe, x, y, wide, tall)
{
	//m_pButtonNPCMenu->setVisible(false);
	//m_pButtonAimMode->setVisible(true);

	ButtonHelper("#Gomod_MenuButton_NoAI", "button_ai_set", iXPos + XRES(144), iYPos + iYSize - YRES(30) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_MenuButton_Ally", "button_allied_set", iXPos + XRES(208), iYPos + iYSize - YRES(30) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_MenuButton_undo", "button_npc_undo", iXPos + XRES(144), iYPos + iYSize - YRES(4) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_MenuButton_remove", "button_npc_remove_all", iXPos + XRES(208), iYPos + iYSize - YRES(4) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
}

CToolsMenu::CToolsMenu(int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CGMMenuBase(iTrans, iRemoveMe, x, y, wide, tall)
{
	//m_pButtonToolsMenu->setVisible(false);

	ButtonHelper("#Gomod_Tool_Duplicator", "tool duplicator", iXPos + XRES(16), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_Remover", "tool remover", iXPos + XRES(80), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_Gib", "tool gibber", iXPos + XRES(144), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_Poser", "tool poser", iXPos + XRES(208), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_Camera", "tool camera", iXPos + XRES(272), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_HPModify", "tool health_set", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelper("#Gomod_Tool_NoCollide", "tool no_collide", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_TakeDamage", "tool take_damage", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_BloodColor", "tool blood_color", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_FrameEditor", "tool frame_set", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_Teleporter", "tool teleporter", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelper("#Gomod_Tool_Glowsticks", "tool glowsticks", iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelper("#Gomod_Tool_NoTools", "tool none", iXPos + XRES(80), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	CommandButton* pButtonRender;
	pButtonRender = new CommandButton(CHudTextMessage::BufferedLocaliseTextString(""), iXPos + XRES(336), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	pButtonRender->m_bNoMarginSpace = true;
	pButtonRender->setText(gHUD.m_TextMessage.BufferedLocaliseTextString("#Gomod_Tool_Render"));
	pButtonRender->addActionSignal(new CMenuHandler_StringCommand("tool render"));
	pButtonRender->addActionSignal(new CMenuHandler_StringCommand("toggleRenderMenu"));
	pButtonRender->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	pButtonRender->setParent(this);
}

CRenderToolMenu::CRenderToolMenu(int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CGMMenuBase(iTrans, iRemoveMe, x, y, wide, tall)
{
	ButtonHelper("#Gomod_RenderMode_Normal", "rendermode normal", iXPos + XRES(16), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderMode_Color", "rendermode color", iXPos + XRES(80), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderMode_Texture", "rendermode texture", iXPos + XRES(144), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderMode_Glow", "rendermode glow", iXPos + XRES(208), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderMode_Solid", "rendermode solid", iXPos + XRES(272), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderMode_Additive", "rendermode additive", iXPos + XRES(336), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));

	ButtonHelper("#Gomod_RenderFX_None", "renderfx normal", iXPos + XRES(16), iYPos + iYSize - YRES(190) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_SPulse", "renderfx slow_pulse", iXPos + XRES(80), iYPos + iYSize - YRES(190) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_FPulse", "renderfx fast_pulse", iXPos + XRES(144), iYPos + iYSize - YRES(190) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_SWPulse", "renderfx slow_wide_pulse", iXPos + XRES(208), iYPos + iYSize - YRES(190) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_FWPulse", "renderfx fast_wide_pulse", iXPos + XRES(272), iYPos + iYSize - YRES(190) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_SFadeAway", "renderfx slow_fade_away", iXPos + XRES(336), iYPos + iYSize - YRES(190) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));

	ButtonHelper("#Gomod_RenderFX_FFadeAway", "renderfx fast_fade_away", iXPos + XRES(16), iYPos + iYSize - YRES(164) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_SSolid", "renderfx slow_become_solid", iXPos + XRES(80), iYPos + iYSize - YRES(164) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_FSolid", "renderfx fast_become_solid", iXPos + XRES(144), iYPos + iYSize - YRES(164) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_SStrobe", "renderfx slow_strobe", iXPos + XRES(208), iYPos + iYSize - YRES(164) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_FStrobe", "renderfx fast_strobe", iXPos + XRES(272), iYPos + iYSize - YRES(164) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_FRStrobe", "renderfx faster_strobe", iXPos + XRES(336), iYPos + iYSize - YRES(164) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));

	ButtonHelper("#Gomod_RenderFX_SFlicker", "renderfx slow_flicker", iXPos + XRES(16), iYPos + iYSize - YRES(138) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_FFlicker", "renderfx fast_flicker", iXPos + XRES(80), iYPos + iYSize - YRES(138) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_ConstantGlow", "renderfx constant_glow", iXPos + XRES(144), iYPos + iYSize - YRES(138) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_Distort", "renderfx distort", iXPos + XRES(208), iYPos + iYSize - YRES(138) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_Hologram", "renderfx hologram", iXPos + XRES(272), iYPos + iYSize - YRES(138) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelper("#Gomod_RenderFX_Explode", "renderfx explode", iXPos + XRES(336), iYPos + iYSize - YRES(138) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));

	ButtonHelper("#Gomod_RenderFX_GlowShell", "renderfx glow_shell", iXPos + XRES(16), iYPos + iYSize - YRES(112) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
}