#ifndef __DVB_PROG__
#define __DVB_PROG__

#include <stdarg.h>

#include <vector>

#include <rapidjson/document.h>

#include <rdlib/strsup.h>
#include <rdlib/DateTime.h>
#include <rdlib/DataList.h>
#include <rdlib/Hash.h>
#include <rdlib/SettingsHandler.h>

#include "dvbpatterns.h"

#define DVBDATVERSION 3

class ADVBProgList;
class ADVBProg {
public:
    /*--------------------------------------------------------------------------------*/
    /** Basic constructor - empty programme
     */
    /*--------------------------------------------------------------------------------*/
    ADVBProg();
    /*--------------------------------------------------------------------------------*/
    /** Construct programme from binary stream
     *
     * @param fp AStdData object to read data from
     */
    /*--------------------------------------------------------------------------------*/
    ADVBProg(AStdData& fp);
    /*--------------------------------------------------------------------------------*/
    /** Construct programme from text field list
     *
     * @param str field list in 'field=value' format (separated by newlines)
     */
    /*--------------------------------------------------------------------------------*/
    ADVBProg(const AString&  str);
    /*--------------------------------------------------------------------------------*/
    /** Copy constructor
     *
     * @param obj existing programme object
     */
    /*--------------------------------------------------------------------------------*/
    ADVBProg(const ADVBProg& obj);
    /*--------------------------------------------------------------------------------*/
    /** Destructor
     */
    /*--------------------------------------------------------------------------------*/
    ~ADVBProg();

    /*--------------------------------------------------------------------------------*/
    /** Initialise programme from binary stream
     *
     * @param fp AStdData object to read data from
     *
     * @return reference to this object
     */
    /*--------------------------------------------------------------------------------*/
    ADVBProg& operator = (AStdData& fp);
    /*--------------------------------------------------------------------------------*/
    /** Initialise programme from text field list
     *
     * @param str field list in 'field=value' format (separated by newlines)
     *
     * @return reference to this object
     */
    /*--------------------------------------------------------------------------------*/
    ADVBProg& operator = (const AString&  str);
    /*--------------------------------------------------------------------------------*/
    /** Initialise from an existing programme object
     *
     * @param obj existing programme object
     *
     * @return reference to this object
     */
    /*--------------------------------------------------------------------------------*/
    ADVBProg& operator = (const ADVBProg& obj);

    /*--------------------------------------------------------------------------------*/
    /** Modify the current programme object from an existing programme object
     *
     * @param obj existing programme object
     *
     * @return reference to this object
     *
     * @note the following fields are copied from the supplied programme if it is valid:
     *   RecordStart
     *   RecordStop
     *   ActualStart
     *   ActualStop
     *   Flags
     *   FileSize
     *   Duration
     *   Video Errors
     *   User
     *   Dir
     *   Filename
     *   Pattern
     *   Prefs
     *   Pri
     *   Score
     *   DVBCard
     *   JobID
     */
    /*--------------------------------------------------------------------------------*/
    ADVBProg& Modify(const ADVBProg& obj);

    /*--------------------------------------------------------------------------------*/
    /** Decode programme data from base64 encoded string
     *
     * @param str base64 string
     *
     * @return true if programme data decoded sucessfully
     */
    /*--------------------------------------------------------------------------------*/
    bool Base64Decode(const AString& str);

    /*--------------------------------------------------------------------------------*/
    /** Return whether programme data is valid
     */
    /*--------------------------------------------------------------------------------*/
    bool Valid() const {return (data && data->start && data->stop);}

    /*--------------------------------------------------------------------------------*/
    /** Delete programme data
     */
    /*--------------------------------------------------------------------------------*/
    void Delete();

    /*--------------------------------------------------------------------------------*/
    /** Write programme data to stream as binary (inverse of operator = (AStdFile& fp))
     *
     * @param fp AStdData object to write data to
     *
     * @return true if write successful
     */
    /*--------------------------------------------------------------------------------*/
    bool WriteToFile(AStdData& fp) const;

