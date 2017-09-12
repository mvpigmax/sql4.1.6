// ora8API_linux.cpp
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <oraAPI.h>

static const char *g_sORA8DLLNames = "libclntsh" SHARED_OBJ_EXT;
static void *g_hORA8DLL = NULL;
long g_nORA8DLLVersionLoaded = 0l;
static long g_nORA8DLLRefs = 0l;

static SAMutex oraLoaderMutex;

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
	g_ora8API.OCIInitialize	= (OCIInitialize_t)dlsym(g_hORA8DLL, "OCIInitialize");	assert(g_ora8API.OCIInitialize != NULL);
	g_ora8API.OCIHandleAlloc	= (OCIHandleAlloc_t)dlsym(g_hORA8DLL, "OCIHandleAlloc");	assert(g_ora8API.OCIHandleAlloc != NULL);
	g_ora8API.OCIHandleFree	= (OCIHandleFree_t)dlsym(g_hORA8DLL, "OCIHandleFree");	assert(g_ora8API.OCIHandleFree != NULL);
	g_ora8API.OCIDescriptorAlloc	= (OCIDescriptorAlloc_t)dlsym(g_hORA8DLL, "OCIDescriptorAlloc");	assert(g_ora8API.OCIDescriptorAlloc != NULL);
	g_ora8API.OCIDescriptorFree	= (OCIDescriptorFree_t)dlsym(g_hORA8DLL, "OCIDescriptorFree");	assert(g_ora8API.OCIDescriptorFree != NULL);
	g_ora8API.OCIEnvInit	= (OCIEnvInit_t)dlsym(g_hORA8DLL, "OCIEnvInit");	assert(g_ora8API.OCIEnvInit != NULL);
	g_ora8API.OCIServerAttach	= (OCIServerAttach_t)dlsym(g_hORA8DLL, "OCIServerAttach");	assert(g_ora8API.OCIServerAttach != NULL);
	g_ora8API.OCIServerDetach	= (OCIServerDetach_t)dlsym(g_hORA8DLL, "OCIServerDetach");	assert(g_ora8API.OCIServerDetach != NULL);
	g_ora8API.OCISessionBegin	= (OCISessionBegin_t)dlsym(g_hORA8DLL, "OCISessionBegin");	assert(g_ora8API.OCISessionBegin != NULL);
	g_ora8API.OCISessionEnd	= (OCISessionEnd_t)dlsym(g_hORA8DLL, "OCISessionEnd");	assert(g_ora8API.OCISessionEnd != NULL);
	g_ora8API.OCILogon	= (OCILogon_t)dlsym(g_hORA8DLL, "OCILogon");	assert(g_ora8API.OCILogon != NULL);
	g_ora8API.OCILogoff	= (OCILogoff_t)dlsym(g_hORA8DLL, "OCILogoff");	assert(g_ora8API.OCILogoff != NULL);
	g_ora8API.OCIPasswordChange	= (OCIPasswordChange_t)dlsym(g_hORA8DLL, "OCIPasswordChange");	assert(g_ora8API.OCIPasswordChange != NULL);
	g_ora8API.OCIStmtPrepare	= (OCIStmtPrepare_t)dlsym(g_hORA8DLL, "OCIStmtPrepare");	assert(g_ora8API.OCIStmtPrepare != NULL);
	g_ora8API.OCIBindByPos	= (OCIBindByPos_t)dlsym(g_hORA8DLL, "OCIBindByPos");	assert(g_ora8API.OCIBindByPos != NULL);
	g_ora8API.OCIBindByName	= (OCIBindByName_t)dlsym(g_hORA8DLL, "OCIBindByName");	assert(g_ora8API.OCIBindByName != NULL);
	g_ora8API.OCIBindObject	= (OCIBindObject_t)dlsym(g_hORA8DLL, "OCIBindObject");	assert(g_ora8API.OCIBindObject != NULL);
	g_ora8API.OCIBindDynamic	= (OCIBindDynamic_t)dlsym(g_hORA8DLL, "OCIBindDynamic");	assert(g_ora8API.OCIBindDynamic != NULL);
	g_ora8API.OCIBindArrayOfStruct	= (OCIBindArrayOfStruct_t)dlsym(g_hORA8DLL, "OCIBindArrayOfStruct");	assert(g_ora8API.OCIBindArrayOfStruct != NULL);
	g_ora8API.OCIStmtGetPieceInfo	= (OCIStmtGetPieceInfo_t)dlsym(g_hORA8DLL, "OCIStmtGetPieceInfo");	assert(g_ora8API.OCIStmtGetPieceInfo != NULL);
	g_ora8API.OCIStmtSetPieceInfo	= (OCIStmtSetPieceInfo_t)dlsym(g_hORA8DLL, "OCIStmtSetPieceInfo");	assert(g_ora8API.OCIStmtSetPieceInfo != NULL);
	g_ora8API.OCIStmtExecute	= (OCIStmtExecute_t)dlsym(g_hORA8DLL, "OCIStmtExecute");	assert(g_ora8API.OCIStmtExecute != NULL);
	g_ora8API.OCIDefineByPos	= (OCIDefineByPos_t)dlsym(g_hORA8DLL, "OCIDefineByPos");	assert(g_ora8API.OCIDefineByPos != NULL);
	g_ora8API.OCIDefineObject	= (OCIDefineObject_t)dlsym(g_hORA8DLL, "OCIDefineObject");	assert(g_ora8API.OCIDefineObject != NULL);
	g_ora8API.OCIDefineDynamic	= (OCIDefineDynamic_t)dlsym(g_hORA8DLL, "OCIDefineDynamic");	assert(g_ora8API.OCIDefineDynamic != NULL);
	g_ora8API.OCIDefineArrayOfStruct	= (OCIDefineArrayOfStruct_t)dlsym(g_hORA8DLL, "OCIDefineArrayOfStruct");	assert(g_ora8API.OCIDefineArrayOfStruct != NULL);
	g_ora8API.OCIStmtFetch	= (OCIStmtFetch_t)dlsym(g_hORA8DLL, "OCIStmtFetch");	assert(g_ora8API.OCIStmtFetch != NULL);
	g_ora8API.OCIStmtGetBindInfo	= (OCIStmtGetBindInfo_t)dlsym(g_hORA8DLL, "OCIStmtGetBindInfo");	assert(g_ora8API.OCIStmtGetBindInfo != NULL);
	g_ora8API.OCIDescribeAny	= (OCIDescribeAny_t)dlsym(g_hORA8DLL, "OCIDescribeAny");	assert(g_ora8API.OCIDescribeAny != NULL);
	g_ora8API.OCIParamGet	= (OCIParamGet_t)dlsym(g_hORA8DLL, "OCIParamGet");	assert(g_ora8API.OCIParamGet != NULL);
	g_ora8API.OCIParamSet	= (OCIParamSet_t)dlsym(g_hORA8DLL, "OCIParamSet");	assert(g_ora8API.OCIParamSet != NULL);
	g_ora8API.OCITransStart	= (OCITransStart_t)dlsym(g_hORA8DLL, "OCITransStart");	assert(g_ora8API.OCITransStart != NULL);
	g_ora8API.OCITransDetach	= (OCITransDetach_t)dlsym(g_hORA8DLL, "OCITransDetach");	assert(g_ora8API.OCITransDetach != NULL);
	g_ora8API.OCITransCommit	= (OCITransCommit_t)dlsym(g_hORA8DLL, "OCITransCommit");	assert(g_ora8API.OCITransCommit != NULL);
	g_ora8API.OCITransRollback	= (OCITransRollback_t)dlsym(g_hORA8DLL, "OCITransRollback");	assert(g_ora8API.OCITransRollback != NULL);
	g_ora8API.OCITransPrepare	= (OCITransPrepare_t)dlsym(g_hORA8DLL, "OCITransPrepare");	assert(g_ora8API.OCITransPrepare != NULL);
	g_ora8API.OCITransForget	= (OCITransForget_t)dlsym(g_hORA8DLL, "OCITransForget");	assert(g_ora8API.OCITransForget != NULL);
	g_ora8API.OCIErrorGet	= (OCIErrorGet_t)dlsym(g_hORA8DLL, "OCIErrorGet");	assert(g_ora8API.OCIErrorGet != NULL);
	g_ora8API.OCILobAppend	= (OCILobAppend_t)dlsym(g_hORA8DLL, "OCILobAppend");	assert(g_ora8API.OCILobAppend != NULL);
	g_ora8API.OCILobAssign	= (OCILobAssign_t)dlsym(g_hORA8DLL, "OCILobAssign");	assert(g_ora8API.OCILobAssign != NULL);
	g_ora8API.OCILobCharSetForm	= (OCILobCharSetForm_t)dlsym(g_hORA8DLL, "OCILobCharSetForm");	assert(g_ora8API.OCILobCharSetForm != NULL);
	g_ora8API.OCILobCharSetId	= (OCILobCharSetId_t)dlsym(g_hORA8DLL, "OCILobCharSetId");	assert(g_ora8API.OCILobCharSetId != NULL);
	g_ora8API.OCILobCopy	= (OCILobCopy_t)dlsym(g_hORA8DLL, "OCILobCopy");	assert(g_ora8API.OCILobCopy != NULL);
	g_ora8API.OCILobDisableBuffering	= (OCILobDisableBuffering_t)dlsym(g_hORA8DLL, "OCILobDisableBuffering");	assert(g_ora8API.OCILobDisableBuffering != NULL);
	g_ora8API.OCILobEnableBuffering	= (OCILobEnableBuffering_t)dlsym(g_hORA8DLL, "OCILobEnableBuffering");	assert(g_ora8API.OCILobEnableBuffering != NULL);
	g_ora8API.OCILobErase	= (OCILobErase_t)dlsym(g_hORA8DLL, "OCILobErase");	assert(g_ora8API.OCILobErase != NULL);
	g_ora8API.OCILobFileClose	= (OCILobFileClose_t)dlsym(g_hORA8DLL, "OCILobFileClose");	assert(g_ora8API.OCILobFileClose != NULL);
	g_ora8API.OCILobFileCloseAll	= (OCILobFileCloseAll_t)dlsym(g_hORA8DLL, "OCILobFileCloseAll");	assert(g_ora8API.OCILobFileCloseAll != NULL);
	g_ora8API.OCILobFileExists	= (OCILobFileExists_t)dlsym(g_hORA8DLL, "OCILobFileExists");	assert(g_ora8API.OCILobFileExists != NULL);
	g_ora8API.OCILobFileGetName	= (OCILobFileGetName_t)dlsym(g_hORA8DLL, "OCILobFileGetName");	assert(g_ora8API.OCILobFileGetName != NULL);
	g_ora8API.OCILobFileIsOpen	= (OCILobFileIsOpen_t)dlsym(g_hORA8DLL, "OCILobFileIsOpen");	assert(g_ora8API.OCILobFileIsOpen != NULL);
	g_ora8API.OCILobFileOpen	= (OCILobFileOpen_t)dlsym(g_hORA8DLL, "OCILobFileOpen");	assert(g_ora8API.OCILobFileOpen != NULL);
	g_ora8API.OCILobFileSetName	= (OCILobFileSetName_t)dlsym(g_hORA8DLL, "OCILobFileSetName");	assert(g_ora8API.OCILobFileSetName != NULL);
	g_ora8API.OCILobFlushBuffer	= (OCILobFlushBuffer_t)dlsym(g_hORA8DLL, "OCILobFlushBuffer");	assert(g_ora8API.OCILobFlushBuffer != NULL);
	g_ora8API.OCILobGetLength	= (OCILobGetLength_t)dlsym(g_hORA8DLL, "OCILobGetLength");	assert(g_ora8API.OCILobGetLength != NULL);
	g_ora8API.OCILobIsEqual	= (OCILobIsEqual_t)dlsym(g_hORA8DLL, "OCILobIsEqual");	assert(g_ora8API.OCILobIsEqual != NULL);
	g_ora8API.OCILobLoadFromFile	= (OCILobLoadFromFile_t)dlsym(g_hORA8DLL, "OCILobLoadFromFile");	assert(g_ora8API.OCILobLoadFromFile != NULL);
	g_ora8API.OCILobLocatorIsInit	= (OCILobLocatorIsInit_t)dlsym(g_hORA8DLL, "OCILobLocatorIsInit");	assert(g_ora8API.OCILobLocatorIsInit != NULL);
	g_ora8API.OCILobRead	= (OCILobRead_t)dlsym(g_hORA8DLL, "OCILobRead");	assert(g_ora8API.OCILobRead != NULL);
	g_ora8API.OCILobRead2	= (OCILobRead2_t)dlsym(g_hORA8DLL, "OCILobRead2");	// assert(g_ora8API.OCILobRead2 != NULL);
	g_ora8API.OCILobTrim	= (OCILobTrim_t)dlsym(g_hORA8DLL, "OCILobTrim");	assert(g_ora8API.OCILobTrim != NULL);
	g_ora8API.OCILobWrite	= (OCILobWrite_t)dlsym(g_hORA8DLL, "OCILobWrite");	assert(g_ora8API.OCILobWrite != NULL);
	g_ora8API.OCILobWrite2	= (OCILobWrite2_t)dlsym(g_hORA8DLL, "OCILobWrite2");	// assert(g_ora8API.OCILobWrite2 != NULL);
	g_ora8API.OCIBreak	= (OCIBreak_t)dlsym(g_hORA8DLL, "OCIBreak");	assert(g_ora8API.OCIBreak != NULL);
	g_ora8API.OCIReset	= (OCIReset_t)dlsym(g_hORA8DLL, "OCIReset");	// assert(g_ora8API.OCIReset != NULL);
	g_ora8API.OCIServerVersion	= (OCIServerVersion_t)dlsym(g_hORA8DLL, "OCIServerVersion");	assert(g_ora8API.OCIServerVersion != NULL);
	g_ora8API.OCIAttrGet	= (OCIAttrGet_t)dlsym(g_hORA8DLL, "OCIAttrGet");	assert(g_ora8API.OCIAttrGet != NULL);
	g_ora8API.OCIAttrSet	= (OCIAttrSet_t)dlsym(g_hORA8DLL, "OCIAttrSet");	assert(g_ora8API.OCIAttrSet != NULL);
	g_ora8API.OCISvcCtxToLda	= (OCISvcCtxToLda_t)dlsym(g_hORA8DLL, "OCISvcCtxToLda");	assert(g_ora8API.OCISvcCtxToLda != NULL);
	g_ora8API.OCILdaToSvcCtx	= (OCILdaToSvcCtx_t)dlsym(g_hORA8DLL, "OCILdaToSvcCtx");	assert(g_ora8API.OCILdaToSvcCtx != NULL);
	g_ora8API.OCIResultSetToStmt	= (OCIResultSetToStmt_t)dlsym(g_hORA8DLL, "OCIResultSetToStmt");	assert(g_ora8API.OCIResultSetToStmt != NULL);

	// 8.1.x (8i) calls
	g_ora8API.OCIEnvCreate	= (OCIEnvCreate_t)dlsym(g_hORA8DLL, "OCIEnvCreate");
	g_ora8API.OCIEnvNlsCreate	= (OCIEnvNlsCreate_t)dlsym(g_hORA8DLL, "OCIEnvNlsCreate");
	g_ora8API.OCIDurationBegin = (OCIDurationBegin_t)dlsym(g_hORA8DLL, "OCIDurationBegin");
	g_ora8API.OCIDurationEnd = (OCIDurationEnd_t)dlsym(g_hORA8DLL, "OCIDurationEnd");
	g_ora8API.OCILobCreateTemporary = (OCILobCreateTemporary_t)dlsym(g_hORA8DLL, "OCILobCreateTemporary");
	g_ora8API.OCILobFreeTemporary = (OCILobFreeTemporary_t)dlsym(g_hORA8DLL, "OCILobFreeTemporary");
	g_ora8API.OCILobIsTemporary = (OCILobIsTemporary_t)dlsym(g_hORA8DLL, "OCILobIsTemporary");

	g_ora8API.OCIAQEnq	= (OCIAQEnq_t)dlsym(g_hORA8DLL, "OCIAQEnq");
	g_ora8API.OCIAQDeq	= (OCIAQDeq_t)dlsym(g_hORA8DLL, "OCIAQDeq");
	g_ora8API.OCIAQListen	= (OCIAQListen_t)dlsym(g_hORA8DLL, "OCIAQListen");
	g_ora8API.OCISubscriptionRegister	= (OCISubscriptionRegister_t)dlsym(g_hORA8DLL, "OCISubscriptionRegister");
	g_ora8API.OCISubscriptionPost	= (OCISubscriptionPost_t)dlsym(g_hORA8DLL, "OCISubscriptionPost");
	g_ora8API.OCISubscriptionUnRegister	= (OCISubscriptionUnRegister_t)dlsym(g_hORA8DLL, "OCISubscriptionUnRegister");
	g_ora8API.OCISubscriptionDisable	= (OCISubscriptionDisable_t)dlsym(g_hORA8DLL, "OCISubscriptionDisable");
	g_ora8API.OCISubscriptionEnable	= (OCISubscriptionEnable_t)dlsym(g_hORA8DLL, "OCISubscriptionEnable");

	g_ora8API.OCIDateTimeConstruct	= (OCIDateTimeConstruct_t)dlsym(g_hORA8DLL, "OCIDateTimeConstruct");
	g_ora8API.OCIDateTimeGetDate = (OCIDateTimeGetDate_t)dlsym(g_hORA8DLL, "OCIDateTimeGetDate");
	g_ora8API.OCIDateTimeGetTime = (OCIDateTimeGetTime_t)dlsym(g_hORA8DLL, "OCIDateTimeGetTime");
    g_ora8API.OCIDateTimeGetTimeZoneOffset = (OCIDateTimeGetTimeZoneOffset_t)dlsym(g_hORA8DLL, "OCIDateTimeGetTimeZoneOffset");

	g_ora8API.OCINlsNumericInfoGet = (OCINlsNumericInfoGet_t)dlsym(g_hORA8DLL, "OCINlsNumericInfoGet");

	g_ora8API.OCIStmtFetch2	= (OCIStmtFetch2_t)dlsym(g_hORA8DLL, "OCIStmtFetch2");

    g_ora8API.OCIConnectionPoolCreate = (OCIConnectionPoolCreate_t)dlsym(g_hORA8DLL, "OCIConnectionPoolCreate");
    g_ora8API.OCIConnectionPoolDestroy = (OCIConnectionPoolDestroy_t)dlsym(g_hORA8DLL, "OCIConnectionPoolDestroy");
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

