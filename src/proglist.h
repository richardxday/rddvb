#ifndef __DVB_PROGLIST__
#define __DVB_PROGLIST__

#include <stdarg.h>

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
	bool ReadFromJobList(bool runningonly = false);

	uint_t Merge(const ADVBProgList& list, bool sort = true, bool removeoverlaps = false);

	void UpdateDVBChannels();

	bool WriteToFile(const AString& filename, bool updatedependantfiles = true) const;
	bool WriteToTextFile(const AString& filename) const;

	void SearchAndReplace(const AString& search, const AString& replace);

	int AddProg(const AString& prog, bool sort = true, bool removeoverlaps = false);
	int AddProg(const ADVBProg& prog, bool sort = true, bool removeoverlaps = false);
	int OverwriteOrAddProg(const ADVBProg& prog, bool sort = true);
	
	void AssignEpisodes(bool reverse = false, bool ignorerepeats = false);

	void DeleteProgrammesBefore(const ADateTime& dt);

	uint_t Count() const {return proglist.Count();}
	const ADVBProg& GetProg(uint_t n) const;
	ADVBProg& GetProgWritable(uint_t n) const;
	bool DeleteProg(uint_t n);
	bool DeleteProg(const ADVBProg& prog);

	typedef struct {
		AString id;
		AString name;
	} CHANNEL;
	uint_t ChannelCount() const {return channellist.Count();}
	const CHANNEL *GetChannelIndex(uint_t n)		 const {return (const CHANNEL *)channellist[n];}
	const CHANNEL *GetChannelByID(const AString& id) const {return (const CHANNEL *)channelhash.Read(id);}

	void CreateHash();
	const ADVBProg *FindUUID(const ADVBProg& prog) const {return FindUUID(prog.GetUUID());}
	const ADVBProg *FindUUID(const AString& uuid) const;
	ADVBProg 	   *FindUUIDWritable(const ADVBProg& prog) const {return FindUUIDWritable(prog.GetUUID());}
	ADVBProg 	   *FindUUIDWritable(const AString& uuid) const;

	static void ReadPatterns(ADataList& patternlist, AString& errors, bool sort = true);

	static bool CheckDiskSpace(bool runcmd = false, bool report = false);

	void FindProgrammes(ADVBProgList& dest, const ADataList& patternlist, uint_t maxmatches = MAX_UNSIGNED(uint_t)) const;
	void FindProgrammes(ADVBProgList& dest, const AString& patterns, AString& errors, const AString& sep = "\n", uint_t maxmatches = MAX_UNSIGNED(uint_t)) const;

	const ADVBProg *FindSimilar(const ADVBProg& prog, const ADVBProg *startprog = NULL) const;
	ADVBProg       *FindSimilarWritable(const ADVBProg& prog, ADVBProg *startprog = NULL);
	const ADVBProg *FindCompleteRecording(const ADVBProg& prog) const;

	void ModifyProgs(const ADVBProg&     prog, bool similar = false);
	void ModifyProgs(const ADVBProgList& list, bool similar = false);
	static void ModifyFile(const AString& filename, const ADVBProg& prog, bool similar = false);

	uint_t FindSimilarProgrammes(ADVBProgList& dest, const ADVBProg& prog, uint_t index = 0) const;

	void Sort(bool reverse = false);
	void Sort(int (*fn)(uptr_t item1, uptr_t item2, void *pContext), void *pContext = NULL);

	void PrioritizeProgrammes(uint_t card, ADVBProgList& scheduledlist, ADVBProgList& rejectedlist, uint64_t recstarttime);
	uint_t Schedule(const ADateTime& starttime = ADateTime().TimeStamp(true));

	typedef struct {
		AString   title;
		ADataList list;
	} SERIES;
	void FindSeries(AHash& hash) const;
	
	typedef struct {
		double recordedfactor;
		double scheduledfactor;
		double rejectedfactor;
		double priorityfactor;
		AHash  userfactors;
	} POPULARITY_FACTORS;
	static double ScoreProgrammeByPopularityFactors(const ADVBProg& prog, void *context);
	void FindPopularTitles(AList& list, const POPULARITY_FACTORS& factors) const {FindPopularTitles(list, &ScoreProgrammeByPopularityFactors, (void *)&factors);}
	void FindPopularTitles(AList& list, double (*fn)(const ADVBProg& prog, void *context), void *context = NULL) const;

	void EnhanceListings();
	
	static void UnscheduleAllProgrammes();
	static uint_t SchedulePatterns(const ADateTime& starttime = ADateTime().TimeStamp(true), bool commit = true);
	static bool WriteToJobList();

	static bool CheckFile(const AString& filename, const AString& targetfilename, const FILE_INFO& fileinfo);
	static void CreateCombinedFile();
	static void CheckRecordingFile();

	static void AddToList(const AString& filename, const ADVBProg& prog, bool sort = true, bool removeoverlaps = false);
	static void RemoveFromList(const AString& filename, const ADVBProg& prog);
	
