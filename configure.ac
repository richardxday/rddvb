AC_PREREQ([2.68])
AC_INIT([RDDVB],[0.1.0.0])

AS_NANO(RDDVB_CVS=no,RDDVB_CVS=yes)

AM_INIT_AUTOMAKE([1.11 silent-rules])
AM_SILENT_RULES([yes])

# Version number that will be appended to the library filename
RDDVB_MAJORMINOR=0.1
AC_SUBST(RDDVB_MAJORMINOR)

AC_CONFIG_HEADERS([config.h])

# CURRENT, REVISION, AGE
# - library source changed but interface the same -> increment REVISION
# - interfaces added/removed/changed -> increment CURRENT, set REVISION = 0
# - interfaces added -> increment AGE also
# - interfaces removed -> set AGE = 0
#    (AGE is the number of previous values of CURRENT that are compatible)
RDDVB_LIBVERSION="1:0:0"
AC_SUBST(RDDVB_LIBVERSION)

LT_PREREQ([2.2.6])
LT_INIT(disable-static win32-dll)

AC_PROG_CXX
AM_PROG_CC_C_O

AC_CONFIG_SRCDIR([src])

AC_CONFIG_MACRO_DIR([m4])
ACLOCAL_AMFLAGS="-I m4 $ACLOCAL_AMFLAGS"
AC_SUBST(ACLOCAL_AMFLAGS)

DX_DOXYGEN_FEATURE(ON)
DX_INIT_DOXYGEN(rddvb, doxygen.cfg)

# Check if we should enable GPL code  (./configure --enable-gpl)
AC_MSG_CHECKING(whether to include code under gpl)
AC_ARG_ENABLE(gpl, AS_HELP_STRING([--disable-gpl], [disable code under GPL]), ENABLE_GPL="no", ENABLE_GPL="yes")
AM_CONDITIONAL(ENABLE_GPL, test "x${ENABLE_GPL}" = "xyes")
if test "x${ENABLE_GPL}" = "xyes"; then
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi

# Check if we should disable optimization  (./configure --disable-opt)
AC_MSG_CHECKING(whether to disable optimization)
AC_ARG_ENABLE(opt, AS_HELP_STRING([--disable-opt], [disable optimzation]), DISABLE_OPTIMIZATION="yes", DISABLE_OPTIMIZATION="no")
if test "x${DISABLE_OPTIMIZATION}" = "xyes"; then
  CFLAGS="$CFLAGS -O0"
  CXXFLAGS="$CXXFLAGS -O0"
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi
AC_SUBST(CFLAGS)
AC_SUBST(CXXFLAGS)

dnl rdlib is required
RDLIB_VER="0.1.0.0"
PKG_CHECK_MODULES(RDLIB, rdlib-0.1 >= $RDLIB_VER, HAVE_RDLIB=yes, HAVE_RDLIB=no)
if test "x${HAVE_RDLIB}" != xyes ; then
  AC_MSG_ERROR([rdlib-0.1 >= $RDLIB_VER is required])
fi

AC_ARG_WITH([thread],
    AS_HELP_STRING([--with-thread=IMPL],[use threading library {auto,pthread,gthread,win32,none} [[default=auto]]]),
    [],
    [with_thread=auto])

AC_MSG_CHECKING([threading implementation to use])
if test "x${with_thread}" = "xauto" ; then
  case $host_os in
    mingw* | pw32*)
      with_thread=win32
      ;;
    *)
      with_thread=pthread
      ;;
  esac
fi
AC_MSG_RESULT([$with_thread])

THREAD_IMPL=${with_thread}
AC_SUBST(THREAD_IMPL)

if test "x${with_thread}" = "xpthread" ; then
  case "$host_os" in
    *)
      PTHREAD_CFLAGS=
      PTHREAD_LIBS=-lpthread
      ;;
  esac
  AC_SUBST(PTHREAD_CFLAGS)
  AC_SUBST(PTHREAD_LIBS)
fi

if test "x${with_thread}" = "xgthread" ; then
  PKG_CHECK_MODULES(GTHREAD, gthread-2.0, HAVE_GTHREAD=yes, HAVE_GTHREAD=no)
  AC_SUBST(GTHREAD_LIBS)
  AC_SUBST(GTHREAD_CFLAGS)
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

AC_APPEND_SUPPORTED_CFLAGS(RDDVB_CFLAGS, [-Wall])
AC_APPEND_SUPPORTED_CFLAGS(RDDVB_CFLAGS, [-Werror])
AC_APPEND_SUPPORTED_CFLAGS(RDDVB_CFLAGS, [-Wextra])
AC_APPEND_SUPPORTED_CFLAGS(RDDVB_CFLAGS, [-msse2])
AC_APPEND_SUPPORTED_CFLAGS(RDDVB_CFLAGS, [-fsigned-char])

RDDVB_LDFLAGS=""
AC_APPEND_SUPPORTED_LDFLAGS(RDDVB_LDFLAGS, [-Wl,--no-undefined])
AC_SUBST(RDDVB_LDFLAGS)

RDDVB_LIBS="\$(top_builddir)/src/librddvb-$RDDVB_MAJORMINOR.la"
AC_SUBST(RDDVB_LIBS)

AC_SUBST(RDDVB_PKG_DEPS)

pkgconfigdir="\$(libdir)/pkgconfig"
AC_SUBST(pkgconfigdir)

librddvbbindir="\$(libdir)/rddvb/"
AC_SUBST(librddvbbindir)

AC_CONFIG_FILES([
Makefile
rddvb-uninstalled.pc
rddvb.pc
src/Makefile
])
AC_OUTPUT
