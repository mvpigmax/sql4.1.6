// myAPI.cpp
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <myAPI.h>

static long g_nMySQLDLLRefs = 0l;
static SAMutex myLoaderMutex;

// API definitions
myAPI g_myAPI;

myAPI::myAPI()
{
	mysql_num_rows = NULL;
	mysql_num_fields = NULL;
	mysql_eof = NULL;
	mysql_fetch_field_direct = NULL;
	mysql_fetch_fields = NULL;
	mysql_row_tell = NULL;
	mysql_field_tell = NULL;
	mysql_field_count = NULL;
	mysql_affected_rows = NULL;
	mysql_insert_id = NULL;
	mysql_errno = NULL;
	mysql_error = NULL;
	mysql_info = NULL;
	mysql_thread_id = NULL;
	mysql_character_set_name = NULL;
	mysql_init = NULL;
	mysql_ssl_set = NULL;
	mysql_ssl_cipher = NULL;
	mysql_ssl_clear = NULL;
	mysql_connect = NULL;
	mysql_change_user = NULL;
	mysql_real_connect1 = NULL;
	mysql_real_connect2 = NULL;
	mysql_close = NULL;
	mysql_next_result = NULL;
	mysql_select_db = NULL;
	mysql_query = NULL;
	mysql_send_query = NULL;
	mysql_read_query_result = NULL;
	mysql_real_query = NULL;
	mysql_create_db = NULL;
	mysql_drop_db = NULL;
	mysql_shutdown = NULL;
	mysql_dump_debug_info = NULL;
	mysql_refresh = NULL;
	mysql_kill = NULL;
	mysql_ping = NULL;
	mysql_stat = NULL;
	mysql_get_server_info = NULL;
	mysql_get_client_info = NULL;
	mysql_get_host_info = NULL;
	mysql_get_proto_info = NULL;
	mysql_list_dbs = NULL;
	mysql_list_tables = NULL;
	mysql_list_fields = NULL;
	mysql_list_processes = NULL;
	mysql_store_result = NULL;
	mysql_use_result = NULL;
	mysql_options = NULL;
	mysql_free_result = NULL;
	mysql_data_seek = NULL;
	mysql_row_seek = NULL;
	mysql_field_seek = NULL;
	mysql_fetch_row = NULL;
	mysql_fetch_lengths = NULL;
	mysql_fetch_field = NULL;
	mysql_escape_string = NULL;
	mysql_real_escape_string = NULL;
	mysql_debug = NULL;
	mysql_odbc_escape_string = NULL;
	myodbc_remove_escape = NULL;
	mysql_thread_init = NULL;
	mysql_thread_end = NULL;
	mysql_thread_safe = NULL;
	mysql_server_init = NULL;
	mysql_server_end = NULL;
	mysql_set_character_set = NULL;
	// MySQL statement API functions
	mysql_stmt_init = NULL;
	mysql_stmt_prepare = NULL;
	mysql_stmt_execute = NULL;
	mysql_stmt_fetch = NULL;
	mysql_stmt_fetch_column = NULL;
	mysql_stmt_store_result = NULL;
	mysql_stmt_param_count = NULL;
	mysql_stmt_attr_set = NULL;
	mysql_stmt_attr_get = NULL;
	mysql_stmt_bind_param = NULL;
	mysql_stmt_bind_result = NULL;
	mysql_stmt_close = NULL;
	mysql_stmt_reset = NULL;
	mysql_stmt_free_result = NULL;
	mysql_stmt_send_long_data = NULL;
	mysql_stmt_result_metadata = NULL;
	mysql_stmt_param_metadata = NULL;
	mysql_stmt_errno = NULL;
	mysql_stmt_error = NULL;
	mysql_stmt_sqlstate = NULL;
	mysql_stmt_row_seek = NULL;
	mysql_stmt_row_tell = NULL;
	mysql_stmt_data_seek = NULL;
	mysql_stmt_num_rows = NULL;
	mysql_stmt_affected_rows = NULL;
	mysql_stmt_insert_id = NULL;
	mysql_stmt_field_count = NULL;

	mysql_get_character_set_info = NULL;
}

myConnectionHandles::myConnectionHandles()
{
	mysql = NULL;
}

