
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "channellist.h"

ADVBChannelList::ADVBChannelList() : changed(false)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AStdFile fp;

	if (fp.open(config.GetDVBChannelsFile()) /*|| fp.open(config.GetChannelsConfFile())*/) {
		AString line;

		while (line.ReadLn(fp) >= 0) {
			CHANNEL *chan = NULL;
			AString name, _freq, _pid1, _pid2;

			if ((line.GetFieldNumber(":", 0, name) >= 0) &&
				(line.GetFieldNumber(":", 1, _freq) >= 0) &&
				(line.GetFieldNumber(":", 10, _pid1) >= 0) &&
				(line.GetFieldNumber(":", 11, _pid2) >= 0)) {
				if ((chan = GetChannelByName(name, true)) != NULL) {
					chan->lcn = 0;
					chan->freq = (uint32_t)_freq;
					chan->pidlist.clear();

					if ((uint_t)_pid1) {
						chan->pidlist.push_back((uint_t)_pid1);
					}
					if ((uint_t)_pid2) {
						chan->pidlist.push_back((uint_t)_pid2);
					}

					changed = true;
				}
			}
			else if ((name = line.Column(0)).Valid() && (_freq = line.Column(1)).Valid()) {
				uint_t lcn = 0;

				if (sscanf(name.str(), "[%u]", &lcn) > 0) {
					name = name.Mid(name.Pos("]") + 1);
				}

				if ((chan = GetChannelByName(name, true)) != NULL) {
					chan->lcn = lcn;
					chan->freq = (uint32_t)_freq;
					chan->pidlist.clear();

					debug("Channel %03u: %s\n", lcn, name.str());
					
					uint_t i, n = line.CountColumns();
					for (i = 2; (i < n); i++) {
						AString col = line.Column(i);

						chan->pidlist.push_back((uint_t)col);
					}
				}
			}
		}

		fp.close();
	}
}

ADVBChannelList::~ADVBChannelList()
{
	if (changed) {
		const ADVBConfig& config = ADVBConfig::Get();
		std::vector<AString> strlist;
		AStdFile fp;
		uint_t i;

		for (i = 0; i < list.size(); i++) {
			const CHANNEL& chan = *list[i];
			AString str;
			uint_t  j;

			if (chan.lcn) {
				str.printf("[%u]", chan.lcn);
			}
			
			str.printf("%s,%u", chan.name.str(), chan.freq);
			
			for (j = 0; j < chan.pidlist.size(); j++) {
				str.printf(",%u", chan.pidlist[j]);
			}

			strlist.push_back(str);
		}

		std::sort(strlist.begin(), strlist.end());

		if (fp.open(config.GetDVBChannelsFile(), "w")) {
			for (i = 0; i < strlist.size(); i++) {
				fp.printf("%s\n", strlist[i].str());
			}

			fp.close();
		}

		for (i = 0; i < list.size(); i++) {
			delete list[i];
		}
	}
}

ADVBChannelList& ADVBChannelList::Get()
{
	static ADVBChannelList channellist;
	return channellist;
}

AString ADVBChannelList::ConvertDVBChannel(const AString& str)
{
	static std::vector<REPLACEMENT> replacements;
	const ADVBConfig& config = ADVBConfig::Get();

	if (!replacements.size()) config.ReadReplacementsFile(replacements, config.GetDVBReplacementsFile());

	return ReplaceStrings(str, &replacements[0], (uint_t)replacements.size());
}

const ADVBChannelList::CHANNEL *ADVBChannelList::GetChannelByName(const AString& name) const
{
	CHANNELMAP::const_iterator it;
	const AString chname = name.SearchAndReplace("_", " ");
	const CHANNEL *chan = NULL;

	if ((it = map.find(chname.ToLower())) != map.end()) {
		chan = it->second;
	}

	return chan;
}

ADVBChannelList::CHANNEL *ADVBChannelList::GetChannelByName(const AString& name, bool create)
{
	CHANNELMAP::const_iterator it;
	const AString chname = name.SearchAndReplace("_", " ");
	CHANNEL *chan = NULL;

	if ((it = map.find(chname.ToLower())) != map.end()) {
		chan = it->second;
	}

	if (!chan && create && ((chan = new CHANNEL) != NULL)) {
		chan->name = chname;
		chan->convertedname = ConvertDVBChannel(chname);

		list.push_back(chan);
		map[chan->name.ToLower()] = chan;

		if (chan->convertedname.ToLower() != chan->name.ToLower()) {
			map[chan->convertedname.ToLower()] = chan;
		}
		
		if (chan->lcn) {
			if (chan->lcn >= lcnlist.size()) {
				lcnlist.resize(chan->lcn + 1);
			}
			lcnlist[chan->lcn] = chan;
		}
		
		changed = true;
	}

	return chan;
}

