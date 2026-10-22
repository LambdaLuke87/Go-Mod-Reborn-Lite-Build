/***
 *
 *	Grenade Launcher - port of the Featureful SDK weapon to Half-Life: Opposing Force Updated SDK.
 *
 *	Differences from the Featureful version:
 *	- Written as a regular CBasePlayerWeapon (no CConfigurableWeapon / WeaponParameters system).
 *	- Shotgun-style reload (one grenade at a time) using m_fInSpecialReload, synced to the client for prediction.
 *	- The projectile ("grenaderound") derives from CGrenade, without the Featureful projectile / NamedSoundScript / NamedVisual systems.
 *
 ****/

#pragma once

enum GrenadeLauncherAnim
{
	GL_IDLE = 0,
	PL_IDLE,
	GL_FIRE,
	PL_FIRE,
	GL_RELOAD_DN,
	GL_RELOAD_UP,
	PL_RELOAD_DN,
	PL_RELOAD_UP,
	GL_DRAW,
	PL_DRAW,
	GL_HOLSTER,
	PL_HOLSTER
};

class CGrenadeLauncher : public CBasePlayerWeapon
{
public:
	using BaseClass = CBasePlayerWeapon;

	void Precache() override;
	void Spawn() override;

	bool Deploy() override;
	void Holster() override;

	void PrimaryAttack() override;
	void Reload() override;
	void WeaponIdle() override;

	int iItemSlot() override { return 4; }
	bool GetItemInfo(ItemInfo* p) override;

	void GetWeaponData(weapon_data_t& data) override;
	void SetWeaponData(const weapon_data_t& data) override;

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
	unsigned short m_usGrenadeLauncher;
};
