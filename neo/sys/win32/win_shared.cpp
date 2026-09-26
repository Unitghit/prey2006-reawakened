/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "precompiled.h"
#pragma hdrstop

#include "win_local.h"
#include <lmerr.h>
#include <lmcons.h>
#include <lmwksta.h>
#include <errno.h>
#include <fcntl.h>
#include <direct.h>
#include <io.h>
#include <conio.h>

#include <dbghelp.h>
#include <atomic>
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>
#pragma comment( lib, "dbghelp.lib" )

/*
===============================================================================

Load profiler (com_profileLoad, diagnostic)

Samples the main thread's call stack about once per millisecond between
Sys_LoadProfileStart and Sys_LoadProfileStop. While the main thread is
suspended, the sampler only unwinds into a preallocated buffer: it never
allocates or prints, so it cannot wait on a lock the main thread holds.
Symbols are resolved afterwards on the main thread.

===============================================================================
*/

namespace {
const int PROFILE_MAX_SAMPLES = 60000;
const int PROFILE_MAX_DEPTH = 48;

struct profileSample_t {
	int		depth;
	DWORD64	pc[PROFILE_MAX_DEPTH];
};

profileSample_t *	profileSamples = NULL;
std::atomic<int>	profileCount( 0 );
std::atomic<bool>	profileStop( false );
HANDLE				profileTarget = NULL;
std::thread			profileWorker;

void ProfileSampleOnce() {
	const int index = profileCount.load();
	if ( index >= PROFILE_MAX_SAMPLES ) {
		return;
	}
	if ( SuspendThread( profileTarget ) == (DWORD)-1 ) {
		return;
	}
	CONTEXT context;
	memset( &context, 0, sizeof( context ) );
	context.ContextFlags = CONTEXT_FULL;
	if ( GetThreadContext( profileTarget, &context ) ) {
		profileSample_t &sample = profileSamples[index];
		sample.depth = 0;
		while ( sample.depth < PROFILE_MAX_DEPTH && context.Rip ) {
			sample.pc[sample.depth++] = context.Rip;
			DWORD64 imageBase = 0;
			PRUNTIME_FUNCTION function = RtlLookupFunctionEntry( context.Rip, &imageBase, NULL );
			if ( !function ) {
				// leaf function: the return address is on top of the stack
				context.Rip = *(DWORD64 *)context.Rsp;
				context.Rsp += 8;
			} else {
				PVOID handlerData;
				DWORD64 establisherFrame;
				RtlVirtualUnwind( UNW_FLAG_NHANDLER, imageBase, context.Rip, function, &context,
					&handlerData, &establisherFrame, NULL );
			}
		}
		profileCount.store( index + 1 );
	}
	ResumeThread( profileTarget );
}
}

