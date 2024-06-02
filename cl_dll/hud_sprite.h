// Based on code from Xash3D and BugfixedHL-Rebased

#pragma once

#include "cdll_int.h"
#include "com_model.h"

struct OriginalSpriteEngfuncs
{
	void (*pfnSetCrosshair)(HSPRITE hspr, Rect rc, int r, int g, int b);
};

class HudSpriteRenderer
{
public:
	HudSpriteRenderer();

	void EnableCustomCrosshair();
	void DisableCustomCrosshair();

	float GetHUDScale() const;
	bool IsCustomScale() const;

	int PerceviedScreenWidth();
	int PerceviedScreenHeight();

	int ScaleScreen(int value);
	int UnscaleScreen(int value);

	void SPR_SetInternal(HSPRITE hPic, int r, int g, int b);
	void SPR_DrawInternal(int frame, float x, float y, float width, float height, const Rect* dimensions, int mode);

	int VidInit();
	void Init();
	void HUD_Frame(double time);

	void SPR_Set(HSPRITE hPic, int r, int g, int b);
	void SPR_DrawAdditive(int frame, int x, int y, const Rect* prc);

	void FillRGBA(int x, int y, int width, int height, int r, int g, int b, int a);

	void SetCrosshair(HSPRITE hspr, Rect rc, int r, int g, int b);
	void DrawCrosshair();

	HudSpriteRenderer& DefaultScale();
	HudSpriteRenderer& RelativeScale(float multiplier);

private:
	void RecalcHUDScale();

	OriginalSpriteEngfuncs origSpriteEngfuncs;

	HSPRITE sprite;
	model_t* sprite_model;
	color24 sprite_color;

	HSPRITE crosshair;
	model_t* crosshair_model;
	Rect crosshair_dimensions;
	color24 crosshair_color;

	float hud_auto_scale_value;
	float cachedHudScale;
	float currentScale;
};