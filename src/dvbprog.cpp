
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#include <map>
#include <algorithm>

#include <rdlib/StdMemFile.h>
#include <rdlib/Regex.h>
#include <rdlib/Recurse.h>
#include <rapidjson/prettywriter.h>

#include "config.h"
#include "dvbprog.h"
#include "proglist.h"
#include "channellist.h"
#include "dvblock.h"
#include "dvbpatterns.h"
#include "iconcache.h"

#define DEBUG_SAMEPROGRAMME 0

bool ADVBProg::debugsameprogramme = true;

AHash ADVBProg::fieldhash;

const ADVBProg::DVBPROG *ADVBProg::nullprog = (const ADVBProg::DVBPROG *)NULL;

#define DVBPROG_OFFSET(x) ((uint16_t)(uptr_t)&nullprog->x)
#define DEFINE_FIELD(name,var,type,desc)    {#name, ADVBPatterns::FieldType_##type, false, DVBPROG_OFFSET(var), desc}
#define DEFINE_SIMPLE(name,type,desc)       DEFINE_FIELD(name, name, type, desc)
#define DEFINE_STRING(name,desc)            DEFINE_FIELD(name, strings.name, string, desc)
#define DEFINE_FLAG(name,flag,desc)         {#name, ADVBPatterns::FieldType_flag + flag, false, DVBPROG_OFFSET(flags), desc}
#define DEFINE_FLAG_ASSIGN(name,flag,desc)  {#name, ADVBPatterns::FieldType_flag + flag, true, DVBPROG_OFFSET(flags), desc}
#define DEFINE_ASSIGN(name,var,type,desc)   {#name, ADVBPatterns::FieldType_##type, true, DVBPROG_OFFSET(var), desc}
#define DEFINE_EXTERNAL(name,id,type,desc)  {#name, ADVBPatterns::FieldType_external_##type, false, id, desc}

const ADVBProg::FIELD ADVBProg::fields[] = {
    {"prog", ADVBPatterns::FieldType_prog, false, 0, "Encoded Programme"},

    DEFINE_STRING(channel,     "TV channel"),
    DEFINE_STRING(basechannel, "Base channel"),
    DEFINE_STRING(channelid,   "XMLTV channel ID"),
    DEFINE_STRING(dvbchannel,  "DVB channel ID"),
    DEFINE_STRING(title,       "Programme title"),
    DEFINE_STRING(subtitle,    "Programme sub-title"),
    DEFINE_STRING(desc,        "Programme description"),
    DEFINE_STRING(category,    "Programme category"),
    DEFINE_STRING(director,    "Director"),
    DEFINE_STRING(episodenum,  "Episode ID (XMLTV)"),
    DEFINE_STRING(uuid,        "UUID"),
    DEFINE_STRING(filename,    "Recorded filename"),
    DEFINE_STRING(pattern,     "Pattern used to find programme"),
    DEFINE_FIELD(actor,        strings.actors, string, "Actor(s)"),

    DEFINE_STRING(episodeid,   "Episode ID"),
    DEFINE_STRING(icon,        "Programme icon"),
    DEFINE_STRING(rating,      "Age rating"),
    DEFINE_STRING(subcategory, "Programme subcategor(ies)"),

    DEFINE_FIELD(on,        start,    date,    "Day"),
    DEFINE_FIELD(day,       start,    date,    "Day"),
    DEFINE_FIELD(age,       stop,     age,     "Age"),
    DEFINE_SIMPLE(start,    date,              "Start"),
    DEFINE_SIMPLE(stop,     date,              "Stop"),
    DEFINE_SIMPLE(recstart, date,              "Record start"),
    DEFINE_SIMPLE(recstop,  date,              "Record stop"),
    DEFINE_SIMPLE(actstart, date,              "Actual record start"),
    DEFINE_SIMPLE(actstop,  date,              "Actual record stop"),
    DEFINE_FIELD(length,    start,    span,    "Programme length"),
    DEFINE_FIELD(reclength, recstart, span,    "Record length"),
    DEFINE_FIELD(actlength, actstart, span,    "Actual record length"),

    DEFINE_SIMPLE(year,     uint16_t,          "Year"),

    DEFINE_FIELD(card,      dvbcard,  uint8_t, "DVB card"),

    DEFINE_FLAG(repeat,                 Flag_repeat,                "Programme is a repeat"),
    DEFINE_FLAG(plus1,                  Flag_plus1,                 "Programme is on +1"),
    DEFINE_FLAG(running,                Flag_running,               "Programme job running"),
    DEFINE_FLAG(onceonly,               Flag_onceonly,              "Programme to be recorded and then the pattern deleted"),
    DEFINE_FLAG(rejected,               Flag_rejected,              "Programme rejected"),
    DEFINE_FLAG(recorded,               Flag_recorded,              "Programme recorded"),
    DEFINE_FLAG(scheduled,              Flag_scheduled,             "Programme scheduled"),
    DEFINE_FLAG(dvbcardspecified,       Flag_dvbcardspecified,      "Programme to be recorded on specified DVB card"),
    DEFINE_FLAG(incompleterecording,    Flag_incompleterecording,   "Programme recorded is incomplete"),
    DEFINE_FLAG(ignorerecording,        Flag_ignorerecording,       "Programme recorded should be ignored when scheduling"),
    DEFINE_FLAG(recordfailed,           Flag_recordfailed,          "Programme recording failed"),
    DEFINE_FLAG(recording,              Flag_recording,             "Programme recording"),
    DEFINE_FLAG(postprocessing,         Flag_postprocessing,        "Programme recording being processing"),
    DEFINE_FLAG(exists,                 Flag_exists,                "Programme exists"),
    DEFINE_FLAG(convertedexists,        Flag_convertedexists,       "Converted programme exists"),
    DEFINE_FLAG(unconvertedexists,      Flag_unconvertedexists,     "Unconverted programme exists (pre-converted)"),
    DEFINE_FLAG(archived,               Flag_archived,              "Programme has been archived (pre-converted)"),
    DEFINE_FLAG(available,              Flag_exists,                "Programme is available"),
    DEFINE_FLAG(notify,                 Flag_notify,                "Notify by when programme has recorded"),
    DEFINE_FLAG(converted,              Flag_converted,             "Programme has been converted"),
    DEFINE_FLAG(film,                   Flag_film,                  "Programme is a film"),
    DEFINE_FLAG(recordable,             Flag_recordable,            "Programme is recordable"),
    DEFINE_FLAG(partial,                Flag_partialpattern,        "Pattern is partial and not complete"),
    DEFINE_FLAG(ignorelatestart,        Flag_ignorelatestart,       "Allow programme to be recorded even if is late starting"),
    DEFINE_FLAG(postprocessfailed,      Flag_postprocessfailed,     "Programme post-processing failed"),
    DEFINE_FLAG(conversionfailed,       Flag_conversionfailed,      "Programme conversion failed"),
    DEFINE_FLAG(failed,                 Flag_failed,                "Programme record, post-process or conversion failed"),
    DEFINE_FLAG(radioprogramme,         Flag_radioprogramme,        "Programme is a radio programme (audio but no video)"),
    DEFINE_FLAG(tvprogramme,            Flag_tvprogramme,           "Programme is a TV programme (audio and video)"),

    DEFINE_FIELD(epvalid,               episode.valid,    uint8_t,  "Series/episode valid"),
    DEFINE_FIELD(series,                episode.series,   uint8_t,  "Series"),
    DEFINE_FIELD(episode,               episode.episode,  uint16_t, "Episode"),
    DEFINE_FIELD(episodes,              episode.episodes, uint16_t, "Number of episodes in series"),

    DEFINE_FIELD(assignedepisode,       assignedepisode,  uint16_t, "Assigned episode"),

    DEFINE_FLAG_ASSIGN(usedesc,         Flag_usedesc,               "Use description"),
    DEFINE_FLAG_ASSIGN(allowrepeats,    Flag_allowrepeats,          "Allow repeats to be recorded"),
    DEFINE_FLAG_ASSIGN(urgent,          Flag_urgent,                "Record as soon as possible"),
    DEFINE_FLAG_ASSIGN(markonly,        Flag_markonly,              "Do not record, just mark as recorded"),
    DEFINE_FLAG_ASSIGN(postprocess,     Flag_postprocess,           "Post process programme"),
    DEFINE_FLAG_ASSIGN(onceonly,        Flag_onceonly,              "Once recorded, delete the pattern"),
    DEFINE_FLAG_ASSIGN(notify,          Flag_notify,                "Once recorded, run 'notifycmd'"),
    DEFINE_FLAG_ASSIGN(partial,         Flag_partialpattern,        "Pattern is partial and not complete"),
    DEFINE_FLAG_ASSIGN(ignorelatestart, Flag_ignorelatestart,       "Allow programme to be recorded even if is late starting"),
    DEFINE_FLAG_ASSIGN(recordifmissing, Flag_recordifmissing,       "Allow programme to be recorded if it has been recorded already but the file doesn't exist"),

    DEFINE_ASSIGN(pri,                  pri,            sint8_t,    "Scheduling priority"),
    DEFINE_ASSIGN(score,                score,          sint16_t,   "Record score"),
    DEFINE_ASSIGN(prehandle,            prehandle,      uint16_t,   "Record pre-handle (minutes)"),
    DEFINE_ASSIGN(posthandle,           posthandle,     uint16_t,   "Record post-handle (minutes)"),
    DEFINE_ASSIGN(user,                 strings.user,   string,     "User"),
    DEFINE_ASSIGN(dir,                  strings.dir,    string,     "Directory to store file in"),
    DEFINE_ASSIGN(prefs,                strings.prefs,  string,     "Misc prefs"),
    DEFINE_ASSIGN(dvbcard,              dvbcard,        uint8_t,    "DVB card to record from"),
    DEFINE_ASSIGN(tags,                 strings.tags,   string,     "Programme tags"),

    DEFINE_EXTERNAL(brate,              Compare_brate,  uint32_t,   "Encoded file bit rate (bits/s)"),
    DEFINE_EXTERNAL(kbrate,             Compare_kbrate, uint32_t,   "Encoded file bit rate (kbits/s)"),
};

AString ADVBProg::dayformat;
AString ADVBProg::dateformat;
AString ADVBProg::timeformat;
AString ADVBProg::fulltimeformat;
AString ADVBProg::tempfilesuffix;
AString ADVBProg::recordedfilesuffix;
AString ADVBProg::videofilesuffix;
AString ADVBProg::audiofilesuffix;

const AString ADVBProg::videotrackname         = "videotrack";
const AString ADVBProg::audiotrackname         = "audiotrack";

const AString ADVBProg::priorityscalename      = "priorityscale";
const AString ADVBProg::overlapscalename       = "overlapscale";
const AString ADVBProg::urgentscalename        = "urgentscale";
const AString ADVBProg::repeatsscalename       = "repeatsscale";
const AString ADVBProg::delayscalename         = "delayscale";
const AString ADVBProg::latescalename          = "latescale";
const AString ADVBProg::recordoverlapscalename = "recordoverlapscale";

/*--------------------------------------------------------------------------------*/
/** Basic constructor - empty programme
 */
/*--------------------------------------------------------------------------------*/
ADVBProg::ADVBProg()
{
    Init();

    Delete();
}

/*--------------------------------------------------------------------------------*/
/** Construct programme from binary stream
 *
 * @param fp AStdData object to read data from
 */
/*--------------------------------------------------------------------------------*/
ADVBProg::ADVBProg(AStdData& fp)
{
    Init();

    operator = (fp);
}

/*--------------------------------------------------------------------------------*/
/** Construct programme from text field list
 *
 * @param str field list in 'field=value' format (separated by newlines)
 */
/*--------------------------------------------------------------------------------*/
ADVBProg::ADVBProg(const AString& str)
{
    Init();

    operator = (str);
}

/*--------------------------------------------------------------------------------*/
/** Copy constructor
 *
 * @param obj existing programme object
 */
/*--------------------------------------------------------------------------------*/
ADVBProg::ADVBProg(const ADVBProg& obj)
{
    Init();

    operator = (obj);
}

/*--------------------------------------------------------------------------------*/
/** Destructor
 */
/*--------------------------------------------------------------------------------*/
ADVBProg::~ADVBProg()
{
    if (data) free(data);
}

/*--------------------------------------------------------------------------------*/
/** Initialise static (local shared) data - only done once
 */
/*--------------------------------------------------------------------------------*/
void ADVBProg::StaticInit()
{
    if (!fieldhash.GetItems()) {
        const ADVBConfig& config = ADVBConfig::Get();
        uint_t i;

        for (i = 0; i < NUMBEROF(fields); i++) {
            fieldhash.Insert(fields[i].name, (uptr_t)(fields + i));
        }

        // initialise date/time formats from configuration file
        dayformat      = config.GetConfigItem("dayformat",      "%d");
        dateformat     = config.GetConfigItem("dateformat",     " %D-%N-%Y ");
        timeformat     = config.GetConfigItem("timeformat",     "%h:%m");
        fulltimeformat = config.GetConfigItem("fulltimeformat", "%h:%m:%s");
    }

    // set up default file suffices from configuration file
    if (tempfilesuffix.Empty()) {
        const ADVBConfig& config = ADVBConfig::Get();

        tempfilesuffix     = config.GetTempFileSuffix();        ///< file as it is being recorded
        recordedfilesuffix = config.GetRecordedFileSuffix();    ///< file after it has been recorded (before conversion)
        videofilesuffix    = config.GetVideoFileSuffix();       ///< intermediate video file prior to conversion
        audiofilesuffix    = config.GetAudioFileSuffix();       ///< intermediate audio file prior to conversion
    }
}

/*--------------------------------------------------------------------------------*/
/** Initialise data for this object
 */
/*--------------------------------------------------------------------------------*/
void ADVBProg::Init()
{
    StaticInit();

    // initialise primary data
    data     = NULL;
    maxsize  = 0;

    // initialise record scheduling data
    list           = NULL;
    priority_score = 0.0;
    overlaps       = 0;
}

ADVBProg::DVBPROG *ADVBProg::ReadData(AStdData& fp, bool readheader)
{
    static const uint8_t bigendian = (uint8_t)MachineIsBigEndian();
    DVBPROG _data, *pdata = NULL;
    uint8_t swap = AStdData::SWAP_NEVER;

    if (readheader) {
        uint8_t header;

        if (fp.readitem(header)) {
            bool _bigendian = ((header & 0x80) != 0);

            swap = (_bigendian != bigendian) ? AStdData::SWAP_ALWAYS : AStdData::SWAP_NEVER;
        }
        else return NULL;
    }

    if (fp.readitem(_data.start, swap) &&
        fp.readitem(_data.stop, swap) &&
        fp.readitem(_data.recstart, swap) &&
        fp.readitem(_data.recstop, swap) &&
        fp.readitem(_data.actstart, swap) &&
        fp.readitem(_data.actstop, swap) &&
        fp.readitem(_data.filesize, swap) &&
        fp.readitem(_data.flags, swap) &&
        fp.readitem(_data.episode.valid, swap) &&
        fp.readitem(_data.episode.series, swap) &&
        fp.readitem(_data.episode.episode, swap) &&
        fp.readitem(_data.episode.episodes, swap) &&
        fp.readitem(_data.assignedepisode, swap) &&
        fp.readitem(_data.year, swap) &&
        fp.readitem(_data.jobid, swap) &&
        fp.readitem(_data.score, swap) &&
        fp.readitem(_data.prehandle, swap) &&
        fp.readitem(_data.posthandle, swap) &&
        fp.readitem(_data.dvbcard, swap) &&
        fp.readitem(_data.pri, swap)) {
        uint16_t *strings = &_data.strings.channel;
        uint_t i;

        for (i = 0; (i < StringCount); i++) {
            if (!fp.readitem(strings[i], swap)) {
                break;
            }
        }

        if ((i == StringCount) &&
            ((pdata = (DVBPROG *)calloc(1, sizeof(_data) + _data.strings.end)) != NULL)) {
            memcpy(pdata, &_data, sizeof(*pdata));

            if (fp.readbytes(pdata->strdata, pdata->strings.end) != pdata->strings.end) {
                free(pdata);
                pdata = NULL;
            }
        }
    }

    return pdata;
}

bool ADVBProg::WriteData(AStdData& fp, bool writeheader) const
{
    static const uint8_t header = ((uint8_t)MachineIsBigEndian() << 7) | DVBDATVERSION;
    bool success = false;

    if ((!writeheader || fp.writeitem(header)) &&
        fp.writeitem(data->start) &&
        fp.writeitem(data->stop) &&
        fp.writeitem(data->recstart) &&
        fp.writeitem(data->recstop) &&
        fp.writeitem(data->actstart) &&
        fp.writeitem(data->actstop) &&
        fp.writeitem(data->filesize) &&
        fp.writeitem(data->flags) &&
        fp.writeitem(data->episode.valid) &&
        fp.writeitem(data->episode.series) &&
        fp.writeitem(data->episode.episode) &&
        fp.writeitem(data->episode.episodes) &&
        fp.writeitem(data->assignedepisode) &&
        fp.writeitem(data->year) &&
        fp.writeitem(data->jobid) &&
        fp.writeitem(data->score) &&
        fp.writeitem(data->prehandle) &&
        fp.writeitem(data->posthandle) &&
        fp.writeitem(data->dvbcard) &&
        fp.writeitem(data->pri)) {
        uint16_t *strings = &data->strings.channel;
        uint_t i;

        for (i = 0; (i < StringCount); i++) {
            if (!fp.writeitem(strings[i])) {
                break;
            }
        }

        if (i == StringCount) {
            success = (fp.writebytes(data->strdata, data->strings.end) == data->strings.end);
        }
    }

    return success;
}

