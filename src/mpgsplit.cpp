
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <vector>

#include <rdlib/StdFile.h>
#include <rdlib/Recurse.h>

double CalcTime(const char *str)
{
	uint_t hrs = 0, mins = 0, secs = 0, ms = 0;
	double t = 0.0;
	
	if (sscanf(str, "%u:%u:%u.%u", &hrs, &mins, &secs, &ms) >= 3) {
		uint64_t _t = (uint64_t)(hrs * 3600 + mins * 60 + secs) * 1000UL + ms;

		t = (double)_t / 1000.0;

		//printf("'%s' -> %0.3lf\n", str, t);
	}

	return t;
}

AString GetParentheses(const AString& line, int p = 0)
{
	int p1, p2;
	
	if (((p1 = line.Pos("(", p)) >= 0) && ((p2 = line.Pos(")", p1)) >= 0)) {
		p1++;
		return line.Mid(p1, p2 - p1);
	}

	return "";
}

typedef struct {
	AString aspect;
	double  start, length;
} SPLIT;

int main(int argc, char *argv[])
{
	AStdFile fp;
	
	if (argc < 3) {
		fprintf(stderr, "Usage: mpgsplit <src-file> <dst-file> [<aspect>...]\n");
		exit(10);
	}

	AString src      = argv[1];
	AString dst      = argv[2];
	AString basename = src.Prefix();
	AString remuxsrc = basename + "_Remuxed.mpg";
	AString logfile  = basename + "_log.txt";
	std::map<AString,bool> desired_aspects;
	int     rc = 0, arg, stage = 1;

	for (arg = 3; arg < argc; arg++) desired_aspects[AString(argv[arg])] = true;

	if (!AStdFile::exists(AString("%s.m2v").Arg(basename)) ||
		!AStdFile::exists(AString("%s.mp2").Arg(basename)) ||
		!AStdFile::exists(logfile)) {
		AString cmd;
			
		cmd.printf("projectx -ini /home/richard/X.ini \"%s\"", src.str());

		printf("--------------------------------------------------------------------------------\n");
		printf("Executing: '%s'\n", cmd.str());
		if (system(cmd) != 0) {
			printf("**** Failed\n");
			rc = -stage;
		}
		remove(basename + ".sup");
		remove(basename + ".sup.IFO");
		printf("--------------------------------------------------------------------------------\n");
	}

	stage++;
	
	std::vector<SPLIT> splits;
	std::map<AString,double> lengths;
	AString bestaspect;
	
	if (!rc) {
		if (fp.open(logfile)) {
			static const AString formatmarker = "new format in next leading sequenceheader detected";
			static const AString videomarker  = "video basics";
			static const AString lengthmarker = "video length:";
			AString line;
			AString aspect = "16:9";
			double  t1     = 0.0;
			double  t2     = 0.0;
			double  totallen = 0.0;

			printf("--------------------------------------------------------------------------------\n");
			printf("Analysing logfile:\n\n");
			
			while (line.ReadLn(fp) >= 0) {
				int p;
			
				if ((p = line.PosNoCase(videomarker)) >= 0) {
					aspect = GetParentheses(line, p);
				}

				if ((p = line.PosNoCase(formatmarker)) >= 0) {
					t2 = CalcTime(GetParentheses(line, p));

					if (t2 > t1) {
						SPLIT split = {aspect, t1, t2 - t1};

						printf("%-6s @ %0.3lf for %0.3lfs\n", split.aspect.str(), split.start, split.length);

						splits.push_back(split);

						lengths[aspect] += split.length;
						if (bestaspect.Empty() || (lengths[aspect] > lengths[bestaspect])) bestaspect = aspect;
						
						t1 = t2;
					}
				}

				if ((p = line.PosNoCase(lengthmarker)) >= 0) {
					if ((p = line.Pos(" @ ", p)) >= 0) {
						p += 3;
						totallen = CalcTime(line.str() + p);
						printf("Total length %0.3lfs\n", totallen);
					}
				}
			}

			{
				SPLIT split = {aspect, t1, 0.0};
				splits.push_back(split);

				if (totallen > 0.0) lengths[aspect] += totallen - t1;
				if (bestaspect.Empty() || (lengths[aspect] > lengths[bestaspect])) bestaspect = aspect;
			}

			fp.close();

			if (bestaspect.Valid()) {
				printf("Best aspect is %s with %0.3lfs\n", bestaspect.str(), lengths[bestaspect]);
				desired_aspects[bestaspect] = true;
			}
		}
		else {
			fprintf(stderr, "Failed to open log file '%s'\n", logfile.str());
			rc = -stage;
		}
	}

	if (!rc) {
		if (splits.size() == 1) {
			AString cmd;

			printf("No need to split file\n");

			cmd.printf("avconv -i \"%s.m2v\" -i \"%s.mp2\" -aspect %s -acodec mp3 -vcodec copy -filter:v yadif -v warning -y \"%s\"", basename.str(), basename.str(), bestaspect.str(), dst.str());

			printf("--------------------------------------------------------------------------------\n");
			printf("Executing: '%s'\n", cmd.str());
			if (system(cmd) != 0) {
				printf("**** Failed\n");
				printf("--------------------------------------------------------------------------------\n");
				rc = -stage;
			}
		}
		else {
			printf("Splitting file...\n");
			
			if (!AStdFile::exists(remuxsrc)) {
				AString cmd;
			
				cmd.printf("avconv -fflags +genpts -i \"%s.m2v\" -i \"%s.mp2\" -acodec copy -vcodec copy -v warning -f mpegts \"%s\"", basename.str(), basename.str(), remuxsrc.str());

				printf("--------------------------------------------------------------------------------\n");
				printf("Executing: '%s'\n", cmd.str());
				if (system(cmd) != 0) {
					printf("**** Failed\n");
					rc = -stage;
				}
				printf("--------------------------------------------------------------------------------\n");
			}

			stage++;

			std::map<AString,bool>::iterator it;
			uint_t outputindex = 0;
			for (it = desired_aspects.begin(); (it != desired_aspects.end()) && !rc; ++it, outputindex++) {
				std::vector<AString> files;
				AString aspect = it->first;
				uint_t i;
			
				for (i = 0; i < splits.size(); i++) {
					const SPLIT& split = splits[i];
					
					if (split.aspect == aspect) {
						AString cmd;
						AString outfile;
					
						outfile.printf("%s-%s-%u.mpg", basename.str(), aspect.SearchAndReplace(":", "_").str(), i);

						if (!AStdFile::exists(outfile)) {
							cmd.printf("avconv -fflags +genpts -i \"%s\" -ss %0.3lf", remuxsrc.str(), split.start);
							if (split.length > 0.0) cmd.printf(" -t %0.3lf", split.length);
							cmd.printf(" -acodec copy -vcodec copy -v warning -y -f mpegts \"%s\"", outfile.str());
				
							printf("--------------------------------------------------------------------------------\n");
							printf("Executing: '%s'\n", cmd.str());
							if (system(cmd) != 0) {
								printf("**** Failed\n");
								printf("--------------------------------------------------------------------------------\n");
								rc = -stage;
								break;
							}
							printf("--------------------------------------------------------------------------------\n");
						}

						stage++;
	
						files.push_back(outfile);
					}
				}

				{
					AString cmd;
					AString inputfiles;

					for (i = 0; i < files.size(); i++) {
						if (inputfiles.Empty()) inputfiles.printf("concat:%s", files[i].str());
						else					inputfiles.printf("|%s",       files[i].str());
					}
					
					cmd.printf("avconv -i \"%s\" -aspect %s -acodec mp3 -vcodec copy -filter:v yadif -v warning -y \"%s%s.%s\"", inputfiles.str(), aspect.str(), dst.Prefix().str(), outputindex ? AString(".%u").Arg(outputindex).str() : "", dst.Suffix().str());

					printf("--------------------------------------------------------------------------------\n");
					printf("Executing: '%s'\n", cmd.str());
					if (system(cmd) != 0) {
						printf("**** Failed\n");
						printf("--------------------------------------------------------------------------------\n");
						rc = -stage;
						break;
					}
					else {
						printf("--------------------------------------------------------------------------------\n");

						for (i = 0; i < files.size(); i++) {
							remove(files[i]);
						}
					}

					stage++;
				}
			}
		}
	}

	if (!rc) {
		FILE_INFO info;
	  
		if (GetFileInfo(dst.PathPart().CatPath("subs"), &info)) {
			AString cmd;

			cmd.printf("mv \"%s.sup\".* \"%s\"", basename.str(), dst.PathPart().CatPath("subs").str());
			if (system(cmd) != 0) printf("Failed to move .sup files\n");
		}
		else {
			remove(basename + ".sup.idx");
			remove(basename + ".sup.sub");
		}
		
		remove(basename + ".m2v");
		remove(basename + ".mp2");
		remove(basename + "_Remuxed.mpg");
		remove(logfile);
	}
	
	return rc;
}
