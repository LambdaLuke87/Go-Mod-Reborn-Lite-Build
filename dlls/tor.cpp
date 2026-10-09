/***
*
*	Tor - port of the Featureful SDK monster to Go-Mod: Reborn SDK
*
****/
//=========================================================
// Tor - Xen alien warrior with staff, energy beam,
// ground slam and the ability to summon alien grunts
// Tor is originally from Sven Co-op mod
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "squadmonster.h"
#include "weapons.h"
#include "customentity.h"
#include "soundent.h"
#include "effects.h"
#include "decals.h"

// TODO (inherited from the original):
// range attack if can't reach target
// increase fps of most animations

#define EVENT_SLAM 1
#define EVENT_STAFF_SWING 2
#define EVENT_SHOOT 3
#define EVENT_SUMMON_GRUNT 4
#define EVENT_STAFF_STAB 7
#define EVENT_STEP_RIGHT 10
#define EVENT_STEP_LEFT 11

#define TOR_MELEE_ATTACK_DISTANCE 100
#define TOR_MELEE_ATTACK_CHECK_DISTANCE 88
#define TOR_MELEE_CHASE_DISTANCE 300

#define TOR_SLAM_CHECK_DISTANCE 150 // how close enemies need to be for a slam to be considered
#define TOR_SLAM_ATTACK_RADIUS 300	// radius of the attack
#define TOR_SLAM_ATTACK_ENEMY_COUNT 2 // minimum enemies nearby needed to do a slam

#define TOR_MAX_BEAM_SHOTS 5
#define TOR_SHOOT_RANGE 4096

#define TOR_SUMMON_DISTANCE 256.0f
#define TOR_SUMMON_HEIGHT 80.0f
#define TOR_MAX_ALLOWED_CHILDREN 3

#define TOR_SUMMON_CLASSNAME "monster_alien_grunt"
#define TOR_SUMMON_CLASSNAME_ALT "monster_alien_grunt_alt"
#define TOR_SUMMON_CLASSNAME_ALLIED "monster_alien_grunt_allied"

// Approximate hull of an alien grunt, used to check if there is space for a summoned child
static const Vector TOR_SUMMON_MINS(-32, -32, 0);
static const Vector TOR_SUMMON_MAXS(32, 32, 85);

//=========================================================
// Local effect helpers (replacement for Featureful's SendBeam*/Visual helpers)
//=========================================================

// TE_BEAMPOINTS between two points.
static void TorSendBeamPoints(const Vector& start, const Vector& end, int modelIndex,
	int life10, int width, int noise, int r, int g, int b, int brightness, int scroll)
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, start);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(start.x);
	WRITE_COORD(start.y);
	WRITE_COORD(start.z);
	WRITE_COORD(end.x);
	WRITE_COORD(end.y);
	WRITE_COORD(end.z);
	WRITE_SHORT(modelIndex);
	WRITE_BYTE(0);		  // framestart
	WRITE_BYTE(0);		  // framerate
	WRITE_BYTE(life10);	  // life in 0.1 s
	WRITE_BYTE(width);	  // width
	WRITE_BYTE(noise);	  // noise
	WRITE_BYTE(r);
	WRITE_BYTE(g);
	WRITE_BYTE(b);
	WRITE_BYTE(brightness);
	WRITE_BYTE(scroll);	  // scroll speed
	MESSAGE_END();
}

// TE_BEAMCYLINDER shockwave
static void TorSendBeamCylinder(const Vector& origin, float radius, int modelIndex,
	int life10, int width, int r, int g, int b, int brightness)
{
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, origin);
	WRITE_BYTE(TE_BEAMCYLINDER);
	WRITE_COORD(origin.x);
	WRITE_COORD(origin.y);
	WRITE_COORD(origin.z);
	WRITE_COORD(origin.x);
	WRITE_COORD(origin.y);
	WRITE_COORD(origin.z + radius); // reach damage radius
	WRITE_SHORT(modelIndex);
	WRITE_BYTE(0);	   // startframe
	WRITE_BYTE(0);	   // framerate
	WRITE_BYTE(life10); // life
	WRITE_BYTE(width); // width
	WRITE_BYTE(0);	   // noise
	WRITE_BYTE(r);
	WRITE_BYTE(g);
	WRITE_BYTE(b);
	WRITE_BYTE(brightness);
	WRITE_BYTE(0);	   // speed
	MESSAGE_END();
}

