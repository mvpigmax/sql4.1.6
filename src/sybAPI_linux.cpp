// sybAPI_linux.cpp
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <sybAPI.h>

#ifndef SHARED_OBJ_SUFFUX
#	if defined(_REENTRANT) || defined(SA_USE_PTHREAD)
#		ifdef SA_64BIT
#			define SHARED_OBJ_SUFFUX "_r64"
#		else
#			define SHARED_OBJ_SUFFUX "_r"
#		endif
#	else
#		ifdef SA_64BIT
#			define SHARED_OBJ_SUFFUX "64"
#		else
#			define SHARED_OBJ_SUFFUX ""
#		endif
#	endif
#endif

//
// System libraries Sybase CD-Lib can require
//
// HPUX(PA-RISK): -lcl -lm -ldld -lBSD
// HPUX(ia64): -lcl -lm -ldld
// Linux: -ldl -lnsl -lm
// Solaris: -lsocket -lnsl -ldl -m
// AIX: -lm

//
// Network libraries, we used to load them - should be loaded by OCS
/*
static const char *g_sDLLName_INSCK = "libinsck" SHARED_OBJ_SUFFUX SHARED_OBJ_EXT;
static void* g_hDLL_INSCK = NULL;
static const char *g_sDLLName_TLI = "libtli" SHARED_OBJ_SUFFUX SHARED_OBJ_EXT;
static void* g_hDLL_TLI = NULL;
*/

//
// Communication libraries
static const char *g_sDLLName_INTL = "libsybintl" SHARED_OBJ_SUFFUX SHARED_OBJ_EXT ":libintl" SHARED_OBJ_SUFFUX SHARED_OBJ_EXT;
static void* g_hDLL_INTL = NULL; // required
static const char *g_sDLLName_COMN = "libsybcomn" SHARED_OBJ_SUFFUX SHARED_OBJ_EXT ":libcomn" SHARED_OBJ_SUFFUX SHARED_OBJ_EXT;
static void* g_hDLL_COMN = NULL; // required
static const char *g_sDLLName_TCL = "libsybtcl" SHARED_OBJ_SUFFUX SHARED_OBJ_EXT ":libtcl" SHARED_OBJ_SUFFUX SHARED_OBJ_EXT;
static void* g_hDLL_TCL = NULL; // required

// From ASE 15.0 - seems should be auto loaded too
/*
static const char *g_sDLLName_UNIC = "libsybunic" SHARED_OBJ_SUFFUX  SHARED_OBJ_EXT;
static void* g_hDLL_UNIC = NULL; // can be NULL
*/

// CT-Lib, CS-Lib
static const char *g_sDLLName_CS = "libsybcs" SHARED_OBJ_SUFFUX SHARED_OBJ_EXT ":libcs" SHARED_OBJ_SUFFUX SHARED_OBJ_EXT;
static void* g_hDLL_CS = NULL; // required
static const char *g_sDLLName_CT = "libsybct" SHARED_OBJ_SUFFUX SHARED_OBJ_EXT ":libct" SHARED_OBJ_SUFFUX SHARED_OBJ_EXT;
static void* g_hDLL_CT = NULL; // required

static long g_nDLLRefs = 0l;

static SAMutex sybLoaderMutex;

// API definitions
sybAPI g_sybAPI;

