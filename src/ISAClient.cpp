// ISAClient.cpp: implementation of the ISAClient class.
//
//////////////////////////////////////////////////////////////////////

#include "osdep.h"
#include "ISAClient.h"

#define SA_Align16(x)	(((x+15) >> 4) << 4)

//////////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////////

// numeric support/conversions helpers

void LittleEndian10000BaseDivide(
	unsigned int LittleEndianSize, // in shorts
	const unsigned short Devidend[], unsigned short Devisor,
	unsigned short Quotient[], unsigned short *pRemainder)
{
	unsigned long ln = 0l;
	for (unsigned int i = 0; i < LittleEndianSize; ++i)
	{
		ln *= 10000;
		ln += Devidend[LittleEndianSize - i - 1];

		assert((unsigned long)(ln / Devisor) == (unsigned long)(unsigned short)(ln / Devisor)); // no truncation here
		Quotient[LittleEndianSize - i - 1] = (unsigned short)(ln / Devisor);
		ln %= Devisor;
	}

	if (pRemainder)
		*pRemainder = (unsigned short)ln;
}

bool AllBytesAreZero(const void *pBytes, size_t nSize)
{
	unsigned char *p = (unsigned char *)pBytes;
	for (size_t i = 0l; i < nSize; ++i)
	{
		if (*p != 0)
			return false;
		++p;
	}

	return true;
}

// Character sets conversion helpers

// returns number of chars written
size_t SAMultiByteToWideChar(wchar_t *pchDataTgt, const char *lpch,
	size_t nLength, char **lppchStop)
{
	wchar_t *pchData = pchDataTgt;
	const char *mbchar = lpch;
	size_t count = nLength;
	while (count > 0)
	{
		int nLen = mbtowc(pchData, mbchar, count);
		if (nLen == -1) // object that mbchar points to does not form a valid multibyte character within the first count characters
			break; // stop conversion
		if (nLen == 0) // mbchar is NULL or the object that it points to is a null character
		{
			// 1 = length of multi-byte '\0'
			++mbchar;
			--count;
		}
		else
		{
			// nLen = length of last multi-byte character
			mbchar += nLen;
			count -= nLen;
		}
		++pchData;
	}

	if (lppchStop)
		*lppchStop = (char*)mbchar;
	return (size_t)(pchData - pchDataTgt);
}

// returns number of chars written
// if pchDataTgt is NULL, function simply counts
size_t SAWideCharToMultiByte(char *pchDataTgt, const wchar_t *lpch,
	size_t nLength)
{
	size_t nWritten = 0;
	for (size_t i = 0l; i < nLength; ++i)
	{
		int nLen;
		if (pchDataTgt)
			nLen = wctomb(pchDataTgt + nWritten, lpch[i]);
		else
		{
			char sTemp[128];
			nLen = wctomb(sTemp, lpch[i]);
		}
		if (nLen == -1) // the conversion is not possible in the current locale
		{
			if (pchDataTgt)
				pchDataTgt[nWritten] = '?';
			++nWritten;
		}
		else
			nWritten += nLen;
	}

	return nWritten;
}

#ifdef SA_UNICODE
// returns number of chars written
// if pchDataTgt is NULL, function simply counts
size_t SAWideCharToUTF8(char *pchDataTgt, const wchar_t *lpch, size_t nLength)
{
#ifdef SQLAPI_WINDOWS
	int nWritten = WideCharToMultiByte(CP_UTF8, 0, lpch, (int)nLength, pchDataTgt, 0, NULL, NULL);
	if( pchDataTgt )
		nWritten = WideCharToMultiByte(CP_UTF8, 0, lpch, (int)nLength, pchDataTgt, nWritten, NULL, NULL);
#else
	int nWritten = wchar_to_utf8(lpch, nLength, pchDataTgt, 0,
		UTF8_IGNORE_ERROR);
	if( pchDataTgt )
		nWritten = wchar_to_utf8(lpch, nLength, pchDataTgt, nWritten,
		UTF8_IGNORE_ERROR);
#endif
	return nWritten;
}

// returns number of chars written
size_t SAUTF8ToWideChar(wchar_t *pchDataTgt, const char *lpch, size_t nLength,
	char **lppchStop)
{
#ifdef SQLAPI_WINDOWS
	int nWritten = 0;
	int nLen = (int)nLength; // It's possible we receive part UTF character at the end.
	// So we try to decrease the buffer for correct size.
	while( 0 == nWritten && nLen > 0 )
	{
		nWritten = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, lpch, nLen, pchDataTgt, 0);
		if( 0 == nWritten )
			nLen--;
	}

	if( pchDataTgt )
	{
		nWritten = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, lpch, nLen, pchDataTgt, nWritten);
		if( lppchStop )
			*lppchStop = (char*)lpch + nLen;
	}
#else
	size_t nWritten = 0l;
	size_t nLen = nLength;
	while( 0 == nWritten && nLen > 0 )
	{
		nWritten = utf8_to_wchar(lpch, nLen, NULL, 0, 0);
		if( 0 == nWritten )
			nLen--;
	}

	if( pchDataTgt )
	{
		nWritten = utf8_to_wchar(lpch, nLen, pchDataTgt, nWritten, 0);
		if( lppchStop )
			*lppchStop = (char*) lpch + nLen;
	}
#endif
	return nWritten;
}
#endif // SA_UNICODE
//////////////////////////////////////////////////////////////////////
// Common interfaces
//////////////////////////////////////////////////////////////////////
saAPI::saAPI()
{
}

/*virtual */
saAPI::~saAPI()
{
}

saConnectionHandles::saConnectionHandles()
{
}

/*virtual */
saConnectionHandles::~saConnectionHandles()
{
}

saCommandHandles::saCommandHandles()
{
}

/*virtual */
saCommandHandles::~saCommandHandles()
{
}

//////////////////////////////////////////////////////////////////////
// ISAClient Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ISAClient::ISAClient()
{
}

ISAClient::~ISAClient()
{
}

//////////////////////////////////////////////////////////////////////
// ISAConnection Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ISAConnection::ISAConnection(SAConnection *pSAConnection) :
m_pSAConnection(pSAConnection)
{
}

ISAConnection::~ISAConnection()
{
}

SAConnection *ISAConnection::getSAConnection()
{
	return m_pSAConnection;
}

void ISAConnection::EnumCursors(EnumCursors_t fn, void *pAddlData)
{
	if (!m_pSAConnection)
		return;

	m_pSAConnection->EnumCursors(fn, pAddlData);
}

bool ISAConnection::getOptionValue(const SAString& sOption, bool bDefault) const
{
    SAString sValue = m_pSAConnection->Option(sOption);
    if (sValue.IsEmpty())
        return bDefault;
    return (0 == sValue.CompareNoCase(_TSA("TRUE"))
        || 0 == sValue.CompareNoCase(_TSA("1")) ? true : false);
}

int ISAConnection::getOptionValue(const SAString& sOption, int nDefault) const
{
    SAString sValue = m_pSAConnection->Option(sOption);
    if (sValue.IsEmpty())
        return nDefault;
    return sa_toi(sValue);
}

//////////////////////////////////////////////////////////////////////
// ISACursor Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ISACursor::ISACursor(ISAConnection *pISAConnection, SACommand *pCommand) :
m_pISAConnection(pISAConnection), m_pCommand(pCommand),
m_fnPreHandleException(NULL), m_eLastFetchType(EFTYP_NONE)
{
	m_pParamBuffer = NULL;
	m_nParamIndSize = 0l;
	m_nParamSizeSize = 0l;

	m_pSelectBuffer = NULL;
	m_nIndSize = 0l;
	m_nSizeSize = 0l;
	m_nBulkReadingBufSize = 0;
}

/*virtual */
ISACursor::~ISACursor()
{
	if (m_pParamBuffer)
		::free(m_pParamBuffer);
	if (m_pSelectBuffer)
		::free(m_pSelectBuffer);
}

/*virtual */
size_t ISACursor::InputBufferSize(const SAParam &Param) const
{
	assert(Param.ParamDirType() == SA_ParamInput
		|| Param.ParamDirType() == SA_ParamInputOutput);

	if (Param.isNull())
		return 0;

	switch (Param.DataType())
	{
	case SA_dtUnknown:
		throw SAException(SA_Library_Error, SA_Library_Error_UnknownParameterType, -1, IDS_UNKNOWN_PARAMETER_TYPE,
			(const SAChar*)Param.Name());
	case SA_dtBool:
		assert(false); // sould be overloaded
		break;
	case SA_dtShort:
		return sizeof(short);
	case SA_dtUShort:
		return sizeof(unsigned short);
	case SA_dtLong:
		return sizeof(long);
	case SA_dtULong:
		return sizeof(unsigned long);
	case SA_dtDouble:
		return sizeof(double);
	case SA_dtNumeric:
		assert(false); // sould be overloaded
		break;
	case SA_dtDateTime:
		assert(false); // sould be overloaded
		return 0;
	case SA_dtString:
		// defualt implementation:
		// 1) no space for '\0' is reserved
		// 2) always returns size in single characters (converts to multi-byte under Unicode)
		// So, under Unicode this should be overloaded
		// by clients that bind Unicode strings directly
		// without converting them to multi-byte before binding
		return Param.asString().GetMultiByteCharsLength();
	case SA_dtBytes:
		// in bytes, not characters
		return Param.asBytes().GetBinaryLength();
	case SA_dtInterval:
	case SA_dtLongBinary:
	case SA_dtLongChar:
	case SA_dtBLob:
	case SA_dtCLob:
		assert(false); // sould be overloaded
		return 0;
	case SA_dtCursor:
		assert(false); // sould be overloaded
		return 0;
	default:
		assert(false); // unknown type
	}

	return 0;
}

