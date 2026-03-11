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
// Zombie
//=========================================================

// UNDONE: Don't flinch every time you get hit

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"


//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define ZOMBIE_AE_ATTACK_RIGHT 0x01
#define ZOMBIE_AE_ATTACK_LEFT 0x02
#define ZOMBIE_AE_ATTACK_BOTH 0x03

#define ZOMBIE_FLINCH_DELAY 2 // at most one flinch every n secs

class CZombie : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void SetYawSpeed() override;
	int Classify() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	int IgnoreConditions() override;

	float m_flNextFlinch;

	virtual void PainSound() override;
	virtual void AlertSound() override;
	virtual void IdleSound() override;
	virtual void AttackSound();

	//static const char* pAttackSounds[];
	//static const char* pIdleSounds[];
	//static const char* pAlertSounds[];
	//static const char* pPainSounds[];
	static const char* pAttackHitSounds[];
	static const char* pAttackMissSounds[];

	virtual float OneSlashDamage() { return gSkillData.zombieDmgOneSlash; }
	virtual float BothSlashDamage() { return gSkillData.zombieDmgBothSlash; }

	// No range attacks
	bool CheckRangeAttack1(float flDot, float flDist) override { return false; }
	bool CheckRangeAttack2(float flDot, float flDist) override { return false; }
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;

protected:
	void SlashAttack(float dmg, float rightScalar, float forwardScalar, float punchz);
	void ZombieSpawnHelper(const char* modelName, float health);
	void PrecacheSounds();
};

LINK_ENTITY_TO_CLASS(monster_zombie, CZombie);

const char* CZombie::pAttackHitSounds[] =
	{
		"zombie/claw_strike1.wav",
		"zombie/claw_strike2.wav",
		"zombie/claw_strike3.wav",
};

const char* CZombie::pAttackMissSounds[] =
	{
		"zombie/claw_miss1.wav",
		"zombie/claw_miss2.wav",
};

/* const char* CZombie::pAttackSounds[] =
	{
		"zombie/zo_attack1.wav",
		"zombie/zo_attack2.wav",
};

const char* CZombie::pIdleSounds[] =
	{
		"zombie/zo_idle1.wav",
		"zombie/zo_idle2.wav",
		"zombie/zo_idle3.wav",
		"zombie/zo_idle4.wav",
};

const char* CZombie::pAlertSounds[] =
	{
		"zombie/zo_alert10.wav",
		"zombie/zo_alert20.wav",
		"zombie/zo_alert30.wav",
};

const char* CZombie::pPainSounds[] =
	{
		"zombie/zo_pain1.wav",
		"zombie/zo_pain2.wav",
};*/

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CZombie::Classify()
{
	if (m_AltClass)
		return CLASS_PLAYER_ALIEN_ALLY;

	return CLASS_ALIEN_MONSTER;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CZombie::SetYawSpeed()
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

bool CZombie::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	// Take 30% damage from bullets
	if (bitsDamageType == DMG_BULLET)
	{
		Vector vecDir = pev->origin - (pevInflictor->absmin + pevInflictor->absmax) * 0.5;
		vecDir = vecDir.Normalize();
		float flForce = DamageForce(flDamage);
		pev->velocity = pev->velocity + vecDir * flForce;
		flDamage *= 0.3;
	}

	// HACK HACK -- until we fix this.
	if (IsAlive())
		PainSound();
	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

void CZombie::PainSound()
{
	int pitch = 95 + RANDOM_LONG(0, 9);

	SENTENCEG_PlayRndSz(ENT(pev), "ZO_PAIN", 1, ATTN_NORM, 0, pitch);
}

void CZombie::AlertSound()
{
	int pitch = 95 + RANDOM_LONG(0, 9);

	SENTENCEG_PlayRndSz(ENT(pev), "ZO_ALERT", 1, ATTN_NORM, 0, pitch);
}

void CZombie::IdleSound()
{
	int pitch = 95 + RANDOM_LONG(0, 9);

	SENTENCEG_PlayRndSz(ENT(pev), "ZO_IDLE", 1, ATTN_NORM, 0, pitch);
}

void CZombie::AttackSound()
{
	int pitch = 95 + RANDOM_LONG(0, 9);

	SENTENCEG_PlayRndSz(ENT(pev), "ZO_ATTACK", 1, ATTN_NORM, 0, pitch);
}


//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CZombie::SlashAttack(float dmg, float rightScalar, float forwardScalar, float punchz)
{
	CBaseEntity* pHurt = CheckTraceHullAttack(70, dmg, DMG_SLASH);
	if (pHurt)
	{
		if (pHurt->pev->flags & (FL_MONSTER | FL_CLIENT))
		{
			if (punchz)
			{
				pHurt->pev->punchangle.z = punchz;
			}
			pHurt->pev->punchangle.x = 5;
			pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_right * rightScalar + gpGlobals->v_forward * forwardScalar;
		}
		// Play a random attack hit sound
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, RANDOM_SOUND_ARRAY(pAttackHitSounds), 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5, 5));
	}
	else // Play a random attack miss sound
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, RANDOM_SOUND_ARRAY(pAttackMissSounds), 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5, 5));

	if (RANDOM_LONG(0, 1))
		AttackSound();
}

