/**********************************************************************
 *														              *
 *		The Code was originally created by the Go-Mod 3 Team		  *
 *		     2009-2010 Ranger366 & FITAMOD (HLMODER)		          *
 *                                                                    *
 *           Updated and improved for Go-Mod: Reborn                  *
 *                  2023-2025 LambdaLuke87                            *
 *														              *
 ********************************************************************/


#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"
#ifndef CLIENT_DLL
#include "game.h"
#endif

//---------------------------------------------------------------------
// Teleporter Code By Shadow Consumes from planethalflife.com (deleted)
//---------------------------------------------------------------------

LINK_ENTITY_TO_CLASS(teleporter, CTeleporter);

void CTeleporter ::Spawn()
{
	pev->movetype = MOVETYPE_BOUNCE;
	pev->classname = MAKE_STRING("teleporter");

	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/grenade.mdl");

	m_vecDir = gpGlobals->v_forward;
}

CTeleporter* CTeleporter::ShootTeleporter(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity)
{
	CTeleporter* pTeleport = GetClassPtr((CTeleporter*)NULL);
	pTeleport->Spawn();
	UTIL_SetOrigin(pTeleport->pev, vecStart);
	pTeleport->pev->owner = ENT(pevOwner);
	pTeleport->pevOwner = pevOwner;

	pTeleport->SetTouch(&CTeleporter::TeleportSlide); // Bounce if touched
	pTeleport->SetThink(&CTeleporter::TeleportThink);
	pTeleport->pev->nextthink = gpGlobals->time + 0.1;

	pTeleport->pev->sequence = 0;
	pTeleport->pev->framerate = 0;

	pTeleport->pev->gravity = 0.5;
	pTeleport->pev->friction = 0.7;

	pTeleport->pev->velocity = vecVelocity;

	SET_MODEL(ENT(pTeleport->pev), "models/grenade.mdl");
	UTIL_SetSize(pTeleport->pev, Vector(0, 0, 0), Vector(0, 0, 0));

	return pTeleport;
}


void CTeleporter ::TeleportThink(void)
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	pev->movetype = MOVETYPE_BOUNCE;
}

void CTeleporter::TeleportSlide(CBaseEntity* pOther)
{
	entvars_t* pevOther = pOther->pev;

	// don''''t hit the guy that launched this grenade
	if (pOther->edict() == pev->owner)
		return;

	// pev->avelocity = Vector (300, 300, 300);
	pev->gravity = 1; // normal gravity now

	// HACKHACK - On ground isn''''t always set, so look for ground underneath
	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin - Vector(0, 0, 10), ignore_monsters, edict(), &tr);

	if (tr.flFraction < 1.0)
	{
		// add a bit of static friction
		pev->velocity = pev->velocity * 0.95;
		pev->avelocity = pev->avelocity * 0.9;
		// play sliding sound, volume based on velocity
	}
	if (!(pev->flags & FL_ONGROUND) && pev->velocity.Length2D() > 10)
	{
		BounceSound();
	}
	StudioFrameAdvance();
}

void CTeleporter ::Precache()
{
	PRECACHE_MODEL("models/grenade.mdl");
	PRECACHE_SOUND("weapons/bounce.wav");
}

void CTeleporter ::BounceSound()
{
	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/bounce.wav", 1, ATTN_NORM);
}

#define GAUSS_PRIMARY_FIRE_VOLUME 450 // how loud gauss is when discharged

LINK_ENTITY_TO_CLASS(weapon_toolgun, CToolgun);

void CToolgun::Spawn()
{
	Precache( );
	SET_MODEL(ENT(pev), MyWModel());
	m_iId = WEAPON_TOOLGUN;

	m_iDefaultAmmo = -1;

	FallInit();// get ready to fall down.
}

void CToolgun::Precache()
{
	PRECACHE_MODEL("models/v_removetool.mdl");
	PRECACHE_MODEL(MyWModel());
	PRECACHE_MODEL("models/p_removetool.mdl");         

	PRECACHE_SOUND ("weapons/tg_shot1.wav");
	PRECACHE_SOUND ("weapons/tg_shot2.wav");

	m_iTeleport = PRECACHE_MODEL("sprites/blast.spr");
	m_usToolGun = PRECACHE_EVENT(1, "events/toolgun.sc");

	UTIL_PrecacheOther("teleporter");
}

bool CToolgun::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 1;
	p->iPosition = 3;
	p->iId = m_iId = WEAPON_TOOLGUN;
	p->iWeight = GLOCK_WEIGHT;

	return true;
}

bool CToolgun::Deploy()
{
	return DefaultDeploy( "models/v_removetool.mdl", "models/p_removetool.mdl", REMOVETOOL_DRAW, "mp5" );
}

