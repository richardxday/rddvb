
#include "json/forwards.h"
#include "json/reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/statvfs.h>
#include <unistd.h>

#include <algorithm>

#include <rdlib/Regex.h>
#include <rdlib/Recurse.h>
#include <rdlib/XMLDecode.h>
#include <json/json.h>

#include "config.h"
#include "proglist.h"
#include "channellist.h"
#include "episodehandler.h"
#include "dvblock.h"
#include "dvbpatterns.h"
#include "iconcache.h"
#include "rdlib/strsup.h"

/*--------------------------------------------------------------------------------*/

uint_t ADVBProgList::writefiledepth = 0;

ADVBProgList::ADVBProgList() : useproghash(false)
{
    proglist.SetDestructor(&DeleteProg);
    proglist.EnableDuplication(true);
}

ADVBProgList::ADVBProgList(const ADVBProgList& list) : useproghash(false)
{
    proglist.SetDestructor(&DeleteProg);
    proglist.EnableDuplication(true);

    operator = (list);
}

ADVBProgList::~ADVBProgList()
{
}

void ADVBProgList::DeleteAll()
{
    proghash.Delete();
    proglist.DeleteList();
}

ADVBProgList& ADVBProgList::operator = (const ADVBProgList& list)
{
    uint_t i;

    DeleteAll();

    for (i = 0; (i < list.Count()) && !HasQuit(); i++) {
        const ADVBProg& prog = list[i];

        AddProg(prog, false, false);
    }

    return *this;
}

void ADVBProgList::Modify(const ADVBProgList& list, uint_t& added, uint_t& modified, uint_t mode, bool sort)
{
    uint_t i;

    if (!proghash.GetItems()) CreateHash();

    added = modified = 0;

    for (i = 0; (i < list.Count()) && !HasQuit(); i++) {
        uint_t res = ModifyProg(list[i], mode, sort);
        if (res & Prog_Add)    added++;
        if (res & Prog_Modify) modified++;
    }
}

bool ADVBProgList::ModifyFromRecordingSlave(const AString& filename, uint_t mode, bool sort)
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString host;
    bool    success = false;

    DeleteAll();

    if ((host = config.GetRecordingSlave()).Valid()) {
        AString dstfilename = config.GetTempFile("proglist", ".dat");

        if (GetFileFromRecordingSlave(filename, dstfilename)) {
            ADVBLock     lock("dvbfiles");
            ADVBProgList list;

            if (ReadFromFile(filename)) {
                if (list.ReadFromFile(dstfilename)) {
                    uint_t added, modified;

                    config.logit("List (from '%s') is %u programmes long, modifying list (from '%s') is %u programmes long",
                                 filename.str(), Count(),
                                 dstfilename.str(), list.Count());

                    Modify(list, added, modified, mode, sort);

                    if (added || modified) {
                        if (WriteToFile(filename)) {
                            if (added || modified) config.printf("Modified programmes in '%s' from recording host, total now %u (%u added, %u modified)", filename.str(), Count(), added, modified);
                            success = true;
                        }
                        else config.printf("Failed to write programme list back!");
                    }
                    else {
                        //config.printf("No programmes added");
                        success = true;
                    }
                }
                else config.printf("Failed to read programme list from '%s'", dstfilename.str());
            }
            else config.printf("Failed to read programmes list from '%s'", filename.str());
        }
        else config.printf("Failed to get '%s' from recording host", filename.str());

        remove(dstfilename);
    }
    else config.printf("No remote host configured!");

    return success;
}

int ADVBProgList::SortProgs(uptr_t item1, uptr_t item2, void *pContext)
{
    return ADVBProg::Compare((const ADVBProg *)item1, (const ADVBProg *)item2, (const bool *)pContext);
}

int ADVBProgList::SortProgsAdvanced(uptr_t item1, uptr_t item2, void *pContext)
{
    return ADVBProg::Compare((const ADVBProg *)item1, (const ADVBProg *)item2, *(const ADVBProg::FIELDLIST *)pContext);
}

void ADVBProgList::AddXMLTVChannel(const AStructuredNode& channel)
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString channelid = channel.GetAttribute("id");
    std::vector<AString> displaynames;
    const AStructuredNode *node = NULL;
    AString xmltvchannelname;

    // collect all entries of display-name
    while ((node = channel.FindChild("display-name", node)) != NULL) {
        displaynames.push_back(node->Value);
    }

    if (channelid.Valid() && (displaynames.size() > 0) && ((xmltvchannelname = displaynames[0]).Valid())) {
        ADVBChannelList& channellist = ADVBChannelList::Get();
        const AStructuredNode *iconnode;
        AString icon;
        uint_t lcn = 0;

        // channel number is represented as a n-digit displayname (why??) in the last display name
        // attempt to decode channel number
        if ((displaynames.size() >= 2) &&
            (sscanf(displaynames[displaynames.size() - 1].str(), "%u", &lcn) < 1)) {
            config.logit("Failed to decode channel number from '%s'", displaynames[displaynames.size() - 1].str());
        }

        if ((iconnode = channel.FindChild("icon")) != NULL) icon = iconnode->GetAttribute("src");

        channellist.AssignOrAddXMLTVChannel(lcn, xmltvchannelname, channelid);

        ADVBIconCache::Get().SetIcon("channel", xmltvchannelname, icon);
    }
}

void ADVBProgList::AssignEpisodes(bool ignorerepeats)
{
    ADVBEpisodeHandler handler;
    uint_t i;

    for (i = 0; i < Count(); i++) {
        ADVBProg& prog = GetProgWritable(i);

        handler.AssignEpisode(prog, ignorerepeats);
    }
}

void ADVBProgList::GetProgrammeValues(AString& str, const AStructuredNode *pNode, const AString& prefix) const
{
    const AKeyValuePair *pAttr;

    while (pNode) {
        AString key   = prefix + pNode->Key.SearchAndReplace("-", "");
        AString value = pNode->Value;

        if ((key == "episodenum") && ((pAttr = pNode->FindAttribute("system")) != NULL)) key += ":" + pAttr->Value;
        if ((key == "icon")       && ((pAttr = pNode->FindAttribute("src"))    != NULL)) value = pAttr->Value;
        if (key == "rating")                                                             value = pNode->GetChildValue("value");

        if (value.Valid() || !pNode->GetChildren()) str.printf("%s=%s\n", key.str(), value.str());

        if (pNode->GetChildren()) {
            if (key == "video") GetProgrammeValues(str, pNode->GetChildren(), key);
            else                GetProgrammeValues(str, pNode->GetChildren());
        }

        pNode = pNode->Next();
    }
}

bool ADVBProgList::ReadFromXMLTVFile(const AString& filename)
{
    const ADVBConfig& config = ADVBConfig::Get();
    std::map<AString, bool> channelidvalidhash;
    AString   data;
    FILE_INFO info;
    bool      success = false;

    if (data.ReadFromFile(filename)) {
        const AString channelstr   = "<channel ";
        const AString programmestr = "<programme ";
        AString   line, channel, programme;
        ADateTime now;
        uint_t len = data.len(), nrejected = 0;
        int  p = 0, p1 = 0;

        success = true;

        do {
            if ((p1 = data.Pos("\n", p)) < 0) p1 = len;

            line.Create(data.str() + p, p1 - p, false);
            line = line.Words(0);

            if (channel.Valid()) {
                channel += line;
                if (channel.PosNoCase("</channel>") >= 0) {
                    const AStructuredNode *pNode;
                    AStructuredNode _node;

                    if (DecodeXML(_node, channel) && ((pNode = _node.GetChildren()) != NULL)) {
                        AddXMLTVChannel(*pNode);
                    }
                    //else config.printf("Failed to decode channel data ('%s')", channel.str());

                    channel.Delete();
                }
            }
            else if (programme.Valid()) {
                programme += line;
                if (programme.PosNoCase("</programme>") >= 0) {
                    const AStructuredNode *pNode;
                    AStructuredNode _node;

                    if (DecodeXML(_node, programme) && ((pNode = _node.GetChildren()) != NULL)) {
                        const ADVBChannelList& channellist = ADVBChannelList::Get();
                        AString channelid = pNode->GetAttribute("channel");
                        AString channel   = channellist.LookupXMLTVChannel(channelid);
                        const ADVBChannelList::CHANNEL *chandata;

                        channelidvalidhash[channelid] = true;

                        if ((chandata = channellist.GetChannelByXMLTVChannelName(channelid)) != NULL) {
                            ADateTime start, stop;
                            AString   str;

                            start.StrToDate(pNode->GetAttribute("start"), ADateTime::Time_Absolute);
                            stop.StrToDate(pNode->GetAttribute("stop"), ADateTime::Time_Absolute);

#if 0
                            {
                                static AStdFile fp;
                                if (!fp.isopen()) fp.open("dates.txt", "w");
                                if (fp.isopen()) {
                                    fp.printf("Start %s -> %s\n", programme.GetField(" start=\"", "\"").str(), start.DateFormat("%Y-%M-%D %h:%m:%s.%S").str());
                                    fp.printf("Stop  %s -> %s\n", programme.GetField(" stop=\"", "\"").str(), stop.DateFormat("%Y-%M-%D %h:%m:%s.%S").str());
                                }
                            }
#endif

                            str.printf("\n");
                            str.printf("start=%s\n", ADVBProg::GetHex(start).str());
                            str.printf("stop=%s\n",  ADVBProg::GetHex(stop).str());

                            str.printf("channelid=%s\n", channelid.str());
                            str.printf("channel=%s\n", chandata->xmltv.convertedchannelname.str());
                            str.printf("dvbchannel=%s\n", chandata->dvb.channelname.str());

                            GetProgrammeValues(str, pNode->GetChildren());

                            //debug("%s", str.str());

                            {
                                ADVBProg prog = str;

                                if (!prog.Valid()) config.logit("Ignoring invalid programme");
                                else if (stop <= start) {
                                    config.logit("Ignoring zero length programme '%s'", prog.GetQuickDescription().str());
                                }
                                else if (stricmp(prog.GetTitle(), "to be announced") == 0) {
                                    // ignore programmes with the above title
                                }
                                else if (AddProg(prog, true, true) < 0) {
                                    config.printf("Failed to add prog!");
                                    success = false;
                                }
                            }
                        }
                        else {
                            nrejected++;
                            //config.printf("Failed to find channel data for channel ID '%s'", channelid.str());
                        }
                    }
                    else config.printf("Failed to decode programme data ('%s')", programme.str());

                    programme.Delete();
                }
            }
            else if (CompareNoCaseN(line, channelstr, channelstr.len()) == 0) {
                channel = line;
            }
            else if (CompareNoCaseN(line, programmestr, programmestr.len()) == 0) {
                programme = line;
            }

            p = p1 + 1;

            if (HasQuit()) break;
        } while (p1 < (int)len);

        if (nrejected) config.printf("%u programmes rejected during read", nrejected);
    }

    return success;
}

bool ADVBProgList::ReadFromTextFile(const AString& filename)
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString data;
    bool    notempty = (Count() != 0);
    bool    success = false;

    if (data.ReadFromFile(filename)) {
        AString str;
        int p = 0, p1;

        //config.printf("Read data from '%s', parsing...", filename.str());

        success = true;

        while ((p1 = data.Pos("\n\n", p)) >= 0) {
            str.Create(data.str() + p, p1 + 1 - p, false);

            if ((str.CountWords() > 0) && (AddProg(str, notempty, notempty) < 0)) {
                config.printf("Failed to add prog");
                success = false;
            }

            p = p1 + 1;

            if (HasQuit()) break;
        }
    }

    return success;
}

bool ADVBProgList::ReadFromBinaryFile(const AString& filename, bool sort, bool removeoverlaps)
{
    const ADVBConfig& config = ADVBConfig::Get();
    AStdFile fp;
    bool success = false;

    if (fp.open(filename, "rb")) {
        ADVBProg prog;

        fp.seek(0, SEEK_END);
        sint32_t size = fp.tell(), pos = 0;
        fp.rewind();

        success = true;

        while (success && (pos < size)) {
            if ((prog = fp).Valid()) {
                if (AddProg(prog, sort, removeoverlaps) < 0) {
                    config.printf("Failed to add prog!");
                    success = false;
                }
            }

            pos = fp.tell();

            if (HasQuit()) break;
        }

        //config.printf("Read data from '%s', parsing complete", filename.str());
    }

    return success;
}

bool ADVBProgList::ReadFromJSONFile(const AString& filename)
{
    //const ADVBConfig& config = ADVBConfig::Get();
    AString data;
    //bool    notempty = (Count() != 0);
    bool    success = false;

    if (data.ReadFromFile(filename)) {
        std::string errors;
        Json::CharReaderBuilder rbuilder;
        Json::CharReader *reader = rbuilder.newCharReader();
        Json::Value obj;

        if ((reader != NULL) && reader->parse(data.str(), data.str() + data.len(), &obj, &errors)) {
            if (obj.isMember("Channels") && obj["Channels"].isArray()) {
                const Json::Value& channels = obj["Channels"];
                uint_t i, nchannels = (uint_t)channels.size();

                for (i = 0; i < nchannels; i++) {
                    const Json::Value& channel = channels[i];

                    if (channel.isMember("DisplayName")) {
                        printf("Channel %u/%u: '%s'\n", i, (uint_t)channels.size(), channel["DisplayName"].asString().c_str());
                    }
                    if (channel.isMember("TvListings") && channel["TvListings"].isArray()) {
                        const Json::Value& tvlistings = channel["TvListings"];
                        uint_t j, ntvlistings = (uint_t)tvlistings.size();

                        for (j = 0; j < ntvlistings; j++) {
                            const Json::Value& programme = tvlistings[j];

                            (void)programme;
                        }
                    }
                }

                success = true;
            }
        }

        if (reader != NULL) {
            delete reader;
        }
    }

    return success;
}

bool ADVBProgList::ReadFromFile(const AString& filename)
{
    const ADVBConfig& config = ADVBConfig::Get();
    AStdFile fp;
    bool notempty = (Count() != 0);
    bool success  = false;

    //config.printf("Reading data from '%s'", filename.str());

    if (filename.Suffix() == "xmltv") {
        success = ReadFromXMLTVFile(filename);
    }
    else if (filename.Suffix() == "txt") {
        success = ReadFromTextFile(filename);
    }
    else if (filename.Suffix() == "json") {
        success = ReadFromJSONFile(filename);
    }
    else if (filename.Suffix() == "dat") {
        success = ReadFromBinaryFile(filename, notempty, notempty);
    }
    else {
        config.printf("Unrecognized suffix '%s' for file '%s'", filename.Suffix().str(), filename.str());
    }

    return success;
}

bool ADVBProgList::ReadFromJobQueue(int queue, bool runningonly)
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString listname   = config.GetTempFile("atlist", ".txt");
    AString scriptname = config.GetTempFile("atjob",  ".txt");
    AString cmd1, cmd2;
    bool success = false;

    remove(listname);

    cmd1.printf("atq -q = >%s", listname.str());
    if (!runningonly) cmd2.printf("atq -q %c >>%s", queue, listname.str());

    if ((system(cmd1) == 0) && (runningonly || (system(cmd2) == 0))) {
        AStdFile fp;

        if (fp.open(listname)) {
            AString line, cmd;

            success = true;
            while (line.ReadLn(fp) >= 0) {
                uint_t jobid = (uint_t)line.Word(0);

                if (jobid) {
                    cmd.Delete();
                    cmd.printf("at -c %u >%s", jobid, scriptname.str());
                    if (system(cmd) == 0) {
                        ADVBProg prog;

                        if (prog.ReadFromJob(scriptname)) {
                            prog.SetJobID(jobid);

                            if (queue == '=') prog.SetRunning();

                            if (AddProg(prog) < 0) {
                                config.logit("Failed to decode and add programme from job %u", jobid);
                                success = false;
                            }
                        }
                        else config.logit("Failed to read job %u", jobid);
                    }
                }

                if (HasQuit()) break;
            }

            fp.close();
        }
    }
    else config.logit("Failed to read job list");

    remove(listname);
    remove(scriptname);

    return success;
}

