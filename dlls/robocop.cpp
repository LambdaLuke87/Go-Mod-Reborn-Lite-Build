/***
*
*	Robocop (originally from Poke646) - port of the Featureful SDK monster to Go-Mod Reborn SDK
*
****/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "soundent.h"
#include "animation.h"
#include "effects.h"
#include "explode.h"
#include "customentity.h"
#include "decals.h"
#include "weapons.h" // g_sModelIndexFireball

//=========================================================
// Fire trail: short-lived bouncing fireball emitter spawned when Robocop explodes
//=========================================================
class CFireTrail : public CBaseEntity
{
public:
	void Spawn() override;
	void Think() override;
	void Touch(CBaseEntity* pOther) override;
	int ObjectCaps() override { return FCAP_DONT_SAVE; }

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	int m_spriteScale; // what's the exact fireball sprite scale?
};

LINK_ENTITY_TO_CLASS(fire_trail, CFireTrail);

TYPEDESCRIPTION CFireTrail::m_SaveData[] =
	{
		DEFINE_FIELD(CFireTrail, m_spriteScale, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CFireTrail, CBaseEntity);

void CFireTrail::Spawn()
{
	pev->velocity = RANDOM_FLOAT(100.0f, 150.0f) * pev->angles;
	if (RANDOM_LONG(0, 1))
		pev->velocity.x += RANDOM_FLOAT(-300.0f, -100.0f);
	else
		pev->velocity.x += RANDOM_FLOAT(100.0f, 300.0f);

	if (RANDOM_LONG(0, 1))
		pev->velocity.y += RANDOM_FLOAT(-300.0f, -100.0f);
	else
		pev->velocity.y += RANDOM_FLOAT(100.0f, 300.0f);

	if (pev->velocity.z >= 0)
		pev->velocity.z += 200.0f;
	else
		pev->velocity.z -= 200.0f;

	m_spriteScale = RANDOM_LONG(7, 13);
	pev->movetype = MOVETYPE_BOUNCE;
	pev->gravity = 0.5f;
	pev->nextthink = gpGlobals->time + 0.1f;
	pev->solid = SOLID_NOT;
	SET_MODEL(edict(), "models/grenade.mdl"); // Need a model, just use the grenade, we don't draw it anyway
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->effects |= EF_NODRAW;
	pev->speed = RANDOM_FLOAT(0.5f, 1.5f);
	pev->maxspeed = pev->speed;

	pev->angles = g_vecZero;
}

void CFireTrail::Think()
{
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_sModelIndexFireball);
	WRITE_BYTE((int)(m_spriteScale * pev->speed)); // scale * 10
	WRITE_BYTE(15);									// framerate
	WRITE_BYTE(TE_EXPLFLAG_NOSOUND);
	MESSAGE_END();

	pev->speed -= 0.1f;

	if (pev->speed > 0)
		pev->nextthink = gpGlobals->time + 0.1f;
	else
		UTIL_Remove(this);

	pev->flags &= ~FL_ONGROUND;
}

void CFireTrail::Touch(CBaseEntity* pOther)
{
	if ((pev->flags & FL_ONGROUND) != 0)
		pev->velocity = pev->velocity * 0.1f;
	else
		pev->velocity = pev->velocity * 0.6f;

	if ((pev->velocity.x * pev->velocity.x + pev->velocity.y * pev->velocity.y) < 10.0f)
		pev->speed = 0;
}

// Defined in gargantua.cpp
void SpawnExplosion(Vector center, float randomRange, float time, int magnitude);

//=========================================================
// Robocop
//=========================================================
#define ROBOCOP_DAMAGE (DMG_ENERGYBEAM | DMG_CRUSH | DMG_MORTAR | DMG_BLAST)

#define ROBOCOP_DEATH_DURATION 2.1f

#define LF_ROBOCOP_LASER 1
#define LF_ROBOCOP_BEAMSPOT 2
#define LF_ROBOCOP_BEAM 4
#define LF_ROBOCOP_LOWBRIGHTNESS 8
#define LF_ROBOCOP_HIGHBRIGHTNESS 16
#define LF_ROBOCOP_FULLBRIGHTNESS 32