void CToolgun::PrimaryAttack()
{
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	Vector vecDir;

	// some specific tools are defined here
	if (m_pPlayer->m_iToolMode == 3)
	{
#ifndef CLIENT_DLL
		CGib::SpawnRandomGibs(pev, 1, true);
#endif
	}
	else if (m_pPlayer->m_iToolMode == 5)
	{
#ifndef CLIENT_DLL
		if (allow_camera.value)
		{
			UTIL_MakeVectors(Vector(pev->v_angle.x * -1, pev->v_angle.y, pev->v_angle.z));
			CBaseEntity::CreateCamera(pev->origin, pev->angles, m_pPlayer);
		}
		else
			ClientPrint(m_pPlayer->pev, HUD_PRINTCENTER, "#Gomod_Camera_NotAllowed");
#endif
	}
	else if (m_pPlayer->m_iToolMode == 12)
	{
		if (not_out == 0)
		{
			ThrowTP();
		}
		else
		{
			TraceResult tr, tr2, tr3, tr4;

			edict_t* pPlayer = m_pPlayer->edict();

			CBaseEntity* pTeleport = NULL;

			Vector vector_Src = m_pPlayer->pev->origin;

			while ((pTeleport = UTIL_FindEntityInSphere(pTeleport, m_pPlayer->pev->origin, 4096)) != NULL)
			{
				if (FClassnameIs(pTeleport->pev, "teleporter"))
				{
					v_Src = pTeleport->pev->origin;

					UTIL_TraceLine(v_Src, pTeleport->pev->origin - Vector(24, 0, 0), ignore_monsters, edict(), &tr);
					UTIL_TraceLine(v_Src, pTeleport->pev->origin + Vector(24, 0, 0), ignore_monsters, edict(), &tr2);
					UTIL_TraceLine(v_Src, pTeleport->pev->origin + Vector(0, 24, 0), ignore_monsters, edict(), &tr3);
					UTIL_TraceLine(v_Src, pTeleport->pev->origin - Vector(0, 24, 0), ignore_monsters, edict(), &tr4);

					if (pTeleport->pev->owner == pPlayer)
					{
						if ((tr.flFraction < 1.0) || (tr2.flFraction < 1.0) || (tr3.flFraction < 1.0) || (tr4.flFraction < 1.0))
						{
							EMIT_SOUND(ENT(pTeleport->pev), CHAN_WEAPON, "buttons/button11.wav", 0.5, ATTN_NORM);
							EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "buttons/button11.wav", 0.5, ATTN_NORM);
							UTIL_Remove(pTeleport);
							not_out = 0;
						}
						else
						{
							MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vector_Src);
							WRITE_BYTE(TE_SPRITE);
							WRITE_COORD(vector_Src.x);
							// pos
							WRITE_COORD(vector_Src.y);
							WRITE_COORD(vector_Src.z);
							WRITE_SHORT(m_iTeleport); // model
							WRITE_BYTE(23);			  // size * 10
							WRITE_BYTE(128);		  // brightness
							MESSAGE_END();

							EMIT_SOUND(ENT(pTeleport->pev), CHAN_WEAPON, "weapons/displacer_teleport.wav", 1.0, ATTN_NORM);

							m_pPlayer->pev->origin = pTeleport->pev->origin;
							m_pPlayer->pev->origin.z += 54;
							UTIL_Remove(pTeleport);
							not_out = 0;
						}
					}
				}
			}
		}
	}
	else
		vecDir = m_pPlayer->FireBulletsToolGun(1, vecSrc, vecAiming, VECTOR_CONE_1DEGREES, 8192, BULLET_PLAYER_MP5, 2, 0, m_pPlayer->pev, m_pPlayer->random_seed);


  int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif
	
	PLAYBACK_EVENT_FULL(flags, m_pPlayer->edict(), m_usToolGun, 0.0, (float*)&g_vecZero, (float*)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0);

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.3;

	if ( m_flNextPrimaryAttack < UTIL_WeaponTimeBase() )
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.3;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CToolgun::SecondaryAttack()
{
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	Vector vecDir;

	// some specific tools are defined here
	if (m_pPlayer->m_iToolMode == 3)
	{
#ifndef CLIENT_DLL
		CGib::SpawnRandomGibs(pev, 1, false);
#endif
	}
	else
		vecDir = m_pPlayer->FireBulletsToolGunAlt(1, vecSrc, vecAiming, VECTOR_CONE_1DEGREES, 8192, BULLET_PLAYER_MP5, 2, 0, m_pPlayer->pev, m_pPlayer->random_seed);

  int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif
	
	PLAYBACK_EVENT_FULL(flags, m_pPlayer->edict(), m_usToolGun, 0.0, (float*)&g_vecZero, (float*)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0);

	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.3;

	if ( m_flNextSecondaryAttack < UTIL_WeaponTimeBase() )
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.1;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CToolgun::WeaponIdle( void )
{
	ResetEmptySound( );
	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	int iAnim;
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:	
		iAnim = REMOVETOOL_IDLE;	
		break;
	
	default:
	case 1:
		iAnim = REMOVETOOL_IDLE2;
		break;
	}

	SendWeaponAnim( iAnim );
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 ); // how long till we do this again.
}

void CToolgun::ThrowTP()
{
	Vector vecSrc = m_pPlayer->GetGunPosition() + gpGlobals->v_up * -2 + gpGlobals->v_right * 2;
	Vector vecThrow = gpGlobals->v_forward * 650 + m_pPlayer->pev->velocity;

	CBaseEntity* pEnt = CTeleporter::ShootTeleporter(m_pPlayer->pev, vecSrc, vecThrow);
	CTeleporter* pTeleport = (CTeleporter*)pEnt;

	//EMIT_SOUND(ENT(pTeleport->pev), CHAN_WEAPON, "x/x_shoot1.wav", 0.5, ATTN_NORM);

	not_out = 1;
}