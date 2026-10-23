// imgui_hudpanels.cpp
// Go-Mod: Reborn - HUD panels done again with Dear ImGui (the VGUI ones are gone):
//
//  * Spectator panel (was vgui_SpectatorPanel.cpp): top bar (map / spectators), bottom bar with
//    the name of the target or the mode, and with the menu open (duck or "spec_menu 1"):
//    Options, < previous, player list, next >, camera mode. Inset window border (click = change).
//
//  * CTF team menu (was vgui_teammenu.cpp) and character menu (was vgui_ClassMenu.cpp):
//    same commands as always (jointeam N, spectate, cancelmenu, selectchar N), number keys,
//    map briefing (maps/<map>.txt), character pictures (gfx/vgui/640_<name>.tga) and
//    descriptions (classes/short_<name>.txt).

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "pm_shared.h"
#include "cl_entity.h"
#include "Exports.h"

#include "imgui.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

extern int g_iUser1;
extern int g_iUser2;
extern int g_iTeamNumber;
extern int g_iPlayerClass;
extern bool g_bShowMenu;

extern float* GetClientColor(int clientIndex);
extern void COM_FileBase(const char* in, char* out);
extern void ImGuiCursor_Update(); // hud_imgui.cpp

// imgui_gomod_menu.cpp
extern unsigned int GoModUI_LoadImage(const char* path, int* w, int* h);

// vgui_TeamFortressViewport.cpp (until it is gone too)
extern int GoModAdmin_NumberOfTeams();
extern const char* GoModAdmin_TeamName(int team);
extern bool GoModAdmin_AllowSpectators();
extern int GoModCTF_MenuTeamNumber();
extern void GoModCTF_SetStatsPlayerImage(const char* image);
extern char* sLocalisedClasses[][7];
extern char* sCTFClassSelection[][7];

#define CTF_MENU_TEAM 2  // MENU_TEAM
#define CTF_MENU_CLASS 3 // MENU_CLASS
#define CTF_TEAMS 2		 // PC_MAX_TEAMS
#define CTF_CHARACTERS 7 // PC_RANDOM (the last one is "random")

// text of the game (titles.txt), or the fallback if the key is not there
static std::string Localise(const char* key, const char* fallback = nullptr)
{
	const char* text = CHudTextMessage::BufferedLocaliseTextString(key);
	if (!text || !text[0] || (text[0] == '#' && fallback))
		return fallback ? fallback : (key ? key : "");
	return text;
}

static void SpecCenterPrint(const char* key)
{
	char string[256];
	snprintf(string, sizeof(string), "%c%s", HUD_PRINTCENTER, CHudTextMessage::BufferedLocaliseTextString(key));
	gHUD.m_TextMessage.MsgFunc_TextMsg(NULL, strlen(string) + 1, string);
}

static std::string CleanText(const char* in)
{
	std::string out;
	for (const char* p = in; p && *p; p++)
	{
		if (*p != '\r')
			out.push_back(*p);
	}
	return out;
}

// =====================================================================
//  Spectator panel
// =====================================================================

static bool g_bSpecVisible = false; // spectating: the bars are drawn
static bool g_bSpecMenu = false;	// the buttons are shown (and the mouse is free)
static bool g_bSpecInset = false;	// picture in picture window
static float g_flSpecNextCheck = 0.0f;

static const char* const kSpecModeNames[] = {
	"Spectator",
	"Chase Locked",
	"Chase Free",
	"Free Look",
	"First Person",
	"Free Map Overview",
	"Chase Map Overview",
};

static std::string SpecModeName(int mode)
{
	if (mode < 0 || mode > 6)
		mode = 0;

	char key[32];
	snprintf(key, sizeof(key), "#Spec_Mode%d", mode);
	return Localise(key, kSpecModeNames[mode]);
}

// height of the top / bottom bars
static float SpecBarHeight()
{
	return (float)(int)(std::max)(48.0f, ScreenHeight * 0.065f);
}

bool GoModSpec_IsVisible()
{
	return g_bSpecVisible;
}

