#pragma once

// Go-Mod: Reborn - physgun beam (client side)
//
// The beam is not an engine BEAM any more and no event is involved: the server sends, inside
// every player's entity state,
//     playerclass bit 0 = the player is firing the physgun
//     aiment            = entity he is holding (0 = none)
// and every frame the client draws a curved beam with TriAPI for every player that is firing,
// from the muzzle of his gun to the held entity (or to where he is aiming).
//
// Color of the beam: physgun_color_r / _g / _b (command "physgun_color", same as hud_color).
// The color travels to the other players through the userinfo key "phc" (RRGGBB in hex).

struct cl_entity_s;
struct ref_params_s;

// cvars (called from CHud::Init, after the physgun_color_* cvars exist)
void PhysGun_Init();

// sprites (called from CHud::VidInit)
void PhysGun_VidInit();

// once per frame, at the end of V_CalcRefdef (before anything is rendered)
//   locked = the view is locked because the local player is rotating the held entity
void PhysGun_Frame(struct ref_params_s* pparams, bool locked);

// player renderer: muzzle of the weapon model (attachment 0) of a player, this frame
void PhysGun_SetMuzzle(int playerIdx, const float* pos);

// HUD_DrawTransparentTriangles
void PhysGun_Draw();

// the local player is holding something with the physgun
bool PhysGun_LocalHolding();

// StudioDrawModel: smoothed origin/angles of the entity the local player is holding (cl_pred_physgun)
void PhysGun_OverrideHeldRender(struct cl_entity_s* ent);
