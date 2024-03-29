#ifndef __DVB_CONFIG__
#define __DVB_CONFIG__

#include <rdlib/strsup.h>
#include <rdlib/DateTime.h>
#include <rdlib/Hash.h>
#include <rdlib/SettingsHandler.h>
#include <rdlib/DataList.h>

#include <vector>
#include <map>

#include "dvbmisc.h"

/*--------------------------------------------------------------------------------*/
/** Config class - handles the dvb.conf configuration file
 *
 * This class uses ASettingsHandler which is designed to allow storing of settings,
 * however, this class does NOT update or re-write dvb.conf
 */
/*--------------------------------------------------------------------------------*/
class ADVBConfig {
public:
    /*--------------------------------------------------------------------------------*/
    /** Singleton access
     */
    /*--------------------------------------------------------------------------------*/
    static const ADVBConfig& Get();
    /*--------------------------------------------------------------------------------*/
    /** Writable singleton access (for updating object, NOT dvb.conf)
     */
    /*--------------------------------------------------------------------------------*/
    static ADVBConfig& GetWriteable();

    enum {
        MaxDVBCards = 4,
    };

    /*--------------------------------------------------------------------------------*/
    /** Disable output (note: write access required)
     */
    /*--------------------------------------------------------------------------------*/
    void DisableOutput() {disableoutput = true;}

    /*--------------------------------------------------------------------------------*/
    /** Return whether output is currently disabled or not
     */
    /*--------------------------------------------------------------------------------*/
    bool IsOutputDisabled() const {return disableoutput;}

    /*--------------------------------------------------------------------------------*/
    /** Report errors during directory creation process
     *
     * @return true if there were some errors during directory creation
     */
    /*--------------------------------------------------------------------------------*/
    bool ReportDirectoryCreationErrors() const {return ReportDirectoryCreationErrors(dircreationerrors);}
    static bool ReportDirectoryCreationErrors(const AString& errors);

    /*--------------------------------------------------------------------------------*/
    /** Configuration item access functions
     *
     * These functions use a hierarchy to decide what to return, the order is *always*:
     * 1. The value(s) stored in dvb.conf
     * 2. The supplied default (if the above doesn't exist)
     * 3. The system default (if the above isn't supplied)
     * 4. An empty string (if the above doesn't exist)
     *
     * Some requests allow for multiple values in dvb.conf, the generic function
     * GetHierarchicalConfigItem(pre, name, post) tries several values in stage 1, above:
     * 1a. <pre>:<name>:<post> in dvb.conf (if <pre> and <post> are valid)
     * 1b. <name>:<post> in dvb.conf (if the above doesn't exist or isn't valid and if <post> is valid)
     * 1c. <pre>:<name> in dvb.conf (if the above doesn't exist or isn't valid and if <pre> is valid)
     * 1d. <name> in dvb.conf (if the above doesn't exist or isn't valid)
     *
     * Before moving onto 2 in the original list.  This mechanism is used to allow
     * pre-user or per-category overrides to parameters.
     */
    /*--------------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------------------*/
    /** Return value config item value or system default value
     *
     * @param name config item name
     *
     * @return config item value, system default or empty string
     */
    /*--------------------------------------------------------------------------------*/
    AString GetConfigItem(const AString& name) const {return GetHierarchicalConfigItem("", name, "");}

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
    AString GetConfigItem(const AString& name, const AString& defval) const {return GetHierarchicalConfigItem("", name, "", defval, true);}

