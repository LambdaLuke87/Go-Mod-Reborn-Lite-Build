// mmui_core.cpp
// Go-Mod: Reborn - Main Menu UI core
//
//  - draws a separate Dear ImGui context from the SDL_GL_SwapWindow hook, so it works in
//    the main menu (where HUD_Redraw does not run) and in the pause menu.
//  - captures mouse/keyboard through an SDL event filter while a panel is open, so clicks
//    do not go through to the GameUI behind it.
//  - loads colors/font from resource/TrackerScheme.res and texts from resource/gameui_english.txt

#include "hud.h"
#include "cl_util.h"
#include "cl_entity.h"
#include "ref_params.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"

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
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include "stb_image.h" // implementation lives in imgui_gomod_menu.cpp (PNG only)

#include "mmui.h"
#include "mmui_swaphook.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_ALPHA_TEST
#define GL_ALPHA_TEST 0x0BC0
#endif

extern ref_params_t g_refparams;  // view.cpp
extern bool g_bInputMouseActive; // inputw32.cpp

// =====================================================================
//  Globals
// =====================================================================

static ImGuiContext* s_pContext = nullptr;
static bool s_bContextFailed = false;
static bool s_bRecreateContext = false;

static bool s_bInMainMenu = true;
static bool s_bIsPaused = false;

static std::vector<const MMUIPanel*> s_Panels;

static cvar_t* s_pCvarScale = nullptr;
static cvar_t* s_pCvarEnable = nullptr;

static Uint64 s_LastCounter = 0;
static float s_flScale = 1.0f;

// input capture
static std::atomic<bool> s_bCapture{false};
static std::mutex s_EventMutex;
static std::vector<SDL_Event> s_Events;
static SDL_EventFilter s_PrevFilter = nullptr;
static void* s_PrevFilterData = nullptr;
static bool s_bWeStartedTextInput = false;

// =====================================================================
//  State
// =====================================================================

static void MMUI_UpdateState()
{
	// same as: inMainMenu = (world == nullptr); isPaused = (g_refparams.paused != 0);
	cl_entity_t* world = nullptr;

	const char* level = gEngfuncs.pfnGetLevelName();
	if (level && level[0] != '\0')
		world = gEngfuncs.GetEntityByIndex(0);

	s_bInMainMenu = (world == nullptr || world->model == nullptr) ? true : false;
	s_bIsPaused = (!s_bInMainMenu && g_refparams.paused != 0) ? true : false;
}

bool MMUI_InMainMenu() { return s_bInMainMenu; }
bool MMUI_IsPaused() { return s_bIsPaused; }

// =====================================================================
//  Panels
// =====================================================================

void MMUI_RegisterPanel(const MMUIPanel* panel)
{
	if (!panel)
		return;

	for (const MMUIPanel* p : s_Panels)
	{
		if (p == panel)
			return;
	}

	s_Panels.push_back(panel);
}

static bool MMUI_AnyPanelOpen()
{
	for (const MMUIPanel* p : s_Panels)
	{
		if (p->IsOpen && p->IsOpen())
			return true;
	}
	return false;
}

static void MMUI_CloseAllPanels()
{
	for (const MMUIPanel* p : s_Panels)
	{
		if (p->IsOpen && p->IsOpen() && p->ForceClose)
			p->ForceClose();
	}
}

// =====================================================================
//  Text files
// =====================================================================

static void AppendUTF8(std::string& out, unsigned int cp)
{
	if (cp < 0x80)
		out += (char)cp;
	else if (cp < 0x800)
	{
		out += (char)(0xC0 | (cp >> 6));
		out += (char)(0x80 | (cp & 0x3F));
	}
	else if (cp < 0x10000)
	{
		out += (char)(0xE0 | (cp >> 12));
		out += (char)(0x80 | ((cp >> 6) & 0x3F));
		out += (char)(0x80 | (cp & 0x3F));
	}
	else
	{
		out += (char)(0xF0 | (cp >> 18));
		out += (char)(0x80 | ((cp >> 12) & 0x3F));
		out += (char)(0x80 | ((cp >> 6) & 0x3F));
		out += (char)(0x80 | (cp & 0x3F));
	}
}

static void UTF16ToUTF8(const unsigned char* p, size_t bytes, bool bigEndian, std::string& out)
{
	out.clear();
	out.reserve(bytes / 2);

	for (size_t i = 0; i + 1 < bytes; i += 2)
	{
		unsigned int u = bigEndian ? ((p[i] << 8) | p[i + 1]) : (p[i] | (p[i + 1] << 8));

		if (u >= 0xD800 && u <= 0xDBFF && i + 3 < bytes)
		{
			unsigned int lo = bigEndian ? ((p[i + 2] << 8) | p[i + 3]) : (p[i + 2] | (p[i + 3] << 8));
			if (lo >= 0xDC00 && lo <= 0xDFFF)
			{
				u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
				i += 2;
			}
		}

		if (u == 0)
			break;

		AppendUTF8(out, u);
	}
}

bool MMUI_LoadTextFile(const char* path, std::string& out)
{
	out.clear();

	int len = 0;
	byte* data = gEngfuncs.COM_LoadFile((char*)path, 5, &len);
	if (!data)
		return false;

	const unsigned char* p = (const unsigned char*)data;

	if (len >= 2 && p[0] == 0xFF && p[1] == 0xFE)
		UTF16ToUTF8(p + 2, (size_t)len - 2, false, out);
	else if (len >= 2 && p[0] == 0xFE && p[1] == 0xFF)
		UTF16ToUTF8(p + 2, (size_t)len - 2, true, out);
	else if (len >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF)
		out.assign((const char*)p + 3, (size_t)len - 3);
	else
		out.assign((const char*)p, (size_t)len);

	gEngfuncs.COM_FreeFile(data);
	return true;
}

void MMUI_Tokenize(const std::string& src, std::vector<MMUIToken>& out)
{
	out.clear();

	const size_t n = src.size();
	size_t i = 0;

	while (i < n)
	{
		const char c = src[i];

		if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
		{
			i++;
			continue;
		}

		if (c == '/' && i + 1 < n && src[i + 1] == '/')
		{
			while (i < n && src[i] != '\n')
				i++;
			continue;
		}

		if (c == '{' || c == '}')
		{
			MMUIToken t;
			t.text = std::string(1, c);
			out.push_back(t);
			i++;
			continue;
		}

		if (c == '"')
		{
			MMUIToken t;
			t.quoted = true;
			i++;

			while (i < n && src[i] != '"')
			{
				if (src[i] == '\\' && i + 1 < n)
				{
					const char e = src[i + 1];
					if (e == 'n')
					{
						t.text += '\n';
						i += 2;
						continue;
					}
					if (e == 't')
					{
						t.text += '\t';
						i += 2;
						continue;
					}
					if (e == '"' || e == '\\')
					{
						t.text += e;
						i += 2;
						continue;
					}
				}

				t.text += src[i++];
			}

			i++; // closing quote
			out.push_back(t);
			continue;
		}

		MMUIToken t;
		while (i < n)
		{
			const char b = src[i];
			if (b == ' ' || b == '\t' || b == '\r' || b == '\n' || b == '{' || b == '}' || b == '"')
				break;
			t.text += b;
			i++;
		}
		out.push_back(t);
	}
}

