#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "com_model.h"
#include "event_api.h"
#include "pm_defs.h"

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

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
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

// ---------------------------------------------------------------------------------------
//  Sentences (sound/sentences.txt): "!NAME" plays the words one after the other
//    NAME dir/word1 word2(p110 v80) word3, word4.
//    - the directory of the first word is used by the next words (default "vox/")
//    - "(...)" alone (or right after "dir/") changes the defaults for the rest of the sentence
//    - "word(...)" changes only that word.  v = volume %, p = pitch %, s = start %, e = end %
//    - ',' and '.' are short pauses
// ---------------------------------------------------------------------------------------

struct SentenceWord
{
	std::string path;	// relative to sound/, empty = pause
	float pause = 0.0f; // seconds, for ',' and '.'
	int volume = 100;
	int pitch = 100;
	int start = 0;
	int end = 100;
};

static std::unordered_map<std::string, std::vector<SentenceWord>> g_Sentences; // key: NAME in upper case
static bool g_SentencesLoaded = false;

static std::string UpperCase(const std::string& s)
{
	std::string r = s;
	for (char& c : r)
		c = (char)toupper((unsigned char)c);
	return r;
}

struct SentenceMods
{
	int volume = 100, pitch = 100, start = 0, end = 100;
};

static void ParseMods(const std::string& text, SentenceMods& mods)
{
	size_t i = 0;
	while (i < text.size())
	{
		const char key = (char)tolower((unsigned char)text[i]);
		i++;

		if (key != 'v' && key != 'p' && key != 's' && key != 'e' && key != 't')
			continue;

		size_t j = i;
		while (j < text.size() && isdigit((unsigned char)text[j]))
			j++;

		if (j == i)
			continue;

		const int value = atoi(text.substr(i, j - i).c_str());
		i = j;

		switch (key)
		{
		case 'v': mods.volume = value; break;
		case 'p': mods.pitch = value; break;
		case 's': mods.start = value; break;
		case 'e': mods.end = value; break;
		default: break; // 't' (time compression) is not supported, ignored
		}
	}
}

static void ParseSentence(const std::string& text, std::vector<SentenceWord>& out)
{
	std::string dir = "vox/";
	SentenceMods defaults;
	bool first = true;

	const size_t n = text.size();
	size_t i = 0;

	while (i < n)
	{
		while (i < n && isspace((unsigned char)text[i]))
			i++;
		if (i >= n)
			break;

		// a token ends at a space that is not inside (...)
		std::string token;
		int depth = 0;
		while (i < n && (depth > 0 || !isspace((unsigned char)text[i])))
		{
			if (text[i] == '(')
				depth++;
			else if (text[i] == ')' && depth > 0)
				depth--;
			token += text[i++];
		}

		// split "word(mods)punct"
		std::string word, mods, tail;
		const size_t open = token.find('(');
		if (open != std::string::npos)
		{
			const size_t close = token.find(')', open);
			word = token.substr(0, open);
			mods = token.substr(open + 1, (close == std::string::npos ? token.size() : close) - open - 1);
			if (close != std::string::npos)
				tail = token.substr(close + 1);
		}
		else
		{
			word = token;
		}

		float pauseAfter = 0.0f;
		auto takePunct = [&](std::string& w)
		{
			while (!w.empty() && (w.back() == ',' || w.back() == '.'))
			{
				pauseAfter += (w.back() == ',') ? 0.25f : 0.5f;
				w.pop_back();
			}
		};
		takePunct(word);
		takePunct(tail);

		if (first && word.find('/') != std::string::npos)
		{
			const size_t slash = word.rfind('/');
			dir = word.substr(0, slash + 1);
			word = word.substr(slash + 1);
		}
		first = false;

		if (word.empty())
		{
			if (!mods.empty())
				ParseMods(mods, defaults); // "(p110)" alone: defaults for the rest
		}
		else
		{
			SentenceMods m = defaults;
			if (!mods.empty())
				ParseMods(mods, m);

			SentenceWord w;
			w.path = (word.find('/') != std::string::npos ? word : dir + word) + ".wav";
			w.volume = m.volume;
			w.pitch = m.pitch;
			w.start = m.start;
			w.end = m.end;
			out.push_back(w);
		}

		if (pauseAfter > 0.0f)
		{
			SentenceWord p;
			p.pause = pauseAfter;
			out.push_back(p);
		}
	}
}

