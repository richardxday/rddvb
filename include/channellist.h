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

    typedef std::vector<uint_t> PIDLIST;
    typedef struct {
        struct {
            uint_t   lcn;
            AString  channelname;
            AString  convertedchannelname;
            uint32_t freq;
            bool     hasvideo;
            bool     hasaudio;
            bool     hassubtitle;
            PIDLIST  pidlist;
        } dvb;
        struct {
            uint_t   sdchannelid;
            AString  channelid;
            AString  channelname;
            AString  convertedchannelname;
        } xmltv;
    } CHANNEL;

    AString GetPIDList(const PIDLIST& pidlist) const;
    AString GetPIDList(const CHANNEL& channel) const {return GetPIDList(channel.dvb.pidlist);}
    AString GetPIDList(const CHANNEL *channel) const {return GetPIDList(channel->dvb.pidlist);}

    uint_t GetChannelCount() const {return (uint_t)list.size();}
    uint_t GetLCNCount() const {return (uint_t)lcnlist.size();}
    const CHANNEL *GetChannel(uint_t n)      const {return (n < list.size())    ? (const CHANNEL *)list[n]    : NULL;}
    const CHANNEL *GetChannelByLCN(uint_t n) const {return (n < lcnlist.size()) ? (const CHANNEL *)lcnlist[n] : NULL;}

    const CHANNEL *GetChannelByDVBChannelName(const AString& name)   const;
    const CHANNEL *GetChannelByXMLTVChannelName(const AString& name) const;
    const CHANNEL *GetChannelByName(const AString& name) const;
    const CHANNEL *GetChannelByFrequencyAndPIDs(uint32_t freq, const AString& pids) const;
    const CHANNEL *GetChannelByFrequencyAndPIDs(uint32_t freq, const PIDLIST& pidlist) const;

    const CHANNEL *AssignOrAddXMLTVChannel(uint_t lcn, const AString& name, const AString& id);

    AString LookupDVBChannel(const AString& channel)   const;
    AString LookupXMLTVChannel(const AString& channel) const;

    void GenerateChanneList(rapidjson::Document& doc, rapidjson::Value& obj, bool incconverted = false) const;

    uint32_t TestCard(uint_t card, const CHANNEL *channel, uint_t seconds) const;

private:
    ADVBChannelList();
    ~ADVBChannelList();

    bool ForceChannelAudioAndVideoFlags(CHANNEL *channel);

    static bool __SortChannels(const CHANNEL *chan1, const CHANNEL *chan2);

    static AString ConvertDVBChannel(const AString& str);
    static AString ConvertXMLTVChannel(const AString& str);

protected:
    CHANNEL *GetChannelByName(const AString& name, bool create, uint_t lcn = 0);
    CHANNEL *GetChannelByDVBChannelName(const AString& name, bool create, uint_t lcn = 0);

    static bool& GetSingleton() {
        static bool valid = false;
        return valid;
    }

    typedef std::vector<CHANNEL *>       CHANNELLIST;
    typedef std::map<AString, CHANNEL *> CHANNELMAP;

protected:
    CHANNELLIST list;
    CHANNELLIST lcnlist;
    CHANNELMAP  dvbchannelmap;
    CHANNELMAP  xmltvchannelmap;
    bool        changed;
};

#endif