bool ADVBProgList::ReadFromJobList(bool runningonly)
{
    const ADVBConfig& config = ADVBConfig::Get();
    return ReadFromJobQueue(config.GetQueue().FirstChar(), runningonly);
}

bool ADVBProgList::CheckFile(const AString& filename, const AString& targetfilename, const FILE_INFO& fileinfo)
{
    FILE_INFO fileinfo2;
    return ((filename == targetfilename) || (::GetFileInfo(targetfilename, &fileinfo2) && (fileinfo2.WriteTime > fileinfo.WriteTime)));
}

void ADVBProgList::UpdateDVBChannels(std::map<uint_t, bool> *sdchannelids)
{
    const ADVBConfig&      config = ADVBConfig::Get();
    const ADVBChannelList& clist  = ADVBChannelList::Get();
    AHash  hash;
    uint_t i;

    for (i = 0; i < Count(); i++) {
        ADVBProg& prog     = GetProgWritable(i);
        AString channel    = prog.GetChannel();
        AString dvbchannel = clist.LookupDVBChannel(channel);

        if (dvbchannel.Valid()) {
            prog.SetDVBChannel(dvbchannel);

            if (sdchannelids) {
                uint_t id;

                // format is 'I101023.json.schedulesdirect.org'
                if (sscanf(prog.GetChannelID(), "I%u.json.schedulesdirect.org", &id) > 0) {
                    (*sdchannelids)[id] = true;
                }
            }
        }
        else if (!hash.Exists(channel)) {
            config.logit("Unable to find DVB channel for '%s' ('%s')", channel.str(), prog.GetChannelID());
            hash.Insert(channel, 0);
        }
    }
}

bool ADVBProgList::WriteToFile(const AString& filename, bool updatedependantfiles) const
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString  backupfilename = filename + ".bak";
    AStdFile fp;
    bool success = false;

    // keep track of 'depth' of file write so that file system sync is done at the *end*
    // of the last write
    writefiledepth++;

    config.printf("Writing '%s'", filename.str());

    if (filename.Suffix() == "txt") {
        success = WriteToTextFile(filename);
    }
    else if (filename.Suffix() == "dat") {
        remove(backupfilename);
        rename(filename, backupfilename);

        if (fp.open(filename, "wb")) {
            uint_t i;

            success = true;

            for (i = 0; i < Count(); i++) {
                const ADVBProg& prog = GetProg(i);

                if (!prog.WriteToFile(fp)) {
                    config.printf("Failed to write programme %u/%u to file '%s'", i, Count(), filename.str());
                    success = false;
                    break;
                }

                if (HasQuit()) break;
            }

            fp.close();
        }
    }

    if (success && updatedependantfiles && config.EnableCombined() && (filename != config.GetCombinedFile())) {
        FILE_INFO fileinfo;

        if (!::GetFileInfo(config.GetCombinedFile(),            &fileinfo) ||
            CheckFile(filename, config.GetRecordedFile(),       fileinfo)  ||
            CheckFile(filename, config.GetScheduledFile(),      fileinfo)  ||
            CheckFile(filename, config.GetRecordingFile(),      fileinfo)  ||
            CheckFile(filename, config.GetRejectedFile(),       fileinfo)  ||
            CheckFile(filename, config.GetProcessingFile(),     fileinfo)) {
            success &= CreateCombinedFile();
            CreateGraphs();
        }
        //else config.printf("No need to update combined file");
    }

    if ((--writefiledepth) == 0) {
        // this is the end of the last write, so
        // sync file system now to minimise chance of losing data
        sync();
    }

    return success;
}

bool ADVBProgList::WriteToTextFile(const AString& filename) const
{
    AString  backupfilename = filename + ".bak";
    AStdFile fp;
    bool success = false;

    remove(backupfilename);
    rename(filename, backupfilename);

    if (fp.open(filename, "w")) {
        uint_t i;

        for (i = 0; i < Count(); i++) {
            fp.printf("%s", GetProg(i).ExportToText().str());

            if (HasQuit()) break;
        }

        fp.printf("\n");
        fp.close();

        success = true;
    }

    return success;
}

ADVBProgList& ADVBProgList::FindDifferences(ADVBProgList& list1, ADVBProgList& list2, bool in1only, bool in2only)
{
    uint_t i;

    CreateHash();
    list1.CreateHash();
    list2.CreateHash();

    if (in1only) {
        for (i = 0; i < list1.Count(); i++) {
            if (!list2.FindSimilar(list1[i])) AddProg(list1[i]);
        }
    }

    if (in2only) {
        for (i = 0; i < list2.Count(); i++) {
            if (!FindUUID(list2[i]) && !list1.FindSimilar(list2[i])) AddProg(list2[i]);
        }
    }

    Sort();

    return *this;
}

ADVBProgList& ADVBProgList::FindSimilarities(ADVBProgList& list1, ADVBProgList& list2)
{
    uint_t i;

    CreateHash();
    list1.CreateHash();
    list2.CreateHash();

    for (i = 0; i < list1.Count(); i++) {
        if (list2.FindUUID(list1[i])) AddProg(list1[i]);
    }

    return *this;
}

bool ADVBProgList::WriteToGNUPlotFile(const AString& filename) const
{
    AStdFile fp;
    bool success = false;

    if (fp.open(filename, "w")) {
        std::vector<AString>      channellist;
        std::map<AString, uint_t> channelmap;
        uint_t i;

        for (i = 0; i < Count(); i++) {
            AString channel = GetProg(i).GetChannel();

            if (channelmap.find(channel) == channelmap.end()) {
                channellist.push_back(channel);
                channelmap[channel] = true;
            }
        }

        std::sort(channellist.begin(), channellist.end());

        for (i = 0; i < (uint_t)channellist.size(); i++) {
            channelmap[channellist[i]] = i;
        }

        fp.printf("#time channel-index channel-name dvb-card length(hours) recorded scheduled rejected desc\n");

        for (i = 0; i < Count(); i++) {
            const ADVBProg& prog    = GetProg(i);
            const AString   channel = prog.GetChannel();
            const uint64_t  times[] = {
                prog.GetRecordStart() ? prog.GetRecordStart() : prog.GetStart(),
                prog.GetStart(),
                prog.GetStop(),
                prog.GetRecordStop() ? prog.GetRecordStop() : prog.GetStop(),
            };
            const double levels[] = {
                0.0,
                (double)prog.GetLength() / 3600000.0,
                (double)prog.GetLength() / 3600000.0,
                0.0,
            };
            AString desc = prog.GetQuickDescription();
            uint_t  card = prog.GetDVBCard();
            uint_t  k;

            for (k = 0; k < NUMBEROF(times); k++) {
                fp.printf("%s %u '%s' %u %s %u %u %u %s\n",
                          ADateTime(times[k]).DateFormat("%Y-%M-%D %h:%m:%s").str(),
                          channelmap[channel],
                          channel.str(),
                          card,
                          AValue(levels[k]).ToString().str(),
                          (uint_t)prog.IsRecorded(),
                          (uint_t)prog.IsScheduled(),
                          (uint_t)prog.IsRejected(),
                          desc.str());
            }

            fp.printf("\n\n");
        }

        fp.close();

        success = true;
    }

    return success;
}

void ADVBProgList::SearchAndReplace(const AString& search, const AString& replace)
{
    uint_t i, n = Count();

    for (i = 0; i < n; i++) {
        GetProgWritable(i).SearchAndReplace(search, replace);
    }
}

int ADVBProgList::AddProg(const AString& prog, bool sort, bool removeoverlaps)
{
    ADVBProg *pprog;
    int index = -1;

    if ((pprog = new ADVBProg(prog)) != NULL) {
        if (pprog->Valid()) {
            index = AddProg(pprog, sort, removeoverlaps);
        }
        else delete pprog;
    }

    return index;
}

int ADVBProgList::AddProg(const ADVBProg& prog, bool sort, bool removeoverlaps)
{
    ADVBProg *pprog;
    int index = -1;

    if ((pprog = new ADVBProg(prog)) != NULL) {
        if (pprog->Valid()) {
            index = AddProg(pprog, sort, removeoverlaps);
        }
        else delete pprog;
    }

    return index;
}

uint_t ADVBProgList::ModifyProg(const ADVBProg& prog, uint_t mode, bool sort)
{
    ADVBProg *pprog;
    uint_t res = 0;

    if ((pprog = FindUUIDWritable(prog)) != NULL) {
        if (mode & Prog_Modify) {
            pprog->Modify(prog);
            res = Prog_Modify;
        }
    }
    else if ((mode & Prog_Add) && (AddProg(prog, sort, false) >= 0)) res = Prog_Add;

    return res;
}

uint_t ADVBProgList::FindIndex(uint_t timeindex, uint64_t t) const
{
    const ADVBProg **progs = (const ADVBProg **)proglist.List();
    const uint_t n = Count();
    uint_t index   = 0;
    bool   forward = true;

    if (n >= 2) {
        uint_t log2 = (uint_t)ceil(log((double)n) / log(2.0)) - 1;
        uint_t inc  = 1 << log2;

        forward = (GetProg(0).GetTimeIndex(timeindex) <= GetProg(n - 1).GetTimeIndex(timeindex));

        if (forward) {
            while (inc) {
                uint_t index2 = index + inc;
                if ((index2 < n) && (t >= progs[index2]->GetTimeIndex(timeindex))) index = index2;
                inc >>= 1;
            }
        }
        else {
            while (inc) {
                uint_t index2 = index + inc;
                if ((index2 < n) && (t <= progs[index2]->GetTimeIndex(timeindex))) index = index2;
                inc >>= 1;
            }
        }
    }

    if (forward) {
        while ((index < n) && (t >= progs[index]->GetTimeIndex(timeindex))) index++;
    }
    else {
        while ((index < n) && (t <= progs[index]->GetTimeIndex(timeindex))) index++;
    }

    return index;
}

uint_t ADVBProgList::FindIndex(const ADVBProg& prog) const
{
    const ADVBProg **progs = (const ADVBProg **)proglist.List();
    const uint_t n = Count();
    uint_t index   = 0;
    bool   forward = true;

    if (n >= 2) {
        uint_t log2 = (uint_t)ceil(log((double)n) / log(2.0)) - 1;
        uint_t inc  = 1 << log2;

        forward = (GetProg(0) <= GetProg(n - 1));

        if (forward) {
            while (inc) {
                uint_t index2 = index + inc;
                if ((index2 < n) && (prog >= *progs[index2])) index = index2;
                inc >>= 1;
            }
        }
        else {
            while (inc) {
                uint_t index2 = index + inc;
                if ((index2 < n) && (prog <= *progs[index2])) index = index2;
                inc >>= 1;
            }
        }
    }

    if (forward) {
        while ((index < n) && (prog >= *progs[index])) index++;
    }
    else {
        while ((index < n) && (prog <= *progs[index])) index++;
    }

    return index;
}

int ADVBProgList::AddProg(const ADVBProg *prog, bool sort, bool removeoverlaps)
{
    const ADVBConfig& config = ADVBConfig::Get();
    uint_t i;
    int index = -1;

    (void)config;

    if (removeoverlaps && Count()) {
        uint_t i1 = FindIndex(ADVBProg::TimeIndex_Stop,  prog->GetTimeIndex(ADVBProg::TimeIndex_Start) - 1);
        uint_t i2 = FindIndex(ADVBProg::TimeIndex_Start, prog->GetTimeIndex(ADVBProg::TimeIndex_Stop)  + 1);

        for (i = i1; (i <= i2) && (i < Count()) && !HasQuit();) {
            const ADVBProg& prog1 = GetProg(i);

            if (prog->OverlapsOnSameChannel(prog1)) {
                //config.printf("'%s' overlaps with '%s', deleting '%s'", prog1.GetQuickDescription().str(), prog->GetQuickDescription().str(), prog1.GetQuickDescription().str());
                DeleteProg(i);
                i2--;
            }
            else i++;
        }
    }

    if (sort) {
        index = FindIndex(*prog);

#if 0
        {
            bool error = false;

            config.printf("Inserting %s between %s and %s (index %d/%u)",
                          prog->GetStartDT().DateToStr().str(),
                          (index > 0)            ? GetProg(index - 1).GetStartDT().DateToStr().str() : "<start>",
                          (index < (int)Count()) ? GetProg(index).GetStartDT().DateToStr().str() : "<end>",
                          index, Count());

            if ((index > 0) && (prog->GetStartDT() < GetProg(index - 1).GetStartDT())) {
                config.printf("Error: %s < %s", prog->GetStartDT().DateToStr().str(), GetProg(index - 1).GetStartDT().DateToStr().str());
                error = true;
            }
            if ((index < (int)Count()) && (prog->GetStartDT() > GetProg(index).GetStartDT())) {
                config.printf("Error: %s > %s", prog->GetStartDT().DateToStr().str(), GetProg(index).GetStartDT().DateToStr().str());
                error = true;
            }
            if (error) {
                config.printf("Errors found, order:");
                for (i = 0; i < Count(); i++) {
                    config.printf("%u/%u: %s", i, Count(), GetProg(i).GetStartDT().DateToStr().str());
                }
                exit(0);
            }
        }
#endif

        index = proglist.Add((uptr_t)prog, index);
    }
    else index = proglist.Add((uptr_t)prog);

    if ((index >= 0) && useproghash) proghash.Insert(prog->GetUUID(), (uptr_t)prog);

    return index;
}

const ADVBProg& ADVBProgList::GetProg(uint_t n) const
{
    static const ADVBProg dummy;
    return proglist[n] ? *(const ADVBProg *)proglist[n] : dummy;
}

ADVBProg& ADVBProgList::GetProgWritable(uint_t n) const
{
    static ADVBProg dummy;
    return proglist[n] ? *(ADVBProg *)proglist[n] : dummy;
}

bool ADVBProgList::DeleteProg(uint_t n)
{
    ADVBProg *prog = (ADVBProg *)proglist[n];
    bool success = false;

    if (prog) {
        proglist.RemoveIndex(n);
        proghash.Remove(prog->GetUUID());
        delete prog;
        success = true;
    }

    return success;
}

bool ADVBProgList::DeleteProg(const ADVBProg& prog)
{
    uint_t i;

    for (i = 0; i < Count(); i++) {
        const ADVBProg& prog1 = GetProg(i);

        if (prog == prog1) {
            return DeleteProg(i);
        }
    }

    return false;
}

void ADVBProgList::DeleteProgrammesBefore(const ADateTime& dt)
{
    uint64_t _dt = (uint64_t)dt;
    uint_t i;

    for (i = 0; i < Count();) {
        const ADVBProg& prog = GetProg(i);

        if (prog.GetStop() < _dt) DeleteProg(i);
        else i++;
    }
}

uint_t ADVBProgList::CountOccurances(const AString& uuid) const
{
    uint_t i, n = 0;

    for (i = 0; i < Count(); i++) {
        if (GetProg(i).GetUUID() == uuid) n++;
    }

    return n;
}

void ADVBProgList::CreateHash()
{
    uint_t i, n = Count();

    proghash.Delete();
    useproghash = true;

    for (i = 0; i < n; i++) {
        const ADVBProg& prog = GetProg(i);

        proghash.Insert(prog.GetUUID(), (uptr_t)&prog);
    }
}

const ADVBProg *ADVBProgList::FindUUID(const AString& uuid) const
{
    const ADVBProg *prog = NULL;
    int p;

    if ((p = FindUUIDIndex(uuid)) >= 0) prog = &GetProg(p);

    return prog;
}

int ADVBProgList::FindUUIDIndex(const AString& uuid) const
{
    int res = -1;

    if (proghash.GetItems() > 0) {
        res = proglist.Find(proghash.Read(uuid));
    }
    else {
        uint_t i, n = Count();

        for (i = 0; i < n; i++) {
            if (GetProg(i) == uuid) {
                res = (int)i;
                break;
            }
        }
    }


    return res;
}