// ---------------------------------------------------------------------
//  Small KeyValues tree (TrackerScheme.res / gameui_english.txt)
// ---------------------------------------------------------------------

static bool CaseEq(const char* a, const char* b)
{
	while (*a && *b)
	{
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return false;
		a++;
		b++;
	}
	return *a == *b;
}

struct KVNode
{
	std::string key;
	std::string value;
	bool section = false;
	std::vector<KVNode> kids;

	const KVNode* Find(const char* name) const
	{
		for (const KVNode& k : kids)
		{
			if (CaseEq(k.key.c_str(), name))
				return &k;
		}
		return nullptr;
	}
};

static bool IsConditional(const MMUIToken& t)
{
	return !t.quoted && !t.text.empty() && t.text[0] == '[';
}

static void KV_Parse(const std::vector<MMUIToken>& toks, size_t& i, KVNode& node)
{
	while (i < toks.size())
	{
		const MMUIToken& t = toks[i];

		if (!t.quoted && t.text == "}")
		{
			i++;
			return;
		}

		if (!t.quoted && t.text == "{") // section without key, just descend
		{
			i++;
			KVNode anon;
			anon.section = true;
			KV_Parse(toks, i, anon);
			node.kids.push_back(std::move(anon));
			continue;
		}

		KVNode child;
		child.key = t.text;
		i++;

		if (i < toks.size() && IsConditional(toks[i]))
			i++;

		if (i >= toks.size())
			break;

		if (!toks[i].quoted && toks[i].text == "{")
		{
			i++;
			child.section = true;
			KV_Parse(toks, i, child);
		}
		else if (!toks[i].quoted && toks[i].text == "}")
		{
			// key without value, ignore
		}
		else
		{
			child.value = toks[i].text;
			i++;

			if (i < toks.size() && IsConditional(toks[i]))
				i++;
		}

		node.kids.push_back(std::move(child));
	}
}

static bool KV_LoadFile(const char* path, KVNode& root)
{
	std::string text;
	if (!MMUI_LoadTextFile(path, text))
		return false;

	std::vector<MMUIToken> toks;
	MMUI_Tokenize(text, toks);

	root = KVNode();
	size_t i = 0;
	KV_Parse(toks, i, root);
	return true;
}

// =====================================================================
//  Localization
// =====================================================================

static std::unordered_map<std::string, std::string> s_Loc;
static bool s_bLocLoaded = false;

static std::string ToLower(const std::string& s)
{
	std::string r = s;
	for (char& c : r)
		c = (char)tolower((unsigned char)c);
	return r;
}

static void LoadLocalizationFile(const char* path)
{
	KVNode root;
	if (!KV_LoadFile(path, root))
		return;

	// "lang" { "Language" "English" "Tokens" { ... } }
	const KVNode* tokens = nullptr;
	for (const KVNode& top : root.kids)
	{
		if (!top.section)
			continue;
		tokens = top.Find("Tokens");
		if (tokens)
			break;
	}

	if (!tokens)
		return;

	int added = 0;
	for (const KVNode& k : tokens->kids)
	{
		if (k.section || k.key.empty() || k.key[0] == '[') // "[english]Key" lines in translated files
			continue;

		// first file loaded wins (gameui_english.txt has priority)
		if (s_Loc.emplace(ToLower(k.key), k.value).second)
			added++;
	}

	gEngfuncs.Con_DPrintf("MMUI: %d strings from %s\n", added, path);
}

void MMUI_ReloadLocalization()
{
	s_Loc.clear();

	LoadLocalizationFile("resource/gameui_english.txt");
	LoadLocalizationFile("resource/valve_english.txt");

	const char* gamedir = gEngfuncs.pfnGetGameDirectory();
	if (gamedir && gamedir[0])
	{
		char path[256];
		snprintf(path, sizeof(path), "resource/%s_english.txt", gamedir);
		LoadLocalizationFile(path);
	}

	LoadLocalizationFile("resource/gomod_english.txt");

	s_bLocLoaded = true;
}

const char* MMUI_Localize(const char* text)
{
	if (!text)
		return "";

	if (text[0] != '#')
		return text;

	if (!s_bLocLoaded)
		MMUI_ReloadLocalization();

	auto it = s_Loc.find(ToLower(text + 1));
	if (it != s_Loc.end())
		return it->second.c_str();

	return text + 1; // not found: show the key without '#'
}

// =====================================================================
//  Scheme
// =====================================================================

static MMUIScheme s_Scheme;
static bool s_bSchemeLoaded = false;

