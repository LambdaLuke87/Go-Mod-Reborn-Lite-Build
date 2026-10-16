/***
*
*	LRC effects ported from the Featureful SDK to Go-Mod: Reborn SDK:
*	env_quakefx, env_shockwave, env_streak, particle_shooter and the extended env_smoker.
*
*	They use the locus system (see locus.cpp) to calculate positions and directions, and
*	env_streak / env_smoker / particle_shooter send user messages handled by cl_dll/cl_lrc_fx.cpp.
*
****/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "locus.h"
#include "weapons.h"
#include "particledef.h"
#include "lrc_fx_flags.h"

// User messages, registered from LinkUserMessages() (player.cpp) through LinkLrcEffectMessages()
int gmsgStreaks = 0;
int gmsgSmoke = 0;
int gmsgParticleShooter = 0;

void LinkLrcEffectMessages()
{
	gmsgStreaks = REG_USER_MSG("Streaks", -1);
	gmsgSmoke = REG_USER_MSG("Smoke", -1);
	gmsgParticleShooter = REG_USER_MSG("Particle", 27);
}

//=========================================================
// LRC - All the particle effects from Quake 1
//=========================================================
#define SF_QUAKEFX_REPEATABLE 1

class CEnvQuakeFx : public CPointEntity
{
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
};

LINK_ENTITY_TO_CLASS(env_quakefx, CEnvQuakeFx);

void CEnvQuakeFx::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	Vector vecPos;
	if (!FStringNull(pev->message))
	{
		if (!TryCalcLocus_Position(this, pActivator, STRING(pev->message), vecPos))
		{
			return;
		}
	}
	else
		vecPos = pev->origin;

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(pev->impulse);
	WRITE_VECTOR(vecPos);
	if (pev->impulse == TE_PARTICLEBURST)
	{
		WRITE_SHORT(pev->armortype);	// radius
		WRITE_BYTE(pev->frags);			// particle colour
		WRITE_BYTE(pev->health * 10);	// duration
	}
	else if (pev->impulse == TE_EXPLOSION2)
	{
		// these fields seem to have no effect - except that it
		// crashes when I send "0" for the number of colours..
		WRITE_BYTE(0); // colour
		WRITE_BYTE(1); // number of colours
	}
	else if (pev->impulse == TE_IMPLOSION)
	{
		WRITE_BYTE(pev->armortype);	 // radius
		WRITE_BYTE(pev->armorvalue); // count
		WRITE_BYTE(pev->health * 10); // duration
	}
	MESSAGE_END();

	if (!FBitSet(pev->spawnflags, SF_QUAKEFX_REPEATABLE))
	{
		SetThink(&CEnvQuakeFx::SUB_Remove);
		pev->nextthink = gpGlobals->time;
	}
}

//==================================================================
// LRC- Shockwave effect, like when a Houndeye attacks.
//==================================================================
#define SF_SHOCKWAVE_CENTERED 1
#define SF_SHOCKWAVE_REPEATABLE 2

class CEnvShockwave : public CPointEntity
{
public:
	void Precache() override;
	void Spawn() override { Precache(); }
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	bool KeyValue(KeyValueData* pkvd) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	int m_iTime;
	int m_iRadius;
	int m_iHeight;
	int m_iScrollRate;
	int m_iNoise;
	int m_iFrameRate;
	int m_iStartFrame;
	int m_iSpriteTexture;
	char m_cType;
	string_t m_iszPosition;
};

LINK_ENTITY_TO_CLASS(env_shockwave, CEnvShockwave);

TYPEDESCRIPTION CEnvShockwave::m_SaveData[] =
	{
		DEFINE_FIELD(CEnvShockwave, m_iHeight, FIELD_INTEGER),
		DEFINE_FIELD(CEnvShockwave, m_iTime, FIELD_INTEGER),
		DEFINE_FIELD(CEnvShockwave, m_iRadius, FIELD_INTEGER),
		DEFINE_FIELD(CEnvShockwave, m_iScrollRate, FIELD_INTEGER),
		DEFINE_FIELD(CEnvShockwave, m_iNoise, FIELD_INTEGER),
		DEFINE_FIELD(CEnvShockwave, m_iFrameRate, FIELD_INTEGER),
		DEFINE_FIELD(CEnvShockwave, m_iStartFrame, FIELD_INTEGER),
		DEFINE_FIELD(CEnvShockwave, m_iSpriteTexture, FIELD_INTEGER),
		DEFINE_FIELD(CEnvShockwave, m_cType, FIELD_CHARACTER),
		DEFINE_FIELD(CEnvShockwave, m_iszPosition, FIELD_STRING),
};

