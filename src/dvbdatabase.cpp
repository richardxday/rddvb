
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
	
	if (database) delete database;
}

bool ADVBDatabase::Open()
{
	if (database) {
		const ADVBConfig& config = ADVBConfig::Get();
		AString host     = config.GetConfigItem("dbhost", "localhost");
		AString username = config.GetConfigItem("dbuser", "richard");
		AString password = config.GetConfigItem("dbpassword", "arsebark");
		AString dbname   = config.GetConfigItem("dbname", "dvb");

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
						}
					};
					uint_t i;

					for (i = 0; i < NUMBEROF(tables); i++) {
						if (!database->CreateTable(tables[i].name, tables[i].columns)) {
							
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
	if (database) database->Close();
}

bool ADVBDatabase::AddProg(const ADVBProg& prog)
{
	bool success = false;

	if (database && prog.Valid()) {
		const ADVBChannelList& channellist = ADVBChannelList::Get();
		const ADVBChannelList::CHANNEL *channel;
		AString channelname = prog.GetChannel();

		if ((channel = channellist.GetChannelByName(channelname)) == NULL) {
			channelname = prog.GetDVBChannel();
			channel     = channellist.GetChannelByName(channelname);
		}

		if (channel) {

		}

	}

	return success;
}
