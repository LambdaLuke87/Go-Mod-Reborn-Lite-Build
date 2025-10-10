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

//=====================================================================
// Teleporter Code By Shadow Consumes from planethalflife.com (deleted)
//=====================================================================

LINK_ENTITY_TO_CLASS(item_teleporter, CTeleporter);

void CTeleporter ::Spawn()
{
	pev->movetype = MOVETYPE_BOUNCE;
	pev->classname = MAKE_STRING("item_teleporter");

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


void CTeleporter ::TeleportThink()
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
}

void CTeleporter ::BounceSound()
{
	switch (RANDOM_LONG(0, 2))
	{
	case 0: EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/g_bounce1.wav", 1, ATTN_NORM); break;
	case 1: EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/g_bounce2.wav", 1, ATTN_NORM); break;
	case 2: EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/g_bounce3.wav", 1, ATTN_NORM); break;
	}
}

LINK_ENTITY_TO_CLASS(weapon_toolbow, CToolbow);

#ifndef CLIENT_DLL

TYPEDESCRIPTION CToolbow::m_SaveData[] =
	{
		DEFINE_FIELD(CToolbow, m_bSpotVisible, FIELD_BOOLEAN),
		DEFINE_FIELD(CToolbow, m_bLaserActive, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CToolbow, CToolbow::BaseClass);

//=========================================================
// ToolBow Laser - an bit more small than Deagle Laser
//=========================================================

LINK_ENTITY_TO_CLASS(toolbow_laser, CToolbowLaser);

CToolbowLaser* CToolbowLaser::CreateSpot()
{
	auto pSpot = GetClassPtr(reinterpret_cast<CToolbowLaser*>(VARS(CREATE_NAMED_ENTITY(MAKE_STRING("toolbow_laser")))));
	pSpot->Spawn();

	// FUN FACT: ToolBow Laser Family: DEagle Laser - Father, RPG Laser - Grandpa
	pSpot->pev->scale = 0.4;

	pSpot->pev->classname = MAKE_STRING("toolbow_laser");

	return pSpot;
}

//=====================================================================
// Bacontsu: Glowstick (originally it was a flare) - world item
//=====================================================================
class CGlowstick : public CBaseEntity
{
	void Spawn();
	void Precache();
	void BounceSound();

	void EXPORT GlowstickSlide(CBaseEntity* pOther);
	void EXPORT GlowstickThink();
	void EXPORT GlowstickUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	int ObjectCaps() override { return CBaseEntity::ObjectCaps() | FCAP_IMPULSE_USE; }

public:
	void Deactivate();

	float lerp[2];
};

LINK_ENTITY_TO_CLASS(item_glowstick, CGlowstick);

//=========================================================
// Deactivate - do whatever it is we do to an orphaned
// satchel when we don't want it in the world anymore.
//=========================================================
void CGlowstick::Deactivate()
{
	pev->solid = SOLID_NOT;
	UTIL_Remove(this);
}

void CGlowstick::Spawn()
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/gomod/glowstick.mdl");

	int colormatic = glowstick_colors.value;

	switch (colormatic)
	{
	default: pev->skin = RANDOM_LONG(0, 6); break;
	case 1: pev->skin = 0; break;
	case 2: pev->skin = 1; break;
	case 3: pev->skin = 2; break;
	case 4: pev->skin = 3; break;
	case 5: pev->skin = 4; break;
	case 6: pev->skin = 5; break;
	case 7: pev->skin = 6; break;
	}

	// UTIL_SetSize(pev, Vector( -16, -16, -4), Vector(16, 16, 32));	// Old box -- size of headcrab monsters/players get blocked by this
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4)); // Uses point-sized, and can be stepped over
	UTIL_SetOrigin(pev, pev->origin);

	SetTouch(&CGlowstick::GlowstickSlide);
	SetThink(&CGlowstick::GlowstickThink);
	SetUse(&CGlowstick::GlowstickUse);
	pev->nextthink = gpGlobals->time + 0.001;

	pev->gravity = 0.5;
	pev->friction = 0.8;

	// ResetSequenceInfo( );
	pev->sequence = 1;
}

void CGlowstick::GlowstickSlide(CBaseEntity* pOther)
{
	entvars_t* pevOther = pOther->pev;

	// don't hit the guy that launched this glowstick
	if (pOther->edict() == pev->owner)
		return;

	// pev->avelocity = Vector (300, 300, 300);
	pev->gravity = 1; // normal gravity now

	// HACKHACK - On ground isn't always set, so look for ground underneath
	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin - Vector(0, 0, 10), ignore_monsters, edict(), &tr);

	if (tr.flFraction < 1.0)
	{
		// add a bit of static friction
		pev->velocity = pev->velocity * 0.95;
		// pev->avelocity = pev->avelocity * 0.9;
		//  play sliding sound, volume based on velocity
	}
	if (!(pev->flags & FL_ONGROUND) && pev->velocity.Length2D() > 10)
	{
		BounceSound();
	}
}

