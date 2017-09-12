// db2API.cpp
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <db2API.h>

static long g_nDB2DLLRefs = 0l;
long g_nDB2DLLVersionLoaded = 0l;
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
	g_db2API.SQLAllocConnect	= SQLAllocConnect;
	g_db2API.SQLAllocEnv	= SQLAllocEnv;
	g_db2API.SQLAllocHandle	= SQLAllocHandle;
	g_db2API.SQLAllocStmt	= SQLAllocStmt;
	g_db2API.SQLBindCol	= SQLBindCol;
	g_db2API.SQLBindFileToCol	= SQLBindFileToCol;
	g_db2API.SQLBindFileToParam	= SQLBindFileToParam;
	g_db2API.SQLBindParameter	= SQLBindParameter;
	g_db2API.SQLBrowseConnect	= 
#ifdef SA_UNICODE
		SQLBrowseConnectW;
#else
		SQLBrowseConnect;
#endif
	g_db2API.SQLBulkOperations	= SQLBulkOperations;
	g_db2API.SQLCancel	= SQLCancel;
	g_db2API.SQLCloseCursor	= SQLCloseCursor;
	g_db2API.SQLColAttribute	=
#ifdef SA_UNICODE
		SQLColAttributeW;
#else
		SQLColAttribute;
#endif
	g_db2API.SQLColAttributes	=
#ifdef SA_UNICODE
		SQLColAttributesW;
#else
		SQLColAttributes;
#endif
	g_db2API.SQLColumnPrivileges	=
#ifdef SA_UNICODE
		SQLColumnPrivilegesW;
#else
		SQLColumnPrivileges;
#endif
	g_db2API.SQLColumns	=
#ifdef SA_UNICODE
		SQLColumnsW;
#else
		SQLColumns;
#endif
	g_db2API.SQLConnect	=
#ifdef SA_UNICODE
		SQLConnectW;
#else
		SQLConnect;
#endif
	g_db2API.SQLCopyDesc	= SQLCopyDesc;
	g_db2API.SQLDataSources	=
#ifdef SA_UNICODE
		SQLDataSourcesW;
#else
		SQLDataSources;
#endif
	g_db2API.SQLDescribeCol	=
#ifdef SA_UNICODE
		SQLDescribeColW;
#else
		SQLDescribeCol;
#endif
	g_db2API.SQLDescribeParam	= SQLDescribeParam;
	g_db2API.SQLDisconnect	= SQLDisconnect;
	g_db2API.SQLDriverConnect	=
#ifdef SA_UNICODE
		SQLDriverConnectW;
#else
		SQLDriverConnect;
#endif
	g_db2API.SQLEndTran	= SQLEndTran;
	g_db2API.SQLError	=
#ifdef SA_UNICODE
		SQLErrorW;
#else
		SQLError;
#endif
	g_db2API.SQLExecDirect	=
#ifdef SA_UNICODE
		SQLExecDirectW;
#else
		SQLExecDirect;
#endif
	g_db2API.SQLExecute	= SQLExecute;
	g_db2API.SQLExtendedBind	= SQLExtendedBind;
	g_db2API.SQLExtendedFetch	= SQLExtendedFetch;
	g_db2API.SQLExtendedPrepare	=
#ifdef SA_UNICODE
		SQLExtendedPrepareW;
#else
		SQLExtendedPrepare;
#endif
	g_db2API.SQLFetch	= SQLFetch;
	g_db2API.SQLFetchScroll	= SQLFetchScroll;
	g_db2API.SQLForeignKeys	=
#ifdef SA_UNICODE
		SQLForeignKeysW;
#else
		SQLForeignKeys;
#endif
	g_db2API.SQLFreeConnect	= SQLFreeConnect;
	g_db2API.SQLFreeEnv	= SQLFreeEnv;
	g_db2API.SQLFreeHandle	= SQLFreeHandle;
	g_db2API.SQLFreeStmt	= SQLFreeStmt;
	g_db2API.SQLGetConnectAttr	=
#ifdef SA_UNICODE
		SQLGetConnectAttrW;
#else
		SQLGetConnectAttr;
#endif
	g_db2API.SQLGetConnectOption	=
#ifdef SA_UNICODE
		SQLGetConnectOptionW;
#else
		SQLGetConnectOption;
#endif
	g_db2API.SQLGetCursorName	=
#ifdef SA_UNICODE
		SQLGetCursorNameW;
#else
		SQLGetCursorName;
#endif
	g_db2API.SQLGetData	= SQLGetData;
	g_db2API.SQLGetDescField	=
#ifdef SA_UNICODE
		SQLGetDescFieldW;
#else
		SQLGetDescField;
#endif
	/*
	g_db2API.SQLGetDescRec	=
#ifdef SA_UNICODE
		SQLGetDescRecW;
#else
		SQLGetDescRec;
#endif
		*/
	g_db2API.SQLGetDiagField	=
