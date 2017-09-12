// odbcClient.cpp: implementation of the IodbcClient class.
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <odbcAPI.h>
#include "odbcClient.h"

#if !defined(SQLAPI_WINDOWS) && !defined(SA_ODBC_SQL_WCHART_CONVERT)
#if defined(SQL_WCHART_CONVERT) || defined(_IODBCUNIX_H)
#define SA_ODBC_SQL_WCHART_CONVERT 1
#else
#define SA_ODBC_SQL_WCHART_CONVERT 0
#endif
#else
#ifdef SQLAPI_WINDOWS
#define SA_ODBC_SQL_WCHART_CONVERT 1
#endif
#endif

#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
#define	TSAChar	UTF16
#else
#define	TSAChar	SAChar
#endif


// By default we will use character buffer for SANumeric
typedef SAChar SA_ODBC_SQL_NUMERIC_STRUCT[1024];

#define SA_OPT_ODBC_USE_SQLGETDATA          _TSA("ODBCUseSQLGetData")
#define SA_OPT_ODBC_ADD_LONGTEXTBUFSPACE    _TSA("ODBCAddLongTextBufferSpace")

//////////////////////////////////////////////////////////////////////
// IodbcConnection Class
//////////////////////////////////////////////////////////////////////

class IodbcConnection : public ISAConnection
{
    enum MaxLong
    {
        MaxLongAtExecSize = 0x7fffffff + SQL_LEN_DATA_AT_EXEC(0)
    };
    friend class IodbcCursor;

    odbcConnectionHandles m_handles;
    bool m_bUseNumeric;

    static void Check(
        SQLRETURN return_code,
        SQLSMALLINT HandleType,
        SQLHANDLE Handle);
    bool NeedLongDataLen();
    bool HasSchemaSupport();
    void issueIsolationLevel(
        SAIsolationLevel_t eIsolationLevel);

    static SADataType_t CnvtNativeToStd(
        int nNativeType,
        SQLULEN ColumnSize,
        SQLULEN nPrec,
        int nScale);
    static SQLSMALLINT CnvtStdToNative(
        SADataType_t eDataType);
    static SQLSMALLINT CnvtStdToNativeValueType(
        SADataType_t eDataType);

    static void CnvtNumericToInternal(
        const SANumeric &numeric,
        SQL_NUMERIC_STRUCT &Internal);
    static void CnvtNumericToInternal(
        const SANumeric &numeric,
        SA_ODBC_SQL_NUMERIC_STRUCT Internal,
        SQLLEN &StrLen_or_IndPtr);
    static void CnvtInternalToDateTime(
        SADateTime &date_time,
        const TIMESTAMP_STRUCT &Internal);
    static void CnvtDateTimeToInternal(
        const SADateTime &date_time,
        TIMESTAMP_STRUCT &Internal);

protected:
    virtual ~IodbcConnection();

public:
    IodbcConnection(SAConnection *pSAConnection);

    virtual void InitializeClient();
    virtual void UnInitializeClient();

    virtual long GetClientVersion() const;
    virtual long GetServerVersion() const;
    virtual SAString GetServerVersionString() const;

    virtual bool IsConnected() const;
    virtual bool IsAlive() const;
    virtual void Connect(
        const SAString &sDBString,
        const SAString &sUserID,
        const SAString &sPassword,
        saConnectionHandler_t fHandler);
    virtual void Disconnect();
    virtual void Destroy();
    virtual void Reset();

    virtual void setIsolationLevel(
        SAIsolationLevel_t eIsolationLevel);
    virtual void setAutoCommit(
        SAAutoCommit_t eAutoCommit);
    virtual void Commit();
    virtual void Rollback();

    virtual saAPI *NativeAPI() const;
    virtual saConnectionHandles *NativeHandles();

    virtual ISACursor *NewCursor(SACommand *m_pCommand);

    virtual void CnvtInternalToNumeric(
        SANumeric &numeric,
        const void *pInternal,
        int nInternalSize);

    virtual void CnvtInternalToDateTime(
        SADateTime &date_time,
        const void *pInternal,
        int nInternalSize);

    virtual void CnvtInternalToInterval(
        SAInterval &interval,
        const void *pInternal,
        int nInternalSize);

    virtual void CnvtInternalToCursor(
        SACommand *pCursor,
        const void *pInternal);
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IodbcConnection::IodbcConnection(
    SAConnection *pSAConnection) : ISAConnection(pSAConnection)
{
    Reset();
}

IodbcConnection::~IodbcConnection()
{
}

/*virtual */
void IodbcConnection::InitializeClient()
{
    ::AddODBCSupport(m_pSAConnection);

    // check if connection pooling is requested
    // connection pooling must be enabled in the environment 
    // before any handles are allocated
    const struct
    {
        const SAChar *SQL_CP_Name;
        SQLUINTEGER SQL_CP_Attr;
    } SQL_CP_Enable[] =
    {
        { _TSA("SQL_CP_OFF"), SQL_CP_OFF },
        { _TSA("SQL_CP_ONE_PER_DRIVER"), SQL_CP_ONE_PER_DRIVER },
        { _TSA("SQL_CP_ONE_PER_HENV"), SQL_CP_ONE_PER_HENV }
    };

    SAString sEnable = m_pSAConnection->Option(_TSA("SQL_ATTR_CONNECTION_POOLING"));
    if (!sEnable.IsEmpty())
    {
        for (unsigned int iEnable = 0; iEnable < sizeof(SQL_CP_Enable) / sizeof(SQL_CP_Enable[0]); ++iEnable)
            if (sEnable.CompareNoCase(SQL_CP_Enable[iEnable].SQL_CP_Name) == 0)
            {
                SQLRETURN ret = g_odbcAPI.SQLSetEnvAttr(
                    SQL_NULL_HANDLE,
                    SQL_ATTR_CONNECTION_POOLING,
                    (SQLPOINTER)SQL_CP_Enable[iEnable].SQL_CP_Attr, SQL_IS_INTEGER);
                if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
                    throw SAException(
                    SA_DBMS_API_Error,
                    ret, -1,
                    _TSA("SQLSetEnvAttr(SQL_ATTR_CONNECTION_POOLING) != SQL_SUCCESS"));
                break;
            }
    }


    assert(!m_handles.m_hevn);
    g_odbcAPI.SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_handles.m_hevn);
    assert(m_handles.m_hevn);

    try
    {
        // 3.0 level?
        SAString sVersion = m_pSAConnection->Option(_TSA("SQL_ATTR_ODBC_VERSION"));
        Check(g_odbcAPI.SQLSetEnvAttr(m_handles.m_hevn,
            SQL_ATTR_ODBC_VERSION,
            0 == sVersion.CompareNoCase(_TSA("SQL_OV_ODBC2")) ?
            SQLPOINTER(SQL_OV_ODBC2) : SQLPOINTER(SQL_OV_ODBC3), 0),
            SQL_HANDLE_ENV, m_handles.m_hevn);

        // if connection pooling has been requested set the matching condition
        const struct
        {
            const SAChar *SQL_CP_Name;
            SQLUINTEGER SQL_CP_Attr;
        } SQL_CP_Match[] =
        {
            { _TSA("SQL_CP_STRICT_MATCH"), SQL_CP_STRICT_MATCH },
            { _TSA("SQL_CP_RELAXED_MATCH"), SQL_CP_RELAXED_MATCH }
        };
        SAString sMatch = m_pSAConnection->Option(_TSA("SQL_ATTR_CP_MATCH"));
        if (!sMatch.IsEmpty())
        {
            for (unsigned int iMatch = 0; iMatch < sizeof(SQL_CP_Match) / sizeof(SQL_CP_Match[0]); ++iMatch)
                if (sMatch.CompareNoCase(SQL_CP_Match[iMatch].SQL_CP_Name) == 0)
                {
                    Check(g_odbcAPI.SQLSetEnvAttr(
                        m_handles.m_hevn,
                        SQL_ATTR_CP_MATCH,
                        SQLPOINTER(SQL_CP_Match[iMatch].SQL_CP_Attr),
                        SQL_IS_INTEGER), SQL_HANDLE_ENV, m_handles.m_hevn);
                    break;
                }
        }
    }
    catch (SAException &)	// clean up
    {
        if (m_handles.m_hevn)
        {
            g_odbcAPI.SQLFreeHandle(SQL_HANDLE_ENV, m_handles.m_hevn);
            m_handles.m_hevn = NULL;
        }

        throw;
    }
}

/*virtual */
void IodbcConnection::UnInitializeClient()
{
    assert(m_handles.m_hevn);

    Check(g_odbcAPI.SQLFreeHandle(SQL_HANDLE_ENV, m_handles.m_hevn), SQL_HANDLE_ENV, m_handles.m_hevn);
    m_handles.m_hevn = NULL;

    if (SAGlobals::UnloadAPI()) ::ReleaseODBCSupport();
}

bool IodbcConnection::NeedLongDataLen()
{
    SQLSMALLINT retlen = 0;
    TSAChar szValue[10];
    Check(g_odbcAPI.SQLGetInfo(
        m_handles.m_hdbc, SQL_NEED_LONG_DATA_LEN, szValue, (SQLSMALLINT)sizeof(szValue), &retlen),
        SQL_HANDLE_DBC, m_handles.m_hdbc);
    if (retlen > 0)
    {
        SAString sValue; sValue.SetUTF16Chars(szValue, 1);
        return 0 == sValue.CompareNoCase(_TSA("Y"));
    }

    return false;
}

bool IodbcConnection::HasSchemaSupport()
{
    TSAChar szValue[256];
    SQLSMALLINT schemaTermActualLen = 0;
    Check(g_odbcAPI.SQLGetInfo(m_handles.m_hdbc, SQL_SCHEMA_TERM, szValue,
        (SQLSMALLINT)sizeof(szValue), &schemaTermActualLen), SQL_HANDLE_DBC, m_handles.m_hdbc);
    // if there is no schema terminator (i.e. length = 0, assume this database does not support schema's
    return (schemaTermActualLen > 0);
}

/*virtual */
void IodbcConnection::CnvtInternalToDateTime(
    SADateTime &date_time,
    const void *pInternal,
    int nInternalSize)
{
    assert((size_t)nInternalSize == sizeof(TIMESTAMP_STRUCT));
    if ((size_t)nInternalSize != sizeof(TIMESTAMP_STRUCT))
        return;
    CnvtInternalToDateTime(date_time, *(const TIMESTAMP_STRUCT*)pInternal);
}

/*static */
void IodbcConnection::CnvtInternalToDateTime(
    SADateTime &date_time,
    const TIMESTAMP_STRUCT &Internal)
{
    date_time = SADateTime(
        Internal.year,
        Internal.month,
        Internal.day,
        Internal.hour,
        Internal.minute,
        Internal.second);

    date_time.Fraction() = Internal.fraction;
}

/*static */
void IodbcConnection::CnvtDateTimeToInternal(
    const SADateTime &date_time,
    TIMESTAMP_STRUCT &Internal)
{
    Internal.year = (SQLSMALLINT)(date_time.GetYear());
    Internal.month = (SQLUSMALLINT)(date_time.GetMonth());
    Internal.day = (SQLUSMALLINT)date_time.GetDay();
    Internal.hour = (SQLUSMALLINT)date_time.GetHour();
    Internal.minute = (SQLUSMALLINT)date_time.GetMinute();
    Internal.second = (SQLUSMALLINT)date_time.GetSecond();

    Internal.fraction = date_time.Fraction();
}

/*virtual */
void IodbcConnection::CnvtInternalToInterval(
    SAInterval & /*interval*/,
    const void * /*pInternal*/,
    int /*nInternalSize*/)
{
    assert(false);
}

/*virtual */
void IodbcConnection::CnvtInternalToCursor(
    SACommand * /*pCursor*/,
    const void * /*pInternal*/)
{
    assert(false);
}

/*virtual */
long IodbcConnection::GetClientVersion() const
{
    if (g_nODBCDLLVersionLoaded == 0)	// has not been detected
    {
        if (IsConnected())
        {
            TSAChar szInfoValue[1024];
            SQLSMALLINT cbInfoValue;
            g_odbcAPI.SQLGetInfo(m_handles.m_hdbc, SQL_DRIVER_VER, szInfoValue, 1024, &cbInfoValue);
            szInfoValue[cbInfoValue / sizeof(TSAChar)] = 0;

            SAString sInfo;
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
            sInfo.SetUTF16Chars(szInfoValue);
#else
            sInfo = szInfoValue;
#endif

            SAChar *sPoint;
            short nMajor = (short)sa_strtol(sInfo, &sPoint, 10);
            assert(*sPoint == _TSA('.'));

            sPoint++;
            short nMinor = (short)sa_strtol(sPoint, &sPoint, 10);
            return SA_MAKELONG(nMinor, nMajor);
        }
    }

    return g_nODBCDLLVersionLoaded;
}

