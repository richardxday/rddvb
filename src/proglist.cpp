
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/statvfs.h>

#include <rdlib/Regex.h>
#include <rdlib/Recurse.h>
#include <rdlib/XMLDecode.h>
#include <json/json.h>

#include "config.h"
#include "proglist.h"
#include "channellist.h"
#include "episodehandler.h"
#include "dvblock.h"
#include "dvbpatterns.h"
#include "iconcache.h"

/*--------------------------------------------------------------------------------*/

ADVBProgList::ADVBProgList()
{
	channellist.SetDestructor(&DeleteChannel);
	proglist.SetDestructor(&DeleteProg);
	proglist.EnableDuplication(true);
}

ADVBProgList::ADVBProgList(const ADVBProgList& list)
{
	channellist.SetDestructor(&DeleteChannel);
	proglist.SetDestructor(&DeleteProg);
	proglist.EnableDuplication(true);

	operator = (list);
}

ADVBProgList::~ADVBProgList()
{
}

void ADVBProgList::DeleteAll()
{
	proghash.Delete();
	proglist.DeleteList();
	channelhash.Delete();
	channellist.DeleteList();
}

ADVBProgList& ADVBProgList::operator = (const ADVBProgList& list)
{
	uint_t i;

	DeleteAll();

	for (i = 0; i < channellist.Count(); i++) {
		const CHANNEL *channel = (const CHANNEL *)list.channellist[i];

		AddChannel(channel->id, channel->name);
	}

	for (i = 0; (i < list.Count()) && !HasQuit(); i++) {
		const ADVBProg& prog = list[i];

		AddProg(prog, false, false);
	}

	return *this;
}

void ADVBProgList::Modify(const ADVBProgList& list, uint_t& added, uint_t& modified, uint_t mode, bool sort)
{
	uint_t i;

	if (!proghash.Valid()) CreateHash();

	added = modified = 0;

	for (i = 0; (i < list.Count()) && !HasQuit(); i++) {
		uint_t res = ModifyProg(list[i], mode, sort);
		if (res & Prog_Add)    added++;
		if (res & Prog_Modify) modified++;
	}
}

bool ADVBProgList::ModifyFromRecordingHost(const AString& filename, uint_t mode, bool sort)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString host;
	bool    success = false;

	DeleteAll();

	if ((host = config.GetRecordingHost()).Valid()) {
		AString dstfilename = config.GetTempFile("proglist", ".dat");

		if (GetFileFromRecordingHost(filename, dstfilename)) {
			ADVBLock     lock("dvbfiles");
			ADVBProgList list;

			if (ReadFromFile(filename)) {
				if (list.ReadFromFile(dstfilename)) {
					uint_t added, modified;

					config.logit("List (from '%s') is %u programmes long, modifying list (from '%s') is %u programmes long",
								 filename.str(), Count(),
								 dstfilename.str(), list.Count());

					Modify(list, added, modified, mode, sort);

					if (added || modified) {
						if (WriteToFile(filename)) {
							if (added || modified) config.printf("Modified programmes in '%s' from recording host, total now %u (%u added, %u modified)", filename.str(), Count(), added, modified);
							success = true;
						}
						else config.printf("Failed to write programme list back!");
					}
					else {
						//config.printf("No programmes added");
						success = true;
					}
				}
				else config.printf("Failed to read programme list from '%s'", dstfilename.str());
			}
			else config.printf("Failed to read programmes list from '%s'", filename.str());
		}
		else config.printf("Failed to get '%s' from recording host", filename.str());

		remove(dstfilename);
	}
	else config.printf("No remote host configured!");

	return success;
}

int ADVBProgList::SortProgs(uptr_t item1, uptr_t item2, void *pContext)
{
	return ADVBProg::Compare((const ADVBProg *)item1, (const ADVBProg *)item2, (const bool *)pContext);
}

int ADVBProgList::SortChannels(uptr_t item1, uptr_t item2, void *pContext)
{
	const AString& name1 = ((const CHANNEL *)item1)->name;
	const AString& name2 = ((const CHANNEL *)item2)->name;

	UNUSED(pContext);

	return CompareNoCase(name1, name2);
}

void ADVBProgList::AddXMLTVChannel(const AStructuredNode& channel)
{
	static std::vector<REPLACEMENT> replacements;
	const ADVBConfig& config = ADVBConfig::Get();
	AString id   = channel.GetAttribute("id");
	AString name = channel.GetChildValue("display-name");

	if (!replacements.size()) config.ReadReplacementsFile(replacements, config.GetXMLTVReplacementsFile());

	if (id.Valid() && name.Valid()) {
		const AStructuredNode *iconnode;
		AString icon;

		if ((iconnode = channel.FindChild("icon")) != NULL) icon = iconnode->GetAttribute("src");

		name = ReplaceStrings(name, &replacements[0], (uint_t)replacements.size());

		(void)config;
		//config.logit("Channel %s=%s icon=%s\n", id.str(), name.str(), icon.str());

		AddChannel(id, name);

		ADVBIconCache::Get().SetIcon("channel", name, icon);
	}
}

void ADVBProgList::AddChannel(const AString& id, const AString& name)
{
	CHANNEL *channel = NULL;

	if (id.Valid() && ((channel = GetChannelWritable(id)) == NULL)) {
		if (!channelhash.Valid()) channelhash.Create(40);

		if ((channel = new CHANNEL) != NULL) {
			channel->id   = id;
			channel->name = name;

			//debug("Added channel id '%s' name '%s'\n", channel->id.str(), channel->name.str());

			channelhash.Insert(id, (uptr_t)channel);
			channellist.Add((uptr_t)channel);

			channellist.Sort(&SortChannels);
		}
	}

	if (channel && name.Valid() && (name != channel->name)) {
		channel->name = name;
		channellist.Sort(&SortChannels);
	}
}

void ADVBProgList::DeleteChannel(const AString& id)
{
	CHANNEL *channel;

	if ((channel = GetChannelWritable(id)) != NULL) {
		channelhash.Remove(id);
		channellist.Remove((uptr_t)channel);
	}
}

AString ADVBProgList::LookupXMLTVChannel(const AString& id) const
{
	const CHANNEL *channel;

	if ((channel = GetChannelByID(id)) != NULL) return channel->name;

	return "";
}

void ADVBProgList::AssignEpisodes(bool reverse, bool ignorerepeats)
{
	ADVBEpisodeHandler handler;
	uint_t i;

	for (i = 0; i < Count(); i++) {
		ADVBProg& prog = GetProgWritable(reverse ? (Count() - 1 - i) : i);

		handler.AssignEpisode(prog, ignorerepeats);
	}
}

bool ADVBProgList::ValidChannelID(const AString& channelid) const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString regionalchannels = config.GetRegionalChannels();
	uint_t  i, n = regionalchannels.CountColumns();
	bool    valid = true;

	for (i = 0; i < n; i++) {
		AString channel = regionalchannels.Column(i).Words(0);
		AString suffix  = "." + channel.Line(0, "=");

		if (channelid.EndsWithNoCase(suffix)) {
			//debug("Channel ID '%s' is invalid (suffix='%s')\n", channelid.str(), suffix.str());
			valid = false;
			break;
		}
	}

	if (!valid) {
		for (i = 0; i < n; i++) {
			AString channel = regionalchannels.Column(i).Words(0);
			AString suffix  = "." + channel.Line(0, "=");
			AString region  = channel.Line(1, "=") + suffix;

			if (channelid.EndsWithNoCase(region)) {
				//debug("Channel ID '%s' is valid again (region='%s')\n", channelid.str(), region.str());
				valid = true;
				break;
			}
		}
	}

	return valid;
}

void ADVBProgList::GetProgrammeValues(AString& str, const AStructuredNode *pNode, const AString& prefix) const
{
	const AKeyValuePair *pAttr;

	while (pNode) {
		AString key   = prefix + pNode->Key.SearchAndReplace("-", "");
		AString value = pNode->Value;

		if ((key == "episodenum") && ((pAttr = pNode->FindAttribute("system")) != NULL)) key += ":" + pAttr->Value;
		if ((key == "icon")       && ((pAttr = pNode->FindAttribute("src"))    != NULL)) value = pAttr->Value;
		if (key == "rating")														     value = pNode->GetChildValue("value");

		if (value.Valid() || !pNode->GetChildren()) str.printf("%s=%s\n", key.str(), value.str());

		if (pNode->GetChildren()) {
			if (key == "video") GetProgrammeValues(str, pNode->GetChildren(), key);
			else				GetProgrammeValues(str, pNode->GetChildren());
		}

		pNode = pNode->Next();
	}
}

bool ADVBProgList::ReadFromXMLTVFile(const AString& filename)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AHash     channelidvalidhash(100);
	AString   data;
	FILE_INFO info;
	bool      success = false;

	if (data.ReadFromFile(filename)) {
		const AString channelstr   = "<channel ";
		const AString programmestr = "<programme ";
		AString   line, channel, programme;
		ADateTime now;
		uint_t len = data.len();
		int  p = 0, p1 = 0;

		success = true;

		do {
			if ((p1 = data.Pos("\n", p)) < 0) p1 = len;

			line.Create(data.str() + p, p1 - p, false);
			line = line.Words(0);

			if (channel.Valid()) {
				channel += line;
				if (channel.PosNoCase("</channel>") >= 0) {
					const AStructuredNode *pNode;
					AStructuredNode _node;

					if (DecodeXML(_node, channel) && ((pNode = _node.GetChildren()) != NULL)) {
						AddXMLTVChannel(*pNode);
					}
					else config.printf("Failed to decode channel data ('%s')", channel.str());

					channel.Delete();
				}
			}
			else if (programme.Valid()) {
				programme += line;
				if (programme.PosNoCase("</programme>") >= 0) {
					const AStructuredNode *pNode;
					AStructuredNode _node;

					if (DecodeXML(_node, programme) && ((pNode = _node.GetChildren()) != NULL)) {
						AString channelid = pNode->GetAttribute("channel");
						AString channel   = LookupXMLTVChannel(channelid);
						const CHANNEL *chandata;

						if (!channelidvalidhash.Exists(channelid)) {
							bool valid = ValidChannelID(channelid);

							channelidvalidhash.Insert(channelid, (uint_t)valid);

							if (!valid) debug("Channel ID '%s' (channel '%s') is%s valid\n", channelid.str(), channel.str(), valid ? "" : " NOT");
						}

						if ((chandata = GetChannelByID(channelid)) != NULL) {
							ADateTime start, stop;
							AString   str;

							start.StrToDate(pNode->GetAttribute("start"), ADateTime::Time_Absolute);
							stop.StrToDate(pNode->GetAttribute("stop"), ADateTime::Time_Absolute);

#if 0
							{
								static AStdFile fp;
								if (!fp.isopen()) fp.open("dates.txt", "w");
								if (fp.isopen()) {
									fp.printf("Start %s -> %s\n", programme.GetField(" start=\"", "\"").str(), start.DateFormat("%Y-%M-%D %h:%m:%s.%S").str());
									fp.printf("Stop  %s -> %s\n", programme.GetField(" stop=\"", "\"").str(), stop.DateFormat("%Y-%M-%D %h:%m:%s.%S").str());
								}
							}
#endif

							str.printf("\n");
							str.printf("start=%s\n", ADVBProg::GetHex(start).str());
							str.printf("stop=%s\n",  ADVBProg::GetHex(stop).str());

							str.printf("channel=%s\n", channel.str());
							str.printf("channelid=%s\n", channelid.str());

							GetProgrammeValues(str, pNode->GetChildren());

							//debug("%s", str.str());

							{
								ADVBProg prog = str;

								if (!prog.Valid()) config.logit("Ignoring invalid programme");
								else if (stop <= start) {
									config.logit("Ignoring zero length programme '%s'", prog.GetQuickDescription().str());
								}
								else if (stricmp(prog.GetTitle(), "to be announced") == 0) {
									// ignore programmes with the above title
								}
								else if (AddProg(prog, true, true) < 0) {
									config.printf("Failed to add prog!");
									success = false;
								}
							}
						}
					}
					else config.printf("Failed to decode programme data ('%s')", programme.str());

					programme.Delete();
				}
			}
			else if (CompareNoCaseN(line, channelstr, channelstr.len()) == 0) {
				channel = line;
			}
			else if (CompareNoCaseN(line, programmestr, programmestr.len()) == 0) {
				programme = line;
			}

			p = p1 + 1;

			if (HasQuit()) break;
		}
		while (p1 < (int)len);
	}

	return success;
}

