// odbcAPI.cpp
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <odbcAPI.h>

static const char *g_sODBCDLLNames =
#if defined(SQLAPI_OSX)
	"libiodbc" SHARED_OBJ_EXT ":libiodbc.3" SHARED_OBJ_EXT ":libiodbc.2" SHARED_OBJ_EXT ":libodbc" SHARED_OBJ_EXT;
#else
	"libiodbc" SHARED_OBJ_EXT ":libiodbc" SHARED_OBJ_EXT ".3:libiodbc" SHARED_OBJ_EXT ".2:libodbc" SHARED_OBJ_EXT ":libodbc" SHARED_OBJ_EXT ".1";
#endif
static void *g_hODBCDLL = NULL;
long g_nODBCDLLVersionLoaded = 0l;
static long g_nODBCDLLRefs = 0l;

#ifdef SA_UNICODE
#define API_FN_SUFFIX	"W"
#else
#define API_FN_SUFFIX	""
#endif

static SAMutex odbcLoaderMutex;

// API definitions
odbcAPI g_odbcAPI;

odbcAPI::odbcAPI()
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

odbcConnectionHandles::odbcConnectionHandles()
{
	m_hevn = NULL;
	m_hdbc = NULL;
}

odbcCommandHandles::odbcCommandHandles()
{
	m_hstmt = NULL;
}

