
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <algorithm>

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
						   disableoutput(false)
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
		{"mindatarate:mpg",				 "12"},
		{"mindatarate:mp4",				 "100"},
		{"mindatarate:mp3",				 "12"},
		{"mindatarate",					 "10"},
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

/*--------------------------------------------------------------------------------*/
/** Singleton access
 */
/*--------------------------------------------------------------------------------*/
const ADVBConfig& ADVBConfig::Get(bool disableoutput)
{
	return (const ADVBConfig &)ADVBConfig::GetWriteable(disableoutput);
}

/*--------------------------------------------------------------------------------*/
/** Writable singleton access (for updating object, NOT dvb.conf)
 */
/*--------------------------------------------------------------------------------*/
ADVBConfig& ADVBConfig::GetWriteable(bool disableoutput)
{
	static ADVBConfig config;
	if (disableoutput) config.DisableOutput();
	return config;
}

/*--------------------------------------------------------------------------------*/
/** Check to see if dvb.conf has changed and if so, re-read it
 */
/*--------------------------------------------------------------------------------*/
void ADVBConfig::CheckUpdate() const
{
	ASettingsHandler *wrconfig = const_cast<ASettingsHandler *>(&config);
	if (wrconfig->CheckRead()) wrconfig->Read();
}

/*--------------------------------------------------------------------------------*/
/** Return system default value for config item or NULL
 *
 * @param name config item name
 *
 * @return ptr to system default or NULL
 */
/*--------------------------------------------------------------------------------*/
const AString *ADVBConfig::GetDefaultItemEx(const AString& name) const
{
	return (const AString *)defaults.Read(name);
}

/*--------------------------------------------------------------------------------*/
/** Return system default value for config item
 *
 * @param name config item name
 *
 * @return system default or empty string
 */
/*--------------------------------------------------------------------------------*/
AString ADVBConfig::GetDefaultItem(const AString& name) const
{
	const AString *str = GetDefaultItemEx(name);;
	return str ? *str : "";
}

/*--------------------------------------------------------------------------------*/
/** Return value config item value or system default value
 *
 * @param name config item name
 *
 * @return config item value, system default or empty string
 */
/*--------------------------------------------------------------------------------*/
AString ADVBConfig::GetConfigItem(const AString& name) const
{
	const AString *def = NULL;
	AString res;

	CheckUpdate();

	if (config.Exists(name)) res = config.Get(name);
	else if ((def = GetDefaultItemEx(name)) != NULL) {
		res = *def;
		if (configrecorder) configrecorder->push_back(name + "=" + *def);
	}

	//debug("Request for '%s' (implicit default: '%s'): '%s'\n", name.str(), def ? def->str() : "<notset>", res.str());

	return res;
}

/*--------------------------------------------------------------------------------*/
/** Return config item value or the specified default
 *
 * @param name config item name
 * @param defval default value
 *
 * @return config item value, specified default or empty string
 *
 * @note this will *never* return the system default for the config item because the default is explicit (but the caller could use GetDefaultItem())
 */
/*--------------------------------------------------------------------------------*/
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

/*--------------------------------------------------------------------------------*/
/** Get value from list of config items, a specified default or system default 
 *
 * @param list list of config items
 * @param defval an explicit default value
 * @param defvalid true if defval is valid
 *
 * @return value
 *
 * @note the value returned is the first of:
 * @note 1. the value of the first config item in list found in dvb.conf; or
 * @note 2. defval if defvalid == true; or
 * @note 3. the value of the first config item in list with a valid system default
 */
/*--------------------------------------------------------------------------------*/
AString ADVBConfig::GetConfigItem(const std::vector<AString>& list, const AString& defval, bool defvalid) const
{
	AString res;
	size_t  i;

	for (i = 0; i < list.size(); i++) {
		const AString& name = list[i];

		if (config.Exists(name)) {
			res = config.Get(name);
			break;
		}
	}

	if (i == list.size()) {
		if (defvalid) {
			res = defval;
		}
		else {
			for (i = 0; i < list.size(); i++) {
				const AString& name = list[i];
				const AString *def;
			
				if ((def = GetDefaultItemEx(name)) != NULL) {
					res = *def;
					break;
				}
			}
		}
	}

	return res;
}