bool GoModSpec_MenuOpen()
{
	return g_bSpecVisible && g_bSpecMenu;
}

void GoModSpec_ShowMenu(bool show)
{
	// closing the menu: say how to open it again
	if (!show && g_bSpecMenu && g_bSpecVisible)
		SpecCenterPrint("#Spec_Duck");

	g_bSpecMenu = show;
	ImGuiCursor_Update();
}

void GoModSpec_ToggleMenu()
{
	GoModSpec_ShowMenu(!g_bSpecMenu);
}

void GoModSpec_SetInset(bool enabled)
{
	g_bSpecInset = enabled;
}

// was TeamFortressViewport::UpdateSpectatorPanel: called every frame and when something changes
void GoModSpec_Update()
{
	const bool spectating = 0 != g_iUser1 && 0 != gHUD.m_pCvarDraw->value && !gHUD.m_iIntermission;

	if (!spectating)
	{
		if (g_bSpecVisible)
		{
			g_bSpecVisible = false;
			g_bSpecMenu = false;
			ImGuiCursor_Update();
		}
		return;
	}

	if (!g_bSpecVisible)
	{
		g_bSpecVisible = true;
		g_bSpecMenu = false;
		SpecCenterPrint("#Spec_Duck");
		ImGuiCursor_Update();
	}

	// check if spectator combinations are still valid (once per second, like before)
	if (g_flSpecNextCheck <= gHUD.m_flTime || g_flSpecNextCheck > gHUD.m_flTime + 2.0f)
	{
		gHUD.m_Spectator.CheckSettings();
		g_flSpecNextCheck = gHUD.m_flTime + 1.0f;
	}
}

// top bar height for the chat input
float GoModSpec_TopHeight()
{
	return SpecBarHeight();
}

// target of the camera (0 = none)
static int SpecTarget()
{
	if (g_iUser2 > 0 && g_iUser2 <= gEngfuncs.GetMaxClients() && g_iUser1 != OBS_ROAMING)
		return g_iUser2;
	return 0;
}

static std::string SpecBottomText(ImU32& color)
{
	color = IM_COL32(255, 255, 255, 255);

	const int player = SpecTarget();

	// special case in free map and inset off, don't show names
	const char* name = nullptr;
	if (!((g_iUser1 == OBS_MAP_FREE) && 0 == gHUD.m_Spectator.m_pip->value))
		name = g_PlayerInfoList[player].name;

	if (player && name && name[0])
	{
		// in first person mode colorize player names
		if (g_iUser1 == OBS_IN_EYE)
		{
			if (float* c = GetClientColor(player))
				color = IM_COL32((int)(c[0] * 255), (int)(c[1] * 255), (int)(c[2] * 255), 255);
		}
		return name;
	}

	return SpecModeName(g_iUser1);
}

