// db2API_linux.cpp
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <db2API.h>

#ifdef ODBC64
#define DB2LIB_NAME "libdb2o"
#else
#define DB2LIB_NAME "libdb2"
#endif

static const char *g_sDB2DLLNames = DB2LIB_NAME SHARED_OBJ_EXT;
static void* g_hDB2DLL = NULL;
long g_nDB2DLLVersionLoaded = 0l;
static long g_nDB2DLLRefs = 0l;

#ifdef SA_UNICODE
#define API_FN_SUFFIX	"W"
#else
#define API_FN_SUFFIX	""
#endif

static SAMutex db2LoaderMutex;

// API definitions
db2API g_db2API;

db2API::db2API()
{
	SQLAllocConnect		= NULL;
	SQLAllocEnv			= NULL;
	SQLAllocHandle		= NULL;
	SQLAllocStmt		= NULL;
	SQLBindCol			= NULL;
	SQLBindFileToCol	= NULL;
	SQLBindFileToParam	= NULL;
	SQLBindParameter	= NULL;
	SQLBrowseConnect	= NULL;
	SQLBulkOperations	= NULL;
	SQLCancel			= NULL;
	SQLCloseCursor		= NULL;
	SQLColAttribute		= NULL;
	SQLColAttributes	= NULL;
	SQLColumnPrivileges	= NULL;
	SQLColumns			= NULL;
	SQLConnect			= NULL;
	SQLCopyDesc			= NULL;
	SQLDataSources		= NULL;
	SQLDescribeCol		= NULL;
	SQLDescribeParam	= NULL;
	SQLDisconnect		= NULL;
	SQLDriverConnect	= NULL;
	SQLEndTran			= NULL;
	SQLError			= NULL;
	SQLExecDirect		= NULL;
	SQLExecute			= NULL;
	SQLExtendedBind		= NULL;
	SQLExtendedFetch	= NULL;
	SQLExtendedPrepare	= NULL;
	SQLFetch			= NULL;
	SQLFetchScroll		= NULL;
	SQLForeignKeys		= NULL;
	SQLFreeConnect		= NULL;
	SQLFreeEnv			= NULL;
	SQLFreeHandle		= NULL;
	SQLFreeStmt			= NULL;
	SQLGetConnectAttr	= NULL;
	SQLGetConnectOption	= NULL;
	SQLGetCursorName	= NULL;
	SQLGetData			= NULL;
	SQLGetDescField		= NULL;
	SQLGetDescRec		= NULL;
	SQLGetDiagField		= NULL;
	SQLGetDiagRec		= NULL;
	SQLGetEnvAttr		= NULL;
	SQLGetFunctions		= NULL;
	SQLGetInfo			= NULL;
	SQLGetLength		= NULL;
	SQLGetPosition		= NULL;
	SQLGetSQLCA			= NULL;
	SQLGetStmtAttr		= NULL;
	SQLGetStmtOption	= NULL;
	SQLGetSubString		= NULL;
	SQLGetTypeInfo		= NULL;
	SQLMoreResults		= NULL;
	SQLNativeSql		= NULL;
	SQLNumParams		= NULL;
	SQLNumResultCols	= NULL;
	SQLParamData		= NULL;
	SQLParamOptions		= NULL;
	SQLPrepare			= NULL;
	SQLPrimaryKeys		= NULL;
	SQLProcedureColumns	= NULL;
	SQLProcedures		= NULL;
	SQLPutData			= NULL;
	SQLRowCount			= NULL;
	SQLSetColAttributes	= NULL;
	SQLSetConnectAttr	= NULL;
	SQLSetConnection	= NULL;
	SQLSetConnectOption	= NULL;
	SQLSetCursorName	= NULL;
	SQLSetDescField		= NULL;
	SQLSetDescRec		= NULL;
	SQLSetEnvAttr		= NULL;
	SQLSetParam			= NULL;
	SQLSetPos			= NULL;
	SQLSetStmtAttr		= NULL;
	SQLSetStmtOption	= NULL;
	SQLSpecialColumns	= NULL;
	SQLStatistics		= NULL;
	SQLTablePrivileges	= NULL;
	SQLTables			= NULL;
	SQLTransact			= NULL;
}