// Checks whether a box is already occupied by another monster/player (replacement for MakerBlocker)
static bool TorBoxBlocked(const Vector& mins, const Vector& maxs, CBaseEntity* pIgnore)
{
	CBaseEntity* pList[8];
	const int count = UTIL_EntitiesInBox(pList, 8, mins, maxs, FL_CLIENT | FL_MONSTER);
	for (int i = 0; i < count; i++)
	{
		if (pList[i] != pIgnore)
			return true;
	}
	return false;
}

// MyMonsterPointer() returns NULL for players in this SDK, so test flags instead
static inline bool TorIsCreature(CBaseEntity* pEntity)
{
	return pEntity && (pEntity->pev->flags & (FL_MONSTER | FL_CLIENT)) != 0;
}

class CTorSummonPoint;

class CTor : public CSquadMonster
{
public:
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void Spawn() override;
	void Precache() override;
	void SetYawSpeed() override;
	int Classify() override;
	const char* DefaultDisplayName() { return "Tor"; }
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	Schedule_t* GetSchedule() override;
	Schedule_t* GetScheduleOfType(int Type) override;
	void StartTask(Task_t* pTask) override;

	void MonsterThink() override;
	bool CheckRangeAttack1(float flDot, float flDist) override;
	bool CheckRangeAttack2(float flDot, float flDist) override;
	bool CheckMeleeAttack1(float flDot, float flDist) override;
	bool CheckMeleeAttack2(float flDot, float flDist) override;
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override;
	void DeathNotice(entvars_t* pevChild) override;
	void ReportAIState() override;

	void SetObjectCollisionBox() override
	{
		pev->absmin = pev->origin + Vector(-24, -24, 0);
		pev->absmax = pev->origin + Vector(24, 24, 88);
	}

	void PainSound() override;
	void DeathSound() override;
	void AlertSound() override;
	void IdleSound() override;

	CUSTOM_SCHEDULES;

private:
	void SlamAttack();
	bool GetSummonPos(Vector& pos);
	void StartSummon();
	void MeleeHit(CBaseEntity* pHurt);

	float m_nextSlam;
	int m_shotsFired;
	float m_nextShoot;		// next time allowed to begin shooting
	float m_nextBeamBurst;	// next time a burst shot will be fired
	float m_nextBeam;		// next time a single beam will be fired
	int m_burstShotsFired;	// number of shots fired in the current burst
	int m_failedMelees;		// don't keep meleeing if the player is moving back and forth to avoid it
	float m_nextSummon;		// next time a grunt can be spawned
	float m_nextSummonCheck;
	int m_numChildren;

	// Not saved: re-filled in Precache(), which is also called on restore
	int m_iXenoBeam;
	int m_iShockwave;
	int m_iSummonSprite;

	friend class CTorSummonPoint;

	static const char* pAttackSounds[];
	static const char* pIdleSounds[];
	static const char* pAlertSounds[];
	static const char* pPainSounds[];
	static const char* pDieSounds[];
	static const char* pAttackHitSounds[];
	static const char* pAttackMissSounds[];
	static const char* pSlamSounds[];
};

LINK_ENTITY_TO_CLASS(monster_alien_tor, CTor);

