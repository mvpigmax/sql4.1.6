// SQLAPI.cpp
//

/*!
\mainpage
\ref enums "SQLAPI++ defined enums"

\ref typedefs "SQLAPI++ defined types"
*/

/*! \typedef SAClient_t
\brief
Supported DBMS clients.

Describes a list of supported DBMS clients.
*/

/*! \typedef SAErrorClass_t
\brief
Possible types of errors.

Describes a set of possible types of a error.
*/

/*! \typedef SAIsolationLevel_t
\brief
Possible transaction isolation levels.

Describes a set of possible transaction isolation levels.
*/

/*! \typedef SAAutoCommit_t
\brief
Possible auto-commit modes.

Describes a set of possible auto-commit modes.
*/

/*! \typedef SACommandType_t
\brief
Possible command types.

Describes a set of possible types of a command.

By default the Library tries to determine the SQL command type itself. It analyzes
the query command, sets the command types and build the parameter list.

\remarks
For any supported DBMS SQLAPI++ interprets the ":<name>" or ":<number>"
SQL command substrings as a bind parameter positions and builds the parameter list.
*/

/*! \typedef SAConnectionHandlerType_t
\brief
Describes types of a handle called for DBMS connection.

The Library allows to define the DBMS connection process handler.
*/

//! \ingroup typedefs
//! \{

//! \typedef saLongOrLobReader_t

//! \typedef saConnectionHandler_t

//! \}

#include "osdep.h"
#include "ISAClient.h"
#include <locale.h>

#ifdef SA_USE_STL
#include <algorithm>
#endif

#ifdef SQLAPI_AllClients
#define SQLAPI_odbcClient
#define SQLAPI_oraClient
#define SQLAPI_ssClient
#define SQLAPI_ibClient
#define SQLAPI_sbClient
#define SQLAPI_db2Client
#define SQLAPI_infClient
#define SQLAPI_sybClient
#define SQLAPI_myClient
#define SQLAPI_pgClient
#define SQLAPI_slClient
#define SQLAPI_asaClient
#endif

#ifdef SQLAPI_odbcClient
#include "odbcClient.h"
#endif
#ifdef SQLAPI_oraClient
#include "oraClient.h"
#endif
#ifdef SQLAPI_ssClient
#include "ssClient.h"
#endif
#ifdef SQLAPI_ibClient
#include "ibClient.h"
#endif
#ifdef SQLAPI_sbClient
#include "sbClient.h"
#endif
#ifdef SQLAPI_db2Client
#include "db2Client.h"
#endif
#ifdef SQLAPI_infClient
#include "infClient.h"
#endif
#ifdef SQLAPI_sybClient
#include "sybClient.h"
#endif
#ifdef SQLAPI_myClient
#include "myClient.h"
#endif
#ifdef SQLAPI_pgClient
#include "pgClient.h"
#endif
#ifdef SQLAPI_slClient
#include "sl3Client.h"
#endif
#ifdef SQLAPI_asaClient
#include "asaClient.h"
#endif

#ifdef SQLAPI_odbcClient
static IodbcClient odbcClient;	// ODBC client
#endif
#ifdef SQLAPI_oraClient
static IoraClient oraClient;	// Oracle client
#endif
#ifdef SQLAPI_ssClient
static IssClient ssClient;		// Microsoft SQL Server client
#endif
#ifdef SQLAPI_ibClient
static IibClient ibClient;		// InterBase client
#endif
#ifdef SQLAPI_sbClient
static IsbClient sbClient;		// Centura SqlBase client
#endif
#ifdef SQLAPI_db2Client
static Idb2Client db2Client;	// DB2 client
#endif
#ifdef SQLAPI_infClient
static IinfClient infClient;	// Informix client
#endif
#ifdef SQLAPI_sybClient
static IsybClient sybClient;	// Sybase client
#endif
#ifdef SQLAPI_myClient
static ImyClient myClient;	// MySQL client
#endif
#ifdef SQLAPI_pgClient
static IpgClient pgClient;	// PostgreSQL client
#endif
#ifdef SQLAPI_slClient
static Isl3Client slClient;	// SQLite client
#endif
#ifdef SQLAPI_asaClient
static IasaClient asaClient;	// SQL Anywhere client
#endif

static ISAClient *stat_SAClients[] =
{
    NULL,
#ifdef SQLAPI_odbcClient
    &odbcClient,
#else
    NULL,
#endif
#ifdef SQLAPI_oraClient
    &oraClient,
#else
    NULL,
#endif
#ifdef SQLAPI_ssClient
    &ssClient,
#else
    NULL,
#endif
#ifdef SQLAPI_ibClient
    &ibClient,
#else
    NULL,
#endif
#ifdef SQLAPI_sbClient
    &sbClient,
#else
    NULL,
#endif
#ifdef SQLAPI_db2Client
    &db2Client,
#else
    NULL,
#endif
#ifdef SQLAPI_infClient
    &infClient,
#else
    NULL,
#endif
#ifdef SQLAPI_sybClient
    &sybClient,
#else
    NULL,
#endif
#ifdef SQLAPI_myClient
    &myClient,
#else
    NULL,
#endif
#ifdef SQLAPI_pgClient
    &pgClient,
#else
    NULL,
#endif
#ifdef SQLAPI_slClient
    &slClient,
#else
    NULL,
#endif
#ifdef SQLAPI_asaClient
    &asaClient,
#else
    NULL,
#endif
};

struct sa_Commands
{
    SACommand	*pCommand;
    ISACursor	*pISACursor;
    sa_Commands	*Next;
};

#if !defined(SA_NO_TRIAL)

const SAChar *sTrialText =
_TSA("Thank you for trying SQLAPI++ Library.\n")
_TSA("\n")
_TSA("This version is for evaluation purpose only.\n")
_TSA("You can register SQLAPI++ using online registration service.\n")
_TSA("\n")
_TSA("For additional information visit:\n")
_TSA("    SQLAPI++ Home (http://www.sqlapi.com)\n")
_TSA("For help on using the Library e-mail to:\n")
_TSA("    howto@sqlapi.com");

const SAChar *sTrialCaption =
_TSA("SQLAPI++ Registration Reminder");

static void CheckTrial()
{
#ifdef SA_TIME_LIMITED_TRIAL
    SADateTime dtValid(2002, 4, 4, 0, 0, 0);
    time_t valid = mktime(&dtValid.operator tm &());

    time_t now;
    time(&now);

    if(now < valid)
        return;
#endif

    static bool bCheckedTrial = false;
    if (!bCheckedTrial)
    {
#ifdef SQLAPI_WINDOWS
        ::MessageBox(NULL,
            sTrialText,
            sTrialCaption, 0);
        bCheckedTrial = true;
#else	// never set bCheckedTrial = true; in Linux/Unix
        srand((unsigned)time(NULL));
        int nRand = rand();
        if(nRand % 5 == 0)
            SAException::throwUserException(-1, _TSA("Trial version exception:\n%") SA_FMT_STR, sTrialText);
        SAString s(sTrialText);
        sa_printf(_TSA("%") SA_FMT_STR _TSA("\n\n"), (const SAChar*)s);
#endif	// SQLAPI_WINDOWS
    }
}

#endif	// !defined(SA_NO_TRIAL)

//////////////////////////////////////////////////////////////////////
// SAGlobals Class
//////////////////////////////////////////////////////////////////////

/*! \class SAGlobals

We use class to avoid namespace using
*/

/*static */
char * SQLAPI_CALLBACK SAGlobals::setlocale(int category, const char *locale)
{
    return ::setlocale(category, locale);
}

/*static */
int SAGlobals::GetVersionMajor()
{
    return SQLAPI_VER_MAJOR;
}

/*static */
int SAGlobals::GetVersionMinor()
{
    return SQLAPI_VER_MINOR;
}

/*static */
int SAGlobals::GetVersionBuild()
{
    return SQLAPI_VER_BUILD;
}

static bool bUnloadAPIs = false;
/*static */
bool& SAGlobals::UnloadAPI()
{
    return bUnloadAPIs;
}

static struct SA_C2S {
    SAClient_t eSAClient;
    const SAChar* szClientName;
} saC2S[] = {
    { SA_Client_NotSpecified, _TSA("Unknown") },
    { SA_ODBC_Client, _TSA("ODBC") },
    { SA_Oracle_Client, _TSA("Oracle") },
    { SA_SQLServer_Client, _TSA("SQLServer") },
    { SA_InterBase_Client, _TSA("InterBase") },
    { SA_SQLBase_Client, _TSA("SQLBase") },
    { SA_DB2_Client, _TSA("DB2") },
    { SA_Informix_Client, _TSA("Informix") },
    { SA_Sybase_Client, _TSA("Sybase") },
    { SA_MySQL_Client, _TSA("MySQL") },
    { SA_PostgreSQL_Client, _TSA("PostgreSQL") },
    { SA_SQLite_Client, _TSA("SQLite") },
    { SA_SQLAnywhere_Client, _TSA("SQLAnywhere") },
};

/*static */
const SAChar* SAGlobals::ClientToString(SAClient_t eSAClient)
{
    if (eSAClient >= SA_Client_NotSpecified && eSAClient <= SA_SQLAnywhere_Client)
        return saC2S[eSAClient].szClientName;
    return _TSA("Unknown");
}

/*static */
SAClient_t SAGlobals::StringToClient(const SAChar* szClientName)
{
    SAString sClientName(szClientName);
    for (int i = SA_Client_NotSpecified; i <= SA_SQLAnywhere_Client; ++i)
    {
        if (0 == sClientName.CompareNoCase(saC2S[i].szClientName))
            return saC2S[i].eSAClient;
    }
    return SA_Client_NotSpecified;
}

/*static */
void SAGlobals::SetTraceFunction(SATraceInfo_t traceInfo, SATraceFunction_t traceFunc, void* pData)
{
    saTraceInfo = traceInfo;
    saTraceFunc = traceFunc;
    saTraceData = pData;
}

//////////////////////////////////////////////////////////////////////
// SAString Class
//////////////////////////////////////////////////////////////////////
struct SAStringConvertedData
{
    size_t nDataLength;        // length of converted data (including terminator)
    // !SAChar ConvertedData[nDataLength+1]

#ifdef SA_UNICODE
    char *data()		// char * to converted data
    {
        return (char *)(this + 1);
    }
#else	// !SA_UNICODE
    wchar_t *data()	// wchar_t * to converted data
    {
        return (wchar_t *)(this + 1);
    }
#endif	//!SA_UNICODE
};

#ifndef SQLAPI_WINDOWS
struct SAStringUtf16Data
{
    size_t nDataLength;
    UTF16* data() { return (UTF16*)(this+1); };
};
#endif

struct SAStringData
{
    SAStringConvertedData	*pConvertedData;	// pointer to converted data, if any

#ifdef SA_UNICODE
    SAStringConvertedData	*pUTF8Data;
    // nDataLength*sizeof(SAChar) - real length of binary data
    // also true: nLengthInBytes % sizeof(SAChar)
    // In bytes [0..sizeof(SAChar)-1]
    size_t nBinaryDataLengthDiff;
#endif	// SA_UNICODE
#ifndef SQLAPI_WINDOWS
    SAStringUtf16Data		*pUTF16Data;
#endif

    long nRefs;				// reference count
    size_t nDataLength;		// length of data (including terminator)
    size_t nAllocLength;	// length of allocation
    // SAChar data[nAllocLength]

    SAChar *data()           // SAChar * to managed data
    {
        return (SAChar *)(this + 1);
    }
};

/*! \class SAString
*
* A SAString object consists of a variable-length sequence of characters.
* Concatenation and comparison operators, together with simplified memory management,
* make SAString objects easier to use than ordinary character arrays.
*
*/

// For an empty string, m_pchData will point here
// (note: avoids special case of checking for NULL m_pchData)
// empty string data (and locked)
static struct saInitData
{
    SAStringData header;
    SAChar data[1];	// SAChar data[nAllocLength+1]
} _saInitData =
{
    {
        0,	// pConvertedData
#ifdef SA_UNICODE
        0, // pUTF8Data
        0,	// nBinaryDataLengthDiff
#endif	// SA_UNICODE
#ifndef SQLAPI_WINDOWS
        0, // pUTF16Data
#endif
        - 1,	// nRefs
        0,	// nDataLength
        0	// nAllocLength
    },
    {
        0	// SAChar data[nAllocLength]
    }
};

static SAStringData *_saDataNil = &_saInitData.header;
static const SAChar *_saPchNil = &_saInitData.data[0];
// special function to make saEmptyString work even during initialization
static const SAString &saGetEmptyString()
{
    return *(SAString *)&_saPchNil;
}
#define saEmptyString saGetEmptyString()

void SAString::AllocBuffer(size_t nLen)
// always allocate one extra character for '\0' termination
// assumes [optimistically] that data length will equal allocation length
{
    assert(nLen <= SIZE_MAX - 1);    // max size (enough room for 1 extra)

    if (nLen == 0)
        Init();
    else
    {
        size_t nAllocLength = nLen;
#ifdef SA_STRING_EXT
        if( m_pchData != NULL )
            nAllocLength *= 2;
#endif
        SAStringData *pData;
        pData = (SAStringData*)
            new unsigned char[sizeof(SAStringData) + (nAllocLength + 1)*sizeof(SAChar)];
        pData->nAllocLength = nAllocLength;

        // start of SAString special
        pData->pConvertedData = NULL;
#ifdef SA_UNICODE
        pData->pUTF8Data = NULL;
        pData->nBinaryDataLengthDiff = 0l;
#endif	// SA_UNICODE
#ifndef SQLAPI_WINDOWS
        pData->pUTF16Data = NULL;
#endif
        // end of SAString special

        pData->nRefs = 1;
        pData->data()[nLen] = _TSA('\0');
        pData->nDataLength = nLen;
        m_pchData = pData->data();
    }
}

/*static */
void SAString::FreeData(SAStringData *pData)
{
#ifdef SA_UNICODE
    if (pData->pUTF8Data)
        delete[](unsigned char*)pData->pUTF8Data;
#endif
#ifndef SQLAPI_WINDOWS
    if( pData->pUTF16Data )
        delete[] (unsigned char*)pData->pUTF16Data;
#endif
    if (pData->pConvertedData)
        delete[](unsigned char*)pData->pConvertedData;
    if (pData)
        delete[](unsigned char*)pData;
}

//////////////////////////////////////////////////////////////////////////////
// concatenate in place

#ifdef SA_UNICODE

void SAString::ConcatBinaryInPlace(size_t nSrcLenInBytes, const void *pSrcData)
{
    // concatenating an empty string is a no-op!
    if (nSrcLenInBytes == 0)
        return;

    // if a conversion has been made already, it is not valid any more
    if (GetData()->pConvertedData)
        delete[](unsigned char*)GetData()->pConvertedData;
    GetData()->pConvertedData = NULL;

    if (GetData()->pUTF8Data)
        delete[](unsigned char*)GetData()->pUTF8Data;
    GetData()->pUTF8Data = NULL;
#ifndef SQLAPI_WINDOWS
    if( GetData()->pUTF16Data )
        delete[] (unsigned char*)GetData()->pUTF16Data;
    GetData()->pUTF16Data = NULL;
#endif
    // if the buffer is too small, or we have a width mis-match, just
    //   allocate a new buffer (slow but sure)
    size_t nNewLenInBytes = GetBinaryLength() + nSrcLenInBytes;
    size_t nBinaryDataLengthDiff = nNewLenInBytes % sizeof(SAChar);
    // allocate at least so many characters that all binary data could be stored (round up if necessary)
    size_t nNewLenInChars = nNewLenInBytes / sizeof(SAChar) + (nBinaryDataLengthDiff ? 1 : 0);
    if (GetData()->nRefs > 1 || nNewLenInChars > GetData()->nAllocLength)
    {
        // we have to grow the buffer, use the ConcatCopy routine
        SAStringData *pOldData = GetData();
        ConcatBinaryCopy(GetBinaryLength(), (const void*)m_pchData, nSrcLenInBytes, pSrcData);
        assert(pOldData != NULL);
        SAString::Release(pOldData);
    }
    else
    {
        // fast concatenation when buffer big enough
        memcpy((unsigned char*)m_pchData + GetBinaryLength(), pSrcData, nSrcLenInBytes);
        GetData()->nDataLength = nNewLenInChars;
        GetData()->nBinaryDataLengthDiff = nBinaryDataLengthDiff;
        assert(GetData()->nDataLength <= GetData()->nAllocLength);
        m_pchData[GetData()->nDataLength] = _TSA('\0');
    }
}

#endif	// SA_UNICODE

void SAString::ConcatInPlace(size_t nSrcLen, const SAChar *lpszSrcData)
{
    //  -- the main routine for += operators

    // concatenating an empty string is a no-op!
    if (nSrcLen == 0)
        return;

    // if a conversion has been made already, it is not valid any more
    if (GetData()->pConvertedData)
        delete[](unsigned char*)GetData()->pConvertedData;
    GetData()->pConvertedData = NULL;
    // whether we have binary data or not
    // it will no longer be binary after we apply SAChar* concatenation
#ifdef SA_UNICODE
    if (GetData()->pUTF8Data)
        delete[](unsigned char*)GetData()->pUTF8Data;
    GetData()->pUTF8Data = NULL;
    GetData()->nBinaryDataLengthDiff = 0l;
#endif
#ifndef SQLAPI_WINDOWS
    if( GetData()->pUTF16Data )
        delete[] (unsigned char*)GetData()->pUTF16Data;
    GetData()->pUTF16Data = NULL;
#endif

    // if the buffer is too small, or we have a width mis-match, just
    //   allocate a new buffer (slow but sure)
    if (GetData()->nRefs > 1 || GetData()->nDataLength + nSrcLen > GetData()->nAllocLength)
    {
        // we have to grow the buffer, use the ConcatCopy routine
        SAStringData *pOldData = GetData();
        ConcatCopy(GetData()->nDataLength, m_pchData, nSrcLen, lpszSrcData);
        assert(pOldData != NULL);
        SAString::Release(pOldData);
    }
    else
    {
        // fast concatenation when buffer big enough
        memcpy(m_pchData + GetData()->nDataLength, lpszSrcData, nSrcLen*sizeof(SAChar));
        GetData()->nDataLength += nSrcLen;
        assert(GetData()->nDataLength <= GetData()->nAllocLength);
        m_pchData[GetData()->nDataLength] = '\0';
    }
}

const SAString &SAString::operator+=(const SAChar *lpsz)
{
    ConcatInPlace(SafeStrlen(lpsz), lpsz);
    return *this;
}

const SAString &SAString::operator+=(SAChar ch)
{
    ConcatInPlace(1, &ch);
    return *this;
}

#ifdef SA_UNICODE
const SAString &SAString::operator+=(char ch)
{
    *this += (SAChar)ch;
    return *this;
}
#endif	// SA_UNICODE


const SAString& SAString::operator+=(const SAString &string)
{
#ifdef SA_UNICODE

    // Under Unicode we should be aware that *this or string (or both)
    // can be binary string with length not being multiply of sizeof(SAChar),
    // so we should concatinate bytes, not characters
    ConcatBinaryInPlace(string.GetBinaryLength(), (const void*)string);
    return *this;

#else	// !SA_UNICODE

    // byte == character in non-Unicode build
    ConcatInPlace(string.GetData()->nDataLength, string.m_pchData);
    return *this;

#endif	// !SA_UNICODE
}

///////////////////////////////////////////////////////////////////////////////
// Advanced direct buffer access

SAChar *SAString::GetBuffer(size_t nMinBufLength)
{
    assert(nMinBufLength <= SIZE_MAX - 1);

    if (GetData()->nRefs > 1 || nMinBufLength > GetData()->nAllocLength)
    {
        // we have to grow the buffer
        SAStringData *pOldData = GetData();
        size_t nOldLen = GetData()->nDataLength;   // AllocBuffer will tromp it
        if (nMinBufLength < nOldLen)
            nMinBufLength = nOldLen;
        AllocBuffer(nMinBufLength);
        memcpy(m_pchData, pOldData->data(), (nOldLen + 1)*sizeof(SAChar));
        GetData()->nDataLength = nOldLen;
        SAString::Release(pOldData);
    }
    assert(GetData()->nRefs <= 1);

    // return a pointer to the character storage for this string
    assert(m_pchData != NULL);
    return m_pchData;
}

void SAString::ReleaseBuffer(size_t nNewLength/* = SIZE_MAX*/)
{
    CopyBeforeWrite();  // just in case GetBuffer was not called

    if (SIZE_MAX == nNewLength)
        nNewLength = sa_strlen(m_pchData); // zero terminated

    assert(nNewLength <= GetData()->nAllocLength);
    GetData()->nDataLength = nNewLength;
    m_pchData[nNewLength] = '\0';

#ifdef SA_UNICODE
    GetData()->nBinaryDataLengthDiff = 0l;
#endif	// SA_UNICODE
}

SAChar *SAString::LockBuffer()
{
    SAChar *lpsz = GetBuffer(0);
    GetData()->nRefs = -1;
    return lpsz;
}

void SAString::UnlockBuffer()
{
    assert(GetData()->nRefs == -1);
    if (GetData() != _saDataNil)
        GetData()->nRefs = 1;
}

void SAString::CopyBeforeWrite()
{
    if (GetData()->nRefs > 1)
    {
        SAStringData *pData = GetData();
        Release();
        AllocBuffer(pData->nDataLength);
        memcpy(m_pchData, pData->data(), (pData->nDataLength + 1)*sizeof(SAChar));
    }
    // start of SAString special
    else
    {
        // if a conversion has been made already, it is not valid any more
        if (GetData()->pConvertedData)
            delete[](unsigned char*)GetData()->pConvertedData;
        GetData()->pConvertedData = NULL;

#ifdef SA_UNICODE
        if (GetData()->pUTF8Data)
            delete[](unsigned char*)GetData()->pUTF8Data;
        GetData()->pUTF8Data = NULL;
        GetData()->nBinaryDataLengthDiff = 0l;
#endif	// SA_UNICODE
#ifndef SQLAPI_WINDOWS
        if( GetData()->pUTF16Data )
            delete[] (unsigned char*)GetData()->pUTF16Data;
        GetData()->pUTF16Data = NULL;
#endif
    }
    // end of SAString special
    assert(GetData()->nRefs <= 1);
}

void SAString::AllocBeforeWrite(size_t nLen)
{
    if (GetData()->nRefs > 1 || nLen > GetData()->nAllocLength)
    {
        Release();
        AllocBuffer(nLen);
    }
    // start of SAString special
    else
    {
        // if a conversion has been made already, it is not valid any more
        if (GetData()->pConvertedData)
            delete[](unsigned char*)GetData()->pConvertedData;
        GetData()->pConvertedData = NULL;

#ifdef SA_UNICODE
        if (GetData()->pUTF8Data)
            delete[](unsigned char*)GetData()->pUTF8Data;
        GetData()->pUTF8Data = NULL;
        GetData()->nBinaryDataLengthDiff = 0l;
#endif	// SA_UNICODE
#ifndef SQLAPI_WINDOWS
        if( GetData()->pUTF16Data )
            delete[] (unsigned char*)GetData()->pUTF16Data;
        GetData()->pUTF16Data = NULL;
#endif
    }
    // end of SAString special
    assert(GetData()->nRefs <= 1);
}

void SAString::Release()
{
    if (GetData() != _saDataNil)
    {
        assert(GetData()->nRefs != 0);
#ifdef SQLAPI_WINDOWS
        if (InterlockedDecrement(&GetData()->nRefs) <= 0)
#else
        if(--GetData()->nRefs <= 0)
#endif	// !defined(SQLAPI_WINDOWS)
            FreeData(GetData());
        Init();
    }
}

/*static */
void SAString::Release(SAStringData *pData)
{
    if (pData != _saDataNil)
    {
        assert(pData->nRefs != 0);
#ifdef SQLAPI_WINDOWS
        if (InterlockedDecrement(&pData->nRefs) <= 0)
#else
        if(--pData->nRefs <= 0)
#endif	// !defined(SQLAPI_WINDOWS)
            FreeData(pData);
    }
}