IMPLEMENT_SAVERESTORE(CEnvShockwave, CPointEntity);

void CEnvShockwave::Precache()
{
	m_iSpriteTexture = PRECACHE_MODEL(STRING(pev->netname));
}

bool CEnvShockwave::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iTime"))
	{
		m_iTime = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iRadius"))
	{
		m_iRadius = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iHeight"))
	{
		m_iHeight = atoi(pkvd->szValue) / 2; // LRC- the actual height is doubled when drawn
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iScrollRate"))
	{
		m_iScrollRate = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iNoise"))
	{
		m_iNoise = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iFrameRate"))
	{
		m_iFrameRate = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iStartFrame"))
	{
		m_iStartFrame = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszPosition"))
	{
		m_iszPosition = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_cType"))
	{
		m_cType = atoi(pkvd->szValue);
		return true;
	}

	return CPointEntity::KeyValue(pkvd);
}

void CEnvShockwave::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	Vector vecPos;
	if (!FStringNull(m_iszPosition))
	{
		if (!TryCalcLocus_Position(this, pActivator, STRING(m_iszPosition), vecPos))
			return;
	}
	else
	{
		vecPos = pev->origin;
	}

	if (!FBitSet(pev->spawnflags, SF_SHOCKWAVE_CENTERED))
		vecPos.z += m_iHeight;

	int type = m_cType;
	switch (type)
	{
	case TE_BEAMTORUS:
	case TE_BEAMDISK:
	case TE_BEAMCYLINDER:
		break;
	default:
		type = TE_BEAMCYLINDER;
		break;
	}

	// blast circle
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecPos);
	WRITE_BYTE(type);
	WRITE_CIRCLE(vecPos, m_iRadius);
	WRITE_SHORT(m_iSpriteTexture); // short (sprite index)
	WRITE_BYTE(m_iStartFrame);	   // byte (starting frame)
	WRITE_BYTE(m_iFrameRate);	   // byte (frame rate in 0.1's)
	WRITE_BYTE(m_iTime);		   // byte (life in 0.1's)
	WRITE_BYTE(m_iHeight);		   // byte (line width in 0.1's)
	WRITE_BYTE(m_iNoise);		   // byte (noise amplitude in 0.01's)
	WRITE_COLOR(pev->rendercolor); // byte,byte,byte (color)
	WRITE_BYTE(pev->renderamt);	   // byte (brightness)
	WRITE_BYTE(m_iScrollRate);	   // byte (scroll speed in 0.1's)
	MESSAGE_END();

	if (!FBitSet(pev->spawnflags, SF_SHOCKWAVE_REPEATABLE))
	{
		SetThink(&CEnvShockwave::SUB_Remove);
		pev->nextthink = gpGlobals->time;
	}
}

//=========================================================
// LRC - Streaks effect (client-side tracer particles)
//=========================================================
#define SF_ENVSTREAK_REMOVE_ON_FIRE 1
#define SF_ENVSTREAK_DIRECTIONAL 2

class CEnvStreak : public CPointEntity
{
public:
	bool KeyValue(KeyValueData* pkvd) override;
	void Spawn() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
};

LINK_ENTITY_TO_CLASS(env_streak, CEnvStreak);

bool CEnvStreak::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "minlife"))
	{
		pev->health = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "maxlife"))
	{
		pev->max_health = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "tracer_color"))
	{
		pev->skin = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "count"))
	{
		pev->body = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "randomness"))
	{
		pev->dmg = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "pt_type"))
	{
		pev->impulse = atoi(pkvd->szValue);
		return true;
	}

	return CPointEntity::KeyValue(pkvd);
}

