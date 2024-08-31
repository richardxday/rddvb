
#include <sys/stat.h>
#include <signal.h>

#include <rdlib/Regex.h>
#include <rdlib/Recurse.h>
#include <rdlib/StdSocket.h>

#include "channellist.h"
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

AString ReplaceStrings(const AString& str, const replacement_t *replacements, uint_t n)
{
    auto   res = str;
    uint_t i;

    for (i = 0; i < n; i++) {
        res = res.SearchAndReplaceNoCase(replacements[i].search, replacements[i].replace);
    }

    return res;
}

bool RunAndLogCommand(const AString& cmd)
{
    const auto& config  = ADVBConfig::Get();
    const auto  logfile = config.GetLogFile(ADateTime().GetDays());
    auto  cmd1 = cmd;

    if (config.IsOutputDisabled()) cmd1.printf(" >>\"%s\" 2>&1", logfile.str());
    else                           cmd1.printf(" 2>&1 | tee -a \"%s\"", logfile.str());

    if (config.LogRemoteCommands()) {
        config.logit("Running '%s'", cmd1.str());
    }

    auto success = (system(cmd1) == 0);
    if (!success) config.logit("Command '%s' failed", cmd.str());

    return success;
}

uint_t GetRemotePort(bool streamslave)
{
    const auto& config = ADVBConfig::Get();
    return streamslave ? config.GetStreamSlavePort() : config.GetRecordingSlavePort();
}

AString GetRemoteHostID(bool streamslave)
{
    const auto& config = ADVBConfig::Get();
    const auto  host = streamslave ? config.GetStreamSlave()     : config.GetRecordingSlave();
    const auto  user = streamslave ? config.GetStreamSlaveUser() : config.GetRecordingSlaveUser();

    return AString::Formatify("%s%s%s", user.str(), user.Valid() ? "@" : "", host.str());
}

bool SendFileToRecordingSlave(const AString& filename)
{
    const auto& config = ADVBConfig::Get();
    const auto  args = config.GetSCPArgs();
    AString cmd;
    bool    success;

    cmd.printf("scp -p -C -P %u %s \"%s\" %s:\"%s\"", GetRemotePort(), args.str(), filename.str(), GetRemoteHostID().str(), filename.str());
    success = RunAndLogCommand(cmd);

    return success;
}

bool GetFileFromRecordingSlave(const AString& filename)
{
    return GetFileFromRecordingSlave(filename, filename);
}

bool GetFileFromRecordingSlave(const AString& filename, const AString& localfilename)
{
    const auto& config = ADVBConfig::Get();
    const auto  args   = config.GetSCPArgs();
    AString cmd;
    bool    success;

    remove(localfilename);

    cmd.printf("scp -p -C -P %u %s %s:\"%s\" \"%s\"", GetRemotePort(), args.str(), GetRemoteHostID().str(), filename.str(), localfilename.str());
    success = RunAndLogCommand(cmd);

    return success;
}