TYPEDESCRIPTION CTor::m_SaveData[] =
	{
		DEFINE_FIELD(CTor, m_nextSlam, FIELD_TIME),
		DEFINE_FIELD(CTor, m_shotsFired, FIELD_INTEGER),
		DEFINE_FIELD(CTor, m_nextShoot, FIELD_TIME),
		DEFINE_FIELD(CTor, m_nextBeamBurst, FIELD_TIME),
		DEFINE_FIELD(CTor, m_nextBeam, FIELD_TIME),
		DEFINE_FIELD(CTor, m_burstShotsFired, FIELD_INTEGER),
		DEFINE_FIELD(CTor, m_failedMelees, FIELD_INTEGER),
		DEFINE_FIELD(CTor, m_nextSummon, FIELD_TIME),
		DEFINE_FIELD(CTor, m_nextSummonCheck, FIELD_TIME),
		DEFINE_FIELD(CTor, m_numChildren, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CTor, CSquadMonster);

//=========================================================
// Summon point: portal that spawns an alien grunt after a short delay
//=========================================================
class CTorSummonPoint : public CPointEntity
{
public:
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void EXPORT SummonThink();

	EHANDLE m_sprite;
	EHANDLE m_torHandle;
};

LINK_ENTITY_TO_CLASS(env_tor_summon_point, CTorSummonPoint);

TYPEDESCRIPTION CTorSummonPoint::m_SaveData[] =
	{
		DEFINE_FIELD(CTorSummonPoint, m_sprite, FIELD_EHANDLE),
		DEFINE_FIELD(CTorSummonPoint, m_torHandle, FIELD_EHANDLE),
};

IMPLEMENT_SAVERESTORE(CTorSummonPoint, CPointEntity);

//=========================================================
// Sounds
//=========================================================
const char* CTor::pAttackSounds[] =
	{
		"tor/tor-attack1.wav",
		"tor/tor-attack2.wav",
};

const char* CTor::pIdleSounds[] =
	{
		"tor/tor-idle.wav",
		"tor/tor-idle2.wav",
		"tor/tor-idle3.wav",
};

const char* CTor::pAlertSounds[] =
	{
		"tor/tor-alerted.wav",
};

const char* CTor::pPainSounds[] =
	{
		"tor/tor-pain.wav",
		"tor/tor-pain2.wav",
};

const char* CTor::pDieSounds[] =
	{
		"tor/tor-die.wav",
		"tor/tor-die2.wav",
};

const char* CTor::pAttackHitSounds[] =
	{
		"zombie/claw_strike1.wav",
		"zombie/claw_strike2.wav",
		"zombie/claw_strike3.wav",
};

const char* CTor::pAttackMissSounds[] =
	{
		"zombie/claw_miss1.wav",
		"zombie/claw_miss2.wav",
};

const char* CTor::pSlamSounds[] =
	{
		"houndeye/he_blast1.wav",
		"houndeye/he_blast2.wav",
		"houndeye/he_blast3.wav",
};

//=========================================================
// Schedules
//=========================================================
Task_t tlTorMeleeAttack[] =
	{
		{TASK_STOP_MOVING, 0},
		{TASK_FACE_ENEMY, (float)0},
		{TASK_MELEE_ATTACK2, (float)0},
};

Schedule_t slTorMeleeAttack[] =
	{
		{tlTorMeleeAttack,
			ARRAYSIZE(tlTorMeleeAttack),
			bits_COND_NEW_ENEMY |
				bits_COND_ENEMY_DEAD |
				bits_COND_HEAVY_DAMAGE |
				bits_COND_ENEMY_OCCLUDED,
			0,
			"Tor Melee Attack"},
};

Task_t tlTorShootAttack[] =
	{
		{TASK_STOP_MOVING, 0},
		{TASK_FACE_ENEMY, (float)0},
		{TASK_RANGE_ATTACK1, (float)0},
};

Schedule_t slTorShootAttack[] =
	{
		{tlTorShootAttack,
			ARRAYSIZE(tlTorShootAttack),
			bits_COND_NEW_ENEMY |
				bits_COND_ENEMY_DEAD |
				bits_COND_HEAVY_DAMAGE |
				bits_COND_ENEMY_OCCLUDED |
				bits_COND_HEAR_SOUND,
			bits_SOUND_DANGER,
			"Tor Shoot Attack"},
};

Task_t tlTorSummonAttack[] =
	{
		{TASK_STOP_MOVING, 0},
		{TASK_RANGE_ATTACK2, (float)0},
};

Schedule_t slTorSummonAttack[] =
	{
		{tlTorSummonAttack,
			ARRAYSIZE(tlTorSummonAttack),
			0,
			0,
			"Tor Summon Attack"},
};

// uninterruptable melee attack
Task_t tlTorSlamAttack[] =
	{
		{TASK_STOP_MOVING, 0},
		{TASK_MELEE_ATTACK1, (float)0},
};

Schedule_t slTorSlamAttack[] =
	{
		{tlTorSlamAttack,
			ARRAYSIZE(tlTorSlamAttack),
			0,
			0,
			"Tor Slam Attack"},
};

DEFINE_CUSTOM_SCHEDULES(CTor){
	slTorMeleeAttack,
	slTorShootAttack,
	slTorSummonAttack,
	slTorSlamAttack,
};

IMPLEMENT_CUSTOM_SCHEDULES(CTor, CSquadMonster);

void CTor::SetYawSpeed()
{
	pev->yaw_speed = 180;
}

int CTor::Classify()
{
	if (m_AltClass)
		return CLASS_PLAYER_ALIEN_ALLY;

	return CLASS_ALIEN_MILITARY;
}

void CTor::MeleeHit(CBaseEntity* pHurt)
{
	if (pHurt)
	{
		m_failedMelees = 0;
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, pAttackHitSounds[RANDOM_LONG(0, ARRAYSIZE(pAttackHitSounds) - 1)], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5, 5));
	}
	else
	{
		m_failedMelees++;
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, pAttackMissSounds[RANDOM_LONG(0, ARRAYSIZE(pAttackMissSounds) - 1)], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5, 5));
	}
}