const ADVBProg::FIELD *ADVBProg::GetFields(uint_t& nfields)
{
    nfields = NUMBEROF(fields);
    return fields;
}

uint16_t ADVBProg::GetDirDataOffset()
{
    return DVBPROG_OFFSET(strings.dir);
}

uint16_t ADVBProg::GetUserDataOffset()
{
    return DVBPROG_OFFSET(strings.user);
}

uint16_t ADVBProg::GetActorsDataOffset()
{
    return DVBPROG_OFFSET(strings.actors);
}

uint16_t ADVBProg::GetSubCategoryDataOffset()
{
    return DVBPROG_OFFSET(strings.subcategory);
}

uint16_t ADVBProg::GetPriDataOffset()
{
    return DVBPROG_OFFSET(pri);
}

uint16_t ADVBProg::GetScoreDataOffset()
{
    return DVBPROG_OFFSET(score);
}

void ADVBProg::ModifySearchValue(const ADVBPatterns::FIELD *field, AString& value)
{
    if ((stricmp(field->name, "channel")    == 0) ||
        (stricmp(field->name, "dvbchannel") == 0)) {
        value = value.SearchAndReplace("_", " ");
    }
    else if (stricmp(field->name, "tags") == 0) {
        value = "|" + value + "|";
    }
}

/*--------------------------------------------------------------------------------*/
/** Initialise programme from binary stream
 *
 * @param fp AStdData object to read data from
 *
 * @return reference to this object
 */
/*--------------------------------------------------------------------------------*/
ADVBProg& ADVBProg::operator = (AStdData& fp)
{
    Delete();

    if ((data = ReadData(fp)) == NULL) {
        Delete();
    }

    return *this;
}

/*--------------------------------------------------------------------------------*/
/** Base 64 encode programme data
 */
/*--------------------------------------------------------------------------------*/
AString ADVBProg::Base64Encode() const
{
    AStdMemFile mem;
    AString str;

    if (mem.open("w")) {
        if (WriteData(mem, false)) {
            str = ::Base64Encode(mem.GetData(), mem.GetLength());
        }
        mem.close();
    }

    return str;
}

/*--------------------------------------------------------------------------------*/
/** Decode programme data from base64 encoded string
 *
 * @param str base64 string
 *
 * @return true if programme data decoded sucessfully
 */
/*--------------------------------------------------------------------------------*/
bool ADVBProg::Base64Decode(const AString& str)
{
    AStdMemFile mem;

    if (mem.open("w")) {
        if (mem.Base64Decode(str)) {
            Delete();

            mem.rewind();
            if ((data = ReadData(mem, false)) == NULL) {
                Delete();
            }
        }

        mem.close();
    }

    return Valid();
}

void ADVBProg::SetMarker(AString& marker, const AString& field)
{
    marker.printf("\n%s=", field.str());
}

bool ADVBProg::FieldExists(const AString& str, const AString& field, int p, int *pos)
{
    AString marker;
    int p1;

    SetMarker(marker, field);

    if ((p1 = str.PosNoCase(marker, p)) >= p) {
        if (pos) *pos = p1;
    }
    else if (pos) *pos = -1;

    return (p1 >= p);
}

bool ADVBProg::GetFlag(uint8_t flag) const
{
    bool set = false;

    if (flag < Flag_count) set = ((data->flags & (1UL << flag)) != 0);

    switch (flag) {
        case Flag_exists:
            set = IsAvailable();
            break;

        case Flag_converted:
            set = IsConverted();
            break;

        case Flag_film:
            set = IsFilm();
            break;

        case Flag_recordable:
            set = IsRecordable();
            break;

        case Flag_convertedexists:
            set = IsAvailable(true);
            break;

        case Flag_unconvertedexists:
            set = IsAvailable(false);
            break;

        case Flag_archived:
            set = IsArchived();
            break;

        case Flag_failed:
            set = HasFailed();
            break;

        case Flag_radioprogramme: {
            const ADVBChannelList& channelist = ADVBChannelList::Get();
            const ADVBChannelList::CHANNEL *channel = channelist.GetChannelByName(GetDVBChannel());

            set = (channel && channel->dvb.hasaudio && !channel->dvb.hasvideo);
            break;
        }

        case Flag_tvprogramme: {
            const ADVBChannelList& channelist = ADVBChannelList::Get();
            const ADVBChannelList::CHANNEL *channel = channelist.GetChannelByName(GetDVBChannel());

            set = (channel && channel->dvb.hasaudio && channel->dvb.hasvideo);
            break;
        }
    }

    return set;
}

AString ADVBProg::GetField(const AString& str, const AString& field, int p, int *pos)
{
    AString marker;
    AString res;
    int p1;

    SetMarker(marker, field);

    if ((p1 = str.PosNoCase(marker, p)) >= p) {
        int p2;

        if (pos) *pos = p1;

        p1 += marker.len();
        if ((p2 = str.PosNoCase("\n", p1)) < 0) p2 = str.len();

        res = str.Mid(p1, p2 - p1);
    }
    else if (pos) *pos = -1;

    return res;
}

bool ADVBProg::SetUUID()
{
    AString uuid;

    uuid.printf("%s:%s:%s", GetStartDT().DateFormat("%Y%M%D%h%m").str(), GetChannel(), GetTitle());

    return SetString(&data->strings.uuid, uuid);
}

void ADVBProg::SetRecordingComplete()
{
    const ADVBConfig& config = ADVBConfig::Get();
    uint64_t maxreclag = (uint64_t)config.GetMaxRecordLag(GetUser(), GetModifiedCategory()) * 1000;

    SetFlag(Flag_incompleterecording, !IgnoreLateStart() &&
                                        ((data->actstart > (data->start + maxreclag)) ||
                                         (data->actstop  < MIN(data->stop, data->recstop - 1000))));
}

uint64_t ADVBProg::GetDate(const AString& str, const AString& fieldname) const
{
    AString date = GetField(str, fieldname);

    if (date.Empty())   return 0;
    if (date[0] == '$') return (uint64_t)date;

    return (uint64_t)ADateTime(date).LocalToUTC();
}

AString ADVBProg::GetProgrammeKey() const
{
    AString key = GetTitle();

    if (IsFilm()) key += ":Film";

    return key;
}

/*--------------------------------------------------------------------------------*/
/** Initialise programme from text field list
 *
 * @param str field list in 'field=value' format (separated by newlines)
 *
 * @return reference to this object
 */
/*--------------------------------------------------------------------------------*/
ADVBProg& ADVBProg::operator = (const AString& str)
{
    static const AString tsod = "tsod.plus-1.";
    const ADVBConfig& config = ADVBConfig::Get();
    const char *pstr;
    AString _str;
    int p = 0;

    Delete();

    data->start    = GetDate(str, "start");
    data->stop     = GetDate(str, "stop");
    data->recstart = GetDate(str, "recstart");
    data->recstop  = GetDate(str, "recstop");
    data->actstart = GetDate(str, "actstart");
    data->actstop  = GetDate(str, "actstop");

    data->filesize = (uint64_t)GetField(str, "filesize");
    data->flags    = (uint32_t)GetField(str, "flags");

    if (FieldExists(str, "episodenum:xmltv_ns")) SetString(&data->strings.episodenum, GetField(str, "episodenum:xmltv_ns"));
    else                                         SetString(&data->strings.episodenum, GetField(str, "episodenum"));

    if      (FieldExists(str, "episodenum:dd_progid"))          SetString(&data->strings.episodeid, GetField(str, "episodenum:dd_progid"));
    else if (FieldExists(str, "episodenum:brandseriesepisode")) SetString(&data->strings.episodeid, GetField(str, "episodenum:brandseriesepisode"));
    else if (FieldExists(str, "episodeid"))                     SetString(&data->strings.episodeid, GetField(str, "episodeid"));

    if ((pstr = GetString(data->strings.episodenum))[0]) {
        data->episode = GetEpisode(pstr);
    }
    else {
        data->episode.series   = (uint_t)GetField(str, "series");
        data->episode.episode  = (uint_t)GetField(str, "episode");
        data->episode.episodes = (uint_t)GetField(str, "episodes");
        data->episode.valid    = (data->episode.series || data->episode.episode || data->episode.episodes);
    }

    SetString(&data->strings.title, GetField(str, "title"));
    SetString(&data->strings.subtitle, GetField(str, "subtitle"));

    if (!GetString(data->strings.subtitle)[0] && data->episode.valid && data->episode.episode && data->episode.episodes) {
        AString str;

        str.printf("Episode %u/%u", data->episode.episode, data->episode.episodes);

        SetString(&data->strings.subtitle, str);
    }

    AString channel = GetField(str, "channel");
    SetString(&data->strings.channel, channel);
    if (channel.EndsWith("+1")) {
        SetString(&data->strings.basechannel, channel.Left(channel.len() - 2).Words(0));
        SetFlag(Flag_plus1);
    }
    else SetString(&data->strings.basechannel, channel);

    SetString(&data->strings.channelid, GetField(str, "channelid"));
    AString dvbchannel = GetField(str, "dvbchannel");
    if (dvbchannel.Empty()) dvbchannel = ADVBChannelList::Get().LookupDVBChannel(channel);
    SetString(&data->strings.dvbchannel, dvbchannel);
    SetString(&data->strings.desc, GetField(str, "desc"));

    {
        AString category, subcategory = GetField(str, "subcategory").DeEscapify();
        bool    ignoreothercategories = subcategory.Valid();

        for (p = 0; (_str = GetField(str, "category", p, &p)).Valid(); p++) {
            if (_str.ToLower() == "film") {
                category = _str;
                break;
            }
        }
        for (p = 0; (_str = GetField(str, "category", p, &p)).Valid(); p++) {
            if      (category.Empty())      category = _str;
            else if (ignoreothercategories) break;
            else if (_str.ToLower() != "film") {
                subcategory += _str + "\n";
            }
        }

        SetString(&data->strings.category, category);

        SetString(&data->strings.subcategory, subcategory);
    }

    SetString(&data->strings.director, GetField(str, "director"));

    if (FieldExists(str, "previouslyshown")) SetFlag(Flag_repeat);

    AString actors = GetField(str, "actors").SearchAndReplace(",", "\n"), actor;
    for (p = 0; (actor = GetField(str, "actor", p, &p)).Valid(); p++) {
        actors.printf("%s\n", actor.str());
    }
    SetString(&data->strings.actors, actors);

    SetString(&data->strings.prefs, GetField(str, "prefs").SearchAndReplace(",", "\n"));
    SetString(&data->strings.tags,  GetField(str, "tags"));

    SetUser(GetField(str, "user"));
    if (FieldExists(str, "dir")) SetDir(GetField(str, "dir"));
    SetFilename(GetField(str, "filename"));
    SetPattern(GetField(str, "pattern"));

    SetUUID();

    if (FieldExists(str, "icon")) {
        AString icon = GetField(str, "icon");
        SetString(&data->strings.icon, icon);
        ADVBIconCache::Get().SetIcon("programme", GetProgrammeKey(), icon);
    }

    if (FieldExists(str, "rating")) SetString(&data->strings.rating, GetField(str, "rating"));

    data->assignedepisode = (uint16_t)GetField(str, "assignedepisode");
    if (FieldExists(str, "date")) {
        data->year = (uint16_t)GetField(str, "date");
    }
    else data->year = (uint16_t)GetField(str, "year");

    data->jobid = (uint_t)GetField(str, "jobid");
    data->score = (sint_t)GetField(str, "score");

    if (FieldExists(str, "prehandle"))  data->prehandle  = (uint_t)GetField(str, "prehandle");
    else                                data->prehandle  = (uint_t)config.GetUserConfigItem(GetUser(), "prehandle");
    if (FieldExists(str, "posthandle")) data->posthandle = (uint_t)GetField(str, "posthandle");
    else                                data->posthandle = (uint_t)config.GetUserConfigItem(GetUser(), "posthandle");
    if (FieldExists(str, "pri"))        data->pri        = (int)GetField(str, "pri");
    else                                data->pri        = (int)config.GetUserConfigItem(GetUser(), "pri");

    data->dvbcard = (uint_t)GetField(str, "card");

    SearchAndReplace("\xc2\xa3", "£");

    if (Valid()) FixData();

    if (!Valid()) Delete();

    return *this;
}

/*--------------------------------------------------------------------------------*/
/** Initialise from an existing programme object
 *
 * @param obj existing programme object
 *
 * @return reference to this object
 */
/*--------------------------------------------------------------------------------*/
ADVBProg& ADVBProg::operator = (const ADVBProg& obj)
{
    bool success = false;

    Delete();

    if (obj.Valid()) {
        if (!data || ((sizeof(*obj.data) + obj.data->strings.end) > maxsize)) {
            if (data) free(data);

            maxsize = sizeof(*obj.data) + obj.data->strings.end;
            data = (DVBPROG *)calloc(1, maxsize);
        }

        if (data && ((sizeof(*obj.data) + obj.data->strings.end) <= maxsize)) {
            memcpy(data, obj.data, sizeof(*obj.data) + obj.data->strings.end);

            success = true;
        }
        else debug("Error in copy\n");
    }

    if (!success) Delete();

    return *this;
}

/*--------------------------------------------------------------------------------*/
/** Modify the current programme object from an existing programme object
 *
 * @param obj existing programme object
 *
 * @return reference to this object
 *
 * @note the following fields are copied from the supplied programme if it is valid:
 *   RecordStart
 *   RecordStop
 *   ActualStart
 *   ActualStop
 *   Flags
 *   FileSize
 *   User
 *   Dir
 *   Filename
 *   Pattern
 *   Prefs
 *   Pri
 *   Score
 *   DVBCard
 *   JobID
 */
/*--------------------------------------------------------------------------------*/
ADVBProg& ADVBProg::Modify(const ADVBProg& obj)
{
    if (obj.Valid()) {
        SetRecordStart(obj.GetRecordStart());
        SetRecordStop(obj.GetRecordStop());
        SetActualStart(obj.GetActualStart());
        SetActualStop(obj.GetActualStop());

        SetFlags(obj.GetFlags());

        SetFileSize(obj.GetFileSize());

        SetUser(obj.GetUser());
        SetDir(obj.GetDir());
        SetFilename(obj.GetFilename());
        SetPattern(obj.GetPattern());
        SetPrefs(obj.GetPrefs());

        SetPri(obj.GetPri());
        SetScore(obj.GetScore());
        SetDVBCard(obj.GetDVBCard());
        SetJobID(obj.GetJobID());
    }

    return *this;
}

/*--------------------------------------------------------------------------------*/
/** Write programme data to stream as binary (inverse of operator = (AStdFile& fp))
 *
 * @param fp AStdData object to write data to
 *
 * @return true if write successful
 */
/*--------------------------------------------------------------------------------*/
bool ADVBProg::WriteToFile(AStdData& fp) const
{
    return WriteData(fp);
}

/*--------------------------------------------------------------------------------*/
/** Return string representing programme data (inverse of operator = (const AString& str))
 *
 * @return string containing encoded data
 */
/*--------------------------------------------------------------------------------*/
AString ADVBProg::ExportToText() const
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString str;
    const char *p;

    str.printf("\n");
    str.printf("start=%s\n", GetHex(data->start).str());
    str.printf("stop=%s\n", GetHex(data->stop).str());
    if (data->recstart || data->recstop) {
        str.printf("recstart=%s\n", GetHex(data->recstart).str());
        str.printf("recstop=%s\n", GetHex(data->recstop).str());
    }
    if (data->actstart || data->actstop) {
        str.printf("actstart=%s\n", GetHex(data->actstart).str());
        str.printf("actstop=%s\n", GetHex(data->actstop).str());
    }
    str.printf("channel=%s\n", GetString(data->strings.channel));
    str.printf("basechannel=%s\n", GetString(data->strings.basechannel));
    str.printf("channelid=%s\n", GetString(data->strings.channelid));
    if ((p = GetString(data->strings.dvbchannel))[0]) str.printf("dvbchannel=%s\n", p);
    str.printf("title=%s\n", GetString(data->strings.title));
    if ((p = GetString(data->strings.subtitle))[0]) str.printf("subtitle=%s\n", p);
    if ((p = GetString(data->strings.desc))[0]) str.printf("desc=%s\n", p);
    if ((p = GetString(data->strings.category))[0]) str.printf("category=%s\n", p);
    if ((p = GetString(data->strings.subcategory))[0]) str.printf("subcategory=%s\n", AString(p).Escapify().str());
    if ((p = GetString(data->strings.director))[0]) str.printf("director=%s\n", p);
    if ((p = GetString(data->strings.episodenum))[0]) str.printf("episodenum=%s\n", p);
    if ((p = GetString(data->strings.user))[0]) str.printf("user=%s\n", p);
    str.printf("dir=%s\n", GetString(data->strings.dir));
    if ((p = GetString(data->strings.filename))[0]) str.printf("filename=%s\n", p);
    if ((p = GetString(data->strings.pattern))[0]) str.printf("pattern=%s\n", p);
    str.printf("uuid=%s\n", GetString(data->strings.uuid));
    if ((p = GetString(data->strings.actors))[0]) str.printf("actors=%s\n", AString(p).SearchAndReplace("\n", ",").str());
    if ((p = GetString(data->strings.prefs))[0]) str.printf("prefs=%s\n", AString(p).SearchAndReplace("\n", ",").str());

    if (data->episode.series) str.printf("series=%u\n", (uint_t)data->episode.series);
    if (data->episode.episode) str.printf("episode=%u\n", (uint_t)data->episode.episode);
    if (data->episode.episodes) str.printf("episodes=%u\n", (uint_t)data->episode.episodes);

    if (data->filesize) str.printf("filesize=%s\n", AValue(data->filesize).ToString().str());
    str.printf("flags=$%08x\n", data->flags);

    if ((p = GetString(data->strings.episodeid))[0]) str.printf("episodeid=%s\n", p);

    AString icon = GetString(data->strings.icon);
    if (icon.Empty() && config.UseOldProgrammeIcon(GetTitle(), GetModifiedCategory())) icon = ADVBIconCache::Get().GetIcon("programme", GetProgrammeKey());
    if (icon.Valid()) str.printf("icon=%s\n", icon.str());

    icon = ADVBIconCache::Get().GetIcon("channel", GetChannel());
    if (icon.Empty() && config.UseOldChannelIcon(GetTitle(), GetModifiedCategory())) icon = ADVBIconCache::Get().GetIcon("channel", GetDVBChannel());
    if (icon.Valid()) str.printf("channelicon=%s", icon.str());

    if ((p = GetString(data->strings.rating))[0])    str.printf("rating=%s\n", p);

    if (data->assignedepisode) str.printf("assignedepisode=%u\n", data->assignedepisode);
    if (data->year) str.printf("year=%u\n", data->year);

    if (GetString(data->strings.pattern)[0]) {
        str.printf("score=%d\n", (sint_t)data->score);
        str.printf("prehandle=%u\n", (uint_t)data->prehandle);
        str.printf("posthandle=%u\n", (uint_t)data->posthandle);
        str.printf("pri=%d\n", (int)data->pri);
    }

    if ((p = GetString(data->strings.tags))[0]) {
        str.printf("tags=%s\n", p);
    }

    if (RecordDataValid()) {
        str.printf("jobid=%u\n", data->jobid);
        str.printf("dvbcard=%u\n", (uint_t)data->dvbcard);
    }

    return str;
}

