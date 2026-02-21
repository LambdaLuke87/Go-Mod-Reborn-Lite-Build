/***
 *
 *	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
// Gargantua
//=========================================================
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "nodes.h"
#include "monsters.h"
#include "schedule.h"
#include "customentity.h"
#include "weapons.h"
#include "effects.h"
#include "soundent.h"
#include "decals.h"
#include "explode.h"
#include "func_break.h"
#include "scripted.h"
#include "gamerules.h"
#include "game.h"

//=========================================================
// Gargantua Monster
//=========================================================
const float GARG_ATTACKDIST = 80.0f;

// Garg animation events
#define GARG_AE_SLASH_LEFT 1
// #define GARG_AE_BEAM_ATTACK_RIGHT		2	// No longer used
#define GARG_AE_LEFT_FOOT 3
#define GARG_AE_RIGHT_FOOT 4
#define GARG_AE_STOMP 5
#define GARG_AE_BREATHE 6
#define BABYGARG_AE_KICK 7
#define STOMP_FRAMETIME 0.015f // gpGlobals->frametime

// Gargantua is immune to any damage but this
#define GARG_DAMAGE (DMG_ENERGYBEAM | DMG_CRUSH | DMG_MORTAR | DMG_BLAST)
#define GARG_EYE_SPRITE_NAME "sprites/gargeye1.spr"
#define GARG_BEAM_SPRITE_NAME "sprites/xbeam3.spr"
#define GARG_BEAM_SPRITE2 "sprites/xbeam3.spr"
#define GARG_STOMP_SPRITE_NAME "sprites/gargeye1.spr"
#define GARG_STOMP_BUZZ_SOUND "weapons/mine_charge.wav"
#define GARG_FLAME_LENGTH 330
#define GARG_GIB_MODEL "models/metalplategibs.mdl"

#define ATTN_GARG (ATTN_NORM)

#define STOMP_SPRITE_COUNT 10

void SpawnExplosion(Vector center, float randomRange, float time, int magnitude);

class CSmoker;

// Spiral Effect
class CSpiral : public CBaseEntity
{
public:
	void Spawn() override;
	void Think() override;
	int ObjectCaps() override { return FCAP_DONT_SAVE; }
	static CSpiral* Create(const Vector& origin, float height, float radius, float duration);
};

LINK_ENTITY_TO_CLASS(streak_spiral, CSpiral);

class CStomp : public CBaseEntity
{
public:
	void Spawn(void);
	void Think(void);
	static CStomp* StompCreate(const Vector& origin, const Vector& end, float speed, const char* spriteName,
		const Vector& color, float damage, float scale, int sprite, edict_t* garg = NULL, float soundAttenuation = 0.0f);

private:
	// UNDONE: re-use this sprite list instead of creating new ones all the time
	//	CSprite		*m_pSprites[STOMP_SPRITE_COUNT];
};

LINK_ENTITY_TO_CLASS(garg_stomp, CStomp)

#define SF_STOMPSHOOTER_FIRE_ONCE 1

class CStompShooter : public CPointEntity
{
public:
	void Spawn();
	void Precache();
	bool KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);

	int m_sprite;
};

LINK_ENTITY_TO_CLASS(env_stompshooter, CStompShooter)

void CStompShooter::Spawn()
{
	Precache();
}

void CStompShooter::Precache()
{
	m_sprite = PRECACHE_MODEL(GARG_STOMP_SPRITE_NAME);
	PRECACHE_SOUND(GARG_STOMP_BUZZ_SOUND);
}

bool CStompShooter::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "attenuation"))
	{
		pev->armortype = atof(pkvd->szValue);
		pkvd->fHandled = true;
	}
	return CPointEntity::KeyValue(pkvd);
}

void CStompShooter::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	Vector vecStart = pev->origin;
	Vector vecEnd;
	if (FStringNull(pev->target))
	{
		UTIL_MakeVectors(pev->angles);
		vecEnd = (gpGlobals->v_forward * 1024) + vecStart;
		TraceResult trace;
		UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, edict(), &trace);
		vecEnd = trace.vecEndPos;
		vecEnd.z = vecStart.z;
	}
	else
	{
		CBaseEntity* pEntity = UTIL_FindEntityByTargetname(NULL, STRING(pev->target));
		if (pEntity)
		{
			vecEnd = pEntity->pev->origin;
		}
		else
		{
			ALERT(at_error, "%s: can't find target '%s'", STRING(pev->classname), STRING(pev->target));
			return;
		}
	}

	CBaseEntity* pOwner = NULL;
	if (!FStringNull(pev->netname))
	{
		pOwner = UTIL_FindEntityByTargetname(NULL, STRING(pev->netname));
	}

	const float scale = pev->scale > 0 ? pev->scale : 1.0f;
	CStomp::StompCreate(vecStart, vecEnd, pev->speed, GARG_STOMP_SPRITE_NAME, Vector(255, 255, 255),
		gSkillData.gargantuaDmgStomp, scale, m_sprite, pOwner ? pOwner->edict() : NULL, pev->armortype);

	if (FBitSet(pev->spawnflags, SF_STOMPSHOOTER_FIRE_ONCE))
	{
		SetThink(&CBaseEntity::SUB_Remove);
		pev->nextthink = gpGlobals->time;
	}
}

CStomp* CStomp::StompCreate(const Vector& origin, const Vector& end, float speed, const char* spriteName,
	const Vector& color, float damage, float scale, int sprite, edict_t* garg, float soundAttenuation)
{
	CStomp* pStomp = GetClassPtr((CStomp*)NULL);

	pStomp->pev->origin = origin;
	Vector dir = end - origin;
	pStomp->pev->scale = dir.Length();
	pStomp->pev->movedir = dir.Normalize();
	pStomp->pev->speed = speed;
	pStomp->pev->model = MAKE_STRING(spriteName);
	pStomp->pev->rendercolor = color;
	pStomp->pev->dmg = damage;
	pStomp->pev->frags = scale;
	pStomp->pev->owner = garg;
	pStomp->pev->armortype = soundAttenuation;
	pStomp->Spawn();

	return pStomp;
}

void CStomp::Spawn(void)
{
	pev->nextthink = gpGlobals->time;
	pev->classname = MAKE_STRING("garg_stomp");
	pev->dmgtime = gpGlobals->time;
	pev->effects |= EF_NODRAW;

	pev->framerate = 30;
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	EMIT_SOUND_DYN(edict(), CHAN_BODY, GARG_STOMP_BUZZ_SOUND, 1, pev->armortype > 0 ? pev->armortype : ATTN_NORM, 0, PITCH_NORM * 0.55f);
}

#define STOMP_INTERVAL 0.025f

void CStomp::Think(void)
{
	TraceResult tr;

	pev->nextthink = gpGlobals->time + 0.1f;

	// Do damage for this frame
	Vector vecStart = pev->origin;
	vecStart.z += 30;
	Vector vecEnd = vecStart + (pev->movedir * pev->speed * STOMP_FRAMETIME);

	UTIL_TraceHull(vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);

	if (tr.pHit && tr.pHit != pev->owner)
	{
		CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);
		if (pEntity && pEntity->pev->takedamage)
		{
			entvars_t* pevOwner = pev;
			if (pev->owner)
				pevOwner = VARS(pev->owner);
			pEntity->TakeDamage(pev, pevOwner, pev->dmg, DMG_SONIC);
		}
	}

	// Accelerate the effect
	pev->speed = pev->speed + (STOMP_FRAMETIME)*pev->framerate;
	pev->framerate = pev->framerate + (STOMP_FRAMETIME) * 1500.0f;

	float stompInterval = STOMP_INTERVAL;
	int numOfSprites = 2;
	int maxNumOfSprites = 8;
	float spriteScale = pev->frags;
	if (g_pGameRules->IsMultiplayer())
	{
		stompInterval = STOMP_INTERVAL * 2;
		spriteScale *= 1.8;
		maxNumOfSprites = 6;
	}
	const int freeEnts = gpGlobals->maxEntities - NUMBER_OF_ENTITIES();
	maxNumOfSprites = V_min(maxNumOfSprites, freeEnts);

	// TODO: make it into clint side effects?
	// Move and spawn trails
	while (gpGlobals->time - pev->dmgtime > stompInterval)
	{
		pev->origin = pev->origin + pev->movedir * pev->speed * stompInterval;
		for (int i = 0; i < numOfSprites && maxNumOfSprites > 0; i++)
		{
			maxNumOfSprites--;
			CSprite* pSprite = CSprite::SpriteCreate(STRING(pev->model), pev->origin, true);
			if (pSprite)
			{
				UTIL_TraceLine(pev->origin, pev->origin - Vector(0, 0, 500), ignore_monsters, edict(), &tr);
				pSprite->pev->origin = tr.vecEndPos;
				pSprite->pev->velocity = Vector(RANDOM_FLOAT(-200.0f, 200.0f), RANDOM_FLOAT(-200.0f, 200.0f), 175);
				// pSprite->AnimateAndDie( RANDOM_FLOAT( 8.0f, 12.0f ) );
				pSprite->pev->nextthink = gpGlobals->time + 0.3f;
				pSprite->SetThink(&CBaseEntity::SUB_Remove);
				pSprite->SetTransparency(kRenderTransAdd, pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z, 255, kRenderFxFadeFast);
				pSprite->SetScale(spriteScale);
			}
		}
		pev->dmgtime += stompInterval;

		// Scale has the "life" of this effect
		pev->scale -= stompInterval * pev->speed;
		if (pev->scale <= 0)
		{
			// Life has run out
			UTIL_Remove(this);
			STOP_SOUND(edict(), CHAN_BODY, GARG_STOMP_BUZZ_SOUND);
		}
	}
}

void StreakSplash(const Vector& origin, const Vector& direction, int color, int count, int speed, int velocityRange)
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, origin);
	WRITE_BYTE(TE_STREAK_SPLASH);
	WRITE_COORD(origin.x); // origin
	WRITE_COORD(origin.y);
	WRITE_COORD(origin.z);
	WRITE_COORD(direction.x); // direction
	WRITE_COORD(direction.y);
	WRITE_COORD(direction.z);
	WRITE_BYTE(color);	// Streak color 6
	WRITE_SHORT(count); // count
	WRITE_SHORT(speed);
	WRITE_SHORT(velocityRange); // Random velocity modifier
	MESSAGE_END();
}

class CGargantua : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void PrecacheImpl();
	void UpdateOnRemove();
	void SetYawSpeed() override;
	int Classify() override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;

	bool CheckMeleeAttack1(float flDot, float flDist); // Swipe
	bool CheckMeleeAttack2(float flDot, float flDist); // Flames
	bool CheckRangeAttack1(float flDot, float flDist); // Stomp attack
	void SetObjectCollisionBox()
	{
		pev->absmin = pev->origin + Vector(-80, -80, 0);
		pev->absmax = pev->origin + Vector(80, 80, 214);
	}

	Schedule_t* GetSchedule();
	Schedule_t* GetScheduleOfType(int Type);
	void StartTask(Task_t* pTask);
	void RunTask(Task_t* pTask);

	void PrescheduleThink();

	void Killed(entvars_t* pevAttacker, int iGib) override;
	void DeathEffect();

	void EyeOff();
	void EyeOn(int level);
	void EyeUpdate();
	void Leap();
	void StompAttack();
	void FlameCreate();
	void FlameUpdate();
	void FlameControls(float angleX, float angleY);
	void FlameDestroy();
	void FlameOffSound();
	inline bool FlameIsOn() { return m_pFlame[0] != NULL; }

	void FlameDamage(Vector vecStart, Vector vecEnd, entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType);

	virtual bool Save(CSave& save);
	virtual bool Restore(CRestore& restore);
	static TYPEDESCRIPTION m_SaveData[];

	CUSTOM_SCHEDULES

	Vector DefaultMinHullSize() { return Vector(-32.0f, -32.0f, 0.0f); }
	Vector DefaultMaxHullSize() { return Vector(32.0f, 32.0f, 64.0f); }

	int m_stompSprite;
	int m_GargGibModel;

protected:
	virtual float DefaultHealth();
	virtual float FireAttackDamage();
	virtual float StompAttackDamage();
	virtual const char* DefaultModel();
	virtual const char* EyeSprite();
	virtual float EyeScale();
	virtual Vector EyeColor();
	virtual const char* StompSprite();
	virtual int MaxEyeBrightness();
	virtual void FootEffect();
	virtual void StompEffect();
	virtual float FlameLength();
	virtual int BigFlameScale();
	virtual int SmallFlameScale();
	virtual void PrecacheSounds();
	virtual void BreatheSound();
	virtual void AttackSound();
	virtual float FlameTimeDivider();
	virtual Vector StompAttackStartVec();

	void HandleSlashAnim(float damage, float punch, float velocity);

	static const char* pAttackHitSounds[];
	static const char* pBeamAttackSounds[];
	static const char* pAttackMissSounds[];
	static const char* pRicSounds[];
	static const char* pFootSounds[];
	// static const char* pIdleSounds[];
	static const char* pAlertSounds[];
	// static const char* pPainSounds[];
	// static const char* pAttackSounds[];
	static const char* pStompSounds[];
	// static const char* pBreatheSounds[];

	CBaseEntity* GargantuaCheckTraceHullAttack(float flDist, int iDamage, int iDmgType);

	CSprite* m_pEyeGlow; // Glow around the eyes
	CBeam* m_pFlame[4];	 // Flame beams

	int m_eyeBrightness;   // Brightness target
	float m_seeTime;	   // Time to attack (when I see the enemy, I set this)
	float m_flameTime;	   // Time of next flame attack
	float m_painSoundTime; // Time of next pain sound
	float m_streakTime;	   // streak timer (don't send too many)
	float m_flameX;		   // Flame thrower aim
	float m_flameY;
	float m_breatheTime;
};

LINK_ENTITY_TO_CLASS(monster_gargantua, CGargantua)

TYPEDESCRIPTION CGargantua::m_SaveData[] =
	{
		DEFINE_FIELD(CGargantua, m_pEyeGlow, FIELD_CLASSPTR),
		DEFINE_FIELD(CGargantua, m_eyeBrightness, FIELD_INTEGER),
		DEFINE_FIELD(CGargantua, m_seeTime, FIELD_TIME),
		DEFINE_FIELD(CGargantua, m_flameTime, FIELD_TIME),
		DEFINE_FIELD(CGargantua, m_streakTime, FIELD_TIME),
		DEFINE_FIELD(CGargantua, m_painSoundTime, FIELD_TIME),
		DEFINE_ARRAY(CGargantua, m_pFlame, FIELD_CLASSPTR, 4),
		DEFINE_FIELD(CGargantua, m_flameX, FIELD_FLOAT),
		DEFINE_FIELD(CGargantua, m_flameY, FIELD_FLOAT),
};

IMPLEMENT_SAVERESTORE(CGargantua, CBaseMonster)

const char* CGargantua::pAttackHitSounds[] =
	{
		"zombie/claw_strike1.wav",
		"zombie/claw_strike2.wav",
		"zombie/claw_strike3.wav",
};

const char* CGargantua::pBeamAttackSounds[] =
	{
		"garg/gar_flameoff1.wav",
		"garg/gar_flameon1.wav",
		"garg/gar_flamerun1.wav",
};

const char* CGargantua::pAttackMissSounds[] =
	{
		"zombie/claw_miss1.wav",
		"zombie/claw_miss2.wav",
};

const char* CGargantua::pRicSounds[] =
	{
#if 0
	"weapons/ric1.wav",
	"weapons/ric2.wav",
	"weapons/ric3.wav",
	"weapons/ric4.wav",
	"weapons/ric5.wav",
#else
		"debris/metal4.wav",
		"debris/metal6.wav",
		"weapons/ric4.wav",
		"weapons/ric5.wav",
#endif
};

const char* CGargantua::pFootSounds[] =
	{
		"garg/gar_step1.wav",
		"garg/gar_step2.wav",
};

/*const char* CGargantua::pIdleSounds[] =
{
	"garg/gar_idle1.wav",
	"garg/gar_idle2.wav",
	"garg/gar_idle3.wav",
	"garg/gar_idle4.wav",
	"garg/gar_idle5.wav",
};

const char* CGargantua::pAttackSounds[] =
{
	"garg/gar_attack1.wav",
	"garg/gar_attack2.wav",
	"garg/gar_attack3.wav",
};*/

