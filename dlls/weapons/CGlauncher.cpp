/***
 *
 *	Grenade Launcher - port of the Featureful SDK weapon to Half-Life: Opposing Force Updated SDK.
 *	See CGrenadeLauncher.h for the list of differences from the Featureful version.
 *
 ****/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "soundent.h"
#include "skill.h"

#include "CGlauncher.h"

// Projectile spawn point relative to the gun position (same offsets as Featureful)
#define GRENADELAUNCHER_OFFSET_FORWARD 8.0f
#define GRENADELAUNCHER_OFFSET_SIDE 8.0f
#define GRENADELAUNCHER_OFFSET_UP -16.0f

// Same lag compensation as the Nailgun (server-created entities reach the owner late, so while strafing the grenade
// looked like it was born where the player was a moment ago). Set to 0 to disable.
#define GRENADELAUNCHER_LAG_COMPENSATION 1
#define GRENADELAUNCHER_INTERP_TIME 0.1f
#define GRENADELAUNCHER_MAX_LAG_COMPENSATION 0.25f

#define GRENADEROUND_SPEED 600.0f
#define GRENADEROUND_FUSE_TIME 2.5f

//=========================================================
// Grenade round projectile (server only)
//=========================================================
#ifndef CLIENT_DLL
class CGrenadeRound : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;

	void EXPORT RoundTouch(CBaseEntity* pOther);
	void EXPORT RoundThink();

	static CGrenadeRound* CreateRound(const Vector& vecOrigin, const Vector& vecDir, const Vector& vecUp, CBaseEntity* pOwner);

private:
	static int s_iTrailSprite;
};

int CGrenadeRound::s_iTrailSprite = 0;

LINK_ENTITY_TO_CLASS(grenaderound, CGrenadeRound);

static const char* const g_pGrenadeRoundBounceSounds[] =
	{
		"weapons/grenade_hit1.wav",
		"weapons/grenade_hit2.wav",
		"weapons/grenade_hit3.wav",
};

void CGrenadeRound::Spawn()
{
	Precache();

	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/pipebomb.mdl");
	pev->skin = 1;

	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	pev->dmg = gSkillData.plrDmgHandGrenade;
	m_fRegisteredSound = false;
}

void CGrenadeRound::Precache()
{
	PRECACHE_MODEL("models/pipebomb.mdl");
	s_iTrailSprite = PRECACHE_MODEL("sprites/smoke.spr");

	for (int i = 0; i < ARRAYSIZE(g_pGrenadeRoundBounceSounds); i++)
		PRECACHE_SOUND(g_pGrenadeRoundBounceSounds[i]);
}

CGrenadeRound* CGrenadeRound::CreateRound(const Vector& vecOrigin, const Vector& vecDir, const Vector& vecUp, CBaseEntity* pOwner)
{
	CGrenadeRound* pRound = GetClassPtr((CGrenadeRound*)nullptr);
	pRound->pev->classname = MAKE_STRING("grenaderound");
	UTIL_SetOrigin(pRound->pev, vecOrigin);
	pRound->pev->angles = UTIL_VecToAngles(vecDir);
	pRound->pev->owner = pOwner ? pOwner->edict() : nullptr;
	pRound->Spawn();

	pRound->pev->velocity = vecDir * GRENADEROUND_SPEED + vecUp * (GRENADEROUND_SPEED / 3.0f);
	pRound->pev->avelocity = Vector(300, 300, 300);
	pRound->pev->friction = 0.5f;
	pRound->pev->dmgtime = gpGlobals->time + GRENADEROUND_FUSE_TIME;

	pRound->SetTouch(&CGrenadeRound::RoundTouch);
	pRound->SetThink(&CGrenadeRound::RoundThink);
	pRound->pev->nextthink = gpGlobals->time;

	// smoke trail
	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(pRound->entindex());
	WRITE_SHORT(s_iTrailSprite);
	WRITE_BYTE(20); // life in 0.1 s
	WRITE_BYTE(4);	// width
	WRITE_BYTE(224);
	WRITE_BYTE(224);
	WRITE_BYTE(255);
	WRITE_BYTE(200); // brightness
	MESSAGE_END();

	return pRound;
}

