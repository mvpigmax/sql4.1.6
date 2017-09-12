// myClient.cpp
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <myAPI.h>
#include "myClient.h"

static void Check(MYSQL *mysql)
{
    assert(mysql);

    unsigned int nErr = g_myAPI.mysql_errno(mysql);
    if (nErr)
    {
        SAString sMsg;
#ifdef SA_UNICODE_WITH_UTF8
        sMsg.SetUTF8Chars(g_myAPI.mysql_error(mysql));
#else
        sMsg = SAString(g_myAPI.mysql_error(mysql));
#endif
        throw SAException(SA_DBMS_API_Error, nErr, -1, sMsg);
    }
}

static void Check(MYSQL_STMT *stmt)
{
    assert(stmt);

    unsigned int nErr = g_myAPI.mysql_stmt_errno(stmt);
    if (nErr)
    {
        SAString sMsg;
#ifdef SA_UNICODE_WITH_UTF8
        sMsg.SetUTF8Chars(g_myAPI.mysql_stmt_error(stmt));
#else
        sMsg = SAString(g_myAPI.mysql_stmt_error(stmt));
#endif
        throw SAException(SA_DBMS_API_Error, nErr, -1, sMsg);
    }
}

static SAString Bin2Hex(const void* pBuf, size_t nLenInBytes)
{
    SAString s;
    if (nLenInBytes)
    {
        SAChar *p2 = s.GetBuffer(nLenInBytes * 2);	// 2 characters for each byte
        const void *p = pBuf;
        for (size_t i = 0l; i < nLenInBytes; ++i)
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
}

//////////////////////////////////////////////////////////////////////
// MySQL statement result bind classes
//////////////////////////////////////////////////////////////////////
/* bind structure 4.1.x */
typedef struct st_mysql_bind_4_1
{
    unsigned long	*length;          /* output length pointer */
    my_bool       *is_null;	  /* Pointer to null indicator */
    void		*buffer;	  /* buffer to get/put data */
    enum enum_field_types buffer_type;	/* buffer type */
    unsigned long buffer_length;    /* buffer length, must be set for str/binary */

    /* Following are for internal use. Set by mysql_stmt_bind_param */
    unsigned char *inter_buffer;    /* for the current data position */
    unsigned long offset;           /* offset position for char/binary fetch */
    unsigned long	internal_length;  /* Used if length is 0 */
    unsigned int	param_number;	  /* For null count and error messages */
    unsigned int  pack_length;	  /* Internal length for packed data */
    my_bool       is_unsigned;      /* set if integer type is unsigned */
    my_bool	long_data_used;	  /* If used with mysql_send_long_data */
    my_bool	internal_is_null; /* Used if is_null is 0 */
    void(*store_param_func)(NET *net, struct st_mysql_bind *param);
    void(*fetch_result)(struct st_mysql_bind *, unsigned char **row);
    void(*skip_result)(struct st_mysql_bind *, MYSQL_FIELD *,
        unsigned char **row);
} MYSQL_BIND_4_1;

/* bind structure 5.0.x */
typedef struct st_mysql_bind_5_0
{
    unsigned long	*length;          /* output length pointer */
    my_bool       *is_null;	  /* Pointer to null indicator */
    void		*buffer;	  /* buffer to get/put data */
    /* set this if you want to track data truncations happened during fetch */
    my_bool       *error;
    enum enum_field_types buffer_type;	/* buffer type */
    /* output buffer length, must be set when fetching str/binary */
    unsigned long buffer_length;
    unsigned char *row_ptr;         /* for the current data position */
    unsigned long offset;           /* offset position for char/binary fetch */
    unsigned long	length_value;     /* Used if length is 0 */
    unsigned int	param_number;	  /* For null count and error messages */
    unsigned int  pack_length;	  /* Internal length for packed data */
    my_bool       error_value;      /* used if error is 0 */
    my_bool       is_unsigned;      /* set if integer type is unsigned */
    my_bool	long_data_used;	  /* If used with mysql_send_long_data */
    my_bool	is_null_value;    /* Used if is_null is 0 */
    void(*store_param_func)(NET *net, struct st_mysql_bind *param);
    void(*fetch_result)(struct st_mysql_bind *, MYSQL_FIELD *,
        unsigned char **row);
    void(*skip_result)(struct st_mysql_bind *, MYSQL_FIELD *,
        unsigned char **row);
} MYSQL_BIND_5_0;

class mysql_bind
{
public:
    virtual ~mysql_bind() {};

public:
    static mysql_bind *getInstance(
        long nClientVersion,
        int nColumnCount);

    virtual enum enum_field_types & buffer_type(int nCol) = 0;
    virtual unsigned long * & length(int nCol) = 0;
    virtual my_bool * & is_null(int nCol) = 0;
    virtual void * & buffer(int nCol) = 0;
    virtual my_bool & is_unsigned(int nCol) = 0;
    virtual unsigned long & buffer_length(int nCol) = 0;
    virtual MYSQL_BIND* get_bind() = 0;
    virtual MYSQL_BIND* get_bind(int nCol) = 0;
};

class mysql_bind_4_1 : public mysql_bind
{
public:
    virtual ~mysql_bind_4_1() { free(current); };

public:
    mysql_bind_4_1(int nColumnCount) {
        current = (MYSQL_BIND_4_1 *)sa_malloc(sizeof(MYSQL_BIND_4_1) * nColumnCount);
        memset(current, 0, sizeof(MYSQL_BIND_4_1) * nColumnCount);
    };

    virtual enum enum_field_types & buffer_type(int nCol) { return current[nCol - 1].buffer_type; };
    virtual unsigned long * & length(int nCol) { return current[nCol - 1].length; };
    virtual my_bool * & is_null(int nCol) { return current[nCol - 1].is_null; };
    virtual void * & buffer(int nCol) { return current[nCol - 1].buffer; };
    virtual my_bool & is_unsigned(int nCol) { return current[nCol - 1].is_unsigned; };
    virtual unsigned long & buffer_length(int nCol) { return current[nCol - 1].buffer_length; };
    virtual MYSQL_BIND* get_bind() { return (MYSQL_BIND*)current; };
    virtual MYSQL_BIND* get_bind(int nCol) { return (MYSQL_BIND*)(&current[nCol - 1]); };

private:
    MYSQL_BIND_4_1 *current;
};

class mysql_bind_5_0 : public mysql_bind
{
public:
    virtual ~mysql_bind_5_0() { free(current); };

public:
    mysql_bind_5_0(int nColumnCount) {
        current = (MYSQL_BIND_5_0 *)sa_malloc(sizeof(MYSQL_BIND_5_0) * nColumnCount);
        memset(current, 0, sizeof(MYSQL_BIND_5_0) * nColumnCount);
    };

    virtual enum enum_field_types & buffer_type(int nCol) { return current[nCol - 1].buffer_type; };
    virtual unsigned long * & length(int nCol) { return current[nCol - 1].length; };
    virtual my_bool * & is_null(int nCol) { return current[nCol - 1].is_null; };
    virtual void * & buffer(int nCol) { return current[nCol - 1].buffer; };
    virtual my_bool & is_unsigned(int nCol) { return current[nCol - 1].is_unsigned; };
    virtual unsigned long & buffer_length(int nCol) { return current[nCol - 1].buffer_length; };
    virtual MYSQL_BIND* get_bind() { return (MYSQL_BIND*)current; };
    virtual MYSQL_BIND* get_bind(int nCol) { return (MYSQL_BIND*)(&current[nCol - 1]); };

private:
    MYSQL_BIND_5_0 *current;
};

class mysql_bind_latest : public mysql_bind
{
public:
    virtual ~mysql_bind_latest() { free(current); };

public:
    mysql_bind_latest(int nColumnCount) {
        current = (MYSQL_BIND *)sa_malloc(sizeof(MYSQL_BIND) * nColumnCount);
        memset(current, 0, sizeof(MYSQL_BIND) * nColumnCount);
    };

    virtual enum enum_field_types & buffer_type(int nCol) { return current[nCol - 1].buffer_type; };
    virtual unsigned long * & length(int nCol) { return current[nCol - 1].length; };
    virtual my_bool * & is_null(int nCol) { return current[nCol - 1].is_null; };
    virtual void * & buffer(int nCol) { return current[nCol - 1].buffer; };
    virtual my_bool & is_unsigned(int nCol) { return current[nCol - 1].is_unsigned; };
    virtual unsigned long & buffer_length(int nCol) { return current[nCol - 1].buffer_length; };
    virtual MYSQL_BIND* get_bind() { return (MYSQL_BIND*)current; };
    virtual MYSQL_BIND* get_bind(int nCol) { return (MYSQL_BIND*)(&current[nCol - 1]); };

private:
    MYSQL_BIND *current;
};

/*static */
mysql_bind *mysql_bind::getInstance(
    long nClientVersion,
    int nColumnCount)
{
    bool b_5_1_plus = nClientVersion >= 0x00050001;
    bool b_5_0 = !b_5_1_plus && nClientVersion > 0x00040001;
    bool b_4_1 = !b_5_0 && !b_5_1_plus && nClientVersion >= 0x00040001;

    if (b_4_1)
        return new mysql_bind_4_1(nColumnCount);
    if (b_5_0)
        return new mysql_bind_5_0(nColumnCount);

    return new mysql_bind_latest(nColumnCount);
}

//////////////////////////////////////////////////////////////////////
// ImyConnection Class
//////////////////////////////////////////////////////////////////////

class ImyConnection : public ISAConnection
{
    friend class ImyCursor;

    SAMutex m_myExecMutex;
    myConnectionHandles m_handles;

    enum MaxLong { MaxLongPiece = (unsigned int)SA_DefaultMaxLong };

    static SADataType_t CnvtNativeToStd(
    enum enum_field_types type,
        size_t length,
        int &Prec,	// not taken from MySQL, but calculated in this function
        unsigned int decimals,
        unsigned int flags);
    static enum_field_types CnvtStdToNative(SADataType_t eDataType);

    static void CnvtInternalToNumeric(
        SANumeric &numeric,
        const char *sValue);

    static void CnvtInternalToDateTime(
        SADateTime &date_time,
        const char *sInternal);

    static void CnvtInternalToInterval(
        SAInterval &interval,
        const char *sInternal);

    static void fraction(const char* szFraction, unsigned int& fraction);
    static int second(const char *sSecond);
    static int minute(const char *sMinute);
    static int shortHour(const char *sHour);
    static int day(const char *sDay);
    static int month(const char *sMonth);
    static int longYear(const char *sYear);
    static int shortYear(const char *sYear);
    static void CnvtDateTimeToInternal(
        const SADateTime &date_time,
        SAString &sInternal);
    static void CnvtDateTimeToInternal(
        const SADateTime &date_time,
        MYSQL_TIME &dtInternal);

protected:
    virtual ~ImyConnection();

public:
    ImyConnection(SAConnection *pSAConnection);

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

/*static */
SADataType_t ImyConnection::CnvtNativeToStd(
enum enum_field_types type,
    size_t length,
    int &Prec,	// not taken from MySQL, calculated in this function
    unsigned int decimals,
    unsigned int flags)
{
    SADataType_t eDataType;
    Prec = (int)length;	// can be adjusted

    switch (type)
    {
    case FIELD_TYPE_BIT:
        eDataType = SA_dtBytes;
        break;
    case FIELD_TYPE_TINY:	// TINYINT field
    case FIELD_TYPE_SHORT:	// SMALLINT field
        if (flags & UNSIGNED_FLAG)
            eDataType = SA_dtUShort;
        else
            eDataType = SA_dtShort;
        break;
    case FIELD_TYPE_LONG:	// INTEGER field
    case FIELD_TYPE_INT24:	// MEDIUMINT field
        if (flags & UNSIGNED_FLAG)
            eDataType = SA_dtULong;
        else
            eDataType = SA_dtLong;
        break;
    case FIELD_TYPE_LONGLONG:	// BIGINT field
        eDataType = SA_dtNumeric;
        break;
    case FIELD_TYPE_NEWDECIMAL:
    case FIELD_TYPE_DECIMAL:	// DECIMAL or NUMERIC field
        if (!(flags & UNSIGNED_FLAG))
            --Prec;	// space for '-' sign
        if (decimals == 0)
        {
            // check for exact type
            if (Prec < 5)
                if (flags & UNSIGNED_FLAG)
                    eDataType = SA_dtUShort;
                else
                    eDataType = SA_dtShort;
            else if (Prec < 10)
                if (flags & UNSIGNED_FLAG)
                    eDataType = SA_dtULong;
                else
                    eDataType = SA_dtLong;
            else
                eDataType = SA_dtNumeric;
        }
        else
        {
            --Prec;	// space for '.'
            eDataType = SA_dtNumeric;
        }
        break;
    case FIELD_TYPE_FLOAT:	// FLOAT field
        eDataType = SA_dtDouble;
        break;
    case FIELD_TYPE_DOUBLE:	// DOUBLE or REAL field
        eDataType = SA_dtDouble;
        break;
    case FIELD_TYPE_TIMESTAMP:	// TIMESTAMP field
        eDataType = SA_dtDateTime;
        break;
    case FIELD_TYPE_DATE:	// DATE field
    case FIELD_TYPE_NEWDATE:
        eDataType = SA_dtDateTime;
        break;
    case FIELD_TYPE_TIME:	// TIME field
        eDataType = SA_dtInterval;
        break;
    case FIELD_TYPE_DATETIME:	// DATETIME field
        eDataType = SA_dtDateTime;
        break;
    case FIELD_TYPE_YEAR:	// YEAR field
        eDataType = SA_dtUShort;
        break;
    case FIELD_TYPE_STRING:	// String (CHAR or VARCHAR) field, also SET and ENUM
    case FIELD_TYPE_VAR_STRING:
        if (flags & BINARY_FLAG)
            eDataType = SA_dtBytes;
        else
            eDataType = SA_dtString;
        break;
    case FIELD_TYPE_TINY_BLOB:
    case FIELD_TYPE_MEDIUM_BLOB:
    case FIELD_TYPE_LONG_BLOB:
    case FIELD_TYPE_BLOB:	// BLOB or TEXT field (use max_length to determine the maximum length)
        if (flags & BINARY_FLAG)
            eDataType = SA_dtLongBinary;
        else
            eDataType = SA_dtLongChar;
        break;
    case FIELD_TYPE_SET:	// SET field
        eDataType = SA_dtString;
        break;
    case FIELD_TYPE_ENUM:	// ENUM field
        eDataType = SA_dtString;
        break;
    case FIELD_TYPE_NULL:	// NULL-type field
        eDataType = SA_dtString;
        break;

    default:
        assert(false);
        eDataType = SA_dtString;
    }

    return eDataType;
}

/*static */
enum_field_types ImyConnection::CnvtStdToNative(SADataType_t eDataType)
{
    enum_field_types dbtype = MYSQL_TYPE_STRING;

    switch (eDataType)
    {
    case SA_dtUnknown:
        throw SAException(SA_Library_Error, SA_Library_Error_UnknownDataType, -1, IDS_UNKNOWN_DATA_TYPE);
    case SA_dtBool:
        dbtype = MYSQL_TYPE_TINY;
        break;
    case SA_dtShort:
        dbtype = MYSQL_TYPE_SHORT;
        break;
    case SA_dtUShort:
        dbtype = MYSQL_TYPE_SHORT;
        break;
    case SA_dtLong:
        dbtype = MYSQL_TYPE_LONG;
        break;
    case SA_dtULong:
        dbtype = MYSQL_TYPE_LONG;
        break;
    case SA_dtDouble:
        dbtype = MYSQL_TYPE_DOUBLE;
        break;
    case SA_dtNumeric:
        dbtype = MYSQL_TYPE_NEWDECIMAL;
        break;
    case SA_dtDateTime:
        dbtype = MYSQL_TYPE_DATETIME;
        break;
    case SA_dtString:
        dbtype = MYSQL_TYPE_STRING;
        break;
    case SA_dtBytes:
        dbtype = MYSQL_TYPE_BLOB;
        break;
    case SA_dtLongBinary:
        dbtype = MYSQL_TYPE_BLOB;
        break;
    case SA_dtLongChar:
        dbtype = MYSQL_TYPE_STRING;
        break;
    case SA_dtBLob:
        dbtype = MYSQL_TYPE_BLOB;
        break;
    case SA_dtCLob:
        dbtype = MYSQL_TYPE_STRING;
        break;
    default:
        assert(false);
        break;
    }

    return dbtype;
}

//////////////////////////////////////////////////////////////////////
// ImyConnection Construction/Destruction
//////////////////////////////////////////////////////////////////////

ImyConnection::ImyConnection(
    SAConnection *pSAConnection) : ISAConnection(pSAConnection)
{
    Reset();
}

ImyConnection::~ImyConnection()
{
}

//////////////////////////////////////////////////////////////////////
// ImyClient Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ImyClient::ImyClient()
{

}

ImyClient::~ImyClient()
{

}

ISAConnection *ImyClient::QueryConnectionInterface(
    SAConnection *pSAConnection)
{
    return new ImyConnection(pSAConnection);
}

//////////////////////////////////////////////////////////////////////
// ImyCursor Class
//////////////////////////////////////////////////////////////////////

#define SA_MYSQL_MAX_NUMERIC_LENGTH 72
#define SA_MYSQL_MAX_INTERVAL_LENGTH 11
#define SA_MYSQL_NO_ROWS_AFFECTED (my_ulonglong)(-1)

class ImyCursor : public ISACursor
{
    // private handles
    myCommandHandles	m_handles;
    my_ulonglong cRows;
    my_ulonglong m_currentRow;

    // private handles (OLD API)
    MYSQL_ROW m_mysql_row;
    unsigned long *m_lengths;
    bool m_bResultSetCanBe;

    SAString	m_sOriginalStmst;
    bool		m_bOpened;

    // private handles (statement API)
    mysql_bind *m_bind;
    mysql_bind *m_result;
    static my_bool my_true;
    static my_bool my_false;

    // Old API
    void ConvertMySQLRowToFields();
    void ConvertMySQLFieldToParam(int iField, SAParam& value);
    SAString MySQLEscapeString(const char *sFrom, size_t nLen);
    SAString MySQLEscapeString(const SAString &sValue);
    void BindBLob(SAParam &Param, SAString &sBoundStmt);
    void BindText(SAParam &Param, SAString &sBoundStmt);
    void NextResult();

    // Statement API
    void Bind(
        int nPlaceHolderCount,
        saPlaceHolder **ppPlaceHolders);
    void SendBlob(unsigned int nParam, SAParam &Param);
    void SendClob(unsigned int nParam, SAParam &Param);
    void DescribeFields_Stmt(DescribeFields_cb_t fn);
    void Execute_Stmt(
        int nPlaceHolderCount,
        saPlaceHolder **ppPlaceHolders);
    bool FetchNext_Stmt();
    void ReadLongOrLOB_Stmt(
        ValueType_t eValueType,
        SAValueRead &vr,
        void *pValue,
        size_t nFieldBufSize,
        saLongOrLobReader_t fnReader,
        size_t nReaderWantedPieceSize,
        void *pAddlData);

public:
    ImyCursor(
        ImyConnection *pImyConnection,
        SACommand *pCommand);
    virtual ~ImyCursor();

protected:
    virtual size_t InputBufferSize(
        const SAParam &Param) const;
    virtual size_t OutputBufferSize(
        SADataType_t eDataType,
        size_t nDataSize) const;
    virtual bool ConvertIndicator(
        int nPos,	// 1-based, can be field or param pos, 0-for return value from SP
        int nNotConverted,
        SAValueRead &vr,
        ValueType_t eValueType,
        void *pInd, size_t nIndSize,
        void *pSize, size_t nSizeSize,
        size_t &nRealSize,
        int nBulkReadingBufPos) const;

public:
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

    virtual void ConvertString(
        SAString &String,
        const void *pData,
        size_t nRealSize);
};

//////////////////////////////////////////////////////////////////////
// ImyCursor Construction/Destruction
//////////////////////////////////////////////////////////////////////

my_bool ImyCursor::my_true = 1;
my_bool ImyCursor::my_false = 0;

ImyCursor::ImyCursor(
    ImyConnection *pImyConnection,
    SACommand *pCommand) :
    ISACursor(pImyConnection, pCommand)
{
    m_bind = NULL;
    m_result = NULL;

    Reset();
}

/*virtual */
ImyCursor::~ImyCursor()
{
    if (NULL != m_bind)
        delete m_bind;

    if (NULL != m_result)
        delete m_result;
}

//////////////////////////////////////////////////////////////////////
// ImyConnection implementation
//////////////////////////////////////////////////////////////////////

/*virtual */
void ImyConnection::InitializeClient()
{
    ::AddMySQLSupport(m_pSAConnection);
}

/*virtual */
void ImyConnection::UnInitializeClient()
{
    if (SAGlobals::UnloadAPI()) ::ReleaseMySQLSupport();
}

/*virtual */
void ImyConnection::CnvtInternalToDateTime(
    SADateTime &date_time,
    const void * pInternal,
    int	nInternalSize)
{
    assert(nInternalSize == int(sizeof(MYSQL_TIME)));
    MYSQL_TIME &dtInternal = *((MYSQL_TIME *)pInternal);

    assert(MYSQL_TIMESTAMP_DATETIME == dtInternal.time_type ||
        MYSQL_TIMESTAMP_DATE == dtInternal.time_type);
    assert(!dtInternal.neg);

    date_time = SADateTime(dtInternal.year, dtInternal.month, dtInternal.day,
        dtInternal.hour, dtInternal.minute, dtInternal.second);
    date_time.Fraction() = (unsigned int)(dtInternal.second_part * 1000);
}

/*virtual */
void ImyConnection::CnvtInternalToInterval(
    SAInterval & interval,
    const void * pInternal,
    int /*nInternalSize*/)
{
    const char* szInterval = (const char*)pInternal;
    CnvtInternalToInterval(interval, szInterval);
}

/*static */
int ImyConnection::second(const char *sSecond)
{
    char s[3] = "SS";
    strncpy(s, sSecond, 2);

    int nSecond = atoi(s);
    assert(nSecond >= 0 && nSecond <= 59);

    return nSecond;
}

/*static */
int ImyConnection::minute(const char *sMinute)
{
    char s[3] = "MM";
    strncpy(s, sMinute, 2);

    int nMinute = atoi(s);
    assert(nMinute >= 0 && nMinute <= 59);

    return nMinute;
}

/*static */
int ImyConnection::shortHour(const char *sHour)
{
    char s[3] = "HH";
    strncpy(s, sHour, 2);

    int nHour = atoi(s);
    assert(nHour >= 0 && nHour <= 23);

    return nHour;
}

/*static */
int ImyConnection::day(const char *sDay)
{
    char s[3] = "DD";
    strncpy(s, sDay, 2);

    int nDay = atoi(s);
    assert(nDay >= 0 && nDay <= 31);

    return nDay;
}

/*static */
int ImyConnection::month(const char *sMonth)
{
    char s[3] = "MM";
    strncpy(s, sMonth, 2);

    int nMonth = atoi(s);
    assert(nMonth >= 0 && nMonth <= 12);

    return nMonth;
}

/*static */
int ImyConnection::longYear(const char *sYear)
{
    char s[5] = "YYYY";
    strncpy(s, sYear, 4);

    int nYear = atoi(s);
    assert(nYear >= 0 && nYear <= 9999);

    return nYear;
}

/*static */
int ImyConnection::shortYear(const char *sYear)
{
    char s[3] = "YY";
    strncpy(s, sYear, 2);

    int nYear = atoi(s);
    assert(nYear >= 0 && nYear <= 99);
    if (nYear <= 69)
        nYear += 2000;
    else
        nYear += 1900;

    return nYear;
}

/*static */
void ImyConnection::CnvtInternalToDateTime(
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

    // also check if date is invalid
    bool bTimeStamp = ::strchr(sInternal, '-') == NULL && strchr(sInternal, ':') == NULL;
    const char* szFraction = ::strchr(sInternal, '.');
    // then try to fill what we can
    size_t nLen = NULL == szFraction ? ::strlen(sInternal) : szFraction - sInternal;

    if (!bTimeStamp)
    {
        switch (nLen)
        {
        case 19:	//	"YYYY-MM-DD HH:MM:SS", DATETIME
            assert(sInternal[10] == ' ');
            assert(sInternal[13] == ':');
            assert(sInternal[16] == ':');
            nSecond = second(sInternal + 17);
            nMinute = minute(sInternal + 14);
            nHour = shortHour(sInternal + 11);
        case 10:	//	"YYYY-MM-DD", DATE
            assert(sInternal[4] == '-');
            assert(sInternal[7] == '-');
            nDay = day(sInternal + 8);
            nMonth = month(sInternal + 5);
            nYear = longYear(sInternal);
            break;
        default:
            assert(false);
        }
    }
    else
    {
        // TIMESTAMP
        switch (nLen)
        {
        case 14:	//	"YYYYMMDDHHMMSS"
            nSecond = second(sInternal + 12);
            nMinute = minute(sInternal + 10);
            nHour = shortHour(sInternal + 8);
        case 8:	//	"YYYYMMDD"
            nDay = day(sInternal + 6);
            nMonth = month(sInternal + 4);
            nYear = longYear(sInternal);
            break;
        case 12:	//	"YYMMDDHHMMSS";
            nSecond = second(sInternal + 10);
        case 10:	//	"YYMMDDHHMM"
            nMinute = minute(sInternal + 8);
            nHour = shortHour(sInternal + 6);
        case 6:	//	"YYMMDD"
            nDay = day(sInternal + 4);
        case 4:	//	"YYMM"
            nMonth = month(sInternal + 2);
        case 2:	//	"YY";
            nYear = shortYear(sInternal);
            break;
        default:
            assert(false);
        }
    }

    if (nMonth != 0 && nDay != 0 && nHour <= 23)	// simple test for validness
        date_time = SADateTime(nYear, nMonth, nDay, nHour, nMinute, nSecond);
    else
        date_time = dt;

    fraction(szFraction, date_time.Fraction());
}

/*static */
void ImyConnection::fraction(const char* szFraction, unsigned int& fraction)
{
    if (NULL != szFraction)
    {
        if ('.' == *szFraction)
            ++szFraction;

        fraction = atoi(szFraction);
        size_t nLen = (9 - strlen(szFraction));
        for (unsigned int i = 0; nLen > i; ++i)
            fraction *= 10;
    }
    else
        // no milli, micro or nano seconds in MySQL now
        fraction = 0;
}

/*static */
void ImyConnection::CnvtInternalToInterval(
    SAInterval &interval,
    const char *sInternal)
{
    char *s = NULL;
    const char *szVal = sInternal;
    bool bNegative = szVal[0] == '-';

    if (bNegative) szVal = sInternal + 1;

    assert(strchr(sInternal, ':') != NULL);

    long nHours = strtol(szVal, &s, 10);
    assert(s && *s == ':');
    szVal = s + 1;
    long nMinute = strtol(szVal, &s, 10);
    assert(s && *s == ':');
    szVal = s + 1;
    long nSecond = strtol(szVal, &s, 10);
    
    // fraction!
    assert(*s == '\0' || *s == '.');
    unsigned int nFraction = 0;
    fraction(s, nFraction);

    if (nSecond <= 59 && nMinute <= 59)	// simple test for validness
    {
        interval = bNegative ? -SAInterval(0, (int)nHours, (int)nMinute, (int)nSecond, nFraction)
            : SAInterval(0, (int)nHours, (int)nMinute, (int)nSecond, nFraction);
    }
    else
        interval = 0.0;
}

/*static */
void ImyConnection::CnvtDateTimeToInternal(
    const SADateTime &date_time,
    SAString &sInternal)
{
    // format should be YYYY-MM-DD HH:MM:SS.fraction
    sInternal.Format(_TSA("%.4d-%.2d-%.2d %.2d:%.2d:%.2d.%.6d"),
        date_time.GetYear(), date_time.GetMonth(), date_time.GetDay(),
        date_time.GetHour(), date_time.GetMinute(), date_time.GetSecond(),
        (int)(date_time.Fraction() / 1000));
}

/*static */
void ImyConnection::CnvtDateTimeToInternal(
    const SADateTime &date_time,
    MYSQL_TIME &dtInternal)
{
    memset(&dtInternal, 0, sizeof(MYSQL_TIME));

    dtInternal.time_type = MYSQL_TIMESTAMP_DATETIME;
    dtInternal.neg = 0;
    dtInternal.year = date_time.GetYear();
    dtInternal.month = date_time.GetMonth();
    dtInternal.day = date_time.GetDay();
    dtInternal.hour = date_time.GetHour();
    dtInternal.minute = date_time.GetMinute();
    dtInternal.second = date_time.GetSecond();
    dtInternal.second_part = (unsigned long)(date_time.Fraction() / 1000);
}

/*virtual */
void ImyConnection::CnvtInternalToCursor(
    SACommand * /*pCursor*/,
    const void * /*pInternal*/)
{
    assert(false);
}

/*virtual */
long ImyConnection::GetClientVersion() const
{
    const char *sClientVer = g_myAPI.mysql_get_client_info();
    char *sPoint;
    short nMajor = (short)strtol(sClientVer, &sPoint, 10);
    assert(*sPoint == '.');
    sPoint++;
    short nMinor = (short)strtol(sPoint, &sPoint, 10);
    return nMinor + (nMajor << 16);
}

/*virtual */
long ImyConnection::GetServerVersion() const
{
    const char *sServerVer = g_myAPI.mysql_get_server_info(m_handles.mysql);
    char *sPoint;
    short nMajor = (short)strtol(sServerVer, &sPoint, 10);
    assert(*sPoint == '.');
    sPoint++;
    short nMinor = (short)strtol(sPoint, &sPoint, 10);
    return nMinor + (nMajor << 16);
}

/*virtual */
SAString ImyConnection::GetServerVersionString() const
{
    return  SAString(g_myAPI.mysql_get_server_info(m_handles.mysql));
}

/*virtual */
bool ImyConnection::IsConnected() const
{
    return (NULL != m_handles.mysql);
}

/*virtual */
bool ImyConnection::IsAlive() const
{
    if (g_myAPI.mysql_ping(m_handles.mysql))
    {
        unsigned int err = g_myAPI.mysql_errno(m_handles.mysql);
        return !(CR_SERVER_GONE_ERROR == err || CR_UNKNOWN_ERROR == err);
    }
    else
        return true;
}

struct ConnectOption
{
    const SAChar *NAME;
    unsigned long CLIENT_FLAG;
};

/*virtual */
void ImyConnection::Connect(
    const SAString &sDBString,
    const SAString &sUserID,
    const SAString &sPassword,
    saConnectionHandler_t fHandler)
{
    assert(m_handles.mysql == NULL);

    // check if any options are provided
    struct ConnectOption CLIENT_FLAGS[] =
    {
        { _TSA("CLIENT_COMPRESS"), CLIENT_COMPRESS },
        { _TSA("CLIENT_FOUND_ROWS"), CLIENT_FOUND_ROWS },
        { _TSA("CLIENT_IGNORE_SPACE"), CLIENT_IGNORE_SPACE },
        { _TSA("CLIENT_INTERACTIVE"), CLIENT_INTERACTIVE },
        { _TSA("CLIENT_LOCAL_FILES"), CLIENT_LOCAL_FILES },
        { _TSA("CLIENT_MULTI_STATEMENTS"), CLIENT_MULTI_STATEMENTS },
        { _TSA("CLIENT_MULTI_RESULTS"), CLIENT_MULTI_RESULTS },
        { _TSA("CLIENT_NO_SCHEMA"), CLIENT_NO_SCHEMA },
        { _TSA("CLIENT_ODBC"), CLIENT_ODBC },
        { _TSA("CLIENT_REMEMBER_OPTIONS"), CLIENT_REMEMBER_OPTIONS }
    };

    unsigned long client_flag = 0l;
    for (size_t i = 0; i < sizeof(CLIENT_FLAGS) / sizeof(struct ConnectOption); ++i)
    {
        SAString sOption = m_pSAConnection->Option(CLIENT_FLAGS[i].NAME);
        // turn on CLIENT_MULTI_STATEMENTS, CLIENT_MULTI_RESULTS and CLIENT_REMEMBER_OPTIONS by default
        if (0 == sa_strcmp(CLIENT_FLAGS[i].NAME, _TSA("CLIENT_MULTI_STATEMENTS")) ||
            0 == sa_strcmp(CLIENT_FLAGS[i].NAME, _TSA("CLIENT_MULTI_RESULTS")) ||
            0 == sa_strcmp(CLIENT_FLAGS[i].NAME, _TSA("CLIENT_REMEMBER_OPTIONS")))
        {
            if (sOption.CompareNoCase(_TSA("FALSE")) != 0)
                client_flag |= CLIENT_FLAGS[i].CLIENT_FLAG;
        }
        else if (0 == sOption.CompareNoCase(_TSA("TRUE")) ||
            0 == sOption.CompareNoCase(_TSA("1")))
            client_flag |= CLIENT_FLAGS[i].CLIENT_FLAG;
    }

    // dbstring as: [server_name][@][dbname]
    // server_name as: hostname[,port], or unix_socket path
    // for connection to server without dbname use 'server_name@'

    unsigned int port = 0;
    SAString sServerName, sDatabase, sHost, sUnixSocket;

    size_t iPos = sDBString.Find(_TSA('@'));
    if (SIZE_MAX == iPos)
        sDatabase = sDBString;
    else  // Database is present in connection string
    {
        sServerName = sDBString.Left(iPos);
        sDatabase = sDBString.Mid(iPos + 1);
    }

#ifdef SQLAPI_WINDOWS
    if (sServerName.IsEmpty() || sServerName.GetAt(0) != _TSA('.'))
#else
    iPos = sServerName.Find(_TSA('/'));
    if (SIZE_MAX == iPos)
#endif
    {
        iPos = sServerName.Find(_TSA(','));
        if (SIZE_MAX == iPos)
            sHost = sServerName;
        else // alternate port number found
        {
            sHost = sServerName.Left(iPos);
            port = sa_toi((const SAChar*)sServerName.Mid(iPos + 1));
        }
    }
    else
#ifdef SQLAPI_WINDOWS
    {
        // named pipes
        sUnixSocket = sServerName.Mid(1);
        sHost = _TSA(".");
    }
#else
        // unix_socket
        sUnixSocket = sServerName;
#endif

    m_handles.mysql = g_myAPI.mysql_init(NULL);

    if (!m_handles.mysql)
        throw SAException(SA_Library_Error, -1, -1, _TSA("mysql_init -> NULL"));

    SAString sOption = m_pSAConnection->Option(_TSA("MYSQL_OPT_READ_TIMEOUT"));
    if (!sOption.IsEmpty())
    {
        unsigned int nVal = sa_toi(sOption);
        g_myAPI.mysql_options(m_handles.mysql,
            MYSQL_OPT_READ_TIMEOUT, (const char*)&nVal);
    }
    sOption = m_pSAConnection->Option(_TSA("MYSQL_OPT_WRITE_TIMEOUT"));
    if (!sOption.IsEmpty())
    {
        unsigned int nVal = sa_toi(sOption);
        g_myAPI.mysql_options(m_handles.mysql,
            MYSQL_OPT_WRITE_TIMEOUT, (const char*)&nVal);
    }
    sOption = m_pSAConnection->Option(_TSA("MYSQL_OPT_CONNECT_TIMEOUT"));
    if (!sOption.IsEmpty())
    {
        unsigned int nVal = sa_toi(sOption);
        g_myAPI.mysql_options(m_handles.mysql,
            MYSQL_OPT_CONNECT_TIMEOUT, (const char*)&nVal);
    }
    sOption = m_pSAConnection->Option(_TSA("MYSQL_OPT_RECONNECT"));
    if (!sOption.IsEmpty())
    {
        my_bool bVal = (0 == sOption.CompareNoCase(_TSA("TRUE")) || 0 == sOption.CompareNoCase(_TSA("1")));
        g_myAPI.mysql_options(m_handles.mysql,
            MYSQL_OPT_RECONNECT, (const char*)&bVal);
    }
    sOption = m_pSAConnection->Option(_TSA("MYSQL_SECURE_AUTH"));
    if (!sOption.IsEmpty())
    {
        my_bool bVal = (0 == sOption.CompareNoCase(_TSA("TRUE")) || 0 == sOption.CompareNoCase(_TSA("1")));
        g_myAPI.mysql_options(m_handles.mysql,
            MYSQL_SECURE_AUTH, (const char*)&bVal);
    }

    if (NULL != g_myAPI.mysql_ssl_set)
    {
        SAString sKey = m_pSAConnection->Option(_TSA("MYSQL_SSL_KEY"));
        SAString sCert = m_pSAConnection->Option(_TSA("MYSQL_SSL_CERT"));
        SAString sCA = m_pSAConnection->Option(_TSA("MYSQL_SSL_CA"));
        SAString sCAPath = m_pSAConnection->Option(_TSA("MYSQL_SSL_CAPATH"));
        SAString sCipher = m_pSAConnection->Option(_TSA("MYSQL_SSL_CIPHER"));

#ifdef SA_UNICODE
        const char *szKey = sKey.IsEmpty() ? NULL : sKey.GetUTF8Chars();
        const char *szCert = sCert.IsEmpty() ? NULL : sCert.GetUTF8Chars();
        const char *szCa = sCA.IsEmpty() ? NULL : sCA.GetUTF8Chars();
        const char *szCapath = sCAPath.IsEmpty() ? NULL : sCAPath.GetUTF8Chars();
        const char *szCipher = sCipher.IsEmpty() ? NULL : sCipher.GetUTF8Chars();
#else
        const char *szKey = sKey.IsEmpty() ? NULL : sKey.GetMultiByteChars();
        const char *szCert = sCert.IsEmpty() ? NULL : sCert.GetMultiByteChars();
        const char *szCa = sCA.IsEmpty() ? NULL : sCA.GetMultiByteChars();
        const char *szCapath = sCAPath.IsEmpty() ? NULL : sCAPath.GetMultiByteChars();
        const char *szCipher = sCipher.IsEmpty() ? NULL : sCipher.GetMultiByteChars();
#endif
        if (NULL != szKey || NULL != szCert || NULL != szCa || NULL != szCapath || NULL != szCipher)
            g_myAPI.mysql_ssl_set(m_handles.mysql, szKey, szCert, szCa, szCapath, szCipher);
    }

    try
    {
        if (NULL != fHandler)
            fHandler(*m_pSAConnection, SA_PreConnectHandler);

        if (g_myAPI.mysql_real_connect2) // version with database name
        {
            if (!g_myAPI.mysql_real_connect2(
                m_handles.mysql,
                sHost.IsEmpty() ? (const char *)NULL : sHost.GetMultiByteChars(),
                sUserID.IsEmpty() ? (const char *)NULL : sUserID.GetMultiByteChars(),
                sPassword.IsEmpty() ? (const char *)NULL : sPassword.GetMultiByteChars(),
                sDatabase.IsEmpty() ? (const char *)NULL : sDatabase.GetMultiByteChars(),
                port,
                sUnixSocket.IsEmpty() ? (const char*)NULL : sUnixSocket.GetMultiByteChars(),
                client_flag))
                Check(m_handles.mysql);
        }
        else
        {
            if (!g_myAPI.mysql_real_connect1(
                m_handles.mysql,
                sHost.IsEmpty() ? (const char *)NULL : sHost.GetMultiByteChars(),
                sUserID.IsEmpty() ? (const char *)NULL : sUserID.GetMultiByteChars(),
                sPassword.IsEmpty() ? (const char *)NULL : sPassword.GetMultiByteChars(),
                port,
                sUnixSocket.IsEmpty() ? (const char *)NULL : sUnixSocket.GetMultiByteChars(),
                client_flag))
                Check(m_handles.mysql);
            if (!sDatabase.IsEmpty() && g_myAPI.mysql_select_db(m_handles.mysql, sDatabase.GetMultiByteChars()))
                Check(m_handles.mysql);
        }

#ifdef SA_UNICODE_WITH_UTF8
        if (NULL != g_myAPI.mysql_set_character_set &&
            g_myAPI.mysql_set_character_set(m_handles.mysql, "utf8"))
            Check(m_handles.mysql);
#else
        sOption = m_pSAConnection->Option(_TSA("CharacterSet"));
        if (!sOption.IsEmpty() && NULL != g_myAPI.mysql_set_character_set &&
            g_myAPI.mysql_set_character_set(m_handles.mysql, sOption.GetMultiByteChars()))
            Check(m_handles.mysql);
#endif

        if (NULL != fHandler)
            fHandler(*m_pSAConnection, SA_PostConnectHandler);
    }
    catch (SAException &)	// clean up
    {
        if (m_handles.mysql)
        {
            g_myAPI.mysql_close(m_handles.mysql);
            m_handles.mysql = NULL;
        }

        throw;
    }
}

/*virtual */
void ImyConnection::Disconnect()
{
    assert(m_handles.mysql != NULL);

    g_myAPI.mysql_close(m_handles.mysql);
    m_handles.mysql = NULL;
}

/*virtual */
void ImyConnection::Destroy()
{
    assert(m_handles.mysql != NULL);

    g_myAPI.mysql_close(m_handles.mysql);
    m_handles.mysql = NULL;
}

/*virtual */
void ImyConnection::Reset()
{
    m_handles.mysql = NULL;
}

/*virtual */
void ImyConnection::Commit()
{
    SACriticalSectionScope scope(&m_myExecMutex);
    SA_TRACE_CMDC(_TSA("COMMIT"));
    if (g_myAPI.mysql_query(m_handles.mysql, "COMMIT"))
        Check(m_handles.mysql);
}

/*virtual */
void ImyConnection::Rollback()
{
    SACriticalSectionScope scope(&m_myExecMutex);
    SA_TRACE_CMDC(_TSA("ROLLBACK"));
    if (g_myAPI.mysql_query(m_handles.mysql, "ROLLBACK"))
        Check(m_handles.mysql);
}

/*virtual */
saAPI *ImyConnection::NativeAPI() const
{
    return 	&g_myAPI;
}

/*virtual */
saConnectionHandles *ImyConnection::NativeHandles()
{
    return &m_handles;
}

/*virtual */
void ImyConnection::setIsolationLevel(
    SAIsolationLevel_t eIsolationLevel)
{
    {
        SACriticalSectionScope scope(&m_myExecMutex);
        // Setting the SESSION privilege will affect
        // the following and all future transactions.
        switch (eIsolationLevel)
        {
        case SA_ReadUncommitted:
            SA_TRACE_CMDC(_TSA("SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED"));
            if (g_myAPI.mysql_query(m_handles.mysql,
                "SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED"))
                Check(m_handles.mysql);
            break;
        case SA_ReadCommitted:
            SA_TRACE_CMDC(_TSA("SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED"));
            if (g_myAPI.mysql_query(m_handles.mysql,
                "SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED"))
                Check(m_handles.mysql);
            break;
        case SA_RepeatableRead:
            SA_TRACE_CMDC(_TSA("SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ"));
            if (g_myAPI.mysql_query(m_handles.mysql,
                "SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ"))
                Check(m_handles.mysql);
            break;
        case SA_Serializable:
            SA_TRACE_CMDC(_TSA("SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE"));
            if (g_myAPI.mysql_query(m_handles.mysql,
                "SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE"))
                Check(m_handles.mysql);
            break;
        default:
            assert(false);
            return;
        }
    }
    // start new transaction
    m_pSAConnection->Commit();
}

/*virtual */
void ImyConnection::setAutoCommit(
    SAAutoCommit_t eAutoCommit)
{
    SACriticalSectionScope scope(&m_myExecMutex);
    switch (eAutoCommit)
    {
    case SA_AutoCommitOff:
        SA_TRACE_CMDC(_TSA("SET AUTOCOMMIT=0"));
        if (g_myAPI.mysql_query(m_handles.mysql, "SET AUTOCOMMIT=0"))
            Check(m_handles.mysql);
        break;
    case SA_AutoCommitOn:
        SA_TRACE_CMDC(_TSA("SET AUTOCOMMIT=1"));
        if (g_myAPI.mysql_query(m_handles.mysql, "SET AUTOCOMMIT=1"))
            Check(m_handles.mysql);
        break;
    default:
        assert(false);
    }
}

/*virtual */
ISACursor *ImyConnection::NewCursor(SACommand *m_pCommand)
{
    return new ImyCursor(this, m_pCommand);
}

//////////////////////////////////////////////////////////////////////
// ImyCursor implementation
//////////////////////////////////////////////////////////////////////

/*virtual */
bool ImyCursor::IsOpened()
{
    if (NULL != m_handles.stmt)
        return true;

    return m_bOpened;
}

/*virtual */
void ImyCursor::Open()
{
    if (NULL != g_myAPI.mysql_stmt_init && getOptionValue(_TSA("UseStatement"), false))
    {
        assert(NULL == m_handles.stmt);

        myConnectionHandles *pConH =
            (myConnectionHandles *)m_pCommand->Connection()->NativeHandles();

        m_handles.stmt = g_myAPI.mysql_stmt_init(pConH->mysql);
        if (NULL == m_handles.stmt)
            Check(pConH->mysql);

        assert(NULL != m_handles.stmt);
    }
    else
    {
        assert(m_bOpened == false);
    }

    m_bOpened = true;
}

/*virtual */
void ImyCursor::Close()
{
    if (NULL != m_handles.stmt) // Use statement API 
    {
        if (g_myAPI.mysql_stmt_close(m_handles.stmt))
            Check(m_handles.stmt);

        m_handles.stmt = NULL;
    }

    m_bOpened = false;
}

/*virtual */
void ImyCursor::Destroy()
{
    if (NULL != m_handles.stmt)
    {
        g_myAPI.mysql_stmt_close(m_handles.stmt);
        m_handles.stmt = NULL;
    }

    m_bOpened = false;
}

/*virtual */
void ImyCursor::Reset()
{
    m_handles.stmt = NULL;
    m_bOpened = false;

    cRows = m_currentRow = SA_MYSQL_NO_ROWS_AFFECTED;

    m_bOpened = false;
    m_bResultSetCanBe = false;

    if (NULL != m_bind)
    {
        delete m_bind;
        m_bind = NULL;
    }

    if (NULL != m_result)
    {
        delete m_result;
        m_result = NULL;
    }
}

// prepare statement (also convert to native format if needed)
/*virtual */
void ImyCursor::Prepare(
    const SAString &sStmt,
    SACommandType_t eCmdType,
    int nPlaceHolderCount,
    saPlaceHolder ** ppPlaceHolders)
{
    if (NULL == m_handles.stmt) // Use old API
    {
        // save original statement
        m_sOriginalStmst = sStmt;
    }
    else // Use statement API
    {
        // clear previou bind structure - can be null if executing fails
        if (NULL != m_bind)
        {
            delete m_bind;
            m_bind = NULL;
        }

        SAString sStmtMySQL;
        size_t nPos = 0l;
        int i;
        switch (eCmdType)
        {
        case SA_CmdSQLStmt:
            // replace bind variables with '?' place holder
            for (i = 0; i < nPlaceHolderCount; i++)
            {
                sStmtMySQL += sStmt.Mid(nPos, ppPlaceHolders[i]->getStart() - nPos);
                sStmtMySQL += _TSA("?");
                nPos = ppPlaceHolders[i]->getEnd() + 1;
            }
            // copy tail
            if (nPos < sStmt.GetLength())
                sStmtMySQL += sStmt.Mid(nPos);
            break;
        case SA_CmdStoredProc:
            assert(false); // MySQL procedure aren't supported with prepared statements
            break;
        case SA_CmdSQLStmtRaw:
            sStmtMySQL = sStmt;
            break;
        default:
            assert(false);
        }

        SACriticalSectionScope scope(&((ImyConnection*)m_pISAConnection)->m_myExecMutex);
        SA_TRACE_CMD(sStmtMySQL);
        if (g_myAPI.mysql_stmt_prepare(m_handles.stmt,
#ifdef SA_UNICODE_WITH_UTF8
            sStmtMySQL.GetUTF8Chars(), (unsigned long)sStmtMySQL.GetUTF8CharsLength()))
#else
            sStmtMySQL.GetMultiByteChars(), (unsigned long)sStmtMySQL.GetMultiByteCharsLength()))
#endif
            Check(m_handles.stmt);
    }
}

