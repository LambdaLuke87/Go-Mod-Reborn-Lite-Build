/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
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
//
// Scoreboard.cpp
//
// Go-Mod: Reborn - the scoreboard, drawn with Dear ImGui (the VGUI ScorePanel is gone).
//
// Data (same messages as always):
//   ScoreInfo  byte player, short frags, short deaths, short class, short team number
//   TeamInfo   byte player, string team name (+ byte team id in CTF)
//   TeamScore  string team name, short frags, short deaths
//   PlayerIcon byte player, byte active, byte icon index, byte item flags (CTF)
//   CTFScore   byte player, byte flag captures
//   GameName   string name of the game mode (Go-Mod, top of the scoreboard)
//
// Steam avatars: steam_api of the game (already loaded by the engine), opened with SDL and
// used through its C functions. If it isn't there or something fails, there are no avatars
// (a square with the first letter of the name instead). cl_scoreboard_avatars 0 turns them off.
//
// Mouse: with the scoreboard open, the right click gives the mouse (to mute players),
// like the old "squelch mode" of the voice manager.
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "voice_status.h"

#include "imgui/imgui.h"

#include <SDL2/SDL.h>

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include "ctf/CTFDefs.h"

extern int giTeamplay;
extern int g_iUser1;

// imgui_gomod_menu.cpp
extern unsigned int GoModUI_UploadTexture(unsigned int tex, const unsigned char* rgba, int w, int h);
extern void GoModUI_DeleteTexture(unsigned int& tex);
extern const char* GoModInfo_ServerName();

team_info_t g_TeamInfo[MAX_TEAMS + 1];
int g_IsSpectator[MAX_PLAYERS_HUD + 1];

// voice_status.cpp (ban list by unique id)
bool HACK_GetPlayerUniqueID(int iPlayer, char playerID[16])
{
	return 0 != gEngfuncs.GetPlayerUniqueID(iPlayer, playerID);
}

// CTF items of every player (flags and power ups, message PlayerIcon)
static unsigned int g_PlayerItems[MAX_PLAYERS_HUD + 1];

static std::string g_GameModeName; // message GameName

// steam id of every player (message SteamID, the engine leaves m_nSteamID in 0)
static uint64 g_PlayerSteamID[MAX_PLAYERS_HUD + 1];

DECLARE_COMMAND(m_Scoreboard, ShowScores);
DECLARE_COMMAND(m_Scoreboard, HideScores);

DECLARE_MESSAGE(m_Scoreboard, ScoreInfo);
DECLARE_MESSAGE(m_Scoreboard, TeamInfo);
DECLARE_MESSAGE(m_Scoreboard, TeamScore);
DECLARE_MESSAGE(m_Scoreboard, PlayerIcon);
DECLARE_MESSAGE(m_Scoreboard, CTFScore);

static int __MsgFunc_SteamID(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	const int player = READ_BYTE();
	const uint32 low = (uint32)READ_LONG();
	const uint32 high = (uint32)READ_LONG();

	if (player >= 1 && player <= MAX_PLAYERS_HUD)
		g_PlayerSteamID[player] = ((uint64)high << 32) | low;

	return 1;
}

static int __MsgFunc_GameName(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	g_GameModeName = READ_STRING();
	return 1;
}

static cvar_t* cl_scoreboard_avatars = nullptr;
static void Scoreboard_AvatarsDebug(); // below

bool CHudScoreboard::Init()
{
	gHUD.AddHudElem(this);

	HOOK_MESSAGE(ScoreInfo);
	HOOK_MESSAGE(TeamScore);
	HOOK_MESSAGE(TeamInfo);
	HOOK_MESSAGE(PlayerIcon);
	HOOK_MESSAGE(CTFScore);
	gEngfuncs.pfnHookUserMsg("GameName", __MsgFunc_GameName);
	gEngfuncs.pfnHookUserMsg("SteamID", __MsgFunc_SteamID);

	InitHUDData();

	cl_showpacketloss = CVAR_CREATE("cl_showpacketloss", "0", FCVAR_ARCHIVE);
	cl_scoreboard_avatars = CVAR_CREATE("cl_scoreboard_avatars", "1", FCVAR_ARCHIVE);
	gEngfuncs.pfnAddCommand("scoreboard_avatars_debug", Scoreboard_AvatarsDebug);

	return true;
}

bool CHudScoreboard::VidInit()
{
	return true;
}

void CHudScoreboard::InitHUDData()
{
	memset(g_PlayerExtraInfo, 0, sizeof g_PlayerExtraInfo);
	m_iLastKilledBy = 0;
	m_fLastKillTime = 0;
	m_iPlayerNum = 0;
	m_iNumTeams = 0;
	m_bForced = false;
	memset(g_TeamInfo, 0, sizeof g_TeamInfo);
	memset(g_PlayerItems, 0, sizeof g_PlayerItems);
	memset(g_PlayerSteamID, 0, sizeof g_PlayerSteamID);

	m_iFlags &= ~HUD_ACTIVE; // starts out inactive
}

// the scoreboard is drawn by ImGui (CHudScoreboard::DrawImGui, from hud_imgui.cpp)
bool CHudScoreboard::Draw(float fTime)
{
	return true;
}

void CHudScoreboard::GetAllPlayersInfo()
{
	for (int i = 1; i < MAX_PLAYERS_HUD; i++)
	{
		gEngfuncs.pfnGetPlayerInfo(i, &g_PlayerInfoList[i]);

		// empty slot: the next one that uses it (a bot...) must not keep this steam id
		if (!g_PlayerInfoList[i].name || !g_PlayerInfoList[i].name[0])
			g_PlayerSteamID[i] = 0;

		if (0 != g_PlayerInfoList[i].thisplayer)
			m_iPlayerNum = i;
	}
}

