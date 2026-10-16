// Flags shared by the server (dlls/effects_lrc.cpp) and the client (cl_dll/cl_lrc_fx.cpp)
// for the LRC effects ported from Featureful: particle_shooter, env_smoker.
#pragma once
#ifndef LRC_FX_FLAGS_H
#define LRC_FX_FLAGS_H

// particle_shooter spawnflags (also sent to the client in the "Particle" message)
#define SF_PARTICLESHOOTER_REPEATABLE 1
#define SF_PARTICLESHOOTER_SPIRAL 2
#define SF_PARTICLESHOOTER_COLLIDE_WITH_WORLD 4
#define SF_PARTICLESHOOTER_AFFECTED_BY_FORCE 8
#define SF_PARTICLESHOOTER_ANIMATED 16
#define SF_PARTICLESHOOTER_STARTON 32
#define SF_PARTICLESHOOTER_KILLED_ON_COLLIDE 64

// env_smoker flags (first byte of the "Smoke" message)
#define SMOKER_FLAG_DIRECTED (1 << 0)
#define SMOKER_FLAG_FADE_SPRITE (1 << 1)
#define SMOKER_FLAG_SCALE_VALUE_IS_NORMAL (1 << 2)

#endif
