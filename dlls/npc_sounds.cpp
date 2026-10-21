#include "extdll.h"
#include "util.h"
#include "cbase.h"

#include "npc_sounds.h"
#include "gm_audio_server.h"
#include "gm_audio_shared.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "../external/rapidjson/document.h"

namespace
{
struct SoundGroup
{
	std::vector<std::string> sounds;
	float volume = 1.0f;
	float pitch = 1.0f;
};

struct SoundConfig
{
	std::unordered_map<std::string, SoundGroup> groups;
};

std::unordered_map<std::string, SoundConfig> g_Configs;
std::unordered_map<std::string, bool> g_ReportedMissing; // "config/group" already reported this map
}

static bool LoadTextFile(const char* path, std::string& out)
{
	int len = 0;
	byte* data = LOAD_FILE_FOR_ME((char*)path, &len);
	if (!data)
		return false;

	out.assign((const char*)data, (size_t)len);
	FREE_FILE(data);
	return true;
}

static void ReadSounds(const rapidjson::Value& list, SoundGroup& group, const char* file, const char* groupName)
{
	if (!list.IsArray())
		return;

	for (const auto& entry : list.GetArray())
	{
		if (!entry.IsString())
			continue;

		const char* path = entry.GetString();
		if (!GMAudio_IsValidSoundPath(path))
		{
			ALERT(at_console, "NPCSounds: %s: invalid sound '%s' in \"%s\"\n", file, path, groupName);
			continue;
		}

		group.sounds.emplace_back(path);
	}
}

static const SoundConfig& LoadConfig(const char* config)
{
	auto it = g_Configs.find(config);
	if (it != g_Configs.end())
		return it->second;

	SoundConfig& cfg = g_Configs[config]; // cached even if it fails: reported once per map

	char file[256];
	snprintf(file, sizeof(file), "sound_configs/%s.json", config);

	std::string text;
	if (!LoadTextFile(file, text))
	{
		ALERT(at_console, "NPCSounds: %s not found\n", file);
		return cfg;
	}

	rapidjson::Document doc;
	if (doc.Parse(text.c_str()).HasParseError() || !doc.IsObject())
	{
		ALERT(at_console, "NPCSounds: invalid JSON in %s\n", file);
		return cfg;
	}

	for (auto m = doc.MemberBegin(); m != doc.MemberEnd(); ++m)
	{
		const char* name = m->name.GetString();
		SoundGroup group;

		if (m->value.IsArray())
		{
			ReadSounds(m->value, group, file, name);
		}
		else if (m->value.IsObject())
		{
			if (m->value.HasMember("sounds"))
				ReadSounds(m->value["sounds"], group, file, name);
			if (m->value.HasMember("volume") && m->value["volume"].IsNumber())
				group.volume = m->value["volume"].GetFloat();
			if (m->value.HasMember("pitch") && m->value["pitch"].IsNumber())
				group.pitch = m->value["pitch"].GetFloat();
		}
		else
		{
			continue; // comments like "_info": "..." are allowed
		}

		cfg.groups[name] = std::move(group);
	}

	return cfg;
}

static const SoundGroup* FindGroup(const char* config, const char* group)
{
	if (!config || !group)
		return nullptr;

	const SoundConfig& cfg = LoadConfig(config);

	auto it = cfg.groups.find(group);
	if (it == cfg.groups.end() || it->second.sounds.empty())
	{
		// report each missing group once per map
		const std::string key = std::string(config) + "/" + group;
		if (!g_ReportedMissing[key])
		{
			g_ReportedMissing[key] = true;
			ALERT(at_console, "NPCSounds: group \"%s\" missing or empty in sound_configs/%s.json\n", group, config);
		}
		return nullptr;
	}

	return &it->second;
}

static void EmitFromGroup(CBaseEntity* pEntity, const SoundGroup& g, int index, int channel, float volume, float attenuation, int pitch)
{
	const float vol = volume * g.volume;
	const int pit = (int)(pitch * g.pitch + 0.5f);
	GMAudio_Emit(pEntity, channel, g.sounds[index].c_str(), vol > 1.0f ? 1.0f : vol, attenuation, pit);
}

bool NPCSounds_Emit(CBaseEntity* pEntity, const char* config, const char* group, int channel, float volume, float attenuation, int pitch)
{
	const SoundGroup* g = FindGroup(config, group);
	if (!g || !pEntity)
		return false;

	EmitFromGroup(pEntity, *g, RANDOM_LONG(0, (int)g->sounds.size() - 1), channel, volume, attenuation, pitch);
	return true;
}

bool NPCSounds_EmitIndex(CBaseEntity* pEntity, const char* config, const char* group, int index, int channel, float volume, float attenuation, int pitch)
{
	const SoundGroup* g = FindGroup(config, group);
	if (!g || !pEntity)
		return false;

	if (index < 0 || index >= (int)g->sounds.size())
		index = RANDOM_LONG(0, (int)g->sounds.size() - 1);

	EmitFromGroup(pEntity, *g, index, channel, volume, attenuation, pitch);
	return true;
}

bool NPCSounds_EmitAmbient(const Vector& origin, const char* config, const char* group, float volume, float attenuation, int pitch)
{
	const SoundGroup* g = FindGroup(config, group);
	if (!g)
		return false;

	const float vol = volume * g->volume;
	GMAudio_EmitAmbient(origin, g->sounds[RANDOM_LONG(0, (int)g->sounds.size() - 1)].c_str(),
		vol > 1.0f ? 1.0f : vol, attenuation, (int)(pitch * g->pitch + 0.5f));
	return true;
}

int NPCSounds_Count(const char* config, const char* group)
{
	const SoundGroup* g = FindGroup(config, group);
	return g ? (int)g->sounds.size() : 0;
}

const char* NPCSounds_Random(const char* config, const char* group)
{
	const SoundGroup* g = FindGroup(config, group);
	if (!g)
		return nullptr;
	return g->sounds[RANDOM_LONG(0, (int)g->sounds.size() - 1)].c_str();
}

const char* NPCSounds_Get(const char* config, const char* group, int index)
{
	const SoundGroup* g = FindGroup(config, group);
	if (!g || index < 0 || index >= (int)g->sounds.size())
		return nullptr;
	return g->sounds[index].c_str();
}

void NPCSounds_ClearCache()
{
	g_Configs.clear();
	g_ReportedMissing.clear();
}
