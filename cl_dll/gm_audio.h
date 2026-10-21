#pragma once

// Go-Mod client audio engine: plays sounds WITHOUT the engine's precache system
// (no 512 sound limit, no precache list, nothing to keep in sync with the server).
//
// The server sends the "GMSound" user message (see dlls/gm_audio_server.h) and this plays it
// through its own SDL audio device, mixed on top of the engine's sound.

void GMAudio_Init();	  // CHud::Init(): opens the audio device, hooks the message, registers the console commands
void GMAudio_Shutdown();  // HUD_Shutdown()
void GMAudio_StopAll();	  // CHud::VidInit(): new map / reconnect
void GMAudio_SetListener(const float* origin, const float* viewangles); // V_CalcRefdef(): once per frame
// 'path': "weapons/pl_gun3.wav" or a sentence "!NAME" (sound/sentences.txt)
void GMAudio_PlayLocal(const char* path, float volume = 1.0f, int pitch = 100);

// Like EV_PlaySound: positioned on an entity (client events of the weapons, etc.).
// origin can be nullptr (the entity's current position is used). Same channel rules as the engine.
void GMAudio_PlayEntity(int entity, const float* origin, int channel, const char* path, float volume, float attenuation, int pitch);
void GMAudio_StopEntity(int entity, int channel);
