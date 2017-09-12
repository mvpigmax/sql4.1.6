// infAPI.cpp
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <infAPI.h>

static const char *g_sInfDLLNames = "libifcli" SHARED_OBJ_EXT ":iclit09a" SHARED_OBJ_EXT ":iclit09b" SHARED_OBJ_EXT;
static void* g_hInfDLL = NULL;
long g_nInfDLLVersionLoaded = 0l;
static long g_nInfDLLRefs = 0l;

#ifdef SA_UNICODE
#define API_FN_SUFFIX	"W"
#else
#define API_FN_SUFFIX	""
#endif

static SAMutex infLoaderMutex;

// API definitions
infAPI g_infAPI;

infAPI::infAPI()
{
	SQLAllocConnect = NULL;		// 1.0
	SQLAllocEnv = NULL;			// 1.0
	SQLAllocHandle = NULL;		// 3.0
	SQLAllocStmt = NULL;		// 1.0
	SQLBindCol = NULL;			// 1.0
	SQLBindParameter = NULL;	// 2.0
	SQLBrowseConnect = NULL;	// 1.0
	SQLBulkOperations = NULL;	// 3.0
	SQLCancel = NULL;			// 1.0
	SQLCloseCursor = NULL;		// 3.0
	SQLColAttribute = NULL;		// 3,0
	SQLColAttributes = NULL;	// 1.0
	SQLColumnPrivileges = NULL;	// 1.0
	SQLColumns = NULL;			// 1.0
	SQLConnect = NULL;			// 1.0
	SQLCopyDesc = NULL;			// 3.0
	SQLDataSources = NULL;		// 1.0
	SQLDescribeCol = NULL;		// 1.0
	SQLDescribeParam = NULL;	// 1.0
	SQLDisconnect = NULL;		// 1.0
	SQLDriverConnect = NULL;	// 1.0
	SQLDrivers = NULL;			// 2.0
	SQLEndTran = NULL;			// 3.0
	SQLError = NULL;			// 1.0
	SQLExecDirect = NULL;		// 1.0
	SQLExecute = NULL;			// 1.0
	SQLExtendedFetch = NULL;	// 1.0
	SQLFetch = NULL;			// 1.0
	SQLFetchScroll = NULL;		// 1.0
	SQLForeignKeys = NULL;		// 1.0
	SQLFreeConnect = NULL;		// 1.0
	SQLFreeEnv = NULL;			// 1.0
	SQLFreeHandle = NULL;		// 3.0
	SQLFreeStmt = NULL;			// 1.0
	SQLGetConnectAttr = NULL;	// 3.0
	SQLGetConnectOption = NULL;	// 1.0
	SQLGetCursorName = NULL;	// 1.0
	SQLGetData = NULL;			// 1.0
	SQLGetDescField = NULL;		// 3.0
	SQLGetDescRec = NULL;		// 3.0
	SQLGetDiagField = NULL;		// 3.0
	SQLGetDiagRec = NULL;		// 3.0
	SQLGetEnvAttr = NULL;		// 3.0
	SQLGetFunctions = NULL;		// 1.0
	SQLGetInfo = NULL;			// 1.0
	SQLGetStmtAttr = NULL;		// 3.0
	SQLGetStmtOption = NULL;	// 1.0
	SQLGetTypeInfo = NULL;		// 1.0
	SQLMoreResults = NULL;		// 1.0
	SQLNativeSql = NULL;		// 1.0
	SQLNumParams = NULL;		// 1.0
	SQLNumResultCols = NULL;	// 1.0
	SQLParamData = NULL;		// 1.0
	SQLParamOptions = NULL;		// 1.0
	SQLPrepare = NULL;			// 1.0
	SQLPrimaryKeys = NULL;		// 1.0
	SQLProcedureColumns = NULL;	// 1.0
	SQLProcedures = NULL;		// 1.0
	SQLPutData = NULL;			// 1.0
	SQLRowCount = NULL;			// 1.0
	SQLSetConnectAttr = NULL;	// 3.0
	SQLSetConnectOption = NULL;	// 1.0
	SQLSetCursorName = NULL;	// 1.0
	SQLSetDescField = NULL;		// 3.0
	SQLSetDescRec = NULL;		// 3.0
	SQLSetEnvAttr = NULL;		// 3.0
	SQLSetParam = NULL;			// 1.0
	SQLSetPos = NULL;			// 1.0
	SQLSetScrollOptions = NULL;	// 1.0
	SQLSetStmtAttr = NULL;		// 3.0
	SQLSetStmtOption = NULL;	// 1.0
	SQLSpecialColumns = NULL;	// 1.0
	SQLStatistics = NULL;		// 1.0
	SQLTablePrivileges = NULL;	// 1.0
	SQLTables = NULL;			// 1.0
	SQLTransact = NULL;			// 1.0
}

