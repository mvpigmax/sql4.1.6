//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <oraAPI.h>
#include <ora7API.h>
#include "oraClient.h"

typedef unsigned char OraDATE_t[7];
typedef unsigned char OraVARNUM_t[22];

#ifndef SA_DefaultMaxLong
#define SA_DefaultMaxLong	0x7FFFFFFC
#endif

//////////////////////////////////////////////////////////////////////
// ora8ConnectionPool Class
//////////////////////////////////////////////////////////////////////

ora8ConnectionPool::ora8ConnectionPool()
{
    poolhp = NULL;
    poolName = NULL;
    poolNameLen = 0;
}

//////////////////////////////////////////////////////////////////////
// ora8ConnectionPools Class
//////////////////////////////////////////////////////////////////////

ora8ConnectionPools::ora8ConnectionPools()
{
    m_nCount = 0;
    m_pPools = NULL;
}

ora8ConnectionPools::ora8ConnectionPools(const ora8ConnectionPools &)
{

}

// disable assignment operator
ora8ConnectionPools & ora8ConnectionPools::operator = (const ora8ConnectionPools &)
{
    return *this;
}

ora8ConnectionPool& ora8ConnectionPools::operator[](int poolIndex)
{
    if (m_nCount <= poolIndex)
    {
        sa_realloc((void**)&m_pPools, sizeof(ora8ConnectionPool*) * poolIndex);
        for (int i = m_nCount; i < poolIndex; ++i)
            m_pPools[i] = new ora8ConnectionPool();
        m_nCount = poolIndex;
    }

    return *m_pPools[poolIndex - 1];
}

void ora8ConnectionPools::Close(ora8API& api)
{
    for (int i = 0; i < m_nCount; ++i)
    {
        if (NULL != m_pPools[i]->poolhp)
        {
            api.OCIConnectionPoolDestroy(m_pPools[i]->poolhp, NULL, OCI_DEFAULT);
            api.OCIHandleFree(m_pPools[i]->poolhp, OCI_HTYPE_CPOOL);
        }

        m_pPools[i]->poolhp = NULL;
        m_pPools[i]->poolName = NULL;
        m_pPools[i]->poolNameLen = 0;
    }

    free(m_pPools);
    m_nCount = 0;
    m_pPools = NULL;
}

//////////////////////////////////////////////////////////////////////
// IoraConnection Class
//////////////////////////////////////////////////////////////////////

class IoraConnection : public ISAConnection
{
    friend class Iora7Cursor;
    friend class Iora8Cursor;

protected:
    static void CnvtNumericToInternal(const SANumeric &numeric,
        OraVARNUM_t &Internal);
    static void CnvtInternalToDateTime(SADateTime &date_time,
        const OraDATE_t *pInternal);
    static void CnvtDateTimeToInternal(const SADateTime &date_time,
        OraDATE_t *pInternal);

    enum MaxLong
    {
        MaxLongPiece = (unsigned int)SA_DefaultMaxLong
    };

    virtual ~IoraConnection();

    SAIsolationLevel_t m_eSwitchToIsolationLevelAfterCommit;
    void issueIsolationLevel(SAIsolationLevel_t eIsolationLevel);

public:
    IoraConnection(SAConnection *pSAConnection);

    virtual void setIsolationLevel(SAIsolationLevel_t eIsolationLevel);

    virtual void CnvtInternalToNumeric(SANumeric &numeric,
        const void *pInternal, int nInternalSize);

    virtual void CnvtInternalToDateTime(SADateTime &date_time,
        const void *pInternal, int nInternalSize);

    virtual void CnvtInternalToInterval(SAInterval &interval,
        const void *pInternal, int nInternalSize);