/*--------------------------------------------------------------------------------*/
/** Export programme data to JSON value
 *
 * @param doc base document of obj
 * @param obj JSON value to store data in
 * @param includebase64 true to include base64 representation of programme data in export
 */
/*--------------------------------------------------------------------------------*/
void ADVBProg::ExportToJSON(rapidjson::Document& doc, rapidjson::Value& obj, bool includebase64) const
{
    const ADVBConfig& config = ADVBConfig::Get();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
    const char *p;

    obj.SetObject();

    // NOTE: times are local to PC, NOT UTC
    obj.AddMember("start", rapidjson::Value(JSONTimeOffset(data->start)), allocator);
    obj.AddMember("stop", rapidjson::Value(JSONTimeOffset(data->stop)), allocator);
    if (data->recstart || data->recstop) {
        obj.AddMember("recstart", rapidjson::Value(JSONTimeOffset(data->recstart)), allocator);
        obj.AddMember("recstop", rapidjson::Value(JSONTimeOffset(data->recstop)), allocator);
    }
    if (data->actstart || data->actstop) {
        obj.AddMember("actstart", rapidjson::Value(JSONTimeOffset(data->actstart)), allocator);
        obj.AddMember("actstop", rapidjson::Value(JSONTimeOffset(data->actstop)), allocator);
    }
    obj.AddMember("channel", rapidjson::Value(GetString(data->strings.channel), allocator), allocator);
    obj.AddMember("basechannel", rapidjson::Value(GetString(data->strings.basechannel), allocator), allocator);
    obj.AddMember("channelid", rapidjson::Value(GetString(data->strings.channelid), allocator), allocator);
    if ((p = GetString(data->strings.dvbchannel))[0]) obj.AddMember("dvbchannel", rapidjson::Value(p, allocator), allocator);
    obj.AddMember("title", rapidjson::Value(GetString(data->strings.title), allocator), allocator);
    if ((p = GetString(data->strings.subtitle))[0]) obj.AddMember("subtitle", rapidjson::Value(p, allocator), allocator);
    if ((p = GetString(data->strings.desc))[0]) obj.AddMember("desc", rapidjson::Value(p, allocator), allocator);
    if ((p = GetString(data->strings.category))[0]) obj.AddMember("category", rapidjson::Value(p, allocator), allocator);
    if ((p = GetString(data->strings.subcategory))[0]) {
        rapidjson::Value subobj;

        subobj.SetArray();

        AString subcategory = p;
        uint_t  i, n = subcategory.CountLines();
        for (i = 0; i < n; i++) {
            subobj.PushBack(rapidjson::Value(subcategory.Line(i).str(), allocator), allocator);
        }

        obj.AddMember("subcategory", subobj, allocator);
    }
    if ((p = GetString(data->strings.director))[0]) obj.AddMember("director", rapidjson::Value(p, allocator), allocator);
    if ((p = GetString(data->strings.episodenum))[0]) obj.AddMember("episodenum", rapidjson::Value(p, allocator), allocator);
    if ((p = GetString(data->strings.user))[0]) obj.AddMember("user", rapidjson::Value(p, allocator), allocator);
    obj.AddMember("dir", rapidjson::Value(GetString(data->strings.dir), allocator), allocator);
    if ((p = GetString(data->strings.filename))[0]) obj.AddMember("filename", rapidjson::Value(p, allocator), allocator);
    if (!IsConverted()) obj.AddMember("convertedfilename", rapidjson::Value(GenerateFilename(true), allocator), allocator);
    if ((p = GetString(data->strings.pattern))[0]) obj.AddMember("pattern", rapidjson::Value(p, allocator), allocator);
    obj.AddMember("uuid", rapidjson::Value(GetString(data->strings.uuid), allocator), allocator);
    if ((p = GetString(data->strings.actors))[0]) {
        rapidjson::Value subobj;
        AString _actors = AString(p);
        uint_t i, n = _actors.CountLines("\n", 0);

        subobj.SetArray();

        for (i = 0; i < n; i++) {
            subobj.PushBack(rapidjson::Value(_actors.Line(i, "\n", 0).str(), allocator), allocator);
        }

        obj.AddMember("actors", subobj, allocator);
    }
    if ((p = GetString(data->strings.prefs))[0]) {
        rapidjson::Value subobj;
        AString _prefs = AString(p);
        uint_t i, n = _prefs.CountLines("\n", 0);

        subobj.SetArray();

        for (i = 0; i < n; i++) {
            subobj.PushBack(rapidjson::Value(_prefs.Line(i, "\n", 0).str(), allocator), allocator);
        }

        obj.AddMember("prefs", subobj, allocator);
    }

    if ((p = GetString(data->strings.tags))[0]) {
        rapidjson::Value subobj;
        AString _tags = AString(p);
        uint_t i, n = _tags.CountLines("||", 0);

        subobj.SetArray();

        for (i = 0; i < n; i++) {
            AString tag = _tags.Line(i, "||");
            if (tag.FirstChar() == '|') tag = tag.Mid(1);
            if (tag.LastChar()  == '|') tag = tag.Left(tag.len() - 1);
            subobj.PushBack(rapidjson::Value(tag.str(), allocator), allocator);
        }

        obj.AddMember("tags", subobj, allocator);
    }

    if (data->episode.valid) {
        rapidjson::Value subobj;

        subobj.SetObject();

        if (data->episode.series) {
            subobj.AddMember("series", rapidjson::Value(data->episode.series), allocator);
        }
        if (data->episode.episode) {
            subobj.AddMember("episode", rapidjson::Value(data->episode.episode), allocator);
        }
        if (data->episode.episodes) {
            subobj.AddMember("episodes", rapidjson::Value(data->episode.episodes), allocator);
        }

        obj.AddMember("episode", subobj, allocator);
    }

    if (GetString(data->strings.episodeid)[0]) obj.AddMember("episodeid", rapidjson::Value(GetString(data->strings.episodeid), allocator), allocator);

    AString icon = GetString(data->strings.icon);
    if (icon.Empty() && config.UseOldProgrammeIcon(GetTitle(), GetModifiedCategory())) icon = ADVBIconCache::Get().GetIcon("programme", GetProgrammeKey());
    if (icon.Valid()) obj.AddMember("icon", rapidjson::Value(icon, allocator), allocator);

    icon = ADVBIconCache::Get().GetIcon("channel", GetChannel());
    if (icon.Empty() && config.UseOldChannelIcon(GetTitle(), GetModifiedCategory())) icon = ADVBIconCache::Get().GetIcon("channel", GetDVBChannel());
    if (icon.Valid()) obj.AddMember("channelicon", rapidjson::Value(icon, allocator), allocator);

    if ((p = GetString(data->strings.rating))[0]) obj.AddMember("rating", rapidjson::Value(p, allocator), allocator);

    {
        rapidjson::Value subobj;
        AHash hash;
        uint_t i;

        subobj.SetObject();

        subobj.AddMember("bitmap", rapidjson::Value(data->flags), allocator);

        for (i = 0; i < NUMBEROF(fields); i++) {
            if (RANGE(fields[i].type, ADVBPatterns::FieldType_flag, ADVBPatterns::FieldType_lastflag) && !hash.Exists(fields[i].name)) {
                hash.Insert(fields[i].name, 0);
                subobj.AddMember(rapidjson::Value(fields[i].name, allocator), rapidjson::Value(GetFlag(fields[i].type - ADVBPatterns::FieldType_flag)), allocator);
            }
        }

        obj.AddMember("flags", subobj, allocator);
    }

    if (data->filesize) {
        obj.AddMember("filesize", rapidjson::Value(data->filesize), allocator);

        uint_t rate = GetRate() / 1024;
        if (rate) obj.AddMember("rate", rapidjson::Value(rate), allocator);
    }

    if (data->assignedepisode) obj.AddMember("assignedepisode", rapidjson::Value(data->assignedepisode), allocator);
    if (data->year) obj.AddMember("year", rapidjson::Value(data->year), allocator);

    if (GetString(data->strings.pattern)[0]) {
        obj.AddMember("score", rapidjson::Value(data->score), allocator);
        obj.AddMember("prehandle", rapidjson::Value(data->prehandle), allocator);
        obj.AddMember("posthandle", rapidjson::Value(data->posthandle), allocator);
        obj.AddMember("pri", rapidjson::Value(data->pri), allocator);
    }

    if (RecordDataValid()) {
        obj.AddMember("jobid", rapidjson::Value(data->jobid), allocator);
        obj.AddMember("dvbcard", rapidjson::Value(data->dvbcard), allocator);

        const AString filename = GetString(data->strings.filename);
        AString relpath;
        if (data->actstart && data->actstop && filename[0] && IsConverted() && (relpath = config.GetRelativePath(filename)).Valid()) {
            bool exists = AStdFile::exists(filename);

            obj.AddMember("exists", rapidjson::Value(exists), allocator);
            if (exists) {
                rapidjson::Value subobj;
                AList subfiles;

                CollectFiles(filename.PathPart(), filename.FilePart().Prefix() + ".*", RECURSE_ALL_SUBDIRS, subfiles);

                obj.AddMember("file", rapidjson::Value(relpath, allocator), allocator);

                subobj.SetArray();

                const AString *subfile = AString::Cast(subfiles.First());
                while (subfile) {
                    if ((*subfile != filename) && (relpath = config.GetRelativePath(*subfile)).Valid()) {
                        subobj.PushBack(rapidjson::Value(relpath.str(), allocator), allocator);
                    }

                    subfile = subfile->Next();
                }

                if (subobj.Size()) {
                    obj.AddMember("subfiles", subobj, allocator);
                }
            }
        }
    }

    if (includebase64) {
        obj.AddMember("base64", rapidjson::Value(Base64Encode().str(), allocator), allocator);
    }
}

/*--------------------------------------------------------------------------------*/
/** Delete programme data
 */
/*--------------------------------------------------------------------------------*/
void ADVBProg::Delete()
{
    if (!data) {
        maxsize = sizeof(*data) + StringCount;
        data = (DVBPROG *)calloc(1, maxsize);
    }
    else memset(data, 0, maxsize);

    if (data) {
        uint16_t *strings = &data->strings.channel;
        uint_t i;

        for (i = 0; i < StringCount; i++) {
            strings[i] = i;
        }
    }
}

bool ADVBProg::FixData()
{
    const ADVBConfig& config = ADVBConfig::Get();
    bool changed = false;

    if (CompareCase(GetTitle(), "Futurama") == 0) {
        EPISODE ep = GetEpisode(GetEpisodeNum());

        if (ep.valid) {
            if      (ep.series == 1) ep.episodes = 20;
            else if (ep.series == 2) {
                ep.series--;
                ep.episode += 9;
            }
            else if (ep.series > 2) ep.series--;

            if ((ep.valid    != data->episode.valid) ||
                (ep.series   != data->episode.series) ||
                (ep.episode  != data->episode.episode) ||
                (ep.episodes != data->episode.episodes)) {
                AString filename = GetFilename();

                AString desc1 = GetDescription(1);
                data->episode = ep;
                AString desc2 = GetDescription(1);

                config.printf("Changed '%s' to '%s'", desc1.str(), desc2.str());

                if (filename.Valid()) {
                    SetFilename(GenerateFilename());

                    AString newfilename = GetFilename();
                    if (AStdFile::exists(filename)) {
                        if (rename(filename, newfilename) != 0) {
                            config.printf("Failed to rename '%s' to '%s'", filename.str(), newfilename.str());
                        }
                        else {
                            config.printf("Renamed '%s' to '%s'", filename.str(), newfilename.str());
                        }
                    }
                }

                changed = true;
            }
        }
    }

    return changed;
}

bool ADVBProg::SetString(const uint16_t *offset, const char *str)
{
    bool success = false;

    //debug("ADVBProg:<%08lx>: Setting field offset %u as '%s'\n", (uptr_t)this, (uint_t)((uptr_t)offset - (uptr_t)data), str);
    if ((offset >= &data->strings.channel) && (offset < &data->strings.end)) {
        uint16_t *strings = &data->strings.channel;
        uint_t   field    = offset - strings;
        uint16_t len1     = strings[field + 1] - strings[field];
        uint16_t len2     = strlen(str) + 1;
        sint16_t diff     = len2 - len1;
        uint_t i;

        // debug("Before:\n");
        // for (i = 0; i < StringCount; i++) {
        //  debug("\tField %2u at %4u '%s'\n", i, strings[i], data->strdata + strings[i]);
        // }
        // debug("Offset %u ('%s') is field %u, diff = %d\n", (uint_t)((uptr_t)offset - (uptr_t)data), data->strdata + strings[field], field, diff);

        if ((diff != 0) || (strcmp(str, data->strdata + strings[field]) != 0)) {
            if ((sizeof(*data) + data->strings.end + diff) > maxsize) {
                maxsize = (sizeof(*data) + data->strings.end + diff + 256) & ~255;
                data    = (DVBPROG *)realloc(data, maxsize);
                if (data && ((sizeof(*data) + data->strings.end) < maxsize)) memset(data->strdata + data->strings.end, 0, maxsize - (sizeof(*data) + data->strings.end));
                strings = &data->strings.channel;
            }

            if (data) {
                if ((diff != 0) && (data->strings.end > strings[field + 1])) {
                    //debug("Moving strings for fields %u to %u by %d bytes\n", field + 1, StringCount, diff);
                    memmove(data->strdata + strings[field + 1] + diff, data->strdata + strings[field + 1], data->strings.end - strings[field + 1]);
                }

                if (diff != 0) {
                    //debug("Updating offsets for fields %u to %u by %d bytes\n", field + 1, StringCount, diff);
                    for (i = field + 1; i < StringCount; i++) {
                        strings[i] += diff;
                    }
                }

                strcpy(&data->strdata[0] + strings[field], str);

                success = true;
            }

            // debug("After:\n");
            // for (i = 0; i < StringCount; i++) {
            //  debug("\tField %2u at %4u '%s'\n", i, strings[i], data->strdata + strings[i]);
            // }
        }
        else success = true;
    }

    return success;
}

void ADVBProg::SetFlag(uint8_t flag, bool set)
{
    if (set) data->flags |= GetFlagMask(flag, set);
    else     data->flags &= GetFlagMask(flag, set);
}

/*--------------------------------------------------------------------------------*/
/** Parse a textual list of fields to populate a comparison priority list
 *
 * @param fieldlist list of field numbers to populate (appended to)
 * @param str a string containing the list of field names
 * @param sep separator for the above
 * @param reverse true to reverse comparison by reversing every field
 *
 * @return true if all field names parsed successfully
 */
/*--------------------------------------------------------------------------------*/
bool ADVBProg::ParseFieldList(FIELDLIST& fieldlist, const AString& str, const AString& sep, bool reverse)
{
    const sint_t mul = reverse ? -1 : 1;                        // multiplier for optional comparison reversal
    uint_t i, n = str.CountLines(sep, 0);
    bool success = (n > 0);

    for (i = 0; i < n; i++) {
        AString name = str.Line(i, sep);
        const FIELD *fieldptr;
        bool reversefield = (name.Left(1) == "-");              // a minus sign means reverse conmparison for this field

        if (reversefield) {
            name = name.Mid(1);                                 // remove minus sign
        }

        if ((fieldptr = (const FIELD *)fieldhash.Read(name)) != NULL) {
            const sint_t mul2 = mul * (reversefield ? -1 : 1);  // new multiplier is product of global reversal multiplier and field reversal flag
            const sint_t n = fieldptr - fields;
            fieldlist.push_back(mul2 * n);                      // multiply field number of reversal multiplier
        }
        else success = false;
    }

    return success;
}

/*--------------------------------------------------------------------------------*/
/** Compare two programmes using the provided list of fields
 *
 * @param prog1 programme 1
 * @param prog2 programme 2
 * @param fieldlist a list of fields to compare by (in priority order)
 * @param reverse ptr to optional reversal flag
 *
 * @return -1, 0 or 1 depending on whether prog1 is <, = or > prog2
 */
