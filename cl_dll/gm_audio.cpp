#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "com_model.h"

#include "gm_audio.h"
#include "gm_audio_core.h"
#include "gm_audio_shared.h"

#ifdef ARRAYSIZE
#undef ARRAYSIZE
#endif

#ifdef _WIN32
#define HSPRITE WINDOWS_HSPRITE
#endif
#include <SDL2/SDL.h>
#ifdef _WIN32
#undef HSPRITE
#endif

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

static GMMixer g_Mixer;
static SDL_AudioDeviceID g_Device = 0;
static bool g_SdlAudioStarted = false;

// decoded sounds, by path relative to sound/. nullptr = it failed once (reported once, not on every play)
static std::unordered_map<std::string, std::shared_ptr<GMSoundData>> g_Cache;
static size_t g_CacheBytes = 0;
static const size_t kCacheLimitBytes = 64u * 1024u * 1024u;

namespace
{
// The audio thread runs the mixer: everything that touches g_Mixer from the game thread goes through this.
struct AudioLock
{
	AudioLock()
	{
		if (g_Device)
			SDL_LockAudioDevice(g_Device);
	}
	~AudioLock()
	{
		if (g_Device)
			SDL_UnlockAudioDevice(g_Device);
	}
};
}

static void SDLCALL AudioCallback(void* /*userdata*/, Uint8* stream, int len)
{
	g_Mixer.Mix((float*)stream, len / (int)(sizeof(float) * 2));
}

// ---------------------------------------------------------------------------------------
//  Loading
// ---------------------------------------------------------------------------------------

static std::shared_ptr<const GMSoundData> LoadSound(const char* path)
{
	const std::string key = GMAudio_RelativePath(path); // "x/y.wav" and "sound/x/y.wav" are the same sound

	auto it = g_Cache.find(key);
	if (it != g_Cache.end())
		return it->second;

	std::shared_ptr<GMSoundData> sound;
	const std::string full = GMAudio_FullPath(path);

	int len = 0;
	byte* file = gEngfuncs.COM_LoadFile((char*)full.c_str(), 5, &len); // through the engine filesystem: pak, addons, workshop...

	if (!file)
	{
		gEngfuncs.Con_Printf("GMAudio: can't open %s\n", full.c_str());
	}
	else
	{
		auto decoded = std::make_shared<GMSoundData>();
		std::string error;

		if (GMAudio_DecodeWav(file, (size_t)len, *decoded, error))
			sound = decoded;
		else
			gEngfuncs.Con_Printf("GMAudio: %s: %s\n", full.c_str(), error.c_str());

		gEngfuncs.COM_FreeFile(file);
	}

	if (sound)
	{
		const size_t bytes = sound->samples.size() * sizeof(float);

		if (g_CacheBytes + bytes > kCacheLimitBytes)
		{
			g_Cache.clear(); // sounds that are playing right now keep their own reference
			g_CacheBytes = 0;
		}

		g_CacheBytes += bytes;
	}

	g_Cache[key] = sound;
	return sound;
}

static void PlayRequest(int flags, int entity, int channel, float volume, float attenuation, int pitch,
	const GMVec3& origin, const char* path)
{
	if (!g_Device)
		return;

	if ((flags & GM_SND_STOP) != 0)
	{
		AudioLock lock;
		g_Mixer.Stop(entity, channel);
		return;
	}

	// The server is not trusted: it could send anything
	if (!GMAudio_IsValidSoundPath(path))
	{
		static int reported = 0;
		if (reported++ < 5)
			gEngfuncs.Con_Printf("GMAudio: ignored a sound with an invalid path\n");
		return;
	}

	std::shared_ptr<const GMSoundData> sound = LoadSound(path);
	if (!sound)
		return;

	GMVoiceParams params;
	params.entity = entity;
	params.channel = channel;
	params.volume = volume;
	params.attenuation = attenuation;
	params.pitch = (float)pitch / 100.0f;
	params.origin = origin;

	// the listener's own sounds are heard at full volume in the center, like the engine does
	cl_entity_t* local = gEngfuncs.GetLocalPlayer();
	params.global = (entity > 0 && local && entity == local->index);

	AudioLock lock;
	g_Mixer.Play(sound, params);
}

// ---------------------------------------------------------------------------------------
//  "GMSound" user message (server -> client)
//    BYTE flags | SHORT entity | BYTE channel | BYTE volume (x255) | BYTE attenuation (x64)
//    BYTE pitch | COORD x | COORD y | COORD z | STRING path
// ---------------------------------------------------------------------------------------

static int GMAudio_MsgFunc(const char* /*pszName*/, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	const int flags = READ_BYTE();
	const int entity = READ_SHORT();
	const int channel = READ_BYTE();
	const float volume = (float)READ_BYTE() / 255.0f;
	const float attenuation = (float)READ_BYTE() / 64.0f;
	const int pitch = READ_BYTE();

	GMVec3 origin;
	origin.x = READ_COORD();
	origin.y = READ_COORD();
	origin.z = READ_COORD();

	const std::string path = READ_STRING(); // copied right away: READ_STRING returns a shared buffer

	PlayRequest(flags, entity, channel, volume, attenuation, pitch, origin, path.c_str());
	return 1;
}

// ---------------------------------------------------------------------------------------
//  Console commands
// ---------------------------------------------------------------------------------------

