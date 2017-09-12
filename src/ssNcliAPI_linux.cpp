// odbcAPI.cpp
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"

#define _SQLNCLI_ODBC_ 1

#include <ssNcliAPI.h>

static const SAChar* g_sssNCliDLLNames = _TSA("/opt/microsoft/msodbcsql/11.0.2270.0/lib64/libmsodbcsql-11.0.so.2270.0:libmsodbcsql.so");
static void* g_hssNCliDLL = NULL;
static long g_nssNCliDLLRefs = 0l;

static SAMutex ssNCliLoaderMutex;

// API definitions
ssNCliAPI g_ssNCliAPI;

ssNCliAPI::ssNCliAPI()
{
	this->SQLAllocHandle = NULL;
	this->SQLBindCol = NULL;
	this->SQLBindParameter = NULL;
	this->SQLBrowseConnectW = NULL;
	this->SQLBulkOperations = NULL;
	this->SQLCancel = NULL;
	this->SQLCloseCursor = NULL;
	this->SQLColAttributeW = NULL;
	this->SQLColumnPrivilegesW = NULL;
	this->SQLColumnsW = NULL;
	this->SQLConnectW = NULL;
	this->SQLCopyDesc = NULL;
	this->SQLDescribeColW = NULL;
	this->SQLDescribeParam = NULL;
	this->SQLDisconnect = NULL;
	this->SQLDriverConnectW = NULL;
	this->SQLEndTran = NULL;
	this->SQLExecDirectW = NULL;
	this->SQLExecute = NULL;
	this->SQLExtendedFetch = NULL;
	this->SQLFetch = NULL;
	this->SQLFetchScroll = NULL;
	this->SQLForeignKeysW = NULL;
	this->SQLFreeHandle = NULL;
	this->SQLFreeStmt = NULL;
	this->SQLGetConnectAttrW = NULL;
	this->SQLGetConnectOptionW = NULL;
	this->SQLGetCursorNameW = NULL;
	this->SQLGetData = NULL;
	this->SQLGetDescFieldW = NULL;
	this->SQLGetDescRecW = NULL;
	this->SQLGetDiagFieldW = NULL;
	this->SQLGetDiagRecW = NULL;
	this->SQLGetEnvAttr = NULL;
	this->SQLGetFunctions = NULL;
	this->SQLGetInfoW = NULL;
	this->SQLGetStmtAttrW = NULL;
	this->SQLGetTypeInfoW = NULL;
	this->SQLMoreResults = NULL;
	this->SQLNativeSqlW = NULL;
	this->SQLNumParams = NULL;
	this->SQLNumResultCols = NULL;
	this->SQLParamData = NULL;
	this->SQLParamOptions = NULL;
	this->SQLPrepareW = NULL;
	this->SQLPrimaryKeysW = NULL;
	this->SQLProcedureColumnsW = NULL;
	this->SQLProceduresW = NULL;
	this->SQLPutData = NULL;
	this->SQLRowCount = NULL;
	this->SQLSetConnectAttrW = NULL;
	this->SQLSetConnectOptionW = NULL;
	this->SQLSetCursorNameW = NULL;
	this->SQLSetDescFieldW = NULL;
	this->SQLSetDescRec = NULL;
	this->SQLSetEnvAttr = NULL;
	this->SQLSetPos = NULL;
	this->SQLSetScrollOptions = NULL;
	this->SQLSetStmtAttrW = NULL;
	this->SQLSpecialColumnsW = NULL;
	this->SQLStatisticsW = NULL;
	this->SQLTablePrivilegesW = NULL;
	this->SQLTablesW = NULL;

	//this->OpenSqlFilestream = NULL;
}

ssNCliConnectionHandles::ssNCliConnectionHandles()
{
	m_hevn = NULL;
	m_hdbc = NULL;
}

ssNCliCommandHandles::ssNCliCommandHandles()
{
	m_hstmt = NULL;
}