void CTor::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case EVENT_SLAM:
		SlamAttack();
		CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, NORMAL_GUN_VOLUME, 0.3);
		break;

	case EVENT_SHOOT:
		m_nextBeam = m_nextBeamBurst = gpGlobals->time;

		// pause sequence to prevent triggering more of these events
		pev->framerate = 0.001f;
		m_fSequenceFinished = false;

		m_shotsFired++;
		if (m_shotsFired >= TOR_MAX_BEAM_SHOTS)
		{
			m_shotsFired = 0;
			m_nextShoot = gpGlobals->time + 3.0f;
		}
		m_failedMelees = 0;
		break;

	case EVENT_SUMMON_GRUNT:
		if (m_nextSummon < gpGlobals->time)
			StartSummon();
		break;

	case EVENT_STAFF_SWING:
	{
		CBaseEntity* pHurt = CheckTraceHullAttack(TOR_MELEE_ATTACK_DISTANCE, gSkillData.torPunch, DMG_SLASH);
		if (pHurt)
		{
			pHurt->pev->punchangle.x = 5;
			pHurt->pev->punchangle.z = 18;

			// CheckTraceHullAttack already called UTIL_MakeAimVectors
			if ((pHurt->pev->flags & (FL_MONSTER | FL_CLIENT)) != 0)
			{
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * 200 + gpGlobals->v_right * 200 + gpGlobals->v_up * 200;
			}
		}
		MeleeHit(pHurt);
		break;
	}

	case EVENT_STAFF_STAB:
	{
		CBaseEntity* pHurt = CheckTraceHullAttack(TOR_MELEE_ATTACK_DISTANCE, gSkillData.torPunch, DMG_SLASH);
		if (pHurt)
		{
			pHurt->pev->punchangle.x = 18;

			if ((pHurt->pev->flags & (FL_MONSTER | FL_CLIENT)) != 0)
			{
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * 100;
			}
		}
		MeleeHit(pHurt);
		break;
	}

	case EVENT_STEP_LEFT:
	case EVENT_STEP_RIGHT:
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "tor/tor-foot.wav", 1.0, ATTN_NORM, 0, RANDOM_LONG(90, 110));
		break;

	default:
		CSquadMonster::HandleAnimEvent(pEvent);
		break;
	}
}

Schedule_t* CTor::GetSchedule()
{
	if (HasConditions(bits_COND_HEAR_SOUND))
	{
		CSound* pSound = PBestSound();

		ASSERT(pSound != NULL);
		if (pSound && (pSound->m_iType & bits_SOUND_DANGER) != 0)
		{
			return GetScheduleOfType(SCHED_TAKE_COVER_FROM_BEST_SOUND);
		}
	}

	return CSquadMonster::GetSchedule();
}

Schedule_t* CTor::GetScheduleOfType(int Type)
{
	switch (Type)
	{
	case SCHED_MELEE_ATTACK1:
		return slTorSlamAttack;
	case SCHED_MELEE_ATTACK2:
		return slTorMeleeAttack;
	case SCHED_RANGE_ATTACK1:
		return slTorShootAttack;
	case SCHED_RANGE_ATTACK2:
		return slTorSummonAttack;
	default:
		return CSquadMonster::GetScheduleOfType(Type);
	}
}

void CTor::StartTask(Task_t* pTask)
{
	if (pTask->iTask == TASK_MELEE_ATTACK2)
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, pAttackSounds[RANDOM_LONG(0, ARRAYSIZE(pAttackSounds) - 1)], 1.0, ATTN_NORM, 0, RANDOM_LONG(95, 105));
	}
	CSquadMonster::StartTask(pTask);
}

