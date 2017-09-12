// sbAPI.cpp
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include <sbAPI.h>

static const char *g_sSBDLLNames = "libsqlbapl" SHARED_OBJ_EXT;
static void *g_hSBDLL = NULL;
long g_nSBDLLVersionLoaded = 0l;
static long g_nSBDLLRefs = 0l;

static SAMutex sbLoaderMutex;

// API declarations
sb7API g_sb7API;
sb6API &g_sb6API = g_sb7API;

sb6API::sb6API()
{
	sqlarf	= NULL;
	sqlbbr	= NULL;
	sqlbdb	= NULL;
	sqlbef	= NULL;
	sqlber	= NULL;
	sqlbkp	= NULL;
	sqlbld	= NULL;
	sqlblf	= NULL;
	sqlblk	= NULL;
	sqlbln	= NULL;
	sqlbna	= NULL;
	sqlbnd	= NULL;
	sqlbnn	= NULL;
	sqlbnu	= NULL;
	sqlbss	= NULL;
	sqlcan	= NULL;
	sqlcbv	= NULL;
	sqlcdr	= NULL;
	sqlcex	= NULL;
	sqlclf	= NULL;
	sqlcmt	= NULL;
	sqlcnc	= NULL;
	sqlcnr	= NULL;
	sqlcom	= NULL;
	sqlcon	= NULL;
	sqlcpy	= NULL;
	sqlcre	= NULL;
	sqlcrf	= NULL;
	sqlcrs	= NULL;
	sqlcsv	= NULL;
	sqlcty	= NULL;
	sqldbn	= NULL;
	sqlded	= NULL;
	sqldel	= NULL;
	sqldes	= NULL;
	sqldid	= NULL;
	sqldii	= NULL;
	sqldin	= NULL;
	sqldir	= NULL;
	sqldis	= NULL;
	sqldon	= NULL;
	sqldox	= NULL;
	sqldrc	= NULL;
	sqldro	= NULL;
	sqldrr	= NULL;
	sqldrs	= NULL;
	sqldsc	= NULL;
	sqldst	= NULL;
	sqldsv	= NULL;
	sqlebk	= NULL;
	sqlefb	= NULL;
	sqlelo	= NULL;
	sqlenr	= NULL;
	sqlepo	= NULL;
	sqlerf	= NULL;
	sqlerr	= NULL;
	sqlers	= NULL;
	sqletx	= NULL;
	sqlexe	= NULL;
	sqlexp	= NULL;
	sqlfbk	= NULL;
	sqlfer	= NULL;
	sqlfet	= NULL;
	sqlfgt	= NULL;
	sqlfpt	= NULL;
	sqlfqn	= NULL;
	sqlgbi	= NULL;
	sqlgdi	= NULL;
	sqlget	= NULL;
	sqlgfi	= NULL;
	sqlgls	= NULL;
	sqlgnl	= NULL;
	sqlgnr	= NULL;
	sqlgsi	= NULL;
	sqlidb	= NULL;
	sqlims	= NULL;
	sqlind	= NULL;
	sqlini	= NULL;
	sqlins	= NULL;
	sqllab	= NULL;
	sqlldp	= NULL;
	sqllsk	= NULL;
	sqlmcl	= NULL;
	sqlmdl	= NULL;
	sqlmop	= NULL;
	sqlmrd	= NULL;
	sqlmsk	= NULL;
	sqlmwr	= NULL;
	sqlnbv	= NULL;
	sqlnii	= NULL;
	sqlnrr	= NULL;
	sqlnsi	= NULL;
	sqloms	= NULL;
	sqlprs	= NULL;
	sqlrbf	= NULL;
	sqlrbk	= NULL;
	sqlrcd	= NULL;
	sqlrdb	= NULL;
	sqlrdc	= NULL;
	sqlrel	= NULL;
	sqlres	= NULL;
	sqlret	= NULL;
	sqlrlf	= NULL;
	sqlrlo	= NULL;
	sqlrof	= NULL;
	sqlrow	= NULL;
	sqlrrd	= NULL;
	sqlrrs	= NULL;
	sqlrsi	= NULL;
	sqlrss	= NULL;
	sqlsab	= NULL;
	sqlsap	= NULL;
	sqlscl	= NULL;
	sqlscn	= NULL;
	sqlscp	= NULL;
	sqlsdn	= NULL;
	sqlsds	= NULL;
	sqlsdx	= NULL;
	sqlset	= NULL;
	sqlsil	= NULL;
	sqlslp	= NULL;
	sqlspr	= NULL;
	sqlsrf	= NULL;
	sqlsrs	= NULL;
	sqlssb	= NULL;
	sqlsss	= NULL;
	sqlsta	= NULL;
	sqlstm	= NULL;
	sqlsto	= NULL;
	sqlstr	= NULL;
	sqlsxt	= NULL;
	sqlsys	= NULL;
	sqltec	= NULL;
	sqltem	= NULL;
	sqltio	= NULL;
	sqlunl	= NULL;
	sqlurs	= NULL;
	sqlwdc	= NULL;
	sqlwlo	= NULL;
	sqlxad	= NULL;
	sqlxcn	= NULL;
	sqlxda	= NULL;
	sqlxdp	= NULL;
	sqlxdv	= NULL;
	sqlxer	= NULL;
	sqlxml	= NULL;
	sqlxnp	= NULL;
	sqlxpd	= NULL;
	sqlxsb	= NULL;
}

