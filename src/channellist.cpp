
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#define PREFER_JSON 1

#if PREFER_JSON
#include <rapidjson/prettywriter.h>
#endif

#include <rdlib/Recurse.h>

#include "channellist.h"
#include "dvblock.h"
#include "dvbmisc.h"

ADVBChannelList::ADVBChannelList() : changed(false)
{
    Read();
}

ADVBChannelList::~ADVBChannelList()
{
    Write();

    size_t i;
    for (i = 0; i < list.size(); i++) {
        delete list[i];
    }
}

bool ADVBChannelList::__SortChannels(const channel_t *chan1, const channel_t *chan2)
{
    bool chan1beforechan2 = false;

    if ((chan1->dvb.lcn == 0) && (chan2->dvb.lcn == 0)) {
        // neither has LCN, sort alphabetically by DVB channel
        chan1beforechan2 = (CompareNoCase(chan1->dvb.channelname, chan2->dvb.channelname) < 0);
    }
    else if (chan1->dvb.lcn == 0) {
        // chan2 has LCN, chan2 must be before chan1
        chan1beforechan2 = false;
    }
    else if (chan2->dvb.lcn == 0) {
        // chan1 has LCN, chan1 must be before chan2
        chan1beforechan2 = true;
    }
    else if (chan1->dvb.lcn != chan2->dvb.lcn) {
        // LCN's are different, sort by LCN
        chan1beforechan2 = (chan1->dvb.lcn < chan2->dvb.lcn);
    }
    else {
        // both LCN's are the same, sort alphabetically by DVB channel
        chan1beforechan2 = (CompareNoCase(chan1->dvb.channelname, chan2->dvb.channelname) < 0);
    }

    return chan1beforechan2;
}

ADVBChannelList& ADVBChannelList::Get()
{
    static ADVBChannelList channellist;
    GetSingleton() = true;
    return channellist;
}

bool ADVBChannelList::ForceChannelAudioAndVideoFlags(channel_t *channel)
{
    bool hadvideo    = channel->dvb.hasvideo;
    bool hadaudio    = channel->dvb.hasaudio;
    bool hadsubtitle = channel->dvb.hassubtitle;

    channel->dvb.hasvideo    = false;
    channel->dvb.hasaudio    = false;
    channel->dvb.hassubtitle = false;

    if (channel->dvb.pidlist.size() >= 3) {
        // ASSUME for now that a PID list of 3 or more means both audio and video and subtitles are available
        channel->dvb.hasvideo = channel->dvb.hasaudio = channel->dvb.hassubtitle = true;
    }
    else if (channel->dvb.pidlist.size() >= 2) {
        // ASSUME for now that a PID list of 2 or more means both audio and video are available
        channel->dvb.hasvideo = channel->dvb.hasaudio = true;
    }
    else if (channel->dvb.pidlist.size() >= 1) {
        // ASSUME for now that a PID list of 1 or more means at least audio is available
        channel->dvb.hasaudio = true;
    }
    else {
        // no PIDS, no audio or video
    }

    return ((channel->dvb.hasvideo != hadvideo) || (channel->dvb.hasaudio != hadaudio) || (channel->dvb.hassubtitle != hadsubtitle));
}

bool ADVBChannelList::Read()
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADVBLock lock("dvbfiles");
    AStdFile fp;
    bool     success = false;

    DeleteAll();

