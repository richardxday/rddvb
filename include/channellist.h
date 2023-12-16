#ifndef DVB_CHANNEL_LIST_H_
#define DVB_CHANNEL_LIST_H_

#include <rapidjson/document.h>

#include <rdlib/DataList.h>
#include <rdlib/Hash.h>

#include "config.h"

class ADVBChannelList {
public:
    static ADVBChannelList& Get();
    static bool IsSingletonValid() {return GetSingleton();}

    bool Read();
    bool Write();
    void DeleteAll();

    bool Update(uint_t card, uint32_t freq, bool verbose = false);
    bool Update(uint_t card, const AString& channel, bool verbose = false);

    void UpdateAll(uint_t card, bool verbose = false);

    bool GetPIDList(uint_t card, const AString& channel, AString& pids, bool update = false);

    typedef std::vector<uint_t> pidlist_t;
    typedef struct {
        struct {
            uint_t    lcn;
            AString   channelname;
            AString   convertedchannelname;
            uint32_t  freq;
            bool      hasvideo;
            bool      hasaudio;
            bool      hassubtitle;
            pidlist_t pidlist;
        } dvb;
        struct {
            uint_t    sdchannelid;
            AString   channelid;
            AString   channelname;
            AString   convertedchannelname;
        } xmltv;
    } channel_t;

    AString GetPIDList(const pidlist_t& pidlist) const;
    AString GetPIDList(const channel_t& channel) const {return GetPIDList(channel.dvb.pidlist);}
    AString GetPIDList(const channel_t *channel) const {return GetPIDList(channel->dvb.pidlist);}

    uint_t GetChannelCount() const {return (uint_t)list.size();}
    uint_t GetLCNCount() const {return (uint_t)lcnlist.size();}
    const channel_t *GetChannel(uint_t n)      const {return (n < list.size())    ? (const channel_t *)list[n]    : NULL;}
    const channel_t *GetChannelByLCN(uint_t n) const {return (n < lcnlist.size()) ? (const channel_t *)lcnlist[n] : NULL;}

    const channel_t *GetChannelByDVBChannelName(const AString& name)   const;
    const channel_t *GetChannelByXMLTVChannelName(const AString& name) const;
    const channel_t *GetChannelByName(const AString& name) const;
    const channel_t *GetChannelByFrequencyAndPIDs(uint32_t freq, const AString& pids) const;
    const channel_t *GetChannelByFrequencyAndPIDs(uint32_t freq, const pidlist_t& pidlist) const;

    const channel_t *AssignOrAddXMLTVChannel(uint_t lcn, const AString& name, const AString& id);

    AString LookupDVBChannel(const AString& channel)   const;
    AString LookupXMLTVChannel(const AString& channel) const;

    void GenerateChanneList(rapidjson::Document& doc, rapidjson::Value& obj, bool incconverted = false) const;

    uint32_t TestCard(uint_t card, const channel_t *channel, uint_t seconds) const;

private:
    ADVBChannelList();
    ~ADVBChannelList();

    bool ForceChannelAudioAndVideoFlags(channel_t *channel);

    static bool __SortChannels(const channel_t *chan1, const channel_t *chan2);

    static AString ConvertDVBChannel(const AString& str);
    static AString ConvertXMLTVChannel(const AString& str);

protected:
    channel_t *GetChannelByName(const AString& name, bool create, uint_t lcn = 0);
    channel_t *GetChannelByDVBChannelName(const AString& name, bool create, uint_t lcn = 0);

    static bool& GetSingleton() {
        static bool valid = false;
        return valid;
    }

    typedef std::vector<channel_t *>       channellist_t;
    typedef std::map<AString, channel_t *> channelmap_t;

protected:
    channellist_t list;
    channellist_t lcnlist;
    channelmap_t  dvbchannelmap;
    channelmap_t  xmltvchannelmap;
    bool        changed;
};

#endif