void CTor::MonsterThink()
{
	if (m_nextBeamBurst != 0 && m_nextBeamBurst < gpGlobals->time)
	{
		pev->framerate = 0.001f;

		if (m_nextBeam < gpGlobals->time)
		{
			m_nextBeam = gpGlobals->time + 0.05;
			m_burstShotsFired++;
			EMIT_SOUND(ENT(pev), CHAN_WEAPON, "tor/tor-staff-discharge.wav", 1.0, ATTN_NORM);
			CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, NORMAL_GUN_VOLUME, 0.3);

			Vector vecSrc, angles;
			GetAttachment(0, vecSrc, angles);
			CBaseEntity* target = m_hEnemy;

			if (target)
			{
				Vector vecDir1 = (target->BodyTarget(pev->origin) - vecSrc).Normalize();

				TraceResult tr;
				UTIL_TraceLine(vecSrc, vecSrc + vecDir1 * TOR_SHOOT_RANGE, dont_ignore_monsters, edict(), &tr);

				// two overlapping beams: a plain one and a sine one
				CBeam* beam = CBeam::BeamCreate("sprites/xenobeam.spr", 50);
				if (beam)
				{
					beam->PointsInit(vecSrc, tr.vecEndPos);
					beam->SetColor(96, 128, 16);
					beam->SetBrightness(150);
					beam->SetNoise(10);
					beam->SetScrollRate(150);
					beam->pev->spawnflags |= SF_BEAM_TEMPORARY;
					beam->LiveForTime(0.5f);
				}

				CBeam* beam2 = CBeam::BeamCreate("sprites/xenobeam.spr", 50);
				if (beam2)
				{
					beam2->PointsInit(vecSrc, tr.vecEndPos);
					beam2->SetColor(96, 255, 16);
					beam2->SetBrightness(150);
					beam2->SetNoise(15);
					beam2->SetScrollRate(150);
					beam2->SetFlags(BEAM_FSINE);
					beam2->pev->spawnflags |= SF_BEAM_TEMPORARY;
					beam2->LiveForTime(0.5f);
				}

				CBaseEntity* phit = CBaseEntity::Instance(tr.pHit);
				if (phit)
				{
					phit->TakeDamage(pev, pev, gSkillData.torEnergyBeam, DMG_ENERGYBEAM);

					if (TorIsCreature(phit) && (phit->pev->movetype == MOVETYPE_STEP || phit->IsPlayer()))
					{
						phit->pev->velocity.z += (phit->pev->flags & FL_ONGROUND) != 0 ? gSkillData.torLiftSpeedGround : gSkillData.torLiftSpeed;
					}
				}
			}

			if (m_burstShotsFired >= 3)
			{
				m_nextBeam = 0;
				m_nextBeamBurst = 0;
				m_burstShotsFired = 0;

				// finish the shoot task
				pev->framerate = 1.0f;
				m_fSequenceFinished = true;
			}
		}
	}
	CSquadMonster::MonsterThink();
}

bool CTor::CheckRangeAttack1(float flDot, float flDist)
{
	const bool shouldMelee = flDist < TOR_MELEE_CHASE_DISTANCE && m_failedMelees < 2;
	if (!shouldMelee && flDist < TOR_SHOOT_RANGE)
	{
		if (gpGlobals->time > m_nextShoot)
			return true;
	}

	return false;
}

bool CTor::CheckRangeAttack2(float flDot, float flDist)
{
	if (m_numChildren < TOR_MAX_ALLOWED_CHILDREN && m_nextSummon < gpGlobals->time && m_nextSummonCheck < gpGlobals->time)
	{
		m_nextSummonCheck = gpGlobals->time + (HasConditions(bits_COND_ENEMY_OCCLUDED) ? 0.5f : 1.0f);
		Vector dummy;
		return GetSummonPos(dummy);
	}
	return false;
}

bool CTor::CheckMeleeAttack1(float flDot, float flDist)
{
	if (m_nextSlam > gpGlobals->time)
		return false;

	int nearbyEnemies = 0;

	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, pev->origin, TOR_SLAM_CHECK_DISTANCE)) != NULL)
	{
		if (TorIsCreature(pEntity) && pEntity->IsAlive() && IRelationship(pEntity) >= R_DL)
		{
			if (pEntity->IsPlayer())
				return true;
			nearbyEnemies++;
			if (nearbyEnemies >= TOR_SLAM_ATTACK_ENEMY_COUNT)
			{
				return true;
			}
		}
	}

	return false;
}

bool CTor::CheckMeleeAttack2(float flDot, float flDist)
{
	// replacement for CheckMeleeAttackImpl
	if (m_hEnemy != NULL && flDist <= TOR_MELEE_ATTACK_CHECK_DISTANCE && flDot >= 0.7f)
	{
		return true;
	}
	return false;
}

void CTor::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	if (ptr->iHitgroup == 10 && (bitsDamageType & (DMG_BULLET | DMG_SLASH | DMG_CLUB)) != 0)
	{
		// hit armor
		if (pev->dmgtime != gpGlobals->time || (RANDOM_LONG(0, 10) < 1))
		{
			UTIL_Ricochet(ptr->vecEndPos, RANDOM_FLOAT(1.0f, 2.0f));
			pev->dmgtime = gpGlobals->time;
		}

		if ((bitsDamageType & DMG_BULLET) != 0 && RANDOM_LONG(0, 1) == 0)
		{
			Vector vecTracerDir = vecDir;

			vecTracerDir.x += RANDOM_FLOAT(-0.3f, 0.3f);
			vecTracerDir.y += RANDOM_FLOAT(-0.3f, 0.3f);
			vecTracerDir.z += RANDOM_FLOAT(-0.3f, 0.3f);

			vecTracerDir = vecTracerDir * -512.0f;

			MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, ptr->vecEndPos);
			WRITE_BYTE(TE_TRACER);
			WRITE_COORD(ptr->vecEndPos.x);
			WRITE_COORD(ptr->vecEndPos.y);
			WRITE_COORD(ptr->vecEndPos.z);

			WRITE_COORD(vecTracerDir.x);
			WRITE_COORD(vecTracerDir.y);
			WRITE_COORD(vecTracerDir.z);
			MESSAGE_END();
		}

		flDamage -= 20.0f;
		if (flDamage <= 0.0f)
			flDamage = 0.1f; // don't hurt the monster much, but allow bits_COND_LIGHT_DAMAGE to be generated

		ptr->iHitgroup = HITGROUP_GENERIC;

		// no blood on armor
		if (0 != pev->takedamage)
		{
			m_LastHitGroup = HITGROUP_GENERIC;
			AddMultiDamage(pevAttacker, this, flDamage, bitsDamageType);
		}
		return;
	}

	CSquadMonster::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
}