myCommandHandles::myCommandHandles()
{
	result = NULL;
	stmt = NULL;
}

static void LoadAPI()
{
	g_myAPI.mysql_num_rows = mysql_num_rows;
	g_myAPI.mysql_num_fields = mysql_num_fields;
	g_myAPI.mysql_eof = mysql_eof;
	g_myAPI.mysql_fetch_field_direct = mysql_fetch_field_direct;
	g_myAPI.mysql_fetch_fields = mysql_fetch_fields;
	g_myAPI.mysql_row_tell = mysql_row_tell;
	g_myAPI.mysql_field_tell = mysql_field_tell;
	g_myAPI.mysql_field_count = mysql_field_count;
	g_myAPI.mysql_affected_rows = mysql_affected_rows;
	g_myAPI.mysql_insert_id = mysql_insert_id;
	g_myAPI.mysql_errno = mysql_errno;
	g_myAPI.mysql_error = mysql_error;
	g_myAPI.mysql_info = mysql_info;
	g_myAPI.mysql_thread_id = mysql_thread_id;
	g_myAPI.mysql_character_set_name = mysql_character_set_name;
	g_myAPI.mysql_init = mysql_init;
	/*
	g_myAPI.mysql_ssl_set = mysql_ssl_set;
	g_myAPI.mysql_ssl_cipher = mysql_ssl_cipher;
	g_myAPI.mysql_ssl_clear = mysql_ssl_clear;
	g_myAPI.mysql_connect = mysql_connect;
	g_myAPI.mysql_change_user = mysql_change_user;
	*/

	g_myAPI.mysql_get_server_info = mysql_get_server_info;
	g_myAPI.mysql_get_client_info = mysql_get_client_info;
	g_myAPI.mysql_get_host_info = mysql_get_host_info;
	g_myAPI.mysql_get_proto_info = mysql_get_proto_info;

	/*
	long version = SAExtractVersionFromString(g_myAPI.mysql_get_client_info());

	if( version < 0x00030016 ) // < 3.22
	{
	g_myAPI.mysql_real_connect1 = (mysql_real_connect1_t)::GetProcAddress(g_hMySQLDLL, "mysql_real_connect"); assert(g_myAPI.mysql_real_connect1 != NULL);
	g_myAPI.mysql_real_connect2 = NULL;
	}
	else
	{
	g_myAPI.mysql_real_connect1 = NULL;
	g_myAPI.mysql_real_connect2 = (mysql_real_connect2_t)::GetProcAddress(g_hMySQLDLL, "mysql_real_connect"); assert(g_myAPI.mysql_real_connect2 != NULL);
	}
	*/
	g_myAPI.mysql_real_connect1 = NULL;
	g_myAPI.mysql_real_connect2 = mysql_real_connect;

	g_myAPI.mysql_close = mysql_close;
	g_myAPI.mysql_next_result = mysql_next_result;
	g_myAPI.mysql_select_db = mysql_select_db;
	g_myAPI.mysql_query = mysql_query;
	g_myAPI.mysql_send_query = mysql_send_query;
	g_myAPI.mysql_read_query_result = mysql_read_query_result;
	g_myAPI.mysql_real_query = mysql_real_query;
	g_myAPI.mysql_create_db = NULL; // mysql_create_db; // deprecated
	g_myAPI.mysql_drop_db = NULL; // mysql_drop_db
	g_myAPI.mysql_shutdown = mysql_shutdown;
	g_myAPI.mysql_dump_debug_info = mysql_dump_debug_info;
	g_myAPI.mysql_refresh = mysql_refresh;
	g_myAPI.mysql_kill = mysql_kill;
	g_myAPI.mysql_ping = mysql_ping;
	g_myAPI.mysql_stat = mysql_stat;

	g_myAPI.mysql_list_dbs = mysql_list_dbs;
	g_myAPI.mysql_list_tables = mysql_list_tables;
	g_myAPI.mysql_list_fields = mysql_list_fields;
	g_myAPI.mysql_list_processes = mysql_list_processes;
	g_myAPI.mysql_store_result = mysql_store_result;
	g_myAPI.mysql_use_result = mysql_use_result;
	g_myAPI.mysql_options = mysql_options;
	g_myAPI.mysql_free_result = mysql_free_result;
	g_myAPI.mysql_data_seek = mysql_data_seek;
	g_myAPI.mysql_row_seek = mysql_row_seek;
	g_myAPI.mysql_field_seek = mysql_field_seek;
	g_myAPI.mysql_fetch_row = mysql_fetch_row;
	g_myAPI.mysql_fetch_lengths = mysql_fetch_lengths;
	g_myAPI.mysql_fetch_field = mysql_fetch_field;
	g_myAPI.mysql_escape_string = mysql_escape_string;
	g_myAPI.mysql_real_escape_string = mysql_real_escape_string; // ???
	g_myAPI.mysql_debug = mysql_debug;
	//g_myAPI.mysql_odbc_escape_string = mysql_odbc_escape_string;
	g_myAPI.myodbc_remove_escape = myodbc_remove_escape;

	g_myAPI.mysql_thread_init = mysql_thread_init;
	g_myAPI.mysql_thread_end = mysql_thread_end;
	g_myAPI.mysql_thread_safe = mysql_thread_safe;

	g_myAPI.mysql_server_init = mysql_server_init;
	g_myAPI.mysql_server_end = mysql_server_end;

	g_myAPI.mysql_set_character_set = mysql_set_character_set;

	// MySQL statement API functions
	g_myAPI.mysql_stmt_init = mysql_stmt_init;
	g_myAPI.mysql_stmt_prepare = mysql_stmt_prepare;
	g_myAPI.mysql_stmt_execute = mysql_stmt_execute;
	g_myAPI.mysql_stmt_fetch = mysql_stmt_fetch;
	g_myAPI.mysql_stmt_fetch_column = mysql_stmt_fetch_column;
	g_myAPI.mysql_stmt_store_result = mysql_stmt_store_result;
	g_myAPI.mysql_stmt_param_count = mysql_stmt_param_count;
	g_myAPI.mysql_stmt_attr_set = mysql_stmt_attr_set;
	g_myAPI.mysql_stmt_attr_get = mysql_stmt_attr_get;
	g_myAPI.mysql_stmt_bind_param = mysql_stmt_bind_param;
	g_myAPI.mysql_stmt_bind_result = mysql_stmt_bind_result;
	g_myAPI.mysql_stmt_close = mysql_stmt_close;
	g_myAPI.mysql_stmt_reset = mysql_stmt_reset;
	g_myAPI.mysql_stmt_free_result = mysql_stmt_free_result;
	g_myAPI.mysql_stmt_send_long_data = mysql_stmt_send_long_data;
	g_myAPI.mysql_stmt_result_metadata = mysql_stmt_result_metadata;
	g_myAPI.mysql_stmt_param_metadata = mysql_stmt_param_metadata;
	g_myAPI.mysql_stmt_errno = mysql_stmt_errno;
	g_myAPI.mysql_stmt_error = mysql_stmt_error;
	g_myAPI.mysql_stmt_sqlstate = mysql_stmt_sqlstate;
	g_myAPI.mysql_stmt_row_seek = mysql_stmt_row_seek;
	g_myAPI.mysql_stmt_row_tell = mysql_stmt_row_tell;
	g_myAPI.mysql_stmt_data_seek = mysql_stmt_data_seek;
	g_myAPI.mysql_stmt_num_rows = mysql_stmt_num_rows;
	g_myAPI.mysql_stmt_affected_rows = mysql_stmt_affected_rows;
	g_myAPI.mysql_stmt_insert_id = mysql_stmt_insert_id;
	g_myAPI.mysql_stmt_field_count = mysql_stmt_field_count;

	g_myAPI.mysql_get_character_set_info = mysql_get_character_set_info;
}