bool ADVBProgList::ReadFromTextFile(const AString& filename)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString data;
	bool    notempty = (Count() != 0);
	bool    success = false;

	if (data.ReadFromFile(filename)) {
		AString str;
		int p = 0, p1;

		//config.printf("Read data from '%s', parsing...", filename.str());

		success = true;

		while ((p1 = data.Pos("\n\n", p)) >= 0) {
			str.Create(data.str() + p, p1 + 1 - p, false);

			if ((str.CountWords() > 0) && (AddProg(str, notempty, notempty) < 0)) {
				config.printf("Failed to add prog");
				success = false;
			}

			p = p1 + 1;

			if (HasQuit()) break;
		}
	}

	return success;
}

bool ADVBProgList::ReadFromBinaryFile(const AString& filename, bool sort, bool removeoverlaps)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AStdFile fp;
	bool success = false;

	if (fp.open(filename, "rb")) {
		ADVBProg prog;

		fp.seek(0, SEEK_END);
		sint32_t size = fp.tell(), pos = 0;
		fp.rewind();

		success = true;

		while (success && (pos < size)) {
			if ((prog = fp).Valid()) {
				if (AddProg(prog, sort, removeoverlaps) < 0) {
					config.printf("Failed to add prog!");
					success = false;
				}
			}

			pos = fp.tell();

			if (HasQuit()) break;
		}

		//config.printf("Read data from '%s', parsing complete", filename.str());
	}

	return success;
}

bool ADVBProgList::ReadFromJSONFile(const AString& filename)
{
	//const ADVBConfig& config = ADVBConfig::Get();
	AString data;
	//bool    notempty = (Count() != 0);
	bool    success = false;

	if (data.ReadFromFile(filename)) {
		Json::Reader reader;
		Json::Value  obj;

		if (reader.parse(data.str(), obj)) {
			if (obj.isMember("Channels") && obj["Channels"].isArray()) {
				const Json::Value& channels = obj["Channels"];
				uint_t i, nchannels = (uint_t)channels.size();

				for (i = 0; i < nchannels; i++) {
					const Json::Value& channel = channels[i];

					if (channel.isMember("DisplayName")) printf("Channel %u/%u: '%s'\n", i, (uint_t)channels.size(), channel["DisplayName"].asString().c_str());
					if (channel.isMember("TvListings") && channel["TvListings"].isArray()) {
						const Json::Value& tvlistings = channel["TvListings"];
						uint_t j, ntvlistings = (uint_t)tvlistings.size();

						for (j = 0; j < ntvlistings; j++) {
							const Json::Value& programme = tvlistings[j];

							(void)programme;
						}
					}
				}

				success = true;
			}
		}
	}

	return success;
}

bool ADVBProgList::ReadFromFile(const AString& filename)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AStdFile fp;
	bool notempty = (Count() != 0);
	bool success  = false;

	//config.printf("Reading data from '%s'", filename.str());

	if (filename.Suffix() == "xmltv") {
		success = ReadFromXMLTVFile(filename);
	}
	else if (filename.Suffix() == "txt") {
		success = ReadFromTextFile(filename);
	}
	else if (filename.Suffix() == "json") {
		success = ReadFromJSONFile(filename);
	}
	else if (filename.Suffix() == "dat") {
		success = ReadFromBinaryFile(filename, notempty, notempty);
	}
	else config.printf("Unrecognized suffix '%s' for file '%s'", filename.Suffix().str(), filename.str());

	return success;
}

bool ADVBProgList::ReadFromJobQueue(int queue, bool runningonly)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString listname   = config.GetTempFile("atlist", ".txt");
	AString scriptname = config.GetTempFile("atjob",  ".txt");
	AString cmd1, cmd2;
	bool success = false;

	remove(listname);

	cmd1.printf("atq -q = >%s", listname.str());
	if (!runningonly) cmd2.printf("atq -q %c >>%s", queue, listname.str());

	if ((system(cmd1) == 0) && (runningonly || (system(cmd2) == 0))) {
		AStdFile fp;

		if (fp.open(listname)) {
			AString line, cmd;

			success = true;
			while (line.ReadLn(fp) >= 0) {
				uint_t jobid = (uint_t)line.Word(0);

				if (jobid) {
					cmd.Delete();
					cmd.printf("at -c %u >%s", jobid, scriptname.str());
					if (system(cmd) == 0) {
						ADVBProg prog;

						if (prog.ReadFromJob(scriptname)) {
							if (queue == '=') prog.SetRunning();

							if (AddProg(prog) < 0) {
								config.logit("Failed to decode and add programme from job %u", jobid);
								success = false;
							}
						}
						else config.logit("Failed to read job %u", jobid);
					}
				}

				if (HasQuit()) break;
			}

			fp.close();
		}
	}
	else config.logit("Failed to read job list");

	remove(listname);
	remove(scriptname);

	return success;
}

bool ADVBProgList::ReadFromJobList(bool runningonly)
{
	const ADVBConfig& config = ADVBConfig::Get();
	return ReadFromJobQueue(config.GetQueue().FirstChar(), runningonly);
}

bool ADVBProgList::CheckFile(const AString& filename, const AString& targetfilename, const FILE_INFO& fileinfo)
{
	FILE_INFO fileinfo2;
	return ((filename == targetfilename) || (::GetFileInfo(targetfilename, &fileinfo2) && (fileinfo2.WriteTime > fileinfo.WriteTime)));
}

void ADVBProgList::UpdateDVBChannels()
{
	const ADVBConfig&      config = ADVBConfig::Get();
	const ADVBChannelList& clist  = ADVBChannelList::Get();
	AHash hash(20);
	uint_t  i;

	for (i = 0; i < Count(); i++) {
		ADVBProg& prog     = GetProgWritable(i);
		AString channel    = prog.GetChannel();
		AString dvbchannel = clist.LookupDVBChannel(channel);

		if		(dvbchannel.Valid()) prog.SetDVBChannel(dvbchannel);
		else if (!hash.Exists(channel)) {
			config.logit("Unable to find DVB channel for '%s'", channel.str());
			hash.Insert(channel, 0);
		}
	}
}

bool ADVBProgList::WriteToFile(const AString& filename, bool updatedependantfiles) const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AStdFile fp;
	bool success = false;

	config.printf("Writing '%s'", filename.str());

	if		(filename.Suffix() == "txt") success = WriteToTextFile(filename);
	else if (filename.Suffix() == "dat") {
		if (fp.open(filename, "wb")) {
			uint_t i;

			success = true;

			for (i = 0; i < Count(); i++) {
				const ADVBProg& prog = GetProg(i);

				if (!prog.WriteToFile(fp)) {
					config.printf("Failed to write programme %u/%u to file '%s'", i, Count(), filename.str());
					success = false;
					break;
				}

				if (HasQuit()) break;
			}

			fp.close();
		}
	}

	if (success && updatedependantfiles && config.EnableCombined() && (filename != config.GetCombinedFile())) {
		FILE_INFO fileinfo;

		if (!::GetFileInfo(config.GetCombinedFile(),			&fileinfo) ||
			CheckFile(filename, config.GetRecordedFile(),       fileinfo)  ||
			CheckFile(filename, config.GetScheduledFile(), 	  	fileinfo)  ||
			CheckFile(filename, config.GetRecordingFile(), 	  	fileinfo)  ||
			CheckFile(filename, config.GetRejectedFile(),  	  	fileinfo)  ||
			CheckFile(filename, config.GetRecordFailuresFile(), fileinfo)  ||
			CheckFile(filename, config.GetProcessingFile(),     fileinfo)) {
			CreateCombinedFile();
		}
		//else config.printf("No need to update combined file");
	}

	return success;
}

bool ADVBProgList::WriteToTextFile(const AString& filename) const
{
	AStdFile fp;
	bool success = false;

	if (fp.open(filename, "w")) {
		uint_t i;

		for (i = 0; i < Count(); i++) {
			fp.printf("%s", GetProg(i).ExportToText().str());

			if (HasQuit()) break;
		}

		fp.printf("\n");
		fp.close();

		success = true;
	}

	return success;
}

bool ADVBProgList::WriteToGNUPlotFile(const AString& filename) const
{
	AStdFile fp;
	bool success = false;

	if (fp.open(filename, "w")) {
		AHash channels(20);
		uint_t i, j, n = 0;

		fp.printf("#time channel-index dvb-card length(hours) recorded scheduled rejected desc\n");

		for (i = 0; i < Count(); i++) {
			AString channel = GetProg(i).GetChannel();

			if (!channels.Exists(channel)) {
				for (j = i; j < Count(); j++) {
					const ADVBProg& prog = GetProg(j);
					AString channel2 = prog.GetChannel();

					if (channel2 == channel) {
						const uint64_t times[] = {
							prog.GetRecordStart() ? prog.GetRecordStart() : prog.GetStart(),
							prog.GetStart(),
							prog.GetStop(),
							prog.GetRecordStop() ? prog.GetRecordStop() : prog.GetStop(),
						};
						const double levels[] = {
							0.0,
							(double)prog.GetLength() / 3600000.0,
							(double)prog.GetLength() / 3600000.0,
							0.0,
						};
						AString desc = prog.GetQuickDescription();
						uint_t  card = prog.GetDVBCard();
						uint_t  k;

						for (k = 0; k < NUMBEROF(times); k++) {
							fp.printf("%s %u %u %s %u %u %u %s\n",
									  ADateTime(times[k]).DateFormat("%Y-%M-%D %h:%m:%s").str(),
									  n, card,
									  AValue(levels[k]).ToString().str(),
									  (uint_t)prog.IsRecorded(),
									  (uint_t)prog.IsScheduled(),
									  (uint_t)prog.IsRejected(),
									  desc.str());
						}

						fp.printf("\n\n");
					}
				}

				channels.Insert(channel, n++);
			}
		}

		fp.close();

		success = true;
	}

	return success;
}

void ADVBProgList::SearchAndReplace(const AString& search, const AString& replace)
{
	uint_t i, n = Count();

	for (i = 0; i < n; i++) {
		GetProgWritable(i).SearchAndReplace(search, replace);
	}
}

int ADVBProgList::AddProg(const AString& prog, bool sort, bool removeoverlaps)
{
	ADVBProg *pprog;
	int index = -1;

	if ((pprog = new ADVBProg(prog)) != NULL) {
		if (pprog->Valid()) {
			index = AddProg(pprog, sort, removeoverlaps);
		}
		else delete pprog;
	}

	return index;
}

