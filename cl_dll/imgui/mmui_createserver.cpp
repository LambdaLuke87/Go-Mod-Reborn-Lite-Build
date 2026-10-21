// mmui_createserver.cpp
// Go-Mod: Reborn - Create Server dialog (replaces Valve's GameUI "Create Server")
//
// Open it with the console command "gm_createserver"
// (resource/GameMenu.res: "command" "engine gm_createserver").
//
// Tabs:
//  Server   - map list (Go-Mod "gmr_*", HLDM hardcoded list, Others), preview image,
//             Game Mode (mp_gamemode) and Steam Networking (sv_use_steam_networking)
//  Game     - cvars from scr/game.scr
//  Settings - cvars from scr/settings.scr
//  Content  - cvars from scr/content.scr
//
// Precache bug fix: the chosen cvars are applied TWICE
//  1) before the server starts (only the cvars that already exist: second server in a session,
//     engine cvars, etc.) through the console, right before "map".
//  2) during the server creation: the values are left in the client cvars _gm_cs_pending1..8,
//     and the game dll applies them in InstallGameRules() BEFORE gamerules/precache, then once
//     more after listenserver.cfg so an old config can't undo them.
//     This works even in a clean session where the server cvars don't exist yet.

#include "hud.h"
#include "cl_util.h"
#include "filesystem_utils.h"

#include "imgui.h"
#include "mmui.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <algorithm>

// =====================================================================
//  Data
// =====================================================================

enum
{
	CS_TAB_SERVER = 0,
	CS_TAB_GAME,
	CS_TAB_SETTINGS,
	CS_TAB_CONTENT,
	CS_TAB_COUNT
};

enum
{
	CS_MAPS_GOMOD = 0,
	CS_MAPS_HLDM,
	CS_MAPS_OTHERS,
	CS_MAPS_COUNT
};

static const char* const kHLDMMaps[] = {
	"boot_camp",
	"bounce",
	"contamination",
	"crossfire",
	"datacore",
	"disposal",
	"doublecross",
	"frenzy",
	"gasworks",
	"lambda_bunker",
	"pool_party",
	"rapidcore",
	"rocket_frenzy",
	"rustmill",
	"snark_pit",
	"stalkyard",
	"subtransit",
	"undertow",
	"xen_dm",
};

static const char* const kPendingCvars[] = {
	"_gm_cs_pending1",
	"_gm_cs_pending2",
	"_gm_cs_pending3",
	"_gm_cs_pending4",
	"_gm_cs_pending5",
	"_gm_cs_pending6",
	"_gm_cs_pending7",
	"_gm_cs_pending8",
};

static const char* kLastValuesFile = "scr/createserver_last.txt";

enum ScrType
{
	SCR_BOOL = 0,
	SCR_NUMBER,
	SCR_STRING,
	SCR_LIST
};

struct ScrOption
{
	std::string cvar;
	std::string prompt;
	ScrType type = SCR_STRING;
	float minValue = -1.0f;
	float maxValue = -1.0f;
	std::vector<std::pair<std::string, std::string>> items; // LIST: label, value
	std::string defValue;

	// editable state
	char text[256] = {};
	bool checked = false;
	int listIndex = 0;
};

struct ScrPage
{
	const char* file;
	std::vector<ScrOption> options;
	bool missing = false;
};

static bool s_bOpen = false;
static int s_iTab = CS_TAB_SERVER;
static int s_iMapTab = CS_MAPS_GOMOD;

static std::vector<std::string> s_Maps[CS_MAPS_COUNT];
static int s_iSelected[CS_MAPS_COUNT] = {0, 0, 0}; // 0 = < Random Map >, 1.. = s_Maps[i][n - 1]

static int s_iGameMode = 1; // 0 Deathmatch, 1 Sandbox, 2 Busters, 3 Snark Frenzy
static bool s_bSteamNetworking = true;

static ScrPage s_Pages[3] = {
	{"scr/game.scr"},
	{"scr/settings.scr"},
	{"scr/content.scr"},
};

static std::unordered_map<std::string, std::string> s_LastValues;

struct PreviewTex
{
	unsigned int tex = 0;
	int w = 0;
	int h = 0;
};

static std::unordered_map<std::string, PreviewTex> s_Previews; // key: image path

// =====================================================================
//  Helpers
// =====================================================================

static std::string Lower(const std::string& s)
{
	std::string r = s;
	for (char& c : r)
		c = (char)tolower((unsigned char)c);
	return r;
}

