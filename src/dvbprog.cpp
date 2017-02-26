
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#include <map>
#include <algorithm>

#include <rdlib/Regex.h>
#include <rdlib/Recurse.h>

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
#define DEFINE_FIELD(name,var,type,desc)	{#name, ADVBPatterns::FieldType_##type, false, DVBPROG_OFFSET(var), desc}
#define DEFINE_SIMPLE(name,type,desc)		DEFINE_FIELD(name, name, type, desc)
#define DEFINE_STRING(name,desc)			DEFINE_FIELD(name, strings.name, string, desc)
#define DEFINE_FLAG(name,flag,desc)			{#name, ADVBPatterns::FieldType_flag + flag, false, DVBPROG_OFFSET(flags), desc}
#define DEFINE_FLAG_ASSIGN(name,flag,desc)	{#name, ADVBPatterns::FieldType_flag + flag, true, DVBPROG_OFFSET(flags), desc}
#define DEFINE_ASSIGN(name,var,type,desc)	{#name, ADVBPatterns::FieldType_##type, true, DVBPROG_OFFSET(var), desc}
#define DEFINE_EXTERNAL(name,id,type,desc)  {#name, ADVBPatterns::FieldType_external_##type, false, id, desc}

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
	DEFINE_STRING(pattern, "Pattern used to find programme"),
	DEFINE_FIELD(actor, strings.actors, string, "Actor(s)"),

#if DVBDATVERSION > 1
	DEFINE_STRING(episodeid, "Episode ID"),
	DEFINE_STRING(icon, "Programme icon"),
	DEFINE_STRING(rating, "Age rating"),
	DEFINE_STRING(subcategory, "Programme subcategor(ies)"),
#endif

	DEFINE_FIELD(on, start, date, "Day"),
	DEFINE_FIELD(day, start, date, "Day"),
	DEFINE_FIELD(age, stop, age, "Age"),
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
	DEFINE_FLAG(incompleterecording, Flag_incompleterecording, "Programme recorded is incomplete"),
	DEFINE_FLAG(ignorerecording,     Flag_ignorerecording,     "Programme recorded should be ignored when scheduling"),
	DEFINE_FLAG(failed,				 Flag_recordingfailed,     "Programme recording failed"),
	DEFINE_FLAG(recording,  		 Flag_recording,  	 	   "Programme recording"),
	DEFINE_FLAG(postprocessing,		 Flag_postprocessing,	   "Programme recording being processing"),
	DEFINE_FLAG(exists,				 Flag_exists,    	 	   "Programme exists"),
	DEFINE_FLAG(available,			 Flag_exists,    	 	   "Programme is available"),
	DEFINE_FLAG(notify,				 Flag_notify,    	 	   "Notify by when programme has recorded"),
	DEFINE_FLAG(converted,			 Flag_converted,    	   "Programme has been converted"),
	DEFINE_FLAG(film,				 Flag_film,				   "Programme is a film"),
	DEFINE_FLAG(recordable,			 Flag_recordable,		   "Programme is recordable"),
	DEFINE_FLAG(partial,			 Flag_partialpattern,      "Pattern is partial and not complete"),

	DEFINE_FIELD(epvalid,  	  episode.valid,    uint8_t,  "Series/episode valid"),
	DEFINE_FIELD(series,   	  episode.series,   uint8_t,  "Series"),
	DEFINE_FIELD(episode,  	  episode.episode,  uint16_t, "Episode"),
	DEFINE_FIELD(episodes, 	  episode.episodes, uint16_t, "Number of episodes in series"),

	DEFINE_FIELD(assignedepisode, assignedepisode, uint16_t, "Assigned episode"),

	DEFINE_FLAG_ASSIGN(usedesc,		  Flag_usedesc,       	   "Use description"),
	DEFINE_FLAG_ASSIGN(allowrepeats,  Flag_allowrepeats,  	   "Allow repeats to be recorded"),
	DEFINE_FLAG_ASSIGN(urgent,		  Flag_urgent,   	  	   "Record as soon as possible"),
	DEFINE_FLAG_ASSIGN(markonly,	  Flag_markonly, 	  	   "Do not record, just mark as recorded"),
	DEFINE_FLAG_ASSIGN(postprocess,	  Flag_postprocess,   	   "Post process programme"),
	DEFINE_FLAG_ASSIGN(onceonly,	  Flag_onceonly,      	   "Once recorded, delete the pattern"),
	DEFINE_FLAG_ASSIGN(notify,		  Flag_notify,        	   "Once recorded, run 'notifycmd'"),
	DEFINE_FLAG_ASSIGN(partial,		  Flag_partialpattern,     "Pattern is partial and not complete"),

	DEFINE_ASSIGN(pri,        pri,        	 	sint8_t,  "Scheduling priority"),
	DEFINE_ASSIGN(score,	  score,			sint16_t, "Record score"),
	DEFINE_ASSIGN(prehandle,  prehandle,  	 	uint16_t, "Record pre-handle (minutes)"),
	DEFINE_ASSIGN(posthandle, posthandle, 	 	uint16_t, "Record post-handle (minutes)"),
	DEFINE_ASSIGN(user,		  strings.user,  	string,   "User"),
	DEFINE_ASSIGN(dir,		  strings.dir,  	string,   "Directory to store file in"),
	DEFINE_ASSIGN(prefs,	  strings.prefs, 	string,   "Misc prefs"),
	DEFINE_ASSIGN(dvbcard,    dvbcard,        	uint8_t,  "DVB card to record from"),
#if DVBDATVERSION>1
	DEFINE_ASSIGN(tags,		  strings.tags, 	string,   "Programme tags"),
#endif
	
	DEFINE_EXTERNAL(brate,	  Compare_brate,  	uint32_t, "Encoded file bit rate (bits/s)"),
	DEFINE_EXTERNAL(kbrate,   Compare_kbrate, 	uint32_t, "Encoded file bit rate (kbits/s)"),
};

AString ADVBProg::dayformat  	 = "%d";
AString ADVBProg::dateformat 	 = "%D-%N-%Y";
AString ADVBProg::timeformat 	 = "%h:%m";
AString ADVBProg::fulltimeformat = "%h:%m:%s";
AString ADVBProg::tempfilesuffix;
AString ADVBProg::recordedfilesuffix;
AString ADVBProg::videofilesuffix;
AString ADVBProg::audiofilesuffix;