/*--------------------------------------------------------------------------------*/
/** Get possible valid combinations of <pre>/<name>/<post>
 *
 * @param list destination of list of valid combinations
 * @param pre optional config item prefix
 * @param name config item
 * @param post optional config item postfix
 */
/*--------------------------------------------------------------------------------*/
void ADVBConfig::GetCombinations(std::vector<AString>& list, const AString& pre, const AString& name, const AString& post) const
{
	if (pre.Valid() && name.Valid() && post.Valid()) list.push_back(Combine(Combine(pre, name), post));
	if (pre.Valid() && name.Valid())				 list.push_back(Combine(pre, name));
	if (			   name.Valid() && post.Valid()) list.push_back(Combine(name, post));
	list.push_back(name);
}

/*--------------------------------------------------------------------------------*/
/** Return the value of the pre/post config item or system default
 *
 * @param pre optional config item prefix
 * @param name config item
 * @param post optional config item postfix
 *
 * @return value
 *
 * @note the value returned is the first of:
 * @note 1. the value of <pre>:<name>:<post> from dvb.conf (if <pre> and <post> are valid and value found); or
 * @note 2. the value of <pre>:<name> from dvb.conf (if <pre> is valid and value found); or
 * @note 3. the value of <name>:<post> from dvb.conf (if <post> is valid and value found); or
 * @note 4. the value of <name> from dvb.conf (if value found); or
 * @note 5. the system default of <pre>:<name>:<post> (if <pre> and <post> are valid and system default found); or
 * @note 6. the system default of <pre>:<name (if <pre> is valid and system default found); or
 * @note 7. the system default of <name>:<post> (if <post> is valid and system default found); or
 * @note 8. the system default of <name> (if system default found)
 * @note 9. an empty string otherwise
 */
/*--------------------------------------------------------------------------------*/
AString ADVBConfig::GetHierarchicalConfigItem(const AString& pre, const AString& name, const AString& post) const
{
	std::vector<AString> list;

	GetCombinations(list, pre, name, post);

	return GetConfigItem(list);
}

/*--------------------------------------------------------------------------------*/
/** Return the value of the pre/post config item or specified default
 *
 * @param pre optional config item prefix
 * @param name config item
 * @param post optional config item postfix
 *
 * @return value
 *
 * @note the value returned is the first of:
 * @note 1. the value of <pre>:<name>:<post> from dvb.conf (if <pre> and <post> are valid and value found); or
 * @note 2. the value of <pre>:<name> from dvb.conf (if <pre> is valid and value found); or
 * @note 3. the value of <name>:<post> from dvb.conf (if <post> is valid and value found); or
 * @note 4. the value of <name> from dvb.conf (if value found); or
 * @note 5. defval otherwise
 */
/*--------------------------------------------------------------------------------*/
AString ADVBConfig::GetHierarchicalConfigItem(const AString& pre, const AString& name, const AString& post, const AString& defval) const
{
	std::vector<AString> list;

	GetCombinations(list, pre, name, post);

	return GetConfigItem(list, defval, true);
}

/*--------------------------------------------------------------------------------*/
/** Return user based config item or system default
 *
 * @param user DVB user name
 * @param name config item name
 *
 * @return user value (<user>:<name>), value (<name>), system default or empty string
 */
/*--------------------------------------------------------------------------------*/
AString ADVBConfig::GetUserConfigItem(const AString& user, const AString& name) const
{
	return GetHierarchicalConfigItem(user, name, "");
}

/*--------------------------------------------------------------------------------*/
/** Return user based config item or the specified default
 *
 * @param user DVB user name
 * @param name config item name
 * @param defval default value
 *
 * @return user value (<user>:<name>), value (<name>), specified default or empty string
 *
 * @note this will *never* return the system default for the config item because the default is explicit (but the caller could use GetDefaultItem())
 */
/*--------------------------------------------------------------------------------*/
AString ADVBConfig::GetUserConfigItem(const AString& user, const AString& name, const AString& defval) const
{
	return GetHierarchicalConfigItem(user, name, "", defval);
}