// version 7 specific
sb7API::sb7API()
{
	sqlcch	= NULL;
	sqldch	= NULL;
	sqlopc	= NULL;
}

sb6ConnectionHandles::sb6ConnectionHandles()
{
}

sb7ConnectionHandles::sb7ConnectionHandles()
{
}

sbCommandHandles::sbCommandHandles()
{
	m_cur = 0;
}

// detects if version 7 or higher is available
bool CanBeLoadedSB7(const SAConnection * /*pCon*/)
{
	// always true for Linux/Unix
	return true;
}

static void Load6API()
{
//	g_sb6API.sqlarf	= (sqlarf_t)dlsym(g_hSBDLL, "sqlarf");	assert(g_sb6API.sqlarf != NULL);
	g_sb6API.sqlbbr	= (sqlbbr_t)dlsym(g_hSBDLL, "sqlbbr");	assert(g_sb6API.sqlbbr != NULL);
	g_sb6API.sqlbdb	= (sqlbdb_t)dlsym(g_hSBDLL, "sqlbdb");	assert(g_sb6API.sqlbdb != NULL);
	g_sb6API.sqlbef	= (sqlbef_t)dlsym(g_hSBDLL, "sqlbef");	assert(g_sb6API.sqlbef != NULL);
	g_sb6API.sqlber	= (sqlber_t)dlsym(g_hSBDLL, "sqlber");	assert(g_sb6API.sqlber != NULL);
	g_sb6API.sqlbkp	= (sqlbkp_t)dlsym(g_hSBDLL, "sqlbkp");	assert(g_sb6API.sqlbkp != NULL);
	g_sb6API.sqlbld	= (sqlbld_t)dlsym(g_hSBDLL, "sqlbld");	assert(g_sb6API.sqlbld != NULL);
	g_sb6API.sqlblf	= (sqlblf_t)dlsym(g_hSBDLL, "sqlblf");	assert(g_sb6API.sqlblf != NULL);
	g_sb6API.sqlblk	= (sqlblk_t)dlsym(g_hSBDLL, "sqlblk");	assert(g_sb6API.sqlblk != NULL);
	g_sb6API.sqlbln	= (sqlbln_t)dlsym(g_hSBDLL, "sqlbln");	assert(g_sb6API.sqlbln != NULL);
	g_sb6API.sqlbna	= (sqlbna_t)dlsym(g_hSBDLL, "sqlbna");	assert(g_sb6API.sqlbna != NULL);
	g_sb6API.sqlbnd	= (sqlbnd_t)dlsym(g_hSBDLL, "sqlbnd");	assert(g_sb6API.sqlbnd != NULL);
	g_sb6API.sqlbnn	= (sqlbnn_t)dlsym(g_hSBDLL, "sqlbnn");	assert(g_sb6API.sqlbnn != NULL);
	g_sb6API.sqlbnu	= (sqlbnu_t)dlsym(g_hSBDLL, "sqlbnu");	assert(g_sb6API.sqlbnu != NULL);
	g_sb6API.sqlbss	= (sqlbss_t)dlsym(g_hSBDLL, "sqlbss");	assert(g_sb6API.sqlbss != NULL);
	g_sb6API.sqlcan	= (sqlcan_t)dlsym(g_hSBDLL, "sqlcan");	assert(g_sb6API.sqlcan != NULL);
	g_sb6API.sqlcbv	= (sqlcbv_t)dlsym(g_hSBDLL, "sqlcbv");	assert(g_sb6API.sqlcbv != NULL);
	g_sb6API.sqlcdr	= (sqlcdr_t)dlsym(g_hSBDLL, "sqlcdr");	assert(g_sb6API.sqlcdr != NULL);
	g_sb6API.sqlcex	= (sqlcex_t)dlsym(g_hSBDLL, "sqlcex");	assert(g_sb6API.sqlcex != NULL);
	g_sb6API.sqlclf	= (sqlclf_t)dlsym(g_hSBDLL, "sqlclf");	assert(g_sb6API.sqlclf != NULL);
	g_sb6API.sqlcmt	= (sqlcmt_t)dlsym(g_hSBDLL, "sqlcmt");	assert(g_sb6API.sqlcmt != NULL);
	g_sb6API.sqlcnc	= (sqlcnc_t)dlsym(g_hSBDLL, "sqlcnc");	assert(g_sb6API.sqlcnc != NULL);
	g_sb6API.sqlcnr	= (sqlcnr_t)dlsym(g_hSBDLL, "sqlcnr");	assert(g_sb6API.sqlcnr != NULL);
	g_sb6API.sqlcom	= (sqlcom_t)dlsym(g_hSBDLL, "sqlcom");	assert(g_sb6API.sqlcom != NULL);
	g_sb6API.sqlcon	= (sqlcon_t)dlsym(g_hSBDLL, "sqlcon");	assert(g_sb6API.sqlcon != NULL);
	g_sb6API.sqlcpy	= (sqlcpy_t)dlsym(g_hSBDLL, "sqlcpy");	assert(g_sb6API.sqlcpy != NULL);
	g_sb6API.sqlcre	= (sqlcre_t)dlsym(g_hSBDLL, "sqlcre");	assert(g_sb6API.sqlcre != NULL);
	g_sb6API.sqlcrf	= (sqlcrf_t)dlsym(g_hSBDLL, "sqlcrf");	assert(g_sb6API.sqlcrf != NULL);
	g_sb6API.sqlcrs	= (sqlcrs_t)dlsym(g_hSBDLL, "sqlcrs");	assert(g_sb6API.sqlcrs != NULL);
	g_sb6API.sqlcsv	= (sqlcsv_t)dlsym(g_hSBDLL, "sqlcsv");	assert(g_sb6API.sqlcsv != NULL);
	g_sb6API.sqlcty	= (sqlcty_t)dlsym(g_hSBDLL, "sqlcty");	assert(g_sb6API.sqlcty != NULL);
	g_sb6API.sqldbn	= (sqldbn_t)dlsym(g_hSBDLL, "sqldbn");	assert(g_sb6API.sqldbn != NULL);
	g_sb6API.sqlded	= (sqlded_t)dlsym(g_hSBDLL, "sqlded");	assert(g_sb6API.sqlded != NULL);
	g_sb6API.sqldel	= (sqldel_t)dlsym(g_hSBDLL, "sqldel");	assert(g_sb6API.sqldel != NULL);
	g_sb6API.sqldes	= (sqldes_t)dlsym(g_hSBDLL, "sqldes");	assert(g_sb6API.sqldes != NULL);
	g_sb6API.sqldid	= (sqldid_t)dlsym(g_hSBDLL, "sqldid");	assert(g_sb6API.sqldid != NULL);
	g_sb6API.sqldii	= (sqldii_t)dlsym(g_hSBDLL, "sqldii");	assert(g_sb6API.sqldii != NULL);
	g_sb6API.sqldin	= (sqldin_t)dlsym(g_hSBDLL, "sqldin");	assert(g_sb6API.sqldin != NULL);
	g_sb6API.sqldir	= (sqldir_t)dlsym(g_hSBDLL, "sqldir");	assert(g_sb6API.sqldir != NULL);
	g_sb6API.sqldis	= (sqldis_t)dlsym(g_hSBDLL, "sqldis");	assert(g_sb6API.sqldis != NULL);
	g_sb6API.sqldon	= (sqldon_t)dlsym(g_hSBDLL, "sqldon");	assert(g_sb6API.sqldon != NULL);
	g_sb6API.sqldox	= (sqldox_t)dlsym(g_hSBDLL, "sqldox");	assert(g_sb6API.sqldox != NULL);
	g_sb6API.sqldrc	= (sqldrc_t)dlsym(g_hSBDLL, "sqldrc");	assert(g_sb6API.sqldrc != NULL);
	g_sb6API.sqldro	= (sqldro_t)dlsym(g_hSBDLL, "sqldro");	assert(g_sb6API.sqldro != NULL);
	g_sb6API.sqldrr	= (sqldrr_t)dlsym(g_hSBDLL, "sqldrr");	assert(g_sb6API.sqldrr != NULL);
	g_sb6API.sqldrs	= (sqldrs_t)dlsym(g_hSBDLL, "sqldrs");	assert(g_sb6API.sqldrs != NULL);
	g_sb6API.sqldsc	= (sqldsc_t)dlsym(g_hSBDLL, "sqldsc");	assert(g_sb6API.sqldsc != NULL);
	g_sb6API.sqldst	= (sqldst_t)dlsym(g_hSBDLL, "sqldst");	assert(g_sb6API.sqldst != NULL);
	g_sb6API.sqldsv	= (sqldsv_t)dlsym(g_hSBDLL, "sqldsv");	assert(g_sb6API.sqldsv != NULL);
	g_sb6API.sqlebk	= (sqlebk_t)dlsym(g_hSBDLL, "sqlebk");	assert(g_sb6API.sqlebk != NULL);
	g_sb6API.sqlefb	= (sqlefb_t)dlsym(g_hSBDLL, "sqlefb");	assert(g_sb6API.sqlefb != NULL);
	g_sb6API.sqlelo	= (sqlelo_t)dlsym(g_hSBDLL, "sqlelo");	assert(g_sb6API.sqlelo != NULL);
	g_sb6API.sqlenr	= (sqlenr_t)dlsym(g_hSBDLL, "sqlenr");	assert(g_sb6API.sqlenr != NULL);
	g_sb6API.sqlepo	= (sqlepo_t)dlsym(g_hSBDLL, "sqlepo");	assert(g_sb6API.sqlepo != NULL);
	g_sb6API.sqlerf	= (sqlerf_t)dlsym(g_hSBDLL, "sqlerf");	assert(g_sb6API.sqlerf != NULL);
	g_sb6API.sqlerr	= (sqlerr_t)dlsym(g_hSBDLL, "sqlerr");	assert(g_sb6API.sqlerr != NULL);
	g_sb6API.sqlers	= (sqlers_t)dlsym(g_hSBDLL, "sqlers");	assert(g_sb6API.sqlers != NULL);
	g_sb6API.sqletx	= (sqletx_t)dlsym(g_hSBDLL, "sqletx");	assert(g_sb6API.sqletx != NULL);
	g_sb6API.sqlexe	= (sqlexe_t)dlsym(g_hSBDLL, "sqlexe");	assert(g_sb6API.sqlexe != NULL);
	g_sb6API.sqlexp	= (sqlexp_t)dlsym(g_hSBDLL, "sqlexp");	assert(g_sb6API.sqlexp != NULL);
	g_sb6API.sqlfbk	= (sqlfbk_t)dlsym(g_hSBDLL, "sqlfbk");	assert(g_sb6API.sqlfbk != NULL);
	g_sb6API.sqlfer	= (sqlfer_t)dlsym(g_hSBDLL, "sqlfer");	assert(g_sb6API.sqlfer != NULL);
	g_sb6API.sqlfet	= (sqlfet_t)dlsym(g_hSBDLL, "sqlfet");	assert(g_sb6API.sqlfet != NULL);
	g_sb6API.sqlfgt	= (sqlfgt_t)dlsym(g_hSBDLL, "sqlfgt");	assert(g_sb6API.sqlfgt != NULL);
	g_sb6API.sqlfpt	= (sqlfpt_t)dlsym(g_hSBDLL, "sqlfpt");	assert(g_sb6API.sqlfpt != NULL);
	g_sb6API.sqlfqn	= (sqlfqn_t)dlsym(g_hSBDLL, "sqlfqn");	assert(g_sb6API.sqlfqn != NULL);
	g_sb6API.sqlgbi	= (sqlgbi_t)dlsym(g_hSBDLL, "sqlgbi");	assert(g_sb6API.sqlgbi != NULL);
	g_sb6API.sqlgdi	= (sqlgdi_t)dlsym(g_hSBDLL, "sqlgdi");	assert(g_sb6API.sqlgdi != NULL);
	g_sb6API.sqlget	= (sqlget_t)dlsym(g_hSBDLL, "sqlget");	assert(g_sb6API.sqlget != NULL);
	g_sb6API.sqlgfi	= (sqlgfi_t)dlsym(g_hSBDLL, "sqlgfi");	assert(g_sb6API.sqlgfi != NULL);
	g_sb6API.sqlgls	= (sqlgls_t)dlsym(g_hSBDLL, "sqlgls");	assert(g_sb6API.sqlgls != NULL);
	g_sb6API.sqlgnl	= (sqlgnl_t)dlsym(g_hSBDLL, "sqlgnl");	assert(g_sb6API.sqlgnl != NULL);
	g_sb6API.sqlgnr	= (sqlgnr_t)dlsym(g_hSBDLL, "sqlgnr");	assert(g_sb6API.sqlgnr != NULL);
	g_sb6API.sqlgsi	= (sqlgsi_t)dlsym(g_hSBDLL, "sqlgsi");	assert(g_sb6API.sqlgsi != NULL);
	g_sb6API.sqlidb	= (sqlidb_t)dlsym(g_hSBDLL, "sqlidb");	assert(g_sb6API.sqlidb != NULL);
	g_sb6API.sqlims	= (sqlims_t)dlsym(g_hSBDLL, "sqlims");	assert(g_sb6API.sqlims != NULL);
	g_sb6API.sqlind	= (sqlind_t)dlsym(g_hSBDLL, "sqlind");	assert(g_sb6API.sqlind != NULL);
	g_sb6API.sqlini	= (sqlini_t)dlsym(g_hSBDLL, "sqlini");	assert(g_sb6API.sqlini != NULL);
	g_sb6API.sqlins	= (sqlins_t)dlsym(g_hSBDLL, "sqlins");	assert(g_sb6API.sqlins != NULL);
	g_sb6API.sqllab	= (sqllab_t)dlsym(g_hSBDLL, "sqllab");	assert(g_sb6API.sqllab != NULL);
	g_sb6API.sqlldp	= (sqlldp_t)dlsym(g_hSBDLL, "sqlldp");	assert(g_sb6API.sqlldp != NULL);
	g_sb6API.sqllsk	= (sqllsk_t)dlsym(g_hSBDLL, "sqllsk");	assert(g_sb6API.sqllsk != NULL);
	g_sb6API.sqlmcl	= (sqlmcl_t)dlsym(g_hSBDLL, "sqlmcl");	assert(g_sb6API.sqlmcl != NULL);
	g_sb6API.sqlmdl	= (sqlmdl_t)dlsym(g_hSBDLL, "sqlmdl");	assert(g_sb6API.sqlmdl != NULL);
	g_sb6API.sqlmop	= (sqlmop_t)dlsym(g_hSBDLL, "sqlmop");	assert(g_sb6API.sqlmop != NULL);
	g_sb6API.sqlmrd	= (sqlmrd_t)dlsym(g_hSBDLL, "sqlmrd");	assert(g_sb6API.sqlmrd != NULL);
	g_sb6API.sqlmsk	= (sqlmsk_t)dlsym(g_hSBDLL, "sqlmsk");	assert(g_sb6API.sqlmsk != NULL);
	g_sb6API.sqlmwr	= (sqlmwr_t)dlsym(g_hSBDLL, "sqlmwr");	assert(g_sb6API.sqlmwr != NULL);
	g_sb6API.sqlnbv	= (sqlnbv_t)dlsym(g_hSBDLL, "sqlnbv");	assert(g_sb6API.sqlnbv != NULL);
	g_sb6API.sqlnii	= (sqlnii_t)dlsym(g_hSBDLL, "sqlnii");	assert(g_sb6API.sqlnii != NULL);
	g_sb6API.sqlnrr	= (sqlnrr_t)dlsym(g_hSBDLL, "sqlnrr");	assert(g_sb6API.sqlnrr != NULL);
	g_sb6API.sqlnsi	= (sqlnsi_t)dlsym(g_hSBDLL, "sqlnsi");	assert(g_sb6API.sqlnsi != NULL);
	g_sb6API.sqloms	= (sqloms_t)dlsym(g_hSBDLL, "sqloms");	assert(g_sb6API.sqloms != NULL);
	g_sb6API.sqlprs	= (sqlprs_t)dlsym(g_hSBDLL, "sqlprs");	assert(g_sb6API.sqlprs != NULL);
	g_sb6API.sqlrbf	= (sqlrbf_t)dlsym(g_hSBDLL, "sqlrbf");	assert(g_sb6API.sqlrbf != NULL);
	g_sb6API.sqlrbk	= (sqlrbk_t)dlsym(g_hSBDLL, "sqlrbk");	assert(g_sb6API.sqlrbk != NULL);
	g_sb6API.sqlrcd	= (sqlrcd_t)dlsym(g_hSBDLL, "sqlrcd");	assert(g_sb6API.sqlrcd != NULL);
	g_sb6API.sqlrdb	= (sqlrdb_t)dlsym(g_hSBDLL, "sqlrdb");	assert(g_sb6API.sqlrdb != NULL);
	g_sb6API.sqlrdc	= (sqlrdc_t)dlsym(g_hSBDLL, "sqlrdc");	assert(g_sb6API.sqlrdc != NULL);
	g_sb6API.sqlrel	= (sqlrel_t)dlsym(g_hSBDLL, "sqlrel");	assert(g_sb6API.sqlrel != NULL);
	g_sb6API.sqlres	= (sqlres_t)dlsym(g_hSBDLL, "sqlres");	assert(g_sb6API.sqlres != NULL);
	g_sb6API.sqlret	= (sqlret_t)dlsym(g_hSBDLL, "sqlret");	assert(g_sb6API.sqlret != NULL);
	g_sb6API.sqlrlf	= (sqlrlf_t)dlsym(g_hSBDLL, "sqlrlf");	assert(g_sb6API.sqlrlf != NULL);
	g_sb6API.sqlrlo	= (sqlrlo_t)dlsym(g_hSBDLL, "sqlrlo");	assert(g_sb6API.sqlrlo != NULL);
	g_sb6API.sqlrof	= (sqlrof_t)dlsym(g_hSBDLL, "sqlrof");	assert(g_sb6API.sqlrof != NULL);
	g_sb6API.sqlrow	= (sqlrow_t)dlsym(g_hSBDLL, "sqlrow");	assert(g_sb6API.sqlrow != NULL);
	g_sb6API.sqlrrd	= (sqlrrd_t)dlsym(g_hSBDLL, "sqlrrd");	assert(g_sb6API.sqlrrd != NULL);
	g_sb6API.sqlrrs	= (sqlrrs_t)dlsym(g_hSBDLL, "sqlrrs");	assert(g_sb6API.sqlrrs != NULL);
	g_sb6API.sqlrsi	= (sqlrsi_t)dlsym(g_hSBDLL, "sqlrsi");	assert(g_sb6API.sqlrsi != NULL);
	g_sb6API.sqlrss	= (sqlrss_t)dlsym(g_hSBDLL, "sqlrss");	assert(g_sb6API.sqlrss != NULL);
	g_sb6API.sqlsab	= (sqlsab_t)dlsym(g_hSBDLL, "sqlsab");	assert(g_sb6API.sqlsab != NULL);
	g_sb6API.sqlsap	= (sqlsap_t)dlsym(g_hSBDLL, "sqlsap");	assert(g_sb6API.sqlsap != NULL);
	g_sb6API.sqlscl	= (sqlscl_t)dlsym(g_hSBDLL, "sqlscl");	assert(g_sb6API.sqlscl != NULL);
	g_sb6API.sqlscn	= (sqlscn_t)dlsym(g_hSBDLL, "sqlscn");	assert(g_sb6API.sqlscn != NULL);
	g_sb6API.sqlscp	= (sqlscp_t)dlsym(g_hSBDLL, "sqlscp");	assert(g_sb6API.sqlscp != NULL);
	g_sb6API.sqlsdn	= (sqlsdn_t)dlsym(g_hSBDLL, "sqlsdn");	assert(g_sb6API.sqlsdn != NULL);
	g_sb6API.sqlsds	= (sqlsds_t)dlsym(g_hSBDLL, "sqlsds");	assert(g_sb6API.sqlsds != NULL);
	g_sb6API.sqlsdx	= (sqlsdx_t)dlsym(g_hSBDLL, "sqlsdx");	assert(g_sb6API.sqlsdx != NULL);
	g_sb6API.sqlset	= (sqlset_t)dlsym(g_hSBDLL, "sqlset");	assert(g_sb6API.sqlset != NULL);
	g_sb6API.sqlsil	= (sqlsil_t)dlsym(g_hSBDLL, "sqlsil");	assert(g_sb6API.sqlsil != NULL);
	g_sb6API.sqlslp	= (sqlslp_t)dlsym(g_hSBDLL, "sqlslp");	assert(g_sb6API.sqlslp != NULL);
	g_sb6API.sqlspr	= (sqlspr_t)dlsym(g_hSBDLL, "sqlspr");	assert(g_sb6API.sqlspr != NULL);
	g_sb6API.sqlsrf	= (sqlsrf_t)dlsym(g_hSBDLL, "sqlsrf");	assert(g_sb6API.sqlsrf != NULL);
	g_sb6API.sqlsrs	= (sqlsrs_t)dlsym(g_hSBDLL, "sqlsrs");	assert(g_sb6API.sqlsrs != NULL);
	g_sb6API.sqlssb	= (sqlssb_t)dlsym(g_hSBDLL, "sqlssb");	assert(g_sb6API.sqlssb != NULL);
	g_sb6API.sqlsss	= (sqlsss_t)dlsym(g_hSBDLL, "sqlsss");	assert(g_sb6API.sqlsss != NULL);
	g_sb6API.sqlsta	= (sqlsta_t)dlsym(g_hSBDLL, "sqlsta");	assert(g_sb6API.sqlsta != NULL);
	g_sb6API.sqlstm	= (sqlstm_t)dlsym(g_hSBDLL, "sqlstm");	assert(g_sb6API.sqlstm != NULL);
	g_sb6API.sqlsto	= (sqlsto_t)dlsym(g_hSBDLL, "sqlsto");	assert(g_sb6API.sqlsto != NULL);
	g_sb6API.sqlstr	= (sqlstr_t)dlsym(g_hSBDLL, "sqlstr");	assert(g_sb6API.sqlstr != NULL);
	g_sb6API.sqlsxt	= (sqlsxt_t)dlsym(g_hSBDLL, "sqlsxt");	assert(g_sb6API.sqlsxt != NULL);
	g_sb6API.sqlsys	= (sqlsys_t)dlsym(g_hSBDLL, "sqlsys");	assert(g_sb6API.sqlsys != NULL);
	g_sb6API.sqltec	= (sqltec_t)dlsym(g_hSBDLL, "sqltec");	assert(g_sb6API.sqltec != NULL);
	g_sb6API.sqltem	= (sqltem_t)dlsym(g_hSBDLL, "sqltem");	assert(g_sb6API.sqltem != NULL);
	g_sb6API.sqltio	= (sqltio_t)dlsym(g_hSBDLL, "sqltio");	assert(g_sb6API.sqltio != NULL);
	g_sb6API.sqlunl	= (sqlunl_t)dlsym(g_hSBDLL, "sqlunl");	assert(g_sb6API.sqlunl != NULL);
	g_sb6API.sqlurs	= (sqlurs_t)dlsym(g_hSBDLL, "sqlurs");	assert(g_sb6API.sqlurs != NULL);
	g_sb6API.sqlwdc	= (sqlwdc_t)dlsym(g_hSBDLL, "sqlwdc");	assert(g_sb6API.sqlwdc != NULL);
	g_sb6API.sqlwlo	= (sqlwlo_t)dlsym(g_hSBDLL, "sqlwlo");	assert(g_sb6API.sqlwlo != NULL);
	g_sb6API.sqlxad	= (sqlxad_t)dlsym(g_hSBDLL, "sqlxad");	assert(g_sb6API.sqlxad != NULL);
	g_sb6API.sqlxcn	= (sqlxcn_t)dlsym(g_hSBDLL, "sqlxcn");	assert(g_sb6API.sqlxcn != NULL);
	g_sb6API.sqlxda	= (sqlxda_t)dlsym(g_hSBDLL, "sqlxda");	assert(g_sb6API.sqlxda != NULL);
	g_sb6API.sqlxdp	= (sqlxdp_t)dlsym(g_hSBDLL, "sqlxdp");	assert(g_sb6API.sqlxdp != NULL);
	g_sb6API.sqlxdv	= (sqlxdv_t)dlsym(g_hSBDLL, "sqlxdv");	assert(g_sb6API.sqlxdv != NULL);
	g_sb6API.sqlxer	= (sqlxer_t)dlsym(g_hSBDLL, "sqlxer");	assert(g_sb6API.sqlxer != NULL);
	g_sb6API.sqlxml	= (sqlxml_t)dlsym(g_hSBDLL, "sqlxml");	assert(g_sb6API.sqlxml != NULL);
	g_sb6API.sqlxnp	= (sqlxnp_t)dlsym(g_hSBDLL, "sqlxnp");	assert(g_sb6API.sqlxnp != NULL);
	g_sb6API.sqlxpd	= (sqlxpd_t)dlsym(g_hSBDLL, "sqlxpd");	assert(g_sb6API.sqlxpd != NULL);
	g_sb6API.sqlxsb	= (sqlxsb_t)dlsym(g_hSBDLL, "sqlxsb");	assert(g_sb6API.sqlxsb != NULL);
}

