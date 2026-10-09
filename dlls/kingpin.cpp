/***
 *
 *	Kingpin - port of the Featureful SDK monster to Go-Mod: Reborn SDK
 *
 ****/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "squadmonster.h"
#include "weapons.h"
#include "soundent.h"
#include "shake.h"
#include "effects.h"
#include "customentity.h"
#include "decals.h"

#define KINGPIN_AE_LEFT 1
#define KINGPIN_AE_RIGHT 2
#define KINGPIN_AE_PLASMA_START 3
#define KINGPIN_AE_PLASMA_LAUNCH 4
#define KINGPIN_AE_PLASMA_END 5

#define KINGPIN_PLASMABALL_LIFETIME 4.5
#define KINGPIN_PLASMABALL_DELAY 6
#define KINGPIN_PLASMABALL_RADIUS 190.0f
#define KINGPIN_PLASMABALL_LIMIT_SPEED 275.0f
#define KINGPIN_REDRAW_BEAMTRAIL_TIME 0.5f
#define KINGPIN_TELEPORT_INTERVAL 8.0f
#define KINGPIN_TELEPORT_DELAY 1.5f
#define KINGPIN_PLASMACLUSTER_DELAY 4
#define KINGPIN_PLASMACLUSTER_ATTACK_DISTANCE 512

#define KINGPIN_MELEE_ATTACK_DISTANCE 100
#define KINGPIN_MELEE_ATTACK_CHECK_DISTANCE 88

#ifndef M_PI_F
#define M_PI_F 3.14159265358979323846f
#endif

enum
{
	SCHED_KINGPIN_TELEPORT = LAST_COMMON_SCHEDULE + 1,
};

enum
{
	TASK_KINGPIN_TELEPORT = LAST_COMMON_TASK + 1,
};

#define KINGPIN_PLASMA_BALL_SCALE 1.5f

//=========================================================
// Local effect helpers (replacement for Featureful's SendEntLight/SendBeam*/Visual helpers)
//=========================================================

// TE_ELIGHT attached to an entity
static void KingpinSendEntLight(int entIndex, const Vector& origin, float radius, int r, int g, int b, float life, float decay)
{
	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_ELIGHT);
	WRITE_SHORT(entIndex);
	WRITE_COORD(origin.x);
	WRITE_COORD(origin.y);
	WRITE_COORD(origin.z);
	WRITE_COORD(radius);
	WRITE_BYTE(r);
	WRITE_BYTE(g);
	WRITE_BYTE(b);
	WRITE_BYTE((int)(life * 10));
	WRITE_COORD(decay);
	MESSAGE_END();
}

// TE_BEAMCYLINDER / TE_BEAMDISK
static void KingpinSendBeamWave(int type, const Vector& origin, float radius, int modelIndex,
	float life, int width, int noise, int r, int g, int b, int brightness)
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, origin);
	WRITE_BYTE(type);
	WRITE_COORD(origin.x);
	WRITE_COORD(origin.y);
	WRITE_COORD(origin.z);
	WRITE_COORD(origin.x);
	WRITE_COORD(origin.y);
	WRITE_COORD(origin.z + radius);
	WRITE_SHORT(modelIndex);
	WRITE_BYTE(0);	// startframe
	WRITE_BYTE(0);	// framerate
	WRITE_BYTE((int)(life * 10));
	WRITE_BYTE(width);
	WRITE_BYTE(noise);
	WRITE_BYTE(r);
	WRITE_BYTE(g);
	WRITE_BYTE(b);
	WRITE_BYTE(brightness);
	WRITE_BYTE(0);	// speed
	MESSAGE_END();
}

// TE_BEAMFOLLOW
static void KingpinSendBeamFollow(int entIndex, int modelIndex, float life, int width, int r, int g, int b, int brightness)
{
	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entIndex);
	WRITE_SHORT(modelIndex);
	WRITE_BYTE((int)(life * 10));
	WRITE_BYTE(width);
	WRITE_BYTE(r);
	WRITE_BYTE(g);
	WRITE_BYTE(b);
	WRITE_BYTE(brightness);
	MESSAGE_END();
}

// MyMonsterPointer() returns NULL for players in this SDK, so test flags instead
static inline bool KingpinIsCreature(CBaseEntity* pEntity)
{
	return pEntity && (pEntity->pev->flags & (FL_MONSTER | FL_CLIENT)) != 0;
}

//=========================================================
// Plasma ball: slow homing orb
//=========================================================
class CKingpinPlasmaBall : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void Activate() override;
	void Launch();
	void EXPORT HuntThink();
	void EXPORT BounceTouch(CBaseEntity* pOther);
	void EXPORT AnimateThink();
	void Animate();
	void MovetoTarget(Vector vecTarget);

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	Vector m_vecIdeal;

protected:
	void Explode(CBaseEntity* pEnemy);
	void RemoveMySelf();
	void DrawTrailingBeam();

	bool m_shouldRestartSound;

	// not saved, refilled by Precache() (called on restore too)
	int m_iTrailSprite;
	int m_iShockwave;
};

LINK_ENTITY_TO_CLASS(kingpin_plasma_ball, CKingpinPlasmaBall);

TYPEDESCRIPTION CKingpinPlasmaBall::m_SaveData[] =
	{
		DEFINE_FIELD(CKingpinPlasmaBall, m_vecIdeal, FIELD_VECTOR),
};

IMPLEMENT_SAVERESTORE(CKingpinPlasmaBall, CBaseMonster);

void CKingpinPlasmaBall::Spawn()
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "sprites/nhth1.spr");
	pev->rendermode = kRenderTransAdd;
	pev->rendercolor = Vector(255, 255, 255);
	pev->renderamt = 255;
	pev->scale = 0.5f;

	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin(pev, pev->origin);
	m_vecIdeal = Vector(0, 0, 0);
	m_shouldRestartSound = false;

	SetThink(&CKingpinPlasmaBall::AnimateThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

void CKingpinPlasmaBall::Launch()
{
	pev->nextthink = gpGlobals->time + 0.1;
	SetThink(&CKingpinPlasmaBall::HuntThink);
	SetTouch(&CKingpinPlasmaBall::BounceTouch);

	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "kingpin/kingpin_seeker_amb.wav", 1.0, ATTN_NORM);

	pev->dmgtime = gpGlobals->time + KINGPIN_PLASMABALL_LIFETIME;
	DrawTrailingBeam();
	pev->air_finished = gpGlobals->time + KINGPIN_REDRAW_BEAMTRAIL_TIME;
}

void CKingpinPlasmaBall::Precache()
{
	PRECACHE_MODEL("sprites/nhth1.spr");
	m_iTrailSprite = PRECACHE_MODEL("sprites/smoke.spr");
	m_iShockwave = PRECACHE_MODEL("sprites/shockwave.spr");

	PRECACHE_SOUND("debris/beamstart14.wav");
	PRECACHE_SOUND("kingpin/kingpin_seeker_amb.wav");
	PRECACHE_SOUND("kingpin/kingpin_seeker1.wav");
	PRECACHE_SOUND("kingpin/kingpin_seeker2.wav");
	PRECACHE_SOUND("kingpin/kingpin_seeker3.wav");
}

void CKingpinPlasmaBall::Activate()
{
	CBaseMonster::Activate();
	if (pev->velocity != g_vecZero)
		m_shouldRestartSound = true; // restart sound
}

void CKingpinPlasmaBall::HuntThink()
{
	if (m_shouldRestartSound)
	{
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, "kingpin/kingpin_seeker_amb.wav", 1.0, ATTN_NORM);
		m_shouldRestartSound = false;
	}

	pev->nextthink = gpGlobals->time + 0.1f;

	// check world boundaries
	if (!IsInWorld())
	{
		RemoveMySelf();
		return;
	}

	if (gpGlobals->time > pev->dmgtime || m_hEnemy == NULL || !m_hEnemy->IsAlive())
	{
		Explode(NULL);
		return;
	}

	MovetoTarget(m_hEnemy->Center());

	if ((m_hEnemy->Center() - pev->origin).Length() < 96.0f)
	{
		Explode(m_hEnemy);
		return;
	}

	if (pev->air_finished <= gpGlobals->time)
	{
		pev->air_finished = gpGlobals->time + KINGPIN_REDRAW_BEAMTRAIL_TIME;
		DrawTrailingBeam();
	}
	Animate();
}

