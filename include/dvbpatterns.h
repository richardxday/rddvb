#ifndef __DVB_PATTERN__
#define __DVB_PATTERN__

#include <rapidjson/document.h>

#include "config.h"

class ADVBProg;
class ADVBPatterns {
public:
	ADVBPatterns();
	~ADVBPatterns() {}

	static bool   UpdatePattern(const AString& olduser, const AString& oldpattern, const AString& newuser, const AString& newpattern);
	static bool   UpdatePattern(const AString& user, const AString& pattern, const AString& newpattern);
	static bool   InsertPattern(const AString& user, const AString& pattern);
	static bool   DeletePattern(const AString& user, const AString& pattern);
	static bool   EnablePattern(const AString& user, const AString& pattern);
	static bool   DisablePattern(const AString& user, const AString& pattern);

	static uint_t ParsePatterns(ADataList& patternlist, const AString& patterns, AString& errors, const AString& sep, const AString& user = "");
	static bool   ParsePattern(ADataList& patternlist, const AString& line, AString& errors, const AString& user = "");

	typedef struct {
		bool      exclude;
		bool	  enabled;
		bool	  scorebased;
		uint_t    pos;
		sint_t	  pri;
		AString   user;
		AString   pattern;
		AString   errors;
		ADataList list;
	} PATTERN;

	static void __DeletePattern(uptr_t item, void *context) {
		UNUSED(context);
		delete (PATTERN *)item;
	}

	static AString ParsePattern(const AString& _line, PATTERN& pattern, const AString& user = "");
	static AString RemoveDuplicateTerms(PATTERN& pattern);

	static bool Match(const ADVBProg& prog, const PATTERN& pattern);
	static void AssignValues(ADVBProg& prog, const PATTERN& pattern);
	static void UpdateValues(ADVBProg& prog, const PATTERN& pattern);

	static rapidjson::Value GetPatternDefinitionJSON(rapidjson::Document& doc);
	
	static const char *GetOperatorText(const PATTERN& pattern, uint_t term);
	static const char *GetOperatorDescription(const PATTERN& pattern, uint_t term);
	static bool       OperatorIsAssign(const PATTERN& pattern, uint_t term);

	static AString    ToString(const PATTERN& pattern, uint_t level = 0);

	typedef struct {
		uint_t    start;
		uint_t    length;
		uint8_t   field;
		uint8_t   opcode;
		uint8_t   opindex;
		AString   value;
		bool	  orflag;
	} TERMDATA;

	typedef struct {
		const char *name;
		uint8_t    type;
		bool       assignable;
		uint16_t   offset;
		const char *desc;
	} FIELD;

	static const TERMDATA *GetTermData(const PATTERN& pattern, uint_t term) {return &(((const TERM *)pattern.list[term])->data);}
	
protected:
	friend class ADVBProg;

	enum {
		FieldType_string = 0,
		FieldType_date,
		FieldType_span,
		FieldType_age,
		FieldType_uint32_t,
		FieldType_sint32_t,
		FieldType_uint16_t,
		FieldType_sint16_t,
		FieldType_uint8_t,
		FieldType_sint8_t,
		FieldType_prog,
		FieldType_external_uint32_t,
		FieldType_external_sint32_t,
		FieldType_flag,
		FieldType_lastflag = FieldType_flag + 63,
	};

	enum {
		Operator_Inverted = 0x80,

		Operator_EQ = 0,
		Operator_LT,
		Operator_GT,

		Operator_Regex,
		Operator_Contains,
		Operator_StartsWith,
		Operator_EndsWith,
		Operator_Within,
		Operator_Starts,
		Operator_Ends,
		Operator_StartsWithLT,
		Operator_StartsWithGT,
		Operator_EndsWithLT,
		Operator_EndsWithGT,
		
		Operator_Assign,
		Operator_Concat,
		Operator_Remove,
		Operator_Add,
		Operator_Subtract,
		Operator_Multiply,
		Operator_Divide,
		Operator_Maximum,
		Operator_Minimum,
		
