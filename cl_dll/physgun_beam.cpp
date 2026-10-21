// Go-Mod: Reborn - physgun beam (client side). See physgun_beam.h

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "ref_params.h"
#include "triangleapi.h"
#include "event_api.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "kbutton.h"
#include "Exports.h"
#include "com_model.h"
#include "physgun_beam.h"

#include <cmath>
#include <cstdio>
#include <cstring>

extern kbutton_t in_attack;
extern cvar_t* cl_pred_physgun;

// physgun_color_r/g/b (hud.cpp)
extern cvar_t* m_pCvarPhysgunR;
extern cvar_t* m_pCvarPhysgunG;
extern cvar_t* m_pCvarPhysgunB;

#define PHYSGUN_BEAM_SPRITE "sprites/laserbeam.spr"
#define PHYSGUN_SEGMENTS 24
#define PHYSGUN_FADE_TIME 0.15f

static cvar_t* s_pUserInfoColor; // "phc", userinfo, RRGGBB
static HSPRITE s_hBeamSprite;
static int s_iFrame;
static float s_flLastTime;
static float s_flFrameTime;

struct PhysPlayer
{
	int muzzleFrame; // s_iFrame when muzzle was set
	Vector muzzle;

	bool aimInit;
	Vector aim; // smoothed view angles of remote players

	// last drawn curve, to fade it out when the beam stops
	bool active;
	float fade; // 1 = fully on, 0 = gone
	Vector p0, p1, p2;
	float width;

	char colorKey[16];
	float color[3];
};

static PhysPlayer s_Players[MAX_PLAYERS + 1];

// local view of this frame
static Vector s_vRenderOrg, s_vRenderAng; // camera
static Vector s_vLocalEye, s_vLocalAim;	  // eye and aim of the local player
static bool s_bThirdPerson;

// previous frame view (the view model attachment is one frame old)
static bool s_bPrevView;
static Vector s_vPrevOrg, s_vPrevFwd, s_vPrevRight, s_vPrevUp;

// smoothed entity the local player is holding (cl_pred_physgun)
static int s_iHeldEnt;
static Vector s_vHeldOrg, s_vHeldAng;

//-----------------------------------------------------------------------------

static float AngleDiff(float a, float b)
{
	float d = fmodf(a - b, 360.0f);
	if (d > 180.0f)
		d -= 360.0f;
	else if (d < -180.0f)
		d += 360.0f;
	return d;
}

static void LerpAngles(Vector& cur, const Vector& target, float f)
{
	if (f > 1.0f)
		f = 1.0f;
	for (int i = 0; i < 3; i++)
		cur[i] = cur[i] + AngleDiff(target[i], cur[i]) * f;
}

static int LocalIndex()
{
	cl_entity_t* local = gEngfuncs.GetLocalPlayer();
	return local ? local->index : 0;
}

static bool LocalHasPhysgun()
{
	WEAPON* w = gHUD.m_Ammo.GetWeapon();
	return w && w->iId == WEAPON_PHYSGUN;
}

// is the local player firing (local input, so the beam starts and stops without lag)
static bool LocalFiring()
{
	if (gHUD.m_fPlayerDead || g_iUser1 != 0)
		return false;
	if (!LocalHasPhysgun())
		return false;
	return (in_attack.state & 1) != 0;
}

// entity received in the last packet
static bool InPacket(cl_entity_t* ent)
{
	cl_entity_t* local = gEngfuncs.GetLocalPlayer();
	if (!ent || !local || !ent->model)
		return false;
	if (ent == local)
		return true;
	return ent->curstate.messagenum == local->curstate.messagenum;
}

