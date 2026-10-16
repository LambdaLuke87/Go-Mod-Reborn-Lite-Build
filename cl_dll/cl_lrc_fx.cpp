/***
*
*	Client side of the LRC effects ported from the Featureful SDK:
*	env_streak ("Streaks"), env_smoker ("Smoke") and particle_shooter ("Particle") user messages.
*	The server side lives in dlls/effects_lrc.cpp.
*
****/
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

#include "const.h"
#include "com_model.h"
#include "r_efx.h"
#include "event_api.h"
#include "entity_types.h"
#include "particledef.h"
#include "particleman.h"

#include "lrc_fx_flags.h"

extern IParticleMan* g_pParticleMan;

// ---- message reading helpers (not part of this SDK's parsemsg.h) ----
static Vector ReadVector()
{
	const float x = READ_COORD();
	const float y = READ_COORD();
	const float z = READ_COORD();
	return Vector(x, y, z);
}

static color24 ReadColor()
{
	color24 color;
	color.r = READ_BYTE();
	color.g = READ_BYTE();
	color.b = READ_BYTE();
	return color;
}

//=========================================================
// env_streak
//=========================================================
struct StreakParams
{
	int color = 0;
	int count = 0;
	float speed = 0;
	float velocityMin = 0;
	float velocityMax = 0;
	float minLife = 0.1f;
	float maxLife = 0.5f;
	ptype_t particleType = pt_grav;
	float length = 1.0f;
};

static void FX_Streaks(Vector pos, Vector dir, const StreakParams& streakParams, bool isDirectional)
{
	float maxLife = streakParams.maxLife;
	const float minLife = streakParams.minLife;
	if (maxLife < minLife)
		maxLife = minLife;

	const Vector baseVel = isDirectional ? dir * streakParams.speed : Vector(0, 0, 0);

	for (int i = 0; i < streakParams.count; i++)
	{
		Vector vel = baseVel;
		vel.x += gEngfuncs.pfnRandomFloat(streakParams.velocityMin, streakParams.velocityMax);
		vel.y += gEngfuncs.pfnRandomFloat(streakParams.velocityMin, streakParams.velocityMax);
		vel.z += gEngfuncs.pfnRandomFloat(streakParams.velocityMin, streakParams.velocityMax);

		particle_t* p = gEngfuncs.pEfxAPI->R_TracerParticles(pos, vel, gEngfuncs.pfnRandomFloat(minLife, maxLife));
		if (!p)
			return;

		p->type = streakParams.particleType;
		p->color = streakParams.color;
		p->ramp = streakParams.length;
	}
}

int __MsgFunc_Streaks(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	StreakParams streakParams;

	const bool isDirectional = READ_BYTE() ? true : false;
	const Vector pos = ReadVector();
	Vector dir(0, 0, 0);
	if (isDirectional)
		dir = ReadVector();
	streakParams.color = READ_BYTE();
	streakParams.count = READ_SHORT();
	streakParams.speed = READ_SHORT();
	streakParams.velocityMax = READ_SHORT();
	streakParams.velocityMin = -streakParams.velocityMax;
	streakParams.minLife = READ_BYTE() * 0.1f;
	streakParams.maxLife = READ_BYTE() * 0.1f;
	streakParams.particleType = (ptype_t)READ_BYTE();
	streakParams.length = READ_BYTE() * 0.1f;

	FX_Streaks(pos, dir, streakParams, isDirectional);

	return 1;
}

//=========================================================
// env_smoker
//=========================================================
// Temp entity callback: sprite fades out with its animation and/or grows (or shrinks) over time
static void ExpandCallback(TEMPENTITY* ent, float frametime, float currenttime)
{
	const float minScale = 0.0001f;
	const float timeCreated = ent->entity.curstate.fuser1;
	const float originalScale = ent->entity.curstate.fuser2;
	const float scaleSpeed = ent->entity.curstate.fuser3;
	const bool fade = ent->entity.curstate.iuser1 != 0;

	if (fade)
	{
		const int originalRenderamt = ent->entity.curstate.iuser2;
		ent->entity.curstate.renderamt = (ent->frameMax - ent->entity.curstate.frame) / ent->frameMax * originalRenderamt;
		if (ent->entity.curstate.renderamt == 0)
		{
			ent->die = currenttime;
		}
	}

	if (scaleSpeed)
	{
		ent->entity.curstate.scale = scaleSpeed * originalScale * (currenttime - timeCreated) + originalScale;
		if (ent->entity.curstate.scale < minScale)
		{
			ent->entity.curstate.scale = minScale;
			ent->die = currenttime;
		}
	}
}

static void FX_Smoke(TEMPENTITY* pTemp, float scale, float speed, float zOffset, int rendermode, int renderamt, color24 color)
{
	pTemp->entity.curstate.rendermode = rendermode;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.baseline.origin[2] = speed;
	pTemp->entity.curstate.rendercolor = color;
	pTemp->entity.curstate.renderamt = renderamt;
	pTemp->entity.origin[2] += zOffset;
	pTemp->entity.curstate.scale = scale;
}

