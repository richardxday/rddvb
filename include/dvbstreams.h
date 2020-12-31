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
    AString  htmlfile;
    AString  url;
} dvbstream_t;

extern bool ListDVBStreams(std::vector<dvbstream_t>& activestreams, const AString& pattern = ".+");
extern bool StartDVBStream(dvbstreamtype_t type, const AString& name, const AString& dvbcardstr = "");
extern bool StopDVBStream(const AString& name, std::vector<dvbstream_t>& stoppedstreams);
extern bool StopDVBStreams(const AString& pattern, std::vector<dvbstream_t>& stoppedstreams);

#endif /* __DVBSTREAMS_H__ */
