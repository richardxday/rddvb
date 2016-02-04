#ifndef __DVB_PROG__
#define __DVB_PROG__

#include <stdarg.h>

#include <vector>

#include <rdlib/strsup.h>
#include <rdlib/DateTime.h>
#include <rdlib/DataList.h>
#include <rdlib/Hash.h>
#include <rdlib/SettingsHandler.h>

#include "dvbpatterns.h"

#define DVBDATVERSION 2

class ADVBProgList;
class ADVBProg {
public:
	ADVBProg();
	ADVBProg(AStdData& fp);
	ADVBProg(const AString&  str);
	ADVBProg(const ADVBProg& obj);
	~ADVBProg();

	ADVBProg& operator = (AStdData& fp);
	ADVBProg& operator = (const AString&  str);
	ADVBProg& operator = (const ADVBProg& obj);

	ADVBProg& Modify(const ADVBProg& obj);

	bool Base64Decode(const AString& str);

	bool Valid() const {return (data && data->start && data->stop);}
	void Delete();

	bool WriteToFile(AStdData& fp) const;

	AString ExportToText() const;

	AString ExportToJSON(bool includebase64 = false) const;

	bool Overlaps(const ADVBProg& prog)              const {return ((data->stop > prog.data->start) && (data->start < prog.data->stop));}
	bool OverlapsOnSameChannel(const ADVBProg& prog) const {return (Overlaps(prog) && (CompareNoCase(GetChannelID(), prog.GetChannelID()) == 0));}

	bool RecordOverlaps(const ADVBProg& prog) const {return ((data->recstop > prog.data->recstart) && (data->recstart < prog.data->recstop));}
	
	bool operator == (const ADVBProg& obj) const {return (CompareNoCase(GetUUID(), obj.GetUUID()) == 0);}
	bool operator != (const ADVBProg& obj) const {return (CompareNoCase(GetUUID(), obj.GetUUID()) != 0);}

	bool operator == (const AString&  str) const {return (CompareNoCase(GetUUID(), str) == 0);}
	bool operator != (const AString&  str) const {return (CompareNoCase(GetUUID(), str) != 0);}

	bool operator <= (const ADVBProg& obj) const {return (Compare(this, &obj) <= 0);}
	bool operator >= (const ADVBProg& obj) const {return (Compare(this, &obj) >= 0);}
	bool operator <  (const ADVBProg& obj) const {return (Compare(this, &obj) <  0);}
	bool operator >  (const ADVBProg& obj) const {return (Compare(this, &obj) >  0);}

	void SearchAndReplace(const AString& search, const AString& replace);

	uint64_t	GetStart()			   	 const {return data->start;}
	uint64_t	GetStop()			   	 const {return data->stop;}
	uint64_t	GetLength()			   	 const {return GetStop() - GetStart();}
	ADateTime	GetStartDT()		   	 const {return ADateTime(GetStart());}
	ADateTime	GetStopDT()			   	 const {return ADateTime(GetStop());}

	uint64_t	GetRecordStart()	   	 const {return data->recstart;}
	uint64_t	GetRecordStop()		   	 const {return data->recstop;}
	uint64_t	GetRecordLength()	   	 const {return data->recstop - data->recstart;}
	ADateTime	GetRecordStartDT()	   	 const {return ADateTime(GetRecordStart());}
	ADateTime	GetRecordStopDT()	   	 const {return ADateTime(GetRecordStop());}
	void		SetRecordStart(uint64_t dt)    {data->recstart = dt;}
	void		SetRecordStop(uint64_t dt)     {data->recstop  = dt;}

	sint64_t    GetPreRecordHandle()     const {return data->start - data->recstart;}
	sint64_t    GetPostRecordHandle()    const {return data->recstop - data->stop;}

	uint64_t	GetActualStart()	   	 const {return data->actstart;}
	uint64_t	GetActualStop()		   	 const {return data->actstop;}
	uint64_t	GetActualLength()	   	 const {return data->actstop - data->actstart;}
	ADateTime	GetActualStartDT()	   	 const {return ADateTime(GetActualStart());}
	ADateTime	GetActualStopDT()	   	 const {return ADateTime(GetActualStop());}
	void		SetActualStart(uint64_t dt)    {data->actstart = dt;}
	void		SetActualStop(uint64_t dt)     {data->actstop  = dt;}

	void	    SetFileSize(uint64_t filesize) {data->filesize = filesize;}
	uint64_t    GetFileSize()          	 const {return data->filesize;}
	
