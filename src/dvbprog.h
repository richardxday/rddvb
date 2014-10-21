#ifndef __DVB_PROG__
#define __DVB_PROG__

#include <stdarg.h>

#include <rdlib/strsup.h>
#include <rdlib/DateTime.h>
#include <rdlib/DataList.h>
#include <rdlib/Hash.h>
#include <rdlib/SettingsHandler.h>

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

	bool Base64Decode(const AString& str);

	bool Valid() const {return (data && data->start && data->stop);}
	void Delete();

	bool WriteToFile(AStdData& fp) const;

	AString ExportToText() const;

	static AString JSONFormat(const AString& str);
	static AString JSONDateFormat(uint64_t dt, const AString& type = "");
	static AString JSONStartStopFormat(uint64_t start, uint64_t stop, const AString& type = "");
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

	uint64_t	GetActualStart()	   	 const {return data->actstart;}
	uint64_t	GetActualStop()		   	 const {return data->actstop;}
	uint64_t	GetActualLength()	   	 const {return data->actstop - data->actstart;}
	ADateTime	GetActualStartDT()	   	 const {return ADateTime(GetActualStart());}
	ADateTime	GetActualStopDT()	   	 const {return ADateTime(GetActualStop());}

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
	const char *GetUser()			 	 const {return GetString(data->strings.user);}
	const char *GetDir()			 	 const {return GetString(data->strings.dir);}
	const char *GetFilename()		 	 const {return GetString(data->strings.filename);}
	const char *GetPattern()		 	 const {return GetString(data->strings.pattern);}
	const char *GetUUID()			 	 const {return GetString(data->strings.uuid);}
	const char *GetActors()			 	 const {return GetString(data->strings.actors);}
	const char *GetPrefs()			 	 const {return GetString(data->strings.prefs);}

	bool SetUUID();

	bool SetDVBChannel(const char *str)    	   {return SetString(&data->strings.dvbchannel, str);}
	bool SetUser(const char    	  *str)    	   {return SetString(&data->strings.user,    	str);}
	bool SetDir(const char    	  *str)    	   {return SetString(&data->strings.dir,    	str);}
	bool SetFilename(const char	  *str)    	   {return SetString(&data->strings.filename,  	str);}
	bool SetPattern(const char 	  *str)    	   {return SetString(&data->strings.pattern, 	str);}
	bool SetPrefs(const char   	  *str)    	   {return SetString(&data->strings.prefs,   	str);}

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

		Flag_count,
	};
	uint32_t GetFlags()		   	  const {return data->flags;}
	bool   GetFlag(uint8_t flag)  const {return ((data->flags & (1UL << flag)) != 0);}
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
	bool   IsMarkOnly()			  const {return GetFlag(Flag_markonly);}
	void   SetMarkOnly()				{SetFlag(Flag_markonly);}
	bool   RunPostProcess()       const {return GetFlag(Flag_postprocess);}
	bool   IsPostProcessed()	  const {return GetFlag(Flag_postprocessed);}
	void   SetPostProcessed()			{SetFlag(Flag_postprocessed);}
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

	sint_t GetPri()   	       	  const {return data->pri;}
	sint_t GetScore()		   	  const {return data->score;}

	uint_t GetPreHandle()      	  const {return data->prehandle;}
	uint_t GetPostHandle()     	  const {return data->posthandle;}
	uint_t GetDVBCard()	   	   	  const {return data->dvbcard;}
	uint_t GetJobID()		   	  const {return data->jobid;}

	void   SetDVBCard(uint8_t card)     {data->dvbcard = card;}
	void   SetJobID(uint_t id)		    {data->jobid   = id;}

	bool   RecordDataValid()   	  const {return (data->recstart || data->recstop);}

	static int Compare(const ADVBProg *prog1, const ADVBProg *prog2, const bool *reverse = NULL);

	static bool SameProgramme(const ADVBProg& prog1, const ADVBProg& prog2);

	AString GetDescription(uint_t verbosity = 0) const;
	AString GetTitleAndSubtitle() const;
	AString GetTitleSubtitleAndChannel() const;
	AString GetQuickDescription() const;

	AString GenerateFilename() const;

	void GenerateRecordData();

	bool WriteToJobQueue();
	bool ReadFromJob(const AString& filename);

	void ClearList() {list = NULL;}
	void AddToList(ADataList *list);
	const ADataList *GetList() const {return list;}

	void SetPriorityScore();
	int  GetPriorityScore() const {return priority_score;}
	void CountOverlaps(const ADVBProgList& proglist);
	uint_t GetOverlaps() const {return overlaps;}

	static void UpdatePattern(const AString& olduser, const AString& oldpattern, const AString& newuser, const AString& newpattern);
	static void UpdatePattern(const AString& user, const AString& pattern, const AString& newpattern);
	static void InsertPattern(const AString& user, const AString& pattern);
	static void DeletePattern(const AString& user, const AString& pattern);
	static void EnablePattern(const AString& user, const AString& pattern);
	static void DisablePattern(const AString& user, const AString& pattern);

	static int SortListByOverlaps(uptr_t item1, uptr_t item2, void *context);
	static int CompareScore(const ADVBProg& prog1, const ADVBProg& prog2);

	static int SortTermsByAssign(uptr_t item1, uptr_t item2, void *context);

	enum {
		Operator_Inverted = 0x40,

		Operator_EQ = 0,
		Operator_LT,
		Operator_GT,

		Operator_Regex,
		Operator_Contains,
		Operator_StartsWith,
		Operator_EndsWith,
		Operator_Within,
		Operator_Starts,
		Operator_Ends,

		Operator_Assign,
		Operator_Concat,
		Operator_Add,
		Operator_Subtract,
		Operator_Multiply,
		Operator_Divide,
		Operator_Maximum,
		Operator_Minimum,

		Operator_NE 		 	  = Operator_Inverted | Operator_EQ,
		Operator_LE 		 	  = Operator_Inverted | Operator_GT,
		Operator_GE 		 	  = Operator_Inverted | Operator_LT,

		Operator_NotRegex    	  = Operator_Inverted | Operator_Regex,
		Operator_NotContains 	  = Operator_Inverted | Operator_Contains,
		Operator_NotStartsWith 	  = Operator_Inverted | Operator_StartsWith,
		Operator_NotEndsWith 	  = Operator_Inverted | Operator_EndsWith,
		Operator_NotWithin		  = Operator_Inverted | Operator_Within,
		Operator_NotStarts 	  	  = Operator_Inverted | Operator_Starts,
		Operator_NotEnds 	  	  = Operator_Inverted | Operator_Ends,

		Operator_First_Assignable = Operator_Assign,
		Operator_Last_Assignable  = Operator_Minimum,
	};

	typedef struct {
		bool      exclude;
		bool	  enabled;
		bool	  scorebased;
		sint_t	  pri;
		AString   user;
		AString   pattern;
		AString   errors;
		ADataList list;
	} PATTERN;

	typedef struct {
		uint8_t   field;
		uint8_t   opcode;
		uint8_t   opindex;
		AString   value;
		bool	  orflag;
	} TERMDATA;

	static const TERMDATA *GetTermData(const PATTERN& pattern, uint_t term) {return &(((const TERM *)pattern.list[term])->data);}
	static const char     *GetOperatorText(const PATTERN& pattern, uint_t term);
	static const char     *GetOperatorDescription(const PATTERN& pattern, uint_t term);
	static bool           OperatorIsAssign(const PATTERN& pattern, uint_t term);

	static uint_t  ParsePatterns(ADataList& patternlist, const AString& patterns, AString& errors, const AString& sep = "\n", const AString& user = "");
	static bool    ParsePattern(ADataList& patternlist, const AString& line, AString& errors, const AString& user = "");
	static AString ParsePattern(const AString& line, PATTERN& pattern, const AString& user = "");

	static void DeletePattern(uptr_t item, void *context) {
		UNUSED(context);
		delete (PATTERN *)item;
	}

	static AString GetPatternDefinitionsJSON();

	bool Match(const PATTERN& pattern) const;
	void AssignValues(const PATTERN& pattern);
	void UpdateValues(const PATTERN& pattern);

	AString Base64Encode() const {return ::Base64Encode((const uint8_t *)data, sizeof(*data) + data->strings.end);}

	void Record();
	bool OnRecordSuccess() const;
	bool OnRecordFailure() const;
	bool PostProcess();

	static void Record(const AString& channel, uint_t mins = 0);

	static int priscale;
	static int repeatsscale;

	static bool debugsameprogramme;

