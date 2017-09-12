// asaAPI.cpp
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <asaAPI.h>

static const SAChar *g_sASADLLNames = _TSA("libdbcapi.so");
static void* g_hASADLL = NULL;
static long g_nASADLLRefs = 0l;
static SAMutex asaLoaderMutex;

// API definitions
asaAPI g_asaAPI;

asaAPI::asaAPI()
{
	sqlany_init = NULL;
	sqlany_fini = NULL;
	sqlany_new_connection = NULL;
	sqlany_free_connection = NULL;
	sqlany_make_connection = NULL;
	sqlany_connect = NULL;
	sqlany_disconnect = NULL;
	sqlany_execute_immediate = NULL;
	sqlany_prepare = NULL;
	sqlany_free_stmt = NULL;
	sqlany_num_params = NULL;
	sqlany_describe_bind_param = NULL;
	sqlany_bind_param = NULL;
	sqlany_send_param_data = NULL;
	sqlany_reset = NULL;
	sqlany_get_bind_param_info = NULL;
	sqlany_execute = NULL;
	sqlany_execute_direct = NULL;
	sqlany_fetch_absolute = NULL;
	sqlany_fetch_next = NULL;
	sqlany_get_next_result = NULL;
	sqlany_affected_rows = NULL;
	sqlany_num_cols = NULL;
	sqlany_num_rows = NULL;
	sqlany_get_column = NULL;
	sqlany_get_data = NULL;
	sqlany_get_data_info = NULL;
	sqlany_get_column_info = NULL;
	sqlany_commit = NULL;
	sqlany_rollback = NULL;
	sqlany_client_version = NULL;
	sqlany_error = NULL;
	sqlany_sqlstate = NULL;
	sqlany_clear_error = NULL;
	// newer API
	sqlany_init_ex = NULL;
	sqlany_fini_ex = NULL;
	sqlany_new_connection_ex = NULL;
	sqlany_make_connection_ex = NULL;
	sqlany_client_version_ex = NULL;
	sqlany_cancel = NULL;
}

asaConnectionHandles::asaConnectionHandles()
{
	pDb = NULL;
}

asaCommandHandles::asaCommandHandles()
{
	pStmt = NULL;
}

