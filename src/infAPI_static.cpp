#include "osdep.h"
#include <infAPI.h>

long g_nInfDLLVersionLoaded = 0l;
static long g_nInfDLLRefs = 0l;

static SAMutex infLoaderMutex;

// API definitions
infAPI g_infAPI;

infAPI::infAPI()
{
	SQLAllocConnect		= NULL;
	SQLAllocEnv			= NULL;
	SQLAllocHandle		= NULL;
	SQLAllocStmt		= NULL;
	SQLBindCol			= NULL;
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
	SQLExtendedFetch	= NULL;
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
	SQLGetStmtAttr		= NULL;
	SQLGetStmtOption	= NULL;
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
	SQLSetConnectAttr	= NULL;
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

infConnectionHandles::infConnectionHandles()
{
	m_hevn = (SQLHENV)NULL;
	m_hdbc = (SQLHDBC)NULL;
}

infCommandHandles::infCommandHandles()
{
	m_hstmt = (SQLHSTMT)NULL;
}

static void LoadAPI()
{
	// 11.70 doesn't have it! g_infAPI.SQLAllocConnect	= SQLAllocConnect;
	// 11.70 doesn't have it! g_infAPI.SQLAllocEnv	= SQLAllocEnv;
	g_infAPI.SQLAllocHandle	= SQLAllocHandle;
	// 11.70 doesn't have it! g_infAPI.SQLAllocStmt	= SQLAllocStmt;
	g_infAPI.SQLBindCol	= SQLBindCol;
	g_infAPI.SQLBindParameter	= SQLBindParameter;
	g_infAPI.SQLBrowseConnect	= 
#ifdef SA_UNICODE
		SQLBrowseConnectW;
#else
		SQLBrowseConnect;
#endif
	g_infAPI.SQLBulkOperations	= SQLBulkOperations;
	g_infAPI.SQLCancel	= SQLCancel;
	g_infAPI.SQLCloseCursor	= SQLCloseCursor;
	g_infAPI.SQLColAttribute	=
#ifdef SA_UNICODE
		SQLColAttributeW;
#else
		SQLColAttribute;
#endif
	// 11.70 doesn't have it!
	/*
	g_infAPI.SQLColAttributes =
#ifdef SA_UNICODE
		SQLColAttributesW;
#else
		SQLColAttributes;
#endif
	*/
	g_infAPI.SQLColumnPrivileges	=
#ifdef SA_UNICODE
		SQLColumnPrivilegesW;
#else
		SQLColumnPrivileges;
#endif
	g_infAPI.SQLColumns	=
#ifdef SA_UNICODE
		SQLColumnsW;
#else
		SQLColumns;
#endif
	g_infAPI.SQLConnect	=
#ifdef SA_UNICODE
		SQLConnectW;
#else
		SQLConnect;
#endif
	g_infAPI.SQLCopyDesc	= SQLCopyDesc;
	// 11.70 doesn't have it!
	/*
	g_infAPI.SQLDataSources	=
#ifdef SA_UNICODE
		SQLDataSourcesW;
#else
		SQLDataSources;
#endif
	*/
	g_infAPI.SQLDescribeCol	=
#ifdef SA_UNICODE
		SQLDescribeColW;
#else
		SQLDescribeCol;
#endif
	g_infAPI.SQLDescribeParam	= SQLDescribeParam;
	g_infAPI.SQLDisconnect	= SQLDisconnect;
	g_infAPI.SQLDriverConnect	=
#ifdef SA_UNICODE
		SQLDriverConnectW;
#else
		SQLDriverConnect;
#endif
	g_infAPI.SQLEndTran	= SQLEndTran;
	// 11.70 doesn't have it!
	/*
	g_infAPI.SQLError	=
#ifdef SA_UNICODE
		SQLErrorW;
#else
		SQLError;
#endif
	*/
	g_infAPI.SQLExecDirect	=
#ifdef SA_UNICODE
		SQLExecDirectW;
#else
		SQLExecDirect;
#endif
	g_infAPI.SQLExecute	= SQLExecute;
	g_infAPI.SQLFetch	= SQLFetch;
	g_infAPI.SQLFetchScroll	= SQLFetchScroll;
	g_infAPI.SQLForeignKeys	=
#ifdef SA_UNICODE
		SQLForeignKeysW;
#else
		SQLForeignKeys;
#endif
	// 11.70 doesn't have it! g_infAPI.SQLFreeConnect	= SQLFreeConnect;
	// 11.70 doesn't have it! g_infAPI.SQLFreeEnv	= SQLFreeEnv;
	g_infAPI.SQLFreeHandle	= SQLFreeHandle;
	g_infAPI.SQLFreeStmt	= SQLFreeStmt;
	g_infAPI.SQLGetConnectAttr	=
#ifdef SA_UNICODE
		SQLGetConnectAttrW;
#else
		SQLGetConnectAttr;
#endif
	// 11.70 doesn't have it!
	/*
	g_infAPI.SQLGetConnectOption	=
#ifdef SA_UNICODE
		SQLGetConnectOptionW;
#else
		SQLGetConnectOption;
#endif
	*/
	g_infAPI.SQLGetCursorName	=
#ifdef SA_UNICODE
		SQLGetCursorNameW;
#else
		SQLGetCursorName;
#endif
	g_infAPI.SQLGetData	= SQLGetData;
	g_infAPI.SQLGetDescField	=
#ifdef SA_UNICODE
		SQLGetDescFieldW;
#else
		SQLGetDescField;
#endif
	g_infAPI.SQLGetDescRec	=
#ifdef SA_UNICODE
		 SQLGetDescRecW;
#else
		 SQLGetDescRec;
#endif
	g_infAPI.SQLGetDiagField	=
#ifdef SA_UNICODE
		SQLGetDiagFieldW;
#else
		SQLGetDiagField;
#endif
	g_infAPI.SQLGetDiagRec	=
#ifdef SA_UNICODE
		SQLGetDiagRecW;
#else
		SQLGetDiagRec;
#endif
	g_infAPI.SQLGetEnvAttr	= SQLGetEnvAttr;
	g_infAPI.SQLGetFunctions	= SQLGetFunctions;
	g_infAPI.SQLGetInfo	=
#ifdef SA_UNICODE
		SQLGetInfoW;
#else
		SQLGetInfo;
#endif
	g_infAPI.SQLGetStmtAttr	=
#ifdef SA_UNICODE
		SQLGetStmtAttrW;
#else
		SQLGetStmtAttr;
#endif
	// 11.70 doesn't have it! g_infAPI.SQLGetStmtOption	= SQLGetStmtOption;
	g_infAPI.SQLGetTypeInfo	= SQLGetTypeInfo;
	g_infAPI.SQLMoreResults	= SQLMoreResults;
	g_infAPI.SQLNativeSql	=
#ifdef SA_UNICODE
		SQLNativeSqlW;
#else
		SQLNativeSql;
#endif
	g_infAPI.SQLNumParams	= SQLNumParams;
	g_infAPI.SQLNumResultCols	= SQLNumResultCols;
	g_infAPI.SQLParamData	= SQLParamData;
	// 11.70 doesn't have it! g_infAPI.SQLParamOptions	= SQLParamOptions;
	g_infAPI.SQLPrepare	=
#ifdef SA_UNICODE
		SQLPrepareW;
#else
		SQLPrepare;
#endif
	g_infAPI.SQLPrimaryKeys	=
#ifdef SA_UNICODE
		SQLPrimaryKeysW;
#else
		SQLPrimaryKeys;
#endif
	g_infAPI.SQLProcedureColumns	=
#ifdef SA_UNICODE
		SQLProcedureColumnsW;
#else
		SQLProcedureColumns;
#endif
	g_infAPI.SQLProcedures	=
#ifdef SA_UNICODE
		SQLProceduresW;
#else
		SQLProcedures;
#endif
	g_infAPI.SQLPutData	= SQLPutData;
	g_infAPI.SQLRowCount	= SQLRowCount;
	g_infAPI.SQLSetConnectAttr	=
#ifdef SA_UNICODE
		SQLSetConnectAttrW;
#else
		SQLSetConnectAttr;
#endif
	// 11.70 doesn't have it!
	/*
	g_infAPI.SQLSetConnectOption	=
#ifdef SA_UNICODE
		SQLSetConnectOptionW;
#else
		SQLSetConnectOption;
#endif
	*/
	g_infAPI.SQLSetCursorName	=
#ifdef SA_UNICODE
		SQLSetCursorNameW;
#else
		SQLSetCursorName;
#endif
	g_infAPI.SQLSetDescField	=
#ifdef SA_UNICODE
		SQLSetDescFieldW;
#else
		SQLSetDescField;
#endif
	g_infAPI.SQLSetDescRec	= SQLSetDescRec;
	g_infAPI.SQLSetEnvAttr	= SQLSetEnvAttr;
	// 11.70 doesn't have it! g_infAPI.SQLSetParam	= SQLSetParam;
	g_infAPI.SQLSetPos	= SQLSetPos;
	g_infAPI.SQLSetStmtAttr	=
#ifdef SA_UNICODE
		SQLSetStmtAttrW;
#else
		SQLSetStmtAttr;
#endif
	// 11.70 doesn't have it! g_infAPI.SQLSetStmtOption	= SQLSetStmtOption;
	g_infAPI.SQLSpecialColumns	=
#ifdef SA_UNICODE
		SQLSpecialColumnsW;
#else
		SQLSpecialColumns;
#endif
	g_infAPI.SQLStatistics	=
#ifdef SA_UNICODE
		SQLStatisticsW;
#else
		SQLStatistics;
#endif
	g_infAPI.SQLTablePrivileges	=
#ifdef SA_UNICODE
		SQLTablePrivilegesW;
#else
		SQLTablePrivileges;
#endif
	g_infAPI.SQLTables	=
#ifdef SA_UNICODE
		SQLTablesW;
#else
		SQLTables;
#endif
	// 11.70 doesn't have it! g_infAPI.SQLTransact	= SQLTransact;
}

static void ResetAPI()
{
	g_infAPI.SQLAllocConnect	= NULL;
	g_infAPI.SQLAllocEnv		= NULL;
	g_infAPI.SQLAllocHandle		= NULL;
	g_infAPI.SQLAllocStmt		= NULL;
	g_infAPI.SQLBindCol			= NULL;
	g_infAPI.SQLBindParameter	= NULL;
	g_infAPI.SQLBrowseConnect	= NULL;
	g_infAPI.SQLBulkOperations	= NULL;
	g_infAPI.SQLCancel			= NULL;
	g_infAPI.SQLCloseCursor		= NULL;
	g_infAPI.SQLColAttribute	= NULL;
	g_infAPI.SQLColAttributes	= NULL;
	g_infAPI.SQLColumnPrivileges= NULL;
	g_infAPI.SQLColumns			= NULL;
	g_infAPI.SQLConnect			= NULL;
	g_infAPI.SQLCopyDesc		= NULL;
	g_infAPI.SQLDataSources		= NULL;
	g_infAPI.SQLDescribeCol		= NULL;
	g_infAPI.SQLDescribeParam	= NULL;
	g_infAPI.SQLDisconnect		= NULL;
	g_infAPI.SQLDriverConnect	= NULL;
	g_infAPI.SQLEndTran			= NULL;
	g_infAPI.SQLError			= NULL;
	g_infAPI.SQLExecDirect		= NULL;
	g_infAPI.SQLExecute			= NULL;
	g_infAPI.SQLFetch			= NULL;
	g_infAPI.SQLFetchScroll		= NULL;
	g_infAPI.SQLForeignKeys		= NULL;
	g_infAPI.SQLFreeConnect		= NULL;
	g_infAPI.SQLFreeEnv			= NULL;
	g_infAPI.SQLFreeHandle		= NULL;
	g_infAPI.SQLFreeStmt		= NULL;
	g_infAPI.SQLGetConnectAttr	= NULL;
	g_infAPI.SQLGetConnectOption= NULL;
	g_infAPI.SQLGetCursorName	= NULL;
	g_infAPI.SQLGetData			= NULL;
	g_infAPI.SQLGetDescField	= NULL;
	g_infAPI.SQLGetDescRec		= NULL;
	g_infAPI.SQLGetDiagField	= NULL;
	g_infAPI.SQLGetDiagRec		= NULL;
	g_infAPI.SQLGetEnvAttr		= NULL;
	g_infAPI.SQLGetFunctions	= NULL;
	g_infAPI.SQLGetInfo			= NULL;
	g_infAPI.SQLGetStmtAttr		= NULL;
	g_infAPI.SQLGetStmtOption	= NULL;
	g_infAPI.SQLGetTypeInfo		= NULL;
	g_infAPI.SQLMoreResults		= NULL;
	g_infAPI.SQLNativeSql		= NULL;
	g_infAPI.SQLNumParams		= NULL;
	g_infAPI.SQLNumResultCols	= NULL;
	g_infAPI.SQLParamData		= NULL;
	g_infAPI.SQLParamOptions	= NULL;
	g_infAPI.SQLPrepare			= NULL;
	g_infAPI.SQLPrimaryKeys		= NULL;
	g_infAPI.SQLProcedureColumns= NULL;
	g_infAPI.SQLProcedures		= NULL;
	g_infAPI.SQLPutData			= NULL;
	g_infAPI.SQLRowCount		= NULL;
	g_infAPI.SQLSetConnectAttr	= NULL;
	g_infAPI.SQLSetConnectOption= NULL;
	g_infAPI.SQLSetCursorName	= NULL;
	g_infAPI.SQLSetDescField	= NULL;
	g_infAPI.SQLSetDescRec		= NULL;
	g_infAPI.SQLSetEnvAttr		= NULL;
	g_infAPI.SQLSetParam		= NULL;
	g_infAPI.SQLSetPos			= NULL;
	g_infAPI.SQLSetStmtAttr		= NULL;
	g_infAPI.SQLSetStmtOption	= NULL;
	g_infAPI.SQLSpecialColumns	= NULL;
	g_infAPI.SQLStatistics		= NULL;
	g_infAPI.SQLTablePrivileges	= NULL;
	g_infAPI.SQLTables			= NULL;
	g_infAPI.SQLTransact		= NULL;
}

void AddInfSupport(const SAConnection * /*pCon*/)
{
	SACriticalSectionScope scope(&infLoaderMutex);

	if(!g_nInfDLLRefs)
		LoadAPI();

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
		ResetAPI();
}

