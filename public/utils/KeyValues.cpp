//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Recursive Key-Values system for configuration and other files
//				Implements JSON-like, but lightweight notation
//////////////////////////////////////////////////////////////////////////////////

// Core interface helper
#include <stdarg.h>
#include <stdio.h>

#ifdef LINUX
#include <ctype.h>
#else
#include <malloc.h>
#endif

#include "core/platform/stackalloc.h"
#include "core/DebugInterface.h"
#include "core/IFileSystem.h"

#include "KeyValues.h"

#include "ds/VirtualStream.h"
#include "utils/strtools.h"

#ifdef _MSC_VER
#pragma warning(disable: 4267)
#endif

static const char* s_szkKVValueTypes[KVPAIR_TYPES] =
{
	"string",
	"int",
	"float",
	"bool",
	"section",		// sections
};


static EKVPairType KV_ResolvePairType( const char* string )
{
	char* typeName = (char*)string;

	// check types
	for(int i = 0; i < KVPAIR_TYPES; i++)
	{
		if(!stricmp(typeName, s_szkKVValueTypes[i]))
			return (EKVPairType)i;
	}

	MsgError("invalid kvpair type '%s'\n", typeName);
	return KVPAIR_STRING;
}

static char* KV_ReadProcessString( const char* pszStr )
{
	const char* ptr = pszStr;
	// convert some symbols to special ones

	size_t sLen = strlen( pszStr );

	char* temp = (char*)PPAlloc( sLen+10 ); // FIXME: extra symbols needs to be counted!!!
	char* ptrTemp = temp;

	do
	{
		if(*ptr == '\\')
		{
			if(*(ptr+1) == '"')
			{
				*ptrTemp++ = '"';
				ptr++;
			}
			else if(*(ptr+1) == 'n')
			{
				*ptrTemp++ = '\n';
				ptr++;
			}
			else if(*(ptr+1) == 't')
			{
				*ptrTemp++ = '\t';
				ptr++;
			}
			else
				*ptrTemp++ = *ptr;
		}
		else
			*ptrTemp++ = *ptr;

	}while(*ptr++);

	// add NULL
	*ptrTemp++ = 0;

	// copy string
	return temp;
}

// section beginning / ending
#define KV_SECTION_BEGIN			'{'
#define KV_SECTION_END				'}'

#define KV_ARRAY_BEGIN				'['
#define KV_ARRAY_END				']'
#define KV_ARRAY_SEPARATOR			','

// string symbols
#define KV_STRING_BEGIN_END			'\"'
#define KV_STRING_NEWLINE			'\n'
#define KV_STRING_CARRIAGERETURN	'\r'

// commnent symbols
#define KV_COMMENT_SYMBOL			'/'
#define KV_RANGECOMMENT_BEGIN_END	'*'

#define KV_TYPE_VALUESYMBOL			':'
#define KV_BREAK					';'

#define KV_IDENT_BINARY				MCHAR4('B','K','V','S')

#define IsKVBufferEOF()				((pData - pszBuffer) > bufferSize-1)
#define IsKVArrayEndOrSeparator(c)	(c == KV_ARRAY_SEPARATOR || c == KV_ARRAY_END)
#define IsKVWhitespace(c)			(isspace(c) || c == '\0' || c == KV_STRING_NEWLINE || c == KV_STRING_CARRIAGERETURN)

//-----------------------------------------------------------------------------------------

enum EQuoteMode
{
	QM_NONE = 0,

	QM_COMMENT_LINE,
	QM_COMMENT_RANGE,

	QM_STRING,
	QM_STRING_QUOTED,

	QM_SECTION,			// section or array
};

enum EParserMode
{
	PM_NONE = 0,
	PM_KEY,
	PM_VALUE,
	PM_ARRAY,
	PM_SECTION,
};

//------------------------------------------------------

KVPairValue::~KVPairValue()
{
	PPFree(value);
	delete section;
}

void KVPairValue::SetFrom(KVPairValue* from)
{
	ASSERT(from != NULL);

	type = from->type;

	SetStringValue( from->value );

	if(type == KVPAIR_INT)
	{
		nValue = from->nValue;
	}
	else if(type == KVPAIR_FLOAT)
	{
		fValue = from->fValue;
	}
	else if(type == KVPAIR_BOOL)
	{
		bValue = from->bValue;
	}
	else if(type == KVPAIR_SECTION)
	{
		#pragma todo("clone the section")
	}
}

// sets string value
void KVPairValue::SetStringValue( const char* pszValue, int len /* = -1*/)
{
	if(value)
	{
		PPFree(value);
		value = NULL;
	}

	if (len < 0)
		len = strlen(pszValue);

	value = (char*)PPAlloc(len+1);
	strncpy(value, pszValue, len);
	value[len] = 0;
}

void KVPairValue::SetFromString( const char* pszValue )
{
	ASSERT(pszValue != NULL);

	SetStringValue( pszValue );

	delete section;
	section = NULL;

	if(type == KVPAIR_INT)
	{
		nValue = atoi( pszValue );
	}
	else if(type == KVPAIR_FLOAT)
	{
		fValue = (float)atof( pszValue );
	}
	else if(type == KVPAIR_BOOL)
	{
		bValue = atoi( pszValue ) > 0;
	}
}

void KVPairValue::SetString(const char* value)
{
	SetStringValue(value);

	// convert'n'set
	if(type == KVPAIR_INT)
		nValue = 0;
	else if(type == KVPAIR_FLOAT)
		fValue = 0;
	else if(type == KVPAIR_BOOL)
		bValue = 0;
}

void KVPairValue::SetInt(int value)
{
	char tempbuf[64];

	// copy value string
	_snprintf(tempbuf, 64, "%d", value);

	SetStringValue(tempbuf);

	// convert'n'set
	if (type == KVPAIR_INT)
		nValue = (int)value;
	else if (type == KVPAIR_FLOAT)
		fValue = value;
	else if (type == KVPAIR_BOOL)
		bValue = value > 0;
}

void KVPairValue::SetFloat(float value)
{
	char tempbuf[64];

	// copy value string
	_snprintf(tempbuf, 64, "%g", value);

	SetStringValue(tempbuf);

	// convert'n'set
	if (type == KVPAIR_INT)
		nValue = (int)value;
	else if (type == KVPAIR_FLOAT)
		fValue = value;
	else if (type == KVPAIR_BOOL)
		bValue = value > 0;
}

void KVPairValue::SetBool(bool value)
{
	char tempbuf[64];

	// copy value string
	_snprintf(tempbuf, 64, "%d", value ? 1 : 0);

	SetStringValue(tempbuf);

	// convert'n'set
	if (type == KVPAIR_INT)
		nValue = value ? 1 : 0;
	else if (type == KVPAIR_FLOAT)
		fValue = value ? 1.0f : 0.0f;
	else if (type == KVPAIR_BOOL)
		bValue = value;
}

const char* KVPairValue::GetString() const
{
	return value;
}

int KVPairValue::GetInt() const
{
	if (type == KVPAIR_INT)
		return nValue;
	else if (type == KVPAIR_FLOAT)
		return fValue;
	else if (type == KVPAIR_BOOL)
		return bValue ? 1 : 0;

	return atoi(value);
}

float KVPairValue::GetFloat() const
{
	if (type == KVPAIR_FLOAT)
		return fValue;
	else if (type == KVPAIR_INT)
		return nValue;
	else if (type == KVPAIR_BOOL)
		return bValue ? 1 : 0;

	return atof(value);
}

bool KVPairValue::GetBool() const
{
	if (type == KVPAIR_BOOL)
		return bValue;
	else if (type == KVPAIR_FLOAT)
		return fValue > 0.0f;

	return atoi(value) > 0;
}

KVSection* KVSection::operator[](const char* pszName)
{
	return FindSection(pszName); 
}

KVPairValue* KVSection::operator[](int index)
{
	return values[index]; 
}



