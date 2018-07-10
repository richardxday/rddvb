
#include <sys/stat.h>
#include <signal.h>

#include <rdlib/Regex.h>
#include <rdlib/Recurse.h>
#include <rdlib/StdSocket.h>

#include "config.h"
#include "dvbmisc.h"

uint64_t JSONTimeOffset(uint64_t dt)
{
	// calculate number of milliseconds between midnight 1-jan-1970 to midnight 1-jan-1980 (1972 and 1976 were leap years)
	static const uint64_t offset = ADateTime::DaysSince1970 * (uint64_t)24 * (uint64_t)3600 * (uint64_t)1000;
	// convert to local time
	return offset + ADateTime(dt).UTCToLocal().operator uint64_t();
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

	if (config.IsOutputDisabled()) cmd1.printf(" >>\"%s\" 2>&1", config.GetLogFile(ADateTime().GetDays()).str());
	else						   cmd1.printf(" 2>&1 | tee -a \"%s\"", config.GetLogFile(ADateTime().GetDays()).str());

	if (config.LogRemoteCommands()) {
		config.logit("Running '%s'", cmd1.str());
	}
	
	bool success = (system(cmd1) == 0);
	if (!success) config.logit("Command '%s' failed", cmd.str());

	return success;
}

bool SendFileToRecordingSlave(const AString& filename)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString cmd;
	bool    success;

	//config.logit("'%s' -> '%s:%s'...", filename.str(), config.GetRecordingSlave().str(), filename.str());
	cmd.printf("scp -p -C -P %u %s \"%s\" %s:\"%s\"", config.GetRecordingSlavePort(), config.GetSCPArgs().str(), filename.str(), config.GetRecordingSlave().str(), filename.str());
	success = RunAndLogCommand(cmd);
	//config.logit("'%s' -> '%s:%s' %s", filename.str(), config.GetRecordingSlave().str(), filename.str(), success ? "success" : "failed");

	return success;
}

bool GetFileFromRecordingSlave(const AString& filename)
{
	return GetFileFromRecordingSlave(filename, filename);
}

bool GetFileFromRecordingSlave(const AString& filename, const AString& localfilename)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString cmd;
	bool    success;

	remove(localfilename);
	//config.logit("'%s:%s' -> '%s'...", config.GetRecordingSlave().str(), filename.str(), localfilename.str());
	cmd.printf("scp -p -C -P %u %s %s:\"%s\" \"%s\"", config.GetRecordingSlavePort(), config.GetSCPArgs().str(), config.GetRecordingSlave().str(), filename.str(), localfilename.str());
	success = RunAndLogCommand(cmd);
	//config.logit("'%s:%s' -> '%s' %s", config.GetRecordingSlave().str(), filename.str(), localfilename.str(), success ? "success" : "failed");

	return success;
}

AString GetRemoteCommand(const AString& cmd, const AString& postcmd, bool compress)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString cmd1;

	cmd1.printf("ssh %s %s -p %u %s \"%s\" %s", compress ? "-C" : "", config.GetSSHArgs().str(), config.GetRecordingSlavePort(), config.GetRecordingSlave().str(), cmd.Escapify().str(), postcmd.str());

	return cmd1;
}

bool RunAndLogRemoteCommand(const AString& cmd, const AString& postcmd, bool compress)
{
	return RunAndLogCommand(GetRemoteCommand(cmd, postcmd, compress));
}

bool SendFileRunRemoteCommand(const AString& filename, const AString& cmd)
{
	return (SendFileToRecordingSlave(filename) && RunAndLogRemoteCommand(cmd));
}

bool RunRemoteCommandGetFile(const AString& cmd, const AString& filename)
{
	return (RunAndLogRemoteCommand(cmd) && GetFileFromRecordingSlave(filename));
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

bool CopyFile(AStdData& fp1, AStdData& fp2)
{
	static uint8_t buffer[65536];
	const ADVBConfig& config = ADVBConfig::Get();
	slong_t sl = -1, dl = -1;

	while ((sl = fp1.readbytes(buffer, sizeof(buffer))) > 0) {
		if ((dl = fp2.writebytes(buffer, sl)) != sl) {
			config.logit("Failed to write %ld bytes (%ld written)", sl, dl);
			break;
		}
	}

	return (sl <= 0);
}

bool CopyFile(const AString& src, const AString& dst, bool binary)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AStdFile fp1, fp2;
	bool success = false;

	if (SameFile(src, dst)) {
		config.printf("File copy: '%s' and '%s' are the same file", src.str(), dst.str());
		success = true;
	}
	else if (fp1.open(src, binary ? "rb" : "r")) {
		if (fp2.open(dst, binary ? "wb" : "w")) {
			success = CopyFile(fp1, fp2);
			if (!success) config.logit("Failed to copy '%s' to '%s': transfer failed", src.str(), dst.str());
		}
		else config.logit("Failed to copy '%s' to '%s': failed to open '%s' for writing", src.str(), dst.str(), dst.str());
	}
	else config.logit("Failed to copy '%s' to '%s': failed to open '%s' for reading", src.str(), dst.str(), src.str());

	return success;
}

bool MoveFile(const AString& src, const AString& dst, bool binary)
{
	const ADVBConfig& config = ADVBConfig::Get();
	bool success = false;

	if (SameFile(src, dst)) {
		config.printf("File move: '%s' and '%s' are the same file", src.str(), dst.str());
		success = true;
	}
	else if	(rename(src, dst) == 0) success = true;
	else if (CopyFile(src, dst, binary)) {
		if (remove(src) == 0) success = true;
		else config.logit("Failed to remove '%s' after copy", src.str());
	}

	return success;
}