static ImVec4 SchemeRGBA(int r, int g, int b, int a = 255)
{
	return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

static bool ParseColor(const std::string& s, ImVec4& out)
{
	int r = 0, g = 0, b = 0, a = 255;
	const int n = sscanf(s.c_str(), "%d %d %d %d", &r, &g, &b, &a);
	if (n < 3)
		return false;

	out = SchemeRGBA(r, g, b, n >= 4 ? a : 255);
	return true;
}

static void SetSchemeDefaults(MMUIScheme& s)
{
	// values of the TrackerScheme.res shipped with the mod
	s.Text = SchemeRGBA(215, 215, 215);
	s.BrightText = SchemeRGBA(255, 255, 255);
	s.Accent = SchemeRGBA(89, 206, 183);
	s.DisabledText = SchemeRGBA(115, 115, 115);
	s.PanelBg = SchemeRGBA(64, 64, 64);
	s.ControlDarkBg = SchemeRGBA(104, 104, 104);
	s.EditBg = SchemeRGBA(48, 48, 48);
	s.ListBg = SchemeRGBA(48, 48, 48);
	s.SelectionBg = SchemeRGBA(64, 183, 174);
	s.SelectionText = SchemeRGBA(255, 255, 255);
	s.BorderBright = SchemeRGBA(78, 78, 78);
	s.BorderDark = SchemeRGBA(28, 28, 28);
	s.CheckMark = SchemeRGBA(89, 206, 183);
	s.CheckBg = SchemeRGBA(48, 48, 48);
	s.ScrollFg = SchemeRGBA(64, 64, 64);
	s.ScrollBg = SchemeRGBA(104, 104, 104);
	s.MenuBg = SchemeRGBA(64, 64, 64);
	s.MenuFg = SchemeRGBA(215, 215, 215);
	s.MenuArmedBg = SchemeRGBA(64, 183, 174);
	s.MenuArmedFg = SchemeRGBA(255, 255, 255);
	s.ArrowColor = SchemeRGBA(215, 215, 215);
	s.TitleText = SchemeRGBA(255, 255, 255);
	s.TitleButtonFg = SchemeRGBA(78, 78, 78);
	s.TitleButtonBg = SchemeRGBA(64, 64, 64);
	s.fontName = "Tahoma";
	s.fontTall = 16;
}

void MMUI_ReloadScheme()
{
	SetSchemeDefaults(s_Scheme);
	s_bSchemeLoaded = true;

	KVNode root;
	if (!KV_LoadFile("resource/TrackerScheme.res", root))
	{
		gEngfuncs.Con_Printf("MMUI: resource/TrackerScheme.res not found, using default colors\n");
		return;
	}

	const KVNode* scheme = root.Find("Scheme");
	if (!scheme)
		return;

	std::unordered_map<std::string, ImVec4> colors;

	if (const KVNode* c = scheme->Find("Colors"))
	{
		for (const KVNode& k : c->kids)
		{
			ImVec4 v;
			if (!k.section && ParseColor(k.value, v))
				colors[ToLower(k.key)] = v;
		}
	}

	auto colorByName = [&](const std::string& value, ImVec4& out) -> bool
	{
		auto it = colors.find(ToLower(value));
		if (it != colors.end())
		{
			out = it->second;
			return true;
		}
		return ParseColor(value, out);
	};

	auto fromColors = [&](const char* name, ImVec4& out)
	{
		auto it = colors.find(ToLower(name));
		if (it != colors.end())
			out = it->second;
	};

	const KVNode* base = scheme->Find("BaseSettings");

	auto fromBase = [&](const char* section, const char* key, ImVec4& out)
	{
		if (!base)
			return;

		const KVNode* node = base;
		if (section)
		{
			node = base->Find(section);
			if (!node)
				return;
		}

		const KVNode* k = node->Find(key);
		if (k && !k->section)
			colorByName(k->value, out);
	};

	MMUIScheme& s = s_Scheme;

	// raw colors first, base settings override them when present
	fromColors("ControlText", s.Text);
	fromColors("BrightBaseText", s.BrightText);
	fromColors("BrightControlText", s.Accent);
	fromColors("DisabledText1", s.DisabledText);
	fromColors("ControlBG", s.PanelBg);
	fromColors("ControlDarkBG", s.ControlDarkBg);
	fromColors("WindowBG", s.EditBg);
	fromColors("ListBG", s.ListBg);
	fromColors("SelectionBG", s.SelectionBg);
	fromColors("SelectedText", s.SelectionText);
	fromColors("BorderBright", s.BorderBright);
	fromColors("BorderDark", s.BorderDark);
	fromColors("TitleText", s.TitleText);

	fromBase(nullptr, "FgColor", s.Text);
	fromBase(nullptr, "BgColor", s.PanelBg);
	fromBase(nullptr, "DisabledFgColor1", s.DisabledText);
	fromBase(nullptr, "WindowBgColor", s.EditBg);
	fromBase(nullptr, "ListBgColor", s.ListBg);
	fromBase(nullptr, "SelectionBgColor", s.SelectionBg);
	fromBase(nullptr, "SelectionFgColor", s.SelectionText);
	fromBase(nullptr, "CheckButtonCheck", s.CheckMark);
	fromBase(nullptr, "CheckBgColor", s.CheckBg);
	fromBase(nullptr, "TitleBarFgColor", s.TitleText);
	fromBase(nullptr, "TitleButtonFgColor", s.TitleButtonFg);
	fromBase(nullptr, "TitleButtonBgColor", s.TitleButtonBg);

	s.MenuBg = s.PanelBg;
	s.MenuFg = s.Text;
	s.MenuArmedBg = s.SelectionBg;
	s.MenuArmedFg = s.BrightText;
	fromBase("Menu", "BgColor", s.MenuBg);
	fromBase("Menu", "FgColor", s.MenuFg);
	fromBase("Menu", "ArmedBgColor", s.MenuArmedBg);
	fromBase("Menu", "ArmedFgColor", s.MenuArmedFg);

	s.ArrowColor = s.Text;
	fromBase("MenuButton", "ButtonArrowColor", s.ArrowColor);

	s.ScrollFg = s.PanelBg;
	s.ScrollBg = s.ControlDarkBg;
	fromBase("ScrollBarSlider", "ScrollBarSliderFgColor", s.ScrollFg);
	fromBase("ScrollBarSlider", "ScrollBarSliderBgColor", s.ScrollBg);

	// font: "Fonts" -> "Default" -> "1" -> name / tall
	if (const KVNode* fonts = scheme->Find("Fonts"))
	{
		if (const KVNode* def = fonts->Find("Default"))
		{
			for (const KVNode& entry : def->kids)
			{
				if (!entry.section)
					continue;

				const KVNode* name = entry.Find("name");
				const KVNode* tall = entry.Find("tall");

				if (name && !name->value.empty())
					s.fontName = name->value;
				if (tall && atoi(tall->value.c_str()) > 0)
					s.fontTall = atoi(tall->value.c_str());
				break; // first entry is enough
			}
		}
	}
}

const MMUIScheme& MMUI_GetScheme()
{
	if (!s_bSchemeLoaded)
		MMUI_ReloadScheme();
	return s_Scheme;
}

float MMUI_Scale()
{
	return s_flScale;
}

// =====================================================================
//  Textures
// =====================================================================

typedef void(APIENTRY* PFN_MMUI_GenTextures)(GLsizei, GLuint*);
typedef void(APIENTRY* PFN_MMUI_DeleteTextures)(GLsizei, const GLuint*);
typedef void(APIENTRY* PFN_MMUI_BindTexture)(GLenum, GLuint);
typedef void(APIENTRY* PFN_MMUI_TexParameteri)(GLenum, GLenum, GLint);
typedef void(APIENTRY* PFN_MMUI_TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
typedef void(APIENTRY* PFN_MMUI_GetIntegerv)(GLenum, GLint*);
typedef void(APIENTRY* PFN_MMUI_PixelStorei)(GLenum, GLint);
typedef GLboolean(APIENTRY* PFN_MMUI_IsEnabled)(GLenum);
typedef void(APIENTRY* PFN_MMUI_Enable)(GLenum);
typedef void(APIENTRY* PFN_MMUI_Disable)(GLenum);

static PFN_MMUI_GenTextures pglGenTextures;
static PFN_MMUI_DeleteTextures pglDeleteTextures;
static PFN_MMUI_BindTexture pglBindTexture;
static PFN_MMUI_TexParameteri pglTexParameteri;
static PFN_MMUI_TexImage2D pglTexImage2D;
static PFN_MMUI_GetIntegerv pglGetIntegerv;
static PFN_MMUI_PixelStorei pglPixelStorei;
static PFN_MMUI_IsEnabled pglIsEnabled;
static PFN_MMUI_Enable pglEnable;
static PFN_MMUI_Disable pglDisable;

static bool MMUI_LoadGL()
{
	static int state = 0;
	if (state != 0)
		return state > 0;

	pglGenTextures = (PFN_MMUI_GenTextures)SDL_GL_GetProcAddress("glGenTextures");
	pglDeleteTextures = (PFN_MMUI_DeleteTextures)SDL_GL_GetProcAddress("glDeleteTextures");
	pglBindTexture = (PFN_MMUI_BindTexture)SDL_GL_GetProcAddress("glBindTexture");
	pglTexParameteri = (PFN_MMUI_TexParameteri)SDL_GL_GetProcAddress("glTexParameteri");
	pglTexImage2D = (PFN_MMUI_TexImage2D)SDL_GL_GetProcAddress("glTexImage2D");
	pglGetIntegerv = (PFN_MMUI_GetIntegerv)SDL_GL_GetProcAddress("glGetIntegerv");
	pglPixelStorei = (PFN_MMUI_PixelStorei)SDL_GL_GetProcAddress("glPixelStorei");
	pglIsEnabled = (PFN_MMUI_IsEnabled)SDL_GL_GetProcAddress("glIsEnabled");
	pglEnable = (PFN_MMUI_Enable)SDL_GL_GetProcAddress("glEnable");
	pglDisable = (PFN_MMUI_Disable)SDL_GL_GetProcAddress("glDisable");

	const bool ok = pglGenTextures && pglDeleteTextures && pglBindTexture && pglTexParameteri &&
					pglTexImage2D && pglGetIntegerv && pglIsEnabled && pglEnable && pglDisable;

	state = ok ? 1 : -1;
	if (!ok)
		gEngfuncs.Con_Printf("MMUI: could not load OpenGL functions\n");
	return ok;
}

unsigned int MMUI_LoadPNG(const char* path, int* outW, int* outH)
{
	if (!path || !path[0] || !MMUI_LoadGL())
		return 0;

	int len = 0;
	byte* file = gEngfuncs.COM_LoadFile((char*)path, 5, &len);
	if (!file)
		return 0;

	int w = 0, h = 0, comp = 0;
	unsigned char* pixels = stbi_load_from_memory((const unsigned char*)file, len, &w, &h, &comp, 4);
	gEngfuncs.COM_FreeFile(file);

	if (!pixels)
	{
		gEngfuncs.Con_Printf("MMUI: invalid PNG %s\n", path);
		return 0;
	}

	GLint lastTexture = 0;
	pglGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexture);

	GLint lastUnpack = 4;
	if (pglPixelStorei)
	{
		pglGetIntegerv(GL_UNPACK_ALIGNMENT, &lastUnpack);
		pglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	}

	GLuint tex = 0;
	pglGenTextures(1, &tex);
	pglBindTexture(GL_TEXTURE_2D, tex);
	pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	pglBindTexture(GL_TEXTURE_2D, (GLuint)lastTexture);

	if (pglPixelStorei)
		pglPixelStorei(GL_UNPACK_ALIGNMENT, lastUnpack);

	stbi_image_free(pixels);

	if (outW)
		*outW = w;
	if (outH)
		*outH = h;

	return tex;
}

// ---------------------------------------------------------------------
//  TGA (the stb_image in this mod is compiled with PNG only)
// ---------------------------------------------------------------------

// Decodes uncompressed (2) / RLE (10) truecolor TGA, 24 or 32 bits, into RGBA.
static bool DecodeTGA(const unsigned char* data, int len, std::vector<unsigned char>& rgba, int& w, int& h)
{
	if (len < 18)
		return false;

	const int idLength = data[0];
	const int colorMapType = data[1];
	const int imageType = data[2];
	w = data[12] | (data[13] << 8);
	h = data[14] | (data[15] << 8);
	const int bpp = data[16];
	const int descriptor = data[17];

	if (colorMapType != 0 || (imageType != 2 && imageType != 10) || (bpp != 24 && bpp != 32) || w <= 0 || h <= 0)
		return false;

	const int bytesPP = bpp / 8;
	const int pixelCount = w * h;
	const unsigned char* p = data + 18 + idLength;
	const unsigned char* end = data + len;

	std::vector<unsigned char> raw((size_t)pixelCount * 4);

	auto readPixel = [&](unsigned char* out) -> bool
	{
		if (p + bytesPP > end)
			return false;
		out[0] = p[2]; // TGA is BGR(A)
		out[1] = p[1];
		out[2] = p[0];
		out[3] = (bytesPP == 4) ? p[3] : 255;
		p += bytesPP;
		return true;
	};

	int i = 0;
	if (imageType == 2)
	{
		for (; i < pixelCount; i++)
		{
			if (!readPixel(&raw[(size_t)i * 4]))
				return false;
		}
	}
	else
	{
		while (i < pixelCount)
		{
			if (p >= end)
				return false;

			const int header = *p++;
			const int count = (header & 0x7F) + 1;

			if (header & 0x80) // run
			{
				unsigned char px[4];
				if (!readPixel(px))
					return false;
				for (int n = 0; n < count && i < pixelCount; n++, i++)
					memcpy(&raw[(size_t)i * 4], px, 4);
			}
			else // raw packet
			{
				for (int n = 0; n < count && i < pixelCount; n++, i++)
				{
					if (!readPixel(&raw[(size_t)i * 4]))
						return false;
				}
			}
		}
	}

	// origin: bottom-left unless bit 5 is set
	const bool topDown = (descriptor & 0x20) != 0;
	rgba.resize(raw.size());
	for (int y = 0; y < h; y++)
	{
		const int srcY = topDown ? y : (h - 1 - y);
		memcpy(&rgba[(size_t)y * w * 4], &raw[(size_t)srcY * w * 4], (size_t)w * 4);
	}

	return true;
}

static unsigned int MMUI_LoadTGA(const char* path)
{
	if (!MMUI_LoadGL())
		return 0;

	int len = 0;
	byte* file = gEngfuncs.COM_LoadFile((char*)path, 5, &len);
	if (!file)
	{
		gEngfuncs.Con_DPrintf("MMUI: %s not found\n", path);
		return 0;
	}

	std::vector<unsigned char> rgba;
	int w = 0, h = 0;
	const bool ok = DecodeTGA((const unsigned char*)file, len, rgba, w, h);
	gEngfuncs.COM_FreeFile(file);

	if (!ok)
	{
		gEngfuncs.Con_Printf("MMUI: unsupported TGA %s (only 24/32 bit, raw or RLE)\n", path);
		return 0;
	}

	GLint lastTexture = 0;
	pglGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexture);

	GLuint tex = 0;
	pglGenTextures(1, &tex);
	pglBindTexture(GL_TEXTURE_2D, tex);
	pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
	pglBindTexture(GL_TEXTURE_2D, (GLuint)lastTexture);

	return tex;
}