void CEnvStreak::Spawn()
{
	CPointEntity::Spawn();
	SetMovedir(pev);
	if (pev->health == 0.0f)
	{
		pev->health = 0.1f;
		if (pev->max_health == 0.0f)
			pev->max_health = 0.5f;
	}
}

void CEnvStreak::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	Vector origin = pev->origin;
	if (!FStringNull(pev->message))
	{
		if (!TryCalcLocus_Position(this, pActivator, STRING(pev->message), origin))
		{
			return;
		}
	}

	bool isDirectional = FBitSet(pev->spawnflags, SF_ENVSTREAK_DIRECTIONAL);
	Vector direction = pev->movedir;

	if (!FStringNull(pev->netname))
	{
		if (TryCalcLocus_Velocity(this, pActivator, STRING(pev->netname), direction))
			isDirectional = true;
		else
			return;
	}

	MESSAGE_BEGIN(MSG_PVS, gmsgStreaks, origin);
	WRITE_BYTE(isDirectional ? 1 : 0);
	WRITE_VECTOR(origin); // origin
	if (isDirectional)
		WRITE_VECTOR(direction); // direction
	WRITE_BYTE(pev->skin);		 // color
	WRITE_SHORT(pev->body);		 // count
	WRITE_SHORT(pev->speed);
	WRITE_SHORT(pev->dmg); // Random velocity modifier
	WRITE_BYTE(pev->health * 10);
	WRITE_BYTE(pev->max_health * 10);
	WRITE_BYTE(pev->impulse > 0 ? pev->impulse : pt_grav);
	WRITE_BYTE(pev->scale > 0 ? pev->scale * 10 : 10);
	MESSAGE_END();

	if (FBitSet(pev->spawnflags, SF_ENVSTREAK_REMOVE_ON_FIRE))
		UTIL_Remove(this);
}

//=========================================================
// LRC - Particle shooter
//=========================================================
class CParticleShooter : public CPointEntity
{
public:
	void Precache() override;
	void Spawn() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	void EXPORT ShootParticle();

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	int m_particleCount;
	int m_particleCountLeft;
	float m_fireDelay;

	float m_particleLife;
	float m_particleSpeed;

	float m_velVariance;

	float m_fadeSpeed;
	float m_scaleSpeed;

	string_t m_iszPosition;
	string_t m_iszDirection;
	EHANDLE m_hActivator;

	int m_iSprite;
};

LINK_ENTITY_TO_CLASS(particle_shooter, CParticleShooter);

TYPEDESCRIPTION CParticleShooter::m_SaveData[] =
	{
		DEFINE_FIELD(CParticleShooter, m_particleCount, FIELD_INTEGER),
		DEFINE_FIELD(CParticleShooter, m_particleCountLeft, FIELD_INTEGER),
		DEFINE_FIELD(CParticleShooter, m_fireDelay, FIELD_FLOAT),
		DEFINE_FIELD(CParticleShooter, m_particleLife, FIELD_FLOAT),
		DEFINE_FIELD(CParticleShooter, m_particleSpeed, FIELD_FLOAT),
		DEFINE_FIELD(CParticleShooter, m_velVariance, FIELD_FLOAT),
		DEFINE_FIELD(CParticleShooter, m_fadeSpeed, FIELD_FLOAT),
		DEFINE_FIELD(CParticleShooter, m_scaleSpeed, FIELD_FLOAT),
		DEFINE_FIELD(CParticleShooter, m_iszPosition, FIELD_STRING),
		DEFINE_FIELD(CParticleShooter, m_iszDirection, FIELD_STRING),
		DEFINE_FIELD(CParticleShooter, m_hActivator, FIELD_EHANDLE),
};

IMPLEMENT_SAVERESTORE(CParticleShooter, CPointEntity);

void CParticleShooter::Spawn()
{
	Precache();
	CPointEntity::Spawn();

	m_particleCountLeft = m_particleCount;

	if (m_fireDelay <= 0.0f)
		m_fireDelay = 0.1f;

	if (FBitSet(pev->spawnflags, SF_PARTICLESHOOTER_STARTON))
	{
		SetThink(&CParticleShooter::ShootParticle);
		pev->nextthink = gpGlobals->time + 0.2f;
	}
}

