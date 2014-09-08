
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <rdlib/Regex.h>
#include <rdlib/Recurse.h>

#include "config.h"
#include "dvbprog.h"
#include "proglist.h"
#include "channellist.h"
#include "dvblock.h"

#define DEBUG_SAMEPROGRAMME 0

int  ADVBProg::priscale     = 2;
int  ADVBProg::repeatsscale = -1;
bool ADVBProg::debugsameprogramme = false;

AHash ADVBProg::fieldhash;
const ADVBProg::DVBPROG *ADVBProg::nullprog = (const ADVBProg::DVBPROG *)NULL;

#define DVBPROG_OFFSET(x) ((uint16_t)(uptr_t)&nullprog->x)
#define DEFINE_FIELD(name,var,type,desc)	{#name, FieldType_##type, false, DVBPROG_OFFSET(var), desc}
#define DEFINE_SIMPLE(name,type,desc)		DEFINE_FIELD(name, name, type, desc)
#define DEFINE_STRING(name,desc)			DEFINE_FIELD(name, strings.name, string, desc)
#define DEFINE_FLAG(name,flag,desc)			{#name, FieldType_flag + flag, false, DVBPROG_OFFSET(flags), desc}
#define DEFINE_FLAG_ASSIGN(name,flag,desc)	{#name, FieldType_flag + flag, true, DVBPROG_OFFSET(flags), desc}
#define DEFINE_ASSIGN(name,var,type,desc)	{#name, FieldType_##type, true, DVBPROG_OFFSET(var), desc}
const ADVBProg::FIELD ADVBProg::fields[] = {
	{"prog", FieldType_prog, false, 0, "Encoded Programme"},

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

	DEFINE_SIMPLE(year, uint16_t, "Year"),

	DEFINE_FIELD(card, dvbcard, uint8_t, "DVB card"),

	DEFINE_FLAG(repeat,    		Flag_repeat,    	 "Programme is a repeat"),
	DEFINE_FLAG(plus1,     		Flag_plus1,     	 "Programme is on +1"),
	DEFINE_FLAG(rejected,  		Flag_rejected,  	 "Programme rejected"),
	DEFINE_FLAG(recorded,  		Flag_recorded,  	 "Programme recorded"),
	DEFINE_FLAG(scheduled, 		Flag_scheduled, 	 "Programme scheduled"),
	DEFINE_FLAG(radioprogramme, Flag_radioprogramme, "Programme is a Radio programme"),

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
};