void CTor::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/Tor.mdl");
	UTIL_SetSize(pev, Vector(-24, -24, 0), Vector(24, 24, 72));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_YELLOW;
	pev->effects = 0;
	pev->health = gSkillData.torHealth;
	pev->view_ofs = Vector(0, 0, 0); // position of the eyes relative to monster's origin.
	m_flFieldOfView = -0.7;			 // VIEW_FIELD_WIDE
	m_MonsterState = MONSTERSTATE_NONE;
	m_afCapability = bits_CAP_SQUAD | bits_CAP_DOORS_GROUP;

	m_nextSlam = 0;
	m_shotsFired = 0;
	m_nextShoot = 0;
	m_nextBeamBurst = 0;
	m_nextBeam = 0;
	m_burstShotsFired = 0;
	m_failedMelees = 0;
	m_nextSummon = 0;
	m_nextSummonCheck = 0;
	m_numChildren = 0;

	MonsterInit();
}

void CTor::Precache()
{
	int i;

	PRECACHE_MODEL("models/Tor.mdl");

	for (i = 0; i < ARRAYSIZE(pAttackSounds); i++)
		PRECACHE_SOUND(pAttackSounds[i]);
	for (i = 0; i < ARRAYSIZE(pIdleSounds); i++)
		PRECACHE_SOUND(pIdleSounds[i]);
	for (i = 0; i < ARRAYSIZE(pAlertSounds); i++)
		PRECACHE_SOUND(pAlertSounds[i]);
	for (i = 0; i < ARRAYSIZE(pPainSounds); i++)
		PRECACHE_SOUND(pPainSounds[i]);
	for (i = 0; i < ARRAYSIZE(pDieSounds); i++)
		PRECACHE_SOUND(pDieSounds[i]);
	for (i = 0; i < ARRAYSIZE(pAttackHitSounds); i++)
		PRECACHE_SOUND(pAttackHitSounds[i]);
	for (i = 0; i < ARRAYSIZE(pAttackMissSounds); i++)
		PRECACHE_SOUND(pAttackMissSounds[i]);
	for (i = 0; i < ARRAYSIZE(pSlamSounds); i++)
		PRECACHE_SOUND(pSlamSounds[i]);

	PRECACHE_SOUND("tor/tor-foot.wav");
	PRECACHE_SOUND("tor/tor-staff-discharge.wav");
	PRECACHE_SOUND("tor/tor-summon.wav");
	PRECACHE_SOUND("debris/beamstart8.wav"); // summon portal
	PRECACHE_SOUND("debris/beamstart7.wav"); // summon spawn

	m_iXenoBeam = PRECACHE_MODEL("sprites/xenobeam.spr");
	m_iShockwave = PRECACHE_MODEL("sprites/shockwave.spr");
	m_iSummonSprite = PRECACHE_MODEL("sprites/exit1.spr");

	UTIL_PrecacheOther(TOR_SUMMON_CLASSNAME);

	m_shotsFired = m_burstShotsFired = 0;
}

void CTor::PainSound()
{
	if (RANDOM_LONG(0, 2) == 0)
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, pPainSounds[RANDOM_LONG(0, ARRAYSIZE(pPainSounds) - 1)], 1.0, ATTN_NORM, 0, RANDOM_LONG(100, 109));
}

void CTor::DeathSound()
{
	EMIT_SOUND(ENT(pev), CHAN_VOICE, pDieSounds[RANDOM_LONG(0, ARRAYSIZE(pDieSounds) - 1)], 1.0, ATTN_NORM);
}

void CTor::AlertSound()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, pAlertSounds[RANDOM_LONG(0, ARRAYSIZE(pAlertSounds) - 1)], 1.0, ATTN_NORM, 0, RANDOM_LONG(100, 109));
}