void SAString::Init()
{
    m_pchData = saEmptyString.m_pchData;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SAStringData *SAString::GetData() const
{
    assert(m_pchData != NULL);
    return ((SAStringData *)m_pchData) - 1;
}

SAString::SAString()
{
    Init();
}

SAString::SAString(const SAString &stringSrc)
{
    assert(stringSrc.GetData()->nRefs != 0);
    if (stringSrc.GetData()->nRefs >= 0)
    {
        assert(stringSrc.GetData() != _saDataNil);
        m_pchData = stringSrc.m_pchData;
#ifdef SQLAPI_WINDOWS
        InterlockedIncrement(&GetData()->nRefs);
#else
        ++GetData()->nRefs;
#endif	// !defined(SQLAPI_WINDOWS)
    }
    else
    {
        Init();
        *this = stringSrc;	//*this = stringSrc.m_pchData; - not right, will not copy strings with embedded '\0'
    }
}

SAString::SAString(const SAChar *lpsz)
{
    Init();
    size_t nLen = SafeStrlen(lpsz);
    if (nLen != 0)
    {
        AllocBuffer(nLen);
        memcpy(m_pchData, lpsz, nLen*sizeof(SAChar));
    }
}

/////////////////////////////////////////////////////////////////////////////
// Special conversion constructors

#ifdef SA_UNICODE

SAString::SAString(const char *lpsz)
{
    Init();
    size_t nSrcLen = lpsz != NULL ? strlen(lpsz) : 0;
    if (nSrcLen != 0)
    {
        AllocBuffer(nSrcLen);
        ReleaseBuffer(SAMultiByteToWideChar(m_pchData, lpsz, nSrcLen));
    }
}

SAString::SAString(const char *lpch, size_t nLength)
{
    Init();
    if (nLength != 0)
    {
        AllocBuffer(nLength);
        ReleaseBuffer(SAMultiByteToWideChar(m_pchData, lpch, nLength));
    }
}

#else	// !SA_UNICODE

SAString::SAString(const wchar_t *lpsz)
{
    Init();
    int nSrcLen = lpsz != NULL ? (int)wcslen(lpsz) : 0;
    if (nSrcLen != 0)
    {
        AllocBuffer(nSrcLen*MB_CUR_MAX);
        ReleaseBuffer(SAWideCharToMultiByte(m_pchData, lpsz, nSrcLen));
    }
}

SAString::SAString(const wchar_t *lpch, size_t nLength)
{
    Init();
    if (nLength != 0)
    {
        AllocBuffer(nLength*MB_CUR_MAX);
        ReleaseBuffer(SAWideCharToMultiByte(m_pchData, lpch, nLength));
    }
}

#endif //!SA_UNICODE

SAString::SAString(const SAChar *lpch, size_t nLength)
{
    Init();
    if (nLength != 0)
    {
        assert(lpch);
        AllocBuffer(nLength);
        memcpy(m_pchData, lpch, nLength*sizeof(SAChar));
    }
}

SAString::SAString(const unsigned char *psz)
{
    Init();
    *this = (const char*)psz;
}

//////////////////////////////////////////////////////////////////////////////
// More sophisticated construction

SAString::SAString(SAChar ch, size_t nRepeat/* = 1*/)
{
    Init();
    if (nRepeat >= 1)
    {
        AllocBuffer(nRepeat);
#ifdef SA_UNICODE
        for (size_t i = 0; i < nRepeat; ++i)
            m_pchData[i] = ch;
#else
        memset(m_pchData, ch, nRepeat);
#endif
    }
}

SAString::~SAString()
{
    if (GetData() != _saDataNil)
    {
        //  free any attached data
#ifdef SQLAPI_WINDOWS
        if (InterlockedDecrement(&GetData()->nRefs) <= 0)
#else
        if(--GetData()->nRefs <= 0)
#endif	// !defined(SQLAPI_WINDOWS)
            FreeData(GetData());
    }
}

void SAString::Empty()
{
    if (GetData()->nDataLength == 0)
        return;

    if (GetData()->nRefs >= 0)
        Release();
    else
        *this = _saPchNil;

    assert(GetData()->nDataLength == 0);
    assert(GetData()->nRefs < 0 || GetData()->nAllocLength == 0);
}

/*!
* \returns
* Nonzero if the SAString object has 0 length; otherwise 0.
*
* \remarks
* Tests a SAString object for the empty condition.
*
* \see
* GetLength | GetMultiByteCharsLength | GetWideCharsLength
*/
bool SAString::IsEmpty() const
{
    return GetData()->nDataLength == 0;
}

/*!
* \returns
* Returns the number of characters in a SAString object.
*
* \remarks
* Call this member function to get a count of characters in this SAString object.
* The count does not include a null terminator.
* For multibyte character sets, GetLength counts each 8-bit character; that is, a
* lead and trail byte in one multibyte character are counted as two bytes.
*
* \see
* IsEmpty
*/
size_t SAString::GetLength() const
{
    return GetData()->nDataLength;
}

SAString::operator const SAChar *() const
{
    return m_pchData;
}

//////////////////////////////////////////////////////////////////////////////
// Assignment operators
//  All assign a new value to the string
//      (a) first see if the buffer is big enough
//      (b) if enough room, copy on top of old buffer, set size and type
//      (c) otherwise free old string data, and create a new one
//
//  All routines return the new string (but as a 'const SAString&' so that
//      assigning it again will cause a copy, eg: s1 = s2 = "hi there".
//

#ifdef SA_UNICODE

void SAString::AssignBinaryCopy(size_t nSrcLenInBytes, const void *pSrcData)
{
    size_t nBinaryDataLengthDiff = nSrcLenInBytes % sizeof(SAChar);
    // allocate at least so many characters that all binary data could be stored (round up if necessary)
    size_t nSrcLenInChars = nSrcLenInBytes / sizeof(SAChar) + (nBinaryDataLengthDiff ? 1 : 0);

    AllocBeforeWrite(nSrcLenInChars);
    memcpy(m_pchData, pSrcData, nSrcLenInBytes);
    GetData()->nDataLength = nSrcLenInChars;
    m_pchData[nSrcLenInChars] = '\0';

    // if nSrcLenInBytes is not a multiply of sizeof(SAChar)
    // we have to save the difference
    GetData()->nBinaryDataLengthDiff = nBinaryDataLengthDiff;
}

#endif	// SA_UNICODE

void SAString::AssignCopy(size_t nSrcLen, const SAChar *lpszSrcData)
{
    AllocBeforeWrite(nSrcLen);
    memcpy(m_pchData, lpszSrcData, nSrcLen*sizeof(SAChar));
    GetData()->nDataLength = nSrcLen;
    m_pchData[nSrcLen] = '\0';
}

const SAString &SAString::operator=(const SAString &stringSrc)
{
    if (m_pchData != stringSrc.m_pchData)
    {
        if ((GetData()->nRefs < 0 && GetData() != _saDataNil) ||
            stringSrc.GetData()->nRefs < 0)
        {
            // actual copy necessary since one of the strings is locked
#ifdef SA_UNICODE

            // Under Unicode we should be aware that stringSrc
            // can be binary string with length not being multiply of sizeof(SAChar),
            // so we should copy bytes, not characters
            AssignBinaryCopy(stringSrc.GetBinaryLength(), (const void*)stringSrc);

#else	// !SA_UNICODE

            AssignCopy(stringSrc.GetData()->nDataLength, stringSrc.m_pchData);

#endif	// !SA_UNICODE
        }
        else
        {
            // can just copy references around
            Release();
            assert(stringSrc.GetData() != _saDataNil);
            m_pchData = stringSrc.m_pchData;
#ifdef SQLAPI_WINDOWS
            InterlockedIncrement(&GetData()->nRefs);
#else
            ++GetData()->nRefs;
#endif	// !defined(SQLAPI_WINDOWS)
        }
    }
    return *this;
}

const SAString &SAString::operator =(const SAChar *lpsz)
{
    AssignCopy(SafeStrlen(lpsz), lpsz);
    return *this;
}

/////////////////////////////////////////////////////////////////////////////
// Special conversion assignment

#ifdef SA_UNICODE

const SAString &SAString::operator=(const char *lpsz)
{
    size_t nSrcLen = lpsz ? strlen(lpsz) : 0;
    AllocBeforeWrite(nSrcLen);
    ReleaseBuffer(SAMultiByteToWideChar(m_pchData, lpsz, nSrcLen));
    return *this;
}

#else //!SA_UNICODE

const SAString &SAString::operator=(const wchar_t *lpsz)
{
    int nSrcLen = lpsz ? (int)wcslen(lpsz) : 0;
    AllocBeforeWrite(nSrcLen * 2);
    ReleaseBuffer(SAWideCharToMultiByte(m_pchData, lpsz, nSrcLen));
    return *this;
}

#endif  //!SA_UNICODE

const SAString &SAString::operator=(SAChar ch)
{
    AssignCopy(1, &ch);
    return *this;
}

#ifdef SA_UNICODE

const SAString &SAString::operator=(char ch)
{
    *this = (SAChar)ch;
    return *this;
}

#endif

const SAString &SAString::operator=(const unsigned char *psz)
{
    *this = (const char *)psz;
    return *this;
}

//////////////////////////////////////////////////////////////////////////////
// concatenation

// NOTE: "operator+" is done as friend functions for simplicity
//      There are three variants:
//          SAString + SAString
// and for ? = SAChar, const SAChar*
//          SAString + ?
//          ? + CString

#ifdef SA_UNICODE

void SAString::ConcatBinaryCopy(size_t nSrc1LenInBytes, const void *pSrc1Data,
    size_t nSrc2LenInBytes, const void *pSrc2Data)
{
    // -- master concatenation routine (binary)
    // Concatenate two sources
    // -- assume that 'this' is a new SAString object

    size_t nNewLenInBytes = nSrc1LenInBytes + nSrc2LenInBytes;
    if (nNewLenInBytes != 0)
    {
        size_t nBinaryDataLengthDiff = nNewLenInBytes % sizeof(SAChar);
        // allocate at least so many characters that all binary data could be stored (round up if necessary)
        size_t nNewLenInChars = nNewLenInBytes / sizeof(SAChar) + (nBinaryDataLengthDiff ? 1 : 0);

        AllocBuffer(nNewLenInChars);
        // if nNewLengthInBytes is not a multiply of sizeof(SAChar)
        // we have to save the difference
        GetData()->nBinaryDataLengthDiff = nBinaryDataLengthDiff;
        memcpy((unsigned char*)m_pchData, pSrc1Data, nSrc1LenInBytes);
        memcpy((unsigned char*)m_pchData + nSrc1LenInBytes, pSrc2Data, nSrc2LenInBytes);
    }
}

#endif	// SA_UNICODE

void SAString::ConcatCopy(size_t nSrc1Len, const SAChar *lpszSrc1Data,
    size_t nSrc2Len, const SAChar *lpszSrc2Data)
{
    // -- master concatenation routine
    // Concatenate two sources
    // -- assume that 'this' is a new SAString object

    size_t nNewLen = nSrc1Len + nSrc2Len;
    if (nNewLen != 0)
    {
        AllocBuffer(nNewLen);
        memcpy(m_pchData, lpszSrc1Data, nSrc1Len*sizeof(SAChar));
        memcpy(m_pchData + nSrc1Len, lpszSrc2Data, nSrc2Len*sizeof(SAChar));
    }
}

SAString operator+(const SAString &string1, const SAString &string2)
{
#ifdef SA_UNICODE

    // Under Unicode we should be aware that *this or string (or both)
    // can be binary string with length not being multiply of sizeof(SAChar),
    // so we should concatinate bytes, not characters
    SAString s;
    s.ConcatBinaryCopy(string1.GetBinaryLength(), (const void*)string1,
        string2.GetBinaryLength(), (const void*)string2);
    return s;

#else	// !SA_UNICODE

    SAString s;
    s.ConcatCopy(string1.GetData()->nDataLength, string1.m_pchData,
        string2.GetData()->nDataLength, string2.m_pchData);
    return s;

#endif	// !SA_UNICODE
}

SAString operator+(const SAString &string, const SAChar *lpsz)
{
    SAString s;
    s.ConcatCopy(string.GetData()->nDataLength, string.m_pchData,
        SAString::SafeStrlen(lpsz), lpsz);
    return s;
}

SAString operator+(const SAChar *lpsz, const SAString &string)
{
    SAString s;
    s.ConcatCopy(SAString::SafeStrlen(lpsz), lpsz, string.GetData()->nDataLength,
        string.m_pchData);
    return s;
}

SAString operator+(const SAString &string1, SAChar ch)
{
    SAString s;
    s.ConcatCopy(string1.GetData()->nDataLength, string1.m_pchData, 1, &ch);
    return s;
}

SAString operator+(SAChar ch, const SAString &string)
{
    SAString s;
    s.ConcatCopy(1, &ch, string.GetData()->nDataLength, string.m_pchData);
    return s;
}

#ifdef SA_UNICODE
SAString operator+(const SAString &string, char ch)
{
    return string + (SAChar)ch;
}

SAString operator+(char ch, const SAString &string)
{
    return (SAChar)ch + string;
}
#endif

/////////////////////////////////////////////////////////////////////////////
// SAString formatting

#define FORCE_ANSI      0x10000
#define FORCE_UNICODE   0x20000
#define FORCE_INT64     0x40000

void SAString::FormatV(const SAChar *lpszFormat, va_list argList)
{
    assert(lpszFormat);

    va_list argListSave;
#ifdef va_copy
    va_copy(argListSave, argList);
#else
#	ifdef __va_copy
    __va_copy(argListSave, argList);
#	else
    memcpy(&argListSave, &argList, sizeof(va_list));
#	endif
#endif

    // make a guess at the maximum length of the resulting string
    size_t nMaxLen = 0l;
    for (const SAChar *lpsz = lpszFormat; *lpsz != '\0'; /*lpsz = */sa_csinc(lpsz))
    {
        // handle '%' character, but watch out for '%%'
        if (*lpsz != '%' || *(/*lpsz = */sa_csinc(lpsz)) == '%')
        {
            nMaxLen += sa_clen(lpsz);
            continue;
        }

        size_t nItemLen = 0l;

        // handle '%' character with format
        int nWidth = 0;
        for (; *lpsz != '\0'; /*lpsz = */sa_csinc(lpsz))
        {
            // check for valid flags
            if (*lpsz == '#')
                nMaxLen += 2;   // for '0x'
            else if (*lpsz == '*')
                nWidth = va_arg(argList, int);
            else if (*lpsz == '-' || *lpsz == '+' || *lpsz == '0' ||
                *lpsz == ' ')
                ;
            else // hit non-flag character
                break;
        }
        // get width and skip it
        if (nWidth == 0)
        {
            // width indicated by
            nWidth = sa_toi(lpsz);
            for (; *lpsz != '\0' && sa_isdigit(*lpsz); /*lpsz = */sa_csinc(lpsz))
                ;
        }
        assert(nWidth >= 0);

        int nPrecision = 0;
        if (*lpsz == _TSA('.'))
        {
            // skip past '.' separator (width.precision)
            /*lpsz = */sa_csinc(lpsz);

            // get precision and skip it
            if (*lpsz == '*')
            {
                nPrecision = va_arg(argList, int);
                /*lpsz = */sa_csinc(lpsz);
            }
            else
            {
                nPrecision = sa_toi(lpsz);
                for (; *lpsz != '\0' && sa_isdigit(*lpsz); /*lpsz = */sa_csinc(lpsz))
                    ;
            }
            assert(nPrecision >= 0);
        }

        // should be on type modifier or specifier
        int nModifier = 0;
        if (sa_strncmp(lpsz, _TSA("I64"), 3) == 0)
        {
            lpsz += 3;
            nModifier = FORCE_INT64;
        }
        else if (sa_strncmp(lpsz, _TSA("ll"), 2) == 0)
        {
            lpsz += 2;
            nModifier = FORCE_INT64;
        }
        else
        {
            switch (*lpsz)
            {
                // modifiers that affect size
            case 'h':
                nModifier = FORCE_ANSI;
                /*lpsz = */sa_csinc(lpsz);
                break;
            case 'l':
                nModifier = FORCE_UNICODE;
                /*lpsz = */sa_csinc(lpsz);
                break;

                // modifiers that do not affect size
            case 'F':
            case 'N':
            case 'L':
                /*lpsz = */sa_csinc(lpsz);
                break;
            }
        }

        // now should be on specifier
        switch (*lpsz | nModifier)
        {
            // single characters
        case 'c':
        case 'C':
            nItemLen = 2;
            // int is OK
            // need a cast here since va_arg only
            // takes fully promoted types
            (void)va_arg(argList, int/*SAChar*/);
            break;
        case 'c' | FORCE_ANSI:
        case 'C' | FORCE_ANSI:
            nItemLen = 2;
            // int is OK
            // need a cast here since va_arg only
            // takes fully promoted types
            (void)va_arg(argList, int/*char*/);
            break;
        case 'c' | FORCE_UNICODE:
        case 'C' | FORCE_UNICODE:
            nItemLen = 2;
            // int is OK
            // need a cast here since va_arg only
            // takes fully promoted types
            (void)va_arg(argList, int/*wchar_t*/);
            break;

            // strings
        case 's':
        {
#ifdef SQLAPI_WINDOWS
            const SAChar *pstrNextArg = va_arg(argList, const SAChar*);
            if (pstrNextArg == NULL)
                nItemLen = 6;  // "(null)"
            else
            {
                nItemLen = sa_strlen(pstrNextArg);
                nItemLen = sa_max(1, nItemLen);
            }
#else
            const char *pstrNextArg = va_arg(argList, const char*);
            if (pstrNextArg == NULL)
                nItemLen = 6; // "(null)"
            else
            {
                nItemLen = strlen(pstrNextArg);
                nItemLen = sa_max(1, nItemLen);
            }
#endif
        }
        break;

        case 'S':
        {
#if !defined(SA_UNICODE) || !defined(SQLAPI_WINDOWS)
            wchar_t *pstrNextArg = va_arg(argList, wchar_t*);
            if (pstrNextArg == NULL)
                nItemLen = 6;  // "(null)"
            else
            {
                nItemLen = wcslen(pstrNextArg);
                nItemLen = sa_max(1, nItemLen);
            }
#else
            const char *pstrNextArg = va_arg(argList, const char*);
            if (pstrNextArg == NULL)
                nItemLen = 6; // "(null)"
            else
            {
                nItemLen = strlen(pstrNextArg);
                nItemLen = sa_max(1, nItemLen);
            }
#endif
        }
        break;

        case 's' | FORCE_ANSI:
        case 'S' | FORCE_ANSI:
        {
            const char *pstrNextArg = va_arg(argList, const char*);
            if (pstrNextArg == NULL)
                nItemLen = 6; // "(null)"
            else
            {
                nItemLen = strlen(pstrNextArg);
                nItemLen = sa_max(1, nItemLen);
            }
        }
        break;

        case 's' | FORCE_UNICODE:
        case 'S' | FORCE_UNICODE:
        {
            wchar_t *pstrNextArg = va_arg(argList, wchar_t*);
            if (pstrNextArg == NULL)
                nItemLen = 6; // "(null)"
            else
            {
                nItemLen = wcslen(pstrNextArg);
                nItemLen = sa_max(1, nItemLen);
            }
        }
        break;
        }

        // adjust nItemLen for strings
        if (nItemLen != 0)
        {
            if (nPrecision != 0)
                nItemLen = sa_min(nItemLen, (size_t)nPrecision);
            nItemLen = sa_max(nItemLen, (size_t)nWidth);
        }
        else
        {
            switch (*lpsz)
            {
                // integers
            case 'd':
            case 'i':
            case 'u':
            case 'x':
            case 'X':
            case 'o':
                if (nModifier & FORCE_INT64)
                    (void)va_arg(argList, sa_int64_t);
                else
                    (void)va_arg(argList, int);
                nItemLen = 32;
                nItemLen = sa_max(nItemLen, (size_t)(nWidth + nPrecision));
                break;

            case 'e':
            case 'g':
            case 'G':
                (void)va_arg(argList, double);
                nItemLen = 128;
                nItemLen = sa_max(nItemLen, (size_t)(nWidth + nPrecision));
                break;

            case 'f':
            {
                double f;
                SAChar *pszTemp;

                // 312 == strlen("-1+(309 zeroes).")
                // 309 zeroes == max precision of a double
                // 6 == adjustment in case precision is not specified,
                //   which means that the precision defaults to 6
                size_t n = sa_max(nWidth, 312 + nPrecision + 6);
                pszTemp = (SAChar *)malloc(n);
                f = va_arg(argList, double);
#if defined(__BORLANDC__) && (__BORLANDC__  <= 0x0520)
                sprintf(pszTemp, _TSA( "%*.*f" ), nWidth, nPrecision+6, f);
#else				
                sa_snprintf(pszTemp, n - 1, _TSA("%*.*f"), nWidth, nPrecision + 6, f);
#endif
                nItemLen = (int)sa_strlen(pszTemp);
                ::free(pszTemp);
            }
            break;

            case 'p':
                (void)va_arg(argList, void*);
                nItemLen = 32;
                nItemLen = sa_max(nItemLen, (size_t)(nWidth + nPrecision));
                break;

                // no output
            case 'n':
                (void)va_arg(argList, int*);
                break;

            default:
                assert(false);  // unknown formatting option
            }
        }

        // adjust nMaxLen for output nItemLen
        nMaxLen += nItemLen;
    }

    // if the buffer is too small, or we have a width mis-match, just
    // allocate a new buffer (slow but sure)
    if (GetData()->nRefs > 1 || nMaxLen > GetData()->nAllocLength)
    {
        // we have to grow the buffer
        Release();
        AllocBuffer(nMaxLen);
    }

    int newLength = sa_vsnprintf(m_pchData, nMaxLen + 1, lpszFormat, argListSave);
    assert(newLength >= 0 && (size_t)newLength <= nMaxLen);
    ReleaseBuffer(newLength);

    va_end(argListSave);
}

// formatting (using wsprintf style formatting)
void SAString::Format(const SAChar *lpszFormat, ...)
{
    assert(lpszFormat);

    va_list argList;
    va_start(argList, lpszFormat);
    FormatV(lpszFormat, argList);
    va_end(argList);
}

/*static */
size_t SAString::SafeStrlen(const SAChar *lpsz)
{
    return (lpsz == NULL) ? 0 : (int)sa_strlen(lpsz);
}

SAString SAString::Mid(size_t nFirst) const
{
    return Mid(nFirst, GetData()->nDataLength - nFirst);
}

SAString SAString::Mid(size_t nFirst, size_t nCount) const
{
    if (SIZE_MAX == nFirst)
        nFirst = 0;

    // optimize case of returning empty string
    if (nFirst > GetLength())
        return _saPchNil;

    // out-of-bounds requests return sensible things
    if (SIZE_MAX == nCount)
        nCount = 0l;

    if (nFirst + nCount > GetLength())
        nCount = GetLength() - nFirst;
    if (nFirst > GetLength())
        nCount = 0l;

    assert(nFirst + nCount <= GetData()->nDataLength);

    // optimize case of returning entire string
    if (nFirst == 0 && nFirst + nCount == GetData()->nDataLength)
        return *this;

    SAString dest(m_pchData + nFirst, nCount);
    return dest;
}

void SAString::MakeUpper()
{
    CopyBeforeWrite();
    sa_strupr(m_pchData);
}

void SAString::MakeLower()
{
    CopyBeforeWrite();
    sa_strlwr(m_pchData);
}

SAString SAString::Right(size_t nCount) const
{
    if (nCount == SIZE_MAX)
        nCount = 0l;
    if (nCount >= GetData()->nDataLength)
        return *this;

    SAString dest(m_pchData + GetData()->nDataLength - nCount, nCount);
    return dest;
}

SAString SAString::Left(size_t nCount) const
{
    if (nCount == SIZE_MAX)
        nCount = 0l;
    if (nCount >= GetData()->nDataLength)
        return *this;

    SAString dest(m_pchData, nCount);
    return dest;
}

SAChar SAString::GetAt(size_t nPos) const
{
    if (nPos >= GetLength())
        return _TSA('\0');
    return ((const SAChar*)*this)[nPos];
}

//////////////////////////////////////////////////////////////////////////////
// Advanced manipulation

size_t SAString::Delete(size_t nIndex, size_t nCount /* = 1 */)
{
    if (nIndex == SIZE_MAX)
        nIndex = 0l;
    size_t nNewLength = GetData()->nDataLength;
    if (nCount > 0 && nIndex < nNewLength)
    {
        CopyBeforeWrite();
        size_t nBytesToCopy = nNewLength - (nIndex + nCount) + 1;

        memmove(m_pchData + nIndex,
            m_pchData + nIndex + nCount, nBytesToCopy * sizeof(SAChar));
        GetData()->nDataLength = nNewLength - nCount;
    }

    return nNewLength;
}

size_t SAString::Insert(size_t nIndex, SAChar ch)
{
    CopyBeforeWrite();

    if (nIndex == SIZE_MAX)
        nIndex = 0l;

    size_t nNewLength = GetData()->nDataLength;
    if (nIndex > nNewLength)
        nIndex = nNewLength;
    nNewLength++;

    if (GetData()->nAllocLength < nNewLength)
    {
        SAStringData *pOldData = GetData();
        SAChar *pstr = m_pchData;
        AllocBuffer(nNewLength);
        memcpy(m_pchData, pstr, (pOldData->nDataLength + 1)*sizeof(SAChar));
        SAString::Release(pOldData);
    }

    // move existing bytes down
    memmove(m_pchData + nIndex + 1,
        m_pchData + nIndex, (nNewLength - nIndex)*sizeof(SAChar));
    m_pchData[nIndex] = ch;
    GetData()->nDataLength = nNewLength;

    return nNewLength;
}

size_t SAString::Insert(size_t nIndex, const SAChar *pstr)
{
    if (SIZE_MAX == nIndex)
        nIndex = 0l;

    size_t nInsertLength = SafeStrlen(pstr);
    size_t nNewLength = GetData()->nDataLength;
    if (nInsertLength > 0)
    {
        CopyBeforeWrite();
        if (nIndex > nNewLength)
            nIndex = nNewLength;
        nNewLength += nInsertLength;

        if (GetData()->nAllocLength < nNewLength)
        {
            SAStringData *pOldData = GetData();
            SAChar* pstr2 = m_pchData;
            AllocBuffer(nNewLength);
            memcpy(m_pchData, pstr2, (pOldData->nDataLength + 1)*sizeof(SAChar));
            SAString::Release(pOldData);
        }

        // move existing bytes down
        memmove(m_pchData + nIndex + nInsertLength,
            m_pchData + nIndex,
            (nNewLength - nIndex - nInsertLength + 1)*sizeof(SAChar));
        memcpy(m_pchData + nIndex,
            pstr, nInsertLength*sizeof(SAChar));
        GetData()->nDataLength = nNewLength;
    }

    return nNewLength;
}

size_t SAString::Replace(const SAChar *lpszOld, const SAChar *lpszNew)
{
    // can't have empty or NULL lpszOld

    size_t nSourceLen = SafeStrlen(lpszOld);
    if (nSourceLen == 0)
        return 0l;
    size_t nReplacementLen = SafeStrlen(lpszNew);

    // loop once to figure out the size of the result string
    size_t nCount = 0l;
    SAChar *lpszStart = m_pchData;
    SAChar *lpszEnd = m_pchData + GetData()->nDataLength;
    SAChar *lpszTarget, *lpszNewTarget;
    while (lpszStart < lpszEnd)
    {
        while ((lpszTarget = sa_strstr(lpszStart, lpszOld)) != NULL)
        {
            nCount++;
            lpszStart = lpszTarget + nSourceLen;
        }
        lpszStart += sa_strlen(lpszStart) + 1;
    }

    // if any changes were made, make them
    if (nCount > 0)
    {
        CopyBeforeWrite();

        // if the buffer is too small, just
        //   allocate a new buffer (slow but sure)
        size_t nOldLength = GetData()->nDataLength;
        size_t nNewLength = nOldLength + (nReplacementLen - nSourceLen)*nCount;
        if (GetData()->nAllocLength < nNewLength || GetData()->nRefs > 1)
        {
            SAStringData *pOldData = GetData();
            SAChar *pstr = m_pchData;
            AllocBuffer(nNewLength);
            memcpy(m_pchData, pstr, pOldData->nDataLength*sizeof(SAChar));
            SAString::Release(pOldData);
        }
        // else, we just do it in-place
        lpszStart = lpszNewTarget = m_pchData;
        if (nOldLength < nNewLength)
        {
            memmove(m_pchData + (nNewLength - nOldLength), m_pchData,
                nOldLength * sizeof(SAChar));
            lpszStart = m_pchData + (nNewLength - nOldLength);
        }
        lpszEnd = m_pchData + GetData()->nDataLength;
        lpszStart[nOldLength] = '\0';

        // loop again to actually do the work
        while (lpszStart < lpszEnd)
        {
            while ((lpszTarget = sa_strstr(lpszStart, lpszOld)) != NULL)
            {
                int nBalance = (int)(lpszTarget - lpszStart);
                if (lpszStart != lpszNewTarget)
                    memmove(lpszNewTarget, lpszStart, nBalance*sizeof(SAChar));
                lpszNewTarget += nBalance;

                memcpy(lpszNewTarget, lpszNew, nReplacementLen*sizeof(SAChar));
                lpszNewTarget += nReplacementLen;
                lpszStart = lpszTarget + nSourceLen;
            }
            if (lpszStart < lpszEnd)
                memmove(lpszNewTarget, lpszStart, (lpszEnd - lpszStart)*sizeof(SAChar));
            lpszStart += sa_strlen(lpszStart) + 1;
        }
        if (nOldLength > nNewLength)
            m_pchData[nNewLength] = '\0';
        assert(m_pchData[nNewLength] == '\0');
        GetData()->nDataLength = nNewLength;
    }

    return nCount;
}

void SAString::TrimRight(const SAChar *lpszTargetList)
{
    // find beginning of trailing matches
    // by starting at beginning (DBCS aware)

    CopyBeforeWrite();
    SAChar *lpsz = m_pchData;
    SAChar *lpszLast = NULL;

    while (*lpsz != '\0')
    {
        if (sa_strchr(lpszTargetList, *lpsz) != NULL)
        {
            if (lpszLast == NULL)
                lpszLast = lpsz;
        }
        else
            lpszLast = NULL;
        ++lpsz;
    }

    if (lpszLast != NULL)
    {
        // truncate at left-most matching character
        *lpszLast = '\0';
        GetData()->nDataLength = (int)(lpszLast - m_pchData);
    }
}

void SAString::TrimRight(SAChar chTarget)
{
    // find beginning of trailing matches
    // by starting at beginning (DBCS aware)

    CopyBeforeWrite();
    SAChar *lpsz = m_pchData;
    SAChar *lpszLast = NULL;

    while (*lpsz != '\0')
    {
        if (*lpsz == chTarget)
        {
            if (lpszLast == NULL)
                lpszLast = lpsz;
        }
        else
            lpszLast = NULL;
        ++lpsz;
    }

    if (lpszLast != NULL)
    {
        // truncate at left-most matching character
        *lpszLast = '\0';
        GetData()->nDataLength = (int)(lpszLast - m_pchData);
    }
}

void SAString::TrimRight()
{
    // find beginning of trailing spaces by starting at beginning (DBCS aware)

    CopyBeforeWrite();
    SAChar *lpsz = m_pchData;
    SAChar *lpszLast = NULL;

    while (*lpsz != '\0')
    {
        if (sa_isspace(*lpsz))
        {
            if (lpszLast == NULL)
                lpszLast = lpsz;
        }
        else
            lpszLast = NULL;
        ++lpsz;
    }

    if (lpszLast != NULL)
    {
        // truncate at trailing space start
        *lpszLast = '\0';
        GetData()->nDataLength = (int)(lpszLast - m_pchData);
    }
}

void SAString::TrimLeft(const SAChar *lpszTargets)
{
    // if we're not trimming anything, we're not doing any work
    if (SafeStrlen(lpszTargets) == 0)
        return;

    CopyBeforeWrite();
    const SAChar *lpsz = m_pchData;

    while (*lpsz != '\0')
    {
        if (sa_strchr(lpszTargets, *lpsz) == NULL)
            break;
        ++lpsz;
    }

    if (lpsz != m_pchData)
    {
        // fix up data and length
        size_t nDataLength = GetData()->nDataLength - (lpsz - m_pchData);
        memmove(m_pchData, lpsz, (nDataLength + 1)*sizeof(SAChar));
        GetData()->nDataLength = nDataLength;
    }
}

void SAString::TrimLeft(SAChar chTarget)
{
    // find first non-matching character

    CopyBeforeWrite();
    const SAChar *lpsz = m_pchData;

    while (chTarget == *lpsz)
        ++lpsz;

    if (lpsz != m_pchData)
    {
        // fix up data and length
        size_t nDataLength = GetData()->nDataLength - (lpsz - m_pchData);
        memmove(m_pchData, lpsz, (nDataLength + 1)*sizeof(SAChar));
        GetData()->nDataLength = nDataLength;
    }
}

void SAString::TrimLeft()
{
    // find first non-space character

    CopyBeforeWrite();
    const SAChar *lpsz = m_pchData;

    while (sa_isspace(*lpsz))
        ++lpsz;

    if (lpsz != m_pchData)
    {
        // fix up data and length
        size_t nDataLength = GetData()->nDataLength - (lpsz - m_pchData);
        memmove(m_pchData, lpsz, (nDataLength + 1)*sizeof(SAChar));
        GetData()->nDataLength = nDataLength;
    }
}

int SAString::Compare(const SAChar *lpsz) const
{
    assert(lpsz);
    return sa_strcmp(m_pchData, lpsz);
}

int SAString::CompareNoCase(const SAChar *lpsz) const
{
    assert(lpsz);

    const SAChar *psThis = m_pchData;
    const SAChar *psOther = lpsz;

#if defined(SA_STRING_EXT) && ! defined(SQLAPI_SOLARIS) && ! defined(SQLAPI_OSX)
    return sa_stricmp(psThis, psOther);
#else
    // implement our own case-insensitive comparison
    // by converting to lower case before comparison
    while (*psThis && *psOther)
    {
        int iThis = sa_tolower(*psThis);
        int iOther = sa_tolower(*psOther);
        if (iThis == iOther)
        {
            ++psThis;
            ++psOther;
            continue;
        }

        if (iThis > iOther)
            return 1;
        return -1;
    }

    if (*psThis && !*psOther)
        return 1;
    if (!*psThis && *psOther)
        return -1;
    assert(!*psThis && !*psOther);
    return 0;
#endif
}

#ifndef UNDER_CE
int SAString::Collate(const SAChar *lpsz) const
{
    assert(lpsz);
    return sa_strcoll(m_pchData, lpsz);
}
#endif

bool operator==(const SAString &s1, const SAString &s2)
{
    return s1.Compare(s2) == 0;
}
bool operator==(const SAString &s1, const SAChar *s2)
{
    return s1.Compare(s2) == 0;
}
bool operator==(const SAChar *s1, const SAString &s2)
{
    return s2.Compare(s1) == 0;
}
bool operator!=(const SAString &s1, const SAString &s2)
{
    return s1.Compare(s2) != 0;
}
bool operator!=(const SAString &s1, const SAChar *s2)
{
    return s1.Compare(s2) != 0;
}
bool operator!=(const SAChar *s1, const SAString &s2)
{
    return s2.Compare(s1) != 0;
}
bool operator<(const SAString &s1, const SAString &s2)
{
    return s1.Compare(s2) < 0;
}
bool operator<(const SAString &s1, const SAChar *s2)
{
    return s1.Compare(s2) < 0;
}
bool operator<(const SAChar *s1, const SAString &s2)
{
    return s2.Compare(s1) > 0;
}
bool operator>(const SAString &s1, const SAString &s2)
{
    return s1.Compare(s2) > 0;
}
bool operator>(const SAString &s1, const SAChar *s2)
{
    return s1.Compare(s2) > 0;
}
bool operator>(const SAChar *s1, const SAString &s2)
{
    return s2.Compare(s1) < 0;
}
bool operator<=(const SAString &s1, const SAString &s2)
{
    return s1.Compare(s2) <= 0;
}
bool operator<=(const SAString &s1, const SAChar *s2)
{
    return s1.Compare(s2) <= 0;
}
bool operator<=(const SAChar *s1, const SAString &s2)
{
    return s2.Compare(s1) >= 0;
}
bool operator>=(const SAString &s1, const SAString &s2)
{
    return s1.Compare(s2) >= 0;
}
bool operator>=(const SAString &s1, const SAChar *s2)
{
    return s1.Compare(s2) >= 0;
}
bool operator>=(const SAChar *s1, const SAString &s2)
{
    return s2.Compare(s1) <= 0;
}

//////////////////////////////////////////////////////////////////////////////
// Finding

size_t SAString::Find(SAChar ch) const
{
    return Find(ch, 0);
}

size_t SAString::Find(SAChar ch, size_t nStart) const
{
    size_t nLength = GetData()->nDataLength;
    if (nStart >= nLength)
        return SIZE_MAX;

    // find first single character
    const SAChar *lpsz = sa_strchr(m_pchData + nStart, ch);

    // return -1 if not found and index otherwise
    return (lpsz == NULL) ? SIZE_MAX : (lpsz - m_pchData);
}

size_t SAString::FindOneOf(const SAChar *lpszCharSet) const
{
    assert(lpszCharSet);
    const SAChar *lpsz = sa_strpbrk(m_pchData, lpszCharSet);
    return (lpsz == NULL) ? SIZE_MAX : (lpsz - m_pchData);
}

size_t SAString::ReverseFind(SAChar ch) const
{
    // find last single character
    const SAChar *lpsz = sa_strrchr(m_pchData, ch);

    // return -1 if not found, distance from beginning otherwise
    return (lpsz == NULL) ? SIZE_MAX : (lpsz - m_pchData);
}

// find a sub-string (like strstr)
size_t SAString::Find(const SAChar *lpszSub) const
{
    return Find(lpszSub, 0);
}

size_t SAString::Find(const SAChar *lpszSub, size_t nStart) const
{
    assert(lpszSub);

    size_t nLength = GetData()->nDataLength;
    if (nStart > nLength)
        return SIZE_MAX;

    // find first matching substring
    const SAChar *lpsz = sa_strstr(m_pchData + nStart, lpszSub);

    // return -1 for not found, distance from beginning otherwise
    return (lpsz == NULL) ? SIZE_MAX : (lpsz - m_pchData);
}

// Special constructor and buffer access routines to manipulate binary data

// constructor for binary data (no converion to SAChar)
SAString::SAString(const void *pBuffer, size_t nLengthInBytes)
{
#ifdef SA_UNICODE

    Init();
    if (nLengthInBytes != 0)
    {
        assert(pBuffer);
        size_t nBinaryDataLengthDiff = nLengthInBytes % sizeof(SAChar);
        // allocate at least so many characters that all binary data can be stored (round up if necessary)
        size_t nLengthInChars = nLengthInBytes / sizeof(SAChar) + (nBinaryDataLengthDiff ? 1 : 0);
        AllocBuffer(nLengthInChars);
        // if nLengthInBytes is not a multiply of sizeof(SAChar)
        // we have to save the difference
        GetData()->nBinaryDataLengthDiff = nBinaryDataLengthDiff;
        memcpy(m_pchData, pBuffer, nLengthInBytes);
    }

#else	// !SA_UNICODE

    // byte == character in non-Unicode build
    Init();
    if (nLengthInBytes != 0)
    {
        assert(pBuffer);
        AllocBuffer(nLengthInBytes);
        memcpy(m_pchData, pBuffer, nLengthInBytes);
    }

#endif	// !SA_UNICODE
}

// get binary data length (in bytes)
size_t SAString::GetBinaryLength() const
{
#ifdef SA_UNICODE
    size_t size = GetLength() * sizeof(SAChar);
    size_t diff = GetData()->nBinaryDataLengthDiff;
    if (0 != diff)
        size = size + diff - sizeof(SAChar);
    return size;

#else	// !SA_UNICODE

    // byte == character in non-Unicode build
    return GetLength();

#endif	// !SA_UNICODE
}

// return pointer to const binary data buffer
SAString::operator const void *() const
{
    return (const void *)m_pchData;
}

// get pointer to modifiable binary data buffer at least as long as nMinBufLengthInBytes
void *SAString::GetBinaryBuffer(size_t nMinBufLengthInBytes)
{
#ifdef SA_UNICODE

    // convert bytes count to characters count (round up if necessasry)
    size_t nMinBufLengthInChars = nMinBufLengthInBytes / sizeof(SAChar) + (nMinBufLengthInBytes % sizeof(SAChar) ? 1 : 0);
    // if current LengthInBytes is not a multiply of sizeof(SAChar)
    // we have to save the difference
    // because GetBuffer may discard it
    size_t nSave = GetData()->nBinaryDataLengthDiff;
    void *pRet = GetBuffer(nMinBufLengthInChars);
    // then restore the difference
    GetData()->nBinaryDataLengthDiff = nSave;
    return pRet;

#else	// !SA_UNICODE

    // byte == character in non-Unicode build
    return GetBuffer(nMinBufLengthInBytes);

#endif	// !SA_UNICODE
}

// release buffer, setting length to nNewLength (or to first nul if -1)
void SAString::ReleaseBinaryBuffer(size_t nNewLengthInBytes)
{
#ifdef SA_UNICODE

    size_t nBinaryDataLengthDiff = nNewLengthInBytes % sizeof(SAChar);
    // release at least so many characters that all binary data can be stored (round up if necessary)
    size_t nNewLengthInChars = nNewLengthInBytes / sizeof(SAChar) + (nBinaryDataLengthDiff ? 1 : 0);
    ReleaseBuffer(nNewLengthInChars);
    // if nNewLengthInBytes is not a multiply of sizeof(SAChar)
    // we have to save the difference
    GetData()->nBinaryDataLengthDiff = nBinaryDataLengthDiff;

#else	// !SA_UNICODE

    // byte == character in non-Unicode build
    ReleaseBuffer(nNewLengthInBytes);

#endif	// !SA_UNICODE
}

// Special conversion functions (multi byte <-> unicode)
#ifdef SA_UNICODE
static void ConvertToMultiByteChars(SAStringData *pData)
{
    if (pData->pConvertedData || pData == _saDataNil)	// already converted and cached or empty
        return;

    // calculate multi-byte length
    size_t mbLen = SAWideCharToMultiByte(
        NULL, pData->data(), pData->nDataLength);

    // alocate space for conversion
    pData->pConvertedData = (SAStringConvertedData *)
        new unsigned char[sizeof(SAStringConvertedData) + (mbLen + 1)*sizeof(char)];
    pData->pConvertedData->nDataLength = mbLen;
    pData->pConvertedData->data()[mbLen] = '\0';

    // now actually convert
    SAWideCharToMultiByte(pData->pConvertedData->data(), pData->data(), pData->nDataLength);
}

#ifdef SQLAPI_WINDOWS
static void ConvertToUTF8Chars(SAStringData *pData)
{
    if (pData->pUTF8Data || pData == _saDataNil)	// already converted and cached or empty
        return;

    // calculate multi-byte length, the source string should be small enough
    int mbLen = WideCharToMultiByte(CP_UTF8, 0, pData->data(),
        (int)pData->nDataLength, NULL, 0, NULL, NULL);

    // alocate space for conversion
    pData->pUTF8Data = (SAStringConvertedData *)
        new unsigned char[sizeof(SAStringConvertedData) + (mbLen + 1)*sizeof(char)];
    pData->pUTF8Data->nDataLength = mbLen;
    pData->pUTF8Data->data()[mbLen] = '\0';

    // now actually convert
    WideCharToMultiByte(CP_UTF8, 0, pData->data(), (int)pData->nDataLength,
        pData->pUTF8Data->data(), mbLen, NULL, NULL);
}
#else
static void ConvertToUTF8Chars(SAStringData *pData)
{
    if( pData->pUTF8Data || pData == _saDataNil )	// already converted and cached or empty
        return;

    // calculate multi-byte length, the source string should be small enough
    size_t mbLen = wchar_to_utf8(pData->data(), pData->nDataLength, NULL, 0, UTF8_IGNORE_ERROR);

    // alocate space for conversion
    pData->pUTF8Data = (SAStringConvertedData *)
        new unsigned char[sizeof(SAStringConvertedData) + (mbLen+1)*sizeof(char)];
    pData->pUTF8Data->nDataLength = mbLen;
    pData->pUTF8Data->data()[mbLen] = '\0';

    // now actually convert
    wchar_to_utf8(pData->data(), pData->nDataLength, pData->pUTF8Data->data(), mbLen, UTF8_IGNORE_ERROR);
}
#endif

const char *SAString::GetUTF8Chars() const
{
    if (IsEmpty())
        return "";

    ConvertToUTF8Chars(GetData());
    return GetData()->pUTF8Data->data();
}

size_t SAString::GetUTF8CharsLength() const
{
    if (IsEmpty())
        return 0;

    ConvertToUTF8Chars(GetData());
    return GetData()->pUTF8Data->nDataLength;
}

void SAString::SetUTF8Chars(const char* szSrc, size_t nSrcLen/* = SIZE_MAX*/)
{
    if (NULL == szSrc)
    {
        Empty();
        return;
    }

    if (SIZE_MAX == nSrcLen)
        nSrcLen = NULL == szSrc ? 0 : strlen(szSrc);

    if (0 == nSrcLen)
    {
        Empty();
        return;
    }

    // calculate wide-char length
#ifdef SQLAPI_WINDOWS
    int wcLen = MultiByteToWideChar(CP_UTF8, 0, szSrc, (int)nSrcLen, NULL, 0);
#else
    size_t wcLen = utf8_to_wchar(szSrc, nSrcLen, NULL, 0, UTF8_IGNORE_ERROR);
#endif

    // allocate space for data
    AllocBeforeWrite(wcLen);
    // now actually convert
#ifdef SQLAPI_WINDOWS
    MultiByteToWideChar(CP_UTF8, 0, szSrc, (int)nSrcLen, m_pchData, wcLen);
#else
    utf8_to_wchar(szSrc, nSrcLen, m_pchData, wcLen, UTF8_IGNORE_ERROR);
#endif
    GetData()->nDataLength = wcLen;
    m_pchData[wcLen] = '\0';
    GetData()->nBinaryDataLengthDiff = 0;

#ifdef SA_STRING_EXT
    GetData()->pUTF8Data = (SAStringConvertedData *)
        new unsigned char[sizeof(SAStringConvertedData) + (nSrcLen+1)*sizeof(char)];
    memcpy(GetData()->pUTF8Data->data(), szSrc, nSrcLen);
    GetData()->pUTF8Data->nDataLength = nSrcLen;
    GetData()->pUTF8Data->data()[nSrcLen] = '\0';
#endif
}
#else	// !SA_UNICODE

static void ConvertToWideChars(SAStringData *pData)
{
    if (pData->pConvertedData || pData == _saDataNil)	// already converted and cached or empty
        return;

    // alocate space for conversion
    pData->pConvertedData = (SAStringConvertedData *)
        new unsigned char[sizeof(SAStringConvertedData) + (pData->nDataLength + 1)*sizeof(wchar_t)];

    size_t wLen = SAMultiByteToWideChar(pData->pConvertedData->data(), pData->data(), pData->nDataLength);
    pData->pConvertedData->nDataLength = wLen;
    pData->pConvertedData->data()[wLen] = '\0';
}
#endif	// !SA_UNICODE

const void *SAString::GetUTF16Chars() const
{
    if (IsEmpty())
        return "\0";

#ifdef SQLAPI_WINDOWS
    return GetWideChars();
#else
    SAStringData *pData = GetData();

    if( ! pData->pUTF16Data )	// already converted and cached
    {
        const wchar_t* srcData;
        size_t nSrcLen;
#ifdef SA_UNICODE
        srcData = pData->data();
        nSrcLen = pData->nDataLength;
#else
        srcData = GetWideChars();
        nSrcLen = GetWideCharsLength();
#endif
        // calculate length, the source string should be small enough
        const UTF32* src = (UTF32*)srcData;
        size_t newLen = utf32_to_utf16(&src, nSrcLen, NULL, 0, UTF16_IGNORE_ERROR);

        // allocate space for conversion
        pData->pUTF16Data = (SAStringUtf16Data *)
            new unsigned char[sizeof(SAStringUtf16Data) + (newLen+1)*sizeof(UTF16)];
        pData->pUTF16Data->nDataLength = newLen;
        pData->pUTF16Data->data()[newLen] = '\0';

        // now actually convert
        src = (UTF32*)srcData;
        utf32_to_utf16(&src, nSrcLen, pData->pUTF16Data->data(), newLen, UTF16_IGNORE_ERROR);
    }

    return pData->pUTF16Data->data();
#endif
}

size_t SAString::GetUTF16CharsLength() const
{
    if (IsEmpty())
        return 0;

#ifdef SQLAPI_WINDOWS
    return GetLength();
#else
    GetUTF16Chars();
    return GetData()->pUTF16Data->nDataLength;
#endif
}

void SAString::SetUTF16Chars(const void* szSrc, size_t nSrcLen/* = SIZE_MAX*/)
{
    if (NULL == szSrc)
    {
        Empty();
        return;
    }

#ifdef SQLAPI_WINDOWS
    if (SIZE_MAX == nSrcLen)
        nSrcLen = wcslen((const wchar_t*)szSrc);

    if (0 == nSrcLen)
    {
        Empty();
        return;
    }

    *this = SAString((const wchar_t*)szSrc, nSrcLen);
#else
    if( SIZE_MAX == nSrcLen )
        nSrcLen = NULL == szSrc ? 0:utf16_strlen((UTF16*)szSrc);

    if( 0 == nSrcLen )
    {
        Empty();
        return;
    }

    const UTF16* in = (UTF16*)szSrc;
    // calculate wide-char length
    size_t wcLen = utf16_to_utf32(&in, nSrcLen, NULL, 0, UTF16_IGNORE_ERROR);

    UTF32* szDst;
#ifdef SA_UNICODE
    // allocate space for data
    AllocBeforeWrite(wcLen);
    szDst = (UTF32*)m_pchData;
#else
    szDst = new UTF32[wcLen + 1];
#endif

    // now actually convert
    in = (UTF16*)szSrc;
    utf16_to_utf32(&in, nSrcLen, szDst, wcLen, UTF16_IGNORE_ERROR);

#ifdef SA_UNICODE
    GetData()->nDataLength = wcLen;
    m_pchData[wcLen] = '\0';
    GetData()->nBinaryDataLengthDiff = 0;
#else
    *this = SAString((const wchar_t*)szDst, wcLen);
    delete szDst;
#endif

#ifdef SA_STRING_EXT
    GetData()->pUTF16Data = (SAStringUtf16Data *)
        new unsigned char[sizeof(SAStringUtf16Data) + (nSrcLen+1)*sizeof(UTF16)];
    memcpy(GetData()->pUTF16Data->data(), szSrc, nSrcLen*sizeof(UTF16));
    GetData()->pUTF16Data->nDataLength = nSrcLen;
    GetData()->pUTF16Data->data()[nSrcLen] = '\0';
#endif
#endif
}

const char *SAString::GetMultiByteChars() const
{
#ifdef SA_UNICODE
    if (IsEmpty())
        return "";

    ConvertToMultiByteChars(GetData());
    return GetData()->pConvertedData->data();
#else
    return m_pchData;
#endif
}

size_t SAString::GetMultiByteCharsLength() const
{
#ifdef SA_UNICODE
    if (IsEmpty())
        return 0;

    ConvertToMultiByteChars(GetData());
    return GetData()->pConvertedData->nDataLength;
#else
    return GetData()->nDataLength;
#endif
}

const wchar_t *SAString::GetWideChars() const
{
#ifdef SA_UNICODE
    return m_pchData;
#else
    if (IsEmpty())
        return L"";

    ConvertToWideChars(GetData());
    return GetData()->pConvertedData->data();
#endif
}

size_t SAString::GetWideCharsLength() const
{
#ifdef SA_UNICODE
    return GetData()->nDataLength;
#else
    if (IsEmpty())
        return 0;

    ConvertToWideChars(GetData());
    return GetData()->pConvertedData->nDataLength;
#endif
}

#ifdef SA_USE_STL
SAString::SAString(const std::string &stringSrc)
{
    *this = stringSrc.c_str();
}

SAString::SAString(const std::wstring &stringSrc)
{
    *this = stringSrc.c_str();
}
#endif

//////////////////////////////////////////////////////////////////////
// SADateTime Class
//////////////////////////////////////////////////////////////////////

/*! \class SADateTime
*
* SADateTime is a subsidiary class. It is intended to replace and extend
* the functionality normally provided by the C run-time library date/time package.
* The SADateTime class supplies member functions and operators for simplified
* date/time handling. The class also provides constructors and operators for
* constructing, and assigning SADateTime objects and standard C++ date/time data types.
*
* To create date/time value use one of SADateTime constructors:
*    - SADateTime( ); default constructor, creates empty date/time value.
*    - SADateTime( const SADateTime & ); copy constructor, used to create
*		new date/time value from existing SADateTime object.
*    - SADateTime( const struct tm & ); creates date/time value from standard
*		C date/time structure.
*    - SADateTime(double); creates date/time from floating-point value
*		(DATE data type in Windows), measuring days from midnight, 30 December 1899.
*		So, midnight, 31 December 1899 is represented by 1.0. Similarly, 6 AM,
*		1 January 1900 is represented by 2.25, and midnight, 29 December 1899 is  1.0.
*		However, 6 AM, 29 December 1899 is  1.25.
*    - SADateTime( int nYear, int nMonth, int nDay, int nHour, int nMin, int nSec );
*		use this constructor to create new date/time value by specifying different
*		date/time components.
*
* \remarks
* SADateTime object encapsulates the struct tm data type used in ANSI C also adding
* fraction of a second functionality.
*
*/

/*static */
int SADateTime::m_saMonthDays[13] =
{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };

/*static */
bool SADateTime::DateFromTm(unsigned short wYear, unsigned short wMonth, unsigned short wDay,
    unsigned short wHour, unsigned short wMinute, unsigned short wSecond,
    unsigned int nNanoSecond,
    double &dtDest)
{
    wYear += (unsigned short)1900;
    // Validate year and month (ignore day of week and milliseconds)
    if (wYear > 9999 || wMonth < 1 || wMonth > 12)
        return false;

    //  Check for leap year and set the number of days in the month
    bool bLeapYear = ((wYear & 3) == 0) &&
        ((wYear % 100) != 0 || (wYear % 400) == 0);

    int nDaysInMonth =
        m_saMonthDays[wMonth] - m_saMonthDays[wMonth - 1] +
        ((bLeapYear && wDay == 29 && wMonth == 2) ? 1 : 0);

    // Finish validating the date
    if (wDay < 1 || wDay > nDaysInMonth ||
        wHour > 23 || wMinute > 59 ||
        wSecond > 59)
    {
        return false;
    }

    // Cache the date in days and time in fractional days
    long nDate;
    double dblTime;

    //It is a valid date; make Jan 1, 1AD be 1
    nDate = wYear * 365L + wYear / 4 - wYear / 100 + wYear / 400 +
        m_saMonthDays[wMonth - 1] + wDay;

    //  If leap year and it's before March, subtract 1:
    if (wMonth <= 2 && bLeapYear)
        --nDate;

    //  Offset so that 12/30/1899 is 0
    // TODO: yas must be checked
    nDate -= 693959L;

    dblTime = (((long)wHour * 3600L) +  // hrs in seconds
        ((long)wMinute * 60L) +  // mins in seconds
        ((long)wSecond)) / 86400.;
    // add nanoseconds
    double dNanoSec = ((double)(nNanoSecond)) / (24.0*60.0*60.0*1e9);
    dblTime += dNanoSec;

    dtDest = (double)nDate + ((nDate >= 0) ? dblTime : -dblTime);

    return true;
}

/*static */
bool SADateTime::TmFromDate(
    double dtSrc,
struct tm &tmDest, unsigned int &nNanoSecond)
{
    if (sa_isnan(dtSrc))
        return false;

    long MIN_DATE = (-657434L);  // about year 100
    long MAX_DATE = 2958465L;    // about year 9999

    // The legal range does not actually span year 0 to 9999.
    // about year 100 to about 9999
    if (dtSrc > MAX_DATE || dtSrc < MIN_DATE)
        return false;

    long nDaysAbsolute;     // Number of days since 1/1/0
    long nSecsInDay;        // Time in seconds since midnight
    long nMinutesInDay;     // Minutes in day

    long n400Years;         // Number of 400 year increments since 1/1/0
    long n400Century;       // Century within 400 year block (0,1,2 or 3)
    long n4Years;           // Number of 4 year increments since 1/1/0
    long n4Day;             // Day within 4 year block
    //  (0 is 1/1/yr1, 1460 is 12/31/yr4)
    long n4Yr;              // Year within 4 year block (0,1,2 or 3)
    bool bLeap4 = true;     // TRUE if 4 year block includes leap year

    double dblDate = dtSrc; // tempory serial date

    // Add days from 1/1/0 to 12/30/1899
    nDaysAbsolute = (long)dblDate + 693959L;

    dblDate = fabs(dblDate);
    double dblTime = dblDate - floor(dblDate);
    nSecsInDay = (long)(dblTime * 86400.0);

    double dNanoSecs = dblTime - double(nSecsInDay) / 86400.0;
    assert(dNanoSecs >= 0);
    dNanoSecs *= 86400.0 * 1e6;
    assert(dNanoSecs < 1e6);
    nNanoSecond = (unsigned int)dNanoSecs;
    if ((dNanoSecs - nNanoSecond) > 0.5)
    {
        ++nNanoSecond;
        if (nNanoSecond == 1000000)
        {
            nNanoSecond = 0;
            ++nSecsInDay;
            if (nSecsInDay == 86400)
            {
                nSecsInDay = 0;
                ++nDaysAbsolute;
            }
        }
    }
    nNanoSecond *= 1000;
    assert(nNanoSecond <= 999999999);

    // Calculate the day of week (sun=1, mon=2...)
    //   -1 because 1/1/0 is Sat.  +1 because we want 1-based
    tmDest.tm_wday = (int)((nDaysAbsolute - 1) % 7L) + 1;

    // Leap years every 4 yrs except centuries not multiples of 400.
    n400Years = (long)(nDaysAbsolute / 146097L);

    // Set nDaysAbsolute to day within 400-year block
    nDaysAbsolute %= 146097L;

    // -1 because first century has extra day
    n400Century = (long)((nDaysAbsolute - 1) / 36524L);

    // Non-leap century
    if (n400Century != 0)
    {
        // Set nDaysAbsolute to day within century
        nDaysAbsolute = (nDaysAbsolute - 1) % 36524L;

        // +1 because 1st 4 year increment has 1460 days
        n4Years = (long)((nDaysAbsolute + 1) / 1461L);

        if (n4Years != 0)
            n4Day = (long)((nDaysAbsolute + 1) % 1461L);
        else
        {
            bLeap4 = false;
            n4Day = (long)nDaysAbsolute;
        }
    }
    else
    {
        // Leap century - not special case!
        n4Years = (long)(nDaysAbsolute / 1461L);
        n4Day = (long)(nDaysAbsolute % 1461L);
    }

    if (bLeap4)
    {
        // -1 because first year has 366 days
        n4Yr = (n4Day - 1) / 365;

        if (n4Yr != 0)
            n4Day = (n4Day - 1) % 365;
    }
    else
    {
        n4Yr = n4Day / 365;
        n4Day %= 365;
    }

    // n4Day is now 0-based day of year.
    // Save 1-based day of year, year number
    tmDest.tm_yday = (int)n4Day + 1;
    tmDest.tm_year = (int)
        (n400Years * 400 + n400Century * 100 + n4Years * 4 + n4Yr);

    // Handle leap year: before, on, and after Feb. 29.
    if (n4Yr == 0 && bLeap4)
    {
        // Leap Year
        if (n4Day == 59)
        {
            /* Feb. 29 */
            tmDest.tm_mon = 2;
            tmDest.tm_mday = 29;

            goto DoTime;
        }

        // Pretend it's not a leap year for month/day comp.
        if (n4Day >= 60)
            --n4Day;
    }

    // Make n4DaY a 1-based day of non-leap year and compute
    // month/day for everything but Feb. 29.
    ++n4Day;

    // Month number always >= n/32, so save some loop time */
    for (tmDest.tm_mon = (int)(n4Day >> 5) + 1;
        n4Day > m_saMonthDays[tmDest.tm_mon]; tmDest.tm_mon++)
        // hide warning
        ;

    tmDest.tm_mday = (int)(n4Day - m_saMonthDays[tmDest.tm_mon - 1]);

DoTime:
    if (nSecsInDay == 0)
        tmDest.tm_hour = tmDest.tm_min = tmDest.tm_sec = 0;
    else
    {
        tmDest.tm_sec = (int)(nSecsInDay % 60L);
        nMinutesInDay = nSecsInDay / 60L;
        tmDest.tm_min = (int)nMinutesInDay % 60;
        tmDest.tm_hour = (int)nMinutesInDay / 60;
    }

    tmDest.tm_isdst = -1;
    tmDest.tm_year -= 1900;  // year is based on 1900
    tmDest.tm_mon -= 1;      // month of year is 0-based
    tmDest.tm_wday -= 1;     // day of week is 0-based
    tmDest.tm_yday -= 1;     // day of year is 0-based
    return true;
}

void SADateTime::Init_Tm()
{
    memset(&m_tm, 0, sizeof(m_tm));
    m_tm.tm_isdst = -1;
    m_nFraction = 0;
}

SADateTime::SADateTime()
{
    Init_Tm();

    /*
    2009-02-19: it's helpless but takes time
    if(!TmFromDate(0.0, m_tm, m_nFraction))
    {
    assert(false);
    Init_Tm();
    }
    */
}

SADateTime::SADateTime(const SADateTime &other)
{
    m_tm = other.m_tm;
    m_tm.tm_isdst = -1;
    m_nFraction = other.m_nFraction;
    m_timezone = other.m_timezone;
}

SADateTime::SADateTime(const struct tm &tmValue)
{
    m_tm = tmValue;
    m_tm.tm_isdst = -1;
    m_nFraction = 0;
}

#ifndef UNDER_CE
SADateTime::SADateTime(const struct timeb &tmbValue)
{
#if defined(_REENTRANT) && !defined(SQLAPI_WINDOWS)
    localtime_r(&tmbValue.time, &m_tm);	// Convert to local time
#else
    m_tm = *localtime((const time_t*)&tmbValue.time);	// Convert to local time
#endif

    m_tm.tm_isdst = -1;
    m_nFraction = tmbValue.millitm * 1000000;
}

SADateTime::SADateTime(const struct timeval &tmvValue)
{
#if defined(_REENTRANT) && !defined(SQLAPI_WINDOWS)
    localtime_r(&tmvValue.tv_sec, &m_tm);	// Convert to local time
#else
    time_t lt = tmvValue.tv_sec;
    m_tm = *localtime((const time_t*)&lt);	// Convert to local time
#endif

    m_tm.tm_isdst = -1;
    m_nFraction = (unsigned int)tmvValue.tv_usec * 1000;
}
#endif

#ifdef SQLAPI_WINDOWS
SADateTime::SADateTime(SYSTEMTIME &st)
{
    *this = SADateTime(st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    m_nFraction = st.wMilliseconds * 1000000;
}
#endif

SADateTime::operator struct tm &()
{
    return m_tm;
}

SADateTime::operator struct tm() const
{
    return m_tm;
}

SADateTime::SADateTime(double dt)
{
    if (!TmFromDate(dt, m_tm, m_nFraction))
#ifdef SQLAPI_DATETIME_WRONG_EXCEPTION
        throw SAException(SA_Library_Error, SA_Library_Error_WrongDatetime, -1, IDS_DATETIME_WRONG_ERROR);
#else
        Init_Tm();
#endif
}

SADateTime::operator double() const
{
    double dt;
    if (!DateFromTm(
        (unsigned short)m_tm.tm_year, (unsigned short)(m_tm.tm_mon + 1), (unsigned short)m_tm.tm_mday,
        (unsigned short)m_tm.tm_hour, (unsigned short)m_tm.tm_min, (unsigned short)m_tm.tm_sec,
        m_nFraction, dt))
#ifdef SQLAPI_DATETIME_WRONG_EXCEPTION
        throw SAException(SA_Library_Error, SA_Library_Error_WrongDatetime, -1, IDS_DATETIME_WRONG_ERROR);
#else
        dt = 0.0;
#endif

    return dt;
}

SADateTime::operator SAString() const
{
    SAString s;
    if (m_nFraction > 0)
    {
        s.Format(_TSA("%04u-%02u-%02uT%02u:%02u:%02u.%09u"),
            GetYear(),
            GetMonth(),
            GetDay(),
            GetHour(),
            GetMinute(),
            GetSecond(),
            m_nFraction);
        s.TrimRight(_TSA('0'));
    }
    else
        s.Format(_TSA("%04u-%02u-%02uT%02u:%02u:%02u"),
        GetYear(),
        GetMonth(),
        GetDay(),
        GetHour(),
        GetMinute(),
        GetSecond());
    if (!m_timezone.IsEmpty())
    {
        SAChar sign = m_timezone.GetAt(0);
        if (sign == _TSA('+') || sign == _TSA('-'))
            s += _TSA(' ');
        s += m_timezone;
    }
    return s;
}

void SADateTime::Init(int nYear, int nMonth, int nDay, int nHour, int nMin, int nSec, unsigned int nFraction, const SAChar* timezone)
{
    m_tm.tm_year = nYear - 1900;
    m_tm.tm_mon = nMonth - 1;
    m_tm.tm_mday = nDay;
    m_tm.tm_hour = nHour;
    m_tm.tm_min = nMin;
    m_tm.tm_sec = nSec;

    // complete structure
    m_tm.tm_isdst = -1;
    m_nFraction = nFraction;
    m_timezone = timezone;

    // Validate year and month
    if (nYear > 9999 || nMonth < 1 || nMonth > 12)
#ifdef SQLAPI_DATETIME_WRONG_EXCEPTION
        throw SAException(SA_Library_Error, SA_Library_Error_WrongDatetime, -1, IDS_DATETIME_WRONG_ERROR);
#else
        return;
#endif

    //  Check for leap year and set the number of days in the month
    bool bLeapYear = ((nYear & 3) == 0) &&
        ((nYear % 100) != 0 || (nYear % 400) == 0);

    int nDaysInMonth =
        m_saMonthDays[nMonth] - m_saMonthDays[nMonth - 1] +
        ((bLeapYear && nDay == 29 && nMonth == 2) ? 1 : 0);

    // Finish validating the date
    if (nDay < 1 || nDay > nDaysInMonth ||
        nHour > 23 || nMin > 59 ||
        nSec > 59)
#ifdef SQLAPI_DATETIME_WRONG_EXCEPTION
        throw SAException(SA_Library_Error, SA_Library_Error_WrongDatetime, -1, IDS_DATETIME_WRONG_ERROR);
#else
        return;
#endif

    // Cache the date in days
    long nDate;

    //It is a valid date; make Jan 1, 1AD be 1
    nDate = nYear * 365L + nYear / 4 - nYear / 100 + nYear / 400 +
        m_saMonthDays[nMonth - 1] + nDay;

    //  If leap year and it's before March, subtract 1:
    if (nMonth <= 2 && bLeapYear)
        --nDate;

    //   -1 because 1/1/0 is Sat.
    m_tm.tm_wday = (int)((nDate - 1) % 7L);
    // -1 because we want zero-based
    m_tm.tm_yday = (int)(nDate - ((nYear - 1) * 365L + (nYear - 1) / 4 - (nYear - 1) / 100 + (nYear - 1) / 400 +
        m_saMonthDays[12 - 1] + 31) - 1);
}

SADateTime::SADateTime(int nYear, int nMonth, int nDay)
{
    Init(nYear, nMonth, nDay, 0, 0, 0, 0, _TSA(""));
}

SADateTime::SADateTime(int nYear, int nMonth, int nDay, int nHour, int nMin, int nSec)
{
    Init(nYear, nMonth, nDay, nHour, nMin, nSec, 0, _TSA(""));
}

SADateTime::SADateTime(int nYear, int nMonth, int nDay, int nHour, int nMin, int nSec, unsigned int nFraction)
{
    Init(nYear, nMonth, nDay, nHour, nMin, nSec, nFraction, _TSA(""));
}

SADateTime::SADateTime(int nYear, int nMonth, int nDay, int nHour, int nMin, int nSec, const SAChar* timezone)
{
    Init(nYear, nMonth, nDay, nHour, nMin, nSec, 0, timezone);
}

SADateTime::SADateTime(int nYear, int nMonth, int nDay, int nHour, int nMin, int nSec, unsigned int nFraction, const SAChar* timezone)
{
    Init(nYear, nMonth, nDay, nHour, nMin, nSec, nFraction, timezone);
}

SADateTime::SADateTime(int nHour, int nMin, int nSec, unsigned int nFraction)
{
    Init(0, 0, 0, nHour, nMin, nSec, nFraction, _TSA(""));
}

unsigned int SADateTime::Fraction() const
{
    return m_nFraction;
}
unsigned int &SADateTime::Fraction()
{
    return m_nFraction;
}

const SAChar* SADateTime::Timezone() const
{
    return m_timezone;
}

SAString& SADateTime::Timezone()
{
    return m_timezone;
}

int SADateTime::GetYear() const
{
    return m_tm.tm_year + 1900;
}

int SADateTime::GetMonth() const
{
    return m_tm.tm_mon + 1;
}

int SADateTime::GetDay() const
{
    return m_tm.tm_mday;
}

int SADateTime::GetHour() const
{
    return m_tm.tm_hour;
}

int SADateTime::GetMinute() const
{
    return m_tm.tm_min;
}

int SADateTime::GetSecond() const
{
    return m_tm.tm_sec;
}

int SADateTime::GetDayOfWeek() const
{
    return m_tm.tm_wday + 1;
}

int SADateTime::GetDayOfYear() const
{
    return m_tm.tm_yday + 1;
}

#ifndef UNDER_CE
void SADateTime::GetTimeValue(struct timeb &tmv)
{
    ftime(&tmv); // setup tz fields
    tmv.time = mktime(&m_tm);
    tmv.millitm = (unsigned short)(m_nFraction != 0 ? (m_nFraction / 1000000) : 0);
}

void SADateTime::GetTimeValue(struct timeval &tmv)
{
    tmv.tv_sec = (long)mktime(&m_tm);
    tmv.tv_usec = (unsigned long)(m_nFraction != 0 ? (m_nFraction / 1000) : 0);
}
#endif

#ifdef SQLAPI_WINDOWS
void SADateTime::GetTimeValue(SYSTEMTIME &st)
{
    st.wYear = (WORD)(m_tm.tm_year + 1900);
    st.wMonth = (WORD)(m_tm.tm_mon + 1);
    st.wDay = (WORD)m_tm.tm_mday;
    st.wDayOfWeek = (WORD)m_tm.tm_wday;
    st.wHour = (WORD)m_tm.tm_hour;
    st.wMinute = (WORD)m_tm.tm_min;
    st.wSecond = (WORD)m_tm.tm_sec;
    st.wMilliseconds = (WORD)(m_nFraction != 0 ? (m_nFraction / 1000000) : 0);
}
#endif

/*static */
SADateTime SQLAPI_CALLBACK SADateTime::currentDateTime()
{
    SADateTime dt;
    time_t long_time;

    time(&long_time);				// Get time as long integer
#if defined(_REENTRANT) && !defined(SQLAPI_WINDOWS)
    localtime_r(&long_time, &(struct tm &)dt);	// Convert to local time
#else
    dt = *localtime(&long_time);	// Convert to local time
#endif

    dt.m_tm.tm_isdst = -1;
    return dt;
}

/*static */
SADateTime SQLAPI_CALLBACK SADateTime::currentDateTimeWithFraction()
{
#ifdef SQLAPI_WINDOWS
#ifdef UNDER_CE
    SYSTEMTIME tmv;
    ::GetLocalTime(&tmv);
#else
    struct timeb tmv;
    ftime(&tmv);
#endif
#else
    struct timeval tmv;
    gettimeofday(&tmv, NULL);
#endif

    SADateTime dt(tmv);
    return dt;
}

SADateTime SADateTime::operator+(SAInterval interval) const
{
    return(SADateTime((double)*this + (double)interval));
}

SADateTime SADateTime::operator-(SAInterval interval) const
{
    return(SADateTime((double)*this - (double)interval));
}

SADateTime& SADateTime::operator+=(SAInterval interval)
{
    if (!TmFromDate((double)*this + (double)interval, m_tm, m_nFraction))
#ifdef SQLAPI_DATETIME_WRONG_EXCEPTION
        throw SAException(SA_Library_Error, SA_Library_Error_WrongDatetime, -1, IDS_DATETIME_WRONG_ERROR);
#else
        Init_Tm();
#endif
    return(*this);
}

SADateTime& SADateTime::operator-=(SAInterval interval)
{
    if (!TmFromDate((double)*this - (double)interval, m_tm, m_nFraction))
#ifdef SQLAPI_DATETIME_WRONG_EXCEPTION
        throw SAException(SA_Library_Error, SA_Library_Error_WrongDatetime, -1, IDS_DATETIME_WRONG_ERROR);
#else
        Init_Tm();
#endif
    return(*this);
}

SAInterval SADateTime::operator-(const SADateTime& dt) const
{
    return(SAInterval((double)*this - (double)dt));
}

//////////////////////////////////////////////////////////////////////
// saOptions Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

saOptions::saOptions()
{
#ifdef SA_USE_STL
    m_pOptions = new std::hash_map<std::string, SAParam*>();
#else
    m_nOptionCount = 0;
    m_ppOptions = NULL;
#endif
}

/*virtual */
saOptions::~saOptions()
{
#ifdef SA_USE_STL
    for (std::hash_map<std::string, SAParam*>::iterator i = m_pOptions->begin(); i != m_pOptions->end(); ++i)
        if (NULL != i->second)
            delete i->second;
    delete m_pOptions;
#else
    while (m_nOptionCount)
        delete m_ppOptions[--m_nOptionCount];
    if (m_ppOptions)
    {
        ::free(m_ppOptions);
        m_ppOptions = NULL;
    }
#endif
}

SAString &saOptions::operator[](const SAString &sOptionName)
{
    SAParam *pRef = NULL;

#ifdef SA_USE_STL
    std::string key = sOptionName.GetMultiByteChars();
    std::transform(key.begin(), key.end(), key.begin(), ::toupper);
    std::hash_map<std::string, SAParam*>::iterator i = m_pOptions->find(key);
    if (i != m_pOptions->end())
        pRef = i->second;
#else
    // check if option already exists
    for (int i = 0; i < m_nOptionCount; i++)
        if (m_ppOptions[i]->Name().CompareNoCase(sOptionName) == 0)
        {
            pRef = m_ppOptions[i];
            break;
        }
#endif

    if (!pRef)	// create new option
    {
        pRef = new SAParam(NULL, sOptionName, SA_dtString, -1, 0, -1, -1, SA_ParamInputOutput);

#ifdef SA_USE_STL
        (*m_pOptions)[key] = pRef;
#else
        sa_realloc((void**)&m_ppOptions, (m_nOptionCount + 1)*sizeof(SAParam*));
        m_ppOptions[m_nOptionCount] = pRef;
        ++m_nOptionCount;
#endif
    }

    return pRef->setAsString();
}

SAString saOptions::operator[](const SAString &sOptionName) const
{
    SAString s;

#ifdef SA_USE_STL
    std::string key = sOptionName.GetMultiByteChars();
    std::transform(key.begin(), key.end(), key.begin(), ::toupper);
    std::hash_map<std::string, SAParam*>::iterator i = m_pOptions->find(key);
    if (i != m_pOptions->end())
        s = i->second->asString();
#else
    for (int i = 0; i < m_nOptionCount; i++)
        if (m_ppOptions[i]->Name().CompareNoCase(sOptionName) == 0)
        {
            s = m_ppOptions[i]->asString();
            break;
        }
#endif

    return s;
}

//////////////////////////////////////////////////////////////////////
// SAPos Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SAPos::SAPos(int nByID)
{
    m_sName.Format(_TSA("%d"), nByID);
}

SAPos::SAPos(const SAString& sByName) :
m_sName(sByName)
{
}

//////////////////////////////////////////////////////////////////////
// SAConnection Class
//////////////////////////////////////////////////////////////////////

/*! \class SAConnection
*
* With the methods of a SAConnection object you can do the following:
*   - Set DBMS client by using setClient method.
*   - Check whether the cursor is connected using isConnected method.
*   - Check whether the database is online using isAlive method.
*   - Connect to data source with the Connect method.
*   - Manage transactions on the open connection by Commit and Rollback methods.
*   - Break the physical connection to the data source with the  Disconnect method.
*
* \remarks
* To set DBMS client use setClient method or specify a client directly in Connect method.
* To get currently set client use Client method. To get information about Client and Server
* use ClientVersion, ServerVersion and ServerVersionString methods.
*
* There are two methods to managing transactions on the open connection. Commit method
* saves any changes and ends the current transaction. Rollback method cancels any changes
* made during the current transaction and ends the transaction.
*
* To disconnect cursor and break the physical connection to the data source use Disconnect method.
*
* \see
* SACommand
*/

SAConnection::SAConnection()
{
    m_eSAClient = SA_Client_NotSpecified;
    m_pISAConnection = NULL;
    m_pCommandsMutex = new SAMutex;
    m_pCommands = NULL;

    m_eIsolationLevel = SA_LevelUnknown;
    m_eAutoCommit = SA_AutoCommitUnknown;
}

SAConnection::~SAConnection()
{
    // we are in destructor, so no exception throwing
    try
    {
        // walk through registered commands and unregister them
        SACriticalSectionScope scope(m_pCommandsMutex);

        sa_Commands *pIter = m_pCommands;
        while (pIter)
        {
            SACommand *pCommand = pIter->pCommand;
            assert(pCommand);

            // we are in destructor, so no exception throwing
            try
            {
                pCommand->setConnection(NULL);
            }
            catch (SAException &)
            {
                pCommand->m_pConnection = NULL;
                m_pCommands = pIter->Next;
                delete pIter;
            }

            pIter = m_pCommands;
        }

        m_pCommands = NULL;

        setClient(SA_Client_NotSpecified);
    }
    catch (SAException &)
    {
        delete m_pISAConnection;
    }

    delete m_pCommandsMutex;
}

void SAConnection::EnumCursors(EnumCursors_t fn, void *pAddlData)
{
    SACriticalSectionScope scope(m_pCommandsMutex);
    sa_Commands *p = m_pCommands;
    while (p)
    {
        fn(p->pISACursor, pAddlData);
        p = p->Next;
    }
}

void SAConnection::RegisterCommand(SACommand *pCommand)
{
    SACriticalSectionScope scope(m_pCommandsMutex);
    sa_Commands **pp = &m_pCommands;
    while (*pp)
    {
        assert((*pp)->pCommand != pCommand);
        pp = &(*pp)->Next;
    }

    *pp = new sa_Commands;
    (*pp)->pCommand = pCommand;
    (*pp)->pISACursor = m_pISAConnection ? m_pISAConnection->NewCursor(pCommand) : NULL;
    (*pp)->Next = NULL;
}

void SAConnection::UnRegisterCommand(SACommand *pCommand)
{
    SACriticalSectionScope scope(m_pCommandsMutex);
    sa_Commands **pp = &m_pCommands;
    while (*pp)
    {
        if ((*pp)->pCommand == pCommand)
        {
            sa_Commands *pNext = (*pp)->Next;
            delete (*pp)->pISACursor;
            delete *pp;
            *pp = pNext;
            return;
        }
        pp = &(*pp)->Next;
    }

    assert(false);
}

ISACursor *SAConnection::GetISACursor(SACommand *pCommand)
{
    SACriticalSectionScope scope(m_pCommandsMutex);
    sa_Commands *p = m_pCommands;
    while (p)
    {
        if (p->pCommand == pCommand)
            return p->pISACursor;
        p = p->Next;
    }

    assert(false);
    return NULL;
}

void SAConnection::setClient(SAClient_t eSAClient) SQLAPI_THROW(SAException)
{
    // nothing to do
    if (eSAClient == m_eSAClient)
        return;

    // dispose previous  connection and release client interface
    if (m_eSAClient != SA_Client_NotSpecified)
    {
        if (isConnected())
            Disconnect();
        assert(NULL != m_pISAConnection);
        m_pISAConnection->UnInitializeClient();
        delete m_pISAConnection;
        m_pISAConnection = NULL;
        m_eSAClient = SA_Client_NotSpecified;
    }

    // set new client interface
    if (eSAClient != SA_Client_NotSpecified)
    {
        ISAClient *pISAClient = stat_SAClients[eSAClient];
        if (!pISAClient)
            throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSupported, -1, IDS_CLIENT_NOT_SUPPORTED);

        ISAConnection *pISAConnection = pISAClient->QueryConnectionInterface(this);
        if (!pISAConnection)
            throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSupported, -1, IDS_CLIENT_NOT_SUPPORTED);

        try
        {
            pISAConnection->InitializeClient();
        }
        catch (SAException&)	// clean up on error
        {
            delete pISAConnection;
            throw;
        }
        m_pISAConnection = pISAConnection;
        m_eSAClient = eSAClient;
    }

    // walk through registered commands and reregister them
    SACriticalSectionScope scope(m_pCommandsMutex);
    sa_Commands **pp = &m_pCommands;
    while (*pp)
    {
        SACommand *pCommand = (*pp)->pCommand;
        assert(pCommand);

        delete (*pp)->pISACursor;
        (*pp)->pISACursor = NULL;
        (*pp)->pISACursor = m_pISAConnection ? m_pISAConnection->NewCursor(pCommand) : NULL;
        pp = &(*pp)->Next;
    }
}

