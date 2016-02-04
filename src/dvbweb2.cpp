
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rdlib/Hash.h>

#include "config.h"
#include "proglist.h"

bool Value(const AHash& vars, AString& val, const AString& var)
{
	AString *p;

	if ((p = (AString *)vars.Read(var)) != NULL) val = *p;
	else val.Delete();

	return (p != NULL);
}

void Merge(ADVBProgList& list, const AString& filename, const uint64_t& start, const uint64_t& end)
{
	ADVBProgList list2;
	ADVBProg     *prog;
	uint_t n = 0;
	
	if (list2.ReadFromFile(filename)) {
		uint_t i;
		
		for (i = 0; i < list2.Count(); i++) {
			const ADVBProg& prog2 = list2[i];
					
			if ((prog2.GetStop() >= start) && (prog2.GetStart() <= end) &&
				((prog = list.FindUUIDWritable(prog2)) != NULL)) {
				*prog = prog2;
				n++;
			}
		}
	}
}

int main(int argc, char *argv[])
{
	const ADVBConfig& config = ADVBConfig::Get(true);
	ADVBProg		  prog;	// ensure ADVBProg initialisation takes place
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

		//debug("dvbweb2: %u lines: %s\n", n, arg.str());

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

	ADVBProgList list;
	if (list.ReadFromFile(config.GetListingsFile())) {
		ADateTime _start, _length("3h", ADateTime::Time_Absolute), _end;
		AString   str;
		uint_t i;
		
		if (Value(vars, str, "start"))  _start.StrToDate(str);
		if (Value(vars, str, "length")) _length.StrToDate(str, ADateTime::Time_Absolute);

		_end = _start + _length;

		uint64_t start = _start;
		uint64_t end   = _end;
		
		for (i = 0; i < list.Count();) {
			if ((list[i].GetStop() < start) || (list[i].GetStart() >= end)) list.DeleteProg(i);
			else i++;
		}

		list.CreateHash();

		Merge(list, config.GetRecordedFile(), start, end);
		Merge(list, config.GetScheduledFile(), start, end);
		Merge(list, config.GetRejectedFile(), start, end);
		Merge(list, config.GetRecordingFile(), start, end);
		Merge(list, config.GetProcessingFile(), start, end);

		printf("{\"progs\":[\n");
		for (i = 0; (i < list.Count()) && !HasQuit();) {
			printf("%s\n", list[i].ExportToJSON(true).str());
		}
		printf("]}");
	}
	
	return 0;
}
