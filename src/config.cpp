
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <rdlib/Regex.h>
#include <rdlib/Recurse.h>

#include "config.h"
#include "findcards.h"

#define DEFAULTCONFDIR "/etc/dvb"
#define DEFAULTDATADIR "/var/dvb"
#define DEFAULTLOGDIR  "/var/log/dvb"

ADVBConfig::ADVBConfig() : config(AString(DEFAULTCONFDIR).CatPath("dvb"), false),
						   defaults(20, &AString::DeleteString),
						   webresponse(false)
{
	static const struct {
		const char *name;
		const char *value;
	} __defaults[] = {
		{"prehandle", "2"},
		{"posthandle", "3"},
		{"pri", "0"},
		{"dir", ""},
		{"h264crf",      	 "19"},
		{"maxvideorate", 	 "1200k"},
		{"aacbitrate", 	 	 "160k"},
		{"mp3bitrate", 	 	 "160k"},
		{"copyvideo", 	 	 "-vcodec copy"},
		{"copyaudio", 	 	 "-acodec copy"},
		{"mp3audio",  	 	 "-acodec mp3 -b:a {conf:mp3bitrate}"},
		{"h264preset",   	 "veryfast"},
		{"h264bufsize",  	 "2000k"},
		{"videodeinterlace", "yadif"},
		{"videofilter",  	 "-filter:v {conf:videodeinterlace}"},
		{"filters",		 	 "{conf:videofilter} {conf:audiofilter}"},
		{"encodeflags",	 	 "-movflags +faststart"},
		{"h264video", 	 	 "-vcodec libx264 -preset {conf:h264preset} -crf {conf:h264crf} -maxrate {conf:maxvideorate} -bufsize {conf:h264bufsize} {conf:encodeflags} {conf:filters}"},
		{"aacaudio",  	 	 "-acodec libfdk_aac -b:a {conf:aacbitrate}"},
		{"encodecopy",   	 "{conf:copyvideo} {conf:mp3audio}"},
		{"encodeh264",   	 "{conf:h264video} {conf:aacaudio}"},
		{"encodeargs",   	 "{conf:encodeh264}"},
	};
	uint_t i;

	signal(SIGINT, &__hasquit);
	signal(SIGPIPE, &__hasquit);
	signal(SIGHUP, &__hasquit);

	for (i = 0; i < NUMBEROF(__defaults); i++) {
		defaults.Insert(__defaults[i].name, (uptr_t)new AString(__defaults[i].value));
	}
	
	CreateDirectory(GetConfigDir());
	CreateDirectory(GetDataDir());
	CreateDirectory(GetLogDir());
	CreateDirectory(GetRecordingsDir());
	CreateDirectory(GetTempDir());
	
	{
		AList users;

		ListUsers(users);

		const AString *user = AString::Cast(users.First());
		while (user) {
			CreateDirectory(GetRecordingsDir(*user));
			CreateDirectory(GetRecordingsStorageDir(*user));
			
			user = user->Next();
		}
	}
}

const ADVBConfig& ADVBConfig::Get(bool webresponse)
{
	return (const ADVBConfig &)ADVBConfig::GetWriteable(webresponse);
}

ADVBConfig& ADVBConfig::GetWriteable(bool webresponse)
{
	static ADVBConfig config;
	if (webresponse) config.WebResponse();
	return config;
}

const AString& ADVBConfig::GetDefaultInterRecTime() const
{
	static const AString rectime = "10";
	return rectime;
}

AString ADVBConfig::GetConfigDir() const
{
	return DEFAULTCONFDIR;
}

AString ADVBConfig::GetDataDir() const
{
	return GetConfigItem("datadir", DEFAULTDATADIR);
}

AString ADVBConfig::GetLogDir() const
{
	return GetConfigItem("logdir", DEFAULTLOGDIR);
}

AString ADVBConfig::GetTempFile(const AString& name, const AString& suffix) const
{
	return GetTempDir().CatPath(AString("%s_%08lx%s").Arg(name).Arg(::GetTickCount()).Arg(suffix));
}

