/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).  

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

//===========================================================================
//
// Name:				l_mem.cpp
// Function:
// Programmer:		Mr Elusive (MrElusive@demigod.demon.nl)
// Last update:	1999-06-02
// Tab Size:		3
//===========================================================================

#include "qbsp.h"
#include "l_log.h"
#include "../CompilerRuntime.h"
#include <windows.h>
#include <climits>
#include <stdexcept>

namespace {
HANDLE compilerHeap = nullptr;
size_t allocatedBytes = 0;
} // namespace

// A job owns the entire arena, including allocations abandoned by a fatal
// legacy parse. The engine's zone/hunk allocators are never used here.
void aasMemoryBegin() {
    compilerHeap = HeapCreate(0, 0, 0);
    if (!compilerHeap) {
        throw std::runtime_error("Cannot create AAS compiler arena");
    }
    allocatedBytes = 0;
}
void aasMemoryEnd() {
    if (compilerHeap) {
        HeapDestroy(compilerHeap);
        compilerHeap = nullptr;
    }
    allocatedBytes = 0;
}
int MemorySize(void *ptr) {
    if (!ptr) {
        return 0;
    }
    const auto size = HeapSize(compilerHeap, 0, ptr);
    if (size == static_cast<SIZE_T>(-1) || size > INT_MAX) {
        throw std::runtime_error("Invalid AAS arena allocation");
    }
    return static_cast<int>(size);
}
void *GetMemory(unsigned long size) {
    compilerCheckCancelled();
    if (!compilerHeap || size > INT_MAX) {
        throw std::runtime_error("Invalid AAS allocation size");
    }
    void *ptr = HeapAlloc(compilerHeap, 0, size ? size : 1);
    if (!ptr) {
        throw std::runtime_error("AAS compiler out of memory");
    }
    allocatedBytes += MemorySize(ptr);
    return ptr;
}
void *GetClearedMemory(unsigned long size) {
    void *ptr = GetMemory(size);
    memset(ptr, 0, size);
    return ptr;
}
void FreeMemory(void *ptr) {
    if (ptr) {
        allocatedBytes -= MemorySize(ptr);
        if (!HeapFree(compilerHeap, 0, ptr)) {
            throw std::runtime_error("Cannot release AAS arena allocation");
        }
    }
}
int TotalAllocatedMemory() {
    return static_cast<int>(allocatedBytes > INT_MAX ? INT_MAX : allocatedBytes);
}
void PrintMemorySize(unsigned long size) {
    Log_Print("%lu bytes", size);
}

//===========================================================================
// Q3 Hunk and Z_ memory management
//===========================================================================

typedef struct memhunk_s
{
	void *ptr;
	struct memhunk_s *next;
} memhunk_t;

memhunk_t *memhunk_high;
memhunk_t *memhunk_low;
int memhunk_high_size = 16 * 1024 * 1024;
int memhunk_low_size = 0;

//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void Hunk_ClearHigh( void ) {
	memhunk_t *h, *nexth;

	for ( h = memhunk_high; h; h = nexth )
	{
		nexth = h->next;
		FreeMemory( h );
	} //end for
	memhunk_high = NULL;
	memhunk_high_size = 16 * 1024 * 1024;
} //end of the function Hunk_ClearHigh
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void *Hunk_Alloc( int size ) {
	memhunk_t *h;

	if ( !size ) {
		return (void *) memhunk_high_size;
	}
	//
    h = (memhunk_t *)(GetClearedMemory(size + sizeof(memhunk_t)));
    h->ptr = (char *)h + sizeof(memhunk_t);
    h->next = memhunk_high;
	memhunk_high = h;
	memhunk_high_size -= size;
	return h->ptr;
} //end of the function Hunk_Alloc
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void *Z_Malloc( int size ) {
	return GetClearedMemory( size );
} //end of the function Z_Malloc
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void Z_Free( void *ptr ) {
	FreeMemory( ptr );
} //end of the function Z_Free
