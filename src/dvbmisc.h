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

extern bool SendFileToRecordingHost(const AString& filename);
extern bool GetFileFromRecordingHost(const AString& filename);
extern bool GetFileFromRecordingHost(const AString& filename, const AString& localfilename);
extern bool RunRemoteCommand(const AString& cmd);
extern bool SendFileRunRemoteCommand(const AString& filename, const AString& cmd);
extern bool RunRemoteCommandGetFile(const AString& cmd, const AString& filename);

#endif

