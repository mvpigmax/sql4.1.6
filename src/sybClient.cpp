// sybClient.cpp: implementation of the IsybClient class.
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <sybAPI.h>
#include "sybClient.h"

#define SA_SYBOPT_RESULT_TYPE _TSA("SybaseResultType")
#define SA_SYBOPT_RESULT_COUNT _TSA("SybaseResultCount")

static int g_nLongDataMax = 1024;

SASybErrInfo::SASybErrInfo()
{
    msgnumber = 0;
    line = -1;

    fMsgHandler = NULL;
    pMsgAddInfo = NULL;
}

/*virtual */
SASybErrInfo::~SASybErrInfo()
{
}

int& sybAPI::DefaultLongMaxLength()
{
    return g_nLongDataMax;
}

SASybErrInfo* getSybErrInfo(CS_CONTEXT *context, CS_CONNECTION * connection)
{
    SASybErrInfo* pSybErrInfo = NULL;
    // get (allocated) user data where to return error info
    if (connection)
        g_sybAPI.ct_con_props(
        connection,
        CS_GET,
        CS_USERDATA,
        &pSybErrInfo,
        (CS_INT)sizeof(SASybErrInfo *),
        NULL);
    if (!pSybErrInfo && context)
        g_sybAPI.cs_config(
        context,
        CS_GET,
        CS_USERDATA,
        &pSybErrInfo,
        (CS_INT)sizeof(SASybErrInfo *),
        NULL);

    assert(pSybErrInfo);
    return pSybErrInfo;
}

void sybAPI::SetMessageCallback(saSybMsgHandler_t fHandler, void *pAddInfo, SAConnection *pCon/* = NULL*/)
{
    if (NULL == pCon)
    {
        SACriticalSectionScope cs(&g_sybAPI.errorInfo);
        g_sybAPI.errorInfo.fMsgHandler = fHandler;
        g_sybAPI.errorInfo.pMsgAddInfo = pAddInfo;
    }
    else if (pCon->isConnected() && SA_Sybase_Client == pCon->Client())
    {
        sybConnectionHandles* pHCon = (sybConnectionHandles*)pCon->NativeHandles();
        SASybErrInfo *pSybErrInfo = getSybErrInfo(pHCon->m_context, pHCon->m_connection);
        if (NULL != pSybErrInfo)
        {
            SACriticalSectionScope cs(pSybErrInfo);
            pSybErrInfo->fMsgHandler = fHandler;
            pSybErrInfo->pMsgAddInfo = pAddInfo;
        }
    }
}

CS_RETCODE CS_PUBLIC DefaultClientMsg_cb(
    CS_CONTEXT *context, CS_CONNECTION *connection, CS_CLIENTMSG *message)
{
    SASybErrInfo *pSybErrInfo = getSybErrInfo(context, connection);
    SACriticalSectionScope cs(pSybErrInfo);

    if (pSybErrInfo->msgnumber == 0	// only if no error is waiting to be handled yet
        && message->severity != CS_SV_INFORM)
    {
        int nLength = sa_min(message->msgstringlen, CS_MAX_MSG);
        pSybErrInfo->msgnumber = message->msgnumber;
        strncpy(pSybErrInfo->msgstring, message->msgstring, nLength);
        pSybErrInfo->msgstring[nLength] = 0;
        pSybErrInfo->line = -1;

        if (CS_SEVERITY(message->msgnumber) == CS_SV_RETRY_FAIL &&
            CS_NUMBER(message->msgnumber) == 63 &&
            CS_ORIGIN(message->msgnumber) == 2 &&
            CS_LAYER(message->msgnumber) == 1)
        {
            CS_INT status = 0;
            if (g_sybAPI.ct_con_props(connection, CS_GET, CS_LOGIN_STATUS,
                &status, CS_UNUSED, NULL) == CS_SUCCEED && status)
            {
                // command timeout
                g_sybAPI.ct_cancel(connection, NULL, CS_CANCEL_ATTN);
            }
        }
    }

    if (NULL != pSybErrInfo->fMsgHandler)
        pSybErrInfo->fMsgHandler(message, false, pSybErrInfo->pMsgAddInfo);
    else if (NULL != g_sybAPI.errorInfo.fMsgHandler)
        g_sybAPI.errorInfo.fMsgHandler(message, false, g_sybAPI.errorInfo.pMsgAddInfo);

    return CS_SUCCEED;
}

CS_RETCODE CS_PUBLIC DefaultServerMsg_cb(
    CS_CONTEXT *context, CS_CONNECTION *connection, CS_SERVERMSG *message)
{
    SASybErrInfo *pSybErrInfo = getSybErrInfo(context, connection);
    SACriticalSectionScope cs(pSybErrInfo);

    if (pSybErrInfo->msgnumber == 0	// only if no error is waiting to be handled yet
        && message->severity > 10)
    {
        pSybErrInfo->msgnumber = message->msgnumber;
        if (message->proclen)
        {
            int nLength = sa_min(message->proclen, CS_MAX_MSG - 2);
            strncpy(pSybErrInfo->msgstring, message->proc, nLength);
            pSybErrInfo->msgstring[nLength] = 0;
            strcat(pSybErrInfo->msgstring, ": ");
            strncat(pSybErrInfo->msgstring, message->text, sa_min(message->textlen, CS_MAX_MSG - nLength - 2));
        }
        else
        {
            int nLength = sa_min(message->textlen, CS_MAX_MSG);
            strncpy(pSybErrInfo->msgstring, message->text, nLength);
            pSybErrInfo->msgstring[nLength] = 0;
        }
        pSybErrInfo->line = message->line;
    }

    if (NULL != pSybErrInfo->fMsgHandler)
        pSybErrInfo->fMsgHandler(message, true, pSybErrInfo->pMsgAddInfo);
    else if (NULL != g_sybAPI.errorInfo.fMsgHandler)
        g_sybAPI.errorInfo.fMsgHandler(message, true, g_sybAPI.errorInfo.pMsgAddInfo);

    return CS_SUCCEED;
}

//////////////////////////////////////////////////////////////////////
// IsybConnection Class
//////////////////////////////////////////////////////////////////////

class IsybConnection : public ISAConnection
{
    friend class IsybCursor;
    friend class IsybClient;

    sybConnectionHandles m_handles;

    SASybErrInfo errorInfo;

    SAString	m_sServerName;
    SAString	m_sDatabase;

    enum ServerType { eUnknown, eASE, eASA } m_type;
    ServerType getServerType();
    ServerType getServerTypeConst(ServerType& type) const;

    CS_RETCODE Check(CS_RETCODE rcd, CS_CONNECTION* connection = NULL) const;
    CS_RETCODE CheckSilent(CS_RETCODE rcd, CS_CONNECTION* connection = NULL) const;
    CS_RETCODE CheckBatch(CS_RETCODE rcd, ISACursor *pCursor) const;
    SAString ConvertToString(
        CS_INT srctype, CS_VOID	*src, CS_INT srclen);
    CS_INT GetClientLibraryVersion();

    void CnvtNumericToInternal(
        const SANumeric &numeric,
        CS_NUMERIC &Internal,
        CS_INT &datalen);
    static void CnvtDateTimeToInternal(
        const SADateTime &date_time,
        SAString &sInternal);
    void CnvtDateTimeToInternal(
        const SADateTime &date_time,
        CS_DATETIME *pInternal);
    void CnvtInternalToDateTime(
        SADateTime &date_time,
        const CS_DATETIME &time);

