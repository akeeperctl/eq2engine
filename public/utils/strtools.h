//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Special String tools to do lesser memory errors
//////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifdef _WIN32
#	define CORRECT_PATH_SEPARATOR '\\'
#	define INCORRECT_PATH_SEPARATOR '/'
#else
#	define CORRECT_PATH_SEPARATOR '/'
#	define INCORRECT_PATH_SEPARATOR '\\'
#endif // _WIN32

static constexpr const char CORRECT_PATH_SEPARATOR_STR[2] = {CORRECT_PATH_SEPARATOR, '\0'};
static constexpr const char INCORRECT_PATH_SEPARATOR_STR[2] = {INCORRECT_PATH_SEPARATOR, '\0'};
static constexpr const int StringHashBits = 24;
static constexpr const int StringHashMask = ((1 << StringHashBits) - 1);

#ifdef PLAT_POSIX

#define _vsnwprintf vswprintf
#define _snprintf snprintf

#define stricmp(a, b)			strcasecmp(a, b)

#endif // LINUX

#ifdef PLAT_ANDROID

typedef __builtin_va_list	va_list;
#ifndef va_start
#	define va_start(v,l)		__builtin_va_start(v,l)
#endif

#ifndef va_end
#	define va_end(v)			__builtin_va_end(v)
#endif

#ifndef va_arg
#	define va_arg(v,l)			__builtin_va_arg(v,l)
#endif

#if !defined(__STRICT_ANSI__) || __STDC_VERSION__ + 0 >= 199900L || defined(__GXX_EXPERIMENTAL_CXX0X__)

#	ifndef va_copy
#		define va_copy(d,s)		__builtin_va_copy(d,s)
#	endif

#endif

#ifndef __va_copy
#	define __va_copy(d,s)		__builtin_va_copy(d,s)
#endif

typedef __builtin_va_list	__gnuc_va_list;
typedef __gnuc_va_list		va_list;
typedef va_list				__va_list;

//wchar_t* wcsncpy(wchar_t * __restrict dst, const wchar_t * __restrict src, size_t n);

#endif // ANDROID

//------------------------------------------------------
// String hash
//------------------------------------------------------

template<int idx, std::size_t N>
struct StringToHashHelper {
	static constexpr int compute(const char(&str)[N], int hash) {
		const int v1 = hash >> 19;
		const int v0 = hash << 5;
		const int chr = str[N - idx - 1];
		hash = ((v0 | v1) + chr) & StringHashMask;
		return StringToHashHelper<idx - 1, N>::compute(str, hash);
	}
};

template<std::size_t N>
struct StringToHashHelper<0, N> {
	static constexpr int compute(const char(&)[N], int hash) {
		return hash;
	}
};

template <auto V> static constexpr auto force_consteval = V;
#define _StringToHashConst(x) StringToHashHelper<sizeof(x) - 1, sizeof(x)>::compute(x, sizeof(x) - 1)
#define StringToHashConst(x) force_consteval<_StringToHashConst(x)>

// generates string hash
int			StringToHash(const char* str, bool caseIns = false);

//------------------------------------------------------
// Path utils
//------------------------------------------------------

// combines paths
void		CombinePathN(EqString& outPath, int num, ...);

template<typename ...Args> // requires std::same_as<Args, const char*>...
void		CombinePath(EqString& outPath, const Args&... args)
{
	CombinePathN(outPath, sizeof...(Args), args...);
}

// fixes slashes in the directory name
void		FixSlashes( char* str );

//------------------------------------------------------
// General string utilities
//------------------------------------------------------

// Split string by multiple separators
void		xstrsplit2(const char* pString, const char** pSeparators, int nSeparators, Array<EqString>& outStrings);

// Split string by one separator
void		xstrsplit(const char* pString, const char* pSeparator, Array<EqString>& outStrings);

char const* xstristr(char const* pStr, char const* pSearch);
char*		xstristr(char* pStr, char const* pSearch);

// is space?
bool		xisspace(int c);

// fast duplicate c string
char*		xstrdup(const char*  s);

// converts string to lower case
char*		xstrupr(char* s1);
char*		xstrlwr(char* s1);

wchar_t*	xwcslwr(wchar_t* str);
wchar_t*	xwcsupr(wchar_t* str);

//------------------------------------------------------
// wide string
//------------------------------------------------------

// Compares string
int			xwcscmp ( const wchar_t *s1, const wchar_t *s2);

// compares two strings case-insensetive
int			xwcsicmp( const wchar_t* s1, const wchar_t* s2 );

// finds substring in string case insensetive
wchar_t*	xwcsistr( wchar_t* pStr, wchar_t const* pSearch );

// finds substring in string case insensetive
wchar_t const* xwcsistr( wchar_t const* pStr, wchar_t const* pSearch );

//------------------------------------------------------
// string encoding conversion
//------------------------------------------------------

namespace EqStringConv
{
	class utf8_to_wchar
	{
	public:
		utf8_to_wchar(EqWString& outStr, const char* val, int length = -1);
		utf8_to_wchar(wchar_t* outStr, int maxLength, const char* val, int length = -1);
	private:
		uint32 NextByte()
		{
			if (!(*m_utf8))
				return 0;

			return *m_utf8++;
		}

		int		GetLength();
		uint32	GetChar();

		ubyte* m_utf8;
	};

	class wchar_to_utf8
	{
	public:
		wchar_to_utf8(EqString& outStr, const wchar_t* val, int length = -1);
	};
};