    /*--------------------------------------------------------------------------------*/
    /** Return string representing programme data (inverse of operator = (const AString& str))
     *
     * @return string containing encoded data
     */
    /*--------------------------------------------------------------------------------*/
    AString ExportToText() const;

    /*--------------------------------------------------------------------------------*/
    /** Export programme data to JSON value
     *
     * @param doc base document of obj
     * @param obj JSON value to store data in
     * @param includebase64 true to include base64 representation of programme data in export
     */
    /*--------------------------------------------------------------------------------*/
    void ExportToJSON(rapidjson::Document& doc, rapidjson::Value& obj, bool includebase64 = false) const;

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

    /*--------------------------------------------------------------------------------*/
    /** typedef for list of comparison fields
     */
    /*--------------------------------------------------------------------------------*/
    typedef std::vector<sint_t> FIELDLIST;

    /*--------------------------------------------------------------------------------*/
    /** Parse a textual list of fields to populate a comparison priority list
     *
     * @param fieldlist list of field numbers to populate (appended to)
     * @param str a string containing the list of field names
     * @param sep separator for the above
     * @param reverse true to reverse comparison by reversing every field
     *
     * @return true if all field names parsed successfully
     */
    /*--------------------------------------------------------------------------------*/
    static bool ParseFieldList(FIELDLIST& fieldlist, const AString& str, const AString& sep = ",", bool reverse = false);

    /*--------------------------------------------------------------------------------*/
    /** Compare two programmes using the provided list of fields
     *
     * @param prog1 programme 1
     * @param prog2 programme 2
     * @param fieldlist a list of fields to compare by (in priority order)
     * @param reverse ptr to optional reversal flag
     *
     * @return -1, 0 or 1 depending on whether prog1 is <, = or > prog2
     */
    /*--------------------------------------------------------------------------------*/
    static int  Compare(const ADVBProg *prog1, const ADVBProg *prog2, const FIELDLIST& fieldlist, const bool *reverse = NULL);
    static int  Compare(const ADVBProg& prog1, const ADVBProg& prog2, const FIELDLIST& fieldlist, const bool *reverse = NULL) {
        return Compare(&prog1, &prog2, fieldlist, reverse);
    }

    /*--------------------------------------------------------------------------------*/
    /** Compare two programmes using the provided list of fields
     *
     * @param prog1 ptr to programme 1
     * @param prog2 ptr to programme 2
     * @param reverse ptr to optional reversal flag
     *
     * @return -1, 0 or 1 depending on whether prog1 is <, = or > prog2
     *
     * @note this comparison uses a fixed set of fields to compare by:
     *       1. Programme start
     *       2. Programme channel
     *       3. Programme title
     *       4. Programme subtitle
     */
    /*--------------------------------------------------------------------------------*/
    static int  Compare(const ADVBProg *prog1, const ADVBProg *prog2, const bool *reverse = NULL);
    static int  Compare(const ADVBProg& prog1, const ADVBProg& prog2, const bool *reverse = NULL) {
        return Compare(&prog1, &prog2, reverse);
    }

    void SearchAndReplace(const AString& search, const AString& replace);

    uint64_t    GetStart()               const {return data->start;}
    uint64_t    GetStop()                const {return data->stop;}
    uint64_t    GetLength()              const {return GetStop() - GetStart();}
    ADateTime   GetStartDT()             const {return ADateTime(GetStart());}
    ADateTime   GetStopDT()              const {return ADateTime(GetStop());}

    uint64_t    GetRecordStart()         const {return data->recstart;}
    uint64_t    GetRecordStop()          const {return data->recstop;}
    uint64_t    GetRecordLength()        const {return data->recstop - data->recstart;}
    ADateTime   GetRecordStartDT()       const {return ADateTime(GetRecordStart());}
    ADateTime   GetRecordStopDT()        const {return ADateTime(GetRecordStop());}
    void        SetRecordStart(uint64_t dt)    {data->recstart = dt;}
    void        SetRecordStop(uint64_t dt)     {data->recstop  = dt;}

