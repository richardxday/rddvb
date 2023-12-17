
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rdlib/PostgresDatabase.h>

#include "dvbdatabase.h"
#include "channellist.h"
#include "config.h"

ADVBDatabase::ADVBDatabase() : database(new PostgresDatabase())
{
    Open();
}

ADVBDatabase::~ADVBDatabase()
{
    Close();

    if (database != NULL) delete database;
}

bool ADVBDatabase::Open()
{
    if (database != NULL) {
        const auto& config = ADVBConfig::Get();
        auto host     = config.GetConfigItem("dbhost", "localhost");
        auto username = config.GetConfigItem("dbuser", "richard");
        auto password = config.GetConfigItem("dbpassword", "arsebark");
        auto dbname   = config.GetConfigItem("dbname", "dvb");

        if (!database->Open(host, username, password, dbname)) {
            if (database->OpenAdmin(host)) {
                debug("Connected to DB server '%s' as admin\n", host.str());
                database->AddUser(username, password);
                database->CreateDatabase(dbname);
                database->GrantPrivileges(dbname, username);
                database->Close();

                if (database->Open(host, username, password, dbname)) {
                    static const struct {
                        const char *name;
                        const char *columns;
                    } tables[] = {
                        {
                            "Channels",
                            "ID id,"
                            "ChannelName string(100),"
                            "DVBName string(100)"
                        },
                        {
                            "Programmes",
                            "ID id64,"
                            "Title string(200),"
                            "Subtitle string(200),"
                            "Description string(1000),"
                            "ChannelID references Channels,"
                            "Start date,"
                            "Stop date,"
                            "Director string(100),"
                            "Category string(100),"
                            "Subcategories string(200),"
                            "Year,"
                            "EpisodeNum string(100),"
                            "EpisodeID string(100),"
                            "Actors string(200)"
                        },
                    };
                    uint_t i;

                    for (i = 0; i < NUMBEROF(tables); i++) {
                        if (!database->CreateTable(tables[i].name, tables[i].columns)) {
                            debug("Failed to create table '%s' in database\n", tables[i].name);
                        }
                    }
                }
            }
            else {
                // failed to connect
                debug("Failed to connect to DB server '%s' as admin\n", host.str());
            }
        }

        if (!database->IsOpen()) {
            debug("Failed to connect to DB server '%s' as user '%s'\n", host.str(), username.str());
        }
    }

    return IsOpen();
}

void ADVBDatabase::Close()
{
    if (database != NULL) database->Close();
}

bool ADVBDatabase::AddProg(const ADVBProg& prog)
{
    bool success = false;

    if ((database != NULL) && prog.Valid()) {
        //const ADVBChannelList& channellist = ADVBChannelList::Get();
        const ADVBChannelList::channel_t *channel = NULL;

        if (channel != NULL) {

        }

    }

    return success;
}
