
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include <rdlib/StdFile.h>
#include <rdlib/strsup.h>

int main(int argc, char *argv[])
{
	if (argc < 6) {
		fprintf(stderr, "Usage: sigcmp [-n <n>] <file1> <file2> <frame1> <frame2> <nframes> [<len1> [<len2>]]\n");
		exit(1);
	}
	
	double fps = 25.0;
	uint_t n = 5;
	int arg = 1;

	if (stricmp(argv[arg], "-n") == 0) {
		n = (uint_t)AString(argv[++arg]);
		arg++;
	}

	std::vector<uint8_t> fdata1;
	std::vector<uint8_t> fdata2;

	AStdFile f;
	if (f.open(argv[arg], "rb")) {
		f.seek(0, SEEK_END);
		fdata1.resize(f.tell());
		f.rewind();

		if (f.readbytes(&fdata1[0], fdata1.size()) != fdata1.size()) {
			fprintf(stderr, "Failed to read %lu bytes from '%s'\n", (ulong_t)fdata1.size(), argv[arg]);
			fdata1.resize(0);
		}
		f.close();
	}
	else fprintf(stderr, "Failed to open file '%s' for reading\n", argv[arg]);

	arg++;

	if (fdata1.size()) {

		if (f.open(argv[arg], "rb")) {
			f.seek(0, SEEK_END);
			fdata2.resize(f.tell());
			f.rewind();

			if (f.readbytes(&fdata2[0], fdata2.size()) != fdata2.size()) {
				fprintf(stderr, "Failed to read %lu bytes from '%s'\n", (ulong_t)fdata2.size(), argv[arg]);
				fdata2.resize(0);
			}
			f.close();
		}
		else fprintf(stderr, "Failed to open file '%s' for reading\n", argv[arg]);
	}
	
	arg++;

	if (fdata1.size() && fdata2.size()) {
		const uint8_t *ptr1 = &fdata1[0];
		const uint8_t *ptr2 = &fdata2[0];
		uint_t frame1  = (uint_t)((double)AString(argv[arg++]) * fps);
		uint_t frame2  = (uint_t)((double)AString(argv[arg++]) * fps);
		uint_t nframes = (uint_t)((double)AString(argv[arg++]) * fps);
		uint_t len1    = 0;
		uint_t len2    = 0;

		if (arg < argc) len1 = (uint_t)((double)AString(argv[arg++]) * fps);
		if (arg < argc) len2 = (uint_t)((double)AString(argv[arg++]) * fps);

		len1 = MAX(len1, 1);
		len2 = MAX(len2, 1);

		uint_t framesize = n * n * 3, totalbytes = nframes * framesize, step = 5;

		len1 = MIN(len1, SUBZ((fdata1.size() - totalbytes) / framesize, frame1));
		len2 = MIN(len2, SUBZ((fdata2.size() - totalbytes) / framesize, frame2));

		uint_t pc = 0;
		uint_t i, j, k, n = 0, count = MAX(len1 / step, 1) * MAX(len2 / step, 1);
		for (i = 0; i < len1; i += step) {
			uint_t p1 = (frame1 + i) * framesize;
			
			for (j = 0; j < len2; j += step) {
				uint_t p2 = (frame2 + j) * framesize;

				slong_t sum = 0;
				for (k = 0; (k < totalbytes); k++) {
					int d = (int)ptr1[p1 + k] - (int)ptr2[p2 + k];
					sum -= abs(d);
				}

				printf("%0.6lf %0.6lf %0.14le\n",
					   (double)(frame1 + i) / fps,
					   (double)(frame2 + j) / fps,
					   (double)sum / (double)totalbytes);
				
				uint_t _pc = (100 * ++n) / count;
				if (_pc > pc) {
					pc = _pc;
					fprintf(stderr, "\rProcessing %u%%", pc);
					fflush(stderr);
				}
			}
		}
	}

	return 0;
}
