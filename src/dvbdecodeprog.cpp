
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "dvbprog.h"

int main(int argc, char *argv[])
{
	ADVBProg prog;	// ensure ADVBProg initialisation takes place

	if (argc > 1) {
		if (prog.Base64Decode(argv[1])) {
			printf("{%s}", prog.ExportToJSON().str());
		}
		else printf("{\"error\": \"Unable to decode Base64 programme\"}");
	}
	else printf("Usage: dvbdecodeprog <base64-programme>\n");
	
	return 0;
}
