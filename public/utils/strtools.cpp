//////////////////////////////////////////////////////////////////////////////////
// Copyright Š Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Special String tools to do lesser memory errors
//////////////////////////////////////////////////////////////////////////////////

#include "core/core_common.h"
#include "strtools.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <locale.h>

static locale_t xgetlocale()
{
	// HACK: Assume the user's system language is the same to the game's language
	static locale_t loc = newlocale(LC_CTYPE_MASK, getenv("LANG"), LC_CTYPE);
	return loc;
}
#endif

char* xstrupr(char* str)
{
	ASSERT(str);
    char* it = str;

    while (*it != 0) { *it = toupper(*it); ++it; }

    return str;
}

char* xstrlwr(char* str)
{
	ASSERT(str);
    char* it = str;

    while (*it != 0) { *it = tolower(*it); ++it; }

    return str;
}

wchar_t* xwcslwr(wchar_t* str)
{
	ASSERT(str);

    wchar_t* it = str;

#ifdef _WIN32
    while (*it != 0) { *it = *CharLowerW(&(*it)); ++it; }
#else
    while (*it != 0) { *it = towlower_l(*it, xgetlocale()); ++it; }
#endif // _WIN32

    return str;
}

wchar_t* xwcsupr(wchar_t* str)
{
	ASSERT(str);

    wchar_t* it = str;

#ifdef _WIN32
    while (*it != 0) { *it = *CharUpperW(&(*it)); ++it; }
#else
    while (*it != 0) { *it = towupper_l(*it, xgetlocale()); ++it; }
#endif // _WIN32

    return str;
}

void CombinePathN(EqString& outPath, int num, ...)
{
	EqString newStr;

	va_list		argptr;
	va_start (argptr,num);

    for( int i = 0; i < num; i++ )        
    {
		EqString pathPart = va_arg( argptr, const char* );
		pathPart.Path_FixSlashes();

		if(pathPart.Length() > 0)
		{
			newStr.Append(_Es(pathPart));

			if(i != num-1 && *newStr.Right(1).ToCString() != CORRECT_PATH_SEPARATOR )
				newStr.Append(CORRECT_PATH_SEPARATOR);
		}
    }

	va_end (argptr);

	// drop old outPath, replace with new string
	outPath.Swap(newStr);
}

void FixSlashes( char* str )
{
	ASSERT(str);
    while ( *str )
    {
        if ( *str == INCORRECT_PATH_SEPARATOR )
        {
            *str = CORRECT_PATH_SEPARATOR;
        }
        str++;
    }
}

//------------------------------------------
// Converts string to 24-bit integer hash
//------------------------------------------
int StringToHash( const char *str, bool caseIns )
{
	ASSERT(str);
	int hash = strlen(str);
	for (; *str; str++)
	{
		int v1 = hash >> 19;
		int v0 = hash << 5;

		int chr = caseIns ? tolower(*str) : *str;

		hash = ((v0 | v1) + chr) & StringHashMask;
	}

	return hash;
}

//------------------------------------------
// Splits string into array
//------------------------------------------

void xstrsplit2( const char* pString, const char* *pSeparators, int nSeparators, Array<EqString> &outStrings )
{
	ASSERT(pString);
	ASSERT(pSeparators);

	outStrings.clear();
	const char* pCurPos = pString;
	while ( 1 )
	{
		int iFirstSeparator = -1;
		const char* pFirstSeparator = 0;
		for ( int i=0; i < nSeparators; i++ )
		{
			const char* pTest = xstristr( pCurPos, pSeparators[i] );
			if ( pTest && (!pFirstSeparator || pTest < pFirstSeparator) )
			{
				iFirstSeparator = i;
				pFirstSeparator = pTest;
			}
		}

		if ( pFirstSeparator )
		{
			// Split on this separator and continue on.
			int separatorLen = strlen( pSeparators[iFirstSeparator] );
			if ( pFirstSeparator > pCurPos )
			{
				outStrings.append(_Es( pCurPos, pFirstSeparator-pCurPos ));
			}

			pCurPos = pFirstSeparator + separatorLen;
		}
		else
		{
			// Copy the rest of the string
			if ( strlen( pCurPos ) )
			{
				outStrings.append( _Es( pCurPos ) );
			}
			return;
		}
	}
}