const char* CGargantua::pAlertSounds[] =
	{
		"garg/gar_alert1.wav",
		"garg/gar_alert2.wav",
		"garg/gar_alert3.wav",
};

/*const char* CGargantua::pPainSounds[] =
{
	"garg/gar_pain1.wav",
	"garg/gar_pain2.wav",
	"garg/gar_pain3.wav",
};*/

const char* CGargantua::pStompSounds[] =
	{
		"garg/gar_stomp1.wav",
};

/*const char* CGargantua::pBreatheSounds[] =
{
	"garg/gar_breathe1.wav",
	"garg/gar_breathe2.wav",
	"garg/gar_breathe3.wav",
};*/
//=========================================================
// AI Schedules Specific to this monster
//=========================================================
#if 0
enum
{
	SCHED_ = LAST_COMMON_SCHEDULE + 1
};
#endif

enum
{
	TASK_SOUND_ATTACK = LAST_COMMON_TASK + 1,
	TASK_FLAME_SWEEP,
};

Task_t tlGargFlame[] =
	{
		{TASK_STOP_MOVING, (float)0},
		{TASK_FACE_ENEMY, (float)0},
		{TASK_SOUND_ATTACK, (float)0},
		// { TASK_PLAY_SEQUENCE, (float)ACT_SIGNAL1 },
		{TASK_SET_ACTIVITY, (float)ACT_MELEE_ATTACK2},
		{TASK_FLAME_SWEEP, (float)4.5},
		{TASK_SET_ACTIVITY, (float)ACT_IDLE},
};