int ADVBProgList::AddProg(const ADVBProg& prog, bool sort, bool removeoverlaps)
{
	ADVBProg *pprog;
	int index = -1;

	if ((pprog = new ADVBProg(prog)) != NULL) {
		if (pprog->Valid()) {
			index = AddProg(pprog, sort, removeoverlaps);
		}
		else delete pprog;
	}

	return index;
}

uint_t ADVBProgList::ModifyProg(const ADVBProg& prog, uint_t mode, bool sort)
{
	ADVBProg *pprog;
	uint_t res = 0;

	if ((pprog = FindUUIDWritable(prog)) != NULL) {
		if (mode & Prog_Modify) {
			pprog->Modify(prog);
			res = Prog_Modify;
		}
	}
	else if ((mode & Prog_Add) && (AddProg(prog, sort, false) >= 0)) res = Prog_Add;

	return res;
}

uint_t ADVBProgList::FindIndex(uint_t timeindex, uint64_t t) const
{
	const ADVBProg **progs = (const ADVBProg **)proglist.List();
	const uint_t n = Count();
	uint_t index   = 0;
	bool   forward = true;

	if (n >= 2) {
		uint_t log2 = (uint_t)ceil(log((double)n) / log(2.0)) - 1;
		uint_t inc  = 1 << log2;

		forward = (GetProg(0).GetTimeIndex(timeindex) <= GetProg(n - 1).GetTimeIndex(timeindex));

		if (forward) {
			while (inc) {
				uint_t index2 = index + inc;
				if ((index2 < n) && (t >= progs[index2]->GetTimeIndex(timeindex))) index = index2;
				inc >>= 1;
			}
		}
		else {
			while (inc) {
				uint_t index2 = index + inc;
				if ((index2 < n) && (t <= progs[index2]->GetTimeIndex(timeindex))) index = index2;
				inc >>= 1;
			}
		}
	}

	if (forward) {
		while ((index < n) && (t >= progs[index]->GetTimeIndex(timeindex))) index++;
	}
	else {
		while ((index < n) && (t <= progs[index]->GetTimeIndex(timeindex))) index++;
	}

	return index;
}

uint_t ADVBProgList::FindIndex(const ADVBProg& prog) const
{
	const ADVBProg **progs = (const ADVBProg **)proglist.List();
	const uint_t n = Count();
	uint_t index   = 0;
	bool   forward = true;

	if (n >= 2) {
		uint_t log2 = (uint_t)ceil(log((double)n) / log(2.0)) - 1;
		uint_t inc  = 1 << log2;

		forward = (GetProg(0) <= GetProg(n - 1));

		if (forward) {
			while (inc) {
				uint_t index2 = index + inc;
				if ((index2 < n) && (prog >= *progs[index2])) index = index2;
				inc >>= 1;
			}
		}
		else {
			while (inc) {
				uint_t index2 = index + inc;
				if ((index2 < n) && (prog <= *progs[index2])) index = index2;
				inc >>= 1;
			}
		}
	}

	if (forward) {
		while ((index < n) && (prog >= *progs[index])) index++;
	}
	else {
		while ((index < n) && (prog <= *progs[index])) index++;
	}

	return index;
}

int ADVBProgList::AddProg(const ADVBProg *prog, bool sort, bool removeoverlaps)
{
	const ADVBConfig& config = ADVBConfig::Get();
	uint_t i;
	int  index = -1;

	AddChannel(prog->GetChannelID(), prog->GetChannel());

	(void)i;
	(void)config;

	if (removeoverlaps && Count()) {
		uint_t i1 = FindIndex(ADVBProg::TimeIndex_Stop,  prog->GetTimeIndex(ADVBProg::TimeIndex_Start) - 1);
		uint_t i2 = FindIndex(ADVBProg::TimeIndex_Start, prog->GetTimeIndex(ADVBProg::TimeIndex_Stop)  + 1);

		for (i = i1; (i <= i2) && (i < Count()) && !HasQuit();) {
			const ADVBProg& prog1 = GetProg(i);

			if (prog->OverlapsOnSameChannel(prog1)) {
                //config.printf("'%s' overlaps with '%s', deleting '%s'", prog1.GetQuickDescription().str(), prog->GetQuickDescription().str(), prog1.GetQuickDescription().str());
				DeleteProg(i);
				i2--;
			}
			else i++;
		}
	}

	if (sort) {
		index = FindIndex(*prog);

#if 0
		{
			bool error = false;

			config.printf("Inserting %s between %s and %s (index %d/%u)",
						  prog->GetStartDT().DateToStr().str(),
						  (index > 0)            ? GetProg(index - 1).GetStartDT().DateToStr().str() : "<start>",
						  (index < (int)Count()) ? GetProg(index).GetStartDT().DateToStr().str() : "<end>",
						  index, Count());

			if ((index > 0) && (prog->GetStartDT() < GetProg(index - 1).GetStartDT())) {
				config.printf("Error: %s < %s", prog->GetStartDT().DateToStr().str(), GetProg(index - 1).GetStartDT().DateToStr().str());
				error = true;
			}
			if ((index < (int)Count()) && (prog->GetStartDT() > GetProg(index).GetStartDT())) {
				config.printf("Error: %s > %s", prog->GetStartDT().DateToStr().str(), GetProg(index).GetStartDT().DateToStr().str());
				error = true;
			}
			if (error) {
				config.printf("Errors found, order:");
				for (i = 0; i < Count(); i++) {
					config.printf("%u/%u: %s", i, Count(), GetProg(i).GetStartDT().DateToStr().str());
				}
				exit(0);
			}
		}
#endif

		index = proglist.Add((uptr_t)prog, index);
	}
	else index = proglist.Add((uptr_t)prog);

	if ((index >= 0) && proghash.Valid()) proghash.Insert(prog->GetUUID(), (uptr_t)prog);

	return index;
}

const ADVBProg& ADVBProgList::GetProg(uint_t n) const
{
	static const ADVBProg dummy;
	return proglist[n] ? *(const ADVBProg *)proglist[n] : dummy;
}

ADVBProg& ADVBProgList::GetProgWritable(uint_t n) const
{
	static ADVBProg dummy;
	return proglist[n] ? *(ADVBProg *)proglist[n] : dummy;
}

bool ADVBProgList::DeleteProg(uint_t n)
{
	ADVBProg *prog = (ADVBProg *)proglist[n];
	bool success = false;

	if (prog) {
		proglist.RemoveIndex(n);
		if (proghash.Valid()) proghash.Remove(prog->GetUUID());
		delete prog;
		success = true;
	}

	return success;
}

bool ADVBProgList::DeleteProg(const ADVBProg& prog)
{
	uint_t i;

	for (i = 0; i < Count(); i++) {
		const ADVBProg& prog1 = GetProg(i);

		if (prog == prog1) {
			return DeleteProg(i);
		}
	}

	return false;
}

void ADVBProgList::DeleteProgrammesBefore(const ADateTime& dt)
{
	uint64_t _dt = (uint64_t)dt;
	uint_t i;

	for (i = 0; i < Count();) {
		const ADVBProg& prog = GetProg(i);

		if (prog.GetStop() < _dt) DeleteProg(i);
		else i++;
	}
}

void ADVBProgList::CreateHash()
{
	uint_t i, n = Count();

	proghash.Delete();
	proghash.Create(200);

	for (i = 0; i < n; i++) {
		const ADVBProg& prog = GetProg(i);

		proghash.Insert(prog.GetUUID(), (uptr_t)&prog);
	}
}

const ADVBProg *ADVBProgList::FindUUID(const AString& uuid) const
{
	const ADVBProg *prog = NULL;
	int p;

	if		(proghash.Valid())				 prog = (const ADVBProg *)proghash.Read(uuid);
	else if ((p = FindUUIDIndex(uuid)) >= 0) prog = &GetProg(p);

	return prog;
}

int ADVBProgList::FindUUIDIndex(const AString& uuid) const
{
	int res = -1;

	if (proghash.Valid()) {
		res = proglist.Find(proghash.Read(uuid));
	}
	else {
		uint_t i, n = Count();

		for (i = 0; i < n; i++) {
			if (GetProg(i) == uuid) {
				res = (int)i;
				break;
			}
		}
	}


	return res;
}

ADVBProg *ADVBProgList::FindUUIDWritable(const AString& uuid) const
{
	ADVBProg *prog = NULL;
	int p;

	if		(proghash.Valid())				 prog = (ADVBProg *)proghash.Read(uuid);
	else if ((p = FindUUIDIndex(uuid)) >= 0) prog = &GetProgWritable(p);

	return prog;
}

void ADVBProgList::FindProgrammes(ADVBProgList& dest, const ADataList& patternlist, uint_t maxmatches) const
{
	const ADVBConfig& config = ADVBConfig::Get();
	uint_t i, j, n = patternlist.Count(), nfound = 0;

	//config.logit("Searching using %u patterns (max matches %u)...", n, maxmatches);
	//for (i = 0; i < n; i++) {
	//debug("Pattern %u/%u:\n%s", i, n, ADVBPatterns::ToString(*(const PATTERN *)patternlist[i]).str());
	//}

	for (i = 0; (i < Count()) && !HasQuit() && (nfound < maxmatches); i++) {
		const ADVBProg& prog = GetProg(i);
		ADVBProg *prog1 = NULL;
		bool     addtolist = false;
		bool     excluded  = false;

		for (j = 0; (j < n) && !HasQuit(); j++) {
			const PATTERN& pattern = *(const PATTERN *)patternlist[j];

			if (pattern.enabled) {
				bool match = prog1 ? prog1->Match(pattern) : prog.Match(pattern);

				if (match) {
					if (pattern.exclude) {
						excluded = true;
						break;
					}

					if (!prog1) prog1 = new ADVBProg(prog);

					if (prog1) {
						if (!prog1->GetPattern()[0] || prog1->IsPartialPattern()) {
							prog1->SetPattern(pattern.pattern);

							if (pattern.user.Valid()) {
								prog1->SetUser(pattern.user);
							}

							prog1->ClearPartialPattern();
							prog1->AssignValues(pattern);
						}
						else {
							prog1->ClearPartialPattern();
							prog1->UpdateValues(pattern);
						}
					
						if (!pattern.scorebased && !prog1->IsPartialPattern()) {
							addtolist = true;
							break;
						}
					}
				}
			}
		}

		if (prog1) {
			addtolist |= (prog1->GetScore() >= config.GetScoreThreshold());

			if (addtolist && !excluded) {
				dest.AddProg(*prog1, false);
				nfound++;
			}

			delete prog1;
		}
	}

	//config.logit("Search done, %u programmes found", nfound);
}

void ADVBProgList::FindProgrammes(ADVBProgList& dest, const AString& patterns, AString& errors, const AString& sep, uint_t maxmatches) const
{
	ADataList patternlist;

	ADVBPatterns::ParsePatterns(patternlist, patterns, errors, sep);

	FindProgrammes(dest, patternlist, maxmatches);
}

uint_t ADVBProgList::FindSimilarProgrammes(ADVBProgList& dest, const ADVBProg& prog, uint_t index) const
{
	uint_t i, n = 0;

	for (i = index; i < Count(); i++) {
		const ADVBProg& prog1 = GetProg(i);

		if ((&prog != &prog1) && ADVBProg::SameProgramme(prog, prog1)) {
			dest.AddProg(prog1);
			n++;
		}
	}

	return n;
}

