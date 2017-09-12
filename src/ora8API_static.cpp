// ora8API.cpp
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <oraAPI.h>

long g_nORA8DLLVersionLoaded = 0l;
static long g_nORA8DLLRefs = 0l;

static SAMutex ora8LoaderMutex;

// API definitions
ora8API g_ora8API;

ora8API::ora8API()
{
	// 8.0.x calls
	OCIInitialize	= NULL;
	OCIHandleAlloc	= NULL;
	OCIHandleFree	= NULL;
	OCIDescriptorAlloc	= NULL;
	OCIDescriptorFree	= NULL;
	OCIEnvInit	= NULL;
	OCIServerAttach	= NULL;
	OCIServerDetach	= NULL;
	OCISessionBegin	= NULL;
	OCISessionEnd	= NULL;
	OCILogon	= NULL;
	OCILogoff	= NULL;
	OCIPasswordChange	= NULL;
	OCIStmtPrepare	= NULL;
	OCIBindByPos	= NULL;
	OCIBindByName	= NULL;
	OCIBindObject	= NULL;
	OCIBindDynamic	= NULL;
	OCIBindArrayOfStruct	= NULL;
	OCIStmtGetPieceInfo	= NULL;
	OCIStmtSetPieceInfo	= NULL;
	OCIStmtExecute	= NULL;
	OCIDefineByPos	= NULL;
	OCIDefineObject	= NULL;
	OCIDefineDynamic	= NULL;
	OCIDefineArrayOfStruct	= NULL;
	OCIStmtFetch	= NULL;
	OCIStmtGetBindInfo	= NULL;
	OCIDescribeAny	= NULL;
	OCIParamGet	= NULL;
	OCIParamSet	= NULL;
	OCITransStart	= NULL;
	OCITransDetach	= NULL;
	OCITransCommit	= NULL;
	OCITransRollback	= NULL;
	OCITransPrepare	= NULL;
	OCITransForget	= NULL;
	OCIErrorGet	= NULL;
	OCILobAppend	= NULL;
	OCILobAssign	= NULL;
	OCILobCharSetForm	= NULL;
	OCILobCharSetId	= NULL;
	OCILobCopy	= NULL;
	OCILobDisableBuffering	= NULL;
	OCILobEnableBuffering	= NULL;
	OCILobErase	= NULL;
	OCILobFileClose	= NULL;
	OCILobFileCloseAll	= NULL;
	OCILobFileExists	= NULL;
	OCILobFileGetName	= NULL;
	OCILobFileIsOpen	= NULL;
	OCILobFileOpen	= NULL;
	OCILobFileSetName	= NULL;
	OCILobFlushBuffer	= NULL;
	OCILobGetLength	= NULL;
	OCILobIsEqual	= NULL;
	OCILobLoadFromFile	= NULL;
	OCILobLocatorIsInit	= NULL;
	OCILobRead	= NULL;
	OCILobRead2	= NULL;
	OCILobTrim	= NULL;
	OCILobWrite	= NULL;
	OCILobWrite2	= NULL;
	OCIBreak	= NULL;
	OCIReset	= NULL;
	OCIServerVersion	= NULL;
	OCIAttrGet	= NULL;
	OCIAttrSet	= NULL;
	OCISvcCtxToLda	= NULL;
	OCILdaToSvcCtx	= NULL;
	OCIResultSetToStmt	= NULL;

	// 8.1.x (8i) calls
	OCIEnvCreate	= NULL;
	OCIEnvNlsCreate = NULL;
	OCIDurationBegin	= NULL;
	OCIDurationEnd	= NULL;
	OCILobCreateTemporary	= NULL;
	OCILobFreeTemporary	= NULL;
	OCILobIsTemporary	= NULL;

	OCIAQEnq	= NULL;
	OCIAQDeq	= NULL;
	OCIAQListen	= NULL;
	OCISubscriptionRegister	= NULL;
	OCISubscriptionPost	= NULL;
	OCISubscriptionUnRegister	= NULL;
	OCISubscriptionDisable	= NULL;
	OCISubscriptionEnable	= NULL;

	// 9.x datetime
	OCIDateTimeConstruct = NULL;
	OCIDateTimeGetDate = NULL;
	OCIDateTimeGetTime = NULL;
    OCIDateTimeGetTimeZoneOffset = NULL;

	OCINlsNumericInfoGet = NULL;

	OCIStmtFetch2 = NULL;

    OCIConnectionPoolCreate = NULL;
    OCIConnectionPoolDestroy = NULL;
    
    envhp = NULL;
};