#define ROBOCOP_AE_RIGHT_FOOT 0x03
#define ROBOCOP_AE_LEFT_FOOT 0x04
#define ROBOCOP_AE_FIST 0x05

#define ROBOCOP_EYE_AMT 255
#define ROBOCOP_BEAM_AMT 255
#define ROBOCOP_SPOT_AMT 255

enum
{
	SCHED_ROBOCOP_LASERFAIL = LAST_COMMON_SCHEDULE + 1,
};

enum
{
	TASK_ROBOCOP_LASER_SOUND = LAST_COMMON_TASK + 1,
	TASK_ROBOCOP_LASER_CHARGE,
	TASK_ROBOCOP_LASER_ON,
	TASK_ROBOCOP_MORTAR_SPAWN,
	TASK_ROBOCOP_LASER_OFF
};

class CRoboCop : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void RemoveSpriteEffects();
	void SetYawSpeed() override;
	int Classify() override;
	const char* DefaultDisplayName() { return "Robocop"; }
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	void SetActivity(Activity NewActivity) override;

	bool CheckMeleeAttack1(float flDot, float flDist) override;
	bool CheckMeleeAttack2(float flDot, float flDist) override { return false; }
	bool CheckRangeAttack1(float flDot, float flDist) override;
	bool CheckRangeAttack2(float flDot, float flDist) override { return false; }

	void SetObjectCollisionBox() override
	{
		pev->absmin = pev->origin + Vector(-80, -80, 0);
		pev->absmax = pev->origin + Vector(80, 80, 214);
	}

	void PrescheduleThink() override;

	void Killed(entvars_t* pevAttacker, int iGib) override;

	Schedule_t* GetScheduleOfType(int Type) override;
	void StartTask(Task_t* pTask) override;
	void RunTask(Task_t* pTask) override;

	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;

	void FistAttack();
	void CreateLaser();
	void ChangeLaserState();
	void HeadControls(float angleX, float angleY, bool zeropoint);

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	CUSTOM_SCHEDULES;

	CSprite* m_pLaserPointer;
	CBeam* m_pBeam;
	CSprite* m_pBeamSpot;
	int m_iLaserFlags;
	float m_flHeadX;
	float m_flHeadY;
	float m_flLaserTime;
	float m_flFistTime;
	float m_flSparkTime;
	Vector m_vecAimPos;
	bool m_vecAimPosSet;

	int m_renderAmtAlive;
	int m_renderFxAlive;
	Vector m_renderColorAlive;
	bool m_renderAliveSaved;

	// not saved, refilled by Precache() (called on restore too) / only used to detect schedule changes
	int m_RobocopGibModel;
	int m_iShockwaveSprite;
	Schedule_t* m_pLastSchedule;

private:
	void EmitSparkSound();
};

LINK_ENTITY_TO_CLASS(monster_robocop, CRoboCop);