    enum LongPiece { MaxLongPiece = (unsigned int)0x7FFFFFFC };

protected:
    virtual ~IsybConnection();

public:
    IsybConnection(SAConnection *pSAConnection);

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

IsybConnection::IsybConnection(
    SAConnection *pSAConnection) : ISAConnection(pSAConnection)
{
    Reset();
}

IsybConnection::~IsybConnection()
{
}

IsybConnection::ServerType IsybConnection::getServerType()
{
    if (eUnknown == m_type)
        m_type = getServerTypeConst(m_type);
    return m_type;
}

IsybConnection::ServerType IsybConnection::getServerTypeConst(ServerType& type) const
{
    SACommand cmd(m_pSAConnection, _TSA("\
if OBJECT_ID('sys.sysprocedure') IS NOT NULL and \
OBJECT_ID('sys.sysprocparm') IS NOT NULL \
select 2 \
else if OBJECT_ID('dbo.sysobjects') IS NOT NULL and \
OBJECT_ID('dbo.syscolumns') IS NOT NULL \
select 1 \
else \
select 0"));

    // ct_cursor cannot be used because of the DBMS error
    //cmd.setOption(_TSA("ct_cursor")) = _TSA("svrtype");

    type = eUnknown;

    try
    {
        cmd.Execute();
        if (cmd.FetchNext())
            type = (ServerType)cmd[1].asLong();
    }
    catch (SAException &)
    {
    }

    return type;
}

CS_INT IsybConnection::GetClientLibraryVersion()
{
    SAString sCS_VERSION = m_pSAConnection->Option(_TSA("CS_VERSION"));
#ifdef CS_VERSION_160
    if(sCS_VERSION.CompareNoCase(_TSA("CS_VERSION_160")) == 0)
        return CS_VERSION_160;
#endif
#ifdef CS_VERSION_157
    if(sCS_VERSION.CompareNoCase(_TSA("CS_VERSION_157")) == 0)
        return CS_VERSION_157;
#endif
#ifdef CS_VERSION_155
    if(sCS_VERSION.CompareNoCase(_TSA("CS_VERSION_155")) == 0)
        return CS_VERSION_155;
#endif
#ifdef CS_VERSION_150
    if(sCS_VERSION.CompareNoCase(_TSA("CS_VERSION_150")) == 0)
        return CS_VERSION_150;
#endif
#ifdef CS_VERSION_125
    if(sCS_VERSION.CompareNoCase(_TSA("CS_VERSION_125")) == 0)
        return CS_VERSION_125;
#endif
    if (sCS_VERSION.CompareNoCase(_TSA("CS_VERSION_110")) == 0)
        return CS_VERSION_110;
    if (sCS_VERSION.CompareNoCase(_TSA("CS_VERSION_100")) == 0)
        return CS_VERSION_100;

    CS_INT version = 0;

    if (sCS_VERSION.CompareNoCase(_TSA("Detect")) == 0)
    {
        // allocate context
        CS_CONTEXT *context = NULL;
        // should always be supported
        CS_RETCODE rc = g_sybAPI.cs_ctx_alloc(CS_VERSION_100, &context);
        if (rc == CS_SUCCEED)
        {
            rc = g_sybAPI.ct_init(context, CS_VERSION_100);
            if (rc == CS_SUCCEED)
            {
                char sVer[1024];
                rc = g_sybAPI.ct_config(
                    context,
                    CS_GET, CS_VER_STRING,
                    sVer, 1024,
                    NULL);
                if (rc == CS_SUCCEED)
                {
                    long nVersion = SAExtractVersionFromString(sVer);
#ifdef CS_VERSION_160
                    if(nVersion >= 0x000F0007)
                        version = CS_VERSION_160;
                    else
#endif
#ifdef CS_VERSION_157
                    if(nVersion >= 0x000F0007)
                        version = CS_VERSION_157;
                    else
#endif
#ifdef CS_VERSION_155
                        if(nVersion >= 0x000F0005)
                            version = CS_VERSION_155;
                        else
#endif
#ifdef CS_VERSION_150
                            if(nVersion >= 0x000F0000)
                                version = CS_VERSION_150;
                            else
#endif
#ifdef CS_VERSION_125
                                if(nVersion >= 0x000C0005)
                                    version = CS_VERSION_125;
                                else
#endif
                                    if (nVersion >= 0x000B0000)
                                        version = CS_VERSION_110;
                                    else
                                    {
                                        assert(nVersion >= 0x000A0000);
                                        version = CS_VERSION_100;
                                    }
                }
            }
        }

        // clean-up
        if (context)
        {
            g_sybAPI.ct_exit(context, CS_UNUSED);
            g_sybAPI.cs_ctx_drop(context);
        }
    }

    return version;
}

/*virtual */
void IsybConnection::InitializeClient()
{
    ::AddSybSupport(m_pSAConnection);

    assert(m_handles.m_context == NULL);

    SACriticalSectionScope cs(&g_sybAPI.errorInfo);

    // allocate context
    CS_CONTEXT *context = NULL;

    try
    {
        CS_INT nClientLibraryVersion = GetClientLibraryVersion();

        // version is initialized here only to prevent compiler's warning
        CS_INT version = CS_VERSION_110;
        CS_RETCODE rc = CS_FAIL;

#ifdef CS_VERSION_160
        // try 16.0 behavior
        if(!nClientLibraryVersion || nClientLibraryVersion == CS_VERSION_160)
        {
            version = CS_VERSION_160;
            rc = g_sybAPI.cs_ctx_alloc(version, &context);
        }
#endif

#ifdef CS_VERSION_157
        if(rc == CS_FAIL) // 15.7 behavior not supported?
        {

            // try 15.7 behavior
            if(!nClientLibraryVersion || nClientLibraryVersion == CS_VERSION_157)
            {
                version = CS_VERSION_157;
                rc = g_sybAPI.cs_ctx_alloc(version, &context);
            }
#endif

#ifdef CS_VERSION_155
            if(rc == CS_FAIL) // 15.7 behavior not supported?
            {
                // try 15.5 behavior
                if(!nClientLibraryVersion || nClientLibraryVersion == CS_VERSION_155)
                {
                    version = CS_VERSION_155;
                    rc = g_sybAPI.cs_ctx_alloc(version, &context);
                }
#endif

#ifdef CS_VERSION_150
                if(rc == CS_FAIL)       // 15.5 behavior not supported?
                {
                    // try 15.0 behavior
                    if(!nClientLibraryVersion || nClientLibraryVersion == CS_VERSION_150)
                    {
                        version = CS_VERSION_150;
                        rc = g_sybAPI.cs_ctx_alloc(version, &context);
                    }
#endif
#ifdef CS_VERSION_125
                    if(rc == CS_FAIL)       // 15.0 behavior not supported?
                    {
                        // try 12.5 behavior
                        if(!nClientLibraryVersion || nClientLibraryVersion == CS_VERSION_125)
                        {
                            version = CS_VERSION_125;
                            rc = g_sybAPI.cs_ctx_alloc(version, &context);
                        }
#endif
                        if (rc == CS_FAIL)	// 12.5 behavior not supported?
                        {
                            // try 11.0 behavior
                            if (!nClientLibraryVersion || nClientLibraryVersion == CS_VERSION_110)
                            {
                                version = CS_VERSION_110;
                                rc = g_sybAPI.cs_ctx_alloc(version, &context);
                            }
                            if (rc == CS_FAIL)	// 11.0 behavior not supported?
                            {
                                // try 10.0 behavior
                                if (!nClientLibraryVersion || nClientLibraryVersion == CS_VERSION_100)
                                {
                                    version = CS_VERSION_100;
                                    rc = g_sybAPI.cs_ctx_alloc(version, &context);
                                }
                            }
                        }
#ifdef CS_VERSION_125
                    }
#endif
#ifdef CS_VERSION_150
                }
#endif
#ifdef CS_VERSION_155
            }
#endif
#ifdef CS_VERSION_157
        }
#endif
        if (rc == CS_MEM_ERROR)
            throw SAException(SA_Library_Error, -1, -1, _TSA("cs_ctx_alloc -> CS_MEM_ERROR"));
        if (rc == CS_FAIL)
            throw SAException(SA_Library_Error, -1, -1, _TSA("cs_ctx_alloc -> CS_FAIL"));
        assert(rc == CS_SUCCEED);

        rc = g_sybAPI.ct_init(context, version);
        if (rc == CS_MEM_ERROR)
            throw SAException(SA_Library_Error, -1, -1, _TSA("ct_init -> CS_MEM_ERROR"));
        if (rc == CS_FAIL)
            throw SAException(SA_Library_Error, -1, -1, _TSA("ct_init -> CS_FAIL"));
        assert(rc == CS_SUCCEED);

        /*
        // set (allocated) user data where to return error info
        SASybErrInfo *pSybErrInfo = (SybErrInfo_t*)malloc(sizeof(SybErrInfo_t));
        memset(pSybErrInfo, 0, sizeof(SybErrInfo_t));
        */

        SASybErrInfo *pSybErrInfo = &g_sybAPI.errorInfo;
        g_sybAPI.cs_config(
            context,
            CS_SET,
            CS_USERDATA,
            &pSybErrInfo,
            (CS_INT)sizeof(SASybErrInfo*),
            NULL);

        // set context level callbacks
        g_sybAPI.ct_callback(
            context, NULL, CS_SET, CS_CLIENTMSG_CB,
            (CS_VOID *)DefaultClientMsg_cb);
        g_sybAPI.ct_callback(
            context, NULL, CS_SET, CS_SERVERMSG_CB,
            (CS_VOID *)DefaultServerMsg_cb);

        m_handles.m_context = context;
    }
    catch (SAException &)
    {
        // clean up
        if (context)
        {
            CS_RETCODE retcode = g_sybAPI.ct_exit(context, CS_UNUSED);
            assert(retcode == CS_SUCCEED);
            if (retcode != CS_SUCCEED)
                g_sybAPI.ct_exit(context, CS_FORCE_EXIT);

            g_sybAPI.cs_ctx_drop(context);
        }

        throw;
    }
}

/*virtual */
void IsybConnection::UnInitializeClient()
{
    assert(m_handles.m_context != NULL);

    SACriticalSectionScope cs(&g_sybAPI.errorInfo);

    Check(g_sybAPI.ct_exit(m_handles.m_context, CS_UNUSED));

    /*
    // free user data where to return error info
    SybErrInfo_t *pSybErrInfo = getSybErrInfo(m_handles.m_context, NULL);
    if( pSybErrInfo )
    free(pSybErrInfo);
    */

    g_sybAPI.cs_ctx_drop(m_handles.m_context);
    m_handles.m_context = NULL;

    if (SAGlobals::UnloadAPI())
        ::ReleaseSybSupport();
}

CS_RETCODE IsybConnection::CheckBatch(
    CS_RETCODE rcd,
    ISACursor *pCursor) const
{
    try
    {
        Check(rcd);
    }
    catch (SAException &x)
    {
        if (!pCursor->PreHandleException(x))
            throw;
    }

    return rcd;
}

CS_RETCODE IsybConnection::Check(CS_RETCODE rcd, CS_CONNECTION* connection/* = NULL*/) const
{
    // get (allocated) user data where to return error info
    SASybErrInfo *pSybErrInfo = getSybErrInfo(m_handles.m_context,
        connection ? connection : m_handles.m_connection);
    SACriticalSectionScope cs(pSybErrInfo);

    // save
    CS_MSGNUM msgnumber = pSybErrInfo->msgnumber;
    pSybErrInfo->msgnumber = 0;

    if (msgnumber)
    {
        SAString sMsg;
#ifdef SA_UNICODE_WITH_UTF8
        sMsg.SetUTF8Chars(pSybErrInfo->msgstring);
#else
        sMsg = SAString(pSybErrInfo->msgstring);
#endif

        throw SAException(SA_DBMS_API_Error, msgnumber, pSybErrInfo->line, sMsg);
    }

    return rcd;
}

CS_RETCODE IsybConnection::CheckSilent(CS_RETCODE rcd, CS_CONNECTION* connection/* = NULL*/) const
{
    // get (allocated) user data where to return error info
    SASybErrInfo *pSybErrInfo = getSybErrInfo(m_handles.m_context,
        connection ? connection : m_handles.m_connection);
    SACriticalSectionScope cs(pSybErrInfo);

    // clear
    pSybErrInfo->msgnumber = 0;

    return rcd;
}

SAString IsybConnection::ConvertToString(
    CS_INT srctype, CS_VOID	*src, CS_INT srclen)
{
    assert(m_handles.m_context != NULL);

    SAString sConverted;
    CS_INT destlen = sa_max(1024, srclen * 2);
    CS_VOID *dest = new char[destlen];

    CS_DATAFMT srcfmt, destfmt;
    memset(&srcfmt, 0, sizeof(CS_DATAFMT));
    memset(&destfmt, 0, sizeof(CS_DATAFMT));

    srcfmt.datatype = srctype;
    srcfmt.format = CS_FMT_UNUSED;
    srcfmt.maxlength = srclen;
    srcfmt.locale = NULL;
    //srcfmt.name = NULL;
    srcfmt.namelen = 0;
    srcfmt.scale = 0;
    srcfmt.precision = 0;
    srcfmt.status = 0;
    srcfmt.count = 0;
    srcfmt.usertype = 0;

    destfmt.datatype = CS_CHAR_TYPE;
    destfmt.format = CS_FMT_UNUSED;
    destfmt.maxlength = destlen;
    destfmt.locale = NULL;
    //destfmt.name = "";
    destfmt.namelen = 0;
    destfmt.scale = 0;
    destfmt.precision = 0;
    destfmt.status = 0;
    destfmt.count = 0;
    destfmt.usertype = 0;

    CS_INT resultlen = 0;
    Check(g_sybAPI.cs_convert(
        m_handles.m_context,
        &srcfmt,
        src,
        &destfmt,
        dest,
        &resultlen));

    sConverted = SAString((const char*)dest, resultlen);
    delete[](char*)dest;

    return sConverted;
}

/*virtual */
void IsybConnection::CnvtInternalToDateTime(
    SADateTime &date_time,
    const void *pInternal,
    int	nInternalSize)
{
    if (nInternalSize != int(sizeof(CS_DATETIME)))
        return;
    CnvtInternalToDateTime(date_time, *(const CS_DATETIME*)pInternal);
}

void IsybConnection::CnvtInternalToDateTime(
    SADateTime &date_time,
    const CS_DATETIME &Internal)
{
    assert(m_handles.m_context != NULL);

    struct tm &_tm = (struct tm&)date_time;

    CS_DATEREC rec;
    Check(g_sybAPI.cs_dt_crack(
        m_handles.m_context,
        CS_DATETIME_TYPE,
        (CS_VOID*)&Internal,
        &rec));

    _tm.tm_hour = rec.datehour;
    _tm.tm_mday = rec.datedmonth;
    _tm.tm_min = rec.dateminute;
    _tm.tm_mon = rec.datemonth;
    _tm.tm_sec = rec.datesecond;
    _tm.tm_year = rec.dateyear - 1900;

    // complete structure
    _tm.tm_isdst = -1;
    // convert CS_DATEREC::datedweek 0 - 6 (Sun. - Sat.) 
    // to struct tm::tm_wday 0 - 6 (Sun. - Sat.)
    _tm.tm_wday = rec.datedweek;
    // convert CS_DATEREC::datedyear 1-366
    // to struct tm::tm_yday 0-365
    _tm.tm_yday = rec.datedyear - 1;

    // convert SyBase milliseconds to SQLAPI++ nanoseconds
    date_time.Fraction() = rec.datemsecond * 1000000;
}

/*virtual */
void IsybConnection::CnvtInternalToInterval(
    SAInterval & /*interval*/,
    const void * /*pInternal*/,
    int /*nInternalSize*/)
{
    assert(false);
}

/*static */
void IsybConnection::CnvtDateTimeToInternal(
    const SADateTime &date_time,
    SAString &sInternal)
{
    // format should be 
    //    YYYYMMDD hh:mm:ss:mmm
    // or YYYYMMDD hh:mm:ss.mmm	- we use this currently
    // or YYYYMMDD hh:mm:ss		- and this
    if (date_time.Fraction())
        sInternal.Format(_TSA("%.4d%.2d%.2d %.2d:%.2d:%.2d.%.3d"),
        date_time.GetYear(), date_time.GetMonth(), date_time.GetDay(),
        date_time.GetHour(), date_time.GetMinute(), date_time.GetSecond(),
        (int)((double)date_time.Fraction() / 1.0e6 + 0.5e-6));
    else
        sInternal.Format(_TSA("%.4d%.2d%.2d %.2d:%.2d:%.2d"),
        date_time.GetYear(), date_time.GetMonth(), date_time.GetDay(),
        date_time.GetHour(), date_time.GetMinute(), date_time.GetSecond());
}

void IsybConnection::CnvtDateTimeToInternal(
    const SADateTime &date_time,
    CS_DATETIME *pInternal)
{
    assert(m_handles.m_context != NULL);

    SAString s;
    CnvtDateTimeToInternal(date_time, s);

    CS_DATAFMT srcfmt, destfmt;
    memset(&srcfmt, 0, sizeof(CS_DATAFMT));
    memset(&destfmt, 0, sizeof(CS_DATAFMT));

    srcfmt.datatype = CS_CHAR_TYPE;
    srcfmt.format = CS_FMT_UNUSED;
    srcfmt.maxlength = (CS_INT)s.GetLength();
    srcfmt.locale = NULL;

    destfmt.datatype = CS_DATETIME_TYPE;
    destfmt.format = CS_FMT_UNUSED;
    destfmt.maxlength = (CS_INT)sizeof(CS_DATETIME);
    destfmt.locale = NULL;

    CS_INT resultlen = 0;
    Check(g_sybAPI.cs_convert(
        m_handles.m_context,
        &srcfmt,
        (CS_VOID*)s.GetMultiByteChars(),
        &destfmt,
        pInternal,
        &resultlen));
}

/*virtual */
void IsybConnection::CnvtInternalToCursor(
    SACommand * /*pCursor*/,
    const void * /*pInternal*/)
{
    assert(false);
}

/*virtual */
long IsybConnection::GetClientVersion() const
{
    assert(m_handles.m_context != NULL);

    char szVer[1024];
    Check(g_sybAPI.ct_config(m_handles.m_context, CS_GET, CS_VER_STRING,
        szVer, 1024, NULL));

    return SAExtractVersionFromString(szVer);
}

/*virtual */
long IsybConnection::GetServerVersion() const
{
    return SAExtractVersionFromString(GetServerVersionString());
}

/*virtual */
SAString IsybConnection::GetServerVersionString() const
{
    SAString sCommand, sVersion(_TSA("Unknown"));

    ServerType type = eUnknown;

    switch (getServerTypeConst(type))
    {
    case eASA:
        // If we are on Adaptive Server Anywhere
        sCommand = _TSA("select dbo.xp_msver('FileDescription') || ' ' || dbo.xp_msver('ProductVersion')");
        break;

    case eASE:
        // If we are on Adaptive Server Enterprise
        sCommand = _TSA("select @@version");
        //sCommand = _TSA("dbo.sp_server_info @attribute_id=2");
        break;

    default:
        return sVersion;
    }

    try
    {
        SACommand cmd(m_pSAConnection);
        cmd.setCommandText(sCommand, SA_CmdSQLStmt);
        cmd.Execute();

        if (cmd.FetchNext())
        {
            sVersion = cmd.Field(1).asString();

            /*
            switch (type)
            {
            case eASA:
            sVersion = cmd.Field(1).asString();
            break;

            case eASE:
            sVersion = cmd.Field(3).asString();
            break;
            }
            */
        }

        cmd.Close();
    }
    catch (SAException&)
    {

    }

    return sVersion;
}

/*virtual */
bool IsybConnection::IsConnected() const
{
    return m_handles.m_connection != NULL;
}

/*virtual */
bool IsybConnection::IsAlive() const
{
    CS_INT status;

    g_sybAPI.ct_con_props(
        m_handles.m_connection, CS_GET,
        CS_CON_STATUS, &status,
        CS_UNUSED, NULL);

    return CS_CONSTAT_DEAD != (status & CS_CONSTAT_DEAD);
}

/*virtual */
void IsybConnection::Connect(
    const SAString &sDBString,
    const SAString &sUserID,
    const SAString &sPassword,
    saConnectionHandler_t fHandler)
{
    assert(m_handles.m_context != NULL);
    assert(m_handles.m_connection == NULL);

    // It should be thread-safe except ct_callback. But it isn't.
    // So we added the critical section here
    SACriticalSectionScope cs(&g_sybAPI.errorInfo);

    CS_CONNECTION *connection = NULL;
    CS_LOCALE* pLoc = NULL;

    try
    {
        SAString sOption = m_pSAConnection->Option(_TSA("CS_LOGIN_TIMEOUT"));
        if (!sOption.IsEmpty())
        {
            SAChar *pStop = NULL;
            CS_INT timeval = (CS_INT)sa_strtoul(sOption, &pStop, 10);
            Check(g_sybAPI.ct_config(
                m_handles.m_context, CS_SET,
                CS_LOGIN_TIMEOUT, &timeval, CS_UNUSED, NULL));
        }

        sOption = m_pSAConnection->Option(_TSA("CS_TIMEOUT"));
        if (!sOption.IsEmpty())
        {
            SAChar *pStop = NULL;
            CS_INT timeval = (CS_INT)sa_strtoul(sOption, &pStop, 10);
            Check(g_sybAPI.ct_config(
                m_handles.m_context, CS_SET,
                CS_TIMEOUT, &timeval, CS_UNUSED, NULL));
        }

        Check(g_sybAPI.ct_con_alloc(m_handles.m_context, &connection));

        // set (allocated) user data where to return error info
        SASybErrInfo* pSybErrInfo = &errorInfo;
        g_sybAPI.ct_con_props(
            connection,
            CS_SET,
            CS_USERDATA,
            &pSybErrInfo,
            (CS_INT)sizeof(SASybErrInfo*),
            NULL);

        // set connection level callbacks
        g_sybAPI.ct_callback(
            NULL, connection, CS_SET, CS_CLIENTMSG_CB,
            (CS_VOID *)DefaultClientMsg_cb);
        g_sybAPI.ct_callback(
            NULL, connection, CS_SET, CS_SERVERMSG_CB,
            (CS_VOID *)DefaultServerMsg_cb);

        Check(g_sybAPI.ct_con_props(
            connection, CS_SET,
            CS_USERNAME, (CS_VOID*)sUserID.GetMultiByteChars(), CS_NULLTERM, NULL), connection);
        Check(g_sybAPI.ct_con_props(
            connection, CS_SET,
            CS_PASSWORD, (CS_VOID*)sPassword.GetMultiByteChars(), CS_NULLTERM, NULL), connection);

#ifdef SA_UNICODE_WITH_UTF8
        Check(g_sybAPI.cs_loc_alloc(m_handles.m_context, &pLoc));
#endif
        sOption = m_pSAConnection->Option(_TSA("CS_LOCALE"));
        if (sOption.IsEmpty())
            sOption = m_pSAConnection->Option(_TSA("CS_LC_ALL"));
        if (!sOption.IsEmpty())
        {
            if (NULL == pLoc)
                Check(g_sybAPI.cs_loc_alloc(m_handles.m_context, &pLoc));
            Check(g_sybAPI.cs_locale(m_handles.m_context, CS_SET, pLoc, CS_LC_ALL,
                (CS_CHAR*)sOption.GetMultiByteChars(), CS_NULLTERM, NULL));
        }
#ifdef SA_UNICODE_WITH_UTF8
        Check(g_sybAPI.cs_locale(m_handles.m_context, CS_SET, pLoc,
            CS_SYB_CHARSET, (CS_CHAR*)"utf8", CS_NULLTERM, NULL));
#else
        sOption = m_pSAConnection->Option(_TSA("CS_SYB_CHARSET"));
        if (!sOption.IsEmpty())
        {
            if (NULL == pLoc)
                Check(g_sybAPI.cs_loc_alloc(m_handles.m_context, &pLoc));
            Check(g_sybAPI.cs_locale(m_handles.m_context, CS_SET, pLoc,
                CS_SYB_CHARSET, (CS_CHAR*)sOption.GetMultiByteChars(), CS_NULLTERM, NULL));
        }
#endif

        // set connection locale if defined
        if (pLoc)
            Check(g_sybAPI.ct_con_props(
            connection, CS_SET,
            CS_LOC_PROP, pLoc, CS_UNUSED, NULL), connection);

        SAString sPacketSize = m_pSAConnection->Option(_TSA("CS_PACKETSIZE"));
        if (!sPacketSize.IsEmpty())
        {
            CS_INT nPacketSize = sa_toi((const SAChar*)sPacketSize);
            Check(g_sybAPI.ct_con_props(
                connection, CS_SET,
                CS_PACKETSIZE, &nPacketSize, CS_UNUSED, NULL), connection);
        }

        SAString sAppName = m_pSAConnection->Option(_TSA("CS_APPNAME"));
        if (sAppName.IsEmpty())
            sAppName = m_pSAConnection->Option(SACON_OPTION_APPNAME);
        if (!sAppName.IsEmpty())
            Check(g_sybAPI.ct_con_props(
            connection, CS_SET,
            CS_APPNAME, (CS_VOID*)sAppName.GetMultiByteChars(), CS_NULLTERM, NULL), connection);

        SAString sHostName = m_pSAConnection->Option(_TSA("CS_HOSTNAME"));
        if (sHostName.IsEmpty())
            sHostName = m_pSAConnection->Option(SACON_OPTION_WSID);
        if (!sHostName.IsEmpty())
            Check(g_sybAPI.ct_con_props(
            connection, CS_SET,
            CS_HOSTNAME, (CS_VOID*)sHostName.GetMultiByteChars(), CS_NULLTERM, NULL), connection);

        sOption = m_pSAConnection->Option(_TSA("CS_BULK_LOGIN"));
        if (sOption.CompareNoCase(_TSA("CS_TRUE")) == 0)
        {
            CS_BOOL bulk_flag = CS_TRUE;
            Check(g_sybAPI.ct_con_props(
                connection, CS_SET,
                CS_BULK_LOGIN, (CS_VOID*)&bulk_flag, CS_UNUSED, NULL), connection);
        }
        else if (sOption.CompareNoCase(_TSA("CS_FALSE")) == 0)
        {
            CS_BOOL bulk_flag = CS_FALSE;
            Check(g_sybAPI.ct_con_props(
                connection, CS_SET,
                CS_BULK_LOGIN, (CS_VOID*)&bulk_flag, CS_UNUSED, NULL), connection);
        }

        sOption = m_pSAConnection->Option(_TSA("CS_HAFAILOVER"));
        if (!sOption.IsEmpty() && (
            0 == sOption.CompareNoCase(_TSA("CS_TRUE")) ||
            0 == sOption.CompareNoCase(_TSA("TRUE")) ||
            0 == sOption.CompareNoCase(_TSA("1"))))
        {
            CS_BOOL failover_flag = CS_TRUE;
            Check(g_sybAPI.ct_con_props(
                connection, CS_SET,
                CS_HAFAILOVER, &failover_flag, CS_UNUSED, NULL), connection);
        }

        size_t iPos = sDBString.FindOneOf(_TSA(":\\@"));
        m_sServerName = sDBString.Left(iPos);
        m_sDatabase = sDBString.Mid(SIZE_MAX == iPos ? 0 : (iPos + 1));

        if (NULL != fHandler)
            fHandler(*m_pSAConnection, SA_PreConnectHandler);

        Check(g_sybAPI.ct_connect(
            connection,
            m_sServerName.IsEmpty() ? NULL : (CS_CHAR*)m_sServerName.GetMultiByteChars(),
            m_sServerName.IsEmpty() ? 0 : CS_NULLTERM), connection);

        m_handles.m_connection = connection;
        m_handles.m_locale = pLoc;

        CS_INT nTextSize = 2147483647;
        Check(g_sybAPI.ct_options(
            m_handles.m_connection,
            CS_SET, CS_OPT_TEXTSIZE, &nTextSize, CS_UNUSED, NULL));

        if (!m_sDatabase.IsEmpty())
        {
            SAString sCmd(_TSA("use "));
            sCmd += m_sDatabase;
            SACommand cmd(getSAConnection(), sCmd, SA_CmdSQLStmt);
            cmd.Execute();
            cmd.Close();
        }

        // start new transaction if needed
        Commit();

        if (NULL != fHandler)
            fHandler(*m_pSAConnection, SA_PostConnectHandler);
    }
    catch (SAException &)
    {
        // clean up

        if (m_handles.m_connection)	// ct_options or use database failed
        {
            CS_RETCODE rcd = CheckSilent(g_sybAPI.ct_close(m_handles.m_connection, CS_UNUSED));
            if (rcd != CS_SUCCEED)
                CheckSilent(g_sybAPI.ct_close(m_handles.m_connection, CS_FORCE_CLOSE));
            m_handles.m_connection = NULL;
        }

        if (connection)
            CheckSilent(g_sybAPI.ct_con_drop(connection));

        if (m_handles.m_locale)
            g_sybAPI.cs_loc_drop(m_handles.m_context, m_handles.m_locale);
        m_handles.m_locale = NULL;

        throw;
    }
}

/*virtual */
void IsybConnection::Disconnect()
{
    assert(m_handles.m_connection);

    CS_INT buffer;
    CS_INT outlen;
    Check(g_sybAPI.ct_con_props(
        m_handles.m_connection,
        CS_GET,
        CS_CON_STATUS,
        &buffer,
        CS_UNUSED,
        &outlen));

    g_sybAPI.ct_cancel(m_handles.m_connection, NULL, CS_CANCEL_ALL);

    Check(g_sybAPI.ct_close(
        m_handles.m_connection,
        buffer & CS_CONSTAT_DEAD ? CS_FORCE_CLOSE : CS_UNUSED));

    Check(g_sybAPI.ct_con_drop(m_handles.m_connection), NULL);
    m_handles.m_connection = NULL;

    if (m_handles.m_locale)
        g_sybAPI.cs_loc_drop(m_handles.m_context, m_handles.m_locale);
    m_handles.m_locale = NULL;

    m_sServerName.Empty();
    m_sDatabase.Empty();
    m_type = eUnknown;
}

/*virtual */
void IsybConnection::Destroy()
{
    assert(m_handles.m_connection);

    CS_INT buffer = CS_CONSTAT_DEAD;
    CS_INT outlen;
    g_sybAPI.ct_con_props(
        m_handles.m_connection,
        CS_GET,
        CS_CON_STATUS,
        &buffer,
        CS_UNUSED,
        &outlen);

    g_sybAPI.ct_cancel(m_handles.m_connection, NULL, CS_CANCEL_ALL);

    g_sybAPI.ct_close(
        m_handles.m_connection,
        buffer & CS_CONSTAT_DEAD ? CS_FORCE_CLOSE : CS_UNUSED);
    g_sybAPI.ct_con_drop(m_handles.m_connection);
    m_handles.m_connection = NULL;

    if (m_handles.m_locale)
        g_sybAPI.cs_loc_drop(m_handles.m_context, m_handles.m_locale);
    m_handles.m_locale = NULL;

    m_sServerName.Empty();
    m_sDatabase.Empty();
    m_type = eUnknown;
}

/*virtual */
void IsybConnection::Reset()
{
    m_handles.m_connection = NULL;
    m_handles.m_locale = NULL;

    m_sServerName.Empty();
    m_sDatabase.Empty();

    m_type = eUnknown;
}

/*virtual */
void IsybConnection::Commit()
{
    // commit
    SACommand cmd(
        m_pSAConnection,
        "commit tran", SA_CmdSQLStmt);
    cmd.Execute();

    // and start new transaction if not in autocommit mode
    if (m_pSAConnection->AutoCommit() == SA_AutoCommitOff)
    {
        cmd.setCommandText("begin tran", SA_CmdSQLStmt);
        cmd.Execute();
    }

    cmd.Close();
}

/*virtual */
void IsybConnection::Rollback()
{
    // rollback
    SACommand cmd(
        m_pSAConnection,
        "rollback tran", SA_CmdSQLStmt);
    cmd.Execute();

    // and start new transaction if not in autocommit mode
    if (m_pSAConnection->AutoCommit() == SA_AutoCommitOff)
    {
        cmd.setCommandText("begin tran", SA_CmdSQLStmt);
        cmd.Execute();
    }

    cmd.Close();
}

//////////////////////////////////////////////////////////////////////
// IsybClient Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IsybClient::IsybClient()
{

}

IsybClient::~IsybClient()
{

}

ISAConnection *IsybClient::QueryConnectionInterface(
    SAConnection *pSAConnection)
{
    return new IsybConnection(pSAConnection);
}

//////////////////////////////////////////////////////////////////////
// IsybCursor Class
//////////////////////////////////////////////////////////////////////

class IsybCursor : public ISACursor
{
    sybCommandHandles	m_handles;