void MMUI_FreeTexture(unsigned int tex)
{
	if (tex && pglDeleteTextures)
	{
		GLuint t = tex;
		pglDeleteTextures(1, &t);
	}
}

// =====================================================================
//  Fonts
// =====================================================================

static bool ReadDiskFile(const char* path, std::vector<unsigned char>& out)
{
	FILE* f = fopen(path, "rb");
	if (!f)
		return false;

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (size <= 0)
	{
		fclose(f);
		return false;
	}

	out.resize((size_t)size);
	const size_t read = fread(out.data(), 1, (size_t)size, f);
	fclose(f);
	return read == (size_t)size;
}

static bool ReadGameFile(const char* path, std::vector<unsigned char>& out)
{
	int len = 0;
	byte* data = gEngfuncs.COM_LoadFile((char*)path, 5, &len);
	if (!data)
		return false;

	out.assign((unsigned char*)data, (unsigned char*)data + len);
	gEngfuncs.COM_FreeFile(data);
	return len > 0;
}

static std::string SystemFontFileName(const std::string& name)
{
	const std::string lower = ToLower(name);

	if (lower == "trebuchet ms")
		return "trebuc.ttf";
	if (lower == "courier new")
		return "cour.ttf";
	if (lower == "times new roman")
		return "times.ttf";
	if (lower == "lucida console")
		return "lucon.ttf";

	std::string file;
	for (char c : lower)
	{
		if (c != ' ')
			file += c;
	}
	return file + ".ttf";
}