static void LoadAPI()
{
	g_asaAPI.sqlany_init = (sqlany_init_t)::dlsym(g_hASADLL, "sqlany_init"); assert(NULL != g_asaAPI.sqlany_init);
	g_asaAPI.sqlany_fini = (sqlany_fini_t)::dlsym(g_hASADLL, "sqlany_fini"); assert(NULL != g_asaAPI.sqlany_fini);
	g_asaAPI.sqlany_new_connection = (sqlany_new_connection_t)::dlsym(g_hASADLL, "sqlany_new_connection"); assert(NULL != g_asaAPI.sqlany_new_connection);
	g_asaAPI.sqlany_free_connection = (sqlany_free_connection_t)::dlsym(g_hASADLL, "sqlany_free_connection"); assert(NULL != g_asaAPI.sqlany_free_connection);
	g_asaAPI.sqlany_make_connection = (sqlany_make_connection_t)::dlsym(g_hASADLL, "sqlany_make_connection"); assert(NULL != g_asaAPI.sqlany_make_connection);
	g_asaAPI.sqlany_connect = (sqlany_connect_t)::dlsym(g_hASADLL, "sqlany_connect"); assert(NULL != g_asaAPI.sqlany_connect);
	g_asaAPI.sqlany_disconnect = (sqlany_disconnect_t)::dlsym(g_hASADLL, "sqlany_disconnect"); assert(NULL != g_asaAPI.sqlany_disconnect);
	g_asaAPI.sqlany_execute_immediate = (sqlany_execute_immediate_t)::dlsym(g_hASADLL, "sqlany_execute_immediate"); assert(NULL != g_asaAPI.sqlany_execute_immediate);
	g_asaAPI.sqlany_prepare = (sqlany_prepare_t)::dlsym(g_hASADLL, "sqlany_prepare"); assert(NULL != g_asaAPI.sqlany_prepare);
	g_asaAPI.sqlany_free_stmt = (sqlany_free_stmt_t)::dlsym(g_hASADLL, "sqlany_free_stmt"); assert(NULL != g_asaAPI.sqlany_free_stmt);
	g_asaAPI.sqlany_num_params = (sqlany_num_params_t)::dlsym(g_hASADLL, "sqlany_num_params"); assert(NULL != g_asaAPI.sqlany_num_params);
	g_asaAPI.sqlany_describe_bind_param = (sqlany_describe_bind_param_t)::dlsym(g_hASADLL, "sqlany_describe_bind_param"); assert(NULL != g_asaAPI.sqlany_describe_bind_param);
	g_asaAPI.sqlany_bind_param = (sqlany_bind_param_t)::dlsym(g_hASADLL, "sqlany_bind_param"); assert(NULL != g_asaAPI.sqlany_bind_param);
	g_asaAPI.sqlany_send_param_data = (sqlany_send_param_data_t)::dlsym(g_hASADLL, "sqlany_send_param_data"); assert(NULL != g_asaAPI.sqlany_send_param_data);
	g_asaAPI.sqlany_reset = (sqlany_reset_t)::dlsym(g_hASADLL, "sqlany_reset"); assert(NULL != g_asaAPI.sqlany_reset);
	g_asaAPI.sqlany_get_bind_param_info = (sqlany_get_bind_param_info_t)::dlsym(g_hASADLL, "sqlany_get_bind_param_info"); assert(NULL != g_asaAPI.sqlany_get_bind_param_info);
	g_asaAPI.sqlany_execute = (sqlany_execute_t)::dlsym(g_hASADLL, "sqlany_execute"); assert(NULL != g_asaAPI.sqlany_execute);
	g_asaAPI.sqlany_execute_direct = (sqlany_execute_direct_t)::dlsym(g_hASADLL, "sqlany_execute_direct"); assert(NULL != g_asaAPI.sqlany_execute_direct);
	g_asaAPI.sqlany_fetch_absolute = (sqlany_fetch_absolute_t)::dlsym(g_hASADLL, "sqlany_fetch_absolute"); assert(NULL != g_asaAPI.sqlany_fetch_absolute);
	g_asaAPI.sqlany_fetch_next = (sqlany_fetch_next_t)::dlsym(g_hASADLL, "sqlany_fetch_next"); assert(NULL != g_asaAPI.sqlany_fetch_next);
	g_asaAPI.sqlany_get_next_result = (sqlany_get_next_result_t)::dlsym(g_hASADLL, "sqlany_get_next_result"); assert(NULL != g_asaAPI.sqlany_get_next_result);
	g_asaAPI.sqlany_affected_rows = (sqlany_affected_rows_t)::dlsym(g_hASADLL, "sqlany_affected_rows"); assert(NULL != g_asaAPI.sqlany_affected_rows);
	g_asaAPI.sqlany_num_cols = (sqlany_num_cols_t)::dlsym(g_hASADLL, "sqlany_num_cols"); assert(NULL != g_asaAPI.sqlany_num_cols);
	g_asaAPI.sqlany_num_rows = (sqlany_num_rows_t)::dlsym(g_hASADLL, "sqlany_num_rows"); assert(NULL != g_asaAPI.sqlany_num_rows);
	g_asaAPI.sqlany_get_column = (sqlany_get_column_t)::dlsym(g_hASADLL, "sqlany_get_column"); assert(NULL != g_asaAPI.sqlany_get_column);
	g_asaAPI.sqlany_get_data = (sqlany_get_data_t)::dlsym(g_hASADLL, "sqlany_get_data"); assert(NULL != g_asaAPI.sqlany_get_data);
	g_asaAPI.sqlany_get_data_info = (sqlany_get_data_info_t)::dlsym(g_hASADLL, "sqlany_get_data_info"); assert(NULL != g_asaAPI.sqlany_get_data_info);
	g_asaAPI.sqlany_get_column_info = (sqlany_get_column_info_t)::dlsym(g_hASADLL, "sqlany_get_column_info"); assert(NULL != g_asaAPI.sqlany_get_column_info);
	g_asaAPI.sqlany_commit = (sqlany_commit_t)::dlsym(g_hASADLL, "sqlany_commit"); assert(NULL != g_asaAPI.sqlany_commit);
	g_asaAPI.sqlany_rollback = (sqlany_rollback_t)::dlsym(g_hASADLL, "sqlany_rollback"); assert(NULL != g_asaAPI.sqlany_rollback);
	g_asaAPI.sqlany_client_version = (sqlany_client_version_t)::dlsym(g_hASADLL, "sqlany_client_version"); assert(NULL != g_asaAPI.sqlany_client_version);
	g_asaAPI.sqlany_error = (sqlany_error_t)::dlsym(g_hASADLL, "sqlany_error"); assert(NULL != g_asaAPI.sqlany_error);
	g_asaAPI.sqlany_sqlstate = (sqlany_sqlstate_t)::dlsym(g_hASADLL, "sqlany_sqlstate"); assert(NULL != g_asaAPI.sqlany_sqlstate);
	g_asaAPI.sqlany_clear_error = (sqlany_clear_error_t)::dlsym(g_hASADLL, "sqlany_clear_error"); assert(NULL != g_asaAPI.sqlany_clear_error);
	// newer API
	g_asaAPI.sqlany_init_ex = (sqlany_init_ex_t)::dlsym(g_hASADLL, "sqlany_init_ex");
	g_asaAPI.sqlany_fini_ex = (sqlany_fini_ex_t)::dlsym(g_hASADLL, "sqlany_fini_ex");
	g_asaAPI.sqlany_new_connection_ex = (sqlany_new_connection_ex_t)::dlsym(g_hASADLL, "sqlany_new_connection_ex");
	g_asaAPI.sqlany_make_connection_ex = (sqlany_make_connection_ex_t)::dlsym(g_hASADLL, "sqlany_make_connection_ex");
	g_asaAPI.sqlany_client_version_ex = (sqlany_client_version_ex_t)::dlsym(g_hASADLL, "sqlany_client_version_ex");
	g_asaAPI.sqlany_cancel = (sqlany_cancel_t)::dlsym(g_hASADLL, "sqlany_cancel");
}

