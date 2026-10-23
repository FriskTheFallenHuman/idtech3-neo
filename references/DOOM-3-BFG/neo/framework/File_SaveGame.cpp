/*
===========================================================================

Doom 3 BFG Edition GPL Source Code
Copyright (C) 1993-2012 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 BFG Edition GPL Source Code ("Doom 3 BFG Edition Source Code").

Doom 3 BFG Edition Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 BFG Edition Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 BFG Edition Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 BFG Edition Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 BFG Edition Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/
#include "precompiled.h"
#pragma hdrstop

#include "File_SaveGame.h"

/*

TODO: CRC on each block

*/


/*
========================
MinizAlloc
========================
*/
void * MinizAlloc( void *opaque, size_t items, size_t size ) {
	return Mem_Alloc( items * size, TAG_SAVEGAMES );
}

/*
========================
MinizFree
========================
*/
void MinizFree( void *opaque, void * address ) {
	Mem_Free( address );
}

idCVar sgf_threads( "sgf_threads", "2", CVAR_INTEGER, "0 = all foreground, 1 = background write, 2 = background write + compress" );
idCVar sgf_checksums( "sgf_checksums", "1", CVAR_BOOL, "enable save game file checksums" );
idCVar sgf_testCorruption( "sgf_testCorruption", "-1", CVAR_INTEGER, "test corruption at the 128 kB compressed block" );

// this is supposed to get faster going from -15 to -9, but it gets slower as well as worse compression
idCVar sgf_windowBits( "sgf_windowBits", "-15", CVAR_INTEGER, "miniz window bits" );

bool idFile_SaveGamePipelined::cancelToTerminate = false;

class idSGFcompressThread : public idSysThread {
public:
	virtual int			Run() { sgf->CompressBlock(); return 0; }
	idFile_SaveGamePipelined * sgf;
};
class idSGFdecompressThread : public idSysThread {
public:
	virtual int			Run() { sgf->DecompressBlock(); return 0; }
	idFile_SaveGamePipelined * sgf;
};
class idSGFwriteThread : public idSysThread {
public:
	virtual int			Run() { sgf->WriteBlock(); return 0; }
	idFile_SaveGamePipelined * sgf;
};
class idSGFreadThread : public idSysThread {
public:
	virtual int			Run() { sgf->ReadBlock(); return 0; }
	idFile_SaveGamePipelined * sgf;
};

/*
============================
idFile_SaveGamePipelined::idFile_SaveGamePipelined
============================
*/
idFile_SaveGamePipelined::idFile_SaveGamePipelined() :
		mode( CLOSED ),
		compressedLength( 0 ),
		uncompressedProducedBytes( 0 ),
		uncompressedConsumedBytes( 0 ),
		compressedProducedBytes( 0 ),
		compressedConsumedBytes( 0 ),
		dataMiniz( NULL ),
		bytesMiniz( 0 ),
		dataIO( NULL ),
		bytesIO( 0 ),
		mzFlushType( Z_NO_FLUSH ),
		mzStreamEndHit( false ),
		numChecksums( 0 ),
		nativeFile( NULL ),
		nativeFileEndHit( false ),
		finished( false ),
		readThread( NULL ),
		writeThread( NULL ),
		decompressThread( NULL ),
		compressThread( NULL ),
		blockFinished( true ),
		buildVersion( "" ),
		saveFormatVersion( 0 ) {

	memset( &mzStream, 0, sizeof( mzStream ) );
	memset( compressed, 0, sizeof( compressed ) );
	memset( uncompressed, 0, sizeof( uncompressed ) );
	mzStream.zalloc = MinizAlloc;
	mzStream.zfree = MinizFree;
}

/*
============================
idFile_SaveGamePipelined::~idFile_SaveGamePipelined
============================
*/
idFile_SaveGamePipelined::~idFile_SaveGamePipelined() {
	Finish();

	// free the threads
	if ( compressThread != NULL ) {
		delete compressThread;
		compressThread = NULL;
	}
	if ( decompressThread != NULL ) {
		delete decompressThread;
		decompressThread = NULL;
	}
	if ( readThread != NULL ) {
		delete readThread;
		readThread = NULL;
	}
	if ( writeThread != NULL ) {
		delete writeThread;
		writeThread = NULL;
	}

	// close the native file
/*	if ( nativeFile != NULL ) {
		delete nativeFile;
		nativeFile = NULL;
	} */

	dataMiniz = NULL;
	dataIO = NULL;
}

/*
========================
idFile_SaveGamePipelined::ReadBuildVersion
========================
*/
bool idFile_SaveGamePipelined::ReadBuildVersion() {
	return ReadString( buildVersion ) != 0;
}

