#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "custom_sounds.h"

#include "../external/rapidjson/document.h"

static std::vector<SoundGroupDef> g_CustomSounds;

static bool LoadTextFileSafe(const char* path, std::string& out)
{
	int len = 0;
	byte* data = LOAD_FILE_FOR_ME((char*)path, &len);

	if (!data)
		return false;

	out.assign((const char*)data, (size_t)len);
	FREE_FILE(data);
	return true;
}

static bool IsValidSoundType(const std::string& type)
{
	return type == "alert" || type == "idle" || type == "pain" || type == "death" || type == "attack";
}

static bool ParseSoundFile(const std::string& path, SoundGroupDef& outDef)
{
	std::string src;
	if (!LoadTextFileSafe(path.c_str(), src))
	{
		ALERT(at_console, "CustomSounds: it could not be opened %s\n", path.c_str());
		return false;
	}

	rapidjson::Document doc;
	if (doc.Parse(src.c_str()).HasParseError() || !doc.IsObject())
	{
		ALERT(at_console, "CustomSounds: invalid JSON in %s\n", path.c_str());
		return false;
	}

	if (!doc.HasMember("id") || !doc["id"].IsString())
	{
		ALERT(at_console, "CustomSounds: missing \"id\" in %s\n", path.c_str());
		return false;
	}
	outDef.id = doc["id"].GetString();

	if (!doc.HasMember("type") || !doc["type"].IsString())
	{
		ALERT(at_console, "CustomSounds: missing \"type\" in %s\n", path.c_str());
		return false;
	}
	outDef.type = doc["type"].GetString();

	if (!IsValidSoundType(outDef.type))
	{
		ALERT(at_console, "CustomSounds: \"type\" invalid ('%s') in %s (valid values: alert, idle, pain, death, attack)\n",
			outDef.type.c_str(), path.c_str());
		return false;
	}

	if (!doc.HasMember("sound_list") || !doc["sound_list"].IsArray() || doc["sound_list"].Empty())
	{
		ALERT(at_console, "CustomSounds: \"sound_list\" empty or missing in %s\n", path.c_str());
		return false;
	}

	for (auto& s : doc["sound_list"].GetArray())
	{
		if (s.IsString())
			outDef.sounds.push_back(s.GetString());
	}

	if (outDef.sounds.empty())
	{
		ALERT(at_console, "CustomSounds: \"sound_list\" no have valid strings in %s\n", path.c_str());
		return false;
	}

	if (doc.HasMember("volume") && doc["volume"].IsNumber())
	{
		float v = doc["volume"].GetFloat();
		if (v < 0.0f || v > 1.0f)
			ALERT(at_console, "CustomSounds: \"volume\" out of range in %s (usa 0.0-1.0), its used 1.0\n", path.c_str());
		else
			outDef.volume = v;
	}

	if (doc.HasMember("pitch") && doc["pitch"].IsInt())
	{
		int v = doc["pitch"].GetInt();
		if (v < 1 || v > 255)
			ALERT(at_console, "CustomSounds: \"pitch\" out of range in %s (usa 1-255), its used 100\n", path.c_str());
		else
			outDef.pitch = v;
	}

	return true;
}

void CustomSounds_Precache()
{
	g_CustomSounds.clear();

	std::string src;
	if (!LoadTextFileSafe("scripts/custom_sounds.json", src))
	{
		ALERT(at_console, "CustomSounds: doesn't exist scripts/custom_sounds.json\n");
		return;
	}

	rapidjson::Document doc;
	if (doc.Parse(src.c_str()).HasParseError() || !doc.IsObject() ||
		!doc.HasMember("sounds") || !doc["sounds"].IsArray())
	{
		ALERT(at_console, "CustomSounds: invalid JSON in custom_sounds.json\n");
		return;
	}

	for (auto& entry : doc["sounds"].GetArray())
	{
		if (!entry.IsString())
			continue;

		SoundGroupDef def;
		std::string soundPath = entry.GetString();

		if (!ParseSoundFile(soundPath, def))
			continue;

		for (const std::string& s : def.sounds)
			PRECACHE_SOUND((char*)s.c_str());

		g_CustomSounds.push_back(std::move(def));
	}

	ALERT(at_console, "CustomSounds: %d group of sounds loaded\n", (int)g_CustomSounds.size());
}

const SoundGroupDef* FindCustomSoundGroup(const std::string& id, const std::string& type)
{
	for (const SoundGroupDef& def : g_CustomSounds)
	{
		if (def.id == id && def.type == type)
			return &def;
	}
	return nullptr;
}