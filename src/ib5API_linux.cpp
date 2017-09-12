// ibAPI_linux.cpp
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <ibAPI.h>

static const char *g_sIB_DLLNames = "libgds" SHARED_OBJ_EXT ":libfbclient" SHARED_OBJ_EXT;
static void* g_hIB_DLL = NULL;
long g_nIB_DLLVersionLoaded = 0l;
static long g_nIB_DLLRefs = 0l;

static SAMutex ibLoaderMutex;

// API definitions
ibAPI g_ibAPI;

ibAPI::ibAPI()
{
	isc_attach_database	= NULL;
	isc_array_gen_sdl	= NULL;
	isc_array_get_slice	= NULL;
	isc_array_lookup_bounds	= NULL;
	isc_array_lookup_desc	= NULL;
	isc_array_set_desc	= NULL;
	isc_array_put_slice	= NULL;
	isc_blob_default_desc	= NULL;
	isc_blob_gen_bpb	= NULL;
	isc_blob_info	= NULL;
	isc_blob_lookup_desc	= NULL;
	isc_blob_set_desc	= NULL;
	isc_cancel_blob	= NULL;
	isc_cancel_events	= NULL;
	isc_close_blob	= NULL;
	isc_commit_retaining	= NULL;
	isc_commit_transaction	= NULL;
	isc_create_blob	= NULL;
	isc_create_blob2	= NULL;
	isc_create_database	= NULL;
	isc_database_info	= NULL;
	isc_decode_date	= NULL;
	isc_detach_database	= NULL;
	isc_drop_database	= NULL;
	isc_dsql_allocate_statement	= NULL;
	isc_dsql_alloc_statement2	= NULL;
	isc_dsql_describe	= NULL;
	isc_dsql_describe_bind	= NULL;
	isc_dsql_exec_immed2	= NULL;
	isc_dsql_execute	= NULL;
	isc_dsql_execute2	= NULL;
	isc_dsql_execute_immediate	= NULL;
	isc_dsql_fetch	= NULL;
	isc_dsql_finish	= NULL;
	isc_dsql_free_statement	= NULL;
	isc_dsql_insert	= NULL;
	isc_dsql_prepare	= NULL;
	isc_dsql_set_cursor_name	= NULL;
	isc_dsql_sql_info	= NULL;
	isc_encode_date	= NULL;
	isc_event_block	= NULL;
	isc_event_counts	= NULL;
	isc_expand_dpb	= NULL;
	isc_modify_dpb	= NULL;
	isc_free	= NULL;
	isc_get_segment	= NULL;
	isc_get_slice	= NULL;
	isc_interprete	= NULL;
	isc_open_blob	= NULL;
	isc_open_blob2	= NULL;
	isc_prepare_transaction2	= NULL;
	isc_print_sqlerror	= NULL;
	isc_print_status	= NULL;
	isc_put_segment	= NULL;
	isc_put_slice	= NULL;
	isc_que_events	= NULL;
	isc_rollback_transaction	= NULL;
	isc_start_multiple	= NULL;
	isc_start_transaction	= NULL;
	isc_sqlcode	= NULL;
	isc_sql_interprete	= NULL;
	isc_transaction_info	= NULL;
	isc_transact_request	= NULL;
	isc_vax_integer	= NULL;
	isc_add_user	= NULL;
	isc_delete_user	= NULL;
	isc_modify_user	= NULL;
	isc_compile_request	= NULL;
	isc_compile_request2	= NULL;
	isc_ddl	= NULL;
	isc_prepare_transaction	= NULL;
	isc_receive	= NULL;
	isc_reconnect_transaction	= NULL;
	isc_release_request	= NULL;
	isc_request_info	= NULL;
	isc_seek_blob	= NULL;
	isc_send	= NULL;
	isc_start_and_send	= NULL;
	isc_start_request	= NULL;
	isc_unwind_request	= NULL;
	isc_wait_for_event	= NULL;
	isc_close	= NULL;
	isc_declare	= NULL;
	isc_execute_immediate	= NULL;
	isc_dsql_execute_m	= NULL;
	isc_dsql_execute2_m	= NULL;
	isc_dsql_execute_immediate_m	= NULL;
	isc_dsql_exec_immed3_m	= NULL;
	isc_dsql_fetch_m	= NULL;
	isc_dsql_insert_m	= NULL;
	isc_dsql_prepare_m	= NULL;
	isc_dsql_release	= NULL;
	isc_embed_dsql_close	= NULL;
	isc_embed_dsql_declare	= NULL;
	isc_embed_dsql_describe	= NULL;
	isc_embed_dsql_describe_bind	= NULL;
	isc_embed_dsql_execute	= NULL;
	isc_embed_dsql_execute2	= NULL;
	isc_embed_dsql_execute_immed	= NULL;
	isc_embed_dsql_fetch	= NULL;
	isc_embed_dsql_open	= NULL;
	isc_embed_dsql_open2	= NULL;
	isc_embed_dsql_insert	= NULL;
	isc_embed_dsql_prepare	= NULL;
	isc_embed_dsql_release	= NULL;
	isc_ftof	= NULL;
	isc_print_blr	= NULL;
	isc_set_debug	= NULL;
	isc_qtoq	= NULL;
	isc_vtof	= NULL;
	isc_vtov	= NULL;
	isc_version	= NULL;
	fb_interpret = NULL;
}