/*
========================
idFile_SaveGamePipelined::ReadSaveFormatVersion
========================
*/
bool idFile_SaveGamePipelined::ReadSaveFormatVersion() {
	if ( ReadBig( pointerSize ) <= 0 ) {
		return false;
	}
	return ReadBig( saveFormatVersion ) != 0;
}

/*
========================
idFile_SaveGamePipelined::GetPointerSize
========================
*/
int idFile_SaveGamePipelined::GetPointerSize() const {
	if ( pointerSize == 0 ) {
		// in original savegames we weren't saving the pointer size, so the 2 high bytes of the save version will be 0
		return 4;
	}  else {
		return pointerSize;
	}
}

/*
============================
idFile_SaveGamePipelined::Finish
============================
*/
void idFile_SaveGamePipelined::Finish() {
	if ( mode == WRITE ) {

		// wait for the compression thread to complete, which may kick off a write
		if ( compressThread != NULL ) {
			compressThread->WaitForThread();
		}

		// force the next compression to emit everything
		mzFlushType = MZ_FINISH;
		FlushUncompressedBlock();

		if ( compressThread != NULL ) {
			compressThread->WaitForThread();
		}

		if ( writeThread != NULL ) {
			// wait for the IO thread to exit
			writeThread->WaitForThread();
		} else if ( nativeFile == NULL && !nativeFileEndHit ) {
			// wait for the last block to be consumed
			blockRequested.Wait();
			finished = true;
			blockAvailable.Raise();
			blockFinished.Wait();
		}

		// free miniz tables
		mz_deflateEnd( &mzStream );

	} else if ( mode == READ ) {

		// wait for the decompression thread to complete, which may kick off a read
		if ( decompressThread != NULL ) {
			decompressThread->WaitForThread();
		}

		if ( readThread != NULL ) {
			// wait for the IO thread to exit
			readThread->WaitForThread();
		} else if ( nativeFile == NULL && !nativeFileEndHit ) {
			// wait for the last block to be consumed
			blockAvailable.Wait();
			finished = true;
			blockRequested.Raise();
			blockFinished.Wait();
		}

		// free miniz tables
		mz_inflateEnd( &mzStream );
	}

	mode = CLOSED;
}

/*
============================
idFile_SaveGamePipelined::Abort
============================
*/
void idFile_SaveGamePipelined::Abort() {
	if ( mode == WRITE ) {

		if ( compressThread != NULL ) {
			compressThread->WaitForThread();
		}
		if ( writeThread != NULL ) {
			writeThread->WaitForThread();
		} else if ( nativeFile == NULL && !nativeFileEndHit ) {
			blockRequested.Wait();
			finished = true;
			dataIO = NULL;
			bytesIO = 0;
			blockAvailable.Raise();
			blockFinished.Wait();
		}

	} else if ( mode == READ ) {

		if ( decompressThread != NULL ) {
			decompressThread->WaitForThread();
		}
		if ( readThread != NULL ) {
			readThread->WaitForThread();
		} else if ( nativeFile == NULL && !nativeFileEndHit ) {
			blockAvailable.Wait();
			finished = true;
			dataIO = NULL;
			bytesIO = 0;
			blockRequested.Raise();
			blockFinished.Wait();
		}
	}

	mode = CLOSED;
}

/*
===================================================================================

WRITE PATH

===================================================================================
*/

/*
============================
idFile_SaveGamePipelined::OpenForWriting
============================
*/
bool idFile_SaveGamePipelined::OpenForWriting( const char * const filename, bool useNativeFile ) {
	assert( mode == CLOSED );

	name = filename;
	osPath = filename;
	mode = WRITE;
	nativeFile = NULL;
	numChecksums = 0;

	if ( useNativeFile ) {
		nativeFile = fileSystem->OpenFileWrite( filename );
		if ( nativeFile == NULL ) {
			return false;
		}
	}

	// raw deflate with no header / checksum
	// use max memory for fastest compression
	// optimize for higher speed
	//mem.PushHeap();
	int status = mz_deflateInit2( &mzStream, MZ_BEST_SPEED, MZ_DEFLATED, sgf_windowBits.GetInteger(), 9, MZ_DEFAULT_STRATEGY );
	//mem.PopHeap();
	if ( status != MZ_OK ) {
		idLib::FatalError( "idFile_SaveGamePipelined::OpenForWriting: mz_deflateInit2() error %i", status );
	}

	// initial buffer setup
	mzStream.avail_out = COMPRESSED_BLOCK_SIZE;
	mzStream.next_out = (Bytef * )compressed;

	if ( sgf_checksums.GetBool() ) {
		mzStream.avail_out -= sizeof( uint32 );
	}

	if ( sgf_threads.GetInteger() >= 1 ) {
		compressThread = new (TAG_IDFILE) idSGFcompressThread();
		compressThread->sgf = this;

		// DG: change threadname from "SGF_CompressThread" to "SGF_Compress", because Linux
		// has a 15 (+ \0) char limit for threadnames. also, "thread" in a threadname is redundant
		compressThread->StartWorkerThread( "SGF_Compress", CORE_2B, THREAD_NORMAL );
	}
	if ( nativeFile != NULL && sgf_threads.GetInteger() >= 2 ) {
		writeThread = new (TAG_IDFILE) idSGFwriteThread();
		writeThread->sgf = this;

		// DG: change threadname from "SGF_WriteThread" to "SGF_Write", because Linux
		// has a 15 (+ \0) char limit for threadnames. also, "thread" in a threadname is redundant
		writeThread->StartWorkerThread( "SGF_Write", CORE_2A, THREAD_NORMAL );
	}

	return true;
}