/*--------------------------------------------------------------------------------*/
int ADVBProg::Compare(const ADVBProg *prog1, const ADVBProg *prog2, const FIELDLIST& fieldlist, const bool *reverse)
{
    int res = 0;

    if (prog1 != prog2) {
        size_t i;

        for (i = 0; (i < fieldlist.size()) && (res == 0); i++) {
            const uint_t n = (uint_t)abs(fieldlist[i]);     // field numbers can be negative (which means their comparison result should be reversed) hence the abs()
            bool  reversefield = (fieldlist[i] < 0);        // whether the comparison should be reversed

            if (n < NUMBEROF(fields)) {
                const FIELD&  field = fields[n];
                const uint8_t *ptr1 = prog1->GetDataPtr(field.offset);
                const uint8_t *ptr2 = prog2->GetDataPtr(field.offset);

                switch (field.type) {
                    case ADVBPatterns::FieldType_string: {
                        uint16_t offset1, offset2;

                        memcpy(&offset1, ptr1, sizeof(offset1));
                        memcpy(&offset2, ptr2, sizeof(offset2));

                        res = CompareNoCase(prog1->GetString(offset1), prog2->GetString(offset2));
                        break;
                    }

                    case ADVBPatterns::FieldType_date: {
                        ADateTime dt1,  dt2;
                        uint64_t  val1, val2;

                        memcpy(&val1, ptr1, sizeof(val1));
                        dt1  = ADateTime(val1).UTCToLocal();
                        val1 = (uint64_t)dt1;

                        memcpy(&val2, ptr2, sizeof(val2));
                        dt2  = ADateTime(val2).UTCToLocal();
                        val2 = (uint64_t)dt2;

                        res = COMPARE_ITEMS(val1, val2);
                        break;
                    }

                    case ADVBPatterns::FieldType_span: {
                        uint64_t start, end;
                        uint64_t val1, val2;

                        memcpy(&start, ptr1, sizeof(start)); ptr1 += sizeof(start);
                        memcpy(&end,   ptr1, sizeof(end));
                        val1 = SUBZ(end, start);

                        memcpy(&start, ptr2, sizeof(start)); ptr2 += sizeof(start);
                        memcpy(&end,   ptr2, sizeof(end));
                        val2 = SUBZ(end, start);

                        res = COMPARE_ITEMS(val1, val2);
                        break;
                    }

                    case ADVBPatterns::FieldType_age: {
                        uint64_t val1, val2, now;

                        now = (uint64_t)ADateTime().TimeStamp(true);

                        memcpy(&val1, ptr1, sizeof(val1));
                        memcpy(&val2, ptr2, sizeof(val2));

                        val1 = SUBZ(now, val1);
                        val2 = SUBZ(now, val2);

                        res = COMPARE_ITEMS(val1, val2);
                        break;
                    }

                    case ADVBPatterns::FieldType_uint32_t: {
                        uint32_t val1, val2;

                        memcpy(&val1, ptr1, sizeof(val1));
                        memcpy(&val2, ptr2, sizeof(val2));

                        res = COMPARE_ITEMS(val1, val2);
                        break;
                    }

                    case ADVBPatterns::FieldType_sint32_t: {
                        sint32_t val1, val2;

                        memcpy(&val1, ptr1, sizeof(val1));
                        memcpy(&val2, ptr2, sizeof(val2));

                        res = COMPARE_ITEMS(val1, val2);
                        break;
                    }

                    case ADVBPatterns::FieldType_uint16_t: {
                        uint16_t val1, val2;

                        memcpy(&val1, ptr1, sizeof(val1));
                        memcpy(&val2, ptr2, sizeof(val2));

                        res = COMPARE_ITEMS(val1, val2);
                        break;
                    }

                    case ADVBPatterns::FieldType_sint16_t: {
                        sint16_t val1, val2;

                        memcpy(&val1, ptr1, sizeof(val1));
                        memcpy(&val2, ptr2, sizeof(val2));

                        res = COMPARE_ITEMS(val1, val2);
                        break;
                    }

                    case ADVBPatterns::FieldType_uint8_t: {
                        uint8_t val1, val2;

                        memcpy(&val1, ptr1, sizeof(val1));
                        memcpy(&val2, ptr2, sizeof(val2));

                        res = COMPARE_ITEMS(val1, val2);
                        break;
                    }

                    case ADVBPatterns::FieldType_sint8_t: {
                        sint8_t val1, val2;

                        memcpy(&val1, ptr1, sizeof(val1));
                        memcpy(&val2, ptr2, sizeof(val2));

                        res = COMPARE_ITEMS(val1, val2);
                        break;
                    }

                    case ADVBPatterns::FieldType_prog:
                        res = Compare(prog1, prog2);
                        break;

                    case ADVBPatterns::FieldType_external_uint32_t: {
                        uint32_t val1 = prog1->GetExternal(field.offset);
                        uint32_t val2 = prog2->GetExternal(field.offset);

                        res = COMPARE_ITEMS(val1, val2);
                        break;
                    }

                    case ADVBPatterns::FieldType_external_sint32_t: {
                        sint32_t val1 = prog1->GetExternal(field.offset);
                        sint32_t val2 = prog2->GetExternal(field.offset);

                        res = COMPARE_ITEMS(val1, val2);
                        break;
                    }

                    case ADVBPatterns::FieldType_flag...ADVBPatterns::FieldType_lastflag: {
                        uint_t flag1 = (uint_t)prog1->GetFlag(field.type - ADVBPatterns::FieldType_flag);
                        uint_t flag2 = (uint_t)prog2->GetFlag(field.type - ADVBPatterns::FieldType_flag);
                        res = (sint_t)flag1 - (sint_t)flag2;
                        break;
                    }
                }

                // if comparison is to be reversed, negate result
                if (reversefield) res = -res;
            }
        }

        // if entire comparison is to be reversed, negate result
        if (reverse && *reverse) res = -res;
    }

    return res;
}

/*--------------------------------------------------------------------------------*/
/** Compare two programmes using the provided list of fields
 *
 * @param prog1 ptr to programme 1
 * @param prog2 ptr to programme 2
 * @param reverse ptr to optional reversal flag
 *
 * @return -1, 0 or 1 depending on whether prog1 is <, = or > prog2
 *
 * @note this comparison uses a fixed set of fields to compare by:
 *       1. Programme start
 *       2. Programme channel
 *       3. Programme title
 *       4. Programme subtitle
 */
/*--------------------------------------------------------------------------------*/
int ADVBProg::Compare(const ADVBProg *prog1, const ADVBProg *prog2, const bool *reverse)
{
    int res = 0;

    if (prog1 != prog2) {
        if      (prog1->GetStart() < prog2->GetStart()) res = -1;
        else if (prog1->GetStart() > prog2->GetStart()) res =  1;
        else if (((res = CompareNoCase(prog1->GetChannel(), prog2->GetChannel())) == 0) &&
                 ((res = CompareNoCase(prog1->GetTitle(),   prog2->GetTitle()))   == 0)) {
            res = CompareNoCase(prog1->GetSubtitle(), prog2->GetSubtitle());
        }

        if (reverse && *reverse) res = -res;
    }

    return res;
}

ADVBProg::EPISODE ADVBProg::GetEpisode(const AString& str)
{
    EPISODE episode = {false, 0, 0, 0};

    if (str.Valid()) {
        uint_t _series, _episode, _episodes;

        episode.valid = true;

        if (sscanf(str, "%u.%u/%u.",
                   &_series,
                   &_episode,
                   &_episodes) == 3) {
            episode.series   = _series  + 1;
            episode.episode  = _episode + 1;
            episode.episodes = _episodes;
        }
        else if (sscanf(str, "%u./%u.",
                   &_series,
                   &_episodes) == 2) {
            episode.series   = _series  + 1;
            episode.episode  = 0;
            episode.episodes = _episodes;
        }
        else if (sscanf(str, ".%u/%u.",
                        &_episode,
                        &_episodes) == 2) {
            episode.episode  = _episode + 1;
            episode.episodes = _episodes;
        }
        else if (sscanf(str, "./%u.",
                        &_episodes) == 1) {
            episode.episode  = 0;
            episode.episodes = _episodes;
        }
        else if (sscanf(str, "%u.%u.",
                        &_series,
                        &_episode) == 2) {
            episode.series   = _series  + 1;
            episode.episode  = _episode + 1;
        }
        else if (sscanf(str, "..%u/%u",
                        &_episode,
                        &_episodes) == 2) {
            episode.episode  = _episode + 1;
            episode.episodes = _episodes + 1;
        }
        else if (sscanf(str, ".%u.",
                        &_episode) == 1) {
            episode.episode  = _episode + 1;
        }
        else if (sscanf(str, "%u..",
                        &_series) == 1) {
            episode.series   = _series  + 1;
        }
        else {
            ADVBConfig::Get().logit("Unrecognized episode structure '%s'", str.str());
            episode.valid = false;
        }
    }

    return episode;
}

AString ADVBProg::GetEpisodeString(const EPISODE& ep)
{
    AString res;

    if (ep.valid) {
        AString efmt, tfmt;
        uint_t n = 2;

        if (ep.episodes >= 100)  n = 3;
        if (ep.episodes >= 1000) n = 4;

        efmt.printf("E%%0%uu", n);
        tfmt.printf("T%%0%uu", n);
        if (ep.series)   res.printf("S%02u", ep.series);
        if (ep.episode)  res.printf(efmt.str(), ep.episode);
        if (ep.episodes) res.printf(tfmt.str(), ep.episodes);
    }

    return res;
}

AString ADVBProg::GetEpisodeString() const
{
    return GetEpisodeString(GetEpisode());
}

AString ADVBProg::GetShortEpisodeID() const
{
    AString _epid = GetEpisodeID();
    return ((_epid.Left(2) == "EP") && !GetEpisode().valid) ? _epid.Left(2) + _epid.Right(4) : "";
}

int ADVBProg::CompareEpisode(const EPISODE& ep1, const EPISODE& ep2)
{
    if (ep1.valid && ep2.valid) {
        if (ep1.series && ep2.series) {
            if (ep1.series < ep2.series) return -1;
            if (ep1.series > ep2.series) return  1;
        }
        if (ep1.episode && ep2.episode) {
            if (ep1.episode < ep2.episode) return -1;
            if (ep1.episode > ep2.episode) return  1;
        }
    }
    return 0;
}

AString ADVBProg::GetDescription(uint_t verbosity) const
{
    ADateTime start = GetStartDT().UTCToLocal();
    ADateTime stop  = GetStopDT().UTCToLocal();
    EPISODE ep;
    AString str;
    const char *p;

    str.printf("%s - %s : %s : %s",
               start.DateFormat(dayformat + dateformat + timeformat).str(),
               stop.DateFormat(timeformat).str(),
               GetChannel(),
               GetTitle());

    if ((p = GetSubtitle())[0]) {
        str.printf(" / %s", p);
    }

    if (verbosity) {
        ep = GetEpisode();
        if (ep.valid) {
            str.printf(" (%s)", GetEpisodeString(ep).str());
        }
        if ((p = GetString(data->strings.episodeid))[0]) {
            str.printf(" (%s)", p);
        }
        if (data->assignedepisode) {
            str.printf(" (F%u)", data->assignedepisode);
        }

        if (IsRepeat()) str.printf(" (R)");
        if (IsPlus1())  str.printf(" (+1)");
    }

    if (verbosity > 2) {
        str.printf(" (DVB channel '%s', channel ID '%s', base channel '%s')", GetDVBChannel(), GetChannelID(), GetBaseChannel());
    }

    if (IsRecording()) str.printf(" -*RECORDING*-");

    if (verbosity > 1) {
        AString str1;

        ep = GetEpisode();

        str.printf("\n\n%s\n", GetDesc());

        if ((verbosity > 2) && IsRepeat()) {
            if (str1.Valid()) str1.printf(" ");
            str1.printf("Repeat.");
        }

        if ((verbosity > 2) && (p = GetCategory())[0]) {
            if (str1.Valid()) str1.printf(" ");
            str1.printf("%s", p);

            if ((p = GetSubCategory())[0]) {
                AString subcategory = p;
                uint_t i, n = subcategory.CountLines();

                str1.printf(" (");
                for (i = 0; i < n; i++) {
                    if (i) str1.printf(", ");
                    str1.printf("%s", subcategory.Line(i).str());
                }
                str1.printf(")");
            }

            str1.printf(".");
        }

        if ((verbosity > 2) && ep.valid) {
            AString epstr;

            if (ep.series) epstr.printf("Series %u", ep.series);
            if (ep.episode) {
                if (epstr.Valid()) epstr.printf(", episode %u", ep.episode);
                else               epstr.printf("Episode %u", ep.episode);
                if (ep.episodes)   epstr.printf(" of %u", ep.episodes);
            }

            if ((p = GetEpisodeID())[0]) {
                if (epstr.Valid()) epstr.printf(" ");
                epstr.printf("(%s)", p);
            }

            if (epstr.Valid()) {
                if (str1.Valid()) str1.printf(" ");
                str1.printf("%s.", epstr.str());
            }
        }
        else if ((verbosity > 3) && data->assignedepisode) {
            if (str1.Valid()) str1.printf(" ");
            str1.printf("Assigned episode %u.", data->assignedepisode);
        }

        if (verbosity > 4) {
            AString actors = AString(GetActors()).SearchAndReplace("\n", ", ");

            if (actors.EndsWith(", ")) actors = actors.Left(actors.len() - 2);

            if (actors.Valid()) {
                if (str1.Valid()) str1.printf(" ");
                str1.printf("Starring: %s.", actors.str());
            }
        }

        if ((verbosity > 3) && GetPattern()[0]) {
            if (str1.Valid()) str1.printf("\n\n");
            str1.printf("Found with pattern '%s', pri %d (score %d)", GetPattern(), (int)data->pri, (int)data->score);

            if ((verbosity > 4) && GetString(data->strings.tags)[0]) {
                AString _tags = GetString(data->strings.tags);
                uint_t i, n = _tags.CountLines("||", 0);

                str1.printf(", tags: ");

                for (i = 0; i < n; i++) {
                    AString tag = _tags.Line(i, "||");
                    if (tag.FirstChar() == '|') tag = tag.Mid(1);
                    if (tag.LastChar()  == '|') tag = tag.Left(tag.len() - 1);
                    if (i) str1.printf(",");
                    str1.printf("%s", tag.str());
                }
            }

            if (GetUser()[0]) str1.printf(", user '%s'", GetUser());
            if (data->jobid) str1.printf(" (job %u, card %u)", data->jobid, (uint_t)data->dvbcard);
            if (IsRejected()) str1.printf(" ** REJECTED **");
        }

        if ((verbosity > 3) && data->recstart) {
            if (str1.Valid()) str1.printf("\n\n");
            str1.printf("Set to record %s - %s (%um %us)",
                        GetRecordStartDT().UTCToLocal().DateFormat(dayformat + dateformat + fulltimeformat).str(),
                        GetRecordStopDT().UTCToLocal().DateFormat(fulltimeformat).str(),
                        (uint_t)(GetRecordLength() / 60000), (uint_t)((GetRecordLength() % 60000) / 1000));

            if (GetUser()[0]) str1.printf(" by user '%s'", GetUser());

            if (data->actstart) {
                str1.printf(", actual record time %s - %s (%um %us)%s",
                            GetActualStartDT().UTCToLocal().DateFormat(dayformat + dateformat + fulltimeformat).str(),
                            GetActualStopDT().UTCToLocal().DateFormat(fulltimeformat).str(),
                            (uint_t)(GetActualLength() / 60000), (uint_t)((GetActualLength() % 60000) / 1000),
                            IsRecordingComplete() ? "" : " *not complete* ");
            }

            if (IgnoreRecording()) str1.printf(" (ignored when scheduling)");
        }

        if ((verbosity > 4) && GetFilename()[0]) {
            if (str1.Valid()) str1.printf("\n\n");
            str1.printf("%s as '%s'", data->actstart ? "Recorded" : (data->recstart ? "To be recorded" : "Would be recorded"), GetFilename());
            if (!IsConverted()) str1.printf(" (will be converted to '%s')", GenerateFilename(true).str());
            str1.printf(".");

            if (HasFailed()) {
                str1.printf(" ** ");
                if (HasRecordFailed())      str1.printf("Record");
                if (HasPostProcessFailed()) str1.printf("Post-process");
                if (HasConversionFailed())  str1.printf("Conversion");
                str1.printf(" failed ** ");
            }

            if (IsPostProcessing()) str1.printf(" Currently being post-processed.");

            bool exists = AStdFile::exists(GetFilename());
            str1.printf(" File %sexists",  exists ? "" : "does *not* ");
            if (GetFileSize()) str1.printf(" and %s %sMB in size", exists ? "is" : "was", AValue(GetFileSize() / ((uint64_t)1024 * (uint64_t)1024)).ToString().str());

            uint_t rate = GetRate() / 1024;
            if (rate) str1.printf(" (rate %ukbits/s)", rate);

            if (!IsRecordingComplete()) str.printf(" **INCOMPLETE**");

            str1.printf(".");
        }

        if (str1.Valid()) str.printf("\n%s\n", str1.str());

        str.printf("%s", AString("-").Copies(80).str());
    }

    return str;
}

AString ADVBProg::GetTitleAndSubtitle() const
{
    AString str = GetTitle();
    const char *p;

    if ((p = GetSubtitle())[0]) {
        str.printf(" / %s", p);
    }

    return str;
}

AString ADVBProg::GetTitleSubtitleAndChannel() const
{
    AString str = GetTitleAndSubtitle();

    str.printf(" (%s)", GetChannel());

    return str;
}