    bool m_bResultsPending;
    void CheckAndCancelPendingResults();
    void CancelPendingResults();
    void ProcessBatchUntilEndOrResultSet();
    CS_INT	m_nRowsAffected;

    int m_cRowsToPrefetch;	// defined in SetSelectBuffers
    CS_INT m_cRowsObtained;
    CS_INT m_cRowCurrent;

    bool m_bCursorRequested;
    bool m_bCursorOpen;
    void CheckAndCloseCursor();
    void ct_setCommandText(
        const SAString &sCmd,
        CS_INT type,
        CS_INT option);

    void BindImage(SAParam &Param, SAString *pValue);
    void BindText(SAParam &Param, SAString *pValue);

    void FetchStatusResult();
    void FetchParamResult();

    bool m_bScrollableResultSet;
    bool FetchRow(CS_INT type, CS_INT offset);

protected:
    // should return false if client binds output/return parameters
    // according to value type (coersing)
    // should return true if client binds output/return parameters
    // according to parameter type
    // defaults to false, so we overload it in OpenClient
    virtual bool OutputBindingTypeIsParamType();

    virtual size_t InputBufferSize(
        const SAParam &Param) const;
    virtual size_t OutputBufferSize(
        SADataType_t eDataType,
        size_t nDataSize) const;

