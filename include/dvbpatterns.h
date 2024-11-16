#ifndef __DVB_PATTERN__
#define __DVB_PATTERN__

#include <rapidjson/document.h>

#include <regex>

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
        bool      enabled;
        bool      scorebased;
        uint_t    pos;
        sint_t    pri;
        AString   user;
        AString   pattern;
        AString   errors;
        ADataList list;
    } pattern_t;

    static void __DeletePattern(pattern_t *pattern);
    static void __DeletePattern(uptr_t item, void *context) {
        UNUSED(context);
        __DeletePattern((pattern_t *)item);
    }

    static const pattern_t DefaultPattern;

    static AString ParsePattern(const AString& _line, pattern_t& pattern, const AString& user = "");
    static void    AppendTerms(pattern_t& dstpattern, const pattern_t& srcpattern, bool excludeduplicatefields = true);
    static AString RemoveDuplicateTerms(pattern_t& pattern);

    static bool Match(const ADVBProg& prog, const pattern_t& pattern);
    static void AssignValues(ADVBProg& prog, const pattern_t& pattern);
    static void UpdateValues(ADVBProg& prog, const pattern_t& pattern);

    static rapidjson::Value GetPatternDefinitionJSON(rapidjson::Document& doc);

    static const char *GetOperatorText(const pattern_t& pattern, uint_t term);
    static const char *GetOperatorDescription(const pattern_t& pattern, uint_t term);
    static bool       OperatorIsAssign(const pattern_t& pattern, uint_t term);

    static AString    ToString(const pattern_t& pattern, uint_t level = 0);

    typedef struct {
        uint_t    start;
        uint_t    length;
        uint8_t   field;
        uint8_t   opcode;
        uint8_t   opindex;
        AString   value;
        bool      orflag;
    } termdata_t;

    typedef struct {
        const char *name;
        uint8_t    type;
        bool       assignable;
        uint16_t   offset;
        const char *desc;
    } field_t;

    static const termdata_t *GetTermData(const pattern_t& pattern, uint_t term) {return &(((const term_t *)pattern.list[term])->data);}

protected:
    friend class ADVBProg;

    enum {
        FieldType_string = 0,
        FieldType_date,
        FieldType_span,
        FieldType_span_single,
        FieldType_age,
        FieldType_uint64_t,
        FieldType_sint64_t,
        FieldType_uint32_t,
        FieldType_sint32_t,
        FieldType_uint16_t,
        FieldType_sint16_t,
        FieldType_uint8_t,
        FieldType_sint8_t,
        FieldType_double,
        FieldType_prog,

        _FieldType_external_first,
        FieldType_external_string = _FieldType_external_first,
        FieldType_external_uint32_t,
        FieldType_external_uint64_t,
        FieldType_external_sint32_t,
        FieldType_external_sint64_t,
        FieldType_external_double,
        FieldType_external_date,
        _FieldType_external_last = FieldType_external_date,

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

        Operator_NE               = Operator_Inverted | Operator_EQ,
        Operator_LE               = Operator_Inverted | Operator_GT,
        Operator_GE               = Operator_Inverted | Operator_LT,

        Operator_StartsWithLE     = Operator_Inverted | Operator_StartsWithGT,
        Operator_StartsWithGE     = Operator_Inverted | Operator_StartsWithLT,
        Operator_EndsWithLE       = Operator_Inverted | Operator_EndsWithGT,
        Operator_EndsWithGE       = Operator_Inverted | Operator_EndsWithLT,

        Operator_NotRegex         = Operator_Inverted | Operator_Regex,
        Operator_NotContains      = Operator_Inverted | Operator_Contains,
        Operator_NotStartsWith    = Operator_Inverted | Operator_StartsWith,
        Operator_NotEndsWith      = Operator_Inverted | Operator_EndsWith,
        Operator_NotWithin        = Operator_Inverted | Operator_Within,
        Operator_NotStarts        = Operator_Inverted | Operator_Starts,
        Operator_NotEnds          = Operator_Inverted | Operator_Ends,

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
                              (1U << FieldType_sint8_t)  |
                              (1U << FieldType_double)),
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
    } operator_t;

    typedef union {
        uint64_t         u64;
        sint64_t         s64;
        uint32_t         u32;
        sint32_t         s32;
        uint16_t         u16;
        sint16_t         s16;
        uint8_t          u8;
        sint8_t          s8;
        double           f64;
        const char       *str;
        const ADVBProg   *prog;
        const std::regex *regex;
    } value_t;

    typedef struct {
        termdata_t    data;
        const field_t *field;
        uint8_t     datetype;
        value_t       value;
        pattern_t   *pattern;
        std::regex  regex;
    } term_t;

    static void __DeleteTerm(term_t *term);
    static void __DeleteTerm(uptr_t item, void *context) {
        UNUSED(context);
        __DeleteTerm((term_t *)item);
    }

    static bool UpdatePatternInFile(const AString& filename, const AString& pattern, const AString& newpattern);
    static bool AddPatternToFile(const AString& filename, const AString& pattern);

    static void GetFieldValue(const field_t& field, value_t& value, AString& val);
    static void AssignValue(ADVBProg& prog, const field_t& field, const value_t& value, uint8_t termtype = Operator_EQ);

    static const operator_t *FindOperator(const pattern_t& pattern, uint_t term);

    static int SortTermsByAssign(uptr_t item1, uptr_t item2, void *context);
    static term_t *DuplicateTerm(const term_t *term);

    static bool MatchString(const term_t& term, const char *str, bool ignoreinvert = false);

    static void     GetString(const ADVBProg& prog, const field_t& field, AString& str);
    static void     SetString(ADVBProg& prog, const field_t& field, const AString& str);

    static sint64_t TermTypeToInt64s(const void *p, uint_t termtype);
    static void     Int64sToTermType(void *p, sint64_t val, uint_t termtype);

    static double   TermTypeToDouble(const void *p, uint_t termtype);
    static void     DoubleToTermType(void *p, double val, uint_t termtype);

    static AString ToString(const value_t&    val, uint8_t fieldtype, uint8_t datetype);
    static AString ToString(const field_t&    val);
    static AString ToString(const termdata_t& val);
    static AString ToString(const term_t&     val, uint_t level);

    static uint_t Skip(const AString& line, uint_t i, char terminator = ')');
    static uint_t CheckOrStatement(const AString& line, uint_t i, bool& orflag);

    static int CompareDates(uint64_t val, const term_t& term);

protected:
    static operator_t operators[];
};

#endif