static bool StartsWithNoCase(const std::string& s, const char* prefix)
{
	const size_t n = strlen(prefix);
	if (s.size() < n)
		return false;
	for (size_t i = 0; i < n; i++)
	{
		if (tolower((unsigned char)s[i]) != tolower((unsigned char)prefix[i]))
			return false;
	}
	return true;
}

static bool IsNumber(const std::string& s, double* out = nullptr)
{
	if (s.empty())
		return false;
	char* end = nullptr;
	const double v = strtod(s.c_str(), &end);
	if (!end || *end != '\0')
		return false;
	if (out)
		*out = v;
	return true;
}

// "32.000000" -> "32", "0.500000" -> "0.5"
static std::string FormatNumber(const std::string& s)
{
	double v = 0.0;
	if (!IsNumber(s, &v))
		return s;

	char buf[64];
	if (std::fabs(v - std::round(v)) < 0.00001)
		snprintf(buf, sizeof(buf), "%d", (int)std::round(v));
	else
		snprintf(buf, sizeof(buf), "%g", v);
	return buf;
}

// values travel through the console inside quotes: no quotes / new lines allowed
static std::string SanitizeValue(const std::string& s)
{
	std::string r;
	for (char c : s)
	{
		if (c == '"' || c == '\n' || c == '\r')
			continue;
		r += c;
	}
	return r;
}

// "name=value;" lists: ';' and '%' inside values are escaped (mp_teamlist uses ';')
static std::string EscapePendingValue(const std::string& s)
{
	std::string r;
	for (char c : s)
	{
		if (c == '%')
			r += "%25";
		else if (c == ';')
			r += "%3B";
		else
			r += c;
	}
	return r;
}

static bool IsValidCvarName(const std::string& s)
{
	if (s.empty())
		return false;
	for (char c : s)
	{
		if (!(isalnum((unsigned char)c) || c == '_' || c == '.'))
			return false;
	}
	return true;
}

static void SafeCopy(char* dst, size_t size, const std::string& src)
{
	snprintf(dst, size, "%s", src.c_str());
}

// =====================================================================
//  Last used values (scr/createserver_last.txt)
// =====================================================================

static void LoadLastValues()
{
	s_LastValues.clear();

	std::string text;
	if (!MMUI_LoadTextFile(kLastValuesFile, text))
		return;

	std::vector<MMUIToken> toks;
	MMUI_Tokenize(text, toks);

	for (size_t i = 0; i + 1 < toks.size(); i += 2)
		s_LastValues[Lower(toks[i].text)] = toks[i + 1].text;
}

static void SaveLastValues(const std::vector<std::pair<std::string, std::string>>& values)
{
	std::string text = "// Go-Mod: Reborn - last values used in the Create Server dialog (auto generated)\n";

	for (const auto& kv : values)
		text += "\"" + kv.first + "\"\t\"" + kv.second + "\"\n";

	g_pFileSystem->CreateDirHierarchy("scr", "GAMECONFIG");

	if (!FileSystem_WriteTextToFile(kLastValuesFile, text.c_str(), "GAMECONFIG"))
		FileSystem_WriteTextToFile(kLastValuesFile, text.c_str());
}

static const std::string* FindLast(const std::string& key)
{
	auto it = s_LastValues.find(Lower(key));
	return it != s_LastValues.end() ? &it->second : nullptr;
}

// =====================================================================
//  .scr parser
// =====================================================================

static bool IsBrace(const MMUIToken& t, char c)
{
	return !t.quoted && t.text.size() == 1 && t.text[0] == c;
}

static void SetOptionValue(ScrOption& opt, const std::string& value)
{
	switch (opt.type)
	{
	case SCR_BOOL:
	{
		double v = 0.0;
		opt.checked = IsNumber(value, &v) ? (v != 0.0) : false;
		break;
	}
	case SCR_NUMBER:
		SafeCopy(opt.text, sizeof(opt.text), FormatNumber(value));
		break;
	case SCR_STRING:
		SafeCopy(opt.text, sizeof(opt.text), value);
		break;
	case SCR_LIST:
	{
		opt.listIndex = 0;
		double want = 0.0;
		const bool numeric = IsNumber(value, &want);

		bool found = false;
		for (size_t i = 0; i < opt.items.size(); i++)
		{
			double have = 0.0;
			if ((numeric && IsNumber(opt.items[i].second, &have) && std::fabs(have - want) < 0.0001) ||
				(!numeric && opt.items[i].second == value))
			{
				opt.listIndex = (int)i;
				found = true;
				break;
			}
		}

		// the header of settings.scr says LIST defaults are an index, accept that too
		if (!found && numeric && want >= 0.0 && want < (double)opt.items.size())
			opt.listIndex = (int)want;
		break;
	}
	}
}

