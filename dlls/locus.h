#ifndef LOCUS_H
#define LOCUS_H

#include "cbase.h"

bool		IsLikelyNumber(const char* szText);
bool		TryCalcLocus_Position	( CBaseEntity *pEntity, CBaseEntity *pLocus, const char *szText , Vector& result, bool showError = true );
bool		TryCalcLocus_Velocity	( CBaseEntity *pEntity, CBaseEntity *pLocus, const char *szText, Vector& result, bool showError = true );
bool		TryCalcLocus_Ratio		( CBaseEntity *pLocus, const char *szText, float& result, bool showError = true );
bool		TryCalcLocus_Color		( CBaseEntity *pEntity, CBaseEntity *pLocus, const char *szText, Vector& result, bool showError = true );

// Helpers that Featureful keeps in util.cpp / util.h; they live here so no other shared file has to change.
bool		UTIL_IsPlayerReference(const char* name);		// "*player"
bool		UTIL_TargetnameIsActivator(const char* targetName);	// "*locus" or "!activator"
bool		UTIL_TargetnameIsActivator(string_t targetName);

// Same as the 2-argument version, but also understands "*locus" / "!activator" (returns pActivator) and "*player"
CBaseEntity* UTIL_FindEntityByTargetname(CBaseEntity* pStartEntity, const char* szName, CBaseEntity* pActivator);

inline void WRITE_VECTOR(const Vector& vec)
{
	WRITE_COORD(vec.x);
	WRITE_COORD(vec.y);
	WRITE_COORD(vec.z);
}

inline void WRITE_COLOR(const Vector& color)
{
	WRITE_BYTE(color.x);
	WRITE_BYTE(color.y);
	WRITE_BYTE(color.z);
}

// Writes the origin followed by the point at the edge of a circle of the given radius (for TE_BEAMCYLINDER & co.)
inline void WRITE_CIRCLE(const Vector& vecSrc, float radius)
{
	WRITE_VECTOR(vecSrc);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z + radius);
}

#endif