infConnectionHandles::infConnectionHandles()
{
	m_hevn = NULL;
	m_hdbc = NULL;
}

infCommandHandles::infCommandHandles()
{
	m_hstmt = NULL;
}

static void LoadAPI()
{
	g_infAPI.SQLAllocConnect		= (SQLAllocConnect_t)dlsym(g_hInfDLL, "SQLAllocConnect");		//assert(g_infAPI.SQLAllocConnect != NULL);
	g_infAPI.SQLAllocEnv			= (SQLAllocEnv_t)dlsym(g_hInfDLL, "SQLAllocEnv");				//assert(g_infAPI.SQLAllocEnv != NULL);
	g_infAPI.SQLAllocHandle			= (SQLAllocHandle_t)dlsym(g_hInfDLL, "SQLAllocHandle");			//assert(g_infAPI.SQLAllocHandle != NULL);
	g_infAPI.SQLAllocStmt			= (SQLAllocStmt_t)dlsym(g_hInfDLL, "SQLAllocStmt");				//assert(g_infAPI.SQLAllocStmt != NULL);
	g_infAPI.SQLBindCol				= (SQLBindCol_t)dlsym(g_hInfDLL, "SQLBindCol");					assert(g_infAPI.SQLBindCol != NULL);
	g_infAPI.SQLBindParameter		= (SQLBindParameter_t)dlsym(g_hInfDLL, "SQLBindParameter");		assert(g_infAPI.SQLBindParameter != NULL);
	g_infAPI.SQLBrowseConnect		= (SQLBrowseConnect_t)dlsym(g_hInfDLL, "SQLBrowseConnect" API_FN_SUFFIX);		assert(g_infAPI.SQLBrowseConnect != NULL);
	g_infAPI.SQLBulkOperations		= (SQLBulkOperations_t)dlsym(g_hInfDLL, "SQLBulkOperations");	//assert(g_infAPI.SQLBulkOperations != NULL);
	g_infAPI.SQLCancel				= (SQLCancel_t)dlsym(g_hInfDLL, "SQLCancel");					assert(g_infAPI.SQLCancel != NULL);
	g_infAPI.SQLCloseCursor			= (SQLCloseCursor_t)dlsym(g_hInfDLL, "SQLCloseCursor");			//assert(g_infAPI.SQLCloseCursor != NULL);
	g_infAPI.SQLColAttribute		= (SQLColAttribute_t)dlsym(g_hInfDLL, "SQLColAttribute" API_FN_SUFFIX);		//assert(g_infAPI.SQLColAttribute != NULL);
	g_infAPI.SQLColAttributes		= (SQLColAttributes_t)dlsym(g_hInfDLL, "SQLColAttributes" API_FN_SUFFIX);		//assert(g_infAPI.SQLColAttributes != NULL);
	g_infAPI.SQLColumnPrivileges	= (SQLColumnPrivileges_t)dlsym(g_hInfDLL, "SQLColumnPrivileges" API_FN_SUFFIX);assert(g_infAPI.SQLColumnPrivileges != NULL);
	g_infAPI.SQLColumns				= (SQLColumns_t)dlsym(g_hInfDLL, "SQLColumns" API_FN_SUFFIX);					assert(g_infAPI.SQLColumns != NULL);
	g_infAPI.SQLConnect				= (SQLConnect_t)dlsym(g_hInfDLL, "SQLConnect" API_FN_SUFFIX);					assert(g_infAPI.SQLConnect != NULL);
	g_infAPI.SQLCopyDesc			= (SQLCopyDesc_t)dlsym(g_hInfDLL, "SQLCopyDesc");				//assert(g_infAPI.SQLCopyDesc != NULL);
	g_infAPI.SQLDataSources			= (SQLDataSources_t)dlsym(g_hInfDLL, "SQLDataSources" API_FN_SUFFIX);			//assert(g_infAPI.SQLDataSources != NULL);
	g_infAPI.SQLDescribeCol			= (SQLDescribeCol_t)dlsym(g_hInfDLL, "SQLDescribeCol" API_FN_SUFFIX);			assert(g_infAPI.SQLDescribeCol != NULL);
	g_infAPI.SQLDescribeParam		= (SQLDescribeParam_t)dlsym(g_hInfDLL, "SQLDescribeParam");		assert(g_infAPI.SQLDescribeParam != NULL);
	g_infAPI.SQLDisconnect			= (SQLDisconnect_t)dlsym(g_hInfDLL, "SQLDisconnect");			assert(g_infAPI.SQLDisconnect != NULL);
	g_infAPI.SQLDriverConnect		= (SQLDriverConnect_t)dlsym(g_hInfDLL, "SQLDriverConnect" API_FN_SUFFIX);		assert(g_infAPI.SQLDriverConnect != NULL);
	g_infAPI.SQLDrivers				= (SQLDrivers_t)dlsym(g_hInfDLL, "SQLDrivers" API_FN_SUFFIX);					//assert(g_infAPI.SQLDrivers != NULL);
	g_infAPI.SQLEndTran				= (SQLEndTran_t)dlsym(g_hInfDLL, "SQLEndTran");					//assert(g_infAPI.SQLEndTran != NULL);
	g_infAPI.SQLError				= (SQLError_t)dlsym(g_hInfDLL, "SQLError" API_FN_SUFFIX);						//assert(g_infAPI.SQLError != NULL);
	g_infAPI.SQLExecDirect			= (SQLExecDirect_t)dlsym(g_hInfDLL, "SQLExecDirect" API_FN_SUFFIX);			assert(g_infAPI.SQLExecDirect != NULL);
	g_infAPI.SQLExecute				= (SQLExecute_t)dlsym(g_hInfDLL, "SQLExecute");					assert(g_infAPI.SQLExecute != NULL);
	g_infAPI.SQLExtendedFetch		= (SQLExtendedFetch_t)dlsym(g_hInfDLL, "SQLExtendedFetch");		assert(g_infAPI.SQLExtendedFetch != NULL);
	g_infAPI.SQLFetch				= (SQLFetch_t)dlsym(g_hInfDLL, "SQLFetch");						assert(g_infAPI.SQLFetch != NULL);
	g_infAPI.SQLFetchScroll			= (SQLFetchScroll_t)dlsym(g_hInfDLL, "SQLFetchScroll");			//assert(g_infAPI.SQLFetchScroll != NULL);
	g_infAPI.SQLForeignKeys			= (SQLForeignKeys_t)dlsym(g_hInfDLL, "SQLForeignKeys" API_FN_SUFFIX);			assert(g_infAPI.SQLForeignKeys != NULL);
	g_infAPI.SQLFreeConnect			= (SQLFreeConnect_t)dlsym(g_hInfDLL, "SQLFreeConnect");			//assert(g_infAPI.SQLFreeConnect != NULL);
	g_infAPI.SQLFreeEnv				= (SQLFreeEnv_t)dlsym(g_hInfDLL, "SQLFreeEnv");					//assert(g_infAPI.SQLFreeEnv != NULL);
	g_infAPI.SQLFreeHandle			= (SQLFreeHandle_t)dlsym(g_hInfDLL, "SQLFreeHandle");			//assert(g_infAPI.SQLFreeHandle != NULL);
	g_infAPI.SQLFreeStmt			= (SQLFreeStmt_t)dlsym(g_hInfDLL, "SQLFreeStmt");				assert(g_infAPI.SQLFreeStmt != NULL);
	g_infAPI.SQLGetConnectAttr		= (SQLGetConnectAttr_t)dlsym(g_hInfDLL, "SQLGetConnectAttr" API_FN_SUFFIX);	//assert(g_infAPI.SQLGetConnectAttr != NULL);
	g_infAPI.SQLGetConnectOption	= (SQLGetConnectOption_t)dlsym(g_hInfDLL, "SQLGetConnectOption" API_FN_SUFFIX);//assert(g_infAPI.SQLGetConnectOption != NULL);
	g_infAPI.SQLGetCursorName		= (SQLGetCursorName_t)dlsym(g_hInfDLL, "SQLGetCursorName" API_FN_SUFFIX);		assert(g_infAPI.SQLGetCursorName != NULL);
	g_infAPI.SQLGetData				= (SQLGetData_t)dlsym(g_hInfDLL, "SQLGetData");					assert(g_infAPI.SQLGetData!= NULL);
	g_infAPI.SQLGetDescField		= (SQLGetDescField_t)dlsym(g_hInfDLL, "SQLGetDescField" API_FN_SUFFIX);		//assert(g_infAPI.SQLGetDescField != NULL);
	g_infAPI.SQLGetDescRec			= (SQLGetDescRec_t)dlsym(g_hInfDLL, "SQLGetDescRec" API_FN_SUFFIX);			//assert(g_infAPI.SQLGetDescRec != NULL);
	g_infAPI.SQLGetDiagField		= (SQLGetDiagField_t)dlsym(g_hInfDLL, "SQLGetDiagField" API_FN_SUFFIX);		//assert(g_infAPI.SQLGetDiagField != NULL);
	g_infAPI.SQLGetDiagRec			= (SQLGetDiagRec_t)dlsym(g_hInfDLL, "SQLGetDiagRec" API_FN_SUFFIX);			//assert(g_infAPI.SQLGetDiagRec != NULL);
	g_infAPI.SQLGetEnvAttr			= (SQLGetEnvAttr_t)dlsym(g_hInfDLL, "SQLGetEnvAttr");			//assert(g_infAPI.SQLGetEnvAttr != NULL);
	g_infAPI.SQLGetFunctions		= (SQLGetFunctions_t)dlsym(g_hInfDLL, "SQLGetFunctions");		assert(g_infAPI.SQLGetFunctions != NULL);
	g_infAPI.SQLGetInfo				= (SQLGetInfo_t)dlsym(g_hInfDLL, "SQLGetInfo" API_FN_SUFFIX);					assert(g_infAPI.SQLGetInfo != NULL);
	g_infAPI.SQLGetStmtAttr			= (SQLGetStmtAttr_t)dlsym(g_hInfDLL, "SQLGetStmtAttr" API_FN_SUFFIX);			//assert(g_infAPI.SQLGetStmtAttr != NULL);
	g_infAPI.SQLGetStmtOption		= (SQLGetStmtOption_t)dlsym(g_hInfDLL, "SQLGetStmtOption");		//assert(g_infAPI.SQLGetStmtOption != NULL);
	g_infAPI.SQLGetTypeInfo			= (SQLGetTypeInfo_t)dlsym(g_hInfDLL, "SQLGetTypeInfo");			assert(g_infAPI.SQLGetTypeInfo != NULL);
	g_infAPI.SQLMoreResults			= (SQLMoreResults_t)dlsym(g_hInfDLL, "SQLMoreResults");			assert(g_infAPI.SQLMoreResults != NULL);
	g_infAPI.SQLNativeSql			= (SQLNativeSql_t)dlsym(g_hInfDLL, "SQLNativeSql" API_FN_SUFFIX);				assert(g_infAPI.SQLNativeSql != NULL);
	g_infAPI.SQLNumParams			= (SQLNumParams_t)dlsym(g_hInfDLL, "SQLNumParams");				assert(g_infAPI.SQLNumParams != NULL);
	g_infAPI.SQLNumResultCols		= (SQLNumResultCols_t)dlsym(g_hInfDLL, "SQLNumResultCols");		assert(g_infAPI.SQLNumResultCols != NULL);
	g_infAPI.SQLParamData			= (SQLParamData_t)dlsym(g_hInfDLL, "SQLParamData");				assert(g_infAPI.SQLParamData != NULL);
	g_infAPI.SQLParamOptions		= (SQLParamOptions_t)dlsym(g_hInfDLL, "SQLParamOptions");		//assert(g_infAPI.SQLParamOptions != NULL);
	g_infAPI.SQLPrepare				= (SQLPrepare_t)dlsym(g_hInfDLL, "SQLPrepare" API_FN_SUFFIX);					assert(g_infAPI.SQLPrepare != NULL);
	g_infAPI.SQLPrimaryKeys			= (SQLPrimaryKeys_t)dlsym(g_hInfDLL, "SQLPrimaryKeys" API_FN_SUFFIX);			assert(g_infAPI.SQLPrimaryKeys != NULL);
	g_infAPI.SQLProcedureColumns	= (SQLProcedureColumns_t)dlsym(g_hInfDLL, "SQLProcedureColumns" API_FN_SUFFIX);assert(g_infAPI.SQLProcedureColumns != NULL);
	g_infAPI.SQLProcedures			= (SQLProcedures_t)dlsym(g_hInfDLL, "SQLProcedures" API_FN_SUFFIX);			assert(g_infAPI.SQLProcedures != NULL);
	g_infAPI.SQLPutData				= (SQLPutData_t)dlsym(g_hInfDLL, "SQLPutData");					assert(g_infAPI.SQLPutData != NULL);
	g_infAPI.SQLRowCount			= (SQLRowCount_t)dlsym(g_hInfDLL, "SQLRowCount");				assert(g_infAPI.SQLRowCount != NULL);
	g_infAPI.SQLSetConnectAttr		= (SQLSetConnectAttr_t)dlsym(g_hInfDLL, "SQLSetConnectAttr" API_FN_SUFFIX);	//assert(g_infAPI.SQLSetConnectAttr != NULL);
	g_infAPI.SQLSetConnectOption	= (SQLSetConnectOption_t)dlsym(g_hInfDLL, "SQLSetConnectOption" API_FN_SUFFIX);//assert(g_infAPI.SQLSetConnectOption != NULL);
	g_infAPI.SQLSetCursorName		= (SQLSetCursorName_t)dlsym(g_hInfDLL, "SQLSetCursorName" API_FN_SUFFIX);		assert(g_infAPI.SQLSetCursorName != NULL);
	g_infAPI.SQLSetDescField		= (SQLSetDescField_t)dlsym(g_hInfDLL, "SQLSetDescField" API_FN_SUFFIX);		//assert(g_infAPI.SQLSetDescField != NULL);
	g_infAPI.SQLSetDescRec			= (SQLSetDescRec_t)dlsym(g_hInfDLL, "SQLSetDescRec");			//assert(g_infAPI.SQLSetDescRec != NULL);
	g_infAPI.SQLSetEnvAttr			= (SQLSetEnvAttr_t)dlsym(g_hInfDLL, "SQLSetEnvAttr");			//assert(g_infAPI.SQLSetEnvAttr != NULL);
	g_infAPI.SQLSetParam			= (SQLSetParam_t)dlsym(g_hInfDLL, "SQLSetParam");				//assert(g_infAPI.SQLSetParam != NULL);
	g_infAPI.SQLSetPos				= (SQLSetPos_t)dlsym(g_hInfDLL, "SQLSetPos");					assert(g_infAPI.SQLSetPos != NULL);
	g_infAPI.SQLSetScrollOptions	= (SQLSetScrollOptions_t)dlsym(g_hInfDLL, "SQLSetScrollOptions");assert(g_infAPI.SQLSetScrollOptions != NULL);
	g_infAPI.SQLSetStmtAttr			= (SQLSetStmtAttr_t)dlsym(g_hInfDLL, "SQLSetStmtAttr" API_FN_SUFFIX);			//assert(g_infAPI.SQLSetStmtAttr != NULL);
	g_infAPI.SQLSetStmtOption		= (SQLSetStmtOption_t)dlsym(g_hInfDLL, "SQLSetStmtOption");		//assert(g_infAPI.SQLSetStmtOption != NULL);
	g_infAPI.SQLSpecialColumns		= (SQLSpecialColumns_t)dlsym(g_hInfDLL, "SQLSpecialColumns" API_FN_SUFFIX);	assert(g_infAPI.SQLSpecialColumns != NULL);
	g_infAPI.SQLStatistics			= (SQLStatistics_t)dlsym(g_hInfDLL, "SQLStatistics" API_FN_SUFFIX);			assert(g_infAPI.SQLStatistics != NULL);
	g_infAPI.SQLTablePrivileges		= (SQLTablePrivileges_t)dlsym(g_hInfDLL, "SQLTablePrivileges" API_FN_SUFFIX);	assert(g_infAPI.SQLTablePrivileges != NULL);
	g_infAPI.SQLTables				= (SQLTables_t)dlsym(g_hInfDLL, "SQLTables" API_FN_SUFFIX);					assert(g_infAPI.SQLTables != NULL);
	g_infAPI.SQLTransact			= (SQLTransact_t)dlsym(g_hInfDLL, "SQLTransact");				//assert(g_infAPI.SQLTransact != NULL);
}

