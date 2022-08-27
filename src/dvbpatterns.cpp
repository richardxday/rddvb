
#include <inttypes.h>

#include <rdlib/Regex.h>

#include "config.h"
#include "dvblock.h"

#include "dvbpatterns.h"
#include "dvbprog.h"

//#define DEBUG_DATE_PARSING

ADVBPatterns _patterns;

ADVBPatterns::OPERATOR ADVBPatterns::operators[] = {
    {"!=",    false, FieldTypes_Default | FieldTypes_Prog, Operator_NE,            "Is not equal to",                      0},
    {"<>",    false, FieldTypes_Default | FieldTypes_Prog, Operator_NE,            "Is not equal to",                      0},
    {"<=",    false, FieldTypes_Default,                   Operator_LE,            "Is less than or equal to",             0},
    {">=",    false, FieldTypes_Default,                   Operator_GE,            "Is greater than or equal to",          0},
    {"==",    false, FieldTypes_Default | FieldTypes_Prog, Operator_EQ,            "Equals",                               0},
    {"=",     false, FieldTypes_Default | FieldTypes_Prog, Operator_EQ,            "Equals",                               0},
    {"<",     false, FieldTypes_Default,                   Operator_LT,            "Is less than",                         0},
    {">",     false, FieldTypes_Default,                   Operator_GT,            "Is greater than",                      0},

    {"!@>",   false, FieldTypes_String,                    Operator_NotEndsWith,   "Does not end with",                    0},
    {"!@<",   false, FieldTypes_String,                    Operator_NotStartsWith, "Does not start with",                  0},
    {"!@>",   false, FieldTypes_String,                    Operator_NotEndsWith,   "Does not end with",                    0},
    {"!%<",   false, FieldTypes_String,                    Operator_NotStarts,     "Does not start",                       0},
    {"!%>",   false, FieldTypes_String,                    Operator_NotEnds,       "Does not end",                         0},
    {"!=",    false, FieldTypes_String,                    Operator_NE,            "Is not equal to",                      0},
    {"!~",    false, FieldTypes_String,                    Operator_NotRegex,      "Does not match regex",                 0},
    {"!@",    false, FieldTypes_String,                    Operator_NotContains,   "Does not contain",                     0},
    {"!%",    false, FieldTypes_String,                    Operator_NotWithin,     "Is not within",                        0},
    {"<>",    false, FieldTypes_String,                    Operator_NE,            "Is not equal to",                      0},
    {"<=",    false, FieldTypes_String,                    Operator_LE,            "Is less than or equal to",             0},
    {">=",    false, FieldTypes_String,                    Operator_GE,            "Is greater than or equal to",          0},
    {"@<<=",  false, FieldTypes_String,                    Operator_StartsWithLE,  "Starts with less than or equal to",    0},
    {"@<>=",  false, FieldTypes_String,                    Operator_StartsWithGE,  "Starts with greater than or equal to", 0},
    {"@<<",   false, FieldTypes_String,                    Operator_StartsWithLT,  "Starts with less than",                0},
    {"@<>",   false, FieldTypes_String,                    Operator_StartsWithGT,  "Starts with greater than",             0},
    {"@><=",  false, FieldTypes_String,                    Operator_EndsWithLE,    "Ends with less than or equal to",      0},
    {"@>>=",  false, FieldTypes_String,                    Operator_EndsWithGE,    "Ends with greater than or equal to",   0},
    {"@><",   false, FieldTypes_String,                    Operator_EndsWithLT,    "Ends with less than",                  0},
    {"@>>",   false, FieldTypes_String,                    Operator_EndsWithGT,    "Ends with greater than",               0},
    {"==",    false, FieldTypes_String,                    Operator_EQ,            "Equals",                               0},
    {"@<",    false, FieldTypes_String,                    Operator_StartsWith,    "Starts with",                          0},
    {"@>",    false, FieldTypes_String,                    Operator_EndsWith,      "Ends with",                            0},
    {"%<",    false, FieldTypes_String,                    Operator_Starts,        "Starts",                               0},
    {"%>",    false, FieldTypes_String,                    Operator_Ends,          "Ends",                                 0},
    {"=",     false, FieldTypes_String,                    Operator_EQ,            "Equals",                               0},
    {"~",     false, FieldTypes_String,                    Operator_Regex,         "Matches regex",                        0},
    {"@",     false, FieldTypes_String,                    Operator_Contains,      "Contains",                             0},
    {"%",     false, FieldTypes_String,                    Operator_Within,        "Is within",                            0},
    {"<",     false, FieldTypes_String,                    Operator_LT,            "Is less than",                         0},
    {">",     false, FieldTypes_String,                    Operator_GT,            "Is greater than",                      0},

    {"+=",    true,  FieldTypes_String,                    Operator_Concat,        "Is concatenated with",                 0},
    {"-=",    true,  FieldTypes_String,                    Operator_Remove,        "Removal of",                           0},
    {":=",    true,  FieldTypes_String,                    Operator_Assign,        "Is set to",                            0},
    {":=",    true,  FieldTypes_Default,                   Operator_Assign,        "Is set to",                            0},

    {"+=",    true,  FieldTypes_Number,                    Operator_Add,           "Is incremented by",                    0},
    {"-=",    true,  FieldTypes_Number,                    Operator_Subtract,      "Is decremented by",                    0},
    {"*=",    true,  FieldTypes_Number,                    Operator_Multiply,      "Is multiplied by",                     0},
    {"/=",    true,  FieldTypes_Number,                    Operator_Divide,        "Is divided by",                        0},
    {":>",    true,  FieldTypes_Number,                    Operator_Maximum,       "Is maximized with",                    0},
    {":<",    true,  FieldTypes_Number,                    Operator_Minimum,       "Is minimized with",                    0},

    //{"+=",    false, FieldTypes_String,                    Operator_Concat},
};

const ADVBPatterns::PATTERN ADVBPatterns::DefaultPattern = {
    .exclude    = false,
    .enabled    = false,
    .scorebased = false,
    .pos        = 0,
    .pri        = 0,
    .user       = "",
    .pattern    = "",
    .errors     = "",
    .list       = ADataList(),
};

ADVBPatterns::ADVBPatterns()
{
    if (!operators[0].len) {
        uint_t i;

        for (i = 0; i < NUMBEROF(operators); i++) operators[i].len = strlen(operators[i].str);
    }
}

bool ADVBPatterns::UpdatePatternInFile(const AString& filename, const AString& pattern, const AString& newpattern)
{
    const ADVBConfig& config = ADVBConfig::Get();
    AString filename1 = filename + ".new";
    AStdFile ifp, ofp;
    bool changed = false, found = false;

    if (ifp.open(filename)) {
        if (ofp.open(filename1, "w")) {
            AString line;

            while (line.ReadLn(ifp) >= 0) {
                if (line == pattern) {
                    if (newpattern.Valid()) ofp.printf("%s\n", newpattern.str());
                    changed = found = true;

                    if (newpattern.Valid()) {
                        config.logit("Changed pattern '%s' to '%s' in file '%s'", pattern.str(), newpattern.str(), filename.str());
                    }
                    else {
                        config.logit("Deleted pattern '%s' from file '%s'", pattern.str(), filename.str());
                    }
                }
                else if (line.Words(0).Valid()) ofp.printf("%s\n", line.str());
                else changed = true;
            }
            ofp.close();
        }

        ifp.close();

        if (changed) {
            remove(filename);
            rename(filename1, filename);
        }
        else remove(filename1);
    }

    return found;
}

