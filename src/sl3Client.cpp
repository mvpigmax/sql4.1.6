//////////////////////////////////////////////////////////////////////
// sl3Client.cpp
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <sl3API.h>
#include "sl3Client.h"

class Isl3Connection : public ISAConnection
{
	friend class Isl3Cursor;

	SAMutex m_execMutex;

	void StartTransactionIndirectly();
	bool m_bTransactionStarted;

	sl3ConnectionHandles m_handles;
	enum MaxLong {MaxLongPiece = (unsigned int)SA_DefaultMaxLong};

    void Check(int nError);

protected:
	virtual ~Isl3Connection();

public:
	Isl3Connection(SAConnection *pSAConnection);

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

	virtual ISACursor *NewCursor(SACommand *pCommand);

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

protected:
	SADataType_t CnvtNativeToStd(int type, const SAChar* declType, int length);
	void Commit(SAAutoCommit_t eAutoCommit);
	bool IsDateTimeType(SAString& sType);
	const char *CmdBeginTransaction(void);
	static bool CnvtInternalToDateTime(SADateTime &dt, const SAChar *sInternal, int nInternalSize);
};

class Isl3Cursor : public ISACursor
{
	sl3CommandHandles	m_handles;
	bool m_bOpened;
	long m_nRowsAffected;
	int m_nExecResult;
	void BindBLob(SAParam &Param);
	void BindCLob(SAParam &Param);
    Isl3Connection* getConnection();

public:
	Isl3Cursor(
		Isl3Connection *pIsl3Connection,
		SACommand *pCommand);
	virtual ~Isl3Cursor();

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
};

//////////////////////////////////////////////////////////////////////
// Isl3Client Class
//////////////////////////////////////////////////////////////////////

Isl3Client::Isl3Client()
{
}

Isl3Client::~Isl3Client()
{
}

ISAConnection *Isl3Client::QueryConnectionInterface(
	SAConnection *pSAConnection)
{
	return new Isl3Connection(pSAConnection);
}

//////////////////////////////////////////////////////////////////////
// Check functions
//////////////////////////////////////////////////////////////////////
void Isl3Connection::Check(int nError)
{
	assert(m_handles.pDb);

	if( SQLITE_OK != nError &&
		SQLITE_DONE != nError &&
		SQLITE_ROW != nError )
	{
        SACriticalSectionScope scope(&m_execMutex);

		SAString sMessage;
        int nDbError = g_sl3API.sqlite3_errcode(m_handles.pDb);
        if (nDbError == nError)
#ifdef SA_UNICODE
            sMessage.SetUTF16Chars(g_sl3API.sqlite3_errmsg(m_handles.pDb), SIZE_MAX);
#else
            sMessage = (const char*)g_sl3API.sqlite3_errmsg(m_handles.pDb);
#endif
        else
            sMessage.Format(_TSA("Fatal error code %d"), nError);

        throw SAException(SA_DBMS_API_Error, nError, -1, sMessage);
	}
}

Isl3Connection* Isl3Cursor::getConnection()
{
    assert(m_pISAConnection);
    return (Isl3Connection*)m_pISAConnection;
}

//////////////////////////////////////////////////////////////////////
// Isl3Connection Class
//////////////////////////////////////////////////////////////////////

Isl3Connection::Isl3Connection(
	SAConnection *pSAConnection) : ISAConnection(pSAConnection)
{
	Reset();
}

Isl3Connection::~Isl3Connection()
{
}

/*virtual*/
void Isl3Connection::InitializeClient()
{
	::AddSQLite3Support(m_pSAConnection);
}

/*virtual*/
void Isl3Connection::UnInitializeClient()
{
	if( SAGlobals::UnloadAPI() ) 
        ::ReleaseSQLite3Support();
}

/*virtual*/
long Isl3Connection::GetClientVersion() const
{
	// SQLite number version = (X*1000000 + Y*1000 + Z)
	int v = g_sl3API.sqlite3_libversion_number();
	return ((((v / 1000000) & 0xFFFF) << 16) | (3006023 % 100000 / 1000));
}

/*virtual*/
long Isl3Connection::GetServerVersion() const
{
	return GetClientVersion();
}

