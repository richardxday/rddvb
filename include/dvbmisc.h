#ifndef __DVB_MISC__
#define __DVB_MISC__

#include <rdlib/misc.h>

extern AString CatPath(const AString& dir1, const AString& dir2);

extern AString JSONFormat(const AString& str);
extern AString JSONTime(uint64_t dt);

typedef struct {
	AString search;
	AString replace;
} REPLACEMENT;
extern AString ReplaceStrings(const AString& str, const REPLACEMENT *replacements, uint_t n);

extern bool RunAndLogCommand(const AString& cmd);
extern bool SendFileToRecordingSlave(const AString& filename);
extern bool GetFileFromRecordingSlave(const AString& filename);
extern bool GetFileFromRecordingSlave(const AString& filename, const AString& localfilename);
extern AString GetRemoteCommand(const AString& cmd, const AString& postcmd, bool compress = true);
extern bool RunAndLogRemoteCommand(const AString& cmd, const AString& postcmd = "", bool compress = true);
extern bool SendFileRunRemoteCommand(const AString& filename, const AString& cmd);
extern bool RunRemoteCommandGetFile(const AString& cmd, const AString& filename);

extern bool TriggerServerCommand(const AString& cmd);

extern bool SameFile(const AString& file1, const AString& file2);

extern bool CopyFile(AStdData& fp1, AStdData& fp2);
extern bool CopyFile(const AString& src, const AString& dst, bool binary = true);
extern bool MoveFile(const AString& src, const AString& dst, bool binary = true);

#endif