static std::string GetOptionValue(const ScrOption& opt)
{
	switch (opt.type)
	{
	case SCR_BOOL:
		return opt.checked ? "1" : "0";

	case SCR_NUMBER:
	{
		double v = 0.0;
		if (!IsNumber(opt.text, &v))
			v = atof(opt.defValue.c_str());

		if (opt.minValue != -1.0f && v < opt.minValue)
			v = opt.minValue;
		if (opt.maxValue != -1.0f && v > opt.maxValue)
			v = opt.maxValue;

		char buf[64];
		snprintf(buf, sizeof(buf), "%g", v);
		return FormatNumber(buf);
	}

	case SCR_STRING:
		return SanitizeValue(opt.text);

	case SCR_LIST:
		if (opt.listIndex >= 0 && opt.listIndex < (int)opt.items.size())
			return opt.items[opt.listIndex].second;
		return opt.defValue;
	}

	return "";
}

static bool ParseScr(const std::string& src, std::vector<ScrOption>& out, std::string& error)
{
	std::vector<MMUIToken> toks;
	MMUI_Tokenize(src, toks);

	size_t i = 0;
	auto next = [&](MMUIToken& t) -> bool
	{
		if (i >= toks.size())
			return false;
		t = toks[i++];
		return true;
	};

	// skip "VERSION x.x" and look for DESCRIPTION <name> {
	while (i < toks.size() && !(!toks[i].quoted && toks[i].text == "DESCRIPTION"))
		i++;

	if (i >= toks.size())
	{
		error = "DESCRIPTION not found";
		return false;
	}

	i += 2; // DESCRIPTION SERVER_OPTIONS
	MMUIToken t;
	if (!next(t) || !IsBrace(t, '{'))
	{
		error = "missing { after DESCRIPTION";
		return false;
	}

	while (next(t))
	{
		if (IsBrace(t, '}'))
			return true; // end of description

		ScrOption opt;
		opt.cvar = t.text;

		MMUIToken a;
		if (!next(a) || !IsBrace(a, '{'))
		{
			error = "missing { after " + opt.cvar;
			return false;
		}

		if (!next(a))
			break;
		opt.prompt = a.text;

		if (!next(a) || !IsBrace(a, '{'))
		{
			error = "missing type block in " + opt.cvar;
			return false;
		}

		MMUIToken type;
		if (!next(type))
			break;

		const std::string typeName = Lower(type.text);
		if (typeName == "bool")
			opt.type = SCR_BOOL;
		else if (typeName == "number")
			opt.type = SCR_NUMBER;
		else if (typeName == "list")
			opt.type = SCR_LIST;
		else
			opt.type = SCR_STRING;

		// type info until }
		std::vector<MMUIToken> info;
		while (next(a) && !IsBrace(a, '}'))
			info.push_back(a);

		if (opt.type == SCR_NUMBER && info.size() >= 2)
		{
			opt.minValue = (float)atof(info[0].text.c_str());
			opt.maxValue = (float)atof(info[1].text.c_str());
		}
		else if (opt.type == SCR_LIST)
		{
			for (size_t k = 0; k + 1 < info.size(); k += 2)
				opt.items.emplace_back(info[k].text, info[k + 1].text);
		}

		// default block { "value" }
		if (!next(a) || !IsBrace(a, '{'))
		{
			error = "missing default block in " + opt.cvar;
			return false;
		}

		bool gotDefault = false;
		while (next(a) && !IsBrace(a, '}'))
		{
			if (!gotDefault)
			{
				opt.defValue = a.text;
				gotDefault = true;
			}
		}

		// end of the cvar block
		if (!next(a) || !IsBrace(a, '}'))
		{
			error = "missing } at the end of " + opt.cvar;
			return false;
		}

		if (!IsValidCvarName(opt.cvar))
			continue;

		const std::string* last = FindLast(opt.cvar);
		SetOptionValue(opt, last ? *last : opt.defValue);

		out.push_back(std::move(opt));
	}

	return true;
}