/*virtual */
void ImyCursor::UnExecute()
{
    if (NULL != m_handles.result)
    {
        // multiple result sets can be here
        do
        {
            g_myAPI.mysql_free_result(m_handles.result);
            m_handles.result = NULL;

            NextResult();
        } while (NULL != m_handles.result);
    }

    if (NULL != m_handles.stmt) // Use statement API
    {
        // Reset any previous long data send
        /* Don't need at least at this place
        g_myAPI.mysql_stmt_reset(m_handles.stmt);
        */

        if (NULL != m_bind)
        {
            delete m_bind;
            m_bind = NULL;
        }
    }

    m_bResultSetCanBe = false;
    cRows = m_currentRow = SA_MYSQL_NO_ROWS_AFFECTED;
}

// executes statement (also binds parameters if needed)
/*virtual */
void ImyCursor::Execute(
    int nPlaceHolderCount,
    saPlaceHolder **ppPlaceHolders)
{
    if (NULL != m_handles.stmt) // Use statement API
    {
        Execute_Stmt(nPlaceHolderCount, ppPlaceHolders);
        return;
    }

    SAString sBoundStmt, sSQL, sProcParams;

    SAParam* pReturn = NULL;
    if (SA_CmdStoredProc == m_pCommand->CommandType())
    {
        // build MySQL stored procedure query parts
        // should be 2 or 3 queries:
        // set @p1=1.1,@p2=NULL
        // call proc1(@p1,@p2)
        // and if output parameters exist
        // select @p2

        if (m_pCommand->ParamCount() > 0 &&
            nPlaceHolderCount == m_pCommand->ParamCount())
        {
            for (int i = 0; i < nPlaceHolderCount; ++i)
            {
                SAParam &p = *ppPlaceHolders[i]->getParam();
                if (SA_ParamReturn == p.ParamDirType())
                {
                    pReturn = &p;
                    continue;
                }

                // build parameter list
                if (i > 0)
                {
                    sSQL += _TSA(',');
                    sProcParams += _TSA(',');
                }
                sSQL += _TSA('@') + p.Name() + _TSA('=');

                ppPlaceHolders[i]->getStart() = 4 + sSQL.GetLength();
                ppPlaceHolders[i]->getEnd() = ppPlaceHolders[i]->getStart()
                    + p.Name().GetLength();

                sSQL += _TSA(':') + p.Name();
                sProcParams += _TSA('@') + p.Name();
            }

            // build final query
            if (!sSQL.IsEmpty())
                sSQL = _TSA("SET ") + sSQL;
        }
        else
            sSQL = _TSA("CALL ") + m_sOriginalStmst + _TSA("()");
    }
    else
        sSQL = m_sOriginalStmst;

    // change ':' param markers to bound values
    size_t nPos = 0l;
    for (int i = 0; i < nPlaceHolderCount; ++i)
    {
        SAParam &Param = *ppPlaceHolders[i]->getParam();
        if (SA_ParamReturn == Param.ParamDirType())
            continue;

        sBoundStmt += sSQL.Mid(nPos, ppPlaceHolders[i]->getStart() - nPos);

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
                sBoundValue = Param.asBool() ? _TSA("1") : _TSA("0");
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
                ImyConnection::CnvtDateTimeToInternal(
                    Param.setAsDateTime(), sTemp);
                sBoundValue = _TSA("'");
                sBoundValue += sTemp;
                sBoundValue += _TSA("'");
                break;
            case SA_dtInterval:
                sTemp = (const SAString&)Param.asInterval();
                sBoundValue = _TSA("'") + sTemp + _TSA("'");
                break;
            case SA_dtString:
                sBoundValue = _TSA("'");
                sBoundValue += MySQLEscapeString(Param.asString());
                sBoundValue += _TSA("'");
                break;
            case SA_dtBytes:
                if (Param.asBytes().GetBinaryLength())
                {
                    sBoundValue = _TSA("0x");
                    sBoundValue += Bin2Hex(Param.asBytes(), Param.asBytes().GetBinaryLength());
                }
                else
                    sBoundValue = _TSA("''");
                break;
            case SA_dtLongBinary:
            case SA_dtBLob:
                BindBLob(Param, sBoundStmt);
                break;
            case SA_dtLongChar:
            case SA_dtCLob:
                BindText(Param, sBoundStmt);
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
    if (nPos < sSQL.GetLength())
        sBoundStmt += sSQL.Mid(nPos);

    myConnectionHandles *pConH =
        (myConnectionHandles *)m_pCommand->Connection()->NativeHandles();

    if (!sBoundStmt.IsEmpty()) // Can be empty when it's a function without parameters
    {
        SACriticalSectionScope scope(&((ImyConnection*)m_pISAConnection)->m_myExecMutex);
        SA_TRACE_CMD(sBoundStmt);
        if (g_myAPI.mysql_real_query(pConH->mysql,
#ifdef SA_UNICODE_WITH_UTF8
            sBoundStmt.GetUTF8Chars(), (unsigned long)sBoundStmt.GetUTF8CharsLength()))
#else
            sBoundStmt.GetMultiByteChars(), (unsigned long)sBoundStmt.GetMultiByteCharsLength()))
#endif
            Check(pConH->mysql);
    }

    if (SA_CmdStoredProc == m_pCommand->CommandType() &&
        (sProcParams.GetLength() > 0 || pReturn))
    {
        sBoundStmt = (pReturn ? _TSA("SELECT ") : _TSA("CALL ")) + m_sOriginalStmst +
            _TSA('(') + sProcParams + _TSA(")");
        {
            SACriticalSectionScope scope(&((ImyConnection*)m_pISAConnection)->m_myExecMutex);
            SA_TRACE_CMD(sBoundStmt);
            if (g_myAPI.mysql_real_query(pConH->mysql,
#ifdef SA_UNICODE_WITH_UTF8
                sBoundStmt.GetUTF8Chars(), (unsigned long)sBoundStmt.GetUTF8CharsLength()))
#else
                sBoundStmt.GetMultiByteChars(), (unsigned long)sBoundStmt.GetMultiByteCharsLength()))