#if PREFER_JSON
    AString json;

    if (AStdFile::exists(config.GetDVBChannelsJSONFile()) &&
        json.ReadFromFile(config.GetDVBChannelsJSONFile())) {
        rapidjson::Document doc;

        //config.printf("Reading channels from '%s'...", config.GetDVBChannelsJSONFile().str());

        if (!doc.Parse(json).HasParseError()) {
            if (doc.IsArray()) {
                size_t i;

                success = true;

                for (i = 0; i < doc.Size(); i++) {
                    if (doc[i].IsObject()) {
                        const rapidjson::Value& chanobj = doc[i];
                        channel_t *chan;

                        if ((chan = new channel_t) != NULL) {
                            chan->dvb.lcn           = 0;
                            chan->xmltv.sdchannelid = 0;
                            chan->dvb.freq          = 0;
                            chan->dvb.hasaudio      = false;
                            chan->dvb.hasvideo      = false;
                            chan->dvb.hassubtitle   = false;

                            if (chanobj.HasMember("dvb")) {
                                const rapidjson::Value& subobj = chanobj["dvb"];

                                if (subobj.HasMember("lcn") && subobj["lcn"].IsNumber()) {
                                    chan->dvb.lcn = subobj["lcn"].GetUint();
                                }

                                if (subobj.HasMember("name") && subobj["name"].IsString()) {
                                    chan->dvb.channelname = subobj["name"].GetString();

                                    if (chan->dvb.channelname.Valid()) {
                                        chan->dvb.convertedchannelname = ConvertDVBChannel(chan->dvb.channelname);
                                        dvbchannelmap[chan->dvb.channelname.ToLower()] = chan;
                                        dvbchannelmap[chan->dvb.convertedchannelname.ToLower()] = chan;
                                    }
                                }

                                if (subobj.HasMember("frequency") && subobj["frequency"].IsNumber()) {
                                    chan->dvb.freq = subobj["frequency"].GetUint();
                                }

                                if (subobj.HasMember("hasvideo") && subobj["hasvideo"].IsBool()) {
                                    chan->dvb.hasvideo = subobj["hasvideo"].GetBool();
                                }
                                else {
                                    chan->dvb.hasvideo = true;
                                    changed = true;
                                }

                                if (subobj.HasMember("hasaudio") && subobj["hasaudio"].IsBool()) {
                                    chan->dvb.hasaudio = subobj["hasaudio"].GetBool();
                                }
                                else {
                                    chan->dvb.hasaudio = true;
                                    changed = true;
                                }

                                if (subobj.HasMember("hassubtitle") && subobj["hassubtitle"].IsBool()) {
                                    chan->dvb.hassubtitle = subobj["hassubtitle"].GetBool();
                                }

                                if (subobj.HasMember("pidlist") && subobj["pidlist"].IsArray()) {
                                    const rapidjson::Value& pidlist = subobj["pidlist"];
                                    size_t j;

                                    for (j = 0; j < pidlist.Size(); j++) {
                                        if (pidlist[j].IsNumber()) {
                                            chan->dvb.pidlist.push_back(pidlist[j].GetUint());
                                        }
                                    }
                                }
                            }

                            if (chanobj.HasMember("xmltv")) {
                                const rapidjson::Value& subobj = chanobj["xmltv"];

                                if (subobj.HasMember("sdchannelid") && subobj["sdchannelid"].IsNumber()) {
                                    chan->xmltv.sdchannelid = subobj["sdchannelid"].GetUint();
                                }

                                if (subobj.HasMember("name") && subobj["name"].IsString()) {
                                    chan->xmltv.channelname = subobj["name"].GetString();

                                    if (chan->xmltv.channelname.Valid()) {
                                        chan->xmltv.convertedchannelname = ConvertXMLTVChannel(chan->xmltv.channelname);
                                        xmltvchannelmap[chan->xmltv.channelname.ToLower()] = chan;
                                        xmltvchannelmap[chan->xmltv.convertedchannelname.ToLower()] = chan;
                                    }
                                }

                                if (subobj.HasMember("id") && subobj["id"].IsString()) {
                                    chan->xmltv.channelid = subobj["id"].GetString();

                                    if (chan->xmltv.channelid.Valid()) {
                                        xmltvchannelmap[chan->xmltv.channelid.ToLower()] = chan;
                                    }
                                }
                            }

                            list.push_back(chan);
                            if (chan->dvb.lcn) {
                                if (chan->dvb.lcn >= lcnlist.size()) {
                                    lcnlist.resize(chan->dvb.lcn + 1);
                                }
                                lcnlist[chan->dvb.lcn] = chan;
                            }
                        }
                    }
                }
            }
        }
        else config.printf("Failed to decode JSON channels file '%s'", config.GetDVBChannelsJSONFile().str());
    }