/*virtual */
long IodbcConnection::GetServerVersion() const
{
    assert(m_handles.m_hdbc);

    TSAChar szInfoValue[1024];
    SQLSMALLINT cbInfoValue;
    g_odbcAPI.SQLGetInfo(m_handles.m_hdbc, SQL_DBMS_VER, szInfoValue, 1024, &cbInfoValue);
    szInfoValue[cbInfoValue / sizeof(TSAChar)] = 0;

    SAString sInfo;
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
    sInfo.SetUTF16Chars(szInfoValue);
#else
    sInfo = szInfoValue;
#endif

    SAChar *sPoint;
    short nMajor = (short)sa_strtol(sInfo, &sPoint, 10);
    assert(*sPoint == _TSA('.'));
    sPoint++;
    short nMinor = (short)sa_strtol(sPoint, &sPoint, 10);
    return SA_MAKELONG(nMinor, nMajor);
}

/*virtual */
SAString IodbcConnection::GetServerVersionString() const
{
    assert(m_handles.m_hdbc);

    TSAChar szInfoValue[1024];
    SQLSMALLINT cbInfoValue;
    g_odbcAPI.SQLGetInfo(m_handles.m_hdbc, SQL_DBMS_NAME, szInfoValue, 1024, &cbInfoValue);
    szInfoValue[cbInfoValue / sizeof(TSAChar)] = 0;

    SAString sInfo;
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
    sInfo.SetUTF16Chars(szInfoValue);
#else
    sInfo = szInfoValue;
#endif

    SAString s = sInfo;
    s += _TSA(" Release ");

    g_odbcAPI.SQLGetInfo(m_handles.m_hdbc, SQL_DBMS_VER, szInfoValue, 1024, &cbInfoValue);
    szInfoValue[cbInfoValue / sizeof(TSAChar)] = 0;

#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
    sInfo.SetUTF16Chars(szInfoValue);
#else
    sInfo = szInfoValue;
#endif

    s += sInfo;

    return s;
}

#define SA_ODBC_MAX_MESSAGE_LENGTH	4096

/*static */
void IodbcConnection::Check(
    SQLRETURN return_code,
    SQLSMALLINT HandleType,
    SQLHANDLE Handle)
{
    if (return_code == SQL_SUCCESS)
        return;
    if (return_code == SQL_SUCCESS_WITH_INFO)
        return;

    SQLTCHAR Sqlstate[5 + 1];
    SQLINTEGER NativeError = SQLINTEGER(0), NativeError2;
    SQLTCHAR szMsg[SA_ODBC_MAX_MESSAGE_LENGTH + 1];
    SAString sMsg;
    SQLSMALLINT TextLength;

    SQLSMALLINT i = SQLSMALLINT(1);
    SQLRETURN rc;
    SAException* pNested = NULL;

    do
    {
        // Zero fill the string buffers - some drivers doesn't null terminate this strings
        memset(Sqlstate, 0, sizeof(Sqlstate));
        memset(szMsg, 0, sizeof(szMsg));

        rc = g_odbcAPI.SQLGetDiagRec(HandleType, Handle,
            i++, Sqlstate, &NativeError2,
            szMsg, SA_ODBC_MAX_MESSAGE_LENGTH, &TextLength);

        if (SQL_SUCCESS == rc || SQL_SUCCESS_WITH_INFO == rc)
        {
            if (sMsg.GetLength() > 0)
                pNested = new SAException(pNested, SA_DBMS_API_Error, NativeError, -1, sMsg);

            NativeError = NativeError2;

#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
            SAString s; s.SetUTF16Chars(Sqlstate);
            sMsg += s;
#else
            sMsg += Sqlstate;
#endif
            sMsg += _TSA(" ");
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
            s.SetUTF16Chars(szMsg);
            sMsg += s;
#else
            sMsg += szMsg;
#endif
        }
    } while (SQL_SUCCESS == rc);


    if (SQL_SUCCESS != rc &&
        SQL_SUCCESS_WITH_INFO != rc &&
        SQL_NO_DATA != rc)
    {
        // iODBC can return SQL_ERROR after all errors was fetched successfully
        if (!sMsg.IsEmpty())
            sMsg += _TSA("\n");
        if (!NativeError)
            NativeError = return_code;
        sMsg += _TSA("rc != SQL_SUCCESS");
    }

    throw SAException(pNested, SA_DBMS_API_Error, NativeError, -1, sMsg);
}

/*virtual */
bool IodbcConnection::IsConnected() const
{
    return (NULL != m_handles.m_hdbc);
}

/*virtual */
bool IodbcConnection::IsAlive() const
{
    assert(m_handles.m_hdbc);

    SQLRETURN rc;
    SQLUINTEGER uVal;
    rc = g_odbcAPI.SQLGetConnectAttr(
        m_handles.m_hdbc, SQL_ATTR_CONNECTION_DEAD,
        (SQLPOINTER)&uVal, SQL_IS_UINTEGER, NULL);

    if ((SQL_SUCCESS == rc || SQL_SUCCESS_WITH_INFO == rc)
        && SQL_CD_TRUE == uVal)
        return false;

    return true;
}

/*virtual */
void IodbcConnection::Connect(
    const SAString &sDBString,
    const SAString &sUserID,
    const SAString &sPassword,
    saConnectionHandler_t fHandler)
{
    assert(m_handles.m_hevn);	// allocated in InitializeClient
    assert(!m_handles.m_hdbc);

    try
    {
        Check(g_odbcAPI.SQLAllocHandle(SQL_HANDLE_DBC, m_handles.m_hevn,
            &m_handles.m_hdbc), SQL_HANDLE_ENV, m_handles.m_hevn);

        if (NULL != fHandler)
            fHandler(*m_pSAConnection, SA_PreConnectHandler);

        SAString sOption = m_pSAConnection->Option(_TSA("SQL_ATTR_LOGIN_TIMEOUT"));
        if (!sOption.IsEmpty())
        {
            SQLPOINTER attrVal = (SQLPOINTER)sa_tol((const SAChar*)sOption);
            g_odbcAPI.SQLSetConnectAttr(m_handles.m_hdbc, SQL_ATTR_LOGIN_TIMEOUT, attrVal, SQL_IS_INTEGER);
        }

        sOption = m_pSAConnection->Option(_TSA("SQL_ATTR_CONNECTION_TIMEOUT"));
        if (!sOption.IsEmpty())
        {
            SQLPOINTER attrVal = (SQLPOINTER)sa_tol((const SAChar*)sOption);
            g_odbcAPI.SQLSetConnectAttr(m_handles.m_hdbc, SQL_ATTR_CONNECTION_TIMEOUT, attrVal, SQL_IS_INTEGER);
        }

        SQLUSMALLINT uDriverCompletion = SQL_DRIVER_NOPROMPT;
        sOption = m_pSAConnection->Option(_TSA("SQL_DRIVER_PROMPT"));
        if (!sOption.IsEmpty())
            uDriverCompletion = SQL_DRIVER_PROMPT;
        else
        {
            sOption = m_pSAConnection->Option(_TSA("SQL_DRIVER_COMPLETE"));
            if (!sOption.IsEmpty())
                uDriverCompletion = SQL_DRIVER_COMPLETE;
            else
            {
                sOption = m_pSAConnection->Option(_TSA("SQL_DRIVER_COMPLETE_REQUIRED"));
                if (!sOption.IsEmpty())
                    uDriverCompletion = SQL_DRIVER_COMPLETE_REQUIRED;
            }
        }

        if (sOption.IsEmpty() && sDBString.Find(_TSA('=')) == SIZE_MAX)	// it's not a valid connection string, but it can be DSN name
            Check(g_odbcAPI.SQLConnect(m_handles.m_hdbc,
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
            (SQLTCHAR *)sDBString.GetUTF16Chars(), SQL_NTS,
            (SQLTCHAR *)sUserID.GetUTF16Chars(), SQL_NTS,
            (SQLTCHAR *)sPassword.GetUTF16Chars(), SQL_NTS),
#else
            (SQLTCHAR *)(const SAChar*)sDBString, SQL_NTS,
            (SQLTCHAR *)(const SAChar*)sUserID, SQL_NTS,
            (SQLTCHAR *)(const SAChar*)sPassword, SQL_NTS),
#endif				
            SQL_HANDLE_DBC, m_handles.m_hdbc);
        else
        {
            SAString s = sDBString;
            if (!sUserID.IsEmpty())
            {
                s += _TSA(";UID=");
                s += sUserID;
            }
            if (!sPassword.IsEmpty())
            {
                s += _TSA(";PWD=");
                s += sPassword;
            }

            // we don't need StringLength2 on output, but UnixODBC manager can "core dump"
            // if we pass NULL instead of &StringLength2 (pointer to SQLSMALLINT)
            SQLTCHAR OutConnectionString[1024];
            SQLSMALLINT StringLength2 = 0;
#ifdef SQLAPI_WINDOWS
            SQLHWND hWnd = NULL;
            if (!sOption.IsEmpty())
                hWnd = (SQLHWND)sa_strtoul((const SAChar*)sOption, NULL, 16);
            Check(g_odbcAPI.SQLDriverConnect(
                m_handles.m_hdbc, hWnd,
                (SQLTCHAR *)(const SAChar*)s,
                SQL_NTS, OutConnectionString, 1024, &StringLength2, uDriverCompletion),
                SQL_HANDLE_DBC, m_handles.m_hdbc);
#else
            Check(g_odbcAPI.SQLDriverConnect(
                m_handles.m_hdbc, NULL,
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
                (SQLTCHAR *)s.GetUTF16Chars(),
#else
                (SQLTCHAR *)(const SAChar*)s,
#endif				
                SQL_NTS, OutConnectionString, 1024, &StringLength2, uDriverCompletion),
                SQL_HANDLE_DBC, m_handles.m_hdbc);
#endif

            if (StringLength2 > 0)
            {
                OutConnectionString[StringLength2] = '\0';
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
                m_pSAConnection->setOption(_TSA("DSN")).SetUTF16Chars(OutConnectionString);
#else
                m_pSAConnection->setOption(_TSA("DSN")) = OutConnectionString;
#endif
            }
        }

        sOption = m_pSAConnection->Option(_TSA("SQL_ATTR_ODBC_CURSORS"));
        if (!sOption.IsEmpty())
        {
            SQLPOINTER pVal = (SQLPOINTER)-1;
            if (0 == sOption.CompareNoCase(_TSA("SQL_CUR_USE_ODBC")))
                pVal = (SQLPOINTER)SQL_CUR_USE_ODBC;
            else if (0 == sOption.CompareNoCase(_TSA("SQL_CUR_USE_DRIVER")))
                pVal = (SQLPOINTER)SQL_CUR_USE_DRIVER;
            else if (0 == sOption.CompareNoCase(_TSA("SQL_CUR_USE_IF_NEEDED")))
                pVal = (SQLPOINTER)SQL_CUR_USE_IF_NEEDED;

            // do not check errors from the next call
            // we only want this functionality if it is supported
            // --
            // 2009-02-18: someone who needs this functionality should define
            // this option because some drivers can crash.
            if (pVal != (SQLPOINTER)-1)
                g_odbcAPI.SQLSetConnectAttr(m_handles.m_hdbc,
                SQL_ATTR_ODBC_CURSORS, pVal, 0);
        }

        if (NULL != fHandler)
            fHandler(*m_pSAConnection, SA_PostConnectHandler);
    }
    catch (SAException &)
    {
        // clean up
        if (m_handles.m_hdbc)
        {
            g_odbcAPI.SQLFreeHandle(SQL_HANDLE_DBC, m_handles.m_hdbc);
            m_handles.m_hdbc = NULL;
        }

        throw;
    }

    SAString sOption = m_pSAConnection->Option(_TSA("ODBCUseNumeric"));
    if (0 == sOption.CompareNoCase(_TSA("TRUE")) ||
        0 == sOption.CompareNoCase(_TSA("1")))
        m_bUseNumeric = true;
}

/*virtual */
void IodbcConnection::Disconnect()
{
    assert(m_handles.m_hdbc);

    Check(g_odbcAPI.SQLDisconnect(m_handles.m_hdbc),
        SQL_HANDLE_DBC, m_handles.m_hdbc);
    Check(g_odbcAPI.SQLFreeHandle(SQL_HANDLE_DBC, m_handles.m_hdbc),
        SQL_HANDLE_DBC, m_handles.m_hdbc);

    m_handles.m_hdbc = NULL;
}

/*virtual */
void IodbcConnection::Destroy()
{
    assert(m_handles.m_hdbc);

    g_odbcAPI.SQLDisconnect(m_handles.m_hdbc);
    g_odbcAPI.SQLFreeHandle(SQL_HANDLE_DBC, m_handles.m_hdbc);
    m_handles.m_hdbc = NULL;
}

/*virtual */
void IodbcConnection::Reset()
{
    m_handles.m_hdbc = NULL;
    m_bUseNumeric = false;
}

/*virtual */
void IodbcConnection::Commit()
{
    assert(m_handles.m_hdbc);

    Check(g_odbcAPI.SQLEndTran(SQL_HANDLE_DBC, m_handles.m_hdbc, SQL_COMMIT),
        SQL_HANDLE_DBC, m_handles.m_hdbc);
}