void CKingpinPlasmaBall::AnimateThink()
{
	pev->nextthink = gpGlobals->time + 0.1;
	Animate();
}

void CKingpinPlasmaBall::Animate()
{
	pev->frame = (int)(pev->frame + 1) % 11;

	if (pev->scale < KINGPIN_PLASMA_BALL_SCALE)
		pev->scale += 0.1f;

	KingpinSendEntLight(entindex(), pev->origin, 128, 128, 128, 255, 1.0f, 128);
}

void CKingpinPlasmaBall::Explode(CBaseEntity* pEnemy)
{
	int classify = CLASS_NONE;
	CBaseEntity* pOwner = CBaseEntity::Instance(pev->owner);
	if (pOwner)
		classify = pOwner->Classify();

	::RadiusDamage(pev->origin, pev, pOwner ? pOwner->pev : pev, gSkillData.kingpinPlasmaBlast, KINGPIN_PLASMABALL_RADIUS, classify, DMG_SHOCK);

	KingpinSendBeamWave(TE_BEAMDISK, pev->origin, 600, m_iShockwave, 0.2f, 32, 10, 255, 200, 255, 255);
	KingpinSendBeamWave(TE_BEAMCYLINDER, pev->origin, 750, m_iShockwave, 0.2f, 32, 10, 255, 200, 255, 255);

	UTIL_EmitAmbientSound(ENT(0), pev->origin, "debris/beamstart14.wav", 0.5f, ATTN_NORM, 0, RANDOM_LONG(140, 160));

	RemoveMySelf();
}

void CKingpinPlasmaBall::DrawTrailingBeam()
{
	KingpinSendBeamFollow(entindex(), m_iTrailSprite, 0.6f, 12, 255, 220, 255, 180);
}

void CKingpinPlasmaBall::RemoveMySelf()
{
	SetTouch(NULL);
	STOP_SOUND(ENT(pev), CHAN_WEAPON, "kingpin/kingpin_seeker_amb.wav");
	UTIL_Remove(this);
}

void CKingpinPlasmaBall::MovetoTarget(Vector vecTarget)
{
	// accelerate
	float flSpeed = m_vecIdeal.Length();
	if (flSpeed == 0.0f)
	{
		m_vecIdeal = pev->velocity;
		flSpeed = m_vecIdeal.Length();
	}

	if (flSpeed > KINGPIN_PLASMABALL_LIMIT_SPEED)
	{
		m_vecIdeal = m_vecIdeal.Normalize() * KINGPIN_PLASMABALL_LIMIT_SPEED;
	}
	m_vecIdeal = m_vecIdeal + (vecTarget - pev->origin).Normalize() * 100.0f;
	pev->velocity = m_vecIdeal;
}

void CKingpinPlasmaBall::BounceTouch(CBaseEntity* pOther)
{
	if (0 != pOther->pev->takedamage && FBitSet(pOther->pev->flags, FL_CLIENT | FL_MONSTER))
	{
		Explode(pOther);
	}
	else
	{
		Vector vecDir = m_vecIdeal.Normalize();

		TraceResult tr = UTIL_GetGlobalTrace();
		float n = -DotProduct(tr.vecPlaneNormal, vecDir);
		vecDir = 2.0 * tr.vecPlaneNormal * n + vecDir;

		m_vecIdeal = vecDir * m_vecIdeal.Length();
	}
}

//=========================================================
// Plasma cluster: ground burst of lightning particles
//=========================================================
#define KINGPIN_CLUSTER_SPRITE_INITIAL_AMT 50
#define KINGPIN_CLUSTER_PARTICLE_COUNT 12
#define KINGPIN_CLUSTER_PARTICLE_SPEED 280.0f
#define KINGPIN_CLUSTER_PARTICLE_SPEED_STEP 10.0f
#define KINGPIN_CLUSTER_DISPERSE_TIME 0.8f
#define KINGPIN_CLUSTER_FALL_TIME 4.5f
#define KINGPIN_CLUSTER_BEAM_BRIGHTNESS 220
#define KINGPIN_CLUSTER_PARTICLE_BRIGHTNESS 220

class CPlasmaClusterParticle : public CBaseEntity
{
public:
	void Precache() override
	{
		PRECACHE_MODEL("sprites/flare1.spr");
	}

	void Spawn() override
	{
		Precache();
		pev->movetype = MOVETYPE_FLY;
		pev->solid = SOLID_NOT;

		SET_MODEL(ENT(pev), "sprites/flare1.spr");
		pev->scale = 0.4f;
		pev->rendermode = kRenderTransAdd;
		pev->rendercolor = Vector(240, 120, 200);
		pev->renderamt = KINGPIN_CLUSTER_PARTICLE_BRIGHTNESS;

		UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));

		pev->gravity = RANDOM_FLOAT(0.09f, 0.11f);
	}

	void SetAltColor()
	{
		pev->rendercolor = Vector(160, 130, 255);
	}
};

LINK_ENTITY_TO_CLASS(plasma_cluster_particle, CPlasmaClusterParticle);

class CKingpinPlasmaCluster : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;

	void EXPORT StartUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void EXPORT AnimateThink();
	void EXPORT DisperseThink();
	void EXPORT FallThink();

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	void Start();
	void Explode();
	void MakeParticleBurst();
	void RemoveEffects();
	void BeamDamage(float flDamage);
	void TurnOffRandomBeams(int count);
	void MakeELight();
	void EmitZapSound(float volume, bool useAltChannel, CBaseEntity* pSource);

	float m_lastTime;
	float m_maxFrame;
	CBaseEntity* m_particles[KINGPIN_CLUSTER_PARTICLE_COUNT];
	CBeam* m_beams[KINGPIN_CLUSTER_PARTICLE_COUNT];
	float zapTime;
	float zapSoundTime;
	int lastZapSoundChannel;
};

LINK_ENTITY_TO_CLASS(env_plasmacluster, CKingpinPlasmaCluster);
LINK_ENTITY_TO_CLASS(kingpin_plasma_cluster, CKingpinPlasmaCluster);

TYPEDESCRIPTION CKingpinPlasmaCluster::m_SaveData[] =
	{
		DEFINE_FIELD(CKingpinPlasmaCluster, m_lastTime, FIELD_TIME),
		DEFINE_FIELD(CKingpinPlasmaCluster, m_maxFrame, FIELD_FLOAT),
		DEFINE_ARRAY(CKingpinPlasmaCluster, m_particles, FIELD_CLASSPTR, KINGPIN_CLUSTER_PARTICLE_COUNT),
		DEFINE_ARRAY(CKingpinPlasmaCluster, m_beams, FIELD_CLASSPTR, KINGPIN_CLUSTER_PARTICLE_COUNT),
		DEFINE_FIELD(CKingpinPlasmaCluster, zapTime, FIELD_TIME),
		DEFINE_FIELD(CKingpinPlasmaCluster, zapSoundTime, FIELD_TIME),
};

IMPLEMENT_SAVERESTORE(CKingpinPlasmaCluster, CBaseEntity);

void CKingpinPlasmaCluster::Spawn()
{
	pev->solid = SOLID_NOT;
	Precache();

	memset(m_particles, 0, sizeof(m_particles));
	memset(m_beams, 0, sizeof(m_beams));
	zapTime = 0;
	zapSoundTime = 0;
	lastZapSoundChannel = CHAN_ITEM;

	if (FStringNull(pev->targetname))
		Start();
	else
		SetUse(&CKingpinPlasmaCluster::StartUse);
}

void CKingpinPlasmaCluster::Precache()
{
	PRECACHE_MODEL("sprites/c-tele1.spr");
	PRECACHE_MODEL("sprites/redflare2.spr");
	PRECACHE_MODEL("sprites/plasma.spr");
	PRECACHE_MODEL("sprites/flare1.spr");

	PRECACHE_SOUND("debris/beamstart1.wav");
	PRECACHE_SOUND("debris/beamstart14.wav");
	PRECACHE_SOUND("debris/zap1.wav");
	PRECACHE_SOUND("debris/zap3.wav");
	PRECACHE_SOUND("debris/zap8.wav");
}

