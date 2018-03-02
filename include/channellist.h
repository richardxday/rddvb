#ifndef __DVB_CHANNEL_LIST__
#define __DVB_CHANNEL_LIST__

#include <rapidjson/document.h>

#include <rdlib/DataList.h>
#include <rdlib/Hash.h>

#include "config.h"

class ADVBChannelList {
public:
	static ADVBChannelList& Get();

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
			PIDLIST  pidlist;
		} dvb;
		struct {
			uint_t   sdchannelid;
			AString  channelid;
			AString  channelname;
			AString  convertedchannelname;
		} xmltv;
	} CHANNEL;

	uint_t GetChannelCount() const {return list.size();}
	uint_t GetLCNCount() const {return lcnlist.size();}
	const CHANNEL *GetChannel(uint_t n)      const {return (n < list.size())    ? (const CHANNEL *)list[n]    : NULL;}
	const CHANNEL *GetChannelByLCN(uint_t n) const {return (n < lcnlist.size()) ? (const CHANNEL *)lcnlist[n] : NULL;}

	const CHANNEL *GetChannelByDVBChannelName(const AString& name)   const;
	const CHANNEL *GetChannelByXMLTVChannelName(const AString& name) const;
	const CHANNEL *GetChannelByName(const AString& name) const;
	
	const CHANNEL *AssignOrAddXMLTVChannel(uint_t lcn, const AString& name, const AString& id);
	bool ValidChannelID(const AString& channelid) const;
	
	AString LookupDVBChannel(const AString& channel)   const;
	AString LookupXMLTVChannel(const AString& channel) const;

	void GenerateChanneList(rapidjson::Document& doc, bool incconverted = false) const;

private:
	ADVBChannelList();
	~ADVBChannelList();

	static bool __SortChannels(const CHANNEL *chan1, const CHANNEL *chan2);

	static AString ConvertDVBChannel(const AString& str);
	static AString ConvertXMLTVChannel(const AString& str);

protected:
	CHANNEL *GetChannelByDVBChannelName(const AString& name, bool create, uint_t lcn = 0);

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