sybAPI::sybAPI()
{
	ct_debug	= NULL;
	ct_bind	= NULL;
	ct_br_column	= NULL;
	ct_br_table	= NULL;
	ct_callback	= NULL;
	ct_cancel	= NULL;
	ct_capability	= NULL;
	ct_compute_info	= NULL;
	ct_close	= NULL;
	ct_cmd_alloc	= NULL;
	ct_cmd_drop	= NULL;
	ct_cmd_props	= NULL;
	ct_command	= NULL;
	ct_con_alloc	= NULL;
	ct_con_drop	= NULL;
	ct_con_props	= NULL;
	ct_connect	= NULL;
	ct_config	= NULL;
	ct_cursor	= NULL;
	ct_dyndesc	= NULL;
	ct_describe	= NULL;
	ct_diag	= NULL;
	ct_dynamic	= NULL;
	ct_dynsqlda	= NULL;
	ct_exit	= NULL;
	ct_fetch	= NULL;
	ct_getformat	= NULL;
	ct_keydata	= NULL;
	ct_init	= NULL;
	ct_options	= NULL;
	ct_param	= NULL;
	ct_getloginfo	= NULL;
	ct_setloginfo	= NULL;
	ct_recvpassthru	= NULL;
	ct_sendpassthru	= NULL;
	ct_poll	= NULL;
	ct_remote_pwd	= NULL;
	ct_results	= NULL;
	ct_res_info	= NULL;
	ct_send	= NULL;
	ct_get_data	= NULL;
	ct_send_data	= NULL;
	ct_data_info	= NULL;
	ct_wakeup	= NULL;
	ct_labels	= NULL;
	ct_ds_lookup	= NULL;
	ct_ds_dropobj	= NULL;
	ct_ds_objinfo	= NULL;
	ct_setparam	= NULL;

	cs_calc	= NULL;
	cs_cmp	= NULL;
	cs_convert	= NULL;
	cs_will_convert	= NULL;
	cs_set_convert	= NULL;
	cs_setnull	= NULL;
	cs_config	= NULL;
	cs_ctx_alloc	= NULL;
	cs_ctx_drop	= NULL;
	cs_ctx_global	= NULL;
	cs_objects	= NULL;
	cs_diag	= NULL;
	cs_dt_crack	= NULL;
	cs_dt_info	= NULL;
	cs_locale	= NULL;
	cs_loc_alloc	= NULL;
	cs_loc_drop	= NULL;
	cs_strbuild	= NULL;
	cs_strcmp	= NULL;
	cs_time	= NULL;
	cs_manage_convert	= NULL;
	cs_conv_mult	= NULL;

	ct_scroll_fetch = NULL;
}

sybConnectionHandles::sybConnectionHandles()
{
	m_context = NULL;
	m_connection = NULL;
	m_locale = NULL;
}

sybCommandHandles::sybCommandHandles()
{
	m_command = NULL;
}

