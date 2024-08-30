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

//=========================================================
// Gonome.cpp
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "player.h"
#include "decals.h"
#include "animation.h"
#include "studio.h"
#include "game.h"

//=========================================================
// Monster's Anim Events Go Here
//=========================================================

#define GONOME_AE_SLASH_RIGHT (1)
#define GONOME_AE_SLASH_LEFT (2)
#define GONOME_AE_BITE1 (19)
#define GONOME_AE_BITE2 (20)
#define GONOME_AE_BITE3 (21)
#define GONOME_AE_BITE4 (22)

#define GONOME_SCRIPT_EVENT_SOUND (1011)
#define ZOMBIE_AE_ATTACK_GUTS_GRAB 0x03
#define ZOMBIE_AE_ATTACK_GUTS_THROW 4
#define GONOME_MELEE_ATTACK_RADIUS 70
#define ZOMBIE_FLINCH_DELAY 2 // at most one flinch every n secs

class COFGonomeGuts : public CBaseEntity
{
public:
	using BaseClass = CBaseEntity;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;

	static TYPEDESCRIPTION m_SaveData[];

	void Spawn() override;

	void Touch(CBaseEntity* pOther) override;

	void EXPORT Animate();

	static void Shoot(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity);

	static COFGonomeGuts* GonomeGutsCreate(const Vector& origin);

	void Launch(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity);

	int m_maxFrame;
};

TYPEDESCRIPTION COFGonomeGuts::m_SaveData[] =
	{
		DEFINE_FIELD(COFGonomeGuts, m_maxFrame, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(COFGonomeGuts, COFGonomeGuts::BaseClass);

LINK_ENTITY_TO_CLASS(gonomeguts, COFGonomeGuts);

void COFGonomeGuts::Spawn()
{
	pev->movetype = MOVETYPE_FLY;
	pev->classname = MAKE_STRING("gonomeguts");

	pev->solid = SOLID_BBOX;
	pev->rendermode = kRenderTransAlpha;
	pev->renderamt = 255;

	// TODO: probably shouldn't be assinging to x every time
	if (g_Language == LANGUAGE_GERMAN)
	{
		SET_MODEL(edict(), "sprites/bigspit.spr");
		pev->rendercolor.x = 0;
		pev->rendercolor.x = 255;
		pev->rendercolor.x = 0;
	}
	else
	{
		SET_MODEL(edict(), "sprites/bigspit.spr");
		pev->rendercolor.x = 128;
		pev->rendercolor.x = 32;
		pev->rendercolor.x = 128;
	}

	pev->frame = 0;
	pev->scale = 0.5;

	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	m_maxFrame = static_cast<int>(MODEL_FRAMES(pev->modelindex) - 1);
}

void COFGonomeGuts::Touch(CBaseEntity* pOther)
{
	// splat sound
	const auto iPitch = RANDOM_FLOAT(90, 110);

	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "bullchicken/bc_acid1.wav", 1, ATTN_NORM, 0, iPitch);

	switch (RANDOM_LONG(0, 1))
	{
	case 0:
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "bullchicken/bc_spithit1.wav", 1, ATTN_NORM, 0, iPitch);
		break;
	case 1:
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "bullchicken/bc_spithit2.wav", 1, ATTN_NORM, 0, iPitch);
		break;
	}

	if (0 == pOther->pev->takedamage)
	{
		TraceResult tr;
		// make a splat on the wall
		UTIL_TraceLine(pev->origin, pev->origin + pev->velocity * 10, dont_ignore_monsters, ENT(pev), &tr);
		UTIL_BloodDecalTrace(&tr, BLOOD_COLOR_RED);
		UTIL_BloodDrips(tr.vecEndPos, tr.vecPlaneNormal, BLOOD_COLOR_RED, 35);
	}
	else
	{
		pOther->TakeDamage(pev, pev, gSkillData.gonomeDmgGuts, DMG_GENERIC);
	}

	SetThink(&COFGonomeGuts::SUB_Remove);
	pev->nextthink = gpGlobals->time;
}