SAClient_t SAConnection::Client() const
{
    return m_eSAClient;
}

long SAConnection::ClientVersion() const SQLAPI_THROW(SAException)
{
    if (!m_pISAConnection)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    return m_pISAConnection->GetClientVersion();
}

long SAConnection::ServerVersion() const SQLAPI_THROW(SAException)
{
    if (!m_pISAConnection)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    return isConnected() ? m_pISAConnection->GetServerVersion() : 0l;
}

SAString SAConnection::ServerVersionString() const SQLAPI_THROW(SAException)
{
    if (!m_pISAConnection)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    return isConnected() ? m_pISAConnection->GetServerVersionString() : SAString();
}

bool SAConnection::isConnected() const
{
    if (!m_pISAConnection)
        return false;

    return m_pISAConnection->IsConnected();
}

bool SAConnection::isAlive() const {
    if (!isConnected())
        return false;

    return m_pISAConnection->IsAlive();
}

void SAConnection::Connect(
    const SAString &sDBString,
    const SAString &sUserID,
    const SAString &sPassword,
    SAClient_t eSAClient,
    saConnectionHandler_t fHandler) SQLAPI_THROW(SAException)
{
    if (eSAClient != SA_Client_NotSpecified)
        setClient(eSAClient);

    if (!m_pISAConnection)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    // now actually connect
    m_pISAConnection->Connect(sDBString, sUserID, sPassword, fHandler);

#if !defined(SA_NO_TRIAL)
    CheckTrial();
#endif	// !defined(SA_NO_TRIAL)
}