bool ADVBPatterns::AddPatternToFile(const AString& filename, const AString& pattern)
{
    const ADVBConfig& config = ADVBConfig::Get();
    AStdFile fp;
    bool done = false;
    bool added = false;

    if (fp.open(filename)) {
        AString line;

        while (line.ReadLn(fp) >= 0) {
            if (line == pattern) {
                done = true;
                break;
            }
        }

        fp.close();
    }

    if (!done) {
        if (fp.open(filename, "a")) {
            fp.printf("%s\n", pattern.str());
            fp.close();

            config.logit("Add pattern '%s' to file '%s'", pattern.str(), filename.str());

            added = true;
        }
    }

    return added;
}

bool ADVBPatterns::UpdatePattern(const AString& olduser, const AString& oldpattern, const AString& newuser, const AString& newpattern)
{
    ADVBLock lock("patterns");
    bool changed = false;

    if (newuser != olduser) {
        changed |= DeletePattern(olduser, oldpattern);
        if (newpattern.Valid()) {
            changed |= InsertPattern(newuser, newpattern);
        }
    }
    else if (newpattern != oldpattern) changed |= UpdatePattern(newuser, oldpattern, newpattern);

    return changed;
}

bool ADVBPatterns::UpdatePattern(const AString& user, const AString& pattern, const AString& newpattern)
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADVBLock lock("patterns");
    bool changed = false;

    if (user.Empty() || !(changed = UpdatePatternInFile(config.GetUserPatternsPattern().SearchAndReplace("{#?}", user), pattern, newpattern))) {
        changed = UpdatePatternInFile(config.GetPatternsFile(), pattern, newpattern);
    }

    return changed;
}

bool ADVBPatterns::InsertPattern(const AString& user, const AString& pattern)
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADVBLock lock("patterns");
    bool changed = false;

    if (user.Valid()) changed = AddPatternToFile(config.GetUserPatternsPattern().SearchAndReplace("{#?}", user), pattern);
    else              changed = AddPatternToFile(config.GetPatternsFile(), pattern);

    return changed;
}

bool ADVBPatterns::DeletePattern(const AString& user, const AString& pattern)
{
    return UpdatePattern(user, pattern, "");
}

bool ADVBPatterns::EnablePattern(const AString& user, const AString& pattern)
{
    ADVBLock lock("patterns");
    bool changed = false;

    if (pattern[0] == '#') {
        changed = UpdatePattern(user, pattern, pattern.Mid(1));
    }

    return changed;
}

bool ADVBPatterns::DisablePattern(const AString& user, const AString& pattern)
{
    ADVBLock lock("patterns");
    bool changed = false;

    if (pattern[0] != '#') {
        changed = UpdatePattern(user, pattern, "#" + pattern);
    }

    return changed;
}

void ADVBPatterns::GetFieldValue(const FIELD& field, VALUE& value, AString& val)
{
    switch (field.type) {
        case FieldType_string:
            value.str = val.Steal();
            break;

        case FieldType_date:
        case FieldType_external_date:
        case FieldType_span:
        case FieldType_span_single:
        case FieldType_age:
            value.u64 = (uint64_t)ADateTime(val, ADateTime::Time_Absolute);
            break;

        case FieldType_uint32_t:
        case FieldType_external_uint32_t:
            value.u32 = (uint32_t)val;
            break;

        case FieldType_sint32_t:
        case FieldType_external_sint32_t:
            value.s32 = (sint32_t)val;
            break;

        case FieldType_uint64_t:
        case FieldType_external_uint64_t:
            value.u64 = (uint64_t)val;
            break;

        case FieldType_sint64_t:
        case FieldType_external_sint64_t:
            value.s64 = (sint64_t)val;
            break;

        case FieldType_uint16_t:
            value.u16 = (uint16_t)val;
            break;

        case FieldType_sint16_t:
            value.s16 = (sint16_t)val;
            break;

        case FieldType_uint8_t:
            value.u8 = (uint8_t)(uint16_t)val;
            break;

        case FieldType_sint8_t:
            value.s8 = (sint8_t)(sint16_t)val;
            break;

        case FieldType_double:
            value.f64 = (double)val;
            break;

        case FieldType_flag...FieldType_lastflag:
            value.u8 = ((uint32_t)val != 0);
            break;
    }
}

void ADVBPatterns::AssignValue(ADVBProg& prog, const FIELD& field, const VALUE& value, uint8_t termtype)
{
    uint8_t *ptr = prog.GetDataPtr(field.offset);

    switch (field.type) {
        case FieldType_string: {
            AString str;

            if (termtype == Operator_Concat) {
                uint16_t offset;

                memcpy(&offset, ptr, sizeof(offset));
                str = prog.GetString(offset);
            }

            if (termtype == Operator_Remove) {
                uint16_t offset;

                memcpy(&offset, ptr, sizeof(offset));
                str = prog.GetString(offset);
                str = str.SearchAndReplace(value.str, "");
            }
            else str += value.str;

            prog.SetString((const uint16_t *)ptr, str.str());
            break;
        }

        case FieldType_date:
        case FieldType_span:
        case FieldType_span_single:
        case FieldType_age:
            memcpy(ptr, &value.u64, sizeof(value.u64));
            break;

        case FieldType_uint32_t...FieldType_sint8_t: {
            sint64_t val1 = TermTypeToInt64s(ptr, field.type);
            sint64_t val2 = TermTypeToInt64s(&value.u64, field.type);
            sint64_t val;

            switch (termtype) {
                case Operator_Add:
                    val = val1 + val2;
                    break;

                case Operator_Subtract:
                    val = val1 - val2;
                    break;

                case Operator_Multiply:
                    val = val1 * val2;
                    break;

                case Operator_Divide:
                    if (val2 != 0) val = val1 / val2;
                    else           val = val1;
                    break;

                case Operator_Maximum:
                    val = MAX(val1, val2);
                    break;

                case Operator_Minimum:
                    val = MIN(val1, val2);
                    break;

                default:
                    val = val2;
                    break;
            }

            Int64sToTermType(ptr, val, field.type);

            // detect writing to dvbcard
            if (ptr == &prog.data->dvbcard) {
                //debug("DVB card specified as %u\n", (uint_t)data->dvbcard);
                prog.SetDVBCardSpecified();
            }
            break;
        }

        case FieldType_double: {
            double val1 = TermTypeToDouble(ptr, field.type);
            double val2 = TermTypeToDouble(&value.f64, field.type);
            double val;

            switch (termtype) {
                case Operator_Add:
                    val = val1 + val2;
                    break;

                case Operator_Subtract:
                    val = val1 - val2;
                    break;

                case Operator_Multiply:
                    val = val1 * val2;
                    break;

                case Operator_Divide:
                    if (val2 != 0.0) val = val1 / val2;
                    else             val = val1;
                    break;

                case Operator_Maximum:
                    val = MAX(val1, val2);
                    break;

                case Operator_Minimum:
                    val = MIN(val1, val2);
                    break;

                default:
                    val = val2;
                    break;
            }

            DoubleToTermType(ptr, val, field.type);
            break;
        }

        case FieldType_flag...FieldType_lastflag: {
            uint32_t flags, mask = (uint32_t)1U << (field.type - FieldType_flag);

            memcpy(&flags, ptr, sizeof(flags));
            if (value.u8) flags |=  mask;
            else          flags &= ~mask;
            memcpy(ptr, &flags, sizeof(flags));
            break;
        }
    }
}

void ADVBPatterns::__DeletePattern(PATTERN *pattern)
{
    delete pattern;
}