#endif
                Check(pConH->mysql);
        }

        if (pReturn)
        {
            // parameters are binary fields, we set the apropriate field types here
            m_handles.result = g_myAPI.mysql_store_result(pConH->mysql);
            Check(pConH->mysql);

            m_mysql_row = g_myAPI.mysql_fetch_row(m_handles.result);
            if (m_mysql_row)
            {
                m_lengths = g_myAPI.mysql_fetch_lengths(m_handles.result);
                ConvertMySQLFieldToParam(0, *pReturn);
            }
            else
                Check(pConH->mysql);

            if (m_handles.result)
                g_myAPI.mysql_free_result(m_handles.result);

            m_handles.result = NULL;
            m_mysql_row = NULL;
            m_lengths = NULL;
        }
    }

    // save affected rows
    GetRowsAffected();

    SAString sOption = m_pCommand->Option(_TSA("HandleResult"));
    if (isSetScrollable() || sOption.CompareNoCase(_TSA("store")) == 0)
        m_handles.result = g_myAPI.mysql_store_result(pConH->mysql);
    else
        m_handles.result = g_myAPI.mysql_use_result(pConH->mysql);

    m_bResultSetCanBe = true;

    if (GetRowsAffected() < 0)
        Check(pConH->mysql);

    if (NULL == m_handles.result)
    {
        m_bResultSetCanBe = false;
        NextResult();
    }
}

