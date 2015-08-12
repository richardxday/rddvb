#ifndef __DVB_CONFIG__
#define __DVB_CONFIG__

#include <rdlib/strsup.h>
#include <rdlib/DateTime.h>
#include <rdlib/Hash.h>
#include <rdlib/SettingsHandler.h>
#include <rdlib/DataList.h>

#include "dvbmisc.h"

class ADVBConfig {
public:
	static const ADVBConfig& Get(bool webresponse = false);
	static ADVBConfig& GetWriteable(bool webresponse = false);

	void WebResponse() {webresponse = true;}

	AString GetConfigItem(const AString& name) const;
	AString GetConfigItem(const AString& name, const AString& defval) const;
	AString GetUserConfigItem(const AString& user, const AString& name) const {return GetConfigItem(user + ":" + name, GetConfigItem(name));}
	bool    ConfigItemExists(const AString& name) const {return config.Exists(name);}
	bool    UserConfigItemExists(const AString& user, const AString& name) const {return ConfigItemExists(user + ":" + name);}

	void    ListUsers(AList& list) const;

	AString GetConfigDir()		   			 const;
	AString GetDataDir()			   		 const;
	AString GetLogDir()			   	  		 const;
	AString GetRecordingsDir()	   	  		 const {return CatPath(GetDataDir(), GetConfigItem("recdir", "recordings"));}
	AString GetRecordingsSubDir(const AString& user) const {return user.Valid() ? GetUserConfigItem(user, "dir") : GetConfigItem("dir");}
	AString GetRecordingsDir(const AString& user) const {return CatPath(GetRecordingsDir(), GetRecordingsSubDir(user));}
	AString GetTempDir()              		 const {return GetConfigItem("tempdir", GetDataDir().CatPath("temp"));}
	AString GetTempFile(const AString& name, const AString& suffix = "") const;
	AString GetQueue()						 const {return GetConfigItem("queue", "d");}
	AString GetFilenameTemplate()			 const {return GetConfigItem("filename", "{title}{sep}{episode}{sep}{date}{sep}{times}{sep}{subtitle}{sep}mpg");}
	AString GetListingsFile()		   		 const {return CatPath(GetDataDir(), GetConfigItem("listings", "dvblistings.dat"));}
	AString GetFreqScanFile()				 const {return CatPath(GetConfigDir(), GetConfigItem("freqscan", "scanfreqs.conf"));}
	AString GetChannelsConfFile()	   		 const {return CatPath(GetConfigDir(), GetConfigItem("channelsconf", "channels.conf"));}
	AString GetDVBChannelsFile()	   		 const {return CatPath(GetConfigDir(), GetConfigItem("dvbchannels", "channels.dat"));}
	AString GetPatternsFile()		   		 const {return CatPath(GetConfigDir(), GetConfigItem("patterns", "patterns.txt"));}
	AString GetUserPatternsPattern()		 const {return CatPath(GetConfigDir(), GetConfigItem("patterns", "{#?}-patterns.txt"));}
	AString GetRecordedFile()		   		 const {return CatPath(GetDataDir(), GetConfigItem("recorded", "recorded.dat"));}
	AString GetRequestedFile()				 const {return CatPath(GetDataDir(), GetConfigItem("requested", "requested.dat"));}
	AString GetScheduledFile()			 	 const {return CatPath(GetDataDir(), GetConfigItem("scheduled", "scheduled.dat"));}
	AString GetRejectedFile()			 	 const {return CatPath(GetDataDir(), GetConfigItem("rejected", "rejected.dat"));}
	AString GetRecordingFile()			 	 const {return CatPath(GetDataDir(), GetConfigItem("recording", "recording.dat"));}
	AString GetCombinedFile()			 	 const {return CatPath(GetDataDir(), GetConfigItem("combined", "combined.dat"));}
	AString GetDVBCardsFile()                const {return CatPath(GetConfigDir(), GetConfigItem("dvbcards", "dvbcards.txt"));}
	AString GetLogFile(uint32_t day)         const {return CatPath(GetLogDir(), "dvblog-" + ADateTime(day, 0UL).DateFormat("%Y-%M-%D") + ".txt");}
	AString GetRecordLog(uint32_t day) 		 const {return CatPath(GetLogDir(), "dvbrecordlog-" + ADateTime(day, 0UL).DateFormat("%Y-%M") + ".txt");}
	AString GetEpisodesFile()				 const {return CatPath(GetDataDir(), GetConfigItem("episodesfile", "episodes.txt"));}
	AString GetSearchesFile()				 const {return CatPath(GetConfigDir(), GetConfigItem("searchesfile", "searches.txt"));}
	AString GetRegionalChannels()            const {return GetConfigItem("regionalchannels", "bbc1.bbc.co.uk=north-west,bbc2.bbc.co.uk=north-west,itv1.itv.co.uk=granada");}
	uint_t  GetPhysicalDVBCard(uint_t n = 0) const;
	uint_t  GetMaxDVBCards()				 const {return (uint_t)GetConfigItem("maxcards", "1");}
	uint_t  GetLatestStart()			     const {return (uint_t)GetConfigItem("lateststart", "30");}
	uint_t  GetDaysToKeep()					 const {return (uint_t)GetConfigItem("daystokeep", "7");}
	sint_t  GetScoreThreshold()				 const {return (sint_t)GetConfigItem("scorethreshold", "100");}
	double  GetLowSpaceWarningLimit()		 const {return (double)GetConfigItem("lowdisklimit", "10.0");}
	bool    CommitScheduling()				 const {return ((uint_t)GetConfigItem("commitscheduling", "0") != 0);}

	int     GetPriorityScale()               const {return (int)GetConfigItem("priscale",     "2");}
	int     GetRepeatsScale()                const {return (int)GetConfigItem("repeatsscale", "-1");}
	int     GetUrgentScale()                 const {return (int)GetConfigItem("urgentscale",  "0");}

	AString GetPriorityDVBPIDs()			 const {return GetConfigItem("prioritypids", "0");}
	AString GetExtraDVBPIDs()				 const {return GetConfigItem("extrapids", "16 17 18");}

	void logit(const char *fmt, ...) const PRINTF_FORMAT_METHOD;
	void printf(const char *fmt, ...) const PRINTF_FORMAT_METHOD;
	void vlogit(const char *fmt, va_list ap, bool show = false) const;

	void addlogit(const char *fmt, ...) const PRINTF_FORMAT_METHOD;

	void writetorecordlog(const char *fmt, ...) const PRINTF_FORMAT_METHOD;

	void SetAdditionalLogFile(const AString& filename, bool timestamps = true) {additionallogfilename = filename; additionallogneedstimestamps = timestamps;}
	const AString& GetAdditionalLogFile() const {return additionallogfilename;}

	AString GetLogFile() const {return additionallogfilename.Valid() ? additionallogfilename : GetLogFile(ADateTime().GetDays());}

	const AString& GetFilename() const {return config.GetFilename();}

	const AString& GetDefaultInterRecTime() const;

protected:
	void MapDVBCards();

private:
	ADVBConfig();
	~ADVBConfig() {}

protected:
	ASettingsHandler config;
	AString          additionallogfilename;
	AHash			 defaults;
	ADataList		 dvbcards;
	bool			 webresponse;
	bool			 additionallogneedstimestamps;
};

#endif