AString ADVBProg::GetQuickDescription() const
{
    AString str = GetTitleAndSubtitle();
    const char *p;
    EPISODE ep;

    ep = GetEpisode();
    if (ep.valid) {
        str.printf(" (%s)", GetEpisodeString(ep).str());
    }
    if ((p = GetString(data->strings.episodeid))[0]) {
        str.printf(" (%s)", p);
    }
    if (data->assignedepisode) {
        str.printf(" (F%u)", data->assignedepisode);
    }

    if (IsRepeat()) str.printf(" (R)");

    ADateTime start = GetStartDT().UTCToLocal();
    ADateTime stop  = GetStopDT().UTCToLocal();
    str.printf(" (%s - %s on %s)",
               start.DateFormat(dayformat + dateformat + timeformat).str(),
               stop.DateFormat(timeformat).str(),
               GetChannel());

    return str;
}

uint32_t ADVBProg::GetExternal(uint_t id) const
{
    uint32_t val = 0;

    switch (id) {
        case Compare_brate:
            val = GetRate();
            break;

        case Compare_kbrate:
            val = (GetRate() + 512U) / 1024U;
            break;
    }

    return val;
}

int ADVBProg::CompareExternal(uint_t id, uint32_t value) const
{
    uint32_t val = GetExternal(id);
    int res = 0;

    if (val < value) res = -1;
    if (val > value) res =  1;

    return res;
}

int ADVBProg::CompareExternal(uint_t id, sint32_t value) const
{
    sint32_t val = GetExternal(id);
    int res = 0;

    if (val < value) res = -1;
    if (val > value) res =  1;

    return res;
}

bool ADVBProg::SameProgramme(const ADVBProg& prog1, const ADVBProg& prog2)
{
    static const uint64_t hour = (uint64_t)3600 * (uint64_t)1000;
    bool same = false;

    if (CompareNoCase(prog1.GetTitle(), prog2.GetTitle()) == 0) {
        const char     *desc1     = prog1.GetDesc();
        const char     *desc2     = prog2.GetDesc();
        const char     *subtitle1 = prog1.GetSubtitle();
        const char     *subtitle2 = prog2.GetSubtitle();
        const EPISODE& ep1        = prog1.GetEpisode();
        const EPISODE& ep2        = prog2.GetEpisode();

        if ((prog1.IsPlus1() && (CompareNoCase(prog1.GetBaseChannel(), prog2.GetChannel()) == 0) && (prog1.GetStart() == (prog2.GetStart() + hour))) ||
            (prog2.IsPlus1() && (CompareNoCase(prog2.GetBaseChannel(), prog1.GetChannel()) == 0) && (prog2.GetStart() == (prog1.GetStart() + hour)))) {
            // either prog2 is on the +1 channel an hour later then prog1
            // or prog1 is on the +1 channel an hour later than prog2
            // -> definitely the same programme
#if DEBUG_SAMEPROGRAMME
            if (debugsameprogramme) debug("'%s' / '%s': plus1 repeat\n", prog1.GetDescription().str(), prog2.GetDescription().str());
#endif
            same = true;
        }
        else if (prog1.IsFilm() && prog2.IsFilm()) {
            // both programmes are films
            // -> same if (either director field is empty OR director fields match) AND
            //            (either year field is empty OR year fields match)
            same = ((!prog1.GetDirector()[0] || !prog2.GetDirector()[0] || (CompareNoCase(prog1.GetDirector(), prog2.GetDirector()) == 0)) &&
                    (!prog1.GetYear() || !prog2.GetYear() || (prog1.GetYear() == prog2.GetYear())));

#if DEBUG_SAMEPROGRAMME
            if (debugsameprogramme) debug("Films '%s' (directed by %s, year %u) / '%s' (directed by %s, year %u): %s\n",
                                          prog1.GetDescription().str(), prog1.GetDirector(), prog1.GetYear(),
                                          prog2.GetDescription().str(), prog2.GetDirector(), prog2.GetYear(),
                                          same ? "same" : "different");
#endif
        }
        else if (prog1.GetEpisodeID()[0] && prog2.GetEpisodeID()[0]) {
            // episodeid is in both -> sameness can be determined
            same = (CompareNoCase(prog1.GetEpisodeID(), prog2.GetEpisodeID()) == 0);
#if DEBUG_SAMEPROGRAMME
            if (debugsameprogramme) debug("'%s' / '%s': episodeid '%s' / '%s': %s\n", prog1.GetDescription().str(), prog2.GetDescription().str(), prog1.GetEpisodeID(), prog2.GetEpisodeID(), same ? "same" : "different");
#endif
        }
        else if (ep1.valid && ep2.valid) {
            same = ((ep1.series == ep2.series) && (ep1.episode == ep2.episode));
#if DEBUG_SAMEPROGRAMME
            if (debugsameprogramme) debug("'%s' / '%s': episode S%uE%02u / S%uE%02u: %s\n", prog1.GetDescription().str(), prog2.GetDescription().str(), (uint_t)ep1.series, (uint_t)ep1.episode, (uint_t)ep2.series, (uint_t)ep2.episode, same ? "same" : "different");
#endif
        }
        else if (prog1.GetEpisodeID()[0] || prog2.GetEpisodeID()[0]) {
            // episodeid is in one or both -> sameness can be determined (assume when episode ID is missing from a programme, it is different from one which has an episode ID)
            same = (CompareNoCase(prog1.GetEpisodeID(), prog2.GetEpisodeID()) == 0);
#if DEBUG_SAMEPROGRAMME
            if (debugsameprogramme) debug("'%s' / '%s': episodeid '%s' / '%s': %s\n", prog1.GetDescription().str(), prog2.GetDescription().str(), prog1.GetEpisodeID(), prog2.GetEpisodeID(), same ? "same" : "different");
#endif
        }
        else if (subtitle1[0] && subtitle2[0]) {
            // sub-title supplied for both -> sameness can be determined
            same = (CompareNoCase(subtitle1, subtitle2) == 0);
#if DEBUG_SAMEPROGRAMME
            if (debugsameprogramme) debug("'%s' / '%s': both subtitles valid: %s\n", prog1.GetDescription().str(), prog2.GetDescription().str(), same ? "same" : "different");
#endif
        }
        else if (subtitle1[0] || subtitle2[0]) {
            // one programme has subtitle -> must be different (since not been caught by the above)
            same = false;
#if DEBUG_SAMEPROGRAMME
            if (debugsameprogramme) debug("'%s' / '%s': only one subtitle valid: different\n", prog1.GetDescription().str(), prog2.GetDescription().str());
#endif
        }
        else if (prog1.GetAssignedEpisode() && prog2.GetAssignedEpisode()) {
            // assigned episode information valid in both
            // -> use episode to determine whether it's the same programme
            same = ((prog1.GetAssignedEpisode() == prog2.GetAssignedEpisode()) &&
                    (CompareNoCase(prog1.GetBaseChannel(), prog2.GetBaseChannel()) == 0));
#if DEBUG_SAMEPROGRAMME
            if (debugsameprogramme) {
                debug("'%s' / '%s': assigned episode %u / %u (%s  / %s): %s\n",
                      prog1.GetDescription().str(), prog2.GetDescription().str(),
                      prog1.GetAssignedEpisode(), prog2.GetAssignedEpisode(),
                      prog1.GetBaseChannel(), prog2.GetBaseChannel(),
                      same ? "same" : "different");
            }
#endif
        }
        else if (prog1.UseDescription() || prog2.UseDescription()) {
            // 'usedesc' specified
            // -> use descriptions to determine sameness
            same = (CompareNoCase(desc1, desc2) == 0);
#if DEBUG_SAMEPROGRAMME
            if (debugsameprogramme) debug("'%s' / '%s': descriptions %s\n", prog1.GetDescription().str(), prog2.GetDescription().str(), same ? "same" : "different");
#endif
        }
        else if (!prog1.IsRepeatOrPlus1() && prog2.IsRepeatOrPlus1() && (prog2.GetStart() < prog1.GetStart())) {
            // prog2 is a repeat but before prog1
            // -> definitely different programmes
            same = false;
#if DEBUG_SAMEPROGRAMME
            if (debugsameprogramme) debug("'%s' / '%s': prog2 repeat before prog1\n", prog1.GetDescription().str(), prog2.GetDescription().str());
#endif
        }
        else if (!prog2.IsRepeatOrPlus1() && prog1.IsRepeatOrPlus1() && (prog1.GetStart() < prog2.GetStart())) {
            // prog1 is a repeat but before prog2
            // -> definitely different programmes
            same = false;
#if DEBUG_SAMEPROGRAMME
            if (debugsameprogramme) debug("'%s' / '%s': prog1 repeat before prog2\n", prog1.GetDescription().str(), prog2.GetDescription().str());
#endif
        }
        else if (ep1.valid && !ep2.valid) {
            // prog1 episode valid but prog2 episode not valid
            // -> different programmes
            same = false;
#if DEBUG_SAMEPROGRAMME
            if (debugsameprogramme) debug("'%s' / '%s': prog1 episode valid, prog2 episode invalid\n", prog1.GetDescription().str(), prog2.GetDescription().str());
#endif
        }
        else if (ep2.valid && !ep1.valid) {
            // prog2 episode valid but prog1 episode not valid
            // -> different programmes
            same = false;
#if DEBUG_SAMEPROGRAMME
            if (debugsameprogramme) debug("'%s' / '%s': prog2 episode valid, prog1 episode invalid\n", prog1.GetDescription().str(), prog2.GetDescription().str());
#endif
        }
        else if (prog1.GetAssignedEpisode() && !prog2.GetAssignedEpisode()) {
            // prog1 assigned episode valid but prog2 assigned episode not valid
            // -> different programmes
            same = false;
#if DEBUG_SAMEPROGRAMME
            if (debugsameprogramme) debug("'%s' / '%s': prog1 assigned episode valid, prog2 assigned episode invalid\n", prog1.GetDescription().str(), prog2.GetDescription().str());
#endif
        }
        else if (prog2.GetAssignedEpisode() && !prog1.GetAssignedEpisode()) {
            // prog2 assigned episode valid but prog1 assigned episode not valid
            // -> different programmes
            same = false;
#if DEBUG_SAMEPROGRAMME
            if (debugsameprogramme) debug("'%s' / '%s': prog2 assigned episode valid, prog1 assigned episode invalid\n", prog1.GetDescription().str(), prog2.GetDescription().str());
#endif
        }
        else {
            // otherwise they are the same programme
            same = true;
#if DEBUG_SAMEPROGRAMME
            if (debugsameprogramme) debug("'%s' / '%s': same\n", prog1.GetDescription().str(), prog2.GetDescription().str());
#endif
        }
    }

    return same;
}

AString ADVBProg::GetPrefItem(const AString& name, const AString& defval) const
{
    AString prefs = GetPrefs();
    AString res;

    if (FieldExists(prefs, name)) {
        res = GetField(prefs, name);
    }
    else res = ADVBConfig::Get().GetConfigItem(name, defval);

    return res;
}

AString ADVBProg::GetAttributedConfigItem(const AString& name, const AString& defval, bool defvalid) const
{
    const ADVBConfig& config = ADVBConfig::Get();

    return config.GetHierarchicalConfigItem(GetUser(), name, GetUUID(),
                                            config.GetHierarchicalConfigItem(GetUser(), name, GetTitle(), defval, defvalid));
}

void ADVBProg::SearchAndReplace(const AString& search, const AString& replace)
{
    const uint16_t *strs = &data->strings.channel;
    uint_t i;

    for (i = 0; i < (sizeof(data->strings) / sizeof(strs[0])); i++) {
        SetString(strs + i, AString(GetString(strs[i])).SearchAndReplace(search, replace));
    }
}

AString ADVBProg::ReplaceTerms(const AString& str, bool filesystem) const
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString date  = GetStartDT().UTCToLocal().DateFormat("%Y-%M-%D");
    AString times = GetStartDT().UTCToLocal().DateFormat("%h%m") + "-" + GetStopDT().UTCToLocal().DateFormat("%h%m");
    AString res   = (config.ReplaceTerms(GetUser(), str, "")
                     .SearchAndReplace("{title}", SanitizeString(GetTitle(), filesystem))
                     .SearchAndReplace("{titleandsubtitle}", SanitizeString(GetTitleAndSubtitle(), filesystem))
                     .SearchAndReplace("{titlesubtitleandchannel}", SanitizeString(GetTitleSubtitleAndChannel(), filesystem))
                     .SearchAndReplace("{subtitle}", SanitizeString(GetSubtitle(), filesystem))
                     .SearchAndReplace("{year}", SanitizeString(GetYear() ? AString("%").Arg(GetYear()) : "", filesystem, true))
                     .SearchAndReplace("{episode}", SanitizeString(GetEpisodeString(), filesystem))
                     .SearchAndReplace("{episodeid}", SanitizeString(GetShortEpisodeID(), filesystem))
                     .SearchAndReplace("{channel}", SanitizeString(GetChannel(), filesystem))
                     .SearchAndReplace("{category}", SanitizeString(GetCategory(), filesystem))
                     .SearchAndReplace("{date}", SanitizeString(date, filesystem))
                     .SearchAndReplace("{times}", SanitizeString(times, filesystem))
                     .SearchAndReplace("{user}", SanitizeString(GetUser(), filesystem))
                     .SearchAndReplace("{capitaluser}", SanitizeString(AString(GetUser()).InitialCapitalCase(), filesystem))
                     .SearchAndReplace("{userdir}", SanitizeString(AString(GetUser()).InitialCapitalCase(), filesystem, true))
                     .SearchAndReplace("{titledir}", SanitizeString(GetTitle(), filesystem, true))
                     .SearchAndReplace("{filename}", SanitizeString(GetFilename(), filesystem, true))
                     .SearchAndReplace("{logfile}", SanitizeString(GetLogFile(), filesystem, true))
                     .SearchAndReplace("{link}", SanitizeString(GetLinkToFile(), filesystem, true))
                     .SearchAndReplace("//", "/"));

    while (res.Pos("{sep}{sep}") >= 0) {
        res = res.SearchAndReplace("{sep}{sep}", "{sep}");
    }

    res = res.SearchAndReplace("{sep}", ".");

    return res;
}

bool ADVBProg::Match(const ADataList& patternlist) const
{
    uint_t i;
    bool match = false;

    for (i = 0; (i < patternlist.Count()) && !HasQuit() && !match; i++) {
        const PATTERN& pattern = *(const PATTERN *)patternlist[i];

        if (pattern.enabled) {
            match |= Match(pattern);
        }
    }

    return match;
}

AString ADVBProg::ReplaceFilenameTerms(const AString& str, bool converted) const
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString suffix = converted ? config.GetConvertedFileSuffix(GetUser()) : recordedfilesuffix;

    return (ReplaceTerms(str, true)
            .SearchAndReplace("{suffix}", SanitizeString(suffix, true)));
}

AString ADVBProg::GetRecordingsSubDir() const
{
    const ADVBConfig& config = ADVBConfig::Get();
    return config.GetRecordingsSubDir(GetUser(), GetModifiedCategory());
}

AString ADVBProg::GenerateFilename(bool converted) const
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString templ = config.GetFilenameTemplate(GetUser(), GetTitle(), GetModifiedCategory());
    AString dir;

    if (converted) {
        AString subdir = GetDir();

        if (subdir.Empty()) {
            subdir = GetRecordingsSubDir();
        }

        dir = CatPath(config.GetRecordingsDir(), subdir);
    }
    else dir = config.GetRecordingsStorageDir();

    AString src = dir.CatPath(templ);
    return ReplaceFilenameTerms(src, converted);
}

AString ADVBProg::GetConvertedDestinationDirectory() const
{
    return GenerateFilename(true).PathPart();
}

static bool __fileexists(const FILE_INFO *file, void *Context)
{
    (void)file;
    (void)Context;
    return false;
}

bool ADVBProg::FilePatternExists(const AString& filename)
{
    AString pattern = filename.Prefix() + ".*";
    return !::Recurse(pattern, 0, &__fileexists);
}

void ADVBProg::GenerateRecordData(uint64_t recstarttime)
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString filename;

    if (!data->recstart) {
        data->recstart = GetStart() - (uint64_t)data->prehandle  * 60000;
        data->recstart = MAX(data->recstart, recstarttime);
    }
    if (!data->recstop) data->recstop = GetStop() + (uint64_t)data->posthandle * 60000;
    if (!GetDVBChannel()[0]) {
        config.logit("Warning: '%s' has no DVB channel, using main channel!\n", GetQuickDescription().str());
    }

    filename = GenerateFilename();

    if (FilePatternExists(filename)) {
        AString filename1;
        uint_t  n = 1;

        do {
            filename1 = filename.Prefix() + AString(".%;.").Arg(n++) + filename.Suffix();
        }
        while (FilePatternExists(filename1));

        filename = filename1;
    }

    SetString(&data->strings.filename, filename.str());
}

