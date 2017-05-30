#ifndef __FIND_CARDS__
#define __FIND_CARDS__

#include <vector>

#include <rdlib/strsup.h>

extern void findcards(void);
extern sint_t findcard(const AString& pattern, const std::vector<uint_t> *cardlist);

#endif