static void LoadAPI()
{
	g_ssNCliAPI.SQLAllocHandle		= (SQLAllocHandle_t)::dlsym(g_hssNCliDLL, "SQLAllocHandle");				assert(g_ssNCliAPI.SQLAllocHandle != NULL);
	g_ssNCliAPI.SQLBindCol			= (SQLBindCol_t)::dlsym(g_hssNCliDLL, "SQLBindCol");						assert(g_ssNCliAPI.SQLBindCol != NULL);
	g_ssNCliAPI.SQLBindParameter		= (SQLBindParameter_t)::dlsym(g_hssNCliDLL, "SQLBindParameter");			assert(g_ssNCliAPI.SQLBindParameter != NULL);
	g_ssNCliAPI.SQLBrowseConnectW		= (SQLBrowseConnectW_t)::dlsym(g_hssNCliDLL, "SQLBrowseConnectW");			// assert(g_ssNCliAPI.SQLBrowseConnectW != NULL);
	g_ssNCliAPI.SQLBulkOperations		= (SQLBulkOperations_t)::dlsym(g_hssNCliDLL, "SQLBulkOperations");		assert(g_ssNCliAPI.SQLBulkOperations != NULL);
	g_ssNCliAPI.SQLCancel				= (SQLCancel_t)::dlsym(g_hssNCliDLL, "SQLCancel");						assert(g_ssNCliAPI.SQLCancel != NULL);
	g_ssNCliAPI.SQLCloseCursor		= (SQLCloseCursor_t)::dlsym(g_hssNCliDLL, "SQLCloseCursor");				assert(g_ssNCliAPI.SQLCloseCursor != NULL);
	g_ssNCliAPI.SQLColAttributeW		= (SQLColAttributeW_t)::dlsym(g_hssNCliDLL, "SQLColAttributeW");				assert(g_ssNCliAPI.SQLColAttributeW != NULL);
	g_ssNCliAPI.SQLColumnPrivilegesW		= (SQLColumnPrivilegesW_t)::dlsym(g_hssNCliDLL, "SQLColumnPrivilegesW");				assert(g_ssNCliAPI.SQLColumnPrivilegesW != NULL);
	g_ssNCliAPI.SQLColumnsW		= (SQLColumnsW_t)::dlsym(g_hssNCliDLL, "SQLColumnsW");				assert(g_ssNCliAPI.SQLColumnsW != NULL);
	g_ssNCliAPI.SQLConnectW		= (SQLConnectW_t)::dlsym(g_hssNCliDLL, "SQLConnectW");				assert(g_ssNCliAPI.SQLConnectW != NULL);
	g_ssNCliAPI.SQLCopyDesc			= (SQLCopyDesc_t)::dlsym(g_hssNCliDLL, "SQLCopyDesc");					assert(g_ssNCliAPI.SQLCopyDesc != NULL);
	g_ssNCliAPI.SQLDescribeColW		= (SQLDescribeColW_t)::dlsym(g_hssNCliDLL, "SQLDescribeColW");				assert(g_ssNCliAPI.SQLDescribeColW != NULL);
	g_ssNCliAPI.SQLDescribeParam		= (SQLDescribeParam_t)::dlsym(g_hssNCliDLL, "SQLDescribeParam");			assert(g_ssNCliAPI.SQLDescribeParam != NULL);
	g_ssNCliAPI.SQLDisconnect			= (SQLDisconnect_t)::dlsym(g_hssNCliDLL, "SQLDisconnect");				assert(g_ssNCliAPI.SQLDisconnect != NULL);
	g_ssNCliAPI.SQLDriverConnectW		= (SQLDriverConnectW_t)::dlsym(g_hssNCliDLL, "SQLDriverConnectW");			assert(g_ssNCliAPI.SQLDriverConnectW != NULL);
	g_ssNCliAPI.SQLEndTran			= (SQLEndTran_t)::dlsym(g_hssNCliDLL, "SQLEndTran");						assert(g_ssNCliAPI.SQLEndTran != NULL);
	g_ssNCliAPI.SQLExecDirectW			= (SQLExecDirectW_t)::dlsym(g_hssNCliDLL, "SQLExecDirectW");						assert(g_ssNCliAPI.SQLExecDirectW != NULL);
	g_ssNCliAPI.SQLExecute			= (SQLExecute_t)::dlsym(g_hssNCliDLL, "SQLExecute");						assert(g_ssNCliAPI.SQLExecute != NULL);
	g_ssNCliAPI.SQLExtendedFetch		= (SQLExtendedFetch_t)::dlsym(g_hssNCliDLL, "SQLExtendedFetch");			assert(g_ssNCliAPI.SQLExtendedFetch != NULL);
	g_ssNCliAPI.SQLFetch				= (SQLFetch_t)::dlsym(g_hssNCliDLL, "SQLFetch");							assert(g_ssNCliAPI.SQLFetch != NULL);
	g_ssNCliAPI.SQLFetchScroll		= (SQLFetchScroll_t)::dlsym(g_hssNCliDLL, "SQLFetchScroll");				assert(g_ssNCliAPI.SQLFetchScroll != NULL);
	g_ssNCliAPI.SQLForeignKeysW		= (SQLForeignKeysW_t)::dlsym(g_hssNCliDLL, "SQLForeignKeysW");				assert(g_ssNCliAPI.SQLForeignKeysW != NULL);
	g_ssNCliAPI.SQLFreeHandle			= (SQLFreeHandle_t)::dlsym(g_hssNCliDLL, "SQLFreeHandle");				assert(g_ssNCliAPI.SQLFreeHandle != NULL);
	g_ssNCliAPI.SQLFreeStmt			= (SQLFreeStmt_t)::dlsym(g_hssNCliDLL, "SQLFreeStmt");					assert(g_ssNCliAPI.SQLFreeStmt != NULL);
	g_ssNCliAPI.SQLGetConnectAttrW		= (SQLGetConnectAttrW_t)::dlsym(g_hssNCliDLL, "SQLGetConnectAttrW");		assert(g_ssNCliAPI.SQLGetConnectAttrW != NULL);
	g_ssNCliAPI.SQLGetConnectOptionW		= (SQLGetConnectOptionW_t)::dlsym(g_hssNCliDLL, "SQLGetConnectOptionW");		assert(g_ssNCliAPI.SQLGetConnectOptionW != NULL);
	g_ssNCliAPI.SQLGetCursorNameW		= (SQLGetCursorNameW_t)::dlsym(g_hssNCliDLL, "SQLGetCursorNameW");		assert(g_ssNCliAPI.SQLGetCursorNameW != NULL);
	g_ssNCliAPI.SQLGetData			= (SQLGetData_t)::dlsym(g_hssNCliDLL, "SQLGetData");						assert(g_ssNCliAPI.SQLGetData!= NULL);
	g_ssNCliAPI.SQLGetDescFieldW			= (SQLGetDescFieldW_t)::dlsym(g_hssNCliDLL, "SQLGetDescFieldW");						assert(g_ssNCliAPI.SQLGetDescFieldW!= NULL);
	g_ssNCliAPI.SQLGetDescRecW			= (SQLGetDescRecW_t)::dlsym(g_hssNCliDLL, "SQLGetDescRecW");						assert(g_ssNCliAPI.SQLGetDescRecW!= NULL);
	g_ssNCliAPI.SQLGetDiagFieldW	= (SQLGetDiagFieldW_t)::dlsym(g_hssNCliDLL, "SQLGetDiagFieldW");						assert(g_ssNCliAPI.SQLGetDiagFieldW!= NULL);
	g_ssNCliAPI.SQLGetDiagRecW			= (SQLGetDiagRecW_t)::dlsym(g_hssNCliDLL, "SQLGetDiagRecW");				assert(g_ssNCliAPI.SQLGetDiagRecW != NULL);
	g_ssNCliAPI.SQLGetEnvAttr			= (SQLGetEnvAttr_t)::dlsym(g_hssNCliDLL, "SQLGetEnvAttr");				assert(g_ssNCliAPI.SQLGetEnvAttr != NULL);
	g_ssNCliAPI.SQLGetFunctions		= (SQLGetFunctions_t)::dlsym(g_hssNCliDLL, "SQLGetFunctions");			assert(g_ssNCliAPI.SQLGetFunctions != NULL);
	g_ssNCliAPI.SQLGetInfoW			= (SQLGetInfoW_t)::dlsym(g_hssNCliDLL, "SQLGetInfoW");						assert(g_ssNCliAPI.SQLGetInfoW != NULL);
	g_ssNCliAPI.SQLGetStmtAttrW		= (SQLGetStmtAttrW_t)::dlsym(g_hssNCliDLL, "SQLGetStmtAttrW");				assert(g_ssNCliAPI.SQLGetStmtAttrW != NULL);
	g_ssNCliAPI.SQLGetTypeInfoW		= (SQLGetTypeInfoW_t)::dlsym(g_hssNCliDLL, "SQLGetTypeInfoW");				assert(g_ssNCliAPI.SQLGetTypeInfoW != NULL);
	g_ssNCliAPI.SQLMoreResults		= (SQLMoreResults_t)::dlsym(g_hssNCliDLL, "SQLMoreResults");				assert(g_ssNCliAPI.SQLMoreResults != NULL);
	g_ssNCliAPI.SQLNativeSqlW		= (SQLNativeSqlW_t)::dlsym(g_hssNCliDLL, "SQLNativeSqlW");				assert(g_ssNCliAPI.SQLNativeSqlW != NULL);
	g_ssNCliAPI.SQLNumParams			= (SQLNumParams_t)::dlsym(g_hssNCliDLL, "SQLNumParams");					assert(g_ssNCliAPI.SQLNumParams != NULL);
	g_ssNCliAPI.SQLNumResultCols		= (SQLNumResultCols_t)::dlsym(g_hssNCliDLL, "SQLNumResultCols");			assert(g_ssNCliAPI.SQLNumResultCols != NULL);
	g_ssNCliAPI.SQLParamData			= (SQLParamData_t)::dlsym(g_hssNCliDLL, "SQLParamData");					assert(g_ssNCliAPI.SQLParamData != NULL);
	g_ssNCliAPI.SQLParamOptions		= (SQLParamOptions_t)::dlsym(g_hssNCliDLL, "SQLParamOptions");			assert(g_ssNCliAPI.SQLParamOptions != NULL);
	g_ssNCliAPI.SQLPrepareW			= (SQLPrepareW_t)::dlsym(g_hssNCliDLL, "SQLPrepareW");						assert(g_ssNCliAPI.SQLPrepareW != NULL);
	g_ssNCliAPI.SQLPrimaryKeysW			= (SQLPrimaryKeysW_t)::dlsym(g_hssNCliDLL, "SQLPrimaryKeysW");						assert(g_ssNCliAPI.SQLPrimaryKeysW != NULL);
	g_ssNCliAPI.SQLProcedureColumnsW	= (SQLProcedureColumnsW_t)::dlsym(g_hssNCliDLL, "SQLProcedureColumnsW");	assert(g_ssNCliAPI.SQLProcedureColumnsW != NULL);
	g_ssNCliAPI.SQLProceduresW			= (SQLProceduresW_t)::dlsym(g_hssNCliDLL, "SQLProceduresW");				assert(g_ssNCliAPI.SQLProceduresW != NULL);
	g_ssNCliAPI.SQLPutData			= (SQLPutData_t)::dlsym(g_hssNCliDLL, "SQLPutData");						assert(g_ssNCliAPI.SQLPutData != NULL);
	g_ssNCliAPI.SQLRowCount			= (SQLRowCount_t)::dlsym(g_hssNCliDLL, "SQLRowCount");					assert(g_ssNCliAPI.SQLRowCount != NULL);
	g_ssNCliAPI.SQLSetConnectAttrW		= (SQLSetConnectAttrW_t)::dlsym(g_hssNCliDLL, "SQLSetConnectAttrW");		assert(g_ssNCliAPI.SQLSetConnectAttrW != NULL);
	g_ssNCliAPI.SQLSetConnectOptionW	= (SQLSetConnectOptionW_t)::dlsym(g_hssNCliDLL, "SQLSetConnectOptionW");	assert(g_ssNCliAPI.SQLSetConnectOptionW != NULL);
	g_ssNCliAPI.SQLSetCursorNameW		= (SQLSetCursorNameW_t)::dlsym(g_hssNCliDLL, "SQLSetCursorNameW");			assert(g_ssNCliAPI.SQLSetCursorNameW != NULL);
	g_ssNCliAPI.SQLSetDescFieldW		= (SQLSetDescFieldW_t)::dlsym(g_hssNCliDLL, "SQLSetDescFieldW");			assert(g_ssNCliAPI.SQLSetDescFieldW != NULL);
	g_ssNCliAPI.SQLSetDescRec			= (SQLSetDescRec_t)::dlsym(g_hssNCliDLL, "SQLSetDescRec");				assert(g_ssNCliAPI.SQLSetDescRec != NULL);
	g_ssNCliAPI.SQLSetEnvAttr			= (SQLSetEnvAttr_t)::dlsym(g_hssNCliDLL, "SQLSetEnvAttr");				assert(g_ssNCliAPI.SQLSetEnvAttr != NULL);
	g_ssNCliAPI.SQLSetPos				= (SQLSetPos_t)::dlsym(g_hssNCliDLL, "SQLSetPos");						assert(g_ssNCliAPI.SQLSetPos != NULL);
	g_ssNCliAPI.SQLSetScrollOptions	= (SQLSetScrollOptions_t)::dlsym(g_hssNCliDLL, "SQLSetScrollOptions");	assert(g_ssNCliAPI.SQLSetScrollOptions != NULL);
	g_ssNCliAPI.SQLSetStmtAttrW		= (SQLSetStmtAttrW_t)::dlsym(g_hssNCliDLL, "SQLSetStmtAttrW");				assert(g_ssNCliAPI.SQLSetStmtAttrW != NULL);
	g_ssNCliAPI.SQLSpecialColumnsW		= (SQLSpecialColumnsW_t)::dlsym(g_hssNCliDLL, "SQLSpecialColumnsW");		assert(g_ssNCliAPI.SQLSpecialColumnsW != NULL);
	g_ssNCliAPI.SQLStatisticsW			= (SQLStatisticsW_t)::dlsym(g_hssNCliDLL, "SQLStatisticsW");				assert(g_ssNCliAPI.SQLStatisticsW != NULL);
	g_ssNCliAPI.SQLTablePrivilegesW	= (SQLTablePrivilegesW_t)::dlsym(g_hssNCliDLL, "SQLTablePrivilegesW");		assert(g_ssNCliAPI.SQLTablePrivilegesW != NULL);
	g_ssNCliAPI.SQLTablesW				= (SQLTablesW_t)::dlsym(g_hssNCliDLL, "SQLTablesW");						assert(g_ssNCliAPI.SQLTablesW != NULL);
	//g_ssNCliAPI.OpenSqlFilestream		= (OpenSqlFilestream_t)::dlsym(g_hssNCliDLL, "OpenSqlFilestream");						assert(g_ssNCliAPI.SQLTablesW != NULL);
}