// =====================================================================
//  Messages
// =====================================================================

bool CHudScoreboard::MsgFunc_ScoreInfo(const char* pszName, int iSize, void* pbuf)
{
	m_iFlags |= HUD_ACTIVE;

	BEGIN_READ(pbuf, iSize);
	short cl = READ_BYTE();
	short frags = READ_SHORT();
	short deaths = READ_SHORT();
	short playerclass = READ_SHORT();
	short teamnumber = READ_SHORT();

	if (cl > 0 && cl <= MAX_PLAYERS_HUD)
	{
		g_PlayerExtraInfo[cl].frags = frags;
		g_PlayerExtraInfo[cl].deaths = deaths;
		g_PlayerExtraInfo[cl].playerclass = playerclass;
		g_PlayerExtraInfo[cl].teamnumber = teamnumber < 0 ? 0 : teamnumber;
	}

	return true;
}

// Message handler for TeamInfo message
// accepts two values:
//		byte: client number
//		string: client team name
bool CHudScoreboard::MsgFunc_TeamInfo(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	short cl = READ_BYTE();

	if (cl > 0 && cl <= MAX_PLAYERS_HUD)
	{
		// set the players team
		strncpy(g_PlayerExtraInfo[cl].teamname, READ_STRING(), MAX_TEAM_NAME);
		g_PlayerExtraInfo[cl].teamname[MAX_TEAM_NAME - 1] = '\0';

		if (gHUD.m_Teamplay == 2)
			g_PlayerExtraInfo[cl].teamid = READ_BYTE();
	}

	RebuildTeams();
	return true;
}

// Message handler for TeamScore message
// accepts three values:
//		string: team name
//		short: teams kills
//		short: teams deaths
// if this message is never received, then scores will simply be the combined totals of the players.
bool CHudScoreboard::MsgFunc_TeamScore(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	char* TeamName = READ_STRING();

	// find the team matching the name
	int i;
	for (i = 1; i <= m_iNumTeams; i++)
	{
		if (!stricmp(TeamName, g_TeamInfo[i].name))
			break;
	}
	if (i > m_iNumTeams)
		return true;

	// use this new score data instead of combined player scores
	g_TeamInfo[i].scores_overriden = true;
	g_TeamInfo[i].frags = READ_SHORT();
	g_TeamInfo[i].deaths = READ_SHORT();

	return true;
}

bool CHudScoreboard::MsgFunc_PlayerIcon(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	const short playerIndex = READ_BYTE();
	const bool isActive = READ_BYTE() != 0;
	const int iconIndex = READ_BYTE();
	const unsigned char itemId = READ_BYTE();

	if (playerIndex < 0 || playerIndex > MAX_PLAYERS_HUD)
		return true;

	if (!isActive)
	{
		g_PlayerItems[playerIndex] &= ~(unsigned int)itemId;
	}
	else if (0 == itemId)
	{
		// slot 0 is the flag
		if (iconIndex == 0)
			g_PlayerItems[playerIndex] &= ~(unsigned int)(CTFItem::BlackMesaFlag | CTFItem::OpposingForceFlag);
	}
	else
	{
		g_PlayerItems[playerIndex] |= itemId;
	}

	m_iFlags |= HUD_ACTIVE;
	return true;
}

bool CHudScoreboard::MsgFunc_CTFScore(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	const int playerIndex = READ_BYTE();
	const int score = READ_BYTE();

	if (playerIndex >= 1 && playerIndex <= MAX_PLAYERS_HUD)
		g_PlayerExtraInfo[playerIndex].flagcaptures = score;

	return true;
}

// Recalculate the existing teams in the match
int CHudScoreboard::RebuildTeams()
{
	int i;

	if (gHUD.m_Teamplay == 2)
	{
		// Hardcoded to 2 because m_iNumTeams may not be the correct value at this time
		for (i = 1; i <= 2; i++)
			g_TeamInfo[i].players = 0;
	}
	else
	{
		for (i = 1; i <= m_iNumTeams; i++)
			g_TeamInfo[i].players = 0;
	}

	// rebuild the team list
	GetAllPlayersInfo();
	m_iNumTeams = 0;

	for (i = 1; i < MAX_PLAYERS_HUD; i++)
	{
		if (g_PlayerInfoList[i].name == NULL)
			continue;

		if (g_PlayerExtraInfo[i].teamname[0] == 0)
			continue; // skip over players who are not in a team

		int j;

		if (gHUD.m_Teamplay == 2)
		{
			// CTF uses predefined teams with fixed team ids
			j = g_PlayerExtraInfo[i].teamid;
			if (j < 1 || j > MAX_TEAMS)
				continue;

			if (g_TeamInfo[j].name[0] == '\0')
			{
				strncpy(g_TeamInfo[j].name, g_PlayerExtraInfo[i].teamname, MAX_TEAM_NAME);
				g_TeamInfo[j].players = 0;
			}

			m_iNumTeams = V_max(j, m_iNumTeams);
		}
		else
		{
			// is this player in an existing team?
			for (j = 1; j <= m_iNumTeams; j++)
			{
				if (g_TeamInfo[j].name[0] == '\0')
					break;

				if (!stricmp(g_PlayerExtraInfo[i].teamname, g_TeamInfo[j].name))
					break;
			}

			if (j > m_iNumTeams)
			{
				// they aren't in a listed team, so make a new one
				for (j = 1; j <= m_iNumTeams; j++)
				{
					if (g_TeamInfo[j].name[0] == '\0')
						break;
				}

				if (j > MAX_TEAMS)
					continue;

				m_iNumTeams = V_max(j, m_iNumTeams);

				strncpy(g_TeamInfo[j].name, g_PlayerExtraInfo[i].teamname, MAX_TEAM_NAME);
				g_TeamInfo[j].players = 0;
			}
		}

		g_TeamInfo[j].players++;
	}

	const int teamsToCheck = gHUD.m_Teamplay == 2 ? 2 : m_iNumTeams;

	// clear out any empty teams
	for (i = 1; i <= teamsToCheck; i++)
	{
		if (g_TeamInfo[i].players < 1)
			memset(&g_TeamInfo[i], 0, sizeof(team_info_t));
	}

	return m_iNumTeams;
}

