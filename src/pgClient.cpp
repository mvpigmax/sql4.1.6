// pgClient.cpp
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <pgAPI.h>
#include "pgClient.h"

#include "pgType.h" // Result of the "cat include/catalog/pg_type.h | grep '#define'"

static void Check(PGresult *res)
{
    int nErr = g_pgAPI.PQresultStatus(res);

    if (!(nErr == PGRES_COMMAND_OK || nErr == PGRES_TUPLES_OK ||
        nErr == PGRES_SINGLE_TUPLE || nErr == PGRES_NONFATAL_ERROR))
    {
        SAString sMsg, sCod(_TSA("???"));
        if (res)
        {
#ifdef SA_UNICODE_WITH_UTF8
            sMsg.SetUTF8Chars(g_pgAPI.PQresultErrorMessage(res));
            if (NULL != g_pgAPI.PQresultErrorField)
                sCod.SetUTF8Chars(g_pgAPI.PQresultErrorField(res, PG_DIAG_SQLSTATE));
#else
            sMsg = SAString(g_pgAPI.PQresultErrorMessage(res));
            if (NULL != g_pgAPI.PQresultErrorField)
                sCod = SAString(g_pgAPI.PQresultErrorField(res, PG_DIAG_SQLSTATE));
#endif
        }
        else
            sMsg.Format(_TSA("Unknown error(%d) occured"), nErr);

        throw SAException(SA_DBMS_API_Error, nErr, -1, _TSA("%") SA_FMT_STR _TSA(" %") SA_FMT_STR,
            (const SAChar*)sCod, (const SAChar*)sMsg);
    }
}

extern long g_nPostgreSQLDLLVersionLoaded;

//////////////////////////////////////////////////////////////////////
// IpgConnection Class
//////////////////////////////////////////////////////////////////////

class IpgConnection : public ISAConnection
{
    friend class IpgCursor;

    SAMutex m_pgExecMutex;
    pgConnectionHandles m_handles;

    int m_nServerVersion;
    bool m_bUseEEscape;

    enum MaxLong { MaxBlobPiece = (unsigned int)4096, MaxLongPiece = (unsigned int)0x7FFFFFFF };

    static void CnvtInternalToNumeric(
        SANumeric &numeric,
        const char *sValue);

    static SADataType_t CnvtNativeToStd(
        Oid NativeType,
        int Mod,
        int Format,
        int &Length,
        int &Prec,
        int &Scale,
        bool bOidAsBlob);
    static void CnvtInternalToDateTime(
        SADateTime &date_time,
        const char *sInternal);
    static int second(const char *sSecond);
    static int minute(const char *sMinute);
    static int hour(const char *sHour);
    static int day(const char *sDay);
    static int month(const char *sMonth);
    static int year(const char *sYear);
    static void CnvtDateTimeToInternal(
        const SADateTime &date_time,
        SAString &sInternal);
    static void ParseInternalTime(const char* sTime, int& nHour, int& nMinute, int& nSecond, int& nNanoSecond, int& nZoneHour);
    static void ParseInternalDate(const char* sDate, int& nYear, int& nMonth, int& nDay);
    char* byte2string(const void* pByte, size_t nBufLen);
    static void* string2byte(const char* szInputText, size_t &nOutBufLen);
    SAString EscapeString(const char* szString);

    void ExecuteImmediate(const char *sStmt, bool lock = true);

    void StartTransactionIndirectly();
    bool m_bTransactionStarted;

protected:
    virtual ~IpgConnection();

public:
    IpgConnection(SAConnection *pSAConnection);

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

void IpgConnection::ExecuteImmediate(const char *sStmt, bool lock/* = true*/)
{
    PGresult *pPGresult = NULL;

    try
    {
        if (lock)
        {
            SACriticalSectionScope scope(&m_pgExecMutex);
            SA_TRACE_CMDC(SAString(sStmt));
            pPGresult = g_pgAPI.PQexec(m_handles.conn, sStmt);
            Check(pPGresult);
        }
        else
        {
            SA_TRACE_CMDC(SAString(sStmt));
            pPGresult = g_pgAPI.PQexec(m_handles.conn, sStmt);
            Check(pPGresult);
        }

        if (pPGresult)
            g_pgAPI.PQclear(pPGresult);
    }
    catch (SAException &)	// clean-up
    {
        if (pPGresult)
            g_pgAPI.PQclear(pPGresult);
        throw;
    }
}

/*static */
SADataType_t IpgConnection::CnvtNativeToStd(
    Oid NativeType,
    int Mod,
    int Format,
    int &Length,
    int &Prec,
    int &Scale,
    bool bOidAsBlob)
{
    SADataType_t eDataType;

    switch (NativeType)
    {
    case BOOLOID: // BOOLEAN|BOOL
        eDataType = SA_dtBool;
        break;

    case BYTEAOID: // BYTEA - variable-length string, binary values escaped
        Length = INT_MAX;
        eDataType = SA_dtLongBinary;
        break;

    case TEXTOID: // TEXT - variable-length string, no limit specified
        Length = INT_MAX;
        eDataType = SA_dtLongChar;
        break;

    case NAMEOID: // NAME - 31-character type for storing system identifiers
        Length -= 1;
    case CHAROID: // CHAR - single character
    case BPCHAROID: // BPCHAR - char(length), blank-padded string, fixed storage length
    case VARCHAROID: // VARCHAR(?) - varchar(length), non-blank-padded string, variable storage length
        if (Length < 0 && Mod > 4)
            Length = Mod - 4; // 4 bytes for data length?
        eDataType = SA_dtString;
        break;

    case INT8OID: // INT8|BIGINT - ~18 digit integer, 8-byte storage
        Prec = 19; // -9223372036854775808 to 9223372036854775807
        eDataType = SA_dtNumeric;
        break;

    case FLOAT4OID: // FLOAT4 - single-precision floating point number, 4-byte storage
    case FLOAT8OID: // FLOAT8 - double-precision floating point number, 8-byte storage 
    case CASHOID: // $d,ddd.cc, money
        eDataType = SA_dtDouble;
        break;

    case NUMERICOID: // NUMERIC(x,y) - numeric(precision, decimal), arbitrary precision number
        Prec = (Mod >> 16) & 0xFFFF;
        Scale = (Mod & 0xFFFF) - 4; // 4 is a fixed offset
        eDataType = SA_dtNumeric;
        break;

    case INT2OID: // INT2|SMALLINT - -32 thousand to 32 thousand, 2-byte storage
        eDataType = SA_dtShort;
        break;

    case INT4OID: // INT4|INTEGER - -2 billion to 2 billion integer, 4-byte storage
    case XIDOID: // transaction id
    case CIDOID: // command identifier type, sequence in transaction id
        eDataType = SA_dtLong;
        break;

    case OIDOID: // OID - object identifier(oid), maximum 4 billion
        eDataType = bOidAsBlob ? SA_dtBLob : SA_dtLong;
        break;

    case ABSTIMEOID: // ABSTIME - absolute, limited-range date and time (Unix system time)
    case DATEOID: // DATE - ANSI SQL date
    case TIMEOID: // TIME - hh:mm:ss, ANSI SQL time
    case TIMESTAMPOID: // TIMESTAMP - date and time
    case TIMESTAMPTZOID: // TIMESTAMP - date and time WITH TIMEZONE
    case TIMETZOID: // TIME WITH TIMEZONE - hh:mm:ss, ANSI SQL time
        eDataType = SA_dtDateTime;
        break;

    case 705: // Unknown

    case INT2VECTOROID: // array of INDEX_MAX_KEYS int2 integers, used in system tables
    case REGPROCOID: // registered procedure
    case TIDOID: // (Block, offset), physical location of tuple
    case OIDVECTOROID: // array of INDEX_MAX_KEYS oids, used in system tables
    case POINTOID: // geometric point '(x, y)'
    case LSEGOID: // geometric line segment '(pt1,pt2)'
    case PATHOID: // geometric path '(pt1,...)'
    case BOXOID: // geometric box '(lower left,upper right)'
    case POLYGONOID: // geometric polygon '(pt1,...)'
    case LINEOID: // geometric line '(pt1,pt2)'

        // TODO: ???
    case RELTIMEOID: // relative, limited-range time interval (Unix delta time)
    case TINTERVALOID: // (abstime,abstime), time interval
    case INTERVALOID: // @ <number> <units>, time interval

    case CIRCLEOID: // geometric circle '(center,radius)'
        // locale specific
    case INETOID: // IP address/netmask, host address, netmask optional
    case CIDROID: // network IP address/netmask, network address
    case 829: // XX:XX:XX:XX:XX:XX, MAC address
    case BITOID: // BIT(?) - fixed-length bit string
    case VARBITOID: // BIT VARING(?) - variable-length bit string
        eDataType = SA_dtString;
        break;

    default:
        // 2011-10-07: Use SAString always (text or binary)
        // assert(false);
        eDataType = 0 == Format ? SA_dtString : SA_dtBytes;
    }

    return eDataType;
}

//////////////////////////////////////////////////////////////////////
// IpgConnection Construction/Destruction
//////////////////////////////////////////////////////////////////////

IpgConnection::IpgConnection(
    SAConnection *pSAConnection) : ISAConnection(pSAConnection)
{
    Reset();
    // set the default server version 7.3
    m_nServerVersion = 70300;
    m_bUseEEscape = false;
}

IpgConnection::~IpgConnection()
{
}

//////////////////////////////////////////////////////////////////////
// IpgClient Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IpgClient::IpgClient()
{
}

IpgClient::~IpgClient()
{
}

ISAConnection *IpgClient::QueryConnectionInterface(
    SAConnection *pSAConnection)
{
    return new IpgConnection(pSAConnection);
}

//////////////////////////////////////////////////////////////////////
// IpgCursor Class
//////////////////////////////////////////////////////////////////////

class IpgCursor : public ISACursor
{
    pgCommandHandles	m_handles;

    // private handles
    int m_nCurrentTuple;
    int m_nTuplesCount;
    bool m_bResultSetCanBe;
    SAString m_sCursor;
    SAString m_sRowsToPrefetch;
    SAString m_sProcCmd;

    bool		m_bOpened;

