// gomod_menu.cpp
// Garry's Mod 11 Styled Menu for Go-Mod: Reborn (Dear ImGui).

#include "hud.h"
#include "cl_util.h"
#include "filesystem_utils.h"

#include "imgui.h"
#include "gm_audio.h"
#include "parsemsg.h"

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
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#include "../external/rapidjson/document.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

extern bool g_bShowMenu;
extern void ImGuiMenu_SetOpen(bool open);
extern bool UTIL_IsSandbox();

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
	{"#Gomod_Voice_hevsuit", "hevsuit"},
	{"#Gomod_Voice_dmc", "dmc"},
	{"#Gomod_Voice_cstrike", "cstrike"},
	{"#Gomod_Voice_hlalpha", "hlalpha"},
	{"#Gomod_Voice_barney", "barney"},
	{"#Gomod_Voice_hgrunt", "hgrunt"},
	{"#Gomod_Voice_scientist", "scientist"},
};

static const FlashLightOption g_FlashLightOptions[] = {
	{"#Gomod_Flashlight_Flashlight", 0},
	{"#Gomod_Flashlight_NightVisionOP4", 1},
	{"#Gomod_Flashlight_NightVisionCS", 2},
	{"#Gomod_Flashlight_NightVisionCustom", 3},
};

// =====================================================================
//  Config, Commands, CVARS
// =====================================================================

static void InitData()
{
	if (!g_Tabs.empty())
		return;

	SpawnTab npcs;
	npcs.title = "#Gomod_Menu_Tab_NPCs";
	npcs.folder = "resource/imgui/npcs";
	npcs.toggles = {
		{"#Gomod_Menu_IgnorePlayers", "button_notarget_set"},
		{"#Gomod_Menu_NoAI", "button_ai_set"},
		{"#Gomod_Menu_ReverseRelationship", "button_reverse_relationship"},
	};
	npcs.actions = {
		{"#Gomod_Menu_Undo", "remove_entity_undo"},
		{"#Gomod_Menu_RemoveAll", "remove_entities_all"},
		{"#Gomod_Menu_FrontSpawn", "button_front_spawn"},
	};

	SpawnTab props;
	props.title = "#Gomod_Menu_Tab_Props";
	props.folder = "resource/imgui/props";
	props.toggles = {
		{"#Gomod_Menu_IgnorePlayers", "button_notarget_set"},
	};
	props.actions = {
		{"#Gomod_Menu_Undo", "remove_entity_undo"},
		{"#Gomod_Menu_RemoveAll", "remove_entities_all"},
		{"#Gomod_Menu_FrontSpawn", "button_front_spawn"},
	};

	SpawnTab items;
	items.title = "#Gomod_Menu_Tab_Items";
	items.folder = "resource/imgui/items";
	items.toggles = {
		{"#Gomod_Menu_GiveMode", "button_self_pickup"},
	};
	items.actions = {
		{"#Gomod_Menu_Undo", "remove_entity_undo"},
		{"#Gomod_Menu_RemoveAll", "remove_entities_all"},
		{"#Gomod_Menu_FrontSpawn", "button_front_spawn"},
	};

	SpawnTab sweps;
	sweps.title = "#Gomod_Menu_Tab_SWEPs";
	sweps.folder = "resource/imgui/sweps";
	sweps.toggles = {
		{"#Gomod_Menu_GiveMode", "button_self_pickup"},
	};
	sweps.actions = {
		{"#Gomod_Menu_Undo", "remove_entity_undo"},
		{"#Gomod_Menu_RemoveAll", "remove_entities_all"},
		{"#Gomod_Menu_FrontSpawn", "button_front_spawn"},
	};

	g_Tabs.push_back(std::move(npcs));
	g_Tabs.push_back(std::move(props));
	g_Tabs.push_back(std::move(items));
	g_Tabs.push_back(std::move(sweps));

	// Tools: command = "tool <name>"
	g_ToolCats = {
		{"#Gomod_Menu_Cat_CommonTools",
			{
				{"#Gomod_Tool_none", "tool none", "#Hint_tool_none"},
				{"#Gomod_Tool_duplicator", "tool duplicator", "#Hint_tool_duplicator"},
				{"#Gomod_Tool_remover", "tool remover", "#Hint_tool_remover"},
				{"#Gomod_Tool_render", "tool render", "#Hint_tool_render"},
			}},
		{"#Gomod_Menu_Cat_MonsterManage",
			{
				{"#Gomod_Tool_blood_color", "tool blood_color", "#Hint_tool_blood_color"},
				{"#Gomod_Tool_frame_set", "tool frame_set", "#Hint_tool_frame_set"},
				{"#Gomod_Tool_health_set", "tool health_set", "#Hint_tool_health_set"},
				{"#Gomod_Tool_manipulator", "tool manipulator", "#Hint_tool_manipulator"},
				{"#Gomod_Tool_model_editor", "tool model_editor", "#Hint_tool_model_edit"},
				{"#Gomod_Tool_no_collide", "tool no_collide", "#Hint_tool_no_colide"},
				{"#Gomod_Tool_poser", "tool poser", "#Hint_tool_poser"},
				{"#Gomod_Tool_scaler", "tool scaler", "#Hint_tool_scaler"},
				{"#Gomod_Tool_spawner", "tool spawner", "#Hint_tool_spawner"},
				{"#Gomod_Tool_take_damage", "tool take_damage", "#Hint_tool_take_damage"},
			}},
		{"#Gomod_Menu_Cat_Utilities",
			{
				{"#Gomod_Tool_camera", "tool camera", "#Hint_tool_camera"},
				{"#Gomod_Tool_gibber", "tool gibber", "#Hint_tool_gibber"},
				{"#Gomod_Tool_glowsticks", "tool glowsticks", "#Hint_tool_glowsticks"},
				{"#Gomod_Tool_teleporter", "tool teleporter", "#Hint_tool_teleporter"},
			}},
	};

	// Render Options:
	// command = "rendermode <name>"
	// command = "renderfx <name>"
	g_RenderToolCats = {
		{"#Gomod_Menu_Cat_RenderMode",
			{
				{"#Gomod_RenderMode_normal", "rendermode normal", "#Hint_rendermode_normal"},
				{"#Gomod_RenderMode_color", "rendermode color", "#Hint_rendermode_color"},
				{"#Gomod_RenderMode_texture", "rendermode texture", "#Hint_rendermode_texture"},
				//{"#Gomod_RenderMode_glow", "rendermode glow", "#Hint_rendermode_glow"},
				//{"#Gomod_RenderMode_solid", "rendermode solid", "#Hint_rendermode_solid"},
				{"#Gomod_RenderMode_additive", "rendermode additive", "#Hint_rendermode_additive"},
			}},
		{"#Gomod_Menu_Cat_RenderFX",
			{
				{"#Gomod_RenderFX_normal", "renderfx normal", "#Hint_renderfx_normal"},
				{"#Gomod_RenderFX_slow_pulse", "renderfx slow_pulse", "#Hint_renderfx_slow_pulse"},
				{"#Gomod_RenderFX_fast_pulse", "renderfx fast_pulse", "#Hint_renderfx_fast_pulse"},
				{"#Gomod_RenderFX_slow_wide_pulse", "renderfx slow_wide_pulse", "#Hint_renderfx_slow_wide_pulse"},
				{"#Gomod_RenderFX_fast_wide_pulse", "renderfx fast_wide_pulse", "#Hint_renderfx_fast_wide_pulse"},
				//{"#Gomod_RenderFX_slow_fade_away", "renderfx slow_fade_away", "#Hint_renderfx_slow_fade_away"},
				//{"#Gomod_RenderFX_fast_fade_away", "renderfx fast_fade_away", "#Hint_renderfx_fast_fade_away"},
				//{"#Gomod_RenderFX_slow_become_solid", "renderfx slow_become_solid", "#Hint_renderfx_slow_become_solid"},
				//{"#Gomod_RenderFX_fast_become_solid", "renderfx fast_become_solid", "#Hint_renderfx_fast_become_solid"},
				{"#Gomod_RenderFX_slow_strobe", "renderfx slow_strobe", "#Hint_renderfx_slow_strobe"},
				{"#Gomod_RenderFX_fast_strobe", "renderfx fast_strobe", "#Hint_renderfx_fast_strobe"},
				{"#Gomod_RenderFX_faster_strobe", "renderfx faster_strobe", "#Hint_renderfx_faster_strobe"},
				{"#Gomod_RenderFX_slow_flicker", "renderfx slow_flicker", "#Hint_renderfx_slow_flicker"},
				{"#Gomod_RenderFX_fast_flicker", "renderfx fast_flicker", "#Hint_renderfx_fast_flicker"},
				//{"#Gomod_RenderFX_constant_glow", "renderfx constant_glow", "#Hint_renderfx_constant_glow"},
				{"#Gomod_RenderFX_distort", "renderfx distort", "#Hint_renderfx_distort"},
				{"#Gomod_RenderFX_hologram", "renderfx hologram", "#Hint_renderfx_hologram"},
				{"#Gomod_RenderFX_explode", "renderfx explode", "#Hint_renderfx_explode"},
				{"#Gomod_RenderFX_glow_shell", "renderfx glow_shell", "#Hint_renderfx_glow_shell"},
			}},
	};
}

// =====================================================================
//  parse json (RapidJSON) - every tab reads ALL the .json files that are
//  inside its folder (resource/imgui/npcs/, props/, items/, sweps/).
//  Each file is one section (section_name + button_list).
// =====================================================================
static std::string ResolveKeyText(const char* key); // below: "#Key" -> resource/gomod_gui.txt

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

	// "#Key" -> resource/gomod_gui.txt, plain text works too
	if (doc.HasMember("section_name") && doc["section_name"].IsString())
		cat.name = ResolveKeyText(doc["section_name"].GetString());
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
				item.name = ResolveKeyText(btn["name"].GetString());
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

// Same as ResolveKeyText, but returns a pointer (for ImGui). Text without '#' is returned as it is.
// The pointer is valid until the strings are reloaded (gomod_menu_reload), so use it right away.
static const char* L(const char* key)
{
	if (!key)
		return "";

	if (key[0] != '#')
		return key;

	EnsureHintStringsLoaded();

	auto it = g_KeyStrings.find(key + 1);
	if (it != g_KeyStrings.end())
		return it->second.c_str();

	return key; // not found: raw text
}

// Translated label with a fixed ImGui ID ("text###key"): the ID doesn't change with the language
static std::string LID(const char* key)
{
	std::string label = L(key);
	label += "###";
	label += key ? key : "";
	return label;
}

// For printf style formats: '%' in a translated text must not be read as a format specifier
static std::string EscapePercent(const char* text)
{
	std::string out;
	for (const char* p = text; p && *p; p++)
	{
		out.push_back(*p);
		if (*p == '%')
			out.push_back('%');
	}
	return out;
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

// creates the texture (tex == 0) or updates it (same texture, new pixels)
static GLuint UploadTexture(GLuint tex, const unsigned char* rgba, int w, int h)
{
	if (!LoadGL())
		return 0;

	// do not break the engine's GL state: restore the previous binding
	GLint lastTexture = 0;
	pfnGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexture);

	if (tex == 0)
		pfnGenTextures(1, &tex);

	pfnBindTexture(GL_TEXTURE_2D, tex);
	pfnTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	pfnTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	pfnTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	pfnTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	pfnTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	pfnBindTexture(GL_TEXTURE_2D, (GLuint)lastTexture);

	return tex;
}

static void DeleteTexture(GLuint& tex)
{
	if (tex && pfnDeleteTextures)
		pfnDeleteTextures(1, &tex);
	tex = 0;
}

// for the other ImGui parts of the client (scoreboard avatars)
unsigned int GoModUI_UploadTexture(unsigned int tex, const unsigned char* rgba, int w, int h)
{
	return UploadTexture((GLuint)tex, rgba, w, h);
}

void GoModUI_DeleteTexture(unsigned int& tex)
{
	GLuint t = (GLuint)tex;
	DeleteTexture(t);
	tex = 0;
}

static GLuint LoadTexture(const char* path, int* outW = nullptr, int* outH = nullptr)
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

	const GLuint tex = UploadTexture(0, pixels, w, h);
	stbi_image_free(pixels);

	if (outW)
		*outW = w;
	if (outH)
		*outH = h;

	return tex;
}

// images of the menu itself (icons, previews): loaded once, by path
struct UiImage
{
	GLuint tex = 0;
	int w = 0;
	int h = 0;
	bool tried = false;
};

static std::unordered_map<std::string, UiImage> g_UiImages;

static const UiImage& GetUiImage(const char* path)
{
	UiImage& img = g_UiImages[path];
	if (!img.tried)
	{
		img.tried = true;
		img.tex = LoadTexture(path, &img.w, &img.h);
	}
	return img;
}

static void FreePlayerPreview(); // below (User Settings)