void COFGonomeGuts::Animate()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (0 != pev->frame++)
	{
		if (pev->frame > m_maxFrame)
		{
			pev->frame = 0;
		}
	}
}

void COFGonomeGuts::Shoot(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity)
{
	auto pGuts = GetClassPtr<COFGonomeGuts>(nullptr);
	pGuts->Spawn();

	UTIL_SetOrigin(pGuts->pev, vecStart);
	pGuts->pev->velocity = vecVelocity;
	pGuts->pev->owner = ENT(pevOwner);

	if (pGuts->m_maxFrame > 0)
	{
		pGuts->SetThink(&COFGonomeGuts::Animate);
		pGuts->pev->nextthink = gpGlobals->time + 0.1;
	}
}

COFGonomeGuts* COFGonomeGuts::GonomeGutsCreate(const Vector& origin)
{
	auto pGuts = GetClassPtr<COFGonomeGuts>(nullptr);
	pGuts->Spawn();

	pGuts->pev->origin = origin;

	return pGuts;
}

void COFGonomeGuts::Launch(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity)
{
	UTIL_SetOrigin(pev, vecStart);
	pev->velocity = vecVelocity;
	pev->owner = ENT(pevOwner);

	SetThink(&COFGonomeGuts::Animate);
	pev->nextthink = gpGlobals->time + 0.1;
}

//=========================================================
// CGonome
//=========================================================
class CGonome : public CBaseMonster
{
public:
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;

	static TYPEDESCRIPTION m_SaveData[];

	void Spawn() override;
	void Precache() override;
	void SetYawSpeed() override;
	int Classify() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	int IgnoreConditions() override;

	void IdleSound() override;
	void AlertSound() override;
	void PainSound() override;
	void DeathSound() override;

	//static const char* pIdleSounds[];
	//static const char* pAlertSounds[];
	//static const char* pPainSounds[];
	static const char* pAttackHitSounds[];
	static const char* pAttackMissSounds[];
	//static const char* pDeathSounds[];

	bool CheckMeleeAttack2(float flDot, float flDist) override;
	bool CheckRangeAttack1(float flDot, float flDist) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;

	Schedule_t* GetSchedule();
	Schedule_t* GetScheduleOfType(int Type);

	void RunTask(Task_t* pTask) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;

	void SetActivity(Activity NewActivity) override;

	CUSTOM_SCHEDULES

	float m_flNextFlinch;
	float m_flNextThrowTime;

	// TODO: needs to be EHANDLE, save/restored or a save during a windup will cause problems
	COFGonomeGuts* m_pGonomeGuts;

protected:
	int GonomeLookupActivity(void* pmodel, int activity);

	bool m_meleeAttack2;
	bool m_playedAttackSound;

	bool gonnaAttack1;
};

TYPEDESCRIPTION CGonome::m_SaveData[] =
	{
		DEFINE_FIELD(CGonome, m_flNextFlinch, FIELD_TIME),
		DEFINE_FIELD(CGonome, m_flNextThrowTime, FIELD_TIME),
};

IMPLEMENT_SAVERESTORE(CGonome, CBaseMonster)


LINK_ENTITY_TO_CLASS(monster_gonome, CGonome)


/*const char* CGonome::pPainSounds[] = {
	"gonome/gonome_pain1.wav",
	"gonome/gonome_pain2.wav",
	"gonome/gonome_pain3.wav",
	"gonome/gonome_pain4.wav"};

const char* CGonome::pAlertSounds[] =
	{
		"zombie/zo_alert10.wav",
		"zombie/zo_alert20.wav",
		"zombie/zo_alert30.wav",
}; 

const char* CGonome::pIdleSounds[] = {
	"gonome/gonome_idle1.wav",
	"gonome/gonome_idle2.wav",
	"gonome/gonome_idle3.wav"};

const char* CGonome::pDeathSounds[] = {
	"gonome/gonome_death2.wav",
	"gonome/gonome_death3.wav",
	"gonome/gonome_death4.wav"};*/

