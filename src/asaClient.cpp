//////////////////////////////////////////////////////////////////////
// asaClient.cpp
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <asaAPI.h>
#include "asaClient.h"

class IasaConnection : public ISAConnection
{
	friend class IasaCursor;

	SAMutex m_execMutex;

	asaConnectionHandles m_handles;
	enum MaxLong {MaxLongPiece = (unsigned int)SA_DefaultMaxLong};

protected:
	virtual ~IasaConnection();

public:
	IasaConnection(SAConnection *pSAConnection);

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
	void Check();
	void Cancel();
	static a_sqlany_data_type CnvtStdToNative(SADataType_t eDataType);
	static SADataType_t CnvtNativeToStd(
		a_sqlany_native_type native_type, a_sqlany_data_type type,
		size_t max_size, unsigned short precision, unsigned short scale);
};

class IasaCursor : public ISACursor
{
	asaCommandHandles	m_handles;

public:
	IasaCursor(
		IasaConnection *pIasaConnection,
		SACommand *pCommand);
	virtual ~IasaCursor();

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

#ifdef SA_UNICODE
	virtual void ConvertString(
		SAString &String,
		const void *pData,
		size_t nRealSize);
#endif

protected:
	void Check() const;
	SAString CallSubProgramSQL();
	SADataType_t CnvtDomainIdToStd(
		int dbtype, 
		int/* dbsubtype*/,
		int/* dbsize*/,
		int prec, 
		int scale) const;
	void Bind(int nPlaceHolderCount, saPlaceHolder **ppPlaceHolders);
	void ParseDateTime(SADateTime& dt, const SAString& s, a_sqlany_native_type type);
};

//////////////////////////////////////////////////////////////////////
// IasaClient Class
//////////////////////////////////////////////////////////////////////

IasaClient::IasaClient()
{
}

IasaClient::~IasaClient()
{
}

ISAConnection *IasaClient::QueryConnectionInterface(
	SAConnection *pSAConnection)
{
	return new IasaConnection(pSAConnection);
}

//////////////////////////////////////////////////////////////////////
// Check function(s)
//////////////////////////////////////////////////////////////////////
#define ERROR_BUFFER_LENGTH 256

void IasaConnection::Check()
{
	SACriticalSectionScope scope(&m_execMutex);

	assert(NULL != m_handles.pDb);

	char szError[ERROR_BUFFER_LENGTH];
	sacapi_i32 nError = g_asaAPI.sqlany_error(m_handles.pDb, szError, ERROR_BUFFER_LENGTH);
	if( nError < 0 )
	{
		SAString sMessage;
#ifdef SA_UNICODE
		sMessage.SetUTF8Chars(szError, SIZE_MAX);
#else
		sMessage = szError;
#endif
		size_t nLen = g_asaAPI.sqlany_sqlstate(m_handles.pDb, szError, ERROR_BUFFER_LENGTH);
		SAString s;
		if( nLen > 0 ) 
		{
			szError[nLen] = '\0';
			s = SAString(szError) + _TSA(" ");
		}
		s += sMessage;

		g_asaAPI.sqlany_clear_error(m_handles.pDb);

		throw SAException(SA_DBMS_API_Error, (int)nError, -1, s);
	}
}

void IasaCursor::Check() const
{
	assert(NULL != m_pISAConnection);
	((IasaConnection*)m_pISAConnection)->Check();
}

//////////////////////////////////////////////////////////////////////
// IasaConnection Class
//////////////////////////////////////////////////////////////////////

IasaConnection::IasaConnection(
	SAConnection *pSAConnection) : ISAConnection(pSAConnection)
{
	Reset();
}

IasaConnection::~IasaConnection()
{
}

/*virtual*/
void IasaConnection::InitializeClient()
{
	::AddSQLAnywhereSupport(m_pSAConnection);

	sacapi_u32 api_version;
	if( ! g_asaAPI.sqlany_init("SQLAPI", SQLANY_API_VERSION_2, &api_version) )
		throw SAException(SA_Library_Error, 0, -1,
		_TSA("Failed to initialize the interface! Supported version=%d"), api_version );
}

/*virtual*/
void IasaConnection::UnInitializeClient()
{
	if( SAGlobals::UnloadAPI() )
	{
		g_asaAPI.sqlany_fini();
		::ReleaseSQLAnywhereSupport();
	}
}

/*virtual*/
long IasaConnection::GetClientVersion() const
{
	long lVersion = 0l;
	char szVersion[64];
	if( g_asaAPI.sqlany_client_version(szVersion, 64) )
	{
		char* sz = strtok(szVersion, ".");
		if( sz )
			lVersion = (atoi(sz) & 0xFFFF) << 16;
		sz = strtok(NULL, ".");
		if( sz )
			lVersion |= (atoi(sz) & 0xFFFF);
	}

	return lVersion;
}

/*virtual*/
long IasaConnection::GetServerVersion() const
{
	return SAExtractVersionFromString(GetServerVersionString());
}

/*virtual*/
SAString IasaConnection::GetServerVersionString() const
{
	SACommand cmd(m_pSAConnection, 
		_TSA("select dbo.xp_msver('FileDescription') || ' ' || dbo.xp_msver('ProductVersion')"), SA_CmdSQLStmt);
	cmd.Execute();

	if( cmd.FetchNext() )
		return cmd.Field(1).asString();

	return SAString(_TSA("Unknown"));
}