/*virtual */
void IodbcConnection::Rollback()
{
    assert(m_handles.m_hdbc);

    Check(g_odbcAPI.SQLEndTran(SQL_HANDLE_DBC, m_handles.m_hdbc, SQL_ROLLBACK),
        SQL_HANDLE_DBC, m_handles.m_hdbc);
}

//////////////////////////////////////////////////////////////////////
// IodbcClient Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IodbcClient::IodbcClient()
{

}

IodbcClient::~IodbcClient()
{

}

ISAConnection *IodbcClient::QueryConnectionInterface(
    SAConnection *pSAConnection)
{
    return new IodbcConnection(pSAConnection);
}

//////////////////////////////////////////////////////////////////////
// IodbcCursor Class
//////////////////////////////////////////////////////////////////////

class IodbcCursor : public ISACursor
{
    odbcCommandHandles	m_handles;

    SQLUINTEGER m_cRowsToPrefetch;	// defined in SetSelectBuffers
    SQLUINTEGER m_cRowsObtained;
    SQLUINTEGER m_cRowCurrent;

    SAString CallSubProgramSQL();

    SQLRETURN BindLongs();

    bool m_bResultSetCanBe;
    SQLLEN m_nRowsAffected;
    int m_nFirstLongFieldIndex;

    void ProcessBatchUntilEndOrResultSet();
    void Check(
        SQLRETURN return_code,
        SQLSMALLINT HandleType,
        SQLHANDLE Handle) const;

protected:
    virtual size_t InputBufferSize(
        const SAParam &Param) const;
    virtual size_t OutputBufferSize(
        SADataType_t eDataType,
        size_t nDataSize) const;
    virtual void SetFieldBuffer(
        int nCol,	// 1-based
        void *pInd,
        size_t nIndSize,
        void *pSize,
        size_t nSizeSize,
        void *pValue,
        size_t nValueSize);
    virtual bool ConvertIndicator(
        int nPos,	// 1-based
        int nNotConverted,
        SAValueRead &vr,
        ValueType_t eValueType,
        void *pInd, size_t nIndSize,
        void *pSize, size_t nSizeSize,
        size_t &nRealSize,
        int nBulkReadingBufPos) const;
    virtual bool ConvertValue(
        int nPos,	// 1 - based
        int nNotConverted,
        size_t nIndSize,
        void *pNull,
        size_t nSizeSize,
        void *pSize,
        size_t nBufSize,
        void *pValue,
        ValueType_t eValueType,
        SAValueRead &vr,
        int nBulkReadingBufPos);
    virtual bool IgnoreUnknownDataType();

public:
    IodbcCursor(
        IodbcConnection *pIodbcConnection,
        SACommand *pCommand);
    virtual ~IodbcCursor();

    virtual bool IsOpened();
    virtual void Open();
    virtual void Close();
    virtual void Destroy();
    virtual void Reset();

    virtual void Prepare(
        const SAString &sStmt,
        SACommandType_t eCmdType,
        int nPlaceHolderCount,
        saPlaceHolder **ppPlaceHolders);
    // binds parameters
    void Bind(
        int nPlaceHolderCount,
        saPlaceHolder **ppPlaceHolders);
    // executes statement
    virtual void Execute(
        int nPlaceHolderCount,
        saPlaceHolder **ppPlaceHolders);
    // cleans up after execute if needed, so the statement can be reexecuted
    virtual void UnExecute();
    virtual void Cancel();
    virtual bool ResultSetExists();
    virtual void DescribeFields(
        DescribeFields_cb_t fn);
    virtual void SetSelectBuffers();
    virtual bool FetchNext();
    virtual bool FetchPrior();
    virtual bool FetchFirst();
    virtual bool FetchLast();
    virtual bool FetchPos(int offset, bool Relative = false);

    virtual long GetRowsAffected();

    virtual void ReadLongOrLOB(
        ValueType_t eValueType,
        SAValueRead &vr,
        void *pValue,
        size_t nFieldBufSize,
        saLongOrLobReader_t fnReader,
        size_t nReaderWantedPieceSize,
        void *pAddlData);

    virtual void DescribeParamSP();

    virtual saCommandHandles *NativeHandles();

