// db2Client.cpp: implementation of the Idb2Client class.
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <db2API.h>
#include "db2Client.h"

#if defined(SA_UNICODE) && !defined(SQLAPI_WINDOWS)
#define	TSAChar	UTF16
#else
#define	TSAChar	SAChar
#endif

// if DB2_SUPPORTS_SQL_C_NUMERIC is not defined externally
// try to define it based on DB2 documentation

#if !defined(DB2_SUPPORTS_SQL_C_NUMERIC)
// From DB2 documentation: 
// SQL_C_NUMERIC is supported on 32-bit Windows only
#if defined(SQLAPI_WINDOWS) && !defined(DB2_SUPPORTS_SQL_C_NUMERIC_UNIX)
#define DB2_SUPPORTS_SQL_C_NUMERIC
typedef SQL_NUMERIC_STRUCT DB2_SQL_NUMERIC_STRUCT;
#else
typedef char DB2_SQL_NUMERIC_STRUCT[1024];
#endif	// !defined(SQLAPI_WINDOWS)
#endif


//////////////////////////////////////////////////////////////////////
// Idb2Connection Class
//////////////////////////////////////////////////////////////////////

class Idb2Connection : public ISAConnection
{
	enum MaxLong
	{
		MaxLongAtExecSize = 0x7fffffff+SQL_LEN_DATA_AT_EXEC(0)
	};
	friend class Idb2Cursor;

	db2ConnectionHandles m_handles;