void CGlowstick::GlowstickThink()
{
	pev->nextthink = gpGlobals->time + 0.001;

	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	if (pev->waterlevel == 3)
	{
		pev->movetype = MOVETYPE_FLY;
		pev->velocity = pev->velocity * 0.8;
		// pev->avelocity = pev->avelocity * 0.9;
		pev->velocity.z += 8;
	}
	else if (pev->waterlevel == 0)
	{
		pev->movetype = MOVETYPE_BOUNCE;
	}
	else
	{
		pev->velocity.z -= 8;
	}

	if ((!(pev->flags & FL_ONGROUND)))
	{
		pev->avelocity = pev->velocity;
		lerp[0] = pev->angles.x;
		lerp[1] = pev->angles.y;
	}

	if (pev->flags & FL_ONGROUND)
	{
		// ALERT(at_console, "angles %f %f \n", pev->angles[0], pev->angles[2]);
		lerp[0] = (0 * 0.03f) + (lerp[0] * (1.0 - 0.1f));
		pev->angles.x = lerp[0];

		lerp[1] = (0 * 0.03f) + (lerp[1] * (1.0 - 0.1f));
		pev->angles.z = lerp[1];
	}
}

void CGlowstick::GlowstickUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	UTIL_Remove(this);
}

void CGlowstick::Precache()
{
	PRECACHE_MODEL("models/gomod/glowstick.mdl");
}

void CGlowstick::BounceSound()
{
	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/grenade_hit1.wav", 0.25, ATTN_NORM, 0, PITCH_HIGH);
		break;
	case 1:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/grenade_hit2.wav", 0.25, ATTN_NORM, 0, PITCH_HIGH);
		break;
	case 2:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/grenade_hit3.wav", 0.25, ATTN_NORM, 0, PITCH_HIGH);
		break;
	}
}
#endif // ! CLIENT_DLL

void CToolbow::Spawn()
{
	Precache( );
	SET_MODEL(ENT(pev), MyWModel());
	m_iId = WEAPON_TOOLBOW;

	m_iDefaultAmmo = -1;

	FallInit();// get ready to fall down.
}

void CToolbow::Precache()
{
	PRECACHE_MODEL("models/v_toolbow.mdl");
	PRECACHE_MODEL(MyWModel());
	PRECACHE_MODEL("models/p_toolbow.mdl");         

	PRECACHE_SOUND ("weapons/tg_shot1.wav");
	PRECACHE_SOUND ("weapons/tg_shot2.wav");

	m_iTeleport = PRECACHE_MODEL("sprites/blast.spr");
	m_usToolBow = PRECACHE_EVENT(1, "events/toolbow.sc");

	UTIL_PrecacheOther("item_teleporter");
	UTIL_PrecacheOther("item_glowstick");
}

bool CToolbow::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 3;
	p->iId = m_iId = WEAPON_TOOLBOW;
	p->iWeight = GLOCK_WEIGHT;

	return true;
}

bool CToolbow::Deploy()
{
	m_bSpotVisible = true;
	m_bLaserActive = true;

	return DefaultDeploy("models/v_toolbow.mdl", "models/p_toolbow.mdl", TOOLBOW_DRAW, "toolbow");
}

void CToolbow::Holster()
{
#ifndef CLIENT_DLL
	if (m_pLaser)
	{
		m_pLaser->Killed(nullptr, GIB_NEVER);
		m_pLaser = nullptr;
		m_bSpotVisible = false;
	}
#endif

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

	m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10.0, 15.0);
}

void CToolbow::PrimaryAttack()
{
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecSrcBalls = m_pPlayer->pev->origin;
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	Vector vecDir;

	// some specific tools are defined here
	if (m_pPlayer->m_iToolMode == 4)
	{
#ifndef CLIENT_DLL
		CGib::SpawnRandomGibs(pev, 1, true);
#endif
	}
	else if (m_pPlayer->m_iToolMode == 6)
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
	else if (m_pPlayer->m_iToolMode == 13)
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

			while ((pTeleport = UTIL_FindEntityInSphere(pTeleport, m_pPlayer->pev->origin, 4096)) != NULL)
			{
				if (FClassnameIs(pTeleport->pev, "item_teleporter"))
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
							MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecSrcBalls);
							WRITE_BYTE(TE_SPRITE);
							WRITE_COORD(vecSrcBalls.x);
							// pos
							WRITE_COORD(vecSrcBalls.y);
							WRITE_COORD(vecSrcBalls.z);
							WRITE_SHORT(m_iTeleport); // model
							WRITE_BYTE(23);			  // size * 10
							WRITE_BYTE(128);		  // brightness
							MESSAGE_END();

							EMIT_SOUND(ENT(pTeleport->pev), CHAN_WEAPON, "weapons/displacer_impact.wav", 1.0, ATTN_NORM);

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
	else if (m_pPlayer->m_iToolMode == 14)
	{
		Vector vecThrow = gpGlobals->v_forward * 274 + m_pPlayer->pev->velocity;

		CBaseEntity* pSatchel = Create("item_glowstick", vecSrcBalls, Vector(0, 0, 0), m_pPlayer->edict());
		pSatchel->pev->velocity = vecThrow;
	}
	else
		vecDir = m_pPlayer->FireBulletsToolBow(1, vecSrc, vecAiming, VECTOR_CONE_1DEGREES, 8192, BULLET_PLAYER_MP5, 2, 0, m_pPlayer->pev, m_pPlayer->random_seed);


  int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif
	
	PLAYBACK_EVENT_FULL(flags, m_pPlayer->edict(), m_usToolBow, 0.0, (float*)&g_vecZero, (float*)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0);

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.3;

	if ( m_flNextPrimaryAttack < UTIL_WeaponTimeBase() )
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.3;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );

#ifndef CLIENT_DLL
	ResetLaserAttack();
#endif
}

void CToolbow::SecondaryAttack()
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
	if (m_pPlayer->m_iToolMode == 4)
	{
#ifndef CLIENT_DLL
		CGib::SpawnRandomGibs(pev, 1, false);
#endif
	}
	else if (m_pPlayer->m_iToolMode == 14)
	{
		edict_t* pPlayer = m_pPlayer->edict();
		CBaseEntity* pGlowstick = NULL;

		while ((pGlowstick = UTIL_FindEntityInSphere(pGlowstick, m_pPlayer->pev->origin, 4096)) != NULL)
		{
			if (FClassnameIs(pGlowstick->pev, "item_glowstick"))
			{
				if (pGlowstick->pev->owner == pPlayer)
				{
					pGlowstick->Use(m_pPlayer, m_pPlayer, USE_ON, 0);
				}
			}
		}

		ClientPrint(m_pPlayer->pev, HUD_PRINTCENTER, "#Gomod_Glowsticks_Removed");
	}
	else
		vecDir = m_pPlayer->FireBulletsToolBowAlt(1, vecSrc, vecAiming, VECTOR_CONE_1DEGREES, 8192, BULLET_PLAYER_MP5, 2, 0, m_pPlayer->pev, m_pPlayer->random_seed);

  int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif
	
	PLAYBACK_EVENT_FULL(flags, m_pPlayer->edict(), m_usToolBow, 0.0, (float*)&g_vecZero, (float*)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0);

	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.3;

	if ( m_flNextSecondaryAttack < UTIL_WeaponTimeBase() )
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.1;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );

#ifndef CLIENT_DLL
	ResetLaserAttack();
#endif
}

void CToolbow::WeaponIdle()
{
#ifndef CLIENT_DLL
	UpdateLaser();
#endif

	ResetEmptySound( );
	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	int iAnim;
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:	
		iAnim = TOOLBOW_IDLE;	
		break;
	
	default:
	case 1:
		iAnim = TOOLBOW_IDLE2;
		break;
	}

	SendWeaponAnim( iAnim );
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 ); // how long till we do this again.
}

void CToolbow::ResetLaserAttack()
{
#ifndef CLIENT_DLL
	if (m_pLaser && m_bLaserActive)
	{
		m_pLaser->pev->effects |= EF_NODRAW;
		m_pLaser->SetThink(&CToolbowLaser::Revive);
		m_pLaser->pev->nextthink = gpGlobals->time + 0.6;
	}

	UpdateLaser();
#endif
}

void CToolbow::UpdateLaser()
{
#ifndef CLIENT_DLL
	if (m_bLaserActive && m_bSpotVisible)
	{
		if (!m_pLaser)
		{
			m_pLaser = CToolbowLaser::CreateSpot();

			EMIT_SOUND_DYN(edict(), CHAN_WEAPON, "weapons/desert_eagle_sight.wav", VOL_NORM, ATTN_NORM, 0, PITCH_HIGH);
		}

		UTIL_MakeVectors(m_pPlayer->pev->v_angle);

		Vector vecSrc = m_pPlayer->GetGunPosition();

		Vector vecEnd = vecSrc + gpGlobals->v_forward * 8192.0;

		TraceResult tr;

		UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, m_pPlayer->edict(), &tr);

		UTIL_SetOrigin(m_pLaser->pev, tr.vecEndPos);
	}
#endif
}

void CToolbow::ThrowTP()
{
	Vector vecSrc = m_pPlayer->GetGunPosition() + gpGlobals->v_up * -2 + gpGlobals->v_right * 2;
	Vector vecThrow = gpGlobals->v_forward * 650 + m_pPlayer->pev->velocity;

	CBaseEntity* pEnt = CTeleporter::ShootTeleporter(m_pPlayer->pev, vecSrc, vecThrow);
	CTeleporter* pTeleport = (CTeleporter*)pEnt;

	//EMIT_SOUND(ENT(pTeleport->pev), CHAN_WEAPON, "x/x_shoot1.wav", 0.5, ATTN_NORM);

	not_out = 1;
}

void CToolbow::GetWeaponData(weapon_data_t& data)
{
	BaseClass::GetWeaponData(data);

	data.iuser1 = static_cast<int>(m_bLaserActive);
}

void CToolbow::SetWeaponData(const weapon_data_t& data)
{
	BaseClass::SetWeaponData(data);

	m_bLaserActive = data.iuser1 != 0;
}