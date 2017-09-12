// slAPI.cpp
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <sl3API.h>

static const char *g_sSQLiteDLLNames = "libsqlite3" SHARED_OBJ_EXT;
static void *g_hSQLiteDLL = NULL;
static long g_nSQLiteDLLRefs = 0l;
static SAMutex sl3LoaderMutex;

// API definitions
sl3API g_sl3API;

sl3API::sl3API()
{
	sqlite3_open = NULL;
	sqlite3_libversion = NULL;
	sqlite3_libversion_number = NULL;
	sqlite3_errcode = NULL;
	sqlite3_errmsg = NULL;
	sqlite3_close = NULL;
	sqlite3_exec = NULL;
	sqlite3_prepare = NULL;
	sqlite3_bind_parameter_index = NULL;
	sqlite3_column_count = NULL;
	sqlite3_column_name = NULL;
	sqlite3_column_type = NULL;
	sqlite3_column_bytes = NULL;
	sqlite3_step = NULL;
	sqlite3_db_handle = NULL;
	sqlite3_reset = NULL;
	sqlite3_clear_bindings = NULL;
	sqlite3_finalize = NULL;
	sqlite3_interrupt = NULL;
	sqlite3_changes = NULL;
	sqlite3_column_int64 = NULL;
	sqlite3_column_double = NULL;
	sqlite3_column_blob = NULL;
	sqlite3_column_text = NULL;
	sqlite3_bind_blob = NULL;
	sqlite3_bind_double = NULL;
	sqlite3_bind_int = NULL;
	sqlite3_bind_int64 = NULL;
	sqlite3_bind_null = NULL;
	sqlite3_bind_text = NULL;

	sqlite3_busy_handler = NULL;
	sqlite3_busy_timeout = NULL;
	sqlite3_threadsafe = NULL;
	sqlite3_last_insert_rowid = NULL;

	sqlite3_column_decltype = NULL;

	sqlite3_open_v2 = NULL;

	sqlite3_backup_init = NULL;
	sqlite3_backup_step = NULL;
	sqlite3_backup_finish = NULL;
	sqlite3_backup_remaining = NULL;
	sqlite3_backup_pagecount = NULL;

	sqlite3_table_column_metadata = NULL;

	sqlite3_column_value = NULL;
	sqlite3_value_type = NULL;

	sqlite3_update_hook = NULL;

    sqlite3_enable_load_extension = NULL;

    sqlite3_key = NULL;
    sqlite3_rekey = NULL;
}

sl3ConnectionHandles::sl3ConnectionHandles()
{
	pDb = NULL;
}

sl3CommandHandles::sl3CommandHandles()
{
	pStmt = NULL;
}

static void LoadAPI()
{
	g_sl3API.sqlite3_open = (sqlite3_open_t)::dlsym(g_hSQLiteDLL,
#ifdef SA_UNICODE
		"sqlite3_open16");
#else
		"sqlite3_open");
#endif
	assert(NULL != g_sl3API.sqlite3_open);
	g_sl3API.sqlite3_open_v2 = (sqlite3_open_v2_t)::dlsym(g_hSQLiteDLL, "sqlite3_open_v2");
	g_sl3API.sqlite3_libversion = (sqlite3_libversion_t)::dlsym(g_hSQLiteDLL, "sqlite3_libversion"); assert(NULL != g_sl3API.sqlite3_libversion);
	g_sl3API.sqlite3_libversion_number = (sqlite3_libversion_number_t)::dlsym(g_hSQLiteDLL, "sqlite3_libversion_number"); assert(NULL != g_sl3API.sqlite3_libversion_number);
	g_sl3API.sqlite3_errcode = (sqlite3_errcode_t)::dlsym(g_hSQLiteDLL, "sqlite3_errcode"); assert(NULL != g_sl3API.sqlite3_errcode);
	g_sl3API.sqlite3_errmsg = (sqlite3_errmsg_t)::dlsym(g_hSQLiteDLL,
#ifdef SA_UNICODE
		"sqlite3_errmsg16");
#else
		"sqlite3_errmsg");
#endif
	assert(NULL != g_sl3API.sqlite3_errmsg);
	g_sl3API.sqlite3_close = (sqlite3_close_t)::dlsym(g_hSQLiteDLL, "sqlite3_close"); assert(NULL != g_sl3API.sqlite3_close);
	g_sl3API.sqlite3_exec = (sqlite3_exec_t)::dlsym(g_hSQLiteDLL, "sqlite3_exec"); assert(NULL != g_sl3API.sqlite3_exec);
	g_sl3API.sqlite3_prepare = (sqlite3_prepare_t)::dlsym(g_hSQLiteDLL,
#ifdef SA_UNICODE
		"sqlite3_prepare16_v2");
