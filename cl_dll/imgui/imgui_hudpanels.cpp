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
#include "demo_api.h"

#include "imgui.h"
#include "imgui_commandmenu.h"

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

// gomod_viewport.cpp
extern int GoModAdmin_NumberOfTeams();
extern const char* GoModAdmin_TeamName(int team);
extern bool GoModAdmin_AllowSpectators();
extern int GoModCTF_MenuTeamNumber();
extern char* sLocalisedClasses[][7];
extern char* sCTFClassSelection[][7];
extern const char* sLocalisedStatsTeams[]; // 4: both teams, Black Mesa, Opposing Force, individual
extern const char* sCTFStatsSelection[];

void GoModStats_SetPlayerImage(const char* image); // below

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
			GoModStats_SetPlayerImage(sCTFClassSelection[team][i]);
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

// =====================================================================
//  Command menu (+commandmenu, commandmenu.txt), was CCommandMenu of VGUI.
//  Same look as the spectator panel (transparent). Number keys / mouse, ESC closes.
// =====================================================================

static GMCommandMenu g_CommandMenu("commandmenu.txt");
static bool g_bCmdMenuOpen = false;
static float g_flCmdMenuOpenTime = 0.0f;

bool GoModCmdMenu_IsOpen()
{
	return g_bCmdMenuOpen;
}

// the command menu doesn't have cursor if hud_capturemouse is turned off
bool GoModCmdMenu_WantsCursor()
{
	return g_bCmdMenuOpen && gHUD.m_pCvarStealMouse && gHUD.m_pCvarStealMouse->value != 0.0f;
}

void GoModCmdMenu_Hide()
{
	if (!g_bCmdMenuOpen)
		return;

	g_bCmdMenuOpen = false;
	g_CommandMenu.ResetChain();
	ImGuiCursor_Update();
}

// +commandmenu
void GoModCmdMenu_Show()
{
	// is the command menu open? close it
	if (g_bCmdMenuOpen)
	{
		GoModCmdMenu_Hide();
		return;
	}

	// not visible while in intermission / with another menu
	if (gHUD.m_iIntermission || g_bShowMenu || GoModCTF_IsOpen())
		return;

	g_CommandMenu.ResetChain();
	g_bCmdMenuOpen = true;
	g_flCmdMenuOpenTime = gHUD.m_flTime;
	ImGuiCursor_Update();

	// command menu parameters ("+commandmenu 1 2": keys pressed right away;
	// the last argument is the key that the engine adds to the +commands)
	for (int i = 2; i < gEngfuncs.Cmd_Argc(); i++)
	{
		const char* param = gEngfuncs.Cmd_Argv(i - 1);
		if (param && g_CommandMenu.KeyInput(param[0]))
		{
			GoModCmdMenu_Hide();
			break;
		}
	}
}

// -commandmenu: if they've just tapped the command menu key, leave it open
void GoModCmdMenu_Release()
{
	if (g_flCmdMenuOpenTime + 0.3f > gHUD.m_flTime)
		return;

	GoModCmdMenu_Hide();
}

// number key with the menu open (true = used)
bool GoModCmdMenu_KeyInput(int key)
{
	if (!g_bCmdMenuOpen)
		return false;

	if (g_CommandMenu.KeyInput(key))
		GoModCmdMenu_Hide();
	return true;
}

void GoModCmdMenu_Reload()
{
	g_CommandMenu.Unload();
}

void GoModCmdMenu_Draw()
{
	if (!g_bCmdMenuOpen)
		return;

	if (g_CommandMenu.IsEmpty())
	{
		GoModCmdMenu_Hide();
		return;
	}

	const float fontSize = (std::max)(13.0f, ScreenHeight * 0.02f);
	ImGui::PushFont(nullptr, fontSize);

	// same place as the VGUI one: left side, 4 buttons from the top
	ImGui::SetNextWindowPos(ImVec2(0.0f, (float)(int)(YRES(30) * 4)));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
							 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
	if (!GoModCmdMenu_WantsCursor())
		flags |= ImGuiWindowFlags_NoInputs;

	// spectator panel look
	ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 160));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 60));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 255, 255, 100));
	ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 255, 255, 70));
	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

	ImGui::Begin("##gomod_commandmenu", nullptr, flags);

	if (g_CommandMenu.Draw((float)(int)XRES(160), (float)(int)YRES(20)))
		GoModCmdMenu_Hide();

	ImGui::End();

	ImGui::PopStyleVar();
	ImGui::PopStyleColor(5);
	ImGui::PopFont();
}

