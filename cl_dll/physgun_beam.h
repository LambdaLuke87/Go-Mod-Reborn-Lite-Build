#pragma once

#include "r_efx.h" // BEAM

BEAM* PhysBeam_Get(int playerIdx);
void PhysBeam_Set(int playerIdx, BEAM* beam);
void PhysBeam_Clear(int playerIdx);

BEAM* PhysBeam_GetLocal();
void PhysBeam_SetLocal(BEAM* beam);
void PhysBeam_ClearLocal();