//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2017
//////////////////////////////////////////////////////////////////////////////////
// Description: RIFF reader utility class
//////////////////////////////////////////////////////////////////////////////////

#include "riff.h"
#include "DebugInterface.h"

CRIFF_Parser::CRIFF_Parser(const char* szFilename)
{
	m_pos = 0;

	m_riff = g_fileSystem->Open(szFilename, "rb" );
	m_riffData = NULL;

	if ( !m_riff )
	{
		m_curChunk.Id = 0;
		m_curChunk.Size = 0;
		return;
	}

	RIFFhdr_t header;
	ReadData((ubyte*)&header, sizeof(header));

	if ( header.Id != RIFF_ID )
	{
		MsgError("LoadRIFF: '%s' not valid RIFF file\n", szFilename);

		header.Id = 0;
		header.Size = 0;
		return;
	}
	else
	{
		m_start = m_pos;

		if ( header.Type != WAVE_ID )
		{
			MsgError("LoadRIFF: '%s' not valid WAVE file\n", szFilename);

			header.Id = 0;
			header.Size = 0;
		}
	}

	ChunkSet();
}

CRIFF_Parser::CRIFF_Parser(ubyte* pChunkData, int nChunkSize)
{
	m_pos = 0;

	m_riff = NULL;
	m_riffData = pChunkData;

	if ( !m_riffData )
	{
		m_curChunk.Id = 0;
		m_curChunk.Size = 0;
		return;
	}

	RIFFhdr_t header;
	ReadData((ubyte*)&header, sizeof(header));

	if ( header.Id != RIFF_ID )
	{
		header.Id = 0;
		header.Size = 0;
		return;
	}
	else
	{
		m_start = m_pos;

		if ( header.Type != WAVE_ID )
		{
			header.Id = 0;
			header.Size = 0;
		}
	}

	ChunkSet();
}

void CRIFF_Parser::ChunkClose ()
{
	if( m_riff )
	{
		g_fileSystem->Close( m_riff );
		m_riff = NULL;
	}
}

int CRIFF_Parser::ReadChunk( void* pOutput )
{
	return ReadData( pOutput, m_curChunk.Size );
}

int CRIFF_Parser::ReadData(void* dest, int len)
{
	if( m_riff )
	{
		int read = m_riff->Read( dest, 1, len );
		m_pos += read;
		return read;
	}
	else if( m_riffData )
	{
		memcpy( dest, m_riffData + m_pos, len );
		m_pos += len;
		return len;
	}
	else
		return 0;
}

int CRIFF_Parser::ReadInt()
{
	int i;
	ReadData( &i, sizeof(i) );
	return i;
}

int CRIFF_Parser::GetPos()
{
	return m_pos;
}

int CRIFF_Parser::SetPos(int pos)
{
	m_pos = pos;

	if ( m_riff )
		m_riff->Seek(pos, VS_SEEK_SET );

	return m_pos;
}

uint CRIFF_Parser::GetName()
{
	return m_curChunk.Id;
}

int CRIFF_Parser::GetSize()
{
	return m_curChunk.Size;
}

// goes to the next chunk
bool CRIFF_Parser::ChunkNext()
{
	bool result = ChunkSet();
	
	if(!result)
	{
		m_curChunk.Id = 0;
		m_curChunk.Size = 0;
	}

	return result;
}

//-----------------------------------------

bool CRIFF_Parser::ChunkSet()
{
	int n = ReadData((ubyte*)&m_curChunk, sizeof(m_curChunk));
	return n > 0;
}