
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#include <rdlib/Regex.h>
#include <rdlib/Recurse.h>

#include "config.h"
#include "dvbprog.h"
#include "proglist.h"
#include "channellist.h"
#include "dvblock.h"
#include "dvbpatterns.h"

#define DEBUG_SAMEPROGRAMME 0

bool ADVBProg::debugsameprogramme = true;

AHash ADVBProg::fieldhash;

const ADVBProg::DVBPROG *ADVBProg::nullprog = (const ADVBProg::DVBPROG *)NULL;

#define DVBPROG_OFFSET(x) ((uint16_t)(uptr_t)&nullprog->x)
#define DEFINE_FIELD(name,var,type,desc)	{#name, ADVBPatterns::FieldType_##type, false, DVBPROG_OFFSET(var), desc}
#define DEFINE_SIMPLE(name,type,desc)		DEFINE_FIELD(name, name, type, desc)
#define DEFINE_STRING(name,desc)			DEFINE_FIELD(name, strings.name, string, desc)
#define DEFINE_FLAG(name,flag,desc)			{#name, ADVBPatterns::FieldType_flag + flag, false, DVBPROG_OFFSET(flags), desc}
#define DEFINE_FLAG_ASSIGN(name,flag,desc)	{#name, ADVBPatterns::FieldType_flag + flag, true, DVBPROG_OFFSET(flags), desc}
#define DEFINE_ASSIGN(name,var,type,desc)	{#name, ADVBPatterns::FieldType_##type, true, DVBPROG_OFFSET(var), desc}
const ADVBProg::FIELD ADVBProg::fields[] = {
	{"prog", ADVBPatterns::FieldType_prog, false, 0, "Encoded Programme"},

	DEFINE_STRING(channel, "TV channel"),
	DEFINE_STRING(basechannel, "Base channel"),
	DEFINE_STRING(channelid, "XMLTV channel ID"),
	DEFINE_STRING(dvbchannel, "DVB channel ID"),
	DEFINE_STRING(title, "Programme title"),
	DEFINE_STRING(subtitle, "Programme sub-title"),
	DEFINE_STRING(desc, "Programme description"),
	DEFINE_STRING(category, "Programme category"),
	DEFINE_STRING(director, "Director"),
	DEFINE_STRING(episodenum, "Episode ID (XMLTV)"),
	DEFINE_STRING(uuid, "UUID"),
	DEFINE_STRING(filename, "Recorded filename"),
	DEFINE_FIELD(actor, strings.actors, string, "Actor(s)"),

	DEFINE_FIELD(on, start, date, "Day"),
	DEFINE_FIELD(day, start, date, "Day"),
	DEFINE_SIMPLE(start, date, "Start"),
	DEFINE_SIMPLE(stop, date, "Stop"),
	DEFINE_SIMPLE(recstart, date, "Record start"),
	DEFINE_SIMPLE(recstop, date, "Record stop"),
	DEFINE_SIMPLE(actstart, date, "Actual record start"),
	DEFINE_SIMPLE(actstop, date, "Actual record stop"),
	DEFINE_FIELD(length, start, span, "Programme length"),
	DEFINE_FIELD(reclength, recstart, span, "Record length"),
	DEFINE_FIELD(actlength, actstart, span, "Actual record length"),

	DEFINE_SIMPLE(year, uint16_t, "Year"),

	DEFINE_FIELD(card, dvbcard, uint8_t, "DVB card"),

	DEFINE_FLAG(repeat,    		   	 Flag_repeat,    	 	   "Programme is a repeat"),
	DEFINE_FLAG(plus1,     		   	 Flag_plus1,     	 	   "Programme is on +1"),
	DEFINE_FLAG(running,			 Flag_running,  	 	   "Programme job running"),
	DEFINE_FLAG(onceonly,  		   	 Flag_onceonly,  	 	   "Programme to be recorded and then the pattern deleted"),
	DEFINE_FLAG(rejected,  		   	 Flag_rejected,  	 	   "Programme rejected"),
	DEFINE_FLAG(recorded,  		   	 Flag_recorded,  	 	   "Programme recorded"),
	DEFINE_FLAG(scheduled, 		   	 Flag_scheduled, 	 	   "Programme scheduled"),
	DEFINE_FLAG(dvbcardspecified, 	 Flag_dvbcardspecified,    "Programme to be recorded on specified DVB card"),
	DEFINE_FLAG(radioprogramme,    	 Flag_radioprogramme,      "Programme is a Radio programme"),
	DEFINE_FLAG(incompleterecording, Flag_incompleterecording, "Programme recorded is incomplete"),
	DEFINE_FLAG(ignorerecording,     Flag_ignorerecording,     "Programme recorded should be ignored when scheduling"),
	DEFINE_FLAG(failed,				 Flag_recordingfailed,     "Programme recording failed"),
	DEFINE_FLAG(recording,  		 Flag_recording,  	 	   "Programme recording"),
	DEFINE_FLAG(postprocessing,		 Flag_postprocessing,	   "Programme recording being processing"),
	DEFINE_FLAG(exists,				 Flag_exists,    	 	   "Programme exists"),

	DEFINE_FIELD(epvalid,  	  episode.valid,    uint8_t,  "Series/episode valid"),
	DEFINE_FIELD(series,   	  episode.series,   uint8_t,  "Series"),
	DEFINE_FIELD(episode,  	  episode.episode,  uint16_t, "Episode"),
	DEFINE_FIELD(episodes, 	  episode.episodes, uint16_t, "Number of episodes in series"),

	DEFINE_FIELD(assignedepisode, assignedepisode, uint16_t, "Assigned episode"),

	DEFINE_FLAG_ASSIGN(usedesc,		  Flag_usedesc, "Use description"),
	DEFINE_FLAG_ASSIGN(allowrepeats,  Flag_allowrepeats, "Allow repeats to be recorded"),
	DEFINE_FLAG_ASSIGN(fakerecording, Flag_fakerecording, "Do not record, just pretend to"),
	DEFINE_FLAG_ASSIGN(urgent,		  Flag_urgent, "Record as soon as possible"),
	DEFINE_FLAG_ASSIGN(markonly,	  Flag_markonly, "Do not record, just mark as recorded"),
	DEFINE_FLAG_ASSIGN(postprocess,	  Flag_postprocess, "Post process programme"),
	DEFINE_FLAG_ASSIGN(onceonly,	  Flag_onceonly, "Once recorded, delete the pattern"),

	DEFINE_ASSIGN(pri,        pri,        	 	sint8_t, "Scheduling priority"),
	DEFINE_ASSIGN(score,	  score,			sint16_t, "Record score"),
	DEFINE_ASSIGN(prehandle,  prehandle,  	 	uint16_t, "Record pre-handle (minutes)"),
	DEFINE_ASSIGN(posthandle, posthandle, 	 	uint16_t, "Record post-handle (minutes)"),
	DEFINE_ASSIGN(user,		  strings.user,  	string, "User"),
	DEFINE_ASSIGN(dir,		  strings.dir,  	string, "Directory to store file in"),
	DEFINE_ASSIGN(prefs,	  strings.prefs, 	string, "Misc prefs"),
	DEFINE_ASSIGN(dvbcard,    dvbcard,        	uint8_t, "DVB card to record from"),
};