void CZombie::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case ZOMBIE_AE_ATTACK_RIGHT:
		SlashAttack(OneSlashDamage(), -100, 0, -18);
		break;

	case ZOMBIE_AE_ATTACK_LEFT:
		SlashAttack(OneSlashDamage(), 100, 0, 18);
		break;

	case ZOMBIE_AE_ATTACK_BOTH:
		SlashAttack(BothSlashDamage(), 0, -100, 0);
		break;

	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CZombie::Spawn()
{
	Precache();
	ZombieSpawnHelper("models/zombie.mdl", gSkillData.zombieHealth);
}

void CZombie::ZombieSpawnHelper(const char* modelName, float health)
{
	SET_MODEL(ENT(pev), modelName);
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_GREEN;
	pev->health = (health);
	pev->view_ofs = VEC_VIEW; // position of the eyes relative to monster's origin.
	m_flFieldOfView = 0.5;	  // indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState = MONSTERSTATE_NONE;
	m_afCapability = bits_CAP_DOORS_GROUP;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CZombie::Precache()
{
	PRECACHE_MODEL("models/zombie.mdl");
	PrecacheSounds();
}

void CZombie::PrecacheSounds()
{
	PRECACHE_SOUND_ARRAY(pAttackHitSounds);
	PRECACHE_SOUND_ARRAY(pAttackMissSounds);

	// Removed, Now are sentences
	// PRECACHE_SOUND_ARRAY(pAttackSounds);
	// PRECACHE_SOUND_ARRAY(pIdleSounds);
	// PRECACHE_SOUND_ARRAY(pAlertSounds);
	// PRECACHE_SOUND_ARRAY(pPainSounds);
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================



int CZombie::IgnoreConditions()
{
	int iIgnore = CBaseMonster::IgnoreConditions();

	if ((m_Activity == ACT_MELEE_ATTACK1) || (m_Activity == ACT_MELEE_ATTACK1))
	{
#if 0
		if (pev->health < 20)
			iIgnore |= (bits_COND_LIGHT_DAMAGE|bits_COND_HEAVY_DAMAGE);
		else
#endif
		if (m_flNextFlinch >= gpGlobals->time)
			iIgnore |= (bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE);
	}

	if ((m_Activity == ACT_SMALL_FLINCH) || (m_Activity == ACT_BIG_FLINCH))
	{
		if (m_flNextFlinch < gpGlobals->time)
			m_flNextFlinch = gpGlobals->time + ZOMBIE_FLINCH_DELAY;
	}

	return iIgnore;
}


class CZombieBarney : public CZombie
{
public:
	void Spawn() override;
	void Precache() override;

	float OneSlashDamage() { return gSkillData.zombieBarneyDmgOneSlash; }
	float BothSlashDamage() { return gSkillData.zombieBarneyDmgBothSlash; }
};

LINK_ENTITY_TO_CLASS(monster_zombie_barney, CZombieBarney);

void CZombieBarney::Spawn()
{
	Precache();
	ZombieSpawnHelper("models/zombie_barney.mdl", gSkillData.zombieBarneyHealth);
}

void CZombieBarney::Precache()
{
	PRECACHE_MODEL("models/zombie_barney.mdl");
	PrecacheSounds();
}

class CZombieSoldier : public CZombie
{
public:
	void Spawn() override;
	void Precache() override;

	float OneSlashDamage() { return gSkillData.zombieSoldierDmgOneSlash; }
	float BothSlashDamage() { return gSkillData.zombieSoldierDmgBothSlash; }
};

LINK_ENTITY_TO_CLASS(monster_zombie_soldier, CZombieSoldier);

void CZombieSoldier::Spawn()
{
	Precache();
	ZombieSpawnHelper("models/zombie_soldier.mdl", gSkillData.zombieSoldierHealth);
}

void CZombieSoldier::Precache()
{
	PRECACHE_MODEL("models/zombie_soldier.mdl");
	PrecacheSounds();
}

//=========================================================
// DEAD HGRUNT ZOMBIE PROP
//=========================================================
class CDeadZombieSoldier : public CBaseMonster
{
public:
	void Spawn() override;
	int Classify() override { return CLASS_ALIEN_MONSTER; }

	bool KeyValue(KeyValueData* pkvd) override;

	int m_iPose; // which sequence to display	-- temporary, don't need to save
	static char* m_szPoses[2];
};

char* CDeadZombieSoldier::m_szPoses[] = {"dead_on_stomach", "dead_on_back"};

bool CDeadZombieSoldier::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "pose"))
	{
		m_iPose = atoi(pkvd->szValue);
		return true;
	}

	return CBaseMonster::KeyValue(pkvd);
}

LINK_ENTITY_TO_CLASS(monster_zombie_soldier_dead, CDeadZombieSoldier);

//=========================================================
// ********** DeadZombieSoldier SPAWN **********
//=========================================================
void CDeadZombieSoldier::Spawn()
{
	PRECACHE_MODEL("models/zombie_soldier.mdl");
	SET_MODEL(ENT(pev), "models/zombie_soldier.mdl");

	pev->effects = 0;
	pev->yaw_speed = 8;
	pev->sequence = 0;
	m_bloodColor = BLOOD_COLOR_RED;

	pev->sequence = LookupSequence(m_szPoses[m_iPose]);

	if (pev->sequence == -1)
	{
		ALERT(at_console, "Dead hgrunt with bad pose\n");
	}

	// Corpses have less health
	pev->health = 8;

	MonsterInitDead();
}