// gm_audio_play <sound.wav> [volume 0-1] [pitch 1-255]  -> test the audio engine without a server
static void GMAudio_CmdPlay()
{
	if (gEngfuncs.Cmd_Argc() < 2)
	{
		gEngfuncs.Con_Printf("usage: gm_audio_play <sound.wav> [volume 0-1] [pitch 1-255]\n");
		return;
	}

	const char* path = gEngfuncs.Cmd_Argv(1);
	const float volume = gEngfuncs.Cmd_Argc() > 2 ? (float)atof(gEngfuncs.Cmd_Argv(2)) : 1.0f;
	const int pitch = gEngfuncs.Cmd_Argc() > 3 ? atoi(gEngfuncs.Cmd_Argv(3)) : 100;

	if (!g_Device)
	{
		gEngfuncs.Con_Printf("GMAudio: no audio device\n");
		return;
	}

	PlayRequest(0, 0, 0, volume, 0.0f, pitch, GMVec3(), path);
}

static void GMAudio_CmdStatus()
{
	int voices;
	{
		AudioLock lock;
		voices = g_Mixer.ActiveVoices();
	}

	gEngfuncs.Con_Printf("GMAudio: device %s, %d Hz, %d/%d voices, %d sounds in cache (%.1f MB)\n",
		g_Device ? "open" : "NOT open", g_Mixer.OutputRate(), voices, GMMixer::kMaxVoices,
		(int)g_Cache.size(), (double)g_CacheBytes / (1024.0 * 1024.0));
}

// ---------------------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------------------

void GMAudio_Init()
{
	if (g_Device)
		return;

	gEngfuncs.pfnHookUserMsg((char*)"GMSound", GMAudio_MsgFunc);
	gEngfuncs.pfnAddCommand((char*)"gm_audio_play", GMAudio_CmdPlay);
	gEngfuncs.pfnAddCommand((char*)"gm_audio_status", GMAudio_CmdStatus);

	// ref-counted by SDL: if the engine already started the audio subsystem this just adds one
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
	{
		gEngfuncs.Con_Printf("GMAudio: SDL audio failed: %s\n", SDL_GetError());
		return;
	}
	g_SdlAudioStarted = true;

	SDL_AudioSpec want, have;
	SDL_zero(want);
	SDL_zero(have);
	want.freq = 44100;
	want.format = AUDIO_F32SYS;
	want.channels = 2;
	want.samples = 1024;
	want.callback = AudioCallback;

	g_Device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

	if (g_Device == 0)
	{
		gEngfuncs.Con_Printf("GMAudio: can't open the audio device: %s\n", SDL_GetError());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		g_SdlAudioStarted = false;
		return;
	}

	g_Mixer.SetOutputRate(have.freq);
	SDL_PauseAudioDevice(g_Device, 0);
}

void GMAudio_Shutdown()
{
	if (g_Device)
	{
		SDL_CloseAudioDevice(g_Device); // when this returns the callback is not running anymore
		g_Device = 0;
	}

	g_Mixer.StopAll();
	g_Cache.clear();
	g_CacheBytes = 0;

	if (g_SdlAudioStarted)
	{
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		g_SdlAudioStarted = false;
	}
}

void GMAudio_PlayLocal(const char* path, float volume, int pitch)
{
	PlayRequest(0, 0, 0, volume, 0.0f, pitch, GMVec3(), path);
}

void GMAudio_StopAll()
{
	AudioLock lock;
	g_Mixer.StopAll();
}

void GMAudio_SetListener(const float* origin, const float* viewangles)
{
	if (!g_Device)
		return;

	float forward[3], right[3], up[3];
	gEngfuncs.pfnAngleVectors(viewangles, forward, right, up);

	cl_entity_t* local = gEngfuncs.GetLocalPlayer();

	// 1) where are the entities that are making a sound right now (the lock is not held while asking the engine)
	std::vector<int> entities;
	{
		AudioLock lock;
		g_Mixer.GetEntities(entities);
	}

	std::vector<std::pair<int, GMVec3>> moved;
	for (int e : entities)
	{
		cl_entity_t* ent = gEngfuncs.GetEntityByIndex(e);

		// Only entities the engine updated this frame, and only studio models: brush entities
		// (doors...) have their origin at 0,0,0, so the position the server sent is the right one.
		if (!ent || !local || !ent->model || ent->model->type != mod_studio ||
			ent->curstate.messagenum != local->curstate.messagenum)
			continue;

		GMVec3 pos;
		pos.x = ent->origin[0];
		pos.y = ent->origin[1];
		pos.z = ent->origin[2];
		moved.emplace_back(e, pos);
	}

	// 2) master volume: the player's own "volume" setting also applies to this
	static cvar_t* pVolume = nullptr;
	if (!pVolume)
		pVolume = gEngfuncs.pfnGetCvarPointer("volume");
	const float master = pVolume ? pVolume->value : 0.8f;

	GMVec3 listener, listenerRight;
	listener.x = origin[0];
	listener.y = origin[1];
	listener.z = origin[2];
	listenerRight.x = right[0];
	listenerRight.y = right[1];
	listenerRight.z = right[2];

	AudioLock lock;
	for (const auto& m : moved)
		g_Mixer.SetEntityOrigin(m.first, m.second);
	g_Mixer.SetListener(listener, listenerRight, master);
}