const ADVBProg *ADVBProgList::FindSimilar(const ADVBProg& prog, const ADVBProg *startprog) const
{
	const ADVBProg *res = NULL;
	uint_t i = 0;

	if (startprog) {
		const ADVBProg *startprog2;
		int p;

		if (((p = proglist.Find((uptr_t)startprog)) >= 0) ||
			(((startprog2 = FindUUID(startprog)) != NULL) && ((p = proglist.Find((uptr_t)startprog2)) >= 0))) {
			i = p + 1;
		}
	}

	for (; i < Count(); i++) {
		const ADVBProg& prog1 = GetProg(i);

		if (ADVBProg::SameProgramme(prog, prog1)) {
			res = &prog1;
			break;
		}
	}

	return res;
}

ADVBProg *ADVBProgList::FindSimilarWritable(const ADVBProg& prog, ADVBProg *startprog)
{
	const ADVBProg *res;

	if ((res = FindSimilar(prog, startprog)) != NULL) return FindUUIDWritable(*res);

	return NULL;
}

const ADVBProg *ADVBProgList::FindCompleteRecording(const ADVBProg& prog) const
{
	const ADVBProg *res = NULL;

	while (((res = FindSimilar(prog, res)) != NULL) && (res->IgnoreRecording() || !res->IsRecordingComplete())) ;

	return res;
}

ADVBProg *ADVBProgList::FindOverlap(const ADVBProg& prog1, const ADVBProg *prog2) const
{
	uint_t i = 0;
	int p;

	if (prog2 && ((p = proglist.Find((uptr_t)prog2)) >= 0)) i = p + 1;

	while ((prog2 = (const ADVBProg *)proglist[i++]) != NULL) {
		if (CompareNoCase(prog2->GetUUID(), prog1.GetUUID()) != 0) {
			if (prog2->Overlaps(prog1)) break;
		}
	}

	return (ADVBProg *)prog2;
}

ADVBProg *ADVBProgList::FindRecordOverlap(const ADVBProg& prog1, const ADVBProg *prog2) const
{
	const ADVBConfig& config = ADVBConfig::Get();
	const uint64_t mininterrectime = (uint64_t)config.GetConfigItem("mininterrectime", config.GetDefaultInterRecTime()) * 1000ULL;
	const uint64_t st1 = prog1.GetRecordStart();
	const uint64_t et1 = prog1.GetRecordStop() + mininterrectime;
	uint_t i = 0;
	int p;

	if (prog2 && ((p = proglist.Find((uptr_t)prog2)) >= 0)) i = p + 1;

	while ((prog2 = (const ADVBProg *)proglist[i++]) != NULL) {
		if (prog2 != &prog1) {
			const uint64_t st2 = prog2->GetRecordStart();
			const uint64_t et2 = prog2->GetRecordStop() + mininterrectime;

			if ((st2 < et1) && (et2 > st1)) break;
		}
	}

	return (ADVBProg *)prog2;
}

void ADVBProgList::AdjustRecordTimes()
{
	const ADVBConfig& config = ADVBConfig::Get();
	const uint64_t mininterrectime = (uint64_t)config.GetConfigItem("mininterrectime", config.GetDefaultInterRecTime()) * 1000ULL;
	uint_t i, j;

	config.logit("Adjusting record times of any overlapping programmes");

	for (i = 0; i < Count(); i++) {
		ADVBProg *prog1 = (ADVBProg *)proglist[i];

		for (j = i + 1; j < Count(); j++) {
			ADVBProg *prog2 = (ADVBProg *)proglist[j];
			ADVBProg *firstprog  = NULL;
			ADVBProg *secondprog = NULL;

			// order programmes in chronilogical order
			if (prog1->GetStart() < prog2->GetStart()) {
				firstprog  = prog1;
				secondprog = prog2;
			}
			else if (prog2->GetStart() < prog1->GetStart()) {
				firstprog  = prog2;
				secondprog = prog1;
			}

            // if firstprog is before secondprog and the recording of firstprog overlaps the recording of secondprog, adjust times
			if (firstprog && secondprog &&
				((firstprog->GetRecordStop() + mininterrectime) >= secondprog->GetRecordStart())) {
				config.logit("'%s' recording end overlaps '%s' recording start - shift times",
							 firstprog->GetQuickDescription().str(),
							 secondprog->GetQuickDescription().str());

				config.logit("Recording times originally %s - %s and %s - %s",
							 firstprog->GetRecordStartDT().UTCToLocal().DateFormat("%h:%m:%s").str(),
							 firstprog->GetRecordStopDT().UTCToLocal().DateFormat("%h:%m:%s").str(),
							 secondprog->GetRecordStartDT().UTCToLocal().DateFormat("%h:%m:%s").str(),
							 secondprog->GetRecordStopDT().UTCToLocal().DateFormat("%h:%m:%s").str());

				uint64_t diff = firstprog->GetRecordStop()   + mininterrectime - secondprog->GetRecordStart();
				uint64_t newt = secondprog->GetRecordStart() + diff / 2;

				newt -= newt % 60000;

				firstprog->SetRecordStop(newt - mininterrectime);
				secondprog->SetRecordStart(newt);

				config.logit("Recording times now        %s - %s and %s - %s",
							 firstprog->GetRecordStartDT().UTCToLocal().DateFormat("%h:%m:%s").str(),
							 firstprog->GetRecordStopDT().UTCToLocal().DateFormat("%h:%m:%s").str(),
							 secondprog->GetRecordStartDT().UTCToLocal().DateFormat("%h:%m:%s").str(),
							 secondprog->GetRecordStopDT().UTCToLocal().DateFormat("%h:%m:%s").str());
			}
		}
	}
}

const ADVBProg *ADVBProgList::FindFirstRecordOverlap() const
{
	const ADVBProg *prog = NULL;
	uint_t i;

	for (i = 0; (i < Count()) && ((prog = FindRecordOverlap(GetProg(i))) == NULL); i++) ;

	return prog;
}

void ADVBProgList::UnscheduleAllProgrammes()
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString cmd;
	AString filename, scriptname;
	int queue = config.GetQueue().FirstChar();

	filename   = config.GetTempFile("joblist",   ".sh");
	scriptname = config.GetTempFile("jobscript", ".sh");

	cmd.printf("atq -q %c >%s", queue, filename.str());
	if (system(cmd) == 0) {
		AStdFile fp;

		if (fp.open(filename)) {
			AString line;

			while (line.ReadLn(fp) >= 0) {
				uint_t job = (uint_t)line.Word(0);

				if (job) {
					bool done = false;

					cmd.Delete();
					cmd.printf("at -c %u >%s", job, scriptname.str());

					if (system(cmd) == 0) {
						ADVBProg prog;

						if (prog.ReadFromJob(scriptname)) {
							cmd.Delete();
							cmd.printf("atrm %u", job);

							if (system(cmd) == 0) {
								config.logit("Deleted job %u ('%s')", job, prog.GetQuickDescription().str());
								done = true;
							}
							else config.logit("Failed to delete job %u ('%s')", job, prog.GetQuickDescription().str());
						}
						else config.logit("Failed to decode job %u", job);
					}

					remove(scriptname);

					if (!done) {
						cmd.Delete();
						cmd.printf("atrm %u", job);
						if (system(cmd) == 0) {
							config.logit("Deleted job %u (unknown)", job);
						}
						else config.logit("Failed to delete job %u (unknown)", job);
					}
				}
			}

			fp.close();
		}
	}

	remove(filename);
}

int ADVBProgList::SortPatterns(uptr_t item1, uptr_t item2, void *context)
{
	const PATTERN& pat1 = *(const PATTERN *)item1;
	const PATTERN& pat2 = *(const PATTERN *)item2;
	uint_t pat1dis = (pat1.pattern[0] == '#');
	uint_t pat2dis = (pat2.pattern[0] == '#');
	int res;

	UNUSED(context);

	if (pat1.pri < pat2.pri) return  1;
	if (pat1.pri > pat2.pri) return -1;

	if (!pat1.enabled &&  pat2.enabled) return 1;
	if ( pat1.enabled && !pat2.enabled) return -1;

	if ((res = stricmp(pat1.user, pat2.user)) != 0) return res;
	if ((res = stricmp(pat1.pattern.str() + pat1dis,
					   pat2.pattern.str() + pat2dis)) != 0) return res;

	return 0;
}

void ADVBProgList::ReadPatterns(ADataList& patternlist, AString& errors, bool sort)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString  patternfilename	= config.GetPatternsFile();
	AString  filepattern 	    = config.GetUserPatternsPattern();
	AString  filepattern_parsed = ParseRegex(filepattern);
	AList    userpatterns;
	AStdFile fp;

	if (fp.open(patternfilename)) {
		AString line;

		while (line.ReadLn(fp) >= 0) {
			if (line.Words(0).Valid()) {
				ADVBPatterns::ParsePattern(patternlist, line, errors);
			}
		}

		fp.close();
	}
	else config.logit("Failed to read patterns file '%s'", patternfilename.str());

	::CollectFiles(filepattern.PathPart(), filepattern.FilePart(), 0, userpatterns);

	const AString *file = AString::Cast(userpatterns.First());
	while (file) {
		AString   user;
		ADataList regions;

		if (MatchRegex(*file, filepattern_parsed, regions)) {
			const REGEXREGION *region = (const REGEXREGION *)regions[0];

			if (region) user = file->Mid(region->pos, region->len);
			else		config.logit("Failed to find user in '%s' (no region)", file->str());
		}
		else config.logit("Failed to find user in '%s'", file->str());

		if (fp.open(*file)) {
			AString line;

			while (line.ReadLn(fp) >= 0) {
				if (line.Words(0).Valid()) {
					ADVBPatterns::ParsePattern(patternlist, line, errors, user);
				}
			}

			fp.close();
		}
		else config.logit("Failed to read file '%s'", file->str());

		file = file->Next();
	}

	if (sort) patternlist.Sort(&SortPatterns);
}

int ADVBProgList::SortProgsByUserThenDir(uptr_t item1, uptr_t item2, void *pContext)
{
	const ADVBProg& prog1 = *(const ADVBProg *)item1;
	const ADVBProg& prog2 = *(const ADVBProg *)item2;
	int res;

	UNUSED(pContext);

	if ((res = strcmp(prog1.GetUser(), prog2.GetUser())) != 0) return res;

	return strcmp(prog1.GetFilename(), prog2.GetFilename());
}

bool ADVBProgList::CheckDiskSpace(bool runcmd, bool report)
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBProgList proglist;

	proglist.ReadFromFile(config.GetScheduledFile());

	return proglist.CheckDiskSpaceList(runcmd, report);
}