db2ConnectionHandles::db2ConnectionHandles()
{
	m_hevn = (SQLHENV)NULL;
	m_hdbc = (SQLHDBC)NULL;
}

db2CommandHandles::db2CommandHandles()
{
	m_hstmt = (SQLHSTMT)NULL;
}

static void LoadAPI()
{
	g_db2API.SQLAllocConnect	= (SQLAllocConnect_t)::dlsym(g_hDB2DLL, "SQLAllocConnect");	assert(g_db2API.SQLAllocConnect != NULL);
	g_db2API.SQLAllocEnv		= (SQLAllocEnv_t)::dlsym(g_hDB2DLL, "SQLAllocEnv");	assert(g_db2API.SQLAllocEnv != NULL);
	g_db2API.SQLAllocHandle		= (SQLAllocHandle_t)::dlsym(g_hDB2DLL, "SQLAllocHandle");	assert(g_db2API.SQLAllocHandle != NULL);
	g_db2API.SQLAllocStmt		= (SQLAllocStmt_t)::dlsym(g_hDB2DLL, "SQLAllocStmt");	assert(g_db2API.SQLAllocStmt != NULL);
	g_db2API.SQLBindCol			= (SQLBindCol_t)::dlsym(g_hDB2DLL, "SQLBindCol");	assert(g_db2API.SQLBindCol != NULL);
	g_db2API.SQLBindFileToCol	= (SQLBindFileToCol_t)::dlsym(g_hDB2DLL, "SQLBindFileToCol");	assert(g_db2API.SQLBindFileToCol != NULL);
	g_db2API.SQLBindFileToParam	= (SQLBindFileToParam_t)::dlsym(g_hDB2DLL, "SQLBindFileToParam");	assert(g_db2API.SQLBindFileToParam != NULL);
	g_db2API.SQLBindParameter	= (SQLBindParameter_t)::dlsym(g_hDB2DLL, "SQLBindParameter");	assert(g_db2API.SQLBindParameter != NULL);
	g_db2API.SQLBrowseConnect	= (SQLBrowseConnect_t)::dlsym(g_hDB2DLL, "SQLBrowseConnect" API_FN_SUFFIX);	assert(g_db2API.SQLBrowseConnect != NULL);
	g_db2API.SQLBulkOperations	= (SQLBulkOperations_t)::dlsym(g_hDB2DLL, "SQLBulkOperations");	assert(g_db2API.SQLBulkOperations != NULL);
	g_db2API.SQLCancel			= (SQLCancel_t)::dlsym(g_hDB2DLL, "SQLCancel");	assert(g_db2API.SQLCancel != NULL);
	g_db2API.SQLCloseCursor		= (SQLCloseCursor_t)::dlsym(g_hDB2DLL, "SQLCloseCursor");	assert(g_db2API.SQLCloseCursor != NULL);
	g_db2API.SQLColAttribute	= (SQLColAttribute_t)::dlsym(g_hDB2DLL, "SQLColAttribute" API_FN_SUFFIX);	assert(g_db2API.SQLColAttribute != NULL);
	g_db2API.SQLColAttributes	= (SQLColAttributes_t)::dlsym(g_hDB2DLL, "SQLColAttributes" API_FN_SUFFIX);	assert(g_db2API.SQLColAttributes != NULL);
	g_db2API.SQLColumnPrivileges= (SQLColumnPrivileges_t)::dlsym(g_hDB2DLL, "SQLColumnPrivileges" API_FN_SUFFIX);	assert(g_db2API.SQLColumnPrivileges != NULL);
	g_db2API.SQLColumns			= (SQLColumns_t)::dlsym(g_hDB2DLL, "SQLColumns" API_FN_SUFFIX);	assert(g_db2API.SQLColumns != NULL);
	g_db2API.SQLConnect			= (SQLConnect_t)::dlsym(g_hDB2DLL, "SQLConnect" API_FN_SUFFIX);	assert(g_db2API.SQLConnect != NULL);
	g_db2API.SQLCopyDesc		= (SQLCopyDesc_t)::dlsym(g_hDB2DLL, "SQLCopyDesc");	assert(g_db2API.SQLCopyDesc != NULL);
	g_db2API.SQLDataSources		= (SQLDataSources_t)::dlsym(g_hDB2DLL, "SQLDataSources" API_FN_SUFFIX);	assert(g_db2API.SQLDataSources != NULL);
	g_db2API.SQLDescribeCol		= (SQLDescribeCol_t)::dlsym(g_hDB2DLL, "SQLDescribeCol" API_FN_SUFFIX);	assert(g_db2API.SQLDescribeCol != NULL);
	g_db2API.SQLDescribeParam	= (SQLDescribeParam_t)::dlsym(g_hDB2DLL, "SQLDescribeParam");	assert(g_db2API.SQLDescribeParam != NULL);
	g_db2API.SQLDisconnect		= (SQLDisconnect_t)::dlsym(g_hDB2DLL, "SQLDisconnect");	assert(g_db2API.SQLDisconnect != NULL);
	g_db2API.SQLDriverConnect	= (SQLDriverConnect_t)::dlsym(g_hDB2DLL, "SQLDriverConnect" API_FN_SUFFIX);	assert(g_db2API.SQLDriverConnect != NULL);
	g_db2API.SQLEndTran			= (SQLEndTran_t)::dlsym(g_hDB2DLL, "SQLEndTran");	assert(g_db2API.SQLEndTran != NULL);
	g_db2API.SQLError			= (SQLError_t)::dlsym(g_hDB2DLL, "SQLError" API_FN_SUFFIX);	assert(g_db2API.SQLError != NULL);
	g_db2API.SQLExecDirect		= (SQLExecDirect_t)::dlsym(g_hDB2DLL, "SQLExecDirect" API_FN_SUFFIX);	assert(g_db2API.SQLExecDirect != NULL);
	g_db2API.SQLExecute			= (SQLExecute_t)::dlsym(g_hDB2DLL, "SQLExecute");	assert(g_db2API.SQLExecute != NULL);
	g_db2API.SQLExtendedBind	= (SQLExtendedBind_t)::dlsym(g_hDB2DLL, "SQLExtendedBind");	assert(g_db2API.SQLExtendedBind != NULL);
	g_db2API.SQLExtendedFetch	= (SQLExtendedFetch_t)::dlsym(g_hDB2DLL, "SQLExtendedFetch");	assert(g_db2API.SQLExtendedFetch != NULL);
	g_db2API.SQLExtendedPrepare	= (SQLExtendedPrepare_t)::dlsym(g_hDB2DLL, "SQLExtendedPrepare" API_FN_SUFFIX);	assert(g_db2API.SQLExtendedPrepare != NULL);
	g_db2API.SQLFetch			= (SQLFetch_t)::dlsym(g_hDB2DLL, "SQLFetch");	assert(g_db2API.SQLFetch != NULL);
	g_db2API.SQLFetchScroll		= (SQLFetchScroll_t)::dlsym(g_hDB2DLL, "SQLFetchScroll");	assert(g_db2API.SQLFetchScroll != NULL);
	g_db2API.SQLForeignKeys		= (SQLForeignKeys_t)::dlsym(g_hDB2DLL, "SQLForeignKeys");	assert(g_db2API.SQLForeignKeys != NULL);
	g_db2API.SQLFreeConnect		= (SQLFreeConnect_t)::dlsym(g_hDB2DLL, "SQLFreeConnect");	assert(g_db2API.SQLFreeConnect != NULL);
	g_db2API.SQLFreeEnv			= (SQLFreeEnv_t)::dlsym(g_hDB2DLL, "SQLFreeEnv");	assert(g_db2API.SQLFreeEnv != NULL);
	g_db2API.SQLFreeHandle		= (SQLFreeHandle_t)::dlsym(g_hDB2DLL, "SQLFreeHandle");	assert(g_db2API.SQLFreeHandle != NULL);
	g_db2API.SQLFreeStmt		= (SQLFreeStmt_t)::dlsym(g_hDB2DLL, "SQLFreeStmt");	assert(g_db2API.SQLFreeStmt != NULL);
	g_db2API.SQLGetConnectAttr	= (SQLGetConnectAttr_t)::dlsym(g_hDB2DLL, "SQLGetConnectAttr" API_FN_SUFFIX);	assert(g_db2API.SQLGetConnectAttr != NULL);
	g_db2API.SQLGetConnectOption= (SQLGetConnectOption_t)::dlsym(g_hDB2DLL, "SQLGetConnectOption" API_FN_SUFFIX);	assert(g_db2API.SQLGetConnectOption != NULL);
	g_db2API.SQLGetCursorName	= (SQLGetCursorName_t)::dlsym(g_hDB2DLL, "SQLGetCursorName" API_FN_SUFFIX);	assert(g_db2API.SQLGetCursorName != NULL);
	g_db2API.SQLGetData			= (SQLGetData_t)::dlsym(g_hDB2DLL, "SQLGetData");	assert(g_db2API.SQLGetData != NULL);
	g_db2API.SQLGetDescField	= (SQLGetDescField_t)::dlsym(g_hDB2DLL, "SQLGetDescField" API_FN_SUFFIX);	assert(g_db2API.SQLGetDescField != NULL);
	g_db2API.SQLGetDescRec		= (SQLGetDescRec_t)::dlsym(g_hDB2DLL, "SQLGetDescRec" API_FN_SUFFIX);	// assert(g_db2API.SQLGetDescRec != NULL);
	g_db2API.SQLGetDiagField	= (SQLGetDiagField_t)::dlsym(g_hDB2DLL, "SQLGetDiagField" API_FN_SUFFIX);	assert(g_db2API.SQLGetDiagField != NULL);
	g_db2API.SQLGetDiagRec		= (SQLGetDiagRec_t)::dlsym(g_hDB2DLL, "SQLGetDiagRec" API_FN_SUFFIX);	assert(g_db2API.SQLGetDiagRec != NULL);
	g_db2API.SQLGetEnvAttr		= (SQLGetEnvAttr_t)::dlsym(g_hDB2DLL, "SQLGetEnvAttr");	assert(g_db2API.SQLGetEnvAttr != NULL);
	g_db2API.SQLGetFunctions	= (SQLGetFunctions_t)::dlsym(g_hDB2DLL, "SQLGetFunctions");	assert(g_db2API.SQLGetFunctions != NULL);
	g_db2API.SQLGetInfo			= (SQLGetInfo_t)::dlsym(g_hDB2DLL, "SQLGetInfo" API_FN_SUFFIX);	assert(g_db2API.SQLGetInfo != NULL);
	g_db2API.SQLGetLength		= (SQLGetLength_t)::dlsym(g_hDB2DLL, "SQLGetLength");	assert(g_db2API.SQLGetLength != NULL);
	g_db2API.SQLGetPosition		= (SQLGetPosition_t)::dlsym(g_hDB2DLL, "SQLGetPosition");	assert(g_db2API.SQLGetPosition != NULL);
	g_db2API.SQLGetSQLCA		= (SQLGetSQLCA_t)::dlsym(g_hDB2DLL, "SQLGetSQLCA");	assert(g_db2API.SQLGetSQLCA != NULL);
	g_db2API.SQLGetStmtAttr		= (SQLGetStmtAttr_t)::dlsym(g_hDB2DLL, "SQLGetStmtAttr" API_FN_SUFFIX);	assert(g_db2API.SQLGetStmtAttr != NULL);
	g_db2API.SQLGetStmtOption	= (SQLGetStmtOption_t)::dlsym(g_hDB2DLL, "SQLGetStmtOption");	assert(g_db2API.SQLGetStmtOption != NULL);
	g_db2API.SQLGetSubString	= (SQLGetSubString_t)::dlsym(g_hDB2DLL, "SQLGetSubString");	assert(g_db2API.SQLGetSubString != NULL);
	g_db2API.SQLGetTypeInfo		= (SQLGetTypeInfo_t)::dlsym(g_hDB2DLL, "SQLGetTypeInfo");	assert(g_db2API.SQLGetTypeInfo != NULL);
	g_db2API.SQLMoreResults		= (SQLMoreResults_t)::dlsym(g_hDB2DLL, "SQLMoreResults");	assert(g_db2API.SQLMoreResults != NULL);
	g_db2API.SQLNativeSql		= (SQLNativeSql_t)::dlsym(g_hDB2DLL, "SQLNativeSql");	assert(g_db2API.SQLNativeSql != NULL);
	g_db2API.SQLNumParams		= (SQLNumParams_t)::dlsym(g_hDB2DLL, "SQLNumParams");	assert(g_db2API.SQLNumParams != NULL);
	g_db2API.SQLNumResultCols	= (SQLNumResultCols_t)::dlsym(g_hDB2DLL, "SQLNumResultCols");	assert(g_db2API.SQLNumResultCols != NULL);
	g_db2API.SQLParamData		= (SQLParamData_t)::dlsym(g_hDB2DLL, "SQLParamData");	assert(g_db2API.SQLParamData != NULL);
	g_db2API.SQLParamOptions	= (SQLParamOptions_t)::dlsym(g_hDB2DLL, "SQLParamOptions");	assert(g_db2API.SQLParamOptions != NULL);
	g_db2API.SQLPrepare			= (SQLPrepare_t)::dlsym(g_hDB2DLL, "SQLPrepare" API_FN_SUFFIX);	assert(g_db2API.SQLPrepare != NULL);
	g_db2API.SQLPrimaryKeys		= (SQLPrimaryKeys_t)::dlsym(g_hDB2DLL, "SQLPrimaryKeys" API_FN_SUFFIX);	assert(g_db2API.SQLPrimaryKeys != NULL);
	g_db2API.SQLProcedureColumns= (SQLProcedureColumns_t)::dlsym(g_hDB2DLL, "SQLProcedureColumns" API_FN_SUFFIX);	assert(g_db2API.SQLProcedureColumns != NULL);
	g_db2API.SQLProcedures		= (SQLProcedures_t)::dlsym(g_hDB2DLL, "SQLProcedures" API_FN_SUFFIX);	assert(g_db2API.SQLProcedures != NULL);
	g_db2API.SQLPutData			= (SQLPutData_t)::dlsym(g_hDB2DLL, "SQLPutData");	assert(g_db2API.SQLPutData != NULL);
	g_db2API.SQLRowCount		= (SQLRowCount_t)::dlsym(g_hDB2DLL, "SQLRowCount");	assert(g_db2API.SQLRowCount != NULL);
	g_db2API.SQLSetColAttributes= (SQLSetColAttributes_t)::dlsym(g_hDB2DLL, "SQLSetColAttributes");	assert(g_db2API.SQLSetColAttributes != NULL);
	g_db2API.SQLSetConnectAttr	= (SQLSetConnectAttr_t)::dlsym(g_hDB2DLL, "SQLSetConnectAttr" API_FN_SUFFIX);	assert(g_db2API.SQLSetConnectAttr != NULL);
	g_db2API.SQLSetConnection	= (SQLSetConnection_t)::dlsym(g_hDB2DLL, "SQLSetConnection");	assert(g_db2API.SQLSetConnection != NULL);
	g_db2API.SQLSetConnectOption= (SQLSetConnectOption_t)::dlsym(g_hDB2DLL, "SQLSetConnectOption" API_FN_SUFFIX);	assert(g_db2API.SQLSetConnectOption != NULL);
	g_db2API.SQLSetCursorName	= (SQLSetCursorName_t)::dlsym(g_hDB2DLL, "SQLSetCursorName" API_FN_SUFFIX);	assert(g_db2API.SQLSetCursorName != NULL);
	g_db2API.SQLSetDescField	= (SQLSetDescField_t)::dlsym(g_hDB2DLL, "SQLSetDescField" API_FN_SUFFIX);	assert(g_db2API.SQLSetDescField != NULL);
	g_db2API.SQLSetDescRec		= (SQLSetDescRec_t)::dlsym(g_hDB2DLL, "SQLSetDescRec");	assert(g_db2API.SQLSetDescRec != NULL);
	g_db2API.SQLSetEnvAttr		= (SQLSetEnvAttr_t)::dlsym(g_hDB2DLL, "SQLSetEnvAttr");	assert(g_db2API.SQLSetEnvAttr != NULL);
	g_db2API.SQLSetParam		= (SQLSetParam_t)::dlsym(g_hDB2DLL, "SQLSetParam");	assert(g_db2API.SQLSetParam != NULL);
	g_db2API.SQLSetPos			= (SQLSetPos_t)::dlsym(g_hDB2DLL, "SQLSetPos");	assert(g_db2API.SQLSetPos != NULL);
	g_db2API.SQLSetStmtAttr		= (SQLSetStmtAttr_t)::dlsym(g_hDB2DLL, "SQLSetStmtAttr" API_FN_SUFFIX);	assert(g_db2API.SQLSetStmtAttr != NULL);

	g_db2API.SQLSetStmtOption	= (SQLSetStmtOption_t)::dlsym(g_hDB2DLL, "SQLSetStmtOption");	assert(g_db2API.SQLSetStmtOption != NULL);
	g_db2API.SQLSpecialColumns	= (SQLSpecialColumns_t)::dlsym(g_hDB2DLL, "SQLSpecialColumns" API_FN_SUFFIX);	assert(g_db2API.SQLSpecialColumns != NULL);
	g_db2API.SQLStatistics		= (SQLStatistics_t)::dlsym(g_hDB2DLL, "SQLStatistics" API_FN_SUFFIX);	assert(g_db2API.SQLStatistics != NULL);
	g_db2API.SQLTablePrivileges	= (SQLTablePrivileges_t)::dlsym(g_hDB2DLL, "SQLTablePrivileges" API_FN_SUFFIX);	assert(g_db2API.SQLTablePrivileges != NULL);
	g_db2API.SQLTables			= (SQLTables_t)::dlsym(g_hDB2DLL, "SQLTables" API_FN_SUFFIX);	assert(g_db2API.SQLTables != NULL);
	g_db2API.SQLTransact		= (SQLTransact_t)::dlsym(g_hDB2DLL, "SQLTransact");	assert(g_db2API.SQLTransact != NULL);
}

