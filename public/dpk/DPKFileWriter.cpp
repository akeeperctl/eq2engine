///////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Data Pack File writer
//////////////////////////////////////////////////////////////////////////////////

#include <lz4hc.h>

#include "core/core_common.h"
#include "core/IFileSystem.h"
#include "utils/KeyValues.h"
#include "DPKFileWriter.h"
#include "DPKUtils.h"

//---------------------------------------------

CDPKFileWriter::CDPKFileWriter(const char* mountPath, int compression, const char* encryptKey)
	: m_ice(0)
{
	memset(m_mountPath, 0, sizeof(m_mountPath));

	strncpy(m_mountPath, mountPath, DPK_STRING_SIZE);
	m_mountPath[DPK_STRING_SIZE - 1] = 0;
	FixSlashes(m_mountPath);
	xstrlwr(m_mountPath);

	m_compressionLevel = compression;
	if(encryptKey && *encryptKey)
	{
		if (strlen(encryptKey) == m_ice.keySize())
		{
			m_ice.set((const unsigned char*)encryptKey);
		}
		else
		{
			MsgError("CDPKFileWriter error - encryptKey size must be %d but only got %d", m_ice.keySize(), strlen(encryptKey));
		}
	}
}

CDPKFileWriter::~CDPKFileWriter()
{
	ASSERT(m_output.IsOpen() == false);
}

bool CDPKFileWriter::Begin(const char* fileName, ESearchPath searchPath)
{
	ASSERT(m_output.IsOpen() == false);
	if (m_output.IsOpen())
		return false;

	if (!m_output.Open(g_fileSystem->GetAbsolutePath(searchPath, fileName), COSFile::WRITE))
		return false;

	memset(&m_header, 0, sizeof(m_header));
	m_header.version = DPK_VERSION;
	m_header.signature = DPK_SIGNATURE;
	m_header.compressionLevel = m_compressionLevel;

	m_output.Write(&m_header, sizeof(m_header));
	m_output.Write(m_mountPath, DPK_STRING_SIZE);

	return true;
}

void CDPKFileWriter::Flush()
{
	if (!m_output.IsOpen())
		return;

	m_output.Flush();
}

int CDPKFileWriter::End()
{
	if (!m_output.IsOpen())
	{
		ASSERT(m_output.IsOpen() == true);
		return 0;
	}

	m_header.fileInfoOffset = m_output.Tell();
	m_header.numFiles = m_files.size();

	// overwrite as we have updated it
	m_output.Seek(0, COSFile::ESeekPos::SET);
	m_output.Write(&m_header, sizeof(m_header));
	m_output.Seek(m_header.fileInfoOffset, COSFile::ESeekPos::SET);

	// write file infos
	for (FileInfo& info : m_files)
	{
		m_output.Write(&info.pakInfo, sizeof(dpkfileinfo_t));
	}

	m_output.Close();

	const int numFiles = m_files.size();
	m_files.clear(true);

	return numFiles;
}

