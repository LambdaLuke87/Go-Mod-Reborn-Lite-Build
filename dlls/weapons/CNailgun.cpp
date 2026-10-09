/***
 *
 *	Nailgun (originally from Team Fortress Classic / Deathmatch Classic)
 *	Port of the Featureful SDK weapon to Go-Mod: Reborn SDK.
 *
 ****/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "soundent.h"
#include "skill.h"

#ifndef CLIENT_DLL
#include "gamerules.h"
#endif

#include "CNailgun.h"

//=========================================================
// Nail projectile (server only)
//=========================================================
#ifndef CLIENT_DLL
class CNail : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;

	void EXPORT NailTouch(CBaseEntity* pOther);

	static CNail* CreateNail(const Vector& vecOrigin, const Vector& vecDir, CBaseEntity* pOwner);
};

LINK_ENTITY_TO_CLASS(nail, CNail);

static const char* const g_pNailHitBodySounds[] =
	{
		"weapons/xbow_hitbod1.wav",
		"weapons/xbow_hitbod2.wav",
};

static const char* const g_pNailHitWallSounds[] =
	{
		"weapons/ric1.wav",
		"weapons/ric2.wav",
		"weapons/ric3.wav",
		"weapons/ric4.wav",
		"weapons/ric5.wav",
};

#define NAIL_SPEED 1000.0f

void CNail::Spawn()
{
	Precache();

	pev->movetype = MOVETYPE_FLYMISSILE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/nail.mdl");

	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	UTIL_SetOrigin(pev, pev->origin);

	SetTouch(&CNail::NailTouch);
}

void CNail::Precache()
{
	PRECACHE_MODEL("models/nail.mdl");

	for (int i = 0; i < ARRAYSIZE(g_pNailHitBodySounds); i++)
		PRECACHE_SOUND(g_pNailHitBodySounds[i]);
	for (int i = 0; i < ARRAYSIZE(g_pNailHitWallSounds); i++)
		PRECACHE_SOUND(g_pNailHitWallSounds[i]);
}

CNail* CNail::CreateNail(const Vector& vecOrigin, const Vector& vecDir, CBaseEntity* pOwner)
{
	CNail* pNail = GetClassPtr((CNail*)nullptr);
	pNail->pev->classname = MAKE_STRING("nail");
	UTIL_SetOrigin(pNail->pev, vecOrigin);
	pNail->pev->angles = UTIL_VecToAngles(vecDir);
	pNail->Spawn();
	pNail->pev->owner = pOwner ? pOwner->edict() : nullptr;
	pNail->pev->velocity = vecDir * NAIL_SPEED;
	return pNail;
}

void CNail::NailTouch(CBaseEntity* pOther)
{
	SetTouch(nullptr);

	TraceResult tr = UTIL_GetGlobalTrace();

	const Vector vecDir = pev->velocity.Normalize();

	// leave a bullet-hole decal on the world
	DecalGunshot(&tr, BULLET_PLAYER_MP5);

	if (0 != pOther->pev->takedamage)
	{
		entvars_t* pevOwner = VARS(pev->owner);

		// player's nailgun and monsters' nails can have different damage
		const bool firedByPlayer = !FNullEnt(pev->owner) && (pev->owner->v.flags & FL_CLIENT) != 0;
		const float flDamage = firedByPlayer ? gSkillData.plrDmgNail : gSkillData.nailDmg;

		ClearMultiDamage();
		pOther->TraceAttack(pevOwner, flDamage, vecDir, &tr, DMG_GENERIC | DMG_NEVERGIB);
		ApplyMultiDamage(pev, pevOwner);

		if ((pOther->pev->flags & FL_CLIENT) != 0 || RANDOM_LONG(0, 1))
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_pNailHitBodySounds[RANDOM_LONG(0, ARRAYSIZE(g_pNailHitBodySounds) - 1)], 1.0f, ATTN_NORM, 0, RANDOM_LONG(105, 110));
	}
	else
	{
		if (RANDOM_LONG(0, 1))
			EMIT_SOUND(ENT(pev), CHAN_BODY, g_pNailHitWallSounds[RANDOM_LONG(0, ARRAYSIZE(g_pNailHitWallSounds) - 1)], 1.0f, ATTN_NORM);
	}

	SetThink(&CNail::SUB_Remove);
	pev->nextthink = gpGlobals->time;
}
#endif