bool ADVBProg::WriteToJobQueue()
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString scriptname, scriptres, cmd;
    AStdFile fp;
    int  queue = config.GetQueue().FirstChar();
    bool success = false;

    scriptname = config.GetTempFile("dvbscript", ".sh");
    scriptres  = scriptname + ".res";

    if (fp.open(scriptname, "w")) {
        ADateTime et = ADateTime().TimeStamp(true);
        ADateTime dt = data->recstart;

        // move on at least 60s and then onto next minute
        et += ADateTime(0, (uint32_t)(119 - et.GetSeconds()) * 1000UL);
        // round down to nearest minute
        et -= ADateTime(0, (uint32_t)et.GetSeconds() * 1000UL);

        dt = MAX(dt, et);

        fp.printf("DVBPROG=\"%s\"\n", Base64Encode().str());
        fp.printf("\n");
        fp.printf("dvb --record $DVBPROG");
        fp.printf("\n");

        AString str;
        str.printf("------------------------------------------------------------------------------------------------------------------------\n");
        str.printf("%s", GetDescription(~0).str());

        uint_t i, n;
        if ((n = str.CountLines("\n", 0)) > 0) {
            str.CutLine(n - 1, "\n", 0);
        }

        str.printf("\nDVB card %u (hardware card %u), priority %d, Recording %s - %s\n",
                   GetDVBCard(),
                   config.GetPhysicalDVBCard(GetDVBCard()),
                   GetPri(),
                   GetRecordStartDT().UTCToLocal().DateFormat(dayformat + dateformat + fulltimeformat).str(),
                   GetRecordStopDT().UTCToLocal().DateFormat(fulltimeformat).str());

        str.printf("\nUser '%s', pattern '%s'\n",
                   GetUser(), GetPattern());

        str.printf("------------------------------------------------------------------------------------------------------------------------\n");

        n = str.CountLines("\n", 0);
        fp.printf("\n");
        for (i = 0; i < n; i++) {
            fp.printf("# %s\n", str.Line(i, "\n", 0).str());
        }
        fp.close();

        // Uses LOCAL time for 'at' since there seems to be no UTC equivalent!!
        // Change to UTC mechanism as soon as possible!!
        cmd.Format("at -q %c -f %s %s >%s 2>&1", queue, scriptname.str(), dt.UTCToLocal().DateFormat("%h:%m %D %N").str(), scriptres.str());

        int res;
        if ((res = system(cmd)) == 0) {
            if (fp.open(scriptres)) {
                AString line;

                while (line.ReadLn(fp) >= 0) {
                    uint_t job;

                    if (sscanf(line, "job %u at ", &job) > 0) {
                        data->jobid = job;
                        success = true;
                        break;
                    }
                }

                fp.close();

                if (!success) config.logit("Scheduled '%s' okay but no job information in res file", GetDescription().str());
            }
            else config.logit("Scheduled '%s' okay but no res file", GetDescription().str());
        }
        else config.logit("Failed to schedule '%s', error %d", GetDescription().str(), res);
    }
    else config.logit("Failed to open script file '%s' for writing (%s)", scriptname.str(), strerror(errno));

    remove(scriptname);
    remove(scriptres);

    return success;
}

bool ADVBProg::ReadFromJob(const AString& filename)
{
    AStdFile fp;
    bool success = false;

    if (fp.open(filename)) {
        AString line;

        while (line.ReadLn(fp) >= 0) {
            if (line.Left(8) == "DVBPROG=") {
                AString str = line.Mid(8).DeQuotify();

                if (Base64Decode(str)) {
                    success = true;
                }

                break;
            }
        }

        fp.close();
    }

    return success;
}

void ADVBProg::AddToList(PROGLIST *list)
{
    this->list = list;
    list->push_back(this);
}

void ADVBProg::RemoveFromList()
{
    PROGLIST::iterator it;
    if (list && ((it = std::find(list->begin(), list->end(), this)) != list->end())) list->erase(it);
}

int ADVBProg::CompareProgrammesByTime(uptr_t item1, uptr_t item2, void *context)
{
    const ADVBProg *prog1 = (const ADVBProg *)item1;
    const ADVBProg *prog2 = (const ADVBProg *)item2;

    return Compare(prog1, prog2, (const bool *)context);
}

void ADVBProg::SetPriorityScore(const ADateTime& starttime)
{
    priority_score = ((double)GetAttributedConfigItem(priorityscalename, "2.0") * (double)data->pri +
                      (double)GetAttributedConfigItem(overlapscalename,  "-.2") * (double)overlaps);

    if (IsUrgent()) priority_score += (double)GetAttributedConfigItem(urgentscalename, "3.0");

    if (list) priority_score += (double)GetAttributedConfigItem(repeatsscalename, "-1.0") * (double)(list->size() - 1);

    priority_score += (double)GetAttributedConfigItem(delayscalename, "-.5") * (double)(GetStartDT().GetDays() - starttime.GetDays());

    if (GetRecordStart() > GetStart()) {
        // for each minute the recording is *late* starting, drop the priority by 2
        priority_score += (double)GetAttributedConfigItem(latescalename, "-2.0") * (double)(GetRecordStart() - GetStart()) / 60000.0;
    }
}

bool ADVBProg::BiasPriorityScore(const ADVBProg& prog)
{
    bool changed = false;

    // if the specified programme overlaps this programme *just* in terms
    // of pre- and post- handles, bias the priority score
    if (RecordOverlaps(prog) && !Overlaps(prog)) {
        priority_score += (double)GetAttributedConfigItem(recordoverlapscalename, "-2.0");
        changed = true;
    }

    return changed;
}

bool ADVBProg::CountOverlaps(const ADVBProgList& proglist)
{
    uint_t i, newoverlaps = 0;
    bool changed = false;

    for (i = 0; i < proglist.Count(); i++) {
        const ADVBProg& prog = proglist.GetProg(i);

        if ((prog != *this) && !SameProgramme(prog, *this) && RecordOverlaps(prog)) {
            newoverlaps++;
        }
    }

    // detect when number of overlaps changes
    if (newoverlaps != overlaps) {
#if 0
        if (overlaps) {
            const ADVBConfig& config = ADVBConfig::Get();
            config.logit("'%s' changed from %u overlaps to %u", GetQuickDescription().str(), overlaps, newoverlaps);
        }
#endif
        overlaps = newoverlaps;
        changed  = true;
    }

    return changed;
}

int ADVBProg::SortListByOverlaps(uptr_t item1, uptr_t item2, void *context)
{
    const ADVBProg& prog1      = *(const ADVBProg *)item1;
    const ADVBProg& prog2      = *(const ADVBProg *)item2;
    bool            prog1urg   = prog1.IsUrgent();
    bool            prog2urg   = prog2.IsUrgent();
    int res = 0;

    UNUSED(context);

    if      ( prog1urg && !prog2urg)                                  res = -1;
    else if (!prog1urg &&  prog2urg)                                  res =  1;
    else if (!prog1urg && (prog1.overlaps < prog2.overlaps))          res = -1;
    else if (!prog1urg && (prog1.overlaps > prog2.overlaps))          res =  1;
    else if (prog1.GetStart() < prog2.GetStart())                     res = -1;
    else if (prog1.GetStart() > prog2.GetStart())                     res =  1;
    else if (prog1urg && (prog1.overlaps  < prog2.overlaps))          res = -1;
    else if (prog1urg && (prog1.overlaps  > prog2.overlaps))          res =  1;
    else res = CompareNoCase(prog1.GetQuickDescription(), prog2.GetQuickDescription());

    return res;
}

int ADVBProg::CompareScore(const ADVBProg& prog1, const ADVBProg& prog2)
{
    int res = 0;

    if      (prog1.priority_score > prog2.priority_score) res = -1;
    else if (prog1.priority_score < prog2.priority_score) res =  1;
    else if (prog1.GetStart()     < prog2.GetStart())     res = -1;
    else if (prog1.GetStart()     > prog2.GetStart())     res =  1;
    else res = CompareNoCase(prog1.GetQuickDescription(), prog2.GetQuickDescription());

    return res;
}

int ADVBProg::SortListByScore(uptr_t item1, uptr_t item2, void *context)
{
    UNUSED(context);
    return CompareScore(*(const ADVBProg *)item1, *(const ADVBProg *)item2);
}

bool ADVBProg::GetRecordPIDs(AString& pids, bool update) const
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADVBChannelList& channellist = ADVBChannelList::Get();
    AString channel = GetDVBChannel();

    if (channel.Empty()) channel = GetChannel();

    return channellist.GetPIDList(config.GetPhysicalDVBCard(GetDVBCard()), channel, pids, update);
}

AString ADVBProg::GenerateStreamCommand(uint_t card, uint_t nsecs, const AString& pids, const AString& logfile)
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString cmd;

    cmd.printf("nice -10 %s -c %u -n %u -f %s -o 2>>\"%s\"",
               config.GetDVBStreamCommand().str(),
               config.GetPhysicalDVBCard(card),
               nsecs,
               pids.str(),
               logfile.str());

    return cmd;
}

AString ADVBProg::GenerateRecordCommand(uint_t nsecs, const AString& pids) const
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString cmd;

    cmd.printf("%s >\"%s\"",
               GenerateStreamCommand(GetDVBCard(),
                                     nsecs,
                                     pids,
                                     config.GetLogFile().str()).str(),
               GetTempFilename().str());

    return cmd;
}

bool ADVBProg::UpdateFileSize()
{
    const ADVBConfig& config = ADVBConfig::Get();
    FILE_INFO info;
    AString   filename = GetFilename();
    bool      valid    = true;

    if (::GetFileInfo(filename, &info)) {
        uint64_t oldfilesize = GetFileSize();
        uint32_t rate        = 0;

        if (GetActualLengthFallback() > 0) {
            uint64_t divisor = (uint64_t)1024U * GetActualLengthFallback();

            rate = (uint32_t)(((uint64_t)1000U * info.FileSize + divisor / 2U) / divisor);
        }

        config.printf("File '%s' exists and is %sMB, %s seconds = %skB/s%s",
                      GetFilename(),
                      AValue(info.FileSize / ((uint64_t)1024U * (uint64_t)1024U)).ToString().str(),
                      AValue((GetActualLengthFallback() + 999U) / 1000U).ToString().str(),
                      rate ? AValue(rate).ToString().str() : "<unknown>",
                      oldfilesize ? AString(", file size ratio = %0.3;").Arg((double)info.FileSize / (double)oldfilesize).str() : "");

        SetFileSize(info.FileSize);

        valid = (rate >= config.GetMinimalDataRate(filename.Suffix()));

        if (!valid) config.printf("File considered to be invalid as rate it too small!");
    }
    else {
        config.printf("File '%s' doesn't exist!", filename.str());
        SetFileSize(0);
    }

    return valid;
}

bool ADVBProg::UpdateRecordedList()
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADVBLock     lock("dvbfiles");
    ADVBProgList reclist;
    bool         nochange = false;

    if (reclist.ReadFromFile(config.GetRecordedFile())) {
        ADVBProg *prog;

        if ((prog = reclist.FindUUIDWritable(*this)) != NULL) {
            if (Base64Encode() != prog->Base64Encode()) prog->Modify(*this);
            else nochange = true;
        }
        else reclist.AddProg(*this);
    }

    return (nochange || reclist.WriteToFile(config.GetRecordedFile()));
}

void ADVBProg::Record()
{
    if (Valid()) {
        const ADVBConfig& config = ADVBConfig::Get();
        uint_t   nsecs;
        bool     record = true;

        SetRunning();

        config.printf("------------------------------------------------------------------------------------------------------------------------");
        config.printf("Starting record of '%s' as '%s'", GetQuickDescription().str(), GetFilename());

        AString pids;
        AString user       = GetUser();
        bool    reschedule = false;
        bool    failed     = false;

        if (record) {
            uint64_t now    = (uint64_t)ADateTime().TimeStamp(true);
            // only update pids if there is at least 30s before programme starts
            bool     update = (SUBZ(GetStart(), now) >= 30000);

            GetRecordPIDs(pids, update);

            if (!update && (pids.CountWords() < 2)) {
                config.printf("No pids for '%s' without update", GetQuickDescription().str());
                GetRecordPIDs(pids, true);
            }

            // write updated channels to file, if changed
            ADVBChannelList::Get().Write();

            if (pids.CountWords() < 2) {
                config.printf("No pids for '%s'", GetQuickDescription().str());
                failed = true;
                record = false;
            }
        }

        if (!GetJobID()) {
            ADVBLock     lock("dvbfiles");
            ADVBProgList list;
            const ADVBProg *prog;

            list.ReadFromFile(config.GetScheduledFile());

            // copy over job ID from scheduled list
            if ((prog = list.FindUUID(*this)) != NULL) {
                SetJobID(prog->GetJobID());
                // delete programme
                list.DeleteProg(*prog);
                // write back
                list.WriteToFile(config.GetScheduledFile());
            }
            else {
                config.printf("Failed to find %s in scheduled list, cannot copy job ID!", GetTitleAndSubtitle().str());
            }
        }

        if (record) {
            uint64_t dt, st, et;
            uint_t   nmins;

            // get current time
            dt    = (uint64_t)ADateTime().TimeStamp(true);

            // work out how many minutes (if any) the programme has been running
            st    = GetStart();
            nmins = (dt >= st) ? (uint_t)((dt - st) / 60000) : 0;

            // work out how many seconds till the recording should stop
            et    = GetRecordStop();
            nsecs = (et >= dt) ? (uint_t)((et - dt + 1000 - 1) / 1000) : 0;

            // if programme has been running too long, abort recording
            if (!IgnoreLateStart() && (nmins >= config.GetLatestStart())) {
                config.printf("'%s' started too long ago (%u minutes)!", GetTitleAndSubtitle().str(), nmins);
                reschedule = true;
                record = false;
                failed = true;
            }
        }

        if (record) {
            uint64_t dt, st, et;

            // get current time
            dt    = (uint64_t)ADateTime().TimeStamp(true);

            // work out how many ms until recording should start
            st    = GetRecordStart();

            // if there is still some time before the recording should start, delay now
            if (dt < st) {
                uint_t delay = (uint_t)(st - dt);

                config.logit("'%s' recording starts in %ums, sleeping...", GetTitleAndSubtitle().str(), delay);
                Sleep(delay);
            }

            // get [updated] current time
            dt    = (uint64_t)ADateTime().TimeStamp(true);

            // work out how many seconds till the recording should stop
            et    = GetRecordStop();
            nsecs = (et > dt) ? (uint_t)((et - dt + 1000 - 1) / 1000) : 0;

            // if programme has already finished, abort recording
            if (nsecs == 0) {
                config.printf("'%s' already finished!", GetTitleAndSubtitle().str());
                reschedule = true;
                record = false;
            }
        }

        if (record) {
            if (!AllowRepeats() && !RecordIfMissing()) {
                const ADVBProg *recordedprog = NULL;
                ADVBProgList recordedlist;

                recordedlist.ReadFromFile(config.GetRecordedFile());

                if (record && ((recordedprog = recordedlist.FindCompleteRecording(*this)) != NULL)) {
                    config.printf("'%s' already recorded ('%s')!", GetTitleAndSubtitle().str(), recordedprog->GetQuickDescription().str());
                    record = false;
                }
            }
        }

        if (record) {
            AString filename = GetFilename();
            AString cmd;
            int     res;

            if (!CreateDirectory(GetTempFilename().PathPart())) {
                if (dircreationerrors.Valid()) dircreationerrors.printf("\n");
                dircreationerrors.printf("Failed to create directory '%s' for temporary file", GetTempFilename().PathPart().str());
            }
            if (!CreateDirectory(filename.PathPart())) {
                if (dircreationerrors.Valid()) dircreationerrors.printf("\n");
                dircreationerrors.printf("Failed to create directory '%s' for final file", filename.PathPart().str());
            }

            config.printf("Recording '%s' for %u seconds (%u minutes) to '%s' using freq %uHz PIDs %s",
                          GetTitleAndSubtitle().str(),
                          nsecs, ((nsecs + 59) / 60),
                          filename.str(),
                          (uint_t)pids.Word(0),
                          pids.Words(1).str());

            cmd = GenerateRecordCommand(nsecs, pids);

            config.printf("Running '%s'", cmd.str());

            SetRecording();
            ADVBProgList::AddToList(config.GetRecordingFile(), *this);

            TriggerServerCommand(config.GetServerUpdateRecordingsCommand());

            config.printf("--------------------------------------------------------------------------------");
            data->actstart = ADateTime().TimeStamp(true);
            config.writetorecordlog("start %s", Base64Encode().str());
            res = system(cmd);
            data->actstop = ADateTime().TimeStamp(true);
            config.writetorecordlog("stop %s", Base64Encode().str());
            config.printf("--------------------------------------------------------------------------------");

            ADVBProgList::RemoveFromList(config.GetRecordingFile(), *this);
            ClearRecording();

            TriggerServerCommand(config.GetServerUpdateRecordingsCommand());

            if (res == 0) {
                AString  str;
                uint64_t dt = GetActualStop();
                uint64_t st = GetStop();

                if (rename(GetTempFilename(), GetFilename()) == 0) {
                    SetRecordingComplete();

                    if (dt < (st - 15000)) {
                        config.printf("Warning: '%s' stopped %ss before programme end!",
                                      GetTitleAndSubtitle().str(),
                                      AValue((st - dt) / 1000).ToString().str());

                        // force reschedule
                        failed = true;
                    }
                    else if (!UpdateFileSize()) {
                        config.printf("Record of '%s' ('%s') is not valid or doesn't exist", GetTitleAndSubtitle().str(), GetFilename());

                        // force reschedule
                        failed = true;
                    }
                    else {
                        config.printf("Adding '%s' to list of recorded programmes", GetTitleAndSubtitle().str());

                        ClearScheduled();
                        SetRecorded();

                        if (!IsRecordingComplete()) {
                            config.printf("Warning: '%s' is incomplete! (%ss missing from the start, %ss missing from the end)",
                                          GetTitleAndSubtitle().str(),
                                          AValue(MAX((sint64_t)(data->actstart - MIN(data->start, data->recstart)), 0) / 1000).ToString().str(),
                                          AValue(MAX((sint64_t)(data->recstop  - data->actstop), 0) / 1000).ToString().str());

                            // force reschedule
                            reschedule = true;
                        }

                        {
                            FlagsSaver saver(this);
                            ClearRunning();
                            UpdateRecordedList();
                        }

                        if (IsOnceOnly() && IsRecordingComplete() && !config.IsRecordingSlave()) {
                            ADVBPatterns::DeletePattern(user, GetPattern());

                            reschedule |= config.RescheduleAfterDeletingPattern(GetUser(), GetModifiedCategory());
                        }

                        bool success = true;
                        if (!PostRecord()) {
                            SetRecordFailed();
                            success = false;
                        }
                        if (success && !PostProcess()) {
                            SetPostProcessFailed();
                            success = false;
                        }
                        if (success && !ConvertVideoEx()) {
                            SetConversionFailed();
                            success = false;
                        }
                        if (success) OnRecordSuccess();
                        else         failed  = true;
                    }
                }
                else {
                    remove(GetTempFilename());
                    config.printf("Unable to rename '%s' to '%s'", GetTempFilename().str(), GetFilename());
                    SetRecordFailed();
                    failed = true;
                }
            }
            else {
                remove(GetTempFilename());
                config.printf("Unable to start record of '%s'", GetTitleAndSubtitle().str());
                SetRecordFailed();
                failed = true;
            }
        }
        else config.printf("NOT recording '%s'", GetTitleAndSubtitle().str());

        if (failed) {
            OnRecordFailure();
            reschedule = true;

            ClearScheduled();

            {
                FlagsSaver saver(this);
                ClearRunning();
                ADVBProgList::AddToList(config.GetRecordFailuresFile(), *this);
            }
        }

        if (reschedule) {
            config.printf("Rescheduling");

            ADVBProgList::SchedulePatterns(ADateTime().TimeStamp(true), true);
        }
        else ADVBProgList::CheckDiskSpace(true);

        config.printf("------------------------------------------------------------------------------------------------------------------------");

        ClearRunning();

        ADVBConfig::ReportDirectoryCreationErrors(dircreationerrors);
        dircreationerrors.Delete();
    }
}