/*
============================
idFile_SaveGamePipelined::OpenForWriting
============================
*/
bool idFile_SaveGamePipelined::OpenForWriting( idFile * file )  {
	assert( mode == CLOSED );

	if ( file == NULL ) {
		return false;
	}

	name = file->GetName();
	osPath = file->GetFullPath();
	mode = WRITE;
	nativeFile = file;
	numChecksums = 0;


	// raw deflate with no header / checksum
	// use max memory for fastest compression
	// optimize for higher speed
	//mem.PushHeap();
	int status = mz_deflateInit2( &mzStream, MZ_BEST_SPEED, MZ_DEFLATED, sgf_windowBits.GetInteger(), 9, MZ_DEFAULT_STRATEGY );
	//mem.PopHeap();
	if ( status != MZ_OK ) {
		idLib::FatalError( "idFile_SaveGamePipelined::OpenForWriting: mz_deflateInit2() error %i", status );
	}

	// initial buffer setup
	mzStream.avail_out = COMPRESSED_BLOCK_SIZE;
	mzStream.next_out = (Bytef * )compressed;

	if ( sgf_checksums.GetBool() ) {
		mzStream.avail_out -= sizeof( uint32 );
	}

	if ( sgf_threads.GetInteger() >= 1 ) {
		compressThread = new (TAG_IDFILE) idSGFcompressThread();
		compressThread->sgf = this;

		// DG: change threadname from "SGF_CompressThread" to "SGF_Compress", because Linux
		// has a 15 (+ \0) char limit for threadnames. also, "thread" in a threadname is redundant
		compressThread->StartWorkerThread( "SGF_Compress", CORE_2B, THREAD_NORMAL );
	}
	if ( nativeFile != NULL && sgf_threads.GetInteger() >= 2 ) {
		writeThread = new (TAG_IDFILE) idSGFwriteThread();
		writeThread->sgf = this;

		// DG: change threadname from "SGF_WriteThread" to "SGF_Write", because Linux
		// has a 15 (+ \0) char limit for threadnames. also, "thread" in a threadname is redundant
		writeThread->StartWorkerThread( "SGF_Write", CORE_2A, THREAD_NORMAL );
	}

	return true;
}

/*
============================
idFile_SaveGamePipelined::NextWriteBlock

Modifies:
	dataIO
	bytesIO
============================
*/
bool idFile_SaveGamePipelined::NextWriteBlock( blockForIO_t * block ) {
	assert( mode == WRITE );

	blockRequested.Raise();		// the background thread is done with the last block

	if ( nativeFileEndHit ) {
		return false;
	}

	blockAvailable.Wait();	// wait for a new block to come through the pipeline

	if ( finished || block == NULL ) {
		nativeFileEndHit = true;
		blockRequested.Raise();
		blockFinished.Raise();
		return false;
	}

	compressedLength += bytesIO;

	block->data = dataIO;
	block->bytes = bytesIO;

	dataIO = NULL;
	bytesIO = 0;

	return true;
}

/*
============================
idFile_SaveGamePipelined::WriteBlock

Modifies:
	dataIO
	bytesIO
	nativeFile
============================
*/
void idFile_SaveGamePipelined::WriteBlock() {
	assert( nativeFile != NULL );

	compressedLength += bytesIO;

	nativeFile->Write( dataIO, bytesIO );

	dataIO = NULL;
	bytesIO = 0;
}