void ADVBPatterns::__DeleteTerm(TERM *term)
{
    if (term->field == NULL) {
        if (term->value.str != NULL) {
            delete[] term->value.str;
        }
    }
    else {
        if (term->field->type == FieldType_string) {
            if (term->value.str != NULL) {
                delete[] term->value.str;
            }
        }
        else if (term->field->type == FieldType_prog) {
            if (term->value.prog != NULL) {
                delete term->value.prog;
            }
        }
    }

    if (term->pattern != NULL) {
        __DeletePattern(term->pattern);
    }

    delete term;
}

const ADVBPatterns::OPERATOR *ADVBPatterns::FindOperator(const PATTERN& pattern, uint_t term)
{
    const TERM *pterm = (const TERM *)pattern.list[term];

    if (pterm && pterm->field) {
        uint_t  fieldtype = 1U << pterm->field->type;
        uint8_t oper      = pterm->data.opcode;
        uint_t  i;

        for (i = 0; ((i < NUMBEROF(operators)) &&
                     !((oper == operators[i].opcode) &&
                       (operators[i].fieldtypes & fieldtype))); i++) ;

        return (i < NUMBEROF(operators)) ? operators + i : NULL;
    }

    return NULL;
}

const char *ADVBPatterns::GetOperatorText(const PATTERN& pattern, uint_t term)
{
    const OPERATOR *oper = FindOperator(pattern, term);

    return oper ? oper->str : NULL;
}

const char *ADVBPatterns::GetOperatorDescription(const PATTERN& pattern, uint_t term)
{
    const OPERATOR *oper = FindOperator(pattern, term);

    return oper ? oper->desc : NULL;
}

bool ADVBPatterns::OperatorIsAssign(const PATTERN& pattern, uint_t term)
{
    const TERM *pterm = (const TERM *)pattern.list[term];

    return (pterm && pterm->field) ? RANGE(pterm->data.opcode, Operator_First_Assignable, Operator_Last_Assignable) : false;
}

rapidjson::Value ADVBPatterns::GetPatternDefinitionJSON(rapidjson::Document& doc)
{
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
    rapidjson::Value obj, subobj;
    AString str;
    uint_t i, j, nfields;
    const FIELD *fields = ADVBProg::GetFields(nfields);

    obj.SetObject();

    {
        rapidjson::Value subobj;

        subobj.SetArray();

        for (i = 0; i < nfields; i++) {
            rapidjson::Value subobj2, subobj3;
            const FIELD& field = fields[i];

            subobj2.SetObject();

            subobj2.AddMember("name", rapidjson::Value(field.name, allocator), allocator);
            subobj2.AddMember("desc", rapidjson::Value(field.desc, allocator), allocator);
            subobj2.AddMember("type", rapidjson::Value(field.type), allocator);
            subobj2.AddMember("assignable", rapidjson::Value(field.assignable), allocator);

            subobj3.SetArray();

            for (j = 0; j < NUMBEROF(operators); j++) {
                const OPERATOR& oper = operators[j];

                if ((field.assignable == oper.assign) &&
                    (oper.fieldtypes & (1U << field.type))) {
                    subobj3.PushBack(rapidjson::Value(j), allocator);
                }
            }

            subobj2.AddMember("operators", subobj3, allocator);

            subobj.PushBack(subobj2, allocator);
        }

        obj.AddMember("fields", subobj, allocator);
    }

    {
        rapidjson::Value subobj;

        subobj.SetObject();

        for (i = 0; i < nfields; i++) {
            const FIELD& field = fields[i];

            subobj.AddMember(rapidjson::Value(field.name, allocator), rapidjson::Value(i), allocator);
        }

        obj.AddMember("fieldnames", subobj, allocator);
    }

    {
        rapidjson::Value subobj;

        subobj.SetArray();

        for (j = 0; j < NUMBEROF(operators); j++) {
            rapidjson::Value subobj2;
            const OPERATOR& oper = operators[j];

            subobj2.SetObject();

            subobj2.AddMember("text", rapidjson::Value(oper.str, allocator), allocator);
            subobj2.AddMember("desc", rapidjson::Value(oper.desc, allocator), allocator);
            subobj2.AddMember("opcode", rapidjson::Value(oper.opcode), allocator);
            subobj2.AddMember("assign", rapidjson::Value(oper.assign), allocator);

            subobj.PushBack(subobj2, allocator);
        }

        obj.AddMember("operators", subobj, allocator);
    }

    {
        rapidjson::Value subobj;

        subobj.SetArray();

        for (j = 0; j < 2; j++) {
            rapidjson::Value subobj2;

            subobj2.SetObject();

            subobj2.AddMember("text", rapidjson::Value(j ? "or" : "and", allocator), allocator);
            subobj2.AddMember("desc", rapidjson::Value(j ? "Or the next term" : "And the next term", allocator), allocator);
            subobj2.AddMember("value", rapidjson::Value(j), allocator);

            subobj.PushBack(subobj2, allocator);
        }

        obj.AddMember("orflags", subobj, allocator);
    }

    return obj;
}

int ADVBPatterns::SortTermsByAssign(uptr_t item1, uptr_t item2, void *context)
{
    const TERM *term1 = (const TERM *)item1;
    const TERM *term2 = (const TERM *)item2;
    bool assign1 = (term1->field && term1->field->assignable);
    bool assign2 = (term2->field && term2->field->assignable);
    int  res = 0;

    UNUSED(context);

    if      ( assign1 && !assign2) res = -1;
    else if (!assign1 &&  assign2) res =  1;
    else if ( assign1 &&  assign2) res = COMPARE_ITEMS(term1->data.field, term2->data.field);

    return res;
}

uint_t ADVBPatterns::ParsePatterns(ADataList& patternlist, const AString& patterns, AString& errors, const AString& sep, const AString& user)
{
    uint_t i, n = patterns.CountLines(sep);

    for (i = 0; i < n; i++) {
        ParsePattern(patternlist, patterns.Line(i, sep), errors, user);
    }

    return patternlist.Count();
}

bool ADVBPatterns::ParsePattern(ADataList& patternlist, const AString& line, AString& errors, const AString& user)
{
    //const ADVBConfig& config = ADVBConfig::Get();
    PATTERN *pattern;
    bool    success = false;

    patternlist.SetDestructor(&__DeletePattern);

    if ((pattern = new PATTERN) != NULL) {
        AString errs = ParsePattern(line, *pattern, user);

        if (errs.Valid()) {
            //config.printf("Error parsing '%s': %s", line.str(), errs.str());
            errors += errs + "\n";
        }

        if ((pattern->list.Count() > 0) || pattern->errors.Valid()) {
            patternlist.Add((uptr_t)pattern);
            success = true;
        }
    }

    return success;
}

uint_t ADVBPatterns::Skip(const AString& line, uint_t i, char terminator)
{
    while (line[i] && (line[i] != terminator)) {
        char newterminator = 0;

        if      (IsQuoteChar(terminator) && (line[i] == '\\') && line[i + 1]) i += 2;
        else if (IsQuoteChar(line[i])) newterminator = line[i];
        else if (line[i] == '(')       newterminator = ')';
        else if (line[i] == '[')       newterminator = ']';
        else if (line[i] == '{')       newterminator = '}';

        if (newterminator) {
            i = Skip(line, i + 1, newterminator);
            if (line[i] == newterminator) i++;
            else break;
        }
        else i++;
    }

    return i;
}