void CKingpinPlasmaCluster::Start()
{
	pev->movetype = MOVETYPE_NONE;
	pev->effects = 0;
	pev->frame = 0;

	SET_MODEL(ENT(pev), "sprites/c-tele1.spr");
	pev->rendermode = kRenderTransAdd;
	pev->renderamt = KINGPIN_CLUSTER_SPRITE_INITIAL_AMT;
	pev->rendercolor = Vector(225, 50, 175);
	pev->framerate = 20.0f;
	m_maxFrame = (float)MODEL_FRAMES(pev->modelindex) - 1;

	m_lastTime = gpGlobals->time;

	SetThink(&CKingpinPlasmaCluster::AnimateThink);
	pev->nextthink = gpGlobals->time;

	EMIT_SOUND(ENT(pev), CHAN_BODY, "debris/beamstart1.wav", 1.0f, ATTN_STATIC);
}

void CKingpinPlasmaCluster::StartUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	SetUse(NULL);
	Start();
}

// UpdateOnRemove() is not virtual in this SDK, so effects are cleaned explicitly before the cluster removes itself
void CKingpinPlasmaCluster::RemoveEffects()
{
	for (int i = 0; i < KINGPIN_CLUSTER_PARTICLE_COUNT; ++i)
	{
		if (m_particles[i])
		{
			UTIL_Remove(m_particles[i]);
			m_particles[i] = nullptr;
		}
		if (m_beams[i])
		{
			UTIL_Remove(m_beams[i]);
			m_beams[i] = nullptr;
		}
	}
}

void CKingpinPlasmaCluster::AnimateThink()
{
	pev->frame += pev->framerate * (gpGlobals->time - m_lastTime);

	if (pev->frame <= m_maxFrame * 0.5f)
	{
		pev->renderamt = KINGPIN_CLUSTER_SPRITE_INITIAL_AMT + (255.0f - KINGPIN_CLUSTER_SPRITE_INITIAL_AMT) * pev->frame / (m_maxFrame * 0.5f);
	}
	else
	{
		pev->renderamt = 255.0f;
	}

	if (pev->frame >= m_maxFrame)
	{
		Explode();
	}
	else
	{
		pev->nextthink = gpGlobals->time + 0.1f;
		m_lastTime = gpGlobals->time;
	}
}

void CKingpinPlasmaCluster::Explode()
{
	pev->movetype = MOVETYPE_NONE;
	pev->frame = 0;

	SET_MODEL(ENT(pev), "sprites/redflare2.spr");
	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 255;
	pev->rendercolor = Vector(225, 0, 150);
	pev->scale = 1.5f;
	m_maxFrame = (float)MODEL_FRAMES(pev->modelindex) - 1;

	MakeParticleBurst();
	pev->dmgtime = gpGlobals->time + KINGPIN_CLUSTER_DISPERSE_TIME;
	MakeELight();
	SetThink(&CKingpinPlasmaCluster::DisperseThink);
	pev->nextthink = gpGlobals->time + 0.05f;

	EMIT_SOUND(ENT(pev), CHAN_BODY, "debris/beamstart14.wav", 1.0f, ATTN_STATIC);
}

void CKingpinPlasmaCluster::MakeParticleBurst()
{
	const Vector center = pev->origin;

	float theta = 0.0f;
	for (int i = 0; i < KINGPIN_CLUSTER_PARTICLE_COUNT; ++i)
	{
		CPlasmaClusterParticle* pParticle = (CPlasmaClusterParticle*)CBaseEntity::Create("plasma_cluster_particle", center, pev->angles, edict());
		if (pParticle)
		{
			const bool altColor = i % 2 == 1;
			if (altColor)
				pParticle->SetAltColor();

			m_particles[i] = pParticle;

			const float phi = RANDOM_FLOAT(0, M_PI_F * 0.5f);
			theta += 2.0f * M_PI_F / KINGPIN_CLUSTER_PARTICLE_COUNT;
			const Vector direction(cos(theta) * sin(phi), sin(theta) * sin(phi), cos(phi));
			pParticle->pev->velocity = direction * KINGPIN_CLUSTER_PARTICLE_SPEED;
			pParticle->pev->velocity.z /= 2.0f;

			CBeam* beam = CBeam::BeamCreate("sprites/plasma.spr", 80);
			if (beam)
			{
				if (altColor)
					beam->SetColor(140, 100, 225);
				else
					beam->SetColor(180, 70, 140);
				beam->SetBrightness(KINGPIN_CLUSTER_BEAM_BRIGHTNESS);
				beam->SetNoise(80);
				beam->SetScrollRate(30);
				beam->EntsInit(entindex(), pParticle->entindex());
				m_beams[i] = beam;
			}
		}
	}
}

void CKingpinPlasmaCluster::EmitZapSound(float volume, bool useAltChannel, CBaseEntity* pSource)
{
	static const char* const zapSounds[] = {"debris/zap1.wav", "debris/zap3.wav", "debris/zap8.wav"};
	edict_t* pSourceEdict = pSource ? pSource->edict() : edict();
	EMIT_SOUND_DYN(pSourceEdict, useAltChannel ? CHAN_WEAPON : CHAN_ITEM, zapSounds[RANDOM_LONG(0, 2)], volume, ATTN_STATIC, 0, 120);
}

void CKingpinPlasmaCluster::BeamDamage(float flDamage)
{
	ClearMultiDamage();

	entvars_t* pevAttacker = pev;
	if (!FNullEnt(pev->owner))
	{
		CBaseEntity* pOwner = CBaseEntity::Instance(pev->owner);
		if (pOwner)
			pevAttacker = pOwner->pev;
	}

	for (int i = 0; i < KINGPIN_CLUSTER_PARTICLE_COUNT; ++i)
	{
		CBaseEntity* pParticle = m_particles[i];
		CBeam* pBeam = m_beams[i];

		if (pParticle && pBeam && pBeam->pev->renderamt > 0)
		{
			TraceResult tr;
			UTIL_TraceHull(pev->origin, pParticle->pev->origin, dont_ignore_monsters, head_hull, ENT(pevAttacker), &tr);
			if (tr.flFraction != 1.0f && !FNullEnt(tr.pHit))
			{
				CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
				if (pHit && KingpinIsCreature(pHit) && pHit->IsAlive())
				{
					// damage scales with how much the beam has faded
					const float damage = ceil(flDamage * pBeam->pev->renderamt / KINGPIN_CLUSTER_BEAM_BRIGHTNESS);
					if (damage >= 2)
					{
						TraceResult* ptr = &tr;

						TraceResult tr2;
						UTIL_TraceLine(pev->origin, pParticle->pev->origin, dont_ignore_monsters, ENT(pevAttacker), &tr2);
						if (tr.pHit == tr2.pHit)
							ptr = &tr2;

						pHit->TraceAttack(pevAttacker, damage, (tr.vecEndPos - pev->origin).Normalize(), ptr, DMG_SHOCK);

						if (zapSoundTime < gpGlobals->time)
						{
							const bool useAlt = lastZapSoundChannel == CHAN_ITEM;
							lastZapSoundChannel = useAlt ? CHAN_WEAPON : CHAN_ITEM;
							EmitZapSound(0.9f, useAlt, pParticle);
							zapSoundTime = gpGlobals->time + 0.5f;
						}
					}
				}
			}
		}
	}
	ApplyMultiDamage(pev, pevAttacker);
}

void CKingpinPlasmaCluster::DisperseThink()
{
	pev->nextthink = gpGlobals->time + 0.1f;
	const bool shouldFade = pev->dmgtime <= gpGlobals->time;

	for (int i = 0; i < KINGPIN_CLUSTER_PARTICLE_COUNT; ++i)
	{
		CBaseEntity* pParticle = m_particles[i];
		if (pParticle)
		{
			if (pParticle->pev->movetype == MOVETYPE_FLY)
			{
				pParticle->pev->velocity = pParticle->pev->velocity - pParticle->pev->velocity.Normalize() * KINGPIN_CLUSTER_PARTICLE_SPEED_STEP;
				const float particleSpeed = pParticle->pev->velocity.Length();
				if (particleSpeed < KINGPIN_CLUSTER_PARTICLE_SPEED_STEP)
				{
					pParticle->pev->velocity = g_vecZero;
					pParticle->pev->movetype = MOVETYPE_TOSS;
				}
			}
			if (shouldFade)
			{
				pParticle->pev->velocity = g_vecZero;
				pParticle->pev->movetype = MOVETYPE_TOSS;
			}
		}
	}

	if (shouldFade)
	{
		pev->movetype = MOVETYPE_TOSS;
		pev->gravity = 0.01f;

		MakeELight();
		pev->dmgtime = gpGlobals->time + KINGPIN_CLUSTER_FALL_TIME;
		SetThink(&CKingpinPlasmaCluster::FallThink);
		pev->nextthink = gpGlobals->time;
	}
	else
	{
		BeamDamage(gSkillData.kingpinLightning);
	}
}