Schedule_t slGargFlame[] =
	{
		{tlGargFlame,
			ARRAYSIZE(tlGargFlame),
			0,
			0,
			"GargFlame"},
};

// primary melee attack
Task_t tlGargSwipe[] =
	{
		{TASK_STOP_MOVING, 0},
		{TASK_FACE_ENEMY, (float)0},
		{TASK_MELEE_ATTACK1, (float)0},
};

Schedule_t slGargSwipe[] =
	{
		{tlGargSwipe,
			ARRAYSIZE(tlGargSwipe),
			bits_COND_CAN_MELEE_ATTACK2,
			0,
			"GargSwipe"},
};

Task_t tlGargStomp[] =
	{
		{TASK_STOP_MOVING, 0},
		{TASK_FACE_ENEMY, (float)0},
		{TASK_RANGE_ATTACK1, (float)0},
};

Schedule_t slGargStomp[] =
	{
		{tlGargStomp,
			ARRAYSIZE(tlGargStomp),
			bits_COND_NEW_ENEMY |
				bits_COND_ENEMY_DEAD,
			0,
			"GargStomp"},
};

DEFINE_CUSTOM_SCHEDULES(CGargantua){
	slGargFlame,
	slGargSwipe,
	slGargStomp,
};

IMPLEMENT_CUSTOM_SCHEDULES(CGargantua, CBaseMonster)

void CGargantua::EyeOn(int level)
{
	m_eyeBrightness = level;
}

void CGargantua::EyeOff()
{
	m_eyeBrightness = 0;
}

void CGargantua::EyeUpdate()
{
	if (m_pEyeGlow)
	{
		m_pEyeGlow->pev->renderamt = UTIL_Approach(m_eyeBrightness, m_pEyeGlow->pev->renderamt, MaxEyeBrightness() / 8 + 1);
		if (m_pEyeGlow->pev->renderamt == 0)
			m_pEyeGlow->pev->effects |= EF_NODRAW;
		else
			m_pEyeGlow->pev->effects &= ~EF_NODRAW;
		UTIL_SetOrigin(m_pEyeGlow->pev, pev->origin);
	}
}

void CGargantua::StompAttack()
{
	TraceResult trace;

	UTIL_MakeVectors(pev->angles);
	Vector vecStart = StompAttackStartVec();
	Vector vecAim = ShootAtEnemy(vecStart);
	Vector vecEnd = (vecAim * 1024) + vecStart;

	UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, edict(), &trace);
	CStomp::StompCreate(vecStart, trace.vecEndPos, 0, StompSprite(), EyeColor(), StompAttackDamage(), EyeScale(), m_stompSprite, edict());
	StompEffect();

	UTIL_TraceLine(pev->origin, pev->origin - Vector(0, 0, 20), ignore_monsters, edict(), &trace);
	if (trace.flFraction < 1.0f)
		UTIL_DecalTrace(&trace, DECAL_GARGSTOMP1);
}

void CGargantua::FlameCreate()
{
	int i;
	Vector posGun, angleGun;
	TraceResult trace;

	UTIL_MakeVectors(pev->angles);

	for (i = 0; i < 4; i++)
	{
		if (i < 2)
			m_pFlame[i] = CBeam::BeamCreate(GARG_BEAM_SPRITE_NAME, BigFlameScale());
		else
			m_pFlame[i] = CBeam::BeamCreate(GARG_BEAM_SPRITE2, SmallFlameScale());
		if (m_pFlame[i])
		{
			int attach = i % 2;
			// attachment is 0 based in GetAttachment
			GetAttachment(attach + 1, posGun, angleGun);

			Vector vecEnd = (gpGlobals->v_forward * FlameLength()) + posGun;
			UTIL_TraceLine(posGun, vecEnd, dont_ignore_monsters, edict(), &trace);

			m_pFlame[i]->PointEntInit(trace.vecEndPos, entindex());
			if (i < 2)
				m_pFlame[i]->SetColor(255, 130, 90);
			else
				m_pFlame[i]->SetColor(0, 120, 255);
			m_pFlame[i]->SetBrightness(190);
			m_pFlame[i]->SetFlags(BEAM_FSHADEIN);
			m_pFlame[i]->SetScrollRate(20);
			// attachment is 1 based in SetEndAttachment
			m_pFlame[i]->SetEndAttachment(attach + 2);
			CSoundEnt::InsertSound(bits_SOUND_COMBAT, posGun, 384, 0.3);
		}
	}
	EMIT_SOUND_DYN(edict(), CHAN_BODY, pBeamAttackSounds[1], 1.0, ATTN_NORM, 0, PITCH_NORM);
	EMIT_SOUND_DYN(edict(), CHAN_WEAPON, pBeamAttackSounds[2], 1.0, ATTN_NORM, 0, PITCH_NORM);
}

