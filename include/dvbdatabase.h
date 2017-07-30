#ifndef __DVB_DATABASE__
#define __DVB_DATABASE__

#include <rdlib/Database.h>

#include "proglist.h"

class ADVBDatabase {
public:
	ADVBDatabase();
	virtual ~ADVBDatabase();

protected:
	Database *database;
};

#endif