void CKingpinPlasmaCluster::TurnOffRandomBeams(int count)
{
	for (int i = 0; i < count; ++i)
	{
		int index = RANDOM_LONG(0, KINGPIN_CLUSTER_PARTICLE_COUNT - 1);
		CBeam* beam = m_beams[index];
		if (beam)
		{
			beam->SetBrightness(0);
		}
	}
}

void CKingpinPlasmaCluster::FallThink()
{
	pev->nextthink = gpGlobals->time + 0.05f;

	float coef = (pev->dmgtime - gpGlobals->time) / KINGPIN_CLUSTER_FALL_TIME;
	if (coef <= 0.0f)
	{
		RemoveEffects();
		SetThink(&CBaseEntity::SUB_Remove);
		pev->nextthink = gpGlobals->time;
		return;
	}

	pev->renderamt = coef * 255.0f;
	for (int i = 0; i < KINGPIN_CLUSTER_PARTICLE_COUNT; ++i)
	{
		CBaseEntity* pParticle = m_particles[i];
		if (pParticle)
		{
			pParticle->pev->renderamt = coef * KINGPIN_CLUSTER_PARTICLE_BRIGHTNESS;
		}
		CBeam* pBeam = m_beams[i];
		if (pBeam && (pBeam->pev->renderamt > 0 || zapTime < gpGlobals->time))
		{
			pBeam->SetBrightness(coef * KINGPIN_CLUSTER_BEAM_BRIGHTNESS);
		}
	}

	if (zapTime < gpGlobals->time && coef > 0.2f)
	{
		if (zapSoundTime < gpGlobals->time)
		{
			const bool useAlt = lastZapSoundChannel == CHAN_ITEM;
			lastZapSoundChannel = useAlt ? CHAN_WEAPON : CHAN_ITEM;
			EmitZapSound(coef, useAlt, nullptr);
			zapSoundTime = gpGlobals->time + 0.6f;
		}
		zapTime = gpGlobals->time + 0.3f;

		TurnOffRandomBeams(2);
	}

	BeamDamage(gSkillData.kingpinLightning * 0.5f);
}

void CKingpinPlasmaCluster::MakeELight()
{
	KingpinSendEntLight(entindex(), pev->origin + Vector(0, 0, -32), 256, 180, 70, 140, 2.5f, 0);
}

//=========================================================
// Kingpin
//=========================================================
#define SF_KINGPIN_ESCAPE 32768 // spawnflag: "Escape on death" (same bit as Featureful's SF_MONSTER_SPECIAL_FLAG, keeps FGD/map compatibility)
#define bits_MEMORY_GOING_TO_USE_SECOND_CHANCE bits_MEMORY_CUSTOM2

#define KINGPIN_GLOW_COUNT 4
#define KINGPIN_GLOW_MAX_AMT 255
#define KINGPIN_GIB_COUNT 10
#define KINGPIN_GIB_SUBMODELS 4 // number of bodies in models/stickygibpink.mdl used for random gibs (body index wraps in the engine if the model has fewer)

#define KINGPIN_SHIELD_REGEN_DELAY 3.0f	  // seconds without taking damage before the shield starts regenerating
#define KINGPIN_SHIELD_REGEN_INTERVAL 0.2f // regen tick
#define KINGPIN_SHIELD_REGEN_STEP 5.0f	  // shield points restored per tick (taken from the reserve)

static const GibData KingpinGibs = {"models/stickygibpink.mdl", 0, KINGPIN_GIB_SUBMODELS};

// TE_BEAMPOINTS with framerate
static void KingpinSendBeamPoints(const Vector& start, const Vector& end, int modelIndex, int framerate,
	float life, int width, int noise, int r, int g, int b, int brightness, int scroll)
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
	WRITE_BYTE(0);
	WRITE_BYTE(framerate);
	WRITE_BYTE((int)(life * 10));
	WRITE_BYTE(width);
	WRITE_BYTE(noise);
	WRITE_BYTE(r);
	WRITE_BYTE(g);
	WRITE_BYTE(b);
	WRITE_BYTE(brightness);
	WRITE_BYTE(scroll);
	MESSAGE_END();
}

// TE_BEAMENTPOINT from an entity attachment to a point
static void KingpinSendBeamEntPoint(int entIndex, int attachment, const Vector& end, int modelIndex, int framerate,
	float life, int width, int noise, int r, int g, int b, int brightness, int scroll)
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, end);
	WRITE_BYTE(TE_BEAMENTPOINT);
	WRITE_SHORT(entIndex + 0x1000 * attachment);
	WRITE_COORD(end.x);
	WRITE_COORD(end.y);
	WRITE_COORD(end.z);
	WRITE_SHORT(modelIndex);
	WRITE_BYTE(0);
	WRITE_BYTE(framerate);
	WRITE_BYTE((int)(life * 10));
	WRITE_BYTE(width);
	WRITE_BYTE(noise);
	WRITE_BYTE(r);
	WRITE_BYTE(g);
	WRITE_BYTE(b);
	WRITE_BYTE(brightness);
	WRITE_BYTE(scroll);
	MESSAGE_END();
}

// Creates an additive sprite that plays once and then disappears
static CSprite* KingpinCreateOneShotSprite(const char* spriteName, const Vector& origin, float scale, int r, int g, int b, int amt, float framerate)
{
	CSprite* pSprite = CSprite::SpriteCreate(spriteName, origin, true);
	if (pSprite)
	{
		pSprite->SetTransparency(kRenderTransAdd, r, g, b, amt, kRenderFxNoDissipation);
		pSprite->SetScale(scale);
		pSprite->AnimateAndDie(framerate);
	}
	return pSprite;
}

class CKingpin : public CSquadMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void SetYawSpeed() override { pev->yaw_speed = 140; }
	int Classify() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	bool KeyValue(KeyValueData* pkvd) override;
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	float MaximumShield() const { return pev->armortype; }

	void Killed(entvars_t* pevAttacker, int iGib) override;
	void BecomeDead() override;

	void IdleSound() override;
	void AlertSound() override;
	void PainSound() override;
	void DeathSound() override;

	bool CheckRangeAttack1(float flDot, float flDist) override { return false; }
	bool CheckRangeAttack2(float flDot, float flDist) override;
	bool CheckMeleeAttack1(float flDot, float flDist) override;

	Schedule_t* GetSchedule() override;
	Schedule_t* GetScheduleOfType(int Type) override;
	void StartTask(Task_t* pTask) override;
	void RunTask(Task_t* pTask) override;
	void PrescheduleThink() override;

	void SetObjectCollisionBox() override
	{
		pev->absmin = pev->origin + Vector(-24.0f, -24.0f, 0.0f);
		pev->absmax = pev->origin + Vector(24.0f, 24.0f, 96.0f);
	}

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	CUSTOM_SCHEDULES;

	void ReportAIState() override;

	float m_plasmaBallTime;
	float m_plasmaClusterTime;
	CSprite* m_Glows[KINGPIN_GLOW_COUNT];
	CKingpinPlasmaBall* m_plasmaBall;
	string_t m_sTeleportTarget;
	bool m_isTeleporting;
	bool m_canUseSecondChance;

	// power shield (local replacement for Featureful's generic power shield)
	float m_shieldReserve;	  // pool of shield points used for regeneration
	float m_nextShieldRegen;
	float m_shieldFlashEnd;

