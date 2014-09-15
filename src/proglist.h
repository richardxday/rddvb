#ifndef __DVB_PROGLIST__
#define __DVB_PROGLIST__

#include <stdarg.h>

#include <rdlib/strsup.h>
#include <rdlib/Hash.h>
#include <rdlib/DataList.h>

#include "dvbprog.h"

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
	bool ReadRadioListings();
	bool ReadFromJobList(bool runningonly = false);
	
	void UpdateDVBChannels();

	bool WriteToFile(const AString& filename, bool updatecombined = true) const;
	bool WriteToTextFile(const AString& filename) const;

	void SearchAndReplace(const AString& search, const AString& replace);

	int AddProg(const AString& prog, bool sort = true, bool removeoverlaps = false, bool reverseorder = false);
	int AddProg(const ADVBProg& prog, bool sort = true, bool removeoverlaps = false, bool reverseorder = false);

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
	const CHANNEL *GetChannel(uint_t n) {return (const CHANNEL *)channellist[n];}
	const CHANNEL *GetChannel(const AString& id) const {return (const CHANNEL *)channelhash.Read(id);}

	void CreateHash();
	const ADVBProg *FindUUID(const ADVBProg& prog) const {return FindUUID(prog.GetUUID());}
	const ADVBProg *FindUUID(const AString& uuid) const;
	ADVBProg 	   *FindUUIDWritable(const ADVBProg& prog) const {return FindUUIDWritable(prog.GetUUID());}
	ADVBProg 	   *FindUUIDWritable(const AString& uuid) const;

	static void ReadPatterns(ADataList& patternlist, AString& errors, bool sort = true);

	static bool CheckDiskSpace(bool runcmd = false, bool report = false);
	static bool CheckDiskSpace(const ADataList& patternlist, bool runcmd = false, bool report = false);

	void FindProgrammes(ADVBProgList& dest, const ADataList& patternlist, uint_t maxmatches = MAX_UNSIGNED(uint_t)) const;
	void FindProgrammes(ADVBProgList& dest, const AString& patterns, AString& errors, const AString& sep = "\n", uint_t maxmatches = MAX_UNSIGNED(uint_t)) const;

	const ADVBProg *FindSimilar(const ADVBProg& prog) const;
	uint_t FindSimilar(ADVBProgList& dest, const ADVBProg& prog, uint_t index = 0) const;

	void Sort(bool reverse = false);

	void PrioritizeProgrammes(ADVBProgList& scheduledlist, ADVBProgList& rejectedlist);
	uint_t Schedule(const ADateTime& starttime = ADateTime().TimeStamp(true));

	void SimpleSchedule();

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

	static void UnscheduleAllProgrammes();
	static uint_t SchedulePatterns(const ADateTime& starttime = ADateTime().TimeStamp(true), bool commit = true);
	static bool WriteToJobList();

	static void CreateCombinedList();

protected:
	CHANNEL *GetChannelWritable(const AString& id) const {return (CHANNEL *)channelhash.Read(id);}
	void DeleteChannel(const AString& id);

	AString LookupXMLTVChannel(const AString& id) const;

	bool ReadFromJobQueue(int queue, bool runningonly = false);

	bool ValidChannelID(const AString& channelid) const;

	void AddXMLTVChannel(const AString& channel);
	void AddChannel(const AString& id, const AString& name);
	int  AddProg(const ADVBProg *prog, bool sort = true, bool removeoverlaps = false, bool reverseorder = false);

	ADVBProg *FindOverlap(const ADVBProg& prog1, const ADVBProg *prog2 = NULL) const;
	ADVBProg *FindRecordOverlap(const ADVBProg& prog1, const ADVBProg *prog2 = NULL) const;

	const ADVBProg *FindFirstRecordOverlap() const;

	void AdjustRecordTimes();
	uint_t ScheduleEx(ADVBProgList& recordedlist, ADVBProgList& allscheduledlist, ADVBProgList& allrejectedlist, const ADateTime& starttime, uint_t card);

	bool ReadFromBinaryFile(const AString& filename, bool sort = false, bool removeoverlaps = false, bool reverseorder = false);
	bool ReadRadioListingsFromHTMLFile(const AString& filename, const AString& channel, const AString& channelid, uint32_t day, bool removeoverlaps);

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

	typedef ADVBProg::PATTERN PATTERN;
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