#else
		"sqlite3_prepare_v2");
#endif
	if( NULL == g_sl3API.sqlite3_prepare )
		g_sl3API.sqlite3_prepare = (sqlite3_prepare_t)::dlsym(g_hSQLiteDLL,
#ifdef SA_UNICODE
			"sqlite3_prepare16");
#else
			"sqlite3_prepare");
#endif
	assert(NULL != g_sl3API.sqlite3_prepare);
	g_sl3API.sqlite3_bind_parameter_index = (sqlite3_bind_parameter_index_t)::dlsym(g_hSQLiteDLL, "sqlite3_bind_parameter_index"); assert(NULL != g_sl3API.sqlite3_bind_parameter_index);
	g_sl3API.sqlite3_column_count = (sqlite3_column_count_t)::dlsym(g_hSQLiteDLL, "sqlite3_column_count"); assert(NULL != g_sl3API.sqlite3_column_count);
	g_sl3API.sqlite3_column_name = (sqlite3_column_name_t)::dlsym(g_hSQLiteDLL,
#ifdef SA_UNICODE
		"sqlite3_column_name16");
#else
		"sqlite3_column_name");
#endif
	assert(NULL != g_sl3API.sqlite3_column_name);
	g_sl3API.sqlite3_column_type = (sqlite3_column_type_t)::dlsym(g_hSQLiteDLL, "sqlite3_column_type"); assert(NULL != g_sl3API.sqlite3_column_type);
	g_sl3API.sqlite3_column_bytes = (sqlite3_column_bytes_t)::dlsym(g_hSQLiteDLL,
#ifdef SA_UNICODE
		"sqlite3_column_bytes16");
#else
		"sqlite3_column_bytes");
#endif
	assert(NULL != g_sl3API.sqlite3_column_bytes);
	g_sl3API.sqlite3_step = (sqlite3_step_t)::dlsym(g_hSQLiteDLL, "sqlite3_step"); assert(NULL != g_sl3API.sqlite3_step);
	g_sl3API.sqlite3_db_handle = (sqlite3_db_handle_t)::dlsym(g_hSQLiteDLL, "sqlite3_db_handle"); assert(NULL != g_sl3API.sqlite3_db_handle);
	g_sl3API.sqlite3_reset = (sqlite3_reset_t)::dlsym(g_hSQLiteDLL, "sqlite3_reset"); assert(NULL != g_sl3API.sqlite3_reset);
	g_sl3API.sqlite3_clear_bindings = (sqlite3_clear_bindings_t)::dlsym(g_hSQLiteDLL, "sqlite3_clear_bindings"); assert(NULL != g_sl3API.sqlite3_clear_bindings);
	g_sl3API.sqlite3_finalize = (sqlite3_finalize_t)::dlsym(g_hSQLiteDLL, "sqlite3_finalize"); assert(NULL != g_sl3API.sqlite3_finalize);
	g_sl3API.sqlite3_interrupt = (sqlite3_interrupt_t)::dlsym(g_hSQLiteDLL, "sqlite3_interrupt"); assert(NULL != g_sl3API.sqlite3_interrupt);
	g_sl3API.sqlite3_changes = (sqlite3_changes_t)::dlsym(g_hSQLiteDLL, "sqlite3_changes"); assert(NULL != g_sl3API.sqlite3_changes);
	g_sl3API.sqlite3_column_int64 = (sqlite3_column_int64_t)::dlsym(g_hSQLiteDLL, "sqlite3_column_int64"); assert(NULL != g_sl3API.sqlite3_column_int64);
	g_sl3API.sqlite3_column_double = (sqlite3_column_double_t)::dlsym(g_hSQLiteDLL, "sqlite3_column_double"); assert(NULL != g_sl3API.sqlite3_column_double);
	g_sl3API.sqlite3_column_blob = (sqlite3_column_blob_t)::dlsym(g_hSQLiteDLL, "sqlite3_column_blob"); assert(NULL != g_sl3API.sqlite3_column_blob);
	g_sl3API.sqlite3_column_text = (sqlite3_column_text_t)::dlsym(g_hSQLiteDLL,
#ifdef SA_UNICODE
		"sqlite3_column_text16");
#else
		"sqlite3_column_text");