ADVBProg::ADVBProg()
{
	Init();

	Delete();
}

ADVBProg::ADVBProg(AStdData& fp)
{
	Init();

	operator = (fp);
}

ADVBProg::ADVBProg(const AString& str)
{
	Init();

	operator = (str);
}

ADVBProg::ADVBProg(const ADVBProg& obj)
{
	Init();

	operator = (obj);
}

ADVBProg::~ADVBProg()
{
	if (data) free(data);
}

void ADVBProg::StaticInit()
{
	if (!fieldhash.Valid()) {
		uint_t i;

		fieldhash.Create(20);
		for (i = 0; i < NUMBEROF(fields); i++) {
			fieldhash.Insert(fields[i].name, (uptr_t)(fields + i));
		}
	}
}

const ADVBProg::FIELD *ADVBProg::GetFields(uint_t& nfields)
{
	nfields = NUMBEROF(fields);
	return fields;
}

uint16_t ADVBProg::GetUserDataOffset()
{
	return DVBPROG_OFFSET(strings.user);
}

uint16_t ADVBProg::GetActorsDataOffset()
{
	return DVBPROG_OFFSET(strings.actors);
}

uint16_t ADVBProg::GetPriDataOffset()
{
	return DVBPROG_OFFSET(pri);
}

uint16_t ADVBProg::GetScoreDataOffset()
{
	return DVBPROG_OFFSET(score);
}

void ADVBProg::Init()
{
	StaticInit();

	// initialise primary data
	data     = NULL;
	maxsize  = 0;

	// initialise record scheduling data
	list           = NULL;
	priority_score = 0;
	overlaps       = 0;
}

void ADVBProg::SwapBytes(DVBPROG *prog)
{
	uint16_t *strings = &prog->strings.channel;
	uint_t i;

	prog->start    = ::SwapBytes(prog->start);
	prog->stop     = ::SwapBytes(prog->stop);
	prog->recstart = ::SwapBytes(prog->recstart);
	prog->recstop  = ::SwapBytes(prog->recstop);
	prog->actstart = ::SwapBytes(prog->actstart);
	prog->actstop  = ::SwapBytes(prog->actstop);

	prog->filesize = ::SwapBytes(prog->filesize);
	prog->flags    = ::SwapBytes(prog->flags);

	prog->episode.episode  = ::SwapBytes(prog->episode.episode);
	prog->episode.episodes = ::SwapBytes(prog->episode.episodes);

	for (i = 0; i < StringCount; i++) strings[i] = ::SwapBytes(strings[i]);

	prog->assignedepisode = ::SwapBytes(prog->assignedepisode);
	prog->year            = ::SwapBytes(prog->year);
	prog->jobid           = ::SwapBytes(prog->jobid);
	prog->score           = ::SwapBytes((uint16_t)prog->score);

	prog->bigendian       = (uint8_t)MachineIsBigEndian();
}

ADVBProg& ADVBProg::operator = (AStdData& fp)
{
	DVBPROG _data;
	sint32_t n;
	bool   success = false;

	Delete();

	if ((n = fp.readbytes((uint8_t *)&_data, sizeof(_data))) == (sint32_t)sizeof(_data)) {
		if (_data.bigendian != (uint8_t)MachineIsBigEndian()) SwapBytes(data);

		if ((sizeof(_data) + _data.strings.end) > maxsize) {
			if (data) free(data);

			maxsize = sizeof(_data) + _data.strings.end;
			data = (DVBPROG *)calloc(1, maxsize);
			data->bigendian = (uint8_t)MachineIsBigEndian();
		}

		if (data && ((sizeof(_data) + _data.strings.end) <= maxsize)) {
			memcpy(data, &_data, sizeof(*data));
				
			if ((n = fp.readbytes(data->strdata, _data.strings.end)) == (sint32_t)_data.strings.end) {
				success = true;
			}
		}
	}

	if (!success) Delete();

	return *this;
}

bool ADVBProg::Base64Decode(const AString& str)
{
	sint_t len = str.Base64DecodeLength();
	bool success = false;

	Delete();

	if (len > 0) {
		if ((uint_t)len > maxsize) {
			if (data) free(data);
			maxsize = len;
			data    = (DVBPROG *)calloc(1, maxsize);
			data->bigendian = (uint8_t)MachineIsBigEndian();
		}

		if (data) {
			success = ((str.Base64Decode((uint8_t *)data, maxsize) == len) && Valid());
		}
	}

	if (!success) Delete();
	
	return success;
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
			set = AStdFile::exists(GetFilename());
			break;
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
	uint64_t maxreclag = (uint64_t)config.GetConfigItem("maxrecordlag", "20") * 1000ULL;

	SetFlag(Flag_incompleterecording, !((data->actstart <= (data->start + maxreclag)) && (data->actstop >= MIN(data->stop, data->recstop - 1000))));
}