    /*--------------------------------------------------------------------------------*/
    /** Return user based config item or system default
     *
     * @param user DVB user name
     * @param name config item name
     *
     * @return user value (<user>:<name>), value (<name>), system default or empty string
     */
    /*--------------------------------------------------------------------------------*/
    AString GetUserConfigItem(const AString& user, const AString& name) const {return GetHierarchicalConfigItem(user, name, "");}

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
    AString GetUserConfigItem(const AString& user, const AString& name, const AString& defval) const {return GetHierarchicalConfigItem(user, name, "", defval, true);}

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
    AString GetUserSubItemConfigItem(const AString& user, const AString& subitem, const AString& name) const {return GetHierarchicalConfigItem(user, name, subitem);}

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
    AString GetUserSubItemConfigItem(const AString& user, const AString& subitem, const AString& name, const AString& defval) const {return GetHierarchicalConfigItem(user, name, subitem, defval, true);}

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
    AString GetHierarchicalConfigItem(const AString& pre, const AString& name, const AString& post, const AString& defval = "", bool defvalid = false) const;

    /*--------------------------------------------------------------------------------*/
    /** Return true if config contains a value for name
     */
    /*--------------------------------------------------------------------------------*/
    bool    ConfigItemExists(const AString& name) const {return config.Exists(name);}

    /*--------------------------------------------------------------------------------*/
    /** Return true if system defaults contain a value for name
     */
    /*--------------------------------------------------------------------------------*/
    bool    DefaultItemExists(const AString& name) const {return (defaults.find(name) != defaults.end());}

    /*--------------------------------------------------------------------------------*/
    /** Create list of users, based on existence of <name>-patterns.txt in /etc/dvb
     */
    /*--------------------------------------------------------------------------------*/
    void    ListUsers(AList& list) const;

    /*--------------------------------------------------------------------------------*/
    /** Add user
     */
    /*--------------------------------------------------------------------------------*/
    bool    AddUser(const AString& user) const;

    /*--------------------------------------------------------------------------------*/
    /** Rename one user to another
     */
    /*--------------------------------------------------------------------------------*/
    bool    ChangeUser(const AString& olduser, const AString& newuser) const;

    /*--------------------------------------------------------------------------------*/
    /** Delete user
     */
    /*--------------------------------------------------------------------------------*/
    bool    DeleteUser(const AString& user) const;

    /*--------------------------------------------------------------------------------*/
    /** Undelete deleted user
     */
    /*--------------------------------------------------------------------------------*/
    bool    UnDeleteUser(const AString& user) const;

    /*--------------------------------------------------------------------------------*/
    /** Return configuration directory
     */
    /*--------------------------------------------------------------------------------*/
    AString GetConfigDir()                   const; // extractconfig()

    /*--------------------------------------------------------------------------------*/
    /** Return data directory where data is to be stored
     */
    /*--------------------------------------------------------------------------------*/
    AString GetDataDir()                     const; // extractconfig()

    /*--------------------------------------------------------------------------------*/
    /** Return log directory where logs are to be stored
     */
    /*--------------------------------------------------------------------------------*/
    AString GetLogDir()                      const; // extractconfig()

    /*--------------------------------------------------------------------------------*/
    /** Return share directory where shared, readonly data is to be stored
     */
    /*--------------------------------------------------------------------------------*/
    AString GetShareDir()                    const; // extractconfig()

    /*--------------------------------------------------------------------------------*/
    /** Return directory where logs from recording slave are to be stored
     */
    /*--------------------------------------------------------------------------------*/
    AString GetSlaveLogDir()                 const; // extractconfig()

    /*--------------------------------------------------------------------------------*/
    /** Return directory where recordings should be saved by default
     */
    /*--------------------------------------------------------------------------------*/
    AString GetRecordingsDir()               const; // extractconfig()

    AString GetRecordingsStorageDir()        const; // extractconfig()
    AString GetRecordingsArchiveDir()        const; // extractconfig()
    AString GetTempDir()                     const; // extractconfig()

    /*--------------------------------------------------------------------------------*/
    /** Return unique filename of temporary file with prefix and suffix
     *
     * @param name filename prefix
     * @param suffix filename suffix
     *
     * @return unique filename
     */
    /*--------------------------------------------------------------------------------*/
    AString GetTempFile(const AString& name, const AString& suffix = "") const;

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
    AString GetTempFileEx(const AString& tempdir, const AString& name, const AString& suffix = "") const;

