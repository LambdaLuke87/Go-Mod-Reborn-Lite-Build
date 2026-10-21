//=========================================================
// Go-Mod: Reborn - Snark Frenzy (mp_gamemode 3)
//
// Hot potato with a snark:
//  - one player (the carrier) has a single snark. When it bites someone, that player becomes the
//    carrier. If it doesn't bite anyone in mp_sf_snark_return seconds it goes back to the carrier.
//  - when the fuse (mp_sf_fuse) reaches 0 the carrier explodes and goes to spectator mode.
//  - nobody can get hurt: no fall damage, the snark only passes the bomb, the crowbars only push,
//    the explosion only kills the carrier.
//  - the last player alive wins the round (+1 point). First to mp_sf_winscore wins the map.
//  - countdowns: mp_sf_countdown at the start of every round, mp_sf_pickdelay before choosing the
//    next carrier after an explosion.
//  - models: everybody "helmet", the carrier "skeleton".
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "game.h"
#include "client.h"
#include "UserMessages.h"
#include "gm_audio_server.h"

cvar_t sf_fuse = {"mp_sf_fuse", "30", FCVAR_SERVER};				 // seconds until the carrier explodes
cvar_t sf_countdown = {"mp_sf_countdown", "10", FCVAR_SERVER};	 // countdown at the start of every round
cvar_t sf_pickdelay = {"mp_sf_pickdelay", "10", FCVAR_SERVER};	 // countdown before choosing the next carrier
cvar_t sf_winscore = {"mp_sf_winscore", "5", FCVAR_SERVER};		 // rounds won to win the map
cvar_t sf_snark_return = {"mp_sf_snark_return", "4", FCVAR_SERVER}; // seconds the thrown snark has to bite someone
cvar_t sf_fuse_reset = {"mp_sf_fuse_reset", "0", FCVAR_SERVER};	 // 1 = the fuse starts again every time the snark changes hands
cvar_t sf_crowbar_push = {"mp_sf_crowbar_push", "550", FCVAR_SERVER}; // crowbar push force

#define SF_ROUNDEND_TIME 5.0f
#define SF_OBSERVER_DELAY 3.0f
#define SF_PASS_DELAY 0.75f // the new carrier can't throw it back right away

void SnarkFrenzy_RegisterCvars()
{
	CVAR_REGISTER(&sf_fuse);
	CVAR_REGISTER(&sf_countdown);
	CVAR_REGISTER(&sf_pickdelay);
	CVAR_REGISTER(&sf_winscore);
	CVAR_REGISTER(&sf_snark_return);
	CVAR_REGISTER(&sf_fuse_reset);
	CVAR_REGISTER(&sf_crowbar_push);
}

static CMultiplaySnarkFrenzy* SnarkFrenzyRules()
{
	if (!g_pGameRules || !g_pGameRules->IsSnarkFrenzy())
		return nullptr;

	return static_cast<CMultiplaySnarkFrenzy*>(g_pGameRules);
}

// ---- called from other files ----

void SnarkFrenzy_SnarkSpawned(CBaseEntity* pSnark, CBaseEntity* pOwner)
{
	if (auto rules = SnarkFrenzyRules())
		rules->SnarkSpawned(pSnark, pOwner);
}

bool SnarkFrenzy_SnarkBite(CBaseEntity* pSnark, CBaseEntity* pVictim)
{
	if (auto rules = SnarkFrenzyRules())
		return rules->SnarkBite(pSnark, pVictim);
	return false;
}

bool SnarkFrenzy_IsCarrier(CBaseEntity* pPlayer)
{
	if (auto rules = SnarkFrenzyRules())
		return rules->IsCarrier(pPlayer);
	return false;
}

float SnarkFrenzy_CrowbarPush()
{
	return sf_crowbar_push.value;
}

//=========================================================

CMultiplaySnarkFrenzy::CMultiplaySnarkFrenzy()
{
	m_State = SF_WAITING;
	m_flStateEnd = 0.0f;
}

