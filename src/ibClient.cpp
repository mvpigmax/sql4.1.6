// ibClient.cpp: implementation of the IibClient class.
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <ibAPI.h>
#include "ibClient.h"

typedef char INTERBASE_SQL_NUMERIC_STRUCT[1024];

//////////////////////////////////////////////////////////////////////
// IibConnection Class
//////////////////////////////////////////////////////////////////////

class IibConnection : public ISAConnection
{
	friend class IibCursor;

	ibConnectionHandles m_handles;

	ISC_STATUS		m_StatusVector[20];		// Error status vector
	char			*m_DPB_buffer;			// Address of the DPB
	short			m_DPB_buffer_length;	// Number of bytes in the database parameter buffer (DPB)
	char			m_TPB_buffer[1024];		// Address of the TPB
	short			m_TPB_buffer_length;	// Number of bytes in the transaction parameter buffer (TPB)

	static void Check(
		const ISC_STATUS &,
		ISC_STATUS* pSV);
	static SAString FormatIdentifierValue(
		unsigned short nSQLDialect,
		const SAString &sValue);

	enum MaxLong { MaxBlobPiece = 0xFFFF };

	static void CnvtNumericToInternal(
		const SANumeric &numeric,
		INTERBASE_SQL_NUMERIC_STRUCT &Internal,
		short &sqllen);
	static void CnvtInternalToDateTime(
		SADateTime &date_time,
		const ISC_QUAD &Internal);
	static void CnvtDateTimeToInternal(
		const SADateTime &date_time,
		ISC_QUAD &Internal);

	static void ExtractServerVersionCallback(long *pnVersion, char *sStr);
	static void ExtractServerVersionStringCallback(SAString *psVersion, char *sStr);