    void ConvertPGTupleToFields(int nTuple);
    void ConvertPGTupleToParams(int nTuple);
    void ConvertPGTupleToValue(int nTuple, int iField, SADataType_t eType, Oid nNativeType, ValueType_t eValueType, SAValueRead& value);
    void BindBLob(SAParam &Param, SAString &sBoundStmt);
    void BindLongChar(SAParam &Param, SAString &sBoundStmt);
    void BindLongBinary(SAParam &Param, SAString &sBoundStmt);
    void ReadBLOB(
        ValueType_t eValueType,
        SAValueRead &vr,
        void *pValue,
        size_t nFieldBufSize,
        saLongOrLobReader_t fnReader,
        size_t nReaderWantedPieceSize,
        void *pAddlData);

    void ReadLongBinary(
        ValueType_t eValueType,
        SAValueRead &vr,
        void *pValue,
        size_t nFieldBufSize,
        saLongOrLobReader_t fnReader,
        size_t nReaderWantedPieceSize,
        void *pAddlData);

    void ReadLongChar(
        ValueType_t eValueType,
        SAValueRead &vr,
        void *pValue,
        size_t nFieldBufSize,
        saLongOrLobReader_t fnReader,
        size_t nReaderWantedPieceSize,
        void *pAddlData);

public:
    IpgCursor(
        IpgConnection *pImyConnection,
        SACommand *pCommand);
    virtual ~IpgCursor();

protected:
    virtual bool IsOpened();
    virtual void Open();
    virtual void Close();
    virtual void Destroy();
    virtual void Reset();

    // prepare statement (also convert to native format if needed)
    virtual void Prepare(
        const SAString &sStmt,
        SACommandType_t eCmdType,
        int nPlaceHolderCount,
        saPlaceHolder **ppPlaceHolders);
    // executes statement (also binds parameters if needed)
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
    virtual void SetFieldBuffer(
        int nCol,	// 1-based
        void *pInd,
        size_t nIndSize,
        void *pSize,
        size_t nSizeSize,
        void *pValue,
        size_t nValueSize);
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

public:
    void OnTransactionClosed(void * /*pAddlData*/);
};

static void SQLAPI_CALLBACK TransactionClosed(ISACursor *pCursor, void *pAddlData)
{
    ((IpgCursor*)pCursor)->OnTransactionClosed(pAddlData);
}

//////////////////////////////////////////////////////////////////////
// IpgCursor Construction/Destruction
//////////////////////////////////////////////////////////////////////

IpgCursor::IpgCursor(
    IpgConnection *pIpgConnection,
    SACommand *pCommand) :
    ISACursor(pIpgConnection, pCommand)
{
    Reset();
}

/*virtual */
IpgCursor::~IpgCursor()
{
}


//////////////////////////////////////////////////////////////////////
// IpgConnection implementation
//////////////////////////////////////////////////////////////////////

/*virtual */
void IpgConnection::InitializeClient()
{
    ::AddPostgreSQLSupport(m_pSAConnection);
}

/*virtual */
void IpgConnection::UnInitializeClient()
{
    if (SAGlobals::UnloadAPI()) ::ReleasePostgreSQLSupport();
}

/*virtual */
void IpgConnection::CnvtInternalToDateTime(
    SADateTime &/*date_time*/,
    const void * /*pInternal*/,
    int/*	nInternalSize*/)
{
    assert(false);	// we do not use ISA... convertors
}

/*virtual */
void IpgConnection::CnvtInternalToInterval(
    SAInterval & /*interval*/,
    const void * /*pInternal*/,
    int /*nInternalSize*/)
{
    assert(false);
}

/*static */
int IpgConnection::second(const char *sSecond)
{
    char s[3] = "SS";
    strncpy(s, sSecond, 2);

    int nSecond = atoi(s);
    assert(nSecond >= 0 && nSecond <= 59);

    return nSecond;
}

/*static */
int IpgConnection::minute(const char *sMinute)
{
    char s[3] = "MM";
    strncpy(s, sMinute, 2);

    int nMinute = atoi(s);
    assert(nMinute >= 0 && nMinute <= 59);

    return nMinute;
}

/*static */
int IpgConnection::hour(const char *sHour)
{
    char s[3] = "HH";
    strncpy(s, sHour, 2);

    int nHour = atoi(s);
    assert(nHour >= 0 && nHour <= 23);

    return nHour;
}

/*static */
int IpgConnection::day(const char *sDay)
{
    char s[3] = "DD";
    strncpy(s, sDay, 2);

    int nDay = atoi(s);
    assert(nDay >= 0 && nDay <= 31);

    return nDay;
}

/*static */
int IpgConnection::month(const char *sMonth)
{
    char s[3] = "MM";
    strncpy(s, sMonth, 2);

    int nMonth = atoi(s);
    assert(nMonth >= 0 && nMonth <= 12);

    return nMonth;
}

/*static */
int IpgConnection::year(const char *sYear)
{
    char s[5] = "YYYY";
    strncpy(s, sYear, 4);

    int nYear = atoi(s);
    assert(nYear >= 0 && nYear <= 9999);

    return nYear;
}

/*static*/
void IpgConnection::ParseInternalTime(const char* sTime, int& nHour, int& nMinute, int& nSecond, int& nNanoSecond, int& nZoneHour)
{
    // 07:37:16-08, 07:37:16.00 PST, 07:37:16 PST
    assert(sTime[2] == ':');
    assert(sTime[5] == ':');
    nHour = hour(sTime);
    nMinute = minute(sTime + 3);
    nSecond = second(sTime + 6);

    char *sTZ = (char*)(sTime + 8);

    if (sTZ[0] == '.') // 07:37:16.000
    {
        int i = 1;
        for (int m = 100000000; isdigit(sTZ[i]); ++i, m /= 10)
            nNanoSecond += (sTZ[i] - '0') * m;
        sTZ += i;
    }

    if (sTZ[0] == ' ') // 07:37:16 PST
    {
        // TODO: PST like timezone
        assert(false);
    }
    else if (sTZ[0] == '-' || sTZ[0] == '+') // 07:37:16-08
        nZoneHour = atoi(sTZ);
}

/*static*/
void IpgConnection::ParseInternalDate(const char* sDate, int& nYear, int& nMonth, int& nDay)
{
    // 1997-12-17, 12/17/1997, 17.12.1997
    if (sDate[4] == '-') // 1997-12-17
    {
        assert(sDate[7] == '-');
        nYear = year(sDate);
        nMonth = month(sDate + 5);
        nDay = day(sDate + 8);
    }
    else if (sDate[2] == '/') // 12/17/1997
    {
        assert(sDate[5] == '/');
        // TODO: Date format may be MM/DD/YYYY(American) or DD/MM/YYYY(European).
        // Can't detect it correctly in some cases. But...
        if (atoi(sDate) > 12) // European
        {
            nDay = day(sDate);
            nMonth = month(sDate + 3);
            nYear = year(sDate + 6);
        }
        else // American
        {
            nMonth = month(sDate);
            nDay = day(sDate + 3);
            nYear = year(sDate + 6);
        }
    }
    else if (sDate[2] == '.') // 17.12.1997
    {
        assert(sDate[5] == '.');
        nDay = day(sDate);
        nMonth = month(sDate + 3);
        nYear = year(sDate + 6);
    }
    else
        assert(false);
}

/*static */
void IpgConnection::CnvtInternalToDateTime(
    SADateTime &date_time,
    const char *sInternal)
{
    // first initialize to default date/time values
    SADateTime dt(1900, 1, 1, 0, 0, 0);
    int nYear = dt.GetYear();
    int nMonth = dt.GetMonth();
    int nDay = dt.GetDay();
    int nHour = 0;
    int nMinute = 0;
    int nSecond = 0;
    int nNanoSecond = 0;
    int nZoneHour = 0;

    // Available date time formats
    // ISO		1997-12-17 07:37:16-08 
    // SQL		12/17/1997 07:37:16.00 PST 
    // Postgres	Wed Dec 17 07:37:16 1997 PST 
    // German	17.12.1997 07:37:16.00 PST 

    if (::strlen(sInternal) < 19) // date only or time only
    {
        if (strchr(sInternal, ':')) // time
            ParseInternalTime(sInternal, nHour, nMinute, nSecond, nNanoSecond, nZoneHour);
        else // date
            ParseInternalDate(sInternal, nYear, nMonth, nDay);
    }
    else // datetime
    {
        if (isdigit(sInternal[0])) // ISO, SQL or German
        {
            ParseInternalDate(sInternal, nYear, nMonth, nDay);
            assert(strchr(sInternal, ' '));
            ParseInternalTime(sInternal + 11, nHour, nMinute, nSecond, nNanoSecond, nZoneHour);
        }
        else // Postgres format
        {
            // TODO:
            assert(false);
        }
    }

    if (nZoneHour != 0)
    {
    }

    if (nMonth != 0 && nDay != 0 && nHour <= 23)	// simple test for validness
        date_time = SADateTime(nYear, nMonth, nDay, nHour, nMinute, nSecond);
    else
        date_time = dt;

    date_time.Fraction() = nNanoSecond;
}

/*static */
void IpgConnection::CnvtDateTimeToInternal(
    const SADateTime &date_time,
    SAString &sInternal)
{
    // Datetime's format is YYYY-MM-DD HH:MM:SS.fraction
    sInternal.Format(_TSA("%.4d-%.2d-%.2d %.2d:%.2d:%.2d.%.3d"),
        date_time.GetYear(), date_time.GetMonth(), date_time.GetDay(),
        date_time.GetHour(), date_time.GetMinute(), date_time.GetSecond(),
        date_time.Fraction() / 1000000);
}

/*virtual */
void IpgConnection::CnvtInternalToCursor(
    SACommand * /*pCursor*/,
    const void * /*pInternal*/)
{
    assert(false);
}

/*virtual */
long IpgConnection::GetClientVersion() const
{
    return g_nPostgreSQLDLLVersionLoaded;
}

/*virtual */
long IpgConnection::GetServerVersion() const
{
    return SAExtractVersionFromString(GetServerVersionString());
}

/*virtual */
SAString IpgConnection::GetServerVersionString() const
{
    SACommand cmd(m_pSAConnection, _TSA("select version()"));
    cmd.Execute();
    cmd.FetchNext();
    SAString sVersion = cmd.Field(1).asString();
    cmd.Close();

    return sVersion;
}

/*virtual */
bool IpgConnection::IsConnected() const
{
    return m_handles.conn != NULL;
}

/*virtual */
bool IpgConnection::IsAlive() const
{
    return CONNECTION_OK == g_pgAPI.PQstatus(m_handles.conn);
}

/*virtual */
void IpgConnection::Connect(
    const SAString &sDBString,
    const SAString &sUserID,
    const SAString &sPassword,
    saConnectionHandler_t fHandler)
{
    assert(m_handles.conn == NULL);

    // dbstring as: [server_name][@][dbname][;options]
    // server_name as: hostname[,port], or unix_socket path
    // for connection to server without dbname use 'server_name@'

    SAString sServerName, sDatabase, sHost, sPort, sMsg, sOptions;

    size_t iPos = sDBString.Find(_TSA('@'));
    if (SIZE_MAX == iPos)
        sDatabase = sDBString;
    else // Database is present in connection string
    {
        sServerName = sDBString.Left(iPos);
        sDatabase = sDBString.Mid(iPos + 1);
    }

    iPos = sDatabase.Find(_TSA(';'));
    if (SIZE_MAX != iPos)
    {
        sMsg = sDatabase;
        sDatabase = sMsg.Left(iPos);
        sOptions = sMsg.Mid(iPos + 1);
    }

    iPos = sServerName.Find(_TSA(','));
    if (SIZE_MAX == iPos)
        sHost = sServerName;
    else // alternate port number found
    {
        sHost = sServerName.Left(iPos);
        sPort = sServerName.Mid(iPos + 1);
    }

    if (NULL != fHandler)
        fHandler(*m_pSAConnection, SA_PreConnectHandler);

    SAString sOption = m_pSAConnection->Option(SACON_OPTION_APPNAME);
    if (!sOption.IsEmpty())
    {
        if (sDatabase.IsEmpty())
            sDatabase.Format(_TSA("application_name='%") SA_FMT_STR _TSA("'"), (const SAChar*)sOption);
        else
        {
            SAString sDbSave(sDatabase);
            sDatabase.Format(_TSA("application_name='%") SA_FMT_STR _TSA("' dbname='%") SA_FMT_STR _TSA("'"),
                (const SAChar*)sOption, (const SAChar*)sDbSave);
        }
    }

    m_handles.conn = g_pgAPI.PQsetdbLogin(
#ifdef SA_UNICODE_WITH_UTF8
        sHost.IsEmpty() ? (const char *)NULL : sHost.GetUTF8Chars(),
        sPort.IsEmpty() ? (const char *)NULL : sPort.GetUTF8Chars(),
        sOptions.IsEmpty() ? (const char *)NULL : sOptions.GetUTF8Chars(),
        NULL,
        sDatabase.IsEmpty() ? (const char *)NULL : sDatabase.GetUTF8Chars(),
        sUserID.IsEmpty() ? (const char *)NULL : sUserID.GetUTF8Chars(),
        sPassword.IsEmpty() ? (const char *)NULL : sPassword.GetUTF8Chars());
#else
        sHost.IsEmpty() ? (const char *)NULL : sHost.GetMultiByteChars(),
        sPort.IsEmpty() ? (const char *)NULL : sPort.GetMultiByteChars(),
        sOptions.IsEmpty() ? (const char *)NULL : sOptions.GetMultiByteChars(),
        NULL,
        sDatabase.IsEmpty() ? (const char *)NULL : sDatabase.GetMultiByteChars(),
        sUserID.IsEmpty() ? (const char *)NULL : sUserID.GetMultiByteChars(),
        sPassword.IsEmpty() ? (const char *)NULL : sPassword.GetMultiByteChars());
#endif

