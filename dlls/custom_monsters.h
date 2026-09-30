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