#endif
	assert(NULL != g_sl3API.sqlite3_column_text);
	g_sl3API.sqlite3_bind_blob = (sqlite3_bind_blob_t)::dlsym(g_hSQLiteDLL, "sqlite3_bind_blob"); assert(NULL != g_sl3API.sqlite3_bind_blob);
	g_sl3API.sqlite3_bind_double = (sqlite3_bind_double_t)::dlsym(g_hSQLiteDLL, "sqlite3_bind_double"); assert(NULL != g_sl3API.sqlite3_bind_double);
	g_sl3API.sqlite3_bind_int = (sqlite3_bind_int_t)::dlsym(g_hSQLiteDLL, "sqlite3_bind_int"); assert(NULL != g_sl3API.sqlite3_bind_int);
	g_sl3API.sqlite3_bind_int64 = (sqlite3_bind_int64_t)::dlsym(g_hSQLiteDLL, "sqlite3_bind_int64"); assert(NULL != g_sl3API.sqlite3_bind_int64);
	g_sl3API.sqlite3_bind_null = (sqlite3_bind_null_t)::dlsym(g_hSQLiteDLL, "sqlite3_bind_null"); assert(NULL != g_sl3API.sqlite3_bind_null);
	g_sl3API.sqlite3_bind_text = (sqlite3_bind_text_t)::dlsym(g_hSQLiteDLL,
#ifdef SA_UNICODE
		"sqlite3_bind_text16");
#else
		"sqlite3_bind_text");
#endif
	assert(NULL != g_sl3API.sqlite3_bind_text);

	g_sl3API.sqlite3_busy_handler = (sqlite3_busy_handler_t)::dlsym(g_hSQLiteDLL, "sqlite3_busy_handler"); assert(NULL != g_sl3API.sqlite3_busy_handler);
	g_sl3API.sqlite3_busy_timeout = (sqlite3_busy_timeout_t)::dlsym(g_hSQLiteDLL, "sqlite3_busy_timeout"); assert(NULL != g_sl3API.sqlite3_busy_timeout);
	g_sl3API.sqlite3_threadsafe = (sqlite3_threadsafe_t)::dlsym(g_hSQLiteDLL, "sqlite3_threadsafe"); assert(NULL != g_sl3API.sqlite3_threadsafe);
	g_sl3API.sqlite3_last_insert_rowid = (sqlite3_last_insert_rowid_t)::dlsym(g_hSQLiteDLL, "sqlite3_last_insert_rowid"); assert(NULL != g_sl3API.sqlite3_last_insert_rowid);

	g_sl3API.sqlite3_column_decltype = (sqlite3_column_decltype_t)::dlsym(g_hSQLiteDLL,
#ifdef SA_UNICODE
		"sqlite3_column_decltype16");
#else
		"sqlite3_column_decltype");
#endif
	assert(NULL != g_sl3API.sqlite3_column_decltype);

	g_sl3API.sqlite3_backup_init = (sqlite3_backup_init_t)::dlsym(g_hSQLiteDLL, "sqlite3_backup_init");
	g_sl3API.sqlite3_backup_step = (sqlite3_backup_step_t)::dlsym(g_hSQLiteDLL, "sqlite3_backup_step");
	g_sl3API.sqlite3_backup_finish = (sqlite3_backup_finish_t)::dlsym(g_hSQLiteDLL, "sqlite3_backup_finish");
	g_sl3API.sqlite3_backup_remaining = (sqlite3_backup_remaining_t)::dlsym(g_hSQLiteDLL, "sqlite3_backup_remaining");
	g_sl3API.sqlite3_backup_pagecount = (sqlite3_backup_pagecount_t)::dlsym(g_hSQLiteDLL, "sqlite3_backup_pagecount");

	g_sl3API.sqlite3_table_column_metadata = (sqlite3_table_column_metadata_t)::dlsym(g_hSQLiteDLL, "sqlite3_table_column_metadata");

	g_sl3API.sqlite3_column_value = (sqlite3_column_value_t)::dlsym(g_hSQLiteDLL, "sqlite3_column_value");
	g_sl3API.sqlite3_value_type = (sqlite3_value_type_t)::dlsym(g_hSQLiteDLL, "sqlite3_value_type");

	g_sl3API.sqlite3_update_hook = (sqlite3_update_hook_t)::dlsym(g_hSQLiteDLL, "sqlite3_update_hook");

    g_sl3API.sqlite3_enable_load_extension = (sqlite3_enable_load_extension_t)::dlsym(g_hSQLiteDLL, "sqlite3_enable_load_extension");

    g_sl3API.sqlite3_key = (sqlite3_key_t)::dlsym(g_hSQLiteDLL, "sqlite3_key");
    g_sl3API.sqlite3_rekey = (sqlite3_rekey_t)::dlsym(g_hSQLiteDLL, "sqlite3_rekey");
}

