#include "hud.h"
#include "cl_util.h"
#include "physgun_beam.h"

static BEAM* s_PhysBeam[MAX_PLAYERS];

static bool IsValidIdx(int playerIdx)
{
	return playerIdx >= 1 && playerIdx <= MAX_PLAYERS;
}

BEAM* PhysBeam_Get(int playerIdx)
{
	if (!IsValidIdx(playerIdx))
		return nullptr;

	return s_PhysBeam[playerIdx - 1];
}

void PhysBeam_Set(int playerIdx, BEAM* beam)
{
	if (!IsValidIdx(playerIdx))
		return;

	s_PhysBeam[playerIdx - 1] = beam;
}

void PhysBeam_Clear(int playerIdx)
{
	PhysBeam_Set(playerIdx, nullptr);
}

static int LocalPlayerIdx()
{
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	return pLocal ? pLocal->index : 0;
}

BEAM* PhysBeam_GetLocal()
{
	return PhysBeam_Get(LocalPlayerIdx());
}

void PhysBeam_SetLocal(BEAM* beam)
{
	PhysBeam_Set(LocalPlayerIdx(), beam);
}

void PhysBeam_ClearLocal()
{
	PhysBeam_Clear(LocalPlayerIdx());
}