/*
============================
idFile_SaveGamePipelined::FlushCompressedBlock

Called when a compressed block fills up, and also to flush the final partial block.
Flushes everything from [compressedConsumedBytes -> compressedProducedBytes)

Reads:
	compressed
	compressedProducedBytes

Modifies:
	dataMiniz
	bytesMiniz
	compressedConsumedBytes
============================
*/
void idFile_SaveGamePipelined::FlushCompressedBlock() {
	// block until the background thread is done with the last block
	if ( writeThread != NULL ) {
		writeThread->WaitForThread();
	} if ( nativeFile == NULL ) {
		if ( !nativeFileEndHit ) {
			blockRequested.Wait();
		}
	}

	// prepare the next block to be written out
	dataIO = &compressed[ compressedConsumedBytes & ( COMPRESSED_BUFFER_SIZE - 1 ) ];
	bytesIO = compressedProducedBytes - compressedConsumedBytes;
	compressedConsumedBytes = compressedProducedBytes;

	if ( writeThread != NULL ) {
		// signal a new block is available to be written out
		writeThread->SignalWork();
	} else if ( nativeFile != NULL ) {
		// write syncronously
		WriteBlock();
	} else {
		// signal a new block is available to be written out
		blockAvailable.Raise();
	}
}

/*
============================
idFile_SaveGamePipelined::CompressBlock

Called when an uncompressed block fills up, and also to flush the final partial block.
Flushes everything from [uncompressedConsumedBytes -> uncompressedProducedBytes)

Modifies:
	dataMiniz
	bytesMiniz
	compressed
	compressedProducedBytes
	mzStream
	mzStreamEndHit
============================
*/
void idFile_SaveGamePipelined::CompressBlock() {
	mzStream.next_in = (Bytef * )dataMiniz;
	mzStream.avail_in = (uInt) bytesMiniz;

	dataMiniz = NULL;
	bytesMiniz = 0;

	// if this is the finish block, we may need to write
	// multiple buffers even after all input has been consumed
	while( mzStream.avail_in > 0 || mzFlushType == MZ_FINISH ) {

		const int zstat = mz_deflate( &mzStream, mzFlushType );

		if ( zstat != MZ_OK && zstat != MZ_STREAM_END ) {
			idLib::FatalError( "idFile_SaveGamePipelined::CompressBlock: mz_deflate() returned %i", zstat );
		}

		if ( mzStream.avail_out == 0 || mzFlushType == MZ_FINISH ) {

			if ( sgf_checksums.GetBool() ) {
				size_t blockSize = mzStream.total_out + numChecksums * sizeof( uint32 ) - compressedProducedBytes;
				uint32 checksum = MD5_BlockChecksum( mzStream.next_out - blockSize, blockSize );
				mzStream.next_out[0] = ( ( checksum >>  0 ) & 0xFF );
				mzStream.next_out[1] = ( ( checksum >>  8 ) & 0xFF );
				mzStream.next_out[2] = ( ( checksum >> 16 ) & 0xFF );
				mzStream.next_out[3] = ( ( checksum >> 24 ) & 0xFF );
				numChecksums++;
			}

			// flush the output buffer IO
			compressedProducedBytes = mzStream.total_out + numChecksums * sizeof( uint32 );
			FlushCompressedBlock();
			if ( zstat == MZ_STREAM_END ) {
				assert( mzFlushType == MZ_FINISH );
				mzStreamEndHit = true;
				return;
			}

			assert( 0 == ( compressedProducedBytes & ( COMPRESSED_BLOCK_SIZE - 1 ) ) );

			mzStream.avail_out = COMPRESSED_BLOCK_SIZE;
			mzStream.next_out = (Bytef * )&compressed[ compressedProducedBytes & ( COMPRESSED_BUFFER_SIZE - 1 ) ];

			if ( sgf_checksums.GetBool() ) {
				mzStream.avail_out -= sizeof( uint32 );
			}
		}
	}
}

/*
============================
idFile_SaveGamePipelined::FlushUncompressedBlock

Called when an uncompressed block fills up, and also to flush the final partial block.
Flushes everything from [uncompressedConsumedBytes -> uncompressedProducedBytes)

Reads:
	uncompressed
	uncompressedProducedBytes

Modifies:
	dataMiniz
	bytesMiniz
	uncompressedConsumedBytes
============================
*/
void idFile_SaveGamePipelined::FlushUncompressedBlock() {
	// block until the background thread has completed
	if ( compressThread != NULL ) {
		// make sure thread has completed the last work
		compressThread->WaitForThread();
	}

	// prepare the next block to be consumed by Miniz
	dataMiniz = &uncompressed[ uncompressedConsumedBytes & ( UNCOMPRESSED_BUFFER_SIZE - 1 ) ];
	bytesMiniz = uncompressedProducedBytes - uncompressedConsumedBytes;
	uncompressedConsumedBytes = uncompressedProducedBytes;

	if ( compressThread != NULL ) {
		// signal thread for more work
		compressThread->SignalWork();
	} else {
		// run syncronously
		CompressBlock();
	}
}

