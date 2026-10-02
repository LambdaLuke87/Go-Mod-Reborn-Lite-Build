/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "animation.h"
#include "scripted.h"
#include "nodes.h"
#include "defaultai.h"
#include "soundent.h"
#include "custom_sounds.h"

static bool PlayCustomSound(CBaseMonster* pThis, const char* type)
{
	if (pThis->m_customSoundsId.empty())
		return false;

	const SoundGroupDef* pGroup = FindCustomSoundGroup(pThis->m_customSoundsId, type);
	if (!pGroup || pGroup->sounds.empty())
		return false;

	int idx = RANDOM_LONG(0, (int)pGroup->sounds.size() - 1);
	EMIT_SOUND_DYN(ENT(pThis->pev), CHAN_VOICE, pGroup->sounds[idx].c_str(), pGroup->volume, ATTN_NORM, 0, pGroup->pitch);
	return true;
}

void CBaseMonster::BaseDeathSound()
{
	if (!PlayCustomSound(this, "death"))
		DeathSound();
}

void CBaseMonster::BaseAlertSound()
{
	if (!PlayCustomSound(this, "alert"))
		AlertSound();
}

void CBaseMonster::BaseIdleSound()
{
	if (!PlayCustomSound(this, "idle"))
		IdleSound();
}

void CBaseMonster::BasePainSound()
{
	if (!PlayCustomSound(this, "pain"))
		PainSound();
}

void CBaseMonster::BaseAttackSound()
{
	if (!PlayCustomSound(this, "attack"))
		AttackSound();
}