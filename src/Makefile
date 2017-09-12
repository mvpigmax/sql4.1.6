# Makefile for SQLAPI++
# Linux/Unix version

.SUFFIXES:
.SUFFIXES: .cpp .obj

ifndef SQLAPI_OS
#SQLAPI_OS=SQLAPI_LINUX
#SQLAPI_OS=SQLAPI_AIX
#SQLAPI_OS=SQLAPI_HPUX
SQLAPI_OS=SQLAPI_OSX
endif

ifndef CFG
CFG=release
$(warning SQLAPI: Build "release" configuration)
endif

ifndef CFG_LINK
CFG_LINK=dynamic
$(warning SQLAPI: Use "dynamic" configuration)
endif

ifndef SA_DEFINE
SA_DEFINE=SA_TRIAL
$(warning SQLAPI: WARNING!!! Use trial compile flag)
endif

CPP = $(SA_TOOLCHAIN_PREFIX)g++$(SA_CPP_VER)
CC = $(SA_TOOLCHAIN_PREFIX)gcc$(SA_CPP_VER)
AR = $(SA_TOOLCHAIN_PREFIX)ar
RUNLIB = $(SA_TOOLCHAIN_PREFIX)ranlib
LIBS = $(SA_LIBS)

#add by xp fix mac -lpthread not found bug
LIBS_PATH = -L/usr/lib

SA_API_SRC_SUFFIX=_linux
SA_INCLUDE_SUFFIX=_linux

SA_INCLUDES = -I../include -I/usr/include
CPPDEFS = -D$(SQLAPI_OS) -D$(SA_DEFINE)
CPPFLAGS = -Wall $(SA_CPPFLAGS)
LDFLAGS = $(SA_LDFLAGS)
LDFLAGS_SONAME = -Wl,-soname,

ifdef SA_UNICODE
CPPDEFS += -DUNICODE -D_UNICODE -DSA_UNICODE
SQLAPI_UNICODE_SUFFIX=u
else
SQLAPI_UNICODE_SUFFIX=
endif

# Thread safe
ifdef SA_USE_PTHREAD
CPPDEFS += -DSA_USE_PTHREAD
ifeq "$(SQLAPI_OS)" "SQLAPI_SOLARIS"
CPPFLAGS += -pthreads
else
CPPFLAGS += -pthread
endif
LIBS += -lpthread
endif

# 64-bit version
ifdef SA_64BIT
CPPDEFS += -DSA_64BIT=1
CPPFLAGS += -m64 -fPIC
LDFLAGS += -m64
else
ifdef SA_32BIT
CPPFLAGS += -m32
LDFLAGS += -m32
endif
endif

ifeq "$(SQLAPI_OS)" "SQLAPI_FREEBSD"
CPPFLAGS += -DFREEBSD
endif

ifeq "$(SQLAPI_OS)" "SQLAPI_AIX"
LIBS += -ldl
endif

ifeq "$(SQLAPI_OS)" "SQLAPI_OSX"
LDFLAGS += -dynamiclib -single_module ${wl}-flat_namespace ${wl}-undefined ${wl}suppress
LDFLAGS_SONAME = -Wl,-dylib_install_name -Wl,
ifndef SA_MACOSX_SYSROOT
#mod by xp for mac link erro
#SA_MACOSX_SYSROOT=/Developer/SDKs/MacOSX10.5.sdk
endif
ifndef SA_MACOSX_MINVER
SA_MACOSX_MINVER=10.5
endif
#mod by xp for mac link error
#CPPFLAGS += -isysroot $(SA_MACOSX_SYSROOT) -mmacosx-version-min=$(SA_MACOSX_MINVER)
#LDFLAGS += -isysroot $(SA_MACOSX_SYSROOT) -mmacosx-version-min=$(SA_MACOSX_MINVER)
CPPFLAGS += -isysroot $(SA_MACOSX_SYSROOT)
LDFLAGS += -isysroot $(SA_MACOSX_SYSROOT)
SA_INCLUDES += -I/usr/include/malloc
SHARED_EXT = dylib
else
SHARED_EXT = so
endif

ifdef SA_USE_STL
CPPDEFS  += -DSA_USE_STL
endif

ifdef SA_PARAM_USE_DEFAULT
CPPDEFS  += -DSA_PARAM_USE_DEFAULT
endif

ifdef SA_STRING_EXT
CPPDEFS  += -DSA_STRING_EXT
endif

ifdef SA_THROW_WRONG_DATA_EXCEPTION
CPPDEFS  += -DSQLAPI_DATETIME_WRONG_EXCEPTION
endif

ifdef SA_ODBC_SQL_WCHART_CONVERT
CPPDEFS  += -DSA_ODBC_SQL_WCHART_CONVERT=$(SA_ODBC_SQL_WCHART_CONVERT)
endif

ifeq "$(CFG)" "release"
CPPDEFS += -DNDEBUG
CPPFLAGS += -O2 -fno-strict-aliasing 
LDFLAGS += -s
SQLAPI_DEBUG_SUFFIX=
else
CPPFLAGS += -g -fno-strict-aliasing 
CPPDEFS += -D_DEBUG
SQLAPI_DEBUG_SUFFIX=d
endif

SA_NO_OLEDB=1
SA_NO_DBLIB=1

ifdef SA_CLIENT_ALL
SA_CLIENT_IBASE=1
SA_CLIENT_ODBC=1
ifdef SA_64BIT
SA_CLIENT_MSSQL=1
endif
SA_CLIENT_ORACLE=1
SA_CLIENT_SQLBASE=1
SA_CLIENT_INFORMIX=1
SA_CLIENT_DB2=1
SA_CLIENT_SYBASE=1
SA_CLIENT_MYSQL=1
SA_CLIENT_PGSQL=1
SA_CLIENT_SQLITE=1
SA_CLIENT_ASA=1
endif

include build/Makefile.DBMS_GNU

ifdef SA_UNICODE
SA_OBJS += utf8.obj
endif

ifdef SA_USE_PCH
DEPS=osdep.h.gch
endif

ifeq "$(CFG_LINK)" "dynamic"

COMPILE_CC=$(CC) $(CPPFLAGS) -c -fPIC $(CPPDEFS) -o 
COMPILE_CPP=$(CPP) $(CPPFLAGS) -c -fPIC $(CPPDEFS) $(SA_CLI_DEFS) -o 
SA_TARGET_NAME=libsqlapi$(SQLAPI_UNICODE_SUFFIX)$(SQLAPI_DEBUG_SUFFIX)
SA_TARGET=$(SA_TARGET_NAME).$(SHARED_EXT)

$(SA_TARGET): $(DEPS) $(SA_OBJS)
	$(CPP) $(LDFLAGS) -shared $(SA_OBJS) $(LIBS_PATH) $(LIBS) $(LDFLAGS_SONAME)$@ -o $@

else

COMPILE_CC=$(CC) $(CPPFLAGS) -c $(CPPDEFS) -o 
COMPILE_CPP=$(CPP) $(CPPFLAGS) -c $(CPPDEFS) $(SA_CLI_DEFS) -o 
SA_TARGET_NAME=libsqlapi$(SQLAPI_UNICODE_SUFFIX)$(SQLAPI_DEBUG_SUFFIX)
SA_TARGET=$(SA_TARGET_NAME).a

$(SA_TARGET): $(DEPS) $(SA_OBJS)
	@$(AR) r $@ $(SA_OBJS)
	@$(RUNLIB) $@

endif

include build/Makefile.SA_OBJS

include build/Makefile.UTILS_UNIX
