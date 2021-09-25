#ifndef __DVB_MISC__
#define __DVB_MISC__

#include <vector>
#include <map>

#include <rdlib/misc.h>
#include <rdlib/strsup.h>

extern AString CatPath(const AString& dir1, const AString& dir2);

extern uint64_t JSONTimeOffset(uint64_t dt);

typedef struct {
    AString search;
    AString replace;
} REPLACEMENT;
extern AString ReplaceStrings(const AString& str, const REPLACEMENT *replacements, uint_t n);

extern bool RunAndLogCommand(const AString& cmd);
extern uint_t GetRemotePort(bool streamslave = false);
extern AString GetRemoteHostID(bool streamslave = false);
extern bool SendFileToRecordingSlave(const AString& filename);
extern bool GetFileFromRecordingSlave(const AString& filename);
extern bool GetFileFromRecordingSlave(const AString& filename, const AString& localfilename);
extern AString GetRemoteCommand(const AString& cmd, const AString& postcmd, bool compress = true, bool streamslave = false);
extern bool RunAndLogRemoteCommand(const AString& cmd, const AString& postcmd = "", bool compress = true);
extern bool SendFileRunRemoteCommand(const AString& filename, const AString& cmd);
extern bool RunRemoteCommandGetFile(const AString& cmd, const AString& filename);

extern bool TriggerServerCommand(const AString& cmd);

extern bool SameFile(const AString& file1, const AString& file2);

extern bool CopyFile(AStdData& fp1, AStdData& fp2);
extern bool CopyFile(const AString& src, const AString& dst, bool binary = true);
extern bool MoveFile(const AString& src, const AString& dst, bool binary = true);

extern AString SanitizeString(const AString& str, bool filesystem = false, bool dir = false);

typedef struct {
    uint32_t pid;
    uint32_t freq;
    uint_t   pcard;     ///< physical card
    uint_t   vcard;     ///< virtual card
    uint32_t time;
    std::vector<uint_t> pids;
} dvbstreamprocs_t;
extern bool FindActiveStreamingProcesses(std::vector<dvbstreamprocs_t>& procs);

extern AString RunCommandAndGetResult(const AString& cmd);
extern AString GetCommandFromPID(uint32_t pid);
extern AString FindChildCommandsFromPID(uint32_t pid);

class APIDTree
{
public:
    APIDTree(uint32_t _pid);
    APIDTree(uint32_t _pid, const AString& _cmd);
    APIDTree(const AString& description);
    APIDTree(const AString& description, int& ln);
    ~APIDTree();

    bool Valid() const {return ((pid > 0) && cmd.Valid());}

    const std::vector<const APIDTree *>& GetChildren() const {return children;}

    bool Kill() const;

    AString Describe() const {return DescribeEx(0);}

protected:
    void FindChildren();
    void Populate(const AString& description, int& ln);

    AString DescribeEx(uint_t level) const;

protected:
    std::vector<const APIDTree *> children;
    uint32_t pid;
    AString  cmd;
};

extern uint32_t TestCard(uint_t card, uint32_t freq, const AString& pidlist, uint_t seconds);
extern uint32_t TestCard(uint_t card, const AString& channel, uint_t seconds);

#endif