/*virtual*/
bool IasaConnection::IsConnected() const
{
	return (NULL != m_handles.pDb);
}

/*virtual*/
bool IasaConnection::IsAlive() const
{
	return false;
}

/*virtual*/
void IasaConnection::Connect(
	const SAString &sDBString,
	const SAString & sUserID,
	const SAString & sPassword,
	saConnectionHandler_t fHandler)
{
	SACriticalSectionScope scope(&m_execMutex);

	assert(NULL == m_handles.pDb);

	try
	{
		m_handles.pDb = g_asaAPI.sqlany_new_connection();

		if( NULL != fHandler )
			fHandler(*m_pSAConnection, SA_PreConnectHandler);

		SAString sConnectString;
		if( !sUserID.IsEmpty() )
			sConnectString += _TSA("UID=") + sUserID + _TSA(";");
		if( !sPassword.IsEmpty() )
			sConnectString += _TSA("PWD=") + sPassword + _TSA(";");

#ifdef SA_UNICODE
		sConnectString += _TSA("CS=UTF-8;");
#endif

		sConnectString += sDBString;

		if( ! g_asaAPI.sqlany_connect(m_handles.pDb, sConnectString.GetMultiByteChars()) )
			Check();

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
void IasaConnection::Disconnect()
{
	SACriticalSectionScope scope(&m_execMutex);

	assert(NULL != m_handles.pDb);

	if( !g_asaAPI.sqlany_disconnect(m_handles.pDb) )
		Check();
	g_asaAPI.sqlany_free_connection(m_handles.pDb);
	m_handles.pDb = NULL;
}

/*virtual*/
void IasaConnection::Destroy()
{
	SACriticalSectionScope scope(&m_execMutex);

	if( NULL != m_handles.pDb )
	{
		g_asaAPI.sqlany_disconnect(m_handles.pDb);
		g_asaAPI.sqlany_free_connection(m_handles.pDb);
		m_handles.pDb = NULL;
	}
}

/*virtual*/
void IasaConnection::Reset()
{
	SACriticalSectionScope scope(&m_execMutex);

	m_handles.pDb = NULL;
}

void IasaConnection::Cancel()
{
	assert(m_handles.pDb);
	g_asaAPI.sqlany_cancel(m_handles.pDb);
}

/*virtual*/
void IasaConnection::setIsolationLevel(
	SAIsolationLevel_t eIsolationLevel)
{
	assert(m_handles.pDb);

	const char* szQuery = "SET TEMPORARY OPTION isolation_level = 0";

	switch(eIsolationLevel)
	{
	case SA_ANSILevel1:
		szQuery = "SET TEMPORARY OPTION isolation_level = 1";
		break;
	case SA_ANSILevel2:
		szQuery = "SET TEMPORARY OPTION isolation_level = 2";
		break;
	case SA_ANSILevel3:
		szQuery = "SET TEMPORARY OPTION isolation_level = 3";
		break;

	default:
		break;
	}

	if( ! g_asaAPI.sqlany_execute_immediate(m_handles.pDb, szQuery) )
		Check();
}

/*virtual*/
void IasaConnection::setAutoCommit(
	SAAutoCommit_t eAutoCommit)
{
	assert(m_handles.pDb);

	if( ! g_asaAPI.sqlany_execute_immediate(m_handles.pDb, eAutoCommit == SA_AutoCommitOn ?
		"SET TEMPORARY OPTION auto_commit On":"SET TEMPORARY OPTION auto_commit Off") )
		Check();
}

/*virtual*/
void IasaConnection::Commit()
{
	assert(m_handles.pDb);

	if( ! g_asaAPI.sqlany_commit(m_handles.pDb) )
		Check();
}

/*virtual*/
void IasaConnection::Rollback()
{
	assert(m_handles.pDb);

	if( ! g_asaAPI.sqlany_rollback(m_handles.pDb) )
		Check();
}

/*virtual*/
saAPI *IasaConnection::NativeAPI() const
{
	return 	&g_asaAPI;
}

/*virtual*/
saConnectionHandles *IasaConnection::NativeHandles()
{
	return &m_handles;
}

/*virtual*/
ISACursor *IasaConnection::NewCursor(SACommand *pCommand)
{
	return new IasaCursor(this, pCommand);
}

/*virtual*/
void IasaConnection::CnvtInternalToNumeric(
	SANumeric & /*numeric*/,
	const void * /*pInternal*/,
	int /*nInternalSize*/)
{
	// TODO
}

/*virtual*/
void IasaConnection::CnvtInternalToDateTime(
	SADateTime & /*date_time*/,
	const void * /*pInternal*/,
	int /*nInternalSize*/)
{
	// TODO
}

/*virtual*/
void IasaConnection::CnvtInternalToInterval(
	SAInterval & /*interval*/,
	const void * /*pInternal*/,
	int /*nInternalSize*/)
{
	// TODO
}

/*virtual*/
void IasaConnection::CnvtInternalToCursor(
	SACommand * /*pCursor*/,
	const void * /*pInternal*/)
{
	// TODO
}

a_sqlany_data_type IasaConnection::CnvtStdToNative(SADataType_t eDataType)
{
	a_sqlany_data_type dbtype = A_INVALID_TYPE;

	switch(eDataType)
	{
	case SA_dtUnknown:
		throw SAException(SA_Library_Error, SA_Library_Error_UnknownDataType, -1, IDS_UNKNOWN_DATA_TYPE);
	case SA_dtBool:
		dbtype = A_UVAL8;
		break;
	case SA_dtShort:
		dbtype = A_VAL16;
		break;
	case SA_dtUShort:
		dbtype = A_UVAL16;
		break;
	case SA_dtLong:
		dbtype = A_VAL32;
		break;
	case SA_dtULong:
		dbtype = A_UVAL32;
		break;
	case SA_dtDouble:
		dbtype = A_DOUBLE;
		break;
	case SA_dtNumeric:
		dbtype = A_STRING;
		break;
	case SA_dtDateTime:
		dbtype = A_STRING;
		break;
	case SA_dtString:
		dbtype = A_STRING;
		break;
	case SA_dtBytes:
		dbtype = A_BINARY;
		break;
	case SA_dtLongBinary:
	case SA_dtBLob:
		dbtype = A_BINARY;
		break;
	case SA_dtLongChar:
	case SA_dtCLob:
		dbtype = A_STRING;
		break;
	default:
		assert(false);
		break;
	}

	return dbtype;
}

SADataType_t IasaConnection::CnvtNativeToStd(
	a_sqlany_native_type native_type, a_sqlany_data_type type,
	size_t/* max_size*/, unsigned short/* precision*/, unsigned short/* scale*/)
{
	switch(type)
	{
	case A_BINARY: // Binary data. Binary data is treated as-is and no character set conversion is performed.
		return native_type == DT_LONGBINARY ? SA_dtLongBinary:SA_dtBytes;

	case A_STRING: // String data. The data where character set conversion is performed.
		{
			switch(native_type)
			{
			case DT_DECIMAL:
				return SA_dtNumeric;
			case DT_LONGVARCHAR:
				return SA_dtLongChar;
			case DT_DATE:
			case DT_TIME:
			case DT_TIMESTAMP:
				return SA_dtDateTime;
			default:
				break;
			}
		}
		return SA_dtString;

	case A_DOUBLE: // Double data. Includes float values.
		return SA_dtDouble;

	case A_VAL64: // 64-bit integer.
	case A_UVAL64: // 64-bit unsigned integer.
		return SA_dtNumeric;

	case A_VAL32: // 32-bit integer.
		return SA_dtLong;

	case A_UVAL32: // 32-bit unsigned integer.
		return SA_dtULong;

	case A_VAL16: // 16-bit integer.
		return SA_dtShort;

	case A_UVAL16: // 16-bit unsigned integer.
		return SA_dtUShort;

	case A_VAL8: // 8-bit integer.
		return DT_BIT == native_type ? SA_dtBool:SA_dtShort;

	case A_UVAL8: // 8-bit unsigned integer.
		return SA_dtUShort;

	default:
		assert(false);
		return SA_dtString;
	}
}

//////////////////////////////////////////////////////////////////////
// IasaCursor Class
//////////////////////////////////////////////////////////////////////

IasaCursor::IasaCursor(
	IasaConnection *pIasaConnection,
	SACommand *pCommand) :
ISACursor(pIasaConnection, pCommand)
{
	Reset();
}

/*virtual */
IasaCursor::~IasaCursor()
{
}

/*virtual */
size_t IasaCursor::InputBufferSize(
	const SAParam & Param) const
{
	assert(NULL != m_handles.pStmt);

	switch(Param.DataType())
	{
	case SA_dtBool:
		return sizeof(unsigned char);
	case SA_dtNumeric:
		return 128l; // max NUMERUC precision.
	case SA_dtDateTime:
		return 23l; // default format is YYYY-MM-DD HH:NN:SS.SSS
	case SA_dtString:
#ifdef SA_UNICODE
		return Param.asString().GetUTF8CharsLength();
#else
		return Param.asString().GetMultiByteCharsLength();
#endif
	case SA_dtLongBinary:
	case SA_dtLongChar:
	case SA_dtBLob:
	case SA_dtCLob:
		return 0l;
	default:
		break;
	}

	return ISACursor::InputBufferSize(Param);
}

/*virtual */
size_t IasaCursor::OutputBufferSize(
	SADataType_t eDataType,
	size_t nDataSize) const
{
	assert(NULL != m_handles.pStmt);

	switch(eDataType)
	{
	case SA_dtBool:
		return sizeof(unsigned char);
	case SA_dtNumeric:
		return 128l; // max NUMERUC precision.
	case SA_dtDateTime:
		return 23l; // default format is YYYY-MM-DD HH:NN:SS.SSS
	case SA_dtString:
#ifdef SA_UNICODE
		return (nDataSize*6 + 1);
#else
		return (nDataSize*MB_CUR_MAX + 1);
#endif
	case SA_dtLongBinary:
	case SA_dtLongChar:
	case SA_dtBLob:
	case SA_dtCLob:
		return 0l;
	default:
		break;
	}

	return ISACursor::OutputBufferSize(eDataType, nDataSize);
}

/*virtual */
bool IasaCursor::ConvertIndicator(
	int nPos,	// 1-based, can be field or param pos, 0-for return value from SP
	int nNotConverted,
	SAValueRead & vr,
	ValueType_t eValueType,
	void * pInd, size_t nIndSize,
	void * pSize, size_t nSizeSize,
	size_t & nRealSize,
	int nBulkReadingBufPos) const
{
	if(eValueType == ISA_ParamValue)
	{
		if(((SAParam&)vr).isDefault())
			return false;	// nothing to convert, we haven't bound
	}

	if(!isLongOrLob(vr.DataType()))
		return ISACursor::ConvertIndicator(
		nPos,
		nNotConverted,
		vr, eValueType,
		pInd, nIndSize,
		pSize, nSizeSize,
		nRealSize,
		nBulkReadingBufPos);

	return true; // converted
}

#ifdef SA_UNICODE
/*virtual */
void IasaCursor::ConvertString(SAString &String, const void *pData, size_t nRealSize)
{
	String.SetUTF8Chars((const char*)pData, nRealSize);
}
#endif

/*virtual */
bool IasaCursor::IsOpened()
{
	return true;
}

/*virtual */
void IasaCursor::Open()
{
	// Do nothing
}

/*virtual */
void IasaCursor::Close()
{
	if( NULL != m_handles.pStmt )
	{
		if( !g_asaAPI.sqlany_reset(m_handles.pStmt))
			Check();
		g_asaAPI.sqlany_free_stmt(m_handles.pStmt);
		m_handles.pStmt = NULL;
	}	
}

/*virtual */
void IasaCursor::Destroy()
{
	if( NULL != m_handles.pStmt )
	{
		g_asaAPI.sqlany_reset(m_handles.pStmt);
		g_asaAPI.sqlany_free_stmt(m_handles.pStmt);
		m_handles.pStmt = NULL;
	}
}

/*virtual */
void IasaCursor::Reset()
{
	m_handles.pStmt = NULL;
}

/*virtual */
void IasaCursor::Prepare(
	const SAString &sStmt,
	SACommandType_t eCmdType,
	int nPlaceHolderCount,
	saPlaceHolder ** ppPlaceHolders)
{
	assert(NULL == m_handles.pStmt);
	assert(NULL != m_pISAConnection);

	SAString sStmtAny;
	size_t nPos = 0l;
	int i;
	switch(eCmdType)
	{
	case SA_CmdSQLStmt:
		// replace bind variables with '?' place holder
		for(i = 0; i < nPlaceHolderCount; i++)
		{
			sStmtAny += sStmt.Mid(nPos, ppPlaceHolders[i]->getStart()-nPos);
			sStmtAny += _TSA("?");
			nPos = ppPlaceHolders[i]->getEnd() + 1;
		}
		// copy tail
		if( nPos < sStmt.GetLength() )
			sStmtAny += sStmt.Mid(nPos);
		break;
	case SA_CmdStoredProc:
		sStmtAny = CallSubProgramSQL();
		break;
	case SA_CmdSQLStmtRaw:
		sStmtAny = sStmt;
		break;
	default:
		assert(false);
		break;
	}

	SA_TRACE_CMD(sStmtAny);

	m_handles.pStmt = g_asaAPI.sqlany_prepare(
		((IasaConnection*) m_pISAConnection)->m_handles.pDb,
#ifdef SA_UNICODE
		sStmtAny.GetUTF8Chars());
#else
		sStmtAny.GetMultiByteChars());
#endif
	if( NULL == m_handles.pStmt )
		Check();
}

SAString IasaCursor::CallSubProgramSQL()
{
	int nParams = m_pCommand->ParamCount();

	SAString sSQL;

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

	return sSQL;
}

void IasaCursor::Bind(
	int nPlaceHolderCount,
	saPlaceHolder **ppPlaceHolders)
{
	// we should bind for every place holder ('?')
	AllocBindBuffer(nPlaceHolderCount, ppPlaceHolders,
		sizeof(sacapi_bool), sizeof(size_t));

	void *pBuf = m_pParamBuffer;
	a_sqlany_bind_param param;

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
		if( eParamType == SA_dtUnknown )
			eParamType = eDataType;	// assume

		memset(&param, 0, sizeof(a_sqlany_bind_param));
		param.value.type = IasaConnection::CnvtStdToNative(
			eParamType == SA_dtUnknown? SA_dtString : eParamType);
		param.value.buffer = (char*)pValue;
		param.value.buffer_size = nDataBufSize;
		param.value.length = (size_t*)pSize;
		param.value.is_null = (sacapi_bool*)pInd;

		*(sacapi_bool*)pInd = Param.isNull();
		*(size_t*)pSize = nDataBufSize;

		switch( Param.ParamDirType() )
		{
		case SA_ParamInput:
			param.direction = DD_INPUT;
			break;
		case SA_ParamInputOutput:
			param.direction = DD_INPUT_OUTPUT;
			break;
		case SA_ParamOutput:
		case SA_ParamReturn:
			param.direction = DD_OUTPUT;
			break;
		default:
			param.direction = DD_INVALID;
			assert(false);
		}

		if(!Param.isNull() && isInputParam(Param))
		{
			switch(eDataType)
			{
			case SA_dtUnknown:
				throw SAException(SA_Library_Error, SA_Library_Error_UnknownParameterType, -1, IDS_UNKNOWN_PARAMETER_TYPE, (const SAChar*)Param.Name());
			case SA_dtBool:
				assert(nDataBufSize == sizeof(unsigned char));
				*(unsigned char*)pValue = (unsigned char)Param.asBool();
				break;
			case SA_dtShort:
				assert(nDataBufSize == sizeof(short));
				*(short*)pValue = Param.asShort();
				break;
			case SA_dtUShort:
				assert(nDataBufSize == sizeof(unsigned short));
				*(unsigned short*)pValue  = Param.asUShort();
				break;
			case SA_dtLong:
				assert(nDataBufSize == sizeof(long));
				*(long*)pValue = Param.asLong();
				break;
			case SA_dtULong:
				assert(nDataBufSize == sizeof(unsigned long));
				*(unsigned long*)pValue  = Param.asLong();
				break;
			case SA_dtDouble:
				assert(nDataBufSize == sizeof(double));
				*(double*)pValue = Param.asDouble();
				break;
			case SA_dtNumeric:
				{
					assert(nDataBufSize == 128l);
					SAString sVal = Param.asString();
					*param.value.length = sVal.GetMultiByteCharsLength();
					memcpy(pValue,  sVal.GetMultiByteChars(), *param.value.length);
				}
				break;
			case SA_dtDateTime:
				{
					assert(nDataBufSize == 23l);
					SADateTime dtVal = Param.asDateTime();
					SAString sVal;
					sVal.Format(_TSA("%04u-%02u-%02uT%02u:%02u:%02u.%03u"),
						dtVal.GetYear(),
						dtVal.GetMonth(),
						dtVal.GetDay(),
						dtVal.GetHour(),
						dtVal.GetMinute(),
						dtVal.GetSecond(),
						dtVal.Fraction() / 1000000);
					*param.value.length = sVal.GetMultiByteCharsLength();
					memcpy(pValue,  sVal.GetMultiByteChars(), *param.value.length);
				}
				break;
			case SA_dtString:
				{
					SAString sVal = Param.asString();
#ifdef SA_UNICODE
					*param.value.length = param.value.buffer_size;
					memcpy(pValue,  sVal.GetUTF8Chars(), *param.value.length);
#else
					*param.value.length = sVal.GetMultiByteCharsLength();
					memcpy(pValue,  sVal.GetMultiByteChars(), *param.value.length);
#endif
				}
				break;
			case SA_dtBytes:
				*param.value.length = param.value.buffer_size;
				memcpy(pValue, (const void*)Param.asBytes(), *param.value.length);
				break;
			case SA_dtLongChar:
			case SA_dtCLob:
			case SA_dtLongBinary:
			case SA_dtBLob:
				{
					size_t nActualWrite;
					SAPieceType_t ePieceType = SA_FirstPiece;
					void *pBuf;

					SADummyConverter DummyConverter;
					ISADataConverter *pIConverter = &DummyConverter;
					size_t nCnvtSize, nCnvtPieceSize = 0l;
					SAPieceType_t eCnvtPieceType;
#if defined(SA_UNICODE)
					bool bAddSpaceForNull = Param.ParamType() == SA_dtCLob || Param.DataType() == SA_dtCLob ||
						Param.ParamType() == SA_dtLongChar || Param.DataType() == SA_dtLongChar;
					SAUnicode2MultibyteConverter Unicode2Utf8Converter;
					Unicode2Utf8Converter.SetUseUTF8();
					if( bAddSpaceForNull )
					{
						pIConverter = &Unicode2Utf8Converter;
					}
#endif
					do
					{
						nActualWrite = Param.InvokeWriter(ePieceType, IasaConnection::MaxLongPiece, pBuf);
						pIConverter->PutStream((unsigned char*)pBuf, nActualWrite, ePieceType);
						// smart while: initialize nCnvtPieceSize before calling pIConverter->GetStream
						while(nCnvtPieceSize = nActualWrite,
							pIConverter->GetStream((unsigned char*)pBuf, nCnvtPieceSize, nCnvtSize, eCnvtPieceType))
						{
							if (! g_asaAPI.sqlany_send_param_data(m_handles.pStmt, i, (char*)pBuf, nCnvtSize) )
								Check();
						}

						if(ePieceType == SA_LastPiece)
							break;
					}
					while( nActualWrite != 0 );
				}
				break;
			default:
				assert(false);
				break;
			}
		}

		if( Param.isNull() || ! isLongOrLob(eDataType) )
		{
			if (! g_asaAPI.sqlany_bind_param(m_handles.pStmt, i, &param) )
				Check();
		}
	}
}