void SAConnection::Disconnect() SQLAPI_THROW(SAException)
{
    if (!m_pISAConnection)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    // walk through all commands and close them
    {
        SACriticalSectionScope scope(m_pCommandsMutex);
        sa_Commands *p = m_pCommands;
        while (p)
        {
            SACommand *pCommand = p->pCommand;
            assert(pCommand);
            pCommand->Close();
            p = p->Next;
        }
    }

    // now actually disconnect
    m_pISAConnection->Disconnect();
}

void SAConnection::Destroy()
{
    if (!m_pISAConnection || !isConnected())
        return;

    // walk through all commands and close them
    {
        SACriticalSectionScope scope(m_pCommandsMutex);
        sa_Commands *p = m_pCommands;
        while (p)
        {
            SACommand *pCommand = p->pCommand;
            assert(pCommand);

            if (pCommand->isOpened())
                pCommand->Destroy();

            p = p->Next;
        }
    }

    // now actually disconnect
    m_pISAConnection->Destroy();
}

void SAConnection::Reset()
{
    if (!m_pISAConnection || !isConnected())
        return;

    // walk through all commands and close them
    {
        SACriticalSectionScope scope(m_pCommandsMutex);
        sa_Commands *p = m_pCommands;
        while (p)
        {
            SACommand *pCommand = p->pCommand;
            assert(pCommand);
            pCommand->Reset();
            p = p->Next;
        }
    }

    // now actually disconnect
    m_pISAConnection->Reset();
}