protected:
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

	static const DVBPROG *nullprog;

	enum {
		StringCount = sizeof(nullprog->strings) / sizeof(nullprog->strings.channel),
	};

	enum {
		FieldType_string = 0,
		FieldType_date,
		FieldType_uint32_t,
		FieldType_sint32_t,
		FieldType_uint16_t,
		FieldType_sint16_t,
		FieldType_uint8_t,
		FieldType_sint8_t,
		FieldType_prog,
		FieldType_flag,
		FieldType_lastflag = FieldType_flag + Flag_count - 1,
	};
	typedef struct {
		const char *name;
		uint8_t    type;
		bool       assignable;
		uint16_t   offset;
		const char *desc;
	} FIELD;

	enum {
		DateType_none = 0,
		DateType_fulldate,
		DateType_date,
		DateType_time,
		DateType_weekday,
	};

	enum {
		FieldTypes_String  = 1U << FieldType_string,
		FieldTypes_Number  = ((1U << FieldType_uint32_t) |
							  (1U << FieldType_sint32_t) |
							  (1U << FieldType_uint16_t) |
							  (1U << FieldType_sint16_t) |
							  (1U << FieldType_uint8_t)  |
							  (1U << FieldType_sint8_t)),
		FieldTypes_Prog    = 1U << FieldType_prog,
		FieldTypes_Default = ~(FieldTypes_String | FieldTypes_Prog),
	};

	typedef struct {
		const char *str;
		bool       assign;
		uint32_t   fieldtypes;
		uint8_t    opcode;
		const char *desc;
		uint_t     len;
	} OPERATOR;

	typedef union {
		uint64_t   u64;
		uint32_t   u32;
		sint32_t   s32;
		uint16_t   u16;
		sint16_t   s16;
		uint8_t    u8;
		sint8_t    s8;
		char   	   *str;
		ADVBProg   *prog;
	} VALUE;

	typedef struct {
		TERMDATA    data;
		const FIELD *field;
		uint8_t 	datetype;
		VALUE 		value;
	} TERM;

	static const OPERATOR *FindOperator(const PATTERN& pattern, uint_t term);

	static void     __DeleteTerm(uptr_t item, void *context);

	static sint64_t TermTypeToInt64s(const void *p, uint_t termtype);
	static void     Int64sToTermType(void *p, sint64_t val, uint_t termtype);

	static bool 	UpdatePatternInFile(const AString& filename, const AString& pattern, const AString& newpattern);
	static void 	AddPatternToFile(const AString& filename, const AString& pattern);

	static void    	SetMarker(AString& marker, const AString& field);
	static bool    	FieldExists(const AString& str, const AString& field, int p = 0, int *pos = NULL);
	static AString 	GetField(const AString& str, const AString& field, int p = 0, int *pos = NULL);

	static void GetFieldValue(const FIELD& field, VALUE& value, AString& val);
	void AssignValue(const FIELD& field, const VALUE& value, uint8_t termtype = Operator_EQ);

	static void SwapBytes(DVBPROG *prog);

	AString ValidFilename(const AString& str) const;

	const char *GetString(uint16_t offset)   const {return data->strdata + offset;}
	bool StringValid(const uint16_t *offset) const {return (offset[1] > offset[0]);}
	bool SetString(const uint16_t *offset, const char *str);
	void SetFlag(uint8_t flag, bool val = true);
	void ClrFlag(uint8_t flag) {SetFlag(flag, false);}

	bool MatchString(const TERM& term, const char *str) const;

	static void StaticInit();
	void Init();

	AString ReplaceTerms(const AString& str) const;

protected:
	DVBPROG  		   *data;
	uint16_t 		   maxsize;
	const ADataList    *list;
	sint_t   		   priority_score;
	uint_t	 		   overlaps;

	static AHash       fieldhash;
	static const FIELD fields[];
	static OPERATOR    operators[];
};

#endif
