// slAPI.cpp
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <sl3API.h>

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
	g_sl3API.sqlite3_open = (sqlite3_open_t)
#ifdef SA_UNICODE
		sqlite3_open16;
#else
		sqlite3_open;
#endif
	g_sl3API.sqlite3_open_v2 = (sqlite3_open_v2_t)sqlite3_open_v2;
	g_sl3API.sqlite3_libversion = sqlite3_libversion;
	g_sl3API.sqlite3_libversion_number = sqlite3_libversion_number;
	g_sl3API.sqlite3_errcode = sqlite3_errcode;
	g_sl3API.sqlite3_errmsg = (sqlite3_errmsg_t)
#ifdef SA_UNICODE
		sqlite3_errmsg16;
#else
		sqlite3_errmsg;
#endif
	g_sl3API.sqlite3_close = sqlite3_close;
	g_sl3API.sqlite3_exec = sqlite3_exec;
	g_sl3API.sqlite3_prepare = (sqlite3_prepare_t)
#ifdef SA_UNICODE
		sqlite3_prepare16_v2;
#else
		sqlite3_prepare_v2;
#endif
	g_sl3API.sqlite3_bind_parameter_index = sqlite3_bind_parameter_index;
	g_sl3API.sqlite3_column_count = sqlite3_column_count;
	g_sl3API.sqlite3_column_name = (sqlite3_column_name_t)
#ifdef SA_UNICODE
		sqlite3_column_name16;
#else
		sqlite3_column_name;
#endif
	g_sl3API.sqlite3_column_type = sqlite3_column_type;
	g_sl3API.sqlite3_column_bytes =
#ifdef SA_UNICODE
		sqlite3_column_bytes16;
#else
		sqlite3_column_bytes;
#endif
	g_sl3API.sqlite3_step = sqlite3_step;
	g_sl3API.sqlite3_db_handle = sqlite3_db_handle;
	g_sl3API.sqlite3_reset = sqlite3_reset;
	g_sl3API.sqlite3_clear_bindings = sqlite3_clear_bindings;
	g_sl3API.sqlite3_finalize = sqlite3_finalize;
	g_sl3API.sqlite3_interrupt = sqlite3_interrupt;
	g_sl3API.sqlite3_changes = sqlite3_changes;
	g_sl3API.sqlite3_column_int64 = sqlite3_column_int64;
	g_sl3API.sqlite3_column_double = sqlite3_column_double;
	g_sl3API.sqlite3_column_blob = sqlite3_column_blob;
	g_sl3API.sqlite3_column_text = (sqlite3_column_text_t)
#ifdef SA_UNICODE
		sqlite3_column_text16;
#else
		sqlite3_column_text;
#endif
	g_sl3API.sqlite3_bind_blob = sqlite3_bind_blob;
	g_sl3API.sqlite3_bind_double = sqlite3_bind_double;
	g_sl3API.sqlite3_bind_int = sqlite3_bind_int;
	g_sl3API.sqlite3_bind_int64 = sqlite3_bind_int64;
	g_sl3API.sqlite3_bind_null = sqlite3_bind_null;
	g_sl3API.sqlite3_bind_text = (sqlite3_bind_text_t)
#ifdef SA_UNICODE
		sqlite3_bind_text16;
#else
		sqlite3_bind_text;
#endif

	g_sl3API.sqlite3_busy_handler = sqlite3_busy_handler;
	g_sl3API.sqlite3_busy_timeout = sqlite3_busy_timeout;
	g_sl3API.sqlite3_threadsafe = sqlite3_threadsafe;
	g_sl3API.sqlite3_last_insert_rowid = sqlite3_last_insert_rowid;

	g_sl3API.sqlite3_column_decltype = (sqlite3_column_decltype_t)
#ifdef SA_UNICODE
		sqlite3_column_decltype16;
#else
		sqlite3_column_decltype;
#endif

	g_sl3API.sqlite3_open_v2 = sqlite3_open_v2;

	g_sl3API.sqlite3_backup_init = sqlite3_backup_init;
	g_sl3API.sqlite3_backup_step = sqlite3_backup_step;
	g_sl3API.sqlite3_backup_finish = sqlite3_backup_finish;
	g_sl3API.sqlite3_backup_remaining = sqlite3_backup_remaining;
	g_sl3API.sqlite3_backup_pagecount = sqlite3_backup_pagecount;

#ifdef SQLITE_ENABLE_COLUMN_METADATA
	g_sl3API.sqlite3_table_column_metadata = sqlite3_table_column_metadata;
#endif

	g_sl3API.sqlite3_column_value = sqlite3_column_value;
	g_sl3API.sqlite3_value_type = sqlite3_value_type;

	g_sl3API.sqlite3_update_hook = sqlite3_update_hook;

    g_sl3API.sqlite3_enable_load_extension = sqlite3_enable_load_extension;

    g_sl3API.sqlite3_key = sqlite3_key;
    g_sl3API.sqlite3_rekey = sqlite3_rekey;
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

void AddSQLite3Support(const SAConnection * /*pCon*/)
{
	SACriticalSectionScope scope(&sl3LoaderMutex);

	if( ! g_nSQLiteDLLRefs )
		LoadAPI();

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
		ResetAPI();
}