/*
============================
idFile_SaveGamePipelined::Write

Modifies:
	uncompressed
	uncompressedProducedBytes
============================
*/
int idFile_SaveGamePipelined::Write( const void * buffer, int length ) {
	if ( buffer == NULL || length <= 0 ) {
		return 0;
	}

#if 1	// quick and dirty fix for user-initiated forced shutdown during a savegame
	if ( cancelToTerminate ) {
		if ( mode != CLOSED ) {
			Abort();
		}
		return 0;
	}
#endif

	assert( mode == WRITE );
	size_t lengthRemaining = length;
	const byte * buffer_p = (const byte *)buffer;
	while ( lengthRemaining > 0 ) {
		const size_t ofsInBuffer = uncompressedProducedBytes & ( UNCOMPRESSED_BUFFER_SIZE - 1 );
		const size_t ofsInBlock = uncompressedProducedBytes & ( UNCOMPRESSED_BLOCK_SIZE - 1 );
		const size_t remainingInBlock = UNCOMPRESSED_BLOCK_SIZE - ofsInBlock;
		const size_t copyToBlock = ( lengthRemaining < remainingInBlock ) ? lengthRemaining : remainingInBlock;

		memcpy( uncompressed + ofsInBuffer, buffer_p, copyToBlock );
		uncompressedProducedBytes += copyToBlock;

		buffer_p += copyToBlock;
		lengthRemaining -= copyToBlock;

		if ( copyToBlock == remainingInBlock ) {
			FlushUncompressedBlock();
		}
	}
	return length;
}

/*
===================================================================================

READ PATH

===================================================================================
*/

/*
============================
idFile_SaveGamePipelined::OpenForReading
============================
*/
bool idFile_SaveGamePipelined::OpenForReading( const char * const filename, bool useNativeFile ) {
	assert( mode == CLOSED );

	name = filename;
	osPath = filename;
	mode = READ;
	nativeFile = NULL;
	numChecksums = 0;

	if ( useNativeFile ) {
		nativeFile = fileSystem->OpenFileRead( filename );
		if ( nativeFile == NULL ) {
			return false;
		}
	}

	// init miniz for raw inflate with a 32k dictionary
	//mem.PushHeap();
	int status = mz_inflateInit2( &mzStream, sgf_windowBits.GetInteger() );
	//mem.PopHeap();
	if ( status != MZ_OK ) {
		idLib::FatalError( "idFile_SaveGamePipelined::OpenForReading: mz_inflateInit2() error %i", status );
	}

	// spawn threads
	if ( sgf_threads.GetInteger() >= 1 ) {
		decompressThread = new (TAG_IDFILE) idSGFdecompressThread();
		decompressThread->sgf = this;

		// DG: change threadname from "SGF_DecompressThread" to "SGF_Decompress", because Linux
		// has a 15 (+ \0) char limit for threadnames. also, "thread" in a threadname is redundant
		decompressThread->StartWorkerThread( "SGF_Decompress", CORE_2B, THREAD_NORMAL );
	}
	if ( nativeFile != NULL && sgf_threads.GetInteger() >= 2 ) {
		readThread = new (TAG_IDFILE) idSGFreadThread();
		readThread->sgf = this;

		// DG: change threadname from "SGF_ReadThread" to "SGF_Read", because Linux
		// has a 15 (+ \0) char limit for threadnames. also, "thread" in a threadname is redundant
		readThread->StartWorkerThread( "SGF_Read", CORE_2A, THREAD_NORMAL );
	}

	return true;
}


/*
============================
idFile_SaveGamePipelined::OpenForReading
============================
*/
bool idFile_SaveGamePipelined::OpenForReading( idFile * file ) {
	assert( mode == CLOSED );

	if ( file == NULL ) {
		return false;
	}

	name = file->GetName();
	osPath = file->GetFullPath();
	mode = READ;
	nativeFile = file;
	numChecksums = 0;

	// init miniz for raw inflate with a 32k dictionary
	//mem.PushHeap();
	int status = mz_inflateInit2( &mzStream, sgf_windowBits.GetInteger() );
	//mem.PopHeap();
	if ( status != MZ_OK ) {
		idLib::FatalError( "idFile_SaveGamePipelined::OpenForReading: mz_inflateInit2() error %i", status );
	}

	// spawn threads
	if ( sgf_threads.GetInteger() >= 1 ) {
		decompressThread = new (TAG_IDFILE) idSGFdecompressThread();
		decompressThread->sgf = this;

		// DG: change threadname from "SGF_DecompressThread" to "SGF_Decompress", because Linux
		// has a 15 (+ \0) char limit for threadnames. also, "thread" in a threadname is redundant
		decompressThread->StartWorkerThread( "SGF_Decompress", CORE_1B, THREAD_NORMAL );
	}
	if ( nativeFile != NULL && sgf_threads.GetInteger() >= 2 ) {
		readThread = new (TAG_IDFILE) idSGFreadThread();
		readThread->sgf = this;

		// DG: change threadname from "SGF_ReadThread" to "SGF_Read", because Linux
		// has a 15 (+ \0) char limit for threadnames. also, "thread" in a threadname is redundant
		readThread->StartWorkerThread( "SGF_Read", CORE_1A, THREAD_NORMAL );
	}

	return true;
}