static void LoadAPI()
{
	// FreeTDS g_sybAPI.ct_debug	= (ct_debug_t)dlsym(g_hDLL_CT, "ct_debug");	assert(g_sybAPI.ct_debug != NULL);
	g_sybAPI.ct_bind	= (ct_bind_t)dlsym(g_hDLL_CT, "ct_bind");	assert(g_sybAPI.ct_bind != NULL);
	// FreeTDS g_sybAPI.ct_br_column	= (ct_br_column_t)dlsym(g_hDLL_CT, "ct_br_column");	assert(g_sybAPI.ct_br_column != NULL);
	// FreeTDS g_sybAPI.ct_br_table	= (ct_br_table_t)dlsym(g_hDLL_CT, "ct_br_table");	assert(g_sybAPI.ct_br_table != NULL);
	g_sybAPI.ct_callback	= (ct_callback_t)dlsym(g_hDLL_CT, "ct_callback");	assert(g_sybAPI.ct_callback != NULL);
	g_sybAPI.ct_cancel	= (ct_cancel_t)dlsym(g_hDLL_CT, "ct_cancel");	assert(g_sybAPI.ct_cancel != NULL);
	g_sybAPI.ct_capability	= (ct_capability_t)dlsym(g_hDLL_CT, "ct_capability");	assert(g_sybAPI.ct_capability != NULL);
	g_sybAPI.ct_compute_info	= (ct_compute_info_t)dlsym(g_hDLL_CT, "ct_compute_info");	assert(g_sybAPI.ct_compute_info != NULL);
	g_sybAPI.ct_close	= (ct_close_t)dlsym(g_hDLL_CT, "ct_close");	assert(g_sybAPI.ct_close != NULL);
	g_sybAPI.ct_cmd_alloc	= (ct_cmd_alloc_t)dlsym(g_hDLL_CT, "ct_cmd_alloc");	assert(g_sybAPI.ct_cmd_alloc != NULL);
	g_sybAPI.ct_cmd_drop	= (ct_cmd_drop_t)dlsym(g_hDLL_CT, "ct_cmd_drop");	assert(g_sybAPI.ct_cmd_drop != NULL);
	g_sybAPI.ct_cmd_props	= (ct_cmd_props_t)dlsym(g_hDLL_CT, "ct_cmd_props");	assert(g_sybAPI.ct_cmd_props != NULL);
	g_sybAPI.ct_command	= (ct_command_t)dlsym(g_hDLL_CT, "ct_command");	assert(g_sybAPI.ct_command != NULL);
	g_sybAPI.ct_con_alloc	= (ct_con_alloc_t)dlsym(g_hDLL_CT, "ct_con_alloc");	assert(g_sybAPI.ct_con_alloc != NULL);
	g_sybAPI.ct_con_drop	= (ct_con_drop_t)dlsym(g_hDLL_CT, "ct_con_drop");	assert(g_sybAPI.ct_con_drop != NULL);
	g_sybAPI.ct_con_props	= (ct_con_props_t)dlsym(g_hDLL_CT, "ct_con_props");	assert(g_sybAPI.ct_con_props != NULL);
	g_sybAPI.ct_connect	= (ct_connect_t)dlsym(g_hDLL_CT, "ct_connect");	assert(g_sybAPI.ct_connect != NULL);
	g_sybAPI.ct_config	= (ct_config_t)dlsym(g_hDLL_CT, "ct_config");	assert(g_sybAPI.ct_config != NULL);
	g_sybAPI.ct_cursor	= (ct_cursor_t)dlsym(g_hDLL_CT, "ct_cursor");	assert(g_sybAPI.ct_cursor != NULL);
	// FreeTDS g_sybAPI.ct_dyndesc	= (ct_dyndesc_t)dlsym(g_hDLL_CT, "ct_dyndesc");	assert(g_sybAPI.ct_dyndesc != NULL);
	g_sybAPI.ct_describe	= (ct_describe_t)dlsym(g_hDLL_CT, "ct_describe");	assert(g_sybAPI.ct_describe != NULL);
	// FreeTDS g_sybAPI.ct_diag	= (ct_diag_t)dlsym(g_hDLL_CT, "ct_diag");	assert(g_sybAPI.ct_diag != NULL);
	g_sybAPI.ct_dynamic	= (ct_dynamic_t)dlsym(g_hDLL_CT, "ct_dynamic");	assert(g_sybAPI.ct_dynamic != NULL);
	g_sybAPI.ct_dynsqlda	= (ct_dynsqlda_t)dlsym(g_hDLL_CT, "ct_dynsqlda"); // assert(g_sybAPI.ct_dynsqlda != NULL);
	g_sybAPI.ct_exit	= (ct_exit_t)dlsym(g_hDLL_CT, "ct_exit");	assert(g_sybAPI.ct_exit != NULL);
	g_sybAPI.ct_fetch	= (ct_fetch_t)dlsym(g_hDLL_CT, "ct_fetch");	assert(g_sybAPI.ct_fetch != NULL);
	// FreeTDS g_sybAPI.ct_getformat	= (ct_getformat_t)dlsym(g_hDLL_CT, "ct_getformat");	assert(g_sybAPI.ct_getformat != NULL);
	// FreeTDS g_sybAPI.ct_keydata	= (ct_keydata_t)dlsym(g_hDLL_CT, "ct_keydata");	assert(g_sybAPI.ct_keydata != NULL);
	g_sybAPI.ct_init	= (ct_init_t)dlsym(g_hDLL_CT, "ct_init");	assert(g_sybAPI.ct_init != NULL);
	g_sybAPI.ct_options	= (ct_options_t)dlsym(g_hDLL_CT, "ct_options");	assert(g_sybAPI.ct_options != NULL);
	g_sybAPI.ct_param	= (syb_ct_param_t)dlsym(g_hDLL_CT, "ct_param");	assert(g_sybAPI.ct_param != NULL);
	// FreeTDS g_sybAPI.ct_getloginfo	= (ct_getloginfo_t)dlsym(g_hDLL_CT, "ct_getloginfo");	assert(g_sybAPI.ct_getloginfo != NULL);
	// FreeTDS g_sybAPI.ct_setloginfo	= (ct_setloginfo_t)dlsym(g_hDLL_CT, "ct_setloginfo");	assert(g_sybAPI.ct_setloginfo != NULL);
	// FreeTDS g_sybAPI.ct_recvpassthru	= (ct_recvpassthru_t)dlsym(g_hDLL_CT, "ct_recvpassthru");	assert(g_sybAPI.ct_recvpassthru != NULL);
	// FreeTDS g_sybAPI.ct_sendpassthru	= (ct_sendpassthru_t)dlsym(g_hDLL_CT, "ct_sendpassthru");	assert(g_sybAPI.ct_sendpassthru != NULL);
	g_sybAPI.ct_poll	= (ct_poll_t)dlsym(g_hDLL_CT, "ct_poll");	assert(g_sybAPI.ct_poll != NULL);
	// FreeTDS g_sybAPI.ct_remote_pwd	= (ct_remote_pwd_t)dlsym(g_hDLL_CT, "ct_remote_pwd");	assert(g_sybAPI.ct_remote_pwd != NULL);
	g_sybAPI.ct_results	= (ct_results_t)dlsym(g_hDLL_CT, "ct_results");	assert(g_sybAPI.ct_results != NULL);
	g_sybAPI.ct_res_info	= (ct_res_info_t)dlsym(g_hDLL_CT, "ct_res_info");	assert(g_sybAPI.ct_res_info != NULL);
	g_sybAPI.ct_send	= (ct_send_t)dlsym(g_hDLL_CT, "ct_send");	assert(g_sybAPI.ct_send != NULL);
	g_sybAPI.ct_get_data	= (ct_get_data_t)dlsym(g_hDLL_CT, "ct_get_data");	assert(g_sybAPI.ct_get_data != NULL);
	g_sybAPI.ct_send_data	= (ct_send_data_t)dlsym(g_hDLL_CT, "ct_send_data");	assert(g_sybAPI.ct_send_data != NULL);
	g_sybAPI.ct_data_info	= (ct_data_info_t)dlsym(g_hDLL_CT, "ct_data_info");	assert(g_sybAPI.ct_data_info != NULL);
	// FreeTDS g_sybAPI.ct_wakeup	= (ct_wakeup_t)dlsym(g_hDLL_CT, "ct_wakeup");	assert(g_sybAPI.ct_wakeup != NULL);
	// FreeTDS g_sybAPI.ct_labels	= (ct_labels_t)dlsym(g_hDLL_CT, "ct_labels");	assert(g_sybAPI.ct_labels != NULL);
	g_sybAPI.ct_ds_lookup	= (ct_ds_lookup_t)dlsym(g_hDLL_CT, "ct_ds_lookup");	// assert(g_sybAPI.ct_ds_lookup != NULL);
	g_sybAPI.ct_ds_dropobj	= (ct_ds_dropobj_t)dlsym(g_hDLL_CT, "ct_ds_dropobj");	// assert(g_sybAPI.ct_ds_dropobj != NULL);
	g_sybAPI.ct_ds_objinfo	= (ct_ds_objinfo_t)dlsym(g_hDLL_CT, "ct_ds_objinfo");	// assert(g_sybAPI.ct_ds_objinfo != NULL);
	g_sybAPI.ct_setparam	= (ct_setparam_t)dlsym(g_hDLL_CT, "ct_setparam");	// assert(g_sybAPI.ct_setparam != NULL);

	g_sybAPI.ct_scroll_fetch = (ct_scroll_fetch_t)dlsym(g_hDLL_CT, "ct_scroll_fetch");

	g_sybAPI.cs_calc	= (cs_calc_t)dlsym(g_hDLL_CS, "cs_calc");	assert(g_sybAPI.cs_calc != NULL);
	g_sybAPI.cs_cmp	= (cs_cmp_t)dlsym(g_hDLL_CS, "cs_cmp");	assert(g_sybAPI.cs_cmp != NULL);
	g_sybAPI.cs_convert	= (cs_convert_t)dlsym(g_hDLL_CS, "cs_convert");	assert(g_sybAPI.cs_convert != NULL);
	g_sybAPI.cs_will_convert	= (cs_will_convert_t)dlsym(g_hDLL_CS, "cs_will_convert");	assert(g_sybAPI.cs_will_convert != NULL);
	g_sybAPI.cs_set_convert	= (cs_set_convert_t)dlsym(g_hDLL_CS, "cs_set_convert");	assert(g_sybAPI.cs_set_convert != NULL);
	g_sybAPI.cs_setnull	= (cs_setnull_t)dlsym(g_hDLL_CS, "cs_setnull");	assert(g_sybAPI.cs_setnull != NULL);
	g_sybAPI.cs_config	= (cs_config_t)dlsym(g_hDLL_CS, "cs_config");	assert(g_sybAPI.cs_config != NULL);
	g_sybAPI.cs_ctx_alloc	= (cs_ctx_alloc_t)dlsym(g_hDLL_CS, "cs_ctx_alloc");	assert(g_sybAPI.cs_ctx_alloc != NULL);
	g_sybAPI.cs_ctx_drop	= (cs_ctx_drop_t)dlsym(g_hDLL_CS, "cs_ctx_drop");	assert(g_sybAPI.cs_ctx_drop != NULL);
	g_sybAPI.cs_ctx_global	= (cs_ctx_global_t)dlsym(g_hDLL_CS, "cs_ctx_global");	assert(g_sybAPI.cs_ctx_global != NULL);
	g_sybAPI.cs_objects	= (cs_objects_t)dlsym(g_hDLL_CS, "cs_objects");	assert(g_sybAPI.cs_objects != NULL);
	g_sybAPI.cs_diag	= (cs_diag_t)dlsym(g_hDLL_CS, "cs_diag");	assert(g_sybAPI.cs_diag != NULL);
	g_sybAPI.cs_dt_crack	= (cs_dt_crack_t)dlsym(g_hDLL_CS, "cs_dt_crack");	assert(g_sybAPI.cs_dt_crack != NULL);
	g_sybAPI.cs_dt_info	= (cs_dt_info_t)dlsym(g_hDLL_CS, "cs_dt_info");	assert(g_sybAPI.cs_dt_info != NULL);
	g_sybAPI.cs_locale	= (cs_locale_t)dlsym(g_hDLL_CS, "cs_locale");	assert(g_sybAPI.cs_locale != NULL);
	g_sybAPI.cs_loc_alloc	= (cs_loc_alloc_t)dlsym(g_hDLL_CS, "cs_loc_alloc");	assert(g_sybAPI.cs_loc_alloc != NULL);
	g_sybAPI.cs_loc_drop	= (cs_loc_drop_t)dlsym(g_hDLL_CS, "cs_loc_drop");	assert(g_sybAPI.cs_loc_drop != NULL);
	g_sybAPI.cs_strbuild	= (cs_strbuild_t)dlsym(g_hDLL_CS, "cs_strbuild");	assert(g_sybAPI.cs_strbuild != NULL);
	g_sybAPI.cs_strcmp	= (cs_strcmp_t)dlsym(g_hDLL_CS, "cs_strcmp");	assert(g_sybAPI.cs_strcmp != NULL);
	g_sybAPI.cs_time	= (cs_time_t)dlsym(g_hDLL_CS, "cs_time");	assert(g_sybAPI.cs_time != NULL);
	g_sybAPI.cs_manage_convert	= (cs_manage_convert_t)dlsym(g_hDLL_CS, "cs_manage_convert");	// assert(g_sybAPI.cs_manage_convert != NULL);
	g_sybAPI.cs_conv_mult	= (cs_conv_mult_t)dlsym(g_hDLL_CS, "cs_conv_mult");	// assert(g_sybAPI.cs_conv_mult != NULL);
}

