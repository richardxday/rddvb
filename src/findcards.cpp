
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
	
	if (config.GetRemoteHost().Valid()) {
		RunCommandGetFile("dvb --find-cards", config.GetDVBCardsFile());
	}
	else {
		static const AString str = "using dvb card";
		AString file;
		AStdFile ofp;
		uint_t i;

		file = config.GetTempFile("cards", ".txt");

		ofp.open(config.GetDVBCardsFile(), "w");

		for (i = 0; i < MaxCards; i++) {
			AString cmd;
			AStdFile fp;

			cmd.printf("dvbtune -c %u -f 1 2>%s", i, file.str());

			remove(file);

			if (system(cmd) == 0) {
				if (fp.open(file)) {
					AString line;

					while (line.ReadLn(fp) >= 0) {
						int p;
					
						//debug("Find card %u: %s\n", i, line.str());

						if ((p = line.PosNoCase(str)) >= 0) {
							p += str.len();
							line = line.Mid(p).RemoveWhiteSpace().DeQuotify().RemoveWhiteSpace();

							config.printf("%u: %s", i, line.str());
							ofp.printf("%u %s\n", i, line.str());
							break;
						}
					}
			
					fp.close();
				}
			}
		}

		remove(file);

		ofp.close();
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