bool ADVBProgList::CheckDiskSpaceList(bool runcmd, bool report) const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AStdFile fp;
	AHash    hash(30);
	AString  filename;
	double   lowlimit = config.GetLowSpaceWarningLimit();
	uint_t   i, userlen = 0;
	bool     okay  = true;

	if (report) printf(",\"diskspace\":[");

	if (runcmd) {
		filename = config.GetTempFile("patterndiskspace", ".txt");
		if (fp.open(filename, "w")) {
			fp.printf("Disk space for patterns as of %s:\n\n", ADateTime().DateToStr().str());
		}
	}

	AString fmt;
	fmt.printf("%%-%us %%6.1lfG %%s", userlen);

	AList reportlist;
	const AString recdir = config.GetRecordingsDir();
	for (i = 0; i < Count(); i++) {
		const ADVBProg& prog = GetProg(i);
		const AString user = prog.GetUser();
		const AString rdir = (prog.IsConverted() || config.IsRecordingSlave()) ? AString(prog.GetFilename()).PathPart() : prog.GetConvertedDestinationDirectory();
		AString dir;

		if (rdir.StartsWith(recdir)) dir = rdir.Mid(recdir.len() + 1);
		else						 dir = rdir;

		//printf("\nUser '%s' dir '%s' rdir '%s'\n", user.str(), dir.str(), rdir.str());

		CreateDirectory(rdir);

		if (!hash.Exists(rdir)) {
			struct statvfs fiData;

			hash.Insert(rdir, 0);

			if (statvfs(rdir, &fiData) >= 0) {
				AString str;
				double  gb = (double)fiData.f_bavail * (double)fiData.f_bsize / (1024.0 * 1024.0 * 1024.0);

				if (report) {
					AString *reportstr = new AString;

					if (reportstr) {
						reportstr->printf("{\"user\":\"%s\"", JSONFormat(user).str());
						reportstr->printf(",\"folder\":\"%s\"", JSONFormat(dir).str());
						reportstr->printf(",\"fullfolder\":\"%s\"", JSONFormat(rdir).str());
						reportstr->printf(",\"freespace\":\"%0.1lfG\"", gb);
						reportstr->printf(",\"level\":%u", (uint_t)(gb / lowlimit));
						reportstr->printf("}");

						reportlist.Add(reportstr, &AString::AlphaCompareNoCase);
					}
				}

				str.printf(fmt, prog.GetUser(), gb, dir.str());
				if (gb < lowlimit) {
					str.printf(" Warning!");
					okay = false;
				}

				if (runcmd) {
					fp.printf("%s\n", str.str());
				}

				//config.printf("%s", str.str());
			}
		}
	}

	{
		const AString *str = AString::Cast(reportlist.First());
		bool  first = true;

		while (str) {
			if (first) first = false;
			else	   printf(",");

			printf("%s", str->str());

			str = str->Next();
		}
	}

	if (runcmd) {
		fp.close();

		if (!okay) {
			AString cmd = config.GetConfigItem("lowdiskspacecmd");
			if (cmd.Valid()) {
				cmd = cmd.SearchAndReplace("{logfile}", filename);

				RunAndLogCommand(cmd);
			}
		}
		else {
			AString cmd = config.GetConfigItem("diskspacecmd");
			if (cmd.Valid()) {
				cmd = cmd.SearchAndReplace("{logfile}", filename);

				RunAndLogCommand(cmd);
			}
		}

		remove(filename);
	}

	if (report) printf("]");

	return okay;
}

uint_t ADVBProgList::SchedulePatterns(const ADateTime& starttime, bool commit)
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBLock	 lock("dvbfiles");
	ADVBProgList proglist;
	AString      filename = config.GetListingsFile();
	uint_t       res = 0;

	commit &= config.CommitScheduling();

	if (!commit) config.logit("** Not committing scheduling **");

	if (config.GetRecordingHost().Valid()) {
		ADVBProgList list;
		list.ModifyFromRecordingHost(config.GetRecordedFile(), ADVBProgList::Prog_Add);
		GetRecordingListFromRecordingSlave();
		GetFileFromRecordingHost(config.GetDVBChannelsFile());
	}

	config.logit("Loading listings from '%s'", filename.str());

	if (proglist.ReadFromFile(filename)) {
		ADVBProgList reslist;

		config.printf("Loaded %u programmes from '%s'", proglist.Count(), filename.str());

		config.logit("Removing programmes that have already finished");

		proglist.DeleteProgrammesBefore(starttime);

		config.logit("List now contains %u programmes", proglist.Count());

		ADataList patternlist;
		AString   errors;
		ReadPatterns(patternlist, errors, false);

		if (errors.Valid()) {
			uint_t i;

			config.printf("Errors found during parsing:");

			for (i = 0; i < patternlist.Count(); i++) {
				const PATTERN& pattern = *(const PATTERN *)patternlist[i];

				if (pattern.errors.Valid()) {
					config.printf("Parsing '%s': %s", pattern.pattern.str(), pattern.errors.str());
				}
			}
		}

		config.printf("Checking disk space");

		config.printf("Finding programmes using %u patterns", patternlist.Count());

		proglist.FindProgrammes(reslist, patternlist);

		config.logit("Found %u programmes from %u patterns", reslist.Count(), patternlist.Count());

		if (AStdFile::exists(config.GetExtraRecordFile())) {
			AString data;

			if (data.ReadFromFile(config.GetExtraRecordFile())) {
				AString str;
				uint_t n = 0;
				int p = 0, p1;

				while ((p1 = data.Pos("\n\n", p)) >= 0) {
					ADVBProg prog;

					str.Create(data.str() + p, p1 + 1 - p, false);

					if (str.CountWords() > 0) {
						prog = str;
						if (prog.Valid()) {
							if (prog.GetStopDT() > starttime) {
								if (reslist.AddProg(prog, true) >= 0) n++;
								else config.printf("Failed to add extra prog");
							}
						}
						else config.printf("Extra prog is invalid!");
					}

					p = p1 + 1;
				}

				if (n) config.printf("Added %u extra programmes to record", n);
			}
		}

		res = reslist.Schedule(starttime);

		if (commit) WriteToJobList();

		CreateCombinedFile();

		ADVBProgList::CheckDiskSpace(true);
	}
	else config.logit("Failed to read listings file '%s'", filename.str());

	return res;
}

void ADVBProgList::Sort(bool reverse)
{
	proglist.Sort(&SortProgs, &reverse);
}

void ADVBProgList::Sort(int (*fn)(uptr_t item1, uptr_t item2, void *pContext), void *pContext)
{
	proglist.Sort(fn, pContext);
}

int ADVBProgList::CompareEpisode(uptr_t item1, uptr_t item2, void *pContext)
{
	UNUSED(pContext);
	return ADVBProg::CompareEpisode(((const ADVBProg *)item1)->GetEpisode(), ((const ADVBProg *)item2)->GetEpisode());
}

void ADVBProgList::AddToList(const AString& filename, const ADVBProg& prog, bool sort, bool removeoverlaps)
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBLock     lock("dvbfiles");
	ADVBProgList list;

	if (!list.ReadFromFile(filename)) config.logit("Failed to read file '%s' for adding a programme", filename.str());
	list.AddProg(prog, sort, removeoverlaps);
	if (!list.WriteToFile(filename)) config.logit("Failed to write file '%s' after adding a programme", filename.str());
}

bool ADVBProgList::RemoveFromList(const AString& filename, const ADVBProg& prog)
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBLock     lock("dvbfiles");
	ADVBProgList list;
	bool		 removed = false;

	if (list.ReadFromFile(filename)) {
		if		(!list.DeleteProg(prog))	  config.logit("Failed to find programme '%s' in list to remove it!", prog.GetQuickDescription().str());
		else if (!list.WriteToFile(filename)) config.logit("Failed to write file '%s' after removing a programme", filename.str());
		else removed = true;
	}
	else config.logit("Failed to read file '%s' for removing a programme", filename.str());

	return removed;
}

bool ADVBProgList::RemoveFromList(const AString& filename, const AString& uuid)
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBLock     lock("dvbfiles");
	ADVBProgList list;
	bool		 removed = false;

	if (list.ReadFromFile(filename)) {
		const ADVBProg *pprog;

		if ((pprog = list.FindUUID(uuid)) != NULL) {
			ADVBProg prog = *pprog;
			if		(!list.DeleteProg(prog))	  config.logit("Failed to find programme '%s' in list to remove it!", prog.GetQuickDescription().str());
			else if (!list.WriteToFile(filename)) config.logit("Failed to write file '%s' after removing a programme", filename.str());
			else removed = true;
		}
	}
	else config.logit("Failed to read file '%s' for removing a programme", filename.str());

	return removed;
}

bool ADVBProgList::RemoveFromRecordLists(const ADVBProg& prog)
{
	return RemoveFromRecordLists(prog.GetUUID());
}

bool ADVBProgList::RemoveFromRecordLists(const AString& uuid)
{
	const ADVBConfig& config = ADVBConfig::Get();
	bool removed = false;

	{
		ADVBLock lock("dvbfiles");
		removed |= ADVBProgList::RemoveFromList(config.GetRecordFailuresFile(), uuid);
		removed |= ADVBProgList::RemoveFromList(config.GetRecordedFile(), uuid);
	}

	if (config.GetRecordingHost().Valid()) {
		AString cmd;

		cmd.printf("dvb --delete-from-record-lists \"%s\"", uuid.Escapify().str());
		RunRemoteCommand(cmd);
	}

	return removed;
}

uint_t ADVBProgList::Schedule(const ADateTime& starttime)
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBLock     lock("dvbfiles");
	ADVBProgList oldrejectedlist;
	ADVBProgList recordedlist;
	ADVBProgList scheduledlist;
	ADVBProgList rejectedlist;
	ADVBProgList runninglist;
	ADVBProgList newrejectedlist;
	uint_t i, n, reccount;

	oldrejectedlist.ReadFromFile(config.GetRejectedFile());

	for (i = 0; i < Count(); ) {
		ADVBProg& prog = GetProgWritable(i);
		AString dvbchannel;

		// check and update DVB channel
		if ((dvbchannel = ADVBChannelList::Get().LookupDVBChannel(prog.GetChannel())) != prog.GetDVBChannel()) {
			config.logit("Changing DVB channel of '%s' from '%s' to '%s'", prog.GetDescription().str(), prog.GetDVBChannel(), dvbchannel.str());
			prog.SetDVBChannel(dvbchannel);
		}

		if (!prog.GetDVBChannel()[0]) {
			config.logit("'%s' does not have a DVB channel", prog.GetDescription().str());

			DeleteProg(i);
		}
		else i++;
	}

	WriteToFile(config.GetRequestedFile(), false);

	recordedlist.ReadFromFile(config.GetRecordedFile());
	reccount = recordedlist.Count();

	// read running job(s)
	runninglist.ReadFromFile(config.GetRecordingFile());
	runninglist.CreateHash();

	config.logit("--------------------------------------------------------------------------------");
	config.printf("Found: %u programmes:", Count());
	config.logit("--------------------------------------------------------------------------------");
	for (i = 0; i < Count(); i++) {
		const ADVBProg& prog = GetProg(i);
		const ADVBProg *rprog;

		config.logit("%s%s",
					 prog.GetDescription(1).str(),
					 ((rprog = runninglist.FindUUID(prog)) != NULL) ? AString(" (recording on DVB card %s)").Arg(rprog->GetDVBCard()).str() : "");
	}
	config.logit("--------------------------------------------------------------------------------");

	// first, remove programmes that do not have a valid DVB channel
	// then, remove any that have already finished
	// then, remove any that have already been recorded
	for (i = 0; i < Count();) {
		const ADVBProg *otherprog;
		ADVBProg& prog = GetProgWritable(i);

		if (!prog.AllowRepeats() && ((otherprog = recordedlist.FindCompleteRecording(prog)) != NULL)) {
			config.logit("'%s' has already been recorded ('%s')", prog.GetQuickDescription().str(), otherprog->GetQuickDescription().str());

			DeleteProg(i);
		}
		else if (prog.IsMarkOnly()) {
			config.logit("Adding '%s' to recorded list (Mark Only)", prog.GetQuickDescription().str());

			recordedlist.AddProg(prog, false);

			DeleteProg(i);
		}
		else if (prog.GetStop() == prog.GetStart()) {
			config.logit("'%s' is zero-length", prog.GetQuickDescription().str());

			DeleteProg(i);
		}
		else if (!prog.AllowRepeats() && ((otherprog = runninglist.FindSimilar(prog)) != NULL)) {
			config.logit("'%s' is being recorded now ('%s')", prog.GetQuickDescription().str(), otherprog->GetQuickDescription().str());

			DeleteProg(i);
		}
		else i++;
	}

	config.logit("Removed recorded and finished programmes");

	n = ScheduleEx(runninglist, scheduledlist, rejectedlist, starttime);

	scheduledlist.WriteToFile(config.GetScheduledFile(), false);
	for (i = 0; i < rejectedlist.Count(); i++) {
		rejectedlist.GetProgWritable(i).SetRejected();
	}
	rejectedlist.WriteToFile(config.GetRejectedFile(), false);

	for (i = 0; i < rejectedlist.Count(); i++) {
		const ADVBProg& prog = rejectedlist.GetProg(i);
		const ADVBProg  *rejprog;

		if ((rejprog = oldrejectedlist.FindUUID(prog.GetUUID())) != NULL) {
			config.printf("%s is still rejected", prog.GetDescription().str());
			oldrejectedlist.DeleteProg(*rejprog);
		}
		else {
			config.printf("%s is a newly rejected programme", prog.GetDescription().str());
			newrejectedlist.AddProg(prog, false, false);
		}
	}

	if ((newrejectedlist.Count() > 0) ||
		(oldrejectedlist.Count() > 0)) {
		AString cmd;

		if ((cmd = config.GetConfigItem("rejectedcmd")).Valid()) {
			AString  filename = config.GetLogDir().CatPath("rejected-text-" + ADateTime().DateFormat("%Y-%M-%D") + ".txt");
			AStdFile fp;

			if (fp.open(filename, "w")) {
				fp.printf("Summary of newly rejected and previously rejected programmes at %s\n", ADateTime().DateToStr().str());

				if (newrejectedlist.Count() > 0) {
					fp.printf("\nProgrammes newly rejected:\n");

					for (i = 0; i < newrejectedlist.Count(); i++) {
						const ADVBProg& prog = newrejectedlist.GetProg(i);

						fp.printf("%s\n", prog.GetDescription().str());
					}
				}

				if (oldrejectedlist.Count() > 0) {
					fp.printf("\nProgrammes no *longer* rejected:\n");

					for (i = 0; i < oldrejectedlist.Count(); i++) {
						const ADVBProg& prog = oldrejectedlist.GetProg(i);

						fp.printf("%s\n", prog.GetDescription().str());
					}
				}

				if (rejectedlist.Count() > 0) {
					fp.printf("\nProgrammes rejected:\n");

					for (i = 0; i < rejectedlist.Count(); i++) {
						const ADVBProg& prog = rejectedlist.GetProg(i);

						fp.printf("%s\n", prog.GetDescription().str());
					}
				}

				fp.close();

				cmd = cmd.SearchAndReplace("{logfile}", filename);

				RunAndLogCommand(cmd);
			}

			remove(filename);
		}
	}

	if (recordedlist.Count() > reccount) {
		config.printf("Updating list of recorded programmes");

		recordedlist.WriteToFile(config.GetRecordedFile(), false);
	}

	return n;
}