void CParticleShooter::Precache()
{
	m_iSprite = PRECACHE_MODEL(STRING(pev->model));
}

bool CParticleShooter::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "num_particles"))
	{
		m_particleCount = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "fire_delay"))
	{
		m_fireDelay = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "particle_life"))
	{
		m_particleLife = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "particle_speed"))
	{
		pev->speed = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "vel_variance"))
	{
		m_velVariance = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "particle_size"))
	{
		pev->scale = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "colour_red"))
	{
		pev->rendercolor.x = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "colour_green"))
	{
		pev->rendercolor.y = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "colour_blue"))
	{
		pev->rendercolor.z = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "brightness"))
	{
		pev->renderamt = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "fade_speed"))
	{
		m_fadeSpeed = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "scale_speed"))
	{
		m_scaleSpeed = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "position"))
	{
		m_iszPosition = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "direction"))
	{
		m_iszDirection = ALLOC_STRING(pkvd->szValue);
		return true;
	}

	return CPointEntity::KeyValue(pkvd);
}

void CParticleShooter::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	const bool active = FBitSet(pev->spawnflags, SF_PARTICLESHOOTER_STARTON);
	if (ShouldToggle(useType, active))
	{
		if (active)
		{
			ClearBits(pev->spawnflags, SF_PARTICLESHOOTER_STARTON);
			SetThink(NULL);
		}
		else
		{
			SetBits(pev->spawnflags, SF_PARTICLESHOOTER_STARTON);
			SetThink(&CParticleShooter::ShootParticle);
			pev->nextthink = gpGlobals->time;
		}
		m_hActivator = pActivator;
	}
}

void CParticleShooter::ShootParticle()
{
	if (FStringNull(pev->target) && FStringNull(m_iszDirection))
	{
		ALERT(at_error, "%s have neither a target nor a direction!\n", STRING(pev->classname));
		return;
	}

	pev->nextthink = gpGlobals->time + m_fireDelay;

	Vector position = pev->origin;

	if (!FStringNull(m_iszPosition))
	{
		if (!TryCalcLocus_Position(this, m_hActivator, STRING(m_iszPosition), position))
		{
			ALERT(at_error, "%s can't calc its position from '%s'!\n", STRING(pev->classname), STRING(m_iszPosition));
			return;
		}
	}

	Vector vecShootDir;
	if (!FStringNull(m_iszDirection))
	{
		if (!TryCalcLocus_Velocity(this, m_hActivator, STRING(m_iszDirection), vecShootDir))
		{
			ALERT(at_error, "%s can't calc its direction from '%s'!\n", STRING(pev->classname), STRING(m_iszDirection));
			return;
		}
		vecShootDir = vecShootDir.Normalize();
	}
	else
	{
		Vector vecShootAtPos;
		if (!TryCalcLocus_Position(this, m_hActivator, STRING(pev->target), vecShootAtPos))
		{
			ALERT(at_error, "%s can't calc position to shoot at from '%s'!\n", STRING(pev->classname), STRING(pev->target));
			return;
		}
		vecShootDir = (vecShootAtPos - position).Normalize();
	}

	float teta = acos(vecShootDir.z);
	float fi = (vecShootDir.x == 0 && vecShootDir.y == 0) ? RANDOM_FLOAT(0, M_PI * 2) : atan2(vecShootDir.y, vecShootDir.x);

	teta += RANDOM_FLOAT(-1.0f, 1.0f) * m_velVariance * M_PI / 180.0f;
	fi += RANDOM_FLOAT(-1.0f, 1.0f) * m_velVariance * M_PI / 180.0f;

	vecShootDir.x = cos(fi) * sin(teta);
	vecShootDir.y = sin(fi) * sin(teta);
	vecShootDir.z = cos(teta);

	const Vector velocity = vecShootDir * pev->speed;

	if (gmsgParticleShooter)
	{
		MESSAGE_BEGIN(MSG_PVS, gmsgParticleShooter, position);
		WRITE_VECTOR(position);
		WRITE_VECTOR(velocity);
		WRITE_BYTE(pev->rendermode);
		WRITE_BYTE(m_particleLife * 10);
		WRITE_SHORT(pev->scale * 10);
		WRITE_COLOR(pev->rendercolor);
		WRITE_BYTE(pev->renderamt);
		WRITE_BYTE(pev->gravity * 100);
		WRITE_BYTE(m_fadeSpeed * 10);
		WRITE_BYTE(m_scaleSpeed * 10);
		WRITE_BYTE(pev->framerate);
		WRITE_BYTE(pev->spawnflags);
		WRITE_SHORT(m_iSprite);
		MESSAGE_END();
	}

	if (m_particleCount > 0)
	{
		m_particleCountLeft--;
		if (m_particleCountLeft <= 0)
		{
			ClearBits(pev->spawnflags, SF_PARTICLESHOOTER_STARTON);
			SetThink(NULL);
			if (FBitSet(pev->spawnflags, SF_PARTICLESHOOTER_REPEATABLE))
			{
				m_particleCountLeft = m_particleCount;
			}
			else
			{
				SetThink(&CBaseEntity::SUB_Remove);
				pev->nextthink = gpGlobals->time;
			}
		}
	}
}