void CGargantua::FlameControls(float angleX, float angleY)
{
	if (angleY < -180)
		angleY += 360;
	else if (angleY > 180)
		angleY -= 360;

	if (angleY < -45)
		angleY = -45;
	else if (angleY > 45)
		angleY = 45;

	m_flameX = UTIL_ApproachAngle(angleX, m_flameX, 4);
	m_flameY = UTIL_ApproachAngle(angleY, m_flameY, 8);
	SetBoneController(0, m_flameY);
	SetBoneController(1, m_flameX);
}

void CGargantua::FlameUpdate()
{
	int i;
	TraceResult trace;
	Vector vecStart, angleGun;
	bool streaks = false;

	for (i = 0; i < 2; i++)
	{
		if (m_pFlame[i])
		{
			Vector vecAim = pev->angles;
			vecAim.x += m_flameX;
			vecAim.y += m_flameY;

			UTIL_MakeVectors(vecAim);

			GetAttachment(i + 1, vecStart, angleGun);
			Vector vecEnd = vecStart + (gpGlobals->v_forward * FlameLength()); //  - offset[i] * gpGlobals->v_right;

			UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, edict(), &trace);

			m_pFlame[i]->SetStartPos(trace.vecEndPos);
			m_pFlame[i + 2]->SetStartPos((vecStart * 0.6f) + (trace.vecEndPos * 0.4f));

			if (trace.flFraction != 1.0f && gpGlobals->time > m_streakTime)
			{
				StreakSplash(trace.vecEndPos, trace.vecPlaneNormal, 6, 20, 50, 400);
				streaks = true;
				UTIL_DecalTrace(&trace, DECAL_SMALLSCORCH1 + RANDOM_LONG(0, 2));
			}

			// RadiusDamage( trace.vecEndPos, pev, pev, gSkillData.gargantuaDmgFire, CLASS_ALIEN_MONSTER, DMG_BURN );
			FlameDamage(vecStart, trace.vecEndPos, pev, pev, FireAttackDamage(), Classify(), DMG_BURN);

			MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(TE_ELIGHT);
			WRITE_SHORT(entindex() + 0x1000 * (i + 2)); // entity, attachment
			WRITE_COORD(vecStart.x);					// origin
			WRITE_COORD(vecStart.y);
			WRITE_COORD(vecStart.z);
			WRITE_COORD(RANDOM_FLOAT(32.0f, 48.0f)); // radius
			WRITE_BYTE(255);						 // R
			WRITE_BYTE(255);						 // G
			WRITE_BYTE(255);						 // B
			WRITE_BYTE(2);							 // life * 10
			WRITE_COORD(0);							 // decay
			MESSAGE_END();
		}
	}
	if (streaks)
		m_streakTime = gpGlobals->time;
}

void CGargantua::FlameDamage(Vector vecStart, Vector vecEnd, entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType)
{
	CBaseEntity* pEntity = NULL;
	TraceResult tr;
	float flAdjustedDamage;
	Vector vecSpot;

	Vector vecMid = (vecStart + vecEnd) * 0.5f;

	float searchRadius = (vecStart - vecMid).Length();

	Vector vecAim = (vecEnd - vecStart).Normalize();

	// iterate on all entities in the vicinity.
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecMid, searchRadius)) != NULL)
	{
		if (pEntity->pev->takedamage != DAMAGE_NO)
		{
			// UNDONE: this should check a damage mask, not an ignore
			if (iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore)
			{
				// houndeyes don't hurt other houndeyes with their attack
				continue;
			}

			vecSpot = pEntity->BodyTarget(vecMid);

			float dist = DotProduct(vecAim, vecSpot - vecMid);
			if (dist > searchRadius)
				dist = searchRadius;
			else if (dist < -searchRadius)
				dist = searchRadius;

			Vector vecSrc = vecMid + dist * vecAim;

			UTIL_TraceLine(vecSrc, vecSpot, dont_ignore_monsters, ENT(pev), &tr);

			if (tr.flFraction == 1.0f || tr.pHit == pEntity->edict())
			{
				// the explosion can 'see' this entity, so hurt them!
				// decrease damage for an ent that's farther from the flame.
				dist = (vecSrc - tr.vecEndPos).Length();

				if (dist > 64.0f)
				{
					flAdjustedDamage = flDamage - (dist - 64.0f) * 0.4f;
					if (flAdjustedDamage <= 0)
						continue;
				}
				else
				{
					flAdjustedDamage = flDamage;
				}

				// ALERT( at_console, "hit %s\n", STRING( pEntity->pev->classname ) );
				if (tr.flFraction != 1.0f)
				{
					ClearMultiDamage();
					pEntity->TraceAttack(pevInflictor, flAdjustedDamage, (tr.vecEndPos - vecSrc).Normalize(), &tr, bitsDamageType);
					ApplyMultiDamage(pevInflictor, pevAttacker);
				}
				else
				{
					pEntity->TakeDamage(pevInflictor, pevAttacker, flAdjustedDamage, bitsDamageType);
				}
			}
		}
	}
}

void CGargantua::FlameDestroy()
{
	for (int i = 0; i < 4; i++)
	{
		if (m_pFlame[i])
		{
			UTIL_Remove(m_pFlame[i]);
			m_pFlame[i] = NULL;
		}
	}
}

void CGargantua::FlameOffSound()
{
	EMIT_SOUND_DYN(edict(), CHAN_WEAPON, pBeamAttackSounds[0], 1.0, ATTN_NORM, 0, PITCH_NORM);
}

