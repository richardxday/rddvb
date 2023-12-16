#ifndef __DVB_PROGLIST__
#define __DVB_PROGLIST__

#include <map>
#include <vector>

#include <rapidjson/document.h>

#include <rdlib/strsup.h>
#include <rdlib/Hash.h>
#include <rdlib/DataList.h>
#include <rdlib/Recurse.h>
#include <rdlib/StructuredNode.h>

#include "dvbprog.h"
#include "dvbpatterns.h"

class ADVBProgList {
public:
    ADVBProgList();
    ADVBProgList(const ADVBProgList& list);
    ~ADVBProgList();

    void DeleteAll();

    ADVBProgList& operator = (const ADVBProgList& list);

    const ADVBProg& operator [](uint_t n) const {return GetProg(n);}

    bool ReadFromFile(const AString& filename);
    bool ReadFromXMLTVFile(const AString& filename);
    bool ReadFromTextFile(const AString& filename);
    bool ReadFromJSONFile(const AString& filename);
    bool ReadFromJobList(bool runningonly = false);

    enum {
        Prog_Modify       = 1,
        Prog_Add          = 2,
        Prog_ModifyAndAdd = 3,
    };

    void Modify(const ADVBProgList& list, uint_t& added, uint_t& modified, uint_t mode = Prog_ModifyAndAdd, bool sort = true);
    bool ModifyFromRecordingSlave(const AString& filename, uint_t mode = Prog_ModifyAndAdd, bool sort = true);

    void AssignEpisodes(bool ignorerepeats = false);

    void UpdateDVBChannels(std::map<uint_t, bool> *sdchannelids = NULL);

    bool WriteToFile(const AString& filename, bool updatedependantfiles = true) const;
    bool WriteToTextFile(const AString& filename) const;

    ADVBProgList& FindDifferences(ADVBProgList& list1, ADVBProgList& list2, bool in1only = true, bool in2only = true);
    ADVBProgList& FindSimilarities(ADVBProgList& list1, ADVBProgList& list2);

    bool WriteToGNUPlotFile(const AString& filename) const;

    void SearchAndReplace(const AString& search, const AString& replace);

    int AddProg(const AString& prog, bool sort = true, bool removeoverlaps = false);
    int AddProg(const ADVBProg& prog, bool sort = true, bool removeoverlaps = false);
    uint_t ModifyProg(const ADVBProg& prog, uint_t mode = Prog_ModifyAndAdd, bool sort = true);

    void DeleteProgrammesBefore(const ADateTime& dt);

    uint_t Count() const {return proglist.Count();}
    const ADVBProg& GetProg(uint_t n) const;
    ADVBProg& GetProgWritable(uint_t n) const;
    bool DeleteProg(uint_t n);
    bool DeleteProg(const ADVBProg& prog);

    uint_t CountOccurances(const ADVBProg& prog) const {return CountOccurances(prog.GetUUID());}
    uint_t CountOccurances(const AString& uuid) const;

    void CreateHash();
    const ADVBProg *FindUUID(const ADVBProg& prog) const {return FindUUID(prog.GetUUID());}
    const ADVBProg *FindUUID(const AString& uuid) const;
    int             FindUUIDIndex(const ADVBProg& prog) const {return FindUUIDIndex(prog.GetUUID());}
    int             FindUUIDIndex(const AString& uuid) const;
    ADVBProg       *FindUUIDWritable(const ADVBProg& prog) const {return FindUUIDWritable(prog.GetUUID());}
    ADVBProg       *FindUUIDWritable(const AString& uuid) const;

    static void ReadPatterns(ADataList& patternlist, AString& errors, bool sort = true);

    static bool CheckDiskSpace(bool runcmd = false, rapidjson::Document *doc = NULL);
    bool CheckDiskSpaceList(bool runcmd = false, rapidjson::Document *doc = NULL) const;

    void FindProgrammes(ADVBProgList& dest, const ADataList& patternlist, uint_t maxmatches = MAX_UNSIGNED(uint_t)) const;
    void FindProgrammes(ADVBProgList& dest, const AString& patterns, AString& errors, const AString& sep = "\n", uint_t maxmatches = MAX_UNSIGNED(uint_t)) const;

    const ADVBProg *GetNextProgramme(const ADVBProg *prog, uint_t *index = NULL) const;
    const ADVBProg *GetPrevProgramme(const ADVBProg *prog, uint_t *index = NULL) const;
    const ADVBProg *FindSimilar(const ADVBProg& prog, const ADVBProg *startprog = NULL) const;
    const ADVBProg *FindLastSimilar(const ADVBProg& prog, const ADVBProg *startprog = NULL) const;
    ADVBProg *FindSimilarWritable(const ADVBProg& prog, ADVBProg *startprog = NULL);
    ADVBProg *FindLastSimilarWritable(const ADVBProg& prog, ADVBProg *startprog = NULL);
    const ADVBProg *FindCompleteRecording(const ADVBProg& prog, bool mustbeavailable = false, const ADVBProg *startprog = NULL) const;
    uint_t FindSimilarProgrammes(ADVBProgList& dest, const ADVBProg& prog, uint_t index = 0) const;

    void Sort(bool reverse = false);
    void Sort(const ADVBProg::fieldlist_t& fieldlist);
    void Sort(int (*fn)(uptr_t item1, uptr_t item2, void *pContext), void *pContext = NULL);

    static int CompareEpisode(uptr_t item1, uptr_t item2, void *pContext);