    AString GetQueue() const; // extractconfig()

    AString GetRecordingsSubDir(const AString& user, const AString& category = "") const; // extractconfig("<name>", "<category>")
    AString GetRecordingsDir(const AString& user, const AString& category = "")    const; // extractconfig("<name>", "<category>")
    AString GetFilenameTemplate(const AString& user, const AString& title, const AString& category) const; // extractconfig("<name>", "<title>", "<category>")
    AString GetListingsFile()                const; // extractconfig()
    AString GetDVBChannelsFile()             const; // extractconfig()
    AString GetDVBChannelsJSONFile()         const; // extractconfig()
    AString GetPatternsFile()                const; // extractconfig()
    AString GetUserPatternsPattern()         const; // extractconfig()
    AString GetRecordedFile()                const; // extractconfig()
    AString GetRequestedFile()               const; // extractconfig()
    AString GetScheduledFile()               const; // extractconfig()
    AString GetRejectedFile()                const; // extractconfig()
    AString GetRecordingFile()               const; // extractconfig()
    AString GetProcessingFile()              const; // extractconfig()
    AString GetRecordFailuresFile()          const; // extractconfig()
    AString GetCombinedFile()                const; // extractconfig()

    AString GetDVBReplacementsFileShare()    const;
    AString GetDVBReplacementsFileConfig()   const;
    AString GetDVBReplacementsFile()         const; // extractconfig()

    AString GetXMLTVReplacementsFileShare()  const;
    AString GetXMLTVReplacementsFileConfig() const;
    AString GetXMLTVReplacementsFile()       const; // extractconfig()

    AString GetXMLTVDownloadCommand()        const; // extractconfig()
    AString GetXMLTVDownloadArguments(const AString& destfile) const; // extractconfig("<destfile>")

    AString GetExtraRecordFile()             const; // extractconfig()
    AString GetDVBCardsFile()                const; // extractconfig()
    AString GetLogBase()                     const;
    AString GetLogFile(uint32_t day)         const;
    AString GetRecordLogBase()               const;
    AString GetRecordLog(uint32_t day)       const;
    AString GetDVBSignalLogBase()            const;
    AString GetDVBSignalLog(uint32_t day)    const;
    AString GetEpisodesFile()                const; // extractconfig()
    AString GetSearchesFile()                const; // extractconfig()
    AString GetIconCacheFilename()           const; // extractconfig()

    AString GetNamedFile(const AString& name) const;

    AString GetConvertedFileSuffix(const AString& user, const AString& def = "mp4") const; // extractconfig("<user>", "<filetype>")

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
    AString ReplaceTerms(const AString& pre, const AString& _str, const AString& post) const;
    AString ReplaceTerms(const AString& _str) const {return ReplaceTerms("", _str, "");}

    bool    GetUseAdvancedEncoding()         const; // extractconfig()
    AString GetVideoEncoder()                const; // extractconfig()
    AString GetVideoProber()                 const; // extractconfig()
    AString GetDVBStreamCommand()            const; // extractconfig()

    AString GetMPlayerArgs()                 const; // extractconfig()
    uint_t  GetMPlayerCacheSize()            const; // extractconfig()
    uint_t  GetMPlayerCacheMinSize()         const; // extractconfig()
    AString GetVideoPlayerCommand()          const; // extractconfig()
    AString GetStreamEncoderCommand()        const; // extractconfig()
    AString GetHLSEncoderCommand()           const; // extractconfig()
    AString GetHLSCleanCommand()             const; // extractconfig()
    AString ReplaceHLSTerms(const AString& str, const AString& name) const;
    AString GetHLSConfigItem(const AString& item, const AString& name) const;
    uint_t  GetHTTPStreamPort(const AString& args) const; // extractconfig("<args")
    AString GetHTTPStreamCommand(const AString& args) const; // extractconfig("<args>")
    AString GetHTTPStreamURL(const AString& args) const; // extractconfig("<args>")