const char* CGonome::pAttackHitSounds[] =
	{
		"zombie/claw_strike1.wav",
		"zombie/claw_strike2.wav",
		"zombie/claw_strike3.wav",
};

const char* CGonome::pAttackMissSounds[] =
	{
		"zombie/claw_miss1.wav",
		"zombie/claw_miss2.wav",
};

/*
 * Hack to ignore activity weights when choosing melee attack animation
 */
int CGonome::GonomeLookupActivity(void* pmodel, int activity)
{
	studiohdr_t* pstudiohdr;

	pstudiohdr = (studiohdr_t*)pmodel;
	if (!pstudiohdr)
		return 0;

	mstudioseqdesc_t* pseqdesc;

	pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex);

	int sameActivityNum = 0;
	for (int i = 0; i < pstudiohdr->numseq && sameActivityNum < 2; i++)
	{
		if (pseqdesc[i].activity == activity)
		{
			sameActivityNum++;
			if (sameActivityNum == 1 && gonnaAttack1)
			{
				return i;
			}
			else if (sameActivityNum == 2)
			{
				return i;
			}
		}
	}

	return ACTIVITY_NOT_AVAILABLE;
}

void CGonome::SetActivity(Activity NewActivity)
{

	if (NewActivity != ACT_RANGE_ATTACK1 && m_pGonomeGuts)
	{
		UTIL_Remove(m_pGonomeGuts);
		m_pGonomeGuts = nullptr;
	}

	Activity OldActivity = m_Activity;
	int iSequence = ACTIVITY_NOT_AVAILABLE;

	if (NewActivity != ACT_RANGE_ATTACK1)
	{
		UTIL_Remove(m_pGonomeGuts);
		m_pGonomeGuts = nullptr;
	}
	if (NewActivity == ACT_MELEE_ATTACK1 && m_hEnemy != 0)
	{
		// special melee animations
		if ((pev->origin - m_hEnemy->pev->origin).Length2D() >= 48)
		{
			m_meleeAttack2 = false;
			iSequence = LookupSequence("attack1");
		}
		else
		{
			m_meleeAttack2 = true;
			iSequence = LookupSequence("attack2");
		}
	}
	else
	{
		if (NewActivity == ACT_RUN && m_hEnemy != 0)
		{
			// special run animations
			if ((pev->origin - m_hEnemy->pev->origin).Length2D() <= 512)
			{
				iSequence = LookupSequence("runshort");
			}
			else
			{
				iSequence = LookupSequence("runlong");
			}
		}
		else
		{
			iSequence = LookupActivity(NewActivity);
		}
	}

	m_Activity = NewActivity; // Go ahead and set this so it doesn't keep trying when the anim is not present

	// In case someone calls this with something other than the ideal activity
	m_IdealActivity = m_Activity;

	// Set to the desired anim, or default anim if the desired is not present
	if (iSequence > ACTIVITY_NOT_AVAILABLE)
	{
		if (pev->sequence != iSequence || !m_fSequenceLoops)
		{
			// don't reset frame between walk and run
			if (!(OldActivity == ACT_WALK || OldActivity == ACT_RUN) || !(NewActivity == ACT_WALK || NewActivity == ACT_RUN))
				pev->frame = 0;
		}

		pev->sequence = iSequence; // Set to the reset anim (if it's there)
		ResetSequenceInfo();
		SetYawSpeed();
	}
	else
	{
		// Not available try to get default anim
		ALERT(at_aiconsole, "%s has no sequence for act:%d\n", STRING(pev->classname), NewActivity);
		pev->sequence = 0; // Set to the reset anim (if it's there)
	}
}

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CGonome::Classify()
{
	if (m_AltClass)
		return CLASS_PLAYER_ALIEN_ALLY;
		
	return CLASS_ALIEN_MONSTER;
}