    // prec and scale can be coerced
    SADataType_t CnvtNativeToStd(
        CS_INT dbtype,
        CS_INT &prec,
        CS_INT &scale) const;
    virtual int CnvtStdToNative(SADataType_t eDataType) const;

    SADataType_t CnvtNativeTypeFromASESysColumnsToStd(
        int dbtype, int/* dbsubtype*/, int/* dbsize*/, int prec, int scale) const;
    SADataType_t CnvtNativeTypeFromASADomainIDToStd(
        int dbtype, int/* dbsubtype*/, int/* dbsize*/, int prec, int scale) const;

protected:
    void ct_bind_Buffer(	// used for fields and output(return) params
        int nPos,	// 1-based
        void *pInd,
        size_t nIndSize,
        void *pSize,
        size_t nSizeSize,
        void *pValue,
        size_t nValueSize,
        SADataType_t eDataType,
        SAString sName,
        CS_INT count,
        CS_INT scale,
        CS_INT precision);

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

public:
    IsybCursor(
        IsybConnection *pIsybConnection,
        SACommand *pCommand);
    virtual ~IsybCursor();

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

IsybCursor::IsybCursor(
    IsybConnection *pIsybConnection,
    SACommand *pCommand) :
    ISACursor(pIsybConnection, pCommand)
{
    Reset();
}

/*virtual */
IsybCursor::~IsybCursor()
{
}

/*virtual */
size_t IsybCursor::InputBufferSize(
    const SAParam &Param) const
{
    switch (Param.DataType())
    {
    case SA_dtBool:
        return sizeof(CS_BIT);
    case SA_dtNumeric:
        return sizeof(CS_NUMERIC);
    case SA_dtDateTime:
        return sizeof(CS_DATETIME);
    case SA_dtString:
        // for empty Sybase strings we must send single '\s'
        if (0 == ISACursor::InputBufferSize(Param))
            return (unsigned int)(1);
        break;
    case SA_dtLongBinary:
    case SA_dtLongChar:
    case SA_dtBLob:
    case SA_dtCLob:
        return 0;
    default:
        break;
    }

    return ISACursor::InputBufferSize(Param);
}

/*virtual */
size_t IsybCursor::OutputBufferSize(
    SADataType_t eDataType,
    size_t nDataSize) const
{
    switch (eDataType)
    {
        // fix INT type size for 64-bit platform
    case SA_dtLong:
    case SA_dtULong:
        return sizeof(CS_INT);
    case SA_dtBool:
        return sizeof(CS_BIT);
    case SA_dtNumeric:
        return sizeof(CS_NUMERIC);
    case SA_dtDateTime:
        return sizeof(CS_DATETIME);
    case SA_dtLongBinary:
    case SA_dtLongChar:
        return 0;
    case SA_dtString:
        return (nDataSize + 1);
    default:
        break;
    }

    return ISACursor::OutputBufferSize(eDataType, nDataSize);
}

SADataType_t IsybCursor::CnvtNativeTypeFromASADomainIDToStd(
    int dbtype, int/* dbsubtype*/, int/* dbsize*/, int prec, int scale) const
{
    SADataType_t eDataType = SA_dtUnknown;

    switch (dbtype)
    {
    case 11:	//binary
    case 28:	//varbinary
        eDataType = SA_dtBytes;
        break;
    case 7:		//char
    case 8:		//char
    case 9:		//varchar
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
        if (scale <= 0)
        {	// check for exact type
            if (prec < 5)
                eDataType = SA_dtShort;
            else if (prec < 10)
                eDataType = SA_dtLong;
            else
                eDataType = SA_dtNumeric;
        }
        else
            eDataType = SA_dtNumeric;
        break;
    case 6:		//date
    case 13:	// timestamp
    case 14:	//time
        eDataType = SA_dtDateTime;
        break;
    case 12:	// long binary
        eDataType = SA_dtLongBinary;
        break;
    case 10:	//long varchar
        eDataType = SA_dtLongChar;
        break;
    default:
        assert(false);
    }

    return eDataType;
}

SADataType_t IsybCursor::CnvtNativeTypeFromASESysColumnsToStd(
    int dbtype, int/* dbsubtype*/, int/* dbsize*/, int prec, int scale) const
{
    SADataType_t eDataType = SA_dtUnknown;

    switch (dbtype)
    {
    case 0x25:	//SQLVARBINARY
        eDataType = SA_dtBytes;
        break;
    case 0x27:	//SQLVARCHAR
        eDataType = SA_dtString;
        break;
    case 0x2f:	//SQLCHAR:	// char, varchar
        eDataType = SA_dtString;

        break;
    case 0x2d:	//SQLBINARY:	// binary, varbinary
        eDataType = SA_dtBytes;
        break;
    case 0x30:	//SQLINT1:	// tinyint
        eDataType = SA_dtShort;
        break;
    case 0x34:	//SQLINT2:	// smallint
        eDataType = SA_dtShort;
        break;
    case 0x38:	//SQLINT4:	// int
        eDataType = SA_dtLong;
        break;
    case 0x3b:	//SQLFLT4:	// real
        eDataType = SA_dtDouble;
        break;
    case 0x3e:	//SQLFLT8:	// float
        eDataType = SA_dtDouble;
        break;
    case 0x7a:	//SQLMONEY4:	// smallmoney
        eDataType = SA_dtDouble;
        break;
    case 0x3c:	//SQLMONEY:	// money
        eDataType = SA_dtDouble;
        break;
    case 0x37:	// decimal
    case 0x3f:	// numeric
        if (scale <= 0)
        {	// check for exact type
            if (prec < 5)
                eDataType = SA_dtShort;
            else if (prec < 10)
                eDataType = SA_dtLong;
            else
                eDataType = SA_dtNumeric;
        }
        else
            eDataType = SA_dtNumeric;
        break;
    case 0x3a:	//SQLDATETIM4:	// smalldatetime
        eDataType = SA_dtDateTime;
        break;
    case 0x3d:	//SQLDATETIME:	// datetime
        eDataType = SA_dtDateTime;
        break;
    case 0x31:	// date
        eDataType = SA_dtDateTime;
        break;
    case 0xaa: // locator
    case 0x22:	//SQLIMAGE:	// image
        eDataType = SA_dtLongBinary;
        break;
    case 0xab: // UNITEXT localtor
    case 0xa9: // TEXT locator
    case 0x23:	//SQLTEXT:	// text
        eDataType = SA_dtLongChar;
        break;
    case 0x32:	//SQLBIT
        eDataType = SA_dtBool;
        break;
    default:
        assert(false);
    }

    return eDataType;
}

/*virtual */
SADataType_t IsybCursor::CnvtNativeToStd(
    CS_INT dbtype, CS_INT &prec, CS_INT &scale) const
{
    SADataType_t eDataType = SA_dtUnknown;

    switch (dbtype)
    {
    case CS_CHAR_TYPE:
    case CS_UNICHAR_TYPE:
        eDataType = SA_dtString;
        break;
    case CS_BINARY_TYPE:
        eDataType = SA_dtBytes;
        break;
    case CS_LONGCHAR_TYPE:
        eDataType = SA_dtString;
        break;
    case CS_LONGBINARY_TYPE:
        eDataType = SA_dtBytes;
        break;
    case CS_TEXT_TYPE:
    case CS_UNITEXT_TYPE:
        eDataType = SA_dtLongChar;
        break;
    case CS_IMAGE_TYPE:
        eDataType = SA_dtLongBinary;
        break;
    case CS_TINYINT_TYPE:
        eDataType = SA_dtShort;
        break;
    case CS_SMALLINT_TYPE:
        eDataType = SA_dtShort;
        break;
    case CS_INT_TYPE:
        eDataType = SA_dtLong;
        break;
    case CS_REAL_TYPE:
        eDataType = SA_dtDouble;
        break;
    case CS_FLOAT_TYPE:
        eDataType = SA_dtDouble;
        break;
    case CS_BIT_TYPE:
        eDataType = SA_dtBool;
        break;
    case CS_DATETIME_TYPE:
        eDataType = SA_dtDateTime;
        break;
    case CS_DATETIME4_TYPE:
        eDataType = SA_dtDateTime;
        break;
    case CS_MONEY_TYPE:
        prec = 19;
        scale = 4;
        eDataType = SA_dtNumeric;
        break;
    case CS_MONEY4_TYPE:
        eDataType = SA_dtDouble;
        break;
    case CS_NUMERIC_TYPE:
    case CS_DECIMAL_TYPE:
        if (scale > 0)
            eDataType = SA_dtNumeric;
        else	// check for exact type
        {
            if (prec < 5)
                eDataType = SA_dtShort;
            else if (prec < 10)
                eDataType = SA_dtLong;
            else
                eDataType = SA_dtNumeric;
        }
        break;
    case CS_VARCHAR_TYPE:
        eDataType = SA_dtString;
        break;
    case CS_VARBINARY_TYPE:
        eDataType = SA_dtBytes;
        break;
    case CS_TIME_TYPE:
        eDataType = SA_dtDateTime;
        break;
    case CS_DATE_TYPE:
        eDataType = SA_dtDateTime;
        break;
    case CS_LONG_TYPE:
        assert(false);
        break;
    case CS_SENSITIVITY_TYPE:
        assert(false);
        break;
    case CS_BOUNDARY_TYPE:
        assert(false);
        break;
    case CS_VOID_TYPE:
        assert(false);
        break;
    case CS_USHORT_TYPE:
        assert(false);
        break;
#ifdef CS_VERSION_150
    case CS_USMALLINT_TYPE:
        eDataType = SA_dtUShort;
        break;
    case CS_UINT_TYPE:
        eDataType = SA_dtULong;
        break;
    case CS_UBIGINT_TYPE:
        prec = 20;
    case CS_BIGINT_TYPE:
        prec = 19;
        scale = 0;
        eDataType = SA_dtNumeric;
        break;
#endif
#ifdef CS_VERSION_155
    case CS_BIGDATETIME_TYPE:
        eDataType = SA_dtDateTime;
        break;
    case CS_BIGTIME_TYPE:
        eDataType = SA_dtDateTime;
        break;
#endif
    default:
        assert(false);
        break;
    }

    return eDataType;
}

/*virtual */
int IsybCursor::CnvtStdToNative(SADataType_t eDataType) const
{
    CS_INT type;

    switch (eDataType)
    {
    case SA_dtUnknown:
        throw SAException(SA_Library_Error, SA_Library_Error_UnknownDataType, -1, IDS_UNKNOWN_DATA_TYPE);
    case SA_dtBool:
        type = CS_BIT_TYPE;	// single bit type
        break;
    case SA_dtUShort:
#ifdef CS_VERSION_150
        type = CS_USMALLINT_TYPE;	// 2-byte unsigned integer type
        break;
#endif
    case SA_dtShort:
        type = CS_SMALLINT_TYPE;	// 2-byte unsigned integer type
        break;
    case SA_dtULong:
#ifdef CS_VERSION_150
        type = CS_UINT_TYPE;	// 4-byte unsigned integer type
        break;
#endif
    case SA_dtLong:
        type = CS_INT_TYPE;	// 4-byte integer type
        break;
    case SA_dtDouble:
        type = CS_FLOAT_TYPE;		// 8-byte float type
        break;
    case SA_dtNumeric:
        type = CS_NUMERIC_TYPE;		// Numeric type
        break;
    case SA_dtDateTime:
        type = CS_DATETIME_TYPE;	// 8-byte datetime type
        break;
    case SA_dtString:
        type = CS_CHAR_TYPE;		// character type
        break;
    case SA_dtBytes:
        type = CS_BINARY_TYPE;		// binary type
        break;
    case SA_dtLongBinary:
    case SA_dtBLob:
        type = CS_IMAGE_TYPE;		// image type
        break;
    case SA_dtLongChar:
    case SA_dtCLob:
        type = CS_TEXT_TYPE;		// text type
        break;
    default:
        type = 0;
        assert(false);
    }

    return type;
}

/*virtual */
bool IsybCursor::IsOpened()
{
    return m_handles.m_command != NULL;
}

/*virtual */
void IsybCursor::Open()
{
    assert(m_handles.m_command == NULL);

    ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_cmd_alloc(
        ((IsybConnection*)m_pISAConnection)->m_handles.m_connection,
        &m_handles.m_command));
}

void IsybCursor::CancelPendingResults()
{
    // cancel any results pending
    ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_cancel(
        NULL, m_handles.m_command, CS_CANCEL_ALL));