CBasePlayer* CMultiplaySnarkFrenzy::GetPlayer(int index)
{
	if (index < 1 || index > gpGlobals->maxClients || index > MAX_PLAYERS)
		return nullptr;

	if (!m_bConnected[index])
		return nullptr;

	CBaseEntity* pEntity = UTIL_PlayerByIndex(index);
	if (!pEntity || !pEntity->IsPlayer())
		return nullptr;

	if ((pEntity->pev->flags & FL_PROXY) != 0)
		return nullptr;

	return static_cast<CBasePlayer*>(pEntity);
}

bool CMultiplaySnarkFrenzy::IsInRound(int index)
{
	CBasePlayer* pPlayer = GetPlayer(index);
	return pPlayer && m_bInRound[index];
}

int CMultiplaySnarkFrenzy::CountInRound(int* pLast)
{
	int count = 0;
	for (int i = 1; i <= gpGlobals->maxClients && i <= MAX_PLAYERS; i++)
	{
		if (IsInRound(i))
		{
			count++;
			if (pLast)
				*pLast = i;
		}
	}
	return count;
}

int CMultiplaySnarkFrenzy::CountConnected()
{
	int count = 0;
	for (int i = 1; i <= gpGlobals->maxClients && i <= MAX_PLAYERS; i++)
	{
		if (GetPlayer(i))
			count++;
	}
	return count;
}

bool CMultiplaySnarkFrenzy::IsCarrier(CBaseEntity* pPlayer)
{
	return pPlayer && m_iCarrier > 0 && pPlayer->entindex() == m_iCarrier;
}

//=========================================================
// HUD
//=========================================================

void CMultiplaySnarkFrenzy::UpdateGameMode(CBasePlayer* pPlayer)
{
	MESSAGE_BEGIN(MSG_ONE, gmsgGameMode, NULL, pPlayer->edict());
	WRITE_BYTE(4); // client: UTIL_IsSnarkFrenzy()
	MESSAGE_END();
}

void CMultiplaySnarkFrenzy::SendState(CBasePlayer* pTo)
{
	float remaining = 0.0f;

	if (m_State == SF_PLAYING)
		remaining = m_flFuseEnd - gpGlobals->time;
	else if (m_State == SF_COUNTDOWN || m_State == SF_ROUNDEND)
		remaining = m_flStateEnd - gpGlobals->time;

	if (remaining < 0.0f)
		remaining = 0.0f;

	int flags = 0;
	if (m_bRoundStartCountdown)
		flags |= 1;

	if (pTo)
		MESSAGE_BEGIN(MSG_ONE, gmsgSnarkFrenzy, NULL, pTo->edict());
	else
		MESSAGE_BEGIN(MSG_ALL, gmsgSnarkFrenzy);

	WRITE_BYTE(m_State);
	WRITE_BYTE(m_iCarrier);
	WRITE_SHORT((int)(remaining * 10.0f));
	WRITE_BYTE(m_iRoundWinner);
	WRITE_BYTE(flags);
	WRITE_BYTE((int)sf_winscore.value);
	MESSAGE_END();
}

void CMultiplaySnarkFrenzy::InitHUD(CBasePlayer* pl)
{
	const int index = pl->entindex();
	if (index >= 1 && index <= MAX_PLAYERS)
		m_bConnected[index] = true;

	CHalfLifeMultiplay::InitHUD(pl);

	SendState(pl);
}

void CMultiplaySnarkFrenzy::ClientDisconnected(edict_t* pClient)
{
	const int index = ENTINDEX(pClient);

	if (index >= 1 && index <= MAX_PLAYERS)
	{
		m_bConnected[index] = false;
		m_bInRound[index] = false;
		m_flMakeObserver[index] = 0.0f;
	}

	// the carrier left: the snark goes with him (Think chooses another one)
	if (index == m_iCarrier)
		RemoveSnarkEntity();

	CHalfLifeMultiplay::ClientDisconnected(pClient);
}

void CMultiplaySnarkFrenzy::ClientUserInfoChanged(CBasePlayer* pPlayer, char* infobuffer)
{
	SetPlayerModel(pPlayer);
	pPlayer->SetPrefsFromUserinfo(infobuffer);
}

