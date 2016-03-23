
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <rdlib/strsup.h>
#include <rdlib/Regex.h>

#include "config.h"
#include "proglist.h"

enum {
	MaxCards = 4,
};

void findcards(void)
{
	const ADVBConfig& config = ADVBConfig::Get();
	
	if (config.GetRecordingHost().Valid()) {
		RunRemoteCommandGetFile("dvb --find-cards", config.GetDVBCardsFile());
	}
	else {
		static const AString str = "using dvb card";
		const uint32_t freq = (uint32_t)((double)config.GetDVBFrequencyRange().Column(0) * 1.0e6);
		AString file;
		AString oldcards, newcards;
		uint_t i;

		if (system("killall dvbfemon") != 0) {
			config.logit("Failed to kill running dvbfemon processes");
		}
		
		file = config.GetTempFile("cards", ".txt");

		oldcards.ReadFromFile(config.GetDVBCardsFile());

		for (i = 0; i < MaxCards; i++) {
			AString cmd;
			AStdFile fp;

			cmd.printf("dvbtune -c %u -f %s 2>%s", i, AValue(freq).ToString().str(), file.str());

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
						line = line.Mid(p).RemoveWhiteSpace().DeQuotify().RemoveWhiteSpace();

						config.printf("%u: %s", i, line.str());
						newcards.printf("%u %s\n", i, line.str());

						AString cmd;
						cmd.printf("femon -a %u 2>/dev/null | dvbfemon --card %u &", i, i);
						if (system(cmd) != 0) {
							config.logit("Command '%s' failed", cmd.str());
						}
						break;
					}
				}
			
				fp.close();
			}
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

sint_t findcard(const AString& pattern, const ADataList *cardlist)
{
	AStdFile fp;
	AString pat = ParseRegex(pattern);
	sint_t card = -1;

	if (fp.open(ADVBConfig::Get().GetDVBCardsFile())) {
		AString line;
				
		while (line.ReadLn(fp) >= 0) {
			uint_t testcard = (uint_t)line.Word(0);
			
			if ((!cardlist || (cardlist->Find(testcard) < 0)) && MatchRegex(line.Words(1), pat)) {
				card = testcard;
				break;
			}
		}

		fp.close();
	}

	return card;
}
