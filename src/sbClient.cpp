// sbClient.cpp: implementation of the IibClient class.
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <sbAPI.h>
#include "sbClient.h"

typedef char SqlBaseDate_t[SQLSDAT];	// max size, real is variable and can be less

class IsbConnection : public ISAConnection
{
	friend class IsbCursor;
	friend class Isb6Cursor;
	friend class Isb7Cursor;

	enum MaxLong {MaxLongPiece = (unsigned int)0x7FFF};

	static void CnvtNumericToInternal(
		const SANumeric &numeric,
		double &Internal);
	static void CnvtDateTimeToInternal(
		const SADateTime &date_time,
		SqlBaseDate_t &Internal,
		SQLTDAL &dal);

protected:
	SQLTCUR	m_cur;	// SQLBase cursor number

	static void Check(const SQLTRCD &rcd);

protected:
	virtual ~IsbConnection();

public:
	IsbConnection(SAConnection *pSAConnection);

	virtual long GetClientVersion() const;
	virtual long GetServerVersion() const;
	virtual SAString GetServerVersionString() const;
	
	virtual bool IsAlive() const;
	virtual void Connect(
		const SAString &sDBString,
		const SAString &sUserID,
		const SAString &sPassword,
		saConnectionHandler_t fHandler);
	virtual void Disconnect();
	virtual void Destroy();

	virtual void setIsolationLevel(
		SAIsolationLevel_t eIsolationLevel);
	virtual void setAutoCommit(
		SAAutoCommit_t eAutoCommit);
	virtual void Commit();
	virtual void Rollback();

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

class Isb6Connection : public IsbConnection
{
	friend class Isb6Cursor;

	sb6ConnectionHandles m_handles;	// copy

	SAString	m_sConnectString;	// connect string

protected:
	virtual ~Isb6Connection();

public:
	Isb6Connection(SAConnection *pSAConnection);

	virtual void InitializeClient();
	virtual void UnInitializeClient();

	virtual bool IsConnected() const;
	virtual void Connect(
		const SAString &sDBString,
		const SAString &sUserID,
		const SAString &sPassword,
		saConnectionHandler_t fHandler);
	virtual void Disconnect();
	virtual void Destroy();
	virtual void Reset();

	virtual saAPI *NativeAPI() const;
	virtual saConnectionHandles *NativeHandles();

	virtual ISACursor *NewCursor(SACommand *m_pCommand);
};

class Isb7Connection : public IsbConnection
{
	friend class Isb7Cursor;

	sb7ConnectionHandles m_handles;	// copy

	SQLTCON	m_hCon;

protected:
	virtual ~Isb7Connection();

public:
	Isb7Connection(SAConnection *pSAConnection);

	virtual void InitializeClient();
	virtual void UnInitializeClient();

	virtual bool IsConnected() const;
	virtual void Connect(
		const SAString &sDBString,
		const SAString &sUserID,
		const SAString &sPassword,
		saConnectionHandler_t fHandler);
	virtual void Disconnect();
	virtual void Destroy();
	virtual void Reset();

	virtual saAPI *NativeAPI() const;
	virtual saConnectionHandles *NativeHandles();

	virtual ISACursor *NewCursor(SACommand *m_pCommand);
};

//////////////////////////////////////////////////////////////////////
// IsbClient Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IsbClient::IsbClient()
{

}

IsbClient::~IsbClient()
{

}

ISAConnection *IsbClient::QueryConnectionInterface(
	SAConnection *pSAConnection)
{
	if(CanBeLoadedSB7(pSAConnection))
		return new Isb7Connection(pSAConnection);

	return new Isb6Connection(pSAConnection);
}

//////////////////////////////////////////////////////////////////////
// IsbConnection Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IsbConnection::IsbConnection(
	SAConnection *pSAConnection) : ISAConnection(pSAConnection)
{
	m_cur = 0;
}

IsbConnection::~IsbConnection()
{
}

/*virtual */
bool IsbConnection::IsAlive() const
{
	SQLTDAL len;
	char sVer[1024];
    return (0 == g_sb6API.sqlget(m_cur, SQLPVER, (SQLTDAP)sVer, &len));
}

/*virtual */
void IsbConnection::Connect(
	const SAString & /*sDBString*/,
	const SAString & /*sUserID*/,
	const SAString & /*sPassword*/,
	saConnectionHandler_t fHandler)
{
	assert(m_cur != 0);	// should be connected by derived classes

	if( NULL != fHandler )
		fHandler(*m_pSAConnection, SA_PostConnectHandler);
}

/*virtual */
void IsbConnection::Disconnect()
{
	assert(m_cur != 0);
	Check(g_sb6API.sqldis(m_cur));

	m_cur	= 0;
}

/*virtual */
void IsbConnection::Destroy()
{
	assert(m_cur != 0);
	g_sb6API.sqldis(m_cur);

	m_cur	= 0;
}

/*virtual */
void IsbConnection::Commit()
{
	Check(g_sb6API.sqlcmt(m_cur));
}

/*virtual */
void IsbConnection::Rollback()
{
	Check(g_sb6API.sqlrbk(m_cur));
}

/*virtual */
void IsbConnection::CnvtInternalToDateTime(
	SADateTime &date_time,
	const void *pInternal,
	int nInternalSize)
{
	assert((size_t)nInternalSize <= sizeof(SqlBaseDate_t));

	const char sMask[] = "DD\0 MM\0 YYYY\0 HH\0 MI\0 SS\0 999999999";
	char sBuf[sizeof(sMask)];

	IsbConnection::Check(g_sb6API.sqlxdp(
		(SQLTDAP)sBuf, sizeof(sBuf),
		(SQLTNMP)pInternal, (SQLTNML)nInternalSize,
		(SQLTDAP)sMask, sizeof(sMask)));

	date_time = SADateTime(
		atoi(sBuf+8),
		atoi(sBuf+4),
		atoi(sBuf),
		atoi(sBuf+14),
		atoi(sBuf+18),
		atoi(sBuf+22));

	// convert SQLBase microseconds to SQLAPI++ nanoseconds
	date_time.Fraction() = atoi(sBuf+26);
}

/*virtual */
void IsbConnection::CnvtInternalToInterval(
		SAInterval & /*interval*/,
		const void * /*pInternal*/,
		int /*nInternalSize*/)
{
	assert(false);
}

/*static */
void IsbConnection::CnvtDateTimeToInternal(
	const SADateTime &date_time,
	SqlBaseDate_t &time,
	SQLTDAL &dal)
{
	SAString sTime;
	sTime.Format(_TSA("%.2d.%.2d.%.4d %.2d:%.2d:%.2d %.9d"),
		date_time.GetDay(), date_time.GetMonth(), date_time.GetYear(),
		date_time.GetHour(), date_time.GetMinute(), date_time.GetSecond(), (int)date_time.Fraction());

	// SQLBase works correctly only if we have seven 9999999, bug or what?
	const char sMask[] = "DD.MM.YYYY HH:MI:SS 9999999";

	SQLTNML olp = sizeof(SqlBaseDate_t);
	IsbConnection::Check(
		g_sb6API.sqlxpd(
		(SQLTNMP)&time,
		&olp,
		(SQLTDAP)sTime.GetMultiByteChars(),
		(SQLTDAP)sMask, (SQLTDAL)strlen(sMask)));

	dal = olp;	// return real size to be used
}

/*virtual */
void IsbConnection::CnvtInternalToCursor(
	SACommand * /*pCursor*/,
	const void * /*pInternal*/)
{
	assert(false);
}

/*virtual */
long IsbConnection::GetClientVersion() const
{
	return g_nSBDLLVersionLoaded;
}

/*virtual */
long IsbConnection::GetServerVersion() const
{
	SQLTDAL len;
	char sVer[1024];

    Check(g_sb6API.sqlget(m_cur, SQLPVER, (SQLTDAP)sVer, &len));
	sVer[len] = 0;
	char *sPoint;
	short nMajor = (short)strtol(sVer, &sPoint, 10);
	assert(*sPoint == '.');
	sPoint++;
	short nMinor = (short)strtol(sPoint, &sPoint, 10);
	return SA_MAKELONG(nMinor, nMajor);
}

