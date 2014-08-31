
AUTOMAKE_OPTIONS = foreign

include doxygen.am

SUBDIRS = src

EXTRA_DIST = README COPYING autogen.sh

pkgconfig_DATA = rddvb-$(RDDVB_MAJORMINOR).pc

rddvb-$(RDDVB_MAJORMINOR).pc: rddvb.pc
	cp rddvb.pc rddvb-$(RDDVB_MAJORMINOR).pc

rddvb-$(RDDVB_MAJORMINOR)-uninstalled.pc: rddvb-uninstalled.pc
	cp rddvb-uninstalled.pc rddvb-$(RDDVB_MAJORMINOR)-uninstalled.pc

BUILT_SOURCES = rddvb-$(RDDVB_MAJORMINOR)-uninstalled.pc

CLEANFILES = rddvb-$(RDDVB_MAJORMINOR).pc rddvb-$(RDDVB_MAJORMINOR)-uninstalled.pc

ACLOCAL_FLAGS = -I m4