SAString ImyCursor::MySQLEscapeString(const char *sFrom, size_t nLen)
{
    char *p = (char*)sa_malloc(nLen * 2 + 1);

    if (g_myAPI.mysql_real_escape_string)
    {
        myConnectionHandles *pConH =
            (myConnectionHandles *)m_pCommand->Connection()->NativeHandles();
        g_myAPI.mysql_real_escape_string(pConH->mysql, p, sFrom, (unsigned long)nLen);
}
    else
        g_myAPI.mysql_escape_string(p, sFrom, (unsigned long)nLen);

    SAString sNew;
#ifdef SA_UNICODE_WITH_UTF8
    sNew.SetUTF8Chars(p);
#else
    sNew = SAString(p);
#endif

    free(p);
    return sNew;
}

SAString ImyCursor::MySQLEscapeString(const SAString &sValue)
{
    size_t nLen =
#ifdef SA_UNICODE_WITH_UTF8
        sValue.GetUTF8CharsLength();
#else
        sValue.GetMultiByteCharsLength();
#endif
    if (!nLen)
        return sValue;

    return MySQLEscapeString(
#ifdef SA_UNICODE_WITH_UTF8
        sValue.GetUTF8Chars(),
#else
        sValue.GetMultiByteChars(),
#endif
        nLen);
}

void ImyCursor::BindBLob(SAParam &Param, SAString &sBoundStmt)
{
    assert(NULL == m_handles.stmt); // Use with old API only

    SAString sBLob;
    size_t nActualWrite;
    SAPieceType_t ePieceType = SA_FirstPiece;
    void *pBuf;
    while ((nActualWrite = Param.InvokeWriter(
        ePieceType, ImyConnection::MaxLongPiece, pBuf)) != 0)
    {
        sBLob += Bin2Hex(pBuf, nActualWrite);

        if (ePieceType == SA_LastPiece)
            break;
    }

    if (sBLob.GetLength())
        sBoundStmt += _TSA("0x") + sBLob;
    else
        sBoundStmt += _TSA("''");
}

void ImyCursor::BindText(SAParam &Param, SAString &sBoundStmt)
{
    assert(NULL == m_handles.stmt); // Use with old API only

    sBoundStmt += _TSA("'");

    size_t nActualWrite;
    SAPieceType_t ePieceType = SA_FirstPiece;
    void *pBuf;
    while ((nActualWrite = Param.InvokeWriter(
        ePieceType, ImyConnection::MaxLongPiece, pBuf)) != 0)
    {
        sBoundStmt += MySQLEscapeString(SAString(pBuf, nActualWrite));

        if (ePieceType == SA_LastPiece)
            break;
    }

    sBoundStmt += _TSA("'");
}

/*virtual */
void ImyCursor::Cancel()
{
    if (NULL != m_handles.stmt) // Use statement API
    {
        if (g_myAPI.mysql_stmt_reset(m_handles.stmt))
            Check(m_handles.stmt);
    }
    else
    {
        myConnectionHandles *pConH = (myConnectionHandles *)
            m_pCommand->Connection()->NativeHandles();

        if (m_pISAConnection->GetClientVersion() >= 0x00050000)
        {
            // send KILL QUERY command
            char szCmd[22];
            sprintf(szCmd, "KILL QUERY %lu", g_myAPI.mysql_thread_id(pConH->mysql));

            SACriticalSectionScope scope(&((ImyConnection*)m_pISAConnection)->m_myExecMutex);
            SA_TRACE_CMD(SAString(szCmd));
            if (g_myAPI.mysql_query(pConH->mysql, szCmd))
                Check(pConH->mysql);
        }
        else
        {
            // kill connection
            if (g_myAPI.mysql_kill(pConH->mysql,
                g_myAPI.mysql_thread_id(pConH->mysql)))
                Check(pConH->mysql);

            // try to restore connection
            g_myAPI.mysql_ping(pConH->mysql);
        }
    }
}

