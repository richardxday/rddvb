
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <rdlib/Regex.h>
#include <rdlib/Recurse.h>

#include "config.h"
#include "findcards.h"

#define DEFAULTCONFDIR "/etc/dvb"
#define DEFAULTDATADIR "/var/dvb"
#define DEFAULTLOGDIR  "/var/log/dvb"

AQuitHandler quithandler;

ADVBConfig::ADVBConfig() : config(AString(DEFAULTCONFDIR).CatPath("dvb"), false),
						   defaults(&AString::DeleteString),
						   configrecorder(NULL),
						   webresponse(false)
{
	const struct passwd *pw = getpwuid(getuid());
	static const struct {
		const char *name;
		const char *value;
	} __defaults[] = {
		{"homedir",						 pw->pw_dir},
		{"prehandle",  		 			 "2"},
		{"posthandle", 		 			 "3"},
		{"pri", 	   		 			 "0"},
		{"dir",              			 "{titledir}"},
		{"h264crf",      	 			 "17"},
		{"maxvideorate", 	 			 "2000k"},
		{"aacbitrate", 	 	 			 "160k"},
		{"mp3bitrate", 	 	 			 "160k"},
		{"copyvideo", 	 	 			 "-vcodec copy"},
		{"copyaudio", 	 	 			 "-acodec copy"},
		{"mp3audio",  	 	 			 "-acodec mp3 -b:a {conf:mp3bitrate}"},
		{"h264preset",   	 			 "veryfast"},
		{"h264bufsize",  	 			 "3000k"},
		{"videodeinterlace", 			 "yadif"},
		{"videofilter",  	 			 "-filter:v {conf:videodeinterlace}"},
		{"filters",		 	 			 "{conf:videofilter} {conf:audiofilter}"},
		{"encodeflags",	 	 			 "-movflags +faststart"},
		{"h264video", 	 	 			 "-vcodec libx264 -preset {conf:h264preset} -crf {conf:h264crf} -maxrate {conf:maxvideorate} -bufsize {conf:h264bufsize} {conf:encodeflags} {conf:filters}"},
		{"aacaudio",  	 	 			 "-acodec libfdk_aac -b:a {conf:aacbitrate}"},
		{"encodecopy",   	 			 "{conf:copyvideo} {conf:mp3audio}"},
		{"encodeh264",   	 			 "{conf:h264video} {conf:aacaudio}"},
		{"encodeargs",   	 			 "{conf:encodeh264}"},
		{"encodeaudioonlyargs", 		 "{conf:mp3audio}"},
		{"episodefirstfilenametemplate", "{title}{sep}{episode}{episodeid}{sep}{date}{sep}{times}{sep}{subtitle}"},
		{"episodelastfilenametemplate",  "{title}{sep}{episode}{sep}{date}{sep}{times}{sep}{episodeid}{sep}{subtitle}"},
		{"dir",							 "{capitaluser}/{titledir}"},
		{"dir:film",					 "Films"},
	};
	uint_t i;

	for (i = 0; i < NUMBEROF(__defaults); i++) {
		AString *str = new AString(__defaults[i].value);
		defaults.Insert(__defaults[i].name, (uptr_t)str);
	}

	// ensure no changes are saved
	config.EnableWrite(false);
	
	CreateDirectory(GetConfigDir());
	CreateDirectory(GetDataDir());
	CreateDirectory(GetLogDir());
	CreateDirectory(GetRecordingsStorageDir());
	CreateDirectory(GetRecordingsDir());
	CreateDirectory(GetTempDir());

	if (CommitScheduling()) {
		AList   users;
		AString dir;

		ListUsers(users);

		const AString *user = AString::Cast(users.First());
		while (user) {
			if (((dir = GetRecordingsDir(*user)).Valid())   && (dir.Pos("{") < 0)) CreateDirectory(dir);
			if (((dir = GetRecordingsArchiveDir()).Valid()) && (dir.Pos("{") < 0)) CreateDirectory(dir);

			user = user->Next();
		}
	}
}

