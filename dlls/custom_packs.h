#pragma once

#include <string>
#include <vector>

// Packs live in scripts/<pack_name>/ and may contain:
//   scripts/<pack_name>/custom_monsters.json
//   scripts/<pack_name>/custom_sounds.json
// Every path listed inside those files is relative to the pack folder, so
// "monsters/zombie.json" means scripts/<pack_name>/monsters/zombie.json

// Pack folder names (and only those) may use letters, numbers, '_' and '-'.
bool CustomPacks_IsValidName(const std::string& name);

// Relative paths listed inside a pack (e.g. "monsters/zombie.json"): same allowed
// characters as pack names plus '/' and '.', and no ".." so a pack can never reach
// files outside its own folder.
bool CustomPacks_IsSafeRelativePath(const std::string& path);

// Fills 'out' with every valid pack folder found in scripts/ (searched through all
// the engine search paths, so addon/downloads folders are included). Sorted
// alphabetically and without duplicates, so loading order is always the same.
void CustomPacks_GetPackNames(std::vector<std::string>& out);

// "scripts/<pack>/<relative>"
std::string CustomPacks_BuildPath(const std::string& pack, const std::string& relative);
