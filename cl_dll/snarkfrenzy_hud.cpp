// Go-Mod: Reborn - Snark Frenzy HUD (mp_gamemode 3, UTIL_IsSnarkFrenzy())
//
// Message "SnarkFrenzy" (dlls/snarkfrenzy_gamerules.cpp, SendState):
//   byte  state     0 waiting, 1 countdown, 2 playing, 3 round end
//   byte  carrier   player index with the snark
//   short remaining 1/10 seconds (countdown / fuse / round end)
//   byte  winner    player that won the round
//   byte  flags     1 = countdown at the start of a round
//   byte  winscore  rounds to win the map

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "gm_audio.h"

#include <cmath>
#include <cstdio>
#include <cstring>

extern int giR, giG, giB;
extern bool UTIL_IsSnarkFrenzy();

enum
{
	SFS_WAITING = 0,
	SFS_COUNTDOWN,
	SFS_PLAYING,
	SFS_ROUNDEND,
};

static int s_iState = SFS_WAITING;
static int s_iCarrier = 0;
static int s_iWinner = 0;
static int s_iFlags = 0;
static int s_iWinScore = 5;
static float s_flEndTime = 0.0f; // client time when the countdown / fuse reaches 0
static int s_iLastBeep = -1;
static float s_flDashReady = 0.0f; // client time when the dash is ready again

static const char* PlayerName(int index)
{
	static hud_player_info_t info;
	memset(&info, 0, sizeof(info));

	if (index < 1 || index > MAX_PLAYERS)
		return "";

	gEngfuncs.pfnGetPlayerInfo(index, &info);
	return info.name ? info.name : "";
}

static int LocalIndex()
{
	cl_entity_t* local = gEngfuncs.GetLocalPlayer();
	return local ? local->index : 0;
}

static int __MsgFunc_SnarkFrenzy(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	s_iState = READ_BYTE();
	s_iCarrier = READ_BYTE();
	const float remaining = READ_SHORT() / 10.0f;
	s_iWinner = READ_BYTE();
	s_iFlags = READ_BYTE();
	s_iWinScore = READ_BYTE();

	s_flEndTime = gHUD.m_flTime + remaining;
	s_iLastBeep = -1;

	// new carrier: the dash is ready
	if (s_iState != SFS_PLAYING || s_iCarrier != LocalIndex())
		s_flDashReady = 0.0f;

	return 1;
}

static int __MsgFunc_SFDash(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	s_flDashReady = gHUD.m_flTime + READ_BYTE() / 10.0f;
	return 1;
}

void SnarkFrenzyHUD_Init()
{
	gEngfuncs.pfnHookUserMsg("SnarkFrenzy", __MsgFunc_SnarkFrenzy);
	gEngfuncs.pfnHookUserMsg("SFDash", __MsgFunc_SFDash);
}

void SnarkFrenzyHUD_Reset()
{
	s_iState = SFS_WAITING;
	s_iCarrier = 0;
	s_iWinner = 0;
	s_iFlags = 0;
	s_flEndTime = 0.0f;
	s_iLastBeep = -1;
	s_flDashReady = 0.0f;
}

// text centered at y, color 0-255
static void DrawCenteredText(int y, const char* text, int r, int g, int b)
{
	const int width = ConsoleStringLen(text);
	gEngfuncs.pfnDrawSetTextColor(r / 255.0f, g / 255.0f, b / 255.0f);
	DrawConsoleString((ScreenWidth - width) / 2, y, text);
}

// big HUD number centered at y, returns its height
static int DrawCenteredNumber(int y, int number, int r, int g, int b)
{
	const Rect& rc = gHUD.GetSpriteRect(gHUD.m_HUD_number_0);
	const int digitWidth = rc.right - rc.left;
	const int digitHeight = rc.bottom - rc.top;

	const int digits = gHUD.GetNumWidth(number, DHN_DRAWZERO);
	const int x = (ScreenWidth - digits * digitWidth) / 2;

	gHUD.DrawHudNumber(x, y, DHN_DRAWZERO, number, r, g, b);
	return digitHeight;
}