ora8ConnectionHandles::ora8ConnectionHandles()
{
	m_pOCIEnv		= NULL;
	m_pOCIError		= NULL;
	m_pOCISvcCtx	= NULL;
	m_pOCIServer	= NULL;
	m_pOCISession	= NULL;
}

ora8CommandHandles::ora8CommandHandles()
{
	m_pOCIStmt		= NULL;
	m_pOCIError		= NULL;
}

static void LoadAPI()
{
	// 8.0.x calls
	g_ora8API.OCIInitialize	= OCIInitialize;
	g_ora8API.OCIHandleAlloc	= OCIHandleAlloc;
	g_ora8API.OCIHandleFree	= OCIHandleFree;
	g_ora8API.OCIDescriptorAlloc	= OCIDescriptorAlloc;
	g_ora8API.OCIDescriptorFree	= OCIDescriptorFree;
	g_ora8API.OCIEnvInit	= OCIEnvInit;
	g_ora8API.OCIServerAttach	= OCIServerAttach;
	g_ora8API.OCIServerDetach	= OCIServerDetach;
	g_ora8API.OCISessionBegin	= OCISessionBegin;
	g_ora8API.OCISessionEnd	= OCISessionEnd;
	g_ora8API.OCILogon	= OCILogon;
	g_ora8API.OCILogoff	= OCILogoff;
	g_ora8API.OCIPasswordChange	= OCIPasswordChange;
	g_ora8API.OCIStmtPrepare	= OCIStmtPrepare;
	g_ora8API.OCIBindByPos	= OCIBindByPos;
	g_ora8API.OCIBindByName	= OCIBindByName;
	g_ora8API.OCIBindObject	= OCIBindObject;
	g_ora8API.OCIBindDynamic	= OCIBindDynamic;
	g_ora8API.OCIBindArrayOfStruct	= OCIBindArrayOfStruct;
	g_ora8API.OCIStmtGetPieceInfo	= OCIStmtGetPieceInfo;
	g_ora8API.OCIStmtSetPieceInfo	= OCIStmtSetPieceInfo;
	g_ora8API.OCIStmtExecute	= OCIStmtExecute;
	g_ora8API.OCIDefineByPos	= OCIDefineByPos;
	g_ora8API.OCIDefineObject	= OCIDefineObject;
	g_ora8API.OCIDefineDynamic	= OCIDefineDynamic;
	g_ora8API.OCIDefineArrayOfStruct	= OCIDefineArrayOfStruct;
	g_ora8API.OCIStmtFetch	= OCIStmtFetch;
	g_ora8API.OCIStmtGetBindInfo	= OCIStmtGetBindInfo;
	g_ora8API.OCIDescribeAny	= OCIDescribeAny;
	g_ora8API.OCIParamGet	= OCIParamGet;
	g_ora8API.OCIParamSet	= OCIParamSet;
	g_ora8API.OCITransStart	= OCITransStart;
	g_ora8API.OCITransDetach	= OCITransDetach;
	g_ora8API.OCITransCommit	= OCITransCommit;
	g_ora8API.OCITransRollback	= OCITransRollback;
	g_ora8API.OCITransPrepare	= OCITransPrepare;
	g_ora8API.OCITransForget	= OCITransForget;
	g_ora8API.OCIErrorGet	= OCIErrorGet;
	g_ora8API.OCILobAppend	= OCILobAppend;
	g_ora8API.OCILobAssign	= OCILobAssign;
	g_ora8API.OCILobCharSetForm	= OCILobCharSetForm;
	g_ora8API.OCILobCharSetId	= OCILobCharSetId;
	g_ora8API.OCILobCopy	= OCILobCopy;
	g_ora8API.OCILobDisableBuffering	= OCILobDisableBuffering;
	g_ora8API.OCILobEnableBuffering	= OCILobEnableBuffering;
	g_ora8API.OCILobErase	= OCILobErase;
	g_ora8API.OCILobFileClose	= OCILobFileClose;
	g_ora8API.OCILobFileCloseAll	= OCILobFileCloseAll;
	g_ora8API.OCILobFileExists	= OCILobFileExists;
	g_ora8API.OCILobFileGetName	= OCILobFileGetName;
	g_ora8API.OCILobFileIsOpen	= OCILobFileIsOpen;
	g_ora8API.OCILobFileOpen	= OCILobFileOpen;
	g_ora8API.OCILobFileSetName	= OCILobFileSetName;
	g_ora8API.OCILobFlushBuffer	= OCILobFlushBuffer;
	g_ora8API.OCILobGetLength	= OCILobGetLength;
	g_ora8API.OCILobIsEqual	= OCILobIsEqual;
	g_ora8API.OCILobLoadFromFile	= OCILobLoadFromFile;
	g_ora8API.OCILobLocatorIsInit	= OCILobLocatorIsInit;
	g_ora8API.OCILobRead	= OCILobRead;
	// comment OCILobRead2 out if OCI library is old
	g_ora8API.OCILobRead2	= OCILobRead2;
	g_ora8API.OCILobTrim	= OCILobTrim;
	g_ora8API.OCILobWrite	= OCILobWrite;
	//g_ora8API.OCILobWrite2	= OCILobWrite2;
	g_ora8API.OCIBreak	= OCIBreak;
	g_ora8API.OCIReset	= OCIReset;
	g_ora8API.OCIServerVersion	= OCIServerVersion;
	g_ora8API.OCIAttrGet	= OCIAttrGet;
	g_ora8API.OCIAttrSet	= OCIAttrSet;
	g_ora8API.OCISvcCtxToLda	= OCISvcCtxToLda;
	g_ora8API.OCILdaToSvcCtx	= OCILdaToSvcCtx;
	g_ora8API.OCIResultSetToStmt	= OCIResultSetToStmt;

	// 8.1.x (8i) calls
	g_ora8API.OCIEnvCreate	= OCIEnvCreate;
#ifdef SA_UNICODE
	g_ora8API.OCIEnvNlsCreate	= OCIEnvNlsCreate;
#endif
	g_ora8API.OCIDurationBegin = OCIDurationBegin;
	g_ora8API.OCIDurationEnd = OCIDurationEnd;
	g_ora8API.OCILobCreateTemporary = OCILobCreateTemporary;
	g_ora8API.OCILobFreeTemporary = OCILobFreeTemporary;
	g_ora8API.OCILobIsTemporary = OCILobIsTemporary;

	g_ora8API.OCIAQEnq	= OCIAQEnq;
	g_ora8API.OCIAQDeq	= OCIAQDeq;
	g_ora8API.OCIAQListen	= OCIAQListen;
	g_ora8API.OCISubscriptionRegister	= OCISubscriptionRegister;
	g_ora8API.OCISubscriptionPost	= OCISubscriptionPost;
	g_ora8API.OCISubscriptionUnRegister	= OCISubscriptionUnRegister;
	g_ora8API.OCISubscriptionDisable	= OCISubscriptionDisable;
	g_ora8API.OCISubscriptionEnable	= OCISubscriptionEnable;

	g_ora8API.OCIDateTimeConstruct	= OCIDateTimeConstruct;
	g_ora8API.OCIDateTimeGetDate = OCIDateTimeGetDate;
	g_ora8API.OCIDateTimeGetTime = OCIDateTimeGetTime;
    g_ora8API.OCIDateTimeGetTimeZoneOffset = OCIDateTimeGetTimeZoneOffset;

	g_ora8API.OCINlsNumericInfoGet = OCINlsNumericInfoGet;

	g_ora8API.OCIStmtFetch2	= OCIStmtFetch2;

    g_ora8API.OCIConnectionPoolCreate = OCIConnectionPoolCreate;
    g_ora8API.OCIConnectionPoolDestroy = OCIConnectionPoolDestroy;
}