static void SpecPopups(float barTop)
{
	// ---- options ----
	ImGui::SetNextWindowPos(ImVec2(ImGui::GetStyle().WindowPadding.x, barTop), ImGuiCond_Appearing, ImVec2(0.0f, 1.0f));
	if (ImGui::BeginPopup("##specoptions"))
	{
		if (ImGui::MenuItem(Localise("#Menu_Close", "Close Menu").c_str()))
			GoModSpec_ShowMenu(false);

		if (ImGui::MenuItem(Localise("#Spec_Help_Title", "Help").c_str()))
			gEngfuncs.pfnClientCmd("spec_help");

		ImGui::Separator();

		auto Toggle = [](const char* label, cvar_t* cvar)
		{
			if (!cvar)
				return;
			bool on = cvar->value != 0;
			if (ImGui::MenuItem(label, nullptr, on))
				gEngfuncs.Cvar_SetValue(cvar->name, on ? 0.0f : 1.0f);
		};

		Toggle(Localise("#Chat_Messages", "Chat Messages").c_str(), gEngfuncs.pfnGetCvarPointer("hud_saytext"));
		Toggle(Localise("#Show_Status", "Show Status").c_str(), gHUD.m_Spectator.m_drawstatus);
		Toggle(Localise("#View_Cone", "View Cone").c_str(), gHUD.m_Spectator.m_drawcone);
		Toggle(Localise("#Player_Names", "Player Names").c_str(), gHUD.m_Spectator.m_drawnames);
		Toggle(Localise("#Auto_Directed", "Auto Director").c_str(), gHUD.m_Spectator.m_autoDirector);

		if (ImGui::MenuItem(Localise("#PIP", "Picture in Picture").c_str(), nullptr, g_bSpecInset))
			gHUD.m_Spectator.SetModes(-1, gHUD.m_Spectator.ToggleInset(true));

		ImGui::Separator();

		if (ImGui::MenuItem(Localise("#Show_Scores", "Show Scores").c_str(), nullptr, gHUD.m_Scoreboard.IsForced()))
			gHUD.m_Scoreboard.SetForced(!gHUD.m_Scoreboard.IsForced());

		ImGui::EndPopup();
	}

	// ---- camera ----
	ImGui::SetNextWindowPos(ImVec2(ScreenWidth - ImGui::GetStyle().WindowPadding.x, barTop), ImGuiCond_Appearing, ImVec2(1.0f, 1.0f));
	if (ImGui::BeginPopup("##speccamera"))
	{
		for (int mode = OBS_CHASE_LOCKED; mode <= OBS_MAP_CHASE; mode++)
		{
			if (ImGui::MenuItem(SpecModeName(mode).c_str(), nullptr, g_iUser1 == mode))
			{
				char cmd[32];
				snprintf(cmd, sizeof(cmd), "spec_mode %d", mode);
				gEngfuncs.pfnClientCmd(cmd);
			}
		}
		ImGui::EndPopup();
	}

	// ---- players ----
	ImGui::SetNextWindowPos(ImVec2(ScreenWidth * 0.5f, barTop), ImGuiCond_Appearing, ImVec2(0.5f, 1.0f));
	if (ImGui::BeginPopup("##specplayers"))
	{
		gHUD.m_Scoreboard.GetAllPlayersInfo();

		int count = 0;
		for (int i = 1; i < MAX_PLAYERS_HUD; i++)
		{
			cl_entity_t* ent = gEngfuncs.GetEntityByIndex(i);
			if (!gHUD.m_Spectator.IsActivePlayer(ent))
				continue;

			const char* name = g_PlayerInfoList[ent->index].name;
			if (!name || !name[0])
				continue;

			count++;
			ImGui::PushID(i);
			if (ImGui::MenuItem(name, nullptr, SpecTarget() == ent->index))
				gHUD.m_Spectator.FindPlayer(name);
			ImGui::PopID();
		}

		if (count == 0)
			ImGui::TextDisabled("%s", "No players");

		ImGui::EndPopup();
	}
}