static void ParseColor(int idx, PhysPlayer& p)
{
	const char* value = nullptr;
	char localValue[16];

	if (idx == LocalIndex() && m_pCvarPhysgunR)
	{
		snprintf(localValue, sizeof(localValue), "%02x%02x%02x",
			(int)m_pCvarPhysgunR->value & 0xFF, (int)m_pCvarPhysgunG->value & 0xFF, (int)m_pCvarPhysgunB->value & 0xFF);
		value = localValue;
	}
	else
	{
		value = gEngfuncs.PlayerInfo_ValueForKey(idx, "phc");
	}

	if (!value || !*value)
		value = "33ccff";

	if (strncmp(value, p.colorKey, sizeof(p.colorKey)) == 0)
		return;

	strncpy(p.colorKey, value, sizeof(p.colorKey) - 1);
	p.colorKey[sizeof(p.colorKey) - 1] = '\0';

	unsigned int rgb = 0x33ccff;
	if (sscanf(value, "%6x", &rgb) != 1)
		rgb = 0x33ccff;

	p.color[0] = ((rgb >> 16) & 0xFF) / 255.0f;
	p.color[1] = ((rgb >> 8) & 0xFF) / 255.0f;
	p.color[2] = (rgb & 0xFF) / 255.0f;
}

// keep the userinfo key in sync with physgun_color_r/g/b
static void SyncUserInfoColor()
{
	if (!s_pUserInfoColor || !m_pCvarPhysgunR)
		return;

	char value[16];
	snprintf(value, sizeof(value), "%02x%02x%02x",
		(int)m_pCvarPhysgunR->value & 0xFF, (int)m_pCvarPhysgunG->value & 0xFF, (int)m_pCvarPhysgunB->value & 0xFF);

	if (strcmp(value, s_pUserInfoColor->string) != 0)
		gEngfuncs.Cvar_Set("phc", value);
}

//-----------------------------------------------------------------------------

void PhysGun_Init()
{
	char value[16];
	snprintf(value, sizeof(value), "%02x%02x%02x",
		m_pCvarPhysgunR ? (int)m_pCvarPhysgunR->value & 0xFF : 0x33,
		m_pCvarPhysgunG ? (int)m_pCvarPhysgunG->value & 0xFF : 0xcc,
		m_pCvarPhysgunB ? (int)m_pCvarPhysgunB->value & 0xFF : 0xff);

	// not archived: it is rebuilt from physgun_color_r/g/b
	s_pUserInfoColor = CVAR_CREATE("phc", value, FCVAR_USERINFO);

	memset(s_Players, 0, sizeof(s_Players));
}

void PhysGun_VidInit()
{
	s_hBeamSprite = SPR_Load(PHYSGUN_BEAM_SPRITE);

	memset(s_Players, 0, sizeof(s_Players));
	s_iHeldEnt = 0;
	s_bPrevView = false;
	s_flLastTime = 0.0f;
}

void PhysGun_SetMuzzle(int playerIdx, const float* pos)
{
	if (playerIdx < 1 || playerIdx > MAX_PLAYERS)
		return;

	s_Players[playerIdx].muzzle = Vector(pos[0], pos[1], pos[2]);
	s_Players[playerIdx].muzzleFrame = s_iFrame;
}

static int LocalHeldEntity()
{
	if (!LocalFiring())
		return 0;

	cl_entity_t* local = gEngfuncs.GetLocalPlayer();
	if (!local)
		return 0;

	return local->curstate.aiment;
}

bool PhysGun_LocalHolding()
{
	return LocalHeldEntity() > 0;
}