ADVBProg *ADVBProgList::FindUUIDWritable(const AString& uuid) const
{
    ADVBProg *prog = NULL;
    int p;

    if      (proghash.GetItems())            prog = (ADVBProg *)proghash.Read(uuid);
    else if ((p = FindUUIDIndex(uuid)) >= 0) prog = &GetProgWritable(p);

    return prog;
}

void ADVBProgList::FindProgrammes(ADVBProgList& dest, const ADataList& patternlist, uint_t maxmatches) const
{
    const ADVBConfig& config = ADVBConfig::Get();
    uint_t i, j, n = patternlist.Count(), nfound = 0;

    //config.logit("Searching using %u patterns (max matches %u)...", n, maxmatches);
    //for (i = 0; i < n; i++) {
    //debug("Pattern %u/%u:\n%s", i, n, ADVBPatterns::ToString(*(const PATTERN *)patternlist[i]).str());
    //}

    for (i = 0; (i < Count()) && !HasQuit() && (nfound < maxmatches); i++) {
        const ADVBProg& prog = GetProg(i);
        ADVBProg *prog1 = NULL;
        bool     addtolist = false;
        bool     excluded  = false;

        for (j = 0; (j < n) && !HasQuit(); j++) {
            const PATTERN& pattern = *(const PATTERN *)patternlist[j];

            if (pattern.enabled) {
                bool match = prog1 ? prog1->Match(pattern) : prog.Match(pattern);

                if (match) {
                    if (pattern.exclude) {
                        excluded = true;
                        break;
                    }

                    if (!prog1) prog1 = new ADVBProg(prog);

                    if (prog1) {
                        if (!prog1->GetPattern()[0] || prog1->IsPartialPattern()) {
                            prog1->SetPattern(pattern.pattern);

                            if (pattern.user.Valid()) {
                                prog1->SetUser(pattern.user);
                            }

                            prog1->ClearPartialPattern();
                            prog1->AssignValues(pattern);
                        }
                        else {
                            prog1->ClearPartialPattern();
                            prog1->UpdateValues(pattern);
                        }

                        if (!pattern.scorebased && !prog1->IsPartialPattern()) {
                            addtolist = true;
                            break;
                        }
                    }
                }
            }
        }

        if (prog1) {
            addtolist |= (prog1->GetScore() >= config.GetScoreThreshold());

            if (addtolist && !excluded) {
                dest.AddProg(*prog1, false);
                nfound++;
            }

            delete prog1;
        }
    }

    //config.logit("Search done, %u programmes found", nfound);
}

void ADVBProgList::FindProgrammes(ADVBProgList& dest, const AString& patterns, AString& errors, const AString& sep, uint_t maxmatches) const
{
    ADataList patternlist;

    ADVBPatterns::ParsePatterns(patternlist, patterns, errors, sep);

    FindProgrammes(dest, patternlist, maxmatches);
}

uint_t ADVBProgList::FindSimilarProgrammes(ADVBProgList& dest, const ADVBProg& prog, uint_t index) const
{
    uint_t i, n = 0;

    for (i = index; i < Count(); i++) {
        const ADVBProg& prog1 = GetProg(i);

        if ((&prog != &prog1) && ADVBProg::SameProgramme(prog, prog1)) {
            dest.AddProg(prog1);
            n++;
        }
    }

    return n;
}

const ADVBProg *ADVBProgList::GetNextProgramme(const ADVBProg *prog, uint_t *index) const
{
    if (prog != NULL) {
        const ADVBProg *tmpprog;
        uint_t progindex;
        int p;

        if ((((p = proglist.Find((uptr_t)prog)) >= 0) ||
             (((tmpprog = FindUUID(*prog)) != NULL) &&
              ((p = proglist.Find((uptr_t)tmpprog)) >= 0))) &&
            (p < ((int)Count() - 1))) {
            progindex = (uint_t)p + 1;
            if (index != NULL) index[0] = progindex;
            prog = &GetProg(progindex);
        }
        else prog = NULL;
    }

    return prog;
}

const ADVBProg *ADVBProgList::GetPrevProgramme(const ADVBProg *prog, uint_t *index) const
{
    if (prog != NULL) {
        const ADVBProg *tmpprog;
        uint_t progindex;
        int p;

        if ((((p = proglist.Find((uptr_t)prog)) >= 0) ||
             (((tmpprog = FindUUID(*prog)) != NULL) &&
              ((p = proglist.Find((uptr_t)tmpprog)) >= 0))) &&
            (p > 0)) {
            progindex = (uint_t)p - 1;
            if (index != NULL) index[0] = progindex;
            prog = &GetProg(progindex);
        }
        else prog = NULL;
    }

    return prog;
}

const ADVBProg *ADVBProgList::FindSimilar(const ADVBProg& prog, const ADVBProg *startprog) const
{
    const ADVBProg *res = NULL;
    uint_t i = 0;

    if (startprog != NULL) {
        if (GetNextProgramme(startprog, &i) == NULL) {
            return res;
        }
    }

    for (; i < Count(); i++) {
        const ADVBProg& prog1 = GetProg(i);

        if (ADVBProg::SameProgramme(prog, prog1)) {
            res = &prog1;
            break;
        }
    }

    return res;
}

const ADVBProg *ADVBProgList::FindLastSimilar(const ADVBProg& prog, const ADVBProg *startprog) const
{
    const ADVBProg *res = NULL;

    if (Count()) {
        uint_t i = Count() - 1;

        if (startprog) {
            if (GetPrevProgramme(startprog, &i) == NULL) {
                return res;
            }
        }

        do {
            const ADVBProg& prog1 = GetProg(i);

            if (ADVBProg::SameProgramme(prog, prog1)) {
                res = &prog1;
                break;
            }
        } while (i--);
    }

    return res;
}

ADVBProg *ADVBProgList::FindSimilarWritable(const ADVBProg& prog, ADVBProg *startprog)
{
    ADVBProg *res = NULL;
    uint_t i = 0;

    if (startprog) {
        if (GetNextProgramme(startprog, &i) == NULL) {
            return res;
        }
    }

    for (; i < Count(); i++) {
        ADVBProg& prog1 = GetProgWritable(i);

        if (ADVBProg::SameProgramme(prog, prog1)) {
            res = &prog1;
            break;
        }
    }

    return res;
}

ADVBProg *ADVBProgList::FindLastSimilarWritable(const ADVBProg& prog, ADVBProg *startprog)
{
    ADVBProg *res = NULL;

    if (Count()) {
        uint_t i = Count() - 1;

        if (startprog) {
            if (GetPrevProgramme(startprog, &i) == NULL) {
                return res;
            }
        }

        do {
            ADVBProg& prog1 = GetProgWritable(i);

            if (ADVBProg::SameProgramme(prog, prog1)) {
                res = &prog1;
                break;
            }
        } while (i--);
    }

    return res;
}

const ADVBProg *ADVBProgList::FindCompleteRecording(const ADVBProg& prog, const ADVBProg *startprog) const
{
    const ADVBProg *res = startprog;

    while (((res = FindSimilar(prog, res)) != NULL) && (res->IgnoreRecording() || !res->IsRecordingComplete())) ;

    return res;
}

const ADVBProg *ADVBProgList::FindCompleteRecordingThatExists(const ADVBProg& prog, const ADVBProg *startprog) const
{
    const ADVBProg *res = startprog;

    while (((res = FindSimilar(prog, res)) != NULL) && (res->IgnoreRecording() || !res->IsRecordingComplete() || !res->IsAvailable())) ;

    return res;
}

ADVBProg *ADVBProgList::FindOverlap(const ADVBProg& prog1, const ADVBProg *prog2) const
{
    uint_t i = 0;
    int p;

    if (prog2 && ((p = proglist.Find((uptr_t)prog2)) >= 0)) i = p + 1;

    while ((prog2 = (const ADVBProg *)proglist[i++]) != NULL) {
        if (CompareNoCase(prog2->GetUUID(), prog1.GetUUID()) != 0) {
            if (prog2->Overlaps(prog1)) break;
        }
    }

    return (ADVBProg *)prog2;
}

ADVBProg *ADVBProgList::FindRecordOverlap(const ADVBProg& prog1, const ADVBProg *prog2) const
{
    const ADVBConfig& config = ADVBConfig::Get();
    const uint64_t mininterrectime = (uint64_t)config.GetConfigItem("mininterrectime", config.GetDefaultInterRecTime()) * 1000;
    const uint64_t st1 = prog1.GetRecordStart();
    const uint64_t et1 = prog1.GetRecordStop() + mininterrectime;
    uint_t i = 0;
    int p;

    if (prog2 && ((p = proglist.Find((uptr_t)prog2)) >= 0)) i = p + 1;

    while ((prog2 = (const ADVBProg *)proglist[i++]) != NULL) {
        if (prog2 != &prog1) {
            const uint64_t st2 = prog2->GetRecordStart();
            const uint64_t et2 = prog2->GetRecordStop() + mininterrectime;

            if ((st2 < et1) && (et2 > st1)) break;
        }
    }

    return (ADVBProg *)prog2;
}

void ADVBProgList::AdjustRecordTimes(uint64_t recstarttime)
{
    const ADVBConfig& config = ADVBConfig::Get();
    const uint64_t mininterrectime = (uint64_t)config.GetConfigItem("mininterrectime", config.GetDefaultInterRecTime()) * 1000;
    uint_t i, j;

    config.logit("Adjusting record times of any overlapping programmes");

    for (i = 0; i < Count(); i++) {
        ADVBProg *prog1 = (ADVBProg *)proglist[i];

        for (j = i + 1; j < Count(); j++) {
            ADVBProg *prog2 = (ADVBProg *)proglist[j];
            ADVBProg *firstprog  = NULL;
            ADVBProg *secondprog = NULL;

            // order programmes in chronilogical order
            if (prog1->GetStart() < prog2->GetStart()) {
                firstprog  = prog1;
                secondprog = prog2;
            }
            else if (prog2->GetStart() < prog1->GetStart()) {
                firstprog  = prog2;
                secondprog = prog1;
            }

            // if firstprog is before secondprog and the recording of firstprog overlaps the recording of secondprog, adjust times
            if (firstprog && secondprog &&
                ((firstprog->GetRecordStop() + mininterrectime) >= secondprog->GetRecordStart())) {
                config.logit("'%s' recording end overlaps '%s' recording start - shift times",
                             firstprog->GetQuickDescription().str(),
                             secondprog->GetQuickDescription().str());

                config.logit("Recording times originally %s - %s and %s - %s",
                             firstprog->GetRecordStartDT().UTCToLocal().DateFormat("%h:%m:%s").str(),
                             firstprog->GetRecordStopDT().UTCToLocal().DateFormat("%h:%m:%s").str(),
                             secondprog->GetRecordStartDT().UTCToLocal().DateFormat("%h:%m:%s").str(),
                             secondprog->GetRecordStopDT().UTCToLocal().DateFormat("%h:%m:%s").str());

                uint64_t diff = firstprog->GetRecordStop()   + mininterrectime - secondprog->GetRecordStart();
                uint64_t newt = secondprog->GetRecordStart() + diff / 2;

                newt -= newt % 60000;

                firstprog->SetRecordStop(newt - mininterrectime);
                secondprog->SetRecordStart(newt);

                config.logit("Recording times now        %s - %s and %s - %s",
                             firstprog->GetRecordStartDT().UTCToLocal().DateFormat("%h:%m:%s").str(),
                             firstprog->GetRecordStopDT().UTCToLocal().DateFormat("%h:%m:%s").str(),
                             secondprog->GetRecordStartDT().UTCToLocal().DateFormat("%h:%m:%s").str(),
                             secondprog->GetRecordStopDT().UTCToLocal().DateFormat("%h:%m:%s").str());
            }
        }

        // ensure record start time is at least recstarttime
        prog1->SetRecordStart(std::max(prog1->GetRecordStart(), recstarttime));

        uint64_t recstart;
        if ((recstart = (prog1->GetRecordStart() % 60000)) != 0) {
            config.printf("Warning: programme '%s' set to start recording at %s, rounding up to next minute", prog1->GetQuickDescription().str(), prog1->GetRecordStartDT().UTCToLocal().DateToStr().str());
            // round up to next minute
            prog1->SetRecordStart(prog1->GetRecordStart() + (60000 - recstart));
        }
    }
}

const ADVBProg *ADVBProgList::FindFirstRecordOverlap() const
{
    const ADVBProg *prog = NULL;
    uint_t i;

    for (i = 0; (i < Count()) && ((prog = FindRecordOverlap(GetProg(i))) == NULL); i++) ;

    return prog;
}

void ADVBProgList::UnscheduleAllProgrammes()
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString cmd;
    AString filename, scriptname;
    int queue = config.GetQueue().FirstChar();

    filename   = config.GetTempFile("joblist",   ".sh");
    scriptname = config.GetTempFile("jobscript", ".sh");

    cmd.printf("atq -q %c >%s", queue, filename.str());
    if (system(cmd) == 0) {
        AStdFile fp;

        if (fp.open(filename)) {
            AString line;

            while (line.ReadLn(fp) >= 0) {
                uint_t job = (uint_t)line.Word(0);

                if (job) {
                    bool done = false;

                    cmd.Delete();
                    cmd.printf("at -c %u >%s", job, scriptname.str());

                    if (system(cmd) == 0) {
                        ADVBProg prog;

                        if (prog.ReadFromJob(scriptname)) {
                            cmd.Delete();
                            cmd.printf("atrm %u", job);

                            if (system(cmd) == 0) {
                                config.logit("Deleted job %u ('%s')", job, prog.GetQuickDescription().str());
                                done = true;
                            }
                            else config.logit("Failed to delete job %u ('%s')", job, prog.GetQuickDescription().str());
                        }
                        else config.logit("Failed to decode job %u", job);
                    }

                    remove(scriptname);

                    if (!done) {
                        cmd.Delete();
                        cmd.printf("atrm %u", job);
                        if (system(cmd) == 0) {
                            config.logit("Deleted job %u (unknown)", job);
                        }
                        else config.logit("Failed to delete job %u (unknown)", job);
                    }
                }
            }

            fp.close();
        }
    }

    remove(filename);
}

int ADVBProgList::SortPatterns(uptr_t item1, uptr_t item2, void *context)
{
    const PATTERN& pat1 = *(const PATTERN *)item1;
    const PATTERN& pat2 = *(const PATTERN *)item2;
    uint_t pat1dis = (pat1.pattern[0] == '#');
    uint_t pat2dis = (pat2.pattern[0] == '#');
    int res;

    UNUSED(context);

    if (pat1.pri < pat2.pri) return  1;
    if (pat1.pri > pat2.pri) return -1;

    if (!pat1.enabled &&  pat2.enabled) return 1;
    if ( pat1.enabled && !pat2.enabled) return -1;

    if ((res = stricmp(pat1.user, pat2.user)) != 0) return res;
    if ((res = stricmp(pat1.pattern.str() + pat1dis,
                       pat2.pattern.str() + pat2dis)) != 0) return res;

    return 0;
}

void ADVBProgList::ReadPatterns(ADataList& patternlist, AString& errors, bool sort)
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString  patternfilename    = config.GetPatternsFile();
    AString  filepattern        = config.GetUserPatternsPattern();
    AString  filepattern_parsed = ParseRegex(filepattern);
    AList    userpatterns;
    AStdFile fp;

    if (fp.open(patternfilename)) {
        AString line;

        while (line.ReadLn(fp) >= 0) {
            if (line.Words(0).Valid()) {
                ADVBPatterns::ParsePattern(patternlist, line, errors);
            }
        }

        fp.close();
    }
    else config.logit("Failed to read patterns file '%s'", patternfilename.str());

    ::CollectFiles(filepattern.PathPart(), filepattern.FilePart(), 0, userpatterns);

    const AString *file = AString::Cast(userpatterns.First());
    while (file) {
        AString   user;
        ADataList regions;

        if (MatchRegex(*file, filepattern_parsed, regions)) {
            const REGEXREGION *region = (const REGEXREGION *)regions[0];

            if (region) user = file->Mid(region->pos, region->len);
            else        config.logit("Failed to find user in '%s' (no region)", file->str());
        }
        else config.logit("Failed to find user in '%s'", file->str());

        if (fp.open(*file)) {
            AString line;

            while (line.ReadLn(fp) >= 0) {
                if (line.Words(0).Valid()) {
                    ADVBPatterns::ParsePattern(patternlist, line, errors, user);
                }
            }

            fp.close();
        }
        else config.logit("Failed to read file '%s'", file->str());

        file = file->Next();
    }

    if (sort) patternlist.Sort(&SortPatterns);
}