/*virtual*/
SAString Isl3Connection::GetServerVersionString() const
{
	return SAString(g_sl3API.sqlite3_libversion());
}

/*virtual*/
bool Isl3Connection::IsConnected() const
{
	return (NULL != m_handles.pDb);
}

/*virtual*/
bool Isl3Connection::IsAlive() const
{
	return (NULL != m_handles.pDb);
}

/*virtual*/
void Isl3Connection::Connect(
	const SAString &sDBString,
	const SAString & /*sUserID*/,
	const SAString & /*sPassword*/,
	saConnectionHandler_t fHandler)
{
	assert(NULL == m_handles.pDb);

	try
	{
		if( NULL != fHandler )
			fHandler(*m_pSAConnection, SA_PreConnectHandler);

		SAString sVFSOption = m_pSAConnection->Option(_TSA("SQLiteVFSName"));
		SAString sVFSFlagsOption = m_pSAConnection->Option(_TSA("SQLiteVFSFlags"));
		if( ! sVFSFlagsOption.IsEmpty() )
            Check(g_sl3API.sqlite3_open_v2(
#ifdef SA_UNICODE
			sDBString.GetUTF8Chars(),
#else
			sDBString.GetMultiByteChars(),
#endif
			&m_handles.pDb, sa_toi((const SAChar*)sVFSFlagsOption),
			sVFSOption.IsEmpty() ? NULL:sVFSOption.GetMultiByteChars()));
		else
            Check(g_sl3API.sqlite3_open(
#ifdef SA_UNICODE
			sDBString.GetUTF16Chars(),
#else
			sDBString.GetMultiByteChars(),
#endif
			&m_handles.pDb));

		SAString sOption = m_pSAConnection->Option(_TSA("BusyTimeout"));
		if( ! sOption.IsEmpty() )
            Check(g_sl3API.sqlite3_busy_timeout(m_handles.pDb,
				sa_toi((const SAChar*)sOption)));

		if( NULL != fHandler )
			fHandler(*m_pSAConnection, SA_PostConnectHandler);
	}
	catch( SAException& )
	{
		Destroy();
		throw;
	}
}

/*virtual*/
void Isl3Connection::Disconnect()
{
	assert(NULL != m_handles.pDb);

	Check(g_sl3API.sqlite3_close(m_handles.pDb));
	m_handles.pDb = NULL;
}

/*virtual*/
void Isl3Connection::Destroy()
{
    if (NULL != m_handles.pDb)
    {
        g_sl3API.sqlite3_close(m_handles.pDb);
        m_handles.pDb = NULL;
    }	
}

/*virtual*/
void Isl3Connection::Reset()
{
	m_handles.pDb = NULL;
	m_bTransactionStarted = false;
}

/*virtual*/
void Isl3Connection::setIsolationLevel(
	SAIsolationLevel_t /*eIsolationLevel*/)
{
	// commit changes and start new transaction if necessary
	Commit(m_pSAConnection->AutoCommit());
}

/*virtual*/
void Isl3Connection::setAutoCommit(
	SAAutoCommit_t eAutoCommit)
{
	// commit changes and start new transaction if necessary
	Commit(eAutoCommit);
}

/*virtual*/
void Isl3Connection::Commit()
{
	Commit(m_pSAConnection->AutoCommit());
}

const char *Isl3Connection::CmdBeginTransaction(void)
{
	SAString sTranType = m_pSAConnection->Option(_TSA("SQLiteTransactionType"));
	if( 0 == sTranType.CompareNoCase(_TSA("IMMEDIATE")) )
		return "BEGIN IMMEDIATE";
	else if( 0 == sTranType.CompareNoCase(_TSA("EXCLUSIVE")) )
		return "BEGIN EXCLUSIVE";
	return "BEGIN";
}

void Isl3Connection::StartTransactionIndirectly()
{
	if(!m_bTransactionStarted && m_pSAConnection->AutoCommit() == SA_AutoCommitOff)
	{
		SA_TRACE_CMDC(SAString(CmdBeginTransaction()));
		Check(g_sl3API.sqlite3_exec(m_handles.pDb,
			CmdBeginTransaction(), NULL, NULL, NULL));
		m_bTransactionStarted = true;
	}
}