static void ResetAPI()
{
    g_ora8API.connecionPools.Close(g_ora8API);
    if (NULL != g_ora8API.envhp)
    {
        g_ora8API.OCIHandleFree(g_ora8API.envhp, OCI_HTYPE_ENV);
        g_ora8API.envhp = NULL;
    }

	// 8.0.x calls
	g_ora8API.OCIInitialize	= NULL;
	g_ora8API.OCIHandleAlloc	= NULL;
	g_ora8API.OCIHandleFree	= NULL;
	g_ora8API.OCIDescriptorAlloc	= NULL;
	g_ora8API.OCIDescriptorFree	= NULL;
	g_ora8API.OCIEnvInit	= NULL;
	g_ora8API.OCIServerAttach	= NULL;
	g_ora8API.OCIServerDetach	= NULL;
	g_ora8API.OCISessionBegin	= NULL;
	g_ora8API.OCISessionEnd	= NULL;
	g_ora8API.OCILogon	= NULL;
	g_ora8API.OCILogoff	= NULL;
	g_ora8API.OCIPasswordChange	= NULL;
	g_ora8API.OCIStmtPrepare	= NULL;
	g_ora8API.OCIBindByPos	= NULL;
	g_ora8API.OCIBindByName	= NULL;
	g_ora8API.OCIBindObject	= NULL;
	g_ora8API.OCIBindDynamic	= NULL;
	g_ora8API.OCIBindArrayOfStruct	= NULL;
	g_ora8API.OCIStmtGetPieceInfo	= NULL;
	g_ora8API.OCIStmtSetPieceInfo	= NULL;
	g_ora8API.OCIStmtExecute	= NULL;
	g_ora8API.OCIDefineByPos	= NULL;
	g_ora8API.OCIDefineObject	= NULL;
	g_ora8API.OCIDefineDynamic	= NULL;
	g_ora8API.OCIDefineArrayOfStruct	= NULL;
	g_ora8API.OCIStmtFetch	= NULL;
	g_ora8API.OCIStmtGetBindInfo	= NULL;
	g_ora8API.OCIDescribeAny	= NULL;
	g_ora8API.OCIParamGet	= NULL;
	g_ora8API.OCIParamSet	= NULL;
	g_ora8API.OCITransStart	= NULL;
	g_ora8API.OCITransDetach	= NULL;
	g_ora8API.OCITransCommit	= NULL;
	g_ora8API.OCITransRollback	= NULL;
	g_ora8API.OCITransPrepare	= NULL;
	g_ora8API.OCITransForget	= NULL;
	g_ora8API.OCIErrorGet	= NULL;
	g_ora8API.OCILobAppend	= NULL;
	g_ora8API.OCILobAssign	= NULL;
	g_ora8API.OCILobCharSetForm	= NULL;
	g_ora8API.OCILobCharSetId	= NULL;
	g_ora8API.OCILobCopy	= NULL;
	g_ora8API.OCILobDisableBuffering	= NULL;
	g_ora8API.OCILobEnableBuffering	= NULL;
	g_ora8API.OCILobErase	= NULL;
	g_ora8API.OCILobFileClose	= NULL;
	g_ora8API.OCILobFileCloseAll	= NULL;
	g_ora8API.OCILobFileExists	= NULL;
	g_ora8API.OCILobFileGetName	= NULL;
	g_ora8API.OCILobFileIsOpen	= NULL;
	g_ora8API.OCILobFileOpen	= NULL;
	g_ora8API.OCILobFileSetName	= NULL;
	g_ora8API.OCILobFlushBuffer	= NULL;
	g_ora8API.OCILobGetLength	= NULL;
	g_ora8API.OCILobIsEqual	= NULL;
	g_ora8API.OCILobLoadFromFile	= NULL;
	g_ora8API.OCILobLocatorIsInit	= NULL;
	g_ora8API.OCILobRead	= NULL;
	g_ora8API.OCILobRead2	= NULL;
	g_ora8API.OCILobTrim	= NULL;
	g_ora8API.OCILobWrite	= NULL;
	g_ora8API.OCILobWrite2	= NULL;
	g_ora8API.OCIBreak	= NULL;
	g_ora8API.OCIReset	= NULL;
	g_ora8API.OCIServerVersion	= NULL;
	g_ora8API.OCIAttrGet	= NULL;
	g_ora8API.OCIAttrSet	= NULL;
	g_ora8API.OCISvcCtxToLda	= NULL;
	g_ora8API.OCILdaToSvcCtx	= NULL;
	g_ora8API.OCIResultSetToStmt	= NULL;

	// 8.1.x (8i) calls
	g_ora8API.OCIEnvCreate	= NULL;
	g_ora8API.OCIEnvNlsCreate = NULL;
	g_ora8API.OCIDurationBegin	= NULL;
	g_ora8API.OCIDurationEnd	= NULL;
	g_ora8API.OCILobCreateTemporary	= NULL;
	g_ora8API.OCILobFreeTemporary	= NULL;
	g_ora8API.OCILobIsTemporary	= NULL;

	g_ora8API.OCIAQEnq	= NULL;
	g_ora8API.OCIAQDeq	= NULL;
	g_ora8API.OCIAQListen	= NULL;
	g_ora8API.OCISubscriptionRegister	= NULL;
	g_ora8API.OCISubscriptionPost	= NULL;
	g_ora8API.OCISubscriptionUnRegister	= NULL;
	g_ora8API.OCISubscriptionDisable	= NULL;
	g_ora8API.OCISubscriptionEnable	= NULL;
	
	g_ora8API.OCIDateTimeConstruct = NULL;
	g_ora8API.OCIDateTimeGetDate = NULL;
	g_ora8API.OCIDateTimeGetTime = NULL;
    g_ora8API.OCIDateTimeGetTimeZoneOffset = NULL;

	g_ora8API.OCINlsNumericInfoGet = NULL;

	g_ora8API.OCIStmtFetch2 = NULL;

    g_ora8API.OCIConnectionPoolCreate = NULL;
    g_ora8API.OCIConnectionPoolDestroy = NULL;
}

void AddORA8Support(const SAConnection * /*pCon*/)
{
	SACriticalSectionScope scope(&ora8LoaderMutex);

	if(!g_nORA8DLLRefs)
	{
		LoadAPI();

		if(g_ora8API.OCIEnvCreate == NULL)	// use 8.0.x method of initialization
			g_ora8API.OCIInitialize(OCI_THREADED | OCI_OBJECT, NULL, NULL, NULL, NULL);
	}

	++g_nORA8DLLRefs;
}

void ReleaseORA8Support()
{
	SACriticalSectionScope scope(&ora8LoaderMutex);

	assert(g_nORA8DLLRefs > 0);
	--g_nORA8DLLRefs;

	if(!g_nORA8DLLRefs)
	{
		ResetAPI();
		g_nORA8DLLVersionLoaded = 0l;
	}
}