/*virtual */
bool ImyCursor::ResultSetExists()
{
    if (!m_bResultSetCanBe)
        return false;

    return (NULL != m_handles.result || NULL != m_handles.stmt);
}

// MYSQL keeps changing header for MYSQL_FIELD
// so we have to archive historical ones here
// to support old versions of MySQL client
typedef struct st_mysql_field_3_x {
    char *name;			/* Name of column */
    char *table;			/* Table of column if column was a field */
    char *def;			/* Default value (set by mysql_list_fields) */
    enum enum_field_types type;	/* Type of field. Se mysql_com.h for types */
    unsigned int length;		/* Width of column */
    unsigned int max_length;	/* Max width of selected set */
    unsigned int flags;		/* Div flags */
    unsigned int decimals;	/* Number of decimals in field */
} MYSQL_FIELD_3_x;

// special support for 4.0 clients
// because in 4.0 they have changed MYSQL_FIELD
typedef struct st_mysql_field_4_0 {
    char *name;			/* Name of column */
    char *table;			/* Table of column if column was a field */
    char *org_table;		/* Org table name if table was an alias */
    char *db;			/* Database for table */
    char *def;			/* Default value (set by mysql_list_fields) */
    unsigned long length;		/* Width of column */
    unsigned long max_length;	/* Max width of selected set */
    unsigned int flags;		/* Div flags */
    unsigned int decimals;	/* Number of decimals in field */
    enum enum_field_types type;	/* Type of field. Se mysql_com.h for types */
} MYSQL_FIELD_4_0;

typedef struct st_mysql_field_4_1 {
    char *name;                 /* Name of column */
    char *org_name;             /* Original column name, if an alias */
    char *table;                /* Table of column if column was a field */
    char *org_table;            /* Org table name, if table was an alias */
    char *db;                   /* Database for table */
    char *catalog;	      /* Catalog for table */
    char *def;                  /* Default value (set by mysql_list_fields) */
    unsigned long length;       /* Width of column (create length) */
    unsigned long max_length;   /* Max width for selected set */
    unsigned int name_length;
    unsigned int org_name_length;
    unsigned int table_length;
    unsigned int org_table_length;
    unsigned int db_length;
    unsigned int catalog_length;
    unsigned int def_length;
    unsigned int flags;         /* Div flags */
    unsigned int decimals;      /* Number of decimals in field */
    unsigned int charsetnr;     /* Character set */
    enum enum_field_types type; /* Type of field. See mysql_com.h for types */
} MYSQL_FIELD_4_1;

class mysql_field
{
public:
    virtual ~mysql_field() {};

public:
    static mysql_field *getInstance(
        long nClientVersion,
        void *fields);

    virtual enum enum_field_types type() = 0;
    virtual unsigned long length() = 0;
    virtual unsigned int decimals() = 0;
    virtual unsigned int flags() = 0;
    virtual char *name() = 0;

    virtual void advance() = 0;
};

class mysql_field_3_x : public mysql_field
{
public:
    virtual ~mysql_field_3_x() {};

public:
    mysql_field_3_x(void *fields)
    {
        this->current = (MYSQL_FIELD_3_x *)fields;
    }

    virtual enum enum_field_types type() { return this->current->type; }
    virtual unsigned long length() { return this->current->length; }
    virtual unsigned int decimals() { return this->current->decimals; }
    virtual unsigned int flags() { return this->current->flags; }
    virtual char *name() { return this->current->name; }

    virtual void advance() { ++this->current; }

private:
    MYSQL_FIELD_3_x *current;
};

class mysql_field_4_0 : public mysql_field
{
public:
    virtual ~mysql_field_4_0() {};

public:
    mysql_field_4_0(void *fields)
    {
        this->current = (MYSQL_FIELD_4_0 *)fields;
    }

    virtual enum enum_field_types type() { return this->current->type; }
    virtual unsigned long length() { return this->current->length; }
    virtual unsigned int decimals() { return this->current->decimals; }
    virtual unsigned int flags() { return this->current->flags; }
    virtual char *name() { return this->current->name; }

    virtual void advance() { ++this->current; }

private:
    MYSQL_FIELD_4_0 *current;
};

class mysql_field_4_1 : public mysql_field
{
public:
    virtual ~mysql_field_4_1() {};

public:
    mysql_field_4_1(void *fields)
    {
        this->current = (MYSQL_FIELD_4_1 *)fields;
    }

    virtual enum enum_field_types type() { return this->current->type; }
    virtual unsigned long length() { return this->current->length; }
    virtual unsigned int decimals() { return this->current->decimals; }
    virtual unsigned int flags() { return this->current->flags; }
    virtual char *name() { return this->current->name; }

    virtual void advance() { ++this->current; }

private:
    MYSQL_FIELD_4_1 *current;
};

class mysql_field_latest : public mysql_field
{
public:
    mysql_field_latest(void *fields)
    {
        this->current = (MYSQL_FIELD *)fields;
    }

    virtual enum enum_field_types type() { return this->current->type; }
    virtual unsigned long length() { return this->current->length; }
    virtual unsigned int decimals() { return this->current->decimals; }
    virtual unsigned int flags() { return this->current->flags; }
    virtual char *name() { return this->current->name; }

    virtual void advance() { ++this->current; }

private:
    MYSQL_FIELD *current;
};

/*static */
mysql_field *mysql_field::getInstance(
    long nClientVersion,
    void *fields)
{
    bool b_3_x = nClientVersion < 0x00040000;
    bool b_5_1_plus = nClientVersion >= 0x00050001;
    bool b_4_1 = !b_5_1_plus && nClientVersion >= 0x00040001;
    bool b_4_0 = !b_4_1 && !b_5_1_plus && nClientVersion >= 0x00040000;

    if (b_3_x)
        return new mysql_field_3_x(fields);
    if (b_4_0)
        return new mysql_field_4_0(fields);
    if (b_4_1)
        return new mysql_field_4_1(fields);

    return new mysql_field_latest(fields);
}

/*virtual */
void ImyCursor::DescribeFields(
    DescribeFields_cb_t fn)
{
    if (NULL != m_handles.stmt) // Use statement API
    {
        DescribeFields_Stmt(fn);
        return;
    }

    if (NULL == m_handles.result)
        return;

    myConnectionHandles *pConH =
        (myConnectionHandles *)m_pCommand->Connection()->NativeHandles();

    unsigned int cFields = g_myAPI.mysql_num_fields(m_handles.result);

    MY_CHARSET_INFO cs;
    memset(&cs, 0, sizeof(cs));
    if (g_myAPI.mysql_get_character_set_info)
        g_myAPI.mysql_get_character_set_info(pConH->mysql, &cs);

    mysql_field* fieldIter = mysql_field::getInstance(
        m_pISAConnection->GetClientVersion(),
        g_myAPI.mysql_fetch_fields(m_handles.result));
    for (unsigned int iField = 0; iField < cFields; ++iField)
    {
        enum enum_field_types type = fieldIter->type();
        unsigned long length = fieldIter->length();
        unsigned int decimals = fieldIter->decimals();
        unsigned int flags = fieldIter->flags();
        SAString name;
#ifdef SA_UNICODE_WITH_UTF8
        name.SetUTF8Chars(fieldIter->name());
#else
        name = SAString(fieldIter->name());
#endif
        int Prec;	// calculated by ImyConnection::CnvtNativeToStd
        SADataType_t eDataType = ImyConnection::CnvtNativeToStd(
            type,
            (unsigned int)length,
            Prec,
            decimals,
            flags);

        if (eDataType == SA_dtString && cs.mbmaxlen)
            length /= cs.mbmaxlen;

        (m_pCommand->*fn)(
            name,
            eDataType,
            type,
            length,
            Prec,
            decimals,
            flags & NOT_NULL_FLAG,
            (int)cFields);

        fieldIter->advance();
    }
    delete fieldIter;
}

/*virtual */
void ImyCursor::SetSelectBuffers()
{
    if (NULL != m_handles.stmt) // Use statement API
    {
        // use default helpers
        AllocSelectBuffer(sizeof(my_bool), sizeof(unsigned long), 1);

        // bind the output buffers
        if (NULL != m_result &&
            g_myAPI.mysql_stmt_bind_result(m_handles.stmt, m_result->get_bind()))
            Check(m_handles.stmt);

        if (isSetScrollable() &&
            g_myAPI.mysql_stmt_result_metadata(m_handles.stmt) &&
            g_myAPI.mysql_stmt_store_result(m_handles.stmt))
            Check(m_handles.stmt);
    }

    // do nothing with old API
}

void ImyCursor::ConvertMySQLFieldToParam(int iField, SAParam& value)
{
    SADataType_t eFieldType = value.DataType();

    if (m_mysql_row[iField] == NULL)
    {
        *value.m_pbNull = true;
        return;
    }
    *value.m_pbNull = false;

    const char *sValue = m_mysql_row[iField];
    unsigned long nRealSize = m_lengths[iField];
    char *sValueCopy;

    switch (eFieldType)
    {
    case SA_dtUnknown:
        throw SAException(SA_Library_Error, SA_Library_Error_UnknownDataType, -1, IDS_UNKNOWN_DATA_TYPE);
    case SA_dtShort:
        *(short*)value.m_pScalar = (short)atol(sValue);
        break;
    case SA_dtUShort:
        *(unsigned short*)value.m_pScalar = (unsigned short)strtoul(sValue, NULL, 10);
        break;
    case SA_dtLong:
        *(long*)value.m_pScalar = atol(sValue);
        break;
    case SA_dtULong:
        *(unsigned long*)value.m_pScalar = strtoul(sValue, NULL, 10);
        break;
    case SA_dtDouble:
        sValueCopy = (char*)sa_malloc(nRealSize + 1);
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
        ImyConnection::CnvtInternalToNumeric(
            *value.m_pNumeric,
            sValue);
        break;
    case SA_dtDateTime:
        ImyConnection::CnvtInternalToDateTime(
            *value.m_pDateTime,
            sValue);
        break;
    case SA_dtInterval:
        ImyConnection::CnvtInternalToInterval(
            *value.m_pInterval,
            sValue);
        break;
    case SA_dtString:
#ifdef SA_UNICODE_WITH_UTF8
        value.m_pString->SetUTF8Chars(sValue, nRealSize);
#else
        *value.m_pString =
            SAString(sValue, nRealSize);
#endif
        break;
    case SA_dtBytes:
        *value.m_pString =
            SAString((const void*)sValue, nRealSize);
        break;
    case SA_dtLongBinary:
    case SA_dtLongChar:
    case SA_dtBLob:
    case SA_dtCLob:
        break;
    default:
        assert(false);	// unknown type
    }

    if (isLongOrLob(eFieldType))
        ConvertLongOrLOB(ISA_ParamValue, value, NULL, 0);
}

void ImyCursor::ConvertMySQLRowToFields()
{
    int cFields = m_pCommand->FieldCount();
    for (int iField = 0; iField < cFields; ++iField)
    {
        SAField &Field = m_pCommand->Field(iField + 1);
        SADataType_t eFieldType = Field.FieldType();

        if (m_mysql_row[iField] == NULL)
        {
            *Field.m_pbNull = true;
            continue;
        }
        *Field.m_pbNull = false;

        const char *sValue = m_mysql_row[iField];
        unsigned long nRealSize = m_lengths[iField];
        char *sValueCopy;

        switch (eFieldType)
        {
        case SA_dtUnknown:
            throw SAException(SA_Library_Error, SA_Library_Error_UnknownDataType, -1, IDS_UNKNOWN_DATA_TYPE);
        case SA_dtShort:
            Field.m_eDataType = SA_dtShort;
            *(short*)Field.m_pScalar = (short)atol(sValue);
            break;
        case SA_dtUShort:
            Field.m_eDataType = SA_dtUShort;
            *(short*)Field.m_pScalar = (unsigned short)strtoul(sValue, NULL, 10);
            break;
        case SA_dtLong:
            Field.m_eDataType = SA_dtLong;
            *(long*)Field.m_pScalar = atol(sValue);
            break;
        case SA_dtULong:
            Field.m_eDataType = SA_dtULong;
            *(unsigned long*)Field.m_pScalar = strtoul(sValue, NULL, 10);
            break;
        case SA_dtDouble:
            Field.m_eDataType = SA_dtDouble;

            sValueCopy = (char*)sa_malloc(nRealSize + 1);
            strcpy(sValueCopy, sValue);

            char *pEnd;
            *(double*)Field.m_pScalar = strtod(sValueCopy, &pEnd);
            // strtod is locale dependent, so if it fails,
            // replace the decimal point and retry
            if (*pEnd != 0)
            {
                struct lconv *plconv = ::localeconv();
                if (plconv && plconv->decimal_point)
                {
                    *pEnd = *plconv->decimal_point;
                    *(double*)Field.m_pScalar = strtod(sValueCopy, &pEnd);
                }
            }

            ::free(sValueCopy);
            break;
        case SA_dtNumeric:
            Field.m_eDataType = SA_dtNumeric;
            ImyConnection::CnvtInternalToNumeric(
                *Field.m_pNumeric,
                sValue);
            break;
        case SA_dtDateTime:
            Field.m_eDataType = SA_dtDateTime;
            ImyConnection::CnvtInternalToDateTime(
                *Field.m_pDateTime,
                sValue);
            break;
        case SA_dtInterval:
            Field.m_eDataType = SA_dtInterval;
            ImyConnection::CnvtInternalToInterval(
                *Field.m_pInterval,
                sValue);
            break;
        case SA_dtString:
            Field.m_eDataType = SA_dtString;
#ifdef SA_UNICODE_WITH_UTF8
            Field.m_pString->SetUTF8Chars(sValue, nRealSize);
#else
            *Field.m_pString =
                SAString(sValue, nRealSize);
#endif
            break;
        case SA_dtBytes:
            Field.m_eDataType = SA_dtBytes;
            *Field.m_pString =
                SAString((const void*)sValue, nRealSize);
            break;
        case SA_dtLongBinary:
            Field.m_eDataType = SA_dtLongBinary;
            break;
        case SA_dtLongChar:
            Field.m_eDataType = SA_dtLongChar;
            break;
        case SA_dtBLob:
            Field.m_eDataType = SA_dtBLob;
            break;
        case SA_dtCLob:
            Field.m_eDataType = SA_dtCLob;
            break;
        default:
            assert(false);	// unknown type
        }

        if (isLongOrLob(eFieldType))
            ConvertLongOrLOB(ISA_FieldValue, Field, NULL, 0);
    }
}