static void LoadAPI()
{
	g_odbcAPI.SQLAllocConnect		= (SQLAllocConnect_t)::dlsym(g_hODBCDLL, "SQLAllocConnect");			assert(g_odbcAPI.SQLAllocConnect != NULL);
	g_odbcAPI.SQLAllocEnv			= (SQLAllocEnv_t)::dlsym(g_hODBCDLL, "SQLAllocEnv");					assert(g_odbcAPI.SQLAllocEnv != NULL);
	g_odbcAPI.SQLAllocHandle		= (SQLAllocHandle_t)::dlsym(g_hODBCDLL, "SQLAllocHandle");				assert(g_odbcAPI.SQLAllocHandle != NULL);
	g_odbcAPI.SQLAllocStmt			= (SQLAllocStmt_t)::dlsym(g_hODBCDLL, "SQLAllocStmt");					assert(g_odbcAPI.SQLAllocStmt != NULL);
	g_odbcAPI.SQLBindCol			= (SQLBindCol_t)::dlsym(g_hODBCDLL, "SQLBindCol");						assert(g_odbcAPI.SQLBindCol != NULL);
	g_odbcAPI.SQLBindParameter		= (SQLBindParameter_t)::dlsym(g_hODBCDLL, "SQLBindParameter");			assert(g_odbcAPI.SQLBindParameter != NULL);
	g_odbcAPI.SQLBrowseConnect		= (SQLBrowseConnect_t)::dlsym(g_hODBCDLL, "SQLBrowseConnect" API_FN_SUFFIX);			assert(g_odbcAPI.SQLBrowseConnect != NULL);
	g_odbcAPI.SQLBulkOperations		= (SQLBulkOperations_t)::dlsym(g_hODBCDLL, "SQLBulkOperations");		assert(g_odbcAPI.SQLBulkOperations != NULL);
	g_odbcAPI.SQLCancel				= (SQLCancel_t)::dlsym(g_hODBCDLL, "SQLCancel");						assert(g_odbcAPI.SQLCancel != NULL);
	g_odbcAPI.SQLCloseCursor		= (SQLCloseCursor_t)::dlsym(g_hODBCDLL, "SQLCloseCursor");				assert(g_odbcAPI.SQLCloseCursor != NULL);
	g_odbcAPI.SQLColAttribute		= (SQLColAttribute_t)::dlsym(g_hODBCDLL, "SQLColAttribute" API_FN_SUFFIX);			assert(g_odbcAPI.SQLColAttribute != NULL);
	g_odbcAPI.SQLColAttributes		= (SQLColAttributes_t)::dlsym(g_hODBCDLL, "SQLColAttributes" API_FN_SUFFIX);			assert(g_odbcAPI.SQLColAttributes != NULL);
	g_odbcAPI.SQLColumnPrivileges	= (SQLColumnPrivileges_t)::dlsym(g_hODBCDLL, "SQLColumnPrivileges" API_FN_SUFFIX);	assert(g_odbcAPI.SQLColumnPrivileges != NULL);
	g_odbcAPI.SQLColumns			= (SQLColumns_t)::dlsym(g_hODBCDLL, "SQLColumns" API_FN_SUFFIX);						assert(g_odbcAPI.SQLColumns != NULL);
	g_odbcAPI.SQLConnect			= (SQLConnect_t)::dlsym(g_hODBCDLL, "SQLConnect" API_FN_SUFFIX);						assert(g_odbcAPI.SQLConnect != NULL);
	g_odbcAPI.SQLCopyDesc			= (SQLCopyDesc_t)::dlsym(g_hODBCDLL, "SQLCopyDesc");					assert(g_odbcAPI.SQLCopyDesc != NULL);
	g_odbcAPI.SQLDataSources		= (SQLDataSources_t)::dlsym(g_hODBCDLL, "SQLDataSources" API_FN_SUFFIX);				assert(g_odbcAPI.SQLDataSources != NULL);
	g_odbcAPI.SQLDescribeCol		= (SQLDescribeCol_t)::dlsym(g_hODBCDLL, "SQLDescribeCol" API_FN_SUFFIX);				assert(g_odbcAPI.SQLDescribeCol != NULL);
	g_odbcAPI.SQLDescribeParam		= (SQLDescribeParam_t)::dlsym(g_hODBCDLL, "SQLDescribeParam");			assert(g_odbcAPI.SQLDescribeParam != NULL);
	g_odbcAPI.SQLDisconnect			= (SQLDisconnect_t)::dlsym(g_hODBCDLL, "SQLDisconnect");				assert(g_odbcAPI.SQLDisconnect != NULL);
	g_odbcAPI.SQLDriverConnect		= (SQLDriverConnect_t)::dlsym(g_hODBCDLL, "SQLDriverConnect" API_FN_SUFFIX);			assert(g_odbcAPI.SQLDriverConnect != NULL);
	g_odbcAPI.SQLDrivers			= (SQLDrivers_t)::dlsym(g_hODBCDLL, "SQLDrivers" API_FN_SUFFIX);						assert(g_odbcAPI.SQLDrivers != NULL);
	g_odbcAPI.SQLEndTran			= (SQLEndTran_t)::dlsym(g_hODBCDLL, "SQLEndTran");						assert(g_odbcAPI.SQLEndTran != NULL);
	g_odbcAPI.SQLError				= (SQLError_t)::dlsym(g_hODBCDLL, "SQLError" API_FN_SUFFIX);							assert(g_odbcAPI.SQLError != NULL);
	g_odbcAPI.SQLExecDirect			= (SQLExecDirect_t)::dlsym(g_hODBCDLL, "SQLExecDirect" API_FN_SUFFIX);				assert(g_odbcAPI.SQLExecDirect != NULL);
	g_odbcAPI.SQLExecute			= (SQLExecute_t)::dlsym(g_hODBCDLL, "SQLExecute");						assert(g_odbcAPI.SQLExecute != NULL);
	g_odbcAPI.SQLExtendedFetch		= (SQLExtendedFetch_t)::dlsym(g_hODBCDLL, "SQLExtendedFetch");			assert(g_odbcAPI.SQLExtendedFetch != NULL);
	g_odbcAPI.SQLFetch				= (SQLFetch_t)::dlsym(g_hODBCDLL, "SQLFetch");							assert(g_odbcAPI.SQLFetch != NULL);
	g_odbcAPI.SQLFetchScroll		= (SQLFetchScroll_t)::dlsym(g_hODBCDLL, "SQLFetchScroll");				assert(g_odbcAPI.SQLFetchScroll != NULL);
	g_odbcAPI.SQLForeignKeys		= (SQLForeignKeys_t)::dlsym(g_hODBCDLL, "SQLForeignKeys" API_FN_SUFFIX);				assert(g_odbcAPI.SQLForeignKeys != NULL);
	g_odbcAPI.SQLFreeConnect		= (SQLFreeConnect_t)::dlsym(g_hODBCDLL, "SQLFreeConnect");				assert(g_odbcAPI.SQLFreeConnect != NULL);
	g_odbcAPI.SQLFreeEnv			= (SQLFreeEnv_t)::dlsym(g_hODBCDLL, "SQLFreeEnv");						assert(g_odbcAPI.SQLFreeEnv != NULL);
	g_odbcAPI.SQLFreeHandle			= (SQLFreeHandle_t)::dlsym(g_hODBCDLL, "SQLFreeHandle");				assert(g_odbcAPI.SQLFreeHandle != NULL);
	g_odbcAPI.SQLFreeStmt			= (SQLFreeStmt_t)::dlsym(g_hODBCDLL, "SQLFreeStmt");					assert(g_odbcAPI.SQLFreeStmt != NULL);
	g_odbcAPI.SQLGetConnectAttr		= (SQLGetConnectAttr_t)::dlsym(g_hODBCDLL, "SQLGetConnectAttr" API_FN_SUFFIX);		assert(g_odbcAPI.SQLGetConnectAttr != NULL);
	g_odbcAPI.SQLGetConnectOption	= (SQLGetConnectOption_t)::dlsym(g_hODBCDLL, "SQLGetConnectOption" API_FN_SUFFIX);	assert(g_odbcAPI.SQLGetConnectOption != NULL);
	g_odbcAPI.SQLGetCursorName		= (SQLGetCursorName_t)::dlsym(g_hODBCDLL, "SQLGetCursorName" API_FN_SUFFIX);			assert(g_odbcAPI.SQLGetCursorName != NULL);
	g_odbcAPI.SQLGetData			= (SQLGetData_t)::dlsym(g_hODBCDLL, "SQLGetData");						assert(g_odbcAPI.SQLGetData!= NULL);
	g_odbcAPI.SQLGetDescField		= (SQLGetDescField_t)::dlsym(g_hODBCDLL, "SQLGetDescField" API_FN_SUFFIX);			assert(g_odbcAPI.SQLGetDescField != NULL);
	g_odbcAPI.SQLGetDescRec			= (SQLGetDescRec_t)::dlsym(g_hODBCDLL, "SQLGetDescRec" API_FN_SUFFIX);				assert(g_odbcAPI.SQLGetDescRec != NULL);
	g_odbcAPI.SQLGetDiagField		= (SQLGetDiagField_t)::dlsym(g_hODBCDLL, "SQLGetDiagField" API_FN_SUFFIX);			assert(g_odbcAPI.SQLGetDiagField != NULL);
	g_odbcAPI.SQLGetDiagRec			= (SQLGetDiagRec_t)::dlsym(g_hODBCDLL, "SQLGetDiagRec" API_FN_SUFFIX);				assert(g_odbcAPI.SQLGetDiagRec != NULL);
	g_odbcAPI.SQLGetEnvAttr			= (SQLGetEnvAttr_t)::dlsym(g_hODBCDLL, "SQLGetEnvAttr");				assert(g_odbcAPI.SQLGetEnvAttr != NULL);
	g_odbcAPI.SQLGetFunctions		= (SQLGetFunctions_t)::dlsym(g_hODBCDLL, "SQLGetFunctions");			assert(g_odbcAPI.SQLGetFunctions != NULL);
	g_odbcAPI.SQLGetInfo			= (SQLGetInfo_t)::dlsym(g_hODBCDLL, "SQLGetInfo" API_FN_SUFFIX);						assert(g_odbcAPI.SQLGetInfo != NULL);
	g_odbcAPI.SQLGetStmtAttr		= (SQLGetStmtAttr_t)::dlsym(g_hODBCDLL, "SQLGetStmtAttr" API_FN_SUFFIX);				assert(g_odbcAPI.SQLGetStmtAttr != NULL);
	g_odbcAPI.SQLGetStmtOption		= (SQLGetStmtOption_t)::dlsym(g_hODBCDLL, "SQLGetStmtOption");			assert(g_odbcAPI.SQLGetStmtOption != NULL);
	g_odbcAPI.SQLGetTypeInfo		= (SQLGetTypeInfo_t)::dlsym(g_hODBCDLL, "SQLGetTypeInfo");				assert(g_odbcAPI.SQLGetTypeInfo != NULL);
	g_odbcAPI.SQLMoreResults		= (SQLMoreResults_t)::dlsym(g_hODBCDLL, "SQLMoreResults");				assert(g_odbcAPI.SQLMoreResults != NULL);
	g_odbcAPI.SQLNativeSql			= (SQLNativeSql_t)::dlsym(g_hODBCDLL, "SQLNativeSql" API_FN_SUFFIX);					assert(g_odbcAPI.SQLNativeSql != NULL);
	g_odbcAPI.SQLNumParams			= (SQLNumParams_t)::dlsym(g_hODBCDLL, "SQLNumParams");					assert(g_odbcAPI.SQLNumParams != NULL);
	g_odbcAPI.SQLNumResultCols		= (SQLNumResultCols_t)::dlsym(g_hODBCDLL, "SQLNumResultCols");			assert(g_odbcAPI.SQLNumResultCols != NULL);
	g_odbcAPI.SQLParamData			= (SQLParamData_t)::dlsym(g_hODBCDLL, "SQLParamData");					assert(g_odbcAPI.SQLParamData != NULL);
	g_odbcAPI.SQLParamOptions		= (SQLParamOptions_t)::dlsym(g_hODBCDLL, "SQLParamOptions");			assert(g_odbcAPI.SQLParamOptions != NULL);
	g_odbcAPI.SQLPrepare			= (SQLPrepare_t)::dlsym(g_hODBCDLL, "SQLPrepare" API_FN_SUFFIX);						assert(g_odbcAPI.SQLPrepare != NULL);
	g_odbcAPI.SQLPrimaryKeys		= (SQLPrimaryKeys_t)::dlsym(g_hODBCDLL, "SQLPrimaryKeys" API_FN_SUFFIX);				assert(g_odbcAPI.SQLPrimaryKeys != NULL);
	g_odbcAPI.SQLProcedureColumns	= (SQLProcedureColumns_t)::dlsym(g_hODBCDLL, "SQLProcedureColumns" API_FN_SUFFIX);	assert(g_odbcAPI.SQLProcedureColumns != NULL);
	g_odbcAPI.SQLProcedures			= (SQLProcedures_t)::dlsym(g_hODBCDLL, "SQLProcedures" API_FN_SUFFIX);				assert(g_odbcAPI.SQLProcedures != NULL);
	g_odbcAPI.SQLPutData			= (SQLPutData_t)::dlsym(g_hODBCDLL, "SQLPutData");						assert(g_odbcAPI.SQLPutData != NULL);
	g_odbcAPI.SQLRowCount			= (SQLRowCount_t)::dlsym(g_hODBCDLL, "SQLRowCount");					assert(g_odbcAPI.SQLRowCount != NULL);
	g_odbcAPI.SQLSetConnectAttr		= (SQLSetConnectAttr_t)::dlsym(g_hODBCDLL, "SQLSetConnectAttr" API_FN_SUFFIX);		assert(g_odbcAPI.SQLSetConnectAttr != NULL);
	g_odbcAPI.SQLSetConnectOption	= (SQLSetConnectOption_t)::dlsym(g_hODBCDLL, "SQLSetConnectOption" API_FN_SUFFIX);	assert(g_odbcAPI.SQLSetConnectOption != NULL);
	g_odbcAPI.SQLSetCursorName		= (SQLSetCursorName_t)::dlsym(g_hODBCDLL, "SQLSetCursorName" API_FN_SUFFIX);			assert(g_odbcAPI.SQLSetCursorName != NULL);
	g_odbcAPI.SQLSetDescField		= (SQLSetDescField_t)::dlsym(g_hODBCDLL, "SQLSetDescField" API_FN_SUFFIX);			assert(g_odbcAPI.SQLSetDescField != NULL);
	g_odbcAPI.SQLSetDescRec			= (SQLSetDescRec_t)::dlsym(g_hODBCDLL, "SQLSetDescRec");				assert(g_odbcAPI.SQLSetDescRec != NULL);
	g_odbcAPI.SQLSetEnvAttr			= (SQLSetEnvAttr_t)::dlsym(g_hODBCDLL, "SQLSetEnvAttr");				assert(g_odbcAPI.SQLSetEnvAttr != NULL);
	g_odbcAPI.SQLSetParam			= (SQLSetParam_t)::dlsym(g_hODBCDLL, "SQLSetParam");					assert(g_odbcAPI.SQLSetParam != NULL);
	g_odbcAPI.SQLSetPos				= (SQLSetPos_t)::dlsym(g_hODBCDLL, "SQLSetPos");						assert(g_odbcAPI.SQLSetPos != NULL);
	g_odbcAPI.SQLSetScrollOptions	= (SQLSetScrollOptions_t)::dlsym(g_hODBCDLL, "SQLSetScrollOptions");	assert(g_odbcAPI.SQLSetScrollOptions != NULL);
	g_odbcAPI.SQLSetStmtAttr		= (SQLSetStmtAttr_t)::dlsym(g_hODBCDLL, "SQLSetStmtAttr" API_FN_SUFFIX);				assert(g_odbcAPI.SQLSetStmtAttr != NULL);
	g_odbcAPI.SQLSetStmtOption		= (SQLSetStmtOption_t)::dlsym(g_hODBCDLL, "SQLSetStmtOption");			assert(g_odbcAPI.SQLSetStmtOption != NULL);
	g_odbcAPI.SQLSpecialColumns		= (SQLSpecialColumns_t)::dlsym(g_hODBCDLL, "SQLSpecialColumns" API_FN_SUFFIX);		assert(g_odbcAPI.SQLSpecialColumns != NULL);
	g_odbcAPI.SQLStatistics			= (SQLStatistics_t)::dlsym(g_hODBCDLL, "SQLStatistics" API_FN_SUFFIX);				assert(g_odbcAPI.SQLStatistics != NULL);
	g_odbcAPI.SQLTablePrivileges	= (SQLTablePrivileges_t)::dlsym(g_hODBCDLL, "SQLTablePrivileges" API_FN_SUFFIX);		assert(g_odbcAPI.SQLTablePrivileges != NULL);
	g_odbcAPI.SQLTables				= (SQLTables_t)::dlsym(g_hODBCDLL, "SQLTables" API_FN_SUFFIX);						assert(g_odbcAPI.SQLTables != NULL);
	g_odbcAPI.SQLTransact			= (SQLTransact_t)::dlsym(g_hODBCDLL, "SQLTransact");					assert(g_odbcAPI.SQLTransact != NULL);
}

