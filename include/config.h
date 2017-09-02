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
	AString GetSlaveLogDir()			  	 const; // extractconfig()
	AString GetRecordingsDir()	   	  		 const; // extractconfig()
	AString GetRecordingsStorageDir()		 const; // extractconfig()
	AString GetRecordingsArchiveDir()		 const;	// extractconfig()
	AString GetTempDir()              		 const;	// extractconfig()
	AString GetTempFile(const AString& name, const AString& suffix = "") const;
	AString GetQueue()						 const; // extractconfig()
	AString GetRecordingsSubDir(const AString& user, const AString& category = "") const; // extractconfig("<name>", "<category>")
	AString GetRecordingsDir(const AString& user, const AString& category = "")    const; // extractconfig("<name>", "<category>")
	AString GetFilenameTemplate(const AString& user, const AString& title, const AString category) const; // extractconfig("<name>", "<title>", "<category>")
	AString GetListingsFile()		   		 const; // extractconfig()
	AString GetDVBChannelsFile()	   		 const; // extractconfig()
	AString GetPatternsFile()		   		 const; // extractconfig()
	AString GetUserPatternsPattern()		 const; // extractconfig()
	AString GetRecordedFile()		   		 const; // extractconfig()
	AString GetRequestedFile()				 const; // extractconfig()
	AString GetScheduledFile()			 	 const; // extractconfig()
	AString GetRejectedFile()			 	 const; // extractconfig()
	AString GetRecordingFile()			 	 const; // extractconfig()
	AString GetProcessingFile()			 	 const; // extractconfig()
	AString GetRecordFailuresFile()			 const; // extractconfig()
	AString GetCombinedFile()			 	 const; // extractconfig()

	AString GetDVBReplacementsFileShare()	 const;
	AString GetDVBReplacementsFileConfig()	 const;
	AString GetDVBReplacementsFile()		 const; // extractconfig()

	AString GetXMLTVReplacementsFileShare()	 const;
	AString GetXMLTVReplacementsFileConfig() const;
	AString GetXMLTVReplacementsFile()		 const; // extractconfig()

	AString GetXMLTVDownloadCommand()		 const; // extractconfig()
	AString GetXMLTVDownloadArguments(const AString& destfile) const; // extractconfig("<destfile>")
	
	AString GetExtraRecordFile()			 const; // extractconfig()
	AString GetDVBCardsFile()                const; // extractconfig()
	AString GetLogBase()				     const;
	AString GetLogFile(uint32_t day)         const;
	AString GetRecordLogBase()				 const;
	AString GetRecordLog(uint32_t day) 		 const;
	AString GetDVBSignalLogBase()			 const;
	AString GetDVBSignalLog(uint32_t day) 	 const;
	AString GetEpisodesFile()				 const; // extractconfig()
	AString GetSearchesFile()				 const; // extractconfig()
	AString GetIconCacheFilename()           const; // extractconfig()
	AString GetRegionalChannels()            const; // extractconfig()

	AString GetNamedFile(const AString& name) const;

	AString GetConvertedFileSuffix(const AString& user, const AString& def = "mp4") const; // extractconfig("<user>", "<filetype>")
	AString ReplaceTerms(const AString& user, const AString& str) const;
	AString ReplaceTerms(const AString& user, const AString& subitem, const AString& str) const;

	AString GetDVBStreamCommand()			 const; // extractconfig()

	AString GetMPlayerArgs()			 	 const; // extractconfig()
	uint_t  GetMPlayerCacheSize()		 	 const; // extractconfig()
	uint_t  GetMPlayerCacheMinSize()	 	 const; // extractconfig()
	AString GetVideoPlayerCommand()			 const; // extractconfig()

	AString GetTempFileSuffix()     		 const; // extractconfig()
	AString GetRecordedFileSuffix() 		 const; // extractconfig()
	AString GetVideoFileSuffix() 			 const; // extractconfig()
	AString GetAudioFileSuffix() 			 const; // extractconfig()
	AString GetAudioDestFileSuffix() 		 const; // extractconfig()

	uint_t  GetPhysicalDVBCard(uint_t n, bool forcemapping = false) const;
	uint_t  GetVirtualDVBCard(uint_t n)      const;
	uint_t  GetMaxDVBCards()				 const;
	AString GetDVBFrequencyRange()           const; // extractconfig()

	uint_t  GetLatestStart()			     const; // extractconfig()
	uint_t  GetDaysToKeep()					 const; // extractconfig()
	sint_t  GetScoreThreshold()				 const; // extractconfig()
	double  GetLowSpaceWarningLimit()		 const; // extractconfig()
	bool    CommitScheduling()				 const; // extractconfig()

	bool	AssignEpisodes()				 const; // extractconfig()
	
	bool    RescheduleAfterDeletingPattern(const AString& user, const AString& category) const; // extractconfig("<user>", "<category>")

	bool    IsRecordingSlave()				 const; // extractconfig()
	bool    ConvertVideos()					 const; // extractconfig()
	bool    EnableCombined()				 const; // extractconfig()

	bool    UseOldChannelIcon(const AString& user, const AString& category) const; // extractconfig("<user>", "<category>")
	bool    UseOldProgrammeIcon(const AString& user, const AString& category) const; // extractconfig("<user>", "<category>")

	bool    MonitorDVBSignal()			     const;
	
	AString GetPriorityDVBPIDs()			 const; // extractconfig()
	AString GetExtraDVBPIDs()				 const; // extractconfig()

	AString GetServerURL()					 const; // extractconfig()

	bool    ForceSubs(const AString& user)   const; // extractconfig("<user>")

	AString GetEncodeCommand(const AString& user, const AString& category) 		 const; // extractconfig("<user>", "<category>")
	AString GetEncodeArgs(const AString& user, const AString& category)    		 const; // extractconfig("<user>", "<category>")
	AString GetEncodeAudioOnlyArgs(const AString& user, const AString& category) const; // extractconfig("<user>", "<category>")

	AString GetEncodeLogLevel(const AString& user, bool verbose) const;

	AString GetRelativePath(const AString& filename) const;

	bool    LogRemoteCommands()              const; // extractconfig()
	AString GetRecordingSlave()              const; // extractconfig()
	uint_t  GetRecordingSlavePort()			 const; // extractconfig()
	AString GetSSHArgs()					 const; // extractconfig()
	AString GetSCPArgs()					 const; // extractconfig()
	AString GetRsyncArgs()					 const; // extractconfig()
	AString GetRsyncBandwidthLimit()		 const; // extractconfig()
	
	AString GetServerHost()                  const; // extractconfig()
	uint_t  GetServerPort()                  const; // extractconfig()
	AString GetServerGetAndConvertCommand()  const; // extractconfig()
	AString GetServerUpdateRecordingsCommand() const; // extractconfig()
	AString GetServerRescheduleCommand()     const; // extractconfig()

	uint_t  GetMinimalDataRate(const AString& filesuffix) const; // extractconfig("<filesuffix>")
	
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