	static AString GetHex(uint64_t t)		   {return AValue(t).ToString("x016");}
	static AString GetHex(const ADateTime& dt) {return GetHex((uint64_t)dt);}

	const char *GetChannel()		 	 const {return GetString(data->strings.channel);}
	const char *GetBaseChannel()	 	 const {return GetString(data->strings.basechannel);}
	const char *GetChannelID()		 	 const {return GetString(data->strings.channelid);}
	const char *GetDVBChannel()		 	 const {return GetString(data->strings.dvbchannel);}
	const char *GetTitle()			 	 const {return GetString(data->strings.title);}
	const char *GetSubtitle()		 	 const {return GetString(data->strings.subtitle);}
	const char *GetDesc()			 	 const {return GetString(data->strings.desc);}
	const char *GetCategory()		 	 const {return GetString(data->strings.category);}
	const char *GetDirector()		 	 const {return GetString(data->strings.director);}
	const char *GetEpisodeNum()		 	 const {return GetString(data->strings.episodenum);}
#if DVBDATVERSION > 1
	const char *GetBrandSeriesEpisode()	 const {return GetString(data->strings.brandseriesepisode);}
#endif
	const char *GetUser()			 	 const {return GetString(data->strings.user);}
	const char *GetDir()			 	 const {return GetString(data->strings.dir);}
	const char *GetFilename()		 	 const {return GetString(data->strings.filename);}
	const char *GetPattern()		 	 const {return GetString(data->strings.pattern);}
	const char *GetUUID()			 	 const {return GetString(data->strings.uuid);}
	const char *GetActors()			 	 const {return GetString(data->strings.actors);}
	const char *GetPrefs()			 	 const {return GetString(data->strings.prefs);}

#if DVBDATVERSION > 1
	const char *GetRating()				 const {return GetString(data->strings.rating);}
	const char *GetSubCategory()		 const {return GetString(data->strings.subcategory);}
	void SetSubCategory(const AString& str)    {SetString(&data->strings.subcategory, str);}
#endif

	bool SetUUID();

	bool SetDVBChannel(const char *str)    	   {return SetString(&data->strings.dvbchannel, str);}
	bool SetUser(const char    	  *str)    	   {return SetString(&data->strings.user,    	str);}
	bool SetDir(const char    	  *str)    	   {return SetString(&data->strings.dir,    	str);}
	bool SetFilename(const char	  *str)    	   {return SetString(&data->strings.filename,  	str);}
	bool SetPattern(const char 	  *str)    	   {return SetString(&data->strings.pattern, 	str);}
	bool SetPrefs(const char   	  *str)    	   {return SetString(&data->strings.prefs,   	str);}

#if DVBDATVERSION > 1
	bool SetBrandSeriesEpisode(const char *str) {return SetString(&data->strings.brandseriesepisode, str);}
#endif
	
	AString GetSourceFilename() const;
	AString GetSource2Filename() const;
	AString GetSignatureFilename(const AString& dstname) const;

	AString GetPrefItem(const AString& name, const AString& defval = "") const;

	typedef PACKEDSTRUCT {
		uint8_t  valid;
		uint8_t  series;
		uint16_t episode;
		uint16_t episodes;
	} EPISODE;

	static EPISODE GetEpisode(const AString& str);
	static AString GetEpisodeString(const EPISODE& ep);

	const EPISODE& GetEpisode() 		 const {return data->episode;}
	AString GetEpisodeString()  		 const {return GetEpisodeString(GetEpisode());}

	uint_t GetAssignedEpisode()          const {return data->assignedepisode;}
	void SetAssignedEpisode(uint16_t ep) 	   {data->assignedepisode = ep;}

	uint_t GetYear() const {return data->year;}

	enum {
		Flag_repeat = 0,
		Flag_plus1,
		Flag_usedesc,
		Flag_allowrepeats,
		Flag_urgent,
		Flag_fakerecording,
		Flag_manualrecording,
		Flag_running,
		Flag_markonly,
		Flag_postprocess,
		Flag_postprocessed,
		Flag_onceonly,
		Flag_rejected,
		Flag_recorded,
		Flag_scheduled,
		Flag_radioprogramme,
		Flag_dvbcardspecified,
		Flag_incompleterecording,
		Flag_ignorerecording,
		Flag_recordingfailed,
		Flag_recording,
		Flag_postprocessing,
		Flag_notify,
		
		Flag_count,

