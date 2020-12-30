
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
#include "rdlib/SettingsHandler.h"

#define DEFAULTCONFDIR RDDVB_ROOT_DIR "etc/dvb"
#define DEFAULTDATADIR RDDVB_ROOT_DIR "var/dvb"
#define DEFAULTLOGDIR  RDDVB_ROOT_DIR "var/log/dvb"

AQuitHandler quithandler;

ADVBConfig::ADVBConfig() : config(AString(DEFAULTCONFDIR).CatPath("dvb"), false),
                           configrecorder(NULL),
                           disableoutput(false)
{
    const struct passwd *pw = getpwuid(getuid());
    static const struct {
        AString name;
        AString value;
    } __defaults[] = {
        {"username",                     pw->pw_name},
        {"homedir",                      pw->pw_dir},
        {"confdir",                      DEFAULTCONFDIR},
        {"datadir",                      DEFAULTDATADIR},
        {"logdir",                       DEFAULTLOGDIR},
        {"sharedir",                     RDDVB_SHARE_DIR},
        {"tempdir",                      RDDVB_ROOT_DIR "tmp"},
        {"prehandle",                    "2"},
        {"posthandle",                   "3"},
        {"pri",                          "0"},
        {"dir",                          "{titledir}"},
        {"h264crf",                      "17"},
        {"maxvideorate",                 "2000k"},
        {"aacbitrate",                   "160k"},
        {"mp3bitrate",                   "160k"},
        {"copyvideo",                    "-vcodec copy"},
        {"copyaudio",                    "-acodec copy"},
        {"mp3audio",                     "-acodec mp3 -b:a {conf:mp3bitrate}"},
        {"h264preset",                   "veryfast"},
        {"h264bufsize",                  "3000k"},
        {"videodeinterlace",             "yadif"},
        {"videofilters",                 "-filter:v {conf:videodeinterlace}"},
        {"encodeflags",                  "-movflags +faststart"},
        {"h264video",                    "-vcodec libx264 -preset {conf:h264preset} -crf {conf:h264crf} -maxrate {conf:maxvideorate} -bufsize {conf:h264bufsize} {conf:encodeflags} {conf:videofilters}"},
        {"aacaudio",                     "-acodec aac -b:a {conf:aacbitrate} {conf:audiofilters}"},
        {"encodecopy",                   "{conf:copyvideo} {conf:mp3audio}"},
        {"encodeh264",                   "{conf:h264video} {conf:aacaudio}"},
        {"encodeargs",                   "{conf:encodeh264}"},
        {"encodeaudioonlyargs",          "{conf:mp3audio}"},
        {"episodefirstfilenametemplate", "{title}{sep}{episode}{episodeid}{sep}{date}{sep}{times}{sep}{subtitle}"},
        {"episodelastfilenametemplate",  "{title}{sep}{episode}{sep}{date}{sep}{times}{sep}{episodeid}{sep}{subtitle}"},
        {"filmfilenametemplate",         "{title}{sep}{year}{sep}{date}{sep}{times}"},
        {"dir",                          "{capitaluser}/{titledir}"},
        {"dir:film",                     "Films"},
        {"mindatarate:mpg",              "12"},
        {"mindatarate:mp4",              "100"},
        {"mindatarate:mp3",              "12"},
        {"mindatarate",                  "10"},
        {"streaminput",                  "-i -"},
        {"streamoutputmp4",              "mp4"},
        {"streamoutputmpegts",           "mpegts"},
        {"streamoutputformat",           "{conf:streamoutputmp4}"},
        {"streamoutput",                 "-f {conf:streamoutputformat} -"},
        {"streamverbosity",              "-v quiet"},
        {"streamh264preset",             "ultrafast"},
        {"streamh264crf",                "22"},
        {"streammaxvideorate",           "{conf:maxvideorate}"},
        {"streamh264bufsize",            "{conf:h264bufsize}"},
        {"streamencodeflags",            "frag_keyframe+faststart+empty_moov"},
        {"streamvideofilters",           "{conf:videofilters}"},
        {"streamotherargs",              "-tune zerolatency -strict experimental"},
        {"streamh264video",              "-vcodec h264 -preset {conf:streamh264preset} -crf {conf:streamh264crf} {conf:streamotherargs} -movflags {conf:streamencodeflags}"},
        {"streamvideo",                  "{conf:streamh264video} {conf:streamvideofilters}"},
        {"streamaacbitrate",             "{conf:aacbitrate}"},
        {"streammp3bitrate",             "{conf:mp3bitrate}"},
        {"streamaudiofilters",           "{conf:audiofilters}"},
        {"streamaacaudio",               "-acodec aac -b:a {conf:streamaacbitrate}"},
        {"streammp3audio",               "-acodec libmp3lame -b:a {conf:streammp3bitrate}"},
        {"streamaudio",                  "{conf:streamaacaudio} {conf:streamaudiofilters}"},
        {"streamsubtitlelang",           "eng"},
        {"streamsubtitlemetadata",       "-metadata:s:s:0 language={conf:streamsubtitlelang}"},
        {"streamsubtitlecodec",          "-scodec copy"},
        {"streamsubtitles",              "{conf:streamsubtitlecodec} {conf:streamsubtitlemetadata}"},
        {"streamencodeargs",             "{conf:streaminput} {conf:streamvideo} {conf:streamaudio} {conf:streamsubtitles} {conf:streamverbosity} {conf:streamoutput}"},
        {"hlsinput",                     "-i -"},
        {"hlsoutputformat",              "hls"},
        {"hlssegmenttime",               "4"},
        {"hlssegmentcount",              "150"},
        {"hlsoutputpath",                "{conf:*recordingsdir}/Stream/{hlsname}"},
        {"hlsoutputfilename",            "{hlsname}.m3u8"},
        {"hlsoutputfullpath",            "{conf:hlsoutputpath}/{conf:hlsoutputfilename}"},
        {"hlscleanup",                   "rm -fr \"{conf:hlsoutputfullpath}\""},
        {"hlsoutputargs",                "-hls_time {conf:hlssegmenttime} -hls_list_size {conf:hlssegmentcount} -hls_wrap {conf:hlssegmentcount} -hls_flags delete_segments -hls_playlist_type event \"{conf:hlsoutputfullpath}\""},
        {"hlsoutput",                    "-f {conf:hlsoutputformat} {conf:hlsoutputargs}"},
        {"hlsverbosity",                 "-v quiet"},
        {"hlsh264preset",                "ultrafast"},
        {"hlsh264crf",                   "22"},
        {"hlsmaxvideorate",              "{conf:maxvideorate}"},
        {"hlsh264bufsize",               "{conf:h264bufsize}"},
        {"hlsencodeflags",               "frag_keyframe+faststart+empty_moov"},
        {"hlsvideofilters",              "{conf:videofilters}"},
        {"hlsotherargs",                 "-tune zerolatency -strict experimental -g 25 -sc_threshold 0"},
        {"hlsh264video",                 "-vcodec h264 -preset {conf:hlsh264preset} -crf {conf:hlsh264crf} {conf:hlsotherargs} -movflags {conf:hlsencodeflags}"},
        {"hlsvideo",                     "{conf:hlsh264video} {conf:hlsvideofilters}"},
        {"hlsaacbitrate",                "{conf:aacbitrate}"},
        {"hlsmp3bitrate",                "{conf:mp3bitrate}"},
        {"hlsaudiofilters",              "{conf:audiofilters}"},
        {"hlsaacaudio",                  "-acodec aac -b:a {conf:hlsaacbitrate}"},
        {"hlsmp3audio",                  "-acodec libmp3lame -b:a {conf:hlsmp3bitrate}"},
        {"hlsaudio",                     "{conf:hlsaacaudio} {conf:hlsaudiofilters}"},
        {"hlssubtitlelang",              "eng"},
        {"hlssubtitlemetadata",          "-metadata:s:s:0 language={conf:hlssubtitlelang}"},
        {"hlssubtitlecodec",             "-scodec copy"},
        {"hlssubtitles",                 "{conf:hlssubtitlecodec} {conf:hlssubtitlemetadata}"},
        {"hlsencodeargs",                "{conf:hlsinput} {conf:hlsvideo} {conf:hlsaudio} {conf:hlsverbosity} {conf:hlsoutput}"},
        {"hlsstreamhtmlsourcefile",      "{conf:sharedir}/stream/hlsstream.html"},
        {"hlsstreamhtmldestfile",        "{conf:hlsoutputpath}/{hlsname}.html"},
    };
    uint_t i;

    for (i = 0; i < NUMBEROF(__defaults); i++) {
        defaults[__defaults[i].name] = __defaults[i].value;
    }

    // ensure no changes are saved
    config.EnableWrite(false);

    // map of directory to create -> name of directory
    std::map<AString, AString> dirs;
    AString dir;

    dirs[GetConfigDir()]            = "config";
    dirs[GetDataDir()]              = "data";
    dirs[GetLogDir()]               = "log";
    dirs[GetRecordingsStorageDir()] = "recording storage";
    dirs[GetRecordingsDir()]        = "recordings";
    dirs[GetTempDir()]              = "temp";
    if (((dir = GetRecordingsArchiveDir()).Valid()) && (dir.Pos("{") < 0)) {
        dirs[dir] = "archive";
    }

    if (CommitScheduling()) {
        AList users;

        ListUsers(users);

        const AString *user = AString::Cast(users.First());
        while (user) {
            if (((dir = GetRecordingsDir(*user)).Valid())   && (dir.Pos("{") < 0)) {
                dirs[dir] = "user " + *user + " recordings";
            }

            user = user->Next();
        }
    }

    std::map<AString, AString>::iterator it;
    for (it = dirs.begin(); it != dirs.end(); ++it)
    {
        if (!CreateDirectory(it->first)) {
            if (dircreationerrors.Valid()) {
                dircreationerrors.printf("\n");
            }

            dircreationerrors.printf("Unable to create %s directory ('%s')", it->second.str(), it->first.str());
        }
    }

    if (!AStdFile::exists(GetRecordFailuresFile())) {
        // create empty record failures file if none exists
        AStdFile fp;

        fp.open(GetRecordFailuresFile(), "w");
        fp.close();
    }

#define CONFIG_GETVALUE(name, fn) livevalues["*" name] = &__##fn;
#include "configlivevalues.def"
#undef CONFIG_GETVALUE
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
/** Report errors during directory creation process
 *
 * @return true if there were some errors during directory creation
 */
/*--------------------------------------------------------------------------------*/
bool ADVBConfig::ReportDirectoryCreationErrors(const AString& errors)
{
    if (errors.Valid()) {
        const ADVBConfig& config = ADVBConfig::Get();
        AString cmd = config.GetConfigItem("createdirfailcmd");

        if (cmd.Valid()) {
            AString  tempfile = config.GetTempFileEx(config.GetTempDir(), "dir-creation-errors", ".txt");
            AStdFile fp;

            if (fp.open(tempfile, "w")) {
                fp.printf("%s\n", errors.str());
                fp.close();
            }

            cmd = cmd.SearchAndReplace("{logfile}", tempfile);
            RunAndLogCommand(cmd);

            remove(tempfile);
        }

        config.printf("%s", errors.str());
    }

    return errors.Valid();
}

/*--------------------------------------------------------------------------------*/
/** Replace terms in given string using possibly-valid pre and post values
 *
 * @param pre optional prefix for config item
 * @param _str string to perform replacements on
 * @param post optional postfix for config item
 *
 * @return string
 */
/*--------------------------------------------------------------------------------*/
AString ADVBConfig::ReplaceTerms(const AString& pre, const AString& _str, const AString& post) const
{
    AString str = _str;
    int p1, p2;

    while (((p1 = str.Pos("{conf:")) >= 0) && ((p2 = str.FindClosing('}', p1 + 6)) >= 0)) {
        AString var = ReplaceTerms(pre, str.Mid(p1 + 6, p2 - p1 - 6), post);
        AString item;

        auto it = livevalues.find(var);
        if (it != livevalues.end()) {
            item = ReplaceTerms(pre, (*it->second)(this), post);
        }
        else item = GetHierarchicalConfigItem(pre, var, post);

        str = str.Left(p1) + item + str.Mid(p2 + 1);
    }

    return str;
}

/*--------------------------------------------------------------------------------*/
/** Return the value of the pre/post config item or specified default
 *
 * @param pre optional config item prefix
 * @param name config item
 * @param post optional config item postfix
 * @param defval default value if no value found (only used if defvalid = true)
 * @param defvalid true if defval is to be used instead of system default
 *
 * @return value
 *
 * @note the value returned is the first of:
 * @note 1. the value of <pre>:<name>:<post> from dvb.conf (if <pre> and <post> are valid and value found); or
 * @note 2. the value of <pre>:<name> from dvb.conf (if <pre> is valid and value found); or
 * @note 3. the value of <name>:<post> from dvb.conf (if <post> is valid and value found); or
 * @note 4. the value of <name> from dvb.conf (if value found); or
 * @note 5. defval if defvalid is true; or
 * @note 6. the system default of <pre>:<name>:<post> (if <pre> and <post> are valid and system default found); or
 * @note 7. the system default of <pre>:<name (if <pre> is valid and system default found); or
 * @note 8. the system default of <name>:<post> (if <post> is valid and system default found); or
 * @note 9. the system default of <name> (if system default found)
 * @note 10. an empty string otherwise
 */
/*--------------------------------------------------------------------------------*/
AString ADVBConfig::GetHierarchicalConfigItem(const AString& pre, const AString& name, const AString& post, const AString& defval, bool defvalid) const
{
    std::map<AString,AString>::const_iterator it;
    AString res;

    CheckUpdate();

    defvalid |= defval.Valid();

    if (pre.Valid() && name.Valid() && post.Valid() && config.Exists(Combine(pre, name, post))) {
        res = config.Get(Combine(pre, name, post));
    }
    else if (pre.Valid() && name.Valid() && config.Exists(Combine(pre, name))) {
        res = config.Get(Combine(pre, name));
    }
    else if (name.Valid() && post.Valid() && config.Exists(Combine(name, post))) {
        res = config.Get(Combine(name, post));
    }
    else if (config.Exists(name)) {
        res = config.Get(name);
    }
    else if (defvalid) {
        res = defval;
    }
    else if (pre.Valid() && name.Valid() && post.Valid() && ((it = defaults.find(Combine(pre, name, post))) != defaults.end())) {
        res = it->second;
    }
    else if (pre.Valid() && name.Valid() && ((it = defaults.find(Combine(pre, name))) != defaults.end())) {
        res = it->second;
    }
    else if (name.Valid() && post.Valid() && ((it = defaults.find(Combine(name, post))) != defaults.end())) {
        res = it->second;
    }
    else if (((it = defaults.find(name)) != defaults.end())) {
        res = it->second;
    }

    res = ReplaceTerms(pre, res, post);

    if (configrecorder) configrecorder->push_back(name + "=" + res);

    return res;
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
    return GetTempFileEx(GetTempDir(), name, suffix);
}

/*--------------------------------------------------------------------------------*/
/** Return unique filename of temporary file with prefix and suffix
 *
 * @param tempdir temporary directory
 * @param name filename prefix
 * @param suffix filename suffix
 *
 * @return unique filename
 */
/*--------------------------------------------------------------------------------*/
AString ADVBConfig::GetTempFileEx(const AString& tempdir, const AString& name, const AString& suffix) const
{
    static uint32_t filenumber = 0U;
    return tempdir.CatPath(name + AString("_%08x_%08x_%u").Arg(getpid()).Arg(::GetTickCount()).Arg(filenumber++) + suffix);
}

const AString& ADVBConfig::GetDefaultInterRecTime() const
{
    static const AString rectime = "10";
    return rectime;
}

AString ADVBConfig::GetNamedFile(const AString& name) const
{
    AString filename;

    if      (name == "listings")              filename = GetListingsFile();
    else if (name == "scheduled")             filename = GetScheduledFile();
    else if (name == "recorded")              filename = GetRecordedFile();
    else if (name == "requested")             filename = GetRequestedFile();
    else if (name == "failures")              filename = GetRecordFailuresFile();
    else if (name == "rejected")              filename = GetRejectedFile();
    else if (name == "combined")              filename = GetCombinedFile();
    else if (name == "processing")            filename = GetProcessingFile();
    else if (name == "extrarecordprogrammes") filename = GetExtraRecordFile();
    else                                      filename = name;

    return filename;
}

AString ADVBConfig::GetRelativePath(const AString& filename) const
{
    AString res;

    if (filename.StartsWith(GetRecordingsDir())) res = AString("/videos").CatPath(filename.Mid(GetRecordingsDir().len()));

    return res;
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
    AList    userpatterns;
    AString  filepattern        = GetUserPatternsPattern();
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

            if      ((p = line.PosNoCase(" user:=")) >= 0) user = line.Mid(p + 7).Word(0).DeQuotify();
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

/*--------------------------------------------------------------------------------*/
/** Add user
 */
/*--------------------------------------------------------------------------------*/
bool ADVBConfig::AddUser(const AString& user) const
{
    AString filename = GetConfigDir().CatPath(GetUserPatternsPattern().SearchAndReplace("{#?}", user));
    AStdFile fp;
    bool success = false;

    if (fp.open(filename, "a")) {
        logit("Created user '%s'", user.str());
        fp.close();
        success = true;
    }

    return success;
}

/*--------------------------------------------------------------------------------*/
/** Rename one user to another
 */
/*--------------------------------------------------------------------------------*/
bool ADVBConfig::ChangeUser(const AString& olduser, const AString& newuser) const
{
    AString filename1 = GetConfigDir().CatPath(GetUserPatternsPattern().SearchAndReplace("{#?}", olduser));
    AString filename2 = GetConfigDir().CatPath(GetUserPatternsPattern().SearchAndReplace("{#?}", newuser));
    bool success = false;

    if (rename(filename1, filename2) == 0) {
        logit("Renamed user '%s' to '%s'", olduser.str(), newuser.str());
        success = true;
    }

    return success;
}

/*--------------------------------------------------------------------------------*/
/** Delete user
 */
/*--------------------------------------------------------------------------------*/
bool ADVBConfig::DeleteUser(const AString& user) const
{
    AString filename    = GetConfigDir().CatPath(GetUserPatternsPattern().SearchAndReplace("{#?}", user));
    AString delfilename = filename + ".deleted";
    bool success = false;

    remove(delfilename);
    if (rename(filename, delfilename) == 0) {
        logit("Deleted user '%s'", user.str());
        success = true;
    }

    return success;
}

/*--------------------------------------------------------------------------------*/
/** Undelete deleted user
 */
/*--------------------------------------------------------------------------------*/
bool ADVBConfig::UnDeleteUser(const AString& user) const
{
    AString filename    = GetConfigDir().CatPath(GetUserPatternsPattern().SearchAndReplace("{#?}", user));
    AString delfilename = filename + ".deleted";
    bool success = false;

    remove(filename);
    if (rename(delfilename, filename) == 0) {
        logit("Undeleted user '%s'", user.str());
        success = true;
    }

    return success;
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
    return CatPath(GetRecordingsDir(), GetConfigItem("archivedir", "Archive"));
}

AString ADVBConfig::GetTempDir() const
{
    return GetConfigItem("tempdir", RDDVB_ROOT_DIR "tmp");
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

AString ADVBConfig::GetFilenameTemplate(const AString& user, const AString& title, const AString& category) const
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

AString ADVBConfig::GetDVBChannelsJSONFile() const
{
    return CatPath(GetDataDir(), GetConfigItem("dvbchannelsjsonfile", "channels.json"));
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

AString ADVBConfig::GetConvertedFileSuffix(const AString& user, const AString& def) const
{
    return GetUserConfigItem(user, "filesuffix", def);
}

AString ADVBConfig::GetDVBStreamCommand() const
{
    return GetConfigItem("dvbstreamcmd", "dvbstream");
}

AString ADVBConfig::GetVideoEncoder() const
{
    return GetConfigItem("videoencoder", "ffmpeg");
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

AString ADVBConfig::GetStreamEncoderCommand() const
{
    return GetConfigItem("streamencodercmd", ReplaceTerms(AString(GetVideoEncoder() + " " + GetConfigItem("streamencodeargs"))));
}

AString ADVBConfig::GetHLSEncoderCommand() const
{
    return GetConfigItem("hlsencodercmd", ReplaceTerms(AString(GetVideoEncoder() + " " + GetConfigItem("hlsencodeargs"))));
}

AString ADVBConfig::GetHLSCleanCommand() const
{
    return GetConfigItem("hlscleanupcmd", ReplaceTerms(GetConfigItem("hlscleanup")));
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

AString ADVBConfig::GetSubtitleFileSuffix() const
{
    return GetConfigItem("subtitlefilesuffix", "sup");
}

AString ADVBConfig::GetSubtitleIndexFileSuffix() const
{
    return GetConfigItem("subtitleindexfilesuffix", "idx");
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
    return GetUserSubItemConfigItem(user, category.ToLower(), "encodecmd", GetVideoEncoder());
}

AString ADVBConfig::GetEncodeArgs(const AString& user, const AString& category) const
{
    return GetUserSubItemConfigItem(user, category.ToLower(), "encodeargs");
}

AString ADVBConfig::GetEncodeAudioOnlyArgs(const AString& user, const AString& category) const
{
    return GetUserSubItemConfigItem(user, category.ToLower(), "encodeaudioonlyargs");
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

AString ADVBConfig::GetStreamSlave() const
{
    return GetConfigItem("streamslave", GetRecordingSlave());
}

uint_t ADVBConfig::GetStreamSlavePort() const
{
    return (uint_t)GetConfigItem("streamslaveport", GetRecordingSlavePort());
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

AString ADVBConfig::GetVideoErrorCheckCommand() const
{
    return GetConfigItem("videocheckcmd", GetVideoEncoder() + " -t 60 -v error -i \"{filename}\" -f null - 2>&1 | wc -l >{logfile}");
}

AString ADVBConfig::GetGraphSuffix() const
{
    return GetConfigItem("graphsuffix", "svg");
}

uint_t ADVBConfig::GetMinimalDataRate(const AString& filesuffix) const
{
    return (uint_t)GetHierarchicalConfigItem("", "mindatarate", filesuffix);
}

uint_t ADVBConfig::GetScheduleReportVerbosity(const AString& type) const
{
    return (uint_t)GetHierarchicalConfigItem("", "schedulereportverbosity", type, "1");
}

uint_t ADVBConfig::GetMaxRecordLag(const AString& user, const AString& category) const
{
    return (uint_t)GetUserSubItemConfigItem(user, category, "maxrecordlag", "30");
}

bool ADVBConfig::DeleteProgrammesWithNoDVBChannel() const
{
    return ((uint_t)GetConfigItem("deleteprogrammeswithnodvbchannel", "0") != 0);
}

AString ADVBConfig::ListConfigValues() const
{
    ADataList list;
    AString res;

    res.printf("Config values:\n");
    config.GetAllLike(list, "");
    for (uint_t i = 0; i < list.Count(); i++)
    {
        const ASettingsHandler::Value& value = *(const ASettingsHandler::Value *)list[i];
        if (value.String1.Valid() && (value.String1[0] != '#')) {
            res.printf("\t%-30s = %s\n", value.String1.str(), GetConfigItem(value.String1).str());
        }
    }

    return res;
}

AString ADVBConfig::ListLiveConfigValues() const
{
    AString res;

    res.printf("Live config values:\n");
    for (auto it = livevalues.begin(); it != livevalues.end(); ++it)
    {
        AString val = ReplaceTerms("{conf:" + it->first + "}");

        if (val.Valid()) {
            res.printf("\t%-30s = %s\n", it->first.str(), val.str());
        }
    }

    return res;
}