void SAConnection::Commit() SQLAPI_THROW(SAException)
{
    if (!m_pISAConnection)
        return;	// client not set, ignore commit request

    if (m_pISAConnection->IsConnected())
        m_pISAConnection->Commit();
}

void SAConnection::Rollback() SQLAPI_THROW(SAException)
{
    if (!m_pISAConnection)
        return;	// client not set, ignore rollback request

    if (m_pISAConnection->IsConnected())
        m_pISAConnection->Rollback();
}

void SAConnection::setIsolationLevel(
    SAIsolationLevel_t eIsolationLevel) SQLAPI_THROW(SAException)
{
    if (!m_pISAConnection)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    if (eIsolationLevel == m_eIsolationLevel)
        return;
    if (eIsolationLevel == SA_LevelUnknown)
        return;

    m_pISAConnection->setIsolationLevel(eIsolationLevel);
    m_eIsolationLevel = eIsolationLevel;
}

SAIsolationLevel_t SAConnection::IsolationLevel() const
{
    return m_eIsolationLevel;
}

void SAConnection::setAutoCommit(SAAutoCommit_t eAutoCommit) SQLAPI_THROW(SAException)
{
    if (!m_pISAConnection)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    if (eAutoCommit == m_eAutoCommit)
        return;
    if (eAutoCommit == SA_AutoCommitUnknown)
        return;

    m_pISAConnection->setAutoCommit(eAutoCommit);
    m_eAutoCommit = eAutoCommit;

    // notify commands
    SACriticalSectionScope scope(m_pCommandsMutex);
    sa_Commands *p = m_pCommands;
    while (p)
    {
        p->pISACursor->OnConnectionAutoCommitChanged();
        p = p->Next;
    }
}

SAAutoCommit_t SAConnection::AutoCommit() const
{
    return m_eAutoCommit;
}

SAString &SAConnection::setOption(const SAString &sOptionName)
{
    SAString &s = m_Options[sOptionName];
    return s;
}

SAString SAConnection::Option(const SAString &sOptionName) const
{
    SAString s = m_Options[sOptionName];
    return s;
}

saAPI *SAConnection::NativeAPI() const SQLAPI_THROW(SAException)
{
    if (!m_pISAConnection)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    return m_pISAConnection->NativeAPI();
}

saConnectionHandles *SAConnection::NativeHandles() SQLAPI_THROW(SAException)
{
    if (!m_pISAConnection)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    return m_pISAConnection->NativeHandles();
}

//////////////////////////////////////////////////////////////////////
// SACommand Class
//////////////////////////////////////////////////////////////////////

/*! \class SACommand
*
* A SACommand object may be used to query a database and return records,
* insert or update data, to manipulate the structure of a database, or
* to run a stored procedure.
*
* With the methods of a SACommand object you can do the following:
*   - Associate an opened connection with a SACommand object by using its setConnection method.
*   - Define the executable text of the command (for example, a SELECT statement or
*		a stored procedure name) and the command type with the setCommandText method.
*   - Bind query input variables assigning SAParam object(s) returning by Param and
*		ParamByIndex methods, or bind input variables using stream operator <<.
*   - Execute a command using Execute method.
*   - Check whether a result set exists using isResultSet method, and fetch row
*		by row from it with the FetchNext method. Get SAField objects with Field method and
*		access their descriptions and values.
*   - Receive output variables and access their values using Param and ParamByIndex methods.
*
* \remarks
* You can construct command based on the given connection, or with no associated SAConnection
* object using appropriate SACommand  constructor. If you create new command with no connection,
* you need to use setConnection method before executing any other SACommand methods.
*
* To set command text use setCommandText method. If the command has parameters, a set of
* SAParam objects creates implicitly. You can get the number of parameters with ParamCount
* method and look them through (and assign) with Param and ParamByIndex methods. Values for
* the input parameters can also be supplied by operator <<.
*
* To prepare a command use Prepare method. If you will not prepare a command it will be
* prepared implicitly before execution. If needed a command will be implicitly opened.
* If, for some reason, you want to open a command explicitly use Open method. To execute
* a command use Execute method. A command will be implicitly closed in destructor. If, for some
* reason, you want to close a command explicitly use Close method. To test whether a command
* is opened use isOpened method.
*
* A command (an SQL statement or procedure) can have a result set after executing.
* To check whether a result set exists use isResultSet method. If result set exists, a set
* of SAField objects created implicitly. SAField object contains full information about
* a field (name, type, size, etc.) and a field's value after fetching a row (FetchNext method).
* You can get the number of fields with FieldCount method and look them through with Field
* method or operator [] .
*
* If command has output parameters, you can get them after a command execution using
* Param and ParamByIndex methods.
*
* \see
* SAConnection
*/

void SACommand::Init()
{
    m_pConnection = NULL;

    m_eCmdType = SA_CmdUnknown;
    m_bPrepared = false;
    m_bExecuted = false;
    m_bFieldsDescribed = false;
    m_bSelectBuffersSet = false;

    m_bParamsKnown = false;
    m_nPlaceHolderCount = 0;
    m_ppPlaceHolders = NULL;
    m_nParamCount = 0;
    m_ppParams = NULL;
    m_nMaxParamID = 0;
    m_ppParamsID = NULL;
    m_nCurParamID = 0;

    m_nFieldCount = 0;
    m_ppFields = NULL;
}

SAParam &SACommand::CreateParam(
    const SAString &sName,
    SADataType_t eParamType,
    SAParamDirType_t eDirType/* = SA_ParamInput*/)
{
    return CreateParam(sName, eParamType, -1, 0, -1, -1, eDirType);
}

SAParam &SACommand::CreateParam(
    const SAString &sName,
    SADataType_t eParamType,
    int nNativeType,
    size_t nParamSize,
    int	nParamPrecision,
    int	nParamScale,
    SAParamDirType_t eDirType)
{
    return CreateParam(sName, eParamType, nNativeType,
        nParamSize, nParamPrecision, nParamScale, eDirType, sName, SIZE_MAX, SIZE_MAX);
}

SAParam &SACommand::CreateParam(
    const SAString &sName,
    SADataType_t eParamType,
    int nNativeType,
    size_t nParamSize,
    int	nParamPrecision,
    int	nParamScale,
    SAParamDirType_t eDirType,
    const SAString &sFullName,
    size_t nStart,	// param position in SQL statement
    size_t nEnd)	// param end position in SQL statemen
{
    m_bParamsKnown = true;

    SAParam *pRef = NULL;
    // first see if param with given name already exist
    for (int i = 0; i < m_nParamCount; ++i)
    {
        if (CompareIdentifier(m_ppParams[i]->m_sName, sName) == 0)
        {
            pRef = m_ppParams[i];
            break;
        }
    }
    if (!pRef)	// create param
    {
        pRef = new SAParam(this, sName, eParamType, nNativeType, nParamSize, nParamPrecision, nParamScale, eDirType);

        sa_realloc((void**)&m_ppParams, (m_nParamCount + 1)*sizeof(SAParam*));
        m_ppParams[m_nParamCount] = pRef;
        ++m_nParamCount;

        if (sa_isdigit(((const SAChar*)sName)[0]))
        {
            int nID = (int)sa_strtol((const SAChar*)sName, NULL, 10);
            if (nID > m_nMaxParamID)
            {
                sa_realloc((void**)&m_ppParamsID, nID*sizeof(SAParam*));
                for (; nID > m_nMaxParamID; ++m_nMaxParamID)
                    m_ppParamsID[m_nMaxParamID] = NULL;
            }
            if (nID > 0 && nID <= m_nMaxParamID)
                m_ppParamsID[nID - 1] = pRef;
        }
    }

    // create place holder
    sa_realloc((void**)&m_ppPlaceHolders, (m_nPlaceHolderCount + 1)*sizeof(saPlaceHolder));
    m_ppPlaceHolders[m_nPlaceHolderCount] = new saPlaceHolder(sFullName, nStart, nEnd, pRef);
    ++m_nPlaceHolderCount;

    return *pRef;
}

void SACommand::CreateField(
    const SAString &sName,
    SADataType_t eFieldType,
    int nNativeType,
    size_t nFieldSize,
    int nFieldPrecision,
    int nFieldScale,
    bool bFieldRequired,
    int nTotalFieldCount)
{
    SAField *pField = new SAField(this, m_nFieldCount + 1, sName,
        eFieldType, nNativeType, nFieldSize, nFieldPrecision, nFieldScale, bFieldRequired);
    if (nTotalFieldCount < 0 || m_nFieldCount + 1 > nTotalFieldCount)
        sa_realloc((void**)&m_ppFields, (m_nFieldCount + 1) * sizeof(SAField*));
    else if (NULL == m_ppFields)
        sa_realloc((void**)&m_ppFields, nTotalFieldCount * sizeof(SAField*));
    m_ppFields[m_nFieldCount] = pField;
    ++m_nFieldCount;
}

void SACommand::UnDescribeParams()
{
    DestroyParams();
    m_bParamsKnown = false;
}

void SACommand::DestroyParams()
{
    while (m_nPlaceHolderCount)
        delete m_ppPlaceHolders[--m_nPlaceHolderCount];
    if (m_ppPlaceHolders)
    {
        ::free(m_ppPlaceHolders);
        m_ppPlaceHolders = NULL;
    }
    while (m_nParamCount)
        delete m_ppParams[--m_nParamCount];
    if (m_ppParams)
    {
        ::free(m_ppParams);
        m_ppParams = NULL;
    }
    if (m_ppParamsID)
    {
        ::free(m_ppParamsID);
        m_ppParamsID = NULL;
    }
    m_nMaxParamID = 0;
}

void SACommand::DestroyFields()
{
    while (m_nFieldCount)
        delete m_ppFields[--m_nFieldCount];
    if (m_ppFields)
    {
        ::free(m_ppFields);
        m_ppFields = NULL;
    }

    m_bFieldsDescribed = false;
    m_bSelectBuffersSet = false;
}

// construct command with no associated connection
SACommand::SACommand()
{
    Init();

    setConnection(NULL);
    setCommandText(_TSA(""));
}

// construct command based on the given connection
SACommand::SACommand(
    SAConnection *pConnection,
    const SAString &sSQL,
    SACommandType_t eCmdType)
{
    Init();

    setConnection(pConnection);
    setCommandText(sSQL, eCmdType);
}

/*virtual */
SACommand::~SACommand()
{
    // we are in destructor, so no exception throwing
    try
    {
        setConnection(NULL);
    }
    catch (SAException &)
    {
        if (m_pConnection)
            m_pConnection->UnRegisterCommand(this);
    }

    UnDescribeParams();
    DestroyFields();
}

SAConnection *SACommand::Connection() const
{
    return m_pConnection;
}

void SACommand::setConnection(SAConnection *pConnection)
{
    if (m_pConnection)
    {
        if (isOpened())
            Close();
        m_pConnection->UnRegisterCommand(this);
    }

    m_pConnection = pConnection;
    if (m_pConnection)
        m_pConnection->RegisterCommand(this);
}

void SACommand::Open() SQLAPI_THROW(SAException)
{
    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    if (m_pConnection && !m_pConnection->isConnected())
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    pISACursor->Open();
}

bool SACommand::isOpened()
{
    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor)
        return false;

    return pISACursor->IsOpened();
}

void SACommand::Close() SQLAPI_THROW(SAException)
{
    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    if (!pISACursor->IsOpened())
        return;

    UnPrepare();

    pISACursor->Close();
}

void SACommand::Destroy()
{
    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor || !pISACursor->IsOpened())
        return;

    try
    {
        UnPrepare();
    }
    catch (SAException&)
    {
    }

    pISACursor->Destroy();

    m_bPrepared = false;
    m_bExecuted = false;
}

void SACommand::Reset()
{
    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor)
        return;

    pISACursor->Reset();

    DestroyFields();
    UnDescribeParams();
}

void SACommand::ParseInputMarkers(
    SAString &sCmd,
    bool *pbSpacesInCmd)
{
    // remove all existing placeholders
    while (m_nPlaceHolderCount)
        delete m_ppPlaceHolders[--m_nPlaceHolderCount];
    if (m_ppPlaceHolders)
    {
        ::free(m_ppPlaceHolders);
        m_ppPlaceHolders = NULL;
    }

    SAString sName, sFullName;

    const SAChar Literals[] = { '\'', '"', '`', 0 };
    const SAChar NameDelimiters[] = { '=', ' ', ',', ';', ')', '(', '[', ']', '\r', '\n', '\t', '+', '-', '*', '/', '>', '<', 0 };

    const SAChar *Value, *CurPos, *StartPos;
    SAChar CurChar;
    SAChar Literal = 0;
    bool EmbeddedLiteral = false;

    Value = sCmd;
    CurPos = Value;

    // simple test for StoredProc - no spaces
    if (pbSpacesInCmd)
        *pbSpacesInCmd = false;

    do
    {
        CurChar = *CurPos;
        // check that this command can not be a stored procedure
        if (pbSpacesInCmd)
        {
            if (!*pbSpacesInCmd && sa_isspace(CurChar) && !Literal)
                *pbSpacesInCmd = true;
        }
        if ((CurChar == _TSA('[')) && !Literal)
        {
            do
            {
                ++CurPos;
                CurChar = *CurPos;
            } while ((CurChar != 0) && (CurChar != _TSA(']')));
        }
        if ((CurChar == _TSA('-')) && !Literal && (*(CurPos + 1) == _TSA('-'))) // -- comment till end of line
        {
            do
            {
                ++CurPos;
                CurChar = *CurPos;
            } while ((CurChar != 0) && (CurChar != _TSA('\n')));
        }
        if ((CurChar == _TSA('/')) && !Literal && (*(CurPos + 1) == _TSA('*'))) // /* comment, last till */ 
        {
            do
            {
                ++CurPos;
                CurChar = *CurPos;
            } while ((CurChar != 0) && (CurChar != _TSA('/')) && (*(CurPos - 1) != _TSA('*')));
        }
        if ((_TSA(':') == CurChar) && !Literal && (CurPos == Value || sa_strchr(NameDelimiters, *(CurPos - 1))) &&
            (sa_isalpha((*(CurPos + 1))) || sa_isdigit((*(CurPos + 1))) || _TSA('_') == (*(CurPos + 1))))
        {
            if (CurPos != Value && _TSA('\\') == (*(CurPos - 1))) // check for escaped ':'
            {
                int nPos = (int)(CurPos - Value - 1);
                sCmd.Delete(nPos, 1);
                Value = sCmd;
                CurPos = Value + nPos;
            }
            else
            {
                StartPos = CurPos;
                while ((CurChar != 0) && (Literal || !sa_strchr(NameDelimiters, CurChar)))
                {
                    ++CurPos;
                    CurChar = *CurPos;
                    if (CurChar && sa_strchr(Literals, CurChar))
                    {
                        Literal = !Literal;
                        if (CurPos == StartPos + 1)
                            EmbeddedLiteral = true;
                    }
                }
                if (EmbeddedLiteral)
                {
                    const SAChar *sTemp = StartPos + 1;
                    const SAChar *sTempLast = CurPos - 1;
                    if (*sTemp && sa_strchr(Literals, *sTemp))
                        ++sTemp;
                    if (*sTempLast && sa_strchr(Literals, *sTempLast))
                        --sTempLast;
                    sName = SAString(sTemp, (int)(sTempLast - sTemp + 1));

                    EmbeddedLiteral = false;
                }
                else
                    sName = SAString(StartPos + 1, (int)(CurPos - StartPos - 1));

                sFullName = SAString(StartPos + 1, (int)(CurPos - StartPos - 1));

                CreateParam(
                    sName,
                    SA_dtUnknown,
                    -1,	// native data type
                    0, -1, -1,	// size, precision, scale
                    SA_ParamInput,
                    sFullName, (int)(StartPos - Value), (int)(CurPos - 1 - Value));
            }
        }
        else if (CurChar && sa_strchr(Literals, CurChar))
        {
            if (!Literal)
                Literal = CurChar;
            else if (CurChar == Literal)
                Literal = 0;
        }
        ++CurPos;
    } while (CurChar != 0);
}