static void FreeAllTextures()
{
	for (SpawnTab& tab : g_Tabs)
	{
		for (MenuCategory& cat : tab.cats)
		{
			for (MenuItem& item : cat.items)
			{
				DeleteTexture(item.tex);
				item.triedLoad = false;
			}
		}
	}

	for (auto& it : g_UiImages)
		DeleteTexture(it.second.tex);
	g_UiImages.clear();

	FreePlayerPreview();
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
		ImGui::TextDisabled("%s %s/", L("#Gomod_Menu_NoEntries"), tab.folder);

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
					GMAudio_PlayLocal("gomod/button_new.wav", 1.0f);
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
		if (ImGui::Button(LID(t.label).c_str(), ImVec2((float)toggleW, (float)toggleH)))
			RunCmd(t.command);
	}
	ImGui::EndGroup();

	ImGui::SameLine(ImGui::GetWindowWidth() - (float)btnW - style.WindowPadding.x);

	ImGui::BeginGroup();
	for (const Action& a : tab.actions)
	{
		if (ImGui::Button(LID(a.label).c_str(), ImVec2((float)btnW, (float)btnH)))
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
			if (ImGui::BeginTabItem(LID(g_Tabs[t].title).c_str()))
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

		if (ImGui::CollapsingHeader(LID(cats[c].name).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (const ToolDef& tool : cats[c].tools)
			{
				if (ImGui::Selectable(LID(tool.name).c_str(), activeTool == &tool))
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
			ImGui::TextWrapped("%s %s", L("#Gomod_Menu_NoHint"), L(activeTool->name));
		}
	}
	else
		ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1.00f), "%s", L("#Gomod_Menu_SelectTool"));

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


// =====================================================================
//  Small helpers for the settings
// =====================================================================

// the menu was just opened (or this part was not drawn for a while): read the cvars again.
// ImGui frames only run with the menu open, so the real time is what tells it was closed.
struct RefreshTimer
{
	double last = -100.0;

	bool Check()
	{
		const double now = SDL_GetTicks() / 1000.0;
		const bool refresh = (now - last) > 0.25;
		last = now;
		return refresh;
	}
};

static int CvarInt(const char* name)
{
	return (int)gEngfuncs.pfnGetCvarFloat(name);
}

static void CvarColor(const char* prefix, int rgb[3])
{
	char name[64];
	static const char* const suffix[3] = {"_r", "_g", "_b"};
	for (int i = 0; i < 3; i++)
	{
		snprintf(name, sizeof(name), "%s%s", prefix, suffix[i]);
		rgb[i] = CvarInt(name);
		if (rgb[i] < 0)
			rgb[i] = 0;
		if (rgb[i] > 255)
			rgb[i] = 255;
	}
}

static ImVec4 ToImColor(const int rgb[3])
{
	return ImVec4(rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f, 1.0f);
}

static ImU32 ToImU32(const int rgb[3], int alpha = 255)
{
	return IM_COL32(rgb[0], rgb[1], rgb[2], alpha);
}

static void CenteredButtonPos(float width)
{
	const float x = (ImGui::GetContentRegionAvail().x - width) * 0.5f;
	if (x > 0.0f)
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x);
}

static void SectionTitle(const char* key)
{
	ImGui::Spacing();
	ImGui::SeparatorText(L(key));
}

// =====================================================================
//  Color window: a separate window with the color picker, "Done" closes it.
//  Done with a command  -> "<command> r g b" is executed (hud_color, physgun_color...)
//  Done with a store    -> the color is only saved there (fog / rain / snow, they have
//                          their own Apply button)
// =====================================================================

struct ColorDialog
{
	bool requestOpen = false;
	const char* titleKey = "";
	std::string command;
	int* store = nullptr;
	float color[3] = {1.0f, 1.0f, 1.0f};
	float original[3] = {1.0f, 1.0f, 1.0f};
};

static ColorDialog g_ColorDlg;

static void OpenColorDialog(const char* titleKey, const char* command, const int rgb[3], int* store = nullptr)
{
	g_ColorDlg.requestOpen = true;
	g_ColorDlg.titleKey = titleKey;
	g_ColorDlg.command = command ? command : "";
	g_ColorDlg.store = store;

	for (int i = 0; i < 3; i++)
		g_ColorDlg.color[i] = g_ColorDlg.original[i] = rgb[i] / 255.0f;
}

