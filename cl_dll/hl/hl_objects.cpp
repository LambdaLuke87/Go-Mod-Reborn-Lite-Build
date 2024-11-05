/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#include "../hud.h"
#include "../cl_util.h"
#include "../demo.h"

#include "demo_api.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"

#include "pm_defs.h"
#include "event_api.h"
#include "entity_types.h"
#include "r_efx.h"

#include <cmath>

extern BEAM* pBeam;
extern BEAM* pBeam2;
void HUD_GetLastOrg(float* org);

extern BEAM* pPhysBeam;
extern float v_frametime;
extern void V_SmoothInterpolateAngles(float* startAngle, float* endAngle, float* finalAngle, float degreesPerSec);

// TODO: Move to mathlib
static float lerp(float a, float b, float f)
{
	return (a * (1.0 - f)) + (b * f);
}

void UpdateBeams()
{
	Vector forward, vecSrc, vecEnd, origin, angles, right, up;
	Vector view_ofs;
	pmtrace_t tr;
	cl_entity_t* pthisplayer = gEngfuncs.GetLocalPlayer();
	int idx = pthisplayer->index;

	// Get our exact viewangles from engine
	gEngfuncs.GetViewAngles((float*)angles);

	// Determine our last predicted origin
	HUD_GetLastOrg((float*)&origin);

	AngleVectors(angles, forward, right, up);

	VectorCopy(origin, vecSrc);

	VectorMA(vecSrc, 2048, forward, vecEnd);

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);

	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);

	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (pBeam)
	{
		pBeam->target = tr.endpos;
		pBeam->die = gEngfuncs.GetClientTime() + 0.1; // We keep it alive just a little bit forward in the future, just in case.
	}

	if (pBeam2)
	{
		pBeam2->target = tr.endpos;
		pBeam2->die = gEngfuncs.GetClientTime() + 0.1; // We keep it alive just a little bit forward in the future, just in case.
	}
}

void UpdatePhysBeam()
{
	if (!pPhysBeam)
		return;

	if (pPhysBeam->endEntity > 0)
	{
		cl_entity_t* targent = gEngfuncs.GetEntityByIndex(pPhysBeam->endEntity);

		NormalizeAngles(targent->baseline.angles);
		Vector targang = targent->curstate.angles;
		NormalizeAngles(targang);

		// Interpolate angles side-side so it looks nicer
		v_frametime = gHUD.m_flTimeDelta * 20.0f;
		V_SmoothInterpolateAngles(targent->baseline.angles, targang, targent->baseline.angles, 180.0f);

		for (size_t i = 0; i < 3; i++)
		{
			targent->baseline.origin[i] = lerp(targent->baseline.origin[i], targent->curstate.origin[i], gHUD.m_flTimeDelta * 13.0f);
		}

		Vector targpos = targent->baseline.origin;
		targpos[2] += targent->curstate.maxs[2] / 2;
		if (pPhysBeam->frameCount > 0)
			VectorAverage(targent->curstate.maxs + targent->origin, targent->curstate.mins + targent->origin, targpos);

		pPhysBeam->target = targpos;
	}
	else
	{
		Vector forward, vecSrc, vecEnd, origin, angles, right, up;
		Vector view_ofs;
		pmtrace_t tr;
		cl_entity_t* pthisplayer = gEngfuncs.GetLocalPlayer();
		int idx = pthisplayer->index;

		// Get our exact viewangles from engine
		gEngfuncs.GetViewAngles((float*)angles);

		// Determine our last predicted origin
		HUD_GetLastOrg((float*)&origin);

		AngleVectors(angles, forward, right, up);

		VectorCopy(origin, vecSrc);

		VectorMA(vecSrc, 2048, forward, vecEnd);

		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);

		// Store off the old count
		gEngfuncs.pEventAPI->EV_PushPMStates();

		// Now add in all of the players.
		gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);

		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);

		gEngfuncs.pEventAPI->EV_PopPMStates();
	
		pPhysBeam->target = tr.endpos;
	}

	pPhysBeam->die = gEngfuncs.GetClientTime() + 0.1; // We keep it alive just a little bit forward in the future, just in case.
	pPhysBeam->freq = pPhysBeam->speed * gEngfuncs.GetClientTime();
}

/*
=====================
Game_AddObjects

Add game specific, client-side objects here
=====================
*/
void Game_AddObjects()
{
	if (pBeam || pBeam2)
		UpdateBeams();

	if (pPhysBeam && pPhysBeam->endEntity == 0)
		UpdatePhysBeam();
}