    AString GetTempFileSuffix()              const; // extractconfig()
    AString GetRecordedFileSuffix()          const; // extractconfig()
    AString GetVideoFileSuffix()             const; // extractconfig()
    AString GetAudioFileSuffix()             const; // extractconfig()
    AString GetAudioDestFileSuffix()         const; // extractconfig()
    AString GetSubtitleFileSuffix()          const; // extractconfig()
    AString GetSubtitleIndexFileSuffix()     const; // extractconfig()

    uint_t  GetPhysicalDVBCard(uint_t n, bool forcemapping = false) const;
    uint_t  GetVirtualDVBCard(uint_t n)      const;
    uint_t  GetMaxDVBCards()                 const;
    AString GetIgnoreDVBCardList()           const; // extractconfig()
    bool    IgnoreDVBCard(uint_t n)          const;
    AString GetDVBFrequencyRange()           const; // extractconfig()

    AString GetTestCardChannel()             const; // extractconfig()
    uint_t  GetTestCardTime()                const; // extractconfig()

    uint_t  GetLatestStart()                 const; // extractconfig()
    uint_t  GetDaysToKeep()                  const; // extractconfig()
    sint_t  GetScoreThreshold()              const; // extractconfig()
    double  GetLowSpaceWarningLimit()        const; // extractconfig()
    bool    CommitScheduling()               const; // extractconfig()

    bool    AssignEpisodes()                 const; // extractconfig()

    bool    RescheduleAfterDeletingPattern(const AString& user, const AString& category) const; // extractconfig("<user>", "<category>")

    bool    IsRecordingSlave()               const; // extractconfig()
    bool    ConvertVideos()                  const; // extractconfig()
    bool    ArchiveRecorded()                const; // extractconfig()
    bool    DeleteUnarchivedRecordings()     const; // extractconfig()
    bool    EnableCombined()                 const; // extractconfig()

    bool    UseOldChannelIcon(const AString& user, const AString& category) const; // extractconfig("<user>", "<category>")
    bool    UseOldProgrammeIcon(const AString& user, const AString& category) const; // extractconfig("<user>", "<category>")

    bool    MonitorDVBSignal()               const;

    AString GetPriorityDVBPIDs()             const; // extractconfig()
    AString GetExtraDVBPIDs()                const; // extractconfig()

    AString GetServerURL()                   const; // extractconfig()

    bool    ForceSubs(const AString& user)   const; // extractconfig("<user>")

    AString GetEncodeCommand(const AString& user, const AString& category)       const; // extractconfig("<user>", "<category>")
    AString GetEncodeCommandLine(const AString& user, const AString& category)   const; // extractconfig("<user>", "<category>")
    AString GetEncodeArgs(const AString& user, const AString& category)          const; // extractconfig("<user>", "<category>")
    AString GetUserEncodeArgs(const AString& user, const AString& category)      const; // extractconfig("<user>", "<category>")
    AString GetEncodeAudioOnlyArgs(const AString& user, const AString& category) const; // extractconfig("<user>", "<category>")
    AString GetEncodeAspect(const AString& user, const AString& programme)       const; // extractconfig("<user>", "<programme>")
    AString GetEncodeLogLevel(const AString& user, bool verbose) const;

    AString GetRelativePath(const AString& filename) const;

    double  GetVideoErrorRateThreshold(const AString& user, const AString& category) const; // extractconfig("<user>", "<category>")

    bool    LogRemoteCommands()              const; // extractconfig()
    AString GetRecordingSlave()              const; // extractconfig()
    uint_t  GetRecordingSlavePort()          const; // extractconfig()
    AString GetRecordingSlaveUser()          const; // extractconfig()
    AString GetStreamSlave()                 const; // extractconfig()
    uint_t  GetStreamSlavePort()             const; // extractconfig()
    AString GetStreamSlaveUser()             const; // extractconfig()
    AString GetSSHArgs()                     const; // extractconfig()
    AString GetSCPArgs()                     const; // extractconfig()
    AString GetRsyncArgs()                   const; // extractconfig()
    AString GetRsyncBandwidthLimit()         const; // extractconfig()