int ADVBProgList::SortProgsByUserThenDir(uptr_t item1, uptr_t item2, void *pContext)
{
    const ADVBProg& prog1 = *(const ADVBProg *)item1;
    const ADVBProg& prog2 = *(const ADVBProg *)item2;
    int res;

    UNUSED(pContext);

    if ((res = strcmp(prog1.GetUser(), prog2.GetUser())) != 0) return res;

    return strcmp(prog1.GetFilename(), prog2.GetFilename());
}

bool ADVBProgList::CheckDiskSpace(bool runcmd, rapidjson::Document *doc)
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADVBProgList proglist;

    proglist.ReadFromFile(config.GetScheduledFile());

    return proglist.CheckDiskSpaceList(runcmd, doc);
}

bool ADVBProgList::CheckDiskSpaceList(bool runcmd, rapidjson::Document *doc) const
{
    rapidjson::Document::AllocatorType *allocator = doc ? &doc->GetAllocator() : NULL;
    rapidjson::Value  obj;
    const ADVBConfig& config = ADVBConfig::Get();
    AStdFile fp;
    AHash    hash;
    AString  filename;
    double   lowlimit = config.GetLowSpaceWarningLimit();
    uint_t   i, userlen = 0;
    bool     okay  = true;

    obj.SetArray();

    if (runcmd) {
        filename = config.GetTempFile("patterndiskspace", ".txt");
        if (fp.open(filename, "w")) {
            fp.printf("Disk space for patterns as of %s:\n\n", ADateTime().DateToStr().str());
        }
    }

    AString fmt;
    fmt.printf("%%-%us %%6.1lfG %%s", userlen);

    const AString recdir = config.GetRecordingsDir();
    for (i = 0; i < Count(); i++) {
        const ADVBProg& prog = GetProg(i);
        const AString   user = prog.GetUser();
        const AString   rdir = (prog.IsConverted() || config.IsRecordingSlave()) ? AString(prog.GetFilename()).PathPart() : prog.GetConvertedDestinationDirectory();
        AString dir;

        if (rdir.StartsWith(recdir)) dir = rdir.Mid(recdir.len() + 1);
        else                         dir = rdir;

        //printf("\nUser '%s' dir '%s' rdir '%s'\n", user.str(), dir.str(), rdir.str());
        //fflush(stdout);

        CreateDirectory(rdir);

        if (!hash.Exists(rdir)) {
            struct statvfs fiData;

            hash.Insert(rdir, 0);

            if (statvfs(rdir, &fiData) >= 0) {
                AString str;
                double  gb = (double)fiData.f_bavail * (double)fiData.f_bsize / ((uint64_t)1024.0 * (uint64_t)1024.0 * (uint64_t)1024.0);

                if (allocator) {
                    rapidjson::Value subobj;

                    subobj.SetObject();

                    subobj.AddMember("user", rapidjson::Value(user.str(), *allocator), *allocator);
                    subobj.AddMember("folder", rapidjson::Value(dir.str(), *allocator), *allocator);
                    subobj.AddMember("fullfolder", rapidjson::Value(rdir.str(), *allocator), *allocator);
                    subobj.AddMember("freespace", rapidjson::Value(gb), *allocator);
                    subobj.AddMember("level", rapidjson::Value((uint_t)(gb / lowlimit)), *allocator);

                    obj.PushBack(subobj, *allocator);
                }

                str.printf(fmt, prog.GetUser(), gb, dir.str());
                if (gb < lowlimit) {
                    str.printf(" Warning!");
                    okay = false;
                }

                if (runcmd) {
                    fp.printf("%s\n", str.str());
                }

                //config.printf("%s", str.str());
            }
        }
    }

    if (runcmd) {
        fp.close();

        if (!okay) {
            AString cmd = config.GetConfigItem("lowdiskspacecmd");
            if (cmd.Valid()) {
                cmd = cmd.SearchAndReplace("{logfile}", filename);

                RunAndLogCommand(cmd);
            }
        }
        else {
            AString cmd = config.GetConfigItem("diskspacecmd");
            if (cmd.Valid()) {
                cmd = cmd.SearchAndReplace("{logfile}", filename);

                RunAndLogCommand(cmd);
            }
        }

        remove(filename);
    }

    if (allocator) {
        doc->AddMember("diskspace", obj, *allocator);
    }

    return okay;
}

uint_t ADVBProgList::SchedulePatterns(const ADateTime& starttime, bool commit)
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADVBLock     lock("dvbfiles");
    ADVBProgList proglist;
    AString      filename = config.GetListingsFile();
    uint_t       res = 0;

    if (config.GetServerHost().Valid()) {
        // push reschedule request up to server
        config.printf("Triggering reschedule on server '%s'", config.GetServerHost().str());
        TriggerServerCommand(config.GetServerRescheduleCommand());
    }
    else {
        commit &= config.CommitScheduling();

        if (!commit) config.logit("** Not committing scheduling **");

        if (config.GetRecordingSlave().Valid()) {
            ADVBProgList list;
            list.ModifyFromRecordingSlave(config.GetRecordedFile(), ADVBProgList::Prog_Add);
            GetRecordingListFromRecordingSlave();
            GetFileFromRecordingSlave(config.GetDVBChannelsFile());
        }

        config.logit("Loading listings from '%s'", filename.str());

        if (proglist.ReadFromFile(filename)) {
            ADVBProgList reslist;
            uint_t i;

            config.printf("Loaded %u programmes from '%s'", proglist.Count(), filename.str());

            config.logit("Removing programmes that have already finished");

            proglist.DeleteProgrammesBefore(starttime);

            config.logit("List now contains %u programmes", proglist.Count());

            ADataList patternlist;
            AString   errors;
            ReadPatterns(patternlist, errors, false);

            std::map<AString, ADVBPatterns::PATTERN> extraterms;
            AString str;
            if ((str = config.GetConfigItem("extraterms")).Valid()) {
                AString errs = ADVBPatterns::ParsePattern(str, extraterms[""]);
                if (errs.Valid()) {
                    errors += errs + "\n";
                }
            }

            for (i = 0; i < patternlist.Count(); i++) {
                PATTERN& pattern = *(PATTERN *)patternlist[i];
                const AString& user = pattern.user;

                if (user.Valid()) {
                    if ((extraterms.find(user) == extraterms.end()) &&
                        ((str = config.GetConfigItem("extraterms:" + user)).Valid())) {
                        AString errs = ADVBPatterns::ParsePattern(str, extraterms[user]);
                        if (errs.Valid()) {
                            errors += errs + "\n";
                        }
                    }
                }

                ADVBPatterns::AppendTerms(pattern, extraterms[user]);
                ADVBPatterns::AppendTerms(pattern, extraterms[""]);
            }

            if (errors.Valid()) {
                config.printf("Errors found during parsing:");

                for (i = 0; i < patternlist.Count(); i++) {
                    const PATTERN& pattern = *(const PATTERN *)patternlist[i];

                    if (pattern.errors.Valid()) {
                        config.printf("Parsing '%s': %s", pattern.pattern.str(), pattern.errors.str());
                    }
                }

                std::map<AString, ADVBPatterns::PATTERN>::iterator it;
                for (it = extraterms.begin(); it != extraterms.end(); ++it) {
                    const PATTERN& pattern = it->second;

                    if (pattern.errors.Valid()) {
                        config.printf("Parsing '%s': %s", pattern.pattern.str(), pattern.errors.str());
                    }
                }
            }

            config.printf("Finding programmes using %u patterns", patternlist.Count());
            proglist.FindProgrammes(reslist, patternlist);

            config.logit("Found %u programmes from %u patterns", reslist.Count(), patternlist.Count());

            if (AStdFile::exists(config.GetExtraRecordFile())) {
                AString data;

                if (data.ReadFromFile(config.GetExtraRecordFile())) {
                    AString str;
                    uint_t n = 0;
                    int p = 0, p1;

                    while ((p1 = data.Pos("\n\n", p)) >= 0) {
                        ADVBProg prog;

                        str.Create(data.str() + p, p1 + 1 - p, false);

                        if (str.CountWords() > 0) {
                            prog = str;
                            if (prog.Valid()) {
                                if (prog.GetStopDT() > starttime) {
                                    if (reslist.AddProg(prog, true) >= 0) n++;
                                    else config.printf("Failed to add extra prog");
                                }
                            }
                            else config.printf("Extra prog is invalid!");
                        }

                        p = p1 + 1;
                    }

                    if (n) config.printf("Added %u extra programmes to record", n);
                }
            }

            res = reslist.Schedule(starttime);

            if (commit) WriteToJobList();

            CreateCombinedFile();
            CreateGraphs();

            ADVBProgList::CheckDiskSpace(true);
        }
        else config.logit("Failed to read listings file '%s'", filename.str());
    }

    return res;
}

void ADVBProgList::Sort(bool reverse)
{
    Sort(&SortProgs, &reverse);
}

void ADVBProgList::Sort(const ADVBProg::FIELDLIST& fieldlist)
{
    if (fieldlist.size() > 0) {
        Sort(&SortProgsAdvanced, (void *)&fieldlist);
    }
}

void ADVBProgList::Sort(int (*fn)(uptr_t item1, uptr_t item2, void *pContext), void *pContext)
{
    proglist.Sort(fn, pContext);
}

int ADVBProgList::CompareEpisode(uptr_t item1, uptr_t item2, void *pContext)
{
    UNUSED(pContext);
    return ADVBProg::CompareEpisode(((const ADVBProg *)item1)->GetEpisode(), ((const ADVBProg *)item2)->GetEpisode());
}

void ADVBProgList::AddToList(const AString& filename, const ADVBProg& prog, bool sort, bool removeoverlaps)
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADVBLock     lock("dvbfiles");
    ADVBProgList list;

    if (!list.ReadFromFile(filename)) config.logit("Failed to read file '%s' for adding a programme", filename.str());
    list.AddProg(prog, sort, removeoverlaps);
    if (!list.WriteToFile(filename)) config.logit("Failed to write file '%s' after adding a programme", filename.str());
}

bool ADVBProgList::RemoveFromList(const AString& filename, const ADVBProg& prog)
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADVBLock     lock("dvbfiles");
    ADVBProgList list;
    bool         removed = false;

    if (list.ReadFromFile(filename)) {
        uint_t n;

        for (n = 0; list.DeleteProg(prog); n++) ;

        if      (!n)                          config.logit("Failed to find programme '%s' in list to remove it!", prog.GetQuickDescription().str());
        else if (!list.WriteToFile(filename)) config.logit("Failed to write file '%s' after removing a programme", filename.str());
        else removed = true;
    }
    else config.logit("Failed to read file '%s' for removing a programme", filename.str());

    return removed;
}

bool ADVBProgList::RemoveFromList(const AString& filename, const AString& uuid)
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADVBLock     lock("dvbfiles");
    ADVBProgList list;
    bool         removed = false;

    if (list.ReadFromFile(filename)) {
        const ADVBProg *pprog;
        uint_t n;

        for (n = 0; ((pprog = list.FindUUID(uuid)) != NULL) && list.DeleteProg(*pprog); n++) ;

        if      (!n)                          config.logit("Failed to find programme '%s' in list to remove it!", uuid.str());
        else if (!list.WriteToFile(filename)) config.logit("Failed to write file '%s' after removing a programme", filename.str());
        else removed = true;
    }
    else config.logit("Failed to read file '%s' for removing a programme", filename.str());

    return removed;
}

bool ADVBProgList::RemoveFromRecordLists(const ADVBProg& prog)
{
    return RemoveFromRecordLists(prog.GetUUID());
}

bool ADVBProgList::RemoveFromRecordLists(const AString& uuid)
{
    const ADVBConfig& config = ADVBConfig::Get();
    bool removed = false;

    {
        ADVBLock lock("dvbfiles");
        removed |= ADVBProgList::RemoveFromList(config.GetRecordFailuresFile(), uuid);
        removed |= ADVBProgList::RemoveFromList(config.GetRecordedFile(), uuid);
    }

    if (config.GetRecordingSlave().Valid()) {
        AString cmd;

        cmd.printf("dvb --delete-from-record-lists \"%s\"", uuid.Escapify().str());
        RunAndLogRemoteCommand(cmd);
    }

    return removed;
}

