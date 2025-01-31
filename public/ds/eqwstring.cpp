//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Equilibrium Engine string base
//
//				Some things was lovely hardcoded (like m_nLength)
//////////////////////////////////////////////////////////////////////////////////

#include "core/core_common.h"
#include "eqwstring.h"

#ifdef _MSC_VER
#pragma warning(disable: 4267)
#endif

#define EXTEND_CHARS	32	// 32 characters for extending
#define EQSTRING_BASE_BUFFER	32

const EqWString EqWString::EmptyStr;
static const PPSourceLine EqWStringSL = PPSourceLine::Make(nullptr, 0);

EqWString::EqWString()
{
	Empty();
}

EqWString::~EqWString()
{
	Clear();
}

// convert from UTF8 string
EqWString::EqWString(const char* pszString, int len)
{
	Assign( pszString, len );
}

EqWString::EqWString(const EqString& str, int nStart, int len)
{
	Assign( str, nStart, len );
}

EqWString::EqWString(const wchar_t* pszString, int len)
{
	Assign( pszString, len );
}

EqWString::EqWString(const EqWString &str, int nStart, int len)
{
	Assign( str, nStart, len );
}

EqWString::EqWString(EqWString&& str)
{
	m_nAllocated = str.m_nAllocated;
	m_nLength = str.m_nLength;
	m_pszString = str.m_pszString;
	str.m_nAllocated = 0;
	str.m_nLength = 0;
	str.m_pszString = nullptr;
}

EqWString EqWString::Format(const wchar_t* pszFormat, ...)
{
	EqWString newString;
	va_list argptr;

	va_start(argptr, pszFormat);
	newString = EqWString::FormatVa(pszFormat, argptr);
	va_end(argptr);

	return newString;
}

EqWString EqWString::FormatVa(const wchar_t* pszFormat, va_list argptr)
{
	EqWString newString;
	newString.Resize(512, false);

	va_list varg;
	va_copy(varg, argptr);
	const int reqSize = _vsnwprintf(newString.m_pszString, newString.m_nAllocated, pszFormat, varg);

	if (reqSize < newString.m_nAllocated)
	{
		newString.m_nLength = reqSize;
		return newString;
	}

	newString.Resize(reqSize+1, false);
	newString.m_nLength = _vsnwprintf(newString.m_pszString, newString.m_nAllocated+1, pszFormat, argptr);

	return newString;
}

// data for printing
const wchar_t* EqWString::GetData() const
{
	if(!m_pszString)
		return L"";

	return m_pszString;
}

// length of it
uint16 EqWString::Length() const
{
	return m_nLength;
}

// string allocated size in bytes
uint16 EqWString::GetSize() const
{
	return m_nAllocated;
}

// erases and deallocates data
void EqWString::Clear()
{
	SAFE_DELETE_ARRAY(m_pszString);

	m_nLength = 0;
	m_nAllocated = 0;
}

// empty the string, but do not deallocate
void EqWString::Empty()
{
	Resize(EQSTRING_BASE_BUFFER, false);
}

// an internal operation of allocation/extend
bool EqWString::ExtendAlloc(int nSize)
{
	if(nSize+1u > m_nAllocated || m_pszString == nullptr)
	{
		nSize += EXTEND_CHARS;
		if(!Resize( nSize - nSize % EXTEND_CHARS ))
			return false;
	}

	return true;
}

// just a resize
bool EqWString::Resize(int nSize, bool bCopy)
{
	const int newSize = max(EQSTRING_BASE_BUFFER, nSize + 1);

	// make new and copy
	wchar_t* pszNewBuffer = PPNewSL(EqWStringSL) wchar_t[ newSize ];

	// allocation error!
	if(!pszNewBuffer)
		return false;

	pszNewBuffer[0] = 0;

	// copy and remove old if available
	if( m_pszString )
	{
		// if we have to copy
		if(bCopy && m_nLength)
		{
			// if string length if bigger, that the new alloc, cut off
			// for safety
			if(m_nLength > newSize)
				m_pszString[newSize] = 0;

			wcscpy(pszNewBuffer, m_pszString);
		}

		// now it's not needed
		SAFE_DELETE_ARRAY(m_pszString);
	}

	// assign
	m_pszString = pszNewBuffer;
	m_nAllocated = newSize;

	if (nSize < m_nLength)
		m_nLength = wcslen( m_pszString );

	return true;
}