void ADVBConfig::CheckUpdate() const
{
	ASettingsHandler& wrconfig = *const_cast<ASettingsHandler *>(&config);
	if (wrconfig.CheckRead()) wrconfig.Read();
}

AString ADVBConfig::GetConfigItem(const AString& name) const
{
	const AString *def = NULL;
	AString res;
	
	CheckUpdate();
	
	if (config.Exists(name)) res = config.Get(name);
	else if ((def = (const AString *)defaults.Read(name)) != NULL) res = *def;

	//debug("Request for '%s' (def: '%s'): '%s'\n", name.str(), def ? def->str() : "<notset>", res.str());

	return res;
}

AString ADVBConfig::GetConfigItem(const AString& name, const AString& defval) const
{
	AString res;

	CheckUpdate();

	if (config.Exists(name)) res = config.Get(name);
	else					 res = defval;
	
	//debug("Request for '%s' (def: '%s'): '%s'\n", name.str(), defval.str(), res.str());

	return res;
}

AString ADVBConfig::GetNamedFile(const AString& name) const
{
	AString filename;

	if		(name == "listings")			  filename = GetListingsFile();
	else if (name == "scheduled")  			  filename = GetScheduledFile();
	else if (name == "recorded")   			  filename = GetRecordedFile();
	else if (name == "requested")  			  filename = GetRequestedFile();
	else if (name == "failures")   			  filename = GetRecordFailuresFile();
	else if (name == "rejected")   			  filename = GetRejectedFile();
	else if (name == "combined")   			  filename = GetCombinedFile();
	else if (name == "processing") 			  filename = GetProcessingFile();
	else if (name == "extrarecordprogrammes") filename = GetExtraRecordFile();
	else									  filename = name;
	
	return filename;
}

AString ADVBConfig::GetRelativePath(const AString& filename) const
{
	AString res;

	if (filename.StartsWith(GetRecordingsDir())) res = AString("/videos").CatPath(filename.Mid(GetRecordingsDir().len()));
	
	return res;
}

AString ADVBConfig::ReplaceTerms(const AString& user, const AString& _str) const
{
	AString str = _str;
	int p1, p2;
	
	while (((p1 = str.Pos("{conf:")) >= 0) && ((p2 = str.Pos("}", p1)) >= 0)) {
		AString item = GetUserConfigItem(user, str.Mid(p1 + 6, p2 - p1 - 6));

		str = str.Left(p1) + item + str.Mid(p2 + 1);
	}

	return str;
}

AString ADVBConfig::ReplaceTerms(const AString& user, const AString& subitem, const AString& _str) const
{
	AString str = _str;
	int p1, p2;
	
	while (((p1 = str.Pos("{conf:")) >= 0) && ((p2 = str.Pos("}", p1)) >= 0)) {
		AString item = GetUserSubItemConfigItem(user, subitem, str.Mid(p1 + 6, p2 - p1 - 6));

		str = str.Left(p1) + item + str.Mid(p2 + 1);
	}

	return str;
}

void ADVBConfig::MapDVBCards()
{
	uint_t i, n = GetMaxDVBCards();

	printf("Finding mappings for %u DVB cards", n);

	for (i = 0; i < n; i++) {
		AString defname;
		AString cardname;
		sint_t  card = -1;

		if (i == 0) defname = "Conexant*";

		cardname = GetConfigItem(AString("card%u").Arg(n), defname);
	
		if ((card = findcard(cardname)) >= 0) {
			dvbcards.Add((uint_t)card);
		}
		else {
			printf("Failed to find DVB card '%s' (card %u)", cardname.str(), i);
			
			uint_t j;
			for (j = 0; j < n; j++) {
				if (dvbcards.Find(j) < 0) {
					dvbcards.Add(j);
					break;
				}
			}

			if (j == n) dvbcards.Add(~0);
		}
	}
	printf("DVB card mapping:");
	for (i = 0; i < dvbcards.Count(); i++) {
		printf("Virtual card %u -> physical card %u", i, (uint_t)dvbcards[i]);
	}
}