/*--------------------------------------------------------------------------------*/
/** Get user/subitem based config item or system default
 *
 * @param user DVB user name
 * @param name config item name
 * @param subitem subitem
 *
 * @return user-subitem value (<user>:<name>:<subitem>), user value (<user>:<name>), value (<name>), system default or empty string
 */
/*--------------------------------------------------------------------------------*/
AString ADVBConfig::GetUserSubItemConfigItem(const AString& user, const AString& subitem, const AString& name) const
{
	return GetHierarchicalConfigItem(user, name, subitem);
}

/*--------------------------------------------------------------------------------*/
/** Get user/subitem based config item or specified default
 *
 * @param user DVB user name
 * @param name config item name
 * @param subitem subitem
 *
 * @return user-subitem value (<user>:<name>:<subitem>), user value (<user>:<name>), value (<name>), specified default or empty string
 *
 * @note this will *never* return the system default for the config item because the default is explicit (but the caller could use GetDefaultItem())
 */
/*--------------------------------------------------------------------------------*/
AString ADVBConfig::GetUserSubItemConfigItem(const AString& user, const AString& subitem, const AString& name, const AString& defval) const
{
	return GetHierarchicalConfigItem(user, name, subitem, defval);
}

/*--------------------------------------------------------------------------------*/
/** Return configuration directory
 */
/*--------------------------------------------------------------------------------*/
AString ADVBConfig::GetConfigDir() const
{
	return DEFAULTCONFDIR;
}

/*--------------------------------------------------------------------------------*/
/** Return data directory where data is to be stored
 */
/*--------------------------------------------------------------------------------*/
AString ADVBConfig::GetDataDir() const
{
	return GetConfigItem("datadir", DEFAULTDATADIR);
}

/*--------------------------------------------------------------------------------*/
/** Return log directory where logs are to be stored
 */
/*--------------------------------------------------------------------------------*/
AString ADVBConfig::GetLogDir() const
{
	return GetConfigItem("logdir", DEFAULTLOGDIR);
}

/*--------------------------------------------------------------------------------*/
/** Return share directory where shared, readonly data is to be stored
 */
/*--------------------------------------------------------------------------------*/
AString ADVBConfig::GetShareDir() const
{
	return GetConfigItem("sharedir", RDDVB_SHARE_DIR);
}

/*--------------------------------------------------------------------------------*/
/** Return unique filename of temporary file with prefix and suffix
 *
 * @param name filename prefix
 * @param suffix filename suffix
 *
 * @return unique filename
 */
/*--------------------------------------------------------------------------------*/
AString ADVBConfig::GetTempFile(const AString& name, const AString& suffix) const
{
	static uint32_t filenumber = 0U;
	return GetTempDir().CatPath(name + AString("_%08x_%08x_%u").Arg(getpid()).Arg(::GetTickCount()).Arg(filenumber++) + suffix);
}

const AString& ADVBConfig::GetDefaultInterRecTime() const
{
	static const AString rectime = "10";
	return rectime;
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

	dvbcards.clear();

	for (i = 0; i < 4; i++) {
		AString defname = "*";
		AString cardname;
		sint_t  card = -1;

		cardname = GetConfigItem(AString("card%;").Arg(i), defname);

		if ((card = findcard(cardname, &dvbcards)) >= 0) {
			dvbcards.push_back((uint_t)card);
		}
		else break;
	}

	printf("DVB card mapping:");
	for (i = 0; i < dvbcards.size(); i++) {
		printf("Virtual card %u -> physical card %u", i, dvbcards[i]);
	}
}

uint_t ADVBConfig::GetPhysicalDVBCard(uint_t n, bool forcemapping) const
{
	if (forcemapping || !dvbcards.size()) {
		(const_cast<ADVBConfig *>(this))->MapDVBCards();
	}

	return (n < dvbcards.size()) ? dvbcards[n] : 0;
}

uint_t ADVBConfig::GetVirtualDVBCard(uint_t n) const
{
	std::vector<uint_t>::const_iterator it;
	return ((it = std::find(dvbcards.begin(), dvbcards.end(), n)) != dvbcards.end()) ? *it : ~0;
}