    sint64_t    GetPreRecordHandle()     const {return (sint64_t)(data->start - data->recstart);}
    sint64_t    GetPostRecordHandle()    const {return (sint64_t)(data->recstop - data->stop);}

    uint64_t    GetActualStart()         const {return data->actstart;}
    uint64_t    GetActualStop()          const {return data->actstop;}
    uint64_t    GetActualLength()        const {return data->actstop - data->actstart;}
    ADateTime   GetActualStartDT()       const {return ADateTime(GetActualStart());}
    ADateTime   GetActualStopDT()        const {return ADateTime(GetActualStop());}
    void        SetActualStart(uint64_t dt)    {data->actstart = dt;}
    void        SetActualStop(uint64_t dt)     {data->actstop  = dt;}

    uint64_t    GetRecordLengthFallback() const;
    uint64_t    GetActualLengthFallback() const;

    enum {
        TimeIndex_Start = 0,
        TimeIndex_Stop,
        TimeIndex_RecStart,
        TimeIndex_RecStop,
        TimeIndex_ActStart,
        TimeIndex_ActStop,
    };
    uint64_t    GetTimeIndex(uint_t index) const {return data ? ((const uint64_t *)&data->start)[index] : 0;}

    void        SetFileSize(uint64_t filesize) {data->filesize = filesize;}
    uint64_t    GetFileSize()              const {return data->filesize;}

    void        SetVideoErrors(uint32_t nerrors) {data->videoerrors = nerrors;}
    uint32_t    GetVideoErrors()           const {return data->videoerrors;}

    void        SetDuration(uint64_t duration) {data->duration = duration;}
    uint64_t    GetDuration()              const {return data->duration;}

    // result is in errors/min
    double      GetVideoErrorRate()      const {return (GetDuration() > 0) ? (60000.0 * (double)GetVideoErrors()) / (double)GetDuration() : 0.0;}

    static AString GetHex(uint64_t t)          {return AString("$%016x").Arg(t);}
    static AString GetHex(const ADateTime& dt) {return GetHex((uint64_t)dt);}

    const char *GetChannel()             const {return GetString(data->strings.channel);}
    const char *GetBaseChannel()         const {return GetString(data->strings.basechannel);}
    const char *GetChannelID()           const {return GetString(data->strings.channelid);}
    const char *GetDVBChannel()          const {return GetString(data->strings.dvbchannel);}
    const char *GetTitle()               const {return GetString(data->strings.title);}
    const char *GetSubtitle()            const {return GetString(data->strings.subtitle);}
    const char *GetDesc()                const {return GetString(data->strings.desc);}
    const char *GetCategory()            const {return GetString(data->strings.category);}
    const char *GetModifiedCategory()    const {return IsFilm() ? "film" : GetCategory();}
    const char *GetDirector()            const {return GetString(data->strings.director);}
    const char *GetEpisodeNum()          const {return GetString(data->strings.episodenum);}
    const char *GetEpisodeID()           const {return GetString(data->strings.episodeid);}
    const char *GetUser()                const {return GetString(data->strings.user);}
    const char *GetDir()                 const {return GetString(data->strings.dir);}
    const char *GetFilename()            const {return GetString(data->strings.filename);}
    const char *GetPattern()             const {return GetString(data->strings.pattern);}
    const char *GetUUID()                const {return GetString(data->strings.uuid);}
    const char *GetActors()              const {return GetString(data->strings.actors);}
    const char *GetPrefs()               const {return GetString(data->strings.prefs);}

    const char *GetRating()              const {return GetString(data->strings.rating);}
    const char *GetSubCategory()         const {return GetString(data->strings.subcategory);}
    void SetSubCategory(const AString& str)    {SetString(&data->strings.subcategory, str);}

    bool SetUUID();

