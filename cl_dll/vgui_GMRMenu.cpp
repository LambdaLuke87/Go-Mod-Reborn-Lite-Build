/**********************************************************************
 *                 Go-Mod: Reborn Lite Build                          *
 *                  LambdaLuke87 - 2025-2026                          *
 ********************************************************************/


#include "hud.h"
#include "cl_util.h"
#include "vgui_TeamFortressViewport.h"
#include "VGUI_TextImage.h"
#include <map>
#include <vector>

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

void CGMMenuBase::ButtonOptionHelper(CommandButton*& newbutton, const char* text, bool visible, int option, int x, int y, int wide, int tall)
{
	newbutton = new CommandButton("", x, y, wide, tall);
	newbutton->setVisible(visible);
	newbutton->m_bNoMarginSpace = true;
	newbutton->setText(gHUD.m_TextMessage.BufferedLocaliseTextString(text));
	newbutton->addInputSignal(new CHandler_MenuButtonOver(this, option));
	newbutton->setParent(this);
}

CMenuPanel* CLinkMenu_Create(const char* szMOTD, int iTrans, bool iRemoveMe, int x, int y, int wide, int tall)
{
	return new CLinkMenu(szMOTD, iTrans, iRemoveMe, x, y, wide, tall);
}

CLinkMenu::CLinkMenu(const char* szMOTD, int iTrans, int iRemoveMe, int x, int y, int wide, int tall) : CGMMenuBase(iTrans, iRemoveMe, x, y, wide, tall)
{
	CSchemeManager* pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle("Title Font");

	// Main Headers
	ButtonOptionHelper(ButtonLinkMenu, "#Gomod_MenuTab_Main", false, 0, iXPos + XRES(16), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	ButtonOptionHelper(ButtonSweepMenu, "#Gomod_MenuTab_Weapons", true, 1, iXPos + XRES(82), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	ButtonOptionHelper(ButtonItemsMenu, "#Gomod_MenuTab_Items", true, 2, iXPos + XRES(148), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	ButtonOptionHelper(ButtonNPCMenu, "#Gomod_MenuTab_Monsters", true, 3, iXPos + XRES(214), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);
	ButtonOptionHelper(ButtonToolsMenu, "#Gomod_MenuTab_Tools", true, 4, iXPos + XRES(280), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(94), BUTTON_SIZE_Y);

	// SubHeaders Weapons
	ButtonOptionHelper(ButtonHLSweeps, "#Gomod_SWEEP_TAB1", false, 5, iXPos + XRES(16), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonOptionHelper(ButtonOp4Sweeps, "#Gomod_SWEEP_TAB2", false, 6, iXPos + XRES(80), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	
	// SubHeaders NPCS
	ButtonOptionHelper(ButtonMilAliens, "#Gomod_NPC_TAB1", false, 7, iXPos + XRES(16), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonOptionHelper(ButtonWildAliens, "#Gomod_NPC_TAB2", false, 8, iXPos + XRES(80), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonOptionHelper(ButtonFriendHumans, "#Gomod_NPC_TAB3", false, 9, iXPos + XRES(144), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonOptionHelper(ButtonMilUnits, "#Gomod_NPC_TAB4", false, 10, iXPos + XRES(208), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonOptionHelper(ButtonMiscNPCS, "#Gomod_NPC_TAB5", false, 11, iXPos + XRES(272), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonOptionHelper(ButtonProps, "#Gomod_NPC_TAB6", false, 12, iXPos + XRES(336), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));

	// HL Weapons
	ButtonHelperWithID(ButtonCrowbar, "#Gomod_Weapon_Crowbar", "summon weapon_crowbar", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonPhysgun, "#Gomod_Weapon_Physgun", "summon weapon_physgun", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonToolbow, "#Gomod_Weapon_Toolbow", "summon weapon_toolbow", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonGlock, "#Gomod_Weapon_Glock", "summon weapon_9mmhandgun", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonPython, "#Gomod_Weapon_Python", "summon weapon_357", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonMP5, "#Gomod_Weapon_Mp5", "summon weapon_9mmAR", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelperWithID(ButtonShotgun, "#Gomod_Weapon_Shotgun", "summon weapon_shotgun", iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonCrossbow, "#Gomod_Weapon_Crossbow", "summon weapon_crossbow", iXPos + XRES(80), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonRPG, "#Gomod_Weapon_RPG", "summon weapon_rpg", iXPos + XRES(144), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonGauss, "#Gomod_Weapon_Gauss", "summon weapon_gauss", iXPos + XRES(208), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonEgon, "#Gomod_Weapon_Egon", "summon weapon_egon", iXPos + XRES(272), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonHiveHand, "#Gomod_Weapon_HiveHand", "summon weapon_hornetgun", iXPos + XRES(336), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelperWithID(ButtonGrenade, "#Gomod_Weapon_Grenade", "summon weapon_handgrenade", iXPos + XRES(16), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonSatchel, "#Gomod_Weapon_Satchel", "summon weapon_satchel", iXPos + XRES(80), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonTripmine, "#Gomod_Weapon_Tripmine", "summon weapon_tripmine", iXPos + XRES(144), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonSnark, "#Gomod_Weapon_Snark", "summon weapon_snark", iXPos + XRES(208), iYPos + iYSize - YRES(104) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	
	// op4 Weapons
	ButtonHelperWithID(ButtonKnife, "#Gomod_Weapon_Knife", "summon weapon_knife", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonPipeWrench, "#Gomod_Weapon_PipeWrench", "summon weapon_pipewrench", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonGrapple, "#Gomod_Weapon_Barnacle", "summon weapon_grapple", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonDEagle, "#Gomod_Weapon_DesertEgle", "summon weapon_eagle", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonM249, "#Gomod_Weapon_M249", "summon weapon_m249", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonSniperRifle, "#Gomod_Weapon_SniperRifle", "summon weapon_sniperrifle", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	
	ButtonHelperWithID(ButtonDisplacer, "#Gomod_Weapon_Displacer", "summon weapon_displacer", iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonShockRifle, "#Gomod_Weapon_ShockRifle", "summon weapon_shockrifle", iXPos + XRES(80), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonSporeLauncher, "#Gomod_Weapon_SporeLauncher", "summon weapon_sporelauncher", iXPos + XRES(144), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonPenguin, "#Gomod_Weapon_Penguin", "summon weapon_penguin", iXPos + XRES(208), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	// AMMO AND ITEMS
	ButtonHelperWithID(ButtonHealthKit, "#Gomod_Item_HealthKit", "summon item_healthkit", iXPos + XRES(16), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonBattery, "#Gomod_Item_Battery", "summon item_battery", iXPos + XRES(80), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonLongJump, "#Gomod_Item_LongJump", "summon item_longjump", iXPos + XRES(144), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonHEV, "#Gomod_Item_HEV", "summon item_suit", iXPos + XRES(208), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonGlockAmmo, "#Gomod_Item_GlockAmmo", "summon ammo_9mmclip", iXPos + XRES(272), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonMP5Ammo, "#Gomod_Item_Mp5Ammo", "summon ammo_9mmAR", iXPos + XRES(336), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelperWithID(ButtonARGrenades, "#Gomod_Item_ARGrenades", "summon ammo_ARgrenades", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonArrows, "#Gomod_Item_Arrows", "summon ammo_crossbow", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonUranium, "#Gomod_Item_Uranium", "summon ammo_gaussclip", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonRockets, "#Gomod_Item_Rockets", "summon ammo_rpgclip", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonBuckshot, "#Gomod_Item_Buckshot", "summon ammo_buckshot", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(Button357Ammo, "#Gomod_Item_357Ammo", "summon ammo_357", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelperWithID(Button556Ammo, "#Gomod_Item_556Ammo", "summon ammo_556", iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonSpores, "#Gomod_Item_Spores", "summon ammo_spore", iXPos + XRES(80), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(Button762Ammo, "#Gomod_Item_762Ammo", "summon ammo_762", iXPos + XRES(144), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

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

	// Props
	ButtonHelperWithID(ButtonXenHair, "#Gomod_Prop_Xen_Hair", "summon xen_hair", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonXenTree, "#Gomod_Prop_Xen_Tree", "summon xen_tree", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonXenPlantL, "#Gomod_Prop_Xen_PlantLight", "summon xen_plantlight", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonXenSporeXS, "#Gomod_Prop_Xen_SporeXS", "summon xen_spore_small", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonXenSporeL, "#Gomod_Prop_Xen_SporeL", "summon xen_spore_medium", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonXenSporeXL, "#Gomod_Prop_Xen_SporeXL", "summon xen_spore_large", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelperWithID(ButtonChiken, "#Gomod_Prop_Chiken", "summon prop_chiken", iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonChumtoad, "#Gomod_Prop_Chumtoads", "summon prop_chumtoad", iXPos + XRES(80), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonPropC4, "#Gomod_Prop_C4", "summon prop_c4", iXPos + XRES(144), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	// Tools
	ButtonHelperWithID(ButtonDuplicator, "#Gomod_Tool_Duplicator", "tool duplicator", iXPos + XRES(16), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonRemover, "#Gomod_Tool_Remover", "tool remover", iXPos + XRES(80), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonGibber, "#Gomod_Tool_Gib", "tool gibber", iXPos + XRES(144), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonPoser, "#Gomod_Tool_Poser", "tool poser", iXPos + XRES(208), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonCamera, "#Gomod_Tool_Camera", "tool camera", iXPos + XRES(272), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonHPModify, "#Gomod_Tool_HPModify", "tool health_set", iXPos + XRES(16), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelperWithID(ButtonNoCollide, "#Gomod_Tool_NoCollide", "tool no_collide", iXPos + XRES(80), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonTakeDamage, "#Gomod_Tool_TakeDamage", "tool take_damage", iXPos + XRES(144), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonBloodColor, "#Gomod_Tool_BloodColor", "tool blood_color", iXPos + XRES(208), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonFramedit, "#Gomod_Tool_FrameEditor", "tool frame_set", iXPos + XRES(272), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonTeleporter, "#Gomod_Tool_Teleporter", "tool teleporter", iXPos + XRES(336), iYPos + iYSize - YRES(184) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonHelperWithID(ButtonGlowsticks, "#Gomod_Tool_Glowsticks", "tool glowsticks", iXPos + XRES(16), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonHelperWithID(ButtonNoTools, "#Gomod_Tool_NoTools", "tool none", iXPos + XRES(80), iYPos + iYSize - YRES(144) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));

	ButtonRender = new CommandButton(CHudTextMessage::BufferedLocaliseTextString(""), iXPos + XRES(336), iYPos + iYSize - YRES(224) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y + YRES(5));
	ButtonRender->m_bNoMarginSpace = true;
	ButtonRender->setText(gHUD.m_TextMessage.BufferedLocaliseTextString("#Gomod_Tool_Render"));
	ButtonRender->addActionSignal(new CMenuHandler_StringCommand("tool render"));
	ButtonRender->addActionSignal(new CMenuHandler_StringCommand("toggleRenderMenu"));
	ButtonRender->addActionSignal(new CMenuHandler_TextWindow(HIDE_TEXTWINDOW));
	ButtonRender->setVisible(false);
	ButtonRender->setParent(this);

	// Options per menu
	ButtonHelperWithID(ButtonAimMode, "#Gomod_MenuButton_FrontSpawn", "button_front_spawn", iXPos + XRES(370), iYPos + iYSize - YRES(275) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(135), BUTTON_SIZE_Y - YRES(8));
	ButtonHelperWithID(ButtonGiveMode, "#Gomod_GiveItem_Mode", "button_self_pickup", iXPos + XRES(16), iYPos + iYSize - YRES(10) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelperWithID(ButtonNoAI, "#Gomod_MenuButton_NoAI", "button_ai_set", iXPos + XRES(144), iYPos + iYSize - YRES(30) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelperWithID(ButtonAllieds, "#Gomod_MenuButton_Ally", "button_allied_set", iXPos + XRES(208), iYPos + iYSize - YRES(30) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelperWithID(ButtonUndoNPC, "#Gomod_MenuButton_undo", "button_npc_undo", iXPos + XRES(144), iYPos + iYSize - YRES(4) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));
	ButtonHelperWithID(ButtonDeleteALlNpcs, "#Gomod_MenuButton_remove", "button_npc_remove_all", iXPos + XRES(208), iYPos + iYSize - YRES(4) - BUTTON_SIZE_Y, CMENU_SIZE_X - XRES(96), BUTTON_SIZE_Y - YRES(8));

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
	// Header Buttons, should always be displayed above
	std::vector<CommandButton*> headerbuttons = {
		ButtonLinkMenu, ButtonSweepMenu, ButtonItemsMenu, ButtonNPCMenu,
		ButtonToolsMenu};

	// All menu buttons (hide them in bulk)
	std::vector<CommandButton*> allButtons = {
		ButtonHLSweeps, ButtonOp4Sweeps, ButtonAimMode, ButtonGiveMode,
		ButtonHealthKit, ButtonBattery, ButtonLongJump, ButtonHEV,
		ButtonGlockAmmo, ButtonMP5Ammo, ButtonARGrenades, ButtonArrows,
		ButtonUranium, ButtonRockets, ButtonBuckshot, Button357Ammo,
		Button556Ammo, ButtonSpores, Button762Ammo, ButtonMilAliens,
		ButtonWildAliens, ButtonFriendHumans, ButtonMilUnits,
		ButtonMiscNPCS, ButtonProps, ButtonNoAI, ButtonAllieds,
		ButtonUndoNPC, ButtonDeleteALlNpcs, ButtonDuplicator,
		ButtonRemover, ButtonGibber, ButtonPoser, ButtonCamera,
		ButtonHPModify, ButtonNoCollide, ButtonTakeDamage, ButtonBloodColor,
		ButtonFramedit, ButtonTeleporter, ButtonGlowsticks, ButtonNoTools,
		ButtonRender, ButtonCrowbar, ButtonPhysgun, ButtonToolbow,
		ButtonGlock, ButtonPython, ButtonMP5, ButtonShotgun, ButtonCrossbow,
		ButtonRPG, ButtonGauss, ButtonEgon, ButtonHiveHand, ButtonGrenade,
		ButtonSatchel, ButtonTripmine, ButtonSnark, ButtonKnife,
		ButtonPipeWrench, ButtonGrapple, ButtonDEagle, ButtonM249,
		ButtonSniperRifle, ButtonDisplacer, ButtonShockRifle,
		ButtonSporeLauncher, ButtonPenguin, ButtonAlienSlave,
		ButtonAlienGrunt, ButtonAlienController, ButtonGargantua,
		ButtonShockTrooper, ButtonVoltigore, ButtonNihilant, ButtonHeadcrab,
		ButtonBabycrab, ButtonShockRoach, ButtonBabyVoltigore,
		ButtonHoundeye, ButtonBullsquid, ButtonPitdrone, ButtonBigMomma,
		ButtonZombie, ButtonZombieBarney, ButtonZombieSoldier, ButtonGonome,
		ButtonIchthyosaur, ButtonBarnacle, ButtonScientist, ButtonClScientist,
		ButtonBarney, ButtonOtis, ButtonAllyHgrunt, ButtonMedicHgrunt,
		ButtonTorchHgrunt, ButtonHGrunt, ButtonMassassin, ButtonFassassin,
		ButtonApache, ButtonSentry, ButtonBoid, ButtonLeech, ButtonCockRoach,
		ButtonRat, ButtonGman, ButtonTentacle, ButtonXenHair, ButtonXenTree,
		ButtonXenPlantL, ButtonXenSporeXS, ButtonXenSporeL, ButtonXenSporeXL,
		ButtonChiken, ButtonChumtoad, ButtonPropC4};

	// Mapping button groups by category
	std::map<int, std::vector<CommandButton*>> menuGroups = {
		{0, {ButtonLinkMenu}},
		{1, {ButtonHLSweeps, ButtonOp4Sweeps, ButtonAimMode, ButtonGiveMode}},
		{2, {ButtonHealthKit, ButtonBattery, ButtonLongJump, ButtonHEV,
				ButtonGlockAmmo, ButtonMP5Ammo, ButtonARGrenades, ButtonArrows,
				ButtonUranium, ButtonRockets, ButtonBuckshot, Button357Ammo,
				Button556Ammo, ButtonSpores, Button762Ammo, ButtonAimMode, ButtonGiveMode}},
		{3, {ButtonMilAliens, ButtonWildAliens, ButtonFriendHumans, ButtonMilUnits,
				ButtonMiscNPCS, ButtonProps, ButtonAimMode, ButtonNoAI,
				ButtonAllieds, ButtonUndoNPC, ButtonDeleteALlNpcs}},
		{4, {ButtonDuplicator, ButtonRemover, ButtonGibber, ButtonPoser, ButtonCamera,
				ButtonHPModify, ButtonNoCollide, ButtonTakeDamage, ButtonBloodColor,
				ButtonFramedit, ButtonTeleporter, ButtonGlowsticks, ButtonNoTools,
				ButtonRender}},
		{5, {ButtonCrowbar, ButtonPhysgun, ButtonToolbow, ButtonGlock, ButtonPython,
				ButtonMP5, ButtonShotgun, ButtonCrossbow, ButtonRPG, ButtonGauss,
				ButtonEgon, ButtonHiveHand, ButtonGrenade, ButtonSatchel, ButtonTripmine,
				ButtonSnark, ButtonAimMode, ButtonGiveMode, ButtonOp4Sweeps}},
		{6, {ButtonKnife, ButtonPipeWrench, ButtonGrapple, ButtonDEagle, ButtonM249,
				ButtonSniperRifle, ButtonDisplacer, ButtonShockRifle, ButtonSporeLauncher,
				ButtonPenguin, ButtonAimMode, ButtonGiveMode, ButtonHLSweeps}},
		{7, {ButtonAlienSlave, ButtonAlienGrunt, ButtonAlienController, ButtonGargantua,
				ButtonShockTrooper, ButtonVoltigore, ButtonNihilant, ButtonAimMode,
				ButtonNoAI, ButtonAllieds, ButtonUndoNPC, ButtonDeleteALlNpcs,
				ButtonWildAliens, ButtonFriendHumans, ButtonMilUnits, ButtonMiscNPCS,ButtonProps}},
		{8, {ButtonHeadcrab, ButtonBabycrab, ButtonShockRoach, ButtonBabyVoltigore,
				ButtonHoundeye, ButtonBullsquid, ButtonPitdrone, ButtonBigMomma, ButtonZombie,
				ButtonZombieBarney, ButtonZombieSoldier, ButtonGonome, ButtonIchthyosaur,
				ButtonBarnacle, ButtonAimMode, ButtonNoAI, ButtonAllieds, ButtonUndoNPC,
				ButtonDeleteALlNpcs, ButtonMilAliens, ButtonFriendHumans, ButtonMilUnits, ButtonMiscNPCS, 
				ButtonProps}},
		{9, {ButtonScientist, ButtonClScientist, ButtonBarney, ButtonOtis, ButtonAllyHgrunt,
				ButtonMedicHgrunt, ButtonTorchHgrunt, ButtonAimMode, ButtonNoAI, ButtonAllieds,
				ButtonUndoNPC, ButtonDeleteALlNpcs, ButtonMilAliens, ButtonWildAliens, ButtonMilUnits, ButtonMiscNPCS,
				ButtonProps}},
		{10, {ButtonHGrunt, ButtonMassassin, ButtonFassassin, ButtonApache, ButtonSentry,
				 ButtonAimMode, ButtonNoAI, ButtonAllieds, ButtonUndoNPC, ButtonDeleteALlNpcs, 
				 ButtonMilAliens, ButtonWildAliens, ButtonFriendHumans, ButtonMiscNPCS, ButtonProps}},
		{11, {ButtonBoid, ButtonLeech, ButtonCockRoach, ButtonRat, ButtonGman, ButtonTentacle,
				 ButtonAimMode, ButtonNoAI, ButtonAllieds, ButtonUndoNPC, ButtonDeleteALlNpcs,
				 ButtonMilAliens, ButtonWildAliens, ButtonFriendHumans, ButtonMilUnits, ButtonProps}},
		{12, {ButtonXenHair, ButtonXenTree, ButtonXenPlantL, ButtonXenSporeXS,
				 ButtonXenSporeL, ButtonXenSporeXL, ButtonChiken, ButtonChumtoad,
				 ButtonPropC4, ButtonAimMode, ButtonNoAI, ButtonAllieds, ButtonUndoNPC,
				 ButtonDeleteALlNpcs, ButtonMilAliens, ButtonWildAliens, ButtonMilUnits, ButtonFriendHumans, 
				 ButtonMiscNPCS}},
	};

	// Make Headers Visible
	for (auto* btn : headerbuttons)
		if (btn)
			btn->setVisible(true);

	// Config per Menu
	if (iShowText == 0)
	{
		pScrollPanel->setVisible(true);
		pText->setVisible(true);
		ButtonLinkMenu->setVisible(false);
	}
	else
	{
		pScrollPanel->setVisible(false);
		pText->setVisible(false);
	}

	// Hide All the buttons
	for (auto* btn : allButtons)
		if (btn)
			btn->setVisible(false);

	// Show those of the active group
	auto it = menuGroups.find(iShowText);
	if (it != menuGroups.end())
	{
		for (auto* btn : it->second)
			if (btn)
				btn->setVisible(true);
	}

	// Hide the main button of the active section
	switch (iShowText)
	{
	case 1: // Weapons
	case 5: // HL Weapons
	case 6: // OP4 Weapons
		ButtonSweepMenu->setVisible(false);
		break;

	case 2: // Items and Ammo
		ButtonItemsMenu->setVisible(false);
		break;

	case 3: // NPCs
	case 7: // Military Aliens
	case 8: // Wildlife Aliens and Zombies
	case 9: // Friendly Humans
	case 10: // Enemy Humans and Machines
	case 11: // Misc Monsters
	case 12: // Props
		ButtonNPCMenu->setVisible(false);
		break;

	case 4: // Tools
		ButtonToolsMenu->setVisible(false);
		break;

	default:
		break;
	}
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