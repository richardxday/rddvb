
all: default-build

MAKEFILEDIR := $(shell rdlib-config --makefiles)

LIBRARY := rddvb

LIBRARY_VERSION_MAJOR	:= 0
LIBRARY_VERSION_MINOR	:= 1
LIBRARY_VERSION_RELEASE := 0
LIBRARY_VERSION_BUILD	:= 0
LIBRARY_DESCRIPTION		:= DVB auto-scheduler

include $(MAKEFILEDIR)/makefile.init

OBJECTS :=										\
	channellist.o								\
	config.o									\
	dvbdatabase.o								\
	dvblock.o									\
	dvbmisc.o									\
	dvbpatterns.o								\
	dvbprog.o									\
	dvbstreams.o								\
	episodehandler.o							\
	findcards.o									\
	iconcache.o									\
	proglist.o

HEADERS :=										\
	channellist.h								\
	config.h									\
	dvbdatabase.h								\
	dvblock.h									\
	dvbmisc.h									\
	dvbpatterns.h								\
	dvbprog.h									\
	dvbstreams.h								\
	episodehandler.h							\
	findcards.h									\
	iconcache.h									\
	proglist.h

GLOBAL_CFLAGS		+= $(call pkgcflags,rdlib-0.1)
GLOBAL_CXXFLAGS		+= $(call pkgcxxflags,rdlib-0.1)
GLOBAL_LIBS			+= $(call pkglibs,rdlib-0.1)

GLOBAL_COMMON_FLAGS += $(call pkgcflags,jsoncpp)
GLOBAL_LIBS			+= $(call pkglibs,jsoncpp)

GLOBAL_COMMON_FLAGS += $(shell curl-config --cflags)
GLOBAL_LIBS			+= $(shell curl-config --libs) -lcrypto

include $(MAKEFILEDIR)/makefile.prebuild

EXTRA_COMMON_FLAGS += "-DRDDVB_ROOT_DIR=\"$(ROOTDIR)\""
EXTRA_COMMON_FLAGS += "-DRDDVB_SHARE_DIR=\"$(INSTALLSHAREDST)\""

EXTRA_CFLAGS   += -std=c99
EXTRA_CXXFLAGS += -std=c++11

include $(MAKEFILEDIR)/makefile.lib