void GoModSpec_Draw()
{
	if (!g_bSpecVisible)
		return;

	const float h = SpecBarHeight();
	const float fontSize = (std::max)(13.0f, ScreenHeight * 0.02f);

	ImDrawList* bg = ImGui::GetBackgroundDrawList();
	const ImU32 barColor = IM_COL32(0, 0, 0, 175);
	const ImU32 lineColor = IM_COL32(255, 255, 255, 40);

	// ---- inset window (picture in picture) ----
	float topLeft = 0.0f;
	if (g_bSpecInset)
	{
		const overviewInfo_t& ov = gHUD.m_Spectator.m_OverviewData;
		const float x = XRES(ov.insetWindowX - 1);
		const float y = YRES(ov.insetWindowY);
		const float w = XRES(ov.insetWindowWidth + 2);
		const float ih = YRES(ov.insetWindowHeight);

		bg->AddRect(ImVec2(x, y), ImVec2(x + w, y + ih), IM_COL32(255, 255, 255, 90), 0.0f, 0, 2.0f);
		topLeft = XRES(ov.insetWindowX + ov.insetWindowWidth + 2);

		// click on it = next inset mode
		if (g_bSpecMenu)
		{
			ImGui::SetNextWindowPos(ImVec2(x, y));
			ImGui::SetNextWindowSize(ImVec2(w, ih));
			ImGui::Begin("##specinset", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
													  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
			if (ImGui::InvisibleButton("##inset", ImVec2(w - ImGui::GetStyle().WindowPadding.x * 2, ih - ImGui::GetStyle().WindowPadding.y * 2)))
				gHUD.m_Spectator.SetModes(-1, gHUD.m_Spectator.ToggleInset(false));
			if (ImGui::IsItemHovered())
				bg->AddRect(ImVec2(x, y), ImVec2(x + w, y + ih), IM_COL32(255, 255, 255, 220), 0.0f, 0, 2.0f);
			ImGui::End();
		}
	}

	// ---- top bar: map name (spectators in HLTV) ----
	bg->AddRectFilled(ImVec2(topLeft, 0.0f), ImVec2((float)ScreenWidth, h), barColor);
	bg->AddLine(ImVec2(topLeft, h), ImVec2((float)ScreenWidth, h), lineColor);

	if (gHUD.m_Spectator.m_drawstatus && gHUD.m_Spectator.m_drawstatus->value != 0)
	{
		char text[128];
		if (0 != gEngfuncs.IsSpectateOnly())
		{
			snprintf(text, sizeof(text), "%s: %d", Localise("#Spectators", "Spectators").c_str(), gHUD.m_Spectator.m_iSpectatorNumber);
		}
		else
		{
			char map[64];
			COM_FileBase(gEngfuncs.pfnGetLevelName(), map);
			snprintf(text, sizeof(text), "%s: %s", Localise("#Spec_Map", "Map").c_str(), map);
		}

		ImFont* font = ImGui::GetFont();
		const ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
		bg->AddText(font, fontSize, ImVec2(ScreenWidth - ts.x - fontSize, (h - ts.y) * 0.5f), IM_COL32(255, 255, 255, 230), text);
	}

	// ---- bottom bar ----
	const float barTop = ScreenHeight - h;
	bg->AddRectFilled(ImVec2(0.0f, barTop), ImVec2((float)ScreenWidth, (float)ScreenHeight), barColor);
	bg->AddLine(ImVec2(0.0f, barTop), ImVec2((float)ScreenWidth, barTop), lineColor);

	ImU32 labelColor;
	std::string label = SpecBottomText(labelColor);

	if (0 != gHUD.m_Spectator.m_autoDirector->value)
		label = "[Auto] " + label;

	if (!g_bSpecMenu)
	{
		ImFont* font = ImGui::GetFont();
		const ImVec2 ts = font->CalcTextSizeA(fontSize * 1.15f, FLT_MAX, 0.0f, label.c_str());
		bg->AddText(font, fontSize * 1.15f, ImVec2((ScreenWidth - ts.x) * 0.5f, barTop + (h - ts.y) * 0.5f), labelColor, label.c_str());
		return;
	}

	// menu: [Options] [<] [ player ] [>] [Camera]
	ImGui::PushFont(nullptr, fontSize);

	ImGui::SetNextWindowPos(ImVec2(0.0f, barTop));
	ImGui::SetNextWindowSize(ImVec2((float)ScreenWidth, h));
	ImGui::Begin("##specbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
										   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);

	const float pad = fontSize;
	const float btnH = (float)(int)(h * 0.6f);
	const float y = (h - btnH) * 0.5f;
	const float optionsW = (float)(int)(ScreenWidth * 0.15f);
	const float cameraW = (float)(int)(ScreenWidth * 0.20f);
	const float arrowW = btnH * 1.4f;
	const float gap = pad * 0.5f;

	ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 120));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 60));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 255, 255, 100));
	ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 255, 255, 70));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

	float x = pad;
	ImGui::SetCursorPos(ImVec2(x, y));
	if (ImGui::Button(Localise("#SPECT_OPTIONS", "Options").c_str(), ImVec2(optionsW, btnH)))
		ImGui::OpenPopup("##specoptions");
	x += optionsW + gap;

	ImGui::SetCursorPos(ImVec2(x, y));
	if (ImGui::Button("<##prev", ImVec2(arrowW, btnH)))
		gHUD.m_Spectator.FindNextPlayer(true);
	x += arrowW + gap;

	const float rightX = ScreenWidth - pad - cameraW;
	const float nextX = rightX - gap - arrowW;
	const float mainW = nextX - gap - x;

	ImGui::SetCursorPos(ImVec2(x, y));
	ImGui::PushStyleColor(ImGuiCol_Text, labelColor);
	if (ImGui::Button((label + "##specmain").c_str(), ImVec2(mainW, btnH)))
		ImGui::OpenPopup("##specplayers");
	ImGui::PopStyleColor();

	ImGui::SetCursorPos(ImVec2(nextX, y));
	if (ImGui::Button(">##next", ImVec2(arrowW, btnH)))
		gHUD.m_Spectator.FindNextPlayer(false);

	ImGui::SetCursorPos(ImVec2(rightX, y));
	if (ImGui::Button((SpecModeName(g_iUser1) + "##speccam").c_str(), ImVec2(cameraW, btnH)))
		ImGui::OpenPopup("##speccamera");

	ImGui::PopStyleVar();
	ImGui::PopStyleColor(4);

	SpecPopups(barTop);

	ImGui::End();
	ImGui::PopFont();
}