static void LoadPage(ScrPage& page)
{
	page.options.clear();
	page.missing = false;

	std::string text;
	if (!MMUI_LoadTextFile(page.file, text))
	{
		page.missing = true;
		gEngfuncs.Con_Printf("Create Server: %s not found\n", page.file);
		return;
	}

	std::string error;
	if (!ParseScr(text, page.options, error))
		gEngfuncs.Con_Printf("Create Server: error in %s: %s\n", page.file, error.c_str());
}

// =====================================================================
//  Maps
// =====================================================================

static void ListMaps(const char* pathID, std::set<std::string>& out)
{
	if (!g_pFileSystem)
		return;

	FileFindHandle_t handle;
	const char* name = g_pFileSystem->FindFirst("maps/*.bsp", &handle, pathID);

	// the handle is only valid if FindFirst found something
	if (!name)
		return;

	for (; name; name = g_pFileSystem->FindNext(handle))
	{
		if (g_pFileSystem->FindIsDirectory(handle))
			continue;

		std::string file = name;

		const size_t slash = file.find_last_of("/\\");
		if (slash != std::string::npos)
			file = file.substr(slash + 1);

		if (file.size() > 4 && Lower(file.substr(file.size() - 4)) == ".bsp")
			file = file.substr(0, file.size() - 4);

		if (!file.empty())
			out.insert(file);
	}

	g_pFileSystem->FindClose(handle);
}

static bool IsHLDMMap(const std::string& name)
{
	const std::string lower = Lower(name);
	for (const char* m : kHLDMMaps)
	{
		if (lower == m)
			return true;
	}
	return false;
}

static void LoadMaps()
{
	for (auto& list : s_Maps)
		list.clear();

	// maps of our mod (+ downloaded ones). Valve's maps live in "DEFAULTGAME".
	std::set<std::string> mod;
	ListMaps("GAME", mod);
	ListMaps("GAMEDOWNLOAD", mod);

	if (mod.empty()) // filesystem without path IDs (Xash3D, etc.)
		ListMaps(nullptr, mod);

	std::set<std::string> valve;
	ListMaps("DEFAULTGAME", valve);

	// if the filesystem ignores path IDs both lists are the same: don't remove anything then
	const bool subtractValve = !valve.empty() && !std::includes(valve.begin(), valve.end(), mod.begin(), mod.end());

	for (const std::string& map : mod)
	{
		if (IsHLDMMap(map))
			continue;

		if (subtractValve && valve.count(map) && !StartsWithNoCase(map, "gmr_"))
			continue;

		if (StartsWithNoCase(map, "gmr_"))
			s_Maps[CS_MAPS_GOMOD].push_back(map);
		else
			s_Maps[CS_MAPS_OTHERS].push_back(map);
	}

	for (const char* m : kHLDMMaps)
		s_Maps[CS_MAPS_HLDM].push_back(m);

	// std::set is case sensitive, sort the final lists without case
	for (int g = 0; g < CS_MAPS_COUNT; g++)
	{
		std::sort(s_Maps[g].begin(), s_Maps[g].end(), [](const std::string& a, const std::string& b)
			{ return Lower(a) < Lower(b); });
	}
}

static void RestoreMapSelection()
{
	for (int& sel : s_iSelected)
		sel = 0;

	s_iMapTab = CS_MAPS_GOMOD;
	if (const std::string* tab = FindLast("_maptab"))
		s_iMapTab = (std::max)(0, (std::min)(atoi(tab->c_str()), CS_MAPS_COUNT - 1));

	if (const std::string* map = FindLast("_map"))
	{
		for (int g = 0; g < CS_MAPS_COUNT; g++)
		{
			for (size_t i = 0; i < s_Maps[g].size(); i++)
			{
				if (!stricmp(s_Maps[g][i].c_str(), map->c_str()))
				{
					s_iSelected[g] = (int)i + 1;
					break;
				}
			}
		}
	}
}

// "" when the list is empty
static std::string GetSelectedMap(bool resolveRandom)
{
	const std::vector<std::string>& list = s_Maps[s_iMapTab];
	if (list.empty())
		return "";

	int sel = s_iSelected[s_iMapTab];
	if (sel <= 0 || sel > (int)list.size())
	{
		if (!resolveRandom)
			return "";
		return list[rand() % list.size()];
	}

	return list[sel - 1];
}

// =====================================================================
//  Preview images
// =====================================================================

