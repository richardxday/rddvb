#ifndef __DVB_LOCK__
#define __DVB_LOCK__

#include <rdlib/Hash.h>
#include <rdlib/strsup.h>

class ADVBLock {
public:
    ADVBLock(const AString& iname = "default");
    ~ADVBLock();

    bool GetLock(uint_t n = 1);
    void ReleaseLock(uint_t n = 1);

protected:
    typedef struct {
        AString filename;
        int     fd;
        uint_t  refcount;
    } LOCK;

    static AString GetFilename(const LOCK *lock);

    static void __DeleteLock(uptr_t item, void *context);

protected:
    AString name;

    static AHash lockhash;
};

#endif