/*
==========================
HUD_ChatInputPosition

Sets the location of the input for chat text (below the top bar of the spectator)
==========================
*/
void DLLEXPORT HUD_ChatInputPosition(int* x, int* y)
{
	if (g_iUser1 != 0 || 0 != gEngfuncs.IsSpectateOnly())
	{
		if (gHUD.m_Spectator.m_pip->value == INSET_OFF)
			*y = (int)SpecBarHeight();
		else
			*y = YRES(gHUD.m_Spectator.m_OverviewData.insetWindowHeight + 5);
	}
}

// =====================================================================
//  CTF team / character menus
// =====================================================================

static int g_iCTFMenu = 0;			  // 0 none, CTF_MENU_TEAM, CTF_MENU_CLASS
static int g_iCTFHover = 0;			  // button with the info on the right
static float g_flTeamFullUntil = 0.0f; // "the team is full" message
static std::string g_CTFBriefingLevel;
static std::string g_CTFBriefing;

struct CharacterInfo
{
	bool loaded = false;
	unsigned int tex = 0;
	int w = 0, h = 0;
	std::string description;
};

static CharacterInfo g_Characters[CTF_TEAMS][CTF_CHARACTERS];

bool GoModCTF_IsOpen()
{
	return g_iCTFMenu != 0;
}

void GoModCTF_Close()
{
	if (!g_iCTFMenu)
		return;

	g_iCTFMenu = 0;
	ImGuiCursor_Update();
}

void GoModCTF_Open(int menu)
{
	if (menu != CTF_MENU_TEAM && menu != CTF_MENU_CLASS)
		return;

	// the character menu needs a team
	if (menu == CTF_MENU_CLASS && GoModCTF_MenuTeamNumber() == 0)
		return;

	g_iCTFMenu = menu;
	g_iCTFHover = 0;
	ImGuiCursor_Update();
}

// message TeamFull: byte full
void GoModCTF_TeamFull(bool full)
{
	g_flTeamFullUntil = full ? gHUD.m_flTime + 2.0f : 0.0f;
}

void GoModCTF_Reset()
{
	g_iCTFMenu = 0;
	g_iCTFHover = 0;
	g_flTeamFullUntil = 0.0f;
	g_CTFBriefingLevel.clear();
}

// the command closes the menu, like the VGUI buttons ("close" handler)
static void CTFCommand(const char* cmd)
{
	GoModCTF_Close();
	gEngfuncs.pfnClientCmd(cmd);
}

static void JoinTeam(int team)
{
	char cmd[32];
	snprintf(cmd, sizeof(cmd), "jointeam %d", team);
	CTFCommand(cmd);

	// try to guess the player's new team (it'll be corrected if it's wrong)
	if (team >= 1 && team <= 4)
		g_iTeamNumber = team;
}