    virtual void ConvertString(
        SAString &String,
        const void *pData,
        size_t nRealSize);
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IodbcCursor::IodbcCursor(
    IodbcConnection *pIodbcConnection,
    SACommand *pCommand) :
    ISACursor(pIodbcConnection, pCommand)
{
    Reset();
}

/*virtual */
IodbcCursor::~IodbcCursor()
{
}

void IodbcCursor::Check(
    SQLRETURN return_code,
    SQLSMALLINT HandleType,
    SQLHANDLE Handle) const
{
    try
    {
        IodbcConnection::Check(return_code, HandleType, Handle);
        return;
    }
    catch (SAException& x)
    {
        if (0 == x.ErrNativeCode())
        {
            SAString sOption = m_pCommand->Option(_TSA("CheckDetectHandle"));
            bool bDetect = (
                0 == sOption.CompareNoCase(_TSA("1")) ||
                0 == sOption.CompareNoCase(_TSA("TRUE")));

            if (bDetect)
            {
                IodbcConnection* pCon = (IodbcConnection*)m_pISAConnection;

                switch (HandleType)
                {
                case SQL_HANDLE_STMT:
                {
                    try {
                        IodbcConnection::Check(return_code, SQL_HANDLE_DBC, pCon->m_handles.m_hdbc);
                        return;
                    }
                    catch (SAException& x) {
                        if (0 != x.ErrNativeCode())
                            throw;
                    }
                }

                case SQL_HANDLE_DBC:
                {
                    try {
                        IodbcConnection::Check(return_code, SQL_HANDLE_ENV, pCon->m_handles.m_hevn);
                        return;
                    }
                    catch (SAException& x) {
                        if (0 != x.ErrNativeCode())
                            throw;
                    }
                }
                }
            }
        }

        throw;
    }

}

/*virtual */
size_t IodbcCursor::InputBufferSize(
    const SAParam &Param) const
{
    if (!Param.isNull())
    {
        switch (Param.DataType())
        {
        case SA_dtBool:
            return sizeof(unsigned char);	// SQL_C_BIT
        case SA_dtNumeric:
            if (((IodbcConnection*)m_pISAConnection)->m_bUseNumeric)
                return sizeof(SQL_NUMERIC_STRUCT);	// SQL_C_NUMERIC
            else
                return sizeof(SA_ODBC_SQL_NUMERIC_STRUCT);	// SQL_C_TCHAR
        case SA_dtDateTime:
            return sizeof(TIMESTAMP_STRUCT);
        case SA_dtString:
            return (Param.asString().GetLength() * sizeof(TSAChar));
        case SA_dtLongBinary:
        case SA_dtLongChar:
        case SA_dtBLob:
        case SA_dtCLob:
            return 0;
        default:
            break;
        }
    }

    return ISACursor::InputBufferSize(Param);
}

/*static */
SADataType_t IodbcConnection::CnvtNativeToStd(
    int dbtype,
    SQLULEN ColumnSize,
    SQLULEN prec,
    int scale)
{
    SADataType_t eDataType = SA_dtUnknown;

    switch (dbtype)
    {
#ifdef SQL_WCHAR
    case SQL_WCHAR:		// Unicode character string of fixed length
#endif // SQL_WCHAR
#ifdef SQL_WVARCHAR
    case SQL_WVARCHAR:	// Unicode variable-length character string
#endif // SQL_WVARCHAR
    case SQL_CHAR:		// Character string of fixed length
    case SQL_VARCHAR:	// Variable-length character string
        if (ColumnSize > 0 && ColumnSize <= 0xFFFF)
            // some drivers (e.g. Access) can report ColumnSize ~= MAX_INT for constants
            // MS ODBC driver sets ColumnSize = 0 for [n]varchar(max) fields
            eDataType = SA_dtString;
        else
            eDataType = SA_dtLongChar;
        break;
    case SQL_BINARY:
    case SQL_VARBINARY:
        if (ColumnSize > 0 && ColumnSize <= 0xFFFF)
            // some drivers (e.g. Access) can report ColumnSize ~= MAX_UINT for constants
            // MS ODBC driver sets ColumnSize = 0 for [n]varchar(max) fields
            eDataType = SA_dtBytes;
        else
            eDataType = SA_dtLongBinary;
        break;
    case SQL_LONGVARCHAR:	// Variable-length character data
#ifdef SQL_WLONGVARCHAR
    case SQL_WLONGVARCHAR:	// Variable-length character data
#endif
    case -152: // SQL Server Native ODBC driver can report SQL_SS_XML data type
        eDataType = SA_dtLongChar;
        break;
    case SQL_LONGVARBINARY: // Variable-length binary data
        eDataType = SA_dtLongBinary;
        break;
    case SQL_DECIMAL:
    case SQL_NUMERIC:
        // we do not use SA_dtNumeric
        // because not all drivers support SQL_NUMERIC/SQL_C_NUMERIC
        // !!! Now we use SA_dtNumeric but according to the connection flag m_bUseNumeric 
        // we choose the data buffer type - SQL_DOUBLE or SQL_C_NUMERIC
        if (scale <= 0)	// check for exact type
        {
            if (prec < 5)
                eDataType = SA_dtShort;
            else if (prec < 10)
                eDataType = SA_dtLong;
            else
                //eDataType = SA_dtDouble;	//SA_dtNumeric;
                eDataType = SA_dtNumeric;
        }
        else
            // eDataType = SA_dtDouble;	//SA_dtNumeric;
            eDataType = SA_dtNumeric;
        break;
    case SQL_SMALLINT:
        eDataType = SA_dtShort;
        break;
    case SQL_INTEGER:
        eDataType = SA_dtLong;
        break;
    case SQL_REAL:
        eDataType = SA_dtDouble;
        break;
    case SQL_FLOAT:
        eDataType = SA_dtDouble;
        break;
    case SQL_DOUBLE:
        eDataType = SA_dtDouble;
        break;
    case SQL_BIT:	// Single bit binary data
        eDataType = SA_dtBool;
        break;
    case SQL_TINYINT:
        eDataType = SA_dtShort;
        break;
    case SQL_BIGINT:
        // we do not use SA_dtNumeric
        // because not all drivers support SQL_NUMERIC/SQL_C_NUMERIC
        // eDataType = SA_dtDouble; //SA_dtNumeric;
        // !!! Now we use SA_dtNumeric but according to the connection flag m_bUseNumeric 
        // we choose the data buffer type - SQL_DOUBLE or SQL_C_NUMERIC
        eDataType = SA_dtNumeric;
        break;
    case SQL_TIME:
    case SQL_DATE:		// == SQL_DATETIME
    case SQL_TIMESTAMP:
    case SQL_TYPE_DATE:
    case SQL_TYPE_TIME:
    case SQL_TYPE_TIMESTAMP:
        eDataType = SA_dtDateTime;
        break;
    case SQL_GUID:
        eDataType = SA_dtBytes;
        break;
    default:
        eDataType = SA_dtUnknown;
        break;
    }

    return eDataType;
}

/*static */
SQLSMALLINT IodbcConnection::CnvtStdToNativeValueType(SADataType_t eDataType)
{
    SQLSMALLINT ValueType;

    switch (eDataType)
    {
    case SA_dtUnknown:
        throw SAException(SA_Library_Error, SA_Library_Error_UnknownDataType, -1, IDS_UNKNOWN_DATA_TYPE);
    case SA_dtBool:
        ValueType = SQL_C_BIT;
        break;
    case SA_dtShort:
        ValueType = SQL_C_SSHORT;
        break;
    case SA_dtUShort:
        ValueType = SQL_C_USHORT;
        break;
    case SA_dtLong:
        ValueType = SQL_C_SLONG;
        break;
    case SA_dtULong:
        ValueType = SQL_C_ULONG;
        break;
    case SA_dtDouble:
        ValueType = SQL_C_DOUBLE;
        break;
    case SA_dtNumeric:
        ValueType = SQL_C_NUMERIC;
        break;
    case SA_dtDateTime:
        ValueType = SQL_C_TYPE_TIMESTAMP;
        break;
    case SA_dtString:
        ValueType = SQL_C_TCHAR;
        break;
    case SA_dtBytes:
        ValueType = SQL_C_BINARY;
        break;
    case SA_dtLongBinary:
    case SA_dtBLob:
        ValueType = SQL_C_BINARY;
        break;
    case SA_dtLongChar:
    case SA_dtCLob:
        ValueType = SQL_C_TCHAR;
        break;
    default:
        ValueType = 0;
        assert(false);
    }

    return ValueType;
}

/*static */
SQLSMALLINT IodbcConnection::CnvtStdToNative(SADataType_t eDataType)
{
    SQLSMALLINT dbtype;

    switch (eDataType)
    {
    case SA_dtUnknown:
        throw SAException(SA_Library_Error, SA_Library_Error_UnknownDataType, -1, IDS_UNKNOWN_DATA_TYPE);
    case SA_dtBool:
        dbtype = SQL_BIT;
        break;
    case SA_dtShort:
        dbtype = SQL_SMALLINT;
        break;
    case SA_dtUShort:
        dbtype = SQL_SMALLINT; // ??? SQL_INTEGER;
        break;
    case SA_dtLong:
        dbtype = SQL_INTEGER; // ??? SQL_BIGINT;
        break;
    case SA_dtULong:
        dbtype = SQL_INTEGER;
        break;
    case SA_dtDouble:
        dbtype = SQL_DOUBLE;
        break;
    case SA_dtNumeric:
        dbtype = SQL_NUMERIC;
        break;
    case SA_dtDateTime:
        dbtype = SQL_TYPE_TIMESTAMP;
        break;
    case SA_dtString:
#ifdef SA_UNICODE
        dbtype = SQL_UNICODE_VARCHAR;
#else
        dbtype = SQL_VARCHAR;
#endif
        break;
    case SA_dtBytes:
        dbtype = SQL_BINARY;
        break;
    case SA_dtLongBinary:
    case SA_dtBLob:
        dbtype = SQL_LONGVARBINARY;
        break;
    case SA_dtLongChar:
    case SA_dtCLob:
#ifdef SA_UNICODE
        dbtype = SQL_UNICODE_LONGVARCHAR;
#else
        dbtype = SQL_LONGVARCHAR;
#endif
        break;
    default:
        dbtype = 0;
        assert(false);
    }

    return dbtype;
}

/*virtual */
bool IodbcCursor::IsOpened()
{
    return (NULL != m_handles.m_hstmt);
}

/*virtual */
void IodbcCursor::Open()
{
    assert(m_handles.m_hstmt == NULL);
    Check(g_odbcAPI.SQLAllocHandle(SQL_HANDLE_STMT,
        ((IodbcConnection*)m_pISAConnection)->m_handles.m_hdbc, &m_handles.m_hstmt),
        SQL_HANDLE_DBC, ((IodbcConnection*)m_pISAConnection)->m_handles.m_hdbc);
    assert(m_handles.m_hstmt != NULL);

    if (isSetScrollable())
    {
        g_odbcAPI.SQLSetStmtAttr(m_handles.m_hstmt,
            SQL_ATTR_CURSOR_TYPE, (SQLPOINTER)SQL_CURSOR_DYNAMIC, SQL_IS_INTEGER);
        g_odbcAPI.SQLSetStmtAttr(m_handles.m_hstmt,
            SQL_ATTR_CONCURRENCY, (SQLPOINTER)SQL_CONCUR_LOCK/*SQL_CONCUR_ROWVER*/, SQL_IS_INTEGER);

        /* alternative
        g_odbcAPI.SQLSetStmtAttr(m_handles.m_hstmt, SQL_ATTR_CURSOR_SCROLLABLE
        , (SQLPOINTER)  SQL_SCROLLABLE, SQL_IS_INTEGER);
        g_odbcAPI.SQLSetStmtAttr(m_handles.m_hstmt, SQL_CURSOR_SENSITIVITY
        , (SQLPOINTER) SQL_SENSITIVE, SQL_IS_INTEGER);
        */
    }

    SAString sOption = m_pCommand->Option(_TSA("SQL_ATTR_CONCURRENCY"));
    if (!sOption.IsEmpty())
    {
        SQLPOINTER attrVal = (SQLPOINTER)SQL_CONCUR_READ_ONLY;
        if (0 == sOption.CompareNoCase(_TSA("SQL_CONCUR_READONLY")))
            attrVal = (SQLPOINTER)SQL_CONCUR_READ_ONLY;
        else if (0 == sOption.CompareNoCase(_TSA("SQL_CONCUR_VALUES")))
            attrVal = (SQLPOINTER)SQL_CONCUR_VALUES;
        else if (0 == sOption.CompareNoCase(_TSA("SQL_CONCUR_ROWVER")))
            attrVal = (SQLPOINTER)SQL_CONCUR_ROWVER;
        else if (0 == sOption.CompareNoCase(_TSA("SQL_CONCUR_LOCK")))
            attrVal = (SQLPOINTER)SQL_CONCUR_LOCK;
        else
            assert(false);
        /* dont check the result - can be driver specific */
        g_odbcAPI.SQLSetStmtAttr(m_handles.m_hstmt,
            SQL_ATTR_CONCURRENCY, attrVal, SQL_IS_INTEGER);
    }

    sOption = m_pCommand->Option(_TSA("SQL_ATTR_CURSOR_TYPE"));
    if (!sOption.IsEmpty())
    {
        SQLPOINTER attrVal = (SQLPOINTER)SQL_CURSOR_FORWARD_ONLY;
        if (0 == sOption.CompareNoCase(_TSA("SQL_CURSOR_FORWARD_ONLY")))
            attrVal = (SQLPOINTER)SQL_CURSOR_FORWARD_ONLY;
        else if (0 == sOption.CompareNoCase(_TSA("SQL_CURSOR_KEYSET_DRIVEN")))
            attrVal = (SQLPOINTER)SQL_CURSOR_KEYSET_DRIVEN;
        else if (0 == sOption.CompareNoCase(_TSA("SQL_CURSOR_DYNAMIC")))
            attrVal = (SQLPOINTER)SQL_CURSOR_DYNAMIC;
        else if (0 == sOption.CompareNoCase(_TSA("SQL_CURSOR_STATIC")))
            attrVal = (SQLPOINTER)SQL_CURSOR_STATIC;
        else
            assert(false);
        /* dont check the result - can be driver specific */
        g_odbcAPI.SQLSetStmtAttr(m_handles.m_hstmt,
            SQL_ATTR_CURSOR_TYPE, attrVal, SQL_IS_INTEGER);
    }

    sOption = m_pCommand->Option(_TSA("SQL_ATTR_CURSOR_SCROLLABLE"));
    if (!sOption.IsEmpty())
    {
        SQLPOINTER attrVal = (SQLPOINTER)SQL_NONSCROLLABLE;
        if (0 == sOption.CompareNoCase(_TSA("SQL_NONSCROLLABLE")))
            attrVal = (SQLPOINTER)SQL_NONSCROLLABLE;
        else if (0 == sOption.CompareNoCase(_TSA("SQL_SCROLLABLE")))
            attrVal = (SQLPOINTER)SQL_SCROLLABLE;
        else
            assert(false);
        /* dont check the result - can be driver specific */
        g_odbcAPI.SQLSetStmtAttr(m_handles.m_hstmt,
            SQL_ATTR_CURSOR_SCROLLABLE, attrVal, SQL_IS_INTEGER);
    }

    sOption = m_pCommand->Option(_TSA("SQL_ATTR_CURSOR_SENSITIVITY"));
    if (!sOption.IsEmpty())
    {
        SQLPOINTER attrVal = (SQLPOINTER)SQL_UNSPECIFIED;
        if (0 == sOption.CompareNoCase(_TSA("SQL_UNSPECIFIED")))
            attrVal = (SQLPOINTER)SQL_UNSPECIFIED;
        else if (0 == sOption.CompareNoCase(_TSA("SQL_INSENSITIVE")))
            attrVal = (SQLPOINTER)SQL_INSENSITIVE;
        else if (0 == sOption.CompareNoCase(_TSA("SQL_SENSITIVE")))
            attrVal = (SQLPOINTER)SQL_SENSITIVE;
        else
            assert(false);
        /* dont check the result - can be driver specific */
        g_odbcAPI.SQLSetStmtAttr(m_handles.m_hstmt,
            SQL_ATTR_CURSOR_SENSITIVITY, attrVal, SQL_IS_INTEGER);
    }

    sOption = m_pCommand->Option(_TSA("SQL_ATTR_QUERY_TIMEOUT"));
    if (!sOption.IsEmpty())
    {
        SQLPOINTER attrVal = (SQLPOINTER)sa_tol((const SAChar*)sOption);
        g_odbcAPI.SQLSetStmtAttr(m_handles.m_hstmt,
            SQL_ATTR_QUERY_TIMEOUT, attrVal, SQL_IS_UINTEGER);
    }

    sOption = m_pCommand->Option(_TSA("SetCursorName"));
    if (!sOption.IsEmpty())
        Check(g_odbcAPI.SQLSetCursorName(m_handles.m_hstmt,
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
        (SQLTCHAR *)sOption.GetUTF16Chars(),
#else
        (SQLTCHAR *)(const SAChar*)sOption,
#endif
        SQL_NTS), SQL_HANDLE_STMT, m_handles.m_hstmt);
}

/*virtual */
void IodbcCursor::Close()
{
    assert(m_handles.m_hstmt != NULL);
    Check(g_odbcAPI.SQLFreeHandle(SQL_HANDLE_STMT, m_handles.m_hstmt),
        SQL_HANDLE_STMT, m_handles.m_hstmt);
    m_handles.m_hstmt = NULL;
}

/*virtual */
void IodbcCursor::Destroy()
{
    assert(m_handles.m_hstmt != NULL);
    g_odbcAPI.SQLFreeHandle(SQL_HANDLE_STMT, m_handles.m_hstmt);
    m_handles.m_hstmt = NULL;
}

/*virtual */
void IodbcCursor::Reset()
{
    m_handles.m_hstmt = NULL;

    m_bResultSetCanBe = false;
    m_nRowsAffected = -1;
    m_nFirstLongFieldIndex = -1;
}

/*virtual */
ISACursor *IodbcConnection::NewCursor(SACommand *m_pCommand)
{
    return new IodbcCursor(this, m_pCommand);
}

/*virtual */
void IodbcCursor::Prepare(
    const SAString &sStmt,
    SACommandType_t eCmdType,
    int nPlaceHolderCount,
    saPlaceHolder**	ppPlaceHolders)
{
    SAString sStmtODBC;
    size_t nPos = 0l;
    int i;
    switch (eCmdType)
    {
    case SA_CmdSQLStmt:
        // replace bind variables with '?' place holder
        for (i = 0; i < nPlaceHolderCount; i++)
        {
            sStmtODBC += sStmt.Mid(nPos, ppPlaceHolders[i]->getStart() - nPos);
            sStmtODBC += _TSA("?");
            nPos = ppPlaceHolders[i]->getEnd() + 1;
        }
        // copy tail
        if (nPos < sStmt.GetLength())
            sStmtODBC += sStmt.Mid(nPos);
        break;
    case SA_CmdStoredProc:
        sStmtODBC = CallSubProgramSQL();
        break;
    case SA_CmdSQLStmtRaw:
        sStmtODBC = sStmt;
        break;
    default:
        assert(false);
    }

    // a bit of clean up
    Check(g_odbcAPI.SQLFreeStmt(m_handles.m_hstmt, SQL_CLOSE),
        SQL_HANDLE_STMT, m_handles.m_hstmt);
    Check(g_odbcAPI.SQLFreeStmt(m_handles.m_hstmt, SQL_UNBIND),
        SQL_HANDLE_STMT, m_handles.m_hstmt);
    Check(g_odbcAPI.SQLFreeStmt(m_handles.m_hstmt, SQL_RESET_PARAMS),
        SQL_HANDLE_STMT, m_handles.m_hstmt);

    SA_TRACE_CMD(sStmtODBC);
    Check(g_odbcAPI.SQLPrepare(m_handles.m_hstmt,
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
        (SQLTCHAR *)sStmtODBC.GetUTF16Chars(),
#else
        (SQLTCHAR*)(const SAChar*)sStmtODBC,
#endif
        SQL_NTS), SQL_HANDLE_STMT, m_handles.m_hstmt);
}

SAString IodbcCursor::CallSubProgramSQL()
{
    int nParams = m_pCommand->ParamCount();

    SAString sSQL = _TSA("{");

    // check for Return parameter
    int i;
    for (i = 0; i < nParams; ++i)
    {
        SAParam &Param = m_pCommand->ParamByIndex(i);
        if (Param.ParamDirType() == SA_ParamReturn)
        {
            sSQL += _TSA("?=");
            break;
        }
    }
    sSQL += _TSA("call ");
    sSQL += m_pCommand->CommandText();

    // specify parameters
    SAString sParams;
    for (i = 0; i < nParams; ++i)
    {
        SAParam &Param = m_pCommand->ParamByIndex(i);
        if (Param.ParamDirType() == SA_ParamReturn)
            continue;

        if (!sParams.IsEmpty())
            sParams += _TSA(", ");
        sParams += _TSA("?");
    }
    if (!sParams.IsEmpty())
    {
        sSQL += _TSA("(");
        sSQL += sParams;
        sSQL += _TSA(")");
    }

    sSQL += _TSA("}");
    return sSQL;
}

SQLRETURN IodbcCursor::BindLongs()
{
    SQLRETURN retcode;
    SQLPOINTER ValuePtr;

    try
    {
        while ((retcode = g_odbcAPI.SQLParamData(m_handles.m_hstmt, &ValuePtr))
            == SQL_NEED_DATA)
        {
            SAParam *pParam = (SAParam *)ValuePtr;
            size_t nActualWrite;
            SAPieceType_t ePieceType = SA_FirstPiece;
            void *pBuf;

            SADummyConverter DummyConverter;
            ISADataConverter *pIConverter = &DummyConverter;
            size_t nCnvtSize, nCnvtPieceSize = 0;
            SAPieceType_t eCnvtPieceType;
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
            bool bUnicode = pParam->ParamType() == SA_dtCLob || pParam->DataType() == SA_dtCLob ||
                pParam->ParamType() == SA_dtLongChar || pParam->DataType() == SA_dtLongChar;
            SAUnicodeUTF16Converter UnicodeUtf2Converter;
            if (bUnicode)
                pIConverter = &UnicodeUtf2Converter;
#endif
            do
            {
                nActualWrite = pParam->InvokeWriter(ePieceType, IodbcConnection::MaxLongAtExecSize, pBuf);
                pIConverter->PutStream((unsigned char*)pBuf, nActualWrite, ePieceType);
                // smart while: initialize nCnvtPieceSize before calling pIConverter->GetStream
                while (nCnvtPieceSize = nActualWrite,
                    pIConverter->GetStream((unsigned char*)pBuf, nCnvtPieceSize, nCnvtSize, eCnvtPieceType))
                    IodbcConnection::Check(g_odbcAPI.SQLPutData(m_handles.m_hstmt, pBuf, nCnvtSize),
                    SQL_HANDLE_STMT, m_handles.m_hstmt);

                if (ePieceType == SA_LastPiece)
                    break;
            } while (nActualWrite != 0);
        }
    }

    catch (SAException &)
    {
        g_odbcAPI.SQLCancel(m_handles.m_hstmt);
        throw;
    }

    if (retcode != SQL_NO_DATA)
        Check(retcode, SQL_HANDLE_STMT, m_handles.m_hstmt);

    return retcode;
}

/*virtual */
void IodbcCursor::UnExecute()
{
    m_bResultSetCanBe = false;
    m_nFirstLongFieldIndex = -1;
}

void IodbcCursor::Bind(
    int nPlaceHolderCount,
    saPlaceHolder **ppPlaceHolders)
{
    // we should bind for every place holder ('?')
    AllocBindBuffer(nPlaceHolderCount, ppPlaceHolders,
        sizeof(SQLLEN), 0);
    void *pBuf = m_pParamBuffer;

    for (int i = 0; i < nPlaceHolderCount; ++i)
    {
        SAParam &Param = *ppPlaceHolders[i]->getParam();

        void *pInd;
        void *pSize;
        size_t nDataBufSize;
        void *pValue;
        IncParamBuffer(pBuf, pInd, pSize, nDataBufSize, pValue);

        SADataType_t eDataType = Param.DataType();
        SADataType_t eParamType = Param.ParamType();
        if (eParamType == SA_dtUnknown)
            eParamType = eDataType;	// assume
        // SA_dtUnknown can be used to bind NULL
        // but some type should be set in that case as well
        SQLSMALLINT ParameterType = IodbcConnection::CnvtStdToNative(
            eParamType == SA_dtUnknown ? SA_dtString : eParamType);
        SQLSMALLINT ValueType = IodbcConnection::CnvtStdToNativeValueType(
            eDataType == SA_dtUnknown ? SA_dtString : eDataType);

        SQLLEN *StrLen_or_IndPtr = (SQLLEN*)pInd;
        SQLPOINTER ParameterValuePtr = (SQLPOINTER)pValue;
        SQLLEN BufferLength = (SQLLEN)nDataBufSize;
        SQLULEN ColumnSize = (SQLULEN)Param.ParamSize();
        if (!ColumnSize)	{ // not set explicitly
            ColumnSize = BufferLength;
            if (eDataType == SA_dtString)
                ColumnSize /= sizeof(TSAChar);
        }

        SQLSMALLINT InputOutputType;
        switch (Param.ParamDirType())
        {
        case SA_ParamInput:
            InputOutputType = SQL_PARAM_INPUT;
            break;
        case SA_ParamOutput:
            InputOutputType = SQL_PARAM_OUTPUT;
            break;
        case SA_ParamInputOutput:
            InputOutputType = SQL_PARAM_INPUT_OUTPUT;
            break;
        default:
            assert(Param.ParamDirType() == SA_ParamReturn);
            InputOutputType = SQL_PARAM_OUTPUT;
        }

        // this code is not required for a good
        // ODBC driver but it is added for safety reasons
        // because some "bug-full" drivers (e.g. Oracle) check this
        // indicator on input even for pure outputs
        *StrLen_or_IndPtr = SQL_NULL_DATA;			// field is null

        if (isInputParam(Param))
        {
            if (Param.isNull())
                *StrLen_or_IndPtr = SQL_NULL_DATA;			// field is null
            else
                *StrLen_or_IndPtr = InputBufferSize(Param);	// field is not null

            if (!Param.isNull())
            {
                switch (eDataType)
                {
                case SA_dtUnknown:
                    throw SAException(SA_Library_Error, SA_Library_Error_UnknownParameterType, -1, IDS_UNKNOWN_PARAMETER_TYPE, (const SAChar*)Param.Name());
                case SA_dtBool:
                    assert(*StrLen_or_IndPtr == (SQLLEN)sizeof(unsigned char));
                    *(unsigned char*)ParameterValuePtr = (unsigned char)Param.asBool();
                    break;
                case SA_dtShort:
                    assert(*StrLen_or_IndPtr == (SQLLEN)sizeof(short));
                    *(short*)ParameterValuePtr = Param.asShort();
                    break;
                case SA_dtUShort:
                    assert(*StrLen_or_IndPtr == (SQLLEN)sizeof(unsigned short));
                    *(unsigned short*)ParameterValuePtr = Param.asUShort();
                    break;
                case SA_dtLong:
                    assert(*StrLen_or_IndPtr == (SQLLEN)sizeof(long));
                    *(long*)ParameterValuePtr = Param.asLong();
                    break;
                case SA_dtULong:
                    assert(*StrLen_or_IndPtr == (SQLLEN)sizeof(unsigned long));
                    *(unsigned long*)ParameterValuePtr = Param.asLong();
                    break;
                case SA_dtDouble:
                    assert(*StrLen_or_IndPtr == (SQLLEN)sizeof(double));
                    *(double*)ParameterValuePtr = Param.asDouble();
                    break;
                case SA_dtNumeric:
                    if (((IodbcConnection*)m_pISAConnection)->m_bUseNumeric)
                    {
                        assert(*StrLen_or_IndPtr == (SQLLEN)sizeof(SQL_NUMERIC_STRUCT));
                        IodbcConnection::CnvtNumericToInternal(
                            Param.asNumeric(),
                            *(SQL_NUMERIC_STRUCT*)ParameterValuePtr);
                    }
                    else
                    {
                        assert(*StrLen_or_IndPtr == (SQLLEN)sizeof(SA_ODBC_SQL_NUMERIC_STRUCT));
                        IodbcConnection::CnvtNumericToInternal(
                            Param.asNumeric(),
                            *((SA_ODBC_SQL_NUMERIC_STRUCT*)ParameterValuePtr),
                            *StrLen_or_IndPtr);
                    }
                    break;
                case SA_dtDateTime:
                    assert(*StrLen_or_IndPtr == (SQLLEN)sizeof(TIMESTAMP_STRUCT));
                    IodbcConnection::CnvtDateTimeToInternal(
                        Param.asDateTime(),
                        *(TIMESTAMP_STRUCT*)ParameterValuePtr);
                    break;
                case SA_dtString:
                    assert(*StrLen_or_IndPtr == (SQLLEN)(Param.asString().GetLength() * sizeof(TSAChar)));
                    memcpy(ParameterValuePtr,
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
                        Param.asString().GetUTF16Chars(),
#else
                        (const SAChar*)Param.asString(),
#endif
                        *StrLen_or_IndPtr);
                    break;
                case SA_dtBytes:
                    assert(*StrLen_or_IndPtr == (SQLLEN)Param.asBytes().GetBinaryLength());
                    memcpy(ParameterValuePtr, (const void*)Param.asBytes(), *StrLen_or_IndPtr);
                    break;
                case SA_dtLongBinary:
                case SA_dtBLob:
                case SA_dtLongChar:
                case SA_dtCLob:
                    assert(*StrLen_or_IndPtr == 0);
                    if (((IodbcConnection*)m_pISAConnection)->NeedLongDataLen())
                    {
                        if (Param.m_fnWriter)	// callback will be used, so we don't know total size now
                        {
                            // Driver has reported that it needs to know
                            // total data length in advance, but we don't know it
                            // because user-provided callback will supply data.
                            // So we have two options as our last chance (both are dummy):
                            // 1) SQL_LEN_DATA_AT_EXEC(IodbcConnection::MaxLongAtExecSize);
                            // 2) SQL_DATA_AT_EXEC
                            // The first one doesn't work with SQL Server driver, so we use the second one.
                            // No problems with the second one are currently knowm.
                            //*StrLen_or_IndPtr = SQL_DATA_AT_EXEC;
                            // 2010-03-09: Is SQL_DATA_AT_EXEC obsolete (ODBC 1.0)? - replaced by
                            *StrLen_or_IndPtr = SQL_LEN_DATA_AT_EXEC(0);
                        }
                        else	// data from the internal buffer will be used
                            *StrLen_or_IndPtr = SQL_LEN_DATA_AT_EXEC((int)Param.m_pString->GetBinaryLength());
                    }
                    else
                        //*StrLen_or_IndPtr = SQL_DATA_AT_EXEC;
                        *StrLen_or_IndPtr = SQL_LEN_DATA_AT_EXEC(0);
                    break;
                default:
                    assert(false);
                }
            }

            if (Param.isDefault())
                *StrLen_or_IndPtr = SQL_DEFAULT_PARAM;
        }

        if (isLongOrLob(eDataType))
        {
            Check(g_odbcAPI.SQLBindParameter(
                m_handles.m_hstmt,
                (SQLUSMALLINT)(i + 1),
                InputOutputType,
                ValueType,
                ParameterType,
                IodbcConnection::MaxLongAtExecSize /
                ((eDataType == SA_dtLongChar || eDataType == SA_dtCLob) ? sizeof(SAChar) : 1), // ColumnSize
                0, // DecimalDigits
                &Param,	// ParameterValuePtr - context
                0, // Buffer length
                StrLen_or_IndPtr), SQL_HANDLE_STMT, m_handles.m_hstmt);
        }
        else if (eDataType == SA_dtNumeric)
        {
            SQLSMALLINT Precision;
            SQLSMALLINT Scale;

            switch (InputOutputType)
            {
            case SQL_PARAM_INPUT:
                // use precision and scale of input data
                Precision = (SQLSMALLINT)Param.asNumeric().precision;
                Scale = (SQLSMALLINT)Param.asNumeric().scale;
                break;
            case SQL_PARAM_OUTPUT:
                // set precision and scale for return value
                Precision = (SQLSMALLINT)Param.ParamPrecision();
                Scale = (SQLSMALLINT)Param.ParamScale();
                break;
            case SQL_PARAM_INPUT_OUTPUT:
                if (Param.isNull())
                {
                    // Param is INOUT but we do not really bind anything
                    // It allows us to optimize precision and scale for return value
                    Precision = (SQLSMALLINT)Param.ParamPrecision();
                    Scale = (SQLSMALLINT)Param.ParamScale();
                }
                else
                {
                    // Param is INOUT but we have to
                    // use the scale of input data
                    // Precision is set to fit bigger number
                    Precision = (SQLSMALLINT)sa_max(Param.asNumeric().precision, Param.ParamPrecision());
                    Scale = (SQLSMALLINT)Param.asNumeric().scale;
                }
                break;
            default:
                assert(false);
                Precision = 0;
                Scale = 0;
            }

            if (!((IodbcConnection*)m_pISAConnection)->m_bUseNumeric)
                ValueType = SQL_C_TCHAR;

            Check(g_odbcAPI.SQLBindParameter(
                m_handles.m_hstmt,
                (SQLUSMALLINT)(i + 1), InputOutputType,
                ValueType,
                ParameterType,
                Precision,	// ColumnSize
                Scale,		// DecimalDigits
                ParameterValuePtr,
                BufferLength,
                StrLen_or_IndPtr), SQL_HANDLE_STMT, m_handles.m_hstmt);

            // Some ODBC drivers (for example MySQL 3.51) don't support this function
            // We don't use it if SQL_C_TCHAR buffer is used for data
            if (((IodbcConnection*)m_pISAConnection)->m_bUseNumeric)
            {
                // for numeric data we have to adjust precision and scale
                // because:
                // precision deaults to some driver specific value
                // scale defaults to 0

                // Get the application row descriptor
                // for the statement handle using SQLGetStmtAttr
                SQLHDESC hdesc;
                Check(g_odbcAPI.SQLGetStmtAttr(
                    m_handles.m_hstmt,
                    SQL_ATTR_APP_PARAM_DESC,
                    &hdesc,
                    0, NULL), SQL_HANDLE_STMT, m_handles.m_hstmt);

                Check(g_odbcAPI.SQLSetDescRec(
                    hdesc,
                    (SQLSMALLINT)(i + 1),
                    ValueType, -1, nDataBufSize,
                    Precision,
                    Scale,
                    pValue,
                    (SQLLEN*)pInd,
                    (SQLLEN*)pInd), SQL_HANDLE_DESC, hdesc);
            }
        }
        else
        {
            // Some ODBC drivers can analyze DecimalDigits
            // even if data being bound is not SA_dtNumeric
            // So we shouldn't pass garabage value
            SQLSMALLINT DecimalDigits = (SQLSMALLINT)Param.ParamScale();
            if (DecimalDigits < 0)	// not set explicitly
            {
                switch (eDataType)
                {
                case SA_dtUnknown:
                    DecimalDigits = 0;
                    break;
                case SA_dtBool:
                case SA_dtShort:
                case SA_dtUShort:
                case SA_dtLong:
                case SA_dtULong:
                    DecimalDigits = 0;
                    break;
                case SA_dtDouble:
                    DecimalDigits = 15;	// MAX precision of double
                    break;
                case SA_dtDateTime:
                    ColumnSize = 23;	// Column size for datetime with fraction
                    DecimalDigits = 3;	// It's necessary for fraction
                    break;
                case SA_dtString:
                    DecimalDigits = 0;
                    break;
                case SA_dtBytes:
                case SA_dtCursor:
                case SA_dtSpecificToDBMS:
                    DecimalDigits = 0;
                    break;
                default:
                    assert(false);
                    DecimalDigits = 0;
                }
            }

            // to avoid default (f.ex. SQLServer ODBC driver throws an error when binding "" string)
            if (ColumnSize == 0 && isInputParam(Param))
                ColumnSize = sa_max(SQLUINTEGER(1), (SQLUINTEGER)InputBufferSize(Param));
            Check(g_odbcAPI.SQLBindParameter(
                m_handles.m_hstmt,
                (SQLUSMALLINT)(i + 1),
                InputOutputType,
                ValueType,
                ParameterType,
                ColumnSize,
                DecimalDigits,
                ParameterValuePtr,
                BufferLength,
                StrLen_or_IndPtr), SQL_HANDLE_STMT, m_handles.m_hstmt);
        }
    }
}

void IodbcCursor::ProcessBatchUntilEndOrResultSet()
{
    SQLRETURN rcMoreResults;
    do
    {
        rcMoreResults = g_odbcAPI.SQLMoreResults(
            m_handles.m_hstmt);

        if (rcMoreResults != SQL_NO_DATA)
        {
            Check(rcMoreResults, SQL_HANDLE_STMT, m_handles.m_hstmt);

            // cache affected rows count
            // because it will not be available after SQL_NO_DATA
            Check(g_odbcAPI.SQLRowCount(m_handles.m_hstmt, &m_nRowsAffected),
                SQL_HANDLE_STMT, m_handles.m_hstmt);

            if (ResultSetExists())
                break;
        }
        else
        {
            m_bResultSetCanBe = false;
            // some drivers (e.g. Microsoft SQL Server)
            // return output parameters only after SQLMoreResults
            // returns SQL_NO_DATA
            ConvertOutputParams();	// if any/if available
        }
    } while (rcMoreResults != SQL_NO_DATA);
}

/*virtual */
void IodbcCursor::Execute(
    int nPlaceHolderCount,
    saPlaceHolder **ppPlaceHolders)
{
    if (nPlaceHolderCount)
        Bind(nPlaceHolderCount, ppPlaceHolders);

    SQLRETURN rc;
    g_odbcAPI.SQLFreeStmt(m_handles.m_hstmt, SQL_CLOSE);

    rc = g_odbcAPI.SQLExecute(m_handles.m_hstmt);

    if (rc != SQL_NEED_DATA)
    {
        if (rc != SQL_NO_DATA)
            // SQL_NO_DATA is OK for DELETEs or UPDATEs that
            // haven't affected any rows
            Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);
    }
    else
        rc = BindLongs();