void SACommand::ParseCmd(
    const SAString &sCmd,
    SACommandType_t eCmdType)
{
    UnDescribeParams();
    m_sCmd = sCmd;
    m_eCmdType = eCmdType;
    m_nCurParamID = 1;

    if (eCmdType == SA_CmdStoredProc)
        return;	// no parsing required if this is stored proc name

    if (eCmdType == SA_CmdSQLStmtRaw)
    {
        m_bParamsKnown = true;
        return; // no parsing required if this is raw query
    }

    bool bSpacesInCmd;
    ParseInputMarkers(m_sCmd, &bSpacesInCmd);

    if (m_nParamCount > 0)
    {
        if (m_eCmdType == SA_CmdUnknown)
            m_eCmdType = SA_CmdSQLStmt;
    }

    // still unknown
    if (m_eCmdType == SA_CmdUnknown)
    {
        if (bSpacesInCmd || sCmd.IsEmpty())
            m_eCmdType = SA_CmdSQLStmt;
        else
            m_eCmdType = SA_CmdStoredProc;
    }

    if (m_eCmdType == SA_CmdSQLStmt && !m_bParamsKnown)
        m_bParamsKnown = true;

    assert(m_eCmdType != SA_CmdUnknown);
}

void SACommand::UnSetCommandText()
{
    UnPrepare();

    UnDescribeParams();
    m_eCmdType = SA_CmdUnknown;
    m_sCmd.Empty();
}

SAString SACommand::CommandText() const
{
    return m_sCmd;
}

/*! Gets the command type currently associated with the SACommand object.
*
* \returns
* One of the following values from SACommandType_t enum.
*
* The CommandType method returns the command type value that was specified
* in SACommand constructor or setCommandText method. If you declared the command
* type value as SA_CmdUnknown (the default value) then command type is detected
* by the Library and the CommandType method returns this detected value.
*
* \remarks
* The command type can be explicitly set in SACommand constructor and setCommandText
* method, but it's not necessary to do it.
*
* \see
* SACommandType_t | CommandText
*/
SACommandType_t SACommand::CommandType() const
{
    return m_eCmdType;
}

void SACommand::UnExecute()
{
    // notify DBMS client API
    // that it can release resources from previous execution
    if (m_bExecuted)
    {
        ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
        if (!pISACursor)
            throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

        pISACursor->UnExecute();
    }

    DestroyFields();
    m_nCurParamID = 1;

    m_bExecuted = false;
}

void SACommand::UnPrepare()
{
    UnExecute();

    m_bPrepared = false;
}

void SACommand::setCommandText(
    const SAString &sCmd,
    SACommandType_t eCmdType)
{
    UnSetCommandText();	// clean up
    ParseCmd(sCmd, eCmdType);
}

void SACommand::Prepare() SQLAPI_THROW(SAException)
{
    if (m_bPrepared)
        return;

    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    UnPrepare();

    if (!isOpened())
        Open();

    pISACursor->Prepare(
        m_sCmd,
        m_eCmdType,
        m_nPlaceHolderCount, m_ppPlaceHolders);
    m_bPrepared = true;
}

void SACommand::Execute() SQLAPI_THROW(SAException)
{
    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    UnExecute();

    // if not yet
    Prepare();

    // if not yet
    if (!m_bParamsKnown)
    {
        assert(m_eCmdType == SA_CmdStoredProc);
        GetParamsSP();
    }

    pISACursor->Execute(m_nPlaceHolderCount, m_ppPlaceHolders);
    m_bExecuted = true;
}

bool SACommand::isExecuted()
{
    return m_bExecuted;
}

void SACommand::Cancel() SQLAPI_THROW(SAException)
{
    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    pISACursor->Cancel();
}

bool SACommand::isResultSet() SQLAPI_THROW(SAException)
{
    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    if (!m_bExecuted)	// no need to disturb native API
        return false;

    return pISACursor->ResultSetExists();
}

bool SACommand::FetchNext() SQLAPI_THROW(SAException)
{
    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    if (!m_bFieldsDescribed)
        DescribeFields();

    if (!m_bSelectBuffersSet)
    {
        pISACursor->SetSelectBuffers();
        m_bSelectBuffersSet = true;
    }

    bool bFetch = pISACursor->FetchNext();
    if (!bFetch)
    {
        if (!pISACursor->isSetScrollable() && pISACursor->ResultSetExists())	// new result set available?
        {
            DestroyFields();
            DescribeFields();
        }
    }
    return bFetch;
}

bool SACommand::FetchPrior() SQLAPI_THROW(SAException)
{
    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    if (!m_bFieldsDescribed)
        DescribeFields();

    if (!m_bSelectBuffersSet)
    {
        pISACursor->SetSelectBuffers();
        m_bSelectBuffersSet = true;
    }

    return pISACursor->FetchPrior();
}

bool SACommand::FetchFirst() SQLAPI_THROW(SAException)
{
    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    if (!m_bFieldsDescribed)
        DescribeFields();

    if (!m_bSelectBuffersSet)
    {
        pISACursor->SetSelectBuffers();
        m_bSelectBuffersSet = true;
    }

    return pISACursor->FetchFirst();
}

bool SACommand::FetchLast() SQLAPI_THROW(SAException)
{
    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    if (!m_bFieldsDescribed)
        DescribeFields();

    if (!m_bSelectBuffersSet)
    {
        pISACursor->SetSelectBuffers();
        m_bSelectBuffersSet = true;
    }

    return pISACursor->FetchLast();
}

bool SACommand::FetchPos(int offset, bool Relative/* = false*/) SQLAPI_THROW(SAException)
{
    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    if (!m_bFieldsDescribed)
        DescribeFields();

    if (!m_bSelectBuffersSet)
    {
        pISACursor->SetSelectBuffers();
        m_bSelectBuffersSet = true;
    }

    return pISACursor->FetchPos(offset, Relative);
}

long SACommand::RowsAffected() SQLAPI_THROW(SAException)
{
    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    if (!m_bExecuted)	// no need to disturb native API
        return -1;

    return pISACursor->GetRowsAffected();
}

void SACommand::GetParamsSP()
{
    assert(m_bParamsKnown == false);
    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    if (!isOpened())
        Open();

    pISACursor->DescribeParamSP();
    m_bParamsKnown = true;
}

int SACommand::ParamCount()
{
    if (!m_bParamsKnown)
    {
        assert(m_eCmdType == SA_CmdStoredProc);
        GetParamsSP();
    }
    return m_nParamCount;
}

SAParam &SACommand::ParamByIndex(int i)	// zero based index of C array
{
    if (!m_bParamsKnown)
    {
        assert(m_eCmdType == SA_CmdStoredProc);
        GetParamsSP();
    }
    assert(i < m_nParamCount);
    return *m_ppParams[i];
}

SAParam &SACommand::Param(int nParamByID)	// id in SQL statement, not in C array
{
    SAString sParamByName; sParamByName.Format(_TSA("%d"), nParamByID);

    if ((nParamByID > m_nMaxParamID) || (nParamByID < 1) || NULL == m_ppParamsID[nParamByID - 1])
        throw SAException(SA_Library_Error, SA_Library_Error_BindVarNotFound, -1, IDS_BIND_VAR_NOT_FOUND, (const SAChar*)sParamByName);

    return *m_ppParamsID[nParamByID - 1];
}


SAParam &SACommand::Param(const SAString &sParamByName)
{
    int iReturn = -1;
    int i;
    for (i = 0; i < ParamCount(); ++i)
    {
        if (CompareIdentifier(ParamByIndex(i).Name(), sParamByName) == 0)
            return ParamByIndex(i);

        // save, may be useful for special test
        if (iReturn == -1 && ParamByIndex(i).ParamDirType() == SA_ParamReturn)
            iReturn = i;
    }

    // special test for return of a fuction
    // we can refer to this parameter as "RETURN_VALUE"
    // on all DMBSs regardless of how they describe it (to be portable)
    if (CompareIdentifier(sParamByName, _TSA("RETURN_VALUE")) == 0 && iReturn != -1)
        return ParamByIndex(iReturn);

    if (i >= ParamCount())
        throw SAException(SA_Library_Error, SA_Library_Error_BindVarNotFound, -1, IDS_BIND_VAR_NOT_FOUND, (const SAChar*)sParamByName);

    return *((SAParam *)0);
}

SACommand &SACommand::operator << (const SAPos& pos)
{
    m_sCurParamName = pos.m_sName;
    m_nCurParamID = sa_toi(m_sCurParamName);

    return *this;
}

SACommand &SACommand::operator << (const SANull&)
{
    SAParam &param =
        m_nCurParamID >= 1 ? Param(m_nCurParamID) : Param(m_sCurParamName);
    m_nCurParamID++;

    param.setAsNull();
    return *this;
}

SACommand &SACommand::operator << (bool Value)
{
    SAParam &param =
        m_nCurParamID >= 1 ? Param(m_nCurParamID) : Param(m_sCurParamName);
    m_nCurParamID++;

    param.setAsBool() = Value;
    return *this;
}

SACommand &SACommand::operator << (short Value)
{
    SAParam &param =
        m_nCurParamID >= 1 ? Param(m_nCurParamID) : Param(m_sCurParamName);
    m_nCurParamID++;

    param.setAsShort() = Value;
    return *this;
}

SACommand &SACommand::operator << (unsigned short Value)
{
    SAParam &param =
        m_nCurParamID >= 1 ? Param(m_nCurParamID) : Param(m_sCurParamName);
    m_nCurParamID++;

    param.setAsUShort() = Value;
    return *this;
}

SACommand &SACommand::operator << (long Value)
{
    SAParam &param =
        m_nCurParamID >= 1 ? Param(m_nCurParamID) : Param(m_sCurParamName);
    m_nCurParamID++;

    param.setAsLong() = Value;
    return *this;
}

SACommand &SACommand::operator << (unsigned long Value)
{
    SAParam &param =
        m_nCurParamID >= 1 ? Param(m_nCurParamID) : Param(m_sCurParamName);
    m_nCurParamID++;

    param.setAsULong() = Value;
    return *this;
}

SACommand &SACommand::operator << (double Value)
{
    SAParam &param =
        m_nCurParamID >= 1 ? Param(m_nCurParamID) : Param(m_sCurParamName);
    m_nCurParamID++;

    param.setAsDouble() = Value;
    return *this;
}

SACommand &SACommand::operator << (const SANumeric &Value)
{
    SAParam &param =
        m_nCurParamID >= 1 ? Param(m_nCurParamID) : Param(m_sCurParamName);
    m_nCurParamID++;

    param.setAsNumeric() = Value;
    return *this;
}

SACommand &SACommand::operator << (sa_int64_t Value)
{
    SAParam &param =
        m_nCurParamID >= 1 ? Param(m_nCurParamID) : Param(m_sCurParamName);
    m_nCurParamID++;

    param.setAsNumeric() = Value;
    return *this;
}

SACommand &SACommand::operator << (sa_uint64_t Value)
{
    SAParam &param =
        m_nCurParamID >= 1 ? Param(m_nCurParamID) : Param(m_sCurParamName);
    m_nCurParamID++;

    param.setAsNumeric() = Value;
    return *this;
}

SACommand &SACommand::operator << (const SADateTime &Value)
{
    SAParam &param =
        m_nCurParamID >= 1 ? Param(m_nCurParamID) : Param(m_sCurParamName);
    m_nCurParamID++;

    param.setAsDateTime() = Value;
    return *this;
}

// special overload for string constants
SACommand &SACommand::operator << (const SAChar *Value)
{
    return operator << (SAString(Value));
}

SACommand &SACommand::operator << (const SAString &Value)
{
    SAParam &param =
        m_nCurParamID >= 1 ? Param(m_nCurParamID) : Param(m_sCurParamName);
    m_nCurParamID++;

    param.setAsString() = Value;
    return *this;
}

SACommand &SACommand::operator << (const SABytes &Value)
{
    SAParam &param =
        m_nCurParamID >= 1 ? Param(m_nCurParamID) : Param(m_sCurParamName);
    m_nCurParamID++;

    param.setAsBytes() = Value;
    return *this;
}

SACommand &SACommand::operator << (const SALongBinary &Value)
{
    SAParam &param =
        m_nCurParamID >= 1 ? Param(m_nCurParamID) : Param(m_sCurParamName);
    m_nCurParamID++;

    param.setAsLongBinary(
        Value.m_fnWriter,
        Value.m_nWriterPieceSize,
        Value.m_pAddlData) = Value;
    return *this;
}

SACommand &SACommand::operator << (const SALongChar &Value)
{
    SAParam &param =
        m_nCurParamID >= 1 ? Param(m_nCurParamID) : Param(m_sCurParamName);
    m_nCurParamID++;

    param.setAsLongChar(
        Value.m_fnWriter,
        Value.m_nWriterPieceSize,
        Value.m_pAddlData) = Value;
    return *this;
}

SACommand &SACommand::operator << (const SABLob &Value)
{
    SAParam &param =
        m_nCurParamID >= 1 ? Param(m_nCurParamID) : Param(m_sCurParamName);
    m_nCurParamID++;


    param.setAsBLob(
        Value.m_fnWriter,
        Value.m_nWriterPieceSize,
        Value.m_pAddlData) = Value;
    return *this;
}

SACommand &SACommand::operator << (const SACLob &Value)
{
    SAParam &param =
        m_nCurParamID >= 1 ? Param(m_nCurParamID) : Param(m_sCurParamName);
    m_nCurParamID++;

    param.setAsCLob(
        Value.m_fnWriter,
        Value.m_nWriterPieceSize,
        Value.m_pAddlData) = Value;
    return *this;
}

SACommand &SACommand::operator << (const SAValueRead &Value)
{
    SAParam &param =
        m_nCurParamID >= 1 ? Param(m_nCurParamID) : Param(m_sCurParamName);
    m_nCurParamID++;

    param.setAsValueRead() = Value;
    return *this;
}

void SACommand::DescribeFields() SQLAPI_THROW(SAException)
{
    assert(m_bFieldsDescribed == false);

    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    DestroyFields();

    pISACursor->DescribeFields(&SACommand::CreateField);
    m_bFieldsDescribed = true;
}

int SACommand::FieldCount() SQLAPI_THROW(SAException)
{
    if (!m_bFieldsDescribed)
        DescribeFields();

    return m_nFieldCount;
}

SAField &SACommand::Field(int nField) SQLAPI_THROW(SAException) // 1-based field number in result set
{
    if (!m_bFieldsDescribed)
        DescribeFields();

    assert(nField > 0);
    assert(nField <= m_nFieldCount);
    return *m_ppFields[nField - 1];
}

SAField &SACommand::Field(const SAString &sField) SQLAPI_THROW(SAException)
{
    int i;
    for (i = 0; i < FieldCount(); i++)
    {
        if (CompareIdentifier(m_ppFields[i]->Name(), sField) == 0)
            return *m_ppFields[i];
    }

    for (i = 0; i < FieldCount(); i++)
    {
        size_t nPos = m_ppFields[i]->Name().Find(_TSA('.'));
        if (nPos != SIZE_MAX)
        {
            if (CompareIdentifier(m_ppFields[i]->Name().Mid(nPos + 1), sField) == 0)
                return *m_ppFields[i];
        }
    }

    if (i >= FieldCount())
        throw SAException(SA_Library_Error, SA_Library_Error_FieldNotFound, -1, IDS_FIELD_NOT_FOUND, (const SAChar*)sField);

    return *((SAField *)0);
}

SAField &SACommand::operator[](int nField) SQLAPI_THROW(SAException)	// 1-based field number in result set
{
    return Field(nField);
}

SAField &SACommand::operator[](const SAString& sField) SQLAPI_THROW(SAException)
{
    return Field(sField);
}

SAString &SACommand::setOption(const SAString &sOptionName)
{
    SAString &s = m_Options[sOptionName];
    return s;
}

SAString SACommand::Option(const SAString &sOptionName) const
{
    SAString s = m_Options[sOptionName];
    return s.IsEmpty() && m_pConnection ? m_pConnection->Option(sOptionName) : s;
}

SAString &SAParam::setOption(const SAString &sOptionName)
{
    SAString &s = m_Options[sOptionName];
    return s;
}

SAString SAParam::Option(const SAString &sOptionName) const
{
    SAString s = m_Options[sOptionName];
    return s.IsEmpty() && m_pCommand ? m_pCommand->Option(sOptionName) : s;
}

SAString &SAField::setOption(const SAString &sOptionName)
{
    SAString &s = m_Options[sOptionName];
    return s;
}

SAString SAField::Option(const SAString &sOptionName) const
{
    SAString s = m_Options[sOptionName];
    return s.IsEmpty() && m_pCommand ? m_pCommand->Option(sOptionName) : s;
}

saCommandHandles *SACommand::NativeHandles() SQLAPI_THROW(SAException)
{
    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    if (!isOpened())
        Open();

    return pISACursor->NativeHandles();
}

void SACommand::setBatchExceptionPreHandler(PreHandleException_t fnHandler, void * pAddlData)
{
    ISACursor *pISACursor = m_pConnection ? m_pConnection->GetISACursor(this) : NULL;
    if (!pISACursor)
        throw SAException(SA_Library_Error, SA_Library_Error_ClientNotSet, -1, IDS_CLIENT_NOT_SET);

    pISACursor->setBatchExceptionPreHandler(fnHandler, pAddlData);
}

//////////////////////////////////////////////////////////////////////
// SAException Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SAException::SAException(
    SAException* pNested,
    SAErrorClass_t eError,
    int nNativeError,
    int nErrPos,
    const SAString &sMsg) :
    m_eErrorClass(eError),
    m_nNativeError(nNativeError),
    m_nErrPos(nErrPos),
    m_sMsg(sMsg),
    m_pNested(pNested)
{
}

SAException::SAException(
    SAException* pNested,
    SAErrorClass_t eError,
    int nNativeError,
    int nErrPos,
    const SAChar *lpszFormat, ...) :
    m_eErrorClass(eError),
    m_nNativeError(nNativeError),
    m_nErrPos(nErrPos),
    m_pNested(pNested)
{
    va_list argList;
    va_start(argList, lpszFormat);
    m_sMsg.FormatV(lpszFormat, argList);
    va_end(argList);
}

SAException::SAException(
    SAErrorClass_t eError,
    int nNativeError,
    int nErrPos,
    const SAString &sMsg) :
    m_eErrorClass(eError),
    m_nNativeError(nNativeError),
    m_nErrPos(nErrPos),
    m_sMsg(sMsg),
    m_pNested(NULL)
{
}

SAException::SAException(
    SAErrorClass_t eError,
    int nNativeError,
    int nErrPos,
    const SAChar *lpszFormat, ...) :
    m_eErrorClass(eError),
    m_nNativeError(nNativeError),
    m_nErrPos(nErrPos),
    m_pNested(NULL)
{
    va_list argList;
    va_start(argList, lpszFormat);
    m_sMsg.FormatV(lpszFormat, argList);
    va_end(argList);
}

SAException::SAException(const SAException &other)
{
    m_eErrorClass = other.m_eErrorClass;
    m_nNativeError = other.m_nNativeError;
    m_nErrPos = other.m_nErrPos;
    m_sMsg = other.m_sMsg;
    m_pNested = NULL == other.m_pNested ? NULL : new SAException(*other.m_pNested);
}

SAException& SAException::operator=(const SAException &other)
{
    if (this != &other)
    {
        m_eErrorClass = other.m_eErrorClass;
        m_nNativeError = other.m_nNativeError;
        m_nErrPos = other.m_nErrPos;
        m_sMsg = other.m_sMsg;
        m_pNested = NULL == other.m_pNested ? NULL : new SAException(*other.m_pNested);
    }

    return *this;
}

/*virtual */
SAException::~SAException()
{
    if (NULL != m_pNested)
        delete m_pNested;
}

/*static */
void SQLAPI_CALLBACK SAException::throwUserException(
    int nUserCode,
    const SAChar *lpszFormat, ...)
{
    va_list argList;
    va_start(argList, lpszFormat);
    SAString sMsg;
    sMsg.FormatV(lpszFormat, argList);
    va_end(argList);

    throw SAUserException(nUserCode, sMsg);
}

SAErrorClass_t SAException::ErrClass() const
{
    return m_eErrorClass;
}

int SAException::ErrNativeCode() const
{
    return m_nNativeError;
}

int SAException::ErrPos() const
{
    return m_nErrPos;
}

SAString SAException::ErrMessage() const
{
    return m_sMsg;
}

SAString SAException::ErrText() const
{
    SAString sText(m_sMsg);
    const SAException* pNested = NestedException();

    while (NULL != pNested)
    {
        sText += _TSA("\n") + pNested->ErrMessage();
        pNested = pNested->NestedException();
    }

    return sText;
}

const SAException* SAException::NestedException() const
{
    return m_pNested;
}

SAUserException::SAUserException(int nUserCode, const SAString &sMsg) :
SAException(SA_UserGenerated_Error, nUserCode, -1, sMsg)
{
}

/*virtual */
SAUserException::~SAUserException()
{
}

//////////////////////////////////////////////////////////////////////
// saPlaceHolder Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

saPlaceHolder::saPlaceHolder(
    const SAString &sFullName,
    size_t nStart,
    size_t nEnd,
    SAParam *pParamRef) :
    m_sFullName(sFullName),
    m_nStart(nStart),
    m_nEnd(nEnd),
    m_pParamRef(pParamRef)
{
}

/*virtual */
saPlaceHolder::~saPlaceHolder()
{
}

const SAString &saPlaceHolder::getFullName() const
{
    return m_sFullName;
}

size_t& saPlaceHolder::getStart()
{
    return m_nStart;
}

size_t& saPlaceHolder::getEnd()
{
    return m_nEnd;
}

SAParam *saPlaceHolder::getParam() const
{
    return m_pParamRef;
}

//////////////////////////////////////////////////////////////////////

// SAValue Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SAValueRead::SAValueRead(SADataType_t eDataType) :
m_eReaderMode(SA_LongOrLobReaderDefault),	// into internal buffer
m_fnReader(NULL),
m_nReaderWantedPieceSize(0),
m_pReaderAddlData(NULL),
m_pReaderBuf(NULL),
m_nReaderAlloc(0),
m_pbNull(&m_bInternalNull),
m_pScalar(&m_InternalScalar),
m_pNumeric(&m_InternalNumeric),
m_pDateTime(&m_InternalDateTime),
m_pInterval(&m_InternalInterval),
m_pString(&m_InternalString),
m_pCursor(&m_InternalCursor)
{
    m_eDataType = eDataType;

    // null indicator
    m_bInternalNull = true;
    // scalar types
    memset(&m_InternalScalar, 0, sizeof(m_InternalScalar));
    // an exact numeric value with a fixed precision and scale
    // Date time
    // variable length types (string, bytes, Longs and Lobs)
    // Cursor
}

SAValueRead::SAValueRead(const SAValueRead &vr) :
m_eReaderMode(SA_LongOrLobReaderDefault),	// into internal buffer
m_fnReader(NULL),
m_nReaderWantedPieceSize(0),
m_pReaderAddlData(NULL),
m_pReaderBuf(NULL),
m_nReaderAlloc(0),
m_pbNull(&m_bInternalNull),
m_pScalar(&m_InternalScalar),
m_pNumeric(&m_InternalNumeric),
m_pDateTime(&m_InternalDateTime),
m_pInterval(&m_InternalInterval),
m_pString(&m_InternalString),
m_pCursor(&m_InternalCursor)
{
    *this = vr;
}

SAValueRead &SAValueRead::operator =(const SAValueRead &vr)
{
    m_eDataType = vr.m_eDataType;

    // null indicator
    *m_pbNull = *vr.m_pbNull;
    // scalar types
    memmove(m_pScalar, vr.m_pScalar, sizeof(m_InternalScalar));
    // an exact numeric value with a fixed precision and scale
    *m_pNumeric = *vr.m_pNumeric;
    // Date time
    *m_pDateTime = *vr.m_pDateTime;
    // Interval
    *m_pInterval = *vr.m_pInterval;
    // variable length types (string, bytes, Longs and Lobs)
    *m_pString = *vr.m_pString;
    // Cursor
    // we can't copy cursor if any

    return *this;
}

SAValueRead::~SAValueRead()
{
    if (m_pReaderBuf)
        ::free(m_pReaderBuf);
}

SAValue::SAValue(SADataType_t eDataType) :
SAValueRead(eDataType),
m_bInternalUseDefault(false),		// do not pass "default value" flag by default
m_pbUseDefault(&m_bInternalUseDefault),
m_fnWriter(NULL),			// from internal buffer
m_nWriterSize(0),			// by default blob will be bound by max avail Pieces
m_pWriterAddlData(NULL),	// no data for default writer
m_pWriterBuf(NULL),
m_nWriterAlloc(0)
{
}

SAValue::~SAValue()
{
    if (m_pWriterBuf)
        ::free(m_pWriterBuf);
}

SADataType_t SAValueRead::DataType() const
{
    return m_eDataType;
}

bool SAValueRead::isNull() const
{
    return *m_pbNull;
}

void SAValue::setAsNull()
{
    *m_pbUseDefault = false;
    *m_pbNull = true;
}

void SAValue::setAsDefault()
{
    *m_pbUseDefault = true;
}

bool SAValue::isDefault() const
{
    return *m_pbUseDefault;
}

void SAValue::setAsUnknown()
{
    m_eDataType = SA_dtUnknown;
}

