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

	m_usPhysGun = PRECACHE_EVENT(1, "events/physgun.sc");
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
	return DefaultDeploy("models/v_physgun.mdl", "models/p_physgun.mdl", PHYSGUN_DRAW, "gauss");
}

void CPhysgun::Holster()
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim(PHYSGUN_HOLSTER);

	STOP_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "ambience/pulsemachine.wav");
}

void CPhysgun::PrimaryAttack()
{
	if (m_pCurrentEntity)
	{
		TraceResult tr;

		m_pPlayer->GetAutoaimVector(0.0f);

		if (m_pCurrentEntity->IsBSPModel())
		{
			Vector absorigin;
			VectorAverage(m_pCurrentEntity->pev->absmax, m_pCurrentEntity->pev->absmin, absorigin);

			Vector normal = (m_pPlayer->pev->origin - absorigin).Normalize();

			m_pCurrentEntity->pev->velocity = (normal + gpGlobals->v_forward * 150) * 35;
		}
		else
		{
			Vector diff = (m_pPlayer->pev->origin - m_pCurrentEntity->pev->origin);

			UTIL_TraceLine(m_pPlayer->GetGunPosition(), m_pCurrentEntity->pev->origin + gpGlobals->v_forward * 20, ignore_monsters, m_pPlayer->edict(), &tr);

			if (!strncmp("weapon_", STRING(m_pCurrentEntity->pev->classname), 7) || !strncmp("item_", STRING(m_pCurrentEntity->pev->classname), 5))
				m_pCurrentEntity->pev->velocity = (diff + gpGlobals->v_forward * (150 + tr.flFraction) + Vector(0, 0, 24)) * 35;
			else
				m_pCurrentEntity->pev->velocity = (diff + gpGlobals->v_forward * (150 * tr.flFraction)) * 35;
		}
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.01f;
	}
	else
	{
		m_pCurrentEntity = GetEntity(2048);
		if (m_pCurrentEntity)
		{
			bool isBspModel = m_pCurrentEntity->IsBSPModel();

			m_pCurrentEntity->pev->origin[2] += 0.2f;
			SendWeaponAnim(PHYSGUN_SPIN);
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.01f;

			if (m_pCurrentEntity->m_movetype == MOVETYPE_NONE)
				m_pCurrentEntity->m_movetype = m_pCurrentEntity->pev->movetype + 1;
		
			m_pCurrentEntity->pev->movetype = MOVETYPE_FLY;

			PLAYBACK_EVENT_FULL(FEV_SERVER, m_pPlayer->edict(), m_usPhysGun,
				0.0, g_vecZero, g_vecZero, 0.0f, 0.0f, ENTINDEX(m_pCurrentEntity->edict()),
				0, isBspModel ? 1 : 0, false);
		}
	}
}

void CPhysgun::SecondaryAttack()
{
	if (m_pCurrentEntity)
	{
		m_pCurrentEntity->pev->movetype = MOVETYPE_NONE;

		m_pCurrentEntity->pev->velocity = g_vecZero;
		m_pCurrentEntity = nullptr;

		PLAYBACK_EVENT_FULL(FEV_SERVER, m_pPlayer->edict(), m_usPhysGun,
			0.0, g_vecZero, g_vecZero, 0.0f, 0.0f, 0,
			0, 0, true);

		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.01f;
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.4f);
	//m_flNextIdleTime = gpGlobals->time + 2.0f;

	SendWeaponAnim(PHYSGUN_FIRE);
}

void CPhysgun::ItemPostFrame()
{
	CBasePlayerWeapon::ItemPostFrame();
}

CBaseEntity* CPhysgun::GetEntity(float fldist, bool m_bTakeDamage)
{
	TraceResult tr;

	Vector forward = m_pPlayer->GetAutoaimVector(0.0f);
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + forward * fldist;
	CBaseEntity* pEntity = nullptr;

	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr);

	if (!tr.pHit)
		pEntity = UTIL_FindEntityInSphere(pEntity, tr.vecEndPos, 2.0f);
	else
		pEntity = CBaseEntity::Instance(tr.pHit);

	if (m_bTakeDamage)
	{
		if (!pEntity)
			return nullptr;

		if ((pEntity->IsBSPModel() && (pEntity->pev->movetype == MOVETYPE_PUSHSTEP || pEntity->pev->takedamage == DAMAGE_YES)))
		{
			return pEntity;
		}
	}
	else
	{
		if (!pEntity || (pEntity->IsBSPModel() && pEntity->pev->movetype != MOVETYPE_PUSHSTEP))
			pEntity = UTIL_FindEntityInSphere(pEntity, tr.vecEndPos, 2.0f);

		if (!pEntity || (pEntity->IsBSPModel() && pEntity->pev->movetype != MOVETYPE_PUSHSTEP))
			return nullptr;
	}
	if (pEntity == m_pPlayer)
		return nullptr;

	return pEntity;
}

void CPhysgun::WeaponIdle()
{	
	//if (m_flNextIdleTime > gpGlobals->time)
	//	return;

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if (m_pCurrentEntity)
	{
		if (m_pCurrentEntity->m_movetype > MOVETYPE_NONE)
			m_pCurrentEntity->pev->movetype = m_pCurrentEntity->m_movetype - 1;

		m_pCurrentEntity->pev->velocity = m_pCurrentEntity->pev->velocity.Normalize() * (m_pCurrentEntity->pev->velocity.Length() / 3.5f);

		PLAYBACK_EVENT_FULL(FEV_SERVER, m_pPlayer->edict(), m_usPhysGun,
			0.0, g_vecZero, g_vecZero, 0.0f, 0.0f, 0,
			0, 0, true);

		m_pCurrentEntity = nullptr;
	}

	int iAnim;
	float flRand = RANDOM_FLOAT(0, 1);
	if (flRand <= 0.5)
	{
		iAnim = PHYSGUN_IDLE;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	}
	else
	{
		iAnim = PHYSGUN_IDLE2;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	}

	SendWeaponAnim(iAnim);
}
