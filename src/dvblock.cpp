
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>

#include "dvblock.h"
#include "config.h"

AHash ADVBLock::lockhash(&ADVBLock::__DeleteLock);

ADVBLock::ADVBLock(const AString& iname) : name(iname)
{
    GetLock();
}

ADVBLock::~ADVBLock()
{
    ReleaseLock();
}

void ADVBLock::__DeleteLock(uptr_t item, void *context)
{
    auto *lock = (lock_t *)item;

    UNUSED(context);

    if (lock->refcount > 0) {
        close(lock->fd);
        lock->fd = -1;

        remove(GetFilename(lock));
    }

    delete lock;
}

AString ADVBLock::GetFilename(const lock_t *lock)
{
    return ADVBConfig::Get().GetTempDir().CatPath(lock->filename);
}

bool ADVBLock::GetLock(uint_t n)
{
    const auto& config = ADVBConfig::Get();
    auto *lock = (lock_t *)lockhash.Read(name);
    bool success = false;

    (void)config;

    if (lock == NULL) {
        if ((lock = new lock_t) != NULL) {
            lock->filename.printf("lockfile_%s.lock", name.str());
            lock->fd = -1;
            lock->refcount = 0;

            lockhash.Insert(name, (uptr_t)lock);
        }
    }

    if (lock != NULL) {
        if (lock->refcount == 0) {
            auto lockfile = GetFilename(lock);

            if ((lock->fd = open(lockfile, O_CREAT | O_RDWR, (S_IRUSR | S_IWUSR))) >= 0) {
                if (flock(lock->fd, LOCK_EX) >= 0) {
                    //config.logit("Acquired lock '%s' (filename '%s')\n", name.str(), lockfile.str());
                    success = true;
                }
                else config.logit("Failed to lock file: %s\n", strerror(errno));
            }
            else config.logit("Failed to create lockfile '%s' (%s)!  All Hell could break loose!!\n", lockfile.str(), strerror(errno));
        }
        else success = true;

        if (success) lock->refcount += n;
    }
    else config.logit("Failed to create lock '%s'!\n", name.str());

    return success;
}

void ADVBLock::ReleaseLock(uint_t n)
{
    const auto& config = ADVBConfig::Get();
    auto *lock = (lock_t *)lockhash.Read(name);

    (void)config;

    if (lock != NULL) {
        lock->refcount = SUBZ(lock->refcount, n);

        if (lock->refcount == 0) {
            auto lockfile = GetFilename(lock);

            close(lock->fd);
            lock->fd = -1;

            //config.logit("Released lock '%s' (filename '%s')\n", name.str(), lockfile.str());

            lockhash.Remove(name);
        }
    }
}