protected:
	void UpdateGlows(int target, int speed);
	void ClearGlows();
	void DeathBeams(int r, int g, int b, int width, float life);
	void ClearPlasmaBall();
	Vector PlasmaBallPos();
	void TryMakePlasmaCluster(const Vector& pos);
	bool CanTeleportNow();
	void RemovePowerShield();
	void UpdatePowerShield();
	void EndTeleportState();

	// not saved, refilled by Precache() (called on restore too)
	int m_iFunnelParticle;
	int m_iShockwave;
	int m_iLightning;
	int m_iPlasma;

	static const char* pIdleSounds[];
	static const char* pAlertSounds[];
	static const char* pPainSounds[];
	static const char* pDieSounds[];
	static const char* pAttackHitSounds[];
	static const char* pAttackMissSounds[];
};

LINK_ENTITY_TO_CLASS(monster_kingpin, CKingpin);

TYPEDESCRIPTION CKingpin::m_SaveData[] =
	{
		DEFINE_FIELD(CKingpin, m_plasmaBallTime, FIELD_TIME),
		DEFINE_FIELD(CKingpin, m_plasmaClusterTime, FIELD_TIME),
		DEFINE_ARRAY(CKingpin, m_Glows, FIELD_CLASSPTR, KINGPIN_GLOW_COUNT),
		DEFINE_FIELD(CKingpin, m_plasmaBall, FIELD_CLASSPTR),
		DEFINE_FIELD(CKingpin, m_sTeleportTarget, FIELD_STRING),
		DEFINE_FIELD(CKingpin, m_isTeleporting, FIELD_BOOLEAN),
		DEFINE_FIELD(CKingpin, m_canUseSecondChance, FIELD_BOOLEAN),
		DEFINE_FIELD(CKingpin, m_shieldReserve, FIELD_FLOAT),
		DEFINE_FIELD(CKingpin, m_nextShieldRegen, FIELD_TIME),
		DEFINE_FIELD(CKingpin, m_shieldFlashEnd, FIELD_TIME),
};

IMPLEMENT_SAVERESTORE(CKingpin, CSquadMonster);

const char* CKingpin::pIdleSounds[] =
	{
		"kingpin/kingpin_idle1.wav",
		"kingpin/kingpin_idle2.wav",
		"kingpin/kingpin_idle3.wav",
};

const char* CKingpin::pAlertSounds[] =
	{
		"kingpin/kingpin_alert1.wav",
		"kingpin/kingpin_alert2.wav",
		"kingpin/kingpin_alert3.wav",
};

const char* CKingpin::pPainSounds[] =
	{
		"kingpin/kingpin_pain1.wav",
		"kingpin/kingpin_pain2.wav",
		"kingpin/kingpin_pain3.wav",
};

const char* CKingpin::pDieSounds[] =
	{
		"kingpin/kingpin_death1.wav",
		"kingpin/kingpin_death2.wav",
};

const char* CKingpin::pAttackHitSounds[] =
	{
		"zombie/claw_strike1.wav",
		"zombie/claw_strike2.wav",
		"zombie/claw_strike3.wav",
};

const char* CKingpin::pAttackMissSounds[] =
	{
		"zombie/claw_miss1.wav",
		"zombie/claw_miss2.wav",
};

bool CKingpin::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "teleport_target"))
	{
		m_sTeleportTarget = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "second_chance"))
	{
		m_canUseSecondChance = atoi(pkvd->szValue) != 0;
		return true;
	}

	return CSquadMonster::KeyValue(pkvd);
}

//=========================================================
// Schedules
//=========================================================
Task_t tlKingpinTeleport[] =
	{
		{TASK_STOP_MOVING, 0},
		{TASK_KINGPIN_TELEPORT, (float)0},
		{TASK_SET_ACTIVITY, (float)ACT_IDLE},
};

Schedule_t slKingpinTeleport[] =
	{
		{tlKingpinTeleport,
			ARRAYSIZE(tlKingpinTeleport),
			0,
			0,
			"Kingpin Teleport"},
};

Task_t tlKingpinFail[] =
	{
		{TASK_STOP_MOVING, 0},
		{TASK_SET_ACTIVITY, (float)ACT_IDLE},
		{TASK_WAIT, (float)1},
		{TASK_WAIT_PVS, (float)0},
};

Schedule_t slKingpinFail[] =
	{
		{tlKingpinFail,
			ARRAYSIZE(tlKingpinFail),
			bits_COND_CAN_ATTACK,
			0,
			"Kingpin Fail"},
};

Task_t tlKingpinCombatFail[] =
	{
		{TASK_STOP_MOVING, 0},
		{TASK_SET_ACTIVITY, (float)ACT_IDLE},
		{TASK_WAIT_FACE_ENEMY, 1.0f},
		{TASK_WAIT_PVS, 0.0f},
};

Schedule_t slKingpinCombatFail[] =
	{
		{tlKingpinCombatFail,
			ARRAYSIZE(tlKingpinCombatFail),
			bits_COND_CAN_ATTACK,
			0,
			"Kingpin Combat Fail"},
};

Task_t tlKingpinRangeAttack2[] =
	{
		{TASK_STOP_MOVING, 0},
		{TASK_FACE_ENEMY, (float)0},
		{TASK_RANGE_ATTACK2, (float)0},
};

Schedule_t slKingpinRangeAttack2[] =
	{
		{tlKingpinRangeAttack2,
			ARRAYSIZE(tlKingpinRangeAttack2),
			bits_COND_NEW_ENEMY |
				bits_COND_ENEMY_DEAD |
				bits_COND_ENEMY_OCCLUDED |
				bits_COND_HEAVY_DAMAGE |
				bits_COND_NO_AMMO_LOADED,
			0,
			"Kingpin Range Attack2"},
};

DEFINE_CUSTOM_SCHEDULES(CKingpin){
	slKingpinTeleport,
	slKingpinFail,
	slKingpinCombatFail,
	slKingpinRangeAttack2,
};

IMPLEMENT_CUSTOM_SCHEDULES(CKingpin, CSquadMonster);

void CKingpin::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/kingpin.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_YELLOW;
	pev->health = gSkillData.kingpinHealth;
	pev->max_health = pev->health;
	m_flFieldOfView = -1.0f; // VIEW_FIELD_FULL
	m_MonsterState = MONSTERSTATE_NONE;
	m_afCapability = bits_CAP_DOORS_GROUP;

	m_plasmaBallTime = 0;
	m_plasmaClusterTime = 0;
	m_plasmaBall = nullptr;
	m_isTeleporting = false;

	// power shield: armorvalue is the current shield, armortype the maximum
	pev->armortype = gSkillData.kingpinShield;
	pev->armorvalue = pev->armortype;
	m_shieldReserve = gSkillData.kingpinShieldReserve;
	m_nextShieldRegen = 0;
	m_shieldFlashEnd = 0;

	for (int i = 0; i < KINGPIN_GLOW_COUNT; ++i)
	{
		m_Glows[i] = CSprite::SpriteCreate("sprites/boss_glow.spr", pev->origin, true);
		if (m_Glows[i])
		{
			m_Glows[i]->SetTransparency(kRenderTransAdd, 255, 255, 255, 0, kRenderFxNoDissipation);
			m_Glows[i]->SetScale(0.2f);
			m_Glows[i]->pev->framerate = 10.0f;
			m_Glows[i]->SetBrightness(0);
			m_Glows[i]->SetAttachment(edict(), i + 1);
		}
	}

	MonsterInit();
}

void CKingpin::Precache()
{
	int i;

	PRECACHE_MODEL("models/kingpin.mdl");
	PRECACHE_MODEL("models/stickygibpink.mdl");
	PRECACHE_MODEL("sprites/boss_glow.spr");
	PRECACHE_MODEL("sprites/b-tele1.spr");
	PRECACHE_MODEL("sprites/d-tele1.spr");
	PRECACHE_MODEL("sprites/xflare3.spr");

	PRECACHE_SOUND("kingpin/kingpin_moveslow.wav");
	PRECACHE_SOUND("kingpin/kingpin_move.wav");

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

	PRECACHE_SOUND("ambience/port_suckin1.wav");
	PRECACHE_SOUND("debris/beamstart7.wav");
	PRECACHE_SOUND("ambience/particle_suck1.wav");
	PRECACHE_SOUND("ambience/alien_humongo.wav");
	PRECACHE_SOUND("debris/beamstart10.wav");

	m_iShockwave = PRECACHE_MODEL("sprites/shockwave.spr");
	m_iLightning = PRECACHE_MODEL("sprites/lgtning.spr");
	m_iPlasma = PRECACHE_MODEL("sprites/plasma.spr");
	m_iFunnelParticle = PRECACHE_MODEL("sprites/redflare2.spr");

	UTIL_PrecacheOther("kingpin_plasma_ball");
	UTIL_PrecacheOther("kingpin_plasma_cluster");
}

