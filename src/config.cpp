
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
						   webresponse(false),
						   additionallogneedstimestamps(false)
{
	static const struct {
		const char *name;
		const char *value;
	} __defaults[] = {
		{"prehandle", "2"},
		{"posthandle", "3"},
		{"pri", "0"},
		{"dir", ""},
	};
	uint_t i;

	signal(SIGINT, &__hasquit);
	signal(SIGPIPE, &__hasquit);
	signal(SIGHUP, &__hasquit);

	for (i = 0; i < NUMBEROF(__defaults); i++) {
		defaults.Insert(__defaults[i].name, (uptr_t)new AString(__defaults[i].value));
	}
	
	additionallogfilename = GetConfigItem("additionallogfile");

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

AString ADVBConfig::GetConfigItem(const AString& name) const
{
	if (config.Exists(name)) return config.Get(name);

	const AString *def = (const AString *)defaults.Read(name);
	return def ? *def : AString();
}

AString ADVBConfig::GetConfigItem(const AString& name, const AString& defval) const
{
	if (config.Exists(name)) return config.Get(name);
	return defval;
}

AString ADVBConfig::GetFileSuffix(const AString& user, const AString& def) const
{
	AString format = GetFileFormat(user);
	AString proc   = GetUserConfigItem(user, "proc", GetUserConfigItem(user, "proc:" + format, ""));
	AString suffix;
	
	if		(proc.Valid()) suffix = GetUserConfigItem(user, "proc:suffix", GetUserConfigItem(user, "filesuffix", def));
	else if ((format == "h264") || (format == "dvb")) {
		suffix = GetUserConfigItem(user, format + ":suffix", GetUserConfigItem(user, "filesuffix", "mp4"));
	}
	else if (format.Valid()) suffix = GetUserConfigItem(user, format + ":suffix", def);
	else				     suffix = GetUserConfigItem(user, "filesuffix", def);

	return suffix;
}

AString ADVBConfig::GetProcessingCommand(const AString& user, const AString& filename) const
{
	AString format = GetFileFormat(user);
	AString proc   = GetUserConfigItem(user, "proc", GetUserConfigItem(user, "proc:" + format, ""));
	AString cmd;

	if		(proc.Valid()) cmd = proc;
	else if (format == "h264") {
		cmd.printf("avconv -i - -acodec %s -vcodec libx264 -crf %s -preset %s -tune %s -filter:v %s -v %s %s %s -y \"%s\"",
				   GetUserConfigItem(user, format + ":acodec", 	 "mp3").str(),
				   GetUserConfigItem(user, format + ":crf",    	 "18").str(),
				   GetUserConfigItem(user, format + ":preset", 	 "veryfast").str(),
				   GetUserConfigItem(user, format + ":tune",   	 "animation").str(),
				   GetUserConfigItem(user, "video:filter",       "yadif").str(),
				   GetUserConfigItem(user, "video:warninglevel", "warning").str(),
				   GetUserConfigItem(user, "video:args",         "").str(),
				   GetUserConfigItem(user, format + ":args",     "").str(),
				   filename.str());
	}
	else if (format == "dvb") {
		cmd.printf("avconv -i - -acodec copy -vcodec copy -filter:v %s -v %s %s %s -y \"%s\"",
				   GetUserConfigItem(user, "video:filter",       "yadif").str(),
				   GetUserConfigItem(user, "video:warninglevel", "warning").str(),
				   GetUserConfigItem(user, "video:args",         "").str(),
				   GetUserConfigItem(user, format + ":args",     "").str(),
				   filename.str());
	}
	else if (format == "avconv") {
		cmd.printf("avconv -i - -acodec %s -vcodec %s -filter:v %s -v %s %s %s -y \"%s\"",
				   GetUserConfigItem(user, format + ":acodec",   "copy").str(),
				   GetUserConfigItem(user, format + ":vcodec",   "copy").str(),
				   GetUserConfigItem(user, "video:filter",       "yadif").str(),
				   GetUserConfigItem(user, "video:warninglevel", "warning").str(),
				   GetUserConfigItem(user, "video:args",         "").str(),
				   GetUserConfigItem(user, format + ":args",     "").str(),
				   filename.str());
	}

	return ReplaceTerms(user, cmd);
}

AString ADVBConfig::ReplaceTerms(const AString& user, const AString& _str) const
{
	AString str = _str;
	int p = 0, p1, p2;
	
	while (((p1 = str.Pos("{conf:", p)) >= 0) && ((p2 = str.Pos("}", p1)) >= 0)) {
		AString item = GetUserConfigItem(user, str.Mid(p1 + 8, p2 - p1 - 8));

		str = str.Left(p1) + item + str.Mid(p2 + 1);

		p = p1 + item.len();
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

	if (additionallogfilename.Valid() && fp.open(additionallogfilename, "a")) {
		if (additionallogneedstimestamps) {
			fp.printf("%s: ", dt.DateFormat("%Y-%M-%D %h:%m:%s.%S").str());
		}
		fp.printf("%s\n", str.str());
		fp.close();
	}

	if (show && !webresponse) {
		Stdout->printf("%s\n", str.str());
	}
}

void ADVBConfig::addlogit(const char *fmt, ...) const
{
	AStdFile fp;

	if (additionallogfilename.Valid() && fp.open(additionallogfilename, "a")) {
		va_list ap;

		if (additionallogneedstimestamps) {
			fp.printf("%s: ", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s.%S").str());
		}

		va_start(ap, fmt);
		fp.vprintf(fmt, ap);
		va_end(ap);

		fp.close();
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