#ifdef SA_UNICODE
		SQLGetDiagFieldW;
#else
		SQLGetDiagField;
#endif
	g_db2API.SQLGetDiagRec	=
#ifdef SA_UNICODE
		SQLGetDiagRecW;
#else
		SQLGetDiagRec;
#endif
	g_db2API.SQLGetEnvAttr	= SQLGetEnvAttr;
	g_db2API.SQLGetFunctions	= SQLGetFunctions;
	g_db2API.SQLGetInfo	=
#ifdef SA_UNICODE
		SQLGetInfoW;
#else
		SQLGetInfo;
#endif
	g_db2API.SQLGetLength	= SQLGetLength;
	g_db2API.SQLGetPosition	= SQLGetPosition;
	g_db2API.SQLGetSQLCA	= SQLGetSQLCA;
	g_db2API.SQLGetStmtAttr	=
#ifdef SA_UNICODE
		SQLGetStmtAttrW;
#else
		SQLGetStmtAttr;
#endif
	g_db2API.SQLGetStmtOption	= SQLGetStmtOption;
	g_db2API.SQLGetSubString	= SQLGetSubString;
	g_db2API.SQLGetTypeInfo	= SQLGetTypeInfo;
	g_db2API.SQLMoreResults	= SQLMoreResults;
	g_db2API.SQLNativeSql	=
#ifdef SA_UNICODE
		SQLNativeSqlW;
#else
		SQLNativeSql;
#endif
	g_db2API.SQLNumParams	= SQLNumParams;
	g_db2API.SQLNumResultCols	= SQLNumResultCols;
	g_db2API.SQLParamData	= SQLParamData;
	g_db2API.SQLParamOptions	= SQLParamOptions;
	g_db2API.SQLPrepare	=
#ifdef SA_UNICODE
		SQLPrepareW;
#else
		SQLPrepare;
#endif
	g_db2API.SQLPrimaryKeys	=
#ifdef SA_UNICODE
		SQLPrimaryKeysW;
#else
		SQLPrimaryKeys;
#endif
	g_db2API.SQLProcedureColumns	=
#ifdef SA_UNICODE
		SQLProcedureColumnsW;
#else
		SQLProcedureColumns;
#endif
	g_db2API.SQLProcedures	=
#ifdef SA_UNICODE
		SQLProceduresW;
#else
		SQLProcedures;
#endif
	g_db2API.SQLPutData	= SQLPutData;
	g_db2API.SQLRowCount	= SQLRowCount;
	g_db2API.SQLSetColAttributes	= SQLSetColAttributes;
	g_db2API.SQLSetConnectAttr	=
#ifdef SA_UNICODE
		SQLSetConnectAttrW;
#else
		SQLSetConnectAttr;
#endif
	g_db2API.SQLSetConnection	= SQLSetConnection;
	g_db2API.SQLSetConnectOption	=
#ifdef SA_UNICODE
		SQLSetConnectOptionW;
#else
		SQLSetConnectOption;
#endif
	g_db2API.SQLSetCursorName	=
#ifdef SA_UNICODE
		SQLSetCursorNameW;
#else
		SQLSetCursorName;
#endif
	g_db2API.SQLSetDescField	=
#ifdef SA_UNICODE
		SQLSetDescFieldW;
#else
		SQLSetDescField;
#endif
	g_db2API.SQLSetDescRec	= SQLSetDescRec;
	g_db2API.SQLSetEnvAttr	= SQLSetEnvAttr;
	g_db2API.SQLSetParam	= SQLSetParam;
	g_db2API.SQLSetPos	= SQLSetPos;
	g_db2API.SQLSetStmtAttr	=
#ifdef SA_UNICODE
		SQLSetStmtAttrW;
#else
		SQLSetStmtAttr;
#endif
	g_db2API.SQLSetStmtOption	= SQLSetStmtOption;
	g_db2API.SQLSpecialColumns	=
#ifdef SA_UNICODE
		SQLSpecialColumnsW;
#else
		SQLSpecialColumns;
#endif
	g_db2API.SQLStatistics	=
#ifdef SA_UNICODE
		SQLStatisticsW;
#else
		SQLStatistics;
#endif
	g_db2API.SQLTablePrivileges	=
#ifdef SA_UNICODE
		SQLTablePrivilegesW;
#else
		SQLTablePrivileges;
#endif
	g_db2API.SQLTables	=
#ifdef SA_UNICODE
		SQLTablesW;
#else
		SQLTables;
#endif
	g_db2API.SQLTransact	= SQLTransact;
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

void AddDB2Support(const SAConnection * /*pCon*/)
{
	SACriticalSectionScope scope(&db2LoaderMutex);

	if(!g_nDB2DLLRefs)
		LoadAPI();

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
		ResetAPI();
}