static void ResetAPI()
{
	g_db2API.SQLAllocConnect	= NULL;
	g_db2API.SQLAllocEnv		= NULL;
	g_db2API.SQLAllocHandle		= NULL;
	g_db2API.SQLAllocStmt		= NULL;
	g_db2API.SQLBindCol			= NULL;
	g_db2API.SQLBindFileToCol	= NULL;
	g_db2API.SQLBindFileToParam	= NULL;
	g_db2API.SQLBindParameter	= NULL;
	g_db2API.SQLBrowseConnect	= NULL;
	g_db2API.SQLBulkOperations	= NULL;
	g_db2API.SQLCancel			= NULL;
	g_db2API.SQLCloseCursor		= NULL;
	g_db2API.SQLColAttribute	= NULL;
	g_db2API.SQLColAttributes	= NULL;
	g_db2API.SQLColumnPrivileges= NULL;
	g_db2API.SQLColumns			= NULL;
	g_db2API.SQLConnect			= NULL;
	g_db2API.SQLCopyDesc		= NULL;
	g_db2API.SQLDataSources		= NULL;
	g_db2API.SQLDescribeCol		= NULL;
	g_db2API.SQLDescribeParam	= NULL;
	g_db2API.SQLDisconnect		= NULL;
	g_db2API.SQLDriverConnect	= NULL;
	g_db2API.SQLEndTran			= NULL;
	g_db2API.SQLError			= NULL;
	g_db2API.SQLExecDirect		= NULL;
	g_db2API.SQLExecute			= NULL;
	g_db2API.SQLExtendedBind	= NULL;
	g_db2API.SQLExtendedFetch	= NULL;
	g_db2API.SQLExtendedPrepare	= NULL;
	g_db2API.SQLFetch			= NULL;
	g_db2API.SQLFetchScroll		= NULL;
	g_db2API.SQLForeignKeys		= NULL;
	g_db2API.SQLFreeConnect		= NULL;
	g_db2API.SQLFreeEnv			= NULL;
	g_db2API.SQLFreeHandle		= NULL;
	g_db2API.SQLFreeStmt		= NULL;
	g_db2API.SQLGetConnectAttr	= NULL;
	g_db2API.SQLGetConnectOption= NULL;
	g_db2API.SQLGetCursorName	= NULL;
	g_db2API.SQLGetData			= NULL;
	g_db2API.SQLGetDescField	= NULL;
	g_db2API.SQLGetDescRec		= NULL;
	g_db2API.SQLGetDiagField	= NULL;
	g_db2API.SQLGetDiagRec		= NULL;
	g_db2API.SQLGetEnvAttr		= NULL;
	g_db2API.SQLGetFunctions	= NULL;
	g_db2API.SQLGetInfo			= NULL;
	g_db2API.SQLGetLength		= NULL;
	g_db2API.SQLGetPosition		= NULL;
	g_db2API.SQLGetSQLCA		= NULL;
	g_db2API.SQLGetStmtAttr		= NULL;
	g_db2API.SQLGetStmtOption	= NULL;
	g_db2API.SQLGetSubString	= NULL;
	g_db2API.SQLGetTypeInfo		= NULL;
	g_db2API.SQLMoreResults		= NULL;
	g_db2API.SQLNativeSql		= NULL;
	g_db2API.SQLNumParams		= NULL;
	g_db2API.SQLNumResultCols	= NULL;
	g_db2API.SQLParamData		= NULL;
	g_db2API.SQLParamOptions	= NULL;
	g_db2API.SQLPrepare			= NULL;
	g_db2API.SQLPrimaryKeys		= NULL;
	g_db2API.SQLProcedureColumns= NULL;
	g_db2API.SQLProcedures		= NULL;
	g_db2API.SQLPutData			= NULL;
	g_db2API.SQLRowCount		= NULL;
	g_db2API.SQLSetColAttributes= NULL;
	g_db2API.SQLSetConnectAttr	= NULL;
	g_db2API.SQLSetConnection	= NULL;
	g_db2API.SQLSetConnectOption= NULL;
	g_db2API.SQLSetCursorName	= NULL;
	g_db2API.SQLSetDescField	= NULL;
	g_db2API.SQLSetDescRec		= NULL;
	g_db2API.SQLSetEnvAttr		= NULL;
	g_db2API.SQLSetParam		= NULL;
	g_db2API.SQLSetPos			= NULL;
	g_db2API.SQLSetStmtAttr		= NULL;
	g_db2API.SQLSetStmtOption	= NULL;
	g_db2API.SQLSpecialColumns	= NULL;
	g_db2API.SQLStatistics		= NULL;
	g_db2API.SQLTablePrivileges	= NULL;
	g_db2API.SQLTables			= NULL;
	g_db2API.SQLTransact		= NULL;
}