static const PreviewTex* GetPreview(const std::string& path)
{
	auto it = s_Previews.find(path);
	if (it == s_Previews.end())
	{
		PreviewTex p;
		p.tex = MMUI_LoadPNG(path.c_str(), &p.w, &p.h);
		it = s_Previews.emplace(path, p).first;
	}

	return it->second.tex ? &it->second : nullptr;
}

static const PreviewTex* GetMapPreview()
{
	const std::string map = GetSelectedMap(false);
	const std::string path = map.empty() ? "maps/preview/random.png" : "maps/preview/" + map + ".png";

	if (const PreviewTex* p = GetPreview(path))
		return p;

	return GetPreview("maps/empty.png");
}

// =====================================================================
//  Start
// =====================================================================

static void AddOrReplace(std::vector<std::pair<std::string, std::string>>& list, const std::string& name, const std::string& value)
{
	for (auto& kv : list)
	{
		if (!stricmp(kv.first.c_str(), name.c_str()))
		{
			kv.second = value;
			return;
		}
	}
	list.emplace_back(name, value);
}

// the game dll reads these in InstallGameRules() (dlls/gamerules.cpp)
static void WritePendingCvars(const std::vector<std::pair<std::string, std::string>>& values)
{
	const int count = (int)(sizeof(kPendingCvars) / sizeof(kPendingCvars[0]));

	for (int i = 0; i < count; i++)
		gEngfuncs.Cvar_Set(kPendingCvars[i], "");

	int slot = 0;
	std::string chunk;

	for (const auto& kv : values)
	{
		const std::string entry = kv.first + "=" + EscapePendingValue(kv.second) + ";";

		if (!chunk.empty() && chunk.size() + entry.size() > 900)
		{
			if (slot < count)
				gEngfuncs.Cvar_Set(kPendingCvars[slot], chunk.c_str());
			slot++;
			chunk.clear();
		}

		chunk += entry;
	}

	if (!chunk.empty())
	{
		if (slot < count)
			gEngfuncs.Cvar_Set(kPendingCvars[slot], chunk.c_str());
		slot++;
	}

	if (slot > count)
		gEngfuncs.Con_Printf("Create Server: too many cvars, some will only apply after the map loads\n");
}

static bool AlwaysSendBeforeMap(const std::string& name)
{
	// engine commands (not cvars), they must run before "map"
	return !stricmp(name.c_str(), "maxplayers") ||
		   !stricmp(name.c_str(), "sv_use_steam_networking");
}

static void CloseDialog()
{
	s_bOpen = false;
}

static void StartServer()
{
	const std::string map = GetSelectedMap(true);
	if (map.empty())
		return;

	std::vector<std::pair<std::string, std::string>> values;

	for (const ScrPage& page : s_Pages)
	{
		for (const ScrOption& opt : page.options)
			AddOrReplace(values, opt.cvar, GetOptionValue(opt));
	}

	// the Server tab always wins
	AddOrReplace(values, "mp_gamemode", std::to_string(s_iGameMode));
	AddOrReplace(values, "sv_use_steam_networking", s_bSteamNetworking ? "1" : "0");

	// remember everything for next time
	std::vector<std::pair<std::string, std::string>> save = values;
	save.emplace_back("_map", GetSelectedMap(false));
	save.emplace_back("_maptab", std::to_string(s_iMapTab));
	SaveLastValues(save);

	// (2) during the creation: the game dll applies these before precaching
	WritePendingCvars(values);

	// (1) before the creation: everything that already exists right now
	std::string cmd = "disconnect\nwait\nwait\n";

	// Steam Networking only works on internet servers (same as Valve's Create Server)
	if (s_bSteamNetworking)
		cmd += "sv_lan 0\nsetmaster enable\n";

	for (const auto& kv : values)
	{
		if (AlwaysSendBeforeMap(kv.first) || gEngfuncs.pfnGetCvarPointer(kv.first.c_str()))
			cmd += kv.first + " \"" + kv.second + "\"\n";
	}

	cmd += "map " + map + "\n";

	gEngfuncs.Con_DPrintf("Create Server: starting %s\n", map.c_str());
	gEngfuncs.pfnClientCmd((char*)cmd.c_str());

	CloseDialog();
}

// =====================================================================
//  Drawing
// =====================================================================

