#pragma once

#include <string>
#include <vector>

struct SoundGroupDef
{
	std::string id;
	std::string type; // "alert" | "idle" | "pain" | "death"
	std::vector<std::string> sounds;
	float volume = 1.0f;
	int pitch = 100;
};

void CustomSounds_Precache();

// search the group of sounds for an id + specific type. nullptr if dont exist.
const SoundGroupDef* FindCustomSoundGroup(const std::string& id, const std::string& type);