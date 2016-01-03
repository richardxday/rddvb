
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include <rdlib/StdFile.h>
#include <rdlib/strsup.h>

int main(int argc, char *argv[])
{
	AStdFile file, ofile;
	AStdData *ifp = Stdin;
	uint_t wid = 256, hgt = 256, n = 5;
	int i;

	if (argc < 3) {
		fprintf(stderr, "Usage: videosig [-s <w>x<h>] [-n <n>] [-if <input-file>] -of <output-file>\n");
		exit(1);
	}
	
	for (i = 1; i < argc; i++) {
		if (stricmp(argv[i], "-s") == 0) {
			uint_t w, h;
			if (sscanf(argv[++i], "%ux%u", &w, &h) == 2) {
				wid = w;
				hgt = h;
			}
			else fprintf(stderr, "Failed to decode widxhgt from '%s'\n", argv[i]);
		}
		else if (stricmp(argv[i], "-n")  == 0) n = (uint_t)AString(argv[++i]);
		else if (stricmp(argv[i], "-if") == 0) {
			file.close();
			if (file.open(argv[++i], "rb")) ifp = &file;
			else fprintf(stderr, "Failed to open file '%s' for reading\n", argv[i]);
		}
		else if (stricmp(argv[i], "-of") == 0) {
			ofile.close();
			if (!ofile.open(argv[++i], "wb")) fprintf(stderr, "Failed to open file '%s' for writing\n", argv[i]);
		}
	}

	uint_t nframes = 0;
	if (ifp && ofile.isopen() && wid && hgt && n) {
		std::vector<uint8_t> inbuf;
		std::vector<uint8_t> outbuf;
		
		inbuf.resize(wid * hgt * 3);
		outbuf.resize(n * n * 3);

		while (ifp->readbytes(&inbuf[0], inbuf.size()) == (slong_t)inbuf.size()) {
			uint8_t *p = &outbuf[0];
			uint_t i, j, div = 2 * (n + 1);

			for (i = 0; i < n; i++) {
				uint_t y = (((2 * i + 1) * hgt) / div) * hgt;
				
				for (j = 0; j < n; j++) {
					uint_t x = ((2 * j + 1) * wid) / div;

					memcpy(p, &inbuf[3 * (x + y)], 3);
					p += 3;
				}
			}
			
			ofile.writebytes(&outbuf[0], outbuf.size());

			nframes++;
		}
	}

	ofile.close();

	printf("Total %u frames processed\n", nframes);
	
	return 0;
}