static void ResetAPI()
{
	g_sybAPI.ct_debug	= NULL;
	g_sybAPI.ct_bind	= NULL;
	g_sybAPI.ct_br_column	= NULL;
	g_sybAPI.ct_br_table	= NULL;
	g_sybAPI.ct_callback	= NULL;
	g_sybAPI.ct_cancel	= NULL;
	g_sybAPI.ct_capability	= NULL;
	g_sybAPI.ct_compute_info	= NULL;
	g_sybAPI.ct_close	= NULL;
	g_sybAPI.ct_cmd_alloc	= NULL;
	g_sybAPI.ct_cmd_drop	= NULL;
	g_sybAPI.ct_cmd_props	= NULL;
	g_sybAPI.ct_command	= NULL;
	g_sybAPI.ct_con_alloc	= NULL;
	g_sybAPI.ct_con_drop	= NULL;
	g_sybAPI.ct_con_props	= NULL;
	g_sybAPI.ct_connect	= NULL;
	g_sybAPI.ct_config	= NULL;
	g_sybAPI.ct_cursor	= NULL;
	g_sybAPI.ct_dyndesc	= NULL;
	g_sybAPI.ct_describe	= NULL;
	g_sybAPI.ct_diag	= NULL;
	g_sybAPI.ct_dynamic	= NULL;
	g_sybAPI.ct_dynsqlda	= NULL;
	g_sybAPI.ct_exit	= NULL;
	g_sybAPI.ct_fetch	= NULL;
	g_sybAPI.ct_getformat	= NULL;
	g_sybAPI.ct_keydata	= NULL;
	g_sybAPI.ct_init	= NULL;
	g_sybAPI.ct_options	= NULL;
	g_sybAPI.ct_param	= NULL;
	g_sybAPI.ct_getloginfo	= NULL;
	g_sybAPI.ct_setloginfo	= NULL;
	g_sybAPI.ct_recvpassthru	= NULL;
	g_sybAPI.ct_sendpassthru	= NULL;
	g_sybAPI.ct_poll	= NULL;
	g_sybAPI.ct_remote_pwd	= NULL;
	g_sybAPI.ct_results	= NULL;
	g_sybAPI.ct_res_info	= NULL;
	g_sybAPI.ct_send	= NULL;
	g_sybAPI.ct_get_data	= NULL;
	g_sybAPI.ct_send_data	= NULL;
	g_sybAPI.ct_data_info	= NULL;
	g_sybAPI.ct_wakeup	= NULL;
	g_sybAPI.ct_labels	= NULL;
	g_sybAPI.ct_ds_lookup	= NULL;
	g_sybAPI.ct_ds_dropobj	= NULL;
	g_sybAPI.ct_ds_objinfo	= NULL;
	g_sybAPI.ct_setparam	= NULL;

	g_sybAPI.cs_calc	= NULL;
	g_sybAPI.cs_cmp	= NULL;
	g_sybAPI.cs_convert	= NULL;
	g_sybAPI.cs_will_convert	= NULL;
	g_sybAPI.cs_set_convert	= NULL;
	g_sybAPI.cs_setnull	= NULL;
	g_sybAPI.cs_config	= NULL;
	g_sybAPI.cs_ctx_alloc	= NULL;
	g_sybAPI.cs_ctx_drop	= NULL;
	g_sybAPI.cs_ctx_global	= NULL;
	g_sybAPI.cs_objects	= NULL;
	g_sybAPI.cs_diag	= NULL;
	g_sybAPI.cs_dt_crack	= NULL;
	g_sybAPI.cs_dt_info	= NULL;
	g_sybAPI.cs_locale	= NULL;
	g_sybAPI.cs_loc_alloc	= NULL;
	g_sybAPI.cs_loc_drop	= NULL;
	g_sybAPI.cs_strbuild	= NULL;
	g_sybAPI.cs_strcmp	= NULL;
	g_sybAPI.cs_time	= NULL;
	g_sybAPI.cs_manage_convert	= NULL;
	g_sybAPI.cs_conv_mult	= NULL;

	g_sybAPI.ct_scroll_fetch = NULL;
}