    if (g_pgAPI.PQstatus(m_handles.conn) == CONNECTION_BAD)
    {
        // It seems the message is always sql_ascii
        /*
#ifdef SA_UNICODE_WITH_UTF8
        int encoding = -1;
        if( NULL != g_pgAPI.PQclientEncoding )
        encoding = g_pgAPI.PQclientEncoding(m_handles.conn);
        SAString sEncoding(NULL != g_pgAPI.pg_encoding_to_char ? g_pgAPI.pg_encoding_to_char(encoding):"");
        if(  0 == sEncoding.CompareNoCase(_TSA("utf8")) || 0 == sEncoding.CompareNoCase(_TSA("unicode")) || 0 == sEncoding.CompareNoCase(_TSA("utf-8")) )
        sMsg.SetUTF8Chars(g_pgAPI.PQerrorMessage(m_handles.conn));
        else
        #endif
        */
        sMsg = SAString(g_pgAPI.PQerrorMessage(m_handles.conn));
        throw SAException(SA_DBMS_API_Error, CONNECTION_BAD, -1, sMsg);
    }

    if (NULL != g_pgAPI.PQserverVersion)
        m_nServerVersion = g_pgAPI.PQserverVersion(m_handles.conn);
    if (m_nServerVersion >= 80204)
    {
        char* szByteStr = byte2string("\\000", 4);
        m_bUseEEscape = szByteStr[2] == '\\';
        if (NULL != g_pgAPI.PQfreemem)
            g_pgAPI.PQfreemem(szByteStr);
        else
            ::free(szByteStr);
    }

    if (NULL != g_pgAPI.PQsetClientEncoding)
    {
#ifdef SA_UNICODE_WITH_UTF8
        g_pgAPI.PQsetClientEncoding(m_handles.conn, "utf8");
#else
        sOption = m_pSAConnection->Option(_TSA("ClientEncoding"));
        if (!sOption.IsEmpty())
            g_pgAPI.PQsetClientEncoding(m_handles.conn, sOption.GetMultiByteChars());
#endif
    }

    if (NULL != fHandler)
        fHandler(*m_pSAConnection, SA_PostConnectHandler);
}

/*virtual */
void IpgConnection::Disconnect()
{
    assert(m_handles.conn != NULL);

    g_pgAPI.PQfinish(m_handles.conn);
    m_handles.conn = NULL;
}

/*virtual */
void IpgConnection::Destroy()
{
    assert(m_handles.conn != NULL);

    g_pgAPI.PQfinish(m_handles.conn);
    m_handles.conn = NULL;
}

/*virtual */
void IpgConnection::Reset()
{
    SACriticalSectionScope scope(&m_pgExecMutex);

    m_handles.conn = NULL;
    m_bTransactionStarted = false;
}

void IpgConnection::StartTransactionIndirectly()
{
    SACriticalSectionScope scope(&m_pgExecMutex);
    if (!m_bTransactionStarted && m_pSAConnection->AutoCommit() == SA_AutoCommitOff)
    {
        ExecuteImmediate("BEGIN", false);
        m_bTransactionStarted = true;
    }
}

/*virtual */
void IpgConnection::Commit()
{
    SACriticalSectionScope scope(&m_pgExecMutex);

    ExecuteImmediate("COMMIT", false);

    // notify transaction changed
    EnumCursors(TransactionClosed, NULL);

    // DISABLED: start transaction only when first query executed
    m_bTransactionStarted = false;
    // and start new transaction if not in autocommit mode
    // if(m_pSAConnection->AutoCommit() == SA_AutoCommitOff)
    //	ExecuteImmediate("BEGIN", false);
}

/*virtual */
void IpgConnection::Rollback()
{
    SACriticalSectionScope scope(&m_pgExecMutex);

    ExecuteImmediate("ROLLBACK", false);

    // notify transaction changed
    EnumCursors(TransactionClosed, NULL);

    // DISABLED: start transaction only when first query executed
    m_bTransactionStarted = false;
    // and start new transaction if not in autocommit mode
    //if(m_pSAConnection->AutoCommit() == SA_AutoCommitOff)
    //	ExecuteImmediate("BEGIN", false);
}

/*virtual */
saAPI *IpgConnection::NativeAPI() const
{
    return 	&g_pgAPI;
}

/*virtual */
saConnectionHandles *IpgConnection::NativeHandles()
{
    return &m_handles;
}

/*virtual */
void IpgConnection::setIsolationLevel(
    SAIsolationLevel_t eIsolationLevel)
{
    SAString sCmd(_TSA("SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL "));

    switch (eIsolationLevel)
    {
    case SA_ReadUncommitted:
        sCmd += _TSA("READ UNCOMMITTED");
        break;
    case SA_RepeatableRead:
        sCmd += _TSA("REPEATABLE READ");
        break;
    case SA_Serializable:
        sCmd += _TSA("SERIALIZABLE");
        break;
    case SA_ReadCommitted:
    default:
        sCmd += _TSA("READ COMMITTED");
        break;
    }

    ExecuteImmediate(sCmd.GetMultiByteChars());
}

/*virtual */
void IpgConnection::setAutoCommit(
    SAAutoCommit_t/* eAutoCommit*/)
{
    SACriticalSectionScope scope(&m_pgExecMutex);

    // if we have issued a "BEGIN" earlier, we have to close it
    if (m_pSAConnection->AutoCommit() == SA_AutoCommitOff)
    {
        ExecuteImmediate("COMMIT", false);

        // notify transaction changed
        EnumCursors(TransactionClosed, NULL);
    }

    // DISABLED: start transaction only when first query executed
    m_bTransactionStarted = false;
    // if transactional mode is requested, issue a "BEGIN" but
    // only if AutoCommit was turned on
    //else if(eAutoCommit == SA_AutoCommitOff)
    //	ExecuteImmediate("BEGIN", false);
}

/*virtual */
ISACursor *IpgConnection::NewCursor(SACommand *m_pCommand)
{
    return new IpgCursor(this, m_pCommand);
}

//////////////////////////////////////////////////////////////////////
// IpgCursor implementation
//////////////////////////////////////////////////////////////////////

/*virtual */
bool IpgCursor::IsOpened()
{
    return m_bOpened;
}

/*virtual */
void IpgCursor::Open()
{
    assert(m_bOpened == false);

    m_bOpened = true;
}

/*virtual */
void IpgCursor::Close()
{
    assert(m_bOpened == true);

    if (m_handles.res)
    {
        g_pgAPI.PQclear(m_handles.res);
        m_handles.res = NULL;
    }

    if (!m_sCursor.IsEmpty())
    {
        m_sCursor = _TSA("CLOSE ") + m_sCursor;
        ((IpgConnection*)m_pISAConnection)->ExecuteImmediate(
            m_sCursor.GetMultiByteChars());
    }

    m_sCursor.Empty();
    m_sRowsToPrefetch.Empty();
    m_nCurrentTuple = m_nTuplesCount = 0;
    m_eLastFetchType = EFTYP_NONE;
    m_bResultSetCanBe = m_bOpened = false;
}

