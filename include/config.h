#ifndef __DVB_CONFIG__
#define __DVB_CONFIG__

#include <rdlib/strsup.h>
#include <rdlib/DateTime.h>
#include <rdlib/Hash.h>
#include <rdlib/SettingsHandler.h>
#include <rdlib/DataList.h>

#include <vector>

#include "dvbmisc.h"

class ADVBConfig {
public:
	static const ADVBConfig& Get(bool webresponse = false);
	static ADVBConfig& GetWriteable(bool webresponse = false);

	void WebResponse() {webresponse = true;}
	bool IsWebResponse() const {return webresponse;}

	AString GetConfigItem(const AString& name) const;
	AString GetDefaultItem(const AString& name) const;
	AString GetConfigItem(const AString& name, const AString& defval) const;
	AString GetUserConfigItem(const AString& user, const AString& name) const;
	AString GetUserConfigItem(const AString& user, const AString& name, const AString& defval) const;
	AString GetUserSubItemConfigItem(const AString& user, const AString& subitem, const AString& name) const;
	AString GetUserSubItemConfigItem(const AString& user, const AString& subitem, const AString& name, const AString& defval) const;

	AString GetConfigItem(const std::vector<AString>& list, const AString& defval = "", bool defvalid = false) const;
	AString GetHierarchicalConfigItem(const AString& pre, const AString& name, const AString& post) const;
	AString GetHierarchicalConfigItem(const AString& pre, const AString& name, const AString& post, const AString& defval) const;

	bool    ConfigItemExists(const AString& name) const {return config.Exists(name);}
	bool    DefaultItemExists(const AString& name) const {return defaults.Exists(name);}

	void    ListUsers(AList& list) const;

	AString GetConfigDir()		   			 const;	// extractconfig()
	AString GetDataDir()			   		 const;	// extractconfig()
	AString GetLogDir()			   	  		 const;	// extractconfig()
	AString GetShareDir()			   		 const;	// extractconfig()
	AString GetSlaveLogDir()			  	 const {return GetConfigItem("slavelogdir", GetLogDir().CatPath(GetConfigItem("slavelogsubdir", "slave")));} // extractconfig()
	AString GetRecordingsDir()	   	  		 const {return CatPath(GetDataDir(), GetConfigItem("recdir", GetConfigItem("homedir").CatPath("Videos")));} // extractconfig()
	AString GetRecordingsStorageDir()		 const {return CatPath(GetRecordingsDir(), GetConfigItem("storagedir", "Temp"));} // extractconfig()
	AString GetRecordingsArchiveDir()		 const {return CatPath(GetRecordingsDir(), GetConfigItem("achivedir", "Archive"));}	// extractconfig()
	AString GetTempDir()              		 const {return GetConfigItem("tempdir", GetDataDir().CatPath("temp"));}	// extractconfig()
	AString GetTempFile(const AString& name, const AString& suffix = "") const;
	AString GetQueue()						 const {return GetConfigItem("queue", "d");} // extractconfig()
	AString GetRecordingsSubDir(const AString& user, const AString& category = "") const {return GetUserSubItemConfigItem(user, category, "dir");} // extractconfig("<name>", "<category>")
	AString GetRecordingsDir(const AString& user, const AString& category = "")    const {return CatPath(GetRecordingsDir(), GetRecordingsSubDir(user, category));} // extractconfig("<name>", "<category>")
	AString GetFilenameTemplate(const AString& user, const AString& title, const AString category) const {return GetHierarchicalConfigItem(user, category, "filename", GetUserSubItemConfigItem(user, title, "filename", "{conf:episodefirstfilenametemplate}")) + "{sep}{suffix}";} // extractconfig("<name>", "<title>", "<category>")
	AString GetListingsFile()		   		 const {return CatPath(GetDataDir(), GetConfigItem("listingsfile", "dvblistings.dat"));} // extractconfig()
	AString GetDVBChannelsFile()	   		 const {return CatPath(GetDataDir(), GetConfigItem("dvbchannelsfile", "channels.dat"));} // extractconfig()
	AString GetPatternsFile()		   		 const {return CatPath(GetConfigDir(), GetConfigItem("patternsfile", "patterns.txt"));} // extractconfig()
	AString GetUserPatternsPattern()		 const {return CatPath(GetConfigDir(), GetConfigItem("patternsfile", "{#?}-patterns.txt"));} // extractconfig()
	AString GetRecordedFile()		   		 const {return CatPath(GetDataDir(), GetConfigItem("recordedfile", "recorded.dat"));} // extractconfig()
	AString GetRequestedFile()				 const {return CatPath(GetDataDir(), GetConfigItem("requestedfile", "requested.dat"));} // extractconfig()
	AString GetScheduledFile()			 	 const {return CatPath(GetDataDir(), GetConfigItem("scheduledfile", "scheduled.dat"));} // extractconfig()
	AString GetRejectedFile()			 	 const {return CatPath(GetDataDir(), GetConfigItem("rejectedfile", "rejected.dat"));} // extractconfig()
	AString GetRecordingFile()			 	 const {return CatPath(GetDataDir(), GetConfigItem("recordingfile", "recording.dat"));} // extractconfig()
	AString GetProcessingFile()			 	 const {return CatPath(GetDataDir(), GetConfigItem("processingfile", "processing.dat"));} // extractconfig()
	AString GetRecordFailuresFile()			 const {return CatPath(GetDataDir(), GetConfigItem("recordfailuresfile", "recordfailures.dat"));} // extractconfig()
	AString GetCombinedFile()			 	 const {return CatPath(GetDataDir(), GetConfigItem("combinedfile", "combined.dat"));} // extractconfig()

