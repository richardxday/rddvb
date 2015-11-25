
#include <rdlib/Regex.h>

#include "config.h"
#include "dvblock.h"

#include "dvbpatterns.h"
#include "dvbprog.h"

ADVBPatterns _patterns;

ADVBPatterns::OPERATOR ADVBPatterns::operators[] = {
    {"!=", 	false, FieldTypes_Default | FieldTypes_Prog, Operator_NE, "Is not equal to", 0},
    {"<>", 	false, FieldTypes_Default | FieldTypes_Prog, Operator_NE, "Is not equal to", 0},
    {"<=", 	false, FieldTypes_Default,                   Operator_LE, "Is less than or equal to", 0},
    {">=", 	false, FieldTypes_Default,                   Operator_GE, "Is greater than or equal to", 0},
    {"=",  	false, FieldTypes_Default | FieldTypes_Prog, Operator_EQ, "Equals", 0},
    {"<",  	false, FieldTypes_Default,                   Operator_LT, "Is less than", 0},
    {">",  	false, FieldTypes_Default,                   Operator_GT, "Is greater than", 0},

    {"!@<", false, FieldTypes_String,                    Operator_NotStartsWith, "Does not start with", 0},
    {"!@>", false, FieldTypes_String,                    Operator_NotEndsWith, "Does not end with", 0},
    {"!%<", false, FieldTypes_String,                    Operator_NotStarts, "Does not start", 0},
    {"!%>", false, FieldTypes_String,                    Operator_NotEnds, "Does not end", 0},
    {"!=", 	false, FieldTypes_String,                    Operator_NE, "Is not equal to", 0},
    {"!~", 	false, FieldTypes_String,                    Operator_NotRegex, "Does not match regex", 0},
    {"!@", 	false, FieldTypes_String,                    Operator_NotContains, "Does not contain", 0},
    {"!%",  false, FieldTypes_String,                    Operator_NotWithin, "Is not within", 0},
    {"<>", 	false, FieldTypes_String,                    Operator_NE, "Is not equal to", 0},
    {"<=", 	false, FieldTypes_String,                    Operator_LE, "Is less than or equal to", 0},
    {">=", 	false, FieldTypes_String,                    Operator_GE, "Is greater than or equal to", 0},
    {"@<", 	false, FieldTypes_String,                    Operator_StartsWith, "Starts with", 0},
    {"@>", 	false, FieldTypes_String,                    Operator_EndsWith, "Ends with", 0},
    {"%<", 	false, FieldTypes_String,                    Operator_Starts, "Starts", 0},
    {"%>", 	false, FieldTypes_String,                    Operator_Ends, "Ends", 0},
    {"=",  	false, FieldTypes_String,                    Operator_EQ, "Equals", 0},
    {"~",  	false, FieldTypes_String,                    Operator_Regex, "Matches regex", 0},
    {"@",  	false, FieldTypes_String,                    Operator_Contains, "Contains", 0},
    {"%",  	false, FieldTypes_String,                    Operator_Within, "Is within", 0},
    {"<",  	false, FieldTypes_String,                    Operator_LT, "Is less than", 0},
    {">",  	false, FieldTypes_String,                    Operator_GT, "Is greater than", 0},

    {"+=", 	true,  FieldTypes_String,                    Operator_Concat, "Is concatenated with", 0},
    {":=", 	true,  FieldTypes_String,                    Operator_Assign, "Is set to", 0},
    {":=", 	true,  FieldTypes_Default,                   Operator_Assign, "Is set to", 0},

    {"+=", 	true,  FieldTypes_Number,                    Operator_Add, "Is incremented by", 0},
    {"-=", 	true,  FieldTypes_Number,                    Operator_Subtract, "Is decremented by", 0},
    {"*=", 	true,  FieldTypes_Number,                    Operator_Multiply, "Is multiplied by", 0},
    {"/=", 	true,  FieldTypes_Number,                    Operator_Divide, "Is divided by", 0},
    {":>", 	true,  FieldTypes_Number,                    Operator_Maximum, "Is maximized with", 0},
    {":<", 	true,  FieldTypes_Number,                    Operator_Minimum, "Is minimized with", 0},

	//{"+=", 	false, FieldTypes_String,  	 			 	 Operator_Concat},
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

void ADVBPatterns::AddPatternToFile(const AString& filename, const AString& pattern)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AStdFile fp;
	bool done = false;

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
		}
	}
}

