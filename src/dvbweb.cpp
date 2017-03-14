
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/statvfs.h>

#include <rdlib/Hash.h>
#include <rdlib/Recurse.h>
#include <rdlib/Regex.h>

#include "config.h"
#include "dvblock.h"
#include "proglist.h"
#include "dvbpatterns.h"
#include "findcards.h"

typedef struct {
	AString title;
	struct {
		uint_t  recorded;
		uint_t  available;
		uint_t  scheduled;
		uint_t  failed;
		uint_t  isfilm;
		uint_t  notfilm;
		uint_t  total;
	} counts;
} PROGTITLE;

bool Value(const AHash& vars, AString& val, const AString& var)
{
	AString *p;

	if ((p = (AString *)vars.Read(var)) != NULL) val = *p;
	else val.Delete();

	return (p != NULL);
}

void printuserdetails(const AString& user)
{
	const ADVBConfig& config = ADVBConfig::Get();
	double lowlimit = config.GetLowSpaceWarningLimit();

	printf("{\"user\":\"%s\"", JSONFormat(user).str());

	AString dir  = config.GetRecordingsSubDir(user);
	AString rdir = config.GetRecordingsDir(user);

	int p;
	if ((p = dir.Pos("/{"))  >= 0) dir  = dir.Left(p);
	if ((p = rdir.Pos("/{")) >= 0) rdir = rdir.Left(p);

	printf(",\"folder\":\"%s\"", JSONFormat(dir).str());
	printf(",\"fullfolder\":\"%s\"", JSONFormat(rdir).str());

	struct statvfs fiData;
	if (statvfs(rdir, &fiData) >= 0) {
		double gb = (double)fiData.f_bavail * (double)fiData.f_bsize / ((uint64_t)1024.0 * (uint64_t)1024.0 * (uint64_t)1024.0);

		printf(",\"freespace\":\"%0.1lfG\"", gb);
		printf(",\"level\":%u", (uint_t)(gb / lowlimit));
	}

	printf("}");
}

void printpattern(const ADVBPatterns::PATTERN& pattern)
{
	uint_t i;

	printf("{\"user\":\"%s\"", 	  JSONFormat(pattern.user).str());
	printf(",\"enabled\":%s",     pattern.enabled ? "true" : "false");
	printf(",\"pri\":%d",      	  pattern.pri);
	printf(",\"pattern\":\"%s\"", JSONFormat(pattern.pattern).str());
	printf(",\"errors\":\"%s\"",  JSONFormat(pattern.errors).str());
	printf(",\"terms\":[");
	for (i = 0; i < pattern.list.Count(); i++) {
		const ADVBPatterns::TERMDATA *data = ADVBPatterns::GetTermData(pattern, i);

		if (i) printf(",");
		printf("{\"start\":\%u",       data->start);
		printf(",\"length\":\%u",      data->length);
		printf(",\"field\":%u",		   (uint_t)data->field);
		printf(",\"opcode\":%u",  	   (uint_t)data->opcode);
		printf(",\"opindex\":%u",  	   (uint_t)data->opindex);
		printf(",\"value\":\"%s\"",    JSONFormat(data->value).str());
		printf(",\"quotes\":%s",       (data->value.Pos(" ") >= 0) ? "true" : "false");
		printf(",\"assign\":%s",	   ADVBPatterns::OperatorIsAssign(pattern, i) ? "true" : "false");
		printf(",\"orflag\":%u",	   (uint_t)data->orflag);
		printf("}");
	}
	printf("]}");
}

void printpattern(AHash& patterns, const ADVBProg& prog)
{
	AString str;

	if ((str = prog.GetPattern()).Valid()) {
		ADVBPatterns::PATTERN *pattern;

		if (((pattern = (ADVBPatterns::PATTERN *)patterns.Read(str)) == NULL) && ((pattern = new ADVBPatterns::PATTERN) != NULL)) {
			ADVBPatterns::ParsePattern(str, *pattern, prog.GetUser());
		}

		if (pattern) {
			printf(",\"patternparsed\":");
			printpattern(*pattern);
		}
	}
}

void printseries(const ADVBProgList::SERIES& serieslist)
{
	uint_t j;

	printf(",\"series\":[");
	for (j = 0; j < serieslist.list.Count(); j++) {
		const AString *str = (const AString *)serieslist.list[j];

		if (j > 0) printf(",");
		printf("{\"state\":");
		if (str) {
			if (str->Pos("-") >= 0) printf("\"incomplete\"");
			else					printf("\"complete\"");
		}
		else printf("\"empty\"");
		printf(",\"episodes\":\"%s\"}", str ? str->str() : "");
	}
	printf("]");
}

