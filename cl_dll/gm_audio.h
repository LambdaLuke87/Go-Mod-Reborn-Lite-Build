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