uint_t ADVBProgList::Schedule(const ADateTime& starttime)
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADVBLock     lock("dvbfiles");
    ADVBProgList oldrejectedlist;
    ADVBProgList oldscheduledlist;
    ADVBProgList recordedlist;
    ADVBProgList scheduledlist;
    ADVBProgList rejectedlist;
    ADVBProgList runninglist;
    ADVBProgList newrejectedlist;
    const uint64_t starttimems   = (uint64_t)starttime;
    const uint64_t lateststartms = (uint64_t)SUBZ(config.GetLatestStart(), 2) * (uint64_t)60000;
    uint_t i, n, reccount;

    oldrejectedlist.ReadFromFile(config.GetRejectedFile());
    oldscheduledlist.ReadFromFile(config.GetScheduledFile());

    for (i = 0; i < Count(); ) {
        ADVBProg& prog = GetProgWritable(i);
        AString dvbchannel;

        // check and update DVB channel
        if ((dvbchannel = ADVBChannelList::Get().LookupDVBChannel(prog.GetChannel())) != prog.GetDVBChannel()) {
            config.logit("Changing DVB channel of '%s' from '%s' to '%s'", prog.GetDescription().str(), prog.GetDVBChannel(), dvbchannel.str());
            prog.SetDVBChannel(dvbchannel);
        }

        if (!prog.GetDVBChannel()[0]) {
            config.logit("'%s' does not have a DVB channel", prog.GetDescription().str());

            DeleteProg(i);
        }
        else i++;
    }

    WriteToFile(config.GetRequestedFile(), false);

    recordedlist.ReadFromFile(config.GetRecordedFile());
    reccount = recordedlist.Count();

    // read running job(s)
    runninglist.ReadFromFile(config.GetRecordingFile());
    runninglist.CreateHash();

    config.logit("--------------------------------------------------------------------------------");
    config.printf("Found: %u programmes:", Count());
    config.logit("--------------------------------------------------------------------------------");
    for (i = 0; i < Count(); i++) {
        const ADVBProg& prog = GetProg(i);
        const ADVBProg *rprog;

        config.logit("%s%s",
                     prog.GetDescription(1).str(),
                     ((rprog = runninglist.FindUUID(prog)) != NULL) ? AString(" (recording on DVB card %u, hardware card %u)").
                     Arg(rprog->GetDVBCard()).
                     Arg(config.GetPhysicalDVBCard(rprog->GetDVBCard())).str() : "");
    }
    config.logit("--------------------------------------------------------------------------------");

    // remove any programmes that have already been recorded
    // or are mark only
    // or have zero length
    // or are being recorded now
    // or have been on too long
    for (i = 0; i < Count();) {
        const ADVBProg *otherprog = NULL;
        ADVBProg& prog = GetProgWritable(i);
        bool deleteprog = true;

        if (!prog.AllowRepeats() &&                                                                     // for a programme to be deleted: 'allowrepeats' must be disabled; and
            ((prog.RecordIfMissing() &&
              ((otherprog = recordedlist.FindCompleteRecordingThatExists(prog)) != NULL)) ||            // 'recordifmissing' enabled and a completed recording that exists is found; or
             (!prog.RecordIfMissing() &&
              ((otherprog = recordedlist.FindCompleteRecording(prog))           != NULL)))) {           // 'recordifmissing' disabled and a completed recording is found
            config.logit("'%s' has already been recorded ('%s': %s)", prog.GetQuickDescription().str(), otherprog->GetQuickDescription().str(), otherprog->IsAvailable() ? "available" : "NOT available");
        }
        else if (prog.IsMarkOnly()) {
            config.logit("Adding '%s' to recorded list (Mark Only)", prog.GetQuickDescription().str());

            recordedlist.AddProg(prog, false);
        }
        else if (prog.GetStop() == prog.GetStart()) {
            config.logit("'%s' is zero-length", prog.GetQuickDescription().str());
        }
        else if (!prog.AllowRepeats() && ((otherprog = runninglist.FindSimilar(prog)) != NULL)) {
            config.logit("'%s' is being recorded now ('%s')", prog.GetQuickDescription().str(), otherprog->GetQuickDescription().str());
        }
        else if ((starttimems >= lateststartms) && ((starttimems - lateststartms) >= prog.GetStart())) {
            config.logit("'%s' started too long ago (%u minutes)", prog.GetQuickDescription().str(), (uint_t)((starttimems - prog.GetStart() + 59999) / 60000));
        }
        else deleteprog = false;

        if (deleteprog) {
            DeleteProg(i);
        }
        else i++;
    }

    config.logit("Removed recorded and finished programmes");

    n = ScheduleEx(runninglist, scheduledlist, rejectedlist, starttime);

    scheduledlist.WriteToFile(config.GetScheduledFile(), false);
    for (i = 0; i < rejectedlist.Count(); i++) {
        rejectedlist.GetProgWritable(i).SetRejected();
    }
    rejectedlist.WriteToFile(config.GetRejectedFile(), false);

    for (i = 0; i < rejectedlist.Count(); i++) {
        const ADVBProg& prog = rejectedlist.GetProg(i);
        const ADVBProg  *rejprog;

        if ((rejprog = oldrejectedlist.FindUUID(prog.GetUUID())) != NULL) {
            config.printf("%s is still rejected", prog.GetDescription().str());
            oldrejectedlist.DeleteProg(*rejprog);
        }
        else {
            config.printf("%s is a newly rejected programme", prog.GetDescription().str());
            newrejectedlist.AddProg(prog, false, false);
        }
    }

    {
        ADVBProgList programmeslostlist;
        ADVBProgList newprogrammeslist;
        SERIESLIST serieslist;

        // find programmes in old schedule list but not in new one
        programmeslostlist.FindDifferences(oldscheduledlist, scheduledlist, true, false);

        // strip those that have already finished or are running
        for (i = 0; i < programmeslostlist.Count();) {
            const ADVBProg& prog = programmeslostlist[i];

            if ((prog.GetStopDT() <= starttime) ||
                runninglist.FindUUID(prog) ||
                scheduledlist.FindSimilar(prog)) {
                programmeslostlist.DeleteProg(i);
            }
            else i++;
        }

        // generate a list of series from recorded programmes
        recordedlist.FindSeries(serieslist);

        // add a list of series from the old scheduled list of programmes
        oldscheduledlist.FindSeries(serieslist);

        // find programmes in new schedile list but not in old one
        newprogrammeslist.FindDifferences(oldscheduledlist, scheduledlist, false, true);

        // strip any programmes that are either films or from a series already recorded
        newprogrammeslist.StripFilmsAndSeries(serieslist);

        if (programmeslostlist.Count() > 0) {
            config.logit("--------------------------------------------------------------------------------");
            config.printf("%u programmes lost from scheduling:", programmeslostlist.Count());
            for (i = 0; i < programmeslostlist.Count(); i++) {
                const ADVBProg& prog = programmeslostlist.GetProg(i);

                config.printf("%s", prog.GetQuickDescription().str());
            }
            config.logit("--------------------------------------------------------------------------------");
        }

        if (newprogrammeslist.Count() > 0) {
            config.logit("--------------------------------------------------------------------------------");
            config.printf("%u new programmes/series in scheduling:", newprogrammeslist.Count());
            for (i = 0; i < newprogrammeslist.Count(); i++) {
                const ADVBProg& prog = newprogrammeslist.GetProg(i);

                config.printf("%s", prog.GetQuickDescription().str());
            }
            config.logit("--------------------------------------------------------------------------------");
        }

        AString cmd;
        if (((programmeslostlist.Count() > 0) ||
             (newprogrammeslist.Count() > 0)) &&
            ((cmd = config.GetConfigItem("scheduledlistchangedcmd")).Valid())) {
            AString  filename = config.GetLogDir().CatPath("scheduled-list-changed-" + ADateTime().DateFormat("%Y-%M-%D-%h-%m-%s") + ".txt");
            AStdFile fp;

            if (fp.open(filename, "w")) {
                if (programmeslostlist.Count() > 0) {
                    fp.printf("Programmes no longer scheduled at %s:\n", ADateTime().DateToStr().str());

                    for (i = 0; i < programmeslostlist.Count(); i++) {
                        const ADVBProg& prog = programmeslostlist.GetProg(i);

                        fp.printf("%s\n", prog.GetDescription(config.GetScheduleReportVerbosity("lost")).str());
                    }
                    fp.printf("\n");
                }

                if (newprogrammeslist.Count() > 0) {
                    fp.printf("New programmes/series scheduled at %s:\n", ADateTime().DateToStr().str());

                    for (i = 0; i < newprogrammeslist.Count(); i++) {
                        const ADVBProg& prog = newprogrammeslist.GetProg(i);

                        fp.printf("%s\n", prog.GetDescription(config.GetScheduleReportVerbosity("new")).str());
                    }
                }

                fp.close();

                cmd = cmd.SearchAndReplace("{logfile}", filename);

                RunAndLogCommand(cmd);
            }

            //remove(filename);
        }
    }

    if ((newrejectedlist.Count() > 0) ||
        (oldrejectedlist.Count() > 0)) {
        AString cmd;

        if ((cmd = config.GetConfigItem("rejectedcmd")).Valid()) {
            AString  filename = config.GetLogDir().CatPath("rejected-text-" + ADateTime().DateFormat("%Y-%M-%D-%h-%m-%s") + ".txt");
            AStdFile fp;

            if (fp.open(filename, "w")) {
                fp.printf("Summary of newly rejected and previously rejected programmes at %s\n", ADateTime().DateToStr().str());

                if (newrejectedlist.Count() > 0) {
                    fp.printf("\nProgrammes newly rejected:\n");

                    for (i = 0; i < newrejectedlist.Count(); i++) {
                        const ADVBProg& prog = newrejectedlist.GetProg(i);

                        fp.printf("%s\n", prog.GetDescription().str());
                    }
                }

                if (oldrejectedlist.Count() > 0) {
                    fp.printf("\nProgrammes no *longer* rejected:\n");

                    for (i = 0; i < oldrejectedlist.Count(); i++) {
                        const ADVBProg& prog = oldrejectedlist.GetProg(i);

                        fp.printf("%s\n", prog.GetDescription().str());
                    }
                }

                if (rejectedlist.Count() > 0) {
                    fp.printf("\nProgrammes rejected:\n");

                    for (i = 0; i < rejectedlist.Count(); i++) {
                        const ADVBProg& prog = rejectedlist.GetProg(i);

                        fp.printf("%s\n", prog.GetDescription().str());
                    }
                }

                fp.close();

                cmd = cmd.SearchAndReplace("{logfile}", filename);

                RunAndLogCommand(cmd);
            }

            //remove(filename);
        }
    }

    if (recordedlist.Count() > reccount) {
        config.printf("Updating list of recorded programmes");

        recordedlist.WriteToFile(config.GetRecordedFile(), false);
    }

    return n;
}

int ADVBProgList::CompareRepeatLists(uptr_t item1, uptr_t item2, void *context)
{
    // item1 and item2 are ADataList objects, sort them according to the number of items in the list (fewest first);
    const ADataList& list1 = *(const ADataList *)item1;
    const ADataList& list2 = *(const ADataList *)item2;
    const ADVBProg&  prog1 = *(const ADVBProg *)list1[0];
    const ADVBProg&  prog2 = *(const ADVBProg *)list2[0];

    UNUSED(context);

    return ADVBProg::CompareScore(prog1, prog2);
}

void ADVBProgList::CountOverlaps(const ADVBProg::PROGLISTLIST& repeatlists, const ADateTime& starttime)
{
    uint_t i;

    // set scores for the first programme of each list (for sorting below)
    for (i = 0; i < repeatlists.size(); i++) {
        const ADVBProg::PROGLIST& list = *repeatlists[i];
        uint_t j;

        for (j = 0; j < list.size(); j++) {
            ADVBProg& prog = *list[j];

            prog.CountOverlaps(*this);
            prog.SetPriorityScore(starttime);
        }
    }
}

void ADVBProgList::PrioritizeProgrammes(ADVBProgList *schedulelists, uint64_t *recstarttimes, uint_t nlists, ADVBProgList& rejectedlist, const ADateTime& starttime)
{
    const ADVBConfig& config = ADVBConfig::Get();
    // for each programme, attach it to a list of repeats
    ADVBProg::PROGLISTLIST repeatlists;
    uint_t i, nscheduledmissed = 0, nearliestscheduled = 0, nscheduled = 0;

    for (i = 0; i < Count(); i++) {
        ADVBProg& prog = GetProgWritable(i);

        // no point checking anything that is already part of a list!
        if (!prog.GetList()) {
            ADVBProg::PROGLIST *list;

            // this programme MUST be part of a new list since it isn't already part of an existing list
            if ((list = new ADVBProg::PROGLIST) != NULL) {
                // dvbadd list to list of repeat lists
                repeatlists.push_back(list);

                // add this programme to the list
                prog.AddToList(list);

                // check all SUBSEQUENT programmes to see if they are repeats of this prog
                // and if so, add them to the list

                uint_t j;
                for (j = i + 1; j < Count(); j++) {
                    ADVBProg& prog1 = GetProgWritable(j);

                    if (ADVBProg::SameProgramme(prog, prog1)) {
                        // prog1 is the same programme as prog, add prog1 to the list
                        prog1.AddToList(list);
                    }
                }
            }
        }
    }

    // count overlaps for every programme
    CountOverlaps(repeatlists, starttime);

    Sort(&ADVBProg::SortListByScore);

    config.logit("--------------------------------------------------------------------------------");
    config.printf("Sorted: %u programmes:", Count());
    config.logit("--------------------------------------------------------------------------------");
    for (i = 0; i < Count(); i++) {
        const ADVBProg&           prog = GetProg(i);
        const ADVBProg::PROGLIST& list = *prog.GetList();
        uint_t entry = std::find(list.begin(), list.end(), &prog) - list.begin();

        config.logit("%s (entry %u/%u, %d overlaps, %u repeats, score %0.1lf)",
                     prog.GetDescription(1).str(),
                     entry,
                     (uint_t)list.size() - 1,
                     prog.GetOverlaps(),
                     (uint_t)prog.GetList()->size() - 1,
                     prog.GetPriorityScore());
    }
    config.logit("--------------------------------------------------------------------------------");

    // first round avoids programmes overlapping including their pre- and post- handles,
    // second round allows programmes to butt up against each other
    const uint_t nchecks = nlists * 2;  // two complete rounds
    for (i = 0; (Count() > 0); i++) {
        ADVBProg::PROGLIST deletelist;
        ADVBProg& prog = GetProgWritable(0);
        uint_t    j, k;
        bool      scheduled = false;

        // find a list to schedule this programme on
        for (j = 0; j < nchecks; j++) {
            const ADVBProg *overlappedprog = NULL;
            uint_t vcard = (i + j) % nlists;
            ADVBProgList& schedulelist = schedulelists[vcard];

            // card must be available (not ignored)
            // programme must start after list's rec start time and not overlap anything already on it
            // run twice over the scheduling list for each card:
            // once not allowing any overlapping of pre- and post- handles
            if (!config.IgnoreDVBCard(vcard) &&
                // first loop through only allow programmes to be recorded if their pre- and post- handles don't overlap
                (((j < nlists) &&                                                           // first round; and
                  (prog.GetRecordStart() >= recstarttimes[vcard]) &&                        // record start (including pre-handle) is after earliest record time; and
                  (((overlappedprog  = schedulelist.FindRecordOverlap(prog)) == NULL) ||    // this programme (including pre- and post- handles) does not overlap anything scheduled for this card; or
                   ((overlappedprog != NULL) &&                                             // it does overlap something on this card; but
                    !schedulelist.FindOverlap(prog) &&                                      // this programme (excluding pre- and post- handles) does not overlap anything scheduled for this card; and
                    (strcmp(overlappedprog->GetTitle(), prog.GetTitle()) == 0) &&           // both programmes are of the same title
                    (strcmp(overlappedprog->GetUser(),  prog.GetUser())  == 0)))) ||        // both programmes have the same user; or
                 // second loop through allow programmes to be recorded if their pre- and post- handles *do* overlap
                 ((j >= nlists) &&                                                          // second round; and
                  (prog.GetStart() >= recstarttimes[vcard]) &&                              // record start (excluding pre-handle) is after earliest record time; and
                  !schedulelist.FindOverlap(prog)))) {                                      // this programme (excluding pre- and post- handles) does not overlap anything scheduled for this card
                // this programme doesn't overlap anything else or anything scheduled -> this can definitely be recorded
                const ADVBProg::PROGLIST& list = *prog.GetList();
                uint_t entry = std::find(list.begin(), list.end(), &prog) - list.begin();

                if (entry == 0) nearliestscheduled++;

                // add to scheduling list
                schedulelist.AddProg(prog);
                deletelist.push_back(&prog);

                config.logit("'%s' can be recorded (card %u, entry %u/%u)%s",
                             prog.GetQuickDescription().str(),
                             vcard,
                             entry + 1,
                             (uint_t)list.size(),
                             (overlappedprog != NULL) ? AString(" (overlapped with %s)").Arg(overlappedprog->GetDescription()).str() : "");

                // remove any programmes that do NOT have the allow repeats flag set
                // OR that use the same base channel
                static const uint64_t hour = 3600UL * 1000UL;
                AString  channel = prog.GetBaseChannel();
                bool     plus1   = prog.IsPlus1();
                uint64_t start   = prog.GetStart() - (plus1 ? hour : 0);
                uint64_t start1  = start + hour;
                bool     resort  = false;

                for (k = 0; k < Count(); k++) {
                    ADVBProg& prog2 = GetProgWritable(k);
                    bool removed = false;

                    if ((prog2 != prog) && (prog2.GetList() == prog.GetList())) {
                        if (!prog2.AllowRepeats()) {
                            deletelist.push_back(&prog2);   // add to delete list that is used later to delete programme from this object
                            removed = true;
                        }
                        else if ((prog2.GetBaseChannel() == channel) &&
                                 (( prog2.IsPlus1() && (prog2.GetStart() == start1)) ||
                                  (!prog2.IsPlus1() && (prog2.GetStart() == start)))) {
                            config.logit("Removing '%s' because it is +/- 1 hour from '%s'", prog2.GetQuickDescription().str(), prog.GetQuickDescription().str());
                            deletelist.push_back(&prog2);   // add to delete list that is used later to delete programme from this object
                            removed = true;
                        }
                    }

                    // if the scheduled programme overlaps this programme *just* in terms
                    // of pre- and post- handles, bias the priority score and enable a re-sorting of the programme list
                    resort |= (!removed && prog2.BiasPriorityScore(prog));
                }

                if (resort) {
                    // need to sort programme a list because some priority scores have changed
                    Sort(&ADVBProg::SortListByScore);
                }

                nscheduled++;
                scheduled = true;
                break;
            }
        }

        // this repeat of the programme cannot be scheduled
        if (!scheduled) {
            const ADVBProg::PROGLIST *repeatlist = prog.GetList();

            // check to see if this repeat is the last one
            if (repeatlist->size() == 1) {
                // the last repeat overlaps something so cannot be recorded -> the programme is *rejected*
                config.logit("No repeats of '%s' can be recorded!", prog.GetQuickDescription().str());

                // add programme to rejected list
                rejectedlist.AddProg(prog);
            }
            else {
                config.logit("'%s' cannot be recorded, %u repeats to go", prog.GetQuickDescription().str(), (uint_t)prog.GetList()->size() - 1);
                nscheduledmissed++;
            }

            // remove programme from list to check
            deletelist.push_back(&prog);
        }

        for (j = 0; j < deletelist.size(); j++) {
            DeleteProg(*deletelist[j]);
        }
    }

    config.printf("Prioritization complete (%u programmes rejected, %u/%u scheduled at earliest time, %u primary entries not scheduled)", rejectedlist.Count(), nearliestscheduled, nscheduled, nscheduledmissed);

    for (i = 0; i < nlists; i++) {
        ADVBProgList& scheduledlist = schedulelists[i];

        // sort list
        scheduledlist.Sort();

        // tweak record times to prevent pre/post handles overlapping
        scheduledlist.AdjustRecordTimes(recstarttimes[i]);
    }

    for (i = 0; i < repeatlists.size(); i++) {
        delete repeatlists[i];
    }
}

