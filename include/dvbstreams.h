#ifndef __DVBSTREAMS_H__
#define __DVBSTREAMS_H__

#include "config.h"
#include <cassert>

typedef enum {
    StreamType_Raw = 0,
    StreamType_HLS,
    StreamType_MP4,
    StreamType_RemoteHTTP,
    StreamType_HTTP,
    StreamType_Video,
} dvbstreamtype_t;

typedef struct {
    uint32_t pid;
    AString  type;
    AString  cmd;
    AString  name;
    AString  url;
    AString  htmlfile;
    AString  hlsfile;
    AString  cleanupcmd;
} dvbstream_t;

extern AString ConvertStream(const dvbstream_t& stream);
extern bool ConvertStream(const AString& base64str, dvbstream_t& stream);

extern bool ListDVBStreams(std::vector<dvbstream_t>& activestreams, const AString& pattern = ".+");
extern bool StartDVBStream(dvbstream_t& stream, dvbstreamtype_t type, const AString& _name, const AString& dvbcardstr = "", bool background = false);
extern bool StopDVBStream(const AString& name, std::vector<dvbstream_t>& stoppedstreams);
extern bool StopDVBStreams(const AString& pattern, std::vector<dvbstream_t>& stoppedstreams);
extern bool GetDVBStreamPIDTree(uint32_t pid);

#endif /* __DVBSTREAMS_H__ */