#endif

    // merge in dat file if it is newer
    if (FileNewerThan(config.GetDVBChannelsFile(), config.GetDVBChannelsJSONFile()) &&
        fp.open(config.GetDVBChannelsFile())) {
        AString line;

        //config.printf("Reading channels from '%s'...", config.GetDVBChannelsFile().str());

        while (line.ReadLn(fp) >= 0) {
            channel_t *chan = NULL;
            AString name, _freq, _pid1, _pid2;

            if ((line.GetFieldNumber(":", 0, name) >= 0) &&
                (line.GetFieldNumber(":", 1, _freq) >= 0) &&
                (line.GetFieldNumber(":", 10, _pid1) >= 0) &&
                (line.GetFieldNumber(":", 11, _pid2) >= 0)) {
                if ((chan = GetChannelByName(name, true)) != NULL) {
                    if (chan->dvb.pidlist.size() >= 2) {
                        const auto& pidlist = chan->dvb.pidlist;
                        AString pidliststr1;
                        AString pidliststr2;

                        pidliststr1.printf("%u", pidlist[0]);
                        for (size_t i = 1; i < pidlist.size(); i++) {
                            pidliststr1.printf(", %u", pidlist[i]);
                        }

                        pidliststr2.printf("%s, %s", _pid1.str(), _pid2.str());

                        config.printf("Warning: NOT going replace pidlist '%s' with '%s' for channel '%s'", pidliststr1.str(), pidliststr2.str(), name.str());
                    }
                    else {
                        chan->dvb.freq = (uint32_t)_freq;
                        chan->dvb.pidlist.clear();

                        if ((uint_t)_pid1) {
                            chan->dvb.pidlist.push_back((uint_t)_pid1);
                            chan->dvb.hasvideo = true;
                        }
                        else chan->dvb.hasvideo = false;

                        if ((uint_t)_pid2) {
                            chan->dvb.pidlist.push_back((uint_t)_pid2);
                            chan->dvb.hasaudio = true;
                        }
                        else chan->dvb.hasaudio = false;

                        chan->dvb.hassubtitle = false;

                        changed = true;
                    }
                }
            }
            else if ((name = line.Column(0)).Valid() && (_freq = line.Column(1)).Valid()) {
                uint_t lcn = 0;

                if (sscanf(name.str(), "[%u]", &lcn) > 0) {
                    name = name.Mid(name.Pos("]") + 1);
                }

                if ((chan = GetChannelByName(name, true, lcn)) != NULL) {
                    if (chan->dvb.pidlist.size() >= 2) {
                        const auto& pidlist = chan->dvb.pidlist;
                        AString pidliststr1;
                        AString pidliststr2;

                        pidliststr1.printf("%u", pidlist[0]);
                        for (size_t i = 1; i < pidlist.size(); i++) {
                            pidliststr1.printf(", %u", pidlist[i]);
                        }

                        pidliststr2.printf("%s", line.Column(2).str());
                        for (int i = 3; i < line.CountColumns(); i++) {
                            pidliststr2.printf(", %s", line.Column(i).str());
                        }

                        config.printf("Warning: NOT going replace pidlist '%s' with '%s' for channel '%s'", pidliststr1.str(), pidliststr2.str(), name.str());
                    }
                    else {
                        chan->dvb.freq = (uint32_t)_freq;
                        chan->dvb.pidlist.clear();

                        int i, n = line.CountColumns();
                        for (i = 2; (i < n); i++) {
                            AString col = line.Column(i);

                            chan->dvb.pidlist.push_back((uint_t)col);
                        }

                        // set video and audio flags based on number of PIDs in list
                        changed |= ForceChannelAudioAndVideoFlags(chan);
                    }
                }
            }
        }

        fp.close();

        success = true;

        // force re-writing as JSON
        changed = true;
    }

    std::sort(list.begin(), list.end(), __SortChannels);

    return success;
}