// align all members to 16-byte boundary
// it will speed up data access on Intel
// and it is very important on RISK 
// platforms (like SPARC Solaris) to avoid non-aligment
void ISACursor::AllocBindBuffer(size_t nIndSize, size_t nSizeSize)
{
	m_nParamIndSize = nIndSize;
	m_nParamSizeSize = nSizeSize;

	size_t nAllocatedSize = 0l;

	int cParamCount = m_pCommand->ParamCount();

	size_t* nCurParamSize = (size_t *)sa_calloc(cParamCount, sizeof(size_t));
	size_t* nDataSize = (size_t *)sa_calloc(cParamCount, sizeof(size_t));

	int iParam;
	for (iParam = 0; iParam < cParamCount; ++iParam)
	{
		SAParam &Param = m_pCommand->ParamByIndex(iParam);

		/*unsigned int */
		nCurParamSize[iParam] = 0l;
		nCurParamSize[iParam] += SA_Align16(sizeof(size_t)); // space allocated for data
		nCurParamSize[iParam] += SA_Align16(nIndSize); // null indicator
		nCurParamSize[iParam] += SA_Align16(nSizeSize); // space for returned size

		/*unsigned int */
		nDataSize[iParam] = 0l;
		switch (Param.ParamDirType())
		{
		case SA_ParamInput:
			nDataSize[iParam] = InputBufferSize(Param);
			break;
		case SA_ParamInputOutput:
			nDataSize[iParam]
				= sa_max(InputBufferSize(Param), OutputBufferSize(Param.ParamType(), Param.ParamSize()));
			break;
		case SA_ParamOutput:
		case SA_ParamReturn:
			nDataSize[iParam] = OutputBufferSize(Param.ParamType(),
				Param.ParamSize());
			break;
		default:
			nDataSize[iParam] = 0l;
			assert(false);
		}
		nCurParamSize[iParam] += SA_Align16(nDataSize[iParam]);
		nAllocatedSize += nCurParamSize[iParam];
	}

	sa_realloc(&m_pParamBuffer, nAllocatedSize);

	nAllocatedSize = 0l;

	for (iParam = 0; iParam < cParamCount; ++iParam)
	{
		*(size_t*)((char*)m_pParamBuffer + nAllocatedSize)
			= nDataSize[iParam];
		nAllocatedSize += nCurParamSize[iParam];
	}

	free(nCurParamSize);
	free(nDataSize);
}

// align all members to 16-byte boundary
// it will speed up data access on Intel
// and it is very important on RISK 
// platforms (like Sun SPARC) to avoid non-aligment
void ISACursor::AllocBindBuffer(int nPlaceHolderCount,
	saPlaceHolder **ppPlaceHolders, size_t nIndSize, size_t nSizeSize,
	size_t nAddlSize, void **ppAddl)
{
	m_nParamIndSize = nIndSize;
	m_nParamSizeSize = nSizeSize;

	size_t nAllocatedSize = 0l;

	size_t* nCurParamSize =
		(size_t *)sa_calloc(nPlaceHolderCount, sizeof(size_t));
	size_t* nDataSize = (size_t *)sa_calloc(nPlaceHolderCount, sizeof(size_t));

	int i;
	for (i = 0; i < nPlaceHolderCount; ++i)
	{
		SAParam &Param = *ppPlaceHolders[i]->getParam();

		nCurParamSize[i] = 0l;
		nCurParamSize[i] += SA_Align16(sizeof(size_t)); // space allocated for data
		nCurParamSize[i] += SA_Align16(nIndSize); // null indicator
		nCurParamSize[i] += SA_Align16(nSizeSize); // space for returned size

		nDataSize[i] = 0l;
		switch (Param.ParamDirType())
		{
		case SA_ParamInput:
			nDataSize[i] = InputBufferSize(Param);
			break;
		case SA_ParamInputOutput:
			nDataSize[i] = sa_max(InputBufferSize(Param),
				OutputBufferSize(Param.ParamType(), Param.ParamSize()));
			break;
		case SA_ParamOutput:
		case SA_ParamReturn:
			nDataSize[i] = OutputBufferSize(Param.ParamType(),
				Param.ParamSize());
			break;
		default:
			nDataSize[i] = 0l;
			assert(false);
		}
		nCurParamSize[i] += SA_Align16(nDataSize[i]);
		nAllocatedSize += nCurParamSize[i];
	}

	sa_realloc(&m_pParamBuffer, nAllocatedSize);

	nAllocatedSize = 0l;

	for (i = 0; i < nPlaceHolderCount; ++i)
	{
		*(size_t*)((char*)m_pParamBuffer + nAllocatedSize) = nDataSize[i];
		nAllocatedSize += nCurParamSize[i];
	}

	free(nCurParamSize);
	free(nDataSize);

	// allocate additional block as requested by caller
	if (nAddlSize)
		sa_realloc(&m_pParamBuffer, nAllocatedSize + SA_Align16(nAddlSize));
	if (ppAddl)
		*ppAddl = (char*)m_pParamBuffer + nAllocatedSize;
}

/*virtual */
size_t ISACursor::OutputBufferSize(SADataType_t eDataType, size_t nDataSize) const
{
	switch (eDataType)
	{
	case SA_dtUnknown:
		throw SAException(SA_Library_Error, SA_Library_Error_UnknownDataType, -1, IDS_UNKNOWN_DATA_TYPE);
	case SA_dtBool:
		assert(false); // sould be overloaded
		return 0;
	case SA_dtShort:
		return sizeof(short);
	case SA_dtUShort:
		return sizeof(unsigned short);
	case SA_dtLong:
		if (sizeof(int) == nDataSize)
			return nDataSize;
		else
			return sizeof(long);
	case SA_dtULong:
		if (sizeof(unsigned int) == nDataSize)
			return nDataSize;
		else
			return sizeof(unsigned long);
	case SA_dtDouble:
		return sizeof(double);
	case SA_dtNumeric:
		assert(false); // sould be overloaded
	case SA_dtDateTime:
		assert(false); // sould be overloaded
		return 0;
	case SA_dtBytes:
		return nDataSize;
	case SA_dtString:
		// No space for '\0' is reserved.
		// nDataSize is always (under Unicode or multi-byte builds)
		// assumed to be in bytes, not characters.
		// If above is not true, client should overload.
		return nDataSize;
	case SA_dtLongBinary:
	case SA_dtLongChar:
	case SA_dtBLob:
	case SA_dtCLob:
		assert(false); // sould be overloaded
		return 0;
	case SA_dtCursor:
		assert(false); // sould be overloaded
		return 0;
	case SA_dtInterval:
		return nDataSize;
	default:
		assert(false); // unknown type
	}

	return 0;
}