void ImyCursor::NextResult()
{
    myConnectionHandles *pConH = (myConnectionHandles *)
        m_pCommand->Connection()->NativeHandles();

    SAString sOption = m_pCommand->Option(_TSA("HandleResult"));

    if (NULL != m_handles.result)
    {
        g_myAPI.mysql_free_result(m_handles.result);
        m_handles.result = NULL;
    }

    do
    {
        if (NULL != g_myAPI.mysql_next_result)
        {
            int nSetExists = g_myAPI.mysql_next_result(pConH->mysql);
            if (0 > nSetExists)
                break;
            else if (0 < nSetExists)
                Check(pConH->mysql);
        }

        if (isSetScrollable() || sOption.CompareNoCase(_TSA("store")) == 0)
            m_handles.result = g_myAPI.mysql_store_result(pConH->mysql);
        else
            m_handles.result = g_myAPI.mysql_use_result(pConH->mysql);

        m_pCommand->DestroyFields();
        m_bResultSetCanBe = true;

        Check(pConH->mysql);
    } while (NULL != g_myAPI.mysql_next_result && NULL == m_handles.result);

    // procedure can have returned parameters
    if (!ResultSetExists() &&
        SA_CmdStoredProc == m_pCommand->CommandType() &&
        m_pCommand->ParamCount() > 0)
    {
        SAString sReturnParams, sBoundStmt;

        // build output parameter list
        for (int i = 0; i < m_pCommand->ParamCount(); ++i)
        {
            SAParam &p = m_pCommand->ParamByIndex(i);
            if (SA_ParamOutput == p.ParamDirType() ||
                SA_ParamInputOutput == p.ParamDirType())
            {
                if (sReturnParams.GetLength() > 0)
                    sReturnParams += _TSA(',');
                sReturnParams += _TSA('@') + p.Name();
            }
        }

        if (sReturnParams.GetLength() > 0)
        {
            sBoundStmt = _TSA("SELECT ") + sReturnParams;
            {
                SACriticalSectionScope scope(&((ImyConnection*)m_pISAConnection)->m_myExecMutex);
                SA_TRACE_CMD(sBoundStmt);
                if (g_myAPI.mysql_query(pConH->mysql,
#ifdef SA_UNICODE_WITH_UTF8
                    sBoundStmt.GetUTF8Chars()))
#else
                    sBoundStmt.GetMultiByteChars()))
#endif
                    Check(pConH->mysql);
            }

            // parameters are binary fields, we set the apropriate field types here
            m_handles.result = g_myAPI.mysql_store_result(pConH->mysql);
            Check(pConH->mysql);

            m_pCommand->DestroyFields();

            m_mysql_row = g_myAPI.mysql_fetch_row(m_handles.result);
            if (m_mysql_row)
            {
                m_lengths = g_myAPI.mysql_fetch_lengths(m_handles.result);
                m_pCommand->DescribeFields();

                for (int i = 0; i < m_pCommand->FieldCount(); ++i)
                    ConvertMySQLFieldToParam(i,
                    m_pCommand->Param(m_pCommand->Field(i + 1).Name().Mid(1)));
            }
            else
                Check(pConH->mysql);
        }

        if (m_handles.result)
            g_myAPI.mysql_free_result(m_handles.result);

        m_handles.result = NULL;
        m_mysql_row = NULL;
        m_lengths = NULL;
        m_bResultSetCanBe = false;
        }
    }

/*virtual */
bool ImyCursor::FetchNext()
{
    if (NULL != m_handles.stmt) // Use statement API
        return FetchNext_Stmt();

    if (NULL == m_handles.result)
        return false;

    m_mysql_row = g_myAPI.mysql_fetch_row(m_handles.result);

    if (m_mysql_row)
    {
        ++m_currentRow;
        m_lengths = g_myAPI.mysql_fetch_lengths(m_handles.result);
        ConvertMySQLRowToFields();
    }
    else if (!isSetScrollable())
    {
        m_bResultSetCanBe = false;
        myConnectionHandles *pConH = (myConnectionHandles *)
            m_pCommand->Connection()->NativeHandles();
        Check(pConH->mysql);
        NextResult();
    }

    return m_mysql_row != NULL;
}

/*virtual */
bool ImyCursor::FetchPrior()
{
    if (m_currentRow <= 1)
    {
        m_currentRow = 0x00000000;
        if (NULL != m_handles.stmt) // Use statement API
            g_myAPI.mysql_stmt_data_seek(m_handles.stmt, m_currentRow);
        else if (NULL != m_handles.result)
            g_myAPI.mysql_data_seek(m_handles.result, m_currentRow);
        return false;
    }

    m_currentRow -= 0x00000002;

    if (NULL != m_handles.stmt) // Use statement API
        g_myAPI.mysql_stmt_data_seek(m_handles.stmt, m_currentRow);
    else if (NULL != m_handles.result)
        g_myAPI.mysql_data_seek(m_handles.result, m_currentRow);
    else
        return false;

    return FetchNext();
}

/*virtual */
bool ImyCursor::FetchFirst()
{
    m_currentRow = 0x00000000;
    if (NULL != m_handles.stmt) // Use statement API
        g_myAPI.mysql_stmt_data_seek(m_handles.stmt, m_currentRow);
    else if (NULL != m_handles.result)
        g_myAPI.mysql_data_seek(m_handles.result, m_currentRow);
    else
        return false;

    return FetchNext();
}

/*virtual */
bool ImyCursor::FetchLast()
{
    if (NULL != m_handles.stmt) // Use statement API
    {
        m_currentRow = g_myAPI.mysql_stmt_num_rows(m_handles.stmt) - 0x00000001;
        g_myAPI.mysql_stmt_data_seek(m_handles.stmt, m_currentRow);
    }
    else if (NULL != m_handles.result)
    {
        m_currentRow = g_myAPI.mysql_num_rows(m_handles.result) - 0x00000001;
        g_myAPI.mysql_data_seek(m_handles.result, m_currentRow);
    }
    else
        return false;

    return FetchNext();
}

/*virtual */
bool ImyCursor::FetchPos(int offset, bool Relative/* = false*/)
{
    my_ulonglong rowCount = 0x00000000;

    if (NULL != m_handles.stmt) // Use statement API
        rowCount = g_myAPI.mysql_stmt_num_rows(m_handles.stmt);
    else if (NULL != m_handles.result)
        rowCount = g_myAPI.mysql_num_rows(m_handles.result);
    else
        return false;

    if (offset > 0 && ((Relative ? m_currentRow : 0x00000000) + offset - 0x00000001) >= rowCount)
        return false;
    else if (offset < 0 && (Relative ? m_currentRow : 0x00000000) < (my_ulonglong)abs(offset))
        return false;
    else
        m_currentRow = (Relative ? m_currentRow : 0x00000000) + offset - 0x00000001;

    if (NULL != m_handles.stmt) // Use statement API
        g_myAPI.mysql_stmt_data_seek(m_handles.stmt, m_currentRow);
    else if (NULL != m_handles.result)
        g_myAPI.mysql_data_seek(m_handles.result, m_currentRow);

    return FetchNext();
}

/*virtual */
long ImyCursor::GetRowsAffected()
{
    if (SA_MYSQL_NO_ROWS_AFFECTED == cRows)
    {
        if (NULL != m_handles.stmt) // Use satetment API
            cRows = g_myAPI.mysql_stmt_affected_rows(m_handles.stmt);
        else
        {
            myConnectionHandles *pConH =
                (myConnectionHandles *)m_pCommand->Connection()->NativeHandles();
            cRows = g_myAPI.mysql_affected_rows(pConH->mysql);
        }
    }

    return (SA_MYSQL_NO_ROWS_AFFECTED == cRows ? ((long)-1) : ((long)cRows));
}

/*virtual */
void ImyCursor::ReadLongOrLOB(
    ValueType_t eValueType,
    SAValueRead &vr,
    void * pValue,
    size_t nFieldBufSize,
    saLongOrLobReader_t fnReader,
    size_t nReaderWantedPieceSize,
    void *pAddlData)
{
    if (NULL != m_handles.stmt) // Use statement API
    {
        ReadLongOrLOB_Stmt(eValueType, vr, pValue, nFieldBufSize,
            fnReader, nReaderWantedPieceSize, pAddlData);
        return;
    }

    int nPos = -1;
    if (ISA_ParamValue == eValueType)
        nPos = m_pCommand->Field(SAString(_TSA("@")) + ((SAParam &)vr).Name()).Pos();
    else
        nPos = ((SAField &)vr).Pos();
    assert(nPos > 0);

    // get long data and size
    const char *pData = m_mysql_row[nPos - 1];
    unsigned long nLongSize = m_lengths[nPos - 1];

    SADummyConverter DummyConverter;
    SAMultibyte2UnicodeConverter Multibyte2UnicodeConverter;
    ISADataConverter *pIConverter = &DummyConverter;
    unsigned int nCnvtLongSizeMax = (unsigned int)nLongSize;
#ifdef SA_UNICODE
    if( SA_dtLongChar == vr.DataType() ||
        SA_dtCLob == vr.DataType() )
    {
        pIConverter = &Multibyte2UnicodeConverter;
#ifdef SA_UNICODE_WITH_UTF8
        Multibyte2UnicodeConverter.SetUseUTF8();
#endif
        // in the worst case each byte can represent a Unicode character
        nCnvtLongSizeMax = nLongSize*sizeof(wchar_t);
    }
#endif

    unsigned char* pBuf;
    size_t nPieceSize = vr.PrepareReader(
        sa_max(nCnvtLongSizeMax, nLongSize),	// real size is nCnvtLongSizeMax
#ifdef SA_UNICODE_WITH_UTF8
        0x7FFF,
#else
        ImyConnection::MaxLongPiece,
#endif
        pBuf,
        fnReader,
        nReaderWantedPieceSize,
        pAddlData);
    assert(nPieceSize > 0 && nPieceSize <= ImyConnection::MaxLongPiece);

    size_t nCnvtPieceSize = nPieceSize;
    SAPieceType_t ePieceType = SA_FirstPiece;
    size_t nTotalRead = 0l;
    size_t nTotalPassedToReader = 0l;
    do
    {
        if (nLongSize)	// known
            nPieceSize = sa_min(nPieceSize, (nLongSize - nTotalRead));
        else
        {
            vr.InvokeReader(SA_LastPiece, pBuf, 0);
            break;
        }

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

        pIConverter->PutStream(pBuf, actual_read, ePieceType);
        size_t nCnvtSize;
        SAPieceType_t eCnvtPieceType;
        // smart while: initialize nCnvtPieceSize before calling pIConverter->GetStream
        while (nCnvtPieceSize = (nCnvtLongSizeMax ? // known
            sa_min(nCnvtPieceSize, nCnvtLongSizeMax - nTotalPassedToReader) : nCnvtPieceSize),
            pIConverter->GetStream(pBuf, nCnvtPieceSize, nCnvtSize, eCnvtPieceType))
        {
            vr.InvokeReader(eCnvtPieceType, pBuf, nCnvtSize);
            nTotalPassedToReader += nCnvtSize;
        }

        if (ePieceType == SA_FirstPiece)
            ePieceType = SA_NextPiece;
    } while (ePieceType != SA_OnePiece && ePieceType != SA_LastPiece);
    assert(pIConverter->IsEmpty());
}