/*virtual */
SAString IsbConnection::GetServerVersionString() const
{
	SQLTDPV brand;

    Check(g_sb6API.sqlget(m_cur, SQLPBRN, (SQLTDAP)&brand, NULL));
	SAString sVer;
	switch(brand)
	{
	case SQLBSQB:
		sVer = "Centura SQLBase";
		break;
	case SQLBDB2:
		sVer = "IBM DB2";
		break;
	case SQLBDBM:
		sVer = "IBM OS/2 Database Manager";
		break;
	case SQLBORA:
		sVer = "Oracle";
		break;
	case SQLBIGW:
		sVer = "Informix";
		break;
	case SQLBNTW:
		sVer = "Netware SQL";
		break;
	case SQLBAS4:
		sVer = "IBM AS/400 SQL/400";
		break;
	case SQLBSYB:
		sVer = "Sybase SQL Server";
		break;
	case SQLBDBC:
		sVer = "Teradata DBC Machines";
		break;
	case SQLBALB:
		sVer = "HP Allbase";
		break;
	case SQLBRDB:
		sVer = "DEC's RDB";
		break;
	case SQLBTDM:
		sVer = "Tandem's Nonstop SQL";
		break;
	case SQLBSDS:
		sVer = "IBM SQL/DS";
		break;
	case SQLBSES:
		sVer = "SNI SESAM";
		break;
	case SQLBING:
		sVer = "Ingres";
		break;
	case SQLBSQL:
		sVer = "SQL Access";
		break;
	case SQLBDBA:
		sVer = "DBase";
		break;
	case SQLBDB4:
		sVer = "SNI DDB4";
		break;
	case SQLBFUJ:
		sVer = "Fujitsu RDBII";
		break;
	case SQLBSUP:
		sVer = "Cincom SUPRA";
		break;
	case SQLB204:
		sVer = "CCA Model 204";
		break;
	case SQLBDAL:
		sVer = "Apple DAL interface";
		break;
	case SQLBSHR:
		sVer = "Teradata ShareBase";
		break;
	case SQLBIOL:
		sVer = "Informix On-Line";
		break;
	case SQLBEDA:
		sVer = "EDA/SQL";
		break;
	case SQLBUDS:
		sVer = "SNI UDS";
		break;
	case SQLBMIM:
		sVer = "Nocom Mimer";
		break;
	case SQLBOR7:
		sVer = "Oracle version 7";
		break;
	case SQLBIOS:
		sVer = "Ingres OpenSQL";
		break;
	case SQLBIOD:
		sVer = "Ingres OpenSQL with date support";
		break;
	case SQLBODB:
		sVer = "ODBC Router";
		break;
	case SQLBS10:
		sVer = "SYBASE System 10";
		break;
	case SQLBSE6:
		sVer = "Informix SE version 6";
		break;
	case SQLBOL6:
		sVer = "Informix On-Line version 6";
		break;
	case SQLBNSE:
		sVer = "Informix SE NLS version 6";
		break;
	case SQLBNOL:
		sVer = "Informix On-Line NLS version 6";
		break;
	case SQLBAPP:
		sVer = "SQLHost App Services";
		break;
	default:
		assert(false);
		sVer = "Unknown server";
	}

    if(brand == SQLBSQB)
	{
		char s[1024];
		SQLTDAL len;
	    Check(g_sb6API.sqlget(m_cur, SQLPVER, (SQLTDAP)s, &len));
		sVer += _TSA(" Release ");
		sVer += SAString(s, len);
	}

	return sVer;
}

/*static */
void IsbConnection::Check(const SQLTRCD &rcd)
{
	if(!rcd)
		return;

	char msg[SQLMERR];
	g_sb6API.sqlfer(rcd, (SQLTDAP)msg);
	
	throw SAException(SA_DBMS_API_Error, rcd, -1, SAString(msg));
}

//////////////////////////////////////////////////////////////////////
// Isb6Connection Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Isb6Connection::Isb6Connection(
	SAConnection *pSAConnection) : IsbConnection(pSAConnection)
{
}

Isb6Connection::~Isb6Connection()
{
}

/*virtual */
void Isb6Connection::InitializeClient()
{
	::AddSB6Support(m_pSAConnection);
}

/*virtual */
void Isb6Connection::UnInitializeClient()
{
	if( SAGlobals::UnloadAPI() ) ::ReleaseSB6Support();
}

/*virtual */
bool Isb6Connection::IsConnected() const
{
	return m_cur != 0;
}

/*virtual */
void Isb6Connection::Connect(
	const SAString &sDBString,
	const SAString &sUserID,
	const SAString &sPassword,
	saConnectionHandler_t fHandler)
{
	assert(m_cur == 0);

	SAString sConnectString;
	sConnectString += sDBString;
	sConnectString += _TSA("/");
	sConnectString += sUserID;
	sConnectString += _TSA("/");
	sConnectString += sPassword;

	if( NULL != fHandler )
		fHandler(*m_pSAConnection, SA_PostConnectHandler);

	Check(g_sb6API.sqlcnc(&m_cur, (SQLTDAP)sConnectString.GetMultiByteChars(), 0));
	m_sConnectString = sConnectString;

	IsbConnection::Connect(sDBString, sUserID, sPassword, fHandler);
}


/*virtual */
void Isb6Connection::Disconnect()
{
	IsbConnection::Disconnect();

	m_sConnectString.Empty();
}

/*virtual */
void Isb6Connection::Destroy()
{
	IsbConnection::Destroy();

	m_sConnectString.Empty();
}

/*virtual */
void Isb6Connection::Reset()
{
	m_cur = m_handles.m_cur = 0;
	m_sConnectString.Empty();
}

//////////////////////////////////////////////////////////////////////
// Isb7Connection Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Isb7Connection::Isb7Connection(
	SAConnection *pSAConnection) : IsbConnection(pSAConnection)
{
	m_hCon = 0;
}

Isb7Connection::~Isb7Connection()
{
}

/*virtual */
void Isb7Connection::InitializeClient()
{
	::AddSB7Support(m_pSAConnection);
}

/*virtual */
void Isb7Connection::UnInitializeClient()
{
	if( SAGlobals::UnloadAPI() ) ::ReleaseSB7Support();
}

/*virtual */
bool Isb7Connection::IsConnected() const
{
	return m_hCon != 0;
}

/*virtual */
void Isb7Connection::Connect(
	const SAString &sDBString,
	const SAString &sUserID,
	const SAString &sPassword,
	saConnectionHandler_t fHandler)
{
	assert(m_hCon == 0);
	assert(m_cur == 0);

	SAString sConnectString;
	sConnectString += sDBString;
	sConnectString += _TSA("/");
	sConnectString += sUserID;
	sConnectString += _TSA("/");
	sConnectString += sPassword;

	if( NULL != fHandler )
		fHandler(*m_pSAConnection, SA_PostConnectHandler);

	Check(g_sb7API.sqlcch(&m_hCon, (SQLTDAP)sConnectString.GetMultiByteChars(), 0, 0));
	Check(g_sb7API.sqlopc(&m_cur, m_hCon, 0));

	IsbConnection::Connect(sDBString, sUserID, sPassword, fHandler);
}

/*virtual */
void Isb7Connection::Disconnect()
{
	IsbConnection::Disconnect();

	assert(m_hCon != 0);
	Check(g_sb7API.sqldch(m_hCon));

	m_hCon = 0;
}

/*virtual */
void Isb7Connection::Destroy()
{
	IsbConnection::Disconnect();

	assert(m_hCon != 0);
	g_sb7API.sqldch(m_hCon);

	m_hCon = 0;
}

/*virtual */
void Isb7Connection::Reset()
{
	m_cur = m_handles.m_cur = 0;
	m_hCon = m_handles.m_hCon = 0;
}

