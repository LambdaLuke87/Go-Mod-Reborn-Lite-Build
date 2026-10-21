// mmui.h
// Go-Mod: Reborn - Main Menu UI (Dear ImGui drawn in the main menu and in the pause menu)
//
// Everything here runs from the SDL_GL_SwapWindow hook (see mmui_swaphook.cpp), with its own
// ImGui context, so it never collides with the HUD spawn menu (hud_imgui.cpp).
//
// Menus ("panels") register themselves with MMUI_RegisterPanel(). The first one is the
// Create Server dialog (mmui_createserver.cpp).
#pragma once

#include "imgui.h"

#include <string>
#include <vector>

// =====================================================================
//  Lifetime (called from the client)
// =====================================================================

void MMUI_Init();	  // HUD_Init  (registers commands / cvars, nothing else)
void MMUI_Shutdown(); // HUD_Shutdown (removes the hook! must run before client.dll unloads)

// installs the swap hook on first use. false = the UI can't be drawn on this engine
bool MMUI_EnsureHook();

// =====================================================================
//  Game state (same idea as: inMainMenu = world == nullptr; isPaused = g_refparams.paused)
// =====================================================================

bool MMUI_InMainMenu();
bool MMUI_IsPaused();

// =====================================================================
//  Panels
// =====================================================================

struct MMUIPanel
{
	const char* name;
	bool (*IsOpen)();
	void (*Draw)();		  // called inside the ImGui frame
	void (*ForceClose)(); // the player went back to the game, etc.
};

void MMUI_RegisterPanel(const MMUIPanel* panel);

// =====================================================================
//  Localization: "#Key" -> text from resource/gameui_english.txt (+ valve_english.txt, ...)
// =====================================================================

const char* MMUI_Localize(const char* textOrKey);
void MMUI_ReloadLocalization();

// =====================================================================
//  Scheme: colors / font from resource/TrackerScheme.res
// =====================================================================

struct MMUIScheme
{
	ImVec4 Text;		  // BaseSettings FgColor
	ImVec4 BrightText;	  // Colors BrightBaseText
	ImVec4 Accent;		  // Colors BrightControlText (selected tab, "Map" label...)
	ImVec4 DisabledText;  // BaseSettings DisabledFgColor1
	ImVec4 PanelBg;		  // BaseSettings BgColor
	ImVec4 ControlDarkBg; // Colors ControlDarkBG
	ImVec4 EditBg;		  // BaseSettings WindowBgColor (text entries, combos)
	ImVec4 ListBg;		  // BaseSettings ListBgColor
	ImVec4 SelectionBg;	  // BaseSettings SelectionBgColor
	ImVec4 SelectionText; // BaseSettings SelectionFgColor
	ImVec4 BorderBright;
	ImVec4 BorderDark;
	ImVec4 CheckMark; // BaseSettings CheckButtonCheck
	ImVec4 CheckBg;	  // BaseSettings CheckBgColor
	ImVec4 ScrollFg;  // ScrollBarSlider ScrollBarSliderFgColor
	ImVec4 ScrollBg;  // ScrollBarSlider ScrollBarSliderBgColor
	ImVec4 MenuBg;	  // Menu BgColor (combo popups)
	ImVec4 MenuFg;
	ImVec4 MenuArmedBg;
	ImVec4 MenuArmedFg;
	ImVec4 ArrowColor; // MenuButton ButtonArrowColor
	ImVec4 TitleText;  // BaseSettings TitleBarFgColor
	ImVec4 TitleButtonFg;
	ImVec4 TitleButtonBg;

	std::string fontName = "Tahoma";
	int fontTall = 16;
};

const MMUIScheme& MMUI_GetScheme();
void MMUI_ReloadScheme();

// layout scale: 1.0 = the reference design (20px font)
float MMUI_Scale();
inline ImU32 MMUI_Col(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

// =====================================================================
//  Files / text helpers
// =====================================================================

// loads a text file through the engine filesystem, UTF-16LE / UTF-8 BOM are converted to UTF-8
bool MMUI_LoadTextFile(const char* path, std::string& out);

struct MMUIToken
{
	std::string text;
	bool quoted = false;
};

// KeyValues-style tokenizer: "quoted strings", bare words, { }, // comments
void MMUI_Tokenize(const std::string& src, std::vector<MMUIToken>& out);

// =====================================================================
//  Textures (PNG)
// =====================================================================

// returns 0 on failure. w/h optional
unsigned int MMUI_LoadPNG(const char* path, int* w = nullptr, int* h = nullptr);
void MMUI_FreeTexture(unsigned int tex);

// =====================================================================
//  Valve-looking widgets
// =====================================================================

// Draws the dialog frame + title bar (icon, title, X). Call MMUI_EndDialog() ALWAYS after it.
// Returns false when the X was pressed (and the dialog should close).
bool MMUI_BeginDialog(const char* id, const char* title, const ImVec2& size);
void MMUI_EndDialog();

// Valve PropertySheet-like tab strip. Returns true when the selection changed.
bool MMUI_TabStrip(const char* id, const char* const* labels, int count, int* selected, float minTabWidth);

// rectangle outline in the scheme border color (panels below tab strips)
void MMUI_DrawPanelFrame(const ImVec2& min, const ImVec2& max);

// checkbox whose label turns Accent when checked (like the Valve dialog)
bool MMUI_Checkbox(const char* label, bool* value);

// button with the Valve raised border
bool MMUI_Button(const char* label, const ImVec2& size);
