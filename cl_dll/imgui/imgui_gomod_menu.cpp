// gomod_menu.cpp
// Garry's Mod 11 Styled Menu for Go-Mod: Reborn (Dear ImGui).

#include "hud.h"
#include "cl_util.h"
#include "filesystem_utils.h"

#include "imgui.h"

#ifdef ARRAYSIZE
#undef ARRAYSIZE
#endif

#ifdef _WIN32
#define HSPRITE WINDOWS_HSPRITE
#endif
#include <SDL2/SDL.h>
#include <../external/SDL2/SDL_opengl.h>
#ifdef _WIN32
#undef HSPRITE
#endif

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#include "../external/rapidjson/document.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

extern bool g_bShowMenu;

// =====================================================================
//  Utilities
// =====================================================================

void ScaleSize(int& width, int& height)
{
	float yfactor = (float)ScreenWidth / (float)ScreenHeight;

	float xscale = ((float)ScreenWidth / 1536.0f);
	float yscale = ((float)ScreenHeight / 1536.0f) * yfactor;

	int minwidth = width * 0.67f;
	int minheight = height * 0.67f;

	width *= xscale;
	height *= yscale;

	if (height < minheight)
		height = minheight;

	if (width < minwidth)
		width = minwidth;
}

static void RunCmd(const char* cmd)
{
	char buf[256];
	snprintf(buf, sizeof(buf), "%s", cmd);
	gEngfuncs.pfnClientCmd(buf);
}