/*virtual*/
void Isl3Connection::Rollback()
{
	if( m_bTransactionStarted )
	{
        SA_TRACE_CMDC(_TSA("ROLLBACK"));
		Check(g_sl3API.sqlite3_exec(m_handles.pDb,
			"ROLLBACK", NULL, NULL, NULL));
		m_bTransactionStarted = false;
	}

	// start transaction only when first query executed
}

/*virtual*/
saAPI *Isl3Connection::NativeAPI() const
{
	return 	&g_sl3API;
}

/*virtual*/
saConnectionHandles *Isl3Connection::NativeHandles()
{
	return &m_handles;
}

/*virtual*/
ISACursor *Isl3Connection::NewCursor(SACommand *pCommand)
{
	return new Isl3Cursor(this, pCommand);
}

/*virtual*/
void Isl3Connection::CnvtInternalToNumeric(
	SANumeric & /*numeric*/,
	const void * /*pInternal*/,
	int /*nInternalSize*/)
{
	// TODO
}

/*virtual*/
void Isl3Connection::CnvtInternalToDateTime(
	SADateTime & /*date_time*/,
	const void * /*pInternal*/,
	int /*nInternalSize*/)
{
	// TODO
}

/*virtual*/
void Isl3Connection::CnvtInternalToInterval(
	SAInterval & /*interval*/,
	const void * /*pInternal*/,
	int /*nInternalSize*/)
{
	// TODO
}

/*virtual*/
void Isl3Connection::CnvtInternalToCursor(
	SACommand * /*pCursor*/,
	const void * /*pInternal*/)
{
	// TODO
}

/*static */
bool Isl3Connection::CnvtInternalToDateTime(SADateTime &dt, const SAChar *sInternal, int nInternalSize)
{
	if( 0 == sa_strncmp(sInternal, _TSA("now"), nInternalSize) )
		dt = SADateTime::currentDateTimeWithFraction();
	else
	{
		SAChar* endPt = NULL;
		double dVal = sa_strtod(sInternal, &endPt);
		if( dVal != 0 && *endPt == '\0' )
			dt = dVal - 2415018.5;
		else
		{
			int year = 0, month = 0, day = 0, hour = 0, minute = 0, sec = 0, ss = 0, n = 
				sa_sscanf(sInternal, sa_strchr(sInternal, _TSA('T')) ?
				_TSA("%d-%d-%dT%d:%d:%d.%d"):_TSA("%d-%d-%d %d:%d:%d.%d"),
				&year, &month, &day, &hour, &minute, &sec, &ss);
			if( n < 3 ) // time only?
			{
				n = sa_sscanf(sInternal, _TSA("%d:%d:%d.%d"), &hour, &minute, &sec, &ss);
				if( n >= 2 )
				{
					year = 2000;
					month = day = 1;
					n = 3;
				}
			}
			if( n >= 3 )
			{
				dt = SADateTime(year, month, day, hour, minute, sec);
				dt.Fraction() = ss * 1000000;
			}
			else
				return false;
		}
	}

	return true;
}

bool Isl3Connection::IsDateTimeType(SAString& sType)
{
	if( ! sType.IsEmpty() )
	{
		sType.MakeUpper();

		SAString dateTypes = m_pSAConnection->Option(_TSA("SQLiteDateTypes"));
		if( dateTypes.IsEmpty() )
			dateTypes = _TSA("DATE,DATETIME,TIME,TIMESTAMP");
		else
			dateTypes.MakeUpper();

		size_t nPos = dateTypes.Find(sType);
		size_t nLen = sType.GetLength();

		if( nPos != SIZE_MAX &&
			(0 == nPos || _TSA(',') == ((const SAChar*)dateTypes)[nPos - 1]) &&
			((nPos + nLen) == dateTypes.GetLength() ||
			_TSA(',') == ((const SAChar*)dateTypes)[nPos + nLen]) )
			return true;
	}

	return false;
}