//-----------------------------------------------------------------------------------------

KeyValues::KeyValues()
{
}

KeyValues::~KeyValues()
{
}

void KeyValues::Reset()
{
	m_root.Cleanup();
}

// searches for keybase
KVSection* KeyValues::FindSection(const char* pszName, int nFlags)
{
	return m_root.FindSection(pszName, nFlags);
}

// loads from file
bool KeyValues::LoadFromFile(const char* pszFileName, int nSearchFlags)
{
	return KV_LoadFromFile(pszFileName, nSearchFlags, &m_root) != NULL;
}

bool KeyValues::LoadFromStream(ubyte* pData)
{
	return KV_ParseSection( (const char*)pData, 0, NULL, &m_root, 0 ) != NULL;
}

bool KeyValues::SaveToFile(const char* pszFileName, int nSearchFlags)
{
	IFile* pStream = g_fileSystem->Open(pszFileName, "wt", nSearchFlags);

	if(pStream)
	{
		KV_WriteToStream(pStream, &m_root, 0, true);
		g_fileSystem->Close(pStream);
	}
	else
	{
		MsgError("Cannot save keyvalues to file '%s'!\n", pszFileName);
		return false;
	}
	return true;
}

KVSection* KeyValues::GetRootSection() 
{
	return &m_root; 
}

KVSection* KeyValues::operator[](const char* pszName)
{
	return FindSection(pszName);
}

//----------------------------------------------------------------------------------------------
// KEY (PAIR) BASE
//----------------------------------------------------------------------------------------------

KVSection::KVSection()
{
	line = 0;
	unicode = false;
	type = KVPAIR_STRING,
	strcpy(name, "unnamed");
}

KVSection::~KVSection()
{
	Cleanup();
}

void KVSection::Cleanup()
{
	ClearValues();

	for(int i = 0; i < keys.numElem(); i++)
		delete keys[i];

	keys.clear();
}

void KVSection::ClearValues()
{
	for(int i = 0; i < values.numElem(); i++)
		delete values[i];

	values.clear();
}

// sets keybase name
void KVSection::SetName(const char* pszName)
{
	strncpy( name, pszName, sizeof(name));
	name[sizeof(name) - 1] = 0;

	nameHash = StringToHash(name, true);
}

const char*	KVSection::GetName() const
{
	return name;
}

//-----------------------------------------------------------------------------------------

KVPairValue* KVSection::CreateValue()
{
	KVPairValue* val = PPNew KVPairValue();

	val->type = type;

	values.append(val);

	return val;
}

KVSection* KVSection::CreateSectionValue()
{
	if(type != KVPAIR_SECTION)
		return NULL;

	KVPairValue* val = PPNew KVPairValue();

	val->type = type;

	values.append(val);

	val->section = PPNew KVSection();
	return val->section;
}

KVSection* KVSection::Clone() const
{
	KVSection* newKey = PPNew KVSection();

	CopyTo(newKey);

	return newKey;
}

void KVSection::CopyTo(KVSection* dest) const
{
	CopyValuesTo(dest);

	for(int i = 0; i < keys.numElem(); i++)
		dest->AddKey(keys[i]->GetName(), keys[i]->Clone());
}

void KVSection::CopyValuesTo(KVSection* dest) const
{
	dest->ClearValues();

	for(int i = 0; i < values.numElem(); i++)
		dest->AddValue(values[i]);
}

void KVSection::SetValueFrom(KVSection* pOther)
{
	this->Cleanup();
	pOther->CopyTo(this);
}

// adds value to key
void KVSection::AddValue(const char* value)
{
	KVPairValue* val = CreateValue();
	val->SetString(value);
}

void KVSection::AddValue(int nValue)
{
	KVPairValue* val = CreateValue();
	val->SetInt(nValue);
}

void KVSection::AddValue(float fValue)
{
	KVPairValue* val = CreateValue();
	val->SetFloat(fValue);
}

void KVSection::AddValue(bool bValue)
{
	KVPairValue* val = CreateValue();
	val->SetBool(bValue);
}

void KVSection::AddValue(const Vector2D& vecValue)
{
	AddValue(vecValue.x);
	AddValue(vecValue.y);
}

void KVSection::AddValue(const Vector3D& vecValue)
{
	AddValue(vecValue.x);
	AddValue(vecValue.y);
	AddValue(vecValue.z);
}

void KVSection::AddValue(const Vector4D& vecValue)
{
	AddValue(vecValue.x);
	AddValue(vecValue.y);
	AddValue(vecValue.z);
	AddValue(vecValue.w);
}

void KVSection::AddValue(KVSection* keybase)
{
	int numVal = values.numElem();

	KVPairValue* val = CreateValue();

	val->section = keybase;
	val->section->SetName(EqString::Format("%d", numVal).ToCString());
}

void KVSection::AddValue(KVPairValue* value)
{
	KVPairValue* val = CreateValue();
	val->SetFrom(value);
}

//-------------------------

// adds value to key
void KVSection::AddUniqueValue(const char* value)
{
	for(int i = 0; i < values.numElem(); i++)
	{
		if( !strcmp(KV_GetValueString(this, i, ""), value) )
			return;
	}

	CreateValue();

	SetValue(value, values.numElem()-1);
}

void KVSection::AddUniqueValue(int nValue)
{
	for(int i = 0; i < values.numElem(); i++)
	{
		if(KV_GetValueInt(this, i, 0) == nValue)
			return;
	}

	CreateValue();

	SetValue(nValue, values.numElem()-1);
}

void KVSection::AddUniqueValue(float fValue)
{
	for(int i = 0; i < values.numElem(); i++)
	{
		if(KV_GetValueFloat(this, i, 0.0f) == fValue)
			return;
	}

	CreateValue();

	SetValue(fValue, values.numElem()-1);
}

void KVSection::AddUniqueValue(bool bValue)
{
	for(int i = 0; i < values.numElem(); i++)
	{
		if(KV_GetValueBool(this, i, false) == bValue)
			return;
	}

	CreateValue();

	SetValue(bValue, values.numElem()-1);
}

//-----------------------------------------------------------------------------
// sets value
void KVSection::SetValue(const char* value, int idxAt)
{
	if(values.numElem() == 0)
		CreateValue();

	if(!values.inRange(idxAt))
		return;

	values[idxAt]->SetString(value);
}

void KVSection::SetValue(int nValue, int idxAt)
{
	if(values.numElem() == 0)
		CreateValue();

	if(!values.inRange(idxAt))
		return;

	values[idxAt]->SetInt(nValue);
}

void KVSection::SetValue(float fValue, int idxAt)
{
	if(values.numElem() == 0)
		CreateValue();

	if(!values.inRange(idxAt))
		return;

	values[idxAt]->SetFloat(fValue);
}

void KVSection::SetValue(bool bValue, int idxAt)
{
	if(values.numElem() == 0)
		CreateValue();

	if(!values.inRange(idxAt))
		return;

	values[idxAt]->SetBool(bValue);
}

void KVSection::SetValue(const Vector2D& value, int idxAt)
{
	SetValue(value.x, idxAt++);
	SetValue(value.y, idxAt++);
}

void KVSection::SetValue(const Vector3D& value, int idxAt)
{
	SetValue(value.x, idxAt++);
	SetValue(value.y, idxAt++);
	SetValue(value.z, idxAt++);
}

void KVSection::SetValue(const Vector4D& value, int idxAt)
{
	SetValue(value.x, idxAt++);
	SetValue(value.y, idxAt++);
	SetValue(value.z, idxAt++);
	SetValue(value.w, idxAt++);
}

void KVSection::SetValue(KVPairValue* value, int idxAt)
{
	if(values.numElem() == 0)
		CreateValue();

	if(!values.inRange(idxAt))
		return;

	values[idxAt]->SetFrom(value);
}