//=========================================================
// LRC - env_smoker: extended version of the HL env_smoker
// (position/direction via locus, directed smoke, fade, scale speed, repeatable...)
// This replaces the simple CSmoker that lived in gargantua.cpp
//=========================================================
#define SF_SMOKER_ACTIVE 1
#define SF_SMOKER_REPEATABLE 4
#define SF_SMOKER_DIRECTIONAL 8
#define SF_SMOKER_FADE 16
#define SF_SMOKER_MAXHEALTH_SET (1 << 24)

enum
{
	SMOKER_SPRITE_SCALE_DECILE = 0,
	SMOKER_SPRITE_SCALE_NORMAL = 1
};

class CSmoker : public CBaseEntity
{
public:
	void Precache() override;
	void Spawn() override;
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	void Think() override;

	bool IsActive()
	{
		return FBitSet(pev->spawnflags, SF_SMOKER_ACTIVE);
	}
	void SetActive(bool active)
	{
		if (active)
			SetBits(pev->spawnflags, SF_SMOKER_ACTIVE);
		else
			ClearBits(pev->spawnflags, SF_SMOKER_ACTIVE);
	}

	string_t m_iszPosition;
	string_t m_iszDirection;
	EHANDLE m_hActivator;

	int smokeIndex;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
};

LINK_ENTITY_TO_CLASS(env_smoker, CSmoker);

TYPEDESCRIPTION CSmoker::m_SaveData[] =
	{
		DEFINE_FIELD(CSmoker, m_iszPosition, FIELD_STRING),
		DEFINE_FIELD(CSmoker, m_iszDirection, FIELD_STRING),
		DEFINE_FIELD(CSmoker, m_hActivator, FIELD_EHANDLE),
};

IMPLEMENT_SAVERESTORE(CSmoker, CBaseEntity);

void CSmoker::Precache()
{
	smokeIndex = 0;
	if (!FStringNull(pev->model))
		smokeIndex = PRECACHE_MODEL(STRING(pev->model));
}

void CSmoker::Spawn()
{
	Precache();

	pev->movetype = MOVETYPE_NONE;
	pev->nextthink = gpGlobals->time;
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->effects |= EF_NODRAW;
	SetMovedir(pev);

	if (pev->scale <= 0.0f)
		pev->scale = 10;
	if (pev->framerate <= 0.0f)
		pev->framerate = 11.0f;

	// delay between puffs
	if (pev->dmg_take <= 0.0f)
		pev->dmg_take = 0.1f;
	if (pev->dmg_save <= 0.0f)
		pev->dmg_save = 0.2f;

	pev->max_health = pev->health;

	if (FStringNull(pev->targetname))
		SetActive(true);

	if (IsActive())
		pev->nextthink = gpGlobals->time + 0.1f;
}