TYPEDESCRIPTION CRoboCop::m_SaveData[] =
	{
		DEFINE_FIELD(CRoboCop, m_pLaserPointer, FIELD_CLASSPTR),
		DEFINE_FIELD(CRoboCop, m_pBeam, FIELD_CLASSPTR),
		DEFINE_FIELD(CRoboCop, m_pBeamSpot, FIELD_CLASSPTR),
		DEFINE_FIELD(CRoboCop, m_iLaserFlags, FIELD_INTEGER),
		DEFINE_FIELD(CRoboCop, m_flLaserTime, FIELD_TIME),
		DEFINE_FIELD(CRoboCop, m_flFistTime, FIELD_TIME),
		DEFINE_FIELD(CRoboCop, m_flSparkTime, FIELD_TIME),
		DEFINE_FIELD(CRoboCop, m_flHeadX, FIELD_FLOAT),
		DEFINE_FIELD(CRoboCop, m_flHeadY, FIELD_FLOAT),
		DEFINE_FIELD(CRoboCop, m_vecAimPos, FIELD_POSITION_VECTOR),
		DEFINE_FIELD(CRoboCop, m_vecAimPosSet, FIELD_BOOLEAN),
		DEFINE_FIELD(CRoboCop, m_renderAmtAlive, FIELD_INTEGER),
		DEFINE_FIELD(CRoboCop, m_renderFxAlive, FIELD_INTEGER),
		DEFINE_FIELD(CRoboCop, m_renderColorAlive, FIELD_VECTOR),
		DEFINE_FIELD(CRoboCop, m_renderAliveSaved, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CRoboCop, CBaseMonster);

//=========================================================
// Schedules
//=========================================================
Task_t tlRoboCopLaser[] =
	{
		{TASK_SET_FAIL_SCHEDULE, (float)SCHED_ROBOCOP_LASERFAIL},
		{TASK_STOP_MOVING, 0.0f},
		{TASK_FACE_ENEMY, 0.0f},
		{TASK_SET_ACTIVITY, (float)ACT_RANGE_ATTACK1},
		{TASK_ROBOCOP_LASER_CHARGE, 2.0f},
		{TASK_ROBOCOP_LASER_SOUND, 0.0f},
		{TASK_ROBOCOP_LASER_ON, 1.0f},
		{TASK_ROBOCOP_MORTAR_SPAWN, 0.1f},
		{TASK_ROBOCOP_LASER_OFF, 1.0f}};

Schedule_t slRoboCopLaser[] =
	{
		{tlRoboCopLaser,
			ARRAYSIZE(tlRoboCopLaser),
			bits_COND_TASK_FAILED |
				bits_COND_CAN_MELEE_ATTACK1 |
				bits_COND_HEAVY_DAMAGE,
			0,
			"RoboCopLaser"}};

Task_t tlRoboCopLaserFail[] =
	{
		{TASK_ROBOCOP_LASER_OFF, 0.0f},
		{TASK_SET_ACTIVITY, (float)ACT_IDLE},
		{TASK_WAIT, 1.0f},
		{TASK_WAIT_PVS, 0.0f}};

Schedule_t slRoboCopLaserFail[] =
	{
		{tlRoboCopLaserFail,
			ARRAYSIZE(tlRoboCopLaserFail),
			bits_COND_CAN_ATTACK,
			0,
			"RoboCopLaserFail"}};

DEFINE_CUSTOM_SCHEDULES(CRoboCop){
	slRoboCopLaser,
	slRoboCopLaserFail};

IMPLEMENT_CUSTOM_SCHEDULES(CRoboCop, CBaseMonster);

//=========================================================
// Local effect helper: TE_BEAMCYLINDER with framerate
//=========================================================
static void RoboCopSendBeamWave(const Vector& origin, float radius, int modelIndex, int framerate, float life,
	int width, int r, int g, int b, int brightness)
{
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, origin);
	WRITE_BYTE(TE_BEAMCYLINDER);
	WRITE_COORD(origin.x);
	WRITE_COORD(origin.y);
	WRITE_COORD(origin.z);
	WRITE_COORD(origin.x);
	WRITE_COORD(origin.y);
	WRITE_COORD(origin.z + radius);
	WRITE_SHORT(modelIndex);
	WRITE_BYTE(0); // startframe
	WRITE_BYTE(framerate);
	WRITE_BYTE((int)(life * 10));
	WRITE_BYTE(width);
	WRITE_BYTE(0); // noise
	WRITE_BYTE(r);
	WRITE_BYTE(g);
	WRITE_BYTE(b);
	WRITE_BYTE(brightness);
	WRITE_BYTE(0); // speed
	MESSAGE_END();
}

