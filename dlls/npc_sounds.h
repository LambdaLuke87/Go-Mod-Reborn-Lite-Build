#pragma once

// Go-Mod: Reborn - sounds of the NPCs, defined OUTSIDE the code.
//
// Every NPC has its own file: sound_configs/<config>.json (normally <config> = its classname)
//
//   {
//       "pain":  [ "zombie/zo_pain1.wav", "zombie/zo_pain2.wav" ],
//       "idle":  [ "zombie/zo_idle1.wav", "!ZO_IDLE" ],
//       "alert": { "sounds": [ "zombie/zo_alert10.wav" ], "volume": 0.8, "pitch": 1.1 }
//   }
//
//  - each key is a group ("pain", "idle", "attack_hit"...), the code picks a random sound of it
//  - a group is a list of sounds, or an object with "sounds" + optional "volume" / "pitch"
//    (both multiply what the code asks for, 1.0 = unchanged)
//  - "!NAME" plays a sentence of sound/sentences.txt
//
// Everything is played through the Go-Mod audio system (gm_audio_server.h): nothing is precached.
// The files are read the first time they are needed and read again on every new map.

class CBaseEntity;
class Vector;

// Plays a random sound of the group on this entity. false if the group doesn't exist / is empty.
bool NPCSounds_Emit(CBaseEntity* pEntity, const char* config, const char* group, int channel, float volume, float attenuation, int pitch);

// Same, but the sound number 'index' of the group (for code that needs a specific one)
bool NPCSounds_EmitIndex(CBaseEntity* pEntity, const char* config, const char* group, int index, int channel, float volume, float attenuation, int pitch);

// Random sound of the group at a point of the world (instead of UTIL_EmitAmbientSound)
bool NPCSounds_EmitAmbient(const Vector& origin, const char* config, const char* group, float volume, float attenuation, int pitch);

int NPCSounds_Count(const char* config, const char* group);

// path of a sound of the group (random / by index). nullptr if it doesn't exist
const char* NPCSounds_Random(const char* config, const char* group);
const char* NPCSounds_Get(const char* config, const char* group, int index);

// forget every file (called on every new map, and by the command "gm_reload_sound_configs")
void NPCSounds_ClearCache();
