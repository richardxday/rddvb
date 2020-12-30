#ifndef __DVBSTREAMS_H__
#define __DVBSTREAMS_H__

#include "config.h"

typedef enum {
    StreamType_Raw = 0,
    StreamType_HLS,
    StreamType_MP4,
    StreamType_Video,
} dvbstreamtype_t;

typedef struct {
    uint32_t pid;
    AString  name;
} dvbstream_t;

extern bool ListDVBStreams(const AString& pattern, std::vector<dvbstream_t>& activestreams);
extern bool StartDVBStream(dvbstreamtype_t type, const AString& name, const AString& dvbcardstr = "");
extern bool StopDVBStreams(const AString& pattern, std::vector<dvbstream_t>& stoppedstreams);

#endif /* __DVBSTREAMS_H__ */