    bool SetDVBChannel(const char *str)        {return SetString(&data->strings.dvbchannel, str);}
    bool SetUser(const char       *str)        {return SetString(&data->strings.user,       str);}
    bool SetDir(const char        *str)        {return SetString(&data->strings.dir,        str);}
    bool SetFilename(const char   *str)        {return SetString(&data->strings.filename,   str);}
    bool SetPattern(const char    *str)        {return SetString(&data->strings.pattern,    str);}
    bool SetPrefs(const char      *str)        {return SetString(&data->strings.prefs,      str);}

    bool SetEpisodeID(const char *str)         {return SetString(&data->strings.episodeid,  str);}

    static const AString& GetDayFormat()              {StaticInit(); return dayformat;}
    static const AString& GetDateFormat()             {StaticInit(); return dateformat;}
    static const AString& GetTimeFormat()             {StaticInit(); return timeformat;}
    static const AString& GetFullTimeFormat()         {StaticInit(); return fulltimeformat;}
    static void SetDayFormat(const AString& str)      {StaticInit(); dayformat        = str;}
    static void SetDateFormat(const AString& str)     {StaticInit(); dateformat       = str;}
    static void SetTimeFormat(const AString& str)     {StaticInit(); timeformat       = str;}
    static void SetFullTimeFormat(const AString& str) {StaticInit(); fulltimeformat = str;}

    AString GetFilenameStub() const;
    AString GetTempFilename() const;

    AString GetArchiveRecordingFilename() const;
    AString GetTempRecordingFilename() const;

    AString GetPrefItem(const AString& name, const AString& defval = "") const;

    uint_t  GetRate() const {return GetActualLength() ? (uint_t)((8000 * GetFileSize()) / GetActualLength()) : 0;}

    enum {
        Compare_brate = 0,
        Compare_kbrate,
        Compare_videoerrorrate,
        Compare_filedate,
        Compare_archivefiledate,
    };
    void GetExternal(uint_t id, uint32_t& val) const;
    void GetExternal(uint_t id, sint32_t& val) const;
    void GetExternal(uint_t id, uint64_t& val) const;
    void GetExternal(uint_t id, double& val) const;

    template<typename T>
    int CompareExternal(uint_t id, T value1) const {
        T value2;
        GetExternal(id, value2);
        return COMPARE_ITEMS(value2, value1);
    }

    typedef struct {
        uint8_t  valid;
        uint8_t  series;
        uint16_t episode;
        uint16_t episodes;
    } EPISODE;

    static EPISODE GetEpisode(const AString& str);
    static AString GetEpisodeString(const EPISODE& ep);

    static int CompareEpisode(const EPISODE& ep1, const EPISODE& ep2);

    const EPISODE& GetEpisode()          const {return data->episode;}
    AString GetEpisodeString()           const;
    AString GetShortEpisodeID()          const;

    uint_t GetAssignedEpisode()          const {return data->assignedepisode;}
    void SetAssignedEpisode(uint16_t ep)       {data->assignedepisode = ep;}

    uint_t GetYear() const {return data->year;}

    enum {
        Flag_repeat = 0,
        Flag_plus1,
        Flag_usedesc,
        Flag_allowrepeats,
        Flag_urgent,
        Flag_unused,
        Flag_markonly,
        Flag_running,
        Flag_recording,
        Flag_postprocess,
        Flag_postprocessed,
        Flag_onceonly,
        Flag_rejected,
        Flag_recorded,
        Flag_scheduled,
        Flag_postprocessing,
        Flag_dvbcardspecified,
        Flag_incompleterecording,
        Flag_ignorerecording,
        Flag_recordfailed,
        Flag_notify,
        Flag_partialpattern,
        Flag_ignorelatestart,
        Flag_recordifmissing,
        Flag_postprocessfailed,
        Flag_conversionfailed,
        Flag_archivedhassubtitles,

        Flag_count,

        _Flag_extra_start = 32,
        Flag_exists = _Flag_extra_start,
        Flag_converted,
        Flag_film,
        Flag_recordable,
        Flag_convertedexists,
        Flag_unconvertedexists,
        Flag_archived,
        Flag_failed,
        Flag_radioprogramme,
        Flag_tvprogramme,
        Flag_highvideoerrorrate,