//=========================================================
// TakeDamage - overridden for gonome so we can keep track
// of how much time has passed since it was last injured
//=========================================================
bool CGonome::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	// Take 15% damage from bullets
	if (bitsDamageType == DMG_BULLET)
	{
		Vector vecDir = pev->origin - (pevInflictor->absmin + pevInflictor->absmax) * 0.5;
		vecDir = vecDir.Normalize();
		float flForce = DamageForce(flDamage);
		pev->velocity = pev->velocity + vecDir * flForce;
		flDamage *= 0.15;
	}

	// HACK HACK -- until we fix this.
	if (IsAlive())
		PainSound();

	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}


//=========================================================
// CheckRangeAttack1
//=========================================================
bool CGonome::CheckRangeAttack1(float flDot, float flDist)
{
	if (flDist < 256.0)
		return false;

	if (IsMoving() && flDist >= 512.0)
	{
		return false;
	}

	if (flDist > 64.0 && flDist <= 784.0 && flDot >= 0.5 && gpGlobals->time >= m_flNextThrowTime)
	{
		if (!m_hEnemy || (fabs(pev->origin.z - m_hEnemy->pev->origin.z) <= 256.0))
		{
			if (IsMoving())
			{
				m_flNextThrowTime = gpGlobals->time + 5.0;
			}
			else
			{
				m_flNextThrowTime = gpGlobals->time + 0.5;
			}

			return true;
		}
	}

	return false;
}

//=========================================================
// CheckMeleeAttack2 - both gonome's melee attacks are ACT_MELEE_ATTACK1
//=========================================================
bool CGonome::CheckMeleeAttack2(float flDot, float flDist)
{
	return false;
}


//=========================================================
// IdleSound
//=========================================================
#define GONOME_ATTN_IDLE (float)1.5

void CGonome::IdleSound()
{
	int pitch = 100 + RANDOM_LONG(-5, 5);

	// Play a random idle sound
	SENTENCEG_PlayRndSz(ENT(pev), "GO_IDLE", 1, ATTN_NORM, 0, pitch);
}

//=========================================================
// PainSound
//=========================================================
void CGonome::PainSound()
{
	int pitch = 95 + RANDOM_LONG(0, 9);

	if (RANDOM_LONG(0, 5) < 2)
		SENTENCEG_PlayRndSz(ENT(pev), "GO_PAIN", 1, ATTN_NORM, 0, pitch);
}

//=========================================================
// AlertSound
//=========================================================
void CGonome::AlertSound()
{
	int pitch = 95 + RANDOM_LONG(0, 9);

	SENTENCEG_PlayRndSz(ENT(pev), "ZO_ALERT", 1, ATTN_NORM, 0, pitch);
}

//=========================================================
// DeathSound
//=========================================================
void CGonome::DeathSound()
{
	SENTENCEG_PlayRndSz(ENT(pev), "GO_DEATH", 1, ATTN_NORM, 0, PITCH_NORM);
}