static void Reset6API()
{
	g_sb6API.sqlarf	= NULL;
	g_sb6API.sqlbbr	= NULL;
	g_sb6API.sqlbdb	= NULL;
	g_sb6API.sqlbef	= NULL;
	g_sb6API.sqlber	= NULL;
	g_sb6API.sqlbkp	= NULL;
	g_sb6API.sqlbld	= NULL;
	g_sb6API.sqlblf	= NULL;
	g_sb6API.sqlblk	= NULL;
	g_sb6API.sqlbln	= NULL;
	g_sb6API.sqlbna	= NULL;
	g_sb6API.sqlbnd	= NULL;
	g_sb6API.sqlbnn	= NULL;
	g_sb6API.sqlbnu	= NULL;
	g_sb6API.sqlbss	= NULL;
	g_sb6API.sqlcan	= NULL;
	g_sb6API.sqlcbv	= NULL;
	g_sb6API.sqlcdr	= NULL;
	g_sb6API.sqlcex	= NULL;
	g_sb6API.sqlclf	= NULL;
	g_sb6API.sqlcmt	= NULL;
	g_sb6API.sqlcnc	= NULL;
	g_sb6API.sqlcnr	= NULL;
	g_sb6API.sqlcom	= NULL;
	g_sb6API.sqlcon	= NULL;
	g_sb6API.sqlcpy	= NULL;
	g_sb6API.sqlcre	= NULL;
	g_sb6API.sqlcrf	= NULL;
	g_sb6API.sqlcrs	= NULL;
	g_sb6API.sqlcsv	= NULL;
	g_sb6API.sqlcty	= NULL;
	g_sb6API.sqldbn	= NULL;
	g_sb6API.sqlded	= NULL;
	g_sb6API.sqldel	= NULL;
	g_sb6API.sqldes	= NULL;
	g_sb6API.sqldid	= NULL;
	g_sb6API.sqldii	= NULL;
	g_sb6API.sqldin	= NULL;
	g_sb6API.sqldir	= NULL;
	g_sb6API.sqldis	= NULL;
	g_sb6API.sqldon	= NULL;
	g_sb6API.sqldox	= NULL;
	g_sb6API.sqldrc	= NULL;
	g_sb6API.sqldro	= NULL;
	g_sb6API.sqldrr	= NULL;
	g_sb6API.sqldrs	= NULL;
	g_sb6API.sqldsc	= NULL;
	g_sb6API.sqldst	= NULL;
	g_sb6API.sqldsv	= NULL;
	g_sb6API.sqlebk	= NULL;
	g_sb6API.sqlefb	= NULL;
	g_sb6API.sqlelo	= NULL;
	g_sb6API.sqlenr	= NULL;
	g_sb6API.sqlepo	= NULL;
	g_sb6API.sqlerf	= NULL;
	g_sb6API.sqlerr	= NULL;
	g_sb6API.sqlers	= NULL;
	g_sb6API.sqletx	= NULL;
	g_sb6API.sqlexe	= NULL;
	g_sb6API.sqlexp	= NULL;
	g_sb6API.sqlfbk	= NULL;
	g_sb6API.sqlfer	= NULL;
	g_sb6API.sqlfet	= NULL;
	g_sb6API.sqlfgt	= NULL;
	g_sb6API.sqlfpt	= NULL;
	g_sb6API.sqlfqn	= NULL;
	g_sb6API.sqlgbi	= NULL;
	g_sb6API.sqlgdi	= NULL;
	g_sb6API.sqlget	= NULL;
	g_sb6API.sqlgfi	= NULL;
	g_sb6API.sqlgls	= NULL;
	g_sb6API.sqlgnl	= NULL;
	g_sb6API.sqlgnr	= NULL;
	g_sb6API.sqlgsi	= NULL;
	g_sb6API.sqlidb	= NULL;
	g_sb6API.sqlims	= NULL;
	g_sb6API.sqlind	= NULL;
	g_sb6API.sqlini	= NULL;
	g_sb6API.sqlins	= NULL;
	g_sb6API.sqllab	= NULL;
	g_sb6API.sqlldp	= NULL;
	g_sb6API.sqllsk	= NULL;
	g_sb6API.sqlmcl	= NULL;
	g_sb6API.sqlmdl	= NULL;
	g_sb6API.sqlmop	= NULL;
	g_sb6API.sqlmrd	= NULL;
	g_sb6API.sqlmsk	= NULL;
	g_sb6API.sqlmwr	= NULL;
	g_sb6API.sqlnbv	= NULL;
	g_sb6API.sqlnii	= NULL;
	g_sb6API.sqlnrr	= NULL;
	g_sb6API.sqlnsi	= NULL;
	g_sb6API.sqloms	= NULL;
	g_sb6API.sqlprs	= NULL;
	g_sb6API.sqlrbf	= NULL;
	g_sb6API.sqlrbk	= NULL;
	g_sb6API.sqlrcd	= NULL;
	g_sb6API.sqlrdb	= NULL;
	g_sb6API.sqlrdc	= NULL;
	g_sb6API.sqlrel	= NULL;
	g_sb6API.sqlres	= NULL;
	g_sb6API.sqlret	= NULL;
	g_sb6API.sqlrlf	= NULL;
	g_sb6API.sqlrlo	= NULL;
	g_sb6API.sqlrof	= NULL;
	g_sb6API.sqlrow	= NULL;
	g_sb6API.sqlrrd	= NULL;
	g_sb6API.sqlrrs	= NULL;
	g_sb6API.sqlrsi	= NULL;
	g_sb6API.sqlrss	= NULL;
	g_sb6API.sqlsab	= NULL;
	g_sb6API.sqlsap	= NULL;
	g_sb6API.sqlscl	= NULL;
	g_sb6API.sqlscn	= NULL;
	g_sb6API.sqlscp	= NULL;
	g_sb6API.sqlsdn	= NULL;
	g_sb6API.sqlsds	= NULL;
	g_sb6API.sqlsdx	= NULL;
	g_sb6API.sqlset	= NULL;
	g_sb6API.sqlsil	= NULL;
	g_sb6API.sqlslp	= NULL;
	g_sb6API.sqlspr	= NULL;
	g_sb6API.sqlsrf	= NULL;
	g_sb6API.sqlsrs	= NULL;
	g_sb6API.sqlssb	= NULL;
	g_sb6API.sqlsss	= NULL;
	g_sb6API.sqlsta	= NULL;
	g_sb6API.sqlstm	= NULL;
	g_sb6API.sqlsto	= NULL;
	g_sb6API.sqlstr	= NULL;
	g_sb6API.sqlsxt	= NULL;
	g_sb6API.sqlsys	= NULL;
	g_sb6API.sqltec	= NULL;
	g_sb6API.sqltem	= NULL;
	g_sb6API.sqltio	= NULL;
	g_sb6API.sqlunl	= NULL;
	g_sb6API.sqlurs	= NULL;
	g_sb6API.sqlwdc	= NULL;
	g_sb6API.sqlwlo	= NULL;
	g_sb6API.sqlxad	= NULL;
	g_sb6API.sqlxcn	= NULL;
	g_sb6API.sqlxda	= NULL;
	g_sb6API.sqlxdp	= NULL;
	g_sb6API.sqlxdv	= NULL;
	g_sb6API.sqlxer	= NULL;
	g_sb6API.sqlxml	= NULL;
	g_sb6API.sqlxnp	= NULL;
	g_sb6API.sqlxpd	= NULL;
	g_sb6API.sqlxsb	= NULL;
}