void CGargantua::PrescheduleThink()
{
	if (!HasConditions(bits_COND_SEE_ENEMY))
	{
		m_seeTime = gpGlobals->time + 5.0f;
		EyeOff();
	}
	else
		EyeOn(MaxEyeBrightness());

	EyeUpdate();
	CBaseMonster::PrescheduleThink();
}

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CGargantua::Classify()
{
	if (m_AltClass)
		return CLASS_PLAYER_ALIEN_ALLY;

	return CLASS_ALIEN_MONSTER;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CGargantua::SetYawSpeed()
{
	int ys;

	switch (m_Activity)
	{
	case ACT_IDLE:
		ys = 60;
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 180;
		break;
	case ACT_WALK:
	case ACT_RUN:
		ys = 60;
		break;
	default:
		ys = 60;
		break;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// Spawn
//=========================================================
void CGargantua::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), DefaultModel());
	UTIL_SetSize(pev, Vector(-32, -32, 0), Vector(32, 32, 64));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_GREEN;
	pev->health = gSkillData.gargantuaHealth;
	// pev->view_ofs		= Vector ( 0, 0, 96 );// taken from mdl file
	m_flFieldOfView = -0.2; // width of forward view cone ( as a dotproduct result )
	m_MonsterState = MONSTERSTATE_NONE;

	MonsterInit();

	m_pEyeGlow = CSprite::SpriteCreate(EyeSprite(), pev->origin, false);
	const Vector eyeColor = EyeColor();
	m_pEyeGlow->SetTransparency(kRenderGlow, eyeColor.x, eyeColor.y, eyeColor.z, 0, kRenderFxNoDissipation);
	m_pEyeGlow->SetAttachment(edict(), 1);
	m_pEyeGlow->SetScale(EyeScale());
	EyeOff();
	m_seeTime = gpGlobals->time + 5;
	m_flameTime = gpGlobals->time + 2;
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CGargantua::Precache()
{
	PrecacheImpl();
	m_GargGibModel = PRECACHE_MODEL(GARG_GIB_MODEL);
}

void CGargantua::PrecacheImpl()
{
	PRECACHE_MODEL(DefaultModel());
	PRECACHE_MODEL(EyeSprite());

	PRECACHE_MODEL(GARG_BEAM_SPRITE_NAME);
	PRECACHE_MODEL(GARG_BEAM_SPRITE2);
	m_stompSprite = PRECACHE_MODEL(StompSprite());
	PRECACHE_SOUND(GARG_STOMP_BUZZ_SOUND);

	PrecacheSounds();
}

void CGargantua::UpdateOnRemove()
{
	if (m_pEyeGlow)
	{
		UTIL_Remove(m_pEyeGlow);
		m_pEyeGlow = 0;
	}

	FlameDestroy();
	CBaseMonster::UpdateOnRemove();
}

void CGargantua::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	ALERT(at_aiconsole, "CGargantua::TraceAttack\n");

	if (!IsAlive())
	{
		CBaseMonster::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
		return;
	}

	// UNDONE: Hit group specific damage?
	if ((bitsDamageType & (GARG_DAMAGE | DMG_BLAST)) != 0)
	{
		if (m_painSoundTime < gpGlobals->time)
		{
			SENTENCEG_PlayRndSz(ENT(pev), "GA_PAIN", 1, ATTN_GARG, 0, PITCH_NORM);
			m_painSoundTime = gpGlobals->time + RANDOM_FLOAT(2.5, 4);
		}
	}

	bitsDamageType &= GARG_DAMAGE;

	if (bitsDamageType == 0)
	{
		if (pev->dmgtime != gpGlobals->time || (RANDOM_LONG(0, 100) < 20))
		{
			UTIL_Ricochet(ptr->vecEndPos, RANDOM_FLOAT(0.5, 1.5));
			pev->dmgtime = gpGlobals->time;
			//			if ( RANDOM_LONG(0,100) < 25 )
			//				EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, pRicSounds[ RANDOM_LONG(0,ARRAYSIZE(pRicSounds)-1) ], 1.0, ATTN_NORM, 0, PITCH_NORM );
		}
		flDamage = 0;
	}

	CBaseMonster::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
}

bool CGargantua::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	ALERT(at_aiconsole, "CGargantua::TakeDamage\n");

	if (IsAlive())
	{
		if ((bitsDamageType & GARG_DAMAGE) == 0)
			flDamage *= 0.01;
		if ((bitsDamageType & DMG_BLAST) != 0)
			SetConditions(bits_COND_LIGHT_DAMAGE);
	}

	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

void CGargantua::DeathEffect()
{
	int i;
	UTIL_MakeVectors(pev->angles);
	Vector deathPos = pev->origin + gpGlobals->v_forward * 100;

	// Create a spiral of streaks
	CSpiral::Create(deathPos, (pev->absmax.z - pev->absmin.z) * 0.6, 125, 1.5);

	Vector position = pev->origin;
	position.z += 32;
	for (i = 0; i < 7; i += 2)
	{
		SpawnExplosion(position, 70, (i * 0.3), 60 + (i * 20));
		position.z += 15;
	}

	CBaseEntity* pSmoker = CBaseEntity::Create("env_smoker", pev->origin, g_vecZero, NULL);
	pSmoker->pev->health = 1;						 // 1 smoke balls
	pSmoker->pev->scale = 46;						 // 4.6X normal size
	pSmoker->pev->dmg = 0;							 // 0 radial distribution
	pSmoker->pev->nextthink = gpGlobals->time + 2.5; // Start in 2.5 seconds
}

void CGargantua::Killed(entvars_t* pevAttacker, int iGib)
{
	EyeOff();
	FlameDestroy();
	UTIL_Remove(m_pEyeGlow);
	m_pEyeGlow = NULL;
	CBaseMonster::Killed(pevAttacker, GIB_NEVER);
}

//=========================================================
// CheckMeleeAttack1
// Garg swipe attack
//
//=========================================================
bool CGargantua::CheckMeleeAttack1(float flDot, float flDist)
{
	// ALERT( at_aiconsole, "CheckMelee(%f, %f)\n", flDot, flDist );

	if (flDot >= 0.7)
	{
		if (flDist <= GARG_ATTACKDIST)
			return true;
	}
	return false;
}

// Flame thrower madness!
bool CGargantua::CheckMeleeAttack2(float flDot, float flDist)
{
	// ALERT( at_aiconsole, "CheckMelee(%f, %f)\n", flDot, flDist );

	if (gpGlobals->time > m_flameTime)
	{
		if (flDot >= 0.8 && flDist > GARG_ATTACKDIST)
		{
			if (flDist <= FlameLength())
				return true;
		}
	}
	return false;
}

//=========================================================
// CheckRangeAttack1
// flDot is the cos of the angle of the cone within which
// the attack can occur.
//=========================================================
//
// Stomp attack
//
//=========================================================
bool CGargantua::CheckRangeAttack1(float flDot, float flDist)
{
	if (gpGlobals->time > m_seeTime)
	{
		if (flDot >= 0.7f && flDist > GARG_ATTACKDIST)
		{
			return true;
		}
	}
	return false;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CGargantua::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case GARG_AE_SLASH_LEFT:
		HandleSlashAnim(gSkillData.gargantuaDmgSlash, 30, 100);
		break;
	case GARG_AE_RIGHT_FOOT:
	case GARG_AE_LEFT_FOOT:
		FootEffect();
		break;
	case GARG_AE_STOMP:
		StompAttack();
		m_seeTime = gpGlobals->time + 12.0f;
		break;
	case GARG_AE_BREATHE:
		BreatheSound();
		break;
	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

void CGargantua::HandleSlashAnim(float damage, float punch, float velocity)
{
	// HACKHACK!!!
	CBaseEntity* pHurt = GargantuaCheckTraceHullAttack(GARG_ATTACKDIST + 10.0f, damage, DMG_SLASH);
	if (pHurt)
	{
		if (pHurt->pev->flags & (FL_MONSTER | FL_CLIENT))
		{
			pHurt->pev->punchangle.x = -punch; // pitch
			pHurt->pev->punchangle.y = -punch; // yaw
			pHurt->pev->punchangle.z = punch;  // roll
			// UTIL_MakeVectors( pev->angles );	// called by CheckTraceHullAttack
			pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_right * velocity;
		}
		EMIT_SOUND_DYN(edict(), CHAN_WEAPON, RANDOM_SOUND_ARRAY(pAttackHitSounds), 1.0, ATTN_NORM, 0, 50 + RANDOM_LONG(0, 15));
	}
	else // Play a random attack miss sound
		EMIT_SOUND_DYN(edict(), CHAN_WEAPON, RANDOM_SOUND_ARRAY(pAttackMissSounds), 1.0, ATTN_NORM, 0, 50 + RANDOM_LONG(0, 15));

	Vector forward;
	UTIL_MakeVectorsPrivate(pev->angles, forward, NULL, NULL);
}

//=========================================================
// CheckTraceHullAttack - expects a length to trace, amount
// of damage to do, and damage type. Returns a pointer to
// the damaged entity in case the monster wishes to do
// other stuff to the victim (punchangle, etc)
// Used for many contact-range melee attacks. Bites, claws, etc.

// Overridden for Gargantua because his swing starts lower as
// a percentage of his height (otherwise he swings over the
// players head)
//=========================================================
CBaseEntity* CGargantua::GargantuaCheckTraceHullAttack(float flDist, int iDamage, int iDmgType)
{
	TraceResult tr;

	UTIL_MakeVectors(pev->angles);
	Vector vecStart = pev->origin;
	vecStart.z += 64;
	Vector vecEnd = vecStart + (gpGlobals->v_forward * flDist) - (gpGlobals->v_up * flDist * 0.3f);

	UTIL_TraceHull(vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);

	if (tr.pHit)
	{
		CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);

		if (pEntity && iDamage > 0)
		{
			pEntity->TakeDamage(pev, pev, iDamage, iDmgType);
		}

		return pEntity;
	}

	return NULL;
}