static void DrawServerTab(const ImVec2& panelMin, const ImVec2& panelMax)
{
	const MMUIScheme& s = MMUI_GetScheme();
	const float k = MMUI_Scale();
	const ImVec2 wp = ImGui::GetWindowPos();

	auto toLocal = [&](float x, float y)
	{ return ImVec2(x - wp.x, y - wp.y); };

	// "Map"
	ImGui::SetCursorPos(toLocal(panelMin.x + 14.0f * k, panelMin.y + 10.0f * k));
	ImGui::TextColored(s.Accent, "%s", MMUI_Localize("#GameUI_Map"));

	// Go-Mod / HLDM / Others
	const float listX = panelMin.x + 100.0f * k;
	const float tabsY = panelMin.y + 40.0f * k;

	const char* mapTabs[CS_MAPS_COUNT] = {
		MMUI_Localize("#GameUI_MapGroup_GoMod"),
		MMUI_Localize("#GameUI_MapGroup_HLDM"),
		MMUI_Localize("#GameUI_MapGroup_Others"),
	};

	ImGui::SetCursorScreenPos(ImVec2(listX, tabsY));
	MMUI_TabStrip("##maptabs", mapTabs, CS_MAPS_COUNT, &s_iMapTab, 95.0f * k);

	// map list
	const float listY = ImGui::GetCursorScreenPos().y;
	const float listW = 450.0f * k;
	const float listH = (std::max)(120.0f * k, (panelMax.y - 100.0f * k) - listY);

	ImGui::SetCursorScreenPos(ImVec2(listX, listY));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, s.ListBg);
	ImGui::PushStyleColor(ImGuiCol_Border, s.BorderBright);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f * k, 4.0f * k));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

	if (ImGui::BeginChild("##maplist", ImVec2(listW, listH), ImGuiChildFlags_Borders))
	{
		const std::vector<std::string>& list = s_Maps[s_iMapTab];
		int& sel = s_iSelected[s_iMapTab];
		const float rowH = ImGui::GetFontSize() + 13.0f * k;

		if (list.empty())
		{
			ImGui::SetCursorPos(ImVec2(6.0f * k, 6.0f * k));
			ImGui::TextDisabled("%s", MMUI_Localize("#GameUI_NoMapsFound"));
		}
		else
		{
			for (int i = 0; i <= (int)list.size(); i++)
			{
				const bool selected = (sel == i);
				const char* label = (i == 0) ? MMUI_Localize("#GameUI_RandomMap") : list[i - 1].c_str();

				ImGui::PushID(i);
				ImGui::PushStyleColor(ImGuiCol_Text, selected ? s.SelectionText : s.Text);
				if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.0f, rowH)))
				{
					sel = i;
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
						StartServer();
				}
				ImGui::PopStyleColor();

				if (selected && ImGui::IsWindowAppearing())
					ImGui::SetScrollHereY();

				ImGui::PopID();
			}
		}
	}
	ImGui::EndChild();

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(2);

	// preview
	const float previewX = listX + listW + 30.0f * k;
	const ImVec2 previewSize(300.0f * k, 275.0f * k);
	const ImVec2 pmin(previewX, listY);
	const ImVec2 pmax(previewX + previewSize.x, listY + previewSize.y);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(pmin, pmax, MMUI_Col(s.ListBg));

	if (const PreviewTex* p = GetMapPreview())
	{
		// fit keeping aspect ratio
		float w = previewSize.x, h = previewSize.y;
		if (p->w > 0 && p->h > 0)
		{
			const float aspect = (float)p->w / (float)p->h;
			if (w / h > aspect)
				w = h * aspect;
			else
				h = w / aspect;
		}

		const ImVec2 imin(pmin.x + (previewSize.x - w) * 0.5f, pmin.y + (previewSize.y - h) * 0.5f);
		dl->AddImage((ImTextureID)(intptr_t)p->tex, imin, ImVec2(imin.x + w, imin.y + h));
	}

	dl->AddRect(pmin, pmax, MMUI_Col(s.BorderDark));

	// Game Mode
	const float gmY = pmax.y + 12.0f * k;
	ImGui::SetCursorScreenPos(ImVec2(previewX + 4.0f * k, gmY));
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(MMUI_Localize("#GameUI_GameMode"));
	ImGui::SameLine(0.0f, 18.0f * k);

	const char* modes[4] = {
		MMUI_Localize("#GameUI_GameMode_Deathmatch"),
		MMUI_Localize("#GameUI_GameMode_Sandbox"),
		MMUI_Localize("#GameUI_GameMode_Busters"),
		MMUI_Localize("#GameUI_GameMode_SnarkFrenzy"),
	};

	if (s_iGameMode < 0 || s_iGameMode > 3)
		s_iGameMode = 1;

	ImGui::SetNextItemWidth(160.0f * k);
	ImGui::PushStyleColor(ImGuiCol_Button, s.EditBg);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, s.EditBg);
	if (ImGui::BeginCombo("##gamemode", modes[s_iGameMode]))
	{
		for (int i = 0; i < 4; i++)
		{
			if (ImGui::Selectable(modes[i], s_iGameMode == i))
				s_iGameMode = i;
		}
		ImGui::EndCombo();
	}
	ImGui::PopStyleColor(2);

	// Enable Steam Networking
	ImGui::SetCursorScreenPos(ImVec2(panelMin.x + 20.0f * k, panelMax.y - 48.0f * k));
	MMUI_Checkbox(MMUI_Localize("#GameUI_SteamNetworking"), &s_bSteamNetworking);
}

