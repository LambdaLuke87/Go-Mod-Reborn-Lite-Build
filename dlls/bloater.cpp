/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// Bloater
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "npc_sounds.h"


//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define BLOATER_AE_ATTACK_MELEE1 0x01


class CBloater : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void SetYawSpeed() override;
	int Classify() override;
	const char* DefaultDisplayName() { return "Flocking Floater"; }
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;

	void PainSound() override;
	void AlertSound() override;
	void IdleSound() override;
	void AttackSnd();

	// No range attacks
	bool CheckRangeAttack1(float flDot, float flDist) override { return false; }
	bool CheckRangeAttack2(float flDot, float flDist) override { return false; }
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
};

LINK_ENTITY_TO_CLASS(monster_bloater, CBloater);

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CBloater::Classify()
{
	return CLASS_ALIEN_MONSTER;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CBloater::SetYawSpeed()
{
	int ys;

	ys = 120;

#if 0
	switch ( m_Activity )
	{
	}
#endif

	pev->yaw_speed = ys;
}

bool CBloater::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	BasePainSound();
	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

void CBloater::PainSound()
{
#if 0	
	int pitch = 95 + RANDOM_LONG(0,9);

	if (RANDOM_LONG(0, 5) < 2) // the original switch had silent cases
		NPCSounds_Emit(this, "monster_bloater", "pain", CHAN_VOICE, 1.0, ATTN_NORM, pitch);
#endif
}

void CBloater::AlertSound()
{
#if 0
	int pitch = 95 + RANDOM_LONG(0,9);

	NPCSounds_Emit(this, "monster_bloater", "alert", CHAN_VOICE, 1.0, ATTN_NORM, pitch);
#endif
}

void CBloater::IdleSound()
{
#if 0
	int pitch = 95 + RANDOM_LONG(0,9);

	NPCSounds_Emit(this, "monster_bloater", "idle", CHAN_VOICE, 1.0, ATTN_NORM, pitch);
#endif
}

void CBloater::AttackSnd()
{
#if 0
	int pitch = 95 + RANDOM_LONG(0,9);

	NPCSounds_Emit(this, "monster_bloater", "attack", CHAN_VOICE, 1.0, ATTN_NORM, pitch);
#endif
}


//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CBloater::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case BLOATER_AE_ATTACK_MELEE1:
	{
		// do stuff for this event.
		AttackSnd();
	}
	break;

	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CBloater::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/floater.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_FLY;
	pev->spawnflags |= FL_FLY;
	m_bloodColor = BLOOD_COLOR_GREEN;
	pev->health = 40;
	pev->view_ofs = VEC_VIEW; // position of the eyes relative to monster's origin.
	m_flFieldOfView = 0.5;	  // indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState = MONSTERSTATE_NONE;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CBloater::Precache()
{
	PRECACHE_MODEL("models/floater.mdl");
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