bool ADVBChannelList::Write()
{
    bool success = false;

    if (changed) {
        const ADVBConfig& config = ADVBConfig::Get();
        ADVBLock lock("dvbfiles");
        AStdFile fp;

        success = true;

        std::sort(list.begin(), list.end(), __SortChannels);

#if PREFER_JSON
        rapidjson::Document doc;

        GenerateChanneList(doc, doc);

        config.printf("Writing channels to '%s'...", config.GetDVBChannelsJSONFile().str());

        if (fp.open(config.GetDVBChannelsJSONFile(), "w")) {
            rapidjson::PrettyWriter<AStdData> writer(fp);

            doc.Accept(writer);

            fp.close();

            // send updated file to recording slave
            if (config.GetRecordingSlave().Valid()) {
                config.printf("Sending channels file to '%s'...", config.GetRecordingSlave().str());

                if (!SendFileToRecordingSlave(config.GetDVBChannelsJSONFile())) {
                    config.printf("Failed to update channels on recording slave");
                    success = false;
                }
            }
        }
        else {
            config.printf("Failed to open file '%s' for writing", config.GetDVBChannelsJSONFile().str());
            success = false;
        }

#else
        std::vector<AString> strlist;

        for (i = 0; i < list.size(); i++) {
            const channel_t& chan = *list[i];

            if (chan.dvb.channelname.Valid()) {
                AString str;
                uint_t  j;

                if (chan.lcn) {
                    str.printf("[%03u]", chan.dvb.lcn);
                }

                str.printf("%s,%u", chan.dvb.channelname.str(), chan.dvb.freq);

                for (j = 0; j < chan.dvb.pidlist.size(); j++) {
                    str.printf(",%u", chan.dvb.pidlist[j]);
                }

                strlist.push_back(str);
            }
        }

        std::sort(strlist.begin(), strlist.end());

        config.printf("Writing channels to '%s'...", config.GetDVBChannelsFile().str());

        if (fp.open(config.GetDVBChannelsFile(), "w")) {
            for (i = 0; i < strlist.size(); i++) {
                fp.printf("%s\n", strlist[i].str());
            }

            fp.close();

            // send updated file to recording slave
            if (config.GetRecordingSlave().Valid()) {
                config.printf("Sending channels file to '%s'...", config.GetRecordingSlave().str());

                if (!SendFileToRecordingSlave(config.GetDVBChannelsFile())) {
                    config.printf("Failed to update channels on recording slave");
                    success = false;
                }
            }
            // if slave, trigger update of channels file
            else TriggerServerCommand("dvbupdatechannels");
        }
        else {
            config.printf("Failed to open file '%s' for writing", config.GetDVBChannelsFile().str());
            success = false;
        }
#endif

        changed &= !success;
    }
    else success = true;

    return success;
}

void ADVBChannelList::DeleteAll()
{
    list.clear();
    lcnlist.clear();
    dvbchannelmap.clear();
    xmltvchannelmap.clear();
    changed = false;
}

void ADVBChannelList::GenerateChanneList(rapidjson::Document& doc, rapidjson::Value& obj, bool incconverted) const
{
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
    size_t i;

    obj.SetArray();

    for (i = 0; i < list.size(); i++) {
        const channel_t& chan = *list[i];
        rapidjson::Value chanobj;
        rapidjson::Value dvbobj;
        rapidjson::Value xmltvobj;

        chanobj.SetObject();
        dvbobj.SetObject();
        xmltvobj.SetObject();

        if (chan.dvb.channelname.Valid()) {
            dvbobj.AddMember("name", rapidjson::Value(chan.dvb.channelname.str(), allocator), allocator);
        }

        if (incconverted && chan.dvb.convertedchannelname.Valid()) {
            dvbobj.AddMember("convertedname", rapidjson::Value(chan.dvb.convertedchannelname.str(), allocator), allocator);
        }

        if (chan.dvb.lcn) {
            dvbobj.AddMember("lcn", rapidjson::Value(chan.dvb.lcn), allocator);
        }

        if (chan.dvb.freq) {
            dvbobj.AddMember("frequency", rapidjson::Value(chan.dvb.freq), allocator);
        }

        dvbobj.AddMember("hasvideo",    rapidjson::Value(chan.dvb.hasvideo), allocator);
        dvbobj.AddMember("hasaudio",    rapidjson::Value(chan.dvb.hasaudio), allocator);
        dvbobj.AddMember("hassubtitle", rapidjson::Value(chan.dvb.hassubtitle), allocator);

        if (chan.dvb.pidlist.size() > 0) {
            rapidjson::Value pidlist;
            size_t j;

            pidlist.SetArray();
            for (j = 0; j < chan.dvb.pidlist.size(); j++) {
                pidlist.PushBack(chan.dvb.pidlist[j], allocator);
            }

            dvbobj.AddMember("pidlist", pidlist, allocator);
        }

        if (chan.xmltv.sdchannelid) {
            xmltvobj.AddMember("sdchannelid", rapidjson::Value(chan.xmltv.sdchannelid), allocator);
        }
        if (chan.xmltv.channelid.Valid()) {
            xmltvobj.AddMember("id", rapidjson::Value(chan.xmltv.channelid.str(), allocator), allocator);
        }
        if (chan.xmltv.channelname.Valid()) {
            xmltvobj.AddMember("name", rapidjson::Value(chan.xmltv.channelname.str(), allocator), allocator);
        }
        if (incconverted && chan.xmltv.convertedchannelname.Valid()) {
            xmltvobj.AddMember("convertedname", rapidjson::Value(chan.xmltv.convertedchannelname.str(), allocator), allocator);
        }

        if (dvbobj.MemberCount() > 0) {
            chanobj.AddMember("dvb", dvbobj, allocator);
        }
        if (xmltvobj.MemberCount() > 0) {
            chanobj.AddMember("xmltv", xmltvobj, allocator);
        }

        if (chanobj.MemberCount() > 0) {
            obj.PushBack(chanobj, allocator);
        }
    }
}

