/***
*
*	Nailgun (originally from Team Fortress Classic / Deathmatch Classic)
*	Port of the Featureful SDK weapon to Go-Mod: Reborn SDK.
*
****/

#pragma once

enum NailgunAnim
{
	NAILGUN_LONGIDLE = 0,
	NAILGUN_IDLE1,
	NAILGUN_LAUNCH,
	NAILGUN_RELOAD,
	NAILGUN_DEPLOY,
	NAILGUN_FIRE1,
	NAILGUN_FIRE2,
	NAILGUN_FIRE3
};

class CNailgun : public CBasePlayerWeapon
{
public:
	using BaseClass = CBasePlayerWeapon;

	void Precache() override;
	void Spawn() override;

	bool Deploy() override;
	void WeaponIdle() override;
	void PrimaryAttack() override;

	int iItemSlot() override { return 3; }
	bool GetItemInfo(ItemInfo* p) override;

	bool UseDecrement() override
	{
#if defined(CLIENT_WEAPONS)
		return UTIL_DefaultUseDecrement();
#else
		return false;
#endif
	}

private:
	unsigned short m_usNailgun;
};