static void DrawScrPage(ScrPage& page, const ImVec2& panelMin, const ImVec2& panelMax)
{
	const MMUIScheme& s = MMUI_GetScheme();
	const float k = MMUI_Scale();

	const ImVec2 min(panelMin.x + 12.0f * k, panelMin.y + 12.0f * k);
	const ImVec2 size(panelMax.x - panelMin.x - 24.0f * k, panelMax.y - panelMin.y - 24.0f * k);

	ImGui::SetCursorScreenPos(min);
	ImGui::PushStyleColor(ImGuiCol_Border, s.BorderBright);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f * k, 12.0f * k));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f * k, 10.0f * k));

	if (ImGui::BeginChild(page.file, size, ImGuiChildFlags_Borders, ImGuiWindowFlags_AlwaysVerticalScrollbar))
	{
		if (page.missing)
		{
			ImGui::TextDisabled("%s %s", MMUI_Localize("#GameUI_ScrFileMissing"), page.file);
		}
		else if (page.options.empty())
		{
			ImGui::TextDisabled("%s", MMUI_Localize("#GameUI_NoOptions"));
		}

		const float contentW = ImGui::GetContentRegionAvail().x;
		const float valueX = ImGui::GetCursorPosX() + contentW * 0.52f;
		const float valueW = contentW * 0.48f;

		for (size_t i = 0; i < page.options.size(); i++)
		{
			ScrOption& opt = page.options[i];
			const char* label = MMUI_Localize(opt.prompt.c_str());

			ImGui::PushID((int)i);

			switch (opt.type)
			{
			case SCR_BOOL:
				MMUI_Checkbox(label, &opt.checked);
				break;

			case SCR_NUMBER:
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(label);
				ImGui::SameLine(valueX);
				ImGui::SetNextItemWidth(valueW);
				ImGui::InputText("##value", opt.text, sizeof(opt.text), ImGuiInputTextFlags_CharsDecimal);
				break;

			case SCR_STRING:
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(label);
				ImGui::SameLine(valueX);
				ImGui::SetNextItemWidth(valueW);
				ImGui::InputText("##value", opt.text, sizeof(opt.text));
				break;

			case SCR_LIST:
			{
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(label);
				ImGui::SameLine(valueX);
				ImGui::SetNextItemWidth(valueW);

				const char* preview = "";
				if (opt.listIndex >= 0 && opt.listIndex < (int)opt.items.size())
					preview = MMUI_Localize(opt.items[opt.listIndex].first.c_str());

				ImGui::PushStyleColor(ImGuiCol_Button, s.EditBg);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, s.EditBg);
				if (ImGui::BeginCombo("##value", preview))
				{
					for (size_t n = 0; n < opt.items.size(); n++)
					{
						if (ImGui::Selectable(MMUI_Localize(opt.items[n].first.c_str()), opt.listIndex == (int)n))
							opt.listIndex = (int)n;
					}
					ImGui::EndCombo();
				}
				ImGui::PopStyleColor(2);
				break;
			}
			}

			ImGui::PopID();
		}
	}
	ImGui::EndChild();

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();
}

