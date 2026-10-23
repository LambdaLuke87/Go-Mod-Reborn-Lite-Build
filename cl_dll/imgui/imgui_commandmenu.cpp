// imgui_commandmenu.cpp
// Go-Mod: Reborn - the command menu (commandmenu.txt format) done in Dear ImGui,
// see imgui_commandmenu.h. It replaces CCommandMenu / CreateCommandMenu of VGUI.

#include "hud.h"
#include "cl_util.h"

#include "imgui.h"
#include "imgui_commandmenu.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern int g_iTeamNumber;

// gomod_viewport.cpp (teams of "CUSTOM !CHANGETEAM")
extern int GoModAdmin_NumberOfTeams();
extern const char* GoModAdmin_TeamName(int team);
extern bool GoModAdmin_AllowSpectators();

// imgui_gomod_menu.cpp: "#Key" of resource/gomod_gui.txt (nullptr if it isn't there)
extern const char* GoModUI_KeyString(const char* key);

static const int CMD_MAX_MENUS = 80;	// MAX_MENUS
static const int CMD_BUTTON_TEXT = 32;	// MAX_BUTTON_SIZE
static const int CMD_COMMAND_SIZE = 128; // cCommandLength

enum
{
	CMD_NORMAL = 0,
	CMD_TOGGLE,
	CMD_MAP,
	CMD_TEAMONLY,
	CMD_TEAM,		 // a team of !CHANGETEAM (1-4, 5 = auto assign)
	CMD_SPECTATE,	 // spectate button of !CHANGETEAM
	CMD_CUSTOM_MENU, // the !CHANGETEAM button itself
};

void GMCommandMenu::Unload()
{
	m_bLoaded = false;
	m_Menus.clear();
	m_Chain.clear();
}

bool GMCommandMenu::IsEmpty()
{
	if (!m_bLoaded)
		Load();
	return m_Menus.empty() || m_Menus[0].buttons.empty();
}

void GMCommandMenu::ResetChain()
{
	m_Chain.clear();
	m_Chain.push_back(0);
}

static void CmdLocalise(const char* token, std::string& out)
{
	char text[CMD_BUTTON_TEXT] = "";

	// our keys first (resource/gomod_gui.txt), then the ones of the game (titles.txt)
	if (token[0] == '#')
	{
		if (const char* key = GoModUI_KeyString(token))
		{
			strncpy(text, key, sizeof(text) - 1);
			out = text;
			return;
		}
	}

	CHudTextMessage::LocaliseTextString(token, text, sizeof(text));
	text[sizeof(text) - 1] = '\0';
	out = text;
}

int GMCommandMenu::NewMenu(int parent)
{
	Menu menu;
	menu.parent = parent;
	m_Menus.push_back(menu);
	return (int)m_Menus.size() - 1;
}