//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CGonome::SetYawSpeed()
{
	pev->yaw_speed = 120;
}
//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CGonome::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case GONOME_SCRIPT_EVENT_SOUND:
		if (m_Activity != ACT_MELEE_ATTACK1)
			EMIT_SOUND(ENT(pev), CHAN_BODY, pEvent->options, 1, ATTN_NORM);
		break;

	case ZOMBIE_AE_ATTACK_GUTS_GRAB:
	{
		// Only if we still have an enemy at this point
		if (m_hEnemy)
		{
			Vector vecGutsPos, vecGutsAngles;
			GetAttachment(0, vecGutsPos, vecGutsAngles);

			if (!m_pGonomeGuts)
			{
				m_pGonomeGuts = COFGonomeGuts::GonomeGutsCreate(vecGutsPos);
			}

			// Attach to hand for throwing
			m_pGonomeGuts->pev->skin = entindex();
			m_pGonomeGuts->pev->body = 1;
			m_pGonomeGuts->pev->aiment = edict();
			m_pGonomeGuts->pev->movetype = MOVETYPE_FOLLOW;

			auto direction = (m_hEnemy->pev->origin + m_hEnemy->pev->view_ofs - vecGutsPos).Normalize();

			direction = direction + Vector(
										RANDOM_FLOAT(-0.05, 0.05),
										RANDOM_FLOAT(-0.05, 0.05),
										RANDOM_FLOAT(-0.05, 0));

			UTIL_BloodDrips(vecGutsPos, direction, BLOOD_COLOR_RED, 35);
		}
	}
	break;

	case ZOMBIE_AE_ATTACK_GUTS_THROW:
	{
		// Note: this check wasn't in the original. If an enemy dies during gut throw windup, this can be null and crash
		if (m_hEnemy)
		{
			Vector vecGutsPos, vecGutsAngles;
			GetAttachment(0, vecGutsPos, vecGutsAngles);

			UTIL_MakeVectors(pev->angles);

			if (!m_pGonomeGuts)
			{
				m_pGonomeGuts = COFGonomeGuts::GonomeGutsCreate(vecGutsPos);
			}

			auto direction = (m_hEnemy->pev->origin + m_hEnemy->pev->view_ofs - vecGutsPos).Normalize();

			direction = direction + Vector(
										RANDOM_FLOAT(-0.05, 0.05),
										RANDOM_FLOAT(-0.05, 0.05),
										RANDOM_FLOAT(-0.05, 0));

			UTIL_BloodDrips(vecGutsPos, direction, BLOOD_COLOR_RED, 35);

			// Detach from owner
			m_pGonomeGuts->pev->skin = 0;
			m_pGonomeGuts->pev->body = 0;
			m_pGonomeGuts->pev->aiment = nullptr;
			m_pGonomeGuts->pev->movetype = MOVETYPE_FLY;

			m_pGonomeGuts->Launch(pev, vecGutsPos, direction * 900);
		}
		else
		{
			UTIL_Remove(m_pGonomeGuts);
		}

		m_pGonomeGuts = nullptr;
	}
	break;

	case GONOME_AE_SLASH_LEFT:
	{
		CBaseEntity* pHurt = CheckTraceHullAttack(GONOME_MELEE_ATTACK_RADIUS, gSkillData.gonomeDmgOneSlash, DMG_SLASH);
		if (pHurt)
		{
			if (FBitSet(pHurt->pev->flags, FL_MONSTER | FL_CLIENT))
			{
				pHurt->pev->punchangle.z = 9;
				pHurt->pev->punchangle.x = 5;
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_right * 25;
			}
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, RANDOM_SOUND_ARRAY(pAttackHitSounds), 1, ATTN_NORM, 0, 100 + RANDOM_LONG(-5, 5));
		}
		else
		{
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, RANDOM_SOUND_ARRAY(pAttackMissSounds), 1, ATTN_NORM, 0, 100 + RANDOM_LONG(-5, 5));
		}
	}
	break;

	case GONOME_AE_SLASH_RIGHT:
	{
		CBaseEntity* pHurt = CheckTraceHullAttack(GONOME_MELEE_ATTACK_RADIUS, gSkillData.gonomeDmgOneSlash, DMG_SLASH);
		if (pHurt)
		{
			if (FBitSet(pHurt->pev->flags, FL_MONSTER | FL_CLIENT))
			{
				pHurt->pev->punchangle.z = -9;
				pHurt->pev->punchangle.x = 5;
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_right * -25;
			}
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, RANDOM_SOUND_ARRAY(pAttackHitSounds), 1, ATTN_NORM, 0, 100 + RANDOM_LONG(-5, 5));
		}
		else
		{
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, RANDOM_SOUND_ARRAY(pAttackMissSounds), 1, ATTN_NORM, 0, 100 + RANDOM_LONG(-5, 5));
		}
	}
	break;

	case GONOME_AE_BITE1:
	case GONOME_AE_BITE2:
	case GONOME_AE_BITE3:
	case GONOME_AE_BITE4:
	{
		int iPitch;

		CBaseEntity* pHurt = CheckTraceHullAttack(GONOME_MELEE_ATTACK_RADIUS, gSkillData.gonomeDmgOneBite, DMG_SLASH);

		if (pHurt)
		{
			// croonchy bite sound
			iPitch = RANDOM_FLOAT(90, 110);
			switch (RANDOM_LONG(0, 1))
			{
			case 0:
				EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "bullchicken/bc_bite2.wav", 1, ATTN_NORM, 0, iPitch);
				break;
			case 1:
				EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "bullchicken/bc_bite3.wav", 1, ATTN_NORM, 0, iPitch);
				break;
			}

			if (FBitSet(pHurt->pev->flags, FL_MONSTER | FL_CLIENT))
			{
				if (pEvent->event == GONOME_AE_BITE4)
				{
					pHurt->pev->punchangle.x = 15;
					pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_forward * 75;
				}
				else
				{
					pHurt->pev->punchangle.x = 9;
					pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_forward * 25;
				}
			}
		}
	}
	break;



	default:
		CBaseMonster::HandleAnimEvent(pEvent);
	}
}