static void ResetAPI()
{
	g_ssNCliAPI.SQLAllocHandle = NULL;
	g_ssNCliAPI.SQLBindCol = NULL;
	g_ssNCliAPI.SQLBindParameter = NULL;
	g_ssNCliAPI.SQLBrowseConnectW = NULL;
	g_ssNCliAPI.SQLBulkOperations = NULL;
	g_ssNCliAPI.SQLCancel = NULL;
	g_ssNCliAPI.SQLCloseCursor = NULL;
	g_ssNCliAPI.SQLColAttributeW = NULL;
	g_ssNCliAPI.SQLColumnPrivilegesW = NULL;
	g_ssNCliAPI.SQLColumnsW = NULL;
	g_ssNCliAPI.SQLConnectW = NULL;
	g_ssNCliAPI.SQLCopyDesc = NULL;
	g_ssNCliAPI.SQLDescribeColW = NULL;
	g_ssNCliAPI.SQLDescribeParam = NULL;
	g_ssNCliAPI.SQLDisconnect = NULL;
	g_ssNCliAPI.SQLDriverConnectW = NULL;
	g_ssNCliAPI.SQLEndTran = NULL;
	g_ssNCliAPI.SQLExecDirectW = NULL;
	g_ssNCliAPI.SQLExecute = NULL;
	g_ssNCliAPI.SQLExtendedFetch = NULL;
	g_ssNCliAPI.SQLFetch = NULL;
	g_ssNCliAPI.SQLFetchScroll = NULL;
	g_ssNCliAPI.SQLForeignKeysW = NULL;
	g_ssNCliAPI.SQLFreeHandle = NULL;
	g_ssNCliAPI.SQLFreeStmt = NULL;
	g_ssNCliAPI.SQLGetConnectAttrW = NULL;
	g_ssNCliAPI.SQLGetConnectOptionW = NULL;
	g_ssNCliAPI.SQLGetCursorNameW = NULL;
	g_ssNCliAPI.SQLGetData = NULL;
	g_ssNCliAPI.SQLGetDescFieldW = NULL;
	g_ssNCliAPI.SQLGetDescRecW = NULL;
	g_ssNCliAPI.SQLGetDiagFieldW = NULL;
	g_ssNCliAPI.SQLGetDiagRecW = NULL;
	g_ssNCliAPI.SQLGetEnvAttr = NULL;
	g_ssNCliAPI.SQLGetFunctions = NULL;
	g_ssNCliAPI.SQLGetInfoW = NULL;
	g_ssNCliAPI.SQLGetStmtAttrW = NULL;
	g_ssNCliAPI.SQLGetTypeInfoW = NULL;
	g_ssNCliAPI.SQLMoreResults = NULL;
	g_ssNCliAPI.SQLNativeSqlW = NULL;
	g_ssNCliAPI.SQLNumParams = NULL;
	g_ssNCliAPI.SQLNumResultCols = NULL;
	g_ssNCliAPI.SQLParamData = NULL;
	g_ssNCliAPI.SQLParamOptions = NULL;
	g_ssNCliAPI.SQLPrepareW = NULL;
	g_ssNCliAPI.SQLPrimaryKeysW = NULL;
	g_ssNCliAPI.SQLProcedureColumnsW = NULL;
	g_ssNCliAPI.SQLProceduresW = NULL;
	g_ssNCliAPI.SQLPutData = NULL;
	g_ssNCliAPI.SQLRowCount = NULL;
	g_ssNCliAPI.SQLSetConnectAttrW = NULL;
	g_ssNCliAPI.SQLSetConnectOptionW = NULL;
	g_ssNCliAPI.SQLSetCursorNameW = NULL;
	g_ssNCliAPI.SQLSetDescFieldW = NULL;
	g_ssNCliAPI.SQLSetDescRec = NULL;
	g_ssNCliAPI.SQLSetEnvAttr = NULL;
	g_ssNCliAPI.SQLSetPos = NULL;
	g_ssNCliAPI.SQLSetScrollOptions = NULL;
	g_ssNCliAPI.SQLSetStmtAttrW = NULL;
	g_ssNCliAPI.SQLSpecialColumnsW = NULL;
	g_ssNCliAPI.SQLStatisticsW = NULL;
	g_ssNCliAPI.SQLTablePrivilegesW = NULL;
	g_ssNCliAPI.SQLTablesW = NULL;

	//g_ssNCliAPI.OpenSqlFilestream = NULL;
}

