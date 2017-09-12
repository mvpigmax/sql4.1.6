// osdep.h

#if !defined(__OSDEP_H__)
#define __OSDEP_H__

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64) || defined(_WINDOWS) || defined(_WINDOWS_)
#define SQLAPI_WINDOWS
#ifdef _MSC_VER
	#pragma warning(disable : 4290 4121 4995)
#endif
#if defined(__BORLANDC__) && (__BORLANDC__ > 0x520)
	#pragma warn -8059
	#pragma warn -8061
	#ifdef NDEBUG
		#pragma warn -8004
		#pragma warn -8057
	#endif
#endif
#endif

#ifdef SQLAPI_WINDOWS
#ifndef _WIN64
// MSVS 2005 works without this now
//#define _USE_32BIT_TIME_T 1
#else
#define SA_64BIT 1
#endif
#define _CRT_SECURE_NO_DEPRECATE 1
#include <windows.h>
#endif

#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <locale.h>
#include <limits.h>
#if !defined(SQLAPI_WINDOWS) || defined (CYGWIN)
#include <wchar.h>
#include <wctype.h>
#include <dlfcn.h>
#endif
#include <ctype.h>
#include <math.h>

#ifdef SA_64BIT
#if defined(SQLAPI_WINDOWS) || defined(SA_DB2_ODBC64)
#define ODBC64 // for DB2
#endif
#define SS_64BIT_SERVER // for OCI7
#define SYB_LP64 1 // for Sybase
#endif

#include <SQLAPI.h>

#ifdef SA_UNICODE
#ifdef SQLAPI_WINDOWS
#define SA_FMT_STR L"s"
#else
#define SA_FMT_STR L"S"
#include "utf8.h"
#include "utf16.h"
#endif
#define SA_UNICODE_WITH_UTF8 1
#else
#define SA_FMT_STR "s"
#undef SA_UNICODE_WITH_UTF8
#ifndef SQLAPI_WINDOWS
#include "utf16.h"
#endif
#endif

SAChar *sa_strlwr(SAChar *src);
SAChar *sa_strupr(SAChar *src);

#ifndef SHARED_OBJ_EXT
#if defined(SQLAPI_WINDOWS)
#	define SHARED_OBJ_EXT _TSA(".dll")
#else
#if defined(SQLAPI_AIX)
#ifdef __64BIT__
#	define SHARED_OBJ_EXT ".a(shr_64.o)"
#else
#	define SHARED_OBJ_EXT ".a(shr.o)"
#endif
#else
#if defined(SQLAPI_HPUX)
#	define SHARED_OBJ_EXT ".sl"
#else
#if defined(SQLAPI_OSX)
#  define SHARED_OBJ_EXT ".dylib"
#else
#  define SHARED_OBJ_EXT ".so"
#endif
#endif
#endif
#endif
#endif

#if defined(SQLAPI_AIX)
#define EXT_DLOPEN_FLAGS RTLD_MEMBER
#define DB2_SUPPORTS_SQL_C_NUMERIC_UNIX
#else
#define EXT_DLOPEN_FLAGS 0
#endif

#define SA_MAKELONG(a, b)      (((long)((short)(a))) | (((long)((short)(b))) << 16))

void* sa_malloc(size_t size);
void* sa_calloc(size_t num, size_t size);
void sa_realloc(void** inPtr, size_t newLen);
long SAExtractVersionFromString(const SAString &sVerString);
#ifdef SQLAPI_WINDOWS
void *SAGetVersionInfo(LPCTSTR sDLLName);
long SAGetFileVersionFromString(LPCTSTR sDLLName);
long SAGetProductVersion(LPCTSTR sDLLName);
DWORD SAGetLastErrorText(SAString& sMessage);
HMODULE SALoadLibraryFromList(SAString& sLibsList, SAString& sErrorMessage, SAString& sLibName);

#include <float.h>
#define sa_isnan(x) _isnan(x)
#else
#include <dlfcn.h>
void* SALoadLibraryFromList(SAString& sLibsList, SAString& sErrorMessage, SAString& sLibName, int flag);

#define sa_isnan(x) isnan(x)
#endif // defined(SQLAPI_WINDOWS)

