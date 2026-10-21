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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"

LINK_ENTITY_TO_CLASS(weapon_physgun, CPhysgun);

void CPhysgun::Spawn()
{
	Precache();
	m_iId = WEAPON_PHYSGUN;
	SET_MODEL(ENT(pev), MyWModel());
	m_iClip = -1;

	FallInit(); // get ready to fall down.
}


void CPhysgun::Precache()
{
	PRECACHE_MODEL("models/v_physgun.mdl");
	PRECACHE_MODEL(MyWModel());
	PRECACHE_MODEL("models/p_physgun.mdl");

	// no event: the beam is drawn by the client from the player's entity state
	// (AddToFullPack: playerclass bit 0 = firing, aiment = held entity)
}

bool CPhysgun::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 4;
	p->iId = WEAPON_PHYSGUN;
	p->iWeight = CROWBAR_WEIGHT;
	return true;
}



bool CPhysgun::Deploy()
{
#ifndef CLIENT_DLL
	g_engfuncs.pfnSetPhysicsKeyValue(m_pPlayer->edict(), "phg", "0");
#endif
	m_pPlayer->m_pPhysgunEnt = nullptr;

	pev->fixangle = 0;
	pev->v_angle = g_vecZero;

	return DefaultDeploy("models/v_physgun.mdl", "models/p_physgun.mdl", PHYSGUN_DRAW, "egon");
}

void CPhysgun::Holster()
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	//SendWeaponAnim(PHYSGUN_HOLSTER);

	STOP_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "ambience/pulsemachine.wav");

	// let go of the entity (before it stayed flying forever with MOVETYPE_FLY)
	ReleaseEntity(false);
}

void CPhysgun::ReleaseEntity(bool freeze)
{
	CBaseEntity* pHeld = m_pPlayer->m_pPhysgunEnt;

	if (pHeld)
	{
		if (freeze)
		{
			pHeld->pev->movetype = MOVETYPE_NONE;
			pHeld->pev->velocity = g_vecZero;
			pHeld->pev->avelocity = g_vecZero;
		}
		else
		{
			// m_movetype = original movetype + 1 (0 = never touched)
			if (pHeld->m_movetype > MOVETYPE_NONE)
				pHeld->pev->movetype = pHeld->m_movetype - 1;

			pHeld->pev->velocity = pHeld->pev->velocity.Normalize() * (pHeld->pev->velocity.Length() / 3.5f);
		}
	}

	m_pPlayer->m_pPhysgunEnt = nullptr;

	pev->fixangle = 0;
	pev->v_angle = g_vecZero;

#ifndef CLIENT_DLL
	g_engfuncs.pfnSetPhysicsKeyValue(m_pPlayer->edict(), "phg", "0");
#endif
}

void CPhysgun::PrimaryAttack()
{
	if (m_pPlayer->m_pPhysgunEnt)
	{
		return;
	}

	auto pEnt = GetEntity(2048);
	if (pEnt)
	{
		m_pPlayer->m_pPhysgunEnt = pEnt;

		bool isBspModel = m_pPlayer->m_pPhysgunEnt->IsBSPModel();

		m_pPlayer->m_pPhysgunEnt->pev->origin[2] += 0.2f;
		SendWeaponAnim(PHYSGUN_FIRE);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.01f;

		if (m_pPlayer->m_pPhysgunEnt->m_movetype == MOVETYPE_NONE)
			m_pPlayer->m_pPhysgunEnt->m_movetype = m_pPlayer->m_pPhysgunEnt->pev->movetype + 1;

		if (!isBspModel)
		{
			m_pPlayer->m_pPhysgunEnt->pev->movetype = MOVETYPE_FLY;
		}

		m_pPlayer->m_flPhysgunDist = 150.0f;
		m_pPlayer->m_flPhysgunHor = 0.0f;
		m_pPlayer->m_flPhysgunVert = 0.0f;
	}
}

void CPhysgun::SecondaryAttack()
{
	if (m_pPlayer->m_pPhysgunEnt)
	{
		// freeze it where it is
		ReleaseEntity(true);

		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.01f;
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.4f);
	//m_flNextIdleTime = gpGlobals->time + 2.0f;

	SendWeaponAnim(PHYSGUN_FIRE);
}

void CPhysgun::HandleEvent(int mode)
{
	if (!m_pPlayer->m_pPhysgunEnt || (m_pPlayer->pev->button & IN_ATTACK) == 0)
		return;

	if (mode == 1)
	{
		m_pPlayer->m_flPhysgunDist += gpGlobals->frametime * 1000.0f;
	}
	else if (mode == 2)
	{
		m_pPlayer->m_flPhysgunDist -= gpGlobals->frametime * 1000.0f;
	}

	if (m_pPlayer->m_flPhysgunDist < 50.0f)
		m_pPlayer->m_flPhysgunDist = 50.0f;
}

