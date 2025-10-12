/**********************************************************************
 *                 Go-Mod: Reborn Lite Build                          *
 *                   LambdaLuke87 - 2025                              *
 ********************************************************************/


#include	"extdll.h"
#include	"plane.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"animation.h"
#include	"squadmonster.h"
#include	"weapons.h"
#include	"talkmonster.h"
#include	"soundent.h"
#include	"effects.h"
#include	"customentity.h"
#include    "game.h"

class CChikenAnimal : public CBaseMonster
{
public:
    void Spawn() override;
    void Precache() override;
    int  Classify () override;      
    void Killed(entvars_t* pevAttacker, int iGib) override;
    void GibMonster() override;
    int m_Activity;                                                                 //What entity is doing (animation)//
    int m_iChikenGibs;
};

LINK_ENTITY_TO_CLASS(prop_chiken, CChikenAnimal);

void CChikenAnimal::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/gomod/chiken_animal.mdl");
	UTIL_SetSize(pev, Vector(-12, -12, 0), Vector(12, 12, 24));
	
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->takedamage = DAMAGE_YES;
	pev->health = 1;
	m_MonsterState = MONSTERSTATE_NONE;

	MonsterInit();
}

void CChikenAnimal::Precache()
{
	PRECACHE_MODEL("models/gomod/chiken_animal.mdl");
	m_iChikenGibs = PRECACHE_MODEL("models/fleshgibs.mdl");
}

const GibLimit ChikenGibLimits[] =
{
    {1},
    {1},
    {1},
    {1},
};

const GibData ChikenGibs = { "models/fleshgibs.mdl", 0, 9, ChikenGibLimits };

int CChikenAnimal::Classify()
{
    return CLASS_HUMAN_PASSIVE;
}

void CChikenAnimal::Killed(entvars_t* pevAttacker, int iGib)
{
    GibMonster();
    EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "!PL_VOICEFRST21", 1.0, ATTN_NORM, 0, PITCH_NORM);
}

void CChikenAnimal::GibMonster()
{
	// don't remove players!
	SetThink(&CBaseMonster::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.15;

	// Note: the original didn't have the violence check
	if (CVAR_GET_FLOAT("violence_agibs") != 0) // Should never get here, but someone might call it directly
	{
		// Gib spawning has been rewritten so the logic for limiting gib submodels is generalized
		CGib::SpawnRandomGibs(pev, 12, ChikenGibs); // Throw alien gibs
	}
}

class CChumtoad : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override;
};

LINK_ENTITY_TO_CLASS(prop_chumtoad, CChumtoad);

void CChumtoad::Spawn()
{
    Precache();

	SET_MODEL(ENT(pev), "models/gomod/chumtoad.mdl");
	UTIL_SetSize(pev, Vector(-4, -4, 0), Vector(4, 4, 8));

    pev->movetype = MOVETYPE_STEP;
	pev->solid = SOLID_SLIDEBOX;
    pev->takedamage = DAMAGE_NO;
    pev->health = 10;
    pev->body = RANDOM_LONG(0, 5);
    m_MonsterState = MONSTERSTATE_NONE;

	MonsterInit();
}

void CChumtoad::Precache()
{
    PRECACHE_MODEL("models/gomod/chumtoad.mdl");
}

int CChumtoad::Classify()
{
    return CLASS_NONE;
}

class CC4Prop : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override;
	void Killed(entvars_t* pevAttacker, int iGib) override;

	int m_iSpriteTexture;
};

LINK_ENTITY_TO_CLASS(prop_c4, CC4Prop);

void CC4Prop::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/gomod/c4_bomb.mdl");
	UTIL_SetSize(pev, Vector(-4, -4, 0), Vector(4, 4, 8));

	pev->movetype = MOVETYPE_STEP;
	pev->solid = SOLID_SLIDEBOX;
	pev->takedamage = DAMAGE_YES;
	m_bloodColor = DONT_BLEED;
	pev->health = 1;
	m_MonsterState = MONSTERSTATE_NONE;

	MonsterInit();
}

void CC4Prop::Precache()
{
	PRECACHE_MODEL("models/gomod/c4_bomb.mdl");
	PRECACHE_SOUND("weapons/mortarhit.wav");
	m_iSpriteTexture = PRECACHE_MODEL("sprites/white.spr");
}

void CC4Prop::Killed(entvars_t* pevAttacker, int iGib)
{
	if (explosion_control.value)
		CBaseMonster::Killed(pevAttacker, GIB_ALWAYS);
	else
	{
		Detonate();

		// blast circle
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_BEAMCYLINDER);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z + 2000); // reach damage radius over .2 seconds
		WRITE_SHORT(m_iSpriteTexture);
		WRITE_BYTE(0);	 // startframe
		WRITE_BYTE(0);	 // framerate
		WRITE_BYTE(4);	 // life
		WRITE_BYTE(32);	 // width
		WRITE_BYTE(0);	 // noise
		WRITE_BYTE(255); // r, g, b
		WRITE_BYTE(255); // r, g, b
		WRITE_BYTE(192); // r, g, b
		WRITE_BYTE(128); // brightness
		WRITE_BYTE(0);	 // speed
		MESSAGE_END();

		EMIT_SOUND(ENT(pev), CHAN_STATIC, "weapons/mortarhit.wav", 1.0, 0.3);

		UTIL_ScreenShake(pev->origin, 4.0, 3.0, 1.0, 750);

		::RadiusDamage(Center(), pev, pev, 950, 950, dont_ignore_monsters, DMG_BLAST);
	}
}

int  CC4Prop::Classify()
{
	return CLASS_NONE;
}

class CWaspCamera : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override;
	int m_Activity;
};

LINK_ENTITY_TO_CLASS(prop_waspcamera, CWaspCamera);

void CWaspCamera::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/gomod/wasp.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 36)); // Houndeye size

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_FLY;
	pev->flags |= FL_FLY;
	m_bloodColor = DONT_BLEED;
	pev->health = 250;
	m_MonsterState = MONSTERSTATE_NONE;

	MonsterInit();
}

void CWaspCamera::Precache()
{
	PRECACHE_MODEL("models/gomod/wasp.mdl");
}

int CWaspCamera::Classify()
{
	return CLASS_NONE;
}