int ADVBProgList::SortDataLists(uptr_t item1, uptr_t item2, void *context)
{
	// item1 and item2 are ADataList objects, sort them according to the number of items in the list (fewest first);
	const ADataList& list1 = *(const ADataList *)item1;
	const ADataList& list2 = *(const ADataList *)item2;
	const ADVBProg&  prog1 = *(const ADVBProg *)list1[0];
	const ADVBProg&  prog2 = *(const ADVBProg *)list2[0];

	UNUSED(context);

	return ADVBProg::CompareScore(prog1, prog2);
}

void ADVBProgList::CountOverlaps(ADataList& repeatlists)
{
	uint_t i;
	
	// set scores for the first programme of each list (for sorting below)
	for (i = 0; i < repeatlists.Count(); i++) {
		ADataList& list = *(ADataList *)repeatlists[i];
		uint_t j;
		bool   changed = false;
		
		for (j = 0; j < list.Count(); j++) {
			ADVBProg& prog = *(ADVBProg *)list[j];

			changed |= prog.CountOverlaps(*this);
			prog.SetPriorityScore();
		}
		
		if (changed) list.Sort(&ADVBProg::SortListByOverlaps);
	}
}

void ADVBProgList::PrioritizeProgrammes(ADVBProgList *schedulelists, uint64_t *recstarttimes, uint_t nlists, ADVBProgList& rejectedlist)
{
	const ADVBConfig& config = ADVBConfig::Get();
	// for each programme, attach it to a list of repeats
	ADataList repeatlists;
	uint_t i;

	repeatlists.SetDestructor(&DeleteDataList);

	for (i = 0; i < Count(); i++) {
		ADVBProg& prog = GetProgWritable(i);

		// no point checking anything that is already part of a list!
		if (!prog.GetList()) {
			ADataList *list;

			// this programme MUST be part of a new list since it isn't already part of an existing list
			if ((list = new ADataList) != NULL) {
				// dvbadd list to list of repeat lists
				repeatlists.Add((uptr_t)list);

				// add this programme to the list
				prog.AddToList(list);

				// check all SUBSEQUENT programmes to see if they are repeats of this prog
				// and if so, add them to the list

				uint_t j;
				for (j = i + 1; j < Count(); j++) {
					ADVBProg& prog1 = GetProgWritable(j);

					if (ADVBProg::SameProgramme(prog, prog1)) {
						// prog1 is the same programme as prog, add prog1 to the list
						prog1.AddToList(list);
					}
				}
			}
		}
	}

	// count overlaps for every programme
	CountOverlaps(repeatlists);

	// sort lists so that programme with fewest repeats is first
	repeatlists.Sort(&SortDataLists);

	//config.logit("Using priority scale %d, repeats scale %d", ADVBProg::priscale, ADVBProg::repeatsscale);

	// display all repeats of each found programme
	for (i = 0; i < repeatlists.Count(); i++) {
		const ADataList& list = *(const ADataList *)repeatlists[i];
		const ADVBProg&  prog = *(const ADVBProg *)list[0];

		config.logit("Order %u/%d: '%s' has %u repeats, pri %d, %u overlaps (score %d)", i + 1, repeatlists.Count(), prog.GetQuickDescription().str(), list.Count(), prog.GetPri(), prog.GetOverlaps(), prog.GetPriorityScore());

		uint_t j, n = list.Count();
		for (j = 0; j < n; j++) {
			const ADVBProg& prog = *(const ADVBProg *)list[j];

			config.logit("    '%s' has %u overlaps", prog.GetQuickDescription().str(), prog.GetOverlaps());
		}
	}

	// clear owner list for each programme
	for (i = 0; i < Count(); i++) {
		ADVBProg& prog = GetProgWritable(i);

		prog.ClearList();
	}

	bool done = false;
	while (!done) {
		done = true;
		
		for (i = 0; i < repeatlists.Count(); i++) {
			ADataList& list = *(ADataList *)repeatlists[i];

			if (list.Count() > 0) {
				ADataList deletelist;
				ADVBProg  *prog = (ADVBProg *)list.First();
				uint_t j, k;
				
				// find a list to schedule this programme on
				for (j = 0; j < nlists; j++) {
					uint_t vcard = (j + i) % nlists;
					ADVBProgList& schedulelist = schedulelists[vcard];

					// programme must start after list's rec start time and not overlap anything already on it
					if ((prog->GetStart() >= recstarttimes[vcard]) && !schedulelist.FindRecordOverlap(*prog)) {
						// this programme doesn't overlap anything else or anything scheduled -> this can definitely be recorded

						// add to scheduling list
						schedulelist.AddProg(*prog);

						// remove programme from list to check
						deletelist.Add(list[0]);	// add to delete list that is used later to delete programme from this object
						list.Pop();
						
						// remove any programmes that do NOT have the allow repeats flag set
						// OR that use the same base channel
						static const uint64_t hour = 3600UL * 1000UL;
						AString  channel = prog->GetBaseChannel();
						bool     plus1   = prog->IsPlus1();
						uint64_t start   = prog->GetStart() - (plus1 ? hour : 0);
						uint64_t start1  = start + hour;
						for (k = 0; k < list.Count();) {
							const ADVBProg& prog2 = *(const ADVBProg *)list[k];

							if (!prog2.AllowRepeats()) {
								deletelist.Add(list[k]);	// add to delete list that is used later to delete programme from this object
								list.RemoveIndex(k);
							}
							else if ((prog2.GetBaseChannel() == channel) &&
									 (( prog2.IsPlus1() && (prog2.GetStart() == start1)) ||
									  (!prog2.IsPlus1() && (prog2.GetStart() == start)))) {
								config.logit("Removing '%s' because it is +/- 1 hour from '%s'", prog2.GetQuickDescription().str(), prog->GetQuickDescription().str());
								deletelist.Add(list[k]);	// add to delete list that is used later to delete programme from this object
								list.RemoveIndex(k);
							}
							else k++;
						}

						config.logit("'%s' does not overlap: can be recorded (card %u, order %u, programmes left %u)",
									 prog->GetQuickDescription().str(),
									 vcard,
									 i + 1,
									 list.Count());
						
						break;
					}
				}

				if (j == nlists) {
					if (list.Count() == 1) {
						// the last repeat overlaps something so cannot be recorded -> the programme is *rejected*
						config.logit("No repeats of '%s' can be recorded!", prog->GetQuickDescription().str());
						
						rejectedlist.AddProg(*prog);
					}
					
					// remove programme from list to check
					list.Pop();
				}

				if (deletelist.Count() > 0) {
					// delete programmes from this list
					for (j = 0; j < deletelist.Count(); j++) {
						DeleteProg(*(ADVBProg *)deletelist[j]);
					}

					// now count overlaps again
					CountOverlaps(repeatlists);
				}
				
				// any non-empty lists cause process to be repeated
				done &= (list.Count() == 0);
			}
		}
	}

	config.printf("Prioritization complete");
	
	for (i = 0; i < nlists; i++) {
		ADVBProgList& scheduledlist = schedulelists[i];

		// sort list
		scheduledlist.Sort();

		// tweak record times to prevent pre/post handles overlapping
		scheduledlist.AdjustRecordTimes();
	}
}