    m_bResultsPending = false;
}

void IsybCursor::CheckAndCancelPendingResults()
{
    // cancel results pending if any
    if (m_bResultsPending)
        CancelPendingResults();
}

void IsybCursor::CheckAndCloseCursor()
{
    if (m_bCursorOpen)
    {
        ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_cursor(
            m_handles.m_command,
            CS_CURSOR_CLOSE,
            NULL, CS_UNUSED,
            NULL, CS_UNUSED,
            CS_DEALLOC));
        ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_send(
            m_handles.m_command));
        ProcessBatchUntilEndOrResultSet();

        m_bCursorOpen = false;
    }
}

/*virtual */
void IsybCursor::Close()
{
    assert(m_handles.m_command != NULL);

    CancelPendingResults();

    ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_cmd_drop(m_handles.m_command));
    m_handles.m_command = NULL;
}

/*virtual */
void IsybCursor::Destroy()
{
    assert(m_handles.m_command != NULL);

    try
    {
        CancelPendingResults();
    }
    catch (SAException&)
    {
    }

    g_sybAPI.ct_cmd_drop(m_handles.m_command);
    m_handles.m_command = NULL;
}


/*virtual */
void IsybCursor::Reset()
{
    m_handles.m_command = NULL;

    m_bResultsPending = false;
    m_nRowsAffected = -1;
    m_bCursorRequested = false;
    m_bCursorOpen = false;
}

/*virtual */
ISACursor *IsybConnection::NewCursor(SACommand *m_pCommand)
{
    return new IsybCursor(this, m_pCommand);
}

/*virtual */
void IsybCursor::Prepare(
    const SAString &/*sStmt*/,
    SACommandType_t/* eCmdType*/,
    int/* nPlaceHolderCount*/,
    saPlaceHolder ** /*ppPlaceHolders*/)
{
    // no preparing in OpenClient
    // all is beeing done in Execute()
}

// process command batch or stored proc results
// stop if all commands processed or result set encounted
void IsybCursor::ProcessBatchUntilEndOrResultSet()
{
    bool bParamResult = false;
    bool bStatusResult = false;

    CS_RETCODE rcd;
    CS_INT res_type;
    while ((rcd = ((IsybConnection*)m_pISAConnection)->CheckBatch(g_sybAPI.ct_results(
        m_handles.m_command, &res_type), this)) == CS_SUCCEED)
    {
        bool bResultSet = false;
        switch (res_type)
        {
        case CS_CMD_SUCCEED:	// Command returning no rows completed sucessfully
            break;
        case CS_CMD_DONE:		// This means we're done with one result set
            ((IsybConnection *)m_pISAConnection)->Check(g_sybAPI.ct_res_info(
                m_handles.m_command,
                CS_ROW_COUNT,
                &m_nRowsAffected, CS_UNUSED, NULL));
            m_pCommand->setOption(SA_SYBOPT_RESULT_COUNT).Format(_TSA("%d"),
                sa_toi((const SAChar*)m_pCommand->Option(SA_SYBOPT_RESULT_COUNT)) + 1);
            break;
        case CS_CMD_FAIL:		// This means that the server encountered an error while processing our command
            ((IsybConnection *)m_pISAConnection)->Check(CS_FAIL);
            break;
        case CS_ROW_RESULT:
            bResultSet = true;
            m_pCommand->setOption(SA_SYBOPT_RESULT_TYPE) = _TSA("CS_ROW_RESULT");
            break;
        case CS_STATUS_RESULT:
            FetchStatusResult();
            bStatusResult = true;
            m_pCommand->setOption(SA_SYBOPT_RESULT_TYPE) = _TSA("CS_STATUS_RESULT");
            break;
        case CS_PARAM_RESULT:
            FetchParamResult();
            bParamResult = true;
            m_pCommand->setOption(SA_SYBOPT_RESULT_TYPE) = _TSA("CS_PARAM_RESULT");
            break;
        case CS_COMPUTE_RESULT:
            bResultSet = true;
            m_pCommand->setOption(SA_SYBOPT_RESULT_TYPE) = _TSA("CS_COMPUTE_RESULT");
            break;
        case CS_CURSOR_RESULT:
            bResultSet = true;
            m_pCommand->setOption(SA_SYBOPT_RESULT_TYPE) = _TSA("CS_CURSOR_RESULT");
            break;
        default:
            assert(false);
        }

        if (bResultSet)
        {
            m_bResultsPending = true;
            return;
        }
    }

    // We've finished processing results. Let's check the
    // return value of ct_results() to see if everything
    // went ok.
    switch (rcd)
    {
    case CS_END_RESULTS:	// Everything went fine
        m_bResultsPending = false;
        if (bStatusResult || bParamResult)
            ConvertOutputParams();
        break;
    case CS_CANCELED: // Command was canceled
        m_bResultsPending = false;
        break;
    case CS_FAIL:			// Something went wrong
        ((IsybConnection*)m_pISAConnection)->Check(rcd);
        break;
    default:				// We got an unexpected return value
        assert(false);
    }
}

/*virtual */
void IsybCursor::UnExecute()
{
    m_pCommand->setOption(SA_SYBOPT_RESULT_COUNT) = _TSA("0");
    m_pCommand->setOption(SA_SYBOPT_RESULT_TYPE) = _TSA("");

    CheckAndCancelPendingResults();
    CheckAndCloseCursor();
}

/*virtual */
void IsybCursor::Execute(
    int nPlaceHolderCount,
    saPlaceHolder **ppPlaceHolders)
{
    // always bind, even if there are no parameters
    // because Bind() does ct_command/ct_cursor
    Bind(nPlaceHolderCount, ppPlaceHolders);

    ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_send(
        m_handles.m_command));
    if (m_bCursorRequested)
        m_bCursorOpen = true;

    try
    {
        ProcessBatchUntilEndOrResultSet();
    }
    catch (SAException &)	// clean up after ct_send
    {
        // cancel any results pending
        assert(!m_bResultsPending);
        g_sybAPI.ct_cancel(NULL, m_handles.m_command, CS_CANCEL_ALL);
        throw;
    }
}

/*virtual */
void IsybCursor::Cancel()
{
    ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_cancel(
        NULL, m_handles.m_command, CS_CANCEL_ATTN));
}

void IsybCursor::ct_setCommandText(
    const SAString &sCmd,
    CS_INT type,
    CS_INT option)
{
    SAString sCursor = m_pCommand->Option(_TSA("ct_cursor"));
    m_bScrollableResultSet = isSetScrollable();
    m_bCursorRequested = !sCursor.IsEmpty() || m_bScrollableResultSet;
    if (m_bCursorRequested)
    {
        if (sCursor.IsEmpty())
            sCursor.Format(_TSA("c%08X"), this);

        ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_cursor(
            m_handles.m_command,
            CS_CURSOR_DECLARE, (CS_CHAR*)sCursor.GetMultiByteChars(), CS_NULLTERM,
#ifdef SA_UNICODE_WITH_UTF8
            (CS_CHAR*)sCmd.GetUTF8Chars(),
#else
            (CS_CHAR*)sCmd.GetMultiByteChars(),
#endif
            CS_NULLTERM,
            m_bScrollableResultSet ? CS_SCROLL_CURSOR : CS_END));
        ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_cursor(
            m_handles.m_command,
            CS_CURSOR_OPEN, NULL, CS_UNUSED,
            NULL, CS_UNUSED, CS_UNUSED));
    }
    else
    {
        SA_TRACE_CMD(sCmd);
        ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_command(
            m_handles.m_command, type,
#ifdef SA_UNICODE_WITH_UTF8
            (CS_CHAR*)sCmd.GetUTF8Chars(),
#else
            (CS_CHAR*)sCmd.GetMultiByteChars(),
#endif
            CS_NULLTERM, option));
    }
}

void IsybCursor::BindImage(SAParam &Param, SAString *pValue)
{
    SAString sImage = _TSA("0x");

    size_t nActualWrite;
    SAPieceType_t ePieceType = SA_FirstPiece;
    void *pBuf;
    while ((nActualWrite = Param.InvokeWriter(
        ePieceType, SA_DefaultMaxLong, pBuf)) != 0)
    {
        sImage += ((IsybConnection*)m_pISAConnection)->ConvertToString(
            CS_IMAGE_TYPE, (CS_VOID*)pBuf, (CS_INT)nActualWrite);

        if (ePieceType == SA_LastPiece)
            break;
    }

    if (sImage.GetLength() > 2)
        *pValue += sImage;
    else
        *pValue += _TSA("NULL");
}

void IsybCursor::BindText(SAParam &Param, SAString *pValue)
{
    *pValue += _TSA("'");

    size_t nActualWrite;
    SAPieceType_t ePieceType = SA_FirstPiece;
    void *pBuf;
    while ((nActualWrite = Param.InvokeWriter(
        ePieceType, SA_DefaultMaxLong, pBuf)) != 0)
    {
        SAString sTemp(/*(const char *)*/pBuf, nActualWrite);
        sTemp.Replace(_TSA("'"), _TSA("''"));
        *pValue += sTemp;

        if (ePieceType == SA_LastPiece)
            break;
    }

    *pValue += _TSA("'");
}