		_Flag_extra_start = 32,
		Flag_exists = _Flag_extra_start,
		Flag_srcexists,
	};
	void     SetFlags(uint32_t flags)	{data->flags = flags;}
	uint32_t GetFlags()		   	  const {return data->flags;}
	
	bool   GetFlag(uint8_t flag)  const;
	bool   IsRepeat() 	   	   	  const {return GetFlag(Flag_repeat);}
	bool   IsPlus1()  	   	   	  const {return GetFlag(Flag_plus1);}
	bool   IsRepeatOrPlus1()   	  const {return IsRepeat() || IsPlus1();}
	bool   IsFilm()   	       	  const {return (CompareNoCase(GetCategory(), "film") == 0);}
	bool   UseDescription()    	  const {return GetFlag(Flag_usedesc);}
	bool   AllowRepeats()      	  const {return GetFlag(Flag_allowrepeats);}
	bool   IsUrgent()		   	  const {return GetFlag(Flag_urgent);}
	bool   IsFakeRecording()   	  const {return GetFlag(Flag_fakerecording);}
	bool   IsManualRecording() 	  const {return GetFlag(Flag_manualrecording);}
	void   SetManualRecording()	        {SetFlag(Flag_manualrecording);}
	bool   IsRunning() 		   	  const {return GetFlag(Flag_running);}
	void   SetRunning()		   	  		{SetFlag(Flag_running);}
	void   ClearRunning()		   	  	{ClrFlag(Flag_running);}
	bool   IsMarkOnly()			  const {return GetFlag(Flag_markonly);}
	void   SetMarkOnly()				{SetFlag(Flag_markonly);}
	bool   RunPostProcess()       const {return GetFlag(Flag_postprocess);}
	bool   IsPostProcessed()	  const {return GetFlag(Flag_postprocessed);}
	void   SetPostProcessed()			{SetFlag(Flag_postprocessed);}
	void   ClearPostProcessed()			{ClrFlag(Flag_postprocessed);}
	bool   IsOnceOnly()			  const {return GetFlag(Flag_onceonly);}
	void   SetOnceOnly()				{SetFlag(Flag_onceonly);}
	bool   IsRejected()			  const {return GetFlag(Flag_rejected);}
	void   SetRejected()				{SetFlag(Flag_rejected); data->recstart = data->recstop = 0;}
	bool   IsRecorded()			  const {return GetFlag(Flag_recorded);}
	void   SetRecorded()				{SetFlag(Flag_recorded);}
	void   ClearRecorded()				{ClrFlag(Flag_recorded);}
	bool   IsScheduled()		  const {return GetFlag(Flag_scheduled);}
	void   SetScheduled()				{SetFlag(Flag_scheduled);}
	void   ClearScheduled()				{ClrFlag(Flag_scheduled);}
	bool   IsRadioProgramme()	  const {return GetFlag(Flag_radioprogramme);}
	void   SetRadioProgramme()			{SetFlag(Flag_radioprogramme);}
	bool   IsDVBCardSpecified()	  const {return GetFlag(Flag_dvbcardspecified);}
	void   SetDVBCardSpecified()        {SetFlag(Flag_dvbcardspecified);}
	bool   IsRecordingComplete()  const {return !GetFlag(Flag_incompleterecording);}
	void   SetRecordingComplete();
	bool   IgnoreRecording()	  const {return GetFlag(Flag_ignorerecording);}
	void   SetIgnoreRecording(bool set = true) {SetFlag(Flag_ignorerecording, set);}
	bool   HasRecordingFailed()   const {return GetFlag(Flag_recordingfailed);}
	void   SetRecordingFailed()         {SetFlag(Flag_recordingfailed);}
	void   ClearRecordingFailed()       {ClrFlag(Flag_recordingfailed);}
	bool   IsRecording() 		  const {return GetFlag(Flag_recording);}
	void   SetRecording()	   	  		{SetFlag(Flag_recording);}
	void   ClearRecording()		   	  	{ClrFlag(Flag_recording);}
	bool   IsPostProcessing()	  const {return GetFlag(Flag_postprocessing);}
	void   SetPostProcessing()			{SetFlag(Flag_postprocessing);}
	void   ClearPostProcessing()		{ClrFlag(Flag_postprocessing);}
	bool   NotifySet()	  		  const {return GetFlag(Flag_notify);}
	void   SetNotify()	  		  		{SetFlag(Flag_notify);}

	sint_t GetPri()   	       	  const {return data->pri;}
	sint_t GetScore()		   	  const {return data->score;}