ibConnectionHandles::ibConnectionHandles()
{
	m_db_handle		= ISC_NULL_HANDLE;
	m_tr_handle		= ISC_NULL_HANDLE;
}

ibCommandHandles::ibCommandHandles()
{
	m_stmt_handle	= ISC_NULL_HANDLE;
}

static void LoadAPI()
{
	g_ibAPI.isc_attach_database	= (isc_attach_database_t)dlsym(g_hIB_DLL, "isc_attach_database");	assert(g_ibAPI.isc_attach_database != NULL);
	g_ibAPI.isc_array_gen_sdl	= (isc_array_gen_sdl_t)dlsym(g_hIB_DLL, "isc_array_gen_sdl");	assert(g_ibAPI.isc_array_gen_sdl != NULL);
	g_ibAPI.isc_array_get_slice	= (isc_array_get_slice_t)dlsym(g_hIB_DLL, "isc_array_get_slice");	assert(g_ibAPI.isc_array_get_slice != NULL);
	g_ibAPI.isc_array_lookup_bounds	= (isc_array_lookup_bounds_t)dlsym(g_hIB_DLL, "isc_array_lookup_bounds");	assert(g_ibAPI.isc_array_lookup_bounds != NULL);
	g_ibAPI.isc_array_lookup_desc	= (isc_array_lookup_desc_t)dlsym(g_hIB_DLL, "isc_array_lookup_desc");	assert(g_ibAPI.isc_array_lookup_desc != NULL);
	g_ibAPI.isc_array_set_desc	= (isc_array_set_desc_t)dlsym(g_hIB_DLL, "isc_array_set_desc");	assert(g_ibAPI.isc_array_set_desc != NULL);
	g_ibAPI.isc_array_put_slice	= (isc_array_put_slice_t)dlsym(g_hIB_DLL, "isc_array_put_slice");	assert(g_ibAPI.isc_array_put_slice != NULL);
	g_ibAPI.isc_blob_default_desc	= (isc_blob_default_desc_t)dlsym(g_hIB_DLL, "isc_blob_default_desc");	assert(g_ibAPI.isc_blob_default_desc != NULL);
	g_ibAPI.isc_blob_gen_bpb	= (isc_blob_gen_bpb_t)dlsym(g_hIB_DLL, "isc_blob_gen_bpb");	assert(g_ibAPI.isc_blob_gen_bpb != NULL);
	g_ibAPI.isc_blob_info	= (isc_blob_info_t)dlsym(g_hIB_DLL, "isc_blob_info");	assert(g_ibAPI.isc_blob_info != NULL);
	g_ibAPI.isc_blob_lookup_desc	= (isc_blob_lookup_desc_t)dlsym(g_hIB_DLL, "isc_blob_lookup_desc");	assert(g_ibAPI.isc_blob_lookup_desc != NULL);
	g_ibAPI.isc_blob_set_desc	= (isc_blob_set_desc_t)dlsym(g_hIB_DLL, "isc_blob_set_desc");	assert(g_ibAPI.isc_blob_set_desc != NULL);
	g_ibAPI.isc_cancel_blob	= (isc_cancel_blob_t)dlsym(g_hIB_DLL, "isc_cancel_blob");	assert(g_ibAPI.isc_cancel_blob != NULL);
	g_ibAPI.isc_cancel_events	= (isc_cancel_events_t)dlsym(g_hIB_DLL, "isc_cancel_events");	assert(g_ibAPI.isc_cancel_events != NULL);
	g_ibAPI.isc_close_blob	= (isc_close_blob_t)dlsym(g_hIB_DLL, "isc_close_blob");	assert(g_ibAPI.isc_close_blob != NULL);
	g_ibAPI.isc_commit_retaining	= (isc_commit_retaining_t)dlsym(g_hIB_DLL, "isc_commit_retaining");	assert(g_ibAPI.isc_commit_retaining != NULL);
	g_ibAPI.isc_commit_transaction	= (isc_commit_transaction_t)dlsym(g_hIB_DLL, "isc_commit_transaction");	assert(g_ibAPI.isc_commit_transaction != NULL);
	g_ibAPI.isc_create_blob	= (isc_create_blob_t)dlsym(g_hIB_DLL, "isc_create_blob");	assert(g_ibAPI.isc_create_blob != NULL);
	g_ibAPI.isc_create_blob2	= (isc_create_blob2_t)dlsym(g_hIB_DLL, "isc_create_blob2");	assert(g_ibAPI.isc_create_blob2 != NULL);
	g_ibAPI.isc_create_database	= (isc_create_database_t)dlsym(g_hIB_DLL, "isc_create_database");	assert(g_ibAPI.isc_create_database != NULL);
	g_ibAPI.isc_database_info	= (isc_database_info_t)dlsym(g_hIB_DLL, "isc_database_info");	assert(g_ibAPI.isc_database_info != NULL);
	g_ibAPI.isc_decode_date	= (isc_decode_date_t)dlsym(g_hIB_DLL, "isc_decode_date");	assert(g_ibAPI.isc_decode_date != NULL);
	g_ibAPI.isc_detach_database	= (isc_detach_database_t)dlsym(g_hIB_DLL, "isc_detach_database");	assert(g_ibAPI.isc_detach_database != NULL);
	g_ibAPI.isc_drop_database	= (isc_drop_database_t)dlsym(g_hIB_DLL, "isc_drop_database");	assert(g_ibAPI.isc_drop_database != NULL);
	g_ibAPI.isc_dsql_allocate_statement	= (isc_dsql_allocate_statement_t)dlsym(g_hIB_DLL, "isc_dsql_allocate_statement");	assert(g_ibAPI.isc_dsql_allocate_statement != NULL);
	g_ibAPI.isc_dsql_alloc_statement2	= (isc_dsql_alloc_statement2_t)dlsym(g_hIB_DLL, "isc_dsql_alloc_statement2");	assert(g_ibAPI.isc_dsql_alloc_statement2 != NULL);
	g_ibAPI.isc_dsql_describe	= (isc_dsql_describe_t)dlsym(g_hIB_DLL, "isc_dsql_describe");	assert(g_ibAPI.isc_dsql_describe != NULL);
	g_ibAPI.isc_dsql_describe_bind	= (isc_dsql_describe_bind_t)dlsym(g_hIB_DLL, "isc_dsql_describe_bind");	assert(g_ibAPI.isc_dsql_describe_bind != NULL);
	g_ibAPI.isc_dsql_exec_immed2	= (isc_dsql_exec_immed2_t)dlsym(g_hIB_DLL, "isc_dsql_exec_immed2");	assert(g_ibAPI.isc_dsql_exec_immed2 != NULL);
	g_ibAPI.isc_dsql_execute	= (isc_dsql_execute_t)dlsym(g_hIB_DLL, "isc_dsql_execute");	assert(g_ibAPI.isc_dsql_execute != NULL);
	g_ibAPI.isc_dsql_execute2	= (isc_dsql_execute2_t)dlsym(g_hIB_DLL, "isc_dsql_execute2");	assert(g_ibAPI.isc_dsql_execute2 != NULL);
	g_ibAPI.isc_dsql_execute_immediate	= (isc_dsql_execute_immediate_t)dlsym(g_hIB_DLL, "isc_dsql_execute_immediate");	assert(g_ibAPI.isc_dsql_execute_immediate != NULL);
	g_ibAPI.isc_dsql_fetch	= (isc_dsql_fetch_t)dlsym(g_hIB_DLL, "isc_dsql_fetch");	assert(g_ibAPI.isc_dsql_fetch != NULL);
	g_ibAPI.isc_dsql_finish	= (isc_dsql_finish_t)dlsym(g_hIB_DLL, "isc_dsql_finish");	assert(g_ibAPI.isc_dsql_finish != NULL);
	g_ibAPI.isc_dsql_free_statement	= (isc_dsql_free_statement_t)dlsym(g_hIB_DLL, "isc_dsql_free_statement");	assert(g_ibAPI.isc_dsql_free_statement != NULL);
	g_ibAPI.isc_dsql_insert	= (isc_dsql_insert_t)dlsym(g_hIB_DLL, "isc_dsql_insert");	assert(g_ibAPI.isc_dsql_insert != NULL);
	g_ibAPI.isc_dsql_prepare	= (isc_dsql_prepare_t)dlsym(g_hIB_DLL, "isc_dsql_prepare");	assert(g_ibAPI.isc_dsql_prepare != NULL);
	g_ibAPI.isc_dsql_set_cursor_name	= (isc_dsql_set_cursor_name_t)dlsym(g_hIB_DLL, "isc_dsql_set_cursor_name");	assert(g_ibAPI.isc_dsql_set_cursor_name != NULL);
	g_ibAPI.isc_dsql_sql_info	= (isc_dsql_sql_info_t)dlsym(g_hIB_DLL, "isc_dsql_sql_info");	assert(g_ibAPI.isc_dsql_sql_info != NULL);
	g_ibAPI.isc_encode_date	= (isc_encode_date_t)dlsym(g_hIB_DLL, "isc_encode_date");	assert(g_ibAPI.isc_encode_date != NULL);
	g_ibAPI.isc_event_block	= (isc_event_block_t)dlsym(g_hIB_DLL, "isc_event_block");	assert(g_ibAPI.isc_event_block != NULL);
	g_ibAPI.isc_event_counts	= (isc_event_counts_t)dlsym(g_hIB_DLL, "isc_event_counts");	assert(g_ibAPI.isc_event_counts != NULL);
	g_ibAPI.isc_expand_dpb	= (isc_expand_dpb_t)dlsym(g_hIB_DLL, "isc_expand_dpb");	assert(g_ibAPI.isc_expand_dpb != NULL);
	g_ibAPI.isc_modify_dpb	= (isc_modify_dpb_t)dlsym(g_hIB_DLL, "isc_modify_dpb");	assert(g_ibAPI.isc_modify_dpb != NULL);
	g_ibAPI.isc_free	= (isc_free_t)dlsym(g_hIB_DLL, "isc_free");	assert(g_ibAPI.isc_free != NULL);
	g_ibAPI.isc_get_segment	= (isc_get_segment_t)dlsym(g_hIB_DLL, "isc_get_segment");	assert(g_ibAPI.isc_get_segment != NULL);
	g_ibAPI.isc_get_slice	= (isc_get_slice_t)dlsym(g_hIB_DLL, "isc_get_slice");	assert(g_ibAPI.isc_get_slice != NULL);
	g_ibAPI.isc_interprete	= (isc_interprete_t)dlsym(g_hIB_DLL, "isc_interprete");	assert(g_ibAPI.isc_interprete != NULL);
	g_ibAPI.isc_open_blob	= (isc_open_blob_t)dlsym(g_hIB_DLL, "isc_open_blob");	assert(g_ibAPI.isc_open_blob != NULL);
	g_ibAPI.isc_open_blob2	= (isc_open_blob2_t)dlsym(g_hIB_DLL, "isc_open_blob2");	assert(g_ibAPI.isc_open_blob2 != NULL);

	g_ibAPI.isc_prepare_transaction2	= (isc_prepare_transaction2_t)dlsym(g_hIB_DLL, "isc_prepare_transaction2");	assert(g_ibAPI.isc_prepare_transaction2 != NULL);
	g_ibAPI.isc_print_sqlerror	= (isc_print_sqlerror_t)dlsym(g_hIB_DLL, "isc_print_sqlerror");	assert(g_ibAPI.isc_print_sqlerror != NULL);
	g_ibAPI.isc_print_status	= (isc_print_status_t)dlsym(g_hIB_DLL, "isc_print_status");	assert(g_ibAPI.isc_print_status != NULL);
	g_ibAPI.isc_put_segment	= (isc_put_segment_t)dlsym(g_hIB_DLL, "isc_put_segment");	assert(g_ibAPI.isc_put_segment != NULL);
	g_ibAPI.isc_put_slice	= (isc_put_slice_t)dlsym(g_hIB_DLL, "isc_put_slice");	assert(g_ibAPI.isc_put_slice != NULL);
	g_ibAPI.isc_que_events	= (isc_que_events_t)dlsym(g_hIB_DLL, "isc_que_events");	assert(g_ibAPI.isc_que_events != NULL);
	g_ibAPI.isc_rollback_transaction	= (isc_rollback_transaction_t)dlsym(g_hIB_DLL, "isc_rollback_transaction");	assert(g_ibAPI.isc_rollback_transaction != NULL);
	g_ibAPI.isc_start_multiple	= (isc_start_multiple_t)dlsym(g_hIB_DLL, "isc_start_multiple");	assert(g_ibAPI.isc_start_multiple != NULL);
	g_ibAPI.isc_start_transaction	= (isc_start_transaction_t)dlsym(g_hIB_DLL, "isc_start_transaction");	assert(g_ibAPI.isc_start_transaction != NULL);
	g_ibAPI.isc_sqlcode	= (isc_sqlcode_t)dlsym(g_hIB_DLL, "isc_sqlcode");	assert(g_ibAPI.isc_sqlcode != NULL);
	g_ibAPI.isc_sql_interprete	= (isc_sql_interprete_t)dlsym(g_hIB_DLL, "isc_sql_interprete");	assert(g_ibAPI.isc_sql_interprete != NULL);
	g_ibAPI.isc_transaction_info	= (isc_transaction_info_t)dlsym(g_hIB_DLL, "isc_transaction_info");	assert(g_ibAPI.isc_transaction_info != NULL);
	g_ibAPI.isc_transact_request	= (isc_transact_request_t)dlsym(g_hIB_DLL, "isc_transact_request");	assert(g_ibAPI.isc_transact_request != NULL);
	g_ibAPI.isc_vax_integer	= (isc_vax_integer_t)dlsym(g_hIB_DLL, "isc_vax_integer");	assert(g_ibAPI.isc_vax_integer != NULL);
	g_ibAPI.isc_add_user	= (isc_add_user_t)dlsym(g_hIB_DLL, "isc_add_user");	assert(g_ibAPI.isc_add_user != NULL);
	g_ibAPI.isc_delete_user	= (isc_delete_user_t)dlsym(g_hIB_DLL, "isc_delete_user");	assert(g_ibAPI.isc_delete_user != NULL);
	g_ibAPI.isc_modify_user	= (isc_modify_user_t)dlsym(g_hIB_DLL, "isc_modify_user");	assert(g_ibAPI.isc_modify_user != NULL);
	g_ibAPI.isc_compile_request	= (isc_compile_request_t)dlsym(g_hIB_DLL, "isc_compile_request");	assert(g_ibAPI.isc_compile_request != NULL);
	g_ibAPI.isc_compile_request2	= (isc_compile_request2_t)dlsym(g_hIB_DLL, "isc_compile_request2");	assert(g_ibAPI.isc_compile_request2 != NULL);
	g_ibAPI.isc_ddl	= (isc_ddl_t)dlsym(g_hIB_DLL, "isc_ddl");	assert(g_ibAPI.isc_ddl != NULL);
	g_ibAPI.isc_prepare_transaction	= (isc_prepare_transaction_t)dlsym(g_hIB_DLL, "isc_prepare_transaction");	assert(g_ibAPI.isc_prepare_transaction != NULL);
	g_ibAPI.isc_receive	= (isc_receive_t)dlsym(g_hIB_DLL, "isc_receive");	assert(g_ibAPI.isc_receive != NULL);
	g_ibAPI.isc_reconnect_transaction	= (isc_reconnect_transaction_t)dlsym(g_hIB_DLL, "isc_reconnect_transaction");	assert(g_ibAPI.isc_reconnect_transaction != NULL);
	g_ibAPI.isc_release_request	= (isc_release_request_t)dlsym(g_hIB_DLL, "isc_release_request");	assert(g_ibAPI.isc_release_request != NULL);
	g_ibAPI.isc_request_info	= (isc_request_info_t)dlsym(g_hIB_DLL, "isc_request_info");	assert(g_ibAPI.isc_request_info != NULL);
	g_ibAPI.isc_seek_blob	= (isc_seek_blob_t)dlsym(g_hIB_DLL, "isc_seek_blob");	assert(g_ibAPI.isc_seek_blob != NULL);
	g_ibAPI.isc_send	= (isc_send_t)dlsym(g_hIB_DLL, "isc_send");	assert(g_ibAPI.isc_send != NULL);
	g_ibAPI.isc_start_and_send	= (isc_start_and_send_t)dlsym(g_hIB_DLL, "isc_start_and_send");	assert(g_ibAPI.isc_start_and_send != NULL);
	g_ibAPI.isc_start_request	= (isc_start_request_t)dlsym(g_hIB_DLL, "isc_start_request");	assert(g_ibAPI.isc_start_request != NULL);
	g_ibAPI.isc_unwind_request	= (isc_unwind_request_t)dlsym(g_hIB_DLL, "isc_unwind_request");	assert(g_ibAPI.isc_unwind_request != NULL);
	g_ibAPI.isc_wait_for_event	= (isc_wait_for_event_t)dlsym(g_hIB_DLL, "isc_wait_for_event");	assert(g_ibAPI.isc_wait_for_event != NULL);
	g_ibAPI.isc_close	= (isc_close_t)dlsym(g_hIB_DLL, "isc_close");	assert(g_ibAPI.isc_close != NULL);
	g_ibAPI.isc_declare	= (isc_declare_t)dlsym(g_hIB_DLL, "isc_declare");	assert(g_ibAPI.isc_declare != NULL);
	g_ibAPI.isc_execute_immediate	= (isc_execute_immediate_t)dlsym(g_hIB_DLL, "isc_execute_immediate");	assert(g_ibAPI.isc_execute_immediate != NULL);

	g_ibAPI.isc_dsql_execute_m	= (isc_dsql_execute_m_t)dlsym(g_hIB_DLL, "isc_dsql_execute_m");	assert(g_ibAPI.isc_dsql_execute_m != NULL);
	g_ibAPI.isc_dsql_execute2_m	= (isc_dsql_execute2_m_t)dlsym(g_hIB_DLL, "isc_dsql_execute2_m");	assert(g_ibAPI.isc_dsql_execute2_m != NULL);
	g_ibAPI.isc_dsql_execute_immediate_m	= (isc_dsql_execute_immediate_m_t)dlsym(g_hIB_DLL, "isc_dsql_execute_immediate_m");	assert(g_ibAPI.isc_dsql_execute_immediate_m != NULL);
	g_ibAPI.isc_dsql_exec_immed3_m	= (isc_dsql_exec_immed3_m_t)dlsym(g_hIB_DLL, "isc_dsql_exec_immed3_m");	assert(g_ibAPI.isc_dsql_exec_immed3_m != NULL);
	g_ibAPI.isc_dsql_fetch_m	= (isc_dsql_fetch_m_t)dlsym(g_hIB_DLL, "isc_dsql_fetch_m");	assert(g_ibAPI.isc_dsql_fetch_m != NULL);
	g_ibAPI.isc_dsql_insert_m	= (isc_dsql_insert_m_t)dlsym(g_hIB_DLL, "isc_dsql_insert_m");	assert(g_ibAPI.isc_dsql_insert_m != NULL);
	g_ibAPI.isc_dsql_prepare_m	= (isc_dsql_prepare_m_t)dlsym(g_hIB_DLL, "isc_dsql_prepare_m");	assert(g_ibAPI.isc_dsql_prepare_m != NULL);
	g_ibAPI.isc_dsql_release	= (isc_dsql_release_t)dlsym(g_hIB_DLL, "isc_dsql_release");	assert(g_ibAPI.isc_dsql_release != NULL);
	g_ibAPI.isc_embed_dsql_close	= (isc_embed_dsql_close_t)dlsym(g_hIB_DLL, "isc_embed_dsql_close");	assert(g_ibAPI.isc_embed_dsql_close != NULL);
	g_ibAPI.isc_embed_dsql_declare	= (isc_embed_dsql_declare_t)dlsym(g_hIB_DLL, "isc_embed_dsql_declare");	assert(g_ibAPI.isc_embed_dsql_declare != NULL);
	g_ibAPI.isc_embed_dsql_describe	= (isc_embed_dsql_describe_t)dlsym(g_hIB_DLL, "isc_embed_dsql_describe");	assert(g_ibAPI.isc_embed_dsql_describe != NULL);
	g_ibAPI.isc_embed_dsql_describe_bind	= (isc_embed_dsql_describe_bind_t)dlsym(g_hIB_DLL, "isc_embed_dsql_describe_bind");	assert(g_ibAPI.isc_embed_dsql_describe_bind != NULL);
	g_ibAPI.isc_embed_dsql_execute	= (isc_embed_dsql_execute_t)dlsym(g_hIB_DLL, "isc_embed_dsql_execute");	assert(g_ibAPI.isc_embed_dsql_execute != NULL);
	g_ibAPI.isc_embed_dsql_execute2	= (isc_embed_dsql_execute2_t)dlsym(g_hIB_DLL, "isc_embed_dsql_execute2");	assert(g_ibAPI.isc_embed_dsql_execute2 != NULL);
	g_ibAPI.isc_embed_dsql_execute_immed	= (isc_embed_dsql_execute_immed_t)dlsym(g_hIB_DLL, "isc_embed_dsql_execute_immed");	assert(g_ibAPI.isc_embed_dsql_execute_immed != NULL);
	g_ibAPI.isc_embed_dsql_fetch	= (isc_embed_dsql_fetch_t)dlsym(g_hIB_DLL, "isc_embed_dsql_fetch");	assert(g_ibAPI.isc_embed_dsql_fetch != NULL);
	g_ibAPI.isc_embed_dsql_open	= (isc_embed_dsql_open_t)dlsym(g_hIB_DLL, "isc_embed_dsql_open");	assert(g_ibAPI.isc_embed_dsql_open != NULL);
	g_ibAPI.isc_embed_dsql_open2	= (isc_embed_dsql_open2_t)dlsym(g_hIB_DLL, "isc_embed_dsql_open2");	assert(g_ibAPI.isc_embed_dsql_open2 != NULL);
	g_ibAPI.isc_embed_dsql_insert	= (isc_embed_dsql_insert_t)dlsym(g_hIB_DLL, "isc_embed_dsql_insert");	assert(g_ibAPI.isc_embed_dsql_insert != NULL);
	g_ibAPI.isc_embed_dsql_prepare	= (isc_embed_dsql_prepare_t)dlsym(g_hIB_DLL, "isc_embed_dsql_prepare");	assert(g_ibAPI.isc_embed_dsql_prepare != NULL);
	g_ibAPI.isc_embed_dsql_release	= (isc_embed_dsql_release_t)dlsym(g_hIB_DLL, "isc_embed_dsql_release");	assert(g_ibAPI.isc_embed_dsql_release != NULL);
	g_ibAPI.isc_ftof	= (isc_ftof_t)dlsym(g_hIB_DLL, "isc_ftof");	assert(g_ibAPI.isc_ftof != NULL);
	g_ibAPI.isc_print_blr	= (isc_print_blr_t)dlsym(g_hIB_DLL, "isc_print_blr");	assert(g_ibAPI.isc_print_blr != NULL);
	g_ibAPI.isc_set_debug	= (isc_set_debug_t)dlsym(g_hIB_DLL, "isc_set_debug");	assert(g_ibAPI.isc_set_debug != NULL);
	g_ibAPI.isc_qtoq	= (isc_qtoq_t)dlsym(g_hIB_DLL, "isc_qtoq");	assert(g_ibAPI.isc_qtoq != NULL);
	g_ibAPI.isc_vtof	= (isc_vtof_t)dlsym(g_hIB_DLL, "isc_vtof");	assert(g_ibAPI.isc_vtof != NULL);
	g_ibAPI.isc_vtov	= (isc_vtov_t)dlsym(g_hIB_DLL, "isc_vtov");	assert(g_ibAPI.isc_vtov != NULL);
	g_ibAPI.isc_version	= (isc_version_t)dlsym(g_hIB_DLL, "isc_version");	assert(g_ibAPI.isc_version != NULL);
	g_ibAPI.fb_interpret = (fb_interpret_t)dlsym(g_hIB_DLL, "fb_interpret");
}