static void LoadSentences()
{
	g_Sentences.clear();
	g_SentencesLoaded = true;

	int len = 0;
	byte* file = gEngfuncs.COM_LoadFile((char*)"sound/sentences.txt", 5, &len);
	if (!file)
	{
		gEngfuncs.Con_Printf("GMAudio: sound/sentences.txt not found\n");
		return;
	}

	const std::string text((const char*)file, (size_t)len);
	gEngfuncs.COM_FreeFile(file);

	size_t pos = 0;
	while (pos < text.size())
	{
		size_t eol = text.find('\n', pos);
		if (eol == std::string::npos)
			eol = text.size();

		std::string line = text.substr(pos, eol - pos);
		pos = eol + 1;

		const size_t comment = line.find("//");
		if (comment != std::string::npos)
			line = line.substr(0, comment);

		size_t a = 0;
		while (a < line.size() && isspace((unsigned char)line[a]))
			a++;
		size_t b = a;
		while (b < line.size() && !isspace((unsigned char)line[b]))
			b++;

		if (b <= a)
			continue;

		const std::string name = UpperCase(line.substr(a, b - a));

		std::vector<SentenceWord> words;
		ParseSentence(line.substr(b), words);

		if (!words.empty())
			g_Sentences[name] = std::move(words);
	}
}

static std::shared_ptr<const GMSoundData> Silence(float seconds)
{
	auto data = std::make_shared<GMSoundData>();
	data->rate = 22050;
	data->samples.assign((size_t)(seconds * 22050.0f) + 1, 0.0f);
	return data;
}