/*virtual */
void ImyCursor::DescribeParamSP()
{
    if (NULL != m_handles.stmt)
    {
        assert(false); // MySQL procedure aren't supported with prepared statements
        return;
    }

    SAString sProcName = m_pCommand->CommandText();
    SAString sDbName;

    // find database name
    size_t pos = m_pCommand->CommandText().Find(_TSA('.'));
    if (SIZE_MAX == pos)
    {
        // simple procedure name without database name
        // get current database name
        SACommand cmd(m_pISAConnection->getSAConnection(),
            _TSA("select database()"));
        cmd.Execute(); cmd.FetchNext();
        sDbName = cmd.Field(1).asString();
    }
    else
    {
        sDbName = sProcName.Left(pos);
        sProcName = sProcName.Mid(pos + 1);
    }

    // read sp parameters from system tables
    SAString sSQL = _TSA("select cast(param_list as char) par, cast(returns as char) ret from mysql.proc");
    sSQL += _TSA(" where UPPER(type) in ('PROCEDURE','FUNCTION') and UPPER(db)=UPPER('") + sDbName +
        _TSA("') and UPPER(name)=UPPER('") + sProcName + _TSA("')");

    SACommand cmd(m_pISAConnection->getSAConnection(), sSQL);
    cmd.Execute();
    if (cmd.FetchNext())
    {
        // parse parameter list
        SAString sParamList = cmd.Field(1).asString();
        if (!cmd.Field(2).isNull() && cmd.Field(2).asString().GetLength() > 0)
        {
            if (sParamList.IsEmpty())
                sParamList += _TSA("RETURN RETURN_VALUE ") + cmd.Field(2).asString();
            else
                sParamList += _TSA(", RETURN RETURN_VALUE ") + cmd.Field(2).asString();
        }

        size_t nStartPos = 0, nEndPos;
        do
        {
            if (nStartPos > 0) nStartPos += 1;
            nEndPos = sParamList.Find(_TSA(','), nStartPos);

            SAString sParamDesc = sParamList.Mid(nStartPos, nEndPos == SIZE_MAX ?
                (sParamList.GetLength() - nStartPos) : (nEndPos - nStartPos));
            sParamDesc.TrimLeft(); sParamDesc.TrimRight();

            size_t nPos = sParamDesc.FindOneOf(_TSA(" \t\r\n"));
            SAString sParamName;
            SADataType_t eParamType = SA_dtString;

            if (nPos != SIZE_MAX)
            {
                SAParamDirType_t eDirType = _SA_ParamDirType_Reserved;

                sParamName = sParamDesc.Left(nPos);
                sParamDesc = sParamDesc.Mid(nPos);
                sParamDesc.TrimLeft();

                if (0 == sParamName.CompareNoCase(_TSA("OUT")))
                    eDirType = SA_ParamOutput;
                else if (0 == sParamName.CompareNoCase(_TSA("INOUT")))
                    eDirType = SA_ParamInputOutput;
                else if (0 == sParamName.CompareNoCase(_TSA("IN")))
                    eDirType = SA_ParamInput;
                else if (0 == sParamName.CompareNoCase(_TSA("RETURN")))
                    eDirType = SA_ParamReturn;

                if (eDirType == _SA_ParamDirType_Reserved)
                    eDirType = SA_ParamInput;
                else
                {
                    nPos = sParamDesc.FindOneOf(_TSA(" \t\r\n"));
                    if (nPos > 0 && nPos != SIZE_MAX)
                    {
                        sParamName = sParamDesc.Left(nPos);
                        sParamDesc = sParamDesc.Mid(nPos);
                        sParamDesc.TrimLeft();
                    }
                }

                enum enum_field_types type = FIELD_TYPE_STRING;
                size_t length = SIZE_MAX;
                int Prec = 0;	// not taken from MySQL, calculated in this function
                unsigned int decimals = 0;
                unsigned int flags = 0;

                sParamDesc.MakeUpper();
                if (SIZE_MAX != sParamDesc.Find(_TSA("UNSIGNED")))
                    flags |= UNSIGNED_FLAG;
                if (SIZE_MAX != sParamDesc.Find(_TSA("BINARY")))
                    flags |= BINARY_FLAG;

                if (0 == sParamDesc.Left(7).CompareNoCase(_TSA("TINYINT")) ||
                    0 == sParamDesc.Left(3).CompareNoCase(_TSA("BIT")) ||
                    0 == sParamDesc.Left(4).CompareNoCase(_TSA("BOOL")))
                    type = FIELD_TYPE_TINY;
                else if (0 == sParamDesc.Left(8).CompareNoCase(_TSA("SMALLINT")))
                    type = FIELD_TYPE_SHORT;
                else if (0 == sParamDesc.Left(9).CompareNoCase(_TSA("MEDIUMINT")))
                    type = FIELD_TYPE_INT24;
                else if (0 == sParamDesc.Left(3).CompareNoCase(_TSA("INT")))
                    type = FIELD_TYPE_LONG;
                else if (0 == sParamDesc.Left(6).CompareNoCase(_TSA("BIGINT")))
                    type = FIELD_TYPE_LONGLONG;
                else if (0 == sParamDesc.Left(5).CompareNoCase(_TSA("FLOAT")))
                    type = FIELD_TYPE_FLOAT;
                else if (0 == sParamDesc.Left(4).CompareNoCase(_TSA("REAL")) ||
                    0 == sParamDesc.Left(6).CompareNoCase(_TSA("DOUBLE")))
                    type = FIELD_TYPE_DOUBLE;
                else if (0 == sParamDesc.Left(3).CompareNoCase(_TSA("DEC")) ||
                    0 == sParamDesc.Left(7).CompareNoCase(_TSA("NUMERIC")) ||
                    0 == sParamDesc.Left(5).CompareNoCase(_TSA("FIXED")))
                {
                    type = FIELD_TYPE_DECIMAL;
                    // defaults
                    Prec = 10;
                    decimals = 0;

                    nPos = sParamDesc.Find(_TSA('('));
                    if (nPos != SIZE_MAX && nPos < sParamDesc.Find(_TSA(')')))
                    {
                        sParamDesc = sParamDesc.Mid(nPos + 1, sParamDesc.Find(_TSA(')')) - nPos - 1);
                        nPos = sParamDesc.Find(_TSA(','));
                        if (SIZE_MAX == nPos)
                            Prec = sa_toi(sParamDesc);
                        else
                        {
                            Prec = sa_toi(sParamDesc.Left(nPos));
                            decimals = sa_toi(sParamDesc.Mid(nPos + 1));
                        }
                    }
                }
                else if (0 == sParamDesc.Left(8).CompareNoCase(_TSA("DATETIME")))
                    type = FIELD_TYPE_DATETIME;
                else if (0 == sParamDesc.Left(4).CompareNoCase(_TSA("DATE")))
                    type = FIELD_TYPE_DATE;
                else if (0 == sParamDesc.Left(9).CompareNoCase(_TSA("TIMESTAMP")))
                    type = FIELD_TYPE_TIMESTAMP;
                else if (0 == sParamDesc.Left(9).CompareNoCase(_TSA("TIME")))
                    type = FIELD_TYPE_TIME;
                else if (0 == sParamDesc.Left(4).CompareNoCase(_TSA("YEAR")))
                    type = FIELD_TYPE_YEAR;
                else if (0 == sParamDesc.Left(8).CompareNoCase(_TSA("VARCHAR")) ||
                    0 == sParamDesc.Left(8).CompareNoCase(_TSA("NATIONAL")) ||
                    0 == sParamDesc.Left(4).CompareNoCase(_TSA("CHAR")))
                    type = FIELD_TYPE_VAR_STRING;
                else if (0 == sParamDesc.Left(4).CompareNoCase(_TSA("TEXT")))
                    type = FIELD_TYPE_BLOB;
                else if (0 == sParamDesc.Left(8).CompareNoCase(_TSA("TINYTEXT")))
                    type = FIELD_TYPE_VAR_STRING;
                else if (0 == sParamDesc.Left(10).CompareNoCase(_TSA("MEDIUMTEXT")))
                    type = FIELD_TYPE_MEDIUM_BLOB;
                else if (0 == sParamDesc.Left(8).CompareNoCase(_TSA("LONGTEXT")))
                    type = FIELD_TYPE_MEDIUM_BLOB;
                else if (0 == sParamDesc.Left(4).CompareNoCase(_TSA("BLOB")))
                    type = FIELD_TYPE_BLOB, flags |= BINARY_FLAG;
                else if (0 == sParamDesc.Left(8).CompareNoCase(_TSA("TINYBLOB")))
                    type = FIELD_TYPE_TINY_BLOB, flags |= BINARY_FLAG;
                else if (0 == sParamDesc.Left(10).CompareNoCase(_TSA("MEDIUMBLOB")))
                    type = FIELD_TYPE_MEDIUM_BLOB, flags |= BINARY_FLAG;
                else if (0 == sParamDesc.Left(8).CompareNoCase(_TSA("LONGBLOB")))
                    type = FIELD_TYPE_MEDIUM_BLOB, flags |= BINARY_FLAG;
                else if (0 == sParamDesc.Left(3).CompareNoCase(_TSA("SET")))
                    type = FIELD_TYPE_VAR_STRING;
                else if (0 == sParamDesc.Left(4).CompareNoCase(_TSA("ENUM")))
                    type = FIELD_TYPE_VAR_STRING;

                eParamType = ImyConnection::CnvtNativeToStd(type,
                    length, Prec, decimals, flags);

                m_pCommand->CreateParam(sParamName, eParamType,
                    type, length, Prec, decimals, eDirType);
            }

            nStartPos = nEndPos;
        } while (nStartPos != SIZE_MAX);
    }
}

/*virtual */
saCommandHandles *ImyCursor::NativeHandles()
{
    return &m_handles;
}

//////////////////////////////////////////////////////////////////////
// ImyCursorStmt implementation
//////////////////////////////////////////////////////////////////////

void ImyCursor::Bind(
    int nPlaceHolderCount,
    saPlaceHolder **ppPlaceHolders)
{
    assert(NULL != m_handles.stmt); // Use with statement API only

    bool bNeedBind = false;

    if (NULL == m_bind)
    {
        // alloc MySQL bind structures
        m_bind = mysql_bind::getInstance(m_pISAConnection->GetClientVersion(), nPlaceHolderCount);

        AllocBindBuffer(sizeof(my_bool), sizeof(unsigned long));
        bNeedBind = true;
    }

    void *pBuf = m_pParamBuffer;
    void *pInd;
    void *pSize;
    size_t nDataBufSize;
    void *pValue;

    for (int i = 0; i < nPlaceHolderCount; ++i)
    {
        SAParam &Param = *ppPlaceHolders[i]->getParam();

        SADataType_t eDataType = Param.DataType();
        SADataType_t eParamType = Param.ParamType();
        if (eParamType == SA_dtUnknown)
            eParamType = eDataType;	// assume

        IncParamBuffer(pBuf, pInd, pSize, nDataBufSize, pValue);

        m_bind->buffer_type(i + 1) = ImyConnection::CnvtStdToNative(eDataType);
        m_bind->buffer(i + 1) = (char*)pValue;
        m_bind->length(i + 1) = (unsigned long*)pInd;

        size_t *StrLen_or_IndPtr = (size_t *)pInd;

        if (Param.isNull())
        {
            m_bind->is_null(i + 1) = &my_true;
            *StrLen_or_IndPtr = (size_t)(-1);
        }
        else
        {
            m_bind->is_null(i + 1) = &my_false;
            *StrLen_or_IndPtr = InputBufferSize(Param);

            switch (eDataType)
            {
                case SA_dtUnknown:
                    throw SAException(SA_Library_Error, SA_Library_Error_UnknownParameterType, -1,
                        IDS_UNKNOWN_PARAMETER_TYPE, (const SAChar*)Param.Name());
                case SA_dtBool:
                    assert(*StrLen_or_IndPtr == sizeof(unsigned char));
                    *(unsigned char*)pValue = (unsigned char)Param.asBool();
                    m_bind->is_unsigned(i + 1) = 1;
                    break;
                case SA_dtShort:
                    assert(*StrLen_or_IndPtr == sizeof(short));
                    *(short*)pValue = Param.asShort();
                    break;
                case SA_dtUShort:
                    assert(*StrLen_or_IndPtr == sizeof(unsigned short));
                    *(unsigned short*)pValue = Param.asUShort();
                    m_bind->is_unsigned(i + 1) = 1;
                    break;
                case SA_dtLong:
                    assert(*StrLen_or_IndPtr == sizeof(long));
                    *(int*)pValue = (int)Param.asLong();
                    break;
                case SA_dtULong:
                    assert(*StrLen_or_IndPtr == sizeof(unsigned long));
                    *(unsigned int*)pValue = (unsigned int)Param.asULong();
                    m_bind->is_unsigned(i + 1) = 1;
                    break;
                case SA_dtDouble:
                    assert(*StrLen_or_IndPtr == sizeof(double));
                    *(double*)pValue = Param.asDouble();
                    break;
                case SA_dtNumeric:
                {
                    assert(*StrLen_or_IndPtr == SA_MYSQL_MAX_NUMERIC_LENGTH);
                    SAString val = Param.asNumeric();
                    ::strcpy((char*)pValue, val.GetMultiByteChars());
                }
                break;
                case SA_dtDateTime:
                    assert(*StrLen_or_IndPtr == sizeof(MYSQL_TIME));
                    ImyConnection::CnvtDateTimeToInternal(
                        Param.asDateTime(), *(MYSQL_TIME*)pValue);
                    break;
                case SA_dtInterval:
                {
                    assert(*StrLen_or_IndPtr == SA_MYSQL_MAX_INTERVAL_LENGTH);
                    SAString val = Param.asInterval();
                    ::strcpy((char*)pValue, val.GetMultiByteChars());
                }
                break;
                case SA_dtString:
                {
                    // we will directly bind parameter buffer
#ifdef SA_UNICODE_WITH_UTF8
                    m_bind->buffer(i + 1) = (char*)Param.setAsString().GetUTF8Chars();
                    *StrLen_or_IndPtr = Param.setAsString().GetUTF8CharsLength();
#else
                    m_bind->buffer(i + 1) = (char*)Param.setAsString().GetMultiByteChars();
                    *StrLen_or_IndPtr = Param.setAsString().GetMultiByteCharsLength();
#endif
                }
                break;
                case SA_dtBytes:
                {
                    // we will directly bind parameter buffer
                    m_bind->buffer(i + 1) = (void*)((const void*)Param.setAsString());
                    *StrLen_or_IndPtr = Param.setAsString().GetBinaryLength();
                }
                break;
                case SA_dtLongBinary:
                case SA_dtBLob:
                case SA_dtLongChar:
                case SA_dtCLob:
                    // We will use mysql_stmt_send_long_data at BindLongs()
                    break;
                default:
                    assert(false);
            }
        }
    }

    if (bNeedBind && g_myAPI.mysql_stmt_bind_param(m_handles.stmt, m_bind->get_bind()))
        Check(m_handles.stmt);
}

void ImyCursor::SendBlob(unsigned int nParam, SAParam &Param)
{
    assert(NULL != m_handles.stmt); // Use with statement API only

    size_t nActualWrite;
    SAPieceType_t ePieceType = SA_FirstPiece;
    void *pBuf;

    while ((nActualWrite = Param.InvokeWriter(ePieceType,
        ImyConnection::MaxLongPiece, pBuf)) != 0)
    {
        if (g_myAPI.mysql_stmt_send_long_data(m_handles.stmt,
            nParam, (const char*)pBuf, (unsigned long)nActualWrite))
            Check(m_handles.stmt);

        if (ePieceType == SA_LastPiece)
            break;
    }
}

void ImyCursor::SendClob(unsigned int nParam, SAParam &Param)
{
    assert(NULL != m_handles.stmt); // Use with statement API only

    size_t nActualWrite;
    SAPieceType_t ePieceType = SA_FirstPiece;

#ifdef SA_UNICODE
    SAUnicode2MultibyteConverter Converter;
#ifdef SA_UNICODE_WITH_UTF8
    Converter.SetUseUTF8();
#endif
    size_t nCnvtPieceSize = 0l;
#endif

    void *pBuf;
    while ((nActualWrite = Param.InvokeWriter(ePieceType,
        ImyConnection::MaxLongPiece, pBuf)) != 0)
    {
#ifdef SA_UNICODE
        Converter.PutStream((unsigned char*)pBuf, nActualWrite, ePieceType);
        size_t nCnvtSize;
        SAPieceType_t eCnvtPieceType;
        // smart while: initialize nCnvtPieceSize before calling pIConverter->GetStream
        while (nCnvtPieceSize = nActualWrite,
            Converter.GetStream((unsigned char*)pBuf, nCnvtPieceSize, nCnvtSize, eCnvtPieceType))
        {
            if (g_myAPI.mysql_stmt_send_long_data(m_handles.stmt,
                nParam, (const char*)pBuf, (unsigned long)nCnvtSize))
                Check(m_handles.stmt);
        }
#else
        if (g_myAPI.mysql_stmt_send_long_data(m_handles.stmt,
            nParam, (const char*)pBuf, (unsigned long)nActualWrite))
            Check(m_handles.stmt);
#endif // SA_UNICODE

        if (ePieceType == SA_LastPiece)
            break;
}
}

// executes statement (also binds parameters if needed)
void ImyCursor::Execute_Stmt(
    int nPlaceHolderCount,
    saPlaceHolder **ppPlaceHolders)
{
    assert(NULL != m_handles.stmt); // Use with statement API only

    if (nPlaceHolderCount)
    {
        Bind(nPlaceHolderCount, ppPlaceHolders);

        // Send longs
        for (int i = 0; i < nPlaceHolderCount; ++i)
        {
            SAParam &Param = *ppPlaceHolders[i]->getParam();

            SADataType_t eDataType = Param.DataType();

            if (isLongOrLob(eDataType) && !Param.isNull())
            {
                switch (eDataType)
                {
                case SA_dtLongBinary:
                case SA_dtBLob:
                    SendBlob(i, Param);
                    break;
                case SA_dtLongChar:
                case SA_dtCLob:
                    SendClob(i, Param);
                    break;
                default:
                    assert(false);
                    break;
                }
            }
        }
    }

    SAString sOption = m_pCommand->Option(SACMD_PREFETCH_ROWS);
    if (!sOption.IsEmpty())
    {
        unsigned long prefetch_rows = (unsigned long)sa_tol(sOption);
        if (prefetch_rows)
        {
            unsigned long type = (unsigned long)CURSOR_TYPE_READ_ONLY;
            g_myAPI.mysql_stmt_attr_set(m_handles.stmt,
                STMT_ATTR_CURSOR_TYPE, (void*)&type);
            g_myAPI.mysql_stmt_attr_set(m_handles.stmt,
                STMT_ATTR_PREFETCH_ROWS, (void*)&prefetch_rows);
        }
    }

    if (g_myAPI.mysql_stmt_execute(m_handles.stmt))
        Check(m_handles.stmt);

    m_bResultSetCanBe = true;
}

