
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <map>
#include <vector>
#include <algorithm>

#include <rdlib/Regex.h>
#include <rdlib/Recurse.h>
#include <rdlib/simpleeval.h>

#include "config.h"
#include "dvblock.h"
#include "proglist.h"
#include "channellist.h"
#include "findcards.h"
#include "dvbdatabase.h"

#define EVALTEST 0

typedef struct {
	AString  filename;
	uint64_t length;
	uint64_t filesize;
} RECORDDETAILS;

bool forcelogging = false;

static bool __DisplaySeries(const AString& key, uptr_t value, void *context)
{
	UNUSED(context);

	if (value) {
		const ADVBProgList::SERIES& series = *(const ADVBProgList::SERIES *)value;
		const ADataList& serieslist = series.list;
		uint_t j;

		for (j = 0; j < serieslist.Count(); j++) {
			const AString *str = (const AString *)serieslist[j];

			if (str) printf("Programme '%s' series %u: %s\n", key.str(), j, str->str());
		}
	}

	return true;
}

#if EVALTEST
static AValue __func(const AString& funcname, const simplevars_t& vars, const simpleargs_t& values, AString& errors, void *context)
{
	AValue res = 0;
	size_t i;
	
	UNUSED(funcname);
	UNUSED(vars);
	UNUSED(values);
	UNUSED(errors);
	UNUSED(context);

	for (i = 0; i < values.size(); i++) {
		res += values[i];
	}
	
	return res;
}
#endif

