
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "channellist.h"

ADVBChannelList::ADVBChannelList() : hash(40),
									 changed(false)
{
	list.SetDestructor(&__DeleteChannel);
	hash.EnableCaseInSensitive(true);

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
					chan->freq = (uint32_t)_freq;
					chan->pidlist.DeleteList();
					
					if ((uint_t)_pid1) {
						chan->pidlist.Add((uint_t)_pid1);
					}
					if ((uint_t)_pid2) {
						chan->pidlist.Add((uint_t)_pid2);
					}

					changed = true;
				}
			}
			else if ((name = line.Column(0)).Valid() && (_freq = line.Column(1)).Valid()) {
				if ((chan = GetChannelByName(name, true)) != NULL) {
					chan->freq = (uint32_t)_freq;
					chan->pidlist.DeleteList();

					uint_t i, n = line.CountColumns();
					for (i = 2; (i < n) && (chan->pidlist.Count() < 2); i++) {
						AString col = line.Column(i);

						chan->pidlist.Add((uint_t)col);
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
		AStdFile fp;
		AList strlist;
		uint_t i;

		for (i = 0; i < list.Count(); i++) {
			const CHANNEL *chan = (const CHANNEL *)list[i];
			AString *str;

			if ((str = new AString) != NULL) {
				uint_t j;
				
				str->printf("%s,%u", chan->name.str(), chan->freq);

				for (j = 0; j < chan->pidlist.Count(); j++) {
					str->printf(",%u", (uint_t)chan->pidlist[j]);
				}
				
				strlist.Add(str);
			}
		}

		strlist.Sort(&AString::AlphaCompareNoCase);

		if (fp.open(config.GetDVBChannelsFile(), "w")) {
			const AString *str = AString::Cast(strlist.First());

			while (str) {
				fp.printf("%s\n", str->str());

				str = str->Next();
			}

			fp.close();
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
	static const REPLACEMENT replacements[] = {
		{"BBC ONE", "BBC1"},
		{"BBC TWO", "BBC2"},
		{"BBC THREE", "BBC3"},
		{"BBC FOUR", "BBC4"},
		{"BBC R1X", "BBC Radia 1Xtra"},
		{"BBC Radio 4 Ex", "BBC Radia 4 Extra"},
		{"CBBC Channel", "CBBC"},
		{" N West", ""},
		{" S West", ""},
		{" N East", ""},
		{" S East", ""},
		{" +1", "+1"},
		{"Sky Three", "Sky3"},
		{"More 4", "More4"},
		{"5 USA", "5USA"},
		{"price-drop", "price drop"},
		{"Al Jazeera Eng", "Al Jazeera English"},
		{"Box Nation", "BoxNation"},
		{"Create & Craft", "Create and Craft"},
	};

	return ReplaceStrings(str, replacements, NUMBEROF(replacements));
}

const ADVBChannelList::CHANNEL *ADVBChannelList::GetChannelByName(const AString& name) const
{
	return (const CHANNEL *)hash.Read(name);
}

ADVBChannelList::CHANNEL *ADVBChannelList::GetChannelByName(const AString& name, bool create)
{
	CHANNEL *chan = (CHANNEL *)hash.Read(name);

	if (!chan && create && ((chan = new CHANNEL) != NULL)) {
		chan->pidlist.EnableDuplication(false);

		chan->name = name;
		chan->convertedname = ConvertDVBChannel(name);

		list.Add((uptr_t)chan);

		hash.Insert(chan->name, (uptr_t)chan);
		hash.Insert(chan->convertedname, (uptr_t)chan);

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
							AHash     pidhash(10);
							ADataList pidlist;
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

									if (id.Valid() && !pidhash.Exists(id)) {
										pidhash.Insert(id);
										include = true;
									}

									if (include) pidlist.Add(pid);

									if (verbose) config.printf("Service %s Stream type %u pid %u (%s)", servname.str(), type, pid, include ? "included" : "EXCLUDED");
								}
							}

							pidlist.Sort(&__CompareItems);

							if (pidlist.Count() && (pidlist != chan->pidlist)) {
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
		for (i = 0; i < chan->pidlist.Count(); i++) {
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