void CPhysgun::ItemThink()
{
	// the held entity was removed (picked up, gibbed, removed by a tool...): clean up
	if (!m_pPlayer->m_pPhysgunEnt && pev->fixangle != 0)
		ReleaseEntity(false);

	if (m_pPlayer->m_pPhysgunEnt && (m_pPlayer->pev->button & IN_ATTACK) != 0)
	{
		TraceResult tr;

		if ((m_pPlayer->pev->button & IN_USE) != 0)
		{
			if (m_pPlayer->pev->button & IN_MOVERIGHT)
			{
				m_pPlayer->m_flPhysgunHor += gpGlobals->frametime * 150.0f;
			}
			else if (m_pPlayer->pev->button & IN_MOVELEFT)
			{
				m_pPlayer->m_flPhysgunHor -= gpGlobals->frametime * 150.0f;
			}
			else if (m_pPlayer->pev->button & IN_FORWARD)
			{
				m_pPlayer->m_flPhysgunVert += gpGlobals->frametime * 150.0f;
			}
			else if (m_pPlayer->pev->button & IN_BACK)
			{
				m_pPlayer->m_flPhysgunVert -= gpGlobals->frametime * 150.0f;
			}

			m_pPlayer->pev->fixangle = 1;

			if (pev->fixangle == 0)
			{
				pev->fixangle = 1;
				pev->v_angle = m_pPlayer->pev->v_angle;
				pev->v_angle.z = 0;
			}

			m_pPlayer->pev->angles = pev->v_angle;

			Vector delta = m_pPlayer->pev->v_angle - pev->v_angle;
			Vector angleadd = g_vecZero;
		//	ALERT(at_console, "%f %f %f\n", delta.x, delta.y / 10.0f, delta.z);

			if (fabs(delta.x) > 0.087f)
			{
				int sign = -1;

				if (delta.x < 0.0f)
					sign = 1;

				angleadd.x = (66.8f * sign) * (gpGlobals->frametime + fabs(delta.x / 10.0f));
			}

			if (fabs(delta.y) > 0.02f)
			{
				int sign = -1;

				if (delta.y < 0.0f)
					sign = 1;

				angleadd.y = (150 * sign) * (gpGlobals->frametime + fabs(delta.y / 10.0f));
			}

			if (m_pPlayer->pev->button & IN_LEFT)
			{
				angleadd.y = gpGlobals->frametime * -300.0f;
			}
			else if (m_pPlayer->pev->button & IN_RIGHT)
			{
				angleadd.y = gpGlobals->frametime * 300.0f;
			}

			// ignore func_pushables: BSP Models should not be rotated
			if (FClassnameIs(m_pPlayer->m_pPhysgunEnt->pev, "func_pushable"))
			{
				angleadd = g_vecZero;
			}

			m_pPlayer->m_pPhysgunEnt->pev->angles = m_pPlayer->m_pPhysgunEnt->pev->angles + angleadd;

			if (m_pPlayer->pev->button & IN_RELOAD)
			{
				m_pPlayer->m_pPhysgunEnt->pev->angles.x = 0.0f;
			}

			NormalizeAngles(m_pPlayer->m_pPhysgunEnt->pev->angles);

#ifndef CLIENT_DLL
			g_engfuncs.pfnSetPhysicsKeyValue(m_pPlayer->edict(), "phg", "1");
#endif

			UTIL_MakeVectors(m_pPlayer->pev->angles + m_pPlayer->pev->punchangle);
		}
		else
		{
			pev->fixangle = 0;
			pev->v_angle = g_vecZero;

#ifndef CLIENT_DLL
			g_engfuncs.pfnSetPhysicsKeyValue(m_pPlayer->edict(), "phg", "0");
#endif		

			m_pPlayer->GetAutoaimVector(0.0f);
		}

		if (m_pPlayer->m_pPhysgunEnt->IsBSPModel())
		{
			Vector absorigin;
			VectorAverage(m_pPlayer->m_pPhysgunEnt->pev->absmax, m_pPlayer->m_pPhysgunEnt->pev->absmin, absorigin);

			Vector diff = (m_pPlayer->pev->origin - absorigin);

			m_pPlayer->m_pPhysgunEnt->pev->velocity = (diff + gpGlobals->v_forward * m_pPlayer->m_flPhysgunDist + gpGlobals->v_right * m_pPlayer->m_flPhysgunHor + gpGlobals->v_up * m_pPlayer->m_flPhysgunVert) * 35;
		}
		else
		{
			Vector diff = (m_pPlayer->pev->origin - m_pPlayer->m_pPhysgunEnt->pev->origin);

			UTIL_TraceLine(m_pPlayer->GetGunPosition(), m_pPlayer->m_pPhysgunEnt->pev->origin + gpGlobals->v_forward * 20, ignore_monsters, m_pPlayer->edict(), &tr);

			if (!strncmp("weapon_", STRING(m_pPlayer->m_pPhysgunEnt->pev->classname), 7) || !strncmp("item_", STRING(m_pPlayer->m_pPhysgunEnt->pev->classname), 5))
				m_pPlayer->m_pPhysgunEnt->pev->velocity = (diff + gpGlobals->v_forward * (m_pPlayer->m_flPhysgunDist * tr.flFraction) + gpGlobals->v_right * m_pPlayer->m_flPhysgunHor + Vector(0, 0, 24)) * 35;
			else
				m_pPlayer->m_pPhysgunEnt->pev->velocity = (diff + gpGlobals->v_forward * (m_pPlayer->m_flPhysgunDist * tr.flFraction) + gpGlobals->v_right * m_pPlayer->m_flPhysgunHor + gpGlobals->v_up * m_pPlayer->m_flPhysgunVert) * 35;
		}
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.01f;
	}
}

