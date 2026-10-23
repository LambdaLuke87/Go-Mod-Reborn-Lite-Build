#pragma once

// Go-Mod: Reborn - the command menu (commandmenu.txt format) done in Dear ImGui.
// Used by the "+commandmenu" menu (commandmenu.txt) and by Admin Settings of the Go-Mod menu
// (resource/adminmenu.txt). Same file format and rules as the old VGUI one:
//    "key" "text" "command"                 normal button
//    "key" "text" { ... }                   submenu
//    TOGGLE "key" "text" "cvar"             switches the cvar 0/1
//    MAP "mapname" "key" "text" "command"   only on that map
//    TEAM1..TEAM4 "key" "text" "command"    only for that team
//    CUSTOM "key" "text" "!CHANGETEAM"      submenu with the teams

#include <string>
#include <vector>

class GMCommandMenu
{
public:
	explicit GMCommandMenu(const char* file) : m_File(file) {}

	void Load();   // reads the file (also when it was already read)
	void Unload(); // it will be read again the next time
	bool IsEmpty();

	void ResetChain(); // only the first menu open
	bool KeyInput(int key);					// number key: true = a final command was run
	bool Draw(float buttonW, float buttonH); // columns inside the current ImGui window: true = a final command was run

	const char* File() const { return m_File.c_str(); }

private:
	struct Button
	{
		int type = 0;
		char key = 0;
		std::string text;
		std::string command;
		std::string map; // "maps/<name>.bsp"
		int team = 0;
		int submenu = -1;
	};

	struct Menu
	{
		int parent = -1;
		std::vector<Button> buttons;
	};

	int NewMenu(int parent);
	bool ButtonValid(const Button& b) const;
	bool ButtonVisible(const Button& b, int depth) const;
	bool MenuHasVisible(int menu, int depth) const;
	std::string ButtonText(const Button& b) const;
	bool Press(int depth, const Button& b); // true = final command

	std::string m_File;
	std::vector<Menu> m_Menus;
	std::vector<int> m_Chain; // open menus: root, submenu, sub-submenu...
	bool m_bLoaded = false;
};