static void ResetAPI()
{
	g_infAPI.SQLAllocConnect = NULL;		// 1.0
	g_infAPI.SQLAllocEnv = NULL;			// 1.0
	g_infAPI.SQLAllocHandle = NULL;			// 3.0
	g_infAPI.SQLAllocStmt = NULL;			// 1.0
	g_infAPI.SQLBindCol = NULL;				// 1.0
	g_infAPI.SQLBindParameter = NULL;		// 2.0
	g_infAPI.SQLBrowseConnect = NULL;		// 1.0
	g_infAPI.SQLBulkOperations = NULL;		// 3.0
	g_infAPI.SQLCancel = NULL;				// 1.0
	g_infAPI.SQLCloseCursor = NULL;			// 3.0
	g_infAPI.SQLColAttribute = NULL;		// 3,0
	g_infAPI.SQLColAttributes = NULL;		// 1.0
	g_infAPI.SQLColumnPrivileges = NULL;	// 1.0
	g_infAPI.SQLColumns = NULL;				// 1.0
	g_infAPI.SQLConnect = NULL;				// 1.0
	g_infAPI.SQLCopyDesc = NULL;			// 3.0
	g_infAPI.SQLDataSources = NULL;			// 1.0
	g_infAPI.SQLDescribeCol = NULL;			// 1.0
	g_infAPI.SQLDescribeParam = NULL;		// 1.0
	g_infAPI.SQLDisconnect = NULL;			// 1.0
	g_infAPI.SQLDriverConnect = NULL;		// 1.0
	g_infAPI.SQLDrivers = NULL;				// 2.0
	g_infAPI.SQLEndTran = NULL;				// 3.0
	g_infAPI.SQLError = NULL;				// 1.0
	g_infAPI.SQLExecDirect = NULL;			// 1.0
	g_infAPI.SQLExecute = NULL;				// 1.0
	g_infAPI.SQLExtendedFetch = NULL;		// 1.0
	g_infAPI.SQLFetch = NULL;				// 1.0
	g_infAPI.SQLFetchScroll = NULL;			// 1.0
	g_infAPI.SQLForeignKeys = NULL;			// 1.0
	g_infAPI.SQLFreeConnect = NULL;			// 1.0
	g_infAPI.SQLFreeEnv = NULL;				// 1.0
	g_infAPI.SQLFreeHandle = NULL;			// 3.0
	g_infAPI.SQLFreeStmt = NULL;			// 1.0
	g_infAPI.SQLGetConnectAttr = NULL;		// 3.0
	g_infAPI.SQLGetConnectOption = NULL;	// 1.0
	g_infAPI.SQLGetCursorName = NULL;		// 1.0
	g_infAPI.SQLGetData = NULL;				// 1.0
	g_infAPI.SQLGetDescField = NULL;		// 3.0
	g_infAPI.SQLGetDescRec = NULL;			// 3.0
	g_infAPI.SQLGetDiagField = NULL;		// 3.0
	g_infAPI.SQLGetDiagRec = NULL;			// 3.0
	g_infAPI.SQLGetEnvAttr = NULL;			// 3.0
	g_infAPI.SQLGetFunctions = NULL;		// 1.0
	g_infAPI.SQLGetInfo = NULL;				// 1.0
	g_infAPI.SQLGetStmtAttr = NULL;			// 3.0
	g_infAPI.SQLGetStmtOption = NULL;		// 1.0
	g_infAPI.SQLGetTypeInfo = NULL;			// 1.0
	g_infAPI.SQLMoreResults = NULL;			// 1.0
	g_infAPI.SQLNativeSql = NULL;			// 1.0
	g_infAPI.SQLNumParams = NULL;			// 1.0
	g_infAPI.SQLNumResultCols = NULL;		// 1.0
	g_infAPI.SQLParamData = NULL;			// 1.0
	g_infAPI.SQLParamOptions = NULL;		// 1.0
	g_infAPI.SQLPrepare = NULL;				// 1.0
	g_infAPI.SQLPrimaryKeys = NULL;			// 1.0
	g_infAPI.SQLProcedureColumns = NULL;	// 1.0
	g_infAPI.SQLProcedures = NULL;			// 1.0
	g_infAPI.SQLPutData = NULL;				// 1.0
	g_infAPI.SQLRowCount = NULL;			// 1.0
	g_infAPI.SQLSetConnectAttr = NULL;		// 3.0
	g_infAPI.SQLSetConnectOption = NULL;	// 1.0
	g_infAPI.SQLSetCursorName = NULL;		// 1.0
	g_infAPI.SQLSetDescField = NULL;		// 3.0
	g_infAPI.SQLSetDescRec = NULL;			// 3.0
	g_infAPI.SQLSetEnvAttr = NULL;			// 3.0
	g_infAPI.SQLSetParam = NULL;			// 1.0
	g_infAPI.SQLSetPos = NULL;				// 1.0
	g_infAPI.SQLSetScrollOptions = NULL;	// 1.0
	g_infAPI.SQLSetStmtAttr = NULL;			// 3.0
	g_infAPI.SQLSetStmtOption = NULL;		// 1.0
	g_infAPI.SQLSpecialColumns = NULL;		// 1.0
	g_infAPI.SQLStatistics = NULL;			// 1.0
	g_infAPI.SQLTablePrivileges = NULL;		// 1.0
	g_infAPI.SQLTables = NULL;				// 1.0
	g_infAPI.SQLTransact = NULL;			// 1.0
}

