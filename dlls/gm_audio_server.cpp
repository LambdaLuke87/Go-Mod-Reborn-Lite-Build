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

static void SendSound(CBaseEntity* pEntity, int flags, int channel, const char* path, float volume, float attenuation, int pitch)
{
	if (!pEntity || !pEntity->pev || gmsgGMSound == 0)
		return;

	// Brush entities (doors...) have their origin at 0,0,0: use the center of the brush
	Vector origin = pEntity->pev->origin;
	if (pEntity->pev->solid == SOLID_BSP || pEntity->pev->movetype == MOVETYPE_PUSH)
		origin = VecBModelOrigin(pEntity->pev);

	// MSG_PAS: only the clients that could hear it, like the engine does with its own sounds
	MESSAGE_BEGIN(MSG_PAS, gmsgGMSound, origin);
	WRITE_BYTE(flags);
	WRITE_SHORT(ENTINDEX(pEntity->edict()));
	WRITE_BYTE(ClampInt(channel, 0, 255));
	WRITE_BYTE(ClampInt((int)(volume * 255.0f + 0.5f), 0, 255));
	WRITE_BYTE(ClampInt((int)(attenuation * 64.0f + 0.5f), 0, 255));
	WRITE_BYTE(ClampInt(pitch, 1, 255));
	WRITE_COORD(origin.x);
	WRITE_COORD(origin.y);
	WRITE_COORD(origin.z);
	WRITE_STRING(path);
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