static void ResetAPI()
{
	g_ibAPI.isc_attach_database	= NULL;
	g_ibAPI.isc_array_gen_sdl	= NULL;
	g_ibAPI.isc_array_get_slice	= NULL;
	g_ibAPI.isc_array_lookup_bounds	= NULL;
	g_ibAPI.isc_array_lookup_desc	= NULL;
	g_ibAPI.isc_array_set_desc	= NULL;
	g_ibAPI.isc_array_put_slice	= NULL;
	g_ibAPI.isc_blob_default_desc	= NULL;
	g_ibAPI.isc_blob_gen_bpb	= NULL;
	g_ibAPI.isc_blob_info	= NULL;
	g_ibAPI.isc_blob_lookup_desc	= NULL;
	g_ibAPI.isc_blob_set_desc	= NULL;
	g_ibAPI.isc_cancel_blob	= NULL;
	g_ibAPI.isc_cancel_events	= NULL;
	g_ibAPI.isc_close_blob	= NULL;
	g_ibAPI.isc_commit_retaining	= NULL;
	g_ibAPI.isc_commit_transaction	= NULL;
	g_ibAPI.isc_create_blob	= NULL;
	g_ibAPI.isc_create_blob2	= NULL;
	g_ibAPI.isc_create_database	= NULL;
	g_ibAPI.isc_database_info	= NULL;
	g_ibAPI.isc_decode_date	= NULL;
	g_ibAPI.isc_detach_database	= NULL;
	g_ibAPI.isc_drop_database	= NULL;
	g_ibAPI.isc_dsql_allocate_statement	= NULL;
	g_ibAPI.isc_dsql_alloc_statement2	= NULL;
	g_ibAPI.isc_dsql_describe	= NULL;
	g_ibAPI.isc_dsql_describe_bind	= NULL;
	g_ibAPI.isc_dsql_exec_immed2	= NULL;
	g_ibAPI.isc_dsql_execute	= NULL;
	g_ibAPI.isc_dsql_execute2	= NULL;
	g_ibAPI.isc_dsql_execute_immediate	= NULL;
	g_ibAPI.isc_dsql_fetch	= NULL;
	g_ibAPI.isc_dsql_finish	= NULL;
	g_ibAPI.isc_dsql_free_statement	= NULL;
	g_ibAPI.isc_dsql_insert	= NULL;
	g_ibAPI.isc_dsql_prepare	= NULL;
	g_ibAPI.isc_dsql_set_cursor_name	= NULL;
	g_ibAPI.isc_dsql_sql_info	= NULL;
	g_ibAPI.isc_encode_date	= NULL;
	g_ibAPI.isc_event_block	= NULL;
	g_ibAPI.isc_event_counts	= NULL;
	g_ibAPI.isc_expand_dpb	= NULL;
	g_ibAPI.isc_modify_dpb	= NULL;
	g_ibAPI.isc_free	= NULL;
	g_ibAPI.isc_get_segment	= NULL;
	g_ibAPI.isc_get_slice	= NULL;
	g_ibAPI.isc_interprete	= NULL;
	g_ibAPI.isc_open_blob	= NULL;
	g_ibAPI.isc_open_blob2	= NULL;
	g_ibAPI.isc_prepare_transaction2	= NULL;
	g_ibAPI.isc_print_sqlerror	= NULL;
	g_ibAPI.isc_print_status	= NULL;
	g_ibAPI.isc_put_segment	= NULL;
	g_ibAPI.isc_put_slice	= NULL;
	g_ibAPI.isc_que_events	= NULL;
	g_ibAPI.isc_rollback_transaction	= NULL;
	g_ibAPI.isc_start_multiple	= NULL;
	g_ibAPI.isc_start_transaction	= NULL;
	g_ibAPI.isc_sqlcode	= NULL;
	g_ibAPI.isc_sql_interprete	= NULL;
	g_ibAPI.isc_transaction_info	= NULL;
	g_ibAPI.isc_transact_request	= NULL;
	g_ibAPI.isc_vax_integer	= NULL;
	g_ibAPI.isc_add_user	= NULL;
	g_ibAPI.isc_delete_user	= NULL;
	g_ibAPI.isc_modify_user	= NULL;
	g_ibAPI.isc_compile_request	= NULL;
	g_ibAPI.isc_compile_request2	= NULL;
	g_ibAPI.isc_ddl	= NULL;
	g_ibAPI.isc_prepare_transaction	= NULL;
	g_ibAPI.isc_receive	= NULL;
	g_ibAPI.isc_reconnect_transaction	= NULL;
	g_ibAPI.isc_release_request	= NULL;
	g_ibAPI.isc_request_info	= NULL;
	g_ibAPI.isc_seek_blob	= NULL;
	g_ibAPI.isc_send	= NULL;
	g_ibAPI.isc_start_and_send	= NULL;
	g_ibAPI.isc_start_request	= NULL;
	g_ibAPI.isc_unwind_request	= NULL;
	g_ibAPI.isc_wait_for_event	= NULL;
	g_ibAPI.isc_close	= NULL;
	g_ibAPI.isc_declare	= NULL;
	g_ibAPI.isc_execute_immediate	= NULL;
	g_ibAPI.isc_dsql_execute_m	= NULL;
	g_ibAPI.isc_dsql_execute2_m	= NULL;
	g_ibAPI.isc_dsql_execute_immediate_m	= NULL;
	g_ibAPI.isc_dsql_exec_immed3_m	= NULL;
	g_ibAPI.isc_dsql_fetch_m	= NULL;
	g_ibAPI.isc_dsql_insert_m	= NULL;
	g_ibAPI.isc_dsql_prepare_m	= NULL;
	g_ibAPI.isc_dsql_release	= NULL;
	g_ibAPI.isc_embed_dsql_close	= NULL;
	g_ibAPI.isc_embed_dsql_declare	= NULL;
	g_ibAPI.isc_embed_dsql_describe	= NULL;
	g_ibAPI.isc_embed_dsql_describe_bind	= NULL;
	g_ibAPI.isc_embed_dsql_execute	= NULL;
	g_ibAPI.isc_embed_dsql_execute2	= NULL;
	g_ibAPI.isc_embed_dsql_execute_immed	= NULL;
	g_ibAPI.isc_embed_dsql_fetch	= NULL;
	g_ibAPI.isc_embed_dsql_open	= NULL;
	g_ibAPI.isc_embed_dsql_open2	= NULL;
	g_ibAPI.isc_embed_dsql_insert	= NULL;
	g_ibAPI.isc_embed_dsql_prepare	= NULL;
	g_ibAPI.isc_embed_dsql_release	= NULL;
	g_ibAPI.isc_ftof	= NULL;
	g_ibAPI.isc_print_blr	= NULL;
	g_ibAPI.isc_set_debug	= NULL;
	g_ibAPI.isc_qtoq	= NULL;
	g_ibAPI.isc_vtof	= NULL;
	g_ibAPI.isc_vtov	= NULL;
	g_ibAPI.isc_version	= NULL;
	g_ibAPI.fb_interpret = NULL;
}