void CHudScoreboard::DeathMsg(int killer, int victim)
{
	// if we were the one killed,  or the world killed us, set the scoreboard to indicate suicide
	if (victim == m_iPlayerNum || killer == 0)
	{
		m_iLastKilledBy = 0 != killer ? killer : m_iPlayerNum;
		m_fLastKillTime = gHUD.m_flTime + 10; // display who we were killed by for 10 seconds

		if (killer == m_iPlayerNum)
			m_iLastKilledBy = m_iPlayerNum;
	}
}

void CHudScoreboard::UserCmd_ShowScores()
{
	m_iShowscoresHeld = true;
}

void CHudScoreboard::UserCmd_HideScores()
{
	m_iShowscoresHeld = false;

	// the mouse goes back to the game
	if (GetClientVoiceMgr()->IsInSquelchMode())
		GetClientVoiceMgr()->StopSquelchMode();
}

void CHudScoreboard::SetForced(bool forced)
{
	m_bForced = forced;

	if (!forced && GetClientVoiceMgr()->IsInSquelchMode())
		GetClientVoiceMgr()->StopSquelchMode();
}

bool CHudScoreboard::IsVisible()
{
	// no scoreboard in single player
	if (gEngfuncs.GetMaxClients() <= 1)
		return false;

	if ((m_iFlags & HUD_ACTIVE) == 0)
		return false;

	if (m_iShowscoresHeld || m_bForced)
		return true;

	// intermission (CTF has its own stats window)
	if (gHUD.m_iIntermission && gHUD.m_Teamplay != 2)
		return true;

	// dead (not spectating)
	if (gHUD.m_Health.m_iHealth <= 0 && 0 == g_iUser1 && !gHUD.m_iIntermission)
		return true;

	return false;
}

// right click with the scoreboard open: mouse to mute players
// (only when it was opened on purpose: dead, the left click has to respawn)
bool CHudScoreboard::WantsMouseClick()
{
	return (m_iShowscoresHeld || m_bForced) && IsVisible() && !gHUD.m_iIntermission;
}

// =====================================================================
//  Steam avatars
// =====================================================================

typedef int (*PFN_GetHSteamUser)();
typedef void* (*PFN_FindOrCreateUserInterface)(int hSteamUser, const char* version);
typedef void* (*PFN_GetInterface)();
typedef int (*PFN_GetMediumFriendAvatar)(void* friends, uint64 steamID);
typedef bool (*PFN_RequestUserInformation)(void* friends, uint64 steamID, bool requireNameOnly);
typedef bool (*PFN_GetImageSize)(void* utils, int image, uint32* width, uint32* height);
typedef bool (*PFN_GetImageRGBA)(void* utils, int image, uint8* dest, int destSize);
typedef uint64 (*PFN_GetSteamID)(void* user);

struct SteamAvatarAPI
{
	bool tried = false;
	bool ok = false;

	void* lib = nullptr;
	void* friends = nullptr;
	void* utils = nullptr;
	void* user = nullptr;
	std::string friendsFrom; // how the interfaces were found (debug)
	std::string utilsFrom;

	PFN_GetMediumFriendAvatar GetMediumFriendAvatar = nullptr;
	PFN_RequestUserInformation RequestUserInformation = nullptr;
	PFN_GetImageSize GetImageSize = nullptr;
	PFN_GetImageRGBA GetImageRGBA = nullptr;
	PFN_GetSteamID GetSteamID = nullptr; // own steam id (if the engine doesn't give it)
};

static SteamAvatarAPI g_Steam;

static void* SteamGetInterface(void* lib, const char* const* versions, const char* const* accessors, std::string& from)
{
	// newer steam_api: interface by version
	PFN_GetHSteamUser getUser = (PFN_GetHSteamUser)SDL_LoadFunction(lib, "SteamAPI_GetHSteamUser");
	PFN_FindOrCreateUserInterface find = (PFN_FindOrCreateUserInterface)SDL_LoadFunction(lib, "SteamInternal_FindOrCreateUserInterface");

	if (getUser && find)
	{
		const int user = getUser();
		if (user)
		{
			for (int i = 0; versions[i]; i++)
			{
				if (void* iface = find(user, versions[i]))
				{
					from = versions[i];
					return iface;
				}
			}
		}
	}

	// accessors: "SteamAPI_SteamFriends_v017", "SteamFriends"...
	for (int i = 0; accessors[i]; i++)
	{
		if (PFN_GetInterface get = (PFN_GetInterface)SDL_LoadFunction(lib, accessors[i]))
		{
			if (void* iface = get())
			{
				from = accessors[i];
				return iface;
			}
		}
	}

	return nullptr;
}