uint_t ADVBPatterns::CheckOrStatement(const AString& line, uint_t i, bool& orflag)
{
    orflag = false;

    if ((line.Mid(i, 2).ToLower() == "and") && IsWhiteSpace(line[i + 3])) {
        i += 3;

        while (IsWhiteSpace(line[i])) i++;
    }
    else if ((line.Mid(i, 2).ToLower() == "or") && IsWhiteSpace(line[i + 2])) {
        orflag = true;
        i += 2;

        while (IsWhiteSpace(line[i])) i++;
    }
    else if ((line[i] == '&') && IsWhiteSpace(line[i + 1])) {
        i += 1;

        while (IsWhiteSpace(line[i])) i++;
    }
    else if ((line[i] == '|') && IsWhiteSpace(line[i + 1])) {
        orflag = true;
        i += 1;

        while (IsWhiteSpace(line[i])) i++;
    }

    return i;
}

AString ADVBPatterns::ParsePattern(const AString& _line, PATTERN& pattern, const AString& user)
{
    const ADVBConfig& config = ADVBConfig::Get();
    ADataList&        list   = pattern.list;
    AString&          errors = pattern.errors;
    AString           line   = config.ReplaceTerms(user, _line, "");
    TERM              *term;
    uint_t            i;

    pattern.exclude    = false;
    pattern.enabled    = true;
    pattern.scorebased = false;
    pattern.pos        = 0;
    pattern.pri        = 0;
    pattern.user       = user;
    pattern.pattern    = line;

    if (pattern.user.Valid()) {
        pattern.pri = (int)config.GetUserConfigItem(pattern.user, "pri");
    }

    list.DeleteList();
    list.SetDestructor(&__DeleteTerm);

    i = 0;
    while (IsWhiteSpace(line[i])) i++;
    if (line[i] == '#') {
        pattern.enabled = false;
        i++;
    }
    else if (line[i] == ';') {
        return errors;
    }

    while (IsWhiteSpace(line[i])) i++;

    if (line[i]) {
        while (line[i] && errors.Empty()) {
            while (IsWhiteSpace(line[i])) i++;

            if (line[i] == '(') {
                i++;

                while (IsWhiteSpace(line[i])) i++;

                uint_t i0 = i;
                i = Skip(line, i, ')');
                uint_t i1 = i;

                if (line[i] == ')') {
                    AString subline = line.Mid(i0, i1 - i0).Words(0);
                    PATTERN *subpattern;
                    bool    orflag;

                    i++;
                    while (IsWhiteSpace(line[i])) i++;

                    i = CheckOrStatement(line, i, orflag);

                    if ((subpattern = new PATTERN) != NULL) {
                        AString suberrors = ParsePattern(subline, *subpattern, user);

                        if (suberrors.Empty()) {
                            TERM *term;

                            subpattern->pos = pattern.pos + i0;

                            if ((term = new TERM) != NULL) {
                                term->data.start   = i0;
                                term->data.length  = i1 - i0;
                                term->data.field   = 0;
                                term->data.opcode  = 0;
                                term->data.opindex = 0;
                                term->data.orflag  = orflag;
                                term->field        = NULL;
                                term->datetype     = DateType_none;
                                term->value.str    = subline.Steal();
                                term->pattern      = subpattern;

                                list.Add((uptr_t)term);
                            }
                            else {
                                if (errors.Valid()) errors += "\n";
                                errors.printf("Failed to allocate term for sub-pattern (term %u)", list.Count() + 1);
                                break;
                            }
                        }
                        else {
                            if (errors.Valid()) errors += "\n";
                            errors += suberrors;
                            break;
                        }
                    }
                    else {
                        if (errors.Valid()) errors += "\n";
                        errors.printf("Failed to allocate sub-pattern (term %u)", list.Count() + 1);
                        break;
                    }
                }
                else {
                    if (errors.Valid()) errors += "\n";
                    errors.printf("Unterminated sub-pattern (at %u) (term %u)", i, list.Count() + 1);
                    break;
                }
            }
            else {
                char quote = 0;

                if (IsQuoteChar(line[i])) quote = line[i++];

                if (!IsSymbolStart(line[i])) {
                    if (errors.Valid()) errors += "\n";
                    errors.printf("Character '%c' (at %u) is not a legal field start character (term %u)", line[i], i, list.Count() + 1);
                    break;
                }

                uint_t fieldstart = i++;
                if (quote) {
                    while (line[i] && (line[i] != quote)) {
                        if (line[i] == '\\') i++;
                        if (line[i]) i++;
                    }
                }
                else {
                    while (IsSymbolChar(line[i])) i++;
                }

                AString field = line.Mid(fieldstart, i - fieldstart).ToLower();

                if (quote && (line[i] == quote)) i++;

                while (IsWhiteSpace(line[i])) i++;

                if (field == "exclude") {
                    pattern.exclude = true;
                    continue;
                }

                const FIELD *fieldptr = (const FIELD *)ADVBProg::fieldhash.Read(field);
                if (!fieldptr) {
                    uint_t nfields;
                    const FIELD *fields = ADVBProg::GetFields(nfields);

                    errors.printf("'%s' (at %u) is not a valid search field (term %u), valid search fields are: ", field.str(), fieldstart, list.Count() + 1);
                    for (i = 0; i < nfields; i++) {
                        const FIELD& field = fields[i];

                        if (i) errors.printf(", ");
                        errors.printf("'%s'", field.name);
                    }
                    break;
                }

                uint_t opstart = i;

                const char *str = line.str() + i;
                bool isassign = fieldptr->assignable;
                uint_t j;
                uint_t opindex = 0, opcode = Operator_EQ;
                for (j = 0; j < NUMBEROF(operators); j++) {
                    if (((isassign == operators[j].assign) ||
                         (isassign && !operators[j].assign)) &&
                        (((fieldptr->type <  FieldType_flag) && (operators[j].fieldtypes & (1U << fieldptr->type))) ||
                         ((fieldptr->type >= FieldType_flag) && (operators[j].fieldtypes & (1U << FieldType_uint8_t)))) &&
                        (strncmp(str, operators[j].str, operators[j].len) == 0)) {
                        i      += operators[j].len;
                        opindex = j;
                        opcode  = operators[j].opcode;
                        break;
                    }
                }

                while (IsWhiteSpace(line[i])) i++;

                AString value;
                bool    implicitvalue = false;
                if (j == NUMBEROF(operators)) {
                    if (!line[i] || IsSymbolStart(line[i])) {
                        if (fieldptr->assignable) {
                            switch (fieldptr->type) {
                                case FieldType_string:
                                    break;

                                case FieldType_date:
                                    value = "now";
                                    break;

                                default:
                                    value = "1";
                                    break;
                            }

                            opcode = Operator_Assign;
                        }
                        else opcode = Operator_NE;

                        for (j = 0; j < NUMBEROF(operators); j++) {
                            if ((opcode == operators[j].opcode) &&
                                ((isassign == operators[j].assign) ||
                                 (isassign && !operators[j].assign)) &&
                                (operators[j].fieldtypes & (1U << fieldptr->type))) {
                                opindex = j;
                                break;
                            }
                        }

                        implicitvalue = true;
                    }
                    else {
                        errors.printf("Symbols at %u do not represent a legal operator (term %u), legal operators for the field '%s' are: ", opstart, list.Count() + 1, field.str());

                        bool flag = false;
                        for (j = 0; j < NUMBEROF(operators); j++) {
                            if (((isassign == operators[j].assign) ||
                                 (isassign && !operators[j].assign)) &&
                                (operators[j].fieldtypes & (1U << fieldptr->type))) {
                                if (flag) errors.printf(", ");
                                errors.printf("'%s'", operators[j].str);
                                flag = true;
                            }
                        }
                        break;
                    }
                }

                if (!implicitvalue) {
                    char quote = 0;
                    if (IsQuoteChar(line[i])) quote = line[i++];

                    uint_t valuestart = i;
                    while (line[i] && ((!quote && !IsWhiteSpace(line[i])) || (quote && (line[i] != quote)))) {
                        if (line[i] == '\\') i++;
                        i++;
                    }

                    value = line.Mid(valuestart, i - valuestart).DeEscapify();

                    if (quote && (line[i] == quote)) i++;

                    while (IsWhiteSpace(line[i])) i++;
                }

                ADVBProg::ModifySearchValue(fieldptr, value);

                bool orflag;
                i = CheckOrStatement(line, i, orflag);

                if ((term = new TERM) != NULL) {
                    term->data.start   = fieldstart;
                    term->data.length  = i - fieldstart;
                    term->data.field   = fieldptr - ADVBProg::fields;
                    term->data.opcode  = opcode;
                    term->data.opindex = opindex;
                    term->data.value   = value;
                    term->data.orflag  = (orflag && !RANGE(opcode, Operator_First_Assignable, Operator_Last_Assignable));
                    term->field        = fieldptr;
                    term->datetype     = DateType_none;
                    term->pattern      = NULL;

                    switch (term->field->type) {
                        case FieldType_string:
                            if ((opcode & ~Operator_Inverted) == Operator_Regex) {
                                AString regexerrors;
                                AString rvalue;

                                rvalue = ParseRegex(value, regexerrors);
                                if (regexerrors.Valid()) {
                                    errors.printf("Regex error in value '%s' (term %u): %s", value.str(), list.Count() + 1, regexerrors.str());
                                }

                                value = rvalue;
                            }
                            term->value.str = value.Steal();
                            break;

                        case FieldType_external_date:
                        case FieldType_date: {
                            ADateTime dt;
                            uint_t specified;

                            dt.StrToDate(value, ADateTime::Time_Relative_Local, &specified);

#ifdef DEBUG_DATE_PARSING
                            debug("Value '%s', specified %u\n", value.str(), specified);
#endif

                            if (!specified) {
                                errors.printf("Failed to parse date '%s' (term %u)", value.str(), list.Count() + 1);
                                break;
                            }
                            else if (((specified == ADateTime::Specified_Day) && (stricmp(term->field->name, "on") == 0)) ||
                                     (stricmp(term->field->name, "day") == 0)) {
#ifdef DEBUG_DATE_PARSING
                                debug("Date from '%s' is '%s' (week day only)\n", value.str(), dt.DateToStr().str());
#endif
                                term->value.u64 = dt.GetWeekDay();
                                term->datetype  = DateType_weekday;
                            }
                            else if (specified & ADateTime::Specified_Day) {
                                specified |= ADateTime::Specified_Date;
                            }

                            if (term->datetype == DateType_none) {
                                specified &= ADateTime::Specified_Date | ADateTime::Specified_Time;

                                if (specified == (ADateTime::Specified_Date | ADateTime::Specified_Time)) {
                                    term->value.u64 = (uint64_t)dt;
                                    term->datetype  = DateType_fulldate;
#ifdef DEBUG_DATE_PARSING
                                    debug("Date from '%s' is '%s' (full date and time), value %" PRIu64 "\n", value.str(), dt.DateToStr().str(), term->value.u64);
#endif
                                }
                                else if (specified == ADateTime::Specified_Date) {
                                    term->value.u64 = dt.GetDays();
                                    term->datetype  = DateType_date;
#ifdef DEBUG_DATE_PARSING
                                    debug("Date from '%s' is '%s' (date only), value %" PRIu64 "\n", value.str(), dt.DateToStr().str(), term->value.u64);
#endif
                                }
                                else if (specified == ADateTime::Specified_Time) {
                                    term->value.u64 = dt.GetMS();
                                    term->datetype  = DateType_time;
#ifdef DEBUG_DATE_PARSING
                                    debug("Date from '%s' is '%s' (time only), value %" PRIu64 "\n", value.str(), dt.DateToStr().str(), term->value.u64);
#endif
                                }
                                else {
                                    errors.printf("Unknown date specifier '%s' (term %u)", value.str(), list.Count() + 1);
                                }
                            }
                            break;
                        }

                        case FieldType_span:
                        case FieldType_span_single:
                        case FieldType_age: {
                            ADateTime dt;
                            //ADateTime::EnableDebugStrToDate(true);
                            term->value.u64 = (uint64_t)ADateTime(value, ADateTime::Time_Absolute);
                            //ADateTime::EnableDebugStrToDate(false);
                            break;
                        }

                        case FieldType_uint32_t:
                        case FieldType_external_uint32_t:
                            term->value.u32 = (uint32_t)value;
                            break;

                        case FieldType_sint32_t:
                        case FieldType_external_sint32_t:
                            term->value.s32 = (sint32_t)value;
                            break;

                        case FieldType_uint64_t:
                        case FieldType_external_uint64_t:
                            term->value.u64 = (uint64_t)value;
                            break;

                        case FieldType_sint64_t:
                        case FieldType_external_sint64_t:
                            term->value.s64 = (sint64_t)value;
                            break;

                        case FieldType_uint16_t:
                            term->value.u16 = (uint16_t)value;
                            break;

                        case FieldType_sint16_t:
                            term->value.s16 = (sint16_t)value;
                            break;

                        case FieldType_uint8_t:
                            term->value.u8 = (uint8_t)(uint16_t)value;
                            break;

                        case FieldType_sint8_t:
                            term->value.s8 = (sint8_t)(sint16_t)value;
                            break;

                        case FieldType_double:
                        case FieldType_external_double:
                            term->value.f64 = (double)value;
                            break;

                        case FieldType_flag...FieldType_lastflag:
                            term->value.u8 = ((uint32_t)value != 0);
                            //debug("Setting test of flag to %u\n", (uint_t)term->value.u8);
                            break;

                        case FieldType_prog: {
                            ADVBProg *prog;

                            if ((prog = new ADVBProg) != NULL) {
                                if (prog->Base64Decode(value)) {
                                    term->value.prog = prog;
                                }
                                else {
                                    errors.printf("Failed to decode base64 programme ('%s') for %s at %u (term %u)", value.str(), field.str(), fieldstart, list.Count() + 1);
                                    delete prog;
                                }
                            }
                            else {
                                errors.printf("Failed to allocate memory for base64 programme ('%s') for %s at %u (term %u)", value.str(), field.str(), fieldstart, list.Count() + 1);
                            }
                            break;
                        }

                        default:
                            errors.printf("Unknown field type (%u) (term %u for '%s')", (uint_t)term->field->type, list.Count() + 1, field.str());
                            break;
                    }

                    //debug("term: field {name '%s', type %u, assignable %u, offset %u} type %u dateflags %u value '%s'\n", term->field->name, (uint_t)term->field->type, (uint_t)term->field->assignable, term->field->offset, (uint_t)term->data.opcode, (uint_t)term->dateflags, term->value.str);

                    if (errors.Empty()) {
                        pattern.scorebased |= (term->field->offset == ADVBProg::GetScoreDataOffset());
                        list.Add((uptr_t)term);
                    }
                    else {
                        __DeleteTerm((uptr_t)term, NULL);
                        break;
                    }
                }
            }
        }
    }
    else errors.printf("No filter terms specified!");

    if (errors.Valid()) {
        AString puretext, conditions;
        int p = line.len();

        for (i = 0; i < NUMBEROF(operators); i++) {
            int p1;

            if ((p1 = line.PosNoCase(operators[i].str)) >= 0) {
                p = MIN(p, p1);
            }
        }

        if (p < line.len()) {
            // move back over whitespace
            while ((p > 0) && IsWhiteSpace(line[p - 1])) p--;
            // move back over non-whitespace
            while ((p > 0) && !IsWhiteSpace(line[p - 1])) p--;
            // move back over whitespace again
            while ((p > 0) && IsWhiteSpace(line[p - 1])) p--;

            puretext   = line.Left(p);
            conditions = line.Mid(p);
        }
        else puretext  = line;

        if (puretext.Valid()) {
            for (i = 0; puretext[i] && (IsAlphaChar(puretext[i]) || IsNumeralChar(puretext[i]) || IsQuoteChar(puretext[i]) || (puretext[i] == ' ') || (puretext[i] == '-')); i++) ;

            if (i == (uint_t)puretext.len()) {
                AString newtext = "@\"" + puretext.DeQuotify() + "\"";
                AString newline = ("title" + newtext +
                                   " or subtitle" + newtext +
                                   " or desc" + newtext +
                                   " or actor" + newtext +
                                   " or director" + newtext +
                                   " or category" + newtext +
#if DVBDATVERSION > 1
                                   " or subcategory" + newtext +
#endif
                                   conditions);

                //debug("Split '%s' into:\npure text:'%s'\nconditions:'%s'\nnew pattern:'%s'\n", line.str(), puretext.str(), conditions.str(), newline.str());

                pattern.errors.Delete();
                errors = ParsePattern(newline, pattern, user);
            }
        }
    }

    if (errors.Valid()) pattern.enabled = false;

    // config.printf("--------------------------------------------------------------------------------\n");
    // config.printf("Pattern before sort:\n%s\n", ToString(pattern).str());
    // config.printf("--------------------------------------------------------------------------------\n");

    if (errors.Empty()) {
        list.Sort(&SortTermsByAssign);

        for (i = 0; i < list.Count(); i++) {
            const TERM *term = (const TERM *)list[i];

            if ((term->data.opcode == Operator_Assign) && (term->field->offset == ADVBProg::GetUserDataOffset())) {
                if (pattern.user.Empty()) {
                    pattern.user = term->value.str;
                    if (pattern.user.Valid()) {
                        pattern.pri = (int)config.GetUserConfigItem(pattern.user, "pri");
                    }
                }
                else {
                    errors.printf("Cannot specify user when user has already been specified (term %u)", i + 1);
                }
            }
        }
    }

    // config.printf("--------------------------------------------------------------------------------\n");
    // config.printf("Pattern after sort:\n%s\n", ToString(pattern).str());
    // config.printf("--------------------------------------------------------------------------------\n");

    if (errors.Empty()) {
        for (i = 0; i < list.Count(); i++) {
            const TERM *term = (const TERM *)list[i];

            if ((term->data.opcode == Operator_Assign) && (term->field->offset == ADVBProg::GetPriDataOffset())) {
                pattern.pri = term->value.s8;
            }
        }
    }

    return errors;
}