KVSection& KVSection::SetKey(const char* name, const char* value)
{
	KVSection* pPair = (KVSection*)FindSection( name );
	if(!pPair)
		return AddKey(name, value);

	pPair->ClearValues();
	pPair->type = KVPAIR_STRING;
	pPair->AddValue(value);

	return *this;
}

KVSection& KVSection::SetKey(const char* name, int nValue)
{
	KVSection* pPair = (KVSection*)FindSection( name );
	if(!pPair)
		return AddKey(name, nValue);

	// value setup
	pPair->ClearValues();
	pPair->type = KVPAIR_INT;
	pPair->AddValue(nValue);

	return *this;
}

KVSection& KVSection::SetKey(const char* name, float fValue)
{
	KVSection* pPair = (KVSection*)FindSection( name );
	if(!pPair)
		return AddKey(name, fValue);

	// value setup
	pPair->ClearValues();
	pPair->type = KVPAIR_FLOAT;
	pPair->AddValue(fValue);

	return *this;
}

KVSection& KVSection::SetKey(const char* name, bool bValue)
{
	KVSection* pPair = (KVSection*)FindSection( name );
	if(!pPair)
		return AddKey(name, bValue);

	// value setup
	pPair->ClearValues();
	pPair->type = KVPAIR_BOOL;
	pPair->AddValue(bValue);

	return *this;
}

KVSection& KVSection::SetKey(const char* name, const Vector2D& value)
{
	KVSection* pPair = (KVSection*)FindSection(name);
	if (!pPair)
		return AddKey(name, value);

	// value setup
	pPair->ClearValues();
	pPair->type = KVPAIR_FLOAT;
	pPair->AddValue(value);

	return *this;
}

KVSection& KVSection::SetKey(const char* name, const Vector3D& value)
{
	KVSection* pPair = (KVSection*)FindSection(name);
	if (!pPair)
		return AddKey(name, value);

	// value setup
	pPair->ClearValues();
	pPair->type = KVPAIR_FLOAT;
	pPair->AddValue(value);

	return *this;
}

KVSection& KVSection::SetKey(const char* name, const Vector4D& value)
{
	KVSection* pPair = (KVSection*)FindSection(name);
	if (!pPair)
		return AddKey(name, value);

	// value setup
	pPair->ClearValues();
	pPair->type = KVPAIR_FLOAT;
	pPair->AddValue(value);

	return *this;
}

KVSection& KVSection::SetKey(const char* name, KVSection* pair)
{
	if(!pair)
		return *this;

	KVSection* pPair = (KVSection*)FindSection( name );
	if(!pPair)
		return AddKey(name, pair);

	pPair->Cleanup();

	pair->CopyTo(pPair);

	return *this;
}

KVSection& KVSection::AddKey(const char* name, const char* value)
{
	KVSection* pPair = CreateSection(name, NULL, KVPAIR_STRING);

	pPair->AddValue(value);

	return *this;
}

KVSection& KVSection::AddKey(const char* name, int nValue)
{
	KVSection* pPair = CreateSection(name, NULL, KVPAIR_INT);
	pPair->AddValue(nValue);

	return *this;
};

KVSection& KVSection::AddKey(const char* name, float fValue)
{
	KVSection* pPair = CreateSection(name, NULL, KVPAIR_FLOAT);
	pPair->AddValue(fValue);

	return *this;
}

KVSection& KVSection::AddKey(const char* name, bool bValue)
{
	KVSection* pPair = CreateSection(name, NULL, KVPAIR_BOOL);
	pPair->AddValue(bValue);

	return *this;
}

KVSection& KVSection::AddKey(const char* name, const Vector2D& vecValue)
{
	KVSection* pPair = CreateSection(name, NULL, KVPAIR_FLOAT);
	pPair->AddValue(vecValue);

	return *this;
}

KVSection& KVSection::AddKey(const char* name, const Vector3D& vecValue)
{
	KVSection* pPair = CreateSection(name, NULL, KVPAIR_FLOAT);
	pPair->AddValue(vecValue);

	return *this;
}

KVSection& KVSection::AddKey(const char* name, const Vector4D& vecValue)
{
	KVSection* pPair = CreateSection(name, NULL, KVPAIR_FLOAT);
	pPair->AddValue(vecValue);

	return *this;
}

KVSection& KVSection::AddKey(const char* name, KVSection* pair)
{
	if(!pair)
		return *this;

	KVSection* newPair = CreateSection(name, NULL, KVPAIR_STRING);
	pair->CopyTo(newPair);

	return *this;
}

//-------------------------------------------------------------------------------

// searches for keybase
KVSection* KVSection::FindSection(const char* pszName, int nFlags) const
{
	const int hash = StringToHash(pszName, true);

	for(int i = 0; i < keys.numElem(); i++)
	{
		if((nFlags & KV_FLAG_SECTION) && keys[i]->keys.numElem() == 0)
			continue;

		if((nFlags & KV_FLAG_NOVALUE) && keys[i]->values.numElem() > 0)
			continue;

		if((nFlags & KV_FLAG_ARRAY) && keys[i]->values.numElem() <= 1)
			continue;

		if(keys[i]->nameHash == hash)
		//if(!stricmp(keys[i]->name, pszName))
			return keys[i];
	}

	return NULL;
}

// adds new keybase
KVSection* KVSection::CreateSection( const char* pszName, const char* pszValue, EKVPairType pairType)
{
	KVSection* pKeyBase = PPNew KVSection;
	pKeyBase->SetName(pszName);
	pKeyBase->type = pairType;

	keys.append( pKeyBase );

	if(pszValue != NULL)
	{
		pKeyBase->AddValue(pszValue);
	}

	return pKeyBase;
}


// adds existing keybase. You should set it's name manually. It should not be allocated by other keybase
void KVSection::AddSection(KVSection* keyBase)
{
	if(keyBase != nullptr)
		keys.append( keyBase );
}

// removes key base by name
void KVSection::RemoveSectionByName( const char* name, bool removeAll )
{
	const int strHash = StringToHash(name, true);

	for(int i = 0; i < keys.numElem(); i++)
	{
		if(keys[i]->nameHash == strHash)
		//if(!stricmp(keys[i]->name, name))
		{
			delete keys[i];
			keys.removeIndex(i);

			if(removeAll)
				i--;
			else
				return;
		}
	}
}

void KVSection::RemoveSection(KVSection* base)
{
	for(int i = 0; i < keys.numElem(); i++)
	{
		if(keys[i] == base)
		{
			delete keys[i];
			keys.removeIndex(i);
			return;
		}
	}
}

void KVSection::MergeFrom(const KVSection* base, bool recursive)
{
	if(base == NULL)
		return;

	for(int i = 0; i < base->keys.numElem(); i++)
	{
		KVSection* src = base->keys[i];

		KVSection* dst = CreateSection(src->name);

		src->CopyValuesTo(dst);

		// go to next in hierarchy
		if(recursive)
			dst->MergeFrom(src, recursive);
	}
}

// checkers
bool KVSection::IsSection() const
{
	return keys.numElem() > 0;
}

bool KVSection::IsArray() const
{
	return values.numElem() > 1;
}

bool KVSection::IsDefinition() const
{
	return values.numElem() == 0;
}

// counters and accessors
int	KVSection::KeyCount() const
{
	return keys.numElem();
}

KVSection* KVSection::KeyAt(int idx) const
{
	return keys[idx];
}

int	KVSection::ValueCount() const
{
	return values.numElem();
}

KVPairValue* KVSection::ValueAt(int idx)  const
{
	return values[idx];
}

void KVSection::SetType(int newType)
{
	type = (EKVPairType)newType;
}

int	KVSection::GetType() const
{
	return type;
}

//---------------------------------------------------------------------------------------------------------
// KEYVALUES API Functions
//---------------------------------------------------------------------------------------------------------

#pragma todo("KV_ParseSectionV3 - MBCS isspace issues.")

