
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <vector>

#include <rdlib/StdFile.h>
#include <rdlib/Recurse.h>

uint64_t CalcTime(const char *str)
{
	uint_t hrs = 0, mins = 0, secs = 0, ms = 0;
	uint64_t t = 0;
	
	if (sscanf(str, "%u:%u:%u:%u", &hrs, &mins, &secs, &ms) >= 4) {
		t = (uint64_t)(hrs * 3600 + mins * 60 + secs) * 1000UL + ms;
	}
	else if (sscanf(str, "%u:%u:%u.%u", &hrs, &mins, &secs, &ms) >= 4) {
		t = (uint64_t)(hrs * 3600 + mins * 60 + secs) * 1000UL + ms;
	}

	return t;
}

AString GenTime(uint64_t t, const char *format = "%02u:%02u:%02u.%03u")
{
	AString  res;
	uint32_t sec = (uint32_t)(t / 1000);
	uint_t   ms  = (uint_t)(t % 1000);

	res.printf(format,
			   (uint_t)(sec / 3600),
			   (uint_t)((sec / 60) % 60),
			   (uint_t)(sec % 60),
			   ms);

	return res;
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

void CopyFile(AStdData& fp1, AStdData& fp2)
{
	static uint8_t buffer[65536];
	slong_t l;

	while ((l = fp1.readbytes(buffer, sizeof(buffer))) > 0) fp2.writebytes(buffer, l);
}

typedef struct {
	AString aspect;
	uint64_t  start, length;
} SPLIT;

void ConvertSubtitles(const AString& src, const AString& dst, const std::vector<SPLIT>& splits, const AString& aspect)
{
	FILE_INFO info;
	
	if (GetFileInfo(dst.PathPart().CatPath("subs"), &info)) {
		AString srcsubfile = src.Prefix() + ".sup.idx";
		AString dstsubfile = dst.PathPart().CatPath("subs", dst.FilePart().Prefix() + ".sup.idx");
		AStdFile fp1, fp2;
						
		if (fp1.open(srcsubfile)) {
			if (fp2.open(dstsubfile, "w")) {
				AString line;

				printf("Converting '%s' to '%s'\n", srcsubfile.str(), dstsubfile.str());
								
				while (line.ReadLn(fp1) >= 0) {
					if (line.Pos("timestamp:") == 0) {
						uint64_t t = CalcTime(line.str() + 11);
						uint64_t sub = 0;
						uint_t i;
						
						for (i = 0; i < splits.size(); i++) {
							const SPLIT& split = splits[i];
											
							if (split.aspect == aspect) {
								if ((t >= split.start) && (!split.length || (t < (split.start + split.length)))) {
									t -= sub;
													
									fp2.printf("timestamp: %s, filepos: %s\n",
											   GenTime(t, "%02u:%02u:%02u:%03u").str(),
											   line.Mid(34).str());
									break;
								}
							}
							else sub += split.length;
						}
					}
					else fp2.printf("%s\n", line.str());
				}

				fp2.close();
			}
			else printf("Failed to open sub file '%s' for writing\n", dstsubfile.str());
			
			fp1.close();

			srcsubfile = src.Prefix() + ".sup.sub";
			dstsubfile = dst.PathPart().CatPath("subs", dst.FilePart().Prefix() + ".sup.sub");
			if (fp1.open(srcsubfile, "rb")) {
				if (fp2.open(dstsubfile, "wb")) {
					printf("Copying '%s' to '%s'\n", srcsubfile.str(), dstsubfile.str());
								
					CopyFile(fp1, fp2);
								
					fp2.close();
				}
				else printf("Failed to open sub file '%s' for writing\n", dstsubfile.str());
			
				fp1.close();
			}
			else printf("Failed to open sub file '%s' for reading\n", srcsubfile.str());
		}
		else printf("Failed to open sub file '%s' for reading\n", srcsubfile.str());
	}
	else printf("'subs' directory doesn't exist\n");
}

int main(int argc, char *argv[])
{
	AStdFile fp;
	bool cleanup = true;
	
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
	int     rc = 0, arg;

	for (arg = 3; arg < argc; arg++) {
		if (argv[arg][0] == '-') {
			if (stricmp(argv[arg], "-keep") == 0) cleanup = false;
		}
		else desired_aspects[AString(argv[arg])] = true;
	}

	if (!AStdFile::exists(AString("%s.m2v").Arg(basename)) ||
		!AStdFile::exists(AString("%s.mp2").Arg(basename)) ||
		!AStdFile::exists(logfile)) {
		AString cmd;
			
		cmd.printf("projectx -ini /home/richard/X.ini \"%s\"", src.str());

		printf("Executing: '%s'\n", cmd.str());
		if (system(cmd) != 0) {
			printf("**** Failed\n");
			rc = -__LINE__;
		}
		remove(basename + ".sup");
		remove(basename + ".sup.IFO");
	}

	std::vector<SPLIT> splits;
	std::map<AString,uint64_t> lengths;
	AString bestaspect;
	
	if (!rc) {
		if (fp.open(logfile)) {
			static const AString formatmarker = "new format in next leading sequenceheader detected";
			static const AString videomarker  = "video basics";
			static const AString lengthmarker = "video length:";
			AString  line;
			AString  aspect = "16:9";
			uint64_t t1     = 0;
			uint64_t t2     = 0;
			uint64_t totallen = 0;

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

						printf("%-6s @ %s for %s\n", split.aspect.str(), GenTime(split.start).str(), GenTime(split.length).str());

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
						printf("Total length %s\n", GenTime(totallen).str());
					}
				}
			}

			{
				SPLIT split = {aspect, t1, 0};
				splits.push_back(split);

				if (totallen > 0) lengths[aspect] += totallen - t1;
				if (bestaspect.Empty() || (lengths[aspect] > lengths[bestaspect])) bestaspect = aspect;
			}

			fp.close();

			if (bestaspect.Valid()) {
				printf("Best aspect is %s with %s\n", bestaspect.str(), GenTime(lengths[bestaspect]).str());
				desired_aspects[bestaspect] = true;
			}
		}
		else {
			fprintf(stderr, "Failed to open log file '%s'\n", logfile.str());
			rc = -__LINE__;
		}
	}

	if (!rc) {		
		if (splits.size() == 1) {
			AString cmd;

			printf("No need to split file\n");

			cmd.printf("avconv -i \"%s.m2v\" -i \"%s.mp2\" -aspect %s -acodec mp3 -vcodec copy -filter:v yadif -v warning -y \"%s\"", basename.str(), basename.str(), bestaspect.str(), dst.str());

			printf("Executing: '%s'\n", cmd.str());
			if (system(cmd) != 0) {
				printf("**** Failed\n");
				rc = -__LINE__;
			}

			if (!rc) ConvertSubtitles(src, dst, splits, bestaspect);
		}
		else {
			printf("Splitting file...\n");
			
			if (!AStdFile::exists(remuxsrc)) {
				AString cmd;
			
				cmd.printf("avconv -fflags +genpts -i \"%s.m2v\" -i \"%s.mp2\" -acodec copy -vcodec copy -v warning -f mpegts \"%s\"", basename.str(), basename.str(), remuxsrc.str());

				printf("Executing: '%s'\n", cmd.str());
				if (system(cmd) != 0) {
					printf("**** Failed\n");
					rc = -__LINE__;
				}
			}

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
							cmd.printf("avconv -fflags +genpts -i \"%s\" -ss %s", remuxsrc.str(), GenTime(split.start).str());
							if (split.length > 0) cmd.printf(" -t %s", GenTime(split.length).str());
							cmd.printf(" -acodec copy -vcodec copy -v warning -y -f mpegts \"%s\"", outfile.str());
				
							printf("Executing: '%s'\n", cmd.str());
							if (system(cmd) != 0) {
								printf("**** Failed\n");
								rc = -__LINE__;
								break;
							}
						}

						files.push_back(outfile);
					}
				}

				AString outputfile;
				outputfile.printf("%s%s.%s", dst.Prefix().str(), outputindex ? AString(".%u").Arg(outputindex).str() : "", dst.Suffix().str());
				
				if (!AStdFile::exists(outputfile)) {
					AStdFile ofp;
					AString  concatfile;

					concatfile = outputfile.Prefix() + "_Concat.mpg";

					if (ofp.open(concatfile, "wb")) {
						for (i = 0; i < files.size(); i++) {
							AStdFile ifp;

							if (ifp.open(files[i], "rb")) {
								printf("Adding '%s'...\n", files[i].str());
								CopyFile(ifp, ofp);
								ifp.close();
							}
							else {
								printf("Failed to open '%s' for reading\n", files[i].str());
								rc = -__LINE__;
								break;
							}
						}

						ofp.close();
					}
					else {
						printf("Failed to open '%s' for writing\n", concatfile.str());
						rc = -__LINE__;
					}

					if (!rc) {
						AString cmd;

						cmd.printf("avconv -i \"%s\" -aspect %s -acodec mp3 -vcodec copy -filter:v yadif -v warning -y \"%s\"", concatfile.str(), aspect.str(), outputfile.str());
						
						printf("Executing: '%s'\n", cmd.str());
						if (system(cmd) != 0) {
							printf("**** Failed\n");
							rc = -__LINE__;
							break;
						}
					}

					if (!rc) ConvertSubtitles(src, outputfile, splits, aspect);
												   
					if (cleanup) remove(concatfile);
				}

				if (cleanup) {
					for (i = 0; i < files.size(); i++) {
						remove(files[i]);
					}
				}
			}
		}
	}

	if (!rc && cleanup) {
		remove(basename + ".m2v");
		remove(basename + ".mp2");

		AList subfiles;
		CollectFiles(src.PathPart(), src.FilePart().Prefix() + ".sup.*", RECURSE_ALL_SUBDIRS, subfiles);

		const AString *file = AString::Cast(subfiles.First());
		while (file) {
			remove(*file);
			file = file->Next();
		}
		remove(remuxsrc);
		remove(logfile);
	}
	
	return rc;
}