void Sys_LoadProfileStart() {
	if ( profileWorker.joinable() ) {
		return;
	}
	if ( !profileSamples ) {
		profileSamples = (profileSample_t *)VirtualAlloc( NULL, sizeof( profileSample_t ) * PROFILE_MAX_SAMPLES,
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
		if ( !profileSamples ) {
			return;
		}
	}
	if ( !DuplicateHandle( GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &profileTarget,
			THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0 ) ) {
		return;
	}
	profileCount = 0;
	profileStop = false;
	timeBeginPeriod( 1 );
	profileWorker = std::thread( [] {
		while ( !profileStop.load() ) {
			ProfileSampleOnce();
			Sleep( 1 );
		}
	} );
}

void Sys_LoadProfileStop( const char *label ) {
	if ( !profileWorker.joinable() ) {
		return;
	}
	profileStop = true;
	profileWorker.join();
	timeEndPeriod( 1 );
	CloseHandle( profileTarget );
	profileTarget = NULL;

	const int count = profileCount.load();
	HANDLE process = GetCurrentProcess();
	SymSetOptions( SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS );
	SymInitialize( process, NULL, TRUE );

	std::map<DWORD64, std::string> names;
	auto resolve = [&]( DWORD64 pc ) -> const std::string & {
		auto found = names.find( pc );
		if ( found != names.end() ) {
			return found->second;
		}
		char buffer[sizeof( SYMBOL_INFO ) + 512];
		SYMBOL_INFO *symbol = (SYMBOL_INFO *)buffer;
		memset( buffer, 0, sizeof( buffer ) );
		symbol->SizeOfStruct = sizeof( SYMBOL_INFO );
		symbol->MaxNameLen = 511;
		DWORD64 displacement = 0;
		std::string name;
		if ( SymFromAddr( process, pc, &displacement, symbol ) ) {
			name = symbol->Name;
		} else {
			IMAGEHLP_MODULE64 module;
			memset( &module, 0, sizeof( module ) );
			module.SizeOfStruct = sizeof( module );
			name = SymGetModuleInfo64( process, pc, &module ) ? std::string( module.ModuleName ) + "!?" : "?";
		}
		return names.emplace( pc, name ).first->second;
	};

	std::map<std::string, int> inclusive, exclusive, paths;
	for ( int i = 0; i < count; i++ ) {
		const profileSample_t &sample = profileSamples[i];
		std::set<std::string> seen;
		std::string path;
		for ( int d = 0; d < sample.depth; d++ ) {
			const std::string &name = resolve( sample.pc[d] );
			if ( d == 0 ) {
				exclusive[name]++;
			}
			if ( seen.insert( name ).second ) {
				inclusive[name]++;
			}
			if ( d < 6 ) {
				path += ( d ? " < " : "" ) + name;
			}
		}
		paths[path]++;
	}
	SymCleanup( process );

	auto report = [&]( const char *kind, const std::map<std::string, int> &table, int limit ) {
		std::vector<std::pair<int, std::string> > sorted;
		for ( const auto &entry : table ) {
			sorted.push_back( std::make_pair( entry.second, entry.first ) );
		}
		std::sort( sorted.begin(), sorted.end(), []( const std::pair<int, std::string> &a, const std::pair<int, std::string> &b ) {
			return a.first > b.first;
		} );
		for ( int i = 0; i < (int)sorted.size() && i < limit; i++ ) {
			common->Printf( "LOAD_PROFILE %s %5.1f%% %s\n", kind, 100.0f * sorted[i].first / Max( 1, count ), sorted[i].second.c_str() );
		}
	};
	common->Printf( "LOAD_PROFILE stage=%s samples=%d\n", label, count );
	report( "incl", inclusive, 60 );
	report( "self", exclusive, 40 );
	report( "path", paths, 25 );
}

/*
================
Sys_GetSystemRam

	returns amount of physical memory in MB
================
*/
int Sys_GetSystemRam( void ) {
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof ( statex );
	GlobalMemoryStatusEx (&statex);
	int physRam = statex.ullTotalPhys / ( 1024 * 1024 );
	// HACK: For some reason, ullTotalPhys is sometimes off by a meg or two, so we round up to the nearest 16 megs
	physRam = ( physRam + 8 ) & ~15;
	return physRam;
}


/*
================
Sys_GetDriveFreeSpace
returns in megabytes
================
*/
int Sys_GetDriveFreeSpace( const char *path ) {
	DWORDLONG lpFreeBytesAvailable;
	DWORDLONG lpTotalNumberOfBytes;
	DWORDLONG lpTotalNumberOfFreeBytes;
	int ret = 26;
	//FIXME: see why this is failing on some machines
	if ( ::GetDiskFreeSpaceEx( path, (PULARGE_INTEGER)&lpFreeBytesAvailable, (PULARGE_INTEGER)&lpTotalNumberOfBytes, (PULARGE_INTEGER)&lpTotalNumberOfFreeBytes ) ) {
		ret = ( double )( lpFreeBytesAvailable ) / ( 1024.0 * 1024.0 );
	}
	return ret;
}

/*
================
Sys_LockMemory
================
*/
bool Sys_LockMemory( void *ptr, int bytes ) {
	return ( VirtualLock( ptr, (SIZE_T)bytes ) != FALSE );
}

/*
================
Sys_UnlockMemory
================
*/
bool Sys_UnlockMemory( void *ptr, int bytes ) {
	return ( VirtualUnlock( ptr, (SIZE_T)bytes ) != FALSE );
}

/*
================
Sys_SetPhysicalWorkMemory
================
*/
void Sys_SetPhysicalWorkMemory( int minBytes, int maxBytes ) {
	::SetProcessWorkingSetSize( GetCurrentProcess(), minBytes, maxBytes );
}

/*
================
Directory change watches (loose-file cache)

Sys_WatchDirectoryTree returns a handle that becomes signaled when a file or
directory anywhere under osPath is added, removed or renamed, or NULL when the
tree cannot be watched (for example, it does not exist). The file system only
caches listings of trees it can watch.
================
*/
void *Sys_WatchDirectoryTree( const char *osPath ) {
	HANDLE h = FindFirstChangeNotificationA( osPath, TRUE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME );
	return h == INVALID_HANDLE_VALUE ? NULL : (void *)h;
}

bool Sys_DirectoryTreeChanged( void *watch ) {
	return watch == NULL || WaitForSingleObject( (HANDLE)watch, 0 ) != WAIT_TIMEOUT;
}

void Sys_ResetDirectoryWatch( void *watch ) {
	// FindNextChangeNotification re-arms a signaled handle; loop in case it
	// is still signaled by changes queued before the call.
	for ( int i = 0; watch != NULL && i < 8 && WaitForSingleObject( (HANDLE)watch, 0 ) == WAIT_OBJECT_0; i++ ) {
		FindNextChangeNotification( (HANDLE)watch );
	}
}

void Sys_CloseDirectoryWatch( void *watch ) {
	if ( watch != NULL ) {
		FindCloseChangeNotification( (HANDLE)watch );
	}
}
