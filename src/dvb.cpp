
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
#include <rdlib/printtable.h>

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

typedef struct {
	const AString option;
	const AString args;
	const AString helptext;
} OPTION;

bool forcelogging = false;

AQuitHandler QuitHandler;

static void DisplaySeries(const ADVBProgList::SERIES& series)
{
	uint_t j;

	for (j = 0; j < series.list.size(); j++) {
		const AString& str = series.list[j];

		if (str.Valid()) printf("Programme '%s' series %u: %s\n", series.title.str(), j, str.str());
	}
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

static bool argsvalid(int argc, const char *argv[], int i, const OPTION& option)
{
	int  n     = option.args.CountWords();
	bool valid = (!(((i + 1) < argc) && (stricmp(argv[i + 1], "--help") == 0)) && ((i + n) < argc));

	if (!valid) {
		fprintf(stderr, "Incorrect arguments: %s %s \t%s\n", argv[i], option.args.str(), option.helptext.str());
	}

	return valid;
}

int main(int argc, const char *argv[])
{
	static const OPTION options[] = {
		{"--no-report-errors",                     "",                                "Do NOT report any errors during directory creation (MUST be first argument)"},
		{"-v[<n>]",								   "",								  "Increase verbosity by <n> or by 1"},
		{"--confdir",							   "",								  "Print conf directory"},
		{"--datadir",							   "",								  "Print data directory"},
		{"--logdir",							   "",								  "Print log directory"},
		{"--dirs",								   "",								  "Print important configuration directories"},
		{"--getxmltv",							   "<outputfile>",					  "Download XMLTV listings"},
		{"--dayformat",							   "<format>",						  AString("Format string for day (default '%')").Arg(ADVBProg::GetDayFormat())},
		{"--dateformat",						   "<format>",						  AString("Format string for date (default '%')").Arg(ADVBProg::GetDateFormat())},
		{"--timeformat",						   "<format>",						  AString("Format string for time (default '%')").Arg(ADVBProg::GetTimeFormat())},
		{"--fulltimeformat",					   "<format>",						  AString("Format string for fulltime (default '%')").Arg(ADVBProg::GetFullTimeFormat())},
		{"--set",								   "<var>=<val>",					  "Set variable in configuration file for this session only"},
		{"-u, --update",						   "<file>",						  "Update main listings with file <file>"},
		{"-l, --load",							   "",								  "Read listings from default file"},
		{"-r, --read",							   "<file>",						  "Read listings from file <file>"},
		{"--merge",								   "<file>",						  "Merge listings from file <file>, adding any programmes that dot not exist"},
		{"--modify-recorded",					   "<file>",						  "Merge listings from file <file> into recorded programmes, adding any programmes that dot not exist"},
		{"--modify-recorded-from-recording-host",  "",								  "Add recorded programmes from recording host into recorded programmes, adding any programmes that dot not exist"},
		{"--modify-scheduled-from-recording-host", "",								  "Modfy scheduled programmes from recording host into scheduled programmes"},
		{"--jobs",								   "",								  "Read programmes from scheduled jobs"},
		{"-w, --write",							   "<file>",						  "Write listings to file <file>"},
		{"--sort",								   "",								  "Sort list in chronological order"},
		{"--sort-by-fields",					   "<fieldlist>",					  "Sort list using field list"},
		{"--sort-rev",							   "",								  "Sort list in reverse-chronological order"},
		{"--write-text",						   "<file>",						  "Write listings to file <file> in text format"},
		{"--write-gnuplot",						   "<file>",						  "Write listings to file <file> in format usable by GNUPlot"},
		{"--email",								   "<recipient> <subject> <message>", "Email current list (if it is non-empty) to <recipient> using subject"},
		{"--fix-pound",							   "<file>",						  "Fix pound symbols in file"},
		{"--update-dvb-channels",				   "",								  "Update DVB channel assignments"},
		{"--update-dvb-channels-output-list",	   "",								  "Update DVB channel assignments and output list of SchedulesDirect channel ID's"},
		{"--update-dvb-channels-file",			   "",								  "Update DVB channels file from recording slave if it is newer"},
		{"--print-channels",					   "",								  "Output all channels"},
		{"--find-unused-channels",				   "",								  "Find listings and DVB channels that are unused"},
		{"--update-uuid",						   "",								  "Set UUID's on every programme"},
		{"--update-combined",					   "",								  "Create a combined list of recorded and scheduled programmes"},
		{"-L, --list",							   "",								  "List programmes in current list"},
		{"--list-to-file",						   "<file>",						  "List programmes in current list to file <file>"},
		{"--list-base64",						   "",								  "List programmes in current list in base64 format"},
		{"--list-channels",						   "",								  "List channels"},
		{"--cut-head",							   "<n>",							  "Cut <n> programmes from the start of the list"},
		{"--cut-tail",							   "<n>",							  "Cut <n> programmes from the end of the list"},
		{"--head",								   "<n>",							  "Keep <n> programmes at the start of the list"},
		{"--tail",								   "<n>",							  "Keep <n> programmes at the end of the list"},
		{"--limit",								   "<n>",							  "Limit list to <n> programmes, deleting programmes from the end of the list"},
		{"-f, --find",							   "<patterns>",					  "Find programmes matching <patterns>"},
		{"-F, --find-with-file",				   "<pattern-file>",				  "Find programmes matching patterns in patterns file <pattern-file>"},
		{"-R, --find-repeats",				       "",								  "For each programme in current list, list repeats"},
		{"--find-similar",						   "<file>",						  "For each programme in current list, find first similar programme in <file>"},
		{"--find-differences",					   "<file1> <file2>",				  "Find programmes in <file1> but not in <file2> or in <file2> and not in <file1>"},
		{"--find-in-file1-only",				   "<file1> <file2>",				  "Find programmes in <file1> but not in <file2>"},
		{"--find-in-file2-only",				   "<file1> <file2>",				  "Find programmes in <file2> and not in <file1>"},
		{"--find-similarities",					   "<file1> <file2>",				  "Find programmes in both <file1> and <file2>"},
		{"--describe-pattern",					   "<patterns>",					  "Describe patterns as parsed"},
		{"--delete-all",						   "",								  "Delete all programmes"},
		{"--delete",							   "<patterns>",					  "Delete programmes matching <patterns>"},
		{"--delete-with-file",					   "<pattern-file>",				  "Delete programmes matching patterns in patterns file <pattern-file>"},
		{"--delete-recorded",					   "",								  "Delete programmes that have been recorded"},
		{"--delete-using-file",					   "<file>",						  "Delete programmes that are similar to those in file <file>"},
		{"--delete-similar",					   "",								  "Delete programmes that are similar to others in the list"},
		{"--schedule",							   "",								  "Schedule and create jobs for default set of patterns on the main listings file"},
		{"--write-scheduled-jobs",				   "",								  "Create jobs for current scheduled list"},
		{"--start-time",						   "<time>",						  "Set start time for scheduling"},
		{"--fake-schedule",						   "",								  "Pretend to schedule (write files) for default set of patterns on the main listings file"},
		{"--fake-schedule-list",				   "",								  "Pretend to schedule (write files) for current list of programmes"},
		{"--unschedule-all",					   "",								  "Unschedule all programmes"},
		{"-S, --schedule-list",					   "",								  "Schedule current list of programmes"},
		{"--record",							   "<prog>",						  "Record programme <prog> (Base64 encoded)"},
		{"--record-title",						   "<title>",						  "Schedule to record programme by title that has already started or starts within the next hour"},
		{"--schedule-record",					   "<base64>",						  "Schedule specified programme (Base64 encoded)"},
		{"--add-recorded",						   "<prog>",						  "Add recorded programme <prog> to recorded list (Base64 encoded)"},
		{"--recover-recorded-from-temp",		   "",								  "Create recorded programme entries from files in temp folder"},
		{"--card",								   "<card>",						  "Set VIRTUAL card for subsequent scanning and PID operations (default 0)"},
		{"--dvbcard",							   "<card>",						  "Set PHYSICAL card for subsequent scanning and PID operations (default 0)"},
		{"--pids",								   "<channel>",						  "Find PIDs (all streams) associated with channel <channel>"},
		{"--scan",								   "<freq>[,<freq>...]",			  "Scan frequencies <freq>MHz for DVB channels"},
		{"--scan-all",							   "",								  "Scan frequencies from existing DVB channels"},
		{"--scan-range",						   "<start-freq> <end-freq> <step>",  "Scan frequencies in specified range for DVB channels"},
		{"--find-channels",						   "",								  "Run Perl update channels script and update channels accordingly"},
		{"--find-cards",						   "",								  "Find DVB cards"},
		{"--change-filename",					   "<filename1> <filename2>",		  "Change filename of recorded progamme with filename <filename1> to <filename2>"},
		{"--change-filename-regex",				   "<filename1> <filename2>",		  "Change filename of recorded progamme with filename <filename1> to <filename2> (where <filename1> can be a regex and <filename2> can be an expansion)"},
		{"--change-filename-regex-test",		   "<filename1> <filename2>",		  "Like above but do not commit changes"},
		{"--find-recorded-programmes-on-disk",	   "",								  "Search for recorded programmes in 'searchdirs' and update any filenames"},
		{"--find-series",						   "",								  "Find series' in programme list"},
		{"--fix-data-in-recorded",				   "",								  "Fix incorrect metadata of certain programmes and rename files as necessary"},
		{"--set-recorded-flag",					   "",								  "Set recorded flag on all programmes in recorded list"},
		{"--list-flags",						   "",								  "List flags that can be used for --set-flag and --clr-flag"},
		{"--set-flag",							   "<flag> <patterns>",				  "Set flag <flag> on programmes matching pattern"},
		{"--clr-flag",							   "<flag> <patterns>",				  "Clear flag <flag> on programmes matching pattern"},
		{"--check-disk-space",					   "",								  "Check disk space for all patterns"},
		{"--update-recording-complete",			   "",								  "Update recording complete flag in every recorded programme"},
		{"--check-recording-file",				   "",								  "Check programmes in running list to ensure they should remain in there"},
		{"--force-failures",					   "",								  "Add current list to recording failures (setting failed flag)"},
		{"--show-encoding-args",				   "",								  "Show encoding arguments for programmes in current list"},
		{"--show-file-format",					   "",								  "Show file format of encoded programme"},
		{"--use-converted-filename",			   "",								  "Change filename to that of converted file, without actually converting file"},
		{"--set-dir",							   "<patterns> <newdir>",			  "Change directory for programmes in current list that match <patterns> to <newdir>"},
		{"--regenerate-filename",				   "<patterns>",					  "Change filename of current list of programmes that match <pattern> (dependant on converted state), renaming files (in original directory OR current directory)"},
		{"--regenerate-filename-test",			   "<patterns>",					  "Test version of the above, will make no changes to programme list or move files"},
		{"--delete-files",						   "",								  "Delete encoded files from programmes in current list"},
		{"--delete-from-record-lists",			   "<uuid>",						  "Delete programme with UUID <uuid> from record lists (recorded and record failues)"},
		{"--record-success",					   "",								  "Run recordsuccess command on programmes in current list"},
		{"--change-user",						   "<patterns> <newuser>",			  "Change user of programmes matching <patterns> to <newuser>"},
		{"--change-dir",						   "<patterns> <newdir>",			  "Change direcotry of programmes matching <patterns> to <newdir>"},
		{"--get-and-convert-recorded",			   "",								  "Pull and convert any recordings from recording host"},
		{"--force-convert-files",				   "",								  "convert files in current list, even if they have already been converted"},
		{"--update-recordings-list",			   "",								  "Pull list of programmes being recorded"},
		{"--check-recording-now",				   "",								  "Check to see if programmes that should be being recording are recording"},
		{"--calc-trend",						   "<start-date>",					  "Calculate average programmes per day and trend line based on programmes after <start-date> in current list"},
		{"--gen-graphs",						   "",								  "Generate graphs from recorded data"},
		{"--assign-episodes",					   "",								  "Assign episodes to current list"},
		{"--reset-assigned-episodes",			   "",								  "Reset assigned episodes to current list"},
		{"--count-hours",						   "",								  "Count total hours of programmes in current list"},
		{"--find-gaps",							   "",								  "Find gap from now until the next working for each card"},
		{"--find-new-programmes",				   "",								  "Find programmes that have been scheduled that are either new or from a new series"},
		{"--check-video-files",					   "",								  "Check archived video files for errors"},
		{"--stream",							   "<text>",						  "Stream DVB channel or programme being recorded <text> to mplayer (or other player)"},
		{"--rawstream",							   "<text>",						  "Stream DVB channel or programme being recorded <text> to console (for piping to arbitrary programs)"},
		{"--return-count",						   "",								  "Return programme list count in error code"},
	};
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBProgList proglist;
	ADVBProg  prog;	// ensure ADVBProg initialisation takes place
	ADateTime starttime = ADateTime().TimeStamp(true);
	uint_t dvbcard   = 0;
	uint_t verbosity = 0;
	uint_t nopt, nsubopt;
	bool   dvbcardspecified = false;
	int    i;
	int    res = 0;

	(void)prog;

	if (((argc < 2) || (strcmp(argv[1], "--no-report-errors") != 0)) &&
		config.ReportDirectoryCreationErrors()) {
		res = -1;
	}

#if 0
	if (argc == 1) {
		ADVBLock lock("dvbfiles");
		ADVBProgList list;

		if (list.ReadFromFile(config.GetRecordedFile())) {
			uint_t i;
			uint_t n = 0;

			for (i = 0; i < list.Count(); i++) {
				ADVBProg& prog = list.GetProgWritable(i);
				AString filename = prog.GetFilename();

				if (filename.FilePart() == ".") {
					AString convertedfilename   = prog.GenerateFilename(true);
					AString unconvertedfilename = prog.GenerateFilename(false);
					if (AStdFile::exists(convertedfilename)) {
						prog.SetFilename(convertedfilename);
					}
					else if (AStdFile::exists(unconvertedfilename)) {
						prog.SetFilename(unconvertedfilename);
					}
					else if (prog.GetStartDT() < ADateTime("today,00:00:00")) {
						prog.SetFilename(convertedfilename);
					}
					printf("Changed '%s' from '%s' to '%s'\n", prog.GetQuickDescription().str(), filename.str(), prog.GetFilename());
					n++;
				}
			}

			if (n > 0) {
				list.WriteToFile(config.GetRecordedFile());
			}
		}
	}
#endif

	if ((argc == 1) || ((argc > 1) && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0)))) {
		std::vector<AString> optionlist;
		uint_t maxwid = 0;

		printf("Usage: dvb {<file>|<cmd> ...}\n");
		printf("Where <cmd> is:\n");

		for (nopt = 0; nopt < NUMBEROF(options); nopt++) {
			const OPTION& option = options[nopt];
			AString text;
			uint_t nsubopts = option.option.CountColumns();

			for (nsubopt = 0; nsubopt < nsubopts; nsubopt++) {
				if (text.Valid()) text += ", ";
				text += option.option.Column(nsubopt).Words(0);
				if (option.args.Valid()) text += " " + option.args;
			}

			optionlist.push_back(text);
			maxwid = std::max(maxwid, (uint_t)text.len());
		}

		maxwid++;

		for (nopt = 0; nopt < NUMBEROF(options); nopt++) {
			const OPTION& option = options[nopt];

			printf("\t%s%s\t%s\n",
				   optionlist[nopt].str(),
				   AString(" ").Copies(maxwid - optionlist[nopt].len()).str(),
				   option.helptext.str());
		}
	}
	else {
		for (i = 1; (i < argc) && !HasQuit(); i++) {
			bool valid = false;

			if (strncmp(argv[i], "-v", 2) == 0) {
				uint_t inc = (uint_t)AString(argv[i] + 2);
				verbosity += inc ? inc : 1;
				continue;
			}

			for (nopt = 0; nopt < NUMBEROF(options); nopt++) {
				const OPTION& option = options[nopt];
				uint_t nsubopts = option.option.CountColumns();

				for (nsubopt = 0; nsubopt < nsubopts; nsubopt++) {
					if (strcmp(argv[i], option.option.Column(nsubopt).Words(0).str()) == 0) {
						valid = argsvalid(argc, argv, i, option);
						break;
					}
				}

				if (nsubopt < nsubopts) break;
			}

			if (nopt == NUMBEROF(options)) {
				fprintf(stderr, "Unrecognized option: '%s'\n", argv[i]);
				break;
			}

			if (!valid) break;

			if		(strcmp(argv[i], "--confdir") == 0) printf("%s\n", config.GetConfigDir().str());
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
			else if (strcmp(argv[i], "--dayformat") == 0) {
				ADVBProg::SetDayFormat(argv[++i]);
			}
			else if (strcmp(argv[i], "--dateformat") == 0) {
				ADVBProg::SetDateFormat(argv[++i]);
			}
			else if (strcmp(argv[i], "--timeformat") == 0) {
				ADVBProg::SetTimeFormat(argv[++i]);
			}
			else if (strcmp(argv[i], "--fulltimeformat") == 0) {
				ADVBProg::SetFullTimeFormat(argv[++i]);
			}
			else if (strcmp(argv[i], "--set") == 0) {
				AString str = argv[++i];
				int p;

				if ((p = str.Pos("=")) >= 0) {
					AString var = str.Left(p);
					AString val = str.Mid(p + 1).DeQuotify().DeEscapify();
					config.Set(var, val);
				}
				else fprintf(stderr, "Configuration setting '%s' invalid, format should be '<var>=<val>'", str.str());
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

					if (config.DeleteProgrammesWithNoDVBChannel()) {
						uint_t j, deleted = 0;

						config.printf("Removing programmes without a DVB channel...");

						for (j = 0; !HasQuit() && (j < proglist.Count()); ) {
							if (AString(proglist[j].GetDVBChannel()).Empty()) {
								proglist.DeleteProg(j);
								deleted++;
							}
							else j++;
						}

						config.printf("%u programmes deleted", deleted);
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
					fprintf(stderr, "Failed to read programme list from '%s'\n", filename.str());
				}
			}
			else if ((strcmp(argv[i], "--update-dvb-channels") == 0) ||
					 (strcmp(argv[i], "--update-dvb-channels-output-list") == 0)) {
				std::map<uint_t, bool> sdchannelids;

				config.printf("Updating DVB channels...");
				proglist.UpdateDVBChannels(&sdchannelids);

				if (strcmp(argv[i], "--update-dvb-channels-output-list") == 0) {
					std::map<uint_t, bool>::iterator it;

					printf("----Channel ID's----\n");
					for (it = sdchannelids.begin(); it != sdchannelids.end(); ++it) {
						printf("channel=%u\n", it->first);
					}
					printf("----Channel ID's----\n");
				}
			}
			else if (strcmp(argv[i], "--update-dvb-channels-file") == 0) {
				if (config.GetRecordingSlave().Valid()) {
					ADVBLock lock("dvbfiles");
					AString filename    = config.GetDVBChannelsJSONFile();
					AString dstfilename = config.GetTempFile(filename.FilePart().Prefix(), "." + filename.Suffix());

					if (GetFileFromRecordingSlave(filename, dstfilename)) {
						FILE_INFO info1, info2;

						if (::GetFileInfo(filename,    &info1) &&
							::GetFileInfo(dstfilename, &info2) &&
							(info2.WriteTime > info1.WriteTime)) {
							// replace local channels file with remote one
							::remove(filename);
							::rename(dstfilename, filename);
							config.printf("Replaced local channels file with one from recording slave");
						}
						else config.printf("Local channels file up to date");
					}
				}
				else printf("No need to update DVB channels file: no recording slave\n");
			}
			else if (strcmp(argv[i], "--print-channels") == 0) {
				const ADVBChannelList& channellist = ADVBChannelList::Get();
				TABLE table;
				size_t j;
				uint_t i;

				table.headerscentred = true;

				{
					TABLEROW row;

					row.push_back("LCN");
					row.push_back("SD Channel");
					row.push_back("XMLTV Channel ID");
					row.push_back("XMLTV Channel Name");
					row.push_back("XMLTV Converted Name");
					row.push_back("DVB Channel Name");
					row.push_back("DVB Converted Name");
					row.push_back("DVB Frequency");
					row.push_back("DVB PID List");

					table.rows.push_back(row);

					table.justify.resize(row.size());

#if 0
					for (j = 0; j < table.justify.size(); j++) {
						table.justify[j] = 2;
					}
#endif
				}

				for (i = 0; i < channellist.GetLCNCount(); i++) {
					const ADVBChannelList::CHANNEL *chan;

					if ((chan = channellist.GetChannelByLCN(i)) != NULL) {
						TABLEROW row;

						table.justify[row.size()] = 2;
						row.push_back(chan->dvb.lcn ? AString("%;").Arg(chan->dvb.lcn) : AString());
						table.justify[row.size()] = 2;
						row.push_back(chan->xmltv.sdchannelid ? AString("%;").Arg(chan->xmltv.sdchannelid) : AString());
						row.push_back(chan->xmltv.channelid);
						row.push_back(chan->xmltv.channelname);
						row.push_back(chan->xmltv.convertedchannelname);
						row.push_back(chan->dvb.channelname);
						row.push_back(chan->dvb.convertedchannelname);

						table.justify[row.size()] = 1;
						row.push_back(chan->dvb.freq ? AString("%;").Arg(chan->dvb.freq) : AString());

						AString pidlist;
						for (j = 0; j < chan->dvb.pidlist.size(); j++) {
							if (j == 0) {
								pidlist.printf("%u", chan->dvb.pidlist[j]);
							}
							else pidlist.printf(", %u", chan->dvb.pidlist[j]);
						}
						table.justify[row.size()] = 2;
						row.push_back(pidlist);

						table.rows.push_back(row);
					}
				}

				PrintTable(*Stdout, table);
			}
			else if (strcmp(argv[i], "--find-unused-channels") == 0) {
				const ADVBChannelList& channellist = ADVBChannelList::Get();
				std::map<AString,uint_t> dvbchannels;
				std::map<AString,uint_t> missingchannels;
				uint_t j, n = channellist.GetChannelCount();

				for (j = 0; j < n; j++) {
					dvbchannels[channellist.GetChannel(j)->dvb.channelname] = 0;
				}

				for (j = 0; j < proglist.Count(); j++) {
					const ADVBProg& prog       = proglist[j];
					const AString   dvbchannel = prog.GetDVBChannel();

					if (dvbchannel.Valid()) {
						//printf("Programme '%s' has DVB channel '%s'\n", prog.GetQuickDescription().str(), dvbchannel.str());
						dvbchannels[dvbchannel]++;
					}
					else missingchannels[prog.GetChannel()]++;
				}

				std::map<AString,uint_t>::iterator it;
				for (it = dvbchannels.begin(); it != dvbchannels.end(); ++it) {
					if (it->second == 0) {
						printf("DVB channel '%s' has no programmes associated with it\n", it->first.str());
					}
				}
				for (it = missingchannels.begin(); it != missingchannels.end(); ++it) {
					printf("Listings channel '%s' has no DVB channel associated with it\n", it->first.str());
				}
			}
			else if (strcmp(argv[i], "--update-uuid") == 0) {
				uint_t j;

				for (j = 0; j < proglist.Count(); j++) {
					proglist.GetProgWritable(j).SetUUID();
				}
			}
			else if (strcmp(argv[i], "--update-combined") == 0) {
				if (!ADVBProgList::CreateCombinedFile()) {
					res = -1;
				}
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
				else fprintf(stderr, "Failed to read programmes from '%s'\n", argv[i]);
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

				for (j = 0; (j < reslist.Count()) && !HasQuit(); j++) {
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

					for (j = 0; (j < reslist.Count()) && !HasQuit(); j++) {
						const ADVBProg& prog = reslist.GetProg(j);

						if (proglist.DeleteProg(prog)) {
							printf("Deleted '%s'\n", prog.GetDescription(verbosity).str());
						}
						else fprintf(stderr, "Failed to delete '%s'!\n", prog.GetDescription(verbosity).str());
					}
				}
				else fprintf(stderr, "Failed to read patterns from '%s'\n", filename.str());
			}
			else if ((strcmp(argv[i], "--delete-recorded") == 0) ||
					 (strcmp(argv[i], "--delete-using-file") == 0)) {
				AString      filename = config.GetRecordedFile();
				ADVBProgList proglist2;

				if (strcmp(argv[i], "--delete-using-file") == 0) filename = config.GetNamedFile(argv[++i]);

				if (proglist2.ReadFromFile(filename)) {
					uint_t j, ndeleted = 0;

					for (j = 0; (j < proglist.Count()) && !HasQuit(); ) {
						if (proglist2.FindSimilar(proglist.GetProg(j))) {
							proglist.DeleteProg(j);
							ndeleted++;
						}
						else j++;
					}

					printf("Deleted %u programmes\n", ndeleted);
				}
				else fprintf(stderr, "Failed to read file '%s'\n", filename.str());
			}
			else if (strcmp(argv[i], "--delete-similar") == 0) {
				uint_t j, ndeleted = 0;

				for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
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
				uint_t j, nnodvb = 0;

				for (j = 0; j < list.GetChannelCount(); j++) {
					const ADVBChannelList::CHANNEL *channel = list.GetChannel(j);

					printf("Channel %u/%u (LCN %u): '%s' (DVB '%s', XMLTV channel-id '%s')\n", j + 1, list.GetChannelCount(), channel->dvb.lcn, channel->xmltv.channelname.str(), channel->dvb.channelname.str(), channel->xmltv.channelid.str());

					nnodvb += channel->dvb.channelname.Empty();
				}
				printf("%u/%u channels have NO DVB channel\n", nnodvb, list.GetChannelCount());
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
			else if (stricmp(argv[i], "--email") == 0) {
				AString recipient = argv[++i];
				AString subject   = argv[++i];
				AString message   = AString(argv[++i]).DeEscapify();

				if (proglist.EmailList(recipient, subject, message, verbosity)) {
					printf("List emailed successfully (if not empty)\n");
				}
				else {
					res = -1;
				}
			}
			else if ((strcmp(argv[i], "--sort") == 0) || (strcmp(argv[i], "--sort-rev") == 0)) {
				bool reverse = (strcmp(argv[i], "--sort-rev") == 0);
				proglist.Sort(reverse);
				printf("%sorted list chronologically\n", reverse ? "Reverse s" : "S");
			}
			else if (strcmp(argv[i], "--sort-by-fields") == 0) {
				ADVBProg::FIELDLIST fieldlist;

				if (ADVBProg::ParseFieldList(fieldlist, argv[++i])) {
					proglist.Sort(fieldlist);
					printf("Sorted list by fields\n");
				}
				else {
					printf("Failed to parse field list for sort\n");
					res = -1;
				}
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
					const AString filename     = prog.GetFilename();
					const AString tempfilename = prog.GetTempFilename();
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

					if (!AStdFile::exists(filename) &&
						AStdFile::exists(tempfilename)) {
						config.printf("File '%s' does not exist but '%s' does, renaming...",
									  filename.str(),
									  tempfilename.str());
						if (rename(tempfilename, filename) != 0) {
							config.printf("Failed to rename '%s' to '%s'", tempfilename.str(), filename.str());
						}
					}

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
					ADVBProgList::RemoveFromList(config.GetRecordingFile(), prog);
				}
				else config.printf("Failed to decode programme '%s'\n", progstr.str());
			}
			else if (strcmp(argv[i], "--recover-recorded-from-temp") == 0) {
				ADVBLock     dvblock("files");
				ADVBProgList reclist;
				AString      reclistfilename = config.GetRecordedFile();
				uint_t       nchanges = 0;

				if (reclist.ReadFromFile(reclistfilename)) {
					AList files;

					reclist.CreateHash();

					CollectFiles(config.GetRecordingsStorageDir(), "*." + config.GetRecordedFileSuffix(), 0, files, FILE_FLAG_IS_DIR, 0, &QuitHandler);

					printf("--------------------------------------------------------------------------------\n");

					// iterate through files
					const AString *str = AString::Cast(files.First());
					while (str && !HasQuit()) {
						// for each file, attempt to find a file in the current list whose's unconverted file has the same filename
						const AString& filename = *str;
						uint_t j;

						printf("Looking for programme for '%s'...\n\n", filename.str());
						for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
							// ensure programme is already marked as recorded
							// and the unconverted filenames match
							if (!proglist[j].IsRecorded() &&
								(filename == proglist[j].GenerateFilename())) {
								const ADVBProg *sprog;

								// make sure it's not already in the recorded list
								if ((sprog = reclist.FindUUID(proglist[j])) != NULL) {
									printf("Already recorded as '%s'\n", sprog->GetQuickDescription().str());
									printf("--------------------------------------------------------------------------------\n");
									break;
								}
								else {
									ADVBProg prog = proglist[j];	// note: take copy of prog

									printf("Found as '%s':\n", prog.GetQuickDescription().str());

									prog.ClearScheduled();
									prog.ClearRecording();
									prog.ClearRunning();
									prog.SetRecorded();
									prog.GenerateRecordData(0);
									prog.SetFilename(filename);	// must set the filename as GenerateRecordData() will set a different one
									prog.SetActualStart(prog.GetRecordStart());
									prog.SetActualStop(prog.GetRecordStop());
									prog.SetRecordingComplete();
									prog.UpdateFileSize();

									reclist.AddProg(prog);

									printf("\nAdded programme:\n\n%s", prog.GetDescription(2).str());

									nchanges++;
									break;
								}
							}
						}

						printf("\n");

						str = str->Next();
					}

					if (nchanges) {
						printf("%u programmes added to recorded list\n", nchanges);
						if (!reclist.WriteToFile(reclistfilename)) {
							fprintf(stderr, "Failed to write recorded list\n");
						}
					}
					else if (files.Count() > 0) printf("No suitable programmes found\n");
				}
				else fprintf(stderr, "Failed to read recorded file list\n");

			}
			else if (strcmp(argv[i], "--card") == 0) {
				const uint_t newcard = (uint_t)AString(argv[++i]);

				config.GetPhysicalDVBCard(0);
				if (config.GetMaxDVBCards()) {
					if (newcard < config.GetMaxDVBCards()) {
						dvbcard = config.GetPhysicalDVBCard(newcard);
						dvbcardspecified = true;
						printf("Switched to using virtual card %u which is physical card %u\n", newcard, dvbcard);
					}
					else fprintf(stderr, "Illegal virtual DVB card specified, must be 0..%u\n", config.GetMaxDVBCards() - 1);
				}
				else fprintf(stderr, "No virtual DVB cards available!\n");
			}
			else if (strcmp(argv[i], "--dvbcard") == 0) {
				dvbcard = (uint_t)AString(argv[++i]);
				dvbcardspecified = true;

				config.GetPhysicalDVBCard(0);
				printf("Switched to using physical card %u\n", dvbcard);
			}
			else if (strcmp(argv[i], "--pids") == 0) {
				ADVBChannelList& list = ADVBChannelList::Get();
				const AString 	 channel = argv[++i];
				AString 	  	 pids;

				if (config.GetRecordingSlave().Valid()) {
					// MUST write channels if they have been updated
					list.Write();

					printf("Fetching PIDs from recording slave '%s'\n", config.GetRecordingSlave().str());
					RunRemoteCommandGetFile(AString("dvb --dvbcard %; --pids \"%;\"").Arg(dvbcard).Arg(channel).EndArgs(), config.GetDVBChannelsJSONFile());

					// force re-reading after remote command
					list.Read();
				}

				// get PIDs (only update if recording slave is invalid)
				list.GetPIDList(dvbcard, channel, pids, config.GetRecordingSlave().Empty());

				printf("pids for '%s': %s\n", channel.str(), pids.str());
			}
			else if (strcmp(argv[i], "--scan") == 0) {
				ADVBChannelList& list = ADVBChannelList::Get();
				const AString freqs = argv[++i];

				if (config.GetRecordingSlave().Valid()) {
					// MUST write channels if they have been updated
					list.Write();

					printf("Scanning on recording slave '%s'\n", config.GetRecordingSlave().str());
					RunRemoteCommandGetFile(AString("dvb --dvbcard %; --scan \"%;\"").Arg(dvbcard).Arg(freqs).EndArgs(), config.GetDVBChannelsJSONFile());

					// force re-reading after remote command
					list.Read();
				}
				else {
					uint_t j, n = freqs.CountColumns();
					for (j = 0; j < n; j++) {
						list.Update(dvbcard, (uint32_t)(1.0e6 * (double)freqs.Column(j) + .5), true);
					}
				}
			}
			else if (strcmp(argv[i], "--scan-all") == 0) {
				ADVBChannelList& list = ADVBChannelList::Get();

				if (config.GetRecordingSlave().Valid()) {
					// MUST write channels if they have been updated
					list.Write();

					printf("Scanning on recording slave '%s'\n", config.GetRecordingSlave().str());
					RunRemoteCommandGetFile(AString("dvb --dvbcard %; --scan-all").Arg(dvbcard).EndArgs(), config.GetDVBChannelsJSONFile());

					// force re-reading after remote command
					list.Read();
				}
				else list.UpdateAll(dvbcard, true);
			}
			else if (strcmp(argv[i], "--scan-range") == 0) {
				ADVBChannelList& list = ADVBChannelList::Get();
				const AString f1str = AString(argv[++i]);
				const AString f2str = AString(argv[++i]);
				const AString ststr = AString(argv[++i]);

				if (config.GetRecordingSlave().Valid()) {
					// MUST write channels if they have been updated
					list.Write();

					printf("Scanning on recording slave '%s'\n", config.GetRecordingSlave().str());
					RunRemoteCommandGetFile(AString("dvb --dvbcard %; --scan-range %; %; %;").Arg(dvbcard).Arg(f1str).Arg(f2str).Arg(ststr).EndArgs(), config.GetDVBChannelsJSONFile());

					// force re-reading after remote command
					list.Read();
				}
				else {
					const double f1   = (double)f1str;
					const double f2   = (double)f2str;
					const double step = (double)ststr;
					double f;

					for (f = f1; f <= f2; f += step) {
						list.Update(dvbcard, (uint32_t)(1.0e6 * f + .5), true);
					}
				}
			}
			else if (strcmp(argv[i], "--find-channels") == 0) {
				ADVBChannelList& list = ADVBChannelList::Get();

				if (config.GetRecordingSlave().Valid()) {
					// MUST write channels if they have been updated
					list.Write();

					printf("Finding channels on recording slave '%s'\n", config.GetRecordingSlave().str());

					// run update script on recording slave then pull the results back
					RunRemoteCommandGetFile("updatedvbchannels.pl", config.GetDVBChannelsFile());
				}
				else {
					// run update script locally
					RunAndLogCommand("updatedvbchannels.pl");
				}

				// read and merge updated channels
				list.Read();
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
				ADVBProgList::SERIESLIST series;
				ADVBProgList::SERIESLIST::iterator it;

				proglist.FindSeries(series);

				printf("Found series for %u programmes\n", (uint_t)series.size());
				for (it = series.begin(); it != series.end(); ++it) {
					DisplaySeries(it->second);
				}
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

				// make sure the same is done on the recording slave
				if (config.GetRecordingSlave().Valid()) {
					RunAndLogRemoteCommand("dvb --check-recording-file");
				}
			}
			else if (stricmp(argv[i], "--list-flags") == 0) {
				std::vector<AString> flags;
				size_t i;

				ADVBProg::GetFlagList(flags, false);

				printf("Flags:\n");
				for (i = 0; i < flags.size(); i++) {
					printf("%s\n", flags[i].str());
				}
			}
			else if ((stricmp(argv[i], "--set-flag") == 0) ||
					 (stricmp(argv[i], "--clr-flag") == 0)) {
				ADVBProgList reslist;
				bool    set      = (stricmp(argv[i], "--set-flag") == 0);
				AString flagname = argv[++i];
				AString patterns = argv[++i], errors;
				uint_t j, nchanged = 0;

				if (ADVBProg::IsFlagNameValid(flagname)) {
					proglist.FindProgrammes(reslist, patterns, errors, (patterns.Pos("\n") >= 0) ? "\n" : ";");

					printf("Found %u programme%s\n", reslist.Count(), (reslist.Count() == 1) ? "" : "s");

					if (errors.Valid()) {
						printf("Errors:\n");

						uint_t n = errors.CountLines();
						for (j = 0; j < n; j++) printf("%s\n", errors.Line(j).str());
					}

					for (j = 0; (j < reslist.Count()) && !HasQuit(); j++) {
						ADVBProg *prog;

						if ((prog = proglist.FindUUIDWritable(reslist.GetProg(j))) != NULL) {
							if (prog->GetFlag(flagname) != set) {
								printf("Changing '%s'\n", prog->GetQuickDescription().str());
								prog->SetFlag(flagname, set);
								nchanged++;
							}
							else printf("NOT changing '%s', flag is already %s\n", prog->GetQuickDescription().str(), prog->GetFlag(flagname) ? "set" : "clear");
						}
						else printf("Failed to find programme '%s' in list!\n", reslist.GetProg(j).GetQuickDescription().str());
					}

					printf("Changed %u programme%s\n", nchanged, (nchanged == 1) ? "" : "s");
				}
				else {
					fprintf(stderr, "Flag name '%s' is not valid, use --list-flags to list valid flags\n", flagname.str());
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
						prog.SetRecordFailed();
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
				AString      patterns = argv[++i], newvalue = argv[++i], errors;
				uint_t		 j;

				proglist.FindProgrammes(reslist, patterns, errors, (patterns.Pos("\n") >= 0) ? "\n" : ";");

				printf("Found %u programme%s\n", reslist.Count(), (reslist.Count() == 1) ? "" : "s");

				if (errors.Valid()) {
					printf("Errors:");

					uint_t n = errors.CountLines();
					for (j = 0; j < n; j++) config.printf("%s", errors.Line(j).str());
				}
				else {
					uint_t nchanged = 0;

					for (j = 0; (j < reslist.Count()) && !HasQuit(); j++) {
						ADVBProg *prog;

						if ((prog = proglist.FindUUIDWritable(reslist.GetProg(j))) != NULL) {
							if (prog->GetUser() != newvalue) {
								prog->SetUser(newvalue);
								nchanged++;
							}
						}
					}

					printf("Changed %u programme%s\n", nchanged, (nchanged == 1) ? "" : "s");
				}
			}
			else if (stricmp(argv[i], "--change-dir") == 0) {
				ADVBProgList reslist;
				AString      patterns = argv[++i], newvalue = argv[++i], errors;
				uint_t		 j;

				proglist.FindProgrammes(reslist, patterns, errors, (patterns.Pos("\n") >= 0) ? "\n" : ";");

				printf("Found %u programme%s\n", reslist.Count(), (reslist.Count() == 1) ? "" : "s");

				if (errors.Valid()) {
					printf("Errors:");

					uint_t n = errors.CountLines();
					for (j = 0; j < n; j++) config.printf("%s", errors.Line(j).str());
				}
				else {
					uint_t nchanged = 0;

					for (j = 0; (j < reslist.Count()) && !HasQuit(); j++) {
						ADVBProg *prog;

						if ((prog = proglist.FindUUIDWritable(reslist.GetProg(j))) != NULL) {
							if (prog->GetDir() != newvalue) {
								prog->SetDir(newvalue);
								nchanged++;
							}
						}
					}

					printf("Changed %u programme%s\n", nchanged, (nchanged == 1) ? "" : "s");
				}
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
				ADVBProgList reslist;
				AString      patterns = argv[++i], errors;
				uint_t		 j;

				proglist.FindProgrammes(reslist, patterns, errors, (patterns.Pos("\n") >= 0) ? "\n" : ";");

				printf("Found %u programme%s\n", reslist.Count(), (reslist.Count() == 1) ? "" : "s");

				if (errors.Valid()) {
					printf("Errors:");

					uint_t n = errors.CountLines();
					for (j = 0; j < n; j++) config.printf("%s", errors.Line(j).str());
				}
				else {
					uint_t ntested = 0, nchanged = 0;

					for (j = 0; (j < reslist.Count()) && !HasQuit(); j++) {
						ADVBProg *pprog;

						if ((pprog = proglist.FindUUIDWritable(reslist.GetProg(j))) != NULL) {
							ADVBProg& prog = *pprog;
							AString   oldfilename = prog.GetFilename();

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

								nchanged++;
							}

							ntested++;
						}
					}

					printf("%u programme%s tested, %u filename%s changed (%u unchanged)\n",
						   ntested, (ntested == 1) ? "" : "s",
						   nchanged, (nchanged == 1) ? "" : "s",
						   ntested - nchanged);
				}
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

				for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
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

				for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
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
				uint_t j;

				for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
					proglist.GetProgWritable(j).SetAssignedEpisode(0);
				}
			}
			else if (stricmp(argv[i], "--find-episode-sequences") == 0) {
				std::map<AString, std::map<AString, std::vector<AString> > > programmes;
				std::map<AString, const std::vector<AString> *> bestlist;
				std::map<AString, uint16_t> episodeids;
				uint_t j;

				for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
					const ADVBProg& prog = proglist.GetProg(j);
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

				for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
					ADVBProg& prog = proglist.GetProgWritable(j);
					AString   epid = prog.GetEpisodeID();
					std::map<AString, uint16_t>::iterator it;

					if ((it = episodeids.find(epid)) != episodeids.end()) {
						prog.SetAssignedEpisode(it->second);
					}
				}
			}
			else if (stricmp(argv[i], "--count-hours") == 0) {
				uint64_t ms = 0;
				uint_t   j;

				for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
					const ADVBProg& prog = proglist.GetProg(j);

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

					list.ReadFromFile(config.GetRecordingFile());
					list.Sort();

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
				else {
					fprintf(stderr, "Failed to read scheduled programmes file\n");
				}
			}
			else if (stricmp(argv[i], "--find-new-programmes") == 0) {
				ADVBLock     lock("dvbfiles");
				ADVBProgList reclist;
				ADVBProgList schlist;

				if (reclist.ReadFromFile(config.GetRecordedFile())) {
					if (schlist.ReadFromFile(config.GetScheduledFile())) {
						ADVBProgList::SERIESLIST serieslist;
						uint_t j;

						lock.ReleaseLock();

						reclist.FindSeries(serieslist);
						schlist.StripFilmsAndSeries(serieslist);

						printf("Found %u new programmes\n", schlist.Count());
						for (j = 0; (j < schlist.Count()) && !HasQuit(); j++) {
							printf("%s\n", schlist[j].GetDescription(verbosity).str());
						}
					}
					else fprintf(stderr, "Failed to read scheduled programmes list\n");
				}
				else fprintf(stderr, "Failed to read recorded programmes list\n");
			}
			else if (stricmp(argv[i], "--check-video-files") == 0) {
				uint_t j;

				for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
					const ADVBProg& prog = proglist.GetProg(j);
					uint_t nerrors;

					if (prog.GetVideoErrorCount(nerrors)) {
						printf("%s: %u\n", prog.GetQuickDescription().str(), nerrors);
					}
				}
			}
			else if ((stricmp(argv[i], "--stream") == 0) ||
					 (stricmp(argv[i], "--rawstream") == 0)) {
				bool rawstream = (stricmp(argv[i], "--rawstream") == 0);
				AString text = AString(argv[++i]).DeEscapify(), cmd;

				ADVBConfig::GetWriteable(true);

				if (config.GetStreamSlave().Valid()) {
					AString cmd1, cmd2;

					cmd1.printf("dvb %s --stream \"%s\"", dvbcardspecified ? AString("--dvbcard %").Arg(dvbcard).str() : "", text.str());

					if (!rawstream) {
						cmd2.printf("| %s", config.GetVideoPlayerCommand().str());
					}

					cmd = GetRemoteCommand(cmd1, cmd2, false, true);
				}
				else {
					ADVBChannelList& channellist = ADVBChannelList::Get();
					AString pids;

					if (channellist.GetPIDList(0U, text, pids, false)) {
						ADVBProgList list;

						if (list.ReadFromFile(config.GetScheduledFile())) {
							std::vector<ADVBProgList::TIMEGAP> gaps;
							ADVBProgList::TIMEGAP best;
							ADateTime maxtime;

							list.ReadFromFile(config.GetRecordingFile());
							list.Sort();

							best = list.FindGaps(ADateTime().TimeStamp(true), gaps);
							if (dvbcardspecified) {
								// ignore the best gap, use the one specified by dvbcard
								best = gaps[dvbcard];
							}
							maxtime = (uint64_t)best.end - (uint64_t)best.start;

							if ((best.card < config.GetMaxDVBCards()) && (maxtime.GetAbsoluteSecond() > 10U)) {
								maxtime -= 10U * 1000U;

								fprintf(stderr, "Max stream time is %s (%ss), using card %u\n", maxtime.SpanStr().str(), AValue(maxtime.GetAbsoluteSecond()).ToString().str(), best.card);

								if (pids.Empty()) {
									channellist.GetPIDList(best.card, text, pids, true);
								}

								if (pids.Valid()) {
									cmd = ADVBProg::GenerateStreamCommand(best.card, (uint_t)std::min(maxtime.GetAbsoluteSecond(), (uint64_t)0xffffffff), pids);

									if (!config.IsRecordingSlave() && !rawstream) cmd.printf(" | %s", config.GetVideoPlayerCommand().str());
								}
								else fprintf(stderr, "Failed to find PIDs for channel '%s'\n", text.str());
							}
							else fprintf(stderr, "No DVB card has big enough gap\n");
						}
						else {
							fprintf(stderr, "Failed to read scheduled programmes file\n");
						}
					}
					else {
						ADVBProgList list, list1;
						AString errors;

						list.ReadFromFile(config.GetRecordedFile());
						list.ReadFromFile(config.GetRecordingFile());

						//fprintf(stderr, "Search pattern '%s'\n", text.str());
						list.FindProgrammes(list1, text, errors);

						if (errors.Valid()) {
							fprintf(stderr, "Errors found in pattern: %s\n", errors.str());
						}
						else if (list1.Count() > 0) {
							AString filename;
							uint_t i;

							for (i = list1.Count(); (i > 0) && filename.Empty(); ) {
								const ADVBProg& prog = list1[--i];

								if (AStdFile::exists(prog.GetFilename())) {
									filename = prog.GetFilename();
								}
								else if (AStdFile::exists(prog.GetArchiveRecordingFilename())) {
									filename = prog.GetArchiveRecordingFilename();
								}
								else if (AStdFile::exists(prog.GetTempRecordingFilename())) {
									filename = prog.GetTempRecordingFilename();
								}
								else if (AStdFile::exists(prog.GetTempFilename())) {
									filename = prog.GetTempFilename();
								}
							}

							if (filename.Valid()) {
								const ADVBProg& prog = list1[i];

								cmd.printf("cat \"%s\"", filename.str());

								fprintf(stderr, "Streaming '%s'\n", prog.GetDescription(1).str());
							}
							else {
								fprintf(stderr, "Failed to find a programme in running list (%u programmes long) matching '%s'\n", list.Count(), text.str());
							}
						}
						else {
							fprintf(stderr, "No programmes found with pattern\n");
						}
					}
				}

				if (cmd.Valid()) {
					if (config.LogRemoteCommands()) {
						config.logit("Running command '%s'", cmd.str());
					}

					int res = system(cmd);
					(void)res;
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
		}
	}

	return res;
}