    void CountOverlaps(const ADVBProg::proglistlist_t& repeatlists, const ADateTime& starttime);
    void PrioritizeProgrammes(ADVBProgList *schedulelists, uint64_t *recstarttimes, uint_t nlists, ADVBProgList& rejectedlist, const ADateTime& starttime);
    uint_t Schedule(const ADateTime& starttime = ADateTime().TimeStamp(true));

    typedef struct {
        AString              title;
        std::vector<AString> list;
    } series_t;
    typedef std::map<AString,series_t> serieslist_t;
    void FindSeries(serieslist_t& serieslist) const;

    void StripFilmsAndSeries(const serieslist_t& serieslist);

    typedef struct {
        double recordedfactor;
        double scheduledfactor;
        double rejectedfactor;
        double priorityfactor;
        AHash  userfactors;
    } popularityfactors_t;
    static double ScoreProgrammeByPopularityFactors(const ADVBProg& prog, void *context);
    void FindPopularTitles(AList& list, const popularityfactors_t& factors) const {FindPopularTitles(list, &ScoreProgrammeByPopularityFactors, (void *)&factors);}
    void FindPopularTitles(AList& list, double (*fn)(const ADVBProg& prog, void *context), void *context = NULL) const;

    void EnhanceListings();

    typedef struct {
        bool   valid;
        double offset;
        double rate;
        double timeoffset;
    } trend_t;
    trend_t CalculateTrend(const ADateTime& startdate = ADateTime::MinDateTime, const ADateTime& enddate = ADateTime::MaxDateTime) const;

    typedef struct {
        ADateTime start;
        ADateTime end;
        uint_t card;
    } timegap_t;
    timegap_t FindGaps(const ADateTime& start, std::vector<timegap_t>& gaps, const std::map<uint_t,bool> *cardstoavoid = NULL) const;

    bool RecordImmediately(const ADateTime& dt, const AString& title, const AString& user = "", uint64_t maxminutes = 3600) const;

    bool EmailList(const AString& recipient, const AString& subject, const AString& message, uint_t verbosity = 1, bool force = false) const;

    static void UnscheduleAllProgrammes();
    static uint_t SchedulePatterns(const ADateTime& starttime = ADateTime().TimeStamp(true), bool commit = true);
    static bool WriteToJobList();

    static bool CheckFile(const AString& filename, const AString& targetfilename, const FILE_INFO& fileinfo);
    static bool CreateCombinedFile();
    static void CheckRecordingFile();
    static bool CreateGraphs(const AString& _graphsuffix = "");

    static bool GetAndConvertRecordings();
    static bool GetRecordingListFromRecordingSlave();
    static bool CheckRecordingNow();

    static void AddToList(const AString& filename, const ADVBProg& prog, bool sort = true, bool removeoverlaps = false);
    static bool RemoveFromList(const AString& filename, const ADVBProg& prog);
    static bool RemoveFromList(const AString& filename, const AString& uuid);
    static bool RemoveFromRecordLists(const ADVBProg& prog);
    static bool RemoveFromRecordLists(const AString& uuid);

protected:
    bool ReadFromJobQueue(int queue, bool runningonly = false);

    void AddXMLTVChannel(const AStructuredNode& channel);
    int  AddProg(const ADVBProg *prog, bool sort = true, bool removeoverlaps = false);

    void GetProgrammeValues(AString& str, const AStructuredNode *pNode, const AString& prefix = "") const;

    ADVBProg *FindOverlap(const ADVBProg& prog1, const ADVBProg *prog2 = NULL) const;
    ADVBProg *FindRecordOverlap(const ADVBProg& prog1, const ADVBProg *prog2 = NULL) const;

    const ADVBProg *FindFirstRecordOverlap() const;

    uint_t FindIndex(uint_t timeindex, uint64_t t) const;
    uint_t FindIndex(const ADVBProg& prog) const;

    void AdjustRecordTimes(uint64_t recstarttime);
    uint_t ScheduleEx(const ADVBProgList& runninglist, ADVBProgList& allscheduledlist, ADVBProgList& allrejectedlist, const ADateTime& starttime);

    bool ReadFromBinaryFile(const AString& filename, bool sort = false, bool removeoverlaps = false);

    static void DeleteProg(uptr_t item, void *context) {
        UNUSED(context);
        delete (ADVBProg *)item;
    }

    static void DeleteDataList(uptr_t item, void *context) {
        UNUSED(context);
        delete (ADataList *)item;
    }

    static void DeleteSeries(uptr_t item, void *context) {
        UNUSED(context);
        delete (series_t *)item;
    }

    static int CompareRepeatLists(uptr_t item1, uptr_t item2, void *context);

    static int SortProgs(uptr_t item1, uptr_t item2, void *pContext);
    static int SortProgsAdvanced(uptr_t item1, uptr_t item2, void *pContext);
    static int SortChannels(uptr_t item1, uptr_t item2, void *pContext);
    static int SortProgsByUserThenDir(uptr_t item1, uptr_t item2, void *pContext);
    //static int sortprogsbyscore(uptr_t item1, uptr_t item2, void *pContext);

    typedef ADVBPatterns::pattern_t pattern_t;
    static int SortPatterns(uptr_t item1, uptr_t item2, void *context);

    typedef struct {
        uint_t count;
        double score;
    } popularity_t;
    static void __DeletePopularity(uptr_t item, void *context) {
        UNUSED(context);
        delete (popularity_t *)item;
    }
    static bool __CollectPopularity(const AString& key, uptr_t item, void *context);
    static int  __ComparePopularity(const AListNode *pNode1, const AListNode *pNode2, void *pContext);

protected:
    AHash     proghash;
    ADataList proglist;
    bool      useproghash;

    static uint_t writefiledepth;
};

#endif
