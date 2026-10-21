// mmui_swaphook.h
// Go-Mod: Reborn - Main Menu UI: SDL_GL_SwapWindow hook (see mmui_swaphook.cpp)
#pragma once

// sdlWindow is the SDL_Window* the engine is presenting
typedef void (*MMUI_SwapCallback)(void* sdlWindow);

bool MMUI_InstallSwapHook(MMUI_SwapCallback callback);
void MMUI_RemoveSwapHook();
bool MMUI_IsSwapHookInstalled();