SADataType_t Isl3Connection::CnvtNativeToStd(int type, const SAChar* declType, int /*length*/)
{
	SAString sType(NULL != declType ? declType:_TSA(""));

	switch( type )
	{
	case SQLITE_INTEGER:
	case SQLITE_FLOAT:
		if( 0 == m_pSAConnection->Option(_TSA("SQLiteDateValueType")).CompareNoCase(_TSA("DOUBLE"))
			&& IsDateTimeType(sType) )
			return SA_dtDateTime;
		return 	SQLITE_INTEGER == type ? SA_dtNumeric:SA_dtDouble;

	case SQLITE_TEXT:
		if( m_pSAConnection->Option(_TSA("SQLiteDateValueType")).CompareNoCase(_TSA("DOUBLE"))
			&& IsDateTimeType(sType) )
			return SA_dtDateTime;
		return SA_dtString;

	case SQLITE_BLOB:
		return SA_dtBytes;

	case SQLITE_NULL:
		if( ! sType.IsEmpty() )
		{
			sType.MakeUpper();
			if( IsDateTimeType(sType) )
				return SA_dtDateTime;
			else if( SIZE_MAX != sType.Find(_TSA("INT")) )
				return SA_dtNumeric;
			else if( SIZE_MAX != sType.Find(_TSA("CHAR")) || SIZE_MAX != sType.Find(_TSA("TEXT")) || SIZE_MAX != sType.Find(_TSA("CLOB")) )
				return SA_dtString;
			else if( SIZE_MAX != sType.Find(_TSA("BLOB")) || SIZE_MAX != sType.Find(_TSA("BYTE")) || SIZE_MAX != sType.Find(_TSA("BINARY")) )
				return SA_dtBytes;
			else if( SIZE_MAX != sType.Find(_TSA("REAL")) || SIZE_MAX != sType.Find(_TSA("FLOA")) || SIZE_MAX != sType.Find(_TSA("DOUB")) )
				return SA_dtDouble;
		}
		return SA_dtString;

	default:
		assert(false);
		return SA_dtString;
	}
}

void Isl3Connection::Commit(SAAutoCommit_t /*eAutoCommit*/)
{
	if( m_bTransactionStarted )
	{
        SA_TRACE_CMDC(_TSA("COMMIT"));
		Check(g_sl3API.sqlite3_exec(m_handles.pDb,
			"COMMIT", NULL, NULL, NULL));
		m_bTransactionStarted = false;
	}

	// start transaction only when first query executed
}

//////////////////////////////////////////////////////////////////////
// Isl3Cursor Class
//////////////////////////////////////////////////////////////////////

Isl3Cursor::Isl3Cursor(
	Isl3Connection *pIsl3Connection,
	SACommand *pCommand) :
ISACursor(pIsl3Connection, pCommand)
{
	Reset();
}

/*virtual */
Isl3Cursor::~Isl3Cursor()
{
}

/*virtual */
size_t Isl3Cursor::InputBufferSize(
	const SAParam & /*Param*/) const
{
	assert(NULL != m_handles.pStmt);
	return 0; // we will directly use parameter buffer/value;
}

/*virtual */
size_t Isl3Cursor::OutputBufferSize(
	SADataType_t /*eDataType*/,
	size_t /*nDataSize*/) const
{
	assert(false);
	return 0;
}

/*virtual */
bool Isl3Cursor::ConvertIndicator(
	int /*nPos*/,	// 1-based, can be field or param pos, 0-for return value from SP
	int /*nNotConverted*/,
	SAValueRead & /*vr*/,
	ValueType_t /*eValueType*/,
	void * /*pInd*/, size_t /*nIndSize*/,
	void * /*pSize*/, size_t /*nSizeSize*/,
	size_t & /*nRealSize*/,
	int /*nBulkReadingBufPos*/) const
{
	assert(false);
	return false;
}

/*virtual */
bool Isl3Cursor::IsOpened()
{
	return m_bOpened;
}

/*virtual */
void Isl3Cursor::Open()
{
	assert(!m_bOpened);
	m_bOpened = true;
}

/*virtual */
void Isl3Cursor::Close()
{
	assert(m_bOpened);

	if( NULL != m_handles.pStmt )
	{
        g_sl3API.sqlite3_finalize(m_handles.pStmt);
		m_handles.pStmt = NULL;
	}
}

/*virtual */
void Isl3Cursor::Destroy()
{
	m_bOpened = false;

	if( NULL != m_handles.pStmt )
	{
        g_sl3API.sqlite3_finalize(m_handles.pStmt);
		m_handles.pStmt = NULL;
	}
}