        Flag_total,
    };
    void     SetFlags(uint32_t flags)   {data->flags = flags;}
    uint32_t GetFlags()           const {return data->flags;}

    bool   GetFlag(uint8_t flag)  const;
    bool   IsRepeat()             const {return GetFlag(Flag_repeat);}
    bool   IsPlus1()              const {return GetFlag(Flag_plus1);}
    bool   IsRepeatOrPlus1()      const {return IsRepeat() || IsPlus1();}
    bool   IsFilm()               const {return ((CompareNoCase(GetCategory(), "film") == 0) || (stristr(GetSubCategory(), "movie") != NULL));}
    bool   UseDescription()       const {return GetFlag(Flag_usedesc);}
    bool   AllowRepeats()         const {return GetFlag(Flag_allowrepeats);}
    bool   IsUrgent()             const {return GetFlag(Flag_urgent);}
    bool   IsMarkOnly()           const {return GetFlag(Flag_markonly);}
    void   SetMarkOnly()                {SetFlag(Flag_markonly);}
    bool   IsRunning()            const {return GetFlag(Flag_running);}
    void   SetRunning()                 {SetFlag(Flag_running);}
    void   ClearRunning()               {ClrFlag(Flag_running);}
    bool   IsRecording()          const {return GetFlag(Flag_recording);}
    void   SetRecording()               {SetFlag(Flag_recording);}
    void   ClearRecording()             {ClrFlag(Flag_recording);}
    bool   RunPostProcess()       const {return GetFlag(Flag_postprocess);}
    bool   IsPostProcessed()      const {return GetFlag(Flag_postprocessed);}
    void   SetPostProcessed()           {SetFlag(Flag_postprocessed);}
    void   ClearPostProcessed()         {ClrFlag(Flag_postprocessed);}
    bool   IsOnceOnly()           const {return GetFlag(Flag_onceonly);}
    void   SetOnceOnly()                {SetFlag(Flag_onceonly);}

