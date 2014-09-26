
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
#include "findcards.h"

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

	printf("{\"user\":\"%s\"", ADVBProg::JSONFormat(user).str());

	AString dir  = config.GetRecordingsSubDir(user);
	AString rdir = config.GetRecordingsDir(user);
	printf(",\"folder\":\"%s\"", ADVBProg::JSONFormat(dir).str());
	printf(",\"fullfolder\":\"%s\"", ADVBProg::JSONFormat(rdir).str());

	struct statvfs fiData;
	if (statvfs(rdir, &fiData) >= 0) {
		double gb = (double)fiData.f_bavail * (double)fiData.f_bsize / (1024.0 * 1024.0 * 1024.0);

		printf(",\"freespace\":\"%0.1lfG\"", gb);
		printf(",\"level\":%u", (uint_t)(gb / lowlimit));
	}

	printf("}");
}

void printpattern(const ADVBProg::PATTERN& pattern)
{
	uint_t i;

	printf("{\"user\":\"%s\"", 	  ADVBProg::JSONFormat(pattern.user).str());
	printf(",\"enabled\":%s",     pattern.enabled ? "true" : "false");
	printf(",\"pri\":%d",      	  pattern.pri);
	printf(",\"pattern\":\"%s\"", ADVBProg::JSONFormat(pattern.pattern).str());
	printf(",\"errors\":\"%s\"",  ADVBProg::JSONFormat(pattern.errors).str());
	printf(",\"terms\":[");
	for (i = 0; i < pattern.list.Count(); i++) {
		const ADVBProg::TERMDATA *data = ADVBProg::GetTermData(pattern, i);

		if (i) printf(",");
		printf("{\"field\":%u",		   (uint_t)data->field);
		printf(",\"opcode\":%u",  	   (uint_t)data->opcode);
		printf(",\"opindex\":%u",  	   (uint_t)data->opindex);
		printf(",\"value\":\"%s\"",    ADVBProg::JSONFormat(data->value).str());
		printf(",\"quotes\":%s",       (data->value.Pos(" ") >= 0) ? "true" : "false");
		printf(",\"assign\":%s",	   ADVBProg::OperatorIsAssign(pattern, i) ? "true" : "false");
		printf(",\"orflag\":%u",	   (uint_t)data->orflag);
		printf("}");
	}
	printf("]}");
}

void printpattern(AHash& patterns, const ADVBProg& prog)
{
	AString str;

	if ((str = prog.GetPattern()).Valid()) {
		ADVBProg::PATTERN *pattern;

		if (((pattern = (ADVBProg::PATTERN *)patterns.Read(str)) == NULL) && ((pattern = new ADVBProg::PATTERN) != NULL)) {
			ADVBProg::ParsePattern(str, *pattern, prog.GetUser());
		}

		if (pattern) {
			printf(",\"patternparsed\":");
			printpattern(*pattern);
		}
	}
}

