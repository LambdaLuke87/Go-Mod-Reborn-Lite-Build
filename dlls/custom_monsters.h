#pragma once

#include <string>
#include <vector>

struct CustomMonsterDef
{
	std::string id;			   // what is written in "summon <id>"
	std::string baseClassname; // real NPC, example: "monster_zombie"
	std::string modelPath;	   // example: "models/custom_zombie.mdl"
	int health = 0;
	float damageMultiplier = 1.0f;

	int body = -1;
	int skin = -1;

	bool hasBloodColor = false;
	int bloodColor = 0; // DONT_BLEED / BLOOD_COLOR_RED / etc.

	bool hasRenderMode = false;
	int renderMode = 0;
	bool hasRenderFx = false;
	int renderFx = 0;
	bool hasColorR = false;
	int colorR = 0;
	bool hasColorG = false;
	int colorG = 0;
	bool hasColorB = false;
	int colorB = 0;
	bool hasRenderAmt = false;
	int renderAmt = 0;

	float scale = 0.0f;
	float gravity = 0.0f;

	int gibModel = 0; // (1=human, 2=human_and_skull, 3=alien)
};

// Call ONCE, within the world's precache hook (still to be defined).
// Loads scripts/custom_monsters.json and each listed monster, and precaches their models.
void CustomMonsters_Precache();

// Look for a definition by "id" (or by "classname" if the JSON does not have an "id").
// Returns nullptr if it does not exist.
const CustomMonsterDef* FindCustomMonsterDef(const char* id);

// Applies a model/hitbox to an entity already created with CBaseEntity::CreateCustom().
class CBaseEntity;
void ApplyCustomMonsterOverrides(CBaseEntity* pEntity, const CustomMonsterDef& def);