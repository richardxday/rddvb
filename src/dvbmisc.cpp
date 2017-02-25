
#include <sys/stat.h>
#include <signal.h>

#include <rdlib/Regex.h>
#include <rdlib/Recurse.h>
#include <rdlib/StdSocket.h>

#include "config.h"
#include "dvbmisc.h"

AString JSONFormat(const AString& str)
{
	return str.SearchAndReplace("/", "\\/").SearchAndReplace("\"", "\\\"").SearchAndReplace("\n", "\\n");
}

AString JSONTime(uint64_t dt)
{
	// calculate number of milliseconds between midnight 1-jan-1970 to midnight 1-jan-1980 (1972 and 1976 were leap years)
	static const uint64_t offset = ADateTime::DaysSince1970 * 24 * 3600 * 1000;
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

bool RunAndLogCommand(const AString& cmd)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString cmd1 = cmd;

	if (config.IsWebResponse()) cmd1.printf(" >>\"%s\" 2>&1", config.GetLogFile(ADateTime().GetDays()).str());
	else						cmd1.printf(" 2>&1 | tee -a \"%s\"", config.GetLogFile(ADateTime().GetDays()).str());

	//config.printf("Running '%s'", cmd1.str());
	bool success = (system(cmd1) == 0);
	if (!success) config.logit("Command '%s' failed", cmd.str());

	return success;
}

bool SendFileToRecordingHost(const AString& filename)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString cmd;
	bool    success;

	//config.logit("'%s' -> '%s:%s'...", filename.str(), config.GetRecordingHost().str(), filename.str());
	cmd.printf("scp -p -C %s \"%s\" %s:\"%s\"", config.GetSCPArgs().str(), filename.str(), config.GetRecordingHost().str(), filename.str());
	success = RunAndLogCommand(cmd);
	//config.logit("'%s' -> '%s:%s' %s", filename.str(), config.GetRecordingHost().str(), filename.str(), success ? "success" : "failed");

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
	bool    success;

	remove(localfilename);
	//config.logit("'%s:%s' -> '%s'...", config.GetRecordingHost().str(), filename.str(), localfilename.str());
	cmd.printf("scp -p -C %s %s:\"%s\" \"%s\"", config.GetSCPArgs().str(), config.GetRecordingHost().str(), filename.str(), localfilename.str());
	success = RunAndLogCommand(cmd);
	//config.logit("'%s:%s' -> '%s' %s", config.GetRecordingHost().str(), filename.str(), localfilename.str(), success ? "success" : "failed");

	return success;
}

bool RunRemoteCommand(const AString& cmd)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString cmd1;

	cmd1.printf("ssh -C %s %s \"%s\"", config.GetSSHArgs().str(), config.GetRecordingHost().str(), cmd.Escapify().str());

	return RunAndLogCommand(cmd1);
}

bool SendFileRunRemoteCommand(const AString& filename, const AString& cmd)
{
	return (SendFileToRecordingHost(filename) && RunRemoteCommand(cmd));
}

bool RunRemoteCommandGetFile(const AString& cmd, const AString& filename)
{
	return (RunRemoteCommand(cmd) && GetFileFromRecordingHost(filename));
}

bool TriggerServerCommand(const AString& cmd)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString host = config.GetServerHost();
	bool success = false;

	if (host.Valid()) {
		static ASocketServer server;
		static AStdSocket    socket(server);

		if (!socket.isopen()) {
			if (socket.open("0.0.0.0", 0, ASocketServer::Type_Datagram)) {
				uint_t port = config.GetServerPort();

				if (!socket.setdatagramdestination(host, port)) {
					config.printf("Failed to set %s:%u as UDP destination for server command triggers", host.str(), port);
				}
			}
			else config.printf("Failed to open UDP socket for server command triggers");
		}

		success = (socket.printf("%s\n", cmd.str()) > 0);
	}

	return success;
}

bool SameFile(const AString& file1, const AString& file2)
{
	struct stat stat1, stat2;

	return ((file1 == file2) ||
			((lstat(file1, &stat1) == 0) &&
			 (lstat(file2, &stat2) == 0) &&
			 (stat1.st_dev == stat2.st_dev) &&
			 (stat1.st_ino == stat2.st_ino)));
}