int CKingpin::Classify()
{
	if (m_AltClass)
		return CLASS_PLAYER_ALIEN_ALLY;

	return CLASS_ALIEN_MONSTER;
}

void CKingpin::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case KINGPIN_AE_LEFT:
	case KINGPIN_AE_RIGHT:
	{
		const float side = pEvent->event == KINGPIN_AE_LEFT ? 1.0f : -1.0f;

		CBaseEntity* pHurt = CheckTraceHullAttack(KINGPIN_MELEE_ATTACK_DISTANCE, gSkillData.kingpinMelee, DMG_CLUB);
		if (pHurt)
		{
			pHurt->pev->punchangle.z = 18 * side;
			pHurt->pev->punchangle.x = 5;
			if ((pHurt->pev->flags & (FL_MONSTER | FL_CLIENT)) != 0)
			{
				// CheckTraceHullAttack already built the aim vectors
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_right * 100 * side;
			}
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, pAttackHitSounds[RANDOM_LONG(0, ARRAYSIZE(pAttackHitSounds) - 1)], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5, 5));
		}
		else
		{
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, pAttackMissSounds[RANDOM_LONG(0, ARRAYSIZE(pAttackMissSounds) - 1)], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5, 5));
		}
	}
	break;
	case KINGPIN_AE_PLASMA_START:
	{
		m_plasmaBall = (CKingpinPlasmaBall*)CBaseEntity::Create("kingpin_plasma_ball", PlasmaBallPos(), pev->angles, edict());
	}
	break;
	case KINGPIN_AE_PLASMA_LAUNCH:
		if (m_plasmaBall)
		{
			UTIL_MakeAimVectors(pev->angles);
			m_plasmaBall->m_hEnemy = m_hEnemy;
			m_plasmaBall->pev->velocity = gpGlobals->v_forward * 32;
			m_plasmaBall->Launch();
			m_plasmaBall = nullptr;
			m_plasmaBallTime = gpGlobals->time + KINGPIN_PLASMABALL_DELAY;
		}
		break;
	case KINGPIN_AE_PLASMA_END:
		break;
	default:
		CSquadMonster::HandleAnimEvent(pEvent);
		break;
	}
}

bool CKingpin::CheckMeleeAttack1(float flDot, float flDist)
{
	// replacement for CheckMeleeAttackImpl
	if (m_hEnemy != NULL && flDist <= KINGPIN_MELEE_ATTACK_CHECK_DISTANCE && flDot >= 0.7f)
	{
		return true;
	}
	return false;
}

void CKingpin::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	if (m_isTeleporting || pev->takedamage == DAMAGE_NO)
		return;

	// Head multiplier is capped by kingpin_head (sk_kingpin_head) if it's lower than the default one
	if (ptr->iHitgroup == HITGROUP_HEAD)
	{
		const float kingpinMultiplier = gSkillData.kingpinHead;
		const float defaultMultiplier = gSkillData.monHead;
		if (kingpinMultiplier > 0.0f && defaultMultiplier > 0.0f && kingpinMultiplier < defaultMultiplier)
		{
			// CBaseMonster::TraceAttack multiplies by monHead, so scale it down to the Kingpin's value
			flDamage *= kingpinMultiplier / defaultMultiplier;
		}
	}

	CSquadMonster::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
}

bool CKingpin::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (m_isTeleporting)
		return false;

	// Power shield absorbs damage before health is touched
	if (pev->armorvalue > 0 && IsAlive() && (bitsDamageType & (DMG_FALL | DMG_DROWN)) == 0)
	{
		const float absorbed = flDamage < pev->armorvalue ? flDamage : pev->armorvalue;
		pev->armorvalue -= absorbed;
		flDamage -= absorbed;

		m_nextShieldRegen = gpGlobals->time + KINGPIN_SHIELD_REGEN_DELAY;

		// visual feedback: short glow shell
		pev->renderfx = kRenderFxGlowShell;
		pev->rendercolor = Vector(255, 170, 255);
		pev->renderamt = 25;
		m_shieldFlashEnd = gpGlobals->time + 0.15f;

		if (pev->armorvalue <= 0)
		{
			pev->armorvalue = 0;
			EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "debris/beamstart7.wav", 0.8f, ATTN_NORM, 0, 130);
		}
	}

	// A fully absorbed hit still goes through the base class so the monster reacts to its attacker (0 damage)
	return CSquadMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

void CKingpin::Killed(entvars_t* pevAttacker, int iGib)
{
	ClearPlasmaBall();
	ClearGlows();
	EndTeleportState();
	pev->armorvalue = 0;

	// Never gib, always wait for death animation
	CSquadMonster::Killed(pevAttacker, GIB_NEVER);
}

void CKingpin::BecomeDead()
{
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_TOSS;
}

bool CKingpin::CheckRangeAttack2(float flDot, float flDist)
{
	if (m_plasmaBallTime > gpGlobals->time)
		return false;
	return CSquadMonster::CheckRangeAttack2(flDot, flDist);
}

Schedule_t* CKingpin::GetSchedule()
{
	if (HasConditions(bits_COND_HEAR_SOUND))
	{
		CSound* pSound = PBestSound();

		ASSERT(pSound != NULL);
		if (pSound && (pSound->m_iType & bits_SOUND_DANGER) != 0)
		{
			if (CanTeleportNow())
			{
				return GetScheduleOfType(SCHED_KINGPIN_TELEPORT);
			}
			return GetScheduleOfType(SCHED_TAKE_COVER_FROM_BEST_SOUND);
		}
	}

	return CSquadMonster::GetSchedule();
}

extern Schedule_t slChaseEnemyFailed[];

Schedule_t* CKingpin::GetScheduleOfType(int Type)
{
	switch (Type)
	{
	case SCHED_ALERT_SMALL_FLINCH:
	case SCHED_ALERT_BIG_FLINCH:
	case SCHED_SMALL_FLINCH:
	{
		if (HasConditions(bits_COND_HEAR_SOUND))
		{
			return GetScheduleOfType(SCHED_ALERT_FACE);
		}
		else
		{
			return GetScheduleOfType(SCHED_ALERT_STAND);
		}
	}
	case SCHED_FAIL:
	{
		if (m_hEnemy != NULL)
			return slKingpinCombatFail;
		else
			return slKingpinFail;
	}
	break;
	case SCHED_RANGE_ATTACK2:
		return slKingpinRangeAttack2;
	case SCHED_CHASE_ENEMY_FAILED:
		if (CanTeleportNow())
		{
			return GetScheduleOfType(SCHED_KINGPIN_TELEPORT);
		}
		else
		{
			return slChaseEnemyFailed;
		}
		break;
	case SCHED_KINGPIN_TELEPORT:
		return slKingpinTeleport;
	default:
		break;
	}
	return CSquadMonster::GetScheduleOfType(Type);
}

void CKingpin::StartTask(Task_t* pTask)
{
	ClearPlasmaBall();
	switch (pTask->iTask)
	{
	case TASK_KINGPIN_TELEPORT:
	{
		m_IdealActivity = ACT_DIESIMPLE;
		m_isTeleporting = true;
		pev->teleport_time = gpGlobals->time + KINGPIN_TELEPORT_INTERVAL;
		RemovePowerShield();

		pev->rendermode = kRenderTransAdd;
		pev->renderfx = kRenderFxPulseFastWide;
		pev->renderamt = 225;

		m_flWaitFinished = gpGlobals->time + KINGPIN_TELEPORT_DELAY;
		CSprite* enterTeleportSprite = KingpinCreateOneShotSprite("sprites/b-tele1.spr", Center() + Vector(0, 0, pev->size.z / 2), 1.0f, 255, 255, 255, 255, 15.0f);
		if (enterTeleportSprite)
		{
			EMIT_SOUND(enterTeleportSprite->edict(), CHAN_BODY, "ambience/port_suckin1.wav", 1.0f, ATTN_NORM);
		}
		break;
	}
	case TASK_DIE:
		pev->framerate = 0.5;
		if (FBitSet(pev->spawnflags, SF_KINGPIN_ESCAPE))
		{
			DeathBeams(215, 225, 145, 40, 2.0f);

			CSprite* escapeSprite = KingpinCreateOneShotSprite("sprites/d-tele1.spr", Center() + Vector(0, 0, pev->size.z / 2), 2.5f, 170, 170, 255, 70, 10.0f);
			if (escapeSprite)
			{
				EMIT_SOUND(escapeSprite->edict(), CHAN_ITEM, "ambience/particle_suck1.wav", 1.0f, ATTN_NORM);
			}
		}
		else
		{
			DeathBeams(170, 85, 127, 70, 2.0f);
		}
		m_flWaitFinished = gpGlobals->time + 2.0;
		CSquadMonster::StartTask(pTask);
		break;
	default:
		CSquadMonster::StartTask(pTask);
		break;
	}
}