void CMultiplaySnarkFrenzy::SetPlayerModel(CBasePlayer* pPlayer)
{
	const char* model = IsCarrier(pPlayer) ? "skeleton" : "helmet";

	const char* current = g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model");
	if (current && strcmp(current, model) == 0)
		return;

	g_engfuncs.pfnSetClientKeyValue(pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", (char*)model);
}

//=========================================================
// Players
//=========================================================

void CMultiplaySnarkFrenzy::PlayerSpawn(CBasePlayer* pPlayer)
{
	pPlayer->SetHasSuit(true);

	g_engfuncs.pfnMessageBegin(MSG_ONE, gmsgOldWeapon, nullptr, pPlayer->edict());
	g_engfuncs.pfnWriteByte(static_cast<int>(1 == oldweapons.value));
	g_engfuncs.pfnMessageEnd();

	const int index = pPlayer->entindex();
	if (index >= 1 && index <= MAX_PLAYERS)
	{
		m_bConnected[index] = true;

		// a round is being played and this player is not in it: spectate until the next one
		// (during the countdown at the start of a round you still get in)
		const bool roundRunning = (m_State == SF_PLAYING || m_State == SF_ROUNDEND ||
								   (m_State == SF_COUNTDOWN && !m_bRoundStartCountdown));

		if (roundRunning && !m_bInRound[index])
		{
			m_flMakeObserver[index] = gpGlobals->time + 0.2f;
		}
		else
		{
			m_bInRound[index] = true;
			m_flMakeObserver[index] = 0.0f;
		}
	}

	if (IsCarrier(pPlayer))
		GiveSnark(pPlayer);
	else
		GiveCrowbar(pPlayer);

	SetPlayerModel(pPlayer);
}

bool CMultiplaySnarkFrenzy::FPlayerCanRespawn(CBasePlayer* pPlayer)
{
	if (m_State == SF_WAITING)
		return true;

	const int index = pPlayer->entindex();
	return index >= 1 && index <= MAX_PLAYERS && m_bInRound[index];
}

void CMultiplaySnarkFrenzy::PlayerKilled(CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pInflictor)
{
	// the only way to die is the snark exploding: losing doesn't take points away
	MESSAGE_BEGIN(MSG_ALL, gmsgDeathMsg);
	WRITE_BYTE(0);
	WRITE_BYTE(ENTINDEX(pVictim->edict()));
	WRITE_STRING("snark");
	MESSAGE_END();

	pVictim->m_iDeaths += 1;

	FireTargets("game_playerdie", pVictim, pVictim, USE_TOGGLE, 0);

	MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
	WRITE_BYTE(ENTINDEX(pVictim->edict()));
	WRITE_SHORT(pVictim->pev->frags);
	WRITE_SHORT(pVictim->m_iDeaths);
	MESSAGE_END();
}

bool CMultiplaySnarkFrenzy::CanHavePlayerItem(CBasePlayer* pPlayer, CBasePlayerItem* pWeapon)
{
	if (!m_bGiving)
		return false; // nothing from the map, weaponboxes...

	return CHalfLifeMultiplay::CanHavePlayerItem(pPlayer, pWeapon);
}

bool CMultiplaySnarkFrenzy::CanHaveAmmo(CBasePlayer* pPlayer, const char* pszAmmoName, int iMaxCarry)
{
	if (!m_bGiving)
		return false;

	return CHalfLifeMultiplay::CanHaveAmmo(pPlayer, pszAmmoName, iMaxCarry);
}

void CMultiplaySnarkFrenzy::GiveCrowbar(CBasePlayer* pPlayer)
{
	pPlayer->RemoveAllItems(false);

	m_bGiving = true;
	pPlayer->GiveNamedItem("weapon_crowbar");
	m_bGiving = false;
}

void CMultiplaySnarkFrenzy::GiveSnark(CBasePlayer* pPlayer)
{
	pPlayer->RemoveAllItems(false);

	m_bGiving = true;
	pPlayer->GiveNamedItem("weapon_snark", 1);
	m_bGiving = false;

	// exactly one snark
	const int ammo = CBasePlayer::GetAmmoIndex("Snarks");
	if (ammo >= 0)
		pPlayer->m_rgAmmo[ammo] = 1;

	pPlayer->SelectItem("weapon_snark");

	// can't throw it back in the same instant
	pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + SF_PASS_DELAY;
}

void CMultiplaySnarkFrenzy::RespawnForRound(CBasePlayer* pPlayer)
{
	if (pPlayer->IsObserver())
	{
		pPlayer->StopObserver(); // calls Spawn
	}
	else if (!pPlayer->IsAlive())
	{
		respawn(pPlayer->pev, false);
	}
	else
	{
		pPlayer->RemoveAllItems(false);
		pPlayer->Spawn();
	}
}

//=========================================================
// Snark
//=========================================================

CBaseEntity* CMultiplaySnarkFrenzy::GetSnark()
{
	if (!m_pSnarkEdict || m_pSnarkEdict->free || m_pSnarkEdict->serialnumber != m_iSnarkSerial)
		return nullptr;

	return CBaseEntity::Instance(m_pSnarkEdict);
}

void CMultiplaySnarkFrenzy::SetSnark(CBaseEntity* pSnark)
{
	m_pSnarkEdict = pSnark ? pSnark->edict() : nullptr;
	m_iSnarkSerial = m_pSnarkEdict ? m_pSnarkEdict->serialnumber : 0;
}

void CMultiplaySnarkFrenzy::SnarkSpawned(CBaseEntity* pSnark, CBaseEntity* pOwner)
{
	// only one: if there was another one running around, it goes away
	CBaseEntity* pOld = GetSnark();
	if (pOld && pOld != pSnark)
		RemoveSnarkEntity();

	SetSnark(pSnark);
	m_flSnarkThrown = gpGlobals->time;
}

void CMultiplaySnarkFrenzy::RemoveSnarkEntity()
{
	CBaseEntity* pSnark = GetSnark();
	if (pSnark)
	{
		pSnark->SetTouch(nullptr);
		pSnark->SetThink(nullptr);
		UTIL_Remove(pSnark);
	}

	SetSnark(nullptr);
}

bool CMultiplaySnarkFrenzy::SnarkBite(CBaseEntity* pSnark, CBaseEntity* pVictim)
{
	if (!pVictim || !pVictim->IsPlayer())
		return false;

	CBasePlayer* pPlayer = static_cast<CBasePlayer*>(pVictim);
	const int index = pPlayer->entindex();

	// the carrier can't bite himself, spectators and players out of the round don't count
	if (index == m_iCarrier || pPlayer->IsObserver() || !pPlayer->IsAlive() || !IsInRound(index))
		return true; // handled: no damage, keeps running

	if (m_State != SF_PLAYING)
		return true;

	GMAudio_Emit(pPlayer, CHAN_VOICE, "squeek/sqk_deploy1.wav", 1.0f, ATTN_NORM, 100);

	RemoveSnarkEntity();

	CBasePlayer* pOld = GetPlayer(m_iCarrier);

	SetCarrier(index, true);

	if (pOld)
	{
		GiveCrowbar(pOld);
		SetPlayerModel(pOld);
	}

	return true;
}

void CMultiplaySnarkFrenzy::SetCarrier(int index, bool announce)
{
	m_iCarrier = index;

	CBasePlayer* pPlayer = GetPlayer(index);
	if (!pPlayer)
	{
		m_iCarrier = 0;
		SendState(nullptr);
		return;
	}

	if (sf_fuse_reset.value != 0.0f || m_flFuseEnd <= gpGlobals->time)
		m_flFuseEnd = gpGlobals->time + V_max(1.0f, sf_fuse.value);

	GiveSnark(pPlayer);
	SetPlayerModel(pPlayer);

	if (announce)
	{
		UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("%s has the snark!\n", STRING(pPlayer->pev->netname)));
		ClientPrint(pPlayer->pev, HUD_PRINTCENTER, "YOU HAVE THE SNARK!\nThrow it at someone!");
	}

	SendState(nullptr);
}

