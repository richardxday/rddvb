#ifndef __EPISODE_HANDLER__
#define __EPISODE_HANDLER__

#include <rdlib/Hash.h>

#include "config.h"
#include "dvbprog.h"

class ADVBEpisodeHandler {
public:
	ADVBEpisodeHandler();
	~ADVBEpisodeHandler();

	void AssignEpisode(ADVBProg& prog, bool ignorerepeats = false);

protected:
	static bool __WriteString(const char *key, uptr_t value, void *context);

	uint32_t GetDayOffset(const AString& key) const;
	void   	 SetDayOffset(const AString& key, uint32_t value);
	uint32_t GetEpisode(const AString& key) const;
	void     SetEpisode(const AString& key, uint32_t value);

protected:
	AHash dayoffsethash;
	AHash episodehash;
	bool  changed;
};

#endif