/*virtual */
void IpgCursor::Destroy()
{
    assert(m_bOpened == true);

    if (m_handles.res)
    {
        g_pgAPI.PQclear(m_handles.res);
        m_handles.res = NULL;
    }

    m_sCursor.Empty();
    m_sRowsToPrefetch.Empty();
    m_nCurrentTuple = m_nTuplesCount = 0;
    m_eLastFetchType = EFTYP_NONE;
    m_bResultSetCanBe = m_bOpened = false;
}

/*virtual */
void IpgCursor::Reset()
{
    m_handles.res = NULL;

    m_sCursor.Empty();
    m_sRowsToPrefetch.Empty();
    m_nCurrentTuple = m_nTuplesCount = 0;
    m_eLastFetchType = EFTYP_NONE;
    m_bResultSetCanBe = m_bOpened = false;
}

// prepare statement (also convert to native format if needed)
/*virtual */
void IpgCursor::Prepare(
    const SAString &/*sStmt*/,
    SACommandType_t/* eCmdType*/,
    int/* nPlaceHolderCount*/,
    saPlaceHolder ** /*ppPlaceHolders*/)
{
    // no prepare in libpq
    // all is beeing done in Execute()
}

/*virtual */
void IpgCursor::UnExecute()
{
    m_nCurrentTuple = m_nTuplesCount = 0;
    m_eLastFetchType = EFTYP_NONE;
    m_bResultSetCanBe = false;

    if (m_handles.res)
    {
        g_pgAPI.PQclear(m_handles.res);
        m_handles.res = NULL;
    }

    if (!m_sCursor.IsEmpty())
    {
        m_sCursor = _TSA("CLOSE ") + m_sCursor;
        ((IpgConnection*)m_pISAConnection)->ExecuteImmediate(
            m_sCursor.GetMultiByteChars());
    }

    m_sCursor.Empty();
    m_sRowsToPrefetch.Empty();
}

// executes statement (also binds parameters if needed)
/*virtual */
void IpgCursor::Execute(
    int nPlaceHolderCount,
    saPlaceHolder **ppPlaceHolders)
{
    m_nCurrentTuple = 0;
    m_nTuplesCount = 0;
    m_eLastFetchType = EFTYP_NONE;

    ((IpgConnection*)m_pISAConnection)->StartTransactionIndirectly();

    SAString sOriginalStmst = m_pCommand->CommandType() == SA_CmdStoredProc ?
    m_sProcCmd : m_pCommand->CommandText();
    SAString sBoundStmt;
    // change ':' param markers to bound values
    size_t nPos = 0l;
    for (int i = 0; i < nPlaceHolderCount; ++i)
    {
        SAParam &Param = *ppPlaceHolders[i]->getParam();

        sBoundStmt += sOriginalStmst.Mid(nPos, ppPlaceHolders[i]->getStart() - nPos);

        if (Param.isNull())
        {
            sBoundStmt += _TSA("NULL");
        }
        else
        {
            SAString sBoundValue;
            SAString sTemp;
            switch (Param.DataType())
            {
            case SA_dtUnknown:
                throw SAException(SA_Library_Error, SA_Library_Error_UnknownParameterType, -1, IDS_UNKNOWN_PARAMETER_TYPE, (const SAChar*)Param.Name());
            case SA_dtBool:
                sBoundValue = Param.asBool() ? _TSA("TRUE") : _TSA("FALSE");
                break;
            case SA_dtShort:
                sBoundValue.Format(_TSA("%hd"), Param.asShort());
                break;
            case SA_dtUShort:
                sBoundValue.Format(_TSA("%hu"), Param.asUShort());
                break;
            case SA_dtLong:
                sBoundValue.Format(_TSA("%ld"), Param.asLong());
                break;
            case SA_dtULong:
                sBoundValue.Format(_TSA("%lu"), Param.asULong());
                break;
            case SA_dtDouble:
            {
                sBoundValue.Format(_TSA("%.*g"), 15, Param.asDouble());
                struct lconv *plconv = ::localeconv();
                if (plconv && plconv->decimal_point && *plconv->decimal_point != '.')
                    sBoundValue.Replace(SAString(plconv->decimal_point), _TSA("."));
            }
            break;
            case SA_dtNumeric:
                // use SQLAPI++ locale-unaffected converter (through SANumeric type)
                sBoundValue = Param.asNumeric().operator SAString();
                break;
            case SA_dtDateTime:
                IpgConnection::CnvtDateTimeToInternal(
                    Param.setAsDateTime(), sTemp);
                sBoundValue = _TSA("'");
                sBoundValue += sTemp;
                sBoundValue += _TSA("'");
                break;
            case SA_dtBytes:
            {
                sBoundValue = ((IpgConnection*)m_pISAConnection)->m_bUseEEscape ? _TSA("E'") : _TSA("'");

                char* szByteStr = ((IpgConnection*)m_pISAConnection)->byte2string(
                    (const void*)Param.asBytes(), Param.asBytes().GetBinaryLength());
                sBoundValue += szByteStr;
                if (NULL != g_pgAPI.PQfreemem)
                    g_pgAPI.PQfreemem(szByteStr);
                else
                    ::free(szByteStr);
                sBoundValue += _TSA("'");
            }
            break;
            case SA_dtString: // Quote ' with '
                sTemp = Param.asString();
                sBoundValue = ((IpgConnection*)m_pISAConnection)->m_bUseEEscape ? _TSA("E'") : _TSA("'");
                sBoundValue +=
                    ((IpgConnection*)m_pISAConnection)->EscapeString(
#ifdef SA_UNICODE_WITH_UTF8
                    sTemp.GetUTF8Chars());
#else
                    sTemp.GetMultiByteChars());
#endif
                sBoundValue += _TSA("'");
                break;
            case SA_dtLongChar:
                BindLongChar(Param, sBoundStmt);
                break;
            case SA_dtLongBinary:
                BindLongBinary(Param, sBoundStmt);
                break;
            case SA_dtBLob:
            case SA_dtCLob:
                BindBLob(Param, sBoundStmt);
                break;
            default:
                assert(false);
            }

            switch (Param.DataType())
            {
            case SA_dtLongBinary:
            case SA_dtLongChar:
            case SA_dtBLob:
            case SA_dtCLob:
                break;	// was already written
            default:
                sBoundStmt += sBoundValue;
            }
        }
        nPos = ppPlaceHolders[i]->getEnd() + 1;
    }

    // copy tail
    if (nPos < sOriginalStmst.GetLength())
        sBoundStmt += sOriginalStmst.Mid(nPos);

    pgConnectionHandles *pConH = (pgConnectionHandles *)m_pCommand->Connection()->NativeHandles();

    if (0 == sBoundStmt.Left(6).CompareNoCase(_TSA("SELECT")) &&
        getOptionValue(_TSA("UseCursor"), false) &&
        SA_AutoCommitOff == m_pISAConnection->getSAConnection()->AutoCommit())
    {
        m_sCursor.Format(_TSA("c%08X"), this);

        m_sRowsToPrefetch = m_pCommand->Option(SACMD_PREFETCH_ROWS);
        if (m_sRowsToPrefetch.IsEmpty())
            m_sRowsToPrefetch = _TSA("");

        if (isSetScrollable())
        {
            sBoundStmt = _TSA("DECLARE ") + m_sCursor +
                _TSA(" SCROLL CURSOR FOR ") + sBoundStmt;
            m_sRowsToPrefetch = _TSA(""); // too complex
        }
        else
            sBoundStmt = _TSA("DECLARE ") + m_sCursor +
            _TSA(" NO SCROLL CURSOR FOR ") + sBoundStmt;

    }
    else
        m_sCursor.Empty();

    try
    {
        SACriticalSectionScope scope(&((IpgConnection*)m_pISAConnection)->m_pgExecMutex);
        SA_TRACE_CMD(SAString(sBoundStmt));
        m_handles.res = g_pgAPI.PQexec(pConH->conn,
#ifdef SA_UNICODE_WITH_UTF8
            sBoundStmt.GetUTF8Chars());
#else
            sBoundStmt.GetMultiByteChars());
#endif
        Check(m_handles.res);

        if (!m_sCursor.IsEmpty())
        {
            if (m_handles.res)
                g_pgAPI.PQclear(m_handles.res);

            sBoundStmt = _TSA("FETCH ") + m_sRowsToPrefetch + _TSA(" FROM ") + m_sCursor;
            SA_TRACE_CMD(SAString(sBoundStmt));
            m_handles.res = g_pgAPI.PQexec(pConH->conn, sBoundStmt.GetMultiByteChars());
            Check(m_handles.res);
        }
    }
    catch (SAException &)  // clean-up
    {
        if (m_handles.res)
        {
            g_pgAPI.PQclear(m_handles.res);
            m_handles.res = NULL;
        }

        m_sCursor.Empty();

        throw;
    }


    if (NULL != m_handles.res &&
        PGRES_TUPLES_OK == g_pgAPI.PQresultStatus(m_handles.res))
    {
        m_nTuplesCount = g_pgAPI.PQntuples(m_handles.res);
        if (m_pCommand->CommandType() == SA_CmdStoredProc && 1 <= m_nTuplesCount)
        {
            // convert output params
            ConvertPGTupleToParams(0);
            g_pgAPI.PQclear(m_handles.res);
            m_handles.res = NULL;
        }
        else
            m_bResultSetCanBe = true;
    }
    else
        m_bResultSetCanBe = false;
}

void IpgCursor::BindLongChar(SAParam &Param, SAString &sBoundStmt)
{
    sBoundStmt += ((IpgConnection*)m_pISAConnection)->m_bUseEEscape ? _TSA("E'") : _TSA("'");
    SAString sTemp;

    size_t nActualWrite;
    SAPieceType_t ePieceType = SA_FirstPiece;
    void *pBuf;

    while ((nActualWrite = Param.InvokeWriter(
        ePieceType, IpgConnection::MaxLongPiece, pBuf)) != 0)
    {
        sTemp += SAString((const void *)pBuf, nActualWrite);
        if (ePieceType == SA_LastPiece)
            break;
    }

    sBoundStmt += ((IpgConnection*)m_pISAConnection)->EscapeString(
#ifdef SA_UNICODE_WITH_UTF8
        sTemp.GetUTF8Chars());
#else
        sTemp.GetMultiByteChars());
