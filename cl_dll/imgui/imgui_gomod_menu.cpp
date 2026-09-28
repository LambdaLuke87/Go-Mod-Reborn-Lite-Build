// gomod_menu.cpp
// Garry's Mod 11 Styled Menu for Go-Mod: Reborn (Dear ImGui).

#include "hud.h"
#include "cl_util.h"

#include "imgui.h"

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

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

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

static bool IsSpace(char c)
{
	return isspace((unsigned char)c) != 0;
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

static bool KeyIs(const std::string& s, const char* key)
{
	size_t n = strlen(key);

	if (s.size() != n)
		return false;

	for (size_t i = 0; i < n; i++)
	{
		if (tolower((unsigned char)s[i]) != tolower((unsigned char)key[i]))
			return false;
	}

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
	const char* cvar; // send "<cvar> 0/1" when change
	bool value;
};

struct Action
{
	const char* label;
	const char* command;
};

struct SpawnTab
{
	const char* title;
	const char* file;
	std::vector<MenuCategory> cats;
	bool loaded = false;
	std::vector<Toggle> toggles;
	std::vector<Action> actions;
};

struct ToolDef
{
	const char* name;
	const char* command;
};

struct ToolCategory
{
	const char* name;
	std::vector<ToolDef> tools;
};

struct HandCategory
{
	std::string name;
	std::vector<std::string> skins;
};

struct VoiceOption
{
	const char* label;
	const char* value;
};

static std::vector<SpawnTab> g_Tabs;
static std::vector<ToolCategory> g_ToolCats;
static std::vector<ToolCategory> g_RenderToolCats;

static std::vector<HandCategory> g_HandCats;
static bool g_HandsLoaded = false;
static const char* HANDS_FILE = "resource/imgui/hands_manage.txt";

static const VoiceOption g_VoiceOptions[] = {
	{"Gordon/HEV Suit", "hevsuit"},
	{"Team Fortress Classic", "dmc"},
	{"Counter-Strike 1.6", "cstrike"},
	{"Half-Life Alpha", "hlalpha"},
};

// =====================================================================
//  Config, Commands, CVARS
// =====================================================================

static void InitData()
{
	//WIP, TOGGLES NEED BE MODIFIED IN CLIENT.CPP

	if (!g_Tabs.empty())
		return;

	SpawnTab npcs;
	npcs.title = "NPCs";
	npcs.file = "resource/imgui/npcs.txt";
	npcs.toggles = {
		{"Ignore Players", "button_notarget_set", false},
		{"No AI", "button_ai_set", false},
		{"Alternate Classify", "button_allied_set", false},
	};
	npcs.actions = {
		{"Undo", "remove_entity_undo"},
		{"Remove All", "remove_entities_all"},
	};

	SpawnTab props;
	props.title = "Props";
	props.file = "resource/imgui/props.txt";
	props.toggles = {
		{"Ignore Players", "button_notarget_set", false},
	};
	props.actions = {
		{"Undo", "remove_entity_undo"},
		{"Remove All", "remove_entities_all"},
	};

	SpawnTab items;
	items.title = "Items";
	items.file = "resource/imgui/items.txt";
	items.toggles = {
		{"Give Mode", "button_self_pickup", false},
		{"Front Spawn", "button_front_spawn", false},
	};
	items.actions = {
		{"Undo", "remove_entity_undo"},
		{"Remove All", "remove_entities_all"},
	};

	SpawnTab sweeps;
	sweeps.title = "Sweeps";
	sweeps.file = "resource/imgui/sweeps.txt";
	sweeps.toggles = {
		{"Give Mode", "button_self_pickup", false},
		{"Front Spawn", "button_front_spawn", false},
	};
	sweeps.actions = {
		{"Undo", "remove_entity_undo"},
		{"Remove All", "remove_entities_all"},
	};

	g_Tabs.push_back(std::move(npcs));
	g_Tabs.push_back(std::move(props));
	g_Tabs.push_back(std::move(items));
	g_Tabs.push_back(std::move(sweeps));

	// Tools: command = "tool <name>"
	g_ToolCats = {
		{"Common Tools",
		 {
			 {"No Tools", "tool none"},
			 {"Duplicator", "tool duplicator"},
			 {"Remover", "tool remover"},
			 {"Render", "tool render"},
		 }},
		{"Monster Manage",
		 {
			 {"Blood Color", "tool blood_color"},
			 {"Frame Editor", "tool frame_set"},
			 {"Health Modify", "tool health_set"},
			 {"Manipulator", "tool manipulator"},
			 {"Model Editor", "tool model_editor"},
			 {"No Colide", "tool no_collide"},
			 {"Poser", "tool poser"},
			 {"Scaler", "tool scaler"},
			 {"Spawner", "tool spawner"},
			 {"Take Damage", "tool take_damage"},
		 }},
		{"Utilites",
		 {
			 {"Camera", "tool camera"},
			 {"Gibber", "tool gibber"},
			 {"Glowsticks", "tool glowsticks"},
			 {"Teleporter", "tool teleporter"},
		}},
	};

	// Render Options:
	// command = "rendermode <name>"
	// command = "renderfx <name>"
	g_RenderToolCats = {
		{"Render Mode",
		 {
			 {"Normal", "rendermode normal"},
			 {"Color", "rendermode color"},
			 {"Texture", "rendermode texture"},
			 {"Glow", "rendermode glow"},
			 {"Solid", "rendermode solid"},
			 {"Additive", "rendermode additive"},
		}},
		{"Render FX",
		{
			 {"Normal", "renderfx normal"},
			 {"Slow Pulse", "renderfx slow_pulse"},
			 {"Fast Pulse", "renderfx fast_pulse"},
			 {"Slow Wide Pulse", "renderfx slow_wide_pulse"},
			 {"Fast Wide Pulse", "renderfx fast_wide_pulse"},
			 {"Slow Fade Away", "renderfx slow_fade_away"},
			 {"Fast Fade Away", "renderfx fast_fade_away"},
			 {"Slow Become Solid", "renderfx slow_become_solid"},
			 {"Fast Become Solid", "renderfx fast_become_solid"},
			 {"Slow Strobe", "renderfx slow_strobe"},
			 {"Fast Strobe", "renderfx fast_strobe"},
			 {"Faster Strobe", "renderfx faster_strobe"},
			 {"Slow Flicker", "renderfx slow_flicker"},
			 {"Fast Flicker", "renderfx fast_flicker"},
			 {"Constant Glow", "renderfx constant_glow"},
			 {"Distort", "renderfx distort"},
			 {"Hologram", "renderfx hologram"},
			 {"Explode", "renderfx explode"},
			 {"Glow Shell", "renderfx glow_shell"},
		}},
	};
}

// =====================================================================
//  parse txt (Format with: ignore commas, ':' and comments //)
// =====================================================================

struct Lexer
{
	enum Type
	{
		T_END,
		T_OPEN,
		T_CLOSE,
		T_STRING
	};

	const char* p;
	const char* end;
	Type type = T_END;
	std::string text;

	Lexer(const char* data, size_t len) : p(data), end(data + len) {}

	void Next()
	{
		for (;;)
		{
			while (p < end && (IsSpace(*p) || *p == ',' || *p == ':'))
				p++;

			if (p + 1 < end && p[0] == '/' && p[1] == '/')
			{
				while (p < end && *p != '\n')
					p++;
				continue;
			}
			break;
		}

		text.clear();

		if (p >= end)
		{
			type = T_END;
			return;
		}

		if (*p == '{')
		{
			type = T_OPEN;
			p++;
			return;
		}

		if (*p == '}')
		{
			type = T_CLOSE;
			p++;
			return;
		}

		type = T_STRING;

		if (*p == '"')
		{
			p++;
			while (p < end && *p != '"')
				text.push_back(*p++);
			if (p < end)
				p++;
		}
		else
		{
			while (p < end && !IsSpace(*p) && *p != ',' && *p != ':' && *p != '{' && *p != '}')
				text.push_back(*p++);
		}
	}
};

static bool ParseMenuFile(const std::string& src, std::vector<MenuCategory>& out)
{
	Lexer lx(src.data(), src.size());
	lx.Next();

	if (lx.type != Lexer::T_OPEN)
		return false;
	lx.Next();

	while (lx.type == Lexer::T_STRING)
	{
		MenuCategory cat;
		cat.name = lx.text;
		lx.Next();

		if (lx.type != Lexer::T_OPEN)
			return false;
		lx.Next();

		while (lx.type == Lexer::T_STRING)
		{
			MenuItem item;
			item.name = lx.text;
			lx.Next();

			if (lx.type != Lexer::T_OPEN)
				return false;
			lx.Next();

			while (lx.type == Lexer::T_STRING)
			{
				std::string key = lx.text;
				lx.Next();

				if (lx.type != Lexer::T_STRING)
					return false;

				if (KeyIs(key, "image"))
					item.image = lx.text;
				else if (KeyIs(key, "command"))
					item.command = lx.text;

				lx.Next();
			}

			if (lx.type != Lexer::T_CLOSE)
				return false;
			lx.Next();

			cat.items.push_back(std::move(item));
		}

		if (lx.type != Lexer::T_CLOSE)
			return false;
		lx.Next();

		out.push_back(std::move(cat));
	}

	return lx.type == Lexer::T_CLOSE;
}

static bool ParseHandsFile(const std::string& src, std::vector<HandCategory>& out)
{
	Lexer lx(src.data(), src.size());
	lx.Next();

	if (lx.type != Lexer::T_OPEN)
		return false;
	lx.Next();

	while (lx.type == Lexer::T_STRING)
	{
		HandCategory cat;
		cat.name = lx.text;
		lx.Next();

		if (lx.type != Lexer::T_OPEN)
			return false;
		lx.Next();

		while (lx.type == Lexer::T_STRING)
		{
			cat.skins.push_back(lx.text);
			lx.Next();
		}

		if (lx.type != Lexer::T_CLOSE)
			return false;
		lx.Next();

		out.push_back(std::move(cat));
	}

	return lx.type == Lexer::T_CLOSE;
}

static void EnsureLoaded(SpawnTab& tab)
{
	if (tab.loaded)
		return;

	tab.loaded = true;
	tab.cats.clear();

	std::string src;

	if (!LoadTextFile(tab.file, src))
	{
		gEngfuncs.Con_Printf("GoMod menu: it could not be opened %s\n", tab.file);
		return;
	}

	if (!ParseMenuFile(src, tab.cats))
	{
		gEngfuncs.Con_Printf("GoMod menu: syntax error in %s\n", tab.file);
		tab.cats.clear();
	}
}

static void EnsureHandsLoaded()
{
	if (g_HandsLoaded)
		return;

	g_HandsLoaded = true;
	g_HandCats.clear();

	std::string src;

	if (!LoadTextFile(HANDS_FILE, src))
	{
		gEngfuncs.Con_Printf("GoMod menu: it could not be opened %s\n", HANDS_FILE);
		return;
	}

	if (!ParseHandsFile(src, g_HandCats))
	{
		gEngfuncs.Con_Printf("GoMod menu: syntax error in %s\n", HANDS_FILE);
		g_HandCats.clear();
	}
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

	int btnW = 120, btnH = 26;
	ScaleSize(btnW, btnH);

	// checkboxes on the left, buttons on the right
	const float rowH = (ImGui::GetFrameHeight() > (float)btnH ? ImGui::GetFrameHeight() : (float)btnH) + style.ItemSpacing.y;
	const size_t rows = tab.toggles.size() > tab.actions.size() ? tab.toggles.size() : tab.actions.size();
	const float footerH = rows * rowH + style.WindowPadding.y * 2.0f + style.ItemSpacing.y;

	// ---- Selection Lists ----
	ImGui::BeginChild("##list", ImVec2(0, -footerH), ImGuiChildFlags_Borders);

	if (tab.cats.empty())
		ImGui::TextDisabled("Sin entradas. Revisa %s", tab.file);

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
					RunCmd(item.command.c_str());

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
	for (Toggle& t : tab.toggles)
	{
		if (ImGui::Checkbox(t.label, &t.value))
		{
			char buf[128];
			snprintf(buf, sizeof(buf), "%s %d", t.cvar, t.value ? 1 : 0);
			RunCmd(buf);
		}
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
		ImGui::Text("%s", activeTool->name);
	else
		ImGui::TextDisabled("Select a Tool");

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
	EnsureHandsLoaded();

	static int s_handCatIndex = 0;
	static int s_handSkinIndex = 0;
	static int s_voiceIndex = 0;

	ImGui::BeginChild("##customization", ImVec2(0, 0), ImGuiChildFlags_None);

	if (g_HandCats.empty())
	{
		ImGui::TextDisabled("Sin datos. Revisa %s", HANDS_FILE);
	}
	else
	{
		if (s_handCatIndex >= (int)g_HandCats.size())
			s_handCatIndex = 0;

		// ---- Hand Model ----
		ImGui::Text("Hand Model");
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		if (ImGui::BeginCombo("##handmodel", g_HandCats[s_handCatIndex].name.c_str()))
		{
			for (int i = 0; i < (int)g_HandCats.size(); i++)
			{
				bool selected = (i == s_handCatIndex);

				ImGui::PushID(i);
				if (ImGui::Selectable(g_HandCats[i].name.c_str(), selected))
				{
					if (i != s_handCatIndex)
						s_handSkinIndex = 0; // reset skin when change category
					s_handCatIndex = i;
				}
				ImGui::PopID();

				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::Spacing();

		// ---- Hand Skins (depende de la categoria elegida) ----
		const HandCategory& cat = g_HandCats[s_handCatIndex];
		if (s_handSkinIndex >= (int)cat.skins.size())
			s_handSkinIndex = 0;

		ImGui::Text("Hand Skins");
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		const char* skinPreview = cat.skins.empty() ? "N/A" : cat.skins[s_handSkinIndex].c_str();
		if (ImGui::BeginCombo("##handskin", skinPreview))
		{
			for (int i = 0; i < (int)cat.skins.size(); i++)
			{
				bool selected = (i == s_handSkinIndex);

				ImGui::PushID(i);
				if (ImGui::Selectable(cat.skins[i].c_str(), selected))
					s_handSkinIndex = i;
				ImGui::PopID();

				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

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

	int btnW = 200, btnH = 30;
	ScaleSize(btnW, btnH);
	ImGui::SetCursorPosX((ImGui::GetWindowSize().x - (float)btnW) * 0.5f);

	if (ImGui::Button("Apply Changes", ImVec2((float)btnW, (float)btnH)))
	{
		char buf[64];

		if (!g_HandCats.empty())
		{
			// EXAMPLE: HEV Hands -> cl_hands 1, Soldier Hands -> cl_hands 2, etc.
			snprintf(buf, sizeof(buf), "cl_hands %d", s_handCatIndex + 1);
			RunCmd(buf);

			const HandCategory& catNow = g_HandCats[s_handCatIndex];
			if (!catNow.skins.empty())
			{
				// EXAMPLE: Gordon -> cl_hands_skin 1, Collete -> 2, Gina -> 3, etc.
				snprintf(buf, sizeof(buf), "cl_hands_skin %d", s_handSkinIndex + 1);
				RunCmd(buf);
			}
		}

		snprintf(buf, sizeof(buf), "cl_player_sfx_type %s", g_VoiceOptions[s_voiceIndex].value);
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

	ImGui::SetCursorPosX((ImGui::GetWindowSize().x - (float)ButtonWidth) * 0.5f);
	if (ImGui::Button("Apply HUD Color", ImVec2((float)ButtonWidth, (float)ButtonHeight)))
		ApplyColorCmd("hud_color");

	ImGui::Spacing();

	ImGui::SetCursorPosX((ImGui::GetWindowSize().x - (float)ButtonWidth) * 0.5f);
	if (ImGui::Button("Apply HUD Color Critical", ImVec2((float)ButtonWidth, (float)ButtonHeight)))
		ApplyColorCmd("hud_color_critical");

	ImGui::Spacing();

	ImGui::SetCursorPosX((ImGui::GetWindowSize().x - (float)ButtonWidth) * 0.5f);
	if (ImGui::Button("Apply Render Tool Color", ImVec2((float)ButtonWidth, (float)ButtonHeight)))
		ApplyColorCmd("render_color");

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
}

void GoModMenu_Shutdown()
{
	FreeAllTextures();
	g_Tabs.clear();
	g_ToolCats.clear();
}