	uint_t GetPreHandle()      	  const {return data->prehandle;}
	uint_t GetPostHandle()     	  const {return data->posthandle;}
	uint_t GetDVBCard()	   	   	  const {return data->dvbcard;}
	uint_t GetJobID()		   	  const {return data->jobid;}

	void   SetPri(sint_t pri)			{data->pri     = pri;}
	void   SetScore(sint_t score)		{data->score   = score;}

	void   SetDVBCard(uint8_t card)     {data->dvbcard = card;}
	void   SetJobID(uint_t id)		    {data->jobid   = id;}

	bool   RecordDataValid()   	  const {return (data->recstart || data->recstop);}

	static const char *PreProcessString(const char *field, AString& temp, const char *str);

	static int Compare(const ADVBProg *prog1, const ADVBProg *prog2, const bool *reverse = NULL);

	static bool SameProgramme(const ADVBProg& prog1, const ADVBProg& prog2);

	AString GetDescription(uint_t verbosity = 0) const;
	AString GetTitleAndSubtitle() const;
	AString GetTitleSubtitleAndChannel() const;
	AString GetQuickDescription() const;

	AString GetRecordingSubDir() const;
	AString GenerateFilename(const AString& templ) const;
	AString GenerateFilename() const;
	void    GenerateRecordData(uint64_t recstarttime);

	bool WriteToJobQueue();
	bool ReadFromJob(const AString& filename);

	void ClearList() {list = NULL;}
	void AddToList(ADataList *list);
	const ADataList *GetList() const {return list;}

	void SetPriorityScore();
	int  GetPriorityScore() const {return priority_score;}
	void CountOverlaps(const ADVBProgList& proglist);
	uint_t GetOverlaps() const {return overlaps;}

	typedef ADVBPatterns::PATTERN PATTERN;
	bool Match(const PATTERN& pattern) const {return ADVBPatterns::Match(*this, pattern);}
	void AssignValues(const PATTERN& pattern) {ADVBPatterns::AssignValues(*this, pattern);}
	void UpdateValues(const PATTERN& pattern) {ADVBPatterns::UpdateValues(*this, pattern);}

	static int SortListByOverlaps(uptr_t item1, uptr_t item2, void *context);
	static int CompareScore(const ADVBProg& prog1, const ADVBProg& prog2);

	AString Base64Encode() const {return ::Base64Encode((const uint8_t *)data, sizeof(*data) + data->strings.end);}

	AString GetLinkToFile() const;

	void Record();
	bool OnRecordSuccess() const;
	bool OnRecordFailure() const;
	bool PostProcess(bool verbose = false);
	bool GenerateSignatureFile(const AString& src, const AString& dst) const;

	void GetEncodedFiles(AList& files) const;
	bool DeleteEncodedFiles() const;
	
	static bool FilePatternExists(const AString& filename);

	static bool GetFileFormat(const AString& filename, AString& format);

	static bool debugsameprogramme;

protected:
	friend class ADVBPatterns;
	typedef ADVBPatterns::FIELD   FIELD;
	typedef ADVBPatterns::VALUE   VALUE;
	typedef ADVBPatterns::TERM    TERM;

	static void    	SetMarker(AString& marker, const AString& field);
	static bool    	FieldExists(const AString& str, const AString& field, int p = 0, int *pos = NULL);
	static AString 	GetField(const AString& str, const AString& field, int p = 0, int *pos = NULL);