#endif
    sBoundStmt += _TSA("'");
}

void IpgCursor::BindLongBinary(SAParam &Param, SAString &sBoundStmt)
{
    sBoundStmt += ((IpgConnection*)m_pISAConnection)->m_bUseEEscape ? _TSA("E'") : _TSA("'");

    size_t nActualWrite;
    SAPieceType_t ePieceType = SA_FirstPiece;
    void *pBuf;
    bool bSkipStart = false;
    while ((nActualWrite = Param.InvokeWriter(
        ePieceType, IpgConnection::MaxLongPiece, pBuf)) != 0)
    {
        char* szByteStr = ((IpgConnection*)m_pISAConnection)->byte2string(pBuf, nActualWrite);

        if (bSkipStart &&
            ((IpgConnection*)m_pISAConnection)->m_nServerVersion >= 90000 &&
            (0 == memcmp(szByteStr, "\\x", 2) || 0 == memcmp(szByteStr, "\\\\x", 3)))
            sBoundStmt += SAString(szByteStr + (0 == memcmp(szByteStr, "\\x", 2) ? 2 : 3));
        else
            sBoundStmt += SAString(szByteStr);

        if (NULL != g_pgAPI.PQfreemem)
            g_pgAPI.PQfreemem(szByteStr);
        else
            ::free(szByteStr);

        if (ePieceType == SA_LastPiece)
            break;

        bSkipStart = true;
    }

    sBoundStmt += _TSA("'");
}

void IpgCursor::BindBLob(SAParam &Param, SAString &sBoundStmt)
{
    size_t nActualWrite;
    SAPieceType_t ePieceType = SA_FirstPiece;
    void *pBuf;
    pgConnectionHandles *pConH = (pgConnectionHandles *)m_pCommand->Connection()->NativeHandles();
    SAConnection *pSAConnection = m_pCommand->Connection();

    // Start transaction if not started
    if (pSAConnection->AutoCommit() != SA_AutoCommitOff)
        ((IpgConnection*)m_pISAConnection)->ExecuteImmediate("BEGIN");

    Oid blobOid = g_pgAPI.lo_creat(pConH->conn, INV_READ | INV_WRITE);
    int fd = g_pgAPI.lo_open(pConH->conn, blobOid, INV_WRITE);

    while ((nActualWrite = Param.InvokeWriter(
        ePieceType, IpgConnection::MaxBlobPiece, pBuf)) != 0)
    {
        char *pWriteBuf = (char*)pBuf;
        size_t nToWrite = nActualWrite;
        int nWritten;

        do
        {
            if ((nWritten = g_pgAPI.lo_write(pConH->conn, fd, pWriteBuf, nToWrite)) < 0)
            {
                g_pgAPI.lo_close(pConH->conn, fd);

                // Rollback transaction, if previos start it
                if (pSAConnection->AutoCommit() != SA_AutoCommitOff)
                    ((IpgConnection*)m_pISAConnection)->ExecuteImmediate("ROLLBACK");

                throw SAException(SA_Library_Error, -1, -1, _TSA("lo_write -> negative number"));
            }

            nToWrite -= nWritten;
            pWriteBuf += nWritten;
        } while (nToWrite > 0);

        if (ePieceType == SA_LastPiece)
            break;
    }

    g_pgAPI.lo_close(pConH->conn, fd);

    // End transaction, if we have previously started it
    if (pSAConnection->AutoCommit() != SA_AutoCommitOff)
        ((IpgConnection*)m_pISAConnection)->ExecuteImmediate("END");

    SAString sBlobOid;
    sBlobOid.Format(_TSA("%d"), blobOid);
    sBoundStmt += sBlobOid;
}

SAString IpgConnection::EscapeString(const char* szString)
{
    SAString sReturn;

    if (NULL != g_pgAPI.PQescapeStringConn)
    {
        assert(m_handles.conn != NULL);

        int error = 0;
        size_t len = strlen(szString);
        char* szTo = (char*)sa_malloc(len * 2 + 1);

        len = g_pgAPI.PQescapeStringConn(m_handles.conn, szTo, szString, len, &error);
        if (0 == error)
#ifdef SA_UNICODE_WITH_UTF8
            sReturn.SetUTF8Chars(szTo);
#else
            sReturn = szTo;
#endif
        ::free(szTo);
    }
    else if (NULL != g_pgAPI.PQescapeString)
    {
        size_t len = strlen(szString);
        char* szTo = (char*)sa_malloc(len * 2 + 1);

        len = g_pgAPI.PQescapeString(szTo, szString, len);
#ifdef SA_UNICODE_WITH_UTF8
        sReturn.SetUTF8Chars(szTo);
#else
        sReturn = szTo;
#endif
        ::free(szTo);
    }
    else
    {
#ifdef SA_UNICODE_WITH_UTF8
        sReturn.SetUTF8Chars(szString);
#else
        sReturn = szString;
#endif
        sReturn.Replace(_TSA("\\"), _TSA("\\\\"));
        sReturn.Replace(_TSA("'"), _TSA("\\'"));
    }

    return sReturn;
}

#define VAL(CH)			((CH) - '0')
#define DIG(VAL)		((VAL) + '0')

char* IpgConnection::byte2string(const void* pByte, size_t nBufLen)
{
    if (NULL != g_pgAPI.PQescapeByteaConn)
    {
        assert(m_handles.conn != NULL);

        size_t from_length = nBufLen, to_length = 0l;
        return (char*)g_pgAPI.PQescapeByteaConn(m_handles.conn,
            (const unsigned char*)pByte, from_length, &to_length);
    }
    else if (NULL != g_pgAPI.PQescapeBytea)
    {
        size_t from_length = nBufLen, to_length = 0l;
        return (char*)g_pgAPI.PQescapeBytea(
            (const unsigned char*)pByte, from_length, &to_length);
    }

    // Non-printable and '\' characters are inserted as '\nnn' (octal)
    // and ''' as '\''.

    size_t i, len = 1;
    char *vp = (char*)pByte;

    for (i = nBufLen; i != 0; i--, vp++)
    {
        if (*vp == '\'')
            len += 2;
        else if (*vp == 0)
            len += 5;
        else if (isprint((unsigned char)*vp) && *vp != '\\')
            len++;
        else
            len += 4;
    }

    char *result = (char*)sa_malloc(len);

    char *rp = result;
    vp = (char*)pByte;

    int val;			/* holds unprintable chars */
    for (i = nBufLen; i != 0; i--, vp++)
    {
        if (*vp == '\'')
        {
            *rp++ = '\\';
            *rp++ = *vp;
        }
        else if (*vp == 0)
        {
            *rp++ = '\\';
            *rp++ = '\\';
            *rp++ = '0';
            *rp++ = '0';
            *rp++ = '0';
        }
        else if (*vp == '\\')
        {
            *rp++ = '\\';
            *rp++ = '\\';
            *rp++ = '\\';
            *rp++ = '\\';
        }
        else if (isprint((unsigned char)*vp))
            *rp++ = *vp;
        else
        {
            val = *vp;
            rp[0] = '\\';
            rp[3] = (char)DIG(val & 07);
            val >>= 3;
            rp[2] = (char)DIG(val & 07);
            val >>= 3;
            rp[1] = (char)DIG(val & 03);
            rp += 4;
        }
    }

    *rp = '\0';
    return result;
}

/*static*/
void* IpgConnection::string2byte(const char* szInputText, size_t &nOutBufLen)
{
    if (NULL != g_pgAPI.PQunescapeBytea)
    {
        const unsigned char *szFrom = (const unsigned char*)szInputText;
        unsigned char *rval = g_pgAPI.PQunescapeBytea(szFrom, &nOutBufLen);
        return rval;
    }

    char *tp = (char*)szInputText;

    for (nOutBufLen = 0; *tp != '\0'; nOutBufLen++)
    {
        if (*tp++ == '\\')
        {
            if (*tp == '\\')
                tp++;
            else if (!isdigit((unsigned char)*tp++)
                || !isdigit((unsigned char)*tp++)
                || !isdigit((unsigned char)*tp++))
                throw SAException(SA_Library_Error, -1, -1, _TSA("Bad input string for type bytea"));
        }
    }

    tp = (char*)szInputText;
    void *result = sa_malloc(nOutBufLen);
    char *rp = (char*)result;
    int byte;

    while (*tp != '\0')
    {
        if (*tp != '\\' || *++tp == '\\')
            *rp++ = *tp++;
        else
        {
            byte = VAL(*tp++);
            byte <<= 3;
            byte += VAL(*tp++);
            byte <<= 3;
            *rp++ = (char)(byte + VAL(*tp++));
        }
    }

    return result;
}

/*virtual */
void IpgCursor::Cancel()
{
    pgConnectionHandles *pConH = (pgConnectionHandles *)m_pCommand->Connection()->NativeHandles();
    SAString sMsg;

    if (NULL != g_pgAPI.PQgetCancel)
    {
        PGcancel* cancel = g_pgAPI.PQgetCancel(pConH->conn);
        if (NULL != cancel)
        {
            char errbuf[256];
            if (!g_pgAPI.PQcancel(cancel, errbuf, 255))
            {
#ifdef SA_UNICODE_WITH_UTF8
                sMsg.SetUTF8Chars(errbuf);
#else
                sMsg = errbuf;
#endif
                g_pgAPI.PQfreeCancel(cancel);
                throw SAException(SA_DBMS_API_Error, 0, -1, sMsg);
            }
            g_pgAPI.PQfreeCancel(cancel);
        }
    }
    else if (!g_pgAPI.PQrequestCancel(pConH->conn))
    {
#ifdef SA_UNICODE_WITH_UTF8
        sMsg.SetUTF8Chars(g_pgAPI.PQerrorMessage(pConH->conn));
#else
        sMsg = g_pgAPI.PQerrorMessage(pConH->conn);
#endif
        throw SAException(SA_DBMS_API_Error, 0, -1, sMsg);
    }
}

