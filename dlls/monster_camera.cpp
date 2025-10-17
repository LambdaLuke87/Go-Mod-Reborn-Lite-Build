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
#include "schedule.h"
#include "player.h"
#include "weapons.h"

class CCamera : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override;
	int IgnoreConditions() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);

	static TYPEDESCRIPTION m_SaveData[];

	int player_use;
	EHANDLE m_hPlayer;
};

LINK_ENTITY_TO_CLASS(monster_camera, CCamera);

TYPEDESCRIPTION CCamera::m_SaveData[] =
	{
		DEFINE_FIELD(CCamera, m_hPlayer, FIELD_EHANDLE),
};
IMPLEMENT_SAVERESTORE(CCamera, CBaseMonster);

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CCamera ::Classify()
{
	return CLASS_NONE;
}

bool CCamera ::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{

	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

CBaseEntity* CBaseEntity::CreateCamera(const Vector& vecOrigin, const Vector& vecAngles, CBasePlayer* pOwner)
{
	if (!pOwner)
		return nullptr;

	// delete previous cameras from the same owner
	CBaseEntity* pEnt = nullptr;
	while ((pEnt = UTIL_FindEntityByClassname(pEnt, "monster_camera")) != nullptr)
	{
		CCamera* pCam = dynamic_cast<CCamera*>(pEnt);
		if (pCam && pCam->m_hPlayer == pOwner)
		{
			UTIL_Remove(pCam);
			break; // only one camera per player
		}
	}

	// create new Camera
	edict_t* pent = CREATE_NAMED_ENTITY(MAKE_STRING("monster_camera"));
	if (!pent)
		return nullptr;

	CCamera* pCamera = static_cast<CCamera*>(Instance(pent));
	if (!pCamera)
		return nullptr;

	pCamera->pev->origin = vecOrigin;
	pCamera->pev->angles = vecAngles;
	pCamera->m_hPlayer = pOwner; // assign owner directly

	DispatchSpawn(pCamera->edict());
	return pCamera;
}

//=========================================================
// Spawn
//=========================================================
void CCamera::Spawn()
{
	Precache();

	if (!m_hPlayer)
	{
		UTIL_Remove(this);
		return;
	}

	SET_MODEL(ENT(pev), "models/gomod/camera.mdl");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));

	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_FLY;
	m_bloodColor = byte(209);
	pev->health = 999999;
	m_MonsterState = MONSTERSTATE_NONE;

	pev->takedamage = DAMAGE_NO;
	pev->flags |= FL_MONSTER;

	MonsterInit();

	pev->origin.z += 25;
	pev->origin.y -= 25;
	player_use = 0;

	pev->nextthink = gpGlobals->time + 0.1;
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CCamera ::Precache()
{
	PRECACHE_MODEL("models/gomod/camera.mdl");
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
int CCamera::IgnoreConditions()
{
	int iIgnore = CBaseMonster::IgnoreConditions();

	if (m_hPlayer->pev->button & (IN_CAMERA))
	{
		if (player_use == 0)
		{
			player_use = 1;
			pev->angles.x *= -1;
			SET_VIEW(m_hPlayer->edict(), edict());
		}
		else
		{
			player_use = 0;
			pev->angles.x *= -1;
			SET_VIEW(m_hPlayer->edict(), m_hPlayer->edict());
		}
	}

	if (m_hPlayer->pev->health <= 0)
	{
		SET_VIEW(m_hPlayer->edict(), m_hPlayer->edict());
	}

	if (player_use == 1)
	{
		//	SET_VIEW( m_hPlayer->edict(), edict() );
		pev->renderamt = 0; // The engine won't draw this model if this is set to 0 and blending is on
		pev->rendermode = kRenderTransTexture;
	}
	else
	{
		//	SET_VIEW( m_hPlayer->edict(), m_hPlayer->edict() );
		pev->renderamt = 255; // The engine won't draw this model if this is set to 0 and blending is on
		pev->rendermode = kRenderNormal;
	}

	pev->nextthink = gpGlobals->time + 0.1;
	return iIgnore;
}