ADVBProg& ADVBProg::operator = (const AString& str)
{
	static const AString tsod = "tsod.plus-1.";
	const ADVBConfig& config = ADVBConfig::Get();

	Delete();

	data->start           = (uint64_t)GetField(str, "start");
	data->stop            = (uint64_t)GetField(str, "stop");
	data->recstart        = (uint64_t)GetField(str, "recstart");
	data->recstop         = (uint64_t)GetField(str, "recstop");
	data->actstart        = (uint64_t)GetField(str, "actstart");
	data->actstop         = (uint64_t)GetField(str, "actstop");
	if (FieldExists(str, "episodenum")) {
		data->episode     = GetEpisode(GetField(str, "episodenum"));
	}
	else {
		data->episode.series   = (uint_t)GetField(str, "series");
		data->episode.episode  = (uint_t)GetField(str, "episode");
		data->episode.episodes = (uint_t)GetField(str, "episodes");
		data->episode.valid    = (data->episode.series || data->episode.episode || data->episode.episodes);
	}
	data->assignedepisode = (uint16_t)GetField(str, "assignedepisode");
	if (FieldExists(str, "date")) {
		data->year = (uint16_t)GetField(str, "date");
	}
	else data->year = (uint16_t)GetField(str, "year");
	
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
	SetString(&data->strings.dvbchannel, GetField(str, "dvbchannel"));
	SetString(&data->strings.desc, GetField(str, "desc"));
	SetString(&data->strings.category, GetField(str, "category"));
	SetString(&data->strings.director, GetField(str, "director"));
	SetString(&data->strings.episodenum, GetField(str, "episodenum"));

	if (FieldExists(str, "previouslyshown")) SetFlag(Flag_repeat);

	AString actors, actor;
	int p = 0;

	while ((actor = GetField(str, "actor", p, &p)).Valid()) {
		actors.printf("%s\n", actor.str());
		p++;
	}
	SetString(&data->strings.actors, actors);

	SetUUID();

	SetUser(GetField(str, "user"));
	if (FieldExists(str, "dir")) SetDir(GetField(str, "dir"));
	else						 SetDir(config.GetUserConfigItem(GetUser(), "dir"));
	SetFilename(GetField(str, "filename"));
	SetPattern(GetField(str, "pattern"));

	data->jobid = (uint_t)GetField(str, "jobid");
	data->score = (sint_t)GetField(str, "score");

	if (FieldExists(str, "prehandle")) 	data->prehandle  = (uint_t)GetField(str, "prehandle");
	else							   	data->prehandle  = (uint_t)config.GetUserConfigItem(GetUser(), "prehandle");
	if (FieldExists(str, "posthandle")) data->posthandle = (uint_t)GetField(str, "posthandle");
	else							    data->posthandle = (uint_t)config.GetUserConfigItem(GetUser(), "posthandle");
	if (FieldExists(str, "pri"))		data->pri		 = (int)GetField(str, "pri");
	else								data->pri		 = (int)config.GetUserConfigItem(GetUser(), "pri");

	data->dvbcard 	 = (uint_t)GetField(str, "card");

	SearchAndReplace("\xa3", "£");

	if (!Valid()) Delete();

	return *this;
}

ADVBProg& ADVBProg::operator = (const ADVBProg& obj)
{
	bool success = false;

	Delete();

	if (obj.Valid()) {
		if ((sizeof(*obj.data) + obj.data->strings.end) > maxsize) {
			if (data) free(data);

			maxsize = sizeof(*obj.data) + obj.data->strings.end;
			data = (DVBPROG *)calloc(1, maxsize);
			data->bigendian = (uint8_t)MachineIsBigEndian();
		}

		if (data && ((sizeof(*obj.data) + obj.data->strings.end) <= maxsize)) {
			memcpy(data, obj.data, sizeof(*obj.data) + obj.data->strings.end);

			success = true;
		}
	}

	if (!success) Delete();

	return *this;
}

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

bool ADVBProg::WriteToFile(AStdData& fp) const
{
	return (fp.writebytes((uint8_t *)data, sizeof(*data) + data->strings.end) == (slong_t)(sizeof(*data) + data->strings.end));
}

AString ADVBProg::ExportToText() const
{
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

	str.printf("flags=$%08x\n", data->flags);

	if (data->filesize) str.printf("filesize=%s\n", NUMSTR("", data->filesize));
	if (data->assignedepisode) str.printf("assignedepisode=%u\n", data->assignedepisode);
	if (data->year) str.printf("year=%u\n", data->year);

	if (GetString(data->strings.pattern)[0]) {
		str.printf("score=%d\n", (sint_t)data->score);
		str.printf("prehandle=%u\n", (uint_t)data->prehandle);
		str.printf("posthandle=%u\n", (uint_t)data->posthandle);
		str.printf("pri=%d\n", (int)data->pri);
	}

	if (RecordDataValid()) {
		str.printf("jobid=%u\n", data->jobid);
		str.printf("dvbcard=%u\n", (uint_t)data->dvbcard);
	}

	return str;
}