#define GONOME_FLINCH_DELAY 2

int CGonome::IgnoreConditions()
{
	int iIgnore = CBaseMonster::IgnoreConditions();

	if (m_Activity == ACT_RANGE_ATTACK1)
	{
		iIgnore |= bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_ENEMY_TOOFAR | bits_COND_ENEMY_OCCLUDED;
	}
	else if ((m_Activity == ACT_MELEE_ATTACK1) || (m_Activity == ACT_MELEE_ATTACK1))
	{
#if 0
		if (pev->health < 20)
			iIgnore |= (bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE);
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

//=========================================================
// Spawn
//=========================================================
void CGonome::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/gonome.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_GREEN;
	pev->effects = 0;
	pev->health = gSkillData.gonomeHealth;
	m_flFieldOfView = 0.2; // indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState = MONSTERSTATE_NONE;

	m_flNextThrowTime = gpGlobals->time;
	m_pGonomeGuts = nullptr;

	if (m_bnpc_allied == true)
		m_AltClass = true;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CGonome::Precache()
{
	PRECACHE_MODEL("models/gonome.mdl");

	PRECACHE_MODEL("sprites/bigspit.spr"); // spit projectile.

	PRECACHE_SOUND("zombie/claw_miss2.wav"); // because we use the basemonster SWIPE animation event

	//PRECACHE_SOUND("gonome/gonome_eat.wav");

	//PRECACHE_SOUND("gonome/gonome_jumpattack.wav");
	//PRECACHE_SOUND("gonome/gonome_melee1.wav");
	//PRECACHE_SOUND("gonome/gonome_melee2.wav");

	//PRECACHE_SOUND_ARRAY(pIdleSounds);
	//PRECACHE_SOUND_ARRAY(pPainSounds);
	//PRECACHE_SOUND_ARRAY(pDeathSounds);
	//PRECACHE_SOUND_ARRAY(pAlertSounds);
	PRECACHE_SOUND_ARRAY(pAttackHitSounds);
	PRECACHE_SOUND_ARRAY(pAttackMissSounds);

	PRECACHE_SOUND("gonome/gonome_run.wav");

	PRECACHE_SOUND("bullchicken/bc_acid1.wav");

	PRECACHE_SOUND("bullchicken/bc_bite2.wav");
	PRECACHE_SOUND("bullchicken/bc_bite3.wav");

	PRECACHE_SOUND("bullchicken/bc_spithit1.wav");
	PRECACHE_SOUND("bullchicken/bc_spithit2.wav");

	PRECACHE_SOUND("gonome/gonome_step1.wav");
	PRECACHE_SOUND("gonome/gonome_step2.wav");
}

//=========================================================
// GetSchedule
//=========================================================
Schedule_t* CGonome::GetSchedule(void)
{
	switch (m_MonsterState)
	{
	case MONSTERSTATE_COMBAT:
	{
		// dead enemy
		if (HasConditions(bits_COND_ENEMY_DEAD))
		{
			// call base class, all code to handle dead enemies is centralized there.
			return CBaseMonster::GetSchedule();
		}

		if (HasConditions(bits_COND_NEW_ENEMY))
		{
			return GetScheduleOfType(SCHED_WAKE_ANGRY);
		}

		if (HasConditions(bits_COND_CAN_RANGE_ATTACK1))
		{
			return GetScheduleOfType(SCHED_RANGE_ATTACK1);
		}

		if (HasConditions(bits_COND_CAN_MELEE_ATTACK1))
		{
			return GetScheduleOfType(SCHED_MELEE_ATTACK1);
		}

		if (HasConditions(bits_COND_CAN_MELEE_ATTACK2))
		{
			return GetScheduleOfType(SCHED_MELEE_ATTACK2);
		}

		return GetScheduleOfType(SCHED_CHASE_ENEMY);
		break;
	}
	default:
		break;
	}

	return CBaseMonster::GetSchedule();
}

// primary range attack
Task_t tlGonomeRangeAttack1[] =
	{
		{TASK_STOP_MOVING, 0},
		{TASK_FACE_IDEAL, (float)0},
		{TASK_RANGE_ATTACK1, (float)0},
		{TASK_SET_ACTIVITY, (float)ACT_IDLE},
};

Schedule_t slGonomeRangeAttack1[] =
	{
		{tlGonomeRangeAttack1,
			ARRAYSIZE(tlGonomeRangeAttack1),
			bits_COND_NEW_ENEMY |
				bits_COND_ENEMY_DEAD |
				bits_COND_HEAVY_DAMAGE |
				bits_COND_ENEMY_OCCLUDED |
				bits_COND_NO_AMMO_LOADED,
			0,
			"Gonome Range Attack1"},
};

// Chase enemy schedule
Task_t tlGonomeChaseEnemy1[] =
	{
		{TASK_SET_FAIL_SCHEDULE, (float)SCHED_RANGE_ATTACK1}, // !!!OEM - this will stop nasty squid oscillation.
		{TASK_GET_PATH_TO_ENEMY, (float)0},
		{TASK_RUN_PATH, (float)0},
		{TASK_WAIT_FOR_MOVEMENT, (float)0},
};

Schedule_t slGonomeChaseEnemy[] =
	{
		{tlGonomeChaseEnemy1,
			ARRAYSIZE(tlGonomeChaseEnemy1),
			bits_COND_NEW_ENEMY |
				bits_COND_ENEMY_DEAD |
				bits_COND_SMELL_FOOD |
				bits_COND_CAN_RANGE_ATTACK1 |
				bits_COND_CAN_MELEE_ATTACK1 |
				bits_COND_CAN_MELEE_ATTACK2 |
				bits_COND_TASK_FAILED,
			0,
			"Gonome Chase Enemy"},
};

// victory dance (eating body)
Task_t tlGonomeVictoryDance[] =
	{
		{TASK_STOP_MOVING, (float)0},
		{TASK_WAIT, 0.1f},
		{TASK_GET_PATH_TO_ENEMY_CORPSE, 40.0f},
		{TASK_WALK_PATH, (float)0},
		{TASK_WAIT_FOR_MOVEMENT, (float)0},
		{TASK_FACE_ENEMY, (float)0},
		{TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE},
		//{TASK_GET_HEALTH_FROM_FOOD, (float)0.25},
		{TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE},
		//{TASK_GET_HEALTH_FROM_FOOD, (float)0.25},
		{TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE},
		//{TASK_GET_HEALTH_FROM_FOOD, (float)0.25},
};

Schedule_t slGonomeVictoryDance[] =
	{
		{tlGonomeVictoryDance,
			ARRAYSIZE(tlGonomeVictoryDance),
			bits_COND_NEW_ENEMY |
				bits_COND_LIGHT_DAMAGE |
				bits_COND_HEAVY_DAMAGE,
			0,
			"GonomeVictoryDance"},
};

DEFINE_CUSTOM_SCHEDULES(CGonome){
	slGonomeRangeAttack1,
	slGonomeChaseEnemy,
	slGonomeVictoryDance,
};

IMPLEMENT_CUSTOM_SCHEDULES(CGonome, CBaseMonster)

Schedule_t* CGonome::GetScheduleOfType(int Type)
{
	switch (Type)
	{
	case SCHED_RANGE_ATTACK1:
		return &slGonomeRangeAttack1[0];
		break;
	case SCHED_CHASE_ENEMY:
		return &slGonomeChaseEnemy[0];
		break;
	case SCHED_VICTORY_DANCE:
		return &slGonomeVictoryDance[0];
		break;
	default:
		break;
	}
	return CBaseMonster::GetScheduleOfType(Type);
}

void CGonome::Killed(entvars_t* pevAttacker, int iGib)
{
	if (m_pGonomeGuts)
	{
		UTIL_Remove(m_pGonomeGuts);
		m_pGonomeGuts = nullptr;
	}

	CBaseMonster::Killed(pevAttacker, iGib);
}

void CGonome::RunTask(Task_t* pTask)
{
	// HACK to stop Gonome from playing attack sound twice
	if (pTask->iTask == TASK_MELEE_ATTACK1)
	{
		if (!m_playedAttackSound)
		{
			const char* sample = NULL;
			if (m_meleeAttack2)
			{
				sample = "!GO_MELEE1";
			}
			else
			{
				sample = "!GO_MELEE0";
			}
			EMIT_SOUND(ENT(pev), CHAN_BODY, sample, 1, ATTN_NORM);
			m_playedAttackSound = true;
		}
	}
	else
	{
		m_playedAttackSound = false;
	}
	CBaseMonster::RunTask(pTask);
}

//=========================================================
// DEAD GONOME PROP
//=========================================================
class CDeadGonome : public CBaseMonster
{
public:
	void Spawn() override;
	int Classify() override { return CLASS_ALIEN_PASSIVE; }

	bool KeyValue(KeyValueData* pkvd) override;

	int m_iPose; // which sequence to display	-- temporary, don't need to save
	static const char* m_szPoses[3];
};

const char* CDeadGonome::m_szPoses[] = {"dead_on_stomach1", "dead_on_back", "dead_on_side"};

bool CDeadGonome::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "pose"))
	{
		m_iPose = atoi(pkvd->szValue);
		return true;
	}

	return CBaseMonster::KeyValue(pkvd);
}

LINK_ENTITY_TO_CLASS(monster_gonome_dead, CDeadGonome);

//=========================================================
// ********** DeadGonome SPAWN **********
//=========================================================
void CDeadGonome::Spawn()
{
	PRECACHE_MODEL("models/gonome.mdl");
	SET_MODEL(ENT(pev), "models/gonome.mdl");

	pev->effects = 0;
	pev->sequence = 0;
	m_bloodColor = BLOOD_COLOR_GREEN;

	pev->sequence = LookupSequence(m_szPoses[m_iPose]);

	if (pev->sequence == -1)
	{
		ALERT(at_console, "Dead gonome with bad pose\n");
	}

	// Corpses have less health
	pev->health = 8;

	MonsterInitDead();
}