#define SAValueConvertScalar(targetType) case SA_dtBool: return (targetType)*(bool*)m_pScalar; case SA_dtShort: return (targetType)*(short*)m_pScalar; case SA_dtUShort: return (targetType)*(unsigned short*)m_pScalar; case SA_dtLong: return (targetType)*(long*)m_pScalar; case SA_dtULong: return (targetType)*(unsigned long*)m_pScalar; case SA_dtDouble: return (targetType)*(double*)m_pScalar;

signed short SAValueRead::asShort() const
{
    if (isNull())
        return 0;

    switch (m_eDataType)
    {
        SAValueConvertScalar(short)

    case SA_dtNumeric:
        return (short)(double)(*m_pNumeric);
    case SA_dtDateTime:
        return (short)(double)(*m_pDateTime);
    case SA_dtInterval:
        return (short)(double)(*m_pInterval);
    case SA_dtString:
        // try to convert
    {
        SAChar *pEnd;
        double d = sa_strtod(*m_pString, &pEnd);
        if (*pEnd != 0)
            throw SAException(SA_Library_Error, SA_Library_Error_WrongConversion, -1, IDS_CONVERTION_FROM_STRING_TO_SHORT_ERROR, (const SAChar*)*m_pString);
        return (short)d;
    }
    case SA_dtBytes:
        assert(false);	// do not convert to short from bytes
        break;
    case SA_dtLongBinary:
    case SA_dtLongChar:
    case SA_dtBLob:
    case SA_dtCLob:
        assert(false);	// do not convert to short from Lob
        break;
    case SA_dtCursor:
        assert(false);
        break;
    case SA_dtUnknown:
        assert(false);
        break;
    default:
        assert(false);	// unknown type
    }

    return 0;
}

unsigned short SAValueRead::asUShort() const
{
    if (isNull())
        return 0;

    switch (m_eDataType)
    {
        SAValueConvertScalar(unsigned short)

    case SA_dtNumeric:
        return (unsigned short)(double)(*m_pNumeric);
    case SA_dtDateTime:
        return (unsigned short)(double)(*m_pDateTime);
    case SA_dtInterval:
        return (unsigned short)(double)(*m_pInterval);
    case SA_dtString:
        // try to convert
    {
        SAChar *pEnd;
        double d = sa_strtod(*m_pString, &pEnd);
        if (*pEnd != 0)
            throw SAException(SA_Library_Error, SA_Library_Error_WrongConversion, -1, IDS_CONVERTION_FROM_STRING_TO_SHORT_ERROR, (const SAChar*)*m_pString);
        return (unsigned short)d;
    }
    case SA_dtBytes:
        assert(false);	// do not convert to short from bytes
        break;
    case SA_dtLongBinary:
    case SA_dtLongChar:
    case SA_dtBLob:
    case SA_dtCLob:
        assert(false);	// do not convert to short from Lob
        break;
    case SA_dtCursor:
        assert(false);
        break;
    case SA_dtUnknown:
        assert(false);
        break;
    default:
        assert(false);	// unknown type
    }

    return 0;
}

short &SAValue::setAsShort()
{
    *m_pbUseDefault = false;
    *m_pbNull = false;
    m_eDataType = SA_dtShort;
    return *(short*)m_pScalar;
}

unsigned short &SAValue::setAsUShort()
{
    *m_pbUseDefault = false;
    *m_pbNull = false;
    m_eDataType = SA_dtUShort;
    return *(unsigned short*)m_pScalar;
}

signed long SAValueRead::asLong() const
{
    if (isNull())
        return 0;

    switch (m_eDataType)
    {
        SAValueConvertScalar(long)

    case SA_dtUnknown:
        assert(false);
        break;
    case SA_dtNumeric:
        return (long)(double)(*m_pNumeric);
    case SA_dtDateTime:
        return (long)(double)(*m_pDateTime);
    case SA_dtInterval:
        return (long)(double)(*m_pInterval);
    case SA_dtString:
        // try to convert
    {
        SAChar *pEnd;
        double d = sa_strtod(*m_pString, &pEnd);
        if (*pEnd != 0)
            throw SAException(SA_Library_Error, SA_Library_Error_WrongConversion, -1, IDS_CONVERTION_FROM_STRING_TO_LONG_ERROR, (const SAChar*)*m_pString);
        return (long)d;
    }
    case SA_dtBytes:
        assert(false);	// do not convert to long from bytes
        break;
    case SA_dtLongBinary:
    case SA_dtLongChar:
    case SA_dtBLob:
    case SA_dtCLob:
        assert(false);	// do not convert to long from Lob
        break;
    case SA_dtCursor:
        assert(false);
        break;
    default:
        assert(false);	// unknown type
    }

    return 0;
}

unsigned long SAValueRead::asULong() const
{
    if (isNull())
        return 0;

    switch (m_eDataType)
    {
        SAValueConvertScalar(unsigned long)

    case SA_dtNumeric:
        return (unsigned long)(double)(*m_pNumeric);
    case SA_dtDateTime:
        return (unsigned long)(double)(*m_pDateTime);
    case SA_dtInterval:
        return (unsigned long)(double)(*m_pInterval);
    case SA_dtString:
        // try to convert
    {
        SAChar *pEnd;
        double d = sa_strtod(*m_pString, &pEnd);
        if (*pEnd != 0)
            throw SAException(SA_Library_Error, SA_Library_Error_WrongConversion, -1, IDS_CONVERTION_FROM_STRING_TO_LONG_ERROR, (const SAChar*)*m_pString);
        return (unsigned long)d;
    }
    case SA_dtBytes:
        assert(false);	// do not convert to long from bytes
        break;
    case SA_dtLongBinary:
    case SA_dtLongChar:
    case SA_dtBLob:
    case SA_dtCLob:
        assert(false);	// do not convert to long from Lob
        break;
    case SA_dtCursor:
        assert(false);
        break;
    case SA_dtUnknown:
        assert(false);
        break;
    default:
        assert(false);	// unknown type
    }

    return 0;
}

long &SAValue::setAsLong()
{
    *m_pbUseDefault = false;
    *m_pbNull = false;
    m_eDataType = SA_dtLong;
    return *(long*)m_pScalar;
}

unsigned long &SAValue::setAsULong()
{
    *m_pbUseDefault = false;
    *m_pbNull = false;
    m_eDataType = SA_dtULong;
    return *(unsigned long*)m_pScalar;
}

double SAValueRead::asDouble() const
{
    if (isNull())
        return 0;

    switch (m_eDataType)
    {
        SAValueConvertScalar(double)

    case SA_dtNumeric:
        return *m_pNumeric;
    case SA_dtDateTime:
        return *m_pDateTime;
    case SA_dtInterval:
        return *m_pInterval;
    case SA_dtString:
        // try to convert
    {
        SAChar *pEnd;
        double d = sa_strtod(*m_pString, &pEnd);
        if (*pEnd != 0)
            throw SAException(SA_Library_Error, SA_Library_Error_WrongConversion, -1, IDS_CONVERTION_FROM_STRING_TO_DOUBLE_ERROR, (const SAChar*)*m_pString);
        return d;
    }
    case SA_dtBytes:
        assert(false);	// do not convert to double from bytes
        break;
    case SA_dtLongBinary:
    case SA_dtLongChar:
    case SA_dtBLob:
    case SA_dtCLob:
        assert(false);	// do not convert to double from Lob
        break;
    case SA_dtCursor:
        assert(false);
        break;
    case SA_dtUnknown:
        assert(false);
        break;
    default:
        assert(false);	// unknown type
    }

    return 0;
}

double &SAValue::setAsDouble()
{
    *m_pbUseDefault = false;
    *m_pbNull = false;
    m_eDataType = SA_dtDouble;
    return *(double*)m_pScalar;
}

SANumeric SAValueRead::asNumeric() const
{
    if (isNull())
        return (double)0;

    switch (m_eDataType)
    {
        // all scalar types are converted to SANumeric through double
        SAValueConvertScalar(double)

    case SA_dtNumeric:
        return *m_pNumeric;
    case SA_dtDateTime:
        return (double)*m_pDateTime;
    case SA_dtInterval:
        return (double)*m_pInterval;
    case SA_dtString:
        // try to convert from const SAChar*
    {
        SANumeric num;
        num = *m_pString;
        return num;
    }
    case SA_dtBytes:
        assert(false);	// do not convert to SANumeric from bytes
        break;
    case SA_dtLongBinary:
    case SA_dtLongChar:
    case SA_dtBLob:
    case SA_dtCLob:
        assert(false);	// do not convert to SANumeric from Lob
        break;
    case SA_dtCursor:
        assert(false);
        break;
    case SA_dtUnknown:
        assert(false);
        break;
    default:
        assert(false);	// unknown type
    }

    return (double)0;
}

SANumeric &SAValue::setAsNumeric()
{
    *m_pbUseDefault = false;
    *m_pbNull = false;
    m_eDataType = SA_dtNumeric;
    return *m_pNumeric;
}

bool SAValueRead::asBool() const
{
    if (isNull())
        return false;

    switch (m_eDataType)
    {
    case SA_dtBool:
        return *(bool*)m_pScalar;
    case SA_dtShort:
        return *(short*)m_pScalar != 0;
    case SA_dtUShort:
        return *(short*)m_pScalar != 0;
    case SA_dtLong:
        return *(long*)m_pScalar != 0;
    case SA_dtULong:
        return *(long*)m_pScalar != 0;
    case SA_dtDouble:
        return *(double*)m_pScalar != 0;

    case SA_dtNumeric:
        return ((double)(*m_pNumeric) != 0.0);

    case SA_dtDateTime:
        assert(false); // do not convert to bool from date/time
        break;
    case SA_dtInterval:
        assert(false); // do not convert to bool from date interval
        break;
    case SA_dtString:
        assert(false); // do not convert to bool from string
        break;
    case SA_dtBytes:
        assert(false); // do not convert to bool from bytes
        break;
    case SA_dtLongBinary:
    case SA_dtLongChar:
    case SA_dtBLob:
    case SA_dtCLob:
        assert(false);	// do not convert to bool from Lob
        break;
    case SA_dtCursor:
        assert(false);
        break;
    case SA_dtUnknown:
        assert(false);
        break;
    default:
        assert(false);	// unknown type
    }

    return false;
}

bool &SAValue::setAsBool()
{
    *m_pbUseDefault = false;
    *m_pbNull = false;
    m_eDataType = SA_dtBool;
    return *(bool*)m_pScalar;
}

SADateTime SAValueRead::asDateTime() const
{
    SADateTime dt;

    if (isNull())
        return dt;

    switch (m_eDataType)
    {
    case SA_dtBool:
    case SA_dtShort:
    case SA_dtUShort:
    case SA_dtLong:
    case SA_dtULong:
    case SA_dtDouble:
    case SA_dtNumeric:
        return SADateTime(asDouble()); // convert scalar type to double first -> date/time
    case SA_dtDateTime:
        return *m_pDateTime;
    case SA_dtInterval:
        return (SADateTime::currentDateTimeWithFraction() + *m_pInterval);
    case SA_dtString:
    case SA_dtBytes:
        // TODO: yas
        assert(false); // do not convert to date/time from string
        break;
    case SA_dtLongBinary:
    case SA_dtLongChar:
    case SA_dtBLob:
    case SA_dtCLob:
        assert(false); // do not convert to date/time from Blob
        break;
    case SA_dtCursor:
        assert(false);
        break;
    case SA_dtUnknown:
        assert(false); // do not convert to date/time from unknow type
        break;
    default:
        assert(false); // unknown type
    }

    return dt;
}

SADateTime &SAValue::setAsDateTime()
{
    *m_pbUseDefault = false;
    *m_pbNull = false;
    m_eDataType = SA_dtDateTime;
    return *m_pDateTime;
}

SAInterval SAValueRead::asInterval() const
{
    SAInterval interval;

    if (isNull())
        return interval;

    switch (m_eDataType)
    {
    case SA_dtBool:
    case SA_dtShort:
    case SA_dtUShort:
    case SA_dtLong:
    case SA_dtULong:
    case SA_dtDouble:
    case SA_dtNumeric:
        return SAInterval(asDouble());
    case SA_dtDateTime:
        return (*m_pDateTime - SADateTime(0.0));
    case SA_dtInterval:
        return *m_pInterval;
    case SA_dtString:
    case SA_dtBytes:
        assert(false);	// do not convert to date/time from string
    case SA_dtLongBinary:
    case SA_dtLongChar:
    case SA_dtBLob:
    case SA_dtCLob:
        assert(false);	// do not convert to date/time from Blob
    case SA_dtCursor:
        assert(false);
        break;
    case SA_dtUnknown:
        assert(false);	// do not convert to date/time from NULL
        break;
    default:
        assert(false);	// unknown type
    }

    return interval;
}

SAInterval &SAValue::setAsInterval()
{
    *m_pbUseDefault = false;
    *m_pbNull = false;
    m_eDataType = SA_dtInterval;
    return *m_pInterval;
}

SAString SAValueRead::asString() const
{
    if (isNull())
        return SAString();

    size_t nLenInBytes;
    SAString s;

    switch (m_eDataType)
    {
    case SA_dtBool:
        if (*(bool *)m_pScalar)
            return _TSA("TRUE");
        return _TSA("FALSE");
    case SA_dtShort:
        s.Format(_TSA("%hd"), *(short*)m_pScalar);
        return s;
    case SA_dtUShort:
        s.Format(_TSA("%hu"), *(unsigned short*)m_pScalar);
        return s;
    case SA_dtLong:
        s.Format(_TSA("%ld"), *(long*)m_pScalar);
        return s;
    case SA_dtULong:
        s.Format(_TSA("%lu"), *(unsigned long*)m_pScalar);
        return s;
    case SA_dtDouble:	// 15 is the maximum precision of double
        s.Format(_TSA("%.*g"), 15, *(double*)m_pScalar);
        return s;
    case SA_dtNumeric:
        s = m_pNumeric->operator SAString();
        return s;
    case SA_dtDateTime:
        s = m_pDateTime->operator SAString();
        return s;
    case SA_dtInterval:
        s = m_pInterval->operator SAString();
        return s;
    case SA_dtString:
    case SA_dtLongChar:
    case SA_dtCLob:
        return *m_pString;
    case SA_dtBytes:
    case SA_dtLongBinary:
    case SA_dtBLob:
        // convert to hexadecimal string
        nLenInBytes = m_pString->GetBinaryLength();
        if (nLenInBytes)
        {
            SAChar *p2 = s.GetBuffer(nLenInBytes * 2);	// 2 characters for each byte
            const void *p = *m_pString;
            for (size_t i = 0; i < nLenInBytes; ++i)
            {
#if defined(__BORLANDC__) && (__BORLANDC__  <= 0x0520)
                sprintf(p2, _TSA("%02x"), *(const unsigned char*)p);
#else
                sa_snprintf(p2, 3, _TSA("%02x"), *(const unsigned char*)p);
#endif
                ++((const unsigned char*&)p);
                p2 += 2;
            }
            s.ReleaseBuffer(nLenInBytes * 2);
        }
        return s;
    case SA_dtCursor:
        assert(false);
        break;
    case SA_dtUnknown:
        assert(false);
        break;
    default:
        assert(false);	// unknown type
    }

    return SAString();
}

SAString &SAValue::setAsString()
{
    *m_pbUseDefault = false;
    *m_pbNull = false;
    m_eDataType = SA_dtString;
    return *m_pString;
}

SAString SAValueRead::asBytes() const
{
    if (isNull())
        return SAString(_TSA(""));

    switch (m_eDataType)
    {
    case SA_dtBool:
        return SAString((const void*)m_pScalar, (int)sizeof(bool));
    case SA_dtShort:
        return SAString((const void*)m_pScalar, (int)sizeof(short));
    case SA_dtUShort:
        return SAString((const void*)m_pScalar, (int)sizeof(unsigned short));
    case SA_dtLong:
        return SAString((const void*)m_pScalar, (int)sizeof(long));
    case SA_dtULong:
        return SAString((const void*)m_pScalar, (int)sizeof(unsigned long));
    case SA_dtDouble:
        return SAString((const void*)m_pScalar, (int)sizeof(double));
    case SA_dtNumeric:
        return SAString((const void*)m_pNumeric, (int)sizeof(SANumeric));
    case SA_dtDateTime:
        return SAString((const void*)m_pDateTime, (int)sizeof(SADateTime));
    case SA_dtInterval:
        return SAString((const void*)m_pInterval, (int)sizeof(SAInterval));
    case SA_dtString:
    case SA_dtBytes:
    case SA_dtLongBinary:
    case SA_dtLongChar:
    case SA_dtBLob:
    case SA_dtCLob:
        return *m_pString;
    case SA_dtCursor:
        assert(false);
        break;
    case SA_dtUnknown:
        assert(false);
        break;
    default:
        assert(false);	// unknown type
    }

    return SAString(_TSA(""));
}

SAString &SAValue::setAsBytes()
{
    *m_pbUseDefault = false;
    *m_pbNull = false;
    m_eDataType = SA_dtBytes;
    return *m_pString;
}

SAString SAValueRead::asLongOrLob() const
{
    if (isNull())
        return SAString(_TSA(""));

    SAString s;
    switch (m_eDataType)
    {
    case SA_dtBool:
    case SA_dtShort:
    case SA_dtUShort:
    case SA_dtLong:
    case SA_dtULong:
    case SA_dtDouble:
        assert(false);	// do not convert from scalar types
        break;
    case SA_dtNumeric:
        assert(false);	// do not convert from numeric
        break;
    case SA_dtDateTime:
        assert(false);	// do not convert from date/time
        break;
    case SA_dtInterval:
        assert(false);
        break;
    case SA_dtString:
    case SA_dtBytes:
    case SA_dtLongBinary:
    case SA_dtLongChar:
    case SA_dtBLob:
    case SA_dtCLob:
        return *m_pString;
    case SA_dtCursor:
        assert(false);	// do not convert from cursor
        break;
    case SA_dtUnknown:
        assert(false);
        break;
    default:
        assert(false);	// unknown type
    }

    return SAString(_TSA(""));
}

SAString SAValueRead::asLongBinary() const
{
    return asLongOrLob();
}

SAString SAValueRead::asLongChar() const
{
    return asLongOrLob();
}

SAString SAValueRead::asBLob() const
{
    return asLongOrLob();
}

SAString SAValueRead::asCLob() const
{
    return asLongOrLob();
}

SAValueRead::operator bool() const
{
    return asBool();
}

SAValueRead::operator short() const
{
    return asShort();
}

SAValueRead::operator unsigned short() const
{
    return asUShort();
}

SAValueRead::operator long() const
{
    return asLong();
}

SAValueRead::operator unsigned long() const
{
    return asULong();
}

SAValueRead::operator double() const
{
    return asDouble();
}

SAValueRead::operator SADateTime() const
{
    return asDateTime();
}

SAValueRead::operator SAInterval() const
{
    return asInterval();
}

SAValueRead::operator SAString() const
{
    return asString();
}

SAValueRead::operator SACommand *() const
{
    return asCursor();
}

SAString &SAValue::setAsLongBinary(
    saLongOrLobWriter_t fnWriter,
    size_t nWriterSize, void *pAddlData)
{
    *m_pbUseDefault = false;
    *m_pbNull = false;
    m_eDataType = SA_dtLongBinary;
    m_fnWriter = fnWriter;
    m_nWriterSize = nWriterSize;
    m_pWriterAddlData = pAddlData;
    return *m_pString;
}

SAString &SAValue::setAsLongChar(
    saLongOrLobWriter_t fnWriter,
    size_t nWriterSize, void *pAddlData)
{
    *m_pbUseDefault = false;
    *m_pbNull = false;
    m_eDataType = SA_dtLongChar;
    m_fnWriter = fnWriter;
    m_nWriterSize = nWriterSize;
    m_pWriterAddlData = pAddlData;
    return *m_pString;
}

SAString &SAValue::setAsBLob(
    saLongOrLobWriter_t fnWriter,
    size_t nWriterSize, void *pAddlData)
{
    *m_pbUseDefault = false;
    *m_pbNull = false;
    m_eDataType = SA_dtBLob;
    m_fnWriter = fnWriter;
    m_nWriterSize = nWriterSize;
    m_pWriterAddlData = pAddlData;
    return *m_pString;
}

SAString &SAValue::setAsCLob(
    saLongOrLobWriter_t fnWriter,
    size_t nWriterSize, void *pAddlData)
{
    *m_pbUseDefault = false;
    *m_pbNull = false;
    m_eDataType = SA_dtCLob;
    m_fnWriter = fnWriter;
    m_nWriterSize = nWriterSize;
    m_pWriterAddlData = pAddlData;
    return *m_pString;
}

SACommand *SAValueRead::asCursor() const
{
    if (isNull())
        return NULL;

    switch (m_eDataType)
    {
    case SA_dtCursor:
        return m_pCursor;
    case SA_dtUnknown:
        assert(false);
        break;
    default:
        assert(false);	// do not convert from any type other then cursor
    }

    return NULL;
}

SACommand *&SAValue::setAsCursor()
{
    *m_pbUseDefault = false;
    *m_pbNull = false;
    m_eDataType = SA_dtCursor;
    return m_pCursor;
}

SAValueRead &SAValue::setAsValueRead()
{
    *m_pbUseDefault = false;
    return *this;
}

void SAValueRead::setLongOrLobReaderMode(SALongOrLobReaderModes_t eMode)
{
    m_eReaderMode = eMode;
}

SALongOrLobReaderModes_t SAValueRead::LongOrLobReaderMode() const
{
    return m_eReaderMode;
}

size_t SAValueRead::PrepareReader(
    size_t nExpectedSizeMax,	// to optimaze buf allocation for internal buffer, 0 = unknown
    size_t nCallerMaxSize,	// max Piece that can be proceeced by API
    unsigned char *&pBuf,
    saLongOrLobReader_t fnReader,
    size_t nReaderWantedPieceSize,
    void *pReaderAddlData,
    bool bAddSpaceForNull)
{
    m_fnReader = fnReader;
    m_nReaderWantedPieceSize = nReaderWantedPieceSize;
    m_pReaderAddlData = pReaderAddlData;

    m_nExpectedSizeMax = nExpectedSizeMax;
    m_nReaderRead = 0l;
    m_nPieceSize = sa_min(m_nReaderWantedPieceSize ? m_nReaderWantedPieceSize : 0x10000, nCallerMaxSize);

    if (m_fnReader == NULL)	// prepare internal buffer
    {
        // if max size of data is known then alloc the whole buffer at once
        // otherwise allocate space for one piece of data
        size_t nPrepareSize = nExpectedSizeMax ? nExpectedSizeMax : m_nPieceSize;
        pBuf = (unsigned char*)m_pString->GetBinaryBuffer(nPrepareSize);

        return sa_min(m_nPieceSize, nPrepareSize);
    }

    // prepare buffer for user supplied handler
    if (m_nReaderAlloc < m_nPieceSize + (bAddSpaceForNull ? 1 : 0))
    {
        size_t n = m_nPieceSize + (bAddSpaceForNull ? 1 : 0);
        sa_realloc((void**)&m_pReaderBuf, n);
        m_nReaderAlloc = n;
    }
    pBuf = m_pReaderBuf;

    return m_nPieceSize;
}

void SAValueRead::InvokeReader(
    SAPieceType_t ePieceType,
    unsigned char *&pBuf,
    size_t nPieceLen)
{
    m_nReaderRead += nPieceLen;
    assert(m_nExpectedSizeMax == 0 || m_nReaderRead <= m_nExpectedSizeMax);

    if (m_fnReader == NULL)
    {
        // data was fetched into internal buffer, just prepare for next iteration
        m_pString->ReleaseBinaryBuffer(m_nReaderRead);

        if (ePieceType != SA_OnePiece && ePieceType != SA_LastPiece)
        {
            if (m_nExpectedSizeMax == 0)	// blob size was unknown
            {
                // expand buffer == prepare it for the next iteration
                // buffer can be relocated, we are ready
                pBuf = (unsigned char*)m_pString->GetBinaryBuffer(m_nReaderRead + m_nPieceSize);
                pBuf += m_nReaderRead;
            }
            else	// buffer is big enough, just increase the pointer == prepare it for the next iteration
                pBuf += nPieceLen;
        }
    }
    else
    {
        // call user supplied handler
        assert(nPieceLen <= m_nReaderAlloc);
        m_fnReader(ePieceType, pBuf, nPieceLen, m_nExpectedSizeMax, m_pReaderAddlData);
    }
}