    bool   IsRejected()           const {return GetFlag(Flag_rejected);}
    void   SetRejected()                {SetFlag(Flag_rejected); data->recstart = data->recstop = 0;}
    bool   IsRecorded()           const {return GetFlag(Flag_recorded);}
    void   SetRecorded()                {SetFlag(Flag_recorded);}
    void   ClearRecorded()              {ClrFlag(Flag_recorded);}
    bool   IsScheduled()          const {return GetFlag(Flag_scheduled);}
    void   SetScheduled()               {SetFlag(Flag_scheduled);}
    void   ClearScheduled()             {ClrFlag(Flag_scheduled);}
    bool   IsPostProcessing()     const {return GetFlag(Flag_postprocessing);}
    void   SetPostProcessing()          {SetFlag(Flag_postprocessing);}
    void   ClearPostProcessing()        {ClrFlag(Flag_postprocessing);}
    bool   IsDVBCardSpecified()   const {return GetFlag(Flag_dvbcardspecified);}
    void   SetDVBCardSpecified()        {SetFlag(Flag_dvbcardspecified);}
    bool   IsRecordingComplete()  const {return !GetFlag(Flag_incompleterecording);}
    void   SetRecordingComplete();
    bool   IgnoreRecording()      const {return GetFlag(Flag_ignorerecording);}
    void   SetIgnoreRecording(bool set = true) {SetFlag(Flag_ignorerecording, set);}
    bool   HasRecordFailed()      const {return GetFlag(Flag_recordfailed);}
    void   SetRecordFailed()            {SetFlag(Flag_recordfailed);}
    void   ClearRecordFailed()          {ClrFlag(Flag_recordfailed);}
    bool   HasPostProcessFailed() const {return GetFlag(Flag_postprocessfailed);}
    void   SetPostProcessFailed()       {SetFlag(Flag_postprocessfailed);}
    void   ClearPostProcessFailed()     {ClrFlag(Flag_postprocessfailed);}
    bool   HasConversionFailed()  const {return GetFlag(Flag_conversionfailed);}
    void   SetConversionFailed()        {SetFlag(Flag_conversionfailed);}
    void   ClearConversionFailed()      {ClrFlag(Flag_conversionfailed);}
    bool   HasFailed()            const {return (GetFlag(Flag_recordfailed) || GetFlag(Flag_postprocessfailed) || GetFlag(Flag_conversionfailed));}
    void   ClearFailed()                {data->flags &= ~(GetFlagMask(Flag_recordfailed) | GetFlagMask(Flag_postprocessfailed) | GetFlagMask(Flag_conversionfailed));}
    bool   IsNotifySet()          const {return GetFlag(Flag_notify);}
    void   SetNotify()                  {SetFlag(Flag_notify);}
    bool   IsAvailable()          const {return (IsConverted() && AStdFile::exists(GetFilename()));}
    bool   IsAvailable(bool converted) const {return AStdFile::exists(GenerateFilename(converted));}
    bool   IsArchived()           const {return AStdFile::exists(GetArchiveRecordingFilename());}
    bool   IsRecordable()         const;
    void   SetPartialPattern()          {SetFlag(Flag_partialpattern);}
    void   ClearPartialPattern()        {ClrFlag(Flag_partialpattern);}
    bool   IsPartialPattern()     const {return GetFlag(Flag_partialpattern);}
    void   SetIgnoreLateStart()         {SetFlag(Flag_ignorelatestart);}
    void   ClearIgnoreLateStart()       {ClrFlag(Flag_ignorelatestart);}
    bool   IgnoreLateStart()      const {return GetFlag(Flag_ignorelatestart);}
    void   SetRecordIfMissing()         {SetFlag(Flag_recordifmissing);}
    void   ClearRecordIfMissing()       {ClrFlag(Flag_recordifmissing);}
    bool   RecordIfMissing()      const {return GetFlag(Flag_recordifmissing);}
    bool   IsHighVideoErrorRate() const;
    bool   IsVideoErrorRateOk()   const {return !IsHighVideoErrorRate();}
    bool   DoesArchiveHaveSubtitles() const {return GetFlag(Flag_archivedhassubtitles);}

    bool   UpdateArchiveHasSubtitlesFlag();

    static void GetFlagList(std::vector<AString>& list, bool includegetonly = true);
    static bool IsFlagNameValid(const AString& name) {return (GetFlagNumber(name) >= 0);}

    bool   SetFlag(const AString& name, bool set = true);
    bool   ClrFlag(const AString& name);
    bool   GetFlag(const AString& name) const;

    sint_t GetPri()               const {return data->pri;}
    sint_t GetScore()             const {return data->score;}

    uint_t GetPreHandle()         const {return data->prehandle;}
    uint_t GetPostHandle()        const {return data->posthandle;}
    uint_t GetDVBCard()           const {return data->dvbcard;} // NOTE: virtual card number!
    uint_t GetJobID()             const {return data->jobid;}

    void   SetPri(sint_t pri)           {data->pri     = pri;}
    void   SetScore(sint_t score)       {data->score   = score;}

    void   SetDVBCard(uint8_t card)     {data->dvbcard = card;} // NOTE: virtual card number!
    void   SetJobID(uint_t id)          {data->jobid   = id;}

    bool   RecordDataValid()      const {return (data->recstart || data->recstop);}

    static bool SameProgramme(const ADVBProg& prog1, const ADVBProg& prog2);

    AString GetDescription(uint_t verbosity = 0) const;
    AString GetTitleAndSubtitle() const;
    AString GetTitleSubtitleAndChannel() const;
    AString GetQuickDescription() const;

    AString GetRecordingsSubDir() const;
    AString GenerateFilename(bool converted = false) const;
    void    GenerateRecordData(uint64_t recstarttime);

    bool WriteToJobQueue();
    bool ReadFromJob(const AString& filename);

    typedef std::vector<ADVBProg *> PROGLIST;
    typedef std::vector<PROGLIST *> PROGLISTLIST;