		Operator_NE 		 	  = Operator_Inverted | Operator_EQ,
		Operator_LE 		 	  = Operator_Inverted | Operator_GT,
		Operator_GE 		 	  = Operator_Inverted | Operator_LT,

		Operator_StartsWithLE	  = Operator_Inverted | Operator_StartsWithGT,
		Operator_StartsWithGE	  = Operator_Inverted | Operator_StartsWithLT,
		Operator_EndsWithLE	      = Operator_Inverted | Operator_EndsWithGT,
		Operator_EndsWithGE	      = Operator_Inverted | Operator_EndsWithLT,

		Operator_NotRegex    	  = Operator_Inverted | Operator_Regex,
		Operator_NotContains 	  = Operator_Inverted | Operator_Contains,
		Operator_NotStartsWith 	  = Operator_Inverted | Operator_StartsWith,
		Operator_NotEndsWith 	  = Operator_Inverted | Operator_EndsWith,
		Operator_NotWithin		  = Operator_Inverted | Operator_Within,
		Operator_NotStarts 	  	  = Operator_Inverted | Operator_Starts,
		Operator_NotEnds 	  	  = Operator_Inverted | Operator_Ends,

		Operator_First_Assignable = Operator_Assign,
		Operator_Last_Assignable  = Operator_Minimum,
	};

	enum {
		DateType_none = 0,
		DateType_fulldate,
		DateType_date,
		DateType_time,
		DateType_weekday,
	};

	enum {
		FieldTypes_String  = 1U << FieldType_string,
		FieldTypes_Number  = ((1U << FieldType_uint32_t) |
							  (1U << FieldType_sint32_t) |
							  (1U << FieldType_uint16_t) |
							  (1U << FieldType_sint16_t) |
							  (1U << FieldType_uint8_t)  |
							  (1U << FieldType_sint8_t)),
		FieldTypes_Prog    = 1U << FieldType_prog,
		FieldTypes_Default = ~(FieldTypes_String | FieldTypes_Prog),
	};

	typedef struct {
		const char *str;
		bool       assign;
		uint32_t   fieldtypes;
		uint8_t    opcode;
		const char *desc;
		uint_t     len;
	} OPERATOR;

	typedef union {
		uint64_t   u64;
		uint32_t   u32;
		sint32_t   s32;
		uint16_t   u16;
		sint16_t   s16;
		uint8_t    u8;
		sint8_t    s8;
		char   	   *str;
		ADVBProg   *prog;
	} VALUE;

	typedef struct {
		TERMDATA    data;
		const FIELD *field;
		uint8_t 	datetype;
		VALUE 		value;
		PATTERN     *pattern;
	} TERM;

protected:
	static bool UpdatePatternInFile(const AString& filename, const AString& pattern, const AString& newpattern);
	static bool AddPatternToFile(const AString& filename, const AString& pattern);

	static void GetFieldValue(const FIELD& field, VALUE& value, AString& val);
	static void AssignValue(ADVBProg& prog, const FIELD& field, const VALUE& value, uint8_t termtype = Operator_EQ);

	static const OPERATOR *FindOperator(const PATTERN& pattern, uint_t term);

	static int SortTermsByAssign(uptr_t item1, uptr_t item2, void *context);

	static bool MatchString(const TERM& term, const char *str, bool ignoreinvert = false);

	static sint64_t TermTypeToInt64s(const void *p, uint_t termtype);
	static void     Int64sToTermType(void *p, sint64_t val, uint_t termtype);

	static void __DeleteTerm(uptr_t item, void *context);

	static AString ToString(const VALUE& 	val, uint8_t fieldtype, uint8_t datetype);
	static AString ToString(const FIELD& 	val);
	static AString ToString(const TERMDATA& val);
	static AString ToString(const TERM&  	val, uint_t level);

	static uint_t Skip(const AString& line, uint_t i, char terminator = ')');
	static uint_t CheckOrStatement(const AString& line, uint_t i, bool& orflag);
	
protected:
	static OPERATOR operators[];
};

#endif