    m_bResultSetCanBe = true;

    // cache affected rows count
    if (rc != SQL_NO_DATA)
        Check(g_odbcAPI.SQLRowCount(m_handles.m_hstmt, &m_nRowsAffected),
        SQL_HANDLE_STMT, m_handles.m_hstmt);
    else
    {
        // do not call SQLRowCount
        // to prevent "Function sequence error" while using iODBC
        m_nRowsAffected = 0;
    }

    // some drivers (e.g. Microsoft SQL Server)
    // return output parameters only after SQLMoreResults returns SQL_NO_DATA
    if (!ResultSetExists())
        ProcessBatchUntilEndOrResultSet();

    ConvertOutputParams();	// if any/if available
}

/*virtual */
void IodbcCursor::Cancel()
{
    IodbcConnection::Check(g_odbcAPI.SQLCancel(
        m_handles.m_hstmt), SQL_HANDLE_STMT, m_handles.m_hstmt);
}

/*virtual */
bool IodbcCursor::ResultSetExists()
{
    if (!m_bResultSetCanBe)
        return false;

    SQLSMALLINT ColumnCount;
    Check(g_odbcAPI.SQLNumResultCols(m_handles.m_hstmt, &ColumnCount),
        SQL_HANDLE_STMT, m_handles.m_hstmt);
    return ColumnCount > 0;
}

