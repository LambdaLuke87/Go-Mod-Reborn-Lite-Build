//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"
#include "cl_util.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "Exports.h"

#include "particleman.h"
#include "environment.h"
#include "tri.h"
extern IParticleMan* g_pParticleMan;

int g_iWaterLevel;

static void RenderFogImpl(short r, short g, short b, float startDist, float endDist, bool skyboxFog, float density, short type)
{
	// Some default values for different fog modes, in case they're not provided
	if (endDist == 0 && density > 0)
	{
		endDist = 1500;
	}
	else if (endDist > 0 && density == 0)
	{
		density = 0.001f;
	}

	if (gEngfuncs.pTriAPI->FogParams != NULL)
	{
		gEngfuncs.pTriAPI->FogParams(density, skyboxFog);
	}

	float fogColor[] = {(float)r, (float)g, (float)b};
	gEngfuncs.pTriAPI->Fog(fogColor, startDist, endDist, 1);

#if OPENGL_AVAILABLE
	int glFogType = 0;

	switch (type)
	{
	case 1:
		glFogType = GL_EXP;
		break;
	case 2:
		glFogType = GL_EXP2;
		break;
	case 3:
		glFogType = GL_LINEAR;
		break;
	default:
		break;
	}

	if (glFogType != 0 && GL_glFogi != NULL && gHUD.m_iHardwareMode == 1)
	{
		GL_glFogi(GL_FOG_MODE, glFogType);
	}
#endif
}

void RenderFog()
{
	const FogProperties& fog = gHUD.fog;
	bool bFog = g_iWaterLevel < 3 && (fog.endDist > 0 || fog.density > 0);
	if (bFog)
	{
		RenderFogImpl(fog.r, fog.g, fog.b, fog.startDist, fog.endDist, fog.affectSkybox, fog.density, fog.type);
	}
	else
	{
		float fogColor[] = {127.0f, 127.0f, 127.0f};
		gEngfuncs.pTriAPI->Fog(fogColor, 0.0f, 0.0f, 0);
	}
}

/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
=================
*/
void DLLEXPORT HUD_DrawNormalTriangles()
{
	//	RecClDrawNormalTriangles();

	UpdatePhysBeam();

	gHUD.m_Spectator.DrawOverview();
}


/*
=================
HUD_DrawTransparentTriangles

Render any triangles with transparent rendermode needs here
=================
*/
void DLLEXPORT HUD_DrawTransparentTriangles()
{
	//	RecClDrawTransparentTriangles();


	if (g_pParticleMan)
	{
		g_pParticleMan->Update();
		g_Environment.Update();
	}
}
