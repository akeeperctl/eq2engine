///////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Data package file (dpk)
//////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "dpk/dpk_defs.h"
#include "utils/IceKey.h"

#include "core/platform/OSFile.h"

// TODO: refactor interface to simply operate IVirtualStream buffers
// and directly write into pack file

class CDPKFileWriter
{
public:
				CDPKFileWriter();
				~CDPKFileWriter();

	void		SetCompression( int compression );
	void		SetEncryption( int type, const char* key );
	void		SetMountPath( const char* path );

	bool		AddFile( const char* fileName, int nameHash );
	bool		AddFile( const char* fileName, const char* targetFilename);
	int			AddDirectory(const char* wildcard, const char* targetDir, bool bRecurse );

	void		AddIgnoreCompressionExtension( const char* extension );
	void		AddKeyValueFileExtension(const char* extension);

	bool		BuildAndSave( const char* fileNamePrefix );

protected:

	struct FileInfo
	{
		dpkfileinfo_t	pkinfo;
		EqString		fileName;
	};

	float				ProcessFile(COSFile& output, FileInfo* info);

	bool				WriteFiles();
	bool				SavePackage();

	bool				CheckCompressionIgnored(const char* extension) const;
	bool				CheckIsKeyValueFile(const char* extension) const;

	COSFile				m_outputFile;
	dpkheader_t			m_header;

	char				m_mountPath[DPK_STRING_SIZE];

	Array<FileInfo*>	m_files{ PP_SL };
	Array<EqString>		m_ignoreCompressionExt{ PP_SL };
	Array<EqString>		m_keyValueFileExt{ PP_SL };

	int					m_compressionLevel;
	int					m_encryption;

	IceKey				m_ice;
};