/*virtual */
bool IpgCursor::ResultSetExists()
{
    if (!m_bResultSetCanBe)
        return false;

    if (m_handles.res)
        return (g_pgAPI.PQresultStatus(m_handles.res) == PGRES_TUPLES_OK);
    else
        return false;
}

/*virtual */
void IpgCursor::DescribeFields(
    DescribeFields_cb_t fn)
{
    SAString s = m_pCommand->Option(_TSA("OidTypeInterpretation"));
    bool bOidAsBlob = s.CompareNoCase(_TSA("LargeObject")) == 0;

    int cFields = g_pgAPI.PQnfields(m_handles.res);

    for (int iField = 0; iField < cFields; ++iField)
    {
        Oid NativeType = g_pgAPI.PQftype(m_handles.res, iField);
        int Length = g_pgAPI.PQfsize(m_handles.res, iField);
        int Mod = g_pgAPI.PQfmod(m_handles.res, iField);
        int Format = g_pgAPI.PQfformat ? g_pgAPI.PQfformat(m_handles.res, iField) : 0;
        int Prec = 0, Scale = 0;

        SADataType_t type = IpgConnection::CnvtNativeToStd(
            NativeType, Mod, Format, Length, Prec, Scale, bOidAsBlob);

        SAString sName;
#ifdef SA_UNICODE_WITH_UTF8
        sName.SetUTF8Chars(g_pgAPI.PQfname(m_handles.res, iField));
#else
        sName = g_pgAPI.PQfname(m_handles.res, iField);
#endif
        (m_pCommand->*fn)(
            sName,
            type,
            NativeType,
            Length,
            Prec,
            Scale,
            false,
            cFields);
    }
}

/*virtual */
void IpgCursor::SetSelectBuffers()
{
    // do nothing
}

/*virtual */
void IpgCursor::SetFieldBuffer(
    int/* nCol*/,	// 1-based
    void * /*pInd*/,
    size_t/* nIndSize*/,
    void * /*pSize*/,
    size_t/* nSizeSize*/,
    void * /*pValue*/,
    size_t/* nValueSize*/)
{
    assert(false);	// we do not use standard allocators
}

void IpgCursor::ConvertPGTupleToValue(int nTuple, int iField, SADataType_t eType, Oid nNativeType, ValueType_t eValueType, SAValueRead& value)
{
    if (g_pgAPI.PQgetisnull(m_handles.res, nTuple, iField))
    {
        *value.m_pbNull = true;
        return;
    }
    *value.m_pbNull = false;

    char *sValue = g_pgAPI.PQgetvalue(m_handles.res, nTuple, iField);
    int nRealSize = g_pgAPI.PQgetlength(m_handles.res, nTuple, iField);
    char *sValueCopy;

    if (g_pgAPI.PQbinaryTuples(m_handles.res))
    {
        value.m_eDataType = SA_dtBytes;
        *value.m_pString = SAString((void*)sValue, nRealSize);
        return;
    }

    switch (eType)
    {
    case SA_dtUnknown:
        throw SAException(SA_Library_Error, SA_Library_Error_UnknownDataType, -1, IDS_UNKNOWN_DATA_TYPE);
    case SA_dtBool: // Output as 't' or 'f' char
        value.m_eDataType = SA_dtBool;
        if (sValue[0] == 't')
            *(bool*)value.m_pScalar = true;
        else if (sValue[0] == 'f')
            *(bool*)value.m_pScalar = false;
        else
            assert(false);
        break;
    case SA_dtShort:
        value.m_eDataType = SA_dtShort;
        *(short*)value.m_pScalar = (short)atol(sValue);
        break;
    case SA_dtUShort:
        value.m_eDataType = SA_dtUShort;
        *(unsigned short*)value.m_pScalar = (unsigned short)strtoul(sValue, NULL, 10);
        break;
    case SA_dtLong:
        value.m_eDataType = SA_dtLong;
        *(long*)value.m_pScalar = atol(sValue);
        break;
    case SA_dtULong:
        value.m_eDataType = SA_dtULong;
        *(unsigned long*)value.m_pScalar = strtoul(sValue, NULL, 10);
        break;
    case SA_dtDouble:
        value.m_eDataType = SA_dtDouble;

        sValueCopy = (char*)sa_malloc(nRealSize + 1);
        if (nNativeType == CASHOID) // $d,ddd.cc, money
        {
            size_t i, out = 0l;
            for (i = 0; i < strlen(sValue); ++i)
            {
                if (sValue[i] == '$' || sValue[i] == ',' || sValue[i] == ')')
                    ;					/* skip these characters */
                else if (sValue[i] == '(')
                    sValueCopy[out++] = '-';
                else
                    sValueCopy[out++] = sValue[i];
            }
            sValueCopy[out] = '\0';
        }
        else
            strcpy(sValueCopy, sValue);

        char *pEnd;
        *(double*)value.m_pScalar = strtod(sValueCopy, &pEnd);
        // strtod is locale dependent, so if it fails,
        // replace the decimal point and retry
        if (*pEnd != 0)
        {
            struct lconv *plconv = ::localeconv();
            if (plconv && plconv->decimal_point)
            {
                *pEnd = *plconv->decimal_point;
                *(double*)value.m_pScalar = strtod(sValueCopy, &pEnd);
            }
        }

        ::free(sValueCopy);
        break;
    case SA_dtNumeric:
        value.m_eDataType = SA_dtNumeric;
        IpgConnection::CnvtInternalToNumeric(
            *value.m_pNumeric,
            sValue);
        break;
    case SA_dtDateTime:
        value.m_eDataType = SA_dtDateTime;
        IpgConnection::CnvtInternalToDateTime(
            *value.m_pDateTime,
            sValue);
        break;
    case SA_dtString:
        value.m_eDataType = SA_dtString;
#ifdef SA_UNICODE_WITH_UTF8
        value.m_pString->SetUTF8Chars(sValue, nRealSize);
#else
        *value.m_pString =
            SAString(sValue, nRealSize);
#endif
        break;
    case SA_dtBytes:
    {
        value.m_eDataType = SA_dtBytes;
        size_t nByteSize = 0l;
        void* pBytes = IpgConnection::string2byte(sValue, nByteSize);
        *value.m_pString = SAString(pBytes, nByteSize);
        if (NULL != g_pgAPI.PQfreemem)
            g_pgAPI.PQfreemem(pBytes);
        else
            ::free(pBytes);
    }
    break;
    case SA_dtLongBinary:
        value.m_eDataType = SA_dtLongBinary;
        *(int*)value.m_pScalar = nTuple; // Pass tuple id
        break;
    case SA_dtLongChar:
        value.m_eDataType = SA_dtLongChar;
        *(int*)value.m_pScalar = nTuple; // Pass tuple id
        break;
    case SA_dtBLob:
        value.m_eDataType = SA_dtBLob;
        *(Oid*)value.m_pScalar = (Oid)strtoul(sValue, NULL, 10); // Pass Oid of the large object (should be LOB!!!) as long
        break;
    case SA_dtCLob:
        value.m_eDataType = SA_dtCLob;
        *(Oid*)value.m_pScalar = (Oid)strtoul(sValue, NULL, 10); // Pass Oid of the large object (should be LOB!!!) as long
        break;
    default:
        assert(false);	// unknown type
    }

    if (isLongOrLob(eType))
        ConvertLongOrLOB(eValueType, value, NULL, 0);
}

void IpgCursor::ConvertPGTupleToParams(int nTuple)
{
    SAString sProcName;
    size_t n = m_pCommand->CommandText().ReverseFind(_TSA('.'));
    if (SIZE_MAX == n)
        sProcName = m_pCommand->CommandText();
    else
        sProcName = m_pCommand->CommandText().Mid(n + 1);

    for (int iParam = 0; iParam < m_pCommand->ParamCount(); ++iParam)
    {
        SAParam& Param = m_pCommand->ParamByIndex(iParam);
        SADataType_t eParamType = Param.ParamType();

        int iField = g_pgAPI.PQfnumber(m_handles.res, SA_ParamReturn == Param.ParamDirType() ?
            sProcName.GetMultiByteChars() : Param.Name().GetMultiByteChars());

        if (iField >= 0)
            ConvertPGTupleToValue(nTuple, iField, eParamType, Param.ParamNativeType(), ISA_ParamValue, Param);
    }
}

void IpgCursor::ConvertPGTupleToFields(int nTuple)
{
    int cFields = m_pCommand->FieldCount();
    for (int iField = 0; iField < cFields; ++iField)
    {
        SAField &Field = m_pCommand->Field(iField + 1);
        SADataType_t eFieldType = Field.FieldType();

        ConvertPGTupleToValue(nTuple, iField, eFieldType, Field.FieldNativeType(), ISA_FieldValue, Field);
    }
}

/*virtual */
bool IpgCursor::FetchNext()
{
    if (EFTYP_PRIOR == m_eLastFetchType ||
        EFTYP_LAST == m_eLastFetchType)
    {
        m_bResultSetCanBe = true;
        ++m_nCurrentTuple;
    }

    if (m_nCurrentTuple < m_nTuplesCount)
        ConvertPGTupleToFields(m_nCurrentTuple++);
    else if (!m_sCursor.IsEmpty())
    {
        SAString sBoundStmt = _TSA("FETCH FORWARD ") + m_sRowsToPrefetch + _TSA(" FROM ") + m_sCursor;
        pgConnectionHandles *pConH = (pgConnectionHandles *)m_pCommand->Connection()->NativeHandles();

        SACriticalSectionScope scope(&((IpgConnection*)m_pISAConnection)->m_pgExecMutex);
        if (m_handles.res)
        {
            g_pgAPI.PQclear(m_handles.res);
            m_handles.res = NULL;
        }
        SA_TRACE_CMD(SAString(sBoundStmt));
        m_handles.res = g_pgAPI.PQexec(pConH->conn, sBoundStmt.GetMultiByteChars());
        Check(m_handles.res);

        if (PGRES_TUPLES_OK == g_pgAPI.PQresultStatus(m_handles.res))
        {
            m_nTuplesCount = g_pgAPI.PQntuples(m_handles.res);
            m_nCurrentTuple = 0;
            if (m_nCurrentTuple < m_nTuplesCount)
                ConvertPGTupleToFields(m_nCurrentTuple++);
            else
                m_bResultSetCanBe = false;
        }
        else
            m_bResultSetCanBe = false;
    }
    else
    {
        m_nCurrentTuple = m_nTuplesCount + 1;
        m_bResultSetCanBe = false;
    }

    m_eLastFetchType = EFTYP_NEXT;
    return m_bResultSetCanBe;
}

