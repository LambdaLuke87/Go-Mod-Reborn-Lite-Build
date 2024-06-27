/**********************************************************************
 *														              *
 *		The Code was originally created by the Go-Mod 3 Team		  *
 *		     2009-2010 Ranger366 & FITAMOD (HLMODER)		          *
 *                                                                    *
 *           Updated and improved for Go-Mod: Reborn                  *
 *                  2023-2024 LambdaLuke87                            *
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

LINK_ENTITY_TO_CLASS(weapon_toolgun, CToolgun);

void CToolgun::Spawn()
{
	Precache( );
	SET_MODEL(ENT(pev), "models/w_removetool.mdl");
	m_iId = WEAPON_TOOLGUN;

	m_iDefaultAmmo = -1;

	FallInit();// get ready to fall down.
}

void CToolgun::Precache()
{
	PRECACHE_MODEL("models/v_removetool.mdl");
	PRECACHE_MODEL("models/w_removetool.mdl");
	PRECACHE_MODEL("models/p_removetool.mdl");         

	PRECACHE_SOUND ("weapons/tg_shot1.wav");
	PRECACHE_SOUND ("weapons/tg_shot2.wav");

	m_usToolGun = PRECACHE_EVENT(1, "events/toolgun.sc");

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

		// optimized multiplayer. Widened to make it easier to hit a moving player
	if (m_pPlayer->m_iToolMode == 3)
	{
#ifndef CLIENT_DLL
		CGib::SpawnRandomGibs(pev, 1, 1);
#endif
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