void CRoboCop::FistAttack()
{
	Vector vecDist;
	float flDist, flAdjustedDamage;

	UTIL_MakeVectors(pev->angles);
	Vector vecSrc = pev->origin + 12 * gpGlobals->v_right + 95 * gpGlobals->v_forward;

	const float shockWaveRadius = gSkillData.robocopSwRadius;

	// blast circles: outer, middle, inner
	static const struct
	{
		float life;
		int r, g, b;
	} waves[] = {
		{0.2f, 101, 133, 221},
		{0.3f, 67, 85, 255},
		{0.4f, 62, 33, 211},
	};

	for (size_t i = 0; i < ARRAYSIZE(waves); i++)
	{
		RoboCopSendBeamWave(vecSrc + Vector(0, 0, 16), shockWaveRadius / ((i + 1) * 0.2f), m_iShockwaveSprite,
			10, waves[i].life, 32, waves[i].r, waves[i].g, waves[i].b, 255);
	}

	CBaseEntity* pEntity = NULL;

	// iterate on all entities in the vicinity.
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, pev->origin, shockWaveRadius)) != NULL)
	{
		if (pEntity->pev->takedamage != DAMAGE_NO)
		{
			// Robocop does not take damage from it's own attacks.
			if (pEntity != this)
			{
				vecDist = pEntity->Center() - vecSrc;
				flDist = V_max(0, shockWaveRadius - vecDist.Length());

				flDist = flDist / shockWaveRadius;

				if (!FVisible(pEntity))
				{
					if (pEntity->IsPlayer())
					{
						// if this entity is a client, and is not in full view, inflict half damage. We do this so that players still
						// take the residual damage if they don't totally leave the effective radius. We restrict it to clients
						// so that monsters in other parts of the level don't take the damage and get pissed.
						flDist *= 0.5f;
					}
					else if (!FClassnameIs(pEntity->pev, "func_breakable") && !FClassnameIs(pEntity->pev, "func_pushable"))
					{
						// do not hurt nonclients through walls, but allow damage to be done to breakables
						flDist = 0;
					}
				}

				flAdjustedDamage = gSkillData.robocopDmgFist * flDist;

				if (flAdjustedDamage > 0)
				{
					pEntity->TakeDamage(pev, pev, flAdjustedDamage, DMG_SONIC);
				}

				if (pEntity->IsPlayer())
				{
					vecDist = vecDist.Normalize();
					vecDist.x = vecDist.x * flDist * 600.0f;
					vecDist.y = vecDist.y * flDist * 600.0f;
					vecDist.z = flDist * 450.0f;
					pEntity->pev->velocity = vecDist + pEntity->pev->velocity;
					pEntity->pev->punchangle.x = 5;
				}
			}
		}
	}

	UTIL_ScreenShake(pev->origin, 12.0f, 100.0f, 2.0f, 1000);
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "robocop/rc_fist.wav", 1.0f, ATTN_NORM, 0, RANDOM_LONG(90, 110));
}

void CRoboCop::CreateLaser()
{
	m_pLaserPointer = CSprite::SpriteCreate("sprites/gargeye1.spr", pev->origin, false);
	if (m_pLaserPointer)
	{
		m_pLaserPointer->SetTransparency(kRenderTransAdd, 255, 255, 255, ROBOCOP_EYE_AMT, kRenderFxNone);
		m_pLaserPointer->SetScale(0.5f);
		m_pLaserPointer->SetAttachment(edict(), 1);
		m_pLaserPointer->SetBrightness(0);
	}

	m_pBeamSpot = CSprite::SpriteCreate("sprites/gargeye1.spr", pev->origin, false);
	if (m_pBeamSpot)
	{
		m_pBeamSpot->SetTransparency(kRenderTransAdd, 255, 255, 255, ROBOCOP_SPOT_AMT, kRenderFxNone);
		m_pBeamSpot->SetScale(0.3f);
		m_pBeamSpot->SetBrightness(0);
	}

	m_pBeam = CBeam::BeamCreate("sprites/smoke.spr", 32);
	if (m_pBeam)
	{
		m_pBeam->SetColor(255, 0, 0);
		m_pBeam->SetNoise(0);
		m_pBeam->SetScrollRate(15);
		m_pBeam->PointEntInit(pev->origin, entindex());
		m_pBeam->SetEndAttachment(1);
		m_pBeam->SetBrightness(0);
	}

	ChangeLaserState();
}

