#ifndef __ICON_CACHE__
#define __ICON_CACHE__

#include <rdlib/Hash.h>

class ADVBIconCache {
public:
	~ADVBIconCache();

	static ADVBIconCache& Get();

	bool    EntryValid(const AString& type, const AString& name) const;
	AString GetIcon(const AString& type, const AString& name) const;
	void    SetIcon(const AString& type, const AString& name, const AString& icon);

protected:
	ADVBIconCache();

	typedef struct {
		AString type;
		AString name;
		AString icon;
	} ENTRY;

	static void __DeleteEntry(uptr_t item, void *context) {
		(void)context;
		delete (ENTRY *)item;
	}

	AString GetKey(const AString& type, const AString& name) const {return type + ":" + name;}

	const ENTRY *GetEntry(const AString& type, const AString& name) const;
	ENTRY *GetWritableEntry(const AString& type, const AString& name, bool create = true);

	static bool __WriteEntry(const AString& key, uptr_t item, void *context);

protected:
	AHash cache;
	bool  changed;
};

#endif