    AString GetServerHost()                  const; // extractconfig()
    uint_t  GetServerPort()                  const; // extractconfig()
    AString GetServerGetAndConvertCommand()  const; // extractconfig()
    AString GetServerUpdateRecordingsCommand() const; // extractconfig()
    AString GetServerRescheduleCommand()     const; // extractconfig()

    AString GetVideoDurationCommand()        const; // extractconfig()
    AString GetVideoErrorCheckArgs()         const; // extractconfig()
    AString GetVideoErrorCheckCommand()      const; // extractconfig()

    AString GetGraphSuffix()                 const; // extractconfig()

    AString GetStreamListingCommand(const AString& tempfile) const; // extractconfig("<tempfile>")
    AString GetStreamListingKillingCommand(uint32_t pid) const; // extractconfig(0)

    double  GetMinimalLengthPercent(const AString& filesuffix) const; // extractconfig("<filesuffix>")
    double  GetMinimalDataRate(const AString& filesuffix) const; // extractconfig("<filesuffix>")

    uint_t  GetScheduleReportVerbosity(const AString& type = "") const; // extractconfig("<type>")

    uint_t  GetMaxRecordLag(const AString& user, const AString& category) const;

    bool    DeleteProgrammesWithNoDVBChannel() const; // extractconfig()

    // NOTE: function cheats 'const'!
    void Set(const AString& var, const AString& val) const;

    void logit(const char *fmt, ...) const PRINTF_FORMAT_METHOD;
    void printf(const char *fmt, ...) const PRINTF_FORMAT_METHOD;
    void vlogit(const char *fmt, va_list ap, bool show = false) const;

    void writetorecordlog(const char *fmt, ...) const PRINTF_FORMAT_METHOD;

    AString GetLogFile() const {return GetLogFile(ADateTime().GetDays());}

    const AString& GetFilename() const {return config.GetFilename();}

    const AString& GetDefaultInterRecTime() const;

    AString ListConfigValues() const;
    AString ListLiveConfigValues() const;

    bool ReadReplacementsFile(std::vector<replacement_t>& replacements, const AString& filename) const;

    bool ExtractLogData(const ADateTime& start, const ADateTime& end, const AString& filename) const;

    // NOTE: function cheats 'const'!
    void SetConfigRecorder(std::vector<AString> *recorder) const;

    static AString Combine(const AString&  str1, const AString&  str2)                       {return str1 + ":" + str2;}
    static AString Combine(const AString&  str1, const AString&  str2, const AString&  str3) {return str1 + ":" + str2 + ":" + str3;}

protected:
    /*--------------------------------------------------------------------------------*/
    /** Return system default value for config item or NULL
     *
     * @param name config item name
     *
     * @return ptr to system default or NULL
     */
    /*--------------------------------------------------------------------------------*/
    const AString *GetDefaultItemEx(const AString& name) const;

    void MapDVBCards();
    void CheckUpdate() const;

    typedef std::map<AString,AString> args_t;
    args_t GetCommaSeparatedArgs(const AString& args) const;
    AString GetArg(const args_t& args, const AString& arg, const AString& def = "") const;

    uint_t GetHTTPStreamPort(const args_t& args) const;

private:
    ADVBConfig();
    ~ADVBConfig();

    typedef AString (*livevaluefn_t)(const ADVBConfig *config);

#define CONFIG_GETVALUE(name, fn) static AString __##fn(const ADVBConfig *config) {return config->fn();}
#include "configlivevalues.def"
#undef CONFIG_GETVALUE

protected:
    ASettingsHandler                 config;
    std::map<AString,AString>        defaults;
    std::vector<uint_t>              dvbcards;
    std::vector<AString>             *configrecorder;
    std::map<AString, livevaluefn_t> livevalues;
    AString                          dircreationerrors;
    bool                             disableoutput;
};

#endif