/*virtual */
void IasaCursor::Execute(
	int nPlaceHolderCount,
	saPlaceHolder ** ppPlaceHolders)
{
	assert(NULL != m_handles.pStmt);

	if( nPlaceHolderCount > 0 )
		Bind(nPlaceHolderCount, ppPlaceHolders);

	if( !g_asaAPI.sqlany_execute(m_handles.pStmt) )
		Check();

	if( ! ResultSetExists() )
		ConvertOutputParams();
}

/*virtual */
void IasaCursor::UnExecute()
{
	if( NULL != m_handles.pStmt ) 
		Close();
}

/*virtual */
void IasaCursor::Cancel()
{
	assert(NULL != m_pISAConnection);
	((IasaConnection*)(m_pISAConnection))->Cancel();
}

/*virtual */
bool IasaCursor::ResultSetExists()
{
	return (NULL != m_handles.pStmt && g_asaAPI.sqlany_num_cols(m_handles.pStmt) > 0);
}

/*virtual */
void IasaCursor::DescribeFields(
	DescribeFields_cb_t fn)
{
	assert(NULL != m_handles.pStmt);

	sacapi_i32 iCol = g_asaAPI.sqlany_num_cols(m_handles.pStmt);
	a_sqlany_column_info colInfo;

	for( sacapi_i32 i = 0; i < iCol; ++i )
	{
		if( !g_asaAPI.sqlany_get_column_info(m_handles.pStmt, i, &colInfo) )
			Check();

		SAString name;
#ifdef SA_UNICODE
		name.SetUTF8Chars(colInfo.name, SIZE_MAX);
#else
		name = (const char*)colInfo.name;
#endif
		SADataType_t eDataType = ((IasaConnection*)m_pISAConnection)->CnvtNativeToStd(
			colInfo.native_type, colInfo.type, colInfo.max_size, colInfo.precision, colInfo.scale);
		(m_pCommand->*fn)(name, eDataType, colInfo.native_type,
			colInfo.max_size, colInfo.precision, colInfo.scale, false, (int)iCol);
	}
}