void ADVBPatterns::AppendTerms(PATTERN& dstpattern, const PATTERN& srcpattern, bool excludeduplicatefields)
{
    const ADataList& srcterms = srcpattern.list;
    ADataList& dstterms = dstpattern.list;
    uint_t i, j = 0;

    for (i = 0; i < srcterms.Count(); i++) {
        const TERM *srcterm = (const TERM *)srcterms[i];

        if (excludeduplicatefields) {
            for (j = 0; j < dstterms.Count(); j++) {
                const TERM *dstterm = (const TERM *)dstterms[j];

                if (srcterm->field == dstterm->field) break;
            }
        }

        if (!excludeduplicatefields || (j == dstterms.Count())) {
            TERM *dstterm;

            if ((dstterm = DuplicateTerm(srcterm)) != NULL) {
                dstterms.Add((uptr_t)dstterm);
            }
        }
    }
}

sint64_t ADVBPatterns::TermTypeToInt64s(const void *p, uint_t termtype)
{
    sint64_t val = 0;

    if (RANGE(termtype, FieldType_uint32_t, FieldType_sint8_t)) {
        uint_t type     = FieldType_sint8_t - termtype;
        // sint8_t/uint8_t   -> 0 -> 1
        // sint16_t/uint16_t -> 1 -> 2
        // sint32_t/uint32_t -> 2 -> 4
        uint_t bytes    = 1U << (type >> 1);
        bool   issigned = ((type & 1) == 0);

        // first, get data from pointer
        if (MachineIsBigEndian()) {
            // big endian -> copy to LAST x bytes
            memcpy((uint8_t *)&val + (sizeof(val) - bytes), p, bytes);
        }
        else {
            // little endian -> copy to FIRST x bytes
            memcpy((uint8_t *)&val, p, bytes);
        }

        // for signed types, sign extend the number
        if (issigned) {
            uint_t shift = (sizeof(val) - bytes) << 3;

            // perform UNSIGNED shift up so that sign bit is in the top bit
            // then perform SIGNED shift down to propagate the sign bit down
            val = (sint64_t)((uint64_t)val << shift) >> shift;
        }
    }

    return val;
}