AString GetRemoteCommand(const AString& cmd, const AString& postcmd, bool compress, bool streamslave)
{
    const auto& config = ADVBConfig::Get();
    AString cmd1;

    cmd1.printf("ssh %s %s -p %u %s \"%s\" %s", compress ? "-C" : "", config.GetSSHArgs().str(), GetRemotePort(streamslave), GetRemoteHostID(streamslave).str(), cmd.Escapify().str(), postcmd.str());

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
    const auto& config = ADVBConfig::Get();
    auto host = config.GetServerHost();
    bool success = false;

    if (host.Valid()) {
        static ASocketServer server;
        static AStdSocket    socket(server);

        if (!socket.isopen()) {
            if (socket.open("0.0.0.0", 0, ASocketServer::Type_Datagram)) {
                auto port = config.GetServerPort();

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
    const auto& config = ADVBConfig::Get();
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
    const auto& config = ADVBConfig::Get();
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
    const auto& config = ADVBConfig::Get();
    bool success = false;

    if (SameFile(src, dst)) {
        config.printf("File move: '%s' and '%s' are the same file", src.str(), dst.str());
        success = true;
    }
    else if (rename(src, dst) == 0) success = true;
    else if (CopyFile(src, dst, binary)) {
        if (remove(src) == 0) success = true;
        else config.logit("Failed to remove '%s' after copy", src.str());
    }

    return success;
}

AString SanitizeString(const AString& str, bool filesystem, bool dir)
{
    AString res;

    if (filesystem) {
        AStringUpdate updater(&res);
        sint_t i;

        for (i = 0; i < str.len(); i++) {
            auto c = str[i];

            if      (IsSymbolChar(c) || (c == '.') || (c == '-') || (dir && (c == ' '))) updater.Update(c);
            else if ((c == '/') || IsWhiteSpace(c)) updater.Update('_');
        }
    }
    else res = str;

    return res;
}

bool FindActiveStreamingProcesses(std::vector<dvbstreamprocs_t>& procs)
{
    const auto& config = ADVBConfig::Get();
    auto tempfile = config.GetTempFile("streams", ".txt");
    AString cmd;
    bool success = false;

    cmd.printf("bash -c 'pgrep -a dvbstream >\"%s\"'", tempfile.str());
    if (system(cmd) == 0) {
        AStdFile fp;

        if (fp.open(tempfile)) {
            static const AString cardid   = "-c";
            static const AString freqid   = "-f";
            static const AString timeid   = "-n";
            static const AString outputid = "-o";
            AString line;

            while (line.ReadLn(fp) >= 0) {
                dvbstreamprocs_t proc;
                int i, n = line.CountWords();

                proc.pid   = 0;
                proc.pcard = 0;
                proc.vcard = 0;
                proc.freq  = 0;
                proc.time  = 0;

                for (i = 0; i < n; i++) {
                    auto word = line.Word(i);

                    if (i == 0) {
                        proc.pid = (uint32_t)line.Word(i);
                    }
                    else if (i >= 2) {
                        if (word == cardid) {
                            proc.pcard = (uint_t)line.Word(++i);
                            proc.vcard = config.GetVirtualDVBCard(proc.pcard);
                        }
                        else if (word == freqid) {
                            proc.freq = (uint32_t)line.Word(++i);
                        }
                        else if (word == timeid) {
                            proc.time = (uint32_t)line.Word(++i);
                        }
                        else if (word[0] != '-') {
                            proc.pids.push_back((uint_t)word);
                        }
                    }
                }

#if 0
                fprintf(stderr, "PID %u: card %u/%u freq %uHz time %us", proc.pid, proc.pcard, proc.vcard, proc.freq, proc.time);
                for (size_t j = 0; j < proc.pids.size(); j++) {
                    if (j == 0) {
                        fprintf(stderr, " pids: ");
                    }
                    else {
                        fprintf(stderr, ", ");
                    }
                    fprintf(stderr, "%u", proc.pids[j]);
                }
                fprintf(stderr, "\n");
#endif

                procs.push_back(proc);
            }

            fp.close();

            success = true;
        }
        else {
            fprintf(stderr, "Failed to open list of dvbstream processes (file '%s')\n", tempfile.str());
        }
    }

    remove(tempfile);

    return success;
}

AString RunCommandAndGetResult(const AString& cmd)
{
    const auto& config = ADVBConfig::Get();
    const auto  tempfile = config.GetTempFile("command", ".txt");
    AString res;

    if (system(cmd + " >" + tempfile) == 0) {
        res.ReadFromFile(tempfile);
    }

    remove(tempfile);

    return res;
}

AString GetCommandFromPID(uint32_t pid)
{
    const auto commands = RunCommandAndGetResult("pgrep -a .+");
    AString res;
    int i, n = commands.CountLines();

    for (i = 0; i < n; i++) {
        const auto cmd = commands.Line(i);

        if ((uint32_t)cmd.Word(0) == pid) {
            res = cmd.Words(1);
            break;
        }
    }

    return res;
}

AString FindChildCommandsFromPID(uint32_t pid)
{
    return RunCommandAndGetResult(AString::Formatify("pgrep -a -P %u", pid));
}

APIDTree::APIDTree(uint32_t _pid) : pid(_pid),
                                    cmd(GetCommandFromPID(pid))
{
    FindChildren();
}

APIDTree::APIDTree(uint32_t _pid, const AString& _cmd) : pid(_pid),
                                                         cmd(_cmd)
{
    FindChildren();
}

APIDTree::APIDTree(const AString& description)
{
    int ln = 0;
    Populate(description, ln);
}

APIDTree::APIDTree(const AString& description, int& ln)
{
    Populate(description, ln);
}

APIDTree::~APIDTree()
{
    for (size_t i = 0; i < children.size(); i++) {
        delete children[i];
    }
}

void APIDTree::Populate(const AString& description, int& ln)
{
    auto line = description.Line(ln++);
    int indent = line.Pos(line.Word(0));

    pid = (uint32_t)line.Word(0);
    cmd = line.Words(1);

    while (((line = description.Line(ln)).Valid()) && (line.Pos(line.Word(0)) > indent)) {
        children.push_back(new APIDTree(description, ln));
    }
}

void APIDTree::FindChildren()
{
    auto childcmds = FindChildCommandsFromPID(pid);
    int i, n = childcmds.CountLines();

    for (i = 0; i < n; i++) {
        auto str = childcmds.Line(i);

        children.push_back(new APIDTree((uint32_t)str.Word(0), str.Words(1)));
    }
}

bool APIDTree::Kill() const
{
    bool success = false;

    if (Valid()) {
        if (!children.empty()) {
            for (size_t i = 0; i < children.size(); i++) {
                success = (success || children[children.size() - 1 - i]->Kill());
            }
        }

        AString _cmd;
        _cmd.printf("kill -9 %u", pid);
        success = (success || (system(_cmd) == 0));
    }

    return success;
}

AString APIDTree::DescribeEx(uint_t level) const
{
    auto    str = AString("  ").Copies((int)level);
    AString res;

    res.printf("%s%u %s\n", str.str(), pid, cmd.str());

    for (size_t i = 0; i < children.size(); i++) {
        res += children[i]->DescribeEx(level + 1);
    }

    return res;
}

uint32_t TestCard(uint_t card, uint32_t freq, const AString& pidlist, uint_t seconds)
{
    const auto& config  = ADVBConfig::Get();
    const auto  logfile = config.GetLogFile(ADateTime().GetDays());
    AString cmd;

#if OLD_STYLE_TEST_CARDS
    cmd.printf("%s -c %u -f %u %s -n %u -o 2>>\"%s\" | wc -c", config.GetDVBStreamCommand().str(), card, freq, pidlist.str(), seconds, logfile.str());
#else
    cmd.printf("%s -c %u -f %u %s -n %u -o 2>>\"%s\" | %s",
               config.GetDVBStreamCommand().str(), card, freq, pidlist.str(), seconds, logfile.str(),
               config.GetVideoErrorCheckCommand().SearchAndReplace("{filename}", "-").str());
#endif

    return (uint32_t)RunCommandAndGetResult(cmd);
}

uint32_t TestCard(uint_t card, const AString& channel, uint_t seconds)
{
    const auto& channels = ADVBChannelList::Get();
    const auto *pchannel = channels.GetChannelByName(channel);

    return (pchannel != NULL) ? TestCard(card, pchannel, seconds) : 0;
}