static void ResetAPI()
{
	g_myAPI.mysql_num_rows = NULL;
	g_myAPI.mysql_num_fields = NULL;
	g_myAPI.mysql_eof = NULL;
	g_myAPI.mysql_fetch_field_direct = NULL;
	g_myAPI.mysql_fetch_fields = NULL;
	g_myAPI.mysql_row_tell = NULL;
	g_myAPI.mysql_field_tell = NULL;
	g_myAPI.mysql_field_count = NULL;
	g_myAPI.mysql_affected_rows = NULL;
	g_myAPI.mysql_insert_id = NULL;
	g_myAPI.mysql_errno = NULL;
	g_myAPI.mysql_error = NULL;
	g_myAPI.mysql_info = NULL;
	g_myAPI.mysql_thread_id = NULL;
	g_myAPI.mysql_character_set_name = NULL;
	g_myAPI.mysql_init = NULL;
	g_myAPI.mysql_ssl_set = NULL;
	g_myAPI.mysql_ssl_cipher = NULL;
	g_myAPI.mysql_ssl_clear = NULL;
	g_myAPI.mysql_connect = NULL;
	g_myAPI.mysql_change_user = NULL;
	g_myAPI.mysql_real_connect1 = NULL;
	g_myAPI.mysql_real_connect2 = NULL;
	g_myAPI.mysql_close = NULL;
	g_myAPI.mysql_next_result = NULL;
	g_myAPI.mysql_select_db = NULL;
	g_myAPI.mysql_query = NULL;
	g_myAPI.mysql_send_query = NULL;
	g_myAPI.mysql_read_query_result = NULL;
	g_myAPI.mysql_real_query = NULL;
	g_myAPI.mysql_create_db = NULL;
	g_myAPI.mysql_drop_db = NULL;
	g_myAPI.mysql_shutdown = NULL;
	g_myAPI.mysql_dump_debug_info = NULL;
	g_myAPI.mysql_refresh = NULL;
	g_myAPI.mysql_kill = NULL;
	g_myAPI.mysql_ping = NULL;
	g_myAPI.mysql_stat = NULL;
	g_myAPI.mysql_get_server_info = NULL;
	g_myAPI.mysql_get_client_info = NULL;
	g_myAPI.mysql_get_host_info = NULL;
	g_myAPI.mysql_get_proto_info = NULL;
	g_myAPI.mysql_list_dbs = NULL;
	g_myAPI.mysql_list_tables = NULL;
	g_myAPI.mysql_list_fields = NULL;
	g_myAPI.mysql_list_processes = NULL;
	g_myAPI.mysql_store_result = NULL;
	g_myAPI.mysql_use_result = NULL;
	g_myAPI.mysql_options = NULL;
	g_myAPI.mysql_free_result = NULL;
	g_myAPI.mysql_data_seek = NULL;
	g_myAPI.mysql_row_seek = NULL;
	g_myAPI.mysql_field_seek = NULL;
	g_myAPI.mysql_fetch_row = NULL;
	g_myAPI.mysql_fetch_lengths = NULL;
	g_myAPI.mysql_fetch_field = NULL;
	g_myAPI.mysql_escape_string = NULL;
	g_myAPI.mysql_real_escape_string = NULL;
	g_myAPI.mysql_debug = NULL;
	g_myAPI.mysql_odbc_escape_string = NULL;
	g_myAPI.myodbc_remove_escape = NULL;
	g_myAPI.mysql_thread_init = NULL;
	g_myAPI.mysql_thread_end = NULL;
	g_myAPI.mysql_thread_safe = NULL;
	g_myAPI.mysql_server_init = NULL;
	g_myAPI.mysql_server_end = NULL;
	g_myAPI.mysql_set_character_set = NULL;

	g_myAPI.mysql_get_character_set_info = NULL;
}

void AddMySQLSupport(const SAConnection * pCon)
{
	SACriticalSectionScope scope(&myLoaderMutex);

	if(!g_nMySQLDLLRefs)
	{
		LoadAPI();

        SAString sValue = pCon->Option(_TSA("SkipServerInit"));
        if (NULL != g_myAPI.mysql_server_init && 0 != sValue.CompareNoCase(_TSA("TRUE"))
            && 0 != sValue.CompareNoCase(_TSA("1")))
            g_myAPI.mysql_server_init(0, NULL, NULL);
	}

	if( SAGlobals::UnloadAPI() )
		g_nMySQLDLLRefs++;
	else
		g_nMySQLDLLRefs = 1;
}

void ReleaseMySQLSupport()
{
	SACriticalSectionScope scope(&myLoaderMutex);

	assert(g_nMySQLDLLRefs > 0);
	g_nMySQLDLLRefs--;
	if(!g_nMySQLDLLRefs)
	{
		if( NULL != g_myAPI.mysql_server_end )
			g_myAPI.mysql_server_end();

		ResetAPI();
	}
}