bool CSmoker::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "scale_speed"))
	{
		pev->armorvalue = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "scale_unit_type"))
	{
		pev->impulse = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "position"))
	{
		m_iszPosition = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "direction"))
	{
		m_iszDirection = ALLOC_STRING(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

void CSmoker::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	const bool active = IsActive();
	if (ShouldToggle(useType, active))
	{
		if (active)
		{
			pev->nextthink = -1;
			SetActive(false);
		}
		else
		{
			pev->nextthink = gpGlobals->time;
			SetActive(true);
		}
		m_hActivator = pActivator;
	}
}

void CSmoker::Think()
{
	// Support for CBaseEntity::Create followed by pev->health setting
	if (!FBitSet(pev->spawnflags, SF_SMOKER_MAXHEALTH_SET))
	{
		pev->max_health = pev->health;
		SetBits(pev->spawnflags, SF_SMOKER_MAXHEALTH_SET);
	}

	if (!IsActive())
		return;

	const int minFramerate = V_max(pev->framerate - 1, 1);
	const int maxFramerate = pev->framerate + 1;

	bool isPosValid = true;
	Vector position = pev->origin;

	if (!FStringNull(m_iszPosition))
	{
		isPosValid = TryCalcLocus_Position(this, m_hActivator, STRING(m_iszPosition), position);
	}

	bool isDirValid = true;
	bool directed = FBitSet(pev->spawnflags, SF_SMOKER_DIRECTIONAL);
	Vector direction = g_vecZero;

	if (!FStringNull(m_iszDirection))
	{
		directed = true;
		isDirValid = TryCalcLocus_Velocity(this, m_hActivator, STRING(m_iszDirection), direction);
		if (isDirValid)
			direction = direction.Normalize();
	}
	else if (!FStringNull(pev->target))
	{
		directed = true;
		Vector targetPos;
		isDirValid = TryCalcLocus_Position(this, m_hActivator, STRING(pev->target), targetPos);
		if (isDirValid)
			direction = (targetPos - position).Normalize();
	}
	else if (directed)
	{
		direction = pev->movedir;
	}

	if (isDirValid && isPosValid)
	{
		int flags = 0;
		if (directed)
			flags |= SMOKER_FLAG_DIRECTED;
		if (FBitSet(pev->spawnflags, SF_SMOKER_FADE))
			flags |= SMOKER_FLAG_FADE_SPRITE;
		if (pev->impulse == SMOKER_SPRITE_SCALE_NORMAL)
			flags |= SMOKER_FLAG_SCALE_VALUE_IS_NORMAL;

		MESSAGE_BEGIN(MSG_PVS, gmsgSmoke, position);
		WRITE_BYTE(flags);
		WRITE_COORD(position.x + RANDOM_FLOAT(-pev->dmg, pev->dmg));
		WRITE_COORD(position.y + RANDOM_FLOAT(-pev->dmg, pev->dmg));
		WRITE_COORD(position.z);
		WRITE_SHORT(smokeIndex ? smokeIndex : g_sModelIndexSmoke);
		WRITE_COORD(RANDOM_FLOAT(pev->scale, pev->scale * 1.1f));
		WRITE_BYTE(RANDOM_LONG(minFramerate, maxFramerate)); // framerate
		WRITE_SHORT(pev->speed);
		WRITE_SHORT(pev->frags);
		WRITE_BYTE(pev->rendermode);
		WRITE_BYTE(pev->renderamt);
		WRITE_COLOR(pev->rendercolor);
		WRITE_SHORT(pev->armorvalue * 10);
		if (directed)
		{
			WRITE_VECTOR(direction);
		}
		MESSAGE_END();
	}

	if (pev->max_health > 0)
		pev->health--;
	if (pev->max_health <= 0 || pev->health > 0)
	{
		const float minDelay = pev->dmg_take;
		const float maxDelay = V_max(pev->dmg_take, pev->dmg_save);

		pev->nextthink = gpGlobals->time + RANDOM_FLOAT(minDelay, maxDelay);
	}
	else
	{
		if (FBitSet(pev->spawnflags, SF_SMOKER_REPEATABLE))
		{
			pev->health = pev->max_health;
			SetActive(false);
		}
		else
			UTIL_Remove(this);
	}
}