// string assignment with conversion (or setvalue)
void EqWString::Assign(const char* pszStr, int len)
{
	EqStringConv::utf8_to_wchar conv( (*this), pszStr, len);
}

void EqWString::Assign(const EqString &str, int nStart, int len)
{
	EqStringConv::utf8_to_wchar conv((*this), str.ToCString() + nStart);
}

// string assignment (or setvalue)
void EqWString::Assign(const wchar_t* pszStr, int len)
{
	if(pszStr == nullptr)
	{
		if (m_pszString)
			m_pszString[0] = 0;
		m_nLength = 0;
		return;
	}

	if (len == -1)
		len = wcslen(pszStr);

	if (m_pszString == pszStr && len <= m_nLength)
	{
		m_nLength = len;
		m_pszString[len] = 0;
	}

	if (ExtendAlloc(len + 1))
	{
		if(pszStr != m_pszString)
			wcsncpy(m_pszString, pszStr, len);
		m_pszString[len] = 0;
	}
	m_nLength = len;
}

void EqWString::Assign(const EqWString &str, int nStart, int len)
{
	ASSERT(nStart >= 0);

	int nLen = str.Length();

	ASSERT(len <= nLen);

	if(len != -1)
		nLen = len;

	if( ExtendAlloc( nLen ) )
	{
		wcscpy( m_pszString+nStart, str.GetData() );
		m_pszString[nLen] = 0;
		m_nLength = nLen;
	}
}

void EqWString::Append(const wchar_t c)
{
	int nNewLen = m_nLength + 1;

	if( ExtendAlloc( nNewLen ) )
	{
		m_pszString[nNewLen-1] = c;
		m_pszString[nNewLen] = 0;
		m_nLength = nNewLen;
	}
}

// appends another string
void EqWString::Append(const wchar_t* pszStr, int nCount)
{
	if(pszStr == nullptr)
		return;

	int nLen = wcslen( pszStr );

	ASSERT(nCount <= nLen);

	if(nCount != -1)
		nLen = nCount;

	int nNewLen = m_nLength + nLen;

	if( ExtendAlloc( nNewLen ) )
	{
		wcsncpy( (m_pszString + m_nLength), pszStr, nLen);
		m_pszString[nNewLen] = 0;
		m_nLength = nNewLen;
	}
}

void EqWString::Append(const EqWString &str)
{
	int nNewLen = m_nLength + str.Length();

	if( ExtendAlloc( nNewLen ) )
	{
		wcscpy( (m_pszString + m_nLength), str.GetData() );
		m_pszString[nNewLen] = 0;
		m_nLength = nNewLen;
	}
}

// inserts another string at position
void EqWString::Insert(const wchar_t* pszStr, int nInsertPos)
{
	if(pszStr == nullptr)
		return;

	int nInsertCount = wcslen( pszStr );

	int nNewLen = m_nLength + nInsertCount;

	if( ExtendAlloc( nNewLen ) )
	{
		wchar_t* tmp = (wchar_t*)stackalloc(m_nLength - nInsertPos);
		wcscpy(tmp, &m_pszString[nInsertPos]);

		// copy the part to the far
		wcsncpy(&m_pszString[nInsertPos + nInsertCount], tmp, m_nLength - nInsertPos);

		// copy insertable
		wcsncpy(m_pszString + nInsertPos, pszStr, nInsertCount);

		m_pszString[nNewLen] = 0;
		m_nLength = nNewLen;
	}
}

void EqWString::Insert(const EqWString &str, int nInsertPos)
{
	int nNewLen = m_nLength + str.Length();

	if( ExtendAlloc( nNewLen ) )
	{
		wchar_t* tmp = (wchar_t*)stackalloc(m_nLength - nInsertPos);
		wcscpy(tmp, &m_pszString[nInsertPos]);

		// copy the part to the far
		wcsncpy(&m_pszString[nInsertPos + str.Length()], tmp, m_nLength - nInsertPos);

		// copy insertable
		wcsncpy(m_pszString + nInsertPos, str.GetData(), str.Length());

		m_pszString[nNewLen] = 0;
		m_nLength = nNewLen;
	}
}