void ADVBPatterns::Int64sToTermType(void *p, sint64_t val, uint_t termtype)
{
    if (RANGE(termtype, FieldType_uint32_t, FieldType_sint8_t)) {
        sint64_t minval, maxval;
        uint_t   type     = FieldType_sint8_t - termtype;
        // sint8_t/uint8_t   -> 0 -> 1
        // sint16_t/uint16_t -> 1 -> 2
        // sint32_t/uint32_t -> 2 -> 4
        uint_t   bytes    = 1U << (type >> 1);
        bool     issigned = ((type & 1) == 0);

        if (issigned) {
            maxval = (sint64_t)(((uint64_t)1 << ((bytes << 3) - 1)) - 1);
            minval = -maxval - 1;
        }
        else {
            minval = 0;
            maxval = (sint64_t)(((uint64_t)1 << (bytes << 3)) - 1);
        }

        // limit value to correct range for type
        val = LIMIT(val, minval, maxval);

        if (MachineIsBigEndian()) {
            // big endian -> copy from LAST x bytes
            memcpy(p, (uint8_t *)&val + (sizeof(val) - bytes), bytes);
        }
        else {
            // little endian -> copy from FIRST x bytes
            memcpy(p, (uint8_t *)&val, bytes);
        }
    }
}

double ADVBPatterns::TermTypeToDouble(const void *p, uint_t termtype)
{
    double val = 0.0;

    if (termtype == FieldType_double) {
        memcpy(&val, p, sizeof(val));
    }

    return val;
}

void ADVBPatterns::DoubleToTermType(void *p, double val, uint_t termtype)
{
    if (termtype == FieldType_double) {
        memcpy(p, &val, sizeof(val));
    }
}

AString ADVBPatterns::RemoveDuplicateTerms(PATTERN& pattern)
{
    AString newpattern;
    uint_t  i, j;

    for (i = 0; i < pattern.list.Count();) {
        for (j = i + 1; j < pattern.list.Count(); j++) {
            const TERM&     term1 = *(const TERM *)pattern.list[i];
            const TERM&     term2 = *(const TERM *)pattern.list[j];
            const TERMDATA& data1 = term1.data;
            const TERMDATA& data2 = term2.data;

            if ((data1.field  == data2.field) &&
                (data1.opcode == data2.opcode) &&
                (data2.opcode == Operator_Assign)) break;
        }

        if (j < pattern.list.Count()) {
            delete (TERM *)pattern.list[i];
            pattern.list.RemoveIndex(i);
        }
        else i++;
    }

    for (i = 0; i < pattern.list.Count(); i++) {
        const TERM&     term = *(const TERM *)pattern.list[i];
        const TERMDATA& data = term.data;

        if (newpattern.Valid()) newpattern.printf(" ");
        newpattern.printf("%s%s%s%s%s",
                          term.field->name,
                          operators[data.opindex].str,
                          (data.value.Pos(" ") >= 0) ? "\"" : "",
                          data.value.str(),
                          (data.value.Pos(" ") >= 0) ? "\"" : "");
    }

    return newpattern;
}

