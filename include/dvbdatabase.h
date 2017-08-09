#ifndef __DVB_DATABASE__
#define __DVB_DATABASE__

#include <rdlib/Database.h>

#include "dvbprog.h"

class ADVBDatabase {
public:
	ADVBDatabase();
	virtual ~ADVBDatabase();

	virtual bool Open();
	virtual void Close();
	
	virtual bool IsOpen() const {return (database && database->IsOpen());};

	virtual bool AddProg(const ADVBProg& prog);
	
protected:
	Database *database;
};

#endif