void GMCommandMenu::Load()
{
	m_bLoaded = true;
	m_Menus.clear();
	m_Chain.clear();

	const int root = NewMenu(-1);

	byte* buffer = gEngfuncs.COM_LoadFile((char*)m_File.c_str(), 5, NULL);
	if (!buffer)
	{
		gEngfuncs.Con_DPrintf("Unable to open %s\n", m_File.c_str());
		return;
	}

	char token[1024];
	const char* pfile = (const char*)buffer;
	int current = root;
	char szLastButtonText[32] = "file start";

	pfile = gEngfuncs.COM_ParseFile(pfile, token);
	while (strlen(token) > 0 && (int)m_Menus.size() < CMD_MAX_MENUS)
	{
		// keep looping until we hit the end of this menu
		while (token[0] != '}' && strlen(token) > 0)
		{
			char cBoundKey[32] = "";
			char cCustom[32] = "";
			char cCommand[CMD_COMMAND_SIZE] = "";
			char szMap[256] = "";
			std::string text;
			bool iCustom = false;
			int iTeamOnly = -1;
			bool iToggle = false;
			bool bGetExtraToken = true;
			int button = -1; // index in the current menu (-1 = no button)

			if (current < 0)
			{
				gEngfuncs.Con_Printf("Error in %s file after '%s'.\n", m_File.c_str(), szLastButtonText);
				m_Menus.resize(1);
				m_Menus[0].buttons.clear();
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
			CmdLocalise(token, text);
			strncpy(szLastButtonText, text.c_str(), sizeof(szLastButtonText) - 1);
			szLastButtonText[sizeof(szLastButtonText) - 1] = '\0';

			// button command
			pfile = gEngfuncs.COM_ParseFile(pfile, token);
			strncpy(cCommand, token, CMD_COMMAND_SIZE);
			cCommand[CMD_COMMAND_SIZE - 1] = '\0';

			Button btn;
			btn.key = cBoundKey[0];
			btn.text = text;

			if (iCustom)
			{
				// only !CHANGETEAM makes a button (!CHANGECLASS is for TFC classes)
				if (0 == strcmp(cCommand, "!CHANGETEAM"))
				{
					btn.type = CMD_CUSTOM_MENU;
					btn.submenu = NewMenu(current);

					Menu& teams = m_Menus[btn.submenu];
					for (int i = 0; i < 4; i++)
					{
						Button team;
						team.type = CMD_TEAM;
						team.team = i + 1;
						team.command = "jointeam " + std::to_string(i + 1);
						teams.buttons.push_back(team);
					}

					Button autoAssign;
					autoAssign.type = CMD_TEAM;
					autoAssign.team = 5;
					autoAssign.text = GoModUI_KeyString("#Gomod_Admin_AutoAssign") ? GoModUI_KeyString("#Gomod_Admin_AutoAssign") : "Auto Assign";
					autoAssign.command = "jointeam 5";
					teams.buttons.push_back(autoAssign);

					Button spectate;
					spectate.type = CMD_SPECTATE;
					spectate.text = GoModUI_KeyString("#Gomod_Admin_Spectate") ? GoModUI_KeyString("#Gomod_Admin_Spectate") : "Spectate";
					spectate.command = "spectate";
					teams.buttons.push_back(spectate);

					m_Menus[current].buttons.push_back(btn);
					button = (int)m_Menus[current].buttons.size() - 1;
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
					btn.type = CMD_MAP;
					btn.map = std::string("maps/") + szMap + ".bsp";
				}
				else if (iTeamOnly != -1)
				{
					btn.type = CMD_TEAMONLY;
					btn.team = iTeamOnly;
				}
				else if (iToggle)
				{
					btn.type = CMD_TOGGLE;
				}

				if (cCommand[0] != '{')
					btn.command = cCommand;

				m_Menus[current].buttons.push_back(btn);
				button = (int)m_Menus[current].buttons.size() - 1;
			}

			// submenu or button?
			if (cCommand[0] == '{')
			{
				if ((int)m_Menus.size() >= CMD_MAX_MENUS)
				{
					gEngfuncs.Con_Printf("Too many menus in %s past '%s'\n", m_File.c_str(), szLastButtonText);
				}
				else if (button >= 0)
				{
					const int submenu = NewMenu(current);
					m_Menus[current].buttons[button].submenu = submenu;
					m_Menus[current].buttons[button].command.clear();
					current = submenu;
				}
			}

			if (bGetExtraToken)
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
		}

		// move back up a menu
		current = (current >= 0) ? m_Menus[current].parent : -1;

		pfile = gEngfuncs.COM_ParseFile(pfile, token);
	}

	gEngfuncs.COM_FreeFile(buffer);
}

// IsNotValid() of the buttons of the command menu, inverted
bool GMCommandMenu::ButtonValid(const Button& b) const
{
	switch (b.type)
	{
	case CMD_MAP:
	{
		const char* level = gEngfuncs.pfnGetLevelName();
		return level && 0 == strcmp(b.map.c_str(), level);
	}

	case CMD_TEAMONLY:
		return g_iTeamNumber == b.team;

	case CMD_TEAM:
	{
		const int teams = GoModAdmin_NumberOfTeams();
		if (teams == 1)
			return false;
		if (b.team == 5)
			return true;
		return teams >= b.team && b.team != g_iTeamNumber;
	}

	case CMD_SPECTATE:
		return GoModAdmin_AllowSpectators();

	default:
		return true;
	}
}

// RecalculateVisibles(): a submenu button without visible buttons inside is not shown
bool GMCommandMenu::ButtonVisible(const Button& b, int depth) const
{
	if (!ButtonValid(b))
		return false;

	if (b.submenu >= 0 && !MenuHasVisible(b.submenu, depth + 1))
		return false;

	return true;
}

bool GMCommandMenu::MenuHasVisible(int menu, int depth) const
{
	if (menu < 0 || menu >= (int)m_Menus.size() || depth > CMD_MAX_MENUS)
		return false;

	for (const Button& b : m_Menus[menu].buttons)
	{
		if (ButtonVisible(b, depth))
			return true;
	}
	return false;
}

std::string GMCommandMenu::ButtonText(const Button& b) const
{
	std::string text = b.text;

	if (b.type == CMD_TEAM && b.team >= 1 && b.team <= 4)
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

	buf[CMD_BUTTON_TEXT - 1] = '\0';
	return buf;
}

// the button was pressed: open its submenu or run it (and close the submenus, like the command menu)
bool GMCommandMenu::Press(int depth, const Button& b)
{
	if (b.submenu >= 0)
	{
		m_Chain.resize(depth + 1);
		m_Chain.push_back(b.submenu);
		return false;
	}

	if (b.type == CMD_TOGGLE)
	{
		if (cvar_t* cvar = gEngfuncs.pfnGetCvarPointer(b.command.c_str()))
			gEngfuncs.Cvar_SetValue(b.command.c_str(), cvar->value != 0 ? 0.0f : 1.0f);
	}
	else if (!b.command.empty())
	{
		gEngfuncs.pfnClientCmd(b.command.c_str());
	}


	// HideCommandMenu()
	ResetChain();
	return true;
}

// CCommandMenu::KeyInput(): the buttons of the deepest open menu
bool GMCommandMenu::KeyInput(int key)
{
	if (!m_bLoaded)
		Load();

	if (m_Chain.empty())
		ResetChain();

	const int depth = (int)m_Chain.size() - 1;
	const int menu = m_Chain.back();
	if (menu < 0 || menu >= (int)m_Menus.size())
		return false;

	for (const Button& b : m_Menus[menu].buttons)
	{
		if (ButtonValid(b) && b.key == key)
			return Press(depth, b);
	}

	return false;
}


bool GMCommandMenu::Draw(float buttonW, float buttonH)
{
	if (!m_bLoaded)
		Load();

	if (m_Chain.empty())
		ResetChain();

	bool finalCommand = false;

	const float btnW = buttonW;
	const float btnH = buttonH;
	const float stepY = btnH - 1.0f;

	const ImVec2 origin = ImGui::GetCursorPos();
	float columnY = origin.y; // y of the button that opened the menu of this column

	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

	int newChainDepth = -1; // the mouse went over a button: menus to keep open
	const Button* hoveredButton = nullptr;
	const Button* clickedButton = nullptr;
	int clickedDepth = -1;

	for (int depth = 0; depth < (int)m_Chain.size(); depth++)
	{
		const int menu = m_Chain[depth];
		if (menu < 0 || menu >= (int)m_Menus.size())
			break;

		const float columnX = origin.x + depth * (btnW + 2.0f);
		const int openChild = (depth + 1 < (int)m_Chain.size()) ? m_Chain[depth + 1] : -1;

		float rowY = columnY;
		float nextColumnY = columnY;

		ImGui::PushID(menu);

		for (int i = 0; i < (int)m_Menus[menu].buttons.size(); i++)
		{
			const Button& b = m_Menus[menu].buttons[i];
			if (!ButtonVisible(b, 0))
				continue;

			const bool armed = (b.submenu >= 0 && b.submenu == openChild);
			if (armed)
			{
				nextColumnY = rowY;
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
			}

			ImGui::SetCursorPos(ImVec2(columnX, rowY));
			ImGui::PushID(i);
			const bool pressed = ImGui::Button(ButtonText(b).c_str(), ImVec2(btnW, btnH));
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
			if (b.type == CMD_TOGGLE)
			{
				cvar_t* cvar = gEngfuncs.pfnGetCvarPointer(b.command.c_str());
				const bool on = cvar && cvar->value != 0;

				const float textW = ImGui::CalcTextSize(ButtonText(b).c_str()).x;
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
		finalCommand = Press(clickedDepth, *clickedButton);
	}
	else if (hoveredButton)
	{
		m_Chain.resize(newChainDepth + 1);
		if (hoveredButton->submenu >= 0)
			m_Chain.push_back(hoveredButton->submenu);
	}


	return finalCommand;
}
