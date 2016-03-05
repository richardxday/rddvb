#ifndef __DVB_MISC__
#define __DVB_MISC__

#include <rdlib/misc.h>

extern AString CatPath(const AString& dir1, const AString& dir2);

extern AString JSONFormat(const AString& str);
extern AString JSONTime(uint64_t dt);

typedef struct {
	const char *search;
	const char *replace;
} REPLACEMENT;
extern AString ReplaceStrings(const AString& str, const REPLACEMENT *replacements, uint_t n);

extern bool SendFile(const AString& filename);
extern bool GetFile(const AString& filename);
extern bool RunRemoteCommand(const AString& cmd);
extern bool SendFileRunCommand(const AString& filename, const AString& cmd);
extern bool RunCommandGetFile(const AString& cmd, const AString& filename);

#endif

