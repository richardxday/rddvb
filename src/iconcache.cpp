
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "dvblock.h"

#include "iconcache.h"

ADVBIconCache::ADVBIconCache() : cache(&__DeleteEntry),
                                 changed(false)
{
    AStdFile fp;

    ADVBLock lock("iconcache");
    if (fp.open(ADVBConfig::Get().GetIconCacheFilename(), "r")) {
        AString line;
        uint_t n = 0;

        while (line.ReadLn(fp) >= 0) {
            AString type = line.Column(0);
            AString name = line.Column(1).DeQuotify().DeEscapify();
            AString icon = line.Column(2).DeQuotify().DeEscapify();

            SetIcon(type, name, icon);
            n++;
        }

        fp.close();

        //ADVBConfig::Get().logit("Read %u lines resulting in %u items in icon cache", n, cache.GetItems());

        changed = false;
    }
}

ADVBIconCache::~ADVBIconCache()
{
    if (changed) {
        AStdFile fp;

        ADVBLock lock("iconcache");
        if (fp.open(ADVBConfig::Get().GetIconCacheFilename(), "w")) {
            cache.Traverse(&__WriteEntry, &fp);
            fp.close();
            //ADVBConfig::Get().logit("Wrote %u items from icon cache", cache.GetItems());
        }
    }
}

ADVBIconCache& ADVBIconCache::Get()
{
    static ADVBIconCache cache;
    return cache;
}

bool ADVBIconCache::__WriteEntry(const AString& key, uptr_t item, void *context)
{
    const ENTRY *entry = (const ENTRY *)item;
    AStdFile& fp = *(AStdFile *)context;

    (void)key;

    fp.printf("%s,\"%s\",\"%s\"\n", entry->type.str(), entry->name.Escapify().str(), entry->icon.Escapify().str());

    return true;
}

bool ADVBIconCache::EntryValid(const AString& type, const AString& name) const
{
    return (GetEntry(type, name) != NULL);
}

AString ADVBIconCache::GetIcon(const AString& type, const AString& name) const
{
    const ENTRY *entry;
    AString icon;

    if ((entry = GetEntry(type, name)) != NULL) {
        icon = entry->icon;
    }

    return icon;
}

void ADVBIconCache::SetIcon(const AString& type, const AString& name, const AString& icon)
{
    ENTRY *entry;

    if ((entry = GetWritableEntry(type, name)) != NULL) {
        changed |= (icon != entry->icon);
        entry->icon = icon;
    }
}

const ADVBIconCache::ENTRY *ADVBIconCache::GetEntry(const AString& type, const AString& name) const
{
    return (const ENTRY *)cache.Read(GetKey(type, name));
}

ADVBIconCache::ENTRY *ADVBIconCache::GetWritableEntry(const AString& type, const AString& name, bool create)
{
    ENTRY *entry;

    if (((entry = (ENTRY *)cache.Read(GetKey(type, name))) == NULL) && create && ((entry = new ENTRY) != NULL)) {
        entry->type = type;
        entry->name = name;

        cache.Insert(GetKey(type, name), (uptr_t)entry);

        changed = true;
    }

    return entry;
}
