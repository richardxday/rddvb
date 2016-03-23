
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <map>

#include <rdlib/DateTime.h>
#include <rdlib/QuitHandler.h>
#include <rdlib/SettingsHandler.h>

#include "config.h"

int main(int argc, char *argv[])
{
	AQuitHandler quithandler;
	uint_t card = 0;
	int i;

	for (i = 1; i < argc; i++) {
		if (stricmp(argv[i], "--card") == 0) card = (uint_t)AString(argv[++i]);
	}

	ASettingsHandler handler(AString("dvbfestats-%").Arg(card), true, 30000);
	AString line;
	while (!HasQuit() && (line.ReadLn(Stdin) > 0)) {
		bool waslocked = handler.Exists("FE_HAS_LOCK");
		
		if (line.EndsWith("FE_HAS_LOCK")) {
			uint_t i, n = line.CountLines("|");

			for (i = 0; i < n; i++) {
				AString column = line.Line(i, "|").Words(0);
				AString name   = column.Word(0).ToLower();
				AString value  = column.Word(1);

				if ((i >= 1) && (i < 4)) value = (uint_t)("$" + value);
				
				handler.Set(name, value);
			}
		}
		else handler.Delete("FE_HAS_LOCK");

		bool islocked = handler.Exists("FE_HAS_LOCK");

		if (islocked != waslocked) {
			printf("%s: %s\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s.%S").str(), islocked ? "Locked" : "Unlocked");
		}
		
		handler.CheckWrite();
	}
	
	return 0;
}