// align all members to 16-byte boundary
// it will speed up data access on Intel
// and it is very important on RISK 
// platforms (like Sun SPARC) to avoid non-aligment
void ISACursor::AllocSelectBuffer(size_t nIndSize, size_t nSizeSize,
	int nBulkReadingBufSize)
{
	m_nIndSize = nIndSize;
	m_nSizeSize = nSizeSize;
	m_nBulkReadingBufSize = nBulkReadingBufSize;

	size_t nAllocatedSize = 0l;

	int cFieldCount = m_pCommand->FieldCount();
	size_t* nCurFieldSize = (size_t *)sa_calloc(cFieldCount, sizeof(size_t));
	size_t* nDataSize = (size_t *)sa_calloc(cFieldCount, sizeof(size_t));

	int iField;
	for (iField = 1; iField <= cFieldCount; ++iField)
	{
		SAField &Field = m_pCommand->Field(iField);

		nCurFieldSize[iField - 1] = 0l;
		nCurFieldSize[iField - 1] += SA_Align16(sizeof(size_t)); // space allocated for data
		nCurFieldSize[iField - 1] += SA_Align16(nIndSize * nBulkReadingBufSize); // null indicator
		nCurFieldSize[iField - 1]
			+= SA_Align16(nSizeSize * nBulkReadingBufSize); // space for returned size

		nDataSize[iField - 1] = OutputBufferSize(Field.FieldType(),
			Field.FieldSize());
		nCurFieldSize[iField - 1]
			+= SA_Align16(nDataSize[iField - 1] * nBulkReadingBufSize);

		nAllocatedSize += nCurFieldSize[iField - 1];
	}

	sa_realloc(&m_pSelectBuffer, nAllocatedSize);

	nAllocatedSize = 0l;
	for (iField = 1; iField <= cFieldCount; ++iField)
	{
		*(size_t*)((char*)m_pSelectBuffer + nAllocatedSize)
			= nDataSize[iField - 1];
		nAllocatedSize += nCurFieldSize[iField - 1];
	}

	free(nCurFieldSize);
	free(nDataSize);

	void *pBuf = m_pSelectBuffer;
	for (iField = 1; iField <= cFieldCount; ++iField)
	{
		void *pInd;
		void *pSize;
		void *pValue;
		size_t nDataBufSize;

		IncFieldBuffer(pBuf, pInd, pSize, nDataBufSize, pValue);

		SetFieldBuffer(iField, pInd, nIndSize, pSize, nSizeSize, pValue,
			nDataBufSize);
	}
}

// default null indicator test: -1 is null value
// ISACursor implementation always returns true
/*virtual */
bool ISACursor::ConvertIndicator(
	int/* nPos*/, // 1-based
	int/* nNotConverted*/, SAValueRead &vr, ValueType_t/* eValueType*/,
	void *pInd, size_t nIndSize, void *pSize, size_t nSizeSize,
	size_t &nRealSize, int nBulkReadingBufPos) const
{
	// try all integer types
	if (nIndSize == sizeof(signed char))
		*vr.m_pbNull = ((signed char*)pInd)[nBulkReadingBufPos] == -1;
	else if (nIndSize == sizeof(short))
		*vr.m_pbNull = ((short*)pInd)[nBulkReadingBufPos] == -1;
	else if (nIndSize == sizeof(int))
		*vr.m_pbNull = ((int*)pInd)[nBulkReadingBufPos] == -1;
	else if (nIndSize == sizeof(long))
		*vr.m_pbNull = ((long*)pInd)[nBulkReadingBufPos] == -1;
	else
	{
		assert(false); // if none of above then should be overloaded
		*vr.m_pbNull = true;
	}

	if (*vr.m_pbNull)
		return true; // converted, no need to convert size

	// try all integer types
	if (nSizeSize == sizeof(unsigned char))
		nRealSize = ((unsigned char*)pSize)[nBulkReadingBufPos];
	else if (nSizeSize == sizeof(unsigned short))
		nRealSize = ((unsigned short*)pSize)[nBulkReadingBufPos];
	else if (nSizeSize == sizeof(unsigned int))
		nRealSize = ((unsigned int*)pSize)[nBulkReadingBufPos];
	else if (nSizeSize == sizeof(unsigned long))
		nRealSize = ((unsigned long*)pSize)[nBulkReadingBufPos];
	else
	{
		assert(false); // if none of above then should be overloaded
		nRealSize = 0l;
	}

	/*
	try to fix the next (oracle 8.1.7.0):

	Bug 1315603      Fixed: 8171

	* OCI
	* Wrong Results

	When using OCI_ATTR_PREFETCH_ROWS in an OCI client the client
	may see wrong Indicator/return code values when fetching NULL data.
	ie: indp / rcodep may be incorrect.
	Workaround: Set OCI_ATTR_PREFETCH_ROWS to 0

	if( 0 == nRealSize )
	*vr.m_pbNull = true;
	*/

	return true; // nullness and size converted
}

void ISACursor::GetFieldBuffer(int nPos, // 1-based
	void *&pValue, size_t &nFieldBufSize)
{
	if (!m_pSelectBuffer)
	{
		pValue = NULL;
		nFieldBufSize = 0l;
		return;
	}

	void *pBuf = m_pSelectBuffer;
	for (int i = 0; i < nPos; ++i)
	{
		void *pInd;
		void *pSize;
		IncFieldBuffer(pBuf, pInd, pSize, nFieldBufSize, pValue);
	}
}

void ISACursor::IncParamBuffer(void *&pBuf, void *&pInd, void *&pSize,
	size_t &nDataBufSize, void *&pValue)
{
	nDataBufSize = *(size_t*)pBuf;
	((char*&)pBuf) += SA_Align16(sizeof(size_t));

	pInd = pBuf;
	((char*&)pBuf) += SA_Align16(m_nParamIndSize);

	pSize = pBuf;
	((char*&)pBuf) += SA_Align16(m_nParamSizeSize);

	pValue = pBuf;
	((char*&)pBuf) += SA_Align16(nDataBufSize);
}

void ISACursor::IncFieldBuffer(void *&pBuf, void *&pInd, void *&pSize,
	size_t &nDataBufSize, void *&pValue)
{
	nDataBufSize = *(size_t*)pBuf;
	((char*&)pBuf) += SA_Align16(sizeof(size_t));

	pInd = pBuf;
	((char*&)pBuf) += SA_Align16(m_nIndSize * m_nBulkReadingBufSize);

	pSize = pBuf;
	((char*&)pBuf) += SA_Align16(m_nSizeSize * m_nBulkReadingBufSize);

	pValue = pBuf;
	((char*&)pBuf) += SA_Align16(nDataBufSize * m_nBulkReadingBufSize);
}

/*virtual */
bool ISACursor::OutputBindingTypeIsParamType()
{
	return false;
}

/*virtual */
bool ISACursor::IgnoreUnknownDataType()
{
	return false;
}

/*virtual */
bool ISACursor::ConvertValue(
	int nPos, // 1 - based
	int nNotConverted, size_t nIndSize, void *pNull, size_t nSizeSize,
	void *pSize, size_t nBufSize, void *pValue, ValueType_t eValueType,
	SAValueRead &vr, int nBulkReadingBufPos)
{
	size_t nRealSize;
	if (!ConvertIndicator(nPos, nNotConverted, vr, eValueType, pNull, nIndSize,
		pSize, nSizeSize, nRealSize, nBulkReadingBufPos))
	{
		return false;
	}

	if (vr.isNull())
		return true; // converted

	const void *pData = (char*)pValue + nBulkReadingBufPos * nBufSize;

	SADataType_t eDataType;
	switch (eValueType)
	{
	case ISA_FieldValue:
		eDataType = ((SAField&)vr).FieldType();
		break;
	default:
		assert(eValueType == ISA_ParamValue);
		if (isOutputParam((SAParam&)vr) && OutputBindingTypeIsParamType()) // virtual call
		{
			eDataType = ((SAParam&)vr).ParamType();
		}
		else
		{
			// in most cases vr.DataType() == ((SAParam&)vr).ParamType()
			// but this can be coerced
			eDataType = vr.DataType();
		}
	}

	switch (eDataType)
	{
	case SA_dtUnknown:
		throw SAException(SA_Library_Error, SA_Library_Error_UnknownDataType, -1, IDS_UNKNOWN_DATA_TYPE);
	case SA_dtBool:
		vr.m_eDataType = SA_dtBool;
		if (nRealSize == sizeof(char))
			*(bool*)vr.m_pScalar = *(char*)pData != 0;
		else if (nRealSize == sizeof(short))
			*(bool*)vr.m_pScalar = *(short*)pData != 0;
		else if (nRealSize == sizeof(int))
			*(bool*)vr.m_pScalar = *(int*)pData != 0;
		else if (nRealSize == sizeof(long))
			*(bool*)vr.m_pScalar = *(long*)pData != 0;
		else
			assert(false);
		break;
	case SA_dtShort:
		assert(nRealSize <= sizeof(short)); // <= because we do not have SA_dtChar
		vr.m_eDataType = SA_dtShort;
		*(short*)vr.m_pScalar = *(short*)pData;
		break;
	case SA_dtUShort:
		assert(nRealSize <= sizeof(unsigned short)); // <= because we do not have SA_dtChar
		vr.m_eDataType = SA_dtUShort;
		*(short*)vr.m_pScalar = *(short*)pData;
		break;
	case SA_dtLong:
		vr.m_eDataType = SA_dtLong;
		if (nRealSize == sizeof(long))
			*(long*)vr.m_pScalar = *(long*)pData;
		else if (nRealSize == sizeof(int))
			*(long*)vr.m_pScalar = (long)*(int*)pData;
		else if (nRealSize == sizeof(unsigned char))
			*(long*)vr.m_pScalar = (long)*(unsigned char*)pData;
		else
			assert(false);
		break;
	case SA_dtULong:
		vr.m_eDataType = SA_dtULong;
		if (nRealSize == sizeof(unsigned int))
			*(unsigned long*)vr.m_pScalar = (unsigned long)*(unsigned int*)pData;
		else if (nRealSize == sizeof(unsigned long))
			*(unsigned long*)vr.m_pScalar = *(unsigned long*)pData;
		else if (nRealSize == sizeof(sa_uint64_t))
			*(unsigned long*)vr.m_pScalar = (unsigned long)*(sa_uint64_t*)pData;
		else
			assert(false);
		break;
	case SA_dtDouble:
		// it's possible we map 4-byte real type (SQLServer CE) into double type too
		// then nRealSize == sizeof(float)
		assert(nRealSize == sizeof(double) || nRealSize == sizeof(float));
		vr.m_eDataType = SA_dtDouble;
		if (nRealSize == sizeof(double))
			*(double*)vr.m_pScalar = *(double*)pData;
		else if (nRealSize == sizeof(float))
			*(double*)vr.m_pScalar = *(float*)pData;
		break;
	case SA_dtNumeric:
		// size can be variable, e.g. in SQLBase, Oracle, Sybase
		assert(nRealSize <= nBufSize);
		vr.m_eDataType = SA_dtNumeric;
		m_pISAConnection->CnvtInternalToNumeric(
			*(SANumeric*)vr.m_pNumeric,
			pData, (int)nRealSize);
		break;
	case SA_dtDateTime:
		assert(nRealSize <= nBufSize); // size can be variable, e.g. in SQLBase
		vr.m_eDataType = SA_dtDateTime;
		m_pISAConnection->CnvtInternalToDateTime(
			*vr.m_pDateTime,
			pData, (int)nRealSize);
		break;
	case SA_dtInterval:
		assert(nRealSize <= nBufSize); // size can be variable, e.g. in MySQL
		vr.m_eDataType = SA_dtInterval;
		m_pISAConnection->CnvtInternalToInterval(
			*vr.m_pInterval,
			pData, (int)nRealSize);
		break;
	case SA_dtString:
		// assert(nRealSize <= nBufSize);
		// Sometimes, at least Sybase ASE reports rela size > buf size
		vr.m_eDataType = SA_dtString;
		ConvertString(*vr.m_pString, pData, sa_min(nRealSize, nBufSize));
		break;
	case SA_dtBytes:
		assert(nRealSize <= nBufSize);
		vr.m_eDataType = SA_dtBytes;
		*vr.m_pString =
			SAString((const void*)pData, nRealSize);
		break;
	case SA_dtLongBinary:
		vr.m_eDataType = SA_dtLongBinary;
		break;
	case SA_dtLongChar:
		vr.m_eDataType = SA_dtLongChar;
		break;
	case SA_dtBLob:
		vr.m_eDataType = SA_dtBLob;
		break;
	case SA_dtCLob:
		vr.m_eDataType = SA_dtCLob;
		break;
	case SA_dtCursor:
		vr.m_eDataType = SA_dtCursor;
		m_pISAConnection->CnvtInternalToCursor(
			vr.m_pCursor, pData);
		break;
	default:
		assert(false); // unknown type
	}

	return true;
}