	AString GetDVBReplacementsFileShare()	 const {return CatPath(GetShareDir(), GetConfigItem("dvbreplacementsfile", "dvbchannelreplacements.txt"));}
	AString GetDVBReplacementsFileConfig()	 const {return CatPath(GetConfigDir(), GetConfigItem("dvbreplacementsfile", "dvbchannelreplacements.txt"));}
	AString GetDVBReplacementsFile()		 const {return AStdFile::exists(GetDVBReplacementsFileConfig()) ? GetDVBReplacementsFileConfig() : GetDVBReplacementsFileShare();} // extractconfig()

	AString GetXMLTVReplacementsFileShare()	 const {return CatPath(GetShareDir(), GetConfigItem("xmltvreplacementsfile", "xmltvchannelreplacements.txt"));}
	AString GetXMLTVReplacementsFileConfig() const {return CatPath(GetConfigDir(), GetConfigItem("xmltvreplacementsfile", "xmltvchannelreplacements.txt"));}
	AString GetXMLTVReplacementsFile()		 const {return AStdFile::exists(GetXMLTVReplacementsFileConfig()) ? GetXMLTVReplacementsFileConfig() : GetXMLTVReplacementsFileShare();} // extractconfig()

	AString GetXMLTVDownloadCommand()		 const {return GetConfigItem("xmltvcmd", "tv_grab_sd_json");} // extractconfig()
	AString GetXMLTVDownloadArguments(const AString& destfile) const {return GetConfigItem("xmltvargs", "--days 14 >\"{destfile}\"").SearchAndReplace("{destfile}", destfile);} // extractconfig("<destfile>")
	
	AString GetExtraRecordFile()			 const {return CatPath(GetDataDir(), GetConfigItem("extrarecordfile", "extrarecordprogrammes.txt"));} // extractconfig()
	AString GetDVBCardsFile()                const {return CatPath(GetDataDir(), GetConfigItem("dvbcardsfile", "dvbcards.txt"));} // extractconfig()
	AString GetLogBase()				     const {return "dvblog-";}
	AString GetLogFile(uint32_t day)         const {return CatPath(GetLogDir(), GetLogBase() + ADateTime(day, 0UL).DateFormat("%Y-%M-%D") + ".txt");}
	AString GetRecordLogBase()				 const {return "dvbrecordlog-";}
	AString GetRecordLog(uint32_t day) 		 const {return CatPath(GetLogDir(), GetRecordLogBase() + ADateTime(day, 0UL).DateFormat("%Y-%M") + ".txt");}
	AString GetDVBSignalLogBase()			 const {return "dvbsignal-";}
	AString GetDVBSignalLog(uint32_t day) 	 const {return CatPath(GetLogDir(), GetDVBSignalLogBase() + ADateTime(day, 0UL).DateFormat("%Y-%M-%D") + ".txt");}
	AString GetEpisodesFile()				 const {return CatPath(GetDataDir(), GetConfigItem("episodesfile", "episodes.txt"));} // extractconfig()
	AString GetSearchesFile()				 const {return CatPath(GetConfigDir(), GetConfigItem("searchesfile", "searches.txt"));} // extractconfig()
	AString GetIconCacheFilename()           const {return CatPath(GetDataDir(), GetConfigItem("iconcache", "iconcache.txt"));} // extractconfig()
	AString GetRegionalChannels()            const {return GetConfigItem("regionalchannels", "bbc1.bbc.co.uk=north-west,bbc2.bbc.co.uk=north-west,bbc2.bbc.co.uk=england,itv1.itv.co.uk=granada");} // extractconfig()