static bool SteamAvatarsReady()
{
	if (g_Steam.tried)
		return g_Steam.ok;

	g_Steam.tried = true;

#ifdef _WIN32
	void* lib = SDL_LoadObject("steam_api.dll");
#elif defined(__APPLE__)
	void* lib = SDL_LoadObject("libsteam_api.dylib");
#else
	void* lib = SDL_LoadObject("libsteam_api.so");
#endif

	if (!lib)
	{
		gEngfuncs.Con_Printf("Scoreboard: steam_api not found, no avatars (%s)\n", SDL_GetError());
		return false;
	}

	g_Steam.lib = lib;

	static const char* const friendsVersions[] = {"SteamFriends017", "SteamFriends016", "SteamFriends015", nullptr};
	static const char* const friendsAccessors[] = {"SteamAPI_SteamFriends_v017", "SteamAPI_SteamFriends", "SteamFriends", nullptr};
	static const char* const utilsVersions[] = {"SteamUtils010", "SteamUtils009", "SteamUtils008", nullptr};
	static const char* const utilsAccessors[] = {"SteamAPI_SteamUtils_v010", "SteamAPI_SteamUtils", "SteamUtils", nullptr};

	static const char* const userVersions[] = {"SteamUser023", "SteamUser022", "SteamUser021", "SteamUser020", "SteamUser019", nullptr};
	static const char* const userAccessors[] = {"SteamAPI_SteamUser_v023", "SteamAPI_SteamUser_v022", "SteamAPI_SteamUser_v021", "SteamAPI_SteamUser", "SteamUser", nullptr};

	g_Steam.friends = SteamGetInterface(lib, friendsVersions, friendsAccessors, g_Steam.friendsFrom);
	g_Steam.utils = SteamGetInterface(lib, utilsVersions, utilsAccessors, g_Steam.utilsFrom);

	std::string userFrom;
	g_Steam.user = SteamGetInterface(lib, userVersions, userAccessors, userFrom);
	g_Steam.GetSteamID = (PFN_GetSteamID)SDL_LoadFunction(lib, "SteamAPI_ISteamUser_GetSteamID");

	g_Steam.GetMediumFriendAvatar = (PFN_GetMediumFriendAvatar)SDL_LoadFunction(lib, "SteamAPI_ISteamFriends_GetMediumFriendAvatar");
	g_Steam.RequestUserInformation = (PFN_RequestUserInformation)SDL_LoadFunction(lib, "SteamAPI_ISteamFriends_RequestUserInformation");
	g_Steam.GetImageSize = (PFN_GetImageSize)SDL_LoadFunction(lib, "SteamAPI_ISteamUtils_GetImageSize");
	g_Steam.GetImageRGBA = (PFN_GetImageRGBA)SDL_LoadFunction(lib, "SteamAPI_ISteamUtils_GetImageRGBA");

	g_Steam.ok = g_Steam.friends && g_Steam.utils && g_Steam.GetMediumFriendAvatar &&
				 g_Steam.RequestUserInformation && g_Steam.GetImageSize && g_Steam.GetImageRGBA;

	if (!g_Steam.ok)
		gEngfuncs.Con_Printf("Scoreboard: this steam_api has no avatar functions, no avatars (scoreboard_avatars_debug)\n");

	// the library stays loaded (the engine has it loaded anyway)
	return g_Steam.ok;
}

struct Avatar
{
	unsigned int tex = 0;
	bool requested = false;
	bool failed = false;
	int tries = 0;
	int lastImage = 0; // last answer of GetMediumFriendAvatar (debug)
	float nextTry = 0.0f;
};

static std::unordered_map<uint64, Avatar> g_Avatars;

// texture of the avatar of this steam id (0 = not yet / none)
static unsigned int GetAvatar(uint64 steamID)
{
	if (steamID == 0 || !cl_scoreboard_avatars || cl_scoreboard_avatars->value == 0)
		return 0;

	if (!SteamAvatarsReady())
		return 0;

	Avatar& av = g_Avatars[steamID];
	if (av.tex || av.failed)
		return av.tex;

	const float now = SDL_GetTicks() / 1000.0f;
	if (now < av.nextTry)
		return 0;

	// players that are not friends: Steam has to download their info first
	if (!av.requested)
	{
		av.requested = true;
		g_Steam.RequestUserInformation(g_Steam.friends, steamID, false);
	}

	const int image = g_Steam.GetMediumFriendAvatar(g_Steam.friends, steamID);
	av.lastImage = image;

	if (image > 0)
	{
		uint32 w = 0, h = 0;
		if (g_Steam.GetImageSize(g_Steam.utils, image, &w, &h) && w > 0 && h > 0 && w <= 512 && h <= 512)
		{
			std::vector<uint8> rgba((size_t)w * h * 4);
			if (g_Steam.GetImageRGBA(g_Steam.utils, image, rgba.data(), (int)rgba.size()))
			{
				av.tex = GoModUI_UploadTexture(0, rgba.data(), (int)w, (int)h);
				return av.tex;
			}
		}
		av.failed = true;
		return 0;
	}

	// -1 = still downloading, 0 = no avatar (yet): try again later, not forever
	if (++av.tries > 60)
		av.failed = true;

	av.nextTry = now + 1.0f;
	return 0;
}

// steam id of a player: the engine (if it gives it), the server (message SteamID), or for the
// local player Steam itself (LAN / no message yet)
static uint64 PlayerSteamID(int player)
{
	uint64 id = g_PlayerInfoList[player].m_nSteamID;
	if (id != 0)
		return id;

	id = g_PlayerSteamID[player];
	if (id != 0)
		return id;

	if (0 != g_PlayerInfoList[player].thisplayer && SteamAvatarsReady() && g_Steam.user && g_Steam.GetSteamID)
		return g_Steam.GetSteamID(g_Steam.user);

	return 0;
}