int __MsgFunc_Smoke(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	Vector pos, dir(0, 0, 0);
	int modelIndex;
	float scale, frameRate, speed, zOffset, scaleSpeed;
	int rendermode, renderamt;

	const int flags = READ_BYTE();
	const bool directed = (flags & SMOKER_FLAG_DIRECTED) != 0;
	const bool fade = (flags & SMOKER_FLAG_FADE_SPRITE) != 0;
	pos = ReadVector();
	modelIndex = READ_SHORT();

	const float scaleVal = READ_COORD();
	scale = (flags & SMOKER_FLAG_SCALE_VALUE_IS_NORMAL) ? scaleVal : (float)(scaleVal * 0.1f);

	frameRate = READ_BYTE();
	speed = READ_SHORT();
	zOffset = READ_SHORT();
	rendermode = READ_BYTE();
	renderamt = READ_BYTE();
	color24 color = ReadColor();
	scaleSpeed = READ_SHORT() / 10.0f;

	if (directed)
	{
		dir = ReadVector();
	}

	// Original hard-coded TE_SMOKE values
	if (speed == 0.0f && zOffset == 0.0f)
	{
		speed = 30;
		zOffset = 20;
	}
	if (renderamt == 0)
		renderamt = 255;
	if (rendermode == 0)
		rendermode = kRenderTransAlpha;
	if (color.r + color.g + color.b == 0)
		color.r = color.g = color.b = gEngfuncs.pfnRandomLong(20, 35);

	TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->R_DefaultSprite(pos, modelIndex, frameRate);

	if (pTemp)
	{
		FX_Smoke(pTemp, scale, speed, zOffset, rendermode, renderamt, color);
		if (directed)
		{
			pTemp->entity.baseline.origin = dir * speed;
		}
		if (scaleSpeed != 0 || fade)
		{
			pTemp->entity.curstate.fuser1 = gEngfuncs.GetClientTime();
			pTemp->entity.curstate.fuser2 = scale;
			pTemp->entity.curstate.fuser3 = scaleSpeed;
			pTemp->entity.curstate.iuser1 = fade ? 1 : 0;
			pTemp->entity.curstate.iuser2 = renderamt;
			pTemp->flags |= FTENT_CLIENTCUSTOM;
			pTemp->callback = &ExpandCallback;
		}
	}

	return 1;
}

//=========================================================
// particle_shooter
//=========================================================
int __MsgFunc_Particle(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	const Vector position = ReadVector();
	const Vector velocity = ReadVector();

	const int renderMode = READ_BYTE();
	const float particleLife = READ_BYTE() / 10.0f;
	const float particleSize = READ_SHORT() / 10.0f;
	const int red = READ_BYTE();
	const int green = READ_BYTE();
	const int blue = READ_BYTE();
	const int brightness = READ_BYTE();
	const float gravity = READ_BYTE() / 100.0f;
	const float fadeSpeed = READ_BYTE() / 10.0f;
	const float scaleSpeed = READ_BYTE() / 10.0f;
	const int frameRate = READ_BYTE();
	const int flags = READ_BYTE();
	const int modelIndex = READ_SHORT();

	// ParticleMan is optional (it's a separate library loaded by cdll_int.cpp)
	if (g_pParticleMan)
	{
		const float clTime = gEngfuncs.GetClientTime();

		model_t* sprite = gEngfuncs.hudGetModelByIndex(modelIndex);
		if (sprite)
		{
			CBaseParticle* particle = g_pParticleMan->CreateParticle(position, Vector(0.0f, 0.0f, 0.0f), sprite, particleSize > 0 ? particleSize : 16, brightness, "particle");
			if (particle)
			{
				particle->SetLightFlag(LIGHT_NONE);
				particle->SetCullFlag(CULL_PVS);
				particle->SetRenderFlag(RENDER_FACEPLAYER);

				particle->m_vVelocity = velocity;
				particle->m_iRendermode = renderMode;
				particle->m_vColor = Vector(red, green, blue);
				particle->m_flGravity = gravity;
				if (fadeSpeed > 0)
					particle->m_flFadeSpeed = fadeSpeed;
				particle->m_flScaleSpeed = scaleSpeed;
				if (flags & SF_PARTICLESHOOTER_ANIMATED)
				{
					particle->m_iFramerate = frameRate > 0 ? frameRate : 10;
					particle->m_iNumFrames = sprite->numframes;
				}

				if (flags & SF_PARTICLESHOOTER_SPIRAL)
					particle->SetCollisionFlags(TRI_SPIRAL);
				if (flags & SF_PARTICLESHOOTER_COLLIDE_WITH_WORLD)
					particle->SetCollisionFlags(TRI_COLLIDEWORLD);
				if (flags & SF_PARTICLESHOOTER_AFFECTED_BY_FORCE)
					particle->m_bAffectedByForce = true;
				if (flags & SF_PARTICLESHOOTER_KILLED_ON_COLLIDE)
					particle->SetCollisionFlags(TRI_COLLIDEKILL);

				particle->m_flDieTime = clTime + particleLife;
			}
		}
	}

	return 1;
}

// Called from CHud::Init()
void HookLrcEffectMessages()
{
	HOOK_MESSAGE(Streaks);
	HOOK_MESSAGE(Smoke);
	HOOK_MESSAGE(Particle);
}