uint_t ADVBProgList::ScheduleEx(const ADVBProgList& runninglist, ADVBProgList& allscheduledlist, ADVBProgList& allrejectedlist, const ADateTime& starttime)
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBProgList *schedulelists;
	uint64_t  *recstarttimes;
	AString   filename;
	uint64_t  recstarttime = (uint64_t)starttime;
	uint_t    i, j;

	// read DVB card mappings
	(void)config.GetPhysicalDVBCard(0, true);
	uint_t ncards = config.GetMaxDVBCards();

	if (!ncards) {
		config.printf("No DVB cards available! %u programmes left to schedule", Count());

		allrejectedlist = *this;
		return Count();
	}

	// allow at least 30s before first schedule point
	recstarttime += 30000ULL;

	schedulelists = new ADVBProgList[ncards];
	recstarttimes = new uint64_t[ncards];
	for (i = 0; i < ncards; i++) {
		// set initial earliest schedule time
		recstarttimes[i] = recstarttime;
	}
	
	// for running list, adjust earliest scheduling time so as not to trampling on ongoing recordings
	if (runninglist.Count()) {
		for (i = 0; i < runninglist.Count(); i++) {
			const ADVBProg& prog = runninglist[i];
			uint_t vcard = config.GetVirtualDVBCard(prog.GetDVBCard());
			
			if (vcard < ncards) {
				recstarttimes[vcard] = MAX(recstarttimes[vcard], prog.GetRecordStop() + 10000);
				config.logit("Adjusting earliest record start time to %s for card %u (physical card %u)", ADateTime(recstarttimes[vcard]).DateToStr().str(), vcard, (uint_t)prog.GetDVBCard());
			}
		}
	}

	// round all earliest schedule times to minute boundaries
	uint64_t minrecstarttime = 0;
	for (i = 0; i < ncards; i++) {
		// round up to the next minute
		recstarttimes[i] += 60000ULL - (recstarttimes[i] % 60000ULL);

		if (!i || (recstarttimes[i] < minrecstarttime)) minrecstarttime = recstarttimes[i];
	}

	// generate record data, clear disabled flag and clear owner list for each programme
	for (i = 0; i < Count(); i++) {
		ADVBProg& prog = GetProgWritable(i);

		prog.GenerateRecordData(minrecstarttime);
		prog.ClearList();
	}

	config.logit("--------------------------------------------------------------------------------");
	config.printf("Requested: %u programmes:", Count());
	config.logit("--------------------------------------------------------------------------------");
	for (i = 0; i < Count(); i++) {
		config.logit("%s", GetProg(i).GetDescription(1).str());
	}
	config.logit("--------------------------------------------------------------------------------");

	//ADVBProg::debugsameprogramme = true;
	PrioritizeProgrammes(schedulelists, recstarttimes, ncards, allrejectedlist);
	//ADVBProg::debugsameprogramme = false;

	for (i = 0; i < ncards; i++) {
		ADVBProgList& scheduledlist = schedulelists[i];
		const ADVBProg *errorprog;
		uint_t dvbcard = config.GetPhysicalDVBCard(i);
		
		if ((errorprog = scheduledlist.FindFirstRecordOverlap()) != NULL) {
			config.printf("Error: found overlap in schedule list for card %u ('%s')!", i, errorprog->GetQuickDescription().str());
		}

		config.logit("--------------------------------------------------------------------------------");
		config.printf("Card %u (hardware card: %u): Scheduling: %u programmes:", i, dvbcard, scheduledlist.Count());
		config.logit("--------------------------------------------------------------------------------");
		for (j = 0; j < scheduledlist.Count(); j++) {
			ADVBProg& prog = scheduledlist.GetProgWritable(j);
			const ADVBProg *rprog;
			
			prog.SetScheduled();
			prog.SetDVBCard(dvbcard);

			allscheduledlist.AddProg(prog, false);

			while ((rprog = allrejectedlist.FindSimilar(prog)) != NULL) {
				config.logit("Removing '%s' from rejected list because it is going to be recorded ('%s')",
							 rprog->GetQuickDescription().str(),
							 prog.GetQuickDescription().str());

				allrejectedlist.DeleteProg(*rprog);
			}
			
			config.logit("%s (%s - %s)",
						 prog.GetDescription(1).str(),
						 prog.GetRecordStartDT().UTCToLocal().DateFormat("%d %D-%N-%Y %h:%m:%s").str(),
						 prog.GetRecordStopDT().UTCToLocal().DateFormat("%h:%m:%s").str());
		}
		config.logit("--------------------------------------------------------------------------------");
	}

	delete[] recstarttimes;
	delete[] schedulelists;

	allscheduledlist.Sort();
	allrejectedlist.Sort();
	
	config.logit("--------------------------------------------------------------------------------");
	config.printf("Scheduling: %u programmes in total:", allscheduledlist.Count());
	config.logit("--------------------------------------------------------------------------------");
	for (i = 0; i < allscheduledlist.Count(); i++) {
		const ADVBProg& prog = allscheduledlist.GetProg(i);

		config.logit("%s (%s - %s) (DVB card %u)",
					 prog.GetDescription(1).str(),
					 prog.GetRecordStartDT().UTCToLocal().DateFormat("%d %D-%N-%Y %h:%m:%s").str(),
					 prog.GetRecordStopDT().UTCToLocal().DateFormat("%h:%m:%s").str(),
					 prog.GetDVBCard());
	}
	config.logit("--------------------------------------------------------------------------------");
	
	return allrejectedlist.Count();
}

bool ADVBProgList::WriteToJobList()
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADateTime dt;
	const AString filename = config.GetScheduledFile();
	bool success = false;

	if (config.GetRecordingHost().Valid()) {
		ADVBProgList scheduledlist;
		ADVBProgList unscheduledlist;
		uint_t tries;
		uint_t i;

		for (tries = 0; (tries < 3) && !success; tries++) {
			unscheduledlist.DeleteAll();

			if (tries) config.printf("Scheduling try %u/3:", tries + 1);

			success = (SendFileRunRemoteCommand(filename, "dvb --write-scheduled-jobs") &&
					   scheduledlist.ModifyFromRecordingHost(config.GetScheduledFile(), ADVBProgList::Prog_ModifyAndAdd));

			if (!success) config.printf("Remote scheduling failed!");

			for (i = 0; i < scheduledlist.Count(); i++) {
				if (!scheduledlist[i].GetJobID()) unscheduledlist.AddProg(scheduledlist[i]);
			}

			if (success && !scheduledlist.Count()) break;
		}

		if (unscheduledlist.Count()) {
			AStdFile fp;
			AString  filename = config.GetTempFile("unscheduled", ".txt");
			AString  cmd;

			config.printf("--------------------------------------------------------------------------------");
			config.printf("%u programmes unscheduled:", unscheduledlist.Count());
			for (i = 0; i < unscheduledlist.Count(); i++) {
				config.printf("%s", unscheduledlist[i].GetQuickDescription().str());
			}
			config.printf("--------------------------------------------------------------------------------");

			success = false;

			if ((cmd = config.GetConfigItem("schedulefailurecmd", "")).Valid()) {
				if (fp.open(filename, "w")) {
					fp.printf("The following programmes have NOT been scheduled:\n");
					for (i = 0; i < unscheduledlist.Count(); i++) {
						fp.printf("%s\n", unscheduledlist[i].GetDescription(10).str());
					}
					fp.close();

					cmd = cmd.SearchAndReplace("{logfile}", filename);
					RunAndLogCommand(cmd);

					remove(filename);
				}
				else config.printf("Failed to open text file for logging unscheduled!");
			}
		}
	}
	else {
		ADVBProgList scheduledlist, runninglist;

		runninglist.ReadFromJobQueue('=');

		UnscheduleAllProgrammes();

		if (scheduledlist.ReadFromFile(filename)) {
			uint_t i, nsucceeded = 0, nfailed = 0;

			config.logit("--------------------------------------------------------------------------------");
			config.printf("Writing %u programmes to job list (%u job(s) running)", scheduledlist.Count(), runninglist.Count());

			success = true;
			for (i = 0; i < scheduledlist.Count(); i++) {
				const ADVBProg *rprog;
				ADVBProg& prog = scheduledlist.GetProgWritable(i);

				if ((rprog = runninglist.FindUUID(prog)) != NULL) {
					prog.SetJobID(rprog->GetJobID());

					config.logit("Recording of '%s' already running (job %u)!", prog.GetDescription().str(), prog.GetJobID());
				}
				else if (prog.WriteToJobQueue()) {
					config.logit("Scheduled '%s' okay, job %u", prog.GetDescription().str(), prog.GetJobID());

					nsucceeded++;
				}
				else {
					nfailed++;
					success = false;
				}
			}

			config.logit("--------------------------------------------------------------------------------");

			config.printf("Scheduled %u programmes successfully (%u failed), writing to '%s'", nsucceeded, nfailed, filename.str());

			if (!scheduledlist.WriteToFile(filename, false)) {
				config.logit("Failed to write updated schedule list '%s'", filename.str());
				success = false;
			}
		}
	}

	if (!success) {
		AString cmd;

		if ((cmd = config.GetConfigItem("schedulefailurecmd")).Valid()) {
			AString logfile = config.GetTempFile("schedulefailure", ".txt");

			config.ExtractLogData(dt, ADateTime(), logfile);

			cmd = cmd.SearchAndReplace("{logfile}", logfile);
			RunAndLogCommand(cmd);

			remove(logfile);
		}
	}

	return success;
}

bool ADVBProgList::CreateCombinedFile()
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBLock     lock("dvbfiles");
	ADVBProgList list;
	bool		 success = false;

	config.printf("Creating combined listings");

	if (list.ReadFromBinaryFile(config.GetRecordedFile())) {
		list.EnhanceListings();
		if (list.WriteToFile(config.GetCombinedFile())) success = true;
		else config.logit("Failed to write combined listings file");
	}
	else config.logit("Failed to read recorded programmes list for generating combined file");

	return success;
}

void ADVBProgList::EnhanceListings()
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBProgList list;
	uint_t added, modified;

	CreateHash();

	list.DeleteAll();
	if (list.ReadFromBinaryFile(config.GetScheduledFile())) {
		Modify(list, added, modified, Prog_Add);
	}
	else config.logit("Failed to read scheduled programme list for enhancing listings");

	list.DeleteAll();
	if (list.ReadFromBinaryFile(config.GetRecordingFile())) {
		Modify(list, added, modified, Prog_ModifyAndAdd);
	}
	else config.logit("Failed to read running programme list for enhancing listings");

	list.DeleteAll();
	if (list.ReadFromBinaryFile(config.GetProcessingFile())) {
		Modify(list, added, modified, Prog_ModifyAndAdd);
	}
	else config.logit("Failed to read processing programme list for enhancing listings");

	list.DeleteAll();
	if (list.ReadFromBinaryFile(config.GetRecordFailuresFile())) {
		Modify(list, added, modified, Prog_ModifyAndAdd);
	}
	else config.logit("Failed to read record failures list for enhancing listings");

	list.DeleteAll();
	if (list.ReadFromBinaryFile(config.GetRejectedFile())) {
		Modify(list, added, modified, Prog_ModifyAndAdd);
	}
	else config.logit("Failed to read rejected programme list for enhancing listings");
}

void ADVBProgList::CheckRecordingFile()
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBLock     lock("dvbfiles");
	ADVBProgList list;
	uint_t i;

	//config.logit("Creating combined list");

	if (list.ReadFromBinaryFile(config.GetRecordingFile())) {
		uint64_t now     = (uint64_t)ADateTime().TimeStamp(true);
		bool     changed = false;

		for (i = 0; i < list.Count();) {
			const ADVBProg& prog = list.GetProg(i);

			if (now >= (prog.GetRecordStop() + 60000)) {
				config.printf("Programme '%s' should have stopped recording by now, removing it from the running list", prog.GetQuickDescription().str());
				list.DeleteProg(i);
				changed = true;
			}
			else i++;
		}

		if (changed) {
			config.logit("Running list changed, writing new version");
			list.WriteToFile(config.GetRecordingFile());
		}
	}
	else config.logit("Failed to read running programme list for checking");
}