void CRoboCop::ChangeLaserState()
{
	float time;
	float brightnessFraction;

	if (m_pBeam)
		m_pBeam->SetEndAttachment(1);

	if (0 == m_iLaserFlags)
	{
		if (m_pBeam)
			m_pBeam->SetBrightness(0);

		if (m_pLaserPointer)
			m_pLaserPointer->SetBrightness(0);

		if (m_pBeamSpot)
			m_pBeamSpot->SetBrightness(0);

		return;
	}

	if ((m_iLaserFlags & LF_ROBOCOP_LOWBRIGHTNESS) != 0)
	{
		time = gpGlobals->time;
		brightnessFraction = fabs(sin(time));
	}
	else if ((m_iLaserFlags & LF_ROBOCOP_HIGHBRIGHTNESS) == 0 && (m_iLaserFlags & LF_ROBOCOP_FULLBRIGHTNESS) != 0)
	{
		brightnessFraction = 0.875f;
	}
	else
	{
		time = gpGlobals->time * 5.0f;
		brightnessFraction = fabs(sin(time));
	}

	if ((m_iLaserFlags & LF_ROBOCOP_LASER) != 0)
	{
		if (m_pLaserPointer)
			m_pLaserPointer->SetBrightness(ROBOCOP_EYE_AMT * brightnessFraction);
	}

	if ((m_iLaserFlags & LF_ROBOCOP_BEAM) != 0)
	{
		if (m_pBeam)
			m_pBeam->SetBrightness(ROBOCOP_BEAM_AMT * brightnessFraction);
	}

	if ((m_iLaserFlags & LF_ROBOCOP_BEAMSPOT) != 0)
	{
		if (m_pBeamSpot)
			m_pBeamSpot->SetBrightness(ROBOCOP_SPOT_AMT * brightnessFraction);
	}
}

void CRoboCop::HeadControls(float angleX, float angleY, bool zeropoint)
{
	if (angleY < -180)
		angleY += 360;
	else if (angleY > 180)
		angleY -= 360;

	if (angleY < -45)
		angleY = -45;
	else if (angleY > 45)
		angleY = 45;

	if (zeropoint)
	{
		m_flHeadX = angleX;
		m_flHeadY = angleY;
	}
	else
	{
		m_flHeadX = UTIL_ApproachAngle(angleX, m_flHeadX, 4);
		m_flHeadY = UTIL_ApproachAngle(angleY, m_flHeadY, 8);
	}

	SetBoneController(0, m_flHeadY);
	SetBoneController(1, m_flHeadX);
}

void CRoboCop::PrescheduleThink()
{
	// Replacement for OnChangeSchedule(): when the schedule changes while the beam is on, switch the laser off
	if (m_pSchedule != m_pLastSchedule)
	{
		m_pLastSchedule = m_pSchedule;
		if ((m_iLaserFlags & LF_ROBOCOP_BEAM) != 0)
		{
			HeadControls(0.0f, 0.0f, true);
			m_iLaserFlags = 0;
			ChangeLaserState();
		}
	}

	if (m_flLaserTime <= gpGlobals->time && m_iLaserFlags != (LF_ROBOCOP_LASER | LF_ROBOCOP_LOWBRIGHTNESS))
	{
		m_iLaserFlags = 0;
		ChangeLaserState();
		m_iLaserFlags = (LF_ROBOCOP_LASER | LF_ROBOCOP_LOWBRIGHTNESS);
	}

	ChangeLaserState();

	CBaseMonster::PrescheduleThink();
}

void CRoboCop::SetYawSpeed()
{
	int ys;

	switch (m_Activity)
	{
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 180;
		break;
	default:
		ys = 90;
		break;
	}

	pev->yaw_speed = ys;
}

int CRoboCop::Classify()
{
	if (m_AltClass)
		return CLASS_PLAYER_ALLY;

	return CLASS_MACHINE;
}

void CRoboCop::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/robocop.mdl");
	UTIL_SetSize(pev, Vector(-40.0f, -40.0f, 0.0f), Vector(40.0f, 40.0f, 200.0f));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = DONT_BLEED;
	pev->health = gSkillData.robocopHealth;
	pev->max_health = pev->health;
	m_flFieldOfView = -0.7f; // VIEW_FIELD_WIDE
	m_MonsterState = MONSTERSTATE_NONE;
	m_afCapability = 0; // can't open doors

	m_pLaserPointer = nullptr;
	m_pBeam = nullptr;
	m_pBeamSpot = nullptr;
	m_iLaserFlags = 0;
	m_flLaserTime = 0;
	m_flSparkTime = 0;
	m_vecAimPosSet = false;
	m_renderAliveSaved = false;
	m_pLastSchedule = nullptr;

	MonsterInit();

	CreateLaser();

	m_flFistTime = gpGlobals->time + 1.0f;
}