	// transaction management helpers
	void StartTransaction(
		SAIsolationLevel_t eIsolationLevel,
		SAAutoCommit_t eAutoCommit);
	void StartTransactionIndirectly();
	void ConstructTPB(
		SAIsolationLevel_t eIsolationLevel,
		SAAutoCommit_t eAutoCommit);
	void CommitTransaction();
	void CommitRetaining();
	void RollbackTransaction();

protected:
	virtual ~IibConnection();

public:
	IibConnection(SAConnection *pSAConnection);

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

IibConnection::IibConnection(
	SAConnection *pSAConnection) : ISAConnection(pSAConnection)
{
	m_DPB_buffer = NULL;
	Reset();
}

IibConnection::~IibConnection()
{
	if (m_DPB_buffer)
		::free(m_DPB_buffer);
}

/*virtual */
void IibConnection::InitializeClient()
{
	::AddIBSupport(m_pSAConnection);
}

/*virtual */
void IibConnection::UnInitializeClient()
{
	if (SAGlobals::UnloadAPI()) ::ReleaseIBSupport();
}

/*virtual */
void IibConnection::CnvtInternalToDateTime(
	SADateTime &date_time,
	const void *pInternal,
	int nInternalSize)
{
	assert((size_t)nInternalSize == sizeof(ISC_QUAD));
	if ((size_t)nInternalSize != sizeof(ISC_QUAD))
		return;
	CnvtInternalToDateTime(date_time, *(const ISC_QUAD*)pInternal);
}

// Note: ISC_TIME_SECONDS_PRECISION is defined in ibase.h
#define SQLAPI_FRACTION_PRECISION  1000000000U
#define SQLAPI_FRACTION (SQLAPI_FRACTION_PRECISION / ISC_TIME_SECONDS_PRECISION ) 

/*static */
void IibConnection::CnvtInternalToDateTime(
	SADateTime &date_time,
	const ISC_QUAD &Internal)
{
	struct tm &_tm = (struct tm&)date_time;

	g_ibAPI.isc_decode_date((ISC_QUAD*)&Internal, (void*)&_tm);

	date_time.Fraction() =
		(((ISC_TIMESTAMP*)&Internal)->timestamp_time % ISC_TIME_SECONDS_PRECISION)
		* SQLAPI_FRACTION;
}

/*static */
void IibConnection::CnvtDateTimeToInternal(
	const SADateTime &hire_time,
	ISC_QUAD &time)
{
	g_ibAPI.isc_encode_date((void*)&hire_time, &time);

	((ISC_TIMESTAMP*)&time)->timestamp_time += hire_time.Fraction() / SQLAPI_FRACTION;
}

/*virtual */
void IibConnection::CnvtInternalToInterval(
	SAInterval & /*interval*/,
	const void * /*pInternal*/,
	int /*nInternalSize*/)
{
	assert(false);
}

/*virtual */
void IibConnection::CnvtInternalToCursor(
	SACommand * /*pCursor*/,
	const void * /*pInternal*/)
{
	assert(false);
}

/*virtual */
long IibConnection::GetClientVersion() const
{
	return g_nIB_DLLVersionLoaded;
}

/*static */
void IibConnection::ExtractServerVersionCallback(
	long *pnVersion, char *sStr)
{
	char *p = strstr(sStr, "(access method)");
	if (p)
		*pnVersion = SAExtractVersionFromString(p);
}

/*static */
void IibConnection::ExtractServerVersionStringCallback(
	SAString *psVersion, char *sStr)
{
	char *p = strstr(sStr, "(access method)");
	if (p)
		*psVersion = sStr;
}

/*virtual */
long IibConnection::GetServerVersion() const
{
	long nVersion = 0l;

	g_ibAPI.isc_version(
		(isc_db_handle*)&m_handles.m_db_handle,
		(isc_callback)ExtractServerVersionCallback,
		&nVersion);

	return nVersion;
}

/*virtual */
SAString IibConnection::GetServerVersionString() const
{
	SAString sVersion;

	g_ibAPI.isc_version(
		(isc_db_handle*)&m_handles.m_db_handle,
		(isc_callback)ExtractServerVersionStringCallback,
		&sVersion);

	return sVersion;
}

/*static */
void IibConnection::Check(
	const ISC_STATUS &error_code,
	ISC_STATUS *pSV)
{
	if (error_code)
	{
		SAString sErr;
		char szMsg[1024];
		int nativeError = 0;
        ISC_LONG nLen;
		ISC_STATUS *pvector; /* Pointer to pointer to status vector. */

		SAException* pNested = NULL;

		// walk through error vector and constract error string
		pvector = pSV; /* (Re)set to start of status vector. */
		while (pvector[0] == 1 && pvector[1] > 0)
		{
            nativeError = (int)pvector[1];

            //ISC_LONG sqlCode = g_ibAPI.isc_sqlcode(pvector);

			// use safe string API if its available
            if ((nLen = (ISC_LONG)(NULL != g_ibAPI.fb_interpret ?
                g_ibAPI.fb_interpret(szMsg, (unsigned int)sizeof(szMsg), &pvector)
                :
                g_ibAPI.isc_interprete(szMsg, &pvector))) > 1)
            {
                if (!sErr.IsEmpty())
                    pNested = new SAException(pNested, SA_DBMS_API_Error, nativeError, -1, sErr);

                sErr = SAString(szMsg, size_t(nLen));
            }
		}

		if (sErr.IsEmpty())
			sErr = _TSA("unknown error");

		throw SAException(pNested, SA_DBMS_API_Error,
			int(nativeError > 0 ? nativeError : error_code),
			-1, sErr);
	}
}

/*virtual */
bool IibConnection::IsConnected() const
{
	return m_handles.m_db_handle != ISC_NULL_HANDLE;
}

/*virtual */
bool IibConnection::IsAlive() const
{
	long nVersion = 0l;

	return 0 == g_ibAPI.isc_version(
		(isc_db_handle*)&m_handles.m_db_handle,
		(isc_callback)ExtractServerVersionCallback,
		&nVersion);
}

/*virtual */
void IibConnection::Connect(
	const SAString &sDBString,
	const SAString &sUserID,
	const SAString &sPassword,
	saConnectionHandler_t fHandler)
{
	assert(m_handles.m_db_handle == ISC_NULL_HANDLE);
	assert(m_handles.m_tr_handle == ISC_NULL_HANDLE);
	assert(m_DPB_buffer == NULL);
	assert(m_DPB_buffer_length == 0);

	// Construct the database parameter buffer.
	sa_realloc((void**)&m_DPB_buffer, 1024);
	char *dpb = m_DPB_buffer;
	const char *p;
	*dpb++ = isc_dpb_version1;
	*dpb++ = isc_dpb_user_name;
	*dpb++ = (char)sUserID.GetMultiByteCharsLength();
	for (p = sUserID.GetMultiByteChars(); *p;)
		*dpb++ = *p++;
	*dpb++ = isc_dpb_password;
	*dpb++ = (char)sPassword.GetMultiByteCharsLength();
	for (p = sPassword.GetMultiByteChars(); *p;)
		*dpb++ = *p++;
	// additional entries to DPB that can be specified
	typedef
	struct
	{
		const char *sKey;
		char cValue;
	} DPB_Stuff_t;
	// string values
	DPB_Stuff_t DPB_Strings[] =
	{
		{ "isc_dpb_lc_ctype", isc_dpb_lc_ctype },
		{ "isc_dpb_sql_role_name", isc_dpb_sql_role_name }
	};
	unsigned int i;
	for (i = 0; i < sizeof(DPB_Strings) / sizeof(DPB_Stuff_t); ++i)
	{
		SAString sOption = m_pSAConnection->Option(DPB_Strings[i].sKey);
#ifdef SA_UNICODE_WITH_UTF8
		if( 0 == strcmp(DPB_Strings[i].sKey, "isc_dpb_lc_ctype") )
			sOption = _TSA("UTF-8");
#endif
		if (sOption.IsEmpty())
			continue;

		*dpb++ = DPB_Strings[i].cValue;
		*dpb++ = (char)sOption.GetMultiByteCharsLength();
		for (p = sOption.GetMultiByteChars(); *p;)
			*dpb++ = *p++;
	}
	// 1-byte values
	DPB_Stuff_t DPB_Byte_1[] =
	{
		{ "isc_dpb_num_buffers", isc_dpb_num_buffers }
	};
	for (i = 0; i < sizeof(DPB_Byte_1) / sizeof(DPB_Stuff_t); ++i)
	{
		SAString sOption = m_pSAConnection->Option(DPB_Byte_1[i].sKey);
		if (sOption.IsEmpty())
			continue;

		*dpb++ = DPB_Byte_1[i].cValue;
		*dpb++ = (char)1;
		*dpb++ = (char)atoi(sOption.GetMultiByteChars());
	}

	m_DPB_buffer_length = (short)(dpb - m_DPB_buffer);

	try
	{
		if (NULL != fHandler)
			fHandler(*m_pSAConnection, SA_PreConnectHandler);

		// attach
		Check(g_ibAPI.isc_attach_database(
			m_StatusVector, 0, (char*)sDBString.GetMultiByteChars(), &m_handles.m_db_handle,
			m_DPB_buffer_length, m_DPB_buffer), m_StatusVector);

		if (NULL != fHandler)
			fHandler(*m_pSAConnection, SA_PostConnectHandler);

		// start transaction with default isolation level and autocommit option
		// DISABLED: start transaction only when first query executed
		// StartTransaction(m_pSAConnection->IsolationLevel(), m_pSAConnection->AutoCommit());
	}
	catch (...)
	{
		// clean up
		if (ISC_NULL_HANDLE != m_handles.m_db_handle)
		{
			g_ibAPI.isc_detach_database(
				m_StatusVector, &m_handles.m_db_handle);
			assert(m_handles.m_db_handle == ISC_NULL_HANDLE);
			m_handles.m_db_handle = ISC_NULL_HANDLE;
		}
		if (m_DPB_buffer)
		{
			::free(m_DPB_buffer);
			m_DPB_buffer = NULL;
		}
		m_DPB_buffer_length = 0;

		throw;
	}
}

/*virtual */
void IibConnection::Disconnect()
{
	assert(m_handles.m_db_handle != ISC_NULL_HANDLE);
	// start transaction only when first query executed

	// first we should close transaction
	CommitTransaction();	// commit and close the transaction
	assert(m_handles.m_tr_handle == ISC_NULL_HANDLE);

	// then we can detach
	Check(g_ibAPI.isc_detach_database(
		m_StatusVector, &m_handles.m_db_handle), m_StatusVector);
	assert(m_handles.m_db_handle == ISC_NULL_HANDLE);


	assert(m_DPB_buffer);
	assert(m_DPB_buffer_length);
	if (m_DPB_buffer)
		::free(m_DPB_buffer);
	m_DPB_buffer = NULL;
	m_DPB_buffer_length = 0;
}

/*virtual */
void IibConnection::Destroy()
{
	assert(m_handles.m_db_handle != ISC_NULL_HANDLE);

	// first we should close transaction
	if (ISC_NULL_HANDLE != m_handles.m_tr_handle)
	{
		g_ibAPI.isc_commit_transaction(m_StatusVector, &m_handles.m_tr_handle);
		m_handles.m_tr_handle = ISC_NULL_HANDLE;
	}

	// then we can detach
	g_ibAPI.isc_detach_database(
		m_StatusVector, &m_handles.m_db_handle);
	m_handles.m_db_handle = ISC_NULL_HANDLE;

	assert(m_DPB_buffer);
	assert(m_DPB_buffer_length);
	if (m_DPB_buffer)
		::free(m_DPB_buffer);
	m_DPB_buffer = NULL;
	m_DPB_buffer_length = 0;
}

/*virtual */
void IibConnection::Reset()
{
	m_handles.m_tr_handle = ISC_NULL_HANDLE;
	m_handles.m_db_handle = ISC_NULL_HANDLE;

	if (m_DPB_buffer)
		::free(m_DPB_buffer);

	m_DPB_buffer = NULL;
	m_DPB_buffer_length = 0;

	m_TPB_buffer_length = 0;
}

/*virtual */
void IibConnection::Commit()
{
	// always keep transaction open
	// start transaction only when first query executed

	// use isc_commit_retaining() only if user explicitly asks for it
	SAString sCommitRetaining = m_pSAConnection->Option(_TSA("CommitRetaining"));
	if (0 == sCommitRetaining.CompareNoCase(_TSA("TRUE")) ||
		0 == sCommitRetaining.CompareNoCase(_TSA("1")))
		CommitRetaining();		// commit but leave the transaction open
	else
	{
		CommitTransaction();	// commit and close the transaction
		// start transaction only when first query executed
	}

	// always keep transaction open
	// start transaction only when first query executed
}

/*virtual */
void IibConnection::Rollback()
{
	// always keep transaction open
	// start transaction only when first query executed

	RollbackTransaction();	// rollback and close the transaction
	// start transaction only when first query executed
}

void IibConnection::ConstructTPB(
	SAIsolationLevel_t eIsolationLevel,
	SAAutoCommit_t eAutoCommit)
{
	// Construct the transaction parameter buffer
	char *tpb = m_TPB_buffer;
	*tpb++ = isc_tpb_version3;

	SAString sAccessMode = m_pSAConnection->Option(_TSA("TPB_AccessMode"));
	if (!sAccessMode.IsEmpty())
	{
		if (sAccessMode.CompareNoCase(_TSA("isc_tpb_write")) == 0)
			*tpb++ = isc_tpb_write;
		else if (sAccessMode.CompareNoCase(_TSA("isc_tpb_read")) == 0)
			*tpb++ = isc_tpb_read;
	}

	switch (eIsolationLevel)
	{
	case SA_LevelUnknown:
		// use default values
		break;
	case SA_ReadUncommitted:
		*tpb++ = isc_tpb_read_committed;
		*tpb++ = isc_tpb_rec_version;
		break;
	case SA_ReadCommitted:
		*tpb++ = isc_tpb_read_committed;
		*tpb++ = isc_tpb_no_rec_version;
		break;
	case SA_RepeatableRead:
		*tpb++ = isc_tpb_consistency;
		break;
	case SA_Serializable:
		*tpb++ = isc_tpb_concurrency;
		break;
	default:
		assert(false);
	}

	SAString sLockResolution = m_pSAConnection->Option(_TSA("TPB_LockResolution"));
	if (!sLockResolution.IsEmpty())
	{
		if (sLockResolution.CompareNoCase(_TSA("isc_tpb_wait")) == 0)
			*tpb++ = isc_tpb_wait;
		else if (sLockResolution.CompareNoCase(_TSA("isc_tpb_nowait")) == 0)
			*tpb++ = isc_tpb_nowait;
	}

	if (eAutoCommit == SA_AutoCommitOn)
		*tpb++ = isc_tpb_autocommit;

	m_TPB_buffer_length = (short)(tpb - m_TPB_buffer);
	if (m_TPB_buffer_length == 1)	// only isc_tpb_version3
		m_TPB_buffer_length = 0;	// use default TPB
}

void IibConnection::StartTransaction(
	SAIsolationLevel_t eIsolationLevel,
	SAAutoCommit_t eAutoCommit)
{
	assert(m_handles.m_tr_handle == ISC_NULL_HANDLE);

	ConstructTPB(eIsolationLevel, eAutoCommit);

	// From IB docs:
	// Providing a TPB for a transaction is optional. 
	// If one is not provided, then a NULL pointer
	// must be passed to isc_start_transaction()
	// in place of a pointer to the TPB.
	Check(g_ibAPI.isc_start_transaction(
		m_StatusVector,
		&m_handles.m_tr_handle,
		1, &m_handles.m_db_handle,
		m_TPB_buffer_length,
		m_TPB_buffer_length ? m_TPB_buffer : NULL), m_StatusVector);

	assert(m_handles.m_tr_handle != ISC_NULL_HANDLE);
}

void IibConnection::StartTransactionIndirectly()
{
	if (ISC_NULL_HANDLE == m_handles.m_tr_handle)
		StartTransaction(m_pSAConnection->IsolationLevel(),
		m_pSAConnection->AutoCommit());
}

//////////////////////////////////////////////////////////////////////
// IibClient Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IibClient::IibClient()
{

}

IibClient::~IibClient()
{

}

ISAConnection *IibClient::QueryConnectionInterface(
	SAConnection *pSAConnection)
{
	return new IibConnection(pSAConnection);
}

//////////////////////////////////////////////////////////////////////
// IibCursor Class
//////////////////////////////////////////////////////////////////////

class IibCursor : public ISACursor
{
	ibCommandHandles	m_handles;