static void SelectCharacter(int index) // 0 based
{
	char cmd[32];
	snprintf(cmd, sizeof(cmd), "selectchar %d", index + 1);
	CTFCommand(cmd);

	// hud_classautokill: kill the player so the new character is used now
	if (CVAR_GET_FLOAT("hud_classautokill") != 0 && g_iPlayerClass != 0)
		gEngfuncs.pfnClientCmd("kill");
}

static int TeamCount()
{
	return (std::min)(2, GoModAdmin_NumberOfTeams());
}

// keys 1-9 (ESC is in hud_imgui.cpp)
bool GoModCTF_SlotInput(int slot)
{
	if (g_iCTFMenu == CTF_MENU_TEAM)
	{
		if (slot >= 1 && slot <= TeamCount())
		{
			JoinTeam(slot);
			return true;
		}
		if (slot == 3)
		{
			JoinTeam(3); // auto assign
			return true;
		}
		if (slot == 4 && GoModAdmin_AllowSpectators())
		{
			CTFCommand("spectate");
			return true;
		}
		return false;
	}

	if (g_iCTFMenu == CTF_MENU_CLASS)
	{
		if (slot >= 1 && slot <= CTF_CHARACTERS)
		{
			SelectCharacter(slot - 1);
			return true;
		}
	}

	return false;
}

// ESC: only if the player already has a team / character
bool GoModCTF_CanCancel()
{
	if (g_iCTFMenu == CTF_MENU_TEAM)
		return 0 != g_iTeamNumber;
	if (g_iCTFMenu == CTF_MENU_CLASS)
		return 0 != g_iPlayerClass;
	return false;
}

static const std::string& MapBriefing(std::string& mapName)
{
	const char* level = gEngfuncs.pfnGetLevelName();
	const std::string current = level ? level : "";

	std::string base = current;
	const size_t dot = base.find_last_of('.');
	if (dot != std::string::npos)
		base = base.substr(0, dot);

	const size_t slash = base.find_last_of("/\\");
	mapName = slash != std::string::npos ? base.substr(slash + 1) : base;

	if (current != g_CTFBriefingLevel)
	{
		g_CTFBriefingLevel = current;
		g_CTFBriefing = "Map Description not available.";

		if (!base.empty())
		{
			const std::string path = base + ".txt";
			if (char* file = (char*)gEngfuncs.COM_LoadFile((char*)path.c_str(), 5, NULL))
			{
				g_CTFBriefing = CleanText(file);
				gEngfuncs.COM_FreeFile(file);
			}
		}
	}

	return g_CTFBriefing;
}

static CharacterInfo& Character(int team, int index)
{
	CharacterInfo& info = g_Characters[team][index];
	if (info.loaded)
		return info;

	info.loaded = true;

	const char* name = sCTFClassSelection[team][index];

	char path[256];
	snprintf(path, sizeof(path), "gfx/vgui/%d_%s.tga", ScreenWidth < 640 ? 320 : 640, name);
	info.tex = GoModUI_LoadImage(path, &info.w, &info.h);

	snprintf(path, sizeof(path), "classes/short_%s.txt", name);
	if (char* file = (char*)gEngfuncs.COM_LoadFile(path, 5, NULL))
	{
		info.description = CleanText(file);
		gEngfuncs.COM_FreeFile(file);
	}
	else
	{
		info.description = "Class Description not available.";
	}

	return info;
}

// menu button "  1  Text"
static bool MenuButton(int key, const char* text, float width, float height, bool active)
{
	char label[256];
	if (key > 0)
		snprintf(label, sizeof(label), "  %d   %s###ctfbtn%d", key, text, key);
	else
		snprintf(label, sizeof(label), "  %s###ctfbtn_%s", text, text);

	if (active)
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));

	const bool pressed = ImGui::Button(label, ImVec2(width, height));

	if (active)
		ImGui::PopStyleColor();

	return pressed;
}

