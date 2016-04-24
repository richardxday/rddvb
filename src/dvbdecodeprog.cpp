
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "dvbprog.h"
#include "dvbmisc.h"
#include "proglist.h"

int main(int argc, char *argv[])
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBProg prog;	// ensure ADVBProg initialisation takes place
	
	if (argc > 1) {
		if (prog.Base64Decode(argv[1])) {
			printf("{%s", prog.ExportToJSON().str());

			const ADVBProg::EPISODE& episode = prog.GetEpisode();
			if (episode.valid) {
				ADVBProgList reclist, list;
				AString		 pattern, errors;

				pattern.printf("title=\"%s\" available", prog.GetTitle());
				reclist.ReadFromFile(config.GetRecordedFile());
				reclist.FindProgrammes(list, pattern, errors);

				list.Sort(&ADVBProgList::CompareEpisode);

				int p;
				if ((p = list.FindUUIDIndex(prog)) >= 0) {
					if (p > 0) {
						const ADVBProg& prog1 = list.GetProg(p - 1);

						printf(",\"previous\":{\"prog\":\"%s\",\"desc\":\"%s\"}", JSONFormat(prog1.Base64Encode()).str(), JSONFormat(prog1.GetTitleAndSubtitle()).str());
					}
					if (p < (int)(list.Count() - 1)) {
						const ADVBProg& prog1 = list.GetProg(p + 1);

						printf(",\"next\":{\"prog\":\"%s\",\"desc\":\"%s\"}", JSONFormat(prog1.Base64Encode()).str(), JSONFormat(prog1.GetTitleAndSubtitle()).str());
					}
				}
			}

			printf("}");
		}
		else printf("{\"error\": \"Unable to decode Base64 programme\"}");
	}
	else printf("Usage: dvbdecodeprog <base64-programme>\n");
	
	return 0;
}
