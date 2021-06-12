
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/statvfs.h>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

#include <rdlib/Hash.h>
#include <rdlib/Recurse.h>
#include <rdlib/Regex.h>

#include "config.h"
#include "dvblock.h"
#include "proglist.h"
#include "dvbpatterns.h"
#include "findcards.h"
#include "channellist.h"
#include "dvbstreams.h"

typedef struct {
    AString title;
    AString desc;
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

bool Value(const std::map<AString,AString>& vars, AString& val, const AString& var)
{
    auto it = vars.find(var);

    if (it != vars.end()) {
        val = it->second;
    }

    return (it != vars.end());
}

rapidjson::Value getuserdetails(rapidjson::Document& doc, const AString& user)
{
    const ADVBConfig& config = ADVBConfig::Get();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
    rapidjson::Value obj;
    double lowlimit = config.GetLowSpaceWarningLimit();

    obj.SetObject();

    obj.AddMember("user", rapidjson::Value(user.str(), allocator), allocator);

    AString dir  = config.GetRecordingsSubDir(user);
    AString rdir = config.GetRecordingsDir(user);

    int p;
    if ((p = dir.Pos("/{"))  >= 0) dir  = dir.Left(p);
    if ((p = rdir.Pos("/{")) >= 0) rdir = rdir.Left(p);

    obj.AddMember("folder", rapidjson::Value(dir.str(), allocator), allocator);
    obj.AddMember("fullfolder", rapidjson::Value(rdir.str(), allocator), allocator);

    struct statvfs fiData;
    if (statvfs(rdir, &fiData) >= 0) {
        double gb = (double)fiData.f_bavail * (double)fiData.f_bsize / ((uint64_t)1024.0 * (uint64_t)1024.0 * (uint64_t)1024.0);

        obj.AddMember("freespace", rapidjson::Value(gb), allocator);
        obj.AddMember("level", rapidjson::Value((int)(gb / lowlimit)), allocator);
    }

    return obj;
}

rapidjson::Value getpattern(rapidjson::Document& doc, const ADVBPatterns::PATTERN& pattern)
{
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
    rapidjson::Value obj, subobj;
    uint_t i;

    obj.SetObject();

    obj.AddMember("user", rapidjson::Value(pattern.user, allocator), allocator);
    obj.AddMember("enabled", rapidjson::Value(pattern.enabled), allocator);
    obj.AddMember("pri", rapidjson::Value(pattern.pri), allocator);
    obj.AddMember("pattern", rapidjson::Value(pattern.pattern, allocator), allocator);
    obj.AddMember("errors", rapidjson::Value(pattern.errors, allocator), allocator);

    subobj.SetArray();

    for (i = 0; i < pattern.list.Count(); i++) {
        const ADVBPatterns::TERMDATA *data = ADVBPatterns::GetTermData(pattern, i);
        rapidjson::Value subobj2;

        subobj2.SetObject();

        subobj2.AddMember("start", rapidjson::Value(data->start), allocator);
        subobj2.AddMember("length", rapidjson::Value(data->length), allocator);
        subobj2.AddMember("field", rapidjson::Value(data->field), allocator);
        subobj2.AddMember("opcode", rapidjson::Value(data->opcode), allocator);
        subobj2.AddMember("opindex", rapidjson::Value(data->opindex), allocator);
        subobj2.AddMember("value", rapidjson::Value(data->value.str(), allocator), allocator);
        subobj2.AddMember("quotes", rapidjson::Value((data->value.Pos(" ") >= 0)), allocator);
        subobj2.AddMember("assign", rapidjson::Value(ADVBPatterns::OperatorIsAssign(pattern, i)), allocator);
        subobj2.AddMember("orflag", rapidjson::Value(data->orflag ? 1 : 0), allocator);

        subobj.PushBack(subobj2, allocator);
    }

    obj.AddMember("terms", subobj, allocator);

    return obj;
}

void addpattern(rapidjson::Document& doc, rapidjson::Value& obj, AHash& patterns, const ADVBProg& prog)
{
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
    AString str;

    if ((str = prog.GetPattern()).Valid()) {
        ADVBPatterns::PATTERN *pattern;

        if (((pattern = (ADVBPatterns::PATTERN *)patterns.Read(str)) == NULL) && ((pattern = new ADVBPatterns::PATTERN) != NULL)) {
            ADVBPatterns::ParsePattern(str, *pattern, prog.GetUser());
        }

        if (pattern) {
            obj.AddMember("patternparsed", getpattern(doc, *pattern), allocator);
        }
    }
}

void addseries(rapidjson::Document& doc, rapidjson::Value& obj, const ADVBProgList::SERIES& serieslist)
{
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
    rapidjson::Value subobj;
    uint_t j;

    subobj.SetArray();

    for (j = 0; j < serieslist.list.size(); j++) {
        rapidjson::Value subobj2;
        const AString& str = serieslist.list[j];

        subobj2.SetObject();

        if (str.Valid()) {
            subobj2.AddMember("state", rapidjson::Value((str.Pos("-") >= 0) ? "incomplete" : "complete", allocator), allocator);
        }
        else subobj2.AddMember("state", "empty", allocator);

        subobj2.AddMember("episodes", rapidjson::Value(str.str(), allocator), allocator);

        subobj.PushBack(subobj2, allocator);
    }

    obj.AddMember("series", subobj, allocator);
}

rapidjson::Value gettrend(rapidjson::Document& doc, const ADVBProgList::TREND& trend)
{
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
    rapidjson::Value obj;

    obj.SetObject();

    obj.AddMember("offset", rapidjson::Value(trend.offset), allocator);
    obj.AddMember("rate", rapidjson::Value(trend.rate), allocator);
    obj.AddMember("timeoffset", rapidjson::Value(trend.timeoffset), allocator);

    return obj;
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

void parsearg(std::map<AString,AString>& vars, const AString& arg, AStdData& log)
{
    uint_t j, n = arg.CountLines("\n", 0);

    for (j = 0; j < n; j++) {
        AString line = arg.Line(j, "\n", 0);
        int p;

        if ((p = line.Pos("=")) > 0) {
            AString var = line.Left(p);
            AString val = line.Mid(p + 1);

            log.printf("%s='%s'\n", var.str(), val.str());

            vars[var] = val;
        }
    }
}

void parsefile(AStdData& fp, std::map<AString,AString>& vars, AStdData& log)
{
    AString line;

    while (line.ReadLn(fp) >= 0) {
        log.printf("stdin = %s\n", line.str());

        parsearg(vars, line, log);
    }
}

int main(int argc, char *argv[])
{
    const ADVBConfig&   config = ADVBConfig::Get();
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
    std::map<AString,AString> vars;
    ADVBProg            prog;   // ensure ADVBProg initialisation takes place
    AStdFile            log(RDDVB_ROOT_DIR "tmp/dvbweb.log", "w");
    AString             val, logdata, errors;
    int  i;

    // ensure output is disabled
    ADVBConfig::GetWriteable().DisableOutput();

    doc.SetObject();

    (void)prog;

    enabledebug(false);

    if (argc == 1) {
        parsefile(*Stdin, vars, log);
    }

    for (i = 1; i < argc; i++) {
        AString arg = argv[i];

        if (arg == "cli") {
            enabledebug(true);
        }
        else if (arg == "-") {
            parsefile(*Stdin, vars, log);
        }
        else if (arg.Pos("=") >= 0) {
            parsearg(vars, arg, log);
        }
        else {
            log.printf("Unrecognised argument '%s'\n", arg.str());
            break;
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

    if (Value(vars, val, "edituser")) {
        AString edit = val.ToLower();
        AString user, newuser;

        Value(vars, user, "user");
        Value(vars, newuser, "newuser");

        if (edit == "add") {

        }
    }

    if (Value(vars, val, "schedule")) {
        bool commit = (val == "commit");

        ADVBProgList::SchedulePatterns(ADateTime().TimeStamp(true), commit);
    }

    std::vector<dvbstream_t> activestreams;
    bool liststreams = (Value(vars, val, "showchannels") && ((uint_t)val > 0));

    if (Value(vars, val, "starthlsstream")) {
        rapidjson::Value subobj;
        dvbstream_t stream;

        subobj.SetArray();

        if (StartDVBStream(stream, StreamType_HLS, val)) {
            activestreams.push_back(stream);

            subobj.PushBack(rapidjson::Value(val.str(), allocator), allocator);

            doc.AddMember("startedstreams", subobj, allocator);
        }
        else
        {
            subobj.PushBack(rapidjson::Value(val.str(), allocator), allocator);

            doc.AddMember("failedstreams", subobj, allocator);
        }

        liststreams = true;
    }

    if (Value(vars, val, "starthttpstream")) {
        rapidjson::Value subobj;
        dvbstream_t stream;

        subobj.SetArray();

        if (StartDVBStream(stream, StreamType_HTTP, val)) {
            activestreams.push_back(stream);

            subobj.PushBack(rapidjson::Value(val.str(), allocator), allocator);

            doc.AddMember("startedstreams", subobj, allocator);
        }
        else
        {
            subobj.PushBack(rapidjson::Value(val.str(), allocator), allocator);

            doc.AddMember("failedstreams", subobj, allocator);
        }

        liststreams = true;
    }

    if (Value(vars, val, "stopstream")) {
        rapidjson::Value subobj;
        std::vector<dvbstream_t> streams;

        subobj.SetArray();

        if (StopDVBStream(val, streams)) {
            for (size_t j = 0; j < streams.size(); j++) {
                subobj.PushBack(rapidjson::Value(streams[j].name.str(), allocator), allocator);
            }

            doc.AddMember("stoppedstreams", subobj, allocator);
        }

        liststreams = true;
    }

    if (Value(vars, val, "stopstreams")) {
        rapidjson::Value subobj;
        std::vector<dvbstream_t> streams;

        subobj.SetArray();

        if (StopDVBStreams(val, streams)) {
            for (size_t j = 0; j < streams.size(); j++) {
                subobj.PushBack(rapidjson::Value(streams[j].name.str(), allocator), allocator);
            }

            doc.AddMember("stoppedstreams", subobj, allocator);
        }

        liststreams = true;
    }

    if (liststreams) {
        rapidjson::Value obj;

        obj.SetArray();

        if (ListDVBStreams(activestreams)) {
            for (size_t j = 0; j < activestreams.size(); j++) {
                rapidjson::Value subobj;
                const auto& stream = activestreams[j];

                subobj.SetObject();
                subobj.AddMember("type", rapidjson::Value(stream.type.str(), allocator), allocator);
                subobj.AddMember("name", rapidjson::Value(stream.name.str(), allocator), allocator);
                subobj.AddMember("url",  rapidjson::Value(stream.url.str(), allocator), allocator);

                obj.PushBack(subobj, allocator);
            }
        }

        doc.AddMember("activestreams", obj, allocator);

        doc.AddMember("showchannels", true, allocator);
    }

    if (Value(vars, val, "parse")) {
        ADVBPatterns::PATTERN pattern;
        AString user;

        Value(vars, user, "user");

        ADVBPatterns::ParsePattern(val, pattern, user);
        AString newpattern = ADVBPatterns::RemoveDuplicateTerms(pattern);

        doc.SetObject();

        ADVBPatterns::ParsePattern(newpattern, pattern, pattern.user);

        doc.AddMember("newpattern", rapidjson::Value(newpattern.str(), allocator), allocator);
        doc.AddMember("parsedpattern", getpattern(doc, pattern), allocator);
    }
    else if (!liststreams) {
        enum {
            DataSource_Progs = 0,
            DataSource_Titles,
            DataSource_Patterns,
            DataSource_Logs,
        };
        ADVBProgList             recordedlist, scheduledlist, requestedlist, rejectedlist, combinedlist, failureslist, recordinglist, processinglist;
        ADVBProgList             list[2], *proglist = NULL;
        AHash                    titleshash;
        ADataList                titleslist;
        ADataList                patternlist;
        ADVBPatterns::PATTERN    filterpattern;
        AHash                    patterns(&ADVBPatterns::__DeletePattern);
        ADVBProgList::SERIESLIST fullseries;
        AString                  from;
        uint_t                   pagesize = (uint_t)config.GetConfigItem("pagesize", "20"), page = 0;
        uint_t                   datasource = DataSource_Progs;
        uint_t                   index = 0;

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
            AString errors;     // use local var to prevent global one being modified
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
        else if (from == "titles (combined)") {
            proglist   = &combinedlist;
            datasource = DataSource_Titles;
        }
        else if (from == "titles (listings)") {
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

            if (Value(vars, val, "sortfields") &&
                val.Valid() &&
                (val != "none")) {
                ADVBProg::FIELDLIST fieldlist;
                ADVBProg::ParseFieldList(fieldlist, val);
                proglist->Sort(fieldlist);
            }

            if (from == "listings") {
                uint_t i;

                // modify programmes from other lists and eliminate those
                // that no longer match
                for (i = 0; i < proglist->Count(); ) {
                    ADVBProg& prog = proglist->GetProgWritable(i);
                    const ADVBProg *prog2;

                    if (((prog2 = processinglist.FindUUID(prog)) != NULL) ||
                        ((prog2 = recordinglist.FindUUID(prog))  != NULL) ||
                        ((prog2 = failureslist.FindUUID(prog))   != NULL) ||
                        ((prog2 = rejectedlist.FindUUID(prog))   != NULL) ||
                        ((prog2 = scheduledlist.FindUUID(prog))  != NULL) ||
                        ((prog2 = recordedlist.FindUUID(prog))   != NULL)) {
                        prog.Modify(*prog2);
                        // check again in case modification means it should be removed
                        if (!ADVBPatterns::Match(prog, filterpattern)) {
                            // delete programme from list
                            proglist->DeleteProg(i);
                        }
                        else i++;
                    }
                    else i++;
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

        {
            uint_t i, nitems, offset, count = pagesize;

            switch (datasource) {
                default:
                case DataSource_Progs:
                    nitems = proglist->Count();
                    break;

                case DataSource_Titles:
                    titleshash.SetDestructor(&deletetitle);
                    titleshash.EnableCaseInSensitive(true);

                    for (i = 0; i < proglist->Count(); i++) {
                        const ADVBProg& prog = proglist->GetProg(i);
                        PROGTITLE *title;

                        if (((title = (PROGTITLE *)titleshash.Read(prog.GetTitle())) == NULL) && ((title = new PROGTITLE) != NULL)) {
                            title->title = prog.GetTitle();
                            title->desc  = prog.GetDesc();

                            memset(&title->counts, 0, sizeof(title->counts));

                            titleshash.Insert(prog.GetTitle(), (uptr_t)title);
                            titleslist.Insert((uptr_t)title, &inserttitle);
                        }

                        if (title) {
                            if (prog.IsRecorded() || (recordedlist.FindSimilar(prog) != NULL))   title->counts.recorded++;
                            if (AStdFile::exists(prog.GetFilename()))                            title->counts.available++;
                            if (prog.IsScheduled() || (scheduledlist.FindUUID(prog) != NULL))    title->counts.scheduled++;
                            if (prog.HasRecordFailed())                                          title->counts.failed++;
                            if (prog.IsFilm())                                                   title->counts.isfilm++;
                            else                                                                 title->counts.notfilm++;
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

            doc.AddMember("total", rapidjson::Value(nitems), allocator);
            doc.AddMember("page", rapidjson::Value(page), allocator);
            doc.AddMember("pagesize", rapidjson::Value(pagesize), allocator);
            doc.AddMember("from", rapidjson::Value(offset), allocator);
            doc.AddMember("for", rapidjson::Value(count), allocator);
            doc.AddMember("parsedpattern", getpattern(doc, filterpattern), allocator);
            doc.AddMember("errors", rapidjson::Value(errors, allocator), allocator);

            if (Value(vars, val, "patterndefs")) {
                doc.AddMember("patterndefs", ADVBPatterns::GetPatternDefinitionJSON(doc), allocator);
            }

            FILE_INFO info;
            if (::GetFileInfo(config.GetSearchesFile(), &info)) {
                if (!Value(vars, val, "searchesref") || ((uint64_t)info.WriteTime > (uint64_t)val)) {
                    AStdFile fp;

                    if (fp.open(config.GetSearchesFile())) {
                        rapidjson::Value subobj;
                        AString line;

                        doc.AddMember("searchesref", rapidjson::Value((uint64_t)info.WriteTime), allocator);

                        subobj.SetArray();

                        while (line.ReadLn(fp) >= 0) {
                            int p;

                            if (IsSymbolChar(line[0]) && ((p = line.Pos("=")) > 0)) {
                                rapidjson::Value subobj2, subobj3;
                                AString title  = line.Left(p).Words(0);
                                AString search = line.Mid(p + 1).Words(0);
                                AString from   = search.Word(0).DeQuotify();
                                AString filter = search.Words(1);

                                subobj2.SetObject();
                                subobj3.SetObject();

                                subobj2.AddMember("title", rapidjson::Value(title.str(), allocator), allocator);

                                if (from.Valid()) subobj3.AddMember("from", rapidjson::Value(from.str(), allocator), allocator);
                                subobj3.AddMember("titlefilter", rapidjson::Value(filter.str(), allocator), allocator);
                                subobj2.AddMember("search", subobj3, allocator);

                                subobj.PushBack(subobj2, allocator);
                            }
                        }

                        doc.AddMember("searches", subobj, allocator);

                        fp.close();
                    }
                }
            }

            if (::GetFileInfo(config.GetDVBChannelsJSONFile(), &info)) {
                if (!Value(vars, val, "channelsref") || ((uint64_t)info.WriteTime > (uint64_t)val)) {
                    rapidjson::Value obj;

                    ADVBChannelList::Get().GenerateChanneList(doc, obj, true);

                    doc.AddMember("channelsref", rapidjson::Value((uint64_t)info.WriteTime), allocator);
                    doc.AddMember("channels", obj, allocator);
                }
            }

            switch (datasource) {
                default:
                case DataSource_Progs: {
                    rapidjson::Value obj;
                    bool testsimilar = ((uint_t)config.GetConfigItem("testsimilarprogrammes", "0") != 0);

                    obj.SetArray();

                    for (i = 0; (i < count) && !HasQuit(); i++) {
                        const ADVBProg& prog = proglist->GetProg(offset + i);
                        const ADVBProg *prog2;
                        rapidjson::Value subobj;

                        prog.ExportToJSON(doc, subobj, true);

                        addpattern(doc, subobj, patterns, prog);

                        if (((prog2 = recordedlist.FindLastSimilar(prog)) != NULL) &&
                            (prog2 != &prog) &&
                            (!testsimilar || ADVBPatterns::Match(*prog2, filterpattern))) {
                            rapidjson::Value subobj2;

                            prog2->ExportToJSON(doc, subobj2, true);

                            addpattern(doc, subobj2, patterns, *prog2);

                            subobj.AddMember("recorded", subobj2, allocator);
                        }

                        if (((prog2 = scheduledlist.FindSimilar(prog)) != NULL) &&
                            (prog2 != &prog) &&
                            (!testsimilar || ADVBPatterns::Match(*prog2, filterpattern))) {
                            rapidjson::Value subobj2;

                            prog2->ExportToJSON(doc, subobj2, true);

                            addpattern(doc, subobj2, patterns, *prog2);

                            subobj.AddMember("scheduled", subobj2, allocator);
                        }

                        ADVBProgList::SERIESLIST::const_iterator it;
                        if ((it = fullseries.find(prog.GetTitle())) != fullseries.end()) {
                            addseries(doc, subobj, it->second);
                        }

                        obj.PushBack(subobj, allocator);
                    }

                    doc.AddMember("progs", obj, allocator);
                    break;
                }

                case DataSource_Titles: {
                    rapidjson::Value obj;

                    obj.SetArray();

                    for (i = 0; i < count; i++) {
                        rapidjson::Value subobj;
                        const PROGTITLE& title = *(const PROGTITLE *)titleslist[i + offset];

                        subobj.SetObject();

                        subobj.AddMember("title", rapidjson::Value(title.title.str(), allocator), allocator);
                        subobj.AddMember("desc", rapidjson::Value(title.desc.str(), allocator), allocator);
                        subobj.AddMember("scheduled", rapidjson::Value(title.counts.scheduled), allocator);
                        subobj.AddMember("recorded", rapidjson::Value(title.counts.recorded), allocator);
                        subobj.AddMember("available", rapidjson::Value(title.counts.available), allocator);
                        subobj.AddMember("failed", rapidjson::Value(title.counts.failed), allocator);
                        subobj.AddMember("isfilm", rapidjson::Value(title.counts.isfilm), allocator);
                        subobj.AddMember("notfilm", rapidjson::Value(title.counts.notfilm), allocator);
                        subobj.AddMember("total", rapidjson::Value(title.counts.total), allocator);

                        ADVBProgList::SERIESLIST::const_iterator it;
                        if ((it = fullseries.find(prog.GetTitle())) != fullseries.end()) {
                            addseries(doc, subobj, it->second);
                        }

                        obj.PushBack(subobj, allocator);
                    }

                    doc.AddMember("titles", obj, allocator);
                    break;
                }

                case DataSource_Patterns: {
                    rapidjson::Value obj;

                    obj.SetArray();

                    for (i = 0; (i < count) && !HasQuit(); i++) {
                        const ADVBPatterns::PATTERN& pattern = *(const ADVBPatterns::PATTERN *)patternlist[offset + i];

                        obj.PushBack(getpattern(doc, pattern), allocator);
                    }

                    doc.AddMember("patterns", obj, allocator);
                    break;
                }

                case DataSource_Logs: {
                    rapidjson::Value obj;

                    obj.SetArray();

                    for (i = 0; (i < count) && !HasQuit(); i++) {
                        obj.PushBack(rapidjson::Value(logdata.Line(offset + i, "\n", 0).SearchAndReplace("\t", " ").StripUnprintable().str(), allocator), allocator);
                    }

                    doc.AddMember("loglines", obj, allocator);
                    break;
                }
            }

            {
                rapidjson::Value obj;
                AList users;

                obj.SetArray();

                obj.PushBack(getuserdetails(doc, ""), allocator);

                config.ListUsers(users);

                const AString *str = AString::Cast(users.First());
                while (str) {
                    obj.PushBack(getuserdetails(doc, *str), allocator);

                    str = str->Next();
                }

                doc.AddMember("users", obj, allocator);
            }

            ADVBProgList::CheckDiskSpace(false, &doc);

            {
                rapidjson::Value obj;

                obj.SetObject();

                obj.AddMember("scheduled", rapidjson::Value(scheduledlist.Count()), allocator);
                obj.AddMember("recorded", rapidjson::Value(recordedlist.Count()), allocator);
                obj.AddMember("requested", rapidjson::Value(requestedlist.Count()), allocator);
                obj.AddMember("rejected", rapidjson::Value(rejectedlist.Count()), allocator);
                obj.AddMember("combined", rapidjson::Value(combinedlist.Count()), allocator);

                doc.AddMember("counts", obj, allocator);
            }

            {
                FILE_INFO info1, info2;
                uint64_t writetime;

                if ((::GetFileInfo(config.GetRecordedFile(), &info1) ||
                     ::GetFileInfo(config.GetCombinedFile(), &info2)) &&
                    ((writetime = std::max((uint64_t)info1.WriteTime, (uint64_t)info2.WriteTime)) > (uint64_t)ADateTime::MinDateTime) &&
                    (!Value(vars, val, "statsref") || (writetime > (uint64_t)val))) {
                    rapidjson::Value obj;
                    ADVBProgList::TREND trend;

                    obj.SetObject();

                    obj.AddMember("ref", rapidjson::Value(writetime), allocator);

                    obj.AddMember("suffix", rapidjson::Value(config.GetGraphSuffix().str(), allocator), allocator);

                    if ((trend = recordedlist.CalculateTrend(ADateTime("utc midnight-4w"))).valid) {
                        obj.AddMember("last4weeks", gettrend(doc, trend), allocator);
                    }
                    if ((trend = recordedlist.CalculateTrend(ADateTime("utc midnight-1w"))).valid) {
                        obj.AddMember("lastweek", gettrend(doc, trend), allocator);
                    }
                    if ((trend = recordedlist.CalculateTrend(ADateTime("utc midnight-6M"))).valid) {
                        obj.AddMember("last6months", gettrend(doc, trend), allocator);
                    }
                    if ((trend = recordedlist.CalculateTrend(ADateTime("utc midnight-1Y"))).valid) {
                        obj.AddMember("lastyear", gettrend(doc, trend), allocator);
                    }
                    if ((trend = combinedlist.CalculateTrend(ADateTime("utc midnight"))).valid) {
                        obj.AddMember("scheduled", gettrend(doc, trend), allocator);
                    }

                    doc.AddMember("stats", obj, allocator);
                }
            }

            {
                rapidjson::Value obj;

                obj.SetObject();

                obj.AddMember("candelete", rapidjson::Value((uint_t)config.GetConfigItem("webcandelete", "0") != 0), allocator);

                doc.AddMember("globals", obj, allocator);
            }
        }
    }

    if (argc == 1) {
        rapidjson::Writer<AStdData> writer(*Stdout);
        doc.Accept(writer);
    }
    else {
        rapidjson::PrettyWriter<AStdData> writer(*Stdout);
        doc.Accept(writer);
    }

    return 0;
}
