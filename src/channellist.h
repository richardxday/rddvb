#ifndef __DVB_CHANNEL_LIST__
#define __DVB_CHANNEL_LIST__

#include <rdlib/DataList.h>
#include <rdlib/Hash.h>

#include "config.h"

class ADVBChannelList {
public:
	static ADVBChannelList& Get();

	bool Update(uint32_t freq, bool verbose = false);
	bool Update(const AString& channel, bool verbose = false);

	void UpdateAll(bool verbose = false);

	bool GetPIDList(const AString& channel, AString& pids, bool update = false);

	AString LookupDVBChannel(const AString& channel) const;

private:
	ADVBChannelList();
	~ADVBChannelList();

	static AString ConvertDVBChannel(const AString& str);

protected:
	typedef struct {
		AString   name;
		AString   convertedname;
		uint32_t  freq;
		ADataList pidlist;
	} CHANNEL;

	static void __DeleteChannel(uptr_t item, void *context) {
		UNUSED(context);
		delete (CHANNEL *)item;
	}

	const CHANNEL *GetChannel(const AString& name) const;
	CHANNEL *GetChannel(const AString& name, bool create = false);

	static int __CompareItems(uptr_t a, uptr_t b, void *context);

protected:
	ADataList list;
	AHash     hash;
	bool      changed;
};

#endif
