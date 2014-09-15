
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/statvfs.h>

#include <rdlib/Regex.h>
#include <rdlib/Recurse.h>

#include "config.h"
#include "proglist.h"
#include "channellist.h"
#include "episodehandler.h"
#include "dvblock.h"

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
	const ADVBConfig& config = ADVBConfig::Get();
	uint_t i;

	DeleteAll();

	for (i = 0; i < channellist.Count(); i++) {
		const CHANNEL *channel = (const CHANNEL *)list.channellist[i];

		AddChannel(channel->id, channel->name);
	}

	for (i = 0; (i < list.Count()) && !config.HasQuit(); i++) {
		const ADVBProg& prog = list[i];

		AddProg(prog, false, false);
	}

	return *this;
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

void ADVBProgList::AddXMLTVChannel(const AString& channel)
{
	static const ADVBConfig::REPLACEMENT replacements[] = {
		{" +1", "+1"},
		{" Northern", ""},
		{" Southern", ""},
		{" North", ""},
		{" South", ""},
		{" West", ""},
		{" East", ""},
		{" Granada", ""},
		{" London", ""},
		{" Midlands", ""},
		{" Yorkshire & Cumbria", ""},
		{" Yorkshire & Lincolnshire", ""},
		{" Border", ""},
		{" England", ""},
		{" Wales", ""},
		{" Scotland", ""},
		{" Ireland", ""},
		{" Channel Islands", ""},
		{" (freeview)", ""},
		{" [freeview]", ""},
		{" (Granada/Border)", ""},
		{" (W)", ""},
		{" (E)", ""},
		{"BBC One", "BBC1"},
		{"BBC Two", "BBC2"},
		{"BBC Three", "BBC3"},
		{"BBC Four", "BBC4"},
		{"CBBC Channel", "CBBC"},
		{"Community Channel", "Community"},
		{"Pick TV", "Pick"},
	};
	AString id   = channel.GetField("channel id=\"", "\"");
	AString name = channel.GetField("<display-name>", "</display-name>").DeHTMLify();

	name = ADVBConfig::replace(name, replacements, NUMBEROF(replacements));

	AddChannel(id, name);
}

void ADVBProgList::AddChannel(const AString& id, const AString& name)
{
	CHANNEL *channel = NULL;

	if (id.Valid() && ((channel = GetChannelWritable(id)) == NULL)) {
		if (!channelhash.Valid()) channelhash.Create(40);

		if ((channel = new CHANNEL) != NULL) {
			channel->id   = id;
			channel->name = name;

			//ADVBConfig::Get().logit("Added channel id '%s' name '%s'", channel->id.str(), channel->name.str());

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

	if ((channel = GetChannel(id)) != NULL) return channel->name;

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
				valid = true;
				break;
			}
		}
	}

	return valid;
}

bool ADVBProgList::ReadFromXMLTVFile(const AString& filename)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AHash     channelidvalidhash(100);
	AString   data;
	FILE_FIND info;
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
					channel = channel.DeHTMLify();
					AddXMLTVChannel(channel);

					channel.Delete();
				}
			}
			else if (programme.Valid()) {
				programme += line;
				if (programme.PosNoCase("</programme>") >= 0) {
					AString channelid = programme.GetField(" channel=\"", "\"").DeHTMLify();
					AString channel   = LookupXMLTVChannel(channelid);

					if (!channelidvalidhash.Exists(channelid)) {
						bool valid = ValidChannelID(channelid);

						channelidvalidhash.Insert(channelid, (uint_t)valid);

						if (!valid) debug("Channel ID '%s' (channel '%s') is%s valid\n", channelid.str(), channel.str(), valid ? "" : " NOT");
					}

					if (channelidvalidhash.Read(channelid)) {
						ADateTime start, stop;
						AString   str;

						start.FromTimeStamp(programme.GetField(" start=\"", "\""), true);
						stop.FromTimeStamp(programme.GetField(" stop=\"", "\""), true);

						str.printf("\n");
						str.printf("start=%s\n", ADVBProg::GetHex(start).str());
						str.printf("stop=%s\n",  ADVBProg::GetHex(stop).str());

						str.printf("channel=%s\n", channel.str());
						str.printf("channelid=%s\n", channelid.str());

						AString attrs = programme.GetXMLAttributes();
						uint_t i, n = attrs.CountLines("\n", 0);
						for (i = 1; i < n; i++) {
							AString line = attrs.Line(i, "\n", 0);
							AString var, value;
							bool valid = false;

							if (line.Left(2) == "</") {
								var   = line.Mid(2).SearchAndReplace(">", "").Word(0);
								value = attrs.Line(i - 1, "\n", 0);
								if ((i >= 2) && (value == "</value>")) value = attrs.Line(i - 2, "\n", 0);
								valid = ((var != "value") && (value.FirstChar() != '<'));
							}
							else if ((line.FirstChar() == '<') && (line.Right(2) == "/>")) {
								var   = line.Mid(1).SearchAndReplace("/>", "").Word(0);
								value = line.Mid(1 + var.len(), line.len() - 1 - var.len() - 2).Words(0);
								valid = (var != "value");
							}

							if (valid) {
								var   = var.SearchAndReplace("-", "");
								value = value.DeHTMLify();

								str.printf("%s=%s\n", var.str(), value.str());
							}
						}
						
						if (AddProg(str, true, true) < 0) {
							config.printf("Failed to add prog!\n");
							success = false;
						}
					}

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

			if (config.HasQuit()) break;
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

			if (AddProg(str, notempty, notempty) < 0) {
				config.printf("Failed to add prog\n");
				success = false;
			}

			p = p1 + 1;

			if (config.HasQuit()) break;
		}
	}

	return success;
}