/*
============================
idFile_SaveGamePipelined::NextReadBlock

Reads the next data block from the filesystem into the memory buffer.

Modifies:
	compressed
	compressedProducedBytes
	nativeFileEndHit
============================
*/
bool idFile_SaveGamePipelined::NextReadBlock( blockForIO_t * block, size_t lastReadBytes ) {
	assert( mode == READ );

	assert( ( lastReadBytes & ( COMPRESSED_BLOCK_SIZE - 1 ) ) == 0 || block == NULL );
	compressedProducedBytes += lastReadBytes;

	blockAvailable.Raise();		// a new block is available for the pipeline to consume

	if ( nativeFileEndHit ) {
		return false;
	}

	blockRequested.Wait();		// wait for the last block to be consumed by the pipeline

	if ( finished || block == NULL ) {
		nativeFileEndHit = true;
		blockAvailable.Raise();
		blockFinished.Raise();
		return false;
	}

	assert( 0 == ( compressedProducedBytes & ( COMPRESSED_BLOCK_SIZE - 1 ) ) );
	block->data = & compressed[compressedProducedBytes & ( COMPRESSED_BUFFER_SIZE - 1 )];
	block->bytes = COMPRESSED_BLOCK_SIZE;

	return true;
}

/*
============================
idFile_SaveGamePipelined::ReadBlock

Reads the next data block from the filesystem into the memory buffer.

Modifies:
	compressed
	compressedProducedBytes
	nativeFile
	nativeFileEndHit
============================
*/
void idFile_SaveGamePipelined::ReadBlock() {
	assert( nativeFile != NULL );
	// normally run in a separate thread
	if ( nativeFileEndHit ) {
		return;
	}
	// when we are reading the last block of the file, we may not fill the entire block
	assert( 0 == ( compressedProducedBytes & ( COMPRESSED_BLOCK_SIZE - 1 ) ) );
	byte * dest = &compressed[ compressedProducedBytes & ( COMPRESSED_BUFFER_SIZE - 1 ) ];
	size_t ioBytes = nativeFile->Read( dest, COMPRESSED_BLOCK_SIZE );
	compressedProducedBytes += ioBytes;
	if ( ioBytes != COMPRESSED_BLOCK_SIZE ) {
		nativeFileEndHit = true;
	}
}

/*
============================
idFile_SaveGamePipelined::PumpCompressedBlock

Reads:
	compressed
	compressedProducedBytes

Modifies:
	dataIO
	byteIO
	compressedConsumedBytes
============================
*/
void idFile_SaveGamePipelined::PumpCompressedBlock() {
	// block until the background thread is done with the last block
	if ( readThread != NULL ) {
		readThread->WaitForThread();
	} else if ( nativeFile == NULL ) {
		if ( !nativeFileEndHit ) {
			blockAvailable.Wait();
		}
	}

	// fetch the next block read in
	dataIO = &compressed[ compressedConsumedBytes & ( COMPRESSED_BUFFER_SIZE - 1 ) ];
	bytesIO = compressedProducedBytes - compressedConsumedBytes;
	compressedConsumedBytes = compressedProducedBytes;

	if ( readThread != NULL ) {
		// signal read thread to read another block
		readThread->SignalWork();
	} else if ( nativeFile != NULL ) {
		// run syncronously
		ReadBlock();
	} else {
		// request a new block
		blockRequested.Raise();
	}
}