static bool LoadTextFile(const char* path, std::string& out)
{
	int len = 0;
	byte* data = gEngfuncs.COM_LoadFile((char*)path, 5, &len);

	if (!data)
		return false;

	const char* p = (const char*)data;

	// saltar BOM UTF-8
	if (len >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
	{
		p += 3;
		len -= 3;
	}

	out.assign(p, len);
	gEngfuncs.COM_FreeFile(data);
	return true;
}

// =====================================================================
//  Data
// =====================================================================

struct MenuItem
{
	std::string name;
	std::string image;
	std::string command;

	GLuint tex = 0;
	bool triedLoad = false;
};

struct MenuCategory
{
	std::string name;
	std::vector<MenuItem> items;
};

struct Toggle
{
	const char* label;
	const char* command;
};

struct Action
{
	const char* label;
	const char* command;
};

struct SpawnTab
{
	const char* title;
	const char* folder; // every .json inside this folder is one section
	std::vector<MenuCategory> cats;
	bool loaded = false;
	std::vector<Toggle> toggles;
	std::vector<Action> actions;
};

struct ToolDef
{
	const char* name;
	const char* command;
	const char* hint = nullptr;
};

struct ToolCategory
{
	const char* name;
	std::vector<ToolDef> tools;
};

struct VoiceOption
{
	const char* label;
	const char* value;
};

struct FlashLightOption
{
	const char* label;
	int value;
};

static std::vector<SpawnTab> g_Tabs;
static std::vector<ToolCategory> g_ToolCats;
static std::vector<ToolCategory> g_RenderToolCats;

static std::unordered_map<std::string, std::string> g_KeyStrings;
static bool g_KeyStringsLoaded = false;
static const char* KEY_STRINGS_FILE = "resource/gomod_gui.txt";

static const VoiceOption g_VoiceOptions[] = {
	{"Gordon/HEV Suit", "hevsuit"},
	{"Team Fortress Classic", "dmc"},
	{"Counter-Strike 1.6", "cstrike"},
	{"Half-Life Alpha", "hlalpha"},
};

static const FlashLightOption g_FlashLightOptions[] = {
	{"Flashlight", 0},
	{"OP4 Night Vision", 1},
	{"CS1.6 Night Vision", 2},
	{"Red Night Vision", 3},
};

// =====================================================================
//  Config, Commands, CVARS
// =====================================================================

static void InitData()
{
	if (!g_Tabs.empty())
		return;

	SpawnTab npcs;
	npcs.title = "NPCs";
	npcs.folder = "resource/imgui/npcs";
	npcs.toggles = {
		{"Ignore Players", "button_notarget_set"},
		{"No AI", "button_ai_set"},
		{"Reverse Relationship", "button_reverse_relationship"},
	};
	npcs.actions = {
		{"Undo", "remove_entity_undo"},
		{"Remove All", "remove_entities_all"},
		{"Front Spawn", "button_front_spawn"},
	};

	SpawnTab props;
	props.title = "Props";
	props.folder = "resource/imgui/props";
	props.toggles = {
		{"Ignore Players", "button_notarget_set"},
	};
	props.actions = {
		{"Undo", "remove_entity_undo"},
		{"Remove All", "remove_entities_all"},
		{"Front Spawn", "button_front_spawn"},
	};

	SpawnTab items;
	items.title = "Items";
	items.folder = "resource/imgui/items";
	items.toggles = {
		{"Give Mode", "button_self_pickup"},
	};
	items.actions = {
		{"Undo", "remove_entity_undo"},
		{"Remove All", "remove_entities_all"},
		{"Front Spawn", "button_front_spawn"},
	};

	SpawnTab sweps;
	sweps.title = "SWEPs";
	sweps.folder = "resource/imgui/sweps";
	sweps.toggles = {
		{"Give Mode", "button_self_pickup"},
	};
	sweps.actions = {
		{"Undo", "remove_entity_undo"},
		{"Remove All", "remove_entities_all"},
		{"Front Spawn", "button_front_spawn"},
	};

	g_Tabs.push_back(std::move(npcs));
	g_Tabs.push_back(std::move(props));
	g_Tabs.push_back(std::move(items));
	g_Tabs.push_back(std::move(sweps));

	// Tools: command = "tool <name>"
	g_ToolCats = {
		{"Common Tools",
			{
				{"No Tools", "tool none", "#Hint_tool_none"},
				{"Duplicator", "tool duplicator", "#Hint_tool_duplicator"},
				{"Remover", "tool remover", "#Hint_tool_remover"},
				{"Render", "tool render", "#Hint_tool_render"},
			}},
		{"Monster Manage",
			{
				{"Blood Color", "tool blood_color", "#Hint_tool_blood_color"},
				{"Frame Editor", "tool frame_set", "#Hint_tool_frame_set"},
				{"Health Modify", "tool health_set", "#Hint_tool_health_set"},
				{"Manipulator", "tool manipulator", "#Hint_tool_manipulator"},
				{"Model Editor", "tool model_editor", "#Hint_tool_model_edit"},
				{"No Colide", "tool no_collide", "#Hint_tool_no_colide"},
				{"Poser", "tool poser", "#Hint_tool_poser"},
				{"Scaler", "tool scaler", "#Hint_tool_scaler"},
				{"Spawner", "tool spawner", "#Hint_tool_spawner"},
				{"Take Damage", "tool take_damage", "#Hint_tool_take_damage"},
			}},
		{"Utilites",
			{
				{"Camera", "tool camera", "#Hint_tool_camera"},
				{"Gibber", "tool gibber", "#Hint_tool_gibber"},
				{"Glowsticks", "tool glowsticks", "#Hint_tool_glowsticks"},
				{"Teleporter", "tool teleporter", "#Hint_tool_teleporter"},
			}},
	};

	// Render Options:
	// command = "rendermode <name>"
	// command = "renderfx <name>"
	g_RenderToolCats = {
		{"Render Mode",
			{
				{"Normal", "rendermode normal", "#Hint_rendermode_normal"},
				{"Color", "rendermode color", "#Hint_rendermode_color"},
				{"Texture", "rendermode texture", "#Hint_rendermode_texture"},
				//{"Glow", "rendermode glow", "#Hint_rendermode_glow"},
				//{"Solid", "rendermode solid", "#Hint_rendermode_solid"},
				{"Additive", "rendermode additive", "#Hint_rendermode_additive"},
			}},
		{"Render FX",
			{
				{"Normal", "renderfx normal", "#Hint_renderfx_normal"},
				{"Slow Pulse", "renderfx slow_pulse", "#Hint_renderfx_slow_pulse"},
				{"Fast Pulse", "renderfx fast_pulse", "#Hint_renderfx_fast_pulse"},
				{"Slow Wide Pulse", "renderfx slow_wide_pulse", "#Hint_renderfx_slow_wide_pulse"},
				{"Fast Wide Pulse", "renderfx fast_wide_pulse", "#Hint_renderfx_fast_wide_pulse"},
				//{"Slow Fade Away", "renderfx slow_fade_away", "#Hint_renderfx_slow_fade_away"},
				//{"Fast Fade Away", "renderfx fast_fade_away", "#Hint_renderfx_fast_fade_away"},
				//{"Slow Become Solid", "renderfx slow_become_solid", "#Hint_renderfx_slow_become_solid"},
				//{"Fast Become Solid", "renderfx fast_become_solid", "#Hint_renderfx_fast_become_solid"},
				{"Slow Strobe", "renderfx slow_strobe", "#Hint_renderfx_slow_strobe"},
				{"Fast Strobe", "renderfx fast_strobe", "#Hint_renderfx_fast_strobe"},
				{"Faster Strobe", "renderfx faster_strobe", "#Hint_renderfx_faster_strobe"},
				{"Slow Flicker", "renderfx slow_flicker", "#Hint_renderfx_slow_flicker"},
				{"Fast Flicker", "renderfx fast_flicker", "#Hint_renderfx_fast_flicker"},
				//{"Constant Glow", "renderfx constant_glow", "#Hint_renderfx_constant_glow"},
				{"Distort", "renderfx distort", "#Hint_renderfx_distort"},
				{"Hologram", "renderfx hologram", "#Hint_renderfx_hologram"},
				{"Explode", "renderfx explode", "#Hint_renderfx_explode"},
				{"Glow Shell", "renderfx glow_shell", "#Hint_renderfx_glow_shell"},
			}},
	};
}

// =====================================================================
//  parse json (RapidJSON) - every tab reads ALL the .json files that are
//  inside its folder (resource/imgui/npcs/, props/, items/, sweps/).
//  Each file is one section (section_name + button_list).
// =====================================================================
// load ONE section (ej. resource/imgui/npcs/military_aliens.json)
// and adds it to the vector as another category 'out'.
static bool LoadSectionFile(const std::string& path, std::vector<MenuCategory>& out)
{
	std::string src;
	if (!LoadTextFile(path.c_str(), src))
	{
		gEngfuncs.Con_Printf("GoMod menu: the section %s could not be opened\n", path.c_str());
		return false;
	}

	rapidjson::Document doc;
	if (doc.Parse(src.c_str()).HasParseError() || !doc.IsObject())
	{
		gEngfuncs.Con_Printf("GoMod menu: invalid JSON in %s\n", path.c_str());
		return false;
	}

	MenuCategory cat;

	if (doc.HasMember("section_name") && doc["section_name"].IsString())
		cat.name = doc["section_name"].GetString();
	else
		cat.name = path; // fallback: if the name is missing, show the path.

	if (doc.HasMember("button_list") && doc["button_list"].IsObject())
	{
		const auto& buttons = doc["button_list"];

		for (auto it = buttons.MemberBegin(); it != buttons.MemberEnd(); ++it)
		{
			if (strcmp(it->name.GetString(), "button") != 0 || !it->value.IsObject())
				continue;

			const auto& btn = it->value;
			MenuItem item;

			if (btn.HasMember("name") && btn["name"].IsString())
				item.name = btn["name"].GetString();
			if (btn.HasMember("image") && btn["image"].IsString())
				item.image = btn["image"].GetString();
			if (btn.HasMember("command") && btn["command"].IsString())
				item.command = btn["command"].GetString();

			cat.items.push_back(std::move(item));
		}
	}

	out.push_back(std::move(cat));
	return true;
}

// Names of every .json file inside 'folder' (only the file names, not the full path).
// They are searched through ALL the engine search paths, so a pack dropped in an
// addon/downloads folder shows up too. The result is sorted alphabetically and has no
// duplicates (the engine returns the same file once per search path that has it),
// so the sections always appear in the same order: name the files "01_xxx.json",
// "02_xxx.json" if you want to choose that order.
static void ListJsonFilesInFolder(const char* folder, std::vector<std::string>& out)
{
	out.clear();

	if (!g_pFileSystem)
	{
		gEngfuncs.Con_Printf("GoMod menu: filesystem not available, cannot read %s/\n", folder);
		return;
	}

	std::set<std::string> found; // removes duplicates and keeps them sorted

	char pattern[256];
	snprintf(pattern, sizeof(pattern), "%s/*.json", folder);

	FileFindHandle_t handle;
	const char* name = g_pFileSystem->FindFirst(pattern, &handle, nullptr);

	// the handle is only valid if FindFirst found something, so no FindClose when it didn't.
	if (!name)
		return;

	for (; name; name = g_pFileSystem->FindNext(handle))
	{
		if (g_pFileSystem->FindIsDirectory(handle))
			continue;

		std::string file = name;

		// some filesystems may return a path, keep only the file name
		size_t pos = file.find_last_of("/\\");
		if (pos != std::string::npos)
			file = file.substr(pos + 1);

		found.insert(file);
	}

	g_pFileSystem->FindClose(handle);

	out.assign(found.begin(), found.end());
}

static void EnsureLoaded(SpawnTab& tab)
{
	if (tab.loaded)
		return;

	tab.loaded = true;
	tab.cats.clear();

	std::vector<std::string> files;
	ListJsonFilesInFolder(tab.folder, files);

	if (files.empty())
	{
		gEngfuncs.Con_Printf("GoMod menu: no sections found in %s/\n", tab.folder);
		return;
	}

	for (const std::string& file : files)
	{
		// if an individual section fails, only that one is skipped (the console says why).
		// the rest of the menu continues to load normally.
		LoadSectionFile(std::string(tab.folder) + "/" + file, tab.cats);
	}
}

// resource/gomod_gui.txt keys
static void EnsureHintStringsLoaded()
{
	if (g_KeyStringsLoaded)
		return;

	g_KeyStringsLoaded = true;
	g_KeyStrings.clear();

	std::string src;
	if (!LoadTextFile(KEY_STRINGS_FILE, src))
	{
		gEngfuncs.Con_Printf("GoMod menu: %s dont exist (the keys with # are not going to be resolved)\n", KEY_STRINGS_FILE);
		return;
	}

	const char* p = src.c_str();
	const char* end = p + src.size();

	auto SkipWhitespaceAndComments = [&]()
	{
		for (;;)
		{
			while (p < end && isspace((unsigned char)*p))
				p++;

			if (p + 1 < end && p[0] == '/' && p[1] == '/')
			{
				while (p < end && *p != '\n')
					p++;
				continue;
			}
			break;
		}
	};

	auto ReadQuoted = [&](std::string& out) -> bool
	{
		SkipWhitespaceAndComments();

		if (p >= end || *p != '"')
			return false;

		p++;
		out.clear();

		while (p < end && *p != '"')
		{
			if (*p == '\\' && p + 1 < end)
			{
				char next = p[1];
				if (next == 'n')
				{
					out.push_back('\n');
					p += 2;
					continue;
				}
				if (next == '"')
				{
					out.push_back('"');
					p += 2;
					continue;
				}
				if (next == '\\')
				{
					out.push_back('\\');
					p += 2;
					continue;
				}
			}
			out.push_back(*p++);
		}

		if (p < end)
			p++;

		return true;
	};

	std::string key, value;
	while (ReadQuoted(key))
	{
		if (!ReadQuoted(value))
		{
			gEngfuncs.Con_Printf("GoMod menu: %s - missing value of \"%s\"\n", KEY_STRINGS_FILE, key.c_str());
			break;
		}

		g_KeyStrings[key] = value;
	}

	gEngfuncs.Con_Printf("GoMod menu: %d keys loaded from %s\n", (int)g_KeyStrings.size(), KEY_STRINGS_FILE);
}

// read the key with '#' in resource/gomod_gui.txt
// if not used, raw text
static std::string ResolveKeyText(const char* key)
{
	if (!key || key[0] == '\0')
		return std::string();

	if (key[0] != '#')
		return std::string(key);

	EnsureHintStringsLoaded();

	auto it = g_KeyStrings.find(key + 1);
	if (it != g_KeyStrings.end())
		return it->second;

	return std::string(key); // not founded: show raw text
}

// =====================================================================
//  Textures
// =====================================================================

typedef void(APIENTRY* PFN_GenTextures)(GLsizei, GLuint*);
typedef void(APIENTRY* PFN_DeleteTextures)(GLsizei, const GLuint*);
typedef void(APIENTRY* PFN_BindTexture)(GLenum, GLuint);
typedef void(APIENTRY* PFN_TexParameteri)(GLenum, GLenum, GLint);
typedef void(APIENTRY* PFN_TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
typedef void(APIENTRY* PFN_GetIntegerv)(GLenum, GLint*);

static PFN_GenTextures pfnGenTextures;
static PFN_DeleteTextures pfnDeleteTextures;
static PFN_BindTexture pfnBindTexture;
static PFN_TexParameteri pfnTexParameteri;
static PFN_TexImage2D pfnTexImage2D;
static PFN_GetIntegerv pfnGetIntegerv;

static bool LoadGL()
{
	static int state = 0; // 0 = sin intentar, 1 = ok, -1 = fallo
	if (state != 0)
		return state > 0;

	pfnGenTextures = (PFN_GenTextures)SDL_GL_GetProcAddress("glGenTextures");
	pfnDeleteTextures = (PFN_DeleteTextures)SDL_GL_GetProcAddress("glDeleteTextures");
	pfnBindTexture = (PFN_BindTexture)SDL_GL_GetProcAddress("glBindTexture");
	pfnTexParameteri = (PFN_TexParameteri)SDL_GL_GetProcAddress("glTexParameteri");
	pfnTexImage2D = (PFN_TexImage2D)SDL_GL_GetProcAddress("glTexImage2D");
	pfnGetIntegerv = (PFN_GetIntegerv)SDL_GL_GetProcAddress("glGetIntegerv");

	const bool ok = pfnGenTextures && pfnDeleteTextures && pfnBindTexture &&
					pfnTexParameteri && pfnTexImage2D && pfnGetIntegerv;

	state = ok ? 1 : -1;
	return ok;
}

static GLuint LoadTexture(const char* path)
{
	if (!LoadGL())
		return 0;

	int len = 0;
	byte* file = gEngfuncs.COM_LoadFile((char*)path, 5, &len);

	if (!file)
	{
		gEngfuncs.Con_Printf("GoMod menu: image not found: %s\n", path);
		return 0;
	}

	int w = 0, h = 0, comp = 0;
	unsigned char* pixels = stbi_load_from_memory((const unsigned char*)file, len, &w, &h, &comp, 4);
	gEngfuncs.COM_FreeFile(file);

	if (!pixels)
	{
		gEngfuncs.Con_Printf("GoMod menu: invalid PNG: %s\n", path);
		return 0;
	}

	// do not break the engine's GL state: restore the previous binding
	GLint lastTexture = 0;
	pfnGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexture);

	GLuint tex = 0;
	pfnGenTextures(1, &tex);
	pfnBindTexture(GL_TEXTURE_2D, tex);
	pfnTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	pfnTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	pfnTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	pfnTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	pfnTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	pfnBindTexture(GL_TEXTURE_2D, (GLuint)lastTexture);

	stbi_image_free(pixels);
	return tex;
}

static void FreeAllTextures()
{
	for (SpawnTab& tab : g_Tabs)
	{
		for (MenuCategory& cat : tab.cats)
		{
			for (MenuItem& item : cat.items)
			{
				if (item.tex && pfnDeleteTextures)
					pfnDeleteTextures(1, &item.tex);
				item.tex = 0;
				item.triedLoad = false;
			}
		}
	}
}

// =====================================================================
//  Widgets
// =====================================================================

// Square button with an image and the name overlaid at the bottom (like in GMod)
static bool ThumbButton(MenuItem& item, const ImVec2& size, int id)
{
	if (!item.triedLoad)
	{
		item.triedLoad = true;
		if (!item.image.empty())
			item.tex = LoadTexture(item.image.c_str());
	}

	ImGui::PushID(id);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

	bool clicked;
	if (item.tex != 0)
		clicked = ImGui::ImageButton("##btn", (ImTextureID)(intptr_t)item.tex, size, ImVec2(0, 0), ImVec2(1, 1));
	else
		clicked = ImGui::Button("##btn", size);

	ImGui::PopStyleVar();
	ImGui::PopID();

	const ImVec2 mn = ImGui::GetItemRectMin();
	const ImVec2 mx = ImGui::GetItemRectMax();
	const float barH = ImGui::GetTextLineHeight() + 4.0f;

	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(ImVec2(mn.x + 1, mx.y - barH - 1), ImVec2(mx.x - 1, mx.y - 1), IM_COL32(0, 0, 0, 150));

	const ImVec2 ts = ImGui::CalcTextSize(item.name.c_str());
	float tx = mn.x + (size.x - ts.x) * 0.5f;
	if (tx < mn.x + 2.0f)
		tx = mn.x + 2.0f;

	dl->PushClipRect(ImVec2(mn.x + 1, mx.y - barH - 1), ImVec2(mx.x - 1, mx.y - 1), true);
	dl->AddText(ImVec2(tx, mx.y - barH + 1.0f), IM_COL32_WHITE, item.name.c_str());
	dl->PopClipRect();

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", item.name.c_str());

	return clicked;
}

// =====================================================================
//  Left Window: spawn menu
// =====================================================================

static void DrawSpawnTab(SpawnTab& tab)
{
	EnsureLoaded(tab);

	const ImGuiStyle& style = ImGui::GetStyle();

	int thumbW = 84, thumbH = 84;
	ScaleSize(thumbW, thumbH);
	const ImVec2 thumb((float)thumbW, (float)thumbH);

	int btnW = 120, btnH = 22;
	ScaleSize(btnW, btnH);

	int toggleW = 130, toggleH = 22;
	ScaleSize(toggleW, toggleH);

	const float rowH = ((float)toggleH > (float)btnH ? (float)toggleH : (float)btnH) + style.ItemSpacing.y;
	const size_t rows = tab.toggles.size() > tab.actions.size() ? tab.toggles.size() : tab.actions.size();
	const float footerH = rows * rowH + style.WindowPadding.y * 2.0f + style.ItemSpacing.y;

	// ---- Selection Lists ----
	ImGui::BeginChild("##list", ImVec2(0, -footerH), ImGuiChildFlags_Borders);

	if (tab.cats.empty())
		ImGui::TextDisabled("witouth entries. check %s/", tab.folder);

	for (size_t c = 0; c < tab.cats.size(); c++)
	{
		MenuCategory& cat = tab.cats[c];

		ImGui::PushID((int)c);

		if (ImGui::CollapsingHeader(cat.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			const float avail = ImGui::GetContentRegionAvail().x;
			const float spacing = style.ItemSpacing.x;
			float used = 0.0f;

			for (size_t i = 0; i < cat.items.size(); i++)
			{
				if (i > 0)
				{
					if (used + spacing + thumb.x <= avail)
					{
						ImGui::SameLine();
						used += spacing;
					}
					else
					{
						used = 0.0f;
					}
				}

				MenuItem& item = cat.items[i];

				if (ThumbButton(item, thumb, (int)i) && !item.command.empty())
				{
					RunCmd(item.command.c_str());
					gEngfuncs.pfnPlaySoundByName("!MI_SENTENC5", 1.0f);
				}

				used += thumb.x;
			}

			ImGui::Spacing();
		}

		ImGui::PopID();
	}

	ImGui::EndChild();

	// ---- checkboxes + buttons ----
	ImGui::BeginChild("##footer", ImVec2(0, 0), ImGuiChildFlags_Borders);

	ImGui::BeginGroup();
	for (const Toggle& t : tab.toggles)
	{
		if (ImGui::Button(t.label, ImVec2((float)toggleW, (float)toggleH)))
			RunCmd(t.command);
	}
	ImGui::EndGroup();

	ImGui::SameLine(ImGui::GetWindowWidth() - (float)btnW - style.WindowPadding.x);

	ImGui::BeginGroup();
	for (const Action& a : tab.actions)
	{
		if (ImGui::Button(a.label, ImVec2((float)btnW, (float)btnH)))
			RunCmd(a.command);
	}
	ImGui::EndGroup();

	ImGui::EndChild();
}

static void DrawSpawnWindow(int x, int y, int w, int h)
{
	ImGui::SetNextWindowPos(ImVec2((float)x, (float)y), ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(ImVec2((float)w, (float)h), ImGuiCond_Appearing);

	// the X close all the menu
	ImGui::Begin("Go-Mod Spawn Menu", &g_bShowMenu, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

	if (ImGui::BeginTabBar("##spawntabs"))
	{
		for (size_t t = 0; t < g_Tabs.size(); t++)
		{
			if (ImGui::BeginTabItem(g_Tabs[t].title))
			{
				ImGui::PushID((int)t);
				DrawSpawnTab(g_Tabs[t]);
				ImGui::PopID();
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
}

// =====================================================================
//  Right Window: Tools + Render Options + Customization + Color Settings
// =====================================================================

static void DrawToolCategoryList(std::vector<ToolCategory>& cats, const ToolDef*& activeTool)
{
	ImGui::BeginChild("##toollist", ImVec2(ImGui::GetContentRegionAvail().x * 0.45f, 0), ImGuiChildFlags_Borders);

	for (size_t c = 0; c < cats.size(); c++)
	{
		ImGui::PushID((int)c);

		if (ImGui::CollapsingHeader(cats[c].name, ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (const ToolDef& tool : cats[c].tools)
			{
				if (ImGui::Selectable(tool.name, activeTool == &tool))
				{
					activeTool = &tool;
					RunCmd(tool.command);
				}
			}
		}

		ImGui::PopID();
	}

	ImGui::EndChild();
	ImGui::SameLine();

	// Active tool options panel (currently just the name)
	ImGui::BeginChild("##tooloptions", ImVec2(0, 0), ImGuiChildFlags_Borders);

	if (activeTool)
	{
		if (activeTool->hint && activeTool->hint[0] != '\0')
		{
			std::string text = ResolveKeyText(activeTool->hint);
			ImGui::TextWrapped("%s", text.c_str());
		}
		else
		{
			std::string lowerName = activeTool->name;
			for (char& ch : lowerName)
				ch = (char)tolower((unsigned char)ch);

			ImGui::TextWrapped("hint of %s", lowerName.c_str());
		}
	}
	else
		ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1.00f), "Select a Tool");

	ImGui::EndChild();
}

static void DrawToolsTab()
{
	static const ToolDef* s_pActive = nullptr;
	DrawToolCategoryList(g_ToolCats, s_pActive);
}

static void DrawRenderOptionsTab()
{
	static const ToolDef* s_pActive = nullptr;
	DrawToolCategoryList(g_RenderToolCats, s_pActive);
}

static void DrawCustomizationTab()
{
	static int s_voiceIndex = 0;
	static int s_flashlightIndex = 0;

	ImGui::BeginChild("##customization", ImVec2(0, 0), ImGuiChildFlags_None);

	// ---- Player Voice (hardcoded) ----
	ImGui::Text("Player Voice");
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	if (ImGui::BeginCombo("##playervoice", g_VoiceOptions[s_voiceIndex].label))
	{
		for (int i = 0; i < (int)(sizeof(g_VoiceOptions) / sizeof(g_VoiceOptions[0])); i++)
		{
			bool selected = (i == s_voiceIndex);
			if (ImGui::Selectable(g_VoiceOptions[i].label, selected))
				s_voiceIndex = i;
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// ---- Player Flashlight (hardcoded) ----
	ImGui::Text("Flashlight Type");
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	if (ImGui::BeginCombo("##flashlighttype", g_FlashLightOptions[s_flashlightIndex].label))
	{
		for (int i = 0; i < (int)(sizeof(g_FlashLightOptions) / sizeof(g_FlashLightOptions[0])); i++)
		{
			bool selected = (i == s_flashlightIndex);
			if (ImGui::Selectable(g_FlashLightOptions[i].label, selected))
				s_flashlightIndex = i;
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	int btnW = 200, btnH = 30;
	ScaleSize(btnW, btnH);
	ImGui::SetCursorPosX((ImGui::GetWindowSize().x - (float)btnW) * 0.5f);

	if (ImGui::Button("Apply Changes", ImVec2((float)btnW, (float)btnH)))
	{
		char buf[64];

		snprintf(buf, sizeof(buf), "cl_player_sfx_type %s", g_VoiceOptions[s_voiceIndex].value);
		RunCmd(buf);

		snprintf(buf, sizeof(buf), "toggle_flashlight_mode %d", g_FlashLightOptions[s_flashlightIndex].value);
		RunCmd(buf);
	}

	ImGui::EndChild();
}

static void DrawColorSettingsTab()
{
	ImGui::BeginChild("##colorsettings", ImVec2(0, 0), ImGuiChildFlags_None);

	ImGui::Text("Pick a color:");
	ImGui::Separator();

	static float miColor[3] = {1.0f, 1.0f, 1.0f}; // RGB

	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	ImGui::ColorPicker3("##picker", miColor, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	int ButtonWidth = 220;
	int ButtonHeight = 30;
	ScaleSize(ButtonWidth, ButtonHeight);

	auto ApplyColorCmd = [&](const char* cvar)
	{
		int r = (int)(miColor[0] * 255.0f);
		int g = (int)(miColor[1] * 255.0f);
		int b = (int)(miColor[2] * 255.0f);

		char cmdBuffer[64];
		snprintf(cmdBuffer, sizeof(cmdBuffer), "%s %d %d %d", cvar, r, g, b);
		RunCmd(cmdBuffer);
	};

	{
		const float avail = ImGui::GetContentRegionAvail().x;
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float btnW = (avail - 2.0f * spacing) / 3.0f;

		if (ImGui::Button("Apply for HUD", ImVec2(btnW, (float)ButtonHeight)))
			ApplyColorCmd("hud_color");

		ImGui::SameLine();
		if (ImGui::Button("Apply HUD Critical", ImVec2(btnW, (float)ButtonHeight)))
			ApplyColorCmd("hud_color_critical");

		ImGui::SameLine();
		if (ImGui::Button("Apply FX Color", ImVec2(btnW, (float)ButtonHeight)))
			ApplyColorCmd("render_color");
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	{
		static int s_renderAmount = 255;

		int applyBtnW = 170, applyBtnH = 30;
		ScaleSize(applyBtnW, applyBtnH);

		const float avail = ImGui::GetContentRegionAvail().x;
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		float sliderW = avail - (float)applyBtnW - spacing;
		if (sliderW < 50.0f)
			sliderW = 50.0f;

		ImGui::SetNextItemWidth(sliderW);
		ImGui::SliderInt("##render_amount", &s_renderAmount, 0, 255, "Render Amount: %d");

		ImGui::SameLine();
		if (ImGui::Button("Apply FX Amount", ImVec2((float)applyBtnW, (float)applyBtnH)))
		{
			char cmdBuffer[64];
			snprintf(cmdBuffer, sizeof(cmdBuffer), "render_amount %d", s_renderAmount);
			RunCmd(cmdBuffer);
		}
	}

	ImGui::EndChild();
}

static void DrawToolsWindow(int x, int y, int w, int h)
{
	ImGui::SetNextWindowPos(ImVec2((float)x, (float)y), ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(ImVec2((float)w, (float)h), ImGuiCond_Appearing);

	ImGui::Begin("Go-Mod Tools", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

	if (ImGui::BeginTabBar("##toolstabs"))
	{
		if (ImGui::BeginTabItem("Tools"))
		{
			DrawToolsTab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Render Options"))
		{
			DrawRenderOptionsTab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Customization"))
		{
			DrawCustomizationTab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Color Settings"))
		{
			DrawColorSettingsTab();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}

// =====================================================================
//  public API
// =====================================================================

// Call Between ImGui::NewFrame() and ImGui::Render(), only with the menu opened
void GoModMenu_Draw()
{
	InitData();

	const int leftW = (int)(ScreenWidth * 0.46f);
	const int rightW = (int)(ScreenWidth * 0.30f);
	const int gap = (int)(ScreenWidth * 0.01f);
	const int h = (int)(ScreenHeight * 0.72f);

	const int x0 = (ScreenWidth - (leftW + gap + rightW)) / 2;
	const int y0 = (ScreenHeight - h) / 2;

	DrawSpawnWindow(x0, y0, leftW, h);
	DrawToolsWindow(x0 + leftW + gap, y0, rightW, h);
}

// command "gomod_menu_reload": read again txt and images
void GoModMenu_Reload()
{
	FreeAllTextures();

	for (SpawnTab& tab : g_Tabs)
	{
		tab.cats.clear();
		tab.loaded = false;
	}

	g_KeyStrings.clear();
	g_KeyStringsLoaded = false;
}

void GoModMenu_Shutdown()
{
	FreeAllTextures();
	g_Tabs.clear();
	g_ToolCats.clear();
}