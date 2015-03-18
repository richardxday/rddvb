
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

	if (fp.open(config.GetDVBChannelsFile()) || fp.open(config.GetChannelsConfFile())) {
		AString line;

		while (line.ReadLn(fp) >= 0) {
			CHANNEL *chan = NULL;
			AString name, _freq, _pid1, _pid2;

			if ((line.GetFieldNumber(":", 0, name) >= 0) &&
				(line.GetFieldNumber(":", 1, _freq) >= 0) &&
				(line.GetFieldNumber(":", 10, _pid1) >= 0) &&
				(line.GetFieldNumber(":", 11, _pid2) >= 0)) {
				if ((chan = GetChannel(name, true)) != NULL) {
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
				if ((chan = GetChannel(name, true)) != NULL) {
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
		{"CBBC Channel", "CBBC"},
		{" NWest", ""},
		{" +1", "+1"},
		{"Sky Three", "Sky3"},
		{"More 4", "More4"},
		{"5 USA", "5USA"},
		{"price-drop", "price drop"},
		{"Al Jazeera Eng", "Al Jazeera English"},
	};

	return ReplaceStrings(str, replacements, NUMBEROF(replacements));
}

const ADVBChannelList::CHANNEL *ADVBChannelList::GetChannel(const AString& name) const
{
	return (const CHANNEL *)hash.Read(name);
}

ADVBChannelList::CHANNEL *ADVBChannelList::GetChannel(const AString& name, bool create)
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

void ADVBChannelList::Update(uint32_t freq, bool verbose)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AStdFile fp;
	AString  sifilename;
	AString  cmd;
	bool success = false;

	if (verbose) config.printf("Scanning %uHz...", freq);

	sifilename = config.GetTempFile("pids", ".txt");

	cmd.printf("dvbtune -c %u -f %u -i >%s 2>>%s", config.GetPhysicalDVBCard(0), freq, sifilename.str(), config.GetLogFile(ADateTime().GetDays()).str());

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
						CHANNEL *chan = GetChannel(servname, true);

						if (chan) {
							ADataList pidlist;
							uint_t i, n = service.CountLines("\n", 0);
							bool hasaudio = false;
							bool hasvideo = false;
							
							chan->freq = freq;

							for (i = 0; i < n; i++) {
								line = service.Line(i, "\n", 0);

								if (line.Left(8) == "<stream ") {
									uint_t type = (uint_t)line.GetField("type=\"", "\"");
									uint_t pid  = (uint_t)line.GetField("pid=\"", "\"");
									bool include = false;

									if (RANGE(type, 1, 2)) {
										include  = !hasvideo;
										hasvideo = true;
									}
									else if (RANGE(type, 3, 4)) {
										include  = !hasaudio;
										hasaudio = true;
									}

									if (include) pidlist.Add(pid);

									if (verbose) config.printf("Service %s Stream type %u pid %u (%s)", servname.str(), type, pid, include ? "included" : "EXCLUDED");
								}
							}

							pidlist.Sort(&__CompareItems);

							if (pidlist != chan->pidlist) {
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
}

void ADVBChannelList::Update(const AString& channel, bool verbose)
{
	const CHANNEL *chan;

	if ((chan = GetChannel(channel)) != NULL) {
		Update(chan->freq, verbose);
	}
}

int ADVBChannelList::__CompareItems(uptr_t a, uptr_t b, void *context)
{
	UNUSED(context);
	return COMPARE_ITEMS(a, b);
}

void ADVBChannelList::UpdateAll(bool verbose)
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADataList freqlist;
	AStdFile  fp;

	// ensure frequencies do not appear in the list more than once!
	freqlist.EnableDuplication(false);

	// use old channels.conf to scan the *useful* frequencies
	if (fp.open(config.GetChannelsConfFile())) {
		AString   line;

		while (line.ReadLn(fp) >= 0) {
			AString freq;

			if (line.GetFieldNumber(":", 1, freq) >= 0) {
				// any duplicates will be ignored
				freqlist.Add((uptr_t)freq);
			}
		}

		fp.close();
	}

	// use new channels.dat to scan the *useful* frequencies
	if (fp.open(config.GetDVBChannelsFile())) {
		AString line;

		while (line.ReadLn(fp) >= 0) {
			// any duplicates will be ignored
			freqlist.Add((uptr_t)line.Column(1));
		}

		fp.close();
	}

	freqlist.Sort(&__CompareItems);
	
	uint_t i;
	for (i = 0; i < freqlist.Count(); i++) {
		Update(freqlist[i], verbose);
	}
}

AString ADVBChannelList::GetPIDList(const AString& channel, bool update)
{
	const ADVBConfig& config = ADVBConfig::Get();
	const CHANNEL *chan;
	AString pids;

	if (update) Update(channel);

	if ((chan = GetChannel(channel)) != NULL) {
		pids.printf("%u", chan->freq);

		uint_t i;
		for (i = 0; i < chan->pidlist.Count(); i++) {
			uint_t pid = (uint_t)chan->pidlist[i];

			pids.printf(" %u", pid);
		}

		AString str;
		if ((str = config.GetExtraDVBPIDs()).Valid()) {
			pids.printf(" %s", str.str());
		}
	}

	return pids.Words(0, 8);
}

AString ADVBChannelList::LookupDVBChannel(const AString& channel) const
{
	const CHANNEL *chan;
	AString channel1;

	if ((chan = GetChannel(channel)) != NULL) {
		channel1 = chan->name;
	}

	return channel1;
}