bool ADVBPatterns::MatchString(const TERM& term, const char *str, bool ignoreinvert)
{
    uint_t opcode = term.data.opcode & ~Operator_Inverted;
    bool   match  = false;

    switch (opcode) {
        case Operator_Regex:
            match = (IsRegexAnyPattern(term.value.str) || MatchRegex(str, term.value.str));
            break;

        case Operator_Contains:
            match = (stristr(str, term.value.str) != NULL);
            break;

        case Operator_StartsWith:
            match = (stristr(str, term.value.str) == str);
            break;

        case Operator_EndsWith:
            match = (stristr(str, term.value.str) == (str + strlen(str) - strlen(term.value.str)));
            break;

        case Operator_Within:
            match = (stristr(term.value.str, str) != NULL);
            break;

        case Operator_Starts:
            match = (stristr(term.value.str, str) == term.value.str);
            break;

        case Operator_Ends:
            match = (stristr(term.value.str, str) == (term.value.str + strlen(term.value.str) - strlen(str)));
            break;

        default: {
            const char *s1 = str;
            const char *s2 = term.value.str;

            if ((opcode >= Operator_StartsWithLT) && (opcode <= Operator_StartsWithGT)) {
                opcode -= Operator_StartsWithLT;
                opcode += Operator_LT;
            }
            else if ((opcode >= Operator_EndsWithLT) && (opcode <= Operator_EndsWithGT)) {
                uint_t l1 = strlen(s1);
                uint_t l2 = strlen(s2);

                s1 += l1 - std::min(l1, l2);
                opcode -= Operator_EndsWithLT;
                opcode += Operator_LT;
            }

            int res = CompareNoCase(s1, s2);

            //debug("Comparing '%s' and '%s': %d\n", str, term.value.str, res);

            switch (opcode) {
                case Operator_EQ:
                    match = (res == 0);
                    break;
                case Operator_GT:
                    match = (res > 0);
                    break;
                case Operator_LT:
                    match = (res < 0);
                    break;
            }
            break;
        }
    }

    return ((term.data.opcode & Operator_Inverted) && !ignoreinvert) ? !match : match;
}

int ADVBPatterns::CompareDates(uint64_t val, const TERM& term)
{
    ADateTime dt = ADateTime(val).UTCToLocal();

    val = (uint64_t)dt;

    switch (term.datetype) {
        case DateType_time:
            val %= 24UL * 3600UL * 1000UL;
            break;

        case DateType_date:
            val /= 24UL * 3600UL * 1000UL;
            break;

        case DateType_weekday:
            val = dt.GetWeekDay();
            break;

        default:
            break;
    }

#if 0
    debug("Date: comparing %s with %s (%s with %s)\n",
          AValue(val).ToString().str(),
          AValue(term.value.u64).ToString().str(),
          ADateTime(val).DateToStr().str(),
          ADateTime(term.value.u64).DateToStr().str());
#endif

    return COMPARE_ITEMS(val, term.value.u64);
}

bool ADVBPatterns::Match(const ADVBProg& prog, const PATTERN& pattern)
{
    const ADataList& list = pattern.list;
    uint_t i, n = list.Count();
    bool match = (n > 0);
    bool orflag = false;

    for (i = 0; (i < n) && (match || orflag); i++) {
        const TERM&  term  = *(const TERM *)list[i];
        const FIELD& field = *term.field;

        if (term.pattern) {
            bool newmatch = Match(prog, *term.pattern);

            if (orflag) match |= newmatch;
            else        match  = newmatch;
        }
        else if (!RANGE(term.data.opcode, Operator_First_Assignable, Operator_Last_Assignable)) {
            const uint8_t *ptr = prog.GetDataPtr(field.offset);
            int  res      = 0;
            bool newmatch = false;

            if (field.type == FieldType_string) {
                const char *str;
                uint16_t offset;

                memcpy(&offset, ptr, sizeof(offset));
                str = prog.GetString(offset);

                if (field.offset == ADVBProg::GetActorsDataOffset()) {
                    AString _str(str);
                    uint_t j, m = _str.CountLines();

                    for (j = 0; j < m; j++) {
                        newmatch = MatchString(term, _str.Line(j), true);
                        if (newmatch) break;
                    }

                    if (term.data.opcode & Operator_Inverted) newmatch = !newmatch;
                }
#if DVBDATVERSION > 1
                else if (field.offset == ADVBProg::GetSubCategoryDataOffset()) {
                    AString _str(str);
                    uint_t j, m = _str.CountLines();

                    for (j = 0; j < m; j++) {
                        newmatch = MatchString(term, _str.Line(j), true);
                        if (newmatch) break;
                    }

                    if (term.data.opcode & Operator_Inverted) newmatch = !newmatch;
                }
#endif
                else newmatch = MatchString(term, str);
            }
            else if (field.type == FieldType_prog) {
                newmatch = ADVBProg::SameProgramme(prog, *term.value.prog);

                if (term.data.opcode & Operator_Inverted) newmatch = !newmatch;
            }
            else {
                switch (field.type) {
                    case FieldType_date: {
                        uint64_t val;

                        memcpy(&val, ptr, sizeof(val));
                        res = CompareDates(val, term);
                        break;
                    }

                    case FieldType_span: {
                        uint64_t val1, val2, val;

                        memcpy(&val1, ptr, sizeof(val1)); ptr += sizeof(val1);
                        memcpy(&val2, ptr, sizeof(val2));

                        val = SUBZ(val2, val1);
                        //debug("Span: comparing %s with %s\n", AValue(val).ToString().str(), AValue(term.value.u64).ToString().str());
                        res = COMPARE_ITEMS(val, term.value.u64);
                        break;
                    }

                    case FieldType_span_single: {
                        uint64_t val;

                        memcpy(&val, ptr, sizeof(val));

                        res = COMPARE_ITEMS(val, term.value.u64);
                        break;
                    }

                    case FieldType_age: {
                        uint64_t val1, val2, val;

                        memcpy(&val1, ptr, sizeof(val1));
                        val2 = (uint64_t)ADateTime().TimeStamp(true);

                        val = SUBZ(val2, val1);
                        //debug("Age: comparing %s with %s\n", AValue(val).ToString().str(), AValue(term.value.u64).ToString().str());
                        res = COMPARE_ITEMS(val, term.value.u64);
                        break;
                    }

                    case FieldType_uint32_t: {
                        uint32_t val;

                        memcpy(&val, ptr, sizeof(val));
                        res = COMPARE_ITEMS(val, term.value.u32);
                        break;
                    }

                    case FieldType_sint32_t: {
                        sint32_t val;

                        memcpy(&val, ptr, sizeof(val));
                        res = COMPARE_ITEMS(val, term.value.s32);
                        break;
                    }

                    case FieldType_uint16_t: {
                        uint16_t val;

                        memcpy(&val, ptr, sizeof(val));
                        res = COMPARE_ITEMS(val, term.value.u16);
                        break;
                    }

                    case FieldType_sint16_t: {
                        sint16_t val;

                        memcpy(&val, ptr, sizeof(val));
                        res = COMPARE_ITEMS(val, term.value.s16);
                        break;
                    }

                    case FieldType_uint8_t: {
                        uint8_t val;

                        memcpy(&val, ptr, sizeof(val));
                        res = COMPARE_ITEMS(val, term.value.u8);
                        break;
                    }

                    case FieldType_sint8_t: {
                        sint8_t val;

                        memcpy(&val, ptr, sizeof(val));
                        res = COMPARE_ITEMS(val, term.value.s8);
                        break;
                    }

                    case FieldType_double: {
                        double val;

                        memcpy(&val, ptr, sizeof(val));
                        res = COMPARE_ITEMS(val, term.value.f64);
                        break;
                    }

                    case FieldType_external_uint32_t:
                        res = prog.CompareExternal(field.offset, term.value.u32);
                        break;

                    case FieldType_external_sint32_t:
                        res = prog.CompareExternal(field.offset, term.value.s32);
                        break;

                    case FieldType_external_uint64_t:
                        res = prog.CompareExternal(field.offset, term.value.u64);
                        break;

                    case FieldType_external_sint64_t:
                        res = prog.CompareExternal(field.offset, term.value.s64);
                        break;

                    case FieldType_external_date: {
                        ADateTime dt;
                        uint64_t  val;

                        prog.GetExternal(field.offset, val);
                        res = CompareDates(val, term);
                        break;
                    }

                    case FieldType_external_double:
                        res = prog.CompareExternal(field.offset, term.value.f64);
                        break;

                    case FieldType_flag...FieldType_lastflag: {
                        uint8_t cmp = prog.GetFlag(field.type - FieldType_flag);

                        res = COMPARE_ITEMS(cmp, term.value.u8);
                        break;
                    }
                }

                switch (term.data.opcode & ~Operator_Inverted) {
                    case Operator_EQ:
                        newmatch = (res == 0);
                        break;
                    case Operator_GT:
                        newmatch = (res > 0);
                        break;
                    case Operator_GE:
                        newmatch = (res >= 0);
                        break;
                    case Operator_LT:
                        newmatch = (res < 0);
                        break;
                    case Operator_LE:
                        newmatch = (res <= 0);
                        break;
                }

                if (term.data.opcode & Operator_Inverted) newmatch = !newmatch;
            }

            if (orflag) match |= newmatch;
            else        match  = newmatch;
        }

        orflag = term.data.orflag;
    }

    return match;
}