//=========================================================
// Round flow
//=========================================================

void CMultiplaySnarkFrenzy::SetState(State state, float duration)
{
	m_State = state;
	m_flStateEnd = gpGlobals->time + duration;
	SendState(nullptr);
}

void CMultiplaySnarkFrenzy::StartRound()
{
	RemoveSnarkEntity();
	m_iCarrier = 0;
	m_iRoundWinner = 0;
	m_flFuseEnd = 0.0f;

	for (int i = 1; i <= gpGlobals->maxClients && i <= MAX_PLAYERS; i++)
	{
		CBasePlayer* pPlayer = GetPlayer(i);
		m_bInRound[i] = pPlayer != nullptr;
		m_flMakeObserver[i] = 0.0f;

		if (pPlayer)
			RespawnForRound(pPlayer);
	}

	m_bRoundStartCountdown = true;
	SetState(SF_COUNTDOWN, V_max(0.0f, sf_countdown.value));

	UTIL_ClientPrintAll(HUD_PRINTTALK, "Snark Frenzy: a new round begins!\n");
}

void CMultiplaySnarkFrenzy::StartPickCountdown()
{
	m_iCarrier = 0;
	m_bRoundStartCountdown = false;
	SetState(SF_COUNTDOWN, V_max(0.0f, sf_pickdelay.value));
}

