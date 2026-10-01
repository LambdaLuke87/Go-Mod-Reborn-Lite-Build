#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "custom_monsters.h"

#include <climits>
#include <unordered_map>

#include "../external/rapidjson/document.h"

static std::vector<CustomMonsterDef> g_CustomMonsters;

static const std::unordered_map<std::string, int> kRenderModeMap = {
	{"normal", 0},	 // kRenderNormal
	{"color", 1},	 // kRenderTransColor
	{"texture", 2},	 // kRenderTransTexture
	{"glow", 3},	 // kRenderGlow
	{"solid", 4},	 // kRenderTransAlpha
	{"additive", 5}, // kRenderTransAdd
};

static const std::unordered_map<std::string, int> kRenderFxMap = {
	{"none", 0},
	{"pulse_slow", 1},
	{"pulse_fast", 2},
	{"pulse_slow_wide", 3},
	{"pulse_fast_wide", 4},
	{"fade_slow", 5},
	{"fade_fast", 6},
	{"solid_slow", 7},
	{"solid_fast", 8},
	{"strobe_slow", 9},
	{"strobe_fast", 10},
	{"strobe_faster", 11},
	{"flicker_slow", 12},
	{"flicker_fast", 13},
	{"no_dissipation", 14},
	{"distort", 15},
	{"hologram", 16},
	// 17 = kRenderFxDeadPlayer -> excluded
	{"explode", 18},
	{"glow_shell", 19},
	// 20 = kRenderFxClampMinScale -> excluded
	// 21 = kRenderFxLightMultiplier -> excluded
};

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

	std::string classname = doc["classname"].GetString();

	if (classname.rfind("monster_", 0) != 0) // only "monster_" is accepted
	{
		ALERT(at_console, "CustomMonsters: invalid \"classname\" in %s ('%s' is not a NPC, must begin with 'monster_')\n",
			path.c_str(), classname.c_str());
		return false;
	}

	outDef.baseClassname = classname;

	if (doc.HasMember("id") && doc["id"].IsString())
		outDef.id = doc["id"].GetString();
	else
		outDef.id = outDef.baseClassname; // fallback

	if (doc.HasMember("model") && doc["model"].IsString())
		outDef.modelPath = doc["model"].GetString();

	if (doc.HasMember("custom_sounds") && doc["custom_sounds"].IsString())
		outDef.customSoundsId = doc["custom_sounds"].GetString();

	if (doc.HasMember("health") && doc["health"].IsInt())
		outDef.health = doc["health"].GetInt();

	if (doc.HasMember("damage_multiplier") && doc["damage_multiplier"].IsNumber())
		outDef.damageMultiplier = doc["damage_multiplier"].GetFloat();

	if (doc.HasMember("body") && doc["body"].IsInt())
	{
		int v = doc["body"].GetInt();
		if (v < 0 || v > 255)
			ALERT(at_console, "CustomMonsters: invalid 'body' in %s (max 255), ignored\n", path.c_str());
		else
			outDef.body = v;
	}

	if (doc.HasMember("skin") && doc["skin"].IsInt())
	{
		int v = doc["skin"].GetInt();
		if (v < 0 || v > 255)
			ALERT(at_console, "CustomMonsters: invalid 'skin' in %s (max 255), ignored\n", path.c_str());
		else
			outDef.skin = v;
	}

	if (doc.HasMember("blood_color") && doc["blood_color"].IsString())
	{
		std::string val = doc["blood_color"].GetString();
		if (val == "none")
		{
			outDef.bloodColor = DONT_BLEED;
			outDef.hasBloodColor = true;
		}
		else if (val == "red")
		{
			outDef.bloodColor = BLOOD_COLOR_RED;
			outDef.hasBloodColor = true;
		}
		else if (val == "green")
		{
			outDef.bloodColor = BLOOD_COLOR_GREEN;
			outDef.hasBloodColor = true;
		}
		else if (val == "blue")
		{
			outDef.bloodColor = BLOOD_COLOR_BLUE;
			outDef.hasBloodColor = true;
		}
		else if (val == "purple")
		{
			outDef.bloodColor = BLOOD_COLOR_PURPLE;
			outDef.hasBloodColor = true;
		}
		else
		{
			ALERT(at_console, "CustomMonsters: '%s' is an invalid blood_color in %s\n", val.c_str(), path.c_str());
		}
	}

	if (doc.HasMember("render") && doc["render"].IsObject())
	{
		const auto& r = doc["render"];

		if (r.HasMember("mode") && r["mode"].IsString())
		{
			std::string v = r["mode"].GetString();
			auto it = kRenderModeMap.find(v);
			if (it != kRenderModeMap.end())
			{
				outDef.renderMode = it->second;
				outDef.hasRenderMode = true;
			}
			else
				ALERT(at_console, "CustomMonsters: invalid render.mode '%s' in %s\n", v.c_str(), path.c_str());
		}

		if (r.HasMember("effect") && r["effect"].IsString())
		{
			std::string v = r["effect"].GetString();
			auto it = kRenderFxMap.find(v);
			if (it != kRenderFxMap.end())
			{
				outDef.renderFx = it->second;
				outDef.hasRenderFx = true;
			}
			else
				ALERT(at_console, "CustomMonsters: invalid render.effect '%s' in %s\n", v.c_str(), path.c_str());
		}

		// we accept both strings and numbers so as not to break if someone simply enters 255.
		auto readIntField = [&](const char* key, bool& hasFlag, int& outVal)
		{
			if (!r.HasMember(key))
				return;

			const auto& field = r[key];
			int v = 0;
			bool ok = false;

			if (field.IsInt())
			{
				v = field.GetInt();
				ok = true;
			}
			else if (field.IsString())
			{
				char* endPtr = nullptr;
				v = (int)strtol(field.GetString(), &endPtr, 10);
				ok = (endPtr && *endPtr == '\0');
			}

			if (ok)
			{
				hasFlag = true;
				outVal = v;
			}
			else
			{
				ALERT(at_console, "CustomMonsters: invalid render.%s in %s\n", key, path.c_str());
			}
		};

		readIntField("color_red", outDef.hasColorR, outDef.colorR);
		readIntField("color_green", outDef.hasColorG, outDef.colorG);
		readIntField("color_blue", outDef.hasColorB, outDef.colorB);
		readIntField("amount", outDef.hasRenderAmt, outDef.renderAmt);
	}

	if (doc.HasMember("scale") && doc["scale"].IsNumber())
	{
		float v = doc["scale"].GetFloat();
		if (v < 0.2f)
			ALERT(at_console, "CustomMonsters: invalid 'scale' in %s, minimum allowed is 0.2\n", path.c_str());
		else if (v > 10.0f)
			ALERT(at_console, "CustomMonsters: invalid 'scale' in %s, maximum allowed is 10.0\n", path.c_str());
		else
			outDef.scale = v;
	}

	if (doc.HasMember("gravity") && doc["gravity"].IsNumber())
	{
		float v = doc["gravity"].GetFloat();
		if (v < 0.2f)
			ALERT(at_console, "CustomMonsters: invalid 'gravity' in %s, minimum allowed is 0.2\n", path.c_str());
		else if (v > 2.5f)
			ALERT(at_console, "CustomMonsters: invalid 'gravity' in %s, maximum allowed is 2.5\n", path.c_str());
		else
			outDef.gravity = v;
	}

	if (doc.HasMember("gib_model") && doc["gib_model"].IsString())
	{
		std::string v = doc["gib_model"].GetString();
		if (v == "human")
			outDef.gibModel = 1;
		else if (v == "human_and_skull")
			outDef.gibModel = 2;
		else if (v == "alien")
			outDef.gibModel = 3;
		else
			ALERT(at_console, "CustomMonsters: invalid gib_model '%s' in %s\n", v.c_str(), path.c_str());
	}

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

	if (!def.customSoundsId.empty())
	{
		// override new sounds to the npc
		CBaseMonster* pMonster = static_cast<CBaseMonster*>(pEntity);
		pMonster->m_customSoundsId = def.customSoundsId;
	}

	if (def.health > 0)
	{
		pEntity->pev->health = def.health;
		pEntity->pev->max_health = def.health;
	}

	// damageMultiplier: Waiting to figure how to modify attack damage.

	if (def.body >= 0)
		pEntity->pev->body = def.body;

	if (def.skin >= 0)
		pEntity->pev->skin = def.skin;

	//if (def.hasBloodColor)
	//	pEntity->m_bloodColor = def.bloodColor;

	if (def.hasRenderMode)
		pEntity->pev->rendermode = def.renderMode;
	if (def.hasRenderFx)
		pEntity->pev->renderfx = def.renderFx;
	if (def.hasColorR)
		pEntity->pev->rendercolor.x = def.colorR;
	if (def.hasColorG)
		pEntity->pev->rendercolor.y = def.colorG;
	if (def.hasColorB)
		pEntity->pev->rendercolor.z = def.colorB;
	if (def.hasRenderAmt)
		pEntity->pev->renderamt = def.renderAmt;

	if (def.scale > 0.0f)
		pEntity->pev->scale = def.scale;

	if (def.gravity > 0.0f)
		pEntity->pev->gravity = def.gravity;

	if (def.gibModel > 0)
		pEntity->m_ForcedGibType = def.gibModel;
}