const AString ADVBProg::videotrackname 	  	   = "videotrack";
const AString ADVBProg::audiotrackname 	  	   = "audiotrack";

const AString ADVBProg::priorityscalename 	   = "priorityscale";
const AString ADVBProg::overlapscalename 	   = "overlapscale";
const AString ADVBProg::urgentscalename  	   = "urgentscale";
const AString ADVBProg::repeatsscalename 	   = "repeatsscale";
const AString ADVBProg::delayscalename   	   = "delayscale";
const AString ADVBProg::recordoverlapscalename = "recordoverlapscale";

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
		const ADVBConfig& config = ADVBConfig::Get();
		uint_t i;

		fieldhash.Create(20);
		for (i = 0; i < NUMBEROF(fields); i++) {
			fieldhash.Insert(fields[i].name, (uptr_t)(fields + i));
		}

		dayformat  	   = config.GetConfigItem("dayformat",  	"%d");
		dateformat 	   = config.GetConfigItem("dateformat", 	" %D-%N-%Y ");
		timeformat 	   = config.GetConfigItem("timeformat", 	"%h:%m");
		fulltimeformat = config.GetConfigItem("fulltimeformat", "%h:%m:%s");
	}

	if (tempfilesuffix.Empty()) {
		const ADVBConfig& config = ADVBConfig::Get();

		tempfilesuffix     = config.GetTempFileSuffix();
		recordedfilesuffix = config.GetRecordedFileSuffix();
		videofilesuffix    = config.GetVideoFileSuffix();
		audiofilesuffix    = config.GetAudioFileSuffix();
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

#if DVBDATVERSION > 1
uint16_t ADVBProg::GetSubCategoryDataOffset()
{
	return DVBPROG_OFFSET(strings.subcategory);
}
#endif

uint16_t ADVBProg::GetPriDataOffset()
{
	return DVBPROG_OFFSET(pri);
}

uint16_t ADVBProg::GetScoreDataOffset()
{
	return DVBPROG_OFFSET(score);
}

#if DVBDATVERSION > 1
uint16_t ADVBProg::GetTagsDataOffset()
{
	return DVBPROG_OFFSET(strings.tags);
}
#endif

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

#if DVBDATVERSION==1
	prog->bigendian       = (uint8_t)MachineIsBigEndian();
#endif
}