void CMultiplaySnarkFrenzy::PickCarrier()
{
	int candidates[MAX_PLAYERS];
	int count = 0;

	for (int i = 1; i <= gpGlobals->maxClients && i <= MAX_PLAYERS; i++)
	{
		CBasePlayer* pPlayer = GetPlayer(i);
		if (pPlayer && m_bInRound[i] && pPlayer->IsAlive() && !pPlayer->IsObserver())
			candidates[count++] = i;
	}

	if (count < 2)
	{
		int last = 0;
		CountInRound(&last);
		EndRound(count == 1 ? candidates[0] : 0);
		return;
	}

	m_bRoundStartCountdown = false;
	m_State = SF_PLAYING;
	m_flFuseEnd = 0.0f; // new fuse
	SetCarrier(candidates[RANDOM_LONG(0, count - 1)], true);
}

void CMultiplaySnarkFrenzy::Explode(int index)
{
	CBasePlayer* pPlayer = GetPlayer(index);

	RemoveSnarkEntity();
	m_iCarrier = 0;

	if (index >= 1 && index <= MAX_PLAYERS)
		m_bInRound[index] = false;

	if (pPlayer && pPlayer->IsAlive())
	{
		const Vector origin = pPlayer->pev->origin + Vector(0, 0, 16);

		// only effects: the explosion doesn't hurt anyone else
		MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, origin);
		WRITE_BYTE(TE_EXPLOSION);
		WRITE_COORD(origin.x);
		WRITE_COORD(origin.y);
		WRITE_COORD(origin.z);
		WRITE_SHORT(g_sModelIndexFireball);
		WRITE_BYTE(30); // scale * 10
		WRITE_BYTE(15); // framerate
		WRITE_BYTE(TE_EXPLFLAG_NOSOUND);
		MESSAGE_END();

		GMAudio_EmitAmbient(origin, "weapons/explode3.wav", 1.0f, ATTN_NORM, 100);

		UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("%s exploded!\n", STRING(pPlayer->pev->netname)));

		// kill him (FPlayerCanTakeDamage is always false, so straight to Killed)
		g_pevLastInflictor = pPlayer->pev;
		pPlayer->pev->health = 0;
		pPlayer->Killed(pPlayer->pev, GIB_ALWAYS);

		m_flMakeObserver[index] = gpGlobals->time + SF_OBSERVER_DELAY;
		SetPlayerModel(pPlayer);
	}

	int last = 0;
	const int alive = CountInRound(&last);

	if (alive <= 1)
		EndRound(alive == 1 ? last : 0);
	else
		StartPickCountdown();
}

void CMultiplaySnarkFrenzy::EndRound(int winner)
{
	RemoveSnarkEntity();
	m_iCarrier = 0;
	m_iRoundWinner = winner;

	CBasePlayer* pWinner = GetPlayer(winner);

	if (pWinner)
	{
		pWinner->pev->frags += 1;

		MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
		WRITE_BYTE(winner);
		WRITE_SHORT(pWinner->pev->frags);
		WRITE_SHORT(pWinner->m_iDeaths);
		MESSAGE_END();

		SetPlayerModel(pWinner);

		UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("%s survived and wins the round! (%d/%d)\n",
											   STRING(pWinner->pev->netname), (int)pWinner->pev->frags, (int)sf_winscore.value));

		if (sf_winscore.value > 0 && pWinner->pev->frags >= sf_winscore.value)
		{
			UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("%s wins the game!\n", STRING(pWinner->pev->netname)));
			SetState(SF_ROUNDEND, SF_ROUNDEND_TIME);
			GoToIntermission();
			return;
		}
	}
	else
	{
		UTIL_ClientPrintAll(HUD_PRINTTALK, "Nobody survived the round.\n");
	}

	SetState(SF_ROUNDEND, SF_ROUNDEND_TIME);
}

