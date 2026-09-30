#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "custom_monsters.h"

#include "../external/rapidjson/document.h"

static std::vector<CustomMonsterDef> g_CustomMonsters;

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

static bool ParseMonsterFile(const std::string& path, CustomMonsterDef& outDef)
{
	std::string src;
	if (!LoadTextFileSafe(path.c_str(), src))
	{
		ALERT(at_console, "CustomMonsters: could not be opened %s\n", path.c_str());
		return false;
	}

	rapidjson::Document doc;
	if (doc.Parse(src.c_str()).HasParseError())
	{
		ALERT(at_console, "CustomMonsters: invalid JSON in %s\n", path.c_str());
		return false;
	}

	if (!doc.IsObject() || !doc.HasMember("classname") || !doc["classname"].IsString())
	{
		ALERT(at_console, "CustomMonsters: missing \"classname\" in %s\n", path.c_str());
		return false;
	}

	outDef.baseClassname = doc["classname"].GetString();

	if (doc.HasMember("id") && doc["id"].IsString())
		outDef.id = doc["id"].GetString();
	else
		outDef.id = outDef.baseClassname; // fallback

	if (doc.HasMember("model") && doc["model"].IsString())
		outDef.modelPath = doc["model"].GetString();

	if (doc.HasMember("health") && doc["health"].IsInt())
		outDef.health = doc["health"].GetInt();

	if (doc.HasMember("damage_multiplier") && doc["damage_multiplier"].IsNumber())
		outDef.damageMultiplier = doc["damage_multiplier"].GetFloat();

	return true;
}

void CustomMonsters_Precache()
{
	g_CustomMonsters.clear();

	std::string src;
	if (!LoadTextFileSafe("scripts/custom_monsters.json", src))
	{
		ALERT(at_console, "CustomMonsters: don't exist in scripts/custom_monsters.json\n");
		return;
	}

	rapidjson::Document doc;
	if (doc.Parse(src.c_str()).HasParseError() || !doc.IsObject() ||
		!doc.HasMember("monsters") || !doc["monsters"].IsArray())
	{
		ALERT(at_console, "CustomMonsters: invalid JSON in custom_monsters.json\n");
		return;
	}

	for (auto& entry : doc["monsters"].GetArray())
	{
		if (!entry.IsString())
			continue;

		CustomMonsterDef def;
		std::string monsterPath = entry.GetString();

		if (!ParseMonsterFile(monsterPath, def))
			continue;

		// Precache
		if (!def.modelPath.empty())
			PRECACHE_MODEL((char*)def.modelPath.c_str());

		g_CustomMonsters.push_back(std::move(def));
	}

	ALERT(at_console, "CustomMonsters: %d loaded definitions\n", (int)g_CustomMonsters.size());
}

const CustomMonsterDef* FindCustomMonsterDef(const char* id)
{
	for (const CustomMonsterDef& def : g_CustomMonsters)
	{
		if (def.id == id)
			return &def;
	}
	return nullptr;
}

void ApplyCustomMonsterOverrides(CBaseEntity* pEntity, const CustomMonsterDef& def)
{
	if (!pEntity)
		return;

	if (!def.modelPath.empty())
	{
		// Save original npc colission
		Vector mins = pEntity->pev->mins;
		Vector maxs = pEntity->pev->maxs;

		// change model
		SET_MODEL(ENT(pEntity->pev), def.modelPath.c_str());

		// restore collision
		UTIL_SetSize(pEntity->pev, mins, maxs);
	}

	if (def.health > 0)
	{
		pEntity->pev->health = def.health;
		pEntity->pev->max_health = def.health;
	}

	// damageMultiplier: Waiting to figure how to modify attack damage.
}