// removes characters
void EqWString::Remove(int nStart, int nCount)
{
	wchar_t* temp = (wchar_t*)stackalloc( m_nAllocated*sizeof(wchar_t) );
	wcscpy(temp, m_pszString);

	wchar_t* pStr = m_pszString;

	uint realEnd = nStart+nCount;

	for(uint i = 0; i < m_nLength; i++)
	{
		if(i >= (uint)nStart && i < realEnd)
			continue;

		*pStr++ = temp[i];
	}
	*pStr = 0;

	int newLen = m_nLength-nCount;

	Resize( newLen );
}

// replaces characters
void EqWString::Replace( wchar_t whichChar, wchar_t to )
{
	wchar_t* pStr = m_pszString;

	for(uint i = 0; i < m_nLength; i++)
	{
		if(*pStr == 0)
			break;

		if(*pStr == whichChar)
			*pStr = to;

		pStr++;
	}
}

// string extractors
EqWString EqWString::Left(int nCount) const
{
	return Mid(0, nCount);
}

EqWString EqWString::Right(int nCount) const
{
	if ( (uint)nCount >= m_nLength )
		return (*this);

	return Mid( m_nLength - nCount, nCount );
}

EqWString EqWString::Mid(int nStart, int nCount) const
{
	int n;
	EqWString result;

	n = m_nLength;
	if( n == 0 || nCount <= 0 || nStart >= n )
		return result;

	if( uint(nStart+nCount) >= m_nLength )
		nCount = n-nStart;

	result.Append( &m_pszString[nStart], nCount );

	return result;
}

// convert to lower case
EqWString EqWString::LowerCase() const
{
	EqWString str(*this);
	xwcslwr(str.m_pszString);

	return str;
}

// convert to upper case
EqWString EqWString::UpperCase() const
{
	EqWString str(*this);
	xwcsupr(str.m_pszString);

    return str;
}

// search, returns char index
int	EqWString::Find(const wchar_t* pszSub, bool bCaseSensetive, int nStart) const
{
	if (!m_pszString)
		return -1;

	int nFound = -1;

	wchar_t* strStart = m_pszString + min((uint16)nStart, m_nLength);

	wchar_t* st = nullptr;

	if(bCaseSensetive)
		st = wcsstr(strStart, pszSub);
	else
		st = xwcsistr(strStart, pszSub);
	 
	if(st)
		nFound = (st - m_pszString);

	return nFound;
}

// searches for substring and replaces it
int EqWString::ReplaceSubstr(const wchar_t* find, const wchar_t* replaceTo, bool bCaseSensetive /*= false*/, int nStart /*= 0*/)
{
	// replace substring
	int foundStrIdx = Find(find, bCaseSensetive, nStart);
	if (foundStrIdx != -1)
		Assign(Left(foundStrIdx) + replaceTo + Mid(foundStrIdx + wcslen(find), Length()));

	return foundStrIdx;
}

/*
EqWString EqWString::StripFileExtension()
{
	for ( int i = m_nLength-1; i >= 0; i-- )
	{
		if ( m_pszString[i] == '.' )
		{
			EqWString str;
			str.Append( m_pszString, i );

			return str;
		}
	}

	return (*this);
}

EqWString EqWString::StripFileName()
{
	ASSERT(!"EqWString::StripFileName() not impletented");
	EqWString result(*this);
	//stripFileName(result.m_pszString);
	//result.m_nLength = strlen(result.m_pszString);

	return result;
}

EqWString EqWString::ExtractFileExtension()
{
	for ( int i = m_nLength-1; i >= 0; i-- )
	{
		if ( m_pszString[i] == '.' )
		{
			EqWString str;
			str.Append( &m_pszString[i+1] );

			return str;
		}
	}

	return EqWString();
}*/

// comparators
int	EqWString::Compare(const wchar_t* pszStr) const
{
	return xwcscmp(m_pszString, pszStr);
}

int	EqWString::Compare(const EqWString &str) const
{
	return xwcscmp(m_pszString, str.GetData());
}

int	EqWString::CompareCaseIns(const wchar_t* pszStr) const
{
	return xwcsicmp(m_pszString, pszStr);
}

int	EqWString::CompareCaseIns(const EqWString &str) const
{
	return xwcsicmp(m_pszString, str.GetData());
}

size_t EqWString::ReadString(IVirtualStream* stream, EqWString& output)
{
	uint16 length = 0;
	stream->Read(&length, 1, sizeof(length));
	output.Resize(length, false);

	stream->Read(output.m_pszString, sizeof(wchar_t), length);
	output.m_nLength = length;

	return 1;
}