void CRoboCop::Precache()
{
	PRECACHE_MODEL("models/robocop.mdl");

	m_RobocopGibModel = PRECACHE_MODEL("models/metalplategibs.mdl");
	m_iShockwaveSprite = PRECACHE_MODEL("sprites/xbeam3.spr");
	PRECACHE_MODEL("sprites/gargeye1.spr");
	PRECACHE_MODEL("sprites/smoke.spr");

	PRECACHE_SOUND("ambience/sparks.wav");
	PRECACHE_SOUND("robocop/rc_charge.wav");
	PRECACHE_SOUND("robocop/rc_fist.wav");
	PRECACHE_SOUND("robocop/rc_laser.wav");
	PRECACHE_SOUND("robocop/rc_step1.wav");
	PRECACHE_SOUND("robocop/rc_step2.wav");

	// spark sounds, as in the spark_shower entity
	PRECACHE_SOUND("buttons/spark1.wav");
	PRECACHE_SOUND("buttons/spark2.wav");
	PRECACHE_SOUND("buttons/spark3.wav");
	PRECACHE_SOUND("buttons/spark4.wav");
	PRECACHE_SOUND("buttons/spark5.wav");
	PRECACHE_SOUND("buttons/spark6.wav");

	UTIL_PrecacheOther("fire_trail");
	UTIL_PrecacheOther("spark_shower");
}

// UpdateOnRemove() is not virtual in this SDK, so laser effects are cleaned in Killed()
void CRoboCop::RemoveSpriteEffects()
{
	if (m_pLaserPointer)
	{
		UTIL_Remove(m_pLaserPointer);
		m_pLaserPointer = nullptr;
	}
	if (m_pBeam)
	{
		UTIL_Remove(m_pBeam);
		m_pBeam = nullptr;
	}
	if (m_pBeamSpot)
	{
		UTIL_Remove(m_pBeamSpot);
		m_pBeamSpot = nullptr;
	}
}

void CRoboCop::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	// only robocop-specific damage types are kept
	bitsDamageType &= ROBOCOP_DAMAGE;

	if (IsAlive() && (bitsDamageType & ROBOCOP_DAMAGE) == 0)
	{
		if (pev->dmgtime != gpGlobals->time || (RANDOM_LONG(0, 100) < 20))
		{
			UTIL_Ricochet(ptr->vecEndPos, RANDOM_FLOAT(0.5f, 1.5f));
			pev->dmgtime = gpGlobals->time;
		}

		flDamage = 0.0f;
	}

	CBaseMonster::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
}

bool CRoboCop::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (IsAlive())
	{
		if ((bitsDamageType & ROBOCOP_DAMAGE) == 0)
			flDamage *= 0.01f;

		if ((bitsDamageType & DMG_BLAST) != 0)
			SetConditions(bits_COND_LIGHT_DAMAGE);
	}
	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

void CRoboCop::Killed(entvars_t* pevAttacker, int iGib)
{
	RemoveSpriteEffects();
	CBaseMonster::Killed(pevAttacker, GIB_NEVER);
}

bool CRoboCop::CheckMeleeAttack1(float flDot, float flDist)
{
	if (m_flFistTime <= gpGlobals->time)
	{
		return flDist <= gSkillData.robocopSwRadius && flDot >= 0.8f;
	}
	return false;
}

bool CRoboCop::CheckRangeAttack1(float flDot, float flDist)
{
	if (m_flLaserTime <= gpGlobals->time)
	{
		if (flDot >= 0.8f && flDist > gSkillData.robocopSwRadius)
		{
			if (flDist < 4096.0f)
				return true;
		}
	}

	return false;
}

void CRoboCop::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case ROBOCOP_AE_RIGHT_FOOT:
	case ROBOCOP_AE_LEFT_FOOT:
		UTIL_ScreenShake(pev->origin, 4.0f, 3.0f, 1.0f, 250.0f);
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, RANDOM_LONG(0, 1) == 0 ? "robocop/rc_step1.wav" : "robocop/rc_step2.wav", 1.0f, ATTN_NORM, 0, RANDOM_LONG(90, 110));
		break;
	case ROBOCOP_AE_FIST:
		FistAttack();
		m_flLaserTime = gpGlobals->time + 2.0f;
		break;
	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

Schedule_t* CRoboCop::GetScheduleOfType(int Type)
{
	switch (Type)
	{
	case SCHED_RANGE_ATTACK1:
		return slRoboCopLaser;

	case SCHED_ROBOCOP_LASERFAIL:
		return slRoboCopLaserFail;
	}

	return CBaseMonster::GetScheduleOfType(Type);
}