uint_t ADVBProgList::ScheduleEx(const ADVBProgList& runninglist, ADVBProgList& allscheduledlist, ADVBProgList& allrejectedlist, const ADateTime& starttime)
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADVBProgList *schedulelists;
    uint64_t  *recstarttimes;
    AString   filename;
    uint64_t  recstarttime = (uint64_t)starttime;
    uint_t    i, j;

    // read DVB card mappings
    (void)config.GetPhysicalDVBCard(0, true);
    uint_t ncards = config.GetMaxDVBCards();

    if (!ncards) {
        config.printf("No DVB cards available! %u programmes left to schedule", Count());

        allrejectedlist = *this;
        return Count();
    }

    // allow at least 30s before first schedule point
    recstarttime += 30000;

    schedulelists = new ADVBProgList[ncards];
    recstarttimes = new uint64_t[ncards];
    for (i = 0; i < ncards; i++) {
        // set initial earliest schedule time
        recstarttimes[i] = recstarttime;
    }

    // for running list, adjust earliest scheduling time so as not to trampling on ongoing recordings
    if (runninglist.Count()) {
        for (i = 0; i < runninglist.Count(); i++) {
            const ADVBProg& prog = runninglist[i];
            uint_t vcard = prog.GetDVBCard();

            if (vcard < ncards) {
                recstarttimes[vcard] = std::max(recstarttimes[vcard], (uint64_t)(prog.GetRecordStop() + 10000));
            }
        }
    }

    // round all earliest schedule times to minute boundaries
    uint64_t minrecstarttime = 0;
    for (i = 0; i < ncards; i++) {
        if (!i || (recstarttimes[i] < minrecstarttime)) minrecstarttime = recstarttimes[i];

        config.printf("Earliest record start time for card %u (physical card %u) is %s", i, config.GetPhysicalDVBCard(i), ADateTime(recstarttimes[i]).UTCToLocal().DateToStr().str());
    }

    // generate record data, clear disabled flag and clear owner list for each programme
    for (i = 0; i < Count(); i++) {
        ADVBProg& prog = GetProgWritable(i);

        prog.GenerateRecordData(minrecstarttime);
        prog.ClearList();
    }

    config.logit("--------------------------------------------------------------------------------");
    config.printf("Requested: %u programmes:", Count());
    config.logit("--------------------------------------------------------------------------------");
    for (i = 0; i < Count(); i++) {
        config.logit("%s", GetProg(i).GetDescription(1).str());
    }
    config.logit("--------------------------------------------------------------------------------");

    //ADVBProg::debugsameprogramme = true;
    PrioritizeProgrammes(schedulelists, recstarttimes, ncards, allrejectedlist, starttime);
    //ADVBProg::debugsameprogramme = false;

    for (i = 0; i < ncards; i++) {
        ADVBProgList& scheduledlist = schedulelists[i];
        const ADVBProg *errorprog;
        uint_t dvbcard = config.GetPhysicalDVBCard(i);

        if ((errorprog = scheduledlist.FindFirstRecordOverlap()) != NULL) {
            config.printf("Error: found overlap in schedule list for card %u (hardware card %u) ('%s')!", i, dvbcard, errorprog->GetQuickDescription().str());
        }

        config.logit("--------------------------------------------------------------------------------");
        config.printf("Card %u (hardware card: %u): Scheduling: %u programmes:", i, dvbcard, scheduledlist.Count());
        config.logit("--------------------------------------------------------------------------------");
        for (j = 0; j < scheduledlist.Count(); j++) {
            ADVBProg& prog = scheduledlist.GetProgWritable(j);
            const ADVBProg *rprog;

            prog.SetScheduled();
            prog.SetDVBCard(i);

            allscheduledlist.AddProg(prog, false);

            while ((rprog = allrejectedlist.FindSimilar(prog)) != NULL) {
                config.logit("Removing '%s' from rejected list because it is going to be recorded ('%s')",
                             rprog->GetQuickDescription().str(),
                             prog.GetQuickDescription().str());

                allrejectedlist.DeleteProg(*rprog);
            }

            config.logit("%s (%s - %s)",
                         prog.GetDescription(1).str(),
                         prog.GetRecordStartDT().UTCToLocal().DateFormat("%d %D-%N-%Y %h:%m:%s").str(),
                         prog.GetRecordStopDT().UTCToLocal().DateFormat("%h:%m:%s").str());
        }
        config.logit("--------------------------------------------------------------------------------");
    }

    delete[] recstarttimes;
    delete[] schedulelists;

    allscheduledlist.Sort();
    allrejectedlist.Sort();

    config.logit("--------------------------------------------------------------------------------");
    config.printf("Scheduling: %u programmes in total:", allscheduledlist.Count());
    config.logit("--------------------------------------------------------------------------------");
    for (i = 0; i < allscheduledlist.Count(); i++) {
        const ADVBProg& prog = allscheduledlist.GetProg(i);

        config.logit("%s (%s - %s) (DVB card %u, hardware card %u)",
                     prog.GetDescription(1).str(),
                     prog.GetRecordStartDT().UTCToLocal().DateFormat("%d %D-%N-%Y %h:%m:%s").str(),
                     prog.GetRecordStopDT().UTCToLocal().DateFormat("%h:%m:%s").str(),
                     prog.GetDVBCard(),
                     config.GetPhysicalDVBCard(prog.GetDVBCard()));
    }
    config.logit("--------------------------------------------------------------------------------");

    return allrejectedlist.Count();
}

bool ADVBProgList::WriteToJobList()
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADateTime dt;
    const AString filename = config.GetScheduledFile();
    bool success = false;

    if (config.GetRecordingSlave().Valid()) {
        ADVBProgList scheduledlist;
        ADVBProgList unscheduledlist;
        uint_t tries;
        uint_t i;

        for (tries = 0; (tries < 3) && !success; tries++) {
            unscheduledlist.DeleteAll();

            if (tries) config.printf("Scheduling try %u/3:", tries + 1);

            success = (SendFileRunRemoteCommand(filename, "dvb --write-scheduled-jobs") &&
                       scheduledlist.ModifyFromRecordingSlave(config.GetScheduledFile(), ADVBProgList::Prog_ModifyAndAdd));

            if (!success) config.printf("Remote scheduling failed!");

            for (i = 0; i < scheduledlist.Count(); i++) {
                if (!scheduledlist[i].GetJobID()) unscheduledlist.AddProg(scheduledlist[i]);
            }

            if (success && !scheduledlist.Count()) break;
        }

        if (unscheduledlist.Count()) {
            AStdFile fp;
            AString  filename = config.GetTempFile("unscheduled", ".txt");
            AString  cmd;

            config.printf("--------------------------------------------------------------------------------");
            config.printf("%u programmes unscheduled:", unscheduledlist.Count());
            for (i = 0; i < unscheduledlist.Count(); i++) {
                config.printf("%s", unscheduledlist[i].GetQuickDescription().str());
            }
            config.printf("--------------------------------------------------------------------------------");

            success = false;

            if ((cmd = config.GetConfigItem("schedulefailurecmd", "")).Valid()) {
                if (fp.open(filename, "w")) {
                    fp.printf("The following programmes have NOT been scheduled:\n");
                    for (i = 0; i < unscheduledlist.Count(); i++) {
                        fp.printf("%s\n", unscheduledlist[i].GetDescription(config.GetScheduleReportVerbosity("schedulefailed")).str());
                    }
                    fp.close();

                    cmd = cmd.SearchAndReplace("{logfile}", filename);
                    RunAndLogCommand(cmd);

                    remove(filename);
                }
                else config.printf("Failed to open text file for logging unscheduled!");
            }
        }
    }
    else {
        ADVBProgList scheduledlist, runninglist;

        runninglist.ReadFromJobQueue('=');

        UnscheduleAllProgrammes();

        if (scheduledlist.ReadFromFile(filename)) {
            uint_t i, nsucceeded = 0, nfailed = 0;

            config.logit("--------------------------------------------------------------------------------");
            config.printf("Writing %u programmes to job list (%u job(s) running)", scheduledlist.Count(), runninglist.Count());

            success = true;
            for (i = 0; i < scheduledlist.Count(); i++) {
                const ADVBProg *rprog;
                ADVBProg& prog = scheduledlist.GetProgWritable(i);

                if ((rprog = runninglist.FindUUID(prog)) != NULL) {
                    prog.SetJobID(rprog->GetJobID());

                    config.logit("Recording of '%s' already running (job %u)!", prog.GetDescription().str(), prog.GetJobID());
                }
                else if (prog.WriteToJobQueue()) {
                    config.logit("Scheduled '%s' okay, job %u", prog.GetDescription().str(), prog.GetJobID());

                    nsucceeded++;
                }
                else {
                    nfailed++;
                    success = false;
                }
            }

            config.logit("--------------------------------------------------------------------------------");

            config.printf("Scheduled %u programmes successfully (%u failed), writing to '%s'", nsucceeded, nfailed, filename.str());

            if (!scheduledlist.WriteToFile(filename, false)) {
                config.logit("Failed to write updated schedule list '%s'", filename.str());
                success = false;
            }
        }
    }

    if (!success) {
        AString cmd;

        if ((cmd = config.GetConfigItem("schedulefailurecmd")).Valid()) {
            AString logfile = config.GetTempFile("schedulefailure", ".txt");

            config.ExtractLogData(dt, ADateTime(), logfile);

            cmd = cmd.SearchAndReplace("{logfile}", logfile);
            RunAndLogCommand(cmd);

            remove(logfile);
        }
    }

    return success;
}

bool ADVBProgList::CreateCombinedFile()
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADVBLock     lock("dvbfiles");
    ADVBProgList list;
    bool         success = false;

    config.printf("Creating combined listings");

    if (list.ReadFromBinaryFile(config.GetRecordedFile())) {
        if ((uint_t)config.GetConfigItem("trimcombinedfile", "1")) {
            ADateTime dt("utc midnight-" + config.GetConfigItem("trimcombinedfiletime", "1y"));

            config.printf("Deleting all programmes that started before %s", dt.DateToStr().str());

            while ((list.Count() > 0) && (list[0].GetStartDT() < dt)) {
                list.DeleteProg(0);
            }
        }

        list.EnhanceListings();
        if (list.WriteToFile(config.GetCombinedFile())) {
            success = true;
        }
        else config.logit("Failed to write combined listings file");
    }
    else config.logit("Failed to read recorded programmes list for generating combined file");

    return success;
}

bool ADVBProgList::CreateGraphs(const AString& _graphsuffix)
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADVBProgList recordedlist, scheduledlist;
    const ADateTime dt;
    const AString graphsuffix      = _graphsuffix.Valid() ? _graphsuffix : config.GetGraphSuffix();
    const AString graphfileall     = config.GetDataDir().CatPath("graphs", "graph-all." + graphsuffix);
    const AString graphfile6months = config.GetDataDir().CatPath("graphs", "graph-6months." + graphsuffix);
    const AString graphfile1week   = config.GetDataDir().CatPath("graphs", "graph-1week." + graphsuffix);
    const AString graphfilepreview = config.GetDataDir().CatPath("graphs", "graph-preview." + graphsuffix);
    bool success = false;

    {
        ADVBLock lock("dvbfiles");
        FILE_INFO info1, info2, info3, info4;
        ADateTime writetime;

        if (!::GetFileInfo(graphfileall, &info1) ||
            !::GetFileInfo(graphfile6months, &info2) ||
            !::GetFileInfo(graphfile1week, &info3) ||
            !::GetFileInfo(graphfilepreview, &info4) ||
            (((writetime = std::min(std::min(info1.WriteTime, info2.WriteTime),
                                    std::min(info3.WriteTime, info4.WriteTime))) > ADateTime::MinDateTime) &&
             ((::GetFileInfo(config.GetRecordedFile(), &info1) && (info1.WriteTime > writetime)) ||
              (::GetFileInfo(config.GetScheduledFile(), &info1) && (info1.WriteTime > writetime))))) {
            recordedlist.ReadFromBinaryFile(config.GetRecordedFile());
            recordedlist.ReadFromBinaryFile(config.GetScheduledFile());
            scheduledlist.ReadFromBinaryFile(config.GetScheduledFile());
        }
    }

    if (recordedlist.Count() > 0) {
        const AString datfile  = config.GetTempFile("graph", ".dat");
        const AString gnpfile  = config.GetTempFile("graph", ".gnp");
        ADateTime firstdate    = ADateTime::MinDateTime;
        ADateTime firstrecdate = ADateTime::MinDateTime;
        ADateTime lastrecdate  = ADateTime::MinDateTime;
        ADateTime firstschdate = ADateTime::MinDateTime;
        ADateTime lastschdate  = ADateTime::MinDateTime;

        config.printf("Updating graphs...");

        success = CreateDirectory(graphfileall.PathPart());
        if (!success) {
            config.printf("Create directroy '%s' failed", graphfileall.PathPart().str());
        }

        AStdFile fp;
        if (fp.open(datfile, "w")) {
            uint_t i;

            scheduledlist.CreateHash();
            for (i = 0; i < recordedlist.Count(); i++) {
                const ADVBProg& prog = recordedlist.GetProg(i);

                fp.printf("%s", prog.GetStartDT().DateFormat("%D-%N-%Y %h:%m").str());

                if (firstdate == ADateTime::MinDateTime) firstdate = prog.GetStartDT();

                if (prog.IsRecorded()) {
                    if (firstrecdate == ADateTime::MinDateTime) firstrecdate = prog.GetStartDT();
                    lastrecdate = prog.GetStartDT();
                    fp.printf(" %u", i);
                }
                else fp.printf(" -");

                if (prog.IsScheduled() || scheduledlist.FindUUID(prog)) {
                    if (firstschdate == ADateTime::MinDateTime) firstschdate = prog.GetStartDT();
                    lastschdate = prog.GetStartDT();
                    fp.printf(" %u", i);
                }
                else fp.printf(" -");

                fp.printf("\n");
            }

            fp.close();
        }
        else {
            config.printf("Failed to open graph data file '%s' for writing", datfile.str());
            success = false;
        }

        if (fp.open(gnpfile, "w")) {
            const ADateTime startdate("utc now-6M");
            const ADateTime enddate("utc now+2w+3d");
            const double    pretime = 2.0 * 24.0 * 3600.0;
            TREND allrectrend = recordedlist.CalculateTrend(firstrecdate, lastrecdate);
            TREND rectrend = recordedlist.CalculateTrend(startdate, lastrecdate);
            TREND schtrend = recordedlist.CalculateTrend(firstschdate, lastschdate);

            if (graphsuffix == "png") {
                fp.printf("set terminal pngcairo size 1280,800\n");
            }
            else if (graphsuffix == "svg") {
                fp.printf("set terminal svg enhanced mouse standalone size 1280,800\n");
            }
            fp.printf("set xdata time\n");
            fp.printf("set timefmt '%%d-%%b-%%Y %%H:%%M'\n");
            fp.printf("set autoscale xy\n");
            fp.printf("set grid\n");
            fp.printf("set xtics rotate by -90 format '%%d-%%b-%%Y'\n");
            fp.printf("set key inside bottom right\n");
            fp.printf("trend(x,a,b,offset,rate,timeoffset)=((x>=a)&&(x<=b))?(offset+rate*(x-timeoffset)/(3600.0*24.0)):1/0\n");

            fp.printf("set output '%s'\n", graphfileall.str());
            fp.printf("set title 'Recording Rate (all) - %s'\n", dt.DateToStr().str());
            fp.printf("set yrange [0:*]\n");
            fp.printf("set xrange ['%s':'%s']\n", firstdate.DateFormat("%D-%N-%Y").str(), enddate.DateFormat("%D-%N-%Y").str());
            fp.printf("plot \\\n");
            fp.printf("'%s' using 1:3 with lines lt 1 title 'Recorded', \\\n", datfile.str());
            fp.printf("trend(x,%lf,%lf,%0.14le,%0.14le,%0.14le) with lines lt 2 title '%0.2lf Recorded/day'\n",
                      (double)firstrecdate.totime(), (double)enddate.totime(), allrectrend.offset, allrectrend.rate, allrectrend.timeoffset, allrectrend.rate);
            fp.printf("unset output\n");

            fp.printf("set output '%s'\n", graphfile6months.str());
            fp.printf("set title 'Recording Rate (6 months) - %s'\n", dt.DateToStr().str());
            fp.printf("set autoscale y\n");
            fp.printf("set xrange ['%s':'%s']\n", startdate.DateFormat("%D-%N-%Y").str(), enddate.DateFormat("%D-%N-%Y").str());
            fp.printf("set key inside bottom right\n");
            fp.printf("plot \\\n");
            fp.printf("'%s' using 1:3 with lines lt 1 title 'Recorded', \\\n", datfile.str());
            fp.printf("'%s' using 1:4 with lines lt 7 title 'Scheduled', \\\n", datfile.str());
            fp.printf("trend(x,%lf,%lf,%0.14le,%0.14le,%0.14le) with lines lt 3 title '%0.2lf Recorded/day', \\\n",
                      (double)startdate.totime(), (double)enddate.totime(), rectrend.offset, rectrend.rate, rectrend.timeoffset, rectrend.rate);
            fp.printf("trend(x,%lf,%lf,%0.14le,%0.14le,%0.14le) with lines lt 4 title '%0.2lf Scheduled/day'\n",
                      (double)firstschdate.totime() - pretime, (double)enddate.totime(), schtrend.offset, schtrend.rate, schtrend.timeoffset, schtrend.rate);
            fp.printf("unset output\n");

            fp.printf("set title 'Recording Rate (1 week) - %s'\n", dt.DateToStr().str());
            fp.printf("set output '%s'\n", graphfile1week.str());
            fp.printf("set xrange ['%s':'%s']\n", ADateTime("utc now-1M").DateFormat("%D-%N-%Y").str(), ADateTime("utc now+2w+3d").DateFormat("%D-%N-%Y").str());
            fp.printf("replot\n");
            fp.printf("unset output\n");

            if (graphsuffix == "png") {
                fp.printf("set terminal pngcairo size 640,480\n");
            }
            else if (graphsuffix == "svg") {
                fp.printf("set terminal svg enhanced mouse standalone size 640,480\n");
            }
            fp.printf("set output '%s'\n", graphfilepreview.str());
            fp.printf("replot\n");
            fp.close();

            if (system("gnuplot " + gnpfile) == 0) {
                const AString datesuffix = dt.DateFormat("-%Y-%M-%D." + graphsuffix);
                const AString copyfiles[] = {
                    graphfileall,
                    graphfile6months,
                    graphfile1week,
                };
                uint_t i;

                for (i = 0; i < NUMBEROF(copyfiles); i++) {
                    AString destfilename = copyfiles[i].Prefix() + datesuffix;

                    if (!AStdFile::exists(destfilename)) {
                        CopyFile(copyfiles[i], destfilename);
                    }
                }
            }
            else config.printf("gnuplot failed");

            remove(datfile);
            remove(gnpfile);
        }
        else {
            config.printf("Failed to open graph instruction file '%s' for writing", gnpfile.str());
            success = false;
        }
    }
    else {
        config.printf("Graph update not required");
        success = true;
    }

    return success;
}