void AddNCliSupport(const SAConnection *pCon)
{
	SACriticalSectionScope scope(&ssNCliLoaderMutex);

	if( ! g_nssNCliDLLRefs )
	{
		SAString sErrorMessage, sLibName, sLibsList = pCon->Option(_TSA("SQLNCLI.LIBS"));
		if( sLibsList.IsEmpty() )
			sLibsList = g_sssNCliDLLNames;

		g_hssNCliDLL = SALoadLibraryFromList(sLibsList, sErrorMessage, sLibName, RTLD_LAZY | RTLD_GLOBAL);
		if( ! g_hssNCliDLL )
			throw SAException(SA_Library_Error, SA_Library_Error_LoadLibraryFails, -1, IDS_LOAD_LIBRARY_FAILS, (const SAChar*)sErrorMessage);

		LoadAPI();
	}

	if( SAGlobals::UnloadAPI() )
		g_nssNCliDLLRefs++;
	else
		g_nssNCliDLLRefs = 1;
}

void ReleaseNCliSupport()
{
	SACriticalSectionScope scope(&ssNCliLoaderMutex);

	assert(g_nssNCliDLLRefs > 0);
	g_nssNCliDLLRefs--;
	if( ! g_nssNCliDLLRefs )
	{
		ResetAPI();

		::dlclose(g_hssNCliDLL);
		g_hssNCliDLL = NULL;
	}
}