static void CreateServer_Draw()
{
	const float k = MMUI_Scale();
	const ImVec2 dialogSize(960.0f * k, 690.0f * k);

	if (!MMUI_BeginDialog("##gomod_createserver", MMUI_Localize("#GameUI_CreateServer"), dialogSize))
		CloseDialog();

	const ImVec2 wp = ImGui::GetWindowPos();
	const ImVec2 ws = ImGui::GetWindowSize();

	// main tabs
	const char* tabs[CS_TAB_COUNT] = {
		MMUI_Localize("#GameUI_Server"),
		MMUI_Localize("#GameUI_Game"),
		MMUI_Localize("#GameUI_Settings"),
		MMUI_Localize("#GameUI_Content"),
	};

	ImGui::SetCursorScreenPos(ImVec2(wp.x + 10.0f * k, wp.y + 44.0f * k));
	MMUI_TabStrip("##maintabs", tabs, CS_TAB_COUNT, &s_iTab, 95.0f * k);

	const ImVec2 panelMin(wp.x + 10.0f * k, ImGui::GetCursorScreenPos().y);
	const ImVec2 panelMax(wp.x + ws.x - 10.0f * k, wp.y + ws.y - 60.0f * k);
	MMUI_DrawPanelFrame(panelMin, panelMax);

	switch (s_iTab)
	{
	case CS_TAB_SERVER:
		DrawServerTab(panelMin, panelMax);
		break;
	case CS_TAB_GAME:
		DrawScrPage(s_Pages[0], panelMin, panelMax);
		break;
	case CS_TAB_SETTINGS:
		DrawScrPage(s_Pages[1], panelMin, panelMax);
		break;
	case CS_TAB_CONTENT:
		DrawScrPage(s_Pages[2], panelMin, panelMax);
		break;
	}

	// Start / Cancel
	const ImVec2 buttonSize(110.0f * k, 36.0f * k);
	const float buttonsY = wp.y + ws.y - 48.0f * k;

	const bool canStart = !s_Maps[s_iMapTab].empty();

	ImGui::SetCursorScreenPos(ImVec2(wp.x + ws.x - 2.0f * buttonSize.x - 22.0f * k, buttonsY));
	ImGui::BeginDisabled(!canStart);
	const bool start = MMUI_Button(MMUI_Localize("#GameUI_Start"), buttonSize);
	ImGui::EndDisabled();

	ImGui::SetCursorScreenPos(ImVec2(wp.x + ws.x - buttonSize.x - 12.0f * k, buttonsY));
	const bool cancel = MMUI_Button(MMUI_Localize("#GameUI_Cancel"), buttonSize);

	// ESC closes (unless a combo is open or a text entry is being edited)
	const bool esc = ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsAnyItemActive() &&
					 !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);

	MMUI_EndDialog();

	if (start && s_bOpen)
		StartServer();
	else if (cancel || esc)
		CloseDialog();
}

// =====================================================================
//  Open / panel registration
// =====================================================================

static void CreateServer_Open()
{
	if (!MMUI_EnsureHook())
		return;

	static bool seeded = false;
	if (!seeded)
	{
		srand((unsigned int)time(nullptr));
		seeded = true;
	}

	// read everything again every time, so editing the .scr files / adding maps needs no restart
	LoadLastValues();

	for (ScrPage& page : s_Pages)
		LoadPage(page);

	LoadMaps();
	RestoreMapSelection();

	s_iGameMode = 1;
	if (const std::string* gm = FindLast("mp_gamemode"))
		s_iGameMode = (std::max)(0, (std::min)(atoi(gm->c_str()), 3));

	s_bSteamNetworking = true;
	if (const std::string* sn = FindLast("sv_use_steam_networking"))
		s_bSteamNetworking = atoi(sn->c_str()) != 0;

	// retry previews that were missing the last time
	for (auto it = s_Previews.begin(); it != s_Previews.end();)
	{
		if (it->second.tex == 0)
			it = s_Previews.erase(it);
		else
			++it;
	}

	s_bOpen = true;
}

static bool CreateServer_IsOpen()
{
	return s_bOpen;
}

static void CreateServer_ForceClose()
{
	s_bOpen = false;
}

static const MMUIPanel s_CreateServerPanel = {
	"createserver",
	CreateServer_IsOpen,
	CreateServer_Draw,
	CreateServer_ForceClose,
};

void MMUI_CreateServer_Register()
{
	for (const char* name : kPendingCvars)
		gEngfuncs.pfnRegisterVariable(name, "", 0);

	gEngfuncs.pfnAddCommand("gm_createserver", CreateServer_Open);

	MMUI_RegisterPanel(&s_CreateServerPanel);
}
