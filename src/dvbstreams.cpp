
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dvbstreams.h"
#include "channellist.h"
#include "proglist.h"
#include "dvbprog.h"
#include "rdlib/Recurse.h"

bool ListDVBStreams(const AString& pattern, std::vector<dvbstream_t>& activestreams)
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString tempfile = config.GetTempFile("streams", ".txt");
    AString cmd      = config.GetStreamListingCommand(pattern, tempfile);
    bool success = false;

    if (system(cmd) == 0) {
        AStdFile fp;

        if (fp.open(tempfile)) {
            AString line;

            while (line.ReadLn(fp) >= 0) {
                dvbstream_t stream = {
                    (uint_t)line.Word(0),
                    line.Words(1),
                };

                activestreams.push_back(stream);
            }

            fp.close();

            success = true;
        }
    }

    remove(tempfile);

    return success;
}

static bool PrepareHLSStreaming(const AString& name)
{
    const ADVBConfig& config   = ADVBConfig::Get();
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

bool StartDVBStream(dvbstreamtype_t type, const AString& name, const AString& dvbcardstr)
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString cmd, pipecmd;
    uint_t dvbcard = (uint_t)dvbcardstr;
    bool dvbcardspecified = dvbcardstr.Valid();
    bool success = false;

    ADVBConfig::GetWriteable(true);

    switch (type) {
        default:
        case StreamType_Raw:
            break;

        case StreamType_HLS:
            PrepareHLSStreaming(name);
            pipecmd.printf("| %s", config.ReplaceHLSTerms(config.GetHLSEncoderCommand(), name).str());
            break;

        case StreamType_MP4:
            pipecmd.printf("| %s", config.GetStreamEncoderCommand().str());
            break;

        case StreamType_Video:
            pipecmd.printf("| %s", config.GetVideoPlayerCommand().str());
            break;
    }

    if (config.GetStreamSlave().Valid()) {
        cmd.printf("dvb %s --stream \"%s\"", dvbcardspecified ? AString("--dvbcard %").Arg(dvbcard).str() : "", name.str());

        cmd = GetRemoteCommand(cmd, pipecmd, false, true);
    }
    else {
        ADVBChannelList& channellist = ADVBChannelList::Get();
        AString pids;

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
                FindActiveStreamingProcesses(procs);

                /// convert list into map
                for (size_t j = 0; j < procs.size(); j++) {
                    const auto& proc = procs[j];
                    virtualcardsinuse[proc.vcard] = true;
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
                        cmd = ADVBProg::GenerateStreamCommand(best.card, (uint_t)std::min(maxtime.GetAbsoluteSecond(), (uint64_t)0xffffffff), pids);

                        if (!config.IsRecordingSlave()) {
                            cmd += " " + pipecmd;
                        }
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

                    cmd.printf("cat \"%s\"", filename.str());

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
    }

    if (cmd.Valid()) {
        if (config.LogRemoteCommands()) {
            config.logit("Running command '%s'", cmd.str());
        }

        int res = system(cmd);
        (void)res;

        if (type == StreamType_HLS) {
            res = system(config.ReplaceHLSTerms(config.GetHLSCleanCommand(), name));
            (void)res;
        }

        success = true;
    }

    return success;
}

bool StopDVBStreams(const AString& pattern, std::vector<dvbstream_t>& stoppedstreams)
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString tempfile = config.GetTempFile("streams", ".txt");
    AString cmd      = config.GetStreamListingCommand(pattern, tempfile);
    bool success = false;

    if (system(cmd) == 0) {
        AStdFile fp;

        if (fp.open(tempfile)) {
            AString line;

            while (line.ReadLn(fp) >= 0) {
                dvbstream_t stream = {
                    (uint32_t)line.Word(0),
                    line.Words(1),
                };
                AString cmd2;

                cmd2.printf("bash -c 'kill -SIGINT %u'", stream.pid);
                if (system(cmd2) == 0) {
                    stoppedstreams.push_back(stream);
                }
            }

            fp.close();

            success = true;
        }
    }

    remove(tempfile);

    return success;
}