Schedule_t* CGargantua::GetSchedule()
{
	switch (m_MonsterState)
	{
	case MONSTERSTATE_IDLE:
	case MONSTERSTATE_ALERT:
	default:
		break;
	}
	return CBaseMonster::GetSchedule();
}

Schedule_t* CGargantua::GetScheduleOfType(int Type)
{
	// HACKHACK - turn off the flames if they are on and garg goes scripted / dead
	if (FlameIsOn())
	{
		FlameOffSound();
		FlameDestroy();
	}

	switch (Type)
	{
	case SCHED_MELEE_ATTACK2:
		return slGargFlame;
	case SCHED_MELEE_ATTACK1:
		return slGargSwipe;
	case SCHED_RANGE_ATTACK1:
		return slGargStomp;
	default:
		break;
	}

	return CBaseMonster::GetScheduleOfType(Type);
}

void CGargantua::StartTask(Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_FLAME_SWEEP:
		FlameCreate();
		m_flWaitFinished = gpGlobals->time + pTask->flData / FlameTimeDivider();
		m_flameTime = gpGlobals->time + 6.0f;
		m_flameX = 0;
		m_flameY = 0;
		break;
	case TASK_SOUND_ATTACK:
		if (RANDOM_LONG(0, 100) < 30)
			AttackSound();
		TaskComplete();
		break;
	case TASK_DIE:
		m_flWaitFinished = gpGlobals->time + 1.6f;
		DeathEffect();
		// FALL THROUGH
	default:
		CBaseMonster::StartTask(pTask);
		break;
	}
}

//=========================================================
// RunTask
//=========================================================
void CGargantua::RunTask(Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_DIE:
		if (gpGlobals->time > m_flWaitFinished)
		{
			pev->renderfx = kRenderFxExplode;
			pev->rendercolor.x = 255;
			pev->rendercolor.y = 0;
			pev->rendercolor.z = 0;
			StopAnimation();
			pev->nextthink = gpGlobals->time + 0.15f;
			SetThink(&CBaseEntity::SUB_Remove);
			int i;
			int parts = MODEL_FRAMES(m_GargGibModel);
			for (i = 0; i < 10; i++)
			{
				CGib* pGib = GetClassPtr((CGib*)NULL);

				pGib->Spawn(GARG_GIB_MODEL);

				int bodyPart = 0;
				if (parts > 1)
					bodyPart = RANDOM_LONG(0, parts - 1);

				pGib->pev->body = bodyPart;
				pGib->m_bloodColor = m_bloodColor;
				pGib->m_material = matNone;
				pGib->pev->origin = pev->origin;
				pGib->pev->velocity = UTIL_RandomBloodVector() * RANDOM_FLOAT(300, 500);
				pGib->pev->nextthink = gpGlobals->time + 1.25f;
				pGib->SetThink(&CBaseEntity::SUB_FadeOut);
			}
			MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
			WRITE_BYTE(TE_BREAKMODEL);

			// position
			WRITE_COORD(pev->origin.x);
			WRITE_COORD(pev->origin.y);
			WRITE_COORD(pev->origin.z);

			// size
			WRITE_COORD(200);
			WRITE_COORD(200);
			WRITE_COORD(128);

			// velocity
			WRITE_COORD(0);
			WRITE_COORD(0);
			WRITE_COORD(0);

			// randomization
			WRITE_BYTE(200);

			// Model
			WRITE_SHORT(m_GargGibModel); // model id#

			// # of shards
			WRITE_BYTE(50);

			// duration
			WRITE_BYTE(20); // 3.0 seconds

			// flags

			WRITE_BYTE(BREAK_FLESH);
			MESSAGE_END();

			return;
		}
		else
			CBaseMonster::RunTask(pTask);
		break;
	case TASK_FLAME_SWEEP:
		if (gpGlobals->time > m_flWaitFinished)
		{
			FlameOffSound();
			FlameDestroy();
			TaskComplete();
			FlameControls(0, 0);
			SetBoneController(0, 0);
			SetBoneController(1, 0);
		}
		else
		{
			bool cancel = false;

			Vector angles = g_vecZero;

			FlameUpdate();

			Vector org = pev->origin;
			org.z += 64;
			Vector dir = g_vecZero;

			if (m_pCine) // LRC- are we obeying a scripted_action?
			{
				if (m_hTargetEnt != 0 && m_hTargetEnt != m_pGoalEnt)
				{
					dir = m_hTargetEnt->BodyTarget(org) - org;
				}
				else
				{
					UTIL_MakeVectors(pev->angles);
					dir = gpGlobals->v_forward;
				}
			}
			else
			{
				CBaseEntity* pEnemy = m_hEnemy;
				if (pEnemy)
				{
					dir = pEnemy->BodyTarget(org) - org;
				}
			}
			if (dir != g_vecZero)
			{
				angles = UTIL_VecToAngles(dir);
				angles.x = -angles.x;
				angles.y -= pev->angles.y;
				if (dir.Length() > 400)
					cancel = true;
			}
			if (fabs(angles.y) > 60)
				cancel = true;

			if (cancel)
			{
				m_flWaitFinished -= 0.5f;
				m_flameTime -= 0.5f;
			}
			// FlameControls( angles.x + 2.0f * sin( gpGlobals->time * 8.0f ), angles.y + 28.0f * sin( gpGlobals->time * 8.5f ) );
			FlameControls(angles.x, angles.y);
		}
		break;
	default:
		CBaseMonster::RunTask(pTask);
		break;
	}
}

float CGargantua::DefaultHealth()
{
	return gSkillData.gargantuaHealth;
}

float CGargantua::FireAttackDamage()
{
	return gSkillData.gargantuaDmgFire;
}

float CGargantua::StompAttackDamage()
{
	return gSkillData.gargantuaDmgStomp;
}

const char* CGargantua::DefaultModel()
{
	return "models/garg.mdl";
}

const char* CGargantua::EyeSprite()
{
	return GARG_EYE_SPRITE_NAME;
}

float CGargantua::EyeScale()
{
	return 1.0f;
}

const char* CGargantua::StompSprite()
{
	return GARG_STOMP_SPRITE_NAME;
}

Vector CGargantua::EyeColor()
{
	return Vector(255, 255, 255);
}

int CGargantua::MaxEyeBrightness()
{
	return 200;
}

void CGargantua::FootEffect()
{
	UTIL_ScreenShake(pev->origin, 4.0, 3.0, 1.0, 750);
	EMIT_SOUND_DYN(edict(), CHAN_BODY, RANDOM_SOUND_ARRAY(pFootSounds), 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG(-10, 10));
}

void CGargantua::StompEffect()
{
	UTIL_ScreenShake(pev->origin, 12.0, 100.0, 2.0, 1000);
	EMIT_SOUND_DYN(edict(), CHAN_WEAPON, RANDOM_SOUND_ARRAY(pStompSounds), 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG(-10, 10));
}