//////////////////////////////////////////////////////////////////////
// IsbCursor Class
//////////////////////////////////////////////////////////////////////


class IsbCursor : public ISACursor
{
	bool m_bNeedToRetriveBeforeRebind;
	bool m_bResultSetCanBe;
	bool m_bOutputParamsRowPrefetched;
	
	bool m_bScrollableResultSetMode;
	SQLTROW m_nCurRow;
	bool FetchRow(SQLTROW nRow);

	enum StoredObject
	{
		IsbCursor_StoredUnknown,
		IsbCursor_StoredProcedure,
		IsbCursor_StoredCommand
	} m_eStoredObject;

	typedef struct tagLongContext
	{
		SQLTSLC slc;
	} LongContext_t;
		
	bool m_bFieldsDescribed;
	SQLTNSI m_nsi;
	struct FieldDescr
	{
		SQLTDDT edt;
		SQLTDDL edl;
		char ch[1024];
		SQLTCHL chl;
		SQLTPRE pre;
		SQLTSCA sca;

		SQLTDDT ddt;
		SQLTDDL	ddl;
		bool bRequired;
	} *m_pFieldDescr;

	void InternalDescribeFields();
	SQLTCTY getStmtType();

	SADataType_t CnvtNativeToStd(
		SQLTDDL edt, SQLTPRE pre, SQLTSCA sca, SQLTDDL ddt) const;
	SQLTPDT CnvtStdToNative(SADataType_t eDataType) const;

	void BindBlob(int nPos, SAParam &Param);

protected:
	sbCommandHandles	m_handles;

	IsbCursor(
		IsbConnection *pIsbConnection,
		SACommand *pCommand);
	virtual ~IsbCursor();

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
	// defaults to false, so we overload it in SQLBase C/API client
	virtual bool OutputBindingTypeIsParamType();
    virtual bool IgnoreUnknownDataType();

public:
	virtual void Open();
	virtual bool IsOpened();
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

