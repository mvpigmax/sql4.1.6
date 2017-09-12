// infClient.cpp: implementation of the IinfClient class.
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <infAPI.h>
#include "infClient.h"

#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
#define	TSAChar	UTF16
#else
#define	TSAChar	SAChar
#endif

//////////////////////////////////////////////////////////////////////
// IinfConnection Class
//////////////////////////////////////////////////////////////////////

class IinfConnection : public ISAConnection
{
	enum MaxLong
	{
		MaxLongAtExecSize = 0x7fffffff+SQL_LEN_DATA_AT_EXEC(0)
	};
	friend class IinfCursor;

	infConnectionHandles m_handles;
	long m_nDriverODBCVer;

	static void Check(
		SQLRETURN return_code,
		SQLSMALLINT HandleType,
		SQLHANDLE Handle);
	SQLINTEGER LenDataAtExec();
	void issueIsolationLevel(
		SAIsolationLevel_t eIsolationLevel);

	static void CnvtNumericToInternal(
		const SANumeric &numeric,
		SQL_NUMERIC_STRUCT &Internal);

	void SafeAllocEnv();
	void SafeAllocConnect();
	void SafeFreeEnv();
	void SafeFreeConnect();
	void SafeSetEnvAttr();
	void SafeCommit();
	void SafeRollback();
	void SafeSetConnectOption(
		SQLINTEGER Attribute,
		SQLUINTEGER ValuePtr,
		SQLINTEGER StringLength);

protected:
	virtual ~IinfConnection();

public:
	IinfConnection(SAConnection *pSAConnection);

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
	static void CnvtInternalToDateTime(
		SADateTime &date_time,
		const TIMESTAMP_STRUCT &Internal);
	static void CnvtDateTimeToInternal(
		const SADateTime &date_time,
		TIMESTAMP_STRUCT &Internal);

	virtual void CnvtInternalToInterval(
		SAInterval &interval,
		const void *pInternal,
		int nInternalSize);
	static void CnvtInternalToInterval(
		SAInterval &interval,
		const SQL_INTERVAL_STRUCT &Internal);
	static void CnvtIntervalToInternal(
		const SAInterval &interval,
		SQL_INTERVAL_STRUCT &Internal);