/*virtual */
void Isl3Cursor::Reset()
{
	m_bOpened = false;
	m_nRowsAffected = 0l;
	m_nExecResult = SQLITE_OK;

	m_handles.pStmt = NULL;
}

/*virtual */
void Isl3Cursor::Prepare(
	const SAString &sStmt,
	SACommandType_t /*eCmdType*/,
	int /*nPlaceHolderCount*/,
	saPlaceHolder ** /*ppPlaceHolders*/)
{
	if( NULL != m_handles.pStmt )
	{
		g_sl3API.sqlite3_finalize(m_handles.pStmt);
		m_handles.pStmt = NULL;
	}

	SA_TRACE_CMD(sStmt);
    getConnection()->Check(g_sl3API.sqlite3_prepare(getConnection()->m_handles.pDb,
#ifdef SA_UNICODE
		sStmt.GetUTF16Chars(),
#else
		sStmt.GetMultiByteChars(),
#endif
		-1, &m_handles.pStmt, NULL));
}

/*virtual */
void Isl3Cursor::Execute(
	int /*nPlaceHolderCount*/,
	saPlaceHolder ** /*ppPlaceHolders*/)
{
	assert(NULL != m_handles.pStmt);

    getConnection()->StartTransactionIndirectly();

	// bind parameters
	for(int i = 0; i < m_pCommand->ParamCount(); ++i )
	{
		SAParam &p = m_pCommand->ParamByIndex(i);
		SAString sParamName = _TSA(":") + p.Name();

		int nIdx = g_sl3API.sqlite3_bind_parameter_index(
#ifdef SA_UNICODE
			m_handles.pStmt, sParamName.GetUTF8Chars());
#else
			m_handles.pStmt, sParamName.GetMultiByteChars());
#endif
		if( 0 >= nIdx )
			continue;

		if( p.isNull() )
		{
			getConnection()->Check(g_sl3API.sqlite3_bind_null(
				m_handles.pStmt, nIdx));
			continue;
		}

		switch( p.DataType() )
		{
		case SA_dtShort:
		case SA_dtUShort:
		case SA_dtBool:
		case SA_dtLong:
            getConnection()->Check(g_sl3API.sqlite3_bind_int64(
				m_handles.pStmt, nIdx, p.asLong()));
			break;

		case SA_dtULong:
            getConnection()->Check(g_sl3API.sqlite3_bind_int64(
				m_handles.pStmt, nIdx, p.asULong()));
			break;

		case SA_dtLongBinary:
		case SA_dtBLob:
			// read the data into internal buffer
			BindBLob(p);
		case SA_dtBytes:
            getConnection()->Check(g_sl3API.sqlite3_bind_blob(
				m_handles.pStmt, nIdx,
				(const void*)p.asBytes(), (int)p.asBytes().GetBinaryLength(),
				SQLITE_TRANSIENT));
			break;

		case SA_dtDateTime:
			if( 0 == m_pCommand->Option(_TSA("SQLiteDateValueType")).CompareNoCase(_TSA("DOUBLE")) )
			{
				// bind datetime as double
                getConnection()->Check(g_sl3API.sqlite3_bind_double(
					m_handles.pStmt, nIdx, (double)p.asDateTime() + 2415018.5));
			}
			else
			{
				SAString sDate;
				SADateTime dt = p.asDateTime();
				sDate.Format(_TSA("%04u-%02u-%02u %02u:%02u:%02u.%03u"),
					dt.GetYear(), dt.GetMonth(), dt.GetDay(),
					dt.GetHour(), dt.GetMinute(), dt.GetSecond(),
					(unsigned int)(dt.Fraction() / 1000000));
                getConnection()->Check(g_sl3API.sqlite3_bind_text(m_handles.pStmt, nIdx,
#ifdef SA_UNICODE
					sDate.GetUTF16Chars(),  -1, // SQLite needs the size in bytes
#else
					sDate.GetMultiByteChars(), (int)sDate.GetMultiByteCharsLength(), // SQLite needs the size in bytes
#endif
					SQLITE_TRANSIENT));
			}
			break;

		case SA_dtDouble:
            getConnection()->Check(g_sl3API.sqlite3_bind_double(
				m_handles.pStmt, nIdx, p.asDouble()));
			break;

		case SA_dtNumeric:
			if( 0 == p.asNumeric().precision )
                getConnection()->Check(g_sl3API.sqlite3_bind_int64(
				m_handles.pStmt, nIdx, (sa_int64_t)p.asNumeric()));
			else
                getConnection()->Check(g_sl3API.sqlite3_bind_double(
				m_handles.pStmt, nIdx, p.asDouble()));
			break;

		case SA_dtLongChar:
		case SA_dtCLob:
			// read the data into internal buffer
			BindCLob(p);
		case SA_dtString:
            getConnection()->Check(g_sl3API.sqlite3_bind_text(
				m_handles.pStmt, nIdx,
#ifdef SA_UNICODE
				p.asString().GetUTF16Chars(),  -1, // SQLite needs the size in bytes
#else
				p.asString().GetMultiByteChars(), (int)p.asString().GetMultiByteCharsLength(), // SQLite needs the size in bytes
#endif
				SQLITE_STATIC));
			break;

		case SA_dtCursor:
		case SA_dtInterval:
		case SA_dtSpecificToDBMS:
		case SA_dtUnknown:
		default:
			assert(false);
			break;
		}
	}

	m_nExecResult = g_sl3API.sqlite3_step(m_handles.pStmt);
	if( SQLITE_DONE == m_nExecResult && 0 >= g_sl3API.sqlite3_column_count(m_handles.pStmt) )
	{
		m_nRowsAffected = g_sl3API.sqlite3_changes(
			g_sl3API.sqlite3_db_handle(m_handles.pStmt));
		// Experimental! reset the statement to avoid possible deadlock
        getConnection()->Check(g_sl3API.sqlite3_reset(m_handles.pStmt));
        getConnection()->Check(g_sl3API.sqlite3_clear_bindings(m_handles.pStmt));
	}
	else if( SQLITE_ROW != m_nExecResult )
	{
		// Experimental! reset the statement to avoid possible deadlock
        getConnection()->Check(g_sl3API.sqlite3_reset(m_handles.pStmt));
        getConnection()->Check(g_sl3API.sqlite3_clear_bindings(m_handles.pStmt));

        getConnection()->Check(m_nExecResult);
	}
}

void Isl3Cursor::BindBLob(SAParam &Param)
{
	if( NULL != Param.m_fnWriter )
	{
		size_t nWritten = 0l;
		size_t nActualWrite;
		SAPieceType_t ePieceType = SA_FirstPiece;
		void *pSrc;
		unsigned char *pDst;
		while((nActualWrite = Param.InvokeWriter(
			ePieceType, Isl3Connection::MaxLongPiece, pSrc)) != 0)
		{
			pDst = (unsigned char *)
				Param.m_pString->GetBinaryBuffer(nWritten + nActualWrite);
			pDst += nWritten;
			memcpy(pDst, pSrc, nActualWrite);
			nWritten += nActualWrite;
			Param.m_pString->ReleaseBinaryBuffer(nWritten);

			if(ePieceType == SA_LastPiece)
				break;
		}
	}
}

void Isl3Cursor::BindCLob(SAParam &Param)
{
	if( NULL != Param.m_fnWriter )
	{
		size_t nWritten = 0l;
		size_t nActualWrite;
		SAPieceType_t ePieceType = SA_FirstPiece;
		void *pSrc;
		unsigned char *pDst;
		while((nActualWrite = Param.InvokeWriter(
			ePieceType, Isl3Connection::MaxLongPiece, pSrc)) != 0)
		{
			pDst = (unsigned char *)
				Param.m_pString->GetBinaryBuffer(nWritten + nActualWrite);
			pDst += nWritten;
			memcpy(pDst, pSrc, nActualWrite);
			nWritten += nActualWrite;
			Param.m_pString->ReleaseBinaryBuffer(nWritten);

			if(ePieceType == SA_LastPiece)
				break;
		}
	}
}

/*virtual */
void Isl3Cursor::UnExecute()
{
	assert(NULL != m_handles.pStmt);

	m_nRowsAffected = 0;
    getConnection()->Check(g_sl3API.sqlite3_reset(m_handles.pStmt));
    getConnection()->Check(g_sl3API.sqlite3_clear_bindings(m_handles.pStmt));
	m_nExecResult = SQLITE_OK;
}