void ADVBProgList::EnhanceListings()
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADVBProgList list;
    uint_t added, modified;

    CreateHash();

    list.DeleteAll();
    if (list.ReadFromBinaryFile(config.GetScheduledFile())) {
        Modify(list, added, modified, Prog_Add);
    }
    else config.logit("Failed to read scheduled programme list for enhancing listings");

    list.DeleteAll();
    if (list.ReadFromBinaryFile(config.GetRecordingFile())) {
        Modify(list, added, modified, Prog_ModifyAndAdd);
    }
    else config.logit("Failed to read running programme list for enhancing listings");

    list.DeleteAll();
    if (list.ReadFromBinaryFile(config.GetProcessingFile())) {
        Modify(list, added, modified, Prog_ModifyAndAdd);
    }
    else config.logit("Failed to read processing programme list for enhancing listings");

    list.DeleteAll();
    if (list.ReadFromBinaryFile(config.GetRejectedFile())) {
        Modify(list, added, modified, Prog_ModifyAndAdd);
    }
    else config.logit("Failed to read rejected programme list for enhancing listings");
}

void ADVBProgList::CheckRecordingFile()
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADVBLock     lock("dvbfiles");
    ADVBProgList list;
    uint_t i;

    //config.logit("Creating combined list");

    if (list.ReadFromBinaryFile(config.GetRecordingFile())) {
        uint64_t now     = (uint64_t)ADateTime().TimeStamp(true);
        bool     changed = false;

        for (i = 0; i < list.Count();) {
            const ADVBProg& prog = list.GetProg(i);

            if (now >= (prog.GetRecordStop() + 60000)) {
                config.printf("Programme '%s' should have stopped recording by now, removing it from the running list", prog.GetQuickDescription().str());
                list.DeleteProg(i);
                changed = true;
            }
            else i++;
        }

        if (changed) {
            config.logit("Running list changed, writing new version");
            list.WriteToFile(config.GetRecordingFile());
        }
    }
    else config.logit("Failed to read running programme list for checking");
}

bool ADVBProgList::GetAndConvertRecordings()
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString  cmd;
    uint32_t tick;
    bool     success = false;

    {
        //config.printf("Waiting for lock to update lists from record host '%s'", config.GetRecordingSlave().str());

        ADVBLock lock("dvbfiles");

        config.printf("Update recording and recorded lists from record host '%s'", config.GetRecordingSlave().str());

        success = GetRecordingListFromRecordingSlave();

        ADVBProgList reclist;
        if (!reclist.ModifyFromRecordingSlave(config.GetRecordedFile(), ADVBProgList::Prog_Add)) {
            config.logit("Failed to read recorded list from recording slave");
            success = false;
        }
    }

    {
        //config.printf("Waiting for lock to get and convert recordings from record host '%s'", config.GetRecordingSlave().str());

        ADVBLock lock("copyfiles");

        config.printf("Getting and converting recordings from record host '%s'", config.GetRecordingSlave().str());

        CreateDirectory(config.GetRecordingsStorageDir());

        tick = GetTickCount();
        cmd.Delete();
        cmd.printf("nice rsync -v --partial --remove-source-files --ignore-missing-args %s %s%s %s:%s/'*.mpg' %s",
                   config.GetRsyncArgs().str(),
                   config.GetRsyncBandwidthLimit().Valid() ? "--bwlimit=" : "",
                   config.GetRsyncBandwidthLimit().str(),
                   config.GetRecordingSlave().str(),
                   config.GetRecordingsStorageDir().str(),
                   config.GetRecordingsStorageDir().str());

        if (RunAndLogCommand(cmd)) config.printf("File copy took %ss", AValue((GetTickCount() + 999 - tick) / 1000).ToString().str());
        else                       config.printf("Warning: Failed to copy all recorded programmes from recording host");

        CreateDirectory(config.GetSlaveLogDir());

        tick = GetTickCount();
        cmd.Delete();
        cmd.printf("nice rsync -z --partial --ignore-missing-args %s %s%s %s:%s/'dvb*.txt' %s",
                   config.GetRsyncArgs().str(),
                   config.GetRsyncBandwidthLimit().Valid() ? "--bwlimit=" : "",
                   config.GetRsyncBandwidthLimit().str(),
                   config.GetRecordingSlave().str(),
                   config.GetLogDir().str(),
                   config.GetSlaveLogDir().str());

        if (RunAndLogCommand(cmd)) config.printf("Log file copy took %ss", AValue((GetTickCount() + 999 - tick) / 1000).ToString().str());
        else                       config.printf("Warning: Failed to copy all DVB logs from recording host");
    }

    //config.printf("Waiting for lock to convert recordings");

    ADVBProgList failureslist;
    {
        ADVBLock lock("dvbfiles");
        failureslist.ReadFromBinaryFile(config.GetRecordFailuresFile());
    }

    ADVBLock lock("convertrecordings");
    uint_t i, converted = 0;
    bool   reschedule = false;

    // re-read recorded list in case it has been modified whilst files have been copying
    ADVBProgList reclist;
    reclist.ReadFromBinaryFile(config.GetRecordedFile());

    config.printf("Converting recordings...");

    ADVBProgList convertlist;
    for (i = 0; i < reclist.Count(); i++) {
        const ADVBProg& prog = reclist.GetProg(i);

        if (!prog.IsConverted() &&
            AStdFile::exists(prog.GetFilename())) {
            uint_t failures;

            if ((failures = failureslist.CountOccurances(prog)) < 2) {
                if (prog.IsOnceOnly() && prog.IsRecordingComplete() && !config.IsRecordingSlave()) {
                    if (ADVBPatterns::DeletePattern(prog.GetUser(), prog.GetPattern())) {
                        const bool rescheduleoption = config.RescheduleAfterDeletingPattern(prog.GetUser(), prog.GetCategory());

                        config.printf("Deleted pattern '%s', %srescheduling...", prog.GetPattern(), rescheduleoption ? "" : "NOT ");

                        reschedule |= rescheduleoption;
                    }
                }

                convertlist.AddProg(prog);
            }
            else config.printf("*NOT* converting %s - failed %u times already", prog.GetQuickDescription().str(), failures);
        }
    }

    if (reschedule) ADVBProgList::SchedulePatterns();

    for (i = 0; i < convertlist.Count(); i++) {
        ADVBProg& prog = convertlist.GetProgWritable(i);

        config.printf("Converting file %u/%u - '%s':", i + 1, reclist.Count(), prog.GetQuickDescription().str());

        if (prog.ConvertVideo(true)) {
            converted++;
        }
        else success = false;
    }

    // don't need lock for the next bit
    lock.ReleaseLock();

    if (converted) {
        std::map<AString, bool> userlist;
        std::map<AString, bool>::iterator it;

        config.printf("%u programmes converted", converted);

        // add empty user (for all users)
        userlist[""] = true;

        // create list of users affected by this round of conversion
        for (i = 0; i < convertlist.Count(); i++) {
            const ADVBProg& prog = convertlist.GetProg(i);

            userlist[prog.GetUser()] = true;
        }

        for (it = userlist.begin(); it != userlist.end(); ++it) {
            AString user = it->first;
            AString cmd;

            // get global or user specific command
            if ((cmd = config.GetUserConfigItem(user, "convertednotifycmd")).Valid()) {
                AString  filename = config.GetLogDir().CatPath("converted-list-" + ADateTime().DateFormat("%Y-%M-%D-%h-%m-%s") + ".txt");
                AStdFile fp;

                if (fp.open(filename, "w")) {
                    fp.printf("Converted files");

                    if (user.Valid()) fp.printf(" (for %s)", user.str());

                    fp.printf(":\n");

                    for (i = 0; i < convertlist.Count(); i++) {
                        const ADVBProg& prog = convertlist.GetProg(i);

                        if (user.Empty() || (user == prog.GetUser())) {
                            fp.printf("%s\n", prog.GetDescription((uint_t)config.GetUserConfigItem(user, "convertednotifyverbosity", "1")).str());
                        }
                    }

                    fp.close();

                    cmd = cmd.SearchAndReplace("{logfile}", filename);

                    RunAndLogCommand(cmd);
                }
            }
        }
    }

    //config.printf("Converted recordings, releasing lock");

    return success;
}

bool ADVBProgList::GetRecordingListFromRecordingSlave()
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADVBLock     lock("dvbfiles");
    ADVBProgList failurelist;
    ADateTime    recordingwritetime = ADateTime::MinDateTime;
    ADateTime    combinedwritetime  = ADateTime::MinDateTime;
    FILE_INFO    info;
    bool         success = true, update = false;

    config.printf("Updating recording list from record host '%s'", config.GetRecordingSlave().str());

    if (::GetFileInfo(config.GetCombinedFile(), &info)) combinedwritetime = info.WriteTime;

    if (!failurelist.ModifyFromRecordingSlave(config.GetRecordFailuresFile(), Prog_Add)) {
        config.printf("Failed to get and modify failures list");
        success = false;
    }

    if (::GetFileInfo(config.GetRecordFailuresFile(), &info)) update |= (info.WriteTime > combinedwritetime);

    // save writetime of existing recordings file
    if (::GetFileInfo(config.GetRecordingFile(), &info)) recordingwritetime = info.WriteTime;

    if (GetFileFromRecordingSlave(config.GetRecordingFile())) {
        // force update of combined if recordings file has been updated
        update |= (::GetFileInfo(config.GetRecordingFile(), &info) && (info.WriteTime > recordingwritetime));
    }
    else {
        config.printf("Failed to get recording list");
        success = false;
    }

    // if recordings file is newer than combined file, update it
    if (::GetFileInfo(config.GetRecordingFile(), &info)) update |= (info.WriteTime > combinedwritetime);

    if (update) {
        config.printf("Updating combined listings because a contributory file has changed");
        success &= ADVBProgList::CreateCombinedFile();
        CreateGraphs();
    }

    return success;
}

bool ADVBProgList::CheckRecordingNow()
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADVBLock     lock("dvbfiles");
    ADVBProgList scheduledlist, recordinglist;
    bool         success = false;

    if (config.GetRecordingSlave().Valid() && !GetRecordingListFromRecordingSlave()) {
        AString cmd;

        if ((cmd = config.GetConfigItem("recordingcheckcmd", "")).Valid()) {
            AStdFile fp;
            AString  filename = config.GetTempFile("recordingcheck", ".txt");

            if (fp.open(filename, "w")) {
                fp.printf("Recording check failed: host '%s' inaccessible\n", config.GetRecordingSlave().str());
                fp.close();

                cmd = cmd.SearchAndReplace("{logfile}", filename);
                RunAndLogCommand(cmd);

                remove(filename);
            }
            else config.printf("Failed to open text file for logging recording errors!");
        }

        return false;
    }

    if (recordinglist.ReadFromFile(config.GetRecordingFile())) {
        recordinglist.CreateHash();

        if (scheduledlist.ReadFromFile(config.GetScheduledFile())) {
            ADVBProgList   shouldberecordinglist, shouldntberecordinglist;
            const uint64_t now = (uint64_t)ADateTime().TimeStamp(true), slack = (uint64_t)2 * (uint64_t)60000;
            uint_t i;

            lock.ReleaseLock();

            success = true;

            for (i = 0; i < scheduledlist.Count(); i++) {
                const ADVBProg& prog = scheduledlist[i];

                if ((now >= (prog.GetRecordStart() + slack)) && (now < prog.GetRecordStop())) {
                    if (!recordinglist.FindUUID(prog)) shouldberecordinglist.AddProg(prog);
                }
                else if ((now < prog.GetRecordStart()) || (now > (prog.GetRecordStop() + slack))) {
                    if (recordinglist.FindUUID(prog)) shouldntberecordinglist.AddProg(prog);
                }
            }

            if (shouldberecordinglist.Count() || shouldntberecordinglist.Count()) {
                AString report;

                if (shouldberecordinglist.Count()) {
                    report.printf("--------------------------------------------------------------------------------\n");
                    report.printf("%u programmes should be recording:\n", shouldberecordinglist.Count());

                    for (i = 0; i < shouldberecordinglist.Count(); i++) {
                        const ADVBProg& prog = shouldberecordinglist[i];

                        report.printf("%s\n", prog.GetQuickDescription().str());
                    }
                    report.printf("--------------------------------------------------------------------------------\n");

                    if (shouldntberecordinglist.Count()) report.printf("\n");
                }

                if (shouldntberecordinglist.Count()) {
                    report.printf("--------------------------------------------------------------------------------\n");
                    report.printf("%u programmes shouldn't be recording:\n", shouldntberecordinglist.Count());

                    for (i = 0; i < shouldntberecordinglist.Count(); i++) {
                        const ADVBProg& prog = shouldntberecordinglist[i];

                        report.printf("%s\n", prog.GetQuickDescription().str());
                    }
                    report.printf("--------------------------------------------------------------------------------\n");
                }

                config.printf("%s", report.str());

                AString cmd;
                if ((cmd = config.GetConfigItem("recordingcheckcmd", "")).Valid()) {
                    AStdFile fp;
                    AString  filename = config.GetTempFile("recordingcheck", ".txt");

                    if (fp.open(filename, "w")) {
                        fp.printf("%s", report.str());
                        fp.close();

                        cmd = cmd.SearchAndReplace("{logfile}", filename);
                        RunAndLogCommand(cmd);

                        remove(filename);
                    }
                    else config.printf("Failed to open text file for logging recording errors!");
                }
            }
        }
        else config.printf("Failed to read scheduled list");
    }
    else config.printf("Failed to read recording list");

    return success;
}