uint CDPKFileWriter::WriteDataToPackFile(IVirtualStream* fileData, dpkfileinfo_t& pakInfo, int packageFlags)
{
	// prepare stream to be read
	CMemoryStream readStream(PP_SL);
	if (fileData->GetType() == VS_TYPE_MEMORY)
	{
		// make a reader from the memory stream to not cause assert
		// when memory stream is open as VS_OPEN_WRITE only
		CMemoryStream* readFromFileData = static_cast<CMemoryStream*>(fileData);
		readStream.Open(readFromFileData->GetBasePointer(), VS_OPEN_READ, readFromFileData->GetSize());
		fileData = &readStream;
	}
	fileData->Seek(0, VS_SEEK_SET);

	// set the size and offset in the file bigfile
	pakInfo.offset = m_output.Tell();
	pakInfo.size = fileData->GetSize();
	pakInfo.crc = fileData->GetCRC32();

	int targetBlockFlags = packageFlags;

	if (!m_compressionLevel)
		targetBlockFlags &= ~DPKFILE_FLAG_COMPRESSED;

	if (!m_encrypted)
		targetBlockFlags &= ~DPKFILE_FLAG_ENCRYPTED;

	Array<ubyte*> readBuffer(PP_SL);
	readBuffer.resize(DPK_BLOCK_MAXSIZE);

	// compressed and encrypted files has to be put into blocks
	// uncompressed files are bypassing blocks
	if (!DPK_IsBlockFile(targetBlockFlags))
	{
		int numBlocks = 0;

		// copy file block by block (assuming we have a large file)
		while (true)
		{
			const int srcOffset = numBlocks * DPK_BLOCK_MAXSIZE;
			const int srcSize = min(DPK_BLOCK_MAXSIZE, ((int)pakInfo.size - srcOffset));

			if (srcSize <= 0)
				break; // EOF

			fileData->Read(readBuffer.ptr(), 1, srcSize);
			m_output.Write(readBuffer.ptr(), srcSize);

			++numBlocks;
			if (srcSize < DPK_BLOCK_MAXSIZE)
				break;
		}

		return pakInfo.size;
	}

	uint packedSize = 0;
	pakInfo.numBlocks = 0;

	// temporary block for both compression and encryption 
	// twice the size
	ubyte tmpBlockData[DPK_BLOCK_MAXSIZE];

	// write blocks
	dpkblock_t blockInfo;
	while (true)
	{
		memset(&blockInfo, 0, sizeof(dpkblock_t));

		// get block offset
		const int srcOffset = (int)pakInfo.numBlocks * DPK_BLOCK_MAXSIZE;
		const int srcSize = min(DPK_BLOCK_MAXSIZE, ((int)pakInfo.size - srcOffset));

		if (srcSize <= 0)
			break; // EOF

		blockInfo.size = srcSize;
		fileData->Read(readBuffer.ptr(), 1, srcSize);

		int compressedSize = -1;

		// try compressing
		if (targetBlockFlags & DPKFILE_FLAG_COMPRESSED)
		{
			memset(tmpBlockData, 0, sizeof(tmpBlockData));
			compressedSize = LZ4_compress_HC((const char*)readBuffer.ptr(), (char*)tmpBlockData, srcSize, sizeof(tmpBlockData), m_compressionLevel);
		}

		// compressedSize could be -1 which means buffer overlow (or uneffective)
		if (compressedSize > 0)
		{
			blockInfo.flags |= DPKFILE_FLAG_COMPRESSED;
			blockInfo.compressedSize = compressedSize;
			packedSize += compressedSize;
		}
		else
		{
			memcpy(tmpBlockData, readBuffer.ptr(), srcSize);
			packedSize += srcSize;
		}

		const int tmpBlockSize = (blockInfo.flags & DPKFILE_FLAG_COMPRESSED) ? blockInfo.compressedSize : srcSize;

		// encrypt tmpBlock
		if (targetBlockFlags & DPKFILE_FLAG_ENCRYPTED)
		{
			blockInfo.flags |= DPKFILE_FLAG_ENCRYPTED;

			const int iceBlockSize = m_ice.blockSize();

			ubyte* iceTempBlock = (ubyte*)stackalloc(iceBlockSize);
			ubyte* tmpBlockPtr = tmpBlockData;

			int bytesLeft = tmpBlockSize;

			// encrypt block by block
			while (bytesLeft > iceBlockSize)
			{
				m_ice.encrypt(tmpBlockPtr, iceTempBlock);

				// copy encrypted block
				memcpy(tmpBlockPtr, iceTempBlock, iceBlockSize);

				tmpBlockPtr += iceBlockSize;
				bytesLeft -= iceBlockSize;
			}
		}

		// write header and data
		m_output.Write(&blockInfo, sizeof(blockInfo));
		m_output.Write(tmpBlockData, tmpBlockSize);

		++pakInfo.numBlocks;

		// small block size indicates last block
		if (srcSize < DPK_BLOCK_MAXSIZE)
			break;
	}

	return packedSize;
}

uint CDPKFileWriter::Add(IVirtualStream* fileData, const char* fileName, int packageFlags)
{
	EqString fileNameString = fileName;
	DPK_FixSlashes(fileNameString);
	const int filenameHash = DPK_FilenameHash(fileNameString, DPK_VERSION);

	auto it = m_files.find(filenameHash);
	if (!it.atEnd())	// already added?
	{
		if ((*it).fileName != fileNameString)
		{
			ASSERT_FAIL("DPK_FilenameHash has hash collisions, please change hashing function for good");
		}
		MsgWarning("CDPKFileWriter warn: file '%s' was already\n", fileName);
		return 0;
	}

	it = m_files.insert(filenameHash);
	FileInfo& info = *it;
	info.fileName = fileNameString;
	info.pakInfo.filenameHash = filenameHash;

	return WriteDataToPackFile(fileData, info.pakInfo, packageFlags);
}

#if 0
IVirtualStream* CDPKFileWriter::Create(const char* fileName, bool skipCompression = false)
{
	EqString fileNameString = fileName;
	DPK_FixSlashes(fileNameString);
	const int filenameHash = DPK_FilenameHash(fileNameString, DPK_VERSION);

	auto it = m_files.find(filenameHash);
	if (!it.atEnd())	// already added?
		return 0;

	CMemoryStream* writeStream = PPNew CMemoryStream();
	m_openStreams.append(writeStream);
}


void CDPKFileWriter::Close(IVirtualStream* virtStream)
{
	if (virtStream->GetType() != VS_TYPE_MEMORY)
		return;

	WriteDataToPackFile(virtStream, );
}
#endif