/*virtual */
void Isl3Cursor::Cancel()
{
	if( NULL != m_handles.pStmt )
		g_sl3API.sqlite3_interrupt(
		g_sl3API.sqlite3_db_handle(m_handles.pStmt));
	m_nExecResult = SQLITE_OK;
}

/*virtual */
bool Isl3Cursor::ResultSetExists()
{
	return (NULL != m_handles.pStmt && (SQLITE_ROW == m_nExecResult ||
		(SQLITE_DONE == m_nExecResult && g_sl3API.sqlite3_column_count(m_handles.pStmt) > 0)));
}

/*virtual */
void Isl3Cursor::DescribeFields(
	DescribeFields_cb_t fn)
{
	assert(NULL != m_handles.pStmt);

	int iCol = g_sl3API.sqlite3_column_count(m_handles.pStmt);

	for( int i = 0; i < iCol; ++i )
	{
		SAString name, declType;
#ifdef SA_UNICODE
		name.SetUTF16Chars(g_sl3API.sqlite3_column_name(m_handles.pStmt, i), SIZE_MAX);
		declType.SetUTF16Chars(g_sl3API.sqlite3_column_decltype(m_handles.pStmt, i), SIZE_MAX);
#else
		name = (const char*)g_sl3API.sqlite3_column_name(m_handles.pStmt, i);
		declType = (const char*)g_sl3API.sqlite3_column_decltype(m_handles.pStmt, i);
#endif
		int type = g_sl3API.sqlite3_column_type(m_handles.pStmt, i);
		int length = g_sl3API.sqlite3_column_bytes(m_handles.pStmt, i);
		SADataType_t eDataType = getConnection()->CnvtNativeToStd(type, declType, length);
		(m_pCommand->*fn)(name, eDataType, type, length, 0, 0, false, iCol);
	}
}

/*virtual */
void Isl3Cursor::SetSelectBuffers()
{
	// Do nothing - SQLite uses own field data buffer and we can access it directly.
}

/*virtual */
void Isl3Cursor::SetFieldBuffer(
	int /*nCol*/,	// 1-based
	void * /*pInd*/,
	size_t /*nIndSize*/,
	void * /*pSize*/,
	size_t /*nSizeSize*/,
	void * /*pValue*/,
	size_t /*nValueSize*/)
{
	assert(false);
}