	// Notifications
	virtual void OnConnectionAutoCommitChanged();
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IsbCursor::IsbCursor(
	IsbConnection *pIsbConnection,
	SACommand *pCommand) :
	ISACursor(pIsbConnection, pCommand)
{
	m_pFieldDescr = NULL;
	Reset();
}

/*virtual */
IsbCursor::~IsbCursor()
{
	if(m_pFieldDescr)
		::free(m_pFieldDescr);
}

/*virtual */
size_t IsbCursor::InputBufferSize(
	const SAParam &Param) const
{
	if(!Param.isNull())
	{
		switch(Param.DataType())
		{
		case SA_dtBool:
			// treat boolean bind in SQLBase as short bind (SQLPSSH)
			return sizeof(short);
		case SA_dtNumeric:
			// there is no need to use SQLBase internal number
			// because maximum precision in SQLBase is 15
			// and can be handled by C double
			return sizeof(double);
		case SA_dtDateTime:
			return sizeof(SqlBaseDate_t);	// SqlBase internal date/time representation
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

/*virtual */
SADataType_t IsbCursor::CnvtNativeToStd(
	SQLTDDL edt, SQLTPRE pre, SQLTSCA sca, SQLTDDL ddt) const
{
	SADataType_t eDataType = SA_dtUnknown;

	if(ddt == SQLDLON)
		eDataType = SA_dtLongChar;
	else
	{
		switch(edt)
		{
		case SQLEBIN:	// BINARY
			eDataType = SA_dtBytes;
			break;
		case SQLEBOO:	// BOOLEAN
			eDataType = SA_dtShort;
			break;
		case SQLECHR:	// CHAR
			eDataType = SA_dtString;
			break;
		case SQLEDAT:	// DATE
			eDataType = SA_dtDateTime;
			break;
		case SQLEDEC:	// DECIMAL
			if(sca > 0)
				eDataType = SA_dtNumeric;
			else	// check for exact type
			{	
				if(pre < 5)
					eDataType = SA_dtShort;
				else if(pre < 10)
					eDataType = SA_dtLong;
				else
					eDataType = SA_dtNumeric;
			}
			break;
		case SQLEDOU:	// DOUBLE
			eDataType = SA_dtDouble;
			break;
		case SQLEFLO:	// FLOAT
			eDataType = SA_dtDouble;
			break;
		case SQLEGPH:	// GRAPHIC
			eDataType = SA_dtBytes;
			break;
		case SQLEINT:	// INTEGER
			eDataType = SA_dtLong;
			break;
		case SQLELBI:	// LONG BINARY
			eDataType = SA_dtLongBinary;
			break;
		case SQLELCH:	// CHAR>254
			eDataType = SA_dtLongChar;
			break;
		case SQLELGP:	// LONG VAR GRAPHIC
			eDataType = SA_dtLongBinary;
			break;
		case SQLELON:	// LONG VARCHAR
			eDataType = SA_dtLongChar;
			break;
		case SQLEMON:	// MONEY
			eDataType = SA_dtDouble;
			break;
		case SQLESMA:	// SMALLINT
			eDataType = SA_dtShort;
			break;
		case SQLETIM:	// TIME
			eDataType = SA_dtDateTime;
			break;
		case SQLETMS:	// TIMESTAMP
			eDataType = SA_dtDateTime;
			break;
		case SQLEVAR:	// VARCHAR
            eDataType = SA_dtString;
            break;
        case SQLELVR:	// VARCHAR > 254
            eDataType = SA_dtLongChar;
            break;
		case SQLEVBI:	// VAR BINARY
			eDataType = SA_dtBytes;
			break;
		case SQLEVGP:	// VAR GRAPHIC
			eDataType = SA_dtBytes;
			break;
		default:
            eDataType = SA_dtUnknown;
            break;
		}
	}

	return eDataType;
}

/*virtual */
bool IsbCursor::IgnoreUnknownDataType()
{
    return true;
}

SQLTPDT IsbCursor::CnvtStdToNative(SADataType_t eDataType) const
{
	SQLTPDT pdt;
	switch(eDataType)
	{
	case SA_dtUnknown:
		throw SAException(SA_Library_Error, SA_Library_Error_UnknownDataType, -1, IDS_UNKNOWN_DATA_TYPE);
	case SA_dtBool:
		// treat boolean bind in SQLBase as short bind (SQLPSSH)
		pdt = SQLPSSH;	// Short
		break;
	case SA_dtShort:
		pdt = SQLPSSH;	// Short
		break;
	case SA_dtUShort:
		pdt = SQLPUSH;	// Unsigned short
		break;
	case SA_dtLong:
		pdt = SQLPSLO;	// Long
		break;
	case SA_dtULong:
		pdt = SQLPULO;	// Unsigned long
		break;
	case SA_dtDouble:
		pdt = SQLPDOU;	// Double
		break;
	case SA_dtNumeric:
		// there is no need to use SQLBase internal number
		// because maximum precision in SQLBase is 15
		// and can be handled by C double
		pdt = SQLPDOU;	// Double
		break;
	case SA_dtDateTime:
		pdt = SQLPDAT;	// Internal datetime
		break;
	case SA_dtString:
		pdt = SQLPBUF;	// buffer
		break;
	case SA_dtBytes:
		pdt = SQLPBUF;	// buffer
		break;
	case SA_dtLongBinary:
	case SA_dtBLob:
		pdt = SQLPLBI;
		break;
	case SA_dtLongChar:
	case SA_dtCLob:
		pdt = SQLPLON;
		break;
	default:
		pdt = 0;
		assert(false);
	}
	
	return pdt;
}

/*virtual */
void IsbCursor::Open()
{
	assert(m_handles.m_cur != 0);	// should be opened by inherited classes

	if(m_pCommand->Connection()->AutoCommit() != SA_AutoCommitUnknown)
		OnConnectionAutoCommitChanged();

	// check for Cursor-Context Preservation
	SAString s = m_pCommand->Option(_TSA("SQLPPCX"));
	if(s.CompareNoCase(_TSA("on")) == 0)
	{
		SQLTDPV preserve = 1;
		IsbConnection::Check(g_sb6API.sqlset(m_handles.m_cur, SQLPPCX, (SQLTDAP)&preserve, 0));
	}
	else if(s.CompareNoCase(_TSA("off")) == 0)
	{
		SQLTDPV preserve = 0;
		IsbConnection::Check(g_sb6API.sqlset(m_handles.m_cur, SQLPPCX, (SQLTDAP)&preserve, 0));
	}

	if( isSetScrollable() ) // start restriction and result set modes
	{
			IsbConnection::Check(g_sb6API.sqlsrs(m_handles.m_cur));
			m_bScrollableResultSetMode = true;
			// turn off restriction mode 
			g_sb6API.sqlspr(m_handles.m_cur);

	}
	else 
		m_bScrollableResultSetMode = false;
}

/*virtual */
bool IsbCursor::IsOpened()
{
	return m_handles.m_cur != 0;
}

/*virtual */
void IsbCursor::Close()
{
	assert(m_handles.m_cur);
	IsbConnection::Check(g_sb6API.sqldis(m_handles.m_cur));
	m_handles.m_cur = 0;

	m_bFieldsDescribed = false;
}

/*virtual */
void IsbCursor::Destroy()
{
	assert(m_handles.m_cur);
	g_sb6API.sqldis(m_handles.m_cur);
	m_handles.m_cur = 0;

	m_bFieldsDescribed = false;
}

/*virtual */
void IsbCursor::Reset()
{
	m_handles.m_cur = 0;

	m_bResultSetCanBe = false;
	m_bOutputParamsRowPrefetched = false;
	m_bScrollableResultSetMode = false;
	m_eStoredObject = IsbCursor_StoredUnknown;

	if(m_pFieldDescr)
	{
		::free(m_pFieldDescr);
		m_pFieldDescr = NULL;
	}

	m_bFieldsDescribed = false;
}

//////////////////////////////////////////////////////////////////////
// Isb6Cursor Class
//////////////////////////////////////////////////////////////////////

class Isb6Cursor : public IsbCursor
{
public:
	Isb6Cursor(
		Isb6Connection *pIsb6Connection,
		SACommand *pCommand);
	virtual ~Isb6Cursor();

	virtual void Open();
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Isb6Cursor::Isb6Cursor(
	Isb6Connection *pIsb6Connection,
	SACommand *pCommand) :
	IsbCursor(pIsb6Connection, pCommand)
{
}

/*virtual */
Isb6Cursor::~Isb6Cursor()
{
}

/*virtual */
void Isb6Cursor::Open()
{
	assert(m_handles.m_cur == 0);
	IsbConnection::Check(g_sb6API.sqlcnc(
		&m_handles.m_cur, (SQLTDAP)((Isb6Connection*)m_pISAConnection)->m_sConnectString.GetMultiByteChars(), 0));

	IsbCursor::Open();
}

//////////////////////////////////////////////////////////////////////
// Isb7Cursor Class
//////////////////////////////////////////////////////////////////////

class Isb7Cursor : public IsbCursor
{
public:
	Isb7Cursor(
		Isb7Connection *pIsb7Connection,
		SACommand *pCommand);
	virtual ~Isb7Cursor();

	virtual void Open();
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Isb7Cursor::Isb7Cursor(
	Isb7Connection *pIsb7Connection,
	SACommand *pCommand) :
	IsbCursor(pIsb7Connection, pCommand)
{
}

/*virtual */
Isb7Cursor::~Isb7Cursor()
{
}

/*virtual */
void Isb7Cursor::Open()
{
	assert(m_handles.m_cur == 0);
	IsbConnection::Check(g_sb7API.sqlopc(
		&m_handles.m_cur, ((Isb7Connection*)m_pISAConnection)->m_hCon, 0));

	IsbCursor::Open();
}

/*virtual */
ISACursor *Isb6Connection::NewCursor(SACommand *m_pCommand)
{
	return new Isb6Cursor(this, m_pCommand);
}

/*virtual */
ISACursor *Isb7Connection::NewCursor(SACommand *m_pCommand)
{
	return new Isb7Cursor(this, m_pCommand);
}

/*virtual */
void IsbCursor::Prepare(
	const SAString &sStmt,
	SACommandType_t eCmdType,
	int /*nPlaceHolderCount*/,
	saPlaceHolder**	/*ppPlaceHolders*/)
{
	switch(eCmdType)
	{
	case SA_CmdSQLStmt:
	case SA_CmdSQLStmtRaw:
		SA_TRACE_CMD(sStmt);
		IsbConnection::Check(g_sb6API.sqlcom(m_handles.m_cur, (SQLTDAP)sStmt.GetMultiByteChars(), 0));
		break;
	case SA_CmdStoredProc:
		SA_TRACE_CMD(sStmt);
		IsbConnection::Check(g_sb6API.sqlret(m_handles.m_cur, (SQLTDAP)sStmt.GetMultiByteChars(), 0));
		break;
	default:
		assert(false);
	}

	m_bNeedToRetriveBeforeRebind = false;
	SAString s = m_pCommand->Option(_TSA("StoredObject"));
	if(s.CompareNoCase(_TSA("Procedure")) == 0)
		m_eStoredObject = IsbCursor_StoredProcedure;
	else if(s.CompareNoCase(_TSA("Command")) == 0)
		m_eStoredObject = IsbCursor_StoredCommand;
	else
		m_eStoredObject = IsbCursor_StoredUnknown;
	m_bFieldsDescribed = false;
}

/*virtual */
void IsbCursor::UnExecute()
{
	m_bResultSetCanBe = false;
	m_bOutputParamsRowPrefetched = false;
	
	if( IsOpened() )
	{
		g_sb6API.sqlcrs(m_handles.m_cur, (SQLNPTR), 0);
		m_bScrollableResultSetMode = false;
		m_nCurRow = -1;
	}
}

void IsbCursor::BindBlob(int nPos, SAParam &Param)
{
	switch(m_pCommand->CommandType())
	{
	case SA_CmdSQLStmtRaw:
	case SA_CmdSQLStmt:
		IsbConnection::Check(g_sb6API.sqlbld(
			m_handles.m_cur,
			(SQLTBNP)Param.Name().GetMultiByteChars(),
			0));
		break;
	case SA_CmdStoredProc:
		IsbConnection::Check(g_sb6API.sqlbln(
			m_handles.m_cur,
			(SQLTBNN)nPos));
		break;
	default:
		assert(false);
	}

	try
	{
		size_t nActualWrite;
		SAPieceType_t ePieceType = SA_FirstPiece;
		void *pBuf;
		while( (nActualWrite = Param.InvokeWriter(
			ePieceType,
			IsbConnection::MaxLongPiece, pBuf)) != 0 )
		{
			IsbConnection::Check(g_sb6API.sqlwlo(
				m_handles.m_cur,
				(SQLTDAP)pBuf,
				(SQLTDAL)nActualWrite));

			if( ePieceType == SA_LastPiece )
				break;
		}
	}
	catch(SAException &)
	{
		// if user aborted long operation we should (try to) close it
		g_sb6API.sqlelo(m_handles.m_cur);
		throw;
	}

	// close long operation
	IsbConnection::Check(g_sb6API.sqlelo(m_handles.m_cur));
}

void IsbCursor::Bind(
		int/* nPlaceHolderCount*/,
		saPlaceHolder ** /*ppPlaceHolders*/)
{
	// SQLBase has a bug when you execute prepared
	// SP with long varchar binds several times.
	// First execute is OK, but second
	// (and further) times it binds garbage into
	// long varchar parameters instead of supplied values.
	// This code is needed to avoid it. Retrieving SP again helps.
	if(m_bNeedToRetriveBeforeRebind)
	{
		assert(m_pCommand->CommandType() == SA_CmdStoredProc);
		IsbConnection::Check(g_sb6API.sqlret(m_handles.m_cur, (SQLTDAP)m_pCommand->CommandText().GetMultiByteChars(), 0));
		m_bNeedToRetriveBeforeRebind = false;
	}

	// we should bind all params, not place holders in SQLBase
	AllocBindBuffer(sizeof(SQLTFSC), sizeof(SQLTCDL));
	void *pBuf = m_pParamBuffer;

	SQLTSLC nReceive = 0;
	int nParams = m_pCommand->ParamCount();
	for(int i = 0; i < nParams; ++i)
	{
		SAParam &Param = m_pCommand->ParamByIndex(i);

		void *pNull;
		void *pSize;
		size_t nDataBufSize;
		void *pValue;
		IncParamBuffer(pBuf, pNull, pSize, nDataBufSize, pValue);

		if(Param.ParamDirType() == SA_ParamInput
			|| Param.ParamDirType() == SA_ParamInputOutput)
		{
			SQLTDAP dap = (SQLTDAP)pValue; /* Bind data buffer */
			SQLTDAL dal = (SQLTDAL)InputBufferSize(Param); /* Bind data length */
			SQLTNUL nli = SQLTNUL(Param.isNull()? -1 : 0); /* Null indicator */

			SADataType_t eDataType = Param.DataType();
			// some type should be set for input (even if Unknown, to bind NULL)
			SQLTPDT pdt = CnvtStdToNative(eDataType == SA_dtUnknown? SA_dtString : eDataType);
			bool bLong = false;

			if(!Param.isNull())
			{
				switch(eDataType)
				{
				case SA_dtUnknown:
					throw SAException(SA_Library_Error, SA_Library_Error_UnknownParameterType, -1, IDS_UNKNOWN_PARAMETER_TYPE, (const SAChar*)Param.Name());
				case SA_dtBool:
					// treat boolean bind in SQLBase as short bind (SQLPSSH)
					assert(dal == sizeof(short));
					*(short*)dap = (short)Param.asBool();
					break;
				case SA_dtShort:
					assert(dal == sizeof(short));
					*(short*)dap = Param.asShort();
					break;
				case SA_dtUShort:
					assert(dal == sizeof(unsigned short));
					*(short*)dap = Param.asUShort();
					break;
				case SA_dtLong:
					assert(dal == sizeof(long));
					*(long*)dap = Param.asLong();
					break;
				case SA_dtULong:
					assert(dal == sizeof(unsigned long));
					*(unsigned long*)dap = Param.asULong();
					break;
				case SA_dtDouble:
					assert(dal == sizeof(double));
					*(double*)dap = Param.asDouble();
					break;
				case SA_dtNumeric:
					assert(dal == sizeof(double));
					IsbConnection::CnvtNumericToInternal(
						Param.asNumeric(),
						*(double*)dap);
					break;
				case SA_dtDateTime:
					assert(dal == sizeof(SqlBaseDate_t));
					IsbConnection::CnvtDateTimeToInternal(
						Param.asDateTime(),
						*(SqlBaseDate_t*)dap,
						dal);
					assert(dal <= sizeof(SqlBaseDate_t));
					break;
				case SA_dtString:
					assert((size_t)dal == Param.asString().GetMultiByteCharsLength());
					memcpy(dap, Param.asString().GetMultiByteChars(), dal);
					break;
				case SA_dtBytes:
					assert((size_t)dal == Param.asBytes().GetBinaryLength());
					memcpy(dap, (const void*)Param.asBytes(), dal);
					break;
				case SA_dtLongBinary:
				case SA_dtBLob:
					assert(dal == 0);
					bLong = true;
					break;
				case SA_dtLongChar:
				case SA_dtCLob:
					assert(dal == 0);
					bLong = true;
					break;
				default:
					assert(false);
				}
			}

			if(bLong)
				BindBlob(i+1, Param);
			else
			{
				if(m_pCommand->CommandType() == SA_CmdStoredProc && m_eStoredObject != IsbCursor_StoredCommand)
				{
					// always bind stored proc by number
					// or SQLBase returns "invalid bind variable"
					IsbConnection::Check(g_sb6API.sqlbnu(
						m_handles.m_cur,
						SQLTBNN(i+1),
						dap, dal, 0, pdt, nli));
				}
				else
				{
					IsbConnection::Check(g_sb6API.sqlbna(
						m_handles.m_cur,
						(SQLTBNP)Param.Name().GetMultiByteChars(), 0,
						dap, dal, 0, pdt, nli));
				}
			}

			// bind outpout parameters
			if(Param.ParamDirType() == SA_ParamInputOutput)
			{
				nReceive++;

				pdt = CnvtStdToNative(Param.ParamType());
				SQLTPDL pdl = (SQLTPDL)OutputBufferSize(
					Param.ParamType(), Param.ParamSize());
				if(isLongOrLob(Param.ParamType()))
				{
					assert(pdl == sizeof(LongContext_t));
					((LongContext_t*)pValue)->slc = nReceive;
					pdl = 0;
				}
				IsbConnection::Check(g_sb6API.sqlssb(
					m_handles.m_cur, nReceive,
					pdt, (SQLTDAP)pValue, pdl,
					0, (SQLTCDL PTR)pSize, (SQLTFSC PTR)pNull));
			}
		}
		else
			assert(false);
	}
}

/*virtual */
void IsbCursor::Execute(
	int nPlaceHolderCount,
	saPlaceHolder **ppPlaceHolders)
{
	if( m_pCommand->ParamCount() > 0 )
		Bind(nPlaceHolderCount, ppPlaceHolders);

	// if fields descriptions were not requested till that time
	// then it is the last chance to do it
	// fields descriptions will not be available after execute
	int cFields = 0;
	int nOutputs = 0;
	if( SQLTSEL == getStmtType() )
		cFields = m_pCommand->FieldCount();	// this will describe fields if not yet

	IsbConnection::Check(g_sb6API.sqlexe(m_handles.m_cur));

	m_bResultSetCanBe = true;

	if( m_bScrollableResultSetMode )
		m_nCurRow = 0;
	else if( SA_CmdStoredProc == m_pCommand->CommandType() )
	{
		int i;
		// SQLBase has a bug when you execute prepared
		// SP with long varchar bindings several times. 
		// First execute is OK, but second
		// (and further) time it binds garbage into 
		// the long varchar parameters instead of supplied values.
		// This code is needed to avoid it. Retrieving SP again helps.
		// Also check for output parameters while looping
		for(i = 0; i < nPlaceHolderCount; ++i)
		{
			if(isLongOrLob(ppPlaceHolders[i]->getParam()->DataType()))
				m_bNeedToRetriveBeforeRebind = true;
			if(isOutputParam(*ppPlaceHolders[i]->getParam()))
				++nOutputs;
		}

		// if there are output params
		// SQLBase will return result set of parameters rows
		// through SAField objects
		// but first row of output params we will also
		// copy into corresponding SAParam objects
		if(nOutputs)
		{
			assert(cFields == nOutputs);
			m_bOutputParamsRowPrefetched = m_pCommand->FetchNext();
			if(m_bOutputParamsRowPrefetched)
			{
				for(int iField = 1; iField <= cFields; ++iField)
				{
					SAField &f = (*m_pCommand)[iField];
					SAParam &p = m_pCommand->Param(f.Name());

					p.setAsValueRead() = f;
				}
			}
		}
	}
}

/*virtual */
void IsbCursor::Cancel()
{
	IsbConnection::Check(g_sb6API.sqlcan(m_handles.m_cur));
}

SQLTCTY IsbCursor::getStmtType()
{
	SQLTCTY cty;
	IsbConnection::Check(g_sb6API.sqlcty(m_handles.m_cur, &cty));

	return cty;
}

/*virtual */
bool IsbCursor::ResultSetExists()
{
	if(!m_bResultSetCanBe)
		return false;

	switch(getStmtType())
	{
	case SQLTSEL:
		return true;
	case SQLTINS:
		return false;
	case SQLTUPD:
		return false;
	case SQLTDEL:
		return false;
	case SQLTCTB:
	case SQLTCIN:
	case SQLTDIN:
	case SQLTDTB:
	case SQLTACO:
	case SQLTDCO:
	case SQLTRTB:
	case SQLTRCO:
	case SQLTMCO:
	case SQLTGRP:
	case SQLTGRD:
	case SQLTGRC:
		return false;
	case SQLTCMT:
	case SQLTRBK:
		return false;
	}

	return false;
}

void IsbCursor::InternalDescribeFields()
{
	assert(m_bFieldsDescribed == false);

	IsbConnection::Check(g_sb6API.sqlnsi(m_handles.m_cur, &m_nsi));
	sa_realloc((void**)&m_pFieldDescr, m_nsi*sizeof(struct FieldDescr));
	for(SQLTSLC nField = 0; nField < m_nsi; nField++)
	{
		IsbConnection::Check(g_sb6API.sqldsc(
			m_handles.m_cur,
			(SQLTSLC)(nField+1),
			&m_pFieldDescr[nField].edt,
			&m_pFieldDescr[nField].edl,
			(SQLTCHP)m_pFieldDescr[nField].ch,
			&m_pFieldDescr[nField].chl,
			&m_pFieldDescr[nField].pre,
			&m_pFieldDescr[nField].sca));

		// all we need from sqldes is database data type
		IsbConnection::Check(g_sb6API.sqldes(
			m_handles.m_cur,
			(SQLTSLC)(nField+1),
			&m_pFieldDescr[nField].ddt,
			&m_pFieldDescr[nField].ddl,
			(SQLTCHP)m_pFieldDescr[nField].ch,
			&m_pFieldDescr[nField].chl,
			&m_pFieldDescr[nField].pre,
			&m_pFieldDescr[nField].sca));

		// all we need from gdidef is nullable ind
		gdidef def;
		def.gdicol = (SQLTSLC)(nField+1);
		IsbConnection::Check(g_sb6API.sqlgdi(m_handles.m_cur, &def));

		// analyze lower byte only
		m_pFieldDescr[nField].bRequired = (char)def.gdinul == 0;
	}

	m_bFieldsDescribed = true;
}

/*virtual */
void IsbCursor::DescribeFields(
	DescribeFields_cb_t fn)
{
	if(!m_bFieldsDescribed)
		InternalDescribeFields();

	for(int iField = 0; iField < (int)m_nsi; ++iField)
	{
		(m_pCommand->*fn)(
			SAString(m_pFieldDescr[iField].ch, m_pFieldDescr[iField].chl),
			CnvtNativeToStd(
				m_pFieldDescr[iField].edt,
				m_pFieldDescr[iField].pre,
				m_pFieldDescr[iField].sca,
				m_pFieldDescr[iField].ddt),
			m_pFieldDescr[iField].edt,
			m_pFieldDescr[iField].edl,
			m_pFieldDescr[iField].pre,
			m_pFieldDescr[iField].sca,
			m_pFieldDescr[iField].bRequired,
            (int)m_nsi);
	}
}

/*virtual */
long IsbCursor::GetRowsAffected()
{
	assert(m_handles.m_cur != 0);
	
	SQLTROW row = -1;

	if( m_bScrollableResultSetMode )
		IsbConnection::Check(g_sb6API.sqlnrr(m_handles.m_cur, &row));
	else
		IsbConnection::Check(g_sb6API.sqlrow(m_handles.m_cur, &row));

	return row;
}

/*virtual */
size_t IsbCursor::OutputBufferSize(
	SADataType_t eDataType,
	size_t nDataSize) const
{
	switch(eDataType)
	{
	case SA_dtNumeric:
		// there is no need to use SQLBase internal number
		// because maximum precision in SQLBase is 15
		// and can be handled by C double
		return sizeof(double);
	case SA_dtDateTime:
		return sizeof(SqlBaseDate_t);
	case SA_dtLongBinary:
	case SA_dtLongChar:
		return sizeof(LongContext_t);
    case SA_dtUnknown: // we have to ignore unknown data types
        return 0;
	default:
		break;
	}
	
	return ISACursor::OutputBufferSize(eDataType, nDataSize);
}

/*virtual */
bool IsbCursor::ConvertIndicator(
	int nPos,	// 1-based
	int/* nNotConverted*/,
	SAValueRead &vr,
	ValueType_t eValueType,
	void * /*pInd*/, size_t nIndSize,
	void *pSize, size_t nSizeSize,
	size_t &nRealSize,
	int/* nBulkReadingBufPos*/) const
{
	assert(nIndSize == sizeof(SQLTFSC));
	if(nIndSize != sizeof(SQLTFSC))
		return false;	// not converted
	assert(nSizeSize == sizeof(SQLTCDL));
	if(nSizeSize != sizeof(SQLTCDL))
		return false;	// not converted

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

	bool bLong = isLongOrLob(eDataType);

	if(bLong)
	{
		SQLTLSI nLongSize;
		IsbConnection::Check(g_sb6API.sqlgls(
			m_handles.m_cur, (SQLTSLC)nPos, &nLongSize));

		*vr.m_pbNull = nLongSize == 0;
		if(!vr.isNull())
			nRealSize = nLongSize;
		return true;	// converted
	}

	// indicator for null return incorect values for versions > 6
	// so use size to determine nulls
    *vr.m_pbNull = eDataType == SA_dtUnknown || *(SQLTCDL*)pSize == 0;
	if(!vr.isNull())
		nRealSize = *(SQLTCDL*)pSize;
	return true;	// converted
}


// default implementation (ISACursor::OutputBindingTypeIsParamType)
// returns false
// But in SQLBase C/API there is a possibility to bind in/out separately
// and we use this
/*virtual */
bool IsbCursor::OutputBindingTypeIsParamType()
{
	return true;
}

/*virtual */
void IsbCursor::SetFieldBuffer(
	int nCol,	// 1-based
	void *pInd,
	size_t/* nIndSize*/,
	void *pSize,
	size_t/* nSizeSize*/,
	void *pValue,
	size_t nValueSize)
{
	SAField &Field = m_pCommand->Field(nCol);

    if (Field.FieldType() == SA_dtUnknown) // ignore unknown field type
        return;

	SQLTPDT pdt = CnvtStdToNative(Field.FieldType());
	SQLTPDL pdl = (SQLTPDL)nValueSize;
	switch(Field.FieldType())
	{
		case SA_dtLongBinary:
		case SA_dtCLob:
			pdl = 0;
			assert(nValueSize == sizeof(LongContext_t));
			((LongContext_t*)pValue)->slc = (SQLTSLC)nCol;
			break;
		case SA_dtLongChar:
		case SA_dtBLob:
			pdl = 0;
			assert(nValueSize == sizeof(LongContext_t));
			((LongContext_t*)pValue)->slc = (SQLTSLC)nCol;
			break;
		default:
			break;
	}

	IsbConnection::Check(g_sb6API.sqlssb(
		m_handles.m_cur, (SQLTSLC)nCol,
		pdt, (SQLTDAP)pValue, pdl,
		0, (SQLTCDL PTR)pSize, (SQLTFSC PTR)pInd));
}

/*virtual */
void IsbCursor::SetSelectBuffers()
{
	// use default helpers
	AllocSelectBuffer(sizeof(SQLTFSC), sizeof(SQLTCDL));
}

/*virtual */
bool IsbCursor::FetchNext()
{
	if(m_bOutputParamsRowPrefetched)
	{
		m_bOutputParamsRowPrefetched = false;
		return true;
	}

	SQLTRCD rc = g_sb6API.sqlfet(m_handles.m_cur);
	if(rc != 1)
	{
		IsbConnection::Check(rc);

		++m_nCurRow;

		// use default helpers
		ConvertSelectBufferToFields(0);
	}
	else if( ! isSetScrollable() )
		m_bResultSetCanBe = false;

	return rc != 1;
}

bool IsbCursor::FetchRow(SQLTROW nRow)
{
	IsbConnection::Check(g_sb6API.sqlprs(m_handles.m_cur, nRow));

	m_nCurRow = nRow;

	SQLTRCD rc = g_sb6API.sqlfet(m_handles.m_cur);
	if(rc != 1)
	{
		IsbConnection::Check(rc);

		// use default helpers
		ConvertSelectBufferToFields(0);
	}
	else
		m_bResultSetCanBe = false;

	return rc != 1;
}

/*virtual */
bool IsbCursor::FetchPrior()
{
	if( m_nCurRow > 0 )
		return FetchRow(m_nCurRow-1);

	return false;
}

/*virtual */
bool IsbCursor::FetchFirst()
{
	return FetchRow(0);
}

/*virtual */
bool IsbCursor::FetchLast()
{
	return FetchRow(GetRowsAffected()-1);
}

/*virtual */
bool IsbCursor::FetchPos(int offset, bool Relative/* = false*/)
{
    SQLTROW nRow;

    if (Relative)
        nRow = m_nCurRow + offset;
    else
        nRow = offset >= 0 ? offset : (GetRowsAffected() + offset);

    if (nRow >= 0 && nRow < GetRowsAffected())
        return FetchRow(nRow);

    return false;
}

/*virtual */
void IsbCursor::ReadLongOrLOB(
	ValueType_t /* eValueType*/,
	SAValueRead &vr,
	void *pValue,
	size_t/* nBufSize*/,
	saLongOrLobReader_t fnReader,
	size_t nReaderWantedPieceSize,
	void *pAddlData)
{
	int nPos = ((LongContext_t*)pValue)->slc;

	// get long size
	SQLTLSI nLongSize;
	IsbConnection::Check(g_sb6API.sqlgls(
		m_handles.m_cur, (SQLTSLC)nPos, &nLongSize));

	unsigned char* pBuf;
	size_t nPieceSize = vr.PrepareReader(
		nLongSize,
		IsbConnection::MaxLongPiece,
		pBuf,
		fnReader,
		nReaderWantedPieceSize,
		pAddlData);
	assert(nPieceSize <= IsbConnection::MaxLongPiece);

	try
	{
		SAPieceType_t ePieceType = SA_FirstPiece;
		size_t nTotalRead = 0;
		do
		{
			nPieceSize =
				sa_min(nPieceSize, nLongSize - nTotalRead);
			SQLTDAL actual_read;

			IsbConnection::Check(g_sb6API.sqlrlo(
				m_handles.m_cur,
				(SQLTSLC)nPos,
				(SQLTDAP)pBuf,
				(SQLTDAL)nPieceSize,
				&actual_read));

			nTotalRead += actual_read;
			assert(nTotalRead <= nLongSize);

            if( nTotalRead == nLongSize )
            {
                if(ePieceType == SA_NextPiece)
                    ePieceType = SA_LastPiece;
                else    // the whole Long was read in one piece
                {
                    assert(ePieceType == SA_FirstPiece);
                    ePieceType = SA_OnePiece;
                }
            }
			vr.InvokeReader(ePieceType, pBuf, actual_read);

			if(ePieceType == SA_FirstPiece)
				ePieceType = SA_NextPiece;
		}
		while(nTotalRead < nLongSize);
	}
	catch(SAUserException &)
	{
		// if user aborted long operation we should (try to) close it
		g_sb6API.sqlelo(m_handles.m_cur);
		throw;
	}

	// close long operation
	IsbConnection::Check(g_sb6API.sqlelo(m_handles.m_cur));
}

static bool ParseKeyword(
	const char *&p, const char *sKeyword,
	bool bAllowOneColon)
{
	if(::_strnicmp(p, sKeyword, strlen(sKeyword)) != 0)
		return false;
	const char *p1 = p+strlen(sKeyword);
	if(isspace(*p1) != 0 || *p1 == 0 || (bAllowOneColon && *p1 == ':'))
	{
		if(*p1 == ':')
		{
			++p1;
			bAllowOneColon = false;
		}

		// remove space, but no new lines
		while(isspace(*p1) && *p1 != 10 && *p1 != 13)
			++p1;
		if(*p1 == ':')
		{
			if(bAllowOneColon)
			{
				++p1;
				//bAllowOneColon = false;
				while(isspace(*p1) && *p1 != 10 && *p1 != 13)
					++p1;
			}
			else
				return false;
		}

		p = p1;
		return true;
	}
	return false;
}

static bool GetIdentation(const char *&p, int &nIdentation)
{
	do
	{
		nIdentation = 0;

		// skip to end of line
		while(*p != 10 && *p != 13)
			++p;
		while(*p == 10 || *p == 13)
			++p;
		// start counting identation					
		while(isspace(*p))
		{
			nIdentation = (*p == 13 || *p == 10)? 0:nIdentation+1;
			++p;
		}
	}
	while(*p == '!'); // comment, restart counting

	return isalpha(*p) != 0;
}

static bool ParseName(const char *&p, SAString& sName)
{
	assert(sName.IsEmpty());
	while(*p != 10 && *p != 13)
	{
		char s[2] = "\0";
		s[0] = *p;
		sName += s;
		++p;
	}

	sName.TrimRight();
	return true;
}

/*virtual */
void IsbCursor::DescribeParamSP()
{
	// prepare if not prepared yet
	m_pCommand->Prepare();

	SQLTNBV nbv;	// Number of Bind Variables
	SQLTNSI nsi;	// Number of Select Items
	IsbConnection::Check(g_sb6API.sqlnbv(m_handles.m_cur, &nbv));
	IsbConnection::Check(g_sb6API.sqlnsi(m_handles.m_cur, &nsi));

	if(nbv + nsi == 0)	// no params
		return;

	// parse stored proc definition if possible
	bool bParsingOK = false;
	while(nbv)	// parsing is only needed when input params are present
	{
		try
		{
			SAString sSQL;
			sSQL.Format(
_TSA("Select TEXT, TYPE from syssql.syscommands \
where (TYPE = 'P' or TYPE = 'C') and \
(@UPPER(CREATOR || '.' || NAME) = @UPPER(USER || '.%") SA_FMT_STR _TSA("') \
or @UPPER(CREATOR || '.' || NAME) = @UPPER('%") SA_FMT_STR _TSA("'))"),
				(const SAChar*)m_pCommand->CommandText(), (const SAChar*)m_pCommand->CommandText());
			SACommand cmd(m_pCommand->Connection(), sSQL);
			cmd.Execute();
			bool bFound = cmd.FetchNext();
			if(!bFound)
				break;

			SAString sText = cmd[1].asString();
			assert(!cmd.FetchNext());	// should have only one row

			if(cmd[2].asString() == _TSA("C"))
			{
				m_eStoredObject = IsbCursor_StoredCommand;
				m_pCommand->ParseInputMarkers(sText, NULL);				
				bParsingOK = true;
			}
			else
			{
				m_eStoredObject = IsbCursor_StoredProcedure;
				const char *p = sText.GetMultiByteChars();
				while(*p)
				{
					// skip space before 'Procedure' keyword
					while(isspace(*p))
						++p;
					if(!ParseKeyword(p, "PROCEDURE", true))
						break;
					// now find Parameters section
					// first find out params identation
					int nParametersIdentation = 0;
					if(!GetIdentation(p, nParametersIdentation))
						break;
					
					int nIdentation = nParametersIdentation;
					bool bBadVar = false;
					while(!bParsingOK && !bBadVar && *p)
					{
						if(nIdentation == nParametersIdentation)
						{
							if(ParseKeyword(p, "PARAMETERS", false))
							{
								// find out vars identation
								int nVarIdentation = 0;
								if(!GetIdentation(p, nVarIdentation))
									break;
								
								if(nVarIdentation <= nParametersIdentation)
									break;	// there were no variables in parameters section
								
								nIdentation = nVarIdentation;
								while(!bBadVar && *p && nIdentation == nVarIdentation)
								{
									// [Receive] type[:] varName
									bool bReceive = false;
									if(ParseKeyword(p, "RECEIVE", false))
									{
										while(isspace(*p))
										{
											if(*p == 10 || *p == 13)
											{
												bBadVar = true;
												break;
											}
											++p;
										}
										if(!isalpha(*p))
										{
											bBadVar = true;
											break;
										}

										bReceive = true;
									}
									
									// check var type
									SADataType_t eVarType = SA_dtUnknown;
									int nNativeType = -1;
									SAString sName;
									int nVarSize = 0;
									if(ParseKeyword(p, "BOOLEAN", true))
									{
										nVarSize = 8;	// like native API reports
										eVarType = CnvtNativeToStd(
											SQLEFLO, 0, 0, SQLDNUM);
										nNativeType = SQLEFLO;
										
										if(!ParseName(p, sName))
											bBadVar = true;
									}
									else if(ParseKeyword(p, "DATE/TIME", true))
									{
										nVarSize = 10;	//	like native API
										eVarType = CnvtNativeToStd(
											SQLETMS, 0, 0, SQLDDAT);
										nNativeType = SQLETMS;
										if(!ParseName(p, sName))
											bBadVar = true;
									}
									else if(ParseKeyword(p, "NUMBER", true))
									{
										nVarSize = 8;	// like native API reports
										eVarType = CnvtNativeToStd(
											SQLEFLO, 0, 0, SQLDNUM);
										nNativeType = SQLEFLO;
										if(!ParseName(p, sName))
											bBadVar = true;
									}
									else if(ParseKeyword(p, "STRING", true))
									{
										nVarSize = 254;	// like native API reports
										eVarType = CnvtNativeToStd(
											SQLECHR, 0, 0, SQLDCHR);
										nNativeType = SQLECHR;
										if(!ParseName(p, sName))
											bBadVar = true;
									}
									else if(ParseKeyword(p, "LONG", false))
									{
										if(!ParseKeyword(p, "STRING", true))
											bBadVar = true;
										else
										{
											nVarSize = 0;	// not like native API reports (it reports char, we need long varchar)
											eVarType = CnvtNativeToStd(
												SQLELON, 0, 0, SQLDLON);
											nNativeType = SQLELON;
											if(!ParseName(p, sName))
												bBadVar = true;
										}
									}
									else if(ParseKeyword(p, "WINDOW", false))
									{
										if(!ParseKeyword(p, "HANDLE", true))
											bBadVar = true;
										else
										{
											nVarSize = 8;	// like native API reports
											eVarType = CnvtNativeToStd(
												SQLEFLO, 0, 0, SQLDNUM);
											nNativeType = SQLEFLO;
											if(!ParseName(p, sName))
												bBadVar = true;
										}
									}
									else
									{
										bBadVar = true;
										break;
									}

									m_pCommand->CreateParam(
										sName,
										eVarType,
										nNativeType,
										nVarSize,
										-1,
										-1,
										bReceive? SA_ParamInputOutput : SA_ParamInput);

									if(!GetIdentation(p, nIdentation))
										break;
								}
					
								if(!bBadVar)
									bParsingOK = true;
							}
						}

						if(!GetIdentation(p, nIdentation))
							break;
					}
					
					break;
				}
			}
		}
		catch(SAException &)
		{
		}

		break;
	}

	if(!bParsingOK)
	{
		m_pCommand->DestroyParams();

		// create input params
		for(int i = 0; i < nbv-nsi; ++i)
		{
			SAString s;
			s.Format(_TSA("%d"), i+1);
			m_pCommand->CreateParam(s, SA_dtUnknown, -1, 0, -1, -1, SA_ParamInput);
		}

		// create output params, they are described like fields in SQLBase
		if(!m_bFieldsDescribed)
			InternalDescribeFields();

		for(int iField = 0; iField < (int)m_nsi; ++iField)
		{
			m_pCommand->CreateParam(
				SAString(m_pFieldDescr[iField].ch, m_pFieldDescr[iField].chl),
				CnvtNativeToStd(
					m_pFieldDescr[iField].edt,
					m_pFieldDescr[iField].pre,
					m_pFieldDescr[iField].sca,
					m_pFieldDescr[iField].ddt),
				m_pFieldDescr[iField].edt,	// native type
				m_pFieldDescr[iField].edl,
				m_pFieldDescr[iField].pre,
				m_pFieldDescr[iField].sca,
				SA_ParamInputOutput);
		}
	}
	else	// check and correct in/out params with values reported by API
	{
		assert(nbv == m_pCommand->ParamCount());
		if(m_eStoredObject != IsbCursor_StoredCommand)
		{
			int nOutputs = 0;
			for(int i = 0; i < m_pCommand->ParamCount(); ++i)
			{
				SAParam &Param = m_pCommand->ParamByIndex(i);
				if(Param.ParamDirType() == SA_ParamInputOutput)
				{
					// Output params are described like fields in SQLBase
					if(!m_bFieldsDescribed)
						InternalDescribeFields();

					assert(SAString(m_pFieldDescr[nOutputs].ch, m_pFieldDescr[nOutputs].chl) == Param.Name());
					Param.setParamNativeType(m_pFieldDescr[nOutputs].edt);
					Param.setParamSize(m_pFieldDescr[nOutputs].edl);

					++nOutputs;
				}
			}
			assert(nOutputs == nsi);
		}
	}
}

/*virtual */
saAPI *Isb6Connection::NativeAPI() const
{
	return &g_sb6API;
}

/*virtual */
saAPI *Isb7Connection::NativeAPI() const
{
	return &g_sb7API;
}

/*virtual */
saConnectionHandles *Isb6Connection::NativeHandles()
{
	m_handles.m_cur = m_cur;
	return &m_handles;
}

/*virtual */
saConnectionHandles *Isb7Connection::NativeHandles()
{
	m_handles.m_hCon = m_hCon;
	m_handles.m_cur = m_cur;
	return &m_handles;
}

/*virtual */
saCommandHandles *IsbCursor::NativeHandles()
{
	return &m_handles;
}

/*virtual */
void IsbConnection::setIsolationLevel(
	SAIsolationLevel_t eIsolationLevel)
{
	SQLTILV isolation;
	switch(eIsolationLevel)
	{
	case SA_ReadUncommitted:
		isolation = (SQLTILV)SQLILRL;
		break;
	case SA_ReadCommitted:
		isolation = (SQLTILV)SQLILCS;
		break;
	case SA_RepeatableRead:
		isolation = (SQLTILV)SQLILRR;
		break;
	case SA_Serializable:
		isolation = (SQLTILV)SQLILRO;
		break;
	default:
		assert(false);
		return;
	}

	Check(g_sb6API.sqlsil(m_cur, isolation));
}

/*virtual */
void IsbConnection::setAutoCommit(
	SAAutoCommit_t eAutoCommit)
{
	SQLTDPV commit;
	switch(eAutoCommit)
	{
	case SA_AutoCommitOn:
		commit = 1;
		break;
	case SA_AutoCommitOff:
	default:
		commit = 0;
		break;
	}

	Check(g_sb6API.sqlset(m_cur, SQLPAUT, (SQLTDAP)&commit, 0));
}

/*virtual */
void IsbCursor::OnConnectionAutoCommitChanged()
{
	SQLTDPV commit;
	switch(m_pCommand->Connection()->AutoCommit())
	{
	case SA_AutoCommitOn:
		commit = 1;
		break;
	case SA_AutoCommitOff:
		commit = 0;
		break;
	default:
		return;
	}

	IsbConnection::Check(g_sb6API.sqlset(m_handles.m_cur, SQLPAUT, (SQLTDAP)&commit, 0));
}

/*virtual */
void IsbConnection::CnvtInternalToNumeric(
	SANumeric &numeric,
	const void *pInternal,
	int nInternalSize)
{
	assert((size_t)nInternalSize == sizeof(double));
	if( (size_t)nInternalSize != sizeof(double) )
		return;

	numeric = *(double*)pInternal;
}

/*static */
void IsbConnection::CnvtNumericToInternal(
	const SANumeric &numeric,
	double &Internal)
{
	Internal = numeric;
}
