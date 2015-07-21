
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
	const AString str = "using dvb card";
	AString file;
	AStdFile ofp;
	uint_t i;

	file = config.GetTempFile("cards", ".txt");

	ofp.open(config.GetDVBCardsFile(), "w");

	for (i = 0; i < MaxCards; i++) {
		AString cmd;
		AStdFile fp;

		cmd.printf("dvbtune -c %u -f 1 2>%s", i, file.str());
		//debug("cmd: '%s'\n", cmd.str());

		remove(file);

		if ((system(cmd) == 0) && fp.open(file)) {
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

	remove(file);

	ofp.close();
}

sint_t findcard(const AString& pattern)
{
	AStdFile fp;
	AString pat = ParseRegex(pattern);
	sint_t card = -1;

	if (fp.open(ADVBConfig::Get().GetDVBCardsFile())) {
		AString line;
				
		while (line.ReadLn(fp) >= 0) {
			if (MatchRegex(line.Words(1), pat)) {
				card = (uint_t)line.Word(0);
				break;
			}
		}

		fp.close();
	}

	return card;
}