void CKingpin::RunTask(Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_KINGPIN_TELEPORT:
	{
		if (!IsAlive())
		{
			TaskFail();
		}
		else if (gpGlobals->time >= m_flWaitFinished)
		{
			m_IdealActivity = ACT_IDLE;
			if (FStringNull(m_sTeleportTarget))
			{
				ALERT(at_aiconsole, "%s: no teleport target defined\n", STRING(pev->classname));
				TaskFail();
			}
			else
			{
				CBaseEntity* pEntity = NULL;
				CBaseEntity* pBestSpot = NULL;
				int bestDistance = 8192;
				while ((pEntity = UTIL_FindEntityByTargetname(pEntity, STRING(m_sTeleportTarget))) != NULL)
				{
					TraceResult tr;
					UTIL_TraceHull(pEntity->pev->origin + Vector(0, 0, pev->size.z / 2.0f), pEntity->pev->origin + Vector(0, 0, pev->size.z / 2.0f + 1), dont_ignore_monsters, human_hull, edict(), &tr);
					if (0 == tr.fStartSolid && 0 == tr.fAllSolid)
					{
						int distance;
						int distanceToSpot = (pev->origin - pEntity->pev->origin).Length();
						if (m_hEnemy != NULL)
						{
							distance = (m_hEnemy->pev->origin - pEntity->pev->origin).Length();
						}
						else
						{
							distance = distanceToSpot;
						}

						if (distanceToSpot >= 192 && distance < bestDistance)
						{
							pBestSpot = pEntity;
							bestDistance = distance;
						}
					}
				}
				if (pBestSpot)
				{
					pev->flags &= ~FL_ONGROUND;
					pev->effects |= EF_NOINTERP;
					if (m_hEnemy != NULL)
					{
						pev->angles.y = pev->ideal_yaw = UTIL_VecToYaw(m_hEnemy->pev->origin - pBestSpot->pev->origin);
					}
					const Vector startBeamPos = Center() + Vector(0, 0, pev->size.z / 4);
					UTIL_SetOrigin(pev, pBestSpot->pev->origin + Vector(0, 0, 1));
					const Vector endBeamPos = Center() + Vector(0, 0, pev->size.z / 4);

					KingpinSendBeamPoints(startBeamPos, endBeamPos, m_iShockwave, 0, 0.7f, 120, 64, 225, 225, 255, 120, 0);

					CSprite* exitTeleportSprite = KingpinCreateOneShotSprite("sprites/b-tele1.spr", Center() + Vector(0, 0, pev->size.z / 4), 1.0f, 255, 255, 255, 255, 15.0f);
					if (exitTeleportSprite)
					{
						EMIT_SOUND(exitTeleportSprite->edict(), CHAN_BODY, "debris/beamstart7.wav", 1.0f, ATTN_STATIC);
					}

					if (m_canUseSecondChance && HasMemory(bits_MEMORY_GOING_TO_USE_SECOND_CHANCE))
					{
						ALERT(at_aiconsole, "%s used second chance. Health: %f. Armor: %f\n", STRING(pev->classname), pev->health, pev->armorvalue);
						m_canUseSecondChance = false;
						Forget(bits_MEMORY_GOING_TO_USE_SECOND_CHANCE);
						pev->armorvalue = MaximumShield();
					}

					UTIL_ScreenShake(pev->origin, 6.0f, 50.0f, 1.0f, 400);

					m_plasmaBallTime = V_max(gpGlobals->time + 0.5f, m_plasmaBallTime);
					m_plasmaClusterTime = V_max(gpGlobals->time + 0.5f, m_plasmaClusterTime);

					TaskComplete();
				}
				else
				{
					ALERT(at_aiconsole, "%s: could not find a valid teleport spot\n", STRING(pev->classname));
					TaskFail();
				}
			}
		}
	}
	break;
	case TASK_DIE:
		if (gpGlobals->time > m_flWaitFinished)
		{
			if (FBitSet(pev->spawnflags, SF_KINGPIN_ESCAPE))
			{
				DeathBeams(75, 210, 130, 40, 2.0f);
				StopAnimation();

				CSprite* escapeFlare = KingpinCreateOneShotSprite("sprites/xflare3.spr", Center() + Vector(0, 0, pev->size.z / 2), 1.25f, 170, 255, 127, 255, 10.0f);
				if (escapeFlare)
				{
					EMIT_SOUND_DYN(escapeFlare->edict(), CHAN_ITEM, "ambience/alien_humongo.wav", 1.0f, ATTN_NORM, 0, 150);
				}

				const float escapeDamage = gSkillData.kingpinPlasmaBlast / 2;
				::RadiusDamage(pev->origin, pev, pev, escapeDamage, escapeDamage * 2.5f, Classify(), DMG_SHOCK);
				UTIL_ScreenFadeAll(Vector(85, 255, 127), 2.0f, 0.0f, 200, FFADE_IN);

				const Vector shockWavePos = Center();
				KingpinSendBeamWave(TE_BEAMCYLINDER, shockWavePos, 1500, m_iShockwave, 0.2f, 255, 0, 85, 255, 127, 255);

				MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
				WRITE_BYTE(TE_LARGEFUNNEL);
				WRITE_COORD(shockWavePos.x);
				WRITE_COORD(shockWavePos.y);
				WRITE_COORD(shockWavePos.z);
				WRITE_SHORT(m_iFunnelParticle);
				WRITE_SHORT(1);
				MESSAGE_END();

				SetThink(&CBaseEntity::SUB_Remove);
				pev->nextthink = gpGlobals->time;
			}
			else
			{
				DeathBeams(170, 170, 255, 40, 1.0f);
				StopAnimation();
				g_vecAttackDir = Vector(0, 0, -1);
				GibMonster();
				::RadiusDamage(pev->origin, pev, pev, gSkillData.kingpinPlasmaBlast, gSkillData.kingpinPlasmaBlast * 2.5f, Classify(), DMG_SHOCK);
				m_bloodColor = BLOOD_COLOR_RED; // HACK to change blood color of pink gibs
				CGib::SpawnRandomGibs(pev, KINGPIN_GIB_COUNT, KingpinGibs);
			}
			return;
		}
		else
			CSquadMonster::RunTask(pTask);
		break;
	default:
		CSquadMonster::RunTask(pTask);
		break;
	}
}

