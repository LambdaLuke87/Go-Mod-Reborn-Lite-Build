#include "extdll.h"
#include "util.h"
#include "cbase.h"

#include "gm_audio_server.h"
#include "gm_audio_shared.h"

static int gmsgGMSound = 0;

void GMAudio_RegisterMessages()
{
	gmsgGMSound = REG_USER_MSG("GMSound", -1);
}

static int ClampInt(int v, int lo, int hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

static void WriteSound(int flags, int entindex, int channel, const char* path, float volume, float attenuation, int pitch, const Vector& origin)
{
	WRITE_BYTE(flags);
	WRITE_SHORT(entindex);
	WRITE_BYTE(ClampInt(channel, 0, 255));
	WRITE_BYTE(ClampInt((int)(volume * 255.0f + 0.5f), 0, 255));
	WRITE_BYTE(ClampInt((int)(attenuation * 64.0f + 0.5f), 0, 255));
	WRITE_BYTE(ClampInt(pitch, 1, 255));
	WRITE_COORD(origin.x);
	WRITE_COORD(origin.y);
	WRITE_COORD(origin.z);
	WRITE_STRING(path);
}

static Vector SoundOrigin(CBaseEntity* pEntity)
{
	// Brush entities (doors...) have their origin at 0,0,0: use the center of the brush
	if (pEntity->pev->solid == SOLID_BSP || pEntity->pev->movetype == MOVETYPE_PUSH)
		return VecBModelOrigin(pEntity->pev);
	return pEntity->pev->origin;
}

static void SendSound(CBaseEntity* pEntity, int flags, int channel, const char* path, float volume, float attenuation, int pitch)
{
	if (!pEntity || !pEntity->pev || gmsgGMSound == 0)
		return;

	const Vector origin = SoundOrigin(pEntity);

	// MSG_PAS: only the clients that could hear it, like the engine does with its own sounds
	MESSAGE_BEGIN(MSG_PAS, gmsgGMSound, origin);
	WriteSound(flags, ENTINDEX(pEntity->edict()), channel, path, volume, attenuation, pitch, origin);
	MESSAGE_END();
}

void GMAudio_Emit(CBaseEntity* pEntity, int channel, const char* path, float volume, float attenuation, int pitch)
{
	if (!GMAudio_IsValidSoundPath(path))
	{
		ALERT(at_console, "GMAudio: invalid sound path '%s' (letters, numbers, _ - / . only, must end in .wav)\n", path ? path : "(null)");
		return;
	}

	SendSound(pEntity, 0, channel, GMAudio_RelativePath(path), volume, attenuation, pitch);
}

void GMAudio_Stop(CBaseEntity* pEntity, int channel)
{
	SendSound(pEntity, GM_SND_STOP, channel, "stop.wav", 0.0f, 0.0f, 100);
}

void GMAudio_EmitAmbient(const Vector& origin, const char* path, float volume, float attenuation, int pitch)
{
	if (gmsgGMSound == 0)
		return;

	if (!GMAudio_IsValidSoundPath(path))
	{
		ALERT(at_console, "GMAudio: invalid sound path '%s'\n", path ? path : "(null)");
		return;
	}

	MESSAGE_BEGIN(MSG_PAS, gmsgGMSound, origin);
	WriteSound(0, 0, 0, GMAudio_RelativePath(path), volume, attenuation, pitch, origin); // entity 0 = the world
	MESSAGE_END();
}

void GMAudio_EmitPredicted(CBaseEntity* pPlayer, int channel, const char* path, float volume, float attenuation, int pitch)
{
	if (!pPlayer || !pPlayer->pev || gmsgGMSound == 0)
		return;

	if (!GMAudio_IsValidSoundPath(path))
	{
		ALERT(at_console, "GMAudio: invalid sound path '%s'\n", path ? path : "(null)");
		return;
	}

	// the owner only gets it if it does NOT predict (then nobody played it for him)
	const bool skipOwner = 0 != g_engfuncs.pfnCanSkipPlayer(pPlayer->edict());
	const Vector origin = SoundOrigin(pPlayer);

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pClient = UTIL_PlayerByIndex(i);
		if (!pClient || (skipOwner && pClient == pPlayer))
			continue;

		MESSAGE_BEGIN(MSG_ONE, gmsgGMSound, nullptr, pClient->edict());
		WriteSound(0, ENTINDEX(pPlayer->edict()), channel, GMAudio_RelativePath(path), volume, attenuation, pitch, origin);
		MESSAGE_END();
	}
}

bool GMAudio_IsNPC(CBaseEntity* pEntity)
{
	return pEntity && pEntity->pev && !pEntity->IsPlayer() && pEntity->MyMonsterPointer() != nullptr;
}