void CTor::IdleSound()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, pIdleSounds[RANDOM_LONG(0, ARRAYSIZE(pIdleSounds) - 1)], 1.0, ATTN_NORM, 0, RANDOM_LONG(95, 105));
}

void CTor::SlamAttack()
{
	m_nextSlam = gpGlobals->time + RANDOM_FLOAT(3.5f, 5.5f);

	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, pev->origin, TOR_SLAM_ATTACK_RADIUS)) != NULL)
	{
		if (TorIsCreature(pEntity) && pEntity->IsAlive() && pEntity->entindex() != entindex() && IRelationship(pEntity) >= R_DL)
		{
			const Vector delta = pEntity->pev->origin - pev->origin;
			Vector pushDir = delta.Normalize();
			pushDir.z = 0;

			float launchPower = 1.0f - ((delta.Length() - 64) * 0.5f / TOR_SLAM_ATTACK_RADIUS);
			if (launchPower < 0.7f)
				launchPower = 0.7f;
			const float pushPower = 1.0f - launchPower;

			const Vector launchForce = Vector(0, 0, 1) * 500 * launchPower;
			const Vector pushForce = pushDir * 700 * pushPower;

			pEntity->pev->velocity = pEntity->pev->velocity + launchForce + pushForce;
			pEntity->TakeDamage(pev, pev, gSkillData.torSonicBlast * launchPower, DMG_SONIC);

			if (pEntity->IsPlayer())
			{
				pEntity->pev->punchangle.x = 10;
			}
		}
	}

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, pSlamSounds[RANDOM_LONG(0, ARRAYSIZE(pSlamSounds) - 1)], 1.0, ATTN_NORM, 0, RANDOM_LONG(95, 105));

	const float radius = (TOR_SLAM_ATTACK_RADIUS + 50) / 0.3f;
	TorSendBeamCylinder(pev->origin, radius, m_iShockwave, 2, 12, 255, 255, 255, 255);
}

bool CTor::GetSummonPos(Vector& pos)
{
	const int attempts = 8;

	const Vector startPos = pev->origin + Vector(0, 0, pev->maxs.z - pev->mins.z + 2.0f);

	for (int i = 0; i < attempts; i++)
	{
		// random point on circle
		const float c = RANDOM_FLOAT(0, 2 * M_PI);
		const float x = cos(c) * TOR_SUMMON_DISTANCE;
		const float y = sin(c) * TOR_SUMMON_DISTANCE;
		const float z = startPos.z + (TOR_SUMMON_MAXS.z - TOR_SUMMON_MINS.z) * 0.5f + RANDOM_FLOAT(0.0f, TOR_SUMMON_HEIGHT);

		const Vector checkPos = pev->origin + Vector(x, y, z);

		TraceResult tr;
		UTIL_TraceHull(startPos, checkPos, dont_ignore_monsters, large_hull, edict(), &tr);

		if (0 != tr.fStartSolid || tr.flFraction < 0.5f)
		{
			continue;
		}

		// move a little bit inward to avoid spawning inside a wall/ceiling
		const Vector dir = (checkPos - pev->origin).Normalize();
		pos = tr.vecEndPos - dir * 40;

		const Vector mins = pos + TOR_SUMMON_MINS;
		const Vector maxs = pos + TOR_SUMMON_MAXS;

		UTIL_TraceLine(mins, maxs, dont_ignore_monsters, nullptr, &tr);
		if (tr.flFraction == 1.0f && !TorBoxBlocked(mins, maxs, this))
			return true;
	}

	return false;
}

void CTor::StartSummon()
{
	Vector summonPos;
	if (!GetSummonPos(summonPos))
	{
		ALERT(at_aiconsole, "%s: failed to find a summon position\n", STRING(pev->classname));
		return;
	}

	m_nextSummon = gpGlobals->time + RANDOM_FLOAT(5.0f, 10.0f);

	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "tor/tor-summon.wav", 1.0, ATTN_NORM);

	Vector startPos = pev->origin;
	startPos.z += (pev->maxs.z - pev->mins.z) * 0.75f;

	for (int i = 0; i < 3; i++)
	{
		TorSendBeamPoints(startPos, summonPos, m_iXenoBeam, 20, 30, 80, 96, 255, 32, 80, 0);
	}

	CSprite* portalSprite = CSprite::SpriteCreate("sprites/exit1.spr", summonPos, true);
	if (portalSprite)
	{
		portalSprite->SetTransparency(kRenderTransAdd, 255, 255, 255, 128, kRenderFxNone);
		portalSprite->SetScale(2.0f);
		portalSprite->pev->framerate = 10.0f;
	}

	CTorSummonPoint* summonPoint = GetClassPtr((CTorSummonPoint*)nullptr);
	summonPoint->pev->classname = MAKE_STRING("env_tor_summon_point");
	UTIL_SetOrigin(summonPoint->pev, summonPos);
	summonPoint->m_sprite = portalSprite;
	summonPoint->m_torHandle = this;
	summonPoint->SetThink(&CTorSummonPoint::SummonThink);
	summonPoint->pev->nextthink = gpGlobals->time + 2.0f;
	summonPoint->pev->dmgtime = gpGlobals->time;

	EMIT_SOUND(ENT(summonPoint->pev), CHAN_ITEM, "debris/beamstart8.wav", 1.0, ATTN_NORM);
}