uint_t ADVBConfig::GetPhysicalDVBCard(uint_t n) const
{
	if (dvbcards.Count() == 0) {
		(const_cast<ADVBConfig *>(this))->MapDVBCards();
	}

	return (n < dvbcards.Count()) ? dvbcards[n] : 0;
}

void ADVBConfig::logit(const char *fmt, ...) const
{
	va_list ap;

	va_start(ap, fmt);
	vlogit(fmt, ap);
	va_end(ap);
}

void ADVBConfig::printf(const char *fmt, ...) const
{
	va_list ap;

	va_start(ap, fmt);
	vlogit(fmt, ap, true);
	va_end(ap);
}

void ADVBConfig::vlogit(const char *fmt, va_list ap, bool show) const
{
	ADateTime dt;
	AString   filename = GetLogFile(dt.GetDays());
	AString   str;
	AStdFile  fp;

	str.vprintf(fmt, ap);
	
	if (fp.open(filename, "a")) {
		fp.printf("%s: %s\n", dt.DateFormat("%Y-%M-%D %h:%m:%s.%S").str(), str.str());
		fp.close();
	}

	if (show && !webresponse) {
		Stdout->printf("%s\n", str.str());
	}
}

void ADVBConfig::writetorecordlog(const char *fmt, ...) const
{
	ADateTime dt;
	AStdFile fp;

	if (fp.open(GetRecordLog(dt.GetDays()), "a")) {
		va_list ap;

		fp.printf("%s: ", dt.DateFormat("%Y-%M-%D %h:%m:%s.%S").str());

		va_start(ap, fmt);
		fp.vprintf(fmt, ap);
		va_end(ap);

		fp.printf("\n");

		fp.close();
	}
}

void ADVBConfig::ListUsers(AList& list) const
{
	const AStringPairWithInt *item = config.GetFirst();
	AHash 	users(10);
	AList 	userpatterns;
	AString filepattern 	   = GetUserPatternsPattern();
	AString filepattern_parsed = ParseRegex(filepattern);

	//debug("Reading users from config %s\n", config.GetFilename().str());

	while (item) {
		if (item->Integer == ASettingsHandler::PairType_Value) {
			const AString& str = item->String1;
			AString word = str.Word(0);
			int p;

			//debug("Item: %s=%s\n", item->String1.str(), item->String2.str());

			if ((p = word.Pos(":")) >= 0) {
				AString user = word.Left(p);

				if (!users.Exists(user)) {
					users.Insert(user, 0);
					list.Add(new AString(user));
				}
			}
		}

		item = item->Next();
	}

	::CollectFiles(filepattern.PathPart(), filepattern.FilePart(), 0, userpatterns);

	const AString *file = AString::Cast(userpatterns.First());
	while (file) {
		AString   user;
		ADataList regions;

		if (MatchRegex(*file, filepattern_parsed, regions)) {
			const REGEXREGION *region = (const REGEXREGION *)regions[0];

			if (region) {
				user = file->Mid(region->pos, region->len);
				if (!users.Exists(user)) {
					users.Insert(user, 0);
					list.Add(new AString(user));
				}
			}
		}

		file = file->Next();
	}

	list.Sort(&AString::AlphaCompareCase);
}

bool ADVBConfig::ExtractLogData(const ADateTime& start, const ADateTime& end, const AString& filename) const
{
	AStdFile dst;
	bool success = false;
	
	if (dst.open(filename, "w")) {
		ADateTime dt;
		uint32_t day1 = start.GetDays();
		uint32_t day2 = end.GetDays();
		uint32_t day;
		bool     valid = false;
		
		for (day = day1; day <= day2; day++) {
			AStdFile src;

			if (src.open(GetLogFile(day))) {
				AString line;

				while (line.ReadLn(src) >= 0) {
					valid |= dt.FromTimeStamp(line.Words(0, 2));

					if (valid) {
						if ((dt >= start) && (dt <= end)) dst.printf("%s\n", line.str());
						if (dt >= end) break;
					}
				}
				
				src.close();
			}
		}
		
		dst.close();
	}

	return success;
}