void CRoboCop::EmitSparkSound()
{
	static const char* const sparkSounds[] = {
		"buttons/spark1.wav", "buttons/spark2.wav", "buttons/spark3.wav",
		"buttons/spark4.wav", "buttons/spark5.wav", "buttons/spark6.wav"};

	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, sparkSounds[RANDOM_LONG(0, ARRAYSIZE(sparkSounds) - 1)], 0.6f, ATTN_NORM, 0, RANDOM_LONG(95, 105));
}

void CRoboCop::StartTask(Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_ROBOCOP_LASER_CHARGE:
	{
		EMIT_SOUND(ENT(pev), CHAN_BODY, "robocop/rc_charge.wav", 1.0f, ATTN_NORM);
		m_iLaserFlags = (LF_ROBOCOP_LASER | LF_ROBOCOP_HIGHBRIGHTNESS);
		m_flWaitFinished = gpGlobals->time + pTask->flData;
		m_flFistTime = m_flLaserTime = gpGlobals->time + 9.0f;

		ChangeLaserState();
		m_vecAimPosSet = false;
	}
	break;

	case TASK_ROBOCOP_LASER_ON:
	{
		m_iLaserFlags = (LF_ROBOCOP_LASER | LF_ROBOCOP_BEAM | LF_ROBOCOP_BEAMSPOT | LF_ROBOCOP_FULLBRIGHTNESS);
		m_flWaitFinished = gpGlobals->time + pTask->flData;

		if (m_pBeam)
		{
			m_pBeam->pev->origin = m_vecAimPos;
			m_pBeam->SetEndAttachment(1);
			m_pBeam->RelinkBeam();
		}

		if (m_pBeamSpot)
			m_pBeamSpot->pev->origin = m_vecAimPos;

		m_failSchedule = SCHED_NONE;

		ChangeLaserState();
	}
	break;

	case TASK_ROBOCOP_MORTAR_SPAWN:
	{
		ExplosionCreate(m_vecAimPos, g_vecZero, edict(), gSkillData.robocopDmgMortar, true);
		// Note: the Featureful version passed an uninitialized TraceResult as the shake origin here; the explosion position is what was meant.
		UTIL_ScreenShake(m_vecAimPos, 25.0f, 150.0f, 1.0f, 750);
		m_flWaitFinished = gpGlobals->time + pTask->flData;
	}
	break;
	case TASK_ROBOCOP_LASER_SOUND:
		EMIT_SOUND(ENT(pev), CHAN_BODY, "robocop/rc_laser.wav", 1.0f, ATTN_NORM);
		TaskComplete();
		break;
	case TASK_ROBOCOP_LASER_OFF:
	{
		HeadControls(0.0f, 0.0f, true);
		m_iLaserFlags = 0;
		ChangeLaserState();
		m_flFistTime = gpGlobals->time + 2.0f;
		m_flLaserTime = gpGlobals->time + 1.0f;

		// stop the charge sound
		EMIT_SOUND(ENT(pev), CHAN_BODY, "common/null.wav", 1.0f, ATTN_NORM);
		TaskComplete();
	}
	break;
	case TASK_DIE:
		m_flWaitFinished = gpGlobals->time + ROBOCOP_DEATH_DURATION;
		m_iLaserFlags = (LF_ROBOCOP_LASER | LF_ROBOCOP_LOWBRIGHTNESS);

		if (!m_renderAliveSaved)
		{
			m_renderAmtAlive = pev->renderamt;
			m_renderFxAlive = pev->renderfx;
			m_renderColorAlive = pev->rendercolor;
			m_renderAliveSaved = true;
		}

		pev->renderamt = 15;
		pev->renderfx = kRenderFxGlowShell;
		pev->rendercolor = Vector(67, 85, 255);

		pev->health = 0;
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "ambience/sparks.wav", 1.0f, ATTN_NORM);
		m_flSparkTime = gpGlobals->time + 0.3f;
		[[fallthrough]];
	default:
		CBaseMonster::StartTask(pTask);
		break;
	}
}