/*
============================
idFile_SaveGamePipelined::DecompressBlock

Decompresses the next data block from the memory buffer

Normally this runs in a separate thread when signalled, but
can be called in the main thread for debugging.

This will not exit until a complete block has been decompressed,
unless end-of-file is reached.

This may require additional compressed blocks to be read.

Reads:
	nativeFileEndHit

Modifies:
	dataIO
	bytesIO
	uncompressed
	uncompressedProducedBytes
	mzStreamEndHit
	mzStream
============================
*/
void idFile_SaveGamePipelined::DecompressBlock() {
	if ( mzStreamEndHit ) {
		return;
	}

	assert( ( uncompressedProducedBytes & ( UNCOMPRESSED_BLOCK_SIZE - 1 ) ) == 0 );
	mzStream.next_out = (Bytef * )&uncompressed[ uncompressedProducedBytes & ( UNCOMPRESSED_BUFFER_SIZE - 1 ) ];
	mzStream.avail_out = UNCOMPRESSED_BLOCK_SIZE;

	while( mzStream.avail_out > 0 ) {
		if ( mzStream.avail_in == 0 ) {
			do {
				PumpCompressedBlock();
				if ( bytesIO == 0 && nativeFileEndHit ) {
					// don't try to decompress any more if there is no more data
					mzStreamEndHit = true;
					return;
				}
			} while ( bytesIO == 0 );

			mzStream.next_in = (Bytef *) dataIO;
			mzStream.avail_in = (uInt) bytesIO;

			dataIO = NULL;
			bytesIO = 0;

			if ( sgf_checksums.GetBool() ) {
				if ( sgf_testCorruption.GetInteger() == numChecksums ) {
					((Bytef *)mzStream.next_in)[0] ^= 0xFF;
				}
				mzStream.avail_in -= sizeof( uint32 );
				uint32 checksum = MD5_BlockChecksum( mzStream.next_in, mzStream.avail_in );
				if (	!verify( mzStream.next_in[mzStream.avail_in + 0] == ( ( checksum >>  0 ) & 0xFF ) ) ||
						!verify( mzStream.next_in[mzStream.avail_in + 1] == ( ( checksum >>  8 ) & 0xFF ) ) ||
						!verify( mzStream.next_in[mzStream.avail_in + 2] == ( ( checksum >> 16 ) & 0xFF ) ) ||
						!verify( mzStream.next_in[mzStream.avail_in + 3] == ( ( checksum >> 24 ) & 0xFF ) ) ) {
					// don't try to decompress any more if the checksum is wrong
					mzStreamEndHit = true;
					return;
				}
				numChecksums++;
			}
		}

		const int mzstat = mz_inflate( &mzStream, MZ_SYNC_FLUSH );

		uncompressedProducedBytes = mzStream.total_out;

		if ( mzstat == MZ_STREAM_END ) {
			// don't try to decompress any more
			mzStreamEndHit = true;
			return;
		}
		if ( mzstat != MZ_OK ) {
			idLib::Warning( "idFile_SaveGamePipelined::DecompressBlock: mz_inflate() returned %i", mzstat );
			mzStreamEndHit = true;
			return;
		}
	}

	assert( ( uncompressedProducedBytes & ( UNCOMPRESSED_BLOCK_SIZE - 1 ) ) == 0 );
}

/*
============================
idFile_SaveGamePipelined::PumpUncompressedBlock

Called when an uncompressed block is drained.

Reads:
	uncompressed
	uncompressedProducedBytes

Modifies:
	dataMiniz
	bytesMiniz
	uncompressedConsumedBytes
============================
*/
void idFile_SaveGamePipelined::PumpUncompressedBlock() {
	if ( decompressThread != NULL ) {
		// make sure thread has completed the last work
		decompressThread->WaitForThread();
	}

	// fetch the next block produced by Miniz
	dataMiniz = &uncompressed[ uncompressedConsumedBytes & ( UNCOMPRESSED_BUFFER_SIZE - 1 ) ];
	bytesMiniz = uncompressedProducedBytes - uncompressedConsumedBytes;
	uncompressedConsumedBytes = uncompressedProducedBytes;

	if ( decompressThread != NULL ) {
		// signal thread for more work
		decompressThread->SignalWork();
	} else {
		// run syncronously
		DecompressBlock();
	}
}

/*
============================
idFile_SaveGamePipelined::Read

Modifies:
	dataMiniz
	bytesMiniz
============================
*/
int idFile_SaveGamePipelined::Read( void * buffer, int length ) {
	if ( buffer == NULL || length <= 0 ) {
		return 0;
	}

	assert( mode == READ );

	size_t ioCount = 0;
	size_t lengthRemaining = length;
	byte * buffer_p = (byte *)buffer;
	while ( lengthRemaining > 0 ) {
		while ( bytesMiniz == 0 ) {
			PumpUncompressedBlock();
			if ( bytesMiniz == 0 && mzStreamEndHit ) {
				return ioCount;
			}
		}

		const size_t copyFromBlock = ( lengthRemaining < bytesMiniz ) ? lengthRemaining : bytesMiniz;

		memcpy( buffer_p, dataMiniz, copyFromBlock );
		dataMiniz += copyFromBlock;
		bytesMiniz -= copyFromBlock;

		buffer_p += copyFromBlock;
		ioCount += copyFromBlock;
		lengthRemaining -= copyFromBlock;
	}
	return ioCount;
}