void CGrenadeRound::RoundTouch(CBaseEntity* pOther)
{
	// don't hit the guy that launched this grenade
	if (pOther->edict() == pev->owner)
		return;

	// explodes on contact with monsters and players, bounces off everything else
	if (pOther->pev->takedamage == DAMAGE_AIM)
	{
		SetThink(nullptr);
		SetTouch(nullptr);
		Detonate();
		return;
	}

	if (!m_fRegisteredSound)
	{
		Vector vecTestVelocity = pev->velocity;
		vecTestVelocity.z *= 0.45f;

		if (vecTestVelocity.Length() <= 60)
		{
			CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin, (int)(pev->dmg * 2.5f), 0.3f);
			m_fRegisteredSound = true;
		}
	}

	if ((pev->flags & FL_ONGROUND) != 0)
	{
		// add a bit of static friction
		pev->velocity = pev->velocity * 0.75f;

		if (pev->velocity.Length() <= 20)
			pev->avelocity = g_vecZero;
	}
	else
	{
		EMIT_SOUND(ENT(pev), CHAN_VOICE, g_pGrenadeRoundBounceSounds[RANDOM_LONG(0, ARRAYSIZE(g_pGrenadeRoundBounceSounds) - 1)], 0.25f, ATTN_NORM);
	}

	if (pev->velocity == g_vecZero)
		pev->avelocity = g_vecZero;
}

void CGrenadeRound::RoundThink()
{
	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1f;

	if (pev->dmgtime - 1 < gpGlobals->time)
	{
		CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin + pev->velocity * (pev->dmgtime - gpGlobals->time), 400, 0.1f);
	}

	if (pev->dmgtime <= gpGlobals->time)
	{
		SetThink(&CGrenade::Detonate);
	}
}
#endif

//=========================================================
// Grenade Launcher
//=========================================================
LINK_ENTITY_TO_CLASS(weapon_grenadelauncher, CGrenadeLauncher);

void CGrenadeLauncher::Precache()
{
	BaseClass::Precache();

	PRECACHE_MODEL("models/v_glauncher.mdl");
	PRECACHE_MODEL("models/p_glauncher.mdl");
	PRECACHE_MODEL(MyWModel());

	PRECACHE_SOUND("weapons/glauncher.wav");

#ifndef CLIENT_DLL
	UTIL_PrecacheOther("grenaderound");
#endif

	m_usGrenadeLauncher = PRECACHE_EVENT(1, "events/glauncher.sc");
}

void CGrenadeLauncher::Spawn()
{
	pev->classname = MAKE_STRING("weapon_grenadelauncher");
	m_iId = WEAPON_GLAUNCHER;

	Precache();

	// TFC has no w_ models for its weapons, so the weaponbox is used as world model
	SET_MODEL(ENT(pev), MyWModel());

	m_iDefaultAmmo = GRENADELAUNCHER_DEFAULT_GIVE;

	FallInit(); // get ready to fall down.
}

bool CGrenadeLauncher::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "grenades";
	p->iMaxAmmo1 = GRENADELAUNCHER_MAX_CARRY;
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = WEAPON_NOCLIP;
	p->iMaxClip = GRENADELAUNCHER_MAX_CLIP;
	p->iSlot = 5;
	p->iPosition = 3;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_GLAUNCHER;
	p->iWeight = GRENADELAUNCHER_WEIGHT;
	return true;
}

bool CGrenadeLauncher::Deploy()
{
	m_fInSpecialReload = 0;
	return DefaultDeploy("models/v_glauncher.mdl", "models/p_glauncher.mdl", GL_DRAW, "mp5");
}

void CGrenadeLauncher::Holster()
{
	// an interrupted reload isn't resumed on deploy
	m_fInSpecialReload = 0;
	BaseClass::Holster();
}