KVSection* KV_ParseSectionV2(const char* pszBuffer, int bufferSize, const char* pszFileName, KVSection* pParseTo, int nStartLine = 0);
KVSection* KV_ParseSectionV3(const char* pszBuffer, int bufferSize, const char* pszFileName, KVSection* pParseTo, int nStartLine = 0);

KVSection* KV_ParseSection(const char* pszBuffer, int bufferSize, const char* pszFileName, KVSection* pParseTo, int nStartLine)
{
	if (bufferSize <= 0)
		bufferSize = strlen(pszBuffer);
	
	int version = 2;
	KVSection* result = nullptr;
	if(pszBuffer[0] == '$')
	{
		pszBuffer++;
		const char* lineStart = pszBuffer;
		while(!IsKVWhitespace(*pszBuffer))
		{
			pszBuffer++;
		}

		if(!strncmp("schema_kv3", lineStart, pszBuffer - lineStart))
		{
			version = 3;
		}
	}

	if(version == 3)
		result = KV_ParseSectionV3(pszBuffer, bufferSize, pszFileName, pParseTo, nStartLine);
	else
		result = KV_ParseSectionV2(pszBuffer, bufferSize, pszFileName, pParseTo, nStartLine);

	return result;
}

//
// Parses the KeyValues section string buffer to the 'pParseTo'
//
KVSection* KV_ParseSectionV2(const char* pszBuffer, int bufferSize, const char* pszFileName, KVSection* pParseTo, int nStartLine)
{
	enum CommentType
	{
		NOCOMMENT,
		LINECOMMENT,
		RANGECOMMENT,
	};

	
	const char* pData = (char*)pszBuffer;
	char c;

	// set the first character of data
	c = *pData;

	const char *pFirstLetter = NULL;
	const char*	pLast = pData;

	int			nSectionLetterLine = 0;
	int			nQuoteLetterLine = 0;

	bool		bInQuotes = false;
	bool		bInSection = false;

	// Skip for sections
	int nSectionRecursionSkip = 0;

	KVSection* pKeyBase = pParseTo;

	if(!pKeyBase)
		pKeyBase = PPNew KVSection;

	KVSection* pCurrentKeyBase = NULL;
	KVSection* pPrevKeyBase = NULL;

	int bCommentaryMode = NOCOMMENT;

	int nValueCounter = 0;

	int nLine = nStartLine;

	EqString tempName;

	for ( ; (pData - pszBuffer) <= bufferSize; ++pData )
	{
		c = *pData;

		if(c == '\0')
			break;

		pLast = pData;

		if(c == KV_STRING_NEWLINE)
			nLine++;

		// check commentary mode
		if ( c == KV_COMMENT_SYMBOL && !bInQuotes )
		{
			char commentend = *(pData+1);

			// we got comment symbol again
			if( commentend == KV_COMMENT_SYMBOL && bCommentaryMode != RANGECOMMENT )
			{
				bCommentaryMode = LINECOMMENT;
				continue;
			}
			else if( commentend == KV_RANGECOMMENT_BEGIN_END && bCommentaryMode != LINECOMMENT )
			{
				bCommentaryMode = RANGECOMMENT;
				continue;
			}
		}

		// Stop cpp commentary mode after newline
		if ( c == KV_STRING_NEWLINE && bCommentaryMode == 1 )
		{
			bCommentaryMode = NOCOMMENT;
			continue;
		}

		if ( c == KV_RANGECOMMENT_BEGIN_END && bCommentaryMode == 2 )
		{
			char commentend = *(pData+1);
			if(commentend == KV_COMMENT_SYMBOL)
			{
				bCommentaryMode = NOCOMMENT;
				pData++; // little hack
				continue;
			}
		}

		// skip commented text
		if(bCommentaryMode)
			continue;

		// TODO: replace/skip special characters in here

		// if we found section opening character and there is a key base
		if( c == KV_SECTION_BEGIN && !bInQuotes)
		{
			// keybase must be created
			if(!pCurrentKeyBase)
			{
				ASSERT_FAIL("'%s' (%d): section has no keybase\n", pszFileName ? pszFileName : "buffer", nLine+1);

				if(pParseTo != pKeyBase)
					delete pKeyBase;
				return NULL;
			}

			// Do skip only if we have in another section
			if(!pFirstLetter && (nSectionRecursionSkip == 0))
			{
				bInSection = true;
				pFirstLetter = pData + 1;
				nSectionLetterLine = nLine;
			}

			// Up recursion by one
			nSectionRecursionSkip++;
			continue;
		}

		if( pFirstLetter && bInSection ) // if we have parsing section
		{
			if( c == KV_SECTION_END )
			{
				if(nSectionRecursionSkip > 0)
					nSectionRecursionSkip--;

				// if we have reached this section ending, start parsing it's contents
				if(nSectionRecursionSkip == 0)
				{
					int nLen = (int)(pLast - pFirstLetter);

					char* endChar = (char*)pFirstLetter+nLen;

					char oldChr = *endChar;
					*endChar = '\0';

					// recurse
					KVSection* pBase = KV_ParseSectionV2( pFirstLetter, nLen, pszFileName, pCurrentKeyBase, nSectionLetterLine );

					*endChar = oldChr;

					// if it got all killed
					if(!pBase)
					{
						//delete pKeyBase;
						return NULL;
					}

					bInSection = false;
					pFirstLetter = NULL;

					// NOTE: we could emit code below to not use KV_BREAK character strictly after section
					pCurrentKeyBase = NULL;
					nValueCounter = 0;
				}
			}

			continue; // don't parse the entire section until we got a section ending
		}

		// if not in quotes and found whitespace
		// or if in quotes and there is closing character
		// TODO: check \" inside quotes
		if( pCurrentKeyBase && pFirstLetter &&
			((!bInQuotes && (isspace((ubyte)c) || (c == KV_BREAK))) ||
			(bInQuotes && (c == KV_STRING_BEGIN_END))))
		{
			char prevSymbol = *(pData-1);

			if(bInQuotes && prevSymbol == '\\')
			{
				continue;
			}

			int nLen = (int)(pLast - pFirstLetter);

			// close token
			if(nValueCounter <= 0)
			{
				tempName.Assign( pFirstLetter, nLen );

				pCurrentKeyBase->SetName(tempName.ToCString());
			}
			else
			{
				char* endChar = (char*)pFirstLetter+nLen;

				char oldChr = *endChar;
				*endChar = '\0';

				char* processedValue = KV_ReadProcessString(pFirstLetter);

				pCurrentKeyBase->AddValue(processedValue);

				PPFree(processedValue);

				*endChar = oldChr;
			}

			pFirstLetter = NULL;
			bInQuotes = false;

			if(c == KV_BREAK)
			{
				pPrevKeyBase = pCurrentKeyBase;
				pCurrentKeyBase = NULL;
				nValueCounter = 0;
			}

			continue;
		}

		// end keybase if we got semicolon
		if( !bInQuotes && (c == KV_BREAK) )
		{
			pCurrentKeyBase = NULL;
			nValueCounter = 0;
			continue;
		}

		// start token
		if ( !pFirstLetter && (c != KV_BREAK) &&
			(c != KV_SECTION_BEGIN) && (c != KV_SECTION_END))
		{
			// if we got quote character or this is not a space character
			// begin new token

			if((c == KV_STRING_BEGIN_END) || !isspace(c))
			{
				// create keybase or increment value counter
				if( pCurrentKeyBase )
				{
					nValueCounter++;
				}
				else
				{
					pCurrentKeyBase = PPNew KVSection;
					pCurrentKeyBase->line = nLine;
					pKeyBase->keys.append( pCurrentKeyBase );
				}

				if( c == KV_STRING_BEGIN_END )
					bInQuotes = true;

				nQuoteLetterLine = nLine;

				pFirstLetter = pData + (bInQuotes ? 1 : 0);

				continue;
			}
		}
	}

	if( bInQuotes )
	{
		ASSERT_FAIL("'%s' (%d): unexcepted end of file, you forgot to close quotes\n", pszFileName ? pszFileName : "buffer", nQuoteLetterLine+1);

		if(pParseTo != pKeyBase)
			delete pKeyBase;
		pKeyBase = NULL;
	}

	if( pCurrentKeyBase )
	{
		ASSERT_FAIL("'%s' (%d): EOF passed, excepted ';'\n", pszFileName ? pszFileName : "buffer", pCurrentKeyBase->line+1);
		if(pParseTo != pKeyBase)
			delete pKeyBase;
		pKeyBase = NULL;
	}

	if( bInSection )
	{
		ASSERT_FAIL("'%s' (%d): EOF passed, excepted '}'\n", pszFileName ? pszFileName : "buffer", nSectionLetterLine+1);
		if(pParseTo != pKeyBase)
			delete pKeyBase;
		pKeyBase = NULL;
	}

	if( bCommentaryMode == 2 )
	{
		ASSERT_FAIL("'%s' (%d): EOF passed, excepted '*/', check whole text please\n", pszFileName ? pszFileName : "buffer", nLine+1);
		if(pParseTo != pKeyBase)
			delete pKeyBase;
		pKeyBase = NULL;
	}

	return pKeyBase;
}