/*virtual */
void IodbcCursor::DescribeFields(
    DescribeFields_cb_t fn)
{
    SQLSMALLINT ColumnCount;
    Check(g_odbcAPI.SQLNumResultCols(m_handles.m_hstmt, &ColumnCount),
        SQL_HANDLE_STMT, m_handles.m_hstmt);

    SAString sOption = m_pCommand->Option("ODBC_Internal_LimitColumns");
    if (!sOption.IsEmpty())
    {
        SQLSMALLINT ColumnLimit = (SQLSMALLINT)sa_toi(sOption);
        if (ColumnLimit < ColumnCount)
            ColumnCount = ColumnLimit;
    }

    for (SQLSMALLINT nField = 1; nField <= ColumnCount; ++nField)
    {
        SQLTCHAR szColName[1024];
        SQLSMALLINT nColLen;
        SQLSMALLINT DataType;
        SQLULEN ColumnSize = 0l;
        SQLSMALLINT Nullable;
        SQLSMALLINT DecimalDigits;

        Check(g_odbcAPI.SQLDescribeCol(
            m_handles.m_hstmt,
            nField,
            szColName,
            1024,
            &nColLen,
            &DataType,
            &ColumnSize,
            &DecimalDigits,
            &Nullable), SQL_HANDLE_STMT, m_handles.m_hstmt);

        SAString sColName;
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
        sColName.SetUTF16Chars(szColName, nColLen);
#else
        sColName = SAString((const SAChar*)szColName, nColLen);
#endif

        (m_pCommand->*fn)(sColName,
            IodbcConnection::CnvtNativeToStd(DataType, ColumnSize, ColumnSize, DecimalDigits),
            (int)DataType,
            (long)ColumnSize,
            (long)ColumnSize,
            DecimalDigits,
            Nullable == SQL_NO_NULLS,
            (int)ColumnCount);
    }
}

/*virtual */
long IodbcCursor::GetRowsAffected()
{
    return long(m_nRowsAffected);
}

/*virtual */
size_t IodbcCursor::OutputBufferSize(
    SADataType_t eDataType,
    size_t nDataSize) const
{
    switch (eDataType)
    {
    case SA_dtLong:
    case SA_dtULong:
        return sizeof(int);
    case SA_dtBool:
        return sizeof(unsigned char);	// SQL_C_BIT
    case SA_dtNumeric:
        return ((IodbcConnection*)m_pISAConnection)->m_bUseNumeric ?
            sizeof(SQL_NUMERIC_STRUCT) : // SQL_C_NUMERIC
            sizeof(SA_ODBC_SQL_NUMERIC_STRUCT); // SQL_C_TCHAR
    case SA_dtString:
        return (sizeof(TSAChar) * (nDataSize + 1));	// always allocate space for NULL
    case SA_dtDateTime:
        return sizeof(TIMESTAMP_STRUCT);
    case SA_dtLongBinary:
    case SA_dtLongChar:
        return 0;
    case SA_dtUnknown: // we have to ignore unknown data types
        return 0;
    default:
        break;
    }

    return ISACursor::OutputBufferSize(eDataType, nDataSize);
}

/*virtual */
void IodbcCursor::SetFieldBuffer(
    int nCol,	// 1-based
    void *pInd,
    size_t nIndSize,
    void * /*pSize*/,
    size_t/* nSizeSize*/,
    void *pValue,
    size_t nBufSize)
{
    if (getOptionValue(SA_OPT_ODBC_USE_SQLGETDATA, false))
        m_nFirstLongFieldIndex = 0;

    assert(nIndSize == sizeof(SQLLEN));
    if (nIndSize != sizeof(SQLLEN))
        return;

    SAField &Field = m_pCommand->Field(nCol);

    if (Field.FieldType() == SA_dtUnknown) // ignore unknown field type
        return;

    SQLSMALLINT TargetType = IodbcConnection::CnvtStdToNativeValueType(Field.FieldType());
    bool bLong = false;
    switch (Field.FieldType())
    {
    case SA_dtUnknown:
        throw SAException(SA_Library_Error, SA_Library_Error_UnknownColumnType, -1, IDS_UNKNOWN_COLUMN_TYPE, (const SAChar*)Field.Name());
    case SA_dtBool:
        assert(nBufSize == sizeof(unsigned char));
        break;
    case SA_dtShort:
    case SA_dtUShort:
        assert(nBufSize == sizeof(short));
        break;
    case SA_dtLong:
    case SA_dtULong:
        assert(nBufSize == sizeof(int));
        break;
    case SA_dtDouble:
        assert(nBufSize == sizeof(double));
        break;
    case SA_dtNumeric:
        if (((IodbcConnection*)m_pISAConnection)->m_bUseNumeric)
        {
            assert(nBufSize == sizeof(SQL_NUMERIC_STRUCT));
        }
        else
        {
            assert(nBufSize == sizeof(SA_ODBC_SQL_NUMERIC_STRUCT));
            TargetType = SQL_C_TCHAR;
        }
        break;
    case SA_dtDateTime:
        assert(nBufSize == sizeof(TIMESTAMP_STRUCT));
        break;
    case SA_dtBytes:
        assert(nBufSize == (unsigned int)Field.FieldSize());
        break;
    case SA_dtString:
        assert(nBufSize == (unsigned int)(sizeof(TSAChar) * (Field.FieldSize() + 1)));
        break;
    case SA_dtLongBinary:
    case SA_dtLongChar:
        assert(nBufSize == 0);
        bLong = true;
        break;
    default:
        assert(false);	// unknown type
    }

    if (!bLong)
    {
        // Don't bind the column if there is already long/LOB field in the result set
        bool bUseBindCol = m_nFirstLongFieldIndex < 0 || m_nFirstLongFieldIndex > nCol;
        if (bUseBindCol)
            Check(g_odbcAPI.SQLBindCol(
            m_handles.m_hstmt,
            (SQLUSMALLINT)nCol,
            TargetType,
            pValue,
            nBufSize,
            (SQLLEN*)pInd), SQL_HANDLE_STMT, m_handles.m_hstmt);

        // for numeric data we have to adjust precision and scale
        // because:
        // precision defaults to some driver specific value
        // scale defaults to 0

        // Some ODBC drivers (for example MySQL 3.51) don't support this function
        // We don't use it if SQL_C_TCHAR buffer is used for data
        if (Field.FieldType() == SA_dtNumeric &&
            ((IodbcConnection*)m_pISAConnection)->m_bUseNumeric)
        {
            // Get the application row descriptor
            // for the statement handle using SQLGetStmtAttr
            SQLHDESC hdesc;
            Check(g_odbcAPI.SQLGetStmtAttr(
                m_handles.m_hstmt,
                SQL_ATTR_APP_ROW_DESC,
                &hdesc,
                0, NULL), SQL_HANDLE_STMT, m_handles.m_hstmt);

            if (bUseBindCol)
                Check(g_odbcAPI.SQLSetDescRec(
                hdesc,
                (SQLSMALLINT)nCol,
                TargetType,
                -1,
                nBufSize,
                (SQLSMALLINT)Field.FieldPrecision(),
                (SQLSMALLINT)Field.FieldScale(),
                pValue,
                (SQLLEN*)pInd,
                (SQLLEN*)pInd), SQL_HANDLE_DESC, hdesc);
            else
            {
                Check(g_odbcAPI.SQLSetDescField(hdesc, (SQLSMALLINT)nCol, SQL_DESC_TYPE,
                    (SQLPOINTER)SQL_C_NUMERIC, 0), SQL_HANDLE_DESC, hdesc);
                Check(g_odbcAPI.SQLSetDescField(hdesc, (SQLSMALLINT)nCol, SQL_DESC_PRECISION,
                    (SQLPOINTER)Field.FieldPrecision(), 0), SQL_HANDLE_DESC, hdesc);
                Check(g_odbcAPI.SQLSetDescField(hdesc, (SQLSMALLINT)nCol, SQL_DESC_SCALE,
                    (SQLPOINTER)Field.FieldScale(), 0), SQL_HANDLE_DESC, hdesc);
            }
        }
    }
    else if (m_nFirstLongFieldIndex < 0)
        m_nFirstLongFieldIndex = nCol;
}