void xstrsplit( const char* pString, const char* pSeparator, Array<EqString> &outStrings )
{
	xstrsplit2( pString, &pSeparator, 1, outStrings );
}

//------------------------------------------
// Duplicates string
//------------------------------------------
char* xstrdup(const char*  s)
{
	ASSERT( s );

	char*  t;
	int len = strlen(s)+1;

    t = PPNew char[len];

    if (t)
	{
        strncpy(t,s,len);
    }
    return t;
}

// is space?
//------------------------------------------
bool xisspace(int c)
{
	// P.S. Don't look for the code in Windows documentation, it has BUG
	return (c == 0x20) || (c >= 0x09 && c <= 0x0d);
}

char* xstrstr(  const char* s1, const char* search )
{
	ASSERT( s1 );
	ASSERT( search );

	return strstr( (char* )s1, search );
}

int xstrfind(char* str, char* search)
{
	ASSERT(str);
	ASSERT(search);

	int len = strlen(str);
	int len2 = strlen(search); // search len

	for(int i = 0; i < len; i++)
	{
		if(str[i] == search[0])
		{
			bool ichk = true;

			for(int z = 0; z < len2; z++)
			{
				if(str[i+z] != search[z])
					ichk = false;
			}

			if(ichk)
				return i;
		}
	}

	return -1; // failure
}

// Finds a string in another string with a case insensitive test
char const* xstristr( char const* pStr, char const* pSearch )
{
	ASSERT(pStr);
	ASSERT(pSearch);

	if (!pStr || !pSearch)
		return 0;

	char const* pLetter = pStr;

	// Check the entire string
	while (*pLetter != 0)
	{
		// Skip over non-matches
		if (tolower(*pLetter) == tolower(*pSearch))
		{
			// Check for match
			char const* pMatch = pLetter + 1;
			char const* pTest = pSearch + 1;
			while (*pTest != 0)
			{
				// We've run off the end; don't bother.
				if (*pMatch == 0)
					return 0;

				if (tolower(*pMatch) != tolower(*pTest))
					break;

				++pMatch;
				++pTest;
			}

			// Found a match!
			if (*pTest == 0)
				return pLetter;
		}

		++pLetter;
	}

	return 0;
}

char* xstristr( char* pStr, char const* pSearch )
{
	return (char*)xstristr( (char const*)pStr, pSearch );
}

//------------------------------------------------------
// wide string
//------------------------------------------------------

// compares two strings
int xwcscmp( const wchar_t *s1, const wchar_t *s2)
{
	ASSERT( s1 );
	ASSERT( s2 );

	while (1)
	{
		if (*s1 != *s2)
			return -1;              // strings not equal
		if (!*s1)
			return 0;               // strings are equal
		s1++;
		s2++;
	}

	return -1;
}

// compares two strings case-insensetive
int xwcsicmp( const wchar_t* s1, const wchar_t* s2 )
{
	ASSERT( s1 );
	ASSERT( s2 );

	while (1)
	{
		if (towlower(*s1) != towlower(*s2))
			return -1;              // strings not equal

		if (!*s1)
			return 0;               // strings are equal
		s1++;
		s2++;
	}

	return -1;
}

// finds substring in string case insensetive
wchar_t* xwcsistr( wchar_t* pStr, wchar_t const* pSearch )
{
	ASSERT( pStr );
	ASSERT( pSearch );

	return (wchar_t*)xwcsistr( (wchar_t const*)pStr, pSearch );
}

// finds substring in string case insensetive
wchar_t const* xwcsistr( wchar_t const* pStr, wchar_t const* pSearch )
{
	ASSERT(pStr);
	ASSERT(pSearch);

	if (!pStr || !pSearch)
		return 0;

	wchar_t const* pLetter = pStr;

	// Check the entire string
	while (*pLetter != 0)
	{
		// Skip over non-matches
		if (tolower(*pLetter) == tolower(*pSearch))
		{
			// Check for match
			wchar_t const* pMatch = pLetter + 1;
			wchar_t const* pTest = pSearch + 1;
			while (*pTest != 0)
			{
				// We've run off the end; don't bother.
				if (*pMatch == 0)
					return 0;

				if (towlower(*pMatch) != towlower(*pTest))
					break;

				++pMatch;
				++pTest;
			}

			// Found a match!
			if (*pTest == 0)
				return pLetter;
		}

		++pLetter;
	}

	return 0;
}