void CGrenadeLauncher::PrimaryAttack()
{
	if (m_iClip <= 0)
	{
		// empty: start reloading, or click if there is nothing to reload with
		Reload();
		if (m_iClip == 0)
			PlayEmptySound();
		return;
	}

	// firing interrupts an ongoing reload
	m_fInSpecialReload = 0;

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	--m_iClip;

	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	int flags;
#if defined(CLIENT_WEAPONS)
	flags = UTIL_DefaultPlaybackFlags();
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL(flags, m_pPlayer->edict(), m_usGrenadeLauncher, 0, g_vecZero, g_vecZero, 0.0, 0.0, GL_FIRE, 0, 0, 0);

#ifndef CLIENT_DLL
	UTIL_MakeVectors(m_pPlayer->pev->v_angle);

	const Vector vecAiming = gpGlobals->v_forward;
	const Vector vecUp = gpGlobals->v_up;
	const Vector vecGun = m_pPlayer->GetGunPosition();

	Vector vecSrc = vecGun + gpGlobals->v_forward * GRENADELAUNCHER_OFFSET_FORWARD + gpGlobals->v_right * GRENADELAUNCHER_OFFSET_SIDE + gpGlobals->v_up * GRENADELAUNCHER_OFFSET_UP;

#if GRENADELAUNCHER_LAG_COMPENSATION
	{
		int ping = 0, packetLoss = 0;
		g_engfuncs.pfnGetPlayerStats(m_pPlayer->edict(), &ping, &packetLoss);

		float lag = GRENADELAUNCHER_INTERP_TIME + ping * 0.001f;
		if (lag > GRENADELAUNCHER_MAX_LAG_COMPENSATION)
			lag = GRENADELAUNCHER_MAX_LAG_COMPENSATION;

		const Vector vecShifted = vecSrc + m_pPlayer->pev->velocity * lag;

		// never move the spawn point through a wall
		TraceResult trShift;
		UTIL_TraceLine(vecSrc, vecShifted, ignore_monsters, m_pPlayer->edict(), &trShift);
		vecSrc = trShift.flFraction < 1.0f ? trShift.vecEndPos - (vecShifted - vecSrc).Normalize() * 2.0f : vecShifted;
	}
#endif

	// autoaim, then adjust the direction so the grenade heads to what the crosshair points at
	Vector vecDir = m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);
	if (vecDir == g_vecZero)
		vecDir = vecAiming;

	TraceResult tr;
	UTIL_TraceLine(vecGun, vecGun + vecDir * 8192.0f, dont_ignore_monsters, m_pPlayer->edict(), &tr);
	if (tr.flFraction < 1.0f)
	{
		const Vector vecToTarget = tr.vecEndPos - vecSrc;
		if (vecToTarget.Length() > 32.0f)
			vecDir = vecToTarget.Normalize();
	}

	CGrenadeRound::CreateRound(vecSrc, vecDir, vecUp, m_pPlayer);
#endif

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.6f;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.6f;
}

void CGrenadeLauncher::Reload()
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == GRENADELAUNCHER_MAX_CLIP)
		return;

	// don't reload until recoil is done
	if (m_flNextPrimaryAttack > UTIL_WeaponTimeBase())
		return;

	if (m_fInSpecialReload == 0)
	{
		// start of the reload
		SendWeaponAnim(GL_RELOAD_DN);
		m_fInSpecialReload = 1;
		m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.1f;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1f;
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.1f;
		return;
	}
	else if (m_fInSpecialReload == 1)
	{
		if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
			return;

		// loading one grenade
		m_fInSpecialReload = 2;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0f / 3.0f;
	}
	else
	{
		// the grenade is in
		m_iClip += 1;
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 1;
		m_fInSpecialReload = 1;
	}
}

void CGrenadeLauncher::WeaponIdle()
{
	ResetEmptySound();

	m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if (m_iClip == 0 && m_fInSpecialReload == 0 && 0 != m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		// auto reload
		Reload();
	}
	else if (m_fInSpecialReload != 0)
	{
		if (m_iClip != GRENADELAUNCHER_MAX_CLIP && 0 != m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		{
			Reload();
		}
		else
		{
			// end of the reload
			SendWeaponAnim(GL_RELOAD_UP);
			m_fInSpecialReload = 0;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.6f;
		}
	}
	else
	{
		SendWeaponAnim(GL_IDLE);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3.6f;
	}
}

void CGrenadeLauncher::GetWeaponData(weapon_data_t& data)
{
	BaseClass::GetWeaponData(data);
	data.m_fInSpecialReload = m_fInSpecialReload;
}

void CGrenadeLauncher::SetWeaponData(const weapon_data_t& data)
{
	BaseClass::SetWeaponData(data);
	m_fInSpecialReload = data.m_fInSpecialReload;
}

//=========================================================
// Grenades ammo pickup
//=========================================================
#ifndef CLIENT_DLL
class CGrenadeAmmoClip : public CBasePlayerAmmo
{
public:
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_ARgrenade.mdl");
		CBasePlayerAmmo::Spawn();
	}

	void Precache() override
	{
		PRECACHE_MODEL("models/w_ARgrenade.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}

	bool AddAmmo(CBaseEntity* pOther) override
	{
		if (pOther->GiveAmmo(AMMO_GRENADECLIP_GIVE, "grenades", GRENADELAUNCHER_MAX_CARRY) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};

LINK_ENTITY_TO_CLASS(ammo_grenadeclip, CGrenadeAmmoClip);
#endif