void CMultiplaySnarkFrenzy::Think()
{
	CHalfLifeMultiplay::Think();

	if (g_fGameOver)
		return;

	// players that are out go to spectator mode
	for (int i = 1; i <= gpGlobals->maxClients && i <= MAX_PLAYERS; i++)
	{
		if (m_flMakeObserver[i] <= 0.0f || m_flMakeObserver[i] > gpGlobals->time)
			continue;

		m_flMakeObserver[i] = 0.0f;

		CBasePlayer* pPlayer = GetPlayer(i);
		if (pPlayer && !pPlayer->IsObserver() && !m_bInRound[i] && m_State != SF_WAITING)
			pPlayer->StartObserver(pPlayer->pev->origin, pPlayer->pev->v_angle);
	}

	const int connected = CountConnected();

	// not enough players
	if (connected < 2)
	{
		if (m_State != SF_WAITING)
		{
			RemoveSnarkEntity();
			m_iCarrier = 0;
			m_iRoundWinner = 0;

			// everybody back in the game, with crowbars
			for (int i = 1; i <= gpGlobals->maxClients && i <= MAX_PLAYERS; i++)
			{
				CBasePlayer* pPlayer = GetPlayer(i);
				m_bInRound[i] = pPlayer != nullptr;
				if (pPlayer && (pPlayer->IsObserver() || !pPlayer->IsAlive()))
					RespawnForRound(pPlayer);
				else if (pPlayer)
				{
					GiveCrowbar(pPlayer);
					SetPlayerModel(pPlayer);
				}
			}

			SetState(SF_WAITING, 0.0f);
		}

		if (m_flNextWaitingMsg <= gpGlobals->time)
		{
			m_flNextWaitingMsg = gpGlobals->time + 15.0f;
			UTIL_ClientPrintAll(HUD_PRINTTALK, "Snark Frenzy: waiting for more players (2 needed)\n");
		}
		return;
	}

	switch (m_State)
	{
	case SF_WAITING:
		StartRound();
		break;

	case SF_COUNTDOWN:
	{
		// everybody left during the countdown between explosions
		int last = 0;
		const int alive = CountInRound(&last);
		if (!m_bRoundStartCountdown && alive <= 1)
		{
			EndRound(alive == 1 ? last : 0);
			break;
		}

		if (m_flStateEnd <= gpGlobals->time)
			PickCarrier();
		break;
	}

	case SF_PLAYING:
	{
		CBasePlayer* pCarrier = GetPlayer(m_iCarrier);

		// the carrier left / died somehow
		if (!pCarrier || !m_bInRound[m_iCarrier] || !pCarrier->IsAlive() || pCarrier->IsObserver())
		{
			RemoveSnarkEntity();
			m_iCarrier = 0;

			int last = 0;
			const int alive = CountInRound(&last);
			if (alive <= 1)
				EndRound(alive == 1 ? last : 0);
			else
				StartPickCountdown();
			break;
		}

		// boom
		if (m_flFuseEnd <= gpGlobals->time)
		{
			Explode(m_iCarrier);
			break;
		}

		CBaseEntity* pSnark = GetSnark();

		if (pSnark)
		{
			// didn't bite anyone in time: back to the carrier
			if (gpGlobals->time - m_flSnarkThrown >= V_max(0.5f, sf_snark_return.value))
			{
				RemoveSnarkEntity();
				GiveSnark(pCarrier);
				ClientPrint(pCarrier->pev, HUD_PRINTCENTER, "The snark came back!");
			}
		}
		else
		{
			// no snark running around and the carrier doesn't have it (it fell out of the world...)
			const int ammo = CBasePlayer::GetAmmoIndex("Snarks");
			if (m_flSnarkThrown + 0.5f < gpGlobals->time && (ammo < 0 || pCarrier->m_rgAmmo[ammo] <= 0))
				GiveSnark(pCarrier);
		}
		break;
	}

	case SF_ROUNDEND:
		if (m_flStateEnd <= gpGlobals->time)
			StartRound();
		break;
	}
}