protected:
	CHANNEL *GetChannelWritable(const AString& id) const {return (CHANNEL *)channelhash.Read(id);}
	void DeleteChannel(const AString& id);

	AString LookupXMLTVChannel(const AString& id) const;

	bool ReadFromJobQueue(int queue, bool runningonly = false);

	bool ValidChannelID(const AString& channelid) const;

	void AddXMLTVChannel(const AStructuredNode& channel);
	void AddChannel(const AString& id, const AString& name);
	int  AddProg(const ADVBProg *prog, bool sort = true, bool removeoverlaps = false);

	void GetProgrammeValues(AString& str, const AStructuredNode *pNode, const AString& prefix = "") const;

	ADVBProg *FindOverlap(const ADVBProg& prog1, const ADVBProg *prog2 = NULL) const;
	ADVBProg *FindRecordOverlap(const ADVBProg& prog1, const ADVBProg *prog2 = NULL) const;

	const ADVBProg *FindFirstRecordOverlap() const;

	uint_t FindIndex(uint_t timeindex, uint64_t t) const;
	uint_t FindIndex(const ADVBProg& prog) const;

	void AdjustRecordTimes();
	uint_t ScheduleEx(ADVBProgList& recordedlist, ADVBProgList& allscheduledlist, ADVBProgList& allrejectedlist, const ADateTime& starttime, uint_t card);

	bool ReadFromBinaryFile(const AString& filename, bool sort = false, bool removeoverlaps = false);

	static void DeleteChannel(uptr_t item, void *context) {
		CHANNEL *channel = (CHANNEL *)item;
		UNUSED(context);
		delete channel;
	}

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
		delete (SERIES *)item;
	}

	static int SortDataLists(uptr_t item1, uptr_t item2, void *context);

	static int SortProgs(uptr_t item1, uptr_t item2, void *pContext);
	static int SortChannels(uptr_t item1, uptr_t item2, void *pContext);
	static int SortProgsByUserThenDir(uptr_t item1, uptr_t item2, void *pContext);
	//static int sortprogsbyscore(uptr_t item1, uptr_t item2, void *pContext);

	typedef ADVBPatterns::PATTERN PATTERN;
	static int SortPatterns(uptr_t item1, uptr_t item2, void *context);

	typedef struct {
		uint_t count;
		double score;
	} POPULARITY;
	static void __DeletePopularity(uptr_t item, void *context) {
		UNUSED(context);
		delete (POPULARITY *)item;
	}
	static bool __CollectPopularity(const char *key, uptr_t item, void *context);
	static int  __ComparePopularity(const AListNode *pNode1, const AListNode *pNode2, void *pContext);

protected:
	AHash     channelhash;
	AHash	  proghash;
	ADataList proglist;
	ADataList channellist;
};

#endif
