
#include "config.h"
#include "dvblock.h"

#include "dvbpatterns.h"

ADVBPatterns::ADVBPatterns()
{
	const ADVBConfig& config = ADVBConfig::Get();

	priscale     = (int)config.GetConfigItem("priscale",    "2");
	repeatsscale = (int)config.GetConfigItem("repeatscale", "-1");
	urgentscale  = (int)config.GetConfigItem("urgentscale", "0");
}

ADVBPatterns& ADVBPatterns::Get()
{
	static ADVBPatterns _pattern;
	return _pattern;
}

bool ADVBPatterns::UpdatePatternInFile(const AString& filename, const AString& pattern, const AString& newpattern)
{
	const ADVBConfig& config = ADVBConfig::Get();
	AString filename1 = filename + ".new";
	AStdFile ifp, ofp;
	bool done = false;

	if (ifp.open(filename)) {
		if (ofp.open(filename1, "w")) {
			AString line;

			while (line.ReadLn(ifp) >= 0) {
				if (line == pattern) {
					if (newpattern.Valid()) ofp.printf("%s\n", newpattern.str());
					done = true;

					if (newpattern.Valid()) {
						config.logit("Changed pattern '%s' to '%s' in file '%s'", pattern.str(), newpattern.str(), filename.str());
					}
					else {
						config.logit("Deleted pattern '%s' from file '%s'", pattern.str(), filename.str());
					}
				}
				else ofp.printf("%s\n", line.str());
			}
			ofp.close();
		}

		ifp.close();

		if (done) {
			remove(filename);
			rename(filename1, filename);
		}
		else remove(filename1);
	}

	return done;
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

		if (!done) {
			if (fp.open(filename, "a")) {
				fp.printf("%s\n", pattern.str());
				fp.close();

				config.logit("Add pattern '%s' to file '%s'", pattern.str(), filename.str());
			}
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