bool ADVBProgList::GetAndConvertRecordings()
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBLock     lock("pullrecordings");
	ADVBProgList reclist;
	bool success = false;

	config.printf("Getting and converting recordings from record host '%s'", config.GetRecordingHost().str());

	if (reclist.ModifyFromRecordingHost(config.GetRecordedFile(), ADVBProgList::Prog_Add)) {
		AString   cmd;
		uint32_t  tick;
		uint_t i, converted = 0;
		bool      reschedule = false;

		GetRecordingListFromRecordingSlave();

		tick = GetTickCount();
		cmd.Delete();
		cmd.printf("nice rsync -v --partial --remove-source-files --ignore-missing-args %s %s:%s/'*.mpg' %s",
				   config.GetRsyncArgs().str(),
				   config.GetRecordingHost().str(),
				   config.GetRecordingsStorageDir().str(),
				   config.GetRecordingsStorageDir().str());

		if (RunAndLogCommand(cmd)) config.printf("File copy took %ss", AValue((GetTickCount() + 999 - tick) / 1000).ToString().str());
		else					   config.printf("Warning: Failed to copy all recorded programmes from recording host");

		CreateDirectory(config.GetSlaveLogDir());

		tick = GetTickCount();
		cmd.Delete();
		cmd.printf("nice rsync -z --partial --ignore-missing-args %s %s:%s/'dvb*.txt' %s",
				   config.GetRsyncArgs().str(),
				   config.GetRecordingHost().str(),
				   config.GetLogDir().str(),
				   config.GetSlaveLogDir().str());

		if (RunAndLogCommand(cmd)) config.printf("Log file copy took %ss", AValue((GetTickCount() + 999 - tick) / 1000).ToString().str());
		else					   config.printf("Warning: Failed to copy all DVB logs from recording host");

		ADVBProgList convertlist;
		for (i = 0; i < reclist.Count(); i++) {
			const ADVBProg& prog = reclist.GetProg(i);

			if (!prog.IsConverted() && AStdFile::exists(prog.GetFilename())) {
				if (prog.IsOnceOnly() && prog.IsRecordingComplete() && !config.IsRecordingSlave()) {
					if (ADVBPatterns::DeletePattern(prog.GetUser(), prog.GetPattern())) {
						const bool rescheduleoption = config.RescheduleAfterDeletingPattern(prog.GetUser(), prog.GetCategory());

						config.printf("Deleted pattern '%s', %srescheduling...", prog.GetPattern(), rescheduleoption ? "" : "NOT ");

						reschedule |= rescheduleoption;
					}
				}

				convertlist.AddProg(prog);
			}
		}

		if (reschedule) ADVBProgList::SchedulePatterns();

		success = true;
		for (i = 0; i < convertlist.Count(); i++) {
			ADVBProg& prog = convertlist.GetProgWritable(i);

			config.printf("Converting file %u/%u - '%s':", i + 1, reclist.Count(), prog.GetQuickDescription().str());

			if (prog.ConvertVideo(true)) {
				converted++;
			}
			else {
				success = false;
				prog.OnRecordFailure();
			}
		}

		if (converted) config.printf("%u programmes converted", converted);
	}
	else config.printf("Unable to retreive recordings from recording host");

	return success;
}

bool ADVBProgList::GetRecordingListFromRecordingSlave()
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBLock     lock("dvbfiles");
	ADVBProgList failurelist;
	ADateTime 	 combinedwritetime = ADateTime::MinDateTime;
	FILE_INFO 	 info;
	bool      	 success = true, update = false;

	config.printf("Updating recording list from record host '%s'", config.GetRecordingHost().str());

	if (::GetFileInfo(config.GetCombinedFile(), &info)) combinedwritetime = info.WriteTime;

	if (!failurelist.ModifyFromRecordingHost(config.GetRecordFailuresFile(), Prog_Add)) {
		config.printf("Failed to get and modify failures list");
		success = false;
	}

	if (::GetFileInfo(config.GetRecordFailuresFile(), &info)) update |= (info.WriteTime > combinedwritetime);

	if (!GetFileFromRecordingHost(config.GetRecordingFile())) {
		config.printf("Failed to get recording list");
		success = false;
	}

	if (::GetFileInfo(config.GetRecordingFile(), &info)) update |= (info.WriteTime > combinedwritetime);

	if (update) success &= ADVBProgList::CreateCombinedFile();

	return success;
}

bool ADVBProgList::CheckRecordingNow()
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBLock     lock("dvbfiles");
	ADVBProgList scheduledlist, recordinglist;
	bool         success = false;

	if (config.GetRecordingHost().Valid() && !GetRecordingListFromRecordingSlave()) return false;

	if (recordinglist.ReadFromFile(config.GetRecordingFile())) {
		recordinglist.CreateHash();

		if (scheduledlist.ReadFromFile(config.GetScheduledFile())) {
			ADVBProgList   shouldberecordinglist, shouldntberecordinglist;
			const uint64_t now = (uint64_t)ADateTime().TimeStamp(true), slack = (uint64_t)2 * (uint64_t)60000;
			uint_t i;

			lock.ReleaseLock();

			success = true;

			for (i = 0; i < scheduledlist.Count(); i++) {
				const ADVBProg& prog = scheduledlist[i];

				if ((now >= (prog.GetRecordStart() + slack)) && (now < prog.GetRecordStop())) {
					if (!recordinglist.FindUUID(prog)) shouldberecordinglist.AddProg(prog);
				}
				else if ((now < prog.GetRecordStart()) || (now > (prog.GetRecordStop() + slack))) {
					if (recordinglist.FindUUID(prog)) shouldntberecordinglist.AddProg(prog);
				}
			}

			if (shouldberecordinglist.Count() || shouldntberecordinglist.Count()) {
				AString report;

				if (shouldberecordinglist.Count()) {
					report.printf("--------------------------------------------------------------------------------\n");
					report.printf("%u programmes should be recording:\n", shouldberecordinglist.Count());

					for (i = 0; i < shouldberecordinglist.Count(); i++) {
						const ADVBProg& prog = shouldberecordinglist[i];

						report.printf("%s\n", prog.GetQuickDescription().str());
					}
					report.printf("--------------------------------------------------------------------------------\n");

					if (shouldntberecordinglist.Count()) report.printf("\n");
				}

				if (shouldntberecordinglist.Count()) {
					report.printf("--------------------------------------------------------------------------------\n");
					report.printf("%u programmes shouldn't be recording:\n", shouldntberecordinglist.Count());

					for (i = 0; i < shouldntberecordinglist.Count(); i++) {
						const ADVBProg& prog = shouldntberecordinglist[i];

						report.printf("%s\n", prog.GetQuickDescription().str());
					}
					report.printf("--------------------------------------------------------------------------------\n");
				}

				config.printf("%s", report.str());

				AString cmd;
				if ((cmd = config.GetConfigItem("recordingcheckcmd", "")).Valid()) {
					AStdFile fp;
					AString  filename = config.GetTempFile("recordingcheck", ".txt");

					if (fp.open(filename, "w")) {
						fp.printf("%s", report.str());
						fp.close();

						cmd = cmd.SearchAndReplace("{logfile}", filename);
						RunAndLogCommand(cmd);

						remove(filename);
					}
					else config.printf("Failed to open text file for logging recording errors!");
				}
			}
		}
		else config.printf("Failed to read scheduled list");
	}
	else config.printf("Failed to read recording list");

	return success;
}

void ADVBProgList::FindSeries(AHash& hash) const
{
	uint_t i;

	hash.Delete();
	hash.Create(50, &DeleteSeries);
	hash.EnableCaseInSensitive(true);

	for (i = 0; i < Count(); i++) {
		const ADVBProg&          prog    = GetProg(i);
		const ADVBProg::EPISODE& episode = prog.GetEpisode();

		if (episode.valid && episode.episode) {
			uint_t ser = episode.series;
			uint_t epn = episode.episode;
			uint_t ept = episode.episodes;
			SERIES *series;

			if (!ser) {
				if (epn >= 100) ept = 100;
				ser = ((epn - 1) / 100);
				epn = 1 + ((epn - 1) % 100);
			}

			if (((series = (SERIES *)hash.Read(prog.GetTitle())) == NULL) &&
				((series = new SERIES) != NULL)) {
				series->title = prog.GetTitle();
				series->list.SetDestructor(&AString::DeleteString);
				hash.Insert(prog.GetTitle(), (uptr_t)series);
			}

			if (series) {
				ADataList& list = series->list;
				AString    *str;

				if (((str = (AString *)list[ser]) == NULL) &&
					((str = new AString) != NULL)) {
					list.Replace(ser, (uptr_t)str);
					*str = AString("-").Copies(ept);
				}

				if (str) {
					AString& elist = *str;
					uint_t   ind   = epn - 1;

					if (ind >= (uint_t)elist.len()) elist += AString("-").Copies(ind + 1 - elist.len());

					char t = elist[ind], t1 = t;
					if		(prog.IsScheduled())	    t1 = 's';
					else if	(prog.HasRecordingFailed()) t1 = 'f';
					else if	(prog.IsAvailable())  		t1 = 'a';
					else if	(prog.IsRecorded())  		t1 = 'r';
					else if (prog.IsRejected())  		t1 = 'x';

					static const char *allowablechanges[] = {
						"-sfarx",
						"sfar",
						"fsrax",
						"asx",
						"rasx",
						"xsfar",
					};
					uint_t i;
					for (i = 0; i < NUMBEROF(allowablechanges); i++) {
						if ((t == allowablechanges[i][0]) && strchr(allowablechanges[i] + 1, t1)) {
							t = t1;
							break;
						}
					}

					if (t != elist[ind]) {
						elist = elist.Left(ind) + AString(t) + elist.Mid(ind + 1);
					}

					//debug("%s: %u/%u: %s (%s)\n", prog.GetTitle(), ser, epn, str->str(), prog.GetDescription(1).str());
				}
			}
		}
	}
}

double ADVBProgList::ScoreProgrammeByPopularityFactors(const ADVBProg& prog, void *context)
{
	const POPULARITY_FACTORS& factors = *(const POPULARITY_FACTORS *)context;
	double score = 0.0;

	if (prog.IsRecorded())  score += factors.recordedfactor;
	if (prog.IsScheduled()) score += factors.scheduledfactor;
	if (prog.IsRejected())  score += factors.rejectedfactor;
	score *= factors.priorityfactor * pow(2.0, (double)prog.GetPri());

	if (factors.userfactors.Exists(prog.GetUser())) {
		score *= (double)factors.userfactors.Read(prog.GetUser());
	}

	return score;
}

bool ADVBProgList::__CollectPopularity(const char *key, uptr_t item, void *context)
{
	AList& list = *(AList *)context;

	UNUSED(item);

	list.Add(new AString(key));

	return true;
}

int  ADVBProgList::__ComparePopularity(const AListNode *pNode1, const AListNode *pNode2, void *pContext)
{
	const AHash&   	  hash   = *(const AHash *)pContext;
	const AString& 	  title1 = *AString::Cast(pNode1);
	const AString& 	  title2 = *AString::Cast(pNode2);
	const POPULARITY& pop1   = *(const POPULARITY *)hash.Read(title1);
	const POPULARITY& pop2   = *(const POPULARITY *)hash.Read(title2);
	int res = 0;

	if (pop2.score > pop1.score) res =  1;
	if (pop2.score < pop1.score) res = -1;

	if (res == 0) res = stricmp(title1, title2);

	return res;
}

void ADVBProgList::FindPopularTitles(AList& list, double (*fn)(const ADVBProg& prog, void *context), void *context) const
{
	AHash hash(100, &__DeletePopularity);
	uint_t i;

	for (i = 0; i < Count(); i++) {
		const ADVBProg& prog  = GetProg(i);
		const AString&  title = prog.GetTitle();
		POPULARITY      *pop  = (POPULARITY *)hash.Read(title);

		if (!pop && ((pop = new POPULARITY) != NULL)) {
			memset(pop, 0, sizeof(*pop));

			hash.Insert(title, (uptr_t)pop);
		}

		if (pop) {
			pop->count++;
			pop->score += (*fn)(prog, context);
		}
	}

	hash.Traverse(&__CollectPopularity, &list);
	list.Sort(&__ComparePopularity, &hash);

#if 0
	const AString *str = AString::Cast(list.First());
	while (str) {
		const POPULARITY& pop = *(const POPULARITY *)hash.Read(*str);

		debug("Title '%s' count %u score %u\n", str->str(), pop.count, pop.score);

		str = str->Next();
	}
#endif
}
