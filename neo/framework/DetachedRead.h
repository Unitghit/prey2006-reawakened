// Reads a file's bytes without using the (single-threaded) file system.
// FS_ResolveDetached runs on the main thread and records where the normal
// search path finds a file. FS_ReadDetached may then run on any thread: it
// opens its own archive handles and uses only malloc-backed std containers.
#ifndef PREY_DETACHED_READ_H
#define PREY_DETACHED_READ_H
#include <string>
#include <vector>

struct fsDetachedSource_t {
	std::string			archive;		// pk4 path, empty for a loose file
	unsigned long long	offset = 0;		// entry position inside the archive
	std::string			loosePath;		// OS path of a loose file
	int					length = 0;
	ID_TIME_T			timestamp = 0;
};

// Main thread only. False when the file is missing or not readable this way.
bool FS_ResolveDetached( const char *relativePath, fsDetachedSource_t &source );
// Any thread. Archive handles are cached per thread; call
// FS_ReleaseDetachedHandles on that thread before it exits.
bool FS_ReadDetached( const fsDetachedSource_t &source, std::vector<unsigned char> &bytes );
void FS_ReleaseDetachedHandles();

#endif