static void ResetAPI()
{
	g_asaAPI.sqlany_init = NULL;
	g_asaAPI.sqlany_fini = NULL;
	g_asaAPI.sqlany_new_connection = NULL;
	g_asaAPI.sqlany_free_connection = NULL;
	g_asaAPI.sqlany_make_connection = NULL;
	g_asaAPI.sqlany_connect = NULL;
	g_asaAPI.sqlany_disconnect = NULL;
	g_asaAPI.sqlany_execute_immediate = NULL;
	g_asaAPI.sqlany_prepare = NULL;
	g_asaAPI.sqlany_free_stmt = NULL;
	g_asaAPI.sqlany_num_params = NULL;
	g_asaAPI.sqlany_describe_bind_param = NULL;
	g_asaAPI.sqlany_bind_param = NULL;
	g_asaAPI.sqlany_send_param_data = NULL;
	g_asaAPI.sqlany_reset = NULL;
	g_asaAPI.sqlany_get_bind_param_info = NULL;
	g_asaAPI.sqlany_execute = NULL;
	g_asaAPI.sqlany_execute_direct = NULL;
	g_asaAPI.sqlany_fetch_absolute = NULL;
	g_asaAPI.sqlany_fetch_next = NULL;
	g_asaAPI.sqlany_get_next_result = NULL;
	g_asaAPI.sqlany_affected_rows = NULL;
	g_asaAPI.sqlany_num_cols = NULL;
	g_asaAPI.sqlany_num_rows = NULL;
	g_asaAPI.sqlany_get_column = NULL;
	g_asaAPI.sqlany_get_data = NULL;
	g_asaAPI.sqlany_get_data_info = NULL;
	g_asaAPI.sqlany_get_column_info = NULL;
	g_asaAPI.sqlany_commit = NULL;
	g_asaAPI.sqlany_rollback = NULL;
	g_asaAPI.sqlany_client_version = NULL;
	g_asaAPI.sqlany_error = NULL;
	g_asaAPI.sqlany_sqlstate = NULL;
	g_asaAPI.sqlany_clear_error = NULL;
	// newer API
	g_asaAPI.sqlany_init_ex = NULL;
	g_asaAPI.sqlany_fini_ex = NULL;
	g_asaAPI.sqlany_new_connection_ex = NULL;
	g_asaAPI.sqlany_make_connection_ex = NULL;
	g_asaAPI.sqlany_client_version_ex = NULL;
	g_asaAPI.sqlany_cancel = NULL;
}

void AddSQLAnywhereSupport(const SAConnection * pCon)
{
	SACriticalSectionScope scope(&asaLoaderMutex);

	if( ! g_nASADLLRefs )
	{
		SAString sErrorMessage, sLibName, sLibsList = pCon->Option(_TSA("SQLANY.LIBS"));
		if( sLibsList.IsEmpty() )
			sLibsList = g_sASADLLNames;

		g_hASADLL = SALoadLibraryFromList(sLibsList, sErrorMessage, sLibName, RTLD_LAZY|RTLD_GLOBAL);
		if( ! g_hASADLL )
			throw SAException(SA_Library_Error, SA_Library_Error_LoadLibraryFails, -1, IDS_LOAD_LIBRARY_FAILS, (const SAChar*)sErrorMessage);

		LoadAPI();
	}

	if( SAGlobals::UnloadAPI() )
		g_nASADLLRefs++;
	else
		g_nASADLLRefs = 1;
}

void ReleaseSQLAnywhereSupport()
{
	SACriticalSectionScope scope(&asaLoaderMutex);

	assert(g_nASADLLRefs > 0);
	g_nASADLLRefs--;
	if( ! g_nASADLLRefs )
	{
		ResetAPI();

		::dlclose(g_hASADLL);
		g_hASADLL = NULL;
	}
}