void PhysGun_Frame(struct ref_params_s* pparams, bool locked)
{
	s_iFrame++;

	const float time = gEngfuncs.GetClientTime();
	s_flFrameTime = time - s_flLastTime;
	if (s_flFrameTime < 0.0f || s_flFrameTime > 0.5f)
		s_flFrameTime = 0.0f;
	s_flLastTime = time;

	SyncUserInfoColor();

	s_bThirdPerson = CL_IsThirdPerson() != 0;
	s_vRenderOrg = pparams->vieworg;
	s_vRenderAng = pparams->viewangles;

	s_vLocalEye = Vector(pparams->simorg[0], pparams->simorg[1], pparams->simorg[2]) + Vector(pparams->viewheight[0], pparams->viewheight[1], pparams->viewheight[2]);

	if (s_bThirdPerson && !locked)
		s_vLocalAim = pparams->cl_viewangles;
	else
		s_vLocalAim = pparams->viewangles;

	// remote players: smoothed aim
	const int maxClients = gEngfuncs.GetMaxClients();
	const int localIdx = LocalIndex();

	for (int i = 1; i <= maxClients && i <= MAX_PLAYERS; i++)
	{
		if (i == localIdx)
			continue;

		cl_entity_t* ent = gEngfuncs.GetEntityByIndex(i);
		PhysPlayer& p = s_Players[i];

		if (!ent || !InPacket(ent))
		{
			p.aimInit = false;
			continue;
		}

		// the player model gets 1/3 of the pitch, inverted
		Vector target;
		target[0] = -ent->curstate.angles[0] * 3.0f;
		target[1] = ent->curstate.angles[1];
		target[2] = 0.0f;

		if (target[0] > 89.0f)
			target[0] = 89.0f;
		else if (target[0] < -89.0f)
			target[0] = -89.0f;

		if (!p.aimInit)
		{
			p.aim = target;
			p.aimInit = true;
		}
		else
		{
			LerpAngles(p.aim, target, s_flFrameTime * 20.0f);
		}
	}

	// smoothing of the entity the local player is holding
	const int held = LocalHeldEntity();
	cl_entity_t* heldEnt = held > 0 ? gEngfuncs.GetEntityByIndex(held) : nullptr;

	if (!heldEnt || !InPacket(heldEnt))
	{
		s_iHeldEnt = 0;
	}
	else if (held != s_iHeldEnt)
	{
		s_iHeldEnt = held;
		s_vHeldOrg = heldEnt->curstate.origin;
		s_vHeldAng = heldEnt->curstate.angles;
	}
	else
	{
		float f = s_flFrameTime * 13.0f;
		if (f > 1.0f)
			f = 1.0f;

		s_vHeldOrg = s_vHeldOrg + (heldEnt->curstate.origin - s_vHeldOrg) * f;
		LerpAngles(s_vHeldAng, heldEnt->curstate.angles, s_flFrameTime * 10.0f);
	}
}

void PhysGun_OverrideHeldRender(struct cl_entity_s* ent)
{
	if (!ent || s_iHeldEnt <= 0 || ent->index != s_iHeldEnt)
		return;

	if (!cl_pred_physgun || (int)cl_pred_physgun->value <= 0)
		return;

	ent->origin = s_vHeldOrg;
	ent->angles = s_vHeldAng;
}

//-----------------------------------------------------------------------------

// muzzle of the first person view model, moved from the view of the last frame to this one
static Vector ViewModelMuzzle(const Vector& fwd, const Vector& right, const Vector& up)
{
	Vector fallback = s_vRenderOrg + fwd * 24.0f + right * 6.0f - up * 7.0f;

	cl_entity_t* vm = gEngfuncs.GetViewModel();
	if (!vm || !vm->model || !s_bPrevView)
		return fallback;

	Vector att = vm->attachment[0];
	Vector rel = att - s_vPrevOrg;

	if (rel.Length() < 1.0f || rel.Length() > 96.0f)
		return fallback;

	const float lf = DotProduct(rel, s_vPrevFwd);
	const float lr = DotProduct(rel, s_vPrevRight);
	const float lu = DotProduct(rel, s_vPrevUp);

	return s_vRenderOrg + fwd * lf + right * lr + up * lu;
}

static Vector EntityCenter(cl_entity_t* ent, bool useHeldSmoothing)
{
	Vector org = ent->origin;

	if (useHeldSmoothing && ent->index == s_iHeldEnt && cl_pred_physgun && (int)cl_pred_physgun->value > 0)
		org = s_vHeldOrg;

	return org + (ent->curstate.mins + ent->curstate.maxs) * 0.5f;
}