void CKingpin::PrescheduleThink()
{
	// leaving the teleport schedule (completed, failed or interrupted) ends the teleport state
	if (m_isTeleporting && m_pSchedule != slKingpinTeleport)
	{
		EndTeleportState();
	}

	UpdatePowerShield();

	const int glowAmtStep = KINGPIN_GLOW_MAX_AMT / 10 + 1;
	int targetGlowAmt = 0;

	if (m_isTeleporting)
	{
		targetGlowAmt = 0;
	}
	else if (m_IdealMonsterState == MONSTERSTATE_ALERT || m_IdealMonsterState == MONSTERSTATE_HUNT)
	{
		targetGlowAmt = KINGPIN_GLOW_MAX_AMT / 6;
	}
	else if (m_IdealMonsterState == MONSTERSTATE_COMBAT)
	{
		if (HasConditions(bits_COND_SEE_ENEMY))
		{
			targetGlowAmt = KINGPIN_GLOW_MAX_AMT;
		}
		else
		{
			targetGlowAmt = KINGPIN_GLOW_MAX_AMT / 4;
		}
	}
	UpdateGlows(targetGlowAmt, glowAmtStep);

	if (IsMoving())
	{
		ClearPlasmaBall();
	}
	if (m_plasmaBall)
	{
		UTIL_SetOrigin(m_plasmaBall->pev, PlasmaBallPos());
	}

	if (IsAlive())
	{
		if (!m_isTeleporting && m_canUseSecondChance &&
			pev->health <= pev->max_health / 2 && pev->armorvalue <= MaximumShield() / 4 &&
			CanTeleportNow())
		{
			Remember(bits_MEMORY_GOING_TO_USE_SECOND_CHANCE);
			ChangeSchedule(GetScheduleOfType(SCHED_KINGPIN_TELEPORT));
		}
		else if (m_plasmaClusterTime <= gpGlobals->time)
		{
			CBaseEntity* pEnemy = m_hEnemy;
			if (pEnemy)
			{
				Vector enemyVelocity = pEnemy->pev->velocity * 0.75f;
				enemyVelocity.z = 0.0f;
				const Vector checkPos = pEnemy->pev->origin + enemyVelocity;
				if ((checkPos - pev->origin).Length() < KINGPIN_PLASMACLUSTER_ATTACK_DISTANCE)
					TryMakePlasmaCluster(pEnemy->EyePosition() + enemyVelocity + Vector(0, 0, 32));
			}
		}
	}

	CSquadMonster::PrescheduleThink();
}

void CKingpin::EndTeleportState()
{
	if (m_isTeleporting)
	{
		m_isTeleporting = false;
		pev->rendermode = kRenderNormal;
		pev->renderfx = kRenderFxNone;
	}
}

void CKingpin::RemovePowerShield()
{
	pev->armorvalue = 0;
	m_nextShieldRegen = gpGlobals->time + KINGPIN_SHIELD_REGEN_DELAY;
	m_shieldFlashEnd = 0;
}

void CKingpin::UpdatePowerShield()
{
	// end of hit flash
	if (m_shieldFlashEnd != 0 && gpGlobals->time >= m_shieldFlashEnd)
	{
		m_shieldFlashEnd = 0;
		if (!m_isTeleporting)
		{
			pev->rendermode = kRenderNormal;
			pev->renderfx = kRenderFxNone;
			pev->renderamt = 0;
		}
	}

	// regeneration, paid from the shield reserve
	if (IsAlive() && !m_isTeleporting && pev->armorvalue < MaximumShield() && m_shieldReserve > 0 && gpGlobals->time >= m_nextShieldRegen)
	{
		float step = KINGPIN_SHIELD_REGEN_STEP;
		if (step > m_shieldReserve)
			step = m_shieldReserve;
		if (step > MaximumShield() - pev->armorvalue)
			step = MaximumShield() - pev->armorvalue;

		pev->armorvalue += step;
		m_shieldReserve -= step;
		m_nextShieldRegen = gpGlobals->time + KINGPIN_SHIELD_REGEN_INTERVAL;
	}
}

void CKingpin::IdleSound()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, pIdleSounds[RANDOM_LONG(0, ARRAYSIZE(pIdleSounds) - 1)], 1.0, ATTN_NORM, 0, 100);
}

void CKingpin::AlertSound()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, pAlertSounds[RANDOM_LONG(0, ARRAYSIZE(pAlertSounds) - 1)], 1.0, ATTN_NORM, 0, 100);
}

void CKingpin::PainSound()
{
	if (RANDOM_LONG(0, 2) == 0)
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, pPainSounds[RANDOM_LONG(0, ARRAYSIZE(pPainSounds) - 1)], 1.0, ATTN_NORM, 0, 100);
}

void CKingpin::DeathSound()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, pDieSounds[RANDOM_LONG(0, ARRAYSIZE(pDieSounds) - 1)], 1.0, ATTN_NORM, 0, 100);
}

void CKingpin::UpdateGlows(int target, int speed)
{
	for (int i = 0; i < KINGPIN_GLOW_COUNT; ++i)
	{
		CSprite* glow = m_Glows[i];
		if (glow)
		{
			glow->pev->renderamt = UTIL_Approach(target, glow->pev->renderamt, speed);
		}
	}
}

void CKingpin::ClearGlows()
{
	for (int i = 0; i < KINGPIN_GLOW_COUNT; ++i)
	{
		if (m_Glows[i])
		{
			UTIL_Remove(m_Glows[i]);
			m_Glows[i] = nullptr;
		}
	}
}

void CKingpin::DeathBeams(int r, int g, int b, int width, float life)
{
	int iTimes = 0;
	int iDrawn = 0;
	const int iBeams = 8;
	while (iDrawn < iBeams && iTimes < (iBeams * 3))
	{
		TraceResult tr;
		const Vector vecOrigin = Center() + Vector(0, 0, pev->size.z * 0.5);
		const Vector vecDest = 1024 * (Vector(RANDOM_FLOAT(-1, 1), RANDOM_FLOAT(-1, 1), RANDOM_FLOAT(-1, 1)).Normalize());
		UTIL_TraceLine(vecOrigin, vecOrigin + vecDest, ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction != 1.0)
		{
			// we hit something.
			iDrawn++;
			KingpinSendBeamPoints(vecOrigin, tr.vecEndPos, m_iLightning, 10, life, width, 80, r, g, b, 255, 35);
		}
		iTimes++;
	}
}

void CKingpin::ClearPlasmaBall()
{
	if (m_plasmaBall)
	{
		UTIL_Remove(m_plasmaBall);
		m_plasmaBall = nullptr;
	}
}

Vector CKingpin::PlasmaBallPos()
{
	UTIL_MakeVectors(pev->angles);
	Vector vecStart, angleGun;
	GetAttachment(0, vecStart, angleGun);
	vecStart.z -= 16;
	vecStart = vecStart + gpGlobals->v_forward * 16;
	return vecStart;
}

void CKingpin::ReportAIState()
{
	CBaseMonster::ReportAIState();
	if (m_plasmaBallTime <= gpGlobals->time)
		ALERT(at_console, "Can throw an energy ball; ");
	if (m_plasmaClusterTime <= gpGlobals->time)
		ALERT(at_console, "Can create plasma cluster; ");
}

void CKingpin::TryMakePlasmaCluster(const Vector& pos)
{
	m_plasmaClusterTime = gpGlobals->time + 0.5f;

	const float checkDistance = 32.0f;

	const Vector eyePosition = EyePosition();
	const Vector dir = (pos - eyePosition).Normalize();

	TraceResult tr;
	UTIL_TraceHull(eyePosition, pos + dir * checkDistance, ignore_monsters, head_hull, edict(), &tr);
	if (tr.flFraction == 1.0f)
	{
		UTIL_TraceHull(pos, pos + Vector(0, 0, checkDistance), ignore_monsters, head_hull, edict(), &tr);
		if (tr.flFraction == 1.0f)
		{
			CBaseEntity::Create("kingpin_plasma_cluster", pos, g_vecZero, edict());
			m_plasmaClusterTime = gpGlobals->time + KINGPIN_PLASMACLUSTER_DELAY;

			float minDistanceFromEye = 4096.0f;
			int eyeAttachmentIndex = 0;
			for (int i = 0; i < KINGPIN_GLOW_COUNT; ++i)
			{
				CSprite* glow = m_Glows[i];
				if (glow)
				{
					float distanceFromEye = (glow->pev->origin - pos).Length();
					if (distanceFromEye < minDistanceFromEye)
					{
						minDistanceFromEye = distanceFromEye;
						eyeAttachmentIndex = i + 1;
					}
				}
			}

			if (eyeAttachmentIndex > 0)
			{
				EMIT_SOUND(ENT(pev), CHAN_ITEM, "debris/beamstart10.wav", 0.7f, ATTN_NORM);
				KingpinSendBeamEntPoint(entindex(), eyeAttachmentIndex, pos, m_iPlasma, 10, 1.0f, 40, 80, 180, 70, 140, 220, 10);
			}
		}
	}
}

bool CKingpin::CanTeleportNow()
{
	return pev->teleport_time <= gpGlobals->time && !FStringNull(m_sTeleportTarget);
}