void ImyCursor::DescribeFields_Stmt(
    DescribeFields_cb_t fn)
{
    assert(NULL != m_handles.stmt); // Use with statement API only
    if (NULL == m_handles.stmt)
        return;

    MYSQL_RES* result = g_myAPI.mysql_stmt_result_metadata(m_handles.stmt);
    if (NULL == result)
    {
        Check(m_handles.stmt);
        return;
    }

    unsigned int cFields = g_myAPI.mysql_stmt_field_count(m_handles.stmt);

    myConnectionHandles *pConH =
        (myConnectionHandles *)m_pCommand->Connection()->NativeHandles();
    MY_CHARSET_INFO cs;
    memset(&cs, 0, sizeof(cs));
    if (g_myAPI.mysql_get_character_set_info)
        g_myAPI.mysql_get_character_set_info(pConH->mysql, &cs);

    // realloc MySQL field bind structures
    if (NULL != m_result)
        delete m_result;
    m_result = mysql_bind::getInstance(
        m_pISAConnection->GetClientVersion(), cFields);

    mysql_field* fieldIter = mysql_field::getInstance(
        m_pISAConnection->GetClientVersion(),
        g_myAPI.mysql_fetch_fields(result));

    for (unsigned int iField = 0; iField < cFields; ++iField)
    {
        enum enum_field_types type = fieldIter->type();
        unsigned long length = fieldIter->length();
        unsigned int decimals = fieldIter->decimals();
        unsigned int flags = fieldIter->flags();
        SAString name;
#ifdef SA_UNICODE_WITH_UTF8
        name.SetUTF8Chars(fieldIter->name());
#else
        name = SAString(fieldIter->name());
#endif
        int Prec;	// calculated by ImyConnection::CnvtNativeToStd
        SADataType_t eDataType = ImyConnection::CnvtNativeToStd(
            type,
            (unsigned int)length,
            Prec,
            decimals,
            flags);

        if (eDataType == SA_dtString && cs.mbmaxlen)
            length /= cs.mbmaxlen;

        (m_pCommand->*fn)(
            name,
            eDataType,
            type,
            length,
            Prec,
            decimals,
            flags & NOT_NULL_FLAG,
            (int)cFields);

        fieldIter->advance();
    }
    delete fieldIter;

    g_myAPI.mysql_free_result(result);
}

/*virtual */
bool ImyCursor::ConvertIndicator(
    int/* nPos*/,	// 1-based
    int/* nNotConverted*/,
    SAValueRead &vr,
    ValueType_t/* eValueType*/,
    void *pInd, size_t nIndSize,
    void *pSize, size_t nSizeSize,
    size_t &nRealSize,
    int nBulkReadingBufPos) const
{
    // Used with statement API only
    assert(NULL != m_handles.stmt);

    assert(nIndSize == sizeof(my_bool));
    *vr.m_pbNull = ((my_bool*)pInd)[nBulkReadingBufPos] != 0;

    if (*vr.m_pbNull)
        return true;	// converted, no need to convert size

    // try all integer types
    assert(nSizeSize == sizeof(unsigned long));
    nRealSize = ((unsigned long*)pSize)[nBulkReadingBufPos];

    return true;
}

/*virtual */
size_t ImyCursor::InputBufferSize(
    const SAParam &Param) const
{
    assert(NULL != m_handles.stmt); // Use with statement API only

    switch (Param.DataType())
    {
    case SA_dtBool:
        return sizeof(unsigned char);
    case SA_dtString:
    case SA_dtBytes:
        return 0; // we will directly use parameter buffer;
    case SA_dtInterval:
        return SA_MYSQL_MAX_INTERVAL_LENGTH;
    case SA_dtLongBinary:
    case SA_dtLongChar:
    case SA_dtBLob:
    case SA_dtCLob:
        return 0; //we will use mysql_stmt_send_long_data
    case SA_dtNumeric:
        return SA_MYSQL_MAX_NUMERIC_LENGTH;
    case SA_dtDateTime:
        return sizeof(MYSQL_TIME);
    case SA_dtCursor:
        assert(false);
        break;
    default:
        break;
    }

    return ISACursor::InputBufferSize(Param);
}

/*virtual */
size_t ImyCursor::OutputBufferSize(
    SADataType_t eDataType,
    size_t nDataSize) const
{
    assert(NULL != m_handles.stmt); // Use with statement API only

    switch (eDataType)
    {
    case SA_dtBool:
        return sizeof(unsigned char);
    case SA_dtNumeric:
        return SA_MYSQL_MAX_NUMERIC_LENGTH;
    case SA_dtString:
    case SA_dtInterval:
        return nDataSize + 1;	// always allocate space for NULL
    case SA_dtDateTime:
        return sizeof(MYSQL_TIME);
    case SA_dtLongBinary:
    case SA_dtLongChar:
        return 0;
    default:
        break;
    }

    return ISACursor::OutputBufferSize(eDataType, nDataSize);
}

/*virtual */
void ImyCursor::SetFieldBuffer(
    int nCol,	// 1-based
    void * pInd,
    size_t nIndSize,
    void * pSize,
    size_t nSizeSize,
    void * pValue,
    size_t nValueSize)
{
    assert(NULL != m_handles.stmt); // Use with statement API only

    assert(nIndSize == sizeof(my_bool));
    if (nIndSize != sizeof(my_bool))
        return;

    assert(nSizeSize == sizeof(unsigned long));
    if (nSizeSize != sizeof(unsigned long))
        return;

    if (NULL == m_result)
        return;

    SAField &Field = m_pCommand->Field(nCol);

    m_result->buffer(nCol) = (char*)pValue;
    m_result->length(nCol) = (unsigned long*)pSize;
    m_result->is_null(nCol) = (my_bool*)pInd;

    switch (Field.FieldType())
    {
    case SA_dtUnknown:
        throw SAException(SA_Library_Error, SA_Library_Error_UnknownColumnType, -1,
            IDS_UNKNOWN_COLUMN_TYPE, (const SAChar*)Field.Name());
    case SA_dtBool:
        assert(nValueSize == sizeof(unsigned char));
        m_result->buffer_type(nCol) = MYSQL_TYPE_TINY;
        m_result->is_unsigned(nCol) = my_true;
        break;
    case SA_dtShort:
        assert(nValueSize == sizeof(short));
        m_result->buffer_type(nCol) = MYSQL_TYPE_SHORT;
        break;
    case SA_dtUShort:
        assert(nValueSize == sizeof(unsigned short));
        m_result->buffer_type(nCol) = MYSQL_TYPE_SHORT;
        m_result->is_unsigned(nCol) = my_true;
        break;
    case SA_dtLong:
        assert(nValueSize == sizeof(long));
        m_result->buffer_type(nCol) = MYSQL_TYPE_LONG;
        break;
    case SA_dtULong:
        m_result->buffer_type(nCol) = MYSQL_TYPE_LONG;
        m_result->is_unsigned(nCol) = my_true;
        assert(nValueSize == sizeof(unsigned long));
        break;
    case SA_dtDouble:
        assert(nValueSize == sizeof(double));
        m_result->buffer_type(nCol) = MYSQL_TYPE_DOUBLE;
        break;
    case SA_dtNumeric:
        assert(nValueSize == SA_MYSQL_MAX_NUMERIC_LENGTH);
        m_result->buffer_type(nCol) = MYSQL_TYPE_STRING; // MYSQL_TYPE_DECIMAL isn't supported by 4.1.x
        m_result->buffer_length(nCol) = (unsigned long)nValueSize;
        break;
    case SA_dtDateTime:
        assert(nValueSize == sizeof(MYSQL_TIME));
        m_result->buffer_type(nCol) = MYSQL_TYPE_DATETIME;
        break;
    case SA_dtBytes:
        assert(nValueSize == (size_t)Field.FieldSize());
        m_result->buffer_type(nCol) = MYSQL_TYPE_STRING;
        m_result->buffer_length(nCol) = (unsigned long)nValueSize;
        break;
    case SA_dtString:
        assert(nValueSize == (size_t)Field.FieldSize() + 1);
        m_result->buffer_type(nCol) = MYSQL_TYPE_STRING;
        m_result->buffer_length(nCol) = (unsigned long)nValueSize;
        break;
    case SA_dtLongBinary:
        assert(nValueSize == 0);
        m_result->buffer_type(nCol) = MYSQL_TYPE_BLOB;
        m_result->buffer_length(nCol) = (unsigned long)nValueSize;
        break;
    case SA_dtLongChar:
        assert(nValueSize == 0);
        m_result->buffer_type(nCol) = MYSQL_TYPE_BLOB;
        m_result->buffer_length(nCol) = (unsigned long)nValueSize;
        break;
    case SA_dtInterval:
        assert(nValueSize == (size_t)Field.FieldSize() + 1);
        m_result->buffer_type(nCol) = MYSQL_TYPE_STRING;
        m_result->buffer_length(nCol) = (unsigned long)nValueSize;
        break;
    default:
        assert(false);	// unknown type
    }
}

bool ImyCursor::FetchNext_Stmt()
{
    assert(NULL != m_handles.stmt); // Use with statement API only

    switch (g_myAPI.mysql_stmt_fetch(m_handles.stmt))
    {
    case MYSQL_DATA_TRUNCATED:
        // BLOB or TEXT
    case 0:
        ++m_currentRow;
        ConvertSelectBufferToFields(0);
        return true;

    case 1:
        Check(m_handles.stmt);
        break;

    case MYSQL_NO_DATA:
    default:
        g_myAPI.mysql_stmt_free_result(m_handles.stmt);
        m_bResultSetCanBe = false;
        break;
    }

    return false;
}

void ImyCursor::ReadLongOrLOB_Stmt(
    ValueType_t /*eValueType*/,
    SAValueRead &vr,
    void * /*pValue*/,
    size_t/* nFieldBufSize*/,
    saLongOrLobReader_t fnReader,
    size_t nReaderWantedPieceSize,
    void *pAddlData)
{
    assert(NULL != m_handles.stmt); // Use with statement API only

    int nPos = ((SAField &)vr).Pos();
    assert(nPos > 0 && nPos <= m_pCommand->FieldCount());

    // get long data size
    unsigned long nLongSize = *(m_result->length(nPos));

    SADummyConverter DummyConverter;
    ISADataConverter *pIConverter = &DummyConverter;
    unsigned int nCnvtLongSizeMax = (unsigned int)nLongSize;
#ifdef SA_UNICODE
    SAMultibyte2UnicodeConverter Multibyte2UnicodeConverter;
    if( SA_dtLongChar == vr.DataType() ||
        SA_dtCLob == vr.DataType())
    {
        pIConverter = &Multibyte2UnicodeConverter;
#ifdef SA_UNICODE_WITH_UTF8
        Multibyte2UnicodeConverter.SetUseUTF8();
#endif
        // in the worst case each byte can represent a Unicode character
        nCnvtLongSizeMax = nLongSize*sizeof(wchar_t);
    }
#endif

    unsigned char* pBuf;
    size_t nPieceSize = vr.PrepareReader(
        sa_max(nCnvtLongSizeMax, nLongSize),	// real size is nCnvtLongSizeMax
        ImyConnection::MaxLongPiece,
        pBuf,
        fnReader,
        nReaderWantedPieceSize,
        pAddlData);
    assert(nPieceSize > 0 && nPieceSize <= ImyConnection::MaxLongPiece);

    size_t nCnvtPieceSize = nPieceSize;
    SAPieceType_t ePieceType = SA_FirstPiece;
    size_t nTotalRead = 0l;
    size_t nTotalPassedToReader = 0l;
    do
    {
        if (nLongSize)	// known
            nPieceSize = sa_min(nPieceSize, (nLongSize - nTotalRead));
        else
        {
            vr.InvokeReader(SA_LastPiece, pBuf, 0);
            break;
        }

        m_result->buffer(nPos) = pBuf;
        m_result->buffer_length(nPos) = (unsigned long)nPieceSize;

        if (g_myAPI.mysql_stmt_fetch_column(m_handles.stmt,
            m_result->get_bind(nPos), nPos - 1, (unsigned long)nTotalRead))
            Check(m_handles.stmt);

        size_t actual_read = *m_result->length(nPos) > nPieceSize ?
        nPieceSize : (*m_result->length(nPos));

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

        pIConverter->PutStream(pBuf, actual_read, ePieceType);
        size_t nCnvtSize;
        SAPieceType_t eCnvtPieceType;
        // smart while: initialize nCnvtPieceSize before calling pIConverter->GetStream
        while (nCnvtPieceSize = (nCnvtLongSizeMax ? // known
            sa_min(nCnvtPieceSize, nCnvtLongSizeMax - nTotalPassedToReader) : nCnvtPieceSize),
            pIConverter->GetStream(pBuf, nCnvtPieceSize, nCnvtSize, eCnvtPieceType))
        {
            vr.InvokeReader(eCnvtPieceType, pBuf, nCnvtSize);
            nTotalPassedToReader += nCnvtSize;
        }

        if (SA_FirstPiece == ePieceType)
            ePieceType = SA_NextPiece;
    } while (nTotalRead < nLongSize);
}

/*virtual */
void ImyCursor::ConvertString(SAString &String, const void *pData, size_t nRealSize)
{
#ifdef SA_UNICODE_WITH_UTF8
    String.SetUTF8Chars((const char*)pData, nRealSize);
#else
    ISACursor::ConvertString(String, pData, nRealSize);
#endif
}

//////////////////////////////////////////////////////////////////////
// MySQL globals
//////////////////////////////////////////////////////////////////////

ISAConnection *newImyConnection(SAConnection *pSAConnection)
{
    return new ImyConnection(pSAConnection);
}

/*static */
void ImyConnection::CnvtInternalToNumeric(
    SANumeric &numeric,
    const char *sValue)
{
    numeric = SAString(sValue);
}

/*virtual */
void ImyConnection::CnvtInternalToNumeric(
    SANumeric &numeric,
    const void * pInternal,
    int nInternalSize)
{
    numeric = SAString((const char*)pInternal, nInternalSize);
}