// default implementation assumes that data for
// string is in multibyte format always
// (in both Unicode and multi-byte builds)
// it also assumes that nRealSize is always in bytes not characters
// Should be overloaded by clients if this is not true
/*virtual */
void ISACursor::ConvertString(SAString &String, const void *pData,
	size_t nRealSize)
{
	String = SAString((const char*)pData, nRealSize);
}

void ISACursor::GetParamBuffer(const SAParam &SrcParam, void *&pValue,
	size_t &nDataBufSize)
{
	void *pBuf = m_pParamBuffer;
	for (int i = 0; i < m_pCommand->ParamCount(); ++i)
	{
		SAParam &Param = m_pCommand->ParamByIndex(i);

		void *pNull;
		void *pSize;
		IncParamBuffer(pBuf, pNull, pSize, nDataBufSize, pValue);

		if (&SrcParam == &Param)
			break;
	}
}

int ISACursor::OutputParamIndex(int nParamPos) const
{
	assert(nParamPos < m_pCommand->ParamCount());

	int i, nCountPos = 0;
	for (i = 0; i < m_pCommand->ParamCount(); ++i)
	{
		SAParam &Param = m_pCommand->ParamByIndex(i);
		if (!isOutputParam(Param))
			continue;

		if (Param.ParamDirType() == SA_ParamReturn)
		{
			if (0 == nParamPos)
				return i;
		}
		else
			++nCountPos;

		if (nCountPos == nParamPos)
			return i;
	}

	assert(false);
	return (-1);
}

int ISACursor::OutputParamPos(SAParam* pParam) const
{
    if (pParam->ParamDirType() == SA_ParamReturn)
        return 0;

    assert(NULL != pParam && isOutputParam(*pParam));

    for (int i = 0; i < m_pCommand->ParamCount(); ++i)
    {
        SAParam &Param = m_pCommand->ParamByIndex(i);
        if (!isOutputParam(Param))
            continue;
        
        if (&Param == pParam)
                return i;
    }

    assert(false);
    return (-1);
}

void ISACursor::ConvertOutputParams()
{
	int nOutputs = 0;
	int nNotConverted = 0;

	void *pBuf = m_pParamBuffer;
	for (int i = 0; i < m_pCommand->ParamCount(); ++i)
	{
		SAParam &Param = m_pCommand->ParamByIndex(i);

		void *pNull;
		void *pSize;
		size_t nDataBufSize;
		void *pValue;
		IncParamBuffer(pBuf, pNull, pSize, nDataBufSize, pValue);

		if (!isOutputParam(Param))
			continue;

		int nPos;
		if (Param.ParamDirType() == SA_ParamReturn)
			nPos = 0;
		else
			nPos = ++nOutputs;

		SADataType_t eParamType = Param.ParamType();
		if (eParamType == SA_dtUnknown)
            throw SAException(SA_Library_Error, SA_Library_Error_UnknownParameterType, -1,
			IDS_UNKNOWN_PARAMETER_TYPE, (const SAChar*)Param.Name());

		bool bConverted = ConvertValue(nPos, nNotConverted, m_nParamIndSize,
			pNull, m_nParamSizeSize, pSize, nDataBufSize, pValue,
			ISA_ParamValue, Param, 0);

		if (!bConverted)
			++nNotConverted;

		if (!Param.isNull() && isLongOrLob(eParamType))
			ConvertLongOrLOB(ISA_ParamValue, Param, pValue, nDataBufSize);
	}
}

void ISACursor::ConvertSelectBufferToFields(int nBulkReadingBufPos)
{
	int cFieldCount = m_pCommand->FieldCount();
	int nNotConverted = 0;

	void *pBuf = m_pSelectBuffer;
	for (int iField = 1; iField <= cFieldCount && pBuf; ++iField)
	{
		SAField &Field = m_pCommand->Field(iField);
		SADataType_t eFieldType = Field.FieldType();

		if (eFieldType == SA_dtUnknown && !IgnoreUnknownDataType())
            throw SAException(SA_Library_Error, SA_Library_Error_UnknownColumnType, -1, IDS_UNKNOWN_COLUMN_TYPE, (const SAChar*)Field.Name());

		void *pInd;
		void *pSize;
		void *pValue;
		size_t nDataBufSize;
		IncFieldBuffer(pBuf, pInd, pSize, nDataBufSize, pValue);

		bool bConverted = ConvertValue(iField, nNotConverted, m_nIndSize, pInd,
			m_nSizeSize, pSize, nDataBufSize, pValue, ISA_FieldValue,
			Field, nBulkReadingBufPos);

		if (!bConverted)
		{
			++nNotConverted;
			continue;
		}

		if (!Field.isNull() && isLongOrLob(eFieldType))
			ConvertLongOrLOB(ISA_FieldValue, Field, pValue, nDataBufSize);
	}
}

/*virtual */
void ISACursor::ConvertLongOrLOB(ValueType_t eValueType, SAValueRead &vr,
	void *pValue, size_t nBufSize)
{
	SADataType_t eDataType;
	switch (eValueType)
	{
	case ISA_FieldValue:
		eDataType = ((SAField&)vr).FieldType();
		break;
	default:
		assert(eValueType == ISA_ParamValue);
		eDataType = ((SAParam&)vr).ParamType();
	}
	switch (eDataType)
	{
	case SA_dtLongBinary:
	case SA_dtLongChar:
	case SA_dtBLob:
	case SA_dtCLob:
		if (vr.LongOrLobReaderMode() == SA_LongOrLobReaderDefault)
			ReadLongOrLOB(eValueType, vr, pValue, nBufSize, NULL, 0, NULL);
		break;
	default:
		assert(false);
	}
}