int main(int argc, char *argv[])
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBProgList proglist;
	ADVBProg  prog;	// ensure ADVBProg initialisation takes place
	ADateTime starttime = ADateTime().TimeStamp(true);
	uint_t dvbcard   = 0;
	uint_t verbosity = 0;
	int  i;
	int  res = 0;

	(void)prog;

	if ((argc == 1) || ((argc > 1) && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0)))) {
		printf("Usage: dvb {<file>|<cmd> ...}\n");
		printf("Where <cmd> is:\n");
		printf("\t-v[<n>]\t\t\t\tIncrease verbosity by <n> or by 1\n");
		printf("\t--confdir\t\t\tPrint conf directory\n");
		printf("\t--datadir\t\t\tPrint data directory\n");
		printf("\t--logdir\t\t\tPrint log directory\n");
		printf("\t--dirs\t\t\t\tPrint important configuration directories\n");
		printf("\t--getxmltv <outputfile>\t\tDownload XMLTV listings\n");
		printf("\t--dayformat <format>\t\tFormat string for day (default '%s')\n", ADVBProg::GetDayFormat().str());
		printf("\t--dateformat <format>\t\tFormat string for date (default '%s')\n", ADVBProg::GetDateFormat().str());
		printf("\t--timeformat <format>\t\tFormat string for time (default '%s')\n", ADVBProg::GetTimeFormat().str());
		printf("\t--fulltimeformat <format>\tFormat string for fulltime (default '%s')\n", ADVBProg::GetFullTimeFormat().str());
		printf("\t--set <var>=<val>\t\tSet variable in configuration file for this session only\n");
		printf("\t--update <file>\t\t\tUpdate main listings with file <file> (-u)\n");
		printf("\t--load\t\t\t\tRead listings from default file (-l)\n");
		printf("\t--read <file>\t\t\tRead listings from file <file> (-r)\n");
		printf("\t--merge <file>\t\t\tMerge listings from file <file>, adding any programmes that dot not exist\n");
		printf("\t--modify-recorded <file>\tMerge listings from file <file> into recorded programmes, adding any programmes that dot not exist\n");
		printf("\t--modify-recorded-from-recording-host Add recorded programmes from recording host into recorded programmes, adding any programmes that dot not exist\n");
		printf("\t--modify-scheduled-from-recording-host Modfy scheduled programmes from recording host into scheduled programmes\n");
		printf("\t--jobs\t\t\t\tRead programmes from scheduled jobs\n");
		printf("\t--write <file>\t\t\tWrite listings to file <file> (-w)\n");
		printf("\t--sort\t\t\t\tSort list in chronological order\n");
		printf("\t--sort-rev\t\t\tSort list in reverse-chronological order\n");
		printf("\t--write-text <filename>\t\tWrite listings to file <filename> in text format\n");
		printf("\t--write-gnuplot <filename>\tWrite listings to file <filename> in format usable by GNUPlot\n");
		printf("\t--fix-pound <file>\t\tFix pound symbols in file\n");
		printf("\t--update-dvb-channels\t\tUpdate DVB channel assignments\n");
		printf("\t--update-uuid\t\t\tSet UUID's on every programme\n");
		printf("\t--update-combined\t\tCreate a combined list of recorded and scheduled programmes\n");
		printf("\t--list\t\t\t\tList programmes in current list (-L)\n");
		printf("\t--list-to-file <file>\t\tList programmes in current list (-L) to file <file>\n");
		printf("\t--list-base64\t\t\tList programmes in current list in base64 format\n");
		printf("\t--list-channels\t\t\tList channels\n");
		printf("\t--cut-head <n>\t\t\tCut <n> programmes from the start of the list\n");
		printf("\t--cut-tail <n>\t\t\tCut <n> programmes from the end of the list\n");
		printf("\t--head <n>\t\t\tKeep <n> programmes at the start of the list\n");
		printf("\t--tail <n>\t\t\tKeep <n> programmes at the end of the list\n");
		printf("\t--limit <n>\t\t\tLimit list to <n> programmes, deleting programmes from the end of the list\n");
		printf("\t--find <patterns>\t\tFind programmes matching <patterns> (-f)\n");
		printf("\t--find-with-file <pattern-file>\tFind programmes matching patterns in patterns file <pattern-file> (-F)\n");
		printf("\t--find-repeats\t\t\tFor each programme in current list, list repeats (-R)\n");
		printf("\t--find-similar <file>\t\tFor each programme in current list, find first similar programme in <file>\n");
		printf("\t--find-differences <file1> <file2>\tFind programmes in <file1> but not in <file2> or in <file2> and not in <file1>\n");
		printf("\t--find-in-file1-only <file1> <file2>\tFind programmes in <file1> but not in <file2>\n");
		printf("\t--find-in-file2-only <file1> <file2>\tFind programmes in <file2> and not in <file1>\n");
		printf("\t--find-similarities <file1> <file2>\tFind programmes in both <file1> and <file2>\n");
		printf("\t--describe-pattern <patterns>\tDescribe patterns as parsed\n");
		printf("\t--delete-all\t\t\tDelete all programmes\n");
		printf("\t--delete <patterns>\t\tDelete programmes matching <patterns>\n");
		printf("\t--delete-with-file <pattern-file> Delete programmes matching patterns in patterns file <pattern-file>\n");
		printf("\t--delete-recorded\t\tDelete programmes that have been recorded\n");
		printf("\t--delete-using-file <file>\tDelete programmes that are similar to those in file <file>\n");
		printf("\t--delete-similar\t\tDelete programmes that are similar to others in the list\n");
		printf("\t--schedule\t\t\tSchedule and create jobs for default set of patterns on the main listings file\n");
		printf("\t--write-scheduled-jobs\t\tCreate jobs for current scheduled list\n");
		printf("\t--start-time <time>\t\tSet start time for scheduling\n");
		printf("\t--fake-schedule\t\t\tPretend to schedule (write files) for default set of patterns on the main listings file\n");
		printf("\t--fake-schedule-list\t\tPretend to schedule (write files) for current list of programmes\n");
		printf("\t--unschedule-all\t\tUnschedule all programmes\n");
		printf("\t--schedule-list\t\t\tSchedule current list of programmes (-S)\n");
		printf("\t--record <prog>\t\t\tRecord programme <prog> (Base64 encoded)\n");
		printf("\t--record-title <title>\t\tSchedule to record programme by title that has already started or starts within the next hour\n");
		printf("\t--schedule-record <base64>\t\tSchedule specified programme (Base64 encoded)\n");
		printf("\t--add-recorded <prog>\t\tAdd recorded programme <prog> to recorded list (Base64 encoded)\n");
		printf("\t--card <card>\t\t\tSet VIRTUAL card for subsequent scanning and PID operations (default 0)\n");
		printf("\t--dvbcard <card>\t\tSet PHYSICAL card for subsequent scanning and PID operations (default 0)\n");
		printf("\t--pids <channel>\t\tFind PIDs (all streams) associated with channel <channel>\n");
		printf("\t--scan <freq>[,<freq>...]\tScan frequencies <freq>MHz for DVB channels\n");
		printf("\t--scan-all\t\t\tScan all known frequencies for DVB channels\n");
		printf("\t--scan-range <start-freq> <end-freq> <step>\n\t\t\t\t\tScan frequencies <freq>MHz for DVB channels\n");
		printf("\t--find-cards\t\t\tFind DVB cards\n");
		printf("\t--change-filename <filename1> <filename2>\n\t\t\t\t\tChange filename of recorded progamme with filename <filename1> to <filename2>\n");
		printf("\t--change-filename-regex <filename1> <filename2>\n\t\t\t\t\tChange filename of recorded progamme with filename <filename1> to <filename2> (where <filename1> can be a regex and <filename2> can be an expansion)\n");
		printf("\t--change-filename-regex-test <filename1> <filename2>\n\t\t\t\t\tLike above but do not commit changes\n");
		printf("\t--find-recorded-programmes-on-disk\n\t\t\t\t\tSearch for recorded programmes in 'searchdirs' and update any filenames\n");
		printf("\t--find-series\t\t\tFind series' in programme list\n");
		printf("\t--fix-data-in-recorded\t\tFix incorrect metadata of certain programmes and rename files as necessary\n");
		printf("\t--set-recorded-flag\t\tSet recorded flag on all programmes in recorded list\n");
		printf("\t--set-ignore-flag <patterns>\tSet ignore recording flag on programmes matching pattern\n");
		printf("\t--clr-ignore-flag <patterns>\tClear ignore recording flag on programmes matching pattern\n");
		printf("\t--check-disk-space\t\tCheck disk space for all patterns\n");
		printf("\t--update-recording-complete\tUpdate recording complete flag in every recorded programme\n");
		printf("\t--check-recording-file\t\tCheck programmes in running list to ensure they should remain in there\n");
		printf("\t--force-failures\t\tAdd current list to recording failures (setting failed flag)\n");
		printf("\t--show-encoding-args\t\tShow encoding arguments for programmes in current list\n");
		printf("\t--show-file-format\t\tShow file format of encoded programme\n");
		printf("\t--use-converted-filename\tChange filename to that of converted file, without actually converting file\n");
		printf("\t--set-dir <patterns> <newdir>\tChange directory for programmes in current list that match <patterns> to <newdir>\n");
		printf("\t--regenerate-filename\t\tChange filename of current list of programmes (dependant on converted state), renaming files (in original directory OR current directory)\n");
		printf("\t--regenerate-filename-test\tTest version of the above, will make no changes to programme list or move files\n");
		printf("\t--delete-files\t\t\tDelete encoded files from programmes in current list\n");
		printf("\t--delete-from-record-lists <uuid> Delete programme with UUID <uuid> from record lists (recorded and record failues)\n");
		printf("\t--record-success\t\tRun recordsuccess command on programmes in current list\n");
		printf("\t--change-user <patterns> <newuser>\n\t\t\t\t\tChange user of programmes matching <patterns> to <newuser>\n");
		printf("\t--get-and-convert-recorded\tPull and convert any recordings from recording host\n");
		printf("\t--force-convert-files\t\tconvert files in current list, even if they have already been converted\n");
		printf("\t--update-recordings-list\tPull list of programmes being recorded\n");
		printf("\t--check-recording-now\t\tCheck to see if programmes that should be being recording are recording\n");
		printf("\t--calc-trend <start-date>\tCalculate average programmes per day and trend line based on programmes after <start-date> in current list\n");
		printf("\t--gen-graphs\t\t\tGenerate graphs from recorded data\n");
		printf("\t--assign-episodes\t\tAssign episodes to current list\n");
		printf("\t--reset-assigned-episodes\tReset assigned episodes to current list\n");
		printf("\t--count-hours\t\t\tCount total hours of programmes in current list\n");
		printf("\t--find-gaps\t\t\tFind gap from now until the next working for each card\n");
		printf("\t--stream <channel>\t\tStream DVB channel <channel> to mplayer (or other player)\n");
		printf("\t--return-count\t\t\tReturn programme list count in error code\n");
	}
	else {
		for (i = 1; (i < argc) && !HasQuit(); i++) {
			if (strncmp(argv[i], "-v", 2) == 0) {
				uint_t inc = (uint_t)AString(argv[i] + 2);
				verbosity += inc ? inc : 1;
			}
			else if (strcmp(argv[i], "--confdir") == 0) printf("%s\n", config.GetConfigDir().str());
			else if (strcmp(argv[i], "--datadir") == 0) printf("%s\n", config.GetDataDir().str());
			else if (strcmp(argv[i], "--logdir") == 0)  printf("%s\n", config.GetLogDir().str());
			else if (strcmp(argv[i], "--dirs") == 0) {
				AList users;

				config.ListUsers(users);

				printf("Configuration: %s\n", config.GetConfigDir().str());
				printf("Data: %s\n", config.GetDataDir().str());
				printf("Log files: %s\n", config.GetLogDir().str());
				printf("Recordings: %s\n", config.GetRecordingsDir().str());

				const AString *user = AString::Cast(users.First());
				while (user) {
					printf("Recordings for %s: %s\n", user->str(), config.GetRecordingsDir(*user).str());
					user = user->Next();
				}

				printf("Recordings (Temp): %s\n", config.GetRecordingsStorageDir().str());
				printf("Recordings (Archive): %s\n", config.GetRecordingsArchiveDir().str());
				printf("Temp: %s\n", config.GetTempDir().str());
			}
			else if (strcmp(argv[i], "--getxmltv") == 0) {
				AString destfile = argv[++i];
				AString cmd = config.GetXMLTVDownloadCommand() + " " + config.GetXMLTVDownloadArguments(destfile);

				config.logit("Downloading XMLTV listings using '%s'", cmd.str());
				if ((res = system(cmd.str())) != 0) {
					config.logit("Downloading failed!");
				}
			}
			else if (strcmp(argv[i], "--dayformat") == 0) ADVBProg::SetDayFormat(argv[++i]);
			else if (strcmp(argv[i], "--dateformat") == 0) ADVBProg::SetDateFormat(argv[++i]);
			else if (strcmp(argv[i], "--timeformat") == 0) ADVBProg::SetTimeFormat(argv[++i]);
			else if (strcmp(argv[i], "--fulltimeformat") == 0) ADVBProg::SetFullTimeFormat(argv[++i]);
			else if (strcmp(argv[i], "--set") == 0) {
				AString str = argv[++i];
				int p;

				if ((p = str.Pos("=")) >= 0) {
					AString var = str.Left(p);
					AString val = str.Mid(p + 1).DeQuotify().DeEscapify();
					config.Set(var, val);
				}
				else config.printf("Configuration setting '%s' invalid, format should be '<var>=<val>'", str.str());
			}
			else if ((strcmp(argv[i], "--update") == 0) || (strcmp(argv[i], "-u") == 0) || (AString(argv[i]).Suffix() == "xmltv") || (AString(argv[i]).Suffix() == "txt")) {
				AString filename = config.GetListingsFile();
				AString updatefile;

				if ((strcmp(argv[i], "--update") == 0) || (strcmp(argv[i], "-u") == 0)) i++;

				config.printf("Reading main listings file...");
				proglist.DeleteAll();
				proglist.ReadFromFile(filename);
				config.printf("Read programmes from '%s', total now %u", filename.str(), proglist.Count());

				updatefile = argv[i];

				config.printf("Updating from new file...");
				if (proglist.ReadFromFile(updatefile)) {
					config.printf("Read programmes from '%s', total now %u", updatefile.str(), proglist.Count());

					ADateTime dt(ADateTime().TimeStamp(true).GetDays() - config.GetDaysToKeep(), 0);
					config.printf("Removing old programmes (before %s)...", dt.UTCToLocal().DateToStr().str());
					proglist.DeleteProgrammesBefore(dt);

					config.printf("Updating DVB channels...");
					proglist.UpdateDVBChannels();

					if (config.AssignEpisodes()) {
						config.printf("Assigning episode numbers where necessary...");
						proglist.AssignEpisodes();
					}

					config.printf("Writing main listings file...");
					if (!HasQuit() && proglist.WriteToFile(filename)) {
						config.printf("Wrote %u programmes to '%s'", proglist.Count(), filename.str());
					}
					else {
						config.printf("Failed to write programme list to '%s'", filename.str());
					}
				}
				else {
					config.printf("Failed to read programme list from '%s'", filename.str());
				}
			}
			else if ((strcmp(argv[i], "--load") == 0) || (strcmp(argv[i], "-l") == 0)) {
				AString filename = config.GetListingsFile();

				printf("Reading main listings file...\n");
				if (proglist.ReadFromFile(filename)) {
					printf("Read programmes from '%s', total now %u\n", filename.str(), proglist.Count());
				}
				else printf("Failed to read programme list from '%s'\n", filename.str());
			}
			else if ((strcmp(argv[i], "--read") == 0) || (strcmp(argv[i], "-r") == 0)) {
				AString filename = config.GetNamedFile(argv[++i]);

				printf("Reading programmes...\n");
				if (proglist.ReadFromFile(filename)) {
					printf("Read programmes from '%s', total now %u\n", filename.str(), proglist.Count());
				}
				else printf("Failed to read programme list from '%s'\n", filename.str());
			}
			else if (strcmp(argv[i], "--merge") == 0) {
				AString      filename = argv[++i];
				ADVBProgList list;

				if (list.ReadFromFile(filename)) {
					uint_t added, modified;

					proglist.Modify(list, added, modified, ADVBProgList::Prog_Add);

					printf("Merged programmes from '%s', total now %u (%u added)\n", filename.str(), proglist.Count(), added);
				}
				else printf("Failed to read programme list from '%s'\n", filename.str());
			}
			else if (strcmp(argv[i], "--modify-recorded") == 0) {
				ADVBLock 	 lock("dvbfiles");
				AString  	 filename = argv[++i];
				ADVBProgList reclist, list;

				if (reclist.ReadFromFile(config.GetRecordedFile())) {
					if (list.ReadFromFile(filename)) {
						uint_t added, modified;

						reclist.Modify(list, added, modified, ADVBProgList::Prog_Add);

						if (added) {
							if (!HasQuit() && reclist.WriteToFile(config.GetRecordedFile())) {
								config.printf("Merged programmes from '%s' into recorded list, total now %u (%u added)", filename.str(), reclist.Count(), added);
							}
							else config.printf("Failed to write recorded programme list back!");
						}
						else config.printf("No programmes added");
					}
					else config.printf("Failed to read programme list from '%s'", filename.str());
				}
				else config.printf("Failed to read recorded programmes from '%s'", config.GetRecordedFile().str());
			}
			else if (strcmp(argv[i], "--modify-recorded-from-recording-host") == 0) {
				ADVBProgList list;
				if (!list.ModifyFromRecordingSlave(config.GetRecordedFile(), ADVBProgList::Prog_Add)) res = -1;
			}
			else if (strcmp(argv[i], "--modify-scheduled-from-recording-host") == 0) {
				ADVBProgList list;
				if (!list.ModifyFromRecordingSlave(config.GetScheduledFile(), ADVBProgList::Prog_ModifyAndAdd)) res = -1;
			}
			else if (strcmp(argv[i], "--jobs") == 0) {
				printf("Reading programmes from job queue...\n");
				if (proglist.ReadFromJobList()) {
					printf("Read programmes from job queue, total now %u\n", proglist.Count());
				}
				else {
					printf("Failed to read programme list from job queue\n");
				}
			}
			else if (strcmp(argv[i], "--fix-pound") == 0) {
				AString filename = config.GetNamedFile(argv[++i]);

				proglist.DeleteAll();

				if (proglist.ReadFromFile(filename)) {
					printf("Read programmes from '%s', total %u\n", filename.str(), proglist.Count());
					printf("Fixing pound symbols...\n");

					proglist.SearchAndReplace("\xa3", "£");

					if (!HasQuit() && proglist.WriteToFile(filename)) {
						printf("Wrote programmes to '%s'\n", filename.str());
					}
				}
				else {
					printf("Failed to read programme list from '%s'\n", filename.str());
				}
			}
			else if (strcmp(argv[i], "--update-dvb-channels") == 0) {
				config.printf("Updating DVB channels...");
				proglist.UpdateDVBChannels();
			}
			else if (strcmp(argv[i], "--update-uuid") == 0) {
				uint_t j;

				for (j = 0; j < proglist.Count(); j++) {
					proglist.GetProgWritable(j).SetUUID();
				}
			}
			else if (strcmp(argv[i], "--update-combined") == 0) {
				ADVBProgList::CreateCombinedFile();
			}
			else if ((strcmp(argv[i], "--find") == 0) || (strcmp(argv[i], "-f") == 0)) {
				ADVBProgList reslist;
				AString      patterns = argv[++i], errors;

				printf("Finding programmes...\n");

				proglist.FindProgrammes(reslist, patterns, errors, (patterns.Pos("\n") >= 0) ? "\n" : ";");

				proglist = reslist;

				if (errors.Valid()) {
					printf("Errors:\n");

					uint_t j, n = errors.CountLines();
					for (j = 0; j < n; j++) printf("%s\n", errors.Line(j).str());
				}

				printf("Found %u programme%s\n", reslist.Count(), (reslist.Count() == 1) ? "" : "s");
			}
			else if ((strcmp(argv[i], "--find-repeats") == 0) || (strcmp(argv[i], "-R") == 0)) {
				uint_t j;

				for (j = 0; j < proglist.Count(); j++) {
					ADVBProgList reslist;
					uint_t n;

					if ((n = proglist.FindSimilarProgrammes(reslist, proglist.GetProg(j))) != 0) {
						uint_t k;

						printf("%s: %u repeats found:\n", proglist[j].GetDescription(verbosity).str(), n);

						for (k = 0; k < reslist.Count(); k++) {
							const ADVBProg& prog = reslist[k];
							AString desc = prog.GetDescription(verbosity);
							uint_t l, nl = desc.CountLines();

							for (l = 0; l < nl; l++) {
								printf("\t%s\n", desc.Line(l).str());
							}

							proglist.DeleteProg(prog);
						}

						printf("\n");
					}
					else printf("%s: NO repeats found\n", proglist[j].GetDescription(verbosity).str());
				}
			}
			else if (strcmp(argv[i], "--find-similar") == 0) {
				ADVBProgList otherlist;

				if (otherlist.ReadFromFile(argv[++i])) {
					uint_t j;

					printf("Loading %u programmes into test list\n", otherlist.Count());

					for (j = 0; j < proglist.Count(); j++) {
						const ADVBProg *prog;

						if ((prog = otherlist.FindSimilar(proglist[j])) != NULL) {
							AString desc = prog->GetDescription(verbosity);
							uint_t k, nl = desc.CountLines();

							printf("Similar to %s:\n", proglist[j].GetDescription(verbosity).str());

							for (k = 0; k < nl; k++) {
								printf("\t%s\n", desc.Line(k).str());
							}
						}
						else printf("Nothing similar to %s\n", proglist[j].GetDescription(verbosity).str());
					}
				}
				else printf("Failed to read programmes from '%s'\n", argv[i]);
			}
			else if ((strcmp(argv[i], "--find-differences")   == 0) ||
					 (strcmp(argv[i], "--find-in-file1-only") == 0) ||
					 (strcmp(argv[i], "--find-in-file2-only") == 0) ||
					 (strcmp(argv[i], "--find-similarities")  == 0)) {
				const bool similarities = (strcmp(argv[i], "--find-similarities") == 0);
				const bool in1only = ((strcmp(argv[i], "--find-differences") == 0) || (strcmp(argv[i], "--find-in-file1-only") == 0));
				const bool in2only = ((strcmp(argv[i], "--find-differences") == 0) || (strcmp(argv[i], "--find-in-file2-only") == 0));
				ADVBProgList file1list, file2list;
				AString file1name = argv[++i];
				AString file2name = argv[++i];
				bool success = true;
				
				if (!file1list.ReadFromFile(file1name)) {
					printf("Failed to read programmes fomr '%s'\n", file1name.str());
					success = false;
				}
				if (!file2list.ReadFromFile(file2name)) {
					printf("Failed to read programmes fomr '%s'\n", file2name.str());
					success = false;
				}

				if (success) {
					proglist.DeleteAll();

					if (similarities) {
						proglist.FindSimilarities(file1list, file2list);
					}
					else {
						proglist.FindDifferences(file1list, file2list, in1only, in2only);
					}
					
					printf("Found %u programme%s\n", proglist.Count(), (proglist.Count() == 1) ? "" : "s");
				}
			}
			else if (strcmp(argv[i], "--describe-pattern") == 0) {
				ADataList patternlist;
				AString   patterns = argv[++i];
				AString   errors;
				uint_t    i;

				ADVBPatterns::ParsePatterns(patternlist, patterns, errors, ';');

				for (i = 0; i < patternlist.Count(); i++) {
					const ADVBPatterns::PATTERN& pattern = *(const ADVBPatterns::PATTERN *)patternlist[i];
					printf("%s", ADVBPatterns::ToString(pattern).str());
				}
			}
			else if ((strcmp(argv[i], "--find-with-file") == 0) || (strcmp(argv[i], "-F") == 0)) {
				ADVBProgList reslist;
				AString      patterns, errors;
				AString      filename = argv[++i];

				if (patterns.ReadFromFile(filename)) {
					printf("Finding programmes...\n");

					proglist.FindProgrammes(reslist, patterns, errors, (patterns.Pos("\n") >= 0) ? "\n" : ";");

					proglist = reslist;

					if (errors.Valid()) {
						printf("Errors:\n");

						uint_t j, n = errors.CountLines();
						for (j = 0; j < n; j++) printf("%s", errors.Line(j).str());
					}

					printf("Found %u programme%s\n", reslist.Count(), (reslist.Count() == 1) ? "" : "s");
				}
				else printf("Failed to read patterns from '%s'", filename.str());
			}
			else if (strcmp(argv[i], "--delete-all") == 0) {
				proglist.DeleteAll();
			}
			else if (strcmp(argv[i], "--delete") == 0) {
				ADVBProgList reslist;
				AString      patterns = argv[++i], errors;
				uint_t       j;

				proglist.FindProgrammes(reslist, patterns, errors, (patterns.Pos("\n") >= 0) ? "\n" : ";");

				printf("Found %u programme%s\n", reslist.Count(), (reslist.Count() == 1) ? "" : "s");

				if (errors.Valid()) {
					printf("Errors:\n");

					uint_t j, n = errors.CountLines();
					for (j = 0; j < n; j++) printf("%s", errors.Line(j).str());
				}

				for (j = 0; j < reslist.Count(); j++) {
					const ADVBProg& prog = reslist.GetProg(j);

					if (proglist.DeleteProg(prog)) {
						printf("Deleted '%s'\n", prog.GetDescription(verbosity).str());
					}
					else printf("Failed to delete '%s'!\n", prog.GetDescription(verbosity).str());
				}
			}
			else if (strcmp(argv[i], "--delete-with-file") == 0) {
				ADVBProgList reslist;
				AString      patterns, errors;
				AString      filename = argv[++i];
				uint_t       j;

				if (patterns.ReadFromFile(filename)) {
					proglist.FindProgrammes(reslist, patterns, errors, (patterns.Pos("\n") >= 0) ? "\n" : ";");

					printf("Found %u programme%s\n", reslist.Count(), (reslist.Count() == 1) ? "" : "s");

					if (errors.Valid()) {
						printf("Errors:\n");

						uint_t j, n = errors.CountLines();
						for (j = 0; j < n; j++) printf("%s", errors.Line(j).str());
					}

					for (j = 0; j < reslist.Count(); j++) {
						const ADVBProg& prog = reslist.GetProg(j);

						if (proglist.DeleteProg(prog)) {
							printf("Deleted '%s'\n", prog.GetDescription(verbosity).str());
						}
						else printf("Failed to delete '%s'!\n", prog.GetDescription(verbosity).str());
					}
				}
				else printf("Failed to read patterns from '%s'\n", filename.str());
			}
			else if ((strcmp(argv[i], "--delete-recorded") == 0) ||
					 (strcmp(argv[i], "--delete-using-file") == 0)) {
				AString      filename = config.GetRecordedFile();
				ADVBProgList proglist2;

				if (strcmp(argv[i], "--delete-using-file") == 0) filename = config.GetNamedFile(argv[++i]);

				if (proglist2.ReadFromFile(filename)) {
					uint_t j, ndeleted = 0;

					for (j = 0; j < proglist.Count(); ) {
						if (proglist2.FindSimilar(proglist.GetProg(j))) {
							proglist.DeleteProg(j);
							ndeleted++;
						}
						else j++;
					}

					printf("Deleted %u programmes\n", ndeleted);
				}
				else printf("Failed to read file '%s'\n", filename.str());
			}
			else if (strcmp(argv[i], "--delete-similar") == 0) {
				uint_t j, ndeleted = 0;

				for (j = 0; j < proglist.Count(); j++) {
					const ADVBProg& prog = proglist.GetProg(j);
					const ADVBProg  *sprog;

					while ((sprog = proglist.FindSimilar(prog, &prog)) != NULL) {
						proglist.DeleteProg(*sprog);
						ndeleted++;
					}
				}

				printf("Deleted %u programmes\n", ndeleted);
			}
			else if ((strcmp(argv[i], "--list") == 0) || (strcmp(argv[i], "-L") == 0)) {
				uint_t j;

				if (verbosity > 1) printf("%s\n", AString("-").Copies(80).str());

				for (j = 0; j < proglist.Count(); j++) {
					printf("%s\n", proglist[j].GetDescription(verbosity).str());
				}
			}
			else if (strcmp(argv[i], "--list-to-file") == 0) {
				AStdFile fp;

				if (fp.open(argv[++i], "a")) {
					uint_t j;

					if (verbosity > 1) fp.printf("%s\n", AString("-").Copies(80).str());

					for (j = 0; j < proglist.Count(); j++) {
						fp.printf("%s\n", proglist[j].GetDescription(verbosity).str());
					}

					fp.close();
				}
				else printf("Failed to open file '%s' for append", argv[i]);
			}
			else if (strcmp(argv[i], "--list-base64") == 0) {
				uint_t j;

				for (j = 0; j < proglist.Count(); j++) {
					printf("%s\n", proglist[j].Base64Encode().str());
				}
			}
			else if (strcmp(argv[i], "--list-channels") == 0) {
				const ADVBChannelList& list = ADVBChannelList::Get();
				uint_t j, ndvb = 0;

				for (j = 0; j < proglist.ChannelCount(); j++) {
					const ADVBProgList::CHANNEL *channel = proglist.GetChannelIndex(j);
					AString dvbchannel = list.LookupDVBChannel(channel->name);

					printf("Channel %u/%u: '%s' (DVB '%s', XMLTV channel-id '%s')\n", j + 1, proglist.ChannelCount(), channel->name.str(), dvbchannel.str(), channel->id.str());

					ndvb += dvbchannel.Empty();
				}
				printf("%u/%u channels have NO DVB channel\n", ndvb, proglist.ChannelCount());
			}
			else if (strcmp(argv[i], "--cut-head") == 0) {
				uint_t n = (uint_t)AString(argv[++i]);
				uint_t j;

				for (j = 0; (j < n) && proglist.Count(); j++) {
					proglist.DeleteProg(0);
				}
			}
			else if (strcmp(argv[i], "--cut-tail") == 0) {
				uint_t n = (uint_t)AString(argv[++i]);
				uint_t j;

				for (j = 0; (j < n) && proglist.Count(); j++) {
					proglist.DeleteProg(proglist.Count() - 1);
				}
			}
			else if (strcmp(argv[i], "--head") == 0) {
				uint_t n = (uint_t)AString(argv[++i]);

				while (proglist.Count() > n) {
					proglist.DeleteProg(proglist.Count() - 1);
				}
			}
			else if (strcmp(argv[i], "--tail") == 0) {
				uint_t n = (uint_t)AString(argv[++i]);

				while (proglist.Count() > n) {
					proglist.DeleteProg(0);
				}
			}
			else if (strcmp(argv[i], "--limit") == 0) {
				uint_t n = (uint_t)AString(argv[++i]);

				while (proglist.Count() > n) {
					proglist.DeleteProg(proglist.Count() - 1);
				}
			}
			else if ((strcmp(argv[i], "--write") == 0) || (strcmp(argv[i], "-w") == 0)) {
				AString filename = config.GetNamedFile(argv[++i]);

				if (!HasQuit() && proglist.WriteToFile(filename)) {
					printf("Wrote %u programmes to '%s'\n", proglist.Count(), filename.str());
				}
				else {
					printf("Failed to write programme list to '%s'\n", filename.str());
				}
			}
			else if (strcmp(argv[i], "--write-text") == 0) {
				AString filename = config.GetNamedFile(argv[++i]);

				if (proglist.WriteToTextFile(filename)) {
					printf("Wrote %u programmes to '%s'\n", proglist.Count(), filename.str());
				}
				else {
					printf("Failed to write programme list to '%s'\n", filename.str());
				}
			}
			else if (stricmp(argv[i], "--write-gnuplot") == 0) {
				AString filename = argv[++i];

				if (!proglist.WriteToGNUPlotFile(filename)) {
					fprintf(stderr, "Failed to write GNU plot file %s\n", filename.str());
				}
			}
			else if ((strcmp(argv[i], "--sort") == 0) || (strcmp(argv[i], "--sort-rev") == 0)) {
				bool reverse = (strcmp(argv[i], "--sort-rev") == 0);
				proglist.Sort(reverse);
				printf("%sorted list\n", reverse ? "Reverse s" : "S");
			}
			else if (strcmp(argv[i], "--schedule") == 0) {
				ADVBProgList::SchedulePatterns(starttime);
			}
			else if (strcmp(argv[i], "--write-scheduled-jobs") == 0) {
				if (!ADVBProgList::WriteToJobList()) {
					printf("Failed to write scheduled list to job list\n");
					res = -1;
				}
			}
			else if (strcmp(argv[i], "--start-time") == 0) {
				starttime.StrToDate(argv[++i]);
			}
			else if (strcmp(argv[i], "--fake-schedule") == 0) {
				ADVBProgList::SchedulePatterns(starttime, false);
			}
			else if (strcmp(argv[i], "--fake-schedule-list") == 0) {
				proglist.Schedule(starttime);
			}
			else if (strcmp(argv[i], "--unschedule-all") == 0) {
				ADVBProgList::UnscheduleAllProgrammes();
			}
			else if ((strcmp(argv[i], "--schedule-list") == 0) || (strcmp(argv[i], "-S") == 0)) {
				proglist.Schedule();
			}
			else if (strcmp(argv[i], "--record") == 0) {
				AString  progstr = argv[++i];
				ADVBProg prog;

				if (prog.Base64Decode(progstr)) prog.Record();
				else printf("Failed to decode programme '%s'\n", progstr.str());
			}
			else if (strcmp(argv[i], "--record-title") == 0) {
				AString  title = argv[++i];
				ADVBLock lock("dvbfiles");
				ADVBProgList list;

				if (list.ReadFromFile(config.GetListingsFile())) {
					lock.ReleaseLock();

					list.RecordImmediately(ADateTime().TimeStamp(true), title);
				}
			}
			else if (stricmp(argv[i], "--schedule-record") == 0) {
				AString  base64 = argv[++i];
				ADVBProg prog;

				if (prog.Base64Decode(base64)) {
					if (prog.WriteToJobQueue()) {
						ADVBLock lock("dvbfiles");
						ADVBProgList scheduledlist;

						config.printf("Scheduled '%s'", prog.GetDescription().str());

						if (scheduledlist.ReadFromFile(config.GetScheduledFile())) {
							scheduledlist.AddProg(prog);
							scheduledlist.WriteToFile(config.GetScheduledFile());
						}
					}
					else config.printf("Failed to schedule '%s'", prog.GetDescription().str());
				}
				else config.printf("Unable to decode base 64 string");
			}
			else if (strcmp(argv[i], "--add-recorded") == 0) {
				AString  progstr = argv[++i];
				ADVBProg prog;

				if (prog.Base64Decode(progstr)) {
					const AString filename = prog.GetFilename();
					FILE_INFO info;
					uint_t    nsecs;

					config.printf("Adding %s to recorded...", prog.GetQuickDescription().str());

					prog.ClearScheduled();
					prog.ClearRecording();
					prog.ClearRunning();
					prog.SetRecorded();
					prog.SetActualStart(prog.GetRecordStart());
					prog.SetActualStop(prog.GetRecordStop());
					prog.SetRecordingComplete();

					config.printf("Faking actual start and stop times (%s-%s)", prog.GetActualStartDT().DateFormat("%h:%m:%s").str(), prog.GetActualStopDT().DateFormat("%h:%m:%s").str());

					nsecs = (uint_t)((prog.GetActualStop() - prog.GetActualStart()) / 1000);

					if (::GetFileInfo(filename, &info)) {
						config.printf("File '%s' exists and is %sMB, %s seconds = %skB/s",
									  filename.str(),
									  AValue(info.FileSize / ((uint64_t)1024 * (uint64_t)1024)).ToString().str(),
									  AValue(nsecs).ToString().str(),
									  AValue(info.FileSize / ((uint64_t)1024 * (uint64_t)nsecs)).ToString().str());

						prog.SetFileSize(info.FileSize);
					}
					else config.printf("File '%s' *doesn't* exists", filename.str());

					ADVBProgList::AddToList(config.GetRecordedFile(), prog, true, true);
				}
				else config.printf("Failed to decode programme '%s'\n", progstr.str());
			}
			else if (strcmp(argv[i], "--card") == 0) {
				const uint_t newcard = (uint_t)AString(argv[++i]);

				config.GetPhysicalDVBCard(0);
				if (config.GetMaxDVBCards()) {
					if (newcard < config.GetMaxDVBCards()) {
						dvbcard = config.GetPhysicalDVBCard(newcard);
						printf("Switched to using virtual card %u which is phsical card %u\n", newcard, dvbcard);
					}
					else fprintf(stderr, "Illegal virtual DVB card specified, must be 0..%u\n", config.GetMaxDVBCards() - 1);
				}
				else fprintf(stderr, "No virtual DVB cards available!\n");
			}
			else if (strcmp(argv[i], "--dvbcard") == 0) {
				dvbcard = (uint_t)AString(argv[++i]);

				config.GetPhysicalDVBCard(0);
				printf("Switched to using physical card %u\n", dvbcard);
			}
			else if (strcmp(argv[i], "--pids") == 0) {
				ADVBChannelList& list = ADVBChannelList::Get();
				const AString channel = argv[++i];
				AString pids;

				list.GetPIDList(dvbcard, channel, pids, true);

				printf("pids for '%s': %s\n", channel.str(), pids.str());
			}
			else if (strcmp(argv[i], "--scan") == 0) {
				ADVBChannelList& list = ADVBChannelList::Get();
				const AString freqs = argv[++i];
				uint_t j, n = freqs.CountColumns();

				for (j = 0; j < n; j++) {
					list.Update(dvbcard, (uint32_t)(1.0e6 * (double)freqs.Column(j)), true);
				}
			}
			else if (strcmp(argv[i], "--scan-all") == 0) {
				ADVBChannelList& list = ADVBChannelList::Get();

				list.UpdateAll(dvbcard, true);
			}
			else if (strcmp(argv[i], "--scan-range") == 0) {
				ADVBChannelList& list = ADVBChannelList::Get();
				const double f1   = (double)AString(argv[++i]);
				const double f2   = (double)AString(argv[++i]);
				const double step = (double)AString(argv[++i]);
				double f;

				for (f = f1; f <= f2; f += step) {
					list.Update(dvbcard, (uint32_t)(1.0e6 * f), true);
				}
			}
			else if (strcmp(argv[i], "--find-cards") == 0) {
				findcards();
				config.GetPhysicalDVBCard(0);
				config.printf("Total %u DVB cards found", config.GetMaxDVBCards());
			}
			else if (strcmp(argv[i], "--change-filename") == 0) {
				ADVBLock 	 lock("dvbfiles");
				AString  	 filename1 = argv[++i];
				AString  	 filename2 = argv[++i];
				ADVBProgList reclist;

				if (filename2 != filename1) {
					if (reclist.ReadFromFile(config.GetRecordedFile())) {
						uint_t j;
						bool changed = false;

						for (j = 0; (j < reclist.Count()) && !HasQuit(); j++) {
							ADVBProg& prog = reclist.GetProgWritable(j);

							if (prog.GetFilename() == filename1) {
								config.printf("Changing filename of '%s' from '%s' to '%s'", prog.GetDescription().str(), filename1.str(), filename2.str());
								prog.SetFilename(filename2);
								changed = true;
								break;
							}
						}

						if (changed) {
							if (HasQuit() || !reclist.WriteToFile(config.GetRecordedFile())) {
								config.printf("Failed to write recorded programme list back!");
							}
						}
						else config.printf("Failed to find programme with filename '%s'", filename1.str());
					}
					else config.printf("Failed to read recorded programmes from '%s'", config.GetRecordedFile().str());
				}
			}
			else if ((strcmp(argv[i], "--change-filename-regex")      == 0) ||
					 (strcmp(argv[i], "--change-filename-regex-test") == 0)) {
				ADVBLock     lock("dvbfiles");
				ADVBProgList reclist;
				AString 	 errors;
				AString 	 pattern       = argv[++i];
				AString 	 parsedpattern = ParseRegex(pattern, errors);
				AString 	 replacement   = argv[++i];
				bool    	 commit        = (strcmp(argv[i - 2], "--change-filename-regex") == 0);

				if (errors.Valid()) {
					config.printf("Errors in regex:");

					uint_t j, n = errors.CountLines();
					for (j = 0; j < n; j++) config.printf("%s", errors.Line(j).str());
				}
				else if (reclist.ReadFromFile(config.GetRecordedFile())) {
					uint_t j;
					bool changed = false;

					for (j = 0; j < reclist.Count(); j++) {
						ADataList regionlist;
						ADVBProg& prog = reclist.GetProgWritable(j);
						const AString& filename1 = prog.GetFilename();

						if (MatchRegex(filename1, parsedpattern, regionlist)) {
							AString filename2 = ExpandRegexRegions(filename1, replacement, regionlist);

							config.printf("Changing filename of '%s' from '%s' to '%s'", prog.GetDescription().str(), filename1.str(), filename2.str());
							prog.SetFilename(filename2);
							changed = true;
						}
					}

					if (changed) {
						if (commit) {
							if (HasQuit() || !reclist.WriteToFile(config.GetRecordedFile())) {
								config.printf("Failed to write recorded programme list back!");
							}
						}
						else config.printf("NOT writing changes back to files");
					}
					else config.printf("Failed to find programmes with filename matching '%s'", pattern.str());
				}
				else config.printf("Failed to read recorded programme list");
			}
			else if (stricmp(argv[i], "--find-series") == 0) {
				AHash series;

				proglist.FindSeries(series);

				printf("Found series for %u programmes\n", series.GetItems());
				series.Traverse(&__DisplaySeries);
			}
			else if (stricmp(argv[i], "--fix-data-in-recorded") == 0) {
				ADVBLock     lock("dvbfiles");
				ADVBProgList reclist;

				if (reclist.ReadFromFile(config.GetRecordedFile())) {
					uint_t j, n = 0;

					for (j = 0; (j < reclist.Count()) && !HasQuit(); j++) {
						ADVBProg& prog = reclist.GetProgWritable(j);

						if (prog.FixData()) n++;
					}

					config.printf("%u programmes changed", n);

					if (n && (HasQuit() || !reclist.WriteToFile(config.GetRecordedFile()))) {
						config.printf("Failed to write recorded programme list back!");
					}
				}
				else config.printf("Failed to read recorded programme list");
			}
			else if (stricmp(argv[i], "--set-recorded-flag") == 0) {
				ADVBLock     lock("dvbfiles");
				ADVBProgList reclist;

				if (reclist.ReadFromFile(config.GetRecordedFile())) {
					uint_t j;

					for (j = 0; (j < reclist.Count()) && !HasQuit(); j++) {
						reclist.GetProgWritable(j).SetRecorded();
					}

					if (HasQuit() || !reclist.WriteToFile(config.GetRecordedFile())) {
						config.printf("Failed to write recorded programme list back!");
					}
				}
				else config.printf("Failed to read recorded programme list");
			}
			else if (stricmp(argv[i], "--find-recorded-programmes-on-disk") == 0) {
				ADVBLock     lock("dvbfiles");
				ADVBProgList reclist;
				AString  	 recdir = config.GetRecordingsDir();
				AString  	 dirs   = config.GetConfigItem("searchdirs", "");

				if (reclist.ReadFromFile(config.GetRecordedFile())) {
					uint_t j, k, n = dirs.CountLines(",");
					uint_t found = 0;

					for (j = 0; (j < reclist.Count()) && !HasQuit(); j++) {
						ADVBProg& prog = reclist.GetProgWritable(j);
						FILE_INFO info;

						if (!AStdFile::exists(prog.GetFilename())) {
							AString filename1 = prog.GenerateFilename(prog.IsConverted());
							if (AStdFile::exists(filename1)) {
								prog.SetFilename(filename1);
								found++;
							}
						}
					}

					for (j = 0; (j < n) && !HasQuit(); j++) {
						AString path = recdir.CatPath(dirs.Line(j, ","));
						AList   dirs;

						dirs.Add(new AString(path));
						CollectFiles(path, "*", RECURSE_ALL_SUBDIRS, dirs, FILE_FLAG_IS_DIR, FILE_FLAG_IS_DIR);

						const AString *dir = AString::Cast(dirs.First());
						while (dir && !HasQuit()) {
							if (dir->FilePart() != "subs") {
								config.printf("Searching '%s'...", dir->str());

								for (k = 0; (k < reclist.Count()) && !HasQuit(); k++) {
									ADVBProg& prog = reclist.GetProgWritable(k);

									if (!AStdFile::exists(prog.GetFilename()) || config.GetRelativePath(prog.GetFilename()).Empty()) {
										AString filename1 = dir->CatPath(AString(prog.GetFilename()).FilePart());
										AString filename2 = filename1.Prefix() + "." + config.GetConvertedFileSuffix(prog.GetUser());

										if (AStdFile::exists(filename2)) {
											prog.SetFilename(filename2);
											found++;
										}
										else if (AStdFile::exists(filename1)) {
											prog.SetFilename(filename1);
											found++;
										}
									}
								}
							}

							dir = dir->Next();
						}
					}

					config.printf("Found %u previously orphaned programmes", found);

					if (found && !HasQuit()) {
						if (!reclist.WriteToFile(config.GetRecordedFile())) {
							config.printf("Failed to write recorded programme list back!");
						}
					}
				}
				else config.printf("Failed to read recorded programme list");
			}
			else if (stricmp(argv[i], "--check-disk-space") == 0) {
				ADVBProgList::CheckDiskSpace(true);
			}
			else if (stricmp(argv[i], "--update-recording-complete") == 0) {
				ADVBLock     lock("dvbfiles");
				ADVBProgList reclist;

				if (reclist.ReadFromFile(config.GetRecordedFile())) {
					uint_t j;

					for (j = 0; (j < reclist.Count()) && !HasQuit(); j++) {
						ADVBProg& prog = reclist.GetProgWritable(j);

						prog.SetRecordingComplete();
					}

					if (HasQuit() || !reclist.WriteToFile(config.GetRecordedFile())) {
						config.printf("Failed to write recorded programme list back!");
					}
				}
			}
			else if (stricmp(argv[i], "--check-recording-file") == 0) {
				ADVBProgList::CheckRecordingFile();
			}
			else if ((stricmp(argv[i], "--set-ignore-flag") == 0) ||
					 (stricmp(argv[i], "--clr-ignore-flag") == 0)) {
				ADVBLock     lock("dvbfiles");
				ADVBProgList reclist;
				bool         ignore = (stricmp(argv[i], "--set-ignore-flag") == 0);
				AString      patterns = argv[++i], errors;

				if (reclist.ReadFromFile(config.GetRecordedFile())) {
					ADVBProgList reslist;
					uint_t j, changed = 0;

					reclist.FindProgrammes(reslist, patterns, errors, (patterns.Pos("\n") >= 0) ? "\n" : ";");

					config.printf("Found %u programme%s", reslist.Count(), (reslist.Count() == 1) ? "" : "s");

					if (errors.Valid()) {
						config.printf("Errors:");

						uint_t j, n = errors.CountLines();
						for (j = 0; j < n; j++) config.printf("%s", errors.Line(j).str());
					}

					for (j = 0; (j < reslist.Count()) && !HasQuit(); j++) {
						ADVBProg *prog;

						if ((prog = reclist.FindUUIDWritable(reslist.GetProg(j))) != NULL) {
							if (prog->IgnoreRecording() != ignore) {
								config.printf("Changing '%s'", prog->GetQuickDescription().str());
								prog->SetIgnoreRecording(ignore);
								changed++;
							}
						}
						else config.printf("Failed to find programme '%s' in recordlist!", reslist.GetProg(j).GetQuickDescription().str());
					}

					config.printf("Changed %u programme%s", changed, (changed == 1) ? "" : "s");

					if (changed) {
						if (HasQuit() || !reclist.WriteToFile(config.GetRecordedFile())) {
							config.printf("Failed to write recorded programme list back!");
						}
					}
				}
			}
			else if (stricmp(argv[i], "--force-failures") == 0) {
				ADVBLock     lock("dvbfiles");
				ADVBProgList failureslist;

				if (!AStdFile::exists(config.GetRecordFailuresFile()) || failureslist.ReadFromFile(config.GetRecordFailuresFile())) {
					uint_t j;

					for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
						failureslist.AddProg(proglist.GetProg(j), false);
					}

					for (j = 0; (j < failureslist.Count()) && !HasQuit(); j++) {
						ADVBProg& prog = failureslist.GetProgWritable(j);

						prog.ClearScheduled();
						prog.SetRecordingFailed();
					}

					if (HasQuit() || !failureslist.WriteToFile(config.GetRecordFailuresFile())) {
						config.printf("Failed to write record failure list back!");
					}
				}
				else config.printf("Failed to read record failure list");
			}
			else if (stricmp(argv[i], "--test-notify") == 0) {
				uint_t j;

				for (j = 0; j < proglist.Count(); j++) {
					ADVBProg& prog = proglist.GetProgWritable(j);
					prog.SetNotify();
					prog.OnRecordSuccess();
				}
			}
			else if (stricmp(argv[i], "--change-user") == 0) {
				ADVBProgList reslist;
				AString      patterns = argv[++i], newuser = argv[++i], errors;
				uint_t j, changed = 0;

				proglist.FindProgrammes(reslist, patterns, errors, (patterns.Pos("\n") >= 0) ? "\n" : ";");

				printf("Found %u programme%s\n", reslist.Count(), (reslist.Count() == 1) ? "" : "s");

				if (errors.Valid()) {
					printf("Errors:");

					uint_t j, n = errors.CountLines();
					for (j = 0; j < n; j++) config.printf("%s", errors.Line(j).str());
				}

				for (j = 0; (j < reslist.Count()) && !HasQuit(); j++) {
					ADVBProg *prog;

					if ((prog = proglist.FindUUIDWritable(reslist.GetProg(j))) != NULL) {
						if (prog->GetUser() != newuser) {
							prog->SetUser(newuser);
							changed++;
						}
					}
				}

				printf("Changed %u programme%s\n", changed, (changed == 1) ? "" : "s");
			}
			else if (stricmp(argv[i], "--show-encoding-args") == 0) {
				uint_t j;

				for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
					const ADVBProg& prog = proglist.GetProg(j);
					AString proccmd = config.GetEncodeCommand(prog.GetUser(), prog.GetCategory());
					AString args    = config.GetEncodeArgs(prog.GetUser(), prog.GetCategory());
					uint_t  k, n = args.CountLines(";");

					printf("%s:\n", prog.GetQuickDescription().str());

					for (k = 0; k < n; k++) {
						printf("\tCommand: %s %s\n",
							   proccmd.str(),
							   args.Line(k, ";").Words(0).str());
					}

					printf("\n");
				}
			}
			else if (stricmp(argv[i], "--show-file-format") == 0) {
				uint_t j;

				for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
					const ADVBProg& prog = proglist.GetProg(j);

					if (AStdFile::exists(prog.GetFilename())) {
						AString format;

						if (ADVBProg::GetFileFormat(prog.GetFilename(), format)) {
							printf("%s: %s\n", prog.GetQuickDescription().str(), format.str());
						}
						else printf("%s: unknown\n", prog.GetQuickDescription().str());
					}
				}
			}
			else if (stricmp(argv[i], "--convert") == 0) {
				uint_t j, converted = 0;

				for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
					ADVBProg& prog = proglist.GetProgWritable(j);
					AString   oldfilename = prog.GenerateFilename();

					if ((!prog.IsConverted() && AStdFile::exists(prog.GetFilename())) ||
						(prog.IsConverted() && !AStdFile::exists(prog.GetFilename()) && AStdFile::exists(oldfilename))) {
						if (!AStdFile::exists(prog.GetFilename()) && AStdFile::exists(oldfilename)) {
							printf("Reverting back to oldfilename of '%s'\n", oldfilename.str());
							prog.SetFilename(oldfilename);
						}

						config.printf("Converting file %u/%u - '%s':", j + 1, proglist.Count(), prog.GetQuickDescription().str());

						prog.ConvertVideo(true);

						converted++;
					}
				}

				printf("Converted %u programmes\n", converted);
			}
			else if (stricmp(argv[i], "--use-converted-filename") == 0) {
				uint_t j;

				for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
					ADVBProg& prog = proglist.GetProgWritable(j);

					if (!prog.IsConverted()) {
						AString oldfilename = prog.GetFilename();

						prog.SetFilename(prog.GenerateFilename(true));

						if (oldfilename != AString(prog.GetFilename())) prog.UpdateRecordedList();
					}
				}
			}
			else if (stricmp(argv[i], "--set-dir") == 0) {
				ADVBProgList reslist;
				AString      patterns = argv[++i], newdir = argv[++i], errors;
				uint_t       j;

				proglist.FindProgrammes(reslist, patterns, errors, (patterns.Pos("\n") >= 0) ? "\n" : ";");

				printf("Found %u programme%s\n", reslist.Count(), (reslist.Count() == 1) ? "" : "s");

				if (errors.Valid()) {
					printf("Errors:\n");

					uint_t j, n = errors.CountLines();
					for (j = 0; j < n; j++) printf("%s", errors.Line(j).str());
				}

				proglist.CreateHash();
				for (j = 0; j < reslist.Count(); j++) {
					ADVBProg *prog;

					if ((prog = proglist.FindUUIDWritable(reslist[j])) != NULL) {
						printf("Set directory of '%s' to '%s'\n", prog->GetQuickDescription().str(), newdir.str());
						prog->SetDir(newdir);
					}
				}
			}
			else if ((stricmp(argv[i], "--regenerate-filename") == 0) ||
					 (stricmp(argv[i], "--regenerate-filename-test") == 0)) {
				const bool test = (stricmp(argv[i], "--regenerate-filename-test") == 0);
				uint_t j, nmatches = 0;

				for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
					ADVBProg& prog = proglist.GetProgWritable(j);

					AString oldfilename = prog.GetFilename();

					if (prog.IsConverted()) {
						AString subdir = oldfilename.PathPart().PathPart().FilePart();

						if (prog.IsFilm()) {
							if (!test) prog.SetDir("Films");
						}
						else if (subdir.StartsWith("Shows")) {
							if (!test) prog.SetDir(subdir + "/{titledir}");
						}
						else if (CompareCase(prog.GetDir(), AString(prog.GetUser()).InitialCapitalCase() + "/{titledir}") == 0) {
							if (!test) prog.SetDir("");
						}
					}

					AString newfilename  = prog.GenerateFilename(prog.IsConverted());
					AString oldfilename1 = oldfilename.FilePart();
					AString newfilename1 = newfilename.FilePart();

					if (newfilename != oldfilename) {
						printf("'%s' -> '%s'\n", oldfilename.str(), newfilename.str());

						if (!test) {
							prog.SetFilename(newfilename);
						
							if (AStdFile::exists(oldfilename)) {
								CreateDirectory(newfilename.PathPart());
								if (MoveFile(oldfilename, newfilename)) {
									printf("\tRenamed '%s' to '%s'\n", oldfilename.str(), newfilename.str());
								}
								else fprintf(stderr, "Failed to rename '%s' to '%s'\n", oldfilename.str(), newfilename.str());
							}
							else if (AStdFile::exists(oldfilename1)) {
								if (MoveFile(oldfilename1, newfilename1)) {
									printf("\tRenamed '%s' to '%s'\n", oldfilename1.str(), newfilename1.str());
								}
								else fprintf(stderr, "Failed to rename '%s' to '%s'\n", oldfilename1.str(), newfilename1.str());
							}
							else printf("\tNeither '%s' nor '%s' exists\n", oldfilename.str(), oldfilename1.str());
						}
					}
					else nmatches++;
				}

				if (nmatches) printf("%u filenames unchanged\n", nmatches);
			}
			else if (stricmp(argv[i], "--delete-files") == 0) {
				uint_t j;

				for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
					const ADVBProg& prog = proglist.GetProg(j);

					prog.DeleteEncodedFiles();
				}
			}
			else if (stricmp(argv[i], "--delete-from-record-lists") == 0) {
				AString uuid = argv[++i];

				if (ADVBProgList::RemoveFromRecordLists(uuid)) {
					printf("Removed programme with UUID '%s' from record lists\n", uuid.str());
				}
				else {
					fprintf(stderr, "Failed to remove programme with UUID '%s' from record lists\n", uuid.str());
					res = -1;
				}
			}
			else if (stricmp(argv[i], "--record-success") == 0) {
				uint_t j;

				for (j = 0; j < proglist.Count(); j++) {
					ADVBProg& prog = proglist.GetProgWritable(j);

					printf("Running record success for '%s'\n", prog.GetQuickDescription().str());
					prog.OnRecordSuccess();
				}
			}
			else if (stricmp(argv[i], "--get-and-convert-recorded") == 0) {
				if (!ADVBProgList::GetAndConvertRecordings()) res = -1;
			}
			else if (stricmp(argv[i], "--force-convert-files") == 0) {
				uint_t j;

				for (j = 0; j < proglist.Count(); j++) {
					ADVBProg& prog = proglist.GetProgWritable(j);

					prog.ForceConvertVideo(true);
				}
			}
			else if (stricmp(argv[i], "--update-recordings-list") == 0) {
				if (!ADVBProgList::GetRecordingListFromRecordingSlave()) res = -1;
			}
			else if (stricmp(argv[i], "--check-recording-now") == 0) {
				ADVBProgList::CheckRecordingNow();
			}
			else if (stricmp(argv[i], "--calc-trend") == 0) {
				ADateTime startdate(argv[++i]);
				ADVBProgList::TREND trend;

				printf("Calculating trend from %s\n", startdate.DateToStr().str());

				if ((trend = proglist.CalculateTrend(startdate.LocalToUTC())).valid) {
					printf("Average: %0.2lf per day\n", trend.rate);
					printf("Trend: %0.8lf + %0.8lf * (x - %0.3lf) / (3600.0 * 24.0)\n",
						   trend.offset, trend.rate, trend.timeoffset);
				}
				else fprintf(stderr, "Cannot calculate rate on empty programme list!\n");
			}
			else if (stricmp(argv[i], "--gen-graphs") == 0) {
				ADVBProgList::CreateGraphs();
			}
			else if (stricmp(argv[i], "--assign-episodes") == 0) {
				printf("Assigning episodes...\n");
				proglist.AssignEpisodes();
			}
			else if (stricmp(argv[i], "--reset-assigned-episodes") == 0) {
				uint_t i;

				for (i = 0; i < proglist.Count(); i++)
				{
					proglist.GetProgWritable(i).SetAssignedEpisode(0);
				}
			}
			else if (stricmp(argv[i], "--find-episode-sequences") == 0) {
				std::map<AString, std::map<AString, std::vector<AString> > > programmes;
				std::map<AString, const std::vector<AString> *> bestlist;
				std::map<AString, uint16_t> episodeids;
				uint_t i;

				for (i = 0; i < proglist.Count(); i++) {
					const ADVBProg& prog = proglist.GetProg(i);
					AString epid = prog.GetEpisodeID();

					if ((epid.Left(2) == "EP") && !prog.GetEpisode().valid) {
						std::vector<AString>& list = programmes[prog.GetTitle()][prog.GetChannel()];

						if (std::find(list.begin(), list.end(), epid) == list.end()) {
							list.push_back(epid);

							if (!bestlist[prog.GetTitle()] || (list.size() > bestlist[prog.GetTitle()]->size())) {
								bestlist[prog.GetTitle()] = &list;
							}

							if (prog.GetAssignedEpisode() != 0) {
								episodeids[epid] = prog.GetAssignedEpisode();
							}
						}
					}
				}

				std::map<AString, const std::vector<AString> *>::iterator it;
				for (it = bestlist.begin(); it != bestlist.end(); ++it) {
					if (it->second) {
						const std::vector<AString>& list = *it->second;

						printf("Episode list for '%s':\n", it->first.str());

						size_t i;
						uint16_t epn = 1;
						for (i = 0; i < list.size(); i++, epn++) {
							const AString& epid = list[i];

							if (episodeids.find(epid) != episodeids.end()) {
								if (episodeids[epid] > epn) epn = episodeids[epid];
							}
							else episodeids[epid] = epn;

							printf("\t%s: %u\n", epid.str(), epn);
						}

						printf("\n");
					}
					else printf("No list for '%s'!\n", it->first.str());
				}

				for (i = 0; i < proglist.Count(); i++) {
					ADVBProg& prog = proglist.GetProgWritable(i);
					AString   epid = prog.GetEpisodeID();
					std::map<AString, uint16_t>::iterator it;

					if ((it = episodeids.find(epid)) != episodeids.end()) {
						prog.SetAssignedEpisode(it->second);
					}
				}
			}
			else if (stricmp(argv[i], "--count-hours") == 0) {
				uint64_t ms = 0;
				uint_t   i;

				for (i = 0; i < proglist.Count(); i++)
				{
					const ADVBProg& prog = proglist.GetProg(i);

					if		(prog.GetActualLength()) ms += prog.GetActualLength();
					else if (prog.GetRecordLength()) ms += prog.GetRecordLength();
					else							 ms += prog.GetLength();
				}

				double hours = (double)ms / (1000.0 * 3600.0);
				printf("%u programmes, %0.2lf hours, average %0.1lf minutes/programme\n", proglist.Count(), hours, (60.0 * hours) / (double)proglist.Count());
			}
			else if (stricmp(argv[i], "--find-gaps") == 0) {
				ADVBProgList list;

				if (list.ReadFromFile(config.GetScheduledFile())) {
					std::vector<ADVBProgList::TIMEGAP> gaps;
					ADVBProgList::TIMEGAP best;
					uint_t i;

					best = list.FindGaps(ADateTime().TimeStamp(true), gaps);

					for (i = 0; i < (uint_t)gaps.size(); i++) {
						const ADVBProgList::TIMEGAP& gap = gaps[i];
						uint_t card = i;
						
						if (gap.end < ADateTime::MaxDateTime) {
							ADateTime diff = gap.end - gap.start;								
							printf("Card %u (hardware card %u): free from %s to %s (%s)\n", card, config.GetPhysicalDVBCard(card), gap.start.UTCToLocal().DateToStr().str(), gap.end.UTCToLocal().DateToStr().str(), diff.SpanStr().str());
						}
						else {
							printf("Card %u (hardware card %u): free from %s onwards\n", card, config.GetPhysicalDVBCard(card), gap.start.UTCToLocal().DateToStr().str());
						}
					}
				}
				else fprintf(stderr, "Failed to read scheduled programmes file\n");
			}
			else if (stricmp(argv[i], "--stream") == 0) {
				AString channel = argv[++i];
				
				if (config.GetRecordingSlave().Valid()) {
					AString cmd1, cmd2;
					
					cmd1.printf("dvb --stream \"%s\"", channel.str());
					cmd2.printf("| %s", config.GetVideoPlayerCommand().str());
					
					RunRemoteCommand(cmd1, cmd2);
				}
				else {
					ADVBChannelList& channellist = ADVBChannelList::Get();
					ADVBProgList list;
					
					if (list.ReadFromFile(config.GetScheduledFile())) {
						std::vector<ADVBProgList::TIMEGAP> gaps;
						ADVBProgList::TIMEGAP best;
						AString cmd, pids;
						uint_t gap;
						
						best = list.FindGaps(ADateTime().TimeStamp(true), gaps);
						gap  = (uint_t)((uint64_t)(best.end - best.start) / 1000);
								
						fprintf(stderr, "Biggest gap is on card %u: %s-%s (%u seconds)\n",
								best.card,
								best.start.UTCToLocal().DateToStr().str(),
								best.end.UTCToLocal().DateToStr().str(),
								gap);

						if (gap > 10U) {
							AString pids;

							gap -= 10U;

							channellist.GetPIDList(best.card, channel, pids);
							if (pids.Valid()) {
								cmd = ADVBProg::GenerateStreamCommand(best.card, gap, pids);

								if (!config.IsRecordingSlave()) cmd.printf(" | %s", config.GetVideoPlayerCommand().str());
								
								if (system(cmd) != 0) {
									fprintf(stderr, "Command '%s' failed!\n", cmd.str());
								}
							}
							else fprintf(stderr, "Failed to find PIDs for channel '%s'\n", channel.str());
						}
						else fprintf(stderr, "No DVB card has big enough gap\n");
					}
					else fprintf(stderr, "Failed to read scheduled programmes file\n");
				}
			}
			else if (stricmp(argv[i], "--return-count") == 0) {
				res = proglist.Count();
			}
#if EVALTEST
			else if (stricmp(argv[i], "--eval") == 0) {
				simplevars_t vars;
				AString  	 str = argv[++i];
				AString  	 errors;
				AValue       res = 0;

				AddSimpleFunction("func", &__func);
				
				if (Evaluate(vars, str, res, errors)) {
					printf("Resultant: %s\n", AValue(res).ToString().str());
				}
				else if (errors.Valid()) printf("Errors:\n%s", errors.str());
				else printf("Failed to evaluate expression '%s'\n", str.str());
			}
#endif
			else if (stricmp(argv[i], "--db") == 0) {
				ADVBDatabase db;
				
			}
			else {
				fprintf(stderr, "Unrecognized option '%s'\n", argv[i]);
				exit(1);
			}
		}
	}

	return res;
}