AString ADVBChannelList::ConvertDVBChannel(const AString& str)
{
    static std::vector<replacement_t> replacements;
    const ADVBConfig& config = ADVBConfig::Get();

    if (!replacements.size()) config.ReadReplacementsFile(replacements, config.GetDVBReplacementsFile());

    return ReplaceStrings(str, &replacements[0], (uint_t)replacements.size());
}

AString ADVBChannelList::ConvertXMLTVChannel(const AString& str)
{
    static std::vector<replacement_t> replacements;
    const ADVBConfig& config = ADVBConfig::Get();

    if (!replacements.size()) config.ReadReplacementsFile(replacements, config.GetXMLTVReplacementsFile());

    return ReplaceStrings(str, &replacements[0], (uint_t)replacements.size());
}

AString ADVBChannelList::GetPIDList(const pidlist_t& pidlist) const
{
    AString str;

    for (size_t i = 0; i < pidlist.size(); i++) {
        str.printf(" %u", pidlist[i]);
    }

    return str.Words(0);
}

const ADVBChannelList::channel_t *ADVBChannelList::GetChannelByDVBChannelName(const AString& name) const
{
    channelmap_t::const_iterator it;
    const channel_t *chan = NULL;

    if ((it = dvbchannelmap.find(name.ToLower())) != dvbchannelmap.end()) {
        chan = it->second;
    }

    return chan;
}

const ADVBChannelList::channel_t *ADVBChannelList::GetChannelByXMLTVChannelName(const AString& name) const
{
    channelmap_t::const_iterator it;
    const channel_t *chan = NULL;

    if ((it = xmltvchannelmap.find(name.ToLower())) != xmltvchannelmap.end()) {
        chan = it->second;
    }

    return chan;
}

const ADVBChannelList::channel_t *ADVBChannelList::GetChannelByName(const AString& name) const
{
    const channel_t *chan;

    if ((chan = GetChannelByDVBChannelName(name)) == NULL) {
        chan = GetChannelByXMLTVChannelName(name);
    }

    return chan;
}

ADVBChannelList::channel_t *ADVBChannelList::GetChannelByName(const AString& name, bool create, uint_t lcn)
{
    channel_t *chan = NULL;

    if ((chan = GetChannelByDVBChannelName(name, false)) == NULL) {
        channelmap_t::iterator it;

        if ((it = xmltvchannelmap.find(name.ToLower())) != xmltvchannelmap.end()) {
            chan = it->second;
        }
        else chan = GetChannelByDVBChannelName(name, create, lcn);
    }

    return chan;
}

ADVBChannelList::channel_t *ADVBChannelList::GetChannelByDVBChannelName(const AString& name, bool create, uint_t lcn)
{
    channelmap_t::iterator it;
    channel_t *chan = NULL;

    if ((it = dvbchannelmap.find(name.ToLower())) != dvbchannelmap.end()) {
        chan = it->second;
    }

    if (!chan && create && ((chan = new channel_t) != NULL)) {
        chan->dvb.lcn = lcn;
        chan->dvb.freq = 0;
        chan->dvb.hasvideo = false;
        chan->dvb.hasaudio = false;
        chan->xmltv.sdchannelid = 0;
        chan->dvb.channelname = name;
        chan->dvb.convertedchannelname = ConvertDVBChannel(name);

        list.push_back(chan);
        dvbchannelmap[chan->dvb.channelname.ToLower()] = chan;
        dvbchannelmap[chan->dvb.convertedchannelname.ToLower()] = chan;

        if (chan->dvb.lcn) {
            if (chan->dvb.lcn >= lcnlist.size()) {
                lcnlist.resize(chan->dvb.lcn + 1);
            }
            lcnlist[chan->dvb.lcn] = chan;
        }

        changed = true;
    }

    return chan;
}

const ADVBChannelList::channel_t *ADVBChannelList::GetChannelByFrequencyAndPIDs(uint32_t freq, const AString& pids) const
{
    pidlist_t pidlist;
    sint_t  i, n = pids.CountWords();

    for (i = 0; i < n; i++) {
        pidlist.push_back((uint_t)pids.Word(i));
    }

    return GetChannelByFrequencyAndPIDs(freq, pidlist);
}

