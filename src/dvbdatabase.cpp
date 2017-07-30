
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rdlib/PostgresDatabase.h>

#include "dvbdatabase.h"

ADVBDatabase::ADVBDatabase() : database(new PostgresDatabase())
{
	
}

ADVBDatabase::~ADVBDatabase()
{
	if (database) delete database;
}