//------------------------------------------------------
// string conversion
//------------------------------------------------------
namespace EqStringConv
{
	utf8_to_wchar::utf8_to_wchar(EqWString& outStr, const char* val, int len /*= -1*/)
	{
		ASSERT(val);

		m_utf8 = (ubyte*)val;

		int length = GetLength();

		if (len == -1)
			len = length;

		outStr.ExtendAlloc(length);

		do
		{
			uint32 wch = GetChar();

			if (!wch)
				break;

			outStr.Append(wch);
		} while (length--);
	}

	utf8_to_wchar::utf8_to_wchar(wchar_t* outStr, int maxLength, const char* val, int len /*= -1*/)
	{
		ASSERT(outStr);
		ASSERT(val);

		m_utf8 = (ubyte*)val;

		if (len == -1)
			len = maxLength;

		if (len > maxLength)
			len = maxLength;

		do
		{
			uint32 wch = GetChar();

			if (!wch)
				break;

			*outStr++ = wch;
		} while (len--);

		*outStr = '\0';
	}

	int utf8_to_wchar::GetLength()
	{
		int utfStringLength = 0;
		ubyte* tmp = m_utf8;
		while(true)
		{
			if (!GetChar())
				break;

			utfStringLength++;
		}
		m_utf8 = tmp;
		return utfStringLength;
	}

	inline uint32 udec(uint32 val)
	{
		return (val & 0x3f);
	}

	uint32 utf8_to_wchar::GetChar()
	{
		uint32 b1 = NextByte();

		if (!b1)
			return 0;

		// Determine whether we are dealing
		// with a one-, two-, three-, or four-
		// byte sequence.
		if ((b1 & 0x80) == 0)
		{
			// 1-byte sequence: 000000000xxxxxxx = 0xxxxxxx
			return b1;
		}
		else if ((b1 & 0xe0) == 0xc0)
		{
			// 2-byte sequence: 00000yyyyyxxxxxx = 110yyyyy 10xxxxxx
			uint32 r = (b1 & 0x1f) << 6;
			r |= udec(NextByte());
			return r;
		}
		else if ((b1 & 0xf0) == 0xe0)
		{
			// 3-byte sequence: zzzzyyyyyyxxxxxx = 1110zzzz 10yyyyyy 10xxxxxx
			uint32 r = (b1 & 0x0f) << 12;
			r |= udec(NextByte()) << 6;
			r |= udec(NextByte());
			return r;
		}
		else if ((b1 & 0xf8) == 0xf0)
		{
			// 4-byte sequence: 11101110wwwwzzzzyy + 110111yyyyxxxxxx
			//     = 11110uuu 10uuzzzz 10yyyyyy 10xxxxxx
			// (uuuuu = wwww + 1)
			int b2 = udec(NextByte());
			int b3 = udec(NextByte());
			int b4 = udec(NextByte());
			return ((b1 & 7) << 18) | ((b2 & 0x3f) << 12) |
				((b3 & 0x3f) << 6) | (b4 & 0x3f);
		}

		//bad start for UTF-8 multi-byte sequence
		return '?';
	}

	//--------------------------------------------------------------

	wchar_to_utf8::wchar_to_utf8(EqString& outStr, const wchar_t* val, int len/* = -1*/)
	{
		ASSERT(val);

		if (len == -1)
			len = wcslen(val) * 4;
		else
			len *= 4;

		// to not call too many allocations
		outStr.ExtendAlloc(len);

		uint32 code;
		do
		{
			code = *val++;

			if(code == 0)
				break;

			if (code <= 0x7F)
			{
				outStr.Append((char)code);
			}
			else if (code <= 0x7FF)
			{
				outStr.Append((code >> 6) + 192);
				outStr.Append((code & 63) + 128);
			}
			else if (code <= 0xFFFF)
			{
				outStr.Append((code >> 12) + 224);
				outStr.Append(((code >> 6) & 63) + 128);
				outStr.Append((code & 63) + 128);
			}
			else if (code <= 0x10FFFF)
			{
				outStr.Append((code >> 18) + 240);
				outStr.Append(((code >> 12) & 63) + 128);
				outStr.Append(((code >> 6) & 63) + 128);
				outStr.Append((code & 63) + 128);
			}
			else if (0xd800 <= code && code <= 0xdfff)
			{
				//invalid block of utf8
			}
		}
		while(len--);
	}

}
