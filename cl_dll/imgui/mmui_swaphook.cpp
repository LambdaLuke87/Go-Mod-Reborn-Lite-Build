// mmui_swaphook.cpp
// Go-Mod: Reborn - Main Menu UI
//
// Hooks SDL_GL_SwapWindow so we can draw Dear ImGui on top of EVERYTHING the engine
// draws (world, HUD, GameUI / main menu, pause menu, console...).
//
// Method: the first bytes of SDL_GL_SwapWindow are replaced with a jump to our function.
// When we want to call the real function we put the original bytes back, call it and
// write back whatever was there before (our jump, or another hook placed on top of ours).
// Rendering only happens on the main thread, so this is safe.

#include <cstring>
#include <cstdint>
#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define MMUI_CDECL __cdecl
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#define MMUI_CDECL
#endif

#include "mmui_swaphook.h"

typedef void(MMUI_CDECL* SwapWindowFn)(void* window);

static constexpr size_t kSavedBytes = 16; // we restore/save 16 bytes, enough for our patch and most foreign ones

static unsigned char* s_pTarget = nullptr;
static unsigned char s_Original[kSavedBytes];
static unsigned char s_Patch[kSavedBytes];
static size_t s_PatchLen = 0;
static bool s_bInstalled = false;
static bool s_bInCallback = false;
static MMUI_SwapCallback s_pCallback = nullptr;

static void FlushCode(void* addr, size_t len)
{
#ifdef _WIN32
	FlushInstructionCache(GetCurrentProcess(), addr, len);
#else
	(void)addr;
	(void)len; // x86 keeps instruction cache coherent
#endif
}

static bool MakeWritable(void* addr, size_t len)
{
#ifdef _WIN32
	DWORD oldProtect = 0;
	return VirtualProtect(addr, len, PAGE_EXECUTE_READWRITE, &oldProtect) != FALSE;
#else
	const long pageSize = sysconf(_SC_PAGESIZE);
	uintptr_t start = (uintptr_t)addr & ~(uintptr_t)(pageSize - 1);
	uintptr_t end = ((uintptr_t)addr + len + pageSize - 1) & ~(uintptr_t)(pageSize - 1);
	return mprotect((void*)start, end - start, PROT_READ | PROT_WRITE | PROT_EXEC) == 0;
#endif
}

static void* FindSwapWindow()
{
#ifdef _WIN32
	HMODULE hSDL = GetModuleHandleA("SDL2.dll");
	if (!hSDL)
		hSDL = GetModuleHandleA("SDL2");
	if (!hSDL)
		return nullptr;
	return (void*)GetProcAddress(hSDL, "SDL_GL_SwapWindow");
#else
	void* p = dlsym(RTLD_DEFAULT, "SDL_GL_SwapWindow");
	if (!p)
	{
		void* lib = dlopen("libSDL2-2.0.so.0", RTLD_NOW | RTLD_NOLOAD);
		if (lib)
		{
			p = dlsym(lib, "SDL_GL_SwapWindow");
			dlclose(lib);
		}
	}
	return p;
#endif
}

static void MMUI_CDECL Hooked_SwapWindow(void* window)
{
	if (s_pCallback && !s_bInCallback)
	{
		s_bInCallback = true;
		s_pCallback(window);
		s_bInCallback = false;
	}

	// Whatever is at the entry right now (normally our jump, maybe another hook on top of ours)
	unsigned char current[kSavedBytes];
	memcpy(current, s_pTarget, kSavedBytes);

	memcpy(s_pTarget, s_Original, kSavedBytes);
	FlushCode(s_pTarget, kSavedBytes);

	((SwapWindowFn)s_pTarget)(window);

	if (s_bInstalled)
	{
		memcpy(s_pTarget, current, kSavedBytes);
		FlushCode(s_pTarget, kSavedBytes);
	}
}

bool MMUI_InstallSwapHook(MMUI_SwapCallback callback)
{
	s_pCallback = callback;

	if (s_bInstalled)
		return true;

	s_pTarget = (unsigned char*)FindSwapWindow();
	if (!s_pTarget)
	{
		printf("MMUI: SDL_GL_SwapWindow not found, main menu UI disabled\n");
		return false;
	}

	if (!MakeWritable(s_pTarget, kSavedBytes))
	{
		printf("MMUI: could not unprotect SDL_GL_SwapWindow, main menu UI disabled\n");
		s_pTarget = nullptr;
		return false;
	}

	memcpy(s_Original, s_pTarget, kSavedBytes);
	memcpy(s_Patch, s_Original, kSavedBytes);

	const uintptr_t hook = (uintptr_t)&Hooked_SwapWindow;

#if defined(_WIN64) || defined(__x86_64__)
	// mov rax, imm64 ; jmp rax
	s_Patch[0] = 0x48;
	s_Patch[1] = 0xB8;
	memcpy(&s_Patch[2], &hook, 8);
	s_Patch[10] = 0xFF;
	s_Patch[11] = 0xE0;
	s_PatchLen = 12;
#else
	// jmp rel32
	const int32_t rel = (int32_t)(hook - ((uintptr_t)s_pTarget + 5));
	s_Patch[0] = 0xE9;
	memcpy(&s_Patch[1], &rel, 4);
	s_PatchLen = 5;
#endif

	memcpy(s_pTarget, s_Patch, kSavedBytes);
	FlushCode(s_pTarget, kSavedBytes);

	s_bInstalled = true;
	return true;
}

void MMUI_RemoveSwapHook()
{
	if (!s_bInstalled || !s_pTarget)
		return;

	// called from HUD_Shutdown: client.dll is about to be unloaded, the jump MUST go away
	memcpy(s_pTarget, s_Original, kSavedBytes);
	FlushCode(s_pTarget, kSavedBytes);

	s_bInstalled = false;
	s_pCallback = nullptr;
}

bool MMUI_IsSwapHookInstalled()
{
	return s_bInstalled;
}