ADVBConfig::~ADVBConfig()
{
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

AString ADVBConfig::GetShareDir() const
{
	return GetConfigItem("sharedir", RDDVB_SHARE_DIR);
}

AString ADVBConfig::GetTempFile(const AString& name, const AString& suffix) const
{
	return GetTempDir().CatPath(AString("%_%08x_%08x%").Arg(name).Arg(getpid()).Arg(::GetTickCount()).Arg(suffix));
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

	if (def && configrecorder) configrecorder->push_back(name + "=" + *def);
	
	//debug("Request for '%s' (implicit default: '%s'): '%s'\n", name.str(), def ? def->str() : "<notset>", res.str());

	return res;
}

AString ADVBConfig::GetDefaultItem(const AString& name) const
{
	const AString *str;
	return ((str = (const AString *)defaults.Read(name)) != NULL) ? *str : "";
}

AString ADVBConfig::GetConfigItem(const AString& name, const AString& defval) const
{
	AString res;

	CheckUpdate();

	if (config.Exists(name)) res = config.Get(name);
	else					 res = defval;

	if (configrecorder) configrecorder->push_back(name + "=" + defval);

	//debug("Request for '%s' (with default: '%s'): '%s'\n", name.str(), defval.str(), res.str());

	return res;
}

AString ADVBConfig::GetConfigItem(const std::vector<AString>& list, const AString& defval, bool defvalid) const
{
	const AString *def;
	AString res = defval;
	size_t  i;

	for (i = 0; i < list.size(); i++) {
		const AString& name = list[i];

		//debug("Checking config for '%s'\n", name.str());
		if (config.Exists(name)) {
			res = config.Get(name);
			//debug("Config for '%s' exists: '%s'\n", name.str(), res.str());
			break;
		}
		else if (!defvalid && ((def = (const AString *)defaults.Read(name)) != NULL)) {
			res = *def;
			//debug("Config for '%s' missing, using default: '%s'\n", name.str(), res.str());
			break;
		}
	}

	return res;
}

void ADVBConfig::GetCombinations(std::vector<AString>& list, const AString& pre, const AString& name, const AString& post) const
{
	if (pre.Valid() && name.Valid() && post.Valid()) list.push_back(Combine(Combine(pre, name), post));
	if (pre.Valid() && name.Valid())				 list.push_back(Combine(pre, name));
	if (			   name.Valid() && post.Valid()) list.push_back(Combine(name, post));
	list.push_back(name);
}

AString ADVBConfig::GetHierarchicalConfigItem(const AString& pre, const AString& name, const AString& post) const
{
	std::vector<AString> list;
	
	GetCombinations(list, pre, name, post);

	return GetConfigItem(list);
}

AString ADVBConfig::GetHierarchicalConfigItem(const AString& pre, const AString& name, const AString& post, const AString& defval) const
{
	std::vector<AString> list;
	
	GetCombinations(list, pre, name, post);

	return GetConfigItem(list, defval, true);
}

AString ADVBConfig::GetUserConfigItem(const AString& user, const AString& name) const
{
	return GetHierarchicalConfigItem(user, name, "");
}

AString ADVBConfig::GetUserConfigItem(const AString& user, const AString& name, const AString& defval) const
{
	return GetHierarchicalConfigItem(user, name, "", defval);
}

AString ADVBConfig::GetUserSubItemConfigItem(const AString& user, const AString& subitem, const AString& name) const
{
	return GetHierarchicalConfigItem(user, name, subitem);
}

AString ADVBConfig::GetUserSubItemConfigItem(const AString& user, const AString& subitem, const AString& name, const AString& defval) const
{
	return GetHierarchicalConfigItem(user, name, subitem, defval);
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
	uint_t i;

	dvbcards.DeleteList();

	for (i = 0; i < 4; i++) {
		AString defname = "*";
		AString cardname;
		sint_t  card = -1;

		cardname = GetConfigItem(AString("card%;").Arg(i), defname);

		if ((card = findcard(cardname, &dvbcards)) >= 0) {
			dvbcards.Add((uint_t)card);
		}
		else break;
	}

	printf("DVB card mapping:");
	for (i = 0; i < dvbcards.Count(); i++) {
		printf("Virtual card %u -> physical card %u", i, (uint_t)dvbcards[i]);
	}
}

uint_t ADVBConfig::GetPhysicalDVBCard(uint_t n, bool forcemapping) const
{
	if (forcemapping || !dvbcards.Count()) {
		(const_cast<ADVBConfig *>(this))->MapDVBCards();
	}

	return dvbcards[n];
}

uint_t ADVBConfig::GetVirtualDVBCard(uint_t n) const
{
	sint_t n1 = dvbcards.Find(n);

	return (n1 >= 0) ? n1 : ~0;
}

void ADVBConfig::Set(const AString& var, const AString& val) const
{
	// must cheat const system
	ASettingsHandler *pconfig = const_cast<ASettingsHandler *>(&config);
	pconfig->Set(var, val);
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
		uint_t i, n = str.CountLines("\n", 0);

		for (i = 0; i < n; i++) {
			fp.printf("%s [%05u]: %s\n", dt.DateFormat("%Y-%M-%D %h:%m:%s.%S").str(), (uint_t)getpid(), str.Line(i, "\n", 0).str());
		}

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
	AHash 	 users;
	AList 	 userpatterns;
	AString  filepattern 	   = GetUserPatternsPattern();
	AString  filepattern_parsed = ParseRegex(filepattern);
	AString  _users             = GetConfigItem("users");
	AStdFile fp;
	uint_t   i, n = _users.CountColumns();

	//debug("Reading users from config %s\n", config.GetFilename().str());

	for (i = 0; i < n; i++) {
		AString user = _users.Column(i).Words(0);

		if (!users.Exists(user)) {
			users.Insert(user, 0);
			list.Add(new AString(user));
		}
	}

	if (fp.open(GetPatternsFile())) {
		AString line;

		while (line.ReadLn(fp) >= 0) {
			AString user;
			int p;

			if		((p = line.PosNoCase(" user:=")) >= 0) user = line.Mid(p + 7).Word(0).DeQuotify();
			else if (line.PosNoCase("user:=") == 0)        user = line.Mid(6).Word(0).DeQuotify();

			if (user.Valid() && !users.Exists(user)) {
				users.Insert(user, 0);
				list.Add(new AString(user));
			}
		}

		fp.close();
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

bool ADVBConfig::ReadReplacementsFile(std::vector<REPLACEMENT>& replacements, const AString& filename) const
{
	AStdFile fp;
	bool success = false;

	if (fp.open(filename)) {
		AString line;

		//printf("Reading replacements file '%s':", filename.str());

		while (line.ReadLn(fp) >= 0) {
			int p = 0;

			if ((line.Word(0)[0] != ';') && ((p = line.Pos("=")) >= 0)) {
				REPLACEMENT repl = {
					line.Left(p).Word(0).DeQuotify(),
					line.Mid(p + 1).Word(0).DeQuotify(),
				};

				//printf("Replacement: '%s' -> '%s'", repl.search.str(), repl.replace.str());

				replacements.push_back(repl);
			}
		}

		fp.close();

		success = true;
	}
	else logit("Failed to open replacements file '%s'", filename.str());

	return success;
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

// NOTE: function cheats 'const'!
void ADVBConfig::SetConfigRecorder(std::vector<AString> *recorder) const
{
	// must cheat const system
	ADVBConfig *pconfig = const_cast<ADVBConfig *>(this);
	pconfig->configrecorder = recorder;
}
