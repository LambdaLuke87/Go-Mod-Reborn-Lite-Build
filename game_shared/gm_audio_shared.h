#pragma once

// Shared by the client (cl_dll) and the server (dlls).
// Rules for the sound paths that travel in the "GMSound" user message.

#include <cctype>
#include <cstring>
#include <string>

enum
{
	GM_SND_STOP = 1 // stop whatever this entity is playing on this channel
};

// Valid: letters, numbers, '_' '-' '/' '.', no "..", no leading '/', max 100 chars, must end in ".wav".
// Both "zombie/zo_idle1.wav" and "sound/zombie/zo_idle1.wav" are accepted.
// The server doesn't trust itself and the client doesn't trust the server: both call this.
inline bool GMAudio_IsValidSoundPath(const char* path)
{
	if (!path)
		return false;

	const size_t len = strlen(path);

	if (len < 5 || len > 100 || path[0] == '/' || strstr(path, "..") != nullptr)
		return false;

	for (size_t i = 0; i < len; ++i)
	{
		const unsigned char c = (unsigned char)path[i];
		const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
						c == '_' || c == '-' || c == '/' || c == '.';
		if (!ok)
			return false;
	}

	const char* ext = path + len - 4;
	return ext[0] == '.' && tolower((unsigned char)ext[1]) == 'w' &&
		   tolower((unsigned char)ext[2]) == 'a' && tolower((unsigned char)ext[3]) == 'v';
}

// Path relative to "sound/" (that's what travels in the message: fewer bytes)
inline const char* GMAudio_RelativePath(const char* path)
{
	return strncmp(path, "sound/", 6) == 0 ? path + 6 : path;
}

// Path to open in the filesystem
inline std::string GMAudio_FullPath(const char* path)
{
	return std::string("sound/") + GMAudio_RelativePath(path);
}