	AString GetNamedFile(const AString& name) const;

	AString GetConvertedFileSuffix(const AString& user, const AString& def = "mp4") const {return GetUserConfigItem(user, "filesuffix", def);} // extractconfig("<user>", "<filetype>")
	AString ReplaceTerms(const AString& user, const AString& str) const;
	AString ReplaceTerms(const AString& user, const AString& subitem, const AString& str) const;

	AString GetTempFileSuffix()     		 const {return GetConfigItem("tempfilesuffix", "tmp");} // extractconfig()
	AString GetRecordedFileSuffix() 		 const {return GetConfigItem("recordedfilesuffix", "mpg");} // extractconfig()
	AString GetVideoFileSuffix() 			 const {return GetConfigItem("videofilesuffix", "m2v");} // extractconfig()
	AString GetAudioFileSuffix() 			 const {return GetConfigItem("audiofilesuffix", "mp2");} // extractconfig()
	AString GetAudioDestFileSuffix() 		 const {return GetConfigItem("audiodestfilesuffix", "mp3");} // extractconfig()

	uint_t  GetPhysicalDVBCard(uint_t n, bool forcemapping = false) const;
	uint_t  GetVirtualDVBCard(uint_t n)      const;
	uint_t  GetMaxDVBCards()				 const {return dvbcards.size();}
	AString GetDVBFrequencyRange()           const {return GetConfigItem("dvbfreqrange", "474,530,8");} // extractconfig()

	uint_t  GetLatestStart()			     const {return (uint_t)GetConfigItem("lateststart", "5");} // extractconfig()
	uint_t  GetDaysToKeep()					 const {return (uint_t)GetConfigItem("daystokeep", "7");} // extractconfig()
	sint_t  GetScoreThreshold()				 const {return (sint_t)GetConfigItem("scorethreshold", "100");} // extractconfig()
	double  GetLowSpaceWarningLimit()		 const {return (double)GetConfigItem("lowdisklimit", "10.0");} // extractconfig()
	bool    CommitScheduling()				 const {return ((uint_t)GetConfigItem("commitscheduling", "0") != 0);} // extractconfig()

	bool	AssignEpisodes()				 const {return ((uint_t)GetConfigItem("assignepisodes", "0") != 0);} // extractconfig()
	
	bool    RescheduleAfterDeletingPattern(const AString& user, const AString& category) const {return ((uint_t)GetUserSubItemConfigItem(user, category, "rescheduleafterdeletingpattern", "0") != 0);} // extractconfig("<user>", "<category>")

	bool    IsRecordingSlave()				 const {return ((uint_t)GetConfigItem("isrecordingslave", "0"));} // extractconfig()
	bool    ConvertVideos()					 const {return ((uint_t)GetConfigItem("convertvideos", AString("%").Arg(!IsRecordingSlave())));} // extractconfig()
	bool    EnableCombined()				 const {return ((uint_t)GetConfigItem("enablecombined", AString("%").Arg(!IsRecordingSlave())));} // extractconfig()

	bool    UseOldChannelIcon(const AString& user, const AString& category)   const {return ((uint_t)GetUserSubItemConfigItem(user, category.ToLower(), "useoldchannelicon",   "1") != 0);}	// extractconfig("<user>", "<category>")
	bool    UseOldProgrammeIcon(const AString& user, const AString& category) const {return ((uint_t)GetUserSubItemConfigItem(user, category.ToLower(), "useoldprogrammeicon", "0") != 0);}	// extractconfig("<user>", "<category>")

	bool    MonitorDVBSignal()			     const {return ((uint_t)GetConfigItem("monitordvbsignal", "0") != 0);}
	
	AString GetPriorityDVBPIDs()			 const {return GetConfigItem("prioritypids", "");} // extractconfig()
	AString GetExtraDVBPIDs()				 const {return GetConfigItem("extrapids", "");} // extractconfig()

