#ifndef __DVB_CHANNEL_LIST__
#define __DVB_CHANNEL_LIST__

#include <rdlib/DataList.h>
#include <rdlib/Hash.h>

#include "config.h"

class ADVBChannelList {
public:
	static ADVBChannelList& Get();

	bool Update(uint_t card, uint32_t freq, bool verbose = false);
	bool Update(uint_t card, const AString& channel, bool verbose = false);

	void UpdateAll(uint_t card, bool verbose = false);

	bool GetPIDList(uint_t card, const AString& channel, AString& pids, bool update = false);

	AString LookupDVBChannel(const AString& channel) const;

	uint_t GetChannelCount() const {return list.size();}

	typedef std::vector<uint_t> PIDLIST;
	typedef struct {
		AString  name;
		AString  convertedname;
		uint32_t freq;
		PIDLIST  pidlist;
	} CHANNEL;
	const CHANNEL *GetChannel(uint_t n) const {return (const CHANNEL *)list[n];}

private:
	ADVBChannelList();
	~ADVBChannelList();

	static AString ConvertDVBChannel(const AString& str);

protected:
	static void __DeleteChannel(uptr_t item, void *context) {
		UNUSED(context);
		delete (CHANNEL *)item;
	}

	const CHANNEL *GetChannelByName(const AString& name) const;
	CHANNEL *GetChannelByName(const AString& name, bool create = false);

	static int __CompareItems(uptr_t a, uptr_t b, void *context);

	typedef std::vector<CHANNEL *>       CHANNELLIST;
	typedef std::map<AString, CHANNEL *> CHANNELMAP;
protected:
	CHANNELLIST list;
	CHANNELMAP  map;
	bool        changed;
};

#endif