//
// Parses the V3 format of KeyValues into pParseTo
//
KVSection* KV_ParseSectionV3( const char* pszBuffer, int bufferSize, const char* pszFileName, KVSection* pParseTo, int nStartLine )
{
	KVSection* pKeyBase = pParseTo;

	if(!pKeyBase)
		pKeyBase = PPNew KVSection;

	const char* pData = (char*)pszBuffer;
	const char* pLast = pData;
	char c = *pData;

	const char* pFirstLetter = NULL;

	EQuoteMode quoteMode = QM_NONE;		// actual quoting mode

	EParserMode parserMode = PM_NONE;	// parser mode for error identification
	EParserMode lastParserMode = parserMode;
	int lastParserModeLine = -1;

	bool valueArray = false;
	int valueArrayStartLine = -1;

	int sectionDepth = 0;

	int nValCounter = 0;

	int nLine = nStartLine+1;
	int nModeStartLine = -1;

	// allocate 129 b in stack
	char* key = (char*)stackalloc(KV_MAX_NAME_LENGTH+1);
	strcpy(key, "unnamed");

	KVSection* curpair = NULL;

	do
	{
		c = *pData;

		pLast = pData;

		if(c == '\n')
			nLine++;

		// skip non-single character white spaces, tabs, carriage returns, and newlines
		if(c == '\\' && (*(pData+1) == 'n' || *(pData+1) == 't' || *(pData+1) == '\"'))
		{
			pData++;
			continue;
		}

		// check for comment mode enabling
		// only when nothing is parsed or unquoted strings
		if ( c == KV_COMMENT_SYMBOL && quoteMode == QM_NONE)
		{
			char commentend = *(pData+1);

			if(commentend == KV_COMMENT_SYMBOL)
			{
				quoteMode = QM_COMMENT_LINE;
				nModeStartLine = nLine;
				continue;
			}
			else if(commentend == KV_RANGECOMMENT_BEGIN_END)
			{
				quoteMode = QM_COMMENT_RANGE;
				nModeStartLine = nLine;
				continue;
			}
		}
		else if ( c == KV_STRING_NEWLINE && quoteMode == QM_COMMENT_LINE ) // check comment mode disabling
		{
			// Stop commentary mode after new line
			quoteMode = QM_NONE;
			lastParserModeLine = nModeStartLine;
			lastParserMode = parserMode;
			parserMode = PM_NONE;
			continue;
		}
		else if ( c == KV_RANGECOMMENT_BEGIN_END && quoteMode == QM_COMMENT_RANGE )
		{
			char commentend = *(pData+1);
			if(commentend == KV_COMMENT_SYMBOL)
			{
				quoteMode = QM_NONE;
				pData++;
				continue;
			}
		}

		// skip chars when in comment mode
		if(quoteMode == QM_COMMENT_RANGE || quoteMode == QM_COMMENT_LINE)
			continue;

		// reading a key/value characters
		if(quoteMode == QM_STRING || quoteMode == QM_STRING_QUOTED)
		{
			// check for reading array

			// read type name
			if((quoteMode == QM_STRING) && (c == KV_TYPE_VALUESYMBOL))
			{
				if( nValCounter == 0 )
				{
					ASSERT_FAIL("'%s':%d error - unexpected type definition\n", pszFileName, nModeStartLine);
					break;
				}

				{
					int nLen = (pLast - pFirstLetter);

					char* endChar = (char*)pFirstLetter+nLen;

					char oldChr = *endChar;
					*endChar = '\0';

					// set the type
					curpair->type = KV_ResolvePairType(pFirstLetter);

					*endChar = oldChr;
				}

				// parse value type and reset
				//pFirstLetter = pData+1;
				quoteMode = QM_NONE;
			}
			else if((quoteMode == QM_STRING_QUOTED && (c == KV_STRING_BEGIN_END)) ||
					(quoteMode == QM_STRING && ( c == KV_COMMENT_SYMBOL || c == KV_BREAK || IsKVBufferEOF() || IsKVWhitespace(c) || (valueArray && IsKVArrayEndOrSeparator(c)))))
			{
				// complete the key and add
				if(!valueArray)
					nValCounter++;
				else if(quoteMode == QM_STRING && IsKVArrayEndOrSeparator(c))
					pData--; // get back by ]

				// force to begin commenting mode on QM_STRING
				if (quoteMode == QM_STRING && (c == KV_COMMENT_SYMBOL))
				{
					char commentStartTest = *(pData + 1);

					// get ready for range comment *ONLY*
					if (commentStartTest == KV_RANGECOMMENT_BEGIN_END)
						pData--;
					else
						continue;
				}

				bool typeIsOk = (curpair->type != KVPAIR_SECTION);

				if(!typeIsOk)
				{
					ASSERT_FAIL("'%s':%d error: type mismatch, expected 'section'\n", pszFileName, nModeStartLine);
					break;
				}

				{
					int nLen = (pLast - pFirstLetter);

					if(nValCounter == 1)
					{
						// set key name
						strncpy(key, pFirstLetter, min(KV_MAX_NAME_LENGTH, nLen));
						key[nLen] = '\0';

						if(c == KV_BREAK)
						{
							ASSERT_FAIL("'%s':%d error - unexpected break\n", pszFileName, nModeStartLine);
							
							break;
						}

					}
					else if(nValCounter == 2)
					{
						char* endChar = (char*)pFirstLetter + nLen;

						char oldChr = *endChar;
						*endChar = '\0';

						// pre-process string
						char* valueString = KV_ReadProcessString(pFirstLetter);
						*endChar = oldChr;

						if(valueArray)
						{
							// make it parsed if the type is different
							KVPairValue* value = curpair->CreateValue();
							value->SetFromString(valueString);
						}
						else
						{
							// set or create a single value
							KVPairValue* value = curpair->values.numElem() ? curpair->values[0] : curpair->CreateValue();
							value->SetFromString(valueString);
						}

						// free processed string
						PPFree(valueString);

						curpair->SetName(key);
						pParseTo->keys.addUnique(curpair);
					}
				}

				if(nValCounter == 2 && !valueArray)
					nValCounter = 0;

				quoteMode = QM_NONE;
				pFirstLetter = NULL;

				lastParserModeLine = nModeStartLine;
				lastParserMode = parserMode;
				parserMode = PM_NONE;
			}
		}
		else if(quoteMode == QM_NONE) // test for begin read keys/sections
		{
			// skip whitespaces
			if(isspace(c))
				continue;

			if(c == KV_BREAK) // skip old-style breaks
				continue;

			// begin section mode
			if( c == KV_SECTION_BEGIN )
			{
				//Msg("start section\n");
				if(curpair == NULL)
				{
					ASSERT_FAIL("'%s':%d error - unexpected anonymous section\n", pszFileName, nModeStartLine);
					break;
				}

				quoteMode = QM_SECTION;
				lastParserModeLine = nModeStartLine;
				lastParserMode = parserMode;
				parserMode = PM_SECTION;

				nModeStartLine = nLine;
				pFirstLetter = pData+1;

				sectionDepth++;
			}
			else if(c == KV_ARRAY_BEGIN) // enable array parsing
			{
				if (nValCounter == 0 || valueArray)
				{
					ASSERT_FAIL("'%s':%d error - unexpected '['\n", pszFileName, nModeStartLine);
					break;
				}

				lastParserModeLine = nModeStartLine;
				lastParserMode = parserMode;
				parserMode = PM_ARRAY;

				valueArray = true;
				valueArrayStartLine = nLine;
				nValCounter++;
				continue;
			}
			else if(IsKVArrayEndOrSeparator(c) && valueArray)
			{
				// add last value after separator or closing

				if(c == KV_ARRAY_END)
				{
					lastParserModeLine = nModeStartLine;
					lastParserMode = parserMode;
					parserMode = PM_NONE;
					valueArray = false;

					// complete the key and add
					nValCounter = 0;
				}

				continue;
			}
			else // any character initiates QM_STRING* mode
			{
				quoteMode = (c == KV_STRING_BEGIN_END) ? QM_STRING_QUOTED:  QM_STRING;

				nModeStartLine = nLine;
				pFirstLetter = (quoteMode == QM_STRING_QUOTED) ? pData + 1 : pData;

				lastParserModeLine = nModeStartLine;
				lastParserMode = parserMode;

				if(nValCounter == 0)
				{
					curpair = PPNew KVSection();
					curpair->line = nModeStartLine;
					parserMode = PM_KEY;
				}
				else
					parserMode = PM_VALUE;
			}
		}
		else if(quoteMode == QM_SECTION) // section skipper and processor
		{
			// skip the next section openning but increment the depths
			if( c == KV_SECTION_BEGIN )
			{
				if(sectionDepth == 0)
					pFirstLetter = pData+1;

				sectionDepth++;
			}
			else if( c == KV_SECTION_END )
			{
				if(sectionDepth > 0)
					sectionDepth--;

				if(sectionDepth == 0)
				{
					// read buffer
					int nLen = (pLast - pFirstLetter);

					if(key[0] == '%')
					{
						// force the pair value type to SECTION
						curpair->type = KVPAIR_STRING;
						
						// make it parsed if the type is different
						KVPairValue* value = curpair->CreateValue();
						value->SetStringValue(pFirstLetter, nLen);

						curpair->SetName(key + 1);
						pParseTo->keys.addUnique(curpair);

						curpair = NULL; // i'ts finally done
						*key = 0;
					}
					else if( valueArray )
					{
						KVSection* newsec = PPNew KVSection();
						bool success = KV_ParseSectionV3(pFirstLetter, nLen, pszFileName, newsec, nModeStartLine-1) != NULL;

						bool typeIsOk = ((curpair->values.numElem() == 0) || curpair->type == KVPAIR_SECTION);

						if(success && typeIsOk)
						{
							// force the pair value type to SECTION
							curpair->type = KVPAIR_SECTION;
							curpair->AddValue(newsec);

							curpair->SetName(key);
							pParseTo->keys.addUnique(curpair);
						}
						else
						{
							delete newsec;

							if(!typeIsOk)
							{
								ASSERT_FAIL("'%s':%d error - type mismatch, expected 'section'\n", pszFileName, nModeStartLine);
								break;
							}
						}
					}
					else
					{
						bool success = KV_ParseSectionV3(pFirstLetter, nLen, pszFileName, curpair, nModeStartLine-1) != NULL;

						if(success)
						{
							curpair->SetName(key);
							pParseTo->keys.addUnique(curpair);

							curpair = NULL; // i'ts finally done
							*key = 0;
						}
					}

					// disable
					nValCounter = 0;
					quoteMode = QM_NONE;

					lastParserModeLine = nModeStartLine;
					lastParserMode = parserMode;
					parserMode = PM_NONE;

					pFirstLetter = NULL;
				} // depth
			} // KV_SECTION_END
		} // QM_SECTION
	}
	while(*pData++ && (pData - pszBuffer) <= bufferSize);

	// check for errors
	bool isError = false;

	// if mode is not none, then is error
	if(quoteMode != QM_NONE)
	{
		if(quoteMode == QM_COMMENT_RANGE)
		{
			ASSERT_FAIL("'%s':%d error - unexpected EOF, did you forgot '*/'?\n", pszFileName, nModeStartLine);
			isError = true;
		}
		else if(quoteMode == QM_SECTION)
		{
			ASSERT_FAIL("'%s':%d error - missing '}'\n", pszFileName, nModeStartLine);
			isError = true;
		}
		else if(quoteMode == QM_STRING_QUOTED)
		{
			ASSERT_FAIL("'%s':%d error - missing '\"'\n", pszFileName, nModeStartLine);
			isError = true;
		}
	}

	if(valueArray)
	{
		ASSERT_FAIL("'%s':%d error - missing ']'\n", pszFileName, valueArrayStartLine);
		isError = true;
	}

	if(isError)
	{
		if(pParseTo != pKeyBase)
			delete pKeyBase;
		return NULL;
	}

	return pKeyBase;
}