int ISACursor::FieldCount(int nCount, ...) const
{
	int nFields = 0;

	va_list argList;
	va_start(argList, nCount);

	for (int i = 0; i < nCount; i++)
	{
		// int is OK
		// need a cast here since va_arg only
		// takes fully promoted types
		SADataType_t eDataType = (SADataType_t)va_arg(argList, int);
		for (int j = 0; j < m_pCommand->FieldCount(); j++)
			if (m_pCommand->Field(j + 1).FieldType() == eDataType)
				++nFields;
	}

	va_end(argList);
	return nFields;
}

int ISACursor::FieldCountKnownOnly() const
{
	int nFields = 0;

	for (int j = 0; j < m_pCommand->FieldCount(); j++)
	{
		if (m_pCommand->Field(j + 1).FieldType() != SA_dtUnknown)
			++nFields;
	}
	return nFields;
}

short ISACursor::ParamDirCount(int nPlaceHolderCount,
	saPlaceHolder **ppPlaceHolders, int nCount, ...) const
{
	short nPlaceHolders = 0;

	va_list argList;
	va_start(argList, nCount);

	for (int i = 0; i < nCount; i++)
	{
		// int is OK
		// need a cast here since va_arg only
		// takes fully promoted types
		SAParamDirType_t eType = (SAParamDirType_t)va_arg(argList, int);
		for (int j = 0; j < nPlaceHolderCount; j++)
			if (ppPlaceHolders[j]->getParam()->ParamDirType() == eType)
				++nPlaceHolders;
	}

	va_end(argList);
	return nPlaceHolders;
}

/*static */
bool ISACursor::isLong(SADataType_t eDataType)
{
	switch (eDataType)
	{
	case SA_dtLongBinary:
	case SA_dtLongChar:
		return true;
	default:
		break;
	}

	return false;
}

/*static */
bool ISACursor::isLob(SADataType_t eDataType)
{
	switch (eDataType)
	{
	case SA_dtBLob:
	case SA_dtCLob:
		return true;
	default:
		break;
	}

	return false;
}

/*static */
bool ISACursor::isLongOrLob(SADataType_t eDataType)
{
	return isLong(eDataType) || isLob(eDataType);
}

/*static */
bool ISACursor::isInputParam(const SAParam& Param)
{
	switch (Param.ParamDirType())
	{
	case SA_ParamInput:
	case SA_ParamInputOutput:
		return true;
	case SA_ParamOutput:
	case SA_ParamReturn:
		break;
	default:
		assert(false);
	}

	return false;
}

/*static */
bool ISACursor::isOutputParam(const SAParam& Param)
{
	switch (Param.ParamDirType())
	{
	case SA_ParamInputOutput:
	case SA_ParamOutput:
	case SA_ParamReturn:
		return true;
	case SA_ParamInput:
		break;
	default:
		assert(false);
	}

	return false;
}

int ISACursor::BulkReadingBufSize() const
{
	SAString s = m_pCommand->Option("BulkReadingBufSize");
	if (s.IsEmpty())
		return 0; // defaults to non bulk reading

	return sa_toi(s);
}

void ISACursor::WriteLongOrLobToInternalValue(SAValue &value)
{
	if (value.m_fnWriter != NULL)
	{
		value.m_pString->Empty();

		size_t nActualWrite;
		SAPieceType_t ePieceType = SA_FirstPiece;
		void *pBuf;
		while ((nActualWrite = value.InvokeWriter(ePieceType, SA_DefaultMaxLong, pBuf))
			!= 0)
		{
			// for non-binary (text) data:
			// nActualWrite is in bytes not characters,
			// so binary processing is used always
			//(*value.m_pString) += SAString(pBuf, nActualWrite);
			size_t nBinaryLen = value.m_pString->GetBinaryLength();
			memcpy((unsigned char*)value.m_pString->GetBinaryBuffer(nBinaryLen
				+ nActualWrite) + nBinaryLen, pBuf, nActualWrite);
			value.m_pString->ReleaseBinaryBuffer(nBinaryLen + nActualWrite);

			if (ePieceType == SA_LastPiece)
				break;
		}
	}
}

//////////////////////////////////////////////////////////////////////
// SADummyConverter Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SADummyConverter::SADummyConverter()
{
	m_nExternalDataSize = 0l;
	m_bFinalPiecePending = false;

	m_eCnvtPieceType = SA_OnePiece; // assumption only
}

//////////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////////

void SADummyConverter::FlushExternalData(unsigned char *
#ifndef NDEBUG
	pData
#endif
	, size_t &nDataSize)
{
	assert(!SADummyConverter::IsEmpty());

	assert(pData == m_pExternalData); // we are just a repeater, we are not expected to copy data into pData
	nDataSize = m_nExternalDataSize;

	// we've given all the data
	m_nExternalDataSize = 0l;
}

void SADummyConverter::SetExternalData(unsigned char *pExternalData,
	size_t nExternalDataSize)
{
	m_pExternalData = pExternalData;
	m_nExternalDataSize = nExternalDataSize;
}

//////////////////////////////////////////////////////////////////////
// Overides
//////////////////////////////////////////////////////////////////////

/*virtual */
void SADummyConverter::PutStream(unsigned char *pExternalData,
	size_t nExternalDataSize, SAPieceType_t eExternalPieceType)
{
	// we do not support several PutStream calls one by one (no buffering)
	// if buffering is required, it should be implemented transparently by
	// inherited classes
	assert(SADummyConverter::IsEmpty());

	SetExternalData(pExternalData, nExternalDataSize);
	m_eExternalPieceType = eExternalPieceType;

	// special case
	if ((eExternalPieceType == SA_OnePiece || eExternalPieceType
		== SA_LastPiece))
		m_bFinalPiecePending = true;
}

bool SADummyConverter::StreamIsEnough(size_t nWantedSize,
	size_t nExternalDataSize) const
{
	assert(nExternalDataSize <= nWantedSize);

	bool bFinalExternalPiece = m_eExternalPieceType == SA_OnePiece
		|| m_eExternalPieceType == SA_LastPiece;
	// check if there isn't enough data
	if (!bFinalExternalPiece && nExternalDataSize < nWantedSize)
		return false;

	return true;
}

/*virtual */
bool SADummyConverter::GetStream(unsigned char *pData, size_t nWantedSize,
	size_t &nDataSize, SAPieceType_t &eCnvtPieceType)
{
	// we do not support buffering
	// if buffering is required, it should be implemented transparently
	// by inherited classes
	if (SADummyConverter::IsEmpty())
		return false;

	assert(m_nExternalDataSize <= nWantedSize);

	bool bFinalExternalPiece = m_eExternalPieceType == SA_OnePiece
		|| m_eExternalPieceType == SA_LastPiece;

	// check if there isn't enough data
	// Why? We can return actual size and we must return actual size
	/*
	if( !bFinalExternalPiece && m_nExternalDataSize < nWantedSize )
	return false;
	*/

	// we have enough data to return, let us do it
	FlushExternalData(pData, nDataSize);
	m_bFinalPiecePending = false;

	// virtual call, it shows if inherited class(es) caches some data
	bool bEmptyAfterFlush = IsEmpty();

	// adjust m_eCnvtPieceType
	switch (m_eCnvtPieceType)
	{
	case SA_OnePiece:
		if (!bEmptyAfterFlush || !bFinalExternalPiece)
			m_eCnvtPieceType = SA_FirstPiece;
		break;
	case SA_FirstPiece:
	case SA_NextPiece:
		m_eCnvtPieceType
			= bFinalExternalPiece ? (bEmptyAfterFlush ? SA_LastPiece
			: SA_NextPiece) : SA_NextPiece;
		break;
	default:
		assert(false);
	}

	eCnvtPieceType = m_eCnvtPieceType;
	return true;
}

/*virtual */
bool SADummyConverter::IsEmpty() const
{
	return m_nExternalDataSize == 0 && !m_bFinalPiecePending;
}

//////////////////////////////////////////////////////////////////////
// SABufferConverter Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SABufferConverter::SABufferConverter()
{
	m_nBufferSizeAtGetAppendBufferCall = 0l;
}

/*virtual */
SABufferConverter::~SABufferConverter()
{
}

//////////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////////

unsigned char *SABufferConverter::GetAppendBuffer(size_t nAppendBytesMaxCount)
{
	m_nBufferSizeAtGetAppendBufferCall = m_Buffer.GetBinaryLength();
	void *pBuf = m_Buffer.GetBinaryBuffer(m_nBufferSizeAtGetAppendBufferCall
		+ nAppendBytesMaxCount);
	return (unsigned char*)pBuf + m_nBufferSizeAtGetAppendBufferCall;
}

void SABufferConverter::ReleaseAppendBuffer(size_t nAppendBytesCount)
{
	m_Buffer.ReleaseBinaryBuffer(m_nBufferSizeAtGetAppendBufferCall
		+ nAppendBytesCount);
}

//////////////////////////////////////////////////////////////////////
// Overides
//////////////////////////////////////////////////////////////////////