void AddIBSupport(const SAConnection *pCon)
{
	SACriticalSectionScope scope(&ibLoaderMutex);

	if( ! g_nIB_DLLRefs )
	{
		// load InterBase API library
		SAString sErrorMessage, sLibName, sLibsList = pCon->Option(_TSA("IBASE.LIBS"));
		if( sLibsList.IsEmpty() )
			sLibsList = g_sIB_DLLNames;

		g_hIB_DLL = SALoadLibraryFromList(sLibsList, sErrorMessage, sLibName, RTLD_LAZY);
		if( ! g_hIB_DLL )
			throw SAException(SA_Library_Error, SA_Library_Error_LoadLibraryFails, -1, IDS_LOAD_LIBRARY_FAILS, (const SAChar*)sErrorMessage);
			
		g_nIB_DLLVersionLoaded = 0l;	// Version unknown
		
		LoadAPI();
	}

	if( SAGlobals::UnloadAPI() )
		g_nIB_DLLRefs++;
	else
		g_nIB_DLLRefs = 1;
}

void ReleaseIBSupport()
{
	SACriticalSectionScope scope(&ibLoaderMutex);

	assert(g_nIB_DLLRefs > 0);
	g_nIB_DLLRefs--;
	if(!g_nIB_DLLRefs)
	{
		g_nIB_DLLVersionLoaded = 0l;
		ResetAPI();

		//we can't unload it, it has registered something with atexit()
		//dlclose(g_hIB_DLL);
		g_hIB_DLL = NULL;
	}
}