ADVBProg::OPERATOR ADVBProg::operators[] = {
    {"!=", 	false, FieldTypes_Default | FieldTypes_Prog, Operator_NE, "Is not equal to", 0},
    {"<>", 	false, FieldTypes_Default | FieldTypes_Prog, Operator_NE, "Is not equal to", 0},
    {"<=", 	false, FieldTypes_Default,                   Operator_LE, "Is less than or equal to", 0},
    {">=", 	false, FieldTypes_Default,                   Operator_GE, "Is greater than or equal to", 0},
    {"=",  	false, FieldTypes_Default | FieldTypes_Prog, Operator_EQ, "Equals", 0},
    {"<",  	false, FieldTypes_Default,                   Operator_LT, "Is less than", 0},
    {">",  	false, FieldTypes_Default,                   Operator_GT, "Is greater than", 0},

    {"!@<", false, FieldTypes_String,                    Operator_NotStartsWith, "Does not start with", 0},
    {"!@>", false, FieldTypes_String,                    Operator_NotEndsWith, "Does not end with", 0},
    {"!%<", false, FieldTypes_String,                    Operator_NotStarts, "Does not start", 0},
    {"!%>", false, FieldTypes_String,                    Operator_NotEnds, "Does not end", 0},
    {"!=", 	false, FieldTypes_String,                    Operator_NE, "Is not equal to", 0},
    {"!~", 	false, FieldTypes_String,                    Operator_NotRegex, "Does not match regex", 0},
    {"!@", 	false, FieldTypes_String,                    Operator_NotContains, "Does not contain", 0},
    {"!%",  false, FieldTypes_String,                    Operator_NotWithin, "Is not within", 0},
    {"<>", 	false, FieldTypes_String,                    Operator_NE, "Is not equal to", 0},
    {"<=", 	false, FieldTypes_String,                    Operator_LE, "Is less than or equal to", 0},
    {">=", 	false, FieldTypes_String,                    Operator_GE, "Is greater than or equal to", 0},
    {"@<", 	false, FieldTypes_String,                    Operator_StartsWith, "Starts with", 0},
    {"@>", 	false, FieldTypes_String,                    Operator_EndsWith, "Ends with", 0},
    {"%<", 	false, FieldTypes_String,                    Operator_Starts, "Starts", 0},
    {"%>", 	false, FieldTypes_String,                    Operator_Ends, "Ends", 0},
    {"=",  	false, FieldTypes_String,                    Operator_EQ, "Equals", 0},
    {"~",  	false, FieldTypes_String,                    Operator_Regex, "Matches regex", 0},
    {"@",  	false, FieldTypes_String,                    Operator_Contains, "Contains", 0},
    {"%",  	false, FieldTypes_String,                    Operator_Within, "Is within", 0},
    {"<",  	false, FieldTypes_String,                    Operator_LT, "Is less than", 0},
    {">",  	false, FieldTypes_String,                    Operator_GT, "Is greater than", 0},

    {"+=", 	true,  FieldTypes_String,                    Operator_Concat, "Is concatenated with", 0},
    {":=", 	true,  FieldTypes_String,                    Operator_Assign, "Is set to", 0},
    {":=", 	true,  FieldTypes_Default,                   Operator_Assign, "Is set to", 0},

    {"+=", 	true,  FieldTypes_Number,                    Operator_Add, "Is incremented by", 0},
    {"-=", 	true,  FieldTypes_Number,                    Operator_Subtract, "Is decremented by", 0},
    {"*=", 	true,  FieldTypes_Number,                    Operator_Multiply, "Is multiplied by", 0},
    {"/=", 	true,  FieldTypes_Number,                    Operator_Divide, "Is divided by", 0},
    {":>", 	true,  FieldTypes_Number,                    Operator_Maximum, "Is maximized with", 0},
    {":<", 	true,  FieldTypes_Number,                    Operator_Minimum, "Is minimized with", 0},

	//{"+=", 	false, FieldTypes_String,  	 			 	 Operator_Concat},
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

	if (!operators[0].len) {
		uint_t i;

		for (i = 0; i < NUMBEROF(operators); i++) {
			operators[i].len = strlen(operators[i].str);
		}
	}
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

void ADVBProg::GetFieldValue(const FIELD& field, VALUE& value, AString& val)
{
	switch (field.type) {
		case FieldType_string:
			value.str = val.Steal();
			break;

		case FieldType_date:
			value.u64 = (uint64_t)ADateTime().StrToDate(val);
			break;

		case FieldType_uint32_t:
			value.u32 = (uint32_t)val;
			break;

		case FieldType_sint32_t:
			value.s32 = (sint32_t)val;
			break;

		case FieldType_uint16_t:
			value.u16 = (uint16_t)val;
			break;

		case FieldType_sint16_t:
			value.s16 = (sint16_t)val;
			break;

		case FieldType_uint8_t:
			value.u8 = (uint8_t)(uint16_t)val;
			break;

		case FieldType_sint8_t:
			value.s8 = (sint8_t)(sint16_t)val;
			break;

		case FieldType_flag...FieldType_lastflag:
			value.u8 = ((uint32_t)val != 0);
			break;
	}
}

bool ADVBProg::SetUUID()
{
	AString uuid;

	uuid.printf("%s:%s:%s", GetStartDT().DateFormat("%Y%M%D%h%m").str(), GetChannel(), GetTitle());

	return SetString(&data->strings.uuid, uuid);
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
	if (channel.StartsWith(tsod)) {
		SetString(&data->strings.basechannel, channel.Mid(tsod.len()));
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

AString ADVBProg::JSONFormat(const AString& str)
{
	return str.SearchAndReplace("/", "\\/").SearchAndReplace("\"", "\\\"").SearchAndReplace("\n", "\\n");
}

AString ADVBProg::JSONDateFormat(uint64_t dt, const AString& type)
{
	// calculate number of milliseconds between midnight 1-jan-1970 to midnight 1-jan-1980 (1972 and 1976 were leap years)
	static const uint64_t offset = (10ULL * 365ULL + 2) * 24ULL * 3600ULL * 1000ULL;
	ADateTime dto = ADateTime(dt).UTCToLocal();
	AString   str;

	str.printf("\"%s\":\"%s\"",             type.str(), dto.ToTimeStamp(ADateTime::TIMESTAMP_FORMAT_FULL).str());
	str.printf(",\"%soffset\":%" FMT64 "u", type.str(), dt + offset);
	str.printf(",\"%stime\":\"%s\"",     	type.str(), dto.DateFormat("%h:%m").str());
	str.printf(",\"%sdate\":\"%s\"",     	type.str(), dto.DateFormat("%d %D-%N-%Y").str());

	return str;
}

AString ADVBProg::JSONStartStopFormat(uint64_t start, uint64_t stop, const AString& type)
{
	AString str;

	str.printf("%s",  JSONDateFormat(start, type + "start").str());
	str.printf(",%s", JSONDateFormat(stop,  type + "stop").str());
	str.printf(",\"%slength\":%" FMT64 "u", type.str(), (stop - start) / 1000);

	return str;
}

AString ADVBProg::ExportToJSON(bool includebase64) const
{
	AString str;
	const char *p;

	str.printf("%s", JSONStartStopFormat(data->start, data->stop).str());
	if (data->recstart || data->recstop) {
		str.printf(",%s", JSONStartStopFormat(data->recstart, data->recstop, "rec").str());
	}
	if (data->actstart || data->actstop) {
		str.printf(",%s", JSONStartStopFormat(data->actstart, data->actstop, "act").str());
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
	str.printf(",\"repeat\":%u", (uint_t)IsRepeat());
	str.printf(",\"plus1\":%u", (uint_t)IsPlus1());
	str.printf(",\"urgent\":%u", (uint_t)IsUrgent());
	str.printf(",\"manual\":%u", (uint_t)IsManualRecording());
	str.printf(",\"markonly\":%u", (uint_t)IsMarkOnly());
	str.printf(",\"postprocessed\":%u", (uint_t)IsPostProcessed());
	str.printf(",\"onceonly\":%u", (uint_t)IsOnceOnly());
	str.printf(",\"rejected\":%u", (uint_t)IsRejected());
	str.printf(",\"recorded\":%u", (uint_t)IsRecorded());
	str.printf(",\"running\":%u", (uint_t)IsRunning());
	str.printf(",\"scheduled\":%u", (uint_t)IsScheduled());
	str.printf(",\"radioprogramme\":%u", (uint_t)IsRadioProgramme());
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

		if (data->actstart && data->actstop && GetString(data->strings.filename)[0]) {
			str.printf(",\"exists\":%u", (uint_t)AStdFile::exists(GetString(data->strings.filename)));
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

void ADVBProg::SetFlag(uint8_t flag, bool val)
{
	if (val) data->flags |=  (1UL << flag);
	else	 data->flags &= ~(1UL << flag);
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
		if (ep.series)   res.printf("S%02u", ep.series);
		if (ep.episode)  res.printf("E%02u", ep.episode);
		if (ep.episodes) res.printf("T%02u", ep.episodes);
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

	if (IsRunning()) str.printf(" -*RUNNING*-");

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
			str1.printf("Set to record %s - %s (%u mins)",
						GetRecordStartDT().UTCToLocal().DateFormat("%d %D-%N-%Y %h:%m").str(),
						GetRecordStopDT().UTCToLocal().DateFormat("%h:%m").str(),
						(uint_t)((GetRecordLength() + 60000 - 1) / 60000ULL));

			if (GetUser()[0]) str1.printf(" by user '%s'", GetUser());

			if (data->actstart) {
				str1.printf(", actual record time %s - %s (%u mins)",
							GetActualStartDT().UTCToLocal().DateFormat("%d %D-%N-%Y %h:%m").str(),
							GetActualStopDT().UTCToLocal().DateFormat("%h:%m").str(),
							(uint_t)((GetRecordLength() + 60000 - 1) / 60000ULL));
			}
		}

		if ((verbosity > 4) && GetFilename()[0]) {
			if (str1.Valid()) str1.printf("\n\n");
			str1.printf("%s as '%s'", data->actstart ? "Recorded" : (data->recstart ? "To be recorded" : "Would be recorded"), GetFilename());
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
		else if (prog1.data->assignedepisode && prog2.data->assignedepisode) {
			// assigned episode information valid in both
			// -> use episode to determine whether it's the same programme
			same = (prog1.data->assignedepisode == prog2.data->assignedepisode);
#if DEBUG_SAMEPROGRAMME
			if (debugsameprogramme) debug("'%s' / '%s': assigned episode %u / %u: %s\n", prog1.GetDescription().str(), prog2.GetDescription().str(), prog1.data->assignedepisode, prog2.data->assignedepisode, same ? "same" : "different");
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
		else if (IsWhiteSpace(c)) updater.Update('_');
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

AString ADVBProg::GenerateFilename() const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString dir      = ADVBConfig::CatPath(config.GetRecordingsDir(), GetDir());
	AString templ    = ADVBConfig::CatPath(dir, config.GetFilenameTemplate());
	AString date     = GetStartDT().UTCToLocal().DateFormat("%Y-%M-%D");
	AString times    = GetStartDT().UTCToLocal().DateFormat("%h%m") + "-" + GetStopDT().UTCToLocal().DateFormat("%h%m");
	AString filename = (templ.SearchAndReplace("{title}", ValidFilename(GetTitle()))
						.SearchAndReplace("{subtitle}", ValidFilename(GetSubtitle()))
						.SearchAndReplace("{episode}", ValidFilename(GetEpisodeString()))
						.SearchAndReplace("{channel}", ValidFilename(GetChannel()))
						.SearchAndReplace("{date}", ValidFilename(date))
						.SearchAndReplace("{times}", ValidFilename(times))
						.SearchAndReplace("{user}", ValidFilename(GetUser())));
		
	while (filename.Pos("{sep}{sep}") >= 0) {
		filename = filename.SearchAndReplace("{sep}{sep}", "{sep}");
	}
	
	filename = filename.SearchAndReplace("{sep}", ".");

	return filename;
}

void ADVBProg::GenerateRecordData()
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString filename;

	if (!data->recstart) data->recstart = GetStart() - (uint64_t)data->prehandle  * 60ULL * 1000ULL;
	if (!data->recstop)  data->recstop  = GetStop()  + (uint64_t)data->posthandle * 60ULL * 1000ULL;
	if (!GetDVBChannel()[0]) {
		config.logit("Warning: '%s' has no DVB channel, using main channel!\n", GetQuickDescription().str());
	}

	if ((filename = GetFilename()).Empty()) {
		filename = GenerateFilename();

		SetString(&data->strings.filename, filename.str());
	}

	if (AStdFile::exists(filename)) {
		AString filename1;
		uint_t  n = 1;

		do {
			filename1 = filename.Prefix() + AString(".%u.").Arg(n++) + filename.Suffix();
		}
		while (AStdFile::exists(filename1));

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
	priority_score = priscale * data->pri;
	if (list) priority_score += (list->Count() - 1) * repeatsscale;
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
	const ADVBProg& prog1 = *(const ADVBProg *)item1;
	const ADVBProg& prog2 = *(const ADVBProg *)item2;
	int res = 0;

	UNUSED(context);

	if		(prog1.overlaps   < prog2.overlaps)   res = -1;
	else if (prog1.overlaps   > prog2.overlaps)   res =  1;
	else if	(prog1.GetStart() < prog2.GetStart()) res = -1;
	else if (prog1.GetStart() > prog2.GetStart()) res =  1;
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

int ADVBProg::SortTermsByAssign(uptr_t item1, uptr_t item2, void *context)
{
	const TERM *term1 = (const TERM *)item1;
	const TERM *term2 = (const TERM *)item2;
	bool assign1 = RANGE(term1->data.opcode, Operator_First_Assignable, Operator_Last_Assignable);
	bool assign2 = RANGE(term2->data.opcode, Operator_First_Assignable, Operator_Last_Assignable);

	UNUSED(context);

	if ( assign1 && !assign2) return  1;
	if (!assign1 &&  assign2) return -1;
	if ( assign1 &&  assign2) return COMPARE_ITEMS(term1->data.field, term2->data.field);

	return 0;
}

void ADVBProg::__DeleteTerm(uptr_t item, void *context)
{
	TERM *term = (TERM *)item;

	UNUSED(context);

	if (term->field->type == FieldType_string) {
		if (term->value.str) delete[] term->value.str;
	}
	else if (term->field->type == FieldType_prog) {
		if (term->value.prog) delete term->value.prog;
	}

	delete term;
}

const ADVBProg::OPERATOR *ADVBProg::FindOperator(const PATTERN& pattern, uint_t term)
{
	const TERM *pterm = (const TERM *)pattern.list[term];

	if (pterm) {
		uint_t  fieldtype = 1U << pterm->field->type;
		uint8_t oper      = pterm->data.opcode;
		uint_t  i;

		for (i = 0; ((i < NUMBEROF(operators)) &&
					 !((oper == operators[i].opcode) &&
					   (operators[i].fieldtypes & fieldtype))); i++) ;

		return (i < NUMBEROF(operators)) ? operators + i : NULL;
	}

	return NULL;
}

const char *ADVBProg::GetOperatorText(const PATTERN& pattern, uint_t term)
{
	const OPERATOR *oper = FindOperator(pattern, term);

	return oper ? oper->str : NULL;
}

const char *ADVBProg::GetOperatorDescription(const PATTERN& pattern, uint_t term)
{
	const OPERATOR *oper = FindOperator(pattern, term);

	return oper ? oper->desc : NULL;
}

bool ADVBProg::OperatorIsAssign(const PATTERN& pattern, uint_t term)
{
	const TERM *pterm = (const TERM *)pattern.list[term];

	return pterm ? RANGE(pterm->data.opcode, Operator_First_Assignable, Operator_Last_Assignable) : false;
}

AString ADVBProg::GetPatternDefinitionsJSON()
{
	AString str;
	uint_t i, j;

	str.printf("\"patterndefs\":{\"fields\":[");
	
	for (i = 0; i < NUMBEROF(fields); i++) {
		const FIELD& field = fields[i];

		if (i) str.printf(",");
		str.printf("{\"name\":\"%s\"", 	 JSONFormat(field.name).str());
		str.printf(",\"desc\":\"%s\"", 	 JSONFormat(field.desc).str());
		str.printf(",\"type\":%u",     	 field.type);
		str.printf(",\"assignable\":%s", field.assignable ? "true" : "false");
		str.printf(",\"operators\":[");

		bool flag = false;
		for (j = 0; j < NUMBEROF(operators); j++) {
			const OPERATOR& oper = operators[j];

			if ((field.assignable == oper.assign) &&
				(oper.fieldtypes & (1U << field.type))) {
				if (flag) str.printf(",");
				str.printf("%u", j);
				flag = true;
			}
		}

		str.printf("]}");
	}

	str.printf("],\"operators\":[");

	for (j = 0; j < NUMBEROF(operators); j++) {
		const OPERATOR& oper = operators[j];

		if (j) str.printf(",");
		str.printf("{\"text\":\"%s\"", JSONFormat(oper.str).str());
		str.printf(",\"desc\":\"%s\"", JSONFormat(oper.desc).str());
		str.printf(",\"opcode\":%u",   (uint_t)oper.opcode);
		str.printf(",\"assign\":%s}",  oper.assign ? "true" : "false");
	}

	str.printf("],\"orflags\":[");

	for (j = 0; j < 2; j++) {
		if (j) str.printf(",");
		str.printf("{\"text\":\"%s\"", JSONFormat(j ? "or" : "and").str());
		str.printf(",\"desc\":\"%s\"", JSONFormat(j ? "Or the next term" : "And the next term").str());
		str.printf(",\"value\":%u}",   j);
	}

	str.printf("]}");

	return str;
}

uint_t ADVBProg::ParsePatterns(ADataList& patternlist, const AString& patterns, AString& errors, const AString& sep, const AString& user)
{
	uint_t i, n = patterns.CountLines(sep);

	for (i = 0; i < n; i++) {
		ParsePattern(patternlist, patterns.Line(i, sep), errors, user);
	}

	return patternlist.Count();
}

bool ADVBProg::ParsePattern(ADataList& patternlist, const AString& line, AString& errors, const AString& user)
{
	//const ADVBConfig& config = ADVBConfig::Get();
	PATTERN *pattern;
	bool    success = false;

	StaticInit();

	patternlist.SetDestructor(&DeletePattern);

	if ((pattern = new PATTERN) != NULL) {
		AString errs = ParsePattern(line, *pattern, user);

		if (errs.Valid()) {
			//config.printf("Error parsing '%s': %s", line.str(), errs.str());
			errors += errs + "\n";
		}

		if ((pattern->list.Count() > 0) || pattern->errors.Valid()) {
			patternlist.Add((uptr_t)pattern);
			success = true;
		}
	}

	return success;
}

AString ADVBProg::ParsePattern(const AString& line, PATTERN& pattern, const AString& user)
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADataList& list   = pattern.list;
	AString&   errors = pattern.errors;
	TERM   *term;
	uint_t i;

	pattern.exclude    = false;
	pattern.enabled    = true;
	pattern.scorebased = false;
	pattern.pri        = 0;
	pattern.user       = user;
	pattern.pattern    = line;

	if (pattern.user.Valid()) {
		pattern.pri = (int)config.GetUserConfigItem(pattern.user, "pri");
	}

	list.DeleteList();
	list.SetDestructor(&__DeleteTerm);

	i = 0;
	while (IsWhiteSpace(line[i])) i++;
	if (line[i] == '#') {
		pattern.enabled = false;
		i++;
	}
	else if (line[i] == ';') {
		return errors;
	}

	while (IsWhiteSpace(line[i])) i++;

	if (line[i]) {
		while (line[i] && errors.Empty()) {
			if (!IsSymbolStart(line[i])) {
				errors.printf("Character '%c' (at %u) is not a legal field start character (term %u)", line[i], i, list.Count() + 1);
				break;
			}

			uint_t fs = i++;
			while (IsSymbolChar(line[i])) i++;
			AString field = line.Mid(fs, i - fs).ToLower();

			while (IsWhiteSpace(line[i])) i++;

			if (field == "exclude") {
				pattern.exclude = true;
				continue;
			}

			const FIELD *fieldptr = (const FIELD *)fieldhash.Read(field);
			if (!fieldptr) {
				errors.printf("'%s' (at %u) is not a valid search field (term %u), valid search fields are: ", field.str(), fs, list.Count() + 1);
				for (i = 0; i < NUMBEROF(fields); i++) {
					if (i) errors.printf(", ");
					errors.printf("'%s'", fields[i].name);
				}
				break;
			}

			fs = i;

			const char *str = line.str() + i;
			bool isassign = fieldptr->assignable;
			uint_t j;
			uint_t opindex = 0, opcode = Operator_EQ;
			for (j = 0; j < NUMBEROF(operators); j++) {
				if (((isassign == operators[j].assign) ||
					 (isassign && !operators[j].assign)) &&
					(operators[j].fieldtypes & (1U << fieldptr->type)) &&
					(strncmp(str, operators[j].str, operators[j].len) == 0)) {
					i      += operators[j].len;
					opindex = j;
					opcode  = operators[j].opcode;
					break;
				}
			}

			while (IsWhiteSpace(line[i])) i++;

			AString value;
			bool    implicitvalue = false;
			if (j == NUMBEROF(operators)) {
				if (!line[i] || IsSymbolStart(line[i])) {
					if (fieldptr->assignable) {
						switch (fieldptr->type) {
							case FieldType_string:
								break;

							case FieldType_date:
								value = "now";
								break;

							default:
								value = "1";
								break;
						}

						opcode = Operator_Assign;
					}
					else opcode = Operator_NE;

					for (j = 0; j < NUMBEROF(operators); j++) {
						if ((opcode == operators[j].opcode) &&
							((isassign == operators[j].assign) ||
							 (isassign && !operators[j].assign)) &&
							(operators[j].fieldtypes & (1U << fieldptr->type))) {
							opindex = j;
							break;
						}
					}

					implicitvalue = true;
				}
				else {
					errors.printf("Symbols at %u do not represent a legal operator (term %u), legal operators for the field '%s' are: ", fs, list.Count() + 1, field.str());

					bool flag = false;
					for (j = 0; j < NUMBEROF(operators); j++) {
						if (((isassign == operators[j].assign) ||
							 (isassign && !operators[j].assign)) &&
							(operators[j].fieldtypes & (1U << fieldptr->type))) {
							if (flag) errors.printf(", ");
							errors.printf("'%s'", operators[j].str);
							flag = true;
						}
					}
					break;
				}
			}

			if (!implicitvalue) {		
				char quote = 0;
				if (IsQuoteChar(line[i])) quote = line[i++];

				uint_t vs = i;
				while (line[i] && ((!quote && !IsWhiteSpace(line[i])) || (quote && (line[i] != quote)))) {
					if (line[i] == '\\') i++;
					i++;
				}

				value = line.Mid(vs, i - vs).DeEscapify();

				if (quote && (line[i] == quote)) i++;

				while (IsWhiteSpace(line[i])) i++;
			}

			bool orflag = false;
			if ((line.Mid(i, 2).ToLower() == "or") && IsWhiteSpace(line[i + 2])) {
				orflag = true;
				i += 2;

				while (IsWhiteSpace(line[i])) i++;
			}
			else if ((line[i] == '|') && IsWhiteSpace(line[i + 1])) {
				orflag = true;
				i += 1;

				while (IsWhiteSpace(line[i])) i++;
			}

			if ((term = new TERM) != NULL) {
				term->data.field   = fieldptr - fields;
				term->data.opcode  = opcode;
				term->data.opindex = opindex;
				term->data.value   = value;
				term->data.orflag  = (orflag && !RANGE(opcode, Operator_First_Assignable, Operator_Last_Assignable));
				term->field    	   = fieldptr;
				term->datetype 	   = DateType_none;

				switch (term->field->type) {
					case FieldType_string:
						if ((opcode & ~Operator_Inverted) == Operator_Regex) {
							AString regexerrors;
							AString rvalue;

							rvalue = ParseRegex(value, regexerrors);
							if (regexerrors.Valid()) {
								errors.printf("Regex error in value '%s' (term %u): %s", value.str(), list.Count() + 1, regexerrors.str());
							}

							value = rvalue;
						}
						term->value.str = value.Steal();
						break;

					case FieldType_date: {
						ADateTime dt;
						uint_t specified;

						if (dt.FromTimeStamp(value)) {
							specified = ADateTime::Specified_Date | ADateTime::Specified_Time;
						}
						else dt.StrToDate(value, false, &specified);

						//debug("Value '%s', specified %u\n", value.str(), specified);

						if (!specified) {
							errors.printf("Failed to parse date '%s' (term %u)", value.str(), list.Count() + 1);
							break;
						}
						else if (((specified == ADateTime::Specified_Day) && (stricmp(term->field->name, "on") == 0)) ||
								 (stricmp(term->field->name, "day") == 0)) {
							//debug("Date from '%s' is '%s' (week day only)\n", value.str(), dt.DateToStr().str());
							term->value.u64 = dt.GetWeekDay();
							term->datetype  = DateType_weekday;
						}
						else if (specified & ADateTime::Specified_Day) {
							specified |= ADateTime::Specified_Date;
						}

						if (term->datetype == DateType_none) {
							specified &= ADateTime::Specified_Date | ADateTime::Specified_Time;

							if (specified == (ADateTime::Specified_Date | ADateTime::Specified_Time)) {
								//debug("Date from '%s' is '%s' (full date and time)\n", value.str(), dt.DateToStr().str());
								term->value.u64 = (uint64_t)dt;
								term->datetype  = DateType_fulldate;
							}
							else if (specified == ADateTime::Specified_Date) {
								//debug("Date from '%s' is '%s' (date only)\n", value.str(), dt.DateToStr().str());
								term->value.u64 = dt.GetDays();
								term->datetype  = DateType_date;
							}
							else if (specified == ADateTime::Specified_Time) {
								//debug("Date from '%s' is '%s' (time only)\n", value.str(), dt.DateToStr().str());
								term->value.u64 = dt.GetMS();
								term->datetype  = DateType_time;
							}
							else {
								errors.printf("Unknown date specifier '%s' (term %u)", value.str(), list.Count() + 1);
							}
						}
						break;
					}

					case FieldType_uint32_t:
						term->value.u32 = (uint32_t)value;
						break;

					case FieldType_sint32_t:
						term->value.s32 = (sint32_t)value;
						break;

					case FieldType_uint16_t:
						term->value.u16 = (uint16_t)value;
						break;

					case FieldType_sint16_t:
						term->value.s16 = (sint16_t)value;
						break;

					case FieldType_uint8_t:
						term->value.u8 = (uint8_t)(uint16_t)value;
						break;

					case FieldType_sint8_t:
						term->value.s8 = (sint8_t)(sint16_t)value;
						break;

					case FieldType_flag...FieldType_lastflag:
						term->value.u8 = ((uint32_t)value != 0);
						//debug("Setting test of flag to %u\n", (uint_t)term->value.u8);
						break;

					case FieldType_prog: {
						ADVBProg *prog;

						if ((prog = new ADVBProg) != NULL) {
							if (prog->Base64Decode(value)) {
								term->value.prog = prog;
							}
							else {
								errors.printf("Failed to decode base64 programme ('%s') for %s at %u (term %u)", value.str(), field.str(), fs, list.Count() + 1);
								delete prog;
							}
						}
						else {
							errors.printf("Failed to allocate memory for base64 programme ('%s') for %s at %u (term %u)", value.str(), field.str(), fs, list.Count() + 1);
						}
						break;
					}

					default:
						errors.printf("Unknown field '%s' type (%u) (term %u)", field.str(), (uint_t)term->field->type, list.Count() + 1);
						break;
				}

				//debug("term: field {name '%s', type %u, assignable %u, offset %u} type %u dateflags %u value '%s'\n", term->field->name, (uint_t)term->field->type, (uint_t)term->field->assignable, term->field->offset, (uint_t)term->data.opcode, (uint_t)term->dateflags, term->value.str);

				if (errors.Empty()) {
					pattern.scorebased |= (term->field->offset == DVBPROG_OFFSET(score));
					list.Add((uptr_t)term);
				}
				else {
					__DeleteTerm((uptr_t)term, NULL);
					break;
				}
			}
		}
	}
	else errors.printf("No filter terms specified!");

	if (errors.Valid()) {
		AString puretext, conditions;
		int p = line.len();

		for (i = 0; i < NUMBEROF(operators); i++) {
			int p1;

			if ((p1 = line.PosNoCase(operators[i].str)) >= 0) {
				p = MIN(p, p1);
			}
		}

		if (p < line.len()) {
			// move back over whitespace
			while ((p > 0) && IsWhiteSpace(line[p - 1])) p--;
			// move back over non-whitespace
			while ((p > 0) && !IsWhiteSpace(line[p - 1])) p--;
			// move back over whitespace again
			while ((p > 0) && IsWhiteSpace(line[p - 1])) p--;

			puretext   = line.Left(p);
			conditions = line.Mid(p);
		}
		else puretext  = line;
	
		if (puretext.Valid()) {
			for (i = 0; puretext[i] && (IsAlphaChar(puretext[i]) || IsNumeralChar(puretext[i]) || (puretext[i] == ' ') || (puretext[i] == '-')); i++) ;

			if (i == (uint_t)puretext.len()) {
				AString newtext = "@\"" + puretext + "\"";
				AString newline = ("title" + newtext +
								   " or subtitle" + newtext +
								   " or desc" + newtext +
								   " or actor" + newtext +
								   " or director" + newtext +
								   " or category" + newtext +
								   conditions);

				//debug("Split '%s' into:\npure text:'%s'\nconditions:'%s'\nnew pattern:'%s'\n", line.str(), puretext.str(), conditions.str(), newline.str());
			
				pattern.errors.Delete();
				errors = ParsePattern(newline, pattern, user);
			}
		}
	}

	if (errors.Valid()) pattern.enabled = false;

	if (errors.Empty()) {
		list.Sort(&SortTermsByAssign);

		for (i = 0; i < list.Count(); i++) {
			const TERM *term = (const TERM *)list[i];

			if ((term->data.opcode == Operator_Assign) && (term->field->offset == DVBPROG_OFFSET(strings.user))) {
				if (pattern.user.Empty()) {
					pattern.user = term->value.str;
					if (pattern.user.Valid()) {
						pattern.pri = (int)config.GetUserConfigItem(pattern.user, "pri");
					}
				}
				else {
					errors.printf("Cannot specify user when user has already been specified (term %u)", i + 1);
				}
			}
		}
	}

	if (errors.Empty()) {
		for (i = 0; i < list.Count(); i++) {
			const TERM *term = (const TERM *)list[i];

			if ((term->data.opcode == Operator_Assign) && (term->field->offset == DVBPROG_OFFSET(pri))) {
				pattern.pri = term->value.s8;
			}
		}
	}

	return errors;
}

bool ADVBProg::MatchString(const TERM& term, const char *str) const
{
	bool match = false;
	
	switch (term.data.opcode & ~Operator_Inverted) {
		case Operator_Regex:
			match = (IsRegexAnyPattern(term.value.str) || MatchRegex(str, term.value.str));
			break;

		case Operator_Contains:
			match = (stristr(str, term.value.str) != NULL);
			break;

		case Operator_StartsWith:
			match = (stristr(str, term.value.str) == str);
			break;

		case Operator_EndsWith:
			match = (stristr(str, term.value.str) == (str + strlen(str) - strlen(term.value.str)));
			break;

		case Operator_Within:
			match = (stristr(term.value.str, str) != NULL);
			break;

		case Operator_Starts:
			match = (stristr(term.value.str, str) == term.value.str);
			break;

		case Operator_Ends:
			match = (stristr(term.value.str, str) == (term.value.str + strlen(term.value.str) - strlen(str)));
			break;

		default: {
			int res = CompareNoCase(term.value.str, str);

			switch (term.data.opcode & ~Operator_Inverted) {
				case Operator_EQ:
					match = (res == 0);
					break;
				case Operator_GT:
					match = (res > 0);
					break;
				case Operator_LT:
					match = (res < 0);
					break;
			}
			break;
		}
	}

	return (term.data.opcode & Operator_Inverted) ? !match : match;
}

bool ADVBProg::Match(const PATTERN& pattern) const
{
	const ADataList& list = pattern.list;
	uint_t i, n = list.Count();
	bool match = (n > 0);
	bool orflag = false;

	for (i = 0; (i < n) && (match || orflag); i++) {
		const TERM&  term  = *(const TERM *)list[i];
		const FIELD& field = *term.field;

		if (!RANGE(term.data.opcode, Operator_First_Assignable, Operator_Last_Assignable)) {
			const uint8_t *ptr = (const uint8_t *)((uptr_t)data + field.offset);
			int  res      = 0;
			bool newmatch = false;

			if (field.type == FieldType_string) {
				const char *str;
				uint16_t offset;

				memcpy(&offset, ptr, sizeof(offset));
				str = GetString(offset);

				if (field.offset == DVBPROG_OFFSET(strings.actors)) {
					AString actors(str);
					uint_t j, m = actors.CountLines();

					for (j = 0; j < m; j++) {
						newmatch = MatchString(term, actors.Line(j));
						if (newmatch) break;
					}
				}
				else newmatch = MatchString(term, str);
			}
			else if (field.type == FieldType_prog) {
				newmatch = SameProgramme(*this, *term.value.prog);

				if (term.data.opcode & Operator_Inverted) newmatch = !newmatch;
			}
			else {
				switch (field.type) {
					case FieldType_date: {
						ADateTime dt;
						uint64_t val;

						memcpy(&val, ptr, sizeof(val));
						dt  = ADateTime(val).UTCToLocal();
						val = (uint64_t)dt;

						switch (term.datetype) {
							case DateType_time:
								val %= 24ULL * 3600ULL * 1000ULL;
								break;

							case DateType_date:
								val /= 24ULL * 3600ULL * 1000ULL;
								break;

							case DateType_weekday:
								val = dt.GetWeekDay();
								break;

							default:
								break;
						}
					
						res = COMPARE_ITEMS(val, term.value.u64);
						break;
					}

					case FieldType_uint32_t: {
						uint32_t val;

						memcpy(&val, ptr, sizeof(val));
						res = COMPARE_ITEMS(val, term.value.u32);
						break;
					}

					case FieldType_sint32_t: {
						sint32_t val;

						memcpy(&val, ptr, sizeof(val));
						res = COMPARE_ITEMS(val, term.value.s32);
						break;
					}

					case FieldType_uint16_t: {
						uint16_t val;

						memcpy(&val, ptr, sizeof(val));
						res = COMPARE_ITEMS(val, term.value.u16);
						break;
					}

					case FieldType_sint16_t: {
						sint16_t val;

						memcpy(&val, ptr, sizeof(val));
						res = COMPARE_ITEMS(val, term.value.s16);
						break;
					}

					case FieldType_uint8_t: {
						uint8_t val;

						memcpy(&val, ptr, sizeof(val));
						res = COMPARE_ITEMS(val, term.value.u8);
						break;
					}

					case FieldType_sint8_t: {
						sint8_t val;

						memcpy(&val, ptr, sizeof(val));
						res = COMPARE_ITEMS(val, term.value.s8);
						break;
					}

					case FieldType_flag...FieldType_lastflag: {
						uint32_t flags, mask = 1UL << (field.type - FieldType_flag);
						uint8_t   cmp;

						memcpy(&flags, ptr, sizeof(flags));

						cmp = ((flags & mask) != 0);

						res = COMPARE_ITEMS(cmp, term.value.u8);
						break;
					}
				}

				switch (term.data.opcode & ~Operator_Inverted) {
					case Operator_EQ:
						newmatch = (res == 0);
						break;
					case Operator_GT:
						newmatch = (res > 0);
						break;
					case Operator_LT:
						newmatch = (res < 0);
						break;
				}

				if (term.data.opcode & Operator_Inverted) newmatch = !newmatch;
			}

			if (orflag) match |= newmatch;
			else		match  = newmatch;
		}

		orflag = term.data.orflag;
	}

	return match;
}

sint64_t ADVBProg::TermTypeToInt64s(const void *p, uint_t termtype)
{
	sint64_t val = 0;

	if (RANGE(termtype, FieldType_uint32_t, FieldType_sint8_t)) {
		uint_t   type     = FieldType_sint8_t - termtype;
		// sint8_t/uint8_t   -> 0 -> 1
		// sint16_t/uint16_t -> 1 -> 2
		// sint32_t/uint32_t -> 2 -> 4
		uint_t   bytes    = 1U << (type >> 1);
		bool   issigned = ((type & 1) == 0);

		// first, get data from pointer
		if (MachineIsBigEndian()) {
			// big endian -> copy to LAST x bytes
			memcpy((uint8_t *)&val + (sizeof(val) - bytes), p, bytes);
		}
		else {
			// little endian -> copy to FIRST x bytes
			memcpy((uint8_t *)&val, p, bytes);
		}

		// for signed types, sign extend the number
		if (issigned) {
			uint_t shift = (sizeof(val) - bytes) << 3;

			// perform UNSIGNED shift up so that sign bit is in the top bit
			// then perform SIGNED shift down to propagate the sign bit down
			val = (sint64_t)((uint64_t)val << shift) >> shift;
		}
	}

	return val;
}

void ADVBProg::Int64sToTermType(void *p, sint64_t val, uint_t termtype)
{
	if (RANGE(termtype, FieldType_uint32_t, FieldType_sint8_t)) {
		sint64_t minval, maxval;
		uint_t   type     = FieldType_sint8_t - termtype;
		// sint8_t/uint8_t   -> 0 -> 1
		// sint16_t/uint16_t -> 1 -> 2
		// sint32_t/uint32_t -> 2 -> 4
		uint_t   bytes    = 1U << (type >> 1);
		bool   issigned = ((type & 1) == 0);

		if (issigned) {
			maxval = (sint64_t)((1ULL << ((bytes << 3) - 1)) - 1);
			minval = -maxval - 1;
		}
		else {
			minval = 0;
			maxval = (sint64_t)((1ULL << (bytes << 3)) - 1);
		}

		//debug("val = %" FMT64 "s, min = " FMT64 "s, max = " FMT64 "s\n", val, minval, maxval);

		val = LIMIT(val, minval, maxval);

		if (MachineIsBigEndian()) {
			// big endian -> copy from LAST x bytes
			memcpy(p, (uint8_t *)&val + (sizeof(val) - bytes), bytes);
		}
		else {
			// little endian -> copy from FIRST x bytes
			memcpy(p, (uint8_t *)&val, bytes);
		}
	}
}

void ADVBProg::AssignValue(const FIELD& field, const VALUE& value, uint8_t termtype)
{
	uint8_t *ptr = (uint8_t *)((uptr_t)data + field.offset);

	switch (field.type) {
		case FieldType_string: {
			AString str;

			if (termtype == Operator_Concat) {
				uint16_t offset;

				memcpy(&offset, ptr, sizeof(offset));
				str = GetString(offset);
			}
			
			str += value.str;

			SetString((const uint16_t *)ptr, str.str());
			break;
		}

		case FieldType_date:
			memcpy(ptr, &value.u64, sizeof(value.u64));
			break;

		case FieldType_uint32_t...FieldType_sint8_t: {
			sint64_t val1 = TermTypeToInt64s(ptr, field.type);
			sint64_t val2 = TermTypeToInt64s(&value.u64, field.type);
			sint64_t val;

			switch (termtype) {
				case Operator_Add:
					val = val1 + val2;
					break;

				case Operator_Subtract:
					val = val1 - val2;
					break;

				case Operator_Multiply:
					val = val1 * val2;
					break;

				case Operator_Divide:
					if (val2 != 0) val = val1 / val2;
					else		   val = val1;
					break;

				case Operator_Maximum:
					val = MAX(val1, val2);
					break;

				case Operator_Minimum:
					val = MIN(val1, val2);
					break;

				default:
					val = val2;
					break;
			}

			//debug("%" FMT64 "s / %" FMT64 "s = %" FMT64 "s\n", val1, val2, val);

			Int64sToTermType(ptr, val, field.type);
			break;
		}

		case FieldType_flag...FieldType_lastflag: {
			uint32_t flags, mask = 1UL << (field.type - FieldType_flag);

			memcpy(&flags, ptr, sizeof(flags));
			if (value.u8) flags |=  mask;
			else		  flags &= ~mask;
			memcpy(ptr, &flags, sizeof(flags));
			break;
		}
	}
}

void ADVBProg::AssignValues(const PATTERN& pattern)
{
	AString user = GetUser();
	uint_t i;

	if (user.Valid()) {
		const ADVBConfig& config = ADVBConfig::Get();

		for (i = 0; i < NUMBEROF(fields); i++) {
			const FIELD& field = fields[i];

			if (field.assignable && (field.offset != DVBPROG_OFFSET(strings.user))) {
				AString val = config.GetUserConfigItem(user, field.name);

				if (val.Valid()) {
					VALUE value;

					GetFieldValue(field, value, val);

					AssignValue(field, value);
				}
			}
		}
	}

	UpdateValues(pattern);
}

void ADVBProg::UpdateValues(const PATTERN& pattern)
{
	const ADataList& list = pattern.list;
	uint_t i, n = list.Count();

	for (i = 0; i < n; i++) {
		const TERM&  term  = *(const TERM *)list[i];
		const FIELD& field = *term.field;

		if (RANGE(term.data.opcode, Operator_First_Assignable, Operator_Last_Assignable) && (field.offset != DVBPROG_OFFSET(strings.user))) {
			AssignValue(field, term.value, term.data.opcode);
		}
	}
}

void ADVBProg::Record()
{
	const ADVBConfig& config = ADVBConfig::Get();

	if (Valid()) {
		AString  oldaddlogfile = config.GetAdditionalLogFile();
		AString  addlogfile    = config.GetLogDir().CatPath(AString(GetFilename()).FilePart().Prefix() + ".txt");
		uint64_t dt, st, et;
		uint_t   nsecs, nmins;
		bool     record = true, deladdlogfile = true;

		config.printf("------------------------------------------------------------------------------------------------------------------------");

		((ADVBConfig&)config).SetAdditionalLogFile(addlogfile, false);

		config.printf("Starting record of '%s' as '%s'", GetQuickDescription().str(), GetFilename());

		AString pids;
		AString user = GetUser();
		bool    fake = (IsFakeRecording() ||
						(user.Valid() && ((uint_t)config.GetUserConfigItem(user, "fakerecordings") != 0)) ||
						((uint_t)config.GetConfigItem("fakerecordings") != 0));
		bool	reschedule = false;

		if (record && !fake) {
			ADVBChannelList& channellist = ADVBChannelList::Get();
			AString dvbchannel = GetDVBChannel();

			pids = channellist.GetPIDList(dvbchannel, true);

			if (pids.CountWords() < 2) {
				config.addlogit("\n");
				config.printf("No pids for '%s'", GetQuickDescription().str());
				reschedule = true;
				record = false;
			}
		}

		if (record) {
			dt 	  = (uint64_t)ADateTime().TimeStamp(true);

			st 	  = GetStart();
			nmins = (dt >= st) ? (uint_t)((dt - st) / 60000ULL) : 0;

			et 	  = GetRecordStop();
			nsecs = (et >= dt) ? (uint_t)((et - dt + 1000 - 1) / 1000ULL) : 0;

			if (nmins >= config.GetLatestStart()) {
				config.addlogit("\n");
				config.printf("'%s' started too long ago (%u minutes)!", GetTitleAndSubtitle().str(), nmins);
				reschedule = true;
				record = false;
			}
			else if (nsecs == 0) {
				config.addlogit("\n");
				config.printf("'%s' already finished!", GetTitleAndSubtitle().str());
				reschedule = true;
				record = false;
			}
		}

		if (record) {
			if (!AllowRepeats()) {
				const ADVBProg *recordedprog;
				ADVBProgList recordedlist;

				recordedlist.ReadFromFile(config.GetRecordedFile());

				if ((recordedprog = recordedlist.FindSimilar(*this)) != NULL) {
					config.addlogit("\n");
					config.printf("'%s' already recorded ('%s')!", GetTitleAndSubtitle().str(), recordedprog->GetQuickDescription().str());
					record = false;
				}
			}
		}

		if (record) {
			AString filename = GetFilename();
			AString cmd;
			int     res;

			if (!GetJobID()) {
				ADVBProgList schedulelist;
				const ADVBProg *prog;

				schedulelist.ReadFromFile(config.GetScheduledFile());

				// copy over job ID from scheduled list
				if ((prog = schedulelist.FindUUID(*this)) != NULL) {
					SetJobID(prog->GetJobID());
				}
				else {
					config.addlogit("\n");
					config.printf("Failed to find %s in scheduled list, cannot copy job ID!", GetTitleAndSubtitle().str());
				}
			}

			config.CreateDirectory(filename.PathPart());

			config.addlogit("\n");

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

			cmd.printf("dvbstream -c %u -n %u -f %s -o >\"%s\" 2>>%s",
					   GetDVBCard(),
					   nsecs,
					   pids.str(),
					   filename.str(),
					   config.GetLogFile().str());

			data->actstart = ADateTime().TimeStamp(true);

			if (fake) {
				// create text file with necessary filename to hold information
				AStdFile fp;

				config.addlogit("\n");
				config.printf("Faking recording of '%s' using '%s'", GetTitleAndSubtitle().str(), filename.str());

				if (fp.open(filename, "w")) {
					fp.printf("Date: %s\n", ADateTime().DateFormat("%h:%m:%s.%S %D/%M/%Y").str());
					fp.printf("Command: %s\n", cmd.str());
					fp.printf("Filename: %s\n", filename.str());
					fp.close();

					res = 0;
				}
				else {
					config.addlogit("\n");
					config.printf("Unable to create '%s' (fake recording of '%s')", filename.str(), GetTitleAndSubtitle().str());
					res = -1;
				}
			}
			else {
				config.addlogit("\n");
				config.addlogit("--------------------------------------------------------------------------------\n");
				config.writetorecordlog("start %s", Base64Encode().str());
				res = system(cmd);
				config.writetorecordlog("stop %s", Base64Encode().str());
				config.addlogit("--------------------------------------------------------------------------------\n");
				config.addlogit("\n");
			}

			if (res == 0) {
				AString   str;
				FILE_FIND info;
				uint64_t  dt = (uint64_t)ADateTime().TimeStamp(true);
				uint64_t  st = GetStop();
				bool      addtorecorded = true;

				data->actstop = dt;

				if (dt < (st - 15000ULL)) {
					config.addlogit("\n");
					config.printf("Warning: '%s' stopped %ss before programme end!", GetTitleAndSubtitle().str(), NUMSTR("", (st - dt) / 1000ULL));
					reschedule = true;
					addtorecorded = fake;
				}

				if (::GetFileDetails(filename, &info)) {
					config.addlogit("\n");
					config.printf("File '%s' exists and is %sMB, %u seconds = %skB/s", filename.str(), NUMSTR("", info.FileSize / (1024 * 1024)), nsecs, NUMSTR("", info.FileSize / (1024ULL * (uint64_t)nsecs)));

					data->filesize = info.FileSize;

					if (addtorecorded && (info.FileSize > 0)) {
						ADVBProgList recordedlist;
						AString      filename = config.GetRecordedFile();

						config.addlogit("\n");
						config.printf("Adding '%s' to list of recorded programmes", GetTitleAndSubtitle().str());

						ClearScheduled();
						SetRecorded();

						{
							ADVBLock lock("schedule");

							recordedlist.ReadFromFile(filename);
							recordedlist.AddProg(*this, false, false, true);
							recordedlist.WriteToFile(filename);
						}

						if (IsOnceOnly()) {
							DeletePattern(GetUser(), GetPattern());

							reschedule = true;
						}

						deladdlogfile &= PostProcess();
						deladdlogfile &= OnRecordSuccess();
					}
					else if (!info.FileSize) {
						config.addlogit("\n");
						config.printf("Record of '%s' ('%s') is zero length", GetTitleAndSubtitle().str(), filename.str());
						remove(filename);
						reschedule = true;
						OnRecordFailure();
						deladdlogfile = false;
					}
				}
				else {
					config.addlogit("\n");
					config.printf("Record of '%s' ('%s') doesn't exist", GetTitleAndSubtitle().str(), filename.str());
					reschedule = true;
					OnRecordFailure();
					deladdlogfile = false;
				} 
			}
			else {
				config.addlogit("\n");
				config.printf("Unable to start record of '%s'", GetTitleAndSubtitle().str());
				reschedule = true;
				deladdlogfile = false;
			} 
		}
		else {
			OnRecordFailure();
			deladdlogfile = false;
		}

		config.printf("------------------------------------------------------------------------------------------------------------------------");

		if (reschedule) {
			config.printf("Rescheduling");

			ADVBProgList::SchedulePatterns();
		}
		else ADVBProgList::CheckDiskSpace(true);

		((ADVBConfig&)config).SetAdditionalLogFile(oldaddlogfile);

		// delete additional log file
		if (deladdlogfile) remove(addlogfile);
	}
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
			.SearchAndReplace("{filename}", GetFilename())
			.SearchAndReplace("{file}", config.GetLogFile()));
}

bool ADVBProg::OnRecordSuccess() const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString user = GetUser();
	AString cmd, success = true;

	if ((cmd = config.GetUserConfigItem(user, "recordsuccesscmd")).Valid()) {
		cmd = ReplaceTerms(cmd);
		
		//config.printf("Running '%s' after successful record", cmd.str());
		if (system(cmd) != 0) {
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
		if (system(cmd) != 0) {
			config.addlogit("\n");
			config.printf("Command '%s' failed!", cmd.str());
			success = false;
		}
	}

	return success;
}

bool ADVBProg::PostProcess()
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString user = GetUser();
	bool success = false;

	if (RunPostProcess() || ((uint_t)config.GetUserConfigItem(user, "postprocessall") != 0)) {
		AString postcmd;

		if ((postcmd = config.GetUserConfigItem(user, "postprocesscmd")).Valid()) {
			AStdFile fp;

			config.addlogit("\n");
			config.printf("Running post process command '%s'", postcmd.str());

			postcmd = ReplaceTerms(postcmd);
			postcmd.printf(" 2>&1 >>\"%s\"", config.GetLogFile().str());

			if (system(postcmd) == 0) {
				ADVBLock     lock("schedule");
				ADVBProgList recordedlist;
				AString  	 filename = config.GetRecordedFile();
				ADVBProg 	 *prog;

				recordedlist.ReadFromFile(filename);
				if ((prog = recordedlist.FindUUIDWritable(*this)) != NULL) {
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
		else {
			config.addlogit("\n");
			config.logit("No post process command specified for user '%s'", user.str());
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

bool ADVBProg::UpdatePatternInFile(const AString& filename, const AString& pattern, const AString& newpattern)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString filename1 = filename + ".new";
	AStdFile ifp, ofp;
	bool done = false;

	if (ifp.open(filename)) {
		if (ofp.open(filename1, "w")) {
			AString line;

			while (line.ReadLn(ifp) >= 0) {
				if (line == pattern) {
					if (newpattern.Valid()) ofp.printf("%s\n", newpattern.str());
					done = true;

					if (newpattern.Valid()) {
						config.logit("Changed pattern '%s' to '%s' in file '%s'", pattern.str(), newpattern.str(), filename.str());
					}
					else {
						config.logit("Deleted pattern '%s' from file '%s'", pattern.str(), filename.str());
					}
				}
				else ofp.printf("%s\n", line.str());
			}
			ofp.close();
		}

		ifp.close();

		if (done) {
			remove(filename);
			rename(filename1, filename);
		}
		else remove(filename1);
	}

	return done;
}

void ADVBProg::AddPatternToFile(const AString& filename, const AString& pattern)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AStdFile fp;
	bool done = false;

	if (fp.open(filename)) {
		AString line;

		while (line.ReadLn(fp) >= 0) {
			if (line == pattern) {
				done = true;
				break;
			}
		}

		fp.close();

		if (!done) {
			if (fp.open(filename, "a")) {
				fp.printf("%s\n", pattern.str());
				fp.close();

				config.logit("Add pattern '%s' to file '%s'", pattern.str(), filename.str());
			}
		}
	}
}

void ADVBProg::UpdatePattern(const AString& olduser, const AString& oldpattern, const AString& newuser, const AString& newpattern)
{
	ADVBLock lock("patterns");

	if (newuser != olduser) {
		DeletePattern(olduser, oldpattern);
		if (newpattern.Valid()) {
			InsertPattern(newuser, newpattern);
		}
	}
	else if (newpattern != oldpattern) UpdatePattern(newuser, oldpattern, newpattern);
}

void ADVBProg::UpdatePattern(const AString& user, const AString& pattern, const AString& newpattern)
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBLock lock("patterns");
	
	if (user.Empty() || !UpdatePatternInFile(config.GetUserPatternsPattern().SearchAndReplace("{#?}", user), pattern, newpattern)) {
		UpdatePatternInFile(config.GetPatternsFile(), pattern, newpattern);
	}
}

void ADVBProg::InsertPattern(const AString& user, const AString& pattern)
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBLock lock("patterns");
	
	if (user.Valid()) AddPatternToFile(config.GetUserPatternsPattern().SearchAndReplace("{#?}", user), pattern);
	else			  AddPatternToFile(config.GetPatternsFile(), pattern);
}

void ADVBProg::DeletePattern(const AString& user, const AString& pattern)
{
	UpdatePattern(user, pattern, "");
}

void ADVBProg::EnablePattern(const AString& user, const AString& pattern)
{
	if (pattern[0] == '#') {
		UpdatePattern(user, pattern, pattern.Mid(1));
	}
}

void ADVBProg::DisablePattern(const AString& user, const AString& pattern)
{
	if (pattern[0] != '#') {
		UpdatePattern(user, pattern, "#" + pattern);
	}
}