/*virtual */
bool SABufferConverter::GetStream(unsigned char *pData, size_t nWantedSize,
	size_t &nDataSize, SAPieceType_t &eDataPieceType)
{
	// do not simply return false
	// let parent class do this
	// (and maybe some additional processing, for example reporting empty last piece)
	if (SABufferConverter::IsEmpty())
		return SADummyConverter::GetStream(pData, nWantedSize, nDataSize,
		eDataPieceType);

	if (m_Buffer.IsEmpty())
	{
		bool bRes = SADummyConverter::GetStream(pData, nWantedSize, nDataSize,
			eDataPieceType);
		if (bRes) // buffer is empty but parent converter has enough data
			return bRes;

		// we need to save data (buffering)
		FlushExternalData(pData, nDataSize);
		void *pBuf = m_Buffer.GetBinaryBuffer(nDataSize);
		memcpy(pBuf, pData, nDataSize);
		m_Buffer.ReleaseBinaryBuffer(nDataSize);
		return false;
	}

	// extend the buffer
	// concatenate old buffer with new data
	size_t nCurSize = m_Buffer.GetBinaryLength();
	if (SADummyConverter::IsEmpty())
		nDataSize = 0l;
	else
		FlushExternalData(pData, nDataSize);
	unsigned char *pBuf = (unsigned char*)m_Buffer.GetBinaryBuffer(nCurSize
		+ nDataSize);
	memcpy(pBuf + nCurSize, pData, nDataSize);

	size_t nSizeToReturn = sa_min(nCurSize + nDataSize, nWantedSize);
	if (StreamIsEnough(nWantedSize, nSizeToReturn))
	{
		// move some data from buffer to parent converter
		memcpy(pData, pBuf, nSizeToReturn);
		SetExternalData(pData, nSizeToReturn);

		// shrink the buffer
		// use memmove, areas can overlap
		memmove(pBuf, pBuf + nSizeToReturn, nCurSize + nDataSize
			- nSizeToReturn);
		m_Buffer.ReleaseBinaryBuffer(nCurSize + nDataSize - nSizeToReturn);

		// we do not simply return data to caller
		// instead, we let parent class to do that
		// (and maybe some additional processing, for example reporting last piece or eDataPieceType management)
		bool bRes = SADummyConverter::GetStream(pData, nWantedSize, nDataSize,
			eDataPieceType);
		//assert(bRes);
		return bRes;
	}

	// !StreamIsEnough(...)
	m_Buffer.ReleaseBinaryBuffer(nCurSize + nDataSize);
	// do not simply return false
	// let parent class do this
	// (and maybe some additional processing, for example reporting empty last piece)
	return SADummyConverter::GetStream(pData, nWantedSize, nDataSize,
		eDataPieceType);
}

/*virtual */
bool SABufferConverter::IsEmpty() const
{
	if (m_Buffer.IsEmpty())
		return SADummyConverter::IsEmpty();

	return false;
}

//////////////////////////////////////////////////////////////////////
// SAUnicode2MultibyteConverter Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SAUnicode2MultibyteConverter::SAUnicode2MultibyteConverter()
{
#ifdef SA_UNICODE
	m_bUseUTF = false;
#endif
	m_ReminderBytesCount = 0l;
	m_MB_CUR_MAX = (unsigned int)MB_CUR_MAX;
}

/*virtual */
SAUnicode2MultibyteConverter::~SAUnicode2MultibyteConverter()
{
}

#ifdef SA_UNICODE
void SAUnicode2MultibyteConverter::SetUseUTF8(bool bUseUTF8/* = true*/)
{
	m_bUseUTF = bUseUTF8;
	m_MB_CUR_MAX = m_bUseUTF ? (unsigned int) 0x04:(unsigned int) MB_CUR_MAX;
}
#endif

//////////////////////////////////////////////////////////////////////
// Overides
//////////////////////////////////////////////////////////////////////

/*virtual */
bool SAUnicode2MultibyteConverter::GetStream(unsigned char *pData,
	size_t nWantedSize, size_t &nDataSize, SAPieceType_t &eDataPieceType)
{
	// do not simply return false
	// let parent class do this
	// (and maybe some additional processing, for example reporting empty last piece)
	if (SAUnicode2MultibyteConverter::IsEmpty())
		return SABufferConverter::GetStream(pData, nWantedSize, nDataSize,
		eDataPieceType);

	// very simple technique: convert all the data and put it 
	// into the buffer of parent class, then let it deal with it
	if (SADummyConverter::IsEmpty())
		nDataSize = 0l;
	else
		FlushExternalData(pData, nDataSize);

	// chek if we have a reminder from previous conversion
	unsigned char *pNewData = pData;
	if (m_ReminderBytesCount)
	{
		assert(m_ReminderBytesCount < sizeof(wchar_t));
		// try to complete one Unicode character
		while (m_ReminderBytesCount < sizeof(wchar_t) && nDataSize > 0)
		{
			m_chReminderBytes[m_ReminderBytesCount] = *pNewData;
			++m_ReminderBytesCount;
			--nDataSize;
			++pNewData;
		}
	}
	assert(m_ReminderBytesCount <= sizeof(wchar_t));

	size_t nNewUnicodeChars = nDataSize / sizeof(wchar_t);

	if (m_ReminderBytesCount == sizeof(wchar_t) || nNewUnicodeChars)
	{
		// assume that each Unicode character can be converted to max MB_CUR_MAX character
		size_t nNewMultiByteBytesMax = m_MB_CUR_MAX * (nNewUnicodeChars
			+ (m_ReminderBytesCount ? 1 : 0));
		void *pMultiByteBuffer = GetAppendBuffer(nNewMultiByteBytesMax);

		size_t nNewReminderMultiByteBytes = 0l;
		if (m_ReminderBytesCount)
		{
			assert(m_ReminderBytesCount == sizeof(wchar_t));
			nNewReminderMultiByteBytes = SAWideCharToMultiByte(
				(char*)pMultiByteBuffer, (wchar_t*)m_chReminderBytes, 1);

			// reminder has been converted, flush it
			m_ReminderBytesCount = 0l;
		}

		size_t nNewDataMultiByteBytes = 0l;
		if (nNewUnicodeChars)
		{
#ifdef SA_UNICODE
			if( m_bUseUTF )
				nNewDataMultiByteBytes = SAWideCharToUTF8(
				(char*) pMultiByteBuffer + nNewReminderMultiByteBytes,
				(const wchar_t*) pNewData, nNewUnicodeChars);
			else
#endif
				nNewDataMultiByteBytes = SAWideCharToMultiByte(
				(char*)pMultiByteBuffer + nNewReminderMultiByteBytes,
				(const wchar_t*)pNewData, nNewUnicodeChars);
		}

		assert(nNewReminderMultiByteBytes + nNewDataMultiByteBytes <= nNewMultiByteBytesMax);
		ReleaseAppendBuffer(nNewReminderMultiByteBytes + nNewDataMultiByteBytes);
	}

	// save new reminder bytes if any
	size_t NewReminderBytesCount = nDataSize % sizeof(wchar_t);
	if (NewReminderBytesCount)
	{
		assert(m_ReminderBytesCount == 0);
		while (m_ReminderBytesCount < NewReminderBytesCount)
		{
			m_chReminderBytes[m_ReminderBytesCount] = pNewData[nNewUnicodeChars
				* sizeof(wchar_t) + m_ReminderBytesCount];
			++m_ReminderBytesCount;
		}
	}

	return SABufferConverter::GetStream(pData, nWantedSize, nDataSize,
		eDataPieceType);
}

/*virtual */
bool SAUnicode2MultibyteConverter::IsEmpty() const
{
	if (!m_ReminderBytesCount)
		return SABufferConverter::IsEmpty();

	return false;
}

//////////////////////////////////////////////////////////////////////
// SAMultibyte2UnicodeConverter Class
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SAMultibyte2UnicodeConverter::SAMultibyte2UnicodeConverter()
{
#ifdef SA_UNICODE
	m_bUseUTF = false;
#endif
	m_bFatalError = false;
	m_chReminderBytes = NULL;
	m_ReminderBytesCount = 0l;
	m_MB_CUR_MAX = (unsigned int)MB_CUR_MAX;
}

/*virtual */
SAMultibyte2UnicodeConverter::~SAMultibyte2UnicodeConverter()
{
	if (m_chReminderBytes)
		::free(m_chReminderBytes);
}

#ifdef SA_UNICODE
void SAMultibyte2UnicodeConverter::SetUseUTF8(bool bUseUTF8/* = true*/)
{
	m_bUseUTF = bUseUTF8;
	m_MB_CUR_MAX = m_bUseUTF ? (unsigned int) 0x04:(unsigned int) MB_CUR_MAX;
}
#endif

//////////////////////////////////////////////////////////////////////
// Overides
//////////////////////////////////////////////////////////////////////

/*virtual */
bool SAMultibyte2UnicodeConverter::GetStream(unsigned char *pData,
	size_t nWantedSize, size_t &nDataSize, SAPieceType_t &eDataPieceType)
{
	// do not simply return false
	// let parent class do this (and maybe some additional processing)
	if (SAMultibyte2UnicodeConverter::IsEmpty())
		return SABufferConverter::GetStream(pData, nWantedSize, nDataSize,
		eDataPieceType);

	// very simple trick: convert all the data and put it
	// into the buffer of parent class, then let it deal with it
	if (SADummyConverter::IsEmpty())
		nDataSize = 0l;
	else
		FlushExternalData(pData, nDataSize);

	// chek if we have a reminder from previous conversion
	unsigned char *pNewData = pData;
	bool bConvertedFromReminder = false;
	wchar_t wchConvertedFromReminder = 0;
	if (m_ReminderBytesCount)
	{
		assert(m_ReminderBytesCount < m_MB_CUR_MAX);
		// try to complete one Mulibyte character
		while (m_ReminderBytesCount < m_MB_CUR_MAX && nDataSize > 0)
		{
			m_chReminderBytes[m_ReminderBytesCount] = *pNewData;
			++m_ReminderBytesCount;
			--nDataSize;
			++pNewData;

			// check if Multibyte character has got valid
			int n = -1;
#ifdef SA_UNICODE
			char* pStop;
			if( m_bUseUTF && SAUTF8ToWideChar(&wchConvertedFromReminder,
				m_chReminderBytes, m_ReminderBytesCount, &pStop) )
				n = (int) (pStop - m_chReminderBytes);
			else
#endif
				n = ::mbtowc(&wchConvertedFromReminder, m_chReminderBytes, m_ReminderBytesCount);
			if (n >= 0)
			{
				assert(n == (int)m_ReminderBytesCount);
				bConvertedFromReminder = true;
				break;
			}
		}
	}
	assert(m_ReminderBytesCount <= m_MB_CUR_MAX);
	if (m_ReminderBytesCount == m_MB_CUR_MAX && !bConvertedFromReminder)
	{
		m_bFatalError = true;
		// reminder could not be converted, flush it
		m_ReminderBytesCount = 0l;
	}

	// if we have got fatal error just flush
	// buffered data (already converted and valid) to client if any
	if (m_bFatalError)
		return SABufferConverter::GetStream(pData, nWantedSize, nDataSize,
		eDataPieceType);

	size_t nNewMultibyteBytes = nDataSize;
	size_t nNewReminderBytesCount = nNewMultibyteBytes; // worst case
	char *lpchStop = (char*)pNewData; // worst case

	if (bConvertedFromReminder || nNewMultibyteBytes)
	{
		// in the worst case each byte can represent a Unicode character
		size_t nNewUnicodeBytesMax = sizeof(wchar_t) * (nNewMultibyteBytes
			+ (bConvertedFromReminder ? 1 : 0));
		void *pUnicodeBuffer = GetAppendBuffer(nNewUnicodeBytesMax);

		if (bConvertedFromReminder)
		{
			assert(m_ReminderBytesCount == sizeof(wchar_t));
			memcpy(pUnicodeBuffer, &wchConvertedFromReminder,
				sizeof(wchConvertedFromReminder));

			// reminder has been converted, flush it
			m_ReminderBytesCount = 0l;
		}

		size_t nNewDataUnicodeChars = 0l;
		if (nNewMultibyteBytes)
		{
#ifdef SA_UNICODE
			if( m_bUseUTF )
				nNewDataUnicodeChars = SAUTF8ToWideChar(
				(wchar_t*) pUnicodeBuffer + (bConvertedFromReminder ? 1
				:0), (const char*) pNewData,
				nNewMultibyteBytes, &lpchStop);
			else
#endif
				nNewDataUnicodeChars = SAMultiByteToWideChar(
				(wchar_t*)pUnicodeBuffer + (bConvertedFromReminder ? 1
				: 0), (const char*)pNewData,
				nNewMultibyteBytes, &lpchStop);
			nNewReminderBytesCount = nNewMultibyteBytes
				- ((unsigned char*)lpchStop - (unsigned char*)pNewData);
		}

		assert(sizeof(wchar_t) * (nNewDataUnicodeChars + (bConvertedFromReminder ? 1 : 0)) <= nNewUnicodeBytesMax);
		ReleaseAppendBuffer(sizeof(wchar_t) * (nNewDataUnicodeChars
			+ (bConvertedFromReminder ? 1 : 0)));
	}

	// save new reminder bytes if any
	if (nNewReminderBytesCount)
	{
		assert(m_ReminderBytesCount == 0);
		if (nNewReminderBytesCount < m_MB_CUR_MAX) // not fatal yet
		{
			sa_realloc((void**)&m_chReminderBytes, m_MB_CUR_MAX);
			while (m_ReminderBytesCount < nNewReminderBytesCount)
			{
				m_chReminderBytes[m_ReminderBytesCount]
					= lpchStop[m_ReminderBytesCount];
				++m_ReminderBytesCount;
			}
		}
		else
		{
			m_bFatalError = true;
		}
	}

	return SABufferConverter::GetStream(pData, nWantedSize, nDataSize,
		eDataPieceType);
}

/*virtual */
bool SAMultibyte2UnicodeConverter::IsEmpty() const
{
	if (!m_ReminderBytesCount)
		return SABufferConverter::IsEmpty();

	return false;
}

#if defined(SA_UNICODE) && !defined(SQLAPI_WINDOWS)
//////////////////////////////////////////////////////////////////////
// SAUTF16UnicodeConverter Class

SAUTF16UnicodeConverter::SAUTF16UnicodeConverter()
{
	m_chReminderBytes = NULL;
	m_ReminderBytesCount = 0l;
	m_bFatalError = false;
}

/*virtual */
SAUTF16UnicodeConverter::~SAUTF16UnicodeConverter()
{
	if( m_chReminderBytes )
		::free(m_chReminderBytes);
}

/*virtual */
bool SAUTF16UnicodeConverter::GetStream(unsigned char *pData,
	size_t nWantedSize, size_t &nDataSize, SAPieceType_t &eDataPieceType)
{
	// do not simply return false
	// let parent class do this (and maybe some additional processing)
	if( SAUTF16UnicodeConverter::IsEmpty() )
		return SABufferConverter::GetStream(pData, nWantedSize, nDataSize,
		eDataPieceType);

	// very simple trick: convert all the data and put it
	// into the buffer of parent class, then let it deal with it
	if( SADummyConverter::IsEmpty() )
		nDataSize = 0l;
	else
		FlushExternalData(pData, nDataSize);

	// check if we have a reminder from previous conversion
	unsigned char *pNewData = pData;
	bool bConvertedFromReminder = false;
	UTF32 wchConvertedFromReminder = 0;
	if( m_ReminderBytesCount )
	{
		assert(m_ReminderBytesCount < 2*sizeof(UTF16)); // 2 UTF16 -> UTF32
		// try to complete one UTF16 character
		while( m_ReminderBytesCount < 2 * sizeof(UTF16) && nDataSize > 0 )
		{
			m_chReminderBytes[m_ReminderBytesCount] = *pNewData;
			++m_ReminderBytesCount;
			--nDataSize;
			++pNewData;

			// check if utf16 character has got valid
			const UTF16* in = (UTF16*) m_chReminderBytes;
			if( utf16_to_utf32(&in, m_ReminderBytesCount / sizeof(UTF16),
				&wchConvertedFromReminder, 1, UTF16_IGNORE_ERROR) )
			{
				bConvertedFromReminder = true;
				break;
			}
		}
	}
	assert(m_ReminderBytesCount <= 2*sizeof(UTF16));
	if( m_ReminderBytesCount == 2 * sizeof(UTF16) && !bConvertedFromReminder )
	{
		m_bFatalError = true;
		// reminder could not be converted, flush it
		m_ReminderBytesCount = 0l;
	}

	// if we have got fatal error just flush
	// buffered data (already converted and valid) to client if any
	if( m_bFatalError )
		return SABufferConverter::GetStream(pData, nWantedSize, nDataSize,
		eDataPieceType);

	size_t nNewMultibyteBytes = nDataSize;
	size_t nNewReminderBytesCount = nNewMultibyteBytes; // worst case
	char *lpchStop = (char*) pNewData; // worst case

	if( bConvertedFromReminder || nNewMultibyteBytes )
	{
		// in the worst case each UTF16 can represent a Unicode character
		size_t nNewUnicodeBytesMax = sizeof(wchar_t) * (nNewMultibyteBytes
			/ sizeof(UTF16) + (bConvertedFromReminder ? 1:0));
		void *pUnicodeBuffer = GetAppendBuffer(nNewUnicodeBytesMax);

		if( bConvertedFromReminder )
		{
			// ? assert(m_ReminderBytesCount == sizeof(wchar_t));
			memcpy(pUnicodeBuffer, &wchConvertedFromReminder,
				sizeof(wchConvertedFromReminder));

			// reminder has been converted, flush it
			m_ReminderBytesCount = 0l;
		}

		size_t nNewDataUnicodeChars = 0l;
		if( nNewMultibyteBytes )
		{
			nNewDataUnicodeChars = utf16_to_utf32((const UTF16**) &lpchStop,
				nNewMultibyteBytes / sizeof(UTF16),
				(UTF32*) ((char*) pUnicodeBuffer
				+ (bConvertedFromReminder ? 1:0)),
				nNewUnicodeBytesMax / sizeof(wchar_t), UTF16_IGNORE_ERROR);
			nNewReminderBytesCount = nNewMultibyteBytes
				- ((unsigned char*) lpchStop - (unsigned char*) pNewData);
		}

		assert(sizeof(wchar_t) * (nNewDataUnicodeChars + (bConvertedFromReminder?1:0)) <= nNewUnicodeBytesMax);
		ReleaseAppendBuffer(sizeof(wchar_t) * (nNewDataUnicodeChars
			+ (bConvertedFromReminder ? 1:0)));
	}

	// save new reminder bytes if any
	if( nNewReminderBytesCount )
	{
		assert(m_ReminderBytesCount == 0);
		if( nNewReminderBytesCount < 2 * sizeof(UTF16) ) // not fatal yet
		{
			sa_realloc((void**) &m_chReminderBytes, 2 * sizeof(UTF16));
			while( m_ReminderBytesCount < nNewReminderBytesCount )
			{
				m_chReminderBytes[m_ReminderBytesCount]
					= lpchStop[m_ReminderBytesCount];
				++m_ReminderBytesCount;
			}
		}
		else
		{
			m_bFatalError = true;
		}
	}

	return SABufferConverter::GetStream(pData, nWantedSize, nDataSize,
		eDataPieceType);
}

