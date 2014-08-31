
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "episodehandler.h"

ADVBEpisodeHandler::ADVBEpisodeHandler() : dayoffsethash(200),
										   episodehash(200),
										   changed(false)
{
	AStdFile fp;
	AString  filename = ADVBConfig::Get().GetEpisodesFile();

	if (fp.open(filename)) {
		AString line;

		while (line.ReadLn(fp) >= 0) {
			dayoffsethash.Insert(line.Line(0, "=", 0), (uptr_t)line.Line(1, "=", 0));
		}

		fp.close();
	}
}

ADVBEpisodeHandler::~ADVBEpisodeHandler()
{
	if (changed) {
		AStdFile fp;
		AString  filename = ADVBConfig::Get().GetEpisodesFile();

		if (fp.open(filename, "w")) {
			dayoffsethash.Traverse(&__WriteString, &fp);
			fp.close();
		}
	}
}

bool ADVBEpisodeHandler::__WriteString(const char *key, uptr_t value, void *context)
{
	AStdData *fp = (AStdData *)context;

	fp->printf("%s=%s\n", key, NUMSTR("", value));

	return true;
}

uint32_t ADVBEpisodeHandler::GetDayOffset(const AString& key) const
{
	return dayoffsethash.Read(key);
}

void ADVBEpisodeHandler::SetDayOffset(const AString& key, uint32_t value)
{
	dayoffsethash.Insert(key, value);
	changed = true;
}

uint32_t ADVBEpisodeHandler::GetEpisode(const AString& key) const
{
	return episodehash.Read(key);
}

void ADVBEpisodeHandler::SetEpisode(const AString& key, uint32_t value)
{
	episodehash.Insert(key, value);
	changed = true;
}

void ADVBEpisodeHandler::AssignEpisode(ADVBProg& prog, bool ignorerepeats)
{
	if (!prog.IsFilm() && (CompareNoCase(prog.GetTitle(), "Close") != 0)) {
		if (!prog.GetAssignedEpisode()) {
			AString key = prog.GetTitle();
			uint32_t  epn;
			bool    valid = false;

			key.printf("(%s)", AString(prog.GetChannel()).SearchAndReplace("+1", "").str());

			if (ignorerepeats || !prog.IsRepeatOrPlus1()) {
				epn = prog.GetStartDT().GetDays() - GetDayOffset(key);

				if (!dayoffsethash.Exists(key)) {
					//debug("Setting day offset for '%s' at %s\n", key.str(), NUMSTR("", epn));
					SetDayOffset(key, epn);
					epn = 0;
				}
		
				SetEpisode(key, epn);
				valid = true;
			}
			else if (episodehash.Exists(key)) {
				epn = GetEpisode(key);
				valid = true;
			}

			if (valid) {
				prog.SetAssignedEpisode(epn + 1);
			}
		}
	}
}