void AddDB2Support(const SAConnection * pCon)
{
	SACriticalSectionScope scope(&db2LoaderMutex);

	if(!g_hDB2DLL)
	{
		// load DB2 API library
		SAString sErrorMessage, sLibName, sLibsList = pCon->Option(_TSA("DB2CLI.LIBS"));
		if( sLibsList.IsEmpty() )
			sLibsList = g_sDB2DLLNames;

		g_hDB2DLL = SALoadLibraryFromList(sLibsList, sErrorMessage, sLibName, RTLD_LAZY|RTLD_GLOBAL);
		if( ! g_hDB2DLL )
			throw SAException(SA_Library_Error, SA_Library_Error_LoadLibraryFails, -1, IDS_LOAD_LIBRARY_FAILS, (const SAChar*)sErrorMessage);

		g_nDB2DLLVersionLoaded = 0l;	// Version unknown

		LoadAPI();
	}

	if( SAGlobals::UnloadAPI() )
		g_nDB2DLLRefs++;
	else
		g_nDB2DLLRefs = 1;
}

void ReleaseDB2Support()
{
	SACriticalSectionScope scope(&db2LoaderMutex);

	assert(g_nDB2DLLRefs > 0);
	g_nDB2DLLRefs--;
	if(!g_nDB2DLLRefs)
	{
		g_nDB2DLLVersionLoaded = 0l;
		ResetAPI();

		// can hand app
		//::dlclose(g_hDB2DLL);
		g_hDB2DLL = NULL;
	}
}

