
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <algorithm>

#include <rdlib/strsup.h>
#include <rdlib/Regex.h>

#include "config.h"
#include "proglist.h"
#include "channellist.h"

enum {
	MaxCards = 4,
};

void findcards(void)
{
	const ADVBConfig& config = ADVBConfig::Get();

	if (config.GetRecordingSlave().Valid()) {
		RunRemoteCommandGetFile("dvb --find-cards", config.GetDVBCardsFile());
	}
	else {
		static const AString str = "using dvb card";
		const ADVBChannelList::CHANNEL *channel = ADVBChannelList::Get().GetChannel(0);
		const uint32_t freq = channel ? channel->dvb.freq : (uint32_t)((double)config.GetDVBFrequencyRange().Column(0) * 1.0e6);
		AString file;
		AString oldcards, newcards;
		uint_t i;

		if (config.MonitorDVBSignal() && (system("killall dvbfemon") != 0)) {
			config.logit("Failed to kill running dvbfemon processes");
		}

		file = config.GetTempFile("cards", ".txt");

		oldcards.ReadFromFile(config.GetDVBCardsFile());

		for (i = 0; i < MaxCards; i++) {
			AStdFile fp;
			AString  cmd;
			AString  cardname;
			bool     locked = false;

			//config.printf("Trying %sHz on card %u...", AValue(freq).ToString().str(), i);
			cmd.printf("timeout 20s dvbtune -c %u -f %s 2>%s", i, AValue(freq).ToString().str(), file.str());

			remove(file);

			int res = system(cmd);
			(void)res;
			if (fp.open(file)) {
				AString line;

				while (line.ReadLn(fp) >= 0) {
					int p;

					//debug("Find card %u: %s\n", i, line.str());

					if ((p = line.PosNoCase(str)) >= 0) {
						p += str.len();
						cardname = line.Mid(p).RemoveWhiteSpace().DeQuotify().RemoveWhiteSpace();
					}
					else if ((line.PosNoCase("fe_has_signal") >= 0) &&
							 (line.PosNoCase("fe_has_lock")   >= 0) &&
							 (line.PosNoCase("fe_has_sync")   >= 0)) {
						locked = true;
					}
					else if (line.PosNoCase("device or resource busy") >= 0) {
						// card is in use -> assume it has not changed
						// need to find card in existing list
						uint_t j, n = oldcards.CountLines();

						for (j = 0; j < n; j++) {
							if ((uint_t)oldcards.Line(j).Word(0) == i) {
								cardname = oldcards.Line(j).Words(1);
								config.printf("Warning: card %u is busy, assuming it is '%s'", i, cardname.str());
								locked = true;
								break;
							}
						}
					}
					
					if (cardname.Valid() && locked) break;
				}

				fp.close();

				if (cardname.Valid() && locked) {
					config.printf("%u: %s", i, cardname.str());
					newcards.printf("%u %s\n", i, cardname.str());

					if (config.MonitorDVBSignal()) {
						AString cmd;
						cmd.printf("femon -a %u 2>/dev/null | dvbfemon --card %u >/dev/null &", i, i);
						if (system(cmd) != 0) {
							config.logit("Command '%s' failed", cmd.str());
						}
					}
				}
				else if (cardname.Valid()) config.printf("%u: %s **NO SIGNAL**", i, cardname.str());
				else if (locked)		   config.printf("%u: card **LOCKED** but no card name found", i);
				else					   config.printf("%u: no card found", i);
			}
			else config.printf("%u: no output", i);
		}

		remove(file);

		if (newcards != oldcards) {
			newcards.WriteToFile(config.GetDVBCardsFile());

			AString cmd;
			if ((cmd = config.GetConfigItem("dvbcardschangedcmd")).Valid()) {
				AStdFile fp;

				file = config.GetTempFile("cards", ".txt");

				if (fp.open(file, "w")) {
					fp.printf("DVB card list changed from:\n%s\nto:\n%s",
							  oldcards.str(),
							  newcards.str());
					fp.close();
				}

				cmd = cmd.SearchAndReplace("{logfile}", file);
				if (system(cmd) != 0) {
					config.logit("Command '%s' failed!", cmd.str());
				}

				remove(file);
			}
		}
	}
}

sint_t findcard(const AString& pattern, const std::vector<uint_t> *cardlist)
{
	AStdFile fp;
	AString pat = ParseRegex(pattern);
	sint_t card = -1;

	if (fp.open(ADVBConfig::Get().GetDVBCardsFile())) {
		AString line;

		while (line.ReadLn(fp) >= 0) {
			uint_t testcard = (uint_t)line.Word(0);

			if ((!cardlist || (std::find(cardlist->begin(), cardlist->end(), testcard) == cardlist->end())) && MatchRegex(line.Words(1), pat)) {
				card = testcard;
				break;
			}
		}

		fp.close();
	}

	return card;
}