static void ResetAPI()
{
	g_odbcAPI.SQLAllocConnect = NULL;		// 1.0
	g_odbcAPI.SQLAllocEnv = NULL;			// 1.0
	g_odbcAPI.SQLAllocHandle = NULL;		// 3.0
	g_odbcAPI.SQLAllocStmt = NULL;			// 1.0
	g_odbcAPI.SQLBindCol = NULL;			// 1.0
	g_odbcAPI.SQLBindParameter = NULL;		// 2.0
	g_odbcAPI.SQLBrowseConnect = NULL;		// 1.0
	g_odbcAPI.SQLBulkOperations = NULL;		// 3.0
	g_odbcAPI.SQLCancel = NULL;				// 1.0
	g_odbcAPI.SQLCloseCursor = NULL;		// 3.0
	g_odbcAPI.SQLColAttribute = NULL;		// 3,0
	g_odbcAPI.SQLColAttributes = NULL;		// 1.0
	g_odbcAPI.SQLColumnPrivileges = NULL;	// 1.0
	g_odbcAPI.SQLColumns = NULL;			// 1.0
	g_odbcAPI.SQLConnect = NULL;			// 1.0
	g_odbcAPI.SQLCopyDesc = NULL;			// 3.0
	g_odbcAPI.SQLDataSources = NULL;		// 1.0
	g_odbcAPI.SQLDescribeCol = NULL;		// 1.0
	g_odbcAPI.SQLDescribeParam = NULL;		// 1.0
	g_odbcAPI.SQLDisconnect = NULL;			// 1.0
	g_odbcAPI.SQLDriverConnect = NULL;		// 1.0
	g_odbcAPI.SQLDrivers = NULL;			// 2.0
	g_odbcAPI.SQLEndTran = NULL;			// 3.0
	g_odbcAPI.SQLError = NULL;				// 1.0
	g_odbcAPI.SQLExecDirect = NULL;			// 1.0
	g_odbcAPI.SQLExecute = NULL;			// 1.0
	g_odbcAPI.SQLExtendedFetch = NULL;		// 1.0
	g_odbcAPI.SQLFetch = NULL;				// 1.0
	g_odbcAPI.SQLFetchScroll = NULL;		// 1.0
	g_odbcAPI.SQLForeignKeys = NULL;		// 1.0
	g_odbcAPI.SQLFreeConnect = NULL;		// 1.0
	g_odbcAPI.SQLFreeEnv = NULL;			// 1.0
	g_odbcAPI.SQLFreeHandle = NULL;			// 3.0
	g_odbcAPI.SQLFreeStmt = NULL;			// 1.0
	g_odbcAPI.SQLGetConnectAttr = NULL;		// 3.0
	g_odbcAPI.SQLGetConnectOption = NULL;	// 1.0
	g_odbcAPI.SQLGetCursorName = NULL;		// 1.0
	g_odbcAPI.SQLGetData = NULL;			// 1.0
	g_odbcAPI.SQLGetDescField = NULL;		// 3.0
	g_odbcAPI.SQLGetDescRec = NULL;			// 3.0
	g_odbcAPI.SQLGetDiagField = NULL;		// 3.0
	g_odbcAPI.SQLGetDiagRec = NULL;			// 3.0
	g_odbcAPI.SQLGetEnvAttr = NULL;			// 3.0
	g_odbcAPI.SQLGetFunctions = NULL;		// 1.0
	g_odbcAPI.SQLGetInfo = NULL;			// 1.0
	g_odbcAPI.SQLGetStmtAttr = NULL;		// 3.0
	g_odbcAPI.SQLGetStmtOption = NULL;		// 1.0
	g_odbcAPI.SQLGetTypeInfo = NULL;		// 1.0
	g_odbcAPI.SQLMoreResults = NULL;		// 1.0
	g_odbcAPI.SQLNativeSql = NULL;			// 1.0
	g_odbcAPI.SQLNumParams = NULL;			// 1.0
	g_odbcAPI.SQLNumResultCols = NULL;		// 1.0
	g_odbcAPI.SQLParamData = NULL;			// 1.0
	g_odbcAPI.SQLParamOptions = NULL;		// 1.0
	g_odbcAPI.SQLPrepare = NULL;			// 1.0
	g_odbcAPI.SQLPrimaryKeys = NULL;		// 1.0
	g_odbcAPI.SQLProcedureColumns = NULL;	// 1.0
	g_odbcAPI.SQLProcedures = NULL;			// 1.0
	g_odbcAPI.SQLPutData = NULL;			// 1.0
	g_odbcAPI.SQLRowCount = NULL;			// 1.0
	g_odbcAPI.SQLSetConnectAttr = NULL;		// 3.0
	g_odbcAPI.SQLSetConnectOption = NULL;	// 1.0
	g_odbcAPI.SQLSetCursorName = NULL;		// 1.0
	g_odbcAPI.SQLSetDescField = NULL;		// 3.0
	g_odbcAPI.SQLSetDescRec = NULL;			// 3.0
	g_odbcAPI.SQLSetEnvAttr = NULL;			// 3.0
	g_odbcAPI.SQLSetParam = NULL;			// 1.0
	g_odbcAPI.SQLSetPos = NULL;				// 1.0
	g_odbcAPI.SQLSetScrollOptions = NULL;	// 1.0
	g_odbcAPI.SQLSetStmtAttr = NULL;		// 3.0
	g_odbcAPI.SQLSetStmtOption = NULL;		// 1.0
	g_odbcAPI.SQLSpecialColumns = NULL;		// 1.0
	g_odbcAPI.SQLStatistics = NULL;			// 1.0
	g_odbcAPI.SQLTablePrivileges = NULL;	// 1.0
	g_odbcAPI.SQLTables = NULL;				// 1.0
	g_odbcAPI.SQLTransact = NULL;			// 1.0
}