void SnarkFrenzyHUD_Draw(float flTime)
{
	if (!UTIL_IsSnarkFrenzy())
		return;

	if (gHUD.m_iIntermission)
		return;

	const int lineHeight = gHUD.m_iFontHeight > 0 ? gHUD.m_iFontHeight : 16;
	float remaining = s_flEndTime - flTime;
	if (remaining < 0.0f)
		remaining = 0.0f;

	const int seconds = (int)ceilf(remaining);
	const bool iAmCarrier = s_iCarrier > 0 && s_iCarrier == LocalIndex();

	char text[256];

	switch (s_iState)
	{
	case SFS_WAITING:
		DrawCenteredText(ScreenHeight / 8, "SNARK FRENZY - waiting for more players (2 needed)", giR, giG, giB);
		break;

	case SFS_COUNTDOWN:
	{
		const int y = ScreenHeight / 3;

		if (s_iFlags & 1)
			snprintf(text, sizeof(text), "SNARK FRENZY - the round starts in");
		else
			snprintf(text, sizeof(text), "Choosing who gets the snark in");

		DrawCenteredText(y - lineHeight - 4, text, giR, giG, giB);
		const int h = DrawCenteredNumber(y, seconds, giR, giG, giB);

		snprintf(text, sizeof(text), "First to %d rounds wins", s_iWinScore);
		DrawCenteredText(y + h + 6, text, 180, 180, 180);

		// beep every second
		if (seconds != s_iLastBeep && seconds > 0 && seconds <= 5)
			GMAudio_PlayLocal("buttons/blip1.wav", 0.6f, 100);
		s_iLastBeep = seconds;
		break;
	}

	case SFS_PLAYING:
	{
		// fuse: red when it's about to blow
		const bool hurry = remaining <= 5.0f;
		const int r = hurry ? 255 : giR;
		const int g = hurry ? 40 : giG;
		const int b = hurry ? 40 : giB;

		const int y = lineHeight * 2;
		snprintf(text, sizeof(text), "SNARK: %.1f", remaining);
		DrawCenteredText(y, text, r, g, b);

		if (iAmCarrier)
		{
			// blinking warning
			if (!hurry || ((int)(flTime * 4.0f) & 1) == 0)
				DrawCenteredText(y + lineHeight + 4, "YOU HAVE THE SNARK! Throw it at someone!", 255, 60, 60);

			// dash (secondary attack)
			const float dashLeft = s_flDashReady - flTime;
			if (dashLeft <= 0.0f)
				DrawCenteredText(y + (lineHeight + 4) * 2, "DASH READY (secondary attack)", 80, 255, 80);
			else
			{
				snprintf(text, sizeof(text), "Dash: %.1f", dashLeft);
				DrawCenteredText(y + (lineHeight + 4) * 2, text, 160, 160, 160);
			}

			// tick every second in the last 5
			if (hurry && seconds != s_iLastBeep && seconds > 0)
				GMAudio_PlayLocal("buttons/blip2.wav", 0.8f, 100 + (5 - seconds) * 10);
			s_iLastBeep = seconds;
		}
		else if (s_iCarrier > 0)
		{
			snprintf(text, sizeof(text), "%s has the snark", PlayerName(s_iCarrier));
			DrawCenteredText(y + lineHeight + 4, text, giR, giG, giB);
		}
		break;
	}

	case SFS_ROUNDEND:
		if (s_iWinner > 0)
			snprintf(text, sizeof(text), "%s survived and wins the round!", PlayerName(s_iWinner));
		else
			snprintf(text, sizeof(text), "Nobody survived the round");

		DrawCenteredText(ScreenHeight / 3, text, giR, giG, giB);
		break;
	}
}