// =====================================================================
//  CTF end of game stats (was vgui_StatsMenuPanel.cpp)
//  messages StatsInfo / StatsPlayer, texts stats/stats_<view>.ost (printf formats),
//  pictures gfx/vgui/640_<name>.tga
// =====================================================================

#define STATS_VIEWS 4 // StatsTeamsCount

struct team_stat_info_t
{
	byte bChunksRead;
	char szMVP[32];
	char szMostKills[32];
	char szMostCTF[32];
	char szMostOff[32];
	char szMostDef[32];
	char szMostSnipe[32];
	char szMostBarnacle[32];
	char szMostDeaths[32];
	char szMostSuicides[32];
	char szMostDamage[32];
	char szMostAccel[32];
	char szMostBackpack[32];
	char szMostHealth[32];
	char szMostShield[32];
	char szMostJump[32];
	int iMVPVal;
	int iKillsVal;
	int iCTFVal;
	int iOffVal;
	int iDefVal;
	int iSnipeVal;
	int iBarnacleVal;
	int iDeathsVal;
	int iSuicidesVal;
	int iDamageVal;
	int iAccelVal;
	int iBackpackVal;
	int iHealthVal;
	int iShieldVal;
	int iJumpVal;
};

static bool g_bStatsOpen = false;
static int g_iStatsView = 0;
static int g_iWinningTeam = 0;
static team_stat_info_t g_TeamStatInfo[3];
static char g_szStatsText[STATS_VIEWS][1024];
static std::string g_StatsImage[STATS_VIEWS]; // picture of every view

struct StatsPicture
{
	unsigned int tex = 0;
	int w = 0, h = 0;
};

static std::unordered_map<std::string, StatsPicture> g_StatsPictures;

static const StatsPicture& StatsGetPicture(const std::string& name)
{
	auto it = g_StatsPictures.find(name);
	if (it != g_StatsPictures.end())
		return it->second;

	StatsPicture pic;
	char path[256];
	snprintf(path, sizeof(path), "gfx/vgui/%d_%s.tga", ScreenWidth < 640 ? 320 : 640, name.c_str());
	pic.tex = GoModUI_LoadImage(path, &pic.w, &pic.h);
	return g_StatsPictures[name] = pic;
}

static void StatsDefaults()
{
	for (int i = 0; i < STATS_VIEWS; i++)
	{
		strncpy(g_szStatsText[i], i == 3 ? "Spectator" : "No players on this team", sizeof(g_szStatsText[i]) - 1);
		g_szStatsText[i][sizeof(g_szStatsText[i]) - 1] = '\0';
		g_StatsImage[i] = sCTFStatsSelection[i == 3 ? 1 : i];
	}
}

bool GoModStats_IsOpen()
{
	return g_bStatsOpen;
}

void GoModStats_Open()
{
	if (0 != gEngfuncs.pDemoAPI->IsPlayingback())
		return;

	if (g_StatsImage[0].empty())
		StatsDefaults();

	g_bStatsOpen = true;
	g_iStatsView = 0;
	gHUD.m_Scoreboard.SetForced(false);
	ImGuiCursor_Update();
}

void GoModStats_Close()
{
	if (!g_bStatsOpen)
		return;

	g_bStatsOpen = false;
	ImGuiCursor_Update();
}

void GoModStats_Reset()
{
	GoModStats_Close();
	memset(g_TeamStatInfo, 0, sizeof(g_TeamStatInfo));
	g_iWinningTeam = 0;
	StatsDefaults();
}

// the character chosen in the CTF character menu (view "individual")
void GoModStats_SetPlayerImage(const char* image)
{
	if (image && image[0])
		g_StatsImage[3] = image;
}

// number keys: the views
bool GoModStats_SlotInput(int slot)
{
	if (!g_bStatsOpen)
		return false;

	if (slot >= 1 && slot <= STATS_VIEWS)
		g_iStatsView = slot - 1;
	return true;
}

// seconds as minutes and seconds (like the original: value / 60, value % 60)
#define STATS_MIN(v) ((v) / 60)
#define STATS_SEC(v) ((v) - ((v) / 60) * 60)