const ADVBChannelList::channel_t *ADVBChannelList::GetChannelByFrequencyAndPIDs(uint32_t freq, const pidlist_t& pidlist) const
{
    const channel_t *channel = NULL;
    size_t i, j;

    for (i = 0; i < list.size(); i++) {
        const auto& chan = *list[i];

        if (freq == chan.dvb.freq) {
            for (j = 0; (j < pidlist.size()) && (j < chan.dvb.pidlist.size()) && (pidlist[j] == chan.dvb.pidlist[j]); j++) ;
            if (j == pidlist.size()) {
                channel = &chan;
                break;
            }
        }
    }

    return channel;
}

const ADVBChannelList::channel_t *ADVBChannelList::AssignOrAddXMLTVChannel(uint_t lcn, const AString& name, const AString& id)
{
    const ADVBConfig& config = ADVBConfig::Get();
    const AString cname = ConvertXMLTVChannel(name);
    channelmap_t::iterator it;
    channel_t *chan = NULL;
    bool changed1 = false, report = false;

    if ((it = xmltvchannelmap.find(name.ToLower())) != xmltvchannelmap.end()) {
        chan = it->second;
    }
    else if ((it = xmltvchannelmap.find(id.ToLower())) != xmltvchannelmap.end()) {
        chan = it->second;
    }
    else if ((it = dvbchannelmap.find(name.ToLower())) != dvbchannelmap.end()) {
        chan = it->second;
    }
    else if ((it = dvbchannelmap.find(cname.ToLower())) != dvbchannelmap.end()) {
        chan = it->second;
    }
    else if ((lcn > 0) && (lcn < lcnlist.size()) && ((chan = lcnlist[lcn]) != NULL)) {
        report = true;
    }

    if (!chan && ((chan = new channel_t) != NULL)) {
        // create new entry
        chan->dvb.lcn = 0;
        chan->dvb.freq = 0;
        chan->xmltv.sdchannelid = 0;
        list.push_back(chan);
        changed1 = true;
    }

    if (chan) {
        uint_t sdchannelid = 0;

        // update logical channel number if possible
        if ((lcn > 0) && (lcn != chan->dvb.lcn)) {
            // remove existing entry from lcnlist[]
            if ((chan->dvb.lcn > 0) && (chan->dvb.lcn < lcnlist.size())) {
                lcnlist[chan->dvb.lcn] = NULL;
            }
            chan->dvb.lcn = lcn;

            // add entry to lcnlist[]
            if (chan->dvb.lcn >= lcnlist.size()) {
                lcnlist.resize(chan->dvb.lcn + 1);
            }
            lcnlist[chan->dvb.lcn] = chan;

            changed1 = true;
        }

        if (id != chan->xmltv.channelid) {
            // erase old entry from xmltv map
            if (chan->xmltv.channelid.Valid() && ((it = xmltvchannelmap.find(chan->xmltv.channelid.ToLower())) != xmltvchannelmap.end())) {
                xmltvchannelmap.erase(it);
            }

            // update ID
            chan->xmltv.channelid = id;

            changed1 = true;
        }

        // get Schedules Direct channel ID, format is 'I101023.json.schedulesdirect.org'
        if ((sscanf(id.str(), "I%u.json.schedulesdirect.org", &sdchannelid) > 0) &&
            (sdchannelid > 0) &&
            (sdchannelid != chan->xmltv.sdchannelid)) {
            // update ID
            chan->xmltv.sdchannelid = sdchannelid;

            changed1 = true;
        }

        if (name != chan->xmltv.channelname) {
            // erase old entry from xmltv map
            if (chan->xmltv.channelname.Valid() && ((it = xmltvchannelmap.find(chan->xmltv.channelname.ToLower())) != xmltvchannelmap.end())) {
                xmltvchannelmap.erase(it);
            }

            // update channel name
            chan->xmltv.channelname = name;

            changed1 = true;
        }

        if (cname != chan->xmltv.convertedchannelname) {
            // erase old entry from xmltv map
            if (chan->xmltv.convertedchannelname.Valid() && ((it = xmltvchannelmap.find(chan->xmltv.convertedchannelname.ToLower())) != xmltvchannelmap.end())) {
                xmltvchannelmap.erase(it);
            }

            // update channel name
            chan->xmltv.convertedchannelname = cname;

            changed1 = true;
        }

        //report = true;
    }

    // update xmltv map using multiple entries
    xmltvchannelmap[id.ToLower()] = chan;
    xmltvchannelmap[name.ToLower()] = chan;
    xmltvchannelmap[cname.ToLower()] = chan;

    if (report && changed1) {
        if (lcn > 0) {
            config.printf("Assigned XMLTV channel '%s' to DVB channel '%s' (ID '%s') via channel number %u", chan->xmltv.channelname.str(), chan->dvb.channelname.str(), id.str(), lcn);
        }
        else {
            config.printf("Assigned XMLTV channel '%s' to DVB channel '%s' (ID '%s')", chan->xmltv.channelname.str(), chan->dvb.channelname.str(), id.str());
        }
    }

    changed |= changed1;

    return chan;
}