void ADVBPatterns::UpdatePattern(const AString& olduser, const AString& oldpattern, const AString& newuser, const AString& newpattern)
{
	ADVBLock lock("patterns");

	if (newuser != olduser) {
		DeletePattern(olduser, oldpattern);
		if (newpattern.Valid()) {
			InsertPattern(newuser, newpattern);
		}
	}
	else if (newpattern != oldpattern) UpdatePattern(newuser, oldpattern, newpattern);
}

void ADVBPatterns::UpdatePattern(const AString& user, const AString& pattern, const AString& newpattern)
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBLock lock("patterns");
	
	if (user.Empty() || !UpdatePatternInFile(config.GetUserPatternsPattern().SearchAndReplace("{#?}", user), pattern, newpattern)) {
		UpdatePatternInFile(config.GetPatternsFile(), pattern, newpattern);
	}
}

void ADVBPatterns::InsertPattern(const AString& user, const AString& pattern)
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADVBLock lock("patterns");
	
	if (user.Valid()) AddPatternToFile(config.GetUserPatternsPattern().SearchAndReplace("{#?}", user), pattern);
	else			  AddPatternToFile(config.GetPatternsFile(), pattern);
}

void ADVBPatterns::DeletePattern(const AString& user, const AString& pattern)
{
	UpdatePattern(user, pattern, "");
}

void ADVBPatterns::EnablePattern(const AString& user, const AString& pattern)
{
	if (pattern[0] == '#') {
		UpdatePattern(user, pattern, pattern.Mid(1));
	}
}

void ADVBPatterns::DisablePattern(const AString& user, const AString& pattern)
{
	if (pattern[0] != '#') {
		UpdatePattern(user, pattern, "#" + pattern);
	}
}

void ADVBPatterns::GetFieldValue(const FIELD& field, VALUE& value, AString& val)
{
	switch (field.type) {
		case FieldType_string:
			value.str = val.Steal();
			break;

		case FieldType_date:
			value.u64 = (uint64_t)ADateTime(val, ADateTime::Time_Absolute);
			break;

		case FieldType_span:
			value.u64 = (uint64_t)ADateTime(val, ADateTime::Time_Absolute);
			break;

		case FieldType_uint32_t:
			value.u32 = (uint32_t)val;
			break;

		case FieldType_sint32_t:
			value.s32 = (sint32_t)val;
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
			
			str += value.str;

			prog.SetString((const uint16_t *)ptr, str.str());
			break;
		}

		case FieldType_date:
		case FieldType_span:
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
					else		   val = val1;
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

			//debug("%" FMT64 "s / %" FMT64 "s = %" FMT64 "s\n", val1, val2, val);

			Int64sToTermType(ptr, val, field.type);

			// detect writing to dvbcard
			if (ptr == &prog.data->dvbcard) {
				//debug("DVB card specified as %u\n", (uint_t)data->dvbcard);
				prog.SetDVBCardSpecified();
			}
			break;
		}

		case FieldType_flag...FieldType_lastflag: {
			uint32_t flags, mask = 1UL << (field.type - FieldType_flag);

			memcpy(&flags, ptr, sizeof(flags));
			if (value.u8) flags |=  mask;
			else		  flags &= ~mask;
			memcpy(ptr, &flags, sizeof(flags));
			break;
		}
	}
}

void ADVBPatterns::__DeleteTerm(uptr_t item, void *context)
{
	TERM *term = (TERM *)item;

	UNUSED(context);

	if (term->field->type == FieldType_string) {
		if (term->value.str) delete[] term->value.str;
	}
	else if (term->field->type == FieldType_prog) {
		if (term->value.prog) delete term->value.prog;
	}

	delete term;
}