int GoModStats_MsgStatsInfo(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	const int teamNum = READ_BYTE();
	g_iWinningTeam = READ_BYTE();
	const int iNumPlayers = READ_BYTE();
	const int chunkId = READ_BYTE();

	if (teamNum < 0 || teamNum > 2)
		return 1;

	if (g_StatsImage[0].empty())
		StatsDefaults();

	if (0 == teamNum && g_iWinningTeam >= 0 && g_iWinningTeam < STATS_VIEWS)
		g_StatsImage[0] = sCTFStatsSelection[g_iWinningTeam];

	team_stat_info_t& info = g_TeamStatInfo[teamNum];

	auto ReadName = [](char* dst)
	{
		strncpy(dst, READ_STRING(), 31);
		dst[31] = '\0';
	};

	switch (chunkId)
	{
	case 0:
		ReadName(info.szMVP);
		info.iMVPVal = READ_SHORT();
		ReadName(info.szMostKills);
		info.iKillsVal = READ_SHORT();
		ReadName(info.szMostCTF);
		info.iCTFVal = READ_SHORT();
		ReadName(info.szMostOff);
		info.iOffVal = READ_SHORT();
		ReadName(info.szMostDef);
		info.iDefVal = READ_SHORT();
		ReadName(info.szMostSnipe);
		info.iSnipeVal = READ_SHORT();
		ReadName(info.szMostBarnacle);
		info.iBarnacleVal = READ_SHORT();
		break;

	case 1:
		ReadName(info.szMostDeaths);
		info.iDeathsVal = READ_SHORT();
		ReadName(info.szMostSuicides);
		info.iSuicidesVal = READ_SHORT();
		ReadName(info.szMostDamage);
		info.iDamageVal = READ_SHORT();
		ReadName(info.szMostAccel);
		info.iAccelVal = READ_SHORT();
		ReadName(info.szMostBackpack);
		info.iBackpackVal = READ_SHORT();
		ReadName(info.szMostHealth);
		info.iHealthVal = READ_SHORT();
		ReadName(info.szMostShield);
		info.iShieldVal = READ_SHORT();
		ReadName(info.szMostJump);
		info.iJumpVal = READ_SHORT();
		break;
	}

	info.bChunksRead |= (1 << chunkId);

	if ((info.bChunksRead & 3) != 0)
	{
		char path[64];
		snprintf(path, sizeof(path), "stats/stats_%s.ost", sCTFStatsSelection[teamNum]);

		if (char* format = (char*)gEngfuncs.COM_LoadFile(path, 5, nullptr))
		{
			if (iNumPlayers <= 0)
			{
				strncpy(g_szStatsText[teamNum], "No players on this team\n", sizeof(g_szStatsText[teamNum]) - 1);
			}
			else
			{
				snprintf(g_szStatsText[teamNum], sizeof(g_szStatsText[teamNum]), format,
					info.szMVP, info.iMVPVal,
					info.szMostKills, info.iKillsVal,
					info.szMostCTF, info.iCTFVal,
					info.szMostOff, info.iOffVal,
					info.szMostDef, info.iDefVal,
					info.szMostSnipe, info.iSnipeVal,
					info.szMostBarnacle, info.iBarnacleVal,
					info.szMostDeaths, info.iDeathsVal,
					info.szMostSuicides, info.iSuicidesVal,
					info.szMostDamage, info.iDamageVal,
					info.szMostAccel, STATS_MIN(info.iAccelVal), STATS_SEC(info.iAccelVal),
					info.szMostBackpack, STATS_MIN(info.iBackpackVal), STATS_SEC(info.iBackpackVal),
					info.szMostHealth, STATS_MIN(info.iHealthVal), STATS_SEC(info.iHealthVal),
					info.szMostShield, STATS_MIN(info.iShieldVal), STATS_SEC(info.iShieldVal),
					info.szMostJump, STATS_MIN(info.iJumpVal), STATS_SEC(info.iJumpVal));
			}

			gEngfuncs.COM_FreeFile(format);
		}
	}

	return 1;
}

int GoModStats_MsgStatsPlayer(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	const int playerIndex = READ_BYTE();
	const int iMVPVal = READ_SHORT();
	const int iKillsVal = READ_SHORT();
	const int iCTFVal = READ_SHORT();
	const int iOffVal = READ_SHORT();
	const int iDefVal = READ_SHORT();
	const int iSnipeVal = READ_SHORT();
	const int iBarnacleVal = READ_SHORT();
	const int iDeathsVal = READ_SHORT();
	const int iSuicidesVal = READ_SHORT();
	const int iDamageVal = READ_SHORT();
	const int iAccelVal = READ_SHORT();
	const int iBackpackVal = READ_SHORT();
	const int iHealthVal = READ_SHORT();
	const int iShieldTime = READ_SHORT();
	const int iJumpVal = READ_SHORT();

	if (playerIndex <= 0 || playerIndex >= MAX_PLAYERS_HUD)
		return 1;

	char path[64];
	snprintf(path, sizeof(path), "stats/stats_%s.ost", sCTFStatsSelection[3]);

	if (char* format = (char*)gEngfuncs.COM_LoadFile(path, 5, nullptr))
	{
		snprintf(g_szStatsText[3], sizeof(g_szStatsText[3]), format,
			iMVPVal, iKillsVal, iCTFVal, iOffVal, iDefVal, iSnipeVal, iBarnacleVal,
			iDeathsVal, iSuicidesVal, iDamageVal,
			STATS_MIN(iAccelVal), STATS_SEC(iAccelVal),
			STATS_MIN(iBackpackVal), STATS_SEC(iBackpackVal),
			STATS_MIN(iHealthVal), STATS_SEC(iHealthVal),
			STATS_MIN(iShieldTime), STATS_SEC(iShieldTime),
			STATS_MIN(iJumpVal), STATS_SEC(iJumpVal));

		gEngfuncs.COM_FreeFile(format);
	}

	return 1;
}