int main(int argc, char *argv[])
{
	enum {
		DataSource_Progs = 0,
		DataSource_Patterns,
		DataSource_Logs,
	};
	const ADVBConfig& config = ADVBConfig::Get(true);
	ADVBProgList 	  recordedlist, scheduledlist, requestedlist, rejectedlist, combinedlist, runninglist;
	ADVBProgList 	  list[2], *proglist = NULL;
	ADataList	      patternlist;
	ADVBProg::PATTERN filterpattern;
	AHash 			  vars(20, &AString::DeleteString);
	AHash 			  patterns(50, &ADVBProg::DeletePattern);
	AHash 			  fullseries;
	AString   		  val, logdata, errors;
	uint_t 			  pagesize = (uint_t)config.GetConfigItem("pagesize", "20"), page = 0;
	uint_t 			  index = 0;
	uint_t 			  datasource = DataSource_Progs;
	bool 			  base64encoded = true;
	int i;

	recordedlist.ReadFromFile(config.GetRecordedFile());
	recordedlist.CreateHash();

	scheduledlist.ReadFromFile(config.GetScheduledFile());
	scheduledlist.CreateHash();

	requestedlist.ReadFromFile(config.GetRequestedFile());
	rejectedlist.ReadFromFile(config.GetRejectedFile());

	runninglist.ReadFromJobList(true);

	combinedlist.ReadFromFile(config.GetCombinedFile());
	combinedlist.CreateHash();
	combinedlist.FindSeries(fullseries);

	for (i = 1; i < argc; i++) {
		AString arg = argv[i];

		if (arg == "cli") {
			base64encoded = false;
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

				//debug("Setting '%s' as '%s'\n", var.str(), val.str());

				vars.Insert(var, (uptr_t)new AString(val));
			}
		}
	}

	if (Value(vars, val, "edit")) {
		AString edit = val.ToLower();
		AString user, pattern;
		AString newuser, newpattern;

		Value(vars, user, "user");
		Value(vars, pattern, "pattern");
		Value(vars, newuser, "newuser");
		Value(vars, newpattern, "newpattern");
			
		if (edit == "add") {
			ADVBProg::InsertPattern(newuser, newpattern);
		}
		else if (edit == "update") {
			ADVBProg::UpdatePattern(user, pattern, newuser, newpattern);
		}
		else if (edit == "enable") {
			ADVBProg::EnablePattern(user, pattern);
		}
		else if (edit == "disable") {
			ADVBProg::DisablePattern(user, pattern);
		}
		else if (edit == "delete") {
			ADVBProg::DeletePattern(user, pattern);
		}
	}

	if (Value(vars, val, "schedule")) {
		ADVBProgList::SchedulePatterns(ADateTime().TimeStamp(true), (val == "commit"));
	}

	{
		AString errors;		// use local var to prevent global one being modified
		ADVBProgList::ReadPatterns(patternlist, errors);
	}

	if (Value(vars, val, "from")) {
		val = val.ToLower();

		if (val == "listings") {
			proglist = list + index;
			proglist->ReadFromFile(config.GetListingsFile());
			index = (index + 1) % NUMBEROF(list);
		}
		else if (val == "recorded") {
			proglist = &recordedlist;
		}
		else if (val == "requested") {
			proglist = &requestedlist;
		}
		else if (val == "scheduled") {
			proglist = &scheduledlist;
		}
		else if (val == "rejected") {
			proglist = &rejectedlist;
		}
		else if (val == "combined") {
			proglist = &combinedlist;
		}
		else if (val == "patterns") {
			datasource = DataSource_Patterns;
		}
		else if (val == "sky") {
			proglist = list + index;
			proglist->ReadFromFile(config.GetDataDir().CatPath("skylistings.dat"));
			index = (index + 1) % NUMBEROF(list);
		}
		else if (val == "logs") {
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
	}

	if (Value(vars, val, "pagesize")) {
		int pval = (int)val;

		pagesize = (uint_t)LIMIT(pval, 1, MAX_SIGNED(sint_t));
	}
	
	if (Value(vars, val, "page")) {
		int pval = (int)val;

		page = (uint_t)LIMIT(pval, 0, MAX_SIGNED(sint_t));
	}

	if ((datasource == DataSource_Progs) && !proglist) {
		proglist = list + index;
		proglist->ReadFromFile(config.GetListingsFile());
		index = (index + 1) % NUMBEROF(list);
	}

	if ((datasource == DataSource_Progs) && Value(vars, val, "filter") && val.Valid()) {
		if (!proglist) {
			proglist = list + index;
			proglist->ReadFromFile(config.GetListingsFile());
			index = (index + 1) % NUMBEROF(list);
		}

		ADVBProgList *reslist = list + index;
		index = (index + 1) % NUMBEROF(list);

		AString filter = val;
		ADVBProg::ParsePattern(filter, filterpattern);
		proglist->FindProgrammes(*reslist, filter, errors, (filter.Pos("\n") >= 0) ? "\n" : ";");
		proglist = reslist;
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
				ADVBProg::PATTERN *pattern = (ADVBProg::PATTERN *)patternlist[i];
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

		printf("\"total\":%u", nitems);
		printf(",\"page\":%u", page);
		printf(",\"pagesize\":%u", pagesize);
		printf(",\"from\":%u", offset);
		printf(",\"for\":%u", count);
		printf(",\"parsedpattern\":");
		printpattern(filterpattern);
		printf(",\"errors\":\"%s\"", ADVBProg::JSONFormat(errors).str());

		if (Value(vars, val, "patterndefs")) {
			printf(",%s", ADVBProg::GetPatternDefinitionsJSON().str());
		}

		FILE_INFO info;
		if (::GetFileInfo(config.GetSearchesFile(), &info)) {
			if (!Value(vars, val, "searchesref") || ((uint64_t)val < (uint64_t)info.WriteTime)) {
				AStdFile fp;

				if (fp.open(config.GetSearchesFile())) {
					AString line;

					printf(",\"searchesref\":%" FMT64 "u", (uint64_t)info.WriteTime);
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
							printf("{\"title\":\"%s\"", ADVBProg::JSONFormat(title).str());
							printf(",\"search\":{");
							if (from.Valid()) printf("\"from\":\"%s\",", ADVBProg::JSONFormat(from).str());
							printf("\"titlefilter\":\"%s\"}}", ADVBProg::JSONFormat(filter).str());
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
				printf(",\"progs\":[");

				for (i = 0; (i < count) && !config.HasQuit(); i++) {
					const ADVBProg& prog = proglist->GetProg(offset + i);
					const ADVBProg *prog2;

					if (i > 0) printf(",");

					if (runninglist.FindUUID(prog)) ((ADVBProg&)prog).SetRunning();

					printf("{");
					printf("%s", prog.ExportToJSON(true).str());
					printpattern(patterns, prog);

					if ((prog2 = scheduledlist.FindUUID(prog)) != NULL) {
						printf(",\"scheduled\":{");
						printf("%s", prog2->ExportToJSON().str());
						printpattern(patterns, *prog2);
						printf("}");
					}

					if ((prog2 = recordedlist.FindSimilar(prog)) != NULL) {
						printf(",\"recorded\":{");
						printf("%s", prog2->ExportToJSON().str());
						printpattern(patterns, *prog2);
						printf("}");
					}

					if ((prog2 = rejectedlist.FindUUID(prog)) != NULL) {
						printf(",\"rejected\":{");
						printf("%s", prog2->ExportToJSON().str());
						printpattern(patterns, *prog2);
						printf("}");
					}

					const ADVBProgList::SERIES *serieslist = (const ADVBProgList::SERIES *)fullseries.Read(prog.GetTitle());
					if (serieslist) {
						uint_t j;

						printf(",\"series\":[");
						for (j = 0; j < serieslist->list.Count(); j++) {
							const AString *str = (const AString *)serieslist->list[j];

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

					printf("}");
				}

				printf("]");
				break;

			case DataSource_Patterns:
				printf(",\"patterns\":[");

				for (i = 0; (i < count) && !config.HasQuit(); i++) {
					const ADVBProg::PATTERN& pattern = *(const ADVBProg::PATTERN *)patternlist[offset + i];

					if (i > 0) printf(",");

					printpattern(pattern);
				}		

				printf("]");
				break;

			case DataSource_Logs:
				printf(",\"loglines\":[");

				for (i = 0; (i < count) && !config.HasQuit(); i++) {
					if (i > 0) printf(",");
					printf("\"%s\"", ADVBProg::JSONFormat(logdata.Line(offset + i, "\n", 0).SearchAndReplace("  ", "&nbsp;")).str());
				}

				printf("]");
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

		ADVBProgList::CheckDiskSpace(patternlist, false, true);

		{
			printf(",\"counts\":{");
			printf("\"scheduled\":%u", scheduledlist.Count());
			printf(",\"recorded\":%u", recordedlist.Count());
			printf(",\"requested\":%u", requestedlist.Count());
			printf(",\"rejected\":%u", rejectedlist.Count());
			printf(",\"combined\":%u", combinedlist.Count());
			printf("}");
		}
	}

	printf("}");

	return 0;
}