static bool IsPickupEntity(CBaseEntity* pEntity)
{
	const char* classname = STRING(pEntity->pev->classname);

	return (strncmp(classname, "item_", 5) == 0) ||
		   (strncmp(classname, "ammo_", 5) == 0) ||
		   (strncmp(classname, "weapon_", 7) == 0);
}

// another player is already holding it
static bool IsHeldByOther(CBasePlayer* pSelf, CBaseEntity* pEntity)
{
#ifndef CLIENT_DLL
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer* pPlayer = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(i));

		if (!pPlayer || pPlayer == pSelf)
			continue;

		if ((CBaseEntity*)pPlayer->m_pPhysgunEnt == pEntity)
			return true;
	}
#endif
	return false;
}

CBaseEntity* CPhysgun::GetEntity(float fldist, bool m_bTakeDamage)
{
#ifdef CLIENT_DLL
	// the client doesn't know the server entities
	return nullptr;
#else
	CBaseEntity* pFound = FindEntity(fldist);

	if (pFound && IsHeldByOther(m_pPlayer, pFound))
		return nullptr;

	return pFound;
#endif
}

CBaseEntity* CPhysgun::FindEntity(float fldist)
{
	TraceResult tr;

	Vector forward = m_pPlayer->GetAutoaimVector(0.0f);
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + forward * fldist;
	CBaseEntity* pEntity = nullptr;

	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr);

	pEntity = CBaseEntity::Instance(tr.pHit);

	if (pEntity)
	{
		if (pEntity->IsBSPModel())
		{
			// only func_pushable can be used
			if (FClassnameIs(pEntity->pev, "func_pushable"))
			{
				return pEntity;
			}

			CBaseEntity* pPickup = nullptr;
			while (pPickup = UTIL_FindEntityInSphere(pPickup, tr.vecEndPos, 2.0f))
			{
				if (pPickup == m_pPlayer)
					continue;

				if (IsPickupEntity(pPickup))
					return pPickup;
			}

			// ignore the rest
			return nullptr;
		}
		else
		{
			// ignore players
			if (pEntity->IsPlayer())
				return nullptr;

			return pEntity;
		}
	}

	pEntity = nullptr;

	while (pEntity = UTIL_FindEntityInSphere(pEntity, tr.vecEndPos, 2.0f))
	{
		if (pEntity == m_pPlayer)
			continue;

		if (pEntity->edict() == INDEXENT(0))
			continue;

		if (pEntity->IsBSPModel())
		{
			if (pEntity->pev->movetype == MOVETYPE_PUSHSTEP)
				return pEntity;
		}
		else
			return pEntity;
	}

	return pEntity;
}

void CPhysgun::WeaponIdle()
{
	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	// the attack button was released: throw it
	if (m_pPlayer->m_pPhysgunEnt)
		ReleaseEntity(false);

	int iAnim;
	float flRand = RANDOM_FLOAT(0, 1);
	if (flRand <= 0.5)
	{
		iAnim = PHYSGUN_IDLE;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	}
	else
	{
		iAnim = PHYSGUN_IDLE_ALT;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	}

	SendWeaponAnim(iAnim);
}
