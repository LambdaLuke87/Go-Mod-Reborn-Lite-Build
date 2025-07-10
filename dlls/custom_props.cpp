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
#include    "weapons.h"
#include    "game.h"

class CChiken_Animal : public CBaseMonster
{
public:
        void Spawn() override;
        void Precache() override;
        int  Classify () override;
        const char* DefaultDisplayName() { return "Chiken"; }
        void SetActivity(int activity);          
        void Killed(entvars_t* pevAttacker, int iGib) override;
        void GibMonster() override;
        int  GetActivity() { return m_Activity; }
        void EXPORT IdleThink();
        int m_Activity;                                                                 //What entity is doing (animation)//
        int m_iChikenGibs;
};
LINK_ENTITY_TO_CLASS(prop_chiken, CChiken_Animal);

void CChiken_Animal::Spawn()
{
        Precache();
        
        pev->solid = SOLID_SLIDEBOX;
		pev->movetype = MOVETYPE_STEP;
		m_bloodColor = BLOOD_COLOR_RED;
        pev->takedamage = DAMAGE_YES;
        //pev->flags              |= FL_MONSTER;
        pev->health             = 1;
        pev->gravity    = 1.0;
        
        SET_MODEL(ENT(pev), "models/gomod/chiken_animal.mdl");

        SetActivity(ACT_IDLE);
        SetSequenceBox();
		SetThink(&CChiken_Animal::IdleThink);
        pev->nextthink = gpGlobals->time + 0.1;           

}

void CChiken_Animal::Precache()
{
        PRECACHE_MODEL("models/gomod/chiken_animal.mdl");
        PRECACHE_SOUND("misc/killChicken.wav");
        m_iChikenGibs = PRECACHE_MODEL("models/fleshgibs.mdl");
}

const GibLimit ChikenGibLimits[] =
{
    {2},
    {1},
    {3},
    {2},
};

const GibData ChikenGibs = { "models/fleshgibs.mdl", 0, 9, ChikenGibLimits };

int CChiken_Animal::Classify()
{
        return CLASS_HUMAN_PASSIVE;
}

void CChiken_Animal::SetActivity(int act)
{       
        int sequence = LookupActivity( act ); 
        if ( sequence != ACTIVITY_NOT_AVAILABLE )
        {
                pev->sequence = sequence;
                m_Activity = act; 
                pev->frame = 0;
                ResetSequenceInfo( );
                //m_flFrameRate = 1.0;
        }

}

void CChiken_Animal::IdleThink()
{
        float flInterval = StudioFrameAdvance();

        pev->nextthink = gpGlobals->time + 1;
        DispatchAnimEvents(flInterval);

        if(!IsInWorld())
        {
                SetTouch(NULL);
                UTIL_Remove(this);
                return;
        }

}

void CChiken_Animal::Killed(entvars_t* pevAttacker, int iGib)
{
		GibMonster();
        EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "misc/killChicken.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
}

void CChiken_Animal::GibMonster()
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

class CChumcolors : public CBaseMonster
{
public:
        void Spawn( void );
        void Precache( void );
        int  Classify ( void );
        const char* DefaultDisplayName() { return "Colorful Chumtoad"; }
        void SetActivity(int activity);                                 
        int  GetActivity() { return m_Activity; }
        void EXPORT IdleThink();
        int m_Activity;                                  //What entity is doing (animation)//
		//static const char* pIdleSounds[];

};

/*
const char* CChumcolors::pIdleSounds[] =
	{
		"chumtoad/cht_croak_short.wav",
		"chumtoad/cht_croak_medium.wav",
		"chumtoad/cht_croak_long.wav",
		"chumtoad/chub_draw.wav"};*/

LINK_ENTITY_TO_CLASS(prop_colored_chummy, CChumcolors);

void CChumcolors::Spawn(void)
{
        Precache();
        
        pev->movetype   = MOVETYPE_STEP;
        pev->solid              = SOLID_BBOX;
        pev->takedamage = DAMAGE_NO;
        //pev->flags              |= FL_MONSTER;
        pev->health             = 10;
        pev->gravity    = 1.0;
		pev->body = RANDOM_LONG(0, 5);
        
        SET_MODEL(ENT(pev), "models/gomod/chumtoad_variants.mdl");

        SetActivity(ACT_IDLE);
        SetSequenceBox();
		SetThink(&CChumcolors::IdleThink);
        pev->nextthink = gpGlobals->time + 0.1;           

}

void CChumcolors::Precache()
{
        PRECACHE_MODEL("models/gomod/chumtoad_variants.mdl");
}

int CChumcolors::Classify(void)
{
        return  CLASS_NONE;
}

void CChumcolors::SetActivity(int act)
{       
        int sequence = LookupActivity( act ); 
        if ( sequence != ACTIVITY_NOT_AVAILABLE )
        {
                pev->sequence = sequence;
                m_Activity = act; 
                pev->frame = 0;
                ResetSequenceInfo( );
                //m_flFrameRate = 1.0;
        }
}

void CChumcolors::IdleThink()
{
        float flInterval = StudioFrameAdvance();

        pev->nextthink = gpGlobals->time + 1;
        DispatchAnimEvents(flInterval);

        if(!IsInWorld())
        {
                SetTouch(NULL);
                UTIL_Remove(this);
                return;
        }
}

class CC4Prop : public CGrenade
{
public:
        void Spawn( void );
        void Precache( void );
        int  Classify ( void );
        const char* DefaultDisplayName() { return "C4 Bomb"; }
        void SetActivity(int activity);                                 
        int  GetActivity() { return m_Activity; }
        void EXPORT IdleThink();
        void Killed(entvars_t* pevAttacker, int iGib) override;
        int BloodColor( void ) { return DONT_BLEED; }
        int m_Activity;                                                                 //What entity is doing (animation)//
        
        int m_iSpriteTexture;
};
LINK_ENTITY_TO_CLASS(prop_c4, CC4Prop);

void CC4Prop::Spawn(void)
{
        Precache();
        
        pev->movetype   = MOVETYPE_STEP;
        pev->solid              = SOLID_BBOX;
        pev->takedamage = DAMAGE_YES;
      //  pev->flags              |= FL_MONSTER;
        pev->health             = 1;
        pev->gravity    = 1.0;
        
        SET_MODEL(ENT(pev), "models/gomod/c4_bomb.mdl");

        SetActivity(ACT_IDLE);
        SetSequenceBox();
		SetThink(&CC4Prop::IdleThink);
        pev->nextthink = gpGlobals->time + 0.1;           

}

void CC4Prop::Precache(void)
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

int  CC4Prop::Classify(void)
{
        return  CLASS_NONE;
}

void CC4Prop::SetActivity(int act)
{       
        int sequence = LookupActivity( act ); 
        if ( sequence != ACTIVITY_NOT_AVAILABLE )
        {
                pev->sequence = sequence;
                m_Activity = act; 
                pev->frame = 0;
                ResetSequenceInfo( );
                //m_flFrameRate = 1.0;
        }

}

void CC4Prop::IdleThink()
{
        float flInterval = StudioFrameAdvance();

        pev->nextthink = gpGlobals->time + 1;
        DispatchAnimEvents(flInterval);

        if(!IsInWorld())
        {
                SetTouch(NULL);
                UTIL_Remove(this);
                return;
        }

}