bool ADVBProgList::ReadRadioListingsFromHTMLFile(const AString& filename, const AString& channel, const AString& channelid, uint32_t day, bool removeoverlaps)
{
	const ADVBConfig& config = ADVBConfig::Get();
    AString data;
    bool    success = true;

    if (data.ReadFromFile(filename)) {
        typedef enum {
            Decode_Idle = 0,
            Decode_LookForTitle,
            Decode_LookForSubTitle,
            Decode_LookForSynopsis,
        } State_t;
        AString prog;
        AString str;
        State_t state = Decode_Idle;
        int     p, p1;

        data = data.SearchAndReplace("\n", "").SearchAndReplace("\r", "").SearchAndReplace("&mdash;", "-").Words(0);

        for (p = 0; data[p] && success && !config.HasQuit(); ) {
            if ((data[p] == '<') && ((p1 = data.Pos(">", p)) >= 0)) {
                AString property, content, datatype, classname;

                p1++;
                str.Create(data.str() + p, p1 - p, false);
                str = str.Words(0);

                if ((property = str.GetField(" property=\"", "\"").Words(0).ToLower()).Valid() &&
                    (datatype = str.GetField(" datatype=\"", "\"").Words(0).ToLower()).Valid() &&
                    (content  = str.GetField(" content=\"",  "\"")).Valid() &&
                    ((property == "startdate") || (property == "enddate")) &&
                    (datatype == "xsd:datetime")) {
                    ADateTime dt;

                    if (property == "startdate") {
                        dt.FromTimeStamp(content, true);
                        prog.Delete();
                        prog.printf("\nstart=%s\n", ADVBProg::GetHex(dt).str());
                    }
                    else if (property == "enddate") {
                        dt.FromTimeStamp(content, true);
                        prog.printf("stop=%s\n", ADVBProg::GetHex(dt).str());
                    }
                }
                else if ((classname = str.GetField(" class=\"", "\"").Words(0).ToLower()).Valid() &&
                         (classname.PosNoCase("programme__") >= 0)) {
                    if      (classname.PosNoCase("programme__title")    >= 0) state = Decode_LookForTitle;
                    else if (classname.PosNoCase("programme__subtitle") >= 0) state = Decode_LookForSubTitle;
                    else if (classname.PosNoCase("programme__synopsis") >= 0) state = Decode_LookForSynopsis;
                }
                p = p1;
            }
            else {
                if ((p1 = data.Pos("<", p)) < 0) p1 = data.len();

                str.Create(data.str() + p, p1 - p, false);
                str = str.Words(0);

                if (str.Valid()) {
                    switch (state) {
                        case Decode_Idle: break;

                        case Decode_LookForTitle:
                            prog.printf("title=%s\n", str.str());
                            break;

                        case Decode_LookForSubTitle:
                            prog.printf("subtitle=%s\n", str.str());
                            break;

                        case Decode_LookForSynopsis: {
                            ADVBProg *pprog;

                            prog.printf("desc=%s\n", str.str());
                            prog.printf("channel=%s\n", channel.str());
                            prog.printf("basechannel=%s\n", channel.str());
                            prog.printf("channelid=%s\n", channelid.str());
                            prog.printf("dvbchannel=%s\n", channel.str());

                            if ((pprog = new ADVBProg(prog)) != NULL) {
                                pprog->SetUUID();
                                if (pprog->Valid()) {
                                    if (pprog->GetStartDT().GetDays() == day) {
                                        pprog->SetRadioProgramme();

                                        if (AddProg(pprog, true, removeoverlaps) < 0) {
                                            config.printf("Failed to add prog '%s'", pprog->GetDescription().str());
                                            delete pprog;
                                            success = false;
                                        }
                                    }
                                    else {
                                        //config.printf("Ingoring programme '%s'", pprog->GetDescription().str());
                                        delete pprog;
                                    }
                                }
                                else {
                                    config.printf("Failed to add prog, prog is invalid");
                                    delete pprog;
                                    success = false;
                                }
                            }

                            prog.Delete();
                            break;
                        }
                    }
                    state = Decode_Idle;
                }

                p = p1;
            }
        }
    }
    else {
        config.printf("Failed to read listings from '%s'", filename.str());
        success = false;
    }

    return success;
}

bool ADVBProgList::ReadRadioListings()
{
    static const AString baseurl = "http://www.bbc.co.uk/{station}/programmes/schedules";
    static const struct {
        const char *station;
        const char *channel;
        const char *extra;
    } radiostations[] = {
        {"radio1", "BBC Radio 1", NULL},
        {"1xtra",  "BBC R1X", NULL},
        {"radio2", "BBC Radio 2", NULL},
        {"radio3", "BBC Radio 3", NULL},
        {"radio4", "BBC Radio 4", "/fm"},
        {"6music", "BBC 6 Music", NULL},
    };
	const ADVBConfig& config = ADVBConfig::Get();
    uint32_t day  = ADateTime().TimeStamp(true).GetDays();
    uint_t   days = 7;
    uint_t   i, j, k;
	bool     notempty = (Count() != 0);
	bool     success = true;

    for (j = 0; (j < days) && success && !config.HasQuit(); j++) {
        for (i = 0; (i < NUMBEROF(radiostations)) && success && !config.HasQuit(); i++) {
            AString url, cmd;
            AString filename = config.GetTempFile("radio", ".html");
            AString channel  = radiostations[i].channel;
            AString date     = ADateTime(day + j, 0).DateFormat("%Y/%M/%D");

            url.printf("%s", baseurl.SearchAndReplace("{station}", radiostations[i].station).str());
            if (radiostations[i].extra) url.printf("%s", radiostations[i].extra);
            url.printf("/%s", date.str());

            config.printf("Downloading listings for '%s' for %s...", channel.str(), date.str());

            cmd.printf("wget \"%s\" -O \"%s\" 2>/dev/null", url.str(), filename.str());

            for (k = 0; k < 3; k++) {
                if (system(cmd) == 0) break;
            }

            if (k < 3) {
                AString channelid;
                uint_t count = Count();

                channelid.printf("radio.%s.bbc.co.uk", radiostations[i].station);

                success = ReadRadioListingsFromHTMLFile(filename, channel, channelid, day + j, notempty);
                config.printf("Downloading listings for '%s' for %s...%u programmes added", channel.str(), date.str(), Count() - count);
            }
            else {
                config.printf("Failed to download listings from '%s'", url.str());
                success = false;
            }

            remove(filename);
        }
    }

	return success;
}