//
// Loads file and parses it as KeyValues into the 'pParseTo'
//
KVSection* KV_LoadFromFile( const char* pszFileName, int nSearchFlags, KVSection* pParseTo )
{
	long lSize = 0;
	char* pBuffer = (char*)g_fileSystem->GetFileBuffer(pszFileName, &lSize, nSearchFlags);
	char* _buffer = pBuffer;

	if(!_buffer)
	{
		DevMsg(1, "Can't open key-values file '%s'\n", pszFileName);
		return NULL;
	}

	ushort byteordermark = *((ushort*)_buffer);

	bool isUTF8 = false;
	bool isBinary = false;

	if(byteordermark == 0xbbef)
	{
		// skip this three byte bom
		_buffer += 3;
		lSize -= 3;
		isUTF8 = true;
	}
	else if(byteordermark == 0xfeff)
	{
		ASSERT(!"Only UTF-8 keyvalues supported!!!");
		PPFree( pBuffer );
		return NULL;
	}
	else
	{
		uint ident = *((uint*)_buffer);

		// it's assuming UTF8 when using binary format
		if(ident == KV_IDENT_BINARY)
		{
			isBinary = true;
			isUTF8 = true;
		}
	}

	// load as stream
	KVSection* pBase = NULL;
	
	if(isBinary)
		pBase = KV_ParseBinary(_buffer, lSize, pParseTo);
	else
		pBase = KV_ParseSection(_buffer, lSize, pszFileName, pParseTo, 0);

	if(pBase)
	{
		pBase->SetName(_Es(pszFileName).Path_Strip_Path().ToCString());
        pBase->unicode = isUTF8;
	}

	// required to clean memory after reading
	PPFree( pBuffer );

	return pBase;
}


//-----------------------------------------------------------------------------------------------------
// BINARY

struct kvbinvalue_s
{
	int type;	// EKVPairType

	union
	{
		int		nValue;		// treat as string length if KVPAIR_STRING
		bool	bValue;
		float	fValue;
	};

	// string is written next after the value struct
};

ALIGNED_TYPE(kvbinvalue_s,4) kvbinvalue_t;

struct kvbinbase_s
{
	int		ident;	// it must be identified by KV_IDENT_BINARY

	int		type;	// EKVPairType
	int		keyCount;

	ushort	valueCount;
	ushort	nameLen;

	// next after section header:
	// - name
	// - values
	// - nested key bases
};

KVSection* KV_ParseBinary(const char* pszBuffer, int bufferSize, KVSection* pParseTo)
{
	CMemoryStream memstr((ubyte*)pszBuffer, VS_OPEN_READ, bufferSize);
	return KV_ReadBinaryBase(&memstr, pParseTo);
}