// console: what happens with the avatars (and try them all again)
static void Scoreboard_AvatarsDebug()
{
	g_Steam.tried = false;
	g_Steam.ok = false;
	const bool ok = SteamAvatarsReady();

	gEngfuncs.Con_Printf("---- scoreboard avatars ----\n");
	gEngfuncs.Con_Printf("cl_scoreboard_avatars: %s\n", cl_scoreboard_avatars ? cl_scoreboard_avatars->string : "?");
	gEngfuncs.Con_Printf("steam_api loaded: %s\n", g_Steam.lib ? "yes" : "NO");
	gEngfuncs.Con_Printf("friends: %p (%s)   utils: %p (%s)   user: %p\n", g_Steam.friends, g_Steam.friendsFrom.c_str(),
		g_Steam.utils, g_Steam.utilsFrom.c_str(), g_Steam.user);
	gEngfuncs.Con_Printf("GetMediumFriendAvatar %s, RequestUserInformation %s, GetImageSize %s, GetImageRGBA %s, GetSteamID %s\n",
		g_Steam.GetMediumFriendAvatar ? "ok" : "NO", g_Steam.RequestUserInformation ? "ok" : "NO",
		g_Steam.GetImageSize ? "ok" : "NO", g_Steam.GetImageRGBA ? "ok" : "NO", g_Steam.GetSteamID ? "ok" : "NO");
	gEngfuncs.Con_Printf("ready: %s\n", ok ? "yes" : "NO");

	gHUD.m_Scoreboard.GetAllPlayersInfo();
	for (int i = 1; i < MAX_PLAYERS_HUD; i++)
	{
		if (!g_PlayerInfoList[i].name || !g_PlayerInfoList[i].name[0])
			continue;

		const uint64 id = PlayerSteamID(i);
		auto it = g_Avatars.find(id);
		gEngfuncs.Con_Printf("  %2d %-24s engine id %llu  server id %llu  used id %llu  tex %u  tries %d  last %d\n", i, g_PlayerInfoList[i].name,
			(unsigned long long)g_PlayerInfoList[i].m_nSteamID, (unsigned long long)g_PlayerSteamID[i], (unsigned long long)id,
			it != g_Avatars.end() ? it->second.tex : 0u, it != g_Avatars.end() ? it->second.tries : 0,
			it != g_Avatars.end() ? it->second.lastImage : 0);
	}

	// the ones that failed are tried again
	for (auto& it : g_Avatars)
	{
		it.second.failed = false;
		it.second.tries = 0;
		it.second.requested = false;
		it.second.nextTry = 0.0f;
	}
}

void Scoreboard_FreeAvatars()
{
	for (auto& it : g_Avatars)
		GoModUI_DeleteTexture(it.second.tex);
	g_Avatars.clear();
}

// =====================================================================
//  Drawing (Dear ImGui)
// =====================================================================

// team colors (CTF uses the team id: 1 Black Mesa, 2 Opposing Force)
static const ImU32 kTeamColors[] = {
	IM_COL32(200, 200, 200, 255),
	IM_COL32(255, 160, 0, 255),
	IM_COL32(30, 200, 60, 255),
	IM_COL32(236, 212, 48, 255),
	IM_COL32(68, 199, 42, 255),
};

static ImU32 TeamColor(int index)
{
	if (index < 0)
		index = 0;
	return kTeamColors[index % IM_ARRAYSIZE(kTeamColors)];
}

static bool IsTeamGame()
{
	return gHUD.m_Teamplay == 1 || gHUD.m_Teamplay == 2;
}

// slot of the team of this player in g_TeamInfo (0 = none): it gives the team color
static int TeamSlot(int player)
{
	if (!g_PlayerExtraInfo[player].teamname[0])
		return 0;

	for (int j = 1; j <= gHUD.m_Scoreboard.m_iNumTeams; j++)
	{
		if (!stricmp(g_PlayerExtraInfo[player].teamname, g_TeamInfo[j].name))
			return j;
	}
	return 0;
}

static const char* GameModeTitle()
{
	if (!g_GameModeName.empty())
		return g_GameModeName.c_str();

	switch (giTeamplay)
	{
	case 1: return "Teamplay";
	case 2: return "Capture the Flag";
	case 3: return "Sandbox";
	case 4: return "Snark Frenzy";
	default: return "Deathmatch";
	}
}

static std::string MapName()
{
	const char* level = gEngfuncs.pfnGetLevelName(); // maps/<name>.bsp
	std::string map = level ? level : "";

	const size_t slash = map.find_last_of("/\\");
	if (slash != std::string::npos)
		map = map.substr(slash + 1);

	const size_t dot = map.find_last_of('.');
	if (dot != std::string::npos)
		map = map.substr(0, dot);

	return map;
}

// players sorted: frags (+ captures in CTF) high first, then deaths low first
static void SortedPlayers(const char* team, bool spectatorsOnly, std::vector<int>& out)
{
	out.clear();

	for (int i = 1; i < MAX_PLAYERS_HUD; i++)
	{
		if (!g_PlayerInfoList[i].name || !g_PlayerInfoList[i].name[0])
			continue;

		if (!gEngfuncs.GetEntityByIndex(i))
			continue;

		const bool spectator = 0 != g_IsSpectator[i] || 0 != g_PlayerInfoList[i].spectator;

		if (spectatorsOnly)
		{
			if (!spectator)
				continue;
		}
		else
		{
			if (spectator)
				continue;
			if (team && stricmp(g_PlayerExtraInfo[i].teamname, team))
				continue;
		}

		out.push_back(i);
	}

	const bool ctf = gHUD.m_Teamplay == 2;
	std::stable_sort(out.begin(), out.end(), [ctf](int a, int b)
		{
			const int sa = g_PlayerExtraInfo[a].frags + (ctf ? g_PlayerExtraInfo[a].flagcaptures : 0);
			const int sb = g_PlayerExtraInfo[b].frags + (ctf ? g_PlayerExtraInfo[b].flagcaptures : 0);
			if (sa != sb)
				return sa > sb;
			return g_PlayerExtraInfo[a].deaths < g_PlayerExtraInfo[b].deaths;
		});
}