size_t SAValue::InvokeWriter(
    SAPieceType_t &ePieceType,
    size_t nCallerMaxSize,
    void *&pBuf)
{
    assert(!isNull());

    size_t nSize =
        sa_min(m_nWriterSize ? m_nWriterSize : SA_DefaultMaxLong, nCallerMaxSize);
#ifdef SA_UNICODE
    nSize /= sizeof(SAChar);
    nSize *= sizeof(SAChar);
#endif

    if (ePieceType == SA_FirstPiece)
        m_nWriterWritten = 0l;

    if (m_fnWriter == NULL)	// use internal buffer
    {
        pBuf = (void*)(const void*)*m_pString;
        (const unsigned char *&)pBuf += m_nWriterWritten;
        size_t nWritten = m_pString->GetBinaryLength() > m_nWriterWritten ?
            sa_min(nSize, m_pString->GetBinaryLength() - m_nWriterWritten) : 0l;
        m_nWriterWritten += nWritten;

        if (m_nWriterWritten < m_pString->GetBinaryLength())
            ePieceType = SA_NextPiece;
        else
            ePieceType = SA_LastPiece;

        return nWritten;
    }

    if (m_nWriterAlloc < nSize)
    {
        sa_realloc((void**)&m_pWriterBuf, nSize);
        m_nWriterAlloc = nSize;
    }
    pBuf = m_pWriterBuf;

    size_t nWritten = m_fnWriter(ePieceType, m_pWriterBuf, nSize, m_pWriterAddlData);
    m_nWriterWritten += nWritten;

    if (ePieceType == SA_FirstPiece)
        ePieceType = SA_NextPiece;

    return nWritten;
}

void SAValueRead::setDataStorage(void *pStorage, SADataType_t eDataType)
{
    m_eDataType = eDataType;

    if (pStorage)
    {
        m_pScalar = pStorage;
        m_pNumeric = (SANumeric *)pStorage;
        m_pDateTime = (SADateTime *)pStorage;
        m_pInterval = (SAInterval *)pStorage;
        m_pString = (SAString *)pStorage;
        m_pCursor = (SACommand *)pStorage;
    }
    else
    {
        m_pScalar = &m_InternalScalar;
        m_pNumeric = &m_InternalNumeric;
        m_pDateTime = &m_InternalDateTime;
        m_pInterval = &m_InternalInterval;
        m_pString = &m_InternalString;
        m_pCursor = &m_InternalCursor;
    }
}

void SAValueRead::setIndicatorStorage(bool *pStorage)
{
    if (pStorage)
        m_pbNull = pStorage;
    else
        m_pbNull = &m_bInternalNull;
}

//////////////////////////////////////////////////////////////////////
// SAParam Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SAParam::SAParam(
    SACommand *pCommand,
    const SAString &sName,
    SADataType_t eParamType,
    int nNativeType,
    size_t nParamSize,
    int	nParamPrecision,
    int	nParamScale,
    SAParamDirType_t eDirType) :
    SAValue(eParamType),
    m_pCommand(pCommand),
    m_sName(sName),
    m_eParamType(eParamType),
    m_nNativeType(nNativeType),
    m_nParamSize(nParamSize),
    m_nParamPrecision(nParamPrecision),
    m_nParamScale(nParamScale),
    m_eDirType(eDirType)
{
}

/*virtual */
SAParam::~SAParam()
{
}

void SAParam::ReadLongOrLob(
    saLongOrLobReader_t fnReader,
    size_t nReaderWantedSize,
    void *pAddlData)
{
    assert(m_pCommand);
    ISACursor *pISACursor = m_pCommand->Connection()->GetISACursor(m_pCommand);
    assert(pISACursor);

    void *pValue;
    size_t nParamBufSize;
    pISACursor->GetParamBuffer(*this, pValue, nParamBufSize);
    pISACursor->ReadLongOrLOB(ISACursor::ISA_ParamValue, *this, pValue, nParamBufSize, fnReader, nReaderWantedSize, pAddlData);
}

const SAString &SAParam::Name() const
{
    return m_sName;
}
SADataType_t SAParam::ParamType() const
{
    return m_eParamType;
}
void SAParam::setParamType(SADataType_t eParamType)
{
    m_eParamType = eParamType;
}
int SAParam::ParamNativeType() const
{
    return m_nNativeType;
}
void SAParam::setParamNativeType(int nNativeType)
{
    m_nNativeType = nNativeType;
}
size_t SAParam::ParamSize() const
{
    return m_nParamSize;
}
void SAParam::setParamSize(size_t nParamSize)
{
    m_nParamSize = nParamSize;
}
SAParamDirType_t SAParam::ParamDirType() const
{
    return m_eDirType;
}
void SAParam::setParamDirType(SAParamDirType_t eParamDirType)
{
    m_eDirType = eParamDirType;
}
int SAParam::ParamPrecision() const
{
    return m_nParamPrecision;
}
void SAParam::setParamPrecision(int nParamPrecision)
{
    m_nParamPrecision = nParamPrecision;
}
int SAParam::ParamScale() const
{
    return m_nParamScale;
}
void SAParam::setParamScale(int nParamScale)
{
    m_nParamScale = nParamScale;
}

//////////////////////////////////////////////////////////////////////
// SAField Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SAField::SAField(
    SACommand *pCommand,
    int nPos,	// 1-based
    const SAString &sName,
    SADataType_t eFieldType,
    int nNativeType,
    size_t nFieldSize,
    int nFieldPrecision,
    int nFieldScale,
    bool bFieldRequired) :
    SAValueRead(eFieldType),
    m_pCommand(pCommand),
    m_nPos(nPos),
    m_sName(sName),
    m_eFieldType(eFieldType),
    m_nNativeType(nNativeType),
    m_nFieldSize(nFieldSize),
    m_nFieldPrecision(nFieldPrecision),
    m_nFieldScale(nFieldScale),
    m_bFieldRequired(bFieldRequired)
{
}

/*virtual */
SAField::~SAField()
{
}

void SAField::ReadLongOrLob(
    saLongOrLobReader_t fnReader,
    size_t nReaderWantedSize,
    void *pAddlData)
{
    ISACursor *pISACursor = m_pCommand->Connection()->GetISACursor(m_pCommand);
    assert(pISACursor);

    void *pValue;
    size_t nFieldBufSize;
    pISACursor->GetFieldBuffer(Pos(), pValue, nFieldBufSize);

    pISACursor->ReadLongOrLOB(ISACursor::ISA_FieldValue, *this, pValue, nFieldBufSize, fnReader, nReaderWantedSize, pAddlData);

    if (m_pReaderBuf)
    {
        ::free(m_pReaderBuf);
        m_pReaderBuf = NULL;
        m_nReaderAlloc = 0l;
    }
}

int SAField::Pos() const
{
    return m_nPos;
}
const SAString &SAField::Name() const
{
    return m_sName;
}
SADataType_t SAField::FieldType() const
{
    return m_eFieldType;
}
int SAField::FieldNativeType() const
{
    return m_nNativeType;
}
size_t SAField::FieldSize() const
{
    return m_nFieldSize;
}
int SAField::FieldPrecision() const
{
    return m_nFieldPrecision;
}
int SAField::FieldScale() const
{
    return m_nFieldScale;
}
bool SAField::isFieldRequired() const
{
    return m_bFieldRequired;
}

void SAField::setFieldSize(int nSize)
{
    m_nFieldSize = nSize;
}
void SAField::setFieldType(SADataType_t eType)
{
    m_eDataType = m_eFieldType = eType;
}

SABytes::SABytes(
    const SAString &sData) :
    SAString(sData)
{
}

SALongOrLob::SALongOrLob(
    const SAString &sData) :
    SAString(sData),
    m_fnWriter(NULL)
{
}

SALongOrLob::SALongOrLob(
    saLongOrLobWriter_t fnWriter,
    size_t nWriterPieceSize,
    void *pAddlData) :
    m_fnWriter(fnWriter),
    m_nWriterPieceSize(nWriterPieceSize),
    m_pAddlData(pAddlData)
{
}

SALongBinary::SALongBinary(
    const SAString &sData) :
    SALongOrLob(sData)
{
}

SALongBinary::SALongBinary(
    saLongOrLobWriter_t fnWriter,
    size_t nWriterPieceSize,
    void *pAddlData) :
    SALongOrLob(fnWriter, nWriterPieceSize, pAddlData)
{
}

SALongChar::SALongChar(
    const SAString &sData) :
    SALongOrLob(sData)
{
}

SALongChar::SALongChar(
    saLongOrLobWriter_t fnWriter,
    size_t nWriterPieceSize,
    void *pAddlData) :
    SALongOrLob(fnWriter, nWriterPieceSize, pAddlData)
{
}

SABLob::SABLob(
    const SAString &sData) :
    SALongOrLob(sData)
{
}

SABLob::SABLob(
    saLongOrLobWriter_t fnWriter,
    size_t nWriterPieceSize,
    void *pAddlData) :
    SALongOrLob(fnWriter, nWriterPieceSize, pAddlData)
{
}

SACLob::SACLob(
    const SAString &sData) :
    SALongOrLob(sData)
{
}

SACLob::SACLob(
    saLongOrLobWriter_t fnWriter,
    unsigned int nWriterPieceSize,
    void *pAddlData) :
    SALongOrLob(fnWriter, nWriterPieceSize, pAddlData)
{
}

//////////////////////////////////////////////////////////////////////
// SANumeric Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// SANumeric Helpers
//////////////////////////////////////////////////////////////////////

static void LittleEndianDivide(
    const unsigned char Devidend[],
    unsigned char Devisor,
    unsigned char Quotient[],
    unsigned char *pRemainder)
{
    unsigned short sh = 0;
    for (int i = 0; i < SA_NUMERIC_MANTISSA_SIZE; ++i)
    {
        sh <<= 8;
        sh = (unsigned short)(sh + Devidend[SA_NUMERIC_MANTISSA_SIZE - i - 1]);

        assert(sh / Devisor == (unsigned char)(sh / Devisor));	// no truncation here
        Quotient[SA_NUMERIC_MANTISSA_SIZE - i - 1] = (unsigned char)(sh / Devisor);
        sh %= Devisor;
    }

    if (pRemainder)
        *pRemainder = (unsigned char)sh;
}

static bool MantissaIsZero(const unsigned char Mantissa[])
{
    for (unsigned int i = 0; i < SA_NUMERIC_MANTISSA_SIZE; ++i)
        if (Mantissa[i])
            return false;

    return true;
}

//////////////////////////////////////////////////////////////////////
// Implementation
//////////////////////////////////////////////////////////////////////

void SANumeric::InitZero()
{
    precision = 1;
    scale = 0;
    sign = 1;

    memset(val, 0, sizeof(val));
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SANumeric::SANumeric()
{
    InitZero();
}

// initializes from double
SANumeric::SANumeric(double dVal)
{
    InitZero();
    *this = dVal;
}

SANumeric::SANumeric(sa_int64_t iVal)
{
    InitZero();
    *this = iVal;
}

SANumeric::SANumeric(sa_uint64_t iVal)
{
    InitZero();
    *this = iVal;
}

bool SANumeric::setFromPlainString(const SAChar *sVal)
{
    InitZero();

    // set sign
    if (*sVal == _TSA('-'))
    {
        this->sign = 0;
        ++sVal;
    }
    else
    {
        this->sign = 1;
        if (*sVal == _TSA('+'))
            ++sVal;
    }

    // convert from string to little endian 10000-based number
    unsigned short Num10000[64];
    memset(Num10000, 0, sizeof(Num10000));
    unsigned int Num10000pos = 0;
    int i = (int)sa_strlen(sVal);
    if (i > 255)
        i = 255;	// truncate number
    this->precision = (unsigned char)i;
    while (i > 0)
    {
        if (i > 0 && (sVal[i - 1] == '.' || sVal[i - 1] == ','))
            this->scale = (unsigned char)(this->precision-- - i--);
        unsigned short i0 = (unsigned short)(i > 0 ? sVal[--i] - _TSA('0') : 0);
        if (i0 > 9)
            return false;
        if (i > 0 && (sVal[i - 1] == '.' || sVal[i - 1] == ','))
            this->scale = (unsigned char)(this->precision-- - i--);
        unsigned short i1 = (unsigned short)(i > 0 ? sVal[--i] - _TSA('0') : 0);
        if (i1 > 9)
            return false;
        if (i > 0 && (sVal[i - 1] == '.' || sVal[i - 1] == ','))
            this->scale = (unsigned char)(this->precision-- - i--);
        unsigned short i2 = (unsigned short)(i > 0 ? sVal[--i] - _TSA('0') : 0);
        if (i2 > 9)
            return false;
        if (i > 0 && (sVal[i - 1] == '.' || sVal[i - 1] == ','))
            this->scale = (unsigned char)(this->precision-- - i--);
        unsigned short i3 = (unsigned short)(i > 0 ? sVal[--i] - _TSA('0') : 0);
        if (i3 > 9)
            return false;

        if (Num10000pos < sizeof(Num10000) / sizeof(Num10000[0]))	// our precision is limited
            Num10000[Num10000pos] = (unsigned short)(i0 + 10 * i1 + 100 * i2 + 1000 * i3);
        ++Num10000pos;
    }

    // convert mantissa
    unsigned short Zero[sizeof(Num10000) / sizeof(Num10000[0])];
    memset(Zero, 0, sizeof(Zero));

    int Num256pos = 0;
    while (memcmp(Num10000, Zero, sizeof(Num10000)) != 0)
    {
        unsigned short Reminder;
        LittleEndian10000BaseDivide(
            (unsigned int)(sizeof(Num10000) / sizeof(Num10000[0])),
            Num10000,
            256,
            Num10000,
            &Reminder);

        if (Reminder != (unsigned char)Reminder)	// no truncation here
            return false;
        this->val[Num256pos++] = (unsigned char)Reminder;
        if (Num256pos == (int)(sizeof(this->val) / sizeof(this->val[0])))
            break;	// we have to truncate the number
    }

    return true;
}

bool SANumeric::setFromExpString(const SAString &sVal)
{
    size_t e = sVal.FindOneOf(_TSA("eEdD"));
    // check if there is a decimal point
    // it can be practically any symbol in general, so don't rely on '.' or ','
    size_t p = SIZE_MAX;
    struct lconv *plconv = localeconv();
    if (plconv && plconv->decimal_point)
    {
        SAChar chDecimalPoint = *plconv->decimal_point;
        p = sVal.Find(chDecimalPoint);
    }
    // just for insurance in case that localeconv haven't worked out as expected
    if (p == SIZE_MAX)
        p = sVal.Find(_TSA('.'));
    if (p == SIZE_MAX)
        p = sVal.Find(_TSA(','));
    if (p == SIZE_MAX)
        p = e;

    SAString sSign;
    SAString sInt;
    if (sVal.Left(1) == _TSA('-'))
    {
        sSign = sVal.Left(1);
        sInt = sVal.Mid(1, p - 1);
    }
    else
        sInt = sVal.Left(p);

    SAString sDec = sVal.Mid(p + 1, e - p - 1);
    sDec.TrimRight(_TSA('0'));
    SAString sExp = sVal.Mid(e + 1);
    int nExp10 = sa_toi(sExp);

    SAString sResult = sInt + sDec;

    size_t Precision = sResult.GetLength();
    size_t Scale = sDec.GetLength();

    if (nExp10 + 1 >= 0)
    {
        if ((size_t)(nExp10 + 1) < Precision)
            sResult.Insert(nExp10 + 1, _TSA('.'));
        else
            sResult += SAString(_TSA('0'), nExp10 - Scale);
    }
    else
        sResult = _TSA('.') + SAString(_TSA('0'), -(nExp10 + 1)) + sResult;

    return setFromPlainString(sSign + sResult);
}

SANumeric &SANumeric::operator=(double dVal)
{
    InitZero();

    SAString s;
    s.Format(_TSA("%.*e"), 14, dVal);

    // do nothing with #INF and #NaN results
    s.MakeUpper();
    if (s.Find(_TSA("INF")) != SIZE_MAX || s.Find(_TSA("NAN")) != SIZE_MAX)
        return *this;

    setFromExpString(s);

    return *this;
}

SANumeric &SANumeric::operator=(sa_int64_t iVal)
{
    InitZero();

#if defined(SQLAPI_WINDOWS)
    SAString s;
    s.Format(_TSA("%I64d"), iVal);
#else
    char szBuf[24];
    sprintf(szBuf, "%lld", (long long int)iVal);
    SAString s(szBuf);
#endif

    setFromPlainString(s);

    return *this;
}

SANumeric &SANumeric::operator=(sa_uint64_t iVal)
{
    InitZero();

#if defined(SQLAPI_WINDOWS)
    SAString s;
    s.Format(_TSA("%I64u"), iVal);
#else
    char szBuf[24];
    sprintf(szBuf, "%llu", (long long unsigned int)iVal);
    SAString s(szBuf);
#endif

    setFromPlainString(s);

    return *this;
}

// initialize from string
SANumeric &SANumeric::operator=(const SAChar *sVal)
{
    if (!sVal || !*sVal)
        throw SAException(SA_Library_Error, SA_Library_Error_WrongConversion, -1, IDS_CONVERTION_FROM_STRING_TO_NUMERIC_ERROR, (const SAChar*)sVal);

    bool bResult = true;
    SAString sValue(sVal);
    sValue.TrimLeft();
    sValue.TrimRight();

    if (sa_strchr(sVal, _TSA('e')) || sa_strchr(sVal, _TSA('E')))
        bResult = setFromExpString(sVal);
    else
        bResult = setFromPlainString(sVal);

    if (!bResult)
    {
        InitZero();
        throw SAException(SA_Library_Error, SA_Library_Error_WrongConversion, -1, IDS_CONVERTION_FROM_STRING_TO_NUMERIC_ERROR, (const SAChar*)sVal);
    }

    return *this;
}

//////////////////////////////////////////////////////////////////////
// Operators
//////////////////////////////////////////////////////////////////////

SANumeric::operator double() const
{
    // VARIANT I, wrong? precision problem
    /*
    double n = 0.0;
    double e = 1.0;
    for(unsigned int iVal = 0; iVal < sizeof(val); ++iVal)
    {
    n += this->val[iVal] * e;
    e *= 256.0;
    }

    for(unsigned char iScale = 0; iScale < this->scale; iScale++)
    n /= 10.0;

    if(this->sign == 0)	// for negative numbers
    n = -n;

    return n;
    */

    // VARIANT II, wrong becasue of possible ulDivisor oveflow
    /*
    double n = 0.0;
    double e = 1.0;
    for(unsigned int iVal = 0; iVal < sizeof(val); ++iVal)
    {
    n += this->val[iVal] * e;
    e *= 256.0;
    }

    // MSVS 6:
    //   error C2520: conversion from unsigned __int64 to double not implemented
    // sa_uint64_t ulDivisor = 1;
    sa_int64_t ulDivisor = 1;
    for(unsigned char iScale = 0; iScale < this->scale; iScale++)
    ulDivisor *= 10;
    n /= ulDivisor;

    if(this->sign == 0)	// for negative numbers
    n = -n;

    return n;
    */

    // VARIANT III - wrong, depends on locale
    /*
    SAString s = *this;
    return sa_strtod(s, NULL);
    */

    // VARIANT IV
    SAString s = *this;
    char cDecimal = *(localeconv()->decimal_point);
    if (cDecimal != '.')
        s.Replace(_TSA("."), SAString(cDecimal, 1));
    return sa_strtod((const SAChar*)s, NULL);
}

SANumeric::operator sa_int64_t() const
{
    sa_int64_t iVal;
    SAString s = this->operator SAString();;

#if defined(SQLAPI_WINDOWS)
    iVal = _atoi64((const char *)s.GetMultiByteChars());
#else
#ifdef SQLAPI_HPUX
#define strtoll strtol
#endif
#ifdef SQLAPI_SCOOSR5
#define strtoll strtol
#endif
    char *x;
    iVal = strtoll((const char *)s.GetMultiByteChars(), &x, 10);
#endif

    return iVal;
}

SANumeric::operator sa_uint64_t() const
{
    sa_uint64_t iVal;
    SAString s = this->operator SAString();;
    char *x;

    iVal = (sa_uint64_t)sa_strtoull((const char *)s.GetMultiByteChars(), &x, 10);

    return iVal;
}

SANumeric::operator SAString() const
{
    unsigned char Devidend[SA_NUMERIC_MANTISSA_SIZE];
    memcpy(Devidend, val, SA_NUMERIC_MANTISSA_SIZE);

    SAString s;
    if (MantissaIsZero(Devidend))
        s = _TSA("0");
    else
    {
        unsigned char iScale = this->scale;
        while (!MantissaIsZero(Devidend))
        {
            unsigned char Reminder;
            LittleEndianDivide(Devidend, 10, Devidend, &Reminder);

            s = SAChar(Reminder + '0') + s;
            if (iScale && iScale-- == 1)
                s = SAChar('.') + s;
        }
        while (iScale > 0)
        {
            s = SAChar('0') + s;
            if (iScale-- == 1)
                s = SAChar('.') + s;
        }
        if (((const SAChar*)s)[0] == SAChar('.'))
            s = SAChar('0') + s;
        if (this->sign == 0)	// for negative numbers
            s = _TSA("-") + s;
    }

    return s;
}


/*private static */
int SACommand::CompareIdentifier(
    const SAString &sIdentifier1,
    const SAString &sIdentifier2)
{
#ifdef ID_CASE_SENSITIVE
    return sIdentifier1.Compare(sIdentifier2);
#else
    return sIdentifier1.CompareNoCase(sIdentifier2);
#endif
}

/////////////////////////////////////////////////////////////////////////////
// SAInterval
/////////////////////////////////////////////////////////////////////////////

SAInterval::SAInterval() : m_interval(0)
{
}

SAInterval::SAInterval(double dVal) : m_interval(dVal)
{
}

SAInterval::SAInterval(long nDays, int nHours, int nMins, int nSecs)
{
    SetInterval(nDays, nHours, nMins, nSecs, 0);
}

SAInterval::SAInterval(long nDays, int nHours, int nMins, int nSecs, unsigned int nNanoSeconds)
{
    SetInterval(nDays, nHours, nMins, nSecs, nNanoSeconds);
}

static const double INTERVAL_HALFSECOND = 1.0 / (2.0 * (60.0 * 60.0 * 24.0));

double SAInterval::GetTotalDays() const
{
    return long(m_interval + (m_interval < 0 ?
        -INTERVAL_HALFSECOND : INTERVAL_HALFSECOND));
}

double SAInterval::GetTotalHours() const
{
    return long((m_interval + (m_interval < 0 ?
        -INTERVAL_HALFSECOND : INTERVAL_HALFSECOND)) * 24);
}

double SAInterval::GetTotalMinutes() const
{
    return long((m_interval + (m_interval < 0 ?
        -INTERVAL_HALFSECOND : INTERVAL_HALFSECOND)) * (24 * 60));
}

double SAInterval::GetTotalSeconds() const
{
    return long((m_interval + (m_interval < 0 ?
        -INTERVAL_HALFSECOND : INTERVAL_HALFSECOND)) * (24 * 60 * 60));
}

long SAInterval::GetDays() const
{
    return long(GetTotalDays());
}

long SAInterval::GetHours() const
{
    return long(GetTotalHours()) % 24;
}

long SAInterval::GetMinutes() const
{
    return long(GetTotalMinutes()) % 60;
}

long SAInterval::GetSeconds() const
{
    return long(GetTotalSeconds()) % 60;
}

unsigned int SAInterval::Fraction() const
{
    return (unsigned int)((m_interval + (m_interval < 0 ?
        -INTERVAL_HALFSECOND : INTERVAL_HALFSECOND)) * (24 * 60 * 60));
}

SAInterval& SAInterval::operator=(double dVal)
{
    m_interval = dVal;
    return *this;
}

SAInterval SAInterval::operator+(const SAInterval& interval) const
{
    SAInterval dtInterval;
    dtInterval.m_interval = m_interval + interval.m_interval;
    return dtInterval;
}

SAInterval SAInterval::operator-(const SAInterval& interval) const
{
    SAInterval dtInterval;
    dtInterval.m_interval = m_interval - interval.m_interval;
    return dtInterval;
}

SAInterval& SAInterval::operator+=(const SAInterval interval)
{
    *this = *this + interval;
    return *this;
}

SAInterval& SAInterval::operator-=(const SAInterval interval)
{
    *this = *this - interval;
    return *this;
}

SAInterval SAInterval::operator-() const
{
    return -this->m_interval;
}

SAInterval::operator double() const
{
    return m_interval;
}

SAInterval::operator SAString() const
{
    SAString s;
    unsigned int nFraction = Fraction();
    if (nFraction > 0)
    {
        s.Format(_TSA("%") SA_FMT_STR _TSA("%02u:%02u:%02u.%09u"),
            m_interval < 0 ? _TSA("-") : _TSA(""),
            labs(GetHours() + GetDays() * 24), labs(GetMinutes()), labs(GetSeconds()),
            m_nFraction);
        s.TrimRight(_TSA('0'));
    }
    else
        s.Format(_TSA("%") SA_FMT_STR _TSA("%02u:%02u:%02u"),
        m_interval < 0 ? _TSA("-") : _TSA(""),
        labs(GetHours() + GetDays() * 24), labs(GetMinutes()), labs(GetSeconds()));
    return s;
}

void SAInterval::SetInterval(long nDays, int nHours, int nMins, int nSecs, unsigned int nNanoSeconds)
{
    m_interval = nDays + ((double)nHours) / 24 + ((double)nMins) / (24 * 60) +
        ((double)nSecs) / (24 * 60 * 60);

    if (nNanoSeconds > 0)
        m_interval += ((double)(nNanoSeconds)) / (24.0*60.0*60.0*1e9);
}