/*virtual */
bool IpgCursor::FetchPrior()
{
    if (EFTYP_NEXT == m_eLastFetchType ||
        EFTYP_FIRST == m_eLastFetchType)
    {
        m_bResultSetCanBe = true;
        --m_nCurrentTuple;
    }

    if (m_nCurrentTuple > 0)
        ConvertPGTupleToFields(--m_nCurrentTuple);
    else if (!m_sCursor.IsEmpty())
    {
        SAString sBoundStmt = _TSA("FETCH BACKWARD ") + m_sRowsToPrefetch + _TSA(" FROM ") + m_sCursor;
        pgConnectionHandles *pConH = (pgConnectionHandles *)m_pCommand->Connection()->NativeHandles();

        SACriticalSectionScope scope(&((IpgConnection*)m_pISAConnection)->m_pgExecMutex);
        if (m_handles.res)
        {
            g_pgAPI.PQclear(m_handles.res);
            m_handles.res = NULL;
        }
        SA_TRACE_CMD(SAString(sBoundStmt));
        m_handles.res = g_pgAPI.PQexec(pConH->conn, sBoundStmt.GetMultiByteChars());
        Check(m_handles.res);

        m_bResultSetCanBe = true;
        m_nCurrentTuple = 0; // first tuple

        if (PGRES_TUPLES_OK == g_pgAPI.PQresultStatus(m_handles.res))
        {
            m_nCurrentTuple = m_nTuplesCount = g_pgAPI.PQntuples(m_handles.res);
            if (m_nCurrentTuple > 0)
                ConvertPGTupleToFields(--m_nCurrentTuple);
            else
                m_bResultSetCanBe = false;
        }
        else
            m_bResultSetCanBe = false;
    }
    else
    {
        m_nCurrentTuple = -1;
        m_bResultSetCanBe = false;
    }

    m_eLastFetchType = EFTYP_PRIOR;
    return m_bResultSetCanBe;
}

/*virtual */
bool IpgCursor::FetchFirst()
{
    if (!m_sCursor.IsEmpty())
    {
        SAString sBoundStmt = _TSA("FETCH FIRST FROM ") + m_sCursor;
        pgConnectionHandles *pConH = (pgConnectionHandles *)m_pCommand->Connection()->NativeHandles();

        SACriticalSectionScope scope(&((IpgConnection*)m_pISAConnection)->m_pgExecMutex);
        if (m_handles.res)
        {
            g_pgAPI.PQclear(m_handles.res);
            m_handles.res = NULL;
        }
        SA_TRACE_CMD(SAString(sBoundStmt));
        m_handles.res = g_pgAPI.PQexec(pConH->conn, sBoundStmt.GetMultiByteChars());
        Check(m_handles.res);

        m_bResultSetCanBe = true;
        m_nCurrentTuple = 0; // first tuple

        if (PGRES_TUPLES_OK == g_pgAPI.PQresultStatus(m_handles.res))
        {
            m_nTuplesCount = g_pgAPI.PQntuples(m_handles.res);
            if (m_nCurrentTuple < m_nTuplesCount)
                ConvertPGTupleToFields(m_nCurrentTuple++);
            else
                m_bResultSetCanBe = false;
        }
        else
            m_bResultSetCanBe = false;
    }
    else
    {
        m_bResultSetCanBe = true;
        m_nCurrentTuple = 0; // first tuple

        if (m_nCurrentTuple < m_nTuplesCount)
            ConvertPGTupleToFields(m_nCurrentTuple++);
        else
            m_bResultSetCanBe = false;
    }

    m_eLastFetchType = EFTYP_FIRST;
    return m_bResultSetCanBe;
}

/*virtual */
bool IpgCursor::FetchLast()
{
    if (!m_sCursor.IsEmpty())
    {
        SAString sBoundStmt = _TSA("FETCH LAST FROM ") + m_sCursor;
        pgConnectionHandles *pConH = (pgConnectionHandles *)m_pCommand->Connection()->NativeHandles();

        SACriticalSectionScope scope(&((IpgConnection*)m_pISAConnection)->m_pgExecMutex);
        SA_TRACE_CMD(SAString(sBoundStmt));
        if (m_handles.res)
        {
            g_pgAPI.PQclear(m_handles.res);
            m_handles.res = NULL;
        }
        m_handles.res = g_pgAPI.PQexec(pConH->conn, sBoundStmt.GetMultiByteChars());
        Check(m_handles.res);

        m_bResultSetCanBe = true;
        m_nCurrentTuple = 0; // first tuple

        if (PGRES_TUPLES_OK == g_pgAPI.PQresultStatus(m_handles.res))
        {
            m_nCurrentTuple = m_nTuplesCount = g_pgAPI.PQntuples(m_handles.res);
            if (m_nCurrentTuple > 0)
                ConvertPGTupleToFields(--m_nCurrentTuple);
            else
                m_bResultSetCanBe = false;
        }
        else
            m_bResultSetCanBe = false;
    }
    else
    {
        m_bResultSetCanBe = true;
        m_nCurrentTuple = m_nTuplesCount; // last tuple

        if (m_nCurrentTuple > 0)
            ConvertPGTupleToFields(--m_nCurrentTuple);
        else
            m_bResultSetCanBe = false;
    }

    m_eLastFetchType = EFTYP_LAST;
    return m_bResultSetCanBe;
}

/*virtual */
bool IpgCursor::FetchPos(int offset, bool Relative/* = false*/)
{
    if (!m_sCursor.IsEmpty())
    {
        SAString sBoundStmt;
        sBoundStmt.Format(SAString(Relative ? _TSA("MOVE RELATIVE %d FROM ") : _TSA("MOVE ABSOLUTE %d FROM ")) + m_sCursor,
            offset >= 0 ? (offset - 1) : (offset + 1));
        pgConnectionHandles *pConH = (pgConnectionHandles *)m_pCommand->Connection()->NativeHandles();

        SACriticalSectionScope scope(&((IpgConnection*)m_pISAConnection)->m_pgExecMutex);
        SA_TRACE_CMD(SAString(sBoundStmt));
        if (m_handles.res)
        {
            g_pgAPI.PQclear(m_handles.res);
            m_handles.res = NULL;
        }
        m_handles.res = g_pgAPI.PQexec(pConH->conn, sBoundStmt.GetMultiByteChars());
        Check(m_handles.res);

        m_bResultSetCanBe = true;
        m_nCurrentTuple = m_nTuplesCount = 0; // first tuple

        if (offset >= 0)
            FetchNext();
        else
            FetchPrior();
    }
    else
    {
        m_bResultSetCanBe = true;

        if (EFTYP_NEXT == m_eLastFetchType || EFTYP_FIRST == m_eLastFetchType)
        {
            if (offset > 0)
                --m_nCurrentTuple;
        }
        else
        {
            if (offset < 0)
                m_nCurrentTuple++;
        }

        if (Relative)
            m_nCurrentTuple += offset;
        else
            m_nCurrentTuple = offset >= 0 ? offset : (m_nTuplesCount + offset);

        if (m_nCurrentTuple >= 0 && m_nCurrentTuple < m_nTuplesCount)
        {
            if (offset >= 0)
                (ConvertPGTupleToFields(m_nCurrentTuple++));
            else
                ConvertPGTupleToFields(--m_nCurrentTuple);
        }
        else
            m_bResultSetCanBe = false;
    }

    m_eLastFetchType = offset > 0 ? EFTYP_NEXT : EFTYP_PRIOR;
    return m_bResultSetCanBe;
}

/*virtual */
long IpgCursor::GetRowsAffected()
{
    return (long)atoi(g_pgAPI.PQcmdTuples(m_handles.res));
}

/*virtual */
void IpgCursor::ReadLongOrLOB(
    ValueType_t eValueType,
    SAValueRead &vr,
    void *pValue,
    size_t nBufSize,
    saLongOrLobReader_t fnReader,
    size_t nReaderWantedPieceSize,
    void *pAddlData)
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

    switch (eDataType)
    {
    case SA_dtLongBinary:
        ReadLongBinary(eValueType, vr, pValue, nBufSize, fnReader, nReaderWantedPieceSize, pAddlData);
        break;
    case SA_dtLongChar:
        ReadLongChar(eValueType, vr, pValue, nBufSize, fnReader, nReaderWantedPieceSize, pAddlData);
        break;
    case SA_dtBLob:
    case SA_dtCLob:
        ReadBLOB(eValueType, vr, pValue, nBufSize, fnReader, nReaderWantedPieceSize, pAddlData);
        break;
    default:
        assert(false);
    }
}

void IpgCursor::ReadLongBinary(
    ValueType_t/* eValueType*/,
    SAValueRead &vr,
    void * /*pValue*/,
    size_t/* nFieldBufSize*/,
    saLongOrLobReader_t fnReader,
    size_t nReaderWantedPieceSize,
    void *pAddlData)
{
    int iField = ((SAField &)vr).Pos() - 1;
    int nTuple = *((int*)vr.m_pScalar);

    // get long data and size
    size_t nRealSize;
    char *pData = (char*)IpgConnection::string2byte(
        g_pgAPI.PQgetvalue(m_handles.res, nTuple, iField), nRealSize);
    size_t nLongSize = nRealSize;

    unsigned char* pBuf;
    size_t nPieceSize = vr.PrepareReader(
        nLongSize,
        IpgConnection::MaxLongPiece,
        pBuf,
        fnReader,
        nReaderWantedPieceSize,
        pAddlData);
    assert(nPieceSize <= IpgConnection::MaxLongPiece);

    SAPieceType_t ePieceType = SA_FirstPiece;
    size_t nTotalRead = 0l;
    do
    {
        nPieceSize =
            sa_min(nPieceSize, (nLongSize - nTotalRead));

        memcpy(pBuf, pData + nTotalRead, nPieceSize);
        size_t actual_read = nPieceSize;

        nTotalRead += actual_read;
        assert(nTotalRead <= nLongSize);

        if (nTotalRead == nLongSize)
        {
            if (ePieceType == SA_NextPiece)
                ePieceType = SA_LastPiece;
            else    // the whole Long was read in one piece
            {
                assert(ePieceType == SA_FirstPiece);
                ePieceType = SA_OnePiece;
            }
        }
        vr.InvokeReader(ePieceType, pBuf, actual_read);

        if (ePieceType == SA_FirstPiece)
            ePieceType = SA_NextPiece;
    } while (nTotalRead < nLongSize);

    if (NULL != g_pgAPI.PQfreemem)
        g_pgAPI.PQfreemem(pData);
    else
        ::free(pData);
}