	ISC_STATUS		m_StatusVector[20];		// Error status vector
	XSQLDA			*m_pInXSQLDA;
	XSQLDA			*m_pOutXSQLDA;

	bool m_bResultSet;
	ISC_LONG readStmtType();
	void closeResultSet();

	XSQLDA *AllocXSQLDA(short nVars);
	void DestroyXSQLDA(XSQLDA *&pXSQLDA);

	void BindBlob(ISC_QUAD &BlobID, SAParam &Param);
	void BindClob(ISC_QUAD &BlobID, SAParam &Param);

	static SADataType_t CnvtNativeToStd(
		const XSQLVAR &var,
		int *pnPrec);
	static void CnvtStdToNative(
		SADataType_t eDataType,
		short &sqltype,
		short &sqlsubtype);

	unsigned short SQLDialect() const;

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
	// should return false if client binds output/return parameters
	// according to value type (coersing)
	// should return true if client binds output/return parameters
	// according to parameter type
	// defaults to false, so we overload it in InterBase client
	virtual bool OutputBindingTypeIsParamType();

	virtual void ReadLongOrLOB(
		ValueType_t eValueType,
		SAValueRead &vr,
		void *pValue,
		size_t nFieldBufSize,
		saLongOrLobReader_t fnReader,
		size_t nReaderWantedPieceSize,
		void *pAddlData);

public:
	IibCursor(
		IibConnection *pIibConnection,
		SACommand *pCommand);
	virtual ~IibCursor();

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

	virtual long GetRowsAffected();

	virtual void DescribeParamSP();

	virtual saCommandHandles *NativeHandles();