ADVBProg& ADVBProg::operator = (AStdData& fp)
{
	DVBPROG _data;
	slong_t n;
	bool    success = false;

	Delete();

#if DVBDATVERSION==1
	if ((n = fp.readbytes((uint8_t *)&_data, sizeof(_data))) == (slong_t)sizeof(_data)) {
		if (_data.bigendian != (uint8_t)MachineIsBigEndian()) SwapBytes(data);

		if ((sizeof(_data) + _data.strings.end) > maxsize) {
			if (data) free(data);

			maxsize = sizeof(_data) + _data.strings.end;
			data    = (DVBPROG *)calloc(1, maxsize);
			data->bigendian = (uint8_t)MachineIsBigEndian();
		}

		if (data && ((sizeof(_data) + _data.strings.end) <= maxsize)) {
			memcpy(data, &_data, sizeof(*data));

			if ((n = fp.readbytes(data->strdata, _data.strings.end)) == (slong_t)_data.strings.end) {
				success = true;
			}
		}
	}
#else
	static const uint8_t bigendian = (uint8_t)MachineIsBigEndian();
	uint8_t header;

	if (fp.readitem(header)) {
		uint8_t version = header & 127;
		bool    swap    = ((header >> 7) != bigendian);

		(void)version;

		if ((n = fp.readbytes((uint8_t *)&_data, sizeof(_data))) == (slong_t)sizeof(_data)) {
			if (swap) SwapBytes(data);

			if ((sizeof(_data) + _data.strings.end) > maxsize) {
				if (data) free(data);

				maxsize = sizeof(_data) + _data.strings.end;
				data    = (DVBPROG *)calloc(1, maxsize);
			}

			if (data && ((sizeof(_data) + _data.strings.end) <= maxsize)) {
				memcpy(data, &_data, sizeof(*data));

				if ((n = fp.readbytes(data->strdata, _data.strings.end)) == (slong_t)_data.strings.end) {
					success = true;
				}
			}
		}
	}
#endif

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
#if DVBDATVERSION==1
			data->bigendian = (uint8_t)MachineIsBigEndian();
#endif
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
	uint64_t maxreclag = (uint64_t)config.GetConfigItem("maxrecordlag", "20") * 1000;

	SetFlag(Flag_incompleterecording, !((data->actstart <= (data->start + maxreclag)) && (data->actstop >= MIN(data->stop, data->recstop - 1000))));
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

ADVBProg& ADVBProg::operator = (const AString& str)
{
	static const AString tsod = "tsod.plus-1.";
	const ADVBConfig& config = ADVBConfig::Get();
	const char *pstr;
	AString _str;
	int p = 0;

	Delete();

	data->start           = GetDate(str, "start");
	data->stop            = GetDate(str, "stop");
	data->recstart        = GetDate(str, "recstart");
	data->recstop         = GetDate(str, "recstop");
	data->actstart        = GetDate(str, "actstart");
	data->actstop         = GetDate(str, "actstop");

	data->filesize = (uint64_t)GetField(str, "filesize");
	data->flags    = (uint32_t)GetField(str, "flags");

	if (FieldExists(str, "episodenum:xmltv_ns")) SetString(&data->strings.episodenum, GetField(str, "episodenum:xmltv_ns"));
	else										 SetString(&data->strings.episodenum, GetField(str, "episodenum"));

#if DVBDATVERSION > 1
	if		(FieldExists(str, "episodenum:dd_progid"))			SetString(&data->strings.episodeid, GetField(str, "episodenum:dd_progid"));
	else if	(FieldExists(str, "episodenum:brandseriesepisode")) SetString(&data->strings.episodeid, GetField(str, "episodenum:brandseriesepisode"));
	else if (FieldExists(str, "episodeid"))						SetString(&data->strings.episodeid, GetField(str, "episodeid"));
#endif

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
			if		(category.Empty())		category = _str;
			else if (ignoreothercategories) break;
			else if (_str.ToLower() != "film") {
				subcategory += _str + "\n";
			}
		}

		SetString(&data->strings.category, category);

#if DVBDATVERSION > 1
		SetString(&data->strings.subcategory, subcategory);
#endif
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
	else						 SetDir(config.GetUserConfigItem(GetUser(), "dir"));
	SetFilename(GetField(str, "filename"));
	SetPattern(GetField(str, "pattern"));

	SetUUID();

#if DVBDATVERSION > 1
	if (FieldExists(str, "icon")) {
		AString icon = GetField(str, "icon");
		SetString(&data->strings.icon, icon);
		ADVBIconCache::Get().SetIcon("programme", GetProgrammeKey(), icon);
	}

	if (FieldExists(str, "rating")) SetString(&data->strings.rating, GetField(str, "rating"));
#endif

	data->assignedepisode = (uint16_t)GetField(str, "assignedepisode");
	if (FieldExists(str, "date")) {
		data->year = (uint16_t)GetField(str, "date");
	}
	else data->year = (uint16_t)GetField(str, "year");

	data->jobid    = (uint_t)GetField(str, "jobid");
	data->score    = (sint_t)GetField(str, "score");

	if (FieldExists(str, "prehandle")) 	data->prehandle  = (uint_t)GetField(str, "prehandle");
	else							   	data->prehandle  = (uint_t)config.GetUserConfigItem(GetUser(), "prehandle");
	if (FieldExists(str, "posthandle")) data->posthandle = (uint_t)GetField(str, "posthandle");
	else							    data->posthandle = (uint_t)config.GetUserConfigItem(GetUser(), "posthandle");
	if (FieldExists(str, "pri"))		data->pri		 = (int)GetField(str, "pri");
	else								data->pri		 = (int)config.GetUserConfigItem(GetUser(), "pri");

	data->dvbcard 	 = (uint_t)GetField(str, "card");

	SearchAndReplace("\xc2\xa3", "£");

	if (Valid()) FixData();

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
#if DVBDATVERSION==1
			data->bigendian = (uint8_t)MachineIsBigEndian();
#endif
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
#if DVBDATVERSION==1
	return (fp.writebytes((uint8_t *)data, sizeof(*data) + data->strings.end) == (slong_t)(sizeof(*data) + data->strings.end));
#else
	static const uint8_t header = ((uint8_t)MachineIsBigEndian() << 7) | DVBDATVERSION;
	return (fp.writeitem(header) && (fp.writebytes((uint8_t *)data, sizeof(*data) + data->strings.end) == (slong_t)(sizeof(*data) + data->strings.end)));
#endif
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

#if DVBDATVERSION > 1
	if ((p = GetString(data->strings.episodeid))[0]) str.printf("episodeid=%s\n", p);
	if ((p = GetString(data->strings.rating))[0])	 str.printf("rating=%s\n", p);

	AString icon = GetString(data->strings.icon);
	if (icon.Empty()) icon = ADVBIconCache::Get().GetIcon("programme", GetProgrammeKey());
	if (icon.Valid()) str.printf("icon=%s\n", icon.str());
#endif

	if (data->assignedepisode) str.printf("assignedepisode=%u\n", data->assignedepisode);
	if (data->year) str.printf("year=%u\n", data->year);

	if (GetString(data->strings.pattern)[0]) {
		str.printf("score=%d\n", (sint_t)data->score);
		str.printf("prehandle=%u\n", (uint_t)data->prehandle);
		str.printf("posthandle=%u\n", (uint_t)data->posthandle);
		str.printf("pri=%d\n", (int)data->pri);
	}

#if DVBDATVERSION > 1
	if ((p = GetString(data->strings.tags))[0]) {
		str.printf("tags=%s\n", p);
	}
#endif
	
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

	str.printf("\"start\":%s", JSONTime(data->start).str());
	str.printf(",\"stop\":%s", JSONTime(data->stop).str());
	if (data->recstart || data->recstop) {
		str.printf(",\"recstart\":%s", JSONTime(data->recstart).str());
		str.printf(",\"recstop\":%s", JSONTime(data->recstop).str());
	}
	if (data->actstart || data->actstop) {
		str.printf(",\"actstart\":%s", JSONTime(data->actstart).str());
		str.printf(",\"actstop\":%s", JSONTime(data->actstop).str());
	}
	str.printf(",\"channel\":\"%s\"", JSONFormat(GetString(data->strings.channel)).str());
	str.printf(",\"basechannel\":\"%s\"", JSONFormat(GetString(data->strings.basechannel)).str());
	str.printf(",\"channelid\":\"%s\"", JSONFormat(GetString(data->strings.channelid)).str());
	if ((p = GetString(data->strings.dvbchannel))[0]) str.printf(",\"dvbchannel\":\"%s\"", JSONFormat(p).str());
	str.printf(",\"title\":\"%s\"", JSONFormat(GetString(data->strings.title)).str());
	if ((p = GetString(data->strings.subtitle))[0]) str.printf(",\"subtitle\":\"%s\"", JSONFormat(p).str());
	if ((p = GetString(data->strings.desc))[0]) str.printf(",\"desc\":\"%s\"", JSONFormat(p).str());
	if ((p = GetString(data->strings.category))[0]) str.printf(",\"category\":\"%s\"", JSONFormat(p).str());
	if ((p = GetString(data->strings.subcategory))[0]) {
		str.printf(",\"subcategory\":[");

		AString subcategory = p;
		uint_t  i, n = subcategory.CountLines();
		for (i = 0; i < n; i++) {
			if (i) str.printf(",");
			str.printf("\"%s\"", JSONFormat(subcategory.Line(i)).str());
		}

		str.printf("]");
	}
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

#if DVBDATVERSION > 1
	if ((p = GetString(data->strings.tags))[0]) {
		AString _tags = AString(p);
		uint_t i, n = _tags.CountLines("||", 0);

		str.printf(",\"tags\":[");

		for (i = 0; i < n; i++) {
			AString tag = _tags.Line(i, "||");
			if (tag.FirstChar() == '|') tag = tag.Mid(1);
			if (tag.LastChar()  == '|') tag = tag.Left(tag.len() - 1);
			if (i) str.printf(",");
			str.printf("\"%s\"", JSONFormat(tag).str());
		}

		str.printf("]");
	}
#endif
	
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

#if DVBDATVERSION > 1
	if (GetString(data->strings.episodeid)[0]) str.printf(",\"episodeid\":\"%s\"", GetString(data->strings.episodeid));

	AString icon = GetString(data->strings.icon);
	if (icon.Empty()) icon = ADVBIconCache::Get().GetIcon("programme", GetProgrammeKey());
	if (icon.Valid()) str.printf(",\"icon\":\"%s\"", JSONFormat(icon).str());

	icon = ADVBIconCache::Get().GetIcon("channel", GetChannel());
	if (icon.Empty()) icon = ADVBIconCache::Get().GetIcon("channel", GetDVBChannel());
	if (icon.Valid()) str.printf(",\"channelicon\":\"%s\"", JSONFormat(icon).str());

	if ((p = GetString(data->strings.rating))[0]) str.printf(",\"rating\":\"%s\"", JSONFormat(p).str());
#endif

	str.printf(",\"flags\":{");
	str.printf("\"bitmap\":%s", AValue(data->flags).ToString().str());
	{
		uint_t i;
		AHash hash(20);
		for (i = 0; i < NUMBEROF(fields); i++) {
			if (RANGE(fields[i].type, ADVBPatterns::FieldType_flag, ADVBPatterns::FieldType_lastflag) && !hash.Exists(fields[i].name)) {
				hash.Insert(fields[i].name, 0);
				str.printf(",\"%s\":%u", fields[i].name, (uint_t)GetFlag(fields[i].type - ADVBPatterns::FieldType_flag));
			}
		}
	}
	str.printf("}");

	if (data->filesize) {
		str.printf(",\"filesize\":%s", AValue(data->filesize).ToString().str());

		uint_t rate = GetRate() / 1024;
		if (rate) str.printf(",\"rate\":%u", rate);
	}
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
		AString relpath;
		if (data->actstart && data->actstop && filename[0] && IsConverted() && (relpath = config.GetRelativePath(filename)).Valid()) {
			bool exists = AStdFile::exists(filename);

			str.printf(",\"exists\":%u", (uint_t)exists);
			if (exists) {
				AList subfiles;

				CollectFiles(filename.PathPart(), filename.FilePart().Prefix() + ".*", RECURSE_ALL_SUBDIRS, subfiles);

				str.printf(",\"file\":\"%s\"", JSONFormat(relpath).str());

				const AString *subfile = AString::Cast(subfiles.First());
				bool first = true;
				while (subfile) {
					if ((*subfile != filename) && (relpath = config.GetRelativePath(*subfile)).Valid()) {
						if (first) {
							str.printf(",\"subfiles\":[");
							first = false;
						}
						else str.printf(",");

						str.printf("\"%s\"", JSONFormat(relpath).str());
					}

					subfile = subfile->Next();
				}

				if (!first) str.printf("]");
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

#if DVBDATVERSION==1
		data->bigendian = (uint8_t)MachineIsBigEndian();
#endif
	}
}

bool ADVBProg::FixData()
{
	const ADVBConfig& config = ADVBConfig::Get();
	bool changed = false;

	if (CompareCase(GetTitle(), "Futurama") == 0) {
		EPISODE ep = GetEpisode(GetEpisodeNum());

		if (ep.valid) {
			if		(ep.series == 1) ep.episodes = 20;
			else if (ep.series == 2) {
				ep.series--;
				ep.episode += 9;
			}
			else if (ep.series > 2) ep.series--;

			if ((ep.valid  	 != data->episode.valid) ||
				(ep.series 	 != data->episode.series) ||
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
	AString res, _res;

	if ((res = GetEpisodeString(GetEpisode())).Empty())
	{
		if ((_res = GetString(data->strings.episodeid)).Valid() && (_res.Left(2) == "EP")) res = _res.Left(2) + _res.Right(4);
	}

	return res;
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

#if DVBDATVERSION > 1
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
#endif

			str1.printf(".");
		}

		if ((verbosity > 2) && ep.valid) {
			AString epstr;

			if (ep.series) epstr.printf("Series %u", ep.series);
			if (ep.episode) {
				if (epstr.Valid()) epstr.printf(", episode %u", ep.episode);
				else			   epstr.printf("Episode %u", ep.episode);
				if (ep.episodes)   epstr.printf(" of %u", ep.episodes);
			}

#if DVBDATVERSION > 1
			if ((p = GetEpisodeID())[0]) {
				if (epstr.Valid()) epstr.printf(" ");
				epstr.printf("(%s)", p);
			}
#endif

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
			
#if DVBDATVERSION > 1
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
#endif

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
			str1.printf("%s as '%s'.", data->actstart ? "Recorded" : (data->recstart ? "To be recorded" : "Would be recorded"), GetFilename());

			if (IsPostProcessing()) str1.printf(" Current being post-processed.");

			bool exists = AStdFile::exists(GetFilename());
			str1.printf(" File %sexists",  exists ? "" : "does *not* ");
			if (GetFileSize()) str1.printf(" and %s %sMB in size", exists ? "is" : "was", AValue(GetFileSize() / (1024 * 1024)).ToString().str());

			uint_t rate = GetRate() / 1024;
			if (rate) str1.printf(" (rate %ukbits/s)", rate);
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

int ADVBProg::CompareExternal(uint_t id, uint32_t value) const
{
	uint32_t val;
	int res = 0;

	switch (id) {
		case Compare_brate:
			val = GetRate();
			if (val < value) res = -1;
			if (val > value) res =  1;
			break;

		case Compare_kbrate:
			val = GetRate();
			if (( val         / 1024) < value) res = -1;
			if (((val + 1023) / 1024) > value) res =  1;
			break;

		default:
			break;
	}

	return res;
}

int ADVBProg::CompareExternal(uint_t id, sint32_t value) const
{
	sint32_t val;
	int res = 0;

	switch (id) {
		case Compare_brate:
			val = GetRate();
			if (val < value) res = -1;
			if (val > value) res =  1;
			break;

		case Compare_kbrate:
			val = GetRate();
			if (( val         / 1024) < value) res = -1;
			if (((val + 1023) / 1024) > value) res =  1;
			break;

		default:
			break;
	}

	return res;
}

bool ADVBProg::SameProgramme(const ADVBProg& prog1, const ADVBProg& prog2)
{
	static const uint64_t hour = 3600 * 1000;
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
			same = (prog1.GetYear() == prog2.GetYear());
		}
		else if (ep1.valid && ep2.valid) {
			same = ((ep1.series == ep2.series) && (ep1.episode == ep2.episode));
#if DEBUG_SAMEPROGRAMME
			if (debugsameprogramme) debug("'%s' / '%s': episode S%uE%02u / S%uE%02u: %s\n", prog1.GetDescription().str(), prog2.GetDescription().str(), (uint_t)ep1.series, (uint_t)ep1.episode, (uint_t)ep2.series, (uint_t)ep2.episode, same ? "same" : "different");
#endif
		}
#if DVBDATVERSION > 1
		else if (prog1.GetEpisodeID()[0] && prog2.GetEpisodeID()[0]) {
			// episodeid is valid in both -> sameness can be determined
			same = (CompareCase(prog1.GetEpisodeID(), prog2.GetEpisodeID()) == 0);
#if DEBUG_SAMEPROGRAMME
			if (debugsameprogramme) debug("'%s' / '%s': episodeid '%s' / '%s': %s\n", prog1.GetDescription().str(), prog2.GetDescription().str(), prog1.GetEpisodeID(), prog2.GetEpisodeID(), same ? "same" : "different");
#endif
		}
#endif
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

AString ADVBProg::GetHierarchicalConfigItem(const AString& name, const AString& defval) const
{
	const ADVBConfig& config = ADVBConfig::Get();
	return config.GetHierarchicalConfigItem(GetUUID(), name,
											config.GetHierarchicalConfigItem(GetTitle(), name,
																			 config.GetHierarchicalConfigItem(GetUser(), name,
																											  config.GetConfigItem(name, defval))));
}
	
AString ADVBProg::ValidFilename(const AString& str, bool dir)
{
	AString res;
	sint_t i;

	{
		AStringUpdate updater(&res);

		for (i = 0; i < str.len(); i++) {
			char c = str[i];

			if		(IsSymbolChar(c) || (c == '.') || (c == '-') || (dir && (c == ' '))) updater.Update(c);
			else if ((c == '/') || IsWhiteSpace(c)) updater.Update('_');
		}
	}

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

AString ADVBProg::ReplaceDirectoryTerms(const AString& str) const
{
	const ADVBConfig& config = ADVBConfig::Get();
	return config.ReplaceTerms(GetUser(),
							   str.SearchAndReplace("{titledir}", ValidFilename(GetTitle(), true)));
}

AString ADVBProg::ReplaceFilenameTerms(const AString& str, bool converted) const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString date  = GetStartDT().UTCToLocal().DateFormat("%Y-%M-%D");
	AString times = GetStartDT().UTCToLocal().DateFormat("%h%m") + "-" + GetStopDT().UTCToLocal().DateFormat("%h%m");
	AString res   = config.ReplaceTerms(GetUser(),
										ReplaceDirectoryTerms(str)
										.SearchAndReplace("{title}", ValidFilename(GetTitle()))
										.SearchAndReplace("{subtitle}", ValidFilename(GetSubtitle()))
										.SearchAndReplace("{episode}", ValidFilename(GetEpisodeString()))
										.SearchAndReplace("{channel}", ValidFilename(GetChannel()))
										.SearchAndReplace("{date}", ValidFilename(date))
										.SearchAndReplace("{times}", ValidFilename(times))
										.SearchAndReplace("{user}", ValidFilename(GetUser()))
										.SearchAndReplace("{userdir}", ValidFilename(GetUser(), true))
										.SearchAndReplace("{suffix}", converted ? config.GetConvertedFileSuffix(GetUser()) : recordedfilesuffix));

	while (res.Pos("{sep}{sep}") >= 0) {
		res = res.SearchAndReplace("{sep}{sep}", "{sep}");
	}

	res = res.SearchAndReplace("{sep}", ".");

	return res;
}

AString ADVBProg::GetRecordingSubDir() const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString subdir = GetDir();
	return CatPath(config.GetRecordingsDir(), subdir.Valid() ? subdir : config.GetRecordingsSubDir(GetUser(), GetCategory()));
}

AString ADVBProg::GenerateFilename(bool converted) const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString templ = config.GetFilenameTemplate(GetUser(), GetCategory());
	AString dir   = converted ? GetRecordingSubDir() : config.GetRecordingsStorageDir();
	return ReplaceFilenameTerms(dir.CatPath(templ), converted);
}

AString ADVBProg::GetConvertedDestinationDirectory() const
{
	return ReplaceDirectoryTerms(GetRecordingSubDir());
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

		str.printf("\nCard %u, priority %d, Recording %s - %s\n",
				   GetDVBCard(), GetPri(),
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

void ADVBProg::AddToList(ADataList *list)
{
	this->list = list;
	list->Add((uptr_t)this);
}

void ADVBProg::RemoveFromList()
{
	if (list) list->Remove((uptr_t)this);
}

int ADVBProg::CompareProgrammesByTime(uptr_t item1, uptr_t item2, void *context)
{
	const ADVBProg *prog1 = (const ADVBProg *)item1;
	const ADVBProg *prog2 = (const ADVBProg *)item2;

	return Compare(prog1, prog2, (const bool *)context);
}

void ADVBProg::SetPriorityScore(const ADateTime& starttime)
{
	priority_score = ((double)GetHierarchicalConfigItem(priorityscalename, "2.0") * (double)data->pri +
					  (double)GetHierarchicalConfigItem(overlapscalename, "-.2")  * (double)overlaps);

	if (IsUrgent()) priority_score += (double)GetHierarchicalConfigItem(urgentscalename, "3.0");
	
	if (list) priority_score += (double)GetHierarchicalConfigItem(repeatsscalename, "-1.0") * (double)(list->Count() - 1);

	priority_score += (double)GetHierarchicalConfigItem(delayscalename, "-.5") * (double)(GetStartDT().GetDays() - starttime.GetDays());
}

bool ADVBProg::BiasPriorityScore(const ADVBProg& prog)
{
	bool changed = false;
						 
	// if the specified programme overlaps this programme *just* in terms
	// of pre- and post- handles, bias the priority score
	if (RecordOverlaps(prog) && !Overlaps(prog)) {
		priority_score += (double)GetHierarchicalConfigItem(recordoverlapscalename, "-2.0");
		changed = true;
	}

	return changed;
}

bool ADVBProg::CountOverlaps(const ADVBProgList& proglist)
{
	uint_t i, newoverlaps = 0;
	bool changed;
	
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
	const ADVBProg& prog1 	   = *(const ADVBProg *)item1;
	const ADVBProg& prog2 	   = *(const ADVBProg *)item2;
	bool			prog1urg   = prog1.IsUrgent();
	bool			prog2urg   = prog2.IsUrgent();
	int res = 0;

	UNUSED(context);
	
	if		( prog1urg && !prog2urg) 		  		  		   		  res = -1;
	else if (!prog1urg &&  prog2urg) 		  		  		   		  res =  1;
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

int ADVBProg::SortListByScore(uptr_t item1, uptr_t item2, void *context)
{
	UNUSED(context);
	return CompareScore(*(const ADVBProg *)item1, *(const ADVBProg *)item2);
}

bool ADVBProg::GetRecordPIDs(AString& pids, bool update) const
{
	ADVBChannelList& channellist = ADVBChannelList::Get();
	AString dvbchannel = GetDVBChannel();

	if (dvbchannel.Empty()) dvbchannel = GetChannel();

	return channellist.GetPIDList(GetDVBCard(), dvbchannel, pids, update);
}

AString ADVBProg::GenerateRecordCommand(uint_t nsecs, const AString& pids) const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString cmd;

	cmd.printf("dvbstream -c %u -n %u -f %s -o 2>>\"%s\" >\"%s\"",
			   config.GetPhysicalDVBCard(GetDVBCard()),
			   nsecs,
			   pids.str(),
			   config.GetLogFile().str(),
			   GetTempFilename().str());

	return cmd;
}

bool ADVBProg::UpdateFileSize(uint_t nsecs)
{
	const ADVBConfig& config = ADVBConfig::Get();
	FILE_INFO info;
	AString   filename = GetFilename();
	bool      valid    = true;

	if (::GetFileInfo(filename, &info)) {
		uint64_t oldfilesize = GetFileSize();
		uint32_t rate        = (uint32_t)(info.FileSize / (1024 * (uint64_t)nsecs));

		config.printf("File '%s' exists and is %sMB, %s seconds = %skB/s%s",
					  GetFilename(),
					  AValue(info.FileSize / (1024 * 1024)).ToString().str(),
					  AValue(nsecs).ToString().str(),
					  AValue(rate).ToString().str(),
					  oldfilesize ? AString(", file size ratio = %0.3;").Arg((double)info.FileSize / (double)oldfilesize).str() : "");

		SetFileSize(info.FileSize);

		valid = (rate > 0);

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
		bool	reschedule = false;
		bool	failed     = false;

		if (record) {
			uint64_t now    = (uint64_t)ADateTime().TimeStamp(true);
			// only update pids if there is at least 30s before programme starts
			bool     update = (SUBZ(GetStart(), now) >= 30000);

			GetRecordPIDs(pids, update);

			if (!update && (pids.CountWords() < 2)) {
				config.printf("No pids for '%s' without update", GetQuickDescription().str());
				GetRecordPIDs(pids, true);
			}

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
			dt 	  = (uint64_t)ADateTime().TimeStamp(true);

			// work out how many minutes (if any) the programme has been running
			st 	  = GetStart();
			nmins = (dt >= st) ? (uint_t)((dt - st) / 60000) : 0;

			// work out how many seconds till the recording should stop
			et 	  = GetRecordStop();
			nsecs = (et >= dt) ? (uint_t)((et - dt + 1000 - 1) / 1000) : 0;

			// if programme has been running too long, abort recording
			if (nmins >= config.GetLatestStart()) {
				config.printf("'%s' started too long ago (%u minutes)!", GetTitleAndSubtitle().str(), nmins);
				reschedule = true;
				record = false;
			}
		}

		if (record) {
			uint64_t dt, st, et;

			// get current time
			dt 	  = (uint64_t)ADateTime().TimeStamp(true);

			// work out how many ms until recording should start
			st    = GetRecordStart();

			// if there is still some time before the recording should start, delay now
			if (dt < st) {
				uint_t delay = (uint_t)(st - dt);

				config.logit("'%s' recording starts in %ums, sleeping...", GetTitleAndSubtitle().str(), delay);
				Sleep(delay);
			}
			
			// get [updated] current time
			dt 	  = (uint64_t)ADateTime().TimeStamp(true);

			// work out how many seconds till the recording should stop
			et 	  = GetRecordStop();
			nsecs = (et > dt) ? (uint_t)((et - dt + 1000 - 1) / 1000) : 0;

			// if programme has already finished, abort recording
			if (nsecs == 0) {
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

			CreateDirectory(AString(GetTempFilename()).PathPart());
			CreateDirectory(filename.PathPart());

			config.printf("Recording '%s' for %u seconds (%u minutes) to '%s' using freq %uHz PIDs %s",
						  GetTitleAndSubtitle().str(),
						  nsecs, ((nsecs + 59) / 60),
						  filename.str(),
						  (uint_t)pids.Word(0),
						  pids.Words(1).str());

			cmd = GenerateRecordCommand(nsecs, pids);

			data->actstart = ADateTime().TimeStamp(true);

			config.printf("Running '%s'", cmd.str());

			SetRecording();
			ADVBProgList::AddToList(config.GetRecordingFile(), *this);

			TriggerServerCommand(config.GetServerUpdateRecordingsCommand());

			config.printf("--------------------------------------------------------------------------------");
			config.writetorecordlog("start %s", Base64Encode().str());
			res = system(cmd);
			config.writetorecordlog("stop %s", Base64Encode().str());
			config.printf("--------------------------------------------------------------------------------");

			ADVBProgList::RemoveFromList(config.GetRecordingFile(), *this);
			ClearRecording();

			TriggerServerCommand(config.GetServerUpdateRecordingsCommand());

			if (res == 0) {
				AString  str;
				uint64_t dt = (uint64_t)ADateTime().TimeStamp(true);
				uint64_t st = GetStop();

				data->actstop = dt;

				if (rename(GetTempFilename(), GetFilename()) == 0) {
					SetRecordingComplete();

					if (dt < (st - 15000)) {
						config.printf("Warning: '%s' stopped %ss before programme end!",
									  GetTitleAndSubtitle().str(),
									  AValue((st - dt) / 1000).ToString().str());

						// force reschedule
						failed = true;
					}
					else if (!IsRecordingComplete()) {
						config.printf("Warning: '%s' is incomplete! (%ss missing from the start, %ss missing from the end)",
									  GetTitleAndSubtitle().str(),
									  AValue(MAX((sint64_t)(data->actstart - MIN(data->start, data->recstart)), 0) / 1000).ToString().str(),
									  AValue(MAX((sint64_t)(data->recstop  - data->actstop), 0) / 1000).ToString().str());

						// force reschedule
						failed = true;
					}
					else if (UpdateFileSize(nsecs)) {
						config.printf("Adding '%s' to list of recorded programmes", GetTitleAndSubtitle().str());

						ClearScheduled();
						SetRecorded();

						{
							FlagsSaver saver(this);
							ClearRunning();
							UpdateRecordedList();
						}

						if (IsOnceOnly() && IsRecordingComplete() && !config.IsRecordingSlave()) {
							ADVBPatterns::DeletePattern(user, GetPattern());

							reschedule |= config.RescheduleAfterDeletingPattern(GetUser(), GetCategory());
						}

						bool success = PostRecord();
						if (success) success = PostProcess();
						if (success) success = ConvertVideoEx();
						if (success) OnRecordSuccess();
						else		 failed  = true;
					}
					else {
						config.printf("Record of '%s' ('%s') is not valid or doesn't exist", GetTitleAndSubtitle().str(), filename.str());
						failed = true;
					}
				}
				else {
					remove(GetTempFilename());
					config.printf("Unable to rename '%s' to '%s'", GetTempFilename().str(), GetFilename());
					failed = true;
				}
			}
			else {
				remove(GetTempFilename());
				config.printf("Unable to start record of '%s'", GetTitleAndSubtitle().str());
				failed = true;
			}
		}
		else config.printf("NOT recording '%s'", GetTitleAndSubtitle().str());

		if (failed) {
			OnRecordFailure();
			reschedule = true;

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

AString ADVBProg::GetTempFilename() const
{
	return AString(GetFilename()).Prefix() + "." + tempfilesuffix;
}

AString ADVBProg::ReplaceTerms(const AString& str) const
{
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
			.SearchAndReplace("{logfile}", GetLogFile())
			.SearchAndReplace("{link}", GetLinkToFile()));
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

		success = RunCommand("nice " + cmd);
	}

	if (IsNotifySet() && (cmd = config.GetUserConfigItem(user, "notifycmd")).Valid()) {
		cmd = ReplaceTerms(cmd);

		success = RunCommand("nice " + cmd);
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

		success = RunCommand("nice " + cmd);
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

		if		(GetActualStart()) starttime = GetActualStartDT().UTCToLocal();
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
		success = RunCommand("nice " + postcmd);
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

		success = postprocessed = RunCommand("nice " + postcmd);

		SetPostProcessed();
		UpdateFileSize((uint_t)(GetActualLength() / 1000));

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

bool ADVBProg::CopyFile(AStdData& fp1, AStdData& fp2)
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

bool ADVBProg::CopyFile(const AString& src, const AString& dst, bool binary)
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

bool ADVBProg::MoveFile(const AString& src, const AString& dst, bool binary)
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

void ADVBProg::ConvertSubtitles(const AString& src, const AString& dst, const std::vector<SPLIT>& splits, const AString& aspect)
{
	const ADVBConfig& config = ADVBConfig::Get();
	FILE_INFO info;
	AString subsdir = dst.PathPart().CatPath("subs");

	if (config.ForceSubs(GetUser())) CreateDirectory(subsdir);

	if (GetFileInfo(subsdir, &info)) {
		AString srcsubfile = src.Prefix() + ".sup.idx";
		AString dstsubfile = dst.PathPart().CatPath("subs", dst.FilePart().Prefix() + ".sup.idx");
		AStdFile fp1, fp2;

		if (fp1.open(srcsubfile)) {
			CreateDirectory(dstsubfile.PathPart());
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
	AString proccmd = config.GetEncodeCommand(GetUser(), GetCategory());
	AString args    = config.GetEncodeArgs(GetUser(), GetCategory());
	AString tempdst = config.GetRecordingsStorageDir().CatPath(outputfile.FilePart().Prefix() + "_temp." + outputfile.Suffix());
	uint_t  i, n = args.CountLines(";");
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
			AString append;

			if (i) append.printf("-%u", i);

			success &= MoveFile(tempdst, outputfile.Prefix() + append + "." + outputfile.Suffix(), true);
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
	const ADVBConfig& config         = ADVBConfig::Get();
	const AString tempfilesuffix     = config.GetTempFileSuffix();
	const AString recordedfilesuffix = config.GetRecordedFileSuffix();
	const AString videofilesuffix    = config.GetVideoFileSuffix();
	const AString audiofilesuffix    = config.GetAudioFileSuffix();
	AString src  	   = GetFilename();
	AString dst  	   = GenerateFilename(true);
	AString archivedst = ReplaceFilenameTerms(config.GetRecordingsArchiveDir(GetUser()), false).CatPath(src.FilePart());

	CreateDirectory(archivedst.PathPart());

	if (IsConverted() || SameFile(src, dst)) return true;

	if (!AStdFile::exists(src)) {
		config.printf("Error: source '%s' does not exist", src.str());
		return false;
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
	AString   proccmd  = config.GetEncodeCommand(GetUser(), GetCategory());
	AString   args     = config.GetEncodeArgs(GetUser(), GetCategory());
	bool      success  = true;

	CreateDirectory(dst.PathPart());

	if (AStdFile::exists(src) &&
		!AStdFile::exists(logfile)) {
		AString cmd;

		cmd.printf("nice projectx -ini %s/X.ini \"%s\"", config.GetConfigDir().str(), src.str());

		success &= RunCommand(cmd, !verbose);
		remove(basename + ".sup");
		remove(basename + ".sup.IFO");
	}

	std::vector<SPLIT> splits;
	std::map<AString,uint64_t> lengths;
	AString bestaspect;
	std::vector<MEDIAFILE> videofiles;
	std::vector<MEDIAFILE> audiofiles;

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
					}
				}
			}

			{
				SPLIT split = {aspect, t1, 0};
				splits.push_back(split);

				config.printf("%-6s @ %s for %s", split.aspect.str(), GenTime(split.start).str(), GenTime(totallen).str());

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

		size_t i;
		config.printf("Video files:");
		for (i = 0; i < videofiles.size(); i++) {
			config.printf("%2u: PID %5u %s", (uint_t)i, videofiles[i].pid, videofiles[i].filename.str());
		}
	}
	if (audiofiles.size() > 0) {
		std::sort(audiofiles.begin(), audiofiles.end(), CompareMediaFiles);
	
		size_t i;
		config.printf("Audio files:");
		for (i = 0; i < audiofiles.size(); i++) {
			config.printf("%2u: PID %5u %s", (uint_t)i, audiofiles[i].pid, audiofiles[i].filename.str());
		}
	}

	AString m2vfile;
	AString mp2file;
	uint_t  videotrack = (uint_t)GetHierarchicalConfigItem(videotrackname, "0");

	if (videotrack < (uint_t)videofiles.size()) {
		m2vfile = videofiles[videotrack].filename;
		config.printf("Using video track %u of '%s', file '%s'", videotrack, GetQuickDescription().str(), m2vfile.str());
	}
	else if (videofiles.size()) {
		m2vfile = videofiles[0].filename;
		config.printf("Video track %u of '%s' doesn't exists, using file '%s' instead", videotrack, GetQuickDescription().str(), m2vfile.str());
	}
	else config.printf("Warning: no video file(s) associated with '%s'", GetQuickDescription().str());

	uint_t audiotrack = (uint_t)GetHierarchicalConfigItem(audiotrackname, "0");
	
	if (audiotrack < (uint_t)audiofiles.size()) {
		mp2file = audiofiles[audiotrack].filename;
		config.printf("Using audio track %u of '%s', file '%s'", audiotrack, GetQuickDescription().str(), mp2file.str());
	}
	else if (audiofiles.size()) {
		mp2file = audiofiles[0].filename;
		config.printf("Audio track %u of '%s' doesn't exists, using file '%s' instead", audiotrack, GetQuickDescription().str(), mp2file.str());
	}
	else config.printf("Warning: no audio file(s) associated with '%s'", GetQuickDescription().str());
	
	if (success) {
		if (m2vfile.Empty() && mp2file.Valid()) {
			config.printf("No video: audio only");

			dst = dst.Prefix() + "." + config.GetAudioDestFileSuffix();
			AString tempdst = config.GetRecordingsStorageDir().CatPath(dst.FilePart().Prefix() + "_temp." + dst.Suffix());

			AString cmd;
			cmd.printf("nice %s -i \"%s\" -v %s %s -y \"%s\"",
					   config.GetEncodeCommand(GetUser(), GetCategory()).str(),
					   mp2file.str(),
					   config.GetEncodeLogLevel(GetUser(), verbose).str(),
					   config.GetEncodeAudioOnlyArgs(GetUser(), GetCategory()).str(),
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
			success &= EncodeFile(inputfiles, bestaspect, dst, verbose);
		}
		else {
			AString remuxsrc = basename + "_Remuxed." + recordedfilesuffix;

			config.printf("Splitting file...");

			if (!AStdFile::exists(remuxsrc)) {
				AString cmd;

				cmd.printf("nice %s -fflags +genpts -i \"%s.%s\" -i \"%s.%s\" -acodec copy -vcodec copy -v warning -f mpegts \"%s\"",
						   proccmd.str(),
						   basename.str(), videofilesuffix.str(),
						   basename.str(), audiofilesuffix.str(),
						   remuxsrc.str());

				success &= RunCommand(cmd, !verbose);
			}

			std::vector<AString> files;
			uint_t i;

			for (i = 0; i < splits.size(); i++) {
				const SPLIT& split = splits[i];

				if (split.aspect == bestaspect) {
					AString cmd;
					AString outfile;

					outfile.printf("%s-%s-%u.%s", basename.str(), bestaspect.SearchAndReplace(":", "_").str(), i, recordedfilesuffix.str());

					if (!AStdFile::exists(outfile)) {
						cmd.printf("nice %s -fflags +genpts -i \"%s\" -ss %s", proccmd.str(), remuxsrc.str(), GenTime(split.start).str());
						if (split.length > 0) cmd.printf(" -t %s", GenTime(split.length).str());
						cmd.printf(" -acodec copy -vcodec copy -v warning -y -f mpegts \"%s\"", outfile.str());

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
				success &= EncodeFile(inputfiles, bestaspect, dst, verbose);
			}

			remove(concatfile);
			remove(remuxsrc);

			for (i = 0; i < files.size(); i++) {
				remove(files[i]);
			}
		}
	}

	if (success) {
		SetFilename(dst);

		UpdateFileSize((uint_t)(GetActualLength() / 1000));

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
		
		CollectFiles(basename.PathPart(), basename.FilePart() + ".sup.*", RECURSE_ALL_SUBDIRS, delfiles);
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
	}
	else config.logit("Process failed!");

	return success;
}

bool ADVBProg::ConvertVideo(bool verbose, bool cleanup, bool force)
{
	const ADVBConfig& config = ADVBConfig::Get();
	bool success = ConvertVideoEx(verbose, cleanup, force);

	if (!success) {
		OnRecordFailure();

		ClearScheduled();
		SetRecordingFailed();

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
	AString src 	= GetArchiveRecordingFilename();
	AString dst 	= GetTempRecordingFilename();
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

AString ADVBProg::GetArchiveRecordingFilename() const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString filename = GetFilename();
	return ReplaceFilenameTerms(config.GetRecordingsArchiveDir(GetUser()), false).CatPath(filename.FilePart().Prefix() + "." + recordedfilesuffix);
}

AString ADVBProg::GetTempRecordingFilename() const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString filename = GetFilename();
	return ReplaceFilenameTerms(config.GetRecordingsStorageDir(), false).CatPath(filename.FilePart().Prefix() + "." + recordedfilesuffix);
}