/*virtual */
void IodbcCursor::SetSelectBuffers()
{
    SAString sOption = m_pCommand->Option(SACMD_PREFETCH_ROWS);
    if (!sOption.IsEmpty())
    {
        int cLongs = FieldCount(4, SA_dtLongBinary, SA_dtLongChar, SA_dtBLob, SA_dtCLob);
        if (cLongs)	// do not use bulk fetch if there are text or image columns
            m_cRowsToPrefetch = 1;
        else
        {
            m_cRowsToPrefetch = sa_toi(sOption);
            if (!m_cRowsToPrefetch)
                m_cRowsToPrefetch = 1;
        }
    }
    else
        m_cRowsToPrefetch = 1;

    m_cRowsObtained = 0;
    m_cRowCurrent = 0;

    // set the SQL_ATTR_ROW_BIND_TYPE statement attribute to use
    // column-wise binding.
    // don't check for error - if driver does not support setting
    // this option - it should be the default one
    g_odbcAPI.SQLSetStmtAttr(m_handles.m_hstmt,
        SQL_ATTR_ROW_BIND_TYPE, SQL_BIND_BY_COLUMN, 0);
    // Declare the rowset size with the SQL_ATTR_ROW_ARRAY_SIZE
    // statement attribute.
    Check(g_odbcAPI.SQLSetStmtAttr(m_handles.m_hstmt,
        SQL_ATTR_ROW_ARRAY_SIZE, SQLPOINTER(m_cRowsToPrefetch), 0),
        SQL_HANDLE_STMT, m_handles.m_hstmt);
    // Set the SQL_ATTR_ROWS_FETCHED_PTR statement attribute
    // to point to m_cRowsObtained
    Check(g_odbcAPI.SQLSetStmtAttr(m_handles.m_hstmt,
        SQL_ATTR_ROWS_FETCHED_PTR, &m_cRowsObtained, 0),
        SQL_HANDLE_STMT, m_handles.m_hstmt);

    // use default helpers
    AllocSelectBuffer(sizeof(SQLLEN), 0, m_cRowsToPrefetch);
}

/*virtual */
bool IodbcCursor::ConvertIndicator(
    int nPos,	// 1-based
    int/* nNotConverted*/,
    SAValueRead &vr,
    ValueType_t eValueType,
    void *pInd, size_t nIndSize,
    void * /*pSize*/, size_t/* nSizeSize*/,
    size_t &nRealSize,
    int nBulkReadingBufPos) const
{
    assert(nIndSize == sizeof(SQLLEN));
    if (nIndSize != sizeof(SQLLEN))
        return false;	// not converted

    bool bLong = false;
    SQLSMALLINT TargetType = 0;
    bool bAddSpaceForNull = false;

    SADataType_t eDataType;
    switch (eValueType)
    {
    case ISA_FieldValue:
        eDataType = ((SAField&)vr).FieldType();
        break;
    default:
        assert(eValueType == ISA_ParamValue);
        eDataType = ((SAParam&)vr).ParamType();
    }
    switch (eDataType)
    {
    case SA_dtLongBinary:
    case SA_dtBLob:
        bLong = true;
        TargetType = SQL_C_BINARY;
        bAddSpaceForNull = false;
        break;
    case SA_dtLongChar:
    case SA_dtCLob:
        bLong = true;
        TargetType = SQL_C_CHAR;
        bAddSpaceForNull = true;
        break;
    default:
        break;
    }

    if (bLong)
    {
        TSAChar Buf[1];
        SQLLEN StrLen_or_IndPtr = 0l;

        SQLRETURN rc = g_odbcAPI.SQLGetData(
            m_handles.m_hstmt, (SQLUSMALLINT)nPos,
            TargetType, Buf,
            bAddSpaceForNull && getOptionValue(SA_OPT_ODBC_ADD_LONGTEXTBUFSPACE, true) ?
            sizeof(TSAChar) : 0, &StrLen_or_IndPtr);

        if (SQL_NO_DATA != rc)
            Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);

        *vr.m_pbNull = ((SQLINTEGER)StrLen_or_IndPtr == SQL_NULL_DATA);

        if (!vr.isNull())
        {
            // also try to get long size
            if (rc == SQL_NO_DATA)
                nRealSize = 0l;
            else if (StrLen_or_IndPtr >= 0)
                nRealSize = (size_t)StrLen_or_IndPtr;
            else
                nRealSize = 0l;	// unknown
        }
        return true;	// converted
    }

    // set unknown data size to null
    *vr.m_pbNull = eDataType == SA_dtUnknown || ((SQLLEN*)pInd)[nBulkReadingBufPos] == SQL_NULL_DATA;
    if (!vr.isNull())
        nRealSize = (size_t)((SQLLEN*)pInd)[nBulkReadingBufPos];
    return true;	// converted
}

/*virtual */
bool IodbcCursor::IgnoreUnknownDataType()
{
    return true;
}

/*virtual */
bool IodbcCursor::ConvertValue(
    int nPos,	// 1 - based
    int nNotConverted,
    size_t nIndSize,
    void *pNull,
    size_t nSizeSize,
    void *pSize,
    size_t nBufSize,
    void *pValue,
    ValueType_t eValueType,
    SAValueRead &vr,
    int nBulkReadingBufPos)
{
    SADataType_t eDataType;
    switch (eValueType)
    {
    case ISA_FieldValue:
        eDataType = ((SAField&)vr).FieldType();
        break;
    default:
        assert(eValueType == ISA_ParamValue);
        eDataType = ((SAParam&)vr).ParamType();
    }

    if (m_nFirstLongFieldIndex >= 0 && m_nFirstLongFieldIndex < nPos && !isLongOrLob(eDataType))
    {
        void *pData = (char*)pValue + nBulkReadingBufPos * nBufSize;
        SQLSMALLINT TargetType = eDataType == SA_dtNumeric ?
            SQLSMALLINT((((IodbcConnection*)m_pISAConnection)->m_bUseNumeric) ? SQL_ARD_TYPE : SQL_C_TCHAR)
            : IodbcConnection::CnvtStdToNativeValueType(eDataType);
        SQLLEN *pStrLen_or_IndPtr = (SQLLEN *)pNull;

        SQLRETURN rc = g_odbcAPI.SQLGetData(
            m_handles.m_hstmt, (SQLUSMALLINT)nPos,
            TargetType, pData, nBufSize,
            pStrLen_or_IndPtr);

        if (SQL_NO_DATA != rc)
            Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);

        *vr.m_pbNull = *pStrLen_or_IndPtr == SQL_NULL_DATA;
    }

    return ISACursor::ConvertValue(nPos, nNotConverted, nIndSize, pNull, nSizeSize,
        pSize, nBufSize, pValue, eValueType, vr, nBulkReadingBufPos);
}

// default implementation (ISACursor::ConvertString) assumes that data for
// string is in multibyte format always
// (in both Unicode and multi-byte builds)
// it also assumes that nRealSize is always in bytes not characters
// We overload this because our buffer is multi-byte
// under multi-byte build, but Unicode under Unicode build
/*virtual */
void IodbcCursor::ConvertString(SAString &String, const void *pData, size_t nRealSize)
{
    // nRealSize is in bytes but we need in characters,
    // so nRealSize should be a multiply of character size
    assert(nRealSize % sizeof(TSAChar) == 0);
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
    String.SetUTF16Chars(pData, nRealSize / sizeof(TSAChar));
#else
    String = SAString((const SAChar*)pData, nRealSize / sizeof(SAChar));
#endif
}

/*virtual */
bool IodbcCursor::FetchNext()
{
    if (!m_cRowsObtained || m_cRowCurrent >= (m_cRowsObtained - 1))
    {
        // SQLFetchScroll is not working with Sybase driver when
        // reading image columns
        //SQLRETURN rc = g_odbcAPI.SQLFetchScroll(
        //		m_handles.m_hstmt, SQL_FETCH_NEXT, 0);
        SQLRETURN rc = g_odbcAPI.SQLFetch(
            m_handles.m_hstmt);
        if (rc != SQL_NO_DATA)
        {
            Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);
            if (m_cRowsToPrefetch == 1)
                // in case updating SQL_ATTR_ROWS_FETCHED_PTR is not supported
                m_cRowsObtained = 1;
        }
        else
            m_cRowsObtained = 0;

        m_cRowCurrent = 0;
    }
    else
        ++m_cRowCurrent;

    if (m_cRowsObtained)
    {
        // use default helpers
        ConvertSelectBufferToFields(m_cRowCurrent);
    }
    else if (!isSetScrollable())
        ProcessBatchUntilEndOrResultSet();

    return m_cRowsObtained != 0;
}

/*virtual */
bool IodbcCursor::FetchPrior()
{
    if (!m_cRowsObtained || m_cRowCurrent <= 0)
    {
        SQLRETURN rc = g_odbcAPI.SQLFetchScroll(m_handles.m_hstmt, SQL_FETCH_PRIOR, 0);

        if (rc != SQL_NO_DATA)
            Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);
        else
            m_cRowsObtained = 0;

        m_cRowCurrent = m_cRowsObtained - 1;
    }
    else
        --m_cRowCurrent;

    if (m_cRowsObtained)
    {
        // use default helpers
        ConvertSelectBufferToFields(m_cRowCurrent);
    }

    return m_cRowsObtained != 0;
}

/*virtual */
bool IodbcCursor::FetchFirst()
{
    SQLRETURN rc = g_odbcAPI.SQLFetchScroll(m_handles.m_hstmt, SQL_FETCH_FIRST, 0);

    if (rc != SQL_NO_DATA)
        Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);
    else
        m_cRowsObtained = 0;

    m_cRowCurrent = 0;

    if (m_cRowsObtained)
    {
        // use default helpers
        ConvertSelectBufferToFields(m_cRowCurrent);
    }

    return m_cRowsObtained != 0;
}

/*virtual */
bool IodbcCursor::FetchLast()
{
    SQLRETURN rc = g_odbcAPI.SQLFetchScroll(m_handles.m_hstmt, SQL_FETCH_LAST, 0);

    if (rc != SQL_NO_DATA)
        Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);
    else
        m_cRowsObtained = 0;

    m_cRowCurrent = m_cRowsObtained - 1;

    if (m_cRowsObtained)
    {
        // use default helpers
        ConvertSelectBufferToFields(m_cRowCurrent);
    }

    return m_cRowsObtained != 0;
}

/*virtual */
bool IodbcCursor::FetchPos(int offset, bool Relative/* = false*/)
{
    SQLRETURN rc = g_odbcAPI.SQLFetchScroll(m_handles.m_hstmt,
        Relative ? SQL_FETCH_RELATIVE : SQL_FETCH_ABSOLUTE, (SQLLEN)offset);

    if (rc != SQL_NO_DATA)
        Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);
    else
        m_cRowsObtained = 0;

    m_cRowCurrent = m_cRowsObtained - 1;

    if (m_cRowsObtained)
    {
        // use default helpers
        ConvertSelectBufferToFields(m_cRowCurrent);
    }

    return m_cRowsObtained != 0;
}