static bool FindFontData(const std::string& name, std::vector<unsigned char>& data)
{
	char path[512];

	// 1) the mod can ship its own font
	snprintf(path, sizeof(path), "resource/fonts/%s.ttf", name.c_str());
	if (ReadGameFile(path, data))
		return true;

	snprintf(path, sizeof(path), "resource/%s.ttf", name.c_str());
	if (ReadGameFile(path, data))
		return true;

	const std::string sysFile = SystemFontFileName(name);

#ifdef _WIN32
	const char* windir = getenv("WINDIR");
	if (!windir)
		windir = getenv("SystemRoot");
	if (!windir)
		windir = "C:\\Windows";

	snprintf(path, sizeof(path), "%s\\Fonts\\%s", windir, sysFile.c_str());
	if (ReadDiskFile(path, data))
		return true;
#else
	const char* dirs[] = {
		"/usr/share/fonts/truetype/msttcorefonts",
		"/usr/share/fonts/TTF",
		"/usr/share/fonts/truetype",
		"/usr/local/share/fonts",
	};

	for (const char* dir : dirs)
	{
		snprintf(path, sizeof(path), "%s/%s", dir, sysFile.c_str());
		if (ReadDiskFile(path, data))
			return true;

		snprintf(path, sizeof(path), "%s/%s.ttf", dir, name.c_str());
		if (ReadDiskFile(path, data))
			return true;
	}

	const char* fallbacks[] = {
		"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
		"/usr/share/fonts/TTF/DejaVuSans.ttf",
		"/usr/share/fonts/dejavu/DejaVuSans.ttf",
		"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
	};

	for (const char* fb : fallbacks)
	{
		if (ReadDiskFile(fb, data))
			return true;
	}
#endif

	return false;
}

static void LoadFonts()
{
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Clear();

	const MMUIScheme& scheme = MMUI_GetScheme();

	std::vector<unsigned char> data;
	if (FindFontData(scheme.fontName, data))
	{
		void* mem = IM_ALLOC(data.size());
		memcpy(mem, data.data(), data.size());

		ImFontConfig cfg;
		cfg.FontDataOwnedByAtlas = true; // the atlas frees mem
		if (io.Fonts->AddFontFromMemoryTTF(mem, (int)data.size(), (float)scheme.fontTall, &cfg))
			return;
	}

	gEngfuncs.Con_DPrintf("MMUI: font '%s' not found, using the ImGui default font\n", scheme.fontName.c_str());
	io.Fonts->AddFontDefaultVector();
}

// =====================================================================
//  Clipboard
// =====================================================================

static const char* MMUI_GetClipboard(ImGuiContext*)
{
	static char* s_pClip = nullptr;
	if (s_pClip)
		SDL_free(s_pClip);
	s_pClip = SDL_GetClipboardText();
	return s_pClip;
}

static void MMUI_SetClipboard(ImGuiContext*, const char* text)
{
	SDL_SetClipboardText(text);
}

// =====================================================================
//  Context
// =====================================================================

static void MMUI_DestroyContext()
{
	if (!s_pContext)
		return;

	ImGuiContext* ctx = s_pContext;
	ImGuiContext* prev = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(ctx);

	ImGui_ImplOpenGL3_Shutdown();
	ImGui::DestroyContext(ctx);
	s_pContext = nullptr;

	ImGui::SetCurrentContext(prev != ctx ? prev : nullptr);
}

static bool MMUI_EnsureContext()
{
	if (s_pContext)
		return true;
	if (s_bContextFailed)
		return false;

	ImGuiContext* prev = ImGui::GetCurrentContext();

	s_pContext = ImGui::CreateContext();
	ImGui::SetCurrentContext(s_pContext);

	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr; // nothing to save
	io.LogFilename = nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	io.BackendPlatformName = "gomod_mmui_sdl";

	ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
	pio.Platform_GetClipboardTextFn = MMUI_GetClipboard;
	pio.Platform_SetClipboardTextFn = MMUI_SetClipboard;

	if (!ImGui_ImplOpenGL3_Init("#version 120"))
	{
		gEngfuncs.Con_Printf("MMUI: ImGui OpenGL3 backend failed to start\n");
		ImGui::DestroyContext(s_pContext);
		s_pContext = nullptr;
		s_bContextFailed = true;
		ImGui::SetCurrentContext(prev);
		return false;
	}

	LoadFonts();

	ImGui::SetCurrentContext(prev);
	s_LastCounter = 0;
	return true;
}

// =====================================================================
//  Input (SDL event filter)
// =====================================================================

static bool IsCapturedEvent(Uint32 type)
{
	switch (type)
	{
	case SDL_KEYDOWN:
	case SDL_KEYUP:
	case SDL_TEXTINPUT:
	case SDL_TEXTEDITING:
	case SDL_MOUSEMOTION:
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEWHEEL:
		return true;
	default:
		return false;
	}
}

static int SDLCALL MMUI_EventFilter(void* userdata, SDL_Event* event)
{
	if (s_bCapture.load() && IsCapturedEvent(event->type))
	{
		std::lock_guard<std::mutex> lock(s_EventMutex);
		if (s_Events.size() < 1024)
			s_Events.push_back(*event);
		return 0; // the engine / GameUI does not get it
	}

	if (s_PrevFilter)
		return s_PrevFilter(s_PrevFilterData, event);

	return 1;
}

static void MMUI_EnsureEventFilter()
{
	SDL_EventFilter current = nullptr;
	void* currentData = nullptr;
	SDL_GetEventFilter(&current, &currentData);

	if (current == MMUI_EventFilter)
		return;

	// chain whatever filter the engine had
	s_PrevFilter = current;
	s_PrevFilterData = currentData;
	SDL_SetEventFilter(MMUI_EventFilter, nullptr);
}

static void MMUI_RemoveEventFilter()
{
	SDL_EventFilter current = nullptr;
	void* currentData = nullptr;
	SDL_GetEventFilter(&current, &currentData);

	if (current == MMUI_EventFilter)
		SDL_SetEventFilter(s_PrevFilter, s_PrevFilterData);

	s_PrevFilter = nullptr;
	s_PrevFilterData = nullptr;
}