void AddODBCSupport(const SAConnection *pCon)
{
	SACriticalSectionScope scope(&odbcLoaderMutex);

	if(!g_hODBCDLL)
	{
		SAString sErrorMessage, sLibName, sLibsList = pCon->Option(_TSA("ODBC.LIBS"));
		if( sLibsList.IsEmpty() )
			sLibsList = g_sODBCDLLNames;

		// load ODBC manager library
		g_hODBCDLL = SALoadLibraryFromList(sLibsList, sErrorMessage, sLibName, RTLD_LAZY);
		if( ! g_hODBCDLL )
			throw SAException(SA_Library_Error, SA_Library_Error_LoadLibraryFails, -1, IDS_LOAD_LIBRARY_FAILS, (const SAChar*)sErrorMessage);

		LoadAPI();
	}

	if( SAGlobals::UnloadAPI() )
		g_nODBCDLLRefs++;
	else
		g_nODBCDLLRefs = 1;
}

void ReleaseODBCSupport()
{
	SACriticalSectionScope scope(&odbcLoaderMutex);

	assert(g_nODBCDLLRefs > 0);
	--g_nODBCDLLRefs;
	if( ! g_nODBCDLLRefs )
	{
		ResetAPI();

		// SegFault
		//::dlclose(g_hODBCDLL);

		g_hODBCDLL = NULL;
	}
}
