
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <regex>

#include "dvbmisc.h"
#include "dvbstreams.h"
#include "channellist.h"
#include "proglist.h"
#include "dvbprog.h"
#include "rdlib/Recurse.h"
#include "rdlib/Regex.h"
#include "rdlib/misc.h"
#include "rdlib/strsup.h"

AString ConvertStream(const dvbstream_t& stream)
{
    AString str;

    str.printf("type %s\n", stream.type.str());
    str.printf("cmd %s\n", stream.cmd.str());
    str.printf("name %s\n", stream.name.str());
    str.printf("url %s\n", stream.url.str());
    str.printf("htmlfile %s\n", stream.htmlfile.str());
    str.printf("hlsfile %s\n", stream.hlsfile.str());

    return str.Base64Encode();
}

bool ConvertStream(const AString& base64str, dvbstream_t& stream)
{
    AString str = base64str.Base64Decode();
    bool success = false;

    if (str.Valid()) {
        sint_t i, n = str.CountLines();

        for (i = 0; i < n; i++) {
            AString line = str.Line(i);
            AString cmd  = line.Word(0);
            AString args = line.Words(1);

            if (cmd == "type") {
                stream.type = args;
            }
            else if (cmd == "cmd") {
                stream.cmd = args;
            }
            else if (cmd == "name") {
                stream.name = args;
            }
            else if (cmd == "url") {
                stream.url = args;
            }
            else if (cmd == "htmlfile") {
                stream.htmlfile = args;
            }
            else if (cmd == "hlsfile") {
                stream.hlsfile = args;
            }
        }

        success = true;
    }

    return success;
}

bool ListDVBStreams(std::vector<dvbstream_t>& activestreams, const AString& pattern)
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString tempfile = config.GetTempFile("streams", ".txt");
    AString cmd      = config.GetStreamListingCommand(tempfile);
    AString pat      = ParseRegex(pattern);
    bool success = false;

    if (system(cmd) == 0) {
        AStdFile fp;

        if (fp.open(tempfile)) {
            AString line;

            while (line.ReadLn(fp) >= 0) {
                uint32_t pid = (uint32_t)line.Word(0);
                AString  str = line.Words(1);
                dvbstream_t stream;

                if (ConvertStream(str, stream) && MatchRegex(stream.name, pat)) {
                    size_t i;

                    stream.pid = pid;

                    for (i = 0; i < activestreams.size(); i++) {
                        dvbstream_t& stream1 = activestreams[i];

                        if ((stream.type == stream1.type) &&
                            (stream.name == stream1.name) &&
                            (stream.url  == stream1.url)) {
                            activestreams[i] = stream;
                            break;
                        }
                    }

                    if (i == activestreams.size()) {
                        activestreams.push_back(stream);
                    }
                }
            }

            fp.close();

            success = true;
        }
    }

    remove(tempfile);

    return success;
}

static bool PrepareHLSStreaming(dvbstream_t& stream)
{
    const ADVBConfig& config   = ADVBConfig::Get();
    const AString&    name     = stream.name;
    const AString     dir      = config.GetHLSConfigItem("hlsoutputpath", name);
    const AString     srcfile  = config.GetHLSConfigItem("hlsstreamhtmlsourcefile", name);
    const AString     destfile = config.GetHLSConfigItem("hlsstreamhtmldestfile", name);
    bool success = false;

    if (CreateDirectory(dir)) {
        AStdFile ifp, ofp;

        if (ifp.open(srcfile)) {
            if (ofp.open(destfile, "w")) {
                AString line;

                while (line.ReadLn(ifp) >= 0) {
                    line = config.ReplaceHLSTerms(config.ReplaceTerms(line), name).SearchAndReplace("{hlsdir}", dir);
                    ofp.printf("%s\n", line.str());
                }

                ofp.close();

                stream.type       = "hls";
                stream.htmlfile   = destfile;
                stream.hlsfile    = config.GetHLSConfigItem("hlsoutputfullpath", name);
                stream.url        = config.GetHLSConfigItem("hlsstreamurl", name);
                stream.cleanupcmd = config.ReplaceHLSTerms(config.GetHLSCleanCommand(), name);

                success = true;
            }
            else {
                fprintf(stderr, "Failed to open file '%s' for writing\n", destfile.str());
            }

            ifp.close();
        }
        else {
            fprintf(stderr, "Failed to open file '%s' for reading\n", srcfile.str());
        }
    }
    else {
        fprintf(stderr, "Failed to create directory '%s'\n", dir.str());
    }

    return success;
}

