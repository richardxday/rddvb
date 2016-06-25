
all: default-build

MAKEFILEDIR = /usr/local/share/rdlib-0.1/makefiles

include $(MAKEFILEDIR)/makefile.init

LIBRARY := rddvb

LIBRARY_VERSION_MAJOR := 0
LIBRARY_VERSION_MINOR := 1
LIBRARY_VERSION_RELEASE := 0
LIBRARY_VERSION_BUILD := 0
LIBRARY_DESCRIPTION := DVB auto-scheduler

OBJECTS :=										\
	channellist.o								\
	config.o									\
	dvblock.o									\
	dvbmisc.o									\
	dvbpatterns.o								\
	dvbprog.o									\
	episodehandler.o							\
	findcards.o									\
	iconcache.o									\
	proglist.o

HEADERS :=										\
	channellist.h								\
	config.h									\
	dvblock.h									\
	dvbmisc.h									\
	dvbpatterns.h								\
	dvbprog.h									\
	episodehandler.h							\
	findcards.h									\
	iconcache.h									\
	proglist.h

EXTRA_CFLAGS += $(shell pkg-config --cflags rdlib-0.1)
EXTRA_CFLAGS += $(shell pkg-config --cflags jsoncpp)
EXTRA_LIBS   += $(shell pkg-config --libs rdlib-0.1)
EXTRA_LIBS   += $(shell pkg-config --libs jsoncpp)

include $(MAKEFILEDIR)/makefile.prebuild

EXTRA_CFLAGS += -DRDDVB_SHARE_DIR=\"$(INSTALLSHAREDST)\"
EXTRA_CXXFLAGS += -std=c++11

include $(MAKEFILEDIR)/makefile.lib

LOCAL_SHARE_FILES := $(shell find share/*)
INSTALLEDSHAREFILES += $(LOCAL_SHARE_FILES:share/%=$(INSTALLSHAREDST)/%)

DEBUG_LIBS := $(DEBUG_LIBDIR)/$(DEBUG_SHARED_LIBRARY)
RELEASE_LIBS := $(RELEASE_LIBDIR)/$(RELEASE_SHARED_LIBRARY)

HEADERS :=

APPLICATION := dvb
OBJECTS     := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION := dvbweb
OBJECTS     := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION := dvbfemon
OBJECTS     := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION := dvbdecodeprog
OBJECTS     := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

include $(MAKEFILEDIR)/makefile.post