void CTorSummonPoint::SummonThink()
{
	auto removeSelf = [&]()
	{
		SetThink(&CBaseEntity::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.1f;

		CSprite* pSprite = m_sprite.Entity<CSprite>();
		if (pSprite)
		{
			pSprite->AnimateAndDie(pSprite->pev->framerate > 10.0f ? pSprite->pev->framerate : 10.0f);
		}
	};

	CBaseEntity* pOwner = m_torHandle;

	const char* removalReason = nullptr;

	if (!pOwner)
	{
		removalReason = "no owner";
	}
	else if (!FStrEq(STRING(pOwner->pev->classname), "monster_alien_tor"))
	{
		removalReason = "owner is not a Tor";
	}
	else if (pev->dmgtime + 3.0f < gpGlobals->time)
	{
		removalReason = "stuck for too long";
	}
	if (removalReason)
	{
		ALERT(at_aiconsole, "%s is going to be removed. Reason: %s\n", STRING(pev->classname), removalReason);
		removeSelf();
		return;
	}

	CTor* pTor = (CTor*)pOwner;
	const Vector mins = pev->origin + TOR_SUMMON_MINS;
	const Vector maxs = pev->origin + TOR_SUMMON_MAXS;

	if (TorBoxBlocked(mins, maxs, pTor))
	{
		pev->nextthink = gpGlobals->time + 0.5f;
		return;
	}

	// same way CMonsterMaker creates children
	edict_t* pent;
	if (pOwner->m_AltClass)
		pent = CREATE_NAMED_ENTITY(MAKE_STRING(TOR_SUMMON_CLASSNAME_ALLIED));
	else
	{
		if (UTIL_IsSandbox())
			pent = CREATE_NAMED_ENTITY(MAKE_STRING(TOR_SUMMON_CLASSNAME_ALT));
		else
			pent = CREATE_NAMED_ENTITY(MAKE_STRING(TOR_SUMMON_CLASSNAME));
	}
	if (FNullEnt(pent))
	{
		ALERT(at_console, "%s is going to be removed. Reason: can't spawn a child '%s'\n", STRING(pev->classname), TOR_SUMMON_CLASSNAME);
		removeSelf();
		return;
	}

	entvars_t* pevCreate = VARS(pent);
	pevCreate->origin = pev->origin;
	pevCreate->angles = pTor->pev->angles;
	SetBits(pevCreate->spawnflags, SF_MONSTER_FALL_TO_GROUND);

	DispatchSpawn(pent);

	if (!FNullEnt(pent) && (pevCreate->flags & FL_KILLME) == 0)
	{
		// owner is set after spawn so DeathNotice() reaches the Tor when the child dies
		pevCreate->owner = pTor->edict();

		CBaseEntity* pChild = CBaseEntity::Instance(pent);
		CBaseMonster* mon = pChild ? pChild->MyMonsterPointer() : nullptr;
		if (mon && pTor->m_hEnemy != NULL)
		{
			mon->PushEnemy(pTor->m_hEnemy, pTor->m_vecEnemyLKP);
		}

		EMIT_SOUND(pOwner->edict(), CHAN_ITEM, "debris/beamstart7.wav", 1.0, ATTN_NORM);
		pTor->m_numChildren++;
	}

	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1f;

	CSprite* pSprite = m_sprite.Entity<CSprite>();
	if (pSprite)
	{
		pSprite->SetThink(&CBaseEntity::SUB_Remove);
		pSprite->pev->nextthink = gpGlobals->time;
	}
}

void CTor::DeathNotice(entvars_t* pevChild)
{
	m_numChildren--;
	if (m_numChildren < 0)
	{
		m_numChildren = 0;
	}
}

void CTor::ReportAIState()
{
	CBaseMonster::ReportAIState();
	ALERT(at_console, "Number of children: %d. ", m_numChildren);
}