/*virtual */
void IodbcCursor::ReadLongOrLOB(
    ValueType_t eValueType,
    SAValueRead &vr,
    void * /*pValue*/,
    size_t/* nBufSize*/,
    saLongOrLobReader_t fnReader,
    size_t nReaderWantedPieceSize,
    void *pAddlData)
{
    if (eValueType == ISA_FieldValue)
    {
        SAField &Field = (SAField &)vr;

        SQLSMALLINT TargetType;
        SQLLEN StrLen_or_IndPtr = 0l;
        SQLRETURN rc;
        bool bAddSpaceForNull;

        switch (Field.FieldType())
        {
        case SA_dtLongBinary:
            TargetType = SQL_C_BINARY;
            bAddSpaceForNull = false;
            break;
        case SA_dtLongChar:
            TargetType = SQL_C_TCHAR;
            bAddSpaceForNull = true;
            break;
        default:
            TargetType = 0;
            bAddSpaceForNull = false;
            assert(false);
        }

        SADummyConverter DummyConverter;
        ISADataConverter *pIConverter = &DummyConverter;
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
        SAUTF16UnicodeConverter Utf16UnicodeConverter;
        if (bAddSpaceForNull)
            pIConverter = &Utf16UnicodeConverter;
#endif

        // try to get long size
        size_t nLongSize = 0l;
        TSAChar Buf[1];
        rc = g_odbcAPI.SQLGetData(
            m_handles.m_hstmt, (SQLUSMALLINT)Field.Pos(),
            TargetType, Buf,
            bAddSpaceForNull && getOptionValue(SA_OPT_ODBC_ADD_LONGTEXTBUFSPACE, true) ? sizeof(TSAChar) :
            0, &StrLen_or_IndPtr);
        if (rc != SQL_NO_DATA)
        {
            Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);
            if (StrLen_or_IndPtr >= 0)
                nLongSize = (StrLen_or_IndPtr * (bAddSpaceForNull ? sizeof(SAChar) / sizeof(TSAChar) : 1));
        }

        unsigned char* pBuf;
        size_t nPortionSize = vr.PrepareReader(
            nLongSize,
            IodbcConnection::MaxLongAtExecSize,
            pBuf,
            fnReader,
            nReaderWantedPieceSize,
            pAddlData,
            bAddSpaceForNull);
        assert(nPortionSize <= IodbcConnection::MaxLongAtExecSize);

        SAPieceType_t ePieceType = SA_FirstPiece;
        size_t nTotalRead = 0l;

        unsigned int nCnvtLongSizeMax = (unsigned int)nLongSize;
        size_t nCnvtSize, nCnvtPieceSize = nPortionSize;
        SAPieceType_t eCnvtPieceType;
        size_t nTotalPassedToReader = 0l;

        do
        {
            if (nLongSize)	// known
                nPortionSize = sa_min(nPortionSize, nLongSize - nTotalRead);

            rc = g_odbcAPI.SQLGetData(
                m_handles.m_hstmt, (SQLUSMALLINT)Field.Pos(),
                TargetType, pBuf, nPortionSize + (bAddSpaceForNull ? sizeof(TSAChar) : 0),
                &StrLen_or_IndPtr);

            // Some ODBC drivers can return 0 size instead of SQL_NO_DATA
            //assert(nPortionSize || rc == SQL_NO_DATA);

            if (rc != SQL_NO_DATA)
            {
                Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);
                size_t NumBytes = (size_t)StrLen_or_IndPtr > nPortionSize ||
                    StrLen_or_IndPtr == SQL_NO_TOTAL ?
                nPortionSize : StrLen_or_IndPtr;
                nTotalRead += NumBytes;
                if (!NumBytes) // The size of the piece is 0 - end the reading
                {
                    if (ePieceType == SA_FirstPiece)
                        ePieceType = SA_OnePiece;
                    else
                        ePieceType = SA_LastPiece;
                    rc = SQL_NO_DATA;
                }

                pIConverter->PutStream(pBuf, NumBytes, ePieceType);
                // smart while: initialize nCnvtPieceSize before calling pIConverter->GetStream
                while (nCnvtPieceSize = (nCnvtLongSizeMax ? // known
                    sa_min(nCnvtPieceSize, nCnvtLongSizeMax - nTotalPassedToReader) : nCnvtPieceSize),
                    pIConverter->GetStream(pBuf, nCnvtPieceSize, nCnvtSize, eCnvtPieceType))
                {
                    vr.InvokeReader(eCnvtPieceType, pBuf, nCnvtSize);
                    nTotalPassedToReader += nCnvtSize;

                    if (ePieceType == SA_FirstPiece)
                        ePieceType = SA_NextPiece;
                }
            }
            else
            {
                if (ePieceType == SA_FirstPiece)
                    ePieceType = SA_OnePiece;
                else
                    ePieceType = SA_LastPiece;

                if (!pIConverter->IsEmpty())
                {
                    pIConverter->PutStream(pBuf, 0, ePieceType);
                    nCnvtPieceSize = (nCnvtLongSizeMax ? // known
                        sa_min(nCnvtPieceSize, nCnvtLongSizeMax - nTotalPassedToReader) : nCnvtPieceSize);
                    if (pIConverter->GetStream(pBuf, nCnvtPieceSize, nCnvtSize, ePieceType))
                        vr.InvokeReader(eCnvtPieceType, pBuf, nCnvtSize);
                }
                else
                    vr.InvokeReader(ePieceType, pBuf, 0);
            }

        } while (rc != SQL_NO_DATA);
    }
    else
    {
        assert(eValueType == ISA_ParamValue);
        assert(false);
    }
}

/*virtual */
void IodbcCursor::DescribeParamSP()
{
    SAString sText = m_pCommand->CommandText();
    SAString sCatalogName, sSchemaName, sProcName;

    size_t n = sText.ReverseFind(_TSA('.'));
    if (SIZE_MAX == n)
        sProcName = sText;
    else
    {
        sProcName = sText.Mid(n + 1);
        sSchemaName = sText.Mid(0, n);
        n = sSchemaName.ReverseFind(_TSA('.'));
        if (SIZE_MAX != n)
        {
            sText = sSchemaName;
            sSchemaName = sText.Mid(n + 1);
            sCatalogName = sText.Mid(0, n);
        }
    }

    SACommand cmd(m_pISAConnection->getSAConnection());
    // we limit the number of colums to return
    // because we don't really need them all
    // and some drivers (e.g. Oracle) can simply crash if we bound all
    cmd.setOption(_TSA("ODBC_Internal_LimitColumns")) = _TSA("13");
    cmd.Open();
    odbcCommandHandles *pHandles = (odbcCommandHandles *)cmd.NativeHandles();

    if (((IodbcConnection*)m_pISAConnection)->HasSchemaSupport())
    {
        if (sSchemaName.IsEmpty())
            sSchemaName = SQL_ALL_SCHEMAS;
    }
    else
        sSchemaName.Empty();

    Check(g_odbcAPI.SQLProcedureColumns(
        pHandles->m_hstmt, sCatalogName.IsEmpty() ? NULL :
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
        (SQLTCHAR *)sCatalogName.GetUTF16Chars(),
#else
        (SQLTCHAR *)(const SAChar*)sCatalogName,
#endif
        (SQLSMALLINT)(sCatalogName.IsEmpty() ? 0 : SQL_NTS),
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
        (SQLTCHAR *)sSchemaName.GetUTF16Chars(),
#else
        (SQLTCHAR *)(const SAChar*)sSchemaName,
#endif
        (SQLSMALLINT)SQL_NTS,
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
        (SQLTCHAR *)sProcName.GetUTF16Chars(),
#else
        (SQLTCHAR *)(const SAChar*)sProcName,
#endif
        (SQLSMALLINT)SQL_NTS,
        (SQLTCHAR *)NULL, (SQLSMALLINT)0), SQL_HANDLE_STMT, pHandles->m_hstmt);

    while (cmd.FetchNext())
    {
        //		SAString sCat = cmd.Field(1);		// "PROCEDURE_CAT"
        //		SAString sSchem = cmd.Field(2);		// "PROCEDURE_SCHEM"
        //		SAString sProc = cmd.Field(3);		// "PROCEDURE_NAME"
        SAString sCol = cmd.Field(4);		// "COLUMN_NAME"
        short nColType = cmd.Field(5);		// "COLUMN_TYPE"
        short nDataType = cmd.Field(6);		// "DATA_TYPE"
        //		SAString sType = cmd.Field(7);		// "TYPE_NAME"
        int nColSize = cmd.Field(8).isNull() ? 0 : (int)cmd.Field(8).asLong();		// "COLUMN_SIZE"
        //		long nBufferLength = cmd.Field(9).isNull()? 0 : (long)cmd.Field(9);	// "BUFFER_LENGTH"
        short nDecDigits = cmd.Field(10).isNull() ? (short)0 : (short)cmd.Field(10);	// "DECIMAL_DIGITS"
        //		short nNumPrecPadix = cmd.Field(11).isNull()? 0 : (short)cmd.Field(11);// "NUM_PREC_RADIX"
        //		short nNullable = cmd.Field(12).isNull()? 0 : (short)cmd.Field(12);	// "NULLABLE"
        SAString sDefault = cmd.Field(13);		// "COLUMN_DEF"

        SAParamDirType_t eDirType;
        switch (nColType)
        {
        case SQL_PARAM_INPUT:
            eDirType = SA_ParamInput;
            break;
        case SQL_PARAM_INPUT_OUTPUT:
            eDirType = SA_ParamInputOutput;
            break;
        case SQL_PARAM_OUTPUT:
            eDirType = SA_ParamOutput;
            break;
        case SQL_RETURN_VALUE:
            eDirType = SA_ParamReturn;
            break;
        case SQL_PARAM_TYPE_UNKNOWN:
        case SQL_RESULT_COL:
            continue;
        default:
            assert(false);
            continue;
        }
        SADataType_t eParamType = IodbcConnection::CnvtNativeToStd(nDataType, nColSize, nColSize, nDecDigits);

        SAString sParamName;
        if (sCol.IsEmpty())
        {
            assert(eDirType == SA_ParamOutput);
            eDirType = SA_ParamReturn;
            sParamName = _TSA("RETURN_VALUE");
        }
        else
            sParamName = sCol;

        m_pCommand->CreateParam(sParamName, eParamType, nDataType, nColSize, nColSize, nDecDigits, eDirType);
    }
}

/*virtual */
saAPI *IodbcConnection::NativeAPI() const
{
    return &g_odbcAPI;
}

/*virtual */
saConnectionHandles *IodbcConnection::NativeHandles()
{
    return &m_handles;
}

/*virtual */
saCommandHandles *IodbcCursor::NativeHandles()
{
    return &m_handles;
}

/*virtual */
void IodbcConnection::setIsolationLevel(
    SAIsolationLevel_t eIsolationLevel)
{
    Commit();

    issueIsolationLevel(eIsolationLevel);
}

void IodbcConnection::issueIsolationLevel(
    SAIsolationLevel_t eIsolationLevel)
{
    SQLPOINTER isolation;
    switch (eIsolationLevel)
    {
    case SA_ReadUncommitted:
        isolation = (SQLPOINTER)SQL_TXN_READ_UNCOMMITTED;
        break;
    case SA_ReadCommitted:
        isolation = (SQLPOINTER)SQL_TXN_READ_COMMITTED;
        break;
    case SA_RepeatableRead:
        isolation = (SQLPOINTER)SQL_TXN_REPEATABLE_READ;
        break;
    case SA_Serializable:
        isolation = (SQLPOINTER)SQL_TXN_SERIALIZABLE;
        break;
    default:
        assert(false);
        return;
    }

    assert(m_handles.m_hdbc);

    Check(g_odbcAPI.SQLSetConnectAttr(
        m_handles.m_hdbc,
        SQL_ATTR_TXN_ISOLATION,
        isolation, 0), SQL_HANDLE_DBC, m_handles.m_hdbc);
}

/*virtual */
void IodbcConnection::setAutoCommit(
    SAAutoCommit_t eAutoCommit)
{
    SQLPOINTER nAutoCommit;
    switch (eAutoCommit)
    {
    case SA_AutoCommitOff:
        nAutoCommit = (SQLPOINTER)SQL_AUTOCOMMIT_OFF;
        break;
    case SA_AutoCommitOn:
        nAutoCommit = (SQLPOINTER)SQL_AUTOCOMMIT_ON;
        break;
    default:
        assert(false);
        return;
    }

    assert(m_handles.m_hdbc);

    Check(g_odbcAPI.SQLSetConnectAttr(
        m_handles.m_hdbc,
        SQL_ATTR_AUTOCOMMIT,
        nAutoCommit, 0), SQL_HANDLE_DBC, m_handles.m_hdbc);
}

/*virtual */
void IodbcConnection::CnvtInternalToNumeric(
    SANumeric &numeric,
    const void *pInternal,
    int nInternalSize)
{
    if (m_bUseNumeric)
    {
        assert((size_t)nInternalSize == sizeof(SQL_NUMERIC_STRUCT));

        SQL_NUMERIC_STRUCT *pSQL_NUMERIC_STRUCT = (SQL_NUMERIC_STRUCT *)pInternal;
        numeric.precision = pSQL_NUMERIC_STRUCT->precision;
        numeric.scale = pSQL_NUMERIC_STRUCT->scale;
        // set sign
        // ODBC: 1 if positive, 0 if negative
        // SQLAPI: 1 if positive, 0 if negative
        numeric.sign = pSQL_NUMERIC_STRUCT->sign;
        // copy mantissa
        // ODBC: little endian format
        // SQLAPI: little endian format
        memset(numeric.val, 0, sizeof(numeric.val));
        memcpy(
            numeric.val,
            pSQL_NUMERIC_STRUCT->val,
            sa_min(sizeof(numeric.val), sizeof(pSQL_NUMERIC_STRUCT->val)));
    }
    else
    {
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
        SAString String;
        String.SetUTF16Chars(pInternal, nInternalSize / sizeof(TSAChar));
        numeric = String;
#else
        numeric = SAString((const SAChar*)pInternal, nInternalSize / sizeof(SAChar));
#endif
    }
}

/*static */
void IodbcConnection::CnvtNumericToInternal(
    const SANumeric &numeric,
    SQL_NUMERIC_STRUCT &Internal)
{
    Internal.precision = numeric.precision;
    Internal.scale = numeric.scale;
    // set sign
    // ODBC: 1 if positive, 0 if negative
    // SQLAPI: 1 if positive, 0 if negative
    Internal.sign = numeric.sign;
    // copy mantissa
    // ODBC: little endian format
    // SQLAPI: little endian format
    memset(Internal.val, 0, sizeof(Internal.val));
    memcpy(
        Internal.val,
        numeric.val,
        sa_min(sizeof(numeric.val), sizeof(Internal.val)));
}

/*static */
void IodbcConnection::CnvtNumericToInternal(
    const SANumeric &numeric,
    SA_ODBC_SQL_NUMERIC_STRUCT Internal,
    SQLLEN &StrLen_or_IndPtr)
{
    SAString sNum = numeric.operator SAString();
    StrLen_or_IndPtr = sNum.GetLength() * sizeof(TSAChar);
#if defined(SA_UNICODE) && (SA_ODBC_SQL_WCHART_CONVERT <= 0)
    memcpy(Internal, sNum.GetUTF16Chars(), (sNum.GetLength() + 1) * sizeof(TSAChar));
#else
    sa_strcpy(Internal, (const SAChar*)sNum);
#endif
}

