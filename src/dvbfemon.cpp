
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <map>

#include <rdlib/DateTime.h>
#include <rdlib/QuitHandler.h>
#include <rdlib/SettingsHandler.h>

#include "config.h"

typedef struct {
	AString  name;
	uint_t   min, max;
	uint64_t sum;
	uint_t   count;
} STATS;

STATS readstats(ASettingsHandler& statsstorage, const AString& name)
{
	STATS stats;

	stats.name  = name;
	stats.min 	= statsstorage.Get(stats.name + ":min");
	stats.max 	= statsstorage.Get(stats.name + ":max");
	stats.sum 	= statsstorage.Get(stats.name + ":sum");
	stats.count = statsstorage.Get(stats.name + ":count");

	return stats;
}

void updatestats(STATS& stats, uint_t value, bool justlocked)
{
	if (!stats.count || justlocked) {
		stats.min = stats.max = value;
		stats.sum = value;
		stats.count = 1;
	}
	else {
		stats.min = MIN(stats.min, value);
		stats.max = MAX(stats.max, value);
		stats.sum += value;
		stats.count++;
	}
}

void writestats(ASettingsHandler& statsstorage, const STATS& stats)
{
	statsstorage.Set(stats.name + ":min",   stats.min);
	statsstorage.Set(stats.name + ":max",   stats.max);
	statsstorage.Set(stats.name + ":sum",   stats.sum);
	statsstorage.Set(stats.name + ":count", stats.count);
}

void logsignalstats(uint_t card, const STATS *stats, uint_t n)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString  msg;
	AStdFile fp;
	uint_t i;

	msg.printf("%s %u", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s.%S").str(), card);

	for (i = 0; i < n; i++) {
		const STATS& stats1 = stats[i];

		msg.printf(" %s %u %u",
				   stats1.name.str(),
				   stats1.min, stats1.max);

		if (stats1.count) {
			msg.printf(" %s",
					   AValue((stats1.sum + stats1.count / 2) / stats1.count).ToString().str());
		}
		else msg.printf(" -");
	}

	if (fp.open(config.GetDVBSignalLog(ADateTime().GetDays()), "a")) {
		fp.printf("%s\n", msg.str());
		fp.close();
	}
}

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

	ASettingsHandler statsstorage(AString("dvbcardstats-%").Arg(card), true, 60000);
	uint64_t locktime   = (uint64_t)statsstorage.Get("locktime");
	uint64_t unlocktime = (uint64_t)statsstorage.Get("unlocktime");
	STATS    stats[] = {
		readstats(statsstorage, "signal"),
		readstats(statsstorage, "snr"),
	};

	values["lock"]      = statsstorage.Get("lockstatus", "unlocked");

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

		statsstorage.Set("lockstatus", values["lock"]);

		bool locked = (((it = values.find("lock")) != values.end()) && (it->second == "locked"));
		if (locked != waslocked) {
			uint64_t t = (uint64_t)ADateTime().TimeStamp(true);

			if (locked) {
				locktime = t;
				config.printf("Card %u LOCKED (unlocked for %ss)", card, AValue((t - unlocktime) / 1000).ToString().str());
			}
			else {
				AString msg;

				unlocktime = t;
				msg.printf("Card %u UNLOCKED (locked for %ss):",
						   card, AValue((t - locktime) / 1000).ToString().str());

				for (i = 0; i < NUMBEROF(stats); i++) {
					const STATS& stats1 = stats[i];

					if (i) msg.printf(",");

					msg.printf(" %s {%u, %u}",
							   stats1.name.str(),
							   stats1.min, stats1.max);

					if (stats1.count) {
						msg.printf(" avg %s",
								   AValue((stats1.sum + stats1.count / 2) / stats1.count).ToString().str());
					}
				}

				config.printf("%s", msg.str());

				logsignalstats(card, stats, NUMBEROF(stats));
			}
		}

		if (locked) {
			bool justlocked = (!waslocked && locked);
			bool log = false;

			for (i = 0; i < NUMBEROF(stats); i++) {
				STATS& stats1 = stats[i];

				updatestats(stats1, values[stats1.name], justlocked);
				writestats(statsstorage, stats1);

				log |= (stats1.count >= 60);
			}

			if (log) {
				logsignalstats(card, stats, NUMBEROF(stats));
				for (i = 0; i < NUMBEROF(stats); i++) {
					stats[i].sum = 0;
					stats[i].count = 0;
				}
			}
		}

		statsstorage.Write();
	}

	return 0;
}