/*virtual */
void IasaCursor::SetSelectBuffers()
{
	assert(NULL != m_handles.pStmt);
	// Do nothing - ASA uses own field data buffer and we can access it directly.
}

/*virtual */
void IasaCursor::SetFieldBuffer(
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

void IasaCursor::ParseDateTime(SADateTime& dt, const SAString& s, a_sqlany_native_type type)
{
	// default format is YYYY-MM-DD HH:NN:SS.SSS

	int year = 1900, month = 1, day = 1, hours = 0, minutes = 0, seconds = 0, milliseconds = 0;
	SAString sDate, sTime;

	switch(type)
	{
	case DT_TIMESTAMP:
		sDate = s.Left(10);
		sTime = s.Mid(11,12);
		break;

	case DT_DATE:
		sDate = s.Left(10);
		break;

	case DT_TIME:
		sTime = s.Left(12);
		break;

	default:
		break;
	}

	if( ! sDate.IsEmpty() )
	{
		year = sa_toi(sDate.Left(4));
		month = sa_toi(sDate.Mid(5,2));
		day = sa_toi(sDate.Mid(8,2));
	}

	if( ! sTime.IsEmpty() )
	{
		hours = sa_toi(sTime.Left(2));
		minutes = sa_toi(sTime.Mid(3,2));
		seconds = sa_toi(sTime.Mid(6,2));
		milliseconds = sa_toi(sTime.Mid(9,3));
	}
	dt = SADateTime(year, month, day, hours, minutes, seconds);
	dt.Fraction() = milliseconds * 1000000;
}

/*virtual */
bool IasaCursor::FetchNext()
{
	if( NULL == m_handles.pStmt )
		return false;

	if( g_asaAPI.sqlany_fetch_next(m_handles.pStmt) )
	{
		a_sqlany_data_value val;

		for( sacapi_u32 i = 0; i < (sacapi_u32)m_pCommand->FieldCount(); ++i )
		{
			SAField &f = m_pCommand->Field(i+1);

			if( isLongOrLob(f.FieldType()) )
			{
				a_sqlany_data_info di;
				if(!g_asaAPI.sqlany_get_data_info(m_handles.pStmt, i, &di))
					Check();
				*f.m_pbNull = di.is_null ? true:false;
				if( ! *f.m_pbNull )
				{
					*(sacapi_u32*)f.m_pScalar = i;
					ConvertLongOrLOB(ISA_FieldValue, f, NULL, 0);			
				}
				continue;
			}
			else
			{
				if( ! g_asaAPI.sqlany_get_column(m_handles.pStmt, i, &val) )
					Check();
				*f.m_pbNull = *val.is_null ? true:false;
			}

			if( *f.m_pbNull )
				continue;

			switch (f.FieldType())
			{
			case SA_dtString:
#ifdef SA_UNICODE
				f.m_pString->SetUTF8Chars(val.buffer, *val.length);
#else
				*f.m_pString = SAString((const char*)val.buffer, *val.length);
#endif
				break;

			case SA_dtNumeric:
				{
					switch(val.type)
					{
					case A_UVAL64:
						assert(*val.length == 8);
						*f.m_pNumeric = *((sa_uint64_t*)val.buffer);
						break;
					case A_VAL64:
						assert(*val.length == 8);
						*f.m_pNumeric = *((sa_int64_t*)val.buffer);
						break;
					case A_STRING:
						*f.m_pNumeric = SAString((const char*)val.buffer, *val.length);
						break;

					default:
						break;
					}
				}
				break;

			case SA_dtBool:
				assert(*val.length == 1);
				*(bool*)f.m_pScalar = *(bool*)val.buffer;
				break;

			case SA_dtDouble:				
				assert(*val.length == 8);
				*(double*)f.m_pScalar = *(double*)val.buffer;
				break;

			case SA_dtShort:
				*(short*)f.m_pScalar = A_VAL8 == val.type ? *(char*)val.buffer:*(short*)val.buffer;
				break;

			case SA_dtUShort:
				*(unsigned short*)f.m_pScalar = A_UVAL8 == val.type ? *(unsigned char*)val.buffer:*(unsigned short*)val.buffer;
				break;

			case SA_dtLong:
				assert(*val.length == 4);
				*(long*)f.m_pScalar = *(int*)val.buffer;
				break;

			case SA_dtULong:
				assert(*val.length == 4);
				*(unsigned long*)f.m_pScalar = *(unsigned int*)val.buffer;
				break;

			case SA_dtDateTime:
				ParseDateTime(*f.m_pDateTime, SAString((const char*)val.buffer, *val.length),
					(a_sqlany_native_type)f.FieldNativeType());
				break;

			case SA_dtBytes:
				*f.m_pString = SAString((const void*)val.buffer, *val.length);
				break;

			default:
				break;
			}
		}
		return true;
	}

	Check();

	if( ! isSetScrollable() )
	{
		if( ! g_asaAPI.sqlany_get_next_result(m_handles.pStmt) )
			// There is not way to reset the statement but oly to close it
				m_pCommand->Close();
	}

	return false;
}

/*virtual */
long IasaCursor::GetRowsAffected()
{
	return (NULL == m_handles.pStmt ? 0 : g_asaAPI.sqlany_affected_rows(m_handles.pStmt));
}

/*virtual */
void IasaCursor::ReadLongOrLOB(
	ValueType_t/* eValueType*/,
	SAValueRead & vr,
	void * /*pValue*/,
	size_t /*nFieldBufSize*/,
	saLongOrLobReader_t fnReader,
	size_t nReaderWantedPieceSize,
	void * pAddlData)
{
	sacapi_u32 iPos = *(sacapi_u32*)vr.m_pScalar;
	a_sqlany_data_info di;
	if(!g_asaAPI.sqlany_get_data_info(m_handles.pStmt, iPos, &di))
		Check();
	*vr.m_pbNull = di.is_null ? true:false;
	if( *vr.m_pbNull )
		return;		
	size_t nLongSize = di.data_size;
	bool bAddSpaceForNull = A_STRING == di.type;

	SADummyConverter DummyConverter;
	SAMultibyte2UnicodeConverter Multibyte2UnicodeConverter;
	ISADataConverter *pIConverter = &DummyConverter;
	size_t nCnvtLongSizeMax = nLongSize;
#if defined(SA_UNICODE)
	SAMultibyte2UnicodeConverter utf8UnicodeConverter;
	utf8UnicodeConverter.SetUseUTF8();
	if( bAddSpaceForNull )
	{
		pIConverter = &utf8UnicodeConverter;
		nCnvtLongSizeMax = nLongSize * sizeof(SAChar);
	}
#endif

	unsigned char* pBuf;
	size_t nPortionSize = vr.PrepareReader(
		sa_max(nCnvtLongSizeMax, nLongSize),	// real size is nCnvtLongSizeMax
		IasaConnection::MaxLongPiece,
		pBuf,
		fnReader,
		nReaderWantedPieceSize,
		pAddlData,
		bAddSpaceForNull);
	assert(nPortionSize <= IasaConnection::MaxLongPiece);

	size_t nCnvtPieceSize = nPortionSize;
	size_t nTotalPassedToReader = 0l;

	SAPieceType_t ePieceType = SA_FirstPiece;
	size_t nTotalRead = 0l;
	SAPieceType_t eCnvtPieceType;

	do
	{
		if(nLongSize)	// known
			nPortionSize = sa_min(nPortionSize, nLongSize - nTotalRead);
		else
		{
			vr.InvokeReader(SA_LastPiece, pBuf, 0);
			break;
		}

		sacapi_i32 NumBytes = g_asaAPI.sqlany_get_data(m_handles.pStmt, iPos, nTotalRead, pBuf, nPortionSize);
		if(NumBytes < 0)
			Check();
		size_t nActualRead = (size_t)NumBytes;

		nTotalRead += nActualRead;

		if(nTotalRead == nLongSize)
		{
			if(ePieceType == SA_NextPiece)
				ePieceType = SA_LastPiece;
			else    // the whole BLob was read in one piece
			{
				assert(ePieceType == SA_FirstPiece);
				ePieceType = SA_OnePiece;
			}
		}

		pIConverter->PutStream(pBuf, nActualRead, ePieceType);
		size_t nCnvtSize;
		// smart while: initialize nCnvtPieceSize before calling pIConverter->GetStream
		while(nCnvtPieceSize = (nCnvtLongSizeMax ? // known
			sa_min(nCnvtPieceSize, nCnvtLongSizeMax - nTotalPassedToReader) : nCnvtPieceSize),
			pIConverter->GetStream(pBuf, nCnvtPieceSize, nCnvtSize, eCnvtPieceType))
		{
			vr.InvokeReader(eCnvtPieceType, pBuf, nCnvtSize);
			nTotalPassedToReader += nCnvtSize;
		}

		if(ePieceType == SA_FirstPiece)
			ePieceType = SA_NextPiece;
	}
	while(nTotalRead < nLongSize);
	assert(nTotalRead == nLongSize);
}

SADataType_t IasaCursor::CnvtDomainIdToStd(
	int dbtype, 
	int/* dbsubtype*/,
	int/* dbsize*/,
	int prec, 
	int scale) const
{
	/*
	domain_id,domain_name,type_id,precision
	1,smallint,5,5
	2,integer,4,10
	3,numeric,2,(NULL)
	4,float,7,7
	5,double,8,15
	6,date,9,(NULL)
	7,char,1,(NULL)
	8,char,1,(NULL)
	9,varchar,12,(NULL)
	10,long varchar,-1,(NULL)
	11,binary,-2,(NULL)
	12,long binary,-4,(NULL)
	13,timestamp,11,(NULL)
	14,time,10,(NULL)
	16,st_geometry,-19,(NULL)
	19,tinyint,-6,3
	20,bigint,-5,20
	21,unsigned int,-9,10
	22,unsigned smallint,-10,5
	23,unsigned bigint,-11,21
	24,bit,-7,1
	26,timestamp with time zone,0,(NULL)
	27,decimal,2,(NULL)
	28,varbinary,-2,(NULL)
	29,uniqueidentifier,-15,(NULL)
	30,varbit,-16,(NULL)
	31,long varbit,-17,(NULL)
	32,xml,-18,(NULL)
	33,nchar,-12,(NULL)
	34,nchar,-12,(NULL)
	35,nvarchar,-13,(NULL)
	36,long nvarchar,-14,(NULL)
	*/
	SADataType_t eDataType = SA_dtUnknown;

	switch(dbtype)
	{
	case 11:	//binary
	case 28:	//varbinary
	case 29:	//uniqueidentifier
		eDataType = SA_dtBytes;
		break;
	case 7:		//char
	case 8:		//char
	case 9:		//varchar
	case 33:	//nchar
	case 34:	//nchar
	case 35:	//nvarchar
		eDataType = SA_dtString;
		break;
	case 1:		//smallint
	case 19:	//tinyint
		eDataType = SA_dtShort;
		break;
	case 22:	//unsigned smallint
		eDataType = SA_dtUShort;
		break;
	case 24:	//bit
		eDataType = SA_dtBool;
		break;
	case 2:		//integer
		eDataType = SA_dtLong;
		break;
	case 21:	//unsigned int
		eDataType = SA_dtULong;
		break;
	case 4:		//float
	case 5:		//double
		eDataType = SA_dtDouble;
		break;
	case 20:	//bigint
	case 23:	//unsigned bigint
		eDataType = SA_dtNumeric;
		break;
	case 3:		//numeric
	case 27:	//decimal
		if(scale <= 0)
		{	// check for exact type
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
	case 6:		//date
	case 13:	//timestamp
	case 14:	//time
	case 26:	//timestamp with time zone
		eDataType = SA_dtDateTime;
		break;
	case 12:	// long binary
		eDataType = SA_dtLongBinary;
		break;
	case 10:	//long varchar
	case 36:	//long nvarchar
	case 32:	//xml
		eDataType = SA_dtLongChar;
		break;
	default:
		assert(false);
	}

	return eDataType;
}

/*virtual */
void IasaCursor::DescribeParamSP()
{
	SACommand cmd(m_pISAConnection->getSAConnection());

	// add SA_Param_Return parameter manually - it's always the first one
	m_pCommand->CreateParam(
		_TSA("RETURN_VALUE"),
		SA_dtLong,
		IasaConnection::CnvtStdToNative(SA_dtLong),
		sizeof(long), 0, 0,	// precision and scale as reported by native API
		SA_ParamReturn);

	SAString sProcName = m_pCommand->CommandText();
	SAString sSQL;
	sSQL.Format(_TSA(
		"select \
		spp.parm_name as name, spp.domain_id as type, spp.width as length, spp.width as prec, spp.scale,\
		spp.parm_mode_in || spp.parm_mode_out as parm_mode \
		from sysobjects so, sysprocedure sp, sysprocparm spp where \
		so.id = object_id('%") SA_FMT_STR _TSA("') and so.type = 'P' and \
		so.name = sp.proc_name and so.uid = sp.creator and \
		spp.proc_id = sp.proc_id and \
		spp.parm_type = 0 \
		order by \
		spp.parm_id"),
		(const SAChar*)sProcName);
	cmd.setCommandText(sSQL);
	cmd.Execute();

	while(cmd.FetchNext())
	{
		SAString sName = cmd[_TSA("name")].asString();
		// if "name" database field is CHAR it could be right padded with spaces
		sName.TrimRight(_TSA(' '));
		if(sName.Left(1) == _TSA("@"))
			sName.Delete(0);

		int nParamSize = cmd[_TSA("length")].asShort();
		short nType = cmd[_TSA("type")].asShort();
		short nPrec = cmd[_TSA("prec")].asShort();
		short nScale = cmd[_TSA("scale")].asShort();
		SADataType_t eDataType = CnvtDomainIdToStd(nType, 0, nParamSize, nPrec, nScale);
		SAParamDirType_t eDirType = SA_ParamInput;
		SAString sParmMode = cmd[_TSA("parm_mode")].asString();
		if(sParmMode == _TSA("YN"))
			eDirType = SA_ParamInput;
		else if(sParmMode == _TSA("YY"))
			eDirType = SA_ParamInputOutput;
		else if(sParmMode == _TSA("NY"))
			eDirType = SA_ParamOutput;
		else
			assert(false);

#ifdef SA_PARAM_USE_DEFAULT
		SAParam& p =
#endif
			m_pCommand->CreateParam(sName,
			eDataType,
			IasaConnection::CnvtStdToNative(eDataType),
			nParamSize, nPrec, nScale,
			eDirType);
#ifdef SA_PARAM_USE_DEFAULT
		if( eDirType == SA_ParamInput )
			p.setAsDefault();
#endif
	}
}

/*virtual */
saCommandHandles *IasaCursor::NativeHandles()
{
	return &m_handles;
}