void AddORA8Support(const SAConnection* pCon)
{
	SACriticalSectionScope scope(&oraLoaderMutex);

	if( ! g_hORA8DLL )
	{
		// load Oracle OCI8 API library
		SAString sErrorMessage, sLibName, sLibsList = pCon->Option(_TSA("OCI8.LIBS"));
		if( sLibsList.IsEmpty() )
			sLibsList = g_sORA8DLLNames;

		// WARNING:
		// adding RTLD_GLOBAL flag may "hang" the application later
		g_hORA8DLL = SALoadLibraryFromList(sLibsList, sErrorMessage, sLibName,
#if defined(SQLAPI_AIX)
			RTLD_LAZY|RTLD_GLOBAL);
#elif defined(SQLAPI_SOLARIS)
			RTLD_NOW|RTLD_LOCAL|RTLD_GROUP);
#elif defined(SQLAPI_LINUX)
			RTLD_LAZY|RTLD_DEEPBIND);
#else
			RTLD_LAZY);
#endif
		if( ! g_hORA8DLL )
			throw SAException(SA_Library_Error, SA_Library_Error_LoadLibraryFails, -1, IDS_LOAD_LIBRARY_FAILS, (const SAChar*)sErrorMessage);

		LoadAPI();

		if( NULL == g_ora8API.OCIEnvCreate ) // use 8.0.x method of initialization
			g_ora8API.OCIInitialize(OCI_THREADED|OCI_OBJECT, NULL, NULL, NULL, NULL);
	}

	if( SAGlobals::UnloadAPI() )
		g_nORA8DLLRefs++;
	else
		g_nORA8DLLRefs = 1;
}

void ReleaseORA8Support()
{
	SACriticalSectionScope scope(&oraLoaderMutex);

	assert(g_nORA8DLLRefs > 0);
	--g_nORA8DLLRefs;
	if(!g_nORA8DLLRefs)
	{
		ResetAPI();

//		dlclose(g_hORA8DLL);
		g_hORA8DLL = NULL;
	}
}
