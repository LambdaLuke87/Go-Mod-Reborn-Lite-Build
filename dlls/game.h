/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
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

#pragma once

extern void GameDLLInit();
void GameDLLShutdown();


extern cvar_t displaysoundlist;

// multiplayer server rules
extern cvar_t fragsleft;
extern cvar_t timeleft;
extern cvar_t teamplay;
extern cvar_t fraglimit;
extern cvar_t timelimit;
extern cvar_t friendlyfire;
extern cvar_t falldamage;
extern cvar_t weaponstay;
extern cvar_t forcerespawn;
extern cvar_t flashlight;
extern cvar_t aimcrosshair;
extern cvar_t decalfrequency;
extern cvar_t teamlist;
extern cvar_t teamoverride;
extern cvar_t defaultteam;
extern cvar_t allowmonsters;

extern cvar_t ctf_capture;
extern cvar_t oldweapons;
extern cvar_t multipower;
extern cvar_t ctf_autoteam;
extern cvar_t coopplay;
extern cvar_t coopweprespawn;
extern cvar_t spamdelay;
extern cvar_t dropweapons;

extern cvar_t sv_allowbunnyhopping;

//extern cvar_t sv_busters; // moved to mp_gamemode 2

// Engine Cvars
extern cvar_t* g_psv_gravity;
extern cvar_t* g_psv_aim;
inline cvar_t* g_psv_allow_autoaim;
extern cvar_t* g_footsteps;
extern cvar_t* g_psv_cheats;
extern cvar_t allow_spectators;
extern cvar_t mp_chattime;

// HLSDK-Portable Cvars
extern cvar_t pushablemode;
extern cvar_t sv_pushable_fixed_tick_fudge;

//===============================
// Go-Mod Reborn CVARS
//===============================

// Global CVARS
extern cvar_t current_gamemode;
extern cvar_t mpweapons_skills;
extern cvar_t allow_gaussfly;

// Sandbox Configs
extern cvar_t custom_npc_health;
extern cvar_t custom_npc_gravity;
extern cvar_t glowstick_colors;
extern cvar_t npc_noai;
extern cvar_t monster_polem;
extern cvar_t monster_variation;
extern cvar_t spawn_wpnsetup;
extern cvar_t worlditems_respawn;

// Allow Content that handle precaches
extern cvar_t allow_props;
extern cvar_t allow_nihilant;
extern cvar_t allow_tentacle;

// Allow Functions and Gameplay
extern cvar_t allow_camera;
extern cvar_t allow_healthmodify;
extern cvar_t allow_noclip;
extern cvar_t allow_voices;

// Admin Locks and Griefing Control
extern cvar_t spawn_limit;
extern cvar_t explosion_control;
extern cvar_t bigdecals_control;
extern cvar_t onlyhoster_spawns;
extern cvar_t onlyhoster_toolbow;
extern cvar_t onlyhoster_changeAI;