bool ADVBChannelList::Update(uint_t card, uint32_t freq, bool verbose)
{
    const ADVBConfig& config = ADVBConfig::Get();
    AStdFile fp;
    AString  sifilename;
    AString  cmd;
    bool success = false;

    config.printf("Scanning %uHz...", freq);

    sifilename = config.GetTempFile("pids", ".txt");

    cmd.printf("timeout 20s dvbtune -c %u -f %u -i >%s 2>>%s", card, (uint_t)freq, sifilename.str(), config.GetLogFile(ADateTime().GetDays()).str());

    if (system(cmd) == 0) {
        if (fp.open(sifilename)) {
            AString service;
            AString line;
            AString servname;

            while (!success && (line.ReadLn(fp) >= 0)) {
                line = line.Words(0);

                if (line.Left(13) == "<description ") {
                    servname = line.GetField("service_name=\"", "\"").DeHTMLify();
                }

                if (line.Left(8) == "<service") {
                    service = line + "\n";
                }
                else {
                    service += line + "\n";

                    if (line == "</service>") {
                        channel_t *chan = GetChannelByName(servname, true);

                        if (chan) {
                            typedef enum {
                                Type_none = 0,
                                Type_video,
                                Type_audio,
                                Type_subtitle,
                            } pidtype_t;
                            std::map<pidtype_t,bool> pidhash;
                            pidlist_t                pidlist;
                            uint_t i, n = service.CountLines("\n", 0);

                            config.printf("Channel '%s' at frequency %uHz:", chan->dvb.channelname.str(), freq);

                            changed |= (freq != chan->dvb.freq);
                            chan->dvb.freq = freq;

                            bool hasvideo    = false;
                            bool hasaudio    = false;
                            bool hassubtitle = false;

                            uint_t    type = 0;
                            uint_t    pid  = 0;
                            uint_t    stid = 0;
                            pidtype_t id   = Type_none;

                            for (i = 0; i < n; i++) {
                                static const AString streamstartstr = "<stream ";
                                static const AString streamendstr   = "</stream>";
                                static const AString subtitlestr    = "<subtitling_descriptor ";

                                line = service.Line(i, "\n", 0);

                                if (line.Left(streamstartstr.len()) == streamstartstr) {
                                    type = (uint_t)line.GetField("type=\"", "\"");
                                    pid  = (uint_t)line.GetField("pid=\"", "\"");

                                    if      (RANGE(type, 1, 2)) id = Type_video;
                                    else if (RANGE(type, 3, 4)) id = Type_audio;
                                }
                                else if (line.Left(subtitlestr.len()) == subtitlestr) {
                                    stid = (uint_t)line.GetField("tag=\"", "\"");

                                    if ((type == 6) && (stid == 0x59)) id = Type_subtitle;
                                }
                                else if (line == streamendstr) {
                                    if ((pid > 0) && (id != Type_none) && (pidhash.find(id) == pidhash.end())) {
                                        pidhash[id] = true;

                                        pidlist.push_back(pid);

                                        switch (id) {
                                            default:
                                                break;

                                            case Type_video:
                                                hasvideo = true;
                                                break;

                                            case Type_audio:
                                                hasaudio = true;
                                                break;

                                            case Type_subtitle:
                                                hassubtitle = true;
                                                break;
                                        }
                                    }

                                    if (verbose) config.printf("Service %s Stream type %u pid %u (%s)", servname.str(), type, pid, (id != Type_none) ? "included" : "EXCLUDED");

                                    type = pid = stid = 0;
                                    id = Type_none;
                                }
                            }

                            if (pidlist.size() > 0) {
                                AString str;

                                std::sort(pidlist.begin(), pidlist.end(), std::less<uint_t>());

                                if (pidlist != chan->dvb.pidlist) {
                                    str.printf("Changing PID list for '%s' from '", chan->dvb.channelname.str());
                                    for (i = 0; i < chan->dvb.pidlist.size(); i++) {
                                        str.printf("%s%s", (i > 0) ? ", " : "", AValue(chan->dvb.pidlist[i]).ToString().str());
                                    }

                                    str.printf("' to '");

                                    changed = true;
                                }
                                else str.printf("PID list for '%s' is '", chan->dvb.channelname.str());

                                for (i = 0; i < pidlist.size(); i++) {
                                    str.printf("%s%s", (i > 0) ? ", " : "", AValue(pidlist[i]).ToString().str());
                                }

                                str.printf("'");

                                config.printf("%s", str.str());

                                chan->dvb.pidlist = pidlist;

                                // only update video and audio flags if a new pidlist is available
                                changed |= ((hasvideo != chan->dvb.hasvideo) || (hasaudio != chan->dvb.hasaudio) || (hassubtitle != chan->dvb.hassubtitle));
                                chan->dvb.hasvideo    = hasvideo;
                                chan->dvb.hasaudio    = hasaudio;
                                chan->dvb.hassubtitle = hassubtitle;
                            }
                            else if (!chan->dvb.hasvideo && !chan->dvb.hasaudio) {
                                // no video and audio flags yet, guess them based on number of PIDs in pidlist
                                changed |= ForceChannelAudioAndVideoFlags(chan);
                            }
                        }

                        service.Delete();
                        servname.Delete();
                    }
                }
            }

            fp.close();
        }
    }

    remove(sifilename);

    return success;
}