	static void Check(
		SQLRETURN return_code,
		SQLSMALLINT HandleType,
		SQLHANDLE Handle);
	SQLINTEGER LenDataAtExec();
	void issueIsolationLevel(
		SAIsolationLevel_t eIsolationLevel);
	static SADataType_t CnvtNativeToStd(
		int nNativeType,
		SQLULEN ColumnSize,
		SQLLEN nPrec,
		int nScale);
	static SQLSMALLINT CnvtStdToNative(
		SADataType_t eDataType);
	static SQLSMALLINT CnvtStdToNativeValueType(
		SADataType_t eDataType);
	static void CnvtNumericToInternal(
		const SANumeric &numeric,
		DB2_SQL_NUMERIC_STRUCT &Internal,
		SQLLEN &StrLen_or_IndPtr);
	static void CnvtInternalToDateTime(
		SADateTime &date_time,
		const TIMESTAMP_STRUCT &Internal);
	static void CnvtDateTimeToInternal(
		const SADateTime &date_time,
		TIMESTAMP_STRUCT &Internal);

protected:
	virtual ~Idb2Connection();

public:
	Idb2Connection(SAConnection *pSAConnection);

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

Idb2Connection::Idb2Connection(
	SAConnection *pSAConnection) : ISAConnection(pSAConnection)
{
	Reset();
}

Idb2Connection::~Idb2Connection()
{
}

/*virtual */
void Idb2Connection::InitializeClient()
{
	::AddDB2Support(m_pSAConnection);
}

/*virtual */
void Idb2Connection::UnInitializeClient()
{
	if( SAGlobals::UnloadAPI() ) ::ReleaseDB2Support();
}

SQLINTEGER Idb2Connection::LenDataAtExec()
{
	SQLSMALLINT retlen = 0;
	SAChar szValue[10];
	Check(g_db2API.SQLGetInfo(m_handles.m_hdbc, SQL_NEED_LONG_DATA_LEN,
		szValue, 10, &retlen),
		SQL_HANDLE_DBC, m_handles.m_hdbc);
	if(retlen > 0 && (*szValue == _TSA('Y') || *szValue == _TSA('y')))
		return SQL_LEN_DATA_AT_EXEC(MaxLongAtExecSize);

	return SQL_DATA_AT_EXEC;
}

/*virtual */
void Idb2Connection::CnvtInternalToDateTime(
	SADateTime &date_time,
	const void *pInternal,
	int nInternalSize)
{
	if(nInternalSize != int(sizeof(TIMESTAMP_STRUCT)))
		return;
	CnvtInternalToDateTime(date_time, *(const TIMESTAMP_STRUCT*)pInternal);
}

/*static */
void Idb2Connection::CnvtInternalToDateTime(
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

/*virtual */
void Idb2Connection::CnvtInternalToInterval(
	SAInterval & /*interval*/,
	const void * /*pInternal*/,
	int /*nInternalSize*/)
{
	assert(false);
}

/*static */
void Idb2Connection::CnvtDateTimeToInternal(
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
void Idb2Connection::CnvtInternalToCursor(
	SACommand * /*pCursor*/,
	const void * /*pInternal*/)
{
	assert(false);
}

/*virtual */
long Idb2Connection::GetClientVersion() const
{
	if(g_nDB2DLLVersionLoaded == 0)	// has not been detected
	{
		if(IsConnected())
		{
			SAChar szInfoValue[1024];
			SQLSMALLINT cbInfoValue;
			g_db2API.SQLGetInfo(m_handles.m_hdbc, SQL_DRIVER_VER, szInfoValue, 1024, &cbInfoValue);
			szInfoValue[cbInfoValue/sizeof(TSAChar)] = 0;

			SAString sInfo;
#ifdef SA_UNICODE
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

	return g_nDB2DLLVersionLoaded;
}

/*virtual */
long Idb2Connection::GetServerVersion() const
{
	assert(m_handles.m_hdbc);

	SAChar szInfoValue[1024];
	SQLSMALLINT cbInfoValue;
	g_db2API.SQLGetInfo(m_handles.m_hdbc, SQL_DBMS_VER, szInfoValue, 1024, &cbInfoValue);
	szInfoValue[cbInfoValue/sizeof(TSAChar)] = 0;

	SAString sInfo;
#ifdef SA_UNICODE
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
SAString Idb2Connection::GetServerVersionString() const
{
	assert(m_handles.m_hdbc);

	SAChar szInfoValue[1024];
	SQLSMALLINT cbInfoValue;
	g_db2API.SQLGetInfo(m_handles.m_hdbc, SQL_DBMS_NAME, szInfoValue, 1024, &cbInfoValue);
	szInfoValue[cbInfoValue/sizeof(TSAChar)] = 0;

	SAString sInfo;
#ifdef SA_UNICODE
	sInfo.SetUTF16Chars(szInfoValue);
#else
	sInfo = szInfoValue;
#endif

	SAString s = sInfo;
	s += _TSA(" Release ");

	g_db2API.SQLGetInfo(m_handles.m_hdbc, SQL_DBMS_VER, szInfoValue, 1024, &cbInfoValue);
	szInfoValue[cbInfoValue/sizeof(TSAChar)] = 0;

#ifdef SA_UNICODE
	sInfo.SetUTF16Chars(szInfoValue);
#else
	sInfo = szInfoValue;
#endif

	s += sInfo;

	return s;
}

#define SA_ODBC_MAX_MESSAGE_LENGTH	4096

/*static */
void Idb2Connection::Check(
	SQLRETURN return_code,
	SQLSMALLINT HandleType,
	SQLHANDLE Handle)
{
	if(return_code == SQL_SUCCESS)
		return;
	if(return_code == SQL_SUCCESS_WITH_INFO)
		return;

	SQLTCHAR Sqlstate[5+1];
	SQLINTEGER NativeError = SQLINTEGER(0), NativeError2;
	SQLTCHAR szMsg[SA_ODBC_MAX_MESSAGE_LENGTH+1];
	SAString sMsg;
	SQLSMALLINT TextLength;

	SQLSMALLINT i = SQLSMALLINT(1);
	SQLRETURN rc;
	SAException* pNested = NULL;

	do
	{
		Sqlstate[0] = '\0';
		szMsg[0] ='\0';

		rc = g_db2API.SQLGetDiagRec(HandleType, Handle,
			i++, Sqlstate, &NativeError2,
			szMsg, SA_ODBC_MAX_MESSAGE_LENGTH, &TextLength);

		if( SQL_SUCCESS == rc || SQL_SUCCESS_WITH_INFO == rc )
		{
			if (sMsg.GetLength() > 0)
				pNested = new SAException(pNested, SA_DBMS_API_Error, NativeError, -1, sMsg);
			
			NativeError = NativeError2;

#ifdef SA_UNICODE
			SAString s; s.SetUTF16Chars(Sqlstate);
			sMsg += s;
#else
			sMsg += Sqlstate;
#endif
			sMsg += _TSA(" ");
#ifdef SA_UNICODE
			s.SetUTF16Chars(szMsg);
			sMsg += s;
#else
			sMsg += szMsg;
#endif
		}
	}
	while( SQL_SUCCESS == rc );


	if( SQL_SUCCESS != rc &&
		SQL_SUCCESS_WITH_INFO != rc &&
		SQL_NO_DATA != rc )
	{
		if( ! sMsg.IsEmpty() )
			sMsg += _TSA("\n");
		if( ! NativeError )
			NativeError = return_code;
		sMsg += _TSA("rc != SQL_SUCCESS");
	}

	throw SAException(pNested, SA_DBMS_API_Error, NativeError, -1, sMsg);
}

/*virtual */
bool Idb2Connection::IsConnected() const
{
	return (m_handles.m_hdbc ? true:false);
}

/*virtual */
bool Idb2Connection::IsAlive() const
{
	try
	{
		SACommand cmd(m_pSAConnection,
			_TSA("select granteetype from syscat.dbauth where grantee=user"));
		cmd.Execute();
	}
	catch(SAException&)
	{
		return false;
	}

	return true;
}

/*virtual */
void Idb2Connection::Connect(
	const SAString &sDBString,
	const SAString &sUserID,
	const SAString &sPassword,
	saConnectionHandler_t fHandler)
{
	assert(!m_handles.m_hevn);
	assert(!m_handles.m_hdbc);

	g_db2API.SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_handles.m_hevn);
	assert(m_handles.m_hevn);
	try
	{
		Check(g_db2API.SQLSetEnvAttr(m_handles.m_hevn,
			SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0),
			SQL_HANDLE_ENV, m_handles.m_hevn);
		Check(g_db2API.SQLAllocHandle(SQL_HANDLE_DBC, m_handles.m_hevn,
			&m_handles.m_hdbc), SQL_HANDLE_ENV, m_handles.m_hevn);

		/* this doesn't work
		if( sUserID.IsEmpty() ) // trusted connection
		Check(g_db2API.SQLSetConnectAttr(m_handles.m_hdbc,
		SQL_ATTR_USE_TRUSTED_CONTEXT, (void*)SQL_TRUE, SQL_IS_INTEGER),
		SQL_HANDLE_DBC, m_handles.m_hdbc);
		*/

		if( NULL != fHandler )
			fHandler(*m_pSAConnection, SA_PreConnectHandler);

		/*
		if( SIZE_MAX == sDBString.Find(_TSA('=')) )	// it's not a valid connection string, but it can be DSN name
		Check(g_db2API.SQLConnect(m_handles.m_hdbc, 
		#ifdef SA_UNICODE
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
		*/
		{
			SAString s = (SIZE_MAX == sDBString.Find(_TSA('=')) ? _TSA("DSN="):_TSA(""));
			s += sDBString;

			if( sUserID.IsEmpty())
				s+= _TSA(";Trusted_Connection=Yes");
			else
			{
				s += _TSA(";UID=");
				s += sUserID;
			}
			if( ! sPassword.IsEmpty())
			{
				s += _TSA(";PWD=");
				s += sPassword;
			}

			Check(g_db2API.SQLDriverConnect(m_handles.m_hdbc, NULL,
#ifdef SA_UNICODE
				(SQLTCHAR *)s.GetUTF16Chars(),
#else
				(SQLTCHAR *)(const SAChar*)s,
#endif
				SQL_NTS, NULL, 0, 0, SQL_DRIVER_NOPROMPT),
				SQL_HANDLE_DBC, m_handles.m_hdbc);
		}

		if( NULL != fHandler )
			fHandler(*m_pSAConnection, SA_PostConnectHandler);
	}
	catch(SAException &)
	{
		// clean up
		if(m_handles.m_hdbc)
		{
			g_db2API.SQLFreeHandle(SQL_HANDLE_DBC, m_handles.m_hdbc);
			m_handles.m_hdbc = (SQLHDBC)NULL;
		}
		if(m_handles.m_hevn)
		{
			g_db2API.SQLFreeHandle(SQL_HANDLE_ENV, m_handles.m_hevn);
			m_handles.m_hevn = (SQLHENV)NULL;
		}

		throw;
	}
}

/*virtual */
void Idb2Connection::Disconnect()
{
	assert(m_handles.m_hevn);
	assert(m_handles.m_hdbc);

	Check(g_db2API.SQLDisconnect(m_handles.m_hdbc),
		SQL_HANDLE_DBC, m_handles.m_hdbc);
	Check(g_db2API.SQLFreeHandle(SQL_HANDLE_DBC, m_handles.m_hdbc),
		SQL_HANDLE_DBC, m_handles.m_hdbc);
	m_handles.m_hdbc = (SQLHDBC)NULL;
	Check(g_db2API.SQLFreeHandle(SQL_HANDLE_ENV, m_handles.m_hevn),
		SQL_HANDLE_ENV, m_handles.m_hevn);
	m_handles.m_hevn = (SQLHENV)NULL;
}

/*virtual */
void Idb2Connection::Destroy()
{
	assert(m_handles.m_hevn);
	assert(m_handles.m_hdbc);

	g_db2API.SQLDisconnect(m_handles.m_hdbc);
	g_db2API.SQLFreeHandle(SQL_HANDLE_DBC, m_handles.m_hdbc);
	m_handles.m_hdbc = (SQLHDBC)NULL;
	g_db2API.SQLFreeHandle(SQL_HANDLE_ENV, m_handles.m_hevn);
	m_handles.m_hevn = (SQLHENV)NULL;
}

/*virtual */
void Idb2Connection::Reset()
{
	m_handles.m_hdbc = (SQLHDBC)NULL;
	m_handles.m_hevn = (SQLHENV)NULL;
}

/*virtual */
void Idb2Connection::Commit()
{
	assert(m_handles.m_hdbc);

	Check(g_db2API.SQLEndTran(SQL_HANDLE_DBC, m_handles.m_hdbc, SQL_COMMIT),
		SQL_HANDLE_DBC, m_handles.m_hdbc);
}

/*virtual */
void Idb2Connection::Rollback()
{
	assert(m_handles.m_hdbc);

	Check(g_db2API.SQLEndTran(SQL_HANDLE_DBC, m_handles.m_hdbc, SQL_ROLLBACK),
		SQL_HANDLE_DBC, m_handles.m_hdbc);
}

//////////////////////////////////////////////////////////////////////
// Idb2Client Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Idb2Client::Idb2Client()
{
}

Idb2Client::~Idb2Client()
{
}

ISAConnection *Idb2Client::QueryConnectionInterface(
	SAConnection *pSAConnection)
{
	return new Idb2Connection(pSAConnection);
}

//////////////////////////////////////////////////////////////////////
// Idb2Cursor Class
//////////////////////////////////////////////////////////////////////

class Idb2Cursor : public ISACursor
{
	db2CommandHandles	m_handles;

	SQLUINTEGER m_cRowsToPrefetch;	// defined in SetSelectBuffers
	SQLUINTEGER m_cRowsObtained;
	SQLUINTEGER m_cRowCurrent;

	SAString CallSubProgramSQL();

	void BindLongs();

	bool m_bResultSetCanBe;

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
    virtual bool IgnoreUnknownDataType();

public:
	Idb2Cursor(
		Idb2Connection *pIdb2Connection,
		SACommand *pCommand);
	virtual ~Idb2Cursor();

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
Idb2Cursor::Idb2Cursor(
	Idb2Connection *pIdb2Connection,
	SACommand *pCommand) :
ISACursor(pIdb2Connection, pCommand)
{
	Reset();
}

/*virtual */
Idb2Cursor::~Idb2Cursor()
{
}

/*virtual */
size_t Idb2Cursor::InputBufferSize(
	const SAParam &Param) const
{
	if(!Param.isNull())
	{
		switch(Param.DataType())
		{
		case SA_dtBool:
			return sizeof(short);	// use short data for boolean value
		case SA_dtNumeric:
			return sizeof(DB2_SQL_NUMERIC_STRUCT);	// SQL_C_NUMERIC or SQL_C_CHAR[]
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
SADataType_t Idb2Connection::CnvtNativeToStd(
	int dbtype,
	SQLULEN/* ColumnSize*/,
	SQLLEN prec,
	int scale)
{
	SADataType_t eDataType = SA_dtUnknown;

	switch(dbtype)
	{
	case SQL_WCHAR:
	case SQL_WVARCHAR:
	case SQL_CHAR:		// Character string of fixed length
	case SQL_VARCHAR:	// Variable-length character string
		eDataType = SA_dtString;
		break;
	case SQL_BINARY:
	case SQL_VARBINARY:
		eDataType = SA_dtBytes;
		break;
	case SQL_WLONGVARCHAR:
	case SQL_LONGVARCHAR:	// Variable-length character data
		eDataType = SA_dtLongChar;
		break;
	case SQL_LONGVARBINARY: // Variable-length binary data
		eDataType = SA_dtLongBinary;
		break;
	case SQL_DECIMAL:
	case SQL_NUMERIC:
		if(scale <= 0)	// check for exact type
		{
			if(prec < 5)
				eDataType = SA_dtShort;
			else if(prec < 10)
				eDataType = SA_dtLong;
			else
				eDataType = SA_dtNumeric;
		}
		else
			eDataType = SA_dtNumeric;
		break;
	case SQL_DECFLOAT:
		eDataType = SA_dtString;
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
	case SQL_BLOB:
		eDataType = SA_dtBLob;
		break;
	case SQL_CLOB:
	case SQL_DBCLOB:
		// Support for graphic types
	case SQL_GRAPHIC:
	case SQL_VARGRAPHIC:
	case SQL_LONGVARGRAPHIC:
		eDataType = SA_dtCLob;
		break;
	case SQL_XML:
		eDataType = SA_dtCLob;
		break;
	default:
        eDataType = SA_dtUnknown;
        break;
    }

	return eDataType;
}

/*static */
SQLSMALLINT Idb2Connection::CnvtStdToNativeValueType(SADataType_t eDataType)
{
	SQLSMALLINT ValueType;

	switch(eDataType)
	{
	case SA_dtUnknown:
		throw SAException(SA_Library_Error, SA_Library_Error_UnknownDataType, -1, IDS_UNKNOWN_DATA_TYPE);
	case SA_dtBool:
		ValueType = SQL_C_SSHORT;
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
#if defined(DB2_SUPPORTS_SQL_C_NUMERIC)
		ValueType = SQL_C_NUMERIC;
#else
		ValueType = SQL_C_CHAR;
#endif
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
SQLSMALLINT Idb2Connection::CnvtStdToNative(SADataType_t eDataType)
{
	SQLSMALLINT dbtype;

	switch(eDataType)
	{
	case SA_dtUnknown:
		throw SAException(SA_Library_Error, SA_Library_Error_UnknownDataType, -1, IDS_UNKNOWN_DATA_TYPE);
	case SA_dtBool:
		dbtype = SQL_SMALLINT;
		break;
	case SA_dtShort:
		dbtype = SQL_SMALLINT;
		break;
	case SA_dtUShort:
		dbtype = SQL_SMALLINT;
		break;
	case SA_dtLong:
		dbtype = SQL_INTEGER;
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
		dbtype = SQL_LONGVARBINARY;
		break;
	case SA_dtLongChar:
#ifdef SA_UNICODE
		dbtype = SQL_UNICODE_LONGVARCHAR;
#else
		dbtype = SQL_LONGVARCHAR;
#endif
		break;
	case SA_dtBLob:
		dbtype = SQL_BLOB;
		break;
	case SA_dtCLob:
		dbtype = SQL_CLOB;
		break;
	default:
		dbtype = 0;
		assert(false);
	}

	return dbtype;
}

/*virtual */
bool Idb2Cursor::IsOpened()
{
	return (m_handles.m_hstmt ? true:false);
}

/*virtual */
void Idb2Cursor::Open()
{
	assert(!m_handles.m_hstmt);
	Idb2Connection::Check(
		g_db2API.SQLAllocHandle(SQL_HANDLE_STMT,
		((Idb2Connection*)m_pISAConnection)->m_handles.m_hdbc, &m_handles.m_hstmt),
		SQL_HANDLE_DBC, ((Idb2Connection*)m_pISAConnection)->m_handles.m_hdbc);
	assert(m_handles.m_hstmt);

	if( isSetScrollable() )
	{		
		g_db2API.SQLSetStmtAttr(m_handles.m_hstmt,
			SQL_ATTR_CONCURRENCY, (SQLPOINTER)SQL_CONCUR_LOCK, SQL_IS_INTEGER);
		g_db2API.SQLSetStmtAttr(m_handles.m_hstmt,
			SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_DYNAMIC, SQL_IS_INTEGER);		
	}

	SAString sOption = m_pCommand->Option(_TSA("SQL_ATTR_CONCURRENCY"));
	if( ! sOption.IsEmpty() )
	{
		SQLPOINTER attrVal = (SQLPOINTER)SQL_CONCUR_READ_ONLY;
		if( 0 == sOption.CompareNoCase(_TSA("SQL_CONCUR_READONLY")) )
			attrVal = (SQLPOINTER)SQL_CONCUR_READ_ONLY;
		else if( 0 == sOption.CompareNoCase(_TSA("SQL_CONCUR_VALUES")) )
			attrVal = (SQLPOINTER)SQL_CONCUR_VALUES;
		else if( 0 == sOption.CompareNoCase(_TSA("SQL_CONCUR_ROWVER")) )
			attrVal = (SQLPOINTER)SQL_CONCUR_ROWVER;
		else if( 0 == sOption.CompareNoCase(_TSA("SQL_CONCUR_LOCK")) )
			attrVal = (SQLPOINTER)SQL_CONCUR_LOCK;
		else
			assert(false);
		g_db2API.SQLSetStmtAttr(m_handles.m_hstmt,
			SQL_ATTR_CONCURRENCY, attrVal, SQL_IS_INTEGER);
	}

	sOption = m_pCommand->Option(_TSA("SQL_ATTR_CURSOR_TYPE"));
	if( ! sOption.IsEmpty() )
	{
		SQLPOINTER attrVal = (SQLPOINTER)SQL_CURSOR_FORWARD_ONLY;
		if( 0 == sOption.CompareNoCase(_TSA("SQL_CURSOR_FORWARD_ONLY")) )
			attrVal = (SQLPOINTER)SQL_CURSOR_FORWARD_ONLY;
		else if( 0 == sOption.CompareNoCase(_TSA("SQL_CURSOR_KEYSET_DRIVEN")) )
			attrVal = (SQLPOINTER)SQL_CURSOR_KEYSET_DRIVEN;
		else if( 0 == sOption.CompareNoCase(_TSA("SQL_CURSOR_DYNAMIC")) )
			attrVal = (SQLPOINTER)SQL_CURSOR_DYNAMIC;
		else if( 0 == sOption.CompareNoCase(_TSA("SQL_CURSOR_STATIC")) )
			attrVal = (SQLPOINTER)SQL_CURSOR_STATIC;
		else
			assert(false);
		g_db2API.SQLSetStmtAttr(m_handles.m_hstmt,
			SQL_ATTR_CURSOR_TYPE, attrVal, SQL_IS_INTEGER);
	}

	sOption = m_pCommand->Option(_TSA("SQL_ATTR_CURSOR_SCROLLABLE"));
	if( ! sOption.IsEmpty() )
	{
		SQLPOINTER attrVal = (SQLPOINTER)SQL_NONSCROLLABLE;
		if( 0 == sOption.CompareNoCase(_TSA("SQL_NONSCROLLABLE")) )
			attrVal = (SQLPOINTER)SQL_NONSCROLLABLE;
		else if( 0 == sOption.CompareNoCase(_TSA("SQL_SCROLLABLE")) )
			attrVal = (SQLPOINTER)SQL_SCROLLABLE;
		else
			assert(false);
		g_db2API.SQLSetStmtAttr(m_handles.m_hstmt,
			SQL_ATTR_CURSOR_SCROLLABLE, attrVal, SQL_IS_INTEGER);
	}

	sOption = m_pCommand->Option(_TSA("SQL_ATTR_CURSOR_SENSITIVITY"));
	if( ! sOption.IsEmpty() )
	{
		SQLPOINTER attrVal = (SQLPOINTER)SQL_UNSPECIFIED;
		if( 0 == sOption.CompareNoCase(_TSA("SQL_UNSPECIFIED")) )
			attrVal = (SQLPOINTER)SQL_UNSPECIFIED;
		else if( 0 == sOption.CompareNoCase(_TSA("SQL_INSENSITIVE")) )
			attrVal = (SQLPOINTER)SQL_INSENSITIVE;
		else if( 0 == sOption.CompareNoCase(_TSA("SQL_SENSITIVE")) )
			attrVal = (SQLPOINTER)SQL_SENSITIVE;
		else
			assert(false);
		g_db2API.SQLSetStmtAttr(m_handles.m_hstmt,
			SQL_ATTR_CURSOR_SENSITIVITY, attrVal, SQL_IS_INTEGER);
	}

	sOption = m_pCommand->Option(_TSA("SQL_ATTR_QUERY_TIMEOUT"));
	if( ! sOption.IsEmpty() )
	{
		SQLPOINTER attrVal = (SQLPOINTER)sa_tol((const SAChar*)sOption);
		g_db2API.SQLSetStmtAttr(m_handles.m_hstmt,
			SQL_ATTR_QUERY_TIMEOUT, attrVal, SQL_IS_UINTEGER);
	}

	sOption = m_pCommand->Option(_TSA("SetCursorName"));
	if( ! sOption.IsEmpty() )
		Idb2Connection::Check(g_db2API.SQLSetCursorName(m_handles.m_hstmt,
#if defined(SA_UNICODE)
		(SQLTCHAR *)sOption.GetUTF16Chars(),
#else
		(SQLTCHAR *)(const SAChar*)sOption,
#endif
		SQL_NTS), SQL_HANDLE_STMT, m_handles.m_hstmt);
}

/*virtual */
void Idb2Cursor::Close()
{
	assert(m_handles.m_hstmt);
	Idb2Connection::Check(g_db2API.SQLFreeHandle(
		SQL_HANDLE_STMT, m_handles.m_hstmt), SQL_HANDLE_STMT, m_handles.m_hstmt);
	m_handles.m_hstmt = (SQLHSTMT)NULL;
}

/*virtual */
void Idb2Cursor::Destroy()
{
	assert(m_handles.m_hstmt);
	g_db2API.SQLFreeHandle(SQL_HANDLE_STMT, m_handles.m_hstmt);
	m_handles.m_hstmt = (SQLHSTMT)NULL;
}

/*virtual */
void Idb2Cursor::Reset()
{
	m_handles.m_hstmt = (SQLHSTMT)NULL;
	m_bResultSetCanBe = false;
}

/*virtual */
ISACursor *Idb2Connection::NewCursor(SACommand *m_pCommand)
{
	return new Idb2Cursor(this, m_pCommand);
}

/*virtual */
void Idb2Cursor::Prepare(
	const SAString &sStmt,
	SACommandType_t eCmdType,
	int nPlaceHolderCount,
	saPlaceHolder**	ppPlaceHolders)
{
	SAString sStmtDB2;
	size_t nPos = 0l;
	int i;
	switch( eCmdType )
	{
	case SA_CmdSQLStmt:
		// replace bind variables with '?' place holder
		for( i = 0; i < nPlaceHolderCount; i++ )
		{
			sStmtDB2 += sStmt.Mid(nPos, ppPlaceHolders[i]->getStart()-nPos);
			sStmtDB2 += _TSA("?");
			nPos = ppPlaceHolders[i]->getEnd() + 1;
		}
		// copy tail
		if( nPos < sStmt.GetLength() )
			sStmtDB2 += sStmt.Mid(nPos);
		break;
	case SA_CmdStoredProc:
		sStmtDB2 = CallSubProgramSQL();
		break;
	case SA_CmdSQLStmtRaw:
		sStmtDB2 = sStmt;
		break;
	default:
		assert(false);
	}

	// a bit of clean up
	Idb2Connection::Check(g_db2API.SQLFreeStmt(
		m_handles.m_hstmt, SQL_CLOSE), SQL_HANDLE_STMT, m_handles.m_hstmt);
	Idb2Connection::Check(g_db2API.SQLFreeStmt(
		m_handles.m_hstmt, SQL_UNBIND), SQL_HANDLE_STMT, m_handles.m_hstmt);

	Idb2Connection::Check(g_db2API.SQLFreeStmt(
		m_handles.m_hstmt, SQL_RESET_PARAMS), SQL_HANDLE_STMT, m_handles.m_hstmt);

	SA_TRACE_CMD(sStmtDB2);
	Idb2Connection::Check(g_db2API.SQLPrepare(m_handles.m_hstmt,
#ifdef SA_UNICODE
		(SQLTCHAR *)sStmtDB2.GetUTF16Chars(),
#else
		(SQLTCHAR *)(const SAChar*)sStmtDB2,
#endif
		SQL_NTS), SQL_HANDLE_STMT, m_handles.m_hstmt);
}

SAString Idb2Cursor::CallSubProgramSQL()
{
	int nParams = m_pCommand->ParamCount();

	SAString sSQL = _TSA("{");

	// check for Return parameter
	int i;
	for(i = 0; i < nParams; ++i)
	{
		SAParam &Param = m_pCommand->ParamByIndex(i);
		if(Param.ParamDirType() == SA_ParamReturn)
		{
			sSQL += _TSA("?=");
			break;
		}
	}
	sSQL += _TSA("call ");
	sSQL += m_pCommand->CommandText();

	// specify parameters
	SAString sParams;
	for(i = 0; i < nParams; ++i)
	{
		SAParam &Param = m_pCommand->ParamByIndex(i);
		if(Param.ParamDirType() == SA_ParamReturn)
			continue;

		if(!sParams.IsEmpty())
			sParams += _TSA(", ");
		sParams += _TSA("?");
	}
	if(!sParams.IsEmpty())
	{
		sSQL += _TSA("(");
		sSQL += sParams;
		sSQL += _TSA(")");
	}
	sSQL += _TSA("}");
	return sSQL;
}

void Idb2Cursor::BindLongs()
{
	SQLRETURN retcode;
	SQLPOINTER ValuePtr;

	try
	{
		while((retcode = g_db2API.SQLParamData(m_handles.m_hstmt, &ValuePtr))
			== SQL_NEED_DATA)
		{
			SAParam *pParam = (SAParam *)ValuePtr;
			size_t nActualWrite;
			SAPieceType_t ePieceType = SA_FirstPiece;
			void *pBuf;

			SADummyConverter DummyConverter;
			ISADataConverter *pIConverter = &DummyConverter;
			size_t nCnvtSize, nCnvtPieceSize = 0l;
			SAPieceType_t eCnvtPieceType;
#if defined(SA_UNICODE) && !defined(SQLAPI_WINDOWS)
			bool bUnicode = pParam->ParamType() == SA_dtCLob || pParam->DataType() == SA_dtCLob ||
				pParam->ParamType() == SA_dtLongChar || pParam->DataType() == SA_dtLongChar;
			SAUnicodeUTF16Converter UnicodeUtf2Converter;
			if( bUnicode )
				pIConverter = &UnicodeUtf2Converter;
#endif
			do
			{
				nActualWrite = pParam->InvokeWriter(ePieceType, Idb2Connection::MaxLongAtExecSize, pBuf);
				pIConverter->PutStream((unsigned char*)pBuf, nActualWrite, ePieceType);
				// smart while: initialize nCnvtPieceSize before calling pIConverter->GetStream
				while(nCnvtPieceSize = nActualWrite,
					pIConverter->GetStream((unsigned char*)pBuf, nCnvtPieceSize, nCnvtSize, eCnvtPieceType))
					Idb2Connection::Check(g_db2API.SQLPutData(m_handles.m_hstmt, pBuf, nCnvtSize),
					SQL_HANDLE_STMT, m_handles.m_hstmt);

				if(ePieceType == SA_LastPiece)
					break;
			}
			while( nActualWrite != 0 );
		}
	}
	catch(SAException &)
	{
		g_db2API.SQLCancel(m_handles.m_hstmt);
		throw;
	}

	if(retcode != SQL_NO_DATA)
		Idb2Connection::Check(retcode, SQL_HANDLE_STMT, m_handles.m_hstmt);
}

/*virtual */
void Idb2Cursor::UnExecute()
{
	m_bResultSetCanBe = false;
}

void Idb2Cursor::Bind(
	int nPlaceHolderCount,
	saPlaceHolder **ppPlaceHolders)
{
	// we should bind for every place holder ('?')
	AllocBindBuffer(nPlaceHolderCount, ppPlaceHolders,
		sizeof(SQLLEN), 0);
	void *pBuf = m_pParamBuffer;

	for(int i = 0; i < nPlaceHolderCount; ++i)
	{
		SAParam &Param = *ppPlaceHolders[i]->getParam();

		void *pInd;
		void *pSize;
		size_t nDataBufSize;
		void *pValue;
		IncParamBuffer(pBuf, pInd, pSize, nDataBufSize, pValue);

		SADataType_t eDataType = Param.DataType();
		SADataType_t eParamType = Param.ParamType();
		if(eParamType == SA_dtUnknown)
			eParamType = eDataType;	// assume
		// SA_dtUnknown can be used to bind NULL
		// but some type should be set in that case as well
		SQLSMALLINT ParameterType = Idb2Connection::CnvtStdToNative(
			eParamType == SA_dtUnknown? SA_dtString : eParamType);
		SQLSMALLINT ValueType = Idb2Connection::CnvtStdToNativeValueType(
			eDataType == SA_dtUnknown? SA_dtString : eDataType);

		SQLLEN *StrLen_or_IndPtr = (SQLLEN *)pInd;
		SQLPOINTER ParameterValuePtr = (SQLPOINTER)pValue;
		SQLLEN BufferLength = (SQLLEN)nDataBufSize;
		SQLULEN ColumnSize = (SQLULEN)Param.ParamSize();
		if(!ColumnSize)	// not set explicitly
			ColumnSize = BufferLength;

		SQLSMALLINT InputOutputType;
		switch(Param.ParamDirType())
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
		// This code will avoid a problem if a driver checks this
		// indicator on input even for pure outputs
		*StrLen_or_IndPtr = SQL_NULL_DATA;			// field is null

		if(isInputParam(Param))
		{
			if(Param.isNull())
				*StrLen_or_IndPtr = SQL_NULL_DATA;			// field is null
			else
				*StrLen_or_IndPtr = InputBufferSize(Param);	// field is not null

			if(!Param.isNull())
			{
				switch(eDataType)
				{
				case SA_dtUnknown:
                    throw SAException(SA_Library_Error, SA_Library_Error_UnknownParameterType, -1,
						IDS_UNKNOWN_PARAMETER_TYPE, (const SAChar*)Param.Name());
				case SA_dtBool:
					assert(*StrLen_or_IndPtr == (SQLLEN)sizeof(short));
					*(short*)ParameterValuePtr = (short)Param.asBool();
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
					*(int*)ParameterValuePtr = (int)Param.asLong();
					break;
				case SA_dtULong:
					assert(*StrLen_or_IndPtr == (SQLLEN)sizeof(unsigned long));
					*(unsigned int*)ParameterValuePtr = (unsigned int)Param.asULong();
					break;
				case SA_dtDouble:
					assert(*StrLen_or_IndPtr == (SQLLEN)sizeof(double));
					*(double*)ParameterValuePtr = Param.asDouble();
					break;
				case SA_dtNumeric:
					assert(*StrLen_or_IndPtr == (SQLLEN)sizeof(DB2_SQL_NUMERIC_STRUCT));
					Idb2Connection::CnvtNumericToInternal(
						Param.asNumeric(),
						*(DB2_SQL_NUMERIC_STRUCT*)ParameterValuePtr,
						*StrLen_or_IndPtr);
					break;
				case SA_dtDateTime:
					assert(*StrLen_or_IndPtr == (SQLLEN)sizeof(TIMESTAMP_STRUCT));
					Idb2Connection::CnvtDateTimeToInternal(
						Param.asDateTime(),
						*(TIMESTAMP_STRUCT*)ParameterValuePtr);
					break;
				case SA_dtString:
					assert(*StrLen_or_IndPtr == (SQLLEN)(Param.asString().GetLength() * sizeof(TSAChar)));
					memcpy(ParameterValuePtr,
#ifdef SA_UNICODE
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
					*StrLen_or_IndPtr = ((Idb2Connection*)m_pISAConnection)->LenDataAtExec();
					break;
				default:
					assert(false);
				}
			}

			if(Param.isDefault())
				*StrLen_or_IndPtr = SQL_DEFAULT_PARAM;
		}

		if(isLongOrLob(eDataType))
		{
			Idb2Connection::Check(g_db2API.SQLBindParameter(
				m_handles.m_hstmt,
				(SQLUSMALLINT)(i+1),
				InputOutputType,
				ValueType,
				ParameterType,
				0, // ColumnSize
				0, // DecimalDigits
				&Param,	// ParameterValuePtr - context
				SQLLEN(0), // Buffer length
				StrLen_or_IndPtr), SQL_HANDLE_STMT, m_handles.m_hstmt);
		}
		else if(eDataType == SA_dtNumeric)
		{
			SQLSMALLINT Precision;
			SQLSMALLINT Scale;

			switch(InputOutputType)
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
				if(Param.isNull())
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

			Idb2Connection::Check(g_db2API.SQLBindParameter(
				m_handles.m_hstmt,
				(SQLUSMALLINT)(i+1), InputOutputType,
				ValueType,
				ParameterType,
				Precision,	// ColumnSize
				Scale,		// DecimalDigits
				ParameterValuePtr,
				BufferLength,
				StrLen_or_IndPtr), SQL_HANDLE_STMT, m_handles.m_hstmt);

			// for numeric data we have to adjust precision and scale
			// because:
			// precision deaults to some driver specific value
			// scale defaults to 0

			// Get the application row descriptor
			// for the statement handle using SQLGetStmtAttr
			SQLHDESC hdesc;
			Idb2Connection::Check(g_db2API.SQLGetStmtAttr(
				m_handles.m_hstmt,
				SQL_ATTR_APP_PARAM_DESC,
				&hdesc,
				0, NULL), SQL_HANDLE_STMT, m_handles.m_hstmt);

			Idb2Connection::Check(g_db2API.SQLSetDescRec(
				hdesc,
				(SQLSMALLINT)(i+1),
				ValueType, -1, nDataBufSize,
				Precision,
				Scale,
				pValue,
				(SQLLEN*)pInd,
				(SQLLEN*)pInd), SQL_HANDLE_DESC, hdesc);
		}
		else
			Idb2Connection::Check(g_db2API.SQLBindParameter(
			m_handles.m_hstmt,
			(SQLUSMALLINT)(i+1), InputOutputType,
			ValueType,
			ParameterType,
			ColumnSize,
			0,	// DecimalDigits
			ParameterValuePtr,
			BufferLength,
			StrLen_or_IndPtr), SQL_HANDLE_STMT, m_handles.m_hstmt);
	}
}

/*virtual */
void Idb2Cursor::Execute(
	int nPlaceHolderCount,
	saPlaceHolder **ppPlaceHolders)
{
	if( nPlaceHolderCount )
		Bind(nPlaceHolderCount, ppPlaceHolders);

	SQLRETURN rc;
	Idb2Connection::Check(g_db2API.SQLFreeStmt(
		m_handles.m_hstmt, SQL_CLOSE), SQL_HANDLE_STMT, m_handles.m_hstmt);
	rc = g_db2API.SQLExecute(m_handles.m_hstmt);

	if(rc != SQL_NEED_DATA)
	{
		if(rc != SQL_NO_DATA)	// SQL_NO_DATA is OK for DELETEs or UPDATEs that haven't affected any rows
			Idb2Connection::Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);
	}
	else
		BindLongs();

	m_bResultSetCanBe = true;

	// if SP: in DB2 output parametrs are available
	// immediately after execute
	// even if there are result sets from SP
	ConvertOutputParams();	// if any
}

/*virtual */
void Idb2Cursor::Cancel()
{
	Idb2Connection::Check(g_db2API.SQLCancel(
		m_handles.m_hstmt), SQL_HANDLE_STMT, m_handles.m_hstmt);
}

/*virtual */
bool Idb2Cursor::ResultSetExists()
{
	if( ! m_bResultSetCanBe )
		return false;

	SQLSMALLINT ColumnCount;
	Idb2Connection::Check(g_db2API.SQLNumResultCols(
		m_handles.m_hstmt, &ColumnCount), SQL_HANDLE_STMT, m_handles.m_hstmt);
	return (ColumnCount > 0);
}

/*virtual */
void Idb2Cursor::DescribeFields(
	DescribeFields_cb_t fn)
{
	SQLSMALLINT ColumnCount;
	Idb2Connection::Check(g_db2API.SQLNumResultCols(m_handles.m_hstmt, &ColumnCount), SQL_HANDLE_STMT, m_handles.m_hstmt);

	for(SQLSMALLINT nField = 1; nField <= ColumnCount; ++nField)
	{
		SQLTCHAR szColName[1024];
		SQLSMALLINT nColLen;
		SQLSMALLINT DataType;
		SQLULEN ColumnSize = 0l;
		SQLSMALLINT Nullable;
		SQLSMALLINT DecimalDigits;

		Idb2Connection::Check(g_db2API.SQLDescribeCol(
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
#ifdef SA_UNICODE
		sColName.SetUTF16Chars(szColName, nColLen);
#else
		sColName = SAString((const SAChar*)szColName, nColLen);
#endif
		(m_pCommand->*fn)(sColName,
			Idb2Connection::CnvtNativeToStd(DataType, ColumnSize, ColumnSize, DecimalDigits),
			(int)DataType,
			ColumnSize,
			(int)ColumnSize,
			DecimalDigits,
			Nullable == SQL_NO_NULLS,
            (int)ColumnCount);
	}
}

/*virtual */
long Idb2Cursor::GetRowsAffected()
{
	assert(m_handles.m_hstmt != 0);

	SQLLEN RowCount = -1l;

	Idb2Connection::Check(g_db2API.SQLRowCount(m_handles.m_hstmt, &RowCount), SQL_HANDLE_STMT, m_handles.m_hstmt);
	return long(RowCount);
}

/*virtual */
size_t Idb2Cursor::OutputBufferSize(
	SADataType_t eDataType,
	size_t nDataSize) const
{
	switch(eDataType)
	{
	case SA_dtLong:
	case SA_dtULong:
		return sizeof(int);
	case SA_dtBool:
		return sizeof(unsigned char);	// SQL_C_BIT
	case SA_dtNumeric:
		return sizeof(DB2_SQL_NUMERIC_STRUCT);	// SQL_C_NUMERIC or SQL_C_CHAR[]
	case SA_dtString:
		return (sizeof(TSAChar) * (nDataSize + 1));	// always allocate space for NULL
	case SA_dtDateTime:
		return sizeof(TIMESTAMP_STRUCT);
	case SA_dtLongBinary:
	case SA_dtLongChar:
	case SA_dtBLob:
	case SA_dtCLob:
		return 0;
    case SA_dtUnknown: // we have to ignore unknown data types
        return 0;
	default:
		break;
	}

	return ISACursor::OutputBufferSize(eDataType, nDataSize);
}

/*virtual */
void Idb2Cursor::SetFieldBuffer(
	int nCol,	// 1-based
	void *pInd,
	size_t nIndSize,
	void * /*pSize*/,
	size_t/* nSizeSize*/,
	void *pValue,
	size_t nBufSize)
{
	assert(nIndSize == sizeof(SQLLEN));
	if(nIndSize != sizeof(SQLLEN))
		return;

	SAField &Field = m_pCommand->Field(nCol);

    if (Field.FieldType() == SA_dtUnknown) // ignore unknown field type
        return;

	SQLSMALLINT TargetType = Idb2Connection::CnvtStdToNativeValueType(Field.FieldType());
	bool bLong = false;
	switch(Field.FieldType())
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
		assert(nBufSize == sizeof(DB2_SQL_NUMERIC_STRUCT));
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
	case SA_dtBLob:
	case SA_dtCLob:
		assert(nBufSize == 0);
		bLong = true;
		break;
	default:
		assert(false);	// unknown type
	}

	if(!bLong)
	{
		Idb2Connection::Check(g_db2API.SQLBindCol(
			m_handles.m_hstmt,
			(SQLUSMALLINT)nCol,
			TargetType,
			pValue,
			nBufSize,
			(SQLLEN*)pInd), SQL_HANDLE_STMT, m_handles.m_hstmt);

		// for numeric data we have to adjust precision and scale
		// because:
		// precision deaults to some driver specific value
		// scale defaults to 0
		if(Field.FieldType() == SA_dtNumeric)
		{
			// Get the application row descriptor
			// for the statement handle using SQLGetStmtAttr
			SQLHDESC hdesc;
			Idb2Connection::Check(g_db2API.SQLGetStmtAttr(
				m_handles.m_hstmt,
				SQL_ATTR_APP_ROW_DESC,
				&hdesc,
				0, NULL), SQL_HANDLE_STMT, m_handles.m_hstmt);

			Idb2Connection::Check(g_db2API.SQLSetDescRec(
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
		}
	}
}

/*virtual */
void Idb2Cursor::SetSelectBuffers()
{
	SAString sOption = m_pCommand->Option(SACMD_PREFETCH_ROWS);
	if( ! sOption.IsEmpty())
	{
		int cLongs = FieldCount(4, SA_dtLongBinary, SA_dtLongChar, SA_dtBLob, SA_dtCLob);
		if(cLongs)	// do not use bulk fetch if there are text or image columns
			m_cRowsToPrefetch = 1;
		else
		{
			m_cRowsToPrefetch = sa_toi(sOption);
			if( ! m_cRowsToPrefetch )
				m_cRowsToPrefetch = 1;
		}
	}
	else
		m_cRowsToPrefetch = 1;

	m_cRowsObtained = 0;
	m_cRowCurrent = 0;

	// set the SQL_ATTR_ROW_BIND_TYPE statement attribute to use
	// column-wise binding.
	Idb2Connection::Check(
		g_db2API.SQLSetStmtAttr(m_handles.m_hstmt, SQL_ATTR_ROW_BIND_TYPE, SQL_BIND_BY_COLUMN, 0),
		SQL_HANDLE_STMT, m_handles.m_hstmt);
	// Declare the rowset size with the SQL_ATTR_ROW_ARRAY_SIZE
	// statement attribute.
	Idb2Connection::Check(
		g_db2API.SQLSetStmtAttr(m_handles.m_hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)m_cRowsToPrefetch, 0),
		SQL_HANDLE_STMT, m_handles.m_hstmt);
	// Set the SQL_ATTR_ROWS_FETCHED_PTR statement attribute
	// to point to m_cRowsObtained
	Idb2Connection::Check(
		g_db2API.SQLSetStmtAttr(m_handles.m_hstmt, SQL_ATTR_ROWS_FETCHED_PTR, &m_cRowsObtained, 0),
		SQL_HANDLE_STMT, m_handles.m_hstmt);

	// use default helpers
	AllocSelectBuffer(sizeof(SQLLEN), 0, m_cRowsToPrefetch);
}

/*virtual */
bool Idb2Cursor::ConvertIndicator(
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
	if(nIndSize != sizeof(SQLLEN))
		return false;	// not converted

	bool bLong = false;
	SQLSMALLINT TargetType = 0;
	bool bAddSpaceForNull = false;

	SADataType_t eDataType;
	switch(eValueType)
	{
	case ISA_FieldValue:
		eDataType = ((SAField&)vr).FieldType();
		break;
	default:
		assert(eValueType == ISA_ParamValue);
		eDataType = ((SAParam&)vr).ParamType();
	}
	switch(eDataType)
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
		TargetType = SQL_C_TCHAR;
		bAddSpaceForNull = true;
		break;
	default:
		break;
	}

	if(bLong)
	{
		TSAChar Buf[1];
		SQLLEN StrLen_or_IndPtr = 0l;

		Idb2Connection::Check(g_db2API.SQLGetData(
			m_handles.m_hstmt, (SQLUSMALLINT)nPos,
			TargetType, Buf, bAddSpaceForNull ? sizeof(TSAChar):0,
			&StrLen_or_IndPtr), SQL_HANDLE_STMT, m_handles.m_hstmt);

        *vr.m_pbNull = ((SQLINTEGER)StrLen_or_IndPtr == SQL_NULL_DATA);
		if(!vr.isNull())
		{
			// also try to get long size
			if(StrLen_or_IndPtr >= 0)
				nRealSize = (size_t)StrLen_or_IndPtr;
			else
				nRealSize = 0l;	// unknown
		}
		return true;	// converted
	}

    // set unknown data size to null
    *vr.m_pbNull = eDataType == SA_dtUnknown || ((SQLINTEGER*)pInd)[nBulkReadingBufPos] == SQL_NULL_DATA;
	if(!vr.isNull())
		nRealSize = ((SQLINTEGER*)pInd)[nBulkReadingBufPos];
	return true;	// converted
}

/*virtual */
bool Idb2Cursor::IgnoreUnknownDataType()
{
    return true;
}

/*virtual */
void Idb2Cursor::ConvertString(SAString &String, const void *pData, size_t nRealSize)
{
	// nRealSize is in bytes but we need in characters,
	// so nRealSize should be a multiply of character size
	assert(nRealSize % sizeof(TSAChar) == 0);
#ifdef SA_UNICODE
	String.SetUTF16Chars(pData, nRealSize/sizeof(TSAChar));
#else
	String = SAString((const SAChar*)pData, nRealSize/sizeof(SAChar));
#endif
}

/*virtual */
bool Idb2Cursor::FetchNext()
{
	if( ! m_cRowsObtained || m_cRowCurrent >= (m_cRowsObtained - 1) )
	{
		SQLRETURN rc = g_db2API.SQLFetchScroll(
			m_handles.m_hstmt, SQL_FETCH_NEXT, 0);
		if( rc != SQL_NO_DATA )
			Idb2Connection::Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);
		else
			m_cRowsObtained = 0;

		m_cRowCurrent = 0;
	}
	else
		++m_cRowCurrent;

	if( m_cRowsObtained )
	{
		// use default helpers
		ConvertSelectBufferToFields(m_cRowCurrent);
	}
	else if ( ! isSetScrollable() )
	{
		SQLRETURN rcMoreResults = g_db2API.SQLMoreResults(
			m_handles.m_hstmt);

		if(rcMoreResults != SQL_NO_DATA)
			Idb2Connection::Check(rcMoreResults, SQL_HANDLE_STMT, m_handles.m_hstmt);
		else
			m_bResultSetCanBe = false;
	}

	return m_cRowsObtained != 0;
}

/*virtual */
bool Idb2Cursor::FetchPrior()
{
	if( ! m_cRowsObtained || m_cRowCurrent <= 0 )
	{
		SQLRETURN rc = g_db2API.SQLFetchScroll(m_handles.m_hstmt, SQL_FETCH_PRIOR, 0);

		if( rc != SQL_NO_DATA )	
			Idb2Connection::Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);		
		else
			m_cRowsObtained = 0;

		m_cRowCurrent = m_cRowsObtained - 1;
	}
	else
		--m_cRowCurrent;

	if( m_cRowsObtained )
	{
		// use default helpers
		ConvertSelectBufferToFields(m_cRowCurrent);
	}	

	return m_cRowsObtained != 0;
}

/*virtual */
bool Idb2Cursor::FetchFirst()
{
	SQLRETURN rc = g_db2API.SQLFetchScroll(m_handles.m_hstmt, SQL_FETCH_FIRST, 0);

	if( rc != SQL_NO_DATA )	
		Idb2Connection::Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);
	else
		m_cRowsObtained = 0;

	m_cRowCurrent = 0;	

	if( m_cRowsObtained )
	{
		// use default helpers
		ConvertSelectBufferToFields(m_cRowCurrent);
	}

	return m_cRowsObtained != 0;
}

/*virtual */
bool Idb2Cursor::FetchLast()
{
	SQLRETURN rc = g_db2API.SQLFetchScroll(m_handles.m_hstmt, SQL_FETCH_LAST, 0);

	if( rc != SQL_NO_DATA )	
		Idb2Connection::Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);
	else
		m_cRowsObtained = 0;

	m_cRowCurrent = m_cRowsObtained - 1;

	if( m_cRowsObtained )
	{
		// use default helpers
		ConvertSelectBufferToFields(m_cRowCurrent);
	}

	return m_cRowsObtained != 0;
}

/*virtual */
bool Idb2Cursor::FetchPos(int offset, bool Relative/* = false*/)
{
    SQLRETURN rc = g_db2API.SQLFetchScroll(m_handles.m_hstmt,
        Relative ? SQL_FETCH_RELATIVE : SQL_FETCH_ABSOLUTE, (SQLLEN)offset);

    if (rc != SQL_NO_DATA)
        Idb2Connection::Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);
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
void Idb2Cursor::ReadLongOrLOB(
	ValueType_t eValueType,
	SAValueRead &vr,
	void * /*pValue*/,
	size_t/* nBufSize*/,
	saLongOrLobReader_t fnReader,
	size_t nReaderWantedPieceSize,
	void *pAddlData)
{
	if(eValueType == ISA_FieldValue)
	{
		SAField &Field = (SAField &)vr;

		SQLSMALLINT TargetType;
		SQLLEN StrLen_or_IndPtr = 0l;
		SQLRETURN rc;
		bool bAddSpaceForNull;

		switch(Field.FieldType())
		{
		case SA_dtLongBinary:
		case SA_dtBLob:
			TargetType = SQL_C_BINARY;
			bAddSpaceForNull = false;
			break;
		case SA_dtLongChar:
		case SA_dtCLob:
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
#if defined(SA_UNICODE) && !defined(SQLAPI_WINDOWS)
		SAUTF16UnicodeConverter Utf16UnicodeConverter;
		if( bAddSpaceForNull )
			pIConverter = &Utf16UnicodeConverter;
#endif

		// try to get long size
		size_t nLongSize = 0l;
		TSAChar Buf[1];
		rc = g_db2API.SQLGetData(
			m_handles.m_hstmt, (SQLUSMALLINT)Field.Pos(),
			TargetType, Buf, bAddSpaceForNull ? sizeof(TSAChar):0l,
			&StrLen_or_IndPtr);
		if( rc != SQL_NO_DATA )
		{
			Idb2Connection::Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);
			if(StrLen_or_IndPtr >= 0)
				nLongSize = (StrLen_or_IndPtr * (bAddSpaceForNull ? sizeof(SAChar)/sizeof(TSAChar):1));
		}

		unsigned char* pBuf;
		size_t nPortionSize = vr.PrepareReader(
			nLongSize,
			Idb2Connection::MaxLongAtExecSize,
			pBuf,
			fnReader,
			nReaderWantedPieceSize,
			pAddlData,
			bAddSpaceForNull);
		assert(nPortionSize <= Idb2Connection::MaxLongAtExecSize);

		SAPieceType_t ePieceType = SA_FirstPiece;
		size_t nTotalRead = 0l;

		unsigned int nCnvtLongSizeMax = (unsigned int)nLongSize;
		size_t nCnvtSize, nCnvtPieceSize = nPortionSize;
		SAPieceType_t eCnvtPieceType;
		size_t nTotalPassedToReader = 0l;

		do
		{
			if(nLongSize)	// known
				nPortionSize = sa_min(nPortionSize, nLongSize - nTotalRead);

			rc = g_db2API.SQLGetData(
				m_handles.m_hstmt, (SQLUSMALLINT)Field.Pos(),
				TargetType, pBuf, nPortionSize + (bAddSpaceForNull ? sizeof(TSAChar):0),
				&StrLen_or_IndPtr);
			assert(nPortionSize || rc == SQL_NO_DATA);

			if(rc != SQL_NO_DATA)
			{
				Idb2Connection::Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);
				size_t NumBytes = (size_t)StrLen_or_IndPtr > nPortionSize || StrLen_or_IndPtr == SQL_NO_TOTAL ? nPortionSize : StrLen_or_IndPtr;
				nTotalRead += NumBytes;
				if( ! NumBytes ) // The size of the piece is 0 - end the reading
				{
					if(ePieceType == SA_FirstPiece)
						ePieceType = SA_OnePiece;
					else
						ePieceType = SA_LastPiece;
					rc = SQL_NO_DATA;
				}

				pIConverter->PutStream(pBuf, NumBytes, ePieceType);
				// smart while: initialize nCnvtPieceSize before calling pIConverter->GetStream
				while(nCnvtPieceSize = (nCnvtLongSizeMax ? // known
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
				if(ePieceType == SA_FirstPiece)
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
		}
		while(rc != SQL_NO_DATA);
	}
	else
	{
		assert(eValueType == ISA_ParamValue);
		assert(false);
	}
}

/*virtual */
void Idb2Cursor::DescribeParamSP()
{
	SAString sText = m_pCommand->CommandText();
	SAString sSchemaName;
	SAString sProcName;

	size_t n = sText.Find(_TSA('.'));
	if( SIZE_MAX == n )
		sProcName = sText;
	else
	{
		sSchemaName = sText.Left(n);
		sProcName = sText.Mid(n+1);
	}

	SACommand cmd(m_pISAConnection->getSAConnection());
	cmd.Open();
	db2CommandHandles *pHandles = (db2CommandHandles *)cmd.NativeHandles();

	if( sSchemaName.IsEmpty() )
		sSchemaName = SQL_ALL_SCHEMAS;

	Idb2Connection::Check(g_db2API.SQLProcedureColumns(
		pHandles->m_hstmt, NULL, 0,
#ifdef SA_UNICODE
		(SQLTCHAR *)sSchemaName.GetUTF16Chars(),
#else
		(SQLCHAR *)sSchemaName.GetMultiByteChars(),
#endif
		SQL_NTS,
#ifdef SA_UNICODE
		(SQLTCHAR *)sProcName.GetUTF16Chars(),
#else
		(SQLCHAR *)sProcName.GetMultiByteChars(),
#endif
		SQL_NTS,
		(SQLTCHAR *)_TSA(""), SQL_NTS), SQL_HANDLE_STMT, pHandles->m_hstmt);

	bool bReturnStatus = m_pCommand->Option(_TSA("ReturnStatus")).CompareNoCase(_TSA("Ignore")) == 0;
	while(cmd.FetchNext())
	{
		//		SAString sCat = cmd.Field(1);		// "PROCEDURE_CAT"
		//		SAString sSchem = cmd.Field(2);		// "PROCEDURE_SCHEM"
		//		SAString sProc = cmd.Field(3);		// "PROCEDURE_NAME"
		SAString sCol = cmd.Field(4);		// "COLUMN_NAME"
		short nColType = cmd.Field(5);		// "COLUMN_TYPE"
		short nDataType = cmd.Field(6);		// "DATA_TYPE"
		//		SAString sType = cmd.Field(7);		// "TYPE_NAME"
		int nColSize = cmd.Field(8).isNull()? 0 : (int)cmd.Field(8).asLong();		// "COLUMN_SIZE"
		//		long nBufferLength = cmd.Field(9).isNull()? 0 : (long)cmd.Field(9);	// "BUFFER_LENGTH"
		short nDecDigits = cmd.Field(10).isNull()? (short)0 : (short)cmd.Field(10);	// "DECIMAL_DIGITS"
		//		short nNumPrecPadix = cmd.Field(11).isNull()? 0 : (short)cmd.Field(11);// "NUM_PREC_RADIX"
		//		short nNullable = cmd.Field(12).isNull()? 0 : (short)cmd.Field(12);	// "NULLABLE"

		SAParamDirType_t eDirType;
		switch( nColType )
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
			bReturnStatus = true;
			break;
		case SQL_PARAM_TYPE_UNKNOWN:
		case SQL_RESULT_COL:
			continue;
		default:
			assert(false);
			continue;
		}
		SADataType_t eParamType = Idb2Connection::CnvtNativeToStd(nDataType, nColSize, nColSize, nDecDigits);
		// DB2 can report the LONG VARCHAR parameter type (at least on OS/390)
		// But it can be a problem for the DB2 UDF client. To avoid it we use
		// the buffer for the whole database field data - i.e. SA_dtString
		if( eDirType == SA_ParamOutput || eDirType == SA_ParamInputOutput )
		{
			if( SA_dtLongChar == eParamType || SA_dtCLob == eParamType )
				eParamType = SA_dtString;
			else if( SA_dtLongBinary == eParamType || SA_dtBLob == eParamType )
				eParamType = SA_dtBytes;
		}

		SAString sParamName;
		if(sCol.IsEmpty() && eDirType == SA_ParamReturn)
			sParamName = _TSA("RETURN_VALUE");
		else
			sParamName = sCol;

		// DB2 procedure always has integer return status
		// If it hasn't been described by SQLProcedureColumns we will add it (as first parameter)
		if(!bReturnStatus)
		{
			if(eDirType != SA_ParamReturn)
				m_pCommand->CreateParam(_TSA("RETURN_VALUE"),
				SA_dtLong, SQL_INTEGER, 10, 10, 0, SA_ParamReturn);
			bReturnStatus = true;
		}

		m_pCommand->CreateParam(sParamName, eParamType, nDataType, nColSize, nColSize, nDecDigits, eDirType);
	}

	// DB2 procedure always has integer return status
	// If it hasn't been described by SQLProcedureColumns we will add it (as first parameter)
	if(!bReturnStatus)	// there's been no parameters at all
		m_pCommand->CreateParam(_TSA("RETURN_VALUE"),
		SA_dtLong, SQL_INTEGER, 10, 10, 0, SA_ParamReturn);
}

/*virtual */
saAPI *Idb2Connection::NativeAPI() const
{
	return &g_db2API;
}

/*virtual */
saConnectionHandles *Idb2Connection::NativeHandles()
{
	return &m_handles;
}

/*virtual */
saCommandHandles *Idb2Cursor::NativeHandles()
{
	return &m_handles;
}

/*virtual */
void Idb2Connection::setIsolationLevel(
	SAIsolationLevel_t eIsolationLevel)
{
	Commit();

	issueIsolationLevel(eIsolationLevel);
}

void Idb2Connection::issueIsolationLevel(
	SAIsolationLevel_t eIsolationLevel)
{
	SQLPOINTER isolation;
	switch(eIsolationLevel)
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

	Check(g_db2API.SQLSetConnectAttr(
		m_handles.m_hdbc,
		SQL_ATTR_TXN_ISOLATION,
		isolation, 0), SQL_HANDLE_DBC, m_handles.m_hdbc);
}

/*virtual */
void Idb2Connection::setAutoCommit(
	SAAutoCommit_t eAutoCommit)
{
	SQLPOINTER nAutoCommit;
	switch(eAutoCommit)
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

	Check(g_db2API.SQLSetConnectAttr(
		m_handles.m_hdbc,
		SQL_ATTR_AUTOCOMMIT,
		nAutoCommit, 0), SQL_HANDLE_DBC, m_handles.m_hdbc);
}

/*virtual */
void Idb2Connection::CnvtInternalToNumeric(
	SANumeric &numeric,
	const void *pInternal,
	int nInternalSize)
{
#if defined(DB2_SUPPORTS_SQL_C_NUMERIC)
	if( nInternalSize != int(sizeof(SQL_NUMERIC_STRUCT)) )
		return;

	SQL_NUMERIC_STRUCT *pSQL_NUMERIC_STRUCT = (SQL_NUMERIC_STRUCT *)pInternal;
	numeric.precision = pSQL_NUMERIC_STRUCT->precision;
	numeric.scale = pSQL_NUMERIC_STRUCT->scale;
	// set sign
	// DB2 CLI: 1 if positive, 0 if negative
	// SQLAPI: 1 if positive, 0 if negative
	numeric.sign = pSQL_NUMERIC_STRUCT->sign;
	// copy mantissa
	// DB2 CLI: little endian format
	// SQLAPI: little endian format
	memset(numeric.val, 0, sizeof(numeric.val));
	memcpy(
		numeric.val,
		pSQL_NUMERIC_STRUCT->val,
		sa_min(sizeof(numeric.val), sizeof(pSQL_NUMERIC_STRUCT->val)));
#else
	numeric = SAString((const char*)pInternal, nInternalSize);
#endif
}

/*static */
void Idb2Connection::CnvtNumericToInternal(
	const SANumeric &numeric,
	DB2_SQL_NUMERIC_STRUCT &Internal,
	SQLLEN &StrLen_or_IndPtr)
{
	assert(StrLen_or_IndPtr == (SQLLEN)sizeof(DB2_SQL_NUMERIC_STRUCT));

#if defined(DB2_SUPPORTS_SQL_C_NUMERIC)
	StrLen_or_IndPtr = sizeof(SQL_NUMERIC_STRUCT);	// to prevent warning
	Internal.precision = numeric.precision;
	Internal.scale = numeric.scale;
	// set sign
	// DB2 CLI: 1 if positive, 0 if negative
	// SQLAPI: 1 if positive, 0 if negative
	Internal.sign = numeric.sign;
	// copy mantissa
	// DB2 CLI: little endian format
	// SQLAPI: little endian format
	memset(Internal.val, 0, sizeof(Internal.val));
	memcpy(
		Internal.val,
		numeric.val,
		sa_min(sizeof(numeric.val), sizeof(Internal.val)));
#else
	SAString sNum = numeric.operator SAString();
	StrLen_or_IndPtr = sNum.GetMultiByteCharsLength();
	memcpy(Internal, sNum.GetMultiByteChars(), StrLen_or_IndPtr);
#endif
}