static ImGuiKey SDLKeyToImGui(SDL_Keycode key)
{
	if (key >= SDLK_a && key <= SDLK_z)
		return (ImGuiKey)(ImGuiKey_A + (key - SDLK_a));
	if (key >= SDLK_0 && key <= SDLK_9)
		return (ImGuiKey)(ImGuiKey_0 + (key - SDLK_0));

	switch (key)
	{
	case SDLK_TAB: return ImGuiKey_Tab;
	case SDLK_LEFT: return ImGuiKey_LeftArrow;
	case SDLK_RIGHT: return ImGuiKey_RightArrow;
	case SDLK_UP: return ImGuiKey_UpArrow;
	case SDLK_DOWN: return ImGuiKey_DownArrow;
	case SDLK_PAGEUP: return ImGuiKey_PageUp;
	case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
	case SDLK_HOME: return ImGuiKey_Home;
	case SDLK_END: return ImGuiKey_End;
	case SDLK_INSERT: return ImGuiKey_Insert;
	case SDLK_DELETE: return ImGuiKey_Delete;
	case SDLK_BACKSPACE: return ImGuiKey_Backspace;
	case SDLK_SPACE: return ImGuiKey_Space;
	case SDLK_RETURN: return ImGuiKey_Enter;
	case SDLK_KP_ENTER: return ImGuiKey_KeypadEnter;
	case SDLK_ESCAPE: return ImGuiKey_Escape;
	case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
	case SDLK_RCTRL: return ImGuiKey_RightCtrl;
	case SDLK_LSHIFT: return ImGuiKey_LeftShift;
	case SDLK_RSHIFT: return ImGuiKey_RightShift;
	case SDLK_LALT: return ImGuiKey_LeftAlt;
	case SDLK_RALT: return ImGuiKey_RightAlt;
	case SDLK_LGUI: return ImGuiKey_LeftSuper;
	case SDLK_RGUI: return ImGuiKey_RightSuper;
	default: return ImGuiKey_None;
	}
}

static void MMUI_FeedEvents(ImGuiIO& io, SDL_Window* window)
{
	std::vector<SDL_Event> events;
	{
		std::lock_guard<std::mutex> lock(s_EventMutex);
		events.swap(s_Events);
	}

	for (const SDL_Event& e : events)
	{
		switch (e.type)
		{
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
		{
			int button = -1;
			if (e.button.button == SDL_BUTTON_LEFT)
				button = ImGuiMouseButton_Left;
			else if (e.button.button == SDL_BUTTON_RIGHT)
				button = ImGuiMouseButton_Right;
			else if (e.button.button == SDL_BUTTON_MIDDLE)
				button = ImGuiMouseButton_Middle;

			if (button >= 0)
			{
				io.AddMousePosEvent((float)e.button.x, (float)e.button.y);
				io.AddMouseButtonEvent(button, e.type == SDL_MOUSEBUTTONDOWN);
			}
			break;
		}
		case SDL_MOUSEWHEEL:
			io.AddMouseWheelEvent((float)e.wheel.x, (float)e.wheel.y);
			break;
		case SDL_MOUSEMOTION:
			io.AddMousePosEvent((float)e.motion.x, (float)e.motion.y);
			break;
		case SDL_TEXTINPUT:
			io.AddInputCharactersUTF8(e.text.text);
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
		{
			const bool down = (e.type == SDL_KEYDOWN);
			const Uint16 mod = e.key.keysym.mod;

			io.AddKeyEvent(ImGuiMod_Ctrl, (mod & KMOD_CTRL) != 0);
			io.AddKeyEvent(ImGuiMod_Shift, (mod & KMOD_SHIFT) != 0);
			io.AddKeyEvent(ImGuiMod_Alt, (mod & KMOD_ALT) != 0);
			io.AddKeyEvent(ImGuiMod_Super, (mod & KMOD_GUI) != 0);

			const ImGuiKey key = SDLKeyToImGui(e.key.keysym.sym);
			if (key != ImGuiKey_None)
				io.AddKeyEvent(key, down);
			break;
		}
		default:
			break;
		}
	}

	// position fallback (if some event was missed, the real cursor position always wins)
	int mx = 0, my = 0;
	SDL_GetMouseState(&mx, &my);
	io.AddMousePosEvent((float)mx, (float)my);
}

static void MMUI_StopCapture()
{
	s_bCapture = false;

	{
		std::lock_guard<std::mutex> lock(s_EventMutex);
		s_Events.clear();
	}

	if (s_bWeStartedTextInput)
	{
		SDL_StopTextInput();
		s_bWeStartedTextInput = false;
	}

	if (s_pContext)
	{
		ImGuiContext* prev = ImGui::GetCurrentContext();
		ImGui::SetCurrentContext(s_pContext);

		ImGuiIO& io = ImGui::GetIO();
		io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
		io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
		io.AddMouseButtonEvent(ImGuiMouseButton_Middle, false);
		io.ClearInputKeys();

		ImGui::SetCurrentContext(prev);
	}
}

// =====================================================================
//  Style
// =====================================================================

static ImVec4 Lighten(const ImVec4& c, float amount)
{
	return ImVec4((std::min)(c.x + amount, 1.0f), (std::min)(c.y + amount, 1.0f), (std::min)(c.z + amount, 1.0f), c.w);
}

static ImVec4 WithAlpha(const ImVec4& c, float a)
{
	return ImVec4(c.x, c.y, c.z, a);
}

static void MMUI_ApplyStyle(float fontPx)
{
	const MMUIScheme& s = MMUI_GetScheme();
	const float k = s_flScale;

	ImGuiStyle& st = ImGui::GetStyle();

	st.FontSizeBase = fontPx;

	st.WindowRounding = 0.0f;
	st.ChildRounding = 0.0f;
	st.FrameRounding = 0.0f;
	st.PopupRounding = 0.0f;
	st.ScrollbarRounding = 0.0f;
	st.GrabRounding = 0.0f;
	st.TabRounding = 0.0f;

	st.WindowBorderSize = 1.0f;
	st.ChildBorderSize = 1.0f;
	st.PopupBorderSize = 1.0f;
	st.FrameBorderSize = 0.0f;

	st.WindowPadding = ImVec2(8.0f * k, 8.0f * k);
	st.FramePadding = ImVec2(6.0f * k, 4.0f * k);
	st.ItemSpacing = ImVec2(8.0f * k, 6.0f * k);
	st.ItemInnerSpacing = ImVec2(8.0f * k, 4.0f * k);
	st.ScrollbarSize = 18.0f * k;
	st.GrabMinSize = 16.0f * k;
	st.SelectableTextAlign = ImVec2(0.0f, 0.5f);

	ImVec4* c = st.Colors;
	c[ImGuiCol_Text] = s.Text;
	c[ImGuiCol_TextDisabled] = s.DisabledText;
	c[ImGuiCol_WindowBg] = s.PanelBg;
	c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
	c[ImGuiCol_PopupBg] = s.MenuBg;
	c[ImGuiCol_Border] = s.BorderDark;
	c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
	c[ImGuiCol_FrameBg] = s.EditBg;
	c[ImGuiCol_FrameBgHovered] = Lighten(s.EditBg, 0.03f);
	c[ImGuiCol_FrameBgActive] = Lighten(s.EditBg, 0.05f);
	c[ImGuiCol_TitleBg] = s.PanelBg;
	c[ImGuiCol_TitleBgActive] = s.PanelBg;
	c[ImGuiCol_TitleBgCollapsed] = s.PanelBg;
	c[ImGuiCol_MenuBarBg] = s.PanelBg;
	c[ImGuiCol_ScrollbarBg] = s.ScrollBg;
	c[ImGuiCol_ScrollbarGrab] = s.ScrollFg;
	c[ImGuiCol_ScrollbarGrabHovered] = Lighten(s.ScrollFg, 0.05f);
	c[ImGuiCol_ScrollbarGrabActive] = Lighten(s.ScrollFg, 0.10f);
	c[ImGuiCol_CheckMark] = s.CheckMark;
	c[ImGuiCol_SliderGrab] = s.ControlDarkBg;
	c[ImGuiCol_SliderGrabActive] = s.SelectionBg;
	c[ImGuiCol_Button] = s.PanelBg;
	c[ImGuiCol_ButtonHovered] = Lighten(s.PanelBg, 0.06f);
	c[ImGuiCol_ButtonActive] = s.BorderDark;
	c[ImGuiCol_Header] = s.SelectionBg;
	c[ImGuiCol_HeaderHovered] = WithAlpha(s.MenuArmedBg, 0.55f);
	c[ImGuiCol_HeaderActive] = s.SelectionBg;
	c[ImGuiCol_Separator] = s.BorderDark;
	c[ImGuiCol_SeparatorHovered] = s.BorderBright;
	c[ImGuiCol_SeparatorActive] = s.BorderBright;
	c[ImGuiCol_TextSelectedBg] = WithAlpha(s.SelectionBg, 0.6f);
	c[ImGuiCol_NavCursor] = WithAlpha(s.SelectionBg, 0.0f);
	c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.35f);
}