void AddSybSupport(const SAConnection *pCon)
{
	SACriticalSectionScope scope(&sybLoaderMutex);

	if( ! g_nDLLRefs )
	{
		try
		{
			// WARNING:
			// adding RTLD_GLOBAL flag will cause core dump 
			// at ct_exit on some systems,
			// for example on Alpha Compaq True64 UNIX

			SAString sErrorMessage, sLibName, sLibsList = pCon->Option(_TSA("SYBINTL.LIBS"));
			if( sLibsList.IsEmpty() )
				sLibsList = g_sDLLName_INTL;
			g_hDLL_INTL = SALoadLibraryFromList(sLibsList, sErrorMessage, sLibName, RTLD_LAZY | RTLD_GLOBAL);
			if( NULL == g_hDLL_INTL )
				throw SAException(SA_Library_Error, SA_Library_Error_LoadLibraryFails, -1, IDS_LOAD_LIBRARY_FAILS, (const SAChar*)sErrorMessage);

			sLibsList = pCon->Option(_TSA("SYBCOMN.LIBS"));
			if( sLibsList.IsEmpty() )
				sLibsList = g_sDLLName_COMN;
			g_hDLL_COMN = SALoadLibraryFromList(sLibsList, sErrorMessage, sLibName, RTLD_LAZY | RTLD_GLOBAL);
			if( NULL == g_hDLL_COMN )
				throw SAException(SA_Library_Error, SA_Library_Error_LoadLibraryFails, -1, IDS_LOAD_LIBRARY_FAILS, (const SAChar*)sErrorMessage);

			sLibsList = pCon->Option(_TSA("SYBTCL.LIBS"));
			if( sLibsList.IsEmpty() )
				sLibsList = g_sDLLName_TCL;
			g_hDLL_TCL = SALoadLibraryFromList(sLibsList, sErrorMessage, sLibName, RTLD_LAZY | RTLD_GLOBAL);
			if( NULL == g_hDLL_TCL )
				throw SAException(SA_Library_Error, SA_Library_Error_LoadLibraryFails, -1, IDS_LOAD_LIBRARY_FAILS, (const SAChar*)sErrorMessage);
			
			sLibsList = pCon->Option(_TSA("SYBCS.LIBS"));
			if( sLibsList.IsEmpty() )
				sLibsList = g_sDLLName_CS;
			g_hDLL_CS = SALoadLibraryFromList(sLibsList, sErrorMessage, sLibName, RTLD_LAZY | RTLD_GLOBAL);
			if( NULL == g_hDLL_CS )
				throw SAException(SA_Library_Error, SA_Library_Error_LoadLibraryFails, -1, IDS_LOAD_LIBRARY_FAILS, (const SAChar*)sErrorMessage);

			sLibsList = pCon->Option(_TSA("SYBCT.LIBS"));
			if( sLibsList.IsEmpty() )
				sLibsList = g_sDLLName_CT;
			g_hDLL_CT = SALoadLibraryFromList(sLibsList, sErrorMessage, sLibName, RTLD_LAZY | RTLD_GLOBAL);
			if( NULL == g_hDLL_CT )
				throw SAException(SA_Library_Error, SA_Library_Error_LoadLibraryFails, -1, IDS_LOAD_LIBRARY_FAILS, (const SAChar*)sErrorMessage);

			LoadAPI();
		}
		catch(SAException &)	// clean up on error
		{
			if( NULL != g_hDLL_CT )
				::dlclose(g_hDLL_CT);
			g_hDLL_CT = NULL;
			if( NULL != g_hDLL_CS )
				::dlclose(g_hDLL_CS);
			g_hDLL_CS = NULL;
			if( NULL != g_hDLL_TCL )
				::dlclose(g_hDLL_TCL);
			g_hDLL_TCL = NULL;
			if( NULL != g_hDLL_COMN )
				::dlclose(g_hDLL_COMN);
			g_hDLL_COMN = NULL;
			if( NULL != g_hDLL_INTL )
				::dlclose(g_hDLL_INTL);
			g_hDLL_INTL = NULL;

			throw;
		}
	}

	if( SAGlobals::UnloadAPI() )
		g_nDLLRefs++;
	else
		g_nDLLRefs = 1;
}

void ReleaseSybSupport()
{
	SACriticalSectionScope scope(&sybLoaderMutex);

	assert(g_nDLLRefs > 0);
	--g_nDLLRefs;
	if(!g_nDLLRefs)
	{
		ResetAPI();

		::dlclose(g_hDLL_CT);
		if( g_hDLL_CS == g_hDLL_CT )
			g_hDLL_CS = NULL;
		g_hDLL_CT = NULL;
		if( NULL != g_hDLL_CS )
			::dlclose(g_hDLL_CS);
		g_hDLL_CS = NULL;

		if( NULL != g_hDLL_TCL )
			::dlclose(g_hDLL_TCL);
		g_hDLL_TCL = NULL;
		if( NULL != g_hDLL_COMN )
			::dlclose(g_hDLL_COMN);
		g_hDLL_COMN = NULL;
		if( NULL != g_hDLL_INTL )
			::dlclose(g_hDLL_INTL);
		g_hDLL_INTL = NULL;
	}
}