void IsybCursor::Bind(
    int nPlaceHolderCount,
    saPlaceHolder**	ppPlaceHolders)
{
    // always alloc bind buffer, even if we will not use it
    // 'exec SP' to work
    AllocBindBuffer(sizeof(CS_SMALLINT), sizeof(CS_INT));

    if (m_pCommand->CommandType() == SA_CmdSQLStmtRaw)
    {
        CancelPendingResults();
        ct_setCommandText(m_pCommand->CommandText(), CS_LANG_CMD, CS_UNUSED);
    }
    else if (m_pCommand->CommandType() == SA_CmdSQLStmt)
    {
        SAString sOriginalStmst = m_pCommand->CommandText();

        // we will bind directly to command buffer
        CancelPendingResults();
        SAString sBoundStmt;

        // change ':' param markers to bound values
        size_t nPos = 0;
        for (int i = 0; i < nPlaceHolderCount; ++i)
        {
            SAParam &Param = *ppPlaceHolders[i]->getParam();

            sBoundStmt += sOriginalStmst.Mid(nPos, ppPlaceHolders[i]->getStart() - nPos);

            SAString sBoundValue;
            SAString sTemp;
            CS_BIT bitTemp;

            if (Param.isNull())
            {
                sBoundStmt += _TSA("NULL");
            }
            else
            {
                switch (Param.DataType())
                {
                case SA_dtUnknown:
                    throw SAException(SA_Library_Error, SA_Library_Error_UnknownParameterType, -1, IDS_UNKNOWN_PARAMETER_TYPE, (const SAChar*)Param.Name());
                case SA_dtBool:
                    bitTemp = (CS_BIT)Param.setAsBool();
                    sBoundValue = ((IsybConnection*)m_pISAConnection)->ConvertToString(
                        CS_BIT_TYPE, (CS_VOID*)&bitTemp, (CS_INT)sizeof(CS_BIT));
                    break;
                case SA_dtShort:
                    sBoundValue = ((IsybConnection*)m_pISAConnection)->ConvertToString(
                        CS_SMALLINT_TYPE, (CS_VOID*)&Param.setAsShort(), (CS_INT)sizeof(short));
                    break;
                case SA_dtUShort:
                    sBoundValue = ((IsybConnection*)m_pISAConnection)->ConvertToString(
                        CS_SMALLINT_TYPE, (CS_VOID*)&Param.setAsUShort(), (CS_INT)sizeof(unsigned short));
                    break;
                case SA_dtLong:
                    sBoundValue = ((IsybConnection*)m_pISAConnection)->ConvertToString(
                        CS_INT_TYPE, (CS_VOID*)&Param.setAsLong(), (CS_INT)sizeof(long));
                    break;
                case SA_dtULong:
                    sBoundValue = ((IsybConnection*)m_pISAConnection)->ConvertToString(
                        CS_INT_TYPE, (CS_VOID*)&Param.setAsULong(), (CS_INT)sizeof(unsigned long));
                    break;
                case SA_dtDouble:
                    sBoundValue = ((IsybConnection*)m_pISAConnection)->ConvertToString(
                        CS_FLOAT_TYPE, (CS_VOID*)&Param.setAsDouble(), (CS_INT)sizeof(double));
                    break;
                case SA_dtNumeric:
                    // use SQLAPI++ locale-unaffected converter (through SANumeric type)
                    sBoundValue = Param.asNumeric().operator SAString();
                    break;
                case SA_dtDateTime:
                    IsybConnection::CnvtDateTimeToInternal(
                        Param.setAsDateTime(), sTemp);
                    sBoundValue = _TSA("'");
                    sBoundValue += sTemp;
                    sBoundValue += _TSA("'");
                    break;
                case SA_dtString:
                    sTemp = Param.asString();
                    sTemp.Replace(_TSA("'"), _TSA("''"));
                    sBoundValue = _TSA("'");
                    sBoundValue += sTemp;
                    sBoundValue += _TSA("'");
                    break;
                case SA_dtBytes:
                {
                    sBoundValue = _TSA("0x");
                    SAString bdata = Param.asBytes();
                    CS_INT blen = (CS_INT)bdata.GetBinaryLength();
                    if (blen > 0)
                    {
                        sBoundValue += ((IsybConnection*)m_pISAConnection)->ConvertToString(
                            CS_BINARY_TYPE, (CS_VOID*)bdata.GetBinaryBuffer(blen), blen);
                        bdata.ReleaseBinaryBuffer(blen);
                    }
                    else
                        sBoundValue = _TSA("NULL");
                }
                    break;
                case SA_dtLongBinary:
                case SA_dtBLob:
                    BindImage(Param, &sBoundStmt);
                    break;
                case SA_dtLongChar:
                case SA_dtCLob:
                    BindText(Param, &sBoundStmt);
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
        if (nPos < sa_strlen(sOriginalStmst))
            sBoundStmt += sOriginalStmst.Mid(nPos);

        ct_setCommandText(sBoundStmt, CS_LANG_CMD, CS_UNUSED);
    }
    else
    {
        assert(m_pCommand->CommandType() == SA_CmdStoredProc);

        ct_setCommandText(m_pCommand->CommandText(), CS_RPC_CMD, CS_NO_RECOMPILE);

        void *pBuf = m_pParamBuffer;

        for (int i = 0; i < m_pCommand->ParamCount(); ++i)
        {
            SAParam &Param = m_pCommand->ParamByIndex(i);

            void *pInd;
            void *pSize;
            size_t nDataBufSize;
            void *pValue;
            IncParamBuffer(pBuf, pInd, pSize, nDataBufSize, pValue);

            if (Param.ParamDirType() == SA_ParamReturn)
                continue;

            SAString sName = "@";
            sName += Param.Name();

            SADataType_t eDataType = Param.DataType();
            CS_INT type = (CS_INT)CnvtStdToNative(eDataType);
            CS_SMALLINT &indicator = *(CS_SMALLINT*)pInd;
            CS_INT &datalen = *(CS_INT*)pSize;
            CS_VOID *data;

            if (Param.isNull())
            {
                indicator = -1;
                datalen = CS_UNUSED;
                pValue = NULL;
            }

            if (isInputParam(Param))
            {
                size_t nInputBufSize = InputBufferSize(Param);
                assert(nInputBufSize <= nDataBufSize);

                if (!Param.isNull())
                {
                    indicator = 0;
                    datalen = (CS_INT)nInputBufSize;
                    data = (CS_VOID*)pValue;

                    switch (eDataType)
                    {
                    case SA_dtUnknown:
                        throw SAException(SA_Library_Error, SA_Library_Error_UnknownParameterType, -1, IDS_UNKNOWN_PARAMETER_TYPE, (const SAChar*)Param.Name());
                    case SA_dtBool:
                        assert(nInputBufSize == sizeof(CS_BIT));
                        *(CS_BIT*)data = (CS_BIT)Param.asBool();
                        break;
                    case SA_dtShort:
                        assert(nInputBufSize == sizeof(short));
                        *(short*)data = Param.asShort();
                        break;
                    case SA_dtUShort:
                        assert(nInputBufSize == sizeof(unsigned short));
                        *(unsigned short*)data = Param.asUShort();
                        break;
                    case SA_dtLong:
                        assert(nInputBufSize == sizeof(long));
                        *(long*)data = Param.asLong();
                        break;
                    case SA_dtULong:
                        assert(nInputBufSize == sizeof(unsigned long));
                        *(unsigned long*)data = Param.asULong();
                        break;
                    case SA_dtDouble:
                        assert(nInputBufSize == sizeof(double));
                        *(double*)data = Param.asDouble();
                        break;
                    case SA_dtNumeric:
                        assert(nInputBufSize == sizeof(CS_NUMERIC));
                        ((IsybConnection*)m_pISAConnection)->CnvtNumericToInternal(
                            Param.asNumeric(),
                            *(CS_NUMERIC*)pValue,
                            datalen);
                        break;
                    case SA_dtDateTime:
                        assert(nInputBufSize == sizeof(CS_DATETIME));	// Sybase internal date/time representation
                        ((IsybConnection*)m_pISAConnection)->CnvtDateTimeToInternal(
                            Param.asDateTime(),
                            (CS_DATETIME*)data);
                        break;
                    case SA_dtString:
#ifdef SA_UNICODE_WITH_UTF8
                        assert(nInputBufSize >= Param.asString().GetUTF8CharsLength());
#else
                        assert(nInputBufSize >= Param.asString().GetMultiByteCharsLength());
#endif
                        if (Param.asString().IsEmpty()) // send '\s'
                        {
                            memcpy(data, " ", 1);
                            datalen = 1;
                        }
                        else
#ifdef SA_UNICODE_WITH_UTF8
                            memcpy(data, Param.asString().GetUTF8Chars(), nInputBufSize);
#else
                            memcpy(data, Param.asString().GetMultiByteChars(), nInputBufSize);
#endif
                        break;
                    case SA_dtBytes:
                        assert(nInputBufSize == Param.asBytes().GetBinaryLength());
                        memcpy(data, (const void*)Param.asBytes(), nInputBufSize);
                        break;
                    case SA_dtLongBinary:
                    case SA_dtBLob:
                    case SA_dtLongChar:
                    case SA_dtCLob:
                        if (Param.m_fnWriter != NULL)	// use internal buffer as temp
                        {
                            Param.m_pString->Empty();

                            size_t nActualWrite;
                            SAPieceType_t ePieceType = SA_FirstPiece;
                            void *pLobBuf;
                            while ((nActualWrite = Param.InvokeWriter(
                                ePieceType, SA_DefaultMaxLong, pLobBuf)) != 0)
                            {
                                (*Param.m_pString) += SAString((const char*)pLobBuf, nActualWrite);

                                if (ePieceType == SA_LastPiece)
                                    break;
                            }
                        }
#ifdef SA_UNICODE_WITH_UTF8
                        pValue = (void*)Param.m_pString->GetUTF8Chars();
                        datalen = (CS_INT)Param.m_pString->GetUTF8CharsLength();
#else
                        pValue = (void*)Param.m_pString->GetMultiByteChars();
                        datalen = (CS_INT)Param.m_pString->GetMultiByteCharsLength();
#endif
                        break;
                    default:
                        assert(false);
                    }
                }
            }

            CS_DATAFMT df;
            strcpy(df.name, sName.GetMultiByteChars());
            df.namelen = (CS_INT)sName.GetMultiByteCharsLength();
            df.datatype = type;
            df.maxlength = isOutputParam(Param) ? (CS_INT)nDataBufSize : CS_UNUSED;
            df.scale = Param.ParamScale();
            df.precision = Param.ParamPrecision();
            df.status = isOutputParam(Param) ? CS_RETURN : CS_INPUTVALUE;
            df.locale = NULL;

            // If the "default value" flag is set, we don't bind,
            // and in that case Sybase will use the "default parameter's value",
            // specified on SP creation.
            if (!Param.isDefault())
            {
                ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_param(
                    m_handles.m_command,
                    &df,
                    (CS_VOID*)pValue,
                    datalen,
                    indicator));
            }
        }
    }
}

/*virtual */
bool IsybCursor::ResultSetExists()
{
    return m_bResultsPending;
}

/*virtual */
void IsybCursor::DescribeFields(
    DescribeFields_cb_t fn)
{
    CS_INT nNumData;
    ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_res_info(
        m_handles.m_command,
        CS_NUMDATA,
        &nNumData,
        CS_UNUSED,
        NULL));

    for (CS_INT iField = 1; iField <= nNumData; ++iField)
    {
        CS_DATAFMT df;

        ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_describe(
            m_handles.m_command,
            (CS_INT)iField,
            &df));

        // prec and scale can be coerced if OpenClient doesn't set them (for example, MONEY type)
        CS_INT prec = df.precision;
        CS_INT scale = df.scale;
        SADataType_t eDataType = CnvtNativeToStd(
            df.datatype,
            prec,
            scale);

        SAString sFieldName;
#ifdef SA_UNICODE_WITH_UTF8
        sFieldName.SetUTF8Chars(df.name, df.namelen);
#else
        sFieldName = SAString(df.name, df.namelen);
#endif
        (m_pCommand->*fn)(
            sFieldName,
            eDataType,
            (int)df.datatype,
            df.maxlength,
            prec,
            scale,
            (df.status & CS_CANBENULL) == 0,
            nNumData);
    }

    // We can use ct_get_data with last fields only
    bool bChangeType = false;
    for (int i = m_pCommand->m_nFieldCount - 1; i >= 0; --i)
    {
        SAField *field = m_pCommand->m_ppFields[i];
        SADataType_t eDataType = field->FieldType();
        bChangeType |= !isLongOrLob(eDataType);
        if (bChangeType && isLongOrLob(eDataType))
        {
            field->setFieldSize(sybAPI::DefaultLongMaxLength());
            if (SA_dtLongChar == eDataType)
                field->setFieldType(SA_dtString);
            else if (SA_dtLongBinary == eDataType)
                field->setFieldType(SA_dtBytes);
        }
    }
}

/*virtual */
long IsybCursor::GetRowsAffected()
{
    return m_nRowsAffected;
}

void IsybCursor::ct_bind_Buffer(
    int nCol,	// 1-based
    void *pInd,
    size_t nIndSize,
    void *pSize,
    size_t nSizeSize,
    void *pValue,
    size_t nValueSize,
    SADataType_t eDataType,
    SAString sName,
    CS_INT count,
    CS_INT scale,
    CS_INT precision)
{
    assert(nIndSize == sizeof(CS_SMALLINT));
    if (nIndSize != sizeof(CS_SMALLINT))
        return;
    assert(nSizeSize == sizeof(CS_INT));
    if (nSizeSize != sizeof(CS_INT))
        return;

    CS_DATAFMT datafmt;
    datafmt.datatype = (CS_INT)CnvtStdToNative(eDataType);
    datafmt.count = count;
    datafmt.format = CS_FMT_UNUSED;
    datafmt.maxlength = (CS_INT)nValueSize;
    datafmt.scale = scale;	//CS_SRC_VALUE;
    datafmt.precision = precision;	//CS_SRC_VALUE;
    datafmt.locale = NULL;

    bool bLong = false;
    switch (eDataType)
    {
    case SA_dtUnknown:
        throw SAException(SA_Library_Error, SA_Library_Error_UnknownColumnType, -1, IDS_UNKNOWN_COLUMN_TYPE, (const SAChar*)sName);
    case SA_dtBool:
        assert(datafmt.maxlength >= (CS_INT)sizeof(CS_BIT));
        break;
    case SA_dtShort:
        assert(datafmt.maxlength >= (CS_INT)sizeof(short));
        break;
    case SA_dtUShort:
        assert(datafmt.maxlength >= (CS_INT)sizeof(unsigned short));
        break;
    case SA_dtLong:
#ifdef SA_64BIT
        assert(datafmt.maxlength >= (CS_INT)sizeof(int));
#else
        assert(datafmt.maxlength >= (CS_INT)sizeof(long));
#endif
        break;
    case SA_dtULong:
#ifdef SA_64BIT
        assert(datafmt.maxlength >= (CS_INT)sizeof(unsigned int));
#else
        assert(datafmt.maxlength >= (CS_INT)sizeof(unsigned long));
#endif
        break;
    case SA_dtDouble:
        assert(datafmt.maxlength >= (CS_INT)sizeof(double));
        break;
    case SA_dtNumeric:
        assert(datafmt.maxlength >= (CS_INT)sizeof(CS_NUMERIC));
        break;
    case SA_dtDateTime:
        assert(datafmt.maxlength >= (CS_INT)sizeof(CS_DATETIME));
        break;
    case SA_dtString:
        break;
    case SA_dtBytes:
        break;
    case SA_dtLongBinary:
    case SA_dtLongChar:
        bLong = true;
        break;
    default:
        assert(false);	// unknown type
    }

    if (!bLong)
    {
        ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_bind(
            m_handles.m_command,
            nCol,
            &datafmt,
            (CS_VOID*)pValue,
            (CS_INT*)pSize,
            (CS_SMALLINT*)pInd));
    }
}

/*virtual */
void IsybCursor::SetFieldBuffer(
    int nCol,	// 1-based
    void *pInd,
    size_t nIndSize,
    void *pSize,
    size_t nSizeSize,
    void *pValue,
    size_t nValueSize)
{
    SAField &Field = m_pCommand->Field(nCol);

    ct_bind_Buffer(
        nCol,
        pInd, nIndSize,
        pSize, nSizeSize,
        pValue, nValueSize,
        Field.FieldType(), Field.Name(),
        m_cRowsToPrefetch,
        Field.FieldScale(),
        Field.FieldPrecision());
}

/*virtual */
void IsybCursor::SetSelectBuffers()
{
    SAString sOption = m_pCommand->Option(SACMD_PREFETCH_ROWS);
    if (!sOption.IsEmpty())
    {
        // do not use bulk fetch if there are text or image columns
        // do not use bulk fetch with scrollable cursor
        if (m_bScrollableResultSet || FieldCount(4, SA_dtLongBinary, SA_dtLongChar, SA_dtBLob, SA_dtCLob))
            m_cRowsToPrefetch = 1;
        else
        {
            m_cRowsToPrefetch = sa_toi((const SAChar*)sOption);
            if (!m_cRowsToPrefetch)
                m_cRowsToPrefetch = 1;
        }
    }
    else
        m_cRowsToPrefetch = 1;

    m_cRowsObtained = 0;
    m_cRowCurrent = 0;

    // use default helpers
    AllocSelectBuffer(sizeof(CS_SMALLINT), sizeof(CS_INT), m_cRowsToPrefetch);
}

