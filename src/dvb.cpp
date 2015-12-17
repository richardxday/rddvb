
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <map>

#include <rdlib/Regex.h>
#include <rdlib/Recurse.h>

#include "config.h"
#include "dvblock.h"
#include "proglist.h"
#include "channellist.h"
#include "findcards.h"

typedef struct {
	AString  filename;
	uint64_t length;
	uint64_t filesize;
} RECORDDETAILS;

bool forcelogging = false;

static bool __DisplaySeries(const char *key, uptr_t value, void *context)
{
	UNUSED(context);

	if (value) {
		const ADVBProgList::SERIES& series = *(const ADVBProgList::SERIES *)value;
		const ADataList& serieslist = series.list;
		uint_t j;

		for (j = 0; j < serieslist.Count(); j++) {
			const AString *str = (const AString *)serieslist[j];
			
			if (str) printf("Programme '%s' series %u: %s\n", key, j, str->str());
		}
	}

	return true;
}

int main(int argc, char *argv[])
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBProgList proglist;
	ADVBProg  prog;	// ensure ADVBProg initialisation takes place
	ADateTime starttime = ADateTime().TimeStamp(true);
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
		printf("\t--update <file>\t\t\tUpdate main listings with file <file> (-u)\n");
		printf("\t--load\t\t\t\tRead listings from default file (-l)\n");
		printf("\t--read <file>\t\t\tRead listings from file <file> (-r)\n");
		printf("\t--read-radio-listings\t\tDownload BBC radio listings\n");
		printf("\t--jobs\t\t\t\tRead programmes from scheduled jobs\n");
		printf("\t--write <file>\t\t\tWrite listings to file <file> (-w)\n");
		printf("\t--sort\t\t\t\tSort list in chronological order\n");
		printf("\t--sort-rev\t\t\tSort list in reverse-chronological order\n");
		printf("\t--writetxt <file>\t\tWrite listings to file <file> in text format\n");
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
		printf("\t--find-similar <file>\tFor each programme in current list, find first similar programme in <file>\n");
		printf("\t--delete <patterns>\t\tDelete programmes matching <patterns>\n");
		printf("\t--delete-with-file <pattern-file> Delete programmes matching patterns in patterns file <pattern-file>\n");
		printf("\t--delete-recorded\t\tDelete programmes that have been recorded\n");
		printf("\t--delete-using-file <file>\tDelete programmes that are similar to those in file <file>\n");
		printf("\t--delete-similar\t\tDelete programmes that are similar to others in the list\n");
		printf("\t--schedule\t\t\tSchedule and create jobs for default set of patterns on the main listings file\n");
		printf("\t--start-time <time>\t\tSet start time for scheduling\n");
		printf("\t--fake-schedule\t\t\tPretend to schedule (write files) for default set of patterns on the main listings file\n");
		printf("\t--fake-schedule-list\t\tPretend to schedule (write files) for current list of programmes\n");
		printf("\t--unschedule-all\t\tUnschedule all programmes\n");
		printf("\t--schedule-list\t\t\tSchedule current list of programmes (-S)\n");
		printf("\t--record <prog>\t\t\tRecord programme <prog> (Base64 encoded)\n");
		printf("\t--add-recorded <prog>\t\tAdd recorded programme <prog> to recorded list (Base64 encoded)\n");
		printf("\t--record-now <channel>[:<mins>]\tRecord channel <channel>\n");
		printf("\t--record-list\t\t\tSet the current list to record, ensuring they don't clash with the current set of jobs\n");
		printf("\t--pids <channel>\t\tFind PIDs (all streams) associated with channel <channel>\n");
		printf("\t--scan <freq>[,<freq>...]\tScan frequencies <freq>MHz for DVB channels\n");
		printf("\t--scan-all\t\t\tScan all known frequencies for DVB channels\n");
		printf("\t--scan-range <start-freq> <end-freq> <step>\n\t\t\t\t\tScan frequencies <freq>MHz for DVB channels\n");
		printf("\t--log <filename>\t\tLog to additional file <filename>\n");
		printf("\t--cards\t\t\t\tFind DVB cards\n");
		printf("\t--change-filename <filename1> <filename2>\n\t\t\t\t\tChange filename of recorded progamme with filename <filename1> to <filename2>\n");
		printf("\t--change-filename-regex <filename1> <filename2>\n\t\t\t\t\tChange filename of recorded progamme with filename <filename1> to <filename2> (where <filename1> can be a regex and <filename2> can be an expansion)\n");
		printf("\t--change-filename-regex-test <filename1> <filename2>\n\t\t\t\t\tLike above but do not commit changes\n");
		printf("\t--rename-files <filename-pattern>\n\t\t\t\t\tRename files in current list and update recorded list, if necessary\n");
		printf("\t--rename-files-test <filename-pattern>\n\t\t\t\t\tLike above but do not commit changes\n");
		printf("\t--find-recorded-programmes-on-disk\n\t\t\t\t\tSearch for recorded programmes in 'searchdirs' and update any filenames\n");
		printf("\t--find-series\t\t\tFind series' in programme list\n");
		printf("\t--set-recorded-flag\t\tSet recorded flag on all programmes in recorded list\n");
		printf("\t--set-ignore-flag <patterns>\tSet ignore recording flag on programmes matching pattern\n");
		printf("\t--clr-ignore-flag <patterns>\tClear ignore recording flag on programmes matching pattern\n");
		printf("\t--check-disk-space\t\tCheck disk space for all patterns\n");
		printf("\t--update-recording-complete\tUpdate recording complete flag in every recorded programme\n");
		printf("\t--check-recording-file\t\tCheck programmes in running list to ensure they should remain in there\n");
		printf("\t--force-failures\t\tAdd current list to recording failures (setting failed flag)\n");
		printf("\t--post-process\t\t\tPost process files in current list (this WILL alter the record list data)\n");
		printf("\t--change-user <patterns> <newuser>\n\t\t\t\t\tChange user of programmes matching <patterns> to <newuser>\n");
		printf("\t--return-count\t\t\tReturn programme list count in error code\n");
	}
	else {
		for (i = 1; i < argc; i++) {
			if (strncmp(argv[i], "-v", 2) == 0) {
				uint_t inc = (uint_t)AString(argv[i] + 2);
				verbosity += inc ? inc : 1;
			}
			else if (strcmp(argv[i], "--confdir") == 0) printf("%s\n", config.GetConfigDir().str());
			else if (strcmp(argv[i], "--datadir") == 0) printf("%s\n", config.GetDataDir().str());
			else if (strcmp(argv[i], "--logdir") == 0)  printf("%s\n", config.GetLogDir().str());
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

					config.printf("Removing old programmes...");
					proglist.DeleteProgrammesBefore(ADateTime(ADateTime().TimeStamp(true).GetDays() - config.GetDaysToKeep(), 0));

					config.printf("Updating DVB channels...");
					proglist.UpdateDVBChannels();

					config.printf("Assigning episode numbers where necessary...");
					proglist.AssignEpisodes();

					config.printf("Writing main listings file...");
					if (proglist.WriteToFile(filename)) {
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
				else {
					printf("Failed to read programme list from '%s'\n", filename.str());
				}
			}
			else if ((strcmp(argv[i], "--read") == 0) || (strcmp(argv[i], "-r") == 0)) {
				AString filename = config.GetNamedFile(argv[++i]);

				printf("Reading programmes...\n");
				if (proglist.ReadFromFile(filename)) {
					printf("Read programmes from '%s', total now %u\n", filename.str(), proglist.Count());
				}
				else {
					printf("Failed to read programme list from '%s'\n", filename.str());
				}
			}
			else if (strcmp(argv[i], "--read-radio-listings") == 0) {
				proglist.ReadRadioListings();
				printf("Read radio listings, total now %u\n", proglist.Count());
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
			else if (strcmp(argv[i], "--assign-episodes") == 0) {
				printf("Assigning episodes...\n");
				proglist.AssignEpisodes();
			}
			else if (strcmp(argv[i], "--assign-rec-episodes") == 0) {
				printf("Assigning episodes (reversed)...\n");
				proglist.AssignEpisodes(true, true);
			}
			else if (strcmp(argv[i], "--fix-pound") == 0) {
				AString filename = config.GetNamedFile(argv[++i]);

				proglist.DeleteAll();

				if (proglist.ReadFromFile(filename)) {
					printf("Read programmes from '%s', total %u\n", filename.str(), proglist.Count());
					printf("Fixing pound symbols...\n");

					proglist.SearchAndReplace("\xa3", "£");

					if (proglist.WriteToFile(filename)) {
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

					uint_t i, n = errors.CountLines();
					for (i = 0; i < n; i++) printf("%s\n", errors.Line(i).str());
				}

				printf("Found %u programme%s\n", reslist.Count(), (reslist.Count() == 1) ? "" : "s");
			}
			else if ((strcmp(argv[i], "--find-repeats") == 0) || (strcmp(argv[i], "-R") == 0)) {
				uint_t i;

				for (i = 0; i < proglist.Count(); i++) {
					ADVBProgList reslist;
					uint_t n;

					if ((n = proglist.FindSimilarProgrammes(reslist, proglist.GetProg(i))) != 0) {
						uint_t j;

						printf("%s: %u repeats found:\n", proglist[i].GetDescription(verbosity).str(), n);

						for (j = 0; j < reslist.Count(); j++) {
							const ADVBProg& prog = reslist[j];
							AString desc = prog.GetDescription(verbosity);
							uint_t k, nl = desc.CountLines();

							for (k = 0; k < nl; k++) {
								printf("\t%s\n", desc.Line(k).str());
							}

							proglist.DeleteProg(prog);
						}

						printf("\n");
					}
					else printf("%s: NO repeats found\n", proglist[i].GetDescription(verbosity).str());
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

						uint_t i, n = errors.CountLines();
						for (i = 0; i < n; i++) printf("%s", errors.Line(i).str());
					}
					
					printf("Found %u programme%s\n", reslist.Count(), (reslist.Count() == 1) ? "" : "s");
				}
				else printf("Failed to read patterns from '%s'", filename.str());
			}
			else if (strcmp(argv[i], "--delete") == 0) {
				ADVBProgList reslist;
				AString      patterns = argv[++i], errors;
				uint_t       i;

				proglist.FindProgrammes(reslist, patterns, errors, (patterns.Pos("\n") >= 0) ? "\n" : ";");
				
				printf("Found %u programme%s\n", reslist.Count(), (reslist.Count() == 1) ? "" : "s");

				if (errors.Valid()) {
					printf("Errors:\n");

					uint_t i, n = errors.CountLines();
					for (i = 0; i < n; i++) printf("%s\n", errors.Line(i).str());
				}

				for (i = 0; i < reslist.Count(); i++) {
					const ADVBProg& prog = reslist.GetProg(i);

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
				uint_t       i;

				if (patterns.ReadFromFile(filename)) {
					proglist.FindProgrammes(reslist, patterns, errors, (patterns.Pos("\n") >= 0) ? "\n" : ";");
				
					printf("Found %u programme%s\n", reslist.Count(), (reslist.Count() == 1) ? "" : "s");

					if (errors.Valid()) {
						printf("Errors:\n");

						uint_t i, n = errors.CountLines();
						for (i = 0; i < n; i++) printf("%s\n", errors.Line(i).str());
					}

					for (i = 0; i < reslist.Count(); i++) {
						const ADVBProg& prog = reslist.GetProg(i);

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
					uint_t i, ndeleted = 0;

					for (i = 0; i < proglist.Count(); ) {
						if (proglist2.FindSimilar(proglist.GetProg(i))) {
							proglist.DeleteProg(i);
							ndeleted++;
						}
						else i++;
					}

					printf("Deleted %u programmes\n", ndeleted);
				}
				else printf("Failed to read file '%s'\n", filename.str());
			}
			else if (strcmp(argv[i], "--delete-similar") == 0) {
				uint_t i, ndeleted = 0;

				for (i = 0; i < proglist.Count(); i++) {
					const ADVBProg& prog = proglist.GetProg(i);
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
					const ADVBProgList::CHANNEL *channel = proglist.GetChannel(j);
					AString dvbchannel = list.LookupDVBChannel(channel->name);

					printf("Channel %u/%u: '%s' (DVB '%s', XMLTV channel-id '%s')\n", j + 1, proglist.ChannelCount(), channel->name.str(), dvbchannel.str(), channel->id.str());

					ndvb += dvbchannel.Empty();
				}
				printf("%u/%u channels have NO DVB channel\n", ndvb, proglist.ChannelCount());
			}
			else if (strcmp(argv[i], "--cut-head") == 0) {
				uint_t n = (uint_t)AString(argv[++i]);
				uint_t i;

				for (i = 0; (i < n) && proglist.Count(); i++) {
					proglist.DeleteProg(0);
				}
			}
			else if (strcmp(argv[i], "--cut-tail") == 0) {
				uint_t n = (uint_t)AString(argv[++i]);
				uint_t i;

				for (i = 0; (i < n) && proglist.Count(); i++) {
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
			
				if (proglist.WriteToFile(filename)) {
					printf("Wrote %u programmes to '%s'\n", proglist.Count(), filename.str());
				}
				else {
					printf("Failed to write programme list to '%s'\n", filename.str());
				}
			}
			else if (strcmp(argv[i], "--writetxt") == 0) {
				AString filename = config.GetNamedFile(argv[++i]);
			
				if (proglist.WriteToTextFile(filename)) {
					printf("Wrote %u programmes to '%s'\n", proglist.Count(), filename.str());
				}
				else {
					printf("Failed to write programme list to '%s'\n", filename.str());
				}
			}
			else if ((strcmp(argv[i], "--sort") == 0) || (strcmp(argv[i], "-sort-rev") == 0)) {
				bool reverse = (strcmp(argv[i], "-sort-rev") == 0);
				proglist.Sort(reverse);
				printf("%sorted list\n", reverse ? "Reverse s" : "S");
			}
			else if (strcmp(argv[i], "--schedule") == 0) {
				ADVBProgList::SchedulePatterns();
			}
			else if (strcmp(argv[i], "--start-time") == 0) {
				starttime.StrToDate(argv[i]);
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
						config.printf("File '%s' exists and is %sMB, %u seconds = %skB/s", filename.str(), NUMSTR("", info.FileSize / (1024 * 1024)), nsecs, NUMSTR("", info.FileSize / (1024 * (uint64_t)nsecs)));

						prog.SetFileSize(info.FileSize);
					}
					else config.printf("File '%s' *doesn't* exists", filename.str());

					ADVBProgList::AddToList(config.GetRecordedFile(), prog, true, true);
				}
				else config.printf("Failed to decode programme '%s'\n", progstr.str());
			}
			else if (strcmp(argv[i], "--record-now") == 0) {
				AString arg = argv[++i];
				AString channel;
				uint_t mins = 0;
				int p;

				if ((p = arg.Pos(":")) >= 0) {
					channel = arg.Left(p);
					mins    = (uint_t)arg.Mid(p + 1);
				}
				else channel = arg;

				ADVBProg::Record(channel, mins);
			}
			else if (strcmp(argv[i], "--record-list") == 0) {
				proglist.SimpleSchedule();
			}
			else if (strcmp(argv[i], "--pids") == 0) {
				ADVBChannelList& list = ADVBChannelList::Get();
				AString channel = argv[++i];
				AString pids    = list.GetPIDList(channel, true);

				printf("pids for '%s': %s\n", channel.str(), pids.str());
			}
			else if (strcmp(argv[i], "--scan") == 0) {
				ADVBChannelList& list = ADVBChannelList::Get();
				AString freqs = argv[++i];
				uint_t i, n = freqs.CountColumns();

				for (i = 0; i < n; i++) {
					list.Update((uint32_t)(1.0e6 * (double)freqs.Column(i)), true);
				}
			}
			else if (strcmp(argv[i], "--scan-all") == 0) {
				ADVBChannelList& list = ADVBChannelList::Get();

				list.UpdateAll(true);
			}
			else if (strcmp(argv[i], "--scan-range") == 0) {
				ADVBChannelList& list = ADVBChannelList::Get();
				double f1   = (double)AString(argv[++i]);
				double f2   = (double)AString(argv[++i]);
				double step = (double)AString(argv[++i]);
				double f;

				for (f = f1; f <= f2; f += step) {
					list.Update((uint32_t)(1.0e6 * f), true);
				}
			}
			else if (strcmp(argv[i], "--cards") == 0) {
				findcards();
			}
			else if (strcmp(argv[i], "--log") == 0) {
				ADVBConfig::GetWriteable().SetAdditionalLogFile(argv[++i]);
			}
			else if (strcmp(argv[i], "--change-filename") == 0) {
				ADVBLock lock("schedule");
				AString  filename1 = argv[++i];
				AString  filename2 = argv[++i];
				ADVBProgList reclist;

				if (filename2 != filename1) {
					if (reclist.ReadFromFile(config.GetRecordedFile())) {
						uint_t i;
						bool changed = false;

						for (i = 0; i < reclist.Count(); i++) {
							ADVBProg& prog = reclist.GetProgWritable(i);

							if (prog.GetFilename() == filename1) {
								config.printf("Changing filename of '%s' from '%s' to '%s'", prog.GetDescription().str(), filename1.str(), filename2.str());
								prog.SetFilename(filename2);
								changed = true;
								break;
							}
						}

						if (changed) {
							if (!reclist.WriteToFile(config.GetRecordedFile())) {
								config.printf("Failed to write recorded programme list back!");
							}
						}
						else config.printf("Failed to find programme with filename '%s'", filename1.str());
					}
				}
			}
			else if ((strcmp(argv[i], "--change-filename-regex")      == 0) ||
					 (strcmp(argv[i], "--change-filename-regex-test") == 0)) {
				ADVBLock     lock("schedule");
				ADVBProgList reclist;
				AString 	 errors;
				AString 	 pattern       = argv[++i];
				AString 	 parsedpattern = ParseRegex(pattern, errors);
				AString 	 replacement   = argv[++i];
				bool    	 commit        = (strcmp(argv[i - 2], "--change-filename-regex") == 0);

				if (errors.Valid()) {
					config.printf("Errors in regex:");

					uint_t i, n = errors.CountLines();
					for (i = 0; i < n; i++) config.printf("%s", errors.Line(i).str());
				}
				else if (reclist.ReadFromFile(config.GetRecordedFile())) {
					uint_t i;
					bool changed = false;

					for (i = 0; i < reclist.Count(); i++) {
						ADataList regionlist;
						ADVBProg& prog = reclist.GetProgWritable(i);
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
							if (!reclist.WriteToFile(config.GetRecordedFile())) {
								config.printf("Failed to write recorded programme list back!");
							}
						}
						else config.printf("NOT writing changes back to files");
					}
					else config.printf("Failed to find programmes with filename matching '%s'", pattern.str());
				}
				else config.printf("Failed to read recorded programme list");
			}
			else if ((strcmp(argv[i], "--rename-files")      == 0) ||
					 (strcmp(argv[i], "--rename-files-test") == 0)) {
				ADVBLock     lock("schedule");
				ADVBProgList reclist;
				AString  	 templ  = argv[++i];
				bool     	 commit = (strcmp(argv[i - 1], "--rename-files") == 0);

				if (reclist.ReadFromFile(config.GetRecordedFile())) {
					uint_t i;
					bool changed = false;

					for (i = 0; i < proglist.Count(); i++) {
						ADVBProg& prog      = proglist.GetProgWritable(i);
						ADVBProg  *recprog;
						AString   filename  = prog.GetFilename();
						AString   filename1 = filename.PathPart().CatPath(prog.GenerateFilename(templ + "{sep}" + filename.Suffix()).FilePart());

						if ((filename1 != filename) && AStdFile::exists(filename) && ((recprog = reclist.FindUUIDWritable(prog)) != NULL)) {
							if		(!commit) config.printf("*TEST* Rename '%s' to '%s'", filename.str(), filename1.str());
							else if (rename(filename, filename1) == 0) {
								recprog->SetFilename(filename1);
								config.printf("Renamed '%s' to '%s'", filename.str(), filename1.str());
								changed = true;
							}
							else config.printf("Failed to rename '%s' to '%s'", filename.str(), filename1.str());
						}
					}

					if (changed) {
						if (commit) {
							if (!reclist.WriteToFile(config.GetRecordedFile())) {
								config.printf("Failed to write recorded programme list back!");
							}
						}
						else config.printf("NOT writing changes back to files");
					}
				}
				else config.printf("Failed to read recorded programme list");
			}
			else if (stricmp(argv[i], "--find-series") == 0) {
				AHash series;

				proglist.FindSeries(series);

				printf("Found series for %u programmes\n", series.GetItems());
				series.Traverse(&__DisplaySeries);
			}
			else if (stricmp(argv[i], "--set-recorded-flag") == 0) {
				ADVBLock lock("schedule");
				ADVBProgList reclist;

				if (reclist.ReadFromFile(config.GetRecordedFile())) {
					uint_t i;

					for (i = 0; i < reclist.Count(); i++) {
						reclist.GetProgWritable(i).SetRecorded();
					}

					if (!reclist.WriteToFile(config.GetRecordedFile())) {
						config.printf("Failed to write recorded programme list back!");
					}
				}
				else config.printf("Failed to read recorded programme list");
			}
			else if (stricmp(argv[i], "--find-recorded-programmes-on-disk") == 0) {
				ADVBLock lock("schedule");
				ADVBProgList reclist;
				AString  recdir = config.GetRecordingsDir();
				AString  dirs   = config.GetConfigItem("searchdirs", "");

				if (reclist.ReadFromFile(config.GetRecordedFile())) {
					uint_t i, j, n = dirs.CountLines(",");
					uint_t found = 0;
					
					for (j = 0; j < reclist.Count(); j++) {
						ADVBProg& prog = reclist.GetProgWritable(j);
						FILE_INFO info;
												
						if (!AStdFile::exists(prog.GetFilename())) {
							AString filename1 = prog.GenerateFilename();
							if (AStdFile::exists(filename1)) {
								prog.SetFilename(filename1);
								found++;
							}
						}
					}

					for (i = 0; i < n; i++) {
						AString path = recdir.CatPath(dirs.Line(i, ","));
						AList   dirs;

						dirs.Add(new AString(path));
						CollectFiles(path, "*", RECURSE_ALL_SUBDIRS, dirs, FILE_FLAG_IS_DIR, FILE_FLAG_IS_DIR);

						const AString *dir = AString::Cast(dirs.First());
						while (dir) {
							if (dir->FilePart() != "subs") {
								config.printf("Searching '%s'...", dir->str());

								for (j = 0; j < reclist.Count(); j++) {
									ADVBProg& prog = reclist.GetProgWritable(j);

									if (!AStdFile::exists(prog.GetFilename()) || config.GetRelativePath(prog.GetFilename()).Empty()) {
										AString filename1 = dir->CatPath(AString(prog.GetFilename()).FilePart());
										AString filename2 = filename1.Prefix() + ".mp4";
										
										if (AStdFile::exists(filename1)) {
											prog.SetFilename(filename1);
											found++;
										}
										else if (AStdFile::exists(filename2)) {
											prog.SetFilename(filename2);
											found++;
										}
									}
								}
							}

							dir = dir->Next();
						}
					}

					config.printf("Found %u previously orphaned programmes", found);

					if (found) {
						if (!reclist.WriteToFile(config.GetRecordedFile())) {
							config.printf("Failed to write recorded programme list back!");
						}
					}
				}
				else config.printf("Failed to read recorded programme list");
			}
			else if (stricmp(argv[i], "--check-disk-space") == 0) {
				ADataList patternlist;
				AString   errors;

				ADVBProgList::ReadPatterns(patternlist, errors);
				ADVBProgList::CheckDiskSpace(patternlist, true);
			}
			else if (stricmp(argv[i], "--update-recording-complete") == 0) {
				ADVBLock lock("schedule");
				ADVBProgList reclist;

				if (reclist.ReadFromFile(config.GetRecordedFile())) {
					uint_t i;

					for (i = 0; i < reclist.Count(); i++) {
						ADVBProg& prog = reclist.GetProgWritable(i);

						prog.SetRecordingComplete();
					}

					if (!reclist.WriteToFile(config.GetRecordedFile())) {
						config.printf("Failed to write recorded programme list back!");
					}
				}
			}
			else if (stricmp(argv[i], "--check-recording-file") == 0) {
				ADVBProgList::CheckRecordingFile();
			}
			else if ((stricmp(argv[i], "--set-ignore-flag") == 0) ||
					 (stricmp(argv[i], "--clr-ignore-flag") == 0)) {
				ADVBLock     lock("schedule");
				ADVBProgList reclist;
				bool         ignore = (stricmp(argv[i], "--set-ignore-flag") == 0);
				AString      patterns = argv[++i], errors;
				
				if (reclist.ReadFromFile(config.GetRecordedFile())) {
					ADVBProgList reslist;
					uint_t i, changed = 0;

					reclist.FindProgrammes(reslist, patterns, errors, (patterns.Pos("\n") >= 0) ? "\n" : ";");

					config.printf("Found %u programme%s", reslist.Count(), (reslist.Count() == 1) ? "" : "s");

					if (errors.Valid()) {
						config.printf("Errors:");
						
						uint_t i, n = errors.CountLines();
						for (i = 0; i < n; i++) config.printf("%s", errors.Line(i).str());
					}

					for (i = 0; i < reslist.Count(); i++) {
						ADVBProg *prog;

						if ((prog = reclist.FindUUIDWritable(reslist.GetProg(i))) != NULL) {
							if (prog->IgnoreRecording() != ignore) {
								config.printf("Changing '%s'", prog->GetQuickDescription().str());
								prog->SetIgnoreRecording(ignore);
								changed++;
							}
						}
						else config.printf("Failed to find programme '%s' in recordlist!", reslist.GetProg(i).GetQuickDescription().str());
					}

					config.printf("Changed %u programme%s", changed, (changed == 1) ? "" : "s");

					if (changed) {
						if (!reclist.WriteToFile(config.GetRecordedFile())) {
							config.printf("Failed to write recorded programme list back!");
						}
					}
				}
			}
			else if (stricmp(argv[i], "--force-failures") == 0) {
				ADVBLock     lock("schedule");
				ADVBProgList failureslist;

				if (!AStdFile::exists(config.GetRecordFailuresFile()) || failureslist.ReadFromFile(config.GetRecordFailuresFile())) {
					uint_t i;

					for (i = 0; i < proglist.Count(); i++) {
						failureslist.AddProg(proglist.GetProg(i), false);
					}

					for (i = 0; i < failureslist.Count(); i++) {
						ADVBProg& prog = failureslist.GetProgWritable(i);

						prog.ClearScheduled();
						prog.SetRecordingFailed();
					}
					
					if (!failureslist.WriteToFile(config.GetRecordFailuresFile())) {
						config.printf("Failed to write record failure list back!");
					}
				}
				else config.printf("Failed to read record failure list");
			}
			else if (stricmp(argv[i], "--test-notify") == 0) {
				uint_t i;

				for (i = 0; i < proglist.Count(); i++) {
					ADVBProg& prog = proglist.GetProgWritable(i);
					prog.SetNotify();
					prog.OnRecordSuccess();
				}
			}
			else if (stricmp(argv[i], "--change-user") == 0) {
				ADVBProgList reslist;
				AString      patterns = argv[++i], newuser = argv[++i], errors;
				uint_t i, changed = 0;

				proglist.FindProgrammes(reslist, patterns, errors, (patterns.Pos("\n") >= 0) ? "\n" : ";");

				printf("Found %u programme%s", reslist.Count(), (reslist.Count() == 1) ? "" : "s");

				if (errors.Valid()) {
					printf("Errors:");
					
					uint_t i, n = errors.CountLines();
					for (i = 0; i < n; i++) printf("%s", errors.Line(i).str());
				}

				for (i = 0; i < reslist.Count(); i++) {
					ADVBProg *prog;
					
					if ((prog = proglist.FindUUIDWritable(reslist.GetProg(i))) != NULL) {
						if (prog->GetUser() != newuser) {
							prog->SetUser(newuser);
							changed++;
						}
					}
				}
				
				printf("Changed %u programme%s", changed, (changed == 1) ? "" : "s");
			}
			else if (stricmp(argv[i], "--post-process") == 0) {
				uint_t i;

				for (i = 0; i < proglist.Count(); i++) {
					ADVBProg& prog = proglist.GetProgWritable(i);

					printf("Post processing '%s':\n", prog.GetQuickDescription().str());
					prog.PostProcess();
				}
			}
			else if (stricmp(argv[i], "--return-count") == 0) {
				res = proglist.Count();
			}
			else {
				fprintf(stderr, "Unrecognized option '%s'\n", argv[i]);
				exit(1);
			}
		}
	}
	
	return res;
}