// =====================================================================
//  Frame (called from the swap hook)
// =====================================================================

static void MMUI_OnSwap(void* sdlWindow)
{
	if (s_Panels.empty())
		return;

	if (s_bRecreateContext)
	{
		MMUI_StopCapture();
		MMUI_DestroyContext();
		s_bRecreateContext = false;
		s_bContextFailed = false;
	}

	MMUI_UpdateState();

	bool anyOpen = MMUI_AnyPanelOpen();

	// the player went back to the game (no main menu, not paused and the game owns the mouse)
	const bool uiVisible = s_bInMainMenu || s_bIsPaused || !g_bInputMouseActive;
	if (anyOpen && !uiVisible)
	{
		MMUI_CloseAllPanels();
		anyOpen = false;
	}

	if (!anyOpen)
	{
		if (s_bCapture)
			MMUI_StopCapture();
		return;
	}

	SDL_Window* window = (SDL_Window*)sdlWindow;
	if (!window)
		window = SDL_GL_GetCurrentWindow();
	if (!window)
		return;

	if (!MMUI_EnsureContext())
		return;

	MMUI_EnsureEventFilter();
	s_bCapture = true;

	ImGuiContext* prev = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(s_pContext);

	ImGuiIO& io = ImGui::GetIO();

	int ww = 0, wh = 0, dw = 0, dh = 0;
	SDL_GetWindowSize(window, &ww, &wh);
	SDL_GL_GetDrawableSize(window, &dw, &dh);
	if (ww <= 0 || wh <= 0)
	{
		ImGui::SetCurrentContext(prev);
		return;
	}

	io.DisplaySize = ImVec2((float)ww, (float)wh);
	io.DisplayFramebufferScale = ImVec2(dw > 0 ? (float)dw / ww : 1.0f, dh > 0 ? (float)dh / wh : 1.0f);

	const Uint64 now = SDL_GetPerformanceCounter();
	float dt = 1.0f / 60.0f;
	if (s_LastCounter != 0)
		dt = (float)((double)(now - s_LastCounter) / (double)SDL_GetPerformanceFrequency());
	s_LastCounter = now;
	io.DeltaTime = (dt > 0.0f && dt < 0.25f) ? dt : 1.0f / 60.0f;

	// font size / layout scale: TrackerScheme "tall" scaled with the screen height
	const MMUIScheme& scheme = MMUI_GetScheme();
	float uiScale = (s_pCvarScale && s_pCvarScale->value > 0.0f) ? s_pCvarScale->value : (float)wh / 864.0f;
	uiScale = (std::max)(1.0f, (std::min)(uiScale, 3.0f));
	const float fontPx = std::floor(scheme.fontTall * uiScale);
	s_flScale = fontPx / 20.0f;

	MMUI_FeedEvents(io, window);
	MMUI_ApplyStyle(fontPx);

	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	for (const MMUIPanel* p : s_Panels)
	{
		if (p->IsOpen && p->IsOpen() && p->Draw)
			p->Draw();
	}

	ImGui::Render();

	// fixed function alpha test would eat the antialiased edges of the text
	const bool alphaTest = pglIsEnabled ? (pglIsEnabled(GL_ALPHA_TEST) != 0) : false;
	if (alphaTest)
		pglDisable(GL_ALPHA_TEST);

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	if (alphaTest)
		pglEnable(GL_ALPHA_TEST);

	// text entries need SDL text input events
	if (io.WantTextInput && !SDL_IsTextInputActive())
	{
		SDL_StartTextInput();
		s_bWeStartedTextInput = true;
	}
	else if (!io.WantTextInput && s_bWeStartedTextInput)
	{
		SDL_StopTextInput();
		s_bWeStartedTextInput = false;
	}

	ImGui::SetCurrentContext(prev);

	if (!MMUI_AnyPanelOpen())
		MMUI_StopCapture();
}

bool MMUI_EnsureHook()
{
	if (MMUI_IsSwapHookInstalled())
		return true;

	if (s_pCvarEnable && s_pCvarEnable->value == 0.0f)
	{
		gEngfuncs.Con_Printf("MMUI: disabled by cl_gomod_mainmenu_ui 0\n");
		return false;
	}

	MMUI_LoadGL();

	if (!MMUI_InstallSwapHook(MMUI_OnSwap))
	{
		gEngfuncs.Con_Printf("MMUI: could not hook SDL_GL_SwapWindow, menus unavailable on this engine\n");
		return false;
	}

	return true;
}

// =====================================================================
//  Commands / lifetime
// =====================================================================

static void MMUI_Cmd_Reload()
{
	MMUI_ReloadScheme();
	MMUI_ReloadLocalization();
	s_bRecreateContext = true; // fonts depend on the scheme
	gEngfuncs.Con_Printf("MMUI: scheme and localization reloaded\n");
}

extern void MMUI_CreateServer_Register(); // mmui_createserver.cpp

void MMUI_Init()
{
	s_pCvarScale = gEngfuncs.pfnRegisterVariable("cl_gomod_mainmenu_scale", "0", FCVAR_ARCHIVE);
	s_pCvarEnable = gEngfuncs.pfnRegisterVariable("cl_gomod_mainmenu_ui", "1", FCVAR_ARCHIVE);
	gEngfuncs.pfnAddCommand("mmui_reload", MMUI_Cmd_Reload);

	// built-in menus
	MMUI_CreateServer_Register();
}

void MMUI_Shutdown()
{
	MMUI_RemoveSwapHook();
	MMUI_StopCapture();
	MMUI_RemoveEventFilter();
	MMUI_DestroyContext();
}

// =====================================================================
//  Widgets
// =====================================================================