void AddInfSupport(const SAConnection * pCon)
{
	SACriticalSectionScope scope(&infLoaderMutex);

	if( ! g_hInfDLL )
	{
		// load Informix API library
		SAString sErrorMessage, sLibName, sLibsList = pCon->Option(_TSA("INFCLI.LIBS"));
		if( sLibsList.IsEmpty() )
			sLibsList = g_sInfDLLNames;

		g_hInfDLL = SALoadLibraryFromList(sLibsList, sErrorMessage, sLibName, RTLD_LAZY|RTLD_GLOBAL);
		if( ! g_hInfDLL )
			throw SAException(SA_Library_Error, SA_Library_Error_LoadLibraryFails, -1, IDS_LOAD_LIBRARY_FAILS, (const SAChar*)sErrorMessage);

		g_nInfDLLVersionLoaded = 0l;

		LoadAPI();
	}

	if( SAGlobals::UnloadAPI() )
		g_nInfDLLRefs++;
	else
		g_nInfDLLRefs = 1;
}

void ReleaseInfSupport()
{
	SACriticalSectionScope scope(&infLoaderMutex);

	assert(g_nInfDLLRefs > 0);
	g_nInfDLLRefs--;
	if(!g_nInfDLLRefs)
	{
		g_nInfDLLVersionLoaded = 0l;
		ResetAPI();

		::dlclose(g_hInfDLL);
		g_hInfDLL = NULL;
	}
}