int inserttitle(uptr_t value1, uptr_t value2, void *context)
{
	const PROGTITLE *title1 = (const PROGTITLE *)value1;
	const PROGTITLE *title2 = (const PROGTITLE *)value2;

	(void)context;

	return CompareNoCase(title1->title, title2->title);
}

void deletetitle(uptr_t value, void *context)
{
	(void)context;
	delete (PROGTITLE *)value;
}

int main(int argc, char *argv[])
{
	const ADVBConfig& config = ADVBConfig::Get(true);
	ADVBProg		  prog;	// ensure ADVBProg initialisation takes place
	AStdFile          log; //("/home/richard/dvbweb.log", "w");
	AHash 			  vars(20, &AString::DeleteString);
	AString   		  val, logdata, errors;
	bool 			  base64encoded = true;
	int i;

	(void)prog;

	enabledebug(false);

	for (i = 1; i < argc; i++) {
		AString arg = argv[i];

		if (arg == "cli") {
			base64encoded = false;
			enabledebug(true);
			continue;
		}

		if (base64encoded) arg = arg.Base64Decode();

		uint_t j, n = arg.CountLines("\n", 0);

		//debug("dvbweb: %u lines: %s\n", n, arg.str());

		for (j = 0; j < n; j++) {
			AString line = arg.Line(j, "\n", 0);
			int p;

			if ((p = line.Pos("=")) > 0) {
				AString var = line.Left(p);
				AString val = line.Mid(p + 1);

				log.printf("%s='%s'\n", var.str(), val.str());

				//debug("Setting '%s' as '%s'\n", var.str(), val.str());

				vars.Insert(var, (uptr_t)new AString(val));
			}
		}
	}

	if (Value(vars, val, "editpattern")) {
		AString edit = val.ToLower();
		AString user, pattern;

		AString newuser, newpattern;

		Value(vars, user, "user");
		Value(vars, pattern, "pattern");
		Value(vars, newuser, "newuser");
		Value(vars, newpattern, "newpattern");

		if (edit == "add") {
			ADVBPatterns::InsertPattern(newuser, newpattern);
		}
		else if (edit == "update") {
			ADVBPatterns::UpdatePattern(user, pattern, newuser, newpattern);
		}
		else if (edit == "enable") {
			ADVBPatterns::EnablePattern(user, pattern);
		}
		else if (edit == "disable") {
			ADVBPatterns::DisablePattern(user, pattern);
		}
		else if (edit == "delete") {
			ADVBPatterns::DeletePattern(user, pattern);
		}
	}
	
	if (Value(vars, val, "schedule")) {
		bool commit = (val == "commit");

		ADVBProgList::SchedulePatterns(ADateTime().TimeStamp(true), commit);
	}

	if (Value(vars, val, "parse")) {
		ADVBPatterns::PATTERN pattern;
		AString user;

		Value(vars, user, "user");

		ADVBPatterns::ParsePattern(val, pattern, user);
		AString newpattern = ADVBPatterns::RemoveDuplicateTerms(pattern);

		printf("{");
		printf("\"newpattern\":\"%s\"", JSONFormat(newpattern).str());
		ADVBPatterns::ParsePattern(newpattern, pattern, pattern.user);
		printf(",\"parsedpattern\":");
		printpattern(pattern);
		printf("}");
	}
	else {
		enum {
			DataSource_Progs = 0,
			DataSource_Titles,
			DataSource_Patterns,
			DataSource_Logs,
		};
		ADVBProgList 	  recordedlist, scheduledlist, requestedlist, rejectedlist, combinedlist, failureslist, recordinglist, processinglist;
		ADVBProgList 	  list[2], *proglist = NULL;
		AHash     		  titleshash;
		ADataList 		  titleslist;
		ADataList	      patternlist;
		ADVBPatterns::PATTERN filterpattern;
		AHash 			  patterns(50, &ADVBPatterns::__DeletePattern);
		AHash 			  fullseries;
		AString           from;
		uint_t 			  pagesize = (uint_t)config.GetConfigItem("pagesize", "20"), page = 0;
		uint_t 			  datasource = DataSource_Progs;
		uint_t 			  index = 0;

		recordedlist.ReadFromFile(config.GetRecordedFile());
		recordedlist.CreateHash();

		scheduledlist.ReadFromFile(config.GetScheduledFile());
		scheduledlist.CreateHash();

		requestedlist.ReadFromFile(config.GetRequestedFile());
		rejectedlist.ReadFromFile(config.GetRejectedFile());

		failureslist.ReadFromFile(config.GetRecordFailuresFile());
		recordinglist.ReadFromFile(config.GetRecordingFile());
		processinglist.ReadFromFile(config.GetProcessingFile());

		combinedlist.ReadFromFile(config.GetCombinedFile());
		combinedlist.CreateHash();
		combinedlist.FindSeries(fullseries);

		if (Value(vars, val, "deleteprogramme")) {
			const ADVBProg *pprog;

			if ((pprog = combinedlist.FindUUID(val)) != NULL) {
				ADVBProg prog = *pprog;
				AString  type;

				Value(vars, type, "type");

				if ((type == "all") || (type == "video")) {
					if (!prog.DeleteEncodedFiles()) {
						errors.printf("Failed to delete all files");
					}
				}

				if ((type == "all") || (type == "recordlist")) {
					config.logit("Deleting '%s' from recorded and failed lists", prog.GetQuickDescription().str());

					ADVBProgList::RemoveFromRecordLists(prog);

					recordedlist.DeleteAll();
					recordedlist.ReadFromFile(config.GetRecordedFile());
					recordedlist.CreateHash();

					combinedlist.DeleteAll();
					combinedlist.ReadFromFile(config.GetCombinedFile());
					combinedlist.CreateHash();

					failureslist.DeleteAll();
					failureslist.ReadFromFile(config.GetRecordFailuresFile());
				}
			}
			else errors.printf("Failed to find UUID '%s' for file deletion", val.str());
		}

		{
			AString errors;		// use local var to prevent global one being modified
			ADVBProgList::ReadPatterns(patternlist, errors);
		}

		Value(vars, from, "from");

		from = from.ToLower();

		if (from == "listings") {
			proglist = list + index;
			proglist->ReadFromFile(config.GetListingsFile());
			index = (index + 1) % NUMBEROF(list);
		}
		else if (from == "recorded") {
			proglist = &recordedlist;
		}
		else if (from == "requested") {
			proglist = &requestedlist;
		}
		else if (from == "scheduled") {
			proglist = &scheduledlist;
		}
		else if (from == "rejected") {
			proglist = &rejectedlist;
		}
		else if (from == "combined") {
			proglist = &combinedlist;
		}
		else if (from == "failures") {
			proglist = &failureslist;
		}
		else if (from == "processing") {
			proglist = &processinglist;
		}
		else if (from == "titles") {
			proglist   = &combinedlist;
			datasource = DataSource_Titles;
		}
		else if (from == "patterns") {
			datasource = DataSource_Patterns;
		}
		else if (from == "logs") {
			if (Value(vars, val, "timefilter")) {
				ADateTime dt;
				int p;

				if ((p = val.Pos("start=")) >= 0) val = val.Mid(p + 6).DeQuotify();

				dt.StrToDate(val);
				uint32_t days = dt.GetDays();
				if (days > ADateTime().GetDays()) days -= 7;

				logdata = AString::ReadFile(config.GetLogFile(days));
				datasource = DataSource_Logs;
			}
		}

		if ((datasource == DataSource_Progs) && !proglist) {
			proglist = list + index;
			proglist->ReadFromFile(config.GetListingsFile());
			index = (index + 1) % NUMBEROF(list);
		}

		if (((datasource == DataSource_Progs) ||
			 (datasource == DataSource_Titles)) &&
			Value(vars, val, "filter") &&
			val.Valid()) {
			if (!proglist) {
				proglist = list + index;
				proglist->ReadFromFile(config.GetListingsFile());
				index = (index + 1) % NUMBEROF(list);
			}

			ADVBProgList *reslist = list + index;
			index = (index + 1) % NUMBEROF(list);

			AString filter = val;
			ADVBPatterns::ParsePattern(filter, filterpattern, errors);
			proglist->FindProgrammes(*reslist, filter, errors, (filter.Pos("\n") >= 0) ? "\n" : ";");
			proglist = reslist;
		}

		if (Value(vars, val, "pagesize")) {
			int pval = (int)val;

			pagesize = (uint_t)LIMIT(pval, 1, MAX_SIGNED(sint_t));
		}

		if (Value(vars, val, "page")) {
			int pval = (int)val;

			page = (uint_t)LIMIT(pval, 0, MAX_SIGNED(sint_t));
		}

		if ((datasource == DataSource_Patterns) && Value(vars, val, "filter") && val.Valid()) {
			AString user_pat    = ParseRegex("*");
			AString pattern_pat = ParseRegex("*");
			char    user_op     = '~';
			char    pattern_op  = '~';
			uint_t  i = 0;
			bool    enabled_check = false;
			bool    enabled_value = false;
			bool    success       = true;

			do {
				while (IsWhiteSpace(val[i])) i++;

				if (!val[i]) break;

				uint_t fs = i;
				while (IsAlphaChar(val[i])) i++;
				if (!val[i]) break;

				AString field = AString(val.str() + fs, i - fs).ToLower();

				while (IsWhiteSpace(val[i])) i++;
				if (!val[i]) break;

				char oper = val[i++];

				while (IsWhiteSpace(val[i])) i++;
				if (!val[i]) break;

				char quote = 0;
				if (IsQuoteChar(val[i])) quote = val[i++];
				fs = i;
				while (!IsWhiteSpace(val[i]) && (val[i] != quote)) i++;
				AString value = AString(val.str() + fs, i - fs).ToLower();
				if (quote && (val[i] == quote)) i++;
				while (IsWhiteSpace(val[i])) i++;

				AString *pat = NULL;
				char    *op  = NULL;
				if (field == "user") {
					pat = &user_pat;
					op  = &user_op;
				}
				else if (field == "pattern") {
					pat = &pattern_pat;
					op  = &pattern_op;
				}
				else if (field == "enabled") {
					enabled_check = true;
					enabled_value = (((uint_t)value > 0) || (value == "yes") || (value == "true"));
					continue;
				}

				if (pat && op) {
					switch (oper) {
						case '=':
						case '@':
							*pat = value;
							*op  = oper;
							break;

						case '~':
							*pat = ParseRegex(value);
							*op  = oper;
							break;

						default:
							errors.printf("Unrecognized operator '%c' for pattern filters", oper);
							success = false;
							break;
					}
				}

				if (!success) break;
			}
			while (val[i]);

			if (success) {
				for (i = 0; i < patternlist.Count();) {
					ADVBPatterns::PATTERN *pattern = (ADVBPatterns::PATTERN *)patternlist[i];
					bool keep = (!enabled_check || (enabled_value == pattern->enabled));

					if (keep) {
						switch (user_op) {
							case '=': keep &= (pattern->user.CompareNoCase(user_pat) == 0); break;
							case '@': keep &= (pattern->user.PosNoCase(user_pat) >= 0); break;
							case '~': keep &= MatchRegex(pattern->user, user_pat); break;
							default: break;
						}
					}

					if (keep) {
						switch (pattern_op) {
							case '=': keep &= (pattern->pattern.CompareNoCase(pattern_pat) == 0); break;
							case '@': keep &= (pattern->pattern.PosNoCase(pattern_pat) >= 0); break;
							case '~': keep &= MatchRegex(pattern->pattern, pattern_pat); break;
							default: break;
						}
					}

					if (keep) i++;
					else {
						patternlist.RemoveIndex(i);
						delete pattern;
					}
				}
			}
		}

		printf("{");

		{
			uint_t i, nitems, offset, count = pagesize;

			switch (datasource) {
				default:
				case DataSource_Progs:
					nitems = proglist->Count();
					break;

				case DataSource_Titles:
					titleshash.Create(50, &deletetitle);
					titleshash.EnableCaseInSensitive(true);

					for (i = 0; i < proglist->Count(); i++) {
						const ADVBProg& prog = proglist->GetProg(i);
						PROGTITLE *title;

						if (((title = (PROGTITLE *)titleshash.Read(prog.GetTitle())) == NULL) && ((title = new PROGTITLE) != NULL)) {
							title->title = prog.GetTitle();
							memset(&title->counts, 0, sizeof(title->counts));

							titleshash.Insert(prog.GetTitle(), (uptr_t)title);
							titleslist.Insert((uptr_t)title, &inserttitle);
						}

						if (title) {
							if (prog.IsRecorded())  	   title->counts.recorded++;
							if (AStdFile::exists(prog.GetFilename())) title->counts.available++;
							if (prog.IsScheduled()) 	   title->counts.scheduled++;
							if (prog.HasRecordingFailed()) title->counts.failed++;
							if (prog.IsFilm())			   title->counts.isfilm++;
							else						   title->counts.notfilm++;
							title->counts.total++;
						}
					}
					nitems = titleslist.Count();
					break;

				case DataSource_Patterns:
					nitems = patternlist.Count();
					break;

				case DataSource_Logs:
					nitems = logdata.CountLines("\n", 0);
					break;
			}

			page   = MIN(page, (nitems / pagesize));
			offset = page * pagesize;
			if (page && (offset == nitems)) {
				page--;
				offset = page * pagesize;
			}
			count = MIN(count, SUBZ(nitems, offset));

			//printf("\n\npage: %u\npagesize: %u\ncount: %u\nnitems: %u\n\n", page, pagesize, count, nitems);

			printf("\"total\":%u", nitems);
			printf(",\"page\":%u", page);
			printf(",\"pagesize\":%u", pagesize);
			printf(",\"from\":%u", offset);
			printf(",\"for\":%u", count);
			printf(",\"parsedpattern\":");
			printpattern(filterpattern);
			printf(",\"errors\":\"%s\"", JSONFormat(errors).str());

			if (Value(vars, val, "patterndefs")) {
				printf(",%s", ADVBPatterns::GetPatternDefinitionsJSON().str());
			}

			FILE_INFO info;
			if (::GetFileInfo(config.GetSearchesFile(), &info)) {
				if (!Value(vars, val, "searchesref") || ((uint64_t)info.WriteTime > (uint64_t)val)) {
					AStdFile fp;

					if (fp.open(config.GetSearchesFile())) {
						AString line;

						printf(",\"searchesref\":%s", AValue((uint64_t)info.WriteTime).ToString().str());
						printf(",\"searches\":[");
						bool needscomma = false;
						while (line.ReadLn(fp) >= 0) {
							int p;

							if (IsSymbolChar(line[0]) && ((p = line.Pos("=")) > 0)) {
								AString title  = line.Left(p).Words(0);
								AString search = line.Mid(p + 1).Words(0);
								AString from   = search.Word(0);
								AString filter = search.Words(1);

								if (needscomma) printf(",");
								printf("{\"title\":\"%s\"", JSONFormat(title).str());
								printf(",\"search\":{");
								if (from.Valid()) printf("\"from\":\"%s\",", JSONFormat(from).str());
								printf("\"titlefilter\":\"%s\"}}", JSONFormat(filter).str());
								needscomma = true;
							}
						}
						printf("]");

						fp.close();
					}
				}
			}

			switch (datasource) {
				default:
				case DataSource_Progs:
					printf(",\"progs\":[\n");

					for (i = 0; (i < count) && !HasQuit(); i++) {
						ADVBProg& prog = proglist->GetProgWritable(offset + i);
						const ADVBProg *prog2;

						if (i > 0) printf(",\n");

						if (from == "listings") {
							if		((prog2 = processinglist.FindUUID(prog)) != NULL) prog.Modify(*prog2);
							else if ((prog2 = recordinglist.FindUUID(prog))  != NULL) prog.Modify(*prog2);
							else if	((prog2 = failureslist.FindUUID(prog))   != NULL) prog.Modify(*prog2);
							else if ((prog2 = rejectedlist.FindUUID(prog))   != NULL) prog.Modify(*prog2);
							else if ((prog2 = scheduledlist.FindUUID(prog))  != NULL) prog.Modify(*prog2);
							else if ((prog2 = recordedlist.FindUUID(prog))   != NULL) prog.Modify(*prog2);
						}

						printf("{");
						printf("%s", prog.ExportToJSON(true).str());
						printpattern(patterns, prog);

						if (((prog2 = recordedlist.FindSimilar(prog)) != NULL) && (prog2 != &prog)) {
							printf(",\"recorded\":{");
							printf("%s", prog2->ExportToJSON(true).str());
							printpattern(patterns, *prog2);
							printf("}");
						}

						const ADVBProgList::SERIES *serieslist = (const ADVBProgList::SERIES *)fullseries.Read(prog.GetTitle());
						if (serieslist) printseries(*serieslist);

						printf("}");
					}

					printf("]\n");
					break;

				case DataSource_Titles:
					printf(",\"titles\":[\n");

					for (i = 0; i < count; i++) {
						const PROGTITLE& title = *(const PROGTITLE *)titleslist[i + offset];

						if (i) printf(",");
						printf("{\"title\":\"%s\"", title.title.str());
						printf(",\"scheduled\":%u", title.counts.scheduled);
						printf(",\"recorded\":%u", title.counts.recorded);
						printf(",\"available\":%u", title.counts.available);
						printf(",\"failed\":%u", title.counts.failed);
						printf(",\"isfilm\":%u", title.counts.isfilm);
						printf(",\"notfilm\":%u", title.counts.notfilm);
						printf(",\"total\":%u", title.counts.total);

						const ADVBProgList::SERIES *serieslist = (const ADVBProgList::SERIES *)fullseries.Read(title.title);
						if (serieslist) printseries(*serieslist);

						printf("}");
					}

					printf("]\n");
					break;

				case DataSource_Patterns:
					printf(",\"patterns\":[\n");

					for (i = 0; (i < count) && !HasQuit(); i++) {
						const ADVBPatterns::PATTERN& pattern = *(const ADVBPatterns::PATTERN *)patternlist[offset + i];

						if (i > 0) printf(",\n");

						printpattern(pattern);
					}

					printf("]");
					break;

				case DataSource_Logs:
					printf(",\"loglines\":[");

					for (i = 0; (i < count) && !HasQuit(); i++) {
						if (i > 0) printf(",");
						printf("\"%s\"", JSONFormat(logdata.Line(offset + i, "\n", 0).SearchAndReplace("\t", " ").StripUnprintable()).str());
					}

					printf("]\n");
					break;
			}

			{
				AList users;
				printf(",\"users\":[");

				printuserdetails("");

				config.ListUsers(users);

				const AString *str = AString::Cast(users.First());
				while (str) {
					printf(",");

					printuserdetails(*str);

					str = str->Next();
				}

				printf("]");
			}

			ADVBProgList::CheckDiskSpace(false, true);

			{
				printf(",\"counts\":{");
				printf("\"scheduled\":%u", scheduledlist.Count());
				printf(",\"recorded\":%u", recordedlist.Count());
				printf(",\"requested\":%u", requestedlist.Count());
				printf(",\"rejected\":%u", rejectedlist.Count());
				printf(",\"combined\":%u", combinedlist.Count());
				printf("}");
			}

			{
				FILE_INFO info1, info2;
				uint64_t writetime;
				
				if ((::GetFileInfo(config.GetRecordedFile(), &info1) ||
					 ::GetFileInfo(config.GetCombinedFile(), &info2)) &&
					((writetime = std::max((uint64_t)info1.WriteTime, (uint64_t)info2.WriteTime)) > (uint64_t)ADateTime::MinDateTime) &&
					(!Value(vars, val, "statsref") || (writetime > (uint64_t)val))) {
					ADVBProgList::TREND trend;
				
					printf(",\"stats\":{");
					printf("\"ref\":%s", AValue(writetime).ToString().str());

					if ((trend = recordedlist.CalculateTrend(ADateTime("utc midnight-4w"))).valid) {
						printf(",\"last4weeks\":{\"offset\":%0.9lf,\"rate\":%0.9lf,\"timeoffset\":%0.14le}", trend.offset, trend.rate, trend.timeoffset);
					}
					if ((trend = recordedlist.CalculateTrend(ADateTime("utc midnight-1w"))).valid) {
						printf(",\"lastweek\":{\"offset\":%0.9lf,\"rate\":%0.9lf,\"timeoffset\":%0.14le}", trend.offset, trend.rate, trend.timeoffset);
					}
					if ((trend = recordedlist.CalculateTrend(ADateTime("utc midnight-6M"))).valid) {
						printf(",\"last6months\":{\"offset\":%0.9lf,\"rate\":%0.9lf,\"timeoffset\":%0.14le}", trend.offset, trend.rate, trend.timeoffset);
					}
					if ((trend = recordedlist.CalculateTrend(ADateTime("utc midnight-1Y"))).valid) {
						printf(",\"lastyear\":{\"offset\":%0.9lf,\"rate\":%0.9lf,\"timeoffset\":%0.14le}", trend.offset, trend.rate, trend.timeoffset);
					}
					if ((trend = combinedlist.CalculateTrend(ADateTime("utc midnight"))).valid) {
						printf(",\"scheduled\":{\"offset\":%0.9lf,\"rate\":%0.9lf,\"timeoffset\":%0.14le}", trend.offset, trend.rate, trend.timeoffset);
					}
					printf("}");
				}
			}
			
			{
				printf(",\"globals\":{");
				printf("\"candelete\":%s", (uint_t)config.GetConfigItem("webcandelete", "0") ? "true" : "false");
				printf("}");
			}
		}

		printf("}");
	}

	return 0;
}