static void DrawTeamMenu(float fontSize)
{
	const int teams = TeamCount();
	const float btnW = (float)(int)(fontSize * 11.0f);
	const float btnH = (float)(int)(fontSize * 1.9f);

	if (g_iCTFHover == 0 && teams > 0)
		g_iCTFHover = 1;

	// ---- left: buttons ----
	ImGui::BeginChild("##teambuttons", ImVec2(btnW + ImGui::GetStyle().WindowPadding.x * 2.0f, 0), ImGuiChildFlags_None);

	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));

	for (int i = 1; i <= teams; i++)
	{
		if (MenuButton(i, GoModAdmin_TeamName(i), btnW, btnH, g_iCTFHover == i))
			JoinTeam(i);
		if (ImGui::IsItemHovered())
			g_iCTFHover = i;
	}

	if (MenuButton(3, Localise("#CTFTeam_AutoAssign", "Auto Assign").c_str(), btnW, btnH, g_iCTFHover == 3))
		JoinTeam(3);
	if (ImGui::IsItemHovered())
		g_iCTFHover = 3;

	if (GoModAdmin_AllowSpectators())
	{
		if (MenuButton(4, Localise("#CTFMenu_Spectate", "Spectate").c_str(), btnW, btnH, g_iCTFHover == 4))
			CTFCommand("spectate");
		if (ImGui::IsItemHovered())
			g_iCTFHover = 4;
	}

	if (0 != GoModCTF_MenuTeamNumber())
	{
		ImGui::Spacing();
		if (MenuButton(0, Localise("#CTFMenu_Cancel", "Cancel").c_str(), btnW, btnH, false))
			CTFCommand("cancelmenu");
	}

	ImGui::PopStyleVar();
	ImGui::EndChild();

	ImGui::SameLine();

	// ---- right: map briefing + team info ----
	ImGui::BeginChild("##teaminfo", ImVec2(0, 0), ImGuiChildFlags_Borders);

	std::string mapName;
	const std::string& briefing = MapBriefing(mapName);

	ImGui::PushFont(nullptr, fontSize * 1.4f);
	ImGui::TextUnformatted(mapName.c_str());
	ImGui::PopFont();
	ImGui::Separator();

	const float infoH = fontSize * 5.0f;
	ImGui::BeginChild("##briefing", ImVec2(0, -infoH), ImGuiChildFlags_None);
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextUnformatted(briefing.c_str());
	ImGui::PopTextWrapPos();
	ImGui::EndChild();

	ImGui::Separator();

	// players of the team under the mouse
	if (g_iCTFHover >= 1 && g_iCTFHover <= teams)
	{
		gHUD.m_Scoreboard.GetAllPlayersInfo();

		std::string players;
		int count = 0;
		for (int i = 1; i < MAX_PLAYERS_HUD; i++)
		{
			if (!g_PlayerInfoList[i].name || !g_PlayerInfoList[i].name[0])
				continue;
			if (g_PlayerExtraInfo[i].teamid != g_iCTFHover && stricmp(g_PlayerExtraInfo[i].teamname, GoModAdmin_TeamName(g_iCTFHover)))
				continue;

			if (count++)
				players += ", ";
			players += g_PlayerInfoList[i].name;
		}

		ImGui::Text("%s: %d %s", GoModAdmin_TeamName(g_iCTFHover), count, count == 1 ? "player" : "players");
		if (count)
		{
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextDisabled("%s", players.c_str());
			ImGui::PopTextWrapPos();
		}
	}

	ImGui::EndChild();
}