bool ADVBProgList::ReadFromBinaryFile(const AString& filename, bool sort, bool removeoverlaps, bool reverseorder)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AStdFile fp;
	bool success = false;

	if (fp.open(filename, "rb")) {
		ADVBProg prog;

		fp.seek(0, SEEK_END);
		sint32_t size = fp.tell(), pos = 0;
		fp.rewind();

		while (pos < size) {
			if ((prog = fp).Valid()) {
				if (AddProg(prog, sort, removeoverlaps, reverseorder) < 0) {
					config.printf("Failed to add prog!\n");
				}
			}

			pos = fp.tell();

			if (config.HasQuit()) break;
		}

		//config.printf("Read data from '%s', parsing complete", filename.str());

		success = true;
	}

	return success;
}

bool ADVBProgList::ReadFromFile(const AString& filename)
{
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
	else {
		success = ReadFromBinaryFile(filename, notempty, notempty);
	}

	return success;
}

bool ADVBProgList::ReadFromJobQueue(int queue, bool runningonly)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString listname   = config.GetTempFile("atlist", ".txt");
	AString scriptname = config.GetTempFile("atjob",  ".txt");
	AString cmd1, cmd2, cmd;
	bool success = false;

	cmd2.printf("atq -q = >%s", listname.str());
	if (!runningonly) cmd1.printf("atq -q %c >>%s", queue, listname.str());

	if ((system(cmd1) == 0) && (system(cmd2) == 0)) {
		AStdFile fp;

		if (fp.open(listname)) {
			AString line;

			success = true;
			while (line.ReadLn(fp) >= 0) {
				uint_t jobid = (uint_t)line.Word(0);

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
					else {
						config.logit("Failed to decode job base64 programme for %u", jobid);
						success = false;
					}
				}
				else config.logit("Failed to read job %u", jobid);

				if (config.HasQuit()) break;
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
	int  queues[] = {'=', config.GetQueue().FirstChar()};
	uint_t i;
	bool success = true;

	for (i = 0; i < NUMBEROF(queues); i++) {
		success &= ReadFromJobQueue(queues[i], runningonly);
	}

	return success;
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

bool ADVBProgList::WriteToFile(const AString& filename, bool updatecombined) const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AStdFile fp;
	bool success = false;

	if (fp.open(filename, "wb")) {
		uint_t i;

		for (i = 0; i < Count(); i++) {
			const ADVBProg& prog = GetProg(i);

			prog.WriteToFile(fp);

			if (config.HasQuit()) break;
		}

		fp.close();

		success = true;
	}

	if (updatecombined) {
		FILE_FIND combined_info, scheduled_info, recorded_info, rejected_info;

		if (!::GetFileDetails(config.GetCombinedFile(),  &combined_info) ||
			(::GetFileDetails(config.GetScheduledFile(), &scheduled_info) && (scheduled_info.WriteTime > combined_info.WriteTime)) ||
			(::GetFileDetails(config.GetRecordedFile(),  &recorded_info)  && (recorded_info.WriteTime  > combined_info.WriteTime)) ||
			(::GetFileDetails(config.GetRejectedFile(),  &rejected_info)  && (rejected_info.WriteTime  > combined_info.WriteTime))) {
			CreateCombinedList();
		}
	}

	return success;
}

bool ADVBProgList::WriteToTextFile(const AString& filename) const
{
	const ADVBConfig& config = ADVBConfig::Get();
	AStdFile fp;
	bool success = false;

	if (fp.open(filename, "w")) {
		uint_t i;

		for (i = 0; i < Count(); i++) {
			fp.printf("%s", GetProg(i).ExportToText().str());

			if (config.HasQuit()) break;
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

int ADVBProgList::AddProg(const AString& prog, bool sort, bool removeoverlaps, bool reverseorder)
{
	ADVBProg *pprog;
	int index = -1;

	if ((pprog = new ADVBProg(prog)) != NULL) {
		if (pprog->Valid()) {
			index = AddProg(pprog, sort, removeoverlaps, reverseorder);
		}
		else delete pprog;
	}

	return index;
}

int ADVBProgList::AddProg(const ADVBProg& prog, bool sort, bool removeoverlaps, bool reverseorder)
{
	ADVBProg *pprog;
	int index = -1;

	if ((pprog = new ADVBProg(prog)) != NULL) {
		if (pprog->Valid()) {
			index = AddProg(pprog, sort, removeoverlaps, reverseorder);
		}
		else delete pprog;
	}

	return index;
}

int ADVBProgList::AddProg(const ADVBProg *prog, bool sort, bool removeoverlaps, bool reverseorder)
{
	const ADVBConfig& config = ADVBConfig::Get();
	uint_t i;
	int  index = -1;

	AddChannel(prog->GetChannelID(), prog->GetChannel());

	if (removeoverlaps && Count()) {
		for (i = 0; (i < Count()) && !config.HasQuit();) {
			const ADVBProg& prog1 = GetProg(i);

			if (prog->OverlapsOnSameChannel(prog1)) {
                //config.printf("'%s' overlaps with '%s', deleting '%s'", prog1.GetQuickDescription().str(), prog->GetQuickDescription().str(), prog1.GetQuickDescription().str());
				DeleteProg(i);
			}
			else i++;
		}
	}

	if (reverseorder) {
		if (sort) {
			for (i = 0; (i < Count()) && !config.HasQuit(); i++) {
				const ADVBProg& prog1 = GetProg(i);

				if (*prog > prog1) {
					index = proglist.Add((uptr_t)prog, i);
					break;
				}
			}

			if (i == Count()) index = proglist.Add((uptr_t)prog);
		}
		else index = proglist.Add((uptr_t)prog, 0);
	}
	else {
		if (sort) {
			for (i = 0; (i < Count()) && !config.HasQuit(); i++) {
				const ADVBProg& prog1 = GetProg(i);

				if (*prog < prog1) {
					index = proglist.Add((uptr_t)prog, i);
					break;
				}
			}

			if (i == Count()) index = proglist.Add((uptr_t)prog);
		}
		else index = proglist.Add((uptr_t)prog);
	}

#if 0
	if (index >= 0) {
		CHANNEL *channel = (CHANNEL *)channelhash.Read(prog->GetChannelID());

		if (channel) {
			static const uint64_t slotlength   = 5ULL * 60ULL * 1000ULL;
			static const uint32_t slotsperweek = (uint32_t)((7ULL * 24ULL * 3600ULL * 1000ULL) / slotlength);
			uint64_t startslot = prog->GetStart() / slotlength;
			uint32_t nslots    = (uint32_t)(((prog->GetStop() + slotlength - 1) / slotlength) - startslot);

			if (!channel->data) {
				channel->startslot = startslot - (startslot % slotsperweek);
				channel->slots     = (uint32_t)(((startslot + nslots + slotsperweek - 1) / slotsperweek) * slotsperweek - channel->startslot);
				channel->data      = (uint8_t *)::Allocate(channel->data);
			}
		}
	}
#endif

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
	uint_t i;

	for (i = 0; i < Count();) {
		const ADVBProg& prog = GetProg(i);

		if (prog.GetStartDT().UTCToLocal() < dt) DeleteProg(i);
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

	if (proghash.Valid()) prog = (const ADVBProg *)proghash.Read(uuid);
	else {
		uint_t i, n = Count();

		for (i = 0; i < n; i++) {
			if (GetProg(i) == uuid) {
				prog = &GetProg(i);
				break;
			}
		}
	}
	
	return prog;
}

ADVBProg *ADVBProgList::FindUUIDWritable(const AString& uuid) const
{
	ADVBProg *prog = NULL;

	if (proghash.Valid()) prog = (ADVBProg *)proghash.Read(uuid);
	else {
		uint_t i, n = Count();

		for (i = 0; i < n; i++) {
			if (GetProg(i) == uuid) {
				prog = &GetProgWritable(i);
				break;
			}
		}
	}
	
	return prog;
}

void ADVBProgList::FindProgrammes(ADVBProgList& dest, const ADataList& patternlist, uint_t maxmatches) const
{
	const ADVBConfig& config = ADVBConfig::Get();
	uint_t i, j, n = patternlist.Count(), nfound = 0;

	//config.logit("Searching using %u patterns (max matches %u)...", n, maxmatches);

	for (i = 0; (i < Count()) && !config.HasQuit() && (nfound < maxmatches); i++) {
		const ADVBProg& prog = GetProg(i);
		ADVBProg *prog1 = NULL;
		bool     addtolist = false;
		bool     excluded  = false;

		for (j = 0; (j < n) && !config.HasQuit(); j++) {
			const PATTERN& pattern = *(const PATTERN *)patternlist[j];

			if (pattern.enabled && prog.Match(pattern)) {
				if (pattern.exclude) {
					excluded = true;
					break;
				}

				if (!prog1) prog1 = new ADVBProg(prog);

				if (prog1) {
					if (!prog1->GetPattern()[0]) {
						prog1->SetPattern(pattern.pattern);
					
						if (pattern.user.Valid()) {
							prog1->SetUser(pattern.user);
						}
					
						prog1->AssignValues(pattern);
					}
					else prog1->UpdateValues(pattern);

					if (!pattern.scorebased) {
						addtolist = true;
						break;
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

	ADVBProg::ParsePatterns(patternlist, patterns, errors, sep);

	FindProgrammes(dest, patternlist, maxmatches);
}

uint_t ADVBProgList::FindSimilar(ADVBProgList& dest, const ADVBProg& prog, uint_t index) const
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

const ADVBProg *ADVBProgList::FindSimilar(const ADVBProg& prog) const
{
	const ADVBProg *res = NULL;
	uint_t i;

	for (i = 0; i < Count(); i++) {
		const ADVBProg& prog1 = GetProg(i);

		if (ADVBProg::SameProgramme(prog, prog1)) {
			res = &prog1;
			break;
		}
	}

	return res;
}

ADVBProg *ADVBProgList::FindOverlap(const ADVBProg& prog1, const ADVBProg *prog2) const
{
	const uint64_t st1 = prog1.GetStart();
	const uint64_t et1 = prog1.GetStop();
	uint_t i = 0;
	int  p;

	if (prog2 && ((p = proglist.Find((uptr_t)prog2)) >= 0)) i = p + 1;

	while ((prog2 = (const ADVBProg *)proglist[i++]) != NULL) {
		if (prog2 != &prog1) {
			const uint64_t st2 = prog2->GetStart();
			const uint64_t et2 = prog2->GetStop();

			if ((st2 < et1) && (et2 > st1)) break;
		}
	}

	return (ADVBProg *)prog2;
}

ADVBProg *ADVBProgList::FindRecordOverlap(const ADVBProg& prog1, const ADVBProg *prog2) const
{
	const uint64_t st1 = prog1.GetRecordStart();
	const uint64_t et1 = prog1.GetRecordStop();
	uint_t i = 0;
	int  p;

	if (prog2 && ((p = proglist.Find((uptr_t)prog2)) >= 0)) i = p + 1;

	while ((prog2 = (const ADVBProg *)proglist[i++]) != NULL) {
		if (prog2 != &prog1) {
			const uint64_t st2 = prog2->GetRecordStart();
			const uint64_t et2 = prog2->GetRecordStop();

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

			if (prog1->GetStop() == prog2->GetStart()) {
				config.logit("'%s' stops just as '%s' starts - shift times",
							 prog1->GetQuickDescription().str(),
							 prog2->GetQuickDescription().str());

				prog1->SetRecordStop(prog2->GetStart() - mininterrectime);
				prog2->SetRecordStart(prog2->GetStart());
			}
			else if (prog2->GetStop() == prog1->GetStart()) {
				config.logit("'%s' stops just as '%s' starts - shift times",
							 prog2->GetQuickDescription().str(),
							 prog1->GetQuickDescription().str());

				prog2->SetRecordStop(prog1->GetStart() - mininterrectime);
				prog1->SetRecordStart(prog1->GetStart());
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

	//if (pat1.pri < pat2.pri) return  1;
	//if (pat1.pri > pat2.pri) return -1;
	
	//if (!pat1.enabled &&  pat2.enabled) return 1;
	//if ( pat1.enabled && !pat2.enabled) return -1;

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
			ADVBProg::ParsePattern(patternlist, line, errors);
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
				ADVBProg::ParsePattern(patternlist, line, errors, user);
			}

			fp.close();
		}
		else config.logit("Failed to read file '%s'", file->str());
			
		file = file->Next();
	}

	if (sort) patternlist.Sort(&SortPatterns);
}

bool ADVBProgList::CheckDiskSpace(bool runcmd, bool report)
{
	ADataList patternlist;
	AString   errors;

	ReadPatterns(patternlist, errors);
	
	return CheckDiskSpace(patternlist, runcmd, report);
}

int ADVBProgList::SortProgsByUserThenDir(uptr_t item1, uptr_t item2, void *pContext)
{
	const ADVBProg& prog1 = *(const ADVBProg *)item1;
	const ADVBProg& prog2 = *(const ADVBProg *)item2;
	int res;

	UNUSED(pContext);

	if ((res = strcmp(prog1.GetUser(), prog2.GetUser())) != 0) return res;

	return strcmp(prog1.GetDir(), prog2.GetDir());
}

bool ADVBProgList::CheckDiskSpace(const ADataList& patternlist, bool runcmd, bool report)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AStdFile  fp;
	AHash     hash(30);
	ADataList proglist;
	AString   filename;
	double    lowlimit = config.GetLowSpaceWarningLimit();
	uint_t    i, userlen = 0;
	bool      first = true;
	bool      okay  = true;

	proglist.SetDestructor(&DeleteProg);

	for (i = 0; i < patternlist.Count(); i++) {
		const ADVBProg::PATTERN& pattern = *(const ADVBProg::PATTERN *)patternlist[i];
		ADVBProg *prog;

		if ((prog = new ADVBProg) != NULL) {
			prog->SetUser(pattern.user);
			prog->AssignValues(pattern);

			proglist.Add(prog);

			AString user = prog->GetUser();
			userlen = MAX(userlen, (uint_t)user.len());
		}
	}

	proglist.Sort(&SortProgsByUserThenDir);

	if (report) printf(",\"diskspace\":[");

	if (runcmd) {
		filename = config.GetTempFile("patterndiskspace", ".txt");
		if (fp.open(filename, "w")) {
			fp.printf("Disk space for patterns as of %s:\n\n", ADateTime().DateToStr().str());
		}
	}

	AString fmt;
	fmt.printf("%%-%us %%6.1lfG %%s", userlen);

	for (i = 0; i < proglist.Count(); i++) {
		const ADVBProg& prog = *(const ADVBProg *)proglist[i];
		AString  user, dir, rdir;

		user = prog.GetUser();
		dir  = prog.GetDir();
		rdir = config.GetRecordingsDir().CatPath(dir);

		if (!hash.Exists(rdir)) {
			struct statvfs fiData;

			hash.Insert(rdir, 0);

			if (statvfs(rdir, &fiData) >= 0) {
				AString str;
				double  gb = (double)fiData.f_bavail * (double)fiData.f_bsize / (1024.0 * 1024.0 * 1024.0);

				if (report) {
					if (first) first = false;
					else	   printf(",");

					printf("{\"user\":\"%s\"", ADVBProg::JSONFormat(user).str());
					printf(",\"folder\":\"%s\"", ADVBProg::JSONFormat(dir).str());
					printf(",\"fullfolder\":\"%s\"", ADVBProg::JSONFormat(rdir).str());
					printf(",\"freespace\":\"%0.1lfG\"", gb);
					printf(",\"level\":%u", (uint_t)(gb / lowlimit));
					printf("}");
				}

				str.printf(fmt, prog.GetUser(), gb, rdir.str());
				if (gb < lowlimit) {
					str.printf(" Warning!");
					okay = false;
				}

				if (runcmd) {
					fp.printf("%s\n", str.str());
				}

				//config.printf("%s", str.str());
			}
			//else config.printf("Directory '%s' doesn't exist!", rdir.str());
		}
	}

	if (runcmd) {
		fp.close();

		if (!okay) {
			AString cmd = config.GetConfigItem("lowdiskspacecmd");
			if (cmd.Valid()) {
				cmd = cmd.SearchAndReplace("{file}", filename);

				int res = system(cmd);
				if (res != 0) config.logit("Command '%s' failed!", cmd.str());
			}
		}
		else {
			AString cmd = config.GetConfigItem("diskspacecmd");
			if (cmd.Valid()) {
				cmd = cmd.SearchAndReplace("{file}", filename);

				int res = system(cmd);
				if (res != 0) config.logit("Command '%s' failed!", cmd.str());
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
	ADVBLock	 lock("schedule");
	ADVBProgList proglist;
	AString      filename = config.GetListingsFile();
	uint_t       res = 0;

	commit &= config.CommitScheduling();

	if (!commit) config.logit("** Not committing scheduling **");

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

		CheckDiskSpace(patternlist, true);

		config.printf("Finding programmes using %u patterns", patternlist.Count());

		proglist.FindProgrammes(reslist, patternlist);
		
		config.logit("Found %u programmes from %u patterns", reslist.Count(), patternlist.Count());

		res = reslist.Schedule(starttime);

		if (commit) WriteToJobList();
	}
	else config.logit("Failed to read listings file '%s'", filename.str());

	return res;
}

void ADVBProgList::Sort(bool reverse)
{
	proglist.Sort(&SortProgs, &reverse);
}

uint_t ADVBProgList::Schedule(const ADateTime& starttime)
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBLock     lock("schedule");
	ADVBProgList recordedlist;
	ADVBProgList scheduledlist;
	ADVBProgList rejectedlist;
	uint_t i, n, reccount;

	WriteToFile(config.GetRequestedFile(), false);

	recordedlist.ReadFromFile(config.GetRecordedFile());
	reccount = recordedlist.Count();

	n = ScheduleEx(recordedlist, scheduledlist, rejectedlist, starttime, 0);

	scheduledlist.Sort();
	rejectedlist.Sort();

	scheduledlist.WriteToFile(config.GetScheduledFile(), false);
	for (i = 0; i < rejectedlist.Count(); i++) {
		rejectedlist.GetProgWritable(i).SetRejected();
	}
	rejectedlist.WriteToFile(config.GetRejectedFile(), false);
	if (rejectedlist.Count() > 0) {
		AString cmd;

		if ((cmd = config.GetConfigItem("rejectedcmd")).Valid()) {
			AString  filename = config.GetLogDir().CatPath("rejected-text-" + ADateTime().DateFormat("%Y-%M-%D") + ".txt");
			AStdFile fp;

			if (fp.open(filename, "w")) {
				fp.printf("Programmes rejected %s:\n", ADateTime().DateToStr().str());

				for (i = 0; i < rejectedlist.Count(); i++) {
					const ADVBProg& prog = rejectedlist.GetProg(i);

					fp.printf("%s\n", prog.GetDescription().str());
				}

				fp.close();

				cmd = cmd.SearchAndReplace("{file}", filename);

				if (system(cmd) != 0) {
					config.printf("Command '%s' failed", cmd.str());
				}
			}

			remove(filename);
		}
	}

	if (recordedlist.Count() > reccount) {
		config.printf("Updating list of recorded programmes");

		recordedlist.WriteToFile(config.GetRecordedFile(), false);
	}

	CreateCombinedList();

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

void ADVBProgList::PrioritizeProgrammes(ADVBProgList& scheduledlist, ADVBProgList& rejectedlist)
{
	const ADVBConfig& config = ADVBConfig::Get();
	// for each programme, attach it to a list of repeats
	ADataList repeatlists;
	uint_t i;

	// next, generate record data, clear disabled flag and clear owner list for each programme
	for (i = 0; i < Count(); i++) {
		ADVBProg& prog = GetProgWritable(i);

		prog.GenerateRecordData();
		prog.ClearList();
	}

	repeatlists.SetDestructor(&DeleteDataList);
	for (i = 0; i < Count(); i++) {
		ADVBProg& prog = GetProgWritable(i);

		// no point checking anything that is already part of a list!
		if (!prog.GetList()) {
			ADataList *list;

			// this programme MUST be part of a new list since it isn't already part of an existing list
			if ((list = new ADataList) != NULL) {
				// add list to list of repeat lists
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

	// set scores for the first programme of each list (for sorting below)
	for (i = 0; i < repeatlists.Count(); i++) {
		ADataList& list = *(ADataList *)repeatlists[i];
		uint_t j;

		for (j = 0; j < list.Count(); j++) {
			ADVBProg& prog = *(ADVBProg *)list[j];

			prog.SetPriorityScore();
			prog.CountOverlaps(*this);
		}

		list.Sort(&ADVBProg::SortListByOverlaps);
	}

	// sort lists so that programme with fewest repeats is first
	repeatlists.Sort(&SortDataLists);

	//config.logit("Using priority scale %d, repeats scale %d", ADVBProg::priscale, ADVBProg::repeatsscale);

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

	for (i = 0; i < Count(); i++) {
		ADVBProg& prog = GetProgWritable(i);

		prog.ClearList();
	}

	for (i = 0; i < repeatlists.Count(); i++) {
		const ADataList& list = *(const ADataList *)repeatlists[i];
		uint_t j;

		for (j = 0; j < list.Count(); j++) {
			const ADVBProg& prog = *(const ADVBProg *)list[j];

			if (!scheduledlist.FindOverlap(prog)) {
				// this programme doesn't overlap anything else or anything scheduled -> this can definitely be recorded
				config.logit("'%s' does not overlap: can be recorded", prog.GetQuickDescription().str());

				// add to scheduling list
				scheduledlist.AddProg(prog);
				break;
			}
		}

		if (j == list.Count()) {
			const ADVBProg& prog = *(const ADVBProg *)list[0];

			// this programme doesn't overlap anything else or anything scheduled -> this can definitely be recorded
			config.logit("No repeats of '%s' can be recorded!", prog.GetQuickDescription().str());

			rejectedlist.AddProg(prog);
		}
	}

	scheduledlist.Sort();
	rejectedlist.Sort();

	// tweak record times to prevent pre/post handles overlapping
	scheduledlist.AdjustRecordTimes();
}

uint_t ADVBProgList::ScheduleEx(ADVBProgList& recordedlist, ADVBProgList& allscheduledlist, ADVBProgList& allrejectedlist, const ADateTime& starttime, uint_t card)
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBProgList scheduledlist;
	ADVBProgList rejectedlist;
	ADVBProgList runninglist;
	AString filename;
	uint64_t lateststart = SUBZ((uint64_t)starttime, (uint64_t)config.GetLatestStart() * 60000ULL);		// set latest start of programmes to be scheduled
	uint_t   i;

	if (Count() == 0) {
		config.printf("All programmes can be recorded!");

		return 0;
	}

	if (card >= config.GetMaxCards()) {
		config.printf("Reached the end of DVB cards, %u programmes left to schedule", Count());

		for (i = 0; i < Count(); i++) {
			const ADVBProg& prog = GetProg(i);

			allrejectedlist.AddProg(prog, false);
		}

		return Count();
	}

	config.logit("--------------------------------------------------------------------------------");
	config.printf("Card %u: Found: %u programmes:", card, Count());
	config.logit("--------------------------------------------------------------------------------");
	for (i = 0; i < Count(); i++) {
		config.logit("%s", GetProg(i).GetDescription(1).str());
	}
	config.logit("--------------------------------------------------------------------------------");

	// read running job(s)
	runninglist.ReadFromJobQueue('=');

	// first, remove programmes that do not have a valid DVB channel
	// then, remove any that have already finished
	// then, remove any that overlap with any running job(s)
	// finally, remove any that have already been recorded
	for (i = 0; i < Count();) {
		const ADVBProg *otherprog;
		ADVBProg& prog = GetProgWritable(i);

		if (!prog.GetDVBChannel()[0]) {
			config.logit("'%s' does not have a DVB channel", prog.GetDescription().str());

			DeleteProg(i);
		}
		else if (prog.GetStart() < lateststart) {
			config.logit("'%s' started too long ago (%u mins)", prog.GetQuickDescription().str(), (uint_t)(((uint64_t)starttime - (uint64_t)prog.GetStart()) / 60000ULL));

			DeleteProg(i);
		}
		else if ((otherprog = runninglist.FindRecordOverlap(prog)) != NULL) {
			config.logit("'%s' overlaps running job ('%s')", prog.GetQuickDescription().str(), otherprog->GetQuickDescription().str());

			DeleteProg(i);
		}
		else if (!prog.AllowRepeats() && ((otherprog = recordedlist.FindSimilar(prog)) != NULL)) {
			config.logit("'%s' has already been recorded ('%s')", prog.GetQuickDescription().str(), otherprog->GetQuickDescription().str());

			DeleteProg(i);
		}
		else if (prog.IsMarkOnly()) {
			config.logit("Adding '%s' to recorded list (Mark Only)", prog.GetQuickDescription().str());

			recordedlist.AddProg(prog, false, false, true);

			DeleteProg(i);
		}
		else i++;
	}

	config.logit("Removed recorded and finished programmes");

	config.logit("--------------------------------------------------------------------------------");
	config.printf("Card %u: Requested: %u programmes:", card, Count());
	config.logit("--------------------------------------------------------------------------------");
	for (i = 0; i < Count(); i++) {
		config.logit("%s", GetProg(i).GetDescription(1).str());
	}
	config.logit("--------------------------------------------------------------------------------");

	ADVBProg::debugsameprogramme = true;
	PrioritizeProgrammes(scheduledlist, rejectedlist);
	ADVBProg::debugsameprogramme = false;

	if (scheduledlist.FindFirstRecordOverlap()) {
		config.printf("Error: found overlap in schedule list for card %u!", card);		
	}
	
	config.logit("--------------------------------------------------------------------------------");
	config.printf("Card %u: Scheduling: %u programmes:", card, scheduledlist.Count());
	config.logit("--------------------------------------------------------------------------------");
	for (i = 0; i < scheduledlist.Count(); i++) {
		const ADVBProg& prog = scheduledlist[i];

		config.logit("%s (%s - %s)",
					 prog.GetDescription(1).str(),
					 prog.GetRecordStartDT().UTCToLocal().DateFormat("%d %D-%N-%Y %h:%m:%s").str(),
					 prog.GetRecordStopDT().UTCToLocal().DateFormat("%h:%m:%s").str());
	}
	config.logit("--------------------------------------------------------------------------------");

	for (i = 0; i < scheduledlist.Count(); i++) {
		ADVBProg& prog = scheduledlist.GetProgWritable(i);

		prog.SetScheduled();
		prog.SetDVBCard(card);

		allscheduledlist.AddProg(prog, false);
	}

	return rejectedlist.ScheduleEx(recordedlist, allscheduledlist, allrejectedlist, starttime, card + 1);
}

void ADVBProgList::SimpleSchedule()
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBProgList joblist, recordedlist, templist;
	uint64_t now = (uint64_t)ADateTime().TimeStamp(true);
	uint_t i, nsucceeded = 0, nfailed = 0;

	joblist.ReadFromJobList();
	recordedlist.ReadFromFile(config.GetRecordedFile());

	for (i = 0; i < Count(); i++) {
		ADVBProg& prog = GetProgWritable(i);
		const ADVBProg *prog1;

		if (!prog.GetDVBChannel()[0]) {
			config.logit("'%s' does not have a DVB channel", prog.GetDescription().str());
			continue;
		}
		else if (prog.GetStop() <= now) {
			config.printf("'%s' already finished", prog.GetQuickDescription().str());
			continue;
		}
		else if ((prog1 = recordedlist.FindSimilar(prog)) != NULL) {
			config.printf("'%s' already recorded ('%s')", prog.GetQuickDescription().str(), prog1->GetQuickDescription().str());
			continue;
		}

		prog.GenerateRecordData();

		if ((prog1 = joblist.FindRecordOverlap(prog)) != NULL) {
			config.logit("Cannot schedule '%s', overlaps with '%s'", prog.GetDescription().str(), prog1->GetDescription().str());
		}
		else if ((prog1 = templist.FindSimilar(prog)) != NULL) {
			config.logit("No need to schedule '%s', it is already going to be recorded with '%s'", prog.GetDescription().str(), prog1->GetDescription().str());
		}
		else templist.AddProg(prog);
	}

	templist.AdjustRecordTimes();

	for (i = 0; i < templist.Count();) {
		const ADVBProg& prog = templist.GetProg(i);
		const ADVBProg *prog1;
		
		if ((prog1 = templist.FindRecordOverlap(prog)) != NULL) {
			config.logit("Cannot schedule '%s', overlaps with '%s' in record list", prog.GetDescription().str(), prog1->GetDescription().str());
			templist.DeleteProg(i);
		}
		else i++;
	}

	config.logit("--------------------------------------------------------------------------------");
	config.printf("Writing %u programmes to job list", templist.Count());

	for (i = 0; i < templist.Count(); i++) {
		ADVBProg& prog = templist.GetProgWritable(i);

		prog.SetManualRecording();

		if (prog.WriteToJobQueue()) {
			config.logit("Scheduled '%s' okay, job %u", prog.GetDescription().str(), prog.GetJobID());
			nsucceeded++;
		}
		else {
			nfailed++;
		}
	}

	config.logit("--------------------------------------------------------------------------------");
	config.printf("Scheduled %u programmes successfully (%u failed)", nsucceeded, nfailed);
}

bool ADVBProgList::WriteToJobList()
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBProgList scheduledlist, runninglist;
	AString filename = config.GetScheduledFile();
	bool success = false;

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

		if (!scheduledlist.WriteToFile(filename)) {
			config.logit("Failed to write updated schedule list '%s'\n", filename.str());
		}
	}

	return success;
}

void ADVBProgList::CreateCombinedList()
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBLock     lock("schedule");
	ADVBProgList list, list2;
	uint_t i;

	//config.logit("Creating combined list");

	if (!list.ReadFromBinaryFile(config.GetRecordedFile(), false, false, true)) {
		config.logit("Failed to read recorded programme list for generating combined list");
	}

	list2.DeleteAll();
	if (list2.ReadFromBinaryFile(config.GetScheduledFile())) {
		for (i = 0; i < list2.Count(); i++) {
			const ADVBProg& prog = list2.GetProg(i);
			if (!list.FindUUID(prog)) {
				list.AddProg(prog, false);
			}
		}
	}
	else config.logit("Failed to read scheduled programme list for generating combined list");

	list2.DeleteAll();
	if (list2.ReadFromBinaryFile(config.GetRejectedFile())) {
		for (i = 0; i < list2.Count(); i++) {
			const ADVBProg& prog = list2.GetProg(i);
			if (!list.FindUUID(prog)) {
				list.AddProg(prog);
			}
		}
	}
	else config.logit("Failed to read rejected programme list for generating combined list");
	
	if (!list.WriteToFile(config.GetCombinedFile())) {
		config.logit("Failed to write programme combined list");
	}
}

void ADVBProgList::FindSeries(AHash& hash) const
{
	uint_t i;

	hash.Delete();
	hash.Create(50, &DeleteSeries);

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
				ser = 1 + ((epn - 1) / 100);
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
					if (elist[ind] == '-') {
						char t;

						if		(prog.IsRecorded())  t = 'r';
						else if (prog.IsScheduled()) t = 's';
						else if (prog.IsRejected())  t = 'x';
						else						 t = '*';

						elist = elist.Left(ind) + AString(t) + elist.Mid(ind + 1);
					}
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