/*virtual */
bool IsybCursor::ConvertIndicator(
    int nPos,	// 1-based, can be field or param pos
    int nNotConverted,
    SAValueRead &vr,
    ValueType_t eValueType,
    void *pInd, size_t nIndSize,
    void *pSize, size_t nSizeSize,
    size_t &nRealSize,
    int nBulkReadingBufPos) const
{
    if (eValueType == ISA_ParamValue)
        if (((SAParam&)vr).isDefault())
            return false;	// nothing to convert, we haven't bound

    if (!isLongOrLob(vr.DataType()))
        return ISACursor::ConvertIndicator(
        nPos,
        nNotConverted,
        vr, eValueType,
        pInd, nIndSize,
        pSize, nSizeSize,
        nRealSize,
        nBulkReadingBufPos);

    char pBuf[1];
    CS_RETCODE rcd = ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_get_data(
        m_handles.m_command,
        nPos - nNotConverted,
        pBuf, 0,
        NULL));

    *vr.m_pbNull = (rcd == CS_END_ITEM || rcd == CS_END_DATA);
    return true;	// converted
}

#ifdef SA_UNICODE_WITH_UTF8
/*virtual */
void IsybCursor::ConvertString(SAString &String, const void *pData, size_t nRealSize)
{
    String.SetUTF8Chars((const char*)pData, nRealSize);
}
#endif

// default implementation (ISACursor::OutputBindingTypeIsParamType)
// returns false
// But in OpenClient there is a possibility to bind in/out separately
// and we use this
/*virtual */
bool IsybCursor::OutputBindingTypeIsParamType()
{
    return true;
}

void IsybCursor::FetchParamResult()
{
    // first bind all output params
    int nOutputs = 0;
    void *pBuf = m_pParamBuffer;
    for (int i = 0; i < m_pCommand->ParamCount(); ++i)
    {
        SAParam &Param = m_pCommand->ParamByIndex(i);

        void *pNull;
        void *pSize;
        size_t nDataBufSize;
        void *pValue;
        IncParamBuffer(pBuf, pNull, pSize, nDataBufSize, pValue);

        if (!isOutputParam(Param))
            continue;
        if (Param.ParamDirType() == SA_ParamReturn)
            continue;
        if (Param.isDefault())
            continue;

        ++nOutputs;

        ct_bind_Buffer(
            nOutputs,
            pNull, sizeof(CS_SMALLINT),
            pSize, sizeof(CS_INT),
            pValue, nDataBufSize,
            Param.ParamType(), Param.Name(),
            1,
            Param.ParamScale(),
            Param.ParamPrecision());
    }

    // then fetch parameter row
    CS_INT nRowsRead;
    CS_RETCODE rcd = ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_fetch(
        m_handles.m_command,
        CS_UNUSED,
        CS_UNUSED,
        CS_UNUSED,
        &nRowsRead));
    assert(nRowsRead == 1);

    while (rcd != CS_END_DATA)
    {
        rcd = ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_fetch(
            m_handles.m_command,
            CS_UNUSED,
            CS_UNUSED,
            CS_UNUSED,
            NULL/*&nRowsRead*/));
    }
}

void IsybCursor::FetchStatusResult()
{
    // first bind return status
    void *pBuf = m_pParamBuffer;
    for (int i = 0; i < m_pCommand->ParamCount(); ++i)
    {
        SAParam &Param = m_pCommand->ParamByIndex(i);

        void *pNull;
        void *pSize;
        size_t nDataBufSize;
        void *pValue;
        IncParamBuffer(pBuf, pNull, pSize, nDataBufSize, pValue);

        if (Param.ParamDirType() != SA_ParamReturn)
            continue;

        ct_bind_Buffer(
            1,
            pNull, sizeof(CS_SMALLINT),
            pSize, sizeof(CS_INT),
            pValue, nDataBufSize,
            Param.ParamType(), Param.Name(),
            1,
            Param.ParamScale(),
            Param.ParamPrecision());
    }

    // then fetch return status
    CS_INT nRowsRead;
    CS_RETCODE rcd = ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_fetch(
        m_handles.m_command,
        CS_UNUSED,
        CS_UNUSED,
        CS_UNUSED,
        &nRowsRead));
    assert(nRowsRead == 1);

    while (rcd != CS_END_DATA)
    {
        rcd = ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_fetch(
            m_handles.m_command,
            CS_UNUSED,
            CS_UNUSED,
            CS_UNUSED,
            NULL/*&nRowsRead*/));
    }
}

/*virtual */
bool IsybCursor::FetchNext()
{
    if (m_bScrollableResultSet)
        return FetchRow(CS_NEXT, CS_UNUSED);

    assert(m_bResultsPending == true);

    if (m_cRowCurrent == m_cRowsObtained)
    {
        CS_RETCODE rcd = ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_fetch(
            m_handles.m_command,
            CS_UNUSED,
            CS_UNUSED,
            CS_UNUSED,
            &m_cRowsObtained));
        if (rcd == CS_END_DATA)
            m_cRowsObtained = 0;

        m_cRowCurrent = 0;
    }

    if (m_cRowsObtained)
    {
        // use default helpers
        ConvertSelectBufferToFields(m_cRowCurrent++);
    }
    else if (!m_bScrollableResultSet)
        ProcessBatchUntilEndOrResultSet();

    return (m_cRowsObtained != 0);
}

bool IsybCursor::FetchRow(CS_INT type, CS_INT offset)
{
    if (NULL == g_sybAPI.ct_scroll_fetch)
        return false;

    CS_RETCODE rcd = ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_scroll_fetch(
        m_handles.m_command, type, offset, CS_TRUE, &m_cRowsObtained));

    if (rcd == CS_END_DATA || rcd == CS_CURSOR_AFTER_LAST || rcd == CS_CURSOR_BEFORE_FIRST)
        m_cRowsObtained = 0;

    m_cRowCurrent = 0;

    if (m_cRowsObtained) // default helpers
        ConvertSelectBufferToFields(m_cRowCurrent++);

    return (m_cRowsObtained != 0);
}

/*virtual */
bool IsybCursor::FetchPrior()
{
    return FetchRow(CS_PREV, CS_UNUSED);
}

/*virtual */
bool IsybCursor::FetchFirst()
{
    return FetchRow(CS_FIRST, CS_UNUSED);
}

/*virtual */
bool IsybCursor::FetchLast()
{
    return FetchRow(CS_LAST, CS_UNUSED);
}

/*virtual */
bool IsybCursor::FetchPos(int offset, bool Relative/* = false*/)
{
    return FetchRow(Relative ? CS_RELATIVE : CS_ABSOLUTE, CS_INT(offset));
}

/*virtual */
void IsybCursor::ReadLongOrLOB(
    ValueType_t /* eValueType*/,
    SAValueRead &vr,
    void * /*pValue*/,
    size_t/* nBufSize*/,
    saLongOrLobReader_t fnReader,
    size_t nReaderWantedPieceSize,
    void *pAddlData)
{
    SAField &Field = (SAField &)vr;

    // get long size
    CS_IODESC iodesc;
    ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_data_info(
        m_handles.m_command,
        CS_GET,
        Field.Pos(),
        &iodesc));

    CS_INT nLongLen = iodesc.total_txtlen;
    assert(nLongLen > 0);	// known

    SADummyConverter DummyConverter;
    SAMultibyte2UnicodeConverter Multibyte2UnicodeConverter;
    SAUnicode2MultibyteConverter Unicode2MultibyteConverter;
    ISADataConverter *pIConverter = &DummyConverter;
    size_t nCnvtLongSizeMax = nLongLen;
#ifdef SA_UNICODE
    // unitext field always comes in UTF-16
    if( (SA_dtLongChar == vr.DataType() ||
        SA_dtCLob == vr.DataType()) && CS_TEXT_TYPE == iodesc.datatype )
    {
        pIConverter = &Multibyte2UnicodeConverter;

#ifdef SA_UNICODE_WITH_UTF8
        Multibyte2UnicodeConverter.SetUseUTF8();
#endif

        // in the worst case each byte can represent a Unicode character
        nCnvtLongSizeMax = nLongLen*sizeof(wchar_t);
    }
#else
    if ((SA_dtLongChar == vr.DataType() ||
        SA_dtCLob == vr.DataType()) && CS_UNITEXT_TYPE == iodesc.datatype)
        pIConverter = &Unicode2MultibyteConverter;
#endif

    unsigned char* pBuf;
    size_t nPortionSize = vr.PrepareReader(
        sa_max(nCnvtLongSizeMax, (size_t)nLongLen),	// real size is nCnvtLongSizeMax
        IsybConnection::MaxLongPiece,
        pBuf,
        fnReader,
        nReaderWantedPieceSize,
        pAddlData);
    assert(nPortionSize <= IsybConnection::MaxLongPiece);

    size_t nCnvtPieceSize = nPortionSize;
    size_t nTotalPassedToReader = 0l;

    SAPieceType_t ePieceType = SA_FirstPiece;
    size_t nTotalRead = 0l;

    do
    {
        if (nLongLen)	// known
            nPortionSize = sa_min(nPortionSize, nLongLen - nTotalRead);
        else
        {
            vr.InvokeReader(SA_LastPiece, pBuf, 0);
            break;
        }

        CS_INT nActualRead;
        ((IsybConnection*)m_pISAConnection)->Check(g_sybAPI.ct_get_data(
            m_handles.m_command,
            Field.Pos(),
            pBuf, (CS_INT)nPortionSize,
            &nActualRead));

        nTotalRead += nActualRead;

        if (nTotalRead == (size_t)nLongLen)
        {
            if (ePieceType == SA_NextPiece)
                ePieceType = SA_LastPiece;
            else    // the whole BLob was read in one piece
            {
                assert(ePieceType == SA_FirstPiece);
                ePieceType = SA_OnePiece;
            }
        }

        pIConverter->PutStream(pBuf, nActualRead, ePieceType);
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

        //vr.InvokeReader(ePieceType, pBuf, nActualRead);

        if (ePieceType == SA_FirstPiece)
            ePieceType = SA_NextPiece;
    } while (nTotalRead < (size_t)nLongLen);
    assert((CS_INT)nTotalRead == nLongLen);
}

/*virtual */
void IsybCursor::DescribeParamSP()
{
    SACommand cmd(m_pISAConnection->getSAConnection());
    cmd.setOption(_TSA("ct_cursor")).Format(_TSA("pp%08X"), this);

    SAString sProcName = m_pCommand->CommandText();
    SAString sSQL;

    // if procname has database prefix  
    // - we should query params from THIS base from 'dbo' schema, instead of base that we connected to
    SAString sPrefix;
    // the sProcName can be:
    // a) qualifier.owner.name (CATALOG.SCHEMA.NAME) - SCHEMA can be empty qualifier..name
    // b) owner.name (SCHEMA.NAME)
    // c) name (NAME)
    size_t pos1 = sProcName.Find('.');
    if (SIZE_MAX != pos1)	// c)
    {
        size_t pos2 = sProcName.Find('.', pos1 + 1);
        if (SIZE_MAX != pos2)	// b)
        {
            sPrefix = sProcName.Left(pos1) + _TSA('.');
        }
    }
    // prefix Catalog.dbo
    sPrefix += _TSA("dbo");

    IsybConnection::ServerType type = ((IsybConnection*)m_pISAConnection)->getServerType();

    if (IsybConnection::eASA == type)
    {
        // Check if we are on Adaptive Server Anywhere
        sSQL.Format(_TSA("select spp.parm_name as name, spp.domain_id as type, spp.width as length, ")
            _TSA("spp.width as prec, spp.scale, spp.parm_mode_in || spp.parm_mode_out as parm_mode ")
            _TSA("from sysobjects so, sysprocedure sp, sysprocparm spp where ")
            _TSA("so.id = object_id('%") SA_FMT_STR _TSA("') and so.type = 'P' and ")
            _TSA("so.name = sp.proc_name and so.uid = sp.creator and ")
            _TSA("spp.proc_id = sp.proc_id and spp.parm_type = 0 ")
            _TSA("order by spp.parm_id"),
            (const SAChar*)sProcName);
        cmd.setCommandText(sSQL);
        cmd.Execute();
    }
    else // IsybConnection::eASE == type
    {
        // find proc number
        // specifying procedure number allows to call proper 
        // stored precedure version
        // eg 'stored_proc;1'
        SAString sNum(_TSA("1"));
        size_t delim = sProcName.Find(_TSA(';'));
        if (SIZE_MAX != delim)
        {
            sNum = sProcName.Mid(delim + 1);
            sProcName = sProcName.Left(delim);
        }
        // If we are on Adaptive Server Enterprise
        sSQL.Format(_TSA("select sc.name, sc.type, sc.length, sc.prec, sc.scale, ")
            _TSA("(case sc.status2 & 15 when 1 then 'YN' when 2 then 'YY' else 'YN' end) as parm_mode ")
            _TSA("from %") SA_FMT_STR _TSA(".sysobjects so, %") SA_FMT_STR _TSA(".syscolumns sc where ")
            _TSA("so.id = object_id('%") SA_FMT_STR _TSA("') and so.type = 'P' and ")
            _TSA("so.id = sc.id and sc.number = %") SA_FMT_STR _TSA(" order by sc.colid"),
            (const SAChar*)sPrefix,
            (const SAChar*)sPrefix,
            (const SAChar*)sProcName,
            (const SAChar*)sNum);
    }

    cmd.setCommandText(sSQL);
    cmd.Execute();

    while (cmd.FetchNext())
    {
        SAString sName = cmd[_TSA("name")].asString();
        // if "name" database field is CHAR it could be right padded with spaces
        sName.TrimRight(_TSA(' '));
        if (sName.Left(1) == _TSA("@"))
            sName.Delete(0);

        int nParamSize = cmd[_TSA("length")].asShort();
        short nType = cmd[_TSA("type")].asShort();
        short nPrec = cmd[_TSA("prec")].asShort();
        short nScale = cmd[_TSA("scale")].asShort();
        SADataType_t eDataType = IsybConnection::eASA == type ?
            CnvtNativeTypeFromASADomainIDToStd(nType, 0, nParamSize, nPrec, nScale)
            :
            CnvtNativeTypeFromASESysColumnsToStd(nType, 0, nParamSize, nPrec, nScale);

        SAParamDirType_t eDirType = SA_ParamInput;
        SAString sParmMode = cmd[_TSA("parm_mode")].asString();
        if (sParmMode == _TSA("YN"))
            eDirType = SA_ParamInput;
        else if (sParmMode == _TSA("YY"))
            eDirType = SA_ParamInputOutput;
        else if (sParmMode == _TSA("NY"))
            eDirType = SA_ParamOutput;
        else
            assert(false);

#ifdef SA_PARAM_USE_DEFAULT
        SAParam& p =
#endif
            m_pCommand->CreateParam(sName,
            eDataType,
            IsybCursor::CnvtStdToNative(eDataType),
            nParamSize, nPrec, nScale,
            eDirType);
#ifdef SA_PARAM_USE_DEFAULT
        if( eDirType == SA_ParamInput )
            p.setAsDefault();
#endif
    }

    // now check if SA_Param_Return parameter is described
    // if not (currently true for Sybase ASE and ASA) add it manually
    m_pCommand->CreateParam(
        _TSA("RETURN_VALUE"),
        SA_dtLong,
        IsybCursor::CnvtStdToNative(SA_dtLong),
        sizeof(long), 0, 0,	// precision and scale as reported by native API
        SA_ParamReturn);
}

