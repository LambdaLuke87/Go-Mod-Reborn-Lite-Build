#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "filesystem_utils.h"
#include "custom_packs.h"

#include <set>

static const char* const PACKS_ROOT = "scripts";

static bool IsAllowedNameChar(unsigned char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

bool CustomPacks_IsValidName(const std::string& name)
{
	if (name.empty())
		return false;

	for (unsigned char c : name)
	{
		if (!IsAllowedNameChar(c))
			return false;
	}

	return true;
}

bool CustomPacks_IsSafeRelativePath(const std::string& path)
{
	if (path.empty() || path[0] == '/')
		return false;

	if (path.find("..") != std::string::npos)
		return false;

	for (unsigned char c : path)
	{
		if (!IsAllowedNameChar(c) && c != '/' && c != '.')
			return false;
	}

	return true;
}

// Some filesystems return the bare name, others may include a path: keep only the last part.
static std::string StripPath(const char* name)
{
	std::string s = name;
	size_t pos = s.find_last_of("/\\");
	return pos == std::string::npos ? s : s.substr(pos + 1);
}

void CustomPacks_GetPackNames(std::vector<std::string>& out)
{
	out.clear();

	if (!g_pFileSystem)
	{
		ALERT(at_console, "CustomPacks: filesystem not available, cannot look for packs in %s/\n", PACKS_ROOT);
		return;
	}

	// The same folder is returned once per search path that contains it, so use a set:
	// it removes the duplicates and keeps the names sorted at the same time.
	std::set<std::string> found;

	char pattern[64];
	snprintf(pattern, sizeof(pattern), "%s/*", PACKS_ROOT);

	FileFindHandle_t handle;
	const char* name = g_pFileSystem->FindFirst(pattern, &handle, nullptr);

	// The handle is only valid if FindFirst found something, so no FindClose when it didn't.
	if (!name)
		return;

	for (; name; name = g_pFileSystem->FindNext(handle))
	{
		if (!g_pFileSystem->FindIsDirectory(handle))
			continue;

		const std::string folder = StripPath(name);

		if (folder == "." || folder == "..")
			continue;

		if (!CustomPacks_IsValidName(folder))
		{
			ALERT(at_console, "CustomPacks: folder '%s' ignored, only letters, numbers, '_' and '-' are allowed in pack names\n", folder.c_str());
			continue;
		}

		found.insert(folder);
	}

	g_pFileSystem->FindClose(handle);

	out.assign(found.begin(), found.end());
}

std::string CustomPacks_BuildPath(const std::string& pack, const std::string& relative)
{
	return std::string(PACKS_ROOT) + "/" + pack + "/" + relative;
}
