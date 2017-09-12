// ssClient.cpp: implementation of the IssClient class.
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include "ssClient.h"

#if !defined(SA_NO_DBLIB)
ISAConnection *newIssDbLibConnection(SAConnection *pSAConnection);
#endif
#if !defined(SA_NO_OLEDB)
ISAConnection *newIssOleDbConnection(SAConnection *pSAConnection);
#endif
#if !defined(SA_NO_SQLNCLI)
ISAConnection *newIssNCliConnection(SAConnection *pSAConnection);
#endif

//////////////////////////////////////////////////////////////////////
// IssClient Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IssClient::IssClient()
{
}

IssClient::~IssClient()
{
}

ISAConnection *IssClient::QueryConnectionInterface(
	SAConnection *pSAConnection)
{
	SAString s = pSAConnection->Option(_TSA("UseAPI"));

#if !defined(SA_NO_DBLIB)
	if( 0 == s.CompareNoCase(_TSA("DB-Library")) ||
		0 == s.CompareNoCase(_TSA("DB-Lib")) )
		return newIssDbLibConnection(pSAConnection);
#endif

#if !defined(SA_NO_OLEDB)
	if( 0 == s.CompareNoCase(_TSA("OLEDB")) )
		return newIssOleDbConnection(pSAConnection);
#endif

#if !defined(SA_NO_SQLNCLI)
	if( 0 == s.CompareNoCase(_TSA("ODBC")) )
		return newIssNCliConnection(pSAConnection);
#endif

#if !defined(SA_NO_SQLNCLI)
	return newIssNCliConnection(pSAConnection);
#else
#if !defined(SA_NO_OLEDB)
	return newIssOleDbConnection(pSAConnection);
#else
#if !defined(SA_NO_DBLIB)
	return newIssDbLibConnection(pSAConnection);
#else
	return NULL;
#endif
#endif
#endif
}