AString ADVBProg::GetTempFilename() const
{
    return AString(GetFilename()).Prefix() + "." + tempfilesuffix;
}

AString ADVBProg::GetLinkToFile() const
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString relpath = config.GetRelativePath(GetFilename());
    return relpath.Valid() ? config.GetServerURL().CatPath(relpath) : "";
}

bool ADVBProg::OnRecordSuccess() const
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString user = GetUser();
    AString cmd, success = true;

    if ((cmd = config.GetUserConfigItem(user, "recordsuccesscmd")).Valid()) {
        cmd = ReplaceTerms(cmd);
        cmd.printf(" 2>&1 >>\"%s\"", config.GetLogFile().str());

        success = RunCommand(cmd);
    }

    if (IsNotifySet() && (cmd = config.GetUserConfigItem(user, "notifycmd")).Valid()) {
        cmd = ReplaceTerms(cmd);
        cmd.printf(" 2>&1 >>\"%s\"", config.GetLogFile().str());

        success = RunCommand(cmd);
    }

    TriggerServerCommand(config.GetServerGetAndConvertCommand());

    return success;
}

bool ADVBProg::OnRecordFailure() const
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString user = GetUser();
    AString cmd, success = true;

    if ((cmd = config.GetUserConfigItem(user, "recordfailurecmd")).Valid()) {
        cmd = ReplaceTerms(cmd);
        cmd.printf(" 2>&1 >>\"%s\"", config.GetLogFile().str());

        success = RunCommand(cmd);
    }

    return success;
}

AString ADVBProg::GeneratePostRecordCommand() const
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString user = GetUser();
    AString postcmd;

    if ((uint_t)config.GetUserConfigItem(user, "postrecord") != 0) {
        if ((postcmd = config.GetUserConfigItem(user, "postrecordcmd")).Valid()) {
            postcmd = ReplaceTerms(postcmd);
            postcmd.printf(" 2>&1 >>\"%s\"", config.GetLogFile().str());
        }
    }

    return postcmd;
}

AString ADVBProg::GeneratePostProcessCommand() const
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString user = GetUser();
    AString postcmd;

    if (RunPostProcess() || ((uint_t)config.GetUserConfigItem(user, "postprocess") != 0)) {
        if ((postcmd = config.GetUserConfigItem(user, "postprocesscmd")).Valid()) {
            postcmd = ReplaceTerms(postcmd);
            postcmd.printf(" 2>&1 >>\"%s\"", config.GetLogFile().str());
        }
    }

    return postcmd;
}

AString ADVBProg::GetLogFile() const
{
    const ADVBConfig& config = ADVBConfig::Get();
    return config.GetLogDir().CatPath(AString(GetFilename()).FilePart().Prefix() + ".txt");
}

bool ADVBProg::RunCommand(const AString& cmd, bool logoutput) const
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString   scmd;
    AString   cmdlogfile = config.GetLogDir().CatPath(AString(GetFilename()).FilePart().Prefix() + "_cmd.txt");
    AString   logfile = GetLogFile();
    bool success = false;

    if (cmd.Pos(logfile) >= 0) {
        ADateTime starttime, stoptime;

        if      (GetActualStart()) starttime = GetActualStartDT().UTCToLocal();
        else if (GetRecordStart()) starttime = GetRecordStartDT().UTCToLocal();
        else if (GetStart())       starttime = GetStartDT().UTCToLocal();

        config.ExtractLogData(starttime, stoptime, logfile);
    }

    config.printf("Running command '%s'", cmd.str());

    scmd = cmd;
    if (logoutput) scmd.printf(" >\"%s\" 2>&1", cmdlogfile.str());

    success = (system(scmd) == 0);

    if (logoutput) {
        AStdFile fp;
        if (fp.open(cmdlogfile)) {
            AString line;
            while (line.ReadLn(fp) >= 0) config.logit("%s", line.str());
            fp.close();
        }

        remove(cmdlogfile);
    }

    if (!success) config.printf("Command '%s' failed!", cmd.str());

    remove(logfile);

    return success;
}

bool ADVBProg::PostRecord()
{
    AString postcmd;
    bool success = false;

    if ((postcmd = GeneratePostRecordCommand()).Valid()) {
        success = RunCommand(postcmd);
    }
    else success = true;

    return success;
}

bool ADVBProg::PostProcess()
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString postcmd;
    bool    postprocessed = false;
    bool    success = false;

    if ((postcmd = GeneratePostProcessCommand()).Valid()) {
        config.printf("Running post process command '%s'", postcmd.str());

        SetPostProcessing();
        ADVBProgList::AddToList(config.GetProcessingFile(), *this);

        success = postprocessed = RunCommand(postcmd);

        SetPostProcessed();
        UpdateFileSize();

        ADVBProgList::RemoveFromList(config.GetProcessingFile(), *this);
        ClearPostProcessing();
    }
    else success = true;

    return success;
}

uint64_t ADVBProg::CalcTime(const char *str)
{
    uint_t hrs = 0, mins = 0, secs = 0, ms = 0;
    uint64_t t = 0;

    if (sscanf(str, "%u:%u:%u:%u", &hrs, &mins, &secs, &ms) >= 4) {
        t = (uint64_t)(hrs * 3600 + mins * 60 + secs) * 1000UL + ms;
    }
    else if (sscanf(str, "%u:%u:%u.%u", &hrs, &mins, &secs, &ms) >= 4) {
        t = (uint64_t)(hrs * 3600 + mins * 60 + secs) * 1000UL + ms;
    }

    return t;
}

AString ADVBProg::GenTime(uint64_t t, const char *format)
{
    AString  res;
    uint32_t sec = (uint32_t)(t / 1000);
    uint_t   ms  = (uint_t)(t % 1000);

    res.printf(format,
               (uint_t)(sec / 3600),
               (uint_t)((sec / 60) % 60),
               (uint_t)(sec % 60),
               ms);

    return res;
}

AString ADVBProg::GetParentheses(const AString& line, int p)
{
    int p1, p2;

    if (((p1 = line.Pos("(", p)) >= 0) && ((p2 = line.Pos(")", p1)) >= 0)) {
        p1++;
        return line.Mid(p1, p2 - p1);
    }

    return "";
}

void ADVBProg::ConvertSubtitles(const AString& src, const AString& dst, const std::vector<SPLIT>& splits, const AString& aspect)
{
    const ADVBConfig& config = ADVBConfig::Get();
    FILE_INFO info;
    AString subsdir = dst.PathPart().CatPath("subs");

    if (config.ForceSubs(GetUser())) {
        if (!CreateDirectory(subsdir)) {
            if (dircreationerrors.Valid()) dircreationerrors.printf("\n");
            dircreationerrors.printf("Failed to create directory '%s' for subs", subsdir.str());
        }
    }

    if (GetFileInfo(subsdir, &info)) {
        AString srcsubfile = src.Prefix() + ".sup.idx";
        AString dstsubfile = dst.PathPart().CatPath("subs", dst.FilePart().Prefix() + ".sup.idx");
        AStdFile fp1, fp2;

        if (fp1.open(srcsubfile)) {
            if (!CreateDirectory(dstsubfile.PathPart())) {
                if (dircreationerrors.Valid()) dircreationerrors.printf("\n");
                dircreationerrors.printf("Failed to create directory '%s' for subs files", dstsubfile.PathPart().str());
            }

            if (fp2.open(dstsubfile, "w")) {
                AString line;

                config.printf("Converting '%s' to '%s'", srcsubfile.str(), dstsubfile.str());

                while (line.ReadLn(fp1) >= 0) {
                    if (line.Pos("timestamp:") == 0) {
                        uint64_t t = CalcTime(line.str() + 11);
                        uint64_t sub = 0;
                        uint_t i;

                        for (i = 0; i < splits.size(); i++) {
                            const SPLIT& split = splits[i];

                            if (split.aspect == aspect) {
                                if ((t >= split.start) && (!split.length || (t < (split.start + split.length)))) {
                                    t -= sub;

                                    fp2.printf("timestamp: %s, filepos: %s\n",
                                               GenTime(t, "%02u:%02u:%02u:%03u").str(),
                                               line.Mid(34).str());
                                    break;
                                }
                            }
                            else sub += split.length;
                        }
                    }
                    else fp2.printf("%s\n", line.str());
                }

                fp2.close();
            }
            else config.printf("Failed to open sub file '%s' for writing", dstsubfile.str());

            fp1.close();

            srcsubfile = src.Prefix() + ".sup.sub";
            dstsubfile = dst.PathPart().CatPath("subs", dst.FilePart().Prefix() + ".sup.sub");

            CopyFile(srcsubfile, dstsubfile);
        }
        else config.printf("Failed to open sub file '%s' for reading", srcsubfile.str());
    }
    else config.printf("'subs' directory doesn't exist");
}

bool ADVBProg::GetFileFormat(const AString& filename, AString& format)
{
    static const AString pattern = ParseRegex("Format# : {#?}");
    AString cmd;
    AString logfile = filename.Prefix() + "_format.txt";
    bool    success = false;

    format.Delete();

    cmd.printf("mediainfo \"%s\" 2>/dev/null >\"%s\"", filename.str(), logfile.str());
    if (system(cmd) == 0) {
        AStdFile fp;

        if (fp.open(logfile)) {
            AString line;

            while (line.ReadLn(fp) >= 0) {
                ADataList regions;

                if (MatchRegex(line, pattern, regions) && (regions.Count() > 0)) {
                    const REGEXREGION& region = *(const REGEXREGION *)regions[0];

                    //debug("Line: '%s', region {%u, %u}: %s\n", line.str(), region.pos, region.len, line.Mid(region.pos, region.len).str());

                    if (format.Valid()) format += ",";
                    format += line.Mid(region.pos, region.len);

                    success = true;
                }
            }

            fp.close();
        }
    }

    remove(logfile);

    return success;
}

bool ADVBProg::EncodeFile(const AString& inputfiles, const AString& aspect, const AString& outputfile, bool verbose) const
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString proccmd = config.GetEncodeCommand(GetUser(), GetModifiedCategory());
    AString args    = config.GetEncodeArgs(GetUser(), GetModifiedCategory());
    AString tempdst = config.GetRecordingsStorageDir().CatPath(outputfile.FilePart().Prefix() + "_temp." + outputfile.Suffix());
    int     i, n = args.CountLines(";");
    bool    success = true;

    for (i = 0; i < n; i++) {
        AString cmd;

        cmd.printf("nice %s %s -v %s -aspect %s %s -y \"%s\"",
                   proccmd.str(),
                   inputfiles.str(),
                   config.GetEncodeLogLevel(GetUser(), verbose).str(),
                   aspect.str(),
                   args.Line(i, ";").Words(0).str(),
                   tempdst.str());

        if (RunCommand(cmd, !verbose)) {
            AString finaldst;
            AString append;

            if (i > 0) append.printf("-%u", i);

            finaldst = outputfile.Prefix() + append + "." + outputfile.Suffix();

            config.printf("Moving file '%s' to final destination '%s'", tempdst.str(), finaldst.str());
            success &= MoveFile(tempdst, finaldst, true);
        }
        else success = false;
    }

    return success;
}

bool ADVBProg::CompareMediaFiles(const MEDIAFILE& file1, const MEDIAFILE& file2)
{
    return ((file1.pid < file2.pid) ||
            ((file1.pid == file2.pid) && (file1.filename < file2.filename)));
}