AString ADVBProg::ExportToJSON(bool includebase64) const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString str;
	const char *p;

	str.printf("\"start\":%lu", JSONTime(data->start));
	str.printf(",\"stop\":%lu", JSONTime(data->stop));
	if (data->recstart || data->recstop) {
		str.printf(",\"recstart\":%lu", JSONTime(data->recstart));
		str.printf(",\"recstop\":%lu", JSONTime(data->recstop));
	}
	if (data->actstart || data->actstop) {
		str.printf(",\"actstart\":%lu", JSONTime(data->actstart));
		str.printf(",\"actstop\":%lu", JSONTime(data->actstop));
	}
	str.printf(",\"channel\":\"%s\"", JSONFormat(GetString(data->strings.channel)).str());
	str.printf(",\"basechannel\":\"%s\"", JSONFormat(GetString(data->strings.basechannel)).str());
	str.printf(",\"channelid\":\"%s\"", JSONFormat(GetString(data->strings.channelid)).str());
	if ((p = GetString(data->strings.dvbchannel))[0]) str.printf(",\"dvbchannel\":\"%s\"", JSONFormat(p).str());
	str.printf(",\"title\":\"%s\"", JSONFormat(GetString(data->strings.title)).str());
	if ((p = GetString(data->strings.subtitle))[0]) str.printf(",\"subtitle\":\"%s\"", JSONFormat(p).str());
	if ((p = GetString(data->strings.desc))[0]) str.printf(",\"desc\":\"%s\"", JSONFormat(p).str());
	if ((p = GetString(data->strings.category))[0]) str.printf(",\"category\":\"%s\"", JSONFormat(p).str());
	if ((p = GetString(data->strings.director))[0]) str.printf(",\"director\":\"%s\"", JSONFormat(p).str());
	if ((p = GetString(data->strings.episodenum))[0]) str.printf(",\"episodenum\":\"%s\"", JSONFormat(p).str());
	if ((p = GetString(data->strings.user))[0]) str.printf(",\"user\":\"%s\"", JSONFormat(p).str());
	str.printf(",\"dir\":\"%s\"", JSONFormat(GetString(data->strings.dir)).str());
	if ((p = GetString(data->strings.filename))[0]) str.printf(",\"filename\":\"%s\"", JSONFormat(p).str());
	if ((p = GetString(data->strings.pattern))[0]) str.printf(",\"pattern\":\"%s\"", JSONFormat(p).str());
	str.printf(",\"uuid\":\"%s\"", JSONFormat(GetString(data->strings.uuid)).str());
	if ((p = GetString(data->strings.actors))[0]) {
		AString _actors = AString(p);
		uint_t i, n = _actors.CountLines("\n", 0);

		str.printf(",\"actors\":[");

		for (i = 0; i < n; i++) {
			if (i) str.printf(",");
			str.printf("\"%s\"", JSONFormat(_actors.Line(i, "\n", 0)).str());
		}

		str.printf("]");
	}
	if ((p = GetString(data->strings.prefs))[0]) {
		AString _prefs = AString(p);
		uint_t i, n = _prefs.CountLines("\n", 0);

		str.printf(",\"prefs\":[");

		for (i = 0; i < n; i++) {
			if (i) str.printf(",");
			str.printf("\"%s\"", JSONFormat(_prefs.Line(i, "\n", 0)).str());
		}

		str.printf("]");
	}

	if (data->episode.valid) {
		str.printf(",\"episode\":{");
		
		bool flag = true;
		if (data->episode.series) {
			str.printf("\"series\":%u", (uint_t)data->episode.series);
			flag = false;
		}
		if (data->episode.episode) {
			str.printf("%s\"episode\":%u", flag ? "" : ",", (uint_t)data->episode.episode);
			flag = false;
		}
		if (data->episode.episodes) {
			str.printf("%s\"episodes\":%u", flag ? "" : ",", (uint_t)data->episode.episodes);
		}
		str.printf("}");
	}

	str.printf(",\"flags\":{");
	str.printf("\"bitmap\":%lu", (ulong_t)data->flags);
	uint_t i;
	for (i = 0; i < NUMBEROF(fields); i++) {
		if (RANGE(fields[i].type, ADVBPatterns::FieldType_flag, ADVBPatterns::FieldType_lastflag)) {
			str.printf(",\"%s\":%u", fields[i].name, (uint_t)GetFlag(fields[i].type - ADVBPatterns::FieldType_flag));
		}
	}
	str.printf("}");

	if (data->filesize) str.printf(",\"filesize\":%" FMT64 "u", data->filesize);
	if (data->assignedepisode) str.printf(",\"assignedepisode\":%u", data->assignedepisode);
	if (data->year) str.printf(",\"year\":%u", (uint_t)data->year);

	if (GetString(data->strings.pattern)[0]) {
		str.printf(",\"score\":%d", (sint_t)data->score);
		str.printf(",\"prehandle\":%u", (uint_t)data->prehandle);
		str.printf(",\"posthandle\":%u", (uint_t)data->posthandle);
		str.printf(",\"pri\":%d", (int)data->pri);
	}

	if (RecordDataValid()) {
		str.printf(",\"jobid\":%u", data->jobid);
		str.printf(",\"dvbcard\":%u", (uint_t)data->dvbcard);

		const AString filename = GetString(data->strings.filename);
		if (data->actstart && data->actstop && filename[0]) {
			str.printf(",\"exists\":%u", (uint_t)AStdFile::exists(filename));

			AString relpath;
			if (AStdFile::exists(filename) && (relpath = config.GetRelativePath(filename)).Valid()) {
				str.printf(",\"path\":\"%s\"", JSONFormat(relpath).str());
				str.printf(",\"subpaths\":[");
				
				AList list;
				CollectFiles(filename.PathPart(), filename.FilePart().Prefix() + ".*", RECURSE_ALL_SUBDIRS, list);
				const AString *subfile = AString::Cast(list.First());
				bool first = true;
				while (subfile) {
					if ((*subfile != filename) && (relpath = config.GetRelativePath(*subfile)).Valid()) {
						if (first) first = false;
						else str.printf(",");
						str.printf("\"%s\"", JSONFormat(relpath).str());
					}
					subfile = subfile->Next();
				}

				str.printf("]");
			}
		}
	}

	if (includebase64) {
		str.printf(",\"base64\":\"%s\"", Base64Encode().str());
	}

	return str;
}

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

		data->bigendian = (uint8_t)MachineIsBigEndian();
	}
}

bool ADVBProg::SetString(const uint16_t *offset, const char *str)
{
	bool success = false;

	//debug("ADVBProg:<%08lx>: Setting field offset %u as '%s'\n", (uptr_t)this, (uint_t)((uptr_t)offset - (uptr_t)data), str);
	if ((offset >= &data->strings.channel) && (offset < &data->strings.end)) {
		uint16_t *strings = &data->strings.channel;
		uint_t   field    = offset - strings;
		uint16_t len1	  = strings[field + 1] - strings[field];
		uint16_t len2	  = strlen(str) + 1;
		sint16_t diff	  = len2 - len1;
		uint_t i;

		// debug("Before:\n");
		// for (i = 0; i < StringCount; i++) {
		// 	debug("\tField %2u at %4u '%s'\n", i, strings[i], data->strdata + strings[i]);
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

				strcpy(data->strdata + strings[field], str);

				success = true;
			}

			// debug("After:\n");
			// for (i = 0; i < StringCount; i++) {
			// 	debug("\tField %2u at %4u '%s'\n", i, strings[i], data->strdata + strings[i]);
			// }
		}
		else success = true;
	}

	return success;
}

void ADVBProg::SetFlag(uint8_t flag, bool set)
{
	if (set) data->flags |= GetFlagMask(flag, set);
	else	 data->flags &= GetFlagMask(flag, set);
}