static void DrawCharacterMenu(float fontSize)
{
	const int team = GoModCTF_MenuTeamNumber() - 1;
	if (team < 0 || team >= CTF_TEAMS)
	{
		GoModCTF_Close();
		return;
	}

	const float btnW = (float)(int)(fontSize * 11.0f);
	const float btnH = (float)(int)(fontSize * 1.9f);

	if (g_iCTFHover < 1 || g_iCTFHover > CTF_CHARACTERS)
		g_iCTFHover = 1;

	// ---- left: characters ----
	ImGui::BeginChild("##charbuttons", ImVec2(btnW + ImGui::GetStyle().WindowPadding.x * 2.0f, 0), ImGuiChildFlags_None);
	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));

	for (int i = 0; i < CTF_CHARACTERS; i++)
	{
		const std::string name = Localise(sLocalisedClasses[team][i], sCTFClassSelection[team][i]);
		if (MenuButton(i + 1, name.c_str(), btnW, btnH, g_iCTFHover == i + 1))
			SelectCharacter(i);

		if (ImGui::IsItemHovered() && g_iCTFHover != i + 1)
		{
			g_iCTFHover = i + 1;
			GoModCTF_SetStatsPlayerImage(sCTFClassSelection[team][i]);
		}
	}

	ImGui::Spacing();
	if (MenuButton(0, Localise("#CTFMenu_Cancel", "Cancel").c_str(), btnW, btnH, false))
		CTFCommand("cancelmenu");

	ImGui::PopStyleVar();
	ImGui::EndChild();

	ImGui::SameLine();

	// ---- right: picture, name and description ----
	ImGui::BeginChild("##charinfo", ImVec2(0, 0), ImGuiChildFlags_Borders);

	const int index = g_iCTFHover - 1;
	CharacterInfo& info = Character(team, index);

	char key[64];
	snprintf(key, sizeof(key), "#CTFTitle_%s", sCTFClassSelection[team][index]);
	const std::string title = Localise(key, sCTFClassSelection[team][index]);

	if (info.tex && info.w > 0 && info.h > 0)
	{
		const float imgH = (std::min)(ImGui::GetContentRegionAvail().y * 0.45f, (float)info.h * 2.0f);
		const float imgW = imgH * info.w / info.h;
		ImGui::Image((ImTextureID)(intptr_t)info.tex, ImVec2(imgW, imgH));
		ImGui::SameLine();
	}

	ImGui::BeginGroup();
	ImGui::PushFont(nullptr, fontSize * 1.4f);
	ImGui::TextUnformatted(title.c_str());
	ImGui::PopFont();
	ImGui::EndGroup();

	ImGui::Separator();

	ImGui::BeginChild("##chardesc", ImVec2(0, 0), ImGuiChildFlags_None);
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextUnformatted(info.description.c_str());
	ImGui::PopTextWrapPos();
	ImGui::EndChild();

	ImGui::EndChild();
}

void GoModCTF_Draw()
{
	if (!g_iCTFMenu)
		return;

	const float fontSize = (std::max)(13.0f, ScreenHeight * 0.019f);
	ImGui::PushFont(nullptr, fontSize);

	const float w = (std::max)(ScreenWidth * 0.62f, (std::min)(700.0f, ScreenWidth - 20.0f));
	const float h = (std::max)(ScreenHeight * 0.65f, (std::min)(420.0f, ScreenHeight - 20.0f));

	ImGui::SetNextWindowPos(ImVec2(ScreenWidth * 0.5f, ScreenHeight * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);

	const std::string title = g_iCTFMenu == CTF_MENU_TEAM ? Localise("#CTFTitle_SelectYourTeam", "Select Your Team")
														  : Localise("#CTFTitle_SelectYourCharacter", "Select Your Character");

	ImGui::Begin((title + "###gomod_ctfmenu").c_str(), nullptr,
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

	if (g_iCTFMenu == CTF_MENU_TEAM)
	{
		DrawTeamMenu(fontSize);
	}
	else
	{
		DrawCharacterMenu(fontSize);
	}

	ImGui::End();

	// "Team balancing enabled. Selected team is full."
	if (g_flTeamFullUntil > gHUD.m_flTime)
	{
		const char* text = "Team balancing enabled.  Selected team is full.";
		ImFont* font = ImGui::GetFont();
		const ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
		const ImVec2 p((ScreenWidth - ts.x) * 0.5f, (ScreenHeight - h) * 0.5f - ts.y * 2.0f);
		ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(p.x - 8, p.y - 4), ImVec2(p.x + ts.x + 8, p.y + ts.y + 4), IM_COL32(0, 0, 0, 170), 4.0f);
		ImGui::GetForegroundDrawList()->AddText(font, fontSize, p, IM_COL32(255, 80, 80, 255), text);
	}

	ImGui::PopFont();
}