void GoModStats_Draw()
{
	if (!g_bStatsOpen)
		return;

	if (g_StatsImage[0].empty())
		StatsDefaults();

	const float fontSize = (std::max)(13.0f, ScreenHeight * 0.019f);
	ImGui::PushFont(nullptr, fontSize);

	const float w = (std::max)(ScreenWidth * 0.7f, (std::min)(720.0f, ScreenWidth - 20.0f));
	const float h = (std::max)(ScreenHeight * 0.7f, (std::min)(440.0f, ScreenHeight - 20.0f));

	ImGui::SetNextWindowPos(ImVec2(ScreenWidth * 0.5f, ScreenHeight * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);

	const std::string title = Localise("#CTFTitle_EndGameStats", "End Game Stats") + "###gomod_stats";
	bool open = true;
	ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

	const float btnH = (float)(int)(fontSize * 1.9f);

	// ---- views (1-4) + scoreboard / close ----
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float rightW = (float)(int)(fontSize * 8.0f);
	const float viewW = (ImGui::GetContentRegionAvail().x - rightW * 2.0f - spacing * (STATS_VIEWS + 1)) / STATS_VIEWS;

	for (int i = 0; i < STATS_VIEWS; i++)
	{
		if (i > 0)
			ImGui::SameLine();

		char label[128];
		snprintf(label, sizeof(label), "%d  %s###statsview%d", i + 1, Localise(sLocalisedStatsTeams[i], sCTFStatsSelection[i]).c_str(), i);

		const bool active = (g_iStatsView == i);
		if (active)
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
		if (ImGui::Button(label, ImVec2(viewW, btnH)))
			g_iStatsView = i;
		if (active)
			ImGui::PopStyleColor();
	}

	ImGui::SameLine();
	if (ImGui::Button((Localise("#CTFMenu_ScoreBoard", "Scoreboard") + "###statsscore").c_str(), ImVec2(rightW, btnH)))
	{
		// was SwitchToScoreBoard: the scoreboard stays open, the mouse goes back to the game
		GoModStats_Close();
		gHUD.m_Scoreboard.SetForced(true);
	}

	ImGui::SameLine();
	if (ImGui::Button((Localise("#CTFMenu_Cancel", "Close") + "###statsclose").c_str(), ImVec2(rightW, btnH)))
		open = false;

	ImGui::Separator();

	// ---- picture + title + text ----
	ImGui::BeginChild("##statsview", ImVec2(0, 0), ImGuiChildFlags_Borders);

	char key[64];
	snprintf(key, sizeof(key), "#CTFTitleStats_%s", sCTFStatsSelection[g_iStatsView]);

	ImGui::PushFont(nullptr, fontSize * 1.4f);
	ImGui::TextUnformatted(Localise(key, sCTFStatsSelection[g_iStatsView]).c_str());
	ImGui::PopFont();
	ImGui::Spacing();

	const StatsPicture& pic = StatsGetPicture(g_StatsImage[g_iStatsView]);
	if (pic.tex && pic.w > 0 && pic.h > 0)
	{
		const float picW = (std::min)((float)pic.w * 1.5f, ImGui::GetContentRegionAvail().x * 0.4f);
		const float picH = picW * pic.h / pic.w;
		ImGui::Image((ImTextureID)(intptr_t)pic.tex, ImVec2(picW, picH));
		ImGui::SameLine();
	}

	ImGui::BeginChild("##statstext", ImVec2(0, 0), ImGuiChildFlags_None);
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextUnformatted(CleanText(g_szStatsText[g_iStatsView]).c_str());
	ImGui::PopTextWrapPos();
	ImGui::EndChild();

	ImGui::EndChild();

	ImGui::End();
	ImGui::PopFont();

	if (!open)
		GoModStats_Close();
}