const ADVBPatterns::OPERATOR *ADVBPatterns::FindOperator(const PATTERN& pattern, uint_t term)
{
	const TERM *pterm = (const TERM *)pattern.list[term];

	if (pterm) {
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

	return pterm ? RANGE(pterm->data.opcode, Operator_First_Assignable, Operator_Last_Assignable) : false;
}

AString ADVBPatterns::GetPatternDefinitionsJSON()
{
	AString str;
	uint_t i, j, nfields;
	const FIELD *fields = ADVBProg::GetFields(nfields);

	str.printf("\"patterndefs\":");
	str.printf("{\"fields\":[");
	
	for (i = 0; i < nfields; i++) {
		const FIELD& field = fields[i];

		if (i) str.printf(",");
		str.printf("{\"name\":\"%s\"", 	 JSONFormat(field.name).str());
		str.printf(",\"desc\":\"%s\"", 	 JSONFormat(field.desc).str());
		str.printf(",\"type\":%u",     	 field.type);
		str.printf(",\"assignable\":%s", field.assignable ? "true" : "false");
		str.printf(",\"operators\":[");

		bool flag = false;
		for (j = 0; j < NUMBEROF(operators); j++) {
			const OPERATOR& oper = operators[j];

			if ((field.assignable == oper.assign) &&
				(oper.fieldtypes & (1U << field.type))) {
				if (flag) str.printf(",");
				str.printf("%u", j);
				flag = true;
			}
		}

		str.printf("]}");
	}

	str.printf("]");
	str.printf(",\"fieldnames\":{");

	for (i = 0; i < nfields; i++) {
		const FIELD& field = fields[i];

		if (i) str.printf(",");
		str.printf("\"%s\":%u", JSONFormat(field.name).str(), i);
	}

	str.printf("}");
	str.printf(",\"operators\":[");

	for (j = 0; j < NUMBEROF(operators); j++) {
		const OPERATOR& oper = operators[j];

		if (j) str.printf(",");
		str.printf("{\"text\":\"%s\"", JSONFormat(oper.str).str());
		str.printf(",\"desc\":\"%s\"", JSONFormat(oper.desc).str());
		str.printf(",\"opcode\":%u",   (uint_t)oper.opcode);
		str.printf(",\"assign\":%s}",  oper.assign ? "true" : "false");
	}

	str.printf("]");
	str.printf(",\"orflags\":[");

	for (j = 0; j < 2; j++) {
		if (j) str.printf(",");
		str.printf("{\"text\":\"%s\"", JSONFormat(j ? "or" : "and").str());
		str.printf(",\"desc\":\"%s\"", JSONFormat(j ? "Or the next term" : "And the next term").str());
		str.printf(",\"value\":%u}",   j);
	}

	str.printf("]}");

	return str;
}

int ADVBPatterns::SortTermsByAssign(uptr_t item1, uptr_t item2, void *context)
{
	const TERM *term1 = (const TERM *)item1;
	const TERM *term2 = (const TERM *)item2;
	bool assign1 = RANGE(term1->data.opcode, Operator_First_Assignable, Operator_Last_Assignable);
	bool assign2 = RANGE(term2->data.opcode, Operator_First_Assignable, Operator_Last_Assignable);

	UNUSED(context);

	if ( assign1 && !assign2) return  1;
	if (!assign1 &&  assign2) return -1;
	if ( assign1 &&  assign2) return COMPARE_ITEMS(term1->data.field, term2->data.field);

	return 0;
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

AString ADVBPatterns::ParsePattern(const AString& line, PATTERN& pattern, const AString& user)
{
	const ADVBConfig& config = ADVBConfig::Get();
	ADataList& list   = pattern.list;
	AString&   errors = pattern.errors;
	TERM   *term;
	uint_t i;

	pattern.exclude    = false;
	pattern.enabled    = true;
	pattern.scorebased = false;
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
			if (!IsSymbolStart(line[i])) {
				errors.printf("Character '%c' (at %u) is not a legal field start character (term %u)", line[i], i, list.Count() + 1);
				break;
			}

			uint_t fieldstart = i++;
			while (IsSymbolChar(line[i])) i++;
			AString field = line.Mid(fieldstart, i - fieldstart).ToLower();

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
					(operators[j].fieldtypes & (1U << fieldptr->type)) &&
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

			bool orflag = false;
			if ((line.Mid(i, 2).ToLower() == "or") && IsWhiteSpace(line[i + 2])) {
				orflag = true;
				i += 2;

				while (IsWhiteSpace(line[i])) i++;
			}
			else if ((line[i] == '|') && IsWhiteSpace(line[i + 1])) {
				orflag = true;
				i += 1;

				while (IsWhiteSpace(line[i])) i++;
			}

			if ((term = new TERM) != NULL) {
				term->data.start   = fieldstart;
				term->data.length  = i - fieldstart;
				term->data.field   = fieldptr - ADVBProg::fields;
				term->data.opcode  = opcode;
				term->data.opindex = opindex;
				term->data.value   = value;
				term->data.orflag  = (orflag && !RANGE(opcode, Operator_First_Assignable, Operator_Last_Assignable));
				term->field    	   = fieldptr;
				term->datetype 	   = DateType_none;

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

					case FieldType_date: {
						ADateTime dt;
						uint_t specified;

						dt.StrToDate(value, ADateTime::Time_Relative_Local, &specified);

						//debug("Value '%s', specified %u\n", value.str(), specified);

						if (!specified) {
							errors.printf("Failed to parse date '%s' (term %u)", value.str(), list.Count() + 1);
							break;
						}
						else if (((specified == ADateTime::Specified_Day) && (stricmp(term->field->name, "on") == 0)) ||
								 (stricmp(term->field->name, "day") == 0)) {
							//debug("Date from '%s' is '%s' (week day only)\n", value.str(), dt.DateToStr().str());
							term->value.u64 = dt.GetWeekDay();
							term->datetype  = DateType_weekday;
						}
						else if (specified & ADateTime::Specified_Day) {
							specified |= ADateTime::Specified_Date;
						}

						if (term->datetype == DateType_none) {
							specified &= ADateTime::Specified_Date | ADateTime::Specified_Time;

							if (specified == (ADateTime::Specified_Date | ADateTime::Specified_Time)) {
								//debug("Date from '%s' is '%s' (full date and time)\n", value.str(), dt.DateToStr().str());
								term->value.u64 = (uint64_t)dt;
								term->datetype  = DateType_fulldate;
							}
							else if (specified == ADateTime::Specified_Date) {
								//debug("Date from '%s' is '%s' (date only)\n", value.str(), dt.DateToStr().str());
								term->value.u64 = dt.GetDays();
								term->datetype  = DateType_date;
							}
							else if (specified == ADateTime::Specified_Time) {
								//debug("Date from '%s' is '%s' (time only)\n", value.str(), dt.DateToStr().str());
								term->value.u64 = dt.GetMS();
								term->datetype  = DateType_time;
							}
							else {
								errors.printf("Unknown date specifier '%s' (term %u)", value.str(), list.Count() + 1);
							}
						}
						break;
					}

					case FieldType_span: {
						ADateTime dt;
						term->value.u64 = (uint64_t)ADateTime(value, ADateTime::Time_Absolute);
						break;
					}

					case FieldType_uint32_t:
						term->value.u32 = (uint32_t)value;
						break;

					case FieldType_sint32_t:
						term->value.s32 = (sint32_t)value;
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
						errors.printf("Unknown field '%s' type (%u) (term %u)", field.str(), (uint_t)term->field->type, list.Count() + 1);
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
			for (i = 0; puretext[i] && (IsAlphaChar(puretext[i]) || IsNumeralChar(puretext[i]) || (puretext[i] == ' ') || (puretext[i] == '-')); i++) ;

			if (i == (uint_t)puretext.len()) {
				AString newtext = "@\"" + puretext + "\"";
				AString newline = ("title" + newtext +
								   " or subtitle" + newtext +
								   " or desc" + newtext +
								   " or actor" + newtext +
								   " or director" + newtext +
								   " or category" + newtext +
								   conditions);

				//debug("Split '%s' into:\npure text:'%s'\nconditions:'%s'\nnew pattern:'%s'\n", line.str(), puretext.str(), conditions.str(), newline.str());
			
				pattern.errors.Delete();
				errors = ParsePattern(newline, pattern, user);
			}
		}
	}

	if (errors.Valid()) pattern.enabled = false;

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
		uint_t 	 bytes    = 1U << (type >> 1);
		bool   	 issigned = ((type & 1) == 0);

		if (issigned) {
			maxval = (sint64_t)((1ULL << ((bytes << 3) - 1)) - 1);
			minval = -maxval - 1;
		}
		else {
			minval = 0;
			maxval = (sint64_t)((1ULL << (bytes << 3)) - 1);
		}

		//debug("val = %" FMT64 "s, min = " FMT64 "s, max = " FMT64 "s\n", val, minval, maxval);

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

bool ADVBPatterns::MatchString(const TERM& term, const char *str)
{
	bool match = false;
	
	switch (term.data.opcode & ~Operator_Inverted) {
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
			int res = CompareNoCase(term.value.str, str);

			switch (term.data.opcode & ~Operator_Inverted) {
				case Operator_EQ:
					match = (res == 0);
					break;
				case Operator_GT:
					match = (res > 0);
					break;
				case Operator_GE:
					match = (res >= 0);
					break;
				case Operator_LT:
					match = (res < 0);
					break;
				case Operator_LE:
					match = (res <= 0);
					break;
			}
			break;
		}
	}

	return (term.data.opcode & Operator_Inverted) ? !match : match;
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
			  
		if (!RANGE(term.data.opcode, Operator_First_Assignable, Operator_Last_Assignable)) {
			const uint8_t *ptr = prog.GetDataPtr(field.offset);
			int  res      = 0;
			bool newmatch = false;

			if (field.type == FieldType_string) {
				const char *str;
				uint16_t offset;

				memcpy(&offset, ptr, sizeof(offset));
				str = prog.GetString(offset);

				if (field.offset == ADVBProg::GetActorsDataOffset()) {
					AString actors(str);
					uint_t j, m = actors.CountLines();

					for (j = 0; j < m; j++) {
						newmatch = MatchString(term, actors.Line(j));
						if (newmatch) break;
					}
				}
				else newmatch = MatchString(term, str);
			}
			else if (field.type == FieldType_prog) {
				newmatch = ADVBProg::SameProgramme(prog, *term.value.prog);

				if (term.data.opcode & Operator_Inverted) newmatch = !newmatch;
			}
			else {
				switch (field.type) {
					case FieldType_date: {
						ADateTime dt;
						uint64_t  val;

						memcpy(&val, ptr, sizeof(val));
						dt  = ADateTime(val).UTCToLocal();
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
					
						res = COMPARE_ITEMS(val, term.value.u64);
						break;
					}

					case FieldType_span: {
						uint64_t val1, val2, val;

						memcpy(&val1, ptr, sizeof(val1)); ptr += sizeof(val1);
						memcpy(&val2, ptr, sizeof(val2));

						val = SUBZ(val2, val1);
						//debug("Span: comparing %lu with %lu\n", (ulong_t)val, (ulong_t)term.value.u64);
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
			else		match  = newmatch;
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

			if (field.assignable && (field.offset != ADVBProg::GetUserDataOffset())) {
				AString val = config.GetUserConfigItem(user, field.name);

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

		if (RANGE(term.data.opcode, Operator_First_Assignable, Operator_Last_Assignable) && (field.offset != ADVBProg::GetUserDataOffset())) {
			AssignValue(prog, field, term.value, term.data.opcode);
		}
	}
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
			str.printf("%lu", (ulong_t)val.u32);
			break;
		case FieldType_sint32_t:
			str.printf("%ld", (slong_t)val.u32);
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
		case FieldType_prog:
			str.printf("%s", val.prog->GetQuickDescription().str());
			break;
		case FieldType_flag...FieldType_lastflag:
			str.printf("flag%u", (uint_t)(fieldtype - FieldType_flag));
			break;
	}
	
	return str;
}

AString ADVBPatterns::ToString(const TERM& val)
{
	AString str;

	str.printf("%s, %s, %s", ToString(val.data).str(), ToString(*val.field).str(), ToString(val.value, val.field->type, val.datetype).str());
	
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

	str.printf("Term [%u:%u] '%s' field %u opcode %u opindex %u orflag %u", val.start, val.length, val.value.str(), (uint_t)val.field, (uint_t)val.opcode, (uint_t)val.opindex, (uint_t)val.orflag);
	
	return str;
}

AString ADVBPatterns::ToString(const PATTERN& pattern)
{
	const ADataList& list = pattern.list;
	uint_t i, n = list.Count();
	AString str;
	
	for (i = 0; i < n; i++) {
		const TERM& term = *(const TERM *)list[i];
		
		str.printf("Term %u/%u: %s\n", i, n, ToString(term).str());
	}

	return str;
}
