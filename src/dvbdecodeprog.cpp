
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

#include "config.h"
#include "dvbprog.h"
#include "dvbmisc.h"
#include "proglist.h"

int main(int argc, char *argv[])
{
    const ADVBConfig&   config = ADVBConfig::Get(true);
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
    ADVBProg prog;  // ensure ADVBProg initialisation takes place

    doc.SetObject();

    if (argc > 1) {
        if (prog.Base64Decode(argv[1])) {
            prog.ExportToJSON(doc, doc);

            const ADVBProg::EPISODE& episode = prog.GetEpisode();
            if (episode.valid) {
                ADVBProgList reclist, list;
                AString      pattern, errors;

                pattern.printf("title=\"%s\" available", prog.GetTitle());

                reclist.ReadFromFile(config.GetRecordedFile());
                reclist.FindProgrammes(list, pattern, errors);

                list.Sort(&ADVBProgList::CompareEpisode);

                int p;
                if ((p = list.FindUUIDIndex(prog)) >= 0) {
                    if (p > 0) {
                        rapidjson::Value obj;
                        const ADVBProg& prog1 = list.GetProg(p - 1);

                        obj.SetObject();

                        obj.AddMember("prog", rapidjson::Value(prog1.Base64Encode().str(), allocator), allocator);
                        obj.AddMember("desc", rapidjson::Value(prog1.GetTitleAndSubtitle().str(), allocator), allocator);

                        doc.AddMember("previous", obj, allocator);
                    }
                    if (p < (int)(list.Count() - 1)) {
                        rapidjson::Value obj;
                        const ADVBProg& prog1 = list.GetProg(p + 1);

                        obj.SetObject();

                        obj.AddMember("prog", rapidjson::Value(prog1.Base64Encode().str(), allocator), allocator);
                        obj.AddMember("desc", rapidjson::Value(prog1.GetTitleAndSubtitle().str(), allocator), allocator);

                        doc.AddMember("next", obj, allocator);
                    }
                }
            }
        }
        else {
            doc.AddMember("error", "Unable to decode Base64 programme", allocator);
        }

        rapidjson::PrettyWriter<AStdData> writer(*Stdout);
        doc.Accept(writer);
    }
    else {
        printf("Usage: dvbdecodeprog <base64-programme>\n");
    }

    return 0;
}