	void OnTransactionClosed(void * /*pAddlData*/);

#ifdef SA_UNICODE_WITH_UTF8
	virtual void ConvertString(
		SAString &String,
		const void *pData,
		size_t nRealSize);
#endif
};


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IibCursor::IibCursor(
	IibConnection *pIibConnection,
	SACommand *pCommand) :
	ISACursor(pIibConnection, pCommand)
{
	m_pInXSQLDA = NULL;
	m_pOutXSQLDA = NULL;

	Reset();
}

/*virtual */
IibCursor::~IibCursor()
{
	DestroyXSQLDA(m_pInXSQLDA);
	DestroyXSQLDA(m_pOutXSQLDA);
}

/*virtual */
void IibCursor::ReadLongOrLOB(
	ValueType_t /*eValueType*/,
	SAValueRead &vr,
	void *pValue,
	size_t nBufSize,
	saLongOrLobReader_t fnReader,
	size_t nReaderWantedPieceSize,
	void *pAddlData)
{
	assert(nBufSize == sizeof(ISC_QUAD));
	if (nBufSize != sizeof(ISC_QUAD))
		return;
	ISC_QUAD *pBlobID = (ISC_QUAD*)pValue;

	isc_blob_handle blob_handle = ISC_NULL_HANDLE;   // set handle to NULL before using it
	IibConnection::Check(g_ibAPI.isc_open_blob(
		m_StatusVector,
		&((IibConnection*)m_pISAConnection)->m_handles.m_db_handle,
		&((IibConnection*)m_pISAConnection)->m_handles.m_tr_handle,
		&blob_handle,   // set by this function to refer to the blob
		pBlobID), m_StatusVector);      // Blob ID put into out_sqlda by isc_fetch()

	try
	{
		// get blob size
		char blob_items[] = { isc_info_blob_total_length };
		char blob_info[100];
		IibConnection::Check(g_ibAPI.isc_blob_info(
			m_StatusVector,
			&blob_handle,
			(short)sizeof(blob_items), blob_items,
			(short)sizeof(blob_info), blob_info), m_StatusVector);
		int i = 0;
		unsigned int nBlobSize = 0;	// unknown
		while (blob_info[i] != isc_info_end)
		{
			char item = blob_info[i];
			++i;
			ISC_LONG item_length = g_ibAPI.isc_vax_integer(&blob_info[i], 2); i += 2;
			if (item == isc_info_blob_total_length)
			{
				nBlobSize = g_ibAPI.isc_vax_integer(&blob_info[i], (short)item_length);
				break;
			}
			i += item_length;
		}

		// InterBase can return pieces of smaller size than asked
		// (even if it is not end-of-file),
		// so we might need buffering
		SABufferConverter BufferConverter;
		ISADataConverter *pIConverter = &BufferConverter;
		unsigned int nCnvtBlobSizeMax = nBlobSize;
#ifdef SA_UNICODE
		SAMultibyte2UnicodeConverter Multibyte2UnicodeConverter;
		if( SA_dtLongChar == vr.DataType() || SA_dtCLob == vr.DataType() )
		{
			pIConverter = &Multibyte2UnicodeConverter;
#ifdef SA_UNICODE_WITH_UTF8
			Multibyte2UnicodeConverter.SetUseUTF8();
#endif
			// in the worst case each byte can represent a Unicode character
			nCnvtBlobSizeMax = nBlobSize*sizeof(wchar_t);
	}
#endif

		// Read and process nReaderWantedPieceSize bytes at a time.
		unsigned char* pBuf;
		size_t nPieceSize = vr.PrepareReader(
			nCnvtBlobSizeMax,
			IibConnection::MaxBlobPiece,
			pBuf,
			fnReader,
			nReaderWantedPieceSize,
			pAddlData);
		assert(nPieceSize <= IibConnection::MaxBlobPiece);
		size_t nCnvtPieceSize = nPieceSize;

		// read all the BLOB data by calling isc_get_segment() repeatedly
		// to get each BLOB segment and its length
		SAPieceType_t ePieceType = SA_FirstPiece;
		size_t nTotalRead = 0l;
		size_t nTotalPassedToReader = 0l;
		do
		{
			if (nBlobSize)	// known
				nPieceSize = sa_min(nPieceSize, (nBlobSize - nTotalRead));

			unsigned short actual_seg_length;
			ISC_STATUS status = g_ibAPI.isc_get_segment(
				m_StatusVector,
				&blob_handle,
				&actual_seg_length,	// length of segment read
				(unsigned short)nPieceSize,	// length of segment buffer
				(char*)pBuf);				// segment buffer

			if (status != 0 && m_StatusVector[1] != isc_segment && m_StatusVector[1] != isc_segstr_eof)
				IibConnection::Check(status, m_StatusVector);

			assert(!(m_StatusVector[1] == isc_segstr_eof) || (actual_seg_length == 0));
			nTotalRead += actual_seg_length;

			if (nBlobSize)	// known
			{
				assert(nTotalRead <= nBlobSize);
				if (nTotalRead == nBlobSize)
				{
					if (ePieceType == SA_NextPiece)
						ePieceType = SA_LastPiece;
					else    // the whole BLob has been read in one piece
					{
						assert(ePieceType == SA_FirstPiece);
						ePieceType = SA_OnePiece;
					}
				}
			}
			else
			{
				if (m_StatusVector[1] == isc_segstr_eof)
				{
					if (ePieceType == SA_NextPiece)
						ePieceType = SA_LastPiece;
					else    // the whole Long has been read in one piece
					{
						assert(ePieceType == SA_FirstPiece);
						ePieceType = SA_OnePiece;
					}
				}
			}

			pIConverter->PutStream(pBuf, actual_seg_length, ePieceType);
			size_t nCnvtSize;
			SAPieceType_t eCnvtPieceType;
			// smart while: initialize nCnvtPieceSize before calling pIConverter->GetStream
			while (nCnvtPieceSize = (nCnvtBlobSizeMax ? // known
				sa_min(nCnvtPieceSize, (nCnvtBlobSizeMax - nTotalPassedToReader)) : nCnvtPieceSize),
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
	catch (SAException &)	// try to clean-up
	{
		g_ibAPI.isc_close_blob(m_StatusVector, &blob_handle);
		throw;
	}

	// close the Blob
	IibConnection::Check(g_ibAPI.isc_close_blob(
		m_StatusVector, &blob_handle), m_StatusVector);
	}

/*virtual */
size_t IibCursor::InputBufferSize(
	const SAParam &Param) const
{
	if (!Param.isNull())
	{
		switch (Param.DataType())
		{
#ifdef SA_UNICODE_WITH_UTF8
		case SA_dtString:
			return Param.asString().GetUTF8CharsLength();
#endif
		case SA_dtBool:
			// there is no "native" boolean type in InterBase,
			// so treat boolean as SQL_SHORT in InterBase
			return sizeof(short);
		case SA_dtNumeric:
			return sizeof(INTERBASE_SQL_NUMERIC_STRUCT);
		case SA_dtDateTime:
			return sizeof(ISC_TIMESTAMP);
		case SA_dtLongBinary:
		case SA_dtLongChar:
		case SA_dtBLob:
		case SA_dtCLob:
			return sizeof(ISC_QUAD);
		default:
			break;
	}
}

	return ISACursor::InputBufferSize(Param);
	}

XSQLDA *IibCursor::AllocXSQLDA(short nVars)
{
	size_t nSize = XSQLDA_LENGTH(nVars);
	XSQLDA *pXSQLDA = (XSQLDA *)sa_malloc(nSize);
	memset(pXSQLDA, 0, nSize);
	pXSQLDA->version = SQLDA_VERSION1;
	pXSQLDA->sqln = nVars;
	pXSQLDA->sqld = nVars;

	return pXSQLDA;
}

void IibCursor::DestroyXSQLDA(XSQLDA *&pXSQLDA)
{
	if (pXSQLDA == NULL)
		return;

	::free(pXSQLDA);
	pXSQLDA = NULL;
}

/*virtual */
bool IibCursor::IsOpened()
{
	return m_handles.m_stmt_handle != ISC_NULL_HANDLE;
}

/*virtual */
void IibCursor::Open()
{
	assert(m_handles.m_stmt_handle == ISC_NULL_HANDLE);

	IibConnection::Check(g_ibAPI.isc_dsql_allocate_statement(
		m_StatusVector,
		&((IibConnection*)m_pISAConnection)->m_handles.m_db_handle,
		&m_handles.m_stmt_handle), m_StatusVector);
	assert(m_handles.m_stmt_handle != ISC_NULL_HANDLE);
}

/*virtual */
void IibCursor::Close()
{
	assert(m_handles.m_stmt_handle != ISC_NULL_HANDLE);
	IibConnection::Check(g_ibAPI.isc_dsql_free_statement(
		m_StatusVector,
		&m_handles.m_stmt_handle,
		DSQL_drop), m_StatusVector);
	assert(m_handles.m_stmt_handle == ISC_NULL_HANDLE);
}

/*virtual */
void IibCursor::Destroy()
{
	assert(m_handles.m_stmt_handle != ISC_NULL_HANDLE);
	g_ibAPI.isc_dsql_free_statement(
		m_StatusVector,
		&m_handles.m_stmt_handle,
		DSQL_drop);
	m_handles.m_stmt_handle = ISC_NULL_HANDLE;
}

/*virtual */
void IibCursor::Reset()
{
	m_handles.m_stmt_handle = ISC_NULL_HANDLE;

	DestroyXSQLDA(m_pInXSQLDA);
	DestroyXSQLDA(m_pOutXSQLDA);

	m_bResultSet = false;
}

/*virtual */
ISACursor *IibConnection::NewCursor(SACommand *m_pCommand)
{
	return new IibCursor(this, m_pCommand);
}

/*virtual */
void IibCursor::Prepare(
	const SAString &sCmd,
	SACommandType_t eCmdType,
	int nPlaceHolderCount,
	saPlaceHolder **ppPlaceHolders)
{
	// replace bind variables with '?' place holder
	SAString sStmtIB;
	size_t nPos = 0l;
	int i;

	((IibConnection*)m_pISAConnection)->StartTransactionIndirectly();

	// these counts can be overiden in case of a stored proc
	short nInputParamCount = 0;

	switch (eCmdType)
	{
	case SA_CmdSQLStmt:
		for (i = 0; i < nPlaceHolderCount; ++i)
		{
			sStmtIB += sCmd.Mid(nPos, ppPlaceHolders[i]->getStart() - nPos);
			sStmtIB += _TSA("?");
			nPos = ppPlaceHolders[i]->getEnd() + 1;
		}
		// copy tail
		if (nPos < sCmd.GetLength())
			sStmtIB += sCmd.Mid(nPos);
		break;
	case SA_CmdStoredProc:
		sStmtIB = _TSA("Execute Procedure ");
		sStmtIB += sCmd;
		for (i = 0; i < m_pCommand->ParamCount(); ++i)
		{
			SAParam &Param = m_pCommand->ParamByIndex(i);
			if (Param.ParamDirType() == SA_ParamInput
				|| Param.ParamDirType() == SA_ParamInputOutput)
			{
				if (++nInputParamCount > 1)
					sStmtIB += _TSA(" ,?");
				else
					sStmtIB += _TSA(" ?");
			}
		}

		break;
	case SA_CmdSQLStmtRaw:
		sStmtIB = sCmd;
		break;
	default:
		assert(false);
	}

	SA_TRACE_CMD(sStmtIB);
	IibConnection::Check(g_ibAPI.isc_dsql_prepare(
		m_StatusVector,
		&((IibConnection*)m_pISAConnection)->m_handles.m_tr_handle,
		&m_handles.m_stmt_handle,
		0,
#ifdef SA_UNICODE_WITH_UTF8
		(char*)sStmtIB.GetUTF8Chars(),
#else
		(char*)sStmtIB.GetMultiByteChars(),
#endif
		SQLDialect(), NULL), m_StatusVector);
}

/*virtual */
void IibCursor::UnExecute()
{
	if (m_bResultSet)
		closeResultSet();
}

void IibCursor::BindBlob(ISC_QUAD &BlobID, SAParam &Param)
{
	BlobID.gds_quad_high = 0;
	BlobID.gds_quad_low = 0;

	isc_blob_handle blob_handle = ISC_NULL_HANDLE;	// set handle to NULL before using it
	IibConnection::Check(g_ibAPI.isc_create_blob(
		m_StatusVector,
		&((IibConnection*)m_pISAConnection)->m_handles.m_db_handle,
		&((IibConnection*)m_pISAConnection)->m_handles.m_tr_handle,
		&blob_handle, &BlobID), m_StatusVector);

	size_t nActualWrite;
	SAPieceType_t ePieceType = SA_FirstPiece;
	void *pBuf;
	while ((nActualWrite = Param.InvokeWriter(
		ePieceType,
		IibConnection::MaxBlobPiece, pBuf)) != 0)
	{
		IibConnection::Check(g_ibAPI.isc_put_segment(
			m_StatusVector, &blob_handle,
			(unsigned short)nActualWrite, (char*)pBuf), m_StatusVector);

		if (ePieceType == SA_LastPiece)
			break;
	}

	// Close the Blob
	IibConnection::Check(g_ibAPI.isc_close_blob(
		m_StatusVector,
		&blob_handle), m_StatusVector);
	assert(blob_handle == ISC_NULL_HANDLE);
}

void IibCursor::BindClob(ISC_QUAD &BlobID, SAParam &Param)
{
	BlobID.gds_quad_high = 0;
	BlobID.gds_quad_low = 0;

	isc_blob_handle blob_handle = ISC_NULL_HANDLE;	// set handle to NULL before using it
	IibConnection::Check(g_ibAPI.isc_create_blob(
		m_StatusVector,
		&((IibConnection*)m_pISAConnection)->m_handles.m_db_handle,
		&((IibConnection*)m_pISAConnection)->m_handles.m_tr_handle,
		&blob_handle, &BlobID), m_StatusVector);

#ifdef SA_UNICODE
	SAUnicode2MultibyteConverter Converter;
	size_t nCnvtPieceSize = 0l;
#ifdef SA_UNICODE_WITH_UTF8
	Converter.SetUseUTF8();
#endif
#endif

	size_t nActualWrite;
	SAPieceType_t ePieceType = SA_FirstPiece;
	void *pBuf;
	while ((nActualWrite = Param.InvokeWriter(
		ePieceType,
		IibConnection::MaxBlobPiece, pBuf)) != 0)
	{
#ifdef SA_UNICODE
		Converter.PutStream((unsigned char*)pBuf, nActualWrite, ePieceType);
		size_t nCnvtSize;
		SAPieceType_t eCnvtPieceType;
		// smart while: initialize nCnvtPieceSize before calling pIConverter->GetStream
		while( nCnvtPieceSize = nActualWrite,
			Converter.GetStream((unsigned char*)pBuf, nCnvtPieceSize, nCnvtSize, eCnvtPieceType))
			IibConnection::Check(g_ibAPI.isc_put_segment(
			m_StatusVector, &blob_handle,
			(unsigned short)nCnvtSize, (char*)pBuf), m_StatusVector);
#else
		IibConnection::Check(g_ibAPI.isc_put_segment(
			m_StatusVector, &blob_handle,
			(unsigned short)nActualWrite, (char*)pBuf), m_StatusVector);
#endif

		if (ePieceType == SA_LastPiece)
			break;
}

	// Close the Blob
	IibConnection::Check(g_ibAPI.isc_close_blob(
		m_StatusVector,
		&blob_handle), m_StatusVector);
	assert(blob_handle == ISC_NULL_HANDLE);
	}

void IibCursor::Bind(
	int nPlaceHolderCount,
	saPlaceHolder **ppPlaceHolders)
{
	// we should bind for every place holder ('?')
	// associated with an input or output param
	short nInputCount = ParamDirCount(
		nPlaceHolderCount, ppPlaceHolders, 1, SA_ParamInput);
	short nOutputCount = ParamDirCount(
		nPlaceHolderCount, ppPlaceHolders, 1, SA_ParamOutput);
	// should be true in InterBase
	// because it doesn't has inout params
	assert(nInputCount + nOutputCount == nPlaceHolderCount);

	AllocBindBuffer(nPlaceHolderCount, ppPlaceHolders, sizeof(short), sizeof(short));
	void *pBuf = m_pParamBuffer;

	DestroyXSQLDA(m_pInXSQLDA);
	m_pInXSQLDA = AllocXSQLDA(nInputCount);
	DestroyXSQLDA(m_pOutXSQLDA);
	m_pOutXSQLDA = AllocXSQLDA(nOutputCount);

	int nInput = 0;
	int nOutput = 0;
	for (int i = 0; i < nPlaceHolderCount; ++i)
	{
		SAParam &Param = *ppPlaceHolders[i]->getParam();

		void *pNullInd;
		void *pSize;
		size_t nDataBufSize;
		void *pValue;
		IncParamBuffer(pBuf, pNullInd, pSize, nDataBufSize, pValue);

		if (isInputParam(Param))
		{
			assert(Param.ParamDirType() == SA_ParamInput);

			short &sqltype = m_pInXSQLDA->sqlvar[nInput].sqltype;
			short &sqlsubtype = m_pInXSQLDA->sqlvar[nInput].sqlsubtype;
			short *&sqlind = m_pInXSQLDA->sqlvar[nInput].sqlind;
			short &sqllen = m_pInXSQLDA->sqlvar[nInput].sqllen;
			char *&sqldata = m_pInXSQLDA->sqlvar[nInput].sqldata;

			assert(sqlind == NULL);
			assert(sqldata == NULL);
			sqldata = (char*)pValue;
			sqllen = (short)InputBufferSize(Param);

			if (Param.isNull())
			{
				sqlind = (short*)pNullInd;
				sqltype = SQL_TEXT + 1;	// some type should be set
				assert(sqllen == 0);
				*sqlind = -1;
			}
			else
			{
				switch (Param.DataType())
				{
				case SA_dtUnknown:
					throw SAException(SA_Library_Error, SA_Library_Error_UnknownParameterType, -1, IDS_UNKNOWN_PARAMETER_TYPE, (const SAChar*)Param.Name());
				case SA_dtBool:
					// there is no "native" boolean type in InterBase,
					// so treat boolean as SQL_SHORT in InterBase
					sqltype = SQL_SHORT;
					assert((size_t)sqllen == sizeof(short));
					*(short*)sqldata = (short)Param.asBool();
					break;
				case SA_dtShort:
					sqltype = SQL_SHORT;
					assert((size_t)sqllen == sizeof(short));
					*(short*)sqldata = Param.asShort();
					break;
				case SA_dtUShort:
					sqltype = SQL_SHORT;
					assert((size_t)sqllen == sizeof(unsigned short));
					*(unsigned short*)sqldata = Param.asUShort();
					break;
				case SA_dtLong:
					sqltype = SQL_LONG;
					assert((size_t)sqllen == sizeof(long));
					*(long*)sqldata = Param.asLong();
					break;
				case SA_dtULong:
					sqltype = SQL_LONG;
					assert((size_t)sqllen == sizeof(unsigned long));
					*(unsigned long*)sqldata = Param.asULong();
					break;
				case SA_dtDouble:
					sqltype = SQL_DOUBLE;
					assert((size_t)sqllen == sizeof(double));
					*(double*)sqldata = Param.asDouble();
					break;
				case SA_dtNumeric:
					sqltype = SQL_TEXT;
					assert((size_t)sqllen == sizeof(INTERBASE_SQL_NUMERIC_STRUCT));
					IibConnection::CnvtNumericToInternal(
						Param.asNumeric(),
						*(INTERBASE_SQL_NUMERIC_STRUCT*)sqldata,
						sqllen);
					break;
				case SA_dtDateTime:
					sqltype = SQL_TIMESTAMP;	// == SQL_DATE
					assert((size_t)sqllen == sizeof(ISC_QUAD));
					IibConnection::CnvtDateTimeToInternal(
						Param.asDateTime(), *(ISC_QUAD*)sqldata);
					break;
				case SA_dtString:
					sqltype = SQL_TEXT;
#ifdef SA_UNICODE_WITH_UTF8
					assert((size_t)sqllen == Param.asString().GetUTF8CharsLength());
					memcpy(sqldata, Param.asString().GetUTF8Chars(), sqllen);
#else
					assert(sqllen == (short)Param.asString().GetMultiByteCharsLength());
					memcpy(sqldata, Param.asString().GetMultiByteChars(), sqllen);
#endif
					break;
				case SA_dtBytes:
					sqltype = SQL_TEXT;
					assert((size_t)sqllen == Param.asBytes().GetBinaryLength());
					memcpy(sqldata, (const void*)Param.asBytes(), sqllen);
					break;
				case SA_dtLongBinary:
				case SA_dtBLob:
					sqltype = SQL_BLOB;
					sqlsubtype = 0;	// binary
					assert((size_t)sqllen == sizeof(ISC_QUAD));
					BindBlob(*(ISC_QUAD*)sqldata, Param);
					break;
				case SA_dtLongChar:
				case SA_dtCLob:
					sqltype = SQL_BLOB;
					sqlsubtype = 1;	// text
					assert((size_t)sqllen == sizeof(ISC_QUAD));
					BindClob(*(ISC_QUAD*)sqldata, Param);
					break;
				default:
					assert(false);
				}
			}

			++nInput;
		}
		if (isOutputParam(Param))
		{
			assert(Param.ParamDirType() == SA_ParamOutput);

			short &sqltype = m_pOutXSQLDA->sqlvar[nOutput].sqltype;
			short &sqlsubtype = m_pOutXSQLDA->sqlvar[nOutput].sqlsubtype;
			short *&sqlind = m_pOutXSQLDA->sqlvar[nOutput].sqlind;
			short &sqllen = m_pOutXSQLDA->sqlvar[nOutput].sqllen;
			char *&sqldata = m_pOutXSQLDA->sqlvar[nOutput].sqldata;

			CnvtStdToNative(Param.ParamType(), sqltype, sqlsubtype);
			sqllen = (short)nDataBufSize;

			assert(sqlind == NULL);
			sqltype |= 1;	// tell InterBase to use sqlind
			sqlind = (short*)pNullInd;

			assert(sqldata == NULL);
			if ((sqltype & ~1) == SQL_VARYING)
			{
				// short is reserved for size returning
				// just before the actual string
				sqldata = (char*)pValue - sizeof(short);
			}
			else
				sqldata = (char*)pValue;

			++nOutput;
		}
	}
}

/*virtual */
void IibCursor::Execute(
	int nPlaceHolderCount,
	saPlaceHolder **ppPlaceHolders)
{
	((IibConnection*)m_pISAConnection)->StartTransactionIndirectly();

	if (nPlaceHolderCount)
		Bind(nPlaceHolderCount, ppPlaceHolders);

	XSQLDA *pOutXSQLDA =
		m_pCommand->CommandType() == SA_CmdStoredProc ? m_pOutXSQLDA : NULL;

	IibConnection::Check(g_ibAPI.isc_dsql_execute2(
		m_StatusVector,
		&((IibConnection*)m_pISAConnection)->m_handles.m_tr_handle,
		&m_handles.m_stmt_handle,
		1,
		m_pInXSQLDA,
		pOutXSQLDA), m_StatusVector);

	switch (readStmtType())
	{
	case isc_info_sql_stmt_select:
	case isc_info_sql_stmt_select_for_upd:
		m_bResultSet = true;
		break;
	case isc_info_sql_stmt_insert:
	case isc_info_sql_stmt_update:
	case isc_info_sql_stmt_delete:
	case isc_info_sql_stmt_ddl:
	case isc_info_sql_stmt_get_segment:
	case isc_info_sql_stmt_put_segment:
	case isc_info_sql_stmt_exec_procedure:
	case isc_info_sql_stmt_start_trans:
	case isc_info_sql_stmt_commit:
	case isc_info_sql_stmt_rollback:
	case isc_info_sql_stmt_set_generator:
	default:
		m_bResultSet = false;
		break;
	}

	if (readStmtType() == isc_info_sql_stmt_exec_procedure && m_pOutXSQLDA)
		ConvertOutputParams();
}

/*virtual */
void IibCursor::Cancel()
{
}

ISC_LONG IibCursor::readStmtType()
{
	char type_item[1];
	char res_buffer[8];
	ISC_LONG nLength;

	type_item[0] = isc_info_sql_stmt_type;
	IibConnection::Check(g_ibAPI.isc_dsql_sql_info(
		m_StatusVector,
		&m_handles.m_stmt_handle,
		(short)(sizeof(type_item) / sizeof(char)), type_item,
		(short)(sizeof(res_buffer) / sizeof(char)), res_buffer), m_StatusVector);
	if (res_buffer[0] == isc_info_sql_stmt_type)
	{
		nLength = g_ibAPI.isc_vax_integer(&res_buffer[1], 2);
		return g_ibAPI.isc_vax_integer(&res_buffer[3], (short)nLength);
	}

	return 0;
}

void IibCursor::closeResultSet()
{
	assert(m_bResultSet);
	IibConnection::Check(g_ibAPI.isc_dsql_free_statement(
		m_StatusVector,
		&m_handles.m_stmt_handle,
		DSQL_close), m_StatusVector);
	m_bResultSet = false;

	DestroyXSQLDA(m_pOutXSQLDA);
}

/*virtual */
bool IibCursor::ResultSetExists()
{
	return m_bResultSet;
}

/*static */
SADataType_t IibCursor::CnvtNativeToStd(
	const XSQLVAR &var,
	int *pnPrec)
{
	assert(pnPrec);
	*pnPrec = 0;	// we will only set it for numeric types

	switch (var.sqltype & ~1)
	{
	case SQL_ARRAY:
		return SA_dtBLob;
	case SQL_BLOB:
		if (var.sqlsubtype == 1)
			return SA_dtCLob;
		else
			return SA_dtBLob;
	case SQL_TEXT:
		return SA_dtString;
	case SQL_SHORT:
		*pnPrec = 4;
		if (var.sqlscale == 0)
			return SA_dtShort;
		return SA_dtNumeric;
	case SQL_LONG:
		*pnPrec = 9;
		if (var.sqlscale == 0)
			return SA_dtLong;
		return SA_dtNumeric;
	case SQL_DOUBLE:
		*pnPrec = 15;	// IEEE double precision: 15 digits
		return SA_dtDouble;
	case SQL_INT64:
		*pnPrec = 18;	// max precision in InterBase
		return SA_dtNumeric;
	case SQL_FLOAT:
		*pnPrec = 7;	// IEEE single precision: 7 digits
		return SA_dtDouble;
	case SQL_TYPE_DATE:
		return SA_dtDateTime;
	case SQL_TYPE_TIME:
		return SA_dtDateTime;
	case SQL_TIMESTAMP:	// == SQL_DATE
		return SA_dtDateTime;
	case SQL_VARYING:
		return SA_dtString;
		/*
		case SQL_BOOLEAN:
		return SA_dtBool;
		*/
	case 590: // SQL_BOOLEAN
		return SA_dtShort;

	default:
		assert(false);	// unknown type
	}

	return SA_dtUnknown;
}

/*static */
void IibCursor::CnvtStdToNative(
	SADataType_t eDataType,
	short &sqltype,
	short &sqlsubtype)
{
	switch (eDataType)
	{
	case SA_dtUnknown:
		throw SAException(SA_Library_Error, SA_Library_Error_UnknownDataType, -1, IDS_UNKNOWN_DATA_TYPE);
	case SA_dtBool:
		// there is no "native" boolean type in InterBase,
		// so treat boolean as SQL_SHORT in InterBase
		sqltype = SQL_SHORT;
		break;
	case SA_dtShort:
		sqltype = SQL_SHORT;
		break;
	case SA_dtUShort:
		sqltype = SQL_SHORT;
		break;
	case SA_dtLong:
		sqltype = SQL_LONG;
		break;
	case SA_dtULong:
		sqltype = SQL_LONG;
		break;
	case SA_dtDouble:
		sqltype = SQL_DOUBLE;
		break;
	case SA_dtNumeric:
		sqltype = SQL_VARYING;
		break;
	case SA_dtDateTime:
		sqltype = SQL_TIMESTAMP;
		break;
	case SA_dtString:
		sqltype = SQL_VARYING;
		break;
	case SA_dtBytes:
		sqltype = SQL_VARYING;
		break;
	case SA_dtLongChar:
	case SA_dtCLob:
		sqltype = SQL_BLOB;
		sqlsubtype = 1;	// text
		break;
	case SA_dtLongBinary:
	case SA_dtBLob:
		sqltype = SQL_BLOB;
		sqlsubtype = 0;	// binary
		break;
	default:
		assert(false);
	}
}

/*virtual */
void IibCursor::DescribeFields(
	DescribeFields_cb_t fn)
{
	short cFields = 1;

	XSQLDA *pOutXSQLDA = AllocXSQLDA(cFields);
	try
	{
		IibConnection::Check(g_ibAPI.isc_dsql_describe(
			m_StatusVector,
			&m_handles.m_stmt_handle, 1, pOutXSQLDA), m_StatusVector);

		if (pOutXSQLDA->sqld > pOutXSQLDA->sqln)
		{
			cFields = pOutXSQLDA->sqld;
			DestroyXSQLDA(pOutXSQLDA);
			pOutXSQLDA = AllocXSQLDA(cFields);

			IibConnection::Check(g_ibAPI.isc_dsql_describe(
				m_StatusVector,
				&m_handles.m_stmt_handle, 1, pOutXSQLDA), m_StatusVector);
			assert(pOutXSQLDA->sqld == cFields);
		}

		for (int iField = 0; iField < cFields; ++iField)
		{
			XSQLVAR &var = pOutXSQLDA->sqlvar[iField];

			int nPrec;	// calculated by CnvtNativeToStd
			SADataType_t eDataType = CnvtNativeToStd(
				var,
				&nPrec);
			(m_pCommand->*fn)(
				SAString(var.aliasname, var.aliasname_length),
				eDataType,
				var.sqltype & ~1,
				var.sqllen,
				nPrec,
				-var.sqlscale,
				!(var.sqltype & 1),
                (int)cFields);
		}

		DestroyXSQLDA(pOutXSQLDA);
	}
	catch (SAException &)	// clean-up
	{
		DestroyXSQLDA(pOutXSQLDA);
		throw;
	}
}

/*virtual */
long IibCursor::GetRowsAffected()
{
	char type_item[1];
	char res_buffer[1048];

	type_item[0] = isc_info_sql_records;
	IibConnection::Check(g_ibAPI.isc_dsql_sql_info(
		m_StatusVector,
		&m_handles.m_stmt_handle,
		(short)(sizeof(type_item) / sizeof(char)), type_item,
		(short)(sizeof(res_buffer) / sizeof(char)), res_buffer), m_StatusVector);
	if (res_buffer[0] == isc_info_sql_records)
	{
		switch (readStmtType())
		{
		case isc_info_sql_stmt_update:
			return g_ibAPI.isc_vax_integer(&res_buffer[6], 2);
		case isc_info_sql_stmt_delete:
			return g_ibAPI.isc_vax_integer(&res_buffer[13], 2);
		case isc_info_sql_stmt_insert:
			return g_ibAPI.isc_vax_integer(&res_buffer[27], 2);
		}
	}

	return -1;
}

/*virtual */
size_t IibCursor::OutputBufferSize(
	SADataType_t eDataType,
	size_t nDataSize) const
{
	switch (eDataType)
	{
	case SA_dtNumeric:
		return sizeof(INTERBASE_SQL_NUMERIC_STRUCT);
	case SA_dtDateTime:
		return sizeof(ISC_TIMESTAMP);
	case SA_dtBLob:
	case SA_dtCLob:
		return sizeof(ISC_QUAD);
	default:
		break;
	}

	return ISACursor::OutputBufferSize(eDataType, nDataSize);
}


/*virtual */
void IibCursor::SetFieldBuffer(
	int nCol,	// 1-based
	void *pInd,
	size_t nIndSize,
	void * /*pSize*/,
	size_t nSizeSize,
	void *pValue,
	size_t nValueSize)
{
	assert(m_pOutXSQLDA != NULL);	// SetSelectBuffers should allocate it
	assert(nIndSize == sizeof(short));
	if (nIndSize != sizeof(short))
		return;
	assert(nSizeSize == sizeof(short));
	if (nSizeSize != sizeof(short))
		return;

	short &sqltype = m_pOutXSQLDA->sqlvar[nCol - 1].sqltype;
	short &sqlsubtype = m_pOutXSQLDA->sqlvar[nCol - 1].sqlsubtype;
	short &sqlscale = m_pOutXSQLDA->sqlvar[nCol - 1].sqlscale;
	short *&sqlind = m_pOutXSQLDA->sqlvar[nCol - 1].sqlind;
	short &sqllen = m_pOutXSQLDA->sqlvar[nCol - 1].sqllen;
	char *&sqldata = m_pOutXSQLDA->sqlvar[nCol - 1].sqldata;

	SAField &Field = m_pCommand->Field(nCol);

	CnvtStdToNative(Field.FieldType(), sqltype, sqlsubtype);
	sqlscale = 0;
	sqllen = (short)nValueSize;

	assert(sqlind == NULL);
	sqltype |= 1;	// tell InterBase to use sqlind
	sqlind = (short*)pInd;

	assert(sqldata == NULL);
	if ((sqltype & ~1) == SQL_VARYING)
	{
		// short is reserved for size returning 
		// just before the actual string
		sqldata = (char*)pValue - sizeof(short);
	}
	else
		sqldata = (char*)pValue;
}

/*virtual */
bool IibCursor::ConvertIndicator(
	int nPos,	// 1-based
	int/* nNotConverted*/,
	SAValueRead &vr,
	ValueType_t/* eValueType*/,
	void * /*pInd*/,
	size_t nIndSize,
	void * /*pSize*/,
	size_t nSizeSize,
	size_t &nRealSize,
	int/* nBulkReadingBufPos*/) const
{
	assert(m_pOutXSQLDA != NULL);	// should always be called after DescribeFields
	assert(nIndSize == sizeof(short));
	if (nIndSize != sizeof(short))
		return false;	// not converted
	assert(nSizeSize == sizeof(short));
	if (nSizeSize != sizeof(short))
		return false;	// not converted

	XSQLVAR &var = m_pOutXSQLDA->sqlvar[nPos - 1];

	// no indicator, field can not be NULL, or it is not null
	*vr.m_pbNull = !(var.sqlind == NULL || *var.sqlind != -1);

	if (!vr.isNull())
	{
		if ((var.sqltype & ~1) == SQL_VARYING)
		{
			// short is reserved for size returning 
			// just before the actual string
			nRealSize = *(short*)var.sqldata;
		}
		else
			nRealSize = var.sqllen;
	}

	return true;	// converted
}

// default implementation (ISACursor::OutputBindingTypeIsParamType)
// returns false
// But in InterBase out parameters are always bound separately
/*virtual */
bool IibCursor::OutputBindingTypeIsParamType()
{
	return true;
}

/*virtual */
void IibCursor::SetSelectBuffers()
{
	long cFields = m_pCommand->FieldCount();

	DestroyXSQLDA(m_pOutXSQLDA);
	m_pOutXSQLDA = AllocXSQLDA((short)cFields);

	// use default helpers
	AllocSelectBuffer(sizeof(short), sizeof(short));
}

/*virtual */
bool IibCursor::FetchNext()
{
	ISC_STATUS Status;
	Status = g_ibAPI.isc_dsql_fetch(
		m_StatusVector,
		&m_handles.m_stmt_handle,
		1, m_pOutXSQLDA);
	if (Status != 100)
	{
		IibConnection::Check(Status, m_StatusVector);

		// use default helpers
		ConvertSelectBufferToFields(0);
	}
	else
		//m_bResultSet = false;
		closeResultSet();

	return Status != 100;
}

/*static */
SAString IibConnection::FormatIdentifierValue(
	unsigned short nSQLDialect,
	const SAString &sValue)
{
	if (nSQLDialect == 1)
	{
		SAString sTemp = sValue;
		sTemp.TrimLeft();
		sTemp.TrimRight();

		return sTemp;
	}

	return sValue;
}

/*virtual */
void IibCursor::DescribeParamSP()
{
	SAString sProcName = m_pCommand->CommandText();
	SAString sStmtIB = _TSA("Execute Procedure ");
	sStmtIB += sProcName;

	// read sp parameters from system tables
	SAString sSQL =
		"SELECT RDB$PARAMETER_NAME,  RDB$PARAMETER_TYPE "
		"FROM RDB$PROCEDURE_PARAMETERS "
		"WHERE UPPER(RDB$PROCEDURE_NAME) = UPPER('";
	sSQL += IibConnection::FormatIdentifierValue(SQLDialect(), sProcName);
	sSQL += _TSA("') ORDER BY RDB$PARAMETER_NUMBER");

	SACommand cmd(m_pISAConnection->getSAConnection(), sSQL);
	cmd.Execute();
	int nTotal = 0;
	int nInputs = 0;
	while (cmd.FetchNext())
	{
		SAString sName = cmd[_TSA("RDB$PARAMETER_NAME")].asString();
		sName.TrimRight(_TSA(" "));
		SAParamDirType_t eDirType = cmd[_TSA("RDB$PARAMETER_TYPE")].asShort() == 0 ? SA_ParamInput : SA_ParamOutput;
		if (eDirType == SA_ParamInput)	// input
		{
			if (++nInputs > 1)
				sStmtIB += _TSA(" ,?");
			else
				sStmtIB += _TSA(" ?");

			// create input parameter
			m_pCommand->CreateParam(sName, SA_dtUnknown, -1, 0, -1, -1, eDirType);
		}
		else
		{
			// we will create outputs later
		}

		++nTotal;
	}

	// query and create output parameters
	if (nTotal > nInputs)
	{
		cmd.setCommandText(sStmtIB);
		cmd.Prepare();
		int nOutputs = cmd.FieldCount();
		assert(nTotal == nInputs + nOutputs);
		for (int i = 1; i <= nOutputs; ++i)
		{
			SAField &Field = cmd.Field(i);
			m_pCommand->CreateParam(
				Field.Name(),
				Field.FieldType(), Field.FieldNativeType(),
				Field.FieldSize(),
				Field.FieldPrecision(), Field.FieldScale(),
				SA_ParamOutput);
		}

		cmd.Close();
	}
}

/*virtual */
saAPI *IibConnection::NativeAPI() const
{
	return &g_ibAPI;
}

/*virtual */
saConnectionHandles *IibConnection::NativeHandles()
{
	return &m_handles;
}

/*virtual */
saCommandHandles *IibCursor::NativeHandles()
{
	return &m_handles;
}

/*virtual */
void IibConnection::setIsolationLevel(
	SAIsolationLevel_t /*eIsolationLevel*/)
{
	// for new settings to take affect we should start new transaction
	CommitTransaction();	// commit and close the transaction
	// adjust isolation level and start new transaction
	// DISABLED: start transaction only when query executed
	//StartTransaction(eIsolationLevel, m_pSAConnection->AutoCommit());		// reopen
}

/*virtual */
void IibConnection::setAutoCommit(
	SAAutoCommit_t /*eAutoCommit*/)
{
	// for new settings to take affect we should start new transaction
	CommitTransaction();	// commit and close the transaction
	// adjust auto-commit option and start new transaction
	// DISABLED: start transaction only when query executed
	// StartTransaction(m_pSAConnection->IsolationLevel(), eAutoCommit);		// reopen
}

unsigned short IibCursor::SQLDialect() const
{
	SAString s = m_pCommand->Option(_TSA("SQLDialect"));
	if (s.IsEmpty())
		return 3;	// defaults to SQL dialect 3

	return (unsigned short)atoi(s.GetMultiByteChars());
}

static void SQLAPI_CALLBACK TransactionClosed(ISACursor *pCursor, void *pAddlData)
{
	((IibCursor*)pCursor)->OnTransactionClosed(pAddlData);
}

void IibCursor::OnTransactionClosed(void * /*pAddlData*/)
{
	m_bResultSet = false;
}

#ifdef SA_UNICODE_WITH_UTF8
/*virtual */
void IibCursor::ConvertString(SAString &String, const void *pData, size_t nRealSize)
{
	String.SetUTF8Chars((const char*)pData, nRealSize);
	}
#endif

void IibConnection::CommitTransaction()
{
	// start transaction only when query executed
	if (ISC_NULL_HANDLE == m_handles.m_tr_handle)
		return;

	Check(g_ibAPI.isc_commit_transaction(
		m_StatusVector, &m_handles.m_tr_handle), m_StatusVector);
	assert(m_handles.m_tr_handle == ISC_NULL_HANDLE);

	EnumCursors(TransactionClosed, NULL);
}

void IibConnection::CommitRetaining()
{
	// start transaction only when query executed
	if (ISC_NULL_HANDLE == m_handles.m_tr_handle)
		return;

	Check(g_ibAPI.isc_commit_retaining(
		m_StatusVector, &m_handles.m_tr_handle), m_StatusVector);
	assert(m_handles.m_tr_handle != ISC_NULL_HANDLE);
}

void IibConnection::RollbackTransaction()
{
	// start transaction only when query executed
	if (ISC_NULL_HANDLE == m_handles.m_tr_handle)
		return;

	Check(g_ibAPI.isc_rollback_transaction(
		m_StatusVector, &m_handles.m_tr_handle), m_StatusVector);
	assert(m_handles.m_tr_handle == ISC_NULL_HANDLE);

	EnumCursors(TransactionClosed, NULL);
}

/*virtual */
void IibConnection::CnvtInternalToNumeric(
	SANumeric &numeric,
	const void *pInternal,
	int nInternalSize)
{
	numeric = SAString((const char*)pInternal, nInternalSize);
}

/*static */
void IibConnection::CnvtNumericToInternal(
	const SANumeric &numeric,
	INTERBASE_SQL_NUMERIC_STRUCT &Internal,
	short &sqllen)
{
	SAString sNum = numeric.operator SAString();
	sqllen = (short)sNum.GetMultiByteCharsLength();
	::memcpy(Internal, sNum.GetMultiByteChars(), sqllen);
}
