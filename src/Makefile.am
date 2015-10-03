pkgincludedir = $(includedir)/rddvb-@RDDVB_MAJORMINOR@/rddvb

lib_LTLIBRARIES = librddvb-@RDDVB_MAJORMINOR@.la

librddvb_@RDDVB_MAJORMINOR@_la_LDFLAGS =		\
	-no-undefined								\
	-version-info $(RDDVB_LIBVERSION)			\
     $(RDDVB_LDFLAGS)

librddvb_@RDDVB_MAJORMINOR@_la_LIBADD =			\
	$(RDLIB_LIBS)

librddvb_@RDDVB_MAJORMINOR@_la_CPPFLAGS =		\
	$(RDDVB_CFLAGS)								\
	$(RDLIB_CFLAGS)

librddvb_@RDDVB_MAJORMINOR@_la_SOURCES = 		\
	channellist.cpp								\
	config.cpp									\
	dvblock.cpp									\
	dvbmisc.cpp									\
	dvbpatterns.cpp								\
	dvbprog.cpp									\
	episodehandler.cpp							\
	findcards.cpp								\
	proglist.cpp

pkginclude_HEADERS = 							\
	channellist.h								\
	config.h									\
	dvblock.h									\
	dvbmisc.h									\
	dvbpatterns.h								\
	dvbprog.h									\
	episodehandler.h							\
	findcards.h									\
	proglist.h

noinst_HEADERS =

nodist_pkginclude_HEADERS =

DISTCLEANFILES =

bin_PROGRAMS = dvb dvbweb mpgsplit

dvb_LDADD =										\
	$(RDLIB_LIBS)								\
	$(RDDVB_LIBS)

dvb_CPPFLAGS =									\
	$(RDLIB_CFLAGS)								\
	$(RDDVB_CFLAGS)

dvb_SOURCES =									\
	dvb.cpp

EXTRA_dvb_DEPENDENCIES = $(lib_LTLIBRARIES)

dvbweb_LDADD =									\
	$(RDLIB_LIBS)								\
	$(RDDVB_LIBS)

dvbweb_CPPFLAGS =								\
	$(RDLIB_CFLAGS)								\
	$(RDDVB_CFLAGS)

dvbweb_SOURCES =								\
	dvbweb.cpp

EXTRA_dvbweb_DEPENDENCIES = $(lib_LTLIBRARIES)

mpgsplit_LDADD =								\
	$(RDLIB_LIBS)								\
	$(RDDVB_LIBS)

mpgsplit_CPPFLAGS =								\
	$(RDLIB_CFLAGS)								\
	$(RDDVB_CFLAGS)

mpgsplit_SOURCES =								\
	mpgsplit.cpp

EXTRA_mpgsplit_DEPENDENCIES = $(lib_LTLIBRARIES)
