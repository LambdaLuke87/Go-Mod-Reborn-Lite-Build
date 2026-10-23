// Go-Mod: Reborn - the only VGUI1 left in the client.
//
// The GoldSrc engine draws the 3D world from the paintBackground of a client VGUI panel
// (gEngfuncs.VGui_ViewportPaintBackground, "only safe to call from inside Panel::paintBackground").
// Without this panel the screen stays black. It draws nothing else: all the HUD panels and
// menus are Dear ImGui (scoreboard.cpp, imgui/*, voice_status.cpp, gomod_viewport.cpp).

#include "hud.h"
#include "cl_util.h"

#include <VGUI_Panel.h>
#include <VGUI_App.h>
#include <VGUI_Scheme.h>
#include <VGUI_BorderLayout.h>

class CWorldViewPanel : public vgui::Panel
{
public:
	CWorldViewPanel(int wide, int tall) : vgui::Panel(0, 0, wide, tall)
	{
		setPaintBorderEnabled(false);
	}

	void paintBackground() override
	{
		// always the size of the screen
		int wide, tall;
		getParent()->getSize(wide, tall);
		setSize(wide, tall);

		// the engine draws the world here
		int extents[4];
		getAbsExtents(extents[0], extents[1], extents[2], extents[3]);
		gEngfuncs.VGui_ViewportPaintBackground(extents);
	}
};

static CWorldViewPanel* g_pWorldView = nullptr;

// HUD_VidInit
void WorldView_Startup()
{
	vgui::Panel* root = (vgui::Panel*)gEngfuncs.VGui_GetPanel();
	if (!root)
		return;

	if (!g_pWorldView)
	{
		root->setBgColor(128, 128, 0, 0);
		root->setLayout(new vgui::BorderLayout(0));

		g_pWorldView = new CWorldViewPanel(root->getWide(), root->getTall());
		g_pWorldView->setParent(root);
	}

	// no VGUI cursor: the mouse is the game's or ImGui's (hud_imgui.cpp)
	vgui::App::getInstance()->setCursorOveride(vgui::App::getInstance()->getScheme()->getCursor(vgui::Scheme::scu_none));
}