void KV_ReadBinaryValue(IVirtualStream* stream, KVSection* addTo)
{
	kvbinvalue_t binValue;
	stream->Read(&binValue, 1, sizeof(binValue));

	if(binValue.type == KVPAIR_STRING)
	{
		 // int value as string length
		char* strVal = (char*)PPAlloc(binValue.nValue+1);

		stream->Read(strVal, 1, binValue.nValue);
		strVal[binValue.nValue] = '\0';

		addTo->AddValue(strVal);
	}
	else if(binValue.type == KVPAIR_INT)
	{
		addTo->AddValue(binValue.nValue);
	}
	else if(binValue.type == KVPAIR_FLOAT)
	{
		addTo->AddValue(binValue.fValue);
	}
	else if(binValue.type == KVPAIR_BOOL)
	{
		addTo->AddValue(binValue.bValue);
	}
	else if(binValue.type == KVPAIR_SECTION)
	{
		KVSection* parsed = KV_ReadBinaryBase(stream, NULL);

		if(parsed)
			addTo->AddValue(parsed);
	}
}

// reads binary keybase
KVSection* KV_ReadBinaryBase(IVirtualStream* stream, KVSection* pParseTo)
{
	kvbinbase_s binBase;
	stream->Read(&binBase, 1, sizeof(binBase));

	if(binBase.ident != KV_IDENT_BINARY)
	{
		MsgError("KV_ReadBinaryBase - invalid header\n");
		return NULL;
	}

	if(!pParseTo)
		pParseTo = PPNew KVSection();

	// read name after keybase header
	char* nameTemp = (char*)stackalloc(binBase.nameLen+1);
	stream->Read(nameTemp, 1, binBase.nameLen);
	nameTemp[binBase.nameLen] = '\0';

	pParseTo->SetName(nameTemp);
	pParseTo->type = (EKVPairType)binBase.type;

	pParseTo->unicode = true; // always as unicode UTF8
	
	// read values if we have any
	for(int i = 0; i < binBase.valueCount; i++)
	{
		KV_ReadBinaryValue(stream, pParseTo);
	}

	// read nested keybases as well
	for(int i = 0; i < binBase.keyCount; i++)
	{
		KVSection* parsed = KV_ReadBinaryBase(stream, NULL);
		pParseTo->AddSection(parsed);
	}

	return pParseTo;
}

void KV_WriteToStreamBinary(IVirtualStream* outStream, KVSection* base);

// writes KV value to the binary stream
void KV_WriteValueBinary(IVirtualStream* outStream, KVPairValue* value)
{
	kvbinvalue_t binValue;
	binValue.type = value->type;

	if(binValue.type == KVPAIR_STRING)
	{
		binValue.nValue = strlen(value->value); // store string length
	}
	else if(binValue.type == KVPAIR_INT)
	{
		binValue.nValue = value->nValue;
	}
	else if(binValue.type == KVPAIR_FLOAT)
	{
		binValue.fValue = value->fValue;
	}
	else if(binValue.type == KVPAIR_BOOL)
	{
		binValue.bValue = value->bValue;
	}

	// store header
	outStream->Write(&binValue, 1, sizeof(binValue));
	
	if(binValue.type == KVPAIR_STRING)
	{
		// store the string
		outStream->Write(value->value, 1, binValue.nValue);
	}
	else if(binValue.type == KVPAIR_SECTION)
	{
		// store section after the binary
		KV_WriteToStreamBinary(outStream, value->section);
	}
}

// writes keybase to the binary stream
void KV_WriteToStreamBinary(IVirtualStream* outStream, KVSection* base)
{
	kvbinbase_s binBase;
	memset(&binBase, 0, sizeof(binBase));

	binBase.ident = KV_IDENT_BINARY;
	binBase.keyCount = base->keys.numElem();
	binBase.valueCount = base->values.numElem();
	binBase.type = base->type;
	binBase.nameLen = strlen(base->name);

	// write header to the stream
	outStream->Write(&binBase, 1, sizeof(binBase));

	// write base name
	outStream->Write(base->name, 1, binBase.nameLen);

	// write each value of key base
	for(int i = 0; i < binBase.valueCount; i++)
	{
		KV_WriteValueBinary(outStream, base->values[i]);
	}

	// then write each subkey of this key recursively... pretty simple, huh?
	for(int i = 0; i < binBase.keyCount; i++)
	{
		KV_WriteToStreamBinary(outStream, base->keys[i]);
	}
}

//----------------------------------------------------------------------------------------------

//
// Detects the need for string quoting (very necessary for keys)
//
bool UTIL_StringNeedsQuotes( const char* pszString )
{
	char const* pLetter = pszString;

	int length = 0;

	// Check the entire string
	while (*pLetter != 0)
	{
		// break on spaces, but allow newline characters
		// in key/values
		if(isspace(*pLetter) && *pLetter != '\n')
			return true;

		length++;

		// commentary also must be quoted
		if(*pLetter == KV_COMMENT_SYMBOL)
		{
			if(*(pLetter+1) == KV_RANGECOMMENT_BEGIN_END || *(pLetter+1) == KV_COMMENT_SYMBOL)
				return true;
		}
		else if(*pLetter == KV_RANGECOMMENT_BEGIN_END)
		{
			if(*(pLetter+1) == KV_COMMENT_SYMBOL)
				return true;
		}

		pLetter++;
	}

	// always indicate empty strings
	if(length == 0)
		return true;

	return false;
}

//
// If string does need quotes it will be written with them
//
static void KV_WriteSelectQuotedString(IVirtualStream* out, const char* pszString)
{
	if( UTIL_StringNeedsQuotes( pszString ) )
		out->Print("\"%s\"", pszString);
	else
		out->Print("%s", pszString);
}

//
// counts the special characters
// used for KV_PreProcessStringValue to detect extra length of buffer
//
static int KV_CountSpecialSymbols(char* pszStr)
{
	char* ptr = pszStr;

	int specials_count = 0;

	do
	{
		if(*ptr == '"')
		{
			specials_count++;
		}
		else if(*ptr == '\n')
		{
			specials_count++;
		}
		else if(*ptr == '\t')
		{
			specials_count++;
		}
	}while(*ptr++);

	return specials_count + 1;
}

//
// converts some symbols to special ones
//
static void KV_PreProcessStringValue( char* out, char* pszStr )
{
	char* ptr = pszStr;
	char* temp = out;

	do
	{
		if(*ptr == '"')
		{
			*temp++ = '\\';
			*temp++ = *ptr;
		}
		else if(*ptr == '\n')
		{
			*temp++ = '\\';
			*temp++ = 'n';
		}
		else if(*ptr == '\t')
		{
			*temp++ = '\\';
			*temp++ = 't';
		}
		else
			*temp++ = *ptr;

	}while(*ptr++);

	// add NULL
	*temp++ = 0;
}

//-----------------------------------------------------------------------------------------------------

void KV_WriteToStreamV3(IVirtualStream* outStream, KVSection* section, int nTabs, bool pretty);

//
// Writes the pair value
//
void KV_WritePairValue(IVirtualStream* out, KVPairValue* val, int depth)
{
	// write typed data
	if(val->type == KVPAIR_STRING)
	{
		if(val->value == NULL)
		{
			out->Print("\"%s\"", "VALUE_MISSING");
			return;
		}

		int numSpecial = KV_CountSpecialSymbols(val->value);

		char* outValueString = (char*)PPAlloc(strlen(val->value) + numSpecial + 1);
		KV_PreProcessStringValue( outValueString, val->value );
		out->Print("\"%s\"", outValueString);

		PPFree( outValueString );
	}
	else if(val->type == KVPAIR_INT)
	{
		out->Print("%d", val->nValue);
	}
	else if(val->type == KVPAIR_FLOAT)
	{
		out->Print("%g", val->fValue);
	}
	else if(val->type == KVPAIR_BOOL)
	{
		out->Print("%d", val->bValue);
	}
	else if(val->type == KVPAIR_SECTION)
	{
		out->Print("%c", KV_SECTION_BEGIN);
		KV_WriteToStreamV3(out, val->section, depth+1, false);
		out->Print("%c", KV_SECTION_END);
	}
}

