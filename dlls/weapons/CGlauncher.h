/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#pragma once

enum glauncher_e
{
	GL_IDLE1 = 0,
	GL_IDLE2,
	GL_IDLEFIDGET,
	GL_FIRE1,
	GL_DRAW,
	GL_RELOAD,
	GL_RELOAD_EMPTY
};

class CGLauncher : public CBasePlayerWeapon
{
public:
	void Spawn() override;
	void Precache() override;
	int iItemSlot() override { return 4; }
	bool GetItemInfo(ItemInfo* p) override;
	//	void AddToPlayer(CBasePlayer* pPlayer) override;
	void PrimaryAttack() override;
	void SecondaryAttack();
	bool Deploy() override;
	void Holster() override;
	void Reload() override;
	void WeaponIdle() override;

	int m_iFire;
	int m_iSmoke;

	bool UseDecrement() override
	{
#if defined(CLIENT_WEAPONS)
		return UTIL_DefaultUseDecrement();
#else
		return false;
#endif
	}

	const char* MyWModel() { return "models/w_glauncher.mdl"; }

private:
	unsigned short m_usFireGLauncher;
};