static void Load7API()
{
	Load6API();

	// load version 7 specific
	g_sb7API.sqlcch	= (sqlcch_t)dlsym(g_hSBDLL, "sqlcch");	assert(g_sb7API.sqlcch != NULL);
	g_sb7API.sqldch	= (sqldch_t)dlsym(g_hSBDLL, "sqldch");	assert(g_sb7API.sqldch != NULL);
	g_sb7API.sqlopc	= (sqlopc_t)dlsym(g_hSBDLL, "sqlopc");	assert(g_sb7API.sqlopc != NULL);
}

static void Reset7API()
{
	// free version 7 specific
	g_sb7API.sqlcch	= NULL;
	g_sb7API.sqldch	= NULL;
	g_sb7API.sqlopc	= NULL;

	Reset6API();
}

void AddSB6Support(const SAConnection * pCon)
{
	if( ! g_hSBDLL )
	{
		// load SQLBase API library
		SAString sErrorMessage, sLibName, sLibsList = pCon->Option(_TSA("SQLBASE.LIBS"));
		if( sLibsList.IsEmpty() )
			sLibsList = g_sSBDLLNames;

		g_hSBDLL = SALoadLibraryFromList(sLibsList, sErrorMessage, sLibName, RTLD_LAZY);
		if( ! g_hSBDLL )
			throw SAException(SA_Library_Error, SA_Library_Error_LoadLibraryFails, -1, IDS_LOAD_LIBRARY_FAILS, (const SAChar*)sErrorMessage);

		Load6API();

		g_sb6API.sqlini((SQLTPFP) (0));
	}

	if( SAGlobals::UnloadAPI() )
		g_nSBDLLRefs++;
	else
		g_nSBDLLRefs = 1;
}