float CGargantua::FlameLength()
{
	return GARG_FLAME_LENGTH;
}

int CGargantua::BigFlameScale()
{
	return 240;
}

int CGargantua::SmallFlameScale()
{
	return 140;
}

void CGargantua::PrecacheSounds()
{
	PRECACHE_SOUND_ARRAY(pAttackHitSounds);
	PRECACHE_SOUND_ARRAY(pBeamAttackSounds);
	PRECACHE_SOUND_ARRAY(pAttackMissSounds);
	PRECACHE_SOUND_ARRAY(pRicSounds);
	PRECACHE_SOUND_ARRAY(pFootSounds);
	// PRECACHE_SOUND_ARRAY(pIdleSounds);
	PRECACHE_SOUND_ARRAY(pAlertSounds);
	// PRECACHE_SOUND_ARRAY(pPainSounds);
	// PRECACHE_SOUND_ARRAY(pAttackSounds);
	PRECACHE_SOUND_ARRAY(pStompSounds);
	// PRECACHE_SOUND_ARRAY(pBreatheSounds);
}

void CGargantua::BreatheSound()
{
	if (m_breatheTime <= gpGlobals->time)
		SENTENCEG_PlayRndSz(ENT(pev), "GA_BREATHE", 1, ATTN_NORM, 0, PITCH_NORM + RANDOM_LONG(-10, 10));
}

void CGargantua::AttackSound()
{
	SENTENCEG_PlayRndSz(ENT(pev), "GA_ATTACK", 1, ATTN_GARG, 0, PITCH_NORM);
}

float CGargantua::FlameTimeDivider()
{
	return 1.0;
}

Vector CGargantua::StompAttackStartVec()
{
	return pev->origin + Vector(0, 0, 60) + 35 * gpGlobals->v_forward;
}

#define SF_SMOKER_ACTIVE 1
#define SF_SMOKER_REPEATABLE 4
#define SF_SMOKER_DIRECTIONAL 8
#define SF_SMOKER_MAXHEALTH_SET (1 << 24)

class CSmoker : public CBaseEntity
{
public:
	void Spawn() override;
	void Think() override;
};

LINK_ENTITY_TO_CLASS(env_smoker, CSmoker);

void CSmoker::Spawn()
{
	pev->movetype = MOVETYPE_NONE;
	pev->nextthink = gpGlobals->time;
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->effects |= EF_NODRAW;
	pev->angles = g_vecZero;
}


void CSmoker::Think()
{
	// lots of smoke
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_SMOKE);
	WRITE_COORD(pev->origin.x + RANDOM_FLOAT(-pev->dmg, pev->dmg));
	WRITE_COORD(pev->origin.y + RANDOM_FLOAT(-pev->dmg, pev->dmg));
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_sModelIndexSmoke);
	WRITE_BYTE(RANDOM_LONG(pev->scale, pev->scale * 1.1));
	WRITE_BYTE(RANDOM_LONG(8, 14)); // framerate
	MESSAGE_END();

	pev->health--;
	if (pev->health > 0)
		pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.1, 0.2);
	else
		UTIL_Remove(this);
}

void CSpiral::Spawn()
{
	pev->movetype = MOVETYPE_NONE;
	pev->nextthink = gpGlobals->time;
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->effects |= EF_NODRAW;
	pev->angles = g_vecZero;
}


CSpiral* CSpiral::Create(const Vector& origin, float height, float radius, float duration)
{
	if (duration <= 0)
		return NULL;

	CSpiral* pSpiral = GetClassPtr((CSpiral*)NULL);
	pSpiral->Spawn();
	pSpiral->pev->dmgtime = pSpiral->pev->nextthink;
	pSpiral->pev->origin = origin;
	pSpiral->pev->scale = radius;
	pSpiral->pev->dmg = height;
	pSpiral->pev->speed = duration;
	pSpiral->pev->health = 0;
	pSpiral->pev->angles = g_vecZero;

	return pSpiral;
}

#define SPIRAL_INTERVAL 0.1 // 025

void CSpiral::Think()
{
	float time = gpGlobals->time - pev->dmgtime;

	while (time > SPIRAL_INTERVAL)
	{
		Vector position = pev->origin;
		Vector direction = Vector(0, 0, 1);

		float fraction = 1.0 / pev->speed;

		float radius = (pev->scale * pev->health) * fraction;

		position.z += (pev->health * pev->dmg) * fraction;
		pev->angles.y = (pev->health * 360 * 8) * fraction;
		UTIL_MakeVectors(pev->angles);
		position = position + gpGlobals->v_forward * radius;
		direction = (direction + gpGlobals->v_forward).Normalize();

		StreakSplash(position, Vector(0, 0, 1), RANDOM_LONG(8, 11), 20, RANDOM_LONG(50, 150), 400);

		// Jeez, how many counters should this take ? :)
		pev->dmgtime += SPIRAL_INTERVAL;
		pev->health += SPIRAL_INTERVAL;
		time -= SPIRAL_INTERVAL;
	}

	pev->nextthink = gpGlobals->time;

	if (pev->health >= pev->speed)
		UTIL_Remove(this);
}


// HACKHACK Cut and pasted from explode.cpp
void SpawnExplosion(Vector center, float randomRange, float time, int magnitude)
{
	KeyValueData kvd;
	char buf[128];

	center.x += RANDOM_FLOAT(-randomRange, randomRange);
	center.y += RANDOM_FLOAT(-randomRange, randomRange);

	CBaseEntity* pExplosion = CBaseEntity::Create("env_explosion", center, g_vecZero, NULL);
	sprintf(buf, "%3d", magnitude);
	kvd.szKeyName = "iMagnitude";
	kvd.szValue = buf;
	pExplosion->KeyValue(&kvd);
	pExplosion->pev->spawnflags |= SF_ENVEXPLOSION_NODAMAGE;

	pExplosion->Spawn();
	pExplosion->SetThink(&CBaseEntity::SUB_CallUseToggle);
	pExplosion->pev->nextthink = gpGlobals->time + time;
}

class CBabyGargantua : public CGargantua
{
public:
	void Precache()
	{
		PrecacheImpl();
	}
	void SetYawSpeed() override;
	void StartTask(Task_t* pTask) override;
	void RunTask(Task_t* pTask);
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	void DeathSound() override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override;
	void SetObjectCollisionBox() override
	{
		pev->absmin = pev->origin + Vector(-32, -32, 0);
		pev->absmax = pev->origin + Vector(32, 32, 100);
	}

	static const char* pBeamAttackSounds[];
	static const char* pFootSounds[];
	static const char* pIdleSounds[];
	static const char* pAlertSounds[];
	static const char* pPainSounds[];
	static const char* pDeathSounds[];
	static const char* pAttackSounds[];
	static const char* pStompSounds[];
	static const char* pBreatheSounds[];

protected:
	void PrecacheSounds();
	float DefaultHealth();
	float FireAttackDamage();
	float StompAttackDamage();
	const char* DefaultModel();
	const char* EyeSprite();
	float EyeScale();
	Vector EyeColor();
	const char* StompSprite();
	int MaxEyeBrightness();
	void FootEffect();
	void StompEffect();
	float FlameLength();
	int BigFlameScale();
	int SmallFlameScale();
	void BreatheSound();
	void AttackSound();
	float FlameTimeDivider();
	Vector StompAttackStartVec();
};

LINK_ENTITY_TO_CLASS(monster_babygarg, CBabyGargantua)