void ADVBPatterns::AssignValues(ADVBProg& prog, const PATTERN& pattern)
{
    AString user = prog.GetUser();
    uint_t i;

    if (user.Valid()) {
        const ADVBConfig& config = ADVBConfig::Get();
        uint_t nfields;
        const FIELD *fields = ADVBProg::GetFields(nfields);

        for (i = 0; i < nfields; i++) {
            const FIELD& field = fields[i];

            if (field.assignable &&
                (field.offset != ADVBProg::GetUserDataOffset()) &&
                (field.offset != ADVBProg::GetDirDataOffset())) {
                AString val = config.GetUserSubItemConfigItem(user, prog.GetModifiedCategory(), field.name);

                if (val.Valid()) {
                    VALUE value;

                    GetFieldValue(field, value, val);

                    AssignValue(prog, field, value);
                }
            }
        }
    }

    UpdateValues(prog, pattern);
}

void ADVBPatterns::UpdateValues(ADVBProg& prog, const PATTERN& pattern)
{
    const ADataList& list = pattern.list;
    uint_t i, n = list.Count();

    for (i = 0; i < n; i++) {
        const TERM&  term  = *(const TERM *)list[i];
        const FIELD& field = *term.field;

        if      (term.pattern) UpdateValues(prog, *term.pattern);
        else if (RANGE(term.data.opcode, Operator_First_Assignable, Operator_Last_Assignable) && (field.offset != ADVBProg::GetUserDataOffset())) {
            AssignValue(prog, field, term.value, term.data.opcode);
        }
    }
}

ADVBPatterns::TERM *ADVBPatterns::DuplicateTerm(const TERM *term)
{
    TERM *term1;

    if ((term1 = new TERM) != NULL) {
        *term1 = *term;

        switch (term1->field->type) {
            default:
                break;

            case FieldType_string:
                term1->value.str = strdup(term1->value.str);
                break;

            case FieldType_prog:
                term1->value.prog = new ADVBProg(*term1->value.prog);
                break;
        }
    }

    return term1;
}

AString ADVBPatterns::ToString(const VALUE& val, uint8_t fieldtype, uint8_t datetype)
{
    ADateTime dt;
    AString str;

    switch (fieldtype) {
        case FieldType_string:
            str.printf("'%s'", val.str);
            break;
        case FieldType_date:
            switch (datetype) {
                case DateType_fulldate:
                    dt = val.u64;
                    str.printf("%s", dt.DateFormat("%d %Y-%M-%D %h:%m:%s.%S").str());
                    break;

                case DateType_time:
                    dt.Set(0, (uint32_t)val.u64);
                    str.printf("%s", dt.DateFormat("%h:%m:%s.%S").str());
                    break;

                case DateType_date:
                    dt.Set((uint32_t)val.u64, 0);
                    str.printf("%s", dt.DateFormat("%Y-%M-%D").str());
                    break;

                case DateType_weekday:
                    str.printf("%s", dt.GetDayName(val.u64));
                    break;
            }
            break;
        case FieldType_uint32_t:
        case FieldType_external_uint32_t:
            str.printf("%lu", (ulong_t)val.u32);
            break;
        case FieldType_sint32_t:
        case FieldType_external_sint32_t:
            str.printf("%ld", (slong_t)val.s32);
            break;
        case FieldType_uint16_t:
            str.printf("%u", (uint_t)val.u16);
            break;
        case FieldType_sint16_t:
            str.printf("%d", (sint_t)val.s16);
            break;
        case FieldType_uint8_t:
            str.printf("%u", (uint_t)val.u8);
            break;
        case FieldType_sint8_t:
            str.printf("%d", (sint_t)val.s8);
            break;
        case FieldType_double:
        case FieldType_external_double:
            str.printf("%0.14e", val.f64);
            break;
        case FieldType_prog:
            str.printf("%s", val.prog->GetQuickDescription().str());
            break;
        case FieldType_flag...FieldType_lastflag:
            str.printf("flag%u", (uint_t)(fieldtype - FieldType_flag));
            break;
    }

    return str;
}

AString ADVBPatterns::ToString(const FIELD& val)
{
    AString str;

    str.printf("Name '%s' ('%s') type %u assignable %u", val.name, val.desc, (uint_t)val.type, (uint_t)val.assignable);

    return str;
}

AString ADVBPatterns::ToString(const TERMDATA& val)
{
    AString str;

    str.printf("Term [%u:%u] '%s' field %u opcode %u opindex %u orflag %u",
               val.start, val.length, val.value.str(), (uint_t)val.field, (uint_t)val.opcode, (uint_t)val.opindex, (uint_t)val.orflag);

    return str;
}

AString ADVBPatterns::ToString(const TERM& val, uint_t level)
{
    AString str;

    if      (val.pattern) str.printf("Sub-pattern '%s' orflag %u:\n%s", val.value.str, (uint_t)val.data.orflag, ToString(*val.pattern, level + 1).str());
    else if (val.field)   str.printf("%s, %s, %s", ToString(val.data).str(), ToString(*val.field).str(), ToString(val.value, val.field->type, val.datetype).str());

    return str;
}

AString ADVBPatterns::ToString(const PATTERN& pattern, uint_t level)
{
    const ADataList& list = pattern.list;
    uint_t i, n = list.Count();
    AString str;

    for (i = 0; i < n; i++) {
        const TERM& term = *(const TERM *)list[i];

        str.printf("%sTerm %u/%u: %s\n", AString("  ").Copies(level).str(), i + 1, n, ToString(term, level).str());
    }

    return str;
}