// at the root of the frame (outside the other windows), so the popup ID is always the same
static void DrawColorDialog()
{
	static const char* const kPopupID = "###gomod_color_dialog";

	if (g_ColorDlg.requestOpen)
	{
		g_ColorDlg.requestOpen = false;
		ImGui::OpenPopup(kPopupID);
	}

	int dlgW = 360, dlgH = 420;
	ScaleSize(dlgW, dlgH);

	ImGui::SetNextWindowPos(ImVec2(ScreenWidth * 0.5f, ScreenHeight * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2((float)dlgW, 0.0f), ImGuiCond_Appearing);

	const std::string title = std::string(L(g_ColorDlg.titleKey)) + kPopupID;

	if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
		return;

	ImGui::SetNextItemWidth((float)dlgW - ImGui::GetStyle().WindowPadding.x * 2.0f);
	ImGui::ColorPicker3("##dialogpicker", g_ColorDlg.color,
		ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB);

	// old / new color
	{
		const float swatchH = ImGui::GetFrameHeight() * 1.2f;
		const float half = ((float)dlgW - ImGui::GetStyle().WindowPadding.x * 2.0f - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

		ImGui::ColorButton("##old", ImVec4(g_ColorDlg.original[0], g_ColorDlg.original[1], g_ColorDlg.original[2], 1.0f),
			ImGuiColorEditFlags_NoTooltip, ImVec2(half, swatchH));
		ImGui::SameLine();
		ImGui::ColorButton("##new", ImVec4(g_ColorDlg.color[0], g_ColorDlg.color[1], g_ColorDlg.color[2], 1.0f),
			ImGuiColorEditFlags_NoTooltip, ImVec2(half, swatchH));
	}

	ImGui::Spacing();

	int btnW = 120, btnH = 28;
	ScaleSize(btnW, btnH);

	CenteredButtonPos((float)btnW * 2.0f + ImGui::GetStyle().ItemSpacing.x);

	if (ImGui::Button(LID("#Gomod_Color_Done").c_str(), ImVec2((float)btnW, (float)btnH)))
	{
		int rgb[3];
		for (int i = 0; i < 3; i++)
		{
			rgb[i] = (int)(g_ColorDlg.color[i] * 255.0f + 0.5f);
			if (rgb[i] < 0)
				rgb[i] = 0;
			if (rgb[i] > 255)
				rgb[i] = 255;
		}

		if (!g_ColorDlg.command.empty())
		{
			char buf[96];
			snprintf(buf, sizeof(buf), "%s %d %d %d", g_ColorDlg.command.c_str(), rgb[0], rgb[1], rgb[2]);
			RunCmd(buf);
		}

		if (g_ColorDlg.store)
		{
			for (int i = 0; i < 3; i++)
				g_ColorDlg.store[i] = rgb[i];
		}

		GMAudio_PlayLocal("gomod/button_new.wav", 1.0f);
		ImGui::CloseCurrentPopup();
	}

	ImGui::SameLine();

	if (ImGui::Button(LID("#Gomod_Color_Cancel").c_str(), ImVec2((float)btnW, (float)btnH)))
		ImGui::CloseCurrentPopup();

	ImGui::EndPopup();
}

// [color square] [button text]: both open the color window
static bool ColorChangeButton(const char* id, const char* textKey, const int rgb[3], bool fullWidth = true)
{
	ImGui::PushID(id);

	const float h = ImGui::GetFrameHeight();
	bool clicked = ImGui::ColorButton("##swatch", ToImColor(rgb), ImGuiColorEditFlags_NoTooltip, ImVec2(h * 1.6f, h));

	ImGui::SameLine();

	const float w = fullWidth ? ImGui::GetContentRegionAvail().x : 0.0f;
	if (ImGui::Button(LID(textKey).c_str(), ImVec2(w, h)))
		clicked = true;

	ImGui::PopID();
	return clicked;
}

// =====================================================================
//  Tools window: Color and Amount
// =====================================================================

static void DrawColorAndAmountTab()
{
	// render_color is a server command (the render tool), the client can't read it back:
	// the square shows the last color chosen here
	static int s_fxColor[3] = {255, 255, 255};
	static int s_renderAmount = 255;

	ImGui::BeginChild("##colorandamount", ImVec2(0, 0), ImGuiChildFlags_None);

	ImGui::TextUnformatted(L("#Gomod_Menu_Color_FXColor"));
	if (ColorChangeButton("fxcolor", "#Gomod_Color_Title_FX", s_fxColor))
		OpenColorDialog("#Gomod_Color_Title_FX", "render_color", s_fxColor, s_fxColor);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::TextUnformatted(L("#Gomod_Menu_RenderFXAmount"));

	int applyBtnW = 170, applyBtnH = 30;
	ScaleSize(applyBtnW, applyBtnH);

	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	const std::string amountFormat = EscapePercent(L("#Gomod_Menu_RenderAmount")) + " %d";
	ImGui::SliderInt("##render_amount", &s_renderAmount, 0, 255, amountFormat.c_str());

	ImGui::Spacing();
	CenteredButtonPos((float)applyBtnW);
	if (ImGui::Button(LID("#Gomod_Menu_ApplyFXAmount").c_str(), ImVec2((float)applyBtnW, (float)applyBtnH)))
	{
		char cmdBuffer[64];
		snprintf(cmdBuffer, sizeof(cmdBuffer), "render_amount %d", s_renderAmount);
		RunCmd(cmdBuffer);
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
		if (ImGui::BeginTabItem(LID("#Gomod_Menu_Tab_Tools").c_str()))
		{
			DrawToolsTab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem(LID("#Gomod_Menu_Tab_RenderOptions").c_str()))
		{
			DrawRenderOptionsTab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem(LID("#Gomod_Menu_Tab_ColorAndAmount").c_str()))
		{
			DrawColorAndAmountTab();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}

// =====================================================================
//  User Settings: Player Settings
// =====================================================================

// ---- player models: models/player/<name>/<name>.mdl ----
static std::vector<std::string> g_PlayerModels;
static bool g_PlayerModelsLoaded = false;

static void LoadPlayerModels()
{
	if (g_PlayerModelsLoaded)
		return;

	g_PlayerModelsLoaded = true;
	g_PlayerModels.clear();

	if (!g_pFileSystem)
		return;

	std::set<std::string> found; // sorted, no duplicates (one result per search path)

	FileFindHandle_t handle;
	const char* name = g_pFileSystem->FindFirst("models/player/*", &handle, nullptr);
	if (!name)
		return;

	for (; name; name = g_pFileSystem->FindNext(handle))
	{
		if (!g_pFileSystem->FindIsDirectory(handle))
			continue;

		std::string folder = name;
		const size_t pos = folder.find_last_of("/\\");
		if (pos != std::string::npos)
			folder = folder.substr(pos + 1);

		if (folder.empty() || folder == "." || folder == "..")
			continue;

		const std::string mdl = "models/player/" + folder + "/" + folder + ".mdl";
		if (g_pFileSystem->FileExists(mdl.c_str()))
			found.insert(folder);
	}

	g_pFileSystem->FindClose(handle);

	g_PlayerModels.assign(found.begin(), found.end());
}

// ---- preview: models/player/<name>/<name>.bmp (8 bit, with palette) ----
// The colors of the player are in the palette: 160-191 = top color, 192-223 = bottom color.
// Those entries keep their saturation and brightness and take the hue of topcolor / bottomcolor
// (0-255 = the whole hue circle), like the player model in the game.

struct PlayerPreview
{
	std::string model; // model of the loaded bmp ("" = nothing loaded)
	bool valid = false;
	int w = 0;
	int h = 0;
	unsigned char palette[256][3];
	std::vector<unsigned char> indices; // top-down
	std::vector<unsigned char> rgba;
	GLuint tex = 0;
	int top = -1;
	int bottom = -1;
};

static PlayerPreview g_Preview;

static void FreePlayerPreview()
{
	DeleteTexture(g_Preview.tex);
	g_Preview.model.clear();
	g_Preview.valid = false;
	g_Preview.top = g_Preview.bottom = -1;
}

static int ReadLE16(const unsigned char* p) { return p[0] | (p[1] << 8); }
static int ReadLE32(const unsigned char* p) { return (int)(p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned)p[3] << 24)); }

static bool LoadPlayerBmp(const std::string& model)
{
	g_Preview.valid = false;
	g_Preview.top = g_Preview.bottom = -1;

	const std::string path = "models/player/" + model + "/" + model + ".bmp";

	int len = 0;
	byte* file = gEngfuncs.COM_LoadFile((char*)path.c_str(), 5, &len);
	if (!file)
		return false;

	const unsigned char* d = (const unsigned char*)file;
	bool ok = false;

	if (len > 54 && d[0] == 'B' && d[1] == 'M')
	{
		const int dataOffset = ReadLE32(d + 10);
		const int headerSize = ReadLE32(d + 14);
		const int w = ReadLE32(d + 18);
		int h = ReadLE32(d + 22);
		const int bpp = ReadLE16(d + 28);
		const int compression = ReadLE32(d + 30);
		int colors = ReadLE32(d + 46);

		const bool bottomUp = h > 0;
		if (h < 0)
			h = -h;

		if (colors <= 0 || colors > 256)
			colors = 256;

		const int stride = (w + 3) & ~3;
		const int paletteOffset = 14 + headerSize;

		if (bpp == 8 && compression == 0 && w > 0 && h > 0 && w <= 1024 && h <= 1024 &&
			paletteOffset + colors * 4 <= len && dataOffset + stride * h <= len)
		{
			memset(g_Preview.palette, 0, sizeof(g_Preview.palette));
			for (int i = 0; i < colors; i++)
			{
				const unsigned char* c = d + paletteOffset + i * 4; // B G R x
				g_Preview.palette[i][0] = c[2];
				g_Preview.palette[i][1] = c[1];
				g_Preview.palette[i][2] = c[0];
			}

			g_Preview.w = w;
			g_Preview.h = h;
			g_Preview.indices.resize((size_t)w * h);

			for (int y = 0; y < h; y++)
			{
				const int srcRow = bottomUp ? (h - 1 - y) : y;
				memcpy(&g_Preview.indices[(size_t)y * w], d + dataOffset + (size_t)srcRow * stride, w);
			}

			ok = true;
		}
	}

	gEngfuncs.COM_FreeFile(file);

	if (!ok)
		gEngfuncs.Con_Printf("GoMod menu: %s is not an 8 bit bmp\n", path.c_str());

	g_Preview.valid = ok;
	return ok;
}

// same saturation and brightness, new hue (0-360)
static void SetHue(const unsigned char in[3], float hue, unsigned char out[3])
{
	const float r = in[0] / 255.0f, g = in[1] / 255.0f, b = in[2] / 255.0f;
	const float mx = (std::max)(r, (std::max)(g, b));
	const float mn = (std::min)(r, (std::min)(g, b));

	const float v = mx;
	const float s = mx > 0.0f ? (mx - mn) / mx : 0.0f;

	float h6 = hue / 60.0f;
	while (h6 >= 6.0f)
		h6 -= 6.0f;
	while (h6 < 0.0f)
		h6 += 6.0f;

	const int sector = (int)h6;
	const float f = h6 - sector;
	const float p = v * (1.0f - s);
	const float q = v * (1.0f - s * f);
	const float t = v * (1.0f - s * (1.0f - f));

	float rr, gg, bb;
	switch (sector)
	{
	case 0: rr = v; gg = t; bb = p; break;
	case 1: rr = q; gg = v; bb = p; break;
	case 2: rr = p; gg = v; bb = t; break;
	case 3: rr = p; gg = q; bb = v; break;
	case 4: rr = t; gg = p; bb = v; break;
	default: rr = v; gg = p; bb = q; break;
	}

	out[0] = (unsigned char)(rr * 255.0f + 0.5f);
	out[1] = (unsigned char)(gg * 255.0f + 0.5f);
	out[2] = (unsigned char)(bb * 255.0f + 0.5f);
}

// color of the slider (0-255) to show next to it
static void HueColor(int value, int rgb[3])
{
	static const unsigned char red[3] = {255, 0, 0};
	unsigned char out[3];
	SetHue(red, value * 360.0f / 255.0f, out);
	rgb[0] = out[0];
	rgb[1] = out[1];
	rgb[2] = out[2];
}

// the preview texture of 'model' with these colors (0 if the model has no bmp)
static GLuint GetPlayerPreview(const std::string& model, int top, int bottom)
{
	if (model != g_Preview.model)
	{
		g_Preview.model = model;
		LoadPlayerBmp(model);
	}

	if (!g_Preview.valid)
		return 0;

	if (top == g_Preview.top && bottom == g_Preview.bottom && g_Preview.tex)
		return g_Preview.tex;

	g_Preview.top = top;
	g_Preview.bottom = bottom;

	unsigned char pal[256][3];
	memcpy(pal, g_Preview.palette, sizeof(pal));

	for (int i = 160; i <= 191; i++)
		SetHue(g_Preview.palette[i], top * 360.0f / 255.0f, pal[i]);
	for (int i = 192; i <= 223; i++)
		SetHue(g_Preview.palette[i], bottom * 360.0f / 255.0f, pal[i]);

	const size_t count = (size_t)g_Preview.w * g_Preview.h;
	g_Preview.rgba.resize(count * 4);

	for (size_t i = 0; i < count; i++)
	{
		const unsigned char* c = pal[g_Preview.indices[i]];
		g_Preview.rgba[i * 4 + 0] = c[0];
		g_Preview.rgba[i * 4 + 1] = c[1];
		g_Preview.rgba[i * 4 + 2] = c[2];
		g_Preview.rgba[i * 4 + 3] = 255;
	}

	g_Preview.tex = UploadTexture(g_Preview.tex, g_Preview.rgba.data(), g_Preview.w, g_Preview.h);
	return g_Preview.tex;
}

// image that fits inside (maxW x maxH) keeping its proportions, centered horizontally
static void FitImage(ImTextureID tex, int imgW, int imgH, float maxW, float maxH, const ImVec4& bg = ImVec4(0, 0, 0, 0))
{
	if (imgW <= 0 || imgH <= 0 || maxW <= 0.0f || maxH <= 0.0f)
		return;

	float scale = maxW / imgW;
	if (imgH * scale > maxH)
		scale = maxH / imgH;

	const ImVec2 size(imgW * scale, imgH * scale);
	CenteredButtonPos(size.x);
	ImGui::ImageWithBg(tex, size, ImVec2(0, 0), ImVec2(1, 1), bg);
}

static void DrawPlayerSettingsTab()
{
	static RefreshTimer s_Refresh;

	static int s_iModel = -1;
	static int s_iTop = 0;
	static int s_iBottom = 0;
	static int s_iVoice = 0;
	static int s_iFlashlight = 0; // server side: it can't be read back, it keeps the last choice

	LoadPlayerModels();

	if (s_Refresh.Check())
	{
		// read what the player has now
		s_iModel = -1;
		if (const char* model = gEngfuncs.pfnGetCvarString("model"))
		{
			for (int i = 0; i < (int)g_PlayerModels.size(); i++)
			{
				if (!stricmp(model, g_PlayerModels[i].c_str()))
					s_iModel = i;
			}
		}
		if (s_iModel < 0 && !g_PlayerModels.empty())
			s_iModel = 0;

		s_iTop = (std::max)(0, (std::min)(255, CvarInt("topcolor")));
		s_iBottom = (std::max)(0, (std::min)(255, CvarInt("bottomcolor")));

		// voice: the one the client has now (not reset every time the menu opens)
		s_iVoice = 0;
		if (const char* voice = gEngfuncs.pfnGetCvarString("cl_player_sfx_type"))
		{
			for (int i = 0; i < (int)IM_ARRAYSIZE(g_VoiceOptions); i++)
			{
				if (!stricmp(voice, g_VoiceOptions[i].value))
					s_iVoice = i;
			}
		}
	}

	const ImGuiStyle& style = ImGui::GetStyle();
	const float avail = ImGui::GetContentRegionAvail().x;
	const float previewW = avail * 0.36f;

	int btnW = 200, btnH = 30;
	ScaleSize(btnW, btnH);

	// ---------------- left: options ----------------
	ImGui::BeginChild("##playerleft", ImVec2(avail - previewW - style.ItemSpacing.x, 0), ImGuiChildFlags_None);

	SectionTitle("#Gomod_User_Tab_Player");

	ImGui::TextUnformatted(L("#Gomod_User_PlayerModel"));
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

	if (g_PlayerModels.empty())
	{
		ImGui::TextDisabled("%s", L("#Gomod_User_NoModels"));
	}
	else if (ImGui::BeginCombo("##playermodel", g_PlayerModels[s_iModel].c_str(), ImGuiComboFlags_HeightLarge))
	{
		for (int i = 0; i < (int)g_PlayerModels.size(); i++)
		{
			const bool selected = (i == s_iModel);
			ImGui::PushID(i);
			if (ImGui::Selectable(g_PlayerModels[i].c_str(), selected))
				s_iModel = i;
			ImGui::PopID();
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::Spacing();

	// top / bottom color (0-255 = hue)
	auto ColorSlider = [&](const char* id, const char* key, int& value)
	{
		ImGui::TextUnformatted(L(key));

		int hue[3];
		HueColor(value, hue);

		const float h = ImGui::GetFrameHeight();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - h * 1.6f - style.ItemSpacing.x);
		ImGui::SliderInt(id, &value, 0, 255);
		ImGui::SameLine();
		ImGui::ColorButton((std::string(id) + "_hue").c_str(), ToImColor(hue), ImGuiColorEditFlags_NoTooltip, ImVec2(h * 1.6f, h));
	};

	ColorSlider("##topcolor", "#Gomod_User_TopColor", s_iTop);
	ColorSlider("##bottomcolor", "#Gomod_User_BottomColor", s_iBottom);

	ImGui::Spacing();

	// physgun beam
	{
		int physgun[3];
		CvarColor("physgun_color", physgun);
		if (ColorChangeButton("physgun", "#Gomod_User_ChangePhysgunColor", physgun))
			OpenColorDialog("#Gomod_Color_Title_Physgun", "physgun_color", physgun);
	}

	ImGui::Spacing();

	CenteredButtonPos((float)btnW);
	if (ImGui::Button(LID("#Gomod_User_ApplyPlayer").c_str(), ImVec2((float)btnW, (float)btnH)))
	{
		char buf[128];

		if (s_iModel >= 0 && s_iModel < (int)g_PlayerModels.size())
		{
			snprintf(buf, sizeof(buf), "model \"%s\"", g_PlayerModels[s_iModel].c_str());
			RunCmd(buf);
		}

		snprintf(buf, sizeof(buf), "topcolor %d", s_iTop);
		RunCmd(buf);

		snprintf(buf, sizeof(buf), "bottomcolor %d", s_iBottom);
		RunCmd(buf);
	}

	// ---- voice + flashlight (same as the old Customization tab) ----
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::TextUnformatted(L("#Gomod_Menu_PlayerVoice"));
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	if (ImGui::BeginCombo("##playervoice", L(g_VoiceOptions[s_iVoice].label)))
	{
		for (int i = 0; i < (int)IM_ARRAYSIZE(g_VoiceOptions); i++)
		{
			const bool selected = (i == s_iVoice);
			if (ImGui::Selectable(LID(g_VoiceOptions[i].label).c_str(), selected))
				s_iVoice = i;
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::Spacing();

	ImGui::TextUnformatted(L("#Gomod_Menu_FlashlightType"));
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	if (ImGui::BeginCombo("##flashlighttype", L(g_FlashLightOptions[s_iFlashlight].label)))
	{
		for (int i = 0; i < (int)IM_ARRAYSIZE(g_FlashLightOptions); i++)
		{
			const bool selected = (i == s_iFlashlight);
			if (ImGui::Selectable(LID(g_FlashLightOptions[i].label).c_str(), selected))
				s_iFlashlight = i;
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::Spacing();

	// night vision color: only for "Custom Night Vision"
	{
		const bool custom = (g_FlashLightOptions[s_iFlashlight].value == 3);

		ImGui::TextUnformatted(L("#Gomod_User_NightVisionColor"));
		ImGui::BeginDisabled(!custom);

		int nv[3];
		CvarColor("nightvision_color", nv);
		if (ColorChangeButton("nightvision", "#Gomod_User_ChangeNightVisionColor", nv))
			OpenColorDialog("#Gomod_Color_Title_NightVision", "nightvision_color", nv);

		ImGui::EndDisabled();

		if (!custom)
			ImGui::TextDisabled("%s", L("#Gomod_User_NightVisionHint"));
	}

	ImGui::Spacing();

	CenteredButtonPos((float)btnW);
	if (ImGui::Button(LID("#Gomod_User_ApplyVoiceFlashlight").c_str(), ImVec2((float)btnW, (float)btnH)))
	{
		char buf[64];

		snprintf(buf, sizeof(buf), "cl_player_sfx_type %s", g_VoiceOptions[s_iVoice].value);
		RunCmd(buf);

		snprintf(buf, sizeof(buf), "toggle_flashlight_mode %d", g_FlashLightOptions[s_iFlashlight].value);
		RunCmd(buf);
	}

	ImGui::EndChild();

	// ---------------- right: preview of the model ----------------
	ImGui::SameLine();
	ImGui::BeginChild("##playerpreview", ImVec2(0, 0), ImGuiChildFlags_Borders);

	if (s_iModel >= 0 && s_iModel < (int)g_PlayerModels.size())
	{
		const std::string& model = g_PlayerModels[s_iModel];
		const GLuint tex = GetPlayerPreview(model, s_iTop, s_iBottom);

		const float w = ImGui::GetContentRegionAvail().x;
		const float h = ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() * 2.0f;

		if (tex)
		{
			FitImage((ImTextureID)(intptr_t)tex, g_Preview.w, g_Preview.h, w, h);
		}
		else
		{
			const ImVec2 ts = ImGui::CalcTextSize(L("#Gomod_User_NoPreview"));
			ImGui::SetCursorPos(ImVec2((w - ts.x) * 0.5f, h * 0.5f));
			ImGui::TextDisabled("%s", L("#Gomod_User_NoPreview"));
		}

		ImGui::Spacing();
		const ImVec2 ts = ImGui::CalcTextSize(model.c_str());
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ts.x) * 0.5f);
		ImGui::TextUnformatted(model.c_str());
	}

	ImGui::EndChild();
}

// =====================================================================
//  User Settings: Hud Color (+ crosshair)
// =====================================================================

static const char* const g_CrosshairSizes[] = {"auto", "small", "medium", "large"};
static const char* const g_CrosshairSizeKeys[] = {
	"#Gomod_Menu_CrosshairSize_Auto",
	"#Gomod_Menu_CrosshairSize_Small",
	"#Gomod_Menu_CrosshairSize_Medium",
	"#Gomod_Menu_CrosshairSize_Large",
};

struct CrosshairSettings
{
	bool legacy = false;
	bool translucent = true;
	int size = 0;
};

static CrosshairSettings g_Crosshair; // what the menu shows (applied with "Apply Crosshair")

static void ReadCrosshairCvars()
{
	g_Crosshair.legacy = gEngfuncs.pfnGetCvarFloat("cl_legacy_crosshair") != 0;
	g_Crosshair.translucent = gEngfuncs.pfnGetCvarFloat("cl_crosshair_translucent") != 0;

	g_Crosshair.size = 0;
	if (const char* size = gEngfuncs.pfnGetCvarString("cl_crosshair_size"))
	{
		for (int i = 0; i < (int)IM_ARRAYSIZE(g_CrosshairSizes); i++)
		{
			if (!stricmp(size, g_CrosshairSizes[i]) || (size[0] == '0' + i && size[1] == '\0'))
				g_Crosshair.size = i;
		}
	}
}

static void DrawCrosshairSettings()
{
	SectionTitle("#Gomod_Menu_Crosshair");

	ImGui::Checkbox(LID("#Gomod_Menu_CrosshairLegacy").c_str(), &g_Crosshair.legacy);

	// the rest only applies to the new crosshair
	ImGui::BeginDisabled(g_Crosshair.legacy);

	ImGui::TextUnformatted(L("#Gomod_Menu_CrosshairSize"));
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	if (ImGui::BeginCombo("##crosshairsize", L(g_CrosshairSizeKeys[g_Crosshair.size])))
	{
		for (int i = 0; i < (int)IM_ARRAYSIZE(g_CrosshairSizes); i++)
		{
			const bool selected = (i == g_Crosshair.size);
			if (ImGui::Selectable(LID(g_CrosshairSizeKeys[i]).c_str(), selected))
				g_Crosshair.size = i;
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	// color: between the size and translucent, it opens the color window
	ImGui::TextUnformatted(L("#Gomod_User_CrosshairColor"));
	{
		int cross[3];
		CvarColor("crosshair_color", cross);
		if (ColorChangeButton("crosshaircolor", "#Gomod_User_ChangeCrosshairColor", cross))
			OpenColorDialog("#Gomod_Color_Title_Crosshair", "crosshair_color", cross);
	}

	ImGui::Checkbox(LID("#Gomod_Menu_CrosshairTranslucent").c_str(), &g_Crosshair.translucent);

	ImGui::EndDisabled();

	ImGui::Spacing();

	int btnW = 200, btnH = 30;
	ScaleSize(btnW, btnH);
	CenteredButtonPos((float)btnW);

	if (ImGui::Button(LID("#Gomod_Menu_ApplyCrosshair").c_str(), ImVec2((float)btnW, (float)btnH)))
	{
		char buf[64];

		snprintf(buf, sizeof(buf), "cl_legacy_crosshair %d", g_Crosshair.legacy ? 1 : 0);
		RunCmd(buf);

		snprintf(buf, sizeof(buf), "cl_crosshair_size %s", g_CrosshairSizes[g_Crosshair.size]);
		RunCmd(buf);

		snprintf(buf, sizeof(buf), "cl_crosshair_translucent %d", g_Crosshair.translucent ? 1 : 0);
		RunCmd(buf);
	}
}

// HUD sprites are white in the png and get the color here, on a dark background (like the game)
static void DrawTintedPreview(const char* path, const int rgb[3], float maxW)
{
	const UiImage& img = GetUiImage(path);
	if (!img.tex)
		return;

	float scale = maxW / img.w;
	if (scale > 1.5f)
		scale = 1.5f;

	const ImVec2 size(img.w * scale, img.h * scale);
	CenteredButtonPos(size.x);

	const ImVec2 p0 = ImGui::GetCursorScreenPos();
	const ImVec2 p1(p0.x + size.x, p0.y + size.y);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(p0, p1, IM_COL32(12, 12, 12, 255), 4.0f);
	dl->AddImage((ImTextureID)(intptr_t)img.tex, p0, p1, ImVec2(0, 0), ImVec2(1, 1), ToImU32(rgb));
	ImGui::Dummy(size);
}

// crosshair over a little scene, with the settings that the menu shows now
static void DrawCrosshairPreview(float maxW)
{
	const UiImage& bg = GetUiImage("resource/imgui/menu/crosshair_preview.png");

	const float w = maxW;
	const float h = (bg.tex && bg.w > 0) ? maxW * bg.h / bg.w : maxW * 0.6f;

	CenteredButtonPos(w);
	const ImVec2 p0 = ImGui::GetCursorScreenPos();
	const ImVec2 p1(p0.x + w, p0.y + h);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	if (bg.tex)
		dl->AddImage((ImTextureID)(intptr_t)bg.tex, p0, p1);
	else
		dl->AddRectFilled(p0, p1, IM_COL32(50, 52, 56, 255));

	const float cx = (float)(int)((p0.x + p1.x) * 0.5f);
	const float cy = (float)(int)((p0.y + p1.y) * 0.5f);

	if (g_Crosshair.legacy)
	{
		const UiImage& legacy = GetUiImage("resource/imgui/menu/crosshair_legacy.png");
		if (legacy.tex)
		{
			const float s = h * 0.30f;
			dl->AddImage((ImTextureID)(intptr_t)legacy.tex, ImVec2(cx - s * 0.5f, cy - s * 0.5f), ImVec2(cx + s * 0.5f, cy + s * 0.5f));
		}
	}
	else
	{
		// same shape as CHudAmmo::DrawLineCrosshair (ammo.cpp), bigger so it can be seen
		static const int kLength[] = {5, 7, 10};
		static const int kGap[] = {3, 4, 5};

		int index = g_Crosshair.size - 1; // 0 auto, 1 small, 2 medium, 3 large
		if (index < 0)
			index = ScreenHeight <= 600 ? 0 : (ScreenHeight <= 1080 ? 1 : 2);

		const float k = (std::max)(1.0f, (float)(int)(h / 90.0f));
		const float length = kLength[index] * k;
		const float gap = kGap[index] * k;
		const float thick = k;

		int rgb[3];
		CvarColor("crosshair_color", rgb);
		const ImU32 col = ToImU32(rgb, g_Crosshair.translucent ? 200 : 255);

		const float half = (float)(int)(thick * 0.5f);
		dl->AddRectFilled(ImVec2(cx - gap - length, cy - half), ImVec2(cx - gap, cy - half + thick), col);			// left
		dl->AddRectFilled(ImVec2(cx + gap + 1, cy - half), ImVec2(cx + gap + 1 + length, cy - half + thick), col);	// right
		dl->AddRectFilled(ImVec2(cx - half, cy - gap - length), ImVec2(cx - half + thick, cy - gap), col);			// top
		dl->AddRectFilled(ImVec2(cx - half, cy + gap + 1), ImVec2(cx - half + thick, cy + gap + 1 + length), col);	// bottom
	}

	ImGui::Dummy(ImVec2(w, h));
}

static void DrawHudColorTab()
{
	static RefreshTimer s_Refresh;
	if (s_Refresh.Check())
		ReadCrosshairCvars();

	const ImGuiStyle& style = ImGui::GetStyle();
	const float avail = ImGui::GetContentRegionAvail().x;
	const float previewW = avail * 0.45f;

	int hud[3], critical[3];
	CvarColor("hud_color", hud);
	CvarColor("hud_color_critical", critical);

	// ---------------- left: options ----------------
	ImGui::BeginChild("##hudleft", ImVec2(avail - previewW - style.ItemSpacing.x, 0), ImGuiChildFlags_None);

	SectionTitle("#Gomod_User_Tab_Hud");

	ImGui::TextUnformatted(L("#Gomod_User_HudColor"));
	if (ColorChangeButton("hudcolor", "#Gomod_User_ChangeHudColor", hud))
		OpenColorDialog("#Gomod_Color_Title_HUD", "hud_color", hud);

	ImGui::Spacing();

	ImGui::TextUnformatted(L("#Gomod_User_HudCriticalColor"));
	if (ColorChangeButton("hudcritical", "#Gomod_User_ChangeHudCriticalColor", critical))
		OpenColorDialog("#Gomod_Color_Title_HUDCritical", "hud_color_critical", critical);
	ImGui::TextDisabled("%s", L("#Gomod_User_HudCriticalHint"));

	DrawCrosshairSettings();

	ImGui::EndChild();

	// ---------------- right: previews ----------------
	ImGui::SameLine();
	ImGui::BeginChild("##hudpreview", ImVec2(0, 0), ImGuiChildFlags_Borders);

	const float w = ImGui::GetContentRegionAvail().x;

	ImGui::TextUnformatted(L("#Gomod_User_HudColor"));
	DrawTintedPreview("resource/imgui/menu/hud_preview.png", hud, w);

	ImGui::Spacing();
	ImGui::TextUnformatted(L("#Gomod_User_HudCriticalColor"));
	DrawTintedPreview("resource/imgui/menu/hud_preview_critical.png", critical, w);

	ImGui::Spacing();
	ImGui::TextUnformatted(L("#Gomod_Menu_Crosshair"));
	DrawCrosshairPreview(w);

	ImGui::EndChild();
}

// =====================================================================
//  User Settings: Sound Effects (gm_audio.cpp, the cvars are not saved)
// =====================================================================

static bool CvarCheckbox(const char* key, const char* cvar)
{
	bool value = gEngfuncs.pfnGetCvarFloat(cvar) != 0;
	if (ImGui::Checkbox(LID(key).c_str(), &value))
	{
		gEngfuncs.Cvar_SetValue(cvar, value ? 1.0f : 0.0f);
		return true;
	}
	return false;
}

static void CvarSliderFloat(const char* key, const char* cvar, float mn, float mx, const char* fmt = "%.2f")
{
	float value = gEngfuncs.pfnGetCvarFloat(cvar);

	ImGui::TextUnformatted(L(key));
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	if (ImGui::SliderFloat((std::string("##") + cvar).c_str(), &value, mn, mx, fmt, ImGuiSliderFlags_AlwaysClamp))
		gEngfuncs.Cvar_SetValue(cvar, value);
}

static void DrawSoundEffectsTab()
{
	ImGui::BeginChild("##soundfx", ImVec2(0, 0), ImGuiChildFlags_None);

	ImGui::TextWrapped("%s", L("#Gomod_Sound_Info"));
	ImGui::Spacing();

	{
		int btnW = 220, btnH = 28;
		ScaleSize(btnW, btnH);

		CenteredButtonPos((float)btnW * 2.0f + ImGui::GetStyle().ItemSpacing.x);
		if (ImGui::Button(LID("#Gomod_Sound_EnableAll").c_str(), ImVec2((float)btnW, (float)btnH)))
			RunCmd("gm_audio_fx_all");
		ImGui::SameLine();
		if (ImGui::Button(LID("#Gomod_Sound_ResetAll").c_str(), ImVec2((float)btnW, (float)btnH)))
			RunCmd("gm_audio_fx_reset");
	}

	// two columns of groups
	if (ImGui::BeginTable("##soundtable", 2, ImGuiTableFlags_SizingStretchSame))
	{
		ImGui::TableNextRow();

		// ---- left column ----
		ImGui::TableSetColumnIndex(0);

		SectionTitle("#Gomod_Sound_Reverb");
		CvarCheckbox("#Gomod_Sound_ReverbEnable", "gm_audio_reverb");
		ImGui::BeginDisabled(gEngfuncs.pfnGetCvarFloat("gm_audio_reverb") == 0);
		{
			int room = CvarInt("gm_audio_reverb_room");
			ImGui::TextUnformatted(L("#Gomod_Sound_ReverbRoom"));
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			const std::string fmt = room == 0 ? EscapePercent(L("#Gomod_Sound_ReverbRoomMap")) : std::string("%d");
			if (ImGui::SliderInt("##reverbroom", &room, 0, 28, fmt.c_str(), ImGuiSliderFlags_AlwaysClamp))
				gEngfuncs.Cvar_SetValue("gm_audio_reverb_room", (float)room);
		}
		CvarSliderFloat("#Gomod_Sound_ReverbMix", "gm_audio_reverb_mix", 0.0f, 2.0f);
		ImGui::EndDisabled();

		SectionTitle("#Gomod_Sound_Occlusion");
		CvarCheckbox("#Gomod_Sound_OcclusionEnable", "gm_audio_occlusion");
		ImGui::BeginDisabled(gEngfuncs.pfnGetCvarFloat("gm_audio_occlusion") == 0);
		CvarSliderFloat("#Gomod_Sound_OcclusionStrength", "gm_audio_occlusion_strength", 0.0f, 1.0f);
		ImGui::EndDisabled();

		SectionTitle("#Gomod_Sound_Doppler");
		CvarCheckbox("#Gomod_Sound_DopplerEnable", "gm_audio_doppler");
		ImGui::BeginDisabled(gEngfuncs.pfnGetCvarFloat("gm_audio_doppler") == 0);
		CvarSliderFloat("#Gomod_Sound_DopplerScale", "gm_audio_doppler_scale", 0.1f, 4.0f);
		ImGui::EndDisabled();

		// ---- right column ----
		ImGui::TableSetColumnIndex(1);

		SectionTitle("#Gomod_Sound_Space");
		CvarCheckbox("#Gomod_Sound_Spatial", "gm_audio_spatial");
		CvarCheckbox("#Gomod_Sound_Air", "gm_audio_air");
		CvarCheckbox("#Gomod_Sound_Underwater", "gm_audio_underwater");
		{
			int interp = CvarInt("gm_audio_interp") != 0 ? 1 : 0;
			static const char* const kInterp[] = {"#Gomod_Sound_InterpLinear", "#Gomod_Sound_InterpCubic"};

			ImGui::TextUnformatted(L("#Gomod_Sound_Interp"));
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			if (ImGui::BeginCombo("##interp", L(kInterp[interp])))
			{
				for (int i = 0; i < 2; i++)
				{
					if (ImGui::Selectable(LID(kInterp[i]).c_str(), i == interp))
						gEngfuncs.Cvar_SetValue("gm_audio_interp", (float)i);
				}
				ImGui::EndCombo();
			}
		}

		SectionTitle("#Gomod_Sound_Vibrato");
		CvarCheckbox("#Gomod_Sound_VibratoEnable", "gm_audio_vibrato");
		ImGui::BeginDisabled(gEngfuncs.pfnGetCvarFloat("gm_audio_vibrato") == 0);
		CvarSliderFloat("#Gomod_Sound_VibratoRate", "gm_audio_vibrato_rate", 0.1f, 30.0f, "%.1f");
		CvarSliderFloat("#Gomod_Sound_VibratoDepth", "gm_audio_vibrato_depth", 0.0f, 0.9f);
		ImGui::EndDisabled();

		ImGui::EndTable();
	}

	ImGui::EndChild();
}

// =====================================================================
//  User Settings: Environment (server commands of client.cpp, sandbox only)
//  fog <r> <g> <b> <density> <skyfog> / fog_remove
//  rain ... / rain_remove, snow ... / snow_remove (same limits as the server)
// =====================================================================

static void DrawEnvironmentTab()
{
	// fog
	static bool s_bFogRead = false;
	static int s_fogColor[3] = {155, 155, 155};
	static int s_fogDensity = 4;
	static bool s_fogSkybox = true;

	// rain (default values of env_rain)
	static int s_rainColor[3] = {255, 255, 255};
	static int s_rainIntensity = 150;
	static int s_rainBrightness = 128;
	static int s_rainSize = 2;
	static int s_rainSpeed = 1800;
	static bool s_rainSplashes = true;
	static bool s_rainRipples = true;
	static bool s_rainIndoors = false;

	// snow (default values of env_snow)
	static int s_snowColor[3] = {128, 128, 128};
	static int s_snowIntensity = 150;
	static int s_snowBrightness = 130;
	static int s_snowSize = 2;
	static int s_snowSpeed = 200;
	static bool s_snowIndoors = false;

	// the first time: the fog that the player has now (map fog or his own)
	if (!s_bFogRead)
	{
		s_bFogRead = true;
		if (gHUD.fog.density > 0.0f)
		{
			s_fogColor[0] = gHUD.fog.r;
			s_fogColor[1] = gHUD.fog.g;
			s_fogColor[2] = gHUD.fog.b;
			s_fogDensity = (int)(gHUD.fog.density * 10000.0f + 0.5f);
			s_fogSkybox = gHUD.fog.affectSkybox;
		}
	}
	s_fogDensity = (std::max)(1, (std::min)(100, s_fogDensity));

	int btnW = 170, btnH = 28;
	ScaleSize(btnW, btnH);

	auto ApplyRemove = [&](const char* applyKey, const char* removeKey, bool& apply, bool& remove)
	{
		CenteredButtonPos((float)btnW * 2.0f + ImGui::GetStyle().ItemSpacing.x);
		apply = ImGui::Button(LID(applyKey).c_str(), ImVec2((float)btnW, (float)btnH));
		ImGui::SameLine();
		remove = ImGui::Button(LID(removeKey).c_str(), ImVec2((float)btnW, (float)btnH));
	};

	auto Slider = [](const char* id, const char* key, int& value, int mn, int mx)
	{
		ImGui::TextUnformatted(L(key));
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::SliderInt(id, &value, mn, mx, "%d", ImGuiSliderFlags_AlwaysClamp);
	};

	ImGui::BeginChild("##environment", ImVec2(0, 0), ImGuiChildFlags_None);

	ImGui::TextWrapped("%s", L("#Gomod_Env_Info"));

	// ---------------- fog ----------------
	SectionTitle("#Gomod_Env_Fog");

	ImGui::TextUnformatted(L("#Gomod_Env_FogColor"));
	if (ColorChangeButton("fogcolor", "#Gomod_Env_ChangeFogColor", s_fogColor))
		OpenColorDialog("#Gomod_Color_Title_Fog", nullptr, s_fogColor, s_fogColor);

	Slider("##fogdensity", "#Gomod_Env_FogDensity", s_fogDensity, 1, 100);
	ImGui::Checkbox(LID("#Gomod_Env_FogSkybox").c_str(), &s_fogSkybox);

	{
		bool apply, remove;
		ApplyRemove("#Gomod_Env_ApplyFog", "#Gomod_Env_RemoveFog", apply, remove);

		if (apply)
		{
			char buf[96];
			snprintf(buf, sizeof(buf), "fog %d %d %d %d %d", s_fogColor[0], s_fogColor[1], s_fogColor[2], s_fogDensity, s_fogSkybox ? 1 : 0);
			RunCmd(buf);
		}
		if (remove)
			RunCmd("fog_remove");
	}

	// weather disabled by the player: say it
	if (gEngfuncs.pfnGetCvarFloat("cl_weather") == 0)
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "%s", L("#Gomod_Env_WeatherOff"));
	}

	if (ImGui::BeginTable("##weathertable", 2, ImGuiTableFlags_SizingStretchSame))
	{
		ImGui::TableNextRow();

		// ---------------- rain ----------------
		ImGui::TableSetColumnIndex(0);
		SectionTitle("#Gomod_Env_Rain");

		ImGui::TextUnformatted(L("#Gomod_Env_RainColor"));
		if (ColorChangeButton("raincolor", "#Gomod_Env_ChangeRainColor", s_rainColor))
			OpenColorDialog("#Gomod_Color_Title_Rain", nullptr, s_rainColor, s_rainColor);

		Slider("##rainintensity", "#Gomod_Env_Intensity", s_rainIntensity, 10, 400);
		Slider("##rainbrightness", "#Gomod_Env_Brightness", s_rainBrightness, 1, 255);
		Slider("##rainsize", "#Gomod_Env_DropSize", s_rainSize, 1, 4);
		Slider("##rainspeed", "#Gomod_Env_Speed", s_rainSpeed, 300, 2500);
		ImGui::Checkbox(LID("#Gomod_Env_Splashes").c_str(), &s_rainSplashes);
		ImGui::Checkbox(LID("#Gomod_Env_Ripples").c_str(), &s_rainRipples);
		ImGui::Checkbox((std::string(L("#Gomod_Env_Indoors")) + "###rainindoors").c_str(), &s_rainIndoors);

		{
			bool apply, remove;
			ImGui::PushID("rain");
			ApplyRemove("#Gomod_Env_ApplyRain", "#Gomod_Env_RemoveRain", apply, remove);
			ImGui::PopID();

			if (apply)
			{
				char buf[160];
				snprintf(buf, sizeof(buf), "rain %d %d %d %d %d %d %d %d %d %d",
					s_rainIntensity, s_rainColor[0], s_rainColor[1], s_rainColor[2], s_rainBrightness,
					s_rainSize, s_rainSpeed, s_rainSplashes ? 1 : 0, s_rainRipples ? 1 : 0, s_rainIndoors ? 1 : 0);
				RunCmd(buf);
			}
			if (remove)
				RunCmd("rain_remove");
		}

		// ---------------- snow ----------------
		ImGui::TableSetColumnIndex(1);
		SectionTitle("#Gomod_Env_Snow");

		ImGui::TextUnformatted(L("#Gomod_Env_SnowColor"));
		if (ColorChangeButton("snowcolor", "#Gomod_Env_ChangeSnowColor", s_snowColor))
			OpenColorDialog("#Gomod_Color_Title_Snow", nullptr, s_snowColor, s_snowColor);

		Slider("##snowintensity", "#Gomod_Env_Intensity", s_snowIntensity, 10, 300);
		Slider("##snowbrightness", "#Gomod_Env_Brightness", s_snowBrightness, 1, 255);
		Slider("##snowsize", "#Gomod_Env_FlakeSize", s_snowSize, 1, 6);
		Slider("##snowspeed", "#Gomod_Env_Speed", s_snowSpeed, 30, 400);
		ImGui::Checkbox((std::string(L("#Gomod_Env_Indoors")) + "###snowindoors").c_str(), &s_snowIndoors);

		{
			bool apply, remove;
			ImGui::PushID("snow");
			ApplyRemove("#Gomod_Env_ApplySnow", "#Gomod_Env_RemoveSnow", apply, remove);
			ImGui::PopID();

			if (apply)
			{
				char buf[160];
				snprintf(buf, sizeof(buf), "snow %d %d %d %d %d %d %d %d",
					s_snowIntensity, s_snowColor[0], s_snowColor[1], s_snowColor[2], s_snowBrightness,
					s_snowSize, s_snowSpeed, s_snowIndoors ? 1 : 0);
				RunCmd(buf);
			}
			if (remove)
				RunCmd("snow_remove");
		}

		ImGui::EndTable();
	}

	ImGui::EndChild();
}

// =====================================================================
//  User Settings: Player Tweaks (server commands of client.cpp, sandbox only)
//  self_health / self_armor / self_speed / self_blood / self_render / buddha / zeus / self_reset
// =====================================================================

static const int TWEAK_SPEED_NORMAL = 320; // sv_maxspeed of Half-Life
static const int TWEAK_SPEED_MIN = 100;	   // same limits as client.cpp
static const int TWEAK_SPEED_MAX = 1000;

struct PlayerTweaks
{
	int health = 100;
	int armor = 1;
	int speed = TWEAK_SPEED_NORMAL;
	int blood = 0;	   // 0 red, 1 green
	int renderMode = 0; // index in the Render Mode list of Render Options
	int renderFX = 0;	// index in the Render FX list of Render Options
	int renderAmount = 255;
	int renderColor[3] = {255, 255, 255};
	bool buddha = false;
	bool zeus = false;
};

// "rendermode color" -> "color"
static const char* ToolArg(const ToolDef& tool)
{
	const char* space = strchr(tool.command, ' ');
	return space ? space + 1 : tool.command;
}

static void DrawPlayerTweaksTab()
{
	static PlayerTweaks s_Tweaks;
	static RefreshTimer s_Refresh;
	static int s_iReadHealth = -1; // what the HUD said when the menu opened
	static int s_iReadArmor = -1;

	// health and suit: what the player has now (they are only sent if they are changed)
	if (s_Refresh.Check())
	{
		s_iReadHealth = (std::max)(1, (std::min)(999, gHUD.m_Health.m_iHealth));
		s_iReadArmor = (std::max)(1, (std::min)(999, gHUD.m_Battery.GetArmor()));
		s_Tweaks.health = s_iReadHealth;
		s_Tweaks.armor = s_iReadArmor;
	}

	const std::vector<ToolDef>& modes = g_RenderToolCats[0].tools;
	const std::vector<ToolDef>& fxs = g_RenderToolCats[1].tools;
	s_Tweaks.renderMode = (std::max)(0, (std::min)((int)modes.size() - 1, s_Tweaks.renderMode));
	s_Tweaks.renderFX = (std::max)(0, (std::min)((int)fxs.size() - 1, s_Tweaks.renderFX));

	auto Slider = [](const char* id, const char* key, int& value, int mn, int mx, const char* fmt = "%d")
	{
		ImGui::TextUnformatted(L(key));
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::SliderInt(id, &value, mn, mx, fmt, ImGuiSliderFlags_AlwaysClamp);
	};

	ImGui::BeginChild("##playertweaks", ImVec2(0, 0), ImGuiChildFlags_None);

	ImGui::TextWrapped("%s", L("#Gomod_Tweaks_Info"));

	if (ImGui::BeginTable("##tweakstable", 2, ImGuiTableFlags_SizingStretchSame))
	{
		ImGui::TableNextRow();

		// ---------------- left: player ----------------
		ImGui::TableSetColumnIndex(0);

		SectionTitle("#Gomod_Tweaks_Player");

		Slider("##tweakhealth", "#Gomod_Tweaks_Health", s_Tweaks.health, 1, 999);
		Slider("##tweakarmor", "#Gomod_Tweaks_Suit", s_Tweaks.armor, 1, 999);
		ImGui::TextDisabled("%s", L("#Gomod_Tweaks_AliveHint"));

		ImGui::Spacing();

		{
			const std::string fmt = s_Tweaks.speed == TWEAK_SPEED_NORMAL
										? "%d (" + EscapePercent(L("#Gomod_Tweaks_SpeedNormal")) + ")"
										: std::string("%d");
			Slider("##tweakspeed", "#Gomod_Tweaks_Speed", s_Tweaks.speed, TWEAK_SPEED_MIN, TWEAK_SPEED_MAX, fmt.c_str());
		}

		ImGui::Spacing();

		{
			static const char* const kBlood[] = {"#Gomod_Tweaks_BloodRed", "#Gomod_Tweaks_BloodGreen"};

			ImGui::TextUnformatted(L("#Gomod_Tweaks_Blood"));
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			if (ImGui::BeginCombo("##tweakblood", L(kBlood[s_Tweaks.blood])))
			{
				for (int i = 0; i < 2; i++)
				{
					if (ImGui::Selectable(LID(kBlood[i]).c_str(), i == s_Tweaks.blood))
						s_Tweaks.blood = i;
				}
				ImGui::EndCombo();
			}
		}

		SectionTitle("#Gomod_Tweaks_Immortality");
		ImGui::Checkbox(LID("#Gomod_Tweaks_Buddha").c_str(), &s_Tweaks.buddha);
		ImGui::TextDisabled("%s", L("#Gomod_Tweaks_BuddhaHint"));
		ImGui::Checkbox(LID("#Gomod_Tweaks_Zeus").c_str(), &s_Tweaks.zeus);
		ImGui::TextDisabled("%s", L("#Gomod_Tweaks_ZeusHint"));

		// ---------------- right: render ----------------
		ImGui::TableSetColumnIndex(1);

		SectionTitle("#Gomod_Tweaks_Render");

		ImGui::TextUnformatted(L("#Gomod_Menu_Cat_RenderMode"));
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		if (ImGui::BeginCombo("##tweakrendermode", L(modes[s_Tweaks.renderMode].name)))
		{
			for (int i = 0; i < (int)modes.size(); i++)
			{
				if (ImGui::Selectable(LID(modes[i].name).c_str(), i == s_Tweaks.renderMode))
					s_Tweaks.renderMode = i;
			}
			ImGui::EndCombo();
		}

		ImGui::TextUnformatted(L("#Gomod_Menu_Cat_RenderFX"));
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		if (ImGui::BeginCombo("##tweakrenderfx", L(fxs[s_Tweaks.renderFX].name), ImGuiComboFlags_HeightLarge))
		{
			for (int i = 0; i < (int)fxs.size(); i++)
			{
				if (ImGui::Selectable(LID(fxs[i].name).c_str(), i == s_Tweaks.renderFX))
					s_Tweaks.renderFX = i;
			}
			ImGui::EndCombo();
		}

		{
			const std::string fmt = EscapePercent(L("#Gomod_Menu_RenderAmount")) + " %d";
			ImGui::TextUnformatted(L("#Gomod_Menu_RenderFXAmount"));
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			ImGui::SliderInt("##tweakrenderamount", &s_Tweaks.renderAmount, 0, 255, fmt.c_str(), ImGuiSliderFlags_AlwaysClamp);
		}

		ImGui::TextUnformatted(L("#Gomod_Menu_Color_FXColor"));
		if (ColorChangeButton("tweakrendercolor", "#Gomod_Tweaks_ChangeRenderColor", s_Tweaks.renderColor))
			OpenColorDialog("#Gomod_Color_Title_FX", nullptr, s_Tweaks.renderColor, s_Tweaks.renderColor);

		// the hint of the selected render, same texts as Render Options
		ImGui::Spacing();
		if (fxs[s_Tweaks.renderFX].hint && s_Tweaks.renderFX != 0)
			ImGui::TextWrapped("%s", ResolveKeyText(fxs[s_Tweaks.renderFX].hint).c_str());
		else if (modes[s_Tweaks.renderMode].hint)
			ImGui::TextWrapped("%s", ResolveKeyText(modes[s_Tweaks.renderMode].hint).c_str());

		ImGui::EndTable();
	}

	// ---------------- apply / reset ----------------
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	int btnW = 200, btnH = 30;
	ScaleSize(btnW, btnH);
	CenteredButtonPos((float)btnW * 2.0f + ImGui::GetStyle().ItemSpacing.x);

	if (ImGui::Button(LID("#Gomod_Tweaks_Apply").c_str(), ImVec2((float)btnW, (float)btnH)))
	{
		char buf[160];

		if (s_Tweaks.health != s_iReadHealth)
		{
			snprintf(buf, sizeof(buf), "self_health %d", s_Tweaks.health);
			RunCmd(buf);
			s_iReadHealth = s_Tweaks.health;
		}

		if (s_Tweaks.armor != s_iReadArmor)
		{
			snprintf(buf, sizeof(buf), "self_armor %d", s_Tweaks.armor);
			RunCmd(buf);
			s_iReadArmor = s_Tweaks.armor;
		}

		snprintf(buf, sizeof(buf), "self_speed %d", s_Tweaks.speed == TWEAK_SPEED_NORMAL ? 0 : s_Tweaks.speed);
		RunCmd(buf);

		RunCmd(s_Tweaks.blood ? "self_blood green" : "self_blood red");

		snprintf(buf, sizeof(buf), "self_render %s %s %d %d %d %d",
			ToolArg(modes[s_Tweaks.renderMode]), ToolArg(fxs[s_Tweaks.renderFX]), s_Tweaks.renderAmount,
			s_Tweaks.renderColor[0], s_Tweaks.renderColor[1], s_Tweaks.renderColor[2]);
		RunCmd(buf);

		RunCmd(s_Tweaks.buddha ? "buddha 1" : "buddha 0");
		RunCmd(s_Tweaks.zeus ? "zeus 1" : "zeus 0");
	}

	ImGui::SameLine();

	if (ImGui::Button(LID("#Gomod_Tweaks_Reset").c_str(), ImVec2((float)btnW, (float)btnH)))
	{
		RunCmd("self_reset");

		s_Tweaks = PlayerTweaks();
		s_Tweaks.health = s_iReadHealth = 100;
		s_iReadArmor = s_Tweaks.armor; // the server puts 0, the slider starts at 1
	}

	ImGui::EndChild();
}

static void DrawUserSettingsWindow(int x, int y, int w, int h)
{
	ImGui::SetNextWindowPos(ImVec2((float)x, (float)y), ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(ImVec2((float)w, (float)h), ImGuiCond_Appearing);

	ImGui::Begin("User Settings", &g_bShowMenu, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

	if (ImGui::BeginTabBar("##usertabs"))
	{
		if (ImGui::BeginTabItem(LID("#Gomod_User_Tab_Player").c_str()))
		{
			DrawPlayerSettingsTab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem(LID("#Gomod_User_Tab_Tweaks").c_str()))
		{
			DrawPlayerTweaksTab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem(LID("#Gomod_User_Tab_Hud").c_str()))
		{
			DrawHudColorTab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem(LID("#Gomod_User_Tab_Sound").c_str()))
		{
			DrawSoundEffectsTab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem(LID("#Gomod_User_Tab_Environment").c_str()))
		{
			DrawEnvironmentTab();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}

// =====================================================================
//  Admin Settings: the command menu (commandmenu.txt, vgui_TeamFortressViewport.cpp)
//  done again in ImGui, reading resource/adminmenu.txt. Same file format and same rules:
//    "key" "text" "command"                 normal button
//    "key" "text" { ... }                   submenu
//    TOGGLE "key" "text" "cvar"             switches the cvar 0/1
//    MAP "mapname" "key" "text" "command"   only on that map
//    TEAM1..TEAM4 "key" "text" "command"    only for that team
//    CUSTOM "key" "text" "!CHANGETEAM"      submenu with the teams
//  The number keys press the buttons of the open menu, like the command menu.
// =====================================================================

extern int g_iTeamNumber;
extern int GoModAdmin_NumberOfTeams();
extern const char* GoModAdmin_TeamName(int team);
extern bool GoModAdmin_AllowSpectators();

static const char* ADMIN_MENU_FILE = "resource/adminmenu.txt";
static const int ADMIN_MAX_MENUS = 80;	  // MAX_MENUS
static const int ADMIN_BUTTON_TEXT = 32;  // MAX_BUTTON_SIZE
static const int ADMIN_COMMAND_SIZE = 128; // cCommandLength

enum AdminButtonType
{
	ADMIN_NORMAL = 0,
	ADMIN_TOGGLE,
	ADMIN_MAP,
	ADMIN_TEAMONLY,
	ADMIN_TEAM,		  // a team of !CHANGETEAM (1-4, 5 = auto assign)
	ADMIN_SPECTATE,	  // spectate button of !CHANGETEAM
	ADMIN_CUSTOM_MENU, // the !CHANGETEAM button itself
};

struct AdminButton
{
	int type = ADMIN_NORMAL;
	char key = 0;
	std::string text;
	std::string command;
	std::string map; // "maps/<name>.bsp"
	int team = 0;
	int submenu = -1;
};

struct AdminMenu
{
	int parent = -1;
	std::vector<AdminButton> buttons;
};

static std::vector<AdminMenu> g_AdminMenus;
static bool g_AdminLoaded = false;
static std::vector<int> g_AdminChain; // open menus: root, submenu, sub-submenu...
static int g_iPendingDigit = 0;		  // number key pressed this frame ('0'-'9')

static void AdminLocalise(const char* token, std::string& out)
{
	char text[ADMIN_BUTTON_TEXT] = "";

	// our keys first (resource/gomod_gui.txt), then the ones of the game (titles.txt)
	if (token[0] == '#')
	{
		EnsureHintStringsLoaded();
		auto it = g_KeyStrings.find(token + 1);
		if (it != g_KeyStrings.end())
		{
			strncpy(text, it->second.c_str(), sizeof(text) - 1);
			out = text;
			return;
		}
	}

	CHudTextMessage::LocaliseTextString(token, text, sizeof(text));
	text[sizeof(text) - 1] = '\0';
	out = text;
}

static int AdminNewMenu(int parent)
{
	AdminMenu menu;
	menu.parent = parent;
	g_AdminMenus.push_back(menu);
	return (int)g_AdminMenus.size() - 1;
}

static void Admin_Load()
{
	g_AdminLoaded = true;
	g_AdminMenus.clear();
	g_AdminChain.clear();

	const int root = AdminNewMenu(-1);

	byte* buffer = gEngfuncs.COM_LoadFile((char*)ADMIN_MENU_FILE, 5, NULL);
	if (!buffer)
	{
		gEngfuncs.Con_DPrintf("Unable to open %s\n", ADMIN_MENU_FILE);
		return;
	}

	char token[1024];
	const char* pfile = (const char*)buffer;
	int current = root;
	char szLastButtonText[32] = "file start";

	pfile = gEngfuncs.COM_ParseFile(pfile, token);
	while (strlen(token) > 0 && (int)g_AdminMenus.size() < ADMIN_MAX_MENUS)
	{
		// keep looping until we hit the end of this menu
		while (token[0] != '}' && strlen(token) > 0)
		{
			char cBoundKey[32] = "";
			char cCustom[32] = "";
			char cCommand[ADMIN_COMMAND_SIZE] = "";
			char szMap[256] = "";
			std::string text;
			bool iCustom = false;
			int iTeamOnly = -1;
			bool iToggle = false;
			bool bGetExtraToken = true;
			int button = -1; // index in the current menu (-1 = no button)

			if (current < 0)
			{
				gEngfuncs.Con_Printf("Error in %s file after '%s'.\n", ADMIN_MENU_FILE, szLastButtonText);
				g_AdminMenus.resize(1);
				g_AdminMenus[0].buttons.clear();
				gEngfuncs.COM_FreeFile(buffer);
				return;
			}

			// token should already be the bound key, or the custom name
			strncpy(cCustom, token, 32);
			cCustom[31] = '\0';

			if (0 == strcmp(cCustom, "CUSTOM"))
			{
				iCustom = true;
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
			}
			else if (0 == strcmp(cCustom, "MAP"))
			{
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
				strncpy(szMap, token, sizeof(szMap));
				szMap[sizeof(szMap) - 1] = '\0';
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
			}
			else if (0 == strncmp(cCustom, "TEAM", 4)) // TEAM1, TEAM2, TEAM3, TEAM4
			{
				iTeamOnly = atoi(cCustom + 4);
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
			}
			else if (0 == strncmp(cCustom, "TOGGLE", 6))
			{
				iToggle = true;
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
			}

			// button bound key
			strncpy(cBoundKey, token, 32);
			cBoundKey[31] = '\0';

			// button text
			pfile = gEngfuncs.COM_ParseFile(pfile, token);
			AdminLocalise(token, text);
			strncpy(szLastButtonText, text.c_str(), sizeof(szLastButtonText) - 1);
			szLastButtonText[sizeof(szLastButtonText) - 1] = '\0';

			// button command
			pfile = gEngfuncs.COM_ParseFile(pfile, token);
			strncpy(cCommand, token, ADMIN_COMMAND_SIZE);
			cCommand[ADMIN_COMMAND_SIZE - 1] = '\0';

			AdminButton btn;
			btn.key = cBoundKey[0];
			btn.text = text;

			if (iCustom)
			{
				// only !CHANGETEAM makes a button (!CHANGECLASS is for TFC classes)
				if (0 == strcmp(cCommand, "!CHANGETEAM"))
				{
					btn.type = ADMIN_CUSTOM_MENU;
					btn.submenu = AdminNewMenu(current);

					AdminMenu& teams = g_AdminMenus[btn.submenu];
					for (int i = 0; i < 4; i++)
					{
						AdminButton team;
						team.type = ADMIN_TEAM;
						team.team = i + 1;
						team.command = "jointeam " + std::to_string(i + 1);
						teams.buttons.push_back(team);
					}

					AdminButton autoAssign;
					autoAssign.type = ADMIN_TEAM;
					autoAssign.team = 5;
					autoAssign.text = L("#Gomod_Admin_AutoAssign");
					autoAssign.command = "jointeam 5";
					teams.buttons.push_back(autoAssign);

					AdminButton spectate;
					spectate.type = ADMIN_SPECTATE;
					spectate.text = L("#Gomod_Admin_Spectate");
					spectate.command = "spectate";
					teams.buttons.push_back(spectate);

					g_AdminMenus[current].buttons.push_back(btn);
					button = (int)g_AdminMenus[current].buttons.size() - 1;
				}

				// next token: is it a menu?
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
				if (token[0] == '{')
					strcpy(cCommand, token);
				else
					bGetExtraToken = false;
			}
			else
			{
				if (szMap[0] != '\0')
				{
					btn.type = ADMIN_MAP;
					btn.map = std::string("maps/") + szMap + ".bsp";
				}
				else if (iTeamOnly != -1)
				{
					btn.type = ADMIN_TEAMONLY;
					btn.team = iTeamOnly;
				}
				else if (iToggle)
				{
					btn.type = ADMIN_TOGGLE;
				}

				if (cCommand[0] != '{')
					btn.command = cCommand;

				g_AdminMenus[current].buttons.push_back(btn);
				button = (int)g_AdminMenus[current].buttons.size() - 1;
			}

			// submenu or button?
			if (cCommand[0] == '{')
			{
				if ((int)g_AdminMenus.size() >= ADMIN_MAX_MENUS)
				{
					gEngfuncs.Con_Printf("Too many menus in %s past '%s'\n", ADMIN_MENU_FILE, szLastButtonText);
				}
				else if (button >= 0)
				{
					const int submenu = AdminNewMenu(current);
					g_AdminMenus[current].buttons[button].submenu = submenu;
					g_AdminMenus[current].buttons[button].command.clear();
					current = submenu;
				}
			}

			if (bGetExtraToken)
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
		}

		// move back up a menu
		current = (current >= 0) ? g_AdminMenus[current].parent : -1;

		pfile = gEngfuncs.COM_ParseFile(pfile, token);
	}

	gEngfuncs.COM_FreeFile(buffer);
}

// IsNotValid() of the buttons of the command menu, inverted
static bool AdminButtonValid(const AdminButton& b)
{
	switch (b.type)
	{
	case ADMIN_MAP:
	{
		const char* level = gEngfuncs.pfnGetLevelName();
		return level && 0 == strcmp(b.map.c_str(), level);
	}

	case ADMIN_TEAMONLY:
		return g_iTeamNumber == b.team;

	case ADMIN_TEAM:
	{
		const int teams = GoModAdmin_NumberOfTeams();
		if (teams == 1)
			return false;
		if (b.team == 5)
			return true;
		return teams >= b.team && b.team != g_iTeamNumber;
	}

	case ADMIN_SPECTATE:
		return GoModAdmin_AllowSpectators();

	default:
		return true;
	}
}

static bool AdminMenuHasVisible(int menu, int depth = 0);

// RecalculateVisibles(): a submenu button without visible buttons inside is not shown
static bool AdminButtonVisible(const AdminButton& b, int depth)
{
	if (!AdminButtonValid(b))
		return false;

	if (b.submenu >= 0 && !AdminMenuHasVisible(b.submenu, depth + 1))
		return false;

	return true;
}

static bool AdminMenuHasVisible(int menu, int depth)
{
	if (menu < 0 || menu >= (int)g_AdminMenus.size() || depth > ADMIN_MAX_MENUS)
		return false;

	for (const AdminButton& b : g_AdminMenus[menu].buttons)
	{
		if (AdminButtonVisible(b, depth))
			return true;
	}
	return false;
}

static std::string AdminButtonText(const AdminButton& b)
{
	std::string text = b.text;

	if (b.type == ADMIN_TEAM && b.team >= 1 && b.team <= 4)
	{
		const char* name = GoModAdmin_TeamName(b.team);
		text = (name && name[0]) ? name : ("Team " + std::to_string(b.team));
	}

	// RecalculateText(): the bound key before the text, or spaces
	char buf[128];
	if (b.key != 0)
		snprintf(buf, sizeof(buf), "  %c  %s", b.key, text.c_str());
	else
		snprintf(buf, sizeof(buf), "     %s", text.c_str());

	buf[ADMIN_BUTTON_TEXT - 1] = '\0';
	return buf;
}

// the button was pressed: open its submenu or run it (and close the submenus, like the command menu)
static void AdminPress(int depth, const AdminButton& b)
{
	if (b.submenu >= 0)
	{
		g_AdminChain.resize(depth + 1);
		g_AdminChain.push_back(b.submenu);
		return;
	}

	if (b.type == ADMIN_TOGGLE)
	{
		if (cvar_t* cvar = gEngfuncs.pfnGetCvarPointer(b.command.c_str()))
			gEngfuncs.Cvar_SetValue(b.command.c_str(), cvar->value != 0 ? 0.0f : 1.0f);
	}
	else if (!b.command.empty())
	{
		gEngfuncs.pfnClientCmd(b.command.c_str());
	}

	GMAudio_PlayLocal("gomod/button_new.wav", 1.0f);

	// HideCommandMenu()
	g_AdminChain.clear();
	g_AdminChain.push_back(0);
}

// CCommandMenu::KeyInput(): the buttons of the deepest open menu
static void AdminKeyInput(int key)
{
	if (g_AdminChain.empty())
		return;

	const int depth = (int)g_AdminChain.size() - 1;
	const int menu = g_AdminChain.back();

	for (const AdminButton& b : g_AdminMenus[menu].buttons)
	{
		if (AdminButtonValid(b) && b.key == key)
		{
			AdminPress(depth, b);
			return;
		}
	}
}

static void DrawAdminSettingsWindow(int x, int y, int w, int h)
{
	if (!g_AdminLoaded)
		Admin_Load();

	if (g_AdminChain.empty())
		g_AdminChain.push_back(0);

	ImGui::SetNextWindowPos(ImVec2((float)x, (float)y), ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(ImVec2((float)w, (float)h), ImGuiCond_Appearing);

	ImGui::Begin("Admin Settings", &g_bShowMenu, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar);

	if (g_AdminMenus.empty() || g_AdminMenus[0].buttons.empty())
	{
		ImGui::TextDisabled("%s", L("#Gomod_Admin_NoFile"));
		ImGui::End();
		return;
	}

	// number keys (not while typing in a text field)
	if (g_iPendingDigit && !ImGui::GetIO().WantTextInput)
		AdminKeyInput(g_iPendingDigit);

	// same size as the command menu buttons
	const float btnW = (float)(int)XRES(160);
	const float btnH = (float)(int)YRES(30);
	const float stepY = btnH - 1.0f;

	const ImVec2 origin = ImGui::GetCursorPos();
	float columnY = origin.y; // y of the button that opened the menu of this column

	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

	int newChainDepth = -1; // the mouse went over a button: menus to keep open
	const AdminButton* hoveredButton = nullptr;
	const AdminButton* clickedButton = nullptr;
	int clickedDepth = -1;

	for (int depth = 0; depth < (int)g_AdminChain.size(); depth++)
	{
		const int menu = g_AdminChain[depth];
		if (menu < 0 || menu >= (int)g_AdminMenus.size())
			break;

		const float columnX = origin.x + depth * (btnW + 2.0f);
		const int openChild = (depth + 1 < (int)g_AdminChain.size()) ? g_AdminChain[depth + 1] : -1;

		float rowY = columnY;
		float nextColumnY = columnY;

		ImGui::PushID(menu);

		for (int i = 0; i < (int)g_AdminMenus[menu].buttons.size(); i++)
		{
			const AdminButton& b = g_AdminMenus[menu].buttons[i];
			if (!AdminButtonVisible(b, 0))
				continue;

			const bool armed = (b.submenu >= 0 && b.submenu == openChild);
			if (armed)
			{
				nextColumnY = rowY;
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
			}

			ImGui::SetCursorPos(ImVec2(columnX, rowY));
			ImGui::PushID(i);
			const bool pressed = ImGui::Button(AdminButtonText(b).c_str(), ImVec2(btnW, btnH));
			ImGui::PopID();

			if (armed)
				ImGui::PopStyleColor();

			const ImVec2 mn = ImGui::GetItemRectMin();
			const ImVec2 mx = ImGui::GetItemRectMax();
			ImDrawList* dl = ImGui::GetWindowDrawList();
			const ImU32 textCol = ImGui::GetColorU32(ImGuiCol_Text);
			const float midY = (mn.y + mx.y) * 0.5f;

			// submenu: arrow at the right
			if (b.submenu >= 0)
			{
				const float s = btnH * 0.18f;
				const float ax = mx.x - s * 2.5f;
				dl->AddTriangleFilled(ImVec2(ax, midY - s), ImVec2(ax + s * 1.3f, midY), ImVec2(ax, midY + s), textCol);
			}

			// toggle: checked / unchecked after the text
			if (b.type == ADMIN_TOGGLE)
			{
				cvar_t* cvar = gEngfuncs.pfnGetCvarPointer(b.command.c_str());
				const bool on = cvar && cvar->value != 0;

				const float textW = ImGui::CalcTextSize(AdminButtonText(b).c_str()).x;
				const float s = ImGui::GetTextLineHeight() * 0.8f;
				const float bx = mn.x + ImGui::GetStyle().FramePadding.x + textW + s * 0.6f;

				dl->AddRect(ImVec2(bx, midY - s * 0.5f), ImVec2(bx + s, midY + s * 0.5f), textCol);
				if (on)
					dl->AddRectFilled(ImVec2(bx + 3, midY - s * 0.5f + 3), ImVec2(bx + s - 3, midY + s * 0.5f - 3), textCol);
			}

			// cursorEntered: open this submenu / close the deeper ones
			if (ImGui::IsItemHovered() && newChainDepth < 0)
			{
				newChainDepth = depth;
				hoveredButton = &b;
			}

			if (pressed)
			{
				clickedButton = &b;
				clickedDepth = depth;
			}

			rowY += stepY;
		}

		ImGui::PopID();

		// the submenu starts at the height of its button
		columnY = nextColumnY;
	}

	ImGui::PopStyleVar(2);

	// changes after drawing (the chain is being read above)
	if (clickedButton)
	{
		AdminPress(clickedDepth, *clickedButton);
	}
	else if (hoveredButton)
	{
		g_AdminChain.resize(newChainDepth + 1);
		if (hoveredButton->submenu >= 0)
			g_AdminChain.push_back(hoveredButton->submenu);
	}

	ImGui::End();
}

// =====================================================================
//  Information: Message of the Day (motd.txt, message "MOTD") and Map Info (maps/<map>.txt)
//  In the sandbox they are a section of the Go-Mod menu (green circle).
//  In the other game modes they are two windows alone (the old VGUI ones, done in ImGui):
//  the MOTD opens by itself when it arrives, toggleMOTDMenu / toggleMapInfoMenu open them.
// =====================================================================

extern const char* GoModInfo_ServerName();	   // vgui_TeamFortressViewport.cpp
extern void GoModInfo_ShowVGUIMenu(int iMenu); // vgui_TeamFortressViewport.cpp

enum
{
	INFO_PAGE_MOTD = 0,
	INFO_PAGE_MAPINFO,
};

static std::string g_MOTD;
static bool g_bMOTDComplete = true; // the last chunk arrived (next message = new MOTD)

static int g_iInfoStandalone = -1; // page open alone (outside the sandbox), -1 = none
static int g_iQueuedVGUIMenu = 0;  // VGUI menu that waits for the window to be closed

static std::string g_MapInfoLevel; // level of the loaded text
static std::string g_MapInfoName;
static std::string g_MapInfoText;
static bool g_bMapInfoFound = false;

static void OpenStandaloneInfo(int page); // below

// message "MOTD": byte last chunk, string text (dlls/multiplay_gamerules.cpp, SendMOTDToClient)
int GoModInfo_MsgMOTD(const char* pszName, int iSize, void* pbuf)
{
	if (g_bMOTDComplete)
		g_MOTD.clear();

	BEGIN_READ(pbuf, iSize);

	g_bMOTDComplete = READ_BYTE() != 0;
	g_MOTD += READ_STRING();

	if (g_MOTD.size() > MAX_MOTD_LENGTH - 1)
		g_MOTD.resize(MAX_MOTD_LENGTH - 1);

	// don't show the MOTD to HLTV spectators. In the sandbox it is only in the Go-Mod menu
	// (the server sends GameMode before the MOTD, InitHUD of multiplay_gamerules.cpp).
	// It opens now, so a VGUI menu that comes after it (team menu of CTF) waits for it.
	if (g_bMOTDComplete && 0 == gEngfuncs.IsSpectateOnly() && !UTIL_IsSandbox())
		OpenStandaloneInfo(INFO_PAGE_MOTD);

	return 1;
}

static void LoadMapInfo()
{
	const char* level = gEngfuncs.pfnGetLevelName(); // "maps/<name>.bsp"
	const std::string current = level ? level : "";

	if (current == g_MapInfoLevel)
		return;

	g_MapInfoLevel = current;
	g_MapInfoName.clear();
	g_MapInfoText.clear();
	g_bMapInfoFound = false;

	if (current.empty())
		return;

	std::string path = current;
	const size_t dot = path.find_last_of('.');
	if (dot != std::string::npos)
		path = path.substr(0, dot);

	const size_t slash = path.find_last_of("/\\");
	g_MapInfoName = (slash != std::string::npos) ? path.substr(slash + 1) : path;

	path += ".txt";

	std::string text;
	if (LoadTextFile(path.c_str(), text))
	{
		g_bMapInfoFound = true;
		g_MapInfoText = text;
	}
}

// the texts come from files written in Windows: no '\r'
static std::string CleanText(const std::string& in)
{
	std::string out;
	out.reserve(in.size());
	for (char c : in)
	{
		if (c != '\r')
			out.push_back(c);
	}
	return out;
}

// title + text with scroll, leaving 'bottom' pixels free below
static void DrawInfoPage(int page, float bottom)
{
	std::string title, text;

	if (page == INFO_PAGE_MOTD)
	{
		const char* server = GoModInfo_ServerName();
		title = (server && server[0]) ? server : "Half-Life";
		text = g_MOTD.empty() ? std::string(L("#Gomod_Info_NoMOTD")) : CleanText(g_MOTD);
	}
	else
	{
		LoadMapInfo();
		title = g_MapInfoName.empty() ? std::string(L("#Gomod_Info_Tab_MapInfo")) : g_MapInfoName;
		text = g_bMapInfoFound ? CleanText(g_MapInfoText) : std::string(L("#Gomod_Info_NoMapInfo"));
	}

	ImGui::SeparatorText(title.c_str());

	ImGui::BeginChild("##infotext", ImVec2(0, -bottom), ImGuiChildFlags_Borders);
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextUnformatted(text.c_str());
	ImGui::PopTextWrapPos();
	ImGui::EndChild();
}

// size of the old VGUI MOTD window
static void InfoWindowRect(float& x, float& y, float& w, float& h)
{
	w = (float)(int)XRES(424);
	h = (float)(int)YRES(312);
	x = (ScreenWidth - w) * 0.5f;
	y = (ScreenHeight - h) * 0.5f;
}

// green circle of the Go-Mod menu (sandbox)
static void DrawInformationWindow()
{
	float x, y, w, h;
	InfoWindowRect(x, y, w, h);

	ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Appearing);

	ImGui::Begin("Information", &g_bShowMenu, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

	if (ImGui::BeginTabBar("##infotabs"))
	{
		if (ImGui::BeginTabItem(LID("#Gomod_Info_Tab_MOTD").c_str()))
		{
			DrawInfoPage(INFO_PAGE_MOTD, 0.0f);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem(LID("#Gomod_Info_Tab_MapInfo").c_str()))
		{
			DrawInfoPage(INFO_PAGE_MAPINFO, 0.0f);
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}

// the window alone (outside the sandbox), with its OK button
static void DrawStandaloneInfo()
{
	float x, y, w, h;
	InfoWindowRect(x, y, w, h);

	ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Appearing);

	const char* title = (g_iInfoStandalone == INFO_PAGE_MOTD) ? "Message of the Day###gomod_info_alone" : "Map Info###gomod_info_alone";

	bool open = true;
	ImGui::Begin(title, &open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

	int btnW = 120, btnH = 28;
	ScaleSize(btnW, btnH);

	DrawInfoPage(g_iInfoStandalone, (float)btnH + ImGui::GetStyle().ItemSpacing.y * 2.0f);

	ImGui::Spacing();
	CenteredButtonPos((float)btnW);
	if (ImGui::Button(LID("#Gomod_Info_OK").c_str(), ImVec2((float)btnW, (float)btnH)))
		open = false;

	ImGui::End();

	if (!open)
		g_bShowMenu = false; // hud_imgui.cpp closes it
}

static void OpenStandaloneInfo(int page)
{
	if (page == INFO_PAGE_MAPINFO)
	{
		LoadMapInfo();
		if (!g_bMapInfoFound)
		{
			gEngfuncs.Con_Printf("This map has no information (%s)\n", g_MapInfoName.c_str());
			return;
		}
	}

	g_iInfoStandalone = page;
	ImGuiMenu_SetOpen(true);
}

extern bool GoModInfo_VGUIMenuOpen(); // vgui_TeamFortressViewport.cpp

// commands toggleMOTDMenu (0) / toggleMapInfoMenu (1)
void GoModInfo_Toggle(int page)
{
	if (UTIL_IsSandbox())
	{
		gEngfuncs.Con_Printf("In the sandbox the MOTD and the map info are in the Go-Mod menu (Information)\n");
		return;
	}

	if (g_bShowMenu && g_iInfoStandalone == page)
	{
		ImGuiMenu_SetOpen(false);
		return;
	}

	// not on top of a VGUI menu (team menu...)
	if (GoModInfo_VGUIMenuOpen())
		return;

	OpenStandaloneInfo(page);
}

bool GoModInfo_StandaloneOpen()
{
	return g_bShowMenu && g_iInfoStandalone >= 0 && !UTIL_IsSandbox();
}

// a VGUI menu wants to open while the MOTD / Map Info is open: it waits (true = queued)
bool GoModInfo_QueueVGUIMenu(int iMenu)
{
	if (!GoModInfo_StandaloneOpen())
		return false;

	g_iQueuedVGUIMenu = iMenu;
	return true;
}

// every frame (also with the menu closed)
void GoModInfo_Frame()
{
	// closed: the queued VGUI menu (team menu of CTF...) can open now
	if (!g_bShowMenu && g_iInfoStandalone >= 0)
	{
		g_iInfoStandalone = -1;

		if (g_iQueuedVGUIMenu)
		{
			const int menu = g_iQueuedVGUIMenu;
			g_iQueuedVGUIMenu = 0;
			GoModInfo_ShowVGUIMenu(menu);
		}
	}
}

// =====================================================================
//  Sections: the circles at the top left
//  red = menu theme (popup), orange = spawn menu + tools, blue = user settings, purple = admin
// =====================================================================

enum
{
	SECTION_MAIN = 0,
	SECTION_USER,
	SECTION_INFO,
	SECTION_ADMIN,
};

static int g_iSection = SECTION_MAIN;

static const char* const g_ThemeKeys[] = {
	"#Gomod_Theme_Default",
	"#Gomod_Theme_Dark",
	"#Gomod_Theme_Valve",
	"#Gomod_Theme_GoMod3",
	"#Gomod_Theme_Light",
};

// round button with an icon (png) or just a colored circle if the png is missing
static bool SectionButton(const char* id, const char* icon, ImU32 fallback, float size, bool active, const char* tooltipKey)
{
	const ImVec2 p0 = ImGui::GetCursorScreenPos();
	const bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
	const bool hovered = ImGui::IsItemHovered();

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 center(p0.x + size * 0.5f, p0.y + size * 0.5f);
	const float radius = size * 0.5f;

	// the section that is open has a ring around
	if (active)
		dl->AddCircle(center, radius - 1.0f, IM_COL32(255, 255, 255, 230), 0, 3.0f);

	const float pad = active ? 4.0f : (hovered ? 0.0f : 2.0f);
	const ImU32 tint = hovered ? IM_COL32_WHITE : IM_COL32(225, 225, 225, 255);

	const UiImage& img = GetUiImage(icon);
	if (img.tex)
		dl->AddImage((ImTextureID)(intptr_t)img.tex, ImVec2(p0.x + pad, p0.y + pad), ImVec2(p0.x + size - pad, p0.y + size - pad),
			ImVec2(0, 0), ImVec2(1, 1), tint);
	else
		dl->AddCircleFilled(center, radius - pad, fallback);

	if (hovered)
		ImGui::SetTooltip("%s", L(tooltipKey));

	if (pressed)
		GMAudio_PlayLocal("gomod/button_new.wav", 1.0f);

	return pressed;
}

static void DrawSectionBar(float x, float y, float size)
{
	ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);

	const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
								   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("##gomod_sections", nullptr, flags);
	ImGui::PopStyleVar(2);

	const float spacing = size * 0.25f;
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));

	// red: theme (menu like the right click ones, but with the left click)
	if (SectionButton("##sec_theme", "resource/imgui/menu/section_theme.png", IM_COL32(210, 45, 45, 255), size, false, "#Gomod_Section_Theme"))
		ImGui::OpenPopup("##themepopup");

	if (ImGui::BeginPopup("##themepopup"))
	{
		const int theme = gHUD.GetMenuThemeCvar();
		for (int i = 0; i < (int)IM_ARRAYSIZE(g_ThemeKeys); i++)
		{
			if (ImGui::MenuItem(LID(g_ThemeKeys[i]).c_str(), nullptr, theme == i))
			{
				char buf[48];
				snprintf(buf, sizeof(buf), "cl_gomod_menu_theme %d", i);
				RunCmd(buf);
			}
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	if (SectionButton("##sec_main", "resource/imgui/menu/section_main.png", IM_COL32(235, 135, 30, 255), size, g_iSection == SECTION_MAIN, "#Gomod_Section_Main"))
		g_iSection = SECTION_MAIN;

	ImGui::SameLine();
	if (SectionButton("##sec_user", "resource/imgui/menu/section_user.png", IM_COL32(40, 110, 220, 255), size, g_iSection == SECTION_USER, "#Gomod_Section_User"))
		g_iSection = SECTION_USER;

	ImGui::SameLine();
	if (SectionButton("##sec_info", "resource/imgui/menu/section_info.png", IM_COL32(40, 170, 70, 255), size, g_iSection == SECTION_INFO, "#Gomod_Section_Info"))
		g_iSection = SECTION_INFO;

	ImGui::SameLine();
	if (SectionButton("##sec_admin", "resource/imgui/menu/section_admin.png", IM_COL32(135, 65, 190, 255), size, g_iSection == SECTION_ADMIN, "#Gomod_Section_Admin"))
		g_iSection = SECTION_ADMIN;

	ImGui::PopStyleVar();
	ImGui::End();
}

// =====================================================================
//  public API
// =====================================================================

// number key pressed with the menu open (hud_imgui.cpp, the game doesn't get it)
void GoModMenu_DigitPressed(int digit)
{
	g_iPendingDigit = digit;
}

// Call Between ImGui::NewFrame() and ImGui::Render(), only with the menu opened
void GoModMenu_Draw()
{
	InitData();

	// outside the sandbox the menu only shows the MOTD / Map Info alone
	if (!UTIL_IsSandbox())
	{
		if (g_iInfoStandalone >= 0)
			DrawStandaloneInfo();
		else
			g_bShowMenu = false;
		return;
	}
	g_iInfoStandalone = -1;

	// the menu was just opened: it starts with the spawn menu and the tools
	static RefreshTimer s_Opened;
	if (s_Opened.Check())
	{
		g_iSection = SECTION_MAIN;
		g_AdminChain.clear();
	}

	const int leftW = (int)(ScreenWidth * 0.46f);
	const int rightW = (int)(ScreenWidth * 0.30f);
	const int gap = (int)(ScreenWidth * 0.01f);
	const int h = (int)(ScreenHeight * 0.72f);

	const int x0 = (ScreenWidth - (leftW + gap + rightW)) / 2;
	const int y0 = (ScreenHeight - h) / 2;

	// circles above the menu, at the left
	const float iconSize = (std::max)(24.0f, ScreenHeight * 0.05f);
	DrawSectionBar((float)x0, (std::max)(4.0f, y0 - iconSize - ScreenHeight * 0.015f), iconSize);

	switch (g_iSection)
	{
	case SECTION_USER:
		// same size as the spawn menu, in the center
		DrawUserSettingsWindow((ScreenWidth - leftW) / 2, y0, leftW, h);
		break;

	case SECTION_INFO:
		DrawInformationWindow();
		break;

	case SECTION_ADMIN:
		DrawAdminSettingsWindow(x0, y0, leftW + gap + rightW, h);
		break;

	default:
		DrawSpawnWindow(x0, y0, leftW, h);
		DrawToolsWindow(x0 + leftW + gap, y0, rightW, h);
		break;
	}

	DrawColorDialog();

	g_iPendingDigit = 0;
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

	g_PlayerModelsLoaded = false;

	g_AdminLoaded = false;
	g_AdminMenus.clear();
	g_AdminChain.clear();

	g_MapInfoLevel.clear(); // read the map text again
}

void GoModMenu_Shutdown()
{
	FreeAllTextures();
	g_Tabs.clear();
	g_ToolCats.clear();
	g_AdminMenus.clear();
}