bool StartDVBStream(dvbstream_t& stream, dvbstreamtype_t type, const AString& _name, const AString& dvbcardstr, bool background)
{
    const ADVBConfig& config      = ADVBConfig::Get();
    const bool   useslave         = config.GetStreamSlave().Valid();
    const uint_t dvbcard          = (uint_t)dvbcardstr;
    const bool   dvbcardspecified = dvbcardstr.Valid();
    AString      name             = _name;
    bool success = false;

    // ensure output is disabled
    ADVBConfig::GetWriteable().DisableOutput();

    stream.pid = 0U;
    stream.type.Delete();
    stream.cmd.Delete();
    stream.name.Delete();
    stream.url.Delete();
    stream.htmlfile.Delete();
    stream.hlsfile.Delete();
    stream.cleanupcmd.Delete();

    if ((type == StreamType_Raw) && !useslave) {
        ADVBChannelList& channellist = ADVBChannelList::Get();
        AString pids;

        stream.type = "raw";
        stream.name = name;

        if (channellist.GetPIDList(0U, name, pids, false)) {
            ADVBProgList list;

            if (list.ReadFromFile(config.GetScheduledFile())) {
                std::vector<ADVBProgList::TIMEGAP> gaps;
                std::vector<dvbstreamprocs_t> procs;
                std::map<uint_t,bool> virtualcardsinuse;
                ADVBProgList::TIMEGAP best;
                ADateTime maxtime;

                list.ReadFromFile(config.GetRecordingFile());
                list.Sort();

                // get list of active dvbstream processes
                if (FindActiveStreamingProcesses(procs)) {
                    // convert list into map
                    for (size_t j = 0; j < procs.size(); j++) {
                        const auto& proc = procs[j];
                        virtualcardsinuse[proc.vcard] = true;
                    }
                }

                best = list.FindGaps(ADateTime().TimeStamp(true), gaps, &virtualcardsinuse);
                if (dvbcardspecified) {
                    // ignore the best gap, use the one specified by dvbcard
                    best = gaps[dvbcard];
                }
                maxtime = (uint64_t)best.end - (uint64_t)best.start;

                if ((best.card < config.GetMaxDVBCards()) && (maxtime.GetAbsoluteSecond() > 10U)) {
                    maxtime -= 10U * 1000U;

                    fprintf(stderr, "Max stream time is %s (%ss), using card %u\n", maxtime.SpanStr().str(), AValue(maxtime.GetAbsoluteSecond()).ToString().str(), best.card);

                    if (pids.Empty()) {
                        channellist.GetPIDList(best.card, name, pids, true);
                    }

                    if (pids.Valid()) {
                        stream.cmd = ADVBProg::GenerateStreamCommand(best.card, (uint_t)std::min(maxtime.GetAbsoluteSecond(), (uint64_t)0xffffffff), pids);
                    }
                    else fprintf(stderr, "Failed to find PIDs for channel '%s'\n", name.str());
                }
                else fprintf(stderr, "No DVB cards available\n");
            }
            else {
                fprintf(stderr, "Failed to read scheduled programmes file\n");
            }
        }
        else {
            ADVBProgList list, list1;
            AString errors;

            list.ReadFromFile(config.GetRecordedFile());
            list.ReadFromFile(config.GetRecordingFile());

            //fprintf(stderr, "Search pattern '%s'\n", text.str());
            list.FindProgrammes(list1, name, errors);

            if (errors.Valid()) {
                fprintf(stderr, "Errors found in pattern: %s\n", errors.str());
            }
            else if (list1.Count() > 0) {
                AString filename;
                uint_t i;

                for (i = list1.Count(); (i > 0) && filename.Empty(); ) {
                    const ADVBProg& prog = list1[--i];

                    if (AStdFile::exists(prog.GetFilename())) {
                        filename = prog.GetFilename();
                    }
                    else if (AStdFile::exists(prog.GetArchiveRecordingFilename())) {
                        filename = prog.GetArchiveRecordingFilename();
                    }
                    else if (AStdFile::exists(prog.GetTempRecordingFilename())) {
                        filename = prog.GetTempRecordingFilename();
                    }
                    else if (AStdFile::exists(prog.GetTempFilename())) {
                        filename = prog.GetTempFilename();
                    }
                }

                if (filename.Valid()) {
                    const ADVBProg& prog = list1[i];

                    stream.cmd.printf("nice 10 cat \"%s\"", filename.str());

                    fprintf(stderr, "Streaming '%s'\n", prog.GetDescription(1).str());
                }
                else {
                    fprintf(stderr, "Failed to find a programme in running list (%u programmes long) matching '%s'\n", list.Count(), name.str());
                }
            }
            else {
                fprintf(stderr, "No programmes found with pattern\n");
            }
        }

        if (stream.cmd.Valid()) {
            AString cmd = stream.cmd;

            if (background) {
                if (stream.cleanupcmd.Valid()) {
                    cmd.printf(" ; %s", stream.cleanupcmd.str());
                    stream.cleanupcmd.Delete();
                }
                cmd = AString::Formatify("bash -c '%s' 2>/dev/null >/dev/null &", cmd.str());
            }

            success = (system(cmd) == 0);

            if (stream.cleanupcmd.Valid()) {
                int res = system(stream.cleanupcmd);
                (void)res;
            }
        }
    }
    else {
        AString cmd, cardstr, args, pipecmd;
        int p;

        if ((p = name.Pos(";")) >= 0) {
            args = name.Mid(p + 1);
            name = name.Left(p);
        }

        stream.name = name;

        switch (type) {
            default:
            case StreamType_Raw:
                stream.type = "raw";
                break;

            case StreamType_HLS:
                stream.type = "hls";
                PrepareHLSStreaming(stream);
                pipecmd.printf("| %s", config.ReplaceHLSTerms(config.GetHLSEncoderCommand(), name).str());
                //pipecmd.printf(" ; %s", stream.cleanupcmd.str());
                break;

            case StreamType_MP4:
                stream.type = "mp4";
                pipecmd.printf("| %s", config.GetStreamEncoderCommand().str());
                break;

            case StreamType_RemoteHTTP:
            case StreamType_HTTP:
                stream.type = "http";
                stream.url  = config.GetHTTPStreamURL(args);
                pipecmd.printf("| %s", config.GetHTTPStreamCommand(args).str());
                break;

            case StreamType_Video:
                stream.type = "video";
                pipecmd.printf("| %s", config.GetVideoPlayerCommand().str());
                break;
        }

        if (dvbcardspecified) {
            cardstr.printf("--dvbcard %u", dvbcard);
        }

        stream.cmd.printf("dvb %s --rawstream \"%s\"", cardstr.str(), name.str());
        if (type == StreamType_RemoteHTTP) {
            stream.cmd += " " + pipecmd;
            pipecmd.Delete();
        }

        cmd.printf("dvb ---stream \"%s\"", ConvertStream(stream).str());
        if (useslave) {
            cmd = GetRemoteCommand(cmd, pipecmd, true, true);
        }
        else if (pipecmd.Valid()) {
            cmd += " " + pipecmd;
        }

        if (background) {
            if (stream.cleanupcmd.Valid()) {
                cmd.printf(" ; %s", stream.cleanupcmd.str());
                stream.cleanupcmd.Delete();
            }
            cmd = AString::Formatify("bash -c '%s' 2>/dev/null >/dev/null &", cmd.str());
        }

        if (config.LogRemoteCommands()) {
            config.logit("Running command '%s'", cmd.str());
        }

        //fprintf(stderr, "Cmd: %s (%s)\n", cmd.str(), stream.cmd.str());
        success = (system(cmd) == 0);

        if (stream.cleanupcmd.Valid()) {
            int res = system(stream.cleanupcmd);
            (void)res;
        }
    }

    return success;
}

