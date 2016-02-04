
#include <signal.h>

#include <rdlib/Regex.h>
#include <rdlib/Recurse.h>

#include "dvbmisc.h"

AString JSONFormat(const AString& str)
{
	return str.SearchAndReplace("/", "\\/").SearchAndReplace("\"", "\\\"").SearchAndReplace("\n", "\\n");
}

ulong_t JSONTime(uint64_t dt)
{
	// calculate number of milliseconds between midnight 1-jan-1970 to midnight 1-jan-1980 (1972 and 1976 were leap years)
	static const uint64_t offset = ADateTime::DaysSince1970 * 24ULL * 3600ULL * 1000ULL;
	return (ulong_t)(dt + offset);
}

AString CatPath(const AString& dir1, const AString& dir2)
{
	if (dir2[0] != '/') return dir1.CatPath(dir2);

	return dir2;
}

AString ReplaceStrings(const AString& str, const REPLACEMENT *replacements, uint_t n)
{
	AString res = str;
	uint_t i;

	for (i = 0; i < n; i++) {
		res = res.SearchAndReplaceNoCase(replacements[i].search, replacements[i].replace);
	}

	return res;
}