	virtual void CnvtInternalToCursor(
		SACommand *pCursor,
		const void *pInternal);
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IinfConnection::IinfConnection(
	SAConnection *pSAConnection) : ISAConnection(pSAConnection)
{
	Reset();
}

IinfConnection::~IinfConnection()
{
}

/*virtual */
void IinfConnection::InitializeClient()
{
	::AddInfSupport(m_pSAConnection);
}

/*virtual */
void IinfConnection::UnInitializeClient()
{
	if( SAGlobals::UnloadAPI() ) ::ReleaseInfSupport();
}

SQLINTEGER IinfConnection::LenDataAtExec()
{
	SQLSMALLINT retlen = 0;
	char szValue[10];
	Check(g_infAPI.SQLGetInfo(
		m_handles.m_hdbc, SQL_NEED_LONG_DATA_LEN, szValue, (SQLSMALLINT)sizeof(szValue), &retlen), SQL_HANDLE_DBC, m_handles.m_hdbc);
	if(retlen > 0 && (*szValue == 'Y' || *szValue == 'y'))
		return SQL_LEN_DATA_AT_EXEC(MaxLongAtExecSize);
	//return SQL_LEN_DATA_AT_EXEC(0);

	return SQL_DATA_AT_EXEC;
}

/*virtual */
void IinfConnection::CnvtInternalToDateTime(
	SADateTime &date_time,
	const void *pInternal,
	int nInternalSize)
{
	if( nInternalSize != int(sizeof(TIMESTAMP_STRUCT)) )
		return;
	CnvtInternalToDateTime(date_time, *(const TIMESTAMP_STRUCT*)pInternal);
}

/*static */
void IinfConnection::CnvtInternalToDateTime(
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
void IinfConnection::CnvtDateTimeToInternal(
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
void IinfConnection::CnvtInternalToInterval(
	SAInterval &interval,
	const void *pInternal,
	int nInternalSize)
{
	if( nInternalSize != int(sizeof(SQL_INTERVAL_STRUCT)) )
		return;
	CnvtInternalToInterval(interval, *(const SQL_INTERVAL_STRUCT*)pInternal);
}

/*static */
void IinfConnection::CnvtInternalToInterval(
	SAInterval &/*interval*/,
	const SQL_INTERVAL_STRUCT &/*Internal*/)
{
	// TODO:
	/*
	SAInterval intVal;
	switch (Internal.interval_type)
	{
	case SQL_IS_YEAR:
	case SQL_IS_MONTH:
	case SQL_IS_YEAR_TO_MONTH:
		break;
	case SQL_IS_DAY:
	case SQL_IS_HOUR:
	case SQL_IS_MINUTE:
	case SQL_IS_SECOND:
	case SQL_IS_DAY_TO_HOUR:
	case SQL_IS_DAY_TO_MINUTE:
	case SQL_IS_DAY_TO_SECOND:
	case SQL_IS_HOUR_TO_MINUTE:
	case SQL_IS_HOUR_TO_SECOND:
	case SQL_IS_MINUTE_TO_SECOND:
		break;
	default:
		return;
	}
	*/
}

/*static */
void IinfConnection::CnvtIntervalToInternal(
	const SAInterval &/*interval*/,
	SQL_INTERVAL_STRUCT &/*Internal*/)
{
	// TODO:
}

/*virtual */
void IinfConnection::CnvtInternalToCursor(
	SACommand * /*pCursor*/,
	const void * /*pInternal*/)
{
	assert(false);
}

/*virtual */
long IinfConnection::GetClientVersion() const
{
	if(g_nInfDLLVersionLoaded == 0)	// has not been detected
	{
		if(IsConnected())
		{
			SAChar szInfoValue[1024];
			SQLSMALLINT cbInfoValue;
			g_infAPI.SQLGetInfo(m_handles.m_hdbc, SQL_DRIVER_VER, szInfoValue, 1024, &cbInfoValue);
			szInfoValue[cbInfoValue/sizeof(TSAChar)] = 0;

			SAString sInfo;
#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
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

	return g_nInfDLLVersionLoaded;
}

/*virtual */
long IinfConnection::GetServerVersion() const
{
	assert(m_handles.m_hdbc);

	SAChar szInfoValue[1024];
	SQLSMALLINT cbInfoValue;
	g_infAPI.SQLGetInfo(m_handles.m_hdbc, SQL_DBMS_VER, szInfoValue, 1024, &cbInfoValue);
	szInfoValue[cbInfoValue/sizeof(TSAChar)] = 0;

	SAString sInfo;
#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
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
SAString IinfConnection::GetServerVersionString() const
{
	assert(m_handles.m_hdbc);

	SAChar szInfoValue[1024];
	SQLSMALLINT cbInfoValue;
	g_infAPI.SQLGetInfo(m_handles.m_hdbc, SQL_DBMS_NAME, szInfoValue, 1024, &cbInfoValue);
	szInfoValue[cbInfoValue/sizeof(TSAChar)] = 0;

	SAString sInfo;
#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
	sInfo.SetUTF16Chars(szInfoValue);
#else
	sInfo = szInfoValue;
#endif

	SAString s = sInfo;
	s += _TSA(" Release ");

	g_infAPI.SQLGetInfo(m_handles.m_hdbc, SQL_DBMS_VER, szInfoValue, 1024, &cbInfoValue);
	szInfoValue[cbInfoValue/sizeof(TSAChar)] = 0;

#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
	sInfo.SetUTF16Chars(szInfoValue);
#else
	sInfo = szInfoValue;
#endif

	s += sInfo;

	return s;
}

#define SA_ODBC_MAX_MESSAGE_LENGTH	4096

/*static */
void IinfConnection::Check(
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

	SAString sMsg;
	TSAChar szMsg[SA_ODBC_MAX_MESSAGE_LENGTH];

	SQLSMALLINT TextLength;

	SQLSMALLINT i = SQLSMALLINT(1);
	SQLRETURN rc = SQL_SUCCESS;
	SAException* pNested = NULL;

	if(g_infAPI.SQLGetDiagRec)
	{
		do
		{
			Sqlstate[0] = '\0';
			szMsg[0] ='\0';

			rc = g_infAPI.SQLGetDiagRec(HandleType, Handle,
				i++, Sqlstate, &NativeError2,
				(SQLTCHAR*)szMsg, SA_ODBC_MAX_MESSAGE_LENGTH, &TextLength);

			if( SQL_SUCCESS == rc || SQL_SUCCESS_WITH_INFO == rc )
			{
				if (sMsg.GetLength() > 0)
					pNested = new SAException(pNested, SA_DBMS_API_Error, NativeError, -1, sMsg);

				NativeError = NativeError2;

#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
				SAString s;
				s.SetUTF16Chars(Sqlstate);
				sMsg += s;
#else
				sMsg += Sqlstate;
#endif
				sMsg += _TSA(" ");
#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
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
	}
	else if(g_infAPI.SQLError)
	{
		switch(HandleType)
		{
		case SQL_HANDLE_ENV:
			rc = g_infAPI.SQLError(
				Handle,
				NULL,
				NULL,
				Sqlstate, &NativeError,
				(SQLTCHAR *)szMsg, 4096, &TextLength);
			break;
		case SQL_HANDLE_DBC:
			rc = g_infAPI.SQLError(
				NULL,
				Handle,
				NULL,
				Sqlstate, &NativeError,
				(SQLTCHAR *)szMsg, 4096, &TextLength);
			break;
		case SQL_HANDLE_STMT:
			rc = g_infAPI.SQLError(
				NULL,
				NULL,
				Handle,
				Sqlstate, &NativeError,
				(SQLTCHAR *)szMsg, 4096, &TextLength);
			break;
		default:
			assert(false);
		}

		switch(rc)
		{
		case SQL_INVALID_HANDLE:
			sMsg = _TSA("SQL_INVALID_HANDLE");
			break;
		case SQL_NO_DATA:
			sMsg = _TSA("SQL_NO_DATA");
			break;
		default:
			{
#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
				SAString s; s.SetUTF16Chars(Sqlstate);
				sMsg += s;
#else
				sMsg += Sqlstate;
#endif
				sMsg += _TSA(" ");
#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
				s.SetUTF16Chars(szMsg);
				sMsg += s;
#else
				sMsg += szMsg;
#endif
			}
			break;
		}
	}
	else
		SAException::throwUserException(-1, _TSA("API bug"));

	throw SAException(pNested, SA_DBMS_API_Error, NativeError, -1, sMsg);
}

/*virtual */
bool IinfConnection::IsConnected() const
{
	return m_handles.m_hdbc != NULL;
}

/*virtual */
bool IinfConnection::IsAlive() const
{
	try
	{
		SACommand cmd(m_pSAConnection, _TSA("select 1 from systables"));
		//SACommand cmd(m_pSAConnection, _TSA("sqlcolumns"));
		cmd.Execute();
	}
	catch(SAException&)
	{
		return false;
	}

	return true;
}

void IinfConnection::SafeAllocEnv()
{
	if(g_infAPI.SQLAllocHandle)
		g_infAPI.SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_handles.m_hevn);
	else if(g_infAPI.SQLAllocEnv)
		g_infAPI.SQLAllocEnv(&m_handles.m_hevn);
	else
		SAException::throwUserException(-1, _TSA("API bug"));

	assert(m_handles.m_hevn);
}

void IinfConnection::SafeAllocConnect()
{
	assert(m_handles.m_hevn);

	if(g_infAPI.SQLAllocHandle)
		Check(g_infAPI.SQLAllocHandle(SQL_HANDLE_DBC, m_handles.m_hevn, &m_handles.m_hdbc), SQL_HANDLE_ENV, m_handles.m_hevn);
	else if( g_infAPI. SQLAllocConnect )
		Check(g_infAPI. SQLAllocConnect(m_handles.m_hevn, &m_handles.m_hdbc), SQL_HANDLE_ENV, m_handles.m_hevn);
	else
		SAException::throwUserException(-1, _TSA("API bug"));
}

void IinfConnection::SafeFreeEnv()
{
	assert(m_handles.m_hevn);

	if(g_infAPI.SQLFreeHandle)
		Check(g_infAPI.SQLFreeHandle(SQL_HANDLE_ENV, m_handles.m_hevn), SQL_HANDLE_ENV, m_handles.m_hevn);
	else if(g_infAPI.SQLFreeEnv)
		Check(g_infAPI.SQLFreeEnv(m_handles.m_hevn), SQL_HANDLE_ENV, m_handles.m_hevn);
	else
		SAException::throwUserException(-1, _TSA("API bug"));

	m_handles.m_hevn = NULL;
}

void IinfConnection::SafeFreeConnect()
{
	assert(m_handles.m_hdbc);

	if(g_infAPI.SQLFreeHandle)
		Check(g_infAPI.SQLFreeHandle(SQL_HANDLE_DBC, m_handles.m_hdbc), SQL_HANDLE_DBC, m_handles.m_hdbc);
	else if(g_infAPI.SQLFreeConnect)
		Check(g_infAPI.SQLFreeConnect(m_handles.m_hdbc), SQL_HANDLE_DBC, m_handles.m_hdbc);
	else
		SAException::throwUserException(-1, _TSA("API bug"));

	m_handles.m_hdbc = NULL;
}

void IinfConnection::SafeSetEnvAttr()
{
	assert(m_handles.m_hevn);

	if(g_infAPI.SQLSetEnvAttr)
		Check(g_infAPI.SQLSetEnvAttr(m_handles.m_hevn, SQL_ATTR_ODBC_VERSION, SQLPOINTER(SQL_OV_ODBC3), 0), SQL_HANDLE_ENV, m_handles.m_hevn);
}

/*virtual */
void IinfConnection::Connect(
	const SAString &sDBString,
	const SAString &sUserID,
	const SAString &sPassword,
	saConnectionHandler_t fHandler)
{
	assert(!m_handles.m_hevn);
	assert(!m_handles.m_hdbc);

	SafeAllocEnv();
	try
	{
		SafeSetEnvAttr();
		SafeAllocConnect();

		if( NULL != fHandler )
			fHandler(*m_pSAConnection, SA_PreConnectHandler);

#ifdef SQL_ATTR_USE_TRUSTED_CONTEXT
		if( sUserID.IsEmpty() ) // trusted connection, also TCTX=1
			Check(g_infAPI.SQLSetConnectAttr(m_handles.m_hdbc,
			SQL_ATTR_USE_TRUSTED_CONTEXT, (void*)SQL_TRUE, SQL_IS_INTEGER),
			SQL_HANDLE_DBC, m_handles.m_hdbc);
#endif

		if( SIZE_MAX == sDBString.Find(_TSA('=')) )	// it's not a valid connection string, but it can be DSN name
			Check(g_infAPI.SQLConnect(m_handles.m_hdbc, 
#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
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
			if( ! sUserID.IsEmpty() )
			{
				s += _TSA(";UID=");
				s += sUserID;
			}
			if( ! sPassword.IsEmpty() )
			{
				s += _TSA(";PWD=");
				s += sPassword;
			}

			Check(g_infAPI.SQLDriverConnect(m_handles.m_hdbc, NULL,
#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
				(SQLTCHAR *)s.GetUTF16Chars(),
#else
				(SQLTCHAR *)(const SAChar*)s,
#endif
				SQL_NTS, NULL, 0, 0, SQL_DRIVER_NOPROMPT),
				SQL_HANDLE_DBC, m_handles.m_hdbc);
		}

		char sVer[512];
		SQLSMALLINT StringLength;
		Check(g_infAPI.SQLGetInfo(
			m_handles.m_hdbc,
			SQL_DRIVER_ODBC_VER,
			sVer,
			512,
			&StringLength), SQL_HANDLE_DBC, m_handles.m_hdbc);
		m_nDriverODBCVer = SAExtractVersionFromString(sVer);

		//SQLINTEGER val = SQL_TRUE;
		Check(g_infAPI.SQLSetConnectAttr(m_handles.m_hdbc, SQL_INFX_ATTR_LO_AUTOMATIC, (SQLPOINTER)SQL_TRUE, SQL_IS_INTEGER), SQL_HANDLE_DBC, m_handles.m_hdbc);

		if( NULL != fHandler )
			fHandler(*m_pSAConnection, SA_PostConnectHandler);
	}
	catch(...)
	{
		// clean up
		if( m_handles.m_hdbc )
		{
			try
			{
				SafeFreeConnect();
			}
			catch(SAException &)
			{
			}
		}
		if( m_handles.m_hevn )
		{
			try
			{
				SafeFreeEnv();
			}
			catch(SAException &)
			{
			}
		}

		throw;
	}
}

/*virtual */
void IinfConnection::Disconnect()
{
	assert(m_handles.m_hevn);
	assert(m_handles.m_hdbc);

	Check(g_infAPI.SQLDisconnect(m_handles.m_hdbc), SQL_HANDLE_DBC, m_handles.m_hdbc);
	SafeFreeConnect();
	SafeFreeEnv();

	m_nDriverODBCVer = 0l;
}

/*virtual */
void IinfConnection::Destroy()
{
	assert(m_handles.m_hevn);
	assert(m_handles.m_hdbc);

	g_infAPI.SQLDisconnect(m_handles.m_hdbc);

	try
	{
		SafeFreeConnect();
	}
	catch(SAException &)
	{
	}
	m_handles.m_hdbc = NULL;

	try
	{
		SafeFreeEnv();
	}
	catch(SAException &)
	{
	}
	m_handles.m_hevn = NULL;

	m_nDriverODBCVer = 0l;
}

/*virtual */
void IinfConnection::Reset()
{
	m_handles.m_hdbc = NULL;
	m_handles.m_hevn = NULL;

	m_nDriverODBCVer = 0l;
}

void IinfConnection::SafeCommit()
{
	assert(m_handles.m_hdbc);

	if(g_infAPI.SQLEndTran)
		Check(g_infAPI.SQLEndTran(SQL_HANDLE_DBC, m_handles.m_hdbc, SQL_COMMIT), SQL_HANDLE_DBC, m_handles.m_hdbc);
	else if(g_infAPI.SQLTransact)
		Check(g_infAPI. SQLTransact(m_handles.m_hevn, m_handles.m_hdbc, SQL_COMMIT), SQL_HANDLE_DBC, m_handles.m_hdbc);
	else
		SAException::throwUserException(-1, _TSA("API bug"));
}

void IinfConnection::SafeRollback()
{
	assert(m_handles.m_hdbc);

	if(g_infAPI.SQLEndTran)
		Check(g_infAPI.SQLEndTran(SQL_HANDLE_DBC, m_handles.m_hdbc, SQL_ROLLBACK), SQL_HANDLE_DBC, m_handles.m_hdbc);
	else if(g_infAPI.SQLTransact)
		Check(g_infAPI. SQLTransact(m_handles.m_hevn, m_handles.m_hdbc, SQL_ROLLBACK), SQL_HANDLE_DBC, m_handles.m_hdbc);
	else
		SAException::throwUserException(-1, _TSA("API bug"));
}

/*virtual */
void IinfConnection::Commit()
{
	SafeCommit();
}

/*virtual */
void IinfConnection::Rollback()
{
	SafeRollback();
}

//////////////////////////////////////////////////////////////////////
// IinfClient Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IinfClient::IinfClient()
{

}

IinfClient::~IinfClient()
{

}

ISAConnection *IinfClient::QueryConnectionInterface(
	SAConnection *pSAConnection)
{
	return new IinfConnection(pSAConnection);
}

//////////////////////////////////////////////////////////////////////
// IinfCursor Class
//////////////////////////////////////////////////////////////////////

class IinfCursor : public ISACursor
{
	infCommandHandles	m_handles;

	SAString CallSubProgramSQL();
	static SADataType_t CnvtNativeToStd(
		int nNativeType,
		SQLULEN nPrec,
		int nScale);
	static SQLSMALLINT CnvtStdToNative(SADataType_t eDataType);
	SQLSMALLINT CnvtStdToNativeValueType(SADataType_t eDataType) const;

	void BindLongs();

	void SafeAllocStmt();
	void SafeFreeStmt();

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
	IinfCursor(
		IinfConnection *pIinfConnection,
		SACommand *pCommand);
	virtual ~IinfCursor();

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

IinfCursor::IinfCursor(
	IinfConnection *pIinfConnection,
	SACommand *pCommand) :
ISACursor(pIinfConnection, pCommand)
{
	Reset();
}

/*virtual */
IinfCursor::~IinfCursor()
{
}

/*virtual */
size_t IinfCursor::InputBufferSize(
	const SAParam &Param) const
{
	if(!Param.isNull())
	{
		switch(Param.DataType())
		{
		case SA_dtBool:
			return sizeof(unsigned char);	// SQL_C_BIT
		case SA_dtNumeric:
			return sizeof(SQL_NUMERIC_STRUCT);	// SQL_C_NUMERIC
		case SA_dtDateTime:
			return sizeof(TIMESTAMP_STRUCT);
		case SA_dtString:
			return (Param.asString().GetLength() * sizeof(TSAChar));
			//case SA_dtInterval:
			//	return sizeof(SQL_INTERVAL_STRUCT);
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
SADataType_t IinfCursor::CnvtNativeToStd(
	int dbtype,
	SQLULEN prec,
	int scale)
{
	SADataType_t eDataType = SA_dtUnknown;

	switch(dbtype)
	{
	case SQL_CHAR:		// Character string of fixed length
	case SQL_VARCHAR:	// Variable-length character string
	case SQL_WCHAR:		// Unicode character string of fixed length
	case SQL_WVARCHAR:	// Unicode variable-length character string
		eDataType = SA_dtString;
		break;
	case SQL_BINARY:
	case SQL_VARBINARY:
		eDataType = SA_dtBytes;
		break;
		//	case SQL_INFX_UDT_CLOB:
	case SQL_LONGVARCHAR:	// Variable-length character data
#ifdef SQL_WLONGVARCHAR
	case SQL_WLONGVARCHAR:	// Variable-length character data
#endif
		eDataType = SA_dtLongChar;
		break;
		//	case SQL_INFX_UDT_BLOB:
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
	case SQL_INTERVAL_YEAR:
	case SQL_INTERVAL_MONTH:
	case SQL_INTERVAL_DAY:	
	case SQL_INTERVAL_HOUR:
	case SQL_INTERVAL_MINUTE:
	case SQL_INTERVAL_SECOND:
	case SQL_INTERVAL_YEAR_TO_MONTH:
	case SQL_INTERVAL_DAY_TO_HOUR:
	case SQL_INTERVAL_DAY_TO_MINUTE:
	case SQL_INTERVAL_DAY_TO_SECOND:
	case SQL_INTERVAL_HOUR_TO_MINUTE:
	case SQL_INTERVAL_HOUR_TO_SECOND:
	case SQL_INTERVAL_MINUTE_TO_SECOND:
		//eDataType = SA_dtInterval;
		eDataType = SA_dtString;
		break;
	default:
        eDataType = SA_dtUnknown;
        break;
    }


	return eDataType;
}

SQLSMALLINT IinfCursor::CnvtStdToNativeValueType(SADataType_t eDataType) const
{
	SQLSMALLINT ValueType;

	switch(eDataType)
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
	case SA_dtInterval:
		ValueType = SQL_C_DEFAULT;
		break;
	default:
		ValueType = 0;
		assert(false);
	}

	if(((IinfConnection*)m_pISAConnection)->m_nDriverODBCVer < 0x00030000)
	{
		switch(eDataType)
		{
		case SA_dtDateTime:
			ValueType = SQL_C_TIMESTAMP;
		default:
			break;
		}
	}

	return ValueType;
}

/*static */
SQLSMALLINT IinfCursor::CnvtStdToNative(SADataType_t eDataType)
{
	SQLSMALLINT dbtype;

	switch(eDataType)
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
		// SQL_UNICODE is only for old versions but infxsql.h uses it even for driver version 3.70
		// Below is not correct but OK for all modern versions.
		if(g_infAPI.SQLSetEnvAttr)
			dbtype = SQL_WVARCHAR;
		else
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
		// SQL_UNICODE is only for old versions but infxsql.h uses it even for driver version 3.70
		// Below is not correct but OK for all modern versions.
		if(g_infAPI.SQLSetEnvAttr)
			dbtype = SQL_WLONGVARCHAR;
		else
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
bool IinfCursor::IsOpened()
{
	return m_handles.m_hstmt != NULL;
}

/*virtual */
void IinfCursor::Open()
{
	SafeAllocStmt();

	if( isSetScrollable() )
	{		
		g_infAPI.SQLSetStmtAttr(m_handles.m_hstmt,
			SQL_ATTR_CONCURRENCY, (SQLPOINTER)SQL_CONCUR_LOCK, SQL_IS_INTEGER);
		g_infAPI.SQLSetStmtAttr(m_handles.m_hstmt,
			SQL_ATTR_CURSOR_TYPE, (SQLPOINTER) SQL_CURSOR_DYNAMIC, SQL_IS_INTEGER);		
	}

	SAString sOption = m_pCommand->Option(_TSA("SQL_ATTR_QUERY_TIMEOUT"));
	if( ! sOption.IsEmpty() )
	{
		SQLPOINTER attrVal = (SQLPOINTER)sa_tol((const SAChar*)sOption);
		g_infAPI.SQLSetStmtAttr(m_handles.m_hstmt,
			SQL_ATTR_QUERY_TIMEOUT, attrVal, SQL_IS_UINTEGER);
	}

	sOption = m_pCommand->Option(_TSA("SetCursorName"));
	if( ! sOption.IsEmpty() )
		IinfConnection::Check(g_infAPI.SQLSetCursorName(m_handles.m_hstmt,
#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
		(SQLTCHAR *)sOption.GetUTF16Chars(),
#else
		(SQLTCHAR *)(const SAChar*)sOption,
#endif
		SQL_NTS), SQL_HANDLE_STMT, m_handles.m_hstmt);
}

void IinfCursor::SafeAllocStmt()
{
	assert(m_handles.m_hstmt == NULL);
	if(g_infAPI.SQLAllocHandle)
		IinfConnection::Check(g_infAPI.SQLAllocHandle(SQL_HANDLE_STMT, ((IinfConnection*)m_pISAConnection)->m_handles.m_hdbc, &m_handles.m_hstmt),
		SQL_HANDLE_DBC, ((IinfConnection*)m_pISAConnection)->m_handles.m_hdbc);
	else
		IinfConnection::Check(g_infAPI.SQLAllocStmt(((IinfConnection*)m_pISAConnection)->m_handles.m_hdbc, &m_handles.m_hstmt),
		SQL_HANDLE_DBC, ((IinfConnection*)m_pISAConnection)->m_handles.m_hdbc);
	assert(m_handles.m_hstmt != NULL);
}

void IinfCursor::SafeFreeStmt()
{
	if(g_infAPI.SQLFreeHandle)
		IinfConnection::Check(g_infAPI.SQLFreeHandle(SQL_HANDLE_STMT, m_handles.m_hstmt), SQL_HANDLE_STMT, m_handles.m_hstmt);
	else
		IinfConnection::Check(g_infAPI.SQLFreeStmt(m_handles.m_hstmt, SQL_DROP), SQL_HANDLE_STMT, m_handles.m_hstmt);

	m_handles.m_hstmt = NULL;
}

/*virtual */
void IinfCursor::Close()
{
	assert(m_handles.m_hstmt != NULL);

	SafeFreeStmt();
}

/*virtual */
void IinfCursor::Destroy()
{
	assert(m_handles.m_hstmt != NULL);

	try
	{
		SafeFreeStmt();
	}
	catch(SAException &)
	{
	}

	m_handles.m_hstmt = NULL;
}

/*virtual */
void IinfCursor::Reset()
{
	m_handles.m_hstmt = NULL;
	m_bResultSetCanBe = false;
}

/*virtual */
ISACursor *IinfConnection::NewCursor(SACommand *m_pCommand)
{
	return new IinfCursor(this, m_pCommand);
}

/*virtual */
void IinfCursor::Prepare(
	const SAString &sStmt,
	SACommandType_t eCmdType,
	int nPlaceHolderCount,
	saPlaceHolder**	ppPlaceHolders)
{
	SAString sStmtInf;
	size_t nPos = 0l;
	int i;
	switch(eCmdType)
	{
	case SA_CmdSQLStmt:
		// replace bind variables with '?' place holder
		for(i = 0; i < nPlaceHolderCount; i++)
		{
			sStmtInf += sStmt.Mid(nPos, ppPlaceHolders[i]->getStart()-nPos);
			sStmtInf += _TSA("?");
			nPos = ppPlaceHolders[i]->getEnd() + 1;
		}
		// copy tail
		if(nPos < sStmt.GetLength())
			sStmtInf += sStmt.Mid(nPos);
		break;
	case SA_CmdStoredProc:
		sStmtInf = CallSubProgramSQL();
		break;
	case SA_CmdSQLStmtRaw:
		sStmtInf = sStmt;
		break;
	default:
		assert(false);
	}

	// a bit of clean up
	IinfConnection::Check(g_infAPI.SQLFreeStmt(m_handles.m_hstmt, SQL_CLOSE), SQL_HANDLE_STMT, m_handles.m_hstmt);
	IinfConnection::Check(g_infAPI.SQLFreeStmt(m_handles.m_hstmt, SQL_UNBIND), SQL_HANDLE_STMT, m_handles.m_hstmt);
	IinfConnection::Check(g_infAPI.SQLFreeStmt(m_handles.m_hstmt, SQL_RESET_PARAMS), SQL_HANDLE_STMT, m_handles.m_hstmt);

	SA_TRACE_CMD(sStmtInf);
	IinfConnection::Check(g_infAPI.SQLPrepare(m_handles.m_hstmt,
#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
		(SQLTCHAR *)sStmtInf.GetUTF16Chars(),
#else
		(SQLTCHAR *)(const SAChar*)sStmtInf,
#endif
		SQL_NTS), SQL_HANDLE_STMT, m_handles.m_hstmt);}

SAString IinfCursor::CallSubProgramSQL()
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
	// Informix specific: unlike other drivers
	// if SP with no parameters is called it should have "()"
	sSQL += _TSA("(");
	if(!sParams.IsEmpty())
	{
		sSQL += sParams;
	}
	sSQL += _TSA(")");

	sSQL += _TSA("}");
	return sSQL;
}

void IinfCursor::BindLongs()
{
	SQLRETURN retcode;
	SQLPOINTER ValuePtr;

	try
	{
		while((retcode = g_infAPI.SQLParamData(m_handles.m_hstmt, &ValuePtr)) == SQL_NEED_DATA)
		{
			SAParam *pParam = (SAParam *)ValuePtr;
			size_t nActualWrite;
			SAPieceType_t ePieceType = SA_FirstPiece;
			void *pBuf;

			SADummyConverter DummyConverter;
			ISADataConverter *pIConverter = &DummyConverter;
			size_t nCnvtSize, nCnvtPieceSize = 0;
			SAPieceType_t eCnvtPieceType;
#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
			bool bUnicode = pParam->ParamType() == SA_dtCLob || pParam->DataType() == SA_dtCLob ||
				pParam->ParamType() == SA_dtLongChar || pParam->DataType() == SA_dtLongChar;
			SAUnicodeUTF16Converter UnicodeUtf2Converter;
			if( bUnicode )
				pIConverter = &UnicodeUtf2Converter;
#endif
			do
			{
				nActualWrite = pParam->InvokeWriter(ePieceType, IinfConnection::MaxLongAtExecSize, pBuf);
				pIConverter->PutStream((unsigned char*)pBuf, nActualWrite, ePieceType);
				// smart while: initialize nCnvtPieceSize before calling pIConverter->GetStream
				while(nCnvtPieceSize = nActualWrite,
					pIConverter->GetStream((unsigned char*)pBuf, nCnvtPieceSize, nCnvtSize, eCnvtPieceType))
					IinfConnection::Check(g_infAPI.SQLPutData(m_handles.m_hstmt, pBuf, (SQLINTEGER)nCnvtSize),
					SQL_HANDLE_STMT, m_handles.m_hstmt);

				if(ePieceType == SA_LastPiece)
					break;
			}
			while( nActualWrite != 0 );
		}
	}
	catch(SAException&)
	{
		g_infAPI.SQLCancel(m_handles.m_hstmt);
		throw;
	}

	if(retcode != SQL_NO_DATA && retcode != SQL_NEED_DATA)
		IinfConnection::Check(retcode, SQL_HANDLE_STMT, m_handles.m_hstmt);
}

/*virtual */
void IinfCursor::UnExecute()
{
	m_bResultSetCanBe = false;
}

void IinfCursor::Bind(
	int nPlaceHolderCount,
	saPlaceHolder**	ppPlaceHolders)
{
	// we should bind for every place holder ('?')
	AllocBindBuffer(nPlaceHolderCount, ppPlaceHolders,
		sizeof(SQLINTEGER), 0);
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
		SQLSMALLINT ParameterType = CnvtStdToNative(
			eParamType == SA_dtUnknown? SA_dtString : eParamType);
		SQLSMALLINT ValueType = CnvtStdToNativeValueType(
			eDataType == SA_dtUnknown? SA_dtString : eDataType);

		SQLLEN *StrLen_or_IndPtr = (SQLLEN *)pInd;
		SQLPOINTER ParameterValuePtr = (SQLPOINTER)pValue;
		SQLINTEGER BufferLength = (SQLINTEGER)nDataBufSize;
		SQLUINTEGER ColumnSize = (SQLUINTEGER)Param.ParamSize();
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
				*StrLen_or_IndPtr = SQL_NULL_DATA;		// field is null
			else
				*StrLen_or_IndPtr = (SQLINTEGER)InputBufferSize(Param);	// field is not null

			if(!Param.isNull())
			{
				switch(eDataType)
				{
				case SA_dtUnknown:
					throw SAException(SA_Library_Error, SA_Library_Error_UnknownParameterType, -1, IDS_UNKNOWN_PARAMETER_TYPE, (const SAChar*)Param.Name());
				case SA_dtBool:
					assert(*StrLen_or_IndPtr == (SQLINTEGER)sizeof(unsigned char));
					*(unsigned char*)ParameterValuePtr = (unsigned char)Param.asBool();
					break;
				case SA_dtShort:
					assert(*StrLen_or_IndPtr == (SQLINTEGER)sizeof(short));
					*(short*)ParameterValuePtr = Param.asShort();
					break;
				case SA_dtUShort:
					assert(*StrLen_or_IndPtr == (SQLINTEGER)sizeof(unsigned short));
					*(unsigned short*)ParameterValuePtr = Param.asUShort();
					break;
				case SA_dtLong:
					assert(*StrLen_or_IndPtr == (SQLINTEGER)sizeof(long));
					*(int*)ParameterValuePtr = (int)Param.asLong();
					break;
				case SA_dtULong:
					assert(*StrLen_or_IndPtr == (SQLINTEGER)sizeof(unsigned long));
					*(unsigned int*)ParameterValuePtr = (unsigned int)Param.asULong();
					break;
				case SA_dtDouble:
					assert(*StrLen_or_IndPtr == (SQLINTEGER)sizeof(double));
					*(double*)ParameterValuePtr = Param.asDouble();
					break;
				case SA_dtNumeric:
					assert(*StrLen_or_IndPtr == (SQLINTEGER)sizeof(SQL_NUMERIC_STRUCT));
					IinfConnection::CnvtNumericToInternal(
						Param.asNumeric(),
						*(SQL_NUMERIC_STRUCT*)ParameterValuePtr);
					break;
				case SA_dtDateTime:
					assert(*StrLen_or_IndPtr == (SQLINTEGER)sizeof(TIMESTAMP_STRUCT));
					IinfConnection::CnvtDateTimeToInternal(
						Param.asDateTime(),
						*(TIMESTAMP_STRUCT*)ParameterValuePtr);
					break;
				case SA_dtString:
					assert(*StrLen_or_IndPtr == (SQLINTEGER)(Param.asString().GetLength() * sizeof(TSAChar)));
					memcpy(ParameterValuePtr,
#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
						Param.asString().GetUTF16Chars(),
#else
						(const SAChar*)Param.asString(),
#endif
						*StrLen_or_IndPtr);
					break;
				case SA_dtBytes:
					assert(*StrLen_or_IndPtr == (SQLINTEGER)Param.asBytes().GetBinaryLength());
					memcpy(ParameterValuePtr, (const void*)Param.asBytes(), *StrLen_or_IndPtr);
					break;
				case SA_dtLongBinary:
				case SA_dtBLob:
				case SA_dtLongChar:
				case SA_dtCLob:
					assert(*StrLen_or_IndPtr == 0);
					*StrLen_or_IndPtr = ((IinfConnection*)m_pISAConnection)->LenDataAtExec();
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
			IinfConnection::Check(g_infAPI.SQLBindParameter(
				m_handles.m_hstmt,
				(SQLUSMALLINT)(i+1),
				InputOutputType,
				ValueType,
				ParameterType,
				0, // ColumnSize
				0, // DecimalDigits
				&Param,	// ParameterValuePtr - context
				0, // Buffer length
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

			IinfConnection::Check(g_infAPI.SQLBindParameter(
				m_handles.m_hstmt,
				(SQLUSMALLINT)(i+1),
				InputOutputType,
				ValueType,
				ParameterType,
				Precision,	// ColumnSize
				Scale,		// DecimalDigits
				ParameterValuePtr,
				BufferLength,
				StrLen_or_IndPtr), SQL_HANDLE_STMT, m_handles.m_hstmt);

			// TODO: see DB2
		}
		else
		{
			IinfConnection::Check(g_infAPI.SQLBindParameter(
				m_handles.m_hstmt,
				(SQLUSMALLINT)(i+1),
				InputOutputType,
				ValueType,
				ParameterType,
				ColumnSize,
				0,	// DecimalDigits
				ParameterValuePtr,
				BufferLength,
				StrLen_or_IndPtr), SQL_HANDLE_STMT, m_handles.m_hstmt);
		}
	}
}

/*virtual */
void IinfCursor::Execute(
	int nPlaceHolderCount,
	saPlaceHolder **ppPlaceHolders)
{
	if(nPlaceHolderCount)
		Bind(nPlaceHolderCount, ppPlaceHolders);

	SQLRETURN rc;
	IinfConnection::Check(g_infAPI.SQLFreeStmt(m_handles.m_hstmt, SQL_CLOSE), SQL_HANDLE_STMT, m_handles.m_hstmt);

	rc = g_infAPI.SQLExecute(m_handles.m_hstmt);

	if(rc != SQL_NEED_DATA)
	{
		if(rc != SQL_NO_DATA)	// SQL_NO_DATA is OK for DELETEs or UPDATEs that haven't affected any rows
			IinfConnection::Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);
	}
	else
		BindLongs();

	m_bResultSetCanBe = true;

	ConvertOutputParams();	// if any
}

/*virtual */
void IinfCursor::Cancel()
{
	IinfConnection::Check(g_infAPI.SQLCancel(
		m_handles.m_hstmt), SQL_HANDLE_STMT, m_handles.m_hstmt);
}

/*virtual */
bool IinfCursor::ResultSetExists()
{
	if(!m_bResultSetCanBe)
		return false;

	SQLSMALLINT ColumnCount;
	IinfConnection::Check(g_infAPI.SQLNumResultCols(
		m_handles.m_hstmt, &ColumnCount), SQL_HANDLE_STMT, m_handles.m_hstmt);
	return (ColumnCount > 0);
}

/*virtual */
void IinfCursor::DescribeFields(
	DescribeFields_cb_t fn)
{
	SQLSMALLINT ColumnCount;
	IinfConnection::Check(g_infAPI.SQLNumResultCols(m_handles.m_hstmt, &ColumnCount), SQL_HANDLE_STMT, m_handles.m_hstmt);

	for(SQLSMALLINT nField = 1; nField <= ColumnCount; ++nField)
	{
		SQLTCHAR szColName[1024];
		SQLSMALLINT nColLen;
		SQLSMALLINT DataType;
		SQLULEN ColumnSize = 0l;
		SQLSMALLINT Nullable;
		SQLSMALLINT DecimalDigits;

		IinfConnection::Check(g_infAPI.SQLDescribeCol(
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
#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
		sColName.SetUTF16Chars(szColName, nColLen);
#else
		sColName = SAString((const SAChar*)szColName, nColLen);
#endif

		nColLen = 0;
		g_infAPI.SQLColAttribute(m_handles.m_hstmt, nField,
			SQL_DESC_BASE_COLUMN_NAME, szColName, 1024, &nColLen, NULL);
		SAString sColBaseName;
#ifdef SA_UNICODE
		sColBaseName.SetUTF16Chars(szColName, nColLen);
#else
		sColBaseName = SAString((const SAChar*)szColName, nColLen);
#endif
		nColLen = 0;
		g_infAPI.SQLColAttribute(m_handles.m_hstmt, nField,
			SQL_DESC_TABLE_NAME, szColName, 1024, &nColLen, NULL);
		SAString sColBaseTableName;
#ifdef SA_UNICODE
		sColBaseTableName.SetUTF16Chars(szColName, nColLen);
#else
		sColBaseTableName = SAString((const SAChar*)szColName, nColLen);
#endif
		nColLen = 0;
		g_infAPI.SQLColAttribute(m_handles.m_hstmt, nField,
			SQL_DESC_CATALOG_NAME, szColName, 1024, &nColLen, NULL);
		SAString sColCatalogName;
#ifdef SA_UNICODE
		sColCatalogName.SetUTF16Chars(szColName, nColLen);
#else
		sColCatalogName = SAString((const SAChar*)szColName, nColLen);
#endif

		(m_pCommand->*fn)(
			sColName,
			CnvtNativeToStd(DataType, ColumnSize, DecimalDigits),
			(int)DataType,
			ColumnSize,
			(int)ColumnSize,
			DecimalDigits,
			Nullable == SQL_NO_NULLS,
            (int)ColumnCount);
	}
}

/*virtual */
long IinfCursor::GetRowsAffected()
{
	assert(m_handles.m_hstmt != NULL);

	SQLLEN RowCount = -1l;

	IinfConnection::Check(g_infAPI.SQLRowCount(m_handles.m_hstmt, &RowCount), SQL_HANDLE_STMT, m_handles.m_hstmt);
	return (long)RowCount;
}

/*virtual */
size_t IinfCursor::OutputBufferSize(
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
		return sizeof(SQL_NUMERIC_STRUCT);	// SQL_C_NUMERIC
	case SA_dtString:
		return (sizeof(TSAChar) * (nDataSize + 1));	// always allocate space for NULL
	case SA_dtDateTime:
		return sizeof(TIMESTAMP_STRUCT);
		//case SA_dtInterval:
		//	return sizeof(SQL_INTERVAL_STRUCT);
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
void IinfCursor::SetFieldBuffer(
	int nCol,	// 1-based
	void *pInd,
	size_t nIndSize,
	void * /*pSize*/,
	size_t/* nSizeSize*/,
	void *pValue,
	size_t nBufSize)
{
	assert(nIndSize == sizeof(SQLINTEGER));
	if(nIndSize != sizeof(SQLINTEGER))
		return;

	SAField &Field = m_pCommand->Field(nCol);

    if (Field.FieldType() == SA_dtUnknown) // ignore unknown field type
        return;

	SQLSMALLINT TargetType = CnvtStdToNativeValueType(Field.FieldType());
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
		assert(nBufSize == sizeof(SQL_NUMERIC_STRUCT));
		break;
	case SA_dtDateTime:
		assert(nBufSize == sizeof(TIMESTAMP_STRUCT));
		break;
		//case SA_dtInterval:
		//	assert(nBufSize == sizeof(SQL_INTERVAL_STRUCT));
		//	break;
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
		TargetType = 0;
		assert(false);	// unknown type
	}

	if(!bLong)
	{
		IinfConnection::Check(g_infAPI.SQLBindCol(
			m_handles.m_hstmt,
			(SQLUSMALLINT)nCol,
			TargetType,
			pValue,
			(SQLINTEGER)nBufSize,
			(SQLLEN*)pInd), SQL_HANDLE_STMT, m_handles.m_hstmt);

		// TODO : see DB2
	}
}

/*virtual */
void IinfCursor::SetSelectBuffers()
{
	// use default helpers
	AllocSelectBuffer(sizeof(SQLINTEGER), 0);
}

/*virtual */
bool IinfCursor::ConvertIndicator(
	int nPos,	// 1-based
	int/* nNotConverted*/,
	SAValueRead &vr,
	ValueType_t eValueType,
	void *pInd, size_t nIndSize,
	void * /*pSize*/, size_t/* nSizeSize*/,
	size_t &nRealSize,
	int/* nBulkReadingBufPos*/) const
{
	assert(nIndSize == sizeof(SQLINTEGER));
	if(nIndSize != sizeof(SQLINTEGER))
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
		SQLLEN StrLen_or_IndPtr;

		IinfConnection::Check(g_infAPI.SQLGetData(
			m_handles.m_hstmt, (SQLUSMALLINT)nPos,
			TargetType, Buf, bAddSpaceForNull ? sizeof(TSAChar):0,
			&StrLen_or_IndPtr), SQL_HANDLE_STMT, m_handles.m_hstmt);

        *vr.m_pbNull = ((SQLINTEGER)StrLen_or_IndPtr == SQL_NULL_DATA);
		if(!vr.isNull())
		{
			// also try to get long size
			if(StrLen_or_IndPtr >= 0)
				nRealSize = StrLen_or_IndPtr;
			else
				nRealSize = 0l;	// unknown
		}
		return true;	// converted
	}

    // set unknown data size to null
    *vr.m_pbNull = eDataType == SA_dtUnknown || *(SQLINTEGER*)pInd == SQL_NULL_DATA;
	if(!vr.isNull())
		nRealSize = *(SQLINTEGER*)pInd;
	return true;	// converted
}

/*virtual */
bool IinfCursor::IgnoreUnknownDataType()
{
    return true;
}

/*virtual */
void IinfCursor::ConvertString(SAString &String, const void *pData, size_t nRealSize)
{
	// nRealSize is in bytes but we need in characters,
	// so nRealSize should be a multiply of character size
	assert(nRealSize % sizeof(TSAChar) == 0);
#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
	String.SetUTF16Chars(pData, nRealSize/sizeof(TSAChar));
#else
	String = SAString((const SAChar*)pData, nRealSize/sizeof(SAChar));
#endif
}

/*virtual */
bool IinfCursor::FetchNext()
{
	SQLRETURN rc = g_infAPI.SQLFetch(m_handles.m_hstmt);

	if( rc != SQL_NO_DATA )
	{
		IinfConnection::Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);

		// use default helpers
		ConvertSelectBufferToFields(0);
	}
	else if( ! isSetScrollable() )
		m_bResultSetCanBe = false;

	return (rc != SQL_NO_DATA);
}

/*virtual */
bool IinfCursor::FetchPrior()
{
	if( NULL == g_infAPI.SQLFetchScroll )
		return false;

	SQLRETURN rc = g_infAPI.SQLFetchScroll(m_handles.m_hstmt, SQL_FETCH_PRIOR, 0);

	if( rc != SQL_NO_DATA )
	{
		IinfConnection::Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);

		// use default helpers
		ConvertSelectBufferToFields(0);
	}

	return (rc != SQL_NO_DATA);
}

/*virtual */
bool IinfCursor::FetchFirst()
{
	if( NULL == g_infAPI.SQLFetchScroll )
		return false;

	SQLRETURN rc = g_infAPI.SQLFetchScroll(m_handles.m_hstmt, SQL_FETCH_FIRST, 0);

	if( rc != SQL_NO_DATA )
	{
		IinfConnection::Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);

		// use default helpers
		ConvertSelectBufferToFields(0);
	}

	return (rc != SQL_NO_DATA);
}

/*virtual */
bool IinfCursor::FetchLast()
{
	if( NULL == g_infAPI.SQLFetchScroll )
		return false;

	SQLRETURN rc = g_infAPI.SQLFetchScroll(m_handles.m_hstmt, SQL_FETCH_LAST, 0);

	if( rc != SQL_NO_DATA )
	{
		IinfConnection::Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);

		// use default helpers
		ConvertSelectBufferToFields(0);
	}

	return (rc != SQL_NO_DATA);
}

/*virtual */
bool IinfCursor::FetchPos(int offset, bool Relative/* = false*/)
{
    if (NULL == g_infAPI.SQLFetchScroll)
        return false;

    SQLRETURN rc = g_infAPI.SQLFetchScroll(m_handles.m_hstmt,
        Relative ? SQL_FETCH_RELATIVE : SQL_FETCH_ABSOLUTE, (SQLLEN)offset);

    if (rc != SQL_NO_DATA)
    {
        IinfConnection::Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);

        // use default helpers
        ConvertSelectBufferToFields(0);
    }

    return (rc != SQL_NO_DATA);
}

/*virtual */
void IinfCursor::ReadLongOrLOB(
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
#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
		SAUTF16UnicodeConverter Utf16UnicodeConverter;
		if( bAddSpaceForNull )
			pIConverter = &Utf16UnicodeConverter;
#endif

		// try to get long size
		size_t nLongSize = 0l;
		TSAChar Buf[1];
		rc = g_infAPI.SQLGetData(
			m_handles.m_hstmt, (SQLUSMALLINT)Field.Pos(),
			TargetType, Buf, bAddSpaceForNull ? sizeof(TSAChar):0,
			&StrLen_or_IndPtr);
		if( rc != SQL_NO_DATA )
		{
			IinfConnection::Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);
			if(StrLen_or_IndPtr >= 0)
				nLongSize = (StrLen_or_IndPtr * (bAddSpaceForNull ? sizeof(SAChar)/sizeof(TSAChar):1));
		}

		unsigned char* pBuf;
		size_t nPortionSize = vr.PrepareReader(
			nLongSize,
			IinfConnection::MaxLongAtExecSize,
			pBuf,
			fnReader,
			nReaderWantedPieceSize,
			pAddlData,
			bAddSpaceForNull);
		assert(nPortionSize <= IinfConnection::MaxLongAtExecSize);

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

			rc = g_infAPI.SQLGetData(
				m_handles.m_hstmt, (SQLUSMALLINT)Field.Pos(),
				TargetType, pBuf, nPortionSize + (bAddSpaceForNull ? sizeof(TSAChar):0),
				&StrLen_or_IndPtr);

			// Some ODBC drivers can return 0 size instead of SQL_NO_DATA (for BLOB/CLOB?)
			//assert(nPortionSize || rc == SQL_NO_DATA);

			if(rc != SQL_NO_DATA)
			{
				IinfConnection::Check(rc, SQL_HANDLE_STMT, m_handles.m_hstmt);
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
void IinfCursor::DescribeParamSP()
{
	SAString sText = m_pCommand->CommandText();
	SAString sSchemaName;
	SAString sProcName;

	size_t n = sText.Find('.');
	if( SIZE_MAX == n )
		sProcName = sText;
	else
	{
		sSchemaName = sText.Left(n);
		sProcName = sText.Mid(n+1);
	}

	SACommand cmd(m_pISAConnection->getSAConnection());
	cmd.Open();
	infCommandHandles *pHandles = (infCommandHandles *)cmd.NativeHandles();

	if( sSchemaName.IsEmpty() )
		sSchemaName = SQL_ALL_SCHEMAS;

	IinfConnection::Check(g_infAPI.SQLProcedureColumns(
		pHandles->m_hstmt, NULL, 0,
#if defined(SA_UNICODE) && defined(SQLAPI_INFORMIX_UTF16)
		(SQLTCHAR *)sSchemaName.GetUTF16Chars(),
		(SQLSMALLINT)sSchemaName.GetUTF16CharsLength(), //SQL_NTS,
		(SQLTCHAR *)sProcName.GetUTF16Chars(),
		(SQLSMALLINT)sProcName.GetUTF16CharsLength(), //SQL_NTS,
#else
		(SQLTCHAR *)(const SAChar*)sSchemaName,
		(SQLSMALLINT)sSchemaName.GetLength(), //SQL_NTS,
		(SQLTCHAR *)(const SAChar*)sProcName,
		(SQLSMALLINT)sProcName.GetLength(), //SQL_NTS,
#endif
		(SQLTCHAR *)NULL, 0), SQL_HANDLE_STMT, pHandles->m_hstmt);

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
		switch(nColType)
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
		SADataType_t eParamType = CnvtNativeToStd(nDataType, nColSize, nDecDigits);

		SAString sParamName;
		if(sCol.IsEmpty())
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
saAPI *IinfConnection::NativeAPI() const
{
	return &g_infAPI;
}

/*virtual */
saConnectionHandles *IinfConnection::NativeHandles()
{
	return &m_handles;
}

/*virtual */
saCommandHandles *IinfCursor::NativeHandles()
{
	return &m_handles;
}

/*virtual */
void IinfConnection::setIsolationLevel(
	SAIsolationLevel_t eIsolationLevel)
{
	Commit();

	issueIsolationLevel(eIsolationLevel);
}

void IinfConnection::SafeSetConnectOption(
	SQLINTEGER Attribute,
	SQLUINTEGER ValuePtr,
	SQLINTEGER StringLength)
{
	assert(m_handles.m_hdbc);

	if(g_infAPI.SQLSetConnectAttr)
		Check(g_infAPI.SQLSetConnectAttr(
		m_handles.m_hdbc,
		Attribute,
		(SQLPOINTER)ValuePtr, StringLength), SQL_HANDLE_DBC, m_handles.m_hdbc);
	else if(g_infAPI.SQLSetConnectOption)
		Check(g_infAPI.SQLSetConnectOption(
		m_handles.m_hdbc,
		(SQLUSMALLINT)Attribute,
		ValuePtr),
		SQL_HANDLE_DBC, m_handles.m_hdbc);
	else
		SAException::throwUserException(-1, _TSA("API bug"));

}

void IinfConnection::issueIsolationLevel(
	SAIsolationLevel_t eIsolationLevel)
{
	SQLUINTEGER isolation;
	switch(eIsolationLevel)
	{
	case SA_ReadUncommitted:
		isolation = (SQLUINTEGER)SQL_TXN_READ_UNCOMMITTED;
		break;
	case SA_ReadCommitted:
		isolation = (SQLUINTEGER)SQL_TXN_READ_COMMITTED;
		break;
	case SA_RepeatableRead:
		isolation = (SQLUINTEGER)SQL_TXN_REPEATABLE_READ;
		break;
	case SA_Serializable:
		isolation = (SQLUINTEGER)SQL_TXN_SERIALIZABLE;
		break;
	default:
		assert(false);
		return;
	}

	SafeSetConnectOption(SQL_ATTR_TXN_ISOLATION, isolation, 0);
}

/*virtual */
void IinfConnection::setAutoCommit(
	SAAutoCommit_t eAutoCommit)
{
	SQLUINTEGER nAutoCommit;
	switch(eAutoCommit)
	{
	case SA_AutoCommitOff:
		nAutoCommit = (SQLUINTEGER)SQL_AUTOCOMMIT_OFF;
		break;
	case SA_AutoCommitOn:
		nAutoCommit = (SQLUINTEGER)SQL_AUTOCOMMIT_ON;
		break;
	default:
		assert(false);
		return;
	}

	SafeSetConnectOption(SQL_ATTR_AUTOCOMMIT, nAutoCommit, 0);
}

/*virtual */
void IinfConnection::CnvtInternalToNumeric(
	SANumeric &numeric,
	const void *pInternal,
	int nInternalSize)
{
	if( nInternalSize != int(sizeof(SQL_NUMERIC_STRUCT)) )
		return;

	SQL_NUMERIC_STRUCT *pSQL_NUMERIC_STRUCT = (SQL_NUMERIC_STRUCT *)pInternal;
	numeric.precision = pSQL_NUMERIC_STRUCT->precision;
	numeric.scale = pSQL_NUMERIC_STRUCT->scale;
	// set sign
	// Informix CLI: 1 if positive, 0 if negative
	// SQLAPI: 1 if positive, 0 if negative
	numeric.sign = pSQL_NUMERIC_STRUCT->sign;
	// copy mantissa
	// Informix CLI: little endian format
	// SQLAPI: little endian format
	memset(numeric.val, 0, sizeof(numeric.val));
	memcpy(
		numeric.val,
		pSQL_NUMERIC_STRUCT->val,
		sa_min(sizeof(numeric.val), sizeof(pSQL_NUMERIC_STRUCT->val)));
}

/*static */
void IinfConnection::CnvtNumericToInternal(
	const SANumeric &numeric,
	SQL_NUMERIC_STRUCT &Internal)
{
	Internal.precision = numeric.precision;
	Internal.scale = numeric.scale;
	// set sign
	// Informix CLI: 1 if positive, 0 if negative
	// SQLAPI: 1 if positive, 0 if negative
	Internal.sign = numeric.sign;
	// copy mantissa
	// Informix CLI: little endian format
	// SQLAPI: little endian format
	memset(Internal.val, 0, sizeof(Internal.val));
	memcpy(
		Internal.val,
		numeric.val,
		sa_min(sizeof(numeric.val), sizeof(Internal.val)));
}