bool ADVBChannelList::Update(uint_t card, uint32_t freq, bool verbose)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AStdFile fp;
	AString  sifilename;
	AString  cmd;
	bool success = false;

	if (verbose) config.printf("Scanning %uHz...", freq);

	sifilename = config.GetTempFile("pids", ".txt");

	cmd.printf("timeout 20s dvbtune -c %u -f %u -i >%s 2>>%s", card, (uint_t)freq, sifilename.str(), config.GetLogFile(ADateTime().GetDays()).str());

	if (system(cmd) == 0) {
		if (fp.open(sifilename)) {
			AString service;
			AString line;
			AString servname;

			while (!success && (line.ReadLn(fp) >= 0)) {
				line = line.Words(0);

				if (line.Left(13) == "<description ") {
					servname = line.GetField("service_name=\"", "\"").DeHTMLify();
				}

				if (line.Left(8) == "<service") {
					service = line + "\n";
				}
				else {
					service += line + "\n";

					if (line == "</service>") {
						CHANNEL *chan = GetChannelByName(servname, true);

						if (chan) {
							std::map<AString,bool> pidhash;
							PIDLIST				   pidlist;
							uint_t i, n = service.CountLines("\n", 0);

							chan->freq = freq;

							for (i = 0; i < n; i++) {
								line = service.Line(i, "\n", 0);

								if (line.Left(8) == "<stream ") {
									uint_t  type = (uint_t)line.GetField("type=\"", "\"");
									uint_t  pid  = (uint_t)line.GetField("pid=\"", "\"");
									AString id;
									bool    include = false;

									if	    (RANGE(type, 1, 2)) id = "video";
									else if (RANGE(type, 3, 4)) id = "audio";
									//else if (type == 5)			include = true;
									else if (type == 6)			include = true;

									if (id.Valid() && (pidhash.find(id) == pidhash.end())) {
										pidhash[id] = true;
										include = true;
									}

									if (include) pidlist.push_back(pid);

									if (verbose) config.printf("Service %s Stream type %u pid %u (%s)", servname.str(), type, pid, include ? "included" : "EXCLUDED");
								}
							}

							std::sort(pidlist.begin(), pidlist.end(), std::less<uint_t>());

							if (pidlist.size() && (pidlist != chan->pidlist)) {
								AString str;

								str.printf("Changing PID list for '%s' from '", chan->name.str());
								for (i = 0; i < chan->pidlist.size(); i++) str.printf(" %s", AValue(chan->pidlist[i]).ToString().str());

								str.printf("' to '");
								for (i = 0; i < pidlist.size(); i++) str.printf(" %s", AValue(pidlist[i]).ToString().str());
								str.printf("'");

								config.printf("%s", str.str());

								chan->pidlist = pidlist;
								changed = true;
							}
						}

						service.Delete();
						servname.Delete();
					}
				}
			}

			fp.close();
		}
	}

	remove(sifilename);

	return success;
}

bool ADVBChannelList::Update(uint_t card, const AString& channel, bool verbose)
{
	const CHANNEL *chan;
	bool success = false;

	if ((chan = GetChannelByName(channel)) != NULL) {
		success = Update(card, chan->freq, verbose);
	}

	return success;
}

int ADVBChannelList::__CompareItems(uptr_t a, uptr_t b, void *context)
{
	UNUSED(context);
	return COMPARE_ITEMS(a, b);
}

void ADVBChannelList::UpdateAll(uint_t card, bool verbose)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString range = config.GetDVBFrequencyRange();
	double  f1    = (double)range.Column(0);
	double  f2    = (double)range.Column(1);
	double  step  = (double)range.Column(2);
	double  f;

	for (f = f1; f <= f2; f += step) {
		Update(card, (uint32_t)(f * 1.0e6), verbose);
	}
}

bool ADVBChannelList::GetPIDList(uint_t card, const AString& channel, AString& pids, bool update)
{
	const ADVBConfig& config = ADVBConfig::Get();
	const CHANNEL *chan;
	AString str;
	bool    success = true;

	if (update) success &= Update(card, channel);

	if ((chan = GetChannelByName(channel)) != NULL) {
		pids.printf("%u", chan->freq);

		if ((str = config.GetPriorityDVBPIDs()).Valid()) {
			pids.printf(" %s", str.str());
		}

		uint_t i;
		for (i = 0; i < chan->pidlist.size(); i++) {
			uint_t pid = (uint_t)chan->pidlist[i];

			pids.printf(" %u", pid);
		}

		if ((str = config.GetExtraDVBPIDs()).Valid()) {
			pids.printf(" %s", str.str());
		}

		pids = pids.Words(0, 9);
	}
	else {
		config.printf("Failed to find channel '%s'", channel.str());
		success = false;
	}

	return success;
}

AString ADVBChannelList::LookupDVBChannel(const AString& channel) const
{
	const CHANNEL *chan;
	AString channel1;

	if ((chan = GetChannelByName(channel)) != NULL) {
		channel1 = chan->name;
	}

	return channel1;
}