// square with the first letter of the name (bots, no steam, avatar not loaded yet)
static void DrawLetterAvatar(ImDrawList* dl, const ImVec2& p0, float size, const char* name, ImU32 color)
{
	dl->AddRectFilled(p0, ImVec2(p0.x + size, p0.y + size), color, size * 0.15f);

	char letter[2] = {name && name[0] ? (char)toupper((unsigned char)name[0]) : '?', 0};
	const ImVec2 ts = ImGui::CalcTextSize(letter);
	dl->AddText(ImVec2(p0.x + (size - ts.x) * 0.5f, p0.y + (size - ts.y) * 0.5f), IM_COL32(255, 255, 255, 230), letter);
}

// CTF items: small colored tags after the name
static void DrawPlayerItems(int player)
{
	struct ItemTag
	{
		unsigned int flag;
		const char* text;
		ImU32 color;
	};

	static const ItemTag tags[] = {
		{CTFItem::BlackMesaFlag, "FLAG", IM_COL32(255, 160, 0, 255)},
		{CTFItem::OpposingForceFlag, "FLAG", IM_COL32(30, 200, 60, 255)},
		{CTFItem::LongJump, "LJ", IM_COL32(255, 160, 0, 255)},
		{CTFItem::PortableHEV, "HEV", IM_COL32(128, 160, 255, 255)},
		{CTFItem::Backpack, "BP", IM_COL32(255, 255, 0, 255)},
		{CTFItem::Acceleration, "ACC", IM_COL32(255, 60, 60, 255)},
		{CTFItem::Regeneration, "REG", IM_COL32(0, 255, 0, 255)},
	};

	const unsigned int items = g_PlayerItems[player];
	if (!items)
		return;

	for (const ItemTag& tag : tags)
	{
		if ((items & tag.flag) == 0)
			continue;

		ImGui::SameLine();
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(tag.color), "[%s]", tag.text);
	}
}

static void MutePlayer(int player)
{
	hud_player_info_t* info = &g_PlayerInfoList[player];
	if (!info->name || !info->name[0])
		return;

	char string[512];
	char string1[256];

	if (GetClientVoiceMgr()->IsPlayerBlocked(player))
	{
		GetClientVoiceMgr()->SetPlayerBlockedState(player, false);
		snprintf(string1, sizeof(string1), CHudTextMessage::BufferedLocaliseTextString("#Unmuted"), info->name);
		snprintf(string, sizeof(string), "%c** %s\n", HUD_PRINTTALK, string1);
	}
	else
	{
		GetClientVoiceMgr()->SetPlayerBlockedState(player, true);
		snprintf(string1, sizeof(string1), CHudTextMessage::BufferedLocaliseTextString("#Muted"), info->name);
		snprintf(string, sizeof(string), "%c** %s %s\n", HUD_PRINTTALK, string1, CHudTextMessage::BufferedLocaliseTextString("#No_longer_hear_that_player"));
	}

	gHUD.m_TextMessage.MsgFunc_TextMsg(NULL, strlen(string) + 1, string);
}

enum
{
	COL_NAME = 0,
	COL_SCORE,
	COL_DEATHS,
	COL_CAPTURES, // CTF
	COL_PING,
	COL_LOSS, // cl_showpacketloss
	COL_VOICE,
};

struct ScoreLayout
{
	bool ctf;
	bool loss;
	float avatar;
	float rowH;
	int columns;
};

static void PlayerRow(int player, const ScoreLayout& lay, bool indent)
{
	hud_player_info_t* info = &g_PlayerInfoList[player];
	extra_player_info_t* extra = &g_PlayerExtraInfo[player];

	ImGui::TableNextRow(ImGuiTableRowFlags_None, lay.rowH);

	// background: me (blue), who killed me (red, it fades in 10 seconds)
	const CHudScoreboard& sb = gHUD.m_Scoreboard;
	if (player == sb.m_iLastKilledBy && 0 != sb.m_fLastKillTime && sb.m_fLastKillTime > gHUD.m_flTime)
	{
		const int alpha = (int)(15.0f * (sb.m_fLastKillTime - gHUD.m_flTime) + 20.0f);
		if (0 != info->thisplayer)
			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(80, 155, 0, 90)); // suicide
		else
			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 0, 0, alpha));
	}
	else if (0 != info->thisplayer)
	{
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(40, 90, 255, 70));
	}

	// ---- name (avatar + name + CTF items) ----
	ImGui::TableSetColumnIndex(COL_NAME);
	if (indent)
		ImGui::Indent(lay.avatar * 0.5f);

	{
		const ImVec2 p0 = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();

		const unsigned int tex = GetAvatar(PlayerSteamID(player));
		const ImU32 color = IsTeamGame() ? TeamColor(TeamSlot(player)) : IM_COL32(90, 90, 90, 255);

		if (tex)
			dl->AddImage((ImTextureID)(intptr_t)tex, p0, ImVec2(p0.x + lay.avatar, p0.y + lay.avatar));
		else
			DrawLetterAvatar(dl, p0, lay.avatar, info->name, color);

		ImGui::Dummy(ImVec2(lay.avatar, lay.avatar));
		ImGui::SameLine();

		// name centered with the avatar
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (lay.avatar - ImGui::GetTextLineHeight()) * 0.5f);

		if (IsTeamGame())
			ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color), "%s", info->name);
		else
			ImGui::TextUnformatted(info->name);

		DrawPlayerItems(player);
	}

	if (indent)
		ImGui::Unindent(lay.avatar * 0.5f);

	const float textY = (lay.avatar - ImGui::GetTextLineHeight()) * 0.5f;

	auto Number = [&](int column, int value)
	{
		ImGui::TableSetColumnIndex(column);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textY);
		ImGui::Text("%d", value);
	};

	Number(COL_SCORE, extra->frags);
	Number(COL_DEATHS, extra->deaths);
	if (lay.ctf)
		Number(COL_CAPTURES, extra->flagcaptures);
	Number(COL_PING, info->ping);

	if (lay.loss)
	{
		ImGui::TableSetColumnIndex(COL_LOSS);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textY);
		if (info->packetloss >= 63)
			ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "!!!!");
		else
			ImGui::Text("%d", info->packetloss);
	}

	// ---- voice: talking / muted, and the mute button with the mouse ----
	ImGui::TableSetColumnIndex(COL_VOICE);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textY);
	{
		CVoiceStatus* voice = GetClientVoiceMgr();
		const bool talking = voice->m_VoicePlayers[player - 1] != 0;
		const bool muted = voice->IsPlayerBlocked(player);
		const bool canMute = 0 == info->thisplayer && voice->IsInSquelchMode();

		const char* text = muted ? "MUTED" : (talking ? "TALKING" : "");
		const ImVec4 color = muted ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f) : ImVec4(0.4f, 1.0f, 0.4f, 1.0f);

		if (canMute)
		{
			ImGui::PushID(player);
			if (ImGui::SmallButton(muted ? "Unmute" : "Mute"))
				MutePlayer(player);
			ImGui::PopID();
		}
		else if (text[0])
		{
			ImGui::TextColored(color, "%s", text);
		}
	}
}