int ADVBProg::Compare(const ADVBProg *prog1, const ADVBProg *prog2, const bool *reverse)
{
	int res = 0;

	if		(prog1->GetStart() < prog2->GetStart()) res = -1;
	else if (prog1->GetStart() > prog2->GetStart()) res =  1;
	else if (((res = CompareNoCase(prog1->GetChannel(), prog2->GetChannel())) == 0) &&
			 ((res = CompareNoCase(prog1->GetTitle(),   prog2->GetTitle()))   == 0)) {
		res = CompareNoCase(prog1->GetSubtitle(), prog2->GetSubtitle());
	}
		

	if (reverse && *reverse) res = -res;

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
		else if (sscanf(str, ".%u/%u.",
						&_episode,
						&_episodes) == 2) {
			episode.episode  = _episode + 1;
			episode.episodes = _episodes;
		}
		else if (sscanf(str, "%u.%u.",
						&_series,
						&_episode) == 2) {
			episode.series   = _series  + 1;
			episode.episode  = _episode + 1;
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

AString ADVBProg::GetDescription(uint_t verbosity) const
{
	EPISODE ep;
	AString str;
	const char *p;

	str.printf("%s - %s : %s : %s",
			   GetStartDT().UTCToLocal().DateFormat("%d %D-%N-%Y %h:%m").str(),
			   GetStopDT().UTCToLocal().DateFormat("%h:%m").str(),
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
		if (data->assignedepisode) {
			str.printf(" (F%u)", data->assignedepisode);
		}

		if (IsRepeat()) str.printf(" (R)");
		if (IsPlus1())  str.printf(" (+1)");
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
			str1.printf("%s.", p);
		}

		if ((verbosity > 2) && ep.valid) {
			AString epstr;

			if (ep.series) epstr.printf("Series %u", ep.series);
			if (ep.episode) {
				if (epstr.Valid()) epstr.printf(", episode %u", ep.episode);
				else			   epstr.printf("Episode %u", ep.episode);
				if (ep.episodes)   epstr.printf(" of %u", ep.episodes);
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
			if (GetUser()[0]) str1.printf(", user '%s'", GetUser());
			if (data->jobid) str1.printf(" (job %u, card %u)", data->jobid, (uint_t)data->dvbcard);
			if (IsRejected()) str1.printf(" ** REJECTED **");
		}

		if ((verbosity > 3) && data->recstart) {
			if (str1.Valid()) str1.printf("\n\n");
			str1.printf("Set to record %s - %s (%um %us)",
						GetRecordStartDT().UTCToLocal().DateFormat("%d %D-%N-%Y %h:%m:%s").str(),
						GetRecordStopDT().UTCToLocal().DateFormat("%h:%m:%s").str(),
						(uint_t)(GetRecordLength() / 60000ULL), (uint_t)((GetRecordLength() % 60000ULL) / 1000ULL));

			if (GetUser()[0]) str1.printf(" by user '%s'", GetUser());

			if (data->actstart) {
				str1.printf(", actual record time %s - %s (%um %us)%s",
							GetActualStartDT().UTCToLocal().DateFormat("%d %D-%N-%Y %h:%m:%s").str(),
							GetActualStopDT().UTCToLocal().DateFormat("%h:%m:%s").str(),
							(uint_t)(GetActualLength() / 60000ULL), (uint_t)((GetActualLength() % 60000ULL) / 1000ULL),
							IsRecordingComplete() ? "" : " *not complete* ");
			}

			if (IgnoreRecording()) str1.printf(" (ignored when scheduling)");
		}

		if ((verbosity > 4) && GetFilename()[0]) {
			if (str1.Valid()) str1.printf("\n\n");
			str1.printf("%s as '%s'.", data->actstart ? "Recorded" : (data->recstart ? "To be recorded" : "Would be recorded"), GetFilename());

			if (IsPostProcessing()) str1.printf(" Current being post-processed.");
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
	EPISODE ep;

	ep = GetEpisode();
	if (ep.valid) {
		str.printf(" (%s)", GetEpisodeString(ep).str());
	}
	if (data->assignedepisode) {
		str.printf(" (F%u)", data->assignedepisode);
	}

	if (IsRepeat()) str.printf(" (R)");

	str.printf(" (%s - %s on %s)",
			   GetStartDT().UTCToLocal().DateFormat("%d %D-%N-%Y %h:%m").str(),
			   GetStopDT().UTCToLocal().DateFormat("%h:%m").str(),
			   GetChannel());

	return str;
}

bool ADVBProg::SameProgramme(const ADVBProg& prog1, const ADVBProg& prog2)
{
	static const uint64_t hour = 3600ULL * 1000ULL;
	bool same = false;

	if (CompareNoCase(prog1.GetTitle(), prog2.GetTitle()) == 0) {
		const char 	   *desc1 	  = prog1.GetDesc();
		const char 	   *desc2 	  = prog2.GetDesc();
		const char 	   *subtitle1 = prog1.GetSubtitle();
		const char 	   *subtitle2 = prog2.GetSubtitle();
		const EPISODE& ep1    	  = prog1.GetEpisode();
		const EPISODE& ep2    	  = prog2.GetEpisode();

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
			// -> definitely the same programme
#if DEBUG_SAMEPROGRAMME
			if (debugsameprogramme) debug("'%s' / '%s': both films\n", prog1.GetDescription().str(), prog2.GetDescription().str());
#endif
			same = true;
		}
		else if (ep1.valid && ep2.valid) {
			same = ((ep1.series == ep2.series) && (ep1.episode == ep2.episode));
#if DEBUG_SAMEPROGRAMME
			if (debugsameprogramme) debug("'%s' / '%s': episode S%uE%02u / S%uE%02u: %s\n", prog1.GetDescription().str(), prog2.GetDescription().str(), (uint_t)ep1.series, (uint_t)ep1.episode, (uint_t)ep2.series, (uint_t)ep2.episode, same ? "same" : "different");
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

AString ADVBProg::ValidFilename(const AString& str) const
{
	AString res;
	AStringUpdate updater(&res);
	uint_t i, l = str.len();

	for (i = 0; i < l; i++) {
		char c = str[i];

		if		(IsSymbolChar(c) || (c == '.') || (c == '-')) updater.Update(c);
		else if ((c == '/') || IsWhiteSpace(c)) updater.Update('_');
	}

	updater.Flush();

	return res;
}

void ADVBProg::SearchAndReplace(const AString& search, const AString& replace)
{
	const uint16_t *strs = &data->strings.channel;
	uint_t i;

	for (i = 0; i < (sizeof(data->strings) / sizeof(strs[0])); i++) {
		SetString(strs + i, AString(GetString(strs[i])).SearchAndReplace(search, replace));
	}
}

AString ADVBProg::GenerateFilename(const AString& templ) const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString dir      = CatPath(config.GetRecordingsDir(), GetDir());
	AString date     = GetStartDT().UTCToLocal().DateFormat("%Y-%M-%D");
	AString times    = GetStartDT().UTCToLocal().DateFormat("%h%m") + "-" + GetStopDT().UTCToLocal().DateFormat("%h%m");
	AString filename = config.ReplaceTerms(GetUser(),
										   CatPath(dir, templ)
										   .SearchAndReplace("{title}", ValidFilename(GetTitle()))
										   .SearchAndReplace("{subtitle}", ValidFilename(GetSubtitle()))
										   .SearchAndReplace("{episode}", ValidFilename(GetEpisodeString()))
										   .SearchAndReplace("{channel}", ValidFilename(GetChannel()))
										   .SearchAndReplace("{date}", ValidFilename(date))
										   .SearchAndReplace("{times}", ValidFilename(times))
										   .SearchAndReplace("{user}", ValidFilename(GetUser()))
										   .SearchAndReplace("{suffix}", config.GetFileSuffix(GetUser())));

	while (filename.Pos("{sep}{sep}") >= 0) {
		filename = filename.SearchAndReplace("{sep}{sep}", "{sep}");
	}
	
	filename = filename.SearchAndReplace("{sep}", ".");

	return filename;
}

AString ADVBProg::GenerateFilename() const
{
	const ADVBConfig& config = ADVBConfig::Get();
	return GenerateFilename(config.GetFilenameTemplate());
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
		data->recstart = GetStart() - (uint64_t)data->prehandle  * 60000ULL;
		data->recstart = MAX(data->recstart, recstarttime);
	}
	if (!data->recstop) data->recstop = GetStop() + (uint64_t)data->posthandle * 60000ULL;
	if (!GetDVBChannel()[0]) {
		config.logit("Warning: '%s' has no DVB channel, using main channel!\n", GetQuickDescription().str());
	}

	filename = GenerateFilename();
	SetString(&data->strings.filename, filename.str());

	if (FilePatternExists(filename)) {
		AString filename1;
		uint_t  n = 1;

		do {
			filename1 = filename.Prefix() + AString(".%u.").Arg(n++) + filename.Suffix();
		}
		while (FilePatternExists(filename1));

		filename = filename1;

		SetString(&data->strings.filename, filename.str());
	}
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
		fp.printf("dvb --record $DVBPROG\n");

		AString str;
		str.printf("------------------------------------------------------------------------------------------------------------------------\n");
		str.printf("%s", GetDescription(~0).str());

		uint_t i, n;
		if ((n = str.CountLines("\n", 0)) > 0) {
			str.CutLine(n - 1, "\n", 0);
		}

		str.printf("\nCard %u, priority %d, Recording %s - %s\n",
				   GetDVBCard(), GetPri(),
				   GetRecordStartDT().UTCToLocal().DateFormat("%d %D-%N-%Y %h:%m:%s").str(),
				   GetRecordStopDT().UTCToLocal().DateFormat("%h:%m:%s").str());

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

void ADVBProg::AddToList(ADataList *list)
{
	this->list = list;
	list->Add((uptr_t)this);
}

void ADVBProg::SetPriorityScore()
{
	const ADVBConfig& config = ADVBConfig::Get();

	priority_score = config.GetPriorityScale() * data->pri;
	if (IsUrgent()) priority_score += config.GetUrgentScale();
	if (list) priority_score += (list->Count() - 1) * config.GetRepeatsScale();
}

void ADVBProg::CountOverlaps(const ADVBProgList& proglist)
{
	uint_t i;

	overlaps = 0;
	for (i = 0; i < proglist.Count(); i++) {
		const ADVBProg& prog = proglist.GetProg(i);

		if ((prog != *this) && !SameProgramme(prog, *this) && RecordOverlaps(prog)) {
			overlaps++;
		}
	}
}

int ADVBProg::SortListByOverlaps(uptr_t item1, uptr_t item2, void *context)
{
	const ADVBProg& prog1 	   = *(const ADVBProg *)item1;
	const ADVBProg& prog2 	   = *(const ADVBProg *)item2;
	const uint64_t& starttime  = *(const uint64_t *)context;
	uint_t          prog1after = (prog1.GetRecordStart() >= starttime);
	uint_t          prog2after = (prog2.GetRecordStart() >= starttime);
	bool			prog1urg   = prog1.IsUrgent();
	bool			prog2urg   = prog2.IsUrgent();
	int res = 0;
	
	if		( prog1urg && !prog2urg) 		  		  		   		  res = -1;
	else if (!prog1urg &&  prog2urg) 		  		  		   		  res =  1;
	else if	(prog1after > prog2after)								  res = -1;
	else if	(prog1after < prog2after)								  res =  1;
	else if	(prog1.GetPreRecordHandle() > prog2.GetPreRecordHandle()) res = -1;
	else if (prog1.GetPreRecordHandle() < prog2.GetPreRecordHandle()) res =  1;
	else if	(!prog1urg && (prog1.overlaps < prog2.overlaps))  		  res = -1;
	else if (!prog1urg && (prog1.overlaps > prog2.overlaps))  		  res =  1;
	else if	(prog1.GetStart() < prog2.GetStart()) 		   			  res = -1;
	else if (prog1.GetStart() > prog2.GetStart()) 		   			  res =  1;
	else if	(prog1urg && (prog1.overlaps  < prog2.overlaps))  		  res = -1;
	else if (prog1urg && (prog1.overlaps  > prog2.overlaps))  		  res =  1;
	else res = CompareNoCase(prog1.GetQuickDescription(), prog2.GetQuickDescription());
	
	return res;
}

int ADVBProg::CompareScore(const ADVBProg& prog1, const ADVBProg& prog2)
{
	int res = 0;

	if		(prog1.priority_score > prog2.priority_score) res = -1;
	else if	(prog1.priority_score < prog2.priority_score) res =  1;
	else if	(prog1.GetStart() 	  < prog2.GetStart()) 	  res = -1;
	else if (prog1.GetStart() 	  > prog2.GetStart()) 	  res =  1;
	else res = CompareNoCase(prog1.GetQuickDescription(), prog2.GetQuickDescription());

	return res;
}

AString ADVBProg::GetAdditionalLogFile() const
{
	const ADVBConfig& config = ADVBConfig::Get();
	return config.GetLogDir().CatPath(AString(GetFilename()).FilePart().Prefix() + ".txt");
}

AString ADVBProg::GetRecordPIDS(bool update) const
{
	ADVBChannelList& channellist = ADVBChannelList::Get();
	AString dvbchannel = GetDVBChannel();

	if (dvbchannel.Empty()) dvbchannel = GetChannel();
	
	return channellist.GetPIDList(dvbchannel, update);
}

AString ADVBProg::GenerateRecordCommand(uint_t nsecs, const AString& pids, AString& filename) const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString cmd;
	AString logfile;

	logfile.printf("2>>\"%s\"", config.GetLogFile().str());
			
	cmd.printf("dvbstream -c %u -n %u -f %s -o %s",
			   GetDVBCard(),
			   nsecs,
			   pids.str(),
			   logfile.str());

	AString proccmd = config.GetProcessingCommand(GetUser(), GetFilename());
	AString srcfile = GetSourceFile();
	CreateDirectory(srcfile.PathPart());
	if (proccmd.Valid()) {
		cmd.printf(" | tee \"%s\" %s | %s 2>&1 >>\"%s\"", srcfile.str(), logfile.str(), proccmd.str(), config.GetLogFile().str());
		filename = GetFilename();
	}
	else {
		cmd.printf(" >\"%s\"", srcfile.str());
		filename = srcfile;
	}
	
	return cmd;
}

void ADVBProg::Record()
{
	if (Valid()) {
		const ADVBConfig& config = ADVBConfig::Get();
		uint64_t dt, st, et;
		uint_t   nsecs, nmins;
		bool     record = true;

		SetRunning();
		
		config.printf("------------------------------------------------------------------------------------------------------------------------");
		config.printf("Starting record of '%s' as '%s'", GetQuickDescription().str(), GetFilename());

		AString pids;
		AString user = GetUser();
		bool    fake = (IsFakeRecording() ||
						(user.Valid() && ((uint_t)config.GetUserConfigItem(user, "fakerecordings") != 0)) ||
						((uint_t)config.GetConfigItem("fakerecordings") != 0));
		bool	reschedule = false;
		bool	failed     = false;
		
		if (record && !fake) {
			pids = GetRecordPIDS();
						
			if (pids.CountWords() < 2) {
				config.printf("No pids for '%s'", GetQuickDescription().str());
				reschedule = true;
				record = false;
			}
		}

		if (!GetJobID()) {
			ADVBLock     lock("schedule");
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
			dt 	  = (uint64_t)ADateTime().TimeStamp(true);

			st 	  = GetStart();
			nmins = (dt >= st) ? (uint_t)((dt - st) / 60000ULL) : 0;

			et 	  = GetRecordStop();
			nsecs = (et >= dt) ? (uint_t)((et - dt + 1000 - 1) / 1000ULL) : 0;

			if (nmins >= config.GetLatestStart()) {
				config.printf("'%s' started too long ago (%u minutes)!", GetTitleAndSubtitle().str(), nmins);
				reschedule = true;
				record = false;
			}
			else if (nsecs == 0) {
				config.printf("'%s' already finished!", GetTitleAndSubtitle().str());
				reschedule = true;
				record = false;
			}
		}

		if (record) {
			if (!AllowRepeats()) {
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

			CreateDirectory(filename.PathPart());

			if (fake) {
				config.printf("**Fake** recording '%s' for %u seconds (%u minutes) to '%s'",
							  GetTitleAndSubtitle().str(),
							  nsecs, ((nsecs + 59) / 60),
							  filename.str());
			}
			else {
				config.printf("Recording '%s' for %u seconds (%u minutes) to '%s' using freq %uHz PIDs %s",
							  GetTitleAndSubtitle().str(),
							  nsecs, ((nsecs + 59) / 60),
							  filename.str(),
							  (uint_t)pids.Word(0),
							  pids.Words(1).str());
			}

			cmd = GenerateRecordCommand(nsecs, pids, filename);

			data->actstart = ADateTime().TimeStamp(true);

			if (fake) {
				// create text file with necessary filename to hold information
				AStdFile fp;

				config.printf("Faking recording of '%s' using '%s'", GetTitleAndSubtitle().str(), filename.str());

				if (fp.open(filename, "w")) {
					fp.printf("Date: %s\n", ADateTime().DateFormat("%h:%m:%s.%S %D/%M/%Y").str());
					fp.printf("Command: %s\n", cmd.str());
					fp.printf("Filename: %s\n", filename.str());
					fp.close();

					res = 0;
				}
				else {
					config.printf("Unable to create '%s' (fake recording of '%s')", filename.str(), GetTitleAndSubtitle().str());
					res = -1;
				}
			}
			else {
				config.printf("Running '%s'", cmd.str());

				SetRecording();
				ADVBProgList::AddToList(config.GetRecordingFile(), *this);

				config.printf("--------------------------------------------------------------------------------");
				config.writetorecordlog("start %s", Base64Encode().str());
				res = system(cmd);
				config.writetorecordlog("stop %s", Base64Encode().str());
				config.printf("--------------------------------------------------------------------------------");

				ADVBProgList::RemoveFromList(config.GetRecordingFile(), *this);
				ClearRecording();
			}

			if (res == 0) {
				AString   str;
				FILE_INFO info;
				uint64_t  dt = (uint64_t)ADateTime().TimeStamp(true);
				uint64_t  st = GetStop();
				bool      addtorecorded = true;

				data->actstop = dt;

				SetRecordingComplete();

				if (dt < (st - 15000)) {
					config.printf("Warning: '%s' stopped %ss before programme end!", GetTitleAndSubtitle().str(), NUMSTR("", (st - dt) / 1000));
					reschedule = true;
					addtorecorded = fake;
				}

				if (!IsRecordingComplete()) {
					config.printf("Warning: '%s' is incomplete! (%ss missing from the start, %ss missing from the end)", GetTitleAndSubtitle().str(),
								  NUMSTR("", MAX((sint64_t)(data->actstart - MIN(data->start, data->recstart)), 0) / 1000),
								  NUMSTR("", MAX((sint64_t)(data->recstop  - data->actstop), 0) / 1000));
				}

				if (::GetFileInfo(filename, &info)) {
					config.printf("File '%s' exists and is %sMB, %u seconds = %skB/s", filename.str(), NUMSTR("", info.FileSize / (1024 * 1024)), nsecs, NUMSTR("", info.FileSize / (1024 * (uint64_t)nsecs)));

					SetFileSize(info.FileSize);

					if (addtorecorded && (info.FileSize > 0)) {
						config.printf("Adding '%s' to list of recorded programmes", GetTitleAndSubtitle().str());

						ClearScheduled();
						SetRecorded();

						{
							FlagsSaver saver(this);
							ClearRunning();
							ADVBProgList::AddToList(config.GetRecordedFile(), *this, false, false, true);
						}
						
						if (IsOnceOnly()) {
							ADVBPatterns::DeletePattern(user, GetPattern());

							reschedule = true;
						}

						SetPostProcessing();
						ADVBProgList::AddToList(config.GetProcessingFile(), *this, false, false, true);

						if (PostProcess()) OnRecordSuccess();
						else {
							failed = true;
							OnRecordFailure();
						}
						ADVBProgList::RemoveFromList(config.GetProcessingFile(), *this);
						ClearPostProcessing();
					}
					else if (!info.FileSize) {
						config.printf("Record of '%s' ('%s') is zero length", GetTitleAndSubtitle().str(), filename.str());
						remove(filename);
						failed = reschedule = true;
						OnRecordFailure();
					}
				}
				else {
					config.printf("Record of '%s' ('%s') doesn't exist", GetTitleAndSubtitle().str(), filename.str());
					failed = reschedule = true;
					OnRecordFailure();
				} 
			}
			else {
				config.printf("Unable to start record of '%s'", GetTitleAndSubtitle().str());
				failed = reschedule = true;
				OnRecordFailure();
			} 
		}

		if (failed) {
			ClearScheduled();
			SetRecordingFailed();
			
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
	}
}

AString ADVBProg::GetSourceFile() const
{
	const ADVBConfig& config = ADVBConfig::Get();
	return config.GetRecordingsStorageDir(GetUser()).CatPath(AString(GetFilename()).FilePart().Prefix() + ".mpg");
}

AString ADVBProg::ReplaceTerms(const AString& str) const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString date  = GetStartDT().UTCToLocal().DateFormat("%Y-%M-%D");
	AString times = GetStartDT().UTCToLocal().DateFormat("%h%m") + "-" + GetStopDT().UTCToLocal().DateFormat("%h%m");

	return (str
			.SearchAndReplace("{title}", GetTitle())
			.SearchAndReplace("{titleandsubtitle}", GetTitleAndSubtitle())
			.SearchAndReplace("{titlesubtitleandchannel}", GetTitleSubtitleAndChannel())
			.SearchAndReplace("{subtitle}", GetSubtitle())
			.SearchAndReplace("{episode}", GetEpisodeString())
			.SearchAndReplace("{channel}", GetChannel())
			.SearchAndReplace("{date}", date)
			.SearchAndReplace("{times}", times)
			.SearchAndReplace("{user}", GetUser())
			.SearchAndReplace("{srcfile}", GetSourceFile())
			.SearchAndReplace("{filename}", GetFilename())
			.SearchAndReplace("{logfile}", config.GetLogFile())
			.SearchAndReplace("{addlogfile}", GetAdditionalLogFile()));
}

bool ADVBProg::OnRecordSuccess() const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString user = GetUser();
	AString cmd, success = true;

	if ((cmd = config.GetUserConfigItem(user, "recordsuccesscmd")).Valid()) {
		cmd = ReplaceTerms(cmd);
		
		//config.printf("Running '%s' after successful record", cmd.str());
		if (system("nice " + cmd) != 0) {
			config.addlogit("\n");
			config.printf("Command '%s' failed!", cmd.str());
			success = false;
		}
	}

	return success;
}

bool ADVBProg::OnRecordFailure() const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString user = GetUser();
	AString cmd, success = true;

	if ((cmd = config.GetUserConfigItem(user, "recordfailurecmd")).Valid()) {
		cmd = ReplaceTerms(cmd);
		
		//config.printf("Running '%s' after failed record", cmd.str());
		if (system("nice " + cmd) != 0) {
			config.addlogit("\n");
			config.printf("Command '%s' failed!", cmd.str());
			success = false;
		}
	}

	return success;
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

bool ADVBProg::PostProcess()
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString postcmd;
	bool success = false;

	if ((postcmd = GeneratePostProcessCommand()).Valid()) {
		config.addlogit("\n");
		config.printf("Running post process command '%s'", postcmd.str());

		if (system("nice " + postcmd) == 0) {
			ADVBLock     lock("schedule");
			ADVBProgList recordedlist;
			AString  	 filename = config.GetRecordedFile();
			ADVBProg 	 *prog;

			recordedlist.ReadFromFile(filename);
			if ((prog = recordedlist.FindUUIDWritable(*this)) != NULL) {
				FILE_INFO info;
				
				if (::GetFileInfo(prog->GetFilename(), &info)) {
					uint_t nsecs = (uint_t)(prog->GetActualLength() / 1000);
					
					config.printf("File '%s' exists and is %sMB, %u seconds = %skB/s", prog->GetFilename(), NUMSTR("", info.FileSize / (1024 * 1024)), nsecs, NUMSTR("", info.FileSize / (1024 * (uint64_t)nsecs)));

					prog->SetFileSize(info.FileSize);
				}
				else config.printf("File '%s' DOESN'T exists!", prog->GetFilename());
				
				prog->SetPostProcessed();
				recordedlist.WriteToFile(filename);
			}

			success = true;
		}
		else {
			config.addlogit("\n");
			config.printf("Command '%s' failed!", postcmd.str());
		}
	}
	else success = true;

	return success;
}

void ADVBProg::Record(const AString& channel, uint_t mins)
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBChannelList& channellist = ADVBChannelList::Get();
	AString pids;

	config.printf("------------------------------------------------------------------------------------------------------------------------");
	config.printf("Starting record on channel '%s'", channel.str());

	pids = channellist.GetPIDList(channel, true);

	if (pids.CountWords() >= 2) {
		AString filename;
		AString cmd;

		config.printf("Recording freq %sHz pids %s", pids.Word(0).str(), pids.Words(1).str());

		filename.printf("dvbrec.%s.mpg", ADateTime().DateFormat("%Y-%M-%D.%h-%m-%s").str());

		cmd.printf("dvbstream");

		if (mins) cmd.printf(" -n %u", mins * 60);

		cmd.printf(" -f %s -o >\"%s\" 2>>%s",
				   pids.str(),
				   filename.str(),
				   config.GetLogFile(ADateTime().GetDays()).str());

		int res;
		if ((res = system(cmd)) == 0) {
			config.printf("Recording finished");
		}
		else config.logit("Recording failed, return code %d", res);
	}
	else {
		config.printf("No pids for channel '%s'", channel.str());
	}

	config.printf("------------------------------------------------------------------------------------------------------------------------");
}

AString ADVBProg::GetProcessingCommands()
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString oldaddlogfile = config.GetAdditionalLogFile();
	AString addlogfile    = GetAdditionalLogFile();
	AString cmd, filename;
	
	GenerateRecordData(ADateTime().TimeStamp(true));

	((ADVBConfig&)config).SetAdditionalLogFile(addlogfile, false);

	cmd.printf("%s\n", GenerateRecordCommand(200, GetRecordPIDS(false), filename).SearchAndReplace(" | ", " |\n").str());
	AString postcmd;
	if ((postcmd = GeneratePostProcessCommand()).Valid()) cmd.printf("Post: %s\n", postcmd.str());
	cmd.printf("\n");

	((ADVBConfig&)config).SetAdditionalLogFile(oldaddlogfile);

	return cmd;
}