    void ClearList() {list = NULL;}
    void AddToList(PROGLIST *list);
    PROGLIST *GetList() const {return list;}
    void RemoveFromList();

    static int CompareProgrammesByTime(uptr_t item1, uptr_t item2, void *context);

    void   SetPriorityScore(const ADateTime& starttime);
    double GetPriorityScore() const {return priority_score;}
    bool   BiasPriorityScore(const ADVBProg& prog);

    bool   CountOverlaps(const ADVBProgList& proglist);
    uint_t GetOverlaps() const {return overlaps;}

    typedef ADVBPatterns::PATTERN PATTERN;
    bool Match(const PATTERN& pattern) const {return ADVBPatterns::Match(*this, pattern);}
    bool Match(const ADataList& patternlist) const;
    void AssignValues(const PATTERN& pattern) {ADVBPatterns::AssignValues(*this, pattern);}
    void UpdateValues(const PATTERN& pattern) {ADVBPatterns::UpdateValues(*this, pattern);}

    static int SortListByOverlaps(uptr_t item1, uptr_t item2, void *context);
    static int CompareScore(const ADVBProg& prog1, const ADVBProg& prog2);
    static int SortListByScore(uptr_t item1, uptr_t item2, void *context);

    /*--------------------------------------------------------------------------------*/
    /** Base 64 encode programme data
     */
    /*--------------------------------------------------------------------------------*/
    AString Base64Encode() const;

    AString GetLinkToFile() const;

    bool    UpdateDuration() {return GetVideoDuration(data->duration);}
    bool    UpdateFileSize();

    void Record();
    bool OnRecordSuccess() const;
    bool OnRecordFailure() const;
    bool PostRecord();
    bool PostProcess();
    bool UpdateRecordedList();

    bool IsConverted() const;
    bool ConvertVideo(bool verbose = false, bool cleanup = true, bool force = false);
    bool ForceConvertVideo(bool verbose = false, bool cleanup = true);

    void GetEncodedFiles(AList& files) const;
    bool DeleteEncodedFiles() const;

    bool GetVideoDuration(uint64_t& duration) const;
    bool GetVideoErrorCount(uint_t& count) const;

    bool FixData();

    AString GetConvertedDestinationDirectory() const;

    static AString GenerateStreamCommand(uint_t card, uint_t nsecs, const AString& pids, const AString& logfile = "/dev/null");

    static bool FilePatternExists(const AString& filename);

    static bool GetFileFormat(const AString& filename, AString& format);

    static void ModifySearchValue(const ADVBPatterns::FIELD *field, AString& value);

    static bool debugsameprogramme;

protected:
    friend class ADVBPatterns;
    typedef ADVBPatterns::FIELD   FIELD;
    typedef ADVBPatterns::VALUE   VALUE;
    typedef ADVBPatterns::TERM    TERM;

    static void     SetMarker(AString& marker, const AString& field);
    static bool     FieldExists(const AString& str, const AString& field, int p = 0, int *pos = NULL);
    static AString  GetField(const AString& str, const AString& field, int p = 0, int *pos = NULL);

    AString GetAttributedConfigItem(const AString& name, const AString& defval = "", bool defvalid = true) const;

    AString GetProgrammeKey() const;

    typedef struct {
        uint64_t start;
        uint64_t stop;
        uint64_t recstart;
        uint64_t recstop;
        uint64_t actstart;
        uint64_t actstop;

        uint64_t filesize;
        uint32_t flags;

        uint32_t videoerrors;
        uint64_t duration;

        EPISODE  episode;

        uint16_t assignedepisode;
        uint16_t year;
        uint16_t jobid;
        sint16_t score;

        uint8_t  prehandle;
        uint8_t  posthandle;
        uint8_t  dvbcard;
        sint8_t  pri;

        struct {
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
            uint16_t episodeid;
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
            uint16_t tags;
            uint16_t reserved[1];
            uint16_t end;
        } strings;

        char     strdata[0];
    } DVBPROG;