bool MMUI_BeginDialog(const char* id, const char* title, const ImVec2& size)
{
	const MMUIScheme& s = MMUI_GetScheme();
	const float k = s_flScale;
	ImGuiIO& io = ImGui::GetIO();

	ImVec2 sz((std::min)(size.x, io.DisplaySize.x - 8.0f), (std::min)(size.y, io.DisplaySize.y - 8.0f));

	ImGui::SetNextWindowSize(sz, ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin(id, nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::PopStyleVar();

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 wp = ImGui::GetWindowPos();
	const ImVec2 ws = ImGui::GetWindowSize();
	const float titleH = 40.0f * k;

	// icon: resource/icon_steam.tga (same as Valve's dialogs), lambda glyph as fallback
	static unsigned int s_TitleIcon = 0;
	static bool s_bTitleIconTried = false;
	if (!s_bTitleIconTried)
	{
		s_TitleIcon = MMUI_LoadTGA("resource/icon_steam.tga");
		s_bTitleIconTried = true;
	}

	const ImVec2 iconCenter(wp.x + 22.0f * k, wp.y + titleH * 0.5f);
	ImFont* font = ImGui::GetFont();
	float textX = wp.x + 14.0f * k;

	if (s_TitleIcon)
	{
		const float half = 10.0f * k;
		dl->AddImage((ImTextureID)(intptr_t)s_TitleIcon, ImVec2(iconCenter.x - half, iconCenter.y - half), ImVec2(iconCenter.x + half, iconCenter.y + half));
		textX = iconCenter.x + half + 8.0f * k;
	}
	else if (font && font->IsGlyphInFont((ImWchar)0x03BB))
	{
		const float r = 10.0f * k;
		dl->AddCircle(iconCenter, r, MMUI_Col(s.TitleText), 0, 1.5f * k);

		const char* lambda = "\xCE\xBB"; // U+03BB
		const float fs = ImGui::GetFontSize() * 0.85f;
		ImVec2 ls = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, lambda);
		dl->AddText(font, fs, ImVec2(iconCenter.x - ls.x * 0.5f, iconCenter.y - ls.y * 0.5f), MMUI_Col(s.TitleText), lambda);
		textX = iconCenter.x + r + 8.0f * k;
	}

	const ImVec2 ts = ImGui::CalcTextSize(title);
	dl->AddText(ImVec2(textX, wp.y + (titleH - ts.y) * 0.5f), MMUI_Col(s.TitleText), title);

	// close button
	const float bs = 26.0f * k;
	const ImVec2 bmin(wp.x + ws.x - bs - 10.0f * k, wp.y + (titleH - bs) * 0.5f);
	ImGui::SetCursorScreenPos(bmin);
	const bool closePressed = ImGui::InvisibleButton("##mmui_close", ImVec2(bs, bs));
	const bool closeHovered = ImGui::IsItemHovered();

	const ImVec2 bmax(bmin.x + bs, bmin.y + bs);
	dl->AddRectFilled(bmin, bmax, MMUI_Col(closeHovered ? Lighten(s.TitleButtonBg, 0.08f) : s.TitleButtonBg));
	dl->AddRect(bmin, bmax, MMUI_Col(s.BorderDark));

	const float pad = bs * 0.28f;
	const ImU32 xcol = MMUI_Col(closeHovered ? s.BrightText : s.TitleText);
	dl->AddLine(ImVec2(bmin.x + pad, bmin.y + pad), ImVec2(bmax.x - pad, bmax.y - pad), xcol, 2.5f * k);
	dl->AddLine(ImVec2(bmax.x - pad, bmin.y + pad), ImVec2(bmin.x + pad, bmax.y - pad), xcol, 2.5f * k);

	ImGui::SetCursorPos(ImVec2(0.0f, titleH));
	return !closePressed;
}

void MMUI_EndDialog()
{
	ImGui::End();
}

bool MMUI_TabStrip(const char* id, const char* const* labels, int count, int* selected, float minTabWidth)
{
	const MMUIScheme& s = MMUI_GetScheme();
	const float k = s_flScale;

	ImGui::PushID(id);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 start = ImGui::GetCursorScreenPos();
	const float h = ImGui::GetFontSize() + 14.0f * k;
	const float padX = 8.0f * k;

	float x = start.x;
	bool changed = false;

	for (int i = 0; i < count; i++)
	{
		const char* label = labels[i];
		const ImVec2 ts = ImGui::CalcTextSize(label);
		const float w = (std::max)(ts.x + padX * 2.0f, minTabWidth);

		ImGui::SetCursorScreenPos(ImVec2(x, start.y));
		ImGui::PushID(i);
		if (ImGui::InvisibleButton("##tab", ImVec2(w, h)) && *selected != i)
		{
			*selected = i;
			changed = true;
		}
		const bool hovered = ImGui::IsItemHovered();
		ImGui::PopID();

		const bool active = (*selected == i);
		const ImVec2 a(x, start.y);
		const ImVec2 b(x + w, start.y + h);

		ImVec4 bg = active ? s.PanelBg : Lighten(s.PanelBg, -0.03f);
		if (!active && hovered)
			bg = Lighten(s.PanelBg, 0.03f);

		dl->AddRectFilled(a, b, MMUI_Col(bg));
		dl->AddLine(a, ImVec2(b.x, a.y), MMUI_Col(s.BorderBright));		   // top
		dl->AddLine(a, ImVec2(a.x, b.y), MMUI_Col(s.BorderBright));		   // left
		dl->AddLine(ImVec2(b.x, a.y), ImVec2(b.x, b.y), MMUI_Col(s.BorderDark)); // right

		if (!active) // bottom line closes inactive tabs (active one merges with the panel)
			dl->AddLine(ImVec2(a.x, b.y - 1.0f), ImVec2(b.x, b.y - 1.0f), MMUI_Col(s.BorderBright));

		dl->AddText(ImVec2(a.x + padX, a.y + (h - ts.y) * 0.5f), MMUI_Col(active ? s.Accent : s.Text), label);

		x += w;
	}

	ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + h));
	ImGui::Dummy(ImVec2(0.0f, 0.0f));
	ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + h));

	ImGui::PopID();
	return changed;
}

void MMUI_DrawPanelFrame(const ImVec2& min, const ImVec2& max)
{
	ImGui::GetWindowDrawList()->AddRect(min, max, MMUI_Col(MMUI_GetScheme().BorderBright));
}

bool MMUI_Checkbox(const char* label, bool* value)
{
	const MMUIScheme& s = MMUI_GetScheme();

	ImGui::PushStyleColor(ImGuiCol_Text, *value ? s.Accent : s.Text);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, s.CheckBg);
	ImGui::PushStyleColor(ImGuiCol_Border, s.BorderBright);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
	const bool changed = ImGui::Checkbox(label, value);
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);
	return changed;
}

bool MMUI_Button(const char* label, const ImVec2& size)
{
	const MMUIScheme& s = MMUI_GetScheme();

	ImGui::PushStyleColor(ImGuiCol_Border, s.BorderBright);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
	const bool pressed = ImGui::Button(label, size);
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();
	return pressed;
}