static void ResetAPI()
{
	g_sl3API.sqlite3_open = NULL;
	g_sl3API.sqlite3_libversion = NULL;
	g_sl3API.sqlite3_libversion_number = NULL;
	g_sl3API.sqlite3_errcode = NULL;
	g_sl3API.sqlite3_errmsg = NULL;
	g_sl3API.sqlite3_close = NULL;
	g_sl3API.sqlite3_exec = NULL;
	g_sl3API.sqlite3_prepare = NULL;
	g_sl3API.sqlite3_bind_parameter_index = NULL;
	g_sl3API.sqlite3_column_count = NULL;
	g_sl3API.sqlite3_column_name = NULL;
	g_sl3API.sqlite3_column_type = NULL;
	g_sl3API.sqlite3_column_bytes = NULL;
	g_sl3API.sqlite3_step = NULL;
	g_sl3API.sqlite3_db_handle = NULL;
	g_sl3API.sqlite3_reset = NULL;
	g_sl3API.sqlite3_clear_bindings = NULL;
	g_sl3API.sqlite3_finalize = NULL;
	g_sl3API.sqlite3_interrupt = NULL;
	g_sl3API.sqlite3_changes = NULL;
	g_sl3API.sqlite3_column_int64 = NULL;
	g_sl3API.sqlite3_column_double = NULL;
	g_sl3API.sqlite3_column_blob = NULL;
	g_sl3API.sqlite3_column_text = NULL;
	g_sl3API.sqlite3_bind_blob = NULL;
	g_sl3API.sqlite3_bind_double = NULL;
	g_sl3API.sqlite3_bind_int = NULL;
	g_sl3API.sqlite3_bind_int64 = NULL;
	g_sl3API.sqlite3_bind_null = NULL;
	g_sl3API.sqlite3_bind_text = NULL;

	g_sl3API.sqlite3_busy_handler = NULL;
	g_sl3API.sqlite3_busy_timeout = NULL;
	g_sl3API.sqlite3_threadsafe = NULL;
	g_sl3API.sqlite3_last_insert_rowid = NULL;

	g_sl3API.sqlite3_column_decltype = NULL;

	g_sl3API.sqlite3_open_v2 = NULL;

	g_sl3API.sqlite3_backup_init = NULL;
	g_sl3API.sqlite3_backup_step = NULL;
	g_sl3API.sqlite3_backup_finish = NULL;
	g_sl3API.sqlite3_backup_remaining = NULL;
	g_sl3API.sqlite3_backup_pagecount = NULL;

	g_sl3API.sqlite3_table_column_metadata = NULL;

	g_sl3API.sqlite3_column_value = NULL;
	g_sl3API.sqlite3_value_type = NULL;

	g_sl3API.sqlite3_update_hook = NULL;

    g_sl3API.sqlite3_enable_load_extension = NULL;

    g_sl3API.sqlite3_key = NULL;
    g_sl3API.sqlite3_rekey = NULL;
}

void AddSQLite3Support(const SAConnection * pCon)
{
	SACriticalSectionScope scope(&sl3LoaderMutex);

	if(!g_nSQLiteDLLRefs)
	{
		SAString sErrorMessage, sLibName, sLibsList = pCon->Option(_TSA("SQLITE.LIBS"));
		if( sLibsList.IsEmpty() )
			sLibsList = g_sSQLiteDLLNames;

		g_hSQLiteDLL = SALoadLibraryFromList(sLibsList, sErrorMessage, sLibName, RTLD_LAZY);
		if( ! g_hSQLiteDLL )
			throw SAException(SA_Library_Error, SA_Library_Error_LoadLibraryFails, -1, IDS_LOAD_LIBRARY_FAILS, (const SAChar*)sErrorMessage);

		LoadAPI();
	}

	if( SAGlobals::UnloadAPI() )
		g_nSQLiteDLLRefs++;
	else
		g_nSQLiteDLLRefs = 1;
}

void ReleaseSQLite3Support()
{
	SACriticalSectionScope scope(&sl3LoaderMutex);

	assert(g_nSQLiteDLLRefs > 0);
	g_nSQLiteDLLRefs--;
	if( ! g_nSQLiteDLLRefs )
	{
		ResetAPI();

		::dlclose(g_hSQLiteDLL);
		g_hSQLiteDLL = NULL;
	}
}

