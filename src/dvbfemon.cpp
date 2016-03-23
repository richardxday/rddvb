
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
	const ADVBConfig& config = ADVBConfig::Get();
	AQuitHandler quithandler;
	uint_t card = 0;
	int i;

	for (i = 1; i < argc; i++) {
		if (stricmp(argv[i], "--card") == 0) card = (uint_t)AString(argv[++i]);
	}

	AString line;
	std::map<AString,AString> values;
	std::map<AString,AString>::iterator it;


	ASettingsHandler stats(AString("dvbcardstats-%").Arg(card), true, 60000);
	uint64_t locktime   = (uint64_t)stats.Get("locktime");
	uint64_t unlocktime = (uint64_t)stats.Get("unlocktime");
	uint_t   minsignal  = (uint)stats.Get("minsignal");
	uint_t   maxsignal  = (uint)stats.Get("maxsignal");
	uint_t   minsnr  	= (uint)stats.Get("minsnr");
	uint_t   maxsnr  	= (uint)stats.Get("maxsnr");

	values["lock"]      = stats.Get("lockstatus", "unlocked");

	while (!HasQuit() && (line.ReadLn(Stdin) > 0)) {
		uint_t i, n = line.CountLines("|");
		bool waslocked = (((it = values.find("lock")) != values.end()) && (it->second == "locked"));
		
		values["lock"] = "unlocked";
		
		for (i = 0; i < n; i++) {
			AString column = line.Line(i, "|").Words(0);
			AString name   = column.Word(0).ToLower();
			AString value  = column.Word(1);

			if ((name == "signal") || (name == "snr") || (name == "ber") || (name == "unc")) value = (uint_t)("$" + value);
			if (name == "fe_has_lock") {
				name  = "lock";
				value = "locked";
			}
			
			values[name] = value;
		}

		stats.Set("lockstatus", values["lock"]);
		
		bool locked = (((it = values.find("lock")) != values.end()) && (it->second == "locked"));
		if (locked != waslocked) {
			uint64_t t = (uint64_t)ADateTime().TimeStamp(true);
			
			if (locked) {
				locktime = t;
				config.printf("Card %u LOCKED (unlocked for %ss)", card, AValue((t - unlocktime) / 1000).ToString().str());
			}
			else {
				unlocktime = t;
				config.printf("Card %u UNLOCKED (locked for %ss): signal %u-%u snr %u-%u",
							  card, AValue((t - locktime) / 1000).ToString().str(),
							  minsignal, maxsignal,
							  minsnr,    maxsnr);
			}
		}

		if (locked) {
			uint_t signal = (uint_t)values["signal"];
			uint_t snr    = (uint_t)values["snr"];

			if (!waslocked && locked) {
				minsignal = maxsignal = signal;
				minsnr    = maxsnr    = snr;
			}
			else {
				minsignal = MIN(minsignal, signal);
				maxsignal = MAX(maxsignal, signal);
				minsnr 	  = MIN(minsnr,    snr);
				maxsnr 	  = MAX(maxsnr,    snr);
			}

			stats.Set("minsignal", minsignal);
			stats.Set("maxsignal", maxsignal);
			stats.Set("minsnr",    minsnr);
			stats.Set("maxsnr",    maxsnr);
		}

		stats.Write();
	}
	
	return 0;
}