    virtual bool IsAlive() const;
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IoraConnection::IoraConnection(SAConnection *pSAConnection) :
ISAConnection(pSAConnection)
{
    m_eSwitchToIsolationLevelAfterCommit = SA_LevelUnknown;
}

IoraConnection::~IoraConnection()
{
}

/*virtual */
void IoraConnection::CnvtInternalToDateTime(SADateTime &date_time,
    const void *pInternal, int nInternalSize)
{
    assert((size_t)nInternalSize == sizeof(OraDATE_t));
    if ((size_t)nInternalSize != sizeof(OraDATE_t))
        return;
    CnvtInternalToDateTime(date_time, (const OraDATE_t*)pInternal);
}

/*static */
void IoraConnection::CnvtInternalToDateTime(SADateTime &date_time,
    const OraDATE_t *pInternal)
{
    date_time = SADateTime(
        ((*pInternal)[0] - 100) * 100 + ((*pInternal)[1] - 100),
        (*pInternal)[2], (*pInternal)[3], (*pInternal)[4] - 1,
        (*pInternal)[5] - 1, (*pInternal)[6] - 1);

    // no milli, micro or nano seconds in Oracle
}

/*virtual */
void IoraConnection::CnvtInternalToInterval(SAInterval & /*interval*/,
    const void * /*pInternal*/, int /*nInternalSize*/)
{
    assert(false);
}

/*static */
void IoraConnection::CnvtDateTimeToInternal(const SADateTime &date_time,
    OraDATE_t *pInternal)
{
    (*pInternal)[0] = (unsigned char)(((date_time.GetYear()) / 100) + 100); // century in excess-100 notation
    (*pInternal)[1] = (unsigned char)(((date_time.GetYear()) % 100) + 100); // year in excess-100 notation
    (*pInternal)[2] = (unsigned char)(date_time.GetMonth()); // month 1..12
    (*pInternal)[3] = (unsigned char)(date_time.GetDay()); // day 1..31
    (*pInternal)[4] = (unsigned char)(date_time.GetHour() + 1); // hour 1..24
    (*pInternal)[5] = (unsigned char)(date_time.GetMinute() + 1); // min 1..60
    (*pInternal)[6] = (unsigned char)(date_time.GetSecond() + 1); // sec 1..60

    // no milli, micro or nano seconds in Oracle
}

/*virtual */
bool IoraConnection::IsAlive() const
{
    try
    {
        SACommand cmd(m_pSAConnection, _TSA("select user from dual"));
        cmd.Execute();
    }
    catch (SAException&)
    {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
// Iora7Connection Class
//////////////////////////////////////////////////////////////////////

class Iora7Connection : public IoraConnection
{
    friend class Iora7Cursor;

    ora7ConnectionHandles m_handles;
    bool m_bConnected;

    void Check(sword return_code, Cda_Def* pCda_Def);

    enum MaxLong
    {
        MaxLongPortion = (unsigned int)SA_DefaultMaxLong
    };

protected:
    virtual ~Iora7Connection();

public:
    Iora7Connection(SAConnection *pSAConnection);

    virtual void InitializeClient();
    virtual void UnInitializeClient();

    virtual long GetClientVersion() const;
    virtual long GetServerVersion() const;
    virtual SAString GetServerVersionString() const;

    virtual bool IsConnected() const;
    virtual void Connect(const SAString &sDBString, const SAString &sUserID,
        const SAString &sPassword, saConnectionHandler_t fHandler);
    virtual void Disconnect();
    virtual void Destroy();
    virtual void Reset();

    virtual void setAutoCommit(SAAutoCommit_t eAutoCommit);
    virtual void Commit();
    virtual void Rollback();

    virtual saAPI *NativeAPI() const;
    virtual saConnectionHandles *NativeHandles();

    virtual ISACursor *NewCursor(SACommand *m_pCommand);

    virtual void CnvtInternalToCursor(SACommand *pCursor,
        const void *pInternal);
    void CnvtInternalToCursor(SACommand &Cursor, const Cda_Def *pInternal);
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Iora7Connection::Iora7Connection(SAConnection *pSAConnection) :
IoraConnection(pSAConnection)
{
    Reset();
}

Iora7Connection::~Iora7Connection()
{
}

/*virtual */
void Iora7Connection::InitializeClient()
{
    ::AddORA7Support();
}

/*virtual */
void Iora7Connection::UnInitializeClient()
{
    if (SAGlobals::UnloadAPI())
        ::ReleaseORA7Support();
}

/*virtual */
long Iora7Connection::GetClientVersion() const
{
    return g_nORA7DLLVersionLoaded;
}

/*virtual */
long Iora7Connection::GetServerVersion() const
{
    SACommand cmd(
        m_pSAConnection,
        _TSA("select VERSION from PRODUCT_COMPONENT_VERSION where PRODUCT like '%Oracle%'"));
    cmd.Execute();
    cmd.FetchNext();
    SAString sVersion = cmd.Field(1).asString();
    cmd.Close();

    SAChar *sPoint;
    short nMajor = (short)sa_strtol(sVersion, &sPoint, 10);
    assert(*sPoint == _TSA('.'));
    sPoint++;
    short nMinor = (short)sa_strtol(sPoint, &sPoint, 10);
    return nMinor + (nMajor << 16);
}

/*virtual */
SAString Iora7Connection::GetServerVersionString() const
{
    SACommand cmd(
        m_pSAConnection,
        _TSA("select PRODUCT || ' Release ' || VERSION || ' - ' || STATUS from PRODUCT_COMPONENT_VERSION where PRODUCT like '%Oracle%'"));
    cmd.Execute();
    cmd.FetchNext();
    SAString sVersion = cmd.Field(1).asString();
    cmd.Close();
    return sVersion;
}

void Iora7Connection::Check(sword result_code, Cda_Def* pCda_Def)
{
    if (!result_code)
        return;

    char sMsg[512];
    g_ora7API.oerhms(&m_handles.m_lda, (sb2)result_code, (text*)sMsg,
        (sword) sizeof(sMsg));

    ub2 peo = pCda_Def ? pCda_Def->peo : m_handles.m_lda.peo;
    ub2 rc = pCda_Def ? pCda_Def->rc : m_handles.m_lda.rc;
    throw SAException(SA_DBMS_API_Error, (int)rc, (int)peo, SAString(sMsg));
}

/*virtual */
bool Iora7Connection::IsConnected() const
{
    return m_bConnected;
}

/*virtual */
void Iora7Connection::Connect(const SAString &sDBString,
    const SAString &sUserID, const SAString &sPassword,
    saConnectionHandler_t fHandler)
{
    assert(!m_bConnected);

    if (NULL != fHandler)
        fHandler(*m_pSAConnection, SA_PreConnectHandler);

    Check(
        g_ora7API.olog(&m_handles.m_lda, m_handles.m_hda,
        (text*)sUserID.GetMultiByteChars(), -1,
        (text*)sPassword.GetMultiByteChars(), -1,
        (text*)sDBString.GetMultiByteChars(), -1, OCI_LM_DEF),
        NULL);
    m_bConnected = true;

    if (NULL != fHandler)
        fHandler(*m_pSAConnection, SA_PostConnectHandler);
}

/*virtual */
void Iora7Connection::Disconnect()
{
    assert(m_bConnected);
    Check(g_ora7API.ologof(&m_handles.m_lda), NULL);
    m_bConnected = false;
}

/*virtual */
void Iora7Connection::Destroy()
{
    assert(m_bConnected);
    g_ora7API.ologof(&m_handles.m_lda);
    m_bConnected = false;
}

/*virtual */
void Iora7Connection::Reset()
{
    m_bConnected = false;
}

/*virtual */
void Iora7Connection::Commit()
{
    SAIsolationLevel_t eIsolationLevel;
    if (m_eSwitchToIsolationLevelAfterCommit != SA_LevelUnknown)
    {
        eIsolationLevel = m_eSwitchToIsolationLevelAfterCommit;
        m_eSwitchToIsolationLevelAfterCommit = SA_LevelUnknown;
    }
    else
        eIsolationLevel = m_pSAConnection->IsolationLevel();

    Check(g_ora7API.ocom(&m_handles.m_lda), NULL);

    if (eIsolationLevel != SA_LevelUnknown)
        issueIsolationLevel(eIsolationLevel);
}

/*virtual */
void Iora7Connection::Rollback()
{
    Check(g_ora7API.orol(&m_handles.m_lda), NULL);

    SAIsolationLevel_t eIsolationLevel = m_pSAConnection->IsolationLevel();
    if (eIsolationLevel != SA_LevelUnknown)
        issueIsolationLevel(eIsolationLevel);
}

//////////////////////////////////////////////////////////////////////
// Iora8Connection Class
//////////////////////////////////////////////////////////////////////

typedef enum SAOra8TempLobSupport
{
    SA_OCI8TempLobSupport_Unknown,
    SA_OCI8TempLobSupport_True,
    SA_OCI8TempLobSupport_False,
    _SA_OCI8TempLobSupport_Reserverd = (int)(((unsigned int)(-1)) / 2)
} SAOra8TempLobSupport_t;

class Iora8Connection : public IoraConnection
{
    friend class Iora8Cursor;

    ora8ConnectionHandles m_handles;

    bool m_bUseTimeStamp;
    bool m_bUseConnectionPool;
#ifdef SA_UNICODE
    bool m_bUseUCS2;
#endif
    
    sb4 mb_cur_max;

    static void Check(sword status, dvoid *hndlp, ub4 type, OCIStmt *pOCIStmt = NULL);

    SAOra8TempLobSupport_t m_eOra8TempLobSupport;
    bool IsTemporaryLobSupported();

    void CnvtInternalToCursor(SACommand &Cursor, const OCIStmt *pInternal);
    void PostInit();

    static ub2 GetCharsetId(const SAString& sCharset);

protected:
    virtual ~Iora8Connection();

public:
    Iora8Connection(SAConnection *pSAConnection);

    virtual void InitializeClient();
    virtual void UnInitializeClient();

    virtual long GetClientVersion() const;
    virtual long GetServerVersion() const;
    virtual SAString GetServerVersionString() const;

    virtual bool IsConnected() const;
    virtual void Connect(const SAString &sDBString, const SAString &sUserID,
        const SAString &sPassword, saConnectionHandler_t fHandler);
    virtual void Disconnect();
    virtual void Destroy();
    virtual void Reset();

    virtual void setAutoCommit(SAAutoCommit_t eAutoCommit);
    virtual void Commit();
    virtual void Rollback();

    virtual saAPI *NativeAPI() const;
    virtual saConnectionHandles *NativeHandles();

    virtual ISACursor *NewCursor(SACommand *m_pCommand);

    virtual void CnvtInternalToDateTime(SADateTime &date_time,
        const void *pInternal, int nInternalSize);
    void CnvtDateTimeToInternal(const SADateTime &date_time,
        OCIDateTime *pInternal);

    virtual void CnvtInternalToCursor(SACommand *pCursor,
        const void *pInternal);

public:
    void Attach(ora8ExternalConnection& extConnection);
    void Detach();
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Iora8Connection::Iora8Connection(SAConnection *pSAConnection) :
IoraConnection(pSAConnection)
{
    Reset();
}

Iora8Connection::~Iora8Connection()
{
}

/*virtual */
void Iora8Connection::InitializeClient()
{
    ::AddORA8Support(m_pSAConnection);

    m_bUseTimeStamp = false;
    m_bUseConnectionPool = false;
#ifdef SA_UNICODE
    m_bUseUCS2 = true;
#endif
    mb_cur_max = MB_CUR_MAX;
}

/*virtual */
void Iora8Connection::UnInitializeClient()
{
    if (SAGlobals::UnloadAPI())
        ::ReleaseORA8Support();

    m_bUseTimeStamp = false;
    m_bUseConnectionPool = false;
#ifdef SA_UNICODE
    m_bUseUCS2 = true;
#endif
    mb_cur_max = MB_CUR_MAX;
}

/*virtual */
long Iora8Connection::GetClientVersion() const
{
    return g_nORA8DLLVersionLoaded;
}

/*virtual */
long Iora8Connection::GetServerVersion() const
{
    return SAExtractVersionFromString(GetServerVersionString());
}

/*virtual */
SAString Iora8Connection::GetServerVersionString() const
{
    SAString sVersion;

    // From Oracle docs: first parameter: The service context handle or the server context handle.
    // But we use service context handle because:
    // 1) if server context handle is used
    // 2) if Oracle is in proccess of shutting down
    // 3) if we call Iora8Connection::GetServerVersionString()
    // 4) next call to Iora8Connection::Disconnect() will make access violation
    // With service context handle all works fine
#ifdef SA_UNICODE
    if( m_bUseUCS2 )
    {
        utext szVersion[1024];
        Check(g_ora8API.OCIServerVersion(
            m_handles.m_pOCISvcCtx, //m_handles.m_pOCIServer
            m_handles.m_pOCIError,
            (text*)szVersion, 1024,
            OCI_HTYPE_SVCCTX),//OCI_HTYPE_SERVER
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        sVersion.SetUTF16Chars(szVersion, SIZE_MAX);
    }
    else
#endif
    {
        char szVersion[1024];
        Check(
            g_ora8API.OCIServerVersion(
            m_handles.m_pOCISvcCtx, //m_handles.m_pOCIServer
            m_handles.m_pOCIError, (text*)szVersion, 1024,
            OCI_HTYPE_SVCCTX), //OCI_HTYPE_SERVER
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        sVersion = szVersion;
    }

    return sVersion;
}

// based on OCIError or OCIEnv
void Iora8Connection::Check(sword status, dvoid *hndlp, ub4 type,
    OCIStmt *pOCIStmt/* = NULL*/)
{
    sb4 errcode, main_errcode = 0;
#ifdef SA_UNICODE
    utext bufWide[512];
#endif
    char buf[512];
    SAString sErr, s;
    ub4 recordno = 1;
    sword ret;
    int nErrPos = -1;
    OCIError *pOCIError = NULL;
    if (type == OCI_HTYPE_ERROR)
        pOCIError = (OCIError *)hndlp;

    switch (status)
    {
    case OCI_SUCCESS:
        break;
    case OCI_SUCCESS_WITH_INFO:
        break;
    case OCI_NEED_DATA:
    case OCI_NO_DATA:
    case OCI_STILL_EXECUTING:
    case OCI_ERROR:
        // According to Oracle Docs we should loop while ret == OCI_SUCCESS,
        // but we can loop forever under some conditions if we will check 
        // for OCI_SUCCESS. Oracle bug?
        //		do
    {
        ret = g_ora8API.OCIErrorGet(hndlp, recordno, (text *)NULL, &errcode,
            (text*)buf, (ub4) sizeof(buf), type);

        if (ret == OCI_SUCCESS)
        {
            //				if(recordno == 1)
            main_errcode = errcode;
            //				if(recordno > 1)
            //					sErr += "\n";
#ifdef SA_UNICODE
            // ugly unicode hack
            if( strncmp(buf, "ORA-", 4) )
            {
                ret = g_ora8API.OCIErrorGet(hndlp, recordno, (text *)NULL, &errcode,
                    (text*)bufWide, (ub4) sizeof(bufWide), type);
                if( ret == OCI_SUCCESS )
                {
                    main_errcode = errcode;
                    s.SetUTF16Chars(bufWide, SIZE_MAX);
                    sErr += s;
                }
            }
            else
#endif
                sErr += buf;

            //				++recordno;
        }
        else
            assert(ret == OCI_NO_DATA);
    }
    //		while(ret == OCI_SUCCESS);

    if (pOCIStmt && pOCIError)
    {
        ub2 peo;
        ret = g_ora8API.OCIAttrGet(pOCIStmt, OCI_HTYPE_STMT, &peo, NULL,
            OCI_ATTR_PARSE_ERROR_OFFSET, pOCIError);
        if (ret == OCI_SUCCESS)
            nErrPos = (int)peo;
    }

    throw SAException(SA_DBMS_API_Error, main_errcode, nErrPos, sErr);

    case OCI_INVALID_HANDLE:
        throw SAException(SA_DBMS_API_Error, main_errcode, nErrPos,
            _TSA("OCI_INVALID_HANDLE"));

    default:
        throw SAException(SA_DBMS_API_Error, main_errcode, nErrPos,
            _TSA("Unknow OCI result status %d"), status);
    }
}

// tests if both client and server support Temporary Lobs
// test is very simple:
// we try to create it
bool Iora8Connection::IsTemporaryLobSupported()
{
    if (m_eOra8TempLobSupport == SA_OCI8TempLobSupport_Unknown)
    {
        // client test
        if (!g_ora8API.OCILobCreateTemporary)
            m_eOra8TempLobSupport = SA_OCI8TempLobSupport_False;
        else
        {
            // server test
            OCILobLocator *pLoc = NULL;
            try
            {
                Check(
                    g_ora8API.OCIDescriptorAlloc(m_handles.m_pOCIEnv,
                    (dvoid **)&pLoc, OCI_DTYPE_LOB, 0, NULL),
                    m_handles.m_pOCIEnv, OCI_HTYPE_ENV);

                Check(
                    g_ora8API.OCILobCreateTemporary(m_handles.m_pOCISvcCtx,
                    m_handles.m_pOCIError, pLoc, OCI_DEFAULT,
                    OCI_DEFAULT, OCI_TEMP_BLOB, OCI_ATTR_NOCACHE,
                    OCI_DURATION_SESSION), m_handles.m_pOCIError,
                    OCI_HTYPE_ERROR);

                m_eOra8TempLobSupport = SA_OCI8TempLobSupport_True;
            }
            catch (SAException &)
            {
                m_eOra8TempLobSupport = SA_OCI8TempLobSupport_False;
            }

            // clean up
            try
            {
                if (m_eOra8TempLobSupport == SA_OCI8TempLobSupport_True)
                    Check(
                    g_ora8API.OCILobFreeTemporary(
                    m_handles.m_pOCISvcCtx,
                    m_handles.m_pOCIError, pLoc),
                    m_handles.m_pOCIError, OCI_HTYPE_ERROR);
            }
            catch (SAException &)
            {
                assert(false);
            }

            if (pLoc)
                g_ora8API.OCIDescriptorFree(pLoc, OCI_DTYPE_LOB);
        }
    }

    assert(m_eOra8TempLobSupport != SA_OCI8TempLobSupport_Unknown);
    return m_eOra8TempLobSupport == SA_OCI8TempLobSupport_True;
}

ub2 Iora8Connection::GetCharsetId(const SAString& sCharset)
{
    ub2 charset = OCI_DEFAULT;
    if (sCharset.IsEmpty())
        return charset;

    static const struct
    {
        const SAChar *sOCI_ATTR_CHARSET_ID_Name;
        ub2 ubOCI_ATTR_CHARSET_ID_Code;
    } ids[] =
    {
        { _TSA("DefaultCharSet"), 0 },
        { _TSA("US7ASCII"), 1 },
        { _TSA("WE8DEC"), 2 },
        { _TSA("WE8HP"), 3 },
        { _TSA("US8PC437"), 4 },
        { _TSA("WE8EBCDIC37"), 5 },
        { _TSA("WE8EBCDIC500"), 6 },
        { _TSA("WE8EBCDIC1140"), 7 },
        { _TSA("WE8EBCDIC285"), 8 },
        { _TSA("WE8EBCDIC1146"), 9 },
        { _TSA("WE8PC850"), 10 },
        { _TSA("D7DEC"), 11 },
        { _TSA("F7DEC"), 12 },
        { _TSA("S7DEC"), 13 },
        { _TSA("E7DEC"), 14 },
        { _TSA("SF7ASCII"), 15 },
        { _TSA("NDK7DEC"), 16 },
        { _TSA("I7DEC"), 17 },
        { _TSA("NL7DEC"), 18 },
        { _TSA("CH7DEC"), 19 },
        { _TSA("YUG7ASCII"), 20 },
        { _TSA("SF7DEC"), 21 },
        { _TSA("TR7DEC"), 22 },
        { _TSA("IW7IS960"), 23 },
        { _TSA("IN8ISCII"), 25 },
        { _TSA("WE8EBCDIC1148"), 27 },
        { _TSA("WE8PC858"), 28 },
        { _TSA("WE8ISO8859P1"), 31 },
        { _TSA("EE8ISO8859P2"), 32 },
        { _TSA("SE8ISO8859P3"), 33 },
        { _TSA("NEE8ISO8859P4"), 34 },
        { _TSA("CL8ISO8859P5"), 35 },
        { _TSA("AR8ISO8859P6"), 36 },
        { _TSA("EL8ISO8859P7"), 37 },
        { _TSA("IW8ISO8859P8"), 38 },
        { _TSA("WE8ISO8859P9"), 39 },
        { _TSA("NE8ISO8859P10"), 40 },
        { _TSA("TH8TISASCII"), 41 },
        { _TSA("TH8TISEBCDIC"), 42 },
        { _TSA("BN8BSCII"), 43 },
        { _TSA("VN8VN3"), 44 },
        { _TSA("VN8MSWIN1258"), 45 },
        { _TSA("WE8ISO8859P15"), 46 },
        { _TSA("WE8NEXTSTEP"), 50 },
        { _TSA("AR8ASMO708PLUS"), 61 },
        { _TSA("AR8EBCDICX"), 70 },
        { _TSA("AR8XBASIC"), 72 },
        { _TSA("EL8DEC"), 81 },
        { _TSA("TR8DEC"), 82 },
        { _TSA("WE8EBCDIC37C"), 90 },
        { _TSA("WE8EBCDIC500C"), 91 },
        { _TSA("IW8EBCDIC424"), 92 },
        { _TSA("TR8EBCDIC1026"), 93 },
        { _TSA("WE8EBCDIC871"), 94 },
        { _TSA("WE8EBCDIC284"), 95 },
        { _TSA("WE8EBCDIC1047"), 96 },
        { _TSA("WE8EBCDIC1140C"), 97 },
        { _TSA("WE8EBCDIC1145"), 98 },
        { _TSA("WE8EBCDIC1148C"), 99 },
        { _TSA("EEC8EUROASCI"), 110 },
        { _TSA("EEC8EUROPA3"), 113 },
        { _TSA("LA8PASSPORT"), 114 },
        { _TSA("BG8PC437S"), 140 },
        { _TSA("EE8PC852"), 150 },
        { _TSA("RU8PC866"), 152 },
        { _TSA("RU8BESTA"), 153 },
        { _TSA("IW8PC1507"), 154 },
        { _TSA("RU8PC855"), 155 },
        { _TSA("TR8PC857"), 156 },
        { _TSA("CL8MACCYRILLIC"), 158 },
        { _TSA("CL8MACCYRILLICS"), 159 },
        { _TSA("WE8PC860"), 160 },
        { _TSA("IS8PC861"), 161 },
        { _TSA("EE8MACCES"), 162 },
        { _TSA("EE8MACCROATIANS"), 163 },
        { _TSA("TR8MACTURKISHS"), 164 },
        { _TSA("IS8MACICELANDICS"), 165 },
        { _TSA("EL8MACGREEKS"), 166 },
        { _TSA("IW8MACHEBREWS"), 167 },
        { _TSA("EE8MSWIN1250"), 170 },
        { _TSA("CL8MSWIN1251"), 171 },
        { _TSA("ET8MSWIN923"), 172 },
        { _TSA("BG8MSWIN"), 173 },
        { _TSA("EL8MSWIN1253"), 174 },
        { _TSA("IW8MSWIN1255"), 175 },
        { _TSA("LT8MSWIN921"), 176 },
        { _TSA("TR8MSWIN1254"), 177 },
        { _TSA("WE8MSWIN1252"), 178 },
        { _TSA("BLT8MSWIN1257"), 179 },
        { _TSA("D8EBCDIC273"), 180 },
        { _TSA("I8EBCDIC280"), 181 },
        { _TSA("DK8EBCDIC277"), 182 },
        { _TSA("S8EBCDIC278"), 183 },
        { _TSA("EE8EBCDIC870"), 184 },
        { _TSA("CL8EBCDIC1025"), 185 },
        { _TSA("F8EBCDIC297"), 186 },
        { _TSA("IW8EBCDIC1086"), 187 },
        { _TSA("CL8EBCDIC1025X"), 188 },
        { _TSA("D8EBCDIC1141"), 189 },
        { _TSA("N8PC865"), 190 },
        { _TSA("BLT8CP921"), 191 },
        { _TSA("LV8PC1117"), 192 },
        { _TSA("LV8PC8LR"), 193 },
        { _TSA("BLT8EBCDIC1112"), 194 },
        { _TSA("LV8RST104090"), 195 },
        { _TSA("CL8KOI8R"), 196 },
        { _TSA("BLT8PC775"), 197 },
        { _TSA("DK8EBCDIC1142"), 198 },
        { _TSA("S8EBCDIC1143"), 199 },
        { _TSA("I8EBCDIC1144"), 200 },
        { _TSA("F7SIEMENS9780X"), 201 },
        { _TSA("E7SIEMENS9780X"), 202 },
        { _TSA("S7SIEMENS9780X"), 203 },
        { _TSA("DK7SIEMENS9780X"), 204 },
        { _TSA("N7SIEMENS9780X"), 205 },
        { _TSA("I7SIEMENS9780X"), 206 },
        { _TSA("D7SIEMENS9780X"), 207 },
        { _TSA("F8EBCDIC1147"), 208 },
        { _TSA("WE8GCOS7"), 210 },
        { _TSA("EL8GCOS7"), 211 },
        { _TSA("US8BS2000"), 221 },
        { _TSA("D8BS2000"), 222 },
        { _TSA("F8BS2000"), 223 },
        { _TSA("E8BS2000"), 224 },
        { _TSA("DK8BS2000"), 225 },
        { _TSA("S8BS2000"), 226 },
        { _TSA("WE8BS2000"), 231 },
        { _TSA("CL8BS2000"), 235 },
        { _TSA("WE8BS2000L5"), 239 },
        { _TSA("WE8DG"), 241 },
        { _TSA("WE8NCR4970"), 251 },
        { _TSA("WE8ROMAN8"), 261 },
        { _TSA("EE8MACCE"), 262 },
        { _TSA("EE8MACCROATIAN"), 263 },
        { _TSA("TR8MACTURKISH"), 264 },
        { _TSA("IS8MACICELANDIC"), 265 },
        { _TSA("EL8MACGREEK"), 266 },
        { _TSA("IW8MACHEBREW"), 267 },
        { _TSA("US8ICL"), 277 },
        { _TSA("WE8ICL"), 278 },
        { _TSA("WE8ISOICLUK"), 279 },
        { _TSA("EE8EBCDIC870C"), 301 },
        { _TSA("EL8EBCDIC875S"), 311 },
        { _TSA("TR8EBCDIC1026S"), 312 },
        { _TSA("BLT8EBCDIC1112S"), 314 },
        { _TSA("IW8EBCDIC424S"), 315 },
        { _TSA("EE8EBCDIC870S"), 316 },
        { _TSA("CL8EBCDIC1025S"), 317 },
        { _TSA("TH8TISEBCDICS"), 319 },
        { _TSA("AR8EBCDIC420S"), 320 },
        { _TSA("CL8EBCDIC1025C"), 322 },
        { _TSA("WE8MACROMAN8"), 351 },
        { _TSA("WE8MACROMAN8S"), 352 },
        { _TSA("TH8MACTHAI"), 353 },
        { _TSA("TH8MACTHAIS"), 354 },
        { _TSA("HU8CWI2"), 368 },
        { _TSA("EL8PC437S"), 380 },
        { _TSA("EL8EBCDIC875"), 381 },
        { _TSA("EL8PC737"), 382 },
        { _TSA("LT8PC772"), 383 },
        { _TSA("LT8PC774"), 384 },
        { _TSA("EL8PC869"), 385 },
        { _TSA("EL8PC851"), 386 },
        { _TSA("CDN8PC863"), 390 },
        { _TSA("HU8ABMOD"), 401 },
        { _TSA("AR8ASMO8X"), 500 },
        { _TSA("AR8NAFITHA711T"), 504 },
        { _TSA("AR8SAKHR707T"), 505 },
        { _TSA("AR8MUSSAD768T"), 506 },
        { _TSA("AR8ADOS710T"), 507 },
        { _TSA("AR8ADOS720T"), 508 },
        { _TSA("AR8APTEC715T"), 509 },
        { _TSA("AR8NAFITHA721T"), 511 },
        { _TSA("AR8HPARABIC8T"), 514 },
        { _TSA("AR8NAFITHA711"), 554 },
        { _TSA("AR8SAKHR707"), 555 },
        { _TSA("AR8MUSSAD768"), 556 },
        { _TSA("AR8ADOS710"), 557 },
        { _TSA("AR8ADOS720"), 558 },
        { _TSA("AR8APTEC715"), 559 },
        { _TSA("AR8MSWIN1256"), 560 },
        { _TSA("AR8MSAWIN"), 560 },
        { _TSA("AR8NAFITHA721"), 561 },
        { _TSA("AR8SAKHR706"), 563 },
        { _TSA("AR8ARABICMAC"), 565 },
        { _TSA("AR8ARABICMACS"), 566 },
        { _TSA("AR8ARABICMACT"), 567 },
        { _TSA("LA8ISO6937"), 590 },
        { _TSA("WE8DECTST"), 798 },
        { _TSA("JA16VMS"), 829 },
        { _TSA("JA16EUC"), 830 },
        { _TSA("JA16EUCYEN"), 831 },
        { _TSA("JA16SJIS"), 832 },
        { _TSA("JA16DBCS"), 833 },
        { _TSA("JA16SJISYEN"), 834 },
        { _TSA("JA16EBCDIC930"), 835 },
        { _TSA("JA16MACSJIS"), 836 },
        { _TSA("KO16KSC5601"), 840 },
        { _TSA("KO16DBCS"), 842 },
        { _TSA("KO16KSCCS"), 845 },
        { _TSA("KO16MSWIN949"), 846 },
        { _TSA("ZHS16CGB231280"), 850 },
        { _TSA("ZHS16MACCGB231280"), 851 },
        { _TSA("ZHS16GBK"), 852 },
        { _TSA("ZHS16DBCS"), 853 },
        { _TSA("ZHT32EUC"), 860 },
        { _TSA("ZHT32SOPS"), 861 },
        { _TSA("ZHT16DBT"), 862 },
        { _TSA("ZHT32TRIS"), 863 },
        { _TSA("ZHT16DBCS"), 864 },
        { _TSA("ZHT16BIG5"), 865 },
        { _TSA("ZHT16CCDC"), 866 },
        { _TSA("ZHT16MSWIN950"), 867 },
        { _TSA("AL24UTFFSS"), 870 },
        { _TSA("UTF8"), 871 },
        { _TSA("UTFE"), 872 },
        { _TSA("AL32UTF8"), 873 },
        { _TSA("WE16DECTST2"), 994 },
        { _TSA("WE16DECTST"), 995 },
        { _TSA("KO16TSTSET"), 996 },
        { _TSA("JA16TSTSET2"), 997 },
        { _TSA("JA16TSTSET"), 998 },
        { _TSA("UTF16"), 1000 },
        { _TSA("US16TSTFIXED"), 1001 },
        { _TSA("JA16EUCFIXED"), 1830 },
        { _TSA("JA16SJISFIXED"), 1832 },
        { _TSA("JA16DBCSFIXED"), 1833 },
        { _TSA("KO16KSC5601FIXED"), 1840 },
        { _TSA("KO16DBCSFIXED"), 1842 },
        { _TSA("ZHS16CGB231280FIXED"), 1850 },
        { _TSA("ZHS16GBKFIXED"), 1852 },
        { _TSA("ZHS16DBCSFIXED"), 1853 },
        { _TSA("ZHT32EUCFIXED"), 1860 },
        { _TSA("ZHT32TRISFIXED"), 1863 },
        { _TSA("ZHT16DBCSFIXED"), 1864 },
        { _TSA("ZHT16BIG5FIXED"), 1865 },
        { _TSA("AL16UTF16"), 2000 },
        { _TSA("AL16UCS2 "), 2001 } };

    if (0 == sa_isdigit(((const SAChar*)sCharset)[0]))
    {
        // first try to match a string representation
        for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i)
        {
            if (0 == sCharset.CompareNoCase(ids[i].sOCI_ATTR_CHARSET_ID_Name))
            {
                charset = ids[i].ubOCI_ATTR_CHARSET_ID_Code;
                break;
            }
        }
    }
    else
        charset = (ub2)sa_toi((const SAChar*)sCharset);

    return charset;
}

/*virtual */
bool Iora8Connection::IsConnected() const
{
    return m_handles.m_pOCIEnv != NULL;
}

static SAMutex OCIEnvMutex;
/*virtual */
void Iora8Connection::Connect(const SAString &sDBString,
    const SAString &sUserID, const SAString &sPassword,
    saConnectionHandler_t fHandler)
{
    int poolIndex = getOptionValue(_TSA("UseConnectionPool"), 0);
    m_bUseConnectionPool = NULL != g_ora8API.OCIConnectionPoolCreate && poolIndex > 0;

    assert(m_handles.m_pOCIEnv == NULL);
    assert(m_handles.m_pOCIError == NULL);
    assert(m_handles.m_pOCISvcCtx == NULL);
    assert(m_handles.m_pOCIServer == NULL);
    assert(m_handles.m_pOCISession == NULL);

    short bServerAttach = 0;
    short bSessionBegin = 0;

    SAString s;

    try
    {
        // we have to mutex OCIEnvInit or OCIEnvCreate
        // or OCI can die
        if (g_ora8API.OCIEnvCreate == NULL) // use 8.0.x method of initialization
        {
            SACriticalSectionScope scope(&OCIEnvMutex);

            g_ora8API.OCIEnvInit(&m_handles.m_pOCIEnv, OCI_DEFAULT, 0,
                (dvoid**)0);

#ifdef SA_UNICODE
            m_bUseUCS2 = false;
#endif
        }
        else
        {
            SACriticalSectionScope scope(&OCIEnvMutex);
            if (!m_bUseConnectionPool || NULL == g_ora8API.envhp)
            {
                ub2 charset = OCI_DEFAULT;

#ifdef SA_UNICODE
                s = m_pSAConnection->Option(_TSA("UseUCS2"));
                if (NULL == g_ora8API.OCIEnvNlsCreate ||
                    0 == s.CompareNoCase(_TSA("No")) ||
                    0 == s.CompareNoCase(_TSA("False")) ||
                    0 == s.CompareNoCase(_TSA("0")))
                    m_bUseUCS2 = false;
                else
                    charset = OCI_UCS2ID;

                if (!m_bUseUCS2)
#endif
                {
                    s = m_pSAConnection->Option(_TSA("NLS_CHAR"));
                    charset = GetCharsetId(s);
                }

                g_ora8API.OCIEnvNlsCreate(&m_handles.m_pOCIEnv,
                    OCI_THREADED | OCI_OBJECT, NULL, NULL, NULL, NULL, 0,
                    (dvoid **)0, charset, charset);

                g_ora8API.envhp = m_handles.m_pOCIEnv;
            }
            else
                m_handles.m_pOCIEnv = g_ora8API.envhp;
        }
        assert(m_handles.m_pOCIEnv != NULL);

        Check(
            g_ora8API.OCIHandleAlloc(m_handles.m_pOCIEnv,
            (dvoid**)&m_handles.m_pOCIError, OCI_HTYPE_ERROR, 0,
            (dvoid**)0), m_handles.m_pOCIEnv, OCI_HTYPE_ENV);
        Check(
            g_ora8API.OCIHandleAlloc(m_handles.m_pOCIEnv,
            (dvoid**)&m_handles.m_pOCISvcCtx, OCI_HTYPE_SVCCTX, 0,
            (dvoid**)0), m_handles.m_pOCIEnv, OCI_HTYPE_ENV);
        Check(
            g_ora8API.OCIHandleAlloc(m_handles.m_pOCIEnv,
            (dvoid**)&m_handles.m_pOCIServer, OCI_HTYPE_SERVER, 0,
            (dvoid**)0), m_handles.m_pOCIEnv, OCI_HTYPE_ENV);
        Check(
            g_ora8API.OCIHandleAlloc(m_handles.m_pOCIEnv,
            (dvoid**)&m_handles.m_pOCISession, OCI_HTYPE_SESSION,
            0, (dvoid**)0), m_handles.m_pOCIEnv, OCI_HTYPE_ENV);

        if (m_bUseConnectionPool)
        {
            SACriticalSectionScope scope(&OCIEnvMutex);
            
            if (NULL == g_ora8API.connecionPools[poolIndex].poolhp)
            {
                Check(g_ora8API.OCIHandleAlloc((void *)m_handles.m_pOCIEnv,
                    (void **)&g_ora8API.connecionPools[poolIndex].poolhp, OCI_HTYPE_CPOOL,
                    (size_t)0, (dvoid **)0), m_handles.m_pOCIEnv, OCI_HTYPE_ENV);

                ub4 connMin = getOptionValue(_TSA("UseConnectionPool_MinConn"), 10);
                ub4 connMax = getOptionValue(_TSA("UseConnectionPool_MaxConn"), 100);
                ub4 connIncr = getOptionValue(_TSA("UseConnectionPool_ConnIncr"), 10);

#ifdef SA_UNICODE
                if (m_bUseUCS2)                
                    Check(g_ora8API.OCIConnectionPoolCreate(m_handles.m_pOCIEnv,
                        m_handles.m_pOCIError, g_ora8API.connecionPools[poolIndex].poolhp,
                        &g_ora8API.connecionPools[poolIndex].poolName, &g_ora8API.connecionPools[poolIndex].poolNameLen,
                        (const OraText*)sDBString.GetUTF16Chars(), (sb4)sDBString.GetUTF16CharsLength()*sizeof(utext),
                        connMin, connMax, connIncr,
                        (const OraText*)sUserID.GetUTF16Chars(), (sb4)sUserID.GetUTF16CharsLength()*sizeof(utext),
                        (const OraText*)sPassword.GetUTF16Chars(), (sb4)sPassword.GetUTF16CharsLength()*sizeof(utext),
                        OCI_DEFAULT), m_handles.m_pOCIError, OCI_HTYPE_ERROR);
                else
#endif
                    Check(g_ora8API.OCIConnectionPoolCreate(m_handles.m_pOCIEnv,
                    m_handles.m_pOCIError, g_ora8API.connecionPools[poolIndex].poolhp,
                    &g_ora8API.connecionPools[poolIndex].poolName, &g_ora8API.connecionPools[poolIndex].poolNameLen,
                    (const OraText*)sDBString.GetMultiByteChars(), (sb4)sDBString.GetLength(),
                    connMin, connMax, connIncr,
                    (const OraText*)sUserID.GetMultiByteChars(), (sb4)sUserID.GetLength(),
                    (const OraText*)sPassword.GetMultiByteChars(), (sb4)sPassword.GetLength(),
                    OCI_DEFAULT), m_handles.m_pOCIError, OCI_HTYPE_ERROR);
            }
        }

        if (m_bUseConnectionPool)
            Check(g_ora8API.OCIServerAttach(m_handles.m_pOCIServer, m_handles.m_pOCIError,
            (CONST text *)g_ora8API.connecionPools[poolIndex].poolName, g_ora8API.connecionPools[poolIndex].poolNameLen,
            OCI_CPOOL), m_handles.m_pOCIError, OCI_HTYPE_ERROR);

#ifdef SA_UNICODE
        if( m_bUseUCS2 )
        {
            Check(g_ora8API.OCIAttrSet(m_handles.m_pOCISession, OCI_HTYPE_SESSION,
                (dvoid*)sUserID.GetUTF16Chars(), (ub4)sUserID.GetUTF16CharsLength()*sizeof(utext),
                OCI_ATTR_USERNAME, m_handles.m_pOCIError),
                m_handles.m_pOCIError, OCI_HTYPE_ERROR);
            Check(g_ora8API.OCIAttrSet(m_handles.m_pOCISession, OCI_HTYPE_SESSION,
                (dvoid*)sPassword.GetUTF16Chars(), (ub4)sPassword.GetUTF16CharsLength()*sizeof(utext),
                OCI_ATTR_PASSWORD, m_handles.m_pOCIError),
                m_handles.m_pOCIError, OCI_HTYPE_ERROR);
            if (!m_bUseConnectionPool)
                Check(g_ora8API.OCIServerAttach(m_handles.m_pOCIServer, m_handles.m_pOCIError,
                    (CONST text *)sDBString.GetUTF16Chars(), (sb4)sDBString.GetUTF16CharsLength()*sizeof(utext),
                    OCI_DEFAULT), m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        }
        else
#endif
        {
            Check(
                g_ora8API.OCIAttrSet(m_handles.m_pOCISession,
                OCI_HTYPE_SESSION,
                (dvoid*)sUserID.GetMultiByteChars(),
                (ub4)sUserID.GetLength(), OCI_ATTR_USERNAME,
                m_handles.m_pOCIError), m_handles.m_pOCIError,
                OCI_HTYPE_ERROR);
            Check(
                g_ora8API.OCIAttrSet(m_handles.m_pOCISession,
                OCI_HTYPE_SESSION,
                (dvoid*)sPassword.GetMultiByteChars(),
                (ub4)sPassword.GetLength(), OCI_ATTR_PASSWORD,
                m_handles.m_pOCIError), m_handles.m_pOCIError,
                OCI_HTYPE_ERROR);
            if (!m_bUseConnectionPool)
                Check(
                    g_ora8API.OCIServerAttach(m_handles.m_pOCIServer,
                    m_handles.m_pOCIError,
                    (CONST text *) sDBString.GetMultiByteChars(),
                    (sb4)sDBString.GetLength(), OCI_DEFAULT),
                    m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        }

        bServerAttach = 1;

        Check(
            g_ora8API.OCIAttrSet(m_handles.m_pOCISvcCtx, OCI_HTYPE_SVCCTX,
            (dvoid*)m_handles.m_pOCIServer, (ub4)0,
            OCI_ATTR_SERVER, m_handles.m_pOCIError),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);

        ub4 mode = OCI_DEFAULT;
        s = m_pSAConnection->Option(_TSA("ConnectAs"));
        if (0 == s.CompareNoCase(_TSA("SYSDBA")))
            mode = OCI_SYSDBA;
        else if (0 == s.CompareNoCase(_TSA("SYSOPER")))
            mode = OCI_SYSOPER;

        if (NULL != fHandler)
            fHandler(*m_pSAConnection, SA_PreConnectHandler);

        if (sUserID.IsEmpty()) // identified externally
            Check(
            g_ora8API.OCISessionBegin(m_handles.m_pOCISvcCtx,
            m_handles.m_pOCIError, m_handles.m_pOCISession,
            OCI_CRED_EXT, mode), m_handles.m_pOCIError,
            OCI_HTYPE_ERROR);
        else
            Check(
            g_ora8API.OCISessionBegin(m_handles.m_pOCISvcCtx,
            m_handles.m_pOCIError, m_handles.m_pOCISession,
            OCI_CRED_RDBMS, mode), m_handles.m_pOCIError,
            OCI_HTYPE_ERROR);

        bSessionBegin = 1;

        Check(
            g_ora8API.OCIAttrSet(m_handles.m_pOCISvcCtx, OCI_HTYPE_SVCCTX,
            (dvoid*)m_handles.m_pOCISession, (ub4)0,
            OCI_ATTR_SESSION, m_handles.m_pOCIError),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);

        PostInit();

        if (NULL != fHandler)
            fHandler(*m_pSAConnection, SA_PostConnectHandler);
    }
    catch (SAException &)
    {
        // clean up
        if (bSessionBegin)
            g_ora8API.OCISessionEnd(m_handles.m_pOCISvcCtx,
            m_handles.m_pOCIError, m_handles.m_pOCISession,
            OCI_DEFAULT);
        if (bServerAttach)
            g_ora8API.OCIServerDetach(m_handles.m_pOCIServer,
            m_handles.m_pOCIError, OCI_DEFAULT);
        if (m_handles.m_pOCIError != NULL)
            g_ora8API.OCIHandleFree(m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        if (m_handles.m_pOCIEnv != NULL)
        {
            // frees all child handles too            
            if ( !m_bUseConnectionPool)
                g_ora8API.OCIHandleFree(m_handles.m_pOCIEnv, OCI_HTYPE_ENV);
            m_handles.m_pOCIEnv = NULL;
            m_handles.m_pOCIError = NULL;
            m_handles.m_pOCISvcCtx = NULL;
            m_handles.m_pOCIServer = NULL;
            m_handles.m_pOCISession = NULL;
        }

        throw;
    }
}

void Iora8Connection::PostInit()
{
    SAString s;

    m_bUseTimeStamp = NULL != g_ora8API.OCIDateTimeConstruct && getOptionValue(_TSA("UseTimeStamp"), true);

#ifdef SA_UNICODE
    if (!m_bUseUCS2)
#endif
    {
        s = m_pSAConnection->Option(_TSA("OCI_NLS_CHARSET_MAXBYTESZ"));
        if (!s.IsEmpty() && sa_toi(s) > 0)
            mb_cur_max = sa_toi(s);
        else if (NULL != g_ora8API.OCINlsNumericInfoGet)
            g_ora8API.OCINlsNumericInfoGet(m_handles.m_pOCIEnv,
            m_handles.m_pOCIError, &mb_cur_max,
            OCI_NLS_CHARSET_MAXBYTESZ);
    }

    s = m_pSAConnection->Option(SACON_OPTION_APPNAME);
    if (!s.IsEmpty())
        Check(
        g_ora8API.OCIAttrSet((dvoid *)m_handles.m_pOCISession,
        OCI_HTYPE_SESSION, (dvoid *)(const SAChar*)s,
        (ub4)s.GetLength(), OCI_ATTR_CLIENT_IDENTIFIER,
        m_handles.m_pOCIError), m_handles.m_pOCIError,
        OCI_HTYPE_ERROR);

    s = m_pSAConnection->Option(_TSA("OCI_ATTR_RECEIVE_TIMEOUT"));
    if (!s.IsEmpty())
    {
        ub4 timeout = sa_toi(s);
        Check(g_ora8API.OCIAttrSet((dvoid *)m_handles.m_pOCIServer,
            OCI_HTYPE_SERVER, (dvoid *)&timeout,
            (ub4)0, OCI_ATTR_RECEIVE_TIMEOUT,
            m_handles.m_pOCIError), m_handles.m_pOCIError,
            OCI_HTYPE_ERROR);
    }

    s = m_pSAConnection->Option(_TSA("OCI_ATTR_SEND_TIMEOUT"));
    if (!s.IsEmpty())
    {
        ub4 timeout = sa_toi(s);
        Check(g_ora8API.OCIAttrSet((dvoid *)m_handles.m_pOCIServer,
            OCI_HTYPE_SERVER, (dvoid *)&timeout,
            (ub4)0, OCI_ATTR_SEND_TIMEOUT,
            m_handles.m_pOCIError), m_handles.m_pOCIError,
            OCI_HTYPE_ERROR);
    }
}

void Iora8Connection::Attach(ora8ExternalConnection& extConnection)
{
    assert(m_handles.m_pOCIEnv == NULL);
    assert(m_handles.m_pOCIError == NULL);
    assert(m_handles.m_pOCISvcCtx == NULL);
    assert(m_handles.m_pOCIServer == NULL);
    assert(m_handles.m_pOCISession == NULL);

    try
    {
        m_handles.m_pOCIEnv = extConnection.m_pOCIEnv;
        assert(m_handles.m_pOCIEnv != NULL);

        Check(g_ora8API.OCIHandleAlloc(m_handles.m_pOCIEnv,
            (dvoid**)&m_handles.m_pOCIError, OCI_HTYPE_ERROR, 0,
            (dvoid**)0), m_handles.m_pOCIEnv, OCI_HTYPE_ENV);

        m_handles.m_pOCISvcCtx = extConnection.m_pOCISvcCtx;

        Check(g_ora8API.OCIAttrGet(m_handles.m_pOCISvcCtx, OCI_HTYPE_SVCCTX, &m_handles.m_pOCIServer, NULL,
            OCI_ATTR_SERVER, m_handles.m_pOCIError), m_handles.m_pOCIError, OCI_HTYPE_ERROR);

        Check(g_ora8API.OCIAttrGet(m_handles.m_pOCISvcCtx, OCI_HTYPE_SVCCTX, &m_handles.m_pOCISession, NULL,
            OCI_ATTR_SESSION, m_handles.m_pOCIError), m_handles.m_pOCIError, OCI_HTYPE_ERROR);


#ifdef SA_UNICODE
        ub2 envUtf16 = 0;
        ub4 csid = 0;;
        Check(g_ora8API.OCIAttrGet(m_handles.m_pOCIEnv, OCI_HTYPE_ENV, &envUtf16, NULL,
            OCI_ATTR_ENV_UTF16, m_handles.m_pOCIError), m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        Check(g_ora8API.OCIAttrGet(m_handles.m_pOCIEnv, OCI_HTYPE_ENV, &csid, NULL,
            OCI_ATTR_ENV_CHARSET_ID, m_handles.m_pOCIError), m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        m_bUseUCS2 = envUtf16 > 0 || csid == OCI_UTF16ID;
#endif

        PostInit();
    }
    catch (SAException&)
    {
        if (m_handles.m_pOCIError != NULL)
            g_ora8API.OCIHandleFree(m_handles.m_pOCIError, OCI_HTYPE_ERROR);

        // frees all child handles too            
        m_handles.m_pOCIEnv = NULL;
        m_handles.m_pOCIError = NULL;
        m_handles.m_pOCISvcCtx = NULL;
        m_handles.m_pOCIServer = NULL;
        m_handles.m_pOCISession = NULL;

        throw;
    }
}

void Iora8Connection::Detach()
{
    if (m_handles.m_pOCIError != NULL)
        g_ora8API.OCIHandleFree(m_handles.m_pOCIError, OCI_HTYPE_ERROR);

    // frees all child handles too            
    m_handles.m_pOCIEnv = NULL;
    m_handles.m_pOCIError = NULL;
    m_handles.m_pOCISvcCtx = NULL;
    m_handles.m_pOCIServer = NULL;
    m_handles.m_pOCISession = NULL;
}

/*virtual */
void Iora8Connection::Disconnect()
{
    assert(m_handles.m_pOCIEnv != NULL);
    assert(m_handles.m_pOCIError != NULL);
    assert(m_handles.m_pOCISvcCtx != NULL);
    assert(m_handles.m_pOCIServer != NULL);
    assert(m_handles.m_pOCISession != NULL);

    Check(
        g_ora8API.OCISessionEnd(m_handles.m_pOCISvcCtx,
        m_handles.m_pOCIError, m_handles.m_pOCISession,
        OCI_DEFAULT), m_handles.m_pOCIError, OCI_HTYPE_ERROR);
    Check(
        g_ora8API.OCIServerDetach(m_handles.m_pOCIServer,
        m_handles.m_pOCIError, OCI_DEFAULT), m_handles.m_pOCIError,
        OCI_HTYPE_ERROR);

    // frees all child handles too
    if (!m_bUseConnectionPool)
        Check(g_ora8API.OCIHandleFree(m_handles.m_pOCIEnv, OCI_HTYPE_ENV),
            m_handles.m_pOCIEnv, OCI_HTYPE_ENV);
    m_handles.m_pOCIEnv = NULL;
    m_handles.m_pOCIError = NULL;
    m_handles.m_pOCISvcCtx = NULL;
    m_handles.m_pOCIServer = NULL;
    m_handles.m_pOCISession = NULL;

    m_eOra8TempLobSupport = SA_OCI8TempLobSupport_Unknown;
}

/*virtual */
void Iora8Connection::Destroy()
{
    assert(m_handles.m_pOCIEnv != NULL);
    assert(m_handles.m_pOCIError != NULL);
    assert(m_handles.m_pOCISvcCtx != NULL);
    assert(m_handles.m_pOCIServer != NULL);
    assert(m_handles.m_pOCISession != NULL);

    g_ora8API.OCISessionEnd(m_handles.m_pOCISvcCtx, m_handles.m_pOCIError,
        m_handles.m_pOCISession, OCI_DEFAULT);
    g_ora8API.OCIServerDetach(m_handles.m_pOCIServer, m_handles.m_pOCIError,
        OCI_DEFAULT);

    // frees all child handles too
    if (!m_bUseConnectionPool)
        g_ora8API.OCIHandleFree(m_handles.m_pOCIEnv, OCI_HTYPE_ENV);
    m_handles.m_pOCIEnv = NULL;
    m_handles.m_pOCIError = NULL;
    m_handles.m_pOCISvcCtx = NULL;
    m_handles.m_pOCIServer = NULL;
    m_handles.m_pOCISession = NULL;

    m_eOra8TempLobSupport = SA_OCI8TempLobSupport_Unknown;
}

/*virtual */
void Iora8Connection::Reset()
{
    m_handles.m_pOCIEnv = NULL;
    m_handles.m_pOCIError = NULL;
    m_handles.m_pOCISvcCtx = NULL;
    m_handles.m_pOCIServer = NULL;
    m_handles.m_pOCISession = NULL;

    m_eOra8TempLobSupport = SA_OCI8TempLobSupport_Unknown;

    m_bUseTimeStamp = false;
#ifdef SA_UNICODE
    m_bUseUCS2 = true;
#endif
    mb_cur_max = MB_CUR_MAX;
}

/*virtual */
void Iora8Connection::Commit()
{
    SAIsolationLevel_t eIsolationLevel;
    if (m_eSwitchToIsolationLevelAfterCommit != SA_LevelUnknown)
    {
        eIsolationLevel = m_eSwitchToIsolationLevelAfterCommit;
        m_eSwitchToIsolationLevelAfterCommit = SA_LevelUnknown;
    }
    else
        eIsolationLevel = m_pSAConnection->IsolationLevel();

    Check(
        g_ora8API.OCITransCommit(m_handles.m_pOCISvcCtx,
        m_handles.m_pOCIError, OCI_DEFAULT), m_handles.m_pOCIError,
        OCI_HTYPE_ERROR);

    if (eIsolationLevel != SA_LevelUnknown)
        issueIsolationLevel(eIsolationLevel);
}

/*virtual */
void Iora8Connection::Rollback()
{
    Check(
        g_ora8API.OCITransRollback(m_handles.m_pOCISvcCtx,
        m_handles.m_pOCIError, OCI_DEFAULT), m_handles.m_pOCIError,
        OCI_HTYPE_ERROR);

    SAIsolationLevel_t eIsolationLevel = m_pSAConnection->IsolationLevel();
    if (eIsolationLevel != SA_LevelUnknown)
        issueIsolationLevel(eIsolationLevel);
}

//////////////////////////////////////////////////////////////////////
// IoraClient Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IoraClient::IoraClient()
{
}

IoraClient::~IoraClient()
{
}

ISAConnection *IoraClient::QueryConnectionInterface(SAConnection *pSAConnection)
{
    SAString s = pSAConnection->Option("UseAPI");

    if (s.CompareNoCase(_TSA("OCI7")) == 0)
        return new Iora7Connection(pSAConnection);

    return new Iora8Connection(pSAConnection);
}

//////////////////////////////////////////////////////////////////////
// IoraCursor Class
//////////////////////////////////////////////////////////////////////

typedef enum SALongContextState
{
    LongContextNormal = 1,
    LongContextPiecewiseDefine = 2,
    LongContextPiecewiseBind = 4,
    LongContextPiecewise = LongContextPiecewiseDefine
    | LongContextPiecewiseBind,
    LongContextCallback = 8
} LongContextState_t;

typedef struct tagLongContext
{
    LongContextState_t eState;
    SAValueRead *pReader;
    SAValue *pWriter;
    sb2 *pInd;
    unsigned char *pBuf;
    ub4 Len;
} LongContext_t;

class IoraCursor : public ISACursor
{
protected:
    SAString m_sInternalPrepareStmt;
    SAString OraStatementSQL() const;
    SAString CallSubProgramSQL() const;
    virtual void InternalPrepare(const SAString &sStmt) = 0;

    int *m_pDTY; // array of bound parameters types
    void CheckForReparseBeforeBinding(int nPlaceHolderCount,
        saPlaceHolder **ppPlaceHolders);

    unsigned char m_PiecewiseNullCheckPreFetch[1];
    bool m_bPiecewiseFetchPending;

    IoraCursor(IoraConnection *pIoraConnection, SACommand *pCommand);
    virtual ~IoraCursor();

    virtual size_t InputBufferSize(const SAParam &Param) const;
    virtual size_t OutputBufferSize(SADataType_t eDataType,
        size_t nDataSize) const;

    virtual SADataType_t CnvtNativeToStd(int nNativeType, int nNativeSubType,
        int nSize, int nPrec, int nScale) const;
    virtual int CnvtStdToNative(SADataType_t eDataType) const = 0;
};

IoraCursor::IoraCursor(IoraConnection *pIoraConnection, SACommand *pCommand) :
ISACursor(pIoraConnection, pCommand)
{
    m_pDTY = NULL;

    m_bPiecewiseFetchPending = false;
}

/*virtual */
IoraCursor::~IoraCursor()
{
    if (m_pDTY)
        ::free(m_pDTY);
}

SAString IoraCursor::OraStatementSQL() const
{
    SAString sSQL;

    switch (m_pCommand->CommandType())
    {
    case SA_CmdSQLStmtRaw:
    case SA_CmdSQLStmt:
        sSQL = m_pCommand->CommandText();
        break;
    case SA_CmdStoredProc:
        sSQL = CallSubProgramSQL();
        break;
    default:
        assert(false);
    }

    return sSQL;
}

SAString IoraCursor::CallSubProgramSQL() const
{
    int nParams = m_pCommand->ParamCount();

    SAString sSQL;

    int i;
    // declare boolean parameters
    for (i = 0; i < nParams; ++i)
    {
        SAParam &Param = m_pCommand->ParamByIndex(i);
        if (SA_dtBool == Param.ParamType())
            sSQL += _TSA("bool") + Param.Name() + _TSA(" boolean;\n");
    }
    if (!sSQL.IsEmpty())
        sSQL = _TSA("declare\n") + sSQL;
    sSQL += _TSA("begin\n");

    // set the boolean parameters and check for return parameter
    SAString sExec;
    for (i = 0; i < nParams; ++i)
    {
        SAParam &Param = m_pCommand->ParamByIndex(i);
        if (SA_dtBool == Param.ParamType()
            && (SA_ParamInput == Param.ParamDirType()
            || SA_ParamInputOutput == Param.ParamDirType()))
        {
            sSQL += _TSA("if :") + Param.Name() + _TSA(" > 0 then bool")
                + Param.Name() + _TSA(":=true; else bool")
                + Param.Name() + _TSA(":=false; end if;\n");
        }
        else if (SA_ParamReturn == Param.ParamDirType())
        {
            sExec = SA_dtBool == Param.ParamType() ? _TSA("bool") : _TSA(":");
            sExec += Param.Name();
            sExec += _TSA(":=");
        }
    }
    sSQL += sExec + m_pCommand->CommandText();

    // specify parameters
    sSQL += _TSA("(");
    SAString sParams;
    for (i = 0; i < nParams; ++i)
    {
        SAParam &Param = m_pCommand->ParamByIndex(i);
        if (Param.ParamDirType() == SA_ParamReturn)
            continue;
        if (Param.isDefault())
            continue;

        if (!sParams.IsEmpty())
            sParams += _TSA(", ");
        sParams += Param.Name();
        sParams += _TSA("=>");
        sParams += SA_dtBool == Param.ParamType() ? _TSA("bool") : _TSA(":");
        sParams += Param.Name();
    }
    sSQL += sParams;
    sSQL += _TSA(");\n");

    // set boolean output parameters
    for (i = 0; i < nParams; ++i)
    {
        SAParam &Param = m_pCommand->ParamByIndex(i);
        if (SA_dtBool == Param.ParamType()
            && (SA_ParamReturn == Param.ParamDirType()
            || SA_ParamOutput == Param.ParamDirType()
            || SA_ParamInputOutput == Param.ParamDirType()))
        {
            sSQL += _TSA("if bool") + Param.Name() + _TSA(" then :")
                + Param.Name() + _TSA(":=1; else :")
                + Param.Name() + _TSA(":=0; end if;\n");
        }
    }

    sSQL += _TSA("end;");
    return sSQL;
}

/*virtual */
size_t IoraCursor::InputBufferSize(const SAParam &Param) const
{
    switch (Param.DataType())
    {
    case SA_dtBool:
        // there is no "native" boolean type in Oracle,
        // so treat boolean as 16-bit signed INTEGER in Oracle
        return sizeof(short);
    case SA_dtNumeric:
        return sizeof(OraVARNUM_t);
    case SA_dtDateTime:
        return sizeof(OraDATE_t); // Oracle internal date/time representation
    case SA_dtLongBinary:
    case SA_dtLongChar:
        return sizeof(LongContext_t);
    default:
        break;
    }

    return ISACursor::InputBufferSize(Param);
}

/*virtual */
size_t IoraCursor::OutputBufferSize(SADataType_t eDataType,
    size_t nDataSize) const
{
    switch (eDataType)
    {
    case SA_dtBool:
        return sizeof(short);
    case SA_dtNumeric:
        return sizeof(OraVARNUM_t);
    case SA_dtDateTime:
        return sizeof(OraDATE_t);
    case SA_dtBytes:
        // Oracle can report nDataSize = 0 for output parameters
        // under some conditions
        return ISACursor::OutputBufferSize(eDataType,
            nDataSize ? nDataSize : 4000);
    case SA_dtString:
        // Oracle can report nDataSize = 0 for output parameters
        // under some conditions
        return ISACursor::OutputBufferSize(eDataType,
            nDataSize ? nDataSize : 4000);
    case SA_dtLongBinary:
    case SA_dtLongChar:
        return sizeof(LongContext_t);
    default:
        break;
    }

    return ISACursor::OutputBufferSize(eDataType, nDataSize);
}

/*virtual */
SADataType_t IoraCursor::CnvtNativeToStd(int dbtype, int/* dbsubtype*/,
    int/* dbsize*/, int prec, int scale) const
{
    SADataType_t eDataType;

    switch (dbtype)
    {
    case 1: // VARCHAR2	2000 (4000) bytes
        eDataType = SA_dtString;
        break;
    case 2: // NUMBER	21 bytes
        // if selecting constant or formula
        if (prec == 0) // prec unknown
            eDataType = SA_dtNumeric;
        else if (scale > 0) // floating point exists?
            eDataType = prec > 15 ? SA_dtNumeric : SA_dtDouble;
        else // check for exact type
        {
            int ShiftedPrec = prec - scale; // not to deal with negative scale if it is
            if (ShiftedPrec < 5) // -9,999 : 9,999
                eDataType = SA_dtShort;
            else if (ShiftedPrec < 10) // -999,999,999 : 999,999,999
                eDataType = SA_dtLong;
            else if (ShiftedPrec <= 15) // When ShiftedPrec <= 15 the C++ double is OK
                eDataType = SA_dtDouble;
            else
                eDataType = SA_dtNumeric;
        }
        break;
    case 3: // pls_integer, binary_integer
        eDataType = SA_dtLong;
        break;
    case 68:
        eDataType = SA_dtULong;
        break;
    case 8: // LONG		2^31-1 bytes
        eDataType = SA_dtLongChar;
        break;
    case 11: // ROWID	6 (10) bytes
        eDataType = SA_dtString;
        break;
    case 12: // DATE		7 bytes
        eDataType = SA_dtDateTime;
        break;
    case 23: // RAW		255 bytes
        eDataType = SA_dtBytes;
        break;
    case 24: // LONG RAW	2^31-2 bytes
        eDataType = SA_dtLongBinary;
        break;
    case 96: // CHAR		255 (2000) bytes
        eDataType = SA_dtString;
        break;
    case 100: // BINARY_FLOAT
    case 101: // BINARY_DOUBLE
        eDataType = SA_dtDouble;
        break;
    case 102: // CURSOR VARIABLE
        eDataType = SA_dtCursor;
        break;
    case 105: // MSLABEL	255 bytes
        eDataType = SA_dtString;
        break;
    case 252: // BOOLEAN
        eDataType = SA_dtBool;
        break;
    default:
        assert(false);
        eDataType = SA_dtUnknown;
    }

    return eDataType;
}

//////////////////////////////////////////////////////////////////////
// Iora7Cursor Class
//////////////////////////////////////////////////////////////////////

class Iora7Cursor : public IoraCursor
{
    friend class Iora7Connection;

    ora7CommandHandles m_handles;

    bool m_bOpened;
    bool m_bResultSetExist;
    bool m_bResultSetCanBe;

    bool m_bPiecewiseBindAllowed;
    bool m_bPiecewiseFetchAllowed;
    void BindLongs();
    void DiscardPiecewiseFetch();
    void CheckPiecewiseNull(bool bAfterExecute = false);
    SAField *WhichFieldIsPiecewise() const;

    sword m_cRowsToPrefetch; // defined in SetSelectBuffers
    sword m_cRowsObtained;
    sword m_cRowCurrent;
    bool FetchNextArray();

protected:
    virtual size_t InputBufferSize(const SAParam &Param) const;
    virtual size_t OutputBufferSize(SADataType_t eDataType,
        size_t nDataSize) const;
    virtual void SetFieldBuffer(
        int nCol, // 1-based
        void *pInd, size_t nIndSize, void *pSize, size_t nSizeSize,
        void *pValue, size_t nValueSize);

    virtual int CnvtStdToNative(SADataType_t eDataType) const;
    virtual void InternalPrepare(const SAString &sStmt);

public:
    Iora7Cursor(Iora7Connection *pIora7Connection, SACommand *pCommand);
    virtual ~Iora7Cursor();

    virtual bool IsOpened();
    virtual void Open();
    virtual void Close();
    virtual void Destroy();
    virtual void Reset();

    virtual void Prepare(const SAString &sStmt, SACommandType_t eCmdType,
        int nPlaceHolderCount, saPlaceHolder **ppPlaceHolders);
    // binds parameters
    void Bind(int nPlaceHolderCount, saPlaceHolder **ppPlaceHolders);
    // executes statement
    virtual void Execute(int nPlaceHolderCount, saPlaceHolder **ppPlaceHolders);
    // cleans up after execute if needed, so the statement can be reexecuted
    virtual void UnExecute();
    virtual void Cancel();

    virtual bool ResultSetExists();
    virtual void DescribeFields(DescribeFields_cb_t fn);
    virtual void SetSelectBuffers();
    virtual bool FetchNext();

    virtual long GetRowsAffected();

    virtual void ReadLongOrLOB(ValueType_t eValueType, SAValueRead &vr,
        void *pValue, size_t nFieldBufSize, saLongOrLobReader_t fnReader,
        size_t nReaderWantedPieceSize, void *pAddlData);

    virtual void DescribeParamSP();

    virtual saCommandHandles *NativeHandles();
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Iora7Cursor::Iora7Cursor(Iora7Connection *pIora7Connection, SACommand *pCommand) :
IoraCursor(pIora7Connection, pCommand)
{
    Reset();
}

/*virtual */
Iora7Cursor::~Iora7Cursor()
{
}

/*virtual */
size_t Iora7Cursor::InputBufferSize(const SAParam &Param) const
{
    switch (Param.DataType())
    {
    case SA_dtBLob:
    case SA_dtCLob:
        return sizeof(LongContext_t);
    case SA_dtCursor:
        return sizeof(Cda_Def);
    default:
        break;
    }

    return IoraCursor::InputBufferSize(Param);
}

/*virtual */
size_t Iora7Cursor::OutputBufferSize(SADataType_t eDataType,
    size_t nDataSize) const
{
    switch (eDataType)
    {
    case SA_dtCursor:
        return sizeof(Cda_Def);
    default:
        break;
    }

    return IoraCursor::OutputBufferSize(eDataType, nDataSize);
}

/*virtual */
bool Iora7Cursor::IsOpened()
{
    return m_bOpened;
}

/*virtual */
void Iora7Cursor::Open()
{
    assert(m_bOpened == false);
    ((Iora7Connection*)m_pISAConnection)->Check(
        g_ora7API.oopen(&m_handles.m_cda,
        &((Iora7Connection*)m_pISAConnection)->m_handles.m_lda,
        NULL, -1, -1, NULL, -1), &m_handles.m_cda);
    m_bOpened = true;
}

/*virtual */
void Iora7Cursor::Close()
{
    assert(m_bOpened != 0);
    ((Iora7Connection*)m_pISAConnection)->Check(
        g_ora7API.oclose(&m_handles.m_cda), &m_handles.m_cda);
    m_bOpened = false;
}

/*virtual */
void Iora7Cursor::Destroy()
{
    assert(m_bOpened != 0);
    g_ora7API.oclose(&m_handles.m_cda);
    m_bOpened = false;
}

/*virtual */
void Iora7Cursor::Reset()
{
    m_bOpened = false;
    m_bResultSetExist = false;

    m_bPiecewiseBindAllowed = true;
    m_bPiecewiseFetchAllowed = true;
}

//////////////////////////////////////////////////////////////////////
// Iora8Cursor Class
//////////////////////////////////////////////////////////////////////

// helper structures
typedef struct tagBlobReturningContext
{
    SAParam *pParam;
    OCIError *pOCIError;
    OCIEnv *pOCIEnv;
    OCILobLocator ***pppOCILobLocators;
    ub4 **ppLobLocatorsLens;
    int nLobLocatorCol;
    ub4 *pnLobReturnBindsColCount;
    ub4 *pnBLobBindsRowCount;
} BlobReturningContext_t;

class Iora8Cursor : public IoraCursor
{
    friend class Iora8Connection;

    ora8CommandHandles m_handles;

    ub2 m_nOraStmtType;
    bool m_bResultSetCanBe;

    void DiscardPiecewiseFetch();
    void ReadLong(ValueType_t eValueType, SAValueRead &vr,
        LongContext_t *pLongContext, saLongOrLobReader_t fnReader,
        size_t nReaderWantedPieceSize, void *pAddlData);
    void CheckPiecewiseNull();
    SAField *WhichFieldIsPiecewise() const;
    void ReadLob(ValueType_t eValueType, SAValueRead &vr,
        OCILobLocator* pOCILobLocator, saLongOrLobReader_t fnReader,
        size_t nReaderWantedPieceSize, void *pAddlData);
    void ReadLob2(ValueType_t eValueType, SAValueRead &vr,
        OCILobLocator* pOCILobLocator, saLongOrLobReader_t fnReader,
        size_t nReaderWantedPieceSize, void *pAddlData);
    void CreateTemporaryLob(OCILobLocator **ppLob, SAParam &Param);
    // returns total size written
    ub4 BindLob(OCILobLocator *pLob, SAParam &Param);

    static sb4 LongInBind(dvoid *ictxp, OCIBind *bindp, ub4 iter, ub4 index,
        dvoid **bufpp, ub4 *alenp, ub1 *piecep, dvoid **indpp);
    static sb4 LongOutBind(dvoid *ictxp, OCIBind *bindp, ub4 iter, ub4 index,
        dvoid **bufpp, ub4 **alenp, ub1 *piecep, dvoid **indpp,
        ub2 **rcodep);
    static sb4 LongDefine(dvoid *octxp, OCIDefine *defnp, ub4 iter,
        dvoid **bufpp, ub4 **alenp, ub1 *piecep, dvoid **indp,
        ub2 **rcodep);

    static sb4 LongDefineOrOutBind(dvoid *octxp, dvoid **bufpp, ub4 **alenpp,
        ub1 *piecep, dvoid **indpp);

    void SetCharSetOptions(const SAString &sOCI_ATTR_CHARSET_FORM,
        const SAString &sOCI_ATTR_CHARSET_ID, dvoid *trgthndlp,
        ub4 trghndltyp);

    // binding LOBs with not NULL values (add returning into clause)
    ub4 m_nLobReturnBindsColCount;
    ub4 m_nBLobBindsRowCount;
    saPlaceHolder **m_ppLobReturnPlaceHolders; // m_ppLobReturnPlaceHolders[m_nLobReturnBindsColCount]
    OCILobLocator ***m_pppBLobReturningLocs; // m_pppBLobReturningLocs[m_nLobReturnBindsColCount][m_nBLobBindsRowCount]
    ub4 **m_ppLobReturningLens; // m_ppLobReturningLens[m_nLobReturnBindsColCount][m_nBLobBindsRowCount]
    void DestroyLobsReturnBinding();
    void CheckForReparseBeforeBinding(int nPlaceHolderCount,
        saPlaceHolder **ppPlaceHolders);
    virtual void InternalPrepare(const SAString &sStmt);
    static sb2 stat_m_BLobReturningNullInd;
    static sb4 LobReturningInBind(dvoid *ictxp, OCIBind *bindp, ub4 iter,
        ub4 index, dvoid **bufpp, ub4 *alenp, ub1 *piecep, dvoid **indpp);
    static sb4 LobReturningOutBind(dvoid *octxp, OCIBind *bindp, ub4 iter,
        ub4 index, dvoid **bufpp, ub4 **alenp, ub1 *piecep, dvoid **indp,
        ub2 **rcodep);
    void BindReturningLobs();

    // binding using temporary Lobs
    OCILobLocator **m_ppTempLobs;
    unsigned int m_cTempLobs;
    void FreeTemporaryLobsIfAny();
    void FreeLobIfTemporary(OCILobLocator* pOCILobLocator);

    // OCIDateTime parameters
    OCIDateTime** m_ppDateTimeParams;
    unsigned int m_cDateTimeParams;
    void AllocDateTimeParam(OCIDateTime** ppParam);
    void FreeDateTimeParamsIfAny();

    ub4 m_cRowsToPrefetch; // defined in SetSelectBuffers
    ub4 m_cRowsObtained;
    ub4 m_cRowCurrent;
    bool m_bOCI_NO_DATA;
    bool FetchNextArray();

protected:
    virtual size_t InputBufferSize(const SAParam &Param) const;
    virtual size_t OutputBufferSize(SADataType_t eDataType,
        size_t nDataSize) const;
    virtual void SetFieldBuffer(
        int nCol, // 1-based
        void *pInd, size_t nIndSize, void *pSize, size_t nSizeSize,
        void *pValue, size_t nValueSize);

    virtual SADataType_t CnvtNativeToStd(int nNativeType, int nNativeSubType,
        int nSize, int nPrec, int nScale) const;
    virtual int CnvtStdToNative(SADataType_t eDataType) const;

public:
    Iora8Cursor(Iora8Connection *pIora8Connection, SACommand *pCommand);
    virtual ~Iora8Cursor();

    virtual bool IsOpened();
    virtual void Open();
    virtual void Close();
    virtual void Destroy();
    virtual void Reset();

    virtual void Prepare(const SAString &sStmt, SACommandType_t eCmdType,
        int nPlaceHolderCount, saPlaceHolder **ppPlaceHolders);
    // binds parameters
    void Bind(int nPlaceHolderCount, saPlaceHolder **ppPlaceHolders);
    // executes statement
    virtual void Execute(int nPlaceHolderCount, saPlaceHolder **ppPlaceHolders);
    // cleans up after execute if needed, so the statement can be reexecuted
    virtual void UnExecute();
    virtual void Cancel();

    virtual bool ResultSetExists();
    virtual void DescribeFields(DescribeFields_cb_t fn);
    virtual void SetSelectBuffers();
    virtual bool FetchNext();
    virtual bool FetchPrior();
    virtual bool FetchFirst();
    virtual bool FetchLast();
    virtual bool FetchPos(int offset, bool Relative = false);

    virtual long GetRowsAffected();

    virtual void ReadLongOrLOB(ValueType_t eValueType, SAValueRead &vr,
        void *pValue, size_t nFieldBufSize, saLongOrLobReader_t fnReader,
        size_t nReaderWantedPieceSize, void *pAddlData);

    virtual void DescribeParamSP();

    virtual saCommandHandles *NativeHandles();

    virtual void ConvertString(SAString &String, const void *pData,
        size_t nRealSize);
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

/*static */
sb2 Iora8Cursor::stat_m_BLobReturningNullInd = OCI_IND_NULL;

Iora8Cursor::Iora8Cursor(Iora8Connection *pIora8Connection, SACommand *pCommand) :
IoraCursor(pIora8Connection, pCommand)
{
    m_ppLobReturnPlaceHolders = NULL;
    m_pppBLobReturningLocs = NULL;
    m_ppLobReturningLens = NULL;

    m_ppTempLobs = NULL;

    m_ppDateTimeParams = NULL;

    m_nLobReturnBindsColCount = 0;

    Reset();
}

/*virtual */
Iora8Cursor::~Iora8Cursor()
{
    try
    {
        DestroyLobsReturnBinding();
    }
    catch (SAException &)
    {
    }
}

/*virtual */
size_t Iora8Cursor::InputBufferSize(const SAParam &Param) const
{
    if (Param.DataType() == SA_dtCursor)
        return sizeof(OCIStmt *);

    switch (Param.DataType())
    {
    case SA_dtBLob:
    case SA_dtCLob:
        return sa_max(
            sizeof(OCILobLocator *), // if binding using temporary Lob
            sizeof(BlobReturningContext_t)); // if binding 'returning into' clause
    case SA_dtDateTime:
        if (((Iora8Connection*)m_pISAConnection)->m_bUseTimeStamp)
            return sizeof(OCIDateTime *);
        break;
#ifdef SA_UNICODE
    case SA_dtString:
        if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
            return (Param.asString().GetUTF16CharsLength() * sizeof(utext));
        break;
#endif
    default:
        break;
}

    return IoraCursor::InputBufferSize(Param);
}

/*virtual */
size_t Iora8Cursor::OutputBufferSize(SADataType_t eDataType,
    size_t nDataSize) const
{
    switch (eDataType)
    {
    case SA_dtBLob:
    case SA_dtCLob:
        return sizeof(OCILobLocator *);
    case SA_dtCursor:
        return sizeof(OCIStmt *);
    case SA_dtDateTime:
        if (((Iora8Connection*)m_pISAConnection)->m_bUseTimeStamp
            && nDataSize != sizeof(OraDATE_t))
            return sizeof(OCIDateTime *);
        break;
#ifdef SA_UNICODE
    case SA_dtString:
        // Oracle can report nDataSize = 0 for output parameters
        // under some conditions
        if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
            return (sizeof(utext) * (nDataSize ? nDataSize : 4000));
        break;
#endif
    default:
        break;
}

    return IoraCursor::OutputBufferSize(eDataType, nDataSize);
}

/*virtual */
bool Iora8Cursor::IsOpened()
{
    return m_handles.m_pOCIStmt != NULL;
}

/*virtual */
void Iora8Cursor::Open()
{
    assert(m_handles.m_pOCIStmt == NULL);
    assert(m_handles.m_pOCIError == NULL);

    Iora8Connection::Check(
        g_ora8API.OCIHandleAlloc(
        ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
        (dvoid**)&m_handles.m_pOCIError, OCI_HTYPE_ERROR, 0,
        (dvoid**)0),
        ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
        OCI_HTYPE_ENV);
    Iora8Connection::Check(
        g_ora8API.OCIHandleAlloc(
        ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
        (dvoid**)&m_handles.m_pOCIStmt, OCI_HTYPE_STMT, 0, NULL),
        ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
        OCI_HTYPE_ENV);

    assert(m_handles.m_pOCIStmt != NULL);
    assert(m_handles.m_pOCIError != NULL);
}

/*virtual */
void Iora8Cursor::Close()
{
    assert(m_handles.m_pOCIStmt != NULL);
    assert(m_handles.m_pOCIError != NULL);

    Iora8Connection::Check(
        g_ora8API.OCIHandleFree(m_handles.m_pOCIStmt, OCI_HTYPE_STMT),
        m_handles.m_pOCIStmt, OCI_HTYPE_STMT);
    Iora8Connection::Check(
        g_ora8API.OCIHandleFree(m_handles.m_pOCIError, OCI_HTYPE_ERROR),
        m_handles.m_pOCIError, OCI_HTYPE_ERROR);
    m_handles.m_pOCIStmt = NULL;
    m_handles.m_pOCIError = NULL;
}

/*virtual */
void Iora8Cursor::Destroy()
{
    assert(m_handles.m_pOCIStmt != NULL);
    assert(m_handles.m_pOCIError != NULL);

    g_ora8API.OCIHandleFree(m_handles.m_pOCIStmt, OCI_HTYPE_STMT);
    g_ora8API.OCIHandleFree(m_handles.m_pOCIError, OCI_HTYPE_ERROR);

    m_handles.m_pOCIStmt = NULL;
    m_handles.m_pOCIError = NULL;
}

/*virtual */
void Iora8Cursor::Reset()
{
    m_handles.m_pOCIStmt = NULL;
    m_handles.m_pOCIError = NULL;

    m_nOraStmtType = 0; // unknown
    m_bResultSetCanBe = false;

    while (m_nLobReturnBindsColCount)
    {
        delete m_pppBLobReturningLocs[--m_nLobReturnBindsColCount];
        delete m_ppLobReturningLens[m_nLobReturnBindsColCount];
    }
    if (m_ppLobReturnPlaceHolders)
    {
        ::free(m_ppLobReturnPlaceHolders);
        m_ppLobReturnPlaceHolders = NULL;
    }
    delete m_pppBLobReturningLocs;
    m_pppBLobReturningLocs = NULL;
    delete m_ppLobReturningLens;
    m_ppLobReturningLens = NULL;
    m_nLobReturnBindsColCount = 0;
    m_nBLobBindsRowCount = 0;

    if (m_ppTempLobs)
    {
        ::free(m_ppTempLobs);
        m_ppTempLobs = NULL;
    }
    m_cTempLobs = 0;

    if (m_ppDateTimeParams)
    {
        ::free(m_ppDateTimeParams);
        m_ppDateTimeParams = NULL;
    }
    m_cDateTimeParams = 0;
}

/*virtual */
ISACursor *Iora7Connection::NewCursor(SACommand *m_pCommand)
{
    return new Iora7Cursor(this, m_pCommand);
}

/*virtual */
ISACursor *Iora8Connection::NewCursor(SACommand *m_pCommand)
{
    return new Iora8Cursor(this, m_pCommand);
}

/*virtual */
void Iora7Cursor::InternalPrepare(const SAString &sStmt)
{
    // save because (extraction from Oracle docs):
    // "The pointer to the text of the statement must be available
    // as long as the statement is
    // executed, or data is fetched from it."
    m_sInternalPrepareStmt = sStmt;

    SA_TRACE_CMD(m_sInternalPrepareStmt);
    ((Iora7Connection*)m_pISAConnection)->Check(
        g_ora7API.oparse(&m_handles.m_cda,
        (text*)m_sInternalPrepareStmt.GetMultiByteChars(), -1, 0,
        1), &m_handles.m_cda);

    // no binds are in effect after parsing
    if (m_pDTY)
    {
        ::free(m_pDTY);
        m_pDTY = NULL;
    }
}

/*virtual */
void Iora7Cursor::Prepare(const SAString &/*sStmt*/,
    SACommandType_t/* eCmdType*/, int/* nPlaceHolderCount*/,
    saPlaceHolder** /*ppPlaceHolders*/)
{

    // always compile original to check errors
    InternalPrepare(OraStatementSQL());
}

void Iora7Cursor::BindLongs()
{
    assert(m_bPiecewiseBindAllowed);
    assert(m_handles.m_cda.rc == 3129);
    // the next Piece to be inserted is required

    try
    {
        // it is a piecewise bind, so we can bind only one column
        sword rc = 0;
        SAPieceType_t ePieceType = SA_FirstPiece;
        while (m_handles.m_cda.rc == 3129)
        {
            ub1 piece;
            dvoid* ctxp;
            ub4 iter;
            ub4 index;
            ((Iora7Connection*)m_pISAConnection)->Check(
                g_ora7API.ogetpi(&m_handles.m_cda, &piece, &ctxp, &iter,
                &index), &m_handles.m_cda);

            LongContext_t *pLongContext = (LongContext_t *)ctxp;
            void *pBuf;
            ub4 nActualWrite = (ub4)pLongContext->pWriter->InvokeWriter(
                ePieceType, Iora7Connection::MaxLongPiece, pBuf);
            if (!nActualWrite || ePieceType == SA_LastPiece
                || ePieceType == SA_OnePiece)
                piece = OCI_LAST_PIECE;
            if (!nActualWrite)
                pBuf = NULL;

            ((Iora7Connection*)m_pISAConnection)->Check(
                g_ora7API.osetpi(&m_handles.m_cda, piece, pBuf,
                &nActualWrite), &m_handles.m_cda);

            rc = g_ora7API.oexec(&m_handles.m_cda);
        }

        if (m_handles.m_cda.rc != 3130) // the buffer for the next Piece to be fetched is required
            ((Iora7Connection*)m_pISAConnection)->Check(rc, &m_handles.m_cda);
    }

    catch (SAUserException &) // clean up after user exception
    {
        ub4 nActualWrite = 0;
        g_ora7API.osetpi(&m_handles.m_cda, OCI_LAST_PIECE, NULL, &nActualWrite);
        g_ora7API.oexec(&m_handles.m_cda);
        throw;
    }
}

/*virtual */
void Iora7Cursor::UnExecute()
{
    m_bResultSetExist = false;
}

/*virtual */
void Iora7Cursor::Execute(int nPlaceHolderCount, saPlaceHolder **ppPlaceHolders)
{
    if (m_pCommand->ParamCount() > 0)
        Bind(nPlaceHolderCount, ppPlaceHolders);

    sword rc = g_ora7API.oexec(&m_handles.m_cda);
    ub2 ft = m_handles.m_cda.ft;

    if (m_handles.m_cda.rc != 3129 // the next Piece to be inserted is required
        && m_handles.m_cda.rc != 3130) // the buffer for the next Piece to be fetched is required
        ((Iora7Connection*)m_pISAConnection)->Check(rc, &m_handles.m_cda);

    // Oracle can return both 3129 and 3130 (f.ex. in PL/SQL block)
    // we will be ready
    if (m_handles.m_cda.rc == 3129)
        BindLongs();
    if (m_handles.m_cda.rc == 3130) // the buffer for the next Piece to be fetched is required
    {
        assert(m_bPiecewiseBindAllowed);
        m_bPiecewiseFetchPending = true;
        CheckPiecewiseNull(true);
    }

    m_bResultSetExist = (ft == 4);
    ConvertOutputParams(); // if any
}

/*virtual */
void Iora7Cursor::Cancel()
{
    ((Iora7Connection*)m_pISAConnection)->Check(
        g_ora7API.obreak(
        &((Iora7Connection*)m_pISAConnection)->m_handles.m_lda),
        NULL);
}

void Iora7Cursor::Bind(int nPlaceHolderCount, saPlaceHolder** ppPlaceHolders)
{
    CheckForReparseBeforeBinding(nPlaceHolderCount, ppPlaceHolders);

    sa_realloc((void**)&m_pDTY, sizeof(int) * m_pCommand->ParamCount());

    // we should bind all params, not place holders in Oracle
    // one exception: :a and :"a" are belong to the same parameter in SQLAPI
    // but Oracle treats them as different
    AllocBindBuffer(sizeof(sb2), sizeof(ub2));
    void *pBuf = m_pParamBuffer;

    for (int i = 0; i < m_pCommand->ParamCount(); ++i)
    {
        SAParam &Param = m_pCommand->ParamByIndex(i);

        void *pInd;
        void *pSize;

        size_t nDataBufSize;
        void *pValue;
        IncParamBuffer(pBuf, pInd, pSize, nDataBufSize, pValue);

        if (Param.isDefault())
            continue;

        SADataType_t eDataType = Param.DataType();
        sword ftype = (sword)CnvtStdToNative(
            eDataType == SA_dtUnknown ? SA_dtString : eDataType);
        m_pDTY[i] = (int)ftype;

        sb2 *indp = (sb2 *)pInd; // bind null indicator
        ub1 *pvctx = (ub1 *)pValue;
        sword progvl = (sword)nDataBufSize; // allocated
        ub2 *alenp = (ub2 *)pSize;

        // special code for ref cursors
        if (eDataType == SA_dtCursor)
        {
            *indp = 0; // set indicator as if field is not null always, or Oracle can throw an error ("invalid cursor")

            assert(nDataBufSize == sizeof(Cda_Def));
            memset(pvctx, 0, nDataBufSize);
            *alenp = 0;

            if (!Param.isNull() && isInputParam(Param))
            {
                SACommand *pCursor = Param.asCursor();
                assert(pCursor);

                const ora7CommandHandles *pH =
                    (ora7CommandHandles *)pCursor->NativeHandles();
                memcpy(pvctx, &pH->m_cda, sizeof(Cda_Def));

                *alenp = (ub2)InputBufferSize(Param);
            }
        }
        else
        {
            if (isInputParam(Param))
            {
                if (Param.isNull())
                    *indp = -1; // field is null
                else
                    *indp = 0; // field is not null

                *alenp = (ub2)InputBufferSize(Param);
                assert(progvl >= *alenp);

                if (Param.isNull())
                {
                    if (progvl == 0)
                        progvl = 1; // dummy, because Oracle can hang when == 0
                }
                else
                {
                    switch (eDataType)
                    {
                    case SA_dtUnknown:
                        throw SAException(SA_Library_Error, SA_Library_Error_UnknownParameterType, -1,
                            IDS_UNKNOWN_PARAMETER_TYPE,
                            (const SAChar*)Param.Name());
                    case SA_dtBool:
                        // there is no "native" boolean type in Oracle,
                        // so treat boolean as 16-bit signed INTEGER in Oracle
                        assert(*alenp == sizeof(short));
                        *(short*)pvctx = (short)Param.asBool();
                        break;
                    case SA_dtShort:
                        assert(*alenp == sizeof(short));
                        *(short*)pvctx = Param.asShort();
                        break;
                    case SA_dtUShort:
                        assert(*alenp == sizeof(unsigned short));
                        *(unsigned short*)pvctx = Param.asUShort();
                        break;
                    case SA_dtLong:
                        assert(*alenp == sizeof(long));
                        *(long*)pvctx = Param.asLong();
                        break;
                    case SA_dtULong:
                        assert(*alenp == sizeof(unsigned long));
                        *(unsigned long*)pvctx = Param.asULong();
                        break;
                    case SA_dtDouble:
                        assert(*alenp == sizeof(double));
                        *(double*)pvctx = Param.asDouble();
                        break;
                    case SA_dtNumeric:
                        assert(*alenp == sizeof(OraVARNUM_t));
                        // Oracle internal number representation
                        IoraConnection::CnvtNumericToInternal(Param.asNumeric(),
                            *(OraVARNUM_t*)pvctx);
                        break;
                    case SA_dtDateTime:
                        assert(*alenp == sizeof(OraDATE_t));
                        IoraConnection::CnvtDateTimeToInternal(
                            Param.asDateTime(), (OraDATE_t*)pvctx);
                        break;
                    case SA_dtString:
                        assert(
                            *alenp == (ub2)Param.asString().GetMultiByteCharsLength());
                        memcpy(pvctx, Param.asString().GetMultiByteChars(),
                            *alenp);
                        break;
                    case SA_dtBytes:
                        assert(
                            *alenp == (ub2)Param.asBytes().GetBinaryLength());
                        memcpy(pvctx, (const void*)Param.asBytes(), *alenp);
                        break;
                    case SA_dtLongBinary:
                    case SA_dtBLob:
                        assert(*alenp == sizeof(LongContext_t));
                        break;
                    case SA_dtLongChar:
                    case SA_dtCLob:
                        assert(*alenp == sizeof(LongContext_t));
                        break;
                    case SA_dtCursor:
                        assert(false);
                        // already handled
                        break;
                    default:
                        assert(false);
                    }
                }
            }
        }

        bool bLong = isLongOrLob(eDataType);

        for (int j = 0; j < nPlaceHolderCount; ++j)
        {
            saPlaceHolder &PlaceHolder = *ppPlaceHolders[j];
            if (PlaceHolder.getParam() != &Param)
                continue;

            if (bLong)
            {
                LongContext_t *pLongContext = (LongContext_t *)pvctx;
                pLongContext->pReader = &Param;
                pLongContext->pWriter = &Param;
                pLongContext->pInd = (sb2*)pInd;

                if (m_bPiecewiseBindAllowed)
                {
                    ((Iora7Connection*)m_pISAConnection)->Check(
                        g_ora7API.obindps(
                        &m_handles.m_cda,
                        0,
                        (text*)PlaceHolder.getFullName().GetMultiByteChars(),
                        -1, pvctx, SB4MAXVAL, ftype,
                        0, // scale, not used
                        indp, (ub2*)0, NULL, 0, 0, 0, 0, 0, NULL,
                        NULL, 0, 0), &m_handles.m_cda); // not used
                    pLongContext->eState = LongContextPiecewiseBind;
                }
                else
                {
                    ((Iora7Connection*)m_pISAConnection)->Check(
                        g_ora7API.obndra(&m_handles.m_cda,
                        (text*)Param.Name().GetMultiByteChars(),
                        -1, pvctx, progvl, ftype, 0, // scale, not used
                        indp, alenp, NULL, 0, NULL, NULL, 0, 0),
                        &m_handles.m_cda); // not used
                    pLongContext->eState = LongContextNormal;
                }
            }
            else
            {
                if (m_bPiecewiseBindAllowed)
                    ((Iora7Connection*)m_pISAConnection)->Check(
                    g_ora7API.obindps(
                    &m_handles.m_cda,
                    1,
                    (text*)PlaceHolder.getFullName().GetMultiByteChars(),
                    -1, pvctx, progvl, ftype,
                    0, // scale, not used
                    indp, alenp, NULL, 0, 0, 0, 0, 0, NULL,
                    NULL, 0, 0), &m_handles.m_cda); // not used
                else
                {
                    ((Iora7Connection*)m_pISAConnection)->Check(
                        g_ora7API.obndra(&m_handles.m_cda,
                        (text*)Param.Name().GetMultiByteChars(),
                        -1, pvctx, progvl, ftype, 0, // scale, not used
                        indp, alenp, NULL, 0, NULL, NULL, 0, 0),
                        &m_handles.m_cda); // not used
                }
            }
        }
    }
}

/*virtual */
bool Iora7Cursor::ResultSetExists()
{
    return m_bResultSetExist;
}

void Iora7Cursor::DescribeFields(DescribeFields_cb_t fn)
{
    sword iField = 0;
    do
    {
        sb4 dbsize;
        sb2 dbtype;
        sb1 cbuf[1024];
        sb4 cbufl = sizeof(cbuf);
        sb4 dsize;
        sb2 prec;
        sb2 scale;
        sb2 nullok;

        sword rc = g_ora7API.odescr(&m_handles.m_cda, ++iField, &dbsize,
            &dbtype, cbuf, &cbufl, &dsize, &prec, &scale, &nullok);

        if (m_handles.m_cda.rc == 1007)
            break;
        ((Iora7Connection*)m_pISAConnection)->Check(rc, &m_handles.m_cda);

        (m_pCommand->*fn)(SAString((const char*)cbuf, cbufl),
            CnvtNativeToStd(dbtype, 0, dbsize, prec, scale), (int)dbtype,
            dbsize, prec, scale, nullok == 0, -1);
    } while (m_handles.m_cda.rc != 1007);
}

/*virtual */
long Iora7Cursor::GetRowsAffected()
{
    return m_handles.m_cda.rpc;
}

SAField *Iora7Cursor::WhichFieldIsPiecewise() const
{
    // field can be fetched piecewise if:
    // 1. it it Long*
    // 2. it is the last in select list
    // 3. it is the only Long* field in select list
    if (FieldCount(2, SA_dtLongBinary, SA_dtLongChar) == 1)
    {
        SAField &Field = m_pCommand->Field(m_pCommand->FieldCount());
        switch (Field.FieldType())
        {
        case SA_dtLongBinary:
        case SA_dtLongChar:
            return &Field;
        default:
            break;
        }
    }
    return 0;
}

/*virtual */
void Iora7Cursor::SetFieldBuffer(
    int nCol, // 1-based
    void *pInd, size_t nIndSize, void *pSize, size_t nSizeSize,
    void *pValue, size_t nValueSize)
{
    SAField &Field = m_pCommand->Field(nCol);

    sword ftype;
    bool bLong = false;
    switch (Field.FieldType())
    {
    case SA_dtUnknown:
        throw SAException(SA_Library_Error, SA_Library_Error_UnknownColumnType, -1, IDS_UNKNOWN_COLUMN_TYPE,
            (const SAChar*)Field.Name());
    case SA_dtShort:
        ftype = 3; // 16-bit signed integer
        break;
    case SA_dtUShort:
        ftype = 68; // 16-bit unsigned integer
        break;
    case SA_dtLong:
        ftype = 3; // 32-bit signed integer
        break;
    case SA_dtULong:
        ftype = 68; // 32-bit unsigned integer
        break;
    case SA_dtDouble:
        ftype = 4; // FLOAT
        break;
    case SA_dtNumeric:
        ftype = 6; // VARNUM
        break;
    case SA_dtDateTime:
        ftype = 12; // DATE
        break;
    case SA_dtBytes:
        ftype = 23; // RAW
        break;
    case SA_dtString:
        ftype = 1; // VARCHAR2
        break;
    case SA_dtLongBinary:
        bLong = true; // piecewise define
        ftype = 24; // LONG RAW
        break;
    case SA_dtLongChar:
        bLong = true; // piecewise define
        ftype = 8; // LONG
        break;
    case SA_dtCursor:
        ftype = 102; // CURSOR VARIABLE
        memset(pValue, 0, sizeof(Cda_Def));
        break;
    default:
        ftype = 0;
        assert(false);
        // unknown type
    }

    if (bLong)
    {
        LongContext_t *pLongContext = (LongContext_t *)pValue;
        pLongContext->pReader = &Field;
        pLongContext->pWriter = 0;
        pLongContext->pInd = (sb2*)pInd;

        if (m_bPiecewiseFetchAllowed)
        {
            // use piecewise only if this long column is the last column and the only long column
            if (WhichFieldIsPiecewise() == &Field)
            {
                ((Iora7Connection*)m_pISAConnection)->Check(
                    g_ora7API.odefinps(&m_handles.m_cda, 0, nCol,
                    (ub1*)pLongContext, SB4MAXVAL, ftype, 0,
                    (sb2*)pInd, NULL, 0, 0, (ub2*)pSize, NULL, 0,
                    0, 0, 0), &m_handles.m_cda);
                pLongContext->eState = LongContextPiecewiseDefine;
            }
            else
            {
                ((Iora7Connection*)m_pISAConnection)->Check(
                    g_ora7API.odefinps(&m_handles.m_cda, 1, nCol, NULL, 0,
                    ftype, 0, (sb2*)pInd, NULL, 0, 0, (ub2*)pSize,
                    NULL, 0, 0, 0, 0), &m_handles.m_cda);
                pLongContext->eState = LongContextNormal;
            }
        }
        else
        {
            ((Iora7Connection*)m_pISAConnection)->Check(
                g_ora7API.odefin(&m_handles.m_cda, nCol, (ub1*)NULL, 0,
                ftype, 0, // scale, not used
                (sb2*)pInd, (text*)NULL, 0, 0, // fmt, fmtl, fmtt, not used
                (ub2*)pSize, NULL), &m_handles.m_cda); // rcode not used
            pLongContext->eState = LongContextNormal;
        }
    }
    else
    {
        if (m_bPiecewiseFetchAllowed)
        {
            ((Iora7Connection*)m_pISAConnection)->Check(
                g_ora7API.odefinps(&m_handles.m_cda, 1, nCol, (ub1*)pValue,
                (sb4)nValueSize, ftype, 0, (sb2*)pInd, NULL, 0, 0,
                (ub2*)pSize, NULL, (sb4)nValueSize, // buf_skip
                (sb4)nIndSize, // ind_skip
                (sb4)nSizeSize, // len_skip
                0), &m_handles.m_cda);
        }
        else
        {
            ((Iora7Connection*)m_pISAConnection)->Check(
                g_ora7API.odefin(&m_handles.m_cda, nCol, (ub1*)pValue,
                (sb4)nValueSize, ftype, 0, // scale, not used
                (sb2*)pInd, (text*)NULL, 0, 0, // fmt, fmtl, fmtt, not used
                (ub2*)pSize, NULL), &m_handles.m_cda); // rcode not used
        }
    }
}

/*virtual */
void Iora7Cursor::SetSelectBuffers()
{
    SAString sOption = m_pCommand->Option(SACMD_PREFETCH_ROWS);
    if (!sOption.IsEmpty())
    {
        int cLongs = FieldCount(4, SA_dtLongBinary, SA_dtLongChar, SA_dtBLob,
            SA_dtCLob);
        if (cLongs) // do not use bulk fetch if there are long columns
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

    // use default helpers
    AllocSelectBuffer(sizeof(sb2), sizeof(ub2), m_cRowsToPrefetch);
}

void Iora7Cursor::DiscardPiecewiseFetch()
{
    sword rc;
    do
    {
        ub1 piece;
        dvoid *ctxp;
        ub4 iter, index;
        char buf[0xFFFF];
        ub4 len = sizeof(buf);

        ((Iora7Connection*)m_pISAConnection)->Check(
            g_ora7API.ogetpi(&m_handles.m_cda, &piece, &ctxp, &iter,
            &index), &m_handles.m_cda);

        ((Iora7Connection*)m_pISAConnection)->Check(
            g_ora7API.osetpi(&m_handles.m_cda, piece, buf, &len),
            &m_handles.m_cda);

        rc = g_ora7API.ofetch(&m_handles.m_cda);
    } while (m_handles.m_cda.rc == 3130);
    ((Iora7Connection*)m_pISAConnection)->Check(rc, &m_handles.m_cda);

    m_bPiecewiseFetchPending = false;
}

void Iora7Cursor::CheckPiecewiseNull(bool bAfterExecute)
{
    // to check if piecewise fetched column (output param) is null (or not)
    // we have to read at least one byte from piecewise stream
    ub1 piece;
    dvoid *ctxp;
    ub4 iter, index;

    sword rc;
    ((Iora7Connection*)m_pISAConnection)->Check(
        g_ora7API.ogetpi(&m_handles.m_cda, &piece, &ctxp, &iter, &index),
        &m_handles.m_cda);
    LongContext_t *pLongContext = (LongContext_t *)ctxp;
    pLongContext->Len = (ub4) sizeof(m_PiecewiseNullCheckPreFetch);
    ((Iora7Connection*)m_pISAConnection)->Check(
        g_ora7API.osetpi(&m_handles.m_cda, piece,
        &m_PiecewiseNullCheckPreFetch[0], &pLongContext->Len),
        &m_handles.m_cda);

    if (bAfterExecute)
        rc = g_ora7API.oexec(&m_handles.m_cda);
    else
        rc = g_ora7API.ofetch(&m_handles.m_cda);

    if (m_handles.m_cda.rc != 3130)
    {
        ((Iora7Connection*)m_pISAConnection)->Check(rc, &m_handles.m_cda);

        // cloumn was null or whole Long* was read
        m_bPiecewiseFetchPending = false;
    }

    // set null indicator manually
    // because Oracle set it only if null or whole read
    *(sb2*)pLongContext->pInd = (sb2)(pLongContext->Len ? 0 : -1);

    // if column was not null then fetched piece is saved
    // in m_PiecewiseNullCheckPreFetch, it's len in pLongContext->Len
    // and we will use it when actual read request happen
}

/*virtual */
bool Iora7Cursor::FetchNext()
{
    if (m_cRowsToPrefetch != 1)
        return FetchNextArray();

    if (m_bPiecewiseFetchPending)
        DiscardPiecewiseFetch();

    sword rc = g_ora7API.ofetch(&m_handles.m_cda);
    switch (m_handles.m_cda.rc)
    {
    case 1403:
        m_bResultSetExist = false;
        return false;
    case 3130: // the buffer for the next Piece to be fetched is required
        assert(m_bPiecewiseFetchAllowed);
        m_bPiecewiseFetchPending = true;
        CheckPiecewiseNull();
        break;
    default:
        ((Iora7Connection*)m_pISAConnection)->Check(rc, &m_handles.m_cda);
    }

    // use default helpers
    ConvertSelectBufferToFields(0);
    return true;
}

/*virtual */
bool Iora7Cursor::FetchNextArray()
{
    assert(!m_bPiecewiseFetchPending);

    if (m_cRowCurrent == m_cRowsObtained)
    {
        if (m_handles.m_cda.rc != 1403)
        {
            sword rows_done = m_handles.m_cda.rpc;
            sword rc = g_ora7API.ofen(&m_handles.m_cda, m_cRowsToPrefetch);
            m_cRowsObtained = m_handles.m_cda.rpc - rows_done;

            if (m_handles.m_cda.rc != 1403)
            {
                assert(m_handles.m_cda.rc != 3130);
                // the buffer for the next Piece to be fetched is required
                ((Iora7Connection*)m_pISAConnection)->Check(rc,
                    &m_handles.m_cda);
            }
        }
        else
            m_cRowsObtained = 0;

        m_cRowCurrent = 0;
    }

    if (m_cRowsObtained)
    {
        ++m_cRowCurrent;

        // use default helpers
        ConvertSelectBufferToFields(m_cRowCurrent - 1);
        return true;
    }

    m_bResultSetExist = false;
    return false;
}

void Iora8Cursor::DestroyLobsReturnBinding()
{
    while (m_nLobReturnBindsColCount)
    {
        while (m_nBLobBindsRowCount)
        {
            OCILobLocator *&pLoc =
                m_pppBLobReturningLocs[m_nLobReturnBindsColCount - 1][m_nBLobBindsRowCount
                - 1];
            if (pLoc)
            {
                g_ora8API.OCIDescriptorFree(pLoc, OCI_DTYPE_LOB);
                pLoc = NULL;
            }
            --m_nBLobBindsRowCount;
        }

        delete m_pppBLobReturningLocs[--m_nLobReturnBindsColCount];
        delete m_ppLobReturningLens[m_nLobReturnBindsColCount];
    }
    if (m_ppLobReturnPlaceHolders)
    {
        ::free(m_ppLobReturnPlaceHolders);
        m_ppLobReturnPlaceHolders = NULL;
    }
    delete m_pppBLobReturningLocs;
    m_pppBLobReturningLocs = NULL;
    delete m_ppLobReturningLens;
    m_ppLobReturningLens = NULL;
}

// There can be several reasons for reparsing (Prepare again)
// already prepared statement:
// 1) if we bind Lobs for update or insert and
//		temporary Lobs are not supported by client or server
//		then we have to add 'returning' clause
// 2) if statement was executed and we rebind with
//		different bind variable datatype (see ORA-01475 error)
void IoraCursor::CheckForReparseBeforeBinding(int/* nPlaceHolderCount*/,
    saPlaceHolder ** /*ppPlaceHolders*/)
{
    //  check if we need to reparse to avoid ORA-01475
    if (m_pDTY) // we have bound before
    {
        for (int i = 0; i < m_pCommand->ParamCount(); ++i)
        {
            SAParam &Param = m_pCommand->ParamByIndex(i);

            SADataType_t eDataType = Param.DataType();
            int dty = CnvtStdToNative(
                eDataType == SA_dtUnknown ? SA_dtString : eDataType);

            if (m_pDTY[i] != dty)
            {
                InternalPrepare(OraStatementSQL());
                break;
            }
        }
    }
}

void Iora8Cursor::CheckForReparseBeforeBinding(int nPlaceHolderCount,
    saPlaceHolder **ppPlaceHolders)
{
    // first check if we have to modify SQL statement and reparse
    // due to Lobs operations and lack of temporary Lobs support

    // quick check, if stmt type is not
    // Insert or Update then nothing to check
    if (m_nOraStmtType == OCI_STMT_UPDATE || m_nOraStmtType == OCI_STMT_INSERT)
    {
        SAString sOriginalStmt = OraStatementSQL();
        SAString sModifiedStmt;
        SAString sReturning;
        SAString sInto;

        saPlaceHolder **ppLobReturnPlaceHolders = NULL;
        int nLobReturnBindsColCount = 0;
        size_t nPos = 0l;
        for (int i = 0; i < nPlaceHolderCount; ++i)
        {
            sModifiedStmt += sOriginalStmt.Mid(nPos,
                ppPlaceHolders[i]->getStart() - nPos);

            bool bNull = ppPlaceHolders[i]->getParam()->isNull();
            if (!bNull && ppPlaceHolders[i]->getParam()->DataType() == SA_dtBLob
                && !((Iora8Connection*)m_pISAConnection)->IsTemporaryLobSupported())
            {
                // replace param marker with empty_blob() function
                sModifiedStmt += _TSA("empty_blob()");
                if (!sReturning.IsEmpty())
                    sReturning += _TSA(", ");
                sReturning += ppPlaceHolders[i]->getParam()->Name();
                if (!sInto.IsEmpty())
                    sInto += _TSA(", ");
                sInto += _TSA(":");
                sInto += ppPlaceHolders[i]->getParam()->Name();

                sa_realloc((void**)&ppLobReturnPlaceHolders,
                    sizeof(SAParam*) * (nLobReturnBindsColCount + 1));
                ppLobReturnPlaceHolders[nLobReturnBindsColCount] =
                    ppPlaceHolders[i];
                ++nLobReturnBindsColCount;
            }
            else if (!bNull
                && ppPlaceHolders[i]->getParam()->DataType() == SA_dtCLob
                && !((Iora8Connection*)m_pISAConnection)->IsTemporaryLobSupported())
            {
                // replace param marker with empty_clob() function
                sModifiedStmt += _TSA("empty_clob()");
                if (!sReturning.IsEmpty())
                    sReturning += _TSA(", ");
                sReturning += ppPlaceHolders[i]->getParam()->Name();
                if (!sInto.IsEmpty())
                    sInto += _TSA(", ");
                sInto += _TSA(":");
                sInto += ppPlaceHolders[i]->getParam()->Name();

                sa_realloc((void**)&ppLobReturnPlaceHolders,
                    sizeof(SAParam*) * (nLobReturnBindsColCount + 1));
                ppLobReturnPlaceHolders[nLobReturnBindsColCount] =
                    ppPlaceHolders[i];
                ++nLobReturnBindsColCount;
            }
            else
                // remain unmodified
                sModifiedStmt += sOriginalStmt.Mid(
                ppPlaceHolders[i]->getStart(),
                ppPlaceHolders[i]->getEnd()
                - ppPlaceHolders[i]->getStart() + 1);

            nPos = ppPlaceHolders[i]->getEnd() + 1;
        }
        // copy tail
        if (nPos < sOriginalStmt.GetLength())
            sModifiedStmt += sOriginalStmt.Mid(nPos);

        if (nLobReturnBindsColCount)
        {
            // add/alter returning into clauses
            bool bAdd = true;
            if (bAdd) // add
            {
                sModifiedStmt += _TSA(" returning ");
                sModifiedStmt += sReturning;
                sModifiedStmt += _TSA(" into ");
                sModifiedStmt += sInto;
            }
            else // modify
            {
            }
        }

        if (nLobReturnBindsColCount // we have returning clause
            || m_nLobReturnBindsColCount) // we had returning clause
        {
            try
            {
                InternalPrepare(sModifiedStmt);
            }
            catch (SAException &)
            {
                if (ppLobReturnPlaceHolders)
                    ::free(ppLobReturnPlaceHolders);
                throw;
            }
        }

        if (nLobReturnBindsColCount)
        {
            m_ppLobReturnPlaceHolders = ppLobReturnPlaceHolders;

            m_pppBLobReturningLocs =
                new OCILobLocator**[nLobReturnBindsColCount];
            memset(m_pppBLobReturningLocs, 0,
                sizeof(OCILobLocator**) * nLobReturnBindsColCount);
            m_ppLobReturningLens = new ub4*[nLobReturnBindsColCount];
            memset(m_ppLobReturningLens, 0,
                sizeof(ub4*) * nLobReturnBindsColCount);
            m_nLobReturnBindsColCount = nLobReturnBindsColCount;
        }
    }

    // then check if we need to reparse
    // in both OCI7 and OCI8
    IoraCursor::CheckForReparseBeforeBinding(nPlaceHolderCount, ppPlaceHolders);
}

/*virtual */
void Iora8Cursor::InternalPrepare(const SAString &sStmt)
{
    // save because (extraction from Oracle docs):
    // "The pointer to the text of the statement must be available
    // as long as the statement is
    // executed, or data is fetched from it."
    m_sInternalPrepareStmt = sStmt;
    SA_TRACE_CMD(m_sInternalPrepareStmt);
#ifdef SA_UNICODE
    if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
        Iora8Connection::Check(g_ora8API.OCIStmtPrepare(
        m_handles.m_pOCIStmt,
        m_handles.m_pOCIError,
        (CONST text*)m_sInternalPrepareStmt.GetUTF16Chars(),
        (ub4)m_sInternalPrepareStmt.GetUTF16CharsLength()*sizeof(utext),
        OCI_NTV_SYNTAX,
        OCI_DEFAULT), m_handles.m_pOCIError, OCI_HTYPE_ERROR);
    else
#endif
        Iora8Connection::Check(
        g_ora8API.OCIStmtPrepare(m_handles.m_pOCIStmt,
        m_handles.m_pOCIError,
        (CONST text*) m_sInternalPrepareStmt.GetMultiByteChars(),
        (ub4)m_sInternalPrepareStmt.GetMultiByteCharsLength(),
        OCI_NTV_SYNTAX, OCI_DEFAULT), m_handles.m_pOCIError,
        OCI_HTYPE_ERROR);

    m_nOraStmtType = 0; // unknown
    // no binds are in effect after parsing
    if (m_pDTY)
    {
        ::free(m_pDTY);
        m_pDTY = NULL;
    }
    DestroyLobsReturnBinding();

    // readStmtType
    Iora8Connection::Check(
        g_ora8API.OCIAttrGet(m_handles.m_pOCIStmt, OCI_HTYPE_STMT,
        &m_nOraStmtType, NULL, OCI_ATTR_STMT_TYPE,
        m_handles.m_pOCIError), m_handles.m_pOCIError,
        OCI_HTYPE_ERROR);
}

/*virtual */
void Iora8Cursor::Prepare(const SAString &/*sStmt*/,
    SACommandType_t/* eCmdType*/, int/* nPlaceHolderCount*/,
    saPlaceHolder** /*ppPlaceHolders*/)
{
    // always compile original to check errors
    InternalPrepare(OraStatementSQL());
}

// *ppLob should be allocated
void Iora8Cursor::CreateTemporaryLob(OCILobLocator **ppLob, SAParam &Param)
{
    if (!g_ora8API.OCILobCreateTemporary)
    {
        assert(false);
        return;
    }

    ub1 csfrm = SQLCS_IMPLICIT;
    if (0
        == Param.Option(_TSA("OCI_ATTR_CHARSET_FORM")).CompareNoCase(
        _TSA("SQLCS_NCHAR")))
        csfrm = SQLCS_NCHAR;

    ub1 lobtype;
    switch (Param.DataType())
    {
    case SA_dtBLob:
        lobtype = OCI_TEMP_BLOB;
        break;
    case SA_dtCLob:
        lobtype = OCI_TEMP_CLOB;
#ifdef SA_UNICODE
        if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
            csfrm = SQLCS_NCHAR;
#endif
        break;
    default:
        assert(false);
        return;
    }

    Iora8Connection::Check(
        g_ora8API.OCILobCreateTemporary(
        ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
        m_handles.m_pOCIError, *ppLob, OCI_DEFAULT, csfrm, lobtype,
        OCI_ATTR_NOCACHE, OCI_DURATION_SESSION),
        m_handles.m_pOCIError, OCI_HTYPE_ERROR);

    sa_realloc((void**)&m_ppTempLobs,
        (m_cTempLobs + 1) * sizeof(OCILobLocator *));
    m_ppTempLobs[m_cTempLobs] = *ppLob;
    ++m_cTempLobs;

    BindLob(*ppLob, Param);
}

void Iora8Cursor::SetCharSetOptions(const SAString &sOCI_ATTR_CHARSET_FORM,
    const SAString &sOCI_ATTR_CHARSET_ID, dvoid *trgthndlp, ub4 trghndltyp)
{
    if (!sOCI_ATTR_CHARSET_FORM.IsEmpty())
    {
        if (sOCI_ATTR_CHARSET_FORM.CompareNoCase(_TSA("SQLCS_IMPLICIT")) == 0)
        {
            ub1 form = SQLCS_IMPLICIT;
            Iora8Connection::Check(
                g_ora8API.OCIAttrSet(trgthndlp, trghndltyp, (dvoid *)&form,
                (ub4)0, (ub4)OCI_ATTR_CHARSET_FORM,
                m_handles.m_pOCIError), m_handles.m_pOCIError,
                OCI_HTYPE_ERROR);
        }
        else if (sOCI_ATTR_CHARSET_FORM.CompareNoCase(_TSA("SQLCS_NCHAR"))
            == 0)
        {
            ub1 form = SQLCS_NCHAR;
            Iora8Connection::Check(
                g_ora8API.OCIAttrSet(trgthndlp, trghndltyp, (dvoid *)&form,
                (ub4)0, (ub4)OCI_ATTR_CHARSET_FORM,
                m_handles.m_pOCIError), m_handles.m_pOCIError,
                OCI_HTYPE_ERROR);
        }
    }

    if (!sOCI_ATTR_CHARSET_ID.IsEmpty())
    {
        ub2 id = Iora8Connection::GetCharsetId(sOCI_ATTR_CHARSET_ID);

        Iora8Connection::Check(
            g_ora8API.OCIAttrSet(trgthndlp, trghndltyp, (dvoid *)&id,
            (ub4)0, (ub4)OCI_ATTR_CHARSET_ID,
            m_handles.m_pOCIError), m_handles.m_pOCIError,
            OCI_HTYPE_ERROR);
    }
}

void Iora8Cursor::Bind(int nPlaceHolderCount, saPlaceHolder** ppPlaceHolders)
{
    CheckForReparseBeforeBinding(nPlaceHolderCount, ppPlaceHolders);

    sa_realloc((void**)&m_pDTY, sizeof(int) * m_pCommand->ParamCount());

    // we should bind all params, not place holders in Oracle
    AllocBindBuffer(sizeof(sb2), sizeof(ub2));
    void *pBuf = m_pParamBuffer;

    ub4 nLobReturnBindsColCount = 0;

    for (int i = 0; i < m_pCommand->ParamCount(); ++i)
    {
        SAParam &Param = m_pCommand->ParamByIndex(i);

        void *pInd;
        void *pSize;
        size_t nDataBufSize;
        void *pValue;
        IncParamBuffer(pBuf, pInd, pSize, nDataBufSize, pValue);

        if (Param.isDefault())
            continue;

        SADataType_t eDataType = Param.DataType();
        ub2 dty = (ub2)CnvtStdToNative(
            eDataType == SA_dtUnknown ? SA_dtString : eDataType);
        m_pDTY[i] = (int)dty;

        sb2 *indp = (sb2 *)pInd; // bind null indicator
        dvoid *valuep = pValue;
        sb4 value_sz = (sb4)nDataBufSize; // allocated
        ub2 *alenp = (ub2 *)pSize;

        // this will be adjusted if Param is input
        *alenp = (ub2)nDataBufSize;

        // special code for ref cursors
        if (eDataType == SA_dtCursor)
        {
            *indp = OCI_IND_NOTNULL; // field is not null

            assert(nDataBufSize == sizeof(OCIStmt *));
            Param.setAsCursor()->setConnection(m_pCommand->Connection());
            *((OCIStmt**)valuep) =
                ((ora8CommandHandles*)Param.asCursor()->NativeHandles())->m_pOCIStmt;
        }
        else
        {
            *indp = OCI_IND_NULL;
            if (isInputParam(Param))
            {
                if (Param.isNull())
                    *indp = OCI_IND_NULL; // field is null
                else
                    *indp = OCI_IND_NOTNULL; // field is not null

                *alenp = (ub2)InputBufferSize(Param);
                assert(value_sz >= *alenp);

                if (!Param.isNull())
                {
                    switch (eDataType)
                    {
                    case SA_dtUnknown:
                        throw SAException(SA_Library_Error, SA_Library_Error_UnknownParameterType, -1,
                            IDS_UNKNOWN_PARAMETER_TYPE,
                            (const SAChar*)Param.Name());
                    case SA_dtBool:
                        // there is no "native" boolean type in Oracle,
                        // so treat boolean as 16-bit signed INTEGER in Oracle
                        assert(*alenp == sizeof(short));
                        *(short*)valuep = (short)Param.asBool();
                        break;
                    case SA_dtShort:
                        assert(*alenp == sizeof(short));
                        *(short*)valuep = Param.asShort();
                        break;
                    case SA_dtUShort:
                        assert(*alenp == sizeof(unsigned short));
                        *(unsigned short*)valuep = Param.asUShort();
                        break;
                    case SA_dtLong:
                        assert(*alenp == sizeof(long));
                        *(long*)valuep = Param.asLong();
                        break;
                    case SA_dtULong:
                        assert(*alenp == sizeof(unsigned long));
                        *(unsigned long*)valuep = Param.asULong();
                        break;
                    case SA_dtDouble:
                        assert(*alenp == sizeof(double));
                        *(double*)valuep = Param.asDouble();
                        break;
                    case SA_dtNumeric:
                        assert(*alenp == sizeof(OraVARNUM_t));
                        // Oracle internal number representation
                        IoraConnection::CnvtNumericToInternal(Param.asNumeric(),
                            *(OraVARNUM_t*)valuep);
                        break;
                    case SA_dtDateTime:
                        if (((Iora8Connection*)m_pISAConnection)->m_bUseTimeStamp
                            && (dty == SQLT_TIMESTAMP || dty == SQLT_TIMESTAMP_TZ)
                            && *alenp != sizeof(OraDATE_t))
                        {
                            AllocDateTimeParam((OCIDateTime**)valuep);
                            ((Iora8Connection*)m_pISAConnection)->CnvtDateTimeToInternal(
                                Param.asDateTime(),
                                *((OCIDateTime**)valuep));
                        }
                        else
                        {
                            assert(*alenp == sizeof(OraDATE_t));
                            // Oracle internal date/time representation
                            IoraConnection::CnvtDateTimeToInternal(
                                Param.asDateTime(), (OraDATE_t*)valuep);
                        }
                        break;
                    case SA_dtString:
#ifdef SA_UNICODE
                        if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
                        {
                            assert(*alenp == (ub2)(Param.asString().GetUTF16CharsLength()*sizeof(utext)));
                            memcpy(valuep, Param.asString().GetUTF16Chars(), *alenp);
                        }
                        else
#endif
                        {
                            assert(
                                *alenp == (ub2)Param.asString().GetMultiByteCharsLength());
                            memcpy(valuep, Param.asString().GetMultiByteChars(),
                                *alenp);
                        }
                        break;
                    case SA_dtBytes:
                        assert(
                            *alenp == (ub2)Param.asBytes().GetBinaryLength());
                        memcpy(valuep, (const void*)Param.asBytes(), *alenp);
                        break;
                    case SA_dtLongBinary:
                        assert(*alenp == sizeof(LongContext_t));
                        break;
                    case SA_dtLongChar:
                        assert(*alenp == sizeof(LongContext_t));
                        break;
                    case SA_dtBLob:
                    case SA_dtCLob:
                        assert(
                            *alenp == sizeof(OCILobLocator *) || *alenp == sizeof(BlobReturningContext_t));
                        break;
                    case SA_dtCursor:
                        assert(false);
                        // already handled
                        break;
                    default:
                        assert(false);
                    }
                }
            }
        }

        bool bLongPiecewise = isLong(eDataType);
        bool bLob = isLob(eDataType);
        bool bLobReturning =
            bLob
            && (m_nOraStmtType == OCI_STMT_UPDATE
            || m_nOraStmtType == OCI_STMT_INSERT)
            && !Param.isNull()
            && !((Iora8Connection*)m_pISAConnection)->IsTemporaryLobSupported();

        OCIBind* pOCIBind = NULL;
        if (bLongPiecewise)
        {
#ifdef SA_UNICODE
            if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
                Iora8Connection::Check(g_ora8API.OCIBindByName(
                m_handles.m_pOCIStmt,
                &pOCIBind,
                m_handles.m_pOCIError,
                (CONST text*)Param.Name().GetUTF16Chars(),
                (sb4)Param.Name().GetUTF16CharsLength()*sizeof(utext),
                valuep, SB4MAXVAL, dty,
                indp, NULL,
                NULL, 0, NULL, OCI_DATA_AT_EXEC), m_handles.m_pOCIError, OCI_HTYPE_ERROR);
            else
#endif
                Iora8Connection::Check(
                g_ora8API.OCIBindByName(m_handles.m_pOCIStmt, &pOCIBind,
                m_handles.m_pOCIError,
                (CONST text*) Param.Name().GetMultiByteChars(),
                (sb4)Param.Name().GetLength(), valuep, SB4MAXVAL,
                dty, indp, NULL, NULL, 0, NULL, OCI_DATA_AT_EXEC),
                m_handles.m_pOCIError, OCI_HTYPE_ERROR);

            LongContext_t *pLongContext = (LongContext_t *)pValue;
            pLongContext->pReader = &Param;
            pLongContext->pWriter = &Param;
            pLongContext->pInd = indp;

            OCICallbackInBind icbfp = &LongInBind; // always assign InBind, otherwise OutBind is not used and Oracle do piecewise instead of callbacks
            OCICallbackOutBind ocbfp = NULL;
            if (isOutputParam(Param))
                ocbfp = &LongOutBind;
            Iora8Connection::Check(
                g_ora8API.OCIBindDynamic(pOCIBind, m_handles.m_pOCIError,
                pLongContext, icbfp, pLongContext, ocbfp),
                m_handles.m_pOCIError, OCI_HTYPE_ERROR);
            pLongContext->eState = LongContextCallback;
        }
        else if (bLob)
        {
            if (bLobReturning)
            {
                // bind Lobs returning into
                assert(m_nLobReturnBindsColCount);
#ifdef SA_UNICODE
                if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
                    Iora8Connection::Check(g_ora8API.OCIBindByName(
                    m_handles.m_pOCIStmt,
                    &pOCIBind,
                    m_handles.m_pOCIError,
                    (CONST text*)Param.Name().GetUTF16Chars(), (sb4)Param.Name().GetUTF16CharsLength()*sizeof(utext),
                    NULL, (sb4)sizeof(OCILobLocator*), dty,
                    NULL, NULL,
                    NULL, 0, NULL, OCI_DATA_AT_EXEC), m_handles.m_pOCIError, OCI_HTYPE_ERROR);
                else
#endif
                    Iora8Connection::Check(
                    g_ora8API.OCIBindByName(m_handles.m_pOCIStmt, &pOCIBind,
                    m_handles.m_pOCIError,
                    (CONST text*) Param.Name().GetMultiByteChars(),
                    (sb4)Param.Name().GetLength(), NULL,
                    (sb4) sizeof(OCILobLocator*), dty, NULL, NULL,
                    NULL, 0, NULL, OCI_DATA_AT_EXEC),
                    m_handles.m_pOCIError, OCI_HTYPE_ERROR);

                BlobReturningContext_t *pCtx = (BlobReturningContext_t *)valuep;
                pCtx->pParam = &Param;
                pCtx->pOCIError = m_handles.m_pOCIError;
                pCtx->pOCIEnv =
                    ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv;
                pCtx->pppOCILobLocators = m_pppBLobReturningLocs;
                pCtx->ppLobLocatorsLens = m_ppLobReturningLens;
                pCtx->nLobLocatorCol = nLobReturnBindsColCount++;
                pCtx->pnLobReturnBindsColCount = &m_nLobReturnBindsColCount;
                pCtx->pnBLobBindsRowCount = &m_nBLobBindsRowCount;
                Iora8Connection::Check(
                    g_ora8API.OCIBindDynamic(pOCIBind,
                    m_handles.m_pOCIError, NULL,
                    &LobReturningInBind, pCtx,
                    &LobReturningOutBind), m_handles.m_pOCIError,
                    OCI_HTYPE_ERROR);
            }
            else
            {
                Iora8Connection::Check(
                    g_ora8API.OCIDescriptorAlloc(
                    ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
                    (dvoid **)valuep, OCI_DTYPE_LOB, 0, NULL),
                    ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
                    OCI_HTYPE_ENV);

                if (!Param.isNull())
                    CreateTemporaryLob((OCILobLocator **)valuep, Param);

#ifdef SA_UNICODE
                if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
                    Iora8Connection::Check(g_ora8API.OCIBindByName(
                    m_handles.m_pOCIStmt,
                    &pOCIBind,
                    m_handles.m_pOCIError,
                    (CONST text*)Param.Name().GetUTF16Chars(),
                    (sb4)Param.Name().GetUTF16CharsLength()*sizeof(utext),
                    valuep, value_sz, dty,
                    indp, alenp,
                    NULL, 0, NULL, OCI_DEFAULT), m_handles.m_pOCIError, OCI_HTYPE_ERROR);
                else
#endif
                    Iora8Connection::Check(
                    g_ora8API.OCIBindByName(m_handles.m_pOCIStmt, &pOCIBind,
                    m_handles.m_pOCIError,
                    (CONST text*) Param.Name().GetMultiByteChars(),
                    (sb4)Param.Name().GetLength(), valuep,
                    value_sz, dty, indp, alenp, NULL, 0, NULL,
                    OCI_DEFAULT), m_handles.m_pOCIError,
                    OCI_HTYPE_ERROR);
        }
    }
        else
        {
            switch (eDataType)
            {
            case SA_dtString:
            case SA_dtBytes:
                break;
            case SA_dtDateTime: // we should allocate the timestamp handle for return parameters
                if (!(isInputParam(Param) && !Param.isNull())
                    && ((Iora8Connection*)m_pISAConnection)->m_bUseTimeStamp
                    && (dty == SQLT_TIMESTAMP || dty == SQLT_TIMESTAMP_TZ)
                    && *alenp != sizeof(OraDATE_t))
                    AllocDateTimeParam((OCIDateTime**)valuep);
                break;
            default:
                // to avoid Ora-03135 or Ora-01460
                value_sz = *alenp;
                alenp = NULL;
            }

#ifdef SA_UNICODE
            if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
                Iora8Connection::Check(g_ora8API.OCIBindByName(
                m_handles.m_pOCIStmt,
                &pOCIBind,
                m_handles.m_pOCIError,
                (CONST text*)Param.Name().GetUTF16Chars(),
                (sb4)Param.Name().GetUTF16CharsLength()*sizeof(utext),
                valuep, value_sz, dty,
                indp, alenp,
                NULL, 0, NULL, OCI_DEFAULT), m_handles.m_pOCIError, OCI_HTYPE_ERROR);
            else
#endif
                Iora8Connection::Check(
                g_ora8API.OCIBindByName(m_handles.m_pOCIStmt, &pOCIBind,
                m_handles.m_pOCIError,
                (CONST text*) Param.Name().GetMultiByteChars(),
                (sb4)Param.Name().GetLength(), valuep, value_sz,
                dty, indp, alenp, NULL, 0, NULL, OCI_DEFAULT),
                m_handles.m_pOCIError, OCI_HTYPE_ERROR);
            }

        assert(pOCIBind);

        // if there are known options for this parameter, set them
        SAString sOCI_ATTR_CHARSET_FORM =
#ifdef SA_UNICODE
            ((Iora8Connection*)m_pISAConnection)->m_bUseUCS2 ? SAString(_TSA("SQLCS_NCHAR")) :
#endif
            Param.Option(_TSA("OCI_ATTR_CHARSET_FORM"));
        SetCharSetOptions(sOCI_ATTR_CHARSET_FORM,
            Param.Option(_TSA("OCI_ATTR_CHARSET_ID")), pOCIBind,
            OCI_HTYPE_BIND);
        }

    assert(nLobReturnBindsColCount == m_nLobReturnBindsColCount);
}

void Iora8Cursor::AllocDateTimeParam(OCIDateTime** ppParam)
{
    Iora8Connection::Check(
        g_ora8API.OCIDescriptorAlloc(
        ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
        (dvoid**)ppParam, OCI_DTYPE_TIMESTAMP_TZ, 0, NULL),
        ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
        OCI_HTYPE_ENV);

    sa_realloc((void**)&m_ppDateTimeParams,
        (m_cDateTimeParams + 1) * sizeof(OCIDateTime *));
    m_ppDateTimeParams[m_cDateTimeParams] = *ppParam;
    ++m_cDateTimeParams;
}

/*virtual */
void Iora8Cursor::UnExecute()
{
    // free OCI allocators
    FreeTemporaryLobsIfAny();
    FreeDateTimeParamsIfAny();

    for (int nField = 1;
        m_pCommand->m_bSelectBuffersSet
        && nField <= m_pCommand->FieldCount(); ++nField)
    {
        SAField &Field = m_pCommand->Field(nField);
        size_t nDataBufSize;
        void *pValue;

        GetFieldBuffer(nField, pValue, nDataBufSize);

        if (NULL == pValue || 0 == nDataBufSize)
            continue;

        ub4 i;

        switch (Field.FieldType())
        {
        case SA_dtDateTime:
            if (((Iora8Connection*)m_pISAConnection)->m_bUseTimeStamp
                && nDataBufSize != sizeof(OraDATE_t))
            {
                ub4 dtype = OCI_DTYPE_TIMESTAMP;

                if (Field.FieldNativeType() == SQLT_TIME_TZ ||
                    Field.FieldNativeType() == SQLT_TIMESTAMP_TZ ||
                    Field.FieldNativeType() == SQLT_TIMESTAMP_LTZ)
                    dtype = OCI_DTYPE_TIMESTAMP_TZ;

                for (i = 0; i < m_cRowsToPrefetch; ++i)
                    g_ora8API.OCIDescriptorFree((dvoid*)*((OCIDateTime**)pValue + i), dtype);
            }
            break;
        case SA_dtBLob:
        case SA_dtCLob:
            if (Field.FieldNativeType() == SQLT_BFILE)
            {
                for (i = 0; i < m_cRowsToPrefetch; ++i)
                    g_ora8API.OCIDescriptorFree(
                    (dvoid*)*((OCILobLocator**)pValue + i),
                    OCI_DTYPE_FILE);
            }
            else
            {
                for (i = 0; i < m_cRowsToPrefetch; ++i)
                {
                    FreeLobIfTemporary(*((OCILobLocator**)pValue + i));
                    g_ora8API.OCIDescriptorFree(
                        (dvoid*)*((OCILobLocator**)pValue + i),
                        OCI_DTYPE_LOB);
                }
            }
            break;
        case SA_dtCursor:
            if (NULL != Field.asCursor())
                Field.asCursor()->UnExecute();
            break;
        default:
            break;
        }
    }

    m_bResultSetCanBe = false;
}

/*virtual */
void Iora8Cursor::Execute(int nPlaceHolderCount, saPlaceHolder **ppPlaceHolders)
{
    if (m_pCommand->ParamCount() > 0)
        Bind(nPlaceHolderCount, ppPlaceHolders);

    try
    {
        // check for bulk reading
        SAString sOption = m_pCommand->Option(SACMD_PREFETCH_ROWS);
        if (!sOption.IsEmpty())
        {
            ub4 prefetch = (ub4)sa_toi(sOption); // prefetch rows count
            if (prefetch)
                Iora8Connection::Check(
                g_ora8API.OCIAttrSet(m_handles.m_pOCIStmt,
                OCI_HTYPE_STMT, &prefetch, 0,
                OCI_ATTR_PREFETCH_ROWS, m_handles.m_pOCIError),
                m_handles.m_pOCIError, OCI_HTYPE_ERROR,
                m_handles.m_pOCIStmt);
        }
        
        sOption = m_pCommand->Option(_TSA("OCI_ATTR_PREFETCH_ROWS"));
        if (!sOption.IsEmpty())
        {
            ub4 prefetch = (ub4)sa_toi(sOption); // prefetch rows count
            Iora8Connection::Check(
                g_ora8API.OCIAttrSet(m_handles.m_pOCIStmt,
                OCI_HTYPE_STMT, &prefetch, 0,
                OCI_ATTR_PREFETCH_ROWS, m_handles.m_pOCIError),
                m_handles.m_pOCIError, OCI_HTYPE_ERROR,
                m_handles.m_pOCIStmt);
        }

        sOption = m_pCommand->Option(_TSA("OCI_ATTR_PREFETCH_MEMORY"));
        if (!sOption.IsEmpty())
        {
            ub4 prefetch = (ub4)sa_toi(sOption); // prefetch rows count
            Iora8Connection::Check(
                g_ora8API.OCIAttrSet(m_handles.m_pOCIStmt,
                OCI_HTYPE_STMT, &prefetch, 0,
                OCI_ATTR_PREFETCH_MEMORY, m_handles.m_pOCIError),
                m_handles.m_pOCIError, OCI_HTYPE_ERROR,
                m_handles.m_pOCIStmt);
        }


        ub4 mode = OCI_DEFAULT;

        if (isSetScrollable())
        {
            mode |= OCI_STMT_SCROLLABLE_READONLY;

            ub4 prefetch = (ub4)1; // prefetch rows count = 1 when scrollable
            Iora8Connection::Check(
                g_ora8API.OCIAttrSet(m_handles.m_pOCIStmt, OCI_HTYPE_STMT,
                &prefetch, 0, OCI_ATTR_PREFETCH_ROWS,
                m_handles.m_pOCIError), m_handles.m_pOCIError,
                OCI_HTYPE_ERROR, m_handles.m_pOCIStmt);
        }

        if (m_pCommand->Connection()->AutoCommit() == SA_AutoCommitOn)
            mode |= OCI_COMMIT_ON_SUCCESS;

        sword rc = g_ora8API.OCIStmtExecute(
            ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
            m_handles.m_pOCIStmt, m_handles.m_pOCIError,
            m_nOraStmtType == OCI_STMT_SELECT ? 0 : 1, 0, (OCISnapshot*)NULL,
            (OCISnapshot*)NULL, mode);

        Iora8Connection::Check(rc, m_handles.m_pOCIError, OCI_HTYPE_ERROR,
            m_handles.m_pOCIStmt);

        if (m_nLobReturnBindsColCount)
            BindReturningLobs();

        m_bResultSetCanBe = true;

        ConvertOutputParams(); // if any
    }
    catch (SAException &) // clean-up
    {
        try
        {
            FreeTemporaryLobsIfAny();
            FreeDateTimeParamsIfAny();
        }
        catch (SAException &)
        {
        }
        throw;
    }
}

/*virtual */
void Iora8Cursor::Cancel()
{
    Iora8Connection::Check(
        g_ora8API.OCIBreak(
        ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
        m_handles.m_pOCIError), m_handles.m_pOCIError,
        OCI_HTYPE_ERROR);
}

// returns total size written
ub4 Iora8Cursor::BindLob(OCILobLocator *pLob, SAParam &Param)
{
    size_t nActualWrite;
    SAPieceType_t ePieceType = SA_FirstPiece;
    void *pBuf;

    ub4 offset = 1;
    ub1 csfrm = SQLCS_IMPLICIT;
    if (0
        == Param.Option(_TSA("OCI_ATTR_CHARSET_FORM")).CompareNoCase(
        _TSA("SQLCS_NCHAR")))
        csfrm = SQLCS_NCHAR;
    SADummyConverter DummyConverter;
    ISADataConverter *pIConverter = &DummyConverter;
    size_t nCnvtSize, nCnvtPieceSize = 0l;
    SAPieceType_t eCnvtPieceType;
#ifdef SA_UNICODE
    bool bUnicode = ((Iora8Connection*)m_pISAConnection)->m_bUseUCS2 &&
        (Param.ParamType() == SA_dtCLob || Param.DataType() == SA_dtCLob);
    if (bUnicode)
        csfrm = SQLCS_NCHAR;
#ifndef SQLAPI_WINDOWS
    SAUnicodeUTF16Converter UnicodeUtf2Converter;
    if (bUnicode)
        pIConverter = &UnicodeUtf2Converter;
#endif
#endif

    while ((nActualWrite = Param.InvokeWriter(ePieceType, SB4MAXVAL, pBuf)) != 0)
    {
        pIConverter->PutStream((unsigned char*)pBuf, nActualWrite, ePieceType);
        // smart while: initialize nCnvtPieceSize before calling pIConverter->GetStream
        while (nCnvtPieceSize = nActualWrite, pIConverter->GetStream(
            (unsigned char*)pBuf, nCnvtPieceSize, nCnvtSize,
            eCnvtPieceType) )
        {
            ub4 amt = (ub4)(
#ifdef SA_UNICODE
                bUnicode ? (nCnvtSize / sizeof(utext)) :
#endif
                nCnvtSize);
            Iora8Connection::Check(
                g_ora8API.OCILobWrite(
                ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
                m_handles.m_pOCIError, pLob, &amt, offset, pBuf,
                (ub4)nCnvtSize, OCI_ONE_PIECE, NULL, NULL, (ub2)(
#ifdef SA_UNICODE
                bUnicode ? OCI_UCS2ID :
#endif
                0), csfrm), m_handles.m_pOCIError, OCI_HTYPE_ERROR);

            offset += amt;
        }

        if (ePieceType == SA_LastPiece)
            break;
    }

    return (offset - 1);
}

void Iora8Cursor::BindReturningLobs()
{
    for (unsigned int nCol = 0; nCol < m_nLobReturnBindsColCount; ++nCol)
    {
        SAParam &Param = *m_ppLobReturnPlaceHolders[nCol]->getParam();

        ub4 nLobSize = 0;

        for (unsigned int nRow = 0; nRow < m_nBLobBindsRowCount; ++nRow)
        {
            if (nRow == 0)
                nLobSize = BindLob(m_pppBLobReturningLocs[nCol][0], Param);
            else
            {
                Iora8Connection::Check(
                    g_ora8API.OCILobCopy(
                    ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
                    m_handles.m_pOCIError,
                    m_pppBLobReturningLocs[nCol][nRow],
                    m_pppBLobReturningLocs[nCol][0], nLobSize, 1,
                    1), m_handles.m_pOCIError, OCI_HTYPE_ERROR);
            }
        }
    }
}

/*static */
sb4 Iora8Cursor::LongInBind(dvoid *ictxp, OCIBind * /*bindp*/, ub4/* iter*/,
    ub4/* index*/, dvoid **bufpp, ub4 *alenp, ub1 *piecep, dvoid **indpp)
{
    LongContext_t *pLongContext = (LongContext_t*)ictxp;

    SADummyConverter DummyConverter;
    ISADataConverter *pIConverter = &DummyConverter;
    SAPieceType_t eCnvtPieceType;
#if defined(SA_UNICODE) && ! defined(SQLAPI_WINDOWS)
    SAUnicodeUTF16Converter UnicodeUtf2Converter;
    if (pLongContext->pWriter->DataType() == SA_dtLongChar)
        pIConverter = &UnicodeUtf2Converter;
#endif

    if (!isInputParam(*(SAParam*)pLongContext->pWriter))
    {
        *pLongContext->pInd = -1;
        *bufpp = NULL;
        *alenp = 0;
        *piecep = OCI_ONE_PIECE;
        *indpp = pLongContext->pInd;
        return OCI_CONTINUE;
    }

    SAPieceType_t ePieceType;
    if (*piecep == OCI_FIRST_PIECE)
        ePieceType = SA_FirstPiece;
    else if (*piecep == OCI_NEXT_PIECE)
        ePieceType = SA_NextPiece;
    else
        assert(false);

    void* pBuf;
    size_t nCnvtSize, nActualWrite = pLongContext->pWriter->InvokeWriter(
        ePieceType, SB4MAXVAL, pBuf);

    eCnvtPieceType = SA_OnePiece;
    pIConverter->PutStream((unsigned char*)pBuf, nActualWrite, eCnvtPieceType);
    eCnvtPieceType = SA_OnePiece;
    nCnvtSize = nActualWrite;
    pIConverter->GetStream((unsigned char*)pBuf, nActualWrite, nCnvtSize,
        eCnvtPieceType);
    *alenp = (ub4)nCnvtSize;

    *bufpp = pBuf;

    if (!*alenp || ePieceType == SA_LastPiece)
        *piecep = OCI_LAST_PIECE;
    if (!*alenp)
        *bufpp = NULL;

    return OCI_CONTINUE;
}

/*static */
sb4 Iora8Cursor::LongOutBind(dvoid *octxp, OCIBind * /*bindp*/, ub4/* iter*/,
    ub4/* index*/, dvoid **bufpp, ub4 **alenpp, ub1 *piecep, dvoid **indpp,
    ub2 ** /*rcodep*/)
{
    return LongDefineOrOutBind(octxp, bufpp, alenpp, piecep, indpp);
}

/*static */
sb4 Iora8Cursor::LobReturningInBind(dvoid * /*ictxp*/, OCIBind * /*bindp*/,
    ub4/* iter*/, ub4/* index*/, dvoid **bufpp, ub4 *alenp, ub1 *piecep,
    dvoid **indpp)
{
    *bufpp = NULL;
    *alenp = 0;
    *piecep = OCI_ONE_PIECE;
    *indpp = &stat_m_BLobReturningNullInd;

    return OCI_CONTINUE;
}

/*static */
sb4 Iora8Cursor::LobReturningOutBind(dvoid *octxp, OCIBind *bindp, ub4/* iter*/,
    ub4 index, dvoid **bufpp, ub4 **alenp, ub1 *piecep, dvoid ** /*indpp*/,
    ub2 ** /*rcodep*/)
{
    BlobReturningContext_t *pCtx = (BlobReturningContext_t *)octxp;
    if (index == 0 && pCtx->nLobLocatorCol == 0)
    {
        assert(*pCtx->pnBLobBindsRowCount == 0);
        Iora8Connection::Check(
            g_ora8API.OCIAttrGet(bindp, OCI_HTYPE_BIND,
            pCtx->pnBLobBindsRowCount, NULL, OCI_ATTR_ROWS_RETURNED,
            pCtx->pOCIError), pCtx->pOCIError, OCI_HTYPE_ERROR);

        for (unsigned int nCol = 0; nCol < *pCtx->pnLobReturnBindsColCount;
            ++nCol)
        {
            assert(pCtx->pppOCILobLocators[nCol] == NULL);
            pCtx->pppOCILobLocators[nCol] =
                new OCILobLocator*[*pCtx->pnBLobBindsRowCount];
            memset(pCtx->pppOCILobLocators[nCol], 0,
                sizeof(OCILobLocator*) * *pCtx->pnBLobBindsRowCount);
            assert(pCtx->ppLobLocatorsLens[nCol] == NULL);
            pCtx->ppLobLocatorsLens[nCol] = new ub4[*pCtx->pnBLobBindsRowCount];
            // allocate Lob locators for all rows in this bind
            for (unsigned int nRow = 0; nRow < *pCtx->pnBLobBindsRowCount;
                nRow++)
            {
                Iora8Connection::Check(
                    g_ora8API.OCIDescriptorAlloc(pCtx->pOCIEnv,
                    (dvoid**)&pCtx->pppOCILobLocators[nCol][nRow],
                    OCI_DTYPE_LOB, 0, NULL), pCtx->pOCIEnv,
                    OCI_HTYPE_ENV);
                pCtx->ppLobLocatorsLens[nCol][nRow] =
                    (ub4) sizeof(OCILobLocator*);
            }
        }
    }

    *bufpp = pCtx->pppOCILobLocators[pCtx->nLobLocatorCol][index];
    *alenp = &pCtx->ppLobLocatorsLens[pCtx->nLobLocatorCol][index];
    *piecep = OCI_ONE_PIECE;

    return OCI_CONTINUE;
}

bool Iora8Cursor::ResultSetExists()
{
    if (!m_bResultSetCanBe)
        return false;

    switch (m_nOraStmtType)
    {
    case OCI_STMT_SELECT:
        return true;
    case OCI_STMT_UPDATE:
        return false;
    case OCI_STMT_DELETE:
        return false;
    case OCI_STMT_INSERT:
        return false;
    case OCI_STMT_CREATE:
    case OCI_STMT_DROP:
    case OCI_STMT_ALTER:
        return false;
    case OCI_STMT_BEGIN:
    case OCI_STMT_DECLARE:
        return false;
    }

    return false;
}

/*virtual */
SADataType_t Iora8Cursor::CnvtNativeToStd(int dbtype, int dbsubtype, int dbsize,
    int prec, int scale) const
{
    switch (dbtype)
    {
    case SQLT_CLOB: // character lob
        return SA_dtCLob;
    case SQLT_BLOB: // binary lob
        return SA_dtBLob;
    case SQLT_BFILEE: // binary file lob
        return SA_dtBLob;
    case SQLT_CFILEE: // character file lob
        return SA_dtCLob;
    case SQLT_RSET: // result set type
        return SA_dtCursor;
    case SQLT_DATE:
    case SQLT_TIME:
    case SQLT_TIME_TZ:
    case SQLT_TIMESTAMP:
    case SQLT_TIMESTAMP_TZ:
    case SQLT_TIMESTAMP_LTZ:
        return SA_dtDateTime;
    case SQLT_INTERVAL_YM:
    case SQLT_INTERVAL_DS:
        return SA_dtString;

    default:
        break;
    }

    return IoraCursor::CnvtNativeToStd(dbtype, dbsubtype, dbsize, prec, scale);
}

/*virtual */
void Iora8Cursor::DescribeFields(DescribeFields_cb_t fn)
{
    // get col count
    ub4 cFields;
    Iora8Connection::Check(
        g_ora8API.OCIAttrGet(m_handles.m_pOCIStmt, OCI_HTYPE_STMT, &cFields,
        NULL, OCI_ATTR_PARAM_COUNT, m_handles.m_pOCIError),
        m_handles.m_pOCIError, OCI_HTYPE_ERROR);

#ifdef SA_UNICODE
    utext *sColNameWide = NULL;
#endif
    char *sColName = NULL;
    ub4 nColNameLen;
    ub2 dbsize;
    ub2 dbtype;
    sb2 prec;
    sb1 scale;
    ub1 isnull;

    for (ub4 iField = 1; iField <= cFields; ++iField)
    {
        OCIParam *pOCIParam = NULL;
        Iora8Connection::Check(
            g_ora8API.OCIParamGet(m_handles.m_pOCIStmt, OCI_HTYPE_STMT,
            m_handles.m_pOCIError, (dvoid**)&pOCIParam, iField),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);

#ifdef SA_UNICODE
        if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
            Iora8Connection::Check(g_ora8API.OCIAttrGet(pOCIParam, OCI_DTYPE_PARAM,
            (dvoid**)&sColNameWide, &nColNameLen, OCI_ATTR_NAME, m_handles.m_pOCIError),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        else
#endif
            Iora8Connection::Check(
            g_ora8API.OCIAttrGet(pOCIParam, OCI_DTYPE_PARAM,
            (dvoid**)&sColName, &nColNameLen, OCI_ATTR_NAME,
            m_handles.m_pOCIError), m_handles.m_pOCIError,
            OCI_HTYPE_ERROR);
        Iora8Connection::Check(
            g_ora8API.OCIAttrGet(pOCIParam, OCI_DTYPE_PARAM, &dbtype, NULL,
            OCI_ATTR_DATA_TYPE, m_handles.m_pOCIError),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        ub4 char_semantics = 0;
        sword status = g_ora8API.OCIAttrGet(pOCIParam, OCI_DTYPE_PARAM,
            &char_semantics, NULL, OCI_ATTR_CHAR_USED,
            m_handles.m_pOCIError);
        if ((OCI_SUCCESS == status || OCI_SUCCESS_WITH_INFO == status)
            && char_semantics > 0)
            Iora8Connection::Check(
            g_ora8API.OCIAttrGet(pOCIParam, OCI_DTYPE_PARAM, &dbsize,
            NULL, OCI_ATTR_CHAR_SIZE, m_handles.m_pOCIError),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        else
            Iora8Connection::Check(
            g_ora8API.OCIAttrGet(pOCIParam, OCI_DTYPE_PARAM, &dbsize,
            NULL, OCI_ATTR_DATA_SIZE, m_handles.m_pOCIError),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        Iora8Connection::Check(
            g_ora8API.OCIAttrGet(pOCIParam, OCI_DTYPE_PARAM, &prec, NULL,
            OCI_ATTR_PRECISION, m_handles.m_pOCIError),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        Iora8Connection::Check(
            g_ora8API.OCIAttrGet(pOCIParam, OCI_DTYPE_PARAM, &scale, NULL,
            OCI_ATTR_SCALE, m_handles.m_pOCIError),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        Iora8Connection::Check(
            g_ora8API.OCIAttrGet(pOCIParam, OCI_DTYPE_PARAM, &isnull, NULL,
            OCI_ATTR_IS_NULL, m_handles.m_pOCIError),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);

        SADataType_t eFieldType;
        // ROWID currently is the only type that
        // we need to use dispsize instead dbsize
        if (dbtype == SQLT_RDD) // rowid descriptor
        {
            sb4 dispsize = 0;
            Iora8Connection::Check(
                g_ora8API.OCIAttrGet(pOCIParam, OCI_DTYPE_PARAM, &dispsize,
                NULL, OCI_ATTR_DISP_SIZE, m_handles.m_pOCIError),
                m_handles.m_pOCIError, OCI_HTYPE_ERROR);
            eFieldType = SA_dtString;
            dbsize = (ub2)dispsize;
    }
        else
            eFieldType = CnvtNativeToStd(dbtype, 0, dbsize, prec, scale);

        SAString colName;
#ifdef SA_UNICODE
        if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
            colName.SetUTF16Chars(sColNameWide, nColNameLen / sizeof(utext));
        else
#endif
            colName = SAString(sColName, nColNameLen);

        if (SA_dtString == eFieldType)
        {
#ifdef SA_UNICODE
            if (!((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
#endif
                dbsize = (ub2)(dbsize
                * ((Iora8Connection*)m_pISAConnection)->mb_cur_max);
        }

        (m_pCommand->*fn)(colName, eFieldType, (int)dbtype, dbsize, prec,
            scale, isnull == 0, (int)cFields);

        g_ora8API.OCIDescriptorFree(pOCIParam, OCI_DTYPE_PARAM);
}
}

/*virtual */
long Iora8Cursor::GetRowsAffected()
{
    ub4 rows;

    Iora8Connection::Check(
        g_ora8API.OCIAttrGet(m_handles.m_pOCIStmt, OCI_HTYPE_STMT, &rows,
        NULL, OCI_ATTR_ROW_COUNT, m_handles.m_pOCIError),
        m_handles.m_pOCIError, OCI_HTYPE_ERROR);
    return rows;
}

SAField *Iora8Cursor::WhichFieldIsPiecewise() const
{
    // because of a serious bug in 8i
    // with piecewise reading of LONG
    // we will not use it (piecewise reading of LONG)
    // will use callbacks instead
    return NULL;

    //	// field can be fetched piecewise if:
    //	// 1. it it Long*
    //	// 2. it is the last in select list (else we will block fields after this)
    //	// 3. there are no *Lob fields in select list (else we will block them until piecewise)
    //	// 4. other Long* are allowed because they will be read by callbacks (no blocking)
    //	if(FieldCount(2, SA_dtBLob, SA_dtCLob) == 0)
    //	{
    //		SAField &Field = m_pCommand->Field(m_pCommand->FieldCount());
    //
    //		switch(Field.FieldType())
    //		{
    //		case SA_dtLongBinary:
    //		case SA_dtLongChar:
    //			return &Field;
    //		default:
    //			break;
    //		}
    //	}
    //
    //	return NULL;
}

/*virtual */
void Iora8Cursor::SetFieldBuffer(
    int nCol, // 1-based
    void *pInd, size_t nIndSize, void *pSize, size_t/* nSizeSize*/,
    void *pValue, size_t nValueSize)
{
    assert(nIndSize == sizeof(sb2));
    if (nIndSize != sizeof(sb2))
        return;

    SAField &Field = m_pCommand->Field(nCol);

    ub4 i;
    ub2 dty;
    bool bLong = false;
    switch (Field.FieldType())
    {
    case SA_dtUnknown:
        throw SAException(SA_Library_Error, SA_Library_Error_UnknownColumnType, -1, IDS_UNKNOWN_COLUMN_TYPE,
            (const SAChar*)Field.Name());
    case SA_dtShort:
        dty = SQLT_INT; // 16-bit signed INTEGER
        break;
    case SA_dtUShort:
        dty = SQLT_UIN; // 16-bit unsigned INTEGER
        break;
    case SA_dtLong:
        dty = SQLT_INT; // 32-bit signed INTEGER
        break;
    case SA_dtULong:
        dty = SQLT_UIN; // 32-bit unsigned INTEGER
        break;
    case SA_dtDouble:
        // For native binary double, OCI 10.1 and higher required
        // but sometime people try to connect to olde server with new client
        // then ORA-03115 occurs
        // dty = NULL != g_ora8API.OCILobRead2 ? SQLT_BDOUBLE:SQLT_FLT;
        dty = SQLT_FLT;
        break;
    case SA_dtNumeric:
        dty = SQLT_VNU; // VARNUM
        break;
    case SA_dtDateTime:
        if (((Iora8Connection*)m_pISAConnection)->m_bUseTimeStamp
            && nValueSize != sizeof(OraDATE_t))
        {
            ub4 dtype = OCI_DTYPE_TIMESTAMP;
            dty = SQLT_TIMESTAMP; // TIMESTAMP

            if (Field.FieldNativeType() == SQLT_TIME_TZ ||
                Field.FieldNativeType() == SQLT_TIMESTAMP_TZ ||
                Field.FieldNativeType() == SQLT_TIMESTAMP_LTZ)
            {
                dty = SQLT_TIMESTAMP_TZ;
                dtype = OCI_DTYPE_TIMESTAMP_TZ;
            }

            for (i = 0; i < m_cRowsToPrefetch; ++i)
            {
                Iora8Connection::Check(
                    g_ora8API.OCIDescriptorAlloc(
                    ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
                    (dvoid**)((OCIDateTime**)pValue + i),
                    dtype, 0, NULL),
                    ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
                    OCI_HTYPE_ENV);
                *((ub2*)pSize + i) = (ub2) sizeof(OCIDateTime*);
            }
        }
        else
            dty = SQLT_DAT; // DATE
        break;
    case SA_dtBytes:
        dty = SQLT_BIN; // binary data(DTYBIN)
        break;
    case SA_dtString:
        dty = SQLT_CHR; // VARCHAR2
        break;
    case SA_dtLongBinary:
        dty = SQLT_LBI; // LONG RAW
        assert(nValueSize == sizeof(LongContext_t));
        bLong = true;
        break;
    case SA_dtLongChar:
        dty = SQLT_LNG; // LONG
        assert(nValueSize == sizeof(LongContext_t));
        bLong = true;
        break;
    case SA_dtBLob:
        if (Field.FieldNativeType() == SQLT_BFILE)
        {
            dty = SQLT_BFILE;
            for (i = 0; i < m_cRowsToPrefetch; ++i)
            {
                Iora8Connection::Check(
                    g_ora8API.OCIDescriptorAlloc(
                    ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
                    (dvoid**)((OCILobLocator**)pValue + i),
                    OCI_DTYPE_FILE, 0, NULL),
                    ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
                    OCI_HTYPE_ENV);
            }
        }
        else
        {
            dty = SQLT_BLOB; // Binary LOB
            for (i = 0; i < m_cRowsToPrefetch; ++i)
            {
                Iora8Connection::Check(
                    g_ora8API.OCIDescriptorAlloc(
                    ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
                    (dvoid**)((OCILobLocator**)pValue + i),
                    OCI_DTYPE_LOB, 0, NULL),
                    ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
                    OCI_HTYPE_ENV);
            }
        }
        break;
    case SA_dtCLob:
        dty = SQLT_CLOB; // Character LOB
        for (i = 0; i < m_cRowsToPrefetch; ++i)
        {
            Iora8Connection::Check(
                g_ora8API.OCIDescriptorAlloc(
                ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
                (dvoid**)((OCILobLocator**)pValue + i),
                OCI_DTYPE_LOB, 0, NULL),
                ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
                OCI_HTYPE_ENV);
        }
        break;
    case SA_dtCursor:
        dty = SQLT_RSET; // result set type
        for (i = 0; i < m_cRowsToPrefetch; ++i)
        {
            Iora8Connection::Check(
                g_ora8API.OCIHandleAlloc(
                ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
                (dvoid**)((OCIStmt**)pValue + i), OCI_HTYPE_STMT,
                0, NULL),
                ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
                OCI_HTYPE_ENV);
        }
        break;
    default:
        dty = 0;
        assert(false);
        // unknown type
    }

    OCIDefine *pOCIDefine = NULL;
    if (bLong)
    {
        Iora8Connection::Check(
            g_ora8API.OCIDefineByPos(m_handles.m_pOCIStmt, &pOCIDefine,
            m_handles.m_pOCIError, nCol, NULL, SB4MAXVAL, dty, pInd,
            (ub2*)pSize, NULL, OCI_DYNAMIC_FETCH),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);

        LongContext_t *pLongContext = (LongContext_t *)pValue;
        pLongContext->pReader = &Field;
        pLongContext->pWriter = 0;
        pLongContext->pInd = (sb2*)pInd;

        // check if this long should be piecewised
        if (WhichFieldIsPiecewise() == &Field)
            pLongContext->eState = LongContextPiecewiseDefine;
        else // if not then callbacks
        {
            Iora8Connection::Check(
                g_ora8API.OCIDefineDynamic(pOCIDefine,
                m_handles.m_pOCIError, (dvoid*)pLongContext,
                &LongDefine), m_handles.m_pOCIError,
                OCI_HTYPE_ERROR);
            pLongContext->eState = LongContextCallback;
        }
    }
    else
    {
        Iora8Connection::Check(
            g_ora8API.OCIDefineByPos(m_handles.m_pOCIStmt, &pOCIDefine,
            m_handles.m_pOCIError, nCol, pValue, (sb4)nValueSize,
            dty, pInd, (ub2*)pSize, NULL, OCI_DEFAULT),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);
}

    assert(pOCIDefine);

    // if there are known options for this parameter, set them
    SAString sOCI_ATTR_CHARSET_FORM =
#ifdef SA_UNICODE
        ((Iora8Connection*)m_pISAConnection)->m_bUseUCS2 ? SAString(_TSA("SQLCS_NCHAR")) :
#endif
        Field.Option(_TSA("OCI_ATTR_CHARSET_FORM"));
    SetCharSetOptions(sOCI_ATTR_CHARSET_FORM,
        Field.Option(_TSA("OCI_ATTR_CHARSET_ID")), pOCIDefine,
        OCI_HTYPE_DEFINE);
        }

/*virtual */
void Iora8Cursor::SetSelectBuffers()
{
    SAString sOption = m_pCommand->Option(SACMD_PREFETCH_ROWS);
    if (!sOption.IsEmpty())
    {
        int cLongs = FieldCount(3, SA_dtLongBinary, SA_dtLongChar, SA_dtCursor);
        if (cLongs) // do not use bulk fetch if there are long columns or refcursor
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
    m_bOCI_NO_DATA = false;

    // use default helpers
    AllocSelectBuffer(sizeof(sb2), sizeof(ub2), m_cRowsToPrefetch);
}

void Iora8Cursor::CheckPiecewiseNull()
{
    // because of serious bug in 8i
    // with piecewise reading of LONG
    // we will not use it (piecewise reading of LONG)
    // will use callbacks instead
    // so this function will never be called
    assert(false);

    SAField *pField = WhichFieldIsPiecewise();
    assert(pField);
    void *pValue;
    size_t nFieldBufSize;
    GetFieldBuffer(pField->Pos(), pValue, nFieldBufSize);
    assert(nFieldBufSize == sizeof(LongContext_t));
    LongContext_t *pLongContext = (LongContext_t *)pValue;

    // to check if piecewise fetched column is null (or not)
    // we have to read at least one byte from piecewise stream
    OCIDefine *pOCIDefine;
    ub4 type;
    ub1 in_out;
    ub4 iter, idx;
    ub1 piece;

    sword rc;
    Iora8Connection::Check(
        g_ora8API.OCIStmtGetPieceInfo(m_handles.m_pOCIStmt,
        m_handles.m_pOCIError, (dvoid**)&pOCIDefine, &type,
        &in_out, &iter, &idx, &piece), m_handles.m_pOCIError,
        OCI_HTYPE_ERROR);
    pLongContext->Len = (ub4) sizeof(m_PiecewiseNullCheckPreFetch);
    Iora8Connection::Check(
        g_ora8API.OCIStmtSetPieceInfo(pOCIDefine, type,
        m_handles.m_pOCIError, &m_PiecewiseNullCheckPreFetch[0],
        &pLongContext->Len, piece, pLongContext->pInd, NULL),
        m_handles.m_pOCIError, OCI_HTYPE_ERROR);

    rc = g_ora8API.OCIStmtFetch(m_handles.m_pOCIStmt, m_handles.m_pOCIError, 1,
        OCI_FETCH_NEXT, OCI_DEFAULT);

    if (rc != OCI_NEED_DATA)
    {
        Iora8Connection::Check(rc, m_handles.m_pOCIError, OCI_HTYPE_ERROR);

        // cloumn was null or whole Long* was read
        m_bPiecewiseFetchPending = false;
    }

    // set null indicator manually
    // because Oracle set it only if null or whole read
    *(sb2*)pLongContext->pInd = (sb2)(pLongContext->Len ? 0 : -1);

    // if column was not null then fetched piece is saved
    // in m_PiecewiseNullCheckPreFetch, it's len in pLongContext->Len
    // and we will use it when actual read request happen
}

/*virtual */
void Iora8Cursor::ConvertString(SAString &String, const void *pData,
    size_t nRealSize)
{
#ifdef SA_UNICODE
    // nRealSize is in bytes but we need in characters,
    // so nRealSize should be a multiply of character size
    if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
    {
        assert(nRealSize % sizeof(utext) == 0);
        String.SetUTF16Chars(pData, nRealSize / sizeof(utext));
    }
    else
#endif
        ISACursor::ConvertString(String, pData, nRealSize);
}

/*virtual */
bool Iora8Cursor::FetchNext()
{
    if (m_cRowsToPrefetch != 1)
        return FetchNextArray();

    if (m_bPiecewiseFetchPending)
        DiscardPiecewiseFetch();

    sword rc = g_ora8API.OCIStmtFetch(m_handles.m_pOCIStmt,
        m_handles.m_pOCIError, 1, OCI_FETCH_NEXT, OCI_DEFAULT);
    if (rc != OCI_NO_DATA)
    {
        if (rc != OCI_NEED_DATA)
            Iora8Connection::Check(rc, m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        else
        {
            m_bPiecewiseFetchPending = true;
            CheckPiecewiseNull();
        }

        // use default helpers
        ConvertSelectBufferToFields(0);
        return true;
    }
    else if (!isSetScrollable())
        m_bResultSetCanBe = false;

    return false;
}

/*virtual */
bool Iora8Cursor::FetchPrior()
{
    if (NULL == g_ora8API.OCIStmtFetch2)
        return false;

    if (m_bPiecewiseFetchPending)
        DiscardPiecewiseFetch();

    sword rc = g_ora8API.OCIStmtFetch2(m_handles.m_pOCIStmt,
        m_handles.m_pOCIError, 1, OCI_FETCH_PRIOR, (sb4)0, OCI_DEFAULT);

    if (rc != OCI_NO_DATA)
    {
        if (rc != OCI_NEED_DATA)
            Iora8Connection::Check(rc, m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        else
        {
            m_bPiecewiseFetchPending = true;
            CheckPiecewiseNull();
        }

        // use default helpers
        ConvertSelectBufferToFields(0);
        return true;
    }

    return false;
}

/*virtual */
bool Iora8Cursor::FetchFirst()
{
    if (NULL == g_ora8API.OCIStmtFetch2)
        return false;

    if (m_bPiecewiseFetchPending)
        DiscardPiecewiseFetch();

    sword rc = g_ora8API.OCIStmtFetch2(m_handles.m_pOCIStmt,
        m_handles.m_pOCIError, 1, OCI_FETCH_FIRST, (sb4)0, OCI_DEFAULT);

    if (rc != OCI_NO_DATA)
    {
        if (rc != OCI_NEED_DATA)
            Iora8Connection::Check(rc, m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        else
        {
            m_bPiecewiseFetchPending = true;
            CheckPiecewiseNull();
        }

        // use default helpers
        ConvertSelectBufferToFields(0);
        return true;
    }

    return false;
}

/*virtual */
bool Iora8Cursor::FetchLast()
{
    if (NULL == g_ora8API.OCIStmtFetch2)
        return false;

    if (m_bPiecewiseFetchPending)
        DiscardPiecewiseFetch();

    sword rc = g_ora8API.OCIStmtFetch2(m_handles.m_pOCIStmt,
        m_handles.m_pOCIError, 1, OCI_FETCH_LAST, (sb4)0, OCI_DEFAULT);

    if (rc != OCI_NO_DATA)
    {
        if (rc != OCI_NEED_DATA)
            Iora8Connection::Check(rc, m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        else
        {
            m_bPiecewiseFetchPending = true;
            CheckPiecewiseNull();
        }

        // use default helpers
        ConvertSelectBufferToFields(0);
        return true;
    }

    return false;
}

bool Iora8Cursor::FetchPos(int offset, bool Relative/* = false*/)
{
    if (NULL == g_ora8API.OCIStmtFetch2)
        return false;

    if (m_bPiecewiseFetchPending)
        DiscardPiecewiseFetch();

    sword rc = g_ora8API.OCIStmtFetch2(m_handles.m_pOCIStmt, m_handles.m_pOCIError, 1,
        Relative ? OCI_FETCH_RELATIVE : OCI_FETCH_ABSOLUTE, (sb4)offset, OCI_DEFAULT);

    if (rc != OCI_NO_DATA)
    {
        if (rc != OCI_NEED_DATA)
            Iora8Connection::Check(rc, m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        else
        {
            m_bPiecewiseFetchPending = true;
            CheckPiecewiseNull();
        }

        // use default helpers
        ConvertSelectBufferToFields(0);
        return true;
    }

    return false;
}

bool Iora8Cursor::FetchNextArray()
{
    assert(!m_bPiecewiseFetchPending);

    if (m_cRowCurrent == m_cRowsObtained)
    {
        if (!m_bOCI_NO_DATA)
        {
            long cRowsFetched = GetRowsAffected();
            sword rc = g_ora8API.OCIStmtFetch(m_handles.m_pOCIStmt,
                m_handles.m_pOCIError, m_cRowsToPrefetch, OCI_FETCH_NEXT,
                OCI_DEFAULT);
            m_cRowsObtained = (ub4)(GetRowsAffected() - cRowsFetched);

            if (rc != OCI_NO_DATA)
            {
                assert(rc != OCI_NEED_DATA);
                Iora8Connection::Check(rc, m_handles.m_pOCIError,
                    OCI_HTYPE_ERROR);
            }
            else
                m_bOCI_NO_DATA = true;
        }
        else
            m_cRowsObtained = 0;

        m_cRowCurrent = 0;
    }

    if (m_cRowsObtained)
    {
        // ensure that m_cRowCurrent is incremented before the conversion
        // to be compatible with manual LOB retrieving
        ++m_cRowCurrent;

        // use default helpers
        ConvertSelectBufferToFields(m_cRowCurrent - 1);
        return true;
    }

    m_bResultSetCanBe = false;
    return false;
}

void Iora8Cursor::DiscardPiecewiseFetch()
{
    OCIDefine *pOCIDefine;
    ub4 type;
    ub1 in_out;
    ub4 iter, idx;
    ub1 piece;
    sb2 ind;
    char buf[0xFFFF];
    ub4 alen = sizeof(buf);

    sword rc;
    do
    {
        Iora8Connection::Check(
            g_ora8API.OCIStmtGetPieceInfo(m_handles.m_pOCIStmt,
            m_handles.m_pOCIError, (dvoid**)&pOCIDefine, &type,
            &in_out, &iter, &idx, &piece), m_handles.m_pOCIError,
            OCI_HTYPE_ERROR);
        Iora8Connection::Check(
            g_ora8API.OCIStmtSetPieceInfo(pOCIDefine, type,
            m_handles.m_pOCIError, buf, &alen, piece, &ind, NULL),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);

        rc = g_ora8API.OCIStmtFetch(m_handles.m_pOCIStmt, m_handles.m_pOCIError,
            1, OCI_FETCH_NEXT, OCI_DEFAULT);
    } while (rc == OCI_NEED_DATA);
    Iora8Connection::Check(rc, m_handles.m_pOCIError, OCI_HTYPE_ERROR);

    m_bPiecewiseFetchPending = false;
}

/*virtual */
void Iora7Cursor::ReadLongOrLOB(ValueType_t eValueType, SAValueRead &vr,
    void *pValue, size_t/* nBufSize*/, saLongOrLobReader_t fnReader,
    size_t nReaderWantedPieceSize, void *pAddlData)
{
    LongContext_t *pLongContext = (LongContext_t *)pValue;
    size_t nLongSize = 0l; // no way to determine size before fetch
    unsigned char* pBuf;
    size_t nPortionSize = vr.PrepareReader(nLongSize,
        Iora7Connection::MaxLongPortion, pBuf, fnReader,
        nReaderWantedPieceSize, pAddlData);
    assert(nPortionSize <= Iora7Connection::MaxLongPortion);

    if (pLongContext->eState & LongContextPiecewise)
    {
        SAPieceType_t ePieceType = SA_FirstPiece;
        // for SQLAPI Callback prefetching should be transparent
        // so, first use prefetched (when checking for null) data
        // and send it as if it was just fetched from Oracle
        size_t nPrefetchedSent = 0l;
        while (pLongContext->Len - nPrefetchedSent >= nPortionSize)
        {
            memcpy(pBuf, m_PiecewiseNullCheckPreFetch + nPrefetchedSent,
                nPortionSize);
            nPrefetchedSent += nPortionSize;

            if (!m_bPiecewiseFetchPending) // all data was fetched during prefetch
            {
                if (pLongContext->Len == nPrefetchedSent)
                {
                    if (ePieceType == SA_NextPiece)
                        ePieceType = SA_LastPiece;
                    else // the whole Long was read in one piece
                    {
                        assert(ePieceType == SA_FirstPiece);
                        ePieceType = SA_OnePiece;
                    }
                }
            }
            vr.InvokeReader(ePieceType, pBuf, nPortionSize);

            if (ePieceType == SA_FirstPiece)
                ePieceType = SA_NextPiece;
        }

        // then if needed fetch all remaining data
        if (m_bPiecewiseFetchPending)
        {
            sword rc = 0;
            do
            {
                ub1 piece;
                dvoid *ctxp;
                ub4 iter, index;
                ub4 len = (ub4)nPortionSize;

                // a tail from prefetching buffer remains?
                size_t nPrefetchedTail = pLongContext->Len - nPrefetchedSent;
                assert(nPrefetchedTail < nPortionSize);
                if (nPrefetchedTail)
                {
                    memcpy(pBuf, m_PiecewiseNullCheckPreFetch + nPrefetchedSent,
                        nPrefetchedTail);
                    nPrefetchedSent += nPrefetchedTail;
                    // we will ask Oracle to read less during this piece
                    len -= (ub4)nPrefetchedTail;
                }assert(pLongContext->Len == nPrefetchedSent);

                ((Iora7Connection*)m_pISAConnection)->Check(
                    g_ora7API.ogetpi(&m_handles.m_cda, &piece, &ctxp, &iter,
                    &index), &m_handles.m_cda);
                ((Iora7Connection*)m_pISAConnection)->Check(
                    g_ora7API.osetpi(&m_handles.m_cda, piece,
                    pBuf + nPrefetchedTail, &len),
                    &m_handles.m_cda);

                switch (pLongContext->eState)
                {
                case LongContextPiecewiseDefine:
                    rc = g_ora7API.ofetch(&m_handles.m_cda);
                    break;
                case LongContextPiecewiseBind:
                    rc = g_ora7API.oexec(&m_handles.m_cda);
                    break;
                default:
                    assert(false);
                }

                if (m_handles.m_cda.rc != 3130)
                {
                    if (ePieceType == SA_NextPiece)
                        ePieceType = SA_LastPiece;
                    else // the whole Long was read in one piece
                    {
                        assert(ePieceType == SA_FirstPiece);
                        ePieceType = SA_OnePiece;
                    }
                }
                vr.InvokeReader(ePieceType, pBuf, nPrefetchedTail + len);

                if (ePieceType == SA_FirstPiece)
                    ePieceType = SA_NextPiece;
            } while (m_handles.m_cda.rc == 3130);
            ((Iora7Connection*)m_pISAConnection)->Check(rc, &m_handles.m_cda);

            m_bPiecewiseFetchPending = false;
        }
    }
    else
    {
        assert(pLongContext->eState == LongContextNormal);
        if (eValueType == ISA_FieldValue)
        {
            SAField &Field = (SAField &)vr;

            SAPieceType_t ePieceType = SA_FirstPiece;
            ub4 retl;
            sb4 offset = 0;
            sword dtype;
            switch (Field.FieldType())
            {
            case SA_dtLongBinary:
                dtype = 24; // LONG RAW
                break;
            case SA_dtLongChar:
                dtype = 8; // LONG
                break;
            default:
                dtype = 0;
                assert(false);
            }
            do
            {
                ((Iora7Connection*)m_pISAConnection)->Check(
                    g_ora7API.oflng(&m_handles.m_cda, Field.Pos(), pBuf,
                    (sb4)nPortionSize, dtype, &retl, offset),
                    &m_handles.m_cda);

                if (retl < nPortionSize)
                {
                    if (ePieceType == SA_NextPiece)
                        ePieceType = SA_LastPiece;
                    else // the whole Long was read in one piece
                    {
                        assert(ePieceType == SA_FirstPiece);
                        ePieceType = SA_OnePiece;
                    }
                }
                vr.InvokeReader(ePieceType, pBuf, retl);

                offset += retl;

                if (ePieceType == SA_FirstPiece)
                    ePieceType = SA_NextPiece;
            } while (retl == nPortionSize);
        }
        else
        {
            assert(false);
        }
    }
    }

void Iora8Cursor::ReadLong(ValueType_t eValueType, SAValueRead &vr,
    LongContext_t *pLongContext, saLongOrLobReader_t fnReader,
    size_t nReaderWantedPieceSize, void *pAddlData)
{
    SADummyConverter DummyConverter;
    ISADataConverter *pIConverter = &DummyConverter;
#if defined(SA_UNICODE) && ! defined(SQLAPI_WINDOWS)
    SAUTF16UnicodeConverter Utf2UnicodeConverter;
    if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2 && (eValueType == ISA_FieldValue ?
        ((SAField&)vr).FieldNativeType() : ((SAParam&)vr).ParamNativeType()) == SQLT_LNG)
        pIConverter = &Utf2UnicodeConverter;
#endif

    if (pLongContext->eState & LongContextPiecewise)
    {
        assert(pLongContext->pReader == WhichFieldIsPiecewise());

        size_t nBlobSize = 0l; // no way to determine size before fetch

        unsigned char* pBuf;
        size_t nPortionSize = pLongContext->pReader->PrepareReader(nBlobSize,
            SB4MAXVAL, pBuf, fnReader, nReaderWantedPieceSize, pAddlData);

        SAPieceType_t ePieceType = SA_FirstPiece;
        // for SQLAPI Callback prefetching should be transparent
        // so, first use prefetched (when checking for null) data
        // and send it as if it was just fetched from Oracle
        size_t nPrefetchedSent = 0l;
        while (pLongContext->Len - nPrefetchedSent >= nPortionSize)
        {
            memcpy(pBuf, m_PiecewiseNullCheckPreFetch + nPrefetchedSent,
                nPortionSize);
            nPrefetchedSent += nPortionSize;

            if (!m_bPiecewiseFetchPending) // all data was fetched during prefetch
            {
                if (pLongContext->Len == nPrefetchedSent)
                {
                    if (ePieceType == SA_NextPiece)
                        ePieceType = SA_LastPiece;
                    else // the whole Long was read in one piece
                    {
                        assert(ePieceType == SA_FirstPiece);
                        ePieceType = SA_OnePiece;
                    }
                }
            }
            pLongContext->pReader->InvokeReader(ePieceType, pBuf, nPortionSize);

            if (ePieceType == SA_FirstPiece)
                ePieceType = SA_NextPiece;
        }

        // then if needed fetch all remaining data
        if (m_bPiecewiseFetchPending)
        {
            sword rc;
            do
            {
                OCIDefine *pOCIDefine;
                ub4 type;
                ub1 in_out;
                ub4 iter, idx;
                ub1 piece;
                ub4 alen = (ub4)nPortionSize;

                // a tail from prefetching buffer remains?
                size_t nPrefetchedTail = pLongContext->Len - nPrefetchedSent;
                assert(nPrefetchedTail < nPortionSize);
                if (nPrefetchedTail)
                {
                    memcpy(pBuf, m_PiecewiseNullCheckPreFetch + nPrefetchedSent,
                        nPrefetchedTail);
                    nPrefetchedSent += nPrefetchedTail;
                    // we will ask Oracle to read less during this piece
                    alen -= (ub4)nPrefetchedTail;
                }assert(pLongContext->Len == nPrefetchedSent);

                Iora8Connection::Check(
                    g_ora8API.OCIStmtGetPieceInfo(m_handles.m_pOCIStmt,
                    m_handles.m_pOCIError, (dvoid**)&pOCIDefine,
                    &type, &in_out, &iter, &idx, &piece),
                    m_handles.m_pOCIError, OCI_HTYPE_ERROR);
                Iora8Connection::Check(
                    g_ora8API.OCIStmtSetPieceInfo(pOCIDefine, type,
                    m_handles.m_pOCIError, pBuf + nPrefetchedTail,
                    &alen, piece, pLongContext->pInd, NULL),
                    m_handles.m_pOCIError, OCI_HTYPE_ERROR);

                rc = g_ora8API.OCIStmtFetch(m_handles.m_pOCIStmt,
                    m_handles.m_pOCIError, 1, OCI_FETCH_NEXT, OCI_DEFAULT);

                if (rc != OCI_NEED_DATA)
                {
                    if (ePieceType == SA_NextPiece)
                        ePieceType = SA_LastPiece;
                    else // the whole Long was read in one piece
                    {
                        assert(ePieceType == SA_FirstPiece);
                        ePieceType = SA_OnePiece;
                    }
                }
                pLongContext->pReader->InvokeReader(ePieceType, pBuf,
                    nPrefetchedTail + alen);

                if (ePieceType == SA_FirstPiece)
                    ePieceType = SA_NextPiece;
            } while (rc == OCI_NEED_DATA);
            Iora8Connection::Check(rc, m_handles.m_pOCIError, OCI_HTYPE_ERROR);

            m_bPiecewiseFetchPending = false;
        }
    }
    else
    {
        assert(pLongContext->eState == LongContextCallback);

        // we always read long data into internal buffer!
        // after long fields callbacks last Piece is read but not
        // processed, do it now
        SAPieceType_t ePieceType = SA_LastPiece;
        vr.InvokeReader(ePieceType, pLongContext->pBuf, pLongContext->Len);

        // send data from internal buffer to user callback
        // Convert the data when it's necessary
        SAString sLong = *pLongContext->pReader->m_pString;
        size_t nLongSize = sLong.GetBinaryLength();
        const char *data = (const char *)sLong.GetBinaryBuffer(nLongSize);

        unsigned char* pBuf;
        size_t nPortionSize = vr.PrepareReader(0, SB4MAXVAL, pBuf, fnReader,
            nReaderWantedPieceSize, pAddlData);

        size_t nTotal = 0l;
        ePieceType = SA_FirstPiece;
        do
        {
            size_t nLen = nPortionSize;
            if (nLongSize - nTotal < nLen)
                nLen = nLongSize - nTotal;

            memcpy(pBuf, data + nTotal, nLen);
            nTotal += nLen;

            if (nTotal == nLongSize)
            {
                if (ePieceType == SA_NextPiece)
                    ePieceType = SA_LastPiece;
                else // the whole Long was read in one piece
                {
                    assert(ePieceType == SA_FirstPiece);
                    ePieceType = SA_OnePiece;
                }
            }

            pIConverter->PutStream(pBuf, nLen, ePieceType);
            size_t nCnvtSize;
            SAPieceType_t eCnvtPieceType;
            while (pIConverter->GetStream(pBuf, nLen, nCnvtSize, eCnvtPieceType))
                vr.InvokeReader(eCnvtPieceType, pBuf, nCnvtSize);

            if (ePieceType == SA_FirstPiece)
                ePieceType = SA_NextPiece;
        } while (nTotal < nLongSize);

        sLong.ReleaseBinaryBuffer(nLongSize);
    }
}

void Iora8Cursor::ReadLob(ValueType_t eValueType, SAValueRead &vr,
    OCILobLocator* pOCILobLocator, saLongOrLobReader_t fnReader,
    size_t nReaderWantedPieceSize, void *pAddlData)
{
    bool bFileLobOpen = false;
    if (eValueType == ISA_FieldValue)
    {
        if (((SAField&)vr).FieldNativeType() == SQLT_BFILE)
        {
            Iora8Connection::Check(
                g_ora8API.OCILobFileOpen(
                ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
                m_handles.m_pOCIError, pOCILobLocator,
                OCI_FILE_READONLY), m_handles.m_pOCIError,
                OCI_HTYPE_ERROR);

            bFileLobOpen = true;
        }
        }

    SADummyConverter DummyConverter;
    ISADataConverter *pIConverter = &DummyConverter;
#ifdef SA_UNICODE
    bool bUnicode = ((Iora8Connection*)m_pISAConnection)->m_bUseUCS2 &&
        (eValueType == ISA_FieldValue ?
        ((SAField&)vr).FieldNativeType() : ((SAParam&)vr).ParamNativeType()) == SQLT_CLOB;
#ifndef SQLAPI_WINDOWS
    SAUTF16UnicodeConverter Utf2UnicodeConverter;
    if (bUnicode)
        pIConverter = &Utf2UnicodeConverter;
#endif
#endif

    unsigned char* pBuf;
    size_t nPortionSize = vr.PrepareReader(0, UB4MAXVAL, pBuf, fnReader,
        nReaderWantedPieceSize, pAddlData);

    try
    {
        size_t nCnvtPieceSize = nPortionSize;
        SAPieceType_t ePieceType = SA_FirstPiece;
        ub4 amt, nBlobSize = 0;
        sb4 offset = 0;
        sword status;
        ub1 csfrm;
        Iora8Connection::Check(
            g_ora8API.OCILobCharSetForm(
            ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
            m_handles.m_pOCIError, pOCILobLocator, &csfrm),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        if (0 == csfrm)
            csfrm = SQLCS_IMPLICIT;

        do
        {
            amt = (ub4)(
#ifdef SA_UNICODE
                bUnicode ? (nPortionSize / sizeof(utext)) :
#endif
                nPortionSize);
            status =
                g_ora8API.OCILobRead(
                ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
                m_handles.m_pOCIError, pOCILobLocator, &amt,
                offset + 1, pBuf, (ub4)nPortionSize, NULL, NULL,
                (ub2)(
#ifdef SA_UNICODE
                bUnicode ? OCI_UCS2ID :
#endif
                0), csfrm);

            if (OCI_NEED_DATA == status)
            {
                // unexpected end of data? no - buffer is too small
                // because we have varying-width client-side character set
                nBlobSize = (ub4)(offset + amt + nPortionSize);
            }
            else
            {
                // OCILobRead return 0 even if not all is read.
                // So we will check the amt value - when it's 0 the lob is read.

                Iora8Connection::Check(status, m_handles.m_pOCIError,
                    OCI_HTYPE_ERROR);

                if (amt < (
#ifdef SA_UNICODE
                    bUnicode ? (nPortionSize / sizeof(utext)) :
#endif
                    nPortionSize))
                {
                    if (SA_NextPiece == ePieceType)
                        ePieceType = SA_LastPiece;
                    else // the whole Long was read in one piece
                    {
                        assert(ePieceType == SA_FirstPiece);
                        ePieceType = SA_OnePiece;
                    }
                    }

                nBlobSize = offset + amt;
                }

            offset += amt;
            assert((ub4)offset <= nBlobSize);

            if (amt > 0)
            {
                pIConverter->PutStream(pBuf,
#ifdef SA_UNICODE
                    bUnicode ? (amt * sizeof(utext)) :
#endif
                    (size_t)amt, ePieceType);

                size_t nCnvtSize;
                SAPieceType_t eCnvtPieceType;
                while (pIConverter->GetStream(pBuf, nCnvtPieceSize, nCnvtSize,
                    eCnvtPieceType))
                    vr.InvokeReader(eCnvtPieceType, pBuf, nCnvtSize);

                if (SA_FirstPiece == ePieceType)
                    ePieceType = SA_NextPiece;
            }
            else
                break;
            } while ((ub4)offset < (nBlobSize + amt) && ePieceType == SA_NextPiece);
    }
    catch (SAException &)
    {
        // try to clean up
        if (bFileLobOpen)
            g_ora8API.OCILobFileClose(
            ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
            m_handles.m_pOCIError, pOCILobLocator);
        throw;
    }

    if (bFileLobOpen)
        Iora8Connection::Check(
        g_ora8API.OCILobFileClose(
        ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
        m_handles.m_pOCIError, pOCILobLocator),
        m_handles.m_pOCIError, OCI_HTYPE_ERROR);
    }

void Iora8Cursor::ReadLob2(ValueType_t eValueType, SAValueRead &vr,
    OCILobLocator* pOCILobLocator, saLongOrLobReader_t fnReader,
    size_t nReaderWantedPieceSize, void *pAddlData)
{
    bool bFileLobOpen = false;

    if (eValueType == ISA_FieldValue)
    {
        if (((SAField&)vr).FieldNativeType() == SQLT_BFILE)
        {
            Iora8Connection::Check(
                g_ora8API.OCILobFileOpen(
                ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
                m_handles.m_pOCIError, pOCILobLocator,
                OCI_FILE_READONLY), m_handles.m_pOCIError,
                OCI_HTYPE_ERROR);

            bFileLobOpen = true;
        }
        }

    SADummyConverter DummyConverter;
    ISADataConverter *pIConverter = &DummyConverter;
#ifdef SA_UNICODE
    bool bUnicode = ((Iora8Connection*)m_pISAConnection)->m_bUseUCS2 &&
        (eValueType == ISA_FieldValue ?
        ((SAField&)vr).FieldNativeType() : ((SAParam&)vr).ParamNativeType()) == SQLT_CLOB;
#ifndef SQLAPI_WINDOWS
    SAUTF16UnicodeConverter Utf2UnicodeConverter;
    if (bUnicode)
        pIConverter = &Utf2UnicodeConverter;
#endif
#endif

    unsigned char* pBuf;
    size_t nPortionSize = vr.PrepareReader(0, UB4MAXVAL, pBuf, fnReader,
        nReaderWantedPieceSize, pAddlData);

    try
    {
        size_t nCnvtPieceSize = nPortionSize;
        SAPieceType_t ePieceType = SA_FirstPiece;
        oraub8 amt = 0l;
        sb4 offset = 1;
        sword status;
        ub1 csfrm;
        ub1 piece = OCI_FIRST_PIECE;

        Iora8Connection::Check(
            g_ora8API.OCILobCharSetForm(
            ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
            m_handles.m_pOCIError, pOCILobLocator, &csfrm),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);

        if (0 == csfrm)
            csfrm = SQLCS_IMPLICIT;

        do
        {
            status =
                g_ora8API.OCILobRead2(
                ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
                m_handles.m_pOCIError, pOCILobLocator, &amt, NULL,
                offset, pBuf, nPortionSize, piece, NULL, NULL,
                (ub2)(
#ifdef SA_UNICODE
                bUnicode ? OCI_UCS2ID :
#endif
                0), csfrm);

            // check the 'status' and change ePieceType
            if (OCI_NEED_DATA != status)
            {
                Iora8Connection::Check(status, m_handles.m_pOCIError,
                    OCI_HTYPE_ERROR);

                if (SA_NextPiece == ePieceType)
                    ePieceType = SA_LastPiece;
                else // the whole Long was read in one piece
                {
                    assert(ePieceType == SA_FirstPiece);
                    ePieceType = SA_OnePiece;
                }
            }

            pIConverter->PutStream(pBuf, (size_t)amt, ePieceType);
            size_t nCnvtSize;
            SAPieceType_t eCnvtPieceType;
            while (pIConverter->GetStream(pBuf, nCnvtPieceSize, nCnvtSize,
                eCnvtPieceType))
                vr.InvokeReader(eCnvtPieceType, pBuf, nCnvtSize);

            if (SA_FirstPiece == ePieceType)
                ePieceType = SA_NextPiece;

            if (OCI_FIRST_PIECE == piece)
                piece = OCI_NEXT_PIECE;
        } while (OCI_NEED_DATA == status);
    }
    catch (SAException &)
    {
        // try to clean up
        if (bFileLobOpen)
            g_ora8API.OCILobFileClose(
            ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
            m_handles.m_pOCIError, pOCILobLocator);
        throw;
    }

    if (bFileLobOpen)
        Iora8Connection::Check(
        g_ora8API.OCILobFileClose(
        ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
        m_handles.m_pOCIError, pOCILobLocator),
        m_handles.m_pOCIError, OCI_HTYPE_ERROR);

    }

void Iora8Cursor::FreeLobIfTemporary(OCILobLocator* pOCILobLocator)
{
    if (NULL == g_ora8API.OCILobIsTemporary)
        return;

    boolean is_temporary = 0;
    Iora8Connection::Check(
        g_ora8API.OCILobIsTemporary(
        ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
        m_handles.m_pOCIError, pOCILobLocator, &is_temporary),
        m_handles.m_pOCIError, OCI_HTYPE_ERROR);

    if (is_temporary)
        Iora8Connection::Check(
        g_ora8API.OCILobFreeTemporary(
        ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
        m_handles.m_pOCIError, pOCILobLocator),
        m_handles.m_pOCIError, OCI_HTYPE_ERROR);
}

/*virtual */
void Iora8Cursor::ReadLongOrLOB(ValueType_t eValueType, SAValueRead &vr,
    void *pValue, size_t nBufSize, saLongOrLobReader_t fnReader,
    size_t nReaderWantedPieceSize, void *pAddlData)
{
    ub4 LocatorPosInArray = 0;

    SADataType_t eDataType;
    switch (eValueType)
    {
    case ISA_FieldValue:
        eDataType = ((SAField&)vr).FieldType();
        if (m_cRowsToPrefetch != 1) // array mode is being used
            LocatorPosInArray = m_cRowCurrent - 1;
        break;
    default:
        assert(eValueType == ISA_ParamValue);
        eDataType = ((SAParam&)vr).ParamType();
    }
    switch (eDataType)
    {
    case SA_dtLongBinary:
    case SA_dtLongChar:
        assert(nBufSize == sizeof(LongContext_t));
        if (nBufSize != sizeof(LongContext_t))
            return;
        ReadLong(eValueType, vr, (LongContext_t *)pValue, fnReader,
            nReaderWantedPieceSize, pAddlData);
        break;
    case SA_dtBLob:
    case SA_dtCLob:
    {
        assert(nBufSize == sizeof(OCILobLocator*));
        if (nBufSize != sizeof(OCILobLocator*))
            return;
        OCILobLocator* pOCILobLocator = *((OCILobLocator**)pValue
            + LocatorPosInArray);
        if (NULL != g_ora8API.OCILobRead2)
            ReadLob2(eValueType, vr, pOCILobLocator, fnReader,
            nReaderWantedPieceSize, pAddlData);
        else
            ReadLob(eValueType, vr, pOCILobLocator, fnReader,
            nReaderWantedPieceSize, pAddlData);
        FreeLobIfTemporary(pOCILobLocator);
    }
    break;
    default:
        assert(false);
    }
}

/*static */
sb4 Iora8Cursor::LongDefine(dvoid *octxp, OCIDefine * /*defnp*/, ub4/* iter*/,
    dvoid **bufpp, ub4 **alenpp, ub1 *piecep, dvoid **indpp,
    ub2 ** /*rcodep*/)
{
    return LongDefineOrOutBind(octxp, bufpp, alenpp, piecep, indpp);
}

/*static */
sb4 Iora8Cursor::LongDefineOrOutBind(dvoid *octxp, dvoid **bufpp, ub4 **alenpp,
    ub1 *piecep, dvoid **indpp)
{
    LongContext_t *pLongContext = (LongContext_t*)octxp;

    if (*piecep == OCI_ONE_PIECE || *piecep == OCI_FIRST_PIECE)
    {
        *piecep = OCI_FIRST_PIECE;
        size_t nBlobSize = 0l; // no way to determine size before fetch

        // always read callback Longs into internal buffer
        // we will execute real Reader later after Execute() or FetchNext() has completed
        size_t nPortionSize = pLongContext->pReader->PrepareReader(nBlobSize,
            UB4MAXVAL, pLongContext->pBuf, NULL,
            pLongContext->pReader->m_nReaderWantedPieceSize, NULL);

        *bufpp = pLongContext->pBuf;
        *alenpp = &pLongContext->Len;
        **alenpp = (ub4)nPortionSize;
        *indpp = pLongContext->pInd;
    }
    else
    {
        assert(*piecep == OCI_NEXT_PIECE);

        SAPieceType_t ePieceType = SA_NextPiece;
        pLongContext->pReader->InvokeReader(ePieceType, pLongContext->pBuf,
            pLongContext->Len);

        *bufpp = pLongContext->pBuf;
        *alenpp = &pLongContext->Len;
        *indpp = pLongContext->pInd;
    }

    return OCI_CONTINUE;
}

/*virtual */
void Iora7Cursor::DescribeParamSP()
{
    SAString sProcName = m_pCommand->CommandText();

    ub4 arrsiz = 1024;

    // An array indicating whether the procedure is overloaded. If the
    // procedure (or function) is not overloaded, 0 is returned. Overloaded
    // procedures return 1...n for n overloadings of the name.
    ub2 ovrld[1024];
    ub2 pos[1024];
    ub2 level[1024];
    text argnm[1024][30];
    ub2 arnlen[1024];
    ub2 dtype[1024];
    ub1 defsup[1024];
    ub1 mode[1024];
    ub4 dtsiz[1024];
    sb2 prec[1024];
    sb2 scale[1024];
    ub1 radix[1024];
    ub4 spare[1024];

    ((Iora7Connection*)m_pISAConnection)->Check(
        g_ora7API.odessp(
        &((Iora7Connection*)m_pISAConnection)->m_handles.m_lda,
        (text*)sProcName.GetMultiByteChars(),
        sProcName.GetLength(), NULL, 0, NULL, 0, ovrld, pos, level,
        (text **)argnm, arnlen, dtype, defsup, mode, dtsiz, prec,
        scale, radix, spare, &arrsiz), &m_handles.m_cda);

    // if overloads are present we describe requested one
    ub2 nOverload =
        (ub2)sa_strtol(m_pCommand->Option(_TSA("Overload")), NULL, 10);
    for (unsigned int i = 0; i < arrsiz; ++i)
    {
        if (ovrld[i] == 0) // procedure is not overloaded
            nOverload = ovrld[i]; // ignore requested
        else if (nOverload == 0) // procedure is overloaded, but needed overload is not specified
            nOverload = 1; // default to the first one

        if (nOverload == ovrld[i])
        {
            SADataType_t eParamType = CnvtNativeToStd(dtype[i], 0, dtsiz[i],
                prec[i], scale[i]);
            int nNativeType = dtype[i];
            int nParamSize = dtsiz[i];
            SAParamDirType_t eDirType;
            if (pos[i] == 0)
            {
                eDirType = SA_ParamReturn;
                m_pCommand->CreateParam("Result", eParamType, nNativeType,
                    nParamSize, prec[i], scale[i], eDirType);
            }
            else
            {
                switch (mode[i])
                {
                case 0:
                    eDirType = SA_ParamInput;
                    break;
                case 1:
                    eDirType = SA_ParamOutput;
                    break;
                case 2:
                    eDirType = SA_ParamInputOutput;
                    break;
                default:
                    assert(false);
                    continue;
                }

                m_pCommand->CreateParam(
                    SAString((const char*)argnm[i], arnlen[i]), eParamType,
                    nNativeType, nParamSize, prec[i], scale[i], eDirType);
            }
        }
    }
}

/*virtual */
void Iora8Cursor::DescribeParamSP()
{
    enum
    {
        NameUnknown, NamePkg, NamePkgDescribed, NameMethod, NameProc, NameFunc
    } eNameType = NameUnknown;

    SAString sName = m_pCommand->CommandText();

    OCIDescribe *pDescr = NULL;
    Iora8Connection::Check(
        g_ora8API.OCIHandleAlloc(
        ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
        (dvoid**)&pDescr, OCI_HTYPE_DESCRIBE, 0, NULL),
        ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
        OCI_HTYPE_ENV);

    try
    {
        sword status;
        ub4 nDescNameLen;
        dvoid *szDescName;
        OCIParam *parmh = NULL;

        // try to search public synonyms
        long one = 1;
        status = g_ora8API.OCIAttrSet(pDescr, OCI_HTYPE_DESCRIBE,
            (dvoid *)&one, (ub4)4, OCI_ATTR_DESC_PUBLIC,
            m_handles.m_pOCIError);
        Iora8Connection::Check(status, m_handles.m_pOCIError, OCI_HTYPE_ERROR);

        SAString sPkg, sMethod, sName1, sName2;
        // if name is in the form of
        // a) ddd - it is global method ddd
        // b) aaa.bbb.ccc - it is a package aaa.bbb with method ccc
        // c) eee.fff than it can be a package eee or user(schema) eee
        size_t pos1 = sName.Find('.');
        size_t pos2;
        if (SIZE_MAX == pos1) // a)
        {
            eNameType = NameMethod;
            sMethod = sName;
        }
        else
        {
            pos2 = sName.Find('.', pos1 + 1);
            if (SIZE_MAX == pos2) // c)
            {
                sName1 = sName.Left(pos1);
                sName2 = sName.Mid(pos1 + 1);
            }
            else // b)
            {
                eNameType = NamePkg;
                sPkg = sName.Left(pos2);
                sMethod = sName.Mid(pos2 + 1);
            }
        }

        if (eNameType == NameUnknown)
        {
            // check package.method
#ifdef SA_UNICODE
            if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
                szDescName = (dvoid*)sName1.GetUTF16Chars(),
                nDescNameLen = (ub4)sName1.GetUTF16CharsLength()*sizeof(utext);
            else
#endif
                szDescName = (dvoid*)sName1.GetMultiByteChars(), nDescNameLen =
                (ub4)sName1.GetLength();

            status =
                g_ora8API.OCIDescribeAny(
                ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
                m_handles.m_pOCIError, szDescName, nDescNameLen,
                OCI_OTYPE_NAME, OCI_DEFAULT, OCI_PTYPE_PKG, pDescr);

            try
            {
                Iora8Connection::Check(status, m_handles.m_pOCIError,
                    OCI_HTYPE_ERROR);
            }
            catch (SAException &x)
            {
                if (x.ErrNativeCode() != 4043) // object "string" does not exist
                    throw;
            }

            if (status != OCI_SUCCESS)
            {
                try
                {
                    // if not package then may be synonym
                    Iora8Connection::Check(
                        g_ora8API.OCIDescribeAny(
                        ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
                        m_handles.m_pOCIError, szDescName,
                        nDescNameLen, OCI_OTYPE_NAME, OCI_DEFAULT,
                        OCI_PTYPE_SYN, pDescr),
                        m_handles.m_pOCIError, OCI_HTYPE_ERROR);

                    // get the parameter handle
                    Iora8Connection::Check(
                        g_ora8API.OCIAttrGet(pDescr, OCI_HTYPE_DESCRIBE,
                        &parmh, NULL, OCI_ATTR_PARAM,
                        m_handles.m_pOCIError),
                        m_handles.m_pOCIError, OCI_HTYPE_ERROR);

                    szDescName = NULL, nDescNameLen = 0;
                    status = g_ora8API.OCIAttrGet(parmh, OCI_DTYPE_PARAM,
                        &szDescName, &nDescNameLen, OCI_ATTR_SCHEMA_NAME,
                        m_handles.m_pOCIError);
                    sPkg = SAString(szDescName, nDescNameLen);
                    Iora8Connection::Check(status, m_handles.m_pOCIError,
                        OCI_HTYPE_ERROR);

                    szDescName = NULL, nDescNameLen = 0;
                    status = g_ora8API.OCIAttrGet(parmh, OCI_DTYPE_PARAM,
                        &szDescName, &nDescNameLen, OCI_ATTR_NAME,
                        m_handles.m_pOCIError);
                    sPkg += _TSA('.') + SAString(szDescName, nDescNameLen);
                    Iora8Connection::Check(status, m_handles.m_pOCIError,
                        OCI_HTYPE_ERROR);

                    sMethod = sName2;
                    eNameType = NamePkg;
                }
                catch (SAException &x)
                {
                    if (x.ErrNativeCode() != 4043) // object "string" does not exist
                        throw;
                }
            }
            else
            {
                eNameType = NamePkgDescribed;
                sPkg = sName1;
                sMethod = sName2;
            }

            if (status != OCI_SUCCESS)
            {
                eNameType = NameMethod;
                sMethod = sName;
        }
    }assert(eNameType != NameUnknown);

    // describe as package
    if (eNameType == NamePkg)
    {
        do
        {
#ifdef SA_UNICODE
            if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
                szDescName = (dvoid*)sPkg.GetUTF16Chars(),
                nDescNameLen = (ub4)sPkg.GetUTF16CharsLength()*sizeof(utext);
            else
#endif
                szDescName = (dvoid*)sPkg.GetMultiByteChars(), nDescNameLen =
                (ub4)sPkg.GetLength();

            status =
                g_ora8API.OCIDescribeAny(
                ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
                m_handles.m_pOCIError, szDescName, nDescNameLen,
                OCI_OTYPE_NAME, OCI_DEFAULT, OCI_PTYPE_PKG,
                pDescr);

            try
            {
                Iora8Connection::Check(status, m_handles.m_pOCIError,
                    OCI_HTYPE_ERROR);
                eNameType = NamePkgDescribed;
            }
            catch (SAException &x)
            {
                if (x.ErrNativeCode() != 4043) // object "string" does not exist
                    throw;
            }

            if (status != OCI_SUCCESS)
            {
                // if not package then may be synonym
                Iora8Connection::Check(
                    g_ora8API.OCIDescribeAny(
                    ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
                    m_handles.m_pOCIError, szDescName,
                    nDescNameLen, OCI_OTYPE_NAME, OCI_DEFAULT,
                    OCI_PTYPE_SYN, pDescr),
                    m_handles.m_pOCIError, OCI_HTYPE_ERROR);

                // get the parameter handle
                Iora8Connection::Check(
                    g_ora8API.OCIAttrGet(pDescr, OCI_HTYPE_DESCRIBE,
                    &parmh, NULL, OCI_ATTR_PARAM,
                    m_handles.m_pOCIError),
                    m_handles.m_pOCIError, OCI_HTYPE_ERROR);

                szDescName = NULL, nDescNameLen = 0;
                status = g_ora8API.OCIAttrGet(parmh, OCI_DTYPE_PARAM,
                    &szDescName, &nDescNameLen, OCI_ATTR_SCHEMA_NAME,
                    m_handles.m_pOCIError);
                sPkg = SAString(szDescName, nDescNameLen);
                Iora8Connection::Check(status, m_handles.m_pOCIError,
                    OCI_HTYPE_ERROR);

                szDescName = NULL, nDescNameLen = 0;
                status = g_ora8API.OCIAttrGet(parmh, OCI_DTYPE_PARAM,
                    &szDescName, &nDescNameLen, OCI_ATTR_NAME,
                    m_handles.m_pOCIError);
                sPkg += _TSA('.') + SAString(szDescName, nDescNameLen);
                Iora8Connection::Check(status, m_handles.m_pOCIError,
                    OCI_HTYPE_ERROR);

                eNameType = NameUnknown;
                continue;
            }
        } while (eNameType == NameUnknown);
    }

    OCIParam *sublisth = NULL;
    ub4 nSubs = 0, nSub;
    ub2 nOverload;

    switch (eNameType)
    {
    case NamePkgDescribed:
        // get the parameter handle
        Iora8Connection::Check(
            g_ora8API.OCIAttrGet(pDescr, OCI_HTYPE_DESCRIBE, &parmh,
            NULL, OCI_ATTR_PARAM, m_handles.m_pOCIError),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        // get parameter for list of subprograms
        Iora8Connection::Check(
            g_ora8API.OCIAttrGet(parmh, OCI_DTYPE_PARAM, &sublisth,
            NULL, OCI_ATTR_LIST_SUBPROGRAMS,
            m_handles.m_pOCIError), m_handles.m_pOCIError,
            OCI_HTYPE_ERROR);
        // How many subroutines are in this package?
        Iora8Connection::Check(
            g_ora8API.OCIAttrGet(sublisth, OCI_DTYPE_PARAM, &nSubs,
            NULL, OCI_ATTR_NUM_PARAMS, m_handles.m_pOCIError),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        // find our subprogram
        nOverload =
            (ub2)sa_strtol(m_pCommand->Option(_TSA("Overload")), NULL, 10);
        if (!nOverload)
            nOverload = 1; // first overload is the default one
        for (nSub = 0; nSub < nSubs; ++nSub)
        {
            Iora8Connection::Check(
                g_ora8API.OCIParamGet(sublisth, OCI_DTYPE_PARAM,
                m_handles.m_pOCIError, (dvoid**)&parmh, nSub),
                m_handles.m_pOCIError, OCI_HTYPE_ERROR);

            // Get the routine type and name
            ub4 nLen;
            SAString sMethodName;

#ifdef SA_UNICODE
            if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
            {
                utext *szNameWide = NULL;
                Iora8Connection::Check(g_ora8API.OCIAttrGet(
                    parmh, OCI_DTYPE_PARAM, &szNameWide, &nLen, OCI_ATTR_NAME,
                    m_handles.m_pOCIError), m_handles.m_pOCIError, OCI_HTYPE_ERROR);
                sMethodName.SetUTF16Chars(szNameWide, nLen / sizeof(utext));
            }
            else
#endif
            {
                char *szName = NULL;
                Iora8Connection::Check(
                    g_ora8API.OCIAttrGet(parmh, OCI_DTYPE_PARAM,
                    &szName, &nLen, OCI_ATTR_NAME,
                    m_handles.m_pOCIError),
                    m_handles.m_pOCIError, OCI_HTYPE_ERROR);
                sMethodName = SAString(szName, nLen);
            }

            if (sMethod.CompareNoCase(sMethodName) != 0)
                continue;
            if (--nOverload)
                continue; // the name matches but we need higher overload

            ub1 nSubType;
            Iora8Connection::Check(
                g_ora8API.OCIAttrGet(parmh, OCI_DTYPE_PARAM, &nSubType,
                NULL, OCI_ATTR_PTYPE, m_handles.m_pOCIError),
                m_handles.m_pOCIError, OCI_HTYPE_ERROR);
            switch (nSubType)
            {
            case OCI_PTYPE_PROC:
                eNameType = NameProc;
                break;
            case OCI_PTYPE_FUNC:
                eNameType = NameFunc;
                break;
            default:
                assert(false);
            }

            break;
            }

        if (eNameType == NamePkgDescribed)
        {
            // method was not found in the package
            // make next dummy call to get Oracle native error
#ifdef SA_UNICODE
            if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
                szDescName = (dvoid*)sName.GetUTF16Chars(),
                nDescNameLen = (ub4)sName.GetUTF16CharsLength()*sizeof(utext);
            else
#endif
                szDescName = (dvoid*)sName.GetMultiByteChars(), nDescNameLen =
                (ub4)sName.GetLength();

            Iora8Connection::Check(
                g_ora8API.OCIDescribeAny(
                ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
                m_handles.m_pOCIError, szDescName, nDescNameLen,
                OCI_OTYPE_NAME, OCI_DEFAULT, OCI_PTYPE_PROC,
                pDescr), m_handles.m_pOCIError,
                OCI_HTYPE_ERROR);

            assert(false);
        }
        break;
    case NameMethod:
        do
        {
            // first check for procedure
#ifdef SA_UNICODE
            if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
                szDescName = (dvoid*)sMethod.GetUTF16Chars(),
                nDescNameLen = (ub4)sMethod.GetUTF16CharsLength()*sizeof(utext);
            else
#endif
                szDescName = (dvoid*)sMethod.GetMultiByteChars(), nDescNameLen =
                (ub4)sMethod.GetLength();

            status =
                g_ora8API.OCIDescribeAny(
                ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
                m_handles.m_pOCIError, szDescName, nDescNameLen,
                OCI_OTYPE_NAME, OCI_DEFAULT, OCI_PTYPE_PROC,
                pDescr);

            try
            {
                Iora8Connection::Check(status, m_handles.m_pOCIError,
                    OCI_HTYPE_ERROR);
                eNameType = NameProc;
            }
            catch (SAException &x)
            {
                if (x.ErrNativeCode() != 4043) // object "string" does not exist
                    throw;
            }

            if (status != OCI_SUCCESS)
            {
                // if not procedure then may be function
                status =
                    g_ora8API.OCIDescribeAny(
                    ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
                    m_handles.m_pOCIError, szDescName,
                    nDescNameLen, OCI_OTYPE_NAME, OCI_DEFAULT,
                    OCI_PTYPE_FUNC, pDescr);

                try
                {
                    Iora8Connection::Check(status, m_handles.m_pOCIError,
                        OCI_HTYPE_ERROR);
                    eNameType = NameFunc;
                }
                catch (SAException &x)
                {
                    if (x.ErrNativeCode() != 4043) // object "string" does not exist
                        throw;
                }
            }

            if (status != OCI_SUCCESS)
            {
                // if not procedure then may be synonym
                Iora8Connection::Check(
                    g_ora8API.OCIDescribeAny(
                    ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
                    m_handles.m_pOCIError, szDescName,
                    nDescNameLen, OCI_OTYPE_NAME, OCI_DEFAULT,
                    OCI_PTYPE_SYN, pDescr),
                    m_handles.m_pOCIError, OCI_HTYPE_ERROR);

                // get the parameter handle
                Iora8Connection::Check(
                    g_ora8API.OCIAttrGet(pDescr, OCI_HTYPE_DESCRIBE,
                    &parmh, NULL, OCI_ATTR_PARAM,
                    m_handles.m_pOCIError),
                    m_handles.m_pOCIError, OCI_HTYPE_ERROR);

                szDescName = NULL, nDescNameLen = 0;
                status = g_ora8API.OCIAttrGet(parmh, OCI_DTYPE_PARAM,
                    &szDescName, &nDescNameLen, OCI_ATTR_SCHEMA_NAME,
                    m_handles.m_pOCIError);
                sMethod = SAString(szDescName, nDescNameLen);
                Iora8Connection::Check(status, m_handles.m_pOCIError,
                    OCI_HTYPE_ERROR);

                szDescName = NULL, nDescNameLen = 0;
                status = g_ora8API.OCIAttrGet(parmh, OCI_DTYPE_PARAM,
                    &szDescName, &nDescNameLen, OCI_ATTR_NAME,
                    m_handles.m_pOCIError);
                sMethod += _TSA('.') + SAString(szDescName, nDescNameLen);
                Iora8Connection::Check(status, m_handles.m_pOCIError,
                    OCI_HTYPE_ERROR);

                eNameType = NameUnknown;
                continue; // go to up
            }

            // get the parameter handle
            Iora8Connection::Check(
                g_ora8API.OCIAttrGet(pDescr, OCI_HTYPE_DESCRIBE, &parmh,
                NULL, OCI_ATTR_PARAM, m_handles.m_pOCIError),
                m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        } while (NameUnknown == eNameType);
        break;
    default:
        assert(false);
    }

    assert(eNameType == NameProc || eNameType == NameFunc);
    // now we can describe subprogram params
    OCIParam *phArgLst;
    ub2 nArgs;
    // Get the number of arguments and the arg list
    Iora8Connection::Check(
        g_ora8API.OCIAttrGet(parmh, OCI_DTYPE_PARAM, &phArgLst, NULL,
        OCI_ATTR_LIST_ARGUMENTS, m_handles.m_pOCIError),
        m_handles.m_pOCIError, OCI_HTYPE_ERROR);
    Iora8Connection::Check(
        g_ora8API.OCIAttrGet(phArgLst, OCI_DTYPE_PARAM, &nArgs, NULL,
        OCI_ATTR_NUM_PARAMS, m_handles.m_pOCIError),
        m_handles.m_pOCIError, OCI_HTYPE_ERROR);

    OCIParam *phArg;

#ifdef SA_UNICODE
    utext *szNameWide = NULL;
#endif
    char *szName = NULL;
    ub4 nLen;
    ub2 dtype, dsize;
    OCITypeParamMode iomode;
    sb1 scale;
    ub1 prec, hasdef = 0;

    for (int i = 0; i <= nArgs; ++i)
    {
        if (i == 0 && eNameType != NameFunc)
            continue;
        if (i == nArgs && eNameType != NameProc)
            break;

        Iora8Connection::Check(
            g_ora8API.OCIParamGet(phArgLst, OCI_DTYPE_PARAM,
            m_handles.m_pOCIError, (dvoid**)&phArg, i),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);

#ifdef SA_UNICODE
        if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
            Iora8Connection::Check(g_ora8API.OCIAttrGet(phArg,
            OCI_DTYPE_PARAM, &szNameWide, &nLen,
            OCI_ATTR_NAME, m_handles.m_pOCIError),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        else
#endif
            Iora8Connection::Check(
            g_ora8API.OCIAttrGet(phArg, OCI_DTYPE_PARAM, &szName, &nLen,
            OCI_ATTR_NAME, m_handles.m_pOCIError),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);

        Iora8Connection::Check(
            g_ora8API.OCIAttrGet(phArg, OCI_DTYPE_PARAM, &dtype, NULL,
            OCI_ATTR_DATA_TYPE, m_handles.m_pOCIError),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        Iora8Connection::Check(
            g_ora8API.OCIAttrGet(phArg, OCI_DTYPE_PARAM, &dsize, NULL,
            OCI_ATTR_DATA_SIZE, m_handles.m_pOCIError),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        Iora8Connection::Check(
            g_ora8API.OCIAttrGet(phArg, OCI_DTYPE_PARAM, &iomode, NULL,
            OCI_ATTR_IOMODE, m_handles.m_pOCIError),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        Iora8Connection::Check(
            g_ora8API.OCIAttrGet(phArg, OCI_DTYPE_PARAM, &prec, NULL,
            OCI_ATTR_PRECISION, m_handles.m_pOCIError),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        Iora8Connection::Check(
            g_ora8API.OCIAttrGet(phArg, OCI_DTYPE_PARAM, &scale, NULL,
            OCI_ATTR_SCALE, m_handles.m_pOCIError),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        /*
        Iora8Connection::Check(g_ora8API.OCIAttrGet(
        phArg, OCI_DTYPE_PARAM,
        &hasdef, NULL, OCI_ATTR_HAS_DEFAULT,
        m_handles.m_pOCIError),
        m_handles.m_pOCIError, OCI_HTYPE_ERROR);
        */

        SADataType_t eParamType = CnvtNativeToStd(dtype, 0, dsize, prec,
            scale);
        int nNativeType = dtype;
        SAParamDirType_t eDirType;
        switch (iomode)
        {
        case OCI_TYPEPARAM_IN:
            eDirType = SA_ParamInput;
            break;
        case OCI_TYPEPARAM_OUT:
            eDirType = SA_ParamOutput;
            break;
        case OCI_TYPEPARAM_INOUT:
            eDirType = SA_ParamInputOutput;
            break;
        default:
            assert(false);
            continue;
        }

        SAString sParamName;
        if (i == 0)
        {
            assert(nLen == 0);
            assert(eDirType == SA_ParamOutput);
            eDirType = SA_ParamReturn;
            sParamName = _TSA("RETURN_VALUE");
        }
        else
        {
#ifdef SA_UNICODE
            if (((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
                sParamName.SetUTF16Chars(szNameWide, nLen / sizeof(utext));
            else
#endif
                sParamName = SAString(szName, nLen);
        }

        if (SA_dtString == eParamType)
        {
#ifdef SA_UNICODE
            if (!((Iora8Connection*)m_pISAConnection)->m_bUseUCS2)
#endif
                dsize = (ub2)((dsize ? dsize : 4000)
                * ((Iora8Connection*)m_pISAConnection)->mb_cur_max);
        }

        SAParam& param = m_pCommand->CreateParam(sParamName, eParamType,
            nNativeType, dsize, prec, scale, eDirType);
        if (hasdef)
            param.setAsDefault();
        }

    try
    {
        Iora8Connection::Check(
            g_ora8API.OCIHandleFree(pDescr, OCI_HTYPE_DESCRIBE),
            ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCIEnv,
            OCI_HTYPE_ENV);
    }
    catch (SAException &)
    {
    }
        }
    catch (SAException &)
    { // clean up
        g_ora8API.OCIHandleFree(pDescr, OCI_HTYPE_DESCRIBE);
        throw;
    }
}

/*virtual */
int Iora7Cursor::CnvtStdToNative(SADataType_t eDataType) const
{
    sword ftype;

    switch (eDataType)
    {
    case SA_dtUnknown:
        throw SAException(SA_Library_Error, SA_Library_Error_UnknownDataType, -1, IDS_UNKNOWN_DATA_TYPE);
    case SA_dtBool:
        // there is no "native" boolean type in Oracle,
        // so treat boolean as 16-bit signed INTEGER in Oracle
        ftype = 3; // 16-bit signed integer
        break;
    case SA_dtShort:
        ftype = 3; // 16-bit signed integer
        break;
    case SA_dtUShort:
        ftype = 68; // 16-bit unsigned integer
        break;
    case SA_dtLong:
        ftype = 3; // 32-bit signed integer
        break;
    case SA_dtULong:
        ftype = 68; // 32-bit unsigned integer
        break;
    case SA_dtDouble:
        ftype = 4; // FLOAT
        break;
    case SA_dtNumeric:
        ftype = 6; // VARNUM
        break;
    case SA_dtDateTime:
        ftype = 12; // DATE
        break;
    case SA_dtString:
        // using 1 can cause problems when 
        // binding/comparing with CHAR(n) columns
        // like 'Select * from my_table where my_char_20 = :1'
        // if we used 1 we would have to pad bind value with spaces
        if (m_pCommand->CommandType() == SA_CmdSQLStmt)
            ftype = 96; // CHAR
        else
            ftype = 1; // VARCHAR2
        break;
    case SA_dtBytes:
        ftype = 23; // RAW
        break;
    case SA_dtLongBinary:
    case SA_dtBLob:
        ftype = 24; // LONG RAW
        break;
    case SA_dtLongChar:
    case SA_dtCLob:
        ftype = 8; // LONG
        break;
    case SA_dtCursor:
        ftype = 102; // cursor  type
        break;
    default:
        ftype = 0;
        assert(false);
    }

    return ftype;
}

/*virtual */
int Iora8Cursor::CnvtStdToNative(SADataType_t eDataType) const
{
    ub2 dty;

    switch (eDataType)
    {
    case SA_dtUnknown:
        throw SAException(SA_Library_Error, SA_Library_Error_UnknownDataType, -1, IDS_UNKNOWN_DATA_TYPE);
    case SA_dtBool:
        // there is no "native" boolean type in Oracle,
        // so treat boolean as 16-bit signed INTEGER in Oracle
        dty = SQLT_INT; // 16-bit signed integer
        break;
    case SA_dtShort:
        dty = SQLT_INT; // 16-bit signed integer
        break;
    case SA_dtUShort:
        dty = SQLT_UIN; // 16-bit usigned integer
        break;
    case SA_dtLong:
        dty = SQLT_INT; // 32-bit signed integer
        break;
    case SA_dtULong:
        dty = SQLT_UIN; // 32-bit unsigned integer
        break;
    case SA_dtDouble:
        // For native binary double, OCI 10.1 and higher required
        // but sometime people try to connect to olde server with new client
        // then ORA-03115 occurs
        // dty = NULL != g_ora8API.OCILobRead2 ? SQLT_BDOUBLE:SQLT_FLT;
        dty = SQLT_FLT;
        break;
    case SA_dtNumeric:
        dty = SQLT_VNU; // VARNUM
        break;
    case SA_dtDateTime:
        if (((Iora8Connection*)m_pISAConnection)->m_bUseTimeStamp)
            dty = SQLT_TIMESTAMP_TZ; // TIMESTAMP
        else
            dty = SQLT_DAT; // DATE
        break;
    case SA_dtString:
        // using SQLT_CHR can cause problems when 
        // binding/comparing with CHAR(n) columns
        // like 'Select * from my_table where my_char_20 = :1'
        // if we used SQLT_CHR we would have to pad bind value with spaces
        if (m_pCommand->CommandType() == SA_CmdSQLStmt)
            dty = SQLT_AFC; // CHAR
        else
            dty = SQLT_CHR; // VARCHAR2
        break;
    case SA_dtBytes:
        dty = SQLT_BIN; // RAW
        break;
    case SA_dtLongBinary:
        dty = SQLT_LBI; // LONG RAW
        break;
    case SA_dtBLob:
        dty = SQLT_BLOB; // Binary LOB
        break;
    case SA_dtLongChar:
        dty = SQLT_LNG; // LONG
        break;
    case SA_dtCLob:
        dty = SQLT_CLOB; // Character LOB
        break;
    case SA_dtCursor:
        dty = SQLT_RSET; // result set type
        break;
    default:
        dty = 0;
        assert(false);
    }

    return dty;
}

/*virtual */
saAPI *Iora7Connection::NativeAPI() const
{
    return &g_ora7API;
}

/*virtual */
saAPI *Iora8Connection::NativeAPI() const
{
    return &g_ora8API;
}

/*virtual */
saConnectionHandles *Iora7Connection::NativeHandles()
{
    return &m_handles;
}

/*virtual */
saConnectionHandles *Iora8Connection::NativeHandles()
{
    return &m_handles;
}

/*virtual */
saCommandHandles *Iora7Cursor::NativeHandles()
{
    return &m_handles;
}

/*virtual */
saCommandHandles *Iora8Cursor::NativeHandles()
{
    return &m_handles;
}

void IoraConnection::issueIsolationLevel(SAIsolationLevel_t eIsolationLevel)
{
    SAString sCmd(_TSA("SET TRANSACTION ISOLATION LEVEL "));
    SACommand cmd(m_pSAConnection);

    switch (eIsolationLevel)
    {
    case SA_ReadUncommitted:
        sCmd += _TSA("READ COMMITTED");
        break;
    case SA_ReadCommitted:
        sCmd += _TSA("READ COMMITTED");
        break;
    case SA_RepeatableRead:
        sCmd += _TSA("SERIALIZABLE");
        break;
    case SA_Serializable:
        sCmd += _TSA("SERIALIZABLE");
        break;
    default:
        assert(false);
        return;
    }

    cmd.setCommandText(sCmd);
    cmd.Execute();
    cmd.Close();
}

/*virtual */
void IoraConnection::setIsolationLevel(SAIsolationLevel_t eIsolationLevel)
{
    m_eSwitchToIsolationLevelAfterCommit = eIsolationLevel;
    Commit();
}

/*virtual */
void Iora7Connection::setAutoCommit(SAAutoCommit_t eAutoCommit)
{
    switch (eAutoCommit)
    {
    case SA_AutoCommitOff:
        Check(g_ora7API.ocof(&m_handles.m_lda), NULL);
        break;
    case SA_AutoCommitOn:
        Check(g_ora7API.ocon(&m_handles.m_lda), NULL);
        break;
    default:
        assert(false);
        return;
    }
}

/*virtual */
void Iora8Connection::setAutoCommit(SAAutoCommit_t eAutoCommit)
{
    switch (eAutoCommit)
    {
    case SA_AutoCommitOff:
        break;
    case SA_AutoCommitOn:
        break;
    default:
        assert(false);
        return;
    }
}

/*virtual */
void Iora7Connection::CnvtInternalToCursor(SACommand *pCursor,
    const void *pInternal)
{
    assert(pCursor);

    if (pCursor->Connection() == NULL)
        pCursor->setConnection(getSAConnection());

    CnvtInternalToCursor(*pCursor, (const Cda_Def *)pInternal);
}

/*static */
void Iora7Connection::CnvtInternalToCursor(SACommand &Cursor,
    const Cda_Def *pInternal)
{
    // We need this because Cursor.NativeHandles() opens it.
    bool bWasOpened = Cursor.isOpened();

    ora7CommandHandles *pH = (ora7CommandHandles *)Cursor.NativeHandles();

    if (!bWasOpened)
    {
        // Close newly created pH->m_pOCIStmt - we don't need it
        Check(g_ora7API.oclose(&pH->m_cda), &pH->m_cda);
    }
    else
    {
        // Clear all previous handle related SQLAPI++ data
        Cursor.setCommandText(_TSA(""));
    }

    // Assign newly fetched statement handle
    pH->m_cda = *pInternal;
    // Set the executed flag
    Cursor.m_bExecuted = true;

    // Set the result set flag
    Iora7Cursor* pISACursor = (Iora7Cursor*)m_pSAConnection->GetISACursor(
        &Cursor);
    pISACursor->m_bResultSetCanBe = pISACursor->m_bResultSetExist = true;
}

/*virtual */
void Iora8Connection::CnvtInternalToCursor(SACommand *pCursor,
    const void *pInternal)
{
    assert(pCursor);

    if (pCursor->Connection() == NULL)
        pCursor->setConnection(getSAConnection());

    CnvtInternalToCursor(*pCursor, *(const OCIStmt **)pInternal);
}

void Iora8Connection::CnvtInternalToCursor(SACommand &Cursor,
    const OCIStmt *pInternal)
{
    // We need this because Cursor.NativeHandles() opens it.
    bool bWasOpened = Cursor.isOpened();

    ora8CommandHandles *pH = (ora8CommandHandles *)Cursor.NativeHandles();

    if (!bWasOpened)
    {
        // Close newly created pH->m_pOCIStmt - we don't need it
        Iora8Connection::Check(
            g_ora8API.OCIHandleFree(pH->m_pOCIStmt, OCI_HTYPE_STMT),
            pH->m_pOCIStmt, OCI_HTYPE_STMT);
    }
    else
    {
        // Clear all previous handle related SQLAPI++ data
        Cursor.setCommandText(_TSA(""));
    }

    // Assign newly fetched statement handle
    pH->m_pOCIStmt = (OCIStmt *)pInternal;
    // Set the executed flag
    Cursor.m_bExecuted = true;

    // Set the result set and statement type flags
    Iora8Cursor* pISACursor = (Iora8Cursor*)m_pSAConnection->GetISACursor(
        &Cursor);
    pISACursor->m_bResultSetCanBe = true;
    Iora8Connection::Check(
        g_ora8API.OCIAttrGet(pH->m_pOCIStmt, OCI_HTYPE_STMT,
        &pISACursor->m_nOraStmtType, NULL, OCI_ATTR_STMT_TYPE,
        pH->m_pOCIError), pH->m_pOCIStmt, OCI_HTYPE_STMT);
}

void Iora8Cursor::FreeTemporaryLobsIfAny()
{
    while (m_cTempLobs)
    {
        Iora8Connection::Check(
            g_ora8API.OCILobFreeTemporary(
            ((Iora8Connection*)m_pISAConnection)->m_handles.m_pOCISvcCtx,
            m_handles.m_pOCIError, m_ppTempLobs[m_cTempLobs - 1]),
            m_handles.m_pOCIError, OCI_HTYPE_ERROR);

        g_ora8API.OCIDescriptorFree(m_ppTempLobs[m_cTempLobs - 1],
            OCI_DTYPE_LOB);

        --m_cTempLobs;
    }

    if (m_ppTempLobs)
    {
        ::free(m_ppTempLobs);
        m_ppTempLobs = NULL;
    }
}

void Iora8Cursor::FreeDateTimeParamsIfAny()
{
    while (m_cDateTimeParams)
    {
        Iora8Connection::Check(
            g_ora8API.OCIDescriptorFree(
            m_ppDateTimeParams[m_cDateTimeParams - 1],
            OCI_DTYPE_TIMESTAMP_TZ),
            m_ppDateTimeParams[m_cDateTimeParams - 1], OCI_DTYPE_TIMESTAMP_TZ);

        --m_cDateTimeParams;
    }

    if (m_ppDateTimeParams)
    {
        ::free(m_ppDateTimeParams);
        m_ppDateTimeParams = NULL;
    }
}

/*virtual */
void Iora8Connection::CnvtInternalToDateTime(SADateTime &date_time,
    const void *pInternal, int nInternalSize)
{
    if (m_bUseTimeStamp && (size_t)nInternalSize != sizeof(OraDATE_t))
    {
        sb2 year;
        ub1 month, day;

        g_ora8API.OCIDateTimeGetDate(m_handles.m_pOCISession,
            m_handles.m_pOCIError, *((OCIDateTime**)pInternal),
            &year, &month, &day);

        ub1 hour, min, sec;
        ub4 fsec;

        g_ora8API.OCIDateTimeGetTime(m_handles.m_pOCISession,
            m_handles.m_pOCIError, *((OCIDateTime**)pInternal),
            &hour, &min, &sec, &fsec);

        sb1 tz_hour = 0;
        sb1 tz_min = 0;

        g_ora8API.OCIDateTimeGetTimeZoneOffset(m_handles.m_pOCISession,
            m_handles.m_pOCIError, *((OCIDateTime**)pInternal),
            &tz_hour, &tz_min);

        date_time = SADateTime(year, month, day, hour, min, sec);
        date_time.Fraction() = fsec;

        if (0 != tz_hour || 0 != tz_min)
            date_time.Timezone().Format(_TSA("%") SA_FMT_STR _TSA("%02u:%02u"),
            (tz_hour < 0 || tz_min < 0) ? _TSA("-") : _TSA("+"), abs(tz_hour), abs(tz_min));

        return;
    }

    IoraConnection::CnvtInternalToDateTime(date_time,
        (const OraDATE_t*)pInternal);
    }

void Iora8Connection::CnvtDateTimeToInternal(const SADateTime &date_time,
    OCIDateTime *pInternal)
{
    if (m_bUseTimeStamp)
    {
        SAString sTZ = date_time.Timezone();
        size_t tzLen =
#ifdef SA_UNICODE
            m_bUseUCS2 ? (sTZ.GetUTF16CharsLength()*sizeof(utext)) :
#endif
            sTZ.GetLength();


        OraText *tz = tzLen ? (OraText*)(
#ifdef SA_UNICODE
            m_bUseUCS2 ? sTZ.GetUTF16Chars() :
#endif
            sTZ.GetMultiByteChars()

            ) : NULL;

        Check(
            g_ora8API.OCIDateTimeConstruct(m_handles.m_pOCISession,
            m_handles.m_pOCIError, pInternal,
            (sb2)date_time.GetYear(), (ub1)date_time.GetMonth(),
            (ub1)date_time.GetDay(), (ub1)date_time.GetHour(),
            (ub1)date_time.GetMinute(),
            (ub1)date_time.GetSecond(), (ub4)date_time.Fraction(),
            tz, tzLen), m_handles.m_pOCIError, OCI_HTYPE_ERROR);
    }
}

/*virtual */
void IoraConnection::CnvtInternalToNumeric(SANumeric &numeric,
    const void *pInternal, int/* nInternalSize*/)
{
    OraVARNUM_t &OraVARNUM = *(OraVARNUM_t *)pInternal;

    unsigned int OraNumSize = OraVARNUM[0];
    assert(OraNumSize > 0);
    // at least exponent byte

    unsigned char ExponentByte = OraVARNUM[1];
    if (0x80 == ExponentByte) // special case in Orcale - zero
    {
        memset(numeric.val, 0, sizeof(numeric.val));
        numeric.precision = 1;
        numeric.scale = 0;
        numeric.sign = 1; // positive
        return;
    }

    bool PositiveBit = (ExponentByte & 0x80) != 0;
    unsigned char ExponentOffset65 = (unsigned char)(ExponentByte & 0x7F);
    unsigned int MantissaSize = OraNumSize - 1; // minus exponent byte
    int Exponent;
    if (!PositiveBit
        && (MantissaSize < 20
        || (MantissaSize == 20 && OraVARNUM[21] == 102)))
        --MantissaSize; // ignore trailing 102 byte
    if (!PositiveBit)
        Exponent = 62 - ExponentOffset65; // ???, not found in Oracle docs, just expreriment
    else
        Exponent = ExponentOffset65 - 65;

    unsigned char *pOracleCodedDigits = OraVARNUM + 1 + 1;

    unsigned char OracleCodedDigits[128]; // this is enough for the biggest number Oracle can support
    memset(OracleCodedDigits, PositiveBit ? 1 : 101, sizeof(OracleCodedDigits));
    memcpy(OracleCodedDigits, pOracleCodedDigits, MantissaSize);

    int OracleScale = MantissaSize - 1 - Exponent;

    // SANumeric class does not support
    // negative scale, so we have to adjust:
    // we will increase scale till zero and
    // at the same time "virtually" increase mantissa
    int AdjustedOracleScale = OracleScale;
    unsigned int AdjustedMantissaSize = MantissaSize;
    while (AdjustedOracleScale < 0)
    {
        ++AdjustedMantissaSize;
        ++AdjustedOracleScale;
    }

    unsigned char Precision10 = (unsigned char)(AdjustedMantissaSize * 2);
    unsigned char Scale10 = (unsigned char)((AdjustedMantissaSize - 1
        - Exponent) * 2);

    // convert Oracle 100-based digits into little endian integer

    // to make conversion easier first convert
    // Oracle 100-based number into 10000-based little endian
    unsigned short Num10000[sizeof(OracleCodedDigits) / sizeof(short)];
    memset(Num10000, 0, sizeof(Num10000));
    int Num10000pos = 0;
    for (int i = AdjustedMantissaSize - 1; i >= 0; i -= 2)
    {
        int Num100 =
            PositiveBit ? OracleCodedDigits[i] - 1 :
            101 - OracleCodedDigits[i];
        assert(Num100 >= 0 && Num100 < 100);
        Num10000[Num10000pos] = (unsigned char)Num100;
        if (i > 0)
        {
            Num100 =
                PositiveBit ? OracleCodedDigits[i - 1] - 1 :
                101 - OracleCodedDigits[i - 1];
            Num10000[Num10000pos] = (unsigned short)(Num10000[Num10000pos]
                + (Num100 * 100));
        }

        ++Num10000pos;
    }

    numeric.precision = Precision10;
    numeric.scale = Scale10;
    // Now let us set sign
    // Oracle: The high-order bit of the exponent byte is the sign bit;
    // it is set for positive numbers.
    // SQLAPI++: numeric.sign is 1 for positive numbers, 0 for negative numbers
    numeric.sign = (unsigned char)(PositiveBit ? 1 : 0);

    // convert mantissa
    memset(numeric.val, 0, sizeof(numeric.val));
    int Num256pos = 0;
    while (!AllBytesAreZero(Num10000, sizeof(Num10000)))
    {
        unsigned short Reminder;
        LittleEndian10000BaseDivide(
            (unsigned int)(sizeof(Num10000) / sizeof(short)), Num10000,
            256, Num10000, &Reminder);

        assert(Reminder == (unsigned char)Reminder);
        // no truncation here
        numeric.val[Num256pos++] = (unsigned char)Reminder;
        if ((size_t)Num256pos == sizeof(numeric.val))
            break; // we have to truncate the number
    }
}

// extract two digits and convert them to 100-based number
// adjust pDigits to point to the digits net yet extracted
// if bLeadingZero is true then first digit is assumed to be zero, second is really extracted
static int Extract100BaseDigit(const SAChar *&pDigits,
    bool bLeadingZero = false)
{
    int n1 = 0;
    int n2 = 0;

    if (*pDigits != 0)
    {
        if (!bLeadingZero)
        {
            n1 = *pDigits - _TSA('0');
            ++pDigits;
        }

        if (*pDigits != 0)
        {
            n2 = *pDigits - _TSA('0');
            ++pDigits;
        }
    }

    assert(n1 >= 0 && n1 <= 9);
    assert(n2 >= 0 && n2 <= 9);
    int n = n1 * 10 + n2;

    return n;
}

/*static */
void IoraConnection::CnvtNumericToInternal(const SANumeric &numeric,
    OraVARNUM_t &Internal)
{
    if (AllBytesAreZero(numeric.val, sizeof(numeric.val)))
    {
        // special case in Orcale - zero
        Internal[0] = 1;
        Internal[1] = 0x80;
        return;
    }

    // first get numeric as a string, it will be easier to convert
    SAString s = numeric;

    // we need to convert digits to 100-base format
    // and normalize the number: xx.xxxx,
    // where 01 <= xx <= 99
    size_t nDecimalPointPos = s.Find(_TSA('.'));
    if (SIZE_MAX == nDecimalPointPos) // no decimal point
        nDecimalPointPos = s.GetLength(); // assumed
    else
        s.TrimRight(_TSA('0')); // trailing zeros after decimal point are not interesting for Oracle
    size_t nWherePrecStarts = s.FindOneOf(_TSA("123456789"));
    assert(nWherePrecStarts != SIZE_MAX);
    size_t nWhereDigitsStart = s.FindOneOf(_TSA("0123456789"));
    assert(nWhereDigitsStart != SIZE_MAX);

    const SAChar *sNumBuf = s;
    bool bPositive = *sNumBuf != _TSA('-');
    const SAChar *pDigits = sNumBuf + nWhereDigitsStart;

    // skip leading zeros
    while ((_TSA('0') == *pDigits || _TSA('.') == *pDigits)
        && size_t(pDigits - sNumBuf) <= nDecimalPointPos)
        ++pDigits;

    // this is the exponent in Oracle understanding,
    // but not Oracle-coded
    int nExponent = 0;
    unsigned int nVARNUMSize; // VARNUM actual size
    nVARNUMSize = 1; // at least the exponent byte

    int n, o; // normal number and Oracle-coded
    if (nWherePrecStarts > nDecimalPointPos) // [-].xxxxx or [-].000..0xxxxx
    {
        // move decimal point right until we normalize
        do
        {
            n = Extract100BaseDigit(pDigits);
            o = bPositive ? n + 1 : 101 - n;
            ++nExponent;
        } while (n == 0 && *pDigits);

        if (nVARNUMSize + 1 < sizeof(Internal) / sizeof(Internal[0]))
            Internal[++nVARNUMSize] = (unsigned char)o;
    }
    else // [-]xxxx.xxxx or [-]xxxx
    {
        size_t nWholeDigits = nDecimalPointPos - nWherePrecStarts;
        n = Extract100BaseDigit(pDigits, nWholeDigits % 2 != 0);
        o = bPositive ? n + 1 : 101 - n;
        if (nVARNUMSize + 1 < sizeof(Internal) / sizeof(Internal[0]))
            Internal[++nVARNUMSize] = (unsigned char)o;

        // we might need to move decimal point left to normalize
        while (*pDigits && *pDigits != _TSA('.'))
        {
            n = Extract100BaseDigit(pDigits);
            o = bPositive ? n + 1 : 101 - n;
            if (nVARNUMSize + 1 < sizeof(Internal) / sizeof(Internal[0]))
                Internal[++nVARNUMSize] = (unsigned char)o;
            --nExponent;
        }

        if (*pDigits == _TSA('.'))
            ++pDigits;
    }

    // these are to the right of the decimal point,
    // number has already been normalized, so
    // we only try to keep as much of precision as possible
    while (*pDigits && nVARNUMSize < sizeof(Internal) / sizeof(Internal[0]))
    {
        n = Extract100BaseDigit(pDigits);
        o = bPositive ? n + 1 : 101 - n;
        if (nVARNUMSize + 1 < sizeof(Internal) / sizeof(Internal[0]))
            Internal[++nVARNUMSize] = (unsigned char)o;
    }

    // normalize Oracle number - trailing zeros should not be present
    int oZero = bPositive ? 0 + 1 : 101 - 0;
    while (Internal[nVARNUMSize] == oZero)
        --nVARNUMSize;

    if (numeric.sign == 0) //	negative
    {
        // if a space is left for a trailing '102' byte
        // we must add it for negative numbers (only)
        if (nVARNUMSize + 1 < sizeof(Internal) / sizeof(Internal[0]))
            Internal[++nVARNUMSize] = 102;
    }

    // set VARNUM size byte
    Internal[0] = (unsigned char)nVARNUMSize;
    // set the exponent byte (and sign as well)
    Internal[1] = (unsigned char)(bPositive ? 193 - nExponent : 62 + nExponent);
}

//////////////////////////////////////////////////////////////////////
// ora8ExternalConnection Class
//////////////////////////////////////////////////////////////////////

ora8ExternalConnection::ora8ExternalConnection(
    SAConnection *pCon,
    OCIEnv *pOCIEnv,
    OCISvcCtx *pOCISvcCtx)
{
    m_bAttached = false;

    m_pCon = pCon;

    m_pOCIEnv = pOCIEnv;
    m_pOCISvcCtx = pOCISvcCtx;
}

void ora8ExternalConnection::Attach()
{
    Detach();	// if any

    if (m_pCon->isConnected())
        m_pCon->Disconnect();
    m_pCon->setOption(_TSA("UseAPI")) = _TSA("OCI8");
    m_pCon->setClient(SA_Oracle_Client);

    assert(m_pCon->isConnected());
    ((Iora8Connection*)m_pCon->m_pISAConnection)->Attach(*this);
    m_bAttached = false;
}

void ora8ExternalConnection::Detach()
{
    if (!m_bAttached)
        return;

    assert(m_pCon->isConnected());
    ((Iora8Connection*)m_pCon->m_pISAConnection)->Detach();

    m_bAttached = false;
}

ora8ExternalConnection::~ora8ExternalConnection()
{
    try
    {
        Detach();
    }
    catch (SAException &)
    {
    }
}