bool ADVBChannelList::Update(uint_t card, const AString& channel, bool verbose)
{
    const channel_t *chan;
    bool success = false;

    if ((chan = GetChannelByName(channel)) != NULL) {
        success = Update(card, chan->dvb.freq, verbose);
    }

    return success;
}

void ADVBChannelList::UpdateAll(uint_t card, bool verbose)
{
    const ADVBConfig& config = ADVBConfig::Get();
    std::map<uint32_t,bool> freqs;
    size_t i;

    // create list of frequencies
    for (i = 0; i < list.size(); i++) {
        if (list[i]->dvb.freq) {
            freqs[list[i]->dvb.freq] = true;
        }
    }

    if (verbose) config.printf("Found %u frequencies to scan", (uint_t)freqs.size());

    std::map<uint32_t,bool>::iterator it;
    for (it = freqs.begin(); it != freqs.end(); ++it) {
        Update(card, it->first, verbose);
    }
}

bool ADVBChannelList::GetPIDList(uint_t card, const AString& channel, AString& pids, bool update)
{
    const ADVBConfig& config = ADVBConfig::Get();
    const channel_t *chan;
    AString str;
    bool    success = true;

    if (update) success &= Update(card, channel);

    if (((chan = GetChannelByName(channel)) != NULL) &&
        (chan->dvb.freq != 0)) {
        pids.printf("%u", chan->dvb.freq);

        if ((str = config.GetPriorityDVBPIDs()).Valid()) {
            pids.printf(" %s", str.str());
        }

        uint_t i;
        for (i = 0; i < chan->dvb.pidlist.size(); i++) {
            uint_t pid = (uint_t)chan->dvb.pidlist[i];

            pids.printf(" %u", pid);
        }

        if ((str = config.GetExtraDVBPIDs()).Valid()) {
            pids.printf(" %s", str.str());
        }

        pids = pids.Words(0, 9);
    }
    else {
        config.printf("Failed to find channel '%s'", channel.str());
        success = false;
    }

    return success;
}

AString ADVBChannelList::LookupDVBChannel(const AString& channel) const
{
    const channel_t *chan;
    AString channel1;

    if (((chan = GetChannelByDVBChannelName(channel))   != NULL) ||
        ((chan = GetChannelByXMLTVChannelName(channel)) != NULL)) {
        channel1 = chan->dvb.channelname;
    }

    return channel1;
}

AString ADVBChannelList::LookupXMLTVChannel(const AString& channel) const
{
    const channel_t *chan;
    AString channel1;

    if ((chan = GetChannelByXMLTVChannelName(channel)) != NULL) {
        channel1 = chan->xmltv.channelname;
    }

    return channel1;
}

uint32_t ADVBChannelList::TestCard(uint_t card, const channel_t *channel, uint_t seconds) const
{
    return ::TestCard(card, channel->dvb.freq, GetPIDList(channel), seconds);
}