    static DVBPROG *ReadData(AStdData& fp);
    bool WriteData(AStdData& fp) const;

    uint8_t *GetDataPtr(uint16_t offset) const {return (uint8_t *)((uptr_t)data + offset);}

    static uint16_t GetDirDataOffset();
    static uint16_t GetUserDataOffset();
    static uint16_t GetActorsDataOffset();
    static uint16_t GetSubCategoryDataOffset();
    static uint16_t GetPriDataOffset();
    static uint16_t GetScoreDataOffset();

    uint64_t GetDate(const AString& str, const AString& fieldname) const;

    const char *GetString(uint16_t offset)   const {return data->strdata + offset;}
    bool StringValid(const uint16_t *offset) const {return (offset[1] > offset[0]);}
    bool SetString(const uint16_t *offset, const char *str);

    static sint_t GetFlagNumber(const AString& name, uint_t maxflags = Flag_total);

    uint32_t GetFlagMask(uint8_t flag, bool set = true) const {return set ? (1U << flag) : ~(1U << flag);}
    void     SetFlag(uint8_t flag, bool set = true);
    void     ClrFlag(uint8_t flag) {SetFlag(flag, false);}

    bool MatchString(const TERM& term, const char *str) const;

    static const FIELD *GetFields(uint_t& nfields);

    /*--------------------------------------------------------------------------------*/
    /** Initialise static (local shared) data - only done once
     */
    /*--------------------------------------------------------------------------------*/
    static void StaticInit();

    /*--------------------------------------------------------------------------------*/
    /** Initialise data for this object
     */
    /*--------------------------------------------------------------------------------*/
    void Init();

    bool    GetRecordPIDs(AString& pids, bool update = true) const;
    AString GenerateRecordCommand(uint_t nsecs, const AString& pids) const;
    AString GeneratePostRecordCommand() const;
    AString GeneratePostProcessCommand() const;

    bool ConvertVideoEx(bool verbose = false, bool cleanup = true, bool force = false);

    AString ReplaceTerms(const AString& str, bool filesystem = false) const;
    AString ReplaceFilenameTerms(const AString& str, bool converted) const;

    AString GetLogFile() const;
    bool    RunCommand(const AString& cmd, bool logoutput = false, const AString& postcmd = "", AString *result = NULL) const;

    static uint64_t CalcTime(const char *str);
    static AString GenTime(uint64_t t, const char *format = "%02u:%02u:%02u.%03u");
    static AString GetParentheses(const AString& line, int p = 0);

    typedef struct {
        AString  aspect;
        uint64_t start, length;
    } SPLIT;

    typedef struct {
        uint_t  pid;
        AString filename;
    } MEDIAFILE;

    static bool CompareMediaFiles(const MEDIAFILE& file1, const MEDIAFILE& file2);
    static bool CompareFilenames(const AString& file1, const AString& file2) {return (CompareCase(file1, file2) > 0);}

    void ConvertSubtitles(const AString& src, const AString& dst, const std::vector<SPLIT>& splits, const AString& aspect);
    bool EncodeFile(const AString& inputfiles, const AString& aspect, const AString& outputfile, bool verbose, uint32_t *videoerrors = NULL);

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
    DVBPROG            *data;
    uint16_t           maxsize;
    PROGLIST           *list;
    double             priority_score;
    uint_t             overlaps;
    AString            dircreationerrors;

    static AHash         fieldhash;
    static const FIELD   fields[];
    static AString       dayformat;
    static AString       dateformat;
    static AString       timeformat;
    static AString       fulltimeformat;
    static AString       tempfilesuffix;
    static AString       recordedfilesuffix;
    static AString       videofilesuffix;
    static AString       audiofilesuffix;
    static const AString videotrackname;
    static const AString audiotrackname;
    static const AString priorityscalename;
    static const AString overlapscalename;
    static const AString urgentscalename;
    static const AString repeatsscalename;
    static const AString delayscalename;
    static const AString latescalename;
    static const AString recordoverlapscalename;
};

#endif
