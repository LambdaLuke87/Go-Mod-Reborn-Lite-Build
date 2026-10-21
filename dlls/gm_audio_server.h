#pragma once

// Server side of the Go-Mod audio engine: sends a sound to the clients that can hear it
// WITHOUT precaching it. The engine's 512 sound limit doesn't apply to these.
//
// The clients need the .wav file in their own game folder (the engine can't download it for them,
// because it is not in the precache list): ship it with the mod / the pack.

class CBaseEntity;
class Vector;

// Extra channels (the engine has 0-7, this system accepts 0-255). Same rule: a new sound replaces the
// previous one of that entity + channel, so the CTF charge sounds can be stopped one by one.
#define GM_CHAN_ARMOR_CHARGE 8
#define GM_CHAN_HEALTH_CHARGE 9

// Call from LinkUserMessages() (player.cpp), next to the other REG_USER_MSG
void GMAudio_RegisterMessages();

// 'path': "zombie/zo_idle1.wav" (relative to sound/; "sound/zombie/zo_idle1.wav" also works).
// volume 0..1, attenuation as in EMIT_SOUND (ATTN_NORM = 0.8, ATTN_NONE = 0), pitch 100 = normal.
// Same channel rules as the engine: a new sound replaces the previous one of that entity + channel.
void GMAudio_Emit(CBaseEntity* pEntity, int channel, const char* path, float volume, float attenuation, int pitch);

// Like STOP_SOUND(entity, channel)
void GMAudio_Stop(CBaseEntity* pEntity, int channel);

// Sound at a fixed point of the world, not attached to an entity (like UTIL_EmitAmbientSound)
void GMAudio_EmitAmbient(const Vector& origin, const char* path, float volume, float attenuation, int pitch);

// Like EMIT_SOUND_PREDICTED: if the player predicts its weapons (cl_lw 1) it already played the
// sound by itself (client weapon code), so it's sent to everyone else.
void GMAudio_EmitPredicted(CBaseEntity* pPlayer, int channel, const char* path, float volume, float attenuation, int pitch);

// true for monsters (not players): their sentences / sounds go through this system
bool GMAudio_IsNPC(CBaseEntity* pEntity);