LOCAL_SHARE_FILES := $(shell find share/* | sed -E "s/\.in$$//" | uniq)
INSTALLEDSHAREFILES += $(LOCAL_SHARE_FILES:share/%=$(INSTALLSHAREDST)/%)

DEBUG_LIBS	 += $(DEBUG_LIBDIR)/$(DEBUG_SHARED_LIBRARY)
RELEASE_LIBS += $(RELEASE_LIBDIR)/$(RELEASE_SHARED_LIBRARY)

HEADERS :=

APPLICATION := dvb
OBJECTS		:= $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION := dvbweb
OBJECTS		:= $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION := dvbfemon
OBJECTS		:= $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION := dvbdecodeprog
OBJECTS		:= $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

INSTALL_BINARIES := 0

DEFAULTCONFIG := share/default-config-values.txt

%: %.in
	@cat $< \
	| sed -E "s#@root@#$(ROOTDIR)#g" \
	| sed -E "s#@prefix@#$(PREFIX)#g" \
	| sed -E "s#@share@#$(INSTALLSHAREDST)#g" \
	>$@
	@chmod a+x $@

$(HEADERSSRC)/config.extract.h: $(HEADERSSRC)/config.h
	@echo "Generating $@"
	@grep -h -E '^[[:blank:]]+[A-Za-z0-9_]+[ \t]+.+// extractconfig' $< | sed -E 's/^[ \t]+[A-Za-z0-9_]+[ \t]+([A-Za-z0-9_]+)\(.+\/\/ extractconfig\((.*)\).*$$/(void)config.\1(\2);/' >$@

$(HEADERSSRC)/configlivevalues.def: $(HEADERSSRC)/config.h
	@echo "Generating $@"
	@grep -o -E "^[ \t]+AString Get[A-Za-z0-9]+\(\)" $< | sed -E "s/^[ \t]+AString[ \t]+//" | sed -E "s/\(\)//" | sed -E "s/Get(.+)$$/\1,\L\1/" | sed -E "s/^([A-Za-z0-9]+),([A-Za-z0-9]+)$$/CONFIG_GETVALUE(\"\2\", Get\1)/" >$@

default-build: $(HEADERSSRC)/configlivevalues.def

APPLICATION := extractconfig
OBJECTS		:= $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

ifdef DEBUG
$(DEBUG_OBJDIR)/$(APPLICATION)/$(APPLICATION).o: $(HEADERSSRC)/config.extract.h

EXTRACTCONFIG := $(DEBUG_BINDIR)/$(DEBUG_APPLICATION)$(APPLICATION_SUFFIX)
$(DEFAULTCONFIG): $(EXTRACTCONFIG)
	@echo "Generating $@"
	@LD_LIBRARY_PATH=$(DEBUG_LIBDIR) $(EXTRACTCONFIG) >$@
else
$(RELEASE_OBJDIR)/$(APPLICATION)/$(APPLICATION).o: $(HEADERSSRC)/config.extract.h

EXTRACTCONFIG := $(RELEASE_BINDIR)/$(RELEASE_APPLICATION)$(APPLICATION_SUFFIX)
$(DEFAULTCONFIG): $(EXTRACTCONFIG)
	@echo "Generating $@"
	@LD_LIBRARY_PATH="$(RELEASE_LIBDIR):$(INSTALLLIBDST)" $(EXTRACTCONFIG) >$@
endif

CLEANFILES	+= $(HEADERSSRC)/config.extract.h $(DEFAULTCONFIG)
CLEANFILES	+= $(HEADERSSRC)/configlivevalues.def

APPLICATION := comparechannels
OBJECTS		:= $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION := sdfetch
OBJECTS		:= $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

LOCAL_INSTALLED_SCRIPTS := $(shell find scripts -type f | sed -E "s/\.in$$//" | uniq)
GLOBAL_INSTALLED_SCRIPTS := $(LOCAL_INSTALLED_SCRIPTS:scripts/%=$(INSTALLBINDST)/%)
INSTALLEDBINARIES += $(GLOBAL_INSTALLED_SCRIPTS)
UNINSTALLFILES += $(GLOBAL_INSTALLED_SCRIPTS)

$(INSTALLBINDST)/%: scripts/%.in
	@cat $< \
	| sed -E "s#@root@#$(ROOTDIR)#g" \
	| sed -E "s#@prefix@#$(PREFIX)#g" \
	| sed -E "s#@share@#$(INSTALLSHAREDST)#g" \
	| $(SUDO) tee $@ >/dev/null
	@$(SUDO) chmod a+x $@

APACHESRC := share/apache
APACHEDST := $(ROOTDIR)etc/apache2
LOCAL_APACHE_FILES := $(shell find $(APACHESRC) -type f | sed -E "s/\.in$$//" | uniq)
GLOBAL_APACHE_FILES := $(LOCAL_APACHE_FILES:$(APACHESRC)/%=$(APACHEDST)/%)

INSTALLEDSHAREFILES += $(GLOBAL_APACHE_FILES)
UNINSTALLFILES += $(GLOBAL_APACHE_FILES)

$(APACHEDST)/%: $(APACHESRC)/%.in
	cat $< \
	| sed -E "s#@root@#$(ROOTDIR)#g" \
	| sed -E "s#@prefix@#$(PREFIX)#g" \
	| sed -E "s#@share@#$(INSTALLSHAREDST)#g" \
	| $(SUDO) tee $@ >/dev/null
	@$(SUDO) chmod a+x $@

all: $(DEFAULTCONFIG)

ifdef DEBUG
RUNDVB=$(DEBUG_BINDIR)/dvb --no-report-errors
else
RUNDVB=$(RELEASE_BINDIR)/dvb --no-report-errors
endif

include $(MAKEFILEDIR)/makefile.post

post-install: $(INSTALLTARGETS)
	@echo "Creating directories..."
	-@test -d $(shell $(RUNDVB) --confdir) || ( $(SUDO) mkdir $(shell $(RUNDVB) --confdir) && $(SUDO) chown -R $(LOGNAME):$(LOGNAME) $(shell $(RUNDVB) --confdir) )
	-@test -d $(shell $(RUNDVB) --datadir) || ( $(SUDO) mkdir $(shell $(RUNDVB) --datadir) && $(SUDO) chown -R $(LOGNAME):$(LOGNAME) $(shell $(RUNDVB) --datadir) )
	-@test -d $(shell $(RUNDVB) --datadir)/graphs || ( $(SUDO) mkdir $(shell $(RUNDVB) --datadir)/graphs && $(SUDO) chown -R $(LOGNAME):$(LOGNAME) $(shell $(RUNDVB) --datadir)/graphs )
	-@test -d $(shell $(RUNDVB) --logdir) || ( $(SUDO) mkdir $(shell $(RUNDVB) --logdir) && $(SUDO) chown -R $(LOGNAME):$(LOGNAME) $(shell $(RUNDVB) --logdir) )
	-@test -d $(shell $(RUNDVB) --logdir)/slave || ( $(SUDO) mkdir $(shell $(RUNDVB) --logdir)/slave && $(SUDO) chown -R $(LOGNAME):$(LOGNAME) $(shell $(RUNDVB) --logdir)/slave )
	-@bash -c "cd 'share/gnuplot' ; find . -name \"*.png\" -exec bash -c \"test -f '$(shell $(RUNDVB) --datadir)/graphs/{}' || cp '{}' '$(shell $(RUNDVB) --datadir)/graphs'\" \\;"