//=========================================================
// Nailgun
//=========================================================
LINK_ENTITY_TO_CLASS(weapon_nailgun, CNailgun);

void CNailgun::Precache()
{
	BaseClass::Precache();

	PRECACHE_MODEL("models/v_nailgun.mdl");
	PRECACHE_MODEL("models/p_nailgun.mdl");
	PRECACHE_MODEL("models/w_weaponbox.mdl");

	PRECACHE_SOUND("weapons/airgun_1.wav");

#ifndef CLIENT_DLL
	UTIL_PrecacheOther("nail");
#endif

	m_usNailgun = PRECACHE_EVENT(1, "events/nailgun.sc");
}

void CNailgun::Spawn()
{
	pev->classname = MAKE_STRING("weapon_nailgun");
	m_iId = WEAPON_NAILGUN;

	Precache();

	// TFC has no w_ models for its weapons, so the weaponbox is used as world model
	SET_MODEL(ENT(pev), "models/w_weaponbox.mdl");

	m_iDefaultAmmo = NAILGUN_DEFAULT_GIVE;

	FallInit(); // get ready to fall down.
}

bool CNailgun::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "nails";
	p->iMaxAmmo1 = NAILGUN_MAX_CARRY;
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = WEAPON_NOCLIP;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 2;
	p->iPosition = 3;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_NAILGUN;
	p->iWeight = NAILGUN_WEIGHT;
	return true;
}

bool CNailgun::Deploy()
{
	return DefaultDeploy("models/v_nailgun.mdl", "models/p_nailgun.mdl", NAILGUN_DEPLOY, "mp5");
}

void CNailgun::PrimaryAttack()
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.15;
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	--m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType];

	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	// anim and pitch are picked with the shared random so client prediction and server agree
	const int iAnim = UTIL_SharedRandomLong(m_pPlayer->random_seed, NAILGUN_FIRE1, NAILGUN_FIRE3);
	const int iPitch = UTIL_SharedRandomLong(m_pPlayer->random_seed, 93, 124);

	int flags;
#if defined(CLIENT_WEAPONS)
	flags = UTIL_DefaultPlaybackFlags();
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL(flags, m_pPlayer->edict(), m_usNailgun, 0, g_vecZero, g_vecZero, 0.0, 0.0, iAnim, iPitch, 0, 0);

#ifndef CLIENT_DLL
	// The projectile respects the punch angle and is adjusted to hit what the crosshair points at
	const Vector vecAnglesAim = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
	UTIL_MakeVectors(vecAnglesAim);

	const Vector vecAiming = gpGlobals->v_forward;
	const Vector vecGun = m_pPlayer->GetGunPosition();
	const Vector vecSrc = vecGun + gpGlobals->v_right * 2.0f + gpGlobals->v_up * -4.0f;

	Vector vecDir = vecAiming;

	TraceResult tr;
	UTIL_TraceLine(vecGun, vecGun + vecAiming * 8192.0f, dont_ignore_monsters, m_pPlayer->edict(), &tr);
	if (tr.flFraction < 1.0f)
	{
		const Vector vecToTarget = tr.vecEndPos - vecSrc;
		if (vecToTarget.Length() > 32.0f)
			vecDir = vecToTarget.Normalize();
	}

	CNail::CreateNail(vecSrc, vecDir, m_pPlayer);
#endif

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.1;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
}

void CNailgun::WeaponIdle()
{
	ResetEmptySound();

	m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	int iAnim;
	const float flRand = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 0, 1);
	if (flRand <= 0.5f)
	{
		iAnim = NAILGUN_LONGIDLE;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 36.0f / 8.0f;
	}
	else
	{
		iAnim = NAILGUN_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 36.0f / 35.0f;
	}

	SendWeaponAnim(iAnim);
}

//=========================================================
// Nails ammo pickup
//=========================================================
#ifndef CLIENT_DLL
class CNailsAmmo : public CBasePlayerAmmo
{
public:
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_9mmARclip.mdl");
		CBasePlayerAmmo::Spawn();
	}

	void Precache() override
	{
		PRECACHE_MODEL("models/w_9mmARclip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}

	bool AddAmmo(CBaseEntity* pOther) override
	{
		if (pOther->GiveAmmo(AMMO_NAILS_GIVE, "nails", NAILGUN_MAX_CARRY) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};

LINK_ENTITY_TO_CLASS(ammo_nails, CNailsAmmo);
#endif
