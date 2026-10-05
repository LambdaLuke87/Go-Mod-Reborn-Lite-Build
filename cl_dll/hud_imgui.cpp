// hud_imgui.cpp
// Dear ImGui in Hud

#include "hud.h"
#include "cl_util.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3.h"

#include <SDL2/SDL.h>
#include "keydefs.h"

extern cvar_t* hud_renderer;

extern bool g_iVisibleMouse;
extern void IN_ResetMouseAfterMenu();

bool g_bShowMenu = false;
static bool s_bMenuApplied = false;
static bool s_bPrevRelative = false;
static int s_iPrevCursor = SDL_DISABLE;

extern void GoModMenu_Draw();
extern void __CmdFunc_ShowMenu();

ImGuiKey TranslateValveKeyToImGui(int keynum)
{
	switch (keynum)
	{
	case K_TAB: return ImGuiKey_Tab;
	case K_LEFTARROW: return ImGuiKey_LeftArrow;
	case K_RIGHTARROW: return ImGuiKey_RightArrow;
	case K_UPARROW: return ImGuiKey_UpArrow;
	case K_DOWNARROW: return ImGuiKey_DownArrow;
	case K_ENTER: return ImGuiKey_Enter;
	case K_ESCAPE: return ImGuiKey_Escape;
	case K_SPACE: return ImGuiKey_Space;
	default: return ImGuiKey_None;
	}
}


void ImGuiMenu_SetOpen(bool open)
{
	g_bShowMenu = open;

	if (open == s_bMenuApplied)
		return;

	s_bMenuApplied = open;
	g_iVisibleMouse = open ? 1 : 0;

	ImGuiIO& io = ImGui::GetIO();

	if (open)
	{
		// save the state set by the engine in order to restore it later
		s_bPrevRelative = (SDL_GetRelativeMouseMode() == SDL_TRUE);
		s_iPrevCursor = SDL_ShowCursor(SDL_QUERY);

		SDL_SetRelativeMouseMode(SDL_FALSE);
		SDL_ShowCursor(SDL_ENABLE);
	}
	else
	{
		io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
		io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
		io.AddMouseButtonEvent(ImGuiMouseButton_Middle, false);
		io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);

		SDL_ShowCursor(s_iPrevCursor);
		SDL_SetRelativeMouseMode(s_bPrevRelative ? SDL_TRUE : SDL_FALSE);

		IN_ResetMouseAfterMenu(); // avoid the camera jump
	}
}