AString ADVBConfig::GetIgnoreDVBCardList() const
{
	return GetConfigItem("ignoredvbcards", "");
}

bool ADVBConfig::IgnoreDVBCard(uint_t n) const
{
	AString cards = GetIgnoreDVBCardList();
	uint_t i, ncards = cards.CountColumns();

	for (i = 0; i < ncards; i++) {
		if ((uint_t)cards.Column(i) == n) return true;
	}

	return false;
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

	if (show && !disableoutput) {
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
	AHash    users;
	AList 	 userpatterns;
	AString  filepattern 	    = GetUserPatternsPattern();
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

AString ADVBConfig::GetSlaveLogDir() const
{
	return GetConfigItem("slavelogdir", GetLogDir().CatPath(GetConfigItem("slavelogsubdir", "slave")));
}

AString ADVBConfig::GetRecordingsDir() const
{
	return CatPath(GetDataDir(), GetConfigItem("recdir", GetConfigItem("homedir").CatPath("Videos")));
}

AString ADVBConfig::GetRecordingsStorageDir() const
{
	return CatPath(GetRecordingsDir(), GetConfigItem("storagedir", "Temp"));
}

AString ADVBConfig::GetRecordingsArchiveDir() const
{
	return CatPath(GetRecordingsDir(), GetConfigItem("achivedir", "Archive"));
}

AString ADVBConfig::GetTempDir() const
{
	return GetConfigItem("tempdir", GetDataDir().CatPath("temp"));
}

AString ADVBConfig::GetQueue() const
{
	return GetConfigItem("queue", "d");
}

AString ADVBConfig::GetRecordingsSubDir(const AString& user, const AString& category) const
{
	return GetUserSubItemConfigItem(user, category, "dir");
}

AString ADVBConfig::GetRecordingsDir(const AString& user, const AString& category) const
{
	return CatPath(GetRecordingsDir(), GetRecordingsSubDir(user, category));
}

AString ADVBConfig::GetFilenameTemplate(const AString& user, const AString& title, const AString category) const
{
	return GetHierarchicalConfigItem(user, category, "filename", GetUserSubItemConfigItem(user, title, "filename", "{conf:episodefirstfilenametemplate}")) + "{sep}{suffix}";
}

AString ADVBConfig::GetListingsFile() const
{
	return CatPath(GetDataDir(), GetConfigItem("listingsfile", "dvblistings.dat"));
}

AString ADVBConfig::GetDVBChannelsFile() const
{
	return CatPath(GetDataDir(), GetConfigItem("dvbchannelsfile", "channels.dat"));
}

AString ADVBConfig::GetPatternsFile() const
{
	return CatPath(GetConfigDir(), GetConfigItem("patternsfile", "patterns.txt"));
}

AString ADVBConfig::GetUserPatternsPattern() const
{
	return CatPath(GetConfigDir(), GetConfigItem("patternsfile", "{#?}-patterns.txt"));
}

AString ADVBConfig::GetRecordedFile() const
{
	return CatPath(GetDataDir(), GetConfigItem("recordedfile", "recorded.dat"));
}

AString ADVBConfig::GetRequestedFile() const
{
	return CatPath(GetDataDir(), GetConfigItem("requestedfile", "requested.dat"));
}

AString ADVBConfig::GetScheduledFile() const
{
	return CatPath(GetDataDir(), GetConfigItem("scheduledfile", "scheduled.dat"));
}

AString ADVBConfig::GetRejectedFile() const
{
	return CatPath(GetDataDir(), GetConfigItem("rejectedfile", "rejected.dat"));
}

AString ADVBConfig::GetRecordingFile() const
{
	return CatPath(GetDataDir(), GetConfigItem("recordingfile", "recording.dat"));
}

AString ADVBConfig::GetProcessingFile() const
{
	return CatPath(GetDataDir(), GetConfigItem("processingfile", "processing.dat"));
}

AString ADVBConfig::GetRecordFailuresFile() const
{
	return CatPath(GetDataDir(), GetConfigItem("recordfailuresfile", "recordfailures.dat"));
}

AString ADVBConfig::GetCombinedFile() const
{
	return CatPath(GetDataDir(), GetConfigItem("combinedfile", "combined.dat"));
}

AString ADVBConfig::GetDVBReplacementsFileShare() const
{
	return CatPath(GetShareDir(), GetConfigItem("dvbreplacementsfile", "dvbchannelreplacements.txt"));
}

AString ADVBConfig::GetDVBReplacementsFileConfig() const
{
	return CatPath(GetConfigDir(), GetConfigItem("dvbreplacementsfile", "dvbchannelreplacements.txt"));
}

AString ADVBConfig::GetDVBReplacementsFile() const
{
	return AStdFile::exists(GetDVBReplacementsFileConfig()) ? GetDVBReplacementsFileConfig() : GetDVBReplacementsFileShare();
}

AString ADVBConfig::GetXMLTVReplacementsFileShare() const
{
	return CatPath(GetShareDir(), GetConfigItem("xmltvreplacementsfile", "xmltvchannelreplacements.txt"));
}

AString ADVBConfig::GetXMLTVReplacementsFileConfig() const
{
	return CatPath(GetConfigDir(), GetConfigItem("xmltvreplacementsfile", "xmltvchannelreplacements.txt"));
}

AString ADVBConfig::GetXMLTVReplacementsFile() const
{
	return AStdFile::exists(GetXMLTVReplacementsFileConfig()) ? GetXMLTVReplacementsFileConfig() : GetXMLTVReplacementsFileShare();
}

AString ADVBConfig::GetXMLTVDownloadCommand() const
{
	return GetConfigItem("xmltvcmd", "tv_grab_zz_sdjson");
}

AString ADVBConfig::GetXMLTVDownloadArguments(const AString& destfile) const
{
	return GetConfigItem("xmltvargs", "--days 14 >\"{destfile}\"").SearchAndReplace("{destfile}", destfile);
}

AString ADVBConfig::GetExtraRecordFile() const
{
	return CatPath(GetDataDir(), GetConfigItem("extrarecordfile", "extrarecordprogrammes.txt"));
}

AString ADVBConfig::GetDVBCardsFile() const
{
	return CatPath(GetDataDir(), GetConfigItem("dvbcardsfile", "dvbcards.txt"));
}

AString ADVBConfig::GetLogBase() const
{
	return "dvblog-";
}

AString ADVBConfig::GetLogFile(uint32_t day) const
{
	return CatPath(GetLogDir(), GetLogBase() + ADateTime(day, 0UL).DateFormat("%Y-%M-%D") + ".txt");
}

AString ADVBConfig::GetRecordLogBase() const
{
	return "dvbrecordlog-";
}

AString ADVBConfig::GetRecordLog(uint32_t day) const
{
	return CatPath(GetLogDir(), GetRecordLogBase() + ADateTime(day, 0UL).DateFormat("%Y-%M") + ".txt");
}

AString ADVBConfig::GetDVBSignalLogBase() const
{
	return "dvbsignal-";
}

AString ADVBConfig::GetDVBSignalLog(uint32_t day) const
{
	return CatPath(GetLogDir(), GetDVBSignalLogBase() + ADateTime(day, 0UL).DateFormat("%Y-%M-%D") + ".txt");
}

AString ADVBConfig::GetEpisodesFile() const
{
	return CatPath(GetDataDir(), GetConfigItem("episodesfile", "episodes.txt"));
}

AString ADVBConfig::GetSearchesFile() const
{
	return CatPath(GetConfigDir(), GetConfigItem("searchesfile", "searches.txt"));
}

AString ADVBConfig::GetIconCacheFilename() const
{
	return CatPath(GetDataDir(), GetConfigItem("iconcache", "iconcache.txt"));
}

AString ADVBConfig::GetRegionalChannels() const
{
	return GetConfigItem("regionalchannels", "bbc1.bbc.co.uk=north-west,bbc2.bbc.co.uk=north-west,bbc2.bbc.co.uk=england,itv1.itv.co.uk=granada");
}

AString ADVBConfig::GetConvertedFileSuffix(const AString& user, const AString& def) const
{
	return GetUserConfigItem(user, "filesuffix", def);
}

AString ADVBConfig::GetDVBStreamCommand() const
{
	return GetConfigItem("dvbstreamcmd", "dvbstream");
}

AString ADVBConfig::GetMPlayerArgs() const
{
	return GetConfigItem("mplayerargs", "");
}

uint_t ADVBConfig::GetMPlayerCacheSize() const
{
	return (uint_t)GetConfigItem("mplayercachesize", "1048576");
}

uint_t ADVBConfig::GetMPlayerCacheMinSize() const
{
	return (uint_t)GetConfigItem("mplayercacheminsize", "1024");
}

AString ADVBConfig::GetVideoPlayerCommand() const
{
	return GetConfigItem("playercmd", (AString("mplayer {args} -cache-min {cacheminpercent} -cache {cachesize} -")
									   .SearchAndReplace("{args}",            GetMPlayerArgs())
									   .SearchAndReplace("{cacheminpercent}", AValue(100.0 * (double)GetMPlayerCacheMinSize() / (double)GetMPlayerCacheSize() + .01).ToString("0.2"))
									   .SearchAndReplace("{cachesize}",       AValue(GetMPlayerCacheSize()).ToString())));
}

AString ADVBConfig::GetTempFileSuffix() const
{
	return GetConfigItem("tempfilesuffix", "ts");
}

AString ADVBConfig::GetRecordedFileSuffix() const
{
	return GetConfigItem("recordedfilesuffix", "mpg");
}

AString ADVBConfig::GetVideoFileSuffix() const
{
	return GetConfigItem("videofilesuffix", "m2v");
}

AString ADVBConfig::GetAudioFileSuffix() const
{
	return GetConfigItem("audiofilesuffix", "mp2");
}

AString ADVBConfig::GetAudioDestFileSuffix() const
{
	return GetConfigItem("audiodestfilesuffix", "mp3");
}

uint_t ADVBConfig::GetMaxDVBCards() const
{
	return dvbcards.size();
}

AString ADVBConfig::GetDVBFrequencyRange() const
{
	return GetConfigItem("dvbfreqrange", "474,530,8");
}

uint_t ADVBConfig::GetLatestStart() const
{
	return (uint_t)GetConfigItem("lateststart", "5");
}

uint_t ADVBConfig::GetDaysToKeep() const
{
	return (uint_t)GetConfigItem("daystokeep", "7");
}

sint_t ADVBConfig::GetScoreThreshold() const
{
	return (sint_t)GetConfigItem("scorethreshold", "100");
}

double ADVBConfig::GetLowSpaceWarningLimit() const
{
	return (double)GetConfigItem("lowdisklimit", "10.0");
}

bool ADVBConfig::CommitScheduling() const
{
	return ((uint_t)GetConfigItem("commitscheduling", "0") != 0);
}

bool ADVBConfig::AssignEpisodes() const
{
	return ((uint_t)GetConfigItem("assignepisodes", "0") != 0);
}

bool ADVBConfig::RescheduleAfterDeletingPattern(const AString& user, const AString& category) const
{
	return ((uint_t)GetUserSubItemConfigItem(user, category, "rescheduleafterdeletingpattern", "0") != 0);
}

bool ADVBConfig::IsRecordingSlave() const
{
	return ((uint_t)GetConfigItem("isrecordingslave", "0"));
}

bool ADVBConfig::ConvertVideos() const
{
	return ((uint_t)GetConfigItem("convertvideos", AString("%").Arg(!IsRecordingSlave())));
}

bool ADVBConfig::EnableCombined() const
{
	return ((uint_t)GetConfigItem("enablecombined", AString("%").Arg(!IsRecordingSlave())));
}

bool ADVBConfig::UseOldChannelIcon(const AString& user, const AString& category) const
{
	return ((uint_t)GetUserSubItemConfigItem(user, category.ToLower(), "useoldchannelicon",   "1") != 0);
}

bool ADVBConfig::UseOldProgrammeIcon(const AString& user, const AString& category) const
{
	return ((uint_t)GetUserSubItemConfigItem(user, category.ToLower(), "useoldprogrammeicon", "0") != 0);
}

bool ADVBConfig::MonitorDVBSignal() const
{
	return ((uint_t)GetConfigItem("monitordvbsignal", "0") != 0);
}

AString ADVBConfig::GetPriorityDVBPIDs() const
{
	return GetConfigItem("prioritypids", "");
}

AString ADVBConfig::GetExtraDVBPIDs() const
{
	return GetConfigItem("extrapids", "");
}

AString ADVBConfig::GetServerURL() const
{
	return GetConfigItem("serverurl", "");
}

bool ADVBConfig::ForceSubs(const AString& user) const
{
	return ((uint_t)GetUserConfigItem(user, "forcesubs", "0") != 0);
}

AString ADVBConfig::GetEncodeCommand(const AString& user, const AString& category) const
{
	return ReplaceTerms(user, category.ToLower(), GetUserSubItemConfigItem(user, category.ToLower(), "encodecmd", "avconv"));
}

AString ADVBConfig::GetEncodeArgs(const AString& user, const AString& category) const
{
	return ReplaceTerms(user, category.ToLower(), GetUserSubItemConfigItem(user, category.ToLower(), "encodeargs"));
}

AString ADVBConfig::GetEncodeAudioOnlyArgs(const AString& user, const AString& category) const
{
	return ReplaceTerms(user, category.ToLower(), GetUserSubItemConfigItem(user, category.ToLower(), "encodeaudioonlyargs"));
}

AString ADVBConfig::GetEncodeLogLevel(const AString& user, bool verbose) const
{
	return verbose ? GetUserConfigItem(user, "processloglevel:verbose", "warning") : GetUserConfigItem(user, "processloglevel:normal", "error");
}

bool ADVBConfig::LogRemoteCommands() const
{
	return ((uint_t)GetConfigItem("logremotecommands", "0") != 0);
}

AString ADVBConfig::GetRecordingSlave() const
{
	return GetConfigItem("recordingslave", "");
}

uint_t ADVBConfig::GetRecordingSlavePort() const
{
	return (uint_t)GetConfigItem("recordingslaveport", "22");
}

AString ADVBConfig::GetSSHArgs() const
{
	return GetConfigItem("sshargs", "");
}

AString ADVBConfig::GetSCPArgs() const
{
	return GetConfigItem("scpargs", GetSSHArgs());
}

AString ADVBConfig::GetRsyncArgs() const
{
	return GetConfigItem("rsyncargs", "");
}

AString ADVBConfig::GetRsyncBandwidthLimit() const
{
	return GetConfigItem("rsyncbw", "");
}

AString ADVBConfig::GetServerHost() const
{
	return GetConfigItem("serverhost", "");
}

uint_t ADVBConfig::GetServerPort() const
{
	return GetConfigItem("serverport", "1722");
}

AString ADVBConfig::GetServerGetAndConvertCommand() const
{
	return GetConfigItem("servergetandconvertcommand", "dvbgetandconvertrecorded");
}

AString ADVBConfig::GetServerUpdateRecordingsCommand() const
{
	return GetConfigItem("serverupdaterecordingscommand", "dvbupdaterecordings");
}

AString ADVBConfig::GetServerRescheduleCommand() const
{
	return GetConfigItem("serverreschedulecommand", "dvbreschedule");
}

uint_t ADVBConfig::GetMinimalDataRate(const AString& filesuffix) const
{
	AString val = GetConfigItem("mindatarate:" + filesuffix);
	if (val.Empty()) val = GetDefaultItem("mindatarate:" + filesuffix);
	if (val.Empty()) val = GetConfigItem("mindatarate");
	if (val.Empty()) val = GetDefaultItem("mindatarate");
	return (uint_t)val;
}

uint_t ADVBConfig::GetScheduleReportVerbosity(const AString& type) const
{
	AString defval = GetConfigItem("schedulereportverbosity", "1");
	return (uint_t)(type.Valid() ? GetConfigItem("schedulereportverbosity:" + type, defval) : defval);
}

uint_t ADVBConfig::GetMaxRecordLag(const AString& user, const AString& category) const
{
	return (uint_t)GetUserSubItemConfigItem(user, category, "maxrecordlag", "30");
}

bool ADVBConfig::DeleteProgrammesWithNoDVBChannel() const
{
	return ((uint_t)GetConfigItem("deleteprogrammeswithnodvbchannel", "0") != 0);
}