const char* CBabyGargantua::pBeamAttackSounds[] =
	{
		"garg/gar_flameoff1.wav",
		"garg/gar_flameon1.wav",
		"garg/gar_flamerun1.wav",
};

const char* CBabyGargantua::pFootSounds[] =
	{
		"babygarg/gar_step1.wav",
		"babygarg/gar_step2.wav",
};

const char* CBabyGargantua::pIdleSounds[] =
	{
		"babygarg/gar_idle1.wav",
		"babygarg/gar_idle2.wav",
		"babygarg/gar_idle3.wav",
		"babygarg/gar_idle4.wav",
		"babygarg/gar_idle5.wav",
};

const char* CBabyGargantua::pAttackSounds[] =
	{
		"babygarg/gar_attack1.wav",
		"babygarg/gar_attack2.wav",
		"babygarg/gar_attack3.wav",
};

const char* CBabyGargantua::pAlertSounds[] =
	{
		"babygarg/gar_alert1.wav",
		"babygarg/gar_alert2.wav",
		"babygarg/gar_alert3.wav",
};

const char* CBabyGargantua::pPainSounds[] =
	{
		"babygarg/gar_pain1.wav",
		"babygarg/gar_pain2.wav",
		"babygarg/gar_pain3.wav",
};

const char* CBabyGargantua::pDeathSounds[] =
	{
		"babygarg/gar_die1.wav",
		"babygarg/gar_die2.wav",
};

const char* CBabyGargantua::pStompSounds[] =
	{
		"babygarg/gar_stomp1.wav",
};

const char* CBabyGargantua::pBreatheSounds[] =
	{
		"babygarg/gar_breathe1.wav",
		"babygarg/gar_breathe2.wav",
		"babygarg/gar_breathe3.wav",
};

void CBabyGargantua::PrecacheSounds()
{
	PRECACHE_SOUND_ARRAY(pAttackHitSounds);
	PRECACHE_SOUND_ARRAY(pBeamAttackSounds);
	PRECACHE_SOUND_ARRAY(pAttackMissSounds);
	PRECACHE_SOUND_ARRAY(pFootSounds);
	PRECACHE_SOUND_ARRAY(pIdleSounds);
	PRECACHE_SOUND_ARRAY(pAlertSounds);
	PRECACHE_SOUND_ARRAY(pPainSounds);
	PRECACHE_SOUND_ARRAY(pDeathSounds);
	PRECACHE_SOUND_ARRAY(pAttackSounds);
	PRECACHE_SOUND_ARRAY(pStompSounds);
	PRECACHE_SOUND_ARRAY(pBreatheSounds);
}

void CBabyGargantua::StartTask(Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_DIE:
		CBaseMonster::StartTask(pTask);
		break;
	default:
		CGargantua::StartTask(pTask);
		break;
	}
}

void CBabyGargantua::RunTask(Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_DIE:
		CBaseMonster::RunTask(pTask);
		break;
	default:
		CGargantua::RunTask(pTask);
		break;
	}
}

void CBabyGargantua::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case GARG_AE_SLASH_LEFT:
		HandleSlashAnim(gSkillData.babygargDmgSlash, 20, 80);
		break;
	case BABYGARG_AE_KICK:
	{
		CBaseEntity* pHurt = GargantuaCheckTraceHullAttack(GARG_ATTACKDIST, gSkillData.babygargDmgSlash, DMG_SLASH);
		if (pHurt)
		{
			if (pHurt->pev->flags & (FL_MONSTER | FL_CLIENT))
			{
				pHurt->pev->punchangle.x = 20;
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_up * 100 + gpGlobals->v_forward * 200;
			}
			EMIT_SOUND_DYN(edict(), CHAN_WEAPON, RANDOM_SOUND_ARRAY(pAttackHitSounds), 1.0, ATTN_NORM, 0, 50 + RANDOM_LONG(0, 15));
		}
		else // Play a random attack miss sound
			EMIT_SOUND_DYN(edict(), CHAN_WEAPON, RANDOM_SOUND_ARRAY(pAttackMissSounds), 1.0, ATTN_NORM, 0, 50 + RANDOM_LONG(0, 15));
	}
	break;
	default:
		CGargantua::HandleAnimEvent(pEvent);
		break;
	}
}

void CBabyGargantua::DeathSound()
{
	EMIT_SOUND(ENT(pev), CHAN_VOICE, RANDOM_SOUND_ARRAY(pDeathSounds), 1.0, ATTN_NORM);
}

float CBabyGargantua::DefaultHealth()
{
	return gSkillData.babygargHealth;
}

float CBabyGargantua::FireAttackDamage()
{
	return gSkillData.babygargDmgFire;
}

float CBabyGargantua::StompAttackDamage()
{
	return gSkillData.babygargDmgStomp;
}

const char* CBabyGargantua::DefaultModel()
{
	return "models/babygarg.mdl";
}

const char* CBabyGargantua::EyeSprite()
{
	return "sprites/flare3.spr";
}

float CBabyGargantua::EyeScale()
{
	return 0.5;
}

const char* CBabyGargantua::StompSprite()
{
	return "sprites/flare3.spr";
}

Vector CBabyGargantua::EyeColor()
{
	return Vector(225, 170, 80);
}

int CBabyGargantua::MaxEyeBrightness()
{
	return 255;
}

bool CBabyGargantua::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

void CBabyGargantua::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	ALERT(at_aiconsole, "CBabyGargantua::TraceAttack\n");

	if (!IsAlive())
	{
		CBaseMonster::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
		return;
	}

	if (m_painSoundTime < gpGlobals->time)
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, RANDOM_SOUND_ARRAY(pPainSounds), 1.0, ATTN_GARG, 0, PITCH_NORM);
		m_painSoundTime = gpGlobals->time + RANDOM_FLOAT(2.5, 4);
	}

	CBaseMonster::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
}

void CBabyGargantua::FootEffect()
{
	UTIL_ScreenShake(pev->origin, 2.0, 3.0, 1.0, 400);
	EMIT_SOUND_DYN(edict(), CHAN_BODY, RANDOM_SOUND_ARRAY(pFootSounds), 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG(-10, 10));
}

void CBabyGargantua::StompEffect()
{
	UTIL_ScreenShake(pev->origin, 6.0, 100.0, 1.5, 600);
	EMIT_SOUND_DYN(edict(), CHAN_WEAPON, RANDOM_SOUND_ARRAY(pStompSounds), 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG(-10, 10));
}

float CBabyGargantua::FlameLength()
{
	return GARG_FLAME_LENGTH / 2;
}

int CBabyGargantua::BigFlameScale()
{
	return 120;
}

int CBabyGargantua::SmallFlameScale()
{
	return 70;
}

void CBabyGargantua::BreatheSound()
{
	if (m_breatheTime <= gpGlobals->time)
		EMIT_SOUND_DYN(edict(), CHAN_VOICE, RANDOM_SOUND_ARRAY(pBreatheSounds), 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG(-10, 10));
}

void CBabyGargantua::AttackSound()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, RANDOM_SOUND_ARRAY(pAttackSounds), 1.0, ATTN_GARG, 0, PITCH_NORM);
}

float CBabyGargantua::FlameTimeDivider()
{
	return 1.5;
}

Vector CBabyGargantua::StompAttackStartVec()
{
	return pev->origin + Vector(0, 0, 30) + 35 * gpGlobals->v_forward;
}

void CBabyGargantua::SetYawSpeed()
{
	int ys;

	switch (m_Activity)
	{
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 180;
		break;
	default:
		ys = 100;
		break;
	}

	pev->yaw_speed = ys;
}