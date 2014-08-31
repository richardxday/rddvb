
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

AHash ADVBLock::lockhash(10, &ADVBLock::__DeleteLock);

ADVBLock::ADVBLock(const AString& iname) : name(iname)
{
	locked = GetLock();
}

ADVBLock::~ADVBLock()
{
	ReleaseLock();
}

void ADVBLock::__DeleteLock(uptr_t item, void *context)
{
	LOCK *lock = (LOCK *)item;

	UNUSED(context);

	if (lock->refcount) {
		close(lock->fd);
		lock->fd = -1;

		remove(GetFilename(lock));
	}

	delete lock;
}

AString ADVBLock::GetFilename(const LOCK *lock)
{
	return ADVBConfig::Get().GetTempDir().CatPath(lock->filename);
}

bool ADVBLock::GetLock(uint_t n)
{
	LOCK *lock = (LOCK *)lockhash.Read(name);
	bool success = false;

	if (!lock) {
		if ((lock = new LOCK) != NULL) {
			lock->filename.printf("lockfile_%s.lock", name.str());
			lock->fd = -1;
			lock->refcount = 0;

			lockhash.Insert(name, (uptr_t)lock);
		}
	}

	if (lock) {
		if (!lock->refcount) {
			AString lockfile = GetFilename(lock);

			if ((lock->fd = open(lockfile, O_CREAT | O_RDWR, (S_IRUSR | S_IWUSR))) >= 0) {
				if (flock(lock->fd, LOCK_EX) >= 0) {
					//debug("Acquired lock '%s' (filename '%s')\n", name.str(), lockfile.str());
					lock->refcount += n;
					success = true;
				}
				else debug("Failed to lock file: %s\n", strerror(errno));
			}
			else debug("Failed to create lockfile '%s' (%s)!  All Hell could break loose!!\n", lockfile.str(), strerror(errno));
		}
	}
	else debug("Failed to create lock '%s'!\n", name.str());

	return success;
}

void ADVBLock::ReleaseLock(uint_t n)
{
	LOCK *lock = (LOCK *)lockhash.Read(name);

	if (locked && lock) {
		lock->refcount = SUBZ(lock->refcount, n);

		if (!lock->refcount) {
			AString lockfile = GetFilename(lock);

			close(lock->fd);
			lock->fd = -1;

			remove(lockfile);

			//debug("Released lock and removed '%s' (filename '%s')\n", name.str(), lockfile.str());
		}
	}
}