#if (_MSC_VER >= 1300) && (WINVER < 0x0500)
// VC7 or later, building with pre-VC7 runtime libraries
extern "C" long _ftol2(double dblSource);
#endif

#include <samisc.h>
#include "saerrmsg.h"

// internal strtoull
#ifdef SQLAPI_WINDOWS
#	ifndef SA_INTERNAL_STRTOULL
#		if (_MSC_VER >= 1400)
#			define sa_strtoull(x,y,z) _strtoui64(x,y,z)
#		else
#			if defined(__MINGW32__) || (__BORLANDC__ > 0x630)
#				define sa_strtoull(x,y,z) strtoull(x,y,z)
#			else
#				define SA_INTERNAL_STRTOULL 1
#			endif
#		endif
#	endif
#else // ! SQLAPI_WINDOWS
#	if defined(SQLAPI_HPUX) || defined(SQLAPI_SCOOSR5)
#		define sa_strtoull(x,y,z) strtoul(x,y,z)
#	else
#		define sa_strtoull(x,y,z) strtoull(x,y,z)
#	endif
#endif // SQLAPI_WINDOWS

#ifdef SA_INTERNAL_STRTOULL
extern "C" sa_uint64_t sa_strtoull(const char *nptr, char **endptr, int base);
#endif

#ifdef __ANDROIDDD__
struct lconv
{
    /* Numeric (non-monetary) information.  */

    char *decimal_point;		/* Decimal point character.  */
    char *thousands_sep;		/* Thousands separator.  */
    /* Each element is the number of digits in each group;
    elements with higher indices are farther left.
    An element with value CHAR_MAX means that no further grouping is done.
    An element with value 0 means that the previous element is used
    for all groups farther left.  */
    char *grouping;

    /* Monetary information.  */

    /* First three chars are a currency symbol from ISO 4217.
    Fourth char is the separator.  Fifth char is '\0'.  */
    char *int_curr_symbol;
    char *currency_symbol;	/* Local currency symbol.  */
    char *mon_decimal_point;	/* Decimal point character.  */
    char *mon_thousands_sep;	/* Thousands separator.  */
    char *mon_grouping;		/* Like `grouping' element (above).  */
    char *positive_sign;		/* Sign for positive values.  */
    char *negative_sign;		/* Sign for negative values.  */
    char int_frac_digits;		/* Int'l fractional digits.  */
    char frac_digits;		/* Local fractional digits.  */
    /* 1 if currency_symbol precedes a positive value, 0 if succeeds.  */
    char p_cs_precedes;
    /* 1 iff a space separates currency_symbol from a positive value.  */
    char p_sep_by_space;
    /* 1 if currency_symbol precedes a negative value, 0 if succeeds.  */
    char n_cs_precedes;
    /* 1 iff a space separates currency_symbol from a negative value.  */
    char n_sep_by_space;
    /* Positive and negative sign positions:
    0 Parentheses surround the quantity and currency_symbol.
    1 The sign string precedes the quantity and currency_symbol.
    2 The sign string follows the quantity and currency_symbol.
    3 The sign string immediately precedes the currency_symbol.
    4 The sign string immediately follows the currency_symbol.  */
    char p_sign_posn;
    char n_sign_posn;
    /* 1 if int_curr_symbol precedes a positive value, 0 if succeeds.  */
    char int_p_cs_precedes;
    /* 1 iff a space separates int_curr_symbol from a positive value.  */
    char int_p_sep_by_space;
    /* 1 if int_curr_symbol precedes a negative value, 0 if succeeds.  */
    char int_n_cs_precedes;
    /* 1 iff a space separates int_curr_symbol from a negative value.  */
    char int_n_sep_by_space;
    /* Positive and negative sign positions:
    0 Parentheses surround the quantity and int_curr_symbol.
    1 The sign string precedes the quantity and int_curr_symbol.
    2 The sign string follows the quantity and int_curr_symbol.
    3 The sign string immediately precedes the int_curr_symbol.
    4 The sign string immediately follows the int_curr_symbol.  */
    char int_p_sign_posn;
    char int_n_sign_posn;
};

char *setlocale(int category, const char *locale);
struct lconv* localeconv(void);
#endif

#endif // !defined(__OSDEP_H__)