#if 0
/*
===================================================================================

TEST CODE

===================================================================================
*/

/*
============================
TestProcessFile
============================
*/
static void TestProcessFile( const char * const filename ) {
	idLib::Printf( "Processing %s:\n", filename );
	// load some test data
	void *testData;
	const int testDataLength = fileSystem->ReadFile( filename, &testData, NULL );

	const char * const outFileName = "junk/savegameTest.bin";
	idFile_SaveGamePipelined *saveFile = new (TAG_IDFILE) idFile_SaveGamePipelined;
	saveFile->OpenForWriting( outFileName, true );

	const uint64 startWriteMicroseconds = Sys_Microseconds();

	saveFile->Write( testData, testDataLength );
	delete saveFile;		// final flush
	const int readDataLength = fileSystem->GetFileLength( outFileName );

	const uint64 endWriteMicroseconds = Sys_Microseconds();
	const uint64 writeMicroseconds = endWriteMicroseconds - startWriteMicroseconds;

	idLib::Printf( "%lld microseconds to compress %i bytes to %i written bytes = %4.1f MB/s\n",
		writeMicroseconds, testDataLength, readDataLength, (float)readDataLength / writeMicroseconds );

	void * readData = (void *)Mem_Alloc( testDataLength, TAG_SAVEGAMES );

	const uint64 startReadMicroseconds = Sys_Microseconds();

	idFile_SaveGamePipelined *loadFile = new (TAG_IDFILE) idFile_SaveGamePipelined;
	loadFile->OpenForReading( outFileName, true );
	loadFile->Read( readData, testDataLength );
	delete loadFile;

	const uint64 endReadMicroseconds = Sys_Microseconds();
	const uint64 readMicroseconds = endReadMicroseconds - startReadMicroseconds;

	idLib::Printf( "%lld microseconds to decompress = %4.1f MB/s\n", readMicroseconds, (float)testDataLength / readMicroseconds );

	int comparePoint;
	for ( comparePoint = 0; comparePoint < testDataLength; comparePoint++ ) {
		if ( ((byte *)readData)[comparePoint] != ((byte *)testData)[comparePoint] ) {
			break;
		}
	}
	if ( comparePoint != testDataLength ) {
		idLib::Printf( "Compare failed at %i.\n", comparePoint );
		assert( 0 );
	} else {
		idLib::Printf( "Compare succeeded.\n" );
	}
	Mem_Free( readData );
	Mem_Free( testData );
}

/*
============================
TestSaveGameFile
============================
*/
CONSOLE_COMMAND( TestSaveGameFile, "Exercises the pipelined savegame code", 0 ) {
#if 1
	TestProcessFile( "maps/game/wasteland1/wasteland1.map" );
#else
	// test every file in base (found a fencepost error >100 files in originally!)
	idFileList * fileList = fileSystem->ListFiles( "", "" );
	for ( int i = 0; i < fileList->GetNumFiles(); i++ ) {
		TestProcessFile( fileList->GetFile( i ) );
		common->UpdateConsoleDisplay();
	}
	delete fileList;
#endif
}

/*
============================
TestCompressionSpeeds
============================
*/
CONSOLE_COMMAND( TestCompressionSpeeds, "Compares miniz and our code", 0 ) {
	const char * const filename = "-colorMap.tga";

	idLib::Printf( "Processing %s:\n", filename );
	// load some test data
	void *testData;
	const int testDataLength = fileSystem->ReadFile( filename, &testData, NULL );

	const int startWriteMicroseconds = Sys_Microseconds();

	idCompressor *compressor = idCompressor::AllocLZW();
//	idFile *f = fileSystem->OpenFileWrite( "junk/lzwTest.bin" );
	idFile_Memory *f = new (TAG_IDFILE) idFile_Memory( "junk/lzwTest.bin" );
	compressor->Init( f, true, 8 );

	compressor->Write( testData, testDataLength );

	const int readDataLength = f->Tell();

	delete compressor;
	delete f;

	const int endWriteMicroseconds = Sys_Microseconds();
	const int writeMicroseconds = endWriteMicroseconds - startWriteMicroseconds;

	idLib::Printf( "%i microseconds to compress %i bytes to %i written bytes = %4.1f MB/s\n",
		writeMicroseconds, testDataLength, readDataLength, (float)readDataLength / writeMicroseconds );

}
#endif