//
// Writes the pairbase recursively to the virtual stream
//
void KV_WriteToStream(IVirtualStream* outStream, KVSection* section, int nTabs, bool pretty)
{
	char* tabs = (char*)stackalloc(nTabs);
	memset(tabs, 0, nTabs);

	int nTab = 0;

	if(pretty)
	{
		while(nTab < nTabs)
			tabs[nTab++] = '\t';
	}

	for(int i = 0; i < section->keys.numElem(); i++)
	{
		KVSection* pKey = section->keys[i];

		// add tabs
		if(pretty)
			outStream->Write(tabs, 1, nTabs);

		bool hasWroteValue = false;

		//
		// write key/value
		//
		{
			KV_WriteSelectQuotedString(outStream, pKey->name);

			if(pKey->type != KVPAIR_SECTION)
			{
				for(int j = 0; j < pKey->values.numElem(); j++)
				{
					outStream->Print(" ");
					KV_WritePairValue(outStream, pKey->values[j], nTabs);
				}
				hasWroteValue = true;
			}
			else
				outStream->Print("; // section arrays not supported in V2\n");
		}

		//
		// write subsection
		//
		if( pKey->keys.numElem() )
		{
			if(pretty)
			{
				outStream->Print("\n");
				outStream->Write(tabs, 1, nTabs);
			}
			outStream->Print("%c\n", KV_SECTION_BEGIN);

			KV_WriteToStream(outStream, pKey, nTabs + 1, pretty);

			if(pretty)
				outStream->Write(tabs, 1, nTabs);

			outStream->Print("%c", KV_SECTION_END);
		}

		if(hasWroteValue)
			outStream->Print("%c\n", KV_BREAK);
	}
}

//
// Writes the pairbase values
//
void KV_WriteValueV3( IVirtualStream* outStream, KVSection* key, int nTabs)
{
	int numValues = key->values.numElem();
	bool isValueArray = numValues > 1;

	if(key->type != KVPAIR_STRING && key->type != KVPAIR_SECTION)
		outStream->Print(" %s%c ", s_szkKVValueTypes[key->type], KV_TYPE_VALUESYMBOL);
	else
		outStream->Print(" ");

	if(isValueArray)
		outStream->Print("%c ", KV_ARRAY_BEGIN);

	for(int j = 0; j < numValues; j++)
	{
		KV_WritePairValue(outStream, key->values[j], nTabs);

		// while array sepearator is not required, always add it
		if(j < numValues-1)
			outStream->Print("%c ", KV_ARRAY_SEPARATOR);
	}

	if(isValueArray)
		outStream->Print(" %c", KV_ARRAY_END);
}

//
// Writes the pairbase recursively to the virtual stream
//
void KV_WriteToStreamV3(IVirtualStream* outStream, KVSection* section, int nTabs, bool pretty)
{
	char* tabs = (char*)stackalloc(nTabs);
	memset(tabs, 0, nTabs);

	int nTab = 0;

	if(pretty)
	{
		while(nTab < nTabs)
			tabs[nTab++] = '\t';
	}

	int numKeys = section->keys.numElem();

	for(int i = 0; i < numKeys; i++)
	{
		KVSection* pKey = section->keys[i];

		// add tabs
		if(pretty)
			outStream->Write(tabs, 1, nTabs);

		//
		// write key/value
		//
		{
			KV_WriteSelectQuotedString(outStream, pKey->name);
			KV_WriteValueV3(outStream, pKey, nTabs);
		}

		//
		// write subsection
		//
		if( pKey->IsSection() )
		{
			if(pretty)
			{
				outStream->Print("\n");
				outStream->Write(tabs, 1, nTabs);
			}

			outStream->Print("%c\n", KV_SECTION_BEGIN);

			KV_WriteToStreamV3(outStream, pKey, nTabs + 1, pretty);

			if(pretty)
				outStream->Write(tabs, 1, nTabs);

			outStream->Print("%c", KV_SECTION_END);
		}

		if(pretty)
			outStream->Print("\n");
		else if(i < numKeys-1)
			outStream->Print(" ");
	}
}

//
// Prints the pairbase to console
//
void KV_PrintSection(KVSection* base)
{
	CMemoryStream stream(nullptr, VS_OPEN_WRITE, 2048);
	KV_WriteToStream(&stream, base, 0, true);

	char nullChar = '\0';
	stream.Write(&nullChar, 1, 1);

	Msg( "%s\n", stream.GetBasePointer() );
}

//-----------------------------------------------------------------------------------------------------
// KeyValues value helpers
//-----------------------------------------------------------------------------------------------------

//
// Returns the string value of pairbase
//
const char* KV_GetValueString( KVSection* pBase, int nIndex, const char* pszDefault )
{
	if(!pBase || pBase && !pBase->values.inRange(nIndex))
		return pszDefault;

	return (*pBase)[nIndex]->GetString();
}


//
// Returns integer value
// Converts the best precise value if type differs
//
int	KV_GetValueInt( KVSection* pBase, int nIndex, int nDefault )
{
	if(!pBase || pBase && !pBase->values.inRange(nIndex))
		return nDefault;

	return (*pBase)[nIndex]->GetInt();
}

//
// Returns float value
// Converts the best precise value if type differs
//
float KV_GetValueFloat( KVSection* pBase, int nIndex, float fDefault )
{
	if(!pBase || pBase && !pBase->values.inRange(nIndex))
		return fDefault;

	return (*pBase)[nIndex]->GetFloat();
}

//
// Returns boolean value
// Converts the best precise value if type differs
//
bool KV_GetValueBool( KVSection* pBase, int nIndex, bool bDefault)
{
	if(!pBase || pBase && !pBase->values.inRange(nIndex))
		return bDefault;

	return (*pBase)[nIndex]->GetBool();
}

//
// Returns Vector2D value
//
Vector2D KV_GetVector2D( KVSection* pBase, int nIndex, const Vector2D& vDefault)
{
	Vector2D retVal = vDefault;
	retVal.x = KV_GetValueFloat(pBase, nIndex, vDefault.x);
	retVal.y = KV_GetValueFloat(pBase, nIndex+1, vDefault.y);

	return retVal;
}

//
// Returns IVector2D value
//
IVector2D KV_GetIVector2D( KVSection* pBase, int nIndex, const IVector2D& vDefault)
{
	IVector2D retVal = vDefault;
	retVal.x = KV_GetValueInt(pBase, nIndex, vDefault.x);
	retVal.y = KV_GetValueInt(pBase, nIndex+1, vDefault.y);

	return retVal;
}

//
// Returns Vector3D value
//
Vector3D KV_GetVector3D( KVSection* pBase, int nIndex, const Vector3D& vDefault)
{
	Vector3D retVal = vDefault;
	retVal.x = KV_GetValueFloat(pBase, nIndex, vDefault.x);
	retVal.y = KV_GetValueFloat(pBase, nIndex+1, vDefault.y);
	retVal.z = KV_GetValueFloat(pBase, nIndex+2, vDefault.z);

	return retVal;
}

//
// Returns Vector4D value
//
Vector4D KV_GetVector4D( KVSection* pBase, int nIndex, const Vector4D& vDefault)
{
	Vector4D retVal = vDefault;
	retVal.x = KV_GetValueFloat(pBase, nIndex, vDefault.x);
	retVal.y = KV_GetValueFloat(pBase, nIndex+1, vDefault.y);
	retVal.z = KV_GetValueFloat(pBase, nIndex+2, vDefault.z);
	retVal.w = KV_GetValueFloat(pBase, nIndex+3, vDefault.w);

	return retVal;
}