bool ADVBProg::ConvertVideoEx(bool verbose, bool cleanup, bool force)
{
    const ADVBConfig& config              = ADVBConfig::Get();
    const AString tempfilesuffix          = config.GetTempFileSuffix();
    const AString recordedfilesuffix      = config.GetRecordedFileSuffix();
    const AString videofilesuffix         = config.GetVideoFileSuffix();
    const AString audiofilesuffix         = config.GetAudioFileSuffix();
    const AString subtitlefilesuffix      = config.GetSubtitleFileSuffix();
    const AString subtitleindexfilesuffix = config.GetSubtitleIndexFileSuffix();
    AString       src                     = GetFilename();
    AString       dst                     = GenerateFilename(true);
    AString       archivedst              = ReplaceFilenameTerms(config.GetRecordingsArchiveDir(), false).CatPath(src.FilePart());

    if (!CreateDirectory(archivedst.PathPart())) {
        if (dircreationerrors.Valid()) dircreationerrors.printf("\n");
        dircreationerrors.printf("Failed to create directory '%s' for archived file", archivedst.PathPart().str());
    }

    if (IsConverted() || SameFile(src, dst)) return true;

    if (!AStdFile::exists(src)) {
        config.printf("Error: source '%s' does not exist", src.str());
        return false;
    }

    {
        uint_t nerrors;
        if (!config.IsRecordingSlave() && GetVideoErrorCount(nerrors)) {
            config.printf("'%s': %u video errors", GetQuickDescription().str(), nerrors);
        }
    }

    if (!force && AStdFile::exists(dst)) {
        config.printf("Warning: destination '%s' exists, assuming conversion is complete", dst.str());
        SetFilename(dst);
        return true;
    }

    if (!config.ConvertVideos()) {
        config.printf("Converting videos disabled on this host, copying '%s' to archive as '%s'", src.str(), archivedst.str());

        return CopyFile(src, archivedst);
    }

    config.printf("Source '%s' exists, destination '%s' does not exist", src.str(), dst.str());

    SetPostProcessing();
    ADVBProgList::AddToList(config.GetProcessingFile(), *this);

    ADateTime now;
    AString   basename = src.Prefix();
    AString   logfile  = basename + "_log.txt";
    AString   proccmd  = config.GetEncodeCommand(GetUser(), GetModifiedCategory());
    AString   args     = config.GetEncodeArgs(GetUser(), GetModifiedCategory());
    bool      success  = true;

    if (!CreateDirectory(dst.PathPart())) {
        if (dircreationerrors.Valid()) dircreationerrors.printf("\n");
        dircreationerrors.printf("Failed to create directory '%s' for destination file", dst.PathPart().str());
    }

    if (AStdFile::exists(src)) {
        AString cmd;

        cmd.printf("nice projectx -ini %s/X.ini \"%s\"", config.GetConfigDir().str(), src.str());

        success &= RunCommand(cmd, !verbose);
        remove(basename + ".sup.IFO");
    }

    std::vector<SPLIT> splits;
    std::map<AString,uint64_t> lengths;
    AString bestaspect;
    std::vector<MEDIAFILE> videofiles;
    std::vector<MEDIAFILE> audiofiles;
    std::vector<AString>   subtitlefiles;
    size_t i;

    if (success) {
        AStdFile fp;

        if (fp.open(logfile)) {
            static const AString formatmarker = "new format in next leading sequenceheader detected";
            static const AString videomarker  = "video basics";
            static const AString lengthmarker = "video length:";
            static const AString filemarker   = "---> new File:";
            static const AString pidmarker    = "++> Mpg ";
            static const AString pidmarker2   = "PID";
            AString  line;
            AString  aspect   = "16:9";
            uint64_t t1       = 0;
            uint64_t t2       = 0;
            uint64_t totallen = 0;
            uint_t   pid = 0;

            config.printf("Analysing logfile:");

            while (line.ReadLn(fp) >= 0) {
                int p;

                if ((p = line.PosNoCase(videomarker)) >= 0) {
                    aspect = GetParentheses(line, p);
                    config.printf("Found aspect '%s'", aspect.str());
                }

                if ((p = line.PosNoCase(formatmarker)) >= 0) {
                    t2 = CalcTime(GetParentheses(line, p));

                    if (t2 > t1) {
                        SPLIT split = {aspect, t1, t2 - t1};

                        config.printf("%-6s @ %s for %s", split.aspect.str(), GenTime(split.start).str(), GenTime(split.length).str());

                        splits.push_back(split);

                        lengths[aspect] += split.length;
                        if (bestaspect.Empty() || (lengths[aspect] > lengths[bestaspect])) bestaspect = aspect;

                        t1 = t2;
                    }
                }

                if ((p = line.PosNoCase(lengthmarker)) >= 0) {
                    if ((p = line.Pos(" @ ", p)) >= 0) {
                        p += 3;
                        totallen = CalcTime(line.str() + p);
                        config.printf("Total length %s", GenTime(totallen).str());
                    }
                }

                if (((p = line.PosNoCase(pidmarker)) >= 0) &&
                    ((p = line.PosNoCase(pidmarker2, p + pidmarker.len())) >= 0)) {
                    pid = (uint16_t)line.Mid(p).Word(1);
                }

                if ((p = line.PosNoCase(filemarker)) >= 0) {
                    p += filemarker.len();

                    AString filename = line.Mid(p).Words(0).DeQuotify();
                    config.printf("Created file: %s%s (PID %u)", filename.str(), AStdFile::exists(filename) ? "" : " (DOES NOT EXIST!)", pid);

                    if (AStdFile::exists(filename)) {
                        MEDIAFILE file = {pid, filename};

                        if (filename.Suffix() == videofilesuffix) {
                            videofiles.push_back(file);
                        }
                        else if (filename.Suffix() == audiofilesuffix) {
                            audiofiles.push_back(file);
                        }
                        else if (filename.Suffix() == subtitlefilesuffix) {
                            if (AStdFile::exists(filename + "." + subtitleindexfilesuffix)) {
                                subtitlefiles.push_back(filename + "." + subtitleindexfilesuffix);
                            }
                            subtitlefiles.push_back(filename);
                        }
                    }
                }
            }

            {
                SPLIT split = {aspect, t1, 0};
                splits.push_back(split);

                config.printf("%-6s @ %s for %s", split.aspect.str(), GenTime(split.start).str(), GenTime(totallen - split.start).str());

                if (totallen > 0) lengths[aspect] += totallen - t1;
                if (bestaspect.Empty() || (lengths[aspect] > lengths[bestaspect])) bestaspect = aspect;
            }

            fp.close();

            if (bestaspect.Valid()) config.printf("Best aspect is %s with %s", bestaspect.str(), GenTime(lengths[bestaspect]).str());
        }
        else {
            config.printf("Failed to open log file '%s'", logfile.str());
            success = false;
        }
    }

    if (videofiles.size() > 0) {
        std::sort(videofiles.begin(), videofiles.end(), CompareMediaFiles);

        config.printf("Video files:");
        for (i = 0; i < videofiles.size(); i++) {
            config.printf("%2u: PID %5u %s", (uint_t)i, videofiles[i].pid, videofiles[i].filename.str());
        }
    }
    if (audiofiles.size() > 0) {
        std::sort(audiofiles.begin(), audiofiles.end(), CompareMediaFiles);

        config.printf("Audio files:");
        for (i = 0; i < audiofiles.size(); i++) {
            config.printf("%2u: PID %5u %s", (uint_t)i, audiofiles[i].pid, audiofiles[i].filename.str());
        }
    }
    if (subtitlefiles.size() > 0) {
        config.printf("Subtitle files:");
        for (i = 0; i < subtitlefiles.size(); i++) {
            config.printf("%2u: %s", (uint_t)i, subtitlefiles[i].str());
        }
    }

    AString m2vfile;
    AString mp2file;
    uint_t  videotrack = (uint_t)GetAttributedConfigItem(videotrackname, "0");

    if (videotrack < (uint_t)videofiles.size()) {
        m2vfile = videofiles[videotrack].filename;
        config.printf("Using video track %u of '%s', file '%s'", videotrack, GetQuickDescription().str(), m2vfile.str());
    }
    else if (videofiles.size() > 0) {
        m2vfile = videofiles[0].filename;
        config.printf("Video track %u of '%s' doesn't exists, using file '%s' instead", videotrack, GetQuickDescription().str(), m2vfile.str());
    }
    else config.printf("Warning: no video file(s) associated with '%s'", GetQuickDescription().str());

    uint_t audiotrack = (uint_t)GetAttributedConfigItem(audiotrackname, "0");

    if (audiotrack < (uint_t)audiofiles.size()) {
        mp2file = audiofiles[audiotrack].filename;
        config.printf("Using audio track %u of '%s', file '%s'", audiotrack, GetQuickDescription().str(), mp2file.str());
    }
    else if (audiofiles.size() > 0) {
        mp2file = audiofiles[0].filename;
        config.printf("Audio track %u of '%s' doesn't exists, using file '%s' instead", audiotrack, GetQuickDescription().str(), mp2file.str());
    }
    else {
        config.printf("Warning: no audio file(s) associated with '%s'", GetQuickDescription().str());
        success = false;
    }

    if (success) {
        if (m2vfile.Empty() && mp2file.Valid()) {
            config.printf("No video: audio only");

            dst = dst.Prefix() + "." + config.GetAudioDestFileSuffix();
            AString tempdst = config.GetRecordingsStorageDir().CatPath(dst.FilePart().Prefix() + "_temp." + dst.Suffix());

            AString cmd;
            cmd.printf("nice %s -i \"%s\" -v %s %s -y \"%s\"",
                       config.GetEncodeCommand(GetUser(), GetModifiedCategory()).str(),
                       mp2file.str(),
                       config.GetEncodeLogLevel(GetUser(), verbose).str(),
                       config.GetEncodeAudioOnlyArgs(GetUser(), GetModifiedCategory()).str(),
                       tempdst.str());

            if (RunCommand(cmd, !verbose)) {
                success &= MoveFile(tempdst, dst, true);
            }
            else success = false;
        }
        else if (splits.size() == 1) {
            config.printf("No need to split file");

            ConvertSubtitles(src, dst, splits, bestaspect);

            AString inputfiles;
            inputfiles.printf("-i \"%s\" -i \"%s\"",
                              m2vfile.str(),
                              mp2file.str());
            for (i = 0; i < subtitlefiles.size(); i++) {
                inputfiles.printf(" -i \"%s\"", subtitlefiles[i].str());
            }
            if (subtitlefiles.size() > 0) {
                inputfiles.printf(" -scodec copy -metadata:s:s:0 language=eng");
            }

            success &= EncodeFile(inputfiles, bestaspect, dst, verbose);
        }
        else {
            AString remuxsrc = basename + "_Remuxed." + recordedfilesuffix;

            config.printf("Splitting file...");

            if (!AStdFile::exists(remuxsrc)) {
                AString cmd;

                cmd.printf("nice %s -fflags +genpts -i \"%s.%s\" -i \"%s.%s\"",
                           proccmd.str(),
                           basename.str(), videofilesuffix.str(),
                           basename.str(), audiofilesuffix.str());
                for (i = 0; i < subtitlefiles.size(); i++) {
                    cmd.printf(" -i \"%s\"", subtitlefiles[i].str());
                }
                if (subtitlefiles.size() > 0) {
                    cmd.printf(" -scodec copy -metadata:s:s:0 language=eng");
                }
                cmd.printf(" -acodec copy -vcodec copy -v warning -f mpegts \"%s\"",
                           remuxsrc.str());

                success &= RunCommand(cmd, !verbose);
            }

            std::vector<AString> files;
            for (i = 0; i < splits.size(); i++) {
                const auto& split = splits[i];

                if (split.aspect == bestaspect) {
                    AString cmd;
                    AString outfile;

                    outfile.printf("%s-%s-%u.%s", basename.str(), bestaspect.SearchAndReplace(":", "_").str(), (uint_t)i, recordedfilesuffix.str());

                    if (!AStdFile::exists(outfile)) {
                        cmd.printf("nice %s -fflags +genpts -i \"%s\"", proccmd.str(), remuxsrc.str());
                        for (size_t j = 0; j < subtitlefiles.size(); j++) {
                            cmd.printf(" -i \"%s\"", subtitlefiles[j].str());
                        }
                        cmd.printf(" -ss %s", GenTime(split.start).str());
                        if (split.length > 0) cmd.printf(" -t %s", GenTime(split.length).str());
                        cmd.printf(" -acodec copy -vcodec copy -scodec copy -metadata:s:s:0 language=eng -v warning -y -f mpegts \"%s\"", outfile.str());

                        success &= RunCommand(cmd, !verbose);
                        if (!success) break;
                    }

                    files.push_back(outfile);
                }
            }

            AStdFile ofp;
            AString  concatfile = basename + "_Concat." + recordedfilesuffix;
            if (ofp.open(concatfile, "wb")) {
                for (i = 0; i < files.size(); i++) {
                    AStdFile ifp;

                    if (ifp.open(files[i], "rb")) {
                        config.printf("Adding '%s'...", files[i].str());
                        CopyFile(ifp, ofp);
                        ifp.close();
                    }
                    else {
                        config.printf("Failed to open '%s' for reading", files[i].str());
                        success = false;
                        break;
                    }
                }

                ofp.close();
            }
            else {
                config.printf("Failed to open '%s' for writing", concatfile.str());
                success = false;
            }

            if (success) {
                ConvertSubtitles(src, dst, splits, bestaspect);

                AString inputfiles;
                inputfiles.printf("-i \"%s\"", concatfile.str());
                for (i = 0; i < subtitlefiles.size(); i++) {
                    inputfiles.printf(" -i \"%s\"", subtitlefiles[i].str());
                }
                if (subtitlefiles.size() > 0) {
                    inputfiles.printf(" -scodec copy -metadata:s:s:0 language=eng");
                }

                success &= EncodeFile(inputfiles, bestaspect, dst, verbose);
            }

            if (success && cleanup) {
                remove(concatfile);
                remove(remuxsrc);

                for (i = 0; i < files.size(); i++) {
                    remove(files[i]);
                }
            }
        }
    }

    if (success) {
        SetFilename(dst);

        UpdateFileSize();

        {
            FlagsSaver saver(this);
            ClearPostProcessing();
            ClearRunning();
            success = UpdateRecordedList();
        }

        config.logit("Moving '%s' to archive directory as '%s'", src.str(), archivedst.str());
        success &= MoveFile(src, archivedst);
    }

    if (success && cleanup) {
        AList delfiles;

        CollectFiles(basename.PathPart(), basename.FilePart() + ".sup*", RECURSE_ALL_SUBDIRS, delfiles);
        CollectFiles(basename.PathPart(), basename.FilePart() + "-*." + videofilesuffix, RECURSE_ALL_SUBDIRS, delfiles);
        CollectFiles(basename.PathPart(), basename.FilePart() + "-*." + audiofilesuffix, RECURSE_ALL_SUBDIRS, delfiles);

        const AString *file = AString::Cast(delfiles.First());
        while (file) {
            remove(*file);
            file = file->Next();
        }
        remove(basename + "." + videofilesuffix);
        remove(basename + "." + audiofilesuffix);
        remove(logfile);
    }

    ADVBProgList::RemoveFromList(config.GetProcessingFile(), *this);
    ClearPostProcessing();

    if (success) {
        uint64_t taken = ADateTime() - now;
        config.printf("Converting '%s' took %ss%s",
                      GetQuickDescription().str(),
                      AValue((taken + 999) / 1000).ToString().str(),
                      taken ? AString(" (%0.3;x real-time)").Arg((double)GetLength() / (double)taken).str() : "");

        AString cmd;
        if ((cmd = config.GetHierarchicalConfigItem(GetUser(), "postconvertcmd", GetTitleAndSubtitle())).Valid() ||
            (cmd = config.GetHierarchicalConfigItem(GetUser(), "postconvertcmd", GetTitle())).Valid() ||
            (cmd = config.GetHierarchicalConfigItem(GetUser(), "postconvertcmd", GetCategory())).Valid() ||
            (cmd = config.GetHierarchicalConfigItem(GetUser(), "postconvertcmd", GetChannel())).Valid() ||
            (cmd = config.GetHierarchicalConfigItem(GetUser(), "postconvertcmd", GetBaseChannel())).Valid()) {
            cmd = ReplaceTerms(cmd);
            cmd.printf(" 2>&1 >>\"%s\"", config.GetLogFile().str());

            success = RunCommand(cmd);
        }
   }
    else config.logit("Process failed!");

    return success;
}

bool ADVBProg::IsRecordable() const
{
    const ADVBConfig& config = ADVBConfig::Get();
    uint64_t now             = (uint64_t)ADateTime().TimeStamp(true);
    uint64_t graceperiod = config.GetLatestStart() * (uint64_t)60 * (uint64_t)1000;

    return (((data->start + graceperiod) >= now) && GetDVBChannel()[0]);
}

bool ADVBProg::IsConverted() const
{
    AString suf = AString(GetFilename()).Suffix();
    return (suf.Valid() && (suf != recordedfilesuffix));
}

void ADVBProg::GetFlagList(std::vector<AString>& list, bool includegetonly)
{
    uint_t maxflags = includegetonly ? Flag_total : Flag_count;
    uint_t i;

    for (i = 0; i < NUMBEROF(fields); i++) {
        if (RANGE(fields[i].type, ADVBPatterns::FieldType_flag, ADVBPatterns::FieldType_flag + maxflags - 1) && !fields[i].assignable) {
            list.push_back(fields[i].name);
        }
    }
}

sint_t ADVBProg::GetFlagNumber(const AString& name, uint_t maxflags)
{
    uint_t i;

    for (i = 0; i < NUMBEROF(fields); i++) {
        if (RANGE(fields[i].type, ADVBPatterns::FieldType_flag, ADVBPatterns::FieldType_flag + maxflags - 1) && !fields[i].assignable &&
            (CompareNoCase(name, fields[i].name) == 0)) {
            return (sint_t)(fields[i].type - ADVBPatterns::FieldType_flag);
        }
    }

    return -1;
}

bool ADVBProg::SetFlag(const AString& name, bool set)
{
    sint_t n;
    bool success = false;

    if ((n = GetFlagNumber(name, Flag_count)) >= 0) {
        SetFlag((uint8_t)n, set);
        success = true;
    }

    return success;
}

bool ADVBProg::ClrFlag(const AString& name)
{
    return SetFlag(name, false);
}

bool ADVBProg::GetFlag(const AString& name) const
{
    sint_t n;
    bool state = false;

    if ((n = GetFlagNumber(name)) >= 0) {
        state = GetFlag((uint8_t)n);
    }

    return state;
}

bool ADVBProg::ConvertVideo(bool verbose, bool cleanup, bool force)
{
    const ADVBConfig& config = ADVBConfig::Get();
    bool success = ConvertVideoEx(verbose, cleanup, force);

    if (success) {
        OnRecordSuccess();
    }
    else {
        OnRecordFailure();

        ClearScheduled();
        SetRecordFailed();

        {
            FlagsSaver saver(this);
            ClearRunning();
            ADVBProgList::AddToList(config.GetRecordFailuresFile(), *this);
        }
    }

    return success;
}

bool ADVBProg::ForceConvertVideo(bool verbose, bool cleanup)
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString src     = GetArchiveRecordingFilename();
    AString dst     = GetTempRecordingFilename();
    bool    success = false;

    if (verbose) config.printf("Force converting '%s', source file '%s', intermediate file '%s'", GetQuickDescription().str(), src.str(), dst.str());

    if (!AStdFile::exists(dst)) {
        if (AStdFile::exists(src)) {
            if (verbose) config.printf("Copying file '%s' to '%s' to convert", src.str(), dst.str());
            if (CopyFile(src, dst)) {
                if (verbose) config.printf("Copied file okay");
                success = true;
            }
        }
        else config.printf("File '%s' doesn't exists", src.str());
    }
    else success = true;

    if (success) {
        SetFilename(dst);
        success = ConvertVideo(verbose, cleanup, true);
    }

    return success;
}

void ADVBProg::GetEncodedFiles(AList& files) const
{
    AString filename = GetFilename();

    CollectFiles(filename.PathPart(), filename.FilePart().Prefix() + ".*", RECURSE_ALL_SUBDIRS, files);
}

bool ADVBProg::DeleteEncodedFiles() const
{
    const ADVBConfig& config = ADVBConfig::Get();
    AList files;

    GetEncodedFiles(files);

    const AString *file = AString::Cast(files.First());
    bool  success = (file != NULL);
    while (file) {
        bool filedeleted = (remove(*file) == 0);

        config.printf("Delete file '%s': %s", file->str(), filedeleted ? "success" : "failed");
        success &= filedeleted;

        file = file->Next();
    }

    return success;
}

bool ADVBProg::GetVideoErrorCount(uint_t& count) const
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString cmd;
    bool success = false;

    if ((cmd = config.GetVideoErrorCheckCommand()).Valid()) {
        AString filename = GenerateFilename();

        if (!AStdFile::exists(filename)) {
            config.logit("File '%s' doesn't exist, trying archive filename '%s'", filename.str(), GetArchiveRecordingFilename().str());
            filename = GetArchiveRecordingFilename();
        }

        if (AStdFile::exists(filename)) {
            AString tempfile = config.GetTempFile("errorcheck", ".txt");

            cmd = cmd.SearchAndReplace("{filename}", filename).SearchAndReplace("{logfile}", tempfile);

            if (system(cmd) == 0) {
                AStdFile fp;

                if (fp.open(tempfile)) {
                    AString line;

                    if (line.ReadLn(fp) >= 0) {
                        if (sscanf(line, "%u", &count) > 0) {
                            success = true;
                        }
                        else config.logit("'%s': failed to extract count from '%s'", GetTitleAndSubtitle().str(), line.str());
                    }
                    else config.logit("'%s': failed to read line from log file '%s'", GetTitleAndSubtitle().str(), tempfile.str());
                }
                else config.logit("'%s': failed to open log file '%s'", GetTitleAndSubtitle().str(), tempfile.str());
            }
            else config.logit("'%s': failed to run command '%s'", GetTitleAndSubtitle().str(), cmd.str());

            remove(tempfile);
        }
        else config.logit("'%s': archive file '%s' doesn't exist", GetTitleAndSubtitle().str(), filename.str());
    }

    return success;
}

AString ADVBProg::GetArchiveRecordingFilename() const
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString filename = GetFilename();
    return ReplaceFilenameTerms(config.GetRecordingsArchiveDir(), false).CatPath(filename.FilePart().Prefix() + "." + recordedfilesuffix);
}

AString ADVBProg::GetTempRecordingFilename() const
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString filename = GetFilename();
    return ReplaceFilenameTerms(config.GetRecordingsStorageDir(), false).CatPath(filename.FilePart().Prefix() + "." + recordedfilesuffix);
}