// "!NAME" -> the segments to play. false if the sentence doesn't exist
static bool BuildSentence(const char* name, std::vector<GMSegment>& segments)
{
	if (!g_SentencesLoaded)
		LoadSentences();

	auto it = g_Sentences.find(UpperCase(name + 1));
	if (it == g_Sentences.end())
	{
		static int reported = 0;
		if (reported++ < 10)
			gEngfuncs.Con_Printf("GMAudio: sentence %s not found in sound/sentences.txt\n", name);
		return false;
	}

	for (const SentenceWord& w : it->second)
	{
		GMSegment seg;

		if (w.path.empty())
		{
			seg.sound = Silence(w.pause);
		}
		else
		{
			if (!GMAudio_IsValidSoundPath(w.path.c_str()))
				continue;

			seg.sound = LoadSound(w.path.c_str());
			if (!seg.sound)
				continue;

			seg.volume = (float)w.volume / 100.0f;
			seg.pitch = (float)w.pitch / 100.0f;
			seg.start = (float)w.start / 100.0f;
			seg.end = (float)w.end / 100.0f;
		}

		segments.push_back(std::move(seg));
	}

	return !segments.empty();
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

	std::vector<GMSegment> segments;

	if (path[0] == '!')
	{
		if (!BuildSentence(path, segments))
			return;
	}
	else
	{
		GMSegment seg;
		seg.sound = LoadSound(path);
		if (!seg.sound)
			return;
		segments.push_back(std::move(seg));
	}

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
	g_Mixer.Play(segments, params);
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
//  Effects (reverb, occlusion, doppler, spatial...). Only for the sounds of this system.
//
//  The cvars are NOT archived on purpose: every time the game starts everything is normal again.
//  "gm_audio_fx_reset" puts everything back to normal, "gm_audio_fx" shows what is on.
// ---------------------------------------------------------------------------------------

struct FxCvar
{
	const char* name;
	const char* def;
	const char* help;
	cvar_t* cvar;
};

static FxCvar g_FxCvars[] = {
	{"gm_audio_reverb", "0", "room reverb (follows env_sound / room_type)", nullptr},
	{"gm_audio_reverb_room", "0", "force a room 1-28 (0 = the one of the map)", nullptr},
	{"gm_audio_reverb_mix", "1", "reverb amount multiplier (0-2)", nullptr},
	{"gm_audio_occlusion", "0", "walls between you and the sound muffle it", nullptr},
	{"gm_audio_occlusion_strength", "1", "how much the walls muffle (0-1)", nullptr},
	{"gm_audio_doppler", "0", "pitch changes with movement (doppler)", nullptr},
	{"gm_audio_doppler_scale", "1", "doppler exaggeration (0.1-4)", nullptr},
	{"gm_audio_spatial", "0", "binaural approximation (time difference + head shadow), best with headphones", nullptr},
	{"gm_audio_air", "0", "air absorption: far sounds lose high frequencies", nullptr},
	{"gm_audio_underwater", "0", "muffle the sounds when you are underwater", nullptr},
	{"gm_audio_interp", "0", "resampling: 0 linear, 1 cubic (better quality)", nullptr},
	{"gm_audio_vibrato", "0", "the pitch goes up and down by itself (YTP style)", nullptr},
	{"gm_audio_vibrato_rate", "6", "vibrato oscillations per second (0.1-30)", nullptr},
	{"gm_audio_vibrato_depth", "0.3", "vibrato amount, 0.3 = +-30% pitch (0-0.9)", nullptr},
};

static const int kFxCvarCount = (int)(sizeof(g_FxCvars) / sizeof(g_FxCvars[0]));

static float FxValue(int i)
{
	return g_FxCvars[i].cvar ? g_FxCvars[i].cvar->value : (float)atof(g_FxCvars[i].def);
}

enum
{
	FX_REVERB = 0,
	FX_REVERB_ROOM,
	FX_REVERB_MIX,
	FX_OCCLUSION,
	FX_OCCLUSION_STRENGTH,
	FX_DOPPLER,
	FX_DOPPLER_SCALE,
	FX_SPATIAL,
	FX_AIR,
	FX_UNDERWATER,
	FX_INTERP,
	FX_VIBRATO,
	FX_VIBRATO_RATE,
	FX_VIBRATO_DEPTH,
};

static void GMAudio_CmdFxReset()
{
	for (int i = 0; i < kFxCvarCount; i++)
		gEngfuncs.Cvar_Set((char*)g_FxCvars[i].name, (char*)g_FxCvars[i].def);

	gEngfuncs.Con_Printf("GMAudio: all audio effects back to normal\n");
}

static void GMAudio_CmdFxAll()
{
	GMAudio_CmdFxReset();
	gEngfuncs.Cvar_Set((char*)"gm_audio_reverb", (char*)"1");
	gEngfuncs.Cvar_Set((char*)"gm_audio_occlusion", (char*)"1");
	gEngfuncs.Cvar_Set((char*)"gm_audio_doppler", (char*)"1");
	gEngfuncs.Cvar_Set((char*)"gm_audio_spatial", (char*)"1");
	gEngfuncs.Cvar_Set((char*)"gm_audio_air", (char*)"1");
	gEngfuncs.Cvar_Set((char*)"gm_audio_underwater", (char*)"1");
	gEngfuncs.Cvar_Set((char*)"gm_audio_interp", (char*)"1");
	gEngfuncs.Con_Printf("GMAudio: all audio effects ON except the vibrato (until the game is closed or gm_audio_fx_reset)\n");
}

static int CurrentRoomType(bool underwater); // below

static void GMAudio_CmdFx()
{
	gEngfuncs.Con_Printf("GMAudio effects (not saved, normal again every time the game starts):\n");

	for (int i = 0; i < kFxCvarCount; i++)
	{
		const bool changed = g_FxCvars[i].cvar && strcmp(g_FxCvars[i].cvar->string, g_FxCvars[i].def) != 0;
		gEngfuncs.Con_Printf("  %-28s %-6s %s%s\n", g_FxCvars[i].name, g_FxCvars[i].cvar ? g_FxCvars[i].cvar->string : "?",
			g_FxCvars[i].help, changed ? "  <- CHANGED" : "");
	}

	extern int g_iWaterLevel;
	const int room = CurrentRoomType(g_iWaterLevel >= 3);
	gEngfuncs.Con_Printf("  room now: %d (%s)\n", room, GMAudio_RoomPreset(room).name);
	gEngfuncs.Con_Printf("gm_audio_fx_reset = everything normal, gm_audio_fx_all = everything on\n");
}

static void RegisterFx()
{
	for (int i = 0; i < kFxCvarCount; i++)
		g_FxCvars[i].cvar = gEngfuncs.pfnRegisterVariable((char*)g_FxCvars[i].name, (char*)g_FxCvars[i].def, 0); // 0 = NOT archived

	gEngfuncs.pfnAddCommand((char*)"gm_audio_fx", GMAudio_CmdFx);
	gEngfuncs.pfnAddCommand((char*)"gm_audio_fx_reset", GMAudio_CmdFxReset);
	gEngfuncs.pfnAddCommand((char*)"gm_audio_fx_all", GMAudio_CmdFxAll);
}

// the engine keeps the env_sound room in "room_type" and the underwater one in "waterroom_type"
static int CurrentRoomType(bool underwater)
{
	const int forced = (int)FxValue(FX_REVERB_ROOM);
	if (forced > 0)
		return forced;

	return (int)gEngfuncs.pfnGetCvarFloat(underwater ? "waterroom_type" : "room_type");
}

// per playing voice: occlusion / doppler state (game thread only)
struct SourceFx
{
	GMVec3 lastOrigin;
	bool hasLast = false;
	float occlusion = 0.0f;
	float nextTrace = 0.0f;
	float doppler = 1.0f;
	bool seen = false;
};

static std::unordered_map<unsigned long long, SourceFx> g_SourceFx;
static GMVec3 g_LastListener;
static bool g_HasLastListener = false;
static float g_LastFxTime = 0.0f;

static bool TraceBlocked(const float* start, const float* end)
{
	pmtrace_t tr;
	gEngfuncs.pEventAPI->EV_SetTraceHull(2); // point
	gEngfuncs.pEventAPI->EV_PlayerTrace((float*)start, (float*)end, PM_WORLD_ONLY, -1, &tr);
	return tr.fraction < 0.98f && !tr.allsolid;
}

// 0 = clear, 1 = blocked. 3 rays (center, above, to the side) so a thin pillar doesn't mute everything
static float ComputeOcclusion(const GMVec3& listener, const float* right, const GMVec3& source)
{
	const float start[3] = {listener.x, listener.y, listener.z};
	const float offsets[3][3] = {
		{0.0f, 0.0f, 16.0f},
		{0.0f, 0.0f, 40.0f},
		{right[0] * 24.0f, right[1] * 24.0f, 16.0f + right[2] * 24.0f},
	};

	int blocked = 0;
	for (const auto& o : offsets)
	{
		const float end[3] = {source.x + o[0], source.y + o[1], source.z + o[2]};
		if (TraceBlocked(start, end))
			blocked++;
	}

	return (float)blocked / 3.0f;
}

// game thread, once per frame: effects config + occlusion / doppler of every voice
static void UpdateEffects(const GMVec3& listener, const float* right)
{
	extern int g_iWaterLevel;

	const bool underwater = g_iWaterLevel >= 3;

	GMEffects fx;
	fx.reverb = FxValue(FX_REVERB) != 0.0f;
	fx.roomType = CurrentRoomType(underwater);
	fx.reverbMix = std::min(2.0f, std::max(0.0f, FxValue(FX_REVERB_MIX)));
	fx.spatial = FxValue(FX_SPATIAL) != 0.0f;
	fx.airAbsorption = FxValue(FX_AIR) != 0.0f;
	fx.underwater = FxValue(FX_UNDERWATER) != 0.0f && underwater;
	fx.interpolation = FxValue(FX_INTERP) != 0.0f ? 1 : 0;
	fx.vibrato = FxValue(FX_VIBRATO) != 0.0f;
	fx.vibratoRate = FxValue(FX_VIBRATO_RATE);
	fx.vibratoDepth = FxValue(FX_VIBRATO_DEPTH);

	const bool occlusion = FxValue(FX_OCCLUSION) != 0.0f;
	const bool doppler = FxValue(FX_DOPPLER) != 0.0f;

	const float now = gEngfuncs.GetClientTime();
	float dt = now - g_LastFxTime;
	g_LastFxTime = now;
	if (dt <= 0.0f || dt > 0.5f)
		dt = 0.0f; // paused / new map: no velocity this frame

	std::vector<GMVoiceSource> sources;
	if (occlusion || doppler)
	{
		AudioLock lock;
		g_Mixer.SetEffects(fx);
		g_Mixer.GetSources(sources);
	}
	else
	{
		AudioLock lock;
		g_Mixer.SetEffects(fx);
	}

	// listener velocity
	GMVec3 listenerVel;
	if (g_HasLastListener && dt > 0.0f)
	{
		listenerVel.x = (listener.x - g_LastListener.x) / dt;
		listenerVel.y = (listener.y - g_LastListener.y) / dt;
		listenerVel.z = (listener.z - g_LastListener.z) / dt;
	}
	g_LastListener = listener;
	g_HasLastListener = true;

	// respawn / teleport: not a real movement
	if (std::sqrt(listenerVel.x * listenerVel.x + listenerVel.y * listenerVel.y + listenerVel.z * listenerVel.z) > 4000.0f)
		listenerVel = GMVec3();

	if (sources.empty())
	{
		g_SourceFx.clear();
		return;
	}

	// traces need a map
	const char* level = gEngfuncs.pfnGetLevelName();
	const bool canTrace = occlusion && level && level[0] && gEngfuncs.GetLocalPlayer();

	const float strength = std::min(1.0f, std::max(0.0f, FxValue(FX_OCCLUSION_STRENGTH)));
	const float dopplerScale = std::min(4.0f, std::max(0.1f, FxValue(FX_DOPPLER_SCALE)));
	const float speedOfSound = 13500.0f; // units per second (~343 m/s)

	for (auto& e : g_SourceFx)
		e.second.seen = false;

	std::vector<std::pair<unsigned long long, std::pair<float, float>>> results;
	results.reserve(sources.size());

	int tracesLeft = 48; // budget per frame, the rest wait for the next frame

	for (const GMVoiceSource& src : sources)
	{
		SourceFx& st = g_SourceFx[src.serial];
		st.seen = true;

		if (src.global)
		{
			results.push_back({src.serial, {0.0f, 1.0f}});
			continue;
		}

		// occlusion: re-traced every 0.1 s
		if (canTrace && now >= st.nextTrace && tracesLeft >= 3)
		{
			st.occlusion = ComputeOcclusion(listener, right, src.origin) * strength;
			st.nextTrace = now + 0.1f;
			tracesLeft -= 3;
		}
		else if (!occlusion)
		{
			st.occlusion = 0.0f;
		}

		// doppler
		float pitch = 1.0f;
		if (doppler && dt > 0.0f && st.hasLast)
		{
			const float vx = (src.origin.x - st.lastOrigin.x) / dt;
			const float vy = (src.origin.y - st.lastOrigin.y) / dt;
			float vz = (src.origin.z - st.lastOrigin.z) / dt;
			float vxs = vx, vys = vy;

			// the origin of a voice gets corrected / teleported sometimes: not a real movement
			if (std::sqrt(vx * vx + vy * vy + vz * vz) > 4000.0f)
				vxs = vys = vz = 0.0f;

			float dx = listener.x - src.origin.x;
			float dy = listener.y - src.origin.y;
			float dz = listener.z - src.origin.z;
			const float len = std::sqrt(dx * dx + dy * dy + dz * dz);

			if (len > 1.0f)
			{
				dx /= len;
				dy /= len;
				dz /= len;

				const float c = speedOfSound / dopplerScale;
				const float vs = std::min(c * 0.5f, vxs * dx + vys * dy + vz * dz);										  // source towards the listener
				const float vl = std::min(c * 0.5f, -(listenerVel.x * dx + listenerVel.y * dy + listenerVel.z * dz)); // listener towards the source
				pitch = (c + vl) / (c - vs);
			}
		}

		st.lastOrigin = src.origin;
		st.hasLast = true;

		// teleports / new origins: ignore absurd jumps
		if (pitch < 0.5f || pitch > 2.0f)
			pitch = 1.0f;

		st.doppler = pitch;
		results.push_back({src.serial, {st.occlusion, st.doppler}});
	}

	// forget voices that are gone
	for (auto it = g_SourceFx.begin(); it != g_SourceFx.end();)
	{
		if (!it->second.seen)
			it = g_SourceFx.erase(it);
		else
			++it;
	}

	AudioLock lock;
	for (const auto& r : results)
		g_Mixer.SetVoiceFx(r.first, r.second.first, r.second.second);
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
	RegisterFx();

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

void GMAudio_PlayEntity(int entity, const float* origin, int channel, const char* path, float volume, float attenuation, int pitch)
{
	GMVec3 pos;
	if (origin)
	{
		pos.x = origin[0];
		pos.y = origin[1];
		pos.z = origin[2];
	}
	else if (cl_entity_t* ent = gEngfuncs.GetEntityByIndex(entity))
	{
		pos.x = ent->origin[0];
		pos.y = ent->origin[1];
		pos.z = ent->origin[2];
	}

	PlayRequest(0, entity, channel, volume, attenuation, pitch, pos, path);
}

void GMAudio_StopEntity(int entity, int channel)
{
	PlayRequest(GM_SND_STOP, entity, channel, 0.0f, 0.0f, 100, GMVec3(), "stop.wav");
}

void GMAudio_StopAll()
{
	{
		AudioLock lock;
		g_Mixer.StopAll();
	}

	g_SentencesLoaded = false; // read sentences.txt again on the next map (the mod may have changed it)
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

	GMVec3 listenerForward;
	listenerForward.x = forward[0];
	listenerForward.y = forward[1];
	listenerForward.z = forward[2];

	std::vector<std::pair<int, float>> voices;
	{
		AudioLock lock;
		for (const auto& m : moved)
			g_Mixer.SetEntityOrigin(m.first, m.second);
		g_Mixer.SetListener(listener, listenerRight, master);
		g_Mixer.SetListenerForward(listenerForward);
		g_Mixer.GetVoiceLevels(voices);
	}

	// effects: reverb / occlusion / doppler...
	UpdateEffects(listener, right);

	// 3) mouths: the engine only moves them for its own sounds, so do it for ours (CHAN_VOICE)
	static std::vector<int> s_OpenMouths;

	for (int e : s_OpenMouths)
	{
		bool stillTalking = false;
		for (const auto& v : voices)
		{
			if (v.first == e)
			{
				stillTalking = true;
				break;
			}
		}

		if (!stillTalking)
		{
			if (cl_entity_t* ent = gEngfuncs.GetEntityByIndex(e))
				ent->mouth.mouthopen = 0;
		}
	}

	s_OpenMouths.clear();

	for (const auto& v : voices)
	{
		cl_entity_t* ent = gEngfuncs.GetEntityByIndex(v.first);
		if (!ent)
			continue;

		// the renderer opens the mouth fully at 64
		const int open = (int)(v.second * 600.0f);
		ent->mouth.mouthopen = (byte)(open > 255 ? 255 : open);
		s_OpenMouths.push_back(v.first);
	}
}