static void ImGuiMenu_UpdateMouse(ImGuiIO& io)
{
	int mx, my;
	Uint32 state = SDL_GetMouseState(&mx, &my);

	float x = (float)mx, y = (float)my;

	// SDL provides window coordinates; ImGui uses DisplaySize coordinates.
	if (SDL_Window* window = SDL_GetMouseFocus())
	{
		int ww, wh;
		SDL_GetWindowSize(window, &ww, &wh);
		if (ww > 0 && wh > 0)
		{
			x *= io.DisplaySize.x / ww;
			y *= io.DisplaySize.y / wh;
		}
	}

	io.AddMousePosEvent(x, y);
	io.AddMouseButtonEvent(ImGuiMouseButton_Left, (state & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0);
	io.AddMouseButtonEvent(ImGuiMouseButton_Right, (state & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0);
	io.AddMouseButtonEvent(ImGuiMouseButton_Middle, (state & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0);
}

int GoModMenu_KeyEvent(int down, int keynum)
{
	if (!g_bShowMenu)
		return 1;

	ImGuiIO& io = ImGui::GetIO();

	if (keynum == K_ESCAPE)
	{
		if (!down)
			ImGuiMenu_SetOpen(false);
		return 0; // prevent the engine from opening the main menu
	}

	if (keynum == K_MWHEELUP || keynum == K_MWHEELDOWN)
	{
		if (down)
			io.AddMouseWheelEvent(0.0f, keynum == K_MWHEELUP ? 1.0f : -1.0f);
		return 0;
	}

	return 1; // the rest (WASD, etc.) still works
}

void CHud::ImGuiMenu_Draw(float flTime)
{
	if (!g_bShowMenu)
		return; // closed menu: ImGui does not run.

	ImGuiIO& io = ImGui::GetIO();

	io.DisplaySize = ImVec2((float)gHUD.m_scrinfo.iWidth, (float)gHUD.m_scrinfo.iHeight);

	static float flLastTime = 0.0f;
	float dt = flTime - flLastTime;
	io.DeltaTime = (dt > 0.0f && dt < 0.1f) ? dt : 1.0f / 60.0f;
	flLastTime = flTime;

	ImGuiMenu_UpdateMouse(io);

	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	GoModImGuiStyle();
	GoModMenu_Draw();

	if (!g_bShowMenu) // close with x button
		ImGuiMenu_SetOpen(false);

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

int CHud::GetMenuThemeCvar()
{
	static cvar_t* pThemeCvar = nullptr;
	if (!pThemeCvar)
		pThemeCvar = gEngfuncs.pfnGetCvarPointer("cl_gomod_menu_theme");

	return pThemeCvar ? (int)pThemeCvar->value : 0;
}

static void ApplyDefaultTheme()
{
	ImGuiStyle& style = ImGui::GetStyle();

	ImVec4 darkGrayOuter = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
	ImVec4 lightGrayInner = ImVec4(0.48f, 0.48f, 0.48f, 1.00f);
	ImVec4 actionButtons = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
	ImVec4 thinBorder = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
	ImVec4 inputBackground = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
	ImVec4 hoveredColor = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
	ImVec4 activeColor = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);

	style.Colors[ImGuiCol_WindowBg] = darkGrayOuter;
	style.Colors[ImGuiCol_TitleBg] = darkGrayOuter;
	style.Colors[ImGuiCol_TitleBgActive] = darkGrayOuter;
	style.Colors[ImGuiCol_ChildBg] = lightGrayInner;
	style.Colors[ImGuiCol_Button] = actionButtons;
	style.Colors[ImGuiCol_ButtonHovered] = hoveredColor;
	style.Colors[ImGuiCol_ButtonActive] = activeColor;
	style.Colors[ImGuiCol_Border] = thinBorder;
	style.Colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	style.Colors[ImGuiCol_FrameBg] = inputBackground;
	style.Colors[ImGuiCol_FrameBgHovered] = activeColor;
	style.Colors[ImGuiCol_FrameBgActive] = thinBorder;
	style.Colors[ImGuiCol_Header] = actionButtons;
	style.Colors[ImGuiCol_HeaderHovered] = hoveredColor;
	style.Colors[ImGuiCol_HeaderActive] = activeColor;
	style.Colors[ImGuiCol_Tab] = actionButtons;
	style.Colors[ImGuiCol_TabHovered] = hoveredColor;
	style.Colors[ImGuiCol_TabActive] = lightGrayInner;
	style.Colors[ImGuiCol_PopupBg] = darkGrayOuter;
	style.Colors[ImGuiCol_ScrollbarBg] = darkGrayOuter;
	style.Colors[ImGuiCol_ScrollbarGrab] = actionButtons;
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = hoveredColor;
	style.Colors[ImGuiCol_ScrollbarGrabActive] = activeColor;
	style.Colors[ImGuiCol_SliderGrab] = actionButtons;
	style.Colors[ImGuiCol_SliderGrabActive] = hoveredColor;
	style.Colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
}

static void ApplyBlackTheme()
{
	ImGuiStyle& style = ImGui::GetStyle();

	ImVec4 windowBG = ImVec4(0.11f, 0.11f, 0.12f, 1.00f);
	ImVec4 childBG = ImVec4(0.15f, 0.15f, 0.16f, 1.00f);
	ImVec4 controlBG = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
	ImVec4 hovered = ImVec4(0.26f, 0.26f, 0.29f, 1.00f);
	ImVec4 active = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);
	ImVec4 border = ImVec4(0.06f, 0.06f, 0.07f, 1.00f);
	ImVec4 text = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
	ImVec4 sliderGrab = ImVec4(0.40f, 0.40f, 0.44f, 1.00f);

	style.Colors[ImGuiCol_WindowBg] = windowBG;
	style.Colors[ImGuiCol_TitleBg] = windowBG;
	style.Colors[ImGuiCol_TitleBgActive] = windowBG;
	style.Colors[ImGuiCol_ChildBg] = childBG;
	style.Colors[ImGuiCol_Button] = controlBG;
	style.Colors[ImGuiCol_ButtonHovered] = hovered;
	style.Colors[ImGuiCol_ButtonActive] = active;
	style.Colors[ImGuiCol_Border] = border;
	style.Colors[ImGuiCol_Text] = text;
	style.Colors[ImGuiCol_FrameBg] = active;
	style.Colors[ImGuiCol_FrameBgHovered] = hovered;
	style.Colors[ImGuiCol_FrameBgActive] = border;
	style.Colors[ImGuiCol_Header] = controlBG;
	style.Colors[ImGuiCol_HeaderHovered] = hovered;
	style.Colors[ImGuiCol_HeaderActive] = active;
	style.Colors[ImGuiCol_Tab] = controlBG;
	style.Colors[ImGuiCol_TabHovered] = hovered;
	style.Colors[ImGuiCol_TabActive] = childBG;
	style.Colors[ImGuiCol_PopupBg] = windowBG;
	style.Colors[ImGuiCol_ScrollbarBg] = windowBG;
	style.Colors[ImGuiCol_ScrollbarGrab] = controlBG;
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = hovered;
	style.Colors[ImGuiCol_ScrollbarGrabActive] = active;
	style.Colors[ImGuiCol_SliderGrab] = sliderGrab;
	style.Colors[ImGuiCol_SliderGrabActive] = hovered;
	style.Colors[ImGuiCol_CheckMark] = text;
}

static void ApplyValveTheme()
{
	ImGuiStyle& style = ImGui::GetStyle();

	ImVec4 baseText = ImVec4(216.0f / 255, 222.0f / 255, 211.0f / 255, 1.00f);	 // BaseText
	ImVec4 controlBG = ImVec4(76.0f / 255, 88.0f / 255, 68.0f / 255, 1.00f);	 // ControlBG
	ImVec4 controlDark = ImVec4(90.0f / 255, 106.0f / 255, 80.0f / 255, 1.00f);	 // ControlDarkBG
	ImVec4 windowBG = ImVec4(62.0f / 255, 70.0f / 255, 55.0f / 255, 1.00f);		 // WindowBG
	ImVec4 selectionBG = ImVec4(149.0f / 255, 136.0f / 255, 49.0f / 255, 1.00f); // SelectionBG
	ImVec4 borderDark = ImVec4(40.0f / 255, 46.0f / 255, 34.0f / 255, 1.00f);	 // BorderDark
	ImVec4 sliderTick = ImVec4(127.0f / 255, 140.0f / 255, 127.0f / 255, 1.00f); // SliderTickColor
	ImVec4 sliderTrack = ImVec4(31.0f / 255, 31.0f / 255, 31.0f / 255, 1.00f);	 // SliderTrackColor

	style.Colors[ImGuiCol_WindowBg] = windowBG;
	style.Colors[ImGuiCol_TitleBg] = controlBG;
	style.Colors[ImGuiCol_TitleBgActive] = controlBG;
	style.Colors[ImGuiCol_ChildBg] = controlDark;
	style.Colors[ImGuiCol_Button] = controlBG;
	style.Colors[ImGuiCol_ButtonHovered] = selectionBG;
	style.Colors[ImGuiCol_ButtonActive] = borderDark;
	style.Colors[ImGuiCol_Border] = borderDark;
	style.Colors[ImGuiCol_Text] = baseText;
	style.Colors[ImGuiCol_FrameBg] = windowBG;
	style.Colors[ImGuiCol_FrameBgHovered] = controlDark;
	style.Colors[ImGuiCol_FrameBgActive] = borderDark;
	style.Colors[ImGuiCol_Header] = controlBG;
	style.Colors[ImGuiCol_HeaderHovered] = selectionBG;
	style.Colors[ImGuiCol_HeaderActive] = borderDark;
	style.Colors[ImGuiCol_Tab] = controlBG;
	style.Colors[ImGuiCol_TabHovered] = selectionBG;
	style.Colors[ImGuiCol_TabActive] = controlDark;
	style.Colors[ImGuiCol_PopupBg] = windowBG;
	style.Colors[ImGuiCol_ScrollbarBg] = sliderTrack;
	style.Colors[ImGuiCol_ScrollbarGrab] = controlDark;
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = selectionBG;
	style.Colors[ImGuiCol_ScrollbarGrabActive] = borderDark;
	style.Colors[ImGuiCol_SliderGrab] = sliderTick;
	style.Colors[ImGuiCol_SliderGrabActive] = selectionBG;
	style.Colors[ImGuiCol_CheckMark] = selectionBG;
}

static void ApplyTranslucentTheme()
{
	ImGuiStyle& style = ImGui::GetStyle();

	ImVec4 baseText = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	ImVec4 controlBG = ImVec4(0.00f, 0.00f, 0.00f, 90.0f / 255);
	ImVec4 windowBG = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	ImVec4 selectionBG = ImVec4(50.0f / 255, 50.0f / 255, 50.0f / 255, 130.0f / 255);
	ImVec4 borderDark = ImVec4(112.0f / 255, 112.0f / 255, 112.0f / 255, 130.0f / 255);
	ImVec4 borderBright = ImVec4(150.0f / 255, 150.0f / 255, 150.0f / 255, 130.0f / 255);
	ImVec4 sliderTick = ImVec4(206.0f / 255, 206.0f / 255, 206.0f / 255, 130.0f / 255); // SliderTickColor
	ImVec4 sliderTrack = ImVec4(56.0f / 255, 56.0f / 255, 56.0f / 255, 130.0f / 255);	// SliderTrackColor

	style.Colors[ImGuiCol_WindowBg] = controlBG;
	style.Colors[ImGuiCol_TitleBg] = controlBG;
	style.Colors[ImGuiCol_TitleBgActive] = controlBG;
	style.Colors[ImGuiCol_ChildBg] = windowBG;
	style.Colors[ImGuiCol_Button] = controlBG;
	style.Colors[ImGuiCol_ButtonHovered] = selectionBG;
	style.Colors[ImGuiCol_ButtonActive] = borderDark;
	style.Colors[ImGuiCol_Border] = borderDark;
	style.Colors[ImGuiCol_Text] = baseText;
	style.Colors[ImGuiCol_FrameBg] = controlBG;
	style.Colors[ImGuiCol_FrameBgHovered] = selectionBG;
	style.Colors[ImGuiCol_FrameBgActive] = borderDark;
	style.Colors[ImGuiCol_Header] = controlBG;
	style.Colors[ImGuiCol_HeaderHovered] = selectionBG;
	style.Colors[ImGuiCol_HeaderActive] = borderDark;
	style.Colors[ImGuiCol_Tab] = controlBG;
	style.Colors[ImGuiCol_TabHovered] = selectionBG;
	style.Colors[ImGuiCol_TabActive] = borderBright;
	style.Colors[ImGuiCol_PopupBg] = controlBG;
	style.Colors[ImGuiCol_ScrollbarBg] = sliderTrack;
	style.Colors[ImGuiCol_ScrollbarGrab] = borderDark;
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = borderBright;
	style.Colors[ImGuiCol_ScrollbarGrabActive] = selectionBG;
	style.Colors[ImGuiCol_SliderGrab] = sliderTick;
	style.Colors[ImGuiCol_SliderGrabActive] = borderBright;
	style.Colors[ImGuiCol_CheckMark] = baseText;
}

static void ApplyLightTheme()
{
	ImGuiStyle& style = ImGui::GetStyle();

	ImVec4 windowBG = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
	ImVec4 childBG = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
	ImVec4 controlBG = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
	ImVec4 hovered = ImVec4(0.70f, 0.70f, 0.72f, 1.00f);
	ImVec4 active = ImVec4(0.60f, 0.60f, 0.63f, 1.00f);
	ImVec4 border = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
	ImVec4 text = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
	ImVec4 frameBG = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	ImVec4 sliderGrab = ImVec4(0.30f, 0.45f, 0.75f, 1.00f); // un azul para que se note bien sobre blanco

	style.Colors[ImGuiCol_WindowBg] = windowBG;
	style.Colors[ImGuiCol_TitleBg] = windowBG;
	style.Colors[ImGuiCol_TitleBgActive] = windowBG;
	style.Colors[ImGuiCol_ChildBg] = childBG;
	style.Colors[ImGuiCol_Button] = controlBG;
	style.Colors[ImGuiCol_ButtonHovered] = hovered;
	style.Colors[ImGuiCol_ButtonActive] = active;
	style.Colors[ImGuiCol_Border] = border;
	style.Colors[ImGuiCol_Text] = text;
	style.Colors[ImGuiCol_FrameBg] = frameBG;
	style.Colors[ImGuiCol_FrameBgHovered] = hovered;
	style.Colors[ImGuiCol_FrameBgActive] = active;
	style.Colors[ImGuiCol_Header] = controlBG;
	style.Colors[ImGuiCol_HeaderHovered] = hovered;
	style.Colors[ImGuiCol_HeaderActive] = active;
	style.Colors[ImGuiCol_Tab] = controlBG;
	style.Colors[ImGuiCol_TabHovered] = hovered;
	style.Colors[ImGuiCol_TabActive] = childBG;
	style.Colors[ImGuiCol_PopupBg] = windowBG;

	style.Colors[ImGuiCol_ScrollbarBg] = windowBG;
	style.Colors[ImGuiCol_ScrollbarGrab] = controlBG;
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = hovered;
	style.Colors[ImGuiCol_ScrollbarGrabActive] = active;
	style.Colors[ImGuiCol_SliderGrab] = sliderGrab;
	style.Colors[ImGuiCol_SliderGrabActive] = active;
	style.Colors[ImGuiCol_CheckMark] = sliderGrab;
}

void CHud::GoModImGuiStyle()
{
	switch (GetMenuThemeCvar())
	{
	case 1:
		ApplyBlackTheme();
		break;
	case 2:
		ApplyValveTheme();
		break;
	case 3:
		ApplyTranslucentTheme();
		break;
	case 4:
		ApplyLightTheme();
		break;
	default:
		ApplyDefaultTheme();
		break;
	}
}