
#include <cstdint>
#include <regex>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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
#include "dvbmisc.h"
#include "proglist.h"
#include "channellist.h"
#include "findcards.h"
#include "dvbstreams.h"
#include "rdlib/misc.h"

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

static AQuitHandler QuitHandler;

static void DisplaySeries(const ADVBProgList::series_t& series)
{
    uint_t j;

    for (j = 0; j < series.list.size(); j++) {
        const auto& str = series.list[j];

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
    auto valid = (!(((i + 1) < argc) && (stricmp(argv[i + 1], "--help") == 0)) && ((i + n) < argc));

    if (!valid) {
        fprintf(stderr, "Incorrect arguments: %s %s \t%s\n", argv[i], option.args.str(), option.helptext.str());
    }

    return valid;
}

int main(int argc, const char *argv[])
{
    static const OPTION options[] = {
        {"--no-report-errors",                      "",                                 "Do NOT report any errors during directory creation (MUST be first argument)"},
        {"-v[<n>]",                                 "",                                 "Increase verbosity by <n> or by 1"},
        {"--confdir",                               "",                                 "Print conf directory"},
        {"--datadir",                               "",                                 "Print data directory"},
        {"--logdir",                                "",                                 "Print log directory"},
        {"--dirs",                                  "",                                 "Print important configuration directories"},
        {"--getxmltv",                              "<outputfile>",                     "Download XMLTV listings"},
        {"--dayformat",                             "<format>",                         AString("Format string for day (default '%')").Arg(ADVBProg::GetDayFormat())},
        {"--dateformat",                            "<format>",                         AString("Format string for date (default '%')").Arg(ADVBProg::GetDateFormat())},
        {"--timeformat",                            "<format>",                         AString("Format string for time (default '%')").Arg(ADVBProg::GetTimeFormat())},
        {"--fulltimeformat",                        "<format>",                         AString("Format string for fulltime (default '%')").Arg(ADVBProg::GetFullTimeFormat())},
        {"--set",                                   "<var>=<val>",                      "Set variable in configuration file for this session only"},
        {"-u, --update",                            "<file>",                           "Update main listings with file <file>"},
        {"-l, --load",                              "",                                 "Read listings from default file"},
        {"-r, --read",                              "<file>",                           "Read listings from file <file>"},
        {"--merge",                                 "<file>",                           "Merge listings from file <file>, adding any programmes that dot not exist"},
        {"--modify-recorded",                       "<file>",                           "Merge listings from file <file> into recorded programmes, adding any programmes that dot not exist"},
        {"--modify-recorded-from-recording-host",   "",                                 "Add recorded programmes from recording host into recorded programmes, adding any programmes that dot not exist"},
        {"--modify-scheduled-from-recording-host",  "",                                 "Modfy scheduled programmes from recording host into scheduled programmes"},
        {"--jobs",                                  "",                                 "Read programmes from scheduled jobs"},
        {"-w, --write",                             "<file>",                           "Write listings to file <file>"},
        {"--sort",                                  "",                                 "Sort list in chronological order"},
        {"--sort-by-fields",                        "<fieldlist>",                      "Sort list using field list"},
        {"--sort-rev",                              "",                                 "Sort list in reverse-chronological order"},
        {"--write-text",                            "<file>",                           "Write listings to file <file> in text format"},
        {"--write-gnuplot",                         "<file>",                           "Write listings to file <file> in format usable by GNUPlot"},
        {"--email",                                 "<recipient> <subject> <message>",  "Email current list (if it is non-empty) to <recipient> using subject"},
        {"--fix-pound",                             "<file>",                           "Fix pound symbols in file"},
        {"--update-dvb-channels",                   "",                                 "Update DVB channel assignments"},
        {"--update-dvb-channels-output-list",       "",                                 "Update DVB channel assignments and output list of SchedulesDirect channel ID's"},
        {"--update-dvb-channels-file",              "",                                 "Update DVB channels file from recording slave if it is newer"},
        {"--print-channels",                        "",                                 "Output all channels"},
        {"--find-unused-channels",                  "",                                 "Find listings and DVB channels that are unused"},
        {"--update-uuid",                           "",                                 "Set UUID's on every programme"},
        {"--update-combined",                       "",                                 "Create a combined list of recorded and scheduled programmes"},
        {"-L, --list",                              "",                                 "List programmes in current list"},
        {"--list-to-file",                          "<file>",                           "List programmes in current list to file <file>"},
        {"--list-base64",                           "",                                 "List programmes in current list in base64 format"},
        {"--list-channels",                         "",                                 "List channels"},
        {"--cut-head",                              "<n>",                              "Cut <n> programmes from the start of the list"},
        {"--cut-tail",                              "<n>",                              "Cut <n> programmes from the end of the list"},
        {"--head",                                  "<n>",                              "Keep <n> programmes at the start of the list"},
        {"--tail",                                  "<n>",                              "Keep <n> programmes at the end of the list"},
        {"--limit",                                 "<n>",                              "Limit list to <n> programmes, deleting programmes from the end of the list"},
        {"--diff-keep-same",                        "<file>",                           "Find differences of current list with supplied list and keep programmes that the same"},
        {"--diff-keep-different",                   "<file>",                           "Find differences of current list with supplied list and keep programmes that are different"},
        {"-f, --find",                              "<patterns>",                       "Find programmes matching <patterns>"},
        {"-F, --find-with-file",                    "<pattern-file>",                   "Find programmes matching patterns in patterns file <pattern-file>"},
        {"-R, --find-repeats",                      "",                                 "For each programme in current list, list repeats"},
        {"--find-similar",                          "<file>",                           "For each programme in current list, find first similar programme in <file>"},
        {"--find-differences",                      "<file1> <file2>",                  "Find programmes in <file1> but not in <file2> or in <file2> and not in <file1>"},
        {"--find-in-file1-only",                    "<file1> <file2>",                  "Find programmes in <file1> but not in <file2>"},
        {"--find-in-file2-only",                    "<file1> <file2>",                  "Find programmes in <file2> and not in <file1>"},
        {"--find-similarities",                     "<file1> <file2>",                  "Find programmes in both <file1> and <file2>"},
        {"--describe-pattern",                      "<patterns>",                       "Describe patterns as parsed"},
        {"--delete-all",                            "",                                 "Delete all programmes"},
        {"--delete",                                "<patterns>",                       "Delete programmes matching <patterns>"},
        {"--delete-with-file",                      "<pattern-file>",                   "Delete programmes matching patterns in patterns file <pattern-file>"},
        {"--delete-recorded",                       "",                                 "Delete programmes that have been recorded"},
        {"--delete-using-file",                     "<file>",                           "Delete programmes that are similar to those in file <file>"},
        {"--delete-similar",                        "",                                 "Delete programmes that are similar to others in the list"},
        {"--delete-dups",                           "",                                 "Delete duplicate programmes (by UUID)"},
        {"--schedule",                              "",                                 "Schedule and create jobs for default set of patterns on the main listings file"},
        {"--write-scheduled-jobs",                  "",                                 "Create jobs for current scheduled list"},
        {"--start-time",                            "<time>",                           "Set start time for scheduling"},
        {"--fake-schedule",                         "",                                 "Pretend to schedule (write files) for default set of patterns on the main listings file"},
        {"--fake-schedule-list",                    "",                                 "Pretend to schedule (write files) for current list of programmes"},
        {"--unschedule-all",                        "",                                 "Unschedule all programmes"},
        {"-S, --schedule-list",                     "",                                 "Schedule current list of programmes"},
        {"--record",                                "<prog>",                           "Record programme <prog> (Base64 encoded)"},
        {"--record-title",                          "<title>",                          "Schedule to record programme by title that has already started or starts within the next hour"},
        {"--schedule-record",                       "<base64>",                         "Schedule specified programme (Base64 encoded)"},
        {"--add-recorded",                          "<prog>",                           "Add recorded programme <prog> to recorded list (Base64 encoded)"},
        {"--recover-recorded-from-temp",            "",                                 "Create recorded programme entries from files in temp folder"},
        {"--card",                                  "<card>",                           "Set VIRTUAL card for subsequent scanning and PID operations (default 0)"},
        {"--dvbcard",                               "<card>",                           "Set PHYSICAL card for subsequent scanning and PID operations (default 0)"},
        {"--pids",                                  "<channel>",                        "Find PIDs (all streams) associated with channel <channel>"},
        {"--scan",                                  "<freq>[,<freq>...]",               "Scan frequencies <freq>MHz for DVB channels"},
        {"--scan-all",                              "",                                 "Scan frequencies from existing DVB channels"},
        {"--scan-range",                            "<start-freq> <end-freq> <step>",   "Scan frequencies in specified range for DVB channels"},
        {"--find-channels",                         "",                                 "Run Perl update channels script and update channels accordingly"},
        {"--find-cards",                            "",                                 "Find DVB cards"},
        {"--change-filename",                       "<filename1> <filename2>",          "Change filename of recorded progamme with filename <filename1> to <filename2>"},
        {"--change-filename-regex",                 "<filename1> <filename2>",          "Change filename of recorded progamme with filename <filename1> to <filename2> (where <filename1> can be a regex and <filename2> can be an expansion)"},
        {"--change-filename-regex-test",            "<filename1> <filename2>",          "Like above but do not commit changes"},
        {"--find-recorded-programmes-on-disk",      "",                                 "Search for recorded programmes in 'searchdirs' and update any filenames"},
        {"--find-series",                           "",                                 "Find series' in programme list"},
        {"--fix-data-in-recorded",                  "",                                 "Fix incorrect metadata of certain programmes and rename files as necessary"},
        {"--set-recorded-flag",                     "",                                 "Set recorded flag on all programmes in recorded list"},
        {"--list-flags",                            "",                                 "List flags that can be used for --set-flag and --clr-flag"},
        {"--set-flag",                              "<flag> <patterns>",                "Set flag <flag> on programmes matching pattern"},
        {"--clr-flag",                              "<flag> <patterns>",                "Clear flag <flag> on programmes matching pattern"},
        {"--check-disk-space",                      "",                                 "Check disk space for all patterns"},
        {"--update-recording-complete",             "",                                 "Update recording complete flag in every recorded programme"},
        {"--check-recording-file",                  "",                                 "Check programmes in running list to ensure they should remain in there"},
        {"--force-failures",                        "",                                 "Add current list to recording failures (setting failed flag)"},
        {"--show-encoding-args",                    "",                                 "Show encoding arguments for programmes in current list"},
        {"--show-file-format",                      "",                                 "Show file format of encoded programme"},
        {"--use-converted-filename",                "",                                 "Change filename to that of converted file, without actually converting file"},
        {"--set-dir",                               "<patterns> <newdir>",              "Change directory for programmes in current list that match <patterns> to <newdir>"},
        {"--regenerate-filename",                   "<patterns>",                       "Change filename of current list of programmes that match <pattern> (dependant on converted state), renaming files (in original directory OR current directory)"},
        {"--regenerate-filename-test",              "<patterns>",                       "Test version of the above, will make no changes to programme list or move files"},
        {"--delete-files",                          "",                                 "Delete encoded files from programmes in current list"},
        {"--delete-from-record-lists",              "<uuid>",                           "Delete programme with UUID <uuid> from record lists (recorded and record failues)"},
        {"--record-success",                        "",                                 "Run recordsuccess command on programmes in current list"},
        {"--change-user",                           "<patterns> <newuser>",             "Change user of programmes matching <patterns> to <newuser>"},
        {"--change-dir",                            "<patterns> <newdir>",              "Change direcotry of programmes matching <patterns> to <newdir>"},
        {"--get-and-convert-recorded",              "",                                 "Pull and convert any recordings from recording host"},
        {"--force-convert-files",                   "",                                 "convert files in current list, even if they have already been converted"},
        {"--update-recordings-list",                "",                                 "Pull list of programmes being recorded"},
        {"--check-recording-now",                   "",                                 "Check to see if programmes that should be being recording are recording"},
        {"--calc-trend",                            "<start-date>",                     "Calculate average programmes per day and trend line based on programmes after <start-date> in current list"},
        {"--gen-graphs",                            "",                                 "Generate graphs from recorded data"},
        {"--assign-episodes",                       "",                                 "Assign episodes to current list"},
        {"--reset-assigned-episodes",               "",                                 "Reset assigned episodes to current list"},
        {"--count-hours",                           "",                                 "Count total hours of programmes in current list"},
        {"--find-gaps",                             "",                                 "Find gap from now until the next working for each card"},
        {"--find-new-programmes",                   "",                                 "Find programmes that have been scheduled that are either new or from a new series"},
        {"--check-video-files",                     "",                                 "Check archived video files for errors and update programmes in current list"},
        {"--update-duration-and-video-errors",      "",                                 "Update duration and video errors in recorded list"},
        {"--force-update-duration-and-video-errors", "",                                "Update duration and video errors in recorded list"},
        {"--update-video-has-subtitles",            "",                                 "Update the 'archivedhassubtitles' flag for all programmes in record list"},
        {"---stream",                               "<base64>",                         "Process stream described by <base64>"},
        {"--stream",                                "<text>",                           "Stream DVB channel or programme being recorded <text> to mplayer (or other player)"},
        {"--rawstream",                             "<text>",                           "Stream DVB channel or programme being recorded <text> to console (for piping to arbitrary programs)"},
        {"--mp4stream",                             "<text>",                           "Stream DVB channel or programme being recorded <text>, encoding as mp4 to console (for piping to arbitrary programs)"},
        {"--hlsstream",                             "<text>",                           "Stream DVB channel or programme being recorded <text>, encoding as HLS"},
        {"--httpstream",                            "<text>[;<port>]",                  "Stream DVB channel or programme being recorded <text>, encoding as http"},
        {"--rhttpstream",                           "<text>[;<port>]",                  "Stream DVB channel or programme being recorded <text>, encoding as http on remote machine"},
        {"--list-streams",                          "",                                 "List active stream(s)"},
        {"--stop-streams",                          "<pattern>",                        "Stop stream(s) matching <pattern>"},
        {"--stop-stream",                           "<name>",                           "Stop specific stream"},
        {"--describe-pid",                          "<pid>",                            "Describe PID tree of <pid>"},
        {"--kill-stream-pid",                       "<pid>",                            "Stop specific stream specified by <pid>"},
        {"--drawprogrammes",                        "<scale>",                          "Draw current list of programmes using a scale of <scale> characters per hour"},
        {"--list-config-values",                    "",                                 "List all config values"},
        {"--config-item",                           "<item>",                           "Return value for config item"},
        {"--user-config-item",                      "<user> <item>",                    "Return value for user's config item"},
        {"--user-category-config-item",             "<user> <category> <item>",         "Return value for user's config item for specified programme category"},
        {"--test-cards-channel",                    "<channel>",                        "Channel used for --test-cards"},
        {"--test-cards-seconds",                    "<seconds>",                        "Amount of time in seconds to collect data for for --test-cards"},
        {"--test-cards",                            "",                                 "Test each DVB card to ensure card is working correctly"},
        {"--test-card",                             "<card>",                           "Test DVB card to ensure card is working correctly"},
        {"--return-count",                          "",                                 "Return programme list count in error code"},
        {"--combine-split-films",                   "",                                 "Combine films split by news programme"},
    };
    const auto& config = ADVBConfig::Get();
    ADVBProgList proglist;
    auto    starttime       = ADateTime().TimeStamp(true);
    auto    testcardchannel = config.GetTestCardChannel();
    auto    testcardseconds = config.GetTestCardTime();
    AString testcardremotecmd;
    bool    testcardsuccess   = false;
    bool    testcardperformed = false;
    uint_t  dvbcard   = 0;
    uint_t  verbosity = 0;
    uint_t  nopt, nsubopt;
    bool    dvbcardspecified = false;
    int     i;
    int     res = 0;

    {   // ensure ADVBProg initialisation takes place
        ADVBProg prog;
        (void)prog;
    }

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
                auto& prog     = list.GetProgWritable(i);
                auto  filename = prog.GetFilename();

                if (filename.FilePart() == ".") {
                    auto convertedfilename   = prog.GenerateFilename(true);
                    auto unconvertedfilename = prog.GenerateFilename(false);
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
            const auto& option = options[nopt];
            AString text;
            uint_t  nsubopts = (uint_t)option.option.CountColumns();

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
            const auto& option = options[nopt];

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
                auto inc = (uint_t)AString(argv[i] + 2);
                verbosity += (inc > 0) ? inc : 1;
                continue;
            }

            for (nopt = 0; nopt < NUMBEROF(options); nopt++) {
                const auto& option = options[nopt];
                uint_t nsubopts = (uint_t)option.option.CountColumns();

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

            if      (strcmp(argv[i], "--confdir") == 0) printf("%s\n", config.GetConfigDir().str());
            else if (strcmp(argv[i], "--datadir") == 0) printf("%s\n", config.GetDataDir().str());
            else if (strcmp(argv[i], "--logdir") == 0)  printf("%s\n", config.GetLogDir().str());
            else if (strcmp(argv[i], "--dirs") == 0) {
                AList users;

                config.ListUsers(users);

                printf("Configuration: %s\n", config.GetConfigDir().str());
                printf("Data: %s\n", config.GetDataDir().str());
                printf("Log files: %s\n", config.GetLogDir().str());
                printf("Recordings: %s\n", config.GetRecordingsDir().str());

                const auto *user = AString::Cast(users.First());
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
                auto    cmd = config.GetXMLTVDownloadCommand() + " " + config.GetXMLTVDownloadArguments(destfile);

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
                    auto var = str.Left(p);
                    auto val = str.Mid(p + 1).DeQuotify().DeEscapify();
                    config.Set(var, val);
                }
                else fprintf(stderr, "Configuration setting '%s' invalid, format should be '<var>=<val>'", str.str());
            }
            else if ((strcmp(argv[i], "--update") == 0) || (strcmp(argv[i], "-u") == 0) || (AString(argv[i]).Suffix() == "xmltv") || (AString(argv[i]).Suffix() == "txt")) {
                auto    filename = config.GetListingsFile();
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

                    config.printf("Combining split programmes...");
                    proglist.CombineSplitFilms();

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
                auto filename = config.GetListingsFile();

                printf("Reading main listings file...\n");
                if (proglist.ReadFromFile(filename)) {
                    printf("Read programmes from '%s', total now %u\n", filename.str(), proglist.Count());
                }
                else printf("Failed to read programme list from '%s'\n", filename.str());
            }
            else if ((strcmp(argv[i], "--read") == 0) || (strcmp(argv[i], "-r") == 0)) {
                auto filename = config.GetNamedFile(argv[++i]);

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
                ADVBLock     lock("dvbfiles");
                AString      filename = argv[++i];
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
                auto filename = config.GetNamedFile(argv[++i]);

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
                    auto filename    = config.GetDVBChannelsJSONFile();
                    auto dstfilename = config.GetTempFile(filename.FilePart().Prefix(), "." + filename.Suffix());

                    if (GetFileFromRecordingSlave(filename, dstfilename)) {
                        FILE_INFO info1, info2;

                        if (!::GetFileInfo(filename, &info1) ||
                            (::GetFileInfo(dstfilename, &info2) &&
                             (info2.WriteTime > info1.WriteTime))) {
                            // replace local channels file with remote one
                            MoveFile(dstfilename, filename);
                            config.printf("Replaced local channels file with one from recording slave");
                        }
                        else config.printf("Local channels file up to date");
                    }
                }
                else printf("No need to update DVB channels file: no recording slave\n");
            }
            else if (strcmp(argv[i], "--print-channels") == 0) {
                const auto& channellist = ADVBChannelList::Get();
                TABLE table;
                size_t j;
                uint_t k;

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

                for (k = 0; k < channellist.GetLCNCount(); k++) {
                    const ADVBChannelList::channel_t *chan;

                    if ((chan = channellist.GetChannelByLCN(k)) != NULL) {
                        TABLEROW row;

                        table.justify[row.size()] = 2;
                        row.push_back((chan->dvb.lcn > 0) ? AString("%;").Arg(chan->dvb.lcn) : AString());
                        table.justify[row.size()] = 2;
                        row.push_back((chan->xmltv.sdchannelid > 0) ? AString("%;").Arg(chan->xmltv.sdchannelid) : AString());
                        row.push_back(chan->xmltv.channelid);
                        row.push_back(chan->xmltv.channelname);
                        row.push_back(chan->xmltv.convertedchannelname);
                        row.push_back(chan->dvb.channelname);
                        row.push_back(chan->dvb.convertedchannelname);

                        table.justify[row.size()] = 1;
                        row.push_back((chan->dvb.freq > 0) ? AString("%;").Arg(chan->dvb.freq) : AString());

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
                const auto& channellist = ADVBChannelList::Get();
                std::map<AString,uint_t> dvbchannels;
                std::map<AString,uint_t> missingchannels;
                uint_t j, n = channellist.GetChannelCount();

                for (j = 0; j < n; j++) {
                    dvbchannels[channellist.GetChannel(j)->dvb.channelname] = 0;
                }

                for (j = 0; j < proglist.Count(); j++) {
                    const auto& prog       = proglist[j];
                    const auto  dvbchannel = AString(prog.GetDVBChannel());

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
                            const auto& prog = reslist[k];
                            auto        desc = prog.GetDescription(verbosity);
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
                            auto   desc = prog->GetDescription(verbosity);
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
                const auto similarities = (strcmp(argv[i], "--find-similarities") == 0);
                const auto in1only = ((strcmp(argv[i], "--find-differences") == 0) || (strcmp(argv[i], "--find-in-file1-only") == 0));
                const auto in2only = ((strcmp(argv[i], "--find-differences") == 0) || (strcmp(argv[i], "--find-in-file2-only") == 0));
                ADVBProgList file1list, file2list;
                auto file1name = AString(argv[++i]);
                auto file2name = AString(argv[++i]);
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
                auto      patterns = AString(argv[++i]);
                AString   errors;
                uint_t    i;

                ADVBPatterns::ParsePatterns(patternlist, patterns, errors, ';');

                for (i = 0; i < patternlist.Count(); i++) {
                    const auto& pattern = *(const ADVBPatterns::pattern_t *)patternlist[i];
                    printf("%s", ADVBPatterns::ToString(pattern).str());
                }
            }
            else if ((strcmp(argv[i], "--find-with-file") == 0) || (strcmp(argv[i], "-F") == 0)) {
                ADVBProgList reslist;
                AString      patterns, errors;
                auto         filename = AString(argv[++i]);

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
                auto         patterns = AString(argv[++i]);
                AString      errors;
                uint_t       j;

                proglist.FindProgrammes(reslist, patterns, errors, (patterns.Pos("\n") >= 0) ? "\n" : ";");

                printf("Found %u programme%s\n", reslist.Count(), (reslist.Count() == 1) ? "" : "s");

                if (errors.Valid()) {
                    printf("Errors:\n");

                    uint_t j, n = errors.CountLines();
                    for (j = 0; j < n; j++) printf("%s", errors.Line(j).str());
                }

                for (j = 0; (j < reslist.Count()) && !HasQuit(); j++) {
                    const auto& prog = reslist.GetProg(j);

                    if (proglist.DeleteProg(prog)) {
                        printf("Deleted '%s'\n", prog.GetDescription(verbosity).str());
                    }
                    else printf("Failed to delete '%s'!\n", prog.GetDescription(verbosity).str());
                }
            }
            else if (strcmp(argv[i], "--delete-with-file") == 0) {
                ADVBProgList reslist;
                AString      patterns, errors;
                auto         filename = AString(argv[++i]);
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
                        const auto& prog = reslist.GetProg(j);

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
                auto         filename = config.GetRecordedFile();
                ADVBProgList proglist2;

                if (strcmp(argv[i], "--delete-using-file") == 0) filename = config.GetNamedFile(argv[++i]);

                if (proglist2.ReadFromFile(filename)) {
                    uint_t j, ndeleted = 0;

                    for (j = 0; (j < proglist.Count()) && !HasQuit(); ) {
                        if (proglist2.FindSimilar(proglist.GetProg(j)) != NULL) {
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
                    const auto&    prog = proglist.GetProg(j);
                    const ADVBProg *sprog;

                    while ((sprog = proglist.FindSimilar(prog, &prog)) != NULL) {
                        proglist.DeleteProg(*sprog);
                        ndeleted++;
                    }
                }

                printf("Deleted %u programmes\n", ndeleted);
            }
            else if (stricmp(argv[i], "--delete-dups") == 0) {
                uint_t j, k, ndeleted = 0;

                for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
                    const auto& prog = proglist.GetProg(j);

                    for (k = j + 1; (k < proglist.Count()) && !HasQuit(); ) {
                        if (prog == proglist[k]) {
                            proglist.DeleteProg(k);
                            ndeleted++;
                        }
                        else k++;
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
                const auto& list = ADVBChannelList::Get();
                uint_t j, nnodvb = 0;

                for (j = 0; j < list.GetChannelCount(); j++) {
                    const auto *channel = list.GetChannel(j);

                    printf("Channel %u/%u (LCN %u): '%s' (DVB '%s', XMLTV channel-id '%s')\n", j + 1, list.GetChannelCount(), channel->dvb.lcn, channel->xmltv.channelname.str(), channel->dvb.channelname.str(), channel->xmltv.channelid.str());

                    nnodvb += channel->dvb.channelname.Empty();
                }
                printf("%u/%u channels have NO DVB channel\n", nnodvb, list.GetChannelCount());
            }
            else if (strcmp(argv[i], "--cut-head") == 0) {
                auto   n = (uint_t)AString(argv[++i]);
                uint_t j;

                for (j = 0; (j < n) && proglist.Count(); j++) {
                    proglist.DeleteProg(0);
                }
            }
            else if (strcmp(argv[i], "--cut-tail") == 0) {
                auto   n = (uint_t)AString(argv[++i]);
                uint_t j;

                for (j = 0; (j < n) && proglist.Count(); j++) {
                    proglist.DeleteProg(proglist.Count() - 1);
                }
            }
            else if (strcmp(argv[i], "--head") == 0) {
                auto n = (uint_t)AString(argv[++i]);

                while (proglist.Count() > n) {
                    proglist.DeleteProg(proglist.Count() - 1);
                }
            }
            else if (strcmp(argv[i], "--tail") == 0) {
                auto n = (uint_t)AString(argv[++i]);

                while (proglist.Count() > n) {
                    proglist.DeleteProg(0);
                }
            }
            else if (strcmp(argv[i], "--limit") == 0) {
                auto n = (uint_t)AString(argv[++i]);

                while (proglist.Count() > n) {
                    proglist.DeleteProg(proglist.Count() - 1);
                }
            }
            else if ((strcmp(argv[i], "--diff-keep-same")      == 0) ||
                     (strcmp(argv[i], "--diff-keep-different") == 0)) {
                const auto   keepsame = (strcmp(argv[i], "--diff-keep-same") == 0);
                const auto   filename = config.GetNamedFile(argv[++i]);
                ADVBProgList proglist1;

                if (proglist1.ReadFromFile(filename)) {
                    uint_t ndeleted = 0;

                    proglist.CreateHash();
                    proglist1.CreateHash();

                    for (uint_t j = 0; j < proglist.Count(); ) {
                        const auto&    prog1 = proglist[j];
                        const ADVBProg *prog2;
                        const auto     same = (((prog2 = proglist1.FindUUID(prog1)) != NULL) &&
                                               (prog1.Base64Encode() == prog2->Base64Encode()));

                        if (same == keepsame) {
                            j++;
                        }
                        else {
                            proglist.DeleteProg(j);
                            ndeleted++;
                        }
                    }

                    printf("%u programmes kept (%u deleted)\n", proglist.Count(), ndeleted);
                }
                else {
                    printf("Failed to read programme list from '%s'\n", filename.str());
                }
            }
            else if ((strcmp(argv[i], "--write") == 0) || (strcmp(argv[i], "-w") == 0)) {
                auto filename = config.GetNamedFile(argv[++i]);

                if (!HasQuit() && proglist.WriteToFile(filename)) {
                    printf("Wrote %u programmes to '%s'\n", proglist.Count(), filename.str());
                }
                else {
                    printf("Failed to write programme list to '%s'\n", filename.str());
                }
            }
            else if (strcmp(argv[i], "--write-text") == 0) {
                auto filename = config.GetNamedFile(argv[++i]);

                if (proglist.WriteToTextFile(filename)) {
                    printf("Wrote %u programmes to '%s'\n", proglist.Count(), filename.str());
                }
                else {
                    printf("Failed to write programme list to '%s'\n", filename.str());
                }
            }
            else if (stricmp(argv[i], "--write-gnuplot") == 0) {
                auto filename = AString(argv[++i]);

                if (!proglist.WriteToGNUPlotFile(filename)) {
                    fprintf(stderr, "Failed to write GNU plot file %s\n", filename.str());
                }
            }
            else if (stricmp(argv[i], "--email") == 0) {
                auto recipient = AString(argv[++i]);
                auto subject   = AString(argv[++i]);
                auto message   = AString(argv[++i]).DeEscapify();

                if (proglist.EmailList(recipient, subject, message, verbosity)) {
                    printf("List emailed successfully (if not empty)\n");
                }
                else {
                    res = -1;
                }
            }
            else if ((strcmp(argv[i], "--sort") == 0) || (strcmp(argv[i], "--sort-rev") == 0)) {
                auto reverse = (strcmp(argv[i], "--sort-rev") == 0);
                proglist.Sort(reverse);
                printf("%sorted list chronologically\n", reverse ? "Reverse s" : "S");
            }
            else if (strcmp(argv[i], "--sort-by-fields") == 0) {
                ADVBProg::fieldlist_t fieldlist;

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
                auto     progstr = AString(argv[++i]);
                ADVBProg prog;

                if (prog.Base64Decode(progstr)) prog.Record();
                else printf("Failed to decode programme '%s'\n", progstr.str());
            }
            else if (strcmp(argv[i], "--record-title") == 0) {
                auto     title = AString(argv[++i]);
                ADVBLock lock("dvbfiles");
                ADVBProgList list;

                if (list.ReadFromFile(config.GetListingsFile())) {
                    lock.ReleaseLock();

                    list.RecordImmediately(ADateTime().TimeStamp(true), title);
                }
            }
            else if (stricmp(argv[i], "--schedule-record") == 0) {
                auto     base64 = AString(argv[++i]);
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
                auto     progstr = AString(argv[++i]);
                ADVBProg prog;

                if (prog.Base64Decode(progstr)) {
                    const auto filename     = AString(prog.GetFilename());
                    const auto tempfilename = AString(prog.GetTempFilename());
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
                        if (!MoveFile(tempfilename, filename)) {
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
                ADVBLock     lock("files");
                ADVBProgList reclist;
                auto         reclistfilename = config.GetRecordedFile();
                uint_t       nchanges = 0;

                if (reclist.ReadFromFile(reclistfilename)) {
                    AList files;

                    reclist.CreateHash();

                    CollectFiles(config.GetRecordingsStorageDir(), "*." + config.GetRecordedFileSuffix(), 0, files, FILE_FLAG_IS_DIR, 0, &QuitHandler);

                    printf("--------------------------------------------------------------------------------\n");

                    // iterate through files
                    const auto *str = AString::Cast(files.First());
                    while (str && !HasQuit()) {
                        // for each file, attempt to find a file in the current list whose's unconverted file has the same filename
                        const auto& filename = *str;
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
                                    auto prog = proglist[j];    // note: take copy of prog

                                    printf("Found as '%s':\n", prog.GetQuickDescription().str());

                                    prog.ClearScheduled();
                                    prog.ClearRecording();
                                    prog.ClearRunning();
                                    prog.SetRecorded();
                                    prog.GenerateRecordData(0);
                                    prog.SetFilename(filename); // must set the filename as GenerateRecordData() will set a different one
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

                    if (nchanges > 0) {
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
                const auto newcard = (uint_t)AString(argv[++i]);

                config.GetPhysicalDVBCard(0);
                if (config.GetMaxDVBCards() > 0) {
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
                auto&      list    = ADVBChannelList::Get();
                const auto channel = AString(argv[++i]);
                AString    pids;

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
                auto&      list  = ADVBChannelList::Get();
                const auto freqs = AString(argv[++i]);

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
                auto& list = ADVBChannelList::Get();

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
                auto&      list  = ADVBChannelList::Get();
                const auto f1str = AString(argv[++i]);
                const auto f2str = AString(argv[++i]);
                const auto ststr = AString(argv[++i]);

                if (config.GetRecordingSlave().Valid()) {
                    // MUST write channels if they have been updated
                    list.Write();

                    printf("Scanning on recording slave '%s'\n", config.GetRecordingSlave().str());
                    RunRemoteCommandGetFile(AString("dvb --dvbcard %; --scan-range %; %; %;").Arg(dvbcard).Arg(f1str).Arg(f2str).Arg(ststr).EndArgs(), config.GetDVBChannelsJSONFile());

                    // force re-reading after remote command
                    list.Read();
                }
                else {
                    const auto f1   = (double)f1str;
                    const auto f2   = (double)f2str;
                    const auto step = (double)ststr;
                    double f;

                    for (f = f1; f <= f2; f += step) {
                        list.Update(dvbcard, (uint32_t)(1.0e6 * f + .5), true);
                    }
                }
            }
            else if (strcmp(argv[i], "--find-channels") == 0) {
                auto& list = ADVBChannelList::Get();

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
                ADVBLock     lock("dvbfiles");
                auto         filename1 = AString(argv[++i]);
                auto         filename2 = AString(argv[++i]);
                ADVBProgList reclist;

                if (filename2 != filename1) {
                    if (reclist.ReadFromFile(config.GetRecordedFile())) {
                        uint_t j;
                        bool changed = false;

                        for (j = 0; (j < reclist.Count()) && !HasQuit(); j++) {
                            auto& prog = reclist.GetProgWritable(j);

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
                ADVBLock      lock("dvbfiles");
                ADVBProgList  reclist;
                const auto arg         = AString(argv[i++]);
                const auto pattern     = AString(argv[i++]);
                const auto replacement = AString(argv[i]);
                const auto commit      = (strcmp(arg, "--change-filename-regex") == 0);

                try {
                    const regex_replace_t replace = {
                        .regex   = ParseRegex(pattern),
                        .replace = replacement,
                    };

                    if (reclist.ReadFromFile(config.GetRecordedFile())) {
                        uint_t j;
                        bool changed = false;

                        for (j = 0; j < reclist.Count(); j++) {
                            ADataList      regionlist;
                            auto&          prog = reclist.GetProgWritable(j);
                            const AString& filename1 = prog.GetFilename();

                            if (MatchRegex(filename1, replace.regex)) {
                                auto filename2 = RegexReplace(filename1, replace);

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
                catch (const std::regex_error& ex) {
                    config.printf("Invalid regex '%s' for '%s'", pattern.str(), arg.str());
                }
            }
            else if (stricmp(argv[i], "--find-series") == 0) {
                ADVBProgList::serieslist_t series;
                ADVBProgList::serieslist_t::iterator it;

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
                        auto& prog = reclist.GetProgWritable(j);

                        if (prog.FixData()) n++;
                    }

                    config.printf("%u programmes changed", n);

                    if ((n > 0) && (HasQuit() || !reclist.WriteToFile(config.GetRecordedFile()))) {
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
                auto         recdir = config.GetRecordingsDir();
                auto         dirs   = config.GetConfigItem("searchdirs", "");

                if (reclist.ReadFromFile(config.GetRecordedFile())) {
                    uint_t j, k, n = dirs.CountLines(",");
                    uint_t found = 0;

                    for (j = 0; (j < reclist.Count()) && !HasQuit(); j++) {
                        auto&     prog = reclist.GetProgWritable(j);
                        FILE_INFO info;

                        if (!AStdFile::exists(prog.GetFilename())) {
                            auto filename1 = prog.GenerateFilename(prog.IsConverted());
                            if (AStdFile::exists(filename1)) {
                                prog.SetFilename(filename1);
                                found++;
                            }
                        }
                    }

                    for (j = 0; (j < n) && !HasQuit(); j++) {
                        auto  path = recdir.CatPath(dirs.Line(j, ","));
                        AList dirs;

                        dirs.Add(new AString(path));
                        CollectFiles(path, "*", RECURSE_ALL_SUBDIRS, dirs, FILE_FLAG_IS_DIR, FILE_FLAG_IS_DIR);

                        const auto *dir = AString::Cast(dirs.First());
                        while (dir && !HasQuit()) {
                            if (dir->FilePart() != "subs") {
                                config.printf("Searching '%s'...", dir->str());

                                for (k = 0; (k < reclist.Count()) && !HasQuit(); k++) {
                                    auto& prog = reclist.GetProgWritable(k);

                                    if (!AStdFile::exists(prog.GetFilename()) || config.GetRelativePath(prog.GetFilename()).Empty()) {
                                        auto filename1 = dir->CatPath(AString(prog.GetFilename()).FilePart());
                                        auto filename2 = filename1.Prefix() + "." + config.GetConvertedFileSuffix(prog.GetUser());

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
                        auto& prog = reclist.GetProgWritable(j);

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
                auto    set      = (stricmp(argv[i], "--set-flag") == 0);
                auto    flagname = AString(argv[++i]);
                auto    patterns = AString(argv[++i]);
                AString errors;
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
                        auto& prog = failureslist.GetProgWritable(j);

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
                    auto& prog = proglist.GetProgWritable(j);
                    prog.SetNotify();
                    prog.OnRecordSuccess();
                }
            }
            else if (stricmp(argv[i], "--change-user") == 0) {
                ADVBProgList reslist;
                auto         patterns = AString(argv[++i]);
                auto         newvalue = AString(argv[++i]);
                AString      errors;
                uint_t       j;

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
                auto         patterns = AString(argv[++i]);
                auto         newvalue = AString(argv[++i]);
                AString      errors;
                uint_t       j;

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
                    const auto& prog = proglist.GetProg(j);
                    auto        proccmd = config.GetEncodeCommand(prog.GetUser(), prog.GetCategory());
                    auto        args    = config.GetEncodeArgs(prog.GetUser(), prog.GetCategory());
                    uint_t k, n = args.CountLines(";");

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
                    const auto& prog = proglist.GetProg(j);

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
                    auto& prog = proglist.GetProgWritable(j);
                    auto  oldfilename = prog.GenerateFilename();

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
                    auto& prog = proglist.GetProgWritable(j);

                    if (!prog.IsConverted()) {
                        auto oldfilename = AString(prog.GetFilename());

                        prog.SetFilename(prog.GenerateFilename(true));

                        if (oldfilename != AString(prog.GetFilename())) prog.UpdateRecordedList();
                    }
                }
            }
            else if (stricmp(argv[i], "--set-dir") == 0) {
                ADVBProgList reslist;
                auto         patterns = AString(argv[++i]);
                auto         newdir   = AString(argv[++i]);
                AString      errors;
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
                const auto test = (stricmp(argv[i], "--regenerate-filename-test") == 0);
                ADVBProgList reslist;
                auto         patterns = AString(argv[++i]);
                AString      errors;
                uint_t       j;

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
                            auto& prog = *pprog;
                            auto  oldfilename = AString(prog.GetFilename());

                            if (prog.IsConverted()) {
                                auto subdir = oldfilename.PathPart().PathPart().FilePart();

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

                            auto newfilename  = prog.GenerateFilename(prog.IsConverted());
                            auto oldfilename1 = oldfilename.FilePart();
                            auto newfilename1 = newfilename.FilePart();

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
                    const auto& prog = proglist.GetProg(j);

                    prog.DeleteEncodedFiles();
                }
            }
            else if (stricmp(argv[i], "--delete-from-record-lists") == 0) {
                auto uuid = AString(argv[++i]);

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
                    auto& prog = proglist.GetProgWritable(j);

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
                    auto& prog = proglist.GetProgWritable(j);

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
                ADVBProgList::trend_t trend;

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
                    const auto& prog = proglist.GetProg(j);
                    auto epid = AString(prog.GetEpisodeID());

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
                    if (it->second != NULL) {
                        const std::vector<AString>& list = *it->second;

                        printf("Episode list for '%s':\n", it->first.str());

                        size_t i;
                        uint16_t epn = 1;
                        for (i = 0; i < list.size(); i++, epn++) {
                            const auto& epid = list[i];

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
                    auto& prog = proglist.GetProgWritable(j);
                    auto  epid = AString(prog.GetEpisodeID());
                    auto  it   = episodeids.find(epid);

                    if (it != episodeids.end()) {
                        prog.SetAssignedEpisode(it->second);
                    }
                }
            }
            else if (stricmp(argv[i], "--count-hours") == 0) {
                uint64_t ms = 0;
                uint_t   j;

                for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
                    const auto& prog = proglist.GetProg(j);

                    if      (prog.GetActualLength() > 0) ms += prog.GetActualLength();
                    else if (prog.GetRecordLength() > 0) ms += prog.GetRecordLength();
                    else                                 ms += prog.GetLength();
                }

                auto hours = (double)ms / (1000.0 * 3600.0);
                printf("%u programmes, %0.2lf hours, average %0.1lf minutes/programme\n", proglist.Count(), hours, (60.0 * hours) / (double)proglist.Count());
            }
            else if (stricmp(argv[i], "--find-gaps") == 0) {
                ADVBProgList list;

                if (list.ReadFromFile(config.GetScheduledFile())) {
                    std::vector<ADVBProgList::timegap_t> gaps;
                    ADVBProgList::timegap_t best;
                    uint_t i;

                    list.ReadFromFile(config.GetRecordingFile());
                    list.Sort();

                    best = list.FindGaps(ADateTime().TimeStamp(true), gaps);

                    for (i = 0; i < (uint_t)gaps.size(); i++) {
                        const auto& gap = gaps[i];
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
                        ADVBProgList::serieslist_t serieslist;
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
            else if ((stricmp(argv[i], "--check-video-files") == 0) ||
                     (stricmp(argv[i], "--update-duration-and-video-errors") == 0) ||
                     (stricmp(argv[i], "--force-update-duration-and-video-errors") == 0)) {
                const auto force  = (stricmp(argv[i], "--force-update-duration-and-video-errors") == 0);
                const auto update = (force || (stricmp(argv[i], "--update-duration-and-video-errors") == 0));
                AString lastuuid;
                uint_t j;

                for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
                    const auto& prog     = proglist[j];
                    uint64_t    duration = 0;
                    uint_t      nerrors  = 0;
                    bool        updated  = false;

                    if (force || !update || (prog.GetDuration() == 0)) {
                        printf("Finding duration and video errors in '%s' ('%s' - %u/%u)...\n", prog.GetTitleAndSubtitle().str(), prog.GetArchiveRecordingFilename().str(), j + 1, proglist.Count());

                        if (AStdFile::exists(prog.GetArchiveRecordingFilename()) ||
                            AStdFile::exists(prog.GetFilename())) {
                            // duration is in ms
                            if (prog.GetVideoDuration(duration) &&
                                prog.GetVideoErrorCount(nerrors)) {
                                printf("%s: %0.2f min, %u errors (%0.1f errors/min)\n",
                                       prog.GetQuickDescription().str(),
                                       (double)duration / 60000.0,
                                       nerrors,
                                       (60000.0 * (double)nerrors) / (double)duration);
                            }
                        }

                        if (duration == 0) {
                            // direct duration not available, use length of programme instead
                            duration = prog.GetActualLengthFallback();
                        }

                        // if a valid duration has been found
                        if (duration > 0) {
                            // set duration and video errors of all contiguous programmes with the
                            // same UUID
                            for (uint_t k = j; (k < proglist.Count()) && (proglist[k] == prog) && !HasQuit(); k++) {
                                auto& prog2 = proglist.GetProgWritable(k);
                                prog2.SetDuration(duration);
                                prog2.SetVideoErrors(nerrors);
                            }

                            // allow updating of recorded proglist
                            updated = update;
                        }
                    }

                    // if recorded proglist should be updated
                    if (!HasQuit() && updated) {
                        // update recorded list: load it here, update it and save it
                        // as quickly as possible
                        ADVBLock     lock("dvbfiles");
                        ADVBProgList recorded;
                        int          progindex;

                        recorded.ReadFromFile(config.GetRecordedFile());

                        // find programme in recorded list
                        if ((progindex = recorded.FindUUIDIndex(prog)) >= 0) {
                            // update all contiguous programmes that have the same UUID
                            for (uint_t k = (uint_t)progindex; (k < recorded.Count()) && (recorded[k] == prog) && !HasQuit(); k++) {
                                // update programme
                                auto& prog2 = recorded.GetProgWritable(k);
                                prog2.SetDuration(duration);
                                prog2.SetVideoErrors(nerrors);
                            }

                            if (!HasQuit())
                            {
                                // save recorded list again
                                recorded.WriteToFile(config.GetRecordedFile());
                            }
                        }
                    }
                }
            }
            else if (stricmp(argv[i], "--update-video-has-subtitles") == 0) {
                uint_t j, nchanged = 0;

                for (j = 0; (j < proglist.Count()) && !HasQuit(); j++) {
                    if (proglist.GetProgWritable(j).UpdateArchiveHasSubtitlesFlag()) {
                        nchanged++;
                    }
                }

                printf("%u programmes changed\n", nchanged);
            }
            else if (stricmp(argv[i], "---stream") == 0) {
                dvbstream_t stream;

                if (ConvertStream(argv[++i], stream)) {
                    res = system(stream.cmd.str());
                }
                else fprintf(stderr, "Invalid base64 value '%s'\n", argv[i]);
            }
            else if ((stricmp(argv[i], "--stream") == 0) ||
                     (stricmp(argv[i], "--rawstream") == 0) ||
                     (stricmp(argv[i], "--mp4stream") == 0) ||
                     (stricmp(argv[i], "--hlsstream") == 0) ||
                     (stricmp(argv[i], "--httpstream") == 0) ||
                     (stricmp(argv[i], "--rhttpstream") == 0)) {
                auto type       = StreamType_Raw;
                auto streamtype = AString(argv[i]);
                auto name       = AString(argv[++i]);

                if (streamtype == "--stream") {
                    type = StreamType_Video;
                }
                else if (streamtype == "--rawstream") {
                    type = StreamType_Raw;
                }
                else if (streamtype == "--mp4stream") {
                    type = StreamType_MP4;
                }
                else if (streamtype == "--hlsstream") {
                    type = StreamType_HLS;
                }
                else if (streamtype == "--httpstream") {
                    type = StreamType_HTTP;
                }
                else if (streamtype == "--rhttpstream") {
                    type = StreamType_RemoteHTTP;
                }

                dvbstream_t stream;
                StartDVBStream(stream, type, name, dvbcardspecified ? AString::Formatify("%u", dvbcard) : "");
            }
            else if (stricmp(argv[i], "--list-streams") == 0) {
                std::vector<dvbstream_t> streams;

                if (ListDVBStreams(streams)) {
                    for (size_t j = 0; j < streams.size(); j++) {
                        const auto& stream = streams[j];
                        printf("Stream '%s' (PID %u): '%s' (URL '%s')\n", stream.name.str(), stream.pid, stream.htmlfile.str(), stream.url.str());
                    }
                }
                else fprintf(stderr, "Failed to find DVB streams\n");
            }
            else if (stricmp(argv[i], "--stop-streams") == 0) {
                std::vector<dvbstream_t> streams;
                auto pattern = AString(argv[++i]);

                if (StopDVBStreams(pattern, streams)) {
                    for (size_t j = 0; j < streams.size(); j++) {
                        printf("Stopped stream '%s'\n", streams[j].name.str());
                    }
                }
                else fprintf(stderr, "Failed to stop DVB streams\n");
            }
            else if (stricmp(argv[i], "--stop-stream") == 0) {
                std::vector<dvbstream_t> streams;
                auto name = AString(argv[++i]);

                if (StopDVBStreams(name, streams)) {
                    for (size_t j = 0; j < streams.size(); j++) {
                        printf("Stopped stream '%s'\n", streams[j].name.str());
                    }
                }
                else fprintf(stderr, "Failed to stop DVB streams\n");
            }
            else if (stricmp(argv[i], "--describe-pid") == 0) {
                auto     pid = (uint32_t)AString(argv[++i]);
                APIDTree tree(pid);

                if (tree.Valid()) {
                    printf("%s", tree.Describe().str());
                }
                else fprintf(stderr, "Invalid pid %u\n", pid);
           }
            else if (stricmp(argv[i], "--kill-stream-pid") == 0) {
                auto     pid = (uint32_t)AString(argv[++i]);
                APIDTree tree(pid);

                if (tree.Valid()) {
                    res = tree.Kill() ? 0 : -1;
                }
                else fprintf(stderr, "Invalid pid %u\n", pid);
            }
            else if (stricmp(argv[i], "--return-count") == 0) {
                res = (int)proglist.Count();
            }
            else if (stricmp(argv[i], "--drawprogrammes") == 0) {
                const auto   hour = (uint64_t)3600U * (uint64_t)1000U;
                const uint_t namewidth = 20;
                std::map<AString, std::vector<const ADVBProg *> > map;
                std::map<AString, std::vector<const ADVBProg *> >::iterator it;
                auto   scale = (uint_t)AString(argv[++i]);
                uint_t j;

                proglist.Sort();

                for (j = 0; j < proglist.Count(); j++) {
                    const ADVBProg& prog = proglist[j];
                    map[prog.GetChannel()].push_back(&prog);
                }

                if (!map.empty()) {
                    uint64_t dt0   = UINT64_MAX;
                    uint64_t dtmax = 0U;

                    for (it = map.begin(); it != map.end(); ++it) {
                        const std::vector<const ADVBProg *>& list = it->second;

                        dt0   = std::min(dt0,   list.front()->GetStart());
                        dtmax = std::max(dtmax, list.back()->GetStop());
                    }

                    // round down to the nearest hour
                    dt0   -= dt0 % hour;

                    dtmax  = std::max(dtmax, dt0);

                    uint_t maxoffset = (uint_t)(((dtmax - dt0) * (uint64_t)scale) / hour);
                    {
                        uint_t offset;

                        printf(AString::Formatify("%%-%us|", namewidth).str(), AString("Channel").Abbreviate(namewidth).str());
                        for (offset = 0U; offset < maxoffset;) {
                            uint64_t dt   = dt0 + ((uint64_t)offset * hour) / scale;
                            auto     desc = ADateTime(dt).DateToStr();
                            uint     diff = (int)(scale - ((uint_t)desc.len() % scale));

                            if (diff < 1) diff += scale;

                            desc += AString(" ").Copies(diff - 1) + "|";

                            printf("%s", desc.str());
                            offset += (uint_t)desc.len();
                        }

                        maxoffset = std::max(maxoffset, offset);
                    }

                    printf("\n");

                    for (it = map.begin(); it != map.end(); ++it) {
                        const std::vector<const ADVBProg *>& list = it->second;
                        size_t k;

                        printf(AString::Formatify("%%-%us|", namewidth).str(), it->first.Abbreviate(namewidth).str());

                        uint_t offset = 0;
                        for (k = 0; k < list.size(); k++) {
                            uint_t startoffset = (uint_t)(((list[k]->GetStart() - dt0) * (uint64_t)scale) / hour);
                            uint_t stopoffset  = (uint_t)(((list[k]->GetStop()  - dt0) * (uint64_t)scale) / hour);

                            if (startoffset > offset) {
                                int len = (int)(startoffset - offset - 1);
                                printf("%s|", AString(" ").Copies(len).str());
                                offset += len + 1;
                            }

                            if (stopoffset > offset) {
                                auto len  = stopoffset - offset - 1;
                                auto desc = list[k]->GetTitleAndSubtitle().Abbreviate((int)len);
                                desc += AString(" ").Copies((int)(len - (uint_t)desc.GetCharCount()));
                                printf("%s|", desc.str());
                                offset += desc.GetCharCount() + 1;
                            }
                        }
                        if (maxoffset > offset) {
                            printf("%s|", AString(" ").Copies((int)(maxoffset - offset - 1)).str());
                        }
                        printf("\n");
                    }
                }
            }
            else if (stricmp(argv[i], "--list-config-values") == 0) {
                auto res = config.ListConfigValues() + "\n" + config.ListLiveConfigValues();
                printf("%s", res.str());
            }
            else if (stricmp(argv[i], "--config-item") == 0) {
                auto var = AString(argv[++i]);
                auto val = config.GetConfigItem(var);
                printf("%s=%s\n", var.str(), val.str());
            }
            else if (stricmp(argv[i], "--user-config-item") == 0) {
                auto user = AString(argv[++i]);
                auto var  = AString(argv[++i]);
                auto val  = config.GetUserConfigItem(user, var);
                printf("%s:%s=%s\n", user.str(), var.str(), val.str());
            }
            else if (stricmp(argv[i], "--user-category-config-item") == 0) {
                auto user     = AString(argv[++i]);
                auto category = AString(argv[++i]);
                auto var      = AString(argv[++i]);
                auto val      = config.GetUserSubItemConfigItem(user, category, var);
                printf("%s:%s:%s=%s\n", user.str(), var.str(), category.str(), val.str());
            }

#if EVALTEST
            else if (stricmp(argv[i], "--eval") == 0) {
                simplevars_t vars;
                auto         str = AString(argv[++i]);
                AString      errors;
                AValue       res = 0;
                AddSimpleFunction("func", &__func);

                if (Evaluate(vars, str, res, errors)) {
                    printf("Resultant: %s\n", AValue(res).ToString().str());
                }
                else if (errors.Valid()) printf("Errors:\n%s", errors.str());
                else printf("Failed to evaluate expression '%s'\n", str.str());
            }
#endif
            else if (stricmp(argv[i], "--test-cards-channel") == 0) {
                const auto& channels = ADVBChannelList::Get();
                const auto  name     = AString(argv[++i]);
                const auto  *channel = channels.GetChannelByName(name);

                if (channel != NULL) {
                    testcardchannel = name;
                }
                else {
                    fprintf(stderr, "Unknown channel for testing cards with '%s'\n", name.str());
                }
            }
            else if (stricmp(argv[i], "--test-cards-seconds") == 0) {
                testcardseconds = (uint_t)AString(argv[++i]);
            }
            else if (stricmp(argv[i], "--test-cards") == 0) {
                if (config.GetRecordingSlave().Valid()) {
                    if (testcardremotecmd.Empty()) {
                        testcardremotecmd.printf("dvb");
                    }

                    testcardremotecmd.printf(" --test-cards-channel \"%s\" --test-cards-seconds %u --test-cards", testcardchannel.str(), testcardseconds);
                }
                else if (!testcardsuccess) {
                    const auto& channels = ADVBChannelList::Get();
                    const auto  *channel = channels.GetChannelByName(testcardchannel);

                    if (channel != NULL) {
                        // map DVB cards
                        (void)config.GetPhysicalDVBCard(0);

                        AString basecmd = AString::Formatify("dvb --test-cards-channel \"%s\" --test-cards-seconds %u {cardarg}",
                                                             testcardchannel.str(), testcardseconds);

                        bool success = true;
                        std::vector<AString> filenames(config.GetMaxDVBCards());
                        for (uint_t card = 0; card < (uint_t)filenames.size(); card++) {
#if OLD_STYLE_TEST_CARDS
                            uint32_t bytes   = channels.TestCard(config.GetPhysicalDVBCard(card), channel, testcardseconds);
                            auto     rate    = (double)bytes / (1024.0 * (double)testcardseconds);
                            auto     invalid = (rate < 100.0);

                            printf("Card %u: %0.1fkb/s%s\n", card, rate, invalid ? " **INVALID**" : "");

                            if (invalid) {
                                success = false;
                            }
#else
                            filenames[card] = config.GetTempFile("carderrors", ".txt");
                            remove(filenames[card]);

                            printf("Starting %u seconds test on card %u...\n", testcardseconds, card);

                            AString cmd = AString::Formatify("%s >\"%s\" &", basecmd.SearchAndReplace("{cardarg}", AString::Formatify("--test-card %u", card)).str(), filenames[card].str());
                            success &= (system(cmd) == 0);
#endif
                        }

#if !OLD_STYLE_TEST_CARDS
                        printf("Waiting until all tests complete...\n");

                        uint_t ntests = 0;
                        AString cmd = AString::Formatify("pgrep -f \"%s\" | wc -l", basecmd.SearchAndReplace("{cardarg}", "--test-card [0-9]+").SearchAndReplace("\"", "").str());
                        do {
                            ntests = (uint_t)RunCommandAndGetResult(cmd);
                            if (ntests > 0) {
                                usleep(1000000);
                            }
                        } while (!HasQuit() && (ntests > 0));

                        if (!HasQuit()) {
                            std::vector<uint_t> errors(filenames.size());
                            for (uint_t card = 0; card < (uint_t)filenames.size(); card++) {
                                AString str;
                                if (str.ReadFromFile(filenames[card])) {
                                    errors[card] = (uint_t)str;

                                    const double rate     = (double)errors[card] / (double)(testcardseconds * 60);
                                    const bool   invalid  = ((errors[card] == 0) || (rate >= config.GetVideoErrorRateThreshold("", "")));

                                    printf("Card %u: %0.2f errors/min (%u in total)%s\n", card, rate, errors[card], invalid ? " **INVALID**" : "");

                                    if (invalid) {
                                        success = false;
                                    }
                                }
                                remove(filenames[card]);
                            }
                        }
#endif

                        testcardsuccess   = success;
                        testcardperformed = true;
                    }
                    else {
                        fprintf(stderr, "Unknown channel '%s'\n", testcardchannel.str());
                    }
                }
            }
            else if (stricmp(argv[i], "--test-card") == 0) {
                const auto   card     = (uint_t)AString(argv[++i]);
                const auto&  channels = ADVBChannelList::Get();
                const auto   *channel = channels.GetChannelByName(testcardchannel);
                const auto   errors   = channels.TestCard(config.GetPhysicalDVBCard(card), channel, testcardseconds);

                printf("%u\n", errors);
            }
            else if (stricmp(argv[i], "--combine-split-films") == 0)
            {
                proglist.CombineSplitFilms();
            }
        }
    }

    if (testcardremotecmd.Valid()) {
        testcardsuccess   = RunAndLogRemoteCommand(testcardremotecmd);
        testcardperformed = true;
    }

    if ((res == 0) && testcardperformed && !testcardsuccess) {
        res = -1;
    }

    return res;
}