static bool KillStream(const dvbstream_t& stream)
{
    const ADVBConfig& config = ADVBConfig::Get();
    const uint32_t pid = stream.pid;
    const AString cmd = config.GetStreamListingKillingCommand(pid);

    return (system(cmd) == 0);
}

bool StopDVBStream(const AString& name, std::vector<dvbstream_t>& stoppedstreams)
{
    std::vector<dvbstream_t> streams;
    bool success = false;

    if (ListDVBStreams(streams)) {
        for (size_t i = 0; i < streams.size(); i++) {
            const auto& stream = streams[i];

            if (CompareNoCase(stream.name, name) == 0) {
                if (KillStream(stream)) {
                    stoppedstreams.push_back(stream);
                    success = true;
                }
                else {
                    fprintf(stderr, "Failed to stop stream '%s' (pid %u)\n", stream.name.str(), stream.pid);
                    success = false;
                    break;
                }
            }
        }
    }

    return success;
}

bool StopDVBStreams(const AString& pattern, std::vector<dvbstream_t>& stoppedstreams)
{
    std::vector<dvbstream_t> streams;
    bool success = false;

    if (ListDVBStreams(streams, pattern)) {
        success = true;

        for (size_t i = 0; i < streams.size(); i++) {
            const auto& stream = streams[i];

            if (KillStream(stream)) {
                stoppedstreams.push_back(stream);
                success = true;
            }
            else {
                fprintf(stderr, "Failed to stop stream '%s' (pid %u)\n", stream.name.str(), stream.pid);
                success = false;
                break;
            }
        }
    }

    return success;
}