void CRoboCop::RunTask(Task_t* pTask)
{
	TraceResult tr;

	switch (pTask->iTask)
	{
	case TASK_ROBOCOP_LASER_ON:
	case TASK_ROBOCOP_MORTAR_SPAWN:
		if (gpGlobals->time > m_flWaitFinished)
			TaskComplete();
		break;
	case TASK_ROBOCOP_LASER_CHARGE:
	{
		if (gpGlobals->time > m_flWaitFinished)
			TaskComplete();

		if (m_hEnemy == NULL)
		{
			TaskFail(); // no enemy
			return;
		}

		const Vector enemyPos = m_hEnemy->Center();
		const bool enemyIsFlying = m_hEnemy->pev->movetype == MOVETYPE_FLY;
		Vector targetPos;

		if (!enemyIsFlying)
		{
			UTIL_TraceLine(enemyPos, enemyPos - Vector(0, 0, 4096), ignore_monsters, edict(), &tr);
			targetPos = tr.vecEndPos;
		}
		else
			targetPos = enemyPos;

		Vector vecSrc, vecAngle, vecDir;
		GetAttachment(0, vecSrc, vecAngle);
		vecDir = (targetPos - vecSrc).Normalize();

		UTIL_TraceLine(vecSrc, vecSrc + vecDir * 4096, dont_ignore_monsters, edict(), &tr);
		if (tr.pHit == m_hEnemy->edict())
		{
			m_vecAimPos = targetPos;
		}
		else if (!m_vecAimPosSet || tr.flFraction == 1.0f)
		{
			m_vecAimPos = tr.vecEndPos;
		}
		m_vecAimPosSet = true;

		vecAngle = UTIL_VecToAngles(vecDir);

		vecAngle.y = UTIL_AngleDiff(vecAngle.y, pev->angles.y);
		vecAngle.x = -vecAngle.x;

		if (fabs(vecAngle.y) > 60.0f)
		{
			TaskFail(); // enemy is out of FOV
			return;
		}

		HeadControls(vecAngle.x, vecAngle.y, false);
	}
	break;

	case TASK_DIE:
		if (m_flWaitFinished <= gpGlobals->time)
		{
			if (m_fSequenceFinished && pev->frame >= 255.0f)
			{
				// stop the death sound
				EMIT_SOUND(ENT(pev), CHAN_ITEM, "common/null.wav", 1.0, ATTN_NORM);

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
				WRITE_SHORT(m_RobocopGibModel); // model id#

				// # of shards
				WRITE_BYTE(20);

				// duration
				WRITE_BYTE(20); // 3.0 seconds

				// flags
				WRITE_BYTE(BREAK_METAL);
				MESSAGE_END();

				SpawnExplosion(pev->origin, 70, 0, 150);

				int trailCount = RANDOM_LONG(2, 4);

				for (int i = 0; i < trailCount; i++)
					Create("fire_trail", pev->origin, Vector(0, 0, 1), NULL);

				// Restore original render values
				pev->renderamt = m_renderAmtAlive;
				pev->renderfx = m_renderFxAlive;
				pev->rendercolor = m_renderColorAlive;

				SetBodygroup(0, 1);

				CBaseMonster::RunTask(pTask);
				return;
			}
		}

		if (gpGlobals->time <= m_flSparkTime)
		{
			Vector headLocation;
			Vector headAngle;
			GetAttachment(0, headLocation, headAngle);

			Vector sparkLocation = pev->origin;
			sparkLocation.z += RANDOM_FLOAT(0, 4);
			sparkLocation.x = (pev->origin.x + headLocation.x) * 0.5f + RANDOM_FLOAT(-pev->size.x, pev->size.x) * 0.3;
			sparkLocation.y = (pev->origin.y + headLocation.y) * 0.5f + RANDOM_FLOAT(-pev->size.y, pev->size.y) * 0.3;

			Create("spark_shower", sparkLocation, Vector(0, 0, 1), NULL);
			EmitSparkSound();
			m_flSparkTime = gpGlobals->time + 0.5f;
		}
		[[fallthrough]];
	default:
		CBaseMonster::RunTask(pTask);
		break;
	}
}

void CRoboCop::SetActivity(Activity NewActivity)
{
	CBaseMonster::SetActivity(NewActivity);

	// same ground speed for every activity
	m_flGroundSpeed = 220.0f;
}