void ReleaseSB6Support()
{
	assert(g_nSBDLLRefs > 0);
	g_nSBDLLRefs--;
	if(!g_nSBDLLRefs)
	{
		g_sb6API.sqldon();

		g_nSBDLLVersionLoaded = 0;
		Reset6API();

		dlclose(g_hSBDLL);
		g_hSBDLL = NULL;
	}
}

void AddSB7Support(const SAConnection * pCon)
{
	SACriticalSectionScope scope(&sbLoaderMutex);

	if(!g_hSBDLL)
	{
		// load SQLBase API library
		SAString sErrorMessage, sLibName, sLibsList = pCon->Option(_TSA("SQLBASE.LIBS"));
		if( sLibsList.IsEmpty() )
			sLibsList = g_sSBDLLNames;
		
		g_hSBDLL = SALoadLibraryFromList(sLibsList, sErrorMessage, sLibName, RTLD_LAZY);
		if( ! g_hSBDLL )
			throw SAException(SA_Library_Error, SA_Library_Error_LoadLibraryFails, -1, IDS_LOAD_LIBRARY_FAILS, (const SAChar*)sErrorMessage);

		Load7API();
		g_sb7API.sqlini((SQLTPFP) (0));
	}

	if( SAGlobals::UnloadAPI() )
		g_nSBDLLRefs++;
	else
		g_nSBDLLRefs = 1;
}

void ReleaseSB7Support()
{
	SACriticalSectionScope scope(&sbLoaderMutex);

	assert(g_nSBDLLRefs > 0);
	--g_nSBDLLRefs;
	if(!g_nSBDLLRefs)
	{
		g_sb7API.sqldon();

		g_nSBDLLVersionLoaded = 0;
		Reset7API();

		dlclose(g_hSBDLL);
		g_hSBDLL = NULL;
	}
}