/*virtual */
bool SAUTF16UnicodeConverter::IsEmpty() const
{
	if( !m_ReminderBytesCount )
		return SABufferConverter::IsEmpty();

	return false;
}

//////////////////////////////////////////////////////////////////////
// SAUnicodeUTF16Converter Class

SAUnicodeUTF16Converter::SAUnicodeUTF16Converter()
{
	m_ReminderBytesCount = 0l;
}

/*virtual */
SAUnicodeUTF16Converter::~SAUnicodeUTF16Converter()
{
}

/*virtual */
bool SAUnicodeUTF16Converter::GetStream(unsigned char *pData,
	size_t nWantedSize, size_t &nDataSize, SAPieceType_t &eDataPieceType)
{
	// do not simply return false
	// let parent class do this
	// (and maybe some additional processing, for example reporting empty last piece)
	if( SAUnicodeUTF16Converter::IsEmpty() )
		return SABufferConverter::GetStream(pData, nWantedSize, nDataSize,
		eDataPieceType);

	// very simple technique: convert all the data and put it
	// into the buffer of parent class, then let it deal with it
	if( SADummyConverter::IsEmpty() )
		nDataSize = 0l;
	else
		FlushExternalData(pData, nDataSize);

	// check if we have a reminder from previous conversion
	unsigned char *pNewData = pData;
	if( m_ReminderBytesCount )
	{
		assert(m_ReminderBytesCount < sizeof(wchar_t));
		// try to complete one Unicode character
		while( m_ReminderBytesCount < sizeof(wchar_t) && nDataSize > 0 )
		{
			m_chReminderBytes[m_ReminderBytesCount] = *pNewData;
			++m_ReminderBytesCount;
			--nDataSize;
			++pNewData;
		}
	}
	assert(m_ReminderBytesCount <= sizeof(wchar_t));

	size_t nNewUnicodeChars = nDataSize / sizeof(wchar_t);

	if( m_ReminderBytesCount == sizeof(wchar_t) || nNewUnicodeChars )
	{
		// assume that each Unicode character can be converted to max 2 UTF16 symbols
		size_t nNewMultiByteBytesMax = 2 * sizeof(UTF16) * (nNewUnicodeChars
			+ (m_ReminderBytesCount ? 1:0));
		void *pMultiByteBuffer = GetAppendBuffer(nNewMultiByteBytesMax);

		size_t nNewReminderMultiByteBytes = 0l;
		if( m_ReminderBytesCount )
		{
			assert(m_ReminderBytesCount == sizeof(wchar_t));
			const UTF32* in = (UTF32*) m_chReminderBytes;
			nNewReminderMultiByteBytes = utf32_to_utf16(&in, 1,
				(UTF16*) pMultiByteBuffer, nNewMultiByteBytesMax
				/ sizeof(UTF16), UTF16_IGNORE_ERROR)
				* sizeof(UTF16);

			// reminder has been converted, flush it
			m_ReminderBytesCount = 0l;
		}

		size_t nNewDataMultiByteBytes = 0l;
		if( nNewUnicodeChars )
		{
			const UTF32* in = (UTF32*) pNewData;
			nNewDataMultiByteBytes = utf32_to_utf16(&in, nNewUnicodeChars,
				(UTF16*) ((char*) pMultiByteBuffer
				+ nNewReminderMultiByteBytes),
				(nNewMultiByteBytesMax - nNewReminderMultiByteBytes)
				/ sizeof(UTF16), UTF16_IGNORE_ERROR)
				* sizeof(UTF16);
		}

		assert( nNewReminderMultiByteBytes+nNewDataMultiByteBytes <= nNewMultiByteBytesMax );
		ReleaseAppendBuffer(nNewReminderMultiByteBytes + nNewDataMultiByteBytes);
	}

	// save new reminder bytes if any
	size_t NewReminderBytesCount = nDataSize % sizeof(wchar_t);
	if( NewReminderBytesCount )
	{
		assert(m_ReminderBytesCount == 0);
		while( m_ReminderBytesCount < NewReminderBytesCount )
		{
			m_chReminderBytes[m_ReminderBytesCount] = pNewData[nNewUnicodeChars
				* sizeof(wchar_t) + m_ReminderBytesCount];
			++m_ReminderBytesCount;
		}
	}

	return SABufferConverter::GetStream(pData, nWantedSize, nDataSize,
		eDataPieceType);
}

/*virtual */
bool SAUnicodeUTF16Converter::IsEmpty() const
{
	if( !m_ReminderBytesCount )
		return SABufferConverter::IsEmpty();

	return false;
}
#endif // defined(SA_UNICODE) && !defined(SQLAPI_WINDOWS)


void ISACursor::setBatchExceptionPreHandler(PreHandleException_t fnHandler, void * pAddlData)
{
	this->m_fnPreHandleException = fnHandler;
	this->m_pPreHandleExceptionAddlData = pAddlData;
}

bool ISACursor::PreHandleException(SAException &x)
{

	if (this->m_fnPreHandleException)
		return this->m_fnPreHandleException(x, this->m_pPreHandleExceptionAddlData);

	return false;
}

bool ISACursor::getOptionValue(const SAString& sOption, bool bDefault) const
{
	SAString sValue = m_pCommand->Option(sOption);
	if (sValue.IsEmpty())
		return bDefault;
	return (0 == sValue.CompareNoCase(_TSA("TRUE"))
		|| 0 == sValue.CompareNoCase(_TSA("1")) ? true : false);
}

bool ISACursor::isSetScrollable() const
{
	SAString sOption = m_pCommand->Option(SACMD_SCROLLABLE);
	if (sOption.IsEmpty())
		sOption = m_pCommand->Option(_TSA("UseScrollableCursor"));
	if (sOption.IsEmpty())
		sOption = m_pCommand->Option(_TSA("UseDynamicCursor"));
	return (!sOption.IsEmpty() && (0 == sOption.CompareNoCase(_TSA("TRUE"))
		|| 0 == sOption.CompareNoCase(_TSA("1"))));
}

bool ISACursor::FetchPrior()
{
	return false;
}

bool ISACursor::FetchFirst()
{
	return false;
}

bool ISACursor::FetchLast()
{
	return false;
}

bool ISACursor::FetchPos(int/* offset*/, bool/* Relative = false*/)
{
    return false;
}

// Tracing feature
SATraceFunction_t saTraceFunc = NULL;
SATraceInfo_t saTraceInfo = SA_Trace_None;
void* saTraceData = NULL;

void SATrace(SATraceInfo_t traceInfo, SAConnection* pCon, SACommand* pCmd, const SAChar* szInfo)
{
	if (NULL != saTraceFunc && traceInfo == (traceInfo & saTraceInfo))
	{
		if (NULL == pCon && NULL != pCmd)
			pCon = pCmd->Connection();
		saTraceFunc(traceInfo, pCon, pCmd, szInfo, saTraceData);
	}
}
