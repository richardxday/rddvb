#ifndef __DVB_PATTERN__
#define __DVB_PATTERN__

#include <map>

#include "dvbprog.h"

class ADVBPatterns {
public:
	~ADVBPatterns() {}

	static ADVBPatterns& Get();

	void UpdatePattern(const AString& olduser, const AString& oldpattern, const AString& newuser, const AString& newpattern);
	void UpdatePattern(const AString& user, const AString& pattern, const AString& newpattern);
	void InsertPattern(const AString& user, const AString& pattern);
	void DeletePattern(const AString& user, const AString& pattern);
	void EnablePattern(const AString& user, const AString& pattern);
	void DisablePattern(const AString& user, const AString& pattern);

protected:
	ADVBPatterns();

	bool UpdatePatternInFile(const AString& filename, const AString& pattern, const AString& newpattern);
	void AddPatternToFile(const AString& filename, const AString& pattern);

protected:
	int priscale;
	int repeatsscale;
	int urgentscale;
};

#endif
