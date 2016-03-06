
#include <signal.h>

#include <rdlib/Regex.h>
#include <rdlib/Recurse.h>

#include "config.h"
#include "dvbmisc.h"

AString JSONFormat(const AString& str)
{
	return str.SearchAndReplace("/", "\\/").SearchAndReplace("\"", "\\\"").SearchAndReplace("\n", "\\n");
}

AString JSONTime(uint64_t dt)
{
	// calculate number of milliseconds between midnight 1-jan-1970 to midnight 1-jan-1980 (1972 and 1976 were leap years)
	static const uint64_t offset = ADateTime::DaysSince1970 * 24ULL * 3600ULL * 1000ULL;
	return AValue(dt + offset).ToString();
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

bool SendFileToRecordingHost(const AString& filename)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString cmd;

	cmd.printf("scp -C %s \"%s\" %s:\"%s\"", config.GetSCPArgs().str(), filename.str(), config.GetRecordingHost().str(), filename.str());

	bool success = (system(cmd) == 0);
	if (!success) config.logit("Command '%s' failed", cmd.str());

	return success;
}

bool GetFileFromRecordingHost(const AString& filename)
{
	return GetFileFromRecordingHost(filename, filename);
}

bool GetFileFromRecordingHost(const AString& filename, const AString& localfilename)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString cmd;

	cmd.printf("scp -C %s %s:\"%s\" \"%s\"", config.GetSCPArgs().str(), config.GetRecordingHost().str(), filename.str(), localfilename.str());

	bool success = (system(cmd) == 0);
	if (!success) config.logit("Command '%s' failed", cmd.str());

	return success;
}

bool RunRemoteCommand(const AString& cmd)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString cmd1;

	cmd1.printf("ssh -C %s %s \"%s\"", config.GetSSHArgs().str(), config.GetRecordingHost().str(), cmd.Escapify().str());

	bool success = (system(cmd1) == 0);
	if (!success) config.logit("Command '%s' failed", cmd1.str());

	return success;
}

bool SendFileRunRemoteCommand(const AString& filename, const AString& cmd)
{
	return (SendFileToRecordingHost(filename) && RunRemoteCommand(cmd));
}

bool RunRemoteCommandGetFile(const AString& cmd, const AString& filename)
{
	return (RunRemoteCommand(cmd) && GetFileFromRecordingHost(filename));
}