/*virtual */
bool Isl3Cursor::FetchNext()
{
	if( NULL != m_handles.pStmt )
	{
		if( SQLITE_ROW == m_nExecResult )
		{
			for( int i = 0; i < m_pCommand->FieldCount(); ++i )
			{
				SAField &f = m_pCommand->Field(i+1);
				*f.m_pbNull = false;

				switch( g_sl3API.sqlite3_column_type(m_handles.pStmt, i) )
				{
				case SQLITE_INTEGER:
					if( f.FieldType() == SA_dtDateTime )
					{
						f.m_eDataType = SA_dtDateTime;
						*(f.m_pDateTime) =
							SADateTime(g_sl3API.sqlite3_column_double(m_handles.pStmt, i) - 2415018.5);
					}
					else
					{
						sa_int64_t value = g_sl3API.sqlite3_column_int64(m_handles.pStmt, i);
						if ((value < 0x7FFFFFF) && (value > -0x7FFFFFF))
						{
							// just an integer 
							f.m_eDataType = SA_dtLong;
							*(long*)f.m_pScalar = (long)value;
						}
						else if ((value <= 0xFFFFFFFF) && (value >= 0))
						{
							// just an integer 
							f.m_eDataType = SA_dtULong;
							*(unsigned long*)f.m_pScalar = (unsigned long)value;
						}
						else
						{
							f.m_eDataType = SA_dtNumeric;
							*f.m_pNumeric = (sa_int64_t)value; // Mandrake Linux 2006, gcc 4.0.1 needs
						}
					}
					break;

				case SQLITE_FLOAT:
					if( f.FieldType() == SA_dtDateTime )
					{
						f.m_eDataType = SA_dtDateTime;
						*(f.m_pDateTime) =
							SADateTime(g_sl3API.sqlite3_column_double(m_handles.pStmt, i) - 2415018.5);
					}
					else
					{
						f.m_eDataType = SA_dtDouble;
						*((double*)f.m_pScalar) =
							g_sl3API.sqlite3_column_double(m_handles.pStmt, i);
					}
					break;

				case SQLITE_BLOB:
					f.m_eDataType = SA_dtBytes;
					*f.m_pString = SAString(
						g_sl3API.sqlite3_column_blob(m_handles.pStmt, i),
						g_sl3API.sqlite3_column_bytes(m_handles.pStmt, i));
					break;

				case SQLITE_NULL:
					*f.m_pbNull = true;
					break;

				case SQLITE_TEXT:
#ifdef SA_UNICODE
					f.m_pString->SetUTF16Chars(g_sl3API.sqlite3_column_text(m_handles.pStmt, i));
#else
					*f.m_pString = SAString((const char*)
						g_sl3API.sqlite3_column_text(m_handles.pStmt, i),
						g_sl3API.sqlite3_column_bytes(m_handles.pStmt, i));
#endif
					if( f.FieldType() == SA_dtDateTime && 
						Isl3Connection::CnvtInternalToDateTime(*f.m_pDateTime,
						(const SAChar*)*f.m_pString, (int)f.m_pString->GetLength()) )
						f.m_eDataType = SA_dtDateTime;
					else
						f.m_eDataType = SA_dtString;
					break;

				default:
					f.m_eDataType = SA_dtString;
#ifdef SA_UNICODE
					f.m_pString->SetUTF16Chars(g_sl3API.sqlite3_column_text(m_handles.pStmt, i));
#else
					*f.m_pString = SAString((const char*)
						g_sl3API.sqlite3_column_text(m_handles.pStmt, i),
						g_sl3API.sqlite3_column_bytes(m_handles.pStmt, i));
#endif						
					break;
				}
			}

            getConnection()->Check(m_nExecResult = g_sl3API.sqlite3_step(m_handles.pStmt));

			return true;
		}
		else if( SQLITE_DONE == m_nExecResult )
			// Experimental! reset the statement to avoid possible deadlock
			UnExecute();
	}

	return false;
}

/*virtual */
long Isl3Cursor::GetRowsAffected()
{
	// return saved value
	return m_nRowsAffected;
}

/*virtual */
void Isl3Cursor::ReadLongOrLOB(
	ValueType_t /*eValueType*/,
	SAValueRead & vr,
	void * /*pValue*/,
	size_t /*nFieldBufSize*/,
	saLongOrLobReader_t fnReader,
	size_t nReaderWantedPieceSize,
	void * pAddlData)
{
	// we already read the data so we only pass it from the value string into reader
	assert( NULL != m_handles.pStmt ); 

	// is it default reader?
	if( NULL == fnReader )
		return;

	// get long data size
	size_t nLongSize = vr.asString().GetBinaryLength();
	const void* pData = vr.asString();

	unsigned char* pBuf;
	size_t nPieceSize = vr.PrepareReader(
		nLongSize,
		Isl3Connection::MaxLongPiece,
		pBuf,
		fnReader,
		nReaderWantedPieceSize,
		pAddlData);
	assert(nPieceSize > 0 && nPieceSize <= Isl3Connection::MaxLongPiece);

	SAPieceType_t ePieceType = SA_FirstPiece;
	size_t nTotalRead = 0l;

	do
	{
		nPieceSize = sa_min(nPieceSize, (nLongSize - nTotalRead));
		memcpy(pBuf, (const char*)pData + nTotalRead, nPieceSize);
		nTotalRead += nPieceSize;
		assert(nTotalRead <= nLongSize);

		if( nTotalRead == nLongSize )
		{
			if(ePieceType == SA_NextPiece)
				ePieceType = SA_LastPiece;
			else
			{
				assert(ePieceType == SA_FirstPiece);
				ePieceType = SA_OnePiece;
			}
		}

		vr.InvokeReader(ePieceType, pBuf, nPieceSize);

		if( SA_FirstPiece == ePieceType )
			ePieceType = SA_NextPiece;
	}
	while(nTotalRead < nLongSize);
}

/*virtual */
void Isl3Cursor::DescribeParamSP()
{
	// Don't supported
}

/*virtual */
saCommandHandles *Isl3Cursor::NativeHandles()
{
	return &m_handles;
}