	AString GetProgrammeKey() const;
	
#if DVBDATVERSION==1
	typedef PACKEDSTRUCT {
		uint64_t start;
		uint64_t stop;
		uint64_t recstart;
		uint64_t recstop;
		uint64_t actstart;
		uint64_t actstop;

		uint64_t filesize;
		uint32_t flags;

		EPISODE  episode;

		PACKEDSTRUCT {
			uint16_t channel;
			uint16_t basechannel;
			uint16_t channelid;
			uint16_t dvbchannel;
			uint16_t title;
			uint16_t subtitle;
			uint16_t desc;
			uint16_t category;
			uint16_t director;
			uint16_t episodenum;
			uint16_t user;
			uint16_t dir;
			uint16_t filename;
			uint16_t pattern;
			uint16_t uuid;
			uint16_t actors;
			uint16_t prefs;
			uint16_t end;
		} strings;

		uint16_t assignedepisode;
		uint16_t year;
		uint16_t jobid;
		sint16_t score;

		uint8_t  prehandle;
		uint8_t  posthandle;
		uint8_t  dvbcard;
		sint8_t  pri;

		uint8_t  bigendian;

		char     strdata[0];
	} DVBPROG;
#else
	typedef PACKEDSTRUCT {
		uint64_t start;
		uint64_t stop;
		uint64_t recstart;
		uint64_t recstop;
		uint64_t actstart;
		uint64_t actstop;

		uint64_t filesize;
		uint32_t flags;

		EPISODE  episode;

		uint16_t assignedepisode;
		uint16_t year;
		uint16_t jobid;
		sint16_t score;

		uint8_t  prehandle;
		uint8_t  posthandle;
		uint8_t  dvbcard;
		sint8_t  pri;

		PACKEDSTRUCT {
			uint16_t channel;
			uint16_t basechannel;
			uint16_t channelid;
			uint16_t dvbchannel;
			uint16_t title;
			uint16_t subtitle;
			uint16_t desc;
			uint16_t category;
			uint16_t director;
			uint16_t episodenum;
			uint16_t brandseriesepisode;
			uint16_t user;
			uint16_t dir;
			uint16_t filename;
			uint16_t pattern;
			uint16_t uuid;
			uint16_t actors;
			uint16_t prefs;
			uint16_t icon;
			uint16_t rating;
			uint16_t subcategory;
			uint16_t reserved[2];
			uint16_t end;
		} strings;

		char     strdata[0];
	} DVBPROG;
#endif
	
	uint8_t *GetDataPtr(uint16_t offset) const {return (uint8_t *)((uptr_t)data + offset);}

	static uint16_t GetUserDataOffset();
	static uint16_t GetActorsDataOffset();
	static uint16_t GetSubCategoryDataOffset();
	static uint16_t GetPriDataOffset();
	static uint16_t GetScoreDataOffset();

	static void SwapBytes(DVBPROG *prog);

	uint64_t GetDate(const AString& str, const AString& fieldname) const;
	AString ValidFilename(const AString& str, bool dir = false) const;

	const char *GetString(uint16_t offset)   const {return data->strdata + offset;}
	bool StringValid(const uint16_t *offset) const {return (offset[1] > offset[0]);}
	bool SetString(const uint16_t *offset, const char *str);
	
	uint32_t GetFlagMask(uint8_t flag, bool set = true) const {return set ? (1U << flag) : ~(1U << flag);}
	void 	 SetFlag(uint8_t flag, bool set = true);
	void 	 ClrFlag(uint8_t flag) {SetFlag(flag, false);}

	bool MatchString(const TERM& term, const char *str) const;
	
	static const FIELD *GetFields(uint_t& nfields);

	static void StaticInit();
	void Init();

	bool    GetRecordPIDS(AString& pids, bool update = true) const;
	AString GenerateRecordCommand(uint_t nsecs, const AString& pids) const;
	AString GeneratePostProcessCommand() const;

	AString ReplaceTerms(const AString& str) const;
	AString ReplaceFilenameTerms(const AString& str) const;

	AString GetLogFile() const;
	bool    RunCommand(const AString& cmd, bool logoutput = false) const;
	
	static uint64_t CalcTime(const char *str);
	static AString GenTime(uint64_t t, const char *format = "%02u:%02u:%02u.%03u");
	static AString GetParentheses(const AString& line, int p = 0);
	static bool CopyFile(AStdData& fp1, AStdData& fp2);
	static bool CopyFile(const AString& src, const AString& dst, bool binary = true);

	typedef struct {
		AString  aspect;
		uint64_t start, length;
	} SPLIT;

	void ConvertSubtitles(const AString& src, const AString& dst, const std::vector<SPLIT>& splits, const AString& aspect);
	bool EncodeFile(const AString& inputfiles, const AString& aspect, const AString& outputfile, bool verbose) const;
	bool ConvertVideoFile(bool verbose, bool cleanup = true);

	static const DVBPROG *nullprog;

	enum {
		StringCount = sizeof(nullprog->strings) / sizeof(nullprog->strings.channel),
	};

	class FlagsSaver {
	public:
		FlagsSaver(ADVBProg *_prog) : prog(_prog),
									  flags(prog->data->flags) {}
		~FlagsSaver() {prog->data->flags = flags;}

	protected:
		ADVBProg *prog;
		uint32_t flags;
	};
	
protected:
	DVBPROG  		   *data;
	uint16_t 		   maxsize;
	const ADataList    *list;
	sint_t   		   priority_score;
	uint_t	 		   overlaps;

	static AHash       fieldhash;
	static const FIELD fields[];
};

#endif