void ADVBProgList::FindSeries(SERIESLIST& serieslist) const
{
    uint_t i;

    for (i = 0; i < Count(); i++) {
        const ADVBProg&          prog    = GetProg(i);
        const ADVBProg::EPISODE& episode = prog.GetEpisode();

        if (episode.valid && episode.episode) {
            uint_t ser = episode.series;
            uint_t epn = episode.episode;
            uint_t ept = episode.episodes;

#if 0
            if (!ser) {
                if (epn >= 100) ept = 100;
                ser = ((epn - 1) / 100);
                epn = 1 + ((epn - 1) % 100);
            }
#endif

            SERIES& series = serieslist[prog.GetTitle()];
            if (series.title.Empty()) series.title = prog.GetTitle();

            if (ser >= series.list.size()) series.list.resize(ser + 1);

            AString& str = series.list[ser];
            int len = (int)std::max(ept, epn);
            if (str.len() < len) str += AString("-").Copies(len - str.len());

            uint_t ind = epn - 1;
            char t = str[ind], t1 = t;

            if      (prog.IsScheduled())          t1 = 's';
            else if (prog.HasRecordFailed())      t1 = 'f';
            else if (prog.HasPostProcessFailed()) t1 = 'f';
            else if (prog.IsAvailable())          t1 = 'a';
            else if (prog.IsRecorded())           t1 = 'r';
            else if (prog.IsRejected())           t1 = 'x';

            static const char *allowablechanges[] = {
                "-sfarx",
                "sfar",
                "fsrax",
                "asx",
                "rasx",
                "xsfar",
            };
            uint_t i;
            for (i = 0; i < NUMBEROF(allowablechanges); i++) {
                if ((t == allowablechanges[i][0]) && strchr(allowablechanges[i] + 1, t1)) {
                    t = t1;
                    break;
                }
            }

            if (t != str[ind]) {
                str = str.Left(ind) + AString(t) + str.Mid(ind + 1);
            }
        }
    }
}

double ADVBProgList::ScoreProgrammeByPopularityFactors(const ADVBProg& prog, void *context)
{
    const POPULARITY_FACTORS& factors = *(const POPULARITY_FACTORS *)context;
    double score = 0.0;

    if (prog.IsRecorded())  score += factors.recordedfactor;
    if (prog.IsScheduled()) score += factors.scheduledfactor;
    if (prog.IsRejected())  score += factors.rejectedfactor;
    score *= factors.priorityfactor * pow(2.0, (double)prog.GetPri());

    if (factors.userfactors.Exists(prog.GetUser())) {
        score *= (double)factors.userfactors.Read(prog.GetUser());
    }

    return score;
}

bool ADVBProgList::__CollectPopularity(const AString& key, uptr_t item, void *context)
{
    AList& list = *(AList *)context;

    UNUSED(item);

    list.Add(new AString(key));

    return true;
}

int  ADVBProgList::__ComparePopularity(const AListNode *pNode1, const AListNode *pNode2, void *pContext)
{
    const AHash&      hash   = *(const AHash *)pContext;
    const AString&    title1 = *AString::Cast(pNode1);
    const AString&    title2 = *AString::Cast(pNode2);
    const POPULARITY& pop1   = *(const POPULARITY *)hash.Read(title1);
    const POPULARITY& pop2   = *(const POPULARITY *)hash.Read(title2);
    int res = 0;

    if (pop2.score > pop1.score) res =  1;
    if (pop2.score < pop1.score) res = -1;

    if (res == 0) res = stricmp(title1, title2);

    return res;
}

void ADVBProgList::FindPopularTitles(AList& list, double (*fn)(const ADVBProg& prog, void *context), void *context) const
{
    AHash hash(&__DeletePopularity);
    uint_t i;

    for (i = 0; i < Count(); i++) {
        const ADVBProg& prog  = GetProg(i);
        const AString&  title = prog.GetTitle();
        POPULARITY      *pop  = (POPULARITY *)hash.Read(title);

        if (!pop && ((pop = new POPULARITY) != NULL)) {
            memset(pop, 0, sizeof(*pop));

            hash.Insert(title, (uptr_t)pop);
        }

        if (pop) {
            pop->count++;
            pop->score += (*fn)(prog, context);
        }
    }

    hash.Traverse(&__CollectPopularity, &list);
    list.Sort(&__ComparePopularity, &hash);

#if 0
    const AString *str = AString::Cast(list.First());
    while (str) {
        const POPULARITY& pop = *(const POPULARITY *)hash.Read(*str);

        debug("Title '%s' count %u score %u\n", str->str(), pop.count, pop.score);

        str = str->Next();
    }
#endif
}

ADVBProgList::TREND ADVBProgList::CalculateTrend(const ADateTime& startdate, const ADateTime& enddate) const
{
    TREND    trend;
    uint64_t start = (uint64_t)startdate;
    uint64_t end   = (uint64_t)enddate;
    uint_t   i;

    memset(&trend, 0, sizeof(trend));

    if (Count() > 0) {
        for (i = Count() - 1; (i > 0) && (GetProg(i).GetStart() >= start); i-- ) ;
        if (GetProg(i).GetStart() < start) i++;

        if (i < Count()) {
            const uint_t n1 = i;
            const double t0 = (double)GetProg(i).GetStartDT().totime();
            double sumx  = 0.0;
            double sumy  = 0.0;
            double sumxx = 0.0;
            double sumxy = 0.0;

            for (; (i < Count()) && (GetProg(i).GetStart() <= end); i++) {
                double x = ((double)GetProg(i).GetStartDT().totime() - t0) / (3600.0 * 24.0);
                double y = (double)(i - n1);

                sumx  += x;
                sumy  += y;
                sumxx += x * x;
                sumxy += x * y;
            }

            // m = (mean(x*y) - mean(x) * mean(y)) / (mean(x*x) - mean(x) * mean(x))
            //   = (sum(x*y)/n - sum(x)/n * sum(y)/n) / (sum(x*x)/n - sum(x)/n * sum(x)/n)
            //   = (sum(x*y)*n - sum(x) * sum(y)) / (sum(x*x)*n - sum(x) * sum(x))
            // c = mean(y) - m * mean(x)
            //   = sum(y)/n - m * sum(x)/n
            //   = (sum(y) - m * sum(x)) / n

            const double n = (double)(i - n1);
            trend.rate       = (sumxy * n - sumx * sumy) / (sumxx * n - sumx * sumx);
            trend.offset     = (double)n1 + (sumy - trend.rate * sumx) / n;
            trend.timeoffset = t0;
            trend.valid      = true;
        }
    }

    return trend;
}

ADVBProgList::TIMEGAP ADVBProgList::FindGaps(const ADateTime& start, std::vector<TIMEGAP>& gaps, const std::map<uint_t,bool> *cardstoavoid) const
{
    const ADVBConfig& config = ADVBConfig::Get();
    std::vector<std::vector<const ADVBProg *> > lists;
    TIMEGAP res = {start, start, ~0U};
    uint_t i;

    config.GetPhysicalDVBCard(0);
    lists.resize(config.GetMaxDVBCards());
    gaps.resize(config.GetMaxDVBCards());

    for (i = 0; i < Count(); i++) {
        const ADVBProg& prog = GetProg(i);

        if (prog.GetDVBCard() >= (uint_t)lists.size()) {
            lists.resize(prog.GetDVBCard() + 1);
            gaps.resize(lists.size());
        }

        if (prog.GetDVBCard() < (uint_t)lists.size()) {
            std::vector<const ADVBProg *>& list = lists[prog.GetDVBCard()];

            if ((list.size() < 2) && (prog.GetRecordStopDT() >= start)) list.push_back(&prog);
        }
    }

    for (i = 0; i < (uint_t)lists.size(); i++) {
        const std::vector<const ADVBProg *>& list = lists[i];
        TIMEGAP& gap = gaps[i];

        gap.start = start;
        gap.end   = ADateTime::MaxDateTime;
        gap.card  = i;  // NOTE: virtual card

        if (list.size() > 0) {
            if (list[0]->GetRecordStartDT() < start) {
                gap.start = list[0]->GetRecordStopDT();
                if (list.size() > 1) {
                    gap.end = list[1]->GetRecordStartDT();
                }
            }
            else gap.end = list[0]->GetRecordStartDT();
        }

        if (((cardstoavoid == NULL) ||
             (cardstoavoid->find(gap.card) == cardstoavoid->end())) &&
            (gap.start <= start) &&
            ((gap.end - gap.start) > (res.end - res.start))) {
            res = gap;
        }
    }

    return res;
}

bool ADVBProgList::RecordImmediately(const ADateTime& dt, const AString& title, const AString& user, uint64_t maxminutes) const
{
    const ADVBConfig& config = ADVBConfig::Get();
    const uint64_t dt1 = (uint64_t)dt;
    const uint64_t dt2 = dt1 + (uint64_t)maxminutes * (uint64_t)1000;
    uint_t i;
    bool success = false;

    for (i = 0; i < Count(); i++) {
        const ADVBProg& prog = GetProg(i);

        if ((prog.GetStop() > dt1) && (prog.GetStart() <= dt2) && (stricmp(prog.GetTitle(), title.str()) == 0)) {
            config.printf("Found '%s'", prog.GetQuickDescription().str());
            break;
        }
    }

    if (i < Count()) {
        const ADateTime buffer(10000);
        ADVBProg prog(GetProg(i));
        std::vector<TIMEGAP> gaps;
        int best = -1;

        if (user.Valid()) prog.SetUser(user);
        prog.GenerateRecordData(dt1 + 20000);
        prog.SetIgnoreLateStart();

        FindGaps(dt, gaps);

        for (i = 0; i < (uint_t)gaps.size(); i++) {
            const TIMEGAP& gap = gaps[i];

            if ((prog.GetRecordStartDT() >= (gap.start + buffer)) && ((prog.GetRecordStopDT() + buffer)  <= gap.end)) {
                if ((best < 0) || (gap.end < gaps[best].end)) best = i;
            }
        }

        if (best >= 0) {
            prog.SetDVBCard(best);
            prog.SetScheduled();

            if (config.GetRecordingSlave().Valid()) {
                AString cmd;

                cmd.printf("dvb --schedule-record %s", prog.Base64Encode().str());

                if (RunAndLogRemoteCommand(cmd)) {
                    ADVBLock lock("dvbfiles");
                    ADVBProgList scheduledlist;

                    config.printf("Scheduled '%s' (remotely) as using DVB card %u (hardware card %u)", prog.GetDescription().str(), prog.GetDVBCard(), config.GetPhysicalDVBCard(prog.GetDVBCard()));

                    if (scheduledlist.ReadFromBinaryFile(config.GetScheduledFile())) {
                        scheduledlist.ModifyFromRecordingSlave(config.GetScheduledFile(), Prog_ModifyAndAdd);
                        scheduledlist.WriteToFile(config.GetScheduledFile());
                        success = true;
                    }
                }
                else config.printf("Failed to schedule '%s' (remotely)", prog.GetDescription().str());
            }
            else if (config.CommitScheduling()) {
                if (prog.WriteToJobQueue()) {
                    ADVBLock lock("dvbfiles");
                    ADVBProgList scheduledlist;

                    config.printf("Scheduled '%s' as job %u using DVB card %u (hardware card %u)", prog.GetDescription().str(), prog.GetJobID(), prog.GetDVBCard(), config.GetPhysicalDVBCard(prog.GetDVBCard()));

                    if (scheduledlist.ReadFromBinaryFile(config.GetScheduledFile())) {
                        scheduledlist.AddProg(prog);
                        scheduledlist.WriteToFile(config.GetScheduledFile());
                        success = true;
                    }
                }
                else config.printf("Failed to schedule '%s'", prog.GetDescription().str());
            }
            else config.printf("Not allowed to schedule '%s'", prog.GetDescription().str());
        }
        else config.printf("No gap big enough to schedule '%s'", prog.GetDescription().str());
    }
    else config.printf("Failed to find '%s'", title.str());

    return success;
}

void ADVBProgList::StripFilmsAndSeries(const SERIESLIST& serieslist)
{
    uint_t i;

    for (i = 0; i < Count();) {
        const ADVBProg& prog = GetProg(i);
        bool delprog = prog.IsFilm();

        if (!delprog) {
            SERIESLIST::const_iterator it = serieslist.find(prog.GetTitle());
            const ADVBProg::EPISODE& ep = prog.GetEpisode();

            delprog = ((it != serieslist.end()) &&
                       (!ep.valid ||
                        (ep.valid &&
                         (ep.series > 0) &&
                         (ep.series < it->second.list.size()) &&
                         it->second.list[ep.series].Valid())));
        }

        if (delprog) {
            DeleteProg(i);
        }
        else i++;
    }
}

bool ADVBProgList::EmailList(const AString& recipient, const AString& subject, const AString& message, uint_t verbosity, bool force) const
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString cmd;
    bool    success = false;

    if (force || (Count() > 0)) {
        if ((cmd = config.GetConfigItem("emailcmd", "mail -s \"{subject}\" {recipient} <{file}")).Valid()) {
            AString  filename = config.GetLogDir().CatPath("email-" + ADateTime().DateFormat("%Y-%M-%D-%h-%m-%s") + ".txt");
            AStdFile fp;

            if (fp.open(filename, "w")) {
                uint_t i;

                if (message.Valid()) {
                    fp.printf("%s\n", message.str());
                }

                for (i = 0; i < Count(); i++) {
                    fp.printf("%s\n", GetProg(i).GetDescription(verbosity).str());
                }

                fp.close();

                cmd = (cmd.
                       SearchAndReplace("{subject}",   subject).
                       SearchAndReplace("{recipient}", recipient).
                       SearchAndReplace("{file}",      filename));

                success = RunAndLogCommand(cmd);

                remove(filename);
            }
            else {
                config.printf("Failed to email '%s' (subject '%s') with list of %u programmes: failed to create temporary file", recipient.str(), subject.str(), Count());
            }
        }
        else {
            config.printf("Failed to email '%s' (subject '%s') with list of %u programmes: no email command configure", recipient.str(), subject.str(), Count());
        }
    }
    else {
        config.printf("List is empty, not emailing '%s' (subject '%s')", recipient.str(), subject.str());

        success = true;
    }

    return success;
}
