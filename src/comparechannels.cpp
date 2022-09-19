
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <vector>
#include <algorithm>

#include <rdlib/strsup.h>
#include <rdlib/StdFile.h>
#include <rdlib/Regex.h>
#include <rdlib/XMLDecode.h>
#include <rdlib/printtable.h>

#include "channellist.h"

typedef struct {
    uint_t  lcn;
    struct {
        AString id;
        AString name;
        std::vector<AString> displaynames;
    } xmltv;
    struct {
        AString name;
    } dvb;
    std::vector<AString> othernames;
} channel_t;

bool operator < (const channel_t& channel1, const channel_t& channel2)
{
    return ((channel1.lcn < channel2.lcn) ||
            ((channel1.lcn == channel2.lcn) && (channel1.xmltv.id < channel2.xmltv.id)));
}

int main(int argc, char *argv[])
{
    std::vector<channel_t> channels;

    if (argc < 3) {
        fprintf(stderr, "Usage: comparechannels <channels-xmltv-file> <channel-files> ...\n");
        exit(1);
    }

    {
        AStructuredNode root;

        if (DecodeXMLFromFile(root, argv[1])) {
            const auto number = ParseGlob("#[0-9]");

            const AStructuredNode *node;
            if (((node = root.FindChild("tv")) != NULL) &&
                ((node = node->GetChildren()) != NULL)) {
                while (node) {
                    if (node->Key == "channel") {
                        const AKeyValuePair *_id;
                        channel_t channel;

                        channel.lcn = 0;
                        channel.othernames.resize(argc - 2);

                        if ((_id = node->FindAttribute("id")) != NULL) {
                            channel.xmltv.id = _id->Value;
                        }

                        std::vector<AString>& displaynames = channel.xmltv.displaynames;
                        const AStructuredNode *node1 = node->GetChildren();
                        while (node1) {
                            if (node1->Key == "display-name") {
                                displaynames.push_back(node1->Value);

                                if (channel.xmltv.name.Empty()) {
                                    channel.xmltv.name = node1->Value;
                                }

                                if (MatchGlob(node1->Value, number)) {
                                    channel.lcn = (uint_t)node1->Value;
                                }
                            }

                            node1 = node1->Next();
                        }

                        channels.push_back(channel);
                    }

                    node = node->Next();
                }
            }
        }
        else {
            fprintf(stderr, "Failed to decoee XML file '%s'\n", argv[1]);
        }
    }

    {

        const ADVBChannelList& channellist = ADVBChannelList::Get();
        uint_t i;
        size_t j;

        for (i = 0; i < 1000; i++) {
            const ADVBChannelList::CHANNEL *chan;

            if ((chan = channellist.GetChannelByLCN(i)) != NULL) {
                for (j = 0; j < channels.size(); j++) {
                    channel_t& channel = channels[j];

                    if (channel.lcn == chan->dvb.lcn) {
                        channel.dvb.name = chan->dvb.channelname;
                        break;
                    }
                }

                if (j == channels.size()) {
                    channel_t channel;

                    channel.lcn = chan->dvb.lcn,
                    channel.othernames.resize(argc - 2);
                    channel.dvb.name = chan->dvb.channelname;

                    channels.push_back(channel);
                }
            }
        }
    }

    int i;
    for (i = 2; i < argc; i++) {
        AStdFile fp;

        if (fp.open(argv[i])) {
            std::map<AString, bool> channelsdone;
            const uint_t index = i - 2;
            uint_t round;
            bool done = false;

            for (round = 0; !done && (round < 3); round++) {
                AString line;

                fp.rewind();

                done = true;

                while (line.ReadLn(fp) >= 0) {
                    if (line.CountWords() > 1) {
                        uint_t  lcn  = line.Word(0);
                        AString name = line.Words(1);

                        if (channelsdone.find(name) == channelsdone.end()) {
                            size_t j;

                            for (j = 0; j < channels.size(); j++) {
                                channel_t& channel = channels[j];

                                if ((lcn == channel.lcn) &&
                                    channel.othernames[index].Empty() &&
                                    ((round > 0) ||
                                     (channel.xmltv.name.PosNoCase(name) >= 0) ||
                                     (name.PosNoCase(channel.xmltv.name) >= 0))) {
                                    channel.othernames[index] = name;
                                    channelsdone[name] = true;
                                    done = false;
                                    break;
                                }
                            }

                            if ((j == channels.size() && (round == 2))) {
                                channel_t channel;

                                channel.lcn = lcn,
                                channel.othernames.resize(argc - 2);
                                channel.othernames[index] = name;

                                channels.push_back(channel);
                                channelsdone[name] = true;
                                done = false;
                            }
                        }
                    }
                }
            }

            fp.close();
        }
    }

    std::sort(channels.begin(), channels.end());

    TABLE table;
    size_t j;

    table.headerscentred = false;

    {
        TABLEROW row;

        row.push_back("LCN");
        row.push_back("XMLTV ID");
        row.push_back("XMLTV Name");
        row.push_back("DVB Name");
        for (i = 2; i < argc; i++) {
            row.push_back(AString(argv[i]).FilePart().SearchAndReplace(".txt", "").SearchAndReplace("channels", "").Words(0));
        }

        table.rows.push_back(row);
    }

    for (j = 0; j < channels.size(); j++) {
        const channel_t& channel = channels[j];
        TABLEROW row;

        row.push_back(AString("%").Arg(channel.lcn));
        row.push_back(channel.xmltv.id);
        row.push_back(channel.xmltv.name);
        row.push_back(channel.dvb.name);

        size_t l;
        for (l = 0; l < channel.othernames.size(); l++) {
            row.push_back(channel.othernames[l]);
        }

        table.rows.push_back(row);
    }

    PrintTable(table);

    return 0;
}