void CHudScoreboard::DrawImGui()
{
	GetAllPlayersInfo();

	const float fontSize = (std::max)(13.0f, ScreenHeight * 0.018f);
	ImGui::PushFont(nullptr, fontSize);

	ScoreLayout lay;
	lay.ctf = gHUD.m_Teamplay == 2;
	lay.loss = cl_showpacketloss && cl_showpacketloss->value != 0;
	lay.avatar = (float)(int)(fontSize * 1.6f);
	lay.rowH = lay.avatar + 4.0f;
	lay.columns = COL_VOICE + 1;

	const bool mouse = GetClientVoiceMgr()->IsInSquelchMode();

	const float w = (std::max)(ScreenWidth * 0.55f, (std::min)(720.0f, ScreenWidth - 20.0f));
	const float maxH = ScreenHeight * 0.85f;

	ImGui::SetNextWindowPos(ImVec2(ScreenWidth * 0.5f, ScreenHeight * 0.07f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(w, 0.0f), ImVec2(w, maxH));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
							 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
							 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
	if (!mouse)
		flags |= ImGuiWindowFlags_NoInputs;

	// always the "Go-Mod 3" theme (transparent): the colors are set in hud_imgui.cpp before this,
	// the table ones here
	// scoreboard version of "Go-Mod 3": transparent, but with white texts that can be read
	// over any map
	ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 130));
	ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 255, 255, 45));
	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
	ImGui::PushStyleColor(ImGuiCol_TextDisabled, IM_COL32(215, 215, 215, 255));
	ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, IM_COL32(0, 0, 0, 120));
	ImGui::PushStyleColor(ImGuiCol_TableRowBg, IM_COL32(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, IM_COL32(255, 255, 255, 8));
	ImGui::PushStyleColor(ImGuiCol_TableBorderLight, IM_COL32(255, 255, 255, 30));
	ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, IM_COL32(255, 255, 255, 50));
	ImGui::PushStyleColor(ImGuiCol_Separator, IM_COL32(255, 255, 255, 70));
	ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 150));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 60));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 255, 255, 100));
	ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(0, 0, 0, 60));
	ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(255, 255, 255, 70));

	// space between the columns
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2((float)(int)(fontSize * 0.8f), 3.0f));

	ImGui::Begin("##gomod_scoreboard", nullptr, flags);

	// ---------------- header: game mode, server, map, players ----------------
	{
		ImGui::PushFont(nullptr, fontSize * 1.7f);
		const char* title = GameModeTitle();
		const float tw = ImGui::CalcTextSize(title).x;
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - tw) * 0.5f);
		ImGui::TextUnformatted(title);
		ImGui::PopFont();

		int players = 0;
		for (int i = 1; i < MAX_PLAYERS_HUD; i++)
		{
			if (g_PlayerInfoList[i].name && g_PlayerInfoList[i].name[0])
				players++;
		}

		const char* server = GoModInfo_ServerName();
		char line[256];
		snprintf(line, sizeof(line), "%s   |   %s   |   %d/%d", (server && server[0]) ? server : "Half-Life",
			MapName().c_str(), players, gEngfuncs.GetMaxClients());

		const float lw = ImGui::CalcTextSize(line).x;
		ImGui::SetCursorPosX((std::max)(ImGui::GetStyle().WindowPadding.x, (ImGui::GetWindowWidth() - lw) * 0.5f));
		ImGui::TextDisabled("%s", line);
		ImGui::Separator();
	}

	// ---------------- table ----------------
	const ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY | ImGuiTableFlags_PadOuterX;
	const float tableH = (std::min)(maxH - ImGui::GetCursorPosY() - ImGui::GetStyle().WindowPadding.y * 2.0f - ImGui::GetTextLineHeightWithSpacing(),
		(float)(MAX_PLAYERS_HUD + 8) * lay.rowH);

	if (ImGui::BeginTable("##scores", lay.columns, tableFlags, ImVec2(0.0f, tableH)))
	{
		// every data column as wide as its title or a 5 digit number, with air around
		const float air = (float)(int)(fontSize * 1.2f);
		auto ColumnW = [air](const char* title)
		{
			return (std::max)(ImGui::CalcTextSize(title).x, ImGui::CalcTextSize("00000").x) + air;
		};

		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn(IsTeamGame() ? "Teams" : "Player", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthFixed, ColumnW("Score"));
		ImGui::TableSetupColumn("Deaths", ImGuiTableColumnFlags_WidthFixed, ColumnW("Deaths"));
		ImGui::TableSetupColumn("Captures", lay.ctf ? ImGuiTableColumnFlags_WidthFixed : (ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_Disabled), ColumnW("Captures"));
		ImGui::TableSetupColumn("Ping", ImGuiTableColumnFlags_WidthFixed, ColumnW("Ping"));
		ImGui::TableSetupColumn("Loss", lay.loss ? ImGuiTableColumnFlags_WidthFixed : (ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_Disabled), ColumnW("Loss"));
		ImGui::TableSetupColumn("Voice", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("TALKING").x + air);
		ImGui::TableHeadersRow();

		std::vector<int> players;

		if (IsTeamGame())
		{
			// team totals
			for (int i = 1; i <= m_iNumTeams; i++)
			{
				if (gHUD.m_Teamplay != 2 && !g_TeamInfo[i].scores_overriden)
					g_TeamInfo[i].frags = g_TeamInfo[i].deaths = 0;
				g_TeamInfo[i].ping = g_TeamInfo[i].packetloss = 0;
				g_TeamInfo[i].already_drawn = false;
			}

			for (int i = 1; i < MAX_PLAYERS_HUD; i++)
			{
				if (!g_PlayerInfoList[i].name || !g_PlayerExtraInfo[i].teamname[0])
					continue;

				for (int j = 1; j <= m_iNumTeams; j++)
				{
					if (stricmp(g_PlayerExtraInfo[i].teamname, g_TeamInfo[j].name))
						continue;

					if (gHUD.m_Teamplay != 2 && !g_TeamInfo[j].scores_overriden)
					{
						g_TeamInfo[j].frags += g_PlayerExtraInfo[i].frags;
						g_TeamInfo[j].deaths += g_PlayerExtraInfo[i].deaths;
					}
					g_TeamInfo[j].ping += g_PlayerInfoList[i].ping;
					g_TeamInfo[j].packetloss += g_PlayerInfoList[i].packetloss;
					break;
				}
			}

			// teams, best first
			std::vector<int> teams;
			for (int i = 1; i <= m_iNumTeams; i++)
			{
				if (g_TeamInfo[i].players > 0)
				{
					g_TeamInfo[i].ping /= g_TeamInfo[i].players;
					g_TeamInfo[i].packetloss /= g_TeamInfo[i].players;
					teams.push_back(i);
				}
			}

			std::stable_sort(teams.begin(), teams.end(), [](int a, int b)
				{
					if (g_TeamInfo[a].frags != g_TeamInfo[b].frags)
						return g_TeamInfo[a].frags > g_TeamInfo[b].frags;
					return g_TeamInfo[a].deaths < g_TeamInfo[b].deaths;
				});

			for (int t : teams)
			{
				team_info_t* team = &g_TeamInfo[t];
				const ImU32 color = TeamColor(t);

				ImGui::TableNextRow(ImGuiTableRowFlags_None, lay.rowH);
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, (color & 0x00FFFFFF) | 0x30000000);

				ImGui::TableSetColumnIndex(COL_NAME);
				ImGui::PushFont(nullptr, fontSize * 1.15f);
				ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color), "%s", team->name);
				ImGui::PopFont();
				ImGui::SameLine();
				ImGui::TextDisabled("(%d %s)", team->players, team->players == 1 ? "player" : "players");

				ImGui::TableSetColumnIndex(COL_SCORE);
				ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color), "%d", team->frags);
				ImGui::TableSetColumnIndex(COL_DEATHS);
				ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color), "%d", team->deaths);
				ImGui::TableSetColumnIndex(COL_PING);
				ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color), "%d", team->ping);

				SortedPlayers(team->name, false, players);
				for (int p : players)
					PlayerRow(p, lay, true);
			}

			// players without a team
			std::vector<int> noTeam;
			SortedPlayers(nullptr, false, players);
			for (int p : players)
			{
				if (!g_PlayerExtraInfo[p].teamname[0])
					noTeam.push_back(p);
			}

			if (!noTeam.empty())
			{
				ImGui::TableNextRow(ImGuiTableRowFlags_None, lay.rowH);
				ImGui::TableSetColumnIndex(COL_NAME);
				ImGui::TextDisabled("%s", "No team");
				for (int p : noTeam)
					PlayerRow(p, lay, true);
			}
		}
		else
		{
			SortedPlayers(nullptr, false, players);
			for (int p : players)
				PlayerRow(p, lay, false);
		}

		// spectators
		SortedPlayers(nullptr, true, players);
		if (!players.empty())
		{
			ImGui::TableNextRow(ImGuiTableRowFlags_None, lay.rowH);
			ImGui::TableSetColumnIndex(COL_NAME);
			ImGui::TextDisabled("Spectators (%d)", (int)players.size());
			for (int p : players)
				PlayerRow(p, lay, true);
		}

		ImGui::EndTable();
	}

	// ---------------- footer ----------------
	if (WantsMouseClick())
	{
		const char* hint = mouse ? "Click Mute / Unmute to stop hearing a player" : "Right click to use the mouse (mute players)";
		const float hw = ImGui::CalcTextSize(hint).x;
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - hw) * 0.5f);
		ImGui::TextDisabled("%s", hint);
	}

	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(15);
	ImGui::PopFont();
}