static Vector TraceAim(int playerIdx, const Vector& eye, const Vector& fwd)
{
	pmtrace_t tr;
	Vector end = eye + fwd * 2048.0f;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(playerIdx - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	Vector start = eye;
	gEngfuncs.pEventAPI->EV_PlayerTrace(start, end, PM_NORMAL, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	return tr.endpos;
}

static Vector Bezier(const Vector& p0, const Vector& p1, const Vector& p2, float t)
{
	const float it = 1.0f - t;
	return p0 * (it * it) + p1 * (2.0f * it * t) + p2 * (t * t);
}

static void DrawStrip(const Vector& p0, const Vector& p1, const Vector& p2, float width, float r, float g, float b, float a, float scroll)
{
	triangleapi_t* tri = gEngfuncs.pTriAPI;

	Vector prev = p0;
	Vector prevSide(0, 0, 0);
	float prevV = scroll;
	bool havePrev = false;
	float length = 0.0f;

	tri->Begin(TRI_QUADS);

	for (int i = 0; i <= PHYSGUN_SEGMENTS; i++)
	{
		const float t = (float)i / PHYSGUN_SEGMENTS;
		Vector pt = Bezier(p0, p1, p2, t);

		// tangent
		const float t2 = (i < PHYSGUN_SEGMENTS) ? (float)(i + 1) / PHYSGUN_SEGMENTS : t;
		const float t1 = (i < PHYSGUN_SEGMENTS) ? t : (float)(i - 1) / PHYSGUN_SEGMENTS;
		Vector dir = Bezier(p0, p1, p2, t2) - Bezier(p0, p1, p2, t1);

		Vector toView = pt - s_vRenderOrg;
		Vector side = CrossProduct(dir, toView);
		if (side.Length() < 0.0001f)
			side = prevSide;
		else
			side = side.Normalize();

		// thinner at the muzzle, a little wider at the end
		const float w = width * (0.55f + 0.45f * t) * 0.5f;
		side = side * w;

		if (havePrev)
		{
			length += (pt - prev).Length();
			const float v = scroll + length / 64.0f;

			tri->Color4fRendermode(r, g, b, a, kRenderTransAdd);

			tri->TexCoord2f(0.0f, prevV);
			tri->Vertex3fv(prev - prevSide);
			tri->TexCoord2f(1.0f, prevV);
			tri->Vertex3fv(prev + prevSide);
			tri->TexCoord2f(1.0f, v);
			tri->Vertex3fv(pt + side);
			tri->TexCoord2f(0.0f, v);
			tri->Vertex3fv(pt - side);

			prevV = v;
		}

		prev = pt;
		prevSide = side;
		havePrev = true;
	}

	tri->End();
}

static void DrawBeam(PhysPlayer& p, float scroll)
{
	const float a = p.fade;
	const float width = p.width * (0.4f + 0.6f * a);

	// glow
	DrawStrip(p.p0, p.p1, p.p2, width * 2.4f, p.color[0], p.color[1], p.color[2], 0.45f * a, scroll * 0.6f);
	// body
	DrawStrip(p.p0, p.p1, p.p2, width, p.color[0], p.color[1], p.color[2], 0.9f * a, scroll);
	// hot core
	DrawStrip(p.p0, p.p1, p.p2, width * 0.35f,
		p.color[0] * 0.5f + 0.5f, p.color[1] * 0.5f + 0.5f, p.color[2] * 0.5f + 0.5f, 0.8f * a, -scroll);
}

void PhysGun_Draw()
{
	if (!s_hBeamSprite)
		return;

	const model_s* sprite = gEngfuncs.GetSpritePointer(s_hBeamSprite);
	if (!sprite)
		return;

	Vector renderFwd, renderRight, renderUp;
	AngleVectors(s_vRenderAng, renderFwd, renderRight, renderUp);

	const int maxClients = gEngfuncs.GetMaxClients();
	const int localIdx = LocalIndex();
	const float time = gEngfuncs.GetClientTime();

	triangleapi_t* tri = gEngfuncs.pTriAPI;
	bool textureSet = false;

	for (int i = 1; i <= maxClients && i <= MAX_PLAYERS; i++)
	{
		PhysPlayer& p = s_Players[i];
		cl_entity_t* ent = gEngfuncs.GetEntityByIndex(i);
		const bool isLocal = (i == localIdx);

		bool firing = false;
		int target = 0;

		if (ent && InPacket(ent))
		{
			if (isLocal)
			{
				firing = LocalFiring();
				target = firing ? ent->curstate.aiment : 0;
			}
			else
			{
				firing = (ent->curstate.playerclass & 1) != 0;
				target = firing ? ent->curstate.aiment : 0;
			}
		}

		if (firing)
		{
			Vector eye, aim;

			if (isLocal)
			{
				eye = s_vLocalEye;
				aim = s_vLocalAim;
			}
			else
			{
				eye = ent->origin + Vector(0, 0, ent->curstate.usehull ? 12.0f : 28.0f);
				aim = p.aimInit ? p.aim : Vector(-ent->curstate.angles[0] * 3.0f, ent->curstate.angles[1], 0.0f);
			}

			Vector fwd, right, up;
			AngleVectors(aim, fwd, right, up);

			// start
			Vector start;
			if (isLocal && !s_bThirdPerson)
				start = ViewModelMuzzle(renderFwd, renderRight, renderUp);
			else if (p.muzzleFrame == s_iFrame && (p.muzzle - eye).Length() < 96.0f)
				start = p.muzzle;
			else
				start = eye + fwd * 20.0f + right * 6.0f - up * 8.0f;

			// end
			cl_entity_t* targEnt = target > 0 ? gEngfuncs.GetEntityByIndex(target) : nullptr;
			Vector end, ctrl;

			if (targEnt && InPacket(targEnt))
			{
				end = EntityCenter(targEnt, isLocal);

				// bend toward where the player is aiming, like the gmod one
				const float dist = (end - eye).Length();
				Vector aimPoint = eye + fwd * dist;
				Vector toEnd = (end - eye).Normalize();

				if (DotProduct(toEnd, fwd) > 0.5f)
					ctrl = start + (aimPoint - start) * 0.5f;
				else
					ctrl = (start + end) * 0.5f;

				p.width = 3.5f;
			}
			else
			{
				end = TraceAim(i, eye, fwd);
				ctrl = (start + end) * 0.5f;
				p.width = 2.0f;
			}

			p.p0 = start;
			p.p1 = ctrl;
			p.p2 = end;

			if (!p.active)
			{
				p.active = true;
				p.fade = 0.0f;
			}

			p.fade += s_flFrameTime / 0.06f;
			if (p.fade > 1.0f)
				p.fade = 1.0f;
		}
		else
		{
			p.active = false;

			if (p.fade <= 0.0f)
				continue;

			p.fade -= s_flFrameTime / PHYSGUN_FADE_TIME;
			if (p.fade <= 0.0f)
			{
				p.fade = 0.0f;
				continue;
			}
		}

		ParseColor(i, p);

		if (!textureSet)
		{
			tri->SpriteTexture((struct model_s*)sprite, 0);
			tri->RenderMode(kRenderTransAdd);
			tri->CullFace(TRI_NONE);
			textureSet = true;
		}

		DrawBeam(p, time * -2.5f + i * 0.37f);
	}

	if (textureSet)
		tri->RenderMode(kRenderNormal);

	// view of this frame, for the view model muzzle of the next one
	s_vPrevOrg = s_vRenderOrg;
	s_vPrevFwd = renderFwd;
	s_vPrevRight = renderRight;
	s_vPrevUp = renderUp;
	s_bPrevView = true;
}