void IpgCursor::ReadLongChar(
    ValueType_t eValueType,
    SAValueRead &vr,
    void * /*pValue*/,
    size_t/* nFieldBufSize*/,
    saLongOrLobReader_t fnReader,
    size_t nReaderWantedPieceSize,
    void *pAddlData)
{
    int iField = eValueType == ISA_ParamValue ? OutputParamPos(&((SAParam &)vr)) : ((SAField &)vr).Pos() - 1;
    int nTuple = *((int*)vr.m_pScalar);

    // get long data and size
#ifdef SA_UNICODE_WITH_UTF8
    SAString dataBuffer;
    dataBuffer.SetUTF8Chars(
        g_pgAPI.PQgetvalue(m_handles.res, nTuple, iField),
        g_pgAPI.PQgetlength(m_handles.res, nTuple, iField));
#else
    SAString dataBuffer(g_pgAPI.PQgetvalue(m_handles.res, nTuple, iField),
        g_pgAPI.PQgetlength(m_handles.res, nTuple, iField));
#endif
    size_t nLongSize = dataBuffer.GetBinaryLength();
    const char *pData = (const char*)dataBuffer.GetBinaryBuffer(nLongSize);

    unsigned char* pBuf;
    size_t nPieceSize = vr.PrepareReader(
        nLongSize,
        IpgConnection::MaxLongPiece,
        pBuf,
        fnReader,
        nReaderWantedPieceSize,
        pAddlData);
    assert(nPieceSize <= IpgConnection::MaxLongPiece);

    SAPieceType_t ePieceType = SA_FirstPiece;
    size_t nTotalRead = 0l;
    do
    {
        nPieceSize =
            sa_min(nPieceSize, (nLongSize - nTotalRead));

        memcpy(pBuf, pData + nTotalRead, nPieceSize);
        size_t actual_read = nPieceSize;

        nTotalRead += actual_read;
        assert(nTotalRead <= nLongSize);

        if (nTotalRead == nLongSize)
        {
            if (ePieceType == SA_NextPiece)
                ePieceType = SA_LastPiece;
            else    // the whole Long was read in one piece
            {
                assert(ePieceType == SA_FirstPiece);
                ePieceType = SA_OnePiece;
            }
        }
        vr.InvokeReader(ePieceType, pBuf, actual_read);

        if (ePieceType == SA_FirstPiece)
            ePieceType = SA_NextPiece;
    } while (nTotalRead < nLongSize);

    dataBuffer.ReleaseBinaryBuffer(nLongSize);
}

void IpgCursor::ReadBLOB(
    ValueType_t/* eValueType*/,
    SAValueRead &vr,
    void * /*pValue*/,
    size_t/* nFieldBufSize*/,
    saLongOrLobReader_t fnReader,
    size_t nReaderWantedPieceSize,
    void *pAddlData)
{
    pgConnectionHandles *pConH = (pgConnectionHandles *)m_pCommand->Connection()->NativeHandles();
    Oid blobOid = *((Oid*)vr.m_pScalar);
    SAConnection *pSAConnection = m_pCommand->Connection();

    // Start transaction if not started
    if (pSAConnection->AutoCommit() != SA_AutoCommitOff)
        ((IpgConnection*)m_pISAConnection)->ExecuteImmediate("BEGIN");

    int fd = g_pgAPI.lo_open(pConH->conn, blobOid, INV_READ);
    if (fd < 0)
        throw SAException(SA_Library_Error, -1, -1, _TSA("lo_open -> negative number"));

    size_t nLongSize = 0l;	// no way to determine size before fetch
    unsigned char *pBuf;
    size_t nPieceSize = vr.PrepareReader(
        nLongSize,
        IpgConnection::MaxBlobPiece,
        pBuf,
        fnReader,
        nReaderWantedPieceSize,
        pAddlData);

    assert(nPieceSize <= IpgConnection::MaxBlobPiece);

    SAPieceType_t ePieceType = SA_FirstPiece;
    int nReaded;

    while ((nReaded = g_pgAPI.lo_read(pConH->conn, fd, (char*)pBuf, nPieceSize)) != 0)
    {
        if (nReaded < 0)
        {
            g_pgAPI.lo_close(pConH->conn, fd);

            // Rollback transaction, if we have previously started it
            if (pSAConnection->AutoCommit() != SA_AutoCommitOff)
                ((IpgConnection*)m_pISAConnection)->ExecuteImmediate("ROLLBACK");

            throw SAException(SA_Library_Error, -1, -1, _TSA("lo_read -> negative number"));
        }

        if (ePieceType != SA_FirstPiece)
        {
            if ((size_t)nReaded < nPieceSize)
                ePieceType = SA_LastPiece;
            else
                ePieceType = SA_NextPiece;
        }
        else if ((size_t)nReaded < nPieceSize)
            ePieceType = SA_OnePiece;

        vr.InvokeReader(ePieceType, pBuf, nReaded);

        if (ePieceType == SA_FirstPiece)
            ePieceType = SA_NextPiece;
    }

    g_pgAPI.lo_close(pConH->conn, fd);

    // End transaction, if we have previously started it
    if (pSAConnection->AutoCommit() != SA_AutoCommitOff)
        ((IpgConnection*)m_pISAConnection)->ExecuteImmediate("END");
}

/*virtual */
void IpgCursor::DescribeParamSP()
{
    SAString sCmdText = m_pCommand->CommandText();
    SAString sProcName, sCatalogName, sSchemaName;
    size_t n = sCmdText.ReverseFind(_TSA('.'));
    if (SIZE_MAX == n)
        sProcName = sCmdText;
    else
    {
        sProcName = sCmdText.Mid(n + 1);
        sSchemaName = sCmdText.Mid(0, n);
    }

    SACommand cmd(m_pCommand->Connection());
    if (sSchemaName.IsEmpty())
        cmd.setCommandText(_TSA("select specific_name, specific_schema, pg_catalog.pg_type.oid from information_schema.routines join pg_catalog.pg_type on pg_type.typname = type_udt_name where upper(routine_name) = upper(:1) and (upper(routine_schema) = 'PUBLIC' or upper(routine_schema) = 'PG_CATALOG' or upper(routine_schema) = 'INFORMATION_SCHEMA')"));
    else
    {
        cmd.setCommandText(_TSA("select specific_name, specific_schema, pg_catalog.pg_type.oid from information_schema.routines join pg_catalog.pg_type on pg_type.typname = type_udt_name where upper(routine_name) = upper(:1) and upper(routine_schema) = upper(:2)"));
        cmd.Param(2).setAsString() = sSchemaName;
    }
    cmd.Param(1).setAsString() = sProcName;
    cmd.Execute();
    if (cmd.FetchNext())
    {
        sProcName = cmd[1].asString();
        sSchemaName = cmd[2].asString();

        Oid nNativeType = cmd[3].asULong();
        int len = 0, prec = 0, scale = 0;

        m_pCommand->CreateParam(_TSA("RETURN_VALUE"),
            IpgConnection::CnvtNativeToStd(nNativeType, 0, 0, len, prec, scale, false),
            nNativeType, len, prec, scale, SA_ParamReturn);
    }

    SAString sParamList;

    cmd.setCommandText(_TSA(
        "select parameter_mode, parameter_name, pg_catalog.pg_type.oid \
                                		from information_schema.parameters join pg_catalog.pg_type on pg_type.typname = udt_name \
                                                                                                                		where upper(specific_name) = upper(:1) and upper(specific_schema) = upper(:2) \
                                                                                                                                                                                                                                                                                		order by ordinal_position"));
    cmd.Param(1).setAsString() = sProcName;
    cmd.Param(2).setAsString() = sSchemaName;
    cmd.Execute();
    while (cmd.FetchNext())
    {
        SAString sParamMode = cmd[1].asString();
        SAString sParamName = cmd[2].asString();
        Oid oidType = cmd[3].asULong();

        int len = 0, prec = 0, scale = 0;
        SAParamDirType_t paramDirType = 0 == sParamMode.CompareNoCase(_TSA("IN")) ? SA_ParamInput :
            (0 == sParamMode.CompareNoCase(_TSA("OUT")) ? SA_ParamOutput : SA_ParamInputOutput);

        m_pCommand->CreateParam(sParamName,
            IpgConnection::CnvtNativeToStd(oidType, 0, 0, len, prec, scale, false),
            oidType, len, prec, scale, paramDirType);

        if (SA_ParamOutput != paramDirType)
        {
            if (!sParamList.IsEmpty())
                sParamList += _TSA(",");
            sParamList += _TSA(":") + sParamName;
        }
    }

    m_sProcCmd = _TSA("select * from ") + sCmdText + _TSA("(") + sParamList + _TSA(")");
    m_pCommand->ParseInputMarkers(m_sProcCmd, NULL);
}

/*virtual */
saCommandHandles *IpgCursor::NativeHandles()
{
    return &m_handles;
}

void IpgCursor::OnTransactionClosed(void * /*pAddlData*/)
{
    if (!m_sCursor.IsEmpty())
    {
        m_sCursor.Empty();
        m_pCommand->UnExecute();
    }
}

//////////////////////////////////////////////////////////////////////
// PostgreSQL globals
//////////////////////////////////////////////////////////////////////

ISAConnection *newIpgConnection(SAConnection *pSAConnection)
{
    return new IpgConnection(pSAConnection);
}

/*static */
void IpgConnection::CnvtInternalToNumeric(
    SANumeric &numeric,
    const char *sValue)
{
    numeric = SAString(sValue);
}

/*virtual */
void IpgConnection::CnvtInternalToNumeric(
    SANumeric &/*numeric*/,
    const void * /*pInternal*/,
    int/* nInternalSize*/)
{
    assert(false);	// we do not use ISA... convertors
}