	AString GetServerURL()					 const {return GetConfigItem("serverurl", "");} // extractconfig()

	bool    ForceSubs(const AString& user)   const {return ((uint_t)GetUserConfigItem(user, "forcesubs", "0") != 0);} // extractconfig("<user>")

	AString GetEncodeCommand(const AString& user, const AString& category) 		 const {return ReplaceTerms(user, category.ToLower(), GetUserSubItemConfigItem(user, category.ToLower(), "encodecmd", "avconv"));} // extractconfig("<user>", "<category>")
	AString GetEncodeArgs(const AString& user, const AString& category)    		 const {return ReplaceTerms(user, category.ToLower(), GetUserSubItemConfigItem(user, category.ToLower(), "encodeargs"));} // extractconfig("<user>", "<category>")
	AString GetEncodeAudioOnlyArgs(const AString& user, const AString& category) const {return ReplaceTerms(user, category.ToLower(), GetUserSubItemConfigItem(user, category.ToLower(), "encodeaudioonlyargs"));} // extractconfig("<user>", "<category>")

	AString GetEncodeLogLevel(const AString& user, bool verbose) const {return verbose ? GetUserConfigItem(user, "processloglevel:verbose", "warning") : GetUserConfigItem(user, "processloglevel:normal", "error");}

	AString GetRelativePath(const AString& filename) const;

	AString GetRecordingSlave()              const {return GetConfigItem("recordingslave", "");} // extractconfig()
	AString GetSSHArgs()					 const {return GetConfigItem("sshargs", "");} // extractconfig()
	AString GetSCPArgs()					 const {return GetConfigItem("scpargs", GetSSHArgs());} // extractconfig()
	AString GetRsyncArgs()					 const {return GetConfigItem("rsyncargs", "");} // extractconfig()

	AString GetServerHost()                  const {return GetConfigItem("serverhost", "");} // extractconfig()
	uint_t  GetServerPort()                  const {return GetConfigItem("serverport", "1722");} // extractconfig()
	AString GetServerGetAndConvertCommand()  const {return GetConfigItem("servergetandconvertcommand", "dvbgetandconvertrecorded");} // extractconfig()
	AString GetServerUpdateRecordingsCommand() const {return GetConfigItem("serverupdaterecordingscommand", "dvbupdaterecordings");} // extractconfig()
	AString GetServerRescheduleCommand()     const {return GetConfigItem("serverreschedulecommand", "dvbreschedule");} // extractconfig()

	// NOTE: function cheats 'const'!
	void Set(const AString& var, const AString& val) const;

	void logit(const char *fmt, ...) const PRINTF_FORMAT_METHOD;
	void printf(const char *fmt, ...) const PRINTF_FORMAT_METHOD;
	void vlogit(const char *fmt, va_list ap, bool show = false) const;

	void writetorecordlog(const char *fmt, ...) const PRINTF_FORMAT_METHOD;

	AString GetLogFile() const {return GetLogFile(ADateTime().GetDays());}

	const AString& GetFilename() const {return config.GetFilename();}

	const AString& GetDefaultInterRecTime() const;

	bool ReadReplacementsFile(std::vector<REPLACEMENT>& replacements, const AString& filename) const;

	bool ExtractLogData(const ADateTime& start, const ADateTime& end, const AString& filename) const;

	// NOTE: function cheats 'const'!
	void SetConfigRecorder(std::vector<AString> *recorder) const;

	static AString Combine(const char     *str1, const char     *str2) {return AString(str1) + ":" + AString(str2);}
	static AString Combine(const char     *str1, const AString&  str2) {return AString(str1) + ":" + str2;}
	static AString Combine(const AString&  str1, const char     *str2) {return str1 + ":" + AString(str2);}
	static AString Combine(const AString&  str1, const AString&  str2) {return str1 + ":" + str2;}
	
protected:
	void MapDVBCards();
	void CheckUpdate() const;

private:
	ADVBConfig();
	~ADVBConfig();

	void GetCombinations(std::vector<AString>& list, const AString& pre, const AString& name, const AString& post = "") const;
	
protected:
	ASettingsHandler 	 config;
	AHash			 	 defaults;
	std::vector<uint_t>  dvbcards;
	std::vector<AString> *configrecorder;
	bool				 webresponse;
};

#endif