/*virtual */
saAPI *IsybConnection::NativeAPI() const
{
    return &g_sybAPI;
}

/*virtual */
saConnectionHandles *IsybConnection::NativeHandles()
{
    return &m_handles;
}

/*virtual */
saCommandHandles *IsybCursor::NativeHandles()
{
    return &m_handles;
}

/*virtual */
void IsybConnection::setIsolationLevel(
    SAIsolationLevel_t eIsolationLevel)
{
    SAString sCmd(_TSA("set transaction isolation level "));
    SACommand cmd(m_pSAConnection);

    switch (eIsolationLevel)
    {
    case SA_ReadUncommitted:
        sCmd += _TSA("0\0read uncommitted");
        break;
    case SA_ReadCommitted:
        sCmd += _TSA("1\0read committed");
        break;
    case SA_RepeatableRead:
        sCmd += _TSA("2\0repeatable read");
        break;
    case SA_Serializable:
        sCmd += _TSA("3\0serializable");
        break;
    default:
        assert(false);
        return;
    }

    cmd.setCommandText(sCmd, SA_CmdSQLStmt);
    cmd.Execute();
    cmd.Close();
}

/*virtual */
void IsybConnection::setAutoCommit(
    SAAutoCommit_t eAutoCommit)
{
    SACommand cmd(m_pSAConnection);
    cmd.setCommandText(_TSA("commit tran"), SA_CmdSQLStmt);
    cmd.Execute();

    if (eAutoCommit == SA_AutoCommitOff)
    {
        cmd.setCommandText(_TSA("begin tran"), SA_CmdSQLStmt);
        cmd.Execute();
    }

    cmd.Close();
}

//////////////////////////////////////////////////////////////////////
// sybExternalConnection Class
//////////////////////////////////////////////////////////////////////

sybExternalConnection::sybExternalConnection(
    SAConnection *pCon,
    CS_CONTEXT *context,
    CS_CONNECTION *connection)
{
    m_bAttached = false;

    m_pCon = pCon;

    m_context = context;
    m_connection = connection;
    m_pExternalUserData = sa_malloc(sizeof(void *));
    m_nExternalUserDataAllocated = (CS_INT)sizeof(void *);
}

void sybExternalConnection::Attach()
{
    Detach();	// if any

    if (m_pCon->isConnected())
        m_pCon->Disconnect();
    m_pCon->setClient(SA_Sybase_Client);

    sybAPI *psybAPI = (sybAPI *)m_pCon->NativeAPI();
    sybConnectionHandles *psybConnectionHandles = (sybConnectionHandles *)
        m_pCon->NativeHandles();

    CS_RETCODE rc;
    // save original callbacks for "external" handles
    // we will later replace them with SQLAPI++ compatible ones
    rc = psybAPI->ct_callback(
        m_context, NULL, CS_GET, CS_CLIENTMSG_CB,
        &m_ExternalContextClientMsg_cb);
    assert(rc == CS_SUCCEED);
    rc = psybAPI->ct_callback(
        m_context, NULL, CS_GET, CS_SERVERMSG_CB,
        &m_ExternalContextServerMsg_cb);
    assert(rc == CS_SUCCEED);
    rc = psybAPI->ct_callback(
        NULL, m_connection, CS_GET, CS_CLIENTMSG_CB,
        &m_ExternalConnectionClientMsg_cb);
    assert(rc == CS_SUCCEED);
    rc = psybAPI->ct_callback(
        NULL, m_connection, CS_GET, CS_SERVERMSG_CB,
        &m_ExternalConnectionServerMsg_cb);
    assert(rc == CS_SUCCEED);
    // disable original callbacks for "external" context
    rc = psybAPI->ct_callback(
        m_context, NULL, CS_SET, CS_CLIENTMSG_CB,
        NULL);
    assert(rc == CS_SUCCEED);
    rc = psybAPI->ct_callback(
        m_context, NULL, CS_SET, CS_SERVERMSG_CB,
        NULL);
    assert(rc == CS_SUCCEED);

    // save original user data for "external" context handle
    // we will later replace it with SQLAPI++ compatible
    rc = psybAPI->cs_config(
        m_context, CS_GET, CS_USERDATA,
        m_pExternalUserData, m_nExternalUserDataAllocated,
        &m_nExternalUserDataLen);
    if (rc != CS_SUCCEED)
    {
        sa_realloc((void**)&m_pExternalUserData, m_nExternalUserDataLen);
        m_nExternalUserDataAllocated = m_nExternalUserDataLen;
        rc = psybAPI->cs_config(
            m_context, CS_GET, CS_USERDATA,
            m_pExternalUserData, m_nExternalUserDataAllocated,
            NULL);
        assert(rc == CS_SUCCEED);
    }

    // get original callback handlers for client and server messages
    // (context level callbacks)
    CS_VOID	*SQLAPIClientMsg_cb = NULL;
    CS_VOID	*SQLAPIServerMsg_cb = NULL;
    rc = psybAPI->ct_callback(
        psybConnectionHandles->m_context, NULL, CS_GET, CS_CLIENTMSG_CB,
        &SQLAPIClientMsg_cb);
    assert(rc == CS_SUCCEED);
    rc = psybAPI->ct_callback(
        psybConnectionHandles->m_context, NULL, CS_GET, CS_SERVERMSG_CB,
        &SQLAPIServerMsg_cb);
    assert(rc == CS_SUCCEED);

    // replace "external" context user data with SQLAPI++ compatible
    SASybErrInfo *pSybErrInfo = &m_SybErrInfo;
    rc = psybAPI->cs_config(
        m_context,
        CS_SET,
        CS_USERDATA,
        &pSybErrInfo,
        (CS_INT)sizeof(SASybErrInfo*),
        NULL);
    assert(rc == CS_SUCCEED);

    // replace "external" callbacks with SQLAPI++ default ones
    rc = psybAPI->ct_callback(
        m_context, NULL, CS_SET, CS_CLIENTMSG_CB,
        SQLAPIClientMsg_cb);
    assert(rc == CS_SUCCEED);
    rc = psybAPI->ct_callback(
        m_context, NULL, CS_SET, CS_SERVERMSG_CB,
        SQLAPIServerMsg_cb);
    assert(rc == CS_SUCCEED);
    rc = psybAPI->ct_callback(
        NULL, m_connection, CS_SET, CS_CLIENTMSG_CB,
        SQLAPIClientMsg_cb);
    assert(rc == CS_SUCCEED);
    rc = psybAPI->ct_callback(
        NULL, m_connection, CS_SET, CS_SERVERMSG_CB,
        SQLAPIServerMsg_cb);
    assert(rc == CS_SUCCEED);

    // save original SAConnection handles
    m_contextSaved = psybConnectionHandles->m_context;
    m_connectionSaved = psybConnectionHandles->m_connection;
    // replace SAConnection handles with "external" ones
    psybConnectionHandles->m_context = m_context;
    psybConnectionHandles->m_connection = m_connection;

    m_bAttached = true;
}

void sybExternalConnection::Detach()
{
    if (!m_bAttached)
        return;

    assert(m_pCon->isConnected());
    sybAPI *psybAPI = (sybAPI *)m_pCon->NativeAPI();
    sybConnectionHandles *psybConnectionHandles = (sybConnectionHandles *)
        m_pCon->NativeHandles();

    // restore original SAConnection handles
    psybConnectionHandles->m_context = m_contextSaved;
    psybConnectionHandles->m_connection = m_connectionSaved;

    CS_RETCODE rc;
    // restore "external" context user data
    if (m_nExternalUserDataLen)
        rc = psybAPI->cs_config(
        m_context,
        CS_SET,
        CS_USERDATA,
        m_pExternalUserData,
        m_nExternalUserDataLen,
        NULL);
    else
        rc = psybAPI->cs_config(
        m_context,
        CS_CLEAR,
        CS_USERDATA,
        NULL,
        CS_UNUSED,
        NULL);
    assert(rc == CS_SUCCEED);
    // restore "external" handles callbacks
    rc = psybAPI->ct_callback(
        m_context, NULL, CS_SET, CS_CLIENTMSG_CB,
        m_ExternalContextClientMsg_cb);
    assert(rc == CS_SUCCEED);
    rc = psybAPI->ct_callback(
        m_context, NULL, CS_SET, CS_SERVERMSG_CB,
        m_ExternalContextServerMsg_cb);
    assert(rc == CS_SUCCEED);
    rc = psybAPI->ct_callback(
        NULL, m_connection, CS_SET, CS_CLIENTMSG_CB,
        m_ExternalConnectionClientMsg_cb);
    assert(rc == CS_SUCCEED);
    rc = psybAPI->ct_callback(
        NULL, m_connection, CS_SET, CS_SERVERMSG_CB,
        m_ExternalConnectionServerMsg_cb);
    assert(rc == CS_SUCCEED);

    if (rc == CS_SUCCEED)
        m_bAttached = false;
}

sybExternalConnection::~sybExternalConnection()
{
    try
    {
        Detach();
    }
    catch (SAException &)
    {
    }
}

/*virtual */
void IsybConnection::CnvtInternalToNumeric(
    SANumeric &numeric,
    const void *pInternal,
    int nInternalSize)
{
    assert(nInternalSize <= (int)sizeof(CS_NUMERIC));

    CS_INT destlen = 128;
    CS_VOID *dest = new char[destlen];

    CS_DATAFMT srcfmt, destfmt;
    memset(&srcfmt, 0, sizeof(CS_DATAFMT));
    memset(&destfmt, 0, sizeof(CS_DATAFMT));

    srcfmt.datatype = CS_NUMERIC_TYPE;
    srcfmt.format = CS_FMT_UNUSED;
    srcfmt.maxlength = nInternalSize;
    srcfmt.locale = NULL;

    destfmt.datatype = CS_CHAR_TYPE;
    destfmt.format = CS_FMT_UNUSED;
    destfmt.maxlength = destlen;
    destfmt.locale = NULL;

    CS_INT resultlen = 0;
    Check(g_sybAPI.cs_convert(
        m_handles.m_context,
        &srcfmt,
        (CS_VOID *)pInternal,
        &destfmt,
        dest,
        &resultlen));

    numeric = SAString((const char*)dest, resultlen);
    delete[](char*)dest;
}

void IsybConnection::CnvtNumericToInternal(
    const SANumeric &numeric,
    CS_NUMERIC &Internal,
    CS_INT &datalen)
{
    SAString s = numeric.operator SAString();

    CS_DATAFMT srcfmt, destfmt;
    memset(&srcfmt, 0, sizeof(CS_DATAFMT));
    memset(&destfmt, 0, sizeof(CS_DATAFMT));

    srcfmt.datatype = CS_CHAR_TYPE;
    srcfmt.format = CS_FMT_UNUSED;
    srcfmt.maxlength = (CS_INT)s.GetLength();
    srcfmt.locale = NULL;

    destfmt.datatype = CS_NUMERIC_TYPE;
    destfmt.format = CS_FMT_UNUSED;
    destfmt.maxlength = (CS_INT)sizeof(CS_NUMERIC);
    destfmt.locale = NULL;
    destfmt.scale = numeric.scale;
    destfmt.precision = numeric.precision;

    Check(g_sybAPI.cs_convert(
        m_handles.m_context,
        &srcfmt,
        (CS_VOID *)s.GetMultiByteChars(),
        &destfmt,
        (CS_VOID *)&Internal,
        &datalen));
}
