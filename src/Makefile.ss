# Makefile for SQLAPI++
# Solaris Studio version

.SUFFIXES:
.SUFFIXES: .cpp .obj

SQLAPI_OS=SQLAPI_SOLARIS

ifndef CFG
CFG=release
$(waning SQLAPI: Build "release" configuration)
endif

ifndef CFG_LINK
CFG_LINK=dynamic
$(waning SQLAPI: Use "dynamic" configuration)
endif

ifndef SA_DEFINE
SA_DEFINE=SA_TRIAL
$(waning SQLAPI: WARNING!!! Use trial compile flag)
endif

CPP = CC
CC = cc
LIBS =

SA_API_SRC_SUFFIX=_linux
SA_INCLUDE_SUFFIX=_linux

SA_INCLUDES = -I../include
CPPDEFS = -D$(SQLAPI_OS) -D$(SA_DEFINE) -DSBSTDCALL=""
CPPFLAGS = +w $(SA_CPPFLAGS)
LDFLAGS = $(SA_LDFLAGS)

ifdef SA_UNICODE
CPPDEFS += -DUNICODE -D_UNICODE -DSA_UNICODE
SQLAPI_UNICODE_SUFFIX=u
else
SQLAPI_UNICODE_SUFFIX=
endif

# Thread safe
ifdef SA_USE_PTHREAD
CPPFLAGS += -mt
LIBS += -lpthread
endif

# 64-bit version
# -m64/-m32 actual since Studio 12 only
# for older version -xarch=xxx option should be used
ifdef SA_64BIT
CPPDEFS += -DSA_64BIT=1
CPPFLAGS += -m64 -KPIC
LDFLAGS += -m64
else
ifdef SA_32BIT
CPPFLAGS += -m32
LDFLAGS += -m32
endif
endif

SHARED_EXT = so

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

ifeq "$(CFG)" "release"
CPPDEFS += -DNDEBUG
# -fast option can forces 32-bit object output
#CPPFLAGS += -fast 
LDFLAGS += -s
SQLAPI_DEBUG_SUFFIX=
else
CPPFLAGS += -g
CPPDEFS += -D_DEBUG
SQLAPI_DEBUG_SUFFIX=d
endif

SA_NO_OLEDB=1
SA_NO_SQLNCLI=1

ifdef SA_CLIENT_ALL
SA_CLIENT_IBASE=1
SA_CLIENT_ODBC=1
#SA_CLIENT_MSSQL=1
SA_CLIENT_ORACLE=1
#SA_CLIENT_SQLBASE=1
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

COMPILE_CC=$(CC) $(CPPFLAGS) -c -KPIC $(CPPDEFS) -o 
COMPILE_CPP=$(CPP) $(CPPFLAGS) -c -KPIC $(CPPDEFS) $(SA_CLI_DEFS) -o 
SA_TARGET_NAME=libsqlapi$(SQLAPI_UNICODE_SUFFIX)$(SQLAPI_DEBUG_SUFFIX)
SA_TARGET=$(SA_TARGET_NAME).$(SHARED_EXT)

$(SA_TARGET): $(DEPS) $(SA_OBJS)
	@$(CPP) $(LDFLAGS) -G $(SA_OBJS) $(LIBS) -o $@

else

COMPILE_CC=$(CC) $(CPPFLAGS) -c $(CPPDEFS) -o 
COMPILE_CPP=$(CPP) $(CPPFLAGS) -c $(CPPDEFS) $(SA_CLI_DEFS) -o 
SA_TARGET_NAME=libsqlapi$(SQLAPI_UNICODE_SUFFIX)$(SQLAPI_DEBUG_SUFFIX)
SA_TARGET=$(SA_TARGET_NAME).a

$(SA_TARGET): $(DEPS) $(SA_OBJS)
	@$(CPP) -xar -o $@ $(SA_OBJS)

endif

include build/Makefile.SA_OBJS

include build/Makefile.UTILS_UNIX
