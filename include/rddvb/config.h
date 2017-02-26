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
	AString GetConfigItem(const AString& name, const AString& defval) const;
	AString GetUserConfigItem(const AString& user, const AString& name) const {return GetHierarchicalConfigItem(user, name);}
	AString GetUserConfigItem(const AString& user, const AString& name, const AString& defval) const {return GetHierarchicalConfigItem(user, name, defval);}
	AString GetUserSubItemConfigItem(const AString& user, const AString& subitem, const AString& name) const {return GetUserConfigItem(user, name + ":" + subitem, GetUserConfigItem(user, name, GetConfigItem(name + ":" + subitem, GetConfigItem(name))));}
	AString GetUserSubItemConfigItem(const AString& user, const AString& subitem, const AString& name, const AString& defval) const {return GetUserConfigItem(user, name + ":" + subitem, GetUserConfigItem(user, name, GetConfigItem(name + ":" + subitem, GetConfigItem(name, defval))));}

	AString GetHierarchicalConfigItem(const AString& str, const AString& name) const {return str.Valid() ? GetConfigItem(str + ":" + name) : GetConfigItem(name);}
	AString GetHierarchicalConfigItem(const AString& str, const AString& name, const AString& defval) const {return str.Valid() ? GetConfigItem(str + ":" + name, defval) : GetConfigItem(name, defval);}
	
	bool    ConfigItemExists(const AString& name) const {return config.Exists(name);}
	bool    UserConfigItemExists(const AString& user, const AString& name) const {return ConfigItemExists(user + ":" + name);}

	void    ListUsers(AList& list) const;

	AString GetConfigDir()		   			 const;
	AString GetDataDir()			   		 const;
	AString GetLogDir()			   	  		 const;
	AString GetShareDir()			   		 const;
	AString GetSlaveLogDir()			  	 const {return GetConfigItem("slavelogdir", GetLogDir().CatPath(GetConfigItem("slavelogsubdir", "slave")));}
	AString GetRecordingsDir()	   	  		 const {return CatPath(GetDataDir(), GetConfigItem("recdir", "recordings"));}
	AString GetRecordingsStorageDir()		 const {return CatPath(GetRecordingsDir(), GetConfigItem("storagedir", "Temp"));}
	AString GetRecordingsArchiveDir(const AString& user) const {return CatPath(GetRecordingsDir(), GetUserConfigItem(user, "achivedir", "Archive"));}
	AString GetTempDir()              		 const {return GetConfigItem("tempdir", GetDataDir().CatPath("temp"));}
	AString GetTempFile(const AString& name, const AString& suffix = "") const;
	AString GetQueue()						 const {return GetConfigItem("queue", "d");}
	AString GetRecordingsSubDir(const AString& user, const AString& category = "") const {return GetUserSubItemConfigItem(user, category, "dir", user.InitialCapitalCase());}
	AString GetRecordingsDir(const AString& user, const AString& category = "")    const {return CatPath(GetRecordingsDir(), GetRecordingsSubDir(user, category));}
	AString GetFilenameTemplate(const AString& user, const AString category)       const {return GetUserSubItemConfigItem(user, category, "filename", "{title}{sep}{episode}{sep}{date}{sep}{times}{sep}{subtitle}{sep}{suffix}");}
	AString GetListingsFile()		   		 const {return CatPath(GetDataDir(), GetConfigItem("listingsfile", "dvblistings.dat"));}
	AString GetDVBChannelsFile()	   		 const {return CatPath(GetDataDir(), GetConfigItem("dvbchannelsfile", "channels.dat"));}
	AString GetPatternsFile()		   		 const {return CatPath(GetConfigDir(), GetConfigItem("patternsfile", "patterns.txt"));}
	AString GetUserPatternsPattern()		 const {return CatPath(GetConfigDir(), GetConfigItem("patternsfile", "{#?}-patterns.txt"));}
	AString GetRecordedFile()		   		 const {return CatPath(GetDataDir(), GetConfigItem("recordedfile", "recorded.dat"));}
	AString GetRequestedFile()				 const {return CatPath(GetDataDir(), GetConfigItem("requestedfile", "requested.dat"));}
	AString GetScheduledFile()			 	 const {return CatPath(GetDataDir(), GetConfigItem("scheduledfile", "scheduled.dat"));}
	AString GetRejectedFile()			 	 const {return CatPath(GetDataDir(), GetConfigItem("rejectedfile", "rejected.dat"));}
	AString GetRecordingFile()			 	 const {return CatPath(GetDataDir(), GetConfigItem("recordingfile", "recording.dat"));}
	AString GetProcessingFile()			 	 const {return CatPath(GetDataDir(), GetConfigItem("processingfile", "processing.dat"));}
	AString GetRecordFailuresFile()			 const {return CatPath(GetDataDir(), GetConfigItem("recordfailuresfile", "recordfailures.dat"));}
	AString GetCombinedFile()			 	 const {return CatPath(GetDataDir(), GetConfigItem("combinedfile", "combined.dat"));}

	AString GetDVBReplacementsFile()		 const {return CatPath(GetShareDir(), GetConfigItem("dvbreplacementsfile",   "dvbchannelreplacements.txt"));}
	AString GetXMLTVReplacementsFile()		 const {return CatPath(GetShareDir(), GetConfigItem("xmltvreplacementsfile", "xmltvchannelreplacements.txt"));}

	AString GetExtraRecordFile()			 const {return CatPath(GetDataDir(), GetConfigItem("extrarecordfile", "extrarecordprogrammes.txt"));}
	AString GetDVBCardsFile()                const {return CatPath(GetDataDir(), GetConfigItem("dvbcardsfile", "dvbcards.txt"));}
	AString GetLogBase()				     const {return "dvblog-";}
	AString GetLogFile(uint32_t day)         const {return CatPath(GetLogDir(), GetLogBase() + ADateTime(day, 0UL).DateFormat("%Y-%M-%D") + ".txt");}
	AString GetRecordLogBase()				 const {return "dvbrecordlog-";}
	AString GetRecordLog(uint32_t day) 		 const {return CatPath(GetLogDir(), GetRecordLogBase() + ADateTime(day, 0UL).DateFormat("%Y-%M") + ".txt");}
	AString GetDVBSignalLogBase()			 const {return "dvbsignal-";}
	AString GetDVBSignalLog(uint32_t day) 	 const {return CatPath(GetLogDir(), GetDVBSignalLogBase() + ADateTime(day, 0UL).DateFormat("%Y-%M-%D") + ".txt");}
	AString GetEpisodesFile()				 const {return CatPath(GetDataDir(), GetConfigItem("episodesfile", "episodes.txt"));}
	AString GetSearchesFile()				 const {return CatPath(GetConfigDir(), GetConfigItem("searchesfile", "searches.txt"));}
	AString GetIconCacheFilename()           const {return CatPath(GetDataDir(), GetConfigItem("iconcache", "iconcache.txt"));}
	AString GetRegionalChannels()            const {return GetConfigItem("regionalchannels", "bbc1.bbc.co.uk=north-west,bbc2.bbc.co.uk=north-west,bbc2.bbc.co.uk=england,itv1.itv.co.uk=granada");}

	AString GetNamedFile(const AString& name) const;

	AString GetConvertedFileSuffix(const AString& user, const AString& def = "mp4") const {return GetUserConfigItem(user, "filesuffix", def);}
	AString ReplaceTerms(const AString& user, const AString& str) const;
	AString ReplaceTerms(const AString& user, const AString& subitem, const AString& str) const;

	AString GetTempFileSuffix()     		 const {return GetConfigItem("tempfilesuffix", "tmp");}
	AString GetRecordedFileSuffix() 		 const {return GetConfigItem("recordedfilesuffix", "mpg");}
	AString GetVideoFileSuffix() 			 const {return GetConfigItem("videofilesuffix", "m2v");}
	AString GetAudioFileSuffix() 			 const {return GetConfigItem("audiofilesuffix", "mp2");}
	AString GetAudioDestFileSuffix() 		 const {return GetConfigItem("audiodestfilesuffix", "mp3");}

	uint_t  GetPhysicalDVBCard(uint_t n, bool forcemapping = false) const;
	uint_t  GetVirtualDVBCard(uint_t n)      const;
	uint_t  GetMaxDVBCards()				 const {return dvbcards.Count();}
	AString GetDVBFrequencyRange()           const {return GetConfigItem("dvbfreqrange", "474,530,8");}

	uint_t  GetLatestStart()			     const {return (uint_t)GetConfigItem("lateststart", "15");}
	uint_t  GetDaysToKeep()					 const {return (uint_t)GetConfigItem("daystokeep", "7");}
	sint_t  GetScoreThreshold()				 const {return (sint_t)GetConfigItem("scorethreshold", "100");}
	double  GetLowSpaceWarningLimit()		 const {return (double)GetConfigItem("lowdisklimit", "10.0");}
	bool    CommitScheduling()				 const {return ((uint_t)GetConfigItem("commitscheduling", "0") != 0);}

	bool    RescheduleAfterDeletingPattern(const AString& user, const AString& category) const {return ((uint_t)GetUserSubItemConfigItem(user, category, "rescheduleafterdeletingpattern", "0") != 0);}

	bool    IsRecordingSlave()				 const {return ((uint_t)GetConfigItem("isrecordingslave", "0"));}
	bool    ConvertVideos()					 const {return ((uint_t)GetConfigItem("convertvideos", AString("%").Arg(!IsRecordingSlave())));}
	bool    EnableCombined()				 const {return ((uint_t)GetConfigItem("enablecombined", AString("%").Arg(!IsRecordingSlave())));}
	
	AString GetPriorityDVBPIDs()			 const {return GetConfigItem("prioritypids", "");}
	AString GetExtraDVBPIDs()				 const {return GetConfigItem("extrapids", "");}

	AString GetServerURL()					 const {return GetConfigItem("serverurl", "");}

	bool    ForceSubs(const AString& user)   const {return ((uint_t)GetUserConfigItem(user, "forcesubs", "0") != 0);}

	AString GetEncodeCommand(const AString& user, const AString& category) 		 const {return ReplaceTerms(user, category.ToLower(), GetUserSubItemConfigItem(user, category.ToLower(), "encodecmd", "avconv"));}
	AString GetEncodeArgs(const AString& user, const AString& category)    		 const {return ReplaceTerms(user, category.ToLower(), GetUserSubItemConfigItem(user, category.ToLower(), "encodeargs"));}
	AString GetEncodeAudioOnlyArgs(const AString& user, const AString& category) const {return ReplaceTerms(user, category.ToLower(), GetUserSubItemConfigItem(user, category.ToLower(), "encodeaudioonlyargs"));}

	AString GetEncodeLogLevel(const AString& user, bool verbose) const {return verbose ? GetUserConfigItem(user, "processloglevel:verbose", "warning") : GetUserConfigItem(user, "processloglevel:normal", "error");}

	AString GetRelativePath(const AString& filename) const;

	AString GetRecordingHost()               const {return GetConfigItem("recordinghost", "");}
	AString GetSSHArgs()					 const {return GetConfigItem("sshargs", "");}
	AString GetSCPArgs()					 const {return GetConfigItem("scpargs", GetSSHArgs());}
	AString GetRsyncArgs()					 const {return GetConfigItem("rsyncargs", "");}

	AString GetServerHost()                  const {return GetConfigItem("serverhost", "");}
	uint_t  GetServerPort()                  const {return GetConfigItem("serverport", "1722");}
	AString GetServerGetAndConvertCommand()  const {return GetConfigItem("servergetandconvertcommand", "dvbgetandconvertrecorded");}
	AString GetServerUpdateRecordingsCommand() const {return GetConfigItem("serverupdaterecordingscommand", "dvbupdaterecordings");}
	AString GetServerRescheduleCommand()     const {return GetConfigItem("serverreschedulecommand", "dvbreschedule");}

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

protected:
	void MapDVBCards();
	void CheckUpdate() const;

private:
	ADVBConfig();
	~ADVBConfig();

protected:
	ASettingsHandler config;
	AHash			 defaults;
	ADataList		 dvbcards;
	bool			 webresponse